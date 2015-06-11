/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 Seppo Tomperi
 * Copyright (C) 2013 Wassim Hamidouche
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

#include "libavutil/common.h"
#include "libavutil/internal.h"
#ifdef WIN32
#include <windows.h>
#include <minwindef.h>
#endif // WIN32
#include "cabac_functions.h"
#include "golomb.h"
#include "hevc.h"
#include "bit_depth_template.c"

#if HAVE_SSE2
#include <emmintrin.h>
#endif
#if HAVE_SSSE3
#include <tmmintrin.h>
#endif
#if HAVE_SSE42
#include <smmintrin.h>
#endif

#define LUMA 0
#define CB 1
#define CR 2

static const uint8_t tctable[54] = {
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 1, // QP  0...18
    1, 1, 1, 1, 1, 1, 1,  1,  2,  2,  2,  2,  3,  3,  3,  3, 4, 4, 4, // QP 19...37
    5, 5, 6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 22, 24           // QP 38...53
};

static const uint8_t betatable[52] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  7,  8, // QP 0...18
     9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, // QP 19...37
    38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64                      // QP 38...51
};

static int chroma_tc(HEVCContext *s, int qp_y, int c_idx, int tc_offset)
{
    static const int qp_c[] = {
        29, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37
    };
    int qp, qp_i, offset, idxt;

    // slice qp offset is not used for deblocking
    if (c_idx == 1)
        offset = s->pps->cb_qp_offset;
    else
        offset = s->pps->cr_qp_offset;

    qp_i = av_clip(qp_y + offset, 0, 57);
    if (s->sps->chroma_array_type == 1) {
        if (qp_i < 30)
            qp = qp_i;
        else if (qp_i > 43)
            qp = qp_i - 6;
        else
            qp = qp_c[qp_i - 30];
    } else {
        qp = av_clip(qp_i, 0, 51);
    }

    idxt = av_clip(qp + DEFAULT_INTRA_TC_OFFSET + tc_offset, 0, 53);
    return tctable[idxt];
}

static int get_qPy_pred(HEVCContext *s, int xBase, int yBase, int log2_cb_size)
{
    HEVCLocalContext *lc     = s->HEVClc;
    int ctb_size_mask        = (1 << s->sps->log2_ctb_size) - 1;
    int MinCuQpDeltaSizeMask = (1 << (s->sps->log2_ctb_size -
                                      s->pps->diff_cu_qp_delta_depth)) - 1;
    int xQgBase              = xBase - (xBase & MinCuQpDeltaSizeMask);
    int yQgBase              = yBase - (yBase & MinCuQpDeltaSizeMask);
    int min_cb_width         = s->sps->min_cb_width;
    int x_cb                 = xQgBase >> s->sps->log2_min_cb_size;
    int y_cb                 = yQgBase >> s->sps->log2_min_cb_size;
    int availableA           = (xBase   & ctb_size_mask) &&
                               (xQgBase & ctb_size_mask);
    int availableB           = (yBase   & ctb_size_mask) &&
                               (yQgBase & ctb_size_mask);
    int qPy_pred, qPy_a, qPy_b;

    // qPy_pred
    if (lc->first_qp_group || (!xQgBase && !yQgBase)) {
        lc->first_qp_group = !lc->tu.is_cu_qp_delta_coded;
        qPy_pred = s->sh.slice_qp;
    } else {
        qPy_pred = lc->qPy_pred;
    }

    // qPy_a
    if (availableA == 0)
        qPy_a = qPy_pred;
    else
        qPy_a = s->qp_y_tab[(x_cb - 1) + y_cb * min_cb_width];

    // qPy_b
    if (availableB == 0)
        qPy_b = qPy_pred;
    else
        qPy_b = s->qp_y_tab[x_cb + (y_cb - 1) * min_cb_width];

    return (qPy_a + qPy_b + 1) >> 1;
}

void ff_hevc_set_qPy(HEVCContext *s, int xBase, int yBase, int log2_cb_size)
{
    int qp_y = get_qPy_pred(s, xBase, yBase, log2_cb_size);

    if (s->HEVClc->tu.cu_qp_delta != 0) {
        int off = s->sps->qp_bd_offset;
        s->HEVClc->qp_y = FFUMOD(qp_y + s->HEVClc->tu.cu_qp_delta + 52 + 2 * off,
                                 52 + off) - off;
    } else
        s->HEVClc->qp_y = qp_y;
}

static int get_qPy(HEVCContext *s, int xC, int yC)
{
    int log2_min_cb_size  = s->sps->log2_min_cb_size;
    int x                 = xC >> log2_min_cb_size;
    int y                 = yC >> log2_min_cb_size;
    return s->qp_y_tab[x + y * s->sps->min_cb_width];
}

static void copy_CTB(uint8_t *dst, uint8_t *src,
                     int width, int height, int stride_dst, int stride_src)
{
    int i;

    for (i = 0; i < height; i++) {
        memcpy(dst, src, width);
        dst += stride_dst;
        src += stride_src;
    }
}

static void restore_tqb_pixels(HEVCContext *s, int x0, int y0, int width, int height, int c_idx)
{
    if ( s->pps->transquant_bypass_enable_flag ||
            (s->sps->pcm.loop_filter_disable_flag && s->sps->pcm_enabled_flag)) {
        int x, y;
        ptrdiff_t stride_dst = s->sao_frame->linesize[c_idx];
        ptrdiff_t stride_src = s->frame->linesize[c_idx];
        int min_pu_size  = 1 << s->sps->log2_min_pu_size;
        int hshift       = s->sps->hshift[c_idx];
        int vshift       = s->sps->vshift[c_idx];
        int x_min        = ((x0         ) >> s->sps->log2_min_pu_size);
        int y_min        = ((y0         ) >> s->sps->log2_min_pu_size);
        int x_max        = ((x0 + width ) >> s->sps->log2_min_pu_size);
        int y_max        = ((y0 + height) >> s->sps->log2_min_pu_size);
        int len          = min_pu_size >> hshift;
        for (y = y_min; y < y_max; y++) {
            for (x = x_min; x < x_max; x++) {
                if (s->is_pcm[y * s->sps->min_pu_width + x]) {
                    int n;
                    uint8_t *src = &s->frame->data[c_idx][    ((y << s->sps->log2_min_pu_size) >> vshift) * stride_src + (((x << s->sps->log2_min_pu_size) >> hshift) << s->sps->pixel_shift)];
                    uint8_t *dst = &s->sao_frame->data[c_idx][((y << s->sps->log2_min_pu_size) >> vshift) * stride_dst + (((x << s->sps->log2_min_pu_size) >> hshift) << s->sps->pixel_shift)];
                    for (n = 0; n < (min_pu_size >> vshift); n++) {
                        memcpy(src, dst, len);
                        src += stride_src;
                        dst += stride_dst;
                    }
                }
            }
        }
    }
}

#define CTB(tab, x, y) ((tab)[(y) * s->sps->ctb_width + (x)])

