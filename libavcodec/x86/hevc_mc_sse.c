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

#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define BIT_DEPTH 8

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define EPEL_V_FILTER_INIT(set)                                                \
    const int8_t *filter_v = ff_hevc_epel_filters[my - 1];                     \
    const __m128i c1 = set(filter_v[0]);                                       \
    const __m128i c2 = set(filter_v[1]);                                       \
    const __m128i c3 = set(filter_v[2]);                                       \
    const __m128i c4 = set(filter_v[3])

#define EPEL_H_FILTER_INIT()                                                   \
    const int8_t *filter_h  = ff_hevc_epel_filters[mx - 1];                    \
    __m128i r0 = _mm_shuffle_epi32(_mm_loadl_epi64((const __m128i *)filter_h),0)

#define QPEL_FILTER_INIT_1(inst)                                               \
    const __m128i c1 = inst( -1);                                              \
    const __m128i c2 = inst(  4);                                              \
    const __m128i c3 = inst(-10);                                              \
    const __m128i c4 = inst( 58);                                              \
    const __m128i c5 = inst( 17);                                              \
    const __m128i c6 = inst( -5);                                              \
    const __m128i c7 = inst(  1);                                              \
    const __m128i c8 = _mm_setzero_si128()

#define QPEL_FILTER_INIT_2(inst)                                               \
    const __m128i c1 = inst( -1);                                              \
    const __m128i c2 = inst(  4);                                              \
    const __m128i c3 = inst(-11);                                              \
    const __m128i c4 = inst( 40);                                              \
    const __m128i c5 = inst( 40);                                              \
    const __m128i c6 = inst(-11);                                              \
    const __m128i c7 = inst(  4);                                              \
    const __m128i c8 = inst( -1)

