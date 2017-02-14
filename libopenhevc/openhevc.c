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
#include "openhevc.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "version.h"

#define MAX_DECODERS 3


typedef struct OHContext {
    AVCodec        *codec;
    AVCodecContext *codec_ctx;
    AVCodecParserContext *parser_ctx;
    AVFrame *picture;
    AVPacket avpkt;
} OHContext;

typedef struct OHContextList {
    OHContext **ctx_list;
    int nb_decoders;
    int active_layer;
    int display_layer;
    int set_display;
    int set_vps;
} OHContextList;


//TODO check if the option has been set
static void init_oh_threads(OHContext *oh_ctx, int nb_pthreads,
                            OHThreadType thread_type)
{
    switch (thread_type) {
    case OH_THREAD_SLICE:
        av_opt_set(oh_ctx->codec_ctx, "thread_type", "slice", 0);
        break;
    case OH_THREAD_FRAMESLICE:
        av_opt_set(oh_ctx->codec_ctx, "thread_type", "frameslice", 0);
        break;
    default:
        av_opt_set(oh_ctx->codec_ctx, "thread_type", "frame", 0);
        break;
    }
    av_opt_set_int(oh_ctx->codec_ctx, "threads", nb_pthreads, 0);
}

OHHandle oh_init(int nb_pthreads, int thread_type)
{
    int i;
    OHContextList *oh_ctx_list = av_mallocz(sizeof(OHContextList));
    OHContext     *oh_ctx;

    avcodec_register_all();

    oh_ctx_list->nb_decoders   = MAX_DECODERS;
    oh_ctx_list->active_layer  = MAX_DECODERS-1;
    oh_ctx_list->display_layer = MAX_DECODERS-1;

    oh_ctx_list->ctx_list = av_malloc(sizeof(OHContext*)*oh_ctx_list->nb_decoders);

    for( i = 0; i < oh_ctx_list->nb_decoders; i++){
        oh_ctx = oh_ctx_list->ctx_list[i] = av_malloc(sizeof(OHContext));

        av_init_packet(&oh_ctx->avpkt);

        oh_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);

        if (!oh_ctx->codec) {
            av_log(NULL, AV_LOG_ERROR,
                   "OpenHEVC could not find a suitable codec for hevc stream\n");
            return NULL;
        }

        oh_ctx->parser_ctx  = av_parser_init( oh_ctx->codec->id );
        oh_ctx->codec_ctx   = avcodec_alloc_context3(oh_ctx->codec);
        oh_ctx->picture     = av_frame_alloc();

        oh_ctx->codec_ctx->flags |= AV_CODEC_FLAG_UNALIGNED;

        //FIXME OpenHEVC does not seem to use AV_CODEC_CAP_TRUNCATED
        if(oh_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
            oh_ctx->codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        init_oh_threads(oh_ctx, nb_pthreads, thread_type);

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "decoder-id", i, 0);
    }
    return (OHHandle) oh_ctx_list;
}

OHHandle oh_init_h264(int nb_pthreads, int thread_type)
{
    int i;
    OHContextList *oh_ctx_list = av_mallocz(sizeof(OHContextList));
    OHContext     *oh_ctx;

    avcodec_register_all();

    oh_ctx_list->nb_decoders   = MAX_DECODERS;
    oh_ctx_list->active_layer  = MAX_DECODERS-1;
    oh_ctx_list->display_layer = MAX_DECODERS-1;

    oh_ctx_list->ctx_list = av_malloc(sizeof(OHContext*)*oh_ctx_list->nb_decoders);

    for(i=0; i < oh_ctx_list->nb_decoders; i++){
        oh_ctx = oh_ctx_list->ctx_list[i] = av_malloc(sizeof(OHContext));

        av_init_packet(&oh_ctx->avpkt);

        oh_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_H264);

        if (!oh_ctx->codec) {
            av_log(NULL, AV_LOG_ERROR,
                   "OpenHEVC could not find a suitable codec for h264 stream\n");
            return NULL;
        }

        oh_ctx->parser_ctx  = av_parser_init( oh_ctx->codec->id );
        oh_ctx->codec_ctx   = avcodec_alloc_context3(oh_ctx->codec);
        oh_ctx->picture     = av_frame_alloc();
        //FIXME OpenHEVC does not seem to use AV_CODEC_FLAG_UNALIGNED
        oh_ctx->codec_ctx->flags |= AV_CODEC_FLAG_UNALIGNED;

        //FIXME OpenHEVC does not seem to use AV_CODEC_CAP_TRUNCATED
        if(oh_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
            oh_ctx->codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        init_oh_threads(oh_ctx, nb_pthreads, thread_type);

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "decoder-id", i, 0);
    }
    return (OHHandle) oh_ctx_list;
}

