#include <stdio.h>
#include "openHevcWrapper.h"
#include "avcodec.h"
#include "libavcodec/hevc.h"

typedef struct OpenHevcWrapperContext {
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *picture;
    AVPacket avpkt;
    AVCodecParserContext *parser;
} OpenHevcWrapperContext;

OpenHevcWrapperContext openHevcContext;


int libOpenHevcInit()
{
    /* register all the codecs */
    avcodec_register_all();
    av_init_packet(&openHevcContext.avpkt);
    openHevcContext.codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!openHevcContext.codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }
    openHevcContext.parser  = av_parser_init( openHevcContext.codec->id );
    openHevcContext.c       = avcodec_alloc_context3(openHevcContext.codec);
    openHevcContext.picture = avcodec_alloc_frame();

    if(openHevcContext.codec->capabilities&CODEC_CAP_TRUNCATED)
        openHevcContext.c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
         MUST be initialized there because this information is not
         available in the bitstream. */

    /* open it */
    if (avcodec_open2(openHevcContext.c, openHevcContext.codec, NULL) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }
    return 0;
}

int libOpenHevcDecode(unsigned char *buff, int nal_len)
{
    uint8_t *poutbuf;
    int got_picture, len;
    openHevcContext.avpkt.size = nal_len;
    if (nal_len == - 1) exit(10);

    av_parser_parse2(openHevcContext.parser,
            openHevcContext.c,
            &poutbuf, &nal_len,
            buff, openHevcContext.avpkt.size,
            0, 0,
            0);
    openHevcContext.avpkt.data = poutbuf;
    len = avcodec_decode_video2(openHevcContext.c, openHevcContext.picture, &got_picture, &openHevcContext.avpkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame \n");
        exit(1);
    }
    return got_picture;
}

void libOpenHevcGetPictureSize(unsigned int *width, unsigned int *height, unsigned int *stride)
{
    *width  = openHevcContext.c->width;
    *height = openHevcContext.c->height;
    *stride = openHevcContext.picture->linesize[0];
}

int libOpenHevcGetOuptut(int got_picture, unsigned char **Y, unsigned char **U, unsigned char **V)
{
    if( got_picture ) {
        *Y = openHevcContext.picture->data[0];
        *U = openHevcContext.picture->data[1];
        *V = openHevcContext.picture->data[2];
    }
    return 1;
}
int libOpenHevcGetOuptutCpy(int got_picture, unsigned char *Y, unsigned char *U, unsigned char *V)
{
    int x, y;
    int y_offset, y_offset2;
    if( got_picture ) {
        y_offset = y_offset2 = 0;
        for(y = 0; y < openHevcContext.c->height; y++) {
            for(x = 0; x < openHevcContext.c->width; x++) {
                Y[y_offset2 + x] = openHevcContext.picture->data[0][y_offset + x];
            }
            y_offset  += openHevcContext.picture->linesize[0];
            y_offset2 += openHevcContext.c->width;
        }
        y_offset = y_offset2 = 0;
        for(y = 0; y < openHevcContext.c->height/2; y++) {
            for(x = 0; x < openHevcContext.c->width/2; x++) {
                U[y_offset2 + x] = openHevcContext.picture->data[1][y_offset + x];
                V[y_offset2 + x] = openHevcContext.picture->data[2][y_offset + x];
            }
            y_offset  += openHevcContext.picture->linesize[1];
            y_offset2 += openHevcContext.c->width / 2;
        }
    }
    return 1;
}

void libOpenHevcSetCheckMD5(int val)
{
    HEVCContext *s;
    s = openHevcContext.c->priv_data;
    s->decode_checksum_sei = val;
}
void libOpenHevcClose()
{
    avcodec_close(openHevcContext.c);
    av_free(openHevcContext.c);
    av_free(openHevcContext.picture);
}

const char *libOpenHevcVersion()
{
    return "OpenHEVC v"NV_VERSION;
}