static void sao_filter_CTB(HEVCContext *s, int x, int y)
{
    int c_idx;
    int edges[4];  // 0 left 1 top 2 right 3 bottom
    int x_ctb                = x >> s->sps->log2_ctb_size;
    int y_ctb                = y >> s->sps->log2_ctb_size;
    int ctb_addr_rs          = y_ctb * s->sps->ctb_width + x_ctb;
    int ctb_addr_ts          = s->pps->ctb_addr_rs_to_ts[ctb_addr_rs];
    SAOParams *sao           = &CTB(s->sao, x_ctb, y_ctb);
    // flags indicating unfilterable edges
    uint8_t vert_edge[]      = { 0, 0 };
    uint8_t horiz_edge[]     = { 0, 0 };
    uint8_t diag_edge[]      = { 0, 0, 0, 0 };
    uint8_t lfase            = CTB(s->filter_slice_edges, x_ctb, y_ctb);
    uint8_t no_tile_filter   = s->pps->tiles_enabled_flag &&
                               !s->pps->loop_filter_across_tiles_enabled_flag;
    uint8_t restore          = no_tile_filter || !lfase;
    uint8_t left_tile_edge   = 0;
    uint8_t right_tile_edge  = 0;
    uint8_t up_tile_edge     = 0;
    uint8_t bottom_tile_edge = 0;

    edges[0]   = x_ctb == 0;
    edges[1]   = y_ctb == 0;
    edges[2]   = x_ctb == s->sps->ctb_width  - 1;
    edges[3]   = y_ctb == s->sps->ctb_height - 1;

    if (restore) {
        if (!edges[0]) {
            left_tile_edge  = no_tile_filter && s->pps->tile_id[ctb_addr_ts] != s->pps->tile_id[s->pps->ctb_addr_rs_to_ts[ctb_addr_rs-1]];
            vert_edge[0]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb - 1, y_ctb)) || left_tile_edge;
        }
        if (!edges[2]) {
            right_tile_edge = no_tile_filter && s->pps->tile_id[ctb_addr_ts] != s->pps->tile_id[s->pps->ctb_addr_rs_to_ts[ctb_addr_rs+1]];
            vert_edge[1]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb + 1, y_ctb)) || right_tile_edge;
        }
        if (!edges[1]) {
            up_tile_edge     = no_tile_filter && s->pps->tile_id[ctb_addr_ts] != s->pps->tile_id[s->pps->ctb_addr_rs_to_ts[ctb_addr_rs - s->sps->ctb_width]];
            horiz_edge[0]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb, y_ctb - 1)) || up_tile_edge;
        }
        if (!edges[3]) {
            bottom_tile_edge = no_tile_filter && s->pps->tile_id[ctb_addr_ts] != s->pps->tile_id[s->pps->ctb_addr_rs_to_ts[ctb_addr_rs + s->sps->ctb_width]];
            horiz_edge[1]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb, y_ctb + 1)) || bottom_tile_edge;
        }
        if (!edges[0] && !edges[1]) {
            diag_edge[0] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb - 1, y_ctb - 1)) || left_tile_edge || up_tile_edge;
        }
        if (!edges[1] && !edges[2]) {
            diag_edge[1] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb + 1, y_ctb - 1)) || right_tile_edge || up_tile_edge;
        }
        if (!edges[2] && !edges[3]) {
            diag_edge[2] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb + 1, y_ctb + 1)) || right_tile_edge || bottom_tile_edge;
        }
        if (!edges[0] && !edges[3]) {
            diag_edge[3] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb - 1, y_ctb + 1)) || left_tile_edge || bottom_tile_edge;
        }
    }

    for (c_idx = 0; c_idx < (s->sps->chroma_array_type ? 3 : 1); c_idx++) {
        int x0       = x >> s->sps->hshift[c_idx];
        int y0       = y >> s->sps->vshift[c_idx];
        int stride_src = s->frame->linesize[c_idx];
        int stride_dst = s->sao_frame->linesize[c_idx];
        int ctb_size_h = (1 << (s->sps->log2_ctb_size)) >> s->sps->hshift[c_idx];
        int ctb_size_v = (1 << (s->sps->log2_ctb_size)) >> s->sps->vshift[c_idx];
        int width    = FFMIN(ctb_size_h, (s->sps->width  >> s->sps->hshift[c_idx]) - x0);
        int height   = FFMIN(ctb_size_v, (s->sps->height >> s->sps->vshift[c_idx]) - y0);
        uint8_t *src = &s->frame->data[c_idx][y0 * stride_src + (x0 << s->sps->pixel_shift)];
        uint8_t *dst = &s->sao_frame->data[c_idx][y0 * stride_dst + (x0 << s->sps->pixel_shift)];

        switch (sao->type_idx[c_idx]) {
        case SAO_BAND:
            copy_CTB(dst, src, width << s->sps->pixel_shift, height, stride_dst, stride_src);
            s->hevcdsp.sao_band_filter(src, dst,
                                       stride_src, stride_dst,
                                       sao,
                                       edges, width,
                                       height, c_idx);
            restore_tqb_pixels(s, x, y, width, height, c_idx);
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        case SAO_EDGE:
        {
            uint8_t left_pixels = !edges[0] && (CTB(s->sao, x_ctb-1, y_ctb).type_idx[c_idx] != SAO_APPLIED);
            if (!edges[1]) {
                uint8_t top_left  = !edges[0] && (CTB(s->sao, x_ctb-1, y_ctb-1).type_idx[c_idx] != SAO_APPLIED);
                uint8_t top_right = !edges[2] && (CTB(s->sao, x_ctb+1, y_ctb-1).type_idx[c_idx] != SAO_APPLIED);
                if (CTB(s->sao, x_ctb  , y_ctb-1).type_idx[c_idx] == 0)
                    memcpy( dst - stride_dst - (top_left << s->sps->pixel_shift),
                            src - stride_src - (top_left << s->sps->pixel_shift),
                            (top_left + width + top_right) << s->sps->pixel_shift);
                else {
                    if (top_left)
                        memcpy( dst - stride_dst - (1 << s->sps->pixel_shift),
                                src - stride_src - (1 << s->sps->pixel_shift),
                                1 << s->sps->pixel_shift);
                    if(top_right)
                        memcpy( dst - stride_dst + (width << s->sps->pixel_shift),
                                src - stride_src + (width << s->sps->pixel_shift),
                                1 << s->sps->pixel_shift);
                }
            }
            if (!edges[3]) {                                                                // bottom and bottom right
                uint8_t bottom_left = !edges[0] && (CTB(s->sao, x_ctb-1, y_ctb+1).type_idx[c_idx] != SAO_APPLIED);
                memcpy( dst + height * stride_dst - (bottom_left << s->sps->pixel_shift),
                        src + height * stride_src - (bottom_left << s->sps->pixel_shift),
                        (width + 1 + bottom_left) << s->sps->pixel_shift);
            }
            copy_CTB(dst - (left_pixels << s->sps->pixel_shift),
                     src - (left_pixels << s->sps->pixel_shift),
                     (width + 1 + left_pixels) << s->sps->pixel_shift, height, stride_dst, stride_src);
            s->hevcdsp.sao_edge_filter[restore](src, dst,
                                                stride_src, stride_dst,
                                                sao,
                                                edges, width,
                                                height, c_idx,
                                                vert_edge,
                                                horiz_edge,
                                                diag_edge);
            restore_tqb_pixels(s, x, y, width, height, c_idx);
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        }
        }
    }
}

static int get_pcm(HEVCContext *s, int x, int y)
{
    int log2_min_pu_size = s->sps->log2_min_pu_size;
    int x_pu, y_pu;

    if (x < 0 || y < 0)
        return 2;

    x_pu = x >> log2_min_pu_size;
    y_pu = y >> log2_min_pu_size;

    if (x_pu >= s->sps->min_pu_width || y_pu >= s->sps->min_pu_height)
        return 2;
    return s->is_pcm[y_pu * s->sps->min_pu_width + x_pu];
}

#define TC_CALC(qp, bs)                                                 \
    tctable[av_clip((qp) + DEFAULT_INTRA_TC_OFFSET * ((bs) - 1) +       \
                    (tc_offset >> 1 << 1),                              \
                    0, MAX_QP + DEFAULT_INTRA_TC_OFFSET)]

