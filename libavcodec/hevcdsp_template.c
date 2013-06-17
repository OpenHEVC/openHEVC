/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 Guillaume Martres
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "get_bits.h"
#include "bit_depth_template.c"
#include "hevcdata.h"
#include "hevcdsp.h"
#include "hevc.h"
//#define USE_SSE
#ifdef USE_SSE
#include <emmintrin.h>
#include <x86intrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#endif
#define shift_1st 7
#define add_1st (1 << (shift_1st - 1))
#define shift_2nd (20 - BIT_DEPTH)
#define add_2nd (1 << (shift_2nd - 1))

#if __GNUC__
//#define GCC_OPTIMIZATION_ENABLE
#endif
#define OPTIMIZATION_ENABLE

/*      SSE Optimizations     */
#ifdef USE_SSE
//#define SSE_TRANS_BYPASS no test video available
#define SSE_DEQUANT
#define SSE_MC
#define SSE_EPEL
#define SSE_unweight_pred
#define SSE_put_weight_pred
#define SSE_weight_pred
#define USE_SSE_4x4_Transform_LUMA
#define USE_SSE_4x4_Transform
#define USE_SSE_8x8_Transform
#define USE_SSE_16x16_Transform
#define USE_SSE_32x32_Transform
#endif


#define SET(dst, x) (dst) = (x)
#define SCALE(dst, x) (dst) = av_clip_int16(((x) + add) >> shift)
#define ADD_AND_SCALE(dst, x) (dst) = av_clip_pixel((dst) + av_clip_int16(((x) + add) >> shift))

static void FUNC(put_pcm)(uint8_t *_dst, ptrdiff_t _stride, int size,
                          GetBitContext *gb, int pcm_bit_depth)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++)
            dst[x] = get_bits(gb, pcm_bit_depth) << (BIT_DEPTH - pcm_bit_depth);
        dst += stride;
    }
}
#ifdef SSE_DEQUANT

static void FUNC(dequant4x4)(int16_t *coeffs, int qp)
{
    __m128i c0,c1,x0,x1,x2,x3,f0,f1,c2,c3;
    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };
  int y;
    //TODO: scaling_list_enabled_flag support
  int16_t coeffs2[16];
  int shift  = BIT_DEPTH -3;
  int scale  = level_scale[qp % 6] << (qp / 6);
  int add    = 1 << (shift - 1);
  int scale2 = scale << 4;  // > 16Bit
     //4x4 = 16 coeffs.

    f0= _mm_set1_epi32(scale2);

    f1= _mm_set1_epi32(add);
    c0= _mm_load_si128((__m128i*)&coeffs[0]); //loads 8 first values
    c2= _mm_load_si128((__m128i*)&coeffs[8]); //loads 8 last values

    c1= _mm_unpackhi_epi16(_mm_setzero_si128(),c0);
    c3= _mm_unpackhi_epi16(_mm_setzero_si128(),c2);
    c0= _mm_unpacklo_epi16(_mm_setzero_si128(),c0);
    c2= _mm_unpacklo_epi16(_mm_setzero_si128(),c2);
    c0= _mm_srai_epi32(c0,16);
    c1= _mm_srai_epi32(c1,16);
    c2= _mm_srai_epi32(c2,16);
    c3= _mm_srai_epi32(c3,16);


    c0= _mm_mullo_epi32(c0,f0);
    c1= _mm_mullo_epi32(c1,f0);
    c2= _mm_mullo_epi32(c2,f0);
    c3= _mm_mullo_epi32(c3,f0);


    c0= _mm_add_epi32(c0,f1);
    c1= _mm_add_epi32(c1,f1);
    c2= _mm_add_epi32(c2,f1);
    c3= _mm_add_epi32(c3,f1);


    c0= _mm_srai_epi32(c0,shift);
    c1= _mm_srai_epi32(c1,shift);
    c2= _mm_srai_epi32(c2,shift);
    c3= _mm_srai_epi32(c3,shift);


    c0= _mm_packs_epi32(c0,c1);
    c2= _mm_packs_epi32(c2,c3);

    _mm_store_si128(&coeffs[0], c0);
    _mm_store_si128(&coeffs[8], c2);


}


static void FUNC(dequant8x8)(int16_t *coeffs, int qp)
{
    __m128i c0,c1,c2,c3,c4,c5,c6,c7,f0,f1;
    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support
        int shift  = BIT_DEPTH - 2;
   int scale2  = level_scale[qp % 6] << ((qp / 6) + 4);
    int add    = 1 << (BIT_DEPTH - 3);

    //8x8= 64 coeffs.
   f0= _mm_set1_epi32(scale2);
    f1= _mm_set1_epi32(add);
    c0= _mm_load_si128((__m128i*)&coeffs[0]); //loads 8 first values
    c2= _mm_load_si128((__m128i*)&coeffs[8]);
    c4= _mm_load_si128((__m128i*)&coeffs[16]);
    c6= _mm_load_si128((__m128i*)&coeffs[24]);

    c1= _mm_unpackhi_epi16(_mm_setzero_si128(),c0);
    c3= _mm_unpackhi_epi16(_mm_setzero_si128(),c2);
    c5= _mm_unpackhi_epi16(_mm_setzero_si128(),c4);
    c7= _mm_unpackhi_epi16(_mm_setzero_si128(),c6);
    c0= _mm_unpacklo_epi16(_mm_setzero_si128(),c0);
    c2= _mm_unpacklo_epi16(_mm_setzero_si128(),c2);
    c4= _mm_unpacklo_epi16(_mm_setzero_si128(),c4);
    c6= _mm_unpacklo_epi16(_mm_setzero_si128(),c6);
    c0= _mm_srai_epi32(c0,16);
    c1= _mm_srai_epi32(c1,16);
    c2= _mm_srai_epi32(c2,16);
    c3= _mm_srai_epi32(c3,16);
    c4= _mm_srai_epi32(c4,16);
    c5= _mm_srai_epi32(c5,16);
    c6= _mm_srai_epi32(c6,16);
    c7= _mm_srai_epi32(c7,16);


    c0= _mm_mullo_epi32(c0,f0);
    c1= _mm_mullo_epi32(c1,f0);
    c2= _mm_mullo_epi32(c2,f0);
    c3= _mm_mullo_epi32(c3,f0);
    c4= _mm_mullo_epi32(c4,f0);
    c5= _mm_mullo_epi32(c5,f0);
    c6= _mm_mullo_epi32(c6,f0);
    c7= _mm_mullo_epi32(c7,f0);
    c0= _mm_add_epi32(c0,f1);
    c1= _mm_add_epi32(c1,f1);
    c2= _mm_add_epi32(c2,f1);
    c3= _mm_add_epi32(c3,f1);
    c4= _mm_add_epi32(c4,f1);
    c5= _mm_add_epi32(c5,f1);
    c6= _mm_add_epi32(c6,f1);
    c7= _mm_add_epi32(c7,f1);


    c0= _mm_srai_epi32(c0,shift);
    c2= _mm_srai_epi32(c2,shift);
    c4= _mm_srai_epi32(c4,shift);
    c6= _mm_srai_epi32(c6,shift);
    c1= _mm_srai_epi32(c1,shift);
    c3= _mm_srai_epi32(c3,shift);
    c5= _mm_srai_epi32(c5,shift);
    c7= _mm_srai_epi32(c7,shift);


    c0= _mm_packs_epi32(c0,c1);
    c2= _mm_packs_epi32(c2,c3);
    c4= _mm_packs_epi32(c4,c5);
    c6= _mm_packs_epi32(c6,c7);

    _mm_store_si128(&coeffs[0], c0);
    _mm_store_si128(&coeffs[8], c2);
    _mm_store_si128(&coeffs[16], c4);
    _mm_store_si128(&coeffs[24], c6);

    c0= _mm_load_si128((__m128i*)&coeffs[32]);
    c2= _mm_load_si128((__m128i*)&coeffs[40]);
    c4= _mm_load_si128((__m128i*)&coeffs[48]);
    c6= _mm_load_si128((__m128i*)&coeffs[56]);

    c1= _mm_unpackhi_epi16(_mm_setzero_si128(),c0);
    c3= _mm_unpackhi_epi16(_mm_setzero_si128(),c2);
    c5= _mm_unpackhi_epi16(_mm_setzero_si128(),c4);
    c7= _mm_unpackhi_epi16(_mm_setzero_si128(),c6);
    c0= _mm_unpacklo_epi16(_mm_setzero_si128(),c0);
    c2= _mm_unpacklo_epi16(_mm_setzero_si128(),c2);
    c4= _mm_unpacklo_epi16(_mm_setzero_si128(),c4);
    c6= _mm_unpacklo_epi16(_mm_setzero_si128(),c6);
    c0= _mm_srai_epi32(c0,16);
    c1= _mm_srai_epi32(c1,16);
    c2= _mm_srai_epi32(c2,16);
    c3= _mm_srai_epi32(c3,16);
    c4= _mm_srai_epi32(c4,16);
    c5= _mm_srai_epi32(c5,16);
    c6= _mm_srai_epi32(c6,16);
    c7= _mm_srai_epi32(c7,16);


    c0= _mm_mullo_epi32(c0,f0);
    c1= _mm_mullo_epi32(c1,f0);
    c2= _mm_mullo_epi32(c2,f0);
    c3= _mm_mullo_epi32(c3,f0);
    c4= _mm_mullo_epi32(c4,f0);
    c5= _mm_mullo_epi32(c5,f0);
    c6= _mm_mullo_epi32(c6,f0);
    c7= _mm_mullo_epi32(c7,f0);
    c0= _mm_add_epi32(c0,f1);
    c1= _mm_add_epi32(c1,f1);
    c2= _mm_add_epi32(c2,f1);
    c3= _mm_add_epi32(c3,f1);
    c4= _mm_add_epi32(c4,f1);
    c5= _mm_add_epi32(c5,f1);
    c6= _mm_add_epi32(c6,f1);
    c7= _mm_add_epi32(c7,f1);


    c0= _mm_srai_epi32(c0,shift);
    c2= _mm_srai_epi32(c2,shift);
    c4= _mm_srai_epi32(c4,shift);
    c6= _mm_srai_epi32(c6,shift);
    c1= _mm_srai_epi32(c1,shift);
    c3= _mm_srai_epi32(c3,shift);
    c5= _mm_srai_epi32(c5,shift);
    c7= _mm_srai_epi32(c7,shift);


    c0= _mm_packs_epi32(c0,c1);
    c2= _mm_packs_epi32(c2,c3);
    c4= _mm_packs_epi32(c4,c5);
    c6= _mm_packs_epi32(c6,c7);

    _mm_store_si128(&coeffs[32], c0);
    _mm_store_si128(&coeffs[40], c2);
    _mm_store_si128(&coeffs[48], c4);
    _mm_store_si128(&coeffs[56], c6);
}

static void FUNC(dequant16x16)(int16_t *coeffs, int qp)
{
    int x, y;
    int size = 16;
    __m128i c0,c1,c2,c3,c4,c5,c6,c7,f0,f1;

    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support

    int shift  = BIT_DEPTH -1;
    int scale2  = level_scale[qp % 6] << ((qp / 6) + 4);
    int add    = 1 << (BIT_DEPTH - 2);
    f0= _mm_set1_epi32(scale2);
     f1= _mm_set1_epi32(add);
    for(x= 0; x< 16*16 ; x+=64)
    {
        c0= _mm_load_si128((__m128i*)&coeffs[0+x]); //loads 8 first values
        c2= _mm_load_si128((__m128i*)&coeffs[8+x]);
        c4= _mm_load_si128((__m128i*)&coeffs[16+x]);
        c6= _mm_load_si128((__m128i*)&coeffs[24+x]);

        c1= _mm_unpackhi_epi16(_mm_setzero_si128(),c0);
        c3= _mm_unpackhi_epi16(_mm_setzero_si128(),c2);
        c5= _mm_unpackhi_epi16(_mm_setzero_si128(),c4);
        c7= _mm_unpackhi_epi16(_mm_setzero_si128(),c6);
        c0= _mm_unpacklo_epi16(_mm_setzero_si128(),c0);
        c2= _mm_unpacklo_epi16(_mm_setzero_si128(),c2);
        c4= _mm_unpacklo_epi16(_mm_setzero_si128(),c4);
        c6= _mm_unpacklo_epi16(_mm_setzero_si128(),c6);
        c0= _mm_srai_epi32(c0,16);
        c1= _mm_srai_epi32(c1,16);
        c2= _mm_srai_epi32(c2,16);
        c3= _mm_srai_epi32(c3,16);
        c4= _mm_srai_epi32(c4,16);
        c5= _mm_srai_epi32(c5,16);
        c6= _mm_srai_epi32(c6,16);
        c7= _mm_srai_epi32(c7,16);


        c0= _mm_mullo_epi32(c0,f0);
        c1= _mm_mullo_epi32(c1,f0);
        c2= _mm_mullo_epi32(c2,f0);
        c3= _mm_mullo_epi32(c3,f0);
        c4= _mm_mullo_epi32(c4,f0);
        c5= _mm_mullo_epi32(c5,f0);
        c6= _mm_mullo_epi32(c6,f0);
        c7= _mm_mullo_epi32(c7,f0);
        c0= _mm_add_epi32(c0,f1);
        c1= _mm_add_epi32(c1,f1);
        c2= _mm_add_epi32(c2,f1);
        c3= _mm_add_epi32(c3,f1);
        c4= _mm_add_epi32(c4,f1);
        c5= _mm_add_epi32(c5,f1);
        c6= _mm_add_epi32(c6,f1);
        c7= _mm_add_epi32(c7,f1);


        c0= _mm_srai_epi32(c0,shift);
        c2= _mm_srai_epi32(c2,shift);
        c4= _mm_srai_epi32(c4,shift);
        c6= _mm_srai_epi32(c6,shift);
        c1= _mm_srai_epi32(c1,shift);
        c3= _mm_srai_epi32(c3,shift);
        c5= _mm_srai_epi32(c5,shift);
        c7= _mm_srai_epi32(c7,shift);


        c0= _mm_packs_epi32(c0,c1);
        c2= _mm_packs_epi32(c2,c3);
        c4= _mm_packs_epi32(c4,c5);
        c6= _mm_packs_epi32(c6,c7);

        _mm_store_si128(&coeffs[0+x], c0);
        _mm_store_si128(&coeffs[8+x], c2);
        _mm_store_si128(&coeffs[16+x], c4);
        _mm_store_si128(&coeffs[24+x], c6);

        c0= _mm_load_si128((__m128i*)&coeffs[32+x]);
        c2= _mm_load_si128((__m128i*)&coeffs[40+x]);
        c4= _mm_load_si128((__m128i*)&coeffs[48+x]);
        c6= _mm_load_si128((__m128i*)&coeffs[56+x]);

        c1= _mm_unpackhi_epi16(_mm_setzero_si128(),c0);
        c3= _mm_unpackhi_epi16(_mm_setzero_si128(),c2);
        c5= _mm_unpackhi_epi16(_mm_setzero_si128(),c4);
        c7= _mm_unpackhi_epi16(_mm_setzero_si128(),c6);
        c0= _mm_unpacklo_epi16(_mm_setzero_si128(),c0);
        c2= _mm_unpacklo_epi16(_mm_setzero_si128(),c2);
        c4= _mm_unpacklo_epi16(_mm_setzero_si128(),c4);
        c6= _mm_unpacklo_epi16(_mm_setzero_si128(),c6);
        c0= _mm_srai_epi32(c0,16);
        c1= _mm_srai_epi32(c1,16);
        c2= _mm_srai_epi32(c2,16);
        c3= _mm_srai_epi32(c3,16);
        c4= _mm_srai_epi32(c4,16);
        c5= _mm_srai_epi32(c5,16);
        c6= _mm_srai_epi32(c6,16);
        c7= _mm_srai_epi32(c7,16);


        c0= _mm_mullo_epi32(c0,f0);
        c1= _mm_mullo_epi32(c1,f0);
        c2= _mm_mullo_epi32(c2,f0);
        c3= _mm_mullo_epi32(c3,f0);
        c4= _mm_mullo_epi32(c4,f0);
        c5= _mm_mullo_epi32(c5,f0);
        c6= _mm_mullo_epi32(c6,f0);
        c7= _mm_mullo_epi32(c7,f0);

        c0= _mm_add_epi32(c0,f1);
        c1= _mm_add_epi32(c1,f1);
        c2= _mm_add_epi32(c2,f1);
        c3= _mm_add_epi32(c3,f1);
        c4= _mm_add_epi32(c4,f1);
        c5= _mm_add_epi32(c5,f1);
        c6= _mm_add_epi32(c6,f1);
        c7= _mm_add_epi32(c7,f1);

        c0= _mm_srai_epi32(c0,shift);
        c1= _mm_srai_epi32(c1,shift);
        c2= _mm_srai_epi32(c2,shift);
        c3= _mm_srai_epi32(c3,shift);
        c4= _mm_srai_epi32(c4,shift);
        c5= _mm_srai_epi32(c5,shift);
        c6= _mm_srai_epi32(c6,shift);
        c7= _mm_srai_epi32(c7,shift);

        c0= _mm_packs_epi32(c0,c1);
        c2= _mm_packs_epi32(c2,c3);
        c4= _mm_packs_epi32(c4,c5);
        c6= _mm_packs_epi32(c6,c7);

        _mm_store_si128(&coeffs[32+x], c0);
        _mm_store_si128(&coeffs[40+x], c2);
        _mm_store_si128(&coeffs[48+x], c4);
        _mm_store_si128(&coeffs[56+x], c6);

    }


}

static void FUNC(dequant32x32)(int16_t *coeffs, int qp)
{
    int x, y;
    int size = 32;
    __m128i c0,c1,c2,c3,c4,c5,c6,c7,f0,f1;

    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support

    int shift  = BIT_DEPTH;
    int scale2  = level_scale[qp % 6] << ((qp / 6) + 4);
    int add    = 1 << (BIT_DEPTH - 1);
    f0= _mm_set1_epi32(scale2);
     f1= _mm_set1_epi32(add);
    for(x= 0; x< 32*32 ; x+=64)
    {

         c0= _mm_load_si128((__m128i*)&coeffs[0+x]); //loads 8 first values
         c2= _mm_load_si128((__m128i*)&coeffs[8+x]);
         c4= _mm_load_si128((__m128i*)&coeffs[16+x]);
         c6= _mm_load_si128((__m128i*)&coeffs[24+x]);

         c1= _mm_unpackhi_epi16(_mm_setzero_si128(),c0);
         c3= _mm_unpackhi_epi16(_mm_setzero_si128(),c2);
         c5= _mm_unpackhi_epi16(_mm_setzero_si128(),c4);
         c7= _mm_unpackhi_epi16(_mm_setzero_si128(),c6);
         c0= _mm_unpacklo_epi16(_mm_setzero_si128(),c0);
         c2= _mm_unpacklo_epi16(_mm_setzero_si128(),c2);
         c4= _mm_unpacklo_epi16(_mm_setzero_si128(),c4);
         c6= _mm_unpacklo_epi16(_mm_setzero_si128(),c6);
         c0= _mm_srai_epi32(c0,16);
         c1= _mm_srai_epi32(c1,16);
         c2= _mm_srai_epi32(c2,16);
         c3= _mm_srai_epi32(c3,16);
         c4= _mm_srai_epi32(c4,16);
         c5= _mm_srai_epi32(c5,16);
         c6= _mm_srai_epi32(c6,16);
         c7= _mm_srai_epi32(c7,16);


         c0= _mm_mullo_epi32(c0,f0);
         c1= _mm_mullo_epi32(c1,f0);
         c2= _mm_mullo_epi32(c2,f0);
         c3= _mm_mullo_epi32(c3,f0);
         c4= _mm_mullo_epi32(c4,f0);
         c5= _mm_mullo_epi32(c5,f0);
         c6= _mm_mullo_epi32(c6,f0);
         c7= _mm_mullo_epi32(c7,f0);
         c0= _mm_add_epi32(c0,f1);
         c1= _mm_add_epi32(c1,f1);
         c2= _mm_add_epi32(c2,f1);
         c3= _mm_add_epi32(c3,f1);
         c4= _mm_add_epi32(c4,f1);
         c5= _mm_add_epi32(c5,f1);
         c6= _mm_add_epi32(c6,f1);
         c7= _mm_add_epi32(c7,f1);


         c0= _mm_srai_epi32(c0,shift);
         c2= _mm_srai_epi32(c2,shift);
         c4= _mm_srai_epi32(c4,shift);
         c6= _mm_srai_epi32(c6,shift);
         c1= _mm_srai_epi32(c1,shift);
         c3= _mm_srai_epi32(c3,shift);
         c5= _mm_srai_epi32(c5,shift);
         c7= _mm_srai_epi32(c7,shift);


         c0= _mm_packs_epi32(c0,c1);
         c2= _mm_packs_epi32(c2,c3);
         c4= _mm_packs_epi32(c4,c5);
         c6= _mm_packs_epi32(c6,c7);

         _mm_store_si128(&coeffs[0+x], c0);
         _mm_store_si128(&coeffs[8+x], c2);
         _mm_store_si128(&coeffs[16+x], c4);
         _mm_store_si128(&coeffs[24+x], c6);

         c0= _mm_load_si128((__m128i*)&coeffs[32+x]);
         c2= _mm_load_si128((__m128i*)&coeffs[40+x]);
         c4= _mm_load_si128((__m128i*)&coeffs[48+x]);
         c6= _mm_load_si128((__m128i*)&coeffs[56+x]);

         c1= _mm_unpackhi_epi16(_mm_setzero_si128(),c0);
         c3= _mm_unpackhi_epi16(_mm_setzero_si128(),c2);
         c5= _mm_unpackhi_epi16(_mm_setzero_si128(),c4);
         c7= _mm_unpackhi_epi16(_mm_setzero_si128(),c6);
         c0= _mm_unpacklo_epi16(_mm_setzero_si128(),c0);
         c2= _mm_unpacklo_epi16(_mm_setzero_si128(),c2);
         c4= _mm_unpacklo_epi16(_mm_setzero_si128(),c4);
         c6= _mm_unpacklo_epi16(_mm_setzero_si128(),c6);
         c0= _mm_srai_epi32(c0,16);
         c1= _mm_srai_epi32(c1,16);
         c2= _mm_srai_epi32(c2,16);
         c3= _mm_srai_epi32(c3,16);
         c4= _mm_srai_epi32(c4,16);
         c5= _mm_srai_epi32(c5,16);
         c6= _mm_srai_epi32(c6,16);
         c7= _mm_srai_epi32(c7,16);


         c0= _mm_mullo_epi32(c0,f0);
         c1= _mm_mullo_epi32(c1,f0);
         c2= _mm_mullo_epi32(c2,f0);
         c3= _mm_mullo_epi32(c3,f0);
         c4= _mm_mullo_epi32(c4,f0);
         c5= _mm_mullo_epi32(c5,f0);
         c6= _mm_mullo_epi32(c6,f0);
         c7= _mm_mullo_epi32(c7,f0);

         c0= _mm_add_epi32(c0,f1);
         c1= _mm_add_epi32(c1,f1);
         c2= _mm_add_epi32(c2,f1);
         c3= _mm_add_epi32(c3,f1);
         c4= _mm_add_epi32(c4,f1);
         c5= _mm_add_epi32(c5,f1);
         c6= _mm_add_epi32(c6,f1);
         c7= _mm_add_epi32(c7,f1);

         c0= _mm_srai_epi32(c0,shift);
         c1= _mm_srai_epi32(c1,shift);
         c2= _mm_srai_epi32(c2,shift);
         c3= _mm_srai_epi32(c3,shift);
         c4= _mm_srai_epi32(c4,shift);
         c5= _mm_srai_epi32(c5,shift);
         c6= _mm_srai_epi32(c6,shift);
         c7= _mm_srai_epi32(c7,shift);

         c0= _mm_packs_epi32(c0,c1);
         c2= _mm_packs_epi32(c2,c3);
         c4= _mm_packs_epi32(c4,c5);
         c6= _mm_packs_epi32(c6,c7);

         _mm_store_si128(&coeffs[32+x], c0);
         _mm_store_si128(&coeffs[40+x], c2);
         _mm_store_si128(&coeffs[48+x], c4);
         _mm_store_si128(&coeffs[56+x], c6);
    }
}

#else
static void FUNC(dequant4x4)(int16_t *coeffs, int qp)
{
    int x, y;
    int size = 4;

    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support

    int shift  = BIT_DEPTH -3;
    int scale  = level_scale[qp % 6] << (qp / 6);
    int add    = 1 << (shift - 1);
    int scale2 = scale << 4;
            for (y = 0; y < 4*4; y+=4) {
                SCALE(coeffs[y+0], (coeffs[y+0] * scale2));
                SCALE(coeffs[y+1], (coeffs[y+1] * scale2));
                SCALE(coeffs[y+2], (coeffs[y+2] * scale2));
                SCALE(coeffs[y+3], (coeffs[y+3] * scale2));
            }

}
static void FUNC(dequant8x8)(int16_t *coeffs, int qp)
{
    int x, y;
    int size = 8;

    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support

    int shift  = BIT_DEPTH - 2;
    int scale  = level_scale[qp % 6] << (qp / 6);
    int add    = 1 << (shift - 1);
    int scale2 = scale << 4;
#define FILTER()                                    \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2))

            for (y = 0; y < 8*8; y+=8) {
                for (x = 0; x < 8; x++) {
                    FILTER();
                }
            }
#undef FILTER
}

static void FUNC(dequant16x16)(int16_t *coeffs, int qp)
{
    int x, y;
    int size = 16;

    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support

    int shift  = BIT_DEPTH -1;
    int scale  = level_scale[qp % 6] << (qp / 6);
    int add    = 1 << (shift - 1);
    int scale2 = scale << 4;
#define FILTER()                                    \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2))
            for (y = 0; y < 16*16; y+=16) {
                for (x = 0; x < 16; x++) {
                    FILTER();
                }
            }
#undef FILTER
}

static void FUNC(dequant32x32)(int16_t *coeffs, int qp)
{
    int x, y;
    int size = 32;

    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support

    int shift  = BIT_DEPTH;
    int scale  = level_scale[qp % 6] << (qp / 6);
    int add    = 1 << (shift - 1);
    int scale2 = scale << 4;
#define FILTER()                                    \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2)); \
        x++;                                        \
        SCALE(coeffs[y+x], (coeffs[y+x] * scale2))
            for (y = 0; y < 32*32; y+=32) {
                for (x = 0; x < 32; x++) {
                    FILTER();
                }
            }

#undef FILTER
}
#endif

#ifdef SSE_TRANS_BYPASS
static void FUNC(transquant_bypass4x4)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    uint8_t x, y;
    __m128i d0,d1,d2,d3,c0,c1,x2,x3,x4, bshuffle1;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    printf("stride = %d \n",stride);
    bshuffle1=_mm_set_epi8(15,13,11,9,7,5,3,1,14,12,10,8,6,4,2,0);
    d0= _mm_loadu_si128((__m128i*)&dst[0]);
    d1= _mm_loadu_si128((__m128i*)&dst[stride]);
    d2= _mm_loadu_si128((__m128i*)&dst[2*stride]);
    d3= _mm_loadu_si128((__m128i*)&dst[3*stride]);
    c0= _mm_loadu_si128((__m128i*)&coeffs[0]);
    c1= _mm_loadu_si128((__m128i*)&coeffs[8]);

    x2= _mm_unpacklo_epi64(d0,d2);
    x3= _mm_unpacklo_epi64(d1,d3);
    x4= _mm_unpacklo_epi32(x2,x3);


    c0= _mm_unpacklo_epi8(c0,c1);
    c0= _mm_shuffle_epi8(c0,bshuffle1);
    d0= _mm_adds_epi8(c0,x4);
    y=0;

        dst[0]= _mm_extract_epi8(d0,0);
        dst[1]= _mm_extract_epi8(d0,1);
        dst[2]= _mm_extract_epi8(d0,2);
        dst[3]= _mm_extract_epi8(d0,3);
        dst[stride]= _mm_extract_epi8(d0,4);
        dst[1+stride]= _mm_extract_epi8(d0,5);
        dst[2+stride]= _mm_extract_epi8(d0,6);
        dst[3+stride]= _mm_extract_epi8(d0,7);
        dst[2*stride]= _mm_extract_epi8(d0,8);
        dst[1+2*stride]= _mm_extract_epi8(d0,9);
        dst[2+2*stride]= _mm_extract_epi8(d0,10);
        dst[3+2*stride]= _mm_extract_epi8(d0,11);
        dst[0+3*stride]= _mm_extract_epi8(d0,12);
        dst[1+3*stride]= _mm_extract_epi8(d0,13);
        dst[2+3*stride]= _mm_extract_epi8(d0,14);
        dst[3+3*stride]= _mm_extract_epi8(d0,15);
    }



static void FUNC(transquant_bypass8x8)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << 3;
            for (y = 0; y < 8; y++) {
                for (x = 0; x < 8; x++) {
                    dst[x] += *coeffs;
                    coeffs++;
                }
                dst += stride;
            }
}

static void FUNC(transquant_bypass16x16)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << 4;
            for (y = 0; y < 16; y++) {
                for (x = 0; x < 16; x++) {
                    dst[x] += *coeffs;
                    coeffs++;
                }
                dst += stride;
            }

}

static void FUNC(transquant_bypass32x32)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << 5;
            for (y = 0; y < 32; y++) {
                for (x = 0; x < 32; x++) {
                    dst[x] += *coeffs;
                    coeffs++;
                }
                dst += stride;
            }

}
#else
static void FUNC(transquant_bypass4x4)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << 2;

            for (y = 0; y < 4; y++) {
                for (x = 0; x < 4; x++) {
                    dst[x] += *coeffs;
                    coeffs++;
                }
                dst += stride;
            }

}

