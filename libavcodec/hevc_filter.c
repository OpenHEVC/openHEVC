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
//temp
#include "h264dec.h"

//TODO move intrinsics to x86 funcions
/*#if HAVE_SSE2
#include <emmintrin.h>
#endif
#if HAVE_SSSE3
#include <tmmintrin.h>
#endif
#if HAVE_SSE42
#include <smmintrin.h>
#endif
*/
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
        offset = s->ps.pps->pps_cb_qp_offset;
    else
        offset = s->ps.pps->pps_cr_qp_offset;

    qp_i = av_clip(qp_y + offset, 0, 57);
    if (s->ps.sps->chroma_format_idc == 1) {
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
    int ctb_size_mask        = (1 << s->ps.sps->log2_ctb_size) - 1;
    int MinCuQpDeltaSizeMask = (1 << (s->ps.sps->log2_ctb_size -
                                      s->ps.pps->diff_cu_qp_delta_depth)) - 1;
    int xQgBase              = xBase - (xBase & MinCuQpDeltaSizeMask);
    int yQgBase              = yBase - (yBase & MinCuQpDeltaSizeMask);
    int min_cb_width         = s->ps.sps->min_cb_width;
    int x_cb                 = xQgBase >> s->ps.sps->log2_min_cb_size;
    int y_cb                 = yQgBase >> s->ps.sps->log2_min_cb_size;
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

    av_assert2(qPy_a >= -s->ps.sps->qp_bd_offset && qPy_a < 52);
    av_assert2(qPy_b >= -s->ps.sps->qp_bd_offset && qPy_b < 52);

    return (qPy_a + qPy_b + 1) >> 1;
}

void ff_hevc_set_qPy(HEVCContext *s, int xBase, int yBase, int log2_cb_size)
{
    int qp_y = get_qPy_pred(s, xBase, yBase, log2_cb_size);

    if (s->HEVClc->tu.cu_qp_delta != 0) {
        int off = s->ps.sps->qp_bd_offset;
        s->HEVClc->qp_y = FFUMOD(qp_y + s->HEVClc->tu.cu_qp_delta + 52 + 2 * off,
                                 52 + off) - off;
    } else
        s->HEVClc->qp_y = qp_y;
}

static int get_qPy(HEVCContext *s, int xC, int yC)
{
    int log2_min_cb_size  = s->ps.sps->log2_min_cb_size;
    int x                 = xC >> log2_min_cb_size;
    int y                 = yC >> log2_min_cb_size;
    return s->qp_y_tab[x + y * s->ps.sps->min_cb_width];
}

static void copy_CTB(uint8_t *dst, const uint8_t *src,
                     int width, int height, int stride_dst, int stride_src)
{
    int i;

    for (i = 0; i < height; i++) {
        memcpy(dst, src, width);
        dst += stride_dst;
        src += stride_src;
    }
}

#if defined(USE_SAO_SMALL_BUFFER)
static void copy_pixel(uint8_t *dst, const uint8_t *src, int pixel_shift)
{
    if (pixel_shift)
        *(uint16_t *)dst = *(uint16_t *)src;
    else
        *dst = *src;

}

static void copy_vert(uint8_t *dst, const uint8_t *src,
                      int pixel_shift, int height, 
                      int stride_dst, int stride_src)
{
    int i;
    if (pixel_shift == 0) {
        for (i = 0; i < height; i++) {
            *dst = *src;
            dst += stride_dst;
            src += stride_src;
        }
    } else {
        for (i = 0; i < height; i++) {
            *(uint16_t *)dst = *(uint16_t *)src;
            dst += stride_dst;
            src += stride_src;
        }
    }
}

static void copy_CTB_to_hv(HEVCContext *s, const uint8_t *src,
                           int stride_src, int x, int y, int width, int height,
                           int c_idx, int x_ctb, int y_ctb)
{
    int sh = s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA];
    int w = s->ps.sps->width >> s->ps.sps->hshift[c_idx];
    int h = s->ps.sps->height >> s->ps.sps->vshift[c_idx];

    /* copy horizontal edges */
    memcpy(s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb) * w + x) << sh),
        src, width << sh);
    memcpy(s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb + 1) * w + x) << sh),
        src + stride_src * (height - 1), width << sh);
    
    /* copy vertical edges */
    copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb) * h + y) << sh), src, sh, height, 1 << sh, stride_src);
        
    copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 1) * h + y) << sh), src + ((width - 1) << sh), sh, height, 1 << sh, stride_src);
}
#endif

static void restore_tqb_pixels(HEVCContext *s, 
                               uint8_t *src1, const uint8_t *dst1,
                               ptrdiff_t stride_src, ptrdiff_t stride_dst,
                               int x0, int y0, int width, int height, int c_idx)
{
    if ( s->ps.pps->transquant_bypass_enable_flag ||
            (s->ps.sps->pcm.loop_filter_disable_flag && s->ps.sps->pcm_enabled_flag)) {
        int x, y;
        int min_pu_size  = 1 << s->ps.sps->log2_min_pu_size;
        int hshift       = s->ps.sps->hshift[c_idx];
        int vshift       = s->ps.sps->vshift[c_idx];
        int x_min        = ((x0         ) >> s->ps.sps->log2_min_pu_size);
        int y_min        = ((y0         ) >> s->ps.sps->log2_min_pu_size);
        int x_max        = ((x0 + width ) >> s->ps.sps->log2_min_pu_size);
        int y_max        = ((y0 + height) >> s->ps.sps->log2_min_pu_size);
        int len          = (min_pu_size >> hshift) << s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA];
        for (y = y_min; y < y_max; y++) {
            for (x = x_min; x < x_max; x++) {
                if (s->is_pcm[y * s->ps.sps->min_pu_width + x]) {
                    int n;
                    uint8_t *src = src1 + (((y << s->ps.sps->log2_min_pu_size) - y0) >> vshift) * stride_src + ((((x << s->ps.sps->log2_min_pu_size) - x0) >> hshift) << s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA]);
                    const uint8_t *dst = dst1 + (((y << s->ps.sps->log2_min_pu_size) - y0) >> vshift) * stride_dst + ((((x << s->ps.sps->log2_min_pu_size) - x0) >> hshift) << s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA]);
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

#define CTB(tab, x, y) ((tab)[(y) * s->ps.sps->ctb_width + (x)])

static void sao_filter_CTB(HEVCContext *s, int x, int y)
{
#if defined(USE_SAO_SMALL_BUFFER)
    HEVCLocalContext *lc = s->HEVClc;
#endif
    int c_idx;
    int edges[4];  // 0 left 1 top 2 right 3 bottom
    int x_ctb                = x >> s->ps.sps->log2_ctb_size;
    int y_ctb                = y >> s->ps.sps->log2_ctb_size;
    int ctb_addr_rs          = y_ctb * s->ps.sps->ctb_width + x_ctb;
    int ctb_addr_ts          = s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs];
    SAOParams *sao           = &CTB(s->sao, x_ctb, y_ctb);
    // flags indicating unfilterable edges
    uint8_t vert_edge[]      = { 0, 0 };
    uint8_t horiz_edge[]     = { 0, 0 };
    uint8_t diag_edge[]      = { 0, 0, 0, 0 };
    uint8_t lfase            = CTB(s->filter_slice_edges, x_ctb, y_ctb);
    uint8_t no_tile_filter   = s->ps.pps->tiles_enabled_flag &&
                               !s->ps.pps->loop_filter_across_tiles_enabled_flag;
    uint8_t restore          = no_tile_filter || !lfase;
    uint8_t left_tile_edge   = 0;
    uint8_t right_tile_edge  = 0;
    uint8_t up_tile_edge     = 0;
    uint8_t bottom_tile_edge = 0;

    edges[0]   = x_ctb == 0;
    edges[1]   = y_ctb == 0;
    edges[2]   = x_ctb == s->ps.sps->ctb_width  - 1;
    edges[3]   = y_ctb == s->ps.sps->ctb_height - 1;

    if (restore) {
        if (!edges[0]) {
            left_tile_edge  = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs-1]];
            vert_edge[0]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb - 1, y_ctb)) || left_tile_edge;
        }
        if (!edges[2]) {
            right_tile_edge = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs+1]];
            vert_edge[1]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb + 1, y_ctb)) || right_tile_edge;
        }
        if (!edges[1]) {
            up_tile_edge     = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs - s->ps.sps->ctb_width]];
            horiz_edge[0]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb, y_ctb - 1)) || up_tile_edge;
        }
        if (!edges[3]) {
            bottom_tile_edge = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs + s->ps.sps->ctb_width]];
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

    for (c_idx = 0; c_idx < (s->ps.sps->chroma_format_idc ? 3 : 1); c_idx++) {
        int x0       = x >> s->ps.sps->hshift[c_idx];
        int y0       = y >> s->ps.sps->vshift[c_idx];
        int stride_src = s->frame->linesize[c_idx];
        int ctb_size_h = (1 << (s->ps.sps->log2_ctb_size)) >> s->ps.sps->hshift[c_idx];
        int ctb_size_v = (1 << (s->ps.sps->log2_ctb_size)) >> s->ps.sps->vshift[c_idx];
        int width    = FFMIN(ctb_size_h, (s->ps.sps->width  >> s->ps.sps->hshift[c_idx]) - x0);
        int height   = FFMIN(ctb_size_v, (s->ps.sps->height >> s->ps.sps->vshift[c_idx]) - y0);
        uint8_t *src = &s->frame->data[c_idx][y0 * stride_src + (x0 << s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA])];
#if defined(USE_SAO_SMALL_BUFFER)
        int stride_dst = ((1 << (FFMAX(4, s->ps.sps->log2_ctb_size))) + 32) << s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA];
        uint8_t *dst = lc->sao_pixel_buffer + (1 * stride_dst) + 16;
#else
        int stride_dst = s->sao_frame->linesize[c_idx];
        uint8_t *dst = &s->sao_frame->data[c_idx][y0 * stride_dst + (x0 << s->ps.sps->pixel_shift)];
#endif

        switch (sao->type_idx[c_idx]) {
        case SAO_BAND:
            copy_CTB(dst, src, width << s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA], height, stride_dst, stride_src);
#if defined(USE_SAO_SMALL_BUFFER)
            copy_CTB_to_hv(s, src, stride_src, x0, y0, width, height, c_idx,
                           x_ctb, y_ctb);