static void deblocking_filter_CTB(HEVCContext *s, int x0, int y0)
{
    uint8_t *src;
    int x, y;
    int chroma;
    int c_tc[2], tc[2], beta;
    uint8_t no_p[2] = { 0 };
    uint8_t no_q[2] = { 0 };

    int log2_ctb_size = s->sps->log2_ctb_size;
    int x_end, x_end2, y_end;
    int ctb_size        = 1 << log2_ctb_size;
    int ctb             = (x0 >> log2_ctb_size) +
                          (y0 >> log2_ctb_size) * s->sps->ctb_width;
    int8_t cur_tc_offset   = s->deblock[ctb].tc_offset;
    int8_t cur_beta_offset = s->deblock[ctb].beta_offset;
    int8_t left_tc_offset, left_beta_offset;
    int8_t tc_offset, beta_offset;
    int pcmf = (s->sps->pcm_enabled_flag &&
                s->sps->pcm.loop_filter_disable_flag) ||
               s->pps->transquant_bypass_enable_flag;

    if (x0) {
        left_tc_offset   = s->deblock[ctb - 1].tc_offset;
        left_beta_offset = s->deblock[ctb - 1].beta_offset;
    } else {
        left_tc_offset   = 0;
        left_beta_offset = 0;
    }

    x_end = x0 + ctb_size;
    if (x_end > s->sps->width)
        x_end = s->sps->width;
    y_end = y0 + ctb_size;
    if (y_end > s->sps->height)
        y_end = s->sps->height;

    tc_offset   = cur_tc_offset;
    beta_offset = cur_beta_offset;

    // vertical filtering luma
    for (y = y0; y < y_end; y += 8) {
        for (x = x0 ? x0 : 8; x < x_end; x += 8) {
            const int bs0 = s->vertical_bs[(x +  y      * s->bs_width) >> 2];
            const int bs1 = s->vertical_bs[(x + (y + 4) * s->bs_width) >> 2];
            if (bs0 || bs1) {
                const int qp = (get_qPy(s, x - 1, y)     + get_qPy(s, x, y)     + 1) >> 1;

                beta    = betatable[av_clip(qp + beta_offset, 0, MAX_QP)];
                tc[0]   = bs0 ? TC_CALC(qp, bs0) : 0;
                tc[1]   = bs1 ? TC_CALC(qp, bs1) : 0;
                src     = &s->frame->data[LUMA][y * s->frame->linesize[LUMA] + (x << s->sps->pixel_shift)];
                if (pcmf) {
                    no_p[0] = get_pcm(s, x - 1, y);
                    no_p[1] = get_pcm(s, x - 1, y + 4);
                    no_q[0] = get_pcm(s, x, y);
                    no_q[1] = get_pcm(s, x, y + 4);

                    if (!no_p[0] &&
                        !no_p[1] &&
                        !no_q[0] &&
                        !no_q[1]) {
                        s->hevcdsp.hevc_v_loop_filter_luma(src,
                                                           s->frame->linesize[LUMA],
                                                           beta, tc, no_p, no_q);
                    } else {
                        s->hevcdsp.hevc_v_loop_filter_luma_c(src,
                                                             s->frame->linesize[LUMA],
                                                             beta, tc, no_p, no_q);
                    }
                } else
                    s->hevcdsp.hevc_v_loop_filter_luma(src,
                                                       s->frame->linesize[LUMA],
                                                       beta, tc, no_p, no_q);
            }
        }
    }

    // vertical filtering chroma
    if (s->sps->chroma_array_type) {
        int c_tc2[2];
        uint8_t* src2;
        int h = 1 << s->sps->hshift[1];
        int v = 1 << s->sps->vshift[1];
        for (y = y0; y < y_end; y += (8 * v)) {
            for (x = x0 ? x0 : 8 * h; x < x_end; x += (8 * h)) {
                const int bs0 = s->vertical_bs[(x +  y            * s->bs_width) >> 2];
                const int bs1 = s->vertical_bs[(x + (y + (4 * v)) * s->bs_width) >> 2];

                if ((bs0 == 2) || (bs1 == 2)) {
                    const int qp0 = (get_qPy(s, x - 1, y)           + get_qPy(s, x, y)           + 1) >> 1;
                    const int qp1 = (get_qPy(s, x - 1, y + (4 * v)) + get_qPy(s, x, y + (4 * v)) + 1) >> 1;

                    c_tc[0]  = (bs0 == 2) ? chroma_tc(s, qp0, 1, tc_offset) : 0;
                    c_tc[1]  = (bs1 == 2) ? chroma_tc(s, qp1, 1, tc_offset) : 0;
                    c_tc2[0] = (bs0 == 2) ? chroma_tc(s, qp0, 2, tc_offset) : 0;
                    c_tc2[1] = (bs1 == 2) ? chroma_tc(s, qp1, 2, tc_offset) : 0;
                    src      = &s->frame->data[1][(y >> s->sps->vshift[1]) * s->frame->linesize[1] + ((x >> s->sps->hshift[1]) << s->sps->pixel_shift)];
                    src2     = &s->frame->data[2][(y >> s->sps->vshift[2]) * s->frame->linesize[2] + ((x >> s->sps->hshift[2]) << s->sps->pixel_shift)];
                    if (pcmf) {
                        no_p[0] = get_pcm(s, x - 1, y);
                        no_p[1] = get_pcm(s, x - 1, y + (4 * v));
                        no_q[0] = get_pcm(s, x, y);
                        no_q[1] = get_pcm(s, x, y + (4 * v));
                        if (!no_p[0] &&
                            !no_p[1] &&
                            !no_q[0] &&
                            !no_q[1]) {
                            s->hevcdsp.hevc_v_loop_filter_chroma(src,
                                                                 s->frame->linesize[1],
                                                                 c_tc, no_p, no_q);
                            s->hevcdsp.hevc_v_loop_filter_chroma(src2,
                                                                 s->frame->linesize[2],
                                                                 c_tc2, no_p, no_q);
                        } else {
                            s->hevcdsp.hevc_v_loop_filter_chroma_c(src,
                                                                   s->frame->linesize[1],
                                                                   c_tc, no_p, no_q);
                            s->hevcdsp.hevc_v_loop_filter_chroma_c(src2,
                                                                   s->frame->linesize[2],
                                                                   c_tc2, no_p, no_q);
                        }
                    } else {
                        s->hevcdsp.hevc_v_loop_filter_chroma(src,
                                                             s->frame->linesize[1],
                                                             c_tc, no_p, no_q);
                        s->hevcdsp.hevc_v_loop_filter_chroma(src2,
                                                             s->frame->linesize[2],
                                                             c_tc2, no_p, no_q);
                    }
                }
            }
        }
    }

    // horizontal filtering luma
    x_end2 = x_end;
    if (x_end != s->sps->width)
        x_end -= 8;
    for (y = y0 ? y0 : 8; y < y_end; y += 8) {
        beta_offset = x0 ? left_beta_offset : cur_beta_offset;
        for (x = x0 ? x0 - 8 : 0; x < x_end; x += 8) {
            const int bs0 = s->horizontal_bs[( x      + y * s->bs_width) >> 2];
            const int bs1 = s->horizontal_bs[((x + 4) + y * s->bs_width) >> 2];
            if (bs0 || bs1) {
                const int qp = (get_qPy(s, x, y - 1)     + get_qPy(s, x, y)     + 1) >> 1;

                beta    = betatable[av_clip(qp + beta_offset, 0, MAX_QP)];
                tc[0]   = bs0 ? TC_CALC(qp, bs0) : 0;
                tc[1]   = bs1 ? TC_CALC(qp, bs1) : 0;
                src     = &s->frame->data[LUMA][y * s->frame->linesize[LUMA] + (x << s->sps->pixel_shift)];
                if (pcmf) {
                    no_p[0] = get_pcm(s, x, y - 1);
                    no_p[1] = get_pcm(s, x + 4, y - 1);
                    no_q[0] = get_pcm(s, x, y);
                    no_q[1] = get_pcm(s, x + 4, y);
                    if (!no_p[0] &&
                        !no_p[1] &&
                        !no_q[0] &&
                        !no_q[1]) {
                        s->hevcdsp.hevc_h_loop_filter_luma(src,
                                                             s->frame->linesize[LUMA],
                                                             beta, tc, no_p, no_q);
                    } else {
                        s->hevcdsp.hevc_h_loop_filter_luma_c(src,
                                                           s->frame->linesize[LUMA],
                                                           beta, tc, no_p, no_q);
                    }
                } else
                    s->hevcdsp.hevc_h_loop_filter_luma(src,
                                                       s->frame->linesize[LUMA],
                                                       beta, tc, no_p, no_q);
            }
            beta_offset = cur_beta_offset;
        }
    }

    // horizontal filtering chroma
    if (s->sps->chroma_array_type) {
        int c_tc2[2];
        uint8_t* src2;
        int h = 1 << s->sps->hshift[1];
        int v = 1 << s->sps->vshift[1];
        if (x_end2 != s->sps->width)
             x_end = x_end2 - 8*h;
        for (y = y0 ? y0 : 8 * v; y < y_end; y += (8 * v)) {
            tc_offset = x0 ? left_tc_offset : cur_tc_offset;
            for (x = x0 ? x0 - 8 * h : 0; x < x_end; x += (8 * h)) {
                const int bs0 = s->horizontal_bs[( x          + y * s->bs_width) >> 2];
                const int bs1 = s->horizontal_bs[((x + 4 * h) + y * s->bs_width) >> 2];
                if ((bs0 == 2) || (bs1 == 2)) {
                    const int qp0 = bs0 == 2 ? (get_qPy(s, x,           y - 1) + get_qPy(s, x,           y) + 1) >> 1 : 0;
                    const int qp1 = bs1 == 2 ? (get_qPy(s, x + (4 * h), y - 1) + get_qPy(s, x + (4 * h), y) + 1) >> 1 : 0;

                    c_tc[0]   = bs0 == 2 ? chroma_tc(s, qp0, 1, tc_offset)     : 0;
                    c_tc[1]   = bs1 == 2 ? chroma_tc(s, qp1, 1, cur_tc_offset) : 0;
                    c_tc2[0]  = bs0 == 2 ? chroma_tc(s, qp0, 2, tc_offset)     : 0;
                    c_tc2[1]  = bs1 == 2 ? chroma_tc(s, qp1, 2, cur_tc_offset) : 0;
                    src       = &s->frame->data[1][(y >> s->sps->vshift[1]) * s->frame->linesize[1] + ((x >> s->sps->hshift[1]) << s->sps->pixel_shift)];
                    src2      = &s->frame->data[2][(y >> s->sps->vshift[1]) * s->frame->linesize[2] + ((x >> s->sps->hshift[1]) << s->sps->pixel_shift)];
                    if (pcmf) {
                        no_p[0] = get_pcm(s, x,           y - 1);
                        no_p[1] = get_pcm(s, x + (4 * h), y - 1);
                        no_q[0] = get_pcm(s, x,           y);
                        no_q[1] = get_pcm(s, x + (4 * h), y);
                        if (!no_p[0] &&
                            !no_p[1] &&
                            !no_q[0] &&
                            !no_q[1]) {
                            s->hevcdsp.hevc_h_loop_filter_chroma(src,
                                                                 s->frame->linesize[1],
                                                                 c_tc, no_p, no_q);
                            s->hevcdsp.hevc_h_loop_filter_chroma(src2,
                                                                 s->frame->linesize[2],
                                                                 c_tc2, no_p, no_q);
                        } else {
                            s->hevcdsp.hevc_h_loop_filter_chroma_c(src,
                                                                   s->frame->linesize[1],
                                                                   c_tc, no_p, no_q);
                            s->hevcdsp.hevc_h_loop_filter_chroma_c (src2,
                                                                   s->frame->linesize[2],
                                                                   c_tc2, no_p, no_q);
                        }
                    } else {
                        s->hevcdsp.hevc_h_loop_filter_chroma(src,
                                                             s->frame->linesize[1],
                                                             c_tc, no_p, no_q);
                        s->hevcdsp.hevc_h_loop_filter_chroma(src2,
                                                             s->frame->linesize[2],
                                                             c_tc2, no_p, no_q);
                    }
                }
                tc_offset = cur_tc_offset;
            }
        }
    }
}