static void FUNC(transquant_bypass8x8)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << 3;

            for (y = 0; y < 8; y++) {
                for (x = 0; x < 8; x++) {
                    dst[x] += *coeffs;
                    coeffs++;
                }
                dst += stride;
            }
}

static void FUNC(transquant_bypass16x16)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << 4;

            for (y = 0; y < 16; y++) {
                for (x = 0; x < 16; x++) {
                    dst[x] += *coeffs;
                    coeffs++;
                }
                dst += stride;
            }

}

static void FUNC(transquant_bypass32x32)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << 5;

            for (y = 0; y < 32; y++) {
                for (x = 0; x < 32; x++) {
                    dst[x] += *coeffs;
                    coeffs++;
                }
                dst += stride;
            }

}

#endif

static void FUNC(transform_skip)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 4;
    int shift = 13 - BIT_DEPTH;
#if BIT_DEPTH <= 13
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif
    int x, y;
    switch (size){
        case 32:
            for (y = 0; y < 32*32; y+=32) {
                for (x = 0; x < 32; x++) {
                    dst[x] = av_clip_pixel(dst[x] + ((coeffs[y + x] + offset) >> shift));
                }
                dst += stride;
            }
            break;
        case 16:
            for (y = 0; y < 16*16; y+=16) {
                for (x = 0; x < 16; x++) {
                    dst[x] = av_clip_pixel(dst[x] + ((coeffs[y + x] + offset) >> shift));
                }
                dst += stride;
            }
            break;
        case 8:
            for (y = 0; y < 8*8; y+=8) {
                for (x = 0; x < 8; x++) {
                    dst[x] = av_clip_pixel(dst[x] + ((coeffs[y + x] + offset) >> shift));
                }
                dst += stride;
            }
            break;
        case 4:
            for (y = 0; y < 4*4; y+=4) {
                for (x = 0; x < 4; x++) {
                    dst[x] = av_clip_pixel(dst[x] + ((coeffs[y + x] + offset) >> shift));
                }
                dst += stride;
            }
            break;
    }
#undef FILTER
}

static void FUNC(transform_4x4_luma_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int16_t *src = coeffs;
#ifdef USE_SSE_4x4_Transform_LUMA
    int j;
    __m128i m128iAdd, S0, S8, m128iTmp1, m128iTmp2, m128iAC, m128iBD, m128iA, m128iD;
    m128iAdd  = _mm_set1_epi32( add_1st );
    
    S0  = _mm_load_si128   ( (__m128i*)( src      ) );
    S8  = _mm_load_si128   ( (__m128i*)( src + 8  ) );
    
    m128iAC  = _mm_unpacklo_epi16( S0 , S8 );
    m128iBD  = _mm_unpackhi_epi16( S0 , S8 );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[0] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[1] ) ) );
    S0   = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S0   = _mm_add_epi32( S0, m128iAdd );
    S0   = _mm_srai_epi32( S0, shift_1st  );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[2] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[3] ) ) );
    S8   = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S8   = _mm_add_epi32( S8, m128iAdd );
    S8   = _mm_srai_epi32( S8, shift_1st  );
    
    m128iA = _mm_packs_epi32( S0, S8 );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[4] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[5] ) ) );
    S0  = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S0  = _mm_add_epi32( S0, m128iAdd );
    S0  = _mm_srai_epi32( S0, shift_1st  );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[6] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[7] ) ) );
    S8  = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S8  = _mm_add_epi32( S8, m128iAdd );
    S8  = _mm_srai_epi32( S8, shift_1st  );
    
    m128iD = _mm_packs_epi32( S0, S8 );
    
    S0 =_mm_unpacklo_epi16(  m128iA, m128iD );
    S8 =_mm_unpackhi_epi16(  m128iA, m128iD );
    
    m128iA =_mm_unpacklo_epi16(  S0, S8 );
    m128iD =_mm_unpackhi_epi16(  S0, S8 );
    
    /*   ###################    */
    m128iAdd  = _mm_set1_epi32( add_2nd );
    
    m128iAC  = _mm_unpacklo_epi16( m128iA , m128iD );
    m128iBD  = _mm_unpackhi_epi16( m128iA , m128iD );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[0] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[1] ) ) );
    S0   = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S0   = _mm_add_epi32( S0, m128iAdd );
    S0   = _mm_srai_epi32( S0, shift_2nd  );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[2] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[3] ) ) );
    S8   = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S8   = _mm_add_epi32( S8, m128iAdd );
    S8   = _mm_srai_epi32( S8, shift_2nd  );
    
    m128iA = _mm_packs_epi32( S0, S8 );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[4] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[5] ) ) );
    S0  = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S0  = _mm_add_epi32( S0, m128iAdd );
    S0  = _mm_srai_epi32( S0, shift_2nd  );
    
    m128iTmp1 = _mm_madd_epi16( m128iAC, _mm_load_si128( (__m128i*)( transform4x4_luma[6] ) ) );
    m128iTmp2 = _mm_madd_epi16( m128iBD, _mm_load_si128( (__m128i*)( transform4x4_luma[7] ) ) );
    S8  = _mm_add_epi32( m128iTmp1, m128iTmp2 );
    S8  = _mm_add_epi32( S8, m128iAdd );
    S8  = _mm_srai_epi32( S8, shift_2nd  );
    
    m128iD = _mm_packs_epi32( S0, S8 );
    
    _mm_storeu_si128( (__m128i*)( src     ), m128iA );
    _mm_storeu_si128( (__m128i*)( src + 8 ), m128iD );
    j = 0;
    for (i = 0; i < 2; i++){
        dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[j]));
        dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[j+4]));
        dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[j+8]));
        dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[j+12]));
        j +=1;
        dst += stride;
        dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[j]));
        dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[j+4]));
        dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[j+8]));
        dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[j+12]));
        j +=1;
        dst += stride;
    }
#else
    
#define TR_4x4_LUMA(dst, src, step, assign)                                     \
do {                                                                        \
int c0 = src[0*step] + src[2*step];                                     \
int c1 = src[2*step] + src[3*step];                                     \
int c2 = src[0*step] - src[3*step];                                     \
int c3 = 74 * src[1*step];                                              \
\
assign(dst[2*step], 74 * (src[0*step] - src[2*step] + src[3*step]));    \
assign(dst[0*step], 29 * c0 + 55 * c1 + c3);                            \
assign(dst[1*step], 55 * c2 - 29 * c1 + c3);                            \
assign(dst[3*step], 55 * c0 + 29 * c2 - c3);                            \
} while (0)
    
    int shift = 7;
    int add = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(src, src, 4, SCALE);
        src++;
    }
    
    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(dst, coeffs, 1, ADD_AND_SCALE);
        coeffs += 4;
        dst += stride;
    }
    
#undef TR_4x4_LUMA
#endif
}

#define TR_4(dst, src, dstep, sstep, assign)                                    \
    do {                                                                        \
        const int e0 = transform[8*0][0] * src[0*sstep] +                       \
                       transform[8*2][0] * src[2*sstep];                        \
        const int e1 = transform[8*0][1] * src[0*sstep] +                       \
                       transform[8*2][1] * src[2*sstep];                        \
        const int o0 = transform[8*1][0] * src[1*sstep] +                       \
                       transform[8*3][0] * src[3*sstep];                        \
        const int o1 = transform[8*1][1] * src[1*sstep] +                       \
                       transform[8*3][1] * src[3*sstep];                        \
                                                                                \
        assign(dst[0*dstep], e0 + o0);                                          \
        assign(dst[1*dstep], e1 + o1);                                          \
        assign(dst[2*dstep], e1 - o1);                                          \
        assign(dst[3*dstep], e0 - o0);                                          \
    } while (0)
#define TR_4_1(dst, src) TR_4(dst, src, 4, 4, SCALE)
#define TR_4_2(dst, src) TR_4(dst, src, 1, 1, ADD_AND_SCALE)

static void FUNC(transform_4x4_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int16_t *src = coeffs;
#ifdef USE_SSE_4x4_Transform
    int j;
    __m128i S0, S8, m128iAdd, m128Tmp, E1, E2, O1, O2, m128iA, m128iD;
    S0   = _mm_load_si128( (__m128i*)( src     ) );
    S8   = _mm_load_si128( (__m128i*)( src + 8 ) );
    m128iAdd  = _mm_set1_epi32( add_1st );
    
    m128Tmp = _mm_unpacklo_epi16(  S0, S8 );
    E1 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[0] ) ) );
    E1 = _mm_add_epi32( E1, m128iAdd );
    
    E2 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[1] ) ) );
    E2 = _mm_add_epi32( E2, m128iAdd );
    
    
    m128Tmp = _mm_unpackhi_epi16(  S0, S8 );
    O1 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[2] ) ) );
    O2 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[3] ) ) );
    
    m128iA  = _mm_add_epi32( E1, O1 );
    m128iA  = _mm_srai_epi32( m128iA, shift_1st  );        // Sum = Sum >> iShiftNum
    m128Tmp = _mm_add_epi32( E2, O2 );
    m128Tmp = _mm_srai_epi32( m128Tmp, shift_1st  );       // Sum = Sum >> iShiftNum
    m128iA = _mm_packs_epi32( m128iA, m128Tmp);
    
    
    
    
    m128iD = _mm_sub_epi32( E2, O2 );
    m128iD = _mm_srai_epi32( m128iD, shift_1st  );         // Sum = Sum >> iShiftNum
    
    m128Tmp = _mm_sub_epi32( E1, O1 );
    m128Tmp = _mm_srai_epi32( m128Tmp, shift_1st  );       // Sum = Sum >> iShiftNum
    
    m128iD = _mm_packs_epi32( m128iD, m128Tmp );
    
    S0 =_mm_unpacklo_epi16(  m128iA, m128iD );
    S8 =_mm_unpackhi_epi16(  m128iA, m128iD );
    
    m128iA =_mm_unpacklo_epi16(  S0, S8 );
    m128iD =_mm_unpackhi_epi16(  S0, S8 );
    
    /*  ##########################  */
    
    
    m128iAdd  = _mm_set1_epi32( add_2nd );
    m128Tmp = _mm_unpacklo_epi16(  m128iA, m128iD );
    E1 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[0] ) ) );
    E1 = _mm_add_epi32( E1, m128iAdd );
    
    E2 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[1] ) ) );
    E2 = _mm_add_epi32( E2, m128iAdd );
    
    
    m128Tmp = _mm_unpackhi_epi16(  m128iA, m128iD );
    O1 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[2] ) ) );
    O2 = _mm_madd_epi16( m128Tmp, _mm_load_si128( (__m128i*)( transform4x4[3] ) ) );
    
    m128iA  = _mm_add_epi32( E1, O1 );
    m128iA  = _mm_srai_epi32( m128iA, shift_2nd  );
    m128Tmp = _mm_add_epi32( E2, O2 );
    m128Tmp = _mm_srai_epi32( m128Tmp, shift_2nd  );
    m128iA = _mm_packs_epi32( m128iA, m128Tmp);
    
    m128iD = _mm_sub_epi32( E2, O2 );
    m128iD = _mm_srai_epi32( m128iD, shift_2nd  );
    
    m128Tmp = _mm_sub_epi32( E1, O1 );
    m128Tmp = _mm_srai_epi32( m128Tmp, shift_2nd  );
    
    m128iD = _mm_packs_epi32( m128iD, m128Tmp );
    _mm_storeu_si128( (__m128i*)( src     ), m128iA );
    _mm_storeu_si128( (__m128i*)( src + 8 ), m128iD );
    j = 0;
    for (i = 0; i < 2; i++){
        dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[j]));
        dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[j+4]));
        dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[j+8]));
        dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[j+12]));
        j +=1;
        dst += stride;
        dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[j]));
        dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[j+4]));
        dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[j+8]));
        dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[j+12]));
        j +=1;
        dst += stride;
    }
#else
    int shift = 7;
    int add = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4_1(src, src);
        src++;
    }
    
    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4_2(dst, coeffs);
        coeffs += 4;
        dst += stride;
    }
#endif
}

#define TR_8(dst, src, dstep, sstep, assign)                \
    do {                                                    \
        int i, j;                                           \
        int e_8[4];                                         \
        int o_8[4] = { 0 };                                 \
        for (i = 0; i < 4; i++)                             \
            for (j = 1; j < 8; j += 2)                      \
                o_8[i] += transform[4*j][i] * src[j*sstep]; \
        TR_4(e_8, src, 1, 2*sstep, SET);                    \
for (i = 0; i < 4; i++) {                           \
            assign(dst[i*dstep], e_8[i] + o_8[i]);          \
            assign(dst[(7-i)*dstep], e_8[i] - o_8[i]);      \
        }                                                   \
    } while (0)
#define TR_16(dst, src, dstep, sstep, assign)                   \
    do {                                                        \
        int i, j;                                               \
        int e_16[8];                                            \
        int o_16[8] = { 0 };                                    \
        for (i = 0; i < 8; i++)                                 \
            for (j = 1; j < 16; j += 2)                         \
                o_16[i] += transform[2*j][i] * src[j*sstep];    \
        TR_8(e_16, src, 1, 2*sstep, SET);                       \
                                                                 \
        for (i = 0; i < 8; i++) {                               \
            assign(dst[i*dstep], e_16[i] + o_16[i]);            \
            assign(dst[(15-i)*dstep], e_16[i] - o_16[i]);       \
        }                                                       \
    } while (0)
#define TR_32(dst, src, dstep, sstep, assign)               \
    do {                                                    \
        int i, j;                                           \
        int e_32[16];                                       \
        int o_32[16] = { 0 };                               \
        for (i = 0; i < 16; i++)                            \
            for (j = 1; j < 32; j += 2)                     \
                o_32[i] += transform[j][i] * src[j*sstep];  \
        TR_16(e_32, src, 1, 2*sstep, SET);                \
                                                            \
        for (i = 0; i < 16; i++) {                          \
            assign(dst[i*dstep], e_32[i] + o_32[i]);        \
            assign(dst[(31-i)*dstep], e_32[i] - o_32[i]);   \
        }                                                   \
    } while (0)

#define TR_8_1(dst, src) TR_8(dst, src, 8, 8, SCALE)
#define TR_16_1(dst, src) TR_16(dst, src, 16, 16, SCALE)
#define TR_32_1(dst, src) TR_32(dst, src, 32, 32, SCALE)

#define TR_8_2(dst, src) TR_8(dst, src, 1, 1, ADD_AND_SCALE)
#define TR_16_2(dst, src) TR_16(dst, src, 1, 1, ADD_AND_SCALE)
#define TR_32_2(dst, src) TR_32(dst, src, 1, 1, ADD_AND_SCALE)

static void FUNC(transform_8x8_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride) {
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int16_t *src = coeffs;
#ifdef USE_SSE_8x8_Transform
    __m128i m128iS0, m128iS1, m128iS2, m128iS3, m128iS4, m128iS5, m128iS6, m128iS7, m128iAdd, m128Tmp0,     m128Tmp1,m128Tmp2, m128Tmp3, E0h, E1h, E2h, E3h, E0l, E1l, E2l, E3l, O0h, O1h, O2h, O3h, O0l, O1l, O2l, O3l, EE0l, EE1l, E00l, E01l, EE0h, EE1h, E00h, E01h;
    int j;
    m128iAdd  = _mm_set1_epi32( add_1st );
    
    m128iS1   = _mm_load_si128( (__m128i*)( src + 8   ) );
    m128iS3   = _mm_load_si128( (__m128i*)( src + 24 ) );
    m128Tmp0 = _mm_unpacklo_epi16(  m128iS1, m128iS3 );
    E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[0] ) ) );
    m128Tmp1 = _mm_unpackhi_epi16(  m128iS1, m128iS3 );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[0] ) ) );
    m128iS5   = _mm_load_si128( (__m128i*)( src + 40   ) );
    m128iS7   = _mm_load_si128( (__m128i*)( src + 56 ) );
    m128Tmp2 =  _mm_unpacklo_epi16(  m128iS5, m128iS7 );
    E2l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[1] ) ) );
    m128Tmp3 = _mm_unpackhi_epi16(  m128iS5, m128iS7 );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[1] ) ) );
    O0l = _mm_add_epi32(E1l, E2l);
    O0h = _mm_add_epi32(E1h, E2h);
    
    E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[2] ) ) );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[2] ) ) );
    E2l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[3] ) ) );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[3] ) ) );
    
    O1l = _mm_add_epi32(E1l, E2l);
    O1h = _mm_add_epi32(E1h, E2h);
    
    E1l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[4] ) ) );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[4] ) ) );
    E2l =  _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[5] ) ) );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[5] ) ) );
    O2l = _mm_add_epi32(E1l, E2l);
    O2h = _mm_add_epi32(E1h, E2h);
    
    E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[6] ) ) );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[6] ) ) );
    E2l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[7] ) ) );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[7] ) ) );
    O3h = _mm_add_epi32(E1h, E2h);
    O3l = _mm_add_epi32(E1l, E2l);
    
    /*    -------     */
    
    m128iS0   = _mm_load_si128( (__m128i*)( src + 0   ) );
    m128iS4   = _mm_load_si128( (__m128i*)( src + 32   ) );
    m128Tmp0 = _mm_unpacklo_epi16(  m128iS0, m128iS4 );
    EE0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[8] ) ) );
    m128Tmp1 = _mm_unpackhi_epi16(  m128iS0, m128iS4 );
    EE0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[8] ) ) );
    
    EE1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[9] ) ) );
    EE1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[9] ) ) );
    
    
    /*    -------     */
    
    m128iS2   = _mm_load_si128( (__m128i*)( src  +16) );
    m128iS6   = _mm_load_si128( (__m128i*)( src + 48   ) );
    m128Tmp0 = _mm_unpacklo_epi16(  m128iS2, m128iS6 );
    E00l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[10] ) ) );
    m128Tmp1 = _mm_unpackhi_epi16(  m128iS2, m128iS6 );
    E00h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[10] ) ) );
    E01l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[11] ) ) );
    E01h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[11] ) ) );
    E0l = _mm_add_epi32(EE0l , E00l);
    E0l = _mm_add_epi32(E0l, m128iAdd);
    E0h = _mm_add_epi32(EE0h , E00h);
    E0h = _mm_add_epi32(E0h, m128iAdd);
    E3l = _mm_sub_epi32(EE0l , E00l);
    E3l = _mm_add_epi32(E3l , m128iAdd);
    E3h = _mm_sub_epi32(EE0h , E00h);
    E3h = _mm_add_epi32(E3h , m128iAdd);
    
    E1l = _mm_add_epi32(EE1l , E01l);
    E1l = _mm_add_epi32(E1l , m128iAdd);
    E1h = _mm_add_epi32(EE1h , E01h);
    E1h = _mm_add_epi32(E1h , m128iAdd);
    E2l = _mm_sub_epi32(EE1l , E01l);
    E2l = _mm_add_epi32(E2l , m128iAdd);
    E2h = _mm_sub_epi32(EE1h , E01h);
    E2h = _mm_add_epi32(E2h , m128iAdd);
    m128iS0 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E0l, O0l),shift_1st), _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift_1st));
    m128iS1 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E1l, O1l),shift_1st), _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift_1st));
    m128iS2 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E2l, O2l),shift_1st), _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift_1st));
    m128iS3 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E3l, O3l),shift_1st), _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift_1st));
    m128iS4 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E3l, O3l),shift_1st), _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift_1st));
    m128iS5 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E2l, O2l),shift_1st), _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift_1st));
    m128iS6 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E1l, O1l),shift_1st), _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift_1st));
    m128iS7 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E0l, O0l),shift_1st), _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift_1st));
    /*  Invers matrix   */
    
    E0l = _mm_unpacklo_epi16(m128iS0, m128iS4);
    E1l = _mm_unpacklo_epi16(m128iS1, m128iS5);
    E2l = _mm_unpacklo_epi16(m128iS2, m128iS6);
    E3l = _mm_unpacklo_epi16(m128iS3, m128iS7);
    O0l = _mm_unpackhi_epi16(m128iS0, m128iS4);
    O1l = _mm_unpackhi_epi16(m128iS1, m128iS5);
    O2l = _mm_unpackhi_epi16(m128iS2, m128iS6);
    O3l = _mm_unpackhi_epi16(m128iS3, m128iS7);
    m128Tmp0 = _mm_unpacklo_epi16(E0l, E2l);
    m128Tmp1 = _mm_unpacklo_epi16(E1l, E3l);
    m128iS0  = _mm_unpacklo_epi16(m128Tmp0, m128Tmp1);
    m128iS1  = _mm_unpackhi_epi16(m128Tmp0, m128Tmp1);
    m128Tmp2 = _mm_unpackhi_epi16(E0l, E2l);
    m128Tmp3 = _mm_unpackhi_epi16(E1l, E3l);
    m128iS2 = _mm_unpacklo_epi16(m128Tmp2, m128Tmp3);
    m128iS3 = _mm_unpackhi_epi16(m128Tmp2, m128Tmp3);
    m128Tmp0 = _mm_unpacklo_epi16(O0l, O2l);
    m128Tmp1 = _mm_unpacklo_epi16(O1l, O3l);
    m128iS4  = _mm_unpacklo_epi16(m128Tmp0, m128Tmp1);
    m128iS5  = _mm_unpackhi_epi16(m128Tmp0, m128Tmp1);
    m128Tmp2 = _mm_unpackhi_epi16(O0l, O2l);
    m128Tmp3 = _mm_unpackhi_epi16(O1l, O3l);
    m128iS6 = _mm_unpacklo_epi16(m128Tmp2, m128Tmp3);
    m128iS7 = _mm_unpackhi_epi16(m128Tmp2, m128Tmp3);
    
    m128iAdd  = _mm_set1_epi32( add_2nd );
    
    
    m128Tmp0 = _mm_unpacklo_epi16(  m128iS1, m128iS3 );
    E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[0] ) ) );
    m128Tmp1 = _mm_unpackhi_epi16(  m128iS1, m128iS3 );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[0] ) ) );
    m128Tmp2 =  _mm_unpacklo_epi16(  m128iS5, m128iS7 );
    E2l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[1] ) ) );
    m128Tmp3 = _mm_unpackhi_epi16(  m128iS5, m128iS7 );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[1] ) ) );
    O0l = _mm_add_epi32(E1l, E2l);
    O0h = _mm_add_epi32(E1h, E2h);
    E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[2] ) ) );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[2] ) ) );
    E2l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[3] ) ) );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[3] ) ) );
    O1l = _mm_add_epi32(E1l, E2l);
    O1h = _mm_add_epi32(E1h, E2h);
    E1l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[4] ) ) );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[4] ) ) );
    E2l =  _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[5] ) ) );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[5] ) ) );
    O2l = _mm_add_epi32(E1l, E2l);
    O2h = _mm_add_epi32(E1h, E2h);
    E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[6] ) ) );
    E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[6] ) ) );
    E2l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform8x8[7] ) ) );
    E2h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform8x8[7] ) ) );
    O3h = _mm_add_epi32(E1h, E2h);
    O3l = _mm_add_epi32(E1l, E2l);
    
    m128Tmp0 = _mm_unpacklo_epi16(  m128iS0, m128iS4 );
    EE0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[8] ) ) );
    m128Tmp1 = _mm_unpackhi_epi16(  m128iS0, m128iS4 );
    EE0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[8] ) ) );
    EE1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[9] ) ) );
    EE1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[9] ) ) );
    
    m128Tmp0 = _mm_unpacklo_epi16(  m128iS2, m128iS6 );
    E00l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[10] ) ) );
    m128Tmp1 = _mm_unpackhi_epi16(  m128iS2, m128iS6 );
    E00h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[10] ) ) );
    E01l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform8x8[11] ) ) );
    E01h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform8x8[11] ) ) );
    E0l = _mm_add_epi32(EE0l , E00l);
    E0l = _mm_add_epi32(E0l, m128iAdd);
    E0h = _mm_add_epi32(EE0h , E00h);
    E0h = _mm_add_epi32(E0h, m128iAdd);
    E3l = _mm_sub_epi32(EE0l , E00l);
    E3l = _mm_add_epi32(E3l , m128iAdd);
    E3h = _mm_sub_epi32(EE0h , E00h);
    E3h = _mm_add_epi32(E3h , m128iAdd);
    E1l = _mm_add_epi32(EE1l , E01l);
    E1l = _mm_add_epi32(E1l , m128iAdd);
    E1h = _mm_add_epi32(EE1h , E01h);
    E1h = _mm_add_epi32(E1h , m128iAdd);
    E2l = _mm_sub_epi32(EE1l , E01l);
    E2l = _mm_add_epi32(E2l , m128iAdd);
    E2h = _mm_sub_epi32(EE1h , E01h);
    E2h = _mm_add_epi32(E2h , m128iAdd);
    
    m128iS0 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E0l, O0l),shift_2nd), _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift_2nd));
    m128iS1 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E1l, O1l),shift_2nd), _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift_2nd));
    m128iS2 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E2l, O2l),shift_2nd), _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift_2nd));
    m128iS3 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E3l, O3l),shift_2nd), _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift_2nd));
    m128iS4 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E3l, O3l),shift_2nd), _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift_2nd));
    m128iS5 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E2l, O2l),shift_2nd), _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift_2nd));
    m128iS6 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E1l, O1l),shift_2nd), _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift_2nd));
    m128iS7 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E0l, O0l),shift_2nd), _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift_2nd));
    
    
    _mm_store_si128( (__m128i*)( src     ), m128iS0 );
    _mm_store_si128( (__m128i*)( src + 8 ), m128iS1 );
    _mm_store_si128( (__m128i*)( src + 16 ), m128iS2 );
    _mm_store_si128( (__m128i*)( src + 24 ), m128iS3 );
    _mm_store_si128( (__m128i*)( src + 32 ), m128iS4 );
    _mm_store_si128( (__m128i*)( src + 40 ), m128iS5 );
    _mm_store_si128( (__m128i*)( src + 48 ), m128iS6 );
    _mm_store_si128( (__m128i*)( src + 56 ), m128iS7 );
    
    j = 0;
    for (i = 0; i < 4; i++) {
    	dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[j]));
        dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[j+8]));
        dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[j+16]));
        dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[j+24]));
        dst[4] = av_clip_pixel(dst[4]+av_clip_int16(src[j+32]));
        dst[5] = av_clip_pixel(dst[5]+av_clip_int16(src[j+40]));
        dst[6] = av_clip_pixel(dst[6]+av_clip_int16(src[j+48]));
        dst[7] = av_clip_pixel(dst[7]+av_clip_int16(src[j+56]));
        j +=1;
        dst += stride;
        dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[j]));
        dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[j+8]));
        dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[j+16]));
        dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[j+24]));
        dst[4] = av_clip_pixel(dst[4]+av_clip_int16(src[j+32]));
        dst[5] = av_clip_pixel(dst[5]+av_clip_int16(src[j+40]));
        dst[6] = av_clip_pixel(dst[6]+av_clip_int16(src[j+48]));
        dst[7] = av_clip_pixel(dst[7]+av_clip_int16(src[j+56]));
        j +=1;
        dst += stride;
    }
#else
    int shift = 7;
    int add = 1 << (shift - 1);
    for (i = 0; i < 8; i++) {
        TR_8_1(src, src);
        src++;
    }
    src = coeffs;
    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 8; i++) {
        TR_8_2(dst, coeffs);
        coeffs += 8;
        dst += stride;
    }
#endif
}

