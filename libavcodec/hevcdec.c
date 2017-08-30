/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2012 - 2013 Mickael Raulet
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 * Copyright (C) 2012 - 2013 Wassim Hamidouche
 * Copyright (C) 2012 - 2013 Seppo Tomperi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/atomic.h"
#include "libavutil/attributes.h"
#include "libavutil/common.h"
#include "libavutil/display.h"
#include "libavutil/internal.h"
#include "libavutil/mastering_display_metadata.h"
#include "libavutil/md5.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/stereo3d.h"

#include "libkvz/bitstream.h"
#include "libkvz/context.h"

#include "bswapdsp.h"
#include "bytestream.h"
#include "cabac_functions.h"
#include "golomb.h"
#include "hevc.h"
#include "hevc_data.h"
#include "hevc_parse.h"
#include "hevcdec.h"
#include "profiles.h"

#include "h264.h"

const uint8_t ff_hevc_pel_weight[65] = { [2] = 0, [4] = 1, [6] = 2, [8] = 3, [12] = 4, [16] = 5, [24] = 6, [32] = 7, [48] = 8, [64] = 9 };

#define POC_DISPLAY_MD5

/*static void calc_md5(uint8_t *md5, uint8_t* src, int stride, int width, int height, int pixel_shift);
static int compare_md5(uint8_t *md5_in1, uint8_t *md5_in2);
static void display_md5(int poc, uint8_t md5[3][16], int chroma_idc);
static void printf_ref_pic_list(HEVCContext *s);*/



/**
 * NOTE: Each function hls_foo correspond to the function foo in the
 * specification (HLS stands for High Level Syntax).
 */

/**
 * Section 5.7
 */

/* free everything allocated  by pic_arrays_init() */
static void pic_arrays_free(HEVCContext *s)
{
    av_freep(&s->sao);
    av_freep(&s->deblock);

    av_freep(&s->skip_flag);
    av_freep(&s->tab_ct_depth);

    av_freep(&s->tab_ipm);

#if HEVC_ENCRYPTION
    av_freep(&s-> tab_ipm_encry);
#endif
    av_freep(&s->cbf_luma);
    av_freep(&s->is_pcm);

    av_freep(&s->qp_y_tab);
    av_freep(&s->tab_slice_address);
    av_freep(&s->filter_slice_edges);

    av_freep(&s->horizontal_bs);
    av_freep(&s->vertical_bs);

    av_freep(&s->sh.entry_point_offset);
    av_freep(&s->sh.size);
    av_freep(&s->sh.offset);

    av_buffer_pool_uninit(&s->tab_mvf_pool);
    av_buffer_pool_uninit(&s->rpl_tab_pool);

#ifdef SVC_EXTENSION
#if ACTIVE_BOTH_FRAME_AND_PU
    av_freep(&s->buffer_frame[0]);
    av_freep(&s->buffer_frame[1]);
    av_freep(&s->buffer_frame[2]);
    av_freep(&s->is_upsampled);
#else
#if !ACTIVE_PU_UPSAMPLING
    av_freep(&s->buffer_frame[0]);
    av_freep(&s->buffer_frame[1]);
    av_freep(&s->buffer_frame[2]);
#else
    av_freep(&s->is_upsampled);
#endif
#endif    
#endif

#if PARALLEL_SLICE
    av_freep(&s->decoded_rows);
#endif
}

/* allocate arrays that depend on frame dimensions */
static int pic_arrays_init(HEVCContext *s, const HEVCSPS *sps)
{
    int log2_min_cb_size = sps->log2_min_cb_size;
    int width            = sps->width;
    int height           = sps->height;
#if !ACTIVE_PU_UPSAMPLING || ACTIVE_BOTH_FRAME_AND_PU
    int pic_size         = width * height;
#endif
    int pic_size_in_ctb  = ((width  >> log2_min_cb_size) + 1) *
                           ((height >> log2_min_cb_size) + 1);
    int ctb_count        = sps->ctb_width * sps->ctb_height;
    int min_pu_size      = sps->min_pu_width * sps->min_pu_height;

    s->bs_width  = (width  >> 2) + 1;
    s->bs_height = (height >> 2) + 1;

    s->sao           = av_mallocz_array(ctb_count, sizeof(*s->sao));
    s->deblock       = av_mallocz_array(ctb_count, sizeof(*s->deblock));
    if (!s->sao || !s->deblock)
        goto fail;

    s->skip_flag    = av_malloc_array(sps->min_cb_height, sps->min_cb_width);
    s->tab_ct_depth = av_malloc_array(sps->min_cb_height, sps->min_cb_width);

#if PARALLEL_SLICE
    s->decoded_rows = av_malloc(sps->ctb_height);
#endif
    if (!s->skip_flag || !s->tab_ct_depth)
        goto fail;

    s->cbf_luma = av_malloc_array(sps->min_tb_width, sps->min_tb_height);
    s->tab_ipm  = av_mallocz(min_pu_size);

#if HEVC_ENCRYPTION
    s->tab_ipm_encry = av_mallocz(min_pu_size);
    if (!s->tab_ipm_encry)
      goto fail;
#endif

    s->is_pcm   = av_malloc_array(sps->min_pu_width + 1, sps->min_pu_height + 1);
    if (!s->tab_ipm || !s->cbf_luma || !s->is_pcm)
        goto fail;

    s->filter_slice_edges = av_mallocz(ctb_count);
    s->tab_slice_address  = av_malloc_array(pic_size_in_ctb,
                                      sizeof(*s->tab_slice_address));
    s->qp_y_tab           = av_malloc_array(pic_size_in_ctb,
                                      sizeof(*s->qp_y_tab));
    if (!s->qp_y_tab || !s->filter_slice_edges || !s->tab_slice_address)
        goto fail;

    s->horizontal_bs = av_mallocz_array(s->bs_width, s->bs_height);
    s->vertical_bs   = av_mallocz_array(s->bs_width, s->bs_height);
    if (!s->horizontal_bs || !s->vertical_bs)
        goto fail;

    s->tab_mvf_pool = av_buffer_pool_init(min_pu_size * sizeof(MvField),
                                          av_buffer_allocz);
    s->rpl_tab_pool = av_buffer_pool_init(ctb_count * sizeof(RefPicListTab),
                                          av_buffer_allocz);
    if (!s->tab_mvf_pool || !s->rpl_tab_pool)
        goto fail;
#ifdef SVC_EXTENSION
    if(s->decoder_id)    {
#if ACTIVE_BOTH_FRAME_AND_PU
        s->buffer_frame[0] = av_malloc(pic_size*sizeof(short));
        s->buffer_frame[1] = av_malloc((pic_size>>2)*sizeof(short));
        s->buffer_frame[2] = av_malloc((pic_size>>2)*sizeof(short));
        s->is_upsampled = av_malloc(sps->ctb_width * sps->ctb_height);
#else
#if !ACTIVE_PU_UPSAMPLING
        s->buffer_frame[0] = av_malloc(pic_size*sizeof(short));
        s->buffer_frame[1] = av_malloc((pic_size>>2)*sizeof(short));
        s->buffer_frame[2] = av_malloc((pic_size>>2)*sizeof(short));
#else
        s->is_upsampled = av_malloc(sps->ctb_width * sps->ctb_height);
#endif
#endif
    }
#endif

    return 0;

fail:
    pic_arrays_free(s);
    return AVERROR(ENOMEM);
}

static int pred_weight_table(HEVCContext *s, GetBitContext *gb)
{
    int i = 0;
    int j = 0;
    uint8_t luma_weight_l0_flag[16];
    uint8_t chroma_weight_l0_flag[16];
    uint8_t luma_weight_l1_flag[16];
    uint8_t chroma_weight_l1_flag[16];
    int luma_log2_weight_denom;
#if HEVC_CIPHERING 
    HEVCLocalContext *lc = s->HEVClc;
    cabac_data_t *const cabac = &lc->ccc;
    bitstream_t *stream = cabac->stream;
#endif

    luma_log2_weight_denom = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
    kvz_bitstream_put_ue(stream,luma_log2_weight_denom);
#endif

    if (luma_log2_weight_denom < 0 || luma_log2_weight_denom > 7)
        av_log(s->avctx, AV_LOG_ERROR, "luma_log2_weight_denom %d is invalid\n", luma_log2_weight_denom);
    s->sh.luma_log2_weight_denom = av_clip_uintp2(luma_log2_weight_denom, 7);
    if (s->ps.sps->chroma_format_idc != 0) {
        int delta = get_se_golomb(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put_se(stream, delta);
#endif
        s->sh.chroma_log2_weight_denom = av_clip_uintp2(s->sh.luma_log2_weight_denom + delta, 7);
    }

    for (i = 0; i < s->sh.nb_refs[L0]; i++) {
        luma_weight_l0_flag[i] = get_bits1(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put(stream, luma_weight_l0_flag[i], 1);
#endif
        if (!luma_weight_l0_flag[i]) {
            s->sh.luma_weight_l0[i] = 1 << s->sh.luma_log2_weight_denom;
            s->sh.luma_offset_l0[i] = 0;
        }
    }
    if (s->ps.sps->chroma_format_idc != 0) {
        for (i = 0; i < s->sh.nb_refs[L0]; i++){
            chroma_weight_l0_flag[i] = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, chroma_weight_l0_flag[i], 1);
#endif
        }
    } else {
        for (i = 0; i < s->sh.nb_refs[L0]; i++)
            chroma_weight_l0_flag[i] = 0;
    }
    for (i = 0; i < s->sh.nb_refs[L0]; i++) {
        if (luma_weight_l0_flag[i]) {
            int delta_luma_weight_l0 = get_se_golomb(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put_se(stream, delta_luma_weight_l0);
#endif
            s->sh.luma_weight_l0[i] = (1 << s->sh.luma_log2_weight_denom) + delta_luma_weight_l0;
            s->sh.luma_offset_l0[i] = get_se_golomb(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put_se(stream, s->sh.luma_offset_l0[i]);
#endif
        }
        if (chroma_weight_l0_flag[i]) {
            for (j = 0; j < 2; j++) {
                int delta_chroma_weight_l0 = get_se_golomb(gb);
                int delta_chroma_offset_l0 = get_se_golomb(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put_se(stream, delta_chroma_weight_l0);
                kvz_bitstream_put_se(stream, delta_chroma_offset_l0);
#endif
                if (   (int8_t)delta_chroma_weight_l0 != delta_chroma_weight_l0
                    || delta_chroma_offset_l0 < -(1<<17) || delta_chroma_offset_l0 > (1<<17)) {
                    return AVERROR_INVALIDDATA;
                }

                s->sh.chroma_weight_l0[i][j] = (1 << s->sh.chroma_log2_weight_denom) + delta_chroma_weight_l0;
                s->sh.chroma_offset_l0[i][j] = av_clip((delta_chroma_offset_l0 - ((128 * s->sh.chroma_weight_l0[i][j])
                                                                                    >> s->sh.chroma_log2_weight_denom) + 128), -128, 127);
            }
        } else {
            s->sh.chroma_weight_l0[i][0] = 1 << s->sh.chroma_log2_weight_denom;
            s->sh.chroma_offset_l0[i][0] = 0;
            s->sh.chroma_weight_l0[i][1] = 1 << s->sh.chroma_log2_weight_denom;
            s->sh.chroma_offset_l0[i][1] = 0;
        }
    }
    if (s->sh.slice_type == HEVC_SLICE_B) {
        for (i = 0; i < s->sh.nb_refs[L1]; i++) {
            luma_weight_l1_flag[i] = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, luma_weight_l1_flag[i], 1);
#endif
            if (!luma_weight_l1_flag[i]) {
                s->sh.luma_weight_l1[i] = 1 << s->sh.luma_log2_weight_denom;
                s->sh.luma_offset_l1[i] = 0;
            }
        }
        if (s->ps.sps->chroma_format_idc != 0) {
            for (i = 0; i < s->sh.nb_refs[L1]; i++){
                chroma_weight_l1_flag[i] = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, chroma_weight_l1_flag[i], 1);
#endif
            }
        } else {
            for (i = 0; i < s->sh.nb_refs[L1]; i++)
                chroma_weight_l1_flag[i] = 0;
        }
        for (i = 0; i < s->sh.nb_refs[L1]; i++) {
            if (luma_weight_l1_flag[i]) {
                int delta_luma_weight_l1 = get_se_golomb(gb);
                s->sh.luma_weight_l1[i] = (1 << s->sh.luma_log2_weight_denom) + delta_luma_weight_l1;
                s->sh.luma_offset_l1[i] = get_se_golomb(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put_se(stream, delta_luma_weight_l1);
                kvz_bitstream_put_se(stream, s->sh.luma_offset_l1[i]);
#endif
            }
            if (chroma_weight_l1_flag[i]) {
                for (j = 0; j < 2; j++) {
                    int delta_chroma_weight_l1 = get_se_golomb(gb);
                    int delta_chroma_offset_l1 = get_se_golomb(gb);
#if HEVC_CIPHERING 
                    kvz_bitstream_put_se(stream, delta_chroma_weight_l1);
                    kvz_bitstream_put_se(stream, delta_chroma_offset_l1);
#endif
                    if (   (int8_t)delta_chroma_weight_l1 != delta_chroma_weight_l1
                        || delta_chroma_offset_l1 < -(1<<17) || delta_chroma_offset_l1 > (1<<17)) {
                        return AVERROR_INVALIDDATA;
                    }

                    s->sh.chroma_weight_l1[i][j] = (1 << s->sh.chroma_log2_weight_denom) + delta_chroma_weight_l1;
                    s->sh.chroma_offset_l1[i][j] = av_clip((delta_chroma_offset_l1 - ((128 * s->sh.chroma_weight_l1[i][j])
                                                                                        >> s->sh.chroma_log2_weight_denom) + 128), -128, 127);
                }
            } else {
                s->sh.chroma_weight_l1[i][0] = 1 << s->sh.chroma_log2_weight_denom;
                s->sh.chroma_offset_l1[i][0] = 0;
                s->sh.chroma_weight_l1[i][1] = 1 << s->sh.chroma_log2_weight_denom;
                s->sh.chroma_offset_l1[i][1] = 0;
            }
        }
    }
    return 0;
}

static int decode_lt_rps(HEVCContext *s, LongTermRPS *rps, GetBitContext *gb)
{
    const HEVCSPS *sps = s->ps.sps;
    int max_poc_lsb    = 1 << sps->log2_max_poc_lsb;
    int prev_delta_msb = 0;
    unsigned int nb_sps = 0, nb_sh;
    int i;
#if HEVC_CIPHERING 
    HEVCLocalContext *lc = s->HEVClc;
    cabac_data_t *const cabac = &lc->ccc;
    bitstream_t *stream = cabac->stream;
#endif

    rps->nb_refs = 0;
    if (!sps->long_term_ref_pics_present_flag)
        return 0;

    if (sps->num_long_term_ref_pics_sps > 0) {
        nb_sps = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put_ue(stream,nb_sps);
#endif
    }
    nb_sh = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
    kvz_bitstream_put_ue(stream, nb_sh);
#endif

    if (nb_sh + (uint64_t)nb_sps > FF_ARRAY_ELEMS(rps->poc))
        return AVERROR_INVALIDDATA;

    rps->nb_refs = nb_sh + nb_sps;

    for (i = 0; i < rps->nb_refs; i++) {
        uint8_t delta_poc_msb_present;

        if (i < nb_sps) {
            uint8_t lt_idx_sps = 0;

            if (sps->num_long_term_ref_pics_sps > 1) {
                lt_idx_sps = get_bits(gb, av_ceil_log2(sps->num_long_term_ref_pics_sps));
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, lt_idx_sps, av_ceil_log2(sps->num_long_term_ref_pics_sps));
#endif
            }

            rps->poc[i]  = sps->lt_ref_pic_poc_lsb_sps[lt_idx_sps];
            rps->used[i] = sps->used_by_curr_pic_lt_sps_flag[lt_idx_sps];
        } else {
            rps->poc[i]  = get_bits(gb, sps->log2_max_poc_lsb);
            rps->used[i] = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, rps->poc[i], sps->log2_max_poc_lsb);
            kvz_bitstream_put(stream, rps->used[i], 1);
#endif
        }

        delta_poc_msb_present = get_bits1(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put(stream, delta_poc_msb_present, 1);
#endif
        if (delta_poc_msb_present) {
            int delta = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put_ue(stream, delta);
#endif

            if (i && i != nb_sps)
                delta += prev_delta_msb;

            rps->poc[i] += s->poc - delta * max_poc_lsb - s->sh.slice_pic_order_cnt_lsb;
            prev_delta_msb = delta;
        }
    }

    return 0;
}

static void export_stream_params(AVCodecContext *avctx, const HEVCParamSets *ps,
                                 const HEVCSPS *sps)
{
    const HEVCVPS *vps = (const HEVCVPS*)ps->vps_list[sps->vps_id]->data;
    const HEVCWindow *ow = &sps->output_window;
    unsigned int num = 0, den = 0;

    avctx->pix_fmt             = sps->pix_fmt;
    avctx->coded_width         = sps->width;
    avctx->coded_height        = sps->height;
    avctx->width               = sps->width  - ow->left_offset - ow->right_offset;
    avctx->height              = sps->height - ow->top_offset  - ow->bottom_offset;
    avctx->has_b_frames        = sps->temporal_layer[sps->sps_max_sub_layers - 1].num_reorder_pics;
    avctx->profile             = sps->ptl.general_ptl.profile_idc;
    avctx->level               = sps->ptl.general_ptl.level_idc;

    ff_set_sar(avctx, sps->vui.sar);

    if (sps->vui.video_signal_type_present_flag)
        avctx->color_range = sps->vui.video_full_range_flag ? AVCOL_RANGE_JPEG
                                                            : AVCOL_RANGE_MPEG;
    else
        avctx->color_range = AVCOL_RANGE_MPEG;

    if (sps->vui.colour_description_present_flag) {
        avctx->color_primaries = sps->vui.colour_primaries;
        avctx->color_trc       = sps->vui.transfer_characteristic;
        avctx->colorspace      = sps->vui.matrix_coeffs;
    } else {
        avctx->color_primaries = AVCOL_PRI_UNSPECIFIED;
        avctx->color_trc       = AVCOL_TRC_UNSPECIFIED;
        avctx->colorspace      = AVCOL_SPC_UNSPECIFIED;
    }

    if (vps->vps_timing_info_present_flag) {
        num = vps->vps_num_units_in_tick;
        den = vps->vps_time_scale;
    } else if (sps->vui.vui_timing_info_present_flag) {
        num = sps->vui.vui_num_units_in_tick;
        den = sps->vui.vui_time_scale;
    }

    if (num != 0 && den != 0)
        av_reduce(&avctx->framerate.den, &avctx->framerate.num,
                  num, den, 1 << 30);
}

#ifndef USE_SAO_SMALL_BUFFER
static int get_buffer_sao(HEVCContext *s, AVFrame *frame, const HEVCSPS *sps)
{
    int ret, i;

    // 16 byte alignment required for ARM assembly
    frame->width  = (s->avctx->width  + 32) & ~15;
    frame->height = s->avctx->height + 2;
    if ((ret = ff_get_buffer(s->avctx, frame, AV_GET_BUFFER_FLAG_REF)) < 0)
        return ret;
    for (i = 0; frame->data[i]; i++) {
        int offset = frame->linesize[i] + 16;
        frame->data[i] += offset;
    }
    frame->width  = s->avctx->width;
    frame->height = s->avctx->height;

    return 0;
}
#endif

static int set_sps(HEVCContext *s, const HEVCSPS *sps,
                   enum AVPixelFormat pix_fmt)
{
    int ret, i;

    pic_arrays_free(s);
    s->ps.sps = NULL;
    s->ps.vps = NULL;

    if (!sps)
        return 0;

    ret = pic_arrays_init(s, sps);
    if (ret < 0)
        goto fail;

    export_stream_params(s->avctx, &s->ps, sps);

    ff_hevc_pred_init(&s->hpc,     sps->bit_depth[CHANNEL_TYPE_LUMA]);
    ff_hevc_dsp_init (&s->hevcdsp, sps->bit_depth[CHANNEL_TYPE_LUMA]);
    ff_videodsp_init (&s->vdsp,    sps->bit_depth[CHANNEL_TYPE_LUMA]);

    if (sps->sao_enabled_flag && !s->avctx->hwaccel) {
#ifdef USE_SAO_SMALL_BUFFER
        {
            int ctb_size = 1 << sps->log2_ctb_size;
            int c_count = (sps->chroma_format_idc != 0) ? 3 : 1;
            int c_idx, i;

            for (i = 0; i < s->threads_number ; i++) {
                HEVCLocalContext    *lc = s->HEVClcList[i];
                lc->sao_pixel_buffer =
                    av_malloc(((FFMAX(ctb_size, 16) + 32) * (ctb_size + 2)) <<
                              sps->pixel_shift[CHANNEL_TYPE_LUMA]);
            }
            for(c_idx = 0; c_idx < c_count; c_idx++) {
                int w = sps->width >> sps->hshift[c_idx];
                int h = sps->height >> sps->vshift[c_idx];
                s->sao_pixel_buffer_h[c_idx] =
                av_malloc((w * 2 * sps->ctb_height) <<
                          sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA]);
                s->sao_pixel_buffer_v[c_idx] =
                av_malloc((h * 2 * sps->ctb_width) <<
                          sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA]);
            }
        }
#else
        av_frame_unref(s->tmp_frame);
        ret = get_buffer_sao(s, s->tmp_frame, sps);
        s->sao_frame = s->tmp_frame;
#endif
    }

    s->ps.sps = sps;
    s->ps.vps = (HEVCVPS*) s->ps.vps_list[s->ps.sps->vps_id]->data;

    return 0;

fail:
    pic_arrays_free(s);
    s->ps.sps = NULL;
    return ret;
}

static int getBitDepth(HEVCContext *s, enum ChannelType channel, int layerId)
{
  const HEVCVPS *vps = s->ps.vps;
  const HEVCSPS *sps;
  int retVal;

  if ( !layerId /*|| sps->v1_compatible*/) {
    if( !layerId && vps->vps_nonHEVCBaseLayerFlag)
      retVal = vps->vps_ext.rep_format[layerId].bit_depth_vps[channel];
    else if(s->ps.sps_list[layerId]){
      sps = (HEVCSPS*)s->ps.sps_list[layerId]->data;
      retVal = sps->bit_depth[channel];
    }
  }
  else if(s->ps.sps_list[layerId]) {
    sps = (HEVCSPS*)s->ps.sps_list[layerId]->data;
    retVal = sps->update_rep_format_flag ? sps->sps_rep_format_idx : vps->vps_ext.vps_rep_format_idx[vps->vps_ext.layer_id_in_vps[layerId]];
    retVal = vps->vps_ext.rep_format[retVal].bit_depth_vps[channel];
  }
  return retVal;
}

int set_el_parameter(HEVCContext *s) {
    int ret = 0, i;
    int phaseHorChroma = 0, phaseVerChroma=0;

    int heightEL, widthEL;
//    const int phaseXC = 0;
//    const int phaseYC = 1;
//    const int phaseAlignFlag = 0; // TO DO ((HEVCVPS*)s->ps.vps_list[s->ps.sps->ps.vps_id]->data)->phase_align_flag;
    const int phaseHorLuma = 0;
    const int phaseVerLuma = 0, refLayer = 0;
    HEVCWindow scaled_ref_layer_window;
    //HEVCVPS *vps = s->ps.vps;
    if(s->ps.vps){
      HEVCWindow base_layer_window = s->ps.pps->ref_window[((HEVCVPS*)s->ps.vps_list[s->ps.sps->vps_id]->data)->vps_ext.ref_layer_id[0][0]];
      s->BL_height = s->ps.vps->vps_ext.rep_format[s->decoder_id-1].pic_height_vps_in_luma_samples - base_layer_window.bottom_offset - base_layer_window.top_offset;
      s->BL_width  = s->ps.vps->vps_ext.rep_format[s->decoder_id-1].pic_width_vps_in_luma_samples  - base_layer_window.left_offset   - base_layer_window.right_offset;
    } else {
      av_log(s->avctx, AV_LOG_ERROR, "VPS Informations related to the inter layer reference frame are missing -- \n");
      ret = AVERROR(ENOMEM);
      goto fail;
    }

    if(!s->BL_height || !s->BL_width) {
        av_log(s->avctx, AV_LOG_ERROR, "Informations related to the inter layer reference frame are missing heightBL: %d widthBL: %d \n", s->BL_height,s->BL_width);
        ret = AVERROR(ENOMEM);
        goto fail;
     }

     scaled_ref_layer_window = s->ps.sps->scaled_ref_layer_window[s->ps.vps->vps_ext.ref_layer_id[s->nuh_layer_id][0]];
     heightEL = /*vps->Hevc_VPS_Ext.rep_format[s->decoder_id].pic_height_vps_in_luma_samples*/s->ps.sps->height - scaled_ref_layer_window.bottom_offset   - scaled_ref_layer_window.top_offset;
     widthEL  = /*vps->Hevc_VPS_Ext.rep_format[s->decoder_id].pic_width_vps_in_luma_samples*/ s->ps.sps->width  - scaled_ref_layer_window.left_offset     - scaled_ref_layer_window.right_offset;
     phaseVerChroma = (4 * heightEL + (s->BL_height >> 1)) / s->BL_height - 4;
#if !ACTIVE_PU_UPSAMPLING //fixme : was this intended to avc base layer??
    if(s->ps.pps->colour_mapping_enabled_flag) { // allocate frame with BL parameters
      av_frame_unref(s->Ref_color_mapped_frame);
      s->Ref_color_mapped_frame->width  = s->BL_width;
      s->Ref_color_mapped_frame->height = s->BL_height;
      ret = ff_get_buffer(s->avctx, s->Ref_color_mapped_frame, AV_GET_BUFFER_FLAG_REF);
      if(ret < 0)
        av_log(s->avctx, AV_LOG_ERROR, "Error in CGS allocation \n");
    }
#endif

    s->up_filter_inf.mv_scale_x = av_clip_c(((widthEL  << 8) + (s->BL_width  >> 1)) / s->BL_width,  -4096, 4095 );
    s->up_filter_inf.mv_scale_y = av_clip_c(((heightEL << 8) + (s->BL_height >> 1)) / s->BL_height, -4096, 4095 );

    s->up_filter_inf.scaleXLum = ((s->BL_width  << 16) + (widthEL  >> 1)) / widthEL;//s->sh.ScalingFactor[s->nuh_layer_id][0];
    s->up_filter_inf.scaleYLum = ((s->BL_height << 16) + (heightEL >> 1)) / heightEL;//s->sh.ScalingFactor[s->nuh_layer_id][1];

    s->up_filter_inf.addXLum   = (( phaseHorLuma * s->up_filter_inf.scaleXLum + 8 ) >> 4 ) - ( 1 << 11 );
    s->up_filter_inf.addYLum   = (( phaseVerLuma * s->up_filter_inf.scaleYLum + 8 ) >> 4 ) - ( 1 << 11 );

    s->up_filter_inf.addXCr   = ((phaseHorChroma * s->up_filter_inf.scaleXLum + 8) >> 4) - ( 1 << 11 );
    s->up_filter_inf.addYCr   = ((phaseVerChroma * s->up_filter_inf.scaleYLum + 8) >> 4) - ( 1 << 11 );

    s->up_filter_inf.scaleXCr = s->up_filter_inf.scaleXLum;
    s->up_filter_inf.scaleYCr = s->up_filter_inf.scaleYLum;
    for(i = 0; i <= s->nuh_layer_id; i++) {
            s->sh.Bit_Depth[i][CHANNEL_TYPE_LUMA  ] = getBitDepth(s, CHANNEL_TYPE_LUMA, i);
            s->sh.Bit_Depth[i][CHANNEL_TYPE_CHROMA] = getBitDepth(s, CHANNEL_TYPE_CHROMA, i);
    }
    if(s->nuh_layer_id){
        int bl_bit_depth = getBitDepth(s, CHANNEL_TYPE_LUMA, s->nuh_layer_id - 1);
        int bit_depth = getBitDepth(s, CHANNEL_TYPE_LUMA, s->nuh_layer_id);
        int have_CGS = s->ps.pps->colour_mapping_enabled_flag;
        if(bl_bit_depth == 8 && bit_depth > 8){
            ff_shvc_dsp_update(&s->hevcdsp, bit_depth, have_CGS);
            ff_videodsp_update(&s->vdsp, have_CGS);
        }
    }
    for(i = 0; i < MAX_NUM_CHANNEL_TYPE; i++) {
      s->up_filter_inf.shift[i]    = s->sh.Bit_Depth[s->nuh_layer_id][i] - s->sh.Bit_Depth[refLayer][i];
      s->up_filter_inf.shift_up[i] = s->sh.Bit_Depth[refLayer][i] - 8;
      if(s->ps.pps->colour_mapping_enabled_flag) {
        s->up_filter_inf.shift[i]    = s->sh.Bit_Depth[s->nuh_layer_id][i] - s->ps.pps->m_nCGSOutputBitDepth[i];
        s->up_filter_inf.shift_up[i] = s->ps.pps->m_nCGSOutputBitDepth[i] - 8;
      }
    }
    if (!s->ps.vps->vps_nonHEVCBaseLayerFlag){//fixme: the way we compute BL dimension should be the same with non HEVC BL
            s->BL_height = s->ps.vps->vps_ext.rep_format[s->decoder_id-1].pic_height_vps_in_luma_samples;
            s->BL_width  = s->ps.vps->vps_ext.rep_format[s->decoder_id-1].pic_width_vps_in_luma_samples;
    }
    if(s->up_filter_inf.scaleXLum == 65536 && s->up_filter_inf.scaleYLum == 65536)
        s->up_filter_inf.idx = SNR;
    else
        if(s->up_filter_inf.scaleXLum == 32768 && s->up_filter_inf.scaleYLum == 32768)
            s->up_filter_inf.idx = X2;
        else
            if(s->up_filter_inf.scaleXLum == 43691 && s->up_filter_inf.scaleYLum == 43691)
                s->up_filter_inf.idx = X1_5;
            else {
                s->up_filter_inf.idx = DEFAULT;
                av_log(s->avctx, AV_LOG_INFO, "DEFAULT mode: SSE optimizations are not implemented for spatial scalability with a ratio different from x2 and x1.5 widthBL %d heightBL %d \n", s->BL_width <<1, s->BL_height<<1);
            }
    fail:
    return ret;
}