#define QPEL_FILTER_INIT_3(inst)                                               \
    const __m128i c1 = _mm_setzero_si128();                                    \
    const __m128i c2 = inst(  1);                                              \
    const __m128i c3 = inst( -5);                                              \
    const __m128i c4 = inst( 17);                                              \
    const __m128i c5 = inst( 58);                                              \
    const __m128i c6 = inst(-10);                                              \
    const __m128i c7 = inst(  4);                                              \
    const __m128i c8 = inst( -1)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define MUL_ADD_H_1(mul, add, dst, src)                                        \
    src ## 2 = mul(src ## 2, r0);                                              \
    dst      = add(src ## 2, c0)
#define MUL_ADD_H_2(mul, add, dst, src)                                        \
    src ## 2 = mul(src ## 2, r0);                                              \
    src ## 3 = mul(src ## 3, r0);                                              \
    src ## 2 = add(src ## 2, src ## 3);                                        \
    dst      = add(src ## 2, c0)
#define MUL_ADD_H_2_2(mul, add, dst, src)                                      \
    src ## 2 = mul(src ## 2, r0);                                              \
    src ## 3 = mul(src ## 3, r0);                                              \
    dst      = add(src ## 2, src ## 3)
#define MUL_ADD_H_4(mul, add, dst, src)                                        \
    src ## 2 = mul(src ## 2, r0);                                              \
    src ## 3 = mul(src ## 3, r0);                                              \
    src ## 4 = mul(src ## 4, r0);                                              \
    src ## 5 = mul(src ## 5, r0);                                              \
    src ## 2 = add(src ## 2, src ## 3);                                        \
    src ## 4 = add(src ## 4, src ## 5);                                        \
    dst      = add(src ## 2, src ## 4)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define LOAD_V_4(inst, tab)                                                    \
    x1 = inst((__m128i *) &tab[x -     srcstride]);                            \
    x2 = inst((__m128i *) &tab[x                ]);                            \
    x3 = inst((__m128i *) &tab[x +     srcstride]);                            \
    x4 = inst((__m128i *) &tab[x + 2 * srcstride])
#define LOAD_V_8(inst, tab)                                                    \
    x1 = inst((__m128i *) &tab[x - 3 * srcstride]);                            \
    x2 = inst((__m128i *) &tab[x - 2 * srcstride]);                            \
    x3 = inst((__m128i *) &tab[x -     srcstride]);                            \
    x4 = inst((__m128i *) &tab[x                ]);                            \
    x5 = inst((__m128i *) &tab[x +     srcstride]);                            \
    x6 = inst((__m128i *) &tab[x + 2 * srcstride]);                            \
    x7 = inst((__m128i *) &tab[x + 3 * srcstride]);                            \
    x8 = inst((__m128i *) &tab[x + 4 * srcstride])

#define UNPACK_V_4(inst, dst)                                                  \
    dst ## 1 = inst(x1, c0);                                                   \
    dst ## 2 = inst(x2, c0);                                                   \
    dst ## 3 = inst(x3, c0);                                                   \
    dst ## 4 = inst(x4, c0)
#define UNPACK_V_8(inst, dst)                                                  \
    dst ## 1 = inst(x1, c0);                                                   \
    dst ## 2 = inst(x2, c0);                                                   \
    dst ## 3 = inst(x3, c0);                                                   \
    dst ## 4 = inst(x4, c0);                                                   \
    dst ## 5 = inst(x5, c0);                                                   \
    dst ## 6 = inst(x6, c0);                                                   \
    dst ## 7 = inst(x7, c0);                                                   \
    dst ## 8 = inst(x8, c0)

#define MUL_ADD_V_4(mul, add, dst, src)                                        \
    dst = mul(src ## 1, c1);                                                   \
    dst = add(dst, mul(src ## 2, c2));                                         \
    dst = add(dst, mul(src ## 3, c3));                                         \
    dst = add(dst, mul(src ## 4, c4))
#define MUL_ADD_V_8(mul, add, dst, src)                                        \
    dst = mul(src ## 1, c1);                                                   \
    dst = add(dst, mul(src ## 2, c2));                                         \
    dst = add(dst, mul(src ## 3, c3));                                         \
    dst = add(dst, mul(src ## 4, c4));                                         \
    dst = add(dst, mul(src ## 5, c5));                                         \
    dst = add(dst, mul(src ## 6, c6));                                         \
    dst = add(dst, mul(src ## 7, c7));                                         \
    dst = add(dst, mul(src ## 8, c8))

#define PEL_STORE2(tab)                                                        \
    _mm_maskmoveu_si128(r1, mask, (char *) &tab[x])
#define PEL_STORE4(tab)                                                        \
    _mm_storel_epi64((__m128i *) &tab[x], r1)
#define PEL_STORE8(tab)                                                        \
    _mm_storeu_si128((__m128i *) &tab[x], r1)
#define PEL_STORE16(tab)                                                       \
    _mm_storeu_si128((__m128i *) &tab[x    ], r1);                             \
    _mm_storeu_si128((__m128i *) &tab[x + 8], r2)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define UNPACK_HV_V_4()                                                        \
    x1 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x1), 16);                       \
    x2 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x2), 16);                       \
    x3 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x3), 16);                       \
    x4 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x4), 16)
#define UNPACK_HV_V_8()                                                        \
    x1 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x1), 16);                       \
    x2 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x2), 16);                       \
    x3 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x3), 16);                       \
    x4 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x4), 16);                       \
    x5 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x5), 16);                       \
    x6 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x6), 16);                       \
    x7 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x7), 16);                       \
    x8 = _mm_srai_epi32(_mm_unpacklo_epi16(c0, x8), 16)

#define UNPACK_HV_UNI(x, c, t)                                                 \
    r1 = _mm_mullo_epi16(x, c);                                                \
    r2 = _mm_mulhi_epi16(x, c);                                                \
    t  = _mm_unpacklo_epi16(r1, r2);                                           \
    x  = _mm_unpackhi_epi16(r1, r2)

#define ADD_HV_V_4(add, dst, src)                                              \
    dst = add(src ## 1, src ## 2);                                             \
    dst = add(dst, add(src ## 3, src ## 4))
#define ADD_HV_V_8(add, dst, src)                                              \
    dst = add(src ## 1, src ## 2);                                             \
    dst = add(dst, add(src ## 3, src ## 4));                                   \
    dst = add(dst, add(src ## 5, src ## 6));                                   \
    dst = add(dst, add(src ## 7, src ## 8))

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define WEIGHTED_INIT2()                                                       \
    __m128i x1, x2;                                                            \
    const __m128i mask = _mm_set_epi16(0, 0, 0, 0, 0, 0, 0, -1)
#define WEIGHTED_INIT4()                                                       \
    __m128i x1, x2;                                                            \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define WEIGHTED_INIT8()                                                       \
    __m128i x1, x2
#define WEIGHTED_INIT16()                                                      \
    __m128i x1, x2, x3

#define WEIGHTED_LOAD2()                                                       \
    x1 = _mm_loadl_epi64((__m128i *) &src[x    ]);                             \
    WEIGHTED_COMPUTE(x1, x2)
#define WEIGHTED_LOAD4()                                                       \
    WEIGHTED_LOAD2()
#define WEIGHTED_LOAD8()                                                       \
    x1 = _mm_load_si128((__m128i *) &src[x    ]);                              \
    WEIGHTED_COMPUTE(x1, x2)
#define WEIGHTED_LOAD16()                                                      \
    WEIGHTED_LOAD8();                                                          \
    x2 = _mm_load_si128((__m128i *) &src[x + 8]);                              \
    WEIGHTED_COMPUTE(x2, x3)

#define WEIGHTED_LOAD2_2()                                                     \
    x1 = _mm_loadl_epi64((__m128i *) &src1[x    ]);                            \
    x2 = _mm_loadl_epi64((__m128i *) &src2[x    ]);                            \
    WEIGHTED_COMPUTE(x1, x2)
#define WEIGHTED_LOAD4_2()                                                     \
    WEIGHTED_LOAD2_2()  
#define WEIGHTED_LOAD8_2()                                                     \
    x1 = _mm_load_si128((__m128i *) &src1[x    ]);                             \
    x2 = _mm_load_si128((__m128i *) &src2[x    ]);                             \
    WEIGHTED_COMPUTE(x1, x2)
#define WEIGHTED_LOAD16_2()                                                    \
    WEIGHTED_LOAD8_2();                                                        \
    x2 = _mm_load_si128((__m128i *) &src1[x + 8]);                             \
    x3 = _mm_load_si128((__m128i *) &src2[x + 8]);                             \
    WEIGHTED_COMPUTE(x2, x3)

#define WEIGHTED_STORE2()                                                      \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    _mm_maskmoveu_si128(x1, mask, (char *) &dst[x])
#define WEIGHTED_STORE4()                                                      \
    WEIGHTED_STORE2()
#define WEIGHTED_STORE8()                                                      \
    x1 = _mm_packus_epi16(x1, x1);                                             \
    _mm_storel_epi64((__m128i *) &dst[x], x1)
#define WEIGHTED_STORE16()                                                     \
    x1 = _mm_packus_epi16(x1, x2);                                             \
    _mm_storeu_si128((__m128i *) &dst[x], x1)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_unweighted_pred_8_sse
////////////////////////////////////////////////////////////////////////////////
#define WEIGHTED_COMPUTE(reg1, reg2)                                           \
    reg1 = _mm_srai_epi16(_mm_adds_epi16(reg1, c1), 14 - BIT_DEPTH)

#define PUT_UNWEIGHTED_PRED(H)                                                 \
static void put_unweighted_pred ## H ## _8_sse (                               \
                                       uint8_t *dst, ptrdiff_t dststride,      \
                                       int16_t *src, ptrdiff_t srcstride,      \
                                       int width, int height) {                \
    int x, y;                                                                  \
    const __m128i c1   = _mm_set1_epi16(32);                                   \
    WEIGHTED_INIT ## H();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H();                                              \
            WEIGHTED_STORE ## H();                                             \
        }                                                                      \
        dst += dststride;                                                      \
        src += srcstride;                                                      \
    }                                                                          \
}
PUT_UNWEIGHTED_PRED(2)
PUT_UNWEIGHTED_PRED(4)
PUT_UNWEIGHTED_PRED(8)
PUT_UNWEIGHTED_PRED(16)

#undef WEIGHTED_COMPUTE

void ff_hevc_put_unweighted_pred_8_sse(
                                       uint8_t *dst, ptrdiff_t dststride,
                                       int16_t *src, ptrdiff_t srcstride,
                                       int width, int height) {
    if(!(width & 15)) {
        put_unweighted_pred16_8_sse(dst, dststride, src, srcstride, width, height);
    } else if(!(width & 7)) {
        put_unweighted_pred8_8_sse(dst, dststride, src, srcstride, width, height);
    } else if(!(width & 3)) {
        put_unweighted_pred4_8_sse(dst, dststride, src, srcstride, width, height);
    } else {
        put_unweighted_pred2_8_sse(dst, dststride, src, srcstride, width, height);
    }
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_weighted_pred_avg_8_sse
////////////////////////////////////////////////////////////////////////////////
#define WEIGHTED_COMPUTE(reg1, reg2)                                           \
    reg1 = _mm_adds_epi16(reg1, c1);                                           \
    reg1 = _mm_adds_epi16(reg1, reg2);                                         \
    reg1 = _mm_srai_epi16(reg1, 14 + 1 - BIT_DEPTH)

#define PUT_WEIGHTED_PRED_AVG(H)                                               \
static void put_weighted_pred_avg ## H ## _8_sse(                              \
                                uint8_t *dst, ptrdiff_t dststride,             \
                                int16_t *src1, int16_t *src2,                  \
                                ptrdiff_t srcstride,                           \
                                int width, int height) {                       \
    int x, y;                                                                  \
    const __m128i c1    = _mm_set1_epi16(64);                                  \
    WEIGHTED_INIT ## H();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H ## _2();                                        \
            WEIGHTED_STORE ## H();                                             \
        }                                                                      \
        dst  += dststride;                                                     \
        src1 += srcstride;                                                     \
        src2 += srcstride;                                                     \
    }                                                                          \
}

PUT_WEIGHTED_PRED_AVG(2)
PUT_WEIGHTED_PRED_AVG(4)
PUT_WEIGHTED_PRED_AVG(8)
PUT_WEIGHTED_PRED_AVG(16)

#undef WEIGHTED_COMPUTE

void ff_hevc_put_weighted_pred_avg_8_sse(
                                        uint8_t *dst, ptrdiff_t dststride,
                                        int16_t *src1, int16_t *src2,
                                        ptrdiff_t srcstride,
                                        int width, int height) {
    if(!(width & 15))
        put_weighted_pred_avg16_8_sse(dst, dststride,
                src1, src2, srcstride, width, height);
    else if(!(width & 7))
        put_weighted_pred_avg8_8_sse(dst, dststride,
                src1, src2, srcstride, width, height);
    else if(!(width & 3))
        put_weighted_pred_avg4_8_sse(dst, dststride,
                src1, src2, srcstride, width, height);
    else
        put_weighted_pred_avg2_8_sse(dst, dststride,
                src1, src2, srcstride, width, height);
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_weighted_pred_8_sse
////////////////////////////////////////////////////////////////////////////////
#define WEIGHTED_COMPUTE(reg1, reg2)                                           \
    reg2 = _mm_unpackhi_epi16(_mm_mullo_epi16(reg1, c1), _mm_mulhi_epi16(reg1, c1)); \
    reg1 = _mm_unpacklo_epi16(_mm_mullo_epi16(reg1, c1), _mm_mulhi_epi16(reg1, c1)); \
    reg1 = _mm_srai_epi32(_mm_add_epi32(reg1, add2), log2Wd);                  \
    reg2 = _mm_srai_epi32(_mm_add_epi32(reg2, add2), log2Wd);                  \
    reg1 = _mm_add_epi32(reg1, add);                                           \
    reg2 = _mm_add_epi32(reg2, add);                                           \
    reg1 = _mm_packus_epi32(reg1, reg2)

#define WEIGHTED_PRED(H)                                                       \
static void weighted_pred ## H ## _8_sse(                                      \
                                    uint8_t denom,                             \
                                    int16_t wlxFlag, int16_t olxFlag,          \
                                    uint8_t *dst, ptrdiff_t dststride,         \
                                    int16_t *src, ptrdiff_t srcstride,         \
                                    int width, int height) {                   \
    int x, y;                                                                  \
    int log2Wd          = denom + 14 - BIT_DEPTH;                              \
    const __m128i add   = _mm_set1_epi32(olxFlag << (BIT_DEPTH - 8));          \
    const __m128i add2  = _mm_set1_epi32(1 << (log2Wd - 1));                   \
    const __m128i c1    = _mm_set1_epi16(wlxFlag);                             \
    WEIGHTED_INIT ## H();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H();                                              \
            WEIGHTED_STORE ## H();                                             \
        }                                                                      \
        dst += dststride;                                                      \
        src += srcstride;                                                      \
    }                                                                          \
}

WEIGHTED_PRED(2)
WEIGHTED_PRED(4)
WEIGHTED_PRED(8)
WEIGHTED_PRED(16)

#undef WEIGHTED_COMPUTE

void ff_hevc_weighted_pred_8_sse(
                                 uint8_t denom,
                                 int16_t wlxFlag, int16_t olxFlag,
                                 uint8_t *dst, ptrdiff_t dststride,
                                 int16_t *src, ptrdiff_t srcstride,
                                 int width, int height) {
    if(!(width & 15))
        weighted_pred16_8_sse(denom, wlxFlag, olxFlag,
                dst, dststride, src, srcstride, width, height);
    else if(!(width & 7))
        weighted_pred8_8_sse(denom, wlxFlag, olxFlag,
                dst, dststride, src, srcstride, width, height);
    else if(!(width & 3))
        weighted_pred4_8_sse(denom, wlxFlag, olxFlag,
                dst, dststride, src, srcstride, width, height);
    else
        weighted_pred2_8_sse(denom, wlxFlag, olxFlag,
                dst, dststride, src, srcstride, width, height);
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_weighted_pred_avg_8_sse
////////////////////////////////////////////////////////////////////////////////
#define WEIGHTED_COMPUTE(reg1, reg2)                                           \
    t1   = _mm_unpacklo_epi16(_mm_mullo_epi16(reg1, c1), _mm_mulhi_epi16(reg1, c1)); \
    t2   = _mm_unpacklo_epi16(_mm_mullo_epi16(reg2, c2), _mm_mulhi_epi16(reg2, c2)); \
    reg1 = _mm_unpackhi_epi16(_mm_mullo_epi16(reg1, c1), _mm_mulhi_epi16(reg1, c1)); \
    reg2 = _mm_unpackhi_epi16(_mm_mullo_epi16(reg2, c2), _mm_mulhi_epi16(reg2, c2)); \
    reg1 = _mm_add_epi32(reg1, reg2);                                          \
    reg2 = _mm_add_epi32(t1, t2);                                              \
    reg1 = _mm_srai_epi32(_mm_add_epi32(reg1, c3), shift);                     \
    reg2 = _mm_srai_epi32(_mm_add_epi32(reg2, c3), shift);                     \
    reg1 = _mm_packus_epi32(reg2, reg1)

#define WEIGHTED_PRED_AVG(H)                                                   \
static void weighted_pred_avg ## H ## _8_sse(                                  \
                                    uint8_t denom,                             \
                                    int16_t wl0Flag, int16_t wl1Flag,          \
                                    int16_t ol0Flag, int16_t ol1Flag,          \
                                    uint8_t *dst, ptrdiff_t dststride,         \
                                    int16_t *src1, int16_t *src2,              \
                                    ptrdiff_t srcstride,                       \
                                    int width, int height) {                   \
    int x, y;                                                                  \
    int log2Wd       = denom + 14 - BIT_DEPTH;                                 \
    int shift        = log2Wd + 1;                                             \
    int o0           = ol0Flag << (BIT_DEPTH - 8);                             \
    int o1           = ol1Flag << (BIT_DEPTH - 8);                             \
    const __m128i c1 = _mm_set1_epi16(wl0Flag);                                \
    const __m128i c2 = _mm_set1_epi16(wl1Flag);                                \
    const __m128i c3 = _mm_set1_epi32((o0 + o1 + 1) << log2Wd);                \
    __m128i t1, t2;                                                            \
    WEIGHTED_INIT ## H();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H ## _2();                                        \
            WEIGHTED_STORE ## H();                                             \
        }                                                                      \
        dst  += dststride;                                                     \
        src1 += srcstride;                                                     \
        src2 += srcstride;                                                     \
    }                                                                          \
}

WEIGHTED_PRED_AVG(2)
WEIGHTED_PRED_AVG(4)
WEIGHTED_PRED_AVG(8)
WEIGHTED_PRED_AVG(16)

#undef WEIGHTED_COMPUTE

void ff_hevc_weighted_pred_avg_8_sse(
                                 uint8_t denom,
                                 int16_t wl0Flag, int16_t wl1Flag,
                                 int16_t ol0Flag, int16_t ol1Flag,
                                 uint8_t *dst, ptrdiff_t dststride,
                                 int16_t *src1, int16_t *src2,
                                 ptrdiff_t srcstride,
                                 int width, int height) {
    if(!(width & 15))
        weighted_pred_avg16_8_sse(denom, wl0Flag, wl1Flag, ol0Flag, ol1Flag,
                dst, dststride, src1, src2, srcstride, width, height);
    else if(!(width & 7))
        weighted_pred_avg8_8_sse(denom, wl0Flag, wl1Flag, ol0Flag, ol1Flag,
                dst, dststride, src1, src2, srcstride, width, height);
    else if(!(width & 3))
        weighted_pred_avg4_8_sse(denom, wl0Flag, wl1Flag, ol0Flag, ol1Flag,
                dst, dststride, src1, src2, srcstride, width, height);
    else
        weighted_pred_avg2_8_sse(denom, wl0Flag, wl1Flag, ol0Flag, ol1Flag,
                dst, dststride, src1, src2, srcstride, width, height);
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////


void ff_hevc_put_hevc_epel_pixels_10_sse(int16_t *dst, ptrdiff_t dststride,
                                         uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx,
                                         int my, int16_t* mcbuffer) {
    int x, y;
    __m128i x1, x2;
    uint16_t *src = (uint16_t*) _src;
    ptrdiff_t srcstride = _srcstride>>1;
    if(!(width & 7)){
        x1= _mm_setzero_si128();
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 8) {

                x2 = _mm_loadu_si128((__m128i *) &src[x]);
                x2 = _mm_slli_epi16(x2, 4);         //shift 14 - BIT LENGTH
                _mm_store_si128((__m128i *) &dst[x], x2);

            }
            src += srcstride;
            dst += dststride;
        }
    }else  if(!(width & 3)){
        x1= _mm_setzero_si128();
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 4) {

                x2 = _mm_loadl_epi64((__m128i *) &src[x]);
                x2 = _mm_slli_epi16(x2, 4);     //shift 14 - BIT LENGTH

                _mm_storel_epi64((__m128i *) &dst[x], x2);

            }
            src += srcstride;
            dst += dststride;
        }
    }else{
        x1= _mm_setzero_si128();
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 2) {

                x2 = _mm_loadl_epi64((__m128i *) &src[x]);
                x2 = _mm_slli_epi16(x2, 4);     //shift 14 - BIT LENGTH
                _mm_maskmoveu_si128(x2,_mm_set_epi8(0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1),(char *) (dst+x));
            }
            src += srcstride;
            dst += dststride;
        }
    }

}