#endif
            s->hevcdsp.sao_band_filter(src, dst,
                                       stride_src, stride_dst,
                                       sao,
                                       edges, width,
                                       height, c_idx);
            restore_tqb_pixels(s, src, dst, stride_src, stride_dst,
                               x, y, width, height, c_idx);
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        case SAO_EDGE:
        {
#if defined(USE_SAO_SMALL_BUFFER)
            int w = s->ps.sps->width >> s->ps.sps->hshift[c_idx];
            int h = s->ps.sps->height >> s->ps.sps->vshift[c_idx];
            int left_edge = edges[0];
            int top_edge = edges[1];
            int right_edge = edges[2];
            int bottom_edge = edges[3];
            int sh = s->ps.sps->pixel_shift[c_idx ? CHANNEL_TYPE_CHROMA:CHANNEL_TYPE_LUMA];
            int left_pixels, right_pixels;

            if (!top_edge) {
                int left = 1 - left_edge;
                int right = 1 - right_edge;
                const uint8_t *src1[2];
                uint8_t *dst1;
                int src_idx, pos;

                dst1 = dst - stride_dst - (left << sh);
                src1[0] = src - stride_src - (left << sh);
                src1[1] = s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb - 1) * w + x0 - left) << sh);
                pos = 0;
                if (left) {
                    src_idx = (CTB(s->sao, x_ctb-1, y_ctb-1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1, src1[src_idx], sh);
                    pos += (1 << sh);
                }
                src_idx = (CTB(s->sao, x_ctb, y_ctb-1).type_idx[c_idx] ==
                           SAO_APPLIED);
                memcpy(dst1 + pos, src1[src_idx] + pos, width << sh);
                if (right) {
                    pos += width << sh;
                    src_idx = (CTB(s->sao, x_ctb+1, y_ctb-1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 + pos, src1[src_idx] + pos, sh);
                }
            }
            if (!bottom_edge) {
                int left = 1 - left_edge;
                int right = 1 - right_edge;
                const uint8_t *src1[2];
                uint8_t *dst1;
                int src_idx, pos;

                dst1 = dst + height * stride_dst - (left << sh);
                src1[0] = src + height * stride_src - (left << sh);
                src1[1] = s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb + 2) * w + x0 - left) << sh);
                pos = 0;
                if (left) {
                    src_idx = (CTB(s->sao, x_ctb-1, y_ctb+1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1, src1[src_idx], sh);
                    pos += (1 << sh);
                }
                src_idx = (CTB(s->sao, x_ctb, y_ctb+1).type_idx[c_idx] ==
                           SAO_APPLIED);
                memcpy(dst1 + pos, src1[src_idx] + pos, width << sh);
                if (right) {
                    pos += width << sh;
                    src_idx = (CTB(s->sao, x_ctb+1, y_ctb+1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 + pos, src1[src_idx] + pos, sh);
                }
            }
            left_pixels = 0;
            if (!left_edge) {
                if (CTB(s->sao, x_ctb-1, y_ctb).type_idx[c_idx] == SAO_APPLIED) {
                    copy_vert(dst - (1 << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb - 1) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    left_pixels = 1;
                }
            }
            right_pixels = 0;
            if (!right_edge) {
                if (CTB(s->sao, x_ctb+1, y_ctb).type_idx[c_idx] == SAO_APPLIED) {
                    copy_vert(dst + (width << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 2) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    right_pixels = 1;
                }
            }

            copy_CTB(dst - (left_pixels << sh),
                     src - (left_pixels << sh),
                     (width + left_pixels + right_pixels) << sh,
                     height, stride_dst, stride_src);

            copy_CTB_to_hv(s, src, stride_src, x0, y0, width, height, c_idx,
                           x_ctb, y_ctb);
#else
            uint8_t left_pixels;
            /* get the CTB edge pixels from the SAO pixel buffer */
            left_pixels = !edges[0] && (CTB(s->sao, x_ctb-1, y_ctb).type_idx[c_idx] != SAO_APPLIED);
            if (!edges[1]) {
                uint8_t top_left  = !edges[0] && (CTB(s->sao, x_ctb-1, y_ctb-1).type_idx[c_idx] != SAO_APPLIED);
                uint8_t top_right = !edges[2] && (CTB(s->sao, x_ctb+1, y_ctb-1).type_idx[c_idx] != SAO_APPLIED);
                if (CTB(s->sao, x_ctb  , y_ctb-1).type_idx[c_idx] == 0)
                    memcpy( dst - stride_dst - (top_left << s->ps.sps->pixel_shift),
                            src - stride_src - (top_left << s->ps.sps->pixel_shift),
                            (top_left + width + top_right) << s->ps.sps->pixel_shift);
                else {
                    if (top_left)
                        memcpy( dst - stride_dst - (1 << s->ps.sps->pixel_shift),
                                src - stride_src - (1 << s->ps.sps->pixel_shift),
                                1 << s->ps.sps->pixel_shift);
                    if(top_right)
                        memcpy( dst - stride_dst + (width << s->ps.sps->pixel_shift),
                                src - stride_src + (width << s->ps.sps->pixel_shift),
                                1 << s->ps.sps->pixel_shift);
                }
            }
            if (!edges[3]) {                                                                // bottom and bottom right
                uint8_t bottom_left = !edges[0] && (CTB(s->sao, x_ctb-1, y_ctb+1).type_idx[c_idx] != SAO_APPLIED);
                memcpy( dst + height * stride_dst - (bottom_left << s->ps.sps->pixel_shift),
                        src + height * stride_src - (bottom_left << s->ps.sps->pixel_shift),
                        (width + 1 + bottom_left) << s->ps.sps->pixel_shift);
            }
            copy_CTB(dst - (left_pixels << s->ps.sps->pixel_shift),
                     src - (left_pixels << s->ps.sps->pixel_shift),
                     (width + 1 + left_pixels) << s->ps.sps->pixel_shift, height, stride_dst, stride_src);
#endif
            /* XXX: could handle the restoration here to simplify the
               DSP functions */
            s->hevcdsp.sao_edge_filter(src, dst, stride_src, stride_dst, sao, width, height, c_idx);
            s->hevcdsp.sao_edge_restore[restore](src, dst,
                                                stride_src, stride_dst,
                                                sao,
                                                edges, width,
                                                height, c_idx,
                                                vert_edge,
                                                horiz_edge,
                                                diag_edge);
            restore_tqb_pixels(s, src, dst, stride_src, stride_dst,
                               x, y, width, height, c_idx);
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        }
        }
    }
}

static int get_pcm(HEVCContext *s, int x, int y)
{
    int log2_min_pu_size = s->ps.sps->log2_min_pu_size;
    int x_pu, y_pu;

    if (x < 0 || y < 0)
        return 2;

    x_pu = x >> log2_min_pu_size;
    y_pu = y >> log2_min_pu_size;

    if (x_pu >= s->ps.sps->min_pu_width || y_pu >= s->ps.sps->min_pu_height)
        return 2;
    return s->is_pcm[y_pu * s->ps.sps->min_pu_width + x_pu];
}

#define TC_CALC(qp, bs)                                                 \
    tctable[av_clip((qp) + DEFAULT_INTRA_TC_OFFSET * ((bs) - 1) +       \
                    (tc_offset >> 1 << 1),                              \
                    0, MAX_QP + DEFAULT_INTRA_TC_OFFSET)]