static int hls_slice_header(HEVCContext *s)
{
    GetBitContext *gb = &s->HEVClc->gb;
    SliceHeader *sh   = &s->sh;
//#if PARALLEL_SLICE
    HEVCContext   *s1   = (HEVCContext*) s->avctx->priv_data;
//#endif
    int i, j, ret, change_pps = 0,numRef;
    int NumILRRefIdx;
    int first_slice_in_pic_flag;
    uint32_t buf;
#if HEVC_CIPHERING 
    HEVCLocalContext *lc = s->HEVClc;
    cabac_data_t *const cabac = &lc->ccc;
    bitstream_t *stream = cabac->stream;
#endif


    // Coded parameters
    first_slice_in_pic_flag = get_bits1(gb);
#if HEVC_CIPHERING 
    kvz_bitstream_put(stream, first_slice_in_pic_flag,1);
#endif
    
#if PARALLEL_SLICE
    if (IS_IRAP(s)) {
        if(!first_slice_in_pic_flag) {
            int self_id, temporal_id, nuh_layer_id, nal_unit_type, job;
            nal_unit_type = s->nal_unit_type;
            self_id       = s->self_id;
            temporal_id   = s->temporal_id;
            nuh_layer_id  = s->nuh_layer_id;
            job           = s->job;
            ff_thread_await_progress_slice(s->avctx);
            memcpy(s, s1, sizeof(HEVCContext));
            s->HEVClc                 = s1->HEVClcList[self_id];
            s->nal_unit_type          = nal_unit_type;
            s->temporal_id            = temporal_id;
            s->nuh_layer_id           = nuh_layer_id;
            s->job                    = job;
        }
    }
#endif
    //TODO move the force first_slice_in pic as an argument to this function
    sh->first_slice_in_pic_flag   = first_slice_in_pic_flag;
    if (s1->force_first_slice_in_pic) {
        if (!sh->first_slice_in_pic_flag) {
            av_log(s->avctx, AV_LOG_DEBUG, "First_slice_in_pic_flag forced\n");
            sh->first_slice_in_pic_flag = 1;
        }
      s1->force_first_slice_in_pic = 0;
    }

    if ((IS_IDR(s) || IS_BLA(s)) && sh->first_slice_in_pic_flag) {
        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra     = INT_MAX;
        if (IS_IDR(s))
            ff_hevc_clear_refs(s);
    }
    sh->no_output_of_prior_pics_flag = 0;
    if (IS_IRAP(s)){
        sh->no_output_of_prior_pics_flag = get_bits1(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put(stream, sh->no_output_of_prior_pics_flag, 1);
#endif

        if (s->decoder_id)
            av_log(s->avctx, AV_LOG_ERROR, "IRAP %d\n", s->nal_unit_type);
    }

    if (s->nal_unit_type == HEVC_NAL_CRA_NUT && s->last_eos == 1)
        sh->no_output_of_prior_pics_flag = 1;

    sh->slice_pps_id = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
    kvz_bitstream_put_ue(stream, sh->slice_pps_id);
#endif


    if (sh->slice_pps_id >= HEVC_MAX_PPS_COUNT || !s->ps.pps_list[sh->slice_pps_id]) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS %d does not exist.\n", sh->slice_pps_id);
        return AVERROR_INVALIDDATA;
    }

    if (!sh->first_slice_in_pic_flag &&
        s->ps.pps != (HEVCPPS*)s->ps.pps_list[sh->slice_pps_id]->data) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS changed between two slices of the same frame.\n");
        return AVERROR_INVALIDDATA;
    }

    //FIXME  We should add a setup ps function checking for VPS SPS and PPS
    //presence before parsing the rest of the slice header
    if(s->ps.pps != (HEVCPPS*)s->ps.pps_list[sh->slice_pps_id]->data)
        change_pps = 1;

    s->ps.pps = (HEVCPPS*)s->ps.pps_list[sh->slice_pps_id]->data;

//if (s->ps.sps_list[s->ps.pps->sps_id]!=NULL)
    //Setting up params set. //TODO set up ctx here
    if (s->ps.sps_list[s->ps.pps->sps_id] && (s->ps.sps != (HEVCSPS*)s->ps.sps_list[s->ps.pps->sps_id]->data)) {
        const HEVCSPS* last_sps = s->ps.sps;
        s->ps.sps = (HEVCSPS*)s->ps.sps_list[s->ps.pps->sps_id]->data;
        if (last_sps && IS_IRAP(s) && s->nal_unit_type != HEVC_NAL_CRA_NUT) {
            if (s->ps.sps->width !=  last_sps->width || s->ps.sps->height != last_sps->height ||
                s->ps.sps->temporal_layer[s->ps.sps->sps_max_sub_layers - 1].max_dec_pic_buffering !=
                last_sps->temporal_layer[last_sps->sps_max_sub_layers - 1].max_dec_pic_buffering)
                sh->no_output_of_prior_pics_flag = 0;
        }
        ff_hevc_clear_refs(s);
        ret = set_sps(s, s->ps.sps, AV_PIX_FMT_NONE);
        if (ret < 0)
            return ret;

        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra     = INT_MAX;
    } else if ((HEVCSPS*)s->ps.sps_list[s->ps.pps->sps_id]==NULL) {
        av_log(s,AV_LOG_ERROR, "Can't find SPS %d. Aborting...\n",s->ps.pps->sps_id);
        return -1;
    }

    setup_pps(s->avctx, (HEVCPPS*)s->ps.pps, (HEVCSPS*)s->ps.sps);

    if(change_pps && s->decoder_id)
        set_el_parameter(s);
    s->avctx->profile = s->ps.sps->ptl.general_ptl.profile_idc;
    s->avctx->level   = s->ps.sps->ptl.general_ptl.level_idc;

    sh->dependent_slice_segment_flag = 0;
    if (!sh->first_slice_in_pic_flag) {
        int slice_address_length;

        if (s->ps.pps->dependent_slice_segments_enabled_flag){
            sh->dependent_slice_segment_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->dependent_slice_segment_flag, 1);
#endif
        }

        slice_address_length = av_ceil_log2(s->ps.sps->ctb_width *
                                            s->ps.sps->ctb_height);

        sh->slice_segment_address = get_bitsz(gb, slice_address_length);
#if HEVC_CIPHERING 
        if (slice_address_length)
            kvz_bitstream_put(stream, sh->slice_segment_address, slice_address_length);
#endif

#if PARALLEL_SLICE
        s1->slice_segment_addr[s->job] = sh->slice_segment_addr;
        ff_thread_report_progress_slice2(s->avctx, s->job);
#endif

        if (sh->slice_segment_address >= s->ps.sps->ctb_width * s->ps.sps->ctb_height) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid slice segment address: %u.\n",
                   sh->slice_segment_address);
            return AVERROR_INVALIDDATA;
        }

        if (!sh->dependent_slice_segment_flag) {
            sh->slice_addr = sh->slice_segment_address;
#if PARALLEL_SLICE
if (0)
    s->slice_idx = s->job;
else
    s->slice_idx++;

#else
            s->slice_idx++;
#endif
        }

        if (sh->first_slice_in_pic_flag != first_slice_in_pic_flag) {
            s->slice_idx           = 0;
            s->slice_initialized   = 0;
        }

    } else {
        sh->slice_segment_address = sh->slice_addr = 0;
        s->slice_idx           = 0;
        s->slice_initialized   = 0;
    }

    if (!sh->dependent_slice_segment_flag) {
        int iBits = 0;
        s->slice_initialized = 0;

        if(s->ps.pps->num_extra_slice_header_bits > iBits) {
            sh->discardable_flag = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->discardable_flag, 1);
#endif
            iBits++;
        }
        if(s->ps.pps->num_extra_slice_header_bits > iBits) {
            sh->cross_layer_bla_flag = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->cross_layer_bla_flag, 1);
#endif
            iBits++;
        }
        for(;iBits < s->ps.pps->num_extra_slice_header_bits;++iBits){
            //slice_reserved not in use yet
            int bit = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, bit, 1);
#endif
        }

        sh->slice_type = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put_ue(stream, sh->slice_type);
#endif

        //TODO later check

        if (!(sh->slice_type == HEVC_SLICE_I ||
              sh->slice_type == HEVC_SLICE_P ||
              sh->slice_type == HEVC_SLICE_B)) {
            av_log(s->avctx, AV_LOG_ERROR, "Unknown slice type: %d %d .\n",
                   sh->slice_type, sh->first_slice_in_pic_flag);
            return AVERROR_INVALIDDATA;
        }

        if (!s->decoder_id && IS_IRAP(s) && sh->slice_type != HEVC_SLICE_I) {
            av_log(s->avctx, AV_LOG_ERROR, "Inter slices in an IRAP frame.\n");
            return AVERROR_INVALIDDATA;
        }

        // when flag is not present, picture is inferred to be output
        sh->pic_output_flag = 1;
        if (s->ps.pps->output_flag_present_flag) {
            sh->pic_output_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->pic_output_flag, 1);
#endif
        }

        if (s->ps.sps->separate_colour_plane_flag) {
            sh->colour_plane_id = get_bits(gb, 2);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->colour_plane_id, 2);
#endif
        }

        //FIXME we could avoid this when the context is not multi-layer.
        if (( s->nuh_layer_id > 0 && !s->ps.vps->vps_ext.poc_lsb_not_present_flag[s->ps.vps->vps_ext.layer_id_in_vps[s->nuh_layer_id]])
            || (!IS_IDR(s)) ) {
            int poc;

            sh->slice_pic_order_cnt_lsb = get_bits(gb, s->ps.sps->log2_max_poc_lsb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->slice_pic_order_cnt_lsb, s->ps.sps->log2_max_poc_lsb);
#endif

            //FIXME We might want the POC to be computed later especially when
            //slice_segment_header_extension_present_flag is used
            poc = ff_hevc_compute_poc(s, sh->slice_pic_order_cnt_lsb);
            if (!sh->first_slice_in_pic_flag && poc != s->poc) {
                av_log(s->avctx, AV_LOG_WARNING,
                       "Ignoring POC change between slices: %d -> %d\n", s->poc, poc);
                if (s->avctx->err_recognition & AV_EF_EXPLODE)
                    return AVERROR_INVALIDDATA;
                poc = s->poc;
            }
            s->poc = poc;
        }


        if(!IS_IDR(s)) {
            int pos;
            sh->short_term_ref_pic_set_sps_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->short_term_ref_pic_set_sps_flag, 1);
#endif

            pos = get_bits_left(gb);
            if (!sh->short_term_ref_pic_set_sps_flag) {
#if HEVC_CIPHERING 
                ret = ff_hevc_decode_short_term_rps_decrypt(stream, gb, s->avctx, &sh->slice_rps, s->ps.sps, 1);
#else
                ret = ff_hevc_decode_short_term_rps(gb, s->avctx, &sh->slice_rps, s->ps.sps, 1);
#endif
                if (ret < 0)
                    return ret;

                sh->short_term_rps = &sh->slice_rps;
            } else {
                int numbits, rps_idx;

                if (!s->ps.sps->num_short_term_rps) {
                    av_log(s->avctx, AV_LOG_ERROR, "No ref lists in the SPS.\n");
                    return AVERROR_INVALIDDATA;
                }

                numbits = av_ceil_log2(s->ps.sps->num_short_term_rps);
                rps_idx = numbits > 0 ? get_bits(gb, numbits) : 0;
                if(numbits > 0)
#if HEVC_CIPHERING 
                    kvz_bitstream_put(stream, rps_idx, numbits);
#endif

                sh->short_term_rps = &s->ps.sps->st_rps[rps_idx];
            }
            sh->short_term_ref_pic_set_size = pos - get_bits_left(gb);

            pos = get_bits_left(gb);
            ret = decode_lt_rps(s, &sh->long_term_rps, gb);
            if (ret < 0) {
                av_log(s->avctx, AV_LOG_WARNING, "Invalid long term RPS.\n");
                if (s->avctx->err_recognition & AV_EF_EXPLODE)
                    return AVERROR_INVALIDDATA;
            }
            sh->long_term_ref_pic_set_size = pos - get_bits_left(gb);

            if (s->ps.sps->sps_temporal_mvp_enabled_flag) {
                sh->slice_temporal_mvp_enabled_flag = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->slice_temporal_mvp_enabled_flag, 1);
#endif

            } else
                sh->slice_temporal_mvp_enabled_flag = 0;
        } else {
            s->sh.short_term_rps = NULL;
            s->poc               = 0;
        }

        /* 8.3.1 */
        if (s->temporal_id == 0 &&
            s->nal_unit_type != HEVC_NAL_TRAIL_N &&
            s->nal_unit_type != HEVC_NAL_TSA_N   &&
            s->nal_unit_type != HEVC_NAL_STSA_N  &&
            s->nal_unit_type != HEVC_NAL_RADL_N  &&
            s->nal_unit_type != HEVC_NAL_RADL_R  &&
            s->nal_unit_type != HEVC_NAL_RASL_N  &&
            s->nal_unit_type != HEVC_NAL_RASL_R)
            s->pocTid0 = s->poc;
        s->sh.active_num_ILR_ref_idx = 0;

        NumILRRefIdx = s->ps.vps->vps_ext.num_direct_ref_layers[s->nuh_layer_id];

        if (s->nuh_layer_id > 0 && !s->ps.vps->vps_ext.all_ref_layers_active_flag && NumILRRefIdx>0) {
            s->sh.inter_layer_pred_enabled_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, s->sh.inter_layer_pred_enabled_flag, 1);
#endif

            if (s->sh.inter_layer_pred_enabled_flag) {
                if (NumILRRefIdx>1)  {
                    int numBits = 1;
                    while ((1 << numBits) < NumILRRefIdx) {
                        numBits++;
                    }
                    if (!s->ps.vps->vps_ext.max_one_active_ref_layer_flag) {
                        buf = get_bits(gb, numBits);
                        s->sh.active_num_ILR_ref_idx = buf + 1;
#if HEVC_CIPHERING 
                        kvz_bitstream_put(stream, buf, numBits);
#endif
                    } else {
                        for(i=0 ; i < NumILRRefIdx; i++ ) {
                            if((s->ps.vps->vps_ext.max_tid_il_ref_pics_plus1[s->ps.vps->vps_ext.layer_id_in_vps[i]][s->nuh_layer_id] > s->temporal_id || s->temporal_id==0) && (s->ps.vps->vps_ext.sub_layers_vps_max_minus1[s->ps.vps->vps_ext.layer_id_in_vps[i]] >= s->temporal_id)) {
                                s->sh.active_num_ILR_ref_idx = 1;
                                break;
                            }
                        }
                    }

                    if( s->sh.active_num_ILR_ref_idx == NumILRRefIdx ) {
                        for( i = 0; i < s->sh.active_num_ILR_ref_idx; i++ )
                            s->sh.inter_layer_pred_layer_idc[i] =  i;
                    } else
                        for (i = 0; i < s->sh.active_num_ILR_ref_idx; i++ ) {
                            s->sh.inter_layer_pred_layer_idc[i] =  get_bits(gb, numBits);
#if HEVC_CIPHERING 
                            kvz_bitstream_put(stream, s->sh.inter_layer_pred_layer_idc[i], numBits);
#endif    
                    }
                } else {
                    if((s->ps.vps->vps_ext.max_tid_il_ref_pics_plus1[s->ps.vps->vps_ext.layer_id_in_vps[0]][s->nuh_layer_id] > s->temporal_id || s->temporal_id==0) && (s->ps.vps->vps_ext.sub_layers_vps_max_minus1[s->ps.vps->vps_ext.layer_id_in_vps[0]] >= s->temporal_id)) {
                        s->sh.active_num_ILR_ref_idx = 1;
                        s->sh.inter_layer_pred_layer_idc[0] = 0;
                    }
                }
            }
        } else {
            if(s->ps.vps->vps_ext.all_ref_layers_active_flag  && s->nuh_layer_id) {
                int   refLayerPicIdc[16];
                s->sh.inter_layer_pred_enabled_flag = 1;
                numRef = 0;
                for(i = 0;  i < NumILRRefIdx; i++ ) {
                    if((s->ps.vps->vps_ext.max_tid_il_ref_pics_plus1[s->ps.vps->vps_ext.layer_id_in_vps[i]][s->nuh_layer_id] > s->temporal_id || s->temporal_id==0) && (s->ps.vps->vps_ext.sub_layers_vps_max_minus1[s->ps.vps->vps_ext.layer_id_in_vps[i]] >= s->temporal_id))
                            refLayerPicIdc[numRef++] = i;
                }
                s->sh.active_num_ILR_ref_idx = numRef;
                for(i = 0;  i < NumILRRefIdx; i++ )
                    s->sh.inter_layer_pred_layer_idc[i] = refLayerPicIdc[i];
            }
        }

        sh->slice_sample_adaptive_offset_flag[0] =
        sh->slice_sample_adaptive_offset_flag[1] =
        sh->slice_sample_adaptive_offset_flag[2] = 0;
        if (s->ps.sps->sao_enabled_flag) {
            enum ChromaFormat format;
            sh->slice_sample_adaptive_offset_flag[0] = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->slice_sample_adaptive_offset_flag[0], 1);
#endif

            if(!s->nuh_layer_id) {
                format = s->ps.sps->chroma_format_idc;
            } else {
                int idex  =  s->ps.sps->update_rep_format_flag ? s->ps.sps->sps_rep_format_idx : s->ps.vps->vps_ext.vps_rep_format_idx [s->ps.vps->vps_ext.layer_id_in_vps[s->nuh_layer_id]];
                format = s->ps.vps->vps_ext.rep_format[idex].chroma_format_vps_idc;
            }
            if (format != CHROMA_400)  {
                sh->slice_sample_adaptive_offset_flag[1] =
                sh->slice_sample_adaptive_offset_flag[2] = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->slice_sample_adaptive_offset_flag[2], 1);
#endif

            } else {
                sh->slice_sample_adaptive_offset_flag[0] = 0;
                sh->slice_sample_adaptive_offset_flag[1] = 0;
                sh->slice_sample_adaptive_offset_flag[2] = 0;
            }
        }

        sh->nb_refs[L0] = sh->nb_refs[L1] = 0;
        if (sh->slice_type != HEVC_SLICE_I) {
            int nb_refs, num_ref_idx_active_override_flag;

            sh->nb_refs[L0] = s->ps.pps->num_ref_idx_l0_default_active;
            if (sh->slice_type == HEVC_SLICE_B)
                sh->nb_refs[L1] = s->ps.pps->num_ref_idx_l1_default_active;
            num_ref_idx_active_override_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, num_ref_idx_active_override_flag, 1);
#endif

            if (num_ref_idx_active_override_flag) { // num_ref_idx_active_override_flag
                buf = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put_ue(stream, buf);
#endif
                sh->nb_refs[L0] = buf + 1;
                if (sh->slice_type == HEVC_SLICE_B) {
                    buf = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
                    kvz_bitstream_put_ue(stream, buf);
#endif
                    sh->nb_refs[L1] = buf + 1;
                }
            }
            if (sh->nb_refs[L0] > HEVC_MAX_REFS || sh->nb_refs[L1] > HEVC_MAX_REFS) {
                av_log(s->avctx, AV_LOG_ERROR, "Too many refs: %d/%d.\n",
                       sh->nb_refs[L0], sh->nb_refs[L1]);
                return AVERROR_INVALIDDATA;
            }

            sh->rpl_modification_flag[0] = 0;
            sh->rpl_modification_flag[1] = 0;
            nb_refs = ff_hevc_frame_nb_refs(s);
            if (!nb_refs) {
                av_log(s->avctx, AV_LOG_ERROR, "Zero refs for a frame with P or B slices.\n");
                return AVERROR_INVALIDDATA;
            }
            if (s->ps.pps->lists_modification_present_flag && nb_refs > 1) {
                sh->rpl_modification_flag[0] = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->rpl_modification_flag[0], 1);
#endif
                if (sh->rpl_modification_flag[0]) {
                    for (i = 0; i < sh->nb_refs[L0]; i++) {
                        sh->list_entry_lx[0][i] = get_bits(gb, av_ceil_log2(nb_refs));
#if HEVC_CIPHERING 
                        kvz_bitstream_put(stream, sh->list_entry_lx[0][i], av_ceil_log2(nb_refs));
#endif
                    }
                }

                if (sh->slice_type == HEVC_SLICE_B) {
                    sh->rpl_modification_flag[1] = get_bits1(gb);
#if HEVC_CIPHERING 
                    kvz_bitstream_put(stream, sh->rpl_modification_flag[1], 1);
#endif
                    if (sh->rpl_modification_flag[1] == 1)
                        for (i = 0; i < sh->nb_refs[L1]; i++) {
                            sh->list_entry_lx[1][i] = get_bits(gb, av_ceil_log2(nb_refs));
#if HEVC_CIPHERING 
                            kvz_bitstream_put(stream, sh->list_entry_lx[1][i], av_ceil_log2(nb_refs));
#endif
                        }
                }
            }

            if (sh->slice_type == HEVC_SLICE_B) {
                sh->mvd_l1_zero_flag = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->mvd_l1_zero_flag, 1);
#endif
            }

            if (s->ps.pps->cabac_init_present_flag) {
                sh->cabac_init_flag = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->cabac_init_flag, 1);
#endif
            } else
                sh->cabac_init_flag = 0;

            sh->collocated_ref_idx = 0;
            if (sh->slice_temporal_mvp_enabled_flag) {
                sh->collocated_list = L0;
                if (sh->slice_type == HEVC_SLICE_B) {
                    buf = get_bits1(gb);
#if HEVC_CIPHERING 
                    kvz_bitstream_put(stream, buf, 1);
#endif
                    sh->collocated_list = !buf;
                }

                if (sh->nb_refs[sh->collocated_list] > 1) {
                    sh->collocated_ref_idx = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
                    kvz_bitstream_put_ue(stream, sh->collocated_ref_idx);
#endif
                    if (sh->collocated_ref_idx >= sh->nb_refs[sh->collocated_list]) {
                        av_log(s->avctx, AV_LOG_ERROR,
                               "Invalid collocated_ref_idx: %d.\n",
                               sh->collocated_ref_idx);
                        return AVERROR_INVALIDDATA;
                    }
                }
            }

            if ((s->ps.pps->weighted_pred_flag   && sh->slice_type == HEVC_SLICE_P) ||
                (s->ps.pps->weighted_bipred_flag && sh->slice_type == HEVC_SLICE_B)) {
                int ret = pred_weight_table(s, gb);
                if (ret < 0)
                    return ret;
            }

            buf = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put_ue(stream, buf);
#endif
            sh->max_num_merge_cand = 5 - buf;
            if (sh->max_num_merge_cand < 1 || sh->max_num_merge_cand > 5) {
                av_log(s->avctx, AV_LOG_ERROR,
                       "Invalid number of merging MVP candidates: %d.\n",
                       sh->max_num_merge_cand);
                return AVERROR_INVALIDDATA;
            }
        }

        sh->slice_qp_delta = get_se_golomb(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put_se(stream, sh->slice_qp_delta);
#endif
        if (s->ps.pps->pps_slice_chroma_qp_offsets_present_flag) {
            sh->slice_cb_qp_offset = get_se_golomb(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put_se(stream, sh->slice_cb_qp_offset);
#endif
            sh->slice_cr_qp_offset = get_se_golomb(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put_se(stream, sh->slice_cr_qp_offset);
#endif
        } else {
            sh->slice_cb_qp_offset = 0;
            sh->slice_cr_qp_offset = 0;
        }

        if (s->ps.pps->chroma_qp_offset_list_enabled_flag){
            sh->cu_chroma_qp_offset_enabled_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->cu_chroma_qp_offset_enabled_flag, 1);
#endif           
        } else
            sh->cu_chroma_qp_offset_enabled_flag = 0;

        if (s->ps.pps->deblocking_filter_control_present_flag) {
            int deblocking_filter_override_flag = 0;

            if (s->ps.pps->deblocking_filter_override_enabled_flag) {
                deblocking_filter_override_flag = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, deblocking_filter_override_flag, 1);
#endif
            }

            if (deblocking_filter_override_flag) {
                sh->disable_deblocking_filter_flag = get_bits1(gb);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream, sh->disable_deblocking_filter_flag, 1);
#endif
                if (!sh->disable_deblocking_filter_flag) {
                    buf = get_se_golomb(gb);
#if HEVC_CIPHERING 
                    kvz_bitstream_put_se(stream, buf);
#endif
                    sh->slice_beta_offset = buf * 2;
                    buf = get_se_golomb(gb);
#if HEVC_CIPHERING 
                    kvz_bitstream_put_se(stream, buf);
#endif
                    sh->slice_tc_offset   = buf * 2;
                }
            } else {
                sh->disable_deblocking_filter_flag = s->ps.pps->pps_deblocking_filter_disabled_flag;
                sh->slice_beta_offset                    = s->ps.pps->pps_beta_offset;
                sh->slice_tc_offset                      = s->ps.pps->pps_tc_offset;
            }
        } else {
            sh->disable_deblocking_filter_flag = 0;
            sh->slice_beta_offset                    = 0;
            sh->slice_tc_offset                      = 0;
        }

        if (s->ps.pps->pps_loop_filter_across_slices_enabled_flag &&
            (sh->slice_sample_adaptive_offset_flag[0] ||
             sh->slice_sample_adaptive_offset_flag[1] ||
             !sh->disable_deblocking_filter_flag)) {
            sh->slice_loop_filter_across_slices_enabled_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream, sh->slice_loop_filter_across_slices_enabled_flag, 1);
#endif
        } else {
            sh->slice_loop_filter_across_slices_enabled_flag = s->ps.pps->pps_loop_filter_across_slices_enabled_flag;
        }
    } else if (!s->slice_initialized) {
        av_log(s->avctx, AV_LOG_ERROR, "Independent slice segment missing.\n");
        return AVERROR_INVALIDDATA;
    }

    sh->num_entry_point_offsets = 0;
    if (s->ps.pps->tiles_enabled_flag || s->ps.pps->entropy_coding_sync_enabled_flag) {
        sh->num_entry_point_offsets = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put_ue(stream, sh->num_entry_point_offsets);
#endif
        if(s->ps.pps->entropy_coding_sync_enabled_flag) {
            if(sh->num_entry_point_offsets < 0) {
                av_log(s->avctx, AV_LOG_ERROR,
                   "The number of entries %d is higher than the number of CTB rows %d \n",
                   sh->num_entry_point_offsets,
                   s->ps.sps->ctb_height);
                return AVERROR_INVALIDDATA;
            }
        } else {
            if(sh->num_entry_point_offsets > s->ps.sps->ctb_height*s->ps.sps->ctb_width || sh->num_entry_point_offsets < 0) {
                av_log(s->avctx, AV_LOG_ERROR,
                       "The number of entries %d is higher than the number of CTBs %d \n",
                       sh->num_entry_point_offsets,
                       s->ps.sps->ctb_height*s->ps.sps->ctb_width);
                return AVERROR_INVALIDDATA;
            }
        }
        if (sh->num_entry_point_offsets > 0) {
            buf = get_ue_golomb_long(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put_ue(stream, buf);
#endif
            int offset_len = buf + 1;
            
            int segments = offset_len >> 4;
            int rest = (offset_len & 15);
            av_freep(&sh->entry_point_offset);
            av_freep(&sh->offset);
            av_freep(&sh->size);
            sh->entry_point_offset = av_malloc_array(sh->num_entry_point_offsets, sizeof(int));
            sh->offset = av_malloc_array(sh->num_entry_point_offsets, sizeof(int));
            sh->size = av_malloc_array(sh->num_entry_point_offsets, sizeof(int));
            if (!sh->entry_point_offset || !sh->offset || !sh->size) {
                sh->num_entry_point_offsets = 0;
                av_log(s->avctx, AV_LOG_ERROR, "Failed to allocate memory\n");
                return AVERROR(ENOMEM);
            }
            for (i = 0; i < sh->num_entry_point_offsets; i++) {
                unsigned val = get_bits_long(gb, offset_len);
#if HEVC_CIPHERING 
                kvz_bitstream_put(stream,val,offset_len);
#endif
                sh->entry_point_offset[i] = val + 1; // +1; // +1 to get the size
            }
            if (s->threads_number > 1 && (s->ps.pps->num_tile_rows > 1 || s->ps.pps->num_tile_columns > 1)) {
                s->enable_parallel_tiles = 0; // TODO: you can enable tiles in parallel here
                //s->threads_number = 1;
            } else
                s->enable_parallel_tiles = 0;
        } else
            s->enable_parallel_tiles = 0;
    }

    if (s->ps.pps->slice_segment_header_extension_present_flag) {
        int curr_index;

        sh->slice_segment_header_extension_length = get_ue_golomb(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put_ue(stream, sh->slice_segment_header_extension_length);
#endif
        curr_index = gb->index;

        if (sh->slice_segment_header_extension_length * 8LL > get_bits_left(gb)) {
            av_log(s->avctx, AV_LOG_ERROR, "too many slice_header_extension_data_bytes\n");
            return AVERROR_INVALIDDATA;
        }


        av_log(s->avctx, AV_LOG_WARNING,
               "========= SLICE HEADER extension are parsed but still unused\n");

        if(s->ps.pps->poc_reset_info_present_flag){
            sh->poc_reset_idc = get_bits(gb,2);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream,sh->poc_reset_idc,2);
#endif
        }

        if (sh->poc_reset_idc){
            sh->poc_reset_period_id = get_bits(gb,6);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream,sh->poc_reset_period_id,6);
#endif
        }

        if(sh->poc_reset_idc == 3){
            sh->full_poc_reset_flag = get_bits1(gb);
            sh->poc_lsb_val = get_bits(gb,s->ps.sps->log2_max_poc_lsb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream,sh->full_poc_reset_flag,1);
            kvz_bitstream_put(stream,sh->poc_lsb_val,s->ps.sps->log2_max_poc_lsb);
#endif
        }

        //FIXME Not really sure about this
        if(s->ps.vps->vps_ext.vps_poc_lsb_aligned_flag && !(( IS_BLA(s) || s->nal_unit_type == HEVC_NAL_CRA_NUT)
                                                            && s->ps.vps->vps_ext.number_ref_layers[s->nuh_layer_id][0] == 0)){
            sh->poc_msb_cycle_val_present_flag = get_bits1(gb);
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream,sh->poc_msb_cycle_val_present_flag,1);
#endif
        }

        if(sh->poc_msb_cycle_val_present_flag){
            sh->poc_msb_cycle_val = get_ue_golomb(gb);
#if HEVC_CIPHERING 
        kvz_bitstream_put_ue(stream, sh->poc_msb_cycle_val);
#endif
        }

        if (gb->index - curr_index < sh->slice_segment_header_extension_length*8U){
            int nb_bits = sh->slice_segment_header_extension_length*8U - (gb->index - curr_index);
            skip_bits(gb, nb_bits );
#if HEVC_CIPHERING 
            kvz_bitstream_put(stream,0x00,nb_bits);	// Another way to copy the skipped bits?
#endif
        }

    }

    //TODO add a global parameters check here + move other infered parameters
    //to a new struture.

    // Inferred parameters
    sh->slice_qp = 26U + s->ps.pps->init_qp_minus26 + sh->slice_qp_delta;
    if (sh->slice_qp > 51 ||
        sh->slice_qp < -s->ps.sps->qp_bd_offset) {
        av_log(s->avctx, AV_LOG_ERROR,
               "The slice_qp %d is outside the valid range "
               "[%d, 51].\n",
               sh->slice_qp,
               -s->ps.sps->qp_bd_offset);
        return AVERROR_INVALIDDATA;
    }

    sh->slice_ctb_addr_rs = sh->slice_segment_address;

    if (!s->sh.slice_ctb_addr_rs && s->sh.dependent_slice_segment_flag) {
        av_log(s->avctx, AV_LOG_ERROR, "Impossible slice segment.\n");
        return AVERROR_INVALIDDATA;
    }

    if (get_bits_left(gb) < 0) {
        av_log(s->avctx, AV_LOG_ERROR,
               "Overread slice header by %d bits\n", -get_bits_left(gb));
        return AVERROR_INVALIDDATA;
    }

    s->HEVClc->first_qp_group = !s->sh.dependent_slice_segment_flag;

    if (!s->ps.pps->cu_qp_delta_enabled_flag)
        s->HEVClc->qp_y = s->sh.slice_qp;

    s->slice_initialized = 1;
    s->HEVClc->tu.cu_qp_offset_cb = 0;
    s->HEVClc->tu.cu_qp_offset_cr = 0;

    s->no_rasl_output_flag = IS_IDR(s) || IS_BLA(s) || (s->nal_unit_type == HEVC_NAL_CRA_NUT && s->last_eos);

    return 0;
}

