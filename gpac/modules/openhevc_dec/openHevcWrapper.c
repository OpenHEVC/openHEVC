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

//TMP
//#include "libavcodec/h264.h"

#define MAX_DECODERS 3
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
    int got_picture_mask;
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
        openHevcContext->picture = av_frame_alloc();
        openHevcContext->c->flags |= AV_CODEC_FLAG_UNALIGNED;

        if(openHevcContext->codec->capabilities&AV_CODEC_CAP_TRUNCATED)
            openHevcContext->c->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

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

OpenHevc_Handle libOpenH264Init(int nb_pthreads, int thread_type)
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
        openHevcContext->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!openHevcContext->codec) {
            fprintf(stderr, "codec not found\n");
            return NULL;
        }

        openHevcContext->parser  = av_parser_init( openHevcContext->codec->id );
        openHevcContext->c       = avcodec_alloc_context3(openHevcContext->codec);
        openHevcContext->picture = av_frame_alloc();
        openHevcContext->c->flags |= AV_CODEC_FLAG_UNALIGNED;

        if(openHevcContext->codec->capabilities&AV_CODEC_CAP_TRUNCATED)
            openHevcContext->c->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

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
/**
 * Init up to MAX_DECODERS decoders for SHVC decoding in case of AVC Base Layer and
 * allocate their contexts
 *    -First decoder will be h264 decoder
 *    -Second one will be HEVC decoder
 *    -Third decoder is allocated but still unused since its not supported yet
 */
OpenHevc_Handle libOpenShvcInit(int nb_pthreads, int thread_type)
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
        if(i == 0)
        	openHevcContext->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        else
            openHevcContext->codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        if (!openHevcContext->codec) {
            fprintf(stderr, "codec not found\n");
            return NULL;
        }

        openHevcContext->parser  = av_parser_init( openHevcContext->codec->id );
        openHevcContext->c       = avcodec_alloc_context3(openHevcContext->codec);
        openHevcContext->picture = av_frame_alloc();
        openHevcContext->c->flags |= AV_CODEC_FLAG_UNALIGNED;

        if(openHevcContext->codec->capabilities&AV_CODEC_CAP_TRUNCATED)
            openHevcContext->c->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

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
    int i, max_layer;
    int ret = 0;
    int err = 0;

    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;

    openHevcContexts->got_picture_mask = 0;

    if(openHevcContexts->set_display)
        max_layer = openHevcContexts->display_layer;
    else
        max_layer = openHevcContexts->active_layer;

    for( i = 0; i < MAX_DECODERS; i++)  {

        int got_picture = 0;

        openHevcContext                = openHevcContexts->wraper[i];
        openHevcContext->c->quality_id = openHevcContexts->active_layer;

        if (i <= openHevcContexts->active_layer) {
            openHevcContext->avpkt.size = au_len;
            openHevcContext->avpkt.data = (uint8_t *) buff;
        } else {
            openHevcContext->avpkt.size = 0;
            openHevcContext->avpkt.data = NULL;
        }
        openHevcContext->avpkt.pts  = pts;
        err                         = avcodec_decode_video2( openHevcContext->c, openHevcContext->picture,
                                                             &got_picture, &openHevcContext->avpkt);
        ret |= (got_picture << i);

        if(i < openHevcContexts->active_layer)
            openHevcContexts->wraper[i+1]->c->BL_frame = openHevcContexts->wraper[i]->c->BL_frame;
    }

    openHevcContexts->got_picture_mask = ret;

    if (err < 0) {
        fprintf(stderr, "Error while decoding frame \n");
        return err;
    }

    return ret;
}

/**
 * Pass the packets to the corresponding decoders and loop over running decoders untill one of them
 * output a got_picture.
 *    -First decoder will be h264 decoder
 *    -Second one will be HEVC decoder
 *    -Third decoder is ignored since its not supported yet
 */
