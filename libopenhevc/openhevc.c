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
#include "ohconfig.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/thread.h"
#include "version.h"

#define MAX_DECODERS 3


typedef struct OHDecoderCtx {
    AVCodec        *codec;
    AVCodecContext *codec_ctx;
    AVCodecParserContext *parser_ctx;
    AVFrame *picture;
    AVPacket avpkt;
} OHDecoderCtx;

typedef struct OHContext {
    OHDecoderCtx **ctx_list;       ///< List of active decoder contexts
    int nb_decoders;               ///< Number of allocated decoders
    int target_active_layer;       ///< Maximum targeted output layer
    int target_display_layer;      ///< Maximum targeted display layer
    int target_output_layer_mask;  ///< Mask for output layers
    int target_display_layer_mask; ///< Mask for display layers
    int got_picture_mask;          ///< Mask of picture outputs from decoder
    int current_max_ouput_layer;   ///< Maximum current output layer
    int set_vps;
    pthread_mutex_t layer_switch;  ///< mutex to avoid in loop changes
} OHContext;


static void init_oh_threads(OHDecoderCtx *oh_ctx, int nb_pthreads,
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
    OHContext        *oh_ctx  = av_mallocz(sizeof(OHContext));
    OHDecoderCtx     *oh_decoder_ctx;

    av_log(NULL,AV_LOG_DEBUG, "INIT openHEVC context\n");

    avcodec_register_all();

    oh_ctx->nb_decoders          = MAX_DECODERS;
    oh_ctx->target_active_layer  = MAX_DECODERS-1;
    oh_ctx->target_display_layer = MAX_DECODERS-1;

    oh_ctx->ctx_list = av_malloc(sizeof(OHDecoderCtx*)*oh_ctx->nb_decoders);

    pthread_mutex_init(&oh_ctx->layer_switch ,NULL);

    for( i = 0; i < oh_ctx->nb_decoders; i++){
        oh_decoder_ctx = oh_ctx->ctx_list[i] = av_malloc(sizeof(OHDecoderCtx));
        av_init_packet(&oh_decoder_ctx->avpkt);

        oh_decoder_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);

        if (!oh_decoder_ctx->codec) {
            av_log(NULL, AV_LOG_ERROR,
                   "OpenHEVC could not find a suitable codec for hevc stream\n");
            return NULL;
        }

        oh_decoder_ctx->parser_ctx  = av_parser_init( oh_decoder_ctx->codec->id );
        oh_decoder_ctx->codec_ctx   = avcodec_alloc_context3(oh_decoder_ctx->codec);
        oh_decoder_ctx->picture     = av_frame_alloc();

        av_log(oh_decoder_ctx->codec_ctx,AV_LOG_ERROR, "test\n");

        oh_decoder_ctx->codec_ctx->flags |= AV_CODEC_FLAG_UNALIGNED;

        //FIXME OpenHEVC does not seem to use AV_CODEC_CAP_TRUNCATED
        if(oh_decoder_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
            oh_decoder_ctx->codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        init_oh_threads(oh_decoder_ctx, nb_pthreads, thread_type);

        av_opt_set_int(oh_decoder_ctx->codec_ctx->priv_data, "decoder-id", i, 0);
    }
    return (OHHandle) oh_ctx;
}

#if OHCONFIG_AVCBASE
OHHandle oh_init_h264(int nb_pthreads, int thread_type)
{
    int i;
    OHContext        *oh_ctx = av_mallocz(sizeof(OHContext));
    OHDecoderCtx     *oh_decoder_ctx;

    avcodec_register_all();

    oh_ctx->nb_decoders          = MAX_DECODERS;
    oh_ctx->target_active_layer  = MAX_DECODERS-1;
    oh_ctx->target_display_layer = MAX_DECODERS-1;

    oh_ctx->ctx_list = av_malloc(sizeof(OHDecoderCtx*)*oh_ctx->nb_decoders);

    for(i=0; i < oh_ctx->nb_decoders; i++){
        oh_decoder_ctx = oh_ctx->ctx_list[i] = av_malloc(sizeof(OHDecoderCtx));

        av_init_packet(&oh_decoder_ctx->avpkt);

        oh_decoder_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_H264);

        if (!oh_decoder_ctx->codec) {
            av_log(NULL, AV_LOG_ERROR,
                   "OpenHEVC could not find a suitable codec for h264 stream\n");
            return NULL;
        }

        oh_decoder_ctx->parser_ctx  = av_parser_init( oh_decoder_ctx->codec->id );
        oh_decoder_ctx->codec_ctx   = avcodec_alloc_context3(oh_decoder_ctx->codec);
        oh_decoder_ctx->picture     = av_frame_alloc();
        //FIXME OpenHEVC does not seem to use AV_CODEC_FLAG_UNALIGNED
        oh_decoder_ctx->codec_ctx->flags |= AV_CODEC_FLAG_UNALIGNED;

        //FIXME OpenHEVC does not seem to use AV_CODEC_CAP_TRUNCATED
        if(oh_decoder_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
            oh_decoder_ctx->codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        init_oh_threads(oh_decoder_ctx, nb_pthreads, thread_type);

        av_opt_set_int(oh_decoder_ctx->codec_ctx->priv_data, "decoder-id", i, 0);
    }
    return (OHHandle) oh_ctx;
}