static void FUNC(transform_16x16_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int16_t *src = coeffs;
    int32_t shift;
#ifdef    USE_SSE_16x16_Transform
    __m128i m128iS0, m128iS1, m128iS2, m128iS3, m128iS4, m128iS5, m128iS6, m128iS7, m128iS8, m128iS9, m128iS10, m128iS11, m128iS12, m128iS13, m128iS14, m128iS15 ,  m128iAdd, m128Tmp0,     m128Tmp1,m128Tmp2, m128Tmp3, m128Tmp4, m128Tmp5,m128Tmp6, m128Tmp7, E0h, E1h, E2h, E3h, E0l, E1l, E2l, E3l, O0h, O1h, O2h, O3h, O4h, O5h, O6h, O7h,O0l, O1l, O2l, O3l, O4l, O5l, O6l, O7l,EE0l, EE1l, EE2l, EE3l, E00l, E01l, EE0h, EE1h, EE2h, EE3h,E00h, E01h;
    __m128i E4l, E5l, E6l, E7l;
    __m128i E4h, E5h, E6h, E7h;
    int j;
    m128iS0   = _mm_load_si128( (__m128i*)( src ) );
    m128iS1   = _mm_load_si128( (__m128i*)( src + 16 ) );
    m128iS2   = _mm_load_si128( (__m128i*)( src + 32   ) );
    m128iS3   = _mm_load_si128( (__m128i*)( src + 48) );
    m128iS4   = _mm_loadu_si128( (__m128i*)( src  +  64 ) );
    m128iS5   = _mm_load_si128( (__m128i*)( src + 80 ) );
    m128iS6   = _mm_load_si128( (__m128i*)( src  + 96 ) );
    m128iS7   = _mm_load_si128( (__m128i*)( src + 112) );
    m128iS8   = _mm_load_si128( (__m128i*)( src + 128 ) );
    m128iS9   = _mm_load_si128( (__m128i*)( src + 144) );
    m128iS10   = _mm_load_si128( (__m128i*)( src + 160   ) );
    m128iS11   = _mm_load_si128( (__m128i*)( src + 176) );
    m128iS12   = _mm_loadu_si128( (__m128i*)( src + 192 ) );
    m128iS13   = _mm_load_si128( (__m128i*)( src + 208 ) );
    m128iS14   = _mm_load_si128( (__m128i*)( src + 224 ) );
    m128iS15   = _mm_load_si128( (__m128i*)( src + 240 ) );
    shift = shift_1st;
    m128iAdd  = _mm_set1_epi32( add_1st );
    
    for(j=0; j< 2; j++) {
        for(i=0; i < 16; i+=8) {
            
            m128Tmp0 = _mm_unpacklo_epi16(  m128iS1, m128iS3 );
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][0] ) ) );
            m128Tmp1 = _mm_unpackhi_epi16(  m128iS1, m128iS3 );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][0] ) ) );
            
            
            m128Tmp2 =  _mm_unpacklo_epi16(  m128iS5, m128iS7 );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][0] ) ) );
            m128Tmp3 = _mm_unpackhi_epi16(  m128iS5, m128iS7 );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][0] ) ) );
            
            
            m128Tmp4 =  _mm_unpacklo_epi16(  m128iS9, m128iS11 );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][0] ) ) );
            m128Tmp5 = _mm_unpackhi_epi16(  m128iS9, m128iS11 );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][0] ) ) );
            
            
            m128Tmp6 =  _mm_unpacklo_epi16(  m128iS13, m128iS15 );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][0] ) ) );
            m128Tmp7 = _mm_unpackhi_epi16(  m128iS13, m128iS15 );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][0] ) ) );
            
            O0l = _mm_add_epi32(E0l, E1l);
            O0l = _mm_add_epi32(O0l, E2l);
            O0l = _mm_add_epi32(O0l, E3l);
            
            O0h = _mm_add_epi32(E0h, E1h);
            O0h = _mm_add_epi32(O0h, E2h);
            O0h = _mm_add_epi32(O0h, E3h);
            
            /* Compute O1*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][1] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][1] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][1] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][1] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][1] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][1] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][1] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][1] ) ) );
            O1l = _mm_add_epi32(E0l, E1l);
            O1l = _mm_add_epi32(O1l, E2l);
            O1l = _mm_add_epi32(O1l, E3l);
            O1h = _mm_add_epi32(E0h, E1h);
            O1h = _mm_add_epi32(O1h, E2h);
            O1h = _mm_add_epi32(O1h, E3h);
            
            /* Compute O2*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][2] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][2] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][2] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][2] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][2] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][2] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][2] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][2] ) ) );
            O2l = _mm_add_epi32(E0l, E1l);
            O2l = _mm_add_epi32(O2l, E2l);
            O2l = _mm_add_epi32(O2l, E3l);
            
            O2h = _mm_add_epi32(E0h, E1h);
            O2h = _mm_add_epi32(O2h, E2h);
            O2h = _mm_add_epi32(O2h, E3h);
            
            /* Compute O3*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][3] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][3] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][3] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][3] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][3] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][3] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][3] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][3] ) ) );
            
            O3l = _mm_add_epi32(E0l, E1l);
            O3l = _mm_add_epi32(O3l, E2l);
            O3l = _mm_add_epi32(O3l, E3l);
            
            O3h = _mm_add_epi32(E0h, E1h);
            O3h = _mm_add_epi32(O3h, E2h);
            O3h = _mm_add_epi32(O3h, E3h);
            
            /* Compute O4*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][4] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][4] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][4] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][4] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][4] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][4] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][4] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][4] ) ) );
            
            O4l = _mm_add_epi32(E0l, E1l);
            O4l = _mm_add_epi32(O4l, E2l);
            O4l = _mm_add_epi32(O4l, E3l);
            
            O4h = _mm_add_epi32(E0h, E1h);
            O4h = _mm_add_epi32(O4h, E2h);
            O4h = _mm_add_epi32(O4h, E3h);
            
            /* Compute O5*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][5] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][5] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][5] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][5] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][5] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][5] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][5] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][5] ) ) );
            
            O5l = _mm_add_epi32(E0l, E1l);
            O5l = _mm_add_epi32(O5l, E2l);
            O5l = _mm_add_epi32(O5l, E3l);
            
            O5h = _mm_add_epi32(E0h, E1h);
            O5h = _mm_add_epi32(O5h, E2h);
            O5h = _mm_add_epi32(O5h, E3h);
            
            /* Compute O6*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][6] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][6] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][6] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][6] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][6] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][6] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][6] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][6] ) ) );
            
            O6l = _mm_add_epi32(E0l, E1l);
            O6l = _mm_add_epi32(O6l, E2l);
            O6l = _mm_add_epi32(O6l, E3l);
            
            O6h = _mm_add_epi32(E0h, E1h);
            O6h = _mm_add_epi32(O6h, E2h);
            O6h = _mm_add_epi32(O6h, E3h);
            
            /* Compute O7*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][7] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][7] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][7] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][7] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][7] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][7] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][7] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][7] ) ) );
            
            O7l = _mm_add_epi32(E0l, E1l);
            O7l = _mm_add_epi32(O7l, E2l);
            O7l = _mm_add_epi32(O7l, E3l);
            
            O7h = _mm_add_epi32(E0h, E1h);
            O7h = _mm_add_epi32(O7h, E2h);
            O7h = _mm_add_epi32(O7h, E3h);
            
            /*  Compute E0  */
            
            m128Tmp0 = _mm_unpacklo_epi16(  m128iS2, m128iS6 );
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][0] ) ) );
            m128Tmp1 = _mm_unpackhi_epi16(  m128iS2, m128iS6 );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][0] ) ) );
            
            
            m128Tmp2 =  _mm_unpacklo_epi16(  m128iS10, m128iS14 );
            E0l = _mm_add_epi32(E0l, _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][0] ) ) ));
            m128Tmp3 = _mm_unpackhi_epi16(  m128iS10, m128iS14 );
            E0h = _mm_add_epi32(E0h, _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][0] ) ) ));
            
            /*  Compute E1  */
            E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][1] ) ));
            E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][1] ) ) );
            E1l = _mm_add_epi32(E1l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][1] ) ) ));
            E1h = _mm_add_epi32(E1h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][1] ) ) ));
            
            
            /*  Compute E2  */
            E2l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][2] ) ) );
            E2h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][2] ) ) );
            E2l = _mm_add_epi32(E2l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][2] ) ) ));
            E2h = _mm_add_epi32(E2h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][2] ) ) ));
            /*  Compute E3  */
            E3l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][3] ) ) );
            E3h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][3] ) ) );
            E3l = _mm_add_epi32(E3l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][3] ) ) ));
            E3h = _mm_add_epi32(E3h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][3] ) ) ));
            
            /*  Compute EE0 and EEE */
            
            m128Tmp0 = _mm_unpacklo_epi16(  m128iS4, m128iS12 );
            E00l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_3[0][0] ) ) );
            m128Tmp1 = _mm_unpackhi_epi16(  m128iS4, m128iS12 );
            E00h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_3[0][0] ) ) );
            
            m128Tmp2 =  _mm_unpacklo_epi16(  m128iS0, m128iS8 );
            EE0l =  _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_3[1][0] ) ) );
            m128Tmp3 = _mm_unpackhi_epi16(  m128iS0, m128iS8 );
            EE0h =  _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_3[1][0] ) ) );
            
            
            E01l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_3[0][1] ) ) );
            E01h  = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_3[0][1] ) ) );
            
            EE1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_3[1][1] ) ) );
            EE1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_3[1][1] ) ) );
            
            /*  Compute EE    */
            EE2l = _mm_sub_epi32(EE1l,E01l);
            EE3l = _mm_sub_epi32(EE0l,E00l);
            EE2h = _mm_sub_epi32(EE1h,E01h);
            EE3h = _mm_sub_epi32(EE0h,E00h);
            
            EE0l = _mm_add_epi32(EE0l,E00l);
            EE1l = _mm_add_epi32(EE1l,E01l);
            EE0h = _mm_add_epi32(EE0h,E00h);
            EE1h = _mm_add_epi32(EE1h,E01h);
            
            /*      Compute E       */
            
            E4l = _mm_sub_epi32(EE3l,E3l);
            E4l = _mm_add_epi32(E4l, m128iAdd);
            
            E5l = _mm_sub_epi32(EE2l,E2l);
            E5l = _mm_add_epi32(E5l, m128iAdd);
            
            E6l = _mm_sub_epi32(EE1l,E1l);
            E6l = _mm_add_epi32(E6l, m128iAdd);
            
            E7l = _mm_sub_epi32(EE0l,E0l);
            E7l = _mm_add_epi32(E7l, m128iAdd);
            
            E4h = _mm_sub_epi32(EE3h,E3h);
            E4h = _mm_add_epi32(E4h, m128iAdd);
            
            E5h = _mm_sub_epi32(EE2h,E2h);
            E5h = _mm_add_epi32(E5h, m128iAdd);
            
            E6h = _mm_sub_epi32(EE1h,E1h);
            E6h = _mm_add_epi32(E6h, m128iAdd);
            
            E7h = _mm_sub_epi32(EE0h,E0h);
            E7h = _mm_add_epi32(E7h, m128iAdd);
            
            E0l = _mm_add_epi32(EE0l,E0l);
            E0l = _mm_add_epi32(E0l, m128iAdd);
            
            E1l = _mm_add_epi32(EE1l,E1l);
            E1l = _mm_add_epi32(E1l, m128iAdd);
            
            E2l = _mm_add_epi32(EE2l,E2l);
            E2l = _mm_add_epi32(E2l, m128iAdd);
            
            E3l = _mm_add_epi32(EE3l,E3l);
            E3l = _mm_add_epi32(E3l, m128iAdd);
            
            E0h = _mm_add_epi32(EE0h,E0h);
            E0h = _mm_add_epi32(E0h, m128iAdd);
            
            E1h = _mm_add_epi32(EE1h,E1h);
            E1h = _mm_add_epi32(E1h, m128iAdd);
            
            E2h = _mm_add_epi32(EE2h,E2h);
            E2h = _mm_add_epi32(E2h, m128iAdd);
            
            E3h = _mm_add_epi32(EE3h,E3h);
            E3h = _mm_add_epi32(E3h, m128iAdd);
            
            m128iS0 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E0l, O0l),shift), _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift));
            m128iS1 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E1l, O1l),shift), _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift));
            m128iS2 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E2l, O2l),shift), _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift));
            m128iS3 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E3l, O3l),shift), _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift));
            
            m128iS4 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E4l, O4l),shift), _mm_srai_epi32(_mm_add_epi32(E4h, O4h), shift));
            m128iS5 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E5l, O5l),shift), _mm_srai_epi32(_mm_add_epi32(E5h, O5h), shift));
            m128iS6 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E6l, O6l),shift), _mm_srai_epi32(_mm_add_epi32(E6h, O6h), shift));
            m128iS7 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E7l, O7l),shift), _mm_srai_epi32(_mm_add_epi32(E7h, O7h), shift));
            
            m128iS15 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E0l, O0l),shift), _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift));
            m128iS14 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E1l, O1l),shift), _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift));
            m128iS13 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E2l, O2l),shift), _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift));
            m128iS12 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E3l, O3l),shift), _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift));
            
            m128iS11 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E4l, O4l),shift), _mm_srai_epi32(_mm_sub_epi32(E4h, O4h), shift));
            m128iS10 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E5l, O5l),shift), _mm_srai_epi32(_mm_sub_epi32(E5h, O5h), shift));
            m128iS9 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E6l, O6l),shift), _mm_srai_epi32(_mm_sub_epi32(E6h, O6h), shift));
            m128iS8 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E7l, O7l),shift), _mm_srai_epi32(_mm_sub_epi32(E7h, O7h), shift));
            
            
            if(!j){
                /*      Inverse the matrix      */
                E0l = _mm_unpacklo_epi16(m128iS0, m128iS8);
                E1l = _mm_unpacklo_epi16(m128iS1, m128iS9);
                E2l = _mm_unpacklo_epi16(m128iS2, m128iS10);
                E3l = _mm_unpacklo_epi16(m128iS3, m128iS11);
                E4l = _mm_unpacklo_epi16(m128iS4, m128iS12);
                E5l = _mm_unpacklo_epi16(m128iS5, m128iS13);
                E6l = _mm_unpacklo_epi16(m128iS6, m128iS14);
                E7l = _mm_unpacklo_epi16(m128iS7, m128iS15);
                
                O0l = _mm_unpackhi_epi16(m128iS0, m128iS8);
                O1l = _mm_unpackhi_epi16(m128iS1, m128iS9);
                O2l = _mm_unpackhi_epi16(m128iS2, m128iS10);
                O3l = _mm_unpackhi_epi16(m128iS3, m128iS11);
                O4l = _mm_unpackhi_epi16(m128iS4, m128iS12);
                O5l = _mm_unpackhi_epi16(m128iS5, m128iS13);
                O6l = _mm_unpackhi_epi16(m128iS6, m128iS14);
                O7l = _mm_unpackhi_epi16(m128iS7, m128iS15);
                
                
                m128Tmp0 = _mm_unpacklo_epi16(E0l, E4l);
                m128Tmp1 = _mm_unpacklo_epi16(E1l, E5l);
                m128Tmp2 = _mm_unpacklo_epi16(E2l, E6l);
                m128Tmp3 = _mm_unpacklo_epi16(E3l, E7l);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS0  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS1  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS2  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS3  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpackhi_epi16(E0l, E4l);
                m128Tmp1 = _mm_unpackhi_epi16(E1l, E5l);
                m128Tmp2 = _mm_unpackhi_epi16(E2l, E6l);
                m128Tmp3 = _mm_unpackhi_epi16(E3l, E7l);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS4  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS5  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS6  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS7  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpacklo_epi16(O0l, O4l);
                m128Tmp1 = _mm_unpacklo_epi16(O1l, O5l);
                m128Tmp2 = _mm_unpacklo_epi16(O2l, O6l);
                m128Tmp3 = _mm_unpacklo_epi16(O3l, O7l);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS8  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS9  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS10  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS11  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpackhi_epi16(O0l, O4l);
                m128Tmp1 = _mm_unpackhi_epi16(O1l, O5l);
                m128Tmp2 = _mm_unpackhi_epi16(O2l, O6l);
                m128Tmp3 = _mm_unpackhi_epi16(O3l, O7l);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS12  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS13  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS14  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS15  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                /*  */
                _mm_store_si128((__m128i*)( src+i ), m128iS0);
                _mm_store_si128((__m128i*)( src + 16 + i ), m128iS1);
                _mm_store_si128((__m128i*)( src + 32 + i ), m128iS2);
                _mm_store_si128((__m128i*)( src + 48 + i ), m128iS3);
                _mm_store_si128((__m128i*)( src + 64 + i ), m128iS4);
                _mm_store_si128((__m128i*)( src + 80 + i ), m128iS5);
                _mm_store_si128((__m128i*)( src + 96 + i ), m128iS6);
                _mm_store_si128((__m128i*)( src + 112 + i), m128iS7);
                _mm_store_si128((__m128i*)( src + 128 + i), m128iS8);
                _mm_store_si128((__m128i*)( src + 144 + i ), m128iS9);
                _mm_store_si128((__m128i*)( src + 160 + i), m128iS10);
                _mm_store_si128((__m128i*)( src + 176 + i), m128iS11);
                _mm_store_si128((__m128i*)( src + 192 + i ), m128iS12);
                _mm_store_si128((__m128i*)( src + 208 + i), m128iS13);
                _mm_store_si128((__m128i*)( src + 224 + i), m128iS14);
                _mm_store_si128((__m128i*)( src + 240+ i ), m128iS15);
                
                if(!i) {
                    m128iS0   = _mm_load_si128( (__m128i*)( src + 8) );
                    m128iS1   = _mm_load_si128( (__m128i*)( src + 24) );
                    m128iS2   = _mm_load_si128( (__m128i*)( src + 40) );
                    m128iS3   = _mm_load_si128( (__m128i*)( src + 56) );
                    m128iS4   = _mm_loadu_si128( (__m128i*)( src  +  72) );
                    m128iS5   = _mm_load_si128( (__m128i*)( src + 88) );
                    m128iS6   = _mm_load_si128( (__m128i*)( src  + 104) );
                    m128iS7   = _mm_load_si128( (__m128i*)( src + 120) );
                    m128iS8   = _mm_load_si128( (__m128i*)( src + 136) );
                    m128iS9   = _mm_load_si128( (__m128i*)( src + 152));
                    m128iS10   = _mm_load_si128( (__m128i*)( src + 168 ) );
                    m128iS11   = _mm_load_si128( (__m128i*)( src + 184));
                    m128iS12   = _mm_loadu_si128( (__m128i*)( src + 200) );
                    m128iS13   = _mm_load_si128( (__m128i*)( src + 216) );
                    m128iS14   = _mm_load_si128( (__m128i*)( src + 232 ) );
                    m128iS15   = _mm_load_si128( (__m128i*)( src + 248) );
                } else {
                    m128iS0   = _mm_load_si128( (__m128i*)( src) );
                    m128iS1   = _mm_load_si128( (__m128i*)( src + 32) );
                    m128iS2   = _mm_load_si128( (__m128i*)( src + 64 ) );
                    m128iS3   = _mm_load_si128( (__m128i*)( src + 96 ) );
                    m128iS4   = _mm_loadu_si128( (__m128i*)( src  + 128 ) );
                    m128iS5   = _mm_load_si128( (__m128i*)( src + 160 ) );
                    m128iS6   = _mm_load_si128( (__m128i*)( src  + 192) );
                    m128iS7   = _mm_load_si128( (__m128i*)( src + 224) );
                    m128iS8   = _mm_load_si128( (__m128i*)( src + 8) );
                    m128iS9   = _mm_load_si128( (__m128i*)( src + 32 +8));
                    m128iS10   = _mm_load_si128( (__m128i*)( src + 64  +8 ) );
                    m128iS11   = _mm_load_si128( (__m128i*)( src + 96 +8));
                    m128iS12   = _mm_loadu_si128( (__m128i*)( src + 128 +8) );
                    m128iS13   = _mm_load_si128( (__m128i*)( src + 160 +8) );
                    m128iS14   = _mm_load_si128( (__m128i*)( src + 192 +8) );
                    m128iS15   = _mm_load_si128( (__m128i*)( src + 224 +8) );
                    shift = shift_2nd;
                    m128iAdd  = _mm_set1_epi32( add_2nd );
                }
                
            } else {
                int k, m = 0;
                _mm_storeu_si128( (__m128i*)( src     ), m128iS0 );
                _mm_storeu_si128( (__m128i*)( src + 8 ), m128iS1 );
                _mm_storeu_si128( (__m128i*)( src + 32 ), m128iS2 );
                _mm_storeu_si128( (__m128i*)( src + 40), m128iS3 );
                _mm_storeu_si128( (__m128i*)( src + 64 ), m128iS4 );
                _mm_storeu_si128( (__m128i*)( src + 72 ), m128iS5 );
                _mm_storeu_si128( (__m128i*)( src + 96 ), m128iS6 );
                _mm_storeu_si128( (__m128i*)( src + 104), m128iS7 );
                _mm_storeu_si128( (__m128i*)( src + 128 ), m128iS8 );
                _mm_storeu_si128( (__m128i*)( src + 136), m128iS9 );
                _mm_storeu_si128( (__m128i*)( src + 160 ), m128iS10 );
                _mm_storeu_si128( (__m128i*)( src + 168), m128iS11 );
                _mm_storeu_si128( (__m128i*)( src +192 ), m128iS12 );
                _mm_storeu_si128( (__m128i*)( src +200), m128iS13 );
                _mm_storeu_si128( (__m128i*)( src + 224 ), m128iS14 );
                _mm_storeu_si128( (__m128i*)( src + 232), m128iS15 );
                dst = (pixel*)_dst + (i*stride);
                
                for ( k = 0; k < 8; k++) {
                    dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[m]));
                    dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[m+8]));
                    dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[m+32]));
                    dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[m+40]));
                    dst[4] = av_clip_pixel(dst[4]+av_clip_int16(src[m+64]));
                    dst[5] = av_clip_pixel(dst[5]+av_clip_int16(src[m+72]));
                    dst[6] = av_clip_pixel(dst[6]+av_clip_int16(src[m+96]));
                    dst[7] = av_clip_pixel(dst[7]+av_clip_int16(src[m+104]));
                    
                    
                    dst[8] = av_clip_pixel(dst[8]+av_clip_int16(src[m+128]));
                    dst[9] = av_clip_pixel(dst[9]+av_clip_int16(src[m+136]));
                    dst[10] = av_clip_pixel(dst[10]+av_clip_int16(src[m+160]));
                    dst[11] = av_clip_pixel(dst[11]+av_clip_int16(src[m+168]));
                    dst[12] = av_clip_pixel(dst[12]+av_clip_int16(src[m+192]));
                    dst[13] = av_clip_pixel(dst[13]+av_clip_int16(src[m+200]));
                    dst[14] = av_clip_pixel(dst[14]+av_clip_int16(src[m+224]));
                    dst[15] = av_clip_pixel(dst[15]+av_clip_int16(src[m+232]));
                    m +=1;
                    dst += stride;
                }
                if(!i){
                    m128iS0   = _mm_load_si128( (__m128i*)( src + 16));
                    m128iS1   = _mm_load_si128( (__m128i*)( src + 48  ) );
                    m128iS2   = _mm_load_si128( (__m128i*)( src + 80));
                    m128iS3   = _mm_loadu_si128( (__m128i*)( src + 112) );
                    m128iS4   = _mm_load_si128( (__m128i*)( src + 144 ) );
                    m128iS5   = _mm_load_si128( (__m128i*)( src + 176 ) );
                    m128iS6   = _mm_load_si128( (__m128i*)( src + 208 ) );
                    m128iS7   = _mm_load_si128( (__m128i*)( src + 240 ) );
                    m128iS8   = _mm_load_si128( (__m128i*)( src + 24));
                    m128iS9   = _mm_load_si128( (__m128i*)( src + 56 ) );
                    m128iS10   = _mm_load_si128( (__m128i*)( src + 88));
                    m128iS11   = _mm_loadu_si128( (__m128i*)( src + 120) );
                    m128iS12   = _mm_load_si128( (__m128i*)( src + 152) );
                    m128iS13   = _mm_load_si128( (__m128i*)( src + 184) );
                    m128iS14   = _mm_load_si128( (__m128i*)( src + 216) );
                    m128iS15   = _mm_load_si128( (__m128i*)( src + 248) );
                }
            }
        }
    }
#else
    shift = 7;
    int add = 1 << (shift - 1);
    for (i = 0; i < 16; i++) {
        TR_16_1(src, src);
        src++;
    }
    
    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    
    for (i = 0; i < 16; i++) {
        TR_16_2(dst, coeffs);
        coeffs += 16;
        dst += stride;
    }
#endif
}

