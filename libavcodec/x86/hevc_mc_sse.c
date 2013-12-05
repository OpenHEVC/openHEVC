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

#define EPEL_FILTER_INIT_H_8()                                                 \
    __m128i bshuffle1 = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    __m128i bshuffle2 = _mm_set_epi8(10, 9, 8, 7, 9, 8, 7, 6, 8, 7, 6, 5, 7, 6, 5, 4); \
    __m128i r0        = _mm_loadl_epi64((__m128i *) &ff_hevc_epel_filters[mx - 1]);    \
    r0 = _mm_shuffle_epi32(r0, 0)

#define EPEL_FILTER_INIT_H_10()                                                \
    __m128i bshuffle1 = _mm_set_epi8( 9, 8, 7, 6, 5, 4, 3, 2, 7, 6, 5, 4, 3, 2, 1, 0); \
    __m128i bshuffle2 = _mm_set_epi8(13,12,11,10, 9, 8, 7, 6,11,10, 9, 8, 7, 6, 5, 4); \
    __m128i r0        = _mm_loadu_si128((__m128i *) &ff_hevc_epel_filters[mx - 1]);    \
    r0 = _mm_srai_epi16(_mm_unpacklo_epi8(c0, r0), 8);                         \
    r0 = _mm_unpacklo_epi64(r0, r0)

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

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define INST_SRC1_CST_4(inst, dst, src, cst)                                   \
    dst ## 1 = inst(src ## 1, cst);                                            \
    dst ## 2 = inst(src ## 2, cst);                                            \
    dst ## 3 = inst(src ## 3, cst);                                            \
    dst ## 4 = inst(src ## 4, cst)
#define INST_SRC1_CST_8(inst, dst, src, cst)                                   \
    dst ## 1 = inst(src ## 1, cst);                                            \
    dst ## 2 = inst(src ## 2, cst);                                            \
    dst ## 3 = inst(src ## 3, cst);                                            \
    dst ## 4 = inst(src ## 4, cst);                                            \
    dst ## 5 = inst(src ## 5, cst);                                            \
    dst ## 6 = inst(src ## 6, cst);                                            \
    dst ## 7 = inst(src ## 7, cst);                                            \
    dst ## 8 = inst(src ## 8, cst)
#define INST_SRC1_SRC2_4(inst, dst, src1, src2)                                \
    dst ## 1 = inst(src1 ## 1, src2 ## 1);                                     \
    dst ## 2 = inst(src1 ## 2, src2 ## 2);                                     \
    dst ## 3 = inst(src1 ## 3, src2 ## 3);                                     \
    dst ## 4 = inst(src1 ## 4, src2 ## 4)
#define INST_SRC1_SRC2_8(inst, dst, src1, src2)                                \
    dst ## 1 = inst(src1 ## 1, src2 ## 1);                                     \
    dst ## 2 = inst(src1 ## 2, src2 ## 2);                                     \
    dst ## 3 = inst(src1 ## 3, src2 ## 3);                                     \
    dst ## 4 = inst(src1 ## 4, src2 ## 4);                                     \
    dst ## 5 = inst(src1 ## 5, src2 ## 5);                                     \
    dst ## 6 = inst(src1 ## 6, src2 ## 6);                                     \
    dst ## 7 = inst(src1 ## 7, src2 ## 7);                                     \
    dst ## 8 = inst(src1 ## 8, src2 ## 8)

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