OHHandle oh_init_lhvc(int nb_pthreads, int thread_type)
{
    int i;
    OHContext        *oh_ctx = av_mallocz(sizeof(OHContext));
    OHDecoderCtx     *oh_decoder_ctx;

    avcodec_register_all();

    oh_ctx->nb_decoders          = MAX_DECODERS;
    oh_ctx->target_active_layer  = MAX_DECODERS-1;
    oh_ctx->target_display_layer = MAX_DECODERS-1;

    oh_ctx->ctx_list = av_malloc(sizeof(OHDecoderCtx*)*oh_ctx->nb_decoders);

    for(i=0; i < oh_ctx->nb_decoders; i++){
        oh_decoder_ctx = oh_ctx->ctx_list[i] = av_malloc(sizeof(OHDecoderCtx));

        av_init_packet(&oh_decoder_ctx->avpkt);

        if(i == 0)
            oh_decoder_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        else
            oh_decoder_ctx->codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);

        if (!oh_decoder_ctx->codec) {
            av_log(NULL, AV_LOG_ERROR,
                   "OpenHEVC could not find a suitable codec for this stream\n");
            return NULL;
        }

        oh_decoder_ctx->parser_ctx  = av_parser_init( oh_decoder_ctx->codec->id );
        oh_decoder_ctx->codec_ctx   = avcodec_alloc_context3(oh_decoder_ctx->codec);
        oh_decoder_ctx->picture     = av_frame_alloc();
        //FIXME OpenHEVC does not seem to use AV_CODEC_FLAG_UNALIGNED
        oh_decoder_ctx->codec_ctx->flags |= AV_CODEC_FLAG_UNALIGNED;

        //FIXME OpenHEVC does not seem to use AV_CODEC_CAP_TRUNCATED
        if(oh_decoder_ctx->codec->capabilities & AV_CODEC_CAP_TRUNCATED)
            oh_decoder_ctx->codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        init_oh_threads(oh_decoder_ctx, nb_pthreads, thread_type);

        av_opt_set_int(oh_decoder_ctx->codec_ctx->priv_data, "decoder-id", i, 0);
        if(i == 1){
            av_opt_set_int(oh_decoder_ctx->codec_ctx->priv_data, "bl_is_avc", 1, 0);
        }
    }
    return (OHHandle) oh_ctx;
}
#endif

int oh_start(OHHandle openHevcHandle)
{
    OHContext        *oh_ctx = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_decoder_ctx;

    int i, err;

    av_log(NULL,AV_LOG_DEBUG, "START openHEVC decoders\n");

    for(i=0; i < oh_ctx->nb_decoders; i++) {
        oh_decoder_ctx = oh_ctx->ctx_list[i];
        if (err = avcodec_open2(oh_decoder_ctx->codec_ctx, oh_decoder_ctx->codec, NULL) < 0) {
            av_log(oh_decoder_ctx->codec_ctx,AV_LOG_ERROR,
                   "openHEVC could not open the decoder\n");
            return err;
        }
        if(i+1 < oh_ctx->nb_decoders)
            oh_ctx->ctx_list[i+1]->codec_ctx->BL_avcontext =
                    oh_ctx->ctx_list[i]->codec_ctx;
    }
    return 1;
}

