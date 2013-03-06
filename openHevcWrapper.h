#ifndef OPEN_HEVC_WRAPPER_H
#define OPEN_HEVC_WRAPPER_H

#include <stdio.h>
#include "avcodec.h"
#include "libavcodec/hevc.h"

#define NV_VERSION  "1.0" ///< Current software version

typedef struct OpenHevcWrapperContext {
	AVCodec *codec;
	AVCodecContext *c;
	AVFrame *picture;
	AVPacket avpkt;
	AVCodecParserContext *parser;
} OpenHevcWrapperContext;

int libOpenHevcInit();
int libOpenHevcDecode(unsigned char *buff, int nal_len);
void libOpenHevcGetPictureSize(unsigned int *width, unsigned int *height, unsigned int *stride);
int libOpenHevcGetOuptut(int got_picture, unsigned char **Y, unsigned char **U, unsigned char **V);
void libOpenHevcClose();
const char *libOpenHevcVersion();

#endif // OPEN_HEVC_WRAPPER_H
