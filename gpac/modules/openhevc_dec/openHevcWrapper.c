/*
 * openhevc.c wrapper to openhevc or ffmpeg
 * Copyright (c) 2012-2013 Micka�l Raulet, Wassim Hamidouche, Gildas Cocherel, Pierre Edouard Lepere
 *
 * This file is part of openhevc.
 *
 * openHevc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * openhevc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with openhevc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdio.h>
#include "openHevcWrapper.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"

#define MAX_DECODERS 2
#define ACTIVE_NAL
typedef struct OpenHevcWrapperContext {
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *picture;
    AVPacket avpkt;
    AVCodecParserContext *parser;
} OpenHevcWrapperContext;

typedef struct OpenHevcWrapperContexts {
    OpenHevcWrapperContext **wraper;
    int nb_decoders;
    int active_layer;
    int display_layer;
    int set_display;
    int set_vps;
} OpenHevcWrapperContexts;

OpenHevc_Handle libOpenHevcInit(int nb_pthreads, int thread_type)
{
    /* register all the codecs */
    int i;
    OpenHevcWrapperContexts *openHevcContexts = av_mallocz(sizeof(OpenHevcWrapperContexts));
    OpenHevcWrapperContext  *openHevcContext;
    avcodec_register_all();
    openHevcContexts->nb_decoders   = MAX_DECODERS;
    openHevcContexts->active_layer  = MAX_DECODERS-1;
    openHevcContexts->display_layer = MAX_DECODERS-1;
    openHevcContexts->wraper = av_malloc(sizeof(OpenHevcWrapperContext*)*openHevcContexts->nb_decoders);
    for(i=0; i < openHevcContexts->nb_decoders; i++){
        openHevcContext = openHevcContexts->wraper[i] = av_malloc(sizeof(OpenHevcWrapperContext));
        av_init_packet(&openHevcContext->avpkt);
        openHevcContext->codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        if (!openHevcContext->codec) {
            fprintf(stderr, "codec not found\n");
            return NULL;
        }

        openHevcContext->parser  = av_parser_init( openHevcContext->codec->id );
        openHevcContext->c       = avcodec_alloc_context3(openHevcContext->codec);
        openHevcContext->picture = avcodec_alloc_frame();

        if(openHevcContext->codec->capabilities&CODEC_CAP_TRUNCATED)
            openHevcContext->c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        /* For some codecs, such as msmpeg4 and mpeg4, width and height
         MUST be initialized there because this information is not
         available in the bitstream. */

        /*      set thread parameters    */
        if(thread_type == 1)
            av_opt_set(openHevcContext->c, "thread_type", "frame", 0);
        else if (thread_type == 2)
            av_opt_set(openHevcContext->c, "thread_type", "slice", 0);
        else
            av_opt_set(openHevcContext->c, "thread_type", "frameslice", 0);

        av_opt_set_int(openHevcContext->c, "threads", nb_pthreads, 0);

        /*  Set the decoder id    */
        av_opt_set_int(openHevcContext->c->priv_data, "decoder-id", i, 0);
    }
    return (OpenHevc_Handle) openHevcContexts;
}

int libOpenHevcStartDecoder(OpenHevc_Handle openHevcHandle)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;
    for(i=0; i < openHevcContexts->nb_decoders; i++) {
        openHevcContext = openHevcContexts->wraper[i];
        if (avcodec_open2(openHevcContext->c, openHevcContext->codec, NULL) < 0) {
            fprintf(stderr, "could not open codec\n");
            return -1;
        }
        if(i+1 < openHevcContexts->nb_decoders)
            openHevcContexts->wraper[i+1]->c->BL_avcontext = openHevcContexts->wraper[i]->c;
    }
    return 1;
}

