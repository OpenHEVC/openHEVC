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

#include "libavutil/attributes.h"
#include "libavutil/arm/cpu.h"
#include "libavcodec/hevcdsp.h"
#include "libavcodec/bit_depth_template.c"
#include "hevcdsp_arm.h"

void ff_hevc_v_loop_filter_luma_neon(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_luma_neon(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_v_loop_filter_chroma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_chroma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_transform_4x4_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_transform_8x8_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_idct_4x4_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_8x8_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_16x16_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_32x32_dc_neon_8(int16_t *coeffs);
void ff_hevc_transform_luma_4x4_neon_8(int16_t *coeffs);
void ff_hevc_add_residual_4x4_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_8x8_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_16x16_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_32x32_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);

#define PUT_PIXELS(name) \
    void name(int16_t *dst, uint8_t *src, \
                                ptrdiff_t srcstride, int height, \
                                intptr_t mx, intptr_t my, int width)
PUT_PIXELS(ff_hevc_put_pixels_w2_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w4_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w6_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w8_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w12_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w16_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w24_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w32_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w48_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w64_neon_8);
#undef PUT_PIXELS

static void (*put_hevc_qpel_neon[4][4])(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, int width);
static void (*put_hevc_qpel_uw_neon[4][4])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);
void ff_hevc_put_qpel_neon_wrapper(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width);
void ff_hevc_put_qpel_uni_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width);
void ff_hevc_put_qpel_bi_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
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

#define QPEL_FUNC_UW_PIX(name) \
    void name(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, \
                                   int height, intptr_t mx, intptr_t my, int width);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w4_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w8_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w16_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w24_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w32_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w48_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w64_neon_8);
#undef QPEL_FUNC_UW_PIX

#define QPEL_FUNC_UW(name) \
    void name(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, \
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_pixels_neon_8);
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

void ff_hevc_put_qpel_neon_wrapper(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {

    put_hevc_qpel_neon[my][mx](dst, MAX_PB_SIZE, src, srcstride, height, width);
}

void ff_hevc_put_qpel_uni_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {

    put_hevc_qpel_uw_neon[my][mx](dst, dststride, src, srcstride, width, height, NULL, 0);
}

void ff_hevc_put_qpel_bi_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width) {
    put_hevc_qpel_uw_neon[my][mx](dst, dststride, src, srcstride, width, height, src2, MAX_PB_SIZE);
}

void ff_hevc_transform_4x4_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_transform_8x8_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_transform_16x16_add_neon_8(uint8_t *_dst, int16_t *coeffs,
                                    ptrdiff_t stride);

void ff_hevc_idct_4x4_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_8x8_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_16x16_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_32x32_dc_neon_8(int16_t *coeffs);

void ff_hevc_transform_luma_4x4_neon_8(int16_t *coeffs);

void ff_hevc_transform_add_4x4_neon_8(uint8_t *_dst, int16_t *coeffs,
                                      ptrdiff_t stride);
void ff_hevc_transform_add_8x8_neon_8(uint8_t *_dst, int16_t *coeffs,
                                      ptrdiff_t stride);
void ff_hevc_transform_add_16x16_neon_8(uint8_t *_dst, int16_t *coeffs,
                                      ptrdiff_t stride);
void ff_hevc_transform_add_32x32_neon_8(uint8_t *_dst, int16_t *coeffs,
                                      ptrdiff_t stride);

void ff_hevc_sao_band_w8_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);
void ff_hevc_sao_band_w16_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);
void ff_hevc_sao_band_w32_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);
void ff_hevc_sao_band_w64_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);

void ff_hevc_sao_edge_eo0_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo1_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo2_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo3_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);

void ff_hevc_sao_edge_eo0_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo1_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo2_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo3_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);