int libOpenShvcDecode(OpenHevc_Handle openHevcHandle, const AVPacket packet[], const int stop_dec1, const int stop_dec2)
{
    int got_picture[MAX_DECODERS], len=0, i, max_layer, au_len, stop_dec;
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    for(i =0; i < MAX_DECODERS; i++)  {
    	//fixme: au_len is unused
    	if(i==0)
    		stop_dec = stop_dec1;
    	if(i==1)
    		stop_dec = stop_dec2;
    	au_len = !stop_dec ? packet[i].size : 0;
        got_picture[i]                 = 0;
        openHevcContext                = openHevcContexts->wraper[i];
        openHevcContext->c->quality_id = openHevcContexts->active_layer;
//        printf("quality_id %d \n", openHevcContext->c->quality_id);
        if (i <= openHevcContexts->active_layer) { // pour la auite remplacer par l = 1
            openHevcContext->avpkt.size = au_len;
            openHevcContext->avpkt.data = (uint8_t *) packet[i].data;
        } else {
            openHevcContext->avpkt.size = 0;
            openHevcContext->avpkt.data = NULL;
        }
        openHevcContext->avpkt.pts  = packet[i].pts;
        len                         = avcodec_decode_video2(openHevcContext->c, openHevcContext->picture,
                                                             &got_picture[i], &openHevcContext->avpkt);



        	//Fixme: This way of passing base layer frame reference to each other is bad and should be corrected
        	//We don't know what the first decoder could be doing with its BL_frame (modifying or deleting it)
        	//A cleanest way to do things would be to handle the h264 decoder from the first decoder, but the main issue
        	//would be finding a way to keep giving AVPacket, to h264 when required until the BL_frames required by HEVC
        	//are decoded and available.
                if(i < openHevcContexts->active_layer)
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
                if(i == openHevcContexts->display_layer) {
                    if (i >= 0 && i < openHevcContexts->nb_decoders)
                        openHevcContexts->display_layer = i;
                    return got_picture[i];
                }
             //   fprintf(stderr, "Display layer %d  \n", i);

            }

        }
    return 0;
}

//FIXME: this variable is used as a frame counter in order to synchronize packets from AVC and HEVC frames
//We don't have a cleaner way of doing things at the moment.
static int poc_id;

/**
 * Pass the packets to the corresponding decoders and loop over running decoders untill one of them
 * output a got_picture.
 *    -First decoder will be h264 decoder
 *    -Second one will be HEVC decoder
 *    -Third decoder is ignored since its not supported yet
 */
int libOpenShvcDecode2(OpenHevc_Handle openHevcHandle, const unsigned char *buff, const unsigned char *buff2, int nal_len, int nal_len2, int64_t pts, int64_t pts2)
{
    int i, max_layer;
    int ret = 0;
    int err = 0;

    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;

    openHevcContexts->got_picture_mask = 0;

    if(openHevcContexts->set_display)
        max_layer = openHevcContexts->display_layer;
    else
        max_layer = openHevcContexts->active_layer;

    poc_id++;
    poc_id&=1023;

    for(i =0; i < MAX_DECODERS; i++)  {
        int got_picture = 0;
        openHevcContext                = openHevcContexts->wraper[i];
        openHevcContext->c->quality_id = openHevcContexts->active_layer;
        if(i==0){
            openHevcContext->avpkt.size = nal_len;
            openHevcContext->avpkt.data = buff;
            openHevcContext->avpkt.pts  = pts;
            openHevcContext->avpkt.poc_id = poc_id;
            if(buff2 && openHevcContexts->active_layer){
                openHevcContext->avpkt.el_available=1;
            }else {
                openHevcContext->avpkt.el_available=0;
            }
        } else if(i > 0 && i <= openHevcContexts->active_layer){
            openHevcContext->avpkt.size = nal_len2;
            openHevcContext->avpkt.data = buff2;
            openHevcContext->avpkt.pts  = pts2;
            openHevcContext->avpkt.poc_id = poc_id;
            if(buff){
                openHevcContext->avpkt.bl_available=1;
            }
            else {
                openHevcContext->avpkt.bl_available=0;
            }
        } else {
            openHevcContext->avpkt.size = 0;
            openHevcContext->avpkt.data = NULL;
        }

        err = avcodec_decode_video2(openHevcContext->c, openHevcContext->picture, &got_picture, &openHevcContext->avpkt);

        ret |= (got_picture << i);

        //Fixme: This way of passing base layer frame reference to each other is bad and should be corrected
        //We don't know what the first decoder could be doing with its BL_frame (modifying or deleting it)
        //A cleanest way to do things would be to handle the h264 decoder from the first decoder, but the main issue
        //would be finding a way to keep giving AVPacket, to h264 when required until the BL_frames required by HEVC
        //are decoded and available.

        openHevcContexts->got_picture_mask = ret;

        if(i < openHevcContexts->active_layer)
            openHevcContexts->wraper[i+1]->c->BL_frame = openHevcContexts->wraper[i]->c->BL_frame;
    }

    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while decoding frame %d", err);
        return err;
    }

    return ret;
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