int libOpenHevcDecode(OpenHevc_Handle openHevcHandle, const unsigned char *buff, int au_len, int64_t pts)
{
    int got_picture[MAX_DECODERS], len=0, i, max_layer;
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    for(i =0; i <= openHevcContexts->active_layer; i++)  {
        got_picture[i]              = 0;
        openHevcContext             = openHevcContexts->wraper[i];
        openHevcContext->avpkt.size = au_len;
        openHevcContext->avpkt.data = (uint8_t *) buff;
        openHevcContext->avpkt.pts  = pts;
        len                         = avcodec_decode_video2( openHevcContext->c, openHevcContext->picture,
                                                             &got_picture[i], &openHevcContext->avpkt);
        if(i+1 < openHevcContexts->nb_decoders)
            openHevcContexts->wraper[i+1]->c->BL_frame = openHevcContexts->wraper[i]->c->BL_frame;
    }
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame \n");
        return -1;
    }
    if(openHevcContexts->set_display)
        max_layer = openHevcContexts->display_layer;
    else
        max_layer = openHevcContexts->active_layer;

    for(i=max_layer; i>=0; i--) {
        if(got_picture[i]){
            if(i != openHevcContexts->display_layer) {
                if (i >= 0 && i < openHevcContexts->nb_decoders)
                    openHevcContexts->display_layer = i;
            }
         //   fprintf(stderr, "Display layer %d  \n", i);
            return got_picture[i];
        }
    }
    return 0;
}

void libOpenHevcCopyExtraData(OpenHevc_Handle openHevcHandle, unsigned char *extra_data, int extra_size_alloc)
{
    int i;
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    for(i =0; i <= openHevcContexts->active_layer; i++)  {
        openHevcContext = openHevcContexts->wraper[i];
        openHevcContext->c->extradata = (uint8_t*)av_mallocz(extra_size_alloc);
        memcpy( openHevcContext->c->extradata, extra_data, extra_size_alloc);
        openHevcContext->c->extradata_size = extra_size_alloc;
	}
}


void libOpenHevcGetPictureInfo(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[openHevcContexts->display_layer];
    AVFrame                 *picture          = openHevcContext->picture;

    openHevcFrameInfo->nYPitch    = picture->width;

    switch (picture->format) {
    case PIX_FMT_YUV420P   :
    case PIX_FMT_YUV420P9  :
    case PIX_FMT_YUV420P10 :
        openHevcFrameInfo->nUPitch    = picture->width>>1;
        openHevcFrameInfo->nVPitch    = picture->width>>1;
        break;
    default :
        openHevcFrameInfo->nUPitch    = picture->width>>1;
        openHevcFrameInfo->nVPitch    = picture->width>>1;
        break;
    }

    switch (picture->format) {
    case PIX_FMT_YUV420P   : openHevcFrameInfo->nBitDepth  =  8; break;
    case PIX_FMT_YUV420P9  : openHevcFrameInfo->nBitDepth  =  9; break;
    case PIX_FMT_YUV420P10 : openHevcFrameInfo->nBitDepth  = 10; break;
    default               : openHevcFrameInfo->nBitDepth   =  8; break;
    }

    openHevcFrameInfo->nWidth                  = picture->width;
    openHevcFrameInfo->nHeight                 = picture->height;
    openHevcFrameInfo->sample_aspect_ratio.num = picture->sample_aspect_ratio.num;
    openHevcFrameInfo->sample_aspect_ratio.den = picture->sample_aspect_ratio.den;
    openHevcFrameInfo->frameRate.num           = openHevcContext->c->time_base.den;
    openHevcFrameInfo->frameRate.den           = openHevcContext->c->time_base.num;
    openHevcFrameInfo->display_picture_number  = picture->display_picture_number;
    openHevcFrameInfo->flag                    = (picture->top_field_first << 2) | picture->interlaced_frame; //progressive, interlaced, interlaced bottom field first, interlaced top field first.
    openHevcFrameInfo->nTimeStamp              = picture->pkt_pts;
}

void libOpenHevcGetPictureSize2(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[openHevcContexts->display_layer];

    libOpenHevcGetPictureInfo(openHevcHandle, openHevcFrameInfo);
    openHevcFrameInfo->nYPitch = openHevcContext->picture->linesize[0];
    openHevcFrameInfo->nUPitch = openHevcContext->picture->linesize[1];
    openHevcFrameInfo->nVPitch = openHevcContext->picture->linesize[2];
}

int libOpenHevcGetOutput(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[openHevcContexts->display_layer];

    if (got_picture) {
        openHevcFrame->pvY       = (void *) openHevcContext->picture->data[0];
        openHevcFrame->pvU       = (void *) openHevcContext->picture->data[1];
        openHevcFrame->pvV       = (void *) openHevcContext->picture->data[2];

        libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrame->frameInfo);
    }
    return 1;
}