static void deblocking_filter_CTB(HEVCContext *s, int x0, int y0)
{
    uint8_t *src;
    int x, y;
    //int chroma;
    int c_tc[2], tc[2], beta;
    uint8_t no_p[2] = { 0 };
    uint8_t no_q[2] = { 0 };

    int log2_ctb_size = s->ps.sps->log2_ctb_size;
    int x_end, x_end2, y_end;
    int ctb_size        = 1 << log2_ctb_size;
    int ctb             = (x0 >> log2_ctb_size) +
                          (y0 >> log2_ctb_size) * s->ps.sps->ctb_width;
    int8_t cur_tc_offset   = s->deblock[ctb].tc_offset;
    int8_t cur_beta_offset = s->deblock[ctb].beta_offset;
    int8_t left_tc_offset, left_beta_offset;
    int8_t tc_offset, beta_offset;
    int pcmf = (s->ps.sps->pcm_enabled_flag &&
                s->ps.sps->pcm.loop_filter_disable_flag) ||
               s->ps.pps->transquant_bypass_enable_flag;

    if (x0) {
        left_tc_offset   = s->deblock[ctb - 1].tc_offset;
        left_beta_offset = s->deblock[ctb - 1].beta_offset;
    } else {
        left_tc_offset   = 0;
        left_beta_offset = 0;
    }

    x_end = x0 + ctb_size;
    if (x_end > s->ps.sps->width)
        x_end = s->ps.sps->width;
    y_end = y0 + ctb_size;
    if (y_end > s->ps.sps->height)
        y_end = s->ps.sps->height;

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
                src     = &s->frame->data[LUMA][y * s->frame->linesize[LUMA] + (x << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA])];
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
    if (s->ps.sps->chroma_format_idc) {
        int c_tc2[2];
        uint8_t* src2;
        int h = 1 << s->ps.sps->hshift[1];
        int v = 1 << s->ps.sps->vshift[1];
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
                    src      = &s->frame->data[1][(y >> s->ps.sps->vshift[1]) * s->frame->linesize[1] + ((x >> s->ps.sps->hshift[1]) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];
                    src2     = &s->frame->data[2][(y >> s->ps.sps->vshift[2]) * s->frame->linesize[2] + ((x >> s->ps.sps->hshift[2]) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];
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
    if (x_end != s->ps.sps->width)
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
                src     = &s->frame->data[LUMA][y * s->frame->linesize[LUMA] + (x << s->ps.sps->pixel_shift[CHANNEL_TYPE_LUMA])];
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
    if (s->ps.sps->chroma_format_idc) {
        int c_tc2[2];
        uint8_t* src2;
        int h = 1 << s->ps.sps->hshift[1];
        int v = 1 << s->ps.sps->vshift[1];
        if (x_end2 != s->ps.sps->width)
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
                    src       = &s->frame->data[1][(y >> s->ps.sps->vshift[1]) * s->frame->linesize[1] + ((x >> s->ps.sps->hshift[1]) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];
                    src2      = &s->frame->data[2][(y >> s->ps.sps->vshift[1]) * s->frame->linesize[2] + ((x >> s->ps.sps->hshift[1]) << s->ps.sps->pixel_shift[CHANNEL_TYPE_CHROMA])];
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
#if 0 //HAVE_SSE42
    {
        __m128i x0, x1, x2, x3, x4;
        unsigned char temp[4] = {0x00, 0x0F, 0xFF, 0xFF};
        x0 = _mm_loadu_si128((__m128i *) neigh);
        x1 = _mm_loadu_si128((__m128i *) curr);
        x2 = _mm_loadl_epi64((__m128i *) &(neigh->pred_flag));
        x3 = _mm_loadl_epi64((__m128i *) &(curr->pred_flag));

        x4 = _mm_loadl_epi64((__m128i *) temp);
        x0 = _mm_cmpeq_epi64 (x0, x1);
        x2 = _mm_cmpeq_epi8 (x2, x3);

        x2 = _mm_or_si128 (x2, x4);
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
#if 0 //HAVE_SSE42
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
#if 0 //HAVE_SSE42
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
#if 0 //HAVE_SSE42
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
        } else
            return 1;
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
                             RefPicList *neigh_refPicList, RefPicList *refPicList)
{
    if ( !memcmp(curr, neigh, sizeof(MvField) ) && refPicList[0].list[curr->ref_idx[0]] == neigh_refPicList[0].list[neigh->ref_idx[0]] && refPicList[1].list[curr->ref_idx[1]] == neigh_refPicList[1].list[neigh->ref_idx[1]])
        return 0;
    if (curr->pred_flag == PF_BI &&  neigh->pred_flag == PF_BI) {
        // same L0 and L1
        if (refPicList[0].list[curr->ref_idx[0]]        == neigh_refPicList[0].list[neigh->ref_idx[0]] &&
            refPicList[0].list[curr->ref_idx[0]]        == refPicList[1].list[curr->ref_idx[1]]        &&
            neigh_refPicList[0].list[neigh->ref_idx[0]] == neigh_refPicList[1].list[neigh->ref_idx[1]]) {
#if 0 //HAVE_SSE42
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
        } else if (neigh_refPicList[0].list[neigh->ref_idx[0]] == refPicList[0].list[curr->ref_idx[0]] &&
                   neigh_refPicList[1].list[neigh->ref_idx[1]] == refPicList[1].list[curr->ref_idx[1]]) {
#if 0 //HAVE_SSE42
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
        } else if (neigh_refPicList[1].list[neigh->ref_idx[1]] == refPicList[0].list[curr->ref_idx[0]] &&
                   neigh_refPicList[0].list[neigh->ref_idx[0]] == refPicList[1].list[curr->ref_idx[1]]) {
#if 0 //HAVE_SSE42
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
        } else
            return 1;
    } else if ((curr->pred_flag != PF_BI) && (neigh->pred_flag != PF_BI)){ // 1 MV
        Mv A, B;
        int ref_A, ref_B;

        if (curr->pred_flag & 1) {
            A     = curr->mv[0];
            ref_A = refPicList[0].list[curr->ref_idx[0]];
        } else {
            A     = curr->mv[1];
            ref_A = refPicList[1].list[curr->ref_idx[1]];
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
    int log2_min_pu_size = s->ps.sps->log2_min_pu_size;
    int log2_min_tu_size = s->ps.sps->log2_min_tb_size;
    int min_pu_width     = s->ps.sps->min_pu_width;
    int min_tu_width     = s->ps.sps->min_tb_width;
    int is_intra = tab_mvf[(y0 >> log2_min_pu_size) * min_pu_width +
                           (x0 >> log2_min_pu_size)].pred_flag == PF_INTRA;
    int i, j, bs;

    if (y0 > 0 && (y0 & 7) == 0) {
        int bd_ctby = y0 & ((1 << s->ps.sps->log2_ctb_size) - 1);
        int bd_slice = s->sh.slice_loop_filter_across_slices_enabled_flag ||
                       !(lc->boundary_flags & BOUNDARY_UPPER_SLICE);
        int bd_tiles = s->ps.pps->loop_filter_across_tiles_enabled_flag ||
                       !(lc->boundary_flags & BOUNDARY_UPPER_TILE);
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
                    bs = boundary_strength(s, curr, top, top_refPicList, ff_hevc_get_ref_list(s, s->ref, x0, y0));
#endif
                s->horizontal_bs[((x0 + i) + y0 * s->bs_width) >> 2] = bs;
            }
        }
    }

    // bs for vertical TU boundaries
    if (x0 > 0 && (x0 & 7) == 0) {
        int bd_ctbx = x0 & ((1 << s->ps.sps->log2_ctb_size) - 1);
        int bd_slice = s->sh.slice_loop_filter_across_slices_enabled_flag ||
                       !(lc->boundary_flags & BOUNDARY_LEFT_SLICE);
        int bd_tiles = s->ps.pps->loop_filter_across_tiles_enabled_flag ||
                       !(lc->boundary_flags & BOUNDARY_LEFT_TILE);
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
                    bs = boundary_strength(s, curr, left, left_refPicList, ff_hevc_get_ref_list(s, s->ref, x0, y0));
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
                bs = boundary_strength(s, curr, top, refPicList, ff_hevc_get_ref_list(s, s->ref, x0, y0));
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
                bs = boundary_strength(s, curr, left, refPicList, ff_hevc_get_ref_list(s, s->ref, x0, y0));
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
    int log2_min_pu_size    = s->ps.sps->log2_min_pu_size;
    int log2_min_tu_size    = s->ps.sps->log2_min_tb_size;
    int pic_width_in_min_pu = s->ps.sps->width >> log2_min_pu_size;
    int pic_width_in_min_tu = s->ps.sps->width >> log2_min_tu_size;
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
                bs = boundary_strength(s, curr, top, top_refPicList, ff_hevc_get_ref_list(s, s->ref, x0, y0));
#endif
        if ((slice_up_boundary & 1) && (y0 % (1 << s->ps.sps->log2_ctb_size)) == 0)
            bs = 0;
        if (s->sh.disable_deblocking_filter_flag == 1)
            bs = 0;
        s->horizontal_bs[(x0 + y0 * s->bs_width) >> 2] =  bs;
    }
}

void ff_hevc_deblocking_boundary_strengths_v(HEVCContext *s, int x0, int y0, int slice_left_boundary)
{
    MvField *tab_mvf        = s->ref->tab_mvf;
    int log2_min_pu_size    = s->ps.sps->log2_min_pu_size;
    int log2_min_tu_size    = s->ps.sps->log2_min_tb_size;
    int pic_width_in_min_pu = s->ps.sps->width >> log2_min_pu_size;
    int pic_width_in_min_tu = s->ps.sps->width >> log2_min_tu_size;
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
                bs = boundary_strength(s, curr, left, left_refPicList, ff_hevc_get_ref_list(s, s->ref, x0, y0));
#endif
        if ((slice_left_boundary & 1) && (x0 % (1 << s->ps.sps->log2_ctb_size)) == 0)
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
    int x_end = x >= s->ps.sps->width  - ctb_size;
    deblocking_filter_CTB(s, x, y);
    if (s->ps.sps->sao_enabled_flag) {
        int y_end = y >= s->ps.sps->height - ctb_size;
        if (y && x)
            sao_filter_CTB(s, x - ctb_size, y - ctb_size);
        if (x && y_end)
            sao_filter_CTB(s, x - ctb_size, y);
        if (y && x_end) {
            sao_filter_CTB(s, x, y - ctb_size);
            if (s->threads_type & FF_THREAD_FRAME )
                ff_thread_report_progress(&s->ref->tf, y, 0);
        }
        if (x_end && y_end) {
            sao_filter_CTB(s, x , y);
            if (s->threads_type & FF_THREAD_FRAME )
                ff_thread_report_progress(&s->ref->tf, y + ctb_size, 0);
        }
    } else if (s->threads_type & FF_THREAD_FRAME && x_end)
        ff_thread_report_progress(&s->ref->tf, y + ctb_size - 4, 0);
}

void ff_hevc_hls_filters(HEVCContext *s, int x_ctb, int y_ctb, int ctb_size)
{
    int x_end = x_ctb >= s->ps.sps->width  - ctb_size;
    int y_end = y_ctb >= s->ps.sps->height - ctb_size;
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
    unsigned int x_end = x_ctb >= s->ps.sps->width  - ctb_size;
    unsigned int y_end = y_ctb >= s->ps.sps->height - ctb_size;
    int ctb_addr_rs = (x_ctb >> s->ps.sps->log2_ctb_size) + (y_ctb >> s->ps.sps->log2_ctb_size) * s->ps.sps->ctb_width;

    if (y_ctb && x_ctb && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1 - s->ps.sps->ctb_width]))
        ff_hevc_hls_filter_slice(s, x_ctb - ctb_size, y_ctb - ctb_size, ctb_size);

    if (y_ctb && x_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->ps.sps->ctb_width]))
        ff_hevc_hls_filter_slice(s, x_ctb, y_ctb - ctb_size, ctb_size);


    if (x_ctb && y_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1]))
        ff_hevc_hls_filter_slice(s, x_ctb - ctb_size, y_ctb, ctb_size);

    if(!x_end)
        x_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + 1]);
    if(!y_end)
        y_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + s->ps.sps->ctb_width]);

    if(x_slice_end && y_ctb && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->ps.sps->ctb_width]))
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
    int ctb_addr_rs = (x >> s->ps.sps->log2_ctb_size) + (y >> s->ps.sps->log2_ctb_size) * s->ps.sps->ctb_width;

    if (s->ps.sps->sao_enabled) {
        int x_end = x >= s->ps.sps->width  - ctb_size;
        int y_end = y >= s->ps.sps->height - ctb_size;
        if (y && x && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1 - s->ps.sps->ctb_width]))
            sao_filter_CTB(s, x - ctb_size, y - ctb_size);
        if (x && y_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - 1]))
            sao_filter_CTB(s, x - ctb_size, y);
        if (y && x_end && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->ps.sps->ctb_width])) {
            sao_filter_CTB(s, x, y - ctb_size);
            if (s->threads_type & FF_THREAD_FRAME ) {
                int i;
                s->decoded_rows[(y - ctb_size)>>s->ps.sps->log2_ctb_size] = 1;
                for(i = 0; i < s->ps.sps->ctb_height && s->decoded_rows[i]; i++);
                ff_thread_report_progress(&s->ref->tf, i*ctb_size, 0);
            }
        }

        if(!x_end)
            x_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + 1]);
        if(!y_end)
            y_slice_end = (s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs + s->ps.sps->ctb_width]);

        if(x_slice_end && y && (s->tab_slice_address[ctb_addr_rs] == s->tab_slice_address[ctb_addr_rs - s->ps.sps->ctb_width])) {
            sao_filter_CTB(s, x , y - ctb_size);
            if (s->threads_type & FF_THREAD_FRAME ) {
                int i; 
                s->decoded_rows[(y - ctb_size)>>s->ps.sps->log2_ctb_size] = 1;
                for(i = 0; i < s->ps.sps->ctb_height && s->decoded_rows[i]; i++);
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
        if (y && x >= s->ps.sps->width - ctb_size)
            if (s->threads_type & FF_THREAD_FRAME ) {
                int i;
                s->decoded_rows[y>>s->ps.sps->log2_ctb_size] = 1;
                for(i = 0; i < s->ps.sps->ctb_height && s->decoded_rows[i]; i++);
                    ff_thread_report_progress(&s->ref->tf, i*ctb_size, 0);
            }
    }
}
#endif
#if !ACTIVE_PU_UPSAMPLING
static void copy_block(HEVCContext *s, uint8_t *src, uint8_t * dst, ptrdiff_t bl_stride, ptrdiff_t el_stride, int ePbH, int ePbW, enum ChannelType channel ) {
    int i;
#if 1
    for (i = 0; i < ePbH ; i++) {
        memcpy(dst, src, ePbW * sizeof(uint8_t));
        src += bl_stride;
        dst += el_stride;
    }
#else
    int j/*, refLayerId*/ = 0;
    int shift = s->up_filter_inf.shift[channel];
    for (i = 0; i < ePbH ; i++) {
        for (j = 0; j < ePbW ; j++)
            dst[j] = src[j]<<shift;
        src += bl_stride;
        dst += el_stride;
    }
#endif
}
#endif
//static void copy_block_16(HEVCContext *s, uint16_t *src, uint16_t * dst, ptrdiff_t bl_stride, ptrdiff_t el_stride, int ePbH, int ePbW, enum ChannelType channel ) {
//    int i;
//#if 0
//    for (i = 0; i < ePbH ; i++) {
//        memcpy(dst, src, ePbW * sizeof(pixel));
//        src += bl_stride;
//        dst += el_stride;
//    }
//#else
//    int j/*, refLayerId*/ = 0;
//    int shift = s->up_filter_inf.shift[channel];
//    for (i = 0; i < ePbH ; i++) {
//        for (j = 0; j < ePbW ; j++){
//            dst[j] = src[j]<<shift;
//        }
//        src += bl_stride;
//        dst += el_stride;
//    }
//#endif
//}
#if !ACTIVE_PU_UPSAMPLING
static void colorMapping(HEVCContext *s, uint8_t *src_y, uint8_t *src_u, uint8_t *src_v, int src_stride, int src_stridec,int x0, int y0, int x0_cr, int y0_cr, int bl_width, int bl_height, int el_width, int el_height) {
    int i, j, k;
    int ctb_size  = ((( 1<<s->ps.sps->log2_ctb_size) * s->up_filter_inf.scaleXCr - s->up_filter_inf.addXLum) >> 12)  >> 4;
    uint8_t srcYaver, tmpU, tmpV;
    uint16_t val[6], val_dst[6], val_prev[2];
    pixel *src_U_prev, *src_V_prev, *src_U_next, *src_V_next;
    SCuboid rCuboid;
    const HEVCPPS *pps   = s->ps.pps;
    uint16_t iMaxValY = (1<<pps->pc3DAsymLUT.cm_output_luma_bit_depth)  -1;
    uint16_t iMaxValC = (1<<pps->pc3DAsymLUT.cm_output_chroma_bit_depth)-1;

    int left_edge    =  x0 ? 1:0;
    int up_edge      =  y0 ? 1:0;
    int right_edge   =  x0 + ctb_size < bl_width  ? 1:0;
    int bottom_edge  =  y0 + ctb_size < bl_height ? 1:0;

    uint16_t * dst_y = s->HEVClc->color_mapping_cgs_y;
    uint16_t * dst_u = s->HEVClc->color_mapping_cgs_u;
    uint16_t * dst_v = s->HEVClc->color_mapping_cgs_v;

    uint8_t *temp_u = src_u + src_stridec * (y0_cr );
    uint8_t *temp_v = src_v + src_stridec * (y0_cr );

    int size = bl_width>>1;
    if( !right_edge ){ // add padding from right
      for(i = 0 ; i < bottom_edge + up_edge + (ctb_size>>1); i++) {
        temp_u[size]     = temp_u[size-1];
        temp_v[size]     = temp_v[size-1];
        temp_u          += src_stridec;
        temp_v          += src_stridec;
      }
    }

    if( !bottom_edge ) { // add padding from bottom
      size = src_stridec * (bl_height>>1) + x0_cr - left_edge;
      for(j = 0 ; j < left_edge + right_edge + (ctb_size>>1); j++) {
        src_u[size+j] = src_u[size+j-src_stridec];
        src_v[size+j] = src_v[size+j-src_stridec];
      }
    }

    if( !right_edge && !bottom_edge){ // add padding from bottom right
    	size = src_stridec * (bl_height>>1) + x0_cr + right_edge + (ctb_size>>1);
    	src_u[size] = src_u[size-1];
		src_v[size] = src_v[size-1];
    }

    src_y      = src_y + src_stride  * (y0)    + x0   ;
    src_u      = src_u + src_stridec * (y0_cr) + x0_cr;
    src_v      = src_v + src_stridec * (y0_cr) + x0_cr;
    src_U_prev = src_u - src_stridec * up_edge;
    src_V_prev = src_v - src_stridec * up_edge;
    src_U_next = src_u + src_stridec;
    src_V_next = src_v + src_stridec;

    for(i = 0 ; i < ctb_size && i+y0 < el_height; i += 2 ) {
      for(j = 0 , k = 0 ; j < ctb_size && j+x0 < el_width; j += 2 , k++ ) {
        short a ,b;
        SYUVP dstUV;
        val[0] = src_y[j];
        val[1] = src_y[j+1];
        val[2] = src_y[j+src_stride];
        val[3] = src_y[j+src_stride+1];
        val[4] = src_u[k];
        val[5] = src_v[k];
        srcYaver = (val[0] + val[2] + 1 ) >> 1;;
        val_prev[0]  = src_U_prev[k]; //srcUP0
        val_prev[1]  = src_V_prev[k]; //srcVP0

        tmpU =  (val_prev[0] + val[4] + (val[4]<<1) + 2 ) >> 2;
        tmpV =  (val_prev[1] + val[5] + (val[5]<<1) + 2 ) >> 2;

        rCuboid = pps->pc3DAsymLUT.S_Cuboid[val[0] >> pps->pc3DAsymLUT.YShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpU>=pps->pc3DAsymLUT.nAdaptCThresholdU : tmpU>> pps->pc3DAsymLUT.UShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpV>=pps->pc3DAsymLUT.nAdaptCThresholdV : tmpV>> pps->pc3DAsymLUT.VShift2Idx];
        val_dst[0] = ( ( rCuboid.P[0].Y * val[0] + rCuboid.P[1].Y * tmpU + rCuboid.P[2].Y * tmpV + pps->pc3DAsymLUT.nMappingOffset ) >> pps->pc3DAsymLUT.nMappingShift ) + rCuboid.P[3].Y;

        a = src_u[k+1] + val[4];
        tmpU =  ((a<<1) + a + val_prev[0] + src_U_prev[k+1] + 4 ) >> 3;
        b = src_v[k+1] + val[5];
        tmpV =  ((b<<1) + b + val_prev[1] + src_V_prev[k+1] + 4 ) >> 3;

        rCuboid = pps->pc3DAsymLUT.S_Cuboid[val[1] >> pps->pc3DAsymLUT.YShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpU>=pps->pc3DAsymLUT.nAdaptCThresholdU : tmpU>> pps->pc3DAsymLUT.UShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpV>=pps->pc3DAsymLUT.nAdaptCThresholdV : tmpV>> pps->pc3DAsymLUT.VShift2Idx];
        val_dst[1] = ( ( rCuboid.P[0].Y * val[1] + rCuboid.P[1].Y * tmpU + rCuboid.P[2].Y * tmpV + pps->pc3DAsymLUT.nMappingOffset ) >> pps->pc3DAsymLUT.nMappingShift ) + rCuboid.P[3].Y;

        tmpU =  (src_U_next[k] + val[4] + (val[4]<<1) + 2 ) >> 2;
        tmpV =  (src_V_next[k] + val[5] + (val[5]<<1) + 2 ) >> 2;
        rCuboid = pps->pc3DAsymLUT.S_Cuboid[val[2] >> pps->pc3DAsymLUT.YShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpU>=pps->pc3DAsymLUT.nAdaptCThresholdU : tmpU>> pps->pc3DAsymLUT.UShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpV>=pps->pc3DAsymLUT.nAdaptCThresholdV : tmpV>> pps->pc3DAsymLUT.VShift2Idx];
        val_dst[2] = ( ( rCuboid.P[0].Y * val[2] + rCuboid.P[1].Y * tmpU + rCuboid.P[2].Y * tmpV + pps->pc3DAsymLUT.nMappingOffset ) >> pps->pc3DAsymLUT.nMappingShift ) + rCuboid.P[3].Y;

        tmpU =  ((a<<1) + a + src_U_next[k] + src_U_next[k+1] + 4 ) >> 3;
        tmpV =  ((b<<1) + b + src_V_next[k] + src_V_next[k+1] + 4 ) >> 3;
        rCuboid = pps->pc3DAsymLUT.S_Cuboid[val[3] >> pps->pc3DAsymLUT.YShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpU>=pps->pc3DAsymLUT.nAdaptCThresholdU : tmpU>> pps->pc3DAsymLUT.UShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? tmpV>=pps->pc3DAsymLUT.nAdaptCThresholdV : tmpV>> pps->pc3DAsymLUT.VShift2Idx];
        val_dst[3] = ( ( rCuboid.P[0].Y * val[3] + rCuboid.P[1].Y * tmpU + rCuboid.P[2].Y * tmpV + pps->pc3DAsymLUT.nMappingOffset ) >> pps->pc3DAsymLUT.nMappingShift ) + rCuboid.P[3].Y;

        rCuboid = pps->pc3DAsymLUT.S_Cuboid[srcYaver >> pps->pc3DAsymLUT.YShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? val[4]>=pps->pc3DAsymLUT.nAdaptCThresholdU : val[4]>> pps->pc3DAsymLUT.UShift2Idx][pps->pc3DAsymLUT.cm_octant_depth==1? val[5]>=pps->pc3DAsymLUT.nAdaptCThresholdV : val[5]>> pps->pc3DAsymLUT.VShift2Idx];
        dstUV.Y = 0;
        dstUV.U = ( ( rCuboid.P[0].U * srcYaver + rCuboid.P[1].U * val[4] + rCuboid.P[2].U * val[5] + pps->pc3DAsymLUT.nMappingOffset ) >> pps->pc3DAsymLUT.nMappingShift ) + rCuboid.P[3].U;
        dstUV.V = ( ( rCuboid.P[0].V * srcYaver + rCuboid.P[1].V * val[4] + rCuboid.P[2].V * val[5] + pps->pc3DAsymLUT.nMappingOffset ) >> pps->pc3DAsymLUT.nMappingShift ) + rCuboid.P[3].V;

        dst_y[j]   = av_clip(val_dst[0],0, iMaxValY);
        dst_y[j+1] = av_clip(val_dst[1], 0, iMaxValY );
        dst_y[j+MAX_EDGE_BUFFER_STRIDE] = av_clip(val_dst[2] , 0, iMaxValY);
        dst_y[j+MAX_EDGE_BUFFER_STRIDE+1] = av_clip(val_dst[3] , 0, iMaxValY);
        dst_u[k]   = av_clip(dstUV.U, 0, iMaxValC );
        dst_v[k]   = av_clip(dstUV.V, 0, iMaxValC );
      }
      src_y += src_stride + src_stride;

      src_U_prev = src_u;
      src_V_prev = src_v;

      src_u = src_U_next;
      src_v = src_V_next;
      src_U_next += src_stridec;
      src_V_next += src_stridec;

      dst_y += (MAX_EDGE_BUFFER_STRIDE + MAX_EDGE_BUFFER_STRIDE);
      dst_u += (MAX_EDGE_BUFFER_STRIDE);
      dst_v += (MAX_EDGE_BUFFER_STRIDE);
    }
}
#endif

