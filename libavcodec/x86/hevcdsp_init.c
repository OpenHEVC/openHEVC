/*
 * Copyright (c) 2013 Seppo Tomperi
 * Copyright (c) 2013 - 2014 Pierre-Edouard Lepere
 *
 * This file is part of ffmpeg.
 *
 * ffmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ffmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ffmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "libavutil/cpu.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include "libavcodec/get_bits.h" /* required for hevcdsp.h GetBitContext */
#include "libavcodec/hevcdsp.h"
#include "libavcodec/x86/hevcdsp.h"
#include "libavcodec/hevc_defs.h"

/***********************************/
/* deblocking */

#define LFC_FUNC(DIR, DEPTH, OPT)                                        \
void ff_hevc_ ## DIR ## _loop_filter_chroma_ ## DEPTH ## _ ## OPT(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

#define LFL_FUNC(DIR, DEPTH, OPT)                                        \
void ff_hevc_ ## DIR ## _loop_filter_luma_ ## DEPTH ## _ ## OPT(uint8_t *_pix, ptrdiff_t stride, int _beta, int *_tc, \
uint8_t *_no_p, uint8_t *_no_q);

#define LFC_FUNCS(type, depth, opt) \
LFC_FUNC(h, depth, opt)  \
LFC_FUNC(v, depth, opt)

#define LFL_FUNCS(type, depth, opt) \
LFL_FUNC(h, depth, opt)  \
LFL_FUNC(v, depth, opt)

LFC_FUNCS(uint8_t,   8, sse2)
LFC_FUNCS(uint8_t,  10, sse2)
LFL_FUNCS(uint8_t,   8, sse2)
LFL_FUNCS(uint8_t,  10, sse2)
LFL_FUNCS(uint8_t,   8, ssse3)
LFL_FUNCS(uint8_t,  10, ssse3)


#if !ARCH_X86_32 && defined(OPTI_ASM)
#if HAVE_SSE2_EXTERNAL
void ff_hevc_idct32_dc_add_8_sse2(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
{
    ff_hevc_idct16_dc_add_8_sse2(dst, coeffs, stride);
    ff_hevc_idct16_dc_add_8_sse2(dst+16, coeffs, stride);
    ff_hevc_idct16_dc_add_8_sse2(dst+16*stride, coeffs, stride);
    ff_hevc_idct16_dc_add_8_sse2(dst+16*stride+16, coeffs, stride);
}

void ff_hevc_idct16_dc_add_10_sse2(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
{
    ff_hevc_idct8_dc_add_10_sse2(dst, coeffs, stride);
    ff_hevc_idct8_dc_add_10_sse2(dst+16, coeffs, stride);
    ff_hevc_idct8_dc_add_10_sse2(dst+8*stride, coeffs, stride);
    ff_hevc_idct8_dc_add_10_sse2(dst+8*stride+16, coeffs, stride);
}

void ff_hevc_idct32_dc_add_10_sse2(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
{
    ff_hevc_idct16_dc_add_10_sse2(dst, coeffs, stride);
    ff_hevc_idct16_dc_add_10_sse2(dst+32, coeffs, stride);
    ff_hevc_idct16_dc_add_10_sse2(dst+16*stride, coeffs, stride);
    ff_hevc_idct16_dc_add_10_sse2(dst+16*stride+32, coeffs, stride);
}
#endif //HAVE_SSE2_EXTERNAL
#if HAVE_AVX_EXTERNAL
void ff_hevc_idct16_dc_add_10_avx(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
{
    ff_hevc_idct8_dc_add_10_avx(dst, coeffs, stride);
    ff_hevc_idct8_dc_add_10_avx(dst+16, coeffs, stride);
    ff_hevc_idct8_dc_add_10_avx(dst+8*stride, coeffs, stride);
    ff_hevc_idct8_dc_add_10_avx(dst+8*stride+16, coeffs, stride);
}

void ff_hevc_idct32_dc_add_10_avx(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
{
    ff_hevc_idct16_dc_add_10_avx(dst, coeffs, stride);
    ff_hevc_idct16_dc_add_10_avx(dst+32, coeffs, stride);
    ff_hevc_idct16_dc_add_10_avx(dst+16*stride, coeffs, stride);
    ff_hevc_idct16_dc_add_10_avx(dst+16*stride+32, coeffs, stride);
}
#endif //HAVE_AVX_EXTERNAL

#if HAVE_AVX2_EXTERNAL

void ff_hevc_idct32_dc_add_10_avx2(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
{
    ff_hevc_idct16_dc_add_10_avx2(dst, coeffs, stride);
    ff_hevc_idct16_dc_add_10_avx2(dst+32, coeffs, stride);
    ff_hevc_idct16_dc_add_10_avx2(dst+16*stride, coeffs, stride);
    ff_hevc_idct16_dc_add_10_avx2(dst+16*stride+32, coeffs, stride);
}
#endif //HAVE_AVX2_EXTERNAL


#define mc_rep_func(name, bitd, step, W, opt) \
void ff_hevc_put_hevc_##name##W##_##bitd##_##opt(int16_t *_dst, ptrdiff_t dststride,                            \
                                                uint8_t *_src, ptrdiff_t _srcstride, int height,                \
                                                intptr_t mx, intptr_t my, int width)                            \
{                                                                                                               \
    int i;                                                                                                      \
    uint8_t *src;                                                                                               \
    int16_t *dst;                                                                                               \
    for (i = 0; i < W; i += step) {                                                                             \
        src  = _src + (i * ((bitd + 7) / 8));                                                                   \
        dst = _dst + i;                                                                                         \
        ff_hevc_put_hevc_##name##step##_##bitd##_##opt(dst, dststride, src, _srcstride, height, mx, my, width); \
    }                                                                                                           \
}
#define mc_rep_uni_func(name, bitd, step, W, opt) \
void ff_hevc_put_hevc_uni_##name##W##_##bitd##_##opt(uint8_t *_dst, ptrdiff_t dststride,                        \
                                                    uint8_t *_src, ptrdiff_t _srcstride, int height,            \
                                                    intptr_t mx, intptr_t my, int width)                        \
{                                                                                                               \
    int i;                                                                                                      \
    uint8_t *src;                                                                                               \
    uint8_t *dst;                                                                                               \
    for (i = 0; i < W; i += step) {                                                                             \
        src = _src + (i * ((bitd + 7) / 8));                                                                    \
        dst = _dst + (i * ((bitd + 7) / 8));                                                                    \
        ff_hevc_put_hevc_uni_##name##step##_##bitd##_##opt(dst, dststride, src, _srcstride,                     \
                                                          height, mx, my, width);                               \
    }                                                                                                           \
}
#define mc_rep_bi_func(name, bitd, step, W, opt) \
void ff_hevc_put_hevc_bi_##name##W##_##bitd##_##opt(uint8_t *_dst, ptrdiff_t dststride, uint8_t *_src,          \
                                                   ptrdiff_t _srcstride, int16_t* _src2, ptrdiff_t _src2stride, \
                                                   int height, intptr_t mx, intptr_t my, int width)             \
{                                                                                                               \
    int i;                                                                                                      \
    uint8_t  *src;                                                                                              \
    uint8_t  *dst;                                                                                              \
    int16_t  *src2;                                                                                             \
    for (i = 0; i < W ; i += step) {                                                                            \
        src  = _src + (i * ((bitd + 7) / 8));                                                                   \
        dst  = _dst + (i * ((bitd + 7) / 8));                                                                   \
        src2 = _src2 + i;                                                                                       \
        ff_hevc_put_hevc_bi_##name##step##_##bitd##_##opt(dst, dststride, src, _srcstride, src2,                \
                                                         _src2stride, height, mx, my, width);                   \
    }                                                                                                           \
}

#define mc_rep_funcs(name, bitd, step, W, opt)        \
    mc_rep_func(name, bitd, step, W, opt);            \
    mc_rep_uni_func(name, bitd, step, W, opt);        \
    mc_rep_bi_func(name, bitd, step, W, opt)


#define mc_rep_mix_10(name, width1, width2, width3, opt1, opt2, width4)                                                      \
void ff_hevc_put_hevc_##name##width1##_10_##opt1(int16_t *dst, ptrdiff_t dststride,                                          \
                                                uint8_t *src, ptrdiff_t _srcstride, int height,                              \
                                                intptr_t mx, intptr_t my, int width)                                         \
{                                                                                                                            \
        ff_hevc_put_hevc_##name##width2##_10_##opt1(dst, dststride, src, _srcstride, height, mx, my, width);                 \
        ff_hevc_put_hevc_##name##width3##_10_##opt2(dst+ width2, dststride, src+ width4, _srcstride, height, mx, my, width); \
}