void ff_hevc_put_hevc_epel_h_10_sse(int16_t *dst, ptrdiff_t dststride,
                                    uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx,
                                    int my, int16_t* mcbuffer) {
    int x, y;
    uint16_t *src = (uint16_t*) _src;
    ptrdiff_t srcstride = _srcstride>>1;
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    __m128i r0, bshuffle1, bshuffle2, x1, x2, x3, r1;
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
    r0 = _mm_set_epi16(filter_3, filter_2, filter_1,
                       filter_0, filter_3, filter_2, filter_1, filter_0);
    bshuffle1 = _mm_set_epi8(9,8,7,6,5,4, 3, 2,7,6,5,4, 3, 2, 1, 0);

    if(!(width & 3)){
        bshuffle2 = _mm_set_epi8(13,12,11,10,9,8,7,6,11,10, 9,8,7,6,5, 4);
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 4) {

                x1 = _mm_loadu_si128((__m128i *) &src[x-1]);
                x2 = _mm_shuffle_epi8(x1, bshuffle1);
                x3 = _mm_shuffle_epi8(x1, bshuffle2);


                x2 = _mm_madd_epi16(x2, r0);
                x3 = _mm_madd_epi16(x3, r0);
                x2 = _mm_hadd_epi32(x2, x3);
                x2= _mm_srai_epi32(x2,2);   //>> (BIT_DEPTH - 8)

                x2 = _mm_packs_epi32(x2,r0);
                //give results back
                _mm_storel_epi64((__m128i *) &dst[x], x2);
            }
            src += srcstride;
            dst += dststride;
        }
    }else{
        r1= _mm_setzero_si128();
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 2) {
                /* load data in register     */
                x1 = _mm_loadu_si128((__m128i *) &src[x-1]);
                x2 = _mm_shuffle_epi8(x1, bshuffle1);

                /*  PMADDUBSW then PMADDW     */
                x2 = _mm_madd_epi16(x2, r0);
                x2 = _mm_hadd_epi32(x2, r1);
                x2= _mm_srai_epi32(x2,2);   //>> (BIT_DEPTH - 8)
                x2 = _mm_packs_epi32(x2, r1);
                /* give results back            */
                _mm_maskmoveu_si128(x2,_mm_set_epi8(0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1),(char *) (dst+x));
            }
            src += srcstride;
            dst += dststride;
        }
    }
}