int oh_decode(OHHandle openHevcHandle, const unsigned char *buff, int au_len,
              int64_t pts)
{
    int i;
    int ret = 0;
    int err = 0;

    OHContext        *oh_ctx = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_decoder_ctx;

    int target_active_layer = oh_ctx->target_active_layer;

    oh_ctx->got_picture_mask = 0;

    pthread_mutex_lock(&oh_ctx->layer_switch);

    for(i =0; i < MAX_DECODERS - 1; i++)  {
        int got_picture = 0;
        oh_decoder_ctx         = oh_ctx->ctx_list[i];

        oh_decoder_ctx->codec_ctx->quality_id = oh_ctx->target_active_layer;

        if (i <= oh_ctx->target_active_layer){
            oh_decoder_ctx->avpkt.size = au_len;
            oh_decoder_ctx->avpkt.data = (uint8_t *) buff;
            oh_decoder_ctx->avpkt.pts  = pts;

AV_NOWARN_DEPRECATED(
            err = avcodec_decode_video2(oh_decoder_ctx->codec_ctx, oh_decoder_ctx->picture,
                                        &got_picture, &oh_decoder_ctx->avpkt);)
            ret |= (got_picture << i);
        } else {
            oh_decoder_ctx->avpkt.size = 0;
            oh_decoder_ctx->avpkt.data = NULL;
            oh_decoder_ctx->avpkt.pts  = 0;
            avcodec_flush_buffers(oh_decoder_ctx->codec_ctx);
        }

        if(i < oh_ctx->target_active_layer)
            oh_ctx->ctx_list[i+1]->codec_ctx->BL_frame =
                    oh_ctx->ctx_list[i]->codec_ctx->BL_frame;
    }
    pthread_mutex_unlock(&oh_ctx->layer_switch);

    oh_ctx->got_picture_mask = ret;

    if (err < 0) {
        av_log(NULL,AV_LOG_ERROR,"openHEVC decoder returned an error while decoding frame \n");
        return err;
    }

    return oh_ctx->got_picture_mask;
}


#if OHCONFIG_AVCBASE
//FIXME: There should be a better way to synchronize decoders
static int poc_id;

int oh_decode_lhvc(OHHandle openHevcHandle, const unsigned char *buff,
                   const unsigned char *buff2, int nal_len, int nal_len2,
                   int64_t pts, int64_t pts2)
{
    int i;
    int ret = 0;
    int err = 0;

    OHContext     *oh_ctx = (OHContext *) openHevcHandle;
    OHDecoderCtx  *oh_decoder_ctx;

    int target_active_layer = oh_ctx->target_active_layer;

    oh_ctx->got_picture_mask = 0;

    poc_id++;
    poc_id &= 1023;

    for(i =0; i < MAX_DECODERS; i++)  {
        int got_picture = 0;
        oh_decoder_ctx                = oh_ctx->ctx_list[i];
        oh_decoder_ctx->codec_ctx->quality_id = target_active_layer;

        if(buff && i==0){
            oh_decoder_ctx->avpkt.size = nal_len;
            oh_decoder_ctx->avpkt.data = (uint8_t *) buff;
            oh_decoder_ctx->avpkt.pts  = pts;
            oh_decoder_ctx->avpkt.poc_id = poc_id;

            if(buff2 && target_active_layer){
                oh_decoder_ctx->avpkt.el_available=1;
            } else {
                oh_decoder_ctx->avpkt.el_available=0;
            }

            AV_NOWARN_DEPRECATED(
            err = avcodec_decode_video2(oh_decoder_ctx->codec_ctx, oh_decoder_ctx->picture,
                                        &got_picture, &oh_decoder_ctx->avpkt);)

            ret |= (got_picture << i);

            oh_ctx->got_picture_mask = ret;
        } else if( buff2 && i > 0 && i <= target_active_layer){
            oh_decoder_ctx->avpkt.size = nal_len2;
            oh_decoder_ctx->avpkt.data = (uint8_t *) buff2;
            oh_decoder_ctx->avpkt.pts  = pts2;
            oh_decoder_ctx->avpkt.poc_id = poc_id;

            if(buff){
                oh_decoder_ctx->avpkt.bl_available=1;
            }
            else {
                oh_decoder_ctx->avpkt.bl_available=0;
            }

            AV_NOWARN_DEPRECATED(
            err = avcodec_decode_video2(oh_decoder_ctx->codec_ctx, oh_decoder_ctx->picture,
                                        &got_picture, &oh_decoder_ctx->avpkt);)

            ret |= (got_picture << i);
            oh_ctx->got_picture_mask = ret;

        } else {
            avcodec_flush_buffers(oh_decoder_ctx->codec_ctx);
            oh_decoder_ctx->avpkt.size = 0;
            oh_decoder_ctx->avpkt.data = NULL;
        }

        if(i < target_active_layer)
            oh_ctx->ctx_list[i+1]->codec_ctx->BL_frame =
                    oh_ctx->ctx_list[i]->codec_ctx->BL_frame;
    }

    if (err < 0) {
        av_log(NULL,AV_LOG_ERROR,"openHEVC decoder returned an error while decoding frame \n");
        return err;
    }

    return oh_ctx->got_picture_mask;
}
#endif