static void upsample_block_luma_cgs(HEVCContext *s, HEVCFrame *ref0, int x0, int y0) {

    uint16_t *dst = (uint16_t *)ref0->frame->data[0];

    int ctb_size  = 1 << s->ps.sps->log2_ctb_size;
    int sample_size = s->ps.sps->bit_depth[0] > 8 ? 2 : 1;

    int el_width  = s->ps.sps->width;
    int el_height = s->ps.sps->height;

    //fixme: AVC BL
    HEVCFrame *bl_frame = s->BL_frame;
    int ref_layer_id = s->ps.vps->vps_ext.ref_layer_id[s->nuh_layer_id][0];
    int bl_sample_size = s->sh.Bit_Depth[ref_layer_id][1] > 8 ? 2 : 1;
    //int bl_sample_size = sample_size;//((HEVCContext*)s->avctx->BL_avcontext)->ps.sps->bit_depth[0];
    //H264Picture *bl_frame = s->BL_frame;
    //int bl_width  =  bl_frame->f->width; //fixme: width or width or blframe->width
    //int bl_height =  bl_frame->f->height;
    //int bl_stride =  bl_frame->f->linesize[0];

    //TODO: compute strides given the bitdepth
    int bl_width  = bl_frame->frame->width;
    int bl_height = bl_frame->frame->height;

    int bl_stride   = bl_frame->frame->linesize[0] / bl_sample_size;
    int bl_stride_c = bl_frame->frame->linesize[1] / bl_sample_size;

    int el_stride =  ref0->frame->linesize[0] / sample_size;


    int ePbW = x0 + ctb_size > el_width  ? el_width  - x0 : ctb_size;
    int ePbH = y0 + ctb_size > el_height ? el_height - y0 : ctb_size;

    int padd_bottom = y0 + ctb_size >= el_height ? 1 : 0;
    int padd_right  = x0 + ctb_size >= el_width  ? 1 : 0;

    int padd_top    = !y0 ? 1 : 0;
    int padd_left   = !x0 ? 1 : 0;

    if (s->up_filter_inf.idx == SNR) { /* x1 quality (SNR) scalability */

      uint8_t *src_y = bl_frame->frame->data[0];
      uint8_t *src_u = bl_frame->frame->data[1];
      uint8_t *src_v = bl_frame->frame->data[2];

      uint8_t *dst_y = ref0->frame->data[0];
      uint8_t *dst_u = ref0->frame->data[1];
      uint8_t *dst_v = ref0->frame->data[2];

      //FIXME: non 420 do not require shifting x0 and y0
      src_y += (x0 + y0 * bl_stride) * bl_sample_size;
      src_u += ((x0>>1) + (y0>>1) * bl_stride_c) * bl_sample_size;
      src_v += ((x0>>1) + (y0>>1) * bl_stride_c) * bl_sample_size;

      dst_y += (x0 + y0 * el_stride) * sample_size;
      dst_u += ((x0>>1) + (y0>>1) * (el_stride>>1)) * sample_size;
      dst_v += ((x0>>1) + (y0>>1) * (el_stride>>1)) * sample_size;

      s->hevcdsp.map_color_block(&s->ps.pps->pc3DAsymLUT, src_y, src_u, src_v,
                                 dst_y, dst_u, dst_v,
                                 bl_stride, bl_stride_c,
                                 el_stride, el_stride>>1,
                                 ePbW, ePbH,
                                 padd_right, padd_bottom,
                                 padd_top, padd_left);
    } else { /* spatial scalability */

      int bl_x0  = (( (x0  - s->ps.sps->conf_win.left_offset) * s->up_filter_inf.scaleXLum - s->up_filter_inf.addXLum) >> 12) >> 4;
      int bl_y0 =  (( (y0  - s->ps.sps->conf_win.top_offset ) * s->up_filter_inf.scaleYLum - s->up_filter_inf.addYLum) >> 12) >> 4;


      int bl_y = av_clip_c(bl_y0 - MAX_EDGE, 0, bl_height);
      int bl_x = av_clip_c(bl_x0 - MAX_EDGE, 0, bl_width );

      //FIXME non 420
      int bl_x_cr0 = ((((x0>>1) - (s->ps.sps->conf_win.left_offset>>1)) * s->up_filter_inf.scaleXCr - s->up_filter_inf.addXLum) >> 12) >> 4;
      int bl_y_cr0 = ((((y0>>1) - (s->ps.sps->conf_win.top_offset >>1)) * s->up_filter_inf.scaleYCr - s->up_filter_inf.addYLum) >> 12) >> 4;

      int bPbW = ((( (x0 + ctb_size - s->ps.sps->conf_win.left_offset) * s->up_filter_inf.scaleXLum - s->up_filter_inf.addXLum) >> 12) >> 4) - bl_x;
      int bPbH = ((( (y0 + ctb_size - s->ps.sps->conf_win.top_offset ) * s->up_filter_inf.scaleYLum - s->up_filter_inf.addYLum) >> 12) >> 4) - bl_y;

      int bl_y_cr = av_clip_c(bl_y_cr0 - MAX_EDGE_CR, 0, bl_height >> 1);
      int bl_x_cr = av_clip_c(bl_x_cr0 - MAX_EDGE_CR, 0, bl_width  >> 1);

      int bl_y2 = av_clip_c(bl_y + bPbH + 2 * MAX_EDGE , 0 , bl_height);
      int bl_x2 = av_clip_c(bl_x + bPbW + 2 * MAX_EDGE , 0 , bl_width );

      int bl_edge_top  = !bl_y ? MAX_EDGE : 0;
      int bl_edge_left = !bl_x ? MAX_EDGE : 0;

      int bl_edge_bottom = bl_y2 == bl_height ? MAX_EDGE : 0;
      int bl_edge_right  = bl_x2 == bl_width  ? MAX_EDGE : 0;

      uint16_t *tmp0;



      //int ref_layer_id = s->ps.vps->Hevc_VPS_Ext.ref_layer_id[s->nuh_layer_id][0];

      uint8_t *src_y = (uint8_t *)bl_frame->frame->data[0];
      uint8_t *src_u = (uint8_t *)bl_frame->frame->data[1];
      uint8_t *src_v = (uint8_t *)bl_frame->frame->data[2];

      //NOTE we padd those buffers here so we can emulate edge into it if needed
      uint8_t *dst_y = (uint8_t *)s->HEVClc->color_mapping_cgs_y + (MAX_EDGE * MAX_EDGE_BUFFER_STRIDE + MAX_EDGE) * 2 * sample_size;
      uint8_t *dst_u = (uint8_t *)s->HEVClc->color_mapping_cgs_u + (MAX_EDGE * MAX_EDGE_BUFFER_STRIDE + MAX_EDGE) * 2 * sample_size;
      uint8_t *dst_v = (uint8_t *)s->HEVClc->color_mapping_cgs_v + (MAX_EDGE * MAX_EDGE_BUFFER_STRIDE + MAX_EDGE) * 2 * sample_size;

      int cm_width;
      int cm_height;

      int bl_x_cm = bl_x;
      int bl_y_cm = bl_y;

      int bl_x_cr_cm = bl_x_cr;
      int bl_y_cr_cm = bl_y_cr;

      bPbH = bl_y2 - bl_y;
      bPbW = bl_x2 - bl_x;

      cm_width  = bPbW;
      cm_height = bPbH;
      //FIXME not sure about this (color mapping is based on pair bl_x and bl_y coordinates)
      if(bl_x%2 ){
          bl_x_cm -= 1;
          cm_width += 1;
          bl_x_cr_cm = bl_x_cm >> 1;
      }

      if(bl_y%2){
          bl_y_cm   -= 1;
          cm_height += 1;
          bl_y_cr_cm = bl_y_cm >> 1;
      }

      src_y += (bl_x_cm + bl_y_cm * bl_stride) * bl_sample_size;
      src_u += ((bl_x_cr_cm) + (bl_y_cr_cm) * bl_stride_c) * bl_sample_size;
      src_v += ((bl_x_cr_cm) + (bl_y_cr_cm) * bl_stride_c) * bl_sample_size;

      s->hevcdsp.map_color_block(&s->ps.pps->pc3DAsymLUT, src_y, src_u, src_v,
                                 dst_y, dst_u, dst_v,
                                 bl_stride, bl_stride_c,
                                 MAX_EDGE_BUFFER_STRIDE, MAX_EDGE_BUFFER_STRIDE,
                                 cm_width, cm_height,
                                 padd_right,padd_bottom,padd_top,padd_left);
      if(bl_x%2 ){
          dst_y += sample_size;
      }

      if(bl_y%2){
         dst_y += MAX_EDGE_BUFFER_STRIDE * sample_size;
      }

      if(bl_edge_left || bl_edge_right)
          s->vdsp.emulated_edge_up_h((uint8_t *)dst_y,(uint8_t *)dst_y, MAX_EDGE_BUFFER_STRIDE,
                                            bPbW, bPbH,
                                            padd_left, padd_right);

      tmp0 = (uint16_t *)s->HEVClc->edge_emu_buffer_up_v + ((MAX_EDGE) * MAX_EDGE_BUFFER_STRIDE);

      s->hevcdsp.upsample_filter_block_luma_h[s->up_filter_inf.idx]((int16_t *)tmp0, MAX_EDGE_BUFFER_STRIDE, (uint8_t *)dst_y, MAX_EDGE_BUFFER_STRIDE, x0, bl_x,
                                                                    ePbW, bPbH, el_width,
                                                                    &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);
      if(bl_edge_top || bl_edge_bottom)
          s->vdsp.emulated_edge_up_v((uint8_t *)tmp0, MAX_EDGE_BUFFER_STRIDE , bPbH, padd_top, padd_bottom);

      s->hevcdsp.upsample_filter_block_luma_v[s->up_filter_inf.idx]((uint8_t *)dst , ref0->frame->linesize[0], tmp0 , MAX_EDGE_BUFFER_STRIDE,
                                                                    bl_y , x0, y0, ePbW, ePbH, el_width, el_height,
                                                                    &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);
    }
}