void ff_hevc_put_hevc_epel_v_10_sse(int16_t *dst, ptrdiff_t dststride,
                                    uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx,
                                    int my, int16_t* mcbuffer) {
    int x, y;
    __m128i x0, x1, x2, x3, t0, t1, t2, t3, r0, f0, f1, f2, f3, r1, r2, r3;
    uint16_t *src = (uint16_t*) _src;
    ptrdiff_t srcstride = _srcstride >>1;
    const int8_t *filter = ff_hevc_epel_filters[my - 1];
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
    f0 = _mm_set1_epi16(filter_0);
    f1 = _mm_set1_epi16(filter_1);
    f2 = _mm_set1_epi16(filter_2);
    f3 = _mm_set1_epi16(filter_3);

    if(!(width & 7)){
        r1= _mm_setzero_si128();
        for (y = 0; y < height; y++) {
            for(x=0;x<width;x+=8){
                x0 = _mm_loadu_si128((__m128i *) &src[x - srcstride]);
                x1 = _mm_loadu_si128((__m128i *) &src[x]);
                x2 = _mm_loadu_si128((__m128i *) &src[x + srcstride]);
                x3 = _mm_loadu_si128((__m128i *) &src[x + 2 * srcstride]);

                // multiply by correct value :
                r0 = _mm_mullo_epi16(x0, f0);
                t0 = _mm_mulhi_epi16(x0, f0);

                x0= _mm_unpacklo_epi16(r0,t0);
                t0= _mm_unpackhi_epi16(r0,t0);

                r1 = _mm_mullo_epi16(x1, f1);
                t1 = _mm_mulhi_epi16(x1, f1);

                x1= _mm_unpacklo_epi16(r1,t1);
                t1= _mm_unpackhi_epi16(r1,t1);


                r2 = _mm_mullo_epi16(x2, f2);
                t2 = _mm_mulhi_epi16(x2, f2);

                x2= _mm_unpacklo_epi16(r2,t2);
                t2= _mm_unpackhi_epi16(r2,t2);


                r3 = _mm_mullo_epi16(x3, f3);
                t3 = _mm_mulhi_epi16(x3, f3);

                x3= _mm_unpacklo_epi16(r3,t3);
                t3= _mm_unpackhi_epi16(r3,t3);


                r0= _mm_add_epi32(x0,x1);
                r1= _mm_add_epi32(x2,x3);

                t0= _mm_add_epi32(t0,t1);
                t1= _mm_add_epi32(t2,t3);

                r0= _mm_add_epi32(r0,r1);
                t0= _mm_add_epi32(t0,t1);

                r0= _mm_srai_epi32(r0,2);//>> (BIT_DEPTH - 8)
                t0= _mm_srai_epi32(t0,2);//>> (BIT_DEPTH - 8)

                r0= _mm_packs_epi32(r0, t0);
                // give results back
                _mm_storeu_si128((__m128i *) &dst[x], r0);
            }
            src += srcstride;
            dst += dststride;
        }
    }else if(!(width & 3)){
        r1= _mm_setzero_si128();
        for (y = 0; y < height; y++) {
            for(x=0;x<width;x+=4){
                x0 = _mm_loadl_epi64((__m128i *) &src[x - srcstride]);
                x1 = _mm_loadl_epi64((__m128i *) &src[x]);
                x2 = _mm_loadl_epi64((__m128i *) &src[x + srcstride]);
                x3 = _mm_loadl_epi64((__m128i *) &src[x + 2 * srcstride]);

                /* multiply by correct value : */
                r0 = _mm_mullo_epi16(x0, f0);
                t0 = _mm_mulhi_epi16(x0, f0);

                x0= _mm_unpacklo_epi16(r0,t0);

                r1 = _mm_mullo_epi16(x1, f1);
                t1 = _mm_mulhi_epi16(x1, f1);

                x1= _mm_unpacklo_epi16(r1,t1);


                r2 = _mm_mullo_epi16(x2, f2);
                t2 = _mm_mulhi_epi16(x2, f2);

                x2= _mm_unpacklo_epi16(r2,t2);


                r3 = _mm_mullo_epi16(x3, f3);
                t3 = _mm_mulhi_epi16(x3, f3);

                x3= _mm_unpacklo_epi16(r3,t3);


                r0= _mm_add_epi32(x0,x1);
                r1= _mm_add_epi32(x2,x3);
                r0= _mm_add_epi32(r0,r1);
                r0= _mm_srai_epi32(r0,2);//>> (BIT_DEPTH - 8)

                r0= _mm_packs_epi32(r0, r0);

                // give results back
                _mm_storel_epi64((__m128i *) &dst[x], r0);
            }
            src += srcstride;
            dst += dststride;
        }
    }else{
        r1= _mm_setzero_si128();
        for (y = 0; y < height; y++) {
            for(x=0;x<width;x+=2){
                x0 = _mm_loadl_epi64((__m128i *) &src[x - srcstride]);
                x1 = _mm_loadl_epi64((__m128i *) &src[x]);
                x2 = _mm_loadl_epi64((__m128i *) &src[x + srcstride]);
                x3 = _mm_loadl_epi64((__m128i *) &src[x + 2 * srcstride]);

                /* multiply by correct value : */
                r0 = _mm_mullo_epi16(x0, f0);
                t0 = _mm_mulhi_epi16(x0, f0);

                x0= _mm_unpacklo_epi16(r0,t0);

                r1 = _mm_mullo_epi16(x1, f1);
                t1 = _mm_mulhi_epi16(x1, f1);

                x1= _mm_unpacklo_epi16(r1,t1);

                r2 = _mm_mullo_epi16(x2, f2);
                t2 = _mm_mulhi_epi16(x2, f2);

                x2= _mm_unpacklo_epi16(r2,t2);

                r3 = _mm_mullo_epi16(x3, f3);
                t3 = _mm_mulhi_epi16(x3, f3);

                x3= _mm_unpacklo_epi16(r3,t3);

                r0= _mm_add_epi32(x0,x1);
                r1= _mm_add_epi32(x2,x3);
                r0= _mm_add_epi32(r0,r1);
                r0= _mm_srai_epi32(r0,2);//>> (BIT_DEPTH - 8)

                r0= _mm_packs_epi32(r0, r0);

                /* give results back            */
                _mm_maskmoveu_si128(r0,_mm_set_epi8(0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1),(char *) (dst+x));

            }
            src += srcstride;
            dst += dststride;
        }
    }
}

