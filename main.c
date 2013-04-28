//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "openHevcWrapper.h"
#include "getopt.h"


//#include <OpenGL/gl.h>
//#include <OpenGL/glu.h>
//#include <GLUT/glut.h>


int find_start_code (unsigned char *Buf, int zeros_in_startcode)
{
    int i;
    for (i = 0; i < zeros_in_startcode; i++)
        if(Buf[i] != 0)
            return 0;
    return Buf[i];
}

int get_next_nal(FILE* inpf, unsigned char* Buf)
{
    int pos = 0;
    int StartCodeFound = 0;
    int info2 = 0;
    int info3 = 0;
    while(!feof(inpf)&&(/*Buf[pos++]=*/fgetc(inpf))==0);

    while (pos < 3) Buf[pos++] = fgetc (inpf);
    while (!StartCodeFound)
    {
        if (feof (inpf))
        {
            //            return -1;
            return pos-1;
        }
        Buf[pos++] = fgetc (inpf);
        info3 = find_start_code(&Buf[pos-4], 3);
        if(info3 != 1)
            info2 = find_start_code(&Buf[pos-3], 2);
        StartCodeFound = (info2 == 1 || info3 == 1);
    }
    fseek (inpf, - 4 + info2, SEEK_CUR);
    return pos - 4 + info2;
}

static int YUV2RGB  (   unsigned char   *picture,
                        unsigned char   *picture_u,
                        unsigned char   *picture_v,
                        unsigned char   *picture_RGB,
                        unsigned int    width,
                        unsigned int    height  )	// convert YUV_888 to RGB_888
{
    unsigned char clipping[1024];
    for(int n=-512; n<512; n++)
    {
        if(n<=0)
        {
            clipping[512+n]=0;
        }
        else if (n>=255)
        {
            clipping[512+n]=255;
        }
        else
        {
            clipping[512+n]=n;
        }
    }
    unsigned char* clip = clipping+512;
    for(int y=0; y<height; y++)
    {
        int ycr = y/2;

        unsigned char *pos_y=picture   + y   * width;
        unsigned char *pos_u=picture_u + ycr * (width>>1);
        unsigned char *pos_v=picture_v + ycr * (width>>1);

        unsigned char *pos_rgb= picture_RGB + y*width*3;
        for(int x=0; x<width; x+=2)
        {
            int c = *pos_y-16;
            int d = *pos_u-128;
            int e = *pos_v-128;
            pos_rgb[0]=clip[(298*c + 409*e + 128)>>8];
            pos_rgb[1]=clip[(298*c -100*d - 208*e + 128)>>8];
            pos_rgb[2]=clip[(298*c + 516*d + 128)>>8];
            pos_y++;
            pos_rgb +=3;
            c = *pos_y-16;
            pos_rgb[0]=clip[(298*c + 409*e + 128)>>8];
            pos_rgb[1]=clip[(298*c -100*d - 208*e + 128)>>8];
            pos_rgb[2]=clip[(298*c + 516*d + 128)>>8];
            pos_y++;
            pos_u++;
            pos_v++;
            pos_rgb +=3;
        }
    }
    return 1;
}

static void video_decode_example(const char *filename)
{
    FILE *f;
    int init    = 1;
    int nbFrame = 0;
    unsigned int width, height, stride;
    unsigned char * buf, *Y, *U, *V;
    
    libOpenHevcInit(nb_pthreads);
    libOpenHevcSetCheckMD5(check_md5_flags);
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "could not open %s\n", filename);
        exit(1);
    }
    buf = calloc ( 1000000, sizeof(char));
    if (display_flags == DISPLAY_ENABLE) {
        while(!feof(f)) {
            int got_picture;
            got_picture = libOpenHevcDecode(buf, get_next_nal(f, buf));
            libOpenHevcGetOutput(got_picture, &Y, &U, &V);
            if (got_picture != 0) {
                fflush(stdout);
                if (init == 1 ) {
                    libOpenHevcGetPictureSize2(&width, &height, &stride);
                    Init_SDL((stride - width)/2, width, height);
                    Init_Time();
                    init = 0;
                }
                SDL_Display((stride - width)/2, width, height, Y, U, V);
                // Use open GL to display
               // YUV2RGB(Y, U, V, picture_RGB, width, height);
                
                nbFrame++;
            }
        }
    } else {
        while(!feof(f)) {
            int got_picture = libOpenHevcDecode(buf, get_next_nal(f, buf));
            libOpenHevcGetOutput(got_picture, &Y, &U, &V);
            if (got_picture != 0) {
                if (init == 1 ) {
                    Init_Time();
                    init = 0;
                }
                nbFrame++;
            }
        }
     }
    CloseSDLDisplay();
    fclose(f);
    libOpenHevcClose();
    printf("nbFrame : %d\n", nbFrame);
}


int main(int argc, char *argv[]) {
    init_main(argc, argv);
    video_decode_example(input_file);
    return 0;
}

