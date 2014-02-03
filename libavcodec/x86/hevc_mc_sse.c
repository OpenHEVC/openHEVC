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
#define EPEL_V_FILTER(set)                                                     \
    const int8_t *filter_v = ff_hevc_epel_filters[my - 1];                     \
    const __m128i c1 = set(filter_v[0]);                                       \
    const __m128i c2 = set(filter_v[1]);                                       \
    const __m128i c3 = set(filter_v[2]);                                       \
    const __m128i c4 = set(filter_v[3])

#define EPEL_V_FILTER_8()                                                      \
    EPEL_V_FILTER(_mm_set1_epi16)
#define EPEL_V_FILTER_10()                                                     \
    EPEL_V_FILTER(_mm_set1_epi32)
#define EPEL_V_FILTER_14()  EPEL_V_FILTER_10()                                                    

#define EPEL_H_FILTER_8()                                                      \
    __m128i bshuffle1 = _mm_set_epi8( 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0); \
    __m128i r0        = _mm_loadl_epi64((__m128i *) &ff_hevc_epel_filters[mx - 1]);    \
    r0 = _mm_shuffle_epi32(r0, 0)
#define EPEL_H_FILTER_10()                                                     \
    __m128i bshuffle1 = _mm_set_epi8( 9, 8, 7, 6, 5, 4, 3, 2, 7, 6, 5, 4, 3, 2, 1, 0); \
    __m128i bshuffle2 = _mm_set_epi8(13,12,11,10, 9, 8, 7, 6,11,10, 9, 8, 7, 6, 5, 4); \
    __m128i r0        = _mm_loadu_si128((__m128i *) &ff_hevc_epel_filters[mx - 1]);    \
    r0 = _mm_srai_epi16(_mm_unpacklo_epi8(c0, r0), 8);                         \
    r0 = _mm_unpacklo_epi64(r0, r0)

#define QPEL_V_FILTER_1(inst)                                                  \
    const __m128i c1 = inst( -1);                                              \
    const __m128i c2 = inst(  4);                                              \
    const __m128i c3 = inst(-10);                                              \
    const __m128i c4 = inst( 58);                                              \
    const __m128i c5 = inst( 17);                                              \
    const __m128i c6 = inst( -5);                                              \
    const __m128i c7 = inst(  1);                                              \
    const __m128i c8 = _mm_setzero_si128()
#define QPEL_V_FILTER_2(inst)                                                  \
    const __m128i c1 = inst( -1);                                              \
    const __m128i c2 = inst(  4);                                              \
    const __m128i c3 = inst(-11);                                              \
    const __m128i c4 = inst( 40);                                              \
    const __m128i c5 = inst( 40);                                              \
    const __m128i c6 = inst(-11);                                              \
    const __m128i c7 = inst(  4);                                              \
    const __m128i c8 = inst( -1)
#define QPEL_V_FILTER_3(inst)                                                  \
    const __m128i c1 = _mm_setzero_si128();                                    \
    const __m128i c2 = inst(  1);                                              \
    const __m128i c3 = inst( -5);                                              \
    const __m128i c4 = inst( 17);                                              \
    const __m128i c5 = inst( 58);                                              \
    const __m128i c6 = inst(-10);                                              \
    const __m128i c7 = inst(  4);                                              \
    const __m128i c8 = inst( -1)

#define QPEL_V_FILTER_1_8()                                                    \
    QPEL_V_FILTER_1(_mm_set1_epi16)
#define QPEL_V_FILTER_2_8()                                                    \
    QPEL_V_FILTER_2(_mm_set1_epi16)
#define QPEL_V_FILTER_3_8()                                                    \
    QPEL_V_FILTER_3(_mm_set1_epi16)
#define QPEL_V_FILTER_1_10()                                                   \
    QPEL_V_FILTER_1(_mm_set1_epi32)
#define QPEL_V_FILTER_2_10()                                                   \
    QPEL_V_FILTER_2(_mm_set1_epi32)
#define QPEL_V_FILTER_3_10()                                                   \
    QPEL_V_FILTER_3(_mm_set1_epi32)

#define QPEL_V_FILTER_1_14() QPEL_V_FILTER_1_10()
#define QPEL_V_FILTER_2_14() QPEL_V_FILTER_2_10()
#define QPEL_V_FILTER_3_14() QPEL_V_FILTER_3_10()


#define QPEL_H_FILTER_1_8()                                                    \
    __m128i bshuffle1 = _mm_set_epi8( 8, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0); \
    __m128i r0 = _mm_set_epi8(  0, 1, -5, 17, 58,-10,  4, -1,  0,  1, -5, 17, 58,-10,  4, -1)
#define QPEL_H_FILTER_2_8()                                                    \
    __m128i bshuffle1 = _mm_set_epi8( 8, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0); \
    __m128i r0 = _mm_set_epi8( -1, 4,-11, 40, 40,-11,  4, -1, -1,  4,-11, 40, 40,-11,  4, -1)
#define QPEL_H_FILTER_3_8()                                                    \
    __m128i bshuffle1 = _mm_set_epi8( 8, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0); \
    __m128i r0 = _mm_set_epi8( -1, 4,-10, 58, 17, -5,  1,  0, -1,  4,-10, 58, 17, -5,  1,  0)

#define QPEL_H_FILTER_1_10()                                                   \
    __m128i r0 = _mm_set_epi16(  0, 1, -5, 17, 58,-10,  4, -1)
#define QPEL_H_FILTER_2_10()                                                   \
    __m128i r0 = _mm_set_epi16( -1, 4,-11, 40, 40,-11,  4, -1)
#define QPEL_H_FILTER_3_10()                                                   \
    __m128i r0 = _mm_set_epi16( -1, 4,-10, 58, 17, -5,  1,  0)

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
#define SRC_INIT1_14() SRC_INIT1_10()

#define DST_INIT_8()                                                           \
    uint8_t  *dst       = (uint8_t*) _dst;                                     \
    ptrdiff_t dststride = _dststride
#define DST_INIT_10()                                                          \
    uint16_t *dst       = (uint16_t*) _dst;                                    \
    ptrdiff_t dststride = _dststride >> 1
#define DST_INIT_14() DST_INIT_10()

#define MC_LOAD_PIXEL()                                                        \
    x1 = _mm_loadu_si128((__m128i *) &src[x])

#define EPEL_H_LOAD2()                                                         \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 1])
#define EPEL_H_LOAD4()                                                         \
    EPEL_H_LOAD2()
#define EPEL_H_LOAD8()                                                         \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 1]);                             \
    x2 = _mm_srli_si128(x1, 4)

#define EPEL_V_LOAD(tab)                                                       \
    x1 = _mm_loadu_si128((__m128i *) &tab[x -     srcstride]);                 \
    x2 = _mm_loadu_si128((__m128i *) &tab[x                ]);                 \
    x3 = _mm_loadu_si128((__m128i *) &tab[x +     srcstride]);                 \
    x4 = _mm_loadu_si128((__m128i *) &tab[x + 2 * srcstride])

#define EPEL_V_LOAD3(tab)                                                       \
    x2 = _mm_loadu_si128((__m128i *) &tab[x -     srcstride]);                 \
    x3 = _mm_loadu_si128((__m128i *) &tab[x                ]);                 \
    x4 = _mm_loadu_si128((__m128i *) &tab[x +     srcstride])

#define QPEL_H_LOAD2()                                                         \
    x1 = _mm_loadu_si128((__m128i *) &src[x - 3]);                             \
    x2 = _mm_srli_si128(x1, 2)
#define QPEL_H_LOAD4()                                                         \
    QPEL_H_LOAD2()
#define QPEL_H_LOAD8()                                                         \
    QPEL_H_LOAD4();                                                            \
    x3 = _mm_srli_si128(x1, 4);                                                \
    x4 = _mm_srli_si128(x1, 6)