OHHandle oh_init_lhvc(int nb_pthreads, int thread_type)
{
    int i;
    OHContextList *oh_ctx_list = av_mallocz(sizeof(OHContextList));
    OHContext     *oh_ctx;

    avcodec_register_all();

    oh_ctx_list->nb_decoders   = MAX_DECODERS;
    oh_ctx_list->active_layer  = MAX_DECODERS-1;
    oh_ctx_list->display_layer = MAX_DECODERS-1;

    oh_ctx_list->ctx_list = av_malloc(sizeof(OHContext*)*oh_ctx_list->nb_decoders);

    for(i=0; i < oh_ctx_list->nb_decoders; i++){
        oh_ctx = oh_ctx_list->ctx_list[i] = av_malloc(sizeof(OHContext));

        av_init_packet(&oh_ctx->avpkt);

        if(i == 0)
            oh_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        else
            oh_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);

        if (!oh_ctx->codec) {
            av_log(NULL, AV_LOG_ERROR,
                   "OpenHEVC could not find a suitable codec for this stream\n");
            return NULL;
        }

        oh_ctx->parser_ctx  = av_parser_init( oh_ctx->codec->id );
        oh_ctx->codec_ctx   = avcodec_alloc_context3(oh_ctx->codec);
        oh_ctx->picture     = av_frame_alloc();
        //FIXME OpenHEVC does not seem to use AV_CODEC_FLAG_UNALIGNED
        oh_ctx->codec_ctx->flags |= AV_CODEC_FLAG_UNALIGNED;

        //FIXME OpenHEVC does not seem to use AV_CODEC_CAP_TRUNCATED
        if(oh_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
            oh_ctx->codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        init_oh_threads(oh_ctx, nb_pthreads, thread_type);

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "decoder-id", i, 0);
    }
    return (OHHandle) oh_ctx_list;
}

int oh_start(OHHandle openHevcHandle)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx;
    int i, err;
    for(i=0; i < oh_ctx_list->nb_decoders; i++) {
        oh_ctx = oh_ctx_list->ctx_list[i];
        if (err = avcodec_open2(oh_ctx->codec_ctx, oh_ctx->codec, NULL) < 0) {
            av_log(oh_ctx->codec_ctx,AV_LOG_ERROR,
                   "OpenHEVC could not open the decoder\n");
            return err;
        }
        if(i+1 < oh_ctx_list->nb_decoders)
            oh_ctx_list->ctx_list[i+1]->codec_ctx->BL_avcontext =
                    oh_ctx_list->ctx_list[i]->codec_ctx;
    }
    return 1;
}