#define mc_bi_rep_mix_10(name, width1, width2, width3, opt1, opt2, width4)                                                   \
void ff_hevc_put_hevc_bi_##name##width1##_10_##opt1(uint8_t *dst, ptrdiff_t dststride, uint8_t *src,                         \
                                                   ptrdiff_t _srcstride, int16_t*src2, ptrdiff_t _src2stride,                \
                                                   int height, intptr_t mx, intptr_t my, int width)                          \
{                                                                                                                            \
        ff_hevc_put_hevc_bi_##name##width2##_10_##opt1(dst, dststride, src, _srcstride, src2,                                 \
                                                         _src2stride, height, mx, my, width);                                \
        ff_hevc_put_hevc_bi_##name##width3##_10_##opt2(dst+width4, dststride, src+width4, _srcstride, src2+width2,            \
                                                         _src2stride, height, mx, my, width);                                \
}

#define mc_uni_rep_mix_10(name, width1, width2, width3, opt1, opt2, width4)                                                           \
void ff_hevc_put_hevc_uni_##name##width1##_10_##opt1(uint8_t *dst, ptrdiff_t dststride,                                       \
                                                    uint8_t *src, ptrdiff_t _srcstride, int height,                          \
                                                    intptr_t mx, intptr_t my, int width)                                     \
{                                                                                                                            \
        ff_hevc_put_hevc_uni_##name##width2##_10_##opt1(dst, dststride, src, _srcstride,                                      \
                                                          height, mx, my, width);                                            \
        ff_hevc_put_hevc_uni_##name##width3##_10_##opt2(dst+width4, dststride, src+width4, _srcstride,                        \
                                                          height, mx, my, width);                                            \
}

#define mc_rep_mixs_10(name, width1, width2, width3, opt1, opt2, width4)    \
mc_rep_mix_10(name, width1, width2, width3, opt1, opt2, width4);            \
mc_bi_rep_mix_10(name, width1, width2, width3, opt1, opt2, width4); \
mc_uni_rep_mix_10(name, width1, width2, width3, opt1, opt2, width4)

#define mc_rep_mix_8(name, width1, width2, width3, opt1, opt2)                                                               \
void ff_hevc_put_hevc_##name##width1##_8_##opt1(int16_t *dst, ptrdiff_t dststride,                                           \
                                                uint8_t *src, ptrdiff_t _srcstride, int height,                              \
                                                intptr_t mx, intptr_t my, int width)                                         \
{                                                                                                                            \
        ff_hevc_put_hevc_##name##width2##_8_##opt1(dst, dststride, src, _srcstride, height, mx, my, width);                  \
        ff_hevc_put_hevc_##name##width3##_8_##opt2(dst+ width2, dststride, src+ width2, _srcstride, height, mx, my, width);  \
}

#define mc_bi_rep_mix_8(name, width1, width2, width3, opt1, opt2)                                                            \
void ff_hevc_put_hevc_bi_##name##width1##_8_##opt1(uint8_t *dst, ptrdiff_t dststride, uint8_t *src,                          \
                                                   ptrdiff_t _srcstride, int16_t*src2, ptrdiff_t _src2stride,                \
                                                   int height, intptr_t mx, intptr_t my, int width)                          \
{                                                                                                                            \
        ff_hevc_put_hevc_bi_##name##width2##_8_##opt1(dst, dststride, src, _srcstride, src2,                                 \
                                                         _src2stride, height, mx, my, width);                                \
        ff_hevc_put_hevc_bi_##name##width3##_8_##opt2(dst+width2, dststride, src+width2, _srcstride, src2+width2,            \
                                                         _src2stride, height, mx, my, width);                                \
}

#define mc_uni_rep_mix_8(name, width1, width2, width3, opt1, opt2)                                                           \
void ff_hevc_put_hevc_uni_##name##width1##_8_##opt1(uint8_t *dst, ptrdiff_t dststride,                                       \
                                                    uint8_t *src, ptrdiff_t _srcstride, int height,                          \
                                                    intptr_t mx, intptr_t my, int width)                                     \
{                                                                                                                            \
        ff_hevc_put_hevc_uni_##name##width2##_8_##opt1(dst, dststride, src, _srcstride,                                      \
                                                          height, mx, my, width);                                            \
        ff_hevc_put_hevc_uni_##name##width3##_8_##opt2(dst+width2, dststride, src+width2, _srcstride,                        \
                                                          height, mx, my, width);                                            \
}

#define mc_rep_mixs_8(name, width1, width2, width3, opt1, opt2)    \
mc_rep_mix_8(name, width1, width2, width3, opt1, opt2);            \
mc_bi_rep_mix_8(name, width1, width2, width3, opt1, opt2); \
mc_uni_rep_mix_8(name, width1, width2, width3, opt1, opt2)

#if HAVE_AVX2_EXTERNAL

mc_rep_mixs_8(pel_pixels, 48, 32, 16, avx2, sse4);
mc_rep_mixs_8(epel_hv,    48, 32, 16, avx2, sse4);
mc_rep_mixs_8(epel_h ,    48, 32, 16, avx2, sse4);
mc_rep_mixs_8(epel_v ,    48, 32, 16, avx2, sse4);

mc_rep_mix_10(pel_pixels, 24, 16, 8, avx2, sse4, 32);
mc_bi_rep_mix_10(pel_pixels,24, 16, 8, avx2, sse4, 32);
mc_rep_mixs_10(epel_hv,   24, 16, 8, avx2, sse4, 32);
mc_rep_mixs_10(epel_h ,   24, 16, 8, avx2, sse4, 32);
mc_rep_mixs_10(epel_v ,   24, 16, 8, avx2, sse4, 32);


mc_rep_mixs_10(qpel_h ,   24, 16, 8, avx2, sse4, 32);
mc_rep_mixs_10(qpel_v ,   24, 16, 8, avx2, sse4, 32);
mc_rep_mixs_10(qpel_hv,   24, 16, 8, avx2, sse4, 32);


mc_rep_uni_func(pel_pixels, 8, 64, 128, avx2);//used for 10bit
mc_rep_uni_func(pel_pixels, 8, 32, 96, avx2); //used for 10bit

mc_rep_funcs(pel_pixels, 8, 32, 64, avx2);

mc_rep_func(pel_pixels, 10, 16, 32, avx2);
mc_rep_func(pel_pixels, 10, 16, 48, avx2);
mc_rep_func(pel_pixels, 10, 32, 64, avx2);

mc_rep_bi_func(pel_pixels, 10, 16, 32, avx2);
mc_rep_bi_func(pel_pixels, 10, 16, 48, avx2);
mc_rep_bi_func(pel_pixels, 10, 32, 64, avx2);

mc_rep_funcs(epel_h, 8, 32, 64, avx2);

mc_rep_funcs(epel_v, 8, 32, 64, avx2);

mc_rep_funcs(epel_h, 10, 16, 32, avx2);
mc_rep_funcs(epel_h, 10, 16, 48, avx2);
mc_rep_funcs(epel_h, 10, 32, 64, avx2);

mc_rep_funcs(epel_v, 10, 16, 32, avx2);
mc_rep_funcs(epel_v, 10, 16, 48, avx2);
mc_rep_funcs(epel_v, 10, 32, 64, avx2);


mc_rep_funcs(epel_hv,  8, 32, 64, avx2);

mc_rep_funcs(epel_hv, 10, 16, 32, avx2);
mc_rep_funcs(epel_hv, 10, 16, 48, avx2);
mc_rep_funcs(epel_hv, 10, 32, 64, avx2);

mc_rep_funcs(qpel_h, 8, 32, 64, avx2);
mc_rep_mixs_8(qpel_h ,  48, 32, 16, avx2, sse4);

mc_rep_funcs(qpel_v, 8, 32, 64, avx2);
mc_rep_mixs_8(qpel_v,  48, 32, 16, avx2, sse4);

mc_rep_funcs(qpel_h, 10, 16, 32, avx2);
mc_rep_funcs(qpel_h, 10, 16, 48, avx2);
mc_rep_funcs(qpel_h, 10, 32, 64, avx2);

mc_rep_funcs(qpel_v, 10, 16, 32, avx2);
mc_rep_funcs(qpel_v, 10, 16, 48, avx2);
mc_rep_funcs(qpel_v, 10, 32, 64, avx2);

mc_rep_funcs(qpel_hv, 10, 16, 32, avx2);
mc_rep_funcs(qpel_hv, 10, 16, 48, avx2);
mc_rep_funcs(qpel_hv, 10, 32, 64, avx2);
#endif //AVX2

mc_rep_funcs(pel_pixels, 8, 16, 64, sse4);
mc_rep_funcs(pel_pixels, 8, 16, 48, sse4);
mc_rep_funcs(pel_pixels, 8, 16, 32, sse4);
mc_rep_funcs(pel_pixels, 8,  8, 24, sse4);

