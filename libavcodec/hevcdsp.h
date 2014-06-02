/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 - 2014 Pierre-Edouard Lepere
 *
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

#ifndef AVCODEC_HEVCDSP_H
#define AVCODEC_HEVCDSP_H

struct AVFrame;
struct UpsamplInf;
struct HEVCWindow;
//#ifdef SVC_EXTENSION
#define NTAPS_LUMA 8
#define NTAPS_CHROMA 4
#define US_FILTER_PREC  6

#define MAX_EDGE  4
#define MAX_EDGE_CR  2
#define N_SHIFT (20-8)
#define I_OFFSET (1 << (N_SHIFT - 1))




/*      Upsampling filters      */
DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma[16][4])=
{
    {  0,  64,   0,  0},
    { -2,  62,   4,  0},
    { -2,  58,  10, -2},
    { -4,  56,  14, -2},
    { -4,  54,  16, -2},
    { -6,  52,  20, -2},
    { -6,  46,  28, -4},
    { -4,  42,  30, -4},
    { -4,  36,  36, -4},
    { -4,  30,  42, -4},
    { -4,  28,  46, -6},
    { -2,  20,  52, -6},
    { -2,  16,  54, -4},
    { -2,  14,  56, -4},
    { -2,  10,  58, -2},
    {  0,   4,  62, -2}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma[16][8] )=
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    {  0,  1,  -3,  63,   4,  -2,  1,  0},
    { -1,  2,  -5,  62,   8,  -3,  1,  0},
    { -1,  3,  -8,  60,  13,  -4,  1,  0},
    { -1,  4, -10,  58,  17,  -5,  1,  0},
    { -1,  4, -11,  52,  26,  -8,  3, -1},
    { -1,  3,  -9,  47,  31, -10,  4, -1},
    { -1,  4, -11,  45,  34, -10,  4, -1},
    { -1,  4, -11,  40,  40, -11,  4, -1},
    { -1,  4, -10,  34,  45, -11,  4, -1},
    { -1,  4, -10,  31,  47,  -9,  3, -1},
    { -1,  3,  -8,  26,  52, -11,  4, -1},
    {  0,  1,  -5,  17,  58, -10,  4, -1},
    {  0,  1,  -4,  13,  60,  -8,  3, -1},
    {  0,  1,  -3,   8,  62,  -5,  2, -1},
    {  0,  1,  -2,   4,  63,  -3,  1,  0}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x2[2][8] )= /*0 , 8 */
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    { -1,  4, -11,  40,  40, -11,  4, -1}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x1_5[3][8] )= /* 0, 11, 5 */
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    { -1,  3,  -8,  26,  52, -11,  4, -1},
    { -1,  4, -11,  52,  26,  -8,  3, -1}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x1_5[3][4])= /* 0, 11, 5 */
{
    {  0,  64,   0,  0},
    { -2,  20,  52, -6},
    { -6,  52,  20, -2}
};
DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_x1_5chroma[3][4])=
{
    {  0,   4,  62, -2},
    { -4,  30,  42, -4},
    { -4,  54,  16, -2}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x2[2][4])=
{
    {  0,  64,   0,  0},
    { -4,  36,  36, -4}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x2_v[2][4])=
{
        { -2,  10,  58, -2},
        { -6,  46,  28, -4},
};
//#endif

typedef struct SAOParams {
    int offset_abs[3][4];   ///< sao_offset_abs
    int offset_sign[3][4];  ///< sao_offset_sign

    int band_position[3];   ///< sao_band_position

    int eo_class[3];        ///< sao_eo_class

    int offset_val[3][5];   ///<SaoOffsetVal

    uint8_t type_idx[3];    ///< sao_type_idx
} SAOParams;

typedef struct HEVCDSPContext {
    void (*put_pcm)(uint8_t *_dst, ptrdiff_t _stride, int width, int height,
                    struct GetBitContext *gb, int pcm_bit_depth);

    void (*transquant_bypass[4])(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);

    void (*transform_skip[2])(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);

    void (*transform_4x4_luma_add)(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);

    void (*transform_add[4])(uint8_t *dst, int16_t *coeffs, ptrdiff_t _stride, int col_limit);

    void (*transform_dc_add[4])(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);

    void (*sao_band_filter)( uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);

    void (*sao_edge_filter[2])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride,  struct SAOParams *sao, int *borders, int _width, int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);

    void (*put_hevc_qpel[10][2][2])(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);

    void (*put_hevc_qpel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2, ptrdiff_t src2stride,
                                         int height, int denom, int wx0, int wx1,
                                         int ox0, int ox1, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel[10][2][2])(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);

    void (*put_hevc_epel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2, ptrdiff_t src2stride,
                                         int height, int denom, int wx0, int ox0, int wx1,
                                         int ox1, intptr_t mx, intptr_t my, int width);


    void (*hevc_h_loop_filter_luma)(uint8_t *_pix, ptrdiff_t _stride, int *_beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_luma)(uint8_t *_pix, ptrdiff_t _stride, int *_beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_h_loop_filter_chroma)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_chroma)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_h_loop_filter_luma_c)(uint8_t *_pix, ptrdiff_t _stride, int *_beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_luma_c)(uint8_t *_pix, ptrdiff_t _stride, int *_beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_h_loop_filter_chroma_c)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_chroma_c)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

    void (*upsample_base_layer_frame)  (struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
    void (*upsample_filter_block_luma_h[3])(
                                         int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                         const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void (*upsample_filter_block_luma_v[3])(
                                         uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                         int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                         const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void (*upsample_filter_block_cr_h[3])(
                                           int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                           int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                           const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void (*upsample_filter_block_cr_v[3])(
                                           uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                           int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                           const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
} HEVCDSPContext;

void ff_hevc_dsp_init(HEVCDSPContext *hpc, int bit_depth);

extern const int8_t ff_hevc_epel_filters[7][4];
extern const int8_t ff_hevc_qpel_filters[3][16];

void ff_hevcdsp_init_x86(HEVCDSPContext *c, const int bit_depth);
void ff_hevcdsp_init_arm(HEVCDSPContext *c, const int bit_depth);
#endif /* AVCODEC_HEVCDSP_H */
