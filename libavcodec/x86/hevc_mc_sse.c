/*
 * Provide SSE MC functions for HEVC decoding
 * Copyright (c) 2013 Pierre-Edouard LEPERE
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


#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcdsp.h"

#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __SSSE3__
#include <tmmintrin.h>
#endif
#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_filters_sse[7][2][16]) = {
    { { -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58},
      { 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2} },
    { { -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54},
      { 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2} },
    { { -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46},
      { 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4} },
    { { -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36},
      { 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4, 36, -4} },
    { { -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28, -4, 28},
      { 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6, 46, -6} },
    { { -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16, -2, 16},
      { 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4, 54, -4} },
    { { -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10, -2, 10},
      { 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2, 58, -2} }
};
DECLARE_ALIGNED(16, const int16_t, ff_hevc_epel_filters_sse_10[7][2][8]) = {
    { { -2, 58, -2, 58, -2, 58, -2, 58},
      { 10, -2, 10, -2, 10, -2, 10, -2} },
    { { -4, 54, -4, 54, -4, 54, -4, 54},
      { 16, -2, 16, -2, 16, -2, 16, -2} },
    { { -6, 46, -6, 46, -6, 46, -6, 46},
      { 28, -4, 28, -4, 28, -4, 28, -4} },
    { { -4, 36, -4, 36, -4, 36, -4, 36},
      { 36, -4, 36, -4, 36, -4, 36, -4} },
    { { -4, 28, -4, 28, -4, 28, -4, 28},
      { 46, -6, 46, -6, 46, -6, 46, -6} },
    { { -2, 16, -2, 16, -2, 16, -2, 16},
      { 54, -4, 54, -4, 54, -4, 54, -4} },
    { { -2, 10, -2, 10, -2, 10, -2, 10},
      { 58, -2, 58, -2, 58, -2, 58, -2} }
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_filters_sse[3][4][16]) = {
    { { -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4},
      {-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58},
      { 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5},
      {  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0} },
    { { -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4},
      {-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40},
      { 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11, 40,-11},
      {  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1} },
    { {  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1},
      { -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17, -5, 17},
      { 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10, 58,-10},
      {  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1,  4, -1} }
};
DECLARE_ALIGNED(16, const int16_t, ff_hevc_qpel_filters_sse_10[3][4][8]) = {
    { { -1,  4, -1,  4, -1,  4, -1,  4},
      {-10, 58,-10, 58,-10, 58,-10, 58},
      { 17, -5, 17, -5, 17, -5, 17, -5},
      {  1,  0,  1,  0,  1,  0,  1,  0} },
    { { -1,  4, -1,  4, -1,  4, -1,  4},
      {-11, 40,-11, 40,-11, 40,-11, 40},
      { 40,-11, 40,-11, 40,-11, 40,-11},
      {  4, -1,  4, -1,  4, -1,  4, -1} },
    { {  0,  1,  0,  1,  0,  1,  0,  1},
      { -5, 17, -5, 17, -5, 17, -5, 17},
      { 58,-10, 58,-10, 58,-10, 58,-10},
      {  4, -1,  4, -1,  4, -1,  4, -1} }
};

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define QPEL_H_FILTER_8(idx)                                                   \
    c1 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][0]);         \
    c2 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][1]);         \
    c3 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][2]);         \
    c4 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][3])

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define SRC_INIT_8()                                                           \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride
#define SRC_INIT_10()                                                          \
    uint16_t *src       = (uint16_t*) _src;                                    \
    ptrdiff_t srcstride = _srcstride >> 1
#define SRC_INIT_14() SRC_INIT_10()

#define SRC_INIT1_8()                                                          \
        src       = (uint8_t*) _src
#define SRC_INIT1_10()                                                         \
        src       = (uint16_t*) _src

#define PEL_STORE_2(tab)                                                       \
    *((uint32_t *) &tab[x]) = _mm_cvtsi128_si32(x1)
#define PEL_STORE_4(tab)                                                       \
    _mm_storel_epi64((__m128i *) &tab[x], x1)
#define PEL_STORE_8(tab)                                                       \
    _mm_store_si128((__m128i *) &tab[x], x1)
#define PEL_STORE_16(tab)                                                      \
    _mm_store_si128((__m128i *) &tab[x    ], x1);                              \
    _mm_store_si128((__m128i *) &tab[x + 8], x2)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define MUL_ADD_H_2(mul, add, dst, src)                                        \
    src ## 1 = mul(src ## 1, r0);                                              \
    src ## 2 = mul(src ## 2, r0);                                              \
    dst      = add(src ## 1, src ## 2);                                        \
    dst      = add(dst, c0)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_mc_pixelsX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define MC_LOAD_PIXEL()                                                        \
    x1 = _mm_loadu_si128((__m128i *) &src[x])

#define MC_PIXEL_COMPUTE2_8()                                                  \
    x1 = _mm_cvtepu8_epi16(x1);                                                \
    x1 = _mm_slli_epi16(x1, 14 - 8)
#define MC_PIXEL_COMPUTE4_8()                                                  \
    MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE8_8()                                                  \
    MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE16_8()                                                 \
    x2 = _mm_unpackhi_epi8(x1, c0);                                            \
    MC_PIXEL_COMPUTE2_8();                                                     \
    x2 = _mm_slli_epi16(x2, 14 - 8)

#define MC_PIXEL_COMPUTE2_10()                                                 \
    x1 = _mm_slli_epi16(x1, 14 - 10)
#define MC_PIXEL_COMPUTE4_10()                                                 \
    MC_PIXEL_COMPUTE2_10()
#define MC_PIXEL_COMPUTE8_10()                                                 \
    MC_PIXEL_COMPUTE2_10()

#define PUT_HEVC_PEL_PIXELS_VAR2_8()                                           \
    __m128i x1, x2
#define PUT_HEVC_PEL_PIXELS_VAR4_8()  PUT_HEVC_PEL_PIXELS_VAR2_8()
#define PUT_HEVC_PEL_PIXELS_VAR8_8()  PUT_HEVC_PEL_PIXELS_VAR2_8()
#define PUT_HEVC_PEL_PIXELS_VAR16_8()                                          \
    const __m128i c0    = _mm_setzero_si128();                                 \
    __m128i x1, x2
#define PUT_HEVC_PEL_PIXELS_VAR2_10()                                          \
    __m128i x1
#define PUT_HEVC_PEL_PIXELS_VAR4_10()   PUT_HEVC_PEL_PIXELS_VAR2_10()
#define PUT_HEVC_PEL_PIXELS_VAR8_10()   PUT_HEVC_PEL_PIXELS_VAR2_10()
#define PUT_HEVC_PEL_PIXELS_VAR16_10()  PUT_HEVC_PEL_PIXELS_VAR16_8()

#define PUT_HEVC_PEL_PIXELS(H, D)                                              \
void ff_hevc_put_hevc_pel_pixels ## H ## _ ## D ## _sse (                      \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int height,                                 \
                                   intptr_t mx, intptr_t my, int width) {      \
    int x, y;                                                                  \
    PUT_HEVC_PEL_PIXELS_VAR ## H ## _ ## D();                                  \
    SRC_INIT_ ## D();                                                          \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define SRC_INIT_H_8()                                                         \
    uint8_t  *src = ((uint8_t*) _src) - 1;                                     \
    ptrdiff_t srcstride = _srcstride
#define SRC_INIT_H_10()                                                        \
    uint16_t *src = ((uint16_t*) _src) - 1;                                    \
    ptrdiff_t srcstride  = _srcstride >> 1
#define EPEL_FILTER_8(f1, f2, idx)                                             \
    f1 = _mm_load_si128((__m128i *) ff_hevc_epel_filters_sse[idx][0]);         \
    f2 = _mm_load_si128((__m128i *) ff_hevc_epel_filters_sse[idx][1])
#define EPEL_FILTER_10(f1, f2, idx)                                            \
    f1 = _mm_load_si128((__m128i *) ff_hevc_epel_filters_sse_10[idx][0]);      \
    f2 = _mm_load_si128((__m128i *) ff_hevc_epel_filters_sse_10[idx][1])

#define EPEL_LOAD_8(src, stride)                                               \
    x1 = _mm_loadl_epi64((__m128i *) &src[x]);                                 \
    x2 = _mm_loadl_epi64((__m128i *) &src[x +     stride]);                    \
    x3 = _mm_loadl_epi64((__m128i *) &src[x + 2 * stride]);                    \
    x4 = _mm_loadl_epi64((__m128i *) &src[x + 3 * stride]);                    \
    x1 = _mm_unpacklo_epi8(x1, x2);                                            \
    x2 = _mm_unpacklo_epi8(x3, x4)
#define EPEL_LOAD_10(src, stride)                                              \
    x1 = _mm_loadu_si128((__m128i *) &src[x]);                                 \
    x2 = _mm_loadu_si128((__m128i *) &src[x +     stride]);                    \
    x3 = _mm_loadu_si128((__m128i *) &src[x + 2 * stride]);                    \
    x4 = _mm_loadu_si128((__m128i *) &src[x + 3 * stride]);                    \
    t1 = _mm_unpackhi_epi16(x1, x2);                                           \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    t2 = _mm_unpackhi_epi16(x3, x4);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4)

#define EPEL_COMPUTE_8(dst, f1, f2)                                            \
    x1  = _mm_maddubs_epi16(x1, f1);                                           \
    x2  = _mm_maddubs_epi16(x2, f2);                                           \
    dst = _mm_add_epi16(x1, x2)
#define EPEL_COMPUTE(dst, f1, f2, shift)                                      \
    x1  = _mm_madd_epi16(x1, f1);                                              \
    t1  = _mm_madd_epi16(t1, f1);                                              \
    x2  = _mm_madd_epi16(x2, f2);                                              \
    t2  = _mm_madd_epi16(t2, f2);                                              \
    x1  = _mm_add_epi32(x1, x2);                                               \
    t1  = _mm_add_epi32(t1, t2);                                               \
    t1  = _mm_srai_epi32(t1, shift);                                           \
    x1  = _mm_srai_epi32(x1, shift);                                           \
    dst = _mm_packs_epi32(x1, t1)
#define EPEL_COMPUTE_10(dst, f1, f2)    EPEL_COMPUTE(dst, f1, f2, 2)
#define EPEL_COMPUTE_14(dst, f1, f2)    EPEL_COMPUTE(dst, f1, f2, 6)

#define PUT_HEVC_EPEL_H_VAR2_8()                                               \
    __m128i x1, x2, x3, x4, f1, f2
#define PUT_HEVC_EPEL_H_VAR4_8()  PUT_HEVC_EPEL_H_VAR2_8()
#define PUT_HEVC_EPEL_H_VAR8_8()  PUT_HEVC_EPEL_H_VAR2_8()
#define PUT_HEVC_EPEL_H_VAR2_10()                                               \
    __m128i x1, x2, x3, x4, t1, t2, f1, f2
#define PUT_HEVC_EPEL_H_VAR4_10()  PUT_HEVC_EPEL_H_VAR2_10()
#define PUT_HEVC_EPEL_H_VAR8_10()  PUT_HEVC_EPEL_H_VAR2_10()

#define PUT_HEVC_EPEL_H(H, D)                                                  \
void ff_hevc_put_hevc_epel_h ## H ## _ ## D ## _sse (                          \
        int16_t *dst, ptrdiff_t dststride,                                     \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_H_ ## D();                                                        \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_vX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define SRC_INIT_V_8()                                                         \
    ptrdiff_t srcstride = _srcstride;                                          \
    uint8_t  *src       = ((uint8_t*) _src) - srcstride
#define SRC_INIT_V_10()                                                        \
    ptrdiff_t srcstride = _srcstride >> 1;                                     \
    uint16_t *src       = ((uint16_t*) _src) - srcstride

#define PUT_HEVC_EPEL_V_VAR2_8()                                               \
    __m128i x1, x2, x3, x4, f1, f2
#define PUT_HEVC_EPEL_V_VAR4_8()  PUT_HEVC_EPEL_V_VAR2_8()
#define PUT_HEVC_EPEL_V_VAR8_8()  PUT_HEVC_EPEL_V_VAR2_8()
#define PUT_HEVC_EPEL_V_VAR2_10()                                              \
    __m128i x1, x2, x3, x4, t1, t2, f1, f2
#define PUT_HEVC_EPEL_V_VAR4_10()  PUT_HEVC_EPEL_V_VAR2_10()
#define PUT_HEVC_EPEL_V_VAR8_10()  PUT_HEVC_EPEL_V_VAR2_10()

#define PUT_HEVC_EPEL_V(H, D)                                                  \
void ff_hevc_put_hevc_epel_v ## H ## _ ## D ## _sse (                          \
        int16_t *dst, ptrdiff_t dststride,                                     \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    int x, y;                                                                  \
    PUT_HEVC_EPEL_V_VAR ## H ## _ ## D();                                     \
    SRC_INIT_V_ ## D();                                                        \
    EPEL_FILTER_ ## D(f1, f2, my - 1);                                         \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_ ## D(src, srcstride);                                   \
            EPEL_COMPUTE_ ## D(x1, f1, f2);                                    \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hvX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define SRC_INIT_HV_8()                                                        \
    uint8_t  *src2, *src = ((uint8_t*) _src) - 1;                              \
    ptrdiff_t srcstride = _srcstride
#define SRC_INIT_HV_10()                                                       \
    uint16_t *src2, *src = ((uint16_t*) _src) - 1;                             \
    ptrdiff_t srcstride  = _srcstride >> 1

#define PUT_HEVC_EPEL_HV(H, D)                                                 \
void ff_hevc_put_hevc_epel_hv ## H ## _ ## D ## _sse (                         \
        int16_t *dst, ptrdiff_t dststride,                                     \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    int x, y;                                                                  \
    __m128i x1, x2, x3, x4, t1, t2, f1, f2, f3, f4, r1, r2, r3, r4;            \
    int16_t *dst2 = dst;                                                       \
    SRC_INIT_HV_ ## D();                                                       \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
    src2 = src;                                                                \
    EPEL_FILTER_ ## D(f1, f2, mx - 1);                                         \
    EPEL_FILTER_10(f3, f4, my - 1);                                            \
    for (x = 0; x < width; x += H) {                                           \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r1, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r2, f1, f2);                                        \
        src += srcstride;                                                      \
        EPEL_LOAD_ ## D(src, 1);                                               \
        EPEL_COMPUTE_ ## D(r3, f1, f2);                                        \
        src += srcstride;                                                      \
        for (y = 0; y < height; y++) {                                         \
            EPEL_LOAD_ ## D(src, 1);                                           \
            EPEL_COMPUTE_ ## D(r4, f1, f2);                                    \
            src += srcstride;                                                  \
                                                                               \
            t1 = _mm_unpackhi_epi16(r1, r2);                                   \
            x1 = _mm_unpacklo_epi16(r1, r2);                                   \
            t2 = _mm_unpackhi_epi16(r3, r4);                                   \
            x2 = _mm_unpacklo_epi16(r3, r4);                                   \
            EPEL_COMPUTE_14(x1, f3, f4);                                       \
            PEL_STORE_ ## H(dst);                                              \
            dst += dststride;                                                  \
                                                                               \
            r1 = r2;                                                           \
            r2 = r3;                                                           \
            r3 = r4;                                                           \
        }                                                                      \
        src = src2;                                                            \
        dst = dst2;                                                            \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define QPEL_H_LOAD()                                                          \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 3]);                             \
    x2 = _mm_loadu_si128((__m128i *) &src[x - 2]);                             \
    x3 = _mm_loadu_si128((__m128i *) &src[x - 1]);                             \
    x4 = _mm_loadu_si128((__m128i *) &src[x    ]);                             \
    x5 = _mm_loadu_si128((__m128i *) &src[x + 1]);                             \
    x6 = _mm_loadu_si128((__m128i *) &src[x + 2]);                             \
    x7 = _mm_loadu_si128((__m128i *) &src[x + 3]);                             \
    x8 = _mm_loadu_si128((__m128i *) &src[x + 4])

#define QPEL_H_COMPUTE4_8() QPEL_H_COMPUTE8_8()

#define QPEL_H_COMPUTE8_8()                                                    \
    x1 = _mm_unpacklo_epi8(x1, x2);                                            \
    x2 = _mm_unpacklo_epi8(x3, x4);                                            \
    x3 = _mm_unpacklo_epi8(x5, x6);                                            \
    x4 = _mm_unpacklo_epi8(x7, x8);                                            \
    x2 = _mm_maddubs_epi16(x2,c2);                                             \
    x3 = _mm_maddubs_epi16(x3,c3);                                             \
    x1 = _mm_maddubs_epi16(x1,c1);                                             \
    x4 = _mm_maddubs_epi16(x4,c4);                                             \
    x1 = _mm_add_epi16(x1, x2);                                                \
    x2 = _mm_add_epi16(x3, x4);                                                \
    x1 = _mm_add_epi16(x1, x2)

#define QPEL_H_COMPUTE4(shift)                                                \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4);                                           \
    x3 = _mm_unpacklo_epi16(x5, x6);                                           \
    x4 = _mm_unpacklo_epi16(x7, x8);                                           \
    x2 = _mm_madd_epi16(x2,c2);                                                \
    x3 = _mm_madd_epi16(x3,c3);                                                \
    x1 = _mm_madd_epi16(x1,c1);                                                \
    x4 = _mm_madd_epi16(x4,c4);                                                \
    x1 = _mm_add_epi32(x1, x2);                                                \
    x2 = _mm_add_epi32(x3, x4);                                                \
    x1 = _mm_add_epi32(x1, x2);                                                \
    x1 = _mm_srai_epi32(x1, shift);                                            \
    x1 = _mm_packs_epi32(x1, c0)

#define QPEL_H_COMPUTE4_10()    QPEL_H_COMPUTE4(2)
#define QPEL_H_COMPUTE4_14()    QPEL_H_COMPUTE4(6)


#define QPEL_H_COMPUTE16_8()                                                   \
    x9 = x1;                                                                   \
    x1 = _mm_unpacklo_epi8(x9, x2);                                            \
    x2 = _mm_unpackhi_epi8(x9, x2);                                            \
    x9 = x3;                                                                   \
    x3 = _mm_unpacklo_epi8(x9, x4);                                            \
    x4 = _mm_unpackhi_epi8(x9, x4);                                            \
    x9 = x5;                                                                   \
    x5 = _mm_unpacklo_epi8(x9, x6);                                            \
    x6 = _mm_unpackhi_epi8(x9, x6);                                            \
    x9 = x7;                                                                   \
    x7 = _mm_unpacklo_epi8(x9, x8);                                            \
    x8 = _mm_unpackhi_epi8(x9, x8);                                            \
    x1 = _mm_maddubs_epi16(x1,c1);                                             \
    x3 = _mm_maddubs_epi16(x3,c2);                                             \
    x5 = _mm_maddubs_epi16(x5,c3);                                             \
    x7 = _mm_maddubs_epi16(x7,c4);                                             \
    x2 = _mm_maddubs_epi16(x2,c1);                                             \
    x4 = _mm_maddubs_epi16(x4,c2);                                             \
    x6 = _mm_maddubs_epi16(x6,c3);                                             \
    x8 = _mm_maddubs_epi16(x8,c4);                                             \
    x1 = _mm_add_epi16(x1, x3);                                                \
    x3 = _mm_add_epi16(x5, x7);                                                \
    x2 = _mm_add_epi16(x2, x4);                                                \
    x4 = _mm_add_epi16(x6, x8);                                                \
    x1 = _mm_add_epi16(x1, x3);                                                \
    x2 = _mm_add_epi16(x2, x4)

#define QPEL_H_COMPUTE8(shift)                                                \
    x9 = x1;                                                                   \
    x1 = _mm_unpacklo_epi16(x9, x2);                                           \
    x2 = _mm_unpackhi_epi16(x9, x2);                                           \
    x9 = x3;                                                                   \
    x3 = _mm_unpacklo_epi16(x9, x4);                                           \
    x4 = _mm_unpackhi_epi16(x9, x4);                                           \
    x9 = x5;                                                                   \
    x5 = _mm_unpacklo_epi16(x9, x6);                                           \
    x6 = _mm_unpackhi_epi16(x9, x6);                                           \
    x9 = x7;                                                                   \
    x7 = _mm_unpacklo_epi16(x9, x8);                                           \
    x8 = _mm_unpackhi_epi16(x9, x8);                                           \
    x1 = _mm_madd_epi16(x1,c1);                                                \
    x3 = _mm_madd_epi16(x3,c2);                                                \
    x5 = _mm_madd_epi16(x5,c3);                                                \
    x7 = _mm_madd_epi16(x7,c4);                                                \
    x2 = _mm_madd_epi16(x2,c1);                                                \
    x4 = _mm_madd_epi16(x4,c2);                                                \
    x6 = _mm_madd_epi16(x6,c3);                                                \
    x8 = _mm_madd_epi16(x8,c4);                                                \
    x1 = _mm_add_epi32(x1, x3);                                                \
    x3 = _mm_add_epi32(x5, x7);                                                \
    x2 = _mm_add_epi32(x2, x4);                                                \
    x4 = _mm_add_epi32(x6, x8);                                                \
    x1 = _mm_add_epi32(x1, x3);                                                \
    x2 = _mm_add_epi32(x2, x4);                                                \
    x1 = _mm_srai_epi32(x1, shift);                                            \
    x2 = _mm_srai_epi32(x2, shift);                                            \
    x1 = _mm_packs_epi32(x1, x2)

#define QPEL_H_COMPUTE8_10()    QPEL_H_COMPUTE8(2)
#define QPEL_H_COMPUTE8_14()    QPEL_H_COMPUTE8(6)

#define PUT_HEVC_QPEL_H_VAR4_8()                                               \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1, c1, c2, c3, c4
#define PUT_HEVC_QPEL_H_VAR8_8()   PUT_HEVC_QPEL_H_VAR4_8()
#define PUT_HEVC_QPEL_H_VAR16_8()                                              \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, x9, r1, r2, c1, c2, c3, c4

#define PUT_HEVC_QPEL_H(H, D)                                                  \
void ff_hevc_put_hevc_qpel_h ## H ## _ ## D ## _sse (                          \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int height, intptr_t mx, intptr_t my, int width) {   \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_H_VAR ## H ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    QPEL_H_FILTER_ ## D(mx - 1);                                               \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_H_LOAD();                                                     \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define QPEL_FILTER_8(idx)                                                     \
    c1 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][0]);         \
    c2 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][1]);         \
    c3 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][2]);         \
    c4 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse[idx][3])
#define QPEL_FILTER_10(idx)                                                    \
    c1 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse_10[idx][0]);      \
    c2 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse_10[idx][1]);      \
    c3 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse_10[idx][2]);      \
    c4 = _mm_load_si128((__m128i *) ff_hevc_qpel_filters_sse_10[idx][3])

#define QPEL_LOAD_LO_8(inst, src, srcstride)                                   \
    x1 = inst((__m128i *) &src[x - 3 * srcstride]);                            \
    x2 = inst((__m128i *) &src[x - 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x - 1 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x                ]);                            \
    x1 = _mm_unpacklo_epi8(x1, x2);                                            \
    x2 = _mm_unpacklo_epi8(x3, x4)
#define QPEL_LOAD_HI_8(inst, src, srcstride)                                   \
    x1 = inst((__m128i *) &src[x +     srcstride]);                            \
    x2 = inst((__m128i *) &src[x + 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x + 3 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x + 4 * srcstride]);                            \
    x1 = _mm_unpacklo_epi8(x1, x2);                                            \
    x2 = _mm_unpacklo_epi8(x3, x4)
#define QPEL_LOAD_LO_10(inst, src, srcstride)                                  \
    x1 = inst((__m128i *) &src[x - 3 * srcstride]);                            \
    x2 = inst((__m128i *) &src[x - 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x - 1 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x                ]);                            \
    t1 = _mm_unpackhi_epi16(x1, x2);                                           \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    t2 = _mm_unpackhi_epi16(x3, x4);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4)
#define QPEL_LOAD_LO_4_10(inst, src, srcstride)                                \
    x1 = inst((__m128i *) &src[x - 3 * srcstride]);                            \
    x2 = inst((__m128i *) &src[x - 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x - 1 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x                ]);                            \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4)
#define QPEL_LOAD_HI_10(inst, src, srcstride)                                  \
    x1 = inst((__m128i *) &src[x +     srcstride]);                            \
    x2 = inst((__m128i *) &src[x + 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x + 3 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x + 4 * srcstride]);                            \
    t1 = _mm_unpackhi_epi16(x1, x2);                                           \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    t2 = _mm_unpackhi_epi16(x3, x4);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4)
#define QPEL_LOAD_HI_4_10(inst, src, srcstride)                                \
    x1 = inst((__m128i *) &src[x +     srcstride]);                            \
    x2 = inst((__m128i *) &src[x + 2 * srcstride]);                            \
    x3 = inst((__m128i *) &src[x + 3 * srcstride]);                            \
    x4 = inst((__m128i *) &src[x + 4 * srcstride]);                            \
    x1 = _mm_unpacklo_epi16(x1, x2);                                           \
    x2 = _mm_unpacklo_epi16(x3, x4)

#define QPEL_V_LOAD()                                                          \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 3 * srcstride]);                 \
    x2 = _mm_loadu_si128((__m128i *) &src[x - 2 * srcstride]);                 \
    x3 = _mm_loadu_si128((__m128i *) &src[x - 1 * srcstride]);                 \
    x4 = _mm_loadu_si128((__m128i *) &src[x                ]);                 \
    x5 = _mm_loadu_si128((__m128i *) &src[x +     srcstride]);                 \
    x6 = _mm_loadu_si128((__m128i *) &src[x + 2 * srcstride]);                 \
    x7 = _mm_loadu_si128((__m128i *) &src[x + 3 * srcstride]);                 \
    x8 = _mm_loadu_si128((__m128i *) &src[x + 4 * srcstride]);                 \

#define QPEL_LOAD_LO4_8(src, srcstride)  QPEL_LOAD_LO_8(_mm_loadl_epi64 , src, srcstride)
#define QPEL_LOAD_LO8_8(src, srcstride)  QPEL_LOAD_LO_8(_mm_loadl_epi64 , src, srcstride)
#define QPEL_LOAD_HI4_8(src, srcstride)  QPEL_LOAD_HI_8(_mm_loadl_epi64 , src, srcstride)
#define QPEL_LOAD_HI8_8(src, srcstride)  QPEL_LOAD_HI_8(_mm_loadl_epi64 , src, srcstride)
#define QPEL_LOAD_LO4_10(src, srcstride) QPEL_LOAD_LO_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_LO8_10(src, srcstride) QPEL_LOAD_LO_10(_mm_loadu_si128, src, srcstride)
#define QPEL_LOAD_HI4_10(src, srcstride) QPEL_LOAD_HI_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_HI8_10(src, srcstride) QPEL_LOAD_HI_10(_mm_loadu_si128, src, srcstride)
#define QPEL_LOAD_LO4_14(src, srcstride) QPEL_LOAD_LO_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_LO8_14(src, srcstride) QPEL_LOAD_LO_10(_mm_load_si128 , src, srcstride)
#define QPEL_LOAD_HI4_14(src, srcstride) QPEL_LOAD_HI_4_10(_mm_loadl_epi64, src, srcstride)
#define QPEL_LOAD_HI8_14(src, srcstride) QPEL_LOAD_HI_10(_mm_load_si128 , src, srcstride)

#define QPEL_COMPUTE4_8(dst1, dst2, f1, f2)                                    \
    x1   = _mm_maddubs_epi16(x1, f1);                                          \
    x2   = _mm_maddubs_epi16(x2, f2);                                          \
    dst1 = _mm_add_epi16(x1, x2)
#define QPEL_COMPUTE8_8(dst1, dst2, f1, f2)                                    \
    QPEL_COMPUTE4_8(dst1, dst2, f1, f2)

#define QPEL_COMPUTE2_10(dst1, dst2, f1, f2)                                   \
    x1   = _mm_madd_epi16(x1, f1);                                             \
    x2   = _mm_madd_epi16(x2, f2);                                             \
    dst1 = _mm_add_epi32(x1, x2)
#define QPEL_COMPUTE4_10(dst1, dst2, f1, f2)                                   \
    QPEL_COMPUTE2_10(dst1, dst2, f1, f2)
#define QPEL_COMPUTE8_10(dst1, dst2, f1, f2)                                   \
    t1   = _mm_madd_epi16(t1, f1);                                             \
    t2   = _mm_madd_epi16(t2, f2);                                             \
    dst2 = _mm_add_epi32(t1, t2);                                              \
    QPEL_COMPUTE4_10(dst1, dst2, f1, f2)

#define QPEL_MERGE4_8()                                                        \
    x1 = _mm_add_epi16(r1, r3)
#define QPEL_MERGE8_8()                                                        \
    QPEL_MERGE4_8()

#define QPEL_MERGE2_10()                                                       \
    x1 = _mm_add_epi32(r1,r3);                                                 \
    x1 = _mm_srai_epi32(x1, shift);                                            \
    x1 = _mm_packs_epi32(x1, c0)
#define QPEL_MERGE4_10()                                                       \
    QPEL_MERGE2_10()
#define QPEL_MERGE8_10()                                                       \
    x1 = _mm_add_epi32(r1, r3);                                                \
    x2 = _mm_add_epi32(r2, r4);                                                \
    x1 = _mm_srai_epi32(x1, shift);                                            \
    x2 = _mm_srai_epi32(x2, shift);                                            \
    x1 = _mm_packs_epi32(x1, x2)

#define QPEL_FILTER_14(idx)                     QPEL_FILTER_10(idx)
#define QPEL_COMPUTE2_14(dst1, dst2, f1, f2)    QPEL_COMPUTE2_10(dst1, dst2, f1, f2)
#define QPEL_COMPUTE4_14(dst1, dst2, f1, f2)    QPEL_COMPUTE4_10(dst1, dst2, f1, f2)
#define QPEL_COMPUTE8_14(dst1, dst2, f1, f2)    QPEL_COMPUTE8_10(dst1, dst2, f1, f2)
#define QPEL_MERGE2_14()                        QPEL_MERGE2_10()
#define QPEL_MERGE4_14()                        QPEL_MERGE4_10()
#define QPEL_MERGE8_14()                        QPEL_MERGE8_10()

#define PUT_HEVC_QPEL_H_10_VAR4_10()                                           \
    const __m128i c0    = _mm_setzero_si128();                                 \
    __m128i x1, x2, x3, x4, r1, r3, c1, c2, c3, c4
#define PUT_HEVC_QPEL_H_10_VAR8_10()                                           \
    __m128i x1, x2, x3, x4, r1, r2, r3, r4, c1, c2, c3, c4, t1, t2

#define PUT_HEVC_QPEL_H_10(H, D)                                               \
void ff_hevc_put_hevc_qpel_h ## H ## _ ## D ## _sse (                          \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int height, intptr_t mx, intptr_t my, int width) {   \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    PUT_HEVC_QPEL_H_10_VAR ## H ## _ ## D();                                   \
    SRC_INIT_ ## D();                                                          \
    QPEL_FILTER_ ## D(mx - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_LOAD_LO ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r1, r2, c1, c2);                       \
            QPEL_LOAD_HI ## H ## _ ## D(src, 1);                               \
            QPEL_COMPUTE ## H ## _ ## D(r3, r4, c3, c4);                       \
            QPEL_MERGE ## H ## _ ## D();                                       \
            PEL_STORE_ ## H(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_vX_X_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_V_VAR4_8()                                               \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1, c1, c2, c3, c4
#define PUT_HEVC_QPEL_V_VAR8_8()     PUT_HEVC_QPEL_V_VAR4_8()
#define PUT_HEVC_QPEL_V_VAR16_8()                                              \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, x9, r1, r2, c1, c2, c3, c4

#define PUT_HEVC_QPEL_V_VAR4_10()                                           \
    const __m128i c0    = _mm_setzero_si128();                                 \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1, c1, c2, c3, c4
#define PUT_HEVC_QPEL_V_VAR8_10()                                           \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, x9, r1, r2, c1, c2, c3, c4

#define PUT_HEVC_QPEL_V_VAR4_14()     PUT_HEVC_QPEL_V_VAR4_10()
#define PUT_HEVC_QPEL_V_VAR8_14()     PUT_HEVC_QPEL_V_VAR8_10()

#define PUT_HEVC_QPEL_V(V, D)                                                  \
void ff_hevc_put_hevc_qpel_v ## V ## _ ## D ## _sse (                          \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int height, intptr_t mx, intptr_t my, int width) {   \
    int x, y;                                                                  \
    PUT_HEVC_QPEL_V_VAR ## V ## _ ## D();                                      \
    SRC_INIT_ ## D();                                                          \
    QPEL_FILTER_ ## D(my - 1);                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD();                                                     \
            QPEL_H_COMPUTE ## V ## _ ## D();                                   \
            PEL_STORE_ ## V(dst);                                              \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hvX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_HV(H, D)                                                 \
void ff_hevc_put_hevc_qpel_hv ## H ## _ ## D ## _sse (                         \
        int16_t *dst, ptrdiff_t dststride,                                     \
        uint8_t *_src, ptrdiff_t _srcstride,                                   \
        int height,                                                            \
        intptr_t mx, intptr_t my, int width) {                                 \
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];               \
    int16_t *tmp = tmp_array;                                                  \
    SRC_INIT_ ## D();                                                          \
    src -= QPEL_EXTRA_BEFORE * srcstride;                                      \
    ff_hevc_put_hevc_qpel_h ## H ## _ ## D ## _sse(                            \
        tmp, MAX_PB_SIZE, (uint8_t *)src, _srcstride, height + QPEL_EXTRA, mx, my, width); \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                      \
    ff_hevc_put_hevc_qpel_v ## H ## _14_sse(                                   \
        dst, dststride, (uint8_t *) tmp, MAX_PB_SIZE <<1 , height, mx, my, width);\
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// ff_hevc_put_hevc_mc_pixelsX_X_sse
#ifdef __SSE4_1__
PUT_HEVC_PEL_PIXELS(  2, 8)
PUT_HEVC_PEL_PIXELS(  4, 8)
PUT_HEVC_PEL_PIXELS(  8, 8)
PUT_HEVC_PEL_PIXELS( 16, 8)
#endif //__SSE4_1__

#ifdef __SSSE3__
PUT_HEVC_PEL_PIXELS(  2, 10)
PUT_HEVC_PEL_PIXELS(  4, 10)
PUT_HEVC_PEL_PIXELS(  8, 10)

// ff_hevc_put_hevc_epel_hX_X_sse
PUT_HEVC_EPEL_H(  2,  8)
PUT_HEVC_EPEL_H(  4,  8)
PUT_HEVC_EPEL_H(  8,  8)

PUT_HEVC_EPEL_H(  2, 10)
PUT_HEVC_EPEL_H(  4, 10)
PUT_HEVC_EPEL_H(  8, 10)

// ff_hevc_put_hevc_epel_vX_X_sse
PUT_HEVC_EPEL_V(  2,  8)
PUT_HEVC_EPEL_V(  4,  8)
PUT_HEVC_EPEL_V(  8,  8)

PUT_HEVC_EPEL_V(  2, 10)
PUT_HEVC_EPEL_V(  4, 10)
PUT_HEVC_EPEL_V(  8, 10)

// ff_hevc_put_hevc_epel_hvX_X_sse
PUT_HEVC_EPEL_HV(  2,  8)
PUT_HEVC_EPEL_HV(  4,  8)
PUT_HEVC_EPEL_HV(  8,  8)

PUT_HEVC_EPEL_HV(  2, 10)
PUT_HEVC_EPEL_HV(  4, 10)
PUT_HEVC_EPEL_HV(  8, 10)

// ff_hevc_put_hevc_qpel_hX_X_X_sse
PUT_HEVC_QPEL_H(  4,  8)
PUT_HEVC_QPEL_H(  8,  8)
PUT_HEVC_QPEL_H( 16,  8)

PUT_HEVC_QPEL_H_10(  4, 10)
PUT_HEVC_QPEL_H_10(  8, 10)

// ff_hevc_put_hevc_qpel_vX_X_X_sse
PUT_HEVC_QPEL_V(  4,  8)
PUT_HEVC_QPEL_V(  8,  8)
PUT_HEVC_QPEL_V( 16,  8)

PUT_HEVC_QPEL_V(  4, 10)
PUT_HEVC_QPEL_V(  8, 10)

static PUT_HEVC_QPEL_V(  4, 14)
static PUT_HEVC_QPEL_V(  8, 14)

// ff_hevc_put_hevc_qpel_hvX_X_sse
PUT_HEVC_QPEL_HV(  4,  8)
PUT_HEVC_QPEL_HV(  8,  8)

PUT_HEVC_QPEL_HV(  4, 10)
PUT_HEVC_QPEL_HV(  8, 10)
#endif //__SSSE3__

#define mc_red_func(name, bitd, step, W) \
void ff_hevc_put_hevc_##name##W##_##bitd##_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my, int width) \
{ \
    ff_hevc_put_hevc_##name##step##_##bitd##_sse(dst, dststride, _src, _srcstride, height, mx, my, width);   \
}


#ifdef __SSE4_1__
mc_red_func(pel_pixels, 8, 2,  6);
mc_red_func(pel_pixels, 8, 4, 12);
mc_red_func(pel_pixels, 8, 8, 24);
mc_red_func(pel_pixels, 8,16, 32);
mc_red_func(pel_pixels, 8,16, 48);
mc_red_func(pel_pixels, 8,16, 64);
#endif //__SSE4_1__

#ifdef __SSSE3__
mc_red_func(pel_pixels,10, 2,  6);
mc_red_func(pel_pixels,10, 4, 12);
mc_red_func(pel_pixels,10, 8, 16);
mc_red_func(pel_pixels,10, 8, 24);
mc_red_func(pel_pixels,10, 8, 32);
mc_red_func(pel_pixels,10, 8, 48);
mc_red_func(pel_pixels,10, 8, 64);

mc_red_func(qpel_h, 8, 4, 12);
mc_red_func(qpel_h, 8, 8, 24);
mc_red_func(qpel_h, 8,16, 32);
mc_red_func(qpel_h, 8,16, 48);
mc_red_func(qpel_h, 8,16, 64);

mc_red_func(qpel_h,10, 4, 12);
mc_red_func(qpel_h,10, 8, 16);
mc_red_func(qpel_h,10, 8, 24);
mc_red_func(qpel_h,10, 8, 32);
mc_red_func(qpel_h,10, 8, 48);
mc_red_func(qpel_h,10, 8, 64);

mc_red_func(qpel_v, 8, 4, 12);
mc_red_func(qpel_v, 8, 8, 24);
mc_red_func(qpel_v, 8,16, 32);
mc_red_func(qpel_v, 8,16, 48);
mc_red_func(qpel_v, 8,16, 64);

mc_red_func(qpel_v,10, 4, 12);
mc_red_func(qpel_v,10, 8, 16);
mc_red_func(qpel_v,10, 8, 24);
mc_red_func(qpel_v,10, 8, 32);
mc_red_func(qpel_v,10, 8, 48);
mc_red_func(qpel_v,10, 8, 64);

mc_red_func(qpel_hv, 8, 4, 12);
mc_red_func(qpel_hv, 8, 8, 16);
mc_red_func(qpel_hv, 8, 8, 24);
mc_red_func(qpel_hv, 8, 8, 32);
mc_red_func(qpel_hv, 8, 8, 48);
mc_red_func(qpel_hv, 8, 8, 64);

mc_red_func(qpel_hv,10, 4, 12);
mc_red_func(qpel_hv,10, 8, 16);
mc_red_func(qpel_hv,10, 8, 24);
mc_red_func(qpel_hv,10, 8, 32);
mc_red_func(qpel_hv,10, 8, 48);
mc_red_func(qpel_hv,10, 8, 64);

mc_red_func(epel_h, 8, 4, 12);
mc_red_func(epel_h, 8, 8, 16);
mc_red_func(epel_h, 8, 8, 24);
mc_red_func(epel_h, 8, 8, 32);
mc_red_func(epel_h, 8, 8, 48);
mc_red_func(epel_h, 8, 8, 64);

mc_red_func(epel_h,10, 4, 12);
mc_red_func(epel_h,10, 8, 16);
mc_red_func(epel_h,10, 8, 24);
mc_red_func(epel_h,10, 8, 32);
mc_red_func(epel_h,10, 8, 48);
mc_red_func(epel_h,10, 8, 64);

mc_red_func(epel_v, 8, 4, 12);
mc_red_func(epel_v, 8, 8, 16);
mc_red_func(epel_v, 8, 8, 24);
mc_red_func(epel_v, 8, 8, 32);
mc_red_func(epel_v, 8, 8, 48);
mc_red_func(epel_v, 8, 8, 64);

mc_red_func(epel_v,10, 4, 12);
mc_red_func(epel_v,10, 8, 16);
mc_red_func(epel_v,10, 8, 24);
mc_red_func(epel_v,10, 8, 32);
mc_red_func(epel_v,10, 8, 48);
mc_red_func(epel_v,10, 8, 64);

mc_red_func(epel_hv, 8, 4, 12);
mc_red_func(epel_hv, 8, 8, 16);
mc_red_func(epel_hv, 8, 8, 24);
mc_red_func(epel_hv, 8, 8, 32);
mc_red_func(epel_hv, 8, 8, 48);
mc_red_func(epel_hv, 8, 8, 64);

mc_red_func(epel_hv,10, 4, 12);
mc_red_func(epel_hv,10, 8, 16);
mc_red_func(epel_hv,10, 8, 24);
mc_red_func(epel_hv,10, 8, 32);
mc_red_func(epel_hv,10, 8, 48);
mc_red_func(epel_hv,10, 8, 64);

#endif //__SSSE3__


