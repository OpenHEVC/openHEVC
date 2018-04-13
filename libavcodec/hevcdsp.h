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
//#include "hevcdec.h"
#include "stdint.h"
#include "config.h"

#include "get_bits.h"
struct AVFrame;
struct UpsamplInf;
struct HEVCWindow;
struct SAOParams;
struct HEVCContext;
struct HEVCLocalContext;
struct HEVCTransformContext;

#define NTAPS_LUMA 8
#define NTAPS_CHROMA 4
#define US_FILTER_PREC  6

#define MAX_EDGE  4
#define MAX_EDGE_CR  2
#define N_SHIFT (20 - BIT_DEPTH)
#define I_OFFSET (1 << (N_SHIFT - 1))
#define CONFIG_OH_OPTIM 0


typedef struct HEVCDSPContext {
    void (*put_pcm)(uint8_t *_dst, ptrdiff_t _stride, int width, int height,
                    struct GetBitContext *gb, int pcm_bit_depth);

    void (*transform_add[4])(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);

    void (*transform_skip)(int16_t *coeffs, int16_t log2_size);

    void (*transform_rdpcm)(int16_t *coeffs, int16_t log2_size, int mode);

    void (*idct_4x4_luma)(int16_t *coeffs);

    void (*idct[4])(int16_t *coeffs, int col_limit);

    void (*idct_dc[4])(int16_t *coeffs);

    void (*sao_band_filter)( uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src, struct SAOParams *sao, int *borders, int width, int height, int c_idx);

    void (*sao_edge_filter)(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst,
                            ptrdiff_t stride_src, struct SAOParams *sao, int width,
                            int height, int c_idx);
#if OHCONFIG_AMT
    ///* idct_emt[pred_mode][emt_tr_idx][log2_tr_size_minus2](...) *///
//    void (*idct2_emt_v[7][5])(int16_t *coeffs, int16_t *tmp);
//    void (*idct2_emt_h[7][5])(int16_t *tmp,    int16_t *dst);
    void (*idct2_emt_v2[4][8][8])(int16_t *coeffs, int16_t *tmp, const int16_t * dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);
    void (*idct2_emt_h2[4][8])(int16_t *tmp,    int16_t *dst,const  int16_t * dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);
//    void (*emt_it_c)(void *s, struct HEVCLocalContext *lc, struct HEVCTransformContext *tr_ctx, int16_t *tmp, int h, int v,int size);
//    void (*emt_it_luma)(void *s,struct HEVCLocalContext *lc,struct HEVCTransformContext *tr_ctx, int16_t *tmp, int h, int v,int size);
    void (*it_amt_dc[4])(int16_t *coeffs, int16_t *tmp, const int16_t * dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);
#endif

    void (*sao_edge_restore[2])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,  struct SAOParams *sao, int *borders, int _width, int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);
    /* put_hevc_qpel[x] block widths
       x     width
       --------------
       0 ->  2 pixels
       1 ->  4 pixels
       2 ->  6 pixels
       3 ->  8 pixels
       4 -> 12 pixels
       5 -> 16 pixels
       6 -> 24 pixels
       7 -> 32 pixels
       8 -> 48 pixels
       9 -> 64 pixels
    */
    void (*put_hevc_qpel[10][2][2])(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);

    void (*put_hevc_qpel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2,
                                         int height, int denom, int wx0, int wx1,
                                         int ox0, int ox1, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel[10][2][2])(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);

    void (*put_hevc_epel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2,
                                         int height, int denom, int wx0, int ox0, int wx1,
                                         int ox1, intptr_t mx, intptr_t my, int width);


    void (*hevc_h_loop_filter_luma)(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_luma)(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_h_loop_filter_chroma)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_chroma)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_h_loop_filter_luma_c)(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_luma_c)(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_h_loop_filter_chroma_c)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
    void (*hevc_v_loop_filter_chroma_c)(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

    void (*colorMapping)(void * pc3DAsymLUT, struct AVFrame *src, struct AVFrame *dst);
    void (*upsample_base_layer_frame)  (struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
    void (*upsample_filter_block_luma_h[3])(
                                         int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                         const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void (*upsample_filter_block_luma_cgs_h[3])(
                                             int16_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
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
    void (*map_color_block)(const void *pc3DAsymLUT, uint8_t *src_y, uint8_t *src_u, uint8_t *src_v,
                            uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v,
                            int src_stride, int src_stride_c,
                            int dst_stride, int dst_stride_c,
                            int dst_width, int dst_height,
                            int is_bound_r,int is_bound_b, int is_bound_t,
                            int is_bound_l);
} HEVCDSPContext;

void ff_hevc_dsp_init(HEVCDSPContext *hpc, int bit_depth);
void ff_shvc_dsp_update(HEVCDSPContext *hevcdsp, int bit_depth, int have_CGS);

extern const int8_t ff_hevc_epel_filters[7][4];
extern const int8_t ff_hevc_qpel_filters[3][16];

#if OHCONFIG_AMT

#define AMT_OPT_C2 0
#define AMT_OPT_C 0

#if AMT_OPT_C2
#define EMT_DECL_V_DCT_4x4(optim,depth)\
    void emt_idct_4x4_0_0_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\


#define EMT_DECL_V_DCT_8x8(y,optim,depth)\
    void emt_idct_8x8_0_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_8x8_1_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\


#define EMT_DECL_V_DCT_16x16(y,optim,depth)\
    void emt_idct_16x16_0_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_16x16_1_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_16x16_2_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_16x16_3_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\


#define EMT_DECL_V_DCT_32x32(y,optim,depth)\
    void emt_idct_32x32_0_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_1_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_2_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_3_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_4_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_5_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_6_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_7_##y##_v_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\

#define EMT_DECL_H_DCT_4x4(optim,depth)\
    void emt_idct_4x4_0_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\


#define EMT_DECL_H_DCT_8x8(optim,depth)\
    void emt_idct_8x8_0_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_8x8_1_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\


#define EMT_DECL_H_DCT_16x16(optim,depth)\
    void emt_idct_16x16_0_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_16x16_1_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_16x16_2_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_16x16_3_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\


#define EMT_DECL_H_DCT_32x32(optim,depth)\
    void emt_idct_32x32_0_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_1_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_2_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_3_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_4_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_5_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_6_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\
    void emt_idct_32x32_7_h_##optim##_##depth(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h);\


#define DCT_EMT_DECL_V_32x32(optim,depth)\
    EMT_DECL_V_DCT_32x32(0,optim,depth)\
    EMT_DECL_V_DCT_32x32(1,optim,depth)\
    EMT_DECL_V_DCT_32x32(2,optim,depth)\
    EMT_DECL_V_DCT_32x32(3,optim,depth)\
    EMT_DECL_V_DCT_32x32(4,optim,depth)\
    EMT_DECL_V_DCT_32x32(5,optim,depth)\
    EMT_DECL_V_DCT_32x32(6,optim,depth)\
    EMT_DECL_V_DCT_32x32(7,optim,depth)\


#define DCT_EMT_DECL_V_16x16(optim,depth)\
    EMT_DECL_V_DCT_16x16(0,optim,depth)\
    EMT_DECL_V_DCT_16x16(1,optim,depth)\
    EMT_DECL_V_DCT_16x16(2,optim,depth)\
    EMT_DECL_V_DCT_16x16(3,optim,depth)\

#define DCT_EMT_DECL_V_8x8(optim,depth)\
    EMT_DECL_V_DCT_8x8(0,optim,depth)\
    EMT_DECL_V_DCT_8x8(1,optim,depth)\

#define DCT_EMT_DECL_V_4x4(optim,depth)\
EMT_DECL_V_DCT_4x4(optim,depth)\

#define DST_EMT_DECL_V_32x32(optim,depth)\
    EMT_DECL_V_DST_32x32(0,optim,depth)\
    EMT_DECL_V_DST_32x32(1,optim,depth)\
    EMT_DECL_V_DST_32x32(2,optim,depth)\
    EMT_DECL_V_DST_32x32(3,optim,depth)\
    EMT_DECL_V_DST_32x32(4,optim,depth)\
    EMT_DECL_V_DST_32x32(5,optim,depth)\
    EMT_DECL_V_DST_32x32(6,optim,depth)\
    EMT_DECL_V_DST_32x32(7,optim,depth)\

#define DECL_DCT(optim,depth)\
    DCT_EMT_DECL_V_32x32(optim,depth)\
    DCT_EMT_DECL_V_16x16(optim,depth)\
    DCT_EMT_DECL_V_8x8(optim,depth)\
    DCT_EMT_DECL_V_4x4(optim,depth)\


   DECL_DCT(template,8)
   DECL_DCT(template,9)
   DECL_DCT(template,10)
   DECL_DCT(template,12)
   DECL_DCT(template,14)

#endif
#endif

void ff_hevc_dsp_init_x86(HEVCDSPContext *c, const int bit_depth);
void ff_shvc_dsp_update_x86(HEVCDSPContext *c, const int bit_depth, const int have_CGS);
void ff_hevcdsp_init_arm(HEVCDSPContext *c, const int bit_depth);
#endif /* AVCODEC_HEVCDSP_H */
