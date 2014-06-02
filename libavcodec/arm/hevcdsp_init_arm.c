/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
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

#include <stdint.h>
#include <stddef.h>

#include "libavutil/attributes.h"
#include "libavutil/arm/cpu.h"
#include "libavcodec/hevcdsp.h"

static void (*put_hevc_qpel_neon[4][4])(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, int width);

static void (*put_hevc_qpel_uw_neon[4][4])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);
void ff_hevc_v_loop_filter_luma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_luma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_v_loop_filter_chroma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_chroma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_put_pixels_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
int ff_hevc_put_weighted_pred_avg_neon_8(uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, int16_t *src2,
                                   ptrdiff_t srcstride, int width, int height);

void ff_hevc_put_qpel_neon_wrapper(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {

    put_hevc_qpel_neon[my][mx](dst, dststride, src, srcstride, height, width);
}

void ff_hevc_put_qpel_uni_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {

    put_hevc_qpel_uw_neon[my][mx](dst, dststride, src, srcstride, width, height, NULL, 0);
}

void ff_hevc_put_qpel_bi_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, intptr_t mx, intptr_t my, int width) {
    put_hevc_qpel_uw_neon[my][mx](dst, dststride, src, srcstride, width, height, src2, src2stride);
}

#define QPEL_FUNC(name) \
    void name(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride, \
                                   int height, int width)

QPEL_FUNC(ff_hevc_put_qpel_v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3v3_neon_8);
#undef QPEL_FUNC

#define QPEL_FUNC_UW(name) \
    void name(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, \
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3v3_neon_8);
#undef QPEL_FUNC_UW

void ff_hevc_put_epel_h_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
void ff_hevc_put_epel_v_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
void ff_hevc_put_epel_hv_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
void ff_hevc_transform_4x4_add_neon_8(uint8_t *_dst, int16_t *coeffs,
                                    ptrdiff_t stride);
void ff_hevc_transform_8x8_add_neon_8(uint8_t *_dst, int16_t *coeffs,
                                    ptrdiff_t stride);
void ff_hevc_transform_16x16_add_neon_8(uint8_t *_dst, int16_t *coeffs,
                                    ptrdiff_t stride);

int ff_hevc_put_qpel_uw_pixels_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);



static av_cold void hevcdsp_init_neon(HEVCDSPContext *c, const int bit_depth)
{
#if HAVE_NEON
    if (bit_depth == 8) {
        int x;
        c->hevc_v_loop_filter_luma     = ff_hevc_v_loop_filter_luma_neon;
        c->hevc_h_loop_filter_luma     = ff_hevc_h_loop_filter_luma_neon;
        c->hevc_v_loop_filter_chroma   = ff_hevc_v_loop_filter_chroma_neon;
        c->hevc_h_loop_filter_chroma   = ff_hevc_h_loop_filter_chroma_neon;
        //c->put_weighted_pred_avg       = ff_hevc_put_weighted_pred_avg_neon_8;
        //put_hevc_qpel_neon[0][0]         = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_neon[1][0]         = ff_hevc_put_qpel_v1_neon_8;
        put_hevc_qpel_neon[2][0]         = ff_hevc_put_qpel_v2_neon_8;
        put_hevc_qpel_neon[3][0]         = ff_hevc_put_qpel_v3_neon_8;
        put_hevc_qpel_neon[0][1]         = ff_hevc_put_qpel_h1_neon_8;
        put_hevc_qpel_neon[0][2]         = ff_hevc_put_qpel_h2_neon_8;
        put_hevc_qpel_neon[0][3]         = ff_hevc_put_qpel_h3_neon_8;
        put_hevc_qpel_neon[1][1]         = ff_hevc_put_qpel_h1v1_neon_8;
        put_hevc_qpel_neon[1][2]         = ff_hevc_put_qpel_h2v1_neon_8;
        put_hevc_qpel_neon[1][3]         = ff_hevc_put_qpel_h3v1_neon_8;
        put_hevc_qpel_neon[2][1]         = ff_hevc_put_qpel_h1v2_neon_8;
        put_hevc_qpel_neon[2][2]         = ff_hevc_put_qpel_h2v2_neon_8;
        put_hevc_qpel_neon[2][3]         = ff_hevc_put_qpel_h3v2_neon_8;
        put_hevc_qpel_neon[3][1]         = ff_hevc_put_qpel_h1v3_neon_8;
        put_hevc_qpel_neon[3][2]         = ff_hevc_put_qpel_h2v3_neon_8;
        put_hevc_qpel_neon[3][3]         = ff_hevc_put_qpel_h3v3_neon_8;
        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_uw_neon[1][0]      = ff_hevc_put_qpel_uw_v1_neon_8;
        put_hevc_qpel_uw_neon[2][0]      = ff_hevc_put_qpel_uw_v2_neon_8;
        put_hevc_qpel_uw_neon[3][0]      = ff_hevc_put_qpel_uw_v3_neon_8;
        put_hevc_qpel_uw_neon[0][1]      = ff_hevc_put_qpel_uw_h1_neon_8;
        put_hevc_qpel_uw_neon[0][2]      = ff_hevc_put_qpel_uw_h2_neon_8;
        put_hevc_qpel_uw_neon[0][3]      = ff_hevc_put_qpel_uw_h3_neon_8;
        put_hevc_qpel_uw_neon[1][1]      = ff_hevc_put_qpel_uw_h1v1_neon_8;
        put_hevc_qpel_uw_neon[1][2]      = ff_hevc_put_qpel_uw_h2v1_neon_8;
        put_hevc_qpel_uw_neon[1][3]      = ff_hevc_put_qpel_uw_h3v1_neon_8;
        put_hevc_qpel_uw_neon[2][1]      = ff_hevc_put_qpel_uw_h1v2_neon_8;
        put_hevc_qpel_uw_neon[2][2]      = ff_hevc_put_qpel_uw_h2v2_neon_8;
        put_hevc_qpel_uw_neon[2][3]      = ff_hevc_put_qpel_uw_h3v2_neon_8;
        put_hevc_qpel_uw_neon[3][1]      = ff_hevc_put_qpel_uw_h1v3_neon_8;
        put_hevc_qpel_uw_neon[3][2]      = ff_hevc_put_qpel_uw_h2v3_neon_8;
        put_hevc_qpel_uw_neon[3][3]      = ff_hevc_put_qpel_uw_h3v3_neon_8;
        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
            //c->put_hevc_qpel_uni[x][0][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            //c->put_hevc_qpel_bi[x][0][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel_v_neon_8;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel_h_neon_8;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel_hv_neon_8;
        }

       /* c->put_hevc_epel[1][0]         = ff_hevc_put_epel_v_neon_8;
        c->put_hevc_epel[0][1]         = ff_hevc_put_epel_h_neon_8;
        c->put_hevc_epel[1][1]         = ff_hevc_put_epel_hv_neon_8;*/
	//Fixme compilation error in transform optimizations
        /* c->transform_add[0]            = ff_hevc_transform_4x4_add_neon_8;
        c->transform_add[1]            = ff_hevc_transform_8x8_add_neon_8; */
        //c->transform_add[2]            = ff_hevc_transform_16x16_add_neon_8;
    }
#endif // HAVE_NEON
}

av_cold void ff_hevcdsp_init_arm(HEVCDSPContext *c, const int bit_depth)
{
    int cpu_flags = av_get_cpu_flags();

    if (have_neon(cpu_flags))
        hevcdsp_init_neon(c, bit_depth);
}