void libOpenShvcCopyExtraData(OpenHevc_Handle openHevcHandle, unsigned char *extra_data_linf, unsigned char *extra_data_lsup, int extra_size_alloc_linf, int extra_size_alloc_lsup)
{
    int i;
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    if (extra_data_linf && extra_size_alloc_linf) {
        openHevcContext = openHevcContexts->wraper[0];
        if (openHevcContext->c->extradata) av_freep(&openHevcContext->c->extradata);
        openHevcContext->c->extradata = (uint8_t*)av_mallocz(extra_size_alloc_linf);
        memcpy( openHevcContext->c->extradata, extra_data_linf, extra_size_alloc_linf);
        openHevcContext->c->extradata_size = extra_size_alloc_linf;
    }

    if (extra_data_lsup && extra_size_alloc_lsup) {
        for(i =1; i <= openHevcContexts->active_layer; i++)  {
            openHevcContext = openHevcContexts->wraper[i];
            if (openHevcContext->c->extradata) av_freep(&openHevcContext->c->extradata);
            openHevcContext->c->extradata = (uint8_t*)av_mallocz(extra_size_alloc_lsup);
            memcpy( openHevcContext->c->extradata, extra_data_lsup, extra_size_alloc_lsup);
            openHevcContext->c->extradata_size = extra_size_alloc_lsup;
        }
	}
}

static int get_picture_bitdepth(int format){

    switch (format) {
        case AV_PIX_FMT_YUV420P   :
        case AV_PIX_FMT_YUV422P   :
        case AV_PIX_FMT_YUV444P   :
            return  8;
        case AV_PIX_FMT_YUV420P9  :
        case AV_PIX_FMT_YUV422P9  :
        case AV_PIX_FMT_YUV444P9  :
            return  9;
        case AV_PIX_FMT_YUV420P10 :
        case AV_PIX_FMT_YUV422P10 :
        case AV_PIX_FMT_YUV444P10 :
            return  10;
        case AV_PIX_FMT_YUV420P12 :
        case AV_PIX_FMT_YUV422P12 :
            return  12;
        default :
            return  8;
    }
}

static int get_chroma_format(int format){

    switch (format) {
        case AV_PIX_FMT_YUV420P   :
        case AV_PIX_FMT_YUV420P9  :
        case AV_PIX_FMT_YUV420P10 :
        case AV_PIX_FMT_YUV420P12 :
            return  YUV420;
        case AV_PIX_FMT_YUV422P   :
        case AV_PIX_FMT_YUV422P9  :
        case AV_PIX_FMT_YUV422P10 :
        case AV_PIX_FMT_YUV422P12 :
            return  YUV422;
        case AV_PIX_FMT_YUV444P   :
        case AV_PIX_FMT_YUV444P9  :
        case AV_PIX_FMT_YUV444P10 :
        case AV_PIX_FMT_YUV444P12 :
            return YUV444;
        default :
            return UNKNOWN_CHROMA_FORMAT;
    }
}