int oh_decode(OHHandle openHevcHandle, const unsigned char *buff, int au_len,
              int64_t pts)
{
    int got_picture[MAX_DECODERS], len=0, i, max_layer;
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx;

    for(i =0; i < MAX_DECODERS; i++)  {
        oh_ctx         = oh_ctx_list->ctx_list[i];
        got_picture[i] = 0;

        oh_ctx->codec_ctx->quality_id = oh_ctx_list->active_layer;

        if (i <= oh_ctx_list->active_layer) {
            oh_ctx->avpkt.size = au_len;
            oh_ctx->avpkt.data = (uint8_t *) buff;
        } else {
            oh_ctx->avpkt.size = 0;
            oh_ctx->avpkt.data = NULL;
        }
        oh_ctx->avpkt.pts  = pts;

        len = avcodec_decode_video2(oh_ctx->codec_ctx, oh_ctx->picture,
                                    &got_picture[i], &oh_ctx->avpkt);

        if(i+1 < oh_ctx_list->nb_decoders)
            oh_ctx_list->ctx_list[i+1]->codec_ctx->BL_frame =
                    oh_ctx_list->ctx_list[i]->codec_ctx->BL_frame;
    }

    if (len < 0) {
        fprintf(stderr, "Error while decoding frame \n");
        return -1;
    }

    if(oh_ctx_list->set_display)
        max_layer = oh_ctx_list->display_layer;
    else
        max_layer = oh_ctx_list->active_layer;

    for(i=max_layer; i>=0; i--) {
        if(got_picture[i]){
            if(i == oh_ctx_list->display_layer) {
                if (i >= 0 && i < oh_ctx_list->nb_decoders)
                    oh_ctx_list->display_layer = i;
                return got_picture[i];
            }
        }
    }
    return 0;
}

//FIXME: There should be a better way to synchronize decoders
static int poc_id;

int oh_decode_lhvc(OHHandle openHevcHandle, const unsigned char *buff,
                   const unsigned char *buff2, int nal_len, int nal_len2,
                   int64_t pts, int64_t pts2)
{
    int got_picture[MAX_DECODERS], len=0, i, max_layer, gpic;
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext  *oh_ctx;

    poc_id++;
    poc_id &= 1023;

    for(i =0; i < MAX_DECODERS; i++)  {
        got_picture[i] = 0;
        len = 0;
        oh_ctx                = oh_ctx_list->ctx_list[i];
        oh_ctx->codec_ctx->quality_id = oh_ctx_list->active_layer;
        if(i==0){
            oh_ctx->avpkt.size = nal_len;
            oh_ctx->avpkt.data = (uint8_t *) buff;
            oh_ctx->avpkt.pts  = pts;
            oh_ctx->avpkt.poc_id = poc_id;
            if(buff2 && oh_ctx_list->active_layer){
                oh_ctx->avpkt.el_available=1;
            }else {
                oh_ctx->avpkt.el_available=0;
            }
        } else if(i > 0 && i <= oh_ctx_list->active_layer){
            oh_ctx->avpkt.size = nal_len2;
            oh_ctx->avpkt.data = (uint8_t *) buff2;
            oh_ctx->avpkt.pts  = pts2;
            oh_ctx->avpkt.poc_id = poc_id;
            if(buff){
                oh_ctx->avpkt.bl_available=1;
            }
            else {
                oh_ctx->avpkt.bl_available=0;
            }
        } else {
            oh_ctx->avpkt.size = 0;
            oh_ctx->avpkt.data = NULL;
        }

        len = avcodec_decode_video2(oh_ctx->codec_ctx, oh_ctx->picture,
                                    &got_picture[i], &oh_ctx->avpkt);

        if(i+1 < oh_ctx_list->nb_decoders)
            oh_ctx_list->ctx_list[i+1]->codec_ctx->BL_frame =
                    oh_ctx_list->ctx_list[i]->codec_ctx->BL_frame;
        //Fixme: Single Layer SHVC decoding
        if(got_picture[i])
           gpic += 1 << i;

        if (len < 0) {
            fprintf(stderr, "Error while decoding frame \n");
            return -1;
        }
    }

    if(oh_ctx_list->set_display)
        max_layer = oh_ctx_list->display_layer;
    else
        max_layer = oh_ctx_list->active_layer;

    for(i=max_layer; i>=0; i--) {
        if(got_picture[i]){
            if(i == oh_ctx_list->display_layer) {
                if (i >= 0 && i < oh_ctx_list->nb_decoders)
                    oh_ctx_list->display_layer = i;
                return got_picture[i];
            }
        }
    }
    return 0;
}