mc_rep_funcs(pel_pixels,10,  8, 64, sse4);
mc_rep_funcs(pel_pixels,10,  8, 48, sse4);
mc_rep_funcs(pel_pixels,10,  8, 32, sse4);
mc_rep_funcs(pel_pixels,10,  8, 24, sse4);
mc_rep_funcs(pel_pixels,10,  8, 16, sse4);
mc_rep_funcs(pel_pixels,10,  4, 12, sse4);

mc_rep_funcs(epel_h, 8, 16, 64, sse4);
mc_rep_funcs(epel_h, 8, 16, 48, sse4);
mc_rep_funcs(epel_h, 8, 16, 32, sse4);
mc_rep_funcs(epel_h, 8,  8, 24, sse4);
mc_rep_funcs(epel_h,10,  8, 64, sse4);
mc_rep_funcs(epel_h,10,  8, 48, sse4);
mc_rep_funcs(epel_h,10,  8, 32, sse4);
mc_rep_funcs(epel_h,10,  8, 24, sse4);
mc_rep_funcs(epel_h,10,  8, 16, sse4);
mc_rep_funcs(epel_h,10,  4, 12, sse4);
mc_rep_funcs(epel_v, 8, 16, 64, sse4);
mc_rep_funcs(epel_v, 8, 16, 48, sse4);
mc_rep_funcs(epel_v, 8, 16, 32, sse4);
mc_rep_funcs(epel_v, 8,  8, 24, sse4);
mc_rep_funcs(epel_v,10,  8, 64, sse4);
mc_rep_funcs(epel_v,10,  8, 48, sse4);
mc_rep_funcs(epel_v,10,  8, 32, sse4);
mc_rep_funcs(epel_v,10,  8, 24, sse4);
mc_rep_funcs(epel_v,10,  8, 16, sse4);
mc_rep_funcs(epel_v,10,  4, 12, sse4);
mc_rep_funcs(epel_hv, 8,  8, 64, sse4);
mc_rep_funcs(epel_hv, 8,  8, 48, sse4);
mc_rep_funcs(epel_hv, 8,  8, 32, sse4);
mc_rep_funcs(epel_hv, 8,  8, 24, sse4);
mc_rep_funcs(epel_hv, 8,  4, 12, sse4);
mc_rep_funcs(epel_hv,10,  8, 64, sse4);
mc_rep_funcs(epel_hv,10,  8, 48, sse4);
mc_rep_funcs(epel_hv,10,  8, 32, sse4);
mc_rep_funcs(epel_hv,10,  8, 24, sse4);
mc_rep_funcs(epel_hv,10,  8, 16, sse4);
mc_rep_funcs(epel_hv,10,  4, 12, sse4);

mc_rep_funcs(qpel_h, 8, 16, 64, sse4);
mc_rep_funcs(qpel_h, 8, 16, 48, sse4);
mc_rep_funcs(qpel_h, 8, 16, 32, sse4);
mc_rep_funcs(qpel_h, 8,  8, 24, sse4);
mc_rep_funcs(qpel_h,10,  8, 64, sse4);
mc_rep_funcs(qpel_h,10,  8, 48, sse4);
mc_rep_funcs(qpel_h,10,  8, 32, sse4);
mc_rep_funcs(qpel_h,10,  8, 24, sse4);
mc_rep_funcs(qpel_h,10,  8, 16, sse4);
mc_rep_funcs(qpel_h,10,  4, 12, sse4);
mc_rep_funcs(qpel_v, 8, 16, 64, sse4);
mc_rep_funcs(qpel_v, 8, 16, 48, sse4);
mc_rep_funcs(qpel_v, 8, 16, 32, sse4);
mc_rep_funcs(qpel_v, 8,  8, 24, sse4);
mc_rep_funcs(qpel_v,10,  8, 64, sse4);
mc_rep_funcs(qpel_v,10,  8, 48, sse4);
mc_rep_funcs(qpel_v,10,  8, 32, sse4);
mc_rep_funcs(qpel_v,10,  8, 24, sse4);
mc_rep_funcs(qpel_v,10,  8, 16, sse4);
mc_rep_funcs(qpel_v,10,  4, 12, sse4);
mc_rep_funcs(qpel_hv, 8,  8, 64, sse4);
mc_rep_funcs(qpel_hv, 8,  8, 48, sse4);
mc_rep_funcs(qpel_hv, 8,  8, 32, sse4);
mc_rep_funcs(qpel_hv, 8,  8, 24, sse4);
mc_rep_funcs(qpel_hv, 8,  8, 16, sse4);
mc_rep_funcs(qpel_hv, 8,  4, 12, sse4);
mc_rep_funcs(qpel_hv,10,  8, 64, sse4);
mc_rep_funcs(qpel_hv,10,  8, 48, sse4);
mc_rep_funcs(qpel_hv,10,  8, 32, sse4);
mc_rep_funcs(qpel_hv,10,  8, 24, sse4);
mc_rep_funcs(qpel_hv,10,  8, 16, sse4);
mc_rep_funcs(qpel_hv,10,  4, 12, sse4);

#define mc_rep_uni_w(bitd, step, W, opt) \
void ff_hevc_put_hevc_uni_w##W##_##bitd##_##opt(uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,\
                                               int height, int denom,  int _wx, int _ox)                                \
{                                                                                                                       \
    int i;                                                                                                              \
    int16_t *src;                                                                                                       \
    uint8_t *dst;                                                                                                       \
    for (i = 0; i < W; i += step) {                                                                                     \
        src= _src + i;                                                                                                  \
        dst= _dst + (i * ((bitd + 7) / 8));                                                                             \
        ff_hevc_put_hevc_uni_w##step##_##bitd##_##opt(dst, dststride, src, _srcstride,                                  \
                                                     height, denom, _wx, _ox);                                          \
    }                                                                                                                   \
}

mc_rep_uni_w(8, 6, 12, sse4);
mc_rep_uni_w(8, 8, 16, sse4);
mc_rep_uni_w(8, 8, 24, sse4);
mc_rep_uni_w(8, 8, 32, sse4);
mc_rep_uni_w(8, 8, 48, sse4);
mc_rep_uni_w(8, 8, 64, sse4);

mc_rep_uni_w(10, 6, 12, sse4);
mc_rep_uni_w(10, 8, 16, sse4);
mc_rep_uni_w(10, 8, 24, sse4);
mc_rep_uni_w(10, 8, 32, sse4);
mc_rep_uni_w(10, 8, 48, sse4);
mc_rep_uni_w(10, 8, 64, sse4);

#define mc_rep_bi_w(bitd, step, W, opt) \
void ff_hevc_put_hevc_bi_w##W##_##bitd##_##opt(uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride, \
                                              int16_t *_src2, ptrdiff_t _src2stride, int height,                        \
                                              int denom,  int _wx0,  int _wx1, int _ox0, int _ox1)                      \
{                                                                                                                       \
    int i;                                                                                                              \
    int16_t *src;                                                                                                       \
    int16_t *src2;                                                                                                      \
    uint8_t *dst;                                                                                                       \
    for (i = 0; i < W; i += step) {                                                                                     \
        src  = _src  + i;                                                                                               \
        src2 = _src2 + i;                                                                                               \
        dst  = _dst  + (i * ((bitd + 7) / 8));                                                                          \
        ff_hevc_put_hevc_bi_w##step##_##bitd##_##opt(dst, dststride, src, _srcstride, src2, _src2stride,                \
                                                    height, denom, _wx0, _wx1, _ox0, _ox1);                             \
    }                                                                                                                   \
}

mc_rep_bi_w(8, 6, 12, sse4);
mc_rep_bi_w(8, 8, 16, sse4);
mc_rep_bi_w(8, 8, 24, sse4);
mc_rep_bi_w(8, 8, 32, sse4);
mc_rep_bi_w(8, 8, 48, sse4);
mc_rep_bi_w(8, 8, 64, sse4);

mc_rep_bi_w(10, 6, 12, sse4);
mc_rep_bi_w(10, 8, 16, sse4);
mc_rep_bi_w(10, 8, 24, sse4);
mc_rep_bi_w(10, 8, 32, sse4);
mc_rep_bi_w(10, 8, 48, sse4);
mc_rep_bi_w(10, 8, 64, sse4);

#define mc_uni_w_func(name, bitd, W, opt) \
void ff_hevc_put_hevc_uni_w_##name##W##_##bitd##_##opt(uint8_t *_dst, ptrdiff_t _dststride,         \
                                                      uint8_t *_src, ptrdiff_t _srcstride,          \
                                                      int height, int denom,                        \
                                                      int _wx, int _ox,                             \
                                                      intptr_t mx, intptr_t my, int width)          \
{                                                                                                   \
    LOCAL_ALIGNED_16(int16_t, temp, [71 * 64]);                                                     \
    ff_hevc_put_hevc_##name##W##_##bitd##_##opt(temp, 64, _src, _srcstride, height, mx, my, width); \
    ff_hevc_put_hevc_uni_w##W##_##bitd##_##opt(_dst, _dststride, temp, 64, height, denom, _wx, _ox);\
}