void oh_extradata_cpy(OHHandle openHevcHandle, unsigned char *extra_data,
                      int extra_size_alloc)
{
    int i;
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_ctx;
    for(i =0; i <= oh_ctx_list->target_active_layer; i++)  {
        oh_ctx = oh_ctx_list->ctx_list[i];
        oh_ctx->codec_ctx->extradata = (uint8_t*)av_mallocz(extra_size_alloc);
        memcpy( oh_ctx->codec_ctx->extradata, extra_data, extra_size_alloc);
        oh_ctx->codec_ctx->extradata_size = extra_size_alloc;
    }
}

#if OHCONFIG_AVCBASE
void oh_extradata_cpy_lhvc(OHHandle openHevcHandle, unsigned char *extra_data_linf,
                           unsigned char *extra_data_lsup, int extra_size_alloc_linf,
                           int extra_size_alloc_lsup)
{
    int i;
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_ctx;

    if (extra_data_linf && extra_size_alloc_linf) {
        oh_ctx = oh_ctx_list->ctx_list[0];
        if (oh_ctx->codec_ctx->extradata) av_freep(&oh_ctx->codec_ctx->extradata);
        oh_ctx->codec_ctx->extradata = (uint8_t*)av_mallocz(extra_size_alloc_linf);
        memcpy( oh_ctx->codec_ctx->extradata, extra_data_linf, extra_size_alloc_linf);
        oh_ctx->codec_ctx->extradata_size = extra_size_alloc_linf;
    }

    if (extra_data_lsup && extra_size_alloc_lsup) {
        for(i =1; i <= oh_ctx_list->target_active_layer; i++)  {
            oh_ctx = oh_ctx_list->ctx_list[i];
            if (oh_ctx->codec_ctx->extradata) av_freep(&oh_ctx->codec_ctx->extradata);
            oh_ctx->codec_ctx->extradata = (uint8_t*)av_mallocz(extra_size_alloc_lsup);
            memcpy( oh_ctx->codec_ctx->extradata, extra_data_lsup, extra_size_alloc_lsup);
            oh_ctx->codec_ctx->extradata_size = extra_size_alloc_lsup;
        }
    }
}
#endif

static int get_bitdepth(int format){

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
        return 10;
    case AV_PIX_FMT_YUV420P12 :
    case AV_PIX_FMT_YUV422P12 :
    case AV_PIX_FMT_YUV444P12 :
        return 12;
    case AV_PIX_FMT_YUV420P14 :
    case AV_PIX_FMT_YUV422P14 :
    case AV_PIX_FMT_YUV444P14 :
        return 12;
    default:
        return 8;
    }
}

static int get_oh_chroma_format(int format){
    switch (format) {
    case AV_PIX_FMT_YUV420P   :
    case AV_PIX_FMT_YUV420P9  :
    case AV_PIX_FMT_YUV420P10 :
    case AV_PIX_FMT_YUV420P12 :
        return OH_YUV420;
    case AV_PIX_FMT_YUV422P   :
    case AV_PIX_FMT_YUV422P9  :
    case AV_PIX_FMT_YUV422P10 :
    case AV_PIX_FMT_YUV422P12 :
        return OH_YUV422;
    case AV_PIX_FMT_YUV444P   :
    case AV_PIX_FMT_YUV444P9  :
    case AV_PIX_FMT_YUV444P10 :
    case AV_PIX_FMT_YUV444P12 :
        return OH_YUV444;
    default :
        return OH_YUV420;
    }
}