#define QPEL_V_LOAD(tab)                                                       \
    x1 = _mm_loadu_si128((__m128i *) &tab[x - 3 * srcstride]);                 \
    x2 = _mm_loadu_si128((__m128i *) &tab[x - 2 * srcstride]);                 \
    x3 = _mm_loadu_si128((__m128i *) &tab[x -     srcstride]);                 \
    x4 = _mm_loadu_si128((__m128i *) &tab[x                ]);                 \
    x5 = _mm_loadu_si128((__m128i *) &tab[x +     srcstride]);                 \
    x6 = _mm_loadu_si128((__m128i *) &tab[x + 2 * srcstride]);                 \
    x7 = _mm_loadu_si128((__m128i *) &tab[x + 3 * srcstride]);                 \
    x8 = _mm_loadu_si128((__m128i *) &tab[x + 4 * srcstride])

#define QPEL_V_LOAD_LO(tab)                                                       \
    x1 = _mm_loadu_si128((__m128i *) &tab[x - 3 * srcstride]);                 \
    x2 = _mm_loadu_si128((__m128i *) &tab[x - 2 * srcstride]);                 \
    x3 = _mm_loadu_si128((__m128i *) &tab[x -     srcstride]);                 \
    x4 = _mm_loadu_si128((__m128i *) &tab[x                ])

#define QPEL_V_LOAD_HI(tab)                                                       \
    x1 = _mm_loadu_si128((__m128i *) &tab[x +     srcstride]);                 \
    x2 = _mm_loadu_si128((__m128i *) &tab[x + 2 * srcstride]);                 \
    x3 = _mm_loadu_si128((__m128i *) &tab[x + 3 * srcstride]);                 \
    x4 = _mm_loadu_si128((__m128i *) &tab[x + 4 * srcstride])