#define CTB(tab, x, y) ((tab)[(y) * s->ps.sps->ctb_width + (x)])

#define SET_SAO(elem, value)                            \
do {                                                    \
    if (!sao_merge_up_flag && !sao_merge_left_flag)     \
        sao->elem = value;                              \
    else if (sao_merge_left_flag)                       \
        sao->elem = CTB(s->sao, rx-1, ry).elem;         \
    else if (sao_merge_up_flag)                         \
        sao->elem = CTB(s->sao, rx, ry-1).elem;         \
    else                                                \
        sao->elem = 0;                                  \
} while (0)

static void hls_sao_param(HEVCContext *s, int rx, int ry)
{
    HEVCLocalContext *lc    = s->HEVClc;
#if HEVC_CIPHERING 
    cabac_data_t *const cabac = &lc->ccc;
#endif
    int sao_merge_left_flag = 0;
    int sao_merge_up_flag   = 0;
    SAOParams *sao          = &CTB(s->sao, rx, ry);
    int c_idx, i;

    if (s->sh.slice_sample_adaptive_offset_flag[0] ||
        s->sh.slice_sample_adaptive_offset_flag[1]) {
        if (rx > 0) {
            if (lc->ctb_left_flag){
                sao_merge_left_flag = ff_hevc_sao_merge_flag_decode(s);
#if HEVC_CIPHERING 
                cabac->cur_ctx = &(cabac->ctx.sao_merge_flag_model);
                CABAC_BIN(cabac, sao_merge_left_flag, "sao_merge_left_flag");
#endif
            }
        }
        if (ry > 0 && !sao_merge_left_flag) {
            if (lc->ctb_up_flag){
                sao_merge_up_flag = ff_hevc_sao_merge_flag_decode(s);
#if HEVC_CIPHERING 
                cabac->cur_ctx = &(cabac->ctx.sao_merge_flag_model);
                CABAC_BIN(cabac, sao_merge_up_flag, "sao_merge_up_flag");
#endif
            }
        }
    }

    for (c_idx = 0; c_idx < (s->ps.sps->chroma_format_idc ? 3 : 1); c_idx++) {
        int log2_sao_offset_scale = c_idx == 0 ? s->ps.pps->log2_sao_offset_scale_luma :
                                                 s->ps.pps->log2_sao_offset_scale_chroma;

        if (!s->sh.slice_sample_adaptive_offset_flag[c_idx]) {
            sao->type_idx[c_idx] = SAO_NOT_APPLIED;
            continue;
        }

        if (c_idx == 2) {
            sao->type_idx[2] = sao->type_idx[1];
            sao->eo_class[2] = sao->eo_class[1];
        } else {
            SET_SAO(type_idx[c_idx], ff_hevc_sao_type_idx_decode(s));
        }

        if (sao->type_idx[c_idx] == SAO_NOT_APPLIED)
            continue;

        for (i = 0; i < 4; i++)
            SET_SAO(offset_abs[c_idx][i], ff_hevc_sao_offset_abs_decode(s));

        if (sao->type_idx[c_idx] == SAO_BAND) {
            for (i = 0; i < 4; i++) {
                if (sao->offset_abs[c_idx][i]) {
                    SET_SAO(offset_sign[c_idx][i],
                            ff_hevc_sao_offset_sign_decode(s));
                } else {
                    sao->offset_sign[c_idx][i] = 0;
                }
            }
            SET_SAO(band_position[c_idx], ff_hevc_sao_band_position_decode(s));
        } else if (c_idx != 2) {
            SET_SAO(eo_class[c_idx], ff_hevc_sao_eo_class_decode(s));
        }

        // Inferred parameters
        sao->offset_val[c_idx][0] = 0;
        for (i = 0; i < 4; i++) {
            sao->offset_val[c_idx][i + 1] = sao->offset_abs[c_idx][i];
            if (sao->type_idx[c_idx] == SAO_EDGE) {
                if (i > 1)
                    sao->offset_val[c_idx][i + 1] = -sao->offset_val[c_idx][i + 1];
            } else if (sao->offset_sign[c_idx][i]) {
                sao->offset_val[c_idx][i + 1] = -sao->offset_val[c_idx][i + 1];
            }
            sao->offset_val[c_idx][i + 1] *= 1 << log2_sao_offset_scale;
        }
    }
}

#undef SET_SAO
#undef CTB

static int hls_cross_component_pred(HEVCContext *s, int idx) {
    HEVCLocalContext *lc    = s->HEVClc;
    int log2_res_scale_abs_plus1 = ff_hevc_log2_res_scale_abs(s, idx);

    if (log2_res_scale_abs_plus1 !=  0) {
        int res_scale_sign_flag = ff_hevc_res_scale_sign_flag(s, idx);
        lc->tu.res_scale_val = (1 << (log2_res_scale_abs_plus1 - 1)) *
                               (1 - 2 * res_scale_sign_flag);
    } else {
        lc->tu.res_scale_val = 0;
    }


    return 0;
}

static int hls_transform_unit(HEVCContext *s, int x0, int y0,
                              int xBase, int yBase, int cb_xBase, int cb_yBase,
                              int log2_cb_size, int log2_trafo_size,
                              int blk_idx, int cbf_luma, int *cbf_cb, int *cbf_cr)
{
    HEVCLocalContext *lc = s->HEVClc;
    const int log2_trafo_size_c = log2_trafo_size - s->ps.sps->hshift[1];
    int i;

    if (lc->cu.pred_mode == MODE_INTRA) {
        int trafo_size = 1 << log2_trafo_size;
        ff_hevc_set_neighbour_available(s, x0, y0, trafo_size, trafo_size);

        s->hpc.intra_pred[log2_trafo_size - 2](s, x0, y0, 0);
    }

    if (cbf_luma || cbf_cb[0] || cbf_cr[0] ||
        (s->ps.sps->chroma_format_idc == 2 && (cbf_cb[1] || cbf_cr[1]))) {
        int scan_idx   = SCAN_DIAG;
        int scan_idx_c = SCAN_DIAG;
        int cbf_chroma = cbf_cb[0] || cbf_cr[0] ||
                         (s->ps.sps->chroma_format_idc == 2 &&
                         (cbf_cb[1] || cbf_cr[1]));

        if (s->ps.pps->cu_qp_delta_enabled_flag && !lc->tu.is_cu_qp_delta_coded) {
            lc->tu.cu_qp_delta = ff_hevc_cu_qp_delta_abs(s);
            if (lc->tu.cu_qp_delta != 0)
                if (ff_hevc_cu_qp_delta_sign_flag(s) == 1)
                    lc->tu.cu_qp_delta = -lc->tu.cu_qp_delta;
            lc->tu.is_cu_qp_delta_coded = 1;

            if (lc->tu.cu_qp_delta < -(26 + s->ps.sps->qp_bd_offset / 2) ||
                lc->tu.cu_qp_delta >  (25 + s->ps.sps->qp_bd_offset / 2)) {
                av_log(s->avctx, AV_LOG_ERROR,
                       "The cu_qp_delta %d is outside the valid range "
                       "[%d, %d].\n",
                       lc->tu.cu_qp_delta,
                       -(26 + s->ps.sps->qp_bd_offset / 2),
                        (25 + s->ps.sps->qp_bd_offset / 2));
                return AVERROR_INVALIDDATA;
            }

            ff_hevc_set_qPy(s, cb_xBase, cb_yBase, log2_cb_size);
        }

        if (s->sh.cu_chroma_qp_offset_enabled_flag && cbf_chroma &&
            !lc->cu.cu_transquant_bypass_flag  &&  !lc->tu.is_cu_chroma_qp_offset_coded) {
            int cu_chroma_qp_offset_flag = ff_hevc_cu_chroma_qp_offset_flag(s);
            if (cu_chroma_qp_offset_flag) {
                int cu_chroma_qp_offset_idx  = 0;
                if (s->ps.pps->chroma_qp_offset_list_len_minus1 > 0) {
                    cu_chroma_qp_offset_idx = ff_hevc_cu_chroma_qp_offset_idx(s);
                    av_log(s->avctx, AV_LOG_ERROR,
                        "cu_chroma_qp_offset_idx not yet tested.\n");
                }
                lc->tu.cu_qp_offset_cb = s->ps.pps->cb_qp_offset_list[cu_chroma_qp_offset_idx];
                lc->tu.cu_qp_offset_cr = s->ps.pps->cr_qp_offset_list[cu_chroma_qp_offset_idx];
            } else {
                lc->tu.cu_qp_offset_cb = 0;
                lc->tu.cu_qp_offset_cr = 0;
            }
            lc->tu.is_cu_chroma_qp_offset_coded = 1;
        }

        if (lc->cu.pred_mode == MODE_INTRA && log2_trafo_size < 4) {
            if (lc->tu.intra_pred_mode >= 6 &&
                lc->tu.intra_pred_mode <= 14) {
                scan_idx = SCAN_VERT;
            } else if (lc->tu.intra_pred_mode >= 22 &&
                       lc->tu.intra_pred_mode <= 30) {
                scan_idx = SCAN_HORIZ;
            }

            if (lc->tu.intra_pred_mode_c >=  6 &&
                lc->tu.intra_pred_mode_c <= 14) {
                scan_idx_c = SCAN_VERT;
            } else if (lc->tu.intra_pred_mode_c >= 22 &&
                       lc->tu.intra_pred_mode_c <= 30) {
                scan_idx_c = SCAN_HORIZ;
            }
        }

        lc->tu.cross_pf = 0;

        if (cbf_luma)
            ff_hevc_hls_residual_coding(s, x0, y0, log2_trafo_size, scan_idx, 0
#if COM16_C806_EMT
                                        , log2_cb_size
#endif
            );
        if (s->ps.sps->chroma_format_idc && (log2_trafo_size > 2 || s->ps.sps->chroma_format_idc == 3)) {
            int trafo_size_h = 1 << (log2_trafo_size_c + s->ps.sps->hshift[1]);
            int trafo_size_v = 1 << (log2_trafo_size_c + s->ps.sps->vshift[1]);
            lc->tu.cross_pf  = (s->ps.pps->cross_component_prediction_enabled_flag && cbf_luma &&
                                (lc->cu.pred_mode == MODE_INTER ||
                                 (lc->tu.chroma_mode_c ==  4)));

            if (lc->tu.cross_pf) {
                hls_cross_component_pred(s, 0);
            }
            for (i = 0; i < (s->ps.sps->chroma_format_idc == 2 ? 2 : 1); i++) {
                if (lc->cu.pred_mode == MODE_INTRA) {
                    ff_hevc_set_neighbour_available(s, x0, y0 + (i << log2_trafo_size_c), trafo_size_h, trafo_size_v);
                    s->hpc.intra_pred[log2_trafo_size_c - 2](s, x0, y0 + (i << log2_trafo_size_c), 1);
                }
                if (cbf_cb[i])
                    ff_hevc_hls_residual_coding(s, x0, y0 + (i << log2_trafo_size_c),
                                                log2_trafo_size_c, scan_idx_c, 1
#if COM16_C806_EMT
                                                , log2_cb_size
#endif
                    );
                else
                    if (lc->tu.cross_pf) {
                        ptrdiff_t stride = s->frame->linesize[1];
                        int hshift = s->ps.sps->hshift[1];
                        int vshift = s->ps.sps->vshift[1];
                        int16_t *coeffs_y = lc->tu.coeffs[0];
                        int16_t *coeffs =   lc->tu.coeffs[1];
                        int size = 1 << log2_trafo_size_c;

                        uint8_t *dst = &s->frame->data[1][(y0 >> vshift) * stride +
                                                              ((x0 >> hshift) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];
                        for (i = 0; i < (size * size); i++) {
                            coeffs[i] = ((lc->tu.res_scale_val * coeffs_y[i]) >> 3);
                        }
                        s->hevcdsp.transform_add[log2_trafo_size-2](dst, coeffs, stride);
                    }
            }

            if (lc->tu.cross_pf) {
                hls_cross_component_pred(s, 1);
            }
            for (i = 0; i < (s->ps.sps->chroma_format_idc == 2 ? 2 : 1); i++) {
                if (lc->cu.pred_mode == MODE_INTRA) {
                    ff_hevc_set_neighbour_available(s, x0, y0 + (i << log2_trafo_size_c), trafo_size_h, trafo_size_v);
                    s->hpc.intra_pred[log2_trafo_size_c - 2](s, x0, y0 + (i << log2_trafo_size_c), 2);
                }
                if (cbf_cr[i])
                    ff_hevc_hls_residual_coding(s, x0, y0 + (i << log2_trafo_size_c),
                                                log2_trafo_size_c, scan_idx_c, 2
#if COM16_C806_EMT
                                                , log2_cb_size
#endif
                    );
                else
                    if (lc->tu.cross_pf) {
                        ptrdiff_t stride = s->frame->linesize[2];
                        int hshift = s->ps.sps->hshift[2];
                        int vshift = s->ps.sps->vshift[2];
                        int16_t *coeffs_y = lc->tu.coeffs[0];
                        int16_t *coeffs =   lc->tu.coeffs[1];
                        int size = 1 << log2_trafo_size_c;

                        uint8_t *dst = &s->frame->data[2][(y0 >> vshift) * stride +
                                                          ((x0 >> hshift) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];
                        for (i = 0; i < (size * size); i++) {
                            coeffs[i] = ((lc->tu.res_scale_val * coeffs_y[i]) >> 3);
                        }
                        s->hevcdsp.transform_add[log2_trafo_size-2](dst, coeffs, stride);
                    }
            }
        } else if (s->ps.sps->chroma_format_idc && blk_idx == 3) {
            int trafo_size_h = 1 << (log2_trafo_size + 1);
            int trafo_size_v = 1 << (log2_trafo_size + s->ps.sps->vshift[1]);
            for (i = 0; i < (s->ps.sps->chroma_format_idc == 2 ? 2 : 1); i++) {
                if (lc->cu.pred_mode == MODE_INTRA) {
                    ff_hevc_set_neighbour_available(s, xBase, yBase + (i << log2_trafo_size),
                                                    trafo_size_h, trafo_size_v);
                    s->hpc.intra_pred[log2_trafo_size - 2](s, xBase, yBase + (i << log2_trafo_size), 1);
                }
                if (cbf_cb[i])
                    ff_hevc_hls_residual_coding(s, xBase, yBase + (i << log2_trafo_size),
                                                log2_trafo_size, scan_idx_c, 1
#if COM16_C806_EMT
                                                , log2_cb_size
#endif
            		);
            }
            for (i = 0; i < (s->ps.sps->chroma_format_idc == 2 ? 2 : 1); i++) {
                if (lc->cu.pred_mode == MODE_INTRA) {
                    ff_hevc_set_neighbour_available(s, xBase, yBase + (i << log2_trafo_size),
                                                trafo_size_h, trafo_size_v);
                    s->hpc.intra_pred[log2_trafo_size - 2](s, xBase, yBase + (i << log2_trafo_size), 2);
                }
                if (cbf_cr[i])
                    ff_hevc_hls_residual_coding(s, xBase, yBase + (i << log2_trafo_size),
                                                log2_trafo_size, scan_idx_c, 2
#if COM16_C806_EMT
                                                , log2_cb_size
#endif
                    );
            }
        }
    } else if (s->ps.sps->chroma_format_idc && lc->cu.pred_mode == MODE_INTRA) {
        if (log2_trafo_size > 2 || s->ps.sps->chroma_format_idc == 3) {
            int trafo_size_h = 1 << (log2_trafo_size_c + s->ps.sps->hshift[1]);
            int trafo_size_v = 1 << (log2_trafo_size_c + s->ps.sps->vshift[1]);
            ff_hevc_set_neighbour_available(s, x0, y0, trafo_size_h, trafo_size_v);
            s->hpc.intra_pred[log2_trafo_size_c - 2](s, x0, y0, 1);
            s->hpc.intra_pred[log2_trafo_size_c - 2](s, x0, y0, 2);
            if (s->ps.sps->chroma_format_idc == 2) {
                ff_hevc_set_neighbour_available(s, x0, y0 + (1 << log2_trafo_size_c),
                                                trafo_size_h, trafo_size_v);
                s->hpc.intra_pred[log2_trafo_size_c - 2](s, x0, y0 + (1 << log2_trafo_size_c), 1);
                s->hpc.intra_pred[log2_trafo_size_c - 2](s, x0, y0 + (1 << log2_trafo_size_c), 2);
            }
        } else if (blk_idx == 3) {
            int trafo_size_h = 1 << (log2_trafo_size + 1);
            int trafo_size_v = 1 << (log2_trafo_size + s->ps.sps->vshift[1]);
            ff_hevc_set_neighbour_available(s, xBase, yBase,
                                            trafo_size_h, trafo_size_v);
            s->hpc.intra_pred[log2_trafo_size - 2](s, xBase, yBase, 1);
            s->hpc.intra_pred[log2_trafo_size - 2](s, xBase, yBase, 2);
            if (s->ps.sps->chroma_format_idc == 2) {
                ff_hevc_set_neighbour_available(s, xBase, yBase + (1 << (log2_trafo_size)),
                                                trafo_size_h, trafo_size_v);
                s->hpc.intra_pred[log2_trafo_size - 2](s, xBase, yBase + (1 << (log2_trafo_size)), 1);
                s->hpc.intra_pred[log2_trafo_size - 2](s, xBase, yBase + (1 << (log2_trafo_size)), 2);
            }
        }
    }

    return 0;
}

static void set_deblocking_bypass(HEVCContext *s, int x0, int y0, int log2_cb_size)
{
    int cb_size          = 1 << log2_cb_size;
    int log2_min_pu_size = s->ps.sps->log2_min_pu_size;

    int min_pu_width     = s->ps.sps->min_pu_width;
    int x_end = FFMIN(x0 + cb_size, s->ps.sps->width);
    int y_end = FFMIN(y0 + cb_size, s->ps.sps->height);
    int i, j;

    for (j = (y0 >> log2_min_pu_size); j < (y_end >> log2_min_pu_size); j++)
        for (i = (x0 >> log2_min_pu_size); i < (x_end >> log2_min_pu_size); i++)
            s->is_pcm[i + j * min_pu_width] = 2;
}

static int hls_transform_tree(HEVCContext *s, int x0, int y0,
                              int xBase, int yBase, int cb_xBase, int cb_yBase,
                              int log2_cb_size, int log2_trafo_size,
                              int trafo_depth, int blk_idx,
                              const int *base_cbf_cb, const int *base_cbf_cr)
{
    HEVCLocalContext *lc = s->HEVClc;
    uint8_t split_transform_flag;
    int cbf_cb[2];
    int cbf_cr[2];
    int ret;

    cbf_cb[0] = base_cbf_cb[0];
    cbf_cb[1] = base_cbf_cb[1];
    cbf_cr[0] = base_cbf_cr[0];
    cbf_cr[1] = base_cbf_cr[1];

    if (lc->cu.intra_split_flag) {
        if (trafo_depth == 1) {
            lc->tu.intra_pred_mode   = lc->pu.intra_pred_mode[blk_idx];
            if (s->ps.sps->chroma_format_idc == 3) {
                lc->tu.intra_pred_mode_c = lc->pu.intra_pred_mode_c[blk_idx];
                lc->tu.chroma_mode_c     = lc->pu.chroma_mode_c[blk_idx];
            } else {
                lc->tu.intra_pred_mode_c = lc->pu.intra_pred_mode_c[0];
                lc->tu.chroma_mode_c     = lc->pu.chroma_mode_c[0];
            }
        }
    } else {
        lc->tu.intra_pred_mode   = lc->pu.intra_pred_mode[0];
        lc->tu.intra_pred_mode_c = lc->pu.intra_pred_mode_c[0];
        lc->tu.chroma_mode_c     = lc->pu.chroma_mode_c[0];
    }

    if (log2_trafo_size <= s->ps.sps->log2_max_trafo_size &&
        log2_trafo_size >  s->ps.sps->log2_min_tb_size    &&
        trafo_depth     < lc->cu.max_trafo_depth       &&
        !(lc->cu.intra_split_flag && trafo_depth == 0)) {
        split_transform_flag = ff_hevc_split_transform_flag_decode(s, log2_trafo_size);
    } else {
        int inter_split = s->ps.sps->max_transform_hierarchy_depth_inter == 0 &&
                          lc->cu.pred_mode == MODE_INTER &&
                          lc->cu.part_mode != PART_2Nx2N &&
                          trafo_depth == 0;

        split_transform_flag = log2_trafo_size > s->ps.sps->log2_max_trafo_size ||
                               (lc->cu.intra_split_flag && trafo_depth == 0) ||
                               inter_split;
    }

    if (s->ps.sps->chroma_format_idc && (log2_trafo_size > 2 || s->ps.sps->chroma_format_idc == 3)) {
        if (trafo_depth == 0 || cbf_cb[0]) {
            cbf_cb[0] = ff_hevc_cbf_cb_cr_decode(s, trafo_depth);
            if (s->ps.sps->chroma_format_idc == 2 && (!split_transform_flag || log2_trafo_size == 3)) {
                cbf_cb[1] = ff_hevc_cbf_cb_cr_decode(s, trafo_depth);
            }
        }

        if (trafo_depth == 0 || cbf_cr[0]) {
            cbf_cr[0] = ff_hevc_cbf_cb_cr_decode(s, trafo_depth);
            if (s->ps.sps->chroma_format_idc == 2 && (!split_transform_flag || log2_trafo_size == 3)) {
                cbf_cr[1] = ff_hevc_cbf_cb_cr_decode(s, trafo_depth);
            }
        }
    }

    if (split_transform_flag) {
        const int trafo_size_split = 1 << (log2_trafo_size - 1);
        const int x1 = x0 + trafo_size_split;
        const int y1 = y0 + trafo_size_split;

#if COM16_C806_EMT
        if (0==trafo_depth)
        {
            s->HEVClc->cu.emt_cu_flag = ff_hevc_emt_cu_flag_decode(s, log2_cb_size, 1);
        }
#endif

#define SUBDIVIDE(x, y, idx)                                                    \
do {                                                                            \
    ret = hls_transform_tree(s, x, y, x0, y0, cb_xBase, cb_yBase, log2_cb_size, \
                             log2_trafo_size - 1, trafo_depth + 1, idx,         \
                             cbf_cb, cbf_cr);                                   \
    if (ret < 0)                                                                \
        return ret;                                                             \
} while (0)

        SUBDIVIDE(x0, y0, 0);
        SUBDIVIDE(x1, y0, 1);
        SUBDIVIDE(x0, y1, 2);
        SUBDIVIDE(x1, y1, 3);

#undef SUBDIVIDE
    } else {
        int min_tu_size      = 1 << s->ps.sps->log2_min_tb_size;
        int log2_min_tu_size = s->ps.sps->log2_min_tb_size;
        int min_tu_width     = s->ps.sps->min_tb_width;
        int cbf_luma         = 1;

        if (lc->cu.pred_mode == MODE_INTRA || trafo_depth != 0 ||
            cbf_cb[0] || cbf_cr[0] ||
            (s->ps.sps->chroma_format_idc == 2 && (cbf_cb[1] || cbf_cr[1]))) {
            cbf_luma = ff_hevc_cbf_luma_decode(s, trafo_depth);
        }

#if COM16_C806_EMT
        if (0 == trafo_depth)
        {
        	s->HEVClc->cu.emt_cu_flag = ff_hevc_emt_cu_flag_decode(s, log2_cb_size, cbf_luma);
        }
#endif

        ret = hls_transform_unit(s, x0, y0, xBase, yBase, cb_xBase, cb_yBase,
                                 log2_cb_size, log2_trafo_size,
                                 blk_idx, cbf_luma, cbf_cb, cbf_cr);
        if (ret < 0)
            return ret;
        // TODO: store cbf_luma somewhere else
        if (cbf_luma) {
            int i, j;
            for (i = 0; i < (1 << log2_trafo_size); i += min_tu_size)
                for (j = 0; j < (1 << log2_trafo_size); j += min_tu_size) {
                    int x_tu = (x0 + j) >> log2_min_tu_size;
                    int y_tu = (y0 + i) >> log2_min_tu_size;
                    s->cbf_luma[y_tu * min_tu_width + x_tu] = 1;
                }
        }
        if (!s->sh.disable_deblocking_filter_flag) {
            ff_hevc_deblocking_boundary_strengths(s, x0, y0, log2_trafo_size);
            if (s->ps.pps->transquant_bypass_enable_flag &&
                lc->cu.cu_transquant_bypass_flag)
                set_deblocking_bypass(s, x0, y0, log2_trafo_size);
        }
    }
    return 0;
}

static int hls_pcm_sample(HEVCContext *s, int x0, int y0, int log2_cb_size)
{
    //TODO: non-4:2:0 support
    HEVCLocalContext *lc = s->HEVClc;
    GetBitContext gb;
    int cb_size   = 1 << log2_cb_size;
    ptrdiff_t stride0 = s->frame->linesize[0];
    ptrdiff_t stride1 = s->frame->linesize[1];
    ptrdiff_t stride2 = s->frame->linesize[2];
    uint8_t *dst0 = &s->frame->data[0][y0 * stride0 + (x0 << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA])];
    uint8_t *dst1 = &s->frame->data[1][(y0 >> s->ps.sps->vshift[1]) * stride1 + ((x0 >> s->ps.sps->hshift[1]) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];
    uint8_t *dst2 = &s->frame->data[2][(y0 >> s->ps.sps->vshift[2]) * stride2 + ((x0 >> s->ps.sps->hshift[2]) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];

    int length         = cb_size * cb_size * s->ps.sps->pcm.bit_depth +
                         (((cb_size >> s->ps.sps->hshift[1]) * (cb_size >> s->ps.sps->vshift[1])) +
                          ((cb_size >> s->ps.sps->hshift[2]) * (cb_size >> s->ps.sps->vshift[2]))) *
                          s->ps.sps->pcm.bit_depth_chroma;
    const uint8_t *pcm = skip_bytes(&lc->cc, (length + 7) >> 3);
    int ret;

    if (!s->sh.disable_deblocking_filter_flag)
        ff_hevc_deblocking_boundary_strengths(s, x0, y0, log2_cb_size);

    ret = init_get_bits(&gb, pcm, length);
    if (ret < 0)
        return ret;

    s->hevcdsp.put_pcm(dst0, stride0, cb_size, cb_size,     &gb, s->ps.sps->pcm.bit_depth);
    if (s->ps.sps->chroma_format_idc) {
        s->hevcdsp.put_pcm(dst1, stride1,
                           cb_size >> s->ps.sps->hshift[1],
                           cb_size >> s->ps.sps->vshift[1],
                           &gb, s->ps.sps->pcm.bit_depth_chroma);
        s->hevcdsp.put_pcm(dst2, stride2,
                           cb_size >> s->ps.sps->hshift[2],
                           cb_size >> s->ps.sps->vshift[2],
                           &gb, s->ps.sps->pcm.bit_depth_chroma);
    }

    return 0;
}

/**
 * 8.5.3.2.2.1 Luma sample unidirectional interpolation process
 *
 * @param s HEVC decoding context
 * @param dst target buffer for block data at block position
 * @param dststride stride of the dst buffer
 * @param ref reference picture buffer at origin (0, 0)
 * @param mv motion vector (relative to block position) to get pixel data from
 * @param x_off horizontal position of block from origin (0, 0)
 * @param y_off vertical position of block from origin (0, 0)
 * @param block_w width of block
 * @param block_h height of block
 * @param luma_weight weighting factor applied to the luma prediction
 * @param luma_offset additive offset applied to the luma prediction value
 */

static void luma_mc_uni(HEVCContext *s, uint8_t *dst, ptrdiff_t dststride,
                        AVFrame *ref, const Mv *mv, int x_off, int y_off,
                        int block_w, int block_h, int luma_weight, int luma_offset)
{
    HEVCLocalContext *lc = s->HEVClc;
    uint8_t *src         = ref->data[0];
    ptrdiff_t srcstride  = ref->linesize[0];
    int pic_width        = s->ps.sps->width;
    int pic_height       = s->ps.sps->height;
    int mx               = mv->x & 3;
    int my               = mv->y & 3;
    int weight_flag      = (s->sh.slice_type == HEVC_SLICE_P && s->ps.pps->weighted_pred_flag) ||
                           (s->sh.slice_type == HEVC_SLICE_B && s->ps.pps->weighted_bipred_flag);
    int idx              = ff_hevc_pel_weight[block_w];

    x_off += mv->x >> 2;
    y_off += mv->y >> 2;
    src   += y_off * srcstride + (x_off << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);

    if (x_off < QPEL_EXTRA_BEFORE || y_off < QPEL_EXTRA_AFTER ||
        x_off >= pic_width - block_w - QPEL_EXTRA_AFTER ||
        y_off >= pic_height - block_h - QPEL_EXTRA_AFTER) {
        const int edge_emu_stride = EDGE_EMU_BUFFER_STRIDE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA];
        int offset     = QPEL_EXTRA_BEFORE * srcstride       + (QPEL_EXTRA_BEFORE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);
        int buf_offset = QPEL_EXTRA_BEFORE * edge_emu_stride + (QPEL_EXTRA_BEFORE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);

        s->vdsp.emulated_edge_mc(lc->edge_emu_buffer, src - offset,
                                 edge_emu_stride, srcstride,
                                 block_w + QPEL_EXTRA,
                                 block_h + QPEL_EXTRA,
                                 x_off - QPEL_EXTRA_BEFORE, y_off - QPEL_EXTRA_BEFORE,
                                 pic_width, pic_height);
        src = lc->edge_emu_buffer + buf_offset;
        srcstride = edge_emu_stride;
    }

    if (!weight_flag)
        s->hevcdsp.put_hevc_qpel_uni[idx][!!my][!!mx](dst, dststride, src, srcstride,
                                                      block_h, mx, my, block_w);
    else
        s->hevcdsp.put_hevc_qpel_uni_w[idx][!!my][!!mx](dst, dststride, src, srcstride,
                                                        block_h, s->sh.luma_log2_weight_denom,
                                                        luma_weight, luma_offset, mx, my, block_w);
}

/**
 * 8.5.3.2.2.1 Luma sample bidirectional interpolation process
 *
 * @param s HEVC decoding context
 * @param dst target buffer for block data at block position
 * @param dststride stride of the dst buffer
 * @param ref0 reference picture0 buffer at origin (0, 0)
 * @param mv0 motion vector0 (relative to block position) to get pixel data from
 * @param x_off horizontal position of block from origin (0, 0)
 * @param y_off vertical position of block from origin (0, 0)
 * @param block_w width of block
 * @param block_h height of block
 * @param ref1 reference picture1 buffer at origin (0, 0)
 * @param mv1 motion vector1 (relative to block position) to get pixel data from
 * @param current_mv current motion vector structure
 */
 static void luma_mc_bi(HEVCContext *s, uint8_t *dst, ptrdiff_t dststride,
                       AVFrame *ref0, const Mv *mv0, int x_off, int y_off,
                       int block_w, int block_h, AVFrame *ref1, const Mv *mv1, struct MvField *current_mv)
{
    HEVCLocalContext *lc = s->HEVClc;
    ptrdiff_t src0stride  = ref0->linesize[0];
    ptrdiff_t src1stride  = ref1->linesize[0];
    int pic_width        = s->ps.sps->width;
    int pic_height       = s->ps.sps->height;
    int mx0              = mv0->x & 3;
    int my0              = mv0->y & 3;
    int mx1              = mv1->x & 3;
    int my1              = mv1->y & 3;
    int weight_flag      = (s->sh.slice_type == HEVC_SLICE_P && s->ps.pps->weighted_pred_flag) ||
                           (s->sh.slice_type == HEVC_SLICE_B && s->ps.pps->weighted_bipred_flag);
    int x_off0           = x_off + (mv0->x >> 2);
    int y_off0           = y_off + (mv0->y >> 2);
    int x_off1           = x_off + (mv1->x >> 2);
    int y_off1           = y_off + (mv1->y >> 2);
    int idx              = ff_hevc_pel_weight[block_w];

    uint8_t *src0  = ref0->data[0] + y_off0 * src0stride + (int)((unsigned)x_off0 << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);
    uint8_t *src1  = ref1->data[0] + y_off1 * src1stride + (int)((unsigned)x_off1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);

    if (x_off0 < QPEL_EXTRA_BEFORE || y_off0 < QPEL_EXTRA_AFTER ||
        x_off0 >= pic_width - block_w - QPEL_EXTRA_AFTER ||
        y_off0 >= pic_height - block_h - QPEL_EXTRA_AFTER) {
        const ptrdiff_t edge_emu_stride = EDGE_EMU_BUFFER_STRIDE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA];
        int offset     = QPEL_EXTRA_BEFORE * src0stride       + (QPEL_EXTRA_BEFORE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);
        int buf_offset = QPEL_EXTRA_BEFORE * edge_emu_stride + (QPEL_EXTRA_BEFORE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);

        s->vdsp.emulated_edge_mc(lc->edge_emu_buffer, src0 - offset,
                                 edge_emu_stride, src0stride,
                                 block_w + QPEL_EXTRA,
                                 block_h + QPEL_EXTRA,
                                 x_off0 - QPEL_EXTRA_BEFORE, y_off0 - QPEL_EXTRA_BEFORE,
                                 pic_width, pic_height);
        src0 = lc->edge_emu_buffer + buf_offset;
        src0stride = edge_emu_stride;
    }

    if (x_off1 < QPEL_EXTRA_BEFORE || y_off1 < QPEL_EXTRA_AFTER ||
        x_off1 >= pic_width - block_w - QPEL_EXTRA_AFTER ||
        y_off1 >= pic_height - block_h - QPEL_EXTRA_AFTER) {
        const ptrdiff_t edge_emu_stride = EDGE_EMU_BUFFER_STRIDE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA];
        int offset     = QPEL_EXTRA_BEFORE * src1stride       + (QPEL_EXTRA_BEFORE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);
        int buf_offset = QPEL_EXTRA_BEFORE * edge_emu_stride + (QPEL_EXTRA_BEFORE << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA]);

        s->vdsp.emulated_edge_mc(lc->edge_emu_buffer2, src1 - offset,
                                 edge_emu_stride, src1stride,
                                 block_w + QPEL_EXTRA,
                                 block_h + QPEL_EXTRA,
                                 x_off1 - QPEL_EXTRA_BEFORE, y_off1 - QPEL_EXTRA_BEFORE,
                                 pic_width, pic_height);
        src1 = lc->edge_emu_buffer2 + buf_offset;
        src1stride = edge_emu_stride;
    }

    s->hevcdsp.put_hevc_qpel[idx][!!my0][!!mx0](lc->tmp, src0, src0stride,
                                                block_h, mx0, my0, block_w);
    if (!weight_flag)
        s->hevcdsp.put_hevc_qpel_bi[idx][!!my1][!!mx1](dst, dststride, src1, src1stride, lc->tmp,
                                                       block_h, mx1, my1, block_w);
    else
        s->hevcdsp.put_hevc_qpel_bi_w[idx][!!my1][!!mx1](dst, dststride, src1, src1stride, lc->tmp,
                                                         block_h, s->sh.luma_log2_weight_denom,
                                                         s->sh.luma_weight_l0[current_mv->ref_idx[0]],
                                                         s->sh.luma_weight_l1[current_mv->ref_idx[1]],
                                                         s->sh.luma_offset_l0[current_mv->ref_idx[0]],
                                                         s->sh.luma_offset_l1[current_mv->ref_idx[1]],
                                                         mx1, my1, block_w);

}