static void get_cropped_picture_size_info(AVFrame *picture, OpenHevc_FrameInfo *openHevcFrameInfo){
    switch (picture->format) {
    case AV_PIX_FMT_YUV420P   :
        openHevcFrameInfo->chromat_format = YUV420;
        openHevcFrameInfo->nYPitch    = picture->width;
        openHevcFrameInfo->nUPitch    = picture->width >> 1;
        openHevcFrameInfo->nVPitch    = picture->width >> 1;
        break;
    case AV_PIX_FMT_YUV420P9  :
    case AV_PIX_FMT_YUV420P10 :
    case AV_PIX_FMT_YUV420P12 :
        openHevcFrameInfo->chromat_format = YUV420;
        openHevcFrameInfo->nYPitch    = picture->width << 1;
        openHevcFrameInfo->nUPitch    = picture->width;
        openHevcFrameInfo->nVPitch    = picture->width;
        break;
    case AV_PIX_FMT_YUV422P   :
        openHevcFrameInfo->chromat_format = YUV422;
        openHevcFrameInfo->nYPitch    = picture->width;
        openHevcFrameInfo->nUPitch    = picture->width >> 1;
        openHevcFrameInfo->nVPitch    = picture->width >> 1;
        break;
    case AV_PIX_FMT_YUV422P9  :
    case AV_PIX_FMT_YUV422P10 :
    case AV_PIX_FMT_YUV422P12 :
        openHevcFrameInfo->chromat_format = YUV422;
        openHevcFrameInfo->nYPitch    = picture->width << 1;
        openHevcFrameInfo->nUPitch    = picture->width;
        openHevcFrameInfo->nVPitch    = picture->width;
        break;
    case AV_PIX_FMT_YUV444P   :
        openHevcFrameInfo->chromat_format = YUV444;
        openHevcFrameInfo->nYPitch    = picture->width;
        openHevcFrameInfo->nUPitch    = picture->width;
        openHevcFrameInfo->nVPitch    = picture->width;
        break;
    case AV_PIX_FMT_YUV444P9  :
    case AV_PIX_FMT_YUV444P10 :
    case AV_PIX_FMT_YUV444P12 :
        openHevcFrameInfo->chromat_format = YUV444;
        openHevcFrameInfo->nYPitch    = picture->width << 1;
        openHevcFrameInfo->nUPitch    = picture->width << 1;
        openHevcFrameInfo->nVPitch    = picture->width << 1;
        break;
    default :
        openHevcFrameInfo->chromat_format = YUV420;
        openHevcFrameInfo->nYPitch    = picture->width;
        openHevcFrameInfo->nUPitch    = picture->width >> 1;
        openHevcFrameInfo->nVPitch    = picture->width >> 1;
        break;
    }
}

int oh_get_picture_params_from_layer(OpenHevc_Handle openHevcHandle, unsigned int layer_id, OpenHevc_FrameInfo *openHevcFrameInfo){

    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    int got_picture = openHevcContexts->got_picture_mask & (1 << layer_id);

    if (got_picture){

        OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[layer_id];
        AVFrame                 *picture          = openHevcContext->picture;

        openHevcFrameInfo->nYPitch    = picture->linesize[0];
        openHevcFrameInfo->nUPitch    = picture->linesize[1];
        openHevcFrameInfo->nVPitch    = picture->linesize[2];

        openHevcFrameInfo->chromat_format = get_chroma_format(picture->format);
        openHevcFrameInfo->nBitDepth      = get_picture_bitdepth(picture->format);

        openHevcFrameInfo->nWidth                  = picture->width;
        openHevcFrameInfo->nHeight                 = picture->height;
        openHevcFrameInfo->sample_aspect_ratio.num = picture->sample_aspect_ratio.num;
        openHevcFrameInfo->sample_aspect_ratio.den = picture->sample_aspect_ratio.den;
        openHevcFrameInfo->frameRate.num           = openHevcContext->c->time_base.den;
        openHevcFrameInfo->frameRate.den           = openHevcContext->c->time_base.num;
        openHevcFrameInfo->display_picture_number  = picture->display_picture_number;
        openHevcFrameInfo->flag                    = (picture->top_field_first << 2) | picture->interlaced_frame; //progressive, interlaced, interlaced bottom field first, interlaced top field first.
        openHevcFrameInfo->nTimeStamp              = picture->pts;

        return got_picture;
    }
    av_log(NULL,AV_LOG_DEBUG,"Could not get parameters for picture in layer %d , no such a picture\n",layer_id);
    return 0;
}

void libOpenHevcGetPictureInfo(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    unsigned int layer_id = openHevcContexts->display_layer;

    while (layer_id >= 0) {
        if (oh_get_picture_params_from_layer(openHevcHandle, layer_id, openHevcFrameInfo))
            break;
        --layer_id;
        av_log(NULL,AV_LOG_DEBUG,"Trying a lower layer_id %d\n",layer_id);
    }
}