static void upsample_block_mc_cgs(HEVCContext *s, HEVCFrame *ref0, int x0, int y0) {
    uint8_t   *src;
    uint16_t  *tmp0;

    //FIXME non 420
    int el_width  =  s->ps.sps->width  >> 1;
    int el_height =  s->ps.sps->height >> 1;

    //FIXME: Non HEVC BL
    //H264Picture *bl_frame = s->BL_frame;
    HEVCFrame *bl_frame = s->BL_frame;
    int bl_width  =  bl_frame->frame->width  >> 1;
    int bl_height  = bl_frame->frame->height >> 1 > el_height>>1 ? bl_frame->frame->height>>1:el_height>>1;

    int ref_layer_id = s->ps.vps->vps_ext.ref_layer_id[s->nuh_layer_id][0];
    int sample_size = s->ps.sps->bit_depth[1] > 8 ? 2 : 1;
    //int bl_sample_size = s->sh.Bit_Depth[ref_layer_id][1] > 8 ? 2 : 1;

    int cr;
    int ctb_size = 1 << (s->ps.sps->log2_ctb_size-1);

    int ePbW = x0 + ctb_size > el_width  ? el_width  - x0 : ctb_size;
    int ePbH = y0 + ctb_size > el_height ? el_height - y0 : ctb_size;

    int el_stride = ref0->frame->linesize[1]/*/sizeof(uint16_t)*/;

    if (s->up_filter_inf.idx == SNR) {
        //nothing to do here (already done in luma)
    } else {
        //FIXME non 420
        int bl_x0 = ((( x0 - (s->ps.sps->conf_win.left_offset>>1)) * s->up_filter_inf.scaleXCr - s->up_filter_inf.addXCr) >> 12)  >> 4;
        int bl_y0 = ((( y0 - (s->ps.sps->conf_win.top_offset >>1)) * s->up_filter_inf.scaleYCr - s->up_filter_inf.addYCr) >> 12)  >> 4;

        int bl_y = av_clip_c(bl_y0 - MAX_EDGE_CR, 0, bl_height);
        int bl_x = av_clip_c(bl_x0 - MAX_EDGE_CR, 0, bl_width );

        int bPbW = ((( (x0 + (ctb_size ) - s->ps.sps->conf_win.left_offset) * s->up_filter_inf.scaleXCr - s->up_filter_inf.addXCr) >> 12) >> 4) - bl_x;
        int bPbH = ((( (y0 + (ctb_size ) - s->ps.sps->conf_win.top_offset ) * s->up_filter_inf.scaleYCr - s->up_filter_inf.addYCr) >> 12) >> 4) - bl_y;

        int bl_y2 = av_clip_c(bl_y + bPbH + 2 * MAX_EDGE_CR , 0 , bl_height);
        int bl_x2 = av_clip_c(bl_x + bPbW + 2 * MAX_EDGE_CR , 0 , bl_width );

        int bl_edge_top  = !bl_y ? MAX_EDGE : 0;
        int bl_edge_left = !bl_x ? MAX_EDGE : 0;

        int bl_edge_bottom = bl_y2 == bl_height ? MAX_EDGE : 0;
        int bl_edge_right  = bl_x2 == bl_width  ? MAX_EDGE : 0;

        bPbH = bl_y2 - bl_y;
        bPbW = bl_x2 - bl_x;

        for (cr = 1; cr <= 2; cr++) {

          if(cr == 1)
            src = (uint8_t*)s->HEVClc->color_mapping_cgs_u + (MAX_EDGE * MAX_EDGE_BUFFER_STRIDE + MAX_EDGE) * 2 * sample_size ;
          else
            src = (uint8_t*)s->HEVClc->color_mapping_cgs_v + (MAX_EDGE * MAX_EDGE_BUFFER_STRIDE + MAX_EDGE) * 2 * sample_size ;

          //FIXME not sure about this (color mapping is based on pair bl_x and bl_y coordinates)
          if((bl_x>>1)%2 && bl_x%2){
//              src += sample_size;
          }

          if(((bl_y>>1)%2 && bl_y%2)|| (!(bl_y%2) && !bl_edge_top)){
              src += MAX_EDGE_BUFFER_STRIDE * sample_size;
          }

          if(bl_edge_left || bl_edge_right)
              s->vdsp.emulated_edge_up_cr_h((uint8_t *)src,(uint8_t *)src,
                                                  MAX_EDGE_BUFFER_STRIDE,
                                                  bPbW, bPbH,
                                                  bl_edge_left, bl_edge_right);

          tmp0 = (uint16_t*)s->HEVClc->edge_emu_buffer_up_v + (MAX_EDGE_CR * MAX_EDGE_BUFFER_STRIDE);

          s->hevcdsp.upsample_filter_block_cr_h[s->up_filter_inf.idx](  (int16_t *)tmp0, MAX_EDGE_BUFFER_STRIDE, (uint8_t *)src, MAX_EDGE_BUFFER_STRIDE,
                                                                        x0, bl_x, ePbW, bPbH,el_width,
                                                                        &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);

          if (!bl_edge_top)
              tmp0 -= (1 * MAX_EDGE_BUFFER_STRIDE);

          if(bl_edge_top || bl_edge_bottom)
              s->vdsp.emulated_edge_up_cr_v((uint8_t *)tmp0, ePbW, bPbH,
                                                  bl_edge_top, bl_edge_bottom);



          s->hevcdsp.upsample_filter_block_cr_v[s->up_filter_inf.idx](  (uint8_t *)ref0->frame->data[cr] , el_stride, (uint16_t*)tmp0 , MAX_EDGE_BUFFER_STRIDE,
                                                                        bl_y, x0, y0, ePbW, ePbH, el_width, el_height,
                                                                        &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);
        }
    }
    s->is_upsampled[((y0) / ctb_size * s->ps.sps->ctb_width) + ((x0) / ctb_size)] = 1;
}