#define mc_uni_w_funcs(name, bitd, opt)       \
        mc_uni_w_func(name, bitd, 4, opt);    \
        mc_uni_w_func(name, bitd, 8, opt);    \
        mc_uni_w_func(name, bitd, 12, opt);   \
        mc_uni_w_func(name, bitd, 16, opt);   \
        mc_uni_w_func(name, bitd, 24, opt);   \
        mc_uni_w_func(name, bitd, 32, opt);   \
        mc_uni_w_func(name, bitd, 48, opt);   \
        mc_uni_w_func(name, bitd, 64, opt)

mc_uni_w_funcs(pel_pixels, 8, sse4);
mc_uni_w_func(pel_pixels, 8, 6, sse4);
mc_uni_w_funcs(epel_h, 8, sse4);
mc_uni_w_func(epel_h, 8, 6, sse4);
mc_uni_w_funcs(epel_v, 8, sse4);
mc_uni_w_func(epel_v, 8, 6, sse4);
mc_uni_w_funcs(epel_hv, 8, sse4);
mc_uni_w_func(epel_hv, 8, 6, sse4);
mc_uni_w_funcs(qpel_h, 8, sse4);
mc_uni_w_funcs(qpel_v, 8, sse4);
mc_uni_w_funcs(qpel_hv, 8, sse4);

mc_uni_w_funcs(pel_pixels, 10, sse4);
mc_uni_w_func(pel_pixels, 10, 6, sse4);
mc_uni_w_funcs(epel_h, 10, sse4);
mc_uni_w_func(epel_h, 10, 6, sse4);
mc_uni_w_funcs(epel_v, 10, sse4);
mc_uni_w_func(epel_v, 10, 6, sse4);
mc_uni_w_funcs(epel_hv, 10, sse4);
mc_uni_w_func(epel_hv, 10, 6, sse4);
mc_uni_w_funcs(qpel_h, 10, sse4);
mc_uni_w_funcs(qpel_v, 10, sse4);
mc_uni_w_funcs(qpel_hv, 10, sse4);


#define mc_bi_w_func(name, bitd, W, opt) \
void ff_hevc_put_hevc_bi_w_##name##W##_##bitd##_##opt(uint8_t *_dst, ptrdiff_t _dststride,           \
                                                     uint8_t *_src, ptrdiff_t _srcstride,            \
                                                     int16_t *_src2, ptrdiff_t _src2stride,          \
                                                     int height, int denom,                          \
                                                     int _wx0, int _wx1, int _ox0, int _ox1,         \
                                                     intptr_t mx, intptr_t my, int width)            \
{                                                                                                    \
    LOCAL_ALIGNED_16(int16_t, temp, [71 * 64]);                                                      \
    ff_hevc_put_hevc_##name##W##_##bitd##_##opt(temp, 64, _src, _srcstride, height, mx, my, width);  \
    ff_hevc_put_hevc_bi_w##W##_##bitd##_##opt(_dst, _dststride, temp, 64, _src2, _src2stride,        \
                                             height, denom, _wx0, _wx1, _ox0, _ox1);                 \
}

#define mc_bi_w_funcs(name, bitd, opt)       \
        mc_bi_w_func(name, bitd, 4, opt);    \
        mc_bi_w_func(name, bitd, 8, opt);    \
        mc_bi_w_func(name, bitd, 12, opt);   \
        mc_bi_w_func(name, bitd, 16, opt);   \
        mc_bi_w_func(name, bitd, 24, opt);   \
        mc_bi_w_func(name, bitd, 32, opt);   \
        mc_bi_w_func(name, bitd, 48, opt);   \
        mc_bi_w_func(name, bitd, 64, opt)

mc_bi_w_funcs(pel_pixels, 8, sse4);
mc_bi_w_func(pel_pixels, 8, 6, sse4);
mc_bi_w_funcs(epel_h, 8, sse4);
mc_bi_w_func(epel_h, 8, 6, sse4);
mc_bi_w_funcs(epel_v, 8, sse4);
mc_bi_w_func(epel_v, 8, 6, sse4);
mc_bi_w_funcs(epel_hv, 8, sse4);
mc_bi_w_func(epel_hv, 8, 6, sse4);
mc_bi_w_funcs(qpel_h, 8, sse4);
mc_bi_w_funcs(qpel_v, 8, sse4);
mc_bi_w_funcs(qpel_hv, 8, sse4);

mc_bi_w_funcs(pel_pixels, 10, sse4);
mc_bi_w_func(pel_pixels, 10, 6, sse4);
mc_bi_w_funcs(epel_h, 10, sse4);
mc_bi_w_func(epel_h, 10, 6, sse4);
mc_bi_w_funcs(epel_v, 10, sse4);
mc_bi_w_func(epel_v, 10, 6, sse4);
mc_bi_w_funcs(epel_hv, 10, sse4);
mc_bi_w_func(epel_hv, 10, 6, sse4);
mc_bi_w_funcs(qpel_h, 10, sse4);
mc_bi_w_funcs(qpel_v, 10, sse4);
mc_bi_w_funcs(qpel_hv, 10, sse4);


#endif