void ff_hevc_put_hevc_epel_hv_10_sse(int16_t *dst, ptrdiff_t dststride,
                                     uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx,
                                     int my, int16_t* mcbuffer) {
    int x, y;
    uint16_t *src = (uint16_t*) _src;
    ptrdiff_t srcstride = _srcstride>>1;
    const int8_t *filter_h = ff_hevc_epel_filters[mx - 1];
    const int8_t *filter_v = ff_hevc_epel_filters[my - 1];
    __m128i r0, bshuffle1, bshuffle2, x0, x1, x2, x3, t0, t1, t2, t3, f0, f1,
    f2, f3, r1, r2, r3;
    int8_t filter_0 = filter_h[0];
    int8_t filter_1 = filter_h[1];
    int8_t filter_2 = filter_h[2];
    int8_t filter_3 = filter_h[3];
    int16_t *tmp = mcbuffer;

    r0 = _mm_set_epi16(filter_3, filter_2, filter_1,
                       filter_0, filter_3, filter_2, filter_1, filter_0);
    bshuffle1 = _mm_set_epi8(9,8,7,6,5,4, 3, 2,7,6,5,4, 3, 2, 1, 0);

    src -= EPEL_EXTRA_BEFORE * srcstride;

    f0 = _mm_set1_epi16(filter_v[0]);
    f1 = _mm_set1_epi16(filter_v[1]);
    f2 = _mm_set1_epi16(filter_v[2]);
    f3 = _mm_set1_epi16(filter_v[3]);


    /* horizontal treatment */
    if(!(width & 3)){
        bshuffle2 = _mm_set_epi8(13,12,11,10,9,8,7,6,11,10, 9,8,7,6,5, 4);
        for (y = 0; y < height + EPEL_EXTRA; y ++) {
            for(x=0;x<width;x+=4){

                x1 = _mm_loadu_si128((__m128i *) &src[x-1]);
                x2 = _mm_shuffle_epi8(x1, bshuffle1);
                x3 = _mm_shuffle_epi8(x1, bshuffle2);


                x2 = _mm_madd_epi16(x2, r0);
                x3 = _mm_madd_epi16(x3, r0);
                x2 = _mm_hadd_epi32(x2, x3);
                x2= _mm_srai_epi32(x2,2);   //>> (BIT_DEPTH - 8)

                x2 = _mm_packs_epi32(x2,r0);
                //give results back
                _mm_storel_epi64((__m128i *) &tmp[x], x2);

            }
            src += srcstride;
            tmp += MAX_PB_SIZE;
        }
        tmp = mcbuffer + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;

        // vertical treatment


        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 4) {
                x0 = _mm_loadl_epi64((__m128i *) &tmp[x - MAX_PB_SIZE]);
                x1 = _mm_loadl_epi64((__m128i *) &tmp[x]);
                x2 = _mm_loadl_epi64((__m128i *) &tmp[x + MAX_PB_SIZE]);
                x3 = _mm_loadl_epi64((__m128i *) &tmp[x + 2 * MAX_PB_SIZE]);

                r0 = _mm_mullo_epi16(x0, f0);
                r1 = _mm_mulhi_epi16(x0, f0);
                r2 = _mm_mullo_epi16(x1, f1);
                t0 = _mm_unpacklo_epi16(r0, r1);

                r0 = _mm_mulhi_epi16(x1, f1);
                r1 = _mm_mullo_epi16(x2, f2);
                t1 = _mm_unpacklo_epi16(r2, r0);

                r2 = _mm_mulhi_epi16(x2, f2);
                r0 = _mm_mullo_epi16(x3, f3);
                t2 = _mm_unpacklo_epi16(r1, r2);

                r1 = _mm_mulhi_epi16(x3, f3);
                t3 = _mm_unpacklo_epi16(r0, r1);



                r0 = _mm_add_epi32(t0, t1);
                r0 = _mm_add_epi32(r0, t2);
                r0 = _mm_add_epi32(r0, t3);
                r0 = _mm_srai_epi32(r0, 6);

                // give results back
                r0 = _mm_packs_epi32(r0, r0);
                _mm_storel_epi64((__m128i *) &dst[x], r0);
            }
            tmp += MAX_PB_SIZE;
            dst += dststride;
        }
    }else{
        bshuffle2=_mm_set_epi8(0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1);
        r1= _mm_setzero_si128();
        for (y = 0; y < height + EPEL_EXTRA; y ++) {
            for(x=0;x<width;x+=2){
                /* load data in register     */
                x1 = _mm_loadu_si128((__m128i *) &src[x-1]);
                x2 = _mm_shuffle_epi8(x1, bshuffle1);

                /*  PMADDUBSW then PMADDW     */
                x2 = _mm_madd_epi16(x2, r0);
                x2 = _mm_hadd_epi32(x2, r1);
                x2= _mm_srai_epi32(x2,2);   //>> (BIT_DEPTH - 8)
                x2 = _mm_packs_epi32(x2, r1);
                /* give results back            */
                _mm_maskmoveu_si128(x2,bshuffle2,(char *) (tmp+x));
            }
            src += srcstride;
            tmp += MAX_PB_SIZE;
        }

        tmp = mcbuffer + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;

        /* vertical treatment */

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 2) {
                /* check if memory needs to be reloaded */
                x0 = _mm_loadl_epi64((__m128i *) &tmp[x - MAX_PB_SIZE]);
                x1 = _mm_loadl_epi64((__m128i *) &tmp[x]);
                x2 = _mm_loadl_epi64((__m128i *) &tmp[x + MAX_PB_SIZE]);
                x3 = _mm_loadl_epi64((__m128i *) &tmp[x + 2 * MAX_PB_SIZE]);

                r0 = _mm_mullo_epi16(x0, f0);
                t0 = _mm_mulhi_epi16(x0, f0);

                x0= _mm_unpacklo_epi16(r0,t0);

                r1 = _mm_mullo_epi16(x1, f1);
                t1 = _mm_mulhi_epi16(x1, f1);

                x1= _mm_unpacklo_epi16(r1,t1);

                r2 = _mm_mullo_epi16(x2, f2);
                t2 = _mm_mulhi_epi16(x2, f2);

                x2= _mm_unpacklo_epi16(r2,t2);

                r3 = _mm_mullo_epi16(x3, f3);
                t3 = _mm_mulhi_epi16(x3, f3);

                x3= _mm_unpacklo_epi16(r3,t3);

                r0= _mm_add_epi32(x0,x1);
                r1= _mm_add_epi32(x2,x3);
                r0= _mm_add_epi32(r0,r1);
                r0 = _mm_srai_epi32(r0, 6);
                /* give results back            */
                r0 = _mm_packs_epi32(r0, r0);
                _mm_maskmoveu_si128(r0,bshuffle2,(char *) (dst+x));
            }
            tmp += MAX_PB_SIZE;
            dst += dststride;
        }
    }
}
void ff_hevc_put_hevc_qpel_pixels_10_sse(int16_t *dst, ptrdiff_t dststride,
                                         uint8_t *_src, ptrdiff_t _srcstride, int width, int height,
                                         int16_t* mcbuffer) {
    int x, y;
    __m128i x1, x2, x4;
    uint16_t *src = (uint16_t*) _src;
    ptrdiff_t srcstride = _srcstride>>1;
    if(!(width & 7)){
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 8) {

                x1 = _mm_loadu_si128((__m128i *) &src[x]);
                x2 = _mm_slli_epi16(x1, 4); //14-BIT DEPTH
                _mm_storeu_si128((__m128i *) &dst[x], x2);

            }
            src += srcstride;
            dst += dststride;
        }
    }else if(!(width & 3)){
        for (y = 0; y < height; y++) {
            for(x=0;x<width;x+=4){
                x1 = _mm_loadl_epi64((__m128i *) &src[x]);
                x2 = _mm_slli_epi16(x1, 4);//14-BIT DEPTH
                _mm_storel_epi64((__m128i *) &dst[x], x2);
            }
            src += srcstride;
            dst += dststride;
        }
    }else{
        x4= _mm_set_epi32(0,0,0,-1); //mask to store
        for (y = 0; y < height; y++) {
            for(x=0;x<width;x+=2){
                x1 = _mm_loadl_epi64((__m128i *) &src[x]);
                x2 = _mm_slli_epi16(x1, 4);//14-BIT DEPTH
                _mm_maskmoveu_si128(x2,x4,(char *) (dst+x));
            }
            src += srcstride;
            dst += dststride;
        }
    }


}

/*
 * @TODO : Valgrind to see if it's useful to use SSE or wait for AVX2 implementation
 */