static void FUNC(transform_32x32_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
#ifdef USE_SSE_32x32_Transform
    int i,j;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int shift;
    int16_t *src = coeffs;
    
    __m128i m128iS0, m128iS1, m128iS2, m128iS3, m128iS4, m128iS5, m128iS6, m128iS7, m128iS8, m128iS9, m128iS10, m128iS11, m128iS12, m128iS13, m128iS14, m128iS15 ,  m128iAdd, m128Tmp0,     m128Tmp1,m128Tmp2, m128Tmp3, m128Tmp4, m128Tmp5,m128Tmp6, m128Tmp7, E0h, E1h, E2h, E3h, E0l, E1l, E2l, E3l, O0h, O1h, O2h, O3h, O4h, O5h, O6h, O7h,O0l, O1l, O2l, O3l, O4l, O5l, O6l, O7l,EE0l, EE1l, EE2l, EE3l, E00l, E01l, EE0h, EE1h, EE2h, EE3h,E00h, E01h;
    __m128i E4l, E5l, E6l, E7l, E8l, E9l, E10l, E11l, E12l, E13l, E14l, E15l;
    __m128i E4h, E5h, E6h, E7h, E8h, E9h, E10h, E11h, E12h, E13h, E14h, E15h, EEE0l, EEE1l, EEE0h, EEE1h;
    __m128i m128iS16, m128iS17, m128iS18, m128iS19, m128iS20, m128iS21, m128iS22, m128iS23, m128iS24, m128iS25, m128iS26, m128iS27, m128iS28, m128iS29, m128iS30, m128iS31, m128Tmp8, m128Tmp9, m128Tmp10, m128Tmp11, m128Tmp12, m128Tmp13, m128Tmp14, m128Tmp15, O8h, O9h, O10h, O11h, O12h, O13h, O14h, O15h,O8l, O9l, O10l, O11l, O12l, O13l, O14l, O15l, E02l, E02h, E03l, E03h, EE7l, EE6l, EE5l, EE4l, EE7h, EE6h, EE5h, EE4h;
    m128iS0   = _mm_load_si128( (__m128i*)( src ) );
    m128iS1   = _mm_load_si128( (__m128i*)( src + 32 ) );
    m128iS2   = _mm_load_si128( (__m128i*)( src + 64   ) );
    m128iS3   = _mm_load_si128( (__m128i*)( src + 96) );
    m128iS4   = _mm_loadu_si128( (__m128i*)( src  +  128 ) );
    m128iS5   = _mm_load_si128( (__m128i*)( src + 160 ) );
    m128iS6   = _mm_load_si128( (__m128i*)( src  + 192 ) );
    m128iS7   = _mm_load_si128( (__m128i*)( src + 224) );
    m128iS8   = _mm_load_si128( (__m128i*)( src + 256 ) );
    m128iS9   = _mm_load_si128( (__m128i*)( src + 288) );
    m128iS10   = _mm_load_si128( (__m128i*)( src + 320   ) );
    m128iS11   = _mm_load_si128( (__m128i*)( src + 352) );
    m128iS12   = _mm_loadu_si128( (__m128i*)( src + 384 ) );
    m128iS13   = _mm_load_si128( (__m128i*)( src + 416 ) );
    m128iS14   = _mm_load_si128( (__m128i*)( src + 448 ) );
    m128iS15   = _mm_load_si128( (__m128i*)( src + 480 ) );
    m128iS16    = _mm_load_si128( (__m128i*)( src + 512 ) );
    m128iS17    = _mm_load_si128( (__m128i*)( src + 544 ) );
    m128iS18    = _mm_load_si128( (__m128i*)( src + 576 ) );
    m128iS19    = _mm_load_si128( (__m128i*)( src + 608 ) );
    m128iS20    = _mm_load_si128( (__m128i*)( src + 640 ) );
    m128iS21    = _mm_load_si128( (__m128i*)( src + 672 ) );
    m128iS22    = _mm_load_si128( (__m128i*)( src + 704 ) );
    m128iS23    = _mm_load_si128( (__m128i*)( src + 736 ) );
    m128iS24    = _mm_load_si128( (__m128i*)( src + 768 ) );
    m128iS25    = _mm_load_si128( (__m128i*)( src + 800 ) );
    m128iS26    = _mm_load_si128( (__m128i*)( src + 832 ) );
    m128iS27    = _mm_load_si128( (__m128i*)( src + 864 ) );
    m128iS28    = _mm_load_si128( (__m128i*)( src + 896 ) );
    m128iS29    = _mm_load_si128( (__m128i*)( src + 928 ) );
    m128iS30    = _mm_load_si128( (__m128i*)( src + 960 ) );
    m128iS31    = _mm_load_si128( (__m128i*)( src + 992 ) );
    
    shift = shift_1st;
    m128iAdd  = _mm_set1_epi32( add_1st );
    
    for(j=0; j< 2; j++) {
        for(i=0; i < 32; i+=8) {
            m128Tmp0 = _mm_unpacklo_epi16(  m128iS1, m128iS3 );
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][0] ) ) );
            m128Tmp1 = _mm_unpackhi_epi16(  m128iS1, m128iS3 );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][0] ) ) );
            
            
            m128Tmp2 =  _mm_unpacklo_epi16(  m128iS5, m128iS7 );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][0] ) ) );
            m128Tmp3 = _mm_unpackhi_epi16(  m128iS5, m128iS7 );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][0] ) ) );
            
            
            m128Tmp4 =  _mm_unpacklo_epi16(  m128iS9, m128iS11 );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][0] ) ) );
            m128Tmp5 = _mm_unpackhi_epi16(  m128iS9, m128iS11 );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][0] ) ) );
            
            
            m128Tmp6 =  _mm_unpacklo_epi16(  m128iS13, m128iS15 );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][0] ) ) );
            m128Tmp7 = _mm_unpackhi_epi16(  m128iS13, m128iS15 );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][0] ) ) );
            
            m128Tmp8 =  _mm_unpacklo_epi16(  m128iS17, m128iS19 );
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][0] ) ) );
            m128Tmp9 = _mm_unpackhi_epi16(  m128iS17, m128iS19 );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][0] ) ) );
            
            m128Tmp10 =  _mm_unpacklo_epi16(  m128iS21, m128iS23 );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][0] ) ) );
            m128Tmp11 = _mm_unpackhi_epi16(  m128iS21, m128iS23 );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][0] ) ) );
            
            m128Tmp12 =  _mm_unpacklo_epi16(  m128iS25, m128iS27 );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][0] ) ) );
            m128Tmp13 = _mm_unpackhi_epi16(  m128iS25, m128iS27 );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][0] ) ) );
            
            m128Tmp14 =  _mm_unpacklo_epi16(  m128iS29, m128iS31 );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][0] ) ) );
            m128Tmp15 = _mm_unpackhi_epi16(  m128iS29, m128iS31 );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][0] ) ) );
            
            
            O0l = _mm_add_epi32(E0l, E1l);
            O0l = _mm_add_epi32(O0l, E2l);
            O0l = _mm_add_epi32(O0l, E3l);
            O0l = _mm_add_epi32(O0l, E4l);
            O0l = _mm_add_epi32(O0l, E5l);
            O0l = _mm_add_epi32(O0l, E6l);
            O0l = _mm_add_epi32(O0l, E7l);
            
            
            O0h = _mm_add_epi32(E0h, E1h);
            O0h = _mm_add_epi32(O0h, E2h);
            O0h = _mm_add_epi32(O0h, E3h);
            O0h = _mm_add_epi32(O0h, E4h);
            O0h = _mm_add_epi32(O0h, E5h);
            O0h = _mm_add_epi32(O0h, E6h);
            O0h = _mm_add_epi32(O0h, E7h);
            
            
            /* Compute O1*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][1] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][1] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][1] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][1] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][1] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][1] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][1] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][1] ) ) );
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][1] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][1] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][1] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][1] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][1] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][1] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][1] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][1] ) ) );
            
            
            
            
            O1l = _mm_add_epi32(E0l, E1l);
            O1l = _mm_add_epi32(O1l, E2l);
            O1l = _mm_add_epi32(O1l, E3l);
            O1l = _mm_add_epi32(O1l, E4l);
            O1l = _mm_add_epi32(O1l, E5l);
            O1l = _mm_add_epi32(O1l, E6l);
            O1l = _mm_add_epi32(O1l, E7l);
            
            O1h = _mm_add_epi32(E0h, E1h);
            O1h = _mm_add_epi32(O1h, E2h);
            O1h = _mm_add_epi32(O1h, E3h);
            O1h = _mm_add_epi32(O1h, E4h);
            O1h = _mm_add_epi32(O1h, E5h);
            O1h = _mm_add_epi32(O1h, E6h);
            O1h = _mm_add_epi32(O1h, E7h);
            /* Compute O2*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][2] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][2] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][2] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][2] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][2] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][2] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][2] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][2] ) ) );
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][2] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][2] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][2] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][2] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][2] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][2] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][2] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][2] ) ) );
            
            
            O2l = _mm_add_epi32(E0l, E1l);
            O2l = _mm_add_epi32(O2l, E2l);
            O2l = _mm_add_epi32(O2l, E3l);
            O2l = _mm_add_epi32(O2l, E4l);
            O2l = _mm_add_epi32(O2l, E5l);
            O2l = _mm_add_epi32(O2l, E6l);
            O2l = _mm_add_epi32(O2l, E7l);
            
            O2h = _mm_add_epi32(E0h, E1h);
            O2h = _mm_add_epi32(O2h, E2h);
            O2h = _mm_add_epi32(O2h, E3h);
            O2h = _mm_add_epi32(O2h, E4h);
            O2h = _mm_add_epi32(O2h, E5h);
            O2h = _mm_add_epi32(O2h, E6h);
            O2h = _mm_add_epi32(O2h, E7h);
            /* Compute O3*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][3] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][3] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][3] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][3] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][3] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][3] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][3] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][3] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][3] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][3] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][3] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][3] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][3] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][3] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][3] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][3] ) ) );
            
            
            O3l = _mm_add_epi32(E0l, E1l);
            O3l = _mm_add_epi32(O3l, E2l);
            O3l = _mm_add_epi32(O3l, E3l);
            O3l = _mm_add_epi32(O3l, E4l);
            O3l = _mm_add_epi32(O3l, E5l);
            O3l = _mm_add_epi32(O3l, E6l);
            O3l = _mm_add_epi32(O3l, E7l);
            
            O3h = _mm_add_epi32(E0h, E1h);
            O3h = _mm_add_epi32(O3h, E2h);
            O3h = _mm_add_epi32(O3h, E3h);
            O3h = _mm_add_epi32(O3h, E4h);
            O3h = _mm_add_epi32(O3h, E5h);
            O3h = _mm_add_epi32(O3h, E6h);
            O3h = _mm_add_epi32(O3h, E7h);
            /* Compute O4*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][4] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][4] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][4] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][4] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][4] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][4] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][4] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][4] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][4] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][4] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][4] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][4] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][4] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][4] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][4] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][4] ) ) );
            
            
            O4l = _mm_add_epi32(E0l, E1l);
            O4l = _mm_add_epi32(O4l, E2l);
            O4l = _mm_add_epi32(O4l, E3l);
            O4l = _mm_add_epi32(O4l, E4l);
            O4l = _mm_add_epi32(O4l, E5l);
            O4l = _mm_add_epi32(O4l, E6l);
            O4l = _mm_add_epi32(O4l, E7l);
            
            O4h = _mm_add_epi32(E0h, E1h);
            O4h = _mm_add_epi32(O4h, E2h);
            O4h = _mm_add_epi32(O4h, E3h);
            O4h = _mm_add_epi32(O4h, E4h);
            O4h = _mm_add_epi32(O4h, E5h);
            O4h = _mm_add_epi32(O4h, E6h);
            O4h = _mm_add_epi32(O4h, E7h);
            
            
            /* Compute O5*/
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][5] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][5] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][5] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][5] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][5] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][5] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][5] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][5] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][5] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][5] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][5] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][5] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][5] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][5] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][5] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][5] ) ) );
            
            
            O5l = _mm_add_epi32(E0l, E1l);
            O5l = _mm_add_epi32(O5l, E2l);
            O5l = _mm_add_epi32(O5l, E3l);
            O5l = _mm_add_epi32(O5l, E4l);
            O5l = _mm_add_epi32(O5l, E5l);
            O5l = _mm_add_epi32(O5l, E6l);
            O5l = _mm_add_epi32(O5l, E7l);
            
            O5h = _mm_add_epi32(E0h, E1h);
            O5h = _mm_add_epi32(O5h, E2h);
            O5h = _mm_add_epi32(O5h, E3h);
            O5h = _mm_add_epi32(O5h, E4h);
            O5h = _mm_add_epi32(O5h, E5h);
            O5h = _mm_add_epi32(O5h, E6h);
            O5h = _mm_add_epi32(O5h, E7h);
            
            /* Compute O6*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][6] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][6] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][6] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][6] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][6] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][6] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][6] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][6] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][6] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][6] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][6] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][6] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][6] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][6] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][6] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][6] ) ) );
            
            
            O6l = _mm_add_epi32(E0l, E1l);
            O6l = _mm_add_epi32(O6l, E2l);
            O6l = _mm_add_epi32(O6l, E3l);
            O6l = _mm_add_epi32(O6l, E4l);
            O6l = _mm_add_epi32(O6l, E5l);
            O6l = _mm_add_epi32(O6l, E6l);
            O6l = _mm_add_epi32(O6l, E7l);
            
            O6h = _mm_add_epi32(E0h, E1h);
            O6h = _mm_add_epi32(O6h, E2h);
            O6h = _mm_add_epi32(O6h, E3h);
            O6h = _mm_add_epi32(O6h, E4h);
            O6h = _mm_add_epi32(O6h, E5h);
            O6h = _mm_add_epi32(O6h, E6h);
            O6h = _mm_add_epi32(O6h, E7h);
            
            /* Compute O7*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][7] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][7] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][7] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][7] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][7] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][7] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][7] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][7] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][7] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][7] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][7] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][7] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][7] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][7] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][7] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][7] ) ) );
            
            
            O7l = _mm_add_epi32(E0l, E1l);
            O7l = _mm_add_epi32(O7l, E2l);
            O7l = _mm_add_epi32(O7l, E3l);
            O7l = _mm_add_epi32(O7l, E4l);
            O7l = _mm_add_epi32(O7l, E5l);
            O7l = _mm_add_epi32(O7l, E6l);
            O7l = _mm_add_epi32(O7l, E7l);
            
            O7h = _mm_add_epi32(E0h, E1h);
            O7h = _mm_add_epi32(O7h, E2h);
            O7h = _mm_add_epi32(O7h, E3h);
            O7h = _mm_add_epi32(O7h, E4h);
            O7h = _mm_add_epi32(O7h, E5h);
            O7h = _mm_add_epi32(O7h, E6h);
            O7h = _mm_add_epi32(O7h, E7h);
            
            /* Compute O8*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][8] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][8] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][8] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][8] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][8] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][8] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][8] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][8] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][8] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][8] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][8] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][8] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][8] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][8] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][8] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][8] ) ) );
            
            
            O8l = _mm_add_epi32(E0l, E1l);
            O8l = _mm_add_epi32(O8l, E2l);
            O8l = _mm_add_epi32(O8l, E3l);
            O8l = _mm_add_epi32(O8l, E4l);
            O8l = _mm_add_epi32(O8l, E5l);
            O8l = _mm_add_epi32(O8l, E6l);
            O8l = _mm_add_epi32(O8l, E7l);
            
            O8h = _mm_add_epi32(E0h, E1h);
            O8h = _mm_add_epi32(O8h, E2h);
            O8h = _mm_add_epi32(O8h, E3h);
            O8h = _mm_add_epi32(O8h, E4h);
            O8h = _mm_add_epi32(O8h, E5h);
            O8h = _mm_add_epi32(O8h, E6h);
            O8h = _mm_add_epi32(O8h, E7h);
            
            
            /* Compute O9*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][9] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][9] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][9] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][9] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][9] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][9] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][9] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][9] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][9] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][9] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][9] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][9] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][9] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][9] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][9] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][9] ) ) );
            
            
            O9l = _mm_add_epi32(E0l, E1l);
            O9l = _mm_add_epi32(O9l, E2l);
            O9l = _mm_add_epi32(O9l, E3l);
            O9l = _mm_add_epi32(O9l, E4l);
            O9l = _mm_add_epi32(O9l, E5l);
            O9l = _mm_add_epi32(O9l, E6l);
            O9l = _mm_add_epi32(O9l, E7l);
            
            O9h = _mm_add_epi32(E0h, E1h);
            O9h = _mm_add_epi32(O9h, E2h);
            O9h = _mm_add_epi32(O9h, E3h);
            O9h = _mm_add_epi32(O9h, E4h);
            O9h = _mm_add_epi32(O9h, E5h);
            O9h = _mm_add_epi32(O9h, E6h);
            O9h = _mm_add_epi32(O9h, E7h);
            
            
            /* Compute 10*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][10] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][10] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][10] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][10] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][10] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][10] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][10] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][10] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][10] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][10] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][10] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][10] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][10] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][10] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][10] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][10] ) ) );
            
            
            O10l = _mm_add_epi32(E0l, E1l);
            O10l = _mm_add_epi32(O10l, E2l);
            O10l = _mm_add_epi32(O10l, E3l);
            O10l = _mm_add_epi32(O10l, E4l);
            O10l = _mm_add_epi32(O10l, E5l);
            O10l = _mm_add_epi32(O10l, E6l);
            O10l = _mm_add_epi32(O10l, E7l);
            
            O10h = _mm_add_epi32(E0h, E1h);
            O10h = _mm_add_epi32(O10h, E2h);
            O10h = _mm_add_epi32(O10h, E3h);
            O10h = _mm_add_epi32(O10h, E4h);
            O10h = _mm_add_epi32(O10h, E5h);
            O10h = _mm_add_epi32(O10h, E6h);
            O10h = _mm_add_epi32(O10h, E7h);
            
            
            
            
            /* Compute 11*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][11] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][11] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][11] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][11] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][11] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][11] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][11] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][11] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][11] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][11] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][11] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][11] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][11] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][11] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][11] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][11] ) ) );
            
            
            O11l = _mm_add_epi32(E0l, E1l);
            O11l = _mm_add_epi32(O11l, E2l);
            O11l = _mm_add_epi32(O11l, E3l);
            O11l = _mm_add_epi32(O11l, E4l);
            O11l = _mm_add_epi32(O11l, E5l);
            O11l = _mm_add_epi32(O11l, E6l);
            O11l = _mm_add_epi32(O11l, E7l);
            
            O11h = _mm_add_epi32(E0h, E1h);
            O11h = _mm_add_epi32(O11h, E2h);
            O11h = _mm_add_epi32(O11h, E3h);
            O11h = _mm_add_epi32(O11h, E4h);
            O11h = _mm_add_epi32(O11h, E5h);
            O11h = _mm_add_epi32(O11h, E6h);
            O11h = _mm_add_epi32(O11h, E7h);
            
            
            
            /* Compute 12*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][12] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][12] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][12] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][12] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][12] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][12] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][12] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][12] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][12] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][12] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][12] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][12] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][12] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][12] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][12] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][12] ) ) );
            
            
            O12l = _mm_add_epi32(E0l, E1l);
            O12l = _mm_add_epi32(O12l, E2l);
            O12l = _mm_add_epi32(O12l, E3l);
            O12l = _mm_add_epi32(O12l, E4l);
            O12l = _mm_add_epi32(O12l, E5l);
            O12l = _mm_add_epi32(O12l, E6l);
            O12l = _mm_add_epi32(O12l, E7l);
            
            O12h = _mm_add_epi32(E0h, E1h);
            O12h = _mm_add_epi32(O12h, E2h);
            O12h = _mm_add_epi32(O12h, E3h);
            O12h = _mm_add_epi32(O12h, E4h);
            O12h = _mm_add_epi32(O12h, E5h);
            O12h = _mm_add_epi32(O12h, E6h);
            O12h = _mm_add_epi32(O12h, E7h);
            
            
            
            /* Compute 13*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][13] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][13] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][13] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][13] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][13] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][13] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][13] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][13] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][13] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][13] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][13] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][13] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][13] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][13] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][13] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][13] ) ) );
            
            
            O13l = _mm_add_epi32(E0l, E1l);
            O13l = _mm_add_epi32(O13l, E2l);
            O13l = _mm_add_epi32(O13l, E3l);
            O13l = _mm_add_epi32(O13l, E4l);
            O13l = _mm_add_epi32(O13l, E5l);
            O13l = _mm_add_epi32(O13l, E6l);
            O13l = _mm_add_epi32(O13l, E7l);
            
            O13h = _mm_add_epi32(E0h, E1h);
            O13h = _mm_add_epi32(O13h, E2h);
            O13h = _mm_add_epi32(O13h, E3h);
            O13h = _mm_add_epi32(O13h, E4h);
            O13h = _mm_add_epi32(O13h, E5h);
            O13h = _mm_add_epi32(O13h, E6h);
            O13h = _mm_add_epi32(O13h, E7h);
            
            
            /* Compute O14  */
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][14] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][14] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][14] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][14] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][14] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][14] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][14] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][14] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][14] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][14] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][14] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][14] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][14] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][14] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][14] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][14] ) ) );
            
            
            O14l = _mm_add_epi32(E0l, E1l);
            O14l = _mm_add_epi32(O14l, E2l);
            O14l = _mm_add_epi32(O14l, E3l);
            O14l = _mm_add_epi32(O14l, E4l);
            O14l = _mm_add_epi32(O14l, E5l);
            O14l = _mm_add_epi32(O14l, E6l);
            O14l = _mm_add_epi32(O14l, E7l);
            
            O14h = _mm_add_epi32(E0h, E1h);
            O14h = _mm_add_epi32(O14h, E2h);
            O14h = _mm_add_epi32(O14h, E3h);
            O14h = _mm_add_epi32(O14h, E4h);
            O14h = _mm_add_epi32(O14h, E5h);
            O14h = _mm_add_epi32(O14h, E6h);
            O14h = _mm_add_epi32(O14h, E7h);
            
            
            
            /* Compute O15*/
            
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform32x32[0][15] ) ) );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform32x32[0][15] ) ) );
            E1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform32x32[1][15] ) ) );
            E1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform32x32[1][15] ) ) );
            E2l = _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform32x32[2][15] ) ) );
            E2h = _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform32x32[2][15] ) ) );
            E3l = _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform32x32[3][15] ) ) );
            E3h = _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform32x32[3][15] ) ) );
            
            
            E4l = _mm_madd_epi16( m128Tmp8, _mm_load_si128( (__m128i*)( transform32x32[4][15] ) ) );
            E4h = _mm_madd_epi16( m128Tmp9, _mm_load_si128( (__m128i*)( transform32x32[4][15] ) ) );
            E5l = _mm_madd_epi16( m128Tmp10, _mm_load_si128( (__m128i*)( transform32x32[5][15] ) ) );
            E5h = _mm_madd_epi16( m128Tmp11, _mm_load_si128( (__m128i*)( transform32x32[5][15] ) ) );
            E6l = _mm_madd_epi16( m128Tmp12, _mm_load_si128( (__m128i*)( transform32x32[6][15] ) ) );
            E6h = _mm_madd_epi16( m128Tmp13, _mm_load_si128( (__m128i*)( transform32x32[6][15] ) ) );
            E7l = _mm_madd_epi16( m128Tmp14, _mm_load_si128( (__m128i*)( transform32x32[7][15] ) ) );
            E7h = _mm_madd_epi16( m128Tmp15, _mm_load_si128( (__m128i*)( transform32x32[7][15] ) ) );
            
            
            O15l = _mm_add_epi32(E0l, E1l);
            O15l = _mm_add_epi32(O15l, E2l);
            O15l = _mm_add_epi32(O15l, E3l);
            O15l = _mm_add_epi32(O15l, E4l);
            O15l = _mm_add_epi32(O15l, E5l);
            O15l = _mm_add_epi32(O15l, E6l);
            O15l = _mm_add_epi32(O15l, E7l);
            
            O15h = _mm_add_epi32(E0h, E1h);
            O15h = _mm_add_epi32(O15h, E2h);
            O15h = _mm_add_epi32(O15h, E3h);
            O15h = _mm_add_epi32(O15h, E4h);
            O15h = _mm_add_epi32(O15h, E5h);
            O15h = _mm_add_epi32(O15h, E6h);
            O15h = _mm_add_epi32(O15h, E7h);
            /*  Compute E0  */
            
            m128Tmp0 = _mm_unpacklo_epi16(  m128iS2, m128iS6 );
            E0l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][0] ) ) );
            m128Tmp1 = _mm_unpackhi_epi16(  m128iS2, m128iS6 );
            E0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][0] ) ) );
            
            
            m128Tmp2 =  _mm_unpacklo_epi16(  m128iS10, m128iS14 );
            E0l = _mm_add_epi32(E0l, _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][0] ) ) ));
            m128Tmp3 = _mm_unpackhi_epi16(  m128iS10, m128iS14 );
            E0h = _mm_add_epi32(E0h, _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][0] ) ) ));
            
            m128Tmp4 =  _mm_unpacklo_epi16(  m128iS18, m128iS22 );
            E0l = _mm_add_epi32(E0l, _mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][0] ) ) ));
            m128Tmp5 = _mm_unpackhi_epi16(  m128iS18, m128iS22 );
            E0h = _mm_add_epi32(E0h, _mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][0] ) ) ));
            
            
            m128Tmp6 =  _mm_unpacklo_epi16(  m128iS26, m128iS30 );
            E0l = _mm_add_epi32(E0l, _mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][0] ) ) ));
            m128Tmp7 = _mm_unpackhi_epi16(  m128iS26, m128iS30 );
            E0h = _mm_add_epi32(E0h, _mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][0] ) ) ));
            
            /*  Compute E1  */
            E1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][1] ) ));
            E1h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][1] ) ) );
            E1l = _mm_add_epi32(E1l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][1] ) ) ));
            E1h = _mm_add_epi32(E1h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][1] ) ) ));
            E1l = _mm_add_epi32(E1l,_mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][1] ) ) ));
            E1h = _mm_add_epi32(E1h,_mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][1] ) ) ));
            E1l = _mm_add_epi32(E1l,_mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][1] ) ) ));
            E1h = _mm_add_epi32(E1h,_mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][1] ) ) ));
            
            /*  Compute E2  */
            E2l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][2] ) ) );
            E2h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][2] ) ) );
            E2l = _mm_add_epi32(E2l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][2] ) ) ));
            E2h = _mm_add_epi32(E2h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][2] ) ) ));
            E2l = _mm_add_epi32(E2l,_mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][2] ) ) ));
            E2h = _mm_add_epi32(E2h,_mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][2] ) ) ));
            E2l = _mm_add_epi32(E2l,_mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][2] ) ) ));
            E2h = _mm_add_epi32(E2h,_mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][2] ) ) ));
            
            
            /*  Compute E3  */
            E3l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][3] ) ) );
            E3h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][3] ) ) );
            E3l = _mm_add_epi32(E3l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][3] ) ) ));
            E3h = _mm_add_epi32(E3h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][3] ) ) ));
            E3l = _mm_add_epi32(E3l,_mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][3] ) ) ));
            E3h = _mm_add_epi32(E3h,_mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][3] ) ) ));
            E3l = _mm_add_epi32(E3l,_mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][3] ) ) ));
            E3h = _mm_add_epi32(E3h,_mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][3] ) ) ));
            
            /*  Compute E4  */
            E4l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][4] ) ) );
            E4h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][4] ) ) );
            E4l = _mm_add_epi32(E4l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][4] ) ) ));
            E4h = _mm_add_epi32(E4h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][4] ) ) ));
            E4l = _mm_add_epi32(E4l,_mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][4] ) ) ));
            E4h = _mm_add_epi32(E4h,_mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][4] ) ) ));
            E4l = _mm_add_epi32(E4l,_mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][4] ) ) ));
            E4h = _mm_add_epi32(E4h,_mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][4] ) ) ));
            
            
            /*  Compute E3  */
            E5l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][5] ) ) );
            E5h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][5] ) ) );
            E5l = _mm_add_epi32(E5l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][5] ) ) ));
            E5h = _mm_add_epi32(E5h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][5] ) ) ));
            E5l = _mm_add_epi32(E5l,_mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][5] ) ) ));
            E5h = _mm_add_epi32(E5h,_mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][5] ) ) ));
            E5l = _mm_add_epi32(E5l,_mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][5] ) ) ));
            E5h = _mm_add_epi32(E5h,_mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][5] ) ) ));
            
            
            /*  Compute E6  */
            E6l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][6] ) ) );
            E6h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][6] ) ) );
            E6l = _mm_add_epi32(E6l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][6] ) ) ));
            E6h = _mm_add_epi32(E6h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][6] ) ) ));
            E6l = _mm_add_epi32(E6l,_mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][6] ) ) ));
            E6h = _mm_add_epi32(E6h,_mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][6] ) ) ));
            E6l = _mm_add_epi32(E6l,_mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][6] ) ) ));
            E6h = _mm_add_epi32(E6h,_mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][6] ) ) ));
            
            /*  Compute E7  */
            E7l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_1[0][7] ) ) );
            E7h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_1[0][7] ) ) );
            E7l = _mm_add_epi32(E7l,_mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_1[1][7] ) ) ));
            E7h = _mm_add_epi32(E7h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_1[1][7] ) ) ));
            E7l = _mm_add_epi32(E7l,_mm_madd_epi16( m128Tmp4, _mm_load_si128( (__m128i*)( transform16x16_1[2][7] ) ) ));
            E7h = _mm_add_epi32(E7h,_mm_madd_epi16( m128Tmp5, _mm_load_si128( (__m128i*)( transform16x16_1[2][7] ) ) ));
            E7l = _mm_add_epi32(E7l,_mm_madd_epi16( m128Tmp6, _mm_load_si128( (__m128i*)( transform16x16_1[3][7] ) ) ));
            E7h = _mm_add_epi32(E7h,_mm_madd_epi16( m128Tmp7, _mm_load_si128( (__m128i*)( transform16x16_1[3][7] ) ) ));
            
            
            /*  Compute EE0 and EEE */
            
            
            m128Tmp0 = _mm_unpacklo_epi16(  m128iS4, m128iS12 );
            E00l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][0] ) ) );
            m128Tmp1 = _mm_unpackhi_epi16(  m128iS4, m128iS12 );
            E00h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][0] ) ) );
            
            m128Tmp2 = _mm_unpacklo_epi16(  m128iS20, m128iS28 );
            E00l =  _mm_add_epi32(E00l, _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][0] ) ) ));
            m128Tmp3 = _mm_unpackhi_epi16(  m128iS20, m128iS28 );
            E00h = _mm_add_epi32(E00h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][0] ) ) ));
            
            
            
            E01l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][1] ) ) );
            E01h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][1] ) ) );
            E01l =  _mm_add_epi32(E01l, _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][1] ) ) ));
            E01h = _mm_add_epi32(E01h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][1] ) ) ));
            
            E02l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][2] ) ) );
            E02h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][2] ) ) );
            E02l =  _mm_add_epi32(E02l, _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][2] ) ) ));
            E02h = _mm_add_epi32(E02h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][2] ) ) ));
            
            E03l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_2[0][3] ) ) );
            E03h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_2[0][3] ) ) );
            E03l =  _mm_add_epi32(E03l, _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_2[1][3] ) ) ));
            E03h = _mm_add_epi32(E03h,_mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_2[1][3] ) ) ));
            
            /*  Compute EE0 and EEE */
            
            
            m128Tmp0 = _mm_unpacklo_epi16(  m128iS8, m128iS24 );
            EE0l =  _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_3[0][0] ) ) );
            m128Tmp1 = _mm_unpackhi_epi16(  m128iS8, m128iS24 );
            EE0h = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_3[0][0] ) ) );
            
            m128Tmp2 =  _mm_unpacklo_epi16(  m128iS0, m128iS16 );
            EEE0l =  _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_3[1][0] ) ) );
            m128Tmp3 = _mm_unpackhi_epi16(  m128iS0, m128iS16 );
            EEE0h =  _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_3[1][0] ) ) );
            
            
            EE1l = _mm_madd_epi16( m128Tmp0, _mm_load_si128( (__m128i*)( transform16x16_3[0][1] ) ) );
            EE1h  = _mm_madd_epi16( m128Tmp1, _mm_load_si128( (__m128i*)( transform16x16_3[0][1] ) ) );
            
            EEE1l = _mm_madd_epi16( m128Tmp2, _mm_load_si128( (__m128i*)( transform16x16_3[1][1] ) ) );
            EEE1h = _mm_madd_epi16( m128Tmp3, _mm_load_si128( (__m128i*)( transform16x16_3[1][1] ) ) );
            
            /*  Compute EE    */
            
            EE2l = _mm_sub_epi32(EEE1l,EE1l);
            EE3l = _mm_sub_epi32(EEE0l,EE0l);
            EE2h = _mm_sub_epi32(EEE1h,EE1h);
            EE3h = _mm_sub_epi32(EEE0h,EE0h);
            
            EE0l = _mm_add_epi32(EEE0l,EE0l);
            EE1l = _mm_add_epi32(EEE1l,EE1l);
            EE0h = _mm_add_epi32(EEE0h,EE0h);
            EE1h = _mm_add_epi32(EEE1h,EE1h);
            /**/
            
            EE7l = _mm_sub_epi32(EE0l, E00l);
            EE6l = _mm_sub_epi32(EE1l, E01l);
            EE5l = _mm_sub_epi32(EE2l, E02l);
            EE4l = _mm_sub_epi32(EE3l, E03l);
            
            EE7h = _mm_sub_epi32(EE0h, E00h);
            EE6h = _mm_sub_epi32(EE1h, E01h);
            EE5h = _mm_sub_epi32(EE2h, E02h);
            EE4h = _mm_sub_epi32(EE3h, E03h);
            
            
            EE0l = _mm_add_epi32(EE0l, E00l);
            EE1l = _mm_add_epi32(EE1l, E01l);
            EE2l = _mm_add_epi32(EE2l, E02l);
            EE3l = _mm_add_epi32(EE3l, E03l);
            
            EE0h = _mm_add_epi32(EE0h, E00h);
            EE1h = _mm_add_epi32(EE1h, E01h);
            EE2h = _mm_add_epi32(EE2h, E02h);
            EE3h = _mm_add_epi32(EE3h, E03h);
            /*      Compute E       */
            
            E15l = _mm_sub_epi32(EE0l,E0l);
            E15l = _mm_add_epi32(E15l, m128iAdd);
            E14l = _mm_sub_epi32(EE1l,E1l);
            E14l = _mm_add_epi32(E14l, m128iAdd);
            E13l = _mm_sub_epi32(EE2l,E2l);
            E13l = _mm_add_epi32(E13l, m128iAdd);
            E12l = _mm_sub_epi32(EE3l,E3l);
            E12l = _mm_add_epi32(E12l, m128iAdd);
            E11l = _mm_sub_epi32(EE4l,E4l);
            E11l = _mm_add_epi32(E11l, m128iAdd);
            E10l = _mm_sub_epi32(EE5l,E5l);
            E10l = _mm_add_epi32(E10l, m128iAdd);
            E9l = _mm_sub_epi32(EE6l,E6l);
            E9l = _mm_add_epi32(E9l, m128iAdd);
            E8l = _mm_sub_epi32(EE7l,E7l);
            E8l = _mm_add_epi32(E8l, m128iAdd);
            
            E0l = _mm_add_epi32(EE0l,E0l);
            E0l = _mm_add_epi32(E0l, m128iAdd);
            E1l = _mm_add_epi32(EE1l,E1l);
            E1l = _mm_add_epi32(E1l, m128iAdd);
            E2l = _mm_add_epi32(EE2l,E2l);
            E2l = _mm_add_epi32(E2l, m128iAdd);
            E3l = _mm_add_epi32(EE3l,E3l);
            E3l = _mm_add_epi32(E3l, m128iAdd);
            E4l = _mm_add_epi32(EE4l,E4l);
            E4l = _mm_add_epi32(E4l, m128iAdd);
            E5l = _mm_add_epi32(EE5l,E5l);
            E5l = _mm_add_epi32(E5l, m128iAdd);
            E6l = _mm_add_epi32(EE6l,E6l);
            E6l = _mm_add_epi32(E6l, m128iAdd);
            E7l = _mm_add_epi32(EE7l,E7l);
            E7l = _mm_add_epi32(E7l, m128iAdd);
            
            
            E15h = _mm_sub_epi32(EE0h,E0h);
            E15h = _mm_add_epi32(E15h, m128iAdd);
            E14h = _mm_sub_epi32(EE1h,E1h);
            E14h = _mm_add_epi32(E14h, m128iAdd);
            E13h = _mm_sub_epi32(EE2h,E2h);
            E13h = _mm_add_epi32(E13h, m128iAdd);
            E12h = _mm_sub_epi32(EE3h,E3h);
            E12h = _mm_add_epi32(E12h, m128iAdd);
            E11h = _mm_sub_epi32(EE4h,E4h);
            E11h = _mm_add_epi32(E11h, m128iAdd);
            E10h = _mm_sub_epi32(EE5h,E5h);
            E10h = _mm_add_epi32(E10h, m128iAdd);
            E9h = _mm_sub_epi32(EE6h,E6h);
            E9h = _mm_add_epi32(E9h, m128iAdd);
            E8h = _mm_sub_epi32(EE7h,E7h);
            E8h = _mm_add_epi32(E8h, m128iAdd);
            
            E0h = _mm_add_epi32(EE0h,E0h);
            E0h = _mm_add_epi32(E0h, m128iAdd);
            E1h = _mm_add_epi32(EE1h,E1h);
            E1h = _mm_add_epi32(E1h, m128iAdd);
            E2h = _mm_add_epi32(EE2h,E2h);
            E2h = _mm_add_epi32(E2h, m128iAdd);
            E3h = _mm_add_epi32(EE3h,E3h);
            E3h = _mm_add_epi32(E3h, m128iAdd);
            E4h = _mm_add_epi32(EE4h,E4h);
            E4h = _mm_add_epi32(E4h, m128iAdd);
            E5h = _mm_add_epi32(EE5h,E5h);
            E5h = _mm_add_epi32(E5h, m128iAdd);
            E6h = _mm_add_epi32(EE6h,E6h);
            E6h = _mm_add_epi32(E6h, m128iAdd);
            E7h = _mm_add_epi32(EE7h,E7h);
            E7h = _mm_add_epi32(E7h, m128iAdd);
            
            
            m128iS0 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E0l, O0l),shift), _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift));
            m128iS1 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E1l, O1l),shift), _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift));
            m128iS2 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E2l, O2l),shift), _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift));
            m128iS3 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E3l, O3l),shift), _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift));
            m128iS4 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E4l, O4l),shift), _mm_srai_epi32(_mm_add_epi32(E4h, O4h), shift));
            m128iS5 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E5l, O5l),shift), _mm_srai_epi32(_mm_add_epi32(E5h, O5h), shift));
            m128iS6 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E6l, O6l),shift), _mm_srai_epi32(_mm_add_epi32(E6h, O6h), shift));
            m128iS7 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E7l, O7l),shift), _mm_srai_epi32(_mm_add_epi32(E7h, O7h), shift));
            m128iS8 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E8l, O8l),shift), _mm_srai_epi32(_mm_add_epi32(E8h, O8h), shift));
            m128iS9 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E9l, O9l),shift), _mm_srai_epi32(_mm_add_epi32(E9h, O9h), shift));
            m128iS10 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E10l, O10l),shift), _mm_srai_epi32(_mm_add_epi32(E10h, O10h), shift));
            m128iS11 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E11l, O11l),shift), _mm_srai_epi32(_mm_add_epi32(E11h, O11h), shift));
            m128iS12 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E12l, O12l),shift), _mm_srai_epi32(_mm_add_epi32(E12h, O12h), shift));
            m128iS13 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E13l, O13l),shift), _mm_srai_epi32(_mm_add_epi32(E13h, O13h), shift));
            m128iS14 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E14l, O14l),shift), _mm_srai_epi32(_mm_add_epi32(E14h, O14h), shift));
            m128iS15 = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(E15l, O15l),shift), _mm_srai_epi32(_mm_add_epi32(E15h, O15h), shift));
            
            m128iS31 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E0l, O0l),shift), _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift));
            m128iS30 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E1l, O1l),shift), _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift));
            m128iS29 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E2l, O2l),shift), _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift));
            m128iS28 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E3l, O3l),shift), _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift));
            m128iS27 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E4l, O4l),shift), _mm_srai_epi32(_mm_sub_epi32(E4h, O4h), shift));
            m128iS26 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E5l, O5l),shift), _mm_srai_epi32(_mm_sub_epi32(E5h, O5h), shift));
            m128iS25 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E6l, O6l),shift), _mm_srai_epi32(_mm_sub_epi32(E6h, O6h), shift));
            m128iS24 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E7l, O7l),shift), _mm_srai_epi32(_mm_sub_epi32(E7h, O7h), shift));
            m128iS23 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E8l, O8l),shift), _mm_srai_epi32(_mm_sub_epi32(E8h, O8h), shift));
            m128iS22 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E9l, O9l),shift), _mm_srai_epi32(_mm_sub_epi32(E9h, O9h), shift));
            m128iS21 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E10l, O10l),shift), _mm_srai_epi32(_mm_sub_epi32(E10h, O10h), shift));
            m128iS20 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E11l, O11l),shift), _mm_srai_epi32(_mm_sub_epi32(E11h, O11h), shift));
            m128iS19 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E12l, O12l),shift), _mm_srai_epi32(_mm_sub_epi32(E12h, O12h), shift));
            m128iS18 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E13l, O13l),shift), _mm_srai_epi32(_mm_sub_epi32(E13h, O13h), shift));
            m128iS17 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E14l, O14l),shift), _mm_srai_epi32(_mm_sub_epi32(E14h, O14h), shift));
            m128iS16 = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(E15l, O15l),shift), _mm_srai_epi32(_mm_sub_epi32(E15h, O15h), shift));
            
            if(!j){
                /*      Inverse the matrix      */
                E0l = _mm_unpacklo_epi16(m128iS0, m128iS16);
                E1l = _mm_unpacklo_epi16(m128iS1, m128iS17);
                E2l = _mm_unpacklo_epi16(m128iS2, m128iS18);
                E3l = _mm_unpacklo_epi16(m128iS3, m128iS19);
                E4l = _mm_unpacklo_epi16(m128iS4, m128iS20);
                E5l = _mm_unpacklo_epi16(m128iS5, m128iS21);
                E6l = _mm_unpacklo_epi16(m128iS6, m128iS22);
                E7l = _mm_unpacklo_epi16(m128iS7, m128iS23);
                E8l = _mm_unpacklo_epi16(m128iS8, m128iS24);
                E9l = _mm_unpacklo_epi16(m128iS9, m128iS25);
                E10l = _mm_unpacklo_epi16(m128iS10, m128iS26);
                E11l = _mm_unpacklo_epi16(m128iS11, m128iS27);
                E12l = _mm_unpacklo_epi16(m128iS12, m128iS28);
                E13l = _mm_unpacklo_epi16(m128iS13, m128iS29);
                E14l = _mm_unpacklo_epi16(m128iS14, m128iS30);
                E15l = _mm_unpacklo_epi16(m128iS15, m128iS31);
                
                O0l = _mm_unpackhi_epi16(m128iS0, m128iS16);
                O1l = _mm_unpackhi_epi16(m128iS1, m128iS17);
                O2l = _mm_unpackhi_epi16(m128iS2, m128iS18);
                O3l = _mm_unpackhi_epi16(m128iS3, m128iS19);
                O4l = _mm_unpackhi_epi16(m128iS4, m128iS20);
                O5l = _mm_unpackhi_epi16(m128iS5, m128iS21);
                O6l = _mm_unpackhi_epi16(m128iS6, m128iS22);
                O7l = _mm_unpackhi_epi16(m128iS7, m128iS23);
                O8l = _mm_unpackhi_epi16(m128iS8, m128iS24);
                O9l = _mm_unpackhi_epi16(m128iS9, m128iS25);
                O10l = _mm_unpackhi_epi16(m128iS10, m128iS26);
                O11l = _mm_unpackhi_epi16(m128iS11, m128iS27);
                O12l = _mm_unpackhi_epi16(m128iS12, m128iS28);
                O13l = _mm_unpackhi_epi16(m128iS13, m128iS29);
                O14l = _mm_unpackhi_epi16(m128iS14, m128iS30);
                O15l = _mm_unpackhi_epi16(m128iS15, m128iS31);
                
                E0h  = _mm_unpacklo_epi16(E0l, E8l);
                E1h  = _mm_unpacklo_epi16(E1l, E9l);
                E2h = _mm_unpacklo_epi16(E2l, E10l);
                E3h  = _mm_unpacklo_epi16(E3l, E11l);
                E4h  = _mm_unpacklo_epi16(E4l, E12l);
                E5h  = _mm_unpacklo_epi16(E5l, E13l);
                E6h  = _mm_unpacklo_epi16(E6l, E14l);
                E7h  = _mm_unpacklo_epi16(E7l, E15l);
                
                E8h = _mm_unpackhi_epi16(E0l, E8l);
                E9h = _mm_unpackhi_epi16(E1l, E9l);
                E10h = _mm_unpackhi_epi16(E2l, E10l);
                E11h = _mm_unpackhi_epi16(E3l, E11l);
                E12h = _mm_unpackhi_epi16(E4l, E12l);
                E13h = _mm_unpackhi_epi16(E5l, E13l);
                E14h = _mm_unpackhi_epi16(E6l, E14l);
                E15h = _mm_unpackhi_epi16(E7l, E15l);
                
                
                m128Tmp0 = _mm_unpacklo_epi16(E0h, E4h);
                m128Tmp1 = _mm_unpacklo_epi16(E1h, E5h);
                m128Tmp2 = _mm_unpacklo_epi16(E2h, E6h);
                m128Tmp3 = _mm_unpacklo_epi16(E3h, E7h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS0  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS1  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS2  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS3  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpackhi_epi16(E0h, E4h);
                m128Tmp1 = _mm_unpackhi_epi16(E1h, E5h);
                m128Tmp2 = _mm_unpackhi_epi16(E2h, E6h);
                m128Tmp3 = _mm_unpackhi_epi16(E3h, E7h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS4  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS5  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS6  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS7  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpacklo_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpacklo_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpacklo_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpacklo_epi16(E11h, E15h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS8  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS9  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS10  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS11  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpackhi_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpackhi_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpackhi_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpackhi_epi16(E11h, E15h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS12  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS13  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS14  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS15  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                /*  */
                E0h  = _mm_unpacklo_epi16(O0l, O8l);
                E1h  = _mm_unpacklo_epi16(O1l, O9l);
                E2h = _mm_unpacklo_epi16(O2l, O10l);
                E3h  = _mm_unpacklo_epi16(O3l, O11l);
                E4h  = _mm_unpacklo_epi16(O4l, O12l);
                E5h  = _mm_unpacklo_epi16(O5l, O13l);
                E6h  = _mm_unpacklo_epi16(O6l, O14l);
                E7h  = _mm_unpacklo_epi16(O7l, O15l);
                
                E8h = _mm_unpackhi_epi16(O0l, O8l);
                E9h = _mm_unpackhi_epi16(O1l, O9l);
                E10h = _mm_unpackhi_epi16(O2l, O10l);
                E11h = _mm_unpackhi_epi16(O3l, O11l);
                E12h = _mm_unpackhi_epi16(O4l, O12l);
                E13h = _mm_unpackhi_epi16(O5l, O13l);
                E14h = _mm_unpackhi_epi16(O6l, O14l);
                E15h = _mm_unpackhi_epi16(O7l, O15l);
                
                m128Tmp0 = _mm_unpacklo_epi16(E0h, E4h);
                m128Tmp1 = _mm_unpacklo_epi16(E1h, E5h);
                m128Tmp2 = _mm_unpacklo_epi16(E2h, E6h);
                m128Tmp3 = _mm_unpacklo_epi16(E3h, E7h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS16  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS17  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS18  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS19  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpackhi_epi16(E0h, E4h);
                m128Tmp1 = _mm_unpackhi_epi16(E1h, E5h);
                m128Tmp2 = _mm_unpackhi_epi16(E2h, E6h);
                m128Tmp3 = _mm_unpackhi_epi16(E3h, E7h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS20  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS21  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS22  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS23  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpacklo_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpacklo_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpacklo_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpacklo_epi16(E11h, E15h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS24  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS25  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS26  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS27  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp0 = _mm_unpackhi_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpackhi_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpackhi_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpackhi_epi16(E11h, E15h);
                
                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS28  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS29  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                
                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS30  = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS31  = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                /*  */
                _mm_store_si128((__m128i*)( src+i ), m128iS0);
                _mm_store_si128((__m128i*)( src + 32 + i ), m128iS1);
                _mm_store_si128((__m128i*)( src + 64 + i ), m128iS2);
                _mm_store_si128((__m128i*)( src + 96 + i ), m128iS3);
                _mm_store_si128((__m128i*)( src + 128 + i ), m128iS4);
                _mm_store_si128((__m128i*)( src + 160 + i ), m128iS5);
                _mm_store_si128((__m128i*)( src + 192 + i ), m128iS6);
                _mm_store_si128((__m128i*)( src + 224 + i), m128iS7);
                _mm_store_si128((__m128i*)( src + 256 + i), m128iS8);
                _mm_store_si128((__m128i*)( src + 288 + i ), m128iS9);
                _mm_store_si128((__m128i*)( src + 320 + i), m128iS10);
                _mm_store_si128((__m128i*)( src + 352 + i), m128iS11);
                _mm_store_si128((__m128i*)( src + 384 + i ), m128iS12);
                _mm_store_si128((__m128i*)( src + 416 + i), m128iS13);
                _mm_store_si128((__m128i*)( src + 448 + i), m128iS14);
                _mm_store_si128((__m128i*)( src + 480 + i ), m128iS15);
                _mm_store_si128((__m128i*)( src + 512 +i ), m128iS16);
                _mm_store_si128((__m128i*)( src + 544 + i ), m128iS17);
                _mm_store_si128((__m128i*)( src + 576 + i ), m128iS18);
                _mm_store_si128((__m128i*)( src + 608 + i ), m128iS19);
                _mm_store_si128((__m128i*)( src + 640 + i ), m128iS20);
                _mm_store_si128((__m128i*)( src + 672 + i ), m128iS21);
                _mm_store_si128((__m128i*)( src + 704 + i ), m128iS22);
                _mm_store_si128((__m128i*)( src + 736 + i), m128iS23);
                _mm_store_si128((__m128i*)( src + 768 + i), m128iS24);
                _mm_store_si128((__m128i*)( src + 800 + i ), m128iS25);
                _mm_store_si128((__m128i*)( src + 832 + i), m128iS26);
                _mm_store_si128((__m128i*)( src + 864 + i), m128iS27);
                _mm_store_si128((__m128i*)( src + 896 + i ), m128iS28);
                _mm_store_si128((__m128i*)( src + 928 + i), m128iS29);
                _mm_store_si128((__m128i*)( src + 960 + i), m128iS30);
                _mm_store_si128((__m128i*)( src + 992+ i ), m128iS31);
                
                if(i <= 16 ) {
                    int k = i+8;
                    m128iS0   = _mm_load_si128( (__m128i*)( src + k ) );
                    m128iS1   = _mm_load_si128( (__m128i*)( src + 32 + k) );
                    m128iS2   = _mm_load_si128( (__m128i*)( src + 64 + k) );
                    m128iS3   = _mm_load_si128( (__m128i*)( src + 96 + k) );
                    m128iS4   = _mm_load_si128( (__m128i*)( src + 128 + k ) );
                    m128iS5   = _mm_load_si128( (__m128i*)( src + 160 + k) );
                    m128iS6   = _mm_load_si128( (__m128i*)( src + 192 +k) );
                    m128iS7   = _mm_load_si128( (__m128i*)( src + 224 +k ) );
                    m128iS8   = _mm_load_si128( (__m128i*)( src + 256 +k ) );
                    m128iS9   = _mm_load_si128( (__m128i*)( src + 288 +k ));
                    m128iS10   = _mm_load_si128( (__m128i*)( src + 320 + k ) );
                    m128iS11   = _mm_load_si128( (__m128i*)( src + 352 + k));
                    m128iS12   = _mm_load_si128( (__m128i*)( src + 384 +k ) );
                    m128iS13   = _mm_load_si128( (__m128i*)( src + 416 + k) );
                    m128iS14   = _mm_load_si128( (__m128i*)( src + 448 + k) );
                    m128iS15   = _mm_load_si128( (__m128i*)( src + 480 + k) );
                    
                    m128iS16   = _mm_load_si128( (__m128i*)( src + 512 + k) );
                    m128iS17   = _mm_load_si128( (__m128i*)( src + 544 + k) );
                    m128iS18   = _mm_load_si128( (__m128i*)( src + 576 + k) );
                    m128iS19   = _mm_load_si128( (__m128i*)( src + 608 + k) );
                    m128iS20   = _mm_load_si128( (__m128i*)( src + 640 + k ) );
                    m128iS21   = _mm_load_si128( (__m128i*)( src + 672 + k) );
                    m128iS22   = _mm_load_si128( (__m128i*)( src + 704 + k) );
                    m128iS23   = _mm_load_si128( (__m128i*)( src + 736 + k ) );
                    m128iS24   = _mm_load_si128( (__m128i*)( src + 768 + k ) );
                    m128iS25   = _mm_load_si128( (__m128i*)( src + 800 + k ));
                    m128iS26   = _mm_load_si128( (__m128i*)( src + 832 + k ) );
                    m128iS27   = _mm_load_si128( (__m128i*)( src + 864 + k));
                    m128iS28   = _mm_load_si128( (__m128i*)( src + 896 + k ) );
                    m128iS29   = _mm_load_si128( (__m128i*)( src + 928 + k) );
                    m128iS30   = _mm_load_si128( (__m128i*)( src + 960 + k) );
                    m128iS31   = _mm_load_si128( (__m128i*)( src + 992 + k) );
                } else {
                    m128iS0   = _mm_load_si128( (__m128i*)( src) );
                    m128iS1   = _mm_load_si128( (__m128i*)( src + 128) );
                    m128iS2   = _mm_load_si128( (__m128i*)( src + 256 ) );
                    m128iS3   = _mm_load_si128( (__m128i*)( src + 384 ) );
                    m128iS4   = _mm_loadu_si128( (__m128i*)( src  + 512 ) );
                    m128iS5   = _mm_load_si128( (__m128i*)( src + 640 ) );
                    m128iS6   = _mm_load_si128( (__m128i*)( src  + 768) );
                    m128iS7   = _mm_load_si128( (__m128i*)( src + 896) );
                    m128iS8   = _mm_load_si128( (__m128i*)( src + 8) );
                    m128iS9   = _mm_load_si128( (__m128i*)( src + 128 +8));
                    m128iS10   = _mm_load_si128( (__m128i*)( src + 256  +8 ) );
                    m128iS11   = _mm_load_si128( (__m128i*)( src + 384 +8));
                    m128iS12   = _mm_loadu_si128( (__m128i*)( src + 512 +8) );
                    m128iS13   = _mm_load_si128( (__m128i*)( src + 640 +8) );
                    m128iS14   = _mm_load_si128( (__m128i*)( src + 768 +8) );
                    m128iS15   = _mm_load_si128( (__m128i*)( src + 896 +8) );
                    m128iS16   = _mm_load_si128( (__m128i*)( src + 16) );
                    m128iS17   = _mm_load_si128( (__m128i*)( src + 128 +16));
                    m128iS18   = _mm_load_si128( (__m128i*)( src + 256  +16 ) );
                    m128iS19   = _mm_load_si128( (__m128i*)( src + 384 +16));
                    m128iS20   = _mm_loadu_si128( (__m128i*)( src + 512 +16) );
                    m128iS21   = _mm_load_si128( (__m128i*)( src + 640 +16) );
                    m128iS22   = _mm_load_si128( (__m128i*)( src + 768 +16) );
                    m128iS23   = _mm_load_si128( (__m128i*)( src + 896 +16) );
                    m128iS24   = _mm_load_si128( (__m128i*)( src + 24) );
                    m128iS25   = _mm_load_si128( (__m128i*)( src + 128 +24));
                    m128iS26   = _mm_load_si128( (__m128i*)( src + 256  +24 ) );
                    m128iS27   = _mm_load_si128( (__m128i*)( src + 384 +24));
                    m128iS28   = _mm_loadu_si128( (__m128i*)( src + 512 +24) );
                    m128iS29   = _mm_load_si128( (__m128i*)( src + 640 +24) );
                    m128iS30   = _mm_load_si128( (__m128i*)( src + 768 +24) );
                    m128iS31   = _mm_load_si128( (__m128i*)( src + 896 +24) );
                    shift = shift_2nd;
                    m128iAdd  = _mm_set1_epi32( add_2nd );
                }
                
            } else {
                int k, m = 0;
                _mm_storeu_si128( (__m128i*)( src     ), m128iS0 );
                _mm_storeu_si128( (__m128i*)( src + 8 ), m128iS1 );
                _mm_storeu_si128( (__m128i*)( src + 16 ), m128iS2 );
                _mm_storeu_si128( (__m128i*)( src + 24), m128iS3 );
                _mm_storeu_si128( (__m128i*)( src + 128 ), m128iS4 );
                _mm_storeu_si128( (__m128i*)( src + 128 + 8 ), m128iS5 );
                _mm_storeu_si128( (__m128i*)( src + 128 + 16), m128iS6 );
                _mm_storeu_si128( (__m128i*)( src + 128 + 24), m128iS7 );
                _mm_storeu_si128( (__m128i*)( src + 256 ), m128iS8 );
                _mm_storeu_si128( (__m128i*)( src + 256 + 8), m128iS9 );
                _mm_storeu_si128( (__m128i*)( src + 256 + 16), m128iS10 );
                _mm_storeu_si128( (__m128i*)( src + 256 + 24), m128iS11 );
                _mm_storeu_si128( (__m128i*)( src + 384 ), m128iS12 );
                _mm_storeu_si128( (__m128i*)( src +384 + 8), m128iS13 );
                _mm_storeu_si128( (__m128i*)( src + 384 + 16), m128iS14 );
                _mm_storeu_si128( (__m128i*)( src + 384 + 24), m128iS15 );
                
                
                _mm_storeu_si128( (__m128i*)( src  + 512   ), m128iS16 );
                _mm_storeu_si128( (__m128i*)( src + 512 + 8), m128iS17 );
                _mm_storeu_si128( (__m128i*)( src + 512 +16 ), m128iS18 );
                _mm_storeu_si128( (__m128i*)( src + 512 +24), m128iS19 );
                _mm_storeu_si128( (__m128i*)( src + 640 ), m128iS20 );
                _mm_storeu_si128( (__m128i*)( src + 640 + 8), m128iS21 );
                _mm_storeu_si128( (__m128i*)( src + 640 + 16), m128iS22 );
                _mm_storeu_si128( (__m128i*)( src + 640 + 24), m128iS23 );
                _mm_storeu_si128( (__m128i*)( src + 768 ), m128iS24 );
                _mm_storeu_si128( (__m128i*)( src + 768 + 8), m128iS25 );
                _mm_storeu_si128( (__m128i*)( src + 768 + 16), m128iS26 );
                _mm_storeu_si128( (__m128i*)( src + 768 + 24), m128iS27 );
                _mm_storeu_si128( (__m128i*)( src + 896 ), m128iS28 );
                _mm_storeu_si128( (__m128i*)( src + 896 + 8), m128iS29 );
                _mm_storeu_si128( (__m128i*)( src + 896 + 16), m128iS30 );
                _mm_storeu_si128( (__m128i*)( src + 896 + 24), m128iS31 );
                dst = (pixel*)_dst + (i*stride);
                for ( k = 0; k < 8; k++) {
                    dst[0] = av_clip_pixel(dst[0]+av_clip_int16(src[m]));
                    dst[1] = av_clip_pixel(dst[1]+av_clip_int16(src[m+8]));
                    dst[2] = av_clip_pixel(dst[2]+av_clip_int16(src[m+16]));
                    dst[3] = av_clip_pixel(dst[3]+av_clip_int16(src[m+24]));
                    dst[4] = av_clip_pixel(dst[4]+av_clip_int16(src[m+128]));
                    dst[5] = av_clip_pixel(dst[5]+av_clip_int16(src[m+128+8]));
                    dst[6] = av_clip_pixel(dst[6]+av_clip_int16(src[m+128+16]));
                    dst[7] = av_clip_pixel(dst[7]+av_clip_int16(src[m+128+24]));
                    
                    dst[8] = av_clip_pixel(dst[8]+av_clip_int16(src[m+256]));
                    dst[9] = av_clip_pixel(dst[9]+av_clip_int16(src[m+256+8]));
                    dst[10] = av_clip_pixel(dst[10]+av_clip_int16(src[m+256+16]));
                    dst[11] = av_clip_pixel(dst[11]+av_clip_int16(src[m+256+24]));
                    dst[12] = av_clip_pixel(dst[12]+av_clip_int16(src[m+384]));
                    dst[13] = av_clip_pixel(dst[13]+av_clip_int16(src[m+384+8]));
                    dst[14] = av_clip_pixel(dst[14]+av_clip_int16(src[m+384+16]));
                    dst[15] = av_clip_pixel(dst[15]+av_clip_int16(src[m+384+24]));
                    
                    dst[16] = av_clip_pixel(dst[16]+av_clip_int16(src[m+512]));
                    dst[17] = av_clip_pixel(dst[17]+av_clip_int16(src[m+512+8]));
                    dst[18] = av_clip_pixel(dst[18]+av_clip_int16(src[m+512+16]));
                    dst[19] = av_clip_pixel(dst[19]+av_clip_int16(src[m+512+24]));
                    dst[20] = av_clip_pixel(dst[20]+av_clip_int16(src[m+640]));
                    dst[21] = av_clip_pixel(dst[21]+av_clip_int16(src[m+640+8]));
                    dst[22] = av_clip_pixel(dst[22]+av_clip_int16(src[m+640+16]));
                    dst[23] = av_clip_pixel(dst[23]+av_clip_int16(src[m+640+24]));
                    
                    dst[24] = av_clip_pixel(dst[24]+av_clip_int16(src[m+768]));
                    dst[25] = av_clip_pixel(dst[25]+av_clip_int16(src[m+768+8]));
                    dst[26] = av_clip_pixel(dst[26]+av_clip_int16(src[m+768+16]));
                    dst[27] = av_clip_pixel(dst[27]+av_clip_int16(src[m+768+24]));
                    dst[28] = av_clip_pixel(dst[28]+av_clip_int16(src[m+896]));
                    dst[29] = av_clip_pixel(dst[29]+av_clip_int16(src[m+896+8]));
                    dst[30] = av_clip_pixel(dst[30]+av_clip_int16(src[m+896+16]));
                    dst[31] = av_clip_pixel(dst[31]+av_clip_int16(src[m+896+24]));
                    
                    m +=1;
                    dst += stride;
                }
                if(i<=16){
                    int k = (i+8)*4;
                    m128iS0   = _mm_load_si128( (__m128i*)( src + k) );
                    m128iS1   = _mm_load_si128( (__m128i*)( src + 128 + k) );
                    m128iS2   = _mm_load_si128( (__m128i*)( src + 256 + k ) );
                    m128iS3   = _mm_load_si128( (__m128i*)( src + 384 + k ) );
                    m128iS4   = _mm_loadu_si128( (__m128i*)( src  + 512 + k ) );
                    m128iS5   = _mm_load_si128( (__m128i*)( src + 640 + k ) );
                    m128iS6   = _mm_load_si128( (__m128i*)( src  + 768 + k) );
                    m128iS7   = _mm_load_si128( (__m128i*)( src + 896 + k) );
                    m128iS8   = _mm_load_si128( (__m128i*)( src + 8 + k) );
                    m128iS9   = _mm_load_si128( (__m128i*)( src + 128 +8 + k));
                    m128iS10   = _mm_load_si128( (__m128i*)( src + 256  +8  + k) );
                    m128iS11   = _mm_load_si128( (__m128i*)( src + 384 +8 + k));
                    m128iS12   = _mm_loadu_si128( (__m128i*)( src + 512 +8 + k) );
                    m128iS13   = _mm_load_si128( (__m128i*)( src + 640 +8 + k) );
                    m128iS14   = _mm_load_si128( (__m128i*)( src + 768 +8 + k) );
                    m128iS15   = _mm_load_si128( (__m128i*)( src + 896 +8 + k) );
                    m128iS16   = _mm_load_si128( (__m128i*)( src + 16 + k) );
                    m128iS17   = _mm_load_si128( (__m128i*)( src + 128 +16 + k));
                    m128iS18   = _mm_load_si128( (__m128i*)( src + 256  +16 + k) );
                    m128iS19   = _mm_load_si128( (__m128i*)( src + 384 +16 + k));
                    m128iS20   = _mm_loadu_si128( (__m128i*)( src + 512 +16 + k) );
                    m128iS21   = _mm_load_si128( (__m128i*)( src + 640 +16 + k) );
                    m128iS22   = _mm_load_si128( (__m128i*)( src + 768 +16 + k) );
                    m128iS23   = _mm_load_si128( (__m128i*)( src + 896 +16 + k) );
                    m128iS24   = _mm_load_si128( (__m128i*)( src + 24 + k) );
                    m128iS25   = _mm_load_si128( (__m128i*)( src + 128 +24 + k));
                    m128iS26   = _mm_load_si128( (__m128i*)( src + 256  +24  + k) );
                    m128iS27   = _mm_load_si128( (__m128i*)( src + 384 +24 + k));
                    m128iS28   = _mm_loadu_si128( (__m128i*)( src + 512 +24 + k) );
                    m128iS29   = _mm_load_si128( (__m128i*)( src + 640 +24 + k) );
                    m128iS30   = _mm_load_si128( (__m128i*)( src + 768 +24 + k) );
                    m128iS31   = _mm_load_si128( (__m128i*)( src + 896 +24 + k) );
                }
            }
        }
    }
#else
#define IT32x32_even(i,w) ( src[ 0*w] * transform[ 0][i] ) + ( src[16*w] * transform[16][i] )
#define IT32x32_odd(i,w)  ( src[ 8*w] * transform[ 8][i] ) + ( src[24*w] * transform[24][i] )
#define IT16x16(i,w)      ( src[ 4*w] * transform[ 4][i] ) + ( src[12*w] * transform[12][i] ) + ( src[20*w] * transform[20][i] ) + ( src[28*w] * transform[28][i] )
#define IT8x8(i,w)        ( src[ 2*w] * transform[ 2][i] ) + ( src[ 6*w] * transform[ 6][i] ) + ( src[10*w] * transform[10][i] ) + ( src[14*w] * transform[14][i] ) + \
( src[18*w] * transform[18][i] ) + ( src[22*w] * transform[22][i] ) + ( src[26*w] * transform[26][i] ) + ( src[30*w] * transform[30][i] )
#define IT4x4(i,w)        ( src[ 1*w] * transform[ 1][i] ) + ( src[ 3*w] * transform[ 3][i] ) + ( src[ 5*w] * transform[ 5][i] ) + ( src[ 7*w] * transform[ 7][i] ) + \
( src[ 9*w] * transform[ 9][i] ) + ( src[11*w] * transform[11][i] ) + ( src[13*w] * transform[13][i] ) + ( src[15*w] * transform[15][i] ) + \
( src[17*w] * transform[17][i] ) + ( src[19*w] * transform[19][i] ) + ( src[21*w] * transform[21][i] ) + ( src[23*w] * transform[23][i] ) + \
( src[25*w] * transform[25][i] ) + ( src[27*w] * transform[27][i] ) + ( src[29*w] * transform[29][i] ) + ( src[31*w] * transform[31][i] )
    int i,j;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int shift = 7;
    int add = 1 << (shift - 1);
    int16_t *src = coeffs;
    for (i = 0; i < 32; i++) {
#ifndef OPTIMIZATION_ENABLE
        TR_32_1(src, src);
#else
        int e_32[16];
        int o_32[16];
        int e_16[8];
        int e_8[4];
        int odd;
        const int e0 = IT32x32_even(0,32);
        const int e1 = IT32x32_even(1,32);
        const int o0 = IT32x32_odd(0,32);
        const int o1 = IT32x32_odd(1,32);
        e_8[0] = e0 + o0;
        e_8[1] = e1 + o1;
        e_8[2] = e1 - o1;
        e_8[3] = e0 - o0;
        for (j = 0; j < 4; j++) {
            odd       = IT16x16(j,32);
            e_16[  j] = e_8[j] + odd;
            e_16[7-j] = e_8[j] - odd;
        }
        for (j = 0; j < 8; j++) {
            odd        = IT8x8(j,32);
            e_32[   j] = e_16[j] + odd;
            e_32[15-j] = e_16[j] - odd;
        }
        for (j = 0; j < 16; j++){
            o_32[j] = IT4x4(j,32);
            
        }
        
        for (j = 0; j < 16; j++) {
            odd        = o_32[j];
            
            SCALE(src[(   j)*32], (e_32[j] + odd));
            SCALE(src[(31-j)*32], (e_32[j] - odd));
        }
        
#endif
        src++;
    }
    src   = coeffs;
    shift = 20 - BIT_DEPTH;
    add   = 1 << (shift - 1);
    for (i = 0; i < 32; i++) {
#ifndef OPTIMIZATION_ENABLE
        TR_32_2(dst, coeffs);
        coeffs += 32;
#else
        int e_32[16];
        int e_16[8];
        int e_8[4];
        int odd;
        const int e0 = IT32x32_even(0,1);
        const int e1 = IT32x32_even(1,1);
        const int o0 = IT32x32_odd(0,1);
        const int o1 = IT32x32_odd(1,1);
        e_8[0] = e0 + o0;
        e_8[1] = e1 + o1;
        e_8[2] = e1 - o1;
        e_8[3] = e0 - o0;
        for (j = 0; j < 4; j++) {
            odd       = IT16x16(j,1);
            e_16[  j] = e_8[j] + odd;
            e_16[7-j] = e_8[j] - odd;
        }
        for (j = 0; j < 8; j++) {
            odd        = IT8x8(j,1);
            e_32[   j] = e_16[j] + odd;
            e_32[15-j] = e_16[j] - odd;
        }
        pixel dst1[32*32];
        memcpy(dst1, dst, 32*32*sizeof(pixel));
        for (j = 0; j < 16; j++) {
            odd       = IT4x4(j,1);
            ADD_AND_SCALE(dst[   j], e_32[j] + odd);
            ADD_AND_SCALE(dst[31-j], e_32[j] - odd);
        }
        src += 32;
#endif
        dst += stride;
    }
#undef IT32x32_even
#undef IT32x32_odd
#undef IT16x16
#undef IT8x8
#undef IT4x4
#endif
}


static void FUNC(sao_band_filter_wpp)( uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int width, int height, int c_idx, int class_index)
{
    uint8_t *dst = _dst;
    uint8_t *src = _src;
    ptrdiff_t stride = _stride;
    int band_table[32] = { 0 };
    int k, y, x;
    int chroma = c_idx!=0;
    int shift = BIT_DEPTH - 5;
    int *sao_offset_val = sao->offset_val[c_idx];
    int sao_left_class = sao->band_position[c_idx];
    
    int init_y = 0, init_x =0;
    switch(class_index) {
        case 0:
            if(!borders[2] )
                width -= ((8>>chroma)+2) ;
            if(!borders[3] )
                height -= ((4>>chroma)+2);
            break;
        case 1:
            init_y = -(4>>chroma)-2;
            if(!borders[2] )
                width -= ((8>>chroma)+2);
            height = (4>>chroma)+2;
            break;
        case 2:
            init_x = -(8>>chroma)-2;
            width = (8>>chroma)+2;
            if(!borders[3])
                height -= ((4>>chroma)+2);
            break;
        case 3:
            init_y = -(4>>chroma)-2;
            init_x = -(8>>chroma)-2;
            width = (8>>chroma)+2;
            height = (4>>chroma)+2;
            break;
    }
    dst = dst + (init_y*_stride + init_x);
    src = src + (init_y*_stride + init_x);
    for (k = 0; k < 4; k++)
        band_table[(k + sao_left_class) & 31] = k + 1;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                dst[x] = av_clip_pixel(src[x] + sao_offset_val[band_table[src[x] >> shift]]);
                x++;
                dst[x] = av_clip_pixel(src[x] + sao_offset_val[band_table[src[x] >> shift]]);
            }
            dst += stride;
            src += stride;
        }
}


static void FUNC(sao_edge_filter_wpp)(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, int class_index)
{
    int x, y;
    uint8_t *dst = _dst;   // put here pixel
    uint8_t *src = _src;
    ptrdiff_t stride = _stride;
    int chroma = c_idx!=0;
    //struct SAOParams *sao;
    int *sao_offset_val = sao->offset_val[c_idx];
    int sao_eo_class = sao->eo_class[c_idx];
    
    const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };
    
    int init_x = 0, init_y = 0, width = _width, height = _height;
    
#define CMP(a, b) ((a) > (b) ? 1 : ((a) == (b) ? 0 : -1))
    
    switch(class_index) {
        case 0:
            if(!borders[2] )
                width -= ((8>>chroma)+2) ;
            if(!borders[3] )
                height -= ((4>>chroma)+2);
            break;
        case 1:
            init_y = -(4>>chroma)-2;
            if(!borders[2] )
                width -= ((8>>chroma)+2);
            height = (4>>chroma)+2;
            break;
        case 2:
            init_x = -(8>>chroma)-2;
            width = (8>>chroma)+2;
            if(!borders[3])
                height -= ((4>>chroma)+2);
            break;
        case 3:
            init_y = -(4>>chroma)-2;
            init_x = -(8>>chroma)-2;
            width = (8>>chroma)+2;
            height = (4>>chroma)+2;
            break;
    }
    dst = dst + (init_y*_stride + init_x);
    src = src + (init_y*_stride + init_x);
    init_y = init_x = 0;
    if (sao_eo_class != SAO_EO_VERT && class_index<=1) {
        if (borders[0]) {
            int offset_val = sao_offset_val[0];
            int y_stride   = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride += stride;
            }
            init_x = 1;
        }
        if (borders[2]) {
            int offset_val = sao_offset_val[0];
            int x_stride   = _width-1;
            for (x = 0; x < height; x++) {
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride += stride;
            }
            width --;
        }
        
    }
    if (sao_eo_class != SAO_EO_HORIZ && class_index!=1 && class_index!=3) {
        if (borders[1]){
            int offset_val = sao_offset_val[0];
            for (x = init_x; x < width; x++) {
                dst[x] = av_clip_pixel(src[x] + offset_val);
            }
            init_y = 1;
        }
        if (borders[3]){
            int offset_val = sao_offset_val[0];
            int y_stride   = stride * (_height-1);
            for (x = init_x; x < width; x++) {
                dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + offset_val);
            }
            height--;
        }
    }
    {
        int y_stride     = init_y * stride;
        int pos_0_0      = pos[sao_eo_class][0][0];
        int pos_0_1      = pos[sao_eo_class][0][1];
        int pos_1_0      = pos[sao_eo_class][1][0];
        int pos_1_1      = pos[sao_eo_class][1][1];
        
        int y_stride_0_1 = (init_y + pos_0_1) * stride;
        int y_stride_1_1 = (init_y + pos_1_1) * stride;
        for (y = init_y; y < height; y++) {
            for (x = init_x; x < width; x++) {
                int diff0         = CMP(src[x + y_stride], src[x + pos_0_0 + y_stride_0_1]);
                int diff1         = CMP(src[x + y_stride], src[x + pos_1_0 + y_stride_1_1]);
                int offset_val    = edge_idx[2 + diff0 + diff1];
                dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + sao_offset_val[offset_val]);
            }
            y_stride     += stride;
            y_stride_0_1 += stride;
            y_stride_1_1 += stride;
        }
    }