static void upsample_block_luma(HEVCContext *s, HEVCFrame *ref0, int x0, int y0) {
    AVFrame *bl_frame;

    uint8_t *src;
    uint8_t *dst = (uint8_t*)ref0->frame->data[0];
    uint8_t *tmp1;

    int ref_layer_id = s->ps.vps->vps_ext.ref_layer_id[s->nuh_layer_id][0];

    int sample_size = s->ps.sps->bit_depth[0] > 8 ? 2 : 1;
    int bl_sample_size = s->sh.Bit_Depth[ref_layer_id][1] > 8 ? 2 : 1;

    int ctb_size  = 1 << s->ps.sps->log2_ctb_size;

    int el_width  = s->ps.sps->width;
    int el_height = s->ps.sps->height;

    int bl_height = s->BL_height;
    int bl_width  = s->BL_width;

    int ePbW = av_clip_c(el_width  - x0, 0, ctb_size);
    int ePbH = av_clip_c(el_height - y0, 0, ctb_size);

    int bl_stride;
    int el_stride;

    if(s->ps.vps->vps_nonHEVCBaseLayerFlag){ // fixme: Not very efficient the cast could be done before instead of doing it for each block
        bl_frame = ((H264Picture *)s->BL_frame)->f;
    } else {
        bl_frame = ((HEVCFrame *)s->BL_frame)->frame;
    }

    bl_stride = bl_frame->linesize[0]    / bl_sample_size;
    el_stride = ref0->frame->linesize[0] / sample_size;

    if (s->up_filter_inf.idx == SNR) { /* x1 quality (SNR) scalability */
      s->vdsp.copy_block (((uint8_t *)bl_frame->data[0]  + (y0 * bl_stride + x0) * bl_sample_size),
                    ((uint8_t *)ref0->frame->data[0] + (y0 * el_stride + x0) * sample_size),
                    bl_stride, el_stride, ePbH, ePbW);
    } else { /* spatial scalability */
        uint16_t *tmp0;

        int bl_x0  = (( (x0  - s->ps.sps->conf_win.left_offset) * s->up_filter_inf.scaleXLum - s->up_filter_inf.addXLum) >> 12) >> 4;
        int bl_y0 =  (( (y0  - s->ps.sps->conf_win.top_offset ) * s->up_filter_inf.scaleYLum - s->up_filter_inf.addYLum) >> 12) >> 4;

        int bl_y = av_clip_c(bl_y0 - MAX_EDGE, 0, bl_height);
        int bl_x = av_clip_c(bl_x0 - MAX_EDGE, 0, bl_width );

        int bPbW = ((( (x0 + ctb_size - s->ps.sps->conf_win.left_offset) * s->up_filter_inf.scaleXLum - s->up_filter_inf.addXLum) >> 12) >> 4) - bl_x;
        int bPbH = ((( (y0 + ctb_size - s->ps.sps->conf_win.top_offset ) * s->up_filter_inf.scaleYLum - s->up_filter_inf.addYLum) >> 12) >> 4) - bl_y;

        int bl_y2 = av_clip_c(bl_y + bPbH + 2 * MAX_EDGE , 0 , bl_height);
        int bl_x2 = av_clip_c(bl_x + bPbW + 2 * MAX_EDGE , 0 , bl_width );

        int bl_edge_top  = !bl_y ? MAX_EDGE : 0;
        int bl_edge_left = !bl_x ? MAX_EDGE : 0;

        int bl_edge_bottom = bl_y2 == bl_height ? MAX_EDGE : 0;
        int bl_edge_right  = bl_x2 == bl_width  ? MAX_EDGE : 0;

        bPbH = bl_y2 - bl_y;
        bPbW = bl_x2 - bl_x;

        src =  (uint8_t *)bl_frame->data[0] + (bl_y * bl_stride + bl_x) * bl_sample_size;

        if(bl_edge_left || bl_edge_right){

            tmp1 = (uint8_t *)s->HEVClc->edge_emu_buffer_up_h;

            if(bl_edge_left)
                tmp1 += MAX_EDGE * bl_sample_size;

            s->vdsp.emulated_edge_up_h((uint8_t *)tmp1, (uint8_t *)src ,
                                         bl_stride,
                                         bPbW, bPbH,
                                         bl_edge_left , bl_edge_right);

            src = tmp1;
            bl_stride = MAX_EDGE_BUFFER_STRIDE;
        }

        tmp0 = ((uint16_t *)s->HEVClc->edge_emu_buffer_up_v) + (MAX_EDGE * MAX_EDGE_BUFFER_STRIDE);

        s->hevcdsp.upsample_filter_block_luma_h[s->up_filter_inf.idx]((uint16_t *)tmp0, MAX_EDGE_BUFFER_STRIDE, (uint8_t *)src, bl_stride, x0, bl_x,
                                                                      ePbW, bPbH, el_width,
                                                                      &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);

        if(bl_edge_top || bl_edge_bottom)
            s->vdsp.emulated_edge_up_v((uint8_t *)tmp0,
                                         ePbW, bPbH,
                                         bl_edge_top, bl_edge_bottom);


        s->hevcdsp.upsample_filter_block_luma_v[s->up_filter_inf.idx]((uint8_t *)dst , ref0->frame->linesize[0], (uint16_t *)tmp0 , MAX_EDGE_BUFFER_STRIDE,
                                                                      bl_y , x0, y0, ePbW, ePbH, el_width, el_height,
                                                                      &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);
    }
}
static void upsample_block_mc(HEVCContext *s, HEVCFrame *ref0, int x0, int y0) {

    uint8_t   *src;
    uint16_t   *tmp0;
    uint8_t   *tmp1;

    AVFrame *bl_frame;

    int ref_layer_id = s->ps.vps->vps_ext.ref_layer_id[s->nuh_layer_id][0];
    int sample_size = s->ps.sps->bit_depth[0] > 8 ? 2 : 1;
    int bl_sample_size = s->sh.Bit_Depth[ref_layer_id][1] > 8 ? 2 : 1;
    int el_width  =  s->ps.sps->width  >> 1;
    int el_height =  s->ps.sps->height >> 1;

    //Fixme: non 420
    int bl_height = s->BL_height >> 1;
    int bl_width  = s->BL_width  >> 1;

    int bl_stride;

    int cr;
    int ctb_size = 1 << (s->ps.sps->log2_ctb_size - 1);

    int ePbW = av_clip_c(el_width  - x0, 0 , ctb_size);//x0 + ctb_size > el_width  ? el_width  - x0 : ctb_size;
    int ePbH = av_clip_c(el_height - y0, 0 , ctb_size);//y0 + ctb_size > el_height ? el_height - y0 : ctb_size;

    int el_stride = ref0->frame->linesize[1];

    if(s->ps.vps->vps_nonHEVCBaseLayerFlag){// fixme: Not very efficient the cast could be done before instead of doing it for each block
        bl_frame = ((H264Picture *)s->BL_frame)->f;
    }else {
        bl_frame = ((HEVCFrame *)s->BL_frame)->frame;
    }

    bl_stride =  bl_frame->linesize[1] / bl_sample_size;

    if (s->up_filter_inf.idx == SNR) {
        el_stride/=sample_size;
        for (cr = 1; cr <= 2; cr++) {
            s->vdsp.copy_block((uint8_t *)bl_frame->data[cr] + (y0 * bl_stride + x0) * sample_size ,
                      (uint8_t *)ref0->frame->data[cr] + (y0 * el_stride + x0) * sample_size ,
                      bl_stride, el_stride, ePbH, ePbW);
        }
    } else {

        int bl_x0 = (( (x0  - s->ps.sps->conf_win.left_offset) * s->up_filter_inf.scaleXCr - s->up_filter_inf.addXLum) >> 12) >> 4;
        int bl_y0 = (( (y0  - s->ps.sps->conf_win.top_offset ) * s->up_filter_inf.scaleYCr - s->up_filter_inf.addYLum) >> 12) >> 4;

        int bl_y = av_clip_c(bl_y0 - MAX_EDGE_CR, 0, bl_height);
        int bl_x = av_clip_c(bl_x0 - MAX_EDGE_CR, 0, bl_width );

        int bPbW = ((( (x0 + ctb_size - s->ps.sps->conf_win.left_offset) * s->up_filter_inf.scaleXCr - s->up_filter_inf.addXLum) >> 12) >> 4) - bl_x;
        int bPbH = ((( (y0 + ctb_size - s->ps.sps->conf_win.top_offset ) * s->up_filter_inf.scaleYCr - s->up_filter_inf.addYLum) >> 12) >> 4) - bl_y;

        int bl_y2 = av_clip_c(bl_y + bPbH + 2 * MAX_EDGE_CR , 0 , bl_height);
        int bl_x2 = av_clip_c(bl_x + bPbW + 2 * MAX_EDGE_CR , 0 , bl_width );

        int bl_edge_top  = !bl_y ? MAX_EDGE_CR : 0;
        int bl_edge_left = !bl_x ? MAX_EDGE_CR : 0;

        int bl_edge_bottom = bl_y2 == bl_height ? MAX_EDGE_CR : 0;
        int bl_edge_right  = bl_x2 == bl_width  ? MAX_EDGE_CR : 0;

        bPbH = bl_y2 - bl_y;
        bPbW = bl_x2 - bl_x;

        for (cr = 1; cr <= 2; cr++) {

            bl_stride = bl_frame->linesize[cr] / bl_sample_size;

            tmp1 = ((uint8_t *)s->HEVClc->edge_emu_buffer_up_h);

            src =  ((uint8_t *)bl_frame->data[cr]) + (bl_y * bl_stride + bl_x) * bl_sample_size;

            if(bl_edge_left || bl_edge_right){

                tmp1 = (uint8_t *)s->HEVClc->edge_emu_buffer_up_h;

                if(bl_edge_left)
                    tmp1 += MAX_EDGE_CR * sample_size;

                s->vdsp.emulated_edge_up_cr_h((uint8_t *)tmp1, (uint8_t *)src ,
                                             bl_stride,
                                             bPbW, bPbH,
                                             bl_edge_left , bl_edge_right);

                src = tmp1;
                bl_stride = MAX_EDGE_BUFFER_STRIDE;
            }


            tmp0 = ((uint16_t *)s->HEVClc->edge_emu_buffer_up_v) + (MAX_EDGE_CR * MAX_EDGE_BUFFER_STRIDE);

            s->hevcdsp.upsample_filter_block_cr_h[s->up_filter_inf.idx]((uint16_t *)tmp0, MAX_EDGE_BUFFER_STRIDE, (uint8_t *)src, bl_stride, x0, bl_x,
                                                                          ePbW, bPbH, el_width,
                                                                          &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);

            if(bl_edge_top || bl_edge_bottom)
                s->vdsp.emulated_edge_up_cr_v((uint8_t *)tmp0,
                                             ePbW, bPbH,
                                             bl_edge_top, bl_edge_bottom);

            s->hevcdsp.upsample_filter_block_cr_v[s->up_filter_inf.idx]((uint8_t *)ref0->frame->data[cr] , ref0->frame->linesize[1], (uint16_t *)tmp0 , MAX_EDGE_BUFFER_STRIDE,
                                                                          bl_y , x0, y0, ePbW, ePbH, el_width, el_height,
                                                                          &s->ps.sps->scaled_ref_layer_window[ref_layer_id], &s->up_filter_inf);
        }
    }
    s->is_upsampled[((y0) / ctb_size * s->ps.sps->ctb_width) + ((x0) / ctb_size)] = 1;
}