static void set_cropped_dim_from_avframe(OHFrameInfo *dst_frameinfo, AVFrame *frame){
    switch (frame->format) {
    case AV_PIX_FMT_YUV420P   :
        dst_frameinfo->chromat_format = OH_YUV420;
        dst_frameinfo->linesize_y     = frame->width;
        dst_frameinfo->linesize_cb    = frame->width >> 1;
        dst_frameinfo->linesize_cr    = frame->width >> 1;
        break;
    case AV_PIX_FMT_YUV420P9  :
    case AV_PIX_FMT_YUV420P10 :
    case AV_PIX_FMT_YUV420P12 :
        dst_frameinfo->chromat_format = OH_YUV420;
        dst_frameinfo->linesize_y     = frame->width << 1;
        dst_frameinfo->linesize_cb    = frame->width;
        dst_frameinfo->linesize_cr    = frame->width;
        break;
    case AV_PIX_FMT_YUV422P   :
        dst_frameinfo->chromat_format = OH_YUV422;
        dst_frameinfo->linesize_y     = frame->width;
        dst_frameinfo->linesize_cb    = frame->width >> 1;
        dst_frameinfo->linesize_cr    = frame->width >> 1;
        break;
    case AV_PIX_FMT_YUV422P9  :
    case AV_PIX_FMT_YUV422P10 :
    case AV_PIX_FMT_YUV422P12 :
        dst_frameinfo->chromat_format = OH_YUV422;
        dst_frameinfo->linesize_y     = frame->width << 1;
        dst_frameinfo->linesize_cb    = frame->width;
        dst_frameinfo->linesize_cr    = frame->width;
        break;
    case AV_PIX_FMT_YUV444P   :
        dst_frameinfo->chromat_format = OH_YUV444;
        dst_frameinfo->linesize_y     = frame->width;
        dst_frameinfo->linesize_cb    = frame->width;
        dst_frameinfo->linesize_cr    = frame->width;
        break;
    case AV_PIX_FMT_YUV444P9  :
    case AV_PIX_FMT_YUV444P10 :
    case AV_PIX_FMT_YUV444P12 :
        dst_frameinfo->chromat_format = OH_YUV444;
        dst_frameinfo->linesize_y     = frame->width << 1;
        dst_frameinfo->linesize_cb    = frame->width << 1;
        dst_frameinfo->linesize_cr    = frame->width << 1;
        break;
    default :
        dst_frameinfo->chromat_format = OH_YUV420;
        dst_frameinfo->linesize_y     = frame->width;
        dst_frameinfo->linesize_cb    = frame->width >> 1;
        dst_frameinfo->linesize_cr    = frame->width >> 1;
        break;
    }
    dst_frameinfo->width                  = frame->width;
    dst_frameinfo->height                 = frame->height;


    dst_frameinfo->bitdepth  =  get_bitdepth(frame->format);

    dst_frameinfo->width                   = frame->width;
    dst_frameinfo->height                  = frame->height;

    dst_frameinfo->sample_aspect_ratio.num = frame->sample_aspect_ratio.num;
    dst_frameinfo->sample_aspect_ratio.den = frame->sample_aspect_ratio.den;

    dst_frameinfo->display_picture_number  = frame->display_picture_number;
    dst_frameinfo->flag                    = (frame->top_field_first << 2) |
            frame->interlaced_frame; //progressive, interlaced, interlaced bottom field first, interlaced top field first.
    dst_frameinfo->pts              = frame->pts;
}

static void set_dim_from_avframe(OHFrameInfo *dst_frameinfo, AVFrame *frame){
    dst_frameinfo->linesize_y    = frame->linesize[0];
    dst_frameinfo->linesize_cb   = frame->linesize[1];
    dst_frameinfo->linesize_cr   = frame->linesize[2];

    dst_frameinfo->chromat_format = get_oh_chroma_format(frame->format);
    dst_frameinfo->bitdepth       = get_bitdepth(frame->format);

    dst_frameinfo->width                  = frame->width;
    dst_frameinfo->height                 = frame->height;

    dst_frameinfo->sample_aspect_ratio.num = frame->sample_aspect_ratio.num;
    dst_frameinfo->sample_aspect_ratio.den = frame->sample_aspect_ratio.den;

    dst_frameinfo->display_picture_number  = frame->display_picture_number;
    dst_frameinfo->flag                    = (frame->top_field_first << 2) |
            frame->interlaced_frame; //progressive, interlaced, interlaced bottom field first, interlaced top field first.
    dst_frameinfo->pts = frame->pts;
}


static inline int get_max_ouput_layer_id(int got_picture_mask){
    if (got_picture_mask){
        int max_layer_id = 1;
        while(got_picture_mask >>= 1)
            max_layer_id <<= 1;
        return max_layer_id - 1;
    } else {
        return 0;
    }
}

void oh_frameinfo_update(OHHandle openHevcHandle, OHFrameInfo *oh_frameinfo)
{
    OHContext     *oh_ctx_list = (OHContext *) openHevcHandle;
    int layer_id = get_max_ouput_layer_id (oh_ctx_list->got_picture_mask);
    OHDecoderCtx  *oh_ctx      = oh_ctx_list->ctx_list[layer_id];
    AVFrame       *picture     = oh_ctx->picture;

    set_dim_from_avframe(oh_frameinfo, picture);

    oh_frameinfo->framerate.num           = oh_ctx->codec_ctx->time_base.den;
    oh_frameinfo->framerate.den           = oh_ctx->codec_ctx->time_base.num;
}