void ff_hevc_put_hevc_qpel_h_1_10_sse(int16_t *dst, ptrdiff_t dststride,
                                      uint8_t *_src, ptrdiff_t _srcstride, int width, int height,
                                      int16_t* mcbuffer) {
    int x, y;
    uint16_t *src = (uint16_t*)_src;
    ptrdiff_t srcstride = _srcstride>>1;
    __m128i x0, x1, x2, x3, r0;

    r0 = _mm_set_epi16(0, 1, -5, 17, 58, -10, 4, -1);
    x0= _mm_setzero_si128();
    x3= _mm_set_epi32(0,0,0,-1);
    for (y = 0; y < height; y ++) {
        for (x = 0; x < width; x += 2){
            x1 = _mm_loadu_si128((__m128i *) &src[x-3]);
            x2 = _mm_srli_si128(x1,2); //last 16bit not used so 1 load can be used for 2 dst

            x1 = _mm_madd_epi16(x1,r0);
            x2 = _mm_madd_epi16(x2,r0);

            x1 = _mm_hadd_epi32(x1,x2);
            x1 = _mm_hadd_epi32(x1,x0);
            x1= _mm_srai_epi32(x1,2); //>>BIT_DEPTH-8
            x1= _mm_packs_epi32(x1,x0);
            //   dst[x]= _mm_extract_epi16(x1,0);
            _mm_maskmoveu_si128(x1,x3,(char *) (dst+x));
        }
        src += srcstride;
        dst += dststride;
    }
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_pixelsX_8_sse
////////////////////////////////////////////////////////////////////////////////
#define MC_PIXEL_INIT2()                                                       \
    __m128i x1, x2, r1;                                                        \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define MC_PIXEL_INIT4()                                                       \
    __m128i x1, x2, r1
#define MC_PIXEL_INIT8()                                                       \
    __m128i x1, x2, r1
#define MC_PIXEL_INIT16()                                                      \
    __m128i x1, x2, x3, r1, r2

#define MC_LOAD_PIXEL2()                                                       \
    x1 = _mm_loadl_epi64((__m128i *) &src[x])
#define MC_LOAD_PIXEL4()                                                       \
   MC_LOAD_PIXEL2()
#define MC_LOAD_PIXEL8()                                                       \
    MC_LOAD_PIXEL2()
#define MC_LOAD_PIXEL16()                                                    \
    x1 = _mm_loadu_si128((__m128i *) &src[x])

#define MC_PIXEL_COMPUTE2()                                                    \
    x2 = _mm_unpacklo_epi8(x1, c0);                                            \
    r1 = _mm_slli_epi16(x2, 14 - BIT_DEPTH)
#define MC_PIXEL_COMPUTE4()                                                    \
    MC_PIXEL_COMPUTE2()
#define MC_PIXEL_COMPUTE8()                                                    \
    MC_PIXEL_COMPUTE2()
#define MC_PIXEL_COMPUTE16()                                                   \
    MC_PIXEL_COMPUTE2();                                                       \
    x3 = _mm_unpackhi_epi8(x1, c0);                                            \
    r2 = _mm_slli_epi16(x3, 14 - BIT_DEPTH)

#define PUT_HEVC_EPEL_PIXELS(H)                                                \
void ff_hevc_put_hevc_epel_pixels ## H ## _8_sse (                             \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(uint8_t);                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    MC_PIXEL_INIT ## H();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL ## H();                                              \
            MC_PIXEL_COMPUTE ## H();                                           \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_PIXELS( 2)
PUT_HEVC_EPEL_PIXELS( 4)
PUT_HEVC_EPEL_PIXELS( 8)
PUT_HEVC_EPEL_PIXELS(16)

#define PUT_HEVC_QPEL_PIXELS(H)                                                \
void ff_hevc_put_hevc_qpel_pixels ## H ## _8_sse (                             \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height,                     \
                                    int16_t* mcbuffer) {                       \
    int x, y;                                                                  \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(uint8_t);                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    MC_PIXEL_INIT ## H();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL ## H();                                                   \
            MC_PIXEL_COMPUTE ## H();                                           \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_QPEL_PIXELS( 4)
PUT_HEVC_QPEL_PIXELS( 8)
PUT_HEVC_QPEL_PIXELS(16)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hX_8_sse
////////////////////////////////////////////////////////////////////////////////
#define EPEL_INIT_H2()                                                         \
    __m128i x1, x2, r1;                                                        \
    __m128i bshuffle1  = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    const __m128i c0   = _mm_setzero_si128();                                  \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define EPEL_INIT_H4()                                                         \
    __m128i x1, x2, r1;                                                        \
    __m128i bshuffle1  = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    const __m128i c0   = _mm_setzero_si128()
#define EPEL_INIT_H8()                                                         \
    __m128i x1, x2, x3, r1;                                                    \
    __m128i bshuffle1  = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    __m128i bshuffle2  = _mm_set_epi8(10, 9, 8, 7, 9, 8, 7, 6, 8, 7, 6, 5, 7, 6, 5, 4)

#define EPEL_FILTER_INIT_H()                                                   \
    EPEL_H_FILTER_INIT()

#define EPEL_LOAD_H()                                                          \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 1]);                             \

#define EPEL_H_COMPUTE2()                                                      \
    x2 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    MUL_ADD_H_1(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)
#define EPEL_H_COMPUTE4()                                                      \
    EPEL_H_COMPUTE2()
#define EPEL_H_COMPUTE8()                                                      \
    x2 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    x3 = _mm_shuffle_epi8(x1, bshuffle2);                                      \
    MUL_ADD_H_2_2(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)

#define PUT_HEVC_EPEL_H(H)                                                     \
void ff_hevc_put_hevc_epel_h ## H ## _8_sse (                                  \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    uint8_t      *src        = (uint8_t*) _src;                                \
    ptrdiff_t     srcstride  = _srcstride / sizeof(uint8_t);                   \
    EPEL_INIT_H ## H();                                                        \
    EPEL_FILTER_INIT_H();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_H();                                                     \
            EPEL_H_COMPUTE ## H();                                             \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_H( 2)
PUT_HEVC_EPEL_H( 4)
PUT_HEVC_EPEL_H( 8)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_vX_8_sse
////////////////////////////////////////////////////////////////////////////////
#define EPEL_INIT_V2()                                                         \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1);                           \
    __m128i x1, x2, x3, x4, r1
#define EPEL_INIT_V4()                                                         \
    __m128i x1, x2, x3, x4, r1
#define EPEL_INIT_V8()                                                         \
    __m128i x1, x2, x3, x4, r1
#define EPEL_INIT_V16()                                                        \
    __m128i x1, x2, x3, x4, r1;                                                \
    __m128i t1, t2, t3, t4, r2

#define EPEL_FILTER_INIT_V()                                                   \
    EPEL_V_FILTER_INIT(_mm_set1_epi16)

#define EPEL_LOAD_V()                                                          \
    LOAD_V_4(_mm_loadu_si128, src)

#define EPEL_V_COMPUTE2()                                                      \
    UNPACK_V_4(_mm_unpacklo_epi8, x);                                          \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r1, x)
#define EPEL_V_COMPUTE4()                                                      \
    EPEL_V_COMPUTE2()
#define EPEL_V_COMPUTE8()                                                      \
    EPEL_V_COMPUTE2()
#define EPEL_V_COMPUTE16()                                                     \
    UNPACK_V_4(_mm_unpackhi_epi8, t);                                          \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r2, t);                       \
    EPEL_V_COMPUTE2()

