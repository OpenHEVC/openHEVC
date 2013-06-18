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
    FILE *f, *fout;
    int init    = 1;
    int nbFrame = 0;
    int pts     = 0;
    unsigned int width, height, stride;
    unsigned char * buf;
    OpenHevc_Frame openHevcFrame;
    OpenHevc_Frame_cpy openHevcFrameCpy;
    OpenHevc_Handle openHevcHandle = libOpenHevcInit(nb_pthreads);
    if (openHevcHandle == NULL) {
        fprintf(stderr, "could not open OpenHevc\n");
        exit(1);
    }
    libOpenHevcSetCheckMD5(openHevcHandle, check_md5_flags);
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "could not open %s\n", filename);
        exit(1);
    }
    if (output_file != NULL) {
        fout = fopen(output_file, "wb");
    }
    buf = calloc ( 1000000, sizeof(char));
    if (display_flags == DISPLAY_ENABLE) {
        while(!feof(f)) {
            if (libOpenHevcDecode(openHevcHandle, buf, get_next_nal(f, buf), pts++)) {
                libOpenHevcGetOutput(openHevcHandle, 1, &openHevcFrame);
                libOpenHevcGetPictureSize2(openHevcHandle, &openHevcFrame.frameInfo);
                fflush(stdout);
                if (init == 1 ) {
                    Init_SDL((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight);
                    Init_Time();
                    init = 0;
                }
                SDL_Display((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight,
                        openHevcFrame.pvY, openHevcFrame.pvU, openHevcFrame.pvV);
                nbFrame++;
            }
        }
        while(libOpenHevcDecode(openHevcHandle, buf, 0, pts++)!=0) {
            libOpenHevcGetOutput(openHevcHandle, 1, &openHevcFrame);
            libOpenHevcGetPictureSize2(openHevcHandle, &openHevcFrame.frameInfo);
            fflush(stdout);
            SDL_Display((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight,
                    openHevcFrame.pvY, openHevcFrame.pvU, openHevcFrame.pvV);
            nbFrame++;
        }
    } else {
        while(!feof(f)) {
            if (libOpenHevcDecode(openHevcHandle, buf, get_next_nal(f, buf), pts++)) {
                if (init == 1 ) {
                    Init_Time();
                    init = 0;
                    if (output_file != NULL) {
                        int nbData;
                        libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrameCpy.frameInfo);
                        nbData = openHevcFrameCpy.frameInfo.nWidth * openHevcFrameCpy.frameInfo.nHeight;
                        openHevcFrameCpy.pvY = calloc ( nbData    , sizeof(unsigned char));
                        openHevcFrameCpy.pvU = calloc ( nbData / 4, sizeof(unsigned char));
                        openHevcFrameCpy.pvV = calloc ( nbData / 4, sizeof(unsigned char));
                    }
                }
               libOpenHevcGetOutputCpy(openHevcHandle, 1, &openHevcFrameCpy);
               if (output_file != NULL) {
                    int nbData = openHevcFrameCpy.frameInfo.nWidth * openHevcFrameCpy.frameInfo.nHeight;
                    fwrite( openHevcFrameCpy.pvY , sizeof(uint8_t) , nbData    , fout);
                    fwrite( openHevcFrameCpy.pvU , sizeof(uint8_t) , nbData / 4, fout);
                    fwrite( openHevcFrameCpy.pvV , sizeof(uint8_t) , nbData / 4, fout);
                }
                nbFrame++;
            }
        }
        while(libOpenHevcDecode(openHevcHandle, buf, 0, pts++)) {
            libOpenHevcGetOutputCpy(openHevcHandle, 1, &openHevcFrameCpy);
            if (output_file != NULL) {
                int nbData = openHevcFrameCpy.frameInfo.nWidth * openHevcFrameCpy.frameInfo.nHeight;
                fwrite( openHevcFrameCpy.pvY , sizeof(uint8_t) , nbData    , fout);
                fwrite( openHevcFrameCpy.pvU , sizeof(uint8_t) , nbData / 4, fout);
                fwrite( openHevcFrameCpy.pvV , sizeof(uint8_t) , nbData / 4, fout);
            }
            nbFrame++;
        }
     }
    CloseSDLDisplay();
    fclose(f);
    libOpenHevcClose(openHevcHandle);
    if (output_file != NULL) {
        printf("video size : %d x %d\n", openHevcFrameCpy.frameInfo.nWidth, openHevcFrameCpy.frameInfo.nHeight);
    }
    printf("nbFrame : %d\n", nbFrame);
}


int main(int argc, char *argv[]) {
    init_main(argc, argv);
    video_decode_example(input_file);
    return 0;
}