int oh_cropped_frameinfo_from_layer(OHHandle openHevcHandle, OHFrameInfo *oh_frameinfo, int layer_id)
{
    OHContext *oh_ctx = (OHContext *) openHevcHandle;
    OHDecoderCtx  *oh_decoder_ctx  = NULL;

    int got_picture = (1 << layer_id) & oh_ctx->got_picture_mask;

    if(!got_picture){
        av_log(NULL, AV_LOG_DEBUG, "No picture found in layer %d for cropped frame parameters output\n", layer_id );
        return 0;
    } else {
        AVFrame    *oh_frame;
        oh_decoder_ctx   = oh_ctx->ctx_list[layer_id];
        oh_frame         = oh_decoder_ctx->picture;

        set_cropped_dim_from_avframe(oh_frameinfo , oh_frame);

        oh_frameinfo->framerate.num           = oh_decoder_ctx->codec_ctx->time_base.den;
        oh_frameinfo->framerate.den           = oh_decoder_ctx->codec_ctx->time_base.num;
        return 1;
    }
}

void oh_cropped_frameinfo(OHHandle openHevcHandle, OHFrameInfo *oh_frameinfo)
{
    OHContext *oh_ctx = (OHContext *) openHevcHandle;
    int layer_id = get_max_ouput_layer_id (oh_ctx->got_picture_mask);
    OHDecoderCtx  *oh_decoder_ctx  = oh_ctx->ctx_list[layer_id];
    AVFrame    *picture = oh_decoder_ctx->picture;

    set_cropped_dim_from_avframe(oh_frameinfo , picture);

    oh_frameinfo->framerate.num  = oh_decoder_ctx->codec_ctx->time_base.den;
    oh_frameinfo->framerate.den  = oh_decoder_ctx->codec_ctx->time_base.num;
}

int oh_output_update(OHHandle openHevcHandle, int got_picture, OHFrame *openHevcFrame)
{
    OHContext *oh_ctx = (OHContext *) openHevcHandle;

    int layer_id = get_max_ouput_layer_id (oh_ctx->got_picture_mask);
    if((1 << layer_id) & oh_ctx->got_picture_mask){
    OHDecoderCtx  *oh_decoder_ctx  = oh_ctx->ctx_list[layer_id];
        AVFrame    *picture = oh_decoder_ctx->picture;

        set_dim_from_avframe(&openHevcFrame->frame_par,picture);

        openHevcFrame->data_y_p   = (void *) oh_decoder_ctx->picture->data[0];
        openHevcFrame->data_cb_p  = (void *) oh_decoder_ctx->picture->data[1];
        openHevcFrame->data_cr_p  = (void *) oh_decoder_ctx->picture->data[2];

        return 1;
    }
    return 0;
}

int oh_output_update_from_layer(OHHandle openHevcHandle, OHFrame *oh_frame, int layer_id)
{
    OHContext *oh_ctx = (OHContext *) openHevcHandle;

    int got_picture = (1 << layer_id) & oh_ctx->got_picture_mask;

    if(!got_picture){
        av_log(NULL, AV_LOG_DEBUG, "No picture found in layer %d for cropped copy output\n", layer_id );
        return 0;
    }

    //FIXME we could alloc a frame here and delete it when openHEVC is closed
    if(!oh_frame || !oh_frame->data_y_p){
        av_log(NULL,AV_LOG_ERROR, "Getting a cropped output from openHEVC decoder require a preallocated OHframe_cpy\n");
        return -1;
    } else {

        OHDecoderCtx  *oh_decoder_ctx  = oh_ctx->ctx_list[layer_id];
        AVFrame    *picture = oh_decoder_ctx->picture;

        set_dim_from_avframe(&oh_frame->frame_par,picture);

        oh_frame->data_y_p   = (void *) oh_decoder_ctx->picture->data[0];
        oh_frame->data_cb_p  = (void *) oh_decoder_ctx->picture->data[1];
        oh_frame->data_cr_p  = (void *) oh_decoder_ctx->picture->data[2];
    }
    return 1;
}