/**
 * 8.5.3.2.2.2 Chroma sample uniprediction interpolation process
 *
 * @param s HEVC decoding context
 * @param dst1 target buffer for block data at block position (U plane)
 * @param dst2 target buffer for block data at block position (V plane)
 * @param dststride stride of the dst1 and dst2 buffers
 * @param ref reference picture buffer at origin (0, 0)
 * @param mv motion vector (relative to block position) to get pixel data from
 * @param x_off horizontal position of block from origin (0, 0)
 * @param y_off vertical position of block from origin (0, 0)
 * @param block_w width of block
 * @param block_h height of block
 * @param chroma_weight weighting factor applied to the chroma prediction
 * @param chroma_offset additive offset applied to the chroma prediction value
 */

static void chroma_mc_uni(HEVCContext *s, uint8_t *dst0,
                          ptrdiff_t dststride, uint8_t *src0, ptrdiff_t srcstride, int reflist,
                          int x_off, int y_off, int block_w, int block_h, struct MvField *current_mv, int chroma_weight, int chroma_offset)
{
    HEVCLocalContext *lc = s->HEVClc;
    int pic_width        = s->ps.sps->width >> s->ps.sps->hshift[1];
    int pic_height       = s->ps.sps->height >> s->ps.sps->vshift[1];
    const Mv *mv         = &current_mv->mv[reflist];
    int weight_flag      = (s->sh.slice_type == HEVC_SLICE_P && s->ps.pps->weighted_pred_flag) ||
                           (s->sh.slice_type == HEVC_SLICE_B && s->ps.pps->weighted_bipred_flag);
    int idx              = ff_hevc_pel_weight[block_w];
    int hshift           = s->ps.sps->hshift[1];
    int vshift           = s->ps.sps->vshift[1];
    intptr_t mx          = av_mod_uintp2(mv->x, 2 + hshift);
    intptr_t my          = av_mod_uintp2(mv->y, 2 + vshift);
    intptr_t _mx         = mx << (1 - hshift);
    intptr_t _my         = my << (1 - vshift);

    x_off += mv->x >> (2 + hshift);
    y_off += mv->y >> (2 + vshift);
    src0  += y_off * srcstride + (x_off << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]);

    if (x_off < EPEL_EXTRA_BEFORE || y_off < EPEL_EXTRA_AFTER ||
        x_off >= pic_width - block_w - EPEL_EXTRA_AFTER ||
        y_off >= pic_height - block_h - EPEL_EXTRA_AFTER) {
        const int edge_emu_stride = EDGE_EMU_BUFFER_STRIDE << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA];
        int offset0 = EPEL_EXTRA_BEFORE * (srcstride + (1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]));
        int buf_offset0 = EPEL_EXTRA_BEFORE *
                          (edge_emu_stride + (1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]));
        s->vdsp.emulated_edge_mc(lc->edge_emu_buffer, src0 - offset0,
                                 edge_emu_stride, srcstride,
                                 block_w + EPEL_EXTRA, block_h + EPEL_EXTRA,
                                 x_off - EPEL_EXTRA_BEFORE,
                                 y_off - EPEL_EXTRA_BEFORE,
                                 pic_width, pic_height);

        src0 = lc->edge_emu_buffer + buf_offset0;
        srcstride = edge_emu_stride;
    }
    if (!weight_flag)
        s->hevcdsp.put_hevc_epel_uni[idx][!!my][!!mx](dst0, dststride, src0, srcstride,
                                                  block_h, _mx, _my, block_w);
    else
        s->hevcdsp.put_hevc_epel_uni_w[idx][!!my][!!mx](dst0, dststride, src0, srcstride,
                                                        block_h, s->sh.chroma_log2_weight_denom,
                                                        chroma_weight, chroma_offset, _mx, _my, block_w);
}

/**
 * 8.5.3.2.2.2 Chroma sample bidirectional interpolation process
 *
 * @param s HEVC decoding context
 * @param dst target buffer for block data at block position
 * @param dststride stride of the dst buffer
 * @param ref0 reference picture0 buffer at origin (0, 0)
 * @param mv0 motion vector0 (relative to block position) to get pixel data from
 * @param x_off horizontal position of block from origin (0, 0)
 * @param y_off vertical position of block from origin (0, 0)
 * @param block_w width of block
 * @param block_h height of block
 * @param ref1 reference picture1 buffer at origin (0, 0)
 * @param mv1 motion vector1 (relative to block position) to get pixel data from
 * @param current_mv current motion vector structure
 * @param cidx chroma component(cb, cr)
 */
static void chroma_mc_bi(HEVCContext *s, uint8_t *dst0, ptrdiff_t dststride, AVFrame *ref0, AVFrame *ref1,
                         int x_off, int y_off, int block_w, int block_h, struct MvField *current_mv, int cidx)
{
    HEVCLocalContext *lc = s->HEVClc;
    uint8_t *src1        = ref0->data[cidx+1];
    uint8_t *src2        = ref1->data[cidx+1];
    ptrdiff_t src1stride = ref0->linesize[cidx+1];
    ptrdiff_t src2stride = ref1->linesize[cidx+1];
    int weight_flag      = (s->sh.slice_type == HEVC_SLICE_P && s->ps.pps->weighted_pred_flag) ||
                           (s->sh.slice_type == HEVC_SLICE_B && s->ps.pps->weighted_bipred_flag);
    int pic_width        = s->ps.sps->width >> s->ps.sps->hshift[1];
    int pic_height       = s->ps.sps->height >> s->ps.sps->vshift[1];
    Mv *mv0              = &current_mv->mv[0];
    Mv *mv1              = &current_mv->mv[1];
    int hshift = s->ps.sps->hshift[1];
    int vshift = s->ps.sps->vshift[1];

    intptr_t mx0 = av_mod_uintp2(mv0->x, 2 + hshift);
    intptr_t my0 = av_mod_uintp2(mv0->y, 2 + vshift);
    intptr_t mx1 = av_mod_uintp2(mv1->x, 2 + hshift);
    intptr_t my1 = av_mod_uintp2(mv1->y, 2 + vshift);
    intptr_t _mx0 = mx0 << (1 - hshift);
    intptr_t _my0 = my0 << (1 - vshift);
    intptr_t _mx1 = mx1 << (1 - hshift);
    intptr_t _my1 = my1 << (1 - vshift);

    int x_off0 = x_off + (mv0->x >> (2 + hshift));
    int y_off0 = y_off + (mv0->y >> (2 + vshift));
    int x_off1 = x_off + (mv1->x >> (2 + hshift));
    int y_off1 = y_off + (mv1->y >> (2 + vshift));
    int idx = ff_hevc_pel_weight[block_w];
    src1  += y_off0 * src1stride + (int)((unsigned)x_off0 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]);
    src2  += y_off1 * src2stride + (int)((unsigned)x_off1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]);

    if (x_off0 < EPEL_EXTRA_BEFORE || y_off0 < EPEL_EXTRA_AFTER ||
        x_off0 >= pic_width - block_w - EPEL_EXTRA_AFTER ||
        y_off0 >= pic_height - block_h - EPEL_EXTRA_AFTER) {
        const int edge_emu_stride = EDGE_EMU_BUFFER_STRIDE << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA];
        int offset1 = EPEL_EXTRA_BEFORE * (src1stride + (1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]));
        int buf_offset1 = EPEL_EXTRA_BEFORE *
                          (edge_emu_stride + (1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]));

        s->vdsp.emulated_edge_mc(lc->edge_emu_buffer, src1 - offset1,
                                 edge_emu_stride, src1stride,
                                 block_w + EPEL_EXTRA, block_h + EPEL_EXTRA,
                                 x_off0 - EPEL_EXTRA_BEFORE,
                                 y_off0 - EPEL_EXTRA_BEFORE,
                                 pic_width, pic_height);

        src1 = lc->edge_emu_buffer + buf_offset1;
        src1stride = edge_emu_stride;
    }

    if (x_off1 < EPEL_EXTRA_BEFORE || y_off1 < EPEL_EXTRA_AFTER ||
        x_off1 >= pic_width - block_w - EPEL_EXTRA_AFTER ||
        y_off1 >= pic_height - block_h - EPEL_EXTRA_AFTER) {
        const int edge_emu_stride = EDGE_EMU_BUFFER_STRIDE << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA];
        int offset1 = EPEL_EXTRA_BEFORE * (src2stride + (1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]));
        int buf_offset1 = EPEL_EXTRA_BEFORE *
                          (edge_emu_stride + (1 << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA]));

        s->vdsp.emulated_edge_mc(lc->edge_emu_buffer2, src2 - offset1,
                                 edge_emu_stride, src2stride,
                                 block_w + EPEL_EXTRA, block_h + EPEL_EXTRA,
                                 x_off1 - EPEL_EXTRA_BEFORE,
                                 y_off1 - EPEL_EXTRA_BEFORE,
                                 pic_width, pic_height);

        src2 = lc->edge_emu_buffer2 + buf_offset1;
        src2stride = edge_emu_stride;
    }

    s->hevcdsp.put_hevc_epel[idx][!!my0][!!mx0](lc->tmp, src1, src1stride,
                                                block_h, _mx0, _my0, block_w);
    if (!weight_flag)
        s->hevcdsp.put_hevc_epel_bi[idx][!!my1][!!mx1](dst0, s->frame->linesize[cidx+1],
                                                       src2, src2stride, lc->tmp,
                                                       block_h, _mx1, _my1, block_w);
    else
        s->hevcdsp.put_hevc_epel_bi_w[idx][!!my1][!!mx1](dst0, s->frame->linesize[cidx+1],
                                                         src2, src2stride, lc->tmp,
                                                         block_h,
                                                         s->sh.chroma_log2_weight_denom,
                                                         s->sh.chroma_weight_l0[current_mv->ref_idx[0]][cidx],
                                                         s->sh.chroma_weight_l1[current_mv->ref_idx[1]][cidx],
                                                         s->sh.chroma_offset_l0[current_mv->ref_idx[0]][cidx],
                                                         s->sh.chroma_offset_l1[current_mv->ref_idx[1]][cidx],
                                                         _mx1, _my1, block_w);
}

static void hevc_await_progress(HEVCContext *s, HEVCFrame *ref,
                                const Mv *mv, int y0, int height)
{
    int y = FFMAX(0, (mv->y >> 2) + y0 + height + 9);

    if (s->threads_type & FF_THREAD_FRAME )
        ff_thread_await_progress(&ref->tf, y, 0);
}
static void hevc_await_progress_bl(HEVCContext *s, HEVCFrame *ref,
                                const Mv *mv, int y0)
{
    int y = (mv->y >> 2) + y0 + (1<<s->ps.sps->log2_ctb_size)*2 + 9;
    int bl_y = (( (y  - s->ps.sps->conf_win.top_offset) * s->up_filter_inf.scaleYLum + s->up_filter_inf.addYLum) >> 12) >> 4;
    if (s->threads_type & FF_THREAD_FRAME ){
        ff_thread_await_progress(&((HEVCFrame*)s->BL_frame)->tf, bl_y, 0);//fixme: await progress won't come back if BL is AVC
    }
}

static void hevc_luma_mv_mvp_mode(HEVCContext *s, int x0, int y0, int nPbW,
                                  int nPbH, int log2_cb_size, int part_idx,
                                  int merge_idx, MvField *mv)
{
    HEVCLocalContext *lc = s->HEVClc;
    enum InterPredIdc inter_pred_idc = PRED_L0;
    int mvp_flag;

    ff_hevc_set_neighbour_available(s, x0, y0, nPbW, nPbH);
    mv->pred_flag = 0;
    if (s->sh.slice_type == HEVC_SLICE_B)
        inter_pred_idc = ff_hevc_inter_pred_idc_decode(s, nPbW, nPbH);

    if (inter_pred_idc != PRED_L1) {
        if (s->sh.nb_refs[L0])
            mv->ref_idx[0]= ff_hevc_ref_idx_lx_decode(s, s->sh.nb_refs[L0]);

        mv->pred_flag = PF_L0;
        ff_hevc_hls_mvd_coding(s, x0, y0, 0);
        mvp_flag = ff_hevc_mvp_lx_flag_decode(s);
        ff_hevc_luma_mv_mvp_mode(s, x0, y0, nPbW, nPbH, log2_cb_size,
                                 part_idx, merge_idx, mv, mvp_flag, 0);
        mv->mv[0].x += lc->pu.mvd.x;
        mv->mv[0].y += lc->pu.mvd.y;
    }

    if (inter_pred_idc != PRED_L0) {
        if (s->sh.nb_refs[L1])
            mv->ref_idx[1]= ff_hevc_ref_idx_lx_decode(s, s->sh.nb_refs[L1]);

        if (s->sh.mvd_l1_zero_flag == 1 && inter_pred_idc == PRED_BI) {
            AV_ZERO32(&lc->pu.mvd);
        } else {
            ff_hevc_hls_mvd_coding(s, x0, y0, 1);
        }

        mv->pred_flag += PF_L1;
        mvp_flag = ff_hevc_mvp_lx_flag_decode(s);
        ff_hevc_luma_mv_mvp_mode(s, x0, y0, nPbW, nPbH, log2_cb_size,
                                 part_idx, merge_idx, mv, mvp_flag, 1);
        mv->mv[1].x += lc->pu.mvd.x;
        mv->mv[1].y += lc->pu.mvd.y;
    }
}

static void hls_prediction_unit(HEVCContext *s, int x0, int y0,
                                int nPbW, int nPbH,
                                int log2_cb_size, int partIdx, int idx)
{
#define POS(c_idx, x, y)                                                              \
    &s->frame->data[c_idx][((y) >> s->ps.sps->vshift[c_idx]) * s->frame->linesize[c_idx] + \
                           (((x) >> s->ps.sps->hshift[c_idx]) << s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA])]
    HEVCLocalContext *lc = s->HEVClc;
    int merge_idx = 0;
    struct MvField current_mv = {{{ 0 }}};

    int min_pu_width = s->ps.sps->min_pu_width;

    MvField *tab_mvf = s->ref->tab_mvf;
    RefPicList  *refPicList = s->ref->refPicList[s->slice_idx];
    HEVCFrame *ref0 = NULL, *ref1 = NULL;
    uint8_t *dst0 = POS(0, x0, y0);
    uint8_t *dst1 = POS(1, x0, y0);
    uint8_t *dst2 = POS(2, x0, y0);
    int log2_min_cb_size = s->ps.sps->log2_min_cb_size;
    int min_cb_width     = s->ps.sps->min_cb_width;
    int x_cb             = x0 >> log2_min_cb_size;
    int y_cb             = y0 >> log2_min_cb_size;
    int mvp_flag;
    int x_pu, y_pu;
    int i, j;

    if (SAMPLE_CTB(s->skip_flag, x_cb, y_cb)) {
        if (s->sh.max_num_merge_cand > 1)
            merge_idx = ff_hevc_merge_idx_decode(s);
        else
            merge_idx = 0;

        ff_hevc_luma_mv_merge_mode(s, x0, y0,
                                   1 << log2_cb_size,
                                   1 << log2_cb_size,
                                   log2_cb_size, partIdx,
                                   merge_idx, &current_mv);
    } else { /* MODE_INTER */
        lc->pu.merge_flag = ff_hevc_merge_flag_decode(s);
        if (lc->pu.merge_flag) {
            if (s->sh.max_num_merge_cand > 1)
                merge_idx = ff_hevc_merge_idx_decode(s);
            else
                merge_idx = 0;

            ff_hevc_luma_mv_merge_mode(s, x0, y0, nPbW, nPbH, log2_cb_size,
                                       partIdx, merge_idx, &current_mv);
        } else {
            enum InterPredIdc inter_pred_idc = PRED_L0;
            ff_hevc_set_neighbour_available(s, x0, y0, nPbW, nPbH);
            current_mv.pred_flag = 0;
            if (s->sh.slice_type == HEVC_SLICE_B)
                inter_pred_idc = ff_hevc_inter_pred_idc_decode(s, nPbW, nPbH);

            if (inter_pred_idc != PRED_L1) {
                if (s->sh.nb_refs[L0]) {
                    current_mv.ref_idx[0] = ff_hevc_ref_idx_lx_decode(s, s->sh.nb_refs[L0]);
#ifdef TEST_MV_POC
                    current_mv.poc[0] = refPicList[0].list[current_mv.ref_idx[0]];
#endif
                }
                current_mv.pred_flag = PF_L0;
                ff_hevc_hls_mvd_coding(s, x0, y0, 0);
                mvp_flag = ff_hevc_mvp_lx_flag_decode(s);
                ff_hevc_luma_mv_mvp_mode(s, x0, y0, nPbW, nPbH, log2_cb_size,
                                         partIdx, merge_idx, &current_mv,
                                         mvp_flag, 0);
                current_mv.mv[0].x += lc->pu.mvd.x;
                current_mv.mv[0].y += lc->pu.mvd.y;
            }
            if (inter_pred_idc != PRED_L0) {
                if (s->sh.nb_refs[L1]) {
                    current_mv.ref_idx[1] = ff_hevc_ref_idx_lx_decode(s, s->sh.nb_refs[L1]);
#ifdef TEST_MV_POC
                    current_mv.poc[1] = refPicList[1].list[current_mv.ref_idx[1]];
#endif
                }
                if (s->sh.mvd_l1_zero_flag == 1 && inter_pred_idc == PRED_BI) {
                    lc->pu.mvd.x = 0;
                    lc->pu.mvd.y = 0;
                } else {
                    ff_hevc_hls_mvd_coding(s, x0, y0, 1);
                }

                current_mv.pred_flag += PF_L1;
                mvp_flag = ff_hevc_mvp_lx_flag_decode(s);
                ff_hevc_luma_mv_mvp_mode(s, x0, y0, nPbW, nPbH, log2_cb_size,
                                         partIdx, merge_idx, &current_mv,
                                         mvp_flag, 1);
                current_mv.mv[1].x += lc->pu.mvd.x;
                current_mv.mv[1].y += lc->pu.mvd.y;
            }
        }
    }

    x_pu = x0 >> s->ps.sps->log2_min_pu_size;
    y_pu = y0 >> s->ps.sps->log2_min_pu_size;

    for (j = 0; j < nPbH >> s->ps.sps->log2_min_pu_size; j++)
        for (i = 0; i < nPbW >> s->ps.sps->log2_min_pu_size; i++)
            tab_mvf[(y_pu + j) * min_pu_width + x_pu + i] = current_mv;

    if (current_mv.pred_flag & PF_L0) {
        ref0 = refPicList[0].ref[current_mv.ref_idx[0]];
        if (!ref0)
            return;
#if ACTIVE_PU_UPSAMPLING
        if(ref0 == s->inter_layer_ref) {
            int y = (current_mv.mv[0].y >> 2) + y0;
            int x = (current_mv.mv[0].x >> 2) + x0;
            hevc_await_progress_bl(s, ref0, &current_mv.mv[0], y0);

            ff_upsample_block(s, ref0, x, y, nPbW, nPbH);
        }
#endif
        hevc_await_progress(s, ref0, &current_mv.mv[0], y0, nPbH);
    }
    if (current_mv.pred_flag & PF_L1) {
        ref1 = refPicList[1].ref[current_mv.ref_idx[1]];
        if (!ref1)
            return;
#if ACTIVE_PU_UPSAMPLING
        if(ref1 == s->inter_layer_ref ) {
            int y = (current_mv.mv[1].y >> 2) + y0;
            int x = (current_mv.mv[1].x >> 2) + x0;
            hevc_await_progress_bl(s, ref1, &current_mv.mv[1], y0);

            ff_upsample_block(s, ref1, x, y, nPbW, nPbH);
        }
#endif
        hevc_await_progress(s, ref1, &current_mv.mv[1], y0, nPbH);
    }

    if (current_mv.pred_flag == PF_L0) {
        int x0_c = x0 >> s->ps.sps->hshift[1];
        int y0_c = y0 >> s->ps.sps->vshift[1];
        int nPbW_c = nPbW >> s->ps.sps->hshift[1];
        int nPbH_c = nPbH >> s->ps.sps->vshift[1];

        luma_mc_uni(s, dst0, s->frame->linesize[0], ref0->frame,
                    &current_mv.mv[0], x0, y0, nPbW, nPbH,
                    s->sh.luma_weight_l0[current_mv.ref_idx[0]],
                    s->sh.luma_offset_l0[current_mv.ref_idx[0]]);

        if (s->ps.sps->chroma_format_idc) {
            chroma_mc_uni(s, dst1, s->frame->linesize[1], ref0->frame->data[1], ref0->frame->linesize[1],
                          0, x0_c, y0_c, nPbW_c, nPbH_c, &current_mv,
                          s->sh.chroma_weight_l0[current_mv.ref_idx[0]][0], s->sh.chroma_offset_l0[current_mv.ref_idx[0]][0]);
            chroma_mc_uni(s, dst2, s->frame->linesize[2], ref0->frame->data[2], ref0->frame->linesize[2],
                          0, x0_c, y0_c, nPbW_c, nPbH_c, &current_mv,
                          s->sh.chroma_weight_l0[current_mv.ref_idx[0]][1], s->sh.chroma_offset_l0[current_mv.ref_idx[0]][1]);
        }
    } else if (current_mv.pred_flag == PF_L1) {
        int x0_c = x0 >> s->ps.sps->hshift[1];
        int y0_c = y0 >> s->ps.sps->vshift[1];
        int nPbW_c = nPbW >> s->ps.sps->hshift[1];
        int nPbH_c = nPbH >> s->ps.sps->vshift[1];

        luma_mc_uni(s, dst0, s->frame->linesize[0], ref1->frame,
                    &current_mv.mv[1], x0, y0, nPbW, nPbH,
                    s->sh.luma_weight_l1[current_mv.ref_idx[1]],
                    s->sh.luma_offset_l1[current_mv.ref_idx[1]]);

        if (s->ps.sps->chroma_format_idc) {
            chroma_mc_uni(s, dst1, s->frame->linesize[1], ref1->frame->data[1], ref1->frame->linesize[1],
                          1, x0_c, y0_c, nPbW_c, nPbH_c, &current_mv,
                          s->sh.chroma_weight_l1[current_mv.ref_idx[1]][0], s->sh.chroma_offset_l1[current_mv.ref_idx[1]][0]);

            chroma_mc_uni(s, dst2, s->frame->linesize[2], ref1->frame->data[2], ref1->frame->linesize[2],
                          1, x0_c, y0_c, nPbW_c, nPbH_c, &current_mv,
                          s->sh.chroma_weight_l1[current_mv.ref_idx[1]][1], s->sh.chroma_offset_l1[current_mv.ref_idx[1]][1]);
        }
    } else if (current_mv.pred_flag == PF_BI) {
        int x0_c = x0 >> s->ps.sps->hshift[1];
        int y0_c = y0 >> s->ps.sps->vshift[1];
        int nPbW_c = nPbW >> s->ps.sps->hshift[1];
        int nPbH_c = nPbH >> s->ps.sps->vshift[1];

        luma_mc_bi(s, dst0, s->frame->linesize[0], ref0->frame,
                   &current_mv.mv[0], x0, y0, nPbW, nPbH,
                   ref1->frame, &current_mv.mv[1], &current_mv);

        if (s->ps.sps->chroma_format_idc) {
            chroma_mc_bi(s, dst1, s->frame->linesize[1], ref0->frame, ref1->frame,
                         x0_c, y0_c, nPbW_c, nPbH_c, &current_mv, 0);

            chroma_mc_bi(s, dst2, s->frame->linesize[2], ref0->frame, ref1->frame,
                         x0_c, y0_c, nPbW_c, nPbH_c, &current_mv, 1);
        }
    }
}

#if HEVC_CIPHERING
static INLINE uint8_t intra_mode_encryption(HEVCContext *s, uint8_t intra_pred_mode)
{
    const uint8_t sets[3][17] =
    {
        {  0,  1,  2,  3,  4,  5, 15, 16, 17, 18, 19, 20, 21, 31, 32, 33, 34},  /* 17 */
        { 22, 23, 24, 25, 27, 28, 29, 30, -1, -1, -1, -1, -1, -1, -1, -1, -1},  /* 9  */
        {  6,  7,  8,  9, 11, 12, 13, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1}   /* 9  */
    };

    const uint8_t nb_elems[3] = {17, 8, 8};

    if (intra_pred_mode == 26 || intra_pred_mode == 10) {
    // correct chroma intra prediction mode
        return intra_pred_mode;

    } else {
        uint8_t keybits, scan_dir, elem_idx=0;

        keybits = ff_get_key(&s->HEVClc->dbs_g, 5);

        scan_dir = SCAN_DIA;
        if (intra_pred_mode > 5  && intra_pred_mode < 15) {
            scan_dir = SCAN_VER;
        }
        if (intra_pred_mode > 21 && intra_pred_mode < 31) {
            scan_dir = SCAN_HOR;
        }

        for (int i = 0; i < nb_elems[scan_dir]; i++) {
            if (intra_pred_mode == sets[scan_dir][i]) {
                elem_idx = i;
                break;
            }
        }

        keybits = keybits % nb_elems[scan_dir];
        keybits = (elem_idx + keybits) % nb_elems[scan_dir];

        return sets[scan_dir][keybits];
    }
}
#endif


/**
 * 8.4.1
 */