#undef CMP
}

static void FUNC(sao_band_filter)(uint8_t * _dst, uint8_t *_src, ptrdiff_t _stride, int *sao_offset_val,
                                  int sao_left_class, int width, int height)
{
    pixel *dst = (pixel*)_dst;
    pixel *src = (pixel*)_src;
    ptrdiff_t stride = _stride/sizeof(pixel);
    int band_table[32] = { 0 };
    int k, y, x;
    int shift = BIT_DEPTH - 5;

    for (k = 0; k < 4; k++)
        band_table[(k + sao_left_class) & 31] = k + 1;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel(src[x] + sao_offset_val[band_table[src[x] >> shift]]);
#ifndef GCC_OPTIMIZATION_ENABLE
            x++;
            dst[x] = av_clip_pixel(src[x] + sao_offset_val[band_table[src[x] >> shift]]);
#endif
        }
        dst += stride;
        src += stride;
    }
}

static void FUNC(sao_edge_filter)(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, int *sao_offset_val,
                                  int sao_eo_class, int at_top_border, int at_bottom_border,
                                  int at_left_border, int at_right_border,
                                  int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    pixel *src = (pixel*)_src;
    ptrdiff_t stride = _stride/sizeof(pixel);

    const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };

    int init_x = 0, init_y = 0;