int oh_output_cropped_cpy_from_layer(OHHandle openHevcHandle, OHFrame_cpy *oh_frame, int layer_id)
{
    OHContext     *oh_ctx          = (OHContext *) openHevcHandle;
    OHDecoderCtx  *oh_decoder_ctx  = NULL;

    int got_picture = (1 << layer_id) & oh_ctx->got_picture_mask;

    if(!got_picture){
        av_log(NULL, AV_LOG_DEBUG, "No picture found in layer %d for cropped copy output\n", layer_id );
        return 0;
    }

    //FIXME we could alloc a frame here and delete it when openHEVC is closed
    if(!oh_frame || !oh_frame->data_y){
        av_log(NULL,AV_LOG_ERROR, "Getting a cropped output from openHEVC decoder require a preallocated OHframe_cpy\n");
        return -1;
    }

    oh_decoder_ctx = oh_ctx->ctx_list[layer_id];

    if(!oh_decoder_ctx->picture){
        av_log(NULL,AV_LOG_ERROR, "Getting a cropped output from openHEVC decoder require a preallocated OHframe_cpy\n");
        return -1;
    } else {
        OHFrameInfo src_frame_info;
        OHFrameInfo dst_frame_info;

        AVFrame *frame = oh_decoder_ctx->picture;

        unsigned char *Y = (unsigned char *) oh_frame->data_y;
        unsigned char *U = (unsigned char *) oh_frame->data_cb;
        unsigned char *V = (unsigned char *) oh_frame->data_cr;

        set_cropped_dim_from_avframe(&oh_frame->frame_par, frame);
        set_dim_from_avframe(&src_frame_info, frame);
        set_cropped_dim_from_avframe(&dst_frame_info, frame);

        dst_frame_info.framerate.num  = oh_decoder_ctx->codec_ctx->time_base.den;
        dst_frame_info.framerate.den  = oh_decoder_ctx->codec_ctx->time_base.num;

        oh_frame->frame_par.framerate.num  = oh_decoder_ctx->codec_ctx->time_base.den;
        oh_frame->frame_par.framerate.den  = oh_decoder_ctx->codec_ctx->time_base.num;

        //TODO memcmp without time info
/*        if(memcmp(&oh_frame->frame_par, &dst_frame_info, sizeof(OHFrameInfo))){
            av_log(NULL,AV_LOG_ERROR, "Informations on cropped dimensions does not match those of current OHframe_cpy\n");
            return -1;
        } else*/ {
            int y;
            int oh_chroma_format = src_frame_info.chromat_format == OH_YUV420 ? 1 : 0;

            int src_stride   = src_frame_info.linesize_y;
            int src_stride_c = src_frame_info.linesize_cb;

            int height = src_frame_info.height;

            int dst_stride   = dst_frame_info.linesize_y;
            int dst_stride_c = dst_frame_info.linesize_cb;

            int y_offset = 0, y_offset2 = 0;

            for (y = 0; y < height; y++) {
                memcpy(&Y[y_offset2], &oh_decoder_ctx->picture->data[0][y_offset], dst_stride);
                y_offset  += src_stride;
                y_offset2 += dst_stride;
            }

            y_offset = y_offset2 = 0;

            for (y = 0; y < (height >> oh_chroma_format); y++) {
                memcpy(&U[y_offset2], &oh_decoder_ctx->picture->data[1][y_offset], dst_stride_c);
                memcpy(&V[y_offset2], &oh_decoder_ctx->picture->data[2][y_offset], dst_stride_c);
                y_offset  += src_stride_c;
                y_offset2 += dst_stride_c;
            }
            return 1;
        }
    }
}



int oh_output_cropped_cpy(OHHandle openHevcHandle, OHFrame_cpy *oh_frame){
    OHContext     *oh_ctx          = (OHContext *) openHevcHandle;
    //OHDecoderCtx  *oh_decoder_ctx  = NULL;
    int ret;

    int layer_id = get_max_ouput_layer_id (oh_ctx->got_picture_mask);
    int got_picture = (1 << layer_id) & oh_ctx->got_picture_mask;

    if(!got_picture){
        av_log(NULL, AV_LOG_DEBUG, "No picture found in layer %d for cropped copy output\n", layer_id );
        return 0;
    }

    //FIXME we could alloc a frame here and delete it when openHEVC is closed
    if(!oh_frame || !oh_frame->data_y){
        av_log(NULL,AV_LOG_ERROR, "Getting a cropped output from openHEVC decoder require a preallocated OHframe_cpy\n");
        return -1;
    }
    ret = oh_output_cropped_cpy_from_layer(openHevcHandle, oh_frame, layer_id);
    return ret;
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
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    if (val >= 0 && val < oh_ctx_list->nb_decoders){
        pthread_mutex_lock(&oh_ctx_list->layer_switch);
        oh_ctx_list->target_active_layer = val;
        pthread_mutex_unlock(&oh_ctx_list->layer_switch);
    }
    else {
        av_log(NULL, AV_LOG_ERROR, "The requested layer %d can not be decoded (it exceeds the number of allocated decoders %d ) \n", val, oh_ctx_list->nb_decoders);
        oh_ctx_list->target_active_layer = oh_ctx_list->nb_decoders-1;
    }
}