static int luma_intra_pred_mode(HEVCContext *s, int x0, int y0, int pu_size,
                                uint8_t *prev_intra_luma_pred_flag, uint8_t *dst)
{
    HEVCLocalContext *lc = s->HEVClc;
    int x_pu             = x0 >> s->ps.sps->log2_min_pu_size;
    int y_pu             = y0 >> s->ps.sps->log2_min_pu_size;
    int min_pu_width     = s->ps.sps->min_pu_width;
    int size_in_pus      = pu_size >> s->ps.sps->log2_min_pu_size;
    int x0b              = av_mod_uintp2(x0, s->ps.sps->log2_ctb_size);
    int y0b              = av_mod_uintp2(y0, s->ps.sps->log2_ctb_size);

    int cand_up   = (lc->ctb_up_flag || y0b) ?
                    s->tab_ipm[(y_pu - 1) * min_pu_width + x_pu] : INTRA_DC;
    int cand_left = (lc->ctb_left_flag || x0b) ?
                    s->tab_ipm[y_pu * min_pu_width + x_pu - 1]   : INTRA_DC;

    int y_ctb = (y0 >> (s->ps.sps->log2_ctb_size)) << (s->ps.sps->log2_ctb_size);

    MvField *tab_mvf = s->ref->tab_mvf;
    int intra_pred_mode;
    int candidate[3];
    int i, j;
#if HEVC_ENCRYPTION
   if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_INTRA_PRED_MODE)) {
     cand_up   = (lc->ctb_up_flag || y0b) ?
	                       s->tab_ipm_encry[(y_pu - 1) * min_pu_width + x_pu] : INTRA_DC;
	 cand_left = (lc->ctb_left_flag || x0b) ?
	                       s->tab_ipm_encry[y_pu * min_pu_width + x_pu - 1]   : INTRA_DC;
   }
#endif
    // intra_pred_mode prediction does not cross vertical CTB boundaries
    if ((y0 - 1) < y_ctb)
        cand_up = INTRA_DC;

    if (cand_left == cand_up) {
        if (cand_left < 2) {
            candidate[0] = INTRA_PLANAR;
            candidate[1] = INTRA_DC;
            candidate[2] = INTRA_ANGULAR_26;
        } else {
            candidate[0] = cand_left;
            candidate[1] = 2 + ((cand_left - 2 - 1 + 32) & 31);
            candidate[2] = 2 + ((cand_left - 2 + 1) & 31);
        }
    } else {
        candidate[0] = cand_left;
        candidate[1] = cand_up;
        if (candidate[0] != INTRA_PLANAR && candidate[1] != INTRA_PLANAR) {
            candidate[2] = INTRA_PLANAR;
        } else if (candidate[0] != INTRA_DC && candidate[1] != INTRA_DC) {
            candidate[2] = INTRA_DC;
        } else {
            candidate[2] = INTRA_ANGULAR_26;
        }
    }

    if (*prev_intra_luma_pred_flag) {
        intra_pred_mode = candidate[lc->pu.mpm_idx];
    } else {
        if (candidate[0] > candidate[1])
            FFSWAP(uint8_t, candidate[0], candidate[1]);
        if (candidate[0] > candidate[2])
            FFSWAP(uint8_t, candidate[0], candidate[2]);
        if (candidate[1] > candidate[2])
            FFSWAP(uint8_t, candidate[1], candidate[2]);

        intra_pred_mode = lc->pu.rem_intra_luma_pred_mode;
        for (i = 0; i < 3; i++)
            if (intra_pred_mode >= candidate[i])
                intra_pred_mode++;
    }

    /* write the intra prediction units into the mv array */
    if (!size_in_pus)
        size_in_pus = 1;

#if HEVC_ENCRYPTION
    if( s->tile_table_encry[s->HEVClc->tile_id]  && (s->encrypt_params & HEVC_CRYPTO_INTRA_PRED_MODE)) {
	    if(intra_pred_mode != INTRA_ANGULAR_26 && intra_pred_mode != INTRA_ANGULAR_10) {/* for correct chroma Inra prediction mode */
    
                int Sets[3][17] = { { 0,  1,  2,  3,  4,  5, 15, 16, 17, 18, 19, 20, 21, 31, 32, 33, 34},/* 17 */
                                    { 22, 23, 24, 25, 27, 28, 29, 30, -1, -1, -1, -1, -1, -1, -1, -1, -1},  /* 9 */
                                    {  6,  7,  8,  9, 11, 12, 13, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1} /* 9 */
                                    };
                uint8_t nb_elems[3] = {17, 8, 8};
                uint8_t keybits, Dir, Index=0;
                for (i = 0; i < size_in_pus; i++)
                    memset(&s->tab_ipm_encry[(y_pu + i) * min_pu_width + x_pu], intra_pred_mode, size_in_pus);
                keybits = ff_get_key (&s->HEVClc->dbs_g, 5);
                Dir = SCAN_DIAG;
                if(intra_pred_mode >5 && intra_pred_mode < 15 )
                    Dir = SCAN_VERT;
                if(intra_pred_mode > 21 && intra_pred_mode < 31 )
                    Dir = SCAN_HORIZ;
                Index = 0;
                for(int i = 0; i < nb_elems[Dir]; i++) {
                    if(intra_pred_mode == Sets[Dir][i]) {
                        Index = i;
                        break;
                    }
                }
                keybits = keybits % nb_elems[Dir];
                keybits = ( Index >= keybits ? (Index-keybits) : (nb_elems[Dir] - (keybits-Index)));
                intra_pred_mode = Sets[Dir][keybits];
            } else
                for (i = 0; i < size_in_pus; i++)
                    memset( &s->tab_ipm_encry[(y_pu + i) * min_pu_width + x_pu],intra_pred_mode, size_in_pus);
    }
#endif //HEVC_ENCRYPTION

#if HEVC_CIPHERING 
    int saved_intra_pred_mode = intra_pred_mode;
    if( s->tile_table_encry[s->HEVClc->tile_id]  && (s->ciphering_params & HEVC_CRYPTO_INTRA_PRED_MODE)) {
        intra_pred_mode = intra_mode_encryption(s, intra_pred_mode);
        cand_up = (lc->ctb_up_flag || y0b) ? s->tab_ipm_encry[(y_pu - 1) * min_pu_width + x_pu] : INTRA_DC;
        cand_left = (lc->ctb_left_flag || x0b) ? s->tab_ipm_encry[y_pu * min_pu_width + x_pu - 1] : INTRA_DC;
        for (i = 0; i < size_in_pus; i++)
            memset(&s->tab_ipm_encry[(y_pu + i) * min_pu_width + x_pu], intra_pred_mode, size_in_pus);
    } else {
        cand_up = (lc->ctb_up_flag || y0b) ? s->tab_ipm[(y_pu - 1) * min_pu_width + x_pu] : INTRA_DC;
        cand_left = (lc->ctb_left_flag || x0b) ? s->tab_ipm[y_pu * min_pu_width + x_pu - 1] : INTRA_DC;
    }

    // intra_pred_mode prediction does not cross vertical CTB boundaries
    if ((y0 - 1) < y_ctb)
        cand_up = INTRA_DC;

    if (cand_left == cand_up) {
        if (cand_left < 2) {
            candidate[0] = INTRA_PLANAR;
            candidate[1] = INTRA_DC;
            candidate[2] = INTRA_ANGULAR_26;
        } else {
            candidate[0] = cand_left;
            candidate[1] = 2 + ((cand_left - 2 - 1 + 32) & 31);
            candidate[2] = 2 + ((cand_left - 2 + 1) & 31);
        }
    } else {
        candidate[0] = cand_left;
        candidate[1] = cand_up;
        if (candidate[0] != INTRA_PLANAR && candidate[1] != INTRA_PLANAR) {
            candidate[2] = INTRA_PLANAR;
        } else if (candidate[0] != INTRA_DC && candidate[1] != INTRA_DC) {
            candidate[2] = INTRA_DC;
        } else {
            candidate[2] = INTRA_ANGULAR_26;
        }
    }
    
    int mode_found = 0;
    for(i=0;i<3&&mode_found==0;i++){
        if(intra_pred_mode==candidate[i]){
            mode_found = 1;
            *dst = i;
        }
    }
    if(!mode_found){
        if (candidate[0] > candidate[1])
            FFSWAP(uint8_t, candidate[0], candidate[1]);
        if (candidate[0] > candidate[2])
            FFSWAP(uint8_t, candidate[0], candidate[2]);
        if (candidate[1] > candidate[2])
            FFSWAP(uint8_t, candidate[1], candidate[2]);
        int mode = intra_pred_mode;
        for (i = 2; i >=0; i--)
            if (mode > candidate[i])
                mode--;
        *dst = mode;
    } 
    *prev_intra_luma_pred_flag = mode_found;
    intra_pred_mode = saved_intra_pred_mode;

#endif  //HEVC_CIPHERING 


   for (i = 0; i < size_in_pus; i++) {
     memset(&s->tab_ipm[(y_pu + i) * min_pu_width + x_pu],
               intra_pred_mode, size_in_pus);

     for (j = 0; j < size_in_pus; j++) {
            tab_mvf[(y_pu + j) * min_pu_width + x_pu + i].pred_flag = PF_INTRA;
     }
   }

   return intra_pred_mode;
}

static av_always_inline void set_ct_depth(HEVCContext *s, int x0, int y0,
                                          int log2_cb_size, int ct_depth)
{
    int length = (1 << log2_cb_size) >> s->ps.sps->log2_min_cb_size;
    int x_cb   = x0 >> s->ps.sps->log2_min_cb_size;
    int y_cb   = y0 >> s->ps.sps->log2_min_cb_size;
    int y;

    for (y = 0; y < length; y++)
        memset(&s->tab_ct_depth[(y_cb + y) * s->ps.sps->min_cb_width + x_cb],
               ct_depth, length);
}

static const uint8_t tab_mode_idx[] = {
     0,  1,  2,  2,  2,  2,  3,  5,  7,  8, 10, 12, 13, 15, 17, 18, 19, 20,
    21, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 29, 30, 31};

static void intra_prediction_unit(HEVCContext *s, int x0, int y0,
                                  int log2_cb_size)
{
    HEVCLocalContext *lc = s->HEVClc;
    static const uint8_t intra_chroma_table[4] = { 0, 26, 10, 1 };
    uint8_t prev_intra_luma_pred_flag[4];
    int split   = lc->cu.part_mode == PART_NxN;
    int pb_size = (1 << log2_cb_size) >> split;
    int side    = split + 1;
    int chroma_mode;
    int i, j;
    uint8_t intra_pred_mode_or_mpm_idx[4];  // Contains intra_pred_mode or mpm_idx depending on the related flag

    for (i = 0; i < side; i++)
        for (j = 0; j < side; j++)
            prev_intra_luma_pred_flag[2 * i + j] = ff_hevc_prev_intra_luma_pred_flag_decode(s);

    for (i = 0; i < side; i++) {
        for (j = 0; j < side; j++) {
            if (prev_intra_luma_pred_flag[2 * i + j])
                lc->pu.mpm_idx = ff_hevc_mpm_idx_decode(s);
            else
                lc->pu.rem_intra_luma_pred_mode = ff_hevc_rem_intra_luma_pred_mode_decode(s);

            lc->pu.intra_pred_mode[2 * i + j] =
                luma_intra_pred_mode(s, x0 + pb_size * j, y0 + pb_size * i, pb_size,
                                     &prev_intra_luma_pred_flag[2 * i + j], &intra_pred_mode_or_mpm_idx[2 * i + j]);
        }
    }
#if HEVC_CIPHERING 
    for (i = 0; i < side; i++)
        for (j = 0; j < side; j++)
            ff_hevc_prev_intra_luma_pred_flag_encode(s, prev_intra_luma_pred_flag[2 * i + j]);
    
    for (i = 0; i < side; i++) {
        for (j = 0; j < side; j++) {
            if (prev_intra_luma_pred_flag[2 * i + j])
                ff_hevc_mpm_idx_encode(s, intra_pred_mode_or_mpm_idx[2 * i + j]);
            else
                ff_hevc_rem_intra_luma_pred_mode_encode(s, intra_pred_mode_or_mpm_idx[2 * i + j]);
        }
    }
#endif


    if (s->ps.sps->chroma_format_idc == 3) {
        for (i = 0; i < side; i++) {
            for (j = 0; j < side; j++) {
                lc->pu.chroma_mode_c[2 * i + j] = chroma_mode = ff_hevc_intra_chroma_pred_mode_decode(s);
                if (chroma_mode != 4) {
                    if (lc->pu.intra_pred_mode[2 * i + j] == intra_chroma_table[chroma_mode])
                        lc->pu.intra_pred_mode_c[2 * i + j] = 34;
                    else
                        lc->pu.intra_pred_mode_c[2 * i + j] = intra_chroma_table[chroma_mode];
                } else {
                    lc->pu.intra_pred_mode_c[2 * i + j] = lc->pu.intra_pred_mode[2 * i + j];
                }
            }
        }
    } else if (s->ps.sps->chroma_format_idc == 2) {
        int mode_idx;
        lc->pu.chroma_mode_c[0] = chroma_mode = ff_hevc_intra_chroma_pred_mode_decode(s);
        if (chroma_mode != 4) {
            if (lc->pu.intra_pred_mode[0] == intra_chroma_table[chroma_mode])
                mode_idx = 34;
            else
                mode_idx = intra_chroma_table[chroma_mode];
        } else {
            mode_idx = lc->pu.intra_pred_mode[0];
        }
        lc->pu.intra_pred_mode_c[0] = tab_mode_idx[mode_idx];
    } else if (s->ps.sps->chroma_format_idc != 0) {
        chroma_mode = ff_hevc_intra_chroma_pred_mode_decode(s);
        if (chroma_mode != 4) {
            if (lc->pu.intra_pred_mode[0] == intra_chroma_table[chroma_mode])
                lc->pu.intra_pred_mode_c[0] = 34;
            else
                lc->pu.intra_pred_mode_c[0] = intra_chroma_table[chroma_mode];
        } else {
            lc->pu.intra_pred_mode_c[0] = lc->pu.intra_pred_mode[0];
        }
    }
}

static void intra_prediction_unit_default_value(HEVCContext *s,
                                                int x0, int y0,
                                                int log2_cb_size)
{
    HEVCLocalContext *lc = s->HEVClc;
    int pb_size          = 1 << log2_cb_size;
    int size_in_pus      = pb_size >> s->ps.sps->log2_min_pu_size;
    int min_pu_width     = s->ps.sps->min_pu_width;
    MvField *tab_mvf     = s->ref->tab_mvf;
    int x_pu             = x0 >> s->ps.sps->log2_min_pu_size;
    int y_pu             = y0 >> s->ps.sps->log2_min_pu_size;
    int j, k;

    if (size_in_pus == 0)
        size_in_pus = 1;
    for (j = 0; j < size_in_pus; j++)
        memset(&s->tab_ipm[(y_pu + j) * min_pu_width + x_pu], INTRA_DC, size_in_pus);

#if HEVC_ENCRYPTION
   if(s->tile_table_encry[s->HEVClc->tile_id]  && (s->encrypt_params & HEVC_CRYPTO_INTRA_PRED_MODE)) {
    for (j = 0; j < size_in_pus; j++)
            memset(&s->tab_ipm_encry[(y_pu + j) * min_pu_width + x_pu], INTRA_DC, size_in_pus);
   }
#endif

#if HEVC_CIPHERING
   if(s->tile_table_encry[s->HEVClc->tile_id]  && (s->ciphering_params & HEVC_CRYPTO_INTRA_PRED_MODE)) {
    for (j = 0; j < size_in_pus; j++)
            memset(&s->tab_ipm_encry[(y_pu + j) * min_pu_width + x_pu], INTRA_DC, size_in_pus);
   }
#endif

    if (lc->cu.pred_mode == MODE_INTRA)
        for (j = 0; j < size_in_pus; j++)
            for (k = 0; k < size_in_pus; k++)
                tab_mvf[(y_pu + j) * min_pu_width + x_pu + k].pred_flag = PF_INTRA;
}

static int hls_coding_unit(HEVCContext *s, int x0, int y0, int log2_cb_size)
{
    int cb_size          = 1 << log2_cb_size;
    HEVCLocalContext *lc = s->HEVClc;
    int log2_min_cb_size = s->ps.sps->log2_min_cb_size;
    int length           = cb_size >> log2_min_cb_size;
    int min_cb_width     = s->ps.sps->min_cb_width;
    int x_cb             = x0 >> log2_min_cb_size;
    int y_cb             = y0 >> log2_min_cb_size;
    int idx              = log2_cb_size - 2;
    int qp_block_mask    = (1<<(s->ps.sps->log2_ctb_size - s->ps.pps->diff_cu_qp_delta_depth)) - 1;
    int x, y, ret;

    lc->cu.x                = x0;
    lc->cu.y                = y0;
    lc->cu.rqt_root_cbf     = 1;
    lc->cu.pred_mode        = MODE_INTRA;
    lc->cu.part_mode        = PART_2Nx2N;
    lc->cu.intra_split_flag = 0;
    lc->cu.pcm_flag         = 0;

    SAMPLE_CTB(s->skip_flag, x_cb, y_cb) = 0;
    for (x = 0; x < 4; x++)
        lc->pu.intra_pred_mode[x] = 1;
    if (s->ps.pps->transquant_bypass_enable_flag) {
        lc->cu.cu_transquant_bypass_flag = ff_hevc_cu_transquant_bypass_flag_decode(s);
        if (lc->cu.cu_transquant_bypass_flag)
            set_deblocking_bypass(s, x0, y0, log2_cb_size);
    } else
        lc->cu.cu_transquant_bypass_flag = 0;

    if (s->sh.slice_type != HEVC_SLICE_I) {
        uint8_t skip_flag = ff_hevc_skip_flag_decode(s, x0, y0, x_cb, y_cb);

        x = y_cb * min_cb_width + x_cb;
        for (y = 0; y < length; y++) {
            memset(&s->skip_flag[x], skip_flag, length);
            x += min_cb_width;
        }
        lc->cu.pred_mode = skip_flag ? MODE_SKIP : MODE_INTER;
    } else {
        x = y_cb * min_cb_width + x_cb;
        for (y = 0; y < length; y++) {
            memset(&s->skip_flag[x], 0, length);
            x += min_cb_width;
        }
    }

    if (SAMPLE_CTB(s->skip_flag, x_cb, y_cb)) {
        hls_prediction_unit(s, x0, y0, cb_size, cb_size, log2_cb_size, 0, idx);
        intra_prediction_unit_default_value(s, x0, y0, log2_cb_size);

        if (!s->sh.disable_deblocking_filter_flag)
            ff_hevc_deblocking_boundary_strengths(s, x0, y0, log2_cb_size);
    } else {
        if (s->sh.slice_type != HEVC_SLICE_I)
            lc->cu.pred_mode = ff_hevc_pred_mode_decode(s);
        if (lc->cu.pred_mode != MODE_INTRA ||
            log2_cb_size == s->ps.sps->log2_min_cb_size) {
            lc->cu.part_mode        = ff_hevc_part_mode_decode(s, log2_cb_size);
            lc->cu.intra_split_flag = lc->cu.part_mode == PART_NxN &&
                                      lc->cu.pred_mode == MODE_INTRA;
        }

        if (lc->cu.pred_mode == MODE_INTRA) {
            if (lc->cu.part_mode == PART_2Nx2N && s->ps.sps->pcm_enabled_flag &&
                log2_cb_size >= s->ps.sps->pcm.log2_min_pcm_cb_size &&
                log2_cb_size <= s->ps.sps->pcm.log2_max_pcm_cb_size) {
                lc->cu.pcm_flag = ff_hevc_pcm_flag_decode(s);
            }
            if (lc->cu.pcm_flag) {
                intra_prediction_unit_default_value(s, x0, y0, log2_cb_size);
                ret = hls_pcm_sample(s, x0, y0, log2_cb_size);
                if (s->ps.sps->pcm.loop_filter_disable_flag)
                    set_deblocking_bypass(s, x0, y0, log2_cb_size);

                if (ret < 0)
                    return ret;
            } else {
                intra_prediction_unit(s, x0, y0, log2_cb_size);
            }
        } else {
            intra_prediction_unit_default_value(s, x0, y0, log2_cb_size);
            switch (lc->cu.part_mode) {
            case PART_2Nx2N:
                hls_prediction_unit(s, x0, y0, cb_size, cb_size, log2_cb_size, 0, idx);
                break;
            case PART_2NxN:
                hls_prediction_unit(s, x0, y0,               cb_size, cb_size / 2, log2_cb_size, 0, idx);
                hls_prediction_unit(s, x0, y0 + cb_size / 2, cb_size, cb_size / 2, log2_cb_size, 1, idx);
                break;
            case PART_Nx2N:
                hls_prediction_unit(s, x0,               y0, cb_size / 2, cb_size, log2_cb_size, 0, idx - 1);
                hls_prediction_unit(s, x0 + cb_size / 2, y0, cb_size / 2, cb_size, log2_cb_size, 1, idx - 1);
                break;
            case PART_2NxnU:
                hls_prediction_unit(s, x0, y0,               cb_size, cb_size     / 4, log2_cb_size, 0, idx);
                hls_prediction_unit(s, x0, y0 + cb_size / 4, cb_size, cb_size * 3 / 4, log2_cb_size, 1, idx);
                break;
            case PART_2NxnD:
                hls_prediction_unit(s, x0, y0,                   cb_size, cb_size * 3 / 4, log2_cb_size, 0, idx);
                hls_prediction_unit(s, x0, y0 + cb_size * 3 / 4, cb_size, cb_size     / 4, log2_cb_size, 1, idx);
                break;
            case PART_nLx2N:
                hls_prediction_unit(s, x0,               y0, cb_size     / 4, cb_size, log2_cb_size, 0, idx - 2);
                hls_prediction_unit(s, x0 + cb_size / 4, y0, cb_size * 3 / 4, cb_size, log2_cb_size, 1, idx - 2);
                break;
            case PART_nRx2N:
                hls_prediction_unit(s, x0,                   y0, cb_size * 3 / 4, cb_size, log2_cb_size, 0, idx - 2);
                hls_prediction_unit(s, x0 + cb_size * 3 / 4, y0, cb_size     / 4, cb_size, log2_cb_size, 1, idx - 2);
                break;
            case PART_NxN:
                hls_prediction_unit(s, x0,               y0,               cb_size / 2, cb_size / 2, log2_cb_size, 0, idx - 1);
                hls_prediction_unit(s, x0 + cb_size / 2, y0,               cb_size / 2, cb_size / 2, log2_cb_size, 1, idx - 1);
                hls_prediction_unit(s, x0,               y0 + cb_size / 2, cb_size / 2, cb_size / 2, log2_cb_size, 2, idx - 1);
                hls_prediction_unit(s, x0 + cb_size / 2, y0 + cb_size / 2, cb_size / 2, cb_size / 2, log2_cb_size, 3, idx - 1);
                break;
            }
        }

        if (!lc->cu.pcm_flag) {
            if (lc->cu.pred_mode != MODE_INTRA &&
                !(lc->cu.part_mode == PART_2Nx2N && lc->pu.merge_flag)) {
                lc->cu.rqt_root_cbf = ff_hevc_no_residual_syntax_flag_decode(s);
            }
            if (lc->cu.rqt_root_cbf) {
                const static int cbf[2] = { 0 };
                lc->cu.max_trafo_depth = lc->cu.pred_mode == MODE_INTRA ?
                                         s->ps.sps->max_transform_hierarchy_depth_intra + lc->cu.intra_split_flag :
                                         s->ps.sps->max_transform_hierarchy_depth_inter;
                ret = hls_transform_tree(s, x0, y0, x0, y0, x0, y0,
                                         log2_cb_size,
                                         log2_cb_size, 0, 0, cbf, cbf);
                if (ret < 0)
                    return ret;
            } else {
                if (!s->sh.disable_deblocking_filter_flag)
                    ff_hevc_deblocking_boundary_strengths(s, x0, y0, log2_cb_size);
            }
        }
    }

    if (s->ps.pps->cu_qp_delta_enabled_flag && lc->tu.is_cu_qp_delta_coded == 0)
        ff_hevc_set_qPy(s, x0, y0, log2_cb_size);

    x = y_cb * min_cb_width + x_cb;
    for (y = 0; y < length; y++) {
        memset(&s->qp_y_tab[x], lc->qp_y, length);
        x += min_cb_width;
    }

    if(((x0 + (1<<log2_cb_size)) & qp_block_mask) == 0 &&
       ((y0 + (1<<log2_cb_size)) & qp_block_mask) == 0) {
        lc->qPy_pred = lc->qp_y;
    }

    set_ct_depth(s, x0, y0, log2_cb_size, lc->ct_depth);

    return 0;
}

static int hls_coding_quadtree(HEVCContext *s, int x0, int y0,
                               int log2_cb_size, int cb_depth)
{
    HEVCLocalContext *lc = s->HEVClc;
    const int cb_size    = 1 << log2_cb_size;
    int ret;
    int split_cu;

    lc->ct_depth = cb_depth;
    if (x0 + cb_size <= s->ps.sps->width  &&
        y0 + cb_size <= s->ps.sps->height &&
        log2_cb_size > s->ps.sps->log2_min_cb_size) {
        split_cu = ff_hevc_split_coding_unit_flag_decode(s, cb_depth, x0, y0);
    } else {
        split_cu = (log2_cb_size > s->ps.sps->log2_min_cb_size);
    }
    if (s->ps.pps->cu_qp_delta_enabled_flag &&
        log2_cb_size >= s->ps.sps->log2_ctb_size - s->ps.pps->diff_cu_qp_delta_depth) {
        lc->tu.is_cu_qp_delta_coded = 0;
        lc->tu.cu_qp_delta          = 0;
    }

    if (s->sh.cu_chroma_qp_offset_enabled_flag &&
        log2_cb_size >= s->ps.sps->log2_ctb_size - s->ps.pps->diff_cu_chroma_qp_offset_depth) {
        lc->tu.is_cu_chroma_qp_offset_coded = 0;
    }

    if (split_cu) {
        int qp_block_mask = (1<<(s->ps.sps->log2_ctb_size - s->ps.pps->diff_cu_qp_delta_depth)) - 1;
        const int cb_size_split = cb_size >> 1;
        const int x1 = x0 + cb_size_split;
        const int y1 = y0 + cb_size_split;

        int more_data = 0;

        more_data = hls_coding_quadtree(s, x0, y0, log2_cb_size - 1, cb_depth + 1);
        if (more_data < 0)
            return more_data;

        if (more_data && x1 < s->ps.sps->width) {
            more_data = hls_coding_quadtree(s, x1, y0, log2_cb_size - 1, cb_depth + 1);
            if (more_data < 0)
                return more_data;
        }
        if (more_data && y1 < s->ps.sps->height) {
            more_data = hls_coding_quadtree(s, x0, y1, log2_cb_size - 1, cb_depth + 1);
            if (more_data < 0)
                return more_data;
        }
        if (more_data && x1 < s->ps.sps->width &&
            y1 < s->ps.sps->height) {
            more_data = hls_coding_quadtree(s, x1, y1, log2_cb_size - 1, cb_depth + 1);
            if (more_data < 0)
                return more_data;
        }

        if(((x0 + (1<<log2_cb_size)) & qp_block_mask) == 0 &&
            ((y0 + (1<<log2_cb_size)) & qp_block_mask) == 0)
            lc->qPy_pred = lc->qp_y;

        if (more_data)
            return ((x1 + cb_size_split) < s->ps.sps->width ||
                    (y1 + cb_size_split) < s->ps.sps->height);
        else
            return 0;
    } else {
        ret = hls_coding_unit(s, x0, y0, log2_cb_size);
        if (ret < 0)
            return ret;
        if ((!((x0 + cb_size) %
               (1 << (s->ps.sps->log2_ctb_size))) ||
             (x0 + cb_size >= s->ps.sps->width)) &&
            (!((y0 + cb_size) %
               (1 << (s->ps.sps->log2_ctb_size))) ||
             (y0 + cb_size >= s->ps.sps->height))) {
            int end_of_slice_flag = ff_hevc_end_of_slice_flag_decode(s);
            return !end_of_slice_flag;
        } else {
            return 1;
        }
    }

    return 0;
}

static void hls_decode_neighbour(HEVCContext *s, int x_ctb, int y_ctb,
                                 int ctb_addr_ts)
{
    HEVCLocalContext *lc  = s->HEVClc;
    int ctb_size          = 1 << s->ps.sps->log2_ctb_size;
    int ctb_addr_rs       = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];
    int ctb_addr_in_slice = ctb_addr_rs - s->sh.slice_addr;

    s->tab_slice_address[ctb_addr_rs] = s->sh.slice_addr;

    if (s->ps.pps->entropy_coding_sync_enabled_flag) {
        if (x_ctb == 0 && (y_ctb & (ctb_size - 1)) == 0)
            lc->first_qp_group = 1;
        lc->end_of_tiles_x = s->ps.sps->width;
    } else if (s->ps.pps->tiles_enabled_flag) {
        if (ctb_addr_ts && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[ctb_addr_ts - 1]) {
            int idxX = s->ps.pps->col_idxX[x_ctb >> s->ps.sps->log2_ctb_size];
            lc->end_of_tiles_x   = x_ctb + (s->ps.pps->column_width[idxX] << s->ps.sps->log2_ctb_size);
            lc->first_qp_group   = 1;
        }
    } else {
        lc->end_of_tiles_x = s->ps.sps->width;
    }

    lc->end_of_tiles_y = FFMIN(y_ctb + ctb_size, s->ps.sps->height);

    lc->boundary_flags = 0;
    if (s->ps.pps->tiles_enabled_flag) {
        if (x_ctb > 0 && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs - 1]])
            lc->boundary_flags |= BOUNDARY_LEFT_TILE;
        if (x_ctb > 0 && s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs - 1])
            lc->boundary_flags |= BOUNDARY_LEFT_SLICE;
        if (y_ctb > 0 && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs - s->ps.sps->ctb_width]])
            lc->boundary_flags |= BOUNDARY_UPPER_TILE;
        if (y_ctb > 0 && s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs - s->ps.sps->ctb_width])
            lc->boundary_flags |= BOUNDARY_UPPER_SLICE;
    } else {
        if (ctb_addr_in_slice <= 0)
            lc->boundary_flags |= BOUNDARY_LEFT_SLICE;
        if (ctb_addr_in_slice < s->ps.sps->ctb_width)
            lc->boundary_flags |= BOUNDARY_UPPER_SLICE;
    }

    lc->ctb_left_flag = ((x_ctb > 0) && (ctb_addr_in_slice > 0) && !(lc->boundary_flags & BOUNDARY_LEFT_TILE));
    lc->ctb_up_flag   = ((y_ctb > 0) && (ctb_addr_in_slice >= s->ps.sps->ctb_width) && !(lc->boundary_flags & BOUNDARY_UPPER_TILE));
    lc->ctb_up_right_flag = ((y_ctb > 0)  && (ctb_addr_in_slice+1 >= s->ps.sps->ctb_width) && (s->ps.pps->tile_id[ctb_addr_ts] == s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs+1 - s->ps.sps->ctb_width]]));
    lc->ctb_up_left_flag = ((x_ctb > 0) && (y_ctb > 0)  && (ctb_addr_in_slice-1 >= s->ps.sps->ctb_width) && (s->ps.pps->tile_id[ctb_addr_ts] == s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs-1 - s->ps.sps->ctb_width]]));
}

