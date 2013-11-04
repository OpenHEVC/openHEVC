//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "openHevcWrapper.h"
#include "getopt.h"
#include <libavformat/avformat.h>


typedef struct OpenHevcWrapperContext {
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *picture;
    AVPacket avpkt;
    AVCodecParserContext *parser;
} OpenHevcWrapperContext;

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
    AVFormatContext *pFormatCtx=NULL;
    AVInputFormat *file_iformat;
    AVPacket        packet;

    FILE *f     = NULL;
    FILE *fout  = NULL;
    unsigned char * buf;

    int init    = 1;
    int nbFrame = 0;
    int pts     = 0;
    int stop    = 0;
    int stop_dec= 0;
    int got_picture;
    float time  = 0.0;
    OpenHevc_Frame openHevcFrame;
    OpenHevc_Frame_cpy openHevcFrameCpy;
    OpenHevcWrapperContext *ost;

    OpenHevc_Handle openHevcHandle;
    openHevcHandle = libOpenHevcInit(nb_pthreads, thread_type/*, pFormatCtx*/);
    libOpenHevcSetCheckMD5(openHevcHandle, check_md5_flags);
    libOpenHevcSetTemporalLayer_id(openHevcHandle, temporal_layer_id);
    if (!openHevcHandle) {
        fprintf(stderr, "could not open OpenHevc\n");
        exit(1);
    }
    av_register_all();
    pFormatCtx = avformat_alloc_context();
    file_iformat = av_guess_format(NULL, filename, NULL);

    if(avformat_open_input(&pFormatCtx, filename, file_iformat, NULL)!=0) {
        printf("%s",filename);
        exit(1); // Couldn't open file
    }
    if (output_file) {
        fout = fopen(output_file, "wb");
    }
    const size_t extra_size_alloc = pFormatCtx->streams[0]->codec->extradata_size > 0 ?
    (pFormatCtx->streams[0]->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE) : 0;

    if (extra_size_alloc)
    {
        libOpenHevcCopyExtraData(openHevcHandle, pFormatCtx->streams[0]->codec->extradata, extra_size_alloc);
    }

    libOpenHevcStartDecoder(openHevcHandle);
    libOpenHevcSetDebugMode(openHevcHandle, 0);

    while(!stop) {
        if (stop_dec == 0 && av_read_frame(pFormatCtx, &packet)<0) stop_dec = 1;
        got_picture = libOpenHevcDecode(openHevcHandle, packet.data, !stop_dec ? packet.size : 0, packet.pts);
        if (got_picture) {
            fflush(stdout);
            if (init == 1 ) {
                libOpenHevcGetPictureSize2(openHevcHandle, &openHevcFrame.frameInfo);
                if (display_flags == ENABLE) {
                    Init_SDL((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight);
                }
                if (fout) {
                    int nbData;
                    libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrameCpy.frameInfo);
                    nbData = openHevcFrameCpy.frameInfo.nWidth * openHevcFrameCpy.frameInfo.nHeight;
                    openHevcFrameCpy.pvY = calloc ( nbData    , sizeof(unsigned char));
                    openHevcFrameCpy.pvU = calloc ( nbData / 4, sizeof(unsigned char));
                    openHevcFrameCpy.pvV = calloc ( nbData / 4, sizeof(unsigned char));
                }
                Init_Time();
                init = 0;
            }
            if (display_flags == ENABLE) {
                libOpenHevcGetOutput(openHevcHandle, 1, &openHevcFrame);
                libOpenHevcGetPictureSize2(openHevcHandle, &openHevcFrame.frameInfo);
                SDL_Display((openHevcFrame.frameInfo.nYPitch - openHevcFrame.frameInfo.nWidth)/2, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight,
                        openHevcFrame.pvY, openHevcFrame.pvU, openHevcFrame.pvV);
            }
            if (fout) {
                int nbData = openHevcFrameCpy.frameInfo.nWidth * openHevcFrameCpy.frameInfo.nHeight;
                libOpenHevcGetOutputCpy(openHevcHandle, 1, &openHevcFrameCpy);
                fwrite( openHevcFrameCpy.pvY , sizeof(uint8_t) , nbData    , fout);
                fwrite( openHevcFrameCpy.pvU , sizeof(uint8_t) , nbData / 4, fout);
                fwrite( openHevcFrameCpy.pvV , sizeof(uint8_t) , nbData / 4, fout);
            }
            nbFrame++;
        } else  if (stop_dec==1 && nbFrame)
            stop = 1;
    }
    time = SDL_GetTime()/1000.0;
    CloseSDLDisplay();
    if (fout)
        fclose(fout);
    avformat_close_input(&pFormatCtx);
    libOpenHevcClose(openHevcHandle);
    printf("frame= %d fps= %.0f time=%.2f video_size= %dx%d\n", nbFrame, nbFrame/time, time, openHevcFrame.frameInfo.nWidth, openHevcFrame.frameInfo.nHeight);
}

int main(int argc, char *argv[]) {
    init_main(argc, argv);
    video_decode_example(input_file);
    return 0;
}