#ifdef TEST_MV_POC
static int boundary_strength(HEVCContext *s, MvField *curr, MvField *neigh)
{
#if HAVE_SSE42
    {
        __m128i x0, x1, x2, x3;
        x0 = _mm_loadu_si128((__m128i *) neigh);
        x1 = _mm_loadu_si128((__m128i *) curr);
        x2 = _mm_loadl_epi64((__m128i *) &(neigh->pred_flag));
        x3 = _mm_loadl_epi64((__m128i *) &(curr->pred_flag));
        x0 = _mm_cmpeq_epi64 (x0, x1);
        x2 = _mm_cmpeq_epi32 (x2, x3);

        if (_mm_test_all_ones(x0) && _mm_test_all_ones(x2))
            return 0;
    }
#else
    if ( memcmp(curr, neigh, sizeof(MvField)) == 0)
        return 0;
#endif
    if (curr->pred_flag == PF_BI &&  neigh->pred_flag == PF_BI) {
        // same L0 and L1
        if (curr->poc[0] == neigh->poc[0]  &&
            curr->poc[0] == curr->poc[1] &&
            neigh->poc[0] == neigh->poc[1]) {
#if HAVE_SSE42
            __m128i x0, x1, x2;
            x0 = _mm_loadl_epi64((__m128i *) neigh);
            x1 = _mm_loadl_epi64((__m128i *) curr);
            x2 =  _mm_shufflelo_epi16(x0, 0x4E);
            x0 = _mm_sub_epi16(x0, x1);
            x2 = _mm_sub_epi16(x2, x1);
            x1 = _mm_set1_epi16(4);
            x0 = _mm_abs_epi16(x0);
            x2 = _mm_abs_epi16(x2);
            x0 = _mm_cmplt_epi16(x0, x1);
            x2 = _mm_cmplt_epi16(x2, x1);
            return !(_mm_test_all_ones(x0) || _mm_test_all_ones(x2));
#else
            if ((FFABS(neigh->mv[0].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[0].y) >= 4 ||
                 FFABS(neigh->mv[1].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[1].y) >= 4) &&
                (FFABS(neigh->mv[1].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[0].y) >= 4 ||
                 FFABS(neigh->mv[0].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[1].y) >= 4))
                return 1;
            else
                return 0;
#endif
        } else if (neigh->poc[0] == curr->poc[0] &&
                   neigh->poc[1] == curr->poc[1]) {
#if HAVE_SSE42
            __m128i x0, x1;
            x0 = _mm_loadl_epi64((__m128i *) neigh);
            x1 = _mm_loadl_epi64((__m128i *) curr);
            x0 = _mm_sub_epi16(x0, x1);
            x1 = _mm_set1_epi16(4);
            x0 = _mm_abs_epi16(x0);
            x0 = _mm_cmplt_epi16(x0, x1);
            return !(_mm_test_all_ones(x0));
#else
            if (FFABS(neigh->mv[0].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[0].y) >= 4 ||
                FFABS(neigh->mv[1].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[1].y) >= 4)
                return 1;
            else
                return 0;
#endif
        } else if (neigh->poc[1] == curr->poc[0] &&
                   neigh->poc[0] == curr->poc[1]) {
#if HAVE_SSE42
            __m128i x0, x1, x2;
            x0 = _mm_loadl_epi64((__m128i *) neigh);
            x1 = _mm_loadl_epi64((__m128i *) curr);
            x2 = _mm_shufflelo_epi16(x0, 0x4E);
            x2 = _mm_sub_epi16(x2, x1);
            x1 = _mm_set1_epi16(4);
            x2 = _mm_abs_epi16(x2);
            x2 = _mm_cmplt_epi16(x2, x1);
            return !(_mm_test_all_ones(x2));
#else
            if (FFABS(neigh->mv[1].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[0].y) >= 4 ||
                FFABS(neigh->mv[0].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[1].y) >= 4)
                return 1;
            else
                return 0;
#endif
        } else {
            return 1;
        }
    } else if ((curr->pred_flag != PF_BI) && (neigh->pred_flag != PF_BI)){ // 1 MV
        Mv A, B;
        int ref_A, ref_B;

        if (curr->pred_flag & 1) {
            A     = curr->mv[0];
            ref_A = curr->poc[0];
        } else {
            A     = curr->mv[1];
            ref_A = curr->poc[1];
        }

        if (neigh->pred_flag & 1) {
            B     = neigh->mv[0];
            ref_B = neigh->poc[0];
        } else {
            B     = neigh->mv[1];
            ref_B = neigh->poc[1];
        }

        if (ref_A == ref_B) {
            if (FFABS(A.x - B.x) >= 4 || FFABS(A.y - B.y) >= 4)
                return 1;
            else
                return 0;
        } else
            return 1;
    }

    return 1;
}
#else
static int boundary_strength(HEVCContext *s, MvField *curr, MvField *neigh,
                             RefPicList *neigh_refPicList)
{
    if (curr->pred_flag == PF_BI &&  neigh->pred_flag == PF_BI) {
        // same L0 and L1
        if (s->ref->refPicList[0].list[curr->ref_idx[0]] == neigh_refPicList[0].list[neigh->ref_idx[0]]  &&
            s->ref->refPicList[0].list[curr->ref_idx[0]] == s->ref->refPicList[1].list[curr->ref_idx[1]] &&
            neigh_refPicList[0].list[neigh->ref_idx[0]] == neigh_refPicList[1].list[neigh->ref_idx[1]]) {
#if HAVE_SSE42
            __m128i x0, x1, x2;
            x0 = _mm_loadl_epi64((__m128i *) neigh);
            x1 = _mm_loadl_epi64((__m128i *) curr);
            x2 =  _mm_shufflelo_epi16(x0, 0x4E);
            x0 = _mm_sub_epi16(x0, x1);
            x2 = _mm_sub_epi16(x2, x1);
            x1 = _mm_set1_epi16(4);
            x0 = _mm_abs_epi16(x0);
            x2 = _mm_abs_epi16(x2);
            x0 = _mm_cmplt_epi16(x0, x1);
            x2 = _mm_cmplt_epi16(x2, x1);
            return !(_mm_test_all_ones(x0) || _mm_test_all_ones(x2));
#else
            if ((FFABS(neigh->mv[0].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[0].y) >= 4 ||
                 FFABS(neigh->mv[1].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[1].y) >= 4) &&
                (FFABS(neigh->mv[1].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[0].y) >= 4 ||
                 FFABS(neigh->mv[0].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[1].y) >= 4))
                return 1;
            else
                return 0;
#endif
        } else if (neigh_refPicList[0].list[neigh->ref_idx[0]] == s->ref->refPicList[0].list[curr->ref_idx[0]] &&
                   neigh_refPicList[1].list[neigh->ref_idx[1]] == s->ref->refPicList[1].list[curr->ref_idx[1]]) {
#if HAVE_SSE42
            __m128i x0, x1;
            x0 = _mm_loadl_epi64((__m128i *) neigh);
            x1 = _mm_loadl_epi64((__m128i *) curr);
            x0 = _mm_sub_epi16(x0, x1);
            x1 = _mm_set1_epi16(4);
            x0 = _mm_abs_epi16(x0);
            x0 = _mm_cmplt_epi16(x0, x1);
            return !(_mm_test_all_ones(x0));
#else
            if (FFABS(neigh->mv[0].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[0].y) >= 4 ||
                FFABS(neigh->mv[1].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[1].y) >= 4)
                return 1;
            else
                return 0;
#endif
        } else if (neigh_refPicList[1].list[neigh->ref_idx[1]] == s->ref->refPicList[0].list[curr->ref_idx[0]] &&
                   neigh_refPicList[0].list[neigh->ref_idx[0]] == s->ref->refPicList[1].list[curr->ref_idx[1]]) {
#if HAVE_SSE42
            __m128i x0, x1, x2;
            x0 = _mm_loadl_epi64((__m128i *) neigh);
            x1 = _mm_loadl_epi64((__m128i *) curr);
            x2 = _mm_shufflelo_epi16(x0, 0x4E);
            x2 = _mm_sub_epi16(x2, x1);
            x1 = _mm_set1_epi16(4);
            x2 = _mm_abs_epi16(x2);
            x2 = _mm_cmplt_epi16(x2, x1);
            return !(_mm_test_all_ones(x2));
#else
            if (FFABS(neigh->mv[1].x - curr->mv[0].x) >= 4 || FFABS(neigh->mv[1].y - curr->mv[0].y) >= 4 ||
                FFABS(neigh->mv[0].x - curr->mv[1].x) >= 4 || FFABS(neigh->mv[0].y - curr->mv[1].y) >= 4)
                return 1;
            else
                return 0;
#endif
        } else {
            return 1;
        }
    } else if ((curr->pred_flag != PF_BI) && (neigh->pred_flag != PF_BI)){ // 1 MV
        Mv A, B;
        int ref_A, ref_B;

        if (curr->pred_flag & 1) {
            A     = curr->mv[0];
            ref_A = s->ref->refPicList[0].list[curr->ref_idx[0]];
        } else {
            A     = curr->mv[1];
            ref_A = s->ref->refPicList[1].list[curr->ref_idx[1]];
        }

        if (neigh->pred_flag & 1) {
            B     = neigh->mv[0];
            ref_B = neigh_refPicList[0].list[neigh->ref_idx[0]];
        } else {
            B     = neigh->mv[1];
            ref_B = neigh_refPicList[1].list[neigh->ref_idx[1]];
        }

        if (ref_A == ref_B) {
            if (FFABS(A.x - B.x) >= 4 || FFABS(A.y - B.y) >= 4)
                return 1;
            else
                return 0;
        } else
            return 1;
    }

    return 1;
}
#endif