void ff_upscale_mv_block(HEVCContext *s, int ctb_x, int ctb_y) {
    HEVCFrame *bl_frame = s->BL_frame;//fixme : we don't have to enter this function for AVC base
//    H264Picture *bl_frame = s->BL_frame;
	int xEL, yEL, xBL, yBL, list, Ref_pre_unit, pre_unit, pre_unit_col;
    int pic_width_in_min_pu = s->ps.sps->width>>s->ps.sps->log2_min_pu_size;
    int pic_height_in_min_pu = s->ps.sps->height>>s->ps.sps->log2_min_pu_size;
    int pic_width_in_min_puBL = bl_frame->frame->width >> s->ps.sps->log2_min_pu_size;
//    int pic_width_in_min_puBL = bl_frame->f->width >> s->ps.sps->log2_min_pu_size;
    int ctb_size = 1 << s->ps.sps->log2_ctb_size;
    int min_pu_size = 1 << s->ps.sps->log2_min_pu_size;
    int nb_list = s->sh.slice_type==B_SLICE ? 2:1, i, j;
    HEVCFrame *refBL = s->BL_frame;
    HEVCFrame *refEL = s->inter_layer_ref;

    for(yEL=ctb_y; yEL < ctb_y+ctb_size && yEL<s->ps.sps->height; yEL+=16) {
        for(xEL=ctb_x; xEL < ctb_x+ctb_size && xEL<s->ps.sps->width; xEL+=16) {
            xBL = (((av_clip_c(xEL, 0, s->ps.sps->width  -1) + 8 - s->ps.sps->conf_win.left_offset)*s->up_filter_inf.scaleXLum + (1<<15)) >> 16) + 4;
            yBL = (((av_clip_c(yEL, 0, s->ps.sps->height -1) + 8 - s->ps.sps->conf_win.top_offset )*s->up_filter_inf.scaleYLum + (1<<15)) >> 16) + 4;
            pre_unit = ((yEL>>s->ps.sps->log2_min_pu_size)*pic_width_in_min_pu) + (xEL>>s->ps.sps->log2_min_pu_size);

            if((xBL& 0xFFFFFFF0) < bl_frame->frame->width && (yBL& 0xFFFFFFF0) < bl_frame->frame->height) {
//            if((xBL& 0xFFFFFFF0) < bl_frame->f->width && (yBL& 0xFFFFFFF0) < bl_frame->f->height) {

                xBL >>= 4;
                xBL <<= 4-s->ps.sps->log2_min_pu_size; // 4 <==> xBL & 0xFFFFFFF0
                yBL >>= 4;
                yBL <<= 4-s->ps.sps->log2_min_pu_size; // 4 <==> yBL & 0xFFFFFFF0
                Ref_pre_unit = (yBL*pic_width_in_min_puBL)+xBL;
                if(refBL->tab_mvf[Ref_pre_unit].pred_flag) {
                    if (s->up_filter_inf.idx == SNR) {
                        memcpy(&refEL->tab_mvf[pre_unit], &refBL->tab_mvf[Ref_pre_unit], sizeof(MvField));
                    } else {
                        for( list=0; list < nb_list; list++) {
                            refEL->tab_mvf[pre_unit].mv[list].x  = av_clip_c( (s->sh.MvScalingFactor[s->nuh_layer_id][0] * refBL->tab_mvf[Ref_pre_unit].mv[list].x + 127 + (s->sh.MvScalingFactor[s->nuh_layer_id][0] * refBL->tab_mvf[Ref_pre_unit].mv[list].x < 0)) >> 8 , -32768, 32767);
                            refEL->tab_mvf[pre_unit].mv[list].y = av_clip_c( (s->sh.MvScalingFactor[s->nuh_layer_id][1] * refBL->tab_mvf[Ref_pre_unit].mv[list].y + 127 + (s->sh.MvScalingFactor[s->nuh_layer_id][1] * refBL->tab_mvf[Ref_pre_unit].mv[list].y < 0)) >> 8, -32768, 32767);
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
            for(i=0; i < 16; i+=min_pu_size) {
                for(j=0; j < 16; j+=min_pu_size) {
                    if(!i && !j)
                        continue;
                    if(!i && ((yEL+j+1)>>s->ps.sps->log2_min_pu_size) < pic_height_in_min_pu) {
                        pre_unit_col = (((yEL+j+1)>>s->ps.sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL)>>s->ps.sps->log2_min_pu_size);
                        memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
                        continue;
                    }

                    if(!j && ((xEL+i+1)>>s->ps.sps->log2_min_pu_size) < pic_width_in_min_pu) {
                        pre_unit_col = ((yEL>>s->ps.sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL+i+1)>>s->ps.sps->log2_min_pu_size);
                        memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
                        continue;
                    }

                    if(((xEL+i+1)>>s->ps.sps->log2_min_pu_size) < pic_width_in_min_pu && ((yEL+j+1)>>s->ps.sps->log2_min_pu_size) < pic_height_in_min_pu) {
                        pre_unit_col = (((yEL+j+1)>>s->ps.sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL+i+1)>>s->ps.sps->log2_min_pu_size);
                        memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
                    }
                }/*
                if( ((xEL+1)>>s->ps.sps->log2_min_pu_size) < pic_width_in_min_pu) {
                    pre_unit_col = ((yEL>>s->ps.sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL+1)>>s->ps.sps->log2_min_pu_size);
                    memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
                }
                if( ((yEL+1)>>s->ps.sps->log2_min_pu_size) < pic_height_in_min_pu) {
                    pre_unit_col = (((yEL+1)>>s->ps.sps->log2_min_pu_size)*pic_width_in_min_pu) + ((xEL)>>s->ps.sps->log2_min_pu_size);
                    memcpy(&refEL->tab_mvf[pre_unit_col], &refEL->tab_mvf[pre_unit], sizeof(MvField));
                }*/
            }
        }
    }
}

void ff_upsample_block(HEVCContext *s, HEVCFrame *ref0, int x0, int y0, int nPbW, int nPbH) {

    int ctb_size =  1<<s->ps.sps->log2_ctb_size;
    int log2_ctb =  s->ps.sps->log2_ctb_size;
    int ctb_x0   =  (av_clip(x0, 0, s->ps.sps->width) >> log2_ctb) << log2_ctb;
    int ctb_y0   =  (av_clip(y0, 0, s->ps.sps->height) >> log2_ctb) << log2_ctb;

    if ((x0 - ctb_x0) < MAX_EDGE &&
        ctb_x0 > ctb_size        &&
        !s->is_upsampled[(ctb_y0 / ctb_size * s->ps.sps->ctb_width)+((ctb_x0 - ctb_size) / ctb_size)]){
        if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
    	    ff_upscale_mv_block(s, ctb_x0 - ctb_size            , ctb_y0);
        if(s->ps.pps->colour_mapping_enabled_flag) {
            upsample_block_luma_cgs(s, ref0, ctb_x0-ctb_size        , ctb_y0);
            upsample_block_mc_cgs  (s, ref0, (ctb_x0-ctb_size) >> 1 , ctb_y0 >> 1);
        } else {
        upsample_block_luma(s, ref0, ctb_x0-ctb_size        , ctb_y0);
        upsample_block_mc  (s, ref0, (ctb_x0-ctb_size) >> 1 , ctb_y0 >> 1);
        }
    }

    if ((y0 - ctb_y0) < MAX_EDGE &&
        ctb_y0 > ctb_size &&
        !s->is_upsampled[((ctb_y0 - ctb_size) / ctb_size * s->ps.sps->ctb_width) + (ctb_x0 / ctb_size)]){
        if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
    		ff_upscale_mv_block(s, ctb_x0            , ctb_y0 - ctb_size);
        if(s->ps.pps->colour_mapping_enabled_flag) {
            upsample_block_luma_cgs(s, ref0, ctb_x0      , ctb_y0 - ctb_size);
            upsample_block_mc_cgs  (s, ref0, ctb_x0 >> 1 , (ctb_y0 - ctb_size) >> 1);
        } else {
          upsample_block_luma(s, ref0, ctb_x0      , ctb_y0 - ctb_size);
          upsample_block_mc  (s, ref0, ctb_x0 >> 1 , (ctb_y0 - ctb_size) >> 1);
        }
    }

    if(!s->is_upsampled[(ctb_y0 / ctb_size * s->ps.sps->ctb_width) + (ctb_x0 / ctb_size)]){
        if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
    		ff_upscale_mv_block(s, ctb_x0           , ctb_y0);
        if(s->ps.pps->colour_mapping_enabled_flag) {
            upsample_block_luma_cgs(s, ref0, ctb_x0     , ctb_y0);
            upsample_block_mc_cgs  (s, ref0, ctb_x0 >> 1, ctb_y0 >> 1);
        } else {
          upsample_block_luma(s, ref0, ctb_x0     , ctb_y0);
          upsample_block_mc  (s, ref0, ctb_x0 >> 1, ctb_y0 >> 1);
        }
    }

    if((((x0 + nPbW + MAX_EDGE) >> log2_ctb) << log2_ctb) > ctb_x0 && ((ctb_x0 + ctb_size) < s->ps.sps->width) &&
       !s->is_upsampled[(ctb_y0 / ctb_size * s->ps.sps->ctb_width) + ((ctb_x0 + ctb_size) / ctb_size)]){
        if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
    	    ff_upscale_mv_block(s, ctb_x0 + ctb_size, ctb_y0);
        if(s->ps.pps->colour_mapping_enabled_flag) {
            upsample_block_luma_cgs(s, ref0, ctb_x0 + ctb_size       , ctb_y0);
            upsample_block_mc_cgs  (s, ref0, (ctb_x0 + ctb_size) >> 1, ctb_y0 >> 1);
        } else {
          upsample_block_luma(s, ref0, ctb_x0 + ctb_size       , ctb_y0);
          upsample_block_mc  (s, ref0, (ctb_x0 + ctb_size) >> 1, ctb_y0 >> 1);
        }
    }

    if((((y0 + nPbH + MAX_EDGE) >> log2_ctb) << log2_ctb) > ctb_y0 &&
       ((ctb_y0 + ctb_size) < s->ps.sps->height)) {
        if (!s->is_upsampled[((ctb_y0 + ctb_size) / ctb_size * s->ps.sps->ctb_width) + (ctb_x0 / ctb_size)]){
            if (s->threads_type & FF_THREAD_FRAME ) {
                int bl_y = ctb_y0 + ctb_size + ctb_size * 2 + 9;
                bl_y = (( (bl_y  - s->ps.sps->conf_win.top_offset) * s->up_filter_inf.scaleYLum + s->up_filter_inf.addYLum) >> 12) >> 4;
                ff_thread_await_progress(&((HEVCFrame *)s->BL_frame)->tf, bl_y, 0);
            }
            if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
                ff_upscale_mv_block(s, ctb_x0, ctb_y0 + ctb_size);
            if(s->ps.pps->colour_mapping_enabled_flag) {
                upsample_block_luma_cgs(s, ref0, ctb_x0     , ctb_y0 + ctb_size);
                upsample_block_mc_cgs  (s, ref0, ctb_x0 >> 1, (ctb_y0 + ctb_size) >> 1);
            } else {
              upsample_block_luma(s, ref0, ctb_x0     , ctb_y0 + ctb_size);
              upsample_block_mc  (s, ref0, ctb_x0 >> 1, (ctb_y0 + ctb_size) >> 1);
            }
        }
        if((((x0 + nPbW + MAX_EDGE) >> log2_ctb) << log2_ctb) > ctb_x0 && ((ctb_x0 + ctb_size) < s->ps.sps->width) &&
           !s->is_upsampled[((ctb_y0 + ctb_size) / ctb_size * s->ps.sps->ctb_width) + ((ctb_x0 + ctb_size) / ctb_size)]){
            if(!s->ps.vps->vps_nonHEVCBaseLayerFlag)
                ff_upscale_mv_block(s, ctb_x0 + ctb_size, ctb_y0 + ctb_size);
            if(s->ps.pps->colour_mapping_enabled_flag) {
              upsample_block_luma_cgs(s, ref0, (ctb_x0 + ctb_size)    , ctb_y0 + ctb_size);
              upsample_block_mc_cgs  (s, ref0, (ctb_x0 + ctb_size) >> 1, (ctb_y0 + ctb_size) >> 1);
            } else {
              upsample_block_luma(s, ref0, (ctb_x0 + ctb_size)    , ctb_y0 + ctb_size);
              upsample_block_mc  (s, ref0, (ctb_x0 + ctb_size) >> 1, (ctb_y0 + ctb_size) >> 1);
            }
        }
    }
}


