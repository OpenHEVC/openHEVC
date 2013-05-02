//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "openHevcWrapper.h"
#include "getopt.h"

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