void ff_hevc_deblocking_boundary_strengths(HEVCContext *s, int x0, int y0,
                                           int log2_trafo_size)
{
    HEVCLocalContext *lc = s->HEVClc;
    MvField *tab_mvf     = s->ref->tab_mvf;
    int log2_min_pu_size = s->sps->log2_min_pu_size;
    int log2_min_tu_size = s->sps->log2_min_tb_size;
    int min_pu_width     = s->sps->min_pu_width;
    int min_tu_width     = s->sps->min_tb_width;
    int is_intra = tab_mvf[(y0 >> log2_min_pu_size) * min_pu_width +
                           (x0 >> log2_min_pu_size)].pred_flag == PF_INTRA;
    int i, j, bs;

    if (y0 > 0 && (y0 & 7) == 0) {
        int bd_ctby = y0 & ((1 << s->sps->log2_ctb_size) - 1);
        int bd_slice = s->sh.slice_loop_filter_across_slices_enabled_flag ||
                       !(lc->slice_or_tiles_up_boundary & 1);
        int bd_tiles = s->pps->loop_filter_across_tiles_enabled_flag ||
                       !(lc->slice_or_tiles_up_boundary & 2);
        if (((bd_slice && bd_tiles)  || bd_ctby)) {
            int yp_pu = (y0 - 1) >> log2_min_pu_size;
            int yq_pu =  y0      >> log2_min_pu_size;
            int yp_tu = (y0 - 1) >> log2_min_tu_size;
            int yq_tu =  y0      >> log2_min_tu_size;
#ifndef TEST_MV_POC
            RefPicList *top_refPicList = ff_hevc_get_ref_list(s, s->ref,
                                                              x0, y0 - 1);
#endif
            for (i = 0; i < (1 << log2_trafo_size); i += 4) {
                int x_pu = (x0 + i) >> log2_min_pu_size;
                int x_tu = (x0 + i) >> log2_min_tu_size;
                MvField *top  = &tab_mvf[yp_pu * min_pu_width + x_pu];
                MvField *curr = &tab_mvf[yq_pu * min_pu_width + x_pu];
                uint8_t top_cbf_luma  = s->cbf_luma[yp_tu * min_tu_width + x_tu];
                uint8_t curr_cbf_luma = s->cbf_luma[yq_tu * min_tu_width + x_tu];

                if (curr->pred_flag == PF_INTRA || top->pred_flag == PF_INTRA)
                    bs = 2;
                else if (curr_cbf_luma || top_cbf_luma)
                    bs = 1;
                else
#ifdef TEST_MV_POC
                    bs = boundary_strength(s, curr, top);
#else
                    bs = boundary_strength(s, curr, top, top_refPicList);
#endif
                s->horizontal_bs[((x0 + i) + y0 * s->bs_width) >> 2] = bs;
            }
        }
    }

    // bs for vertical TU boundaries
    if (x0 > 0 && (x0 & 7) == 0) {
        int bd_ctbx = x0 & ((1 << s->sps->log2_ctb_size) - 1);
        int bd_slice = s->sh.slice_loop_filter_across_slices_enabled_flag ||
                       !(lc->slice_or_tiles_left_boundary & 1);
        int bd_tiles = s->pps->loop_filter_across_tiles_enabled_flag ||
                       !(lc->slice_or_tiles_left_boundary & 2);
        if (((bd_slice && bd_tiles)  || bd_ctbx)) {
            int xp_pu = (x0 - 1) >> log2_min_pu_size;
            int xq_pu =  x0      >> log2_min_pu_size;
            int xp_tu = (x0 - 1) >> log2_min_tu_size;
            int xq_tu =  x0      >> log2_min_tu_size;
#ifndef TEST_MV_POC
            RefPicList *left_refPicList = ff_hevc_get_ref_list(s, s->ref,
                                                               x0 - 1, y0);
#endif
            for (i = 0; i < (1 << log2_trafo_size); i += 4) {
                int y_pu      = (y0 + i) >> log2_min_pu_size;
                int y_tu      = (y0 + i) >> log2_min_tu_size;
                MvField *left = &tab_mvf[y_pu * min_pu_width + xp_pu];
                MvField *curr = &tab_mvf[y_pu * min_pu_width + xq_pu];
                uint8_t left_cbf_luma = s->cbf_luma[y_tu * min_tu_width + xp_tu];
                uint8_t curr_cbf_luma = s->cbf_luma[y_tu * min_tu_width + xq_tu];

                if (curr->pred_flag == PF_INTRA || left->pred_flag == PF_INTRA)
                    bs = 2;
                else if (curr_cbf_luma || left_cbf_luma)
                    bs = 1;
                else
#ifdef TEST_MV_POC
                    bs = boundary_strength(s, curr, left);
#else
                    bs = boundary_strength(s, curr, left, left_refPicList);
#endif
                s->vertical_bs[(x0 + (y0 + i) * s->bs_width) >> 2] = bs;
            }
        }
    }

    if (log2_trafo_size > log2_min_pu_size && !is_intra) {
#ifndef TEST_MV_POC
        RefPicList *refPicList = ff_hevc_get_ref_list(s, s->ref,
                                                           x0,
                                                           y0);
#endif
        // bs for TU internal horizontal PU boundaries
        for (i = 0; i < (1 << log2_trafo_size); i += 4) {
            int x_pu  = (x0 + i) >> log2_min_pu_size;
            int yp_pu = (y0 + 8 - 1) >> log2_min_pu_size;
            MvField *top  = &tab_mvf[yp_pu * min_pu_width + x_pu];

            for (j = 8; j < (1 << log2_trafo_size); j += 8) {
                int yq_pu = (y0 + j)     >> log2_min_pu_size;
                MvField *curr = &tab_mvf[yq_pu * min_pu_width + x_pu];

#ifdef TEST_MV_POC
                bs = boundary_strength(s, curr, top);
#else
                bs = boundary_strength(s, curr, top, refPicList);
#endif
                s->horizontal_bs[((x0 + i) + (y0 + j) * s->bs_width) >> 2] = bs;
                top = curr;
            }
        }

        // bs for TU internal vertical PU boundaries
        for (j = 0; j < (1 << log2_trafo_size); j += 4) {
            int y_pu  = (y0 + j) >> log2_min_pu_size;
            int xp_pu = (x0 + 8 - 1) >> log2_min_pu_size;
            MvField *left = &tab_mvf[y_pu * min_pu_width + xp_pu];

            for (i = 8; i < (1 << log2_trafo_size); i += 8) {
                int xq_pu = (x0 + i)     >> log2_min_pu_size;
                MvField *curr = &tab_mvf[y_pu * min_pu_width + xq_pu];

#ifdef TEST_MV_POC
                bs = boundary_strength(s, curr, left);
#else
                bs = boundary_strength(s, curr, left, refPicList);
#endif
                s->vertical_bs[((x0 + i) + (y0 + j) * s->bs_width) >> 2] = bs;
                left = curr;
            }
        }
    }
}

void ff_hevc_deblocking_boundary_strengths_h(HEVCContext *s, int x0, int y0, int slice_up_boundary)
{
    MvField *tab_mvf        = s->ref->tab_mvf;
    int log2_min_pu_size    = s->sps->log2_min_pu_size;
    int log2_min_tu_size    = s->sps->log2_min_tb_size;
    int pic_width_in_min_pu = s->sps->width >> log2_min_pu_size;
    int pic_width_in_min_tu = s->sps->width >> log2_min_tu_size;
    int bs;
    if (y0 > 0 && (y0 & 7) == 0) {
        int yp_pu = (y0 - 1) >> log2_min_pu_size;
        int yq_pu =  y0      >> log2_min_pu_size;
        int yp_tu = (y0 - 1) >> log2_min_tu_size;
        int yq_tu =  y0      >> log2_min_tu_size;
        int x_pu  =  x0      >> log2_min_pu_size;
        int x_tu  =  x0      >> log2_min_tu_size;
        MvField *top  = &tab_mvf[yp_pu * pic_width_in_min_pu + x_pu];
        MvField *curr = &tab_mvf[yq_pu * pic_width_in_min_pu + x_pu];
        uint8_t top_cbf_luma  = s->cbf_luma[yp_tu * pic_width_in_min_tu + x_tu];
        uint8_t curr_cbf_luma = s->cbf_luma[yq_tu * pic_width_in_min_tu + x_tu];
#ifndef TEST_MV_POC
        RefPicList* top_refPicList = ff_hevc_get_ref_list(s, s->ref, x0, y0 - 1);
#endif
        if (curr->pred_flag == PF_INTRA || top->pred_flag == PF_INTRA)
            bs = 2;
        else if (curr_cbf_luma || top_cbf_luma)
            bs = 1;
        else
#ifdef TEST_MV_POC
                bs = boundary_strength(s, curr, top);
#else
                bs = boundary_strength(s, curr, top, top_refPicList);
#endif
        if ((slice_up_boundary & 1) && (y0 % (1 << s->sps->log2_ctb_size)) == 0)
            bs = 0;
        if (s->sh.disable_deblocking_filter_flag == 1)
            bs = 0;
        s->horizontal_bs[(x0 + y0 * s->bs_width) >> 2] =  bs;
    }
}