#define EPEL_LINKS(pointer, my, mx, fname, bitd, opt)           \
        PEL_LINK(pointer, 1, my , mx , fname##4 ,  bitd, opt ); \
        PEL_LINK(pointer, 2, my , mx , fname##6 ,  bitd, opt ); \
        PEL_LINK(pointer, 3, my , mx , fname##8 ,  bitd, opt ); \
        PEL_LINK(pointer, 4, my , mx , fname##12,  bitd, opt ); \
        PEL_LINK(pointer, 5, my , mx , fname##16,  bitd, opt ); \
        PEL_LINK(pointer, 6, my , mx , fname##24,  bitd, opt ); \
        PEL_LINK(pointer, 7, my , mx , fname##32,  bitd, opt ); \
        PEL_LINK(pointer, 8, my , mx , fname##48,  bitd, opt ); \
        PEL_LINK(pointer, 9, my , mx , fname##64,  bitd, opt )
#define QPEL_LINKS(pointer, my, mx, fname, bitd, opt)           \
        PEL_LINK(pointer, 1, my , mx , fname##4 ,  bitd, opt ); \
        PEL_LINK(pointer, 3, my , mx , fname##8 ,  bitd, opt ); \
        PEL_LINK(pointer, 4, my , mx , fname##12,  bitd, opt ); \
        PEL_LINK(pointer, 5, my , mx , fname##16,  bitd, opt ); \
        PEL_LINK(pointer, 6, my , mx , fname##24,  bitd, opt ); \
        PEL_LINK(pointer, 7, my , mx , fname##32,  bitd, opt ); \
        PEL_LINK(pointer, 8, my , mx , fname##48,  bitd, opt ); \
        PEL_LINK(pointer, 9, my , mx , fname##64,  bitd, opt )


void ff_hevcdsp_init_x86(HEVCDSPContext *c, const int bit_depth) {
    int mm_flags = av_get_cpu_flags();

    if (bit_depth == 8) {
        if (EXTERNAL_MMX(mm_flags)) {
            if (EXTERNAL_MMXEXT(mm_flags)) {
#if ARCH_X86_32 && HAVE_MMXEXT_EXTERNAL
                /* MMEXT optimizations */
#endif /* ARCH_X86_32 && HAVE_MMXEXT_EXTERNAL */
#ifdef OPTI_ASM
                // c->transform_dc_add[0]    =  ff_hevc_idct4_dc_add_8_mmxext;
                // c->transform_dc_add[1]    =  ff_hevc_idct8_dc_add_8_mmxext;
#endif

#if HAVE_SSE2
                if (EXTERNAL_SSE2(mm_flags)) {
                    c->hevc_v_loop_filter_chroma = ff_hevc_v_loop_filter_chroma_8_sse2;
                    c->hevc_h_loop_filter_chroma = ff_hevc_h_loop_filter_chroma_8_sse2;
                    if (ARCH_X86_64) {
                        c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_8_sse2;
                        c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_8_sse2;
                    }

                    // only 4X4 needs update for Rext                   c->transform_skip    = ff_hevc_transform_skip_8_sse;
                    c->idct_4x4_luma = ff_hevc_transform_4x4_luma_8_sse4;
                    c->idct[0] = ff_hevc_transform_4x4_8_sse4;
                    c->idct[1] = ff_hevc_transform_8x8_8_sse4;
                    c->idct[2] = ff_hevc_transform_16x16_8_sse4;
                    c->idct[3] = ff_hevc_transform_32x32_8_sse4;
                    c->transform_add[0] = ff_hevc_transform_4x4_add_8_sse4;
                    c->transform_add[1] = ff_hevc_transform_8x8_add_8_sse4;
                    c->transform_add[2] = ff_hevc_transform_16x16_add_8_sse4;
                    c->transform_add[3] = ff_hevc_transform_32x32_add_8_sse4;

#ifdef OPTI_ASM
                    // c->transform_dc_add[2]    =  ff_hevc_idct16_dc_add_8_sse2;
                    // c->transform_dc_add[3]    =  ff_hevc_idct32_dc_add_8_sse2;
#endif
#ifndef OPTI_ASM
                    // c->transform_dc_add[0] = ff_hevc_transform_4x4_dc_add_8_sse4;
                    // c->transform_dc_add[1] = ff_hevc_transform_8x8_dc_add_8_sse4;
                    // c->transform_dc_add[2] = ff_hevc_transform_16x16_dc_add_8_sse4;
                    // c->transform_dc_add[3] = ff_hevc_transform_32x32_dc_add_8_sse4;
#endif
                }
#endif //HAVE_SSE2
#if HAVE_SSSE3
                if (EXTERNAL_SSSE3(mm_flags)) {
#if ARCH_X86_64
                    c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_8_ssse3;
                    c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_8_ssse3;
#endif
                    EPEL_LINKS(c->put_hevc_epel, 0, 0, pel_pixels, 8, sse4);
                    EPEL_LINKS(c->put_hevc_epel, 0, 1, epel_h,     8, sse4);
                    EPEL_LINKS(c->put_hevc_epel, 1, 0, epel_v,     8, sse4);
                    EPEL_LINKS(c->put_hevc_epel, 1, 1, epel_hv,    8, sse4);

                    QPEL_LINKS(c->put_hevc_qpel, 0, 0, pel_pixels, 8, sse4);
                    QPEL_LINKS(c->put_hevc_qpel, 0, 1, qpel_h,     8, sse4);
                    QPEL_LINKS(c->put_hevc_qpel, 1, 0, qpel_v,     8, sse4);
                    QPEL_LINKS(c->put_hevc_qpel, 1, 1, qpel_hv,    8, sse4);

                    c->sao_band_filter    = ff_hevc_sao_band_filter_0_8_sse;
                    c->sao_edge_filter[0] = ff_hevc_sao_edge_filter_0_8_sse;
                    c->sao_edge_filter[1] = ff_hevc_sao_edge_filter_1_8_sse;
                }
#endif //HAVE_SSSE3
#if HAVE_SSE42
                if (EXTERNAL_SSE4(mm_flags)) {

#ifdef SVC_EXTENSION
                    c->upsample_filter_block_luma_h[1] = ff_upsample_filter_block_luma_h_x2_sse;
                    c->upsample_filter_block_cr_h[1] = ff_upsample_filter_block_cr_h_x2_sse;
                    c->upsample_filter_block_luma_v[1] = ff_upsample_filter_block_luma_v_x2_sse;
                    c->upsample_filter_block_cr_v[1] = ff_upsample_filter_block_cr_v_x2_sse;

                    c->upsample_filter_block_luma_h[2] = ff_upsample_filter_block_luma_h_x1_5_sse;
                    c->upsample_filter_block_cr_h[2] = ff_upsample_filter_block_cr_h_x1_5_sse;
                    c->upsample_filter_block_luma_v[2] = ff_upsample_filter_block_luma_v_x1_5_sse;
                    c->upsample_filter_block_cr_v[2] = ff_upsample_filter_block_cr_v_x1_5_sse;


                    // c->upsample_filter_block_luma_h[0] = ff_upsample_filter_block_luma_h_all_sse;
                    // c->upsample_filter_block_cr_h[0] = ff_upsample_filter_block_cr_h_all_sse;
                    // c->upsample_filter_block_luma_v[0] = ff_upsample_filter_block_luma_v_all_sse;
                    // c->upsample_filter_block_cr_v[0] = ff_upsample_filter_block_cr_v_all_sse;

#endif
                }
#endif //HAVE_SSE42
                if (EXTERNAL_AVX2(mm_flags)) {
#ifdef OPTI_ASM

                    c->put_hevc_epel[7][0][0] = ff_hevc_put_hevc_pel_pixels32_8_avx2;
                    c->put_hevc_epel[8][0][0] = ff_hevc_put_hevc_pel_pixels48_8_avx2;
                    c->put_hevc_epel[9][0][0] = ff_hevc_put_hevc_pel_pixels64_8_avx2;

                    c->put_hevc_qpel[7][0][0] = ff_hevc_put_hevc_pel_pixels32_8_avx2;
                    c->put_hevc_qpel[8][0][0] = ff_hevc_put_hevc_pel_pixels48_8_avx2;
                    c->put_hevc_qpel[9][0][0] = ff_hevc_put_hevc_pel_pixels64_8_avx2;

                    c->put_hevc_epel_uni[7][0][0] = ff_hevc_put_hevc_uni_pel_pixels32_8_avx2;
                    c->put_hevc_epel_uni[8][0][0] = ff_hevc_put_hevc_uni_pel_pixels48_8_avx2;
                    c->put_hevc_epel_uni[9][0][0] = ff_hevc_put_hevc_uni_pel_pixels64_8_avx2;

                    c->put_hevc_qpel_uni[7][0][0] = ff_hevc_put_hevc_uni_pel_pixels32_8_avx2;
                    c->put_hevc_qpel_uni[8][0][0] = ff_hevc_put_hevc_uni_pel_pixels48_8_avx2;
                    c->put_hevc_qpel_uni[9][0][0] = ff_hevc_put_hevc_uni_pel_pixels64_8_avx2;

                    c->put_hevc_qpel_bi[7][0][0] = ff_hevc_put_hevc_bi_pel_pixels32_8_avx2;
                    c->put_hevc_qpel_bi[8][0][0] = ff_hevc_put_hevc_bi_pel_pixels48_8_avx2;
                    c->put_hevc_qpel_bi[9][0][0] = ff_hevc_put_hevc_bi_pel_pixels64_8_avx2;

                    c->put_hevc_epel_bi[7][0][0] = ff_hevc_put_hevc_bi_pel_pixels32_8_avx2;
                    c->put_hevc_epel_bi[8][0][0] = ff_hevc_put_hevc_bi_pel_pixels48_8_avx2;
                    c->put_hevc_epel_bi[9][0][0] = ff_hevc_put_hevc_bi_pel_pixels64_8_avx2;

                    c->put_hevc_epel[7][0][1] = ff_hevc_put_hevc_epel_h32_8_avx2;
                    c->put_hevc_epel[8][0][1] = ff_hevc_put_hevc_epel_h48_8_avx2;
                    c->put_hevc_epel[9][0][1] = ff_hevc_put_hevc_epel_h64_8_avx2;

                    c->put_hevc_epel_uni[7][0][1] = ff_hevc_put_hevc_uni_epel_h32_8_avx2;
                    c->put_hevc_epel_uni[8][0][1] = ff_hevc_put_hevc_uni_epel_h48_8_avx2;
                    c->put_hevc_epel_uni[9][0][1] = ff_hevc_put_hevc_uni_epel_h64_8_avx2;

                    c->put_hevc_epel_bi[7][0][1] = ff_hevc_put_hevc_bi_epel_h32_8_avx2;
                    c->put_hevc_epel_bi[8][0][1] = ff_hevc_put_hevc_bi_epel_h48_8_avx2;
                    c->put_hevc_epel_bi[9][0][1] = ff_hevc_put_hevc_bi_epel_h64_8_avx2;

                    c->put_hevc_epel[7][1][0] = ff_hevc_put_hevc_epel_v32_8_avx2;
                    c->put_hevc_epel[8][1][0] = ff_hevc_put_hevc_epel_v48_8_avx2;
                    c->put_hevc_epel[9][1][0] = ff_hevc_put_hevc_epel_v64_8_avx2;

                    c->put_hevc_epel_uni[7][1][0] = ff_hevc_put_hevc_uni_epel_v32_8_avx2;
                    c->put_hevc_epel_uni[8][1][0] = ff_hevc_put_hevc_uni_epel_v48_8_avx2;
                    c->put_hevc_epel_uni[9][1][0] = ff_hevc_put_hevc_uni_epel_v64_8_avx2;

                    c->put_hevc_epel_bi[7][1][0] = ff_hevc_put_hevc_bi_epel_v32_8_avx2;
                    c->put_hevc_epel_bi[8][1][0] = ff_hevc_put_hevc_bi_epel_v48_8_avx2;
                    c->put_hevc_epel_bi[9][1][0] = ff_hevc_put_hevc_bi_epel_v64_8_avx2;

                    c->put_hevc_epel[7][1][1] = ff_hevc_put_hevc_epel_hv32_8_avx2;
                    c->put_hevc_epel[8][1][1] = ff_hevc_put_hevc_epel_hv48_8_avx2;
                    c->put_hevc_epel[9][1][1] = ff_hevc_put_hevc_epel_hv64_8_avx2;

                    c->put_hevc_epel_uni[7][1][1] = ff_hevc_put_hevc_uni_epel_hv32_8_avx2;
                    c->put_hevc_epel_uni[8][1][1] = ff_hevc_put_hevc_uni_epel_hv48_8_avx2;
                    c->put_hevc_epel_uni[9][1][1] = ff_hevc_put_hevc_uni_epel_hv64_8_avx2;

                    c->put_hevc_epel_bi[7][1][1] = ff_hevc_put_hevc_bi_epel_hv32_8_avx2;
                    c->put_hevc_epel_bi[8][1][1] = ff_hevc_put_hevc_bi_epel_hv48_8_avx2;
                    c->put_hevc_epel_bi[9][1][1] = ff_hevc_put_hevc_bi_epel_hv64_8_avx2;

                    c->put_hevc_qpel[7][0][1] = ff_hevc_put_hevc_qpel_h32_8_avx2;
                    c->put_hevc_qpel[8][0][1] = ff_hevc_put_hevc_qpel_h48_8_avx2;
                    c->put_hevc_qpel[9][0][1] = ff_hevc_put_hevc_qpel_h64_8_avx2;

                    c->put_hevc_qpel[7][1][0] = ff_hevc_put_hevc_qpel_v32_8_avx2;
                    c->put_hevc_qpel[8][1][0] = ff_hevc_put_hevc_qpel_v48_8_avx2;
                    c->put_hevc_qpel[9][1][0] = ff_hevc_put_hevc_qpel_v64_8_avx2;

                    c->put_hevc_qpel_uni[7][0][1] = ff_hevc_put_hevc_uni_qpel_h32_8_avx2;
                    c->put_hevc_qpel_uni[8][0][1] = ff_hevc_put_hevc_uni_qpel_h48_8_avx2;
                    c->put_hevc_qpel_uni[9][0][1] = ff_hevc_put_hevc_uni_qpel_h64_8_avx2;

                    c->put_hevc_qpel_uni[7][1][0] = ff_hevc_put_hevc_uni_qpel_v32_8_avx2;
                    c->put_hevc_qpel_uni[8][1][0] = ff_hevc_put_hevc_uni_qpel_v48_8_avx2;
                    c->put_hevc_qpel_uni[9][1][0] = ff_hevc_put_hevc_uni_qpel_v64_8_avx2;

                    c->put_hevc_qpel_bi[7][0][1] = ff_hevc_put_hevc_bi_qpel_h32_8_avx2;
                    c->put_hevc_qpel_bi[8][0][1] = ff_hevc_put_hevc_bi_qpel_h48_8_avx2;
                    c->put_hevc_qpel_bi[9][0][1] = ff_hevc_put_hevc_bi_qpel_h64_8_avx2;

                    c->put_hevc_qpel_bi[7][1][0] = ff_hevc_put_hevc_bi_qpel_v32_8_avx2;
                    c->put_hevc_qpel_bi[8][1][0] = ff_hevc_put_hevc_bi_qpel_v48_8_avx2;
                    c->put_hevc_qpel_bi[9][1][0] = ff_hevc_put_hevc_bi_qpel_v64_8_avx2;

#endif
                }
            }
        }
    } else if (bit_depth == 10) {
        if (EXTERNAL_MMX(mm_flags)) {
            if (EXTERNAL_MMXEXT(mm_flags)) {
#if ARCH_X86_32
#endif /* ARCH_X86_32 */
#ifdef OPTI_ASM
                // c->transform_dc_add[0]    =  ff_hevc_idct4_dc_add_10_mmxext;
#endif
#if HAVE_SSE2
                if (EXTERNAL_SSE2(mm_flags)) {
                    c->hevc_v_loop_filter_chroma = ff_hevc_v_loop_filter_chroma_10_sse2;
                    c->hevc_h_loop_filter_chroma = ff_hevc_h_loop_filter_chroma_10_sse2;

#ifdef OPTI_ASM
                    // c->transform_dc_add[1]    =  ff_hevc_idct8_dc_add_10_sse2;
                    // c->transform_dc_add[2]    =  ff_hevc_idct16_dc_add_10_sse2;
                    // c->transform_dc_add[3]    =  ff_hevc_idct32_dc_add_10_sse2;
#endif
                    c->idct_4x4_luma = ff_hevc_transform_4x4_luma_10_sse4;
                    c->idct[0] = ff_hevc_transform_4x4_10_sse4;
                    c->idct[1] = ff_hevc_transform_8x8_10_sse4;
                    c->idct[2] = ff_hevc_transform_16x16_10_sse4;
                    c->idct[3] = ff_hevc_transform_32x32_10_sse4;
                    c->transform_add[0] = ff_hevc_transform_4x4_add_10_sse4;
                    c->transform_add[1] = ff_hevc_transform_8x8_add_10_sse4;
                    c->transform_add[2] = ff_hevc_transform_16x16_add_10_sse4;
                    c->transform_add[3] = ff_hevc_transform_32x32_add_10_sse4;
                    if (ARCH_X86_64) {
                        c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_10_sse2;
                        c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_10_sse2;
                    }

                }
#endif // HAVE_SSE2
#if HAVE_SSSE3
                if (EXTERNAL_SSSE3(mm_flags)) {
#if ARCH_X86_64
                    c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_10_ssse3;
                    c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_10_ssse3;
#endif

                    EPEL_LINKS(c->put_hevc_epel, 0, 0, pel_pixels, 10, sse4);
                    EPEL_LINKS(c->put_hevc_epel, 0, 1, epel_h,     10, sse4);
                    EPEL_LINKS(c->put_hevc_epel, 1, 0, epel_v,     10, sse4);
                    EPEL_LINKS(c->put_hevc_epel, 1, 1, epel_hv,    10, sse4);

                    QPEL_LINKS(c->put_hevc_qpel, 0, 0, pel_pixels, 10, sse4);
                    QPEL_LINKS(c->put_hevc_qpel, 0, 1, qpel_h,     10, sse4);
                    QPEL_LINKS(c->put_hevc_qpel, 1, 0, qpel_v,     10, sse4);
                    QPEL_LINKS(c->put_hevc_qpel, 1, 1, qpel_hv,    10, sse4);

                    c->sao_band_filter    = ff_hevc_sao_band_filter_0_10_sse;
                    c->sao_edge_filter[0] = ff_hevc_sao_edge_filter_0_10_sse;
                    c->sao_edge_filter[1] = ff_hevc_sao_edge_filter_1_10_sse;
                }
#endif //HAVE_SSSE3
#if HAVE_SSE42
                if (EXTERNAL_SSE4(mm_flags)) {

                }
#endif
                if (EXTERNAL_AVX(mm_flags)) {
#ifdef OPTI_ASM
                    // c->transform_dc_add[1]    =  ff_hevc_idct8_dc_add_10_avx;
                    // c->transform_dc_add[2]    =  ff_hevc_idct16_dc_add_10_avx;
                    // c->transform_dc_add[3]    =  ff_hevc_idct32_dc_add_10_avx;
#endif
                }
                if (EXTERNAL_AVX2(mm_flags)) {
#ifdef OPTI_ASM
                    // c->transform_dc_add[2]    =  ff_hevc_idct16_dc_add_10_avx2;
                    // c->transform_dc_add[3]    =  ff_hevc_idct32_dc_add_10_avx2;

                    c->put_hevc_epel[5][0][0] = ff_hevc_put_hevc_pel_pixels16_10_avx2;
                    c->put_hevc_epel[6][0][0] = ff_hevc_put_hevc_pel_pixels24_10_avx2;
                    c->put_hevc_epel[7][0][0] = ff_hevc_put_hevc_pel_pixels32_10_avx2;
                    c->put_hevc_epel[8][0][0] = ff_hevc_put_hevc_pel_pixels48_10_avx2;
                    c->put_hevc_epel[9][0][0] = ff_hevc_put_hevc_pel_pixels64_10_avx2;

                    c->put_hevc_qpel[5][0][0] = ff_hevc_put_hevc_pel_pixels16_10_avx2;
                    c->put_hevc_qpel[6][0][0] = ff_hevc_put_hevc_pel_pixels24_10_avx2;
                    c->put_hevc_qpel[7][0][0] = ff_hevc_put_hevc_pel_pixels32_10_avx2;
                    c->put_hevc_qpel[8][0][0] = ff_hevc_put_hevc_pel_pixels48_10_avx2;
                    c->put_hevc_qpel[9][0][0] = ff_hevc_put_hevc_pel_pixels64_10_avx2;

                    c->put_hevc_epel_uni[5][0][0] = ff_hevc_put_hevc_uni_pel_pixels32_8_avx2;
                    c->put_hevc_epel_uni[6][0][0] = ff_hevc_put_hevc_uni_pel_pixels48_8_avx2;
                    c->put_hevc_epel_uni[7][0][0] = ff_hevc_put_hevc_uni_pel_pixels64_8_avx2;
                    c->put_hevc_epel_uni[8][0][0] = ff_hevc_put_hevc_uni_pel_pixels96_8_avx2;
                    c->put_hevc_epel_uni[9][0][0] = ff_hevc_put_hevc_uni_pel_pixels128_8_avx2;

                    c->put_hevc_qpel_uni[5][0][0] = ff_hevc_put_hevc_uni_pel_pixels32_8_avx2;
                    c->put_hevc_qpel_uni[6][0][0] = ff_hevc_put_hevc_uni_pel_pixels48_8_avx2;
                    c->put_hevc_qpel_uni[7][0][0] = ff_hevc_put_hevc_uni_pel_pixels64_8_avx2;
                    c->put_hevc_qpel_uni[8][0][0] = ff_hevc_put_hevc_uni_pel_pixels96_8_avx2;
                    c->put_hevc_qpel_uni[9][0][0] = ff_hevc_put_hevc_uni_pel_pixels128_8_avx2;

                    c->put_hevc_epel_bi[5][0][0] = ff_hevc_put_hevc_bi_pel_pixels16_10_avx2;
                    c->put_hevc_epel_bi[6][0][0] = ff_hevc_put_hevc_bi_pel_pixels24_10_avx2;
                    c->put_hevc_epel_bi[7][0][0] = ff_hevc_put_hevc_bi_pel_pixels32_10_avx2;
                    c->put_hevc_epel_bi[8][0][0] = ff_hevc_put_hevc_bi_pel_pixels48_10_avx2;
                    c->put_hevc_epel_bi[9][0][0] = ff_hevc_put_hevc_bi_pel_pixels64_10_avx2;
                    c->put_hevc_qpel_bi[5][0][0] = ff_hevc_put_hevc_bi_pel_pixels16_10_avx2;
                    c->put_hevc_qpel_bi[6][0][0] = ff_hevc_put_hevc_bi_pel_pixels24_10_avx2;
                    c->put_hevc_qpel_bi[7][0][0] = ff_hevc_put_hevc_bi_pel_pixels32_10_avx2;
                    c->put_hevc_qpel_bi[8][0][0] = ff_hevc_put_hevc_bi_pel_pixels48_10_avx2;
                    c->put_hevc_qpel_bi[9][0][0] = ff_hevc_put_hevc_bi_pel_pixels64_10_avx2;

                    c->put_hevc_epel[5][0][1] = ff_hevc_put_hevc_epel_h16_10_avx2;
                    c->put_hevc_epel[6][0][1] = ff_hevc_put_hevc_epel_h24_10_avx2;
                    c->put_hevc_epel[7][0][1] = ff_hevc_put_hevc_epel_h32_10_avx2;
                    c->put_hevc_epel[8][0][1] = ff_hevc_put_hevc_epel_h48_10_avx2;
                    c->put_hevc_epel[9][0][1] = ff_hevc_put_hevc_epel_h64_10_avx2;

                    c->put_hevc_epel_uni[5][0][1] = ff_hevc_put_hevc_uni_epel_h16_10_avx2;
                    c->put_hevc_epel_uni[6][0][1] = ff_hevc_put_hevc_uni_epel_h24_10_avx2;
                    c->put_hevc_epel_uni[7][0][1] = ff_hevc_put_hevc_uni_epel_h32_10_avx2;
                    c->put_hevc_epel_uni[8][0][1] = ff_hevc_put_hevc_uni_epel_h48_10_avx2;
                    c->put_hevc_epel_uni[9][0][1] = ff_hevc_put_hevc_uni_epel_h64_10_avx2;

                    c->put_hevc_epel_bi[5][0][1] = ff_hevc_put_hevc_bi_epel_h16_10_avx2;
                    c->put_hevc_epel_bi[6][0][1] = ff_hevc_put_hevc_bi_epel_h24_10_avx2;
                    c->put_hevc_epel_bi[7][0][1] = ff_hevc_put_hevc_bi_epel_h32_10_avx2;
                    c->put_hevc_epel_bi[8][0][1] = ff_hevc_put_hevc_bi_epel_h48_10_avx2;
                    c->put_hevc_epel_bi[9][0][1] = ff_hevc_put_hevc_bi_epel_h64_10_avx2;

                    c->put_hevc_epel[5][1][0] = ff_hevc_put_hevc_epel_v16_10_avx2;
                    c->put_hevc_epel[6][1][0] = ff_hevc_put_hevc_epel_v24_10_avx2;
                    c->put_hevc_epel[7][1][0] = ff_hevc_put_hevc_epel_v32_10_avx2;
                    c->put_hevc_epel[8][1][0] = ff_hevc_put_hevc_epel_v48_10_avx2;
                    c->put_hevc_epel[9][1][0] = ff_hevc_put_hevc_epel_v64_10_avx2;

                    c->put_hevc_epel_uni[5][1][0] = ff_hevc_put_hevc_uni_epel_v16_10_avx2;
                    c->put_hevc_epel_uni[6][1][0] = ff_hevc_put_hevc_uni_epel_v24_10_avx2;
                    c->put_hevc_epel_uni[7][1][0] = ff_hevc_put_hevc_uni_epel_v32_10_avx2;
                    c->put_hevc_epel_uni[8][1][0] = ff_hevc_put_hevc_uni_epel_v48_10_avx2;
                    c->put_hevc_epel_uni[9][1][0] = ff_hevc_put_hevc_uni_epel_v64_10_avx2;

                    c->put_hevc_epel_bi[5][1][0] = ff_hevc_put_hevc_bi_epel_v16_10_avx2;
                    c->put_hevc_epel_bi[6][1][0] = ff_hevc_put_hevc_bi_epel_v24_10_avx2;
                    c->put_hevc_epel_bi[7][1][0] = ff_hevc_put_hevc_bi_epel_v32_10_avx2;
                    c->put_hevc_epel_bi[8][1][0] = ff_hevc_put_hevc_bi_epel_v48_10_avx2;
                    c->put_hevc_epel_bi[9][1][0] = ff_hevc_put_hevc_bi_epel_v64_10_avx2;

                    c->put_hevc_epel[5][1][1] = ff_hevc_put_hevc_epel_hv16_10_avx2;
                    c->put_hevc_epel[6][1][1] = ff_hevc_put_hevc_epel_hv24_10_avx2;
                    c->put_hevc_epel[7][1][1] = ff_hevc_put_hevc_epel_hv32_10_avx2;
                    c->put_hevc_epel[8][1][1] = ff_hevc_put_hevc_epel_hv48_10_avx2;
                    c->put_hevc_epel[9][1][1] = ff_hevc_put_hevc_epel_hv64_10_avx2;

                    c->put_hevc_epel_uni[5][1][1] = ff_hevc_put_hevc_uni_epel_hv16_10_avx2;
                    c->put_hevc_epel_uni[6][1][1] = ff_hevc_put_hevc_uni_epel_hv24_10_avx2;
                    c->put_hevc_epel_uni[7][1][1] = ff_hevc_put_hevc_uni_epel_hv32_10_avx2;
                    c->put_hevc_epel_uni[8][1][1] = ff_hevc_put_hevc_uni_epel_hv48_10_avx2;
                    c->put_hevc_epel_uni[9][1][1] = ff_hevc_put_hevc_uni_epel_hv64_10_avx2;

                    c->put_hevc_epel_bi[5][1][1] = ff_hevc_put_hevc_bi_epel_hv16_10_avx2;
                    c->put_hevc_epel_bi[6][1][1] = ff_hevc_put_hevc_bi_epel_hv24_10_avx2;
                    c->put_hevc_epel_bi[7][1][1] = ff_hevc_put_hevc_bi_epel_hv32_10_avx2;
                    c->put_hevc_epel_bi[8][1][1] = ff_hevc_put_hevc_bi_epel_hv48_10_avx2;
                    c->put_hevc_epel_bi[9][1][1] = ff_hevc_put_hevc_bi_epel_hv64_10_avx2;

                    c->put_hevc_qpel[5][0][1] = ff_hevc_put_hevc_qpel_h16_10_avx2;
                    c->put_hevc_qpel[6][0][1] = ff_hevc_put_hevc_qpel_h24_10_avx2;
                    c->put_hevc_qpel[7][0][1] = ff_hevc_put_hevc_qpel_h32_10_avx2;
                    c->put_hevc_qpel[8][0][1] = ff_hevc_put_hevc_qpel_h48_10_avx2;
                    c->put_hevc_qpel[9][0][1] = ff_hevc_put_hevc_qpel_h64_10_avx2;

                    c->put_hevc_qpel_uni[5][0][1] = ff_hevc_put_hevc_uni_qpel_h16_10_avx2;
                    c->put_hevc_qpel_uni[6][0][1] = ff_hevc_put_hevc_uni_qpel_h24_10_avx2;
                    c->put_hevc_qpel_uni[7][0][1] = ff_hevc_put_hevc_uni_qpel_h32_10_avx2;
                    c->put_hevc_qpel_uni[8][0][1] = ff_hevc_put_hevc_uni_qpel_h48_10_avx2;
                    c->put_hevc_qpel_uni[9][0][1] = ff_hevc_put_hevc_uni_qpel_h64_10_avx2;

                    c->put_hevc_qpel_bi[5][0][1] = ff_hevc_put_hevc_bi_qpel_h16_10_avx2;
                    c->put_hevc_qpel_bi[6][0][1] = ff_hevc_put_hevc_bi_qpel_h24_10_avx2;
                    c->put_hevc_qpel_bi[7][0][1] = ff_hevc_put_hevc_bi_qpel_h32_10_avx2;
                    c->put_hevc_qpel_bi[8][0][1] = ff_hevc_put_hevc_bi_qpel_h48_10_avx2;
                    c->put_hevc_qpel_bi[9][0][1] = ff_hevc_put_hevc_bi_qpel_h64_10_avx2;

                    c->put_hevc_qpel[5][1][0] = ff_hevc_put_hevc_qpel_v16_10_avx2;
                    c->put_hevc_qpel[6][1][0] = ff_hevc_put_hevc_qpel_v24_10_avx2;
                    c->put_hevc_qpel[7][1][0] = ff_hevc_put_hevc_qpel_v32_10_avx2;
                    c->put_hevc_qpel[8][1][0] = ff_hevc_put_hevc_qpel_v48_10_avx2;
                    c->put_hevc_qpel[9][1][0] = ff_hevc_put_hevc_qpel_v64_10_avx2;

                    c->put_hevc_qpel_uni[5][1][0] = ff_hevc_put_hevc_uni_qpel_v16_10_avx2;
                    c->put_hevc_qpel_uni[6][1][0] = ff_hevc_put_hevc_uni_qpel_v24_10_avx2;
                    c->put_hevc_qpel_uni[7][1][0] = ff_hevc_put_hevc_uni_qpel_v32_10_avx2;
                    c->put_hevc_qpel_uni[8][1][0] = ff_hevc_put_hevc_uni_qpel_v48_10_avx2;
                    c->put_hevc_qpel_uni[9][1][0] = ff_hevc_put_hevc_uni_qpel_v64_10_avx2;

                    c->put_hevc_qpel_bi[5][1][0] = ff_hevc_put_hevc_bi_qpel_v16_10_avx2;
                    c->put_hevc_qpel_bi[6][1][0] = ff_hevc_put_hevc_bi_qpel_v24_10_avx2;
                    c->put_hevc_qpel_bi[7][1][0] = ff_hevc_put_hevc_bi_qpel_v32_10_avx2;
                    c->put_hevc_qpel_bi[8][1][0] = ff_hevc_put_hevc_bi_qpel_v48_10_avx2;
                    c->put_hevc_qpel_bi[9][1][0] = ff_hevc_put_hevc_bi_qpel_v64_10_avx2;

                    c->put_hevc_qpel[5][1][1] = ff_hevc_put_hevc_qpel_hv16_10_avx2;
                    c->put_hevc_qpel[6][1][1] = ff_hevc_put_hevc_qpel_hv24_10_avx2;
                    c->put_hevc_qpel[7][1][1] = ff_hevc_put_hevc_qpel_hv32_10_avx2;
                    c->put_hevc_qpel[8][1][1] = ff_hevc_put_hevc_qpel_hv48_10_avx2;
                    c->put_hevc_qpel[9][1][1] = ff_hevc_put_hevc_qpel_hv64_10_avx2;

                    c->put_hevc_qpel_uni[5][1][1] = ff_hevc_put_hevc_uni_qpel_hv16_10_avx2;
                    c->put_hevc_qpel_uni[6][1][1] = ff_hevc_put_hevc_uni_qpel_hv24_10_avx2;
                    c->put_hevc_qpel_uni[7][1][1] = ff_hevc_put_hevc_uni_qpel_hv32_10_avx2;
                    c->put_hevc_qpel_uni[8][1][1] = ff_hevc_put_hevc_uni_qpel_hv48_10_avx2;
                    c->put_hevc_qpel_uni[9][1][1] = ff_hevc_put_hevc_uni_qpel_hv64_10_avx2;

                    c->put_hevc_qpel_bi[5][1][1] = ff_hevc_put_hevc_bi_qpel_hv16_10_avx2;
                    c->put_hevc_qpel_bi[6][1][1] = ff_hevc_put_hevc_bi_qpel_hv24_10_avx2;
                    c->put_hevc_qpel_bi[7][1][1] = ff_hevc_put_hevc_bi_qpel_hv32_10_avx2;
                    c->put_hevc_qpel_bi[8][1][1] = ff_hevc_put_hevc_bi_qpel_hv48_10_avx2;
                    c->put_hevc_qpel_bi[9][1][1] = ff_hevc_put_hevc_bi_qpel_hv64_10_avx2;
#endif
                }
            } else if (bit_depth == 12) {
                if (EXTERNAL_MMX(mm_flags)) {
                    if (EXTERNAL_MMXEXT(mm_flags)) {
#if ARCH_X86_32
#endif /* ARCH_X86_32 */
#ifdef OPTI_ASM
                        // c->transform_dc_add[0]    =  ff_hevc_idct4_dc_add_10_mmxext;
#endif
#if HAVE_SSE2
                        if (EXTERNAL_SSE2(mm_flags)) {
                            // c->hevc_v_loop_filter_chroma = ff_hevc_v_loop_filter_chroma_10_sse2;
                            // c->hevc_h_loop_filter_chroma = ff_hevc_h_loop_filter_chroma_10_sse2;
                            
#ifdef OPTI_ASM
                            // c->transform_dc_add[1]    =  ff_hevc_idct8_dc_add_10_sse2;
                            // c->transform_dc_add[2]    =  ff_hevc_idct16_dc_add_10_sse2;
                            // c->transform_dc_add[3]    =  ff_hevc_idct32_dc_add_10_sse2;
#endif
                            c->idct_4x4_luma = ff_hevc_transform_4x4_luma_12_sse4;
                            c->idct[0] = ff_hevc_transform_4x4_12_sse4;
                            c->idct[1] = ff_hevc_transform_8x8_12_sse4;
                            c->idct[2] = ff_hevc_transform_16x16_12_sse4;
                            c->idct[3] = ff_hevc_transform_32x32_12_sse4;
                            c->transform_add[0] = ff_hevc_transform_4x4_add_12_sse4;
                            c->transform_add[1] = ff_hevc_transform_8x8_add_12_sse4;
                            c->transform_add[2] = ff_hevc_transform_16x16_add_12_sse4;
                            c->transform_add[3] = ff_hevc_transform_32x32_add_12_sse4;
                            
                        }
#endif // HAVE_SSE2
#if HAVE_SSSE3
                        if (EXTERNAL_SSSE3(mm_flags)) {
#if ARCH_X86_64
                            // c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_10_ssse3;
                            // c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_10_ssse3;
#endif
                            
                            EPEL_LINKS(c->put_hevc_epel, 0, 0, pel_pixels, 12, sse4);
                            EPEL_LINKS(c->put_hevc_epel, 0, 1, epel_h, 12, sse4);
                            EPEL_LINKS(c->put_hevc_epel, 1, 0, epel_v, 12, sse4);
                            EPEL_LINKS(c->put_hevc_epel, 1, 1, epel_hv, 12, sse4);
                            
                            QPEL_LINKS(c->put_hevc_qpel, 0, 0, pel_pixels, 12, sse4);
                            QPEL_LINKS(c->put_hevc_qpel, 0, 1, qpel_h, 12, sse4);
                            QPEL_LINKS(c->put_hevc_qpel, 1, 0, qpel_v, 12, sse4);
                            QPEL_LINKS(c->put_hevc_qpel, 1, 1, qpel_hv, 12, sse4);
                            
                            c->sao_band_filter = ff_hevc_sao_band_filter_0_12_sse;
                            c->sao_edge_filter[0] = ff_hevc_sao_edge_filter_0_12_sse;
                            c->sao_edge_filter[1] = ff_hevc_sao_edge_filter_1_12_sse;
                        }
#endif //HAVE_SSSE3
#if HAVE_SSE42
                        if (EXTERNAL_SSE4(mm_flags)) {
                            
                        }
#endif
                    }
                }
            }
        }
    }
}