#ifndef OPTIMIZATION_ENABLE
    int border_edge_idx = 0;
#define DST(x, y) dst[(x) + stride * (y)]
#define SRC(x, y) src[(x) + stride * (y)]

#define FILTER(x, y, edge_idx)                                      \
    DST(x, y) = av_clip_pixel(SRC(x, y) + sao_offset_val[edge_idx])

#define DIFF(x, y, k) CMP(SRC(x, y), SRC((x) + pos[sao_eo_class][(k)][0],       \
                                         (y) + pos[sao_eo_class][(k)][1]))
#endif
#define CMP(a, b) ((a) > (b) ? 1 : ((a) == (b) ? 0 : -1))

    if (sao_eo_class != SAO_EO_VERT) {
        if (at_left_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            int y_stride   = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride += stride;
            }
#else
            for (y = 0; y < height; y++)
                FILTER(0, y, border_edge_idx);
#endif
            init_x = 1;
        }
        if (at_right_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            int x_stride   = width - 1;
            for (x = 0; x < height; x++) {
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride += stride;
            }
#else
            for (x = 0; x < height; x++)
                FILTER(width - 1, x, border_edge_idx);
#endif
            width--;
        }
    }
    if (sao_eo_class != SAO_EO_HORIZ) {
        if (at_top_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            for (x = init_x; x < width; x++) {
                dst[x] = av_clip_pixel(src[x] + offset_val);
            }
#else
            for (x = init_x; x < width; x++)
                FILTER(x, 0, border_edge_idx);
#endif
            init_y = 1;
        }
        if (at_bottom_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            int y_stride   = stride * (height - 1);
            for (x = init_x; x < width; x++) {
                dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + offset_val);
            }
#else
            for (x = init_x; x < width; x++)
                FILTER(x, height - 1, border_edge_idx);
#endif
            height--;
        }
    }
#ifdef OPTIMIZATION_ENABLE
    {
    int y_stride     = init_y * stride;
    int pos_0_0      = pos[sao_eo_class][0][0];
    int pos_0_1      = pos[sao_eo_class][0][1];
    int pos_1_0      = pos[sao_eo_class][1][0];
    int pos_1_1      = pos[sao_eo_class][1][1];
    int y_stride_0_1 = (init_y + pos_0_1) * stride;
    int y_stride_1_1 = (init_y + pos_1_1) * stride;
    for (y = init_y; y < height; y++) {
        for (x = init_x; x < width; x++) {
            int diff0         = CMP(src[x + y_stride], src[x + pos_0_0 + y_stride_0_1]);
            int diff1         = CMP(src[x + y_stride], src[x + pos_1_0 + y_stride_1_1]);
            int offset_val    = edge_idx[2 + diff0 + diff1];
            dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + sao_offset_val[offset_val]);
        }
        y_stride     += stride;
        y_stride_0_1 += stride;
        y_stride_1_1 += stride;
    }
    }
#else
    for (y = init_y; y < height; y++) {
        for (x = init_x; x < width; x++)
            FILTER(x, y, edge_idx[2 + DIFF(x, y, 0) + DIFF(x, y, 1)]);
    }
#endif

#ifndef OPTIMIZATION_ENABLE
#undef DST
#undef SRC
#undef FILTER
#undef DIFF
#endif
#undef CMP
}

#undef SET
#undef SCALE
#undef ADD_AND_SCALE
#undef TR_4
#undef TR_4_1
#undef TR_4_2
#undef TR_8
#undef TR_8_1
#undef TR_8_2
#undef TR_16
#undef TR_16_1
#undef TR_16_2
#undef TR_32
#undef TR_32_1
#undef TR_32_2

#ifdef SSE_MC
static void FUNC(put_hevc_qpel_pixels)(int16_t *dst, ptrdiff_t dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int width, int height)
{
    int x, y;
    __m128i x1, x2, x3;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    if(width == 4){
        for (y = 0; y < height; y++) {
            x1= _mm_loadu_si128((__m128i*)&src[0]);
            x2 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));
            x2= _mm_slli_epi16(x2,14 - BIT_DEPTH);
            _mm_storeu_si128(&dst[0], x2);
         /*   dst[0]= _mm_extract_epi16(x2,0);
            dst[1]= _mm_extract_epi16(x2,1);
            dst[2]= _mm_extract_epi16(x2,2);
            dst[3]= _mm_extract_epi16(x2,3);*/

            src += srcstride;
            dst += dststride;
        }
    }
    else
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x+=16) {

                    x1= _mm_loadu_si128((__m128i*)&src[x]);
                    x2 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));

                    x3 = _mm_unpackhi_epi8(x1,_mm_set1_epi8(0));

                x2= _mm_slli_epi16(x2,14 - BIT_DEPTH);
                x3= _mm_slli_epi16(x3,14 - BIT_DEPTH);
                _mm_storeu_si128(&dst[x], x2);
                _mm_storeu_si128(&dst[x+8], x3);
                
            }
            src += srcstride;
            dst += dststride;
        }
    
}
#else
static void FUNC(put_hevc_qpel_pixels)(int16_t *dst, ptrdiff_t dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int width, int height)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = src[x] << (14 - BIT_DEPTH);
#ifndef GCC_OPTIMIZATION_ENABLE
            x++;
            dst[x] = src[x] << (14 - BIT_DEPTH);
            x++;
            dst[x] = src[x] << (14 - BIT_DEPTH);
            x++;
            dst[x] = src[x] << (14 - BIT_DEPTH);
#endif
        }
        src += srcstride;
        dst += dststride;
    }
}
#endif

#define QPEL_FILTER_1(src, stride)                                              \
    (-src[x-3*stride] + 4*src[x-2*stride] - 10*src[x-stride] + 58*src[x] +      \
     17*src[x+stride] - 5*src[x+2*stride] + 1*src[x+3*stride])
#define QPEL_FILTER_2(src, stride)                                              \
    (-src[x-3*stride] + 4*src[x-2*stride] - 11*src[x-stride] + 40*src[x] +      \
     40*src[x+stride] - 11*src[x+2*stride] + 4*src[x+3*stride] - src[x+4*stride])
#define QPEL_FILTER_3(src, stride)                                              \
    (src[x-2*stride] - 5*src[x-stride] + 17*src[x] + 58*src[x+stride]           \
     - 10*src[x+2*stride] + 4*src[x+3*stride] - src[x+4*stride])
#define QPEL2_FILTER_3                                              \
_mm_set_epi16(-1 ,4 ,-10 ,58 ,17 ,-5 ,1 ,0)
#define QPEL2_FILTER_2                                             \
_mm_set_epi16(-1 ,4 ,-11 ,40 ,40 ,-11 ,4 ,-1)
#define QPEL2_FILTER_1                                              \
_mm_set_epi16(0 ,1 ,-5 ,17 ,58 ,-10 ,4 ,-1)
#define QPEL2_H_FILTER_3                                              \
_mm_set_epi8(-1 ,4 ,-10 ,58 ,17 ,-5 ,1 ,0 ,-1 ,4 ,-10 ,58 ,17 ,-5 ,1 ,0)
#define QPEL2_H_FILTER_2                                             \
_mm_set_epi8(-1 ,4 ,-11 ,40 ,40 ,-11 ,4 ,-1 ,-1 ,4 ,-11 ,40 ,40 ,-11 ,4 ,-1)
#define QPEL2_H_FILTER_1                                              \
_mm_set_epi8(0 ,1 ,-5 ,17 ,58 ,-10 ,4 ,-1 ,0 ,1 ,-5 ,17 ,58 ,-10 ,4 ,-1)
#define QPEL2_X3V_FILTER_3                                              \
_mm_setzero_si128()
#define QPEL2_X3V_FILTER_2                                              \
_mm_load_si128((__m128i*)&tmp[x-3*srcstride])
#define QPEL2_X3V_FILTER_1                                              \
_mm_load_si128((__m128i*)&tmp[x-3*srcstride])
#define QPEL2_X4V_FILTER_3                                              \
_mm_load_si128((__m128i*)&tmp[x+4*srcstride])
#define QPEL2_X4V_FILTER_2                                             \
_mm_load_si128((__m128i*)&tmp[x+4*srcstride])
#define QPEL2_X4V_FILTER_1                                              \
_mm_setzero_si128()
#define QPEL2_X3VV_FILTER_3                                              \
_mm_setzero_si128()
#define QPEL2_X3VV_FILTER_2                                              \
_mm_loadu_si128((__m128i*)&src[x-3*srcstride])
#define QPEL2_X3VV_FILTER_1                                              \
_mm_loadu_si128((__m128i*)&src[x-3*srcstride])
#define QPEL2_X4VV_FILTER_3                                              \
_mm_loadu_si128((__m128i*)&src[x+4*srcstride])
#define QPEL2_X4VV_FILTER_2                                             \
_mm_loadu_si128((__m128i*)&src[x+4*srcstride])
#define QPEL2_X4VV_FILTER_1                                              \
_mm_setzero_si128()

#ifdef SSE_MC
#define PUT_HEVC_QPEL_H(H)                                                      \
static void FUNC(put_hevc_qpel_h ## H)(int16_t *dst, ptrdiff_t dststride,       \
uint8_t *_src, ptrdiff_t _srcstride,  \
int width, int height)                \
{                                                                           \
int x, y, i;                                                            \
uint8_t *src = _src;                                                    \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                         \
__m128i x1, rBuffer, rTemp, r0, r1, r2, x2, x3, x4,x5, y1,y2,y3;                  \
const __m128i rk0 = _mm_set1_epi8(0);                                   \
\
r0= QPEL2_H_FILTER_## H;                                            \
\
/* LOAD src from memory to registers to limit memory bandwidth */     \
if(width == 4){                                                             \
\
for (y = 0; y < height; y+=2) {                                              \
/* load data in register     */                                   \
x1= _mm_loadu_si128((__m128i*)&src[-3]);                                        \
    src+= srcstride;                                                            \
y1= _mm_loadu_si128((__m128i*)&src[-3]);                                      \
x2= _mm_unpacklo_epi64(x1,_mm_srli_si128(x1,1));                                \
x3= _mm_unpacklo_epi64(_mm_srli_si128(x1,2),_mm_srli_si128(x1,3));              \
y2= _mm_unpacklo_epi64(y1,_mm_srli_si128(y1,1));                                \
y3= _mm_unpacklo_epi64(_mm_srli_si128(y1,2),_mm_srli_si128(y1,3));              \
\
/*  PMADDUBSW then PMADDW     */                                                \
x2= _mm_maddubs_epi16(x2,r0);                                                   \
    y2= _mm_maddubs_epi16(y2,r0);                                                   \
x3= _mm_maddubs_epi16(x3,r0);                                                   \
    y3= _mm_maddubs_epi16(y3,r0);                                                   \
x2= _mm_hadd_epi16(x2,x3);                                                      \
    y2= _mm_hadd_epi16(y2,y3);                                                      \
x2= _mm_hadd_epi16(x2,_mm_set1_epi16(0));                                   \
    y2= _mm_hadd_epi16(y2,_mm_set1_epi16(0));                                   \
x2= _mm_srli_epi16(x2, BIT_DEPTH - 8);                                         \
    y2= _mm_srli_epi16(y2, BIT_DEPTH - 8);                                      \
/* give results back            */                                \
    _mm_store_si128(&dst[0],x2); \
/*dst[0]= _mm_extract_epi16(x2,0);                                            \
dst[1]= _mm_extract_epi16(x2,1);                                        \
dst[2]= _mm_extract_epi16(x2,2);                                        \
dst[3]= _mm_extract_epi16(x2,3);           */                             \
    dst+=dststride; \
    _mm_store_si128(&dst[0],y2); \
/*    dst[0]= _mm_extract_epi16(y2,0);                                            \
    dst[1]= _mm_extract_epi16(y2,1);                                        \
    dst[2]= _mm_extract_epi16(y2,2);                                        \
    dst[3]= _mm_extract_epi16(y2,3);    */                                    \
src += srcstride;                                                       \
dst += dststride;                                                       \
}                                                                           \
}else                                                                       \
for (y = 0; y < height; y++) {                                              \
for (x = 0; x < width; x+=8)  {                                             \
/* load data in register     */                                             \
x1= _mm_loadu_si128((__m128i*)&src[x-3]);                                   \
x2= _mm_unpacklo_epi64(x1,_mm_srli_si128(x1,1));                            \
x3= _mm_unpacklo_epi64(_mm_srli_si128(x1,2),_mm_srli_si128(x1,3));          \
x4= _mm_unpacklo_epi64(_mm_srli_si128(x1,4),_mm_srli_si128(x1,5));          \
x5= _mm_unpacklo_epi64(_mm_srli_si128(x1,6),_mm_srli_si128(x1,7));          \
\
/*  PMADDUBSW then PMADDW     */                                            \
x2= _mm_maddubs_epi16(x2,r0);                                               \
x3= _mm_maddubs_epi16(x3,r0);                                               \
x4= _mm_maddubs_epi16(x4,r0);                                               \
x5= _mm_maddubs_epi16(x5,r0);                                               \
x2= _mm_hadd_epi16(x2,x3);                                                  \
x4= _mm_hadd_epi16(x4,x5);                                                  \
x2= _mm_hadd_epi16(x2,x4);                                                  \
/* give results back            */                            \
_mm_store_si128(&dst[x], _mm_srli_si128(x2,BIT_DEPTH -8));                  \
}                                                                   \
src += srcstride;                                                   \
dst += dststride;                                                   \
}                                                                       \
\
}
/**
 for column MC treatment, we will calculate 8 pixels at the same time by multiplying the values
 of each row.
 
 */