void ff_hevc_deblocking_boundary_strengths_v(HEVCContext *s, int x0, int y0, int slice_left_boundary)
{
    MvField *tab_mvf        = s->ref->tab_mvf;
    int log2_min_pu_size    = s->sps->log2_min_pu_size;
    int log2_min_tu_size    = s->sps->log2_min_tb_size;
    int pic_width_in_min_pu = s->sps->width >> log2_min_pu_size;
    int pic_width_in_min_tu = s->sps->width >> log2_min_tu_size;
    int bs;
    // bs for vertical TU boundaries
    if (x0 > 0 && (x0 & 7) == 0) {
        int xp_pu = (x0 - 1) >> log2_min_pu_size;
        int xq_pu =  x0      >> log2_min_pu_size;
        int xp_tu = (x0 - 1) >> log2_min_tu_size;
        int xq_tu =  x0      >> log2_min_tu_size;
        int y_pu  =  y0      >> log2_min_pu_size;
        int y_tu  =  y0      >> log2_min_tu_size;
        MvField *left = &tab_mvf[y_pu * pic_width_in_min_pu + xp_pu];
        MvField *curr = &tab_mvf[y_pu * pic_width_in_min_pu + xq_pu];
        uint8_t left_cbf_luma = s->cbf_luma[y_tu * pic_width_in_min_tu + xp_tu];
        uint8_t curr_cbf_luma = s->cbf_luma[y_tu * pic_width_in_min_tu + xq_tu];
#ifndef TEST_MV_POC
        RefPicList* left_refPicList = ff_hevc_get_ref_list(s, s->ref, x0 - 1, y0);
#endif
        if (curr->pred_flag == PF_INTRA || left->pred_flag == PF_INTRA)
            bs = 2;
        else if (curr_cbf_luma || left_cbf_luma)
            bs = 1;
        else
#ifdef TEST_MV_POC
                bs = boundary_strength(s, curr, left);
#else
                bs = boundary_strength(s, curr, left, left_refPicList);
#endif
        if ((slice_left_boundary & 1) && (x0 % (1 << s->sps->log2_ctb_size)) == 0)
            bs = 0;
        if (s->sh.disable_deblocking_filter_flag == 1)
            bs = 0;
        s->vertical_bs[(x0 + y0 * s->bs_width) >> 2] =  bs;
    }
}
#undef LUMA
#undef CB
#undef CR

void ff_hevc_hls_filter(HEVCContext *s, int x, int y, int ctb_size)
{
    deblocking_filter_CTB(s, x, y);
    if (s->sps->sao_enabled) {
        int x_end = x >= s->sps->width  - ctb_size;
        int y_end = y >= s->sps->height - ctb_size;
        if (y && x)
            sao_filter_CTB(s, x - ctb_size, y - ctb_size);
        if (x && y_end)
            sao_filter_CTB(s, x - ctb_size, y);
        if (y && x_end) {
            sao_filter_CTB(s, x, y - ctb_size);
            if (s->threads_type & FF_THREAD_FRAME )
                ff_thread_report_progress(&s->ref->tf, y - ctb_size, 0);
        }
        if (x_end && y_end) {
            sao_filter_CTB(s, x , y);
            if (s->threads_type & FF_THREAD_FRAME )
                ff_thread_report_progress(&s->ref->tf, y, 0);
        }
    } else {
        if (y && x >= s->sps->width - ctb_size)
            if (s->threads_type & FF_THREAD_FRAME )
                ff_thread_report_progress(&s->ref->tf, y, 0);
    }
}

void ff_hevc_hls_filters(HEVCContext *s, int x_ctb, int y_ctb, int ctb_size)
{
    int x_end = x_ctb >= s->sps->width  - ctb_size;
    int y_end = y_ctb >= s->sps->height - ctb_size;
    if (y_ctb && x_ctb)
        ff_hevc_hls_filter(s, x_ctb - ctb_size, y_ctb - ctb_size, ctb_size);
    if (y_ctb && x_end)
        ff_hevc_hls_filter(s, x_ctb, y_ctb - ctb_size, ctb_size);
    if (x_ctb && y_end)
        ff_hevc_hls_filter(s, x_ctb - ctb_size, y_ctb, ctb_size);
}

#if PARALLEL_FILTERS
void ff_hevc_hls_filters_slice(HEVCContext *s, int x_ctb, int y_ctb, int ctb_size) {
    int x_slice_end = 0, y_slice_end = 0;
    unsigned int x_end = x_ctb >= s->sps->width  - ctb_size;
    unsigned int y_end = y_ctb >= s->sps->height - ctb_size;
    int ctb_addr_rs = (x_ctb >> s->sps->log2_ctb_size) + (y_ctb >> s->sps->log2_ctb_size) * s->sps->ctb_width;

    if (y_ctb && x_ctb && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1 - s->sps->ctb_width]))
        ff_hevc_hls_filter_slice(s, x_ctb - ctb_size, y_ctb - ctb_size, ctb_size);

    if (y_ctb && x_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->sps->ctb_width]))
        ff_hevc_hls_filter_slice(s, x_ctb, y_ctb - ctb_size, ctb_size);


    if (x_ctb && y_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1]))
        ff_hevc_hls_filter_slice(s, x_ctb - ctb_size, y_ctb, ctb_size);

    if(!x_end)
        x_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + 1]);
    if(!y_end)
        y_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + s->sps->ctb_width]);

    if(x_slice_end && y_ctb && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->sps->ctb_width]))
        ff_hevc_hls_filter_slice(s, x_ctb , y_ctb - ctb_size, ctb_size);

    if(y_slice_end && x_ctb && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs- 1]))
        ff_hevc_hls_filter_slice(s, x_ctb - ctb_size , y_ctb, ctb_size);

    if(x_end && y_slice_end)
        ff_hevc_hls_filter_slice(s, x_ctb , y_ctb, ctb_size);
    if(y_end && x_slice_end)
        ff_hevc_hls_filter_slice(s, x_ctb , y_ctb, ctb_size);
    if(y_end && x_end)
        ff_hevc_hls_filter_slice(s, x_ctb , y_ctb, ctb_size);
}
void ff_hevc_hls_filter_slice(HEVCContext *s, int x, int y, int ctb_size)
{
    int x_slice_end = 0, y_slice_end= 0;
    deblocking_filter_CTB(s, x, y);
    int ctb_addr_rs = (x >> s->sps->log2_ctb_size) + (y >> s->sps->log2_ctb_size) * s->sps->ctb_width;

    if (s->sps->sao_enabled) {
        int x_end = x >= s->sps->width  - ctb_size;
        int y_end = y >= s->sps->height - ctb_size;
        if (y && x && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1 - s->sps->ctb_width]))
            sao_filter_CTB(s, x - ctb_size, y - ctb_size);
        if (x && y_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1]))
            sao_filter_CTB(s, x - ctb_size, y);
        if (y && x_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->sps->ctb_width])) {
            sao_filter_CTB(s, x, y - ctb_size);
            if (s->threads_type & FF_THREAD_FRAME ) {
                int i;
                s->decoded_rows[(y - ctb_size)>>s->sps->log2_ctb_size] = 1;
                for(i = 0; i < s->sps->ctb_height && s->decoded_rows[i]; i++);
                ff_thread_report_progress(&s->ref->tf, i*ctb_size, 0);
            }
        }

        if(!x_end)
            x_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + 1]);
        if(!y_end)
            y_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + s->sps->ctb_width]);

        if(x_slice_end && y && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->sps->ctb_width])) {
            sao_filter_CTB(s, x , y - ctb_size);
            if (s->threads_type & FF_THREAD_FRAME ) {
                int i; 
                s->decoded_rows[(y - ctb_size)>>s->sps->log2_ctb_size] = 1;
                for(i = 0; i < s->sps->ctb_height && s->decoded_rows[i]; i++);
                    ff_thread_report_progress(&s->ref->tf, i*ctb_size, 0);
            }
        }

        if(y_slice_end && x && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs- 1]))
            sao_filter_CTB(s, x - ctb_size , y);

        if(x_end && y_slice_end)
            sao_filter_CTB(s, x , y);

        if(y_end && x_slice_end)
            sao_filter_CTB(s, x , y);

        if (x_end && y_end) {
            sao_filter_CTB(s, x , y);
            if (s->threads_type & FF_THREAD_FRAME )
                ff_thread_report_progress(&s->ref->tf, y, 0);
        }
    } else {
        if (y && x >= s->sps->width - ctb_size)
            if (s->threads_type & FF_THREAD_FRAME ) {
                int i;
                s->decoded_rows[y>>s->sps->log2_ctb_size] = 1;
                for(i = 0; i < s->sps->ctb_height && s->decoded_rows[i]; i++);
                    ff_thread_report_progress(&s->ref->tf, i*ctb_size, 0);
            }
    }
}
#endif

static void copy_block (pixel *src, pixel * dst, ptrdiff_t bl_stride, ptrdiff_t el_stride, int ePbH, int ePbW ) {
    int i;

    for (i = 0; i < ePbH ; i++) {
        memcpy(dst, src, ePbW * sizeof(pixel));
        src += bl_stride;
        dst += el_stride;
    }
}