#define PUT_HEVC_EPEL_V(V)                                                     \
void ff_hevc_put_hevc_epel_v ## V ## _8_sse (                                  \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(uint8_t);                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    EPEL_INIT_V ## V();                                                        \
    EPEL_FILTER_INIT_V();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for(x = 0; x < width; x += V) {                                        \
            EPEL_LOAD_V();                                                     \
            EPEL_V_COMPUTE ## V();                                             \
            PEL_STORE ## V(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_V( 2)
PUT_HEVC_EPEL_V( 4)
PUT_HEVC_EPEL_V( 8)
PUT_HEVC_EPEL_V(16)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hvX_8_sse
////////////////////////////////////////////////////////////////////////////////
#define EPEL_INIT_HV2()                                                        \
    __m128i x1, x2, x3, x4, r1;                                                \
    int16_t *tmp       = mcbuffer;                                             \
    __m128i bshuffle1  = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define EPEL_INIT_HV4()                                                        \
    __m128i x1, x2, x3, x4, r1;                                                \
    int16_t *tmp       = mcbuffer;                                             \
    __m128i bshuffle1  = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define EPEL_INIT_HV8()                                                        \
    __m128i x1, x2, x3, x4,  t1, t2, t3, t4, r1, r2;                           \
    int16_t *tmp       = mcbuffer;                                             \
    __m128i bshuffle1  = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    __m128i bshuffle2  = _mm_set_epi8(10, 9, 8, 7, 9, 8, 7, 6, 8, 7, 6, 5, 7, 6, 5, 4)

#define EPEL_FILTER_INIT_HV_V2()                                                \
    EPEL_V_FILTER_INIT(_mm_set1_epi32)
#define EPEL_FILTER_INIT_HV_V4()                                                \
    EPEL_V_FILTER_INIT(_mm_set1_epi32)
#define EPEL_FILTER_INIT_HV_V8()                                                \
    EPEL_V_FILTER_INIT(_mm_set1_epi16)

#define EPEL_FILTER_INIT_HV_H()                                                \
    const int8_t *filter_h = ff_hevc_epel_filters[mx - 1];                     \
    __m128i r0 = _mm_set_epi8(filter_h[3], filter_h[2], filter_h[1], filter_h[0], \
                              filter_h[3], filter_h[2], filter_h[1], filter_h[0], \
                              filter_h[3], filter_h[2], filter_h[1], filter_h[0], \
                              filter_h[3], filter_h[2], filter_h[1], filter_h[0])

#define EPEL_LOAD_HV_V2()                                                      \
    LOAD_V_4(_mm_loadl_epi64, tmp)
#define EPEL_LOAD_HV_V4()                                                      \
    LOAD_V_4(_mm_loadl_epi64, tmp)
#define EPEL_LOAD_HV_V8()                                                      \
    LOAD_V_4(_mm_load_si128, tmp)

#define EPEL_HV_COMPUTE2()                                                     \
    UNPACK_HV_V_4();                                                           \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, 14 - BIT_DEPTH);                                   \
    r1 = _mm_packs_epi32(r1, c0)
#define EPEL_HV_COMPUTE4()                                                     \
    UNPACK_HV_V_4();                                                           \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, 14 - BIT_DEPTH);                                   \
    r1 = _mm_packs_epi32(r1, c0)
#define EPEL_HV_COMPUTE8()                                                     \
    UNPACK_HV_UNI(x1, c1, t1);                                                 \
    UNPACK_HV_UNI(x2, c2, t2);                                                 \
    UNPACK_HV_UNI(x3, c3, t3);                                                 \
    UNPACK_HV_UNI(x4, c4, t4);                                                 \
    ADD_HV_V_4(_mm_add_epi32, r1, x);                                          \
    r1 = _mm_srai_epi32(r1, 14 - BIT_DEPTH);                                   \
    ADD_HV_V_4(_mm_add_epi32, r2, t);                                          \
    r2 = _mm_srai_epi32(r2, 14 - BIT_DEPTH);                                   \
    r1 = _mm_packs_epi32(r2, r1)