#define PUT_HEVC_QPEL_V(V)                                                  \
static void FUNC(put_hevc_qpel_v ## V)(int16_t *dst, ptrdiff_t dststride,   \
uint8_t *_src, ptrdiff_t _srcstride,                                        \
int width, int height)                                                      \
{                                                                           \
int x, y;                                                                   \
pixel *src = (pixel*)_src;                                                  \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                             \
__m128i x1,x2,x3,x4,x5,x6,x7,x8, r0, r1, r2;                                \
__m128i t1,t2,t3,t4,t5,t6,t7,t8;                                            \
r1= QPEL2_FILTER_## V;                                                      \
/* case width = 4 */                                                        \
if(width == 4){                                                             \
for (y = 0; y < height; y+=2)  {                                             \
r0= _mm_set1_epi16(0);                                                      \
/* load data in register  */                                                \
x1= QPEL2_X3VV_FILTER_## V;                                                  \
x2= _mm_loadu_si128((__m128i*)&src[-2*srcstride]);                          \
x3= _mm_loadu_si128((__m128i*)&src[-srcstride]);                            \
x4= _mm_loadu_si128((__m128i*)&src[0]);                                     \
x5= _mm_loadu_si128((__m128i*)&src[srcstride]);                             \
x6= _mm_loadu_si128((__m128i*)&src[2*srcstride]);                           \
x7= _mm_loadu_si128((__m128i*)&src[3*srcstride]);                           \
x8= QPEL2_X4VV_FILTER_## V;                                                  \
    src += srcstride;                                                       \
    t1= QPEL2_X3VV_FILTER_## V;                                                  \
    t2= _mm_loadu_si128((__m128i*)&src[-2*srcstride]);                          \
    t3= _mm_loadu_si128((__m128i*)&src[-srcstride]);                            \
    t4= _mm_loadu_si128((__m128i*)&src[0]);                                     \
    t5= _mm_loadu_si128((__m128i*)&src[srcstride]);                             \
    t6= _mm_loadu_si128((__m128i*)&src[2*srcstride]);                           \
    t7= _mm_loadu_si128((__m128i*)&src[3*srcstride]);                           \
    t8= QPEL2_X4VV_FILTER_## V;                                                  \
    x1 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));                                \
    x2 = _mm_unpacklo_epi8(x2,_mm_set1_epi8(0));                                \
    x3 = _mm_unpacklo_epi8(x3,_mm_set1_epi8(0));                                \
    x4 = _mm_unpacklo_epi8(x4,_mm_set1_epi8(0));                                \
    x5 = _mm_unpacklo_epi8(x5,_mm_set1_epi8(0));                                \
    x6 = _mm_unpacklo_epi8(x6,_mm_set1_epi8(0));                                \
    x7 = _mm_unpacklo_epi8(x7,_mm_set1_epi8(0));                                \
    x8 = _mm_unpacklo_epi8(x8,_mm_set1_epi8(0));                                \
    t1 = _mm_unpacklo_epi8(t1,_mm_set1_epi8(0));                                \
    t2 = _mm_unpacklo_epi8(t2,_mm_set1_epi8(0));                                \
    t3 = _mm_unpacklo_epi8(t3,_mm_set1_epi8(0));                                \
    t4 = _mm_unpacklo_epi8(t4,_mm_set1_epi8(0));                                \
    t5 = _mm_unpacklo_epi8(t5,_mm_set1_epi8(0));                                \
    t6 = _mm_unpacklo_epi8(t6,_mm_set1_epi8(0));                                \
    t7 = _mm_unpacklo_epi8(t7,_mm_set1_epi8(0));                                \
    t8 = _mm_unpacklo_epi8(t8,_mm_set1_epi8(0));                                \
\
\
r0= _mm_mullo_epi16(x1,_mm_set1_epi16(_mm_extract_epi16(r1,0))) ;           \
    r2= _mm_mullo_epi16(t1,_mm_set1_epi16(_mm_extract_epi16(r1,0))) ;           \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(x2,_mm_set1_epi16(_mm_extract_epi16(r1,1)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(t2,_mm_set1_epi16(_mm_extract_epi16(r1,1)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(x3,_mm_set1_epi16(_mm_extract_epi16(r1,2)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(t3,_mm_set1_epi16(_mm_extract_epi16(r1,2)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(x4,_mm_set1_epi16(_mm_extract_epi16(r1,3)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(t4,_mm_set1_epi16(_mm_extract_epi16(r1,3)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(x5,_mm_set1_epi16(_mm_extract_epi16(r1,4)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(t5,_mm_set1_epi16(_mm_extract_epi16(r1,4)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(x6,_mm_set1_epi16(_mm_extract_epi16(r1,5)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(t6,_mm_set1_epi16(_mm_extract_epi16(r1,5)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(x7,_mm_set1_epi16(_mm_extract_epi16(r1,6)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(t7,_mm_set1_epi16(_mm_extract_epi16(r1,6)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(x8,_mm_set1_epi16(_mm_extract_epi16(r1,7))));     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(t8,_mm_set1_epi16(_mm_extract_epi16(r1,7))));     \
\
r0= _mm_srli_epi16(r0, BIT_DEPTH - 8);                                      \
r2= _mm_srli_epi16(r2, BIT_DEPTH - 8);                                      \
/* give results back            */                                          \
    _mm_store_si128(&dst[0],r0); \
    dst += dststride;                                                       \
        _mm_store_si128(&dst[0],r2); \
src += srcstride;                                                       \
dst += dststride;                                                       \
}                                                                           \
                                                                        \
}else      /*case width >=8 */                                              \
for (y = 0; y < height; y++)  {                                             \
for (x = 0; x < width; x+=16)  {                                         \
/* check if memory needs to be reloaded */                              \
x1= QPEL2_X3VV_FILTER_## V;                                                  \
x2= _mm_loadu_si128((__m128i*)&src[x-2*srcstride]);                        \
x3= _mm_loadu_si128((__m128i*)&src[x-srcstride]);                           \
x4= _mm_loadu_si128((__m128i*)&src[x]);                                     \
x5= _mm_loadu_si128((__m128i*)&src[x+srcstride]);                            \
x6= _mm_loadu_si128((__m128i*)&src[x+2*srcstride]);                        \
x7= _mm_loadu_si128((__m128i*)&src[x+3*srcstride]);                         \
x8= QPEL2_X4VV_FILTER_## V;                                                  \
\
t1 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));                                \
t2 = _mm_unpacklo_epi8(x2,_mm_set1_epi8(0));                                \
t3 = _mm_unpacklo_epi8(x3,_mm_set1_epi8(0));                                \
t4 = _mm_unpacklo_epi8(x4,_mm_set1_epi8(0));                                \
t5 = _mm_unpacklo_epi8(x5,_mm_set1_epi8(0));                                \
t6 = _mm_unpacklo_epi8(x6,_mm_set1_epi8(0));                                \
t7 = _mm_unpacklo_epi8(x7,_mm_set1_epi8(0));                                \
t8 = _mm_unpacklo_epi8(x8,_mm_set1_epi8(0));                                \
                                                                      \
x1 = _mm_unpackhi_epi8(x1,_mm_set1_epi8(0));                                \
x2 = _mm_unpackhi_epi8(x2,_mm_set1_epi8(0));                                \
x3 = _mm_unpackhi_epi8(x3,_mm_set1_epi8(0));                                \
x4 = _mm_unpackhi_epi8(x4,_mm_set1_epi8(0));                                \
x5 = _mm_unpackhi_epi8(x5,_mm_set1_epi8(0));                                \
x6 = _mm_unpackhi_epi8(x6,_mm_set1_epi8(0));                                \
x7 = _mm_unpackhi_epi8(x7,_mm_set1_epi8(0));                                \
x8 = _mm_unpackhi_epi8(x8,_mm_set1_epi8(0));                                \
                                                                       \
/* multiply by correct value : */                                          \
r0= _mm_mullo_epi16(t1,_mm_set1_epi16(_mm_extract_epi16(r1,0))) ;       \
r2= _mm_mullo_epi16(x1,_mm_set1_epi16(_mm_extract_epi16(r1,0))) ;       \
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t2,_mm_set1_epi16(_mm_extract_epi16(r1,1)))) ;     \
r2= _mm_adds_epi16(r2, _mm_mullo_epi16(x2,_mm_set1_epi16(_mm_extract_epi16(r1,1)))) ;     \
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t3,_mm_set1_epi16(_mm_extract_epi16(r1,2)))) ;     \
r2= _mm_adds_epi16(r2, _mm_mullo_epi16(x3,_mm_set1_epi16(_mm_extract_epi16(r1,2)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t4,_mm_set1_epi16(_mm_extract_epi16(r1,3)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(x4,_mm_set1_epi16(_mm_extract_epi16(r1,3)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t5,_mm_set1_epi16(_mm_extract_epi16(r1,4)))) ;     \
r2= _mm_adds_epi16(r2, _mm_mullo_epi16(x5,_mm_set1_epi16(_mm_extract_epi16(r1,4)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t6,_mm_set1_epi16(_mm_extract_epi16(r1,5)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(x6,_mm_set1_epi16(_mm_extract_epi16(r1,5)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t7,_mm_set1_epi16(_mm_extract_epi16(r1,6)))) ;     \
    r2= _mm_adds_epi16(r2, _mm_mullo_epi16(x7,_mm_set1_epi16(_mm_extract_epi16(r1,6)))) ;     \
\
r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t8,_mm_set1_epi16(_mm_extract_epi16(r1,7)))) ;     \
r2= _mm_adds_epi16(r2, _mm_mullo_epi16(x8,_mm_set1_epi16(_mm_extract_epi16(r1,7)))) ;     \
    \
    \
    \
/* give results back            */                                          \
_mm_store_si128(&dst[x], _mm_srli_epi16(r0,BIT_DEPTH -8));      \
_mm_store_si128(&dst[x+8], _mm_srli_epi16(r2,BIT_DEPTH -8));      \
}                                                                       \
src += srcstride;                                                       \
dst += dststride;                                                       \
}                                                                           \
}
#define PUT_HEVC_QPEL_HV(H, V)                                                            \
static void FUNC(put_hevc_qpel_h ## H ## v ## V )(int16_t *dst, ptrdiff_t dststride,      \
uint8_t *_src, ptrdiff_t _srcstride,    \
int width, int height)                  \
{                                                                       \
int x, y, temp1;                                                               \
pixel *src = (pixel*)_src;                                              \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                         \
DECLARE_ALIGNED( 16, int16_t, tmp_array[(MAX_PB_SIZE+7)*MAX_PB_SIZE] );\
/* int16_t tmp_array[(MAX_PB_SIZE+7)*MAX_PB_SIZE];     */                    \
 int16_t *tmp = tmp_array;                                               \
__m128i x1,x2,x3,x4,x5,x6,x7,x8, rBuffer, rTemp, r0, r1, r2;            \
__m128i t1,t2,t3,t4,t5,t6,t7,t8;                                        \
  \
src -= qpel_extra_before[V] * srcstride;                                \
r0= QPEL2_H_FILTER_## H;                                                \
\
/* LOAD src from memory to registers to limit memory bandwidth */         \
if(width == 4){                                                             \
\
for (y = 0; y < height + qpel_extra[V]; y+=2) {                              \
/* load data in register     */                                       \
    x1= _mm_loadu_si128((__m128i*)&src[-3]);                                    \
    src += srcstride;                                                           \
    t1= _mm_loadu_si128((__m128i*)&src[-3]);                                      \
    x2= _mm_unpacklo_epi64(x1,_mm_srli_si128(x1,1));                                \
    t2= _mm_unpacklo_epi64(t1,_mm_srli_si128(t1,1));                                \
    x3= _mm_unpacklo_epi64(_mm_srli_si128(x1,2),_mm_srli_si128(x1,3));              \
    t3= _mm_unpacklo_epi64(_mm_srli_si128(t1,2),_mm_srli_si128(t1,3));              \
\
/*  PMADDUBSW then PMADDW     */                                            \
    x2= _mm_maddubs_epi16(x2,r0);                                                   \
        t2= _mm_maddubs_epi16(t2,r0);                                                   \
    x3= _mm_maddubs_epi16(x3,r0);                                                   \
        t3= _mm_maddubs_epi16(t3,r0);                                                   \
    x2= _mm_hadd_epi16(x2,x3);                                                      \
        t2= _mm_hadd_epi16(t2,t3);                                                      \
    x2= _mm_hadd_epi16(x2,_mm_set1_epi16(0));                                   \
        t2= _mm_hadd_epi16(t2,_mm_set1_epi16(0));                                   \
    x2= _mm_srli_epi16(x2, BIT_DEPTH - 8);                                         \
        t2= _mm_srli_epi16(t2, BIT_DEPTH - 8);                                      \
/* give results back            */                         \
    _mm_store_si128(&tmp[0], x2);\
/*tmp[0]= _mm_extract_epi16(x2,0);                           \
tmp[1]= _mm_extract_epi16(x2,1);                           \
tmp[2]= _mm_extract_epi16(x2,2);                           \
tmp[3]= _mm_extract_epi16(x2,3);          */                 \
    tmp += MAX_PB_SIZE;                                    \
    _mm_store_si128(&tmp[0], t2);\
/*tmp[0]= _mm_extract_epi16(t2,0);                           \
tmp[1]= _mm_extract_epi16(t2,1);                           \
tmp[2]= _mm_extract_epi16(t2,2);                           \
tmp[3]= _mm_extract_epi16(t2,3);       */                    \
src += srcstride;                                                           \
tmp += MAX_PB_SIZE;                                                         \
}                                                                           \
}else                                                                       \
for (y = 0; y < height + qpel_extra[V]; y++) {                              \
for (x = 0; x < width; x+=8)  {                                         \
/* load data in register     */                                   \
x1= _mm_loadu_si128((__m128i*)&src[x-3]);                                   \
x2= _mm_unpacklo_epi64(x1,_mm_srli_si128(x1,1));                            \
x3= _mm_unpacklo_epi64(_mm_srli_si128(x1,2),_mm_srli_si128(x1,3));          \
x4= _mm_unpacklo_epi64(_mm_srli_si128(x1,4),_mm_srli_si128(x1,5));          \
x5= _mm_unpacklo_epi64(_mm_srli_si128(x1,6),_mm_srli_si128(x1,7));          \
\
/*  PMADDUBSW then PMADDW     */                                            \
x2= _mm_maddubs_epi16(x2,r0);                                               \
x3= _mm_maddubs_epi16(x3,r0);                                               \
x4= _mm_maddubs_epi16(x4,r0);                                               \
x5= _mm_maddubs_epi16(x5,r0);                                               \
x2= _mm_hadd_epi16(x2,x3);                                                  \
x4= _mm_hadd_epi16(x4,x5);                                                  \
x2= _mm_hadd_epi16(x2,x4);                                                  \
    x2= _mm_srli_si128(x2,BIT_DEPTH -8);                                    \
\
/* give results back            */                                \
_mm_store_si128(&tmp[x], x2);                  \
\
}                                                                       \
src += srcstride;                                                       \
tmp += MAX_PB_SIZE;                                                     \
}                                                                           \
\
\
tmp = tmp_array + qpel_extra_before[V] * MAX_PB_SIZE;                   \
    srcstride= MAX_PB_SIZE;                                             \
                                                                        \
/* vertical treatment on temp table : tmp contains 16 bit values, so need to use 32 bit  integers
for register calculations */                                            \
rTemp= QPEL2_FILTER_## V;                                               \
for (y = 0; y < height; y++)  {                                         \
for (x = 0; x < width; x+=8)  {                                         \
                                                                        \
x1= QPEL2_X3V_FILTER_## V;                                              \
x2= _mm_load_si128((__m128i*)&tmp[x-2*srcstride]);                     \
x3= _mm_load_si128((__m128i*)&tmp[x-srcstride]);                       \
x4= _mm_load_si128((__m128i*)&tmp[x]);                                 \
x5= _mm_load_si128((__m128i*)&tmp[x+srcstride]);                       \
x6= _mm_load_si128((__m128i*)&tmp[x+2*srcstride]);                     \
x7= _mm_load_si128((__m128i*)&tmp[x+3*srcstride]);                     \
x8= QPEL2_X4V_FILTER_## V;                                              \
                                                                        \
    r0= _mm_set1_epi16(_mm_extract_epi16(rTemp,0));                     \
    r1= _mm_set1_epi16(_mm_extract_epi16(rTemp,1));                     \
    t8= _mm_mullo_epi16(x1,r0);                                         \
    rBuffer= _mm_mulhi_epi16(x1,r0);                                    \
        t7= _mm_mullo_epi16(x2,r1);                                     \
    t1 = _mm_unpacklo_epi16(t8,rBuffer);                                \
    x1 = _mm_unpackhi_epi16(t8,rBuffer);                                \
                                                                        \
                                                                        \
    r0=_mm_set1_epi16(_mm_extract_epi16(rTemp,2));                      \
    rBuffer= _mm_mulhi_epi16(x2,r1);                                    \
    t8= _mm_mullo_epi16(x3,r0);                                         \
    t2 = _mm_unpacklo_epi16(t7,rBuffer);                                \
    x2 = _mm_unpackhi_epi16(t7,rBuffer);                                \
                                                                        \
    r1=_mm_set1_epi16(_mm_extract_epi16(rTemp,3));                      \
    rBuffer= _mm_mulhi_epi16(x3,r0);                                    \
    t7= _mm_mullo_epi16(x4,r1);                                         \
t3 = _mm_unpacklo_epi16(t8,rBuffer);                                    \
x3 = _mm_unpackhi_epi16(t8,rBuffer);                                    \
                                                                        \
    r0=_mm_set1_epi16(_mm_extract_epi16(rTemp,4));                      \
rBuffer= _mm_mulhi_epi16(x4,r1);                                        \
    t8= _mm_mullo_epi16(x5,r0);                                         \
t4 = _mm_unpacklo_epi16(t7,rBuffer);                                    \
x4 = _mm_unpackhi_epi16(t7,rBuffer);                                    \
                                                                        \
    r1=_mm_set1_epi16(_mm_extract_epi16(rTemp,5));                      \
rBuffer= _mm_mulhi_epi16(x5,r0);                                        \
    t7= _mm_mullo_epi16(x6,r1);                                         \
t5 = _mm_unpacklo_epi16(t8,rBuffer);                                    \
x5 = _mm_unpackhi_epi16(t8,rBuffer);                                    \
                                                                        \
    r0=_mm_set1_epi16(_mm_extract_epi16(rTemp,6));                      \
    rBuffer= _mm_mulhi_epi16(x6,r1);                                    \
    t8= _mm_mullo_epi16(x7,r0);                                         \
    t6 = _mm_unpacklo_epi16(t7,rBuffer);                                \
    x6 = _mm_unpackhi_epi16(t7,rBuffer);                                \
                                                                        \
    rBuffer= _mm_mulhi_epi16(x7,r0);                                    \
    t7 = _mm_unpacklo_epi16(t8,rBuffer);                                \
    x7 = _mm_unpackhi_epi16(t8,rBuffer);                                \
                                                                        \
    t8 = _mm_unpacklo_epi16(_mm_mullo_epi16(x8,_mm_set1_epi16(_mm_extract_epi16(rTemp,7))),_mm_mulhi_epi16(x8,_mm_set1_epi16(_mm_extract_epi16(rTemp,7))));    \
    x8 = _mm_unpackhi_epi16(_mm_mullo_epi16(x8,_mm_set1_epi16(_mm_extract_epi16(rTemp,7))),_mm_mulhi_epi16(x8,_mm_set1_epi16(_mm_extract_epi16(rTemp,7))));    \
                                                                        \
                                                                        \
/* add calculus by correct value : */                                   \
                                                                        \
    r1= _mm_add_epi32(x1,x2);                                           \
    x3= _mm_add_epi32(x3,x4);                                           \
    x5= _mm_add_epi32(x5,x6);                                           \
    r1= _mm_add_epi32(r1,x3);                                           \
    x7= _mm_add_epi32(x7,x8);                                           \
    r1= _mm_add_epi32(r1,x5);                                           \
                                                                        \
r0= _mm_add_epi32(t1,t2);                                               \
t3= _mm_add_epi32(t3,t4);                                               \
t5= _mm_add_epi32(t5,t6);                                               \
r0= _mm_add_epi32(r0,t3);                                               \
t7= _mm_add_epi32(t7,t8);                                               \
r0= _mm_add_epi32(r0,t5);                                               \
r1= _mm_add_epi32(r1,x7);                                               \
r0= _mm_add_epi32(r0,t7);                                               \
r1= _mm_srli_epi32(r1,6);                                               \
r0= _mm_srli_epi32(r0,6);                                               \
                                                                        \
    r1= _mm_and_si128(r1,_mm_set_epi16(0,65535,0,65535,0,65535,0,65535));   \
                                                                        \
    r0= _mm_and_si128(r0,_mm_set_epi16(0,65535,0,65535,0,65535,0,65535));   \
    r0= _mm_hadd_epi16(r0,r1);                                          \
    _mm_store_si128(&dst[x],r0);                                       \
                                                                        \
}                                                                       \
tmp += MAX_PB_SIZE;                                                     \
dst += dststride;                                                       \
}                                                                       \
}

#elif  defined GCC_OPTIMIZATION_ENABLE
#define PUT_HEVC_QPEL_H(H)                                                      \
static void FUNC(put_hevc_qpel_h ## H)(int16_t *dst, ptrdiff_t dststride,       \
uint8_t *_src, ptrdiff_t _srcstride,  \
int width, int height)                \
{                                                                               \
int x, y;                                                                   \
pixel *src = (pixel*)_src;                                                  \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                             \
\
for (y = 0; y < height; y++) {                                              \
for (x = 0; x < width; x++)                                             \
dst[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);             \
src += srcstride;                                                       \
dst += dststride;                                                       \
}                                                                           \
}
#define PUT_HEVC_QPEL_V(V)                                                      \
static void FUNC(put_hevc_qpel_v ## V)(int16_t *dst, ptrdiff_t dststride,       \
uint8_t *_src, ptrdiff_t _srcstride,  \
int width, int height)                \
{                                                                               \
int x, y;                                                                   \
pixel *src = (pixel*)_src;                                                  \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                             \
\
for (y = 0; y < height; y++)  {                                             \
for (x = 0; x < width; x++)                                             \
dst[x] = QPEL_FILTER_ ## V (src, srcstride) >> (BIT_DEPTH - 8);     \
src += srcstride;                                                       \
dst += dststride;                                                       \
}                                                                           \
}
#define PUT_HEVC_QPEL_HV(H, V)                                                            \
static void FUNC(put_hevc_qpel_h ## H ## v ## V )(int16_t *dst, ptrdiff_t dststride,      \
uint8_t *_src, ptrdiff_t _srcstride,    \
int width, int height)                  \
{                                                                                         \
int x, y;                                                                             \
pixel *src = (pixel*)_src;                                                            \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                                       \
\
int16_t tmp_array[(MAX_PB_SIZE+7)*MAX_PB_SIZE];                                       \
int16_t *tmp = tmp_array;                                                             \
\
src -= qpel_extra_before[V] * srcstride;                                              \
\
for (y = 0; y < height + qpel_extra[V]; y++) {                                        \
for (x = 0; x < width; x++)                                                       \
tmp[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);                       \
src += srcstride;                                                                 \
tmp += MAX_PB_SIZE;                                                               \
}                                                                                     \
\
tmp = tmp_array + qpel_extra_before[V] * MAX_PB_SIZE;                                 \
\
for (y = 0; y < height; y++) {                                                        \
for (x = 0; x < width; x++)                                                       \
dst[x] = QPEL_FILTER_ ## V (tmp, MAX_PB_SIZE) >> 6;                           \
tmp += MAX_PB_SIZE;                                                               \
dst += dststride;                                                                 \
}                                                                                     \
}

#else
#define PUT_HEVC_QPEL_H(H)                                                      \
static void FUNC(put_hevc_qpel_h ## H)(int16_t *dst, ptrdiff_t dststride,       \
uint8_t *_src, ptrdiff_t _srcstride,  \
int width, int height)                \
{                                                                               \
int x, y;                                                                   \
pixel *src = (pixel*)_src;                                                  \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                             \
\
for (y = 0; y < height; y++) {                                              \
for (x = 0; x < width; x++) {                                           \
dst[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);             \
x++;                                                                \
dst[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);             \
x++;                                                                \
dst[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);             \
x++;                                                                \
dst[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);             \
}                                                                       \
src += srcstride;                                                       \
dst += dststride;                                                       \
}                                                                           \
}
#define PUT_HEVC_QPEL_V(V)                                                      \
static void FUNC(put_hevc_qpel_v ## V)(int16_t *dst, ptrdiff_t dststride,       \
uint8_t *_src, ptrdiff_t _srcstride,  \
int width, int height)                \
{                                                                               \
int x, y;                                                                   \
pixel *src = (pixel*)_src;                                                  \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                             \
\
for (y = 0; y < height; y++)  {                                             \
for (x = 0; x < width; x++) {                                           \
dst[x] = QPEL_FILTER_ ## V (src, srcstride) >> (BIT_DEPTH - 8);     \
x++;                                                                \
dst[x] = QPEL_FILTER_ ## V (src, srcstride) >> (BIT_DEPTH - 8);     \
x++;                                                                \
dst[x] = QPEL_FILTER_ ## V (src, srcstride) >> (BIT_DEPTH - 8);     \
x++;                                                                \
dst[x] = QPEL_FILTER_ ## V (src, srcstride) >> (BIT_DEPTH - 8);     \
}                                                                       \
src += srcstride;                                                       \
dst += dststride;                                                       \
}                                                                           \
}
#define PUT_HEVC_QPEL_HV(H, V)                                                            \
static void FUNC(put_hevc_qpel_h ## H ## v ## V )(int16_t *dst, ptrdiff_t dststride,      \
uint8_t *_src, ptrdiff_t _srcstride,    \
int width, int height)                  \
{                                                                                         \
int x, y;                                                                             \
pixel *src = (pixel*)_src;                                                            \
ptrdiff_t srcstride = _srcstride/sizeof(pixel);                                       \
\
int16_t tmp_array[(MAX_PB_SIZE+7)*MAX_PB_SIZE];                                       \
int16_t *tmp = tmp_array;                                                             \
\
src -= qpel_extra_before[V] * srcstride;                                              \
\
for (y = 0; y < height + qpel_extra[V]; y++) {                                        \
for (x = 0; x < width; x++) {                                                     \
tmp[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);                       \
x++;                                                                          \
tmp[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);                       \
x++;                                                                          \
tmp[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);                       \
x++;                                                                          \
tmp[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);                       \
}                                                                                 \
src += srcstride;                                                                 \
tmp += MAX_PB_SIZE;                                                               \
}                                                                                     \
\
tmp = tmp_array + qpel_extra_before[V] * MAX_PB_SIZE;                                 \
\
for (y = 0; y < height; y++) {                                                        \
for (x = 0; x < width; x++) {                                                     \
dst[x] = QPEL_FILTER_ ## V (tmp, MAX_PB_SIZE) >> 6;                           \
x++;                                                                          \
dst[x] = QPEL_FILTER_ ## V (tmp, MAX_PB_SIZE) >> 6;                           \
x++;                                                                          \
dst[x] = QPEL_FILTER_ ## V (tmp, MAX_PB_SIZE) >> 6;                           \
x++;                                                                          \
dst[x] = QPEL_FILTER_ ## V (tmp, MAX_PB_SIZE) >> 6;                           \
}                                                                                 \
tmp += MAX_PB_SIZE;                                                               \
dst += dststride;                                                                 \
}                                                                                     \
}
#endif

PUT_HEVC_QPEL_H(1)
PUT_HEVC_QPEL_H(2)
PUT_HEVC_QPEL_H(3)
PUT_HEVC_QPEL_V(1)
PUT_HEVC_QPEL_V(2)
PUT_HEVC_QPEL_V(3)
PUT_HEVC_QPEL_HV(1, 1)
PUT_HEVC_QPEL_HV(1, 2)
PUT_HEVC_QPEL_HV(1, 3)
PUT_HEVC_QPEL_HV(2, 1)
PUT_HEVC_QPEL_HV(2, 2)
PUT_HEVC_QPEL_HV(2, 3)
PUT_HEVC_QPEL_HV(3, 1)
PUT_HEVC_QPEL_HV(3, 2)
PUT_HEVC_QPEL_HV(3, 3)

#ifndef SSE_EPEL
static void FUNC(put_hevc_epel_pixels)(int16_t *dst, ptrdiff_t dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = src[x] << (14 - BIT_DEPTH);
#ifndef GCC_OPTIMIZATION_ENABLE
            x++;
            dst[x] = src[x] << (14 - BIT_DEPTH);
#endif
        }
        src += srcstride;
        dst += dststride;
    }
}
#else
static void FUNC(put_hevc_epel_pixels)(int16_t *dst, ptrdiff_t dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int width, int height, int mx, int my)
{
    int x, y;
    __m128i x1, x2;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    if(width == 4){
        for (y = 0; y < height; y++) {
            x1= _mm_loadu_si128((__m128i*)&src[0]);
            x2 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));
            x2= _mm_slli_epi16(x2,14 - BIT_DEPTH);
           /* dst[0]= _mm_extract_epi16(x2,0);
            dst[1]= _mm_extract_epi16(x2,1);
            dst[2]= _mm_extract_epi16(x2,2);
            dst[3]= _mm_extract_epi16(x2,3);*/
            _mm_store_si128(&dst[0],x2);
            src += srcstride;
            dst += dststride;
        }
    }
    else
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x+=16) {

                    x1= _mm_loadu_si128((__m128i*)&src[x]);
                    x2 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));

                    x1 = _mm_unpackhi_epi8(x1,_mm_set1_epi8(0));

                x2= _mm_slli_epi16(x2,14 - BIT_DEPTH);
                x1= _mm_slli_epi16(x1,14 - BIT_DEPTH);
                _mm_store_si128(&dst[x], x2);
                _mm_store_si128(&dst[x+8], x1);
                
            }
            src += srcstride;
            dst += dststride;
        }
    
}
#endif

#ifdef OPTIMIZATION_ENABLE
#define EPEL_FILTER(src, stride) \
    (filter_0*src[x-stride] + filter_1*src[x] + filter_2*src[x+stride] + filter_3*src[x+2*stride])
#else
#define EPEL_FILTER(src, stride, F) \
    (F[0]*src[x-stride] + F[1]*src[x] + F[2]*src[x+stride] + F[3]*src[x+2*stride])
#endif
#ifdef SSE_EPEL
static void FUNC(put_hevc_epel_h)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    const int8_t *filter = epel_filters[mx-1];
    __m128i r0, bshuffle1, bshuffle2, x1, x2,x3;
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
    r0= _mm_set_epi8(filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0);
    bshuffle1=_mm_set_epi8(6,5,4,3,5,4,3,2,4,3,2,1,3,2,1,0);
    
    if(width == 4){
        
        for (y = 0; y < height; y++) {
            /* load data in register     */
            x1= _mm_loadu_si128((__m128i*)&src[-1]);
            x2= _mm_shuffle_epi8(x1,bshuffle1);
            
            /*  PMADDUBSW then PMADDW     */
            x2= _mm_maddubs_epi16(x2,r0);
            x2= _mm_hadd_epi16(x2,_mm_set1_epi16(0));
            x2= _mm_srli_epi16(x2, BIT_DEPTH - 8);
            /* give results back            */
            _mm_store_si128(&dst[0],x2);
       /*     dst[0]= _mm_extract_epi16(x2,0);
            dst[1]= _mm_extract_epi16(x2,1);
            dst[2]= _mm_extract_epi16(x2,2);
            dst[3]= _mm_extract_epi16(x2,3);*/
            src += srcstride;
            dst += dststride;
        }
    }else{
        bshuffle2=_mm_set_epi8(10,9,8,7,9,8,7,6,8,7,6,5,7,6,5,4);
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x+=8) {
                
                x1= _mm_loadu_si128((__m128i*)&src[x-1]);
                x2= _mm_shuffle_epi8(x1,bshuffle1);
                x3= _mm_shuffle_epi8(x1,bshuffle2);
                
                /*  PMADDUBSW then PMADDW     */
                x2= _mm_maddubs_epi16(x2,r0);
                x3= _mm_maddubs_epi16(x3,r0);
                x2= _mm_hadd_epi16(x2,x3);
                x2= _mm_srli_epi16(x2,BIT_DEPTH - 8);
                _mm_store_si128(&dst[x],x2);
            }
            src += srcstride;
            dst += dststride;
        }
    }
}
#else
static void FUNC(put_hevc_epel_h)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    const int8_t *filter = epel_filters[mx-1];
#ifdef OPTIMIZATION_ENABLE
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
#endif
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
#ifdef GCC_OPTIMIZATION_ENABLE
            dst[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
#else
            dst[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
            x++;
            dst[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
#endif
        }
        src += srcstride;
        dst += dststride;
    }
}
#endif
#ifdef SSE_EPEL
static void FUNC(put_hevc_epel_v)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my)
{
    int x, y;
    __m128i x0,x1,x2,x3,t0,t1,t2,t3,r0,f0,f1,f2,f3,r1;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    const int8_t *filter = epel_filters[my-1];
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
    f0= _mm_set1_epi16(filter_0);
    f1= _mm_set1_epi16(filter_1);
    f2= _mm_set1_epi16(filter_2);
    f3= _mm_set1_epi16(filter_3);
    if(width == 4)
        for (y = 0; y < height; y++)  {
            
            /* check if memory needs to be reloaded */

                x0= _mm_loadu_si128((__m128i*)&src[-srcstride]);
                x1= _mm_loadu_si128((__m128i*)&src[0]);
                x2= _mm_loadu_si128((__m128i*)&src[srcstride]);
                x3= _mm_loadu_si128((__m128i*)&src[2*srcstride]);
                
                t0 = _mm_unpacklo_epi8(x0,_mm_set1_epi8(0));
                t1 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));
                t2 = _mm_unpacklo_epi8(x2,_mm_set1_epi8(0));
                t3 = _mm_unpacklo_epi8(x3,_mm_set1_epi8(0));

            r0= _mm_set1_epi16(0);
            /* multiply by correct value : */
            r0= _mm_mullo_epi16(t0,f0) ;
            r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t1,f1)) ;
            
            r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t2,f2)) ;
            
            r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t3,f3)) ;
            r0= _mm_srli_epi16(r0,BIT_DEPTH - 8);
            /* give results back            */
            _mm_store_si128(&dst[0], r0);\
           /* dst[0]= _mm_extract_epi16(r0,0);
            dst[1]= _mm_extract_epi16(r0,1);
            dst[2]= _mm_extract_epi16(r0,2);
            dst[3]= _mm_extract_epi16(r0,3);*/
            
            src += srcstride;
            dst += dststride;
        }
    
    else
        for (y = 0; y < height; y++)  {
            for (x = 0; x < width; x+=16)  {
                /* check if memory needs to be reloaded */

                    x0= _mm_loadu_si128((__m128i*)&src[x-srcstride]);
                    x1= _mm_loadu_si128((__m128i*)&src[x]);
                    x2= _mm_loadu_si128((__m128i*)&src[x+srcstride]);
                    x3= _mm_loadu_si128((__m128i*)&src[x+2*srcstride]);
                    
                    t0 = _mm_unpacklo_epi8(x0,_mm_set1_epi8(0));
                    t1 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));
                    t2 = _mm_unpacklo_epi8(x2,_mm_set1_epi8(0));
                    t3 = _mm_unpacklo_epi8(x3,_mm_set1_epi8(0));

                    x0 = _mm_unpackhi_epi8(x0,_mm_set1_epi8(0));
                    x1 = _mm_unpackhi_epi8(x1,_mm_set1_epi8(0));
                    x2 = _mm_unpackhi_epi8(x2,_mm_set1_epi8(0));
                    x3 = _mm_unpackhi_epi8(x3,_mm_set1_epi8(0));


                /* multiply by correct value : */
                r0= _mm_mullo_epi16(t0,f0) ;
                r1= _mm_mullo_epi16(x0,f0) ;
                r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t1,f1)) ;
                r1= _mm_adds_epi16(r1, _mm_mullo_epi16(x1,f1)) ;
                r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t2,f2)) ;
                r1= _mm_adds_epi16(r1, _mm_mullo_epi16(x2,f2)) ;
                r0= _mm_adds_epi16(r0, _mm_mullo_epi16(t3,f3)) ;
                r1= _mm_adds_epi16(r1, _mm_mullo_epi16(x3,f3)) ;
                r0= _mm_srli_epi16(r0,BIT_DEPTH - 8);
                r1= _mm_srli_epi16(r1,BIT_DEPTH - 8);
                /* give results back            */
                _mm_store_si128(&dst[x], r0);
                _mm_storeu_si128(&dst[x+8], r1);
            }
            src += srcstride;
            dst += dststride;
        }
    
}
#else
static void FUNC(put_hevc_epel_v)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    const int8_t *filter = epel_filters[my-1];
#ifdef OPTIMIZATION_ENABLE
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
#endif
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
#ifdef GCC_OPTIMIZATION_ENABLE
            dst[x] = EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8);