#define ADD_V_4(add, dst, src)                                                 \
    dst = add(src ## 1, src ## 2);                                             \
    dst = add(dst, add(src ## 3, src ## 4))
#define ADD_V_8(add, dst, src)                                                 \
    dst = add(src ## 1, src ## 2);                                             \
    dst = add(dst, add(src ## 3, src ## 4));                                   \
    dst = add(dst, add(src ## 5, src ## 6));                                   \
    dst = add(dst, add(src ## 7, src ## 8))

#define AND_ADD_V8(dst, src)                                                   \
    ADD_V_8(_mm_add_epi32, dst, src);                                          \
    dst = _mm_srli_epi32(dst, 14- BIT_DEPTH);                                  \
    dst = _mm_and_si128(dst, mask);                                            \

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
    t1   = _mm_mullo_epi16(reg1, c1);                                          \
    t2   = _mm_mulhi_epi16(reg1, c1);                                          \
    reg2 = _mm_unpackhi_epi16(t1, t2);                                         \
    reg1 = _mm_unpacklo_epi16(t1, t2);                                         \
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
    __m128i t1, t2;                                                            \
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
    t1   = _mm_mullo_epi16(reg1, c1);                                          \
    t2   = _mm_mulhi_epi16(reg1, c1);                                          \
    t3   = _mm_mullo_epi16(reg2, c2);                                          \
    t4   = _mm_mulhi_epi16(reg2, c2);                                          \
    t5   = _mm_unpacklo_epi16(t1, t2);                                         \
    t6   = _mm_unpacklo_epi16(t3, t4);                                         \
    reg1 = _mm_unpackhi_epi16(t1, t2);                                         \
    reg2 = _mm_unpackhi_epi16(t3, t4);                                         \
    reg1 = _mm_add_epi32(reg1, reg2);                                          \
    reg2 = _mm_add_epi32(t5, t6);                                              \
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
    __m128i t1, t2, t3, t4, t5, t6;                                            \
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
// ff_hevc_put_hevc_mc_pixelsX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define MC_PIXEL_INIT_8()                                                      \
    uint8_t  *src       = (uint8_t*) _src;                                     \
    ptrdiff_t srcstride = _srcstride
#define MC_PIXEL_INIT_10()                                                     \
    uint16_t *src       = (uint16_t*) _src;                                    \
    ptrdiff_t srcstride = _srcstride >> 1

#define MC_LOAD_PIXEL()                                                        \
    x1 = _mm_loadu_si128((__m128i *) &src[x])

#define MC_PIXEL_COMPUTE2_8()                                                  \
    x2 = _mm_unpacklo_epi8(x1, c0);                                            \
    r1 = _mm_slli_epi16(x2, shift)
#define MC_PIXEL_COMPUTE4_8()                                                  \
    MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE8_8()                                                  \
    MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE16_8()                                                 \
    MC_PIXEL_COMPUTE2_8();                                                     \
    x3 = _mm_unpackhi_epi8(x1, c0);                                            \
    r2 = _mm_slli_epi16(x3, shift)

#define MC_PIXEL_COMPUTE2_10()                                                 \
    r1 = _mm_slli_epi16(x1, shift)
#define MC_PIXEL_COMPUTE4_10()                                                 \
    MC_PIXEL_COMPUTE2_10()
#define MC_PIXEL_COMPUTE8_10()                                                 \
    MC_PIXEL_COMPUTE2_10()

#define PUT_HEVC_EPEL_PIXELS(H, D)                                             \
void ff_hevc_put_hevc_epel_pixels ## H ## _ ## D ## _sse (                     \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    int shift = 14 - D;                                                        \
    __m128i x1, x2, x3, r1, r2;                                                \
    const __m128i c0    = _mm_setzero_si128();                                 \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    MC_PIXEL_INIT_ ## D();                                                     \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_PIXELS( 2,  8)
PUT_HEVC_EPEL_PIXELS( 4,  8)
PUT_HEVC_EPEL_PIXELS( 8,  8)
PUT_HEVC_EPEL_PIXELS(16,  8)

PUT_HEVC_EPEL_PIXELS( 2, 10)
PUT_HEVC_EPEL_PIXELS( 4, 10)
PUT_HEVC_EPEL_PIXELS( 8, 10)

#define PUT_HEVC_QPEL_PIXELS(H, D)                                             \
void ff_hevc_put_hevc_qpel_pixels ## H  ## _ ## D ## _sse (                    \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height,                     \
                                    int16_t* mcbuffer) {                       \
    int x, y;                                                                  \
    int shift = 14 - D;                                                        \
    __m128i x1, x2, x3, r1, r2;                                                \
    const __m128i c0    = _mm_setzero_si128();                                 \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    MC_PIXEL_INIT_ ## D();                                                     \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_QPEL_PIXELS( 4,  8)
PUT_HEVC_QPEL_PIXELS( 8,  8)
PUT_HEVC_QPEL_PIXELS(16,  8)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define EPEL_INIT_8()                                                          \
    uint8_t  *src        = (uint8_t*) _src;                                    \
    ptrdiff_t srcstride  = _srcstride
#define EPEL_INIT_10()                                                         \
    uint16_t *src        = (uint16_t*) _src;                                   \
    ptrdiff_t srcstride  = _srcstride >> 1

#define EPEL_LOAD_H()                                                          \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 1])

#define EPEL_H_COMPUTE2_8()                                                    \
    x2 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    MUL_ADD_H_1(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)
#define EPEL_H_COMPUTE4_8()                                                    \
    EPEL_H_COMPUTE2_8()
#define EPEL_H_COMPUTE8_8()                                                    \
    x2 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    x3 = _mm_shuffle_epi8(x1, bshuffle2);                                      \
    MUL_ADD_H_2_2(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)

#define EPEL_H_COMPUTE2_10()                                                   \
    x2 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    MUL_ADD_H_1(_mm_madd_epi16, _mm_hadd_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, 10 - 8);                                           \
    r1 = _mm_packs_epi32(r1, c0)
#define EPEL_H_COMPUTE4_10()                                                   \
    x2 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    x3 = _mm_shuffle_epi8(x1, bshuffle2);                                      \
    MUL_ADD_H_2_2(_mm_madd_epi16, _mm_hadd_epi32, r1, x);                      \
    r1 = _mm_srai_epi32(r1, 10 - 8);                                           \
    r1 = _mm_packs_epi32(r1, c0)

#define PUT_HEVC_EPEL_H(H, D)                                                  \
void ff_hevc_put_hevc_epel_h ## H ## _ ## D ## _sse (                          \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    __m128i x1, x2, x3, r1;                                                    \
    const __m128i c0     = _mm_setzero_si128();                                \
    const __m128i mask   = _mm_set_epi32(0, 0, 0, -1);                         \
    EPEL_INIT_ ## D();                                                         \
    EPEL_FILTER_INIT_H_ ## D();                                                \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_LOAD_H();                                                     \
            EPEL_H_COMPUTE ## H ## _ ## D();                                   \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_H( 2,  8)
PUT_HEVC_EPEL_H( 4,  8)
PUT_HEVC_EPEL_H( 8,  8)

PUT_HEVC_EPEL_H( 2, 10)
PUT_HEVC_EPEL_H( 4, 10)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_vX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define EPEL_FILTER_INIT_V()                                                   \
    EPEL_V_FILTER_INIT(_mm_set1_epi16)

#define EPEL_LOAD_V()                                                          \
    LOAD_V_4(_mm_loadu_si128, src)

#define EPEL_V_COMPUTE2_8()                                                    \
    INST_SRC1_CST_4(_mm_unpacklo_epi8, x, x , c0);                             \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r1, x)
#define EPEL_V_COMPUTE4_8()                                                    \
    EPEL_V_COMPUTE2_8()
#define EPEL_V_COMPUTE8_8()                                                    \
    EPEL_V_COMPUTE2_8()
#define EPEL_V_COMPUTE16_8()                                                   \
    INST_SRC1_CST_4(_mm_unpackhi_epi8, t, x , c0);                             \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r2, t);                       \
    EPEL_V_COMPUTE2_8()

#define EPEL_V_COMPUTE2_10()                                                   \
    INST_SRC1_SRC2_4(_mm_mullo_epi16, r, x, c);                                \
    INST_SRC1_SRC2_4(_mm_mulhi_epi16, x, x, c);                                \
    INST_SRC1_SRC2_4(_mm_unpacklo_epi16, x, r, x);                             \
    ADD_V_4(_mm_add_epi32, r1, x);                                             \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    r1 = _mm_packs_epi32(r1, r1)
#define EPEL_V_COMPUTE4_10()                                                   \
    EPEL_V_COMPUTE2_10()
#define EPEL_V_COMPUTE8_10()                                                   \
    INST_SRC1_SRC2_4(_mm_mullo_epi16, r, x, c);                                \
    INST_SRC1_SRC2_4(_mm_mulhi_epi16, t, x, c);                                \
    INST_SRC1_SRC2_4(_mm_unpacklo_epi16, x, r, t);                             \
    INST_SRC1_SRC2_4(_mm_unpackhi_epi16, t, r, t);                             \
    ADD_V_4(_mm_add_epi32, r1, x);                                             \
    ADD_V_4(_mm_add_epi32, t1, t);                                             \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    t1 = _mm_srai_epi32(t1, shift);                                            \
    r1 = _mm_packs_epi32(r1, t1)

#define PUT_HEVC_EPEL_V(V, D)                                                  \
void ff_hevc_put_hevc_epel_v ## V ## _ ## D ## _sse (                          \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    __m128i x1, x2, x3, x4;                                                    \
    __m128i t1, t2, t3, t4;                                                    \
    __m128i r1, r2, r3, r4;                                                    \
    const __m128i c0    = _mm_setzero_si128();                                 \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    EPEL_INIT_ ## D();                                                         \
    EPEL_FILTER_INIT_V();                                                      \
    for (y = 0; y < height; y++) {                                             \
        for(x = 0; x < width; x += V) {                                        \
            EPEL_LOAD_V();                                                     \
            EPEL_V_COMPUTE ## V ## _ ## D();                                   \
            PEL_STORE ## V(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_V( 2,  8)
PUT_HEVC_EPEL_V( 4,  8)
PUT_HEVC_EPEL_V( 8,  8)
PUT_HEVC_EPEL_V(16,  8)

PUT_HEVC_EPEL_V( 2, 10)
PUT_HEVC_EPEL_V( 4, 10)
PUT_HEVC_EPEL_V( 8, 10)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hvX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define EPEL_INIT_HV2_8()                                                      \
    EPEL_INIT_8();                                                             \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define EPEL_INIT_HV4_8()                                                      \
    EPEL_INIT_8();                                                             \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define EPEL_INIT_HV8_8()                                                      \
    EPEL_INIT_8()

#define EPEL_INIT_HV2_10()                                                     \
    EPEL_INIT_10();                                                            \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define EPEL_INIT_HV4_10()                                                     \
    EPEL_INIT_10();                                                            \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define EPEL_INIT_HV8_10()                                                     \
    EPEL_INIT_10()

#define EPEL_FILTER_INIT_HV_V2_8()                                             \
    EPEL_V_FILTER_INIT(_mm_set1_epi32)
#define EPEL_FILTER_INIT_HV_V4_8()                                             \
    EPEL_V_FILTER_INIT(_mm_set1_epi32)
#define EPEL_FILTER_INIT_HV_V8_8()                                             \
    EPEL_V_FILTER_INIT(_mm_set1_epi16)
#define EPEL_FILTER_INIT_HV_V2_10()                                            \
    EPEL_V_FILTER_INIT(_mm_set1_epi16)
#define EPEL_FILTER_INIT_HV_V4_10()                                            \
    EPEL_V_FILTER_INIT(_mm_set1_epi16)

#define EPEL_LOAD_HV_V2()                                                      \
    LOAD_V_4(_mm_loadl_epi64, tmp)
#define EPEL_LOAD_HV_V4()                                                      \
    LOAD_V_4(_mm_loadl_epi64, tmp)
#define EPEL_LOAD_HV_V8()                                                      \
    LOAD_V_4(_mm_load_si128, tmp)

#define EPEL_HV_COMPUTE2_8()                                                   \
    UNPACK_HV_V_4();                                                           \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    r1 = _mm_packs_epi32(r1, c0)
#define EPEL_HV_COMPUTE4_8()                                                   \
    EPEL_HV_COMPUTE2_8()
#define EPEL_HV_COMPUTE8_8()                                                   \
    EPEL_V_COMPUTE8_10()

#define EPEL_HV_COMPUTE2_10()                                                  \
    EPEL_V_COMPUTE2_10()
#define EPEL_HV_COMPUTE4_10()                                                  \
    EPEL_V_COMPUTE2_10()

#define PUT_HEVC_EPEL_HV(H, D)                                                 \
void ff_hevc_put_hevc_epel_hv ## H ## _ ## D ## _sse (                         \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my, int16_t* mcbuffer) {        \
    int x, y;                                                                  \
    int shift = 6;                                                             \
    __m128i x1, x2, x3, x4,  t1, t2, t3, t4, r1, r2,r3, r4;                    \
    const __m128i c0   = _mm_setzero_si128();                                  \
    int16_t *tmp       = mcbuffer;                                             \
    EPEL_INIT_HV ## H ## _ ## D();                                             \
    EPEL_FILTER_INIT_HV_V ## H ## _ ## D();                                    \
    EPEL_FILTER_INIT_H_ ## D();                                                \
                                                                               \
    src -= EPEL_EXTRA_BEFORE * srcstride;                                      \
                                                                               \
    /* horizontal */                                                           \
    for (y = 0; y < height + EPEL_EXTRA; y ++) {                               \
        for(x = 0; x < width; x += H) {                                        \
            EPEL_LOAD_H();                                                     \
            EPEL_H_COMPUTE ## H ## _ ## D();                                   \
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
            EPEL_HV_COMPUTE ## H  ## _ ## D();                                 \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        tmp += MAX_PB_SIZE;                                                    \
        dst += dststride;                                                      \
    }                                                                          \
}

PUT_HEVC_EPEL_HV( 2,  8)
PUT_HEVC_EPEL_HV( 4,  8)
PUT_HEVC_EPEL_HV( 8,  8)

PUT_HEVC_EPEL_HV( 2, 10)
PUT_HEVC_EPEL_HV( 4, 10)

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
    INST_SRC1_CST_8(_mm_unpacklo_epi8, x, x , c0);                             \
    MUL_ADD_V_8(_mm_mullo_epi16, _mm_adds_epi16, r1, x)
#define QPEL_V_COMPUTE16_8()                                                   \
    INST_SRC1_CST_8(_mm_unpackhi_epi8, t, x , c0);                             \
    MUL_ADD_V_8(_mm_mullo_epi16, _mm_adds_epi16, r2, t);                       \
    QPEL_V_COMPUTE4_8()
#define QPEL_V_COMPUTE4_10()                                                   \
    INST_SRC1_CST_8(_mm_unpacklo_epi16, x, x , c0);                            \
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
    __m128i x1, x2, x3, x4, x5, x6, x7, x8;                                    \
    __m128i r1, r2, r3, r4, r5, r6, r7, r8;                                    \
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

#define QPEL_MUL_ADD_HV_H4()                                                   \
    MUL_ADD_H_2(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x);                     \
    r1 = _mm_srli_epi16(r1, BIT_DEPTH - 8)
#define QPEL_MUL_ADD_HV_H8()                                                   \
    MUL_ADD_H_4(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x);                     \
    r1 = _mm_srli_si128(r1, BIT_DEPTH - 8)

#define QPEL_HV_V_COMPUTE4()                                                   \
    UNPACK_HV_V_8();                                                           \
    MUL_ADD_V_8(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1,6);                                                 \
    r1 = _mm_packs_epi32(r1,c0)
#define QPEL_HV_V_COMPUTE8()                                                   \
    INST_SRC1_SRC2_8(_mm_mullo_epi16, r, x, c);                                \
    INST_SRC1_SRC2_8(_mm_mulhi_epi16, t, x, c);                                \
    INST_SRC1_SRC2_8(_mm_unpacklo_epi16, x, r, t);                             \
    INST_SRC1_SRC2_8(_mm_unpackhi_epi16, t, r, t);                             \
    AND_ADD_V8(r1, x);                                                         \
    AND_ADD_V8(t1, t);                                                         \
    r1 = _mm_hadd_epi16(r1, t1)

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
            QPEL_HV_V_COMPUTE ## H();                                          \
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