void oh_extradata_cpy(OHHandle openHevcHandle, unsigned char *extra_data,
                      int extra_size_alloc)
{
    int i;
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx;

    for(i =0; i <= oh_ctx_list->active_layer; i++)  {
        oh_ctx = oh_ctx_list->ctx_list[i];
        oh_ctx->codec_ctx->extradata = (uint8_t*)av_mallocz(extra_size_alloc);
        memcpy( oh_ctx->codec_ctx->extradata, extra_data, extra_size_alloc);
        oh_ctx->codec_ctx->extradata_size = extra_size_alloc;
    }
}

void oh_extradata_cpy_lhvc(OHHandle openHevcHandle, unsigned char *extra_data_linf,
                           unsigned char *extra_data_lsup, int extra_size_alloc_linf,
                           int extra_size_alloc_lsup)
{
    int i;
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx;

    if (extra_data_linf && extra_size_alloc_linf) {
        oh_ctx = oh_ctx_list->ctx_list[0];
        if (oh_ctx->codec_ctx->extradata) av_freep(&oh_ctx->codec_ctx->extradata);
        oh_ctx->codec_ctx->extradata = (uint8_t*)av_mallocz(extra_size_alloc_linf);
        memcpy( oh_ctx->codec_ctx->extradata, extra_data_linf, extra_size_alloc_linf);
        oh_ctx->codec_ctx->extradata_size = extra_size_alloc_linf;
    }

    if (extra_data_lsup && extra_size_alloc_lsup) {
        for(i =1; i <= oh_ctx_list->active_layer; i++)  {
            oh_ctx = oh_ctx_list->ctx_list[i];
            if (oh_ctx->codec_ctx->extradata) av_freep(&oh_ctx->codec_ctx->extradata);
            oh_ctx->codec_ctx->extradata = (uint8_t*)av_mallocz(extra_size_alloc_lsup);
            memcpy( oh_ctx->codec_ctx->extradata, extra_data_lsup, extra_size_alloc_lsup);
            oh_ctx->codec_ctx->extradata_size = extra_size_alloc_lsup;
        }
    }
}


void oh_frameinfo_update(OHHandle openHevcHandle, OHFrameInfo *oh_frameinfo)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx      = oh_ctx_list->ctx_list[oh_ctx_list->display_layer];
    AVFrame       *picture     = oh_ctx->picture;

    oh_frameinfo->linesize_y    = picture->linesize[0];

    switch (picture->format) {
    case AV_PIX_FMT_YUV420P   :
    case AV_PIX_FMT_YUV420P9  :
    case AV_PIX_FMT_YUV420P10 :
    case AV_PIX_FMT_YUV420P12 :
        oh_frameinfo->linesize_cb    = picture->linesize[1];
        oh_frameinfo->linesize_cr    = picture->linesize[2];
        oh_frameinfo->chromat_format = OH_YUV420;
        break;
    case AV_PIX_FMT_YUV422P   :
    case AV_PIX_FMT_YUV422P9  :
    case AV_PIX_FMT_YUV422P10 :
    case AV_PIX_FMT_YUV422P12 :
        oh_frameinfo->linesize_cb    = picture->linesize[1];
        oh_frameinfo->linesize_cr    = picture->linesize[2];
        oh_frameinfo->chromat_format = OH_YUV422;
        break;
    case AV_PIX_FMT_YUV444P   :
    case AV_PIX_FMT_YUV444P9  :
    case AV_PIX_FMT_YUV444P10 :
    case AV_PIX_FMT_YUV444P12 :
        oh_frameinfo->linesize_cb    = picture->linesize[1];
        oh_frameinfo->linesize_cr    = picture->linesize[2];
        oh_frameinfo->chromat_format = OH_YUV444;
        break;
    default :
        oh_frameinfo->linesize_cb    = picture->linesize[1];
        oh_frameinfo->linesize_cr    = picture->linesize[2];
        break;
    }

    switch (picture->format) {
    case AV_PIX_FMT_YUV420P   :
    case AV_PIX_FMT_YUV422P   :
    case AV_PIX_FMT_YUV444P   :
        oh_frameinfo->bitdepth  =  8;
        break;
    case AV_PIX_FMT_YUV420P9  :
    case AV_PIX_FMT_YUV422P9  :
    case AV_PIX_FMT_YUV444P9  :
        oh_frameinfo->bitdepth  =  9;
        break;
    case AV_PIX_FMT_YUV420P10 :
    case AV_PIX_FMT_YUV422P10 :
    case AV_PIX_FMT_YUV444P10 :
        oh_frameinfo->bitdepth  = 10;
        break;
    case AV_PIX_FMT_YUV420P12 :
    case AV_PIX_FMT_YUV422P12 :
    case AV_PIX_FMT_YUV444P12 :
        oh_frameinfo->bitdepth  = 12;
        break;
    default:
        oh_frameinfo->bitdepth   =  8;
        break;
    }

    oh_frameinfo->width                  = picture->width;
    oh_frameinfo->height                 = picture->height;

    oh_frameinfo->sample_aspect_ratio.num = picture->sample_aspect_ratio.num;
    oh_frameinfo->sample_aspect_ratio.den = picture->sample_aspect_ratio.den;

    oh_frameinfo->framerate.num           = oh_ctx->codec_ctx->time_base.den;
    oh_frameinfo->framerate.den           = oh_ctx->codec_ctx->time_base.num;

    oh_frameinfo->display_picture_number  = picture->display_picture_number;
    oh_frameinfo->flag                    = (picture->top_field_first << 2) |
            picture->interlaced_frame; //progressive, interlaced, interlaced bottom field first, interlaced top field first.
    oh_frameinfo->pts = picture->pts;
}