static int hls_decode_entry(AVCodecContext *avctxt, void *isFilterThread)
{
    HEVCContext *s  = avctxt->priv_data;
    int ctb_size    = 1 << s->ps.sps->log2_ctb_size;
    int more_data   = 1;
    int x_ctb       = 0;
    int y_ctb       = 0;
    int ctb_addr_ts = s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_ctb_addr_rs];
    int ret;

    if (!ctb_addr_ts && s->sh.dependent_slice_segment_flag) {
        av_log(s->avctx, AV_LOG_ERROR, "Impossible initial tile.\n");
        return AVERROR_INVALIDDATA;
    }

    if (s->sh.dependent_slice_segment_flag) {
        int prev_rs = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts - 1];
        if (s->tab_slice_address[prev_rs] != s->sh.slice_addr) {
            av_log(s->avctx, AV_LOG_ERROR, "Previous slice segment missing\n");
            return AVERROR_INVALIDDATA;
        }
    }

    while (more_data && ctb_addr_ts < s->ps.sps->ctb_size) {
        int ctb_addr_rs = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        s->HEVClc->tile_id = s->ps.pps->tile_id[ctb_addr_ts];

        x_ctb = FFUMOD(ctb_addr_rs, s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
        y_ctb = FFUDIV(ctb_addr_rs, s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
        hls_decode_neighbour(s, x_ctb, y_ctb, ctb_addr_ts);

        ret = ff_hevc_cabac_init(s, ctb_addr_ts);
        if (ret < 0) {
            s->tab_slice_address[ctb_addr_rs] = -1;
            return ret;
        }

        hls_sao_param(s, x_ctb >> s->ps.sps->log2_ctb_size, y_ctb >> s->ps.sps->log2_ctb_size);

        s->deblock[ctb_addr_rs].beta_offset = s->sh.slice_beta_offset;
        s->deblock[ctb_addr_rs].tc_offset   = s->sh.slice_tc_offset;
        s->filter_slice_edges[ctb_addr_rs]  = s->sh.slice_loop_filter_across_slices_enabled_flag;

        more_data = hls_coding_quadtree(s, x_ctb, y_ctb, s->ps.sps->log2_ctb_size, 0);
        if (more_data < 0) {
            s->tab_slice_address[ctb_addr_rs] = -1;
            return more_data;
        }


        ctb_addr_ts++;
        s->HEVClc->ctb_tile_rs++;
        ff_hevc_save_states(s, ctb_addr_ts);
#if HEVC_CIPHERING
        cabac_save_encoder_states(s, ctb_addr_ts);
#endif
        ff_hevc_hls_filters(s, x_ctb, y_ctb, ctb_size);
    }

#if HEVC_CIPHERING 
    HEVCLocalContext *lc = s->HEVClc;
    cabac_data_t *const cabac = &lc->ccc;
    kvz_cabac_finish(cabac);
    kvz_bitstream_add_rbsp_trailing_bits(cabac->stream);
#endif

    if (x_ctb + ctb_size >= s->ps.sps->width &&
        y_ctb + ctb_size >= s->ps.sps->height)
        ff_hevc_hls_filter(s, x_ctb, y_ctb, ctb_size);

    return ctb_addr_ts;
}


#if PARALLEL_SLICE
static int hls_decode_entry_slice(HEVCContext *s)
{
    int ctb_size    = 1 << s->ps.sps->log2_ctb_size;
    int more_data   = 1;
    int x_ctb       = 0;
    int y_ctb       = 0;
    int ctb_addr_ts = s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_ctb_addr_rs];

    if (!ctb_addr_ts && s->sh.dependent_slice_segment_flag) {
        av_log(s->avctx, AV_LOG_ERROR, "Impossible initial tile.\n");
        return AVERROR_INVALIDDATA;
    }

    if (s->sh.dependent_slice_segment_flag) {
        int prev_rs = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts - 1];
        if (s->tab_slice_address[prev_rs] != s->sh.slice_addr) {
            av_log(s->avctx, AV_LOG_ERROR, "Previous slice segment missing\n");
            return AVERROR_INVALIDDATA;
        }
    }

    while (more_data) {
        int ctb_addr_rs = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];

        x_ctb = FFUMOD(ctb_addr_rs, s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
        y_ctb = FFUDIV(ctb_addr_rs, s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
        hls_decode_neighbour(s, x_ctb, y_ctb, ctb_addr_ts);

        ff_hevc_cabac_init(s, ctb_addr_ts);

        hls_sao_param(s, x_ctb >> s->ps.sps->log2_ctb_size, y_ctb >> s->ps.sps->log2_ctb_size);

        s->deblock[ctb_addr_rs].beta_offset = s->sh.beta_offset;
        s->deblock[ctb_addr_rs].tc_offset   = s->sh.tc_offset;
        s->filter_slice_edges[ctb_addr_rs]  = s->sh.slice_loop_filter_across_slices_enabled_flag;
        more_data = hls_coding_quadtree(s, x_ctb, y_ctb, s->ps.sps->log2_ctb_size, 0);

        if (more_data < 0) {
            s->tab_slice_address[ctb_addr_rs] = -1;
            return more_data;
        }
        ctb_addr_ts++;
        ff_hevc_save_states(s, ctb_addr_ts);
#if HEVC_CIPHERING
        cabac_save_encoder_states(s, ctb_addr_ts);
#endif
#if PARALLEL_FILTERS
        ff_hevc_hls_filters_slice(s, x_ctb, y_ctb, ctb_size);
#endif
    }
    return ctb_addr_ts;
}
#endif

static int hls_decode_entry_wpp(AVCodecContext *avctxt, void *input_ctb_row, int job, int self_id)
{
    HEVCContext *s1  = avctxt->priv_data, *s;
    HEVCLocalContext *lc;
    int ctb_size    = 1<< s1->ps.sps->log2_ctb_size;
    int more_data   = 1;
    int *ctb_row_p    = input_ctb_row;
    int ctb_row = ctb_row_p[job];
    int ctb_addr_rs = s1->sh.slice_ctb_addr_rs + ctb_row * ((s1->ps.sps->width + ctb_size - 1) >> s1->ps.sps->log2_ctb_size);
    int ctb_addr_ts = s1->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs];
    int thread = ctb_row % s1->threads_number;
    int ret;

    s = s1->sList[self_id];
    s->HEVClc->ctb_tile_rs = ctb_addr_rs;
    lc = s->HEVClc;

    if(ctb_row) {
        ret = init_get_bits8(&lc->gb, s->data + s->sh.offset[ctb_row - 1], s->sh.size[ctb_row - 1]);
        if (ret < 0)
            return ret;
        ff_init_cabac_decoder(&lc->cc, s->data + s->sh.offset[(ctb_row)-1], s->sh.size[ctb_row - 1]);
    }

    while(more_data && ctb_addr_ts < s->ps.sps->ctb_size) {
        int x_ctb = (ctb_addr_rs % s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
        int y_ctb = (ctb_addr_rs / s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;

        hls_decode_neighbour(s, x_ctb, y_ctb, ctb_addr_ts);

        ff_thread_await_progress2(s->avctx, ctb_row, thread, SHIFT_CTB_WPP);

        if (atomic_load(&s1->wpp_err)) {
            ff_thread_report_progress2(s->avctx, ctb_row , thread, SHIFT_CTB_WPP);
            return ctb_addr_ts;
        }

        ff_hevc_cabac_init(s, ctb_addr_ts);
        hls_sao_param(s, x_ctb >> s->ps.sps->log2_ctb_size, y_ctb >> s->ps.sps->log2_ctb_size);

        s->deblock[ctb_addr_rs].beta_offset = s->sh.slice_beta_offset;
        s->deblock[ctb_addr_rs].tc_offset   = s->sh.slice_tc_offset;
        s->filter_slice_edges[ctb_addr_rs]  = s->sh.slice_loop_filter_across_slices_enabled_flag;

        more_data = hls_coding_quadtree(s, x_ctb, y_ctb, s->ps.sps->log2_ctb_size, 0);

        if (more_data < 0) {
            s->tab_slice_address[ctb_addr_rs] = -1;
            atomic_store(&s1->wpp_err,  1);
            ff_thread_report_progress2(s->avctx, ctb_row ,thread, SHIFT_CTB_WPP);
            return more_data;
        }

        ctb_addr_ts++;
        s->HEVClc->ctb_tile_rs++;

        ff_hevc_save_states(s, ctb_addr_ts);
#if HEVC_CIPHERING
        cabac_save_encoder_states(s, ctb_addr_ts);
#endif
        ff_thread_report_progress2(s->avctx, ctb_row, thread, 1);
        ff_hevc_hls_filters(s, x_ctb, y_ctb, ctb_size);

        if (!more_data && (x_ctb+ctb_size) < s->ps.sps->width && ctb_row != s->sh.num_entry_point_offsets) {
            atomic_store(&s1->wpp_err, 1);
            ff_thread_report_progress2(s->avctx, ctb_row ,thread, SHIFT_CTB_WPP);
            return 0;
        }

        if ((x_ctb+ctb_size) >= s->ps.sps->width && (y_ctb+ctb_size) >= s->ps.sps->height ) {
            ff_hevc_hls_filter(s, x_ctb, y_ctb, ctb_size);
            ff_thread_report_progress2(s->avctx, ctb_row , thread, SHIFT_CTB_WPP);
            return ctb_addr_ts;
        }
        ctb_addr_rs       = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        x_ctb+=ctb_size;

        if(x_ctb >= s->ps.sps->width) {
            break;
        }
    }
    ff_thread_report_progress2(s->avctx, ctb_row ,thread, SHIFT_CTB_WPP);

    return ctb_addr_ts;
}

static int hls_slice_data_wpp(HEVCContext *s, const H2645NAL *nal)
{
    const uint8_t *data = nal->data;
    int length          = nal->size;
    HEVCLocalContext *lc = s->HEVClc;
    int *ret = av_malloc_array(s->sh.num_entry_point_offsets + 1, sizeof(int));
    int *arg = av_malloc_array(s->sh.num_entry_point_offsets + 1, sizeof(int));
    int64_t offset;
    int64_t startheader, cmpt = 0;
    int i, j, res = 0;

    if (!ret || !arg) {
        av_free(ret);
        av_free(arg);
        return AVERROR(ENOMEM);
    }

    if (s->sh.slice_ctb_addr_rs + s->sh.num_entry_point_offsets * s->ps.sps->ctb_width >= s->ps.sps->ctb_width * s->ps.sps->ctb_height) {
        av_log(s->avctx, AV_LOG_ERROR, "WPP ctb addresses are wrong (%d %d %d %d)\n",
            s->sh.slice_ctb_addr_rs, s->sh.num_entry_point_offsets,
            s->ps.sps->ctb_width, s->ps.sps->ctb_height
        );
        res = AVERROR_INVALIDDATA;
        goto error;
    }

    ff_alloc_entries(s->avctx, s->sh.num_entry_point_offsets + 1);

    if (!s->sList[1]) {
        for (i = 1; i < s->threads_number; i++) {
            s->sList[i] = av_malloc(sizeof(HEVCContext));
            memcpy(s->sList[i], s, sizeof(HEVCContext));
            s->HEVClcList[i] = av_mallocz(sizeof(HEVCLocalContext));
            s->sList[i]->HEVClc = s->HEVClcList[i];
        }
    }

    offset = (lc->gb.index >> 3);

    for (j = 0, cmpt = 0, startheader = offset + s->sh.entry_point_offset[0]; j < nal->skipped_bytes; j++) {
        if (nal->skipped_bytes_pos[j] >= offset && nal->skipped_bytes_pos[j] < startheader) {
            startheader--;
            cmpt++;
        }
    }

    for (i = 1; i < s->sh.num_entry_point_offsets; i++) {
        offset += (s->sh.entry_point_offset[i - 1] - cmpt);
        for (j = 0, cmpt = 0, startheader = offset
             + s->sh.entry_point_offset[i]; j < nal->skipped_bytes; j++) {
            if (nal->skipped_bytes_pos[j] >= offset && nal->skipped_bytes_pos[j] < startheader) {
                startheader--;
                cmpt++;
            }
        }
        s->sh.size[i - 1] = s->sh.entry_point_offset[i] - cmpt;
        s->sh.offset[i - 1] = offset;

    }
    if (s->sh.num_entry_point_offsets != 0) {
        offset += s->sh.entry_point_offset[s->sh.num_entry_point_offsets - 1] - cmpt;
        if (length < offset) {
            av_log(s->avctx, AV_LOG_ERROR, "entry_point_offset table is corrupted\n");
            res = AVERROR_INVALIDDATA;
            goto error;
        }
        s->sh.size[s->sh.num_entry_point_offsets - 1] = length - offset;
        s->sh.offset[s->sh.num_entry_point_offsets - 1] = offset;

    }
    s->data = data;

    for (i = 1; i < s->threads_number; i++) {
        s->sList[i]->HEVClc->first_qp_group = 1;
        s->sList[i]->HEVClc->qp_y = s->sList[0]->HEVClc->qp_y;
        memcpy(s->sList[i], s, sizeof(HEVCContext));
        s->sList[i]->HEVClc = s->HEVClcList[i];
    }

    atomic_store(&s->wpp_err, 0);
    ff_reset_entries(s->avctx);

    for (i = 0; i <= s->sh.num_entry_point_offsets; i++) {
        arg[i] = i;
        ret[i] = 0;
    }

    if (s->ps.pps->entropy_coding_sync_enabled_flag)
        s->avctx->execute2(s->avctx, hls_decode_entry_wpp, arg, ret, s->sh.num_entry_point_offsets + 1);

    for (i = 0; i <= s->sh.num_entry_point_offsets; i++)
        res += ret[i];
error:
    av_free(ret);
    av_free(arg);
    return res;
}

static int hls_decode_entry_tiles(AVCodecContext *avctxt, int *input_ctb_row, int job, int self_id)
{
    HEVCContext *s = avctxt->priv_data;
    HEVCLocalContext *lc;
    int ctb_size    = 1 << s->ps.sps->log2_ctb_size;
    int x_ctb = 0, y_ctb = 0;
    int more_data  = 1;
    int *ctb_row_p  = input_ctb_row;
    int ctb_row     = ctb_row_p[job];
    int tile_id     = s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_ctb_addr_rs]]+ctb_row;
    int ctb_addr_rs = ctb_row == 0 ? s->sh.slice_ctb_addr_rs : s->ps.pps->tile_pos_rs[tile_id];
    int ctb_addr_ts = s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs];
    int ret;

    s = s->sList[self_id];
    s->HEVClc->ctb_tile_rs = ctb_addr_rs;
    lc = s->HEVClc;
    lc->tile_id = s->ps.pps->tile_id[ctb_addr_ts];

    if(ctb_row) {
        ret = init_get_bits8(&lc->gb, s->data + s->sh.offset[ctb_row - 1], s->sh.size[ctb_row - 1]);
        if (ret < 0)
            return ret;
    }
    while (more_data) {

        ctb_addr_rs = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        x_ctb = (ctb_addr_rs % s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
        y_ctb = (ctb_addr_rs / s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;

        hls_decode_neighbour(s, x_ctb, y_ctb, ctb_addr_ts);
        ff_hevc_cabac_init(s, ctb_addr_ts);
        hls_sao_param(s, x_ctb >> s->ps.sps->log2_ctb_size, y_ctb >> s->ps.sps->log2_ctb_size);

        s->deblock[ctb_addr_rs].beta_offset = s->sh.slice_beta_offset;
        s->deblock[ctb_addr_rs].tc_offset   = s->sh.slice_tc_offset;
        s->filter_slice_edges[ctb_addr_rs]  = s->sh.slice_loop_filter_across_slices_enabled_flag;

        more_data = hls_coding_quadtree(s, x_ctb, y_ctb, s->ps.sps->log2_ctb_size, 0);
        if (more_data < 0) {
            s->tab_slice_address[ctb_addr_rs] = -1;
            return more_data;
        }
        ctb_addr_ts++;
        s->HEVClc->ctb_tile_rs++;
        if (x_ctb + ctb_size < s->ps.sps->width || y_ctb + ctb_size < s->ps.sps->height)
            if (s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[ctb_addr_ts-1])
                break;
    }
    return ctb_addr_ts;
}

static int hls_decode_entry_wpp_in_tiles(AVCodecContext *avctxt, int *input_ctb_row, int job, int self_id)
{
    HEVCContext *s1  = avctxt->priv_data, *s;
    HEVCLocalContext *lc;
    int ctb_size    = 1<< s1->ps.sps->log2_ctb_size;
    int more_data   = 1;
    int *ctb_row_p    = input_ctb_row;
    int ctb_row = ctb_row_p[job];

    int ctb_addr_ts = s1->ps.pps->wpp_pos_ts[ctb_row];
    int ctb_addr_rs = s1->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];

    int thread = ctb_row % s1->threads_number;
    int ret;

    s = s1->sList[self_id];
    s->HEVClc->ctb_tile_rs = 0;
    lc = s->HEVClc;

    int tile_id     = s->ps.pps->tile_id[ctb_addr_ts];
    int tile_width  = s->ps.pps->tile_width[ctb_addr_ts];

    if(ctb_row) {
        ret = init_get_bits8(&lc->gb, s->data + s->sh.offset[ctb_row - 1], s->sh.size[ctb_row - 1]);

        if (ret < 0)
            return ret;
        ff_init_cabac_decoder(&lc->cc, s->data + s->sh.offset[(ctb_row)-1], s->sh.size[ctb_row - 1]);
    }

    int index_in_row = 0;
    while(more_data && index_in_row < tile_width) {
        int x_ctb = (ctb_addr_rs % s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
        int y_ctb = (ctb_addr_rs / s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;

        hls_decode_neighbour(s, x_ctb, y_ctb, ctb_addr_ts);

        ff_thread_await_progress2(s->avctx, ctb_row, thread, SHIFT_CTB_WPP);

        if (atomic_load(&s1->wpp_err)){
            ff_thread_report_progress2(s->avctx, ctb_row , thread, SHIFT_CTB_WPP);
            return ctb_addr_ts;
        }

        ff_hevc_cabac_init(s, ctb_addr_ts);
        hls_sao_param(s, x_ctb >> s->ps.sps->log2_ctb_size, y_ctb >> s->ps.sps->log2_ctb_size);

        s->deblock[ctb_addr_rs].beta_offset = s->sh.slice_beta_offset;
        s->deblock[ctb_addr_rs].tc_offset   = s->sh.slice_tc_offset;
        s->filter_slice_edges[ctb_addr_rs]  = s->sh.slice_loop_filter_across_slices_enabled_flag;

        more_data = hls_coding_quadtree(s, x_ctb, y_ctb, s->ps.sps->log2_ctb_size, 0);

        if (more_data < 0) {
            s->tab_slice_address[ctb_addr_rs] = -1;
            atomic_store(&s1->wpp_err,  1);
            ff_thread_report_progress2(s->avctx, ctb_row ,thread, SHIFT_CTB_WPP);
            return more_data;
        }

        ctb_addr_ts++;
        s->HEVClc->ctb_tile_rs++;
        index_in_row++;

        ff_hevc_save_states(s, ctb_addr_ts);
#if HEVC_CIPHERING
        cabac_save_encoder_states(s, ctb_addr_ts);
#endif
        ff_thread_report_progress2(s->avctx, ctb_row, thread, 1);
        ff_hevc_hls_filters(s, x_ctb, y_ctb, ctb_size);

        if (!more_data && (x_ctb+ctb_size) < s->ps.sps->width && ctb_row != s->sh.num_entry_point_offsets) {
            atomic_store(&s1->wpp_err,  1);
            ff_thread_report_progress2(s->avctx, ctb_row ,thread, SHIFT_CTB_WPP);
            return 0;
        }

        if ((x_ctb+ctb_size) >= s->ps.sps->width && (y_ctb+ctb_size) >= s->ps.sps->height ) {
            ff_hevc_hls_filter(s, x_ctb, y_ctb, ctb_size);
            ff_thread_report_progress2(s->avctx, ctb_row , thread, SHIFT_CTB_WPP);
            return ctb_addr_ts;
        }
        ctb_addr_rs       = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        x_ctb+=ctb_size;

        if(x_ctb >= s->ps.sps->width) {
            break;
        }
    }
    ff_thread_report_progress2(s->avctx, ctb_row ,thread, SHIFT_CTB_WPP);

    return ctb_addr_ts;
}

static void tiles_filters(HEVCContext *s)
{
    uint16_t ctb_size        = 1 << s->ps.sps->log2_ctb_size;
    int min_size            = 1 << s->ps.sps->log2_min_tb_size;
    int ctb_addr_rs;
    int x0, y0, i;

    // Deblocking and SAO filters
    if(s->ps.pps->loop_filter_across_tiles_enabled_flag) {
        for (i = 1; i < s->ps.pps->num_tile_columns; i++) {
            int slice_left_boundary;
            ctb_addr_rs = s->ps.pps->tile_pos_rs[i];
            x0 = (ctb_addr_rs % s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
            for (y0 = 0; y0 < s->ps.sps->height; y0+=min_size) {
                ctb_addr_rs = (x0 >> s->ps.sps->log2_ctb_size) + ((y0 >> s->ps.sps->log2_ctb_size) * s->ps.sps->ctb_width);
                slice_left_boundary = ((x0 > 0) &&
                                        (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs - 1]));
                ff_hevc_deblocking_boundary_strengths_v(s, x0, y0, !s->filter_slice_edges[ctb_addr_rs] && slice_left_boundary);
            }
        }
        for (i = 1; i < s->ps.pps->num_tile_rows; i++) {
            int slice_up_boundary;
            ctb_addr_rs = s->ps.pps->tile_pos_rs[i * s->ps.pps->num_tile_columns];
            y0 = (ctb_addr_rs / s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size;
            for (x0 = 0; x0 < s->ps.sps->width; x0+=min_size) {
                ctb_addr_rs = (x0 >> s->ps.sps->log2_ctb_size) + ((y0 >> s->ps.sps->log2_ctb_size) * s->ps.sps->ctb_width);
                slice_up_boundary = ((y0 > 0) &&
                                        (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs - s->ps.sps->ctb_width]));
                ff_hevc_deblocking_boundary_strengths_h(s, x0, y0, !s->filter_slice_edges[ctb_addr_rs] && slice_up_boundary);
            }
        }
    }

    for (y0 = 0; y0 < s->ps.sps->height; y0 += ctb_size)
        for (x0 = 0; x0 < s->ps.sps->width; x0 += ctb_size)
            ff_hevc_hls_filter(s, x0, y0, ctb_size);
}
#if PARALLEL_SLICE && !PARALLEL_FILTERS
static void slices_filters(HEVCContext *s)
{
    uint16_t ctb_size        = 1 << s->ps.sps->log2_ctb_size;
    int x0, y0;
    // Deblocking and SAO filters
    for (y0 = 0; y0 < s->ps.sps->height; y0 += ctb_size)
        for (x0 = 0; x0 < s->ps.sps->width; x0 += ctb_size)
            ff_hevc_hls_filter(s, x0, y0, ctb_size);
}
#endif


static int hls_slice_data(HEVCContext *s, const H2645NAL *nal)
{
    HEVCLocalContext *lc = s->HEVClc;
    int *ret = av_malloc((s->sh.num_entry_point_offsets + 1) * sizeof(int));
    int *arg = av_malloc((s->sh.num_entry_point_offsets + 1) * sizeof(int));
    int offset;
    int startheader, cmpt = 0;
    int i, j, res = 0;



    ff_alloc_entries(s->avctx, s->sh.num_entry_point_offsets + 1);

    if (s->sh.num_entry_point_offsets > 0) {
        offset = (lc->gb.index >> 3);
        for (j = 0, cmpt = 0, startheader = offset + s->sh.entry_point_offset[0]; j < nal->skipped_bytes; j++) {
            if (nal->skipped_bytes_pos[j] >= offset && nal->skipped_bytes_pos[j] < startheader) {
                startheader--;
                cmpt++;
            }
        }

        for (i = 1; i < s->sh.num_entry_point_offsets; i++) {
            offset += (s->sh.entry_point_offset[i - 1] - cmpt);
            for (j = 0, cmpt = 0, startheader = offset
                    + s->sh.entry_point_offset[i]; j < nal->skipped_bytes; j++) {
                if (nal->skipped_bytes_pos[j] >= offset && nal->skipped_bytes_pos[j] < startheader) {
                    startheader--;
                    cmpt++;
                }
            }
            s->sh.size[i - 1] = s->sh.entry_point_offset[i] - cmpt;
            s->sh.offset[i - 1] = offset;
        }
        offset += s->sh.entry_point_offset[s->sh.num_entry_point_offsets - 1] - cmpt;
        s->sh.size[s->sh.num_entry_point_offsets - 1] = nal->size - offset;
        s->sh.offset[s->sh.num_entry_point_offsets - 1] = offset;

        if(s->sh.offset[i - 1]+s->sh.size[i - 1] > nal->size) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "hls_slice_data:  packet length < image size : %d < %d\n",
                   nal->size, s->sh.offset[i - 1]+s->sh.size[i - 1]);
            return AVERROR_INVALIDDATA;
        }

        atomic_store(&s->wpp_err, 0);
        ff_reset_entries(s->avctx);
    }
#if HEVC_ENCRYPTION
    InitC(s->HEVClc->dbs_g, s->encrypt_init_val);
    s->HEVClc->prev_pos = 0;
    s->HEVClc->ciphering_prev_pos = 0;
#endif
    s->data = nal->data;
    if (s->sh.first_slice_in_pic_flag){
        s->HEVClc->ctb_tile_rs = 0;
    }
    for (i = 1; i < s->threads_number; i++) {
        if (s->sh.first_slice_in_pic_flag){
            s->sList[i]->HEVClc->ctb_tile_rs = 0;
        }
        s->sList[i]->HEVClc->first_qp_group = 1;
        s->sList[i]->HEVClc->qp_y = s->sList[0]->HEVClc->qp_y;
        memcpy(s->sList[i], s, sizeof(HEVCContext));
        s->sList[i]->HEVClc = s->HEVClcList[i];
#if HEVC_ENCRYPTION
        InitC(s->sList[i]->HEVClc->dbs_g, s->encrypt_init_val);
        s->sList[i]->HEVClc->prev_pos = 0;
        s->sList[i]->HEVClc->ciphering_prev_pos = 0;
#endif
    }

    atomic_store(&s->wpp_err, 0);
    ff_reset_entries(s->avctx);

    for (i = 0; i <= s->sh.num_entry_point_offsets; i++) {
        arg[i] = i;
        ret[i] = 0;
    }
#if HEVC_ENCRYPTION
    if(s->last_click_pos.den != 0 || s->last_click_pos.num != 0){
        int x,y, tmptile_id= 0;

        if(s->last_click_pos.num < s->ps.sps->width && s->last_click_pos.den < s->ps.sps->height){
            x = (s->last_click_pos.den) >> s->ps.sps->log2_ctb_size;
            y = (s->last_click_pos.num) >> s->ps.sps->log2_ctb_size;
            printf("Click position inside picture boundary, %d,%d\n", x,y);

            tmptile_id = s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[x * s->ps.sps->ctb_width + y]];

            s->tile_table_encry[tmptile_id]= (s->tile_table_encry[tmptile_id] == 0)? 1 : 0;
        } else {
            av_log(s,AV_LOG_ERROR, "Click position outside picture boundary, %d,%d\n", x,y);
            printf("Click position outside picture boundary, %d,%d\n", x,y);
        }
        s->last_click_pos.num = s->last_click_pos.den = 0;
    }
#endif

    if (s->ps.pps->entropy_coding_sync_enabled_flag && s->ps.pps->tiles_enabled_flag && s->threads_number!=1)
        s->avctx->execute2(s->avctx, (void *) hls_decode_entry_wpp_in_tiles  , arg, ret, s->sh.num_entry_point_offsets + 1);
    else if (s->ps.pps->entropy_coding_sync_enabled_flag && s->threads_number!=1)
        s->avctx->execute2(s->avctx, (void *) hls_decode_entry_wpp  , arg, ret, s->sh.num_entry_point_offsets + 1);
    else if (s->ps.pps->tiles_enabled_flag        && s->threads_number!=1)
        s->avctx->execute2(s->avctx, (void *) hls_decode_entry_tiles, arg, ret, s->sh.num_entry_point_offsets + 1);
    else
        s->avctx->execute(s->avctx, hls_decode_entry, arg, ret , 1, sizeof(int));

    res = ret[s->threads_number==1 ? 0:s->sh.num_entry_point_offsets];

    av_free(ret);
    av_free(arg);
    return res;
}