#define PUT_HEVC_EPEL_HV(H)                                                    \
void ff_hevc_put_hevc_epel_hv ## H ## _8_sse (                                 \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(uint8_t);                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    EPEL_INIT_HV ## H();                                                       \
    EPEL_FILTER_INIT_HV_V ## H();                                              \
    EPEL_FILTER_INIT_HV_H();                                                   \
                                                                               \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
                                                                               \
    /* horizontal */                                                           \
    for (y = 0; y < height + EPEL_EXTRA; y ++) {                               \
        for(x = 0; x < width; x += H) {                                        \
            EPEL_LOAD_H();                                                     \
            EPEL_H_COMPUTE ## H();                                             \
            PEL_STORE ## H(tmp);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        tmp += MAX_PB_SIZE;                                                    \
    }                                                                          \
                                                                               \
    tmp       = mcbuffer + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;                    \
    srcstride = MAX_PB_SIZE;                                                   \
                                                                               \
    /* vertical */                                                             \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_HV_V ## H();                                             \
            EPEL_HV_COMPUTE ## H();                                            \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        tmp += MAX_PB_SIZE;                                                    \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_HV( 2)
PUT_HEVC_EPEL_HV( 4)
PUT_HEVC_EPEL_HV( 8)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hX_X_8_sse
////////////////////////////////////////////////////////////////////////////////
#define QPEL_INIT_H4()                                                         \
    __m128i x1, x2, x3, x4, r1
#define QPEL_INIT_H8()                                                         \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1

#define QPEL_FILTER_INIT_H1()                                                  \
   __m128i r0 = _mm_set_epi8(  0, 1, -5, 17, 58,-10,  4, -1,  0,  1, -5, 17, 58,-10,  4, -1)
#define QPEL_FILTER_INIT_H2()                                                  \
   __m128i r0 = _mm_set_epi8( -1, 4,-11, 40, 40,-11,  4, -1, -1,  4,-11, 40, 40,-11,  4, -1)
#define QPEL_FILTER_INIT_H3()                                                  \
   __m128i r0 = _mm_set_epi8( -1, 4,-10, 58, 17, -5,  1,  0, -1,  4,-10, 58, 17, -5,  1,  0)

#define QPEL_LOAD_H()                                                          \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 3])

#define QPEL_UNPACK_H4()                                                       \
    x2 = _mm_srli_si128(x1, 1);                                                \
    x3 = _mm_srli_si128(x1, 2);                                                \
    x4 = _mm_srli_si128(x1, 3);                                                \
    x2 = _mm_unpacklo_epi64(x1, x2);                                           \
    x3 = _mm_unpacklo_epi64(x3, x4)
#define QPEL_UNPACK_H8()                                                       \
    QPEL_UNPACK_H4();                                                          \
    x5 = _mm_srli_si128(x1, 4);                                                \
    x6 = _mm_srli_si128(x1, 5);                                                \
    x7 = _mm_srli_si128(x1, 6);                                                \
    x8 = _mm_srli_si128(x1, 7);                                                \
    x4 = _mm_unpacklo_epi64(x5, x6);                                           \
    x5 = _mm_unpacklo_epi64(x7, x8)

#define QPEL_MUL_ADD_H4()                                                      \
    MUL_ADD_H_2(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)
#define QPEL_MUL_ADD_H8()                                                      \
    MUL_ADD_H_4(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)

#define PUT_HEVC_QPEL_H_F(H, F)                                                \
void ff_hevc_put_hevc_qpel_h ## H ##_ ## F ## _8_sse (                         \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height,                     \
                                    int16_t* mcbuffer) {                       \
    int x, y;                                                                  \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(uint8_t);                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    QPEL_INIT_H ## H();                                                        \
    QPEL_FILTER_INIT_H ## F();                                                 \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_LOAD_H();                                                     \
            QPEL_UNPACK_H ## H();                                              \
            QPEL_MUL_ADD_H ## H();                                             \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_QPEL_H_F(4, 1)
PUT_HEVC_QPEL_H_F(4, 2)
PUT_HEVC_QPEL_H_F(4, 3)

PUT_HEVC_QPEL_H_F(8, 1)
PUT_HEVC_QPEL_H_F(8, 2)
PUT_HEVC_QPEL_H_F(8, 3)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_vX_X_8_sse
////////////////////////////////////////////////////////////////////////////////
#define QPEL_INIT_V4()                                                         \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1
#define QPEL_INIT_V16()                                                        \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1, r2;                            \
    __m128i t1, t2, t3, t4, t5, t6, t7, t8

#define QPEL_FILTER_INIT_V1_8()                                                \
    QPEL_FILTER_INIT_1(_mm_set1_epi16)
#define QPEL_FILTER_INIT_V2_8()                                                \
    QPEL_FILTER_INIT_2(_mm_set1_epi16)
#define QPEL_FILTER_INIT_V3_8()                                                \
    QPEL_FILTER_INIT_3(_mm_set1_epi16)

#define QPEL_FILTER_INIT_V1_10()                                               \
    QPEL_FILTER_INIT_1(_mm_set1_epi32)
#define QPEL_FILTER_INIT_V2_10()                                               \
    QPEL_FILTER_INIT_2(_mm_set1_epi32)
#define QPEL_FILTER_INIT_V3_10()                                               \
    QPEL_FILTER_INIT_3(_mm_set1_epi32)

#define QPEL_LOAD_V4(tab)                                                      \
    LOAD_V_8(_mm_loadl_epi64, tab)
#define QPEL_LOAD_V16(tab)                                                     \
    LOAD_V_8(_mm_loadu_si128, tab)

#define QPEL_V_COMPUTE4_8()                                                    \
    UNPACK_V_8(_mm_unpacklo_epi8, x);                                          \
    MUL_ADD_V_8(_mm_mullo_epi16, _mm_adds_epi16, r1, x)
#define QPEL_V_COMPUTE16_8()                                                   \
    UNPACK_V_8(_mm_unpackhi_epi8, t);                                          \
    MUL_ADD_V_8(_mm_mullo_epi16, _mm_adds_epi16, r2, t);                       \
    QPEL_V_COMPUTE4_8()
#define QPEL_V_COMPUTE4_10()                                                   \
    UNPACK_V_8(_mm_unpacklo_epi16, x);                                         \
    MUL_ADD_V_8(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, BIT_DEPTH - 8);                                    \
    r1 = _mm_packs_epi32(r1, c0)

#define PUT_HEVC_QPEL_V_F_DEPTH(V, F, D)                                       \
void ff_hevc_put_hevc_qpel_v ## V ##_ ## F ## _ ## D ## _sse (                 \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height,                     \
                                    int16_t* mcbuffer) {                       \
    int x, y;                                                                  \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(uint8_t);                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    QPEL_INIT_V ## V();                                                        \
    QPEL_FILTER_INIT_V ## F ## _ ## D();                                       \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_LOAD_V ## V(src);                                             \
            QPEL_V_COMPUTE ## V ## _ ## D();                                   \
            PEL_STORE ## V(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_QPEL_V_F_DEPTH( 4, 1,  8)
PUT_HEVC_QPEL_V_F_DEPTH( 4, 2,  8)
PUT_HEVC_QPEL_V_F_DEPTH( 4, 3,  8)

PUT_HEVC_QPEL_V_F_DEPTH(16, 1,  8)
PUT_HEVC_QPEL_V_F_DEPTH(16, 2,  8)
PUT_HEVC_QPEL_V_F_DEPTH(16, 3,  8)

PUT_HEVC_QPEL_V_F_DEPTH( 4, 1, 10)
PUT_HEVC_QPEL_V_F_DEPTH( 4, 2, 10)
PUT_HEVC_QPEL_V_F_DEPTH( 4, 3, 10)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hX_X_vX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define QPEL_INIT_HV4()                                                        \
    int16_t *tmp = mcbuffer;                                                   \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1
#define QPEL_INIT_HV8()                                                        \
    int16_t *tmp = mcbuffer;                                                   \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1, r2;                            \
    __m128i t1, t2, t3, t4, t5, t6, t7, t8;                                    \
    const __m128i mask = _mm_set_epi16(0, -1, 0, -1, 0, -1, 0, -1)

#define QPEL_FILTER_INIT_HV4_V1()                                              \
    QPEL_FILTER_INIT_1(_mm_set1_epi32)
#define QPEL_FILTER_INIT_HV4_V2()                                              \
    QPEL_FILTER_INIT_2(_mm_set1_epi32)
#define QPEL_FILTER_INIT_HV4_V3()                                              \
    QPEL_FILTER_INIT_3(_mm_set1_epi32)

#define QPEL_FILTER_INIT_HV8_V1()                                              \
    QPEL_FILTER_INIT_1(_mm_set1_epi16)
#define QPEL_FILTER_INIT_HV8_V2()                                              \
    QPEL_FILTER_INIT_2(_mm_set1_epi16)
#define QPEL_FILTER_INIT_HV8_V3()                                              \
    QPEL_FILTER_INIT_3(_mm_set1_epi16)

#define QPEL_LOAD_V8(tab)                                                      \
    LOAD_V_8(_mm_load_si128, tab)

#define QPEL_UNPACK_HV_V4()                                                    \
    UNPACK_HV_V_8()
#define QPEL_UNPACK_HV_V8()                                                    \
    UNPACK_HV_UNI(x1, c1, t1);                                                 \
    UNPACK_HV_UNI(x2, c2, t2);                                                 \
    UNPACK_HV_UNI(x3, c3, t3);                                                 \
    UNPACK_HV_UNI(x4, c4, t4);                                                 \
    UNPACK_HV_UNI(x5, c5, t5);                                                 \
    UNPACK_HV_UNI(x6, c6, t6);                                                 \
    UNPACK_HV_UNI(x7, c7, t7);                                                 \
    UNPACK_HV_UNI(x8, c8, t8)

#define QPEL_MUL_ADD_HV_H4()                                                   \
    MUL_ADD_H_2(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x);                     \
    r1 = _mm_srli_epi16(r1, BIT_DEPTH - 8)
#define QPEL_MUL_ADD_HV_H8()                                                   \
    MUL_ADD_H_4(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x);                     \
    r1 = _mm_srli_si128(r1, BIT_DEPTH - 8)

#define AND_ADD_HV_V8(dst, src)                                                \
    ADD_HV_V_8(_mm_add_epi32, dst, src);                                       \
    dst = _mm_srli_epi32(dst, 14- BIT_DEPTH);                                  \
    dst = _mm_and_si128(dst, mask);                                            \

#define QPEL_MUL_ADD_HV_V4()                                                   \
    MUL_ADD_V_8(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1,6);                                                 \
    r1 = _mm_packs_epi32(r1,c0)
#define QPEL_MUL_ADD_HV_V8()                                                   \
    AND_ADD_HV_V8(r1, x);                                                       \
    AND_ADD_HV_V8(r0, t);                                                       \
    r1 = _mm_hadd_epi16(r0, r1)

#define PUT_HEVC_QPEL_HV_F(H, FH, FV)                                          \
void ff_hevc_put_hevc_qpel_h ## H ##_ ## FH ## _v ## _ ## FV ## _sse (         \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height,                     \
                                    int16_t* mcbuffer) {                       \
    int x, y;                                                                  \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(uint8_t);                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    QPEL_INIT_HV ## H();                                                       \
    QPEL_FILTER_INIT_H ## FH();                                                \
    QPEL_FILTER_INIT_HV ## H ##_V ## FV();                                     \
                                                                               \
    src -= ff_hevc_qpel_extra_before[FV] * srcstride;                          \
                                                                               \
    /* horizontal */                                                           \
    for (y = 0; y < height + ff_hevc_qpel_extra[FV] ; y++) {                   \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_LOAD_H();                                                     \
            QPEL_UNPACK_H ## H();                                              \
            QPEL_MUL_ADD_HV_H ## H();                                          \
            PEL_STORE ## H(tmp);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        tmp += MAX_PB_SIZE;                                                    \
    }                                                                          \
                                                                               \
    tmp       = mcbuffer + ff_hevc_qpel_extra_before[FV] * MAX_PB_SIZE;        \
    srcstride = MAX_PB_SIZE;                                                   \
                                                                               \
    /* vertical treatment on temp table : tmp contains 16 bit values, */       \
    /* so need to use 32 bit  integers for register calculations      */       \
                                                                               \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_LOAD_V ## H(tmp);                                             \
            QPEL_UNPACK_HV_V ## H();                                           \
            QPEL_MUL_ADD_HV_V ## H();                                          \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        tmp += MAX_PB_SIZE;                                                    \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_QPEL_HV_F(4, 1, 1)
PUT_HEVC_QPEL_HV_F(4, 1, 2)
PUT_HEVC_QPEL_HV_F(4, 1, 3)
PUT_HEVC_QPEL_HV_F(4, 2, 1)
PUT_HEVC_QPEL_HV_F(4, 2, 2)
PUT_HEVC_QPEL_HV_F(4, 2, 3)
PUT_HEVC_QPEL_HV_F(4, 3, 1)
PUT_HEVC_QPEL_HV_F(4, 3, 2)
PUT_HEVC_QPEL_HV_F(4, 3, 3)

PUT_HEVC_QPEL_HV_F(8, 1, 1)
PUT_HEVC_QPEL_HV_F(8, 1, 2)
PUT_HEVC_QPEL_HV_F(8, 1, 3)
PUT_HEVC_QPEL_HV_F(8, 2, 1)
PUT_HEVC_QPEL_HV_F(8, 2, 2)
PUT_HEVC_QPEL_HV_F(8, 2, 3)
PUT_HEVC_QPEL_HV_F(8, 3, 1)
PUT_HEVC_QPEL_HV_F(8, 3, 2)
PUT_HEVC_QPEL_HV_F(8, 3, 3)