int oh_get_cropped_picture_params_from_layer(OpenHevc_Handle openHevcHandle, unsigned int layer_id, OpenHevc_FrameInfo *openHevcFrameInfo)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    unsigned int got_picture = openHevcContexts->got_picture_mask & (1 << layer_id);

    if (got_picture){
        OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[layer_id];
        AVFrame                 *picture          = openHevcContext->picture;

        get_cropped_picture_size_info(picture,openHevcFrameInfo);

        openHevcFrameInfo->chromat_format = get_chroma_format(picture->format);

        openHevcFrameInfo->nBitDepth = get_picture_bitdepth(picture->format);

        openHevcFrameInfo->nWidth                  = picture->width;
        openHevcFrameInfo->nHeight                 = picture->height;
        openHevcFrameInfo->sample_aspect_ratio.num = picture->sample_aspect_ratio.num;
        openHevcFrameInfo->sample_aspect_ratio.den = picture->sample_aspect_ratio.den;
        openHevcFrameInfo->frameRate.num           = openHevcContext->c->time_base.den;
        openHevcFrameInfo->frameRate.den           = openHevcContext->c->time_base.num;
        openHevcFrameInfo->display_picture_number  = picture->display_picture_number;
        openHevcFrameInfo->flag                    = (picture->top_field_first << 2) | picture->interlaced_frame; //progressive, interlaced, interlaced bottom field first, interlaced top field first.
        openHevcFrameInfo->nTimeStamp              = picture->pts;
        return got_picture;
    }
    av_log(NULL,AV_LOG_DEBUG,"Could not get cropped parameters for picture in layer %d , no such a picture\n",layer_id);
    return 0;

}

void libOpenHevcGetPictureInfoCpy(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    int layer_id = openHevcContexts->display_layer;

    while (layer_id >= 0) {
        if(oh_get_cropped_picture_params_from_layer(openHevcHandle, layer_id ,openHevcFrameInfo))
            break;
        --layer_id;
        av_log(NULL,AV_LOG_DEBUG,"Trying a lower layer_id %d \n",layer_id);
    }
}


int oh_get_output_picture_from_layer(OpenHevc_Handle openHevcHandle, int layer_id, OpenHevc_Frame *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    int got_picture = openHevcContexts->got_picture_mask & (1 << layer_id);

    if (got_picture){

        OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[layer_id];

        openHevcFrame->pvY       = (void *) openHevcContext->picture->data[0];
        openHevcFrame->pvU       = (void *) openHevcContext->picture->data[1];
        openHevcFrame->pvV       = (void *) openHevcContext->picture->data[2];

        if(oh_get_picture_params_from_layer(openHevcHandle, layer_id, &openHevcFrame->frameInfo));
           return got_picture;
    }
    av_log(NULL, AV_LOG_DEBUG, "No frame found in output while trying to get picture from layer %d\n ", layer_id);
    return 0;
}

int oh_get_output_picture(OpenHevc_Handle openHevcHandle, OpenHevc_Frame *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    int layer_id =  openHevcContexts->display_layer;

    while (layer_id >= 0) {
        if(oh_get_output_picture_from_layer(openHevcHandle, layer_id, openHevcFrame))
            return 1;
        --layer_id;
        av_log(NULL,AV_LOG_DEBUG,"Trying a lower layer_id %d \n",layer_id);
    }
    return 0;
}

int libOpenHevcGetOutput(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    int ret = 0;

    int layer_id =  openHevcContexts->display_layer;

    if (got_picture){
        while (layer_id >= 0) {
           if (ret = oh_get_output_picture_from_layer(openHevcHandle, layer_id, openHevcFrame))
               return ret;

        --layer_id;
        av_log(NULL,AV_LOG_DEBUG,"Trying a lower layer_id for output %d \n",layer_id);
        }

    }
    return 0;
}

int oh_get__cropped_picture_copy_from_layer(OpenHevc_Handle openHevcHandle, int layer_id, OpenHevc_Frame_cpy *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    int got_picture = openHevcContexts->got_picture_mask & (1 << layer_id);

    if (got_picture){

        OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[layer_id];

        int y;
        int y_offset, y_offset2;
        int height, format;

        int src_stride;
        int dst_stride;
        int src_stride_c;
        int dst_stride_c;

        unsigned char *Y = (unsigned char *) openHevcFrame->pvY;
        unsigned char *U = (unsigned char *) openHevcFrame->pvU;
        unsigned char *V = (unsigned char *) openHevcFrame->pvV;

        oh_get_picture_params_from_layer(openHevcHandle,layer_id, &openHevcFrame->frameInfo);
        format = openHevcFrame->frameInfo.chromat_format == YUV420 ? 1 : 0;
        src_stride = openHevcFrame->frameInfo.nYPitch;
        src_stride_c = openHevcFrame->frameInfo.nUPitch;
        height = openHevcFrame->frameInfo.nHeight;

        oh_get_cropped_picture_params_from_layer(openHevcHandle,layer_id, &openHevcFrame->frameInfo);
        dst_stride = openHevcFrame->frameInfo.nYPitch;
        dst_stride_c = openHevcFrame->frameInfo.nUPitch;

        y_offset = y_offset2 = 0;

        for (y = 0; y < height; y++) {
            memcpy(&Y[y_offset2], &openHevcContext->picture->data[0][y_offset], dst_stride);
            y_offset  += src_stride;
            y_offset2 += dst_stride;
        }

        y_offset = y_offset2 = 0;

        for (y = 0; y < (height >> format); y++) {
            memcpy(&U[y_offset2], &openHevcContext->picture->data[1][y_offset], dst_stride_c);
            memcpy(&V[y_offset2], &openHevcContext->picture->data[2][y_offset], dst_stride_c);
            y_offset  += src_stride_c;
            y_offset2 += dst_stride_c;
        }
        return got_picture;
    } else {
        av_log(NULL,AV_LOG_DEBUG,"No picture found for copy in layer %d\n",layer_id);
        return 0;
    }
}