static int set_side_data(HEVCContext *s)
{
    AVFrame *out = s->ref->frame;

    if (s->sei.frame_packing.present &&
        s->sei.frame_packing.arrangement_type >= 3 &&
        s->sei.frame_packing.arrangement_type <= 5 &&
        s->sei.frame_packing.content_interpretation_type > 0 &&
        s->sei.frame_packing.content_interpretation_type < 3) {
        AVStereo3D *stereo = av_stereo3d_create_side_data(out);
        if (!stereo)
            return AVERROR(ENOMEM);

        switch (s->sei.frame_packing.arrangement_type) {
        case 3:
            if (s->sei.frame_packing.quincunx_subsampling)
                stereo->type = AV_STEREO3D_SIDEBYSIDE_QUINCUNX;
            else
                stereo->type = AV_STEREO3D_SIDEBYSIDE;
            break;
        case 4:
            stereo->type = AV_STEREO3D_TOPBOTTOM;
            break;
        case 5:
            stereo->type = AV_STEREO3D_FRAMESEQUENCE;
            break;
        }

        if (s->sei.frame_packing.content_interpretation_type == 2)
            stereo->flags = AV_STEREO3D_FLAG_INVERT;
    }

    if (s->sei.display_orientation.present &&
        (s->sei.display_orientation.anticlockwise_rotation ||
         s->sei.display_orientation.hflip || s->sei.display_orientation.vflip)) {
        double angle = s->sei.display_orientation.anticlockwise_rotation * 360 / (double) (1 << 16);
        AVFrameSideData *rotation = av_frame_new_side_data(out,
                                                           AV_FRAME_DATA_DISPLAYMATRIX,
                                                           sizeof(int32_t) * 9);
        if (!rotation)
            return AVERROR(ENOMEM);

        av_display_rotation_set((int32_t *)rotation->data, angle);
        av_display_matrix_flip((int32_t *)rotation->data,
                               s->sei.display_orientation.hflip,
                               s->sei.display_orientation.vflip);
    }

    // Decrement the mastering display flag when IRAP frame has no_rasl_output_flag=1
    // so the side data persists for the entire coded video sequence.
    if (s->sei.mastering_display.present > 0 &&
        IS_IRAP(s) && s->no_rasl_output_flag) {
        s->sei.mastering_display.present--;
    }
    if (s->sei.mastering_display.present) {
        // HEVC uses a g,b,r ordering, which we convert to a more natural r,g,b
        const int mapping[3] = {2, 0, 1};
        const int chroma_den = 50000;
        const int luma_den = 10000;
        int i;
        AVMasteringDisplayMetadata *metadata =
            av_mastering_display_metadata_create_side_data(out);
        if (!metadata)
            return AVERROR(ENOMEM);

        for (i = 0; i < 3; i++) {
            const int j = mapping[i];
            metadata->display_primaries[i][0].num = s->sei.mastering_display.display_primaries[j][0];
            metadata->display_primaries[i][0].den = chroma_den;
            metadata->display_primaries[i][1].num = s->sei.mastering_display.display_primaries[j][1];
            metadata->display_primaries[i][1].den = chroma_den;
        }
        metadata->white_point[0].num = s->sei.mastering_display.white_point[0];
        metadata->white_point[0].den = chroma_den;
        metadata->white_point[1].num = s->sei.mastering_display.white_point[1];
        metadata->white_point[1].den = chroma_den;

        metadata->max_luminance.num = s->sei.mastering_display.max_luminance;
        metadata->max_luminance.den = luma_den;
        metadata->min_luminance.num = s->sei.mastering_display.min_luminance;
        metadata->min_luminance.den = luma_den;
        metadata->has_luminance = 1;
        metadata->has_primaries = 1;

        av_log(s->avctx, AV_LOG_DEBUG, "Mastering Display Metadata:\n");
        av_log(s->avctx, AV_LOG_DEBUG,
               "r(%5.4f,%5.4f) g(%5.4f,%5.4f) b(%5.4f %5.4f) wp(%5.4f, %5.4f)\n",
               av_q2d(metadata->display_primaries[0][0]),
               av_q2d(metadata->display_primaries[0][1]),
               av_q2d(metadata->display_primaries[1][0]),
               av_q2d(metadata->display_primaries[1][1]),
               av_q2d(metadata->display_primaries[2][0]),
               av_q2d(metadata->display_primaries[2][1]),
               av_q2d(metadata->white_point[0]), av_q2d(metadata->white_point[1]));
        av_log(s->avctx, AV_LOG_DEBUG,
               "min_luminance=%f, max_luminance=%f\n",
               av_q2d(metadata->min_luminance), av_q2d(metadata->max_luminance));
    }

    if (s->sei.a53_caption.a53_caption) {
        AVFrameSideData* sd = av_frame_new_side_data(out,
                                                     AV_FRAME_DATA_A53_CC,
                                                     s->sei.a53_caption.a53_caption_size);
        if (sd)
            memcpy(sd->data, s->sei.a53_caption.a53_caption, s->sei.a53_caption.a53_caption_size);
        av_freep(&s->sei.a53_caption.a53_caption);
        s->sei.a53_caption.a53_caption_size = 0;
        s->avctx->properties |= FF_CODEC_PROPERTY_CLOSED_CAPTIONS;
    }

    if (s->sei.alternative_transfer.present &&
        av_color_transfer_name(s->sei.alternative_transfer.preferred_transfer_characteristics) &&
        s->sei.alternative_transfer.preferred_transfer_characteristics != AVCOL_TRC_UNSPECIFIED) {
        s->avctx->color_trc = s->sei.alternative_transfer.preferred_transfer_characteristics;
    }

    return 0;
}

static int hevc_ref_frame(HEVCContext *s, HEVCFrame *dst, HEVCFrame *src)
{
    int ret;

    ret = ff_thread_ref_frame(&dst->tf, &src->tf);
    if (ret < 0)
        return ret;

    dst->tab_mvf_buf = av_buffer_ref(src->tab_mvf_buf);
    if (!dst->tab_mvf_buf)
        goto fail;
    dst->tab_mvf = src->tab_mvf;

    dst->rpl_tab_buf = av_buffer_ref(src->rpl_tab_buf);
    if (!dst->rpl_tab_buf)
        goto fail;
    dst->rpl_tab = src->rpl_tab;

    dst->rpl_buf = av_buffer_ref(src->rpl_buf);
    if (!dst->rpl_buf)
        goto fail;

    dst->poc        = src->poc;
    dst->ctb_count  = src->ctb_count;
    dst->window     = src->window;
    dst->flags      = src->flags;
    dst->sequence   = src->sequence;
    dst->field_order= src->field_order;

    if (src->hwaccel_picture_private) {
        dst->hwaccel_priv_buf = av_buffer_ref(src->hwaccel_priv_buf);
        if (!dst->hwaccel_priv_buf)
            goto fail;
        dst->hwaccel_picture_private = dst->hwaccel_priv_buf->data;
    }

    return 0;
fail:
    ff_hevc_unref_frame(s, dst, ~0);
    return AVERROR(ENOMEM);
}

static int hevc_frame_start(HEVCContext *s)
{
    HEVCLocalContext *lc = s->HEVClc;
    int pic_size_in_ctb  = ((s->ps.sps->width  >> s->ps.sps->log2_min_cb_size) + 1) *
                           ((s->ps.sps->height >> s->ps.sps->log2_min_cb_size) + 1);
    int ret = 0;
    av_log(s->avctx, AV_LOG_DEBUG, "frame start %d\n", s->decoder_id);


    memset(s->horizontal_bs, 0, s->bs_width * s->bs_height);
    memset(s->vertical_bs,   0, s->bs_width * s->bs_height);
    memset(s->cbf_luma,      0, s->ps.sps->min_tb_width * s->ps.sps->min_tb_height);
    memset(s->is_pcm,        0, (s->ps.sps->min_pu_width + 1) * (s->ps.sps->min_pu_height + 1));
    memset(s->tab_slice_address, -1, pic_size_in_ctb * sizeof(*s->tab_slice_address));
#if PARALLEL_SLICE
    memset(s->decoded_rows, 0,s->ps.sps->ctb_height);
#endif
    s->is_decoded        = 0;
    s->first_nal_type    = s->nal_unit_type;

    s->no_rasl_output_flag = IS_IDR(s) || IS_BLA(s) || (s->nal_unit_type == HEVC_NAL_CRA_NUT && s->last_eos);

    if (s->ps.pps->tiles_enabled_flag)
        lc->end_of_tiles_x = s->ps.pps->column_width[0] << s->ps.sps->log2_ctb_size;
    if (s->nuh_layer_id) {
#if ACTIVE_PU_UPSAMPLING
        if(!s->is_upsampled)
            s->is_upsampled = av_mallocz(s->ps.sps->ctb_width * s->ps.sps->ctb_height);
        else//if(s->is_upsampled)
           memset (s->is_upsampled, 0, s->ps.sps->ctb_width * s->ps.sps->ctb_height);
#endif
       if (s->el_decoder_el_exist){
            ff_thread_await_il_progress(s->avctx, s->poc_id, &s->avctx->BL_frame);
        } else if(s->bl_available && s->ps.vps->vps_nonHEVCBaseLayerFlag && (s->threads_type & FF_THREAD_FRAME )){
            ff_thread_await_il_progress(s->avctx, s->poc_id2, &s->avctx->BL_frame);
        } else
            if(s->threads_type & FF_THREAD_FRAME)
                s->avctx->BL_frame = NULL; // Base Layer does not exist

        if(s->avctx->BL_frame){
                s->BL_frame = s->avctx->BL_frame;
        }else {
            av_log(s->avctx, AV_LOG_ERROR, "Error BL reference frame does not exist. decoder_id %d\n", s->decoder_id);
            goto fail;  // FIXME: add error concealment solution when the base layer frame is missing
        }

        if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
            s->poc = ((HEVCFrame *)s->BL_frame)->poc; //fixme BL AVC poc & no BL frame

        ret = ff_hevc_set_new_iter_layer_ref(s, &s->EL_frame, s->poc);
        if (ret < 0)
            goto fail;
#if !ACTIVE_PU_UPSAMPLING || ACTIVE_BOTH_FRAME_AND_PU

        if(s->ps.pps->colour_mapping_enabled_flag) {
            s->hevcdsp.colorMapping((void*)&s->ps.pps->pc3DAsymLUT, ((HEVCFrame *)s->avctx->BL_frame)->frame, s->Ref_color_mapped_frame);
            s->hevcdsp.upsample_base_layer_frame(s->EL_frame, s->Ref_color_mapped_frame, s->buffer_frame, &s->ps.sps->scaled_ref_layer_window[s->ps.vps->Hevc_VPS_Ext.ref_layer_id[s->nuh_layer_id][0]], &s->up_filter_inf, 1);
        } else if (s->ps.vps->vps_nonHEVCBaseLayerFlag) {
            s->hevcdsp.upsample_base_layer_frame(s->EL_frame, ((H264Picture *)s->BL_frame)->f, s->buffer_frame, &s->ps.sps->scaled_ref_layer_window[s->ps.vps->Hevc_VPS_Ext.ref_layer_id[s->nuh_layer_id][0]], &s->up_filter_inf, 1);
        } else {
            s->hevcdsp.upsample_base_layer_frame(s->EL_frame, ((HEVCFrame *)s->BL_frame)->frame, s->buffer_frame, &s->ps.sps->scaled_ref_layer_window[s->ps.vps->Hevc_VPS_Ext.ref_layer_id[s->nuh_layer_id][0]], &s->up_filter_inf, 1);
        }
#endif
    }

    ret = ff_hevc_set_new_ref(s, &s->frame, s->poc);

    if (ret < 0)
        goto fail;
    s->avctx->BL_frame = s->ref;
    ret = ff_hevc_frame_rps(s);
    if (ret < 0) {
        av_log(s->avctx, AV_LOG_ERROR, "Error constructing the frame RPS. decoder_id %d \n", s->decoder_id);
        goto fail;
    }

    s->ref->frame->key_frame = IS_IRAP(s);

    ret = set_side_data(s);
    if (ret < 0)
        goto fail;

    s->frame->pict_type = 3 - s->sh.slice_type;

    if (!IS_IRAP(s))
        ff_hevc_bump_frame(s);

    av_frame_unref(s->output_frame);
    ret = ff_hevc_output_frame(s, s->output_frame, 0);
    if (ret < 0)
        goto fail;

    if (!s->avctx->hwaccel)
        ff_thread_finish_setup(s->avctx);

    return 0;

fail:
    if (s->ref && (s->threads_type & FF_THREAD_FRAME))
        ff_thread_report_progress(&s->ref->tf, INT_MAX, 0);
    if (s->decoder_id) {
        if(s->el_decoder_el_exist)
            ff_thread_report_il_status(s->avctx, s->poc_id, 2);
#if SVC_EXTENSION
        if(s->bl_available && s->ps.vps->vps_nonHEVCBaseLayerFlag && (s->threads_type & FF_THREAD_FRAME ))
            ff_thread_report_il_status_avc(s->avctx, s->poc_id2, 2);
#endif
        if (s->inter_layer_ref)
            ff_hevc_unref_frame(s, s->inter_layer_ref, ~0);
    }
//??
    //if (s->ref)
    //    ff_hevc_unref_frame(s, s->ref, ~0);
    s->ref = NULL;
    return ret;
}

static int decode_nal_unit(HEVCContext *s, const H2645NAL *nal)
{
    HEVCLocalContext *lc = s->HEVClc;
    GetBitContext *gb    = &lc->gb;
    int ctb_addr_ts, ret;

#if HEVC_CIPHERING 
    cabac_data_t *const cabac = &lc->ccc;
    kvz_bitstream_clear(cabac->stream);
#endif

    *gb              = nal->gb;
    s->nal_unit_type = nal->type;
    s->temporal_id   = nal->temporal_id;
    s->nuh_layer_id = nal->nuh_layer_id;

    if (nal->nuh_layer_id != s->decoder_id && (s->nal_unit_type != HEVC_NAL_VPS && s->nal_unit_type != HEVC_NAL_SPS /*&& s->nal_unit_type != NAL_PPS*/))
        return 0;

    if ((s->temporal_id > s->temporal_layer_id) || (nal->nuh_layer_id > s->quality_layer_id))
        return 0;

#if HEVC_CIPHERING
    kvz_bitstream_writebyte(cabac->stream, 0x00);
    kvz_bitstream_writebyte(cabac->stream, 0x00);
    kvz_bitstream_writebyte(cabac->stream, 0x01);
    
    int i;
    switch(nal->type){
    case HEVC_NAL_VPS:
    case HEVC_NAL_SPS:
    case HEVC_NAL_PPS:
    case HEVC_NAL_SEI_PREFIX:
    case HEVC_NAL_SEI_SUFFIX:
    case HEVC_NAL_AUD:
    case HEVC_NAL_FD_NUT:
        for(i=0;i<nal->size;i++){
            kvz_bitstream_put(cabac->stream, nal->data[i], 8);
        }
        break;
    default:
        kvz_bitstream_put(cabac->stream, 0, 1);
        kvz_bitstream_put(cabac->stream, nal->type, 6);
        kvz_bitstream_put(cabac->stream, nal->nuh_layer_id, 6);
        kvz_bitstream_put(cabac->stream, nal->temporal_id + 1, 3);
    }


#endif

    switch (s->nal_unit_type) {
    case HEVC_NAL_VPS:
        ret = ff_hevc_decode_nal_vps(gb, s->avctx, &s->ps);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_SPS:
        ret = ff_hevc_decode_nal_sps(gb, s->avctx, &s->ps,
                                     s->apply_defdispwin, s->nuh_layer_id);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_PPS:
        ret = ff_hevc_decode_nal_pps(gb, s->avctx, &s->ps);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_SEI_PREFIX:
    case HEVC_NAL_SEI_SUFFIX:
        ret = ff_hevc_decode_nal_sei(gb, s->avctx, &s->sei, &s->ps, s->nal_unit_type);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_TRAIL_R:
    case HEVC_NAL_TRAIL_N:
    case HEVC_NAL_TSA_N:
    case HEVC_NAL_TSA_R:
    case HEVC_NAL_STSA_N:
    case HEVC_NAL_STSA_R:
    case HEVC_NAL_BLA_W_LP:
    case HEVC_NAL_BLA_W_RADL:
    case HEVC_NAL_BLA_N_LP:
    case HEVC_NAL_IDR_W_RADL:
    case HEVC_NAL_IDR_N_LP:
    case HEVC_NAL_CRA_NUT:
    case HEVC_NAL_RADL_N:
    case HEVC_NAL_RADL_R:
    case HEVC_NAL_RASL_N:
    case HEVC_NAL_RASL_R:
        ret = hls_slice_header(s);
#if HEVC_CIPHERING 
        kvz_bitstream_add_rbsp_trailing_bits(cabac->stream);
#endif

        if (ret < 0)
            return ret;

#if HEVC_ENCRYPTION
    if(!s->tile_table_encry){
        s->tile_table_encry = av_mallocz(sizeof(uint8_t)*s->ps.pps->num_tile_columns * s->ps.pps->num_tile_rows);
        s->tile_table_encry[0]=1;
    } else if (s->ps.pps->num_tile_columns != s->prev_num_tile_columns ||
               s->ps.pps->num_tile_rows != s->prev_num_tile_rows){
        if(s->tile_table_encry)
            av_freep(&s->tile_table_encry);
        s->tile_table_encry = av_mallocz(sizeof(uint8_t)*s->ps.pps->num_tile_columns*s->ps.pps->num_tile_rows);
        s->tile_table_encry[0]=1;
    }

    s->prev_num_tile_columns = s->ps.pps->num_tile_columns;
    s->prev_num_tile_rows    = s->ps.pps->num_tile_rows;
#endif
        if(s->au_poc !=-1 && s->au_poc != s->poc) {
            av_log(s->avctx, AV_LOG_ERROR, "Receive different poc in one AU. \n");
            s->max_ra = INT_MAX;
            goto fail;
        }
        s->au_poc = s->poc;
        if (s->max_ra == INT_MAX) {
            if (s->nal_unit_type == HEVC_NAL_CRA_NUT || IS_BLA(s)) {
                s->max_ra = s->poc;
                av_log(s->avctx, AV_LOG_WARNING,
                       "max_ra equal to s->max_ra %d \n", s->max_ra);
            } else {
                if (IS_IDR(s))
                    s->max_ra = INT_MIN;
                else if( s->decoder_id ) {
                    av_log(s->avctx, AV_LOG_WARNING,
                           "Nal type %d s->max_ra %d \n", s->nal_unit_type,  s->max_ra);
                    break;
                }
            }
        }

        if ((s->nal_unit_type == HEVC_NAL_RASL_R || s->nal_unit_type == HEVC_NAL_RASL_N) &&
            s->poc <= s->max_ra) {
            s->is_decoded = 0;
                return 0;
        } else {
            if (s->nal_unit_type == HEVC_NAL_RASL_R && s->poc > s->max_ra)
                s->max_ra = INT_MIN;
        }

        if (s->sh.first_slice_in_pic_flag) {
            ret = hevc_frame_start(s);
            if (ret < 0)
                return ret;
        } else if (!s->ref) {
            av_log(s->avctx, AV_LOG_ERROR, "First slice in a frame missing.\n");
            goto fail;
        }

        if (s->nal_unit_type != s->first_nal_type) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "Non-matching NAL types of the VCL NALUs: %d %d\n",
                   s->first_nal_type, s->nal_unit_type);
            goto fail;
        }

        if (!s->sh.dependent_slice_segment_flag &&
            s->sh.slice_type != HEVC_SLICE_I) {
            ret = ff_hevc_slice_rpl(s);
            if (ret < 0) {
                av_log(s->avctx, AV_LOG_WARNING,
                       "Error constructing the reference lists for the current slice.\n");
                goto fail;
            }
        }

#if ACTIVE_PU_UPSAMPLING
            if (s->bl_decoder_el_exist) {
                int i;
                s->bl_decoder_el_exist = 0;
                for (i = 0; i < FF_ARRAY_ELEMS(s->Add_ref); i++) {
                    HEVCFrame *frame = &s->Add_ref[i];
                    if (frame->frame->buf[0])
                        continue;
                    ret = hevc_ref_frame(s, &s->Add_ref[i], s->ref);
                    if (ret < 0)
                        return ret;
                    ff_thread_report_il_progress(s->avctx, s->poc_id, &s->Add_ref[i], s->ref);
                    break;
                }
                if(i==FF_ARRAY_ELEMS(s->Add_ref))
                    av_log(s->avctx, AV_LOG_ERROR, "Error allocating frame, Addditional DPB full, decoder_%d.\n", s->decoder_id);
            }
#endif
        if (s->sh.first_slice_in_pic_flag && s->avctx->hwaccel) {
            ret = s->avctx->hwaccel->start_frame(s->avctx, NULL, 0);
            if (ret < 0)
                goto fail;
        }

        if (s->avctx->hwaccel) {
            av_log(s->avctx, AV_LOG_WARNING, "Hardware acceleration not handled yet");
            //ret = s->avctx->hwaccel->decode_slice(s->avctx, nal->raw_data, nal->raw_size);
            //if (ret < 0)
                goto fail;
        } else {
            //if (s->threads_number > 1 && s->sh.num_entry_point_offsets > 0)
            //    ctb_addr_ts = hls_slice_data_wpp(s, nal);
            //else
                ctb_addr_ts = hls_slice_data(s, nal);
            if (ctb_addr_ts >= (s->ps.sps->ctb_width * s->ps.sps->ctb_height)) {
                s->is_decoded = 1;
                if (s->ps.pps->tiles_enabled_flag && s->threads_number!=1)
                    tiles_filters(s);
#if !ACTIVE_PU_UPSAMPLING
                if (s->bl_decoder_el_exist) {
                    int i;
                    s->bl_decoder_el_exist = 0;
                    for (i = 0; i < FF_ARRAY_ELEMS(s->Add_ref); i++) {
                        HEVCFrame *frame = &s->Add_ref[i];
                        if (frame->frame->buf[0])
                            continue;
                        ret = hevc_ref_frame(s, &s->Add_ref[i], s->ref);
                        if (ret < 0)
                            return ret;
                        ff_thread_report_il_progress(s->avctx, s->poc_id, &s->Add_ref[i], s->ref);
                        break;
                    }
                    if(i==FF_ARRAY_ELEMS(s->Add_ref))
                        av_log(s->avctx, AV_LOG_ERROR, "Error allocating frame, Addditional DPB full, decoder_%d.\n", s->decoder_id);
                }
#endif
            if(s->decoder_id > 0)
                ff_hevc_unref_frame(s, s->inter_layer_ref, ~0);
        }

            if (ctb_addr_ts < 0) {
                ret = ctb_addr_ts;
                goto fail;
            }
        }
        break;
    case HEVC_NAL_EOS_NUT:
    case HEVC_NAL_EOB_NUT:
        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra     = INT_MAX;
        break;
    case HEVC_NAL_AUD:
    case HEVC_NAL_FD_NUT:
        break;
    default:
        av_log(s->avctx, AV_LOG_INFO,
               "Skipping NAL unit %d\n", s->nal_unit_type);
    }

    return 0;
fail:
    if (s->avctx->err_recognition & AV_EF_EXPLODE)
        return ret;
    return 0;
}


static int decode_nal_units(HEVCContext *s, const uint8_t *buf, int length, uint8_t **output_buffer, int* output_buffer_length)
{
    HEVCLocalContext *lc = s->HEVClc;
    int i, ret = 0;
    int eos_at_start = 1;
    int data_length = length;
    int output_size = 0;

#if HEVC_CIPHERING
    cabac_data_t *const cabac = &lc->ccc;
    lc->ccc.stream = &lc->stream;
    kvz_bitstream_init(lc->ccc.stream);
#endif

#if PARALLEL_SLICE
    int cum_nal_pos = 0, k, nal_type, prv_nal_type=-1;
    int arg[128];
    int is_irap = 1;
#endif
    s->ref = NULL;
    s->au_poc = -1;
    s->last_eos = s->eos;
    s->eos = 0;
    s->bl_decoder_el_exist  = 0;
    s->el_decoder_el_exist  = 0;
    s->el_decoder_bl_exist  = 0;
#if PARALLEL_SLICE
    s->NbListElement        = 0;
    for(i = 0; i < 16; i++)
        s->NALListOrder[i]  = 0;
#endif
    /* split the input packet into NAL units, so we know the upper bound on the
     * number of slices in the frame */

    ret = ff_h2645_packet_split(&s->pkt, buf, length, s->avctx, s->is_nalff,
                                s->nal_length_size, s->avctx->codec_id, 1);

    if (ret < 0) {
        av_log(s->avctx, AV_LOG_ERROR,
               "Error splitting the input into NAL units.\n");
        return ret;
    }

    for (i = 0; i < s->pkt.nb_nals; i++) {
        if (s->pkt.nals[i].type == HEVC_NAL_EOB_NUT ||
            s->pkt.nals[i].type == HEVC_NAL_EOS_NUT) {
            if (eos_at_start) {
                s->last_eos = 1;
            } else {
                s->eos = 1;
            }
        } else {
            eos_at_start = 0;
        }

        if(!s->bl_decoder_el_exist && s->pkt.nals[i].nuh_layer_id == s->decoder_id + 1 && s->avctx->quality_id >= s->pkt.nals[i].nuh_layer_id && s->pkt.nals[i].type <= HEVC_NAL_CRA_NUT && (s->threads_type&FF_THREAD_FRAME)) {
            s->bl_decoder_el_exist = 1;
            s->poc_id++;
            s->poc_id &= (MAX_POC-1);
        }

        if(!s->el_decoder_bl_exist && s->decoder_id  && s->pkt.nals[i].nuh_layer_id == s->decoder_id - 1 && s->pkt.nals[i].type <= HEVC_NAL_CRA_NUT && (s->threads_type&FF_THREAD_FRAME)) {
            s->el_decoder_bl_exist=1;
        }

        if(!s->el_decoder_el_exist && s->decoder_id && s->pkt.nals[i].nuh_layer_id == s->decoder_id && s->pkt.nals[i].type <= HEVC_NAL_CRA_NUT && (s->threads_type&FF_THREAD_FRAME)) {
            s->poc_id++;
            s->poc_id &= (MAX_POC-1);
            s->el_decoder_el_exist = 1;
        }
    }

    /* decode the NAL units */
    for (i = 0; i < s->pkt.nb_nals; i++) {
        H2645NAL *cur_nal = &s->pkt.nals[i];

        ret = decode_nal_unit(s, cur_nal);
        if (ret < 0) {
            av_log(s->avctx, AV_LOG_WARNING,
                   "Error parsing NAL unit #%d.\n", i);
            goto fail;
        }

#if HEVC_CIPHERING 
        int len_out = kvz_bitstream_tell(lc->ccc.stream) / 8;

        uint64_t written = 0;
        kvz_data_chunk *data_out = kvz_bitstream_take_chunks(lc->ccc.stream);
        kvz_data_chunk *to_free = data_out;
        if (output_buffer != NULL)
            while (data_out != NULL)
            {
                int len_chuck = data_out->len;
                assert(written + len_chuck <= len_out);
                if (data_length < *output_buffer_length + len_chuck)
                    *output_buffer = av_realloc(*output_buffer, output_size + len_chuck);
                memcpy(*output_buffer + output_size, data_out->data, len_chuck);
                output_size += len_chuck;

                written += len_chuck;
                data_out = data_out->next;
            }
        kvz_bitstream_free_chunks(to_free);

#endif // HEVC_CIPHERING 
    }

fail:
#if HEVC_CIPHERING
    if (output_buffer_length!=NULL)
        *output_buffer_length = output_size;
    kvz_bitstream_finalize(lc->ccc.stream);
#endif
#if PARALLEL_SLICE
    ff_thread_report_progress_slice(s->avctx);
    ff_thread_report_progress_slice2(s->avctx, s->job);
#endif
    if (s->ref && (s->threads_type & FF_THREAD_FRAME))
        ff_thread_report_progress(&s->ref->tf, INT_MAX, 0);
    if (s->decoder_id) {
        if(s->el_decoder_el_exist)
            ff_thread_report_il_status(s->avctx, s->poc_id, 2);
#if SVC_EXTENSION
        if(s->ps.vps && s->ps.vps->vps_nonHEVCBaseLayerFlag && (s->threads_type & FF_THREAD_FRAME))
            ff_thread_report_il_status_avc(s->avctx, s->poc_id2, 2);
#endif
    }
    if (s->bl_decoder_el_exist)
        ff_thread_report_il_progress(s->avctx, s->poc_id, NULL, NULL);

    return ret;
}

// static int decode_nal_units(HEVCContext *s, uint8_t **data, int *data_length)
// {
//     HEVCLocalContext *lc = s->HEVClc;
//     int i, ret = 0;
//     int eos_at_start = 1;
//     int nb_start_bytes = 0;
//     int skipped_bytes = 0;
    
//     int length = *data_length;  // the initial length of the buffer

//     uint8_t *buf = NULL; // copy of the initial buffer 
//     buf = (uint8_t*)av_malloc(length);
//     if(buf==NULL){
//         fprintf(stderr,"cannot allocate memory for buf\n");
//         exit(1);
//     }
//     buf = memcpy(buf,*data,length);
//     uint8_t *initial_buf = buf; // pointer to the buf

//     uint8_t *packet_buffer = *data; // the new buffer that will replace the initial data
//     int packet_length = 0;  // the length of the new buffer

// #if HEVC_CIPHERING 
//     cabac_data_t *const cabac = &lc->ccc;

//     uint8_t **nal_start_code = NULL;
//     uint8_t *size_start_code = NULL;
//     lc->ccc.stream = &lc->stream;
//     kvz_bitstream_init(lc->ccc.stream);
// #endif

// #if PARALLEL_SLICE
//     int cum_nal_pos = 0, k, nal_type, prv_nal_type = -1;
//     int arg[128];
//     int is_irap = 1;
// #endif
//     s->ref = NULL;
//     s->au_poc = -1;
//     s->last_eos = s->eos;
//     s->eos = 0;
//     s->bl_decoder_el_exist  = 0;
//     s->el_decoder_el_exist  = 0;
//     s->el_decoder_bl_exist  = 0;
// #if PARALLEL_SLICE
//     s->NbListElement        = 0;
//     for(i = 0; i < 16; i++)
//         s->NALListOrder[i]  = 0;
// #endif
//     /* split the input packet into NAL units, so we know the upper bound on the
//      * number of slices in the frame */
//     ret = ff_h2645_packet_split(&s->pkt, buf, length, s->avctx, s->is_nalff,
//                                 s->nal_length_size, s->avctx->codec_id, 1);
//     if (ret < 0) {
//         av_log(s->avctx, AV_LOG_ERROR,
//                "Error splitting the input into NAL units.\n");
//         return ret;
//     }

//     for (i = 0; i < s->pkt.nb_nals; i++) {
//         if (s->pkt.nals[i].type == HEVC_NAL_EOB_NUT ||
//             s->pkt.nals[i].type == HEVC_NAL_EOS_NUT) {
//             if (eos_at_start) {
//                 s->last_eos = 1;
//             } else {
//                 s->eos = 1;
//             }
//         } else {
//             eos_at_start = 0;
//         }

//         if(!s->bl_decoder_el_exist && s->pkt.nals[i].nuh_layer_id == s->decoder_id + 1 && s->avctx->quality_id >= s->pkt.nals[i].nuh_layer_id && s->pkt.nals[i].type <= HEVC_NAL_CRA_NUT && (s->threads_type&FF_THREAD_FRAME)) {
//             s->bl_decoder_el_exist = 1;
//             s->poc_id++;
//             s->poc_id &= (MAX_POC-1);
//         }

//         if(!s->el_decoder_bl_exist && s->decoder_id  && s->pkt.nals[i].nuh_layer_id == s->decoder_id - 1 && s->pkt.nals[i].type <= HEVC_NAL_CRA_NUT && (s->threads_type&FF_THREAD_FRAME)) {
//             s->el_decoder_bl_exist=1;
//         }

//         if(!s->el_decoder_el_exist && s->decoder_id && s->pkt.nals[i].nuh_layer_id == s->decoder_id && s->pkt.nals[i].type <= HEVC_NAL_CRA_NUT && (s->threads_type&FF_THREAD_FRAME)) {
//             s->poc_id++;
//             s->poc_id &= (MAX_POC-1);
//             s->el_decoder_el_exist = 1;
//         }
//     }

//     /* decode the NAL units */
//     for (i = 0; i < s->pkt.nb_nals; i++) {
//         ret = decode_nal_unit(s, &s->pkt.nals[i]);
//         if (ret < 0) {
//             av_log(s->avctx, AV_LOG_WARNING,
//                    "Error parsing NAL unit #%d.\n", i);
//             goto fail;
//         }
//     }

// fail:
// #if PARALLEL_SLICE
//     ff_thread_report_progress_slice(s->avctx);
//     ff_thread_report_progress_slice2(s->avctx, s->job);
// #endif
//     if (s->ref && (s->threads_type & FF_THREAD_FRAME))
//         ff_thread_report_progress(&s->ref->tf, INT_MAX, 0);
//     if (s->decoder_id) {
//         if(s->el_decoder_el_exist)
//             ff_thread_report_il_status(s->avctx, s->poc_id, 2);
// #if SVC_EXTENSION
//         if(s->ps.vps && s->ps.vps->vps_nonHEVCBaseLayerFlag && (s->threads_type & FF_THREAD_FRAME))
//             ff_thread_report_il_status_avc(s->avctx, s->poc_id2, 2);
// #endif
//     }
//     if (s->bl_decoder_el_exist)
//         ff_thread_report_il_progress(s->avctx, s->poc_id, NULL, NULL);

//     return ret;
// }