void oh_frameinfo_cpy(OHHandle openHevcHandle, OHFrameInfo *oh_frameinfo)
{

    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext  *oh_ctx  = oh_ctx_list->ctx_list[oh_ctx_list->display_layer];
    AVFrame    *picture = oh_ctx->picture;

    switch (picture->format) {
    case AV_PIX_FMT_YUV420P   :
        oh_frameinfo->chromat_format = OH_YUV420;
        oh_frameinfo->linesize_y     = picture->width;
        oh_frameinfo->linesize_cb    = picture->width >> 1;
        oh_frameinfo->linesize_cr    = picture->width >> 1;
        break;
    case AV_PIX_FMT_YUV420P9  :
    case AV_PIX_FMT_YUV420P10 :
    case AV_PIX_FMT_YUV420P12 :
        oh_frameinfo->chromat_format = OH_YUV420;
        oh_frameinfo->linesize_y     = picture->width << 1;
        oh_frameinfo->linesize_cb    = picture->width;
        oh_frameinfo->linesize_cr    = picture->width;
        break;
    case AV_PIX_FMT_YUV422P   :
        oh_frameinfo->chromat_format = OH_YUV422;
        oh_frameinfo->linesize_y     = picture->width;
        oh_frameinfo->linesize_cb    = picture->width >> 1;
        oh_frameinfo->linesize_cr    = picture->width >> 1;
        break;
    case AV_PIX_FMT_YUV422P9  :
    case AV_PIX_FMT_YUV422P10 :
    case AV_PIX_FMT_YUV422P12 :
        oh_frameinfo->chromat_format = OH_YUV422;
        oh_frameinfo->linesize_y     = picture->width << 1;
        oh_frameinfo->linesize_cb    = picture->width;
        oh_frameinfo->linesize_cr    = picture->width;
        break;
    case AV_PIX_FMT_YUV444P   :
        oh_frameinfo->chromat_format = OH_YUV444;
        oh_frameinfo->linesize_y     = picture->width;
        oh_frameinfo->linesize_cb    = picture->width;
        oh_frameinfo->linesize_cr    = picture->width;
        break;
    case AV_PIX_FMT_YUV444P9  :
    case AV_PIX_FMT_YUV444P10 :
    case AV_PIX_FMT_YUV444P12 :
        oh_frameinfo->chromat_format = OH_YUV444;
        oh_frameinfo->linesize_y     = picture->width << 1;
        oh_frameinfo->linesize_cb    = picture->width << 1;
        oh_frameinfo->linesize_cr    = picture->width << 1;
        break;
    default :
        oh_frameinfo->chromat_format = OH_YUV420;
        oh_frameinfo->linesize_y     = picture->width;
        oh_frameinfo->linesize_cb    = picture->width >> 1;
        oh_frameinfo->linesize_cr    = picture->width >> 1;
        break;
    }

    switch (picture->format) {
    case AV_PIX_FMT_YUV420P   :
    case AV_PIX_FMT_YUV422P   :
    case AV_PIX_FMT_YUV444P   :
        oh_frameinfo->bitdepth  =  8;
        break;
    case AV_PIX_FMT_YUV420P9  :
    case AV_PIX_FMT_YUV422P9  :
    case AV_PIX_FMT_YUV444P9  :
        oh_frameinfo->bitdepth  =  9;
        break;
    case AV_PIX_FMT_YUV420P10 :
    case AV_PIX_FMT_YUV422P10 :
    case AV_PIX_FMT_YUV444P10 :
        oh_frameinfo->bitdepth  = 10;
        break;
    case AV_PIX_FMT_YUV420P12 :
    case AV_PIX_FMT_YUV422P12 :
    case AV_PIX_FMT_YUV444P12 :
        oh_frameinfo->bitdepth  = 12;
        break;
    default :
        oh_frameinfo->bitdepth   =  8;
        break;
    }

    oh_frameinfo->width                   = picture->width;
    oh_frameinfo->height                  = picture->height;
    oh_frameinfo->sample_aspect_ratio.num = picture->sample_aspect_ratio.num;
    oh_frameinfo->sample_aspect_ratio.den = picture->sample_aspect_ratio.den;
    oh_frameinfo->framerate.num           = oh_ctx->codec_ctx->time_base.den;
    oh_frameinfo->framerate.den           = oh_ctx->codec_ctx->time_base.num;
    oh_frameinfo->display_picture_number  = picture->display_picture_number;
    oh_frameinfo->flag                    = (picture->top_field_first << 2) |
            picture->interlaced_frame; //progressive, interlaced, interlaced bottom field first, interlaced top field first.
    oh_frameinfo->pts              = picture->pts;
}