int oh_get_cropped_picture_copy(OpenHevc_Handle openHevcHandle, OpenHevc_Frame_cpy *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    int layer_id =  openHevcContexts->display_layer;
    int ret = 0;

    while (layer_id >= 0) {
        if (ret = oh_get__cropped_picture_copy_from_layer(openHevcHandle,layer_id,openHevcFrame))
            return ret;

        --layer_id;
        av_log(NULL,AV_LOG_DEBUG,"Trying a lower layer_id for copy %d \n",layer_id);
    }
    return 0;
}

int libOpenHevcGetOutputCpy(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame_cpy *openHevcFrame)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;

    int layer_id =  openHevcContexts->display_layer;
    int ret = 0;
    if (got_picture){
        while (layer_id >= 0) {
            if (ret = oh_get__cropped_picture_copy_from_layer(openHevcHandle,layer_id,openHevcFrame))
                return ret;
            --layer_id;
            av_log(NULL,AV_LOG_DEBUG,"Trying a lower layer_id for copy %d \n",layer_id);
        }
    }
    return 0;
}

void libOpenHevcSetDebugMode(OpenHevc_Handle openHevcHandle, OHEVC_LogLevel val)
{
	av_log_set_level(val);
}

void libOpenHevcSetLogCallback(OpenHevc_Handle openHevcHandle, void (*callback)(void*, int, const char*, va_list))
{
	av_log_set_callback(callback);
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
    //openHevcContexts->set_display = 1;
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
        av_opt_set_int(openHevcContext->c->priv_data, "temporal-layer-id", val, 0);
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

void libOpenHevcSetMouseClick(OpenHevc_Handle openHevcHandle, int val_x,int val_y)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;

    AVRational tmp;
    tmp.num=val_x;
    tmp.den=val_y;

    int i;

    for (i = 0; i < openHevcContexts->nb_decoders; i++) {
        openHevcContext = openHevcContexts->wraper[i];
        av_opt_set_q(openHevcContext->c->priv_data, "mouse-click-pos", tmp, 0);
    }

}

void libOpenHevcClose(OpenHevc_Handle openHevcHandle)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;

    for (i = openHevcContexts->nb_decoders-1; i >=0 ; i--){
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

void libOpenHevcFlushSVC(OpenHevc_Handle openHevcHandle, int decoderId)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext  = openHevcContexts->wraper[decoderId];

    openHevcContext->codec->flush(openHevcContext->c);
}

const char *libOpenHevcVersion(OpenHevc_Handle openHevcHandle)
{
    return "OpenHEVC v"NV_VERSION;
}

void oh_set_crypto_mode(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;

    for (i = 0; i < openHevcContexts->nb_decoders; i++) {
        openHevcContext = openHevcContexts->wraper[i];
        av_opt_set_int(openHevcContext->c->priv_data, "crypto-param", val, 0);
    }

}

void oh_set_crypto_key(OpenHevc_Handle openHevcHandle, uint8_t *val)
{
    OpenHevcWrapperContexts *openHevcContexts = (OpenHevcWrapperContexts *) openHevcHandle;
    OpenHevcWrapperContext  *openHevcContext;
    int i;

    for (i = 0; i < openHevcContexts->nb_decoders; i++) {
        openHevcContext = openHevcContexts->wraper[i];
        av_opt_set_bin(openHevcContext->c->priv_data, "crypto-key", val, 16*sizeof(uint8_t), 0);
    }

}
