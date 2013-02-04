//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "SDL.h"
#include <stdio.h>
#include "avcodec.h"
#include "libavcodec/hevc.h"

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
	while(!feof(inpf)&&(Buf[pos++]=fgetc(inpf))==0);
    
	int StartCodeFound = 0;
	int info2 = 0;
	int info3 = 0;
    
	while (!StartCodeFound)
	{
		if (feof (inpf))
		{
            //			return -1;
			return pos - 1;
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
    AVCodec *codec;
    AVCodecContext *c= NULL;
    int frame, got_picture, len;
    FILE *f;
    AVFrame *picture;
    AVPacket avpkt;
    int nal_len = 0;
	unsigned char* buf; 
    
    av_init_packet(&avpkt);
    
    printf("Video decoding\n");
    
    codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }
    AVCodecParserContext *parser = av_parser_init( codec->id );
    
    c = avcodec_alloc_context3(codec);
    picture= avcodec_alloc_frame();
    
    
    if(codec->capabilities&CODEC_CAP_TRUNCATED)
        c->flags|= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */
    
    /* For some codecs, such as msmpeg4 and mpeg4, width and height
     MUST be initialized there because this information is not
     available in the bitstream. */
    
    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }
    
    /* the codec gives us the frame size, in samples */
    
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "could not open %s\n", filename);
        exit(1);
    }
    buf = calloc ( 1000000, sizeof(char));
    
    frame = 0;
    for(;;) {
        uint8_t *poutbuf;
        avpkt.size = nal_len = get_next_nal(f, buf);
        if (nal_len == - 1) exit(10);
        
        av_parser_parse2(parser,
                         c,
                         &poutbuf, &nal_len,
                         buf, avpkt.size,
                         0, 0,
                         0);
        avpkt.data = poutbuf;
        len = avcodec_decode_video2(c, picture, &got_picture, &avpkt);
        if (len < 0) {
            fprintf(stderr, "Error while decoding frame %d\n", frame);
            exit(1);
        }
        if (got_picture) {
            printf("saving frame %3d\n", frame);
            fflush(stdout);
            if (init == 1) Init_SDL((picture->linesize[0] - c->width)/2, c->width, c->height);
            init=0;

            SDL_Display((picture->linesize[0] - c->width)/2, c->width, c->height, picture->data[0], (picture->data[1]),
                        picture->data[2]);
            /* the picture is allocated by the decoder. no need to
            free it */
            //snprintf(buf, sizeof(buf), outfilename, frame);
            /*pgm_save(picture->data[0], picture->linesize[0],
                         c->width, c->height, buf);*/
            frame++;
        }
     }
    
    /* some codecs, such as MPEG, transmit the I and P frame with a
     latency of one frame. You must do the following to have a
     chance to get the last frame of the video */
    avpkt.data = NULL;
    avpkt.size = 0;
    len = avcodec_decode_video2(c, picture, &got_picture, &avpkt);
    if (got_picture) {
        printf("saving last frame %3d\n", frame);
        fflush(stdout);
        /* the picture is allocated by the decoder. no need to
         free it */
        //snprintf(buf, sizeof(buf), outfilename, frame);
        /*pgm_save(picture->data[0], picture->linesize[0],
                 c->width, c->height, buf);*/
        frame++;
    }
    
    fclose(f);
    
    avcodec_close(c);
    av_free(c);
    av_free(picture);
    printf("\n");
}


int main(int argc, char *argv[]) {
    const char *filename;
    
    /* register all the codecs */
    avcodec_register_all();
    if(argc == 2) {
    	filename = argv[1];
    } else {
        fprintf(stderr, "An input file must be specified\n");
        exit(1);
    }
    video_decode_example(filename);

    // insert code here...
    printf("Hello, World!\n");
    return 0;
}