static void printf_ref_pic_list(void *log_ctx, int level, HEVCContext *s)
{
    RefPicList  *refPicList = s->ref->refPicList[s->slice_idx];

    int i, list_idx;
    if (s->sh.slice_type == HEVC_SLICE_I)
        av_log(log_ctx, level,"POC %4d TId: %1d QId: %1d ( I-SLICE, QP%3d ) ", s->poc, s->temporal_id, s->nuh_layer_id, s->sh.slice_qp);
    else if (s->sh.slice_type == HEVC_SLICE_B)
        av_log(log_ctx, level,"POC %4d TId: %1d QId: %1d ( B-SLICE, QP%3d ) ", s->poc, s->temporal_id, s->nuh_layer_id, s->sh.slice_qp);
    else
        av_log(log_ctx, level,"POC %4d TId: %1d QId: %1d ( P-SLICE, QP%3d ) ", s->poc, s->temporal_id, s->nuh_layer_id,  s->sh.slice_qp);

    for ( list_idx = 0; list_idx < 2; list_idx++) {
        av_log(log_ctx, level,"[L%d ",list_idx);
        if (refPicList)
            for(i = 0; i < refPicList[list_idx].nb_refs; i++)
                av_log(log_ctx, level,"%d ",refPicList[list_idx].list[i]);
        else
            av_log(log_ctx, level,"O");
        av_log(log_ctx, level,"] ");
    }
}

static void print_md5(void *log_ctx, int level, uint8_t md5[16])
{
    int i;
    for (i = 0; i < 16; i++)
        av_log(log_ctx, level, "%02"PRIx8, md5[i]);
}

static int verify_md5(HEVCContext *s, AVFrame *frame)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(frame->format);
    int pixel_shift;
    int i, j;

    if (!desc)
        return AVERROR(EINVAL);

    pixel_shift = desc->comp[0].depth > 8;

    av_log(s->avctx, AV_LOG_DEBUG, "Verifying checksum for frame with POC %d: ",
           s->poc);

    /* the checksums are LE, so we have to byteswap for >8bpp formats
     * on BE arches */
#if HAVE_BIGENDIAN
    if (pixel_shift && !s->checksum_buf) {
        av_fast_malloc(&s->checksum_buf, &s->checksum_buf_size,
                       FFMAX3(frame->linesize[0], frame->linesize[1],
                              frame->linesize[2]));
        if (!s->checksum_buf)
            return AVERROR(ENOMEM);
    }
#endif

    for (i = 0; frame->data[i]; i++) {
        int width  = s->avctx->coded_width;
        int height = s->avctx->coded_height;
        int w = (i == 1 || i == 2) ? (width  >> desc->log2_chroma_w) : width;
        int h = (i == 1 || i == 2) ? (height >> desc->log2_chroma_h) : height;
        uint8_t md5[16];

        av_md5_init(s->sei.picture_hash.md5_ctx);
        for (j = 0; j < h; j++) {
            const uint8_t *src = frame->data[i] + j * frame->linesize[i];
#if HAVE_BIGENDIAN
            if (pixel_shift) {
                s->bdsp.bswap16_buf((uint16_t *) s->checksum_buf,
                                    (const uint16_t *) src, w);
                src = s->checksum_buf;
            }
#endif
            av_md5_update(s->sei.picture_hash.md5_ctx, src, w << pixel_shift);
        }
        av_md5_final(s->sei.picture_hash.md5_ctx, md5);

        if (!memcmp(md5, s->sei.picture_hash.md5[i], 16)) {
            av_log(s->avctx, AV_LOG_INFO, "Correct MD5 (poc: %d, plane: %d)", s->poc, i);
            print_md5(s->avctx, AV_LOG_INFO, md5);
            av_log   (s->avctx, AV_LOG_INFO, "\n ");
        } else {
            av_log(s->avctx, AV_LOG_ERROR, "Incorrect MD5 (poc: %d, plane: %d)\n", s->poc, i);
            print_md5(s->avctx, AV_LOG_ERROR, md5);
            av_log   (s->avctx, AV_LOG_ERROR, "\n ");
            return AVERROR_INVALIDDATA;
        }
    }
    printf_ref_pic_list(s->avctx,AV_LOG_INFO,s);

    av_log(s->avctx, AV_LOG_INFO, "\n");

    return 0;
}


static int hevc_decode_extradata(HEVCContext *s)
{
    AVCodecContext *avctx = s->avctx;
    GetByteContext gb;
    int ret, i;

    bytestream2_init(&gb, avctx->extradata, avctx->extradata_size);

    if (avctx->extradata_size > 3 &&
        (avctx->extradata[0] || avctx->extradata[1] ||
         avctx->extradata[2] > 1)) {
        /* It seems the extradata is encoded as hvcC format.
         * Temporarily, we support configurationVersion==0 until 14496-15 3rd
         * is finalized. When finalized, configurationVersion will be 1 and we
         * can recognize hvcC by checking if avctx->extradata[0]==1 or not. */
        int i, j, num_arrays, nal_len_size;

        s->is_nalff = 1;

        bytestream2_skip(&gb, 21);
        nal_len_size = (bytestream2_get_byte(&gb) & 3) + 1;
        num_arrays   = bytestream2_get_byte(&gb);

        /* nal units in the hvcC always have length coded with 2 bytes,
         * so put a fake nal_length_size = 2 while parsing them */
        s->nal_length_size = 2;

        /* Decode nal units from hvcC. */
        for (i = 0; i < num_arrays; i++) {
            int type = bytestream2_get_byte(&gb) & 0x3f;
            int cnt  = bytestream2_get_be16(&gb);

            for (j = 0; j < cnt; j++) {
                // +2 for the nal size field
                int nalsize = bytestream2_peek_be16(&gb) + 2;
                if (bytestream2_get_bytes_left(&gb) < nalsize) {
                    av_log(s->avctx, AV_LOG_ERROR,
                           "Invalid NAL unit size in extradata.\n");
                    return AVERROR_INVALIDDATA;
                }

                ret = decode_nal_units(s, gb.buffer, nalsize, NULL, NULL);
                if (ret < 0) {
                    av_log(avctx, AV_LOG_ERROR,
                           "Decoding nal unit %d %d from hvcC failed\n",
                           type, i);
                    return ret;
                }
                bytestream2_skip(&gb, nalsize);
            }
        }

        /* Now store right nal length size, that will be used to parse
         * all other nals */
        s->nal_length_size = nal_len_size;
    } else {
        s->is_nalff = 0;

        ret = decode_nal_units(s, avctx->extradata, avctx->extradata_size, NULL, NULL);
        if (ret < 0)
            return ret;
    }

    /* export stream parameters from the first SPS */
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        if (s->ps.sps_list[i]) {
            const HEVCSPS *sps = (const HEVCSPS*)s->ps.sps_list[i]->data;
            export_stream_params(s->avctx, &s->ps, sps);
            break;
        }
    }

    return 0;
}

// static int hevc_decode_extradata(HEVCContext *s)
// {
//     AVCodecContext *avctx = s->avctx;
//     GetByteContext gb;
//     int ret, i;

//     bytestream2_init(&gb, avctx->extradata, avctx->extradata_size);

//     if (avctx->extradata_size > 3 &&
//         (avctx->extradata[0] || avctx->extradata[1] ||
//          avctx->extradata[2] > 1)) {
//         /* It seems the extradata is encoded as hvcC format.
//          * Temporarily, we support configurationVersion==0 until 14496-15 3rd
//          * is finalized. When finalized, configurationVersion will be 1 and we
//          * can recognize hvcC by checking if avctx->extradata[0]==1 or not. */
//         int i, j, num_arrays, nal_len_size;

//         s->is_nalff = 1;

//         bytestream2_skip(&gb, 21);
//         nal_len_size = (bytestream2_get_byte(&gb) & 3) + 1;
//         num_arrays   = bytestream2_get_byte(&gb);

//         /* nal units in the hvcC always have length coded with 2 bytes,
//          * so put a fake nal_length_size = 2 while parsing them */
//         s->nal_length_size = 2;

//         /* Decode nal units from hvcC. */
//         for (i = 0; i < num_arrays; i++) {
//             int type = bytestream2_get_byte(&gb) & 0x3f;
//             int cnt  = bytestream2_get_be16(&gb);

//             for (j = 0; j < cnt; j++) {
//                 // +2 for the nal size field
//                 int nalsize = bytestream2_peek_be16(&gb) + 2;
//                 if (bytestream2_get_bytes_left(&gb) < nalsize) {
//                     av_log(s->avctx, AV_LOG_ERROR,
//                            "Invalid NAL unit size in extradata.\n");
//                     return AVERROR_INVALIDDATA;
//                 }

//                 ret = decode_nal_units(s, (uint8_t**)&(gb.buffer), &nalsize);
//                 if (ret < 0) {
//                     av_log(avctx, AV_LOG_ERROR,
//                            "Decoding nal unit %d %d from hvcC failed\n",
//                            type, i);
//                     return ret;
//                 }
//                 bytestream2_skip(&gb, nalsize);
//             }
//         }

//         /* Now store right nal length size, that will be used to parse
//          * all other nals */
//         s->nal_length_size = nal_len_size;
//     } else {
//         s->is_nalff = 0;
//         ret = decode_nal_units(s, &(avctx->extradata), &(avctx->extradata_size));
//         if (ret < 0)
//             return ret;
//     }

//     /* export stream parameters from the first SPS */
//     for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
//         if (s->ps.sps_list[i]) {
//             const HEVCSPS *sps = (const HEVCSPS*)s->ps.sps_list[i]->data;
//             export_stream_params(s->avctx, &s->ps, sps);
//             break;
//         }
//     }

//     return 0;
// }


static int hevc_decode_frame(AVCodecContext *avctx, void *data, int *got_output,
                             AVPacket *avpkt)
{
    int ret;
    HEVCContext *s = avctx->priv_data;

    s->poc_id2 = avpkt->poc_id;
    s->bl_available = avpkt->bl_available;

    if (!avpkt->size) {
        ret = ff_hevc_output_frame(s, data, 1);
        if (ret < 0)
            return ret;
        if (s->decoder_id) {
            // av_log(s->avctx, AV_LOG_ERROR, "flush poc %d\n", s->poc);
            s->max_ra = INT_MAX;
        }
        *got_output = ret;
        return 0;
    }


    s->ref = NULL;
#if PARALLEL_SLICE
    ff_thread_set_slice_flag(avctx, 0);
    ff_init_flags(avctx);
#endif

    if (avpkt->pts != AV_NOPTS_VALUE) {
      if (! s->last_frame_pts || (s->last_frame_pts != avpkt->pts)) {
          av_log(s->avctx, AV_LOG_DEBUG, "Forcing first_slice_in_pic_flag for pts %ld\n", avpkt->pts);
          s->force_first_slice_in_pic = 1;
      }
      s->last_frame_pts = avpkt->pts;
    }

    s->ref = NULL;
    ret = decode_nal_units(s, avpkt->data, avpkt->size, &avctx->output_buffer, &avctx->output_buffer_size);
    if (ret < 0)
        return ret;

    if (s->decode_checksum_sei && s->is_decoded &&
        s->sei.picture_hash.is_md5) {
    /* verify the SEI checksum */
        ret = verify_md5(s, s->ref->frame);
        if (ret < 0 && avctx->err_recognition & AV_EF_EXPLODE) {
            ff_hevc_unref_frame(s, s->ref, ~0);
            return ret;
        }
    }
    s->sei.picture_hash.is_md5 = 0;

    if (s->is_decoded) {
//FIXME why is key frame set here??
        s->ref->frame->key_frame = IS_IRAP(s);
        av_log(avctx, AV_LOG_DEBUG, "Decoded frame with POC %d.\n", s->poc);
        s->is_decoded = 0;
    }

    if (s->output_frame->buf[0]) {
        av_frame_move_ref(data, s->output_frame);
        *got_output = 1;
    }
    av_log(s->avctx, AV_LOG_DEBUG, "frame end %d\n", s->decoder_id);

    return avpkt->size;
}

// static int hevc_decode_frame(AVCodecContext *avctx, void *data, int *got_output,
//                              AVPacket *avpkt)
// {
//     int ret;
//     HEVCContext *s = avctx->priv_data;

//     s->poc_id2 = avpkt->poc_id;
//     s->bl_available = avpkt->bl_available;

//     if (!avpkt->size) {
//         ret = ff_hevc_output_frame(s, data, 1);
//         if (ret < 0)
//             return ret;
//         if (s->decoder_id) {
//             // av_log(s->avctx, AV_LOG_ERROR, "flush poc %d\n", s->poc);
//             s->max_ra = INT_MAX;
//         }
//         *got_output = ret;
//         return 0;
//     }

//     uint8_t *data_buffer = NULL;
//     int data_buffer_size = avpkt->size;
//     data_buffer = (uint8_t*)av_malloc(avpkt->size);
//     if(data_buffer==NULL){
//         fprintf(stderr,"error of allocation for data_buffer\n");
//         exit(1);
//     }
//     memcpy(data_buffer,avpkt->data,avpkt->size);

//     s->ref = NULL;
// #if PARALLEL_SLICE
//     ff_thread_set_slice_flag(avctx, 0);
//     ff_init_flags(avctx);
// #endif

//     if (avpkt->pts != AV_NOPTS_VALUE) {
//       if (! s->last_frame_pts || (s->last_frame_pts != avpkt->pts)) {
//           av_log(s->avctx, AV_LOG_DEBUG, "Forcing first_slice_in_pic_flag for pts %ld\n", avpkt->pts);
//           s->force_first_slice_in_pic = 1;
//       }
//       s->last_frame_pts = avpkt->pts;
//     }

//     s->ref = NULL;
//     ret    = decode_nal_units(s, &data_buffer, &data_buffer_size);
//     if (ret < 0)
//         return ret;



//     if (s->decode_checksum_sei && s->is_decoded &&
//         s->sei.picture_hash.is_md5) {
//     /* verify the SEI checksum */
//         ret = verify_md5(s, s->ref->frame);
//         if (ret < 0 && avctx->err_recognition & AV_EF_EXPLODE) {
//             ff_hevc_unref_frame(s, s->ref, ~0);
//             return ret;
//         }
//     }
//     s->sei.picture_hash.is_md5 = 0;

//     if (s->is_decoded) {
// //FIXME why is key frame set here??
//         s->ref->frame->key_frame = IS_IRAP(s);
//         av_log(avctx, AV_LOG_DEBUG, "Decoded frame with POC %d.\n", s->poc);
//         s->is_decoded = 0;
//     }

//     if (s->output_frame->buf[0]) {
//         av_frame_move_ref(data, s->output_frame);
//         *got_output = 1;
//     }
//     av_log(s->avctx, AV_LOG_DEBUG, "frame end %d\n", s->decoder_id);

//     return avpkt->size;
// }


static av_cold int hevc_decode_free(AVCodecContext *avctx)
{
    HEVCContext       *s = avctx->priv_data;
    int i;

    pic_arrays_free(s);

    av_freep(&s->sei.picture_hash.md5_ctx);

//    for(i=0; i < s->nals_allocated; i++) {
//        av_freep(&s->skipped_bytes_pos_nal[i]);
//    }

    av_freep(&s->cabac_state);

//    av_freep(&s->skipped_bytes_pos_size_nal);
//    av_freep(&s->skipped_bytes_nal);
//    av_freep(&s->skipped_bytes_pos_nal);
    av_freep(&s->cabac_state);

#if HEVC_CIPHERING
    av_freep(&s->cabac_encoder_states);
#endif

#if !ACTIVE_PU_UPSAMPLING
    av_frame_unref(s->Ref_color_mapped_frame);
    av_frame_free(&s->Ref_color_mapped_frame);
#endif
#ifdef USE_SAO_SMALL_BUFFER
    for (i = 0; i < s->threads_number; i++) {
        av_freep(&s->HEVClcList[i]->sao_pixel_buffer);
    }
    for (i = 0; i < 3; i++) {
        av_freep(&s->sao_pixel_buffer_h[i]);
        av_freep(&s->sao_pixel_buffer_v[i]);
    }
#else
    av_frame_free(&s->tmp_frame);
#endif
    av_frame_free(&s->output_frame);

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        ff_hevc_unref_frame(s, &s->DPB[i], ~0);
        av_frame_free(&s->DPB[i].frame);
    }
    for (i = 0; i < FF_ARRAY_ELEMS(s->Add_ref); i++) {
        ff_hevc_unref_frame(s, &s->Add_ref[i], ~0);
        av_frame_free(&s->Add_ref[i].frame);
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++)
        av_buffer_unref(&s->ps.vps_list[i]);
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++)
        av_buffer_unref(&s->ps.sps_list[i]);
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++){
        if((HEVCPPS*)s->ps.pps_list[i])
          Free3DArray((HEVCPPS*)s->ps.pps_list[i]->data);
        av_buffer_unref(&s->ps.pps_list[i]);
    }

    s->ps.sps = NULL;
    s->ps.pps = NULL;
    s->ps.vps = NULL;

    av_freep(&s->sh.entry_point_offset); // TODO Free for each slice
    av_freep(&s->sh.offset);
    av_freep(&s->sh.size);

    for (i = 1; i < s->threads_number; i++) {
        HEVCLocalContext *lc = s->HEVClcList[i];
        if (lc) {
#if HEVC_ENCRYPTION
    DeleteCryptoC(s->HEVClcList[i]->dbs_g);
#endif
            av_freep(&s->HEVClcList[i]);
            av_freep(&s->sList[i]);
        }
    }
    if (s->HEVClc == s->HEVClcList[0])
        s->HEVClc = NULL;
#if HEVC_ENCRYPTION
    DeleteCryptoC(s->HEVClcList[0]->dbs_g);
    if(s->tile_table_encry)
       av_free(s->tile_table_encry);
#endif
    av_freep(&s->HEVClcList[0]);

    ff_h2645_packet_uninit(&s->pkt);

    return 0;
}

static av_cold int hevc_init_context(AVCodecContext *avctx)
{
    HEVCContext *s = avctx->priv_data;
    int i;

    s->avctx = avctx;

    s->HEVClc = av_mallocz(sizeof(HEVCLocalContext));
    if (!s->HEVClc)
        goto fail;
    s->HEVClcList[0] = s->HEVClc;
    s->sList[0] = s;

    s->cabac_state = av_malloc(HEVC_CONTEXTS);
    if (!s->cabac_state)
        goto fail;

#if HEVC_CIPHERING
    s->cabac_encoder_states = av_malloc(sizeof(s->HEVClc->ccc.ctx));
    if(!s->cabac_encoder_states)
        goto fail;
#endif

#if HEVC_ENCRYPTION
    s->HEVClc->dbs_g = CreateC();
    s->HEVClc->prev_pos = 0;
    s->HEVClc->ciphering_prev_pos = 0;
#endif

#if !ACTIVE_PU_UPSAMPLING
    s->Ref_color_mapped_frame  = av_frame_alloc();
#endif
#ifndef USE_SAO_SMALL_BUFFER
    s->tmp_frame = av_frame_alloc();
    if (!s->tmp_frame)
        goto fail;
#endif

    s->output_frame = av_frame_alloc();
    if (!s->output_frame)
        goto fail;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        s->DPB[i].frame = av_frame_alloc();
        if (!s->DPB[i].frame)
            goto fail;
        s->DPB[i].tf.f = s->DPB[i].frame;
    }
    for (i = 0; i < FF_ARRAY_ELEMS(s->Add_ref); i++) {
        s->Add_ref[i].frame = av_frame_alloc();
        if (!s->Add_ref[i].frame)
            goto fail;
        s->Add_ref[i].tf.f = s->Add_ref[i].frame;
    }

    s->max_ra = INT_MAX;

    s->sei.picture_hash.md5_ctx = av_md5_alloc();
    if (!s->sei.picture_hash.md5_ctx)
        goto fail;

    ff_bswapdsp_init(&s->bdsp);
#if FRAME_CONCEALMENT
    s->prev_display_poc = -1;
    s->no_display_pic   =  0;
#endif

    s->temporal_layer_id   = 8;
    s->quality_layer_id    = 8;

    s->context_initialized = 1;
    s->threads_type        = avctx->active_thread_type;
    if(avctx->active_thread_type == FF_THREAD_SLICE)
        s->threads_number  = avctx->thread_count_frame;
    else if (avctx->active_thread_type & FF_THREAD_SLICE)
        s->threads_number  = avctx->thread_count;
    else
        s->threads_number  = 1;
    s->eos = 0;

    for (i = 1; i < s->threads_number ; i++) {
        s->sList[i] = av_mallocz(sizeof(HEVCContext));
        memcpy(s->sList[i], s, sizeof(HEVCContext));
        s->HEVClcList[i] = av_mallocz(sizeof(HEVCLocalContext));
#if HEVC_ENCRYPTION
        s->HEVClcList[i]->dbs_g = CreateC();
        s->HEVClcList[i]->prev_pos = 0;
        s->HEVClcList[i]->ciphering_prev_pos = 0;
#endif
        s->sList[i]->HEVClc = s->HEVClcList[i];
    }

    ff_hevc_reset_sei(&s->sei);

    return 0;

fail:
    hevc_decode_free(avctx);
    return AVERROR(ENOMEM);
}

static int hevc_update_thread_context(AVCodecContext *dst,
                                      const AVCodecContext *src)
{
    HEVCContext *s  = dst->priv_data;
    HEVCContext *s0 = src->priv_data;
    int i, ret;

    if (!s->context_initialized) {
        ret = hevc_init_context(dst);
        if (ret < 0)
            return ret;
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        ff_hevc_unref_frame(s, &s->DPB[i], ~0);
        if (s0->DPB[i].frame->buf[0] && &s0->DPB[i] != s0->inter_layer_ref ) {
            ret = hevc_ref_frame(s, &s->DPB[i], &s0->DPB[i]);
            if (ret < 0)
                return ret;
        }
    }

    if (s->ps.sps != s0->ps.sps)
        s->ps.sps = NULL;
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++) {
        av_buffer_unref(&s->ps.vps_list[i]);
        if (s0->ps.vps_list[i]) {
            s->ps.vps_list[i] = av_buffer_ref(s0->ps.vps_list[i]);
            if (!s->ps.vps_list[i])
                return AVERROR(ENOMEM);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        av_buffer_unref(&s->ps.sps_list[i]);
        if (s0->ps.sps_list[i]) {
            s->ps.sps_list[i] = av_buffer_ref(s0->ps.sps_list[i]);
            if (!s->ps.sps_list[i])
                return AVERROR(ENOMEM);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++) {
        av_buffer_unref(&s->ps.pps_list[i]);
        if (s0->ps.pps_list[i]) {
            s->ps.pps_list[i] = av_buffer_ref(s0->ps.pps_list[i]);
            if (!s->ps.pps_list[i])
                return AVERROR(ENOMEM);
        }
    }

    if (s->ps.sps != s0->ps.sps)
        if ((ret = set_sps(s, s0->ps.sps, src->pix_fmt)) < 0)
            return ret;

    s->seq_decode = s0->seq_decode;
    s->seq_output = s0->seq_output;
    s->pocTid0    = s0->pocTid0;
    s->max_ra     = s0->max_ra;
    s->eos        = s0->eos;
    s->no_rasl_output_flag = s0->no_rasl_output_flag;

    s->is_nalff        = s0->is_nalff;
    s->nal_length_size = s0->nal_length_size;

    s->threads_number      = s0->threads_number;
    s->threads_type        = s0->threads_type;

    s->nuh_layer_id         = s0->nuh_layer_id;
    s->decoder_id           = s0->decoder_id;
    s->temporal_layer_id    = s0->temporal_layer_id;
    s->quality_layer_id     = s0->quality_layer_id;
    s->decode_checksum_sei  = s0->decode_checksum_sei;
    s->poc_id               = s0->poc_id;
    s->field_order          = s0->field_order;
    s->picture_struct       = s0->picture_struct;
    s->interlaced           = s0->interlaced;

#if HEVC_CIPHERING
    s->ciphering_params = s0->ciphering_params;
#endif
#if HEVC_ENCRYPTION
    s->encrypt_params        = s0->encrypt_params;

    if (s0->prev_num_tile_columns != s->prev_num_tile_columns || s0->prev_num_tile_rows != s->prev_num_tile_rows){
        if(s->tile_table_encry )
            av_freep(s->tile_table_encry);
        s->tile_table_encry = av_mallocz(sizeof(uint8_t)*s0->prev_num_tile_rows * s0->prev_num_tile_columns);
    }

    for(i=0; i < s0->prev_num_tile_rows * s0->prev_num_tile_columns; ++i){
        s->tile_table_encry[i] = s0->tile_table_encry[i];
    }
    s->prev_num_tile_rows    = s0->prev_num_tile_rows;
    s->prev_num_tile_columns = s0->prev_num_tile_columns;
#endif
    s->poc_id2              = s0->poc_id2;
    s->bl_available         = s0->bl_available;

    if (s0->eos) {
        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra = INT_MAX;
    }

    s->sei.frame_packing        = s0->sei.frame_packing;
    s->sei.display_orientation  = s0->sei.display_orientation;
    s->sei.mastering_display    = s0->sei.mastering_display;
    s->sei.content_light        = s0->sei.content_light;
    s->sei.alternative_transfer = s0->sei.alternative_transfer;

    return 0;
}

static av_cold int hevc_decode_init(AVCodecContext *avctx)
{
    HEVCContext *s = avctx->priv_data;
    int ret;

    avctx->internal->allocate_progress = 1;

    ret = hevc_init_context(avctx);
    if (ret < 0)
        return ret;

    s->enable_parallel_tiles = 0;
    s->picture_struct = 0;
    s->sei.picture_timing.picture_struct = 0;
#if HEVC_ENCRYPTION
    //s->encrypt_params =  HEVC_CRYPTO_MV_SIGNS | HEVC_CRYPTO_MVs | HEVC_CRYPTO_TRANSF_COEFF_SIGNS | HEVC_CRYPTO_TRANSF_COEFFS | HEVC_CRYPTO_INTRA_PRED_MODE;
    s->last_click_pos.den = 0;
    s->last_click_pos.num = 0;
    s->tile_table_encry = NULL;
    s->prev_num_tile_columns=0;
    s->prev_num_tile_rows=0;
#endif
    s->eos = 1;

    atomic_init(&s->wpp_err, 0);

//    if(avctx->active_thread_type & FF_THREAD_SLICE)
//        s->threads_number = avctx->thread_count;
//    else
//        s->threads_number = 1;

    if (avctx->extradata_size > 0 && avctx->extradata) {
        ret = hevc_decode_extradata(s);
        if (ret < 0) {
            hevc_decode_free(avctx);
            return ret;
        }
    }

//    if((avctx->active_thread_type & FF_THREAD_FRAME) && avctx->thread_count_frame > 1)
//        s->threads_type = FF_THREAD_FRAME;
//    else
//        s->threads_type = FF_THREAD_SLICE;

    return 0;
}

static av_cold int hevc_init_thread_copy(AVCodecContext *avctx)
{
    HEVCContext *s = avctx->priv_data;
    int ret;

    memset(s, 0, sizeof(*s));

    ret = hevc_init_context(avctx);
    if (ret < 0)
        return ret;

    return 0;
}

static void hevc_decode_flush(AVCodecContext *avctx)
{
    HEVCContext *s = avctx->priv_data;
    ff_hevc_flush_dpb(s);
    s->max_ra = INT_MAX;
    s->eos = 1;
}

#define OFFSET(x) offsetof(HEVCContext, x)
#define PAR (AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM)

#define OFFSET(x) offsetof(HEVCContext, x)
#define PAR (AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM)

static const AVOption options[] = {
    { "apply_defdispwin", "Apply default display window from VUI", OFFSET(apply_defdispwin),
        AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, PAR },
    { "decode-checksum", "decode picture checksum SEI message", OFFSET(decode_checksum_sei),
        AV_OPT_TYPE_INT, {.i64 = 0}, 0, 1, PAR },
    { "strict-displaywin", "stricly apply default display window size", OFFSET(apply_defdispwin),
        AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, PAR },
    { "decoder-id", "set the decoder id", OFFSET(decoder_id),
        AV_OPT_TYPE_INT, {.i64 = 0}, 0, 10, PAR },
    { "temporal-layer-id", "set the max temporal id", OFFSET(temporal_layer_id),
        AV_OPT_TYPE_INT, {.i64 = 0}, 0, 10, PAR },
    { "quality_layer_id", "set the max quality id", OFFSET(quality_layer_id),
        AV_OPT_TYPE_INT, {.i64 = 0}, 0, 10, PAR },
#if HEVC_ENCRYPTION
    { "mouse-click-pos", "select tile from last click position and enable/disable encryption",OFFSET(last_click_pos),
       AV_OPT_TYPE_RATIONAL,{.dbl = 0},0,INT_MAX,PAR },
    { "crypto-param", "",OFFSET(encrypt_params),
       AV_OPT_TYPE_INT,{.i64 = 0},0,32,PAR },
    { "cipher-param", "",OFFSET(ciphering_params),
       AV_OPT_TYPE_INT,{.i64 = 0},0,32,PAR },
    { "crypto-key", "",OFFSET(encrypt_init_val),
       AV_OPT_TYPE_BINARY },
#endif
    { NULL },
};

static const AVClass hevc_decoder_class = {
    .class_name = "HEVC decoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_hevc_decoder = {
    .name                  = "hevc",
    .long_name             = NULL_IF_CONFIG_SMALL("HEVC (High Efficiency Video Coding)"),
    .type                  = AVMEDIA_TYPE_VIDEO,
    .id                    = AV_CODEC_ID_HEVC,
    .priv_data_size        = sizeof(HEVCContext),
    .priv_class            = &hevc_decoder_class,
    .init                  = hevc_decode_init,
    .close                 = hevc_decode_free,
    .decode                = hevc_decode_frame,
    .flush                 = hevc_decode_flush,
    .update_thread_context = hevc_update_thread_context,
    .init_thread_copy      = hevc_init_thread_copy,
    .capabilities          = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_DELAY |
                             AV_CODEC_CAP_SLICE_THREADS | AV_CODEC_CAP_FRAME_THREADS,
    .profiles              = NULL_IF_CONFIG_SMALL(ff_hevc_profiles),
};

static const AVClass shvc_decoder_class = {
    .class_name = "SHVC decoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_shvc_decoder = {
    .name                  = "shvc",
    .long_name             = NULL_IF_CONFIG_SMALL("SHVC (High Efficiency Video Coding)"),
    .type                  = AVMEDIA_TYPE_VIDEO,
    .id                    = AV_CODEC_ID_SHVC,
    .priv_data_size        = sizeof(HEVCContext),
    .priv_class            = &shvc_decoder_class,
    .init                  = hevc_decode_init,
    .close                 = hevc_decode_free,
    .decode                = hevc_decode_frame,
    .flush                 = hevc_decode_flush,
    .update_thread_context = hevc_update_thread_context,
    .init_thread_copy      = hevc_init_thread_copy,
    .capabilities          = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_DELAY |
                             AV_CODEC_CAP_SLICE_THREADS | AV_CODEC_CAP_FRAME_THREADS,
    .profiles              = NULL_IF_CONFIG_SMALL(ff_hevc_profiles),
};