int libOpenHevcGetOutputCpy(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame_cpy *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;


    OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[openHevcContexts->display_layer];
    AVFrame                 *picture          = openHevcContext->picture;

    int y;
    int y_offset, y_offset2;
    if( got_picture ) {
        unsigned char *Y = (unsigned char *) openHevcFrame->pvY;
        unsigned char *U = (unsigned char *) openHevcFrame->pvU;
        unsigned char *V = (unsigned char *) openHevcFrame->pvV;
        int width;

        switch (openHevcContext->picture->format) {
        case PIX_FMT_YUV420P   : width = picture->width;     break;
        default                : width = picture->width * 2; break;
        }

        y_offset = y_offset2 = 0;

        for (y = 0; y < picture->height; y++) {
            memcpy(&Y[y_offset2], &openHevcContext->picture->data[0][y_offset], width);
            y_offset  += openHevcContext->picture->linesize[0];
            y_offset2 += width;
        }
        y_offset = y_offset2 = 0;

        for (y = 0; y < picture->height/2; y++) {
            memcpy(&U[y_offset2], &openHevcContext->picture->data[1][y_offset], width/2);
            memcpy(&V[y_offset2], &openHevcContext->picture->data[2][y_offset], width/2);
            y_offset  += openHevcContext->picture->linesize[1];
            y_offset2 += width / 2;
        }
        libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrame->frameInfo);
    }
    return 1;
}

void libOpenHevcSetDebugMode(OpenHevc_Handle openHevcHandle, int val)
{
    if (val == 1)
        av_log_set_level(AV_LOG_DEBUG);
}
void libOpenHevcSetActiveDecoders(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    if (val >= 0 && val < openHevcContexts->nb_decoders)
        openHevcContexts->active_layer = val;
    else {
        fprintf(stderr, "The requested layer %d can not be decoded (it exceeds the number of allocated decoders %d ) \n", val, openHevcContexts->nb_decoders);
        openHevcContexts->active_layer = openHevcContexts->nb_decoders-1;
    }
}

void libOpenHevcSetViewLayers(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    openHevcContexts->set_display = 1;
    if (val >= 0 && val < openHevcContexts->nb_decoders)
        openHevcContexts->display_layer = val;
    else {
        fprintf(stderr, "The requested layer %d can not be viewed (it exceeds the number of allocated decoders %d ) \n", val, openHevcContexts->nb_decoders);
        openHevcContexts->display_layer = openHevcContexts->nb_decoders-1;
    }
}


void libOpenHevcSetCheckMD5(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;

    for (i = 0; i < openHevcContexts->nb_decoders; i++) {
        openHevcContext = openHevcContexts->wraper[i];

        av_opt_set_int(openHevcContext->c->priv_data, "decode-checksum", val, 0);
    }
}

void libOpenHevcSetTemporalLayer_id(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;

    for (i = 0; i < openHevcContexts->nb_decoders; i++) {
        openHevcContext = openHevcContexts->wraper[i];
        av_opt_set_int(openHevcContext->c->priv_data, "temporal-layer-id", val+1, 0);
    }
    
}

void libOpenHevcSetNoCropping(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;

    for (i = 0; i < openHevcContexts->nb_decoders; i++) {
        openHevcContext = openHevcContexts->wraper[i];
        av_opt_set_int(openHevcContext->c->priv_data, "no-cropping", val, 0);
    }
}

void libOpenHevcClose(OpenHevc_Handle openHevcHandle)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;

    for (i = 0; i < openHevcContexts->nb_decoders; i++){
        openHevcContext = openHevcContexts->wraper[i];
        avcodec_close(openHevcContext->c);
        av_parser_close(openHevcContext->parser);
        av_freep(&openHevcContext->c);
        av_freep(&openHevcContext->picture);
        av_freep(&openHevcContext);
    }
    av_freep(&openHevcContexts->wraper);
    av_freep(&openHevcContexts);
}

void libOpenHevcFlush(OpenHevc_Handle openHevcHandle)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[openHevcContexts->active_layer];

    openHevcContext->codec->flush(openHevcContext->c);
}

const char *libOpenHevcVersion(OpenHevc_Handle openHevcHandle)
{
    return "OpenHEVC v"NV_VERSION;
}