//FIXME: The layer need also to be active in order to be decoded
void oh_select_view_layer(OHHandle openHevcHandle, int val)
{
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;

    if (val >= 0 && val <= oh_ctx_list->target_active_layer)
        oh_ctx_list->target_display_layer = val;

    else {
        av_log(NULL, AV_LOG_ERROR,
                "The requested layer %d can not be viewed (it exceeds the number of allocated decoders %d ) \n", val, oh_ctx_list->nb_decoders);
        oh_ctx_list->target_display_layer = oh_ctx_list->nb_decoders-1;
    }
}


void oh_enable_sei_checksum(OHHandle openHevcHandle, int val)
{
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx  *oh_ctx;

    int i;

    for (i = 0; i < oh_ctx_list->nb_decoders; i++) {
        oh_ctx = oh_ctx_list->ctx_list[i];

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "decode-checksum", val, 0);
    }
}

void oh_select_temporal_layer(OHHandle openHevcHandle, int val)
{
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_ctx;

    int i;

    for (i = 0; i < oh_ctx_list->nb_decoders; i++) {
        oh_ctx = oh_ctx_list->ctx_list[i];

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "temporal-layer-id", val, 0);
    }
}

void oh_disable_cropping(OHHandle openHevcHandle, int val)
{
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_ctx;
    int i;

    for (i = 0; i < oh_ctx_list->nb_decoders; i++) {
        oh_ctx = oh_ctx_list->ctx_list[i];

        av_opt_set_int(oh_ctx->codec_ctx->priv_data, "no-cropping", val, 0);
    }
}

void oh_close(OHHandle openHevcHandle)
{
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_ctx;
    int i;

    av_log(NULL,AV_LOG_DEBUG,"Closing openHEVC\n");
    for (i = oh_ctx_list->nb_decoders - 1; i >= 0 ; i--){
        oh_ctx = oh_ctx_list->ctx_list[i];
        avcodec_flush_buffers(oh_ctx->codec_ctx);

        avcodec_close(oh_ctx->codec_ctx);

        av_parser_close(oh_ctx->parser_ctx);

        av_freep(&oh_ctx->codec_ctx);
        av_freep(&oh_ctx->picture);
        av_freep(&oh_ctx);
    }
    pthread_mutex_destroy(&oh_ctx_list->layer_switch);
    av_freep(&oh_ctx_list->ctx_list);
    av_freep(&oh_ctx_list);
    av_log(NULL,AV_LOG_DEBUG,"Close openHEVC decoder\n");


}

void oh_flush(OHHandle openHevcHandle)
{
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_ctx  = oh_ctx_list->ctx_list[oh_ctx_list->target_active_layer];
    oh_ctx->codec->flush(oh_ctx->codec_ctx);
    avcodec_flush_buffers(oh_ctx->codec_ctx);
}

void oh_flush_shvc(OHHandle openHevcHandle, int decoderId)
{
    OHContext *oh_ctx_list = (OHContext *) openHevcHandle;
    OHDecoderCtx     *oh_ctx      = oh_ctx_list->ctx_list[decoderId];
    if (oh_ctx){
    oh_ctx->codec->flush(oh_ctx->codec_ctx);
    avcodec_flush_buffers(oh_ctx->codec_ctx);
    }
}

#if OHCONFIG_ENCRYPTION
void oh_set_crypto_mode(OHHandle oh_hdl, int val)
{
    OHContext *oh_ctx = (OHContext *) oh_hdl;
    OHDecoderCtx     *oh_decoder_ctx;
    int i;

    for (i = 0; i < oh_ctx->nb_decoders; i++) {
        oh_decoder_ctx = oh_ctx->ctx_list[i];
        av_opt_set_int(oh_decoder_ctx->codec_ctx->priv_data, "crypto-param", val, 0);
    }

}

void oh_set_crypto_key(OHHandle oh_hdl, uint8_t *val)
{
    OHContext *oh_ctx = (OHContext *) oh_hdl;
    OHDecoderCtx     *oh_decoder_ctx;
    int i;

    for (i = 0; i < oh_ctx->nb_decoders; i++) {
        oh_decoder_ctx = oh_ctx->ctx_list[i];
        av_opt_set_bin(oh_decoder_ctx->codec_ctx->priv_data, "crypto-key", val, 16*sizeof(uint8_t), 0);
    }

}
#endif

const unsigned oh_version(OHHandle openHevcHandle)
{
    return LIBOPENHEVC_VERSION_INT;
}

unsigned openhevc_version(void){
    return LIBOPENHEVC_VERSION_INT;
}

const char *openhevc_configuration(void){
    return FFMPEG_CONFIGURATION;
}