#define PEL_STORE2(tab)                                                        \
    *((uint32_t *) &tab[x]) = _mm_cvtsi128_si32(r1)
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
    src ## 1 = mul(src ## 1, r0);                                              \
    dst      = add(src ## 1, c0)

#define MUL_ADD_H_2_2(mul, add, dst, src)                                      \
    src ## 1 = mul(src ## 1, r0);                                              \
    src ## 2 = mul(src ## 2, r0);                                              \
    dst      = add(src ## 1, src ## 2)

#define MUL_ADD_H_2(mul, add, dst, src)                                        \
    MUL_ADD_H_2_2(mul, add, dst, src);                                          \
    dst      = add(dst, c0)

#define MUL_ADD_H_4(mul, add, dst, src)                                        \
    src ## 1 = mul(src ## 1, r0);                                              \
    src ## 2 = mul(src ## 2, r0);                                              \
    src ## 3 = mul(src ## 3, r0);                                              \
    src ## 4 = mul(src ## 4, r0);                                              \
    src ## 1 = add(src ## 1, src ## 2);                                        \
    src ## 3 = add(src ## 3, src ## 4);                                        \
    dst      = add(src ## 1, src ## 3)

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

#define MUL_ADD_V_LAST_4(mul, add, dst, src)                                        \
    dst = mul(src ## 1, c5);                                                   \
    dst = add(dst, mul(src ## 2, c6));                                         \
    dst = add(dst, mul(src ## 3, c7));                                         \
    dst = add(dst, mul(src ## 4, c8))

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define INST_SRC1_CST_2(inst, dst, src, cst)                                   \
    dst ## 1 = inst(src ## 1, cst);                                            \
    dst ## 2 = inst(src ## 2, cst)
#define INST_SRC1_CST_4(inst, dst, src, cst)                                   \
    INST_SRC1_CST_2(inst, dst, src, cst);                                      \
    dst ## 3 = inst(src ## 3, cst);                                            \
    dst ## 4 = inst(src ## 4, cst)
#define INST_SRC1_CST_8(inst, dst, src, cst)                                   \
    INST_SRC1_CST_4(inst, dst, src, cst);                                      \
    dst ## 5 = inst(src ## 5, cst);                                            \
    dst ## 6 = inst(src ## 6, cst);                                            \
    dst ## 7 = inst(src ## 7, cst);                                            \
    dst ## 8 = inst(src ## 8, cst)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define UNPACK_SRAI16_4(inst, dst, src)                                        \
    dst ## 1 = _mm_srai_epi32(inst(c0, src ## 1), 16);                         \
    dst ## 2 = _mm_srai_epi32(inst(c0, src ## 2), 16);                         \
    dst ## 3 = _mm_srai_epi32(inst(c0, src ## 3), 16);                         \
    dst ## 4 = _mm_srai_epi32(inst(c0, src ## 4), 16)
#define UNPACK_SRAI16_8(inst, dst, src)                                        \
    dst ## 1 = _mm_srai_epi32(inst(c0, src ## 1), 16);                         \
    dst ## 2 = _mm_srai_epi32(inst(c0, src ## 2), 16);                         \
    dst ## 3 = _mm_srai_epi32(inst(c0, src ## 3), 16);                         \
    dst ## 4 = _mm_srai_epi32(inst(c0, src ## 4), 16);                         \
    dst ## 5 = _mm_srai_epi32(inst(c0, src ## 5), 16);                         \
    dst ## 6 = _mm_srai_epi32(inst(c0, src ## 6), 16);                         \
    dst ## 7 = _mm_srai_epi32(inst(c0, src ## 7), 16);                         \
    dst ## 8 = _mm_srai_epi32(inst(c0, src ## 8), 16)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define WEIGHTED_INIT2_8()                                                     \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    const __m128i mask2 = _mm_set_epi16(0, 0, 0, 0, 0, 0, 0, -1)
#define WEIGHTED_INIT4_8()                                                     \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    const __m128i mask2 = _mm_set_epi32(0, 0, 0, -1)
#define WEIGHTED_INIT8_8()
#define WEIGHTED_INIT16_8()

#define WEIGHTED_INIT2_10()                                                    \
    const __m128i mask = _mm_set_epi32(0, 0, 0, -1)
#define WEIGHTED_INIT4_10()                                                       
#define WEIGHTED_INIT8_10()
#define WEIGHTED_INIT16_10()

#define WEIGHTED_INIT2_14()  WEIGHTED_INIT2_10()                                                      
#define WEIGHTED_INIT4_14()                                                       
#define WEIGHTED_INIT8_14() 
#define WEIGHTED_INIT16_14()


#define WEIGHTED_LOAD2()                                                       \
    r1 = _mm_loadl_epi64((__m128i *) &src[x    ])
#define WEIGHTED_LOAD4()                                                       \
    WEIGHTED_LOAD2()
#define WEIGHTED_LOAD8()                                                       \
    r1 = _mm_load_si128((__m128i *) &src[x    ])
#define WEIGHTED_LOAD16()                                                      \
    WEIGHTED_LOAD8();                                                          \
    r2 = _mm_load_si128((__m128i *) &src[x + 8])

#define WEIGHTED_LOAD2_1()                                                     \
    r3 = _mm_loadl_epi64((__m128i *) &src1[x    ])
#define WEIGHTED_LOAD4_1()                                                     \
    WEIGHTED_LOAD2_1()  
#define WEIGHTED_LOAD8_1()                                                     \
    r3 = _mm_load_si128((__m128i *) &src1[x    ])
#define WEIGHTED_LOAD16_1()                                                    \
    WEIGHTED_LOAD8_1();                                                        \
    r4 = _mm_load_si128((__m128i *) &src1[x + 8])

#define WEIGHTED_STORE2_8()                                                      \
    r1 = _mm_packus_epi16(r1, r1);                                             \
    *((short *) (dst + x)) = _mm_extract_epi16(r1, 0)
#define WEIGHTED_STORE4_8()                                                    \
    r1 = _mm_packus_epi16(r1, r1);                                             \
    *((uint32_t *) &dst[x]) =_mm_cvtsi128_si32(r1)
#define WEIGHTED_STORE8_8()                                                      \
    r1 = _mm_packus_epi16(r1, r1);                                             \
    _mm_storel_epi64((__m128i *) &dst[x], r1)
#define WEIGHTED_STORE16_8()                                                   \
    r1 = _mm_packus_epi16(r1, r2);                                             \
    _mm_storeu_si128((__m128i *) &dst[x], r1)

#define WEIGHTED_STORE2_10() PEL_STORE2(dst);
#define WEIGHTED_STORE4_10() PEL_STORE4(dst);
#define WEIGHTED_STORE8_10() PEL_STORE8(dst);

#define WEIGHTED_STORE2_14() PEL_STORE2(dst);
#define WEIGHTED_STORE4_14() PEL_STORE4(dst);
#define WEIGHTED_STORE8_14() PEL_STORE8(dst);

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

#define WEIGHTED_INIT_0(H, D)                                                  \
    WEIGHTED_INIT ## H ## _ ## D();                                             \
    const int shift2 = 14 - D;                                                 \
    WEIGHTED_INIT_0_ ## D()

#define WEIGHTED_INIT_0_8()                                                    \
    const __m128i m1 = _mm_set1_epi16(1 << (14 - 8 - 1))

#define WEIGHTED_INIT_0_10()                                                    \
    const __m128i m1 = _mm_set1_epi16(1 << (14 - 10 - 1))

#define WEIGHTED_INIT_0_14()                                                    \
    const __m128i m1 = _mm_setzero_si128()

#define WEIGHTED_INIT_1(H, D)                                                  \
    WEIGHTED_INIT ## H ## _ ## D();                                             \
    const int shift2     = denom + 14 - D;                                     \
    const __m128i add   = _mm_set1_epi32(olxFlag << (D - 8));                  \
    const __m128i add2  = _mm_set1_epi32(1 << (shift2-1));                     \
    const __m128i m1    = _mm_set1_epi16(wlxFlag);                             \
    __m128i s1, s2, s3

#define WEIGHTED_INIT_2(H, D)                                                  \
    WEIGHTED_INIT ## H ## _ ## D();                                             \
    const int shift2 = 14 + 1 - D;                                             \
    const __m128i m1 = _mm_set1_epi16(1 << (14 - D))

#define WEIGHTED_INIT_3(H, D)                                                  \
    WEIGHTED_INIT ## H ## _ ## D();                                             \
    const int log2Wd = denom + 14 - D;                                         \
    const int shift2  = log2Wd + 1;                                            \
    const int o0     = olxFlag << (D - 8);                                     \
    const int o1     = ol1Flag << (D - 8);                                     \
    const __m128i m1 = _mm_set1_epi16(wlxFlag);                                \
    const __m128i m2 = _mm_set1_epi16(wl1Flag);                                \
    const __m128i m3 = _mm_set1_epi32((o0 + o1 + 1) << log2Wd);                \
    __m128i s1, s2, s3, s4, s5, s6


////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

#define WEIGHTED_COMPUTE_0(reg1)                                               \
    reg1 = _mm_srai_epi16(_mm_adds_epi16(reg1, m1), shift2)
#define WEIGHTED_COMPUTE2_0()                                                  \
    WEIGHTED_COMPUTE_0(r1)
#define WEIGHTED_COMPUTE4_0()                                                  \
    WEIGHTED_COMPUTE_0(r1)
#define WEIGHTED_COMPUTE8_0()                                                  \
    WEIGHTED_COMPUTE_0(r1)
#define WEIGHTED_COMPUTE16_0()                                                 \
    WEIGHTED_COMPUTE_0(r1);                                                    \
    WEIGHTED_COMPUTE_0(r2)

#define WEIGHTED_COMPUTE_1(reg1)                                               \
    s1   = _mm_mullo_epi16(reg1, m1);                                          \
    s2   = _mm_mulhi_epi16(reg1, m1);                                          \
    s3   = _mm_unpackhi_epi16(s1, s2);                                         \
    reg1 = _mm_unpacklo_epi16(s1, s2);                                         \
    reg1 = _mm_srai_epi32(_mm_add_epi32(reg1, add2), shift2);                  \
    s3   = _mm_srai_epi32(_mm_add_epi32(s3  , add2), shift2);                  \
    reg1 = _mm_add_epi32(reg1, add);                                           \
    s3   = _mm_add_epi32(s3  , add);                                           \
    reg1 = _mm_packus_epi32(reg1, s3)
#define WEIGHTED_COMPUTE2_1()                                                  \
    WEIGHTED_COMPUTE_1(r1)
#define WEIGHTED_COMPUTE4_1()                                                  \
    WEIGHTED_COMPUTE_1(r1)
#define WEIGHTED_COMPUTE4_1()                                                  \
    WEIGHTED_COMPUTE_1(r1)
#define WEIGHTED_COMPUTE8_1()                                                  \
    WEIGHTED_COMPUTE_1(r1)
#define WEIGHTED_COMPUTE16_1()                                                 \
    WEIGHTED_COMPUTE_1(r1);                                                    \
    WEIGHTED_COMPUTE_1(r2)

#define WEIGHTED_COMPUTE_2(reg1, reg2)                                         \
    reg1 = _mm_adds_epi16(reg1, m1);                                           \
    reg1 = _mm_adds_epi16(reg1, reg2);                                         \
    reg2 = _mm_srai_epi16(reg1, shift2)
#define WEIGHTED_COMPUTE2_2()                                                  \
    WEIGHTED_LOAD2_1();                                                        \
    WEIGHTED_COMPUTE_2(r3, r1)
#define WEIGHTED_COMPUTE4_2()                                                  \
    WEIGHTED_LOAD4_1();                                                        \
    WEIGHTED_COMPUTE_2(r3, r1)
#define WEIGHTED_COMPUTE8_2()                                                  \
    WEIGHTED_LOAD8_1();                                                        \
    WEIGHTED_COMPUTE_2(r3, r1)
#define WEIGHTED_COMPUTE16_2()                                                 \
    WEIGHTED_LOAD16_1();                                                       \
    WEIGHTED_COMPUTE_2(r3, r1);                                                \
    WEIGHTED_COMPUTE_2(r4, r2)

#define WEIGHTED_COMPUTE_3(reg1, reg2)                                         \
    s1   = _mm_mullo_epi16(reg1, m1);                                          \
    s2   = _mm_mulhi_epi16(reg1, m1);                                          \
    s3   = _mm_mullo_epi16(reg2, m2);                                          \
    s4   = _mm_mulhi_epi16(reg2, m2);                                          \
    s5   = _mm_unpacklo_epi16(s1, s2);                                         \
    s6   = _mm_unpacklo_epi16(s3, s4);                                         \
    reg1 = _mm_unpackhi_epi16(s1, s2);                                         \
    reg2 = _mm_unpackhi_epi16(s3, s4);                                         \
    reg1 = _mm_add_epi32(reg1, reg2);                                          \
    reg2 = _mm_add_epi32(s5, s6);                                              \
    reg1 = _mm_srai_epi32(_mm_add_epi32(reg1, m3), shift2);                    \
    reg2 = _mm_srai_epi32(_mm_add_epi32(reg2, m3), shift2);                    \
    reg2 = _mm_packus_epi32(reg2, reg1)
#define WEIGHTED_COMPUTE2_3()                                                  \
    WEIGHTED_LOAD2_1();                                                        \
    WEIGHTED_COMPUTE_3(r3, r1)
#define WEIGHTED_COMPUTE4_3()                                                  \
    WEIGHTED_LOAD4_1();                                                        \
    WEIGHTED_COMPUTE_3(r3, r1)
#define WEIGHTED_COMPUTE8_3()                                                  \
    WEIGHTED_LOAD8_1();                                                        \
    WEIGHTED_COMPUTE_3(r3, r1)
#define WEIGHTED_COMPUTE16_3()                                                 \
    WEIGHTED_LOAD16_1();                                                       \
    WEIGHTED_COMPUTE_3(r3, r1);                                                \
    WEIGHTED_COMPUTE_3(r4, r2)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_mc_pixelsX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define MC_PIXEL_COMPUTE2_8()                                                  \
    x2 = _mm_unpacklo_epi8(x1, c0);                                            \
    r1 = _mm_slli_epi16(x2, 14 - 8)
#define MC_PIXEL_COMPUTE4_8()                                                  \
    MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE8_8()                                                  \
    MC_PIXEL_COMPUTE2_8()
#define MC_PIXEL_COMPUTE16_8()                                                 \
    MC_PIXEL_COMPUTE2_8();                                                     \
    x3 = _mm_unpackhi_epi8(x1, c0);                                            \
    r2 = _mm_slli_epi16(x3, 14 - 8)

#define MC_PIXEL_COMPUTE2_10()                                                 \
    r1 = _mm_slli_epi16(x1, 14 - 10)
#define MC_PIXEL_COMPUTE4_10()                                                 \
    MC_PIXEL_COMPUTE2_10()
#define MC_PIXEL_COMPUTE8_10()                                                 \
    MC_PIXEL_COMPUTE2_10()

#define PUT_HEVC_EPEL_PIXELS(H, D)                                             \
void ff_hevc_put_hevc_epel_pixels ## H ## _ ## D ## _sse (                     \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my) {        \
    int x, y;                                                                  \
    __m128i x1, x2, x3, r1, r2;                                                \
    const __m128i c0    = _mm_setzero_si128();                                 \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    SRC_INIT_ ## D();                                                          \
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

#define PUT_HEVC_QPEL_PIXELS(H, D)                                             \
void ff_hevc_put_hevc_qpel_pixels ## H  ## _ ## D ## _sse (                    \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height) {                   \
    int x, y;                                                                  \
    __m128i x1, x2, x3, r1, r2;                                                \
    const __m128i c0    = _mm_setzero_si128();                                 \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    SRC_INIT_ ## D();                                                          \
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

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hX_X_sse
////////////////////////////////////////////////////////////////////////////////   
#define EPEL_H_COMPUTE2_8()                                                    \
    x1 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    MUL_ADD_H_1(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)
#define EPEL_H_COMPUTE4_8()                                                    \
    EPEL_H_COMPUTE2_8()
#define EPEL_H_COMPUTE8_8()                                                    \
    INST_SRC1_CST_2(_mm_shuffle_epi8, x, x , bshuffle1);                       \
    MUL_ADD_H_2_2(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)

#define EPEL_H_COMPUTE2_10()                                                   \
    x1 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    MUL_ADD_H_1(_mm_madd_epi16, _mm_hadd_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, 10 - 8);                                           \
    r1 = _mm_packs_epi32(r1, c0)
#define EPEL_H_COMPUTE4_10()                                                   \
    x2 = _mm_shuffle_epi8(x1, bshuffle2);                                      \
    x1 = _mm_shuffle_epi8(x1, bshuffle1);                                      \
    MUL_ADD_H_2_2(_mm_madd_epi16, _mm_hadd_epi32, r1, x);                      \
    r1 = _mm_srai_epi32(r1, 10 - 8);                                           \
    r1 = _mm_packs_epi32(r1, c0)

#define PUT_HEVC_EPEL_H(H, D)                                                  \
void ff_hevc_put_hevc_epel_h ## H ## _ ## D ## _sse (                          \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my) {        \
    int x, y;                                                                  \
    __m128i x1, x2, r1;                                                        \
    const __m128i c0     = _mm_setzero_si128();                                \
    const __m128i mask   = _mm_set_epi32(0, 0, 0, -1);                         \
    SRC_INIT_ ## D();                                                          \
    EPEL_H_FILTER_ ## D();                                                     \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_H_LOAD ## H();                                                \
            EPEL_H_COMPUTE ## H ## _ ## D();                                   \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_vX_X_sse
////////////////////////////////////////////////////////////////////////////////
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
    UNPACK_SRAI16_4(_mm_unpacklo_epi16, x, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    r1 = _mm_packs_epi32(r1, c0)
#define EPEL_V_COMPUTE4_10()                                                   \
    EPEL_V_COMPUTE2_10()
#define EPEL_V_COMPUTE8_10()                                                   \
    UNPACK_SRAI16_4(_mm_unpacklo_epi16, t, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r2, t);                        \
    UNPACK_SRAI16_4(_mm_unpackhi_epi16, x, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, x);                        \
    r2 = _mm_srai_epi32(r2, shift);                                            \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    r1 = _mm_packs_epi32(r2, r1)

#define EPEL_V_COMPUTE2_14() EPEL_V_COMPUTE2_10()
#define EPEL_V_COMPUTE4_14() EPEL_V_COMPUTE4_10()
#define EPEL_V_COMPUTE8_14() EPEL_V_COMPUTE8_10()

#define EPEL_V_COMPUTEB2_8()                                                    \
    INST_SRC1_CST_4(_mm_unpacklo_epi8, y, x , c0);                             \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r1, y)
#define EPEL_V_COMPUTEB4_8()                                                    \
    EPEL_V_COMPUTEB2_8()
#define EPEL_V_COMPUTEB8_8()                                                    \
    EPEL_V_COMPUTEB2_8()
#define EPEL_V_COMPUTEB16_8()                                                   \
    INST_SRC1_CST_4(_mm_unpackhi_epi8, t, x , c0);                             \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r2, t);                       \
    EPEL_V_COMPUTEB2_8()

#define EPEL_V_COMPUTEB2_10()                                                   \
    UNPACK_SRAI16_4(_mm_unpacklo_epi16, y, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, y);                        \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    r1 = _mm_packs_epi32(r1, c0)
#define EPEL_V_COMPUTEB4_10()                                                   \
    EPEL_V_COMPUTEB2_10()
#define EPEL_V_COMPUTEB8_10()                                                   \
    UNPACK_SRAI16_4(_mm_unpacklo_epi16, t, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r2, t);                        \
    UNPACK_SRAI16_4(_mm_unpackhi_epi16, y, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, y);                        \
    r2 = _mm_srai_epi32(r2, shift);                                            \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    r1 = _mm_packs_epi32(r2, r1)

#define EPEL_V_COMPUTEB2_14() EPEL_V_COMPUTEB2_10()                            
#define EPEL_V_COMPUTEB4_14() EPEL_V_COMPUTEB2_10()
#define EPEL_V_COMPUTEB8_14() EPEL_V_COMPUTEB8_10()


#define EPEL_V_SHIFT(tab)                                                      \
    x1= x2;                                                                    \
    x2= x3;                                                                    \
    x3= x4;                                                                    \
    x4 = _mm_loadu_si128((__m128i *) &tab[x + 2 * srcstride])

#define PUT_HEVC_EPEL_V(V, D)                                                  \
void ff_hevc_put_hevc_epel_v ## V ## _ ## D ## _sse (                          \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my) {        \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    __m128i x1, x2, x3, x4;                                                    \
    __m128i y1, y2, y3, y4;                                                    \
    __m128i t1, t2, t3, t4;                                                    \
    __m128i r1, r2;                                                            \
    const __m128i c0    = _mm_setzero_si128();                                 \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    SRC_INIT_ ## D();                                                          \
    uint16_t  *_dst       = (uint16_t*) dst;                                   \
    EPEL_V_FILTER_ ## D();                                                     \
for (x = 0; x < width; x+= V) {                                                \
    EPEL_V_LOAD3(src);                                                         \
        for(y = 0; y < height; y++) {                                          \
            EPEL_V_SHIFT(src);                                                 \
            EPEL_V_COMPUTEB ## V ## _ ## D();                                  \
            PEL_STORE ## V(_dst);                                              \
            src += srcstride;                                                  \
            _dst += dststride;                                                 \
        }                                                                      \
        SRC_INIT1_ ## D();                                                     \
        _dst       = (uint16_t*) dst;                                          \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hvX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_EPEL_HV(H, D)                                                 \
void ff_hevc_put_hevc_epel_hv ## H ## _ ## D ## _sse (                         \
                                   int16_t *dst, ptrdiff_t dststride,          \
                                   uint8_t *_src, ptrdiff_t _srcstride,        \
                                   int width, int height,                      \
                                   int mx, int my) {        \
                                                                        \
                                                                          \
}


////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hX_X_X_sse
////////////////////////////////////////////////////////////////////////////////
#define QPEL_H_COMPUTE4_8()                                                    \
    INST_SRC1_CST_2(_mm_shuffle_epi8, x, x , bshuffle1);                       \
    MUL_ADD_H_2(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)
#define QPEL_H_COMPUTE8_8()                                                    \
    INST_SRC1_CST_4(_mm_shuffle_epi8, x, x , bshuffle1);                       \
    MUL_ADD_H_4(_mm_maddubs_epi16, _mm_hadd_epi16, r1, x)
#define QPEL_H_COMPUTE2_10()                                                   \
    MUL_ADD_H_2(_mm_madd_epi16, _mm_hadd_epi32, r1, x);                        \
    r1 = _mm_srai_epi32(r1, 10 - 8);                                           \
    r1 = _mm_packs_epi32(r1, c0)
#define QPEL_H_COMPUTE4_10()                                                   \
    QPEL_H_COMPUTE2_10()

#define PUT_HEVC_QPEL_H(H, F, D)                                               \
void ff_hevc_put_hevc_qpel_h ## H ## _ ## F ## _ ## D ## _sse (                \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height) {                   \
    int x, y;                                                                  \
    __m128i x1, x2, x3, x4, r1;                                                \
    const __m128i c0    = _mm_setzero_si128();                                 \
    const __m128i mask  = _mm_set_epi32(0, 0, 0, -1);                          \
    SRC_INIT_ ## D();                                                          \
    QPEL_H_FILTER_ ## F ## _ ## D();                                           \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_H_LOAD ## H();                                                \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            PEL_STORE ## H(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_vX_X_X_sse
////////////////////////////////////////////////////////////////////////////////

#define QPEL_V_COMPUTE_FIRST4_8()                                              \
    INST_SRC1_CST_4(_mm_unpacklo_epi8, x, x , c0);                              \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r1, x)
#define QPEL_V_COMPUTE_FIRST8_8()                                              \
    QPEL_V_COMPUTE_FIRST4_8()
#define QPEL_V_COMPUTE_FIRST16_8()                                             \
    INST_SRC1_CST_4(_mm_unpackhi_epi8, t, x , c0);                             \
    MUL_ADD_V_4(_mm_mullo_epi16, _mm_adds_epi16, r2, t);                       \
    QPEL_V_COMPUTE_FIRST4_8()

#define QPEL_V_COMPUTE_LAST4_8()                                              \
    INST_SRC1_CST_4(_mm_unpacklo_epi8, x, x , c0);                             \
    MUL_ADD_V_LAST_4(_mm_mullo_epi16, _mm_adds_epi16, r3, x)
#define QPEL_V_COMPUTE_LAST8_8()                                               \
    QPEL_V_COMPUTE_LAST4_8()
#define QPEL_V_COMPUTE_LAST16_8()                                              \
    INST_SRC1_CST_4(_mm_unpackhi_epi8, t, x , c0);                             \
    MUL_ADD_V_LAST_4(_mm_mullo_epi16, _mm_adds_epi16, r4, t);                  \
    QPEL_V_COMPUTE_LAST4_8()

#define QPEL_V_MERGE4_8()                                                      \
    r1= _mm_add_epi16(r1,r3)
#define QPEL_V_MERGE8_8()                                                      \
        QPEL_V_MERGE4_8()
#define QPEL_V_MERGE16_8()                                                     \
    r1= _mm_add_epi16(r1,r3);                                                  \
    r2= _mm_add_epi16(r2,r4)


#define QPEL_V_COMPUTE_FIRST2_10()                                             \
    UNPACK_SRAI16_4(_mm_unpacklo_epi16, x, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r1, x)
#define QPEL_V_COMPUTE_FIRST4_10()                                             \
    QPEL_V_COMPUTE_FIRST2_10()

#define QPEL_V_COMPUTE_FIRST8_10()                                             \
    UNPACK_SRAI16_4(_mm_unpackhi_epi16, t, x);                                 \
    MUL_ADD_V_4(_mm_mullo_epi32, _mm_add_epi32, r2, t);                        \
    QPEL_V_COMPUTE_FIRST2_10()


#define QPEL_V_COMPUTE_LAST2_10()                                              \
    UNPACK_SRAI16_4(_mm_unpacklo_epi16, x, x);                                 \
    MUL_ADD_V_LAST_4(_mm_mullo_epi32, _mm_add_epi32, r3, x)

#define QPEL_V_COMPUTE_LAST4_10()                                              \
    QPEL_V_COMPUTE_LAST2_10()

#define QPEL_V_COMPUTE_LAST8_10()                                              \
    UNPACK_SRAI16_4(_mm_unpackhi_epi16, t, x);                                 \
    MUL_ADD_V_LAST_4(_mm_mullo_epi32, _mm_add_epi32, r4, t);                   \
    QPEL_V_COMPUTE_LAST2_10()

#define QPEL_V_COMPUTE_FIRST2_14() QPEL_V_COMPUTE_FIRST2_10()
#define QPEL_V_COMPUTE_FIRST4_14() QPEL_V_COMPUTE_FIRST4_10()
#define QPEL_V_COMPUTE_FIRST8_14() QPEL_V_COMPUTE_FIRST8_10()

#define QPEL_V_COMPUTE_LAST2_14() QPEL_V_COMPUTE_LAST2_10()
#define QPEL_V_COMPUTE_LAST4_14() QPEL_V_COMPUTE_LAST4_10()
#define QPEL_V_COMPUTE_LAST8_14() QPEL_V_COMPUTE_LAST8_10()

#define QPEL_V_MERGE2_10()                                                     \
    r1= _mm_add_epi32(r1,r3);                                                  \
r1 = _mm_srai_epi32(r1, shift);                                                \
r1 = _mm_packs_epi32(r1, c0)
#define QPEL_V_MERGE4_10()                                                     \
        QPEL_V_MERGE2_10()
#define QPEL_V_MERGE8_10()                                                     \
    r1= _mm_add_epi32(r1,r3);                                                  \
    r3= _mm_add_epi32(r2,r4);                                                  \
    r1 = _mm_srai_epi32(r1, shift);                                            \
    r3 = _mm_srai_epi32(r3, shift);                                            \
    r1 = _mm_packs_epi32(r1, r3)


#define QPEL_V_MERGE2_14() QPEL_V_MERGE2_10()
#define QPEL_V_MERGE4_14() QPEL_V_MERGE4_10()
#define QPEL_V_MERGE8_14() QPEL_V_MERGE8_10()

#define PUT_HEVC_QPEL_V(V, F, D)                                               \
void ff_hevc_put_hevc_qpel_v ## V ##_ ## F ## _ ## D ## _sse (                 \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height) {                   \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    __m128i x1, x2, x3, x4, r1, r2,r3,r4;                                      \
    __m128i t1, t2, t3, t4;                                                    \
    const __m128i c0    = _mm_setzero_si128();                                 \
    SRC_INIT_ ## D();                                                          \
    QPEL_V_FILTER_ ## F ## _ ## D();                                           \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD_LO(src);                                               \
            QPEL_V_COMPUTE_FIRST ## V ## _ ## D();                             \
            QPEL_V_LOAD_HI(src);                                                \
            QPEL_V_COMPUTE_LAST ## V ## _ ## D();                              \
            QPEL_V_MERGE ## V ## _ ## D();                                     \
            PEL_STORE ## V(dst);                                               \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hX_X_vX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_HV(H, FH, FV, D)                                         \
void ff_hevc_put_hevc_qpel_h ## H ## _ ## FH ## _v_ ## FV ##_ ## D ## _sse (   \
                                    int16_t *dst, ptrdiff_t dststride,         \
                                    uint8_t *_src, ptrdiff_t _srcstride,       \
                                    int width, int height) {                   \
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//PUT_UNWEIGHTED_PRED_END
#define WEIGHTED_END_0()

//PUT_WEIGHTED_PRED_END
#define WEIGHTED_END_1()

//PUT_WEIGHTED_PRED_ARG_END
#define WEIGHTED_END_2()                                                       \
    src1 += src1stride;

//WEIGHTED_PRED_ARG_END
#define WEIGHTED_END_3()                                                       \
    src1 += src1stride;

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_mc_pixelsX_X_w_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_EPEL_PIXELS_W(H, D, W)                                        \
void ff_hevc_put_hevc_epel_pixels ## H ## _w ## W ## _## D ## _sse (           \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height, int mx, int my,            \
                             uint8_t denom, int16_t wlxFlag, int16_t wl1Flag,  \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    __m128i x1, x2, x3, r1, r2, r3, r4;                                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    WEIGHTED_INIT_ ## W(H, D);                                                 \
    SRC_INIT_ ## D();                                                          \
    DST_INIT_ ## D();                                                          \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            WEIGHTED_COMPUTE ## H ## _ ## W();                                 \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}

#define PUT_HEVC_QPEL_PIXELS_W(H, D, W)                                        \
void ff_hevc_put_hevc_qpel_pixels ## H  ## _w ## W ## _## D ## _sse (         \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height, uint8_t denom,             \
                             int16_t wlxFlag, int16_t wl1Flag,                 \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    __m128i x1, x2, x3, r1, r2, r3, r4;                                        \
    const __m128i c0    = _mm_setzero_si128();                                 \
    WEIGHTED_INIT_ ## W(H, D);                                                 \
    SRC_INIT_ ## D();                                                          \
    DST_INIT_ ## D();                                                          \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            MC_LOAD_PIXEL();                                                   \
            MC_PIXEL_COMPUTE ## H ## _ ## D();                                 \
            WEIGHTED_COMPUTE ## H ## _ ## W();                                 \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_EPEL_H_W(H, D, W)                                             \
void ff_hevc_put_hevc_epel_h ## H ## _w ## W ## _## D ## _sse (                \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height, int mx, int my,            \
                             uint8_t denom, int16_t wlxFlag, int16_t wl1Flag,  \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    __m128i x1, x2, r1, r2, r3, r4;                                            \
    const __m128i c0     = _mm_setzero_si128();                                \
    WEIGHTED_INIT_ ## W(H, D);                                                 \
    SRC_INIT_ ## D();                                                          \
    DST_INIT_ ## D();                                                          \
    EPEL_H_FILTER_ ## D();                                                     \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            EPEL_H_LOAD ## H();                                                \
            EPEL_H_COMPUTE ## H ## _ ## D();                                   \
            WEIGHTED_COMPUTE ## H ## _ ## W();                                 \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_vX_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_EPEL_V_W(V, D, W)                                             \
void ff_hevc_put_hevc_epel_v ## V ## _w ## W ## _## D ## _sse (                \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height, int mx, int my,            \
                             uint8_t denom, int16_t wlxFlag, int16_t wl1Flag,  \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    __m128i x1, x2, x3, x4;                                                    \
    __m128i t1, t2, t3, t4;                                                    \
    __m128i r1, r2, r3, r4;                                                    \
    const __m128i c0    = _mm_setzero_si128();                                 \
    WEIGHTED_INIT_ ## W(V, D);                                                 \
    SRC_INIT_ ## D();                                                          \
    DST_INIT_ ## D();                                                          \
    EPEL_V_FILTER_ ## D();                                                     \
    for (y = 0; y < height; y++) {                                             \
        for(x = 0; x < width; x += V) {                                        \
            EPEL_V_LOAD(src);                                                  \
            EPEL_V_COMPUTE ## V ## _ ## D();                                   \
            WEIGHTED_COMPUTE ## V ## _ ## W();                                 \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}
#define PUT_HEVC_EPEL_V_W_14b(V, D, W)                                         \
void ff_hevc_put_hevc_epel_v ## V ## _w ## W ## _14b_## D ## _sse (            \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height, int mx, int my,            \
                             uint8_t denom, int16_t wlxFlag, int16_t wl1Flag,  \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    int shift = 14 - 8;                                                        \
    __m128i x1, x2, x3, x4;                                                    \
    __m128i t1, t2, t3, t4;                                                    \
    __m128i r1, r2, r3, r4;                                                    \
    const __m128i c0    = _mm_setzero_si128();                                 \
    WEIGHTED_INIT_ ## W(V, D);                                                 \
    SRC_INIT_10();                                                             \
    DST_INIT_ ## D();                                                          \
    EPEL_V_FILTER_10();                                                        \
    for (y = 0; y < height; y++) {                                             \
        for(x = 0; x < width; x += V) {                                        \
            EPEL_V_LOAD(src);                                                  \
            EPEL_V_COMPUTE ## V ## _10();                                      \
            WEIGHTED_COMPUTE ## V ## _ ## W();                                 \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_epel_hvX_X_sse
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_hX_X_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_H_W(H, F, D, W)                                          \
void ff_hevc_put_hevc_qpel_h ## H ## _ ## F ## _w ## W ## _## D ## _sse (      \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height, uint8_t denom,             \
                             int16_t wlxFlag, int16_t wl1Flag,                 \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    __m128i x1, x2, x3, x4, r1, r2, r3, r4;                                    \
    const __m128i c0    = _mm_setzero_si128();                                 \
    WEIGHTED_INIT_ ## W(H, D);                                                 \
    SRC_INIT_ ## D();                                                          \
    DST_INIT_ ## D();                                                          \
    QPEL_H_FILTER_ ## F ## _ ## D();                                           \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            QPEL_H_LOAD ## H();                                                \
            QPEL_H_COMPUTE ## H ## _ ## D();                                   \
            WEIGHTED_COMPUTE ## H ## _ ## W();                                 \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_put_hevc_qpel_vX_X_X_sse
////////////////////////////////////////////////////////////////////////////////
#define PUT_HEVC_QPEL_V_W(V, F, D, W)                                          \
void ff_hevc_put_hevc_qpel_v ## V ##_ ## F ## _w ## W ## _## D ## _sse (       \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height, uint8_t denom,             \
                             int16_t wlxFlag, int16_t wl1Flag,                 \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    int shift = D - 8;                                                         \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8;                                    \
    __m128i t1, t2, t3, t4, t5, t6, t7, t8;                                    \
    __m128i r1, r2, r3, r4;                                                    \
    const __m128i c0    = _mm_setzero_si128();                                 \
    WEIGHTED_INIT_ ## W(V, D);                                                 \
    SRC_INIT_ ## D();                                                          \
    DST_INIT_ ## D();                                                          \
    QPEL_V_FILTER_ ## F ## _ ## D();                                           \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD_LO(src);                                               \
            QPEL_V_COMPUTE_FIRST ## V ## _ ## D();                             \
            QPEL_V_LOAD_HI(src);                                                \
            QPEL_V_COMPUTE_LAST ## V ## _ ## D();                              \
            QPEL_V_MERGE ## V ## _ ## D();                                     \
            WEIGHTED_COMPUTE ## V ## _ ## W();                                 \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}                                                                              
#define PUT_HEVC_QPEL_V_14b_W(V, F, D, W)                                      \
void ff_hevc_put_hevc_qpel_v ## V ##_ ## F ## _14b_w ## W ## _## D ## _sse (   \
                             uint8_t *_dst, ptrdiff_t _dststride,              \
                             int16_t *src1, ptrdiff_t src1stride,              \
                             uint8_t *_src, ptrdiff_t _srcstride,              \
                             int width, int height,                            \
                             uint8_t denom,                                    \
                             int16_t wlxFlag, int16_t wl1Flag,                 \
                             int16_t olxFlag, int16_t ol1Flag) {               \
    int x, y;                                                                  \
    int shift = 14 - 8;                                                        \
    __m128i x1, x2, x3, x4, x5, x6, x7, x8;                                    \
    __m128i t1, t2, t3, t4, t5, t6, t7, t8;                                    \
    __m128i r1, r2, r3, r4;                                                    \
    const __m128i c0    = _mm_setzero_si128();                                 \
    WEIGHTED_INIT_ ## W(V, D);                                                 \
    SRC_INIT_10();                                                             \
    DST_INIT_ ## D();                                                          \
    QPEL_V_FILTER_ ## F ## _10();                                              \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += V) {                                       \
            QPEL_V_LOAD_LO(src);                                               \
            QPEL_V_COMPUTE_FIRST ## V ## _10();                                \
            QPEL_V_LOAD_HI(src);                                               \
            QPEL_V_COMPUTE_LAST ## V ## _10();                                 \
            QPEL_V_MERGE ## V ## _10();                                        \
            WEIGHTED_COMPUTE ## V ## _ ## W();                                 \
            WEIGHTED_STORE ## V ## _ ## D();                                   \
        }                                                                      \
        src += srcstride;                                                      \
        dst += dststride;                                                      \
        WEIGHTED_END_ ## W()                                                   \
    }                                                                          \
}


////////////////////////////////////////////////////////////////////////////////

#define PUT_UNWEIGHTED_PRED(H, D)                                              \
static void put_unweighted_pred ## H ## _ ## D ##_sse (                        \
                                       uint8_t *dst, ptrdiff_t dststride,      \
                                       int16_t *src, ptrdiff_t srcstride,      \
                                       int width, int height) {                \
    int x, y;                                                                  \
    __m128i r1, r2;                                                            \
    WEIGHTED_INIT_0(H, D);                                                     \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H();                                              \
            WEIGHTED_COMPUTE ## H ## _0();                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        dst += dststride;                                                      \
        src += srcstride;                                                      \
    }                                                                          \
}


PUT_UNWEIGHTED_PRED(2,  8)
PUT_UNWEIGHTED_PRED(4,  8)
PUT_UNWEIGHTED_PRED(8,  8)
PUT_UNWEIGHTED_PRED(16, 8)

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

#define PUT_WEIGHTED_PRED_AVG(H, D)                                            \
static void put_weighted_pred_avg ## H ## _ ## D ##_sse(                              \
                                uint8_t *dst, ptrdiff_t dststride,             \
                                int16_t *src1, int16_t *src,                   \
                                ptrdiff_t srcstride,                           \
                                int width, int height) {                       \
    int x, y;                                                                  \
    __m128i r1, r2, r3, r4;                                                    \
    WEIGHTED_INIT_2(H, D);                                                     \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H();                                              \
            WEIGHTED_COMPUTE ## H ## _2();                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        dst  += dststride;                                                     \
        src  += srcstride;                                                     \
        src1 += srcstride;                                                     \
    }                                                                          \
}
PUT_WEIGHTED_PRED_AVG(2,  8)
PUT_WEIGHTED_PRED_AVG(4,  8)
PUT_WEIGHTED_PRED_AVG(8,  8)
PUT_WEIGHTED_PRED_AVG(16, 8)

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

#define WEIGHTED_PRED(H, D)                                                    \
static void weighted_pred ## H ## _ ## D ##_sse(                                      \
                                    uint8_t denom,                             \
                                    int16_t wlxFlag, int16_t olxFlag,          \
                                    uint8_t *dst, ptrdiff_t dststride,         \
                                    int16_t *src, ptrdiff_t srcstride,         \
                                    int width, int height) {                   \
    int x, y;                                                                  \
    __m128i r1, r2;                                                            \
    WEIGHTED_INIT_1(H, D);                                             \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H();                                              \
            WEIGHTED_COMPUTE ## H ## _1();                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        dst += dststride;                                                      \
        src += srcstride;                                                      \
    }                                                                          \
}
WEIGHTED_PRED(2, 8)
WEIGHTED_PRED(4, 8)
WEIGHTED_PRED(8, 8)
WEIGHTED_PRED(16, 8)

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

#define WEIGHTED_PRED_AVG(H, D)                                                   \
static void weighted_pred_avg ## H ## _ ## D ##_sse(                                  \
                                    uint8_t denom,                             \
                                    int16_t wlxFlag, int16_t wl1Flag,          \
                                    int16_t olxFlag, int16_t ol1Flag,          \
                                    uint8_t *dst, ptrdiff_t dststride,         \
                                    int16_t *src1, int16_t *src,               \
                                    ptrdiff_t srcstride,                       \
                                    int width, int height) {                   \
    int x, y;                                                                  \
    __m128i r1, r2, r3, r4;                                                    \
    WEIGHTED_INIT_3(H, D);                                             \
    for (y = 0; y < height; y++) {                                             \
        for (x = 0; x < width; x += H) {                                       \
            WEIGHTED_LOAD ## H();                                              \
            WEIGHTED_COMPUTE ## H ## _3();                                     \
            WEIGHTED_STORE ## H ## _ ## D();                                   \
        }                                                                      \
        dst  += dststride;                                                     \
        src  += srcstride;                                                     \
        src1 += srcstride;                                                     \
    }                                                                          \
}
WEIGHTED_PRED_AVG(2, 8)
WEIGHTED_PRED_AVG(4, 8)
WEIGHTED_PRED_AVG(8, 8)
WEIGHTED_PRED_AVG(16, 8)

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
// ff_hevc_put_hevc_qpel_hX_X_vX_X_sse
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#undef EPEL_FUNC
#define EPEL_FUNC(FUNC, H, D)                                                  \
    FUNC(H, D)                                                                 \
    FUNC ##_W(H, D, 0)                                                         \
    FUNC ##_W(H, D, 1)                                                         \
    FUNC ##_W(H, D, 2)                                                         \
    FUNC ##_W(H, D, 3)
#undef QPEL_FUNC
#define QPEL_FUNC(FUNC, H, F, D)                                               \
    FUNC(H, F, D)                                                              \
    FUNC ##_W(H, F, D, 0)                                                      \
    FUNC ##_W(H, F, D, 1)                                                      \
    FUNC ##_W(H, F, D, 2)                                                      \
    FUNC ##_W(H, F, D, 3)
#undef QPEL_FUNC_HV
#define QPEL_FUNC_HV(FUNC, H, FH, FV, D)                                       \
    FUNC(H, FH, FV, D)                                                         \
    FUNC ##_W(H, FH, FV, D, 0)                                                 \
    FUNC ##_W(H, FH, FV, D, 1)                                                 \
    FUNC ##_W(H, FH, FV, D, 2)                                                 \
    FUNC ##_W(H, FH, FV, D, 3)


// ff_hevc_put_hevc_mc_pixelsX_X_sse
EPEL_FUNC( PUT_HEVC_EPEL_PIXELS,  2, 8)
EPEL_FUNC( PUT_HEVC_EPEL_PIXELS,  4, 8)
EPEL_FUNC( PUT_HEVC_EPEL_PIXELS,  8, 8)
EPEL_FUNC( PUT_HEVC_EPEL_PIXELS, 16, 8)

EPEL_FUNC( PUT_HEVC_EPEL_PIXELS,  2, 10)
EPEL_FUNC( PUT_HEVC_EPEL_PIXELS,  4, 10)
EPEL_FUNC( PUT_HEVC_EPEL_PIXELS,  8, 10)

EPEL_FUNC( PUT_HEVC_QPEL_PIXELS,  4,  8)
EPEL_FUNC( PUT_HEVC_QPEL_PIXELS,  8,  8)
EPEL_FUNC( PUT_HEVC_QPEL_PIXELS, 16,  8)

EPEL_FUNC( PUT_HEVC_QPEL_PIXELS,  4, 10)
EPEL_FUNC( PUT_HEVC_QPEL_PIXELS,  8, 10)

// ff_hevc_put_hevc_epel_hX_X_sse
EPEL_FUNC( PUT_HEVC_EPEL_H,  2,  8)
EPEL_FUNC( PUT_HEVC_EPEL_H,  4,  8)
EPEL_FUNC( PUT_HEVC_EPEL_H,  8,  8)

EPEL_FUNC( PUT_HEVC_EPEL_H,  2, 10)
EPEL_FUNC( PUT_HEVC_EPEL_H,  4, 10)

// ff_hevc_put_hevc_epel_vX_X_sse
EPEL_FUNC( PUT_HEVC_EPEL_V,  2, 8)
EPEL_FUNC( PUT_HEVC_EPEL_V,  4, 8)
EPEL_FUNC( PUT_HEVC_EPEL_V,  8, 8)
EPEL_FUNC( PUT_HEVC_EPEL_V, 16, 8)

EPEL_FUNC( PUT_HEVC_EPEL_V,  2, 10)
EPEL_FUNC( PUT_HEVC_EPEL_V,  4, 10)
EPEL_FUNC( PUT_HEVC_EPEL_V,  8, 10)

EPEL_FUNC( PUT_HEVC_EPEL_V,  2, 14)
EPEL_FUNC( PUT_HEVC_EPEL_V,  4, 14)
EPEL_FUNC( PUT_HEVC_EPEL_V,  8, 14)
/*
PUT_HEVC_EPEL_V( 2, 14)
PUT_HEVC_EPEL_V( 4, 14)
PUT_HEVC_EPEL_V( 8, 14)
*/
PUT_HEVC_EPEL_V_W_14b(2, 8, 0)
PUT_HEVC_EPEL_V_W_14b(4, 8, 0)
PUT_HEVC_EPEL_V_W_14b(8, 8, 0)
PUT_HEVC_EPEL_V_W_14b(2, 8, 1)
PUT_HEVC_EPEL_V_W_14b(4, 8, 1)
PUT_HEVC_EPEL_V_W_14b(8, 8, 1)
PUT_HEVC_EPEL_V_W_14b(2, 8, 2)
PUT_HEVC_EPEL_V_W_14b(4, 8, 2)
PUT_HEVC_EPEL_V_W_14b(8, 8, 2)
PUT_HEVC_EPEL_V_W_14b(2, 8, 3)
PUT_HEVC_EPEL_V_W_14b(4, 8, 3)
PUT_HEVC_EPEL_V_W_14b(8, 8, 3)

PUT_HEVC_EPEL_V_W_14b(2, 10, 0)
PUT_HEVC_EPEL_V_W_14b(4, 10, 0)
PUT_HEVC_EPEL_V_W_14b(8, 10, 0)
PUT_HEVC_EPEL_V_W_14b(2, 10, 1)
PUT_HEVC_EPEL_V_W_14b(4, 10, 1)
PUT_HEVC_EPEL_V_W_14b(8, 10, 1)
PUT_HEVC_EPEL_V_W_14b(2, 10, 2)
PUT_HEVC_EPEL_V_W_14b(4, 10, 2)
PUT_HEVC_EPEL_V_W_14b(8, 10, 2)
PUT_HEVC_EPEL_V_W_14b(2, 10, 3)
PUT_HEVC_EPEL_V_W_14b(4, 10, 3)
PUT_HEVC_EPEL_V_W_14b(8, 10, 3)


//EPEL_FUNC( PUT_HEVC_EPEL_V,  4, 14)
//EPEL_FUNC( PUT_HEVC_EPEL_V,  8, 14)

// ff_hevc_put_hevc_epel_hvX_X_sse


// ff_hevc_put_hevc_qpel_hX_X_X_sse
QPEL_FUNC( PUT_HEVC_QPEL_H,  4, 1,  8)
QPEL_FUNC( PUT_HEVC_QPEL_H,  4, 2,  8)
QPEL_FUNC( PUT_HEVC_QPEL_H,  4, 3,  8)

QPEL_FUNC( PUT_HEVC_QPEL_H,  8, 1,  8)
QPEL_FUNC( PUT_HEVC_QPEL_H,  8, 2,  8)
QPEL_FUNC( PUT_HEVC_QPEL_H,  8, 3,  8)

QPEL_FUNC( PUT_HEVC_QPEL_H,  4, 1, 10)
//QPEL_FUNC( PUT_HEVC_QPEL_H,  2, 2, 10)
//QPEL_FUNC( PUT_HEVC_QPEL_H,  2, 3, 10)

// ff_hevc_put_hevc_qpel_vX_X_X_sse
QPEL_FUNC( PUT_HEVC_QPEL_V,  4, 1,  8)
QPEL_FUNC( PUT_HEVC_QPEL_V,  4, 2,  8)
QPEL_FUNC( PUT_HEVC_QPEL_V,  4, 3,  8)

QPEL_FUNC( PUT_HEVC_QPEL_V,  8, 1,  8)
QPEL_FUNC( PUT_HEVC_QPEL_V,  8, 2,  8)
QPEL_FUNC( PUT_HEVC_QPEL_V,  8, 3,  8)

QPEL_FUNC( PUT_HEVC_QPEL_V, 16, 1,  8)
QPEL_FUNC( PUT_HEVC_QPEL_V, 16, 2,  8)
QPEL_FUNC( PUT_HEVC_QPEL_V, 16, 3,  8)

PUT_HEVC_QPEL_V_14b_W( 4, 1,  8, 0)
PUT_HEVC_QPEL_V_14b_W( 4, 2,  8, 0)
PUT_HEVC_QPEL_V_14b_W( 4, 3,  8, 0)

PUT_HEVC_QPEL_V_14b_W( 4, 1,  8, 1)
PUT_HEVC_QPEL_V_14b_W( 4, 2,  8, 1)
PUT_HEVC_QPEL_V_14b_W( 4, 3,  8, 1)

PUT_HEVC_QPEL_V_14b_W( 4, 1,  8, 2)
PUT_HEVC_QPEL_V_14b_W( 4, 2,  8, 2)
PUT_HEVC_QPEL_V_14b_W( 4, 3,  8, 2)

PUT_HEVC_QPEL_V_14b_W( 4, 1,  8, 3)
PUT_HEVC_QPEL_V_14b_W( 4, 2,  8, 3)
PUT_HEVC_QPEL_V_14b_W( 4, 3,  8, 3)

PUT_HEVC_QPEL_V_14b_W( 8, 1,  8, 0)
PUT_HEVC_QPEL_V_14b_W( 8, 2,  8, 0)
PUT_HEVC_QPEL_V_14b_W( 8, 3,  8, 0)

PUT_HEVC_QPEL_V_14b_W( 8, 1,  8, 1)
PUT_HEVC_QPEL_V_14b_W( 8, 2,  8, 1)
PUT_HEVC_QPEL_V_14b_W( 8, 3,  8, 1)

PUT_HEVC_QPEL_V_14b_W( 8, 1,  8, 2)
PUT_HEVC_QPEL_V_14b_W( 8, 2,  8, 2)
PUT_HEVC_QPEL_V_14b_W( 8, 3,  8, 2)

PUT_HEVC_QPEL_V_14b_W( 8, 1,  8, 3)
PUT_HEVC_QPEL_V_14b_W( 8, 2,  8, 3)
PUT_HEVC_QPEL_V_14b_W( 8, 3,  8, 3)


PUT_HEVC_QPEL_V( 4, 1, 14)
PUT_HEVC_QPEL_V( 4, 2, 14)
PUT_HEVC_QPEL_V( 4, 3, 14)

PUT_HEVC_QPEL_V( 8, 1, 14)
PUT_HEVC_QPEL_V( 8, 2, 14)
PUT_HEVC_QPEL_V( 8, 3, 14)


QPEL_FUNC( PUT_HEVC_QPEL_V,  4, 1, 10)
//QPEL_FUNC( PUT_HEVC_QPEL_V,  4, 2, 10)
//QPEL_FUNC( PUT_HEVC_QPEL_V,  4, 3, 10)

// ff_hevc_put_hevc_qpel_hvX_X_X_sse


//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 1, 1, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 1, 2, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 1, 3, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 2, 1, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 2, 2, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 2, 3, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 3, 1, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 3, 2, 10)
//QPEL_FUNC_HV( PUT_HEVC_QPEL_HV,  2, 3, 3, 10)