int oh_output_update(OHHandle openHevcHandle, int got_picture, OHFrame *openHevcFrame)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext  *oh_ctx  = oh_ctx_list->ctx_list[oh_ctx_list->display_layer];

    if (got_picture) {
        openHevcFrame->data_y_p   = (void *) oh_ctx->picture->data[0];
        openHevcFrame->data_cb_p  = (void *) oh_ctx->picture->data[1];
        openHevcFrame->data_cr_p  = (void *) oh_ctx->picture->data[2];

        oh_frameinfo_update(openHevcHandle, &openHevcFrame->frame_par);
    }
    return 1;
}

int oh_output_cpy(OHHandle openHevcHandle, int got_picture, OHFrame_cpy *openHevcFrame)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx  = oh_ctx_list->ctx_list[oh_ctx_list->display_layer];

    int y;
    int y_offset, y_offset2;
    if( got_picture ) {
        unsigned char *Y = (unsigned char *) openHevcFrame->data_y;
        unsigned char *U = (unsigned char *) openHevcFrame->data_cb;
        unsigned char *V = (unsigned char *) openHevcFrame->data_cr;

        int height, format;
        int src_stride;
        int dst_stride;
        int src_stride_c;
        int dst_stride_c;

        oh_frameinfo_update(openHevcHandle, &openHevcFrame->frame_par);
        format = openHevcFrame->frame_par.chromat_format == OH_YUV420 ? 1 : 0;

        src_stride   = openHevcFrame->frame_par.linesize_y;
        src_stride_c = openHevcFrame->frame_par.linesize_cb;

        height = openHevcFrame->frame_par.height;

        oh_frameinfo_cpy(openHevcHandle, &openHevcFrame->frame_par);

        dst_stride   = openHevcFrame->frame_par.linesize_y;
        dst_stride_c = openHevcFrame->frame_par.linesize_cb;

        y_offset = y_offset2 = 0;

        for (y = 0; y < height; y++) {
            memcpy(&Y[y_offset2], &oh_ctx->picture->data[0][y_offset], dst_stride);
            y_offset  += src_stride;
            y_offset2 += dst_stride;
        }

        y_offset = y_offset2 = 0;

        for (y = 0; y < (height >> format); y++) {
            memcpy(&U[y_offset2], &oh_ctx->picture->data[1][y_offset], dst_stride_c);
            memcpy(&V[y_offset2], &oh_ctx->picture->data[2][y_offset], dst_stride_c);
            y_offset  += src_stride_c;
            y_offset2 += dst_stride_c;
        }
    }
    return 1;
}