static void upsample_block_luma(HEVCContext *s, HEVCFrame *ref0, int x0, int y0) {
    uint8_t *src,  *dst = ref0->frame->data[0];
    int ctb_size  = 1<<s->sps->log2_ctb_size;
    int el_width  =  s->sps->width;
    int el_height =  s->sps->height;
    int bl_width  =  s->BL_frame->frame->coded_width;
    int bl_height =  s->BL_frame->frame->coded_height;
    int bl_stride =  s->BL_frame->frame->linesize[0];
    int el_stride =  ref0->frame->linesize[0];
    int ePbW = x0 + ctb_size > el_width  ? el_width  - x0 : ctb_size ;
    int ePbH = y0 + ctb_size > el_height ? el_height - y0 : ctb_size;

    if (s->up_filter_inf.idx == SNR) { /* x1 quality (SNR) scalability */
        copy_block (s->BL_frame->frame->data[0] + y0 * bl_stride + x0,
                    ref0->frame->data[0] + y0 * el_stride + x0,
                    bl_stride, el_stride, ePbH, ePbW );
    } else { /* spatial scalability */
        int bl_edge_bottom, bl_edge_right, ret;
        int bPbW = ((( ePbW + 1 )*s->up_filter_inf.scaleXLum + s->up_filter_inf.addXLum) >> 12) >> 4; /*    FIXME: check if this method is correct  */
        int bPbH = ((( ePbH + 2 ) *s->up_filter_inf.scaleYLum + s->up_filter_inf.addYLum) >> 12) >> 4;

        int bl_x = (( (x0  - s->sps->pic_conf_win.left_offset) * s->up_filter_inf.scaleXLum + s->up_filter_inf.addXLum) >> 12) >> 4;
        int bl_y = (( (y0  - s->sps->pic_conf_win.top_offset) * s->up_filter_inf.scaleYLum + s->up_filter_inf.addYLum) >> 12) >> 4;
        int bl_edge_left   =  (MAX_EDGE - 1 - bl_x ) > 0 ?  0: MAX_EDGE - 1;
        int bl_edge_top    = ( MAX_EDGE - 1 - bl_y ) > 0 ?  0: MAX_EDGE - 1;
        int ref_layer_id = s->vps->m_refLayerId[s->nuh_layer_id][0];
        int16_t *tmp0;

        if(bl_x+bPbW > bl_width)
            bPbW = bl_width -bl_x;

        if(bl_y+bPbH > bl_height)
            bPbH = bl_height-bl_y;

        bl_edge_right  =  (MAX_EDGE > (bl_width  - bl_x - bPbW))  ? bl_width  - bl_x - bPbW: MAX_EDGE;
        bl_edge_bottom =  (MAX_EDGE > (bl_height - bl_y - bPbH))  ? bl_height - bl_y - bPbH: MAX_EDGE;

        src = s->BL_frame->frame->data[0] + (bl_y - bl_edge_top) * bl_stride + (bl_x - bl_edge_left);
        ret = s->vdsp.emulated_edge_up_h(src , bl_stride, &s->sps->scaled_ref_layer_window[ref_layer_id],
                                         bPbW + bl_edge_left + bl_edge_right, bPbH + bl_edge_top + bl_edge_bottom,
                                         bl_edge_left , bl_edge_right, MAX_EDGE-1);

        if(ret)
            src += (MAX_EDGE-1);

        tmp0 = s->HEVClc->edge_emu_buffer_up_v+ ((MAX_EDGE - 1) * MAX_EDGE_BUFFER_STRIDE);

        s->hevcdsp.upsample_filter_block_luma_h[s->up_filter_inf.idx](tmp0, MAX_EDGE_BUFFER_STRIDE, src, bl_stride, x0, bl_x,
                                                                      ePbW, bPbH + bl_edge_top + bl_edge_bottom, el_width,
                                                                      &s->sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);

        ret = s->vdsp.emulated_edge_up_v(tmp0, MAX_EDGE_BUFFER_STRIDE, &s->sps->scaled_ref_layer_window[ref_layer_id],
                                         ePbW, bPbH + bl_edge_top + bl_edge_bottom, x0, bl_edge_top ,
                                         bl_edge_bottom, el_width, MAX_EDGE - 1);
        if(ret)
            tmp0 += ((MAX_EDGE - 1) * MAX_EDGE_BUFFER_STRIDE);

        s->hevcdsp.upsample_filter_block_luma_v[s->up_filter_inf.idx](dst , ref0->frame->linesize[0], tmp0 , MAX_EDGE_BUFFER_STRIDE,
                                                                      bl_y , x0, y0, ePbW, ePbH, el_width, el_height,
                                                                      &s->sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);

    }
    s->is_upsampled[(y0 / ctb_size * s->sps->ctb_width) + (x0 / ctb_size)] = 1;
}

static void upsample_block_mc(HEVCContext *s, HEVCFrame *ref0, int x0, int y0) {
    uint8_t   *src;
    int16_t   *tmp0;

    int el_width  =  s->sps->width>>1;
    int el_height =  s->sps->height>>1;
    int bl_width  =  s->BL_frame->frame->coded_width  >>1;
    int bl_height  = s->BL_frame->frame->coded_height  > el_height ? s->BL_frame->frame->coded_height>>1:el_height>>1;

    int ret, cr, bl_edge_top0;
    int ctb_size = 1<<(s->sps->log2_ctb_size-1);

    int ePbW = x0 + ctb_size > el_width  ? el_width  - x0 : ctb_size ;
    int ePbH = y0 + ctb_size > el_height ? el_height - y0 : ctb_size;
    int bl_stride = s->BL_frame->frame->linesize[1];
    int el_stride = ref0->frame->linesize[1];

    if (s->up_filter_inf.idx == SNR) {
        for (cr = 1; cr <= 2; cr++)
            copy_block(s->BL_frame->frame->data[cr] + y0 * bl_stride + x0,
                       ref0->frame->data[cr] + y0 * el_stride + x0,
                       bl_stride, el_stride, ePbH, ePbW );
    } else {
        int bl_edge_right, bl_edge_bottom;
        int bPbW = ((( ePbW + 1 ) * s->up_filter_inf.scaleXLum + s->up_filter_inf.addXLum) >> 12)  >> 4;    /*    FIXME: check if this method is correct  */
        int bPbH = ((( ePbH + 2 ) * s->up_filter_inf.scaleYLum + s->up_filter_inf.addYLum) >> 12)  >> 4;

        int bl_x = (((  x0 - (s->sps->pic_conf_win.left_offset>>1)) * s->up_filter_inf.scaleXLum + s->up_filter_inf.addXLum) >> 12)      >> 4;
        int bl_y = (((( y0 - (s->sps->pic_conf_win.top_offset >>1)) * s->up_filter_inf.scaleYLum + s->up_filter_inf.addYLum) >> 12) -4 ) >> 4;

        int bl_edge_left  = (MAX_EDGE_CR - 1 - bl_x) > 0 ?  0 : MAX_EDGE_CR - 1;
        int bl_edge_top   = (MAX_EDGE_CR - 1 - bl_y) > 0 ?  0 : MAX_EDGE_CR - 1;
        int ref_layer_id = s->vps->m_refLayerId[s->nuh_layer_id][0];

        bPbW = bl_x+bPbW > bl_width  ? bl_width -bl_x:bPbW;
        bPbH = bl_y+bPbH > bl_height ? bl_height-bl_y:bPbH;

        bl_edge_top0 = bl_y < 0 ? bl_y:0;       // This for -4 the top can go in negative

        bl_edge_right  = MAX_EDGE_CR < (bl_width -  bl_x - bPbW) ? MAX_EDGE_CR:bl_width  - bl_x - bPbW;
        bl_edge_bottom = MAX_EDGE_CR < (bl_height - bl_y - bPbH) ? MAX_EDGE_CR:bl_height - bl_y - bPbH;

        for (cr = 1; cr <= 2; cr++) {
            src = s->BL_frame->frame->data[cr]+ (bl_y-bl_edge_top)*bl_stride+(bl_x-bl_edge_left);
            ret = s->vdsp.emulated_edge_up_h(   src , bl_stride,
                                             &s->sps->scaled_ref_layer_window[ref_layer_id],
                                             bPbW + bl_edge_left+bl_edge_right, bPbH + bl_edge_top + bl_edge_bottom,
                                             bl_edge_left , bl_edge_right, MAX_EDGE_CR-1);
            if(ret)
                src += (MAX_EDGE_CR - 1);

            tmp0 = s->HEVClc->edge_emu_buffer_up_v+ ((MAX_EDGE_CR - 1) * MAX_EDGE_BUFFER_STRIDE);

            s->hevcdsp.upsample_filter_block_cr_h[s->up_filter_inf.idx](  tmp0, MAX_EDGE_BUFFER_STRIDE, src, bl_stride,
                                                                        x0, bl_x, ePbW, bPbH + bl_edge_top + bl_edge_bottom, el_width,
                                                                        &s->sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);

            ret = s->vdsp.emulated_edge_up_v(tmp0, MAX_EDGE_BUFFER_STRIDE, &s->sps->scaled_ref_layer_window[ref_layer_id],
                                             ePbW, bPbH + bl_edge_top + bl_edge_bottom, x0, bl_edge_top+bl_edge_top0 , bl_edge_bottom,
                                             el_width, MAX_EDGE_CR-1);

            if(ret)
                tmp0 += ((MAX_EDGE_CR-1)*MAX_EDGE_BUFFER_STRIDE);

            s->hevcdsp.upsample_filter_block_cr_v[s->up_filter_inf.idx](  ref0->frame->data[cr] , el_stride, tmp0 , MAX_EDGE_BUFFER_STRIDE,
                                                                        bl_y, x0, y0, ePbW, ePbH, el_width, el_height,
                                                                        &s->sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);
        }
    }
}