#else
            dst[x] = EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8);
            x++;
            dst[x] = EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8);
#endif
        }
        src += srcstride;
        dst += dststride;
    }
}
#endif
/*#ifdef OPTIMIZATION_ENABLE
 int16_t tmp_array[(MAX_PB_SIZE+3)*MAX_PB_SIZE];
 #endif*/
#ifndef SSE_EPEL
static void FUNC(put_hevc_epel_hv)(int16_t *dst, ptrdiff_t dststride,
                                   uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    const int8_t *filter_h = epel_filters[mx-1];
    const int8_t *filter_v = epel_filters[my-1];
#ifdef OPTIMIZATION_ENABLE
    int8_t filter_0 = filter_h[0];
    int8_t filter_1 = filter_h[1];
    int8_t filter_2 = filter_h[2];
    int8_t filter_3 = filter_h[3];
#endif
    //#ifndef OPTIMIZATION_ENABLE
    int16_t tmp_array[(MAX_PB_SIZE+3)*MAX_PB_SIZE];
    //#endif
    int16_t *tmp = tmp_array;
    
    src -= epel_extra_before * srcstride;
    
    for (y = 0; y < height + epel_extra; y++) {
        for (x = 0; x < width; x++) {
#ifdef GCC_OPTIMIZATION_ENABLE
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
#else
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
            x++;
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
#endif
        }
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }
    
    tmp = tmp_array + epel_extra_before * MAX_PB_SIZE;
#ifdef OPTIMIZATION_ENABLE
    filter_0 = filter_v[0];
    filter_1 = filter_v[1];
    filter_2 = filter_v[2];
    filter_3 = filter_v[3];
#endif
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
#ifdef GCC_OPTIMIZATION_ENABLE
            dst[x] = EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6;
#else
            dst[x] = EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6;
            x++;
            dst[x] = EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6;
#endif
        }
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}
#else
static void FUNC(put_hevc_epel_hv)(int16_t *dst, ptrdiff_t dststride,
                                   uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    const int8_t *filter_h = epel_filters[mx-1];
    const int8_t *filter_v = epel_filters[my-1];
    __m128i r0, bshuffle1, bshuffle2, x0, x1, x2,x3, t0, t1, t2, t3, f0, f1, f2, f3,r1,r2;
    int8_t filter_0 = filter_h[0];
    int8_t filter_1 = filter_h[1];
    int8_t filter_2 = filter_h[2];
    int8_t filter_3 = filter_h[3];
    r0= _mm_set_epi8(filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0);
    bshuffle1=_mm_set_epi8(6,5,4,3,5,4,3,2,4,3,2,1,3,2,1,0);
    DECLARE_ALIGNED( 16, int16_t, tmp_array[(MAX_PB_SIZE+3)*MAX_PB_SIZE] );
    //#ifndef OPTIMIZATION_ENABLE
    // int16_t tmp_array[(MAX_PB_SIZE+3)*MAX_PB_SIZE];
    //#endif
     int16_t *tmp = tmp_array;
    
    src -= epel_extra_before * srcstride;
    /* horizontal treatment */
    if(width == 4){
        
        for (y = 0; y < height + epel_extra; y+=2) {
            /* load data in register     */
            x1= _mm_loadu_si128((__m128i*)&src[-1]);
            src += srcstride;
            x2= _mm_loadu_si128((__m128i*)&src[-1]);
            x1= _mm_shuffle_epi8(x1,bshuffle1);
            x2= _mm_shuffle_epi8(x2,bshuffle1);
            
            /*  PMADDUBSW then PMADDW     */
            x1= _mm_maddubs_epi16(x1,r0);
            x1= _mm_hadd_epi16(x1,_mm_set1_epi16(0));
            x1= _mm_srli_epi16(x1, BIT_DEPTH - 8);
            x2= _mm_maddubs_epi16(x2,r0);
            x2= _mm_hadd_epi16(x2,_mm_set1_epi16(0));
            x2= _mm_srli_epi16(x2, BIT_DEPTH - 8);
            /* give results back            */
            _mm_store_si128(&tmp[0], x1);
/*            tmp[0]= _mm_extract_epi16(x1,0);
            tmp[1]= _mm_extract_epi16(x1,1);
            tmp[2]= _mm_extract_epi16(x1,2);
            tmp[3]= _mm_extract_epi16(x1,3);*/
            tmp += MAX_PB_SIZE;
            _mm_store_si128(&tmp[0], x2);
            /*tmp[0]= _mm_extract_epi16(x2,0);
            tmp[1]= _mm_extract_epi16(x2,1);
            tmp[2]= _mm_extract_epi16(x2,2);
            tmp[3]= _mm_extract_epi16(x2,3);*/
            src += srcstride;
            tmp += MAX_PB_SIZE;
        }
    }else{
        bshuffle2=_mm_set_epi8(10,9,8,7,9,8,7,6,8,7,6,5,7,6,5,4);
        for (y = 0; y < height + epel_extra; y++) {
            for (x = 0; x < width; x+=8) {
                
                x1= _mm_loadu_si128((__m128i*)&src[x-1]);
                x2= _mm_shuffle_epi8(x1,bshuffle1);
                x3= _mm_shuffle_epi8(x1,bshuffle2);
                
                /*  PMADDUBSW then PMADDW     */
                x2= _mm_maddubs_epi16(x2,r0);
                x3= _mm_maddubs_epi16(x3,r0);
                x2= _mm_hadd_epi16(x2,x3);
                x2= _mm_srli_epi16(x2,BIT_DEPTH - 8);
                _mm_store_si128(&tmp[x],x2);
            }
            src += srcstride;
            tmp += MAX_PB_SIZE;
        }
    }
    
    tmp = tmp_array + epel_extra_before * MAX_PB_SIZE;
    
    /* vertical treatment */
    //f0= _mm_loadu_si128((__m128i *)&filter_v);
    f3= _mm_set1_epi16(filter_v[3]);
    f1= _mm_set1_epi16(filter_v[1]);
    f2= _mm_set1_epi16(filter_v[2]);
    f0= _mm_set1_epi16(filter_v[0]);
    for (y = 0; y < height; y++)  {
        for (x = 0; x < width; x+=8)  {
            /* check if memory needs to be reloaded */
                x0= _mm_load_si128((__m128i*)&tmp[x-MAX_PB_SIZE]);
                x1= _mm_load_si128((__m128i*)&tmp[x]);
                x2= _mm_load_si128((__m128i*)&tmp[x+MAX_PB_SIZE]);
                x3= _mm_load_si128((__m128i*)&tmp[x+2*MAX_PB_SIZE]);

                r0= _mm_mullo_epi16(x0,f0);
                r1= _mm_mulhi_epi16(x0,f0);
                r2= _mm_mullo_epi16(x1,f1);
                t0 = _mm_unpacklo_epi16(r0,r1);
                x0 = _mm_unpackhi_epi16(r0,r1);
                r0= _mm_mulhi_epi16(x1,f1);
                r1= _mm_mullo_epi16(x2,f2);
                t1 = _mm_unpacklo_epi16(r2,r0);
                x1 = _mm_unpackhi_epi16(r2,r0);
                r2= _mm_mulhi_epi16(x2,f2);
                r0= _mm_mullo_epi16(x3,f3);
                t2 = _mm_unpacklo_epi16(r1,r2);
                x2 = _mm_unpackhi_epi16(r1,r2);
                r1= _mm_mulhi_epi16(x3,f3);
                t3 = _mm_unpacklo_epi16(r0,r1);
                x3 = _mm_unpackhi_epi16(r0,r1);

            /* multiply by correct value : */
            r0= _mm_add_epi32(t0,t1);
            r1= _mm_add_epi32(x0,x1);
            r0= _mm_add_epi32(r0,t2) ;
            r1= _mm_add_epi32(r1,x2) ;
            r0= _mm_add_epi32(r0,t3) ;
            r1= _mm_add_epi32(r1,x3) ;
            r0= _mm_srli_epi32(r0,6);
            r1= _mm_srli_epi32(r1,6);

            /* give results back            */
            r0=_mm_packs_epi32(r0,r1);
            _mm_store_si128(&dst[x],r0);
        }
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
    
}

#endif
#ifdef SSE_unweight_pred
static void FUNC(put_unweighted_pred)(uint8_t *_dst, ptrdiff_t _dststride,
                                      int16_t *src, ptrdiff_t srcstride,
                                      int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);
    __m128i r0,r1,f0;
    int shift = 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int16_t offset = 1 << (shift - 1);
#else
    int16_t offset = 0;

#endif
    f0= _mm_set1_epi16(offset);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=16) {
            r0= _mm_load_si128(&src[x]);

            r1= _mm_load_si128(&src[x+8]);
            r0= _mm_adds_epi16(r0,f0);

            r1= _mm_adds_epi16(r1,f0);
            r0= _mm_srai_epi16(r0,shift);
            r1= _mm_srai_epi16(r1,shift);
            r0= _mm_packus_epi16(r0,r1);

            _mm_storeu_si128(&dst[x],r0);
        }
        dst += dststride;
        src += srcstride;
    }
}


#else
static void FUNC(put_unweighted_pred)(uint8_t *_dst, ptrdiff_t _dststride,
                                      int16_t *src, ptrdiff_t srcstride,
                                      int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);

    int shift = 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel((src[x] + offset) >> shift);
#ifndef GCC_OPTIMIZATION_ENABLE
            x++;
            dst[x] = av_clip_pixel((src[x] + offset) >> shift);
            x++;
            dst[x] = av_clip_pixel((src[x] + offset) >> shift);
            x++;
            dst[x] = av_clip_pixel((src[x] + offset) >> shift);
#endif
        }
        dst += dststride;
        src += srcstride;
    }
}

#endif


#ifdef SSE_put_weight_pred
static void FUNC(put_weighted_pred_avg)(uint8_t *_dst, ptrdiff_t _dststride,
                                        int16_t *src1, int16_t *src2, ptrdiff_t srcstride,
                                        int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);
    __m128i r0,r1,f0,r2,r3;
    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif
            f0= _mm_set1_epi16(offset);
    for (y = 0; y < height; y++) {

            for (x = 0; x < width; x+=16) {
            r0= _mm_load_si128(&src1[x]);
            r1= _mm_load_si128(&src1[x+8]);
            r2=_mm_load_si128(&src2[x]);
            r3= _mm_load_si128(&src2[x+8]);

            r0= _mm_adds_epi16(r0,f0);
            r1= _mm_adds_epi16(r1,f0);
            r0= _mm_adds_epi16(r0,r2);
            r1= _mm_adds_epi16(r1,r3);
            r0= _mm_srai_epi16(r0,shift);
            r1= _mm_srai_epi16(r1,shift);
            r0= _mm_packus_epi16(r0,r1);

            _mm_storeu_si128(&dst[x],r0);
        }
        dst  += dststride;
        src1 += srcstride;
        src2 += srcstride;
    }
}

#else
static void FUNC(put_weighted_pred_avg)(uint8_t *_dst, ptrdiff_t _dststride,
                                        int16_t *src1, int16_t *src2, ptrdiff_t srcstride,
                                        int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);

    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel((src1[x] + src2[x] + offset) >> shift);
#ifndef GCC_OPTIMIZATION_ENABLE
            x++;
            dst[x] = av_clip_pixel((src1[x] + src2[x] + offset) >> shift);
            x++;
            dst[x] = av_clip_pixel((src1[x] + src2[x] + offset) >> shift);
            x++;
            dst[x] = av_clip_pixel((src1[x] + src2[x] + offset) >> shift);
#endif
        }
        dst  += dststride;
        src1 += srcstride;
        src2 += srcstride;
    }
}

#endif

#ifdef SSE_weight_pred
static void FUNC(weighted_pred)(uint8_t denom, int16_t wlxFlag, int16_t olxFlag,
                                     uint8_t *_dst, ptrdiff_t _dststride,
                                     int16_t *src, ptrdiff_t srcstride,
                                     int width, int height)
{
//    int shift;
    int log2Wd;
//    int16_t wx;
//    int ox;
    int x , y;
 //   int offset;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);
    __m128i x0,x1,x2,x3,c0,add,add2;
    //shift = 14 - BIT_DEPTH;
    log2Wd = denom + 14 - BIT_DEPTH;
    //offset = 1 << (log2Wd - 1);
   // wx = wlxFlag;
    //ox = olxFlag * ( 1 << ( BIT_DEPTH - 8 ) );
    add= _mm_set1_epi32(olxFlag * ( 1 << ( BIT_DEPTH - 8 ) ));
    add2= _mm_set1_epi32(1 << (log2Wd - 1));
    c0= _mm_set1_epi16(wlxFlag);
    if (log2Wd >= 1)
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=16) {
            x0= _mm_load_si128(&src[x]);
            x2= _mm_load_si128(&src[x+8]);
            x1= _mm_unpackhi_epi16(_mm_mullo_epi16(x0,c0),_mm_mulhi_epi16(x0,c0));
            x3= _mm_unpackhi_epi16(_mm_mullo_epi16(x2,c0),_mm_mulhi_epi16(x2,c0));
            x0= _mm_unpacklo_epi16(_mm_mullo_epi16(x0,c0),_mm_mulhi_epi16(x0,c0));
            x2= _mm_unpacklo_epi16(_mm_mullo_epi16(x2,c0),_mm_mulhi_epi16(x2,c0));
            x0= _mm_add_epi32(x0,add2);
            x1= _mm_add_epi32(x1,add2);
            x2= _mm_add_epi32(x2,add2);
            x3= _mm_add_epi32(x3,add2);
            x0= _mm_srai_epi32(x0,log2Wd);
            x1= _mm_srai_epi32(x1,log2Wd);
            x2= _mm_srai_epi32(x2,log2Wd);
            x3= _mm_srai_epi32(x3,log2Wd);
            x0= _mm_add_epi32(x0,add);
            x1= _mm_add_epi32(x1,add);
            x2= _mm_add_epi32(x2,add);
            x3= _mm_add_epi32(x3,add);
            x0= _mm_packus_epi32(x0,x1);
            x2= _mm_packus_epi32(x2,x3);
            x0= _mm_packus_epi16(x0,x2);

            _mm_storeu_si128(&dst[x],x0);

        }
        dst  += dststride;
        src  += srcstride;
    }else
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=16) {

            x0= _mm_load_si128(&src[x]);
            x2= _mm_load_si128(&src[x+8]);
            x1= _mm_unpackhi_epi16(_mm_mullo_epi16(x0,c0),_mm_mulhi_epi16(x0,c0));
            x3= _mm_unpackhi_epi16(_mm_mullo_epi16(x2,c0),_mm_mulhi_epi16(x2,c0));
            x0= _mm_unpacklo_epi16(_mm_mullo_epi16(x0,c0),_mm_mulhi_epi16(x0,c0));
            x2= _mm_unpacklo_epi16(_mm_mullo_epi16(x2,c0),_mm_mulhi_epi16(x2,c0));

            x0= _mm_add_epi32(x0,add2);
            x1= _mm_add_epi32(x1,add2);
            x2= _mm_add_epi32(x2,add2);
            x3= _mm_add_epi32(x3,add2);

            x0= _mm_packus_epi32(x0,x1);
            x2= _mm_packus_epi32(x2,x3);
            x0= _mm_packus_epi16(x0,x2);

            _mm_storeu_si128(&dst[x],x0);

        }
        dst  += dststride;
        src  += srcstride;
    }
}

static void FUNC(weighted_pred_avg)(uint8_t denom, int16_t wl0Flag, int16_t wl1Flag,
                                         int16_t ol0Flag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride,
                                         int16_t *src1, int16_t *src2, ptrdiff_t srcstride,
                                         int width, int height)
{
    int shift, shift2;
    int log2Wd;
    int w0;
    int w1;
    int o0;
    int o1;
    int x , y;
    int add;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);
    __m128i x0,x1,x2,x3,r0,r1,r2,r3,c0,c1,c2;
    shift = 14 - BIT_DEPTH;
    log2Wd = denom + shift;
    //w0 = wl0Flag;
    //w1 = wl1Flag;
    o0 = (ol0Flag) * ( 1 << ( BIT_DEPTH - 8 ) );
    o1 = (ol1Flag) * ( 1 << ( BIT_DEPTH - 8 ) );
    //add = (o0 + o1 + 1) << log2Wd;
    shift2= (log2Wd + 1);
    c0= _mm_set1_epi16(wl0Flag);
    c1= _mm_set1_epi16(wl1Flag);
    c2= _mm_set1_epi32((o0 + o1 + 1) << log2Wd);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=16) {
            x0= _mm_load_si128(&src1[x]);
            x1= _mm_load_si128(&src1[x+8]);
            x2= _mm_load_si128(&src2[x]);
            x3= _mm_load_si128(&src2[x+8]);
            r0= _mm_unpacklo_epi16(_mm_mullo_epi16(x0,c0),_mm_mulhi_epi16(x0,c0));
            r1= _mm_unpacklo_epi16(_mm_mullo_epi16(x1,c0),_mm_mulhi_epi16(x1,c0));
            r2= _mm_unpacklo_epi16(_mm_mullo_epi16(x2,c1),_mm_mulhi_epi16(x2,c1));
            r3= _mm_unpacklo_epi16(_mm_mullo_epi16(x3,c1),_mm_mulhi_epi16(x3,c1));
            x0= _mm_unpackhi_epi16(_mm_mullo_epi16(x0,c0),_mm_mulhi_epi16(x0,c0));
            x1= _mm_unpackhi_epi16(_mm_mullo_epi16(x1,c0),_mm_mulhi_epi16(x1,c0));
            x2= _mm_unpackhi_epi16(_mm_mullo_epi16(x2,c1),_mm_mulhi_epi16(x2,c1));
            x3= _mm_unpackhi_epi16(_mm_mullo_epi16(x3,c1),_mm_mulhi_epi16(x3,c1));

            r0= _mm_add_epi32(r0,r2);
            r1= _mm_add_epi32(r1,r3);
            r2= _mm_add_epi32(x0,x2);
            r3= _mm_add_epi32(x1,x3);

            r0= _mm_add_epi32(r0,c2);
            r1= _mm_add_epi32(r1,c2);
            r2= _mm_add_epi32(r2,c2);
            r3= _mm_add_epi32(r3,c2);

            r0= _mm_srai_epi32(r0,shift2);
            r1= _mm_srai_epi32(r1,shift2);
            r2= _mm_srai_epi32(r2,shift2);
            r3= _mm_srai_epi32(r3,shift2);

            r0= _mm_packs_epi32(r0,r2);
            r1= _mm_packs_epi32(r1,r3);
            r0= _mm_packus_epi16(r0,r1);

            _mm_storeu_si128(&dst[x],r0);

            //dst[x] = av_clip_pixel((src1[x] * w0 + src2[x] * w1 + add) >> shift2);
        }
        dst  += dststride;
        src1 += srcstride;
        src2 += srcstride;
    }
}
#else

static void FUNC(weighted_pred)(uint8_t denom, int16_t wlxFlag, int16_t olxFlag,
                                     uint8_t *_dst, ptrdiff_t _dststride,
                                     int16_t *src, ptrdiff_t srcstride,
                                     int width, int height)
{
    int shift;
    int log2Wd;
    int wx;
    int ox;
    int x , y;
    int offset;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);

    shift = 14 - BIT_DEPTH;
    log2Wd = denom + shift;
    offset = 1 << (log2Wd - 1);
    wx = wlxFlag;
    ox = olxFlag * ( 1 << ( BIT_DEPTH - 8 ) );

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (log2Wd >= 1) {
                dst[x] = av_clip_pixel(((src[x] * wx + offset) >> log2Wd) + ox);
            } else {
                dst[x] = av_clip_pixel(src[x] * wx + ox);
            }
        }
        dst  += dststride;
        src  += srcstride;
    }
}

static void FUNC(weighted_pred_avg)(uint8_t denom, int16_t wl0Flag, int16_t wl1Flag,
                                         int16_t ol0Flag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride,
                                         int16_t *src1, int16_t *src2, ptrdiff_t srcstride,
                                         int width, int height)
{
    int shift;
    int log2Wd;
    int w0;
    int w1;
    int o0;
    int o1;
    int x , y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);

    shift = 14 - BIT_DEPTH;
    log2Wd = denom + shift;
    w0 = wl0Flag;
    w1 = wl1Flag;
    o0 = (ol0Flag) * ( 1 << ( BIT_DEPTH - 8 ) );
    o1 = (ol1Flag) * ( 1 << ( BIT_DEPTH - 8 ) );

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel((src1[x] * w0 + src2[x] * w1 + ((o0 + o1 + 1) << log2Wd)) >> (log2Wd + 1));
        }
        dst  += dststride;
        src1 += srcstride;
        src2 += srcstride;
    }
}
#endif


// line zero
#define P3 pix[-4*xstride]
#define P2 pix[-3*xstride]
#define P1 pix[-2*xstride]
#define P0 pix[-xstride]
#define Q0 pix[0]
#define Q1 pix[xstride]
#define Q2 pix[2*xstride]
#define Q3 pix[3*xstride]

// line three. used only for deblocking decision
#define TP3 pix[-4*xstride+3*ystride]
#define TP2 pix[-3*xstride+3*ystride]
#define TP1 pix[-2*xstride+3*ystride]
#define TP0 pix[-xstride+3*ystride]
#define TQ0 pix[3*ystride]
#define TQ1 pix[xstride+3*ystride]
#define TQ2 pix[2*xstride+3*ystride]
#define TQ3 pix[3*xstride+3*ystride]

static void FUNC(hevc_loop_filter_luma)(uint8_t *_pix, ptrdiff_t _xstride, ptrdiff_t _ystride,
                                        int no_p, int no_q, int _beta, int _tc)
{
    int d;
    pixel *pix = (pixel*)_pix;
    ptrdiff_t xstride = _xstride/sizeof(pixel);
    ptrdiff_t ystride = _ystride/sizeof(pixel);
    const int dp0 = abs(P2 - 2 * P1 +  P0);
    const int dq0 = abs(Q2 - 2 * Q1 +  Q0);
    const int dp3 = abs(TP2 - 2 * TP1 + TP0);
    const int dq3 = abs(TQ2 - 2 * TQ1 + TQ0);
    const int d0 = dp0 + dq0;
    const int d3 = dp3 + dq3;

    const int beta = _beta << (BIT_DEPTH - 8);
    const int tc = _tc << (BIT_DEPTH - 8);

    if (d0 + d3 < beta) {
        const int beta_3 = beta >> 3;
        const int beta_2 = beta >> 2;
        const int tc25 = ((tc * 5 + 1) >> 1);

        if(abs( P3 -  P0) + abs( Q3 -  Q0) < beta_3 && abs( P0 -  Q0) < tc25 &&
           abs(TP3 - TP0) + abs(TQ3 - TQ0) < beta_3 && abs(TP0 - TQ0) < tc25 &&
                                 (d0 << 1) < beta_2 &&      (d3 << 1) < beta_2) {
            // strong filtering
            const int tc2 = tc << 1;
            for(d = 0; d < 4; d++) {
                const int p3 = P3;
                const int p2 = P2;
                const int p1 = P1;
                const int p0 = P0;
                const int q0 = Q0;
                const int q1 = Q1;
                const int q2 = Q2;
                const int q3 = Q3;
                if(!no_p) {
                    P0 = av_clip_c(( p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4 ) >> 3, p0-tc2, p0+tc2);
                    P1 = av_clip_c(( p2 + p1 + p0 + q0 + 2 ) >> 2, p1-tc2, p1+tc2);
                    P2 = av_clip_c(( 2*p3 + 3*p2 + p1 + p0 + q0 + 4 ) >> 3, p2-tc2, p2+tc2);
                }
                if(!no_q) {
                    Q0 = av_clip_c(( p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4 ) >> 3, q0-tc2, q0+tc2);
                    Q1 = av_clip_c(( p0 + q0 + q1 + q2 + 2 ) >> 2, q1-tc2, q1+tc2);
                    Q2 = av_clip_c(( 2*q3 + 3*q2 + q1 + q0 + p0 + 4 ) >> 3, q2-tc2, q2+tc2);
                }
                pix += ystride;
            }
        } else { // normal filtering
            int nd_p = 1;
            int nd_q = 1;
            const int tc_2 = tc >> 1;
            if (dp0 + dp3 < ((beta+(beta>>1))>>3))
                nd_p = 2;
            if (dq0 + dq3 < ((beta+(beta>>1))>>3))
                nd_q = 2;

            for(d = 0; d < 4; d++) {
                const int p2 = P2;
                const int p1 = P1;
                const int p0 = P0;
                const int q0 = Q0;
                const int q1 = Q1;
                const int q2 = Q2;
                int delta0 = (9*(q0 - p0) - 3*(q1 - p1) + 8) >> 4;
                if (abs(delta0) < 10 * tc) {
                    delta0 = av_clip_c(delta0, -tc, tc);
                    if(!no_p)
                        P0 = av_clip_pixel(p0 + delta0);
                    if(!no_q)
                        Q0 = av_clip_pixel(q0 - delta0);
                    if(!no_p && nd_p > 1) {
                        const int deltap1 = av_clip_c((((p2 + p0 + 1) >> 1) - p1 + delta0) >> 1, -tc_2, tc_2);
                        P1 = av_clip_pixel(p1 + deltap1);
                    }
                    if(!no_q && nd_q > 1) {
                        const int deltaq1 = av_clip_c((((q2 + q0 + 1) >> 1) - q1 - delta0) >> 1, -tc_2, tc_2);
                        Q1 = av_clip_pixel(q1 + deltaq1);
                    }
                }
                pix += ystride;
            }
        }
    }
}

static void FUNC(hevc_loop_filter_chroma)(uint8_t *_pix, ptrdiff_t _xstride, ptrdiff_t _ystride, int no_p, int no_q, int _tc)
{
    int d;
    pixel *pix = (pixel*)_pix;
    ptrdiff_t xstride = _xstride/sizeof(pixel);
    ptrdiff_t ystride = _ystride/sizeof(pixel);
    const int tc = _tc << (BIT_DEPTH - 8);

    for(d = 0; d < 4; d++) {
        int delta0;
        const int p1 = P1;
        const int p0 = P0;
        const int q0 = Q0;
        const int q1 = Q1;
        delta0 = av_clip_c((((q0 - p0) << 2) + p1 - q1 + 4) >> 3, -tc, tc);
        if(!no_p)
            P0 = av_clip_pixel(p0 + delta0);
        if(!no_q)
            Q0 = av_clip_pixel(q0 - delta0);
        pix += ystride;
    }
}

#undef P3
#undef P2
#undef P1
#undef P0
#undef Q0
#undef Q1
#undef Q2
#undef Q3

#undef TP3
#undef TP2
#undef TP1
#undef TP0
#undef TQ0
#undef TQ1
#undef TQ2
#undef TQ3