void oh_set_log_level(OHHandle openHevcHandle, OHEVC_LogLevel val)
{
    av_log_set_level(val);
}

void oh_set_log_callback(OHHandle openHevcHandle, void (*callback)(void*, int, const char*, va_list))
{
    av_log_set_callback(callback);
}

void oh_select_active_layer(OHHandle openHevcHandle, int val)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    if (val >= 0 && val < oh_ctx_list->nb_decoders)
        oh_ctx_list->active_layer = val;
    else {
        fprintf(stderr, "The requested layer %d can not be decoded (it exceeds the number of allocated decoders %d ) \n", val, oh_ctx_list->nb_decoders);
        oh_ctx_list->active_layer = oh_ctx_list->nb_decoders-1;
    }
}

//FIXME: The layer need also to be active in order to be decoded
void oh_select_view_layer(OHHandle openHevcHandle, int val)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;

    if (val >= 0 && val < oh_ctx_list->nb_decoders)
        oh_ctx_list->display_layer = val;
    else {
        fprintf(stderr,
                "The requested layer %d can not be viewed (it exceeds the number of allocated decoders %d ) \n", val, oh_ctx_list->nb_decoders);
        oh_ctx_list->display_layer = oh_ctx_list->nb_decoders-1;
    }
}


void oh_enable_sei_checksum(OHHandle openHevcHandle, int val)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext  *oh_ctx;

    int i;

    for (i = 0; i < oh_ctx_list->nb_decoders; i++) {
        oh_ctx = oh_ctx_list->ctx_list[i];

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "decode-checksum", val, 0);
    }
}

void oh_select_temporal_layer(OHHandle openHevcHandle, int val)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx;

    int i;

    for (i = 0; i < oh_ctx_list->nb_decoders; i++) {
        oh_ctx = oh_ctx_list->ctx_list[i];

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "temporal-layer-id", val, 0);
    }
}

void oh_disable_cropping(OHHandle openHevcHandle, int val)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx;
    int i;

    for (i = 0; i < oh_ctx_list->nb_decoders; i++) {
        oh_ctx = oh_ctx_list->ctx_list[i];

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "no-cropping", val, 0);
    }
}

void oh_close(OHHandle openHevcHandle)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx;
    int i;

    for (i = oh_ctx_list->nb_decoders-1; i >=0 ; i--){
        oh_ctx = oh_ctx_list->ctx_list[i];

        avcodec_close(oh_ctx->codec_ctx);

        av_parser_close(oh_ctx->parser_ctx);

        av_freep(&oh_ctx->codec_ctx);
        av_freep(&oh_ctx->picture);
        av_freep(&oh_ctx);
    }
    av_freep(&oh_ctx_list->ctx_list);
    av_freep(&oh_ctx_list);
}

void oh_flush(OHHandle openHevcHandle)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx  = oh_ctx_list->ctx_list[oh_ctx_list->active_layer];

    oh_ctx->codec->flush(oh_ctx->codec_ctx);
}

void oh_flush_shvc(OHHandle openHevcHandle, int decoderId)
{
    OHContextList *oh_ctx_list = (OHContextList *) openHevcHandle;
    OHContext     *oh_ctx      = oh_ctx_list->ctx_list[decoderId];

    oh_ctx->codec->flush(oh_ctx->codec_ctx);
}

const unsigned oh_version(OHHandle openHevcHandle)
{
    return LIBOPENHEVC_VERSION_INT;
}