void ff_upsample_filter_block_luma_h_x2_neon( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
void ff_upsample_filter_block_luma_v_x2_neon( uint8_t *_dst, ptrdiff_t _dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                  int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
void ff_upsample_filter_block_cr_h_x2_neon(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
void ff_upsample_filter_block_cr_v_x2_neon( uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
void ff_upsample_filter_block_cr_h_x1_5_neon(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
void ff_upsample_filter_block_cr_v_x1_5_neon( uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
void ff_upsample_filter_block_luma_h_x1_5_neon( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
void ff_upsample_filter_block_luma_v_x1_5_neon( uint8_t *_dst, ptrdiff_t _dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                  int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);


static void ff_hevc_sao_band_neon_wrapper(uint8_t *_dst, uint8_t *_src,
                                    ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                    SAOParams *sao,
                                    int *borders, int width, int height,
                                    int c_idx)
{
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int8_t offset_table[32] = { 0 };
    int k, y, x;
    int shift  = 3; // BIT_DEPTH - 5
    int cwidth = 0;
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t sao_left_class  = sao->band_position[c_idx];

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    for (k = 0; k < 4; k++)
        offset_table[(k + sao_left_class) & 31] = sao_offset_val[k + 1];

    if (height % 8 == 0)
        cwidth = width;

    switch(cwidth){
    case 8:
        ff_hevc_sao_band_w8_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    case 16:
        ff_hevc_sao_band_w16_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    case 32:
        ff_hevc_sao_band_w32_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    case 64:
        ff_hevc_sao_band_w64_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    default:
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++)
                dst[x] = av_clip_pixel(src[x] + offset_table[src[x] >> shift]);
            dst += stride_dst;
            src += stride_src;
        }
    }
}

#define CMP(a, b) ((a) > (b) ? 1 : ((a) == (b) ? 0 : -1))
static void ff_hevc_sao_edge_neon_wrapper(uint8_t *_dst, uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  SAOParams *sao,
                                  int width, int height,
                                  int c_idx)
{
    static const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };
    static const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    int8_t sao_offset_val[8];  // padding of 3 for vld
    uint8_t eo = sao->eo_class[c_idx];
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int a_stride, b_stride;
    int src_offset = 0;
    int dst_offset = 0;
    int x, y;
    int cwidth = 0;

    for (x = 0; x < 5; x++) {
        sao_offset_val[x] = sao->offset_val[c_idx][edge_idx[x]];
    }

    if (height % 8 == 0)
        cwidth = width;

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);
    switch (cwidth) {
    case 32:
        switch(eo) {
        case 0:
            ff_hevc_sao_edge_eo0_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 1:
            ff_hevc_sao_edge_eo1_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 2:
            ff_hevc_sao_edge_eo2_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 3:
            ff_hevc_sao_edge_eo3_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        }
        break;
    case 64:
        switch(eo) {
        case 0:
            ff_hevc_sao_edge_eo0_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 1:
            ff_hevc_sao_edge_eo1_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 2:
            ff_hevc_sao_edge_eo2_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 3:
            ff_hevc_sao_edge_eo3_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        }
        break;
    default:
        a_stride = pos[eo][0][0] + pos[eo][0][1] * stride_src;
        b_stride = pos[eo][1][0] + pos[eo][1][1] * stride_src;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                int diff0         = CMP(src[x + src_offset], src[x + src_offset + a_stride]);
                int diff1         = CMP(src[x + src_offset], src[x + src_offset + b_stride]);
                int idx           = diff0 + diff1;
                if (idx)
                    dst[x] = av_clip_pixel(src[x] + sao_offset_val[idx+2]);
            }
            src += stride_src;
            dst += stride_dst;
        }
    }
}
#undef CMP
av_cold void ff_hevcdsp_init_neon(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        c->hevc_v_loop_filter_luma     = ff_hevc_v_loop_filter_luma_neon;
        c->hevc_h_loop_filter_luma     = ff_hevc_h_loop_filter_luma_neon;
        c->hevc_v_loop_filter_chroma   = ff_hevc_v_loop_filter_chroma_neon;
        c->hevc_h_loop_filter_chroma   = ff_hevc_h_loop_filter_chroma_neon;
        c->idct[0]                     = ff_hevc_transform_4x4_neon_8;
        c->idct[1]                     = ff_hevc_transform_8x8_neon_8;
        c->idct_dc[0]                  = ff_hevc_idct_4x4_dc_neon_8;
        c->idct_dc[1]                  = ff_hevc_idct_8x8_dc_neon_8;
        c->idct_dc[2]                  = ff_hevc_idct_16x16_dc_neon_8;
        c->idct_dc[3]                  = ff_hevc_idct_32x32_dc_neon_8;
        c->transform_add[0]            = ff_hevc_transform_add_4x4_neon_8;
        c->transform_add[1]            = ff_hevc_transform_add_8x8_neon_8;
        c->transform_add[2]            = ff_hevc_transform_add_16x16_neon_8;
        c->transform_add[3]            = ff_hevc_transform_add_32x32_neon_8;
        c->idct_4x4_luma               = ff_hevc_transform_luma_4x4_neon_8;
        c->sao_band_filter             = ff_hevc_sao_band_neon_wrapper;
        c->sao_edge_filter             = ff_hevc_sao_edge_neon_wrapper;
        c->upsample_filter_block_luma_h[1] = ff_upsample_filter_block_luma_h_x2_neon;
        c->upsample_filter_block_luma_v[1] = ff_upsample_filter_block_luma_v_x2_neon;
        c->upsample_filter_block_cr_h[1]   = ff_upsample_filter_block_cr_h_x2_neon;
        c->upsample_filter_block_cr_v[1]   = ff_upsample_filter_block_cr_v_x2_neon;
        c->upsample_filter_block_cr_h[2]   = ff_upsample_filter_block_cr_h_x1_5_neon;
        c->upsample_filter_block_cr_v[2]   = ff_upsample_filter_block_cr_v_x1_5_neon;
        c->upsample_filter_block_luma_h[2] = ff_upsample_filter_block_luma_h_x1_5_neon;
        c->upsample_filter_block_luma_v[2] = ff_upsample_filter_block_luma_v_x1_5_neon;
        //c->put_weighted_pred_avg       = ff_hevc_put_weighted_pred_avg_neon_8;
        put_hevc_qpel_neon[1][0]       = ff_hevc_put_qpel_v1_neon_8;
        put_hevc_qpel_neon[2][0]       = ff_hevc_put_qpel_v2_neon_8;
        put_hevc_qpel_neon[3][0]       = ff_hevc_put_qpel_v3_neon_8;
        put_hevc_qpel_neon[0][1]       = ff_hevc_put_qpel_h1_neon_8;
        put_hevc_qpel_neon[0][2]       = ff_hevc_put_qpel_h2_neon_8;
        put_hevc_qpel_neon[0][3]       = ff_hevc_put_qpel_h3_neon_8;
        put_hevc_qpel_neon[1][1]       = ff_hevc_put_qpel_h1v1_neon_8;
        put_hevc_qpel_neon[1][2]       = ff_hevc_put_qpel_h2v1_neon_8;
        put_hevc_qpel_neon[1][3]       = ff_hevc_put_qpel_h3v1_neon_8;
        put_hevc_qpel_neon[2][1]       = ff_hevc_put_qpel_h1v2_neon_8;
        put_hevc_qpel_neon[2][2]       = ff_hevc_put_qpel_h2v2_neon_8;
        put_hevc_qpel_neon[2][3]       = ff_hevc_put_qpel_h3v2_neon_8;
        put_hevc_qpel_neon[3][1]       = ff_hevc_put_qpel_h1v3_neon_8;
        put_hevc_qpel_neon[3][2]       = ff_hevc_put_qpel_h2v3_neon_8;
        put_hevc_qpel_neon[3][3]       = ff_hevc_put_qpel_h3v3_neon_8;
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
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
//            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel_v_neon_8;
//            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel_h_neon_8;
//            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel_hv_neon_8;
        }
        c->put_hevc_qpel[0][0][0]  = ff_hevc_put_pixels_w2_neon_8;
        c->put_hevc_qpel[1][0][0]  = ff_hevc_put_pixels_w4_neon_8;
        c->put_hevc_qpel[2][0][0]  = ff_hevc_put_pixels_w6_neon_8;
        c->put_hevc_qpel[3][0][0]  = ff_hevc_put_pixels_w8_neon_8;
        c->put_hevc_qpel[4][0][0]  = ff_hevc_put_pixels_w12_neon_8;
        c->put_hevc_qpel[5][0][0]  = ff_hevc_put_pixels_w16_neon_8;
        c->put_hevc_qpel[6][0][0]  = ff_hevc_put_pixels_w24_neon_8;
        c->put_hevc_qpel[7][0][0]  = ff_hevc_put_pixels_w32_neon_8;
        c->put_hevc_qpel[8][0][0]  = ff_hevc_put_pixels_w48_neon_8;
        c->put_hevc_qpel[9][0][0]  = ff_hevc_put_pixels_w64_neon_8;

        c->put_hevc_qpel_uni[1][0][0]  = ff_hevc_put_qpel_uw_pixels_w4_neon_8;
        c->put_hevc_qpel_uni[3][0][0]  = ff_hevc_put_qpel_uw_pixels_w8_neon_8;
        c->put_hevc_qpel_uni[5][0][0]  = ff_hevc_put_qpel_uw_pixels_w16_neon_8;
        c->put_hevc_qpel_uni[6][0][0]  = ff_hevc_put_qpel_uw_pixels_w24_neon_8;
        c->put_hevc_qpel_uni[7][0][0]  = ff_hevc_put_qpel_uw_pixels_w32_neon_8;
        c->put_hevc_qpel_uni[8][0][0]  = ff_hevc_put_qpel_uw_pixels_w48_neon_8;
        c->put_hevc_qpel_uni[9][0][0]  = ff_hevc_put_qpel_uw_pixels_w64_neon_8;
    }
}
