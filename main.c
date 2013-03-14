//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "SDL.h"
#include "openHevcWrapper.h"
#include "getopt.h"



int find_start_code (unsigned char *Buf, int zeros_in_startcode)
{
    int info;
    int i;
    
    info = 1;
    for (i = 0; i < zeros_in_startcode; i++)
        if(Buf[i] != 0)
            info = 0;
    
    if(Buf[i] != 1)
        info = 0;
    return info;
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
int init=1;
static void video_decode_example(const char *filename)
{
    FILE *f;
    int nal_len = 0;
    unsigned int width, height, stride;
    unsigned char * buf, *Y, *U, *V;
    
    libOpenHevcInit();
    libOpenHevcSetCheckMD5(check_md5_flags);
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "could not open %s\n", filename);
        exit(1);
    }
    buf = calloc ( 1000000, sizeof(char));
    while(!feof(f)) {
        int got_picture = libOpenHevcDecode(buf, get_next_nal(f, buf));
        libOpenHevcGetOuptut(got_picture, &Y, &U, &V);
        if (got_picture && display_flags == DISPLAY_ENABLE) {
            fflush(stdout);
            if (init == 1 ) {
                libOpenHevcGetPictureSize(&width, &height, &stride);
                Init_SDL((stride - width)/2, width, height);
            }
            init=0;
            SDL_Display((stride - width)/2, width, height, Y, U, V);
        }
     }
    fclose(f);
    libOpenHevcClose();
}


int main(int argc, char *argv[]) {
    init_main(argc, argv);
    video_decode_example(input_file);
    return 0;
}