void ff_upscale_mv_block(HEVCContext *s, int ctb_x, int ctb_y) {
    int xEL, yEL, xBL, yBL, list, Ref_pre_unit, pre_unit, pre_unit_col;
    int pic_width_in_min_pu = s->sps->width>>s->sps->log2_min_pu_size;
    int pic_height_in_min_pu = s->sps->height>>s->sps->log2_min_pu_size;
    int pic_width_in_min_puBL = s->BL_frame->frame->coded_width >> s->sps->log2_min_pu_size;
    int ctb_size = 1 << s->sps->log2_ctb_size;
    int nb_list = s->sh.slice_type==B_SLICE ? 2:1;
    HEVCFrame *refBL = s->BL_frame;
    HEVCFrame *refEL = s->inter_layer_ref;

    for(yEL=ctb_y; yEL < ctb_y+ctb_size && yEL<s->sps->height; yEL+=16) {
        for(xEL=ctb_x; xEL < ctb_x+ctb_size && xEL<s->sps->width; xEL+=16) {
            xBL = (((av_clip_c(xEL+8, 0, s->sps->width -1)  - s->sps->pic_conf_win.left_offset)*s->up_filter_inf.scaleXLum + (1<<15)) >> 16) + 4;
            yBL = (((av_clip_c(yEL+8, 0, s->sps->height -1) - s->sps->pic_conf_win.top_offset )*s->up_filter_inf.scaleYLum + (1<<15)) >> 16) + 4;
            pre_unit = ((yEL>>s->sps->log2_min_pu_size)*pic_width_in_min_pu) + (xEL>>s->sps->log2_min_pu_size);
            if(xBL < s->BL_frame->frame->coded_width && yBL < s->BL_frame->frame->coded_height) {
                xBL >>= 4;
                xBL <<= 4-s->sps->log2_min_pu_size; // 4 <==> xBL & 0xFFFFFFF0
                yBL >>= 4;
                yBL <<= 4-s->sps->log2_min_pu_size; // 4 <==> yBL & 0xFFFFFFF0
                Ref_pre_unit = (yBL*pic_width_in_min_puBL)+xBL;
                if(refBL->tab_mvf[Ref_pre_unit].pred_flag) {
                    if (s->up_filter_inf.idx == SNR) {
                        memcpy(&refEL->tab_mvf[pre_unit], &refBL->tab_mvf[Ref_pre_unit], sizeof(MvField));
                    } else {

                        for( list=0; list < nb_list; list++) {
                            refEL->tab_mvf[pre_unit].mv[list].x  = av_clip_c( (s->sh.ScalingFactor[s->nuh_layer_id][0] * refBL->tab_mvf[Ref_pre_unit].mv[list].x + 127 + (s->sh.ScalingFactor[s->nuh_layer_id][0] * refBL->tab_mvf[Ref_pre_unit].mv[list].x < 0)) >> 8 , -32768, 32767);
                            refEL->tab_mvf[pre_unit].mv[list].y = av_clip_c( (s->sh.ScalingFactor[s->nuh_layer_id][1] * refBL->tab_mvf[Ref_pre_unit].mv[list].y + 127 + (s->sh.ScalingFactor[s->nuh_layer_id][1] * refBL->tab_mvf[Ref_pre_unit].mv[list].y < 0)) >> 8, -32768, 32767);
                            refEL->tab_mvf[pre_unit].ref_idx[list] = refBL->tab_mvf[Ref_pre_unit].ref_idx[list];
#ifdef TEST_MV_POC
                            refEL->tab_mvf[pre_unit].poc[list] = refBL->tab_mvf[Ref_pre_unit].poc[list];
#endif
                            refEL->tab_mvf[pre_unit].pred_flag = refBL->tab_mvf[Ref_pre_unit].pred_flag;
                        }
                    }
                } else
                    memset(&refEL->tab_mvf[pre_unit], 0, sizeof(MvField));


            } else
                memset(&refEL->tab_mvf[pre_unit], 0, sizeof(MvField));

            if( ((xEL+1)>>s->sps->log2_min_pu_size) < pic_width_in_min_pu && ((yEL+1)>>s->sps->log2_min_pu_size) < pic_height_in_min_pu) {
                pre_unit_col = (((yEL+1)>>s->sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL+1)>>s->sps->log2_min_pu_size);
                memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
            }
            if( ((xEL+1)>>s->sps->log2_min_pu_size) < pic_width_in_min_pu) {
                pre_unit_col = ((yEL>>s->sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL+1)>>s->sps->log2_min_pu_size);
                memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
            }
            if( ((yEL+1)>>s->sps->log2_min_pu_size) < pic_height_in_min_pu) {
                pre_unit_col = (((yEL+1)>>s->sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL)>>s->sps->log2_min_pu_size);
                memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
            }
        }
    }
}

void ff_upsample_block(HEVCContext *s, HEVCFrame *ref0, int x0, int y0, int nPbW, int nPbH) {

    int ctb_size =  1<<s->sps->log2_ctb_size;
    int log2_ctb =  s->sps->log2_ctb_size;
    int ctb_x0   =  (av_clip(x0, 0, s->sps->width) >> log2_ctb) << log2_ctb;
    int ctb_y0   =  (av_clip(y0, 0, s->sps->height) >> log2_ctb) << log2_ctb;

    if ((x0 - ctb_x0) < MAX_EDGE &&
        ctb_x0 > ctb_size        &&
        !s->is_upsampled[(ctb_y0 / ctb_size * s->sps->ctb_width)+((ctb_x0 - ctb_size) / ctb_size)]){
        ff_upscale_mv_block(s, ctb_x0 - ctb_size            , ctb_y0);
        upsample_block_mc  (s, ref0, (ctb_x0-ctb_size) >> 1 , ctb_y0 >> 1);
        upsample_block_luma(s, ref0, ctb_x0-ctb_size        , ctb_y0);
    }

    if ((y0 - ctb_y0) < MAX_EDGE &&
        ctb_y0 > ctb_size &&
        !s->is_upsampled[((ctb_y0 - ctb_size) / ctb_size * s->sps->ctb_width) + (ctb_x0 / ctb_size)]){
        ff_upscale_mv_block(s, ctb_x0            , ctb_y0 - ctb_size);
        upsample_block_mc  (s, ref0, ctb_x0 >> 1 , (ctb_y0 - ctb_size) >> 1);
        upsample_block_luma(s, ref0, ctb_x0      , ctb_y0 - ctb_size);
    }

    if(!s->is_upsampled[(ctb_y0 / ctb_size * s->sps->ctb_width) + (ctb_x0 / ctb_size)]){
        ff_upscale_mv_block(s, ctb_x0           , ctb_y0);
        upsample_block_mc  (s, ref0, ctb_x0 >> 1, ctb_y0 >> 1);
        upsample_block_luma(s, ref0, ctb_x0     , ctb_y0);

    }

    if((((x0 + nPbW + MAX_EDGE) >> log2_ctb) << log2_ctb) > ctb_x0 && ((ctb_x0 + ctb_size) < s->sps->width) &&
       !s->is_upsampled[(ctb_y0 / ctb_size * s->sps->ctb_width) + ((ctb_x0 + ctb_size) / ctb_size)]){
        ff_upscale_mv_block(s, ctb_x0 + ctb_size, ctb_y0);
        upsample_block_mc  (s, ref0, (ctb_x0 + ctb_size) >> 1, ctb_y0 >> 1);
        upsample_block_luma(s, ref0, ctb_x0 + ctb_size       , ctb_y0);
    }

    if((((y0 + nPbH + MAX_EDGE) >> log2_ctb) << log2_ctb) > ctb_y0 &&
       ((ctb_y0 + ctb_size) < s->sps->height)) {
        if (!s->is_upsampled[((ctb_y0 + ctb_size) / ctb_size * s->sps->ctb_width) + (ctb_x0 / ctb_size)]){
            if (s->threads_type & FF_THREAD_FRAME ) {
                int bl_y = ctb_y0 + ctb_size + ctb_size * 2 + 9;
                bl_y = (( (bl_y  - s->sps->pic_conf_win.top_offset) * s->up_filter_inf.scaleYLum + s->up_filter_inf.addYLum) >> 12) >> 4;
                ff_thread_await_progress(&s->BL_frame->tf, bl_y, 0);
            }
            ff_upscale_mv_block(s, ctb_x0, ctb_y0 + ctb_size);
            upsample_block_mc  (s, ref0, ctb_x0 >> 1, (ctb_y0 + ctb_size) >> 1);
            upsample_block_luma(s, ref0, ctb_x0     , ctb_y0 + ctb_size);
        }
        if((((x0 + nPbW + MAX_EDGE) >> log2_ctb) << log2_ctb) > ctb_x0 && ((ctb_x0 + ctb_size) < s->sps->width) &&
           !s->is_upsampled[((ctb_y0 + ctb_size) / ctb_size * s->sps->ctb_width) + ((ctb_x0 + ctb_size) / ctb_size)]){
            ff_upscale_mv_block(s, ctb_x0 + ctb_size, ctb_y0 + ctb_size);
            upsample_block_mc  (s, ref0, (ctb_x0 + ctb_size) >> 1, (ctb_y0 + ctb_size) >> 1);
            upsample_block_luma(s, ref0, (ctb_x0 + ctb_size)    , ctb_y0 + ctb_size);
        }
    }
}
