#include "libavcodec/hevc.h"

#if HAVE_AVX2
#include <immintrin.h>
#endif

#if HAVE_SSE2
#include <emmintrin.h>
#endif


#include <smmintrin.h>


#include "libavcodec/bit_depth_template.c"

#if COM16_C806_EMT
#define SHIFT_EMT_V (EMT_TRANSFORM_MATRIX_SHIFT + 1 + COM16_C806_TRANS_PREC)
#define ADD_EMT_V (1 << (SHIFT_EMT_V - 1))
#endif


void FUNC(emt_idst_VII_4x4_v_avx2) (int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
    const __m128i zeros = _mm_setzero_si128();
    const __m128i add  = _mm_set1_epi32(ADD_EMT_V);

    const __m128i max  = _mm_set1_epi32(clip_max);
    const __m128i min  = _mm_set1_epi32(clip_min);

    const __m128i a = _mm_set1_epi16(-336);
    const __m128i b = _mm_set1_epi16(296);
    const __m128i c = _mm_set1_epi16(219);
    const __m128i d = _mm_set1_epi16(117);

    __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;

      x0  = _mm_load_si128((__m128i *)x);
      x8  = _mm_load_si128((__m128i *)&x[8]);
      x12 = _mm_loadu_si128((__m128i *)&x[12]);
      x4  = _mm_loadu_si128((__m128i *)&x[4]);

      c0 = _mm_unpacklo_epi16(x8,x12);
      c2 = _mm_unpacklo_epi16(x0,x8);

      c3 = _mm_sub_epi16(zeros,x12);
      c3 = _mm_unpacklo_epi16(x0,c3);

      c4 = _mm_unpacklo_epi16(x4,zeros);

      c1 = _mm_subs_epi16(x0,x8);
      c1 = _mm_unpacklo_epi16(c1,x12);

      c0 = _mm_madd_epi16(c0,a);
      c1 = _mm_madd_epi16(c1,b);
      c2 = _mm_madd_epi16(c2,c);
      c3 = _mm_madd_epi16(c3,d);

      c4 = _mm_madd_epi16(c4,b);

      x0 = _mm_sub_epi32(c3,c0);
      x0 = _mm_add_epi32(x0,c4);

      x12 = _mm_add_epi32(c2,c3);
      x12 = _mm_sub_epi32(x12,c4);

      x4 = _mm_add_epi32(c2,c0);
      x4 = _mm_add_epi32(x4,c4);

      x0 =  _mm_add_epi32(x0,add);
      x4 =  _mm_add_epi32(x4,add);
      x8 =  _mm_add_epi32(c1,add);
      x12 = _mm_add_epi32(x12,add);

      x0 =  _mm_srai_epi32(x0,SHIFT_EMT_V);
      x4 =  _mm_srai_epi32(x4,SHIFT_EMT_V);
      x8 =  _mm_srai_epi32(x8,SHIFT_EMT_V);
      x12 = _mm_srai_epi32(x12,SHIFT_EMT_V);

      x0 = _mm_min_epi32(x0,max);
      x0 = _mm_max_epi32(x0,min);

      x4 = _mm_min_epi32(x4,max);
      x4 = _mm_max_epi32(x4,min);

      x8 = _mm_min_epi32(x8,max);
      x8 = _mm_max_epi32(x8,min);

      x12 = _mm_min_epi32(x12,max);
      x12 = _mm_max_epi32(x12,min);

      c0 = _mm_unpacklo_epi32(x0,x8);
      c1 = _mm_unpacklo_epi32(x4,x12);
      c2 = _mm_unpackhi_epi32(x0,x8);
      c3 = _mm_unpackhi_epi32(x4,x12);

      c0 = _mm_packs_epi32(c0,c2);
      c1 = _mm_packs_epi32(c1,c3);

      x0 = _mm_unpacklo_epi16(c0,c1);
      x4 = _mm_unpackhi_epi16(c0,c1);

      _mm_store_si128((__m128i *)&block[0],x0);
      _mm_store_si128((__m128i *)&block[8],x4);
}



void FUNC(emt_idst_VII_4x4_h_avx2)(int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
  const int shift = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
  const __m128i zeros = _mm_setzero_si128();
  const __m128i add  = _mm_set1_epi32(1 << (shift - 1));

  const  __m128i max  = _mm_set1_epi32(clip_max);
  const __m128i min  = _mm_set1_epi32(clip_min);

  const __m128i a = _mm_set1_epi16(-336);
  const __m128i b = _mm_set1_epi16(296);
  const __m128i c = _mm_set1_epi16(219);
  const __m128i d = _mm_set1_epi16(117);

  __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;

    x0  = _mm_load_si128((__m128i *)x);
    x8  = _mm_load_si128((__m128i *)&x[8]);
    x12 = _mm_loadu_si128((__m128i *)&x[12]);
    x4  = _mm_loadu_si128((__m128i *)&x[4]);

    c0 = _mm_unpacklo_epi16(x8,x12);
    c2 = _mm_unpacklo_epi16(x0,x8);

    c3 = _mm_sub_epi16(zeros,x12);
    c3 = _mm_unpacklo_epi16(x0,c3);

    c4 = _mm_unpacklo_epi16(x4,zeros);

    c1 = _mm_subs_epi16(x0,x8);
    c1 = _mm_unpacklo_epi16(c1,x12);

    c0 = _mm_madd_epi16(c0,a);
    c1 = _mm_madd_epi16(c1,b);
    c2 = _mm_madd_epi16(c2,c);
    c3 = _mm_madd_epi16(c3,d);

    c4 = _mm_madd_epi16(c4,b);

    x0 = _mm_sub_epi32(c3,c0);
    x0 = _mm_add_epi32(x0,c4);

    x12 = _mm_add_epi32(c2,c3);
    x12 = _mm_sub_epi32(x12,c4);

    x4 = _mm_add_epi32(c2,c0);
    x4 = _mm_add_epi32(x4,c4);

    x0 =  _mm_add_epi32(x0,add);
    x4 =  _mm_add_epi32(x4,add);
    x8 =  _mm_add_epi32(c1,add);
    x12 = _mm_add_epi32(x12,add);

    x0 =  _mm_srai_epi32(x0,shift);
    x4 =  _mm_srai_epi32(x4,shift);
    x8 =  _mm_srai_epi32(x8,shift);
    x12 = _mm_srai_epi32(x12,shift);

    x0 = _mm_min_epi32(x0,max);
    x0 = _mm_max_epi32(x0,min);

    x4 = _mm_min_epi32(x4,max);
    x4 = _mm_max_epi32(x4,min);

    x8 = _mm_min_epi32(x8,max);
    x8 = _mm_max_epi32(x8,min);

    x12 = _mm_min_epi32(x12,max);
    x12 = _mm_max_epi32(x12,min);

    c0 = _mm_unpacklo_epi32(x0,x8);
    c1 = _mm_unpacklo_epi32(x4,x12);
    c2 = _mm_unpackhi_epi32(x0,x8);
    c3 = _mm_unpackhi_epi32(x4,x12);

    c0 = _mm_packs_epi32(c0,c2);
    c1 = _mm_packs_epi32(c1,c3);

    x0 = _mm_unpacklo_epi16(c0,c1);
    x4 = _mm_unpackhi_epi16(c0,c1);

    _mm_store_si128((__m128i *)&block[0],x0);
    _mm_store_si128((__m128i *)&block[8],x4);
}

void FUNC(emt_idct_VIII_4x4_v_avx2) (int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
    const __m128i zeros = _mm_setzero_si128();
    const __m128i add  = _mm_set1_epi32(ADD_EMT_V);

    const __m128i max  = _mm_set1_epi32(clip_max);
    const __m128i min  = _mm_set1_epi32(clip_min);

    const __m128i a = _mm_set1_epi16(219);
    const __m128i b = _mm_set1_epi16(296);
    const __m128i c = _mm_set1_epi16(117);
    const __m128i d = _mm_set1_epi16(336);

    __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;

    x0  = _mm_load_si128((__m128i *)x);
    x8  = _mm_load_si128((__m128i *)&x[8]);
    x12 = _mm_loadu_si128((__m128i *)&x[12]);
    x4  = _mm_loadu_si128((__m128i *)&x[4]);


    c1 = _mm_subs_epi16(x0,x8);

    c0 = _mm_sub_epi16(zeros,x12);
    c1 = _mm_unpacklo_epi16(c1,c0);
    c0 = _mm_unpacklo_epi16(x8,c0);

    c2 = _mm_unpacklo_epi16(x0,x8);

    c3 = _mm_unpacklo_epi16(x0,x12);

    c4 = _mm_unpacklo_epi16(x4,zeros);


    c0 = _mm_madd_epi16(c0,a);
    c1 = _mm_madd_epi16(c1,b);
    c2 = _mm_madd_epi16(c2,c);
    c3 = _mm_madd_epi16(c3,d);

    c4 = _mm_madd_epi16(c4,b);

    x0 = _mm_add_epi32(c3,c0);
    x0 = _mm_add_epi32(x0,c4);

    x8 = _mm_sub_epi32(c3,c2);
    x8 = _mm_sub_epi32(x8,c4);

    x12 = _mm_add_epi32(c2,c0);
    x12 = _mm_sub_epi32(x12,c4);

    x0 =  _mm_add_epi32(x0,add);
    x4 =  _mm_add_epi32(c1,add);
    x8 =  _mm_add_epi32(x8,add);
    x12 = _mm_add_epi32(x12,add);

    x0 =  _mm_srai_epi32(x0,SHIFT_EMT_V);
    x4 =  _mm_srai_epi32(x4,SHIFT_EMT_V);
    x8 =  _mm_srai_epi32(x8,SHIFT_EMT_V);
    x12 = _mm_srai_epi32(x12,SHIFT_EMT_V);

    x0 = _mm_min_epi32(x0,max);
    x0 = _mm_max_epi32(x0,min);

    x4 = _mm_min_epi32(x4,max);
    x4 = _mm_max_epi32(x4,min);

    x8 = _mm_min_epi32(x8,max);
    x8 = _mm_max_epi32(x8,min);

    x12 = _mm_min_epi32(x12,max);
    x12 = _mm_max_epi32(x12,min);

    c0 = _mm_unpacklo_epi32(x0,x8);
    c1 = _mm_unpacklo_epi32(x4,x12);
    c2 = _mm_unpackhi_epi32(x0,x8);
    c3 = _mm_unpackhi_epi32(x4,x12);

    c0 = _mm_packs_epi32(c0,c2);
    c1 = _mm_packs_epi32(c1,c3);

    x0 = _mm_unpacklo_epi16(c0,c1);
    x4 = _mm_unpackhi_epi16(c0,c1);

    _mm_store_si128((__m128i *)&block[0],x0);
    _mm_store_si128((__m128i *)&block[8],x4);
}



void FUNC(emt_idct_VIII_4x4_h_avx2)(int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
    int shift = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
  const __m128i zeros = _mm_setzero_si128();
  const __m128i add  = _mm_set1_epi32(1 << (shift - 1));

  const __m128i max  = _mm_set1_epi32(clip_max);
  const __m128i min  = _mm_set1_epi32(clip_min);

  const __m128i a = _mm_set1_epi16(219);
  const __m128i b = _mm_set1_epi16(296);
  const __m128i c = _mm_set1_epi16(117);
  const __m128i d = _mm_set1_epi16(336);

  __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;

    x0  = _mm_load_si128((__m128i *)x);
    x8  = _mm_load_si128((__m128i *)&x[8]);
    x12 = _mm_loadu_si128((__m128i *)&x[12]);
    x4  = _mm_loadu_si128((__m128i *)&x[4]);

    c1 = _mm_subs_epi16(x0,x8);

    c0 = _mm_sub_epi16(zeros,x12);
    c1 = _mm_unpacklo_epi16(c1,c0);
    c0 = _mm_unpacklo_epi16(x8,c0);

    c2 = _mm_unpacklo_epi16(x0,x8);

    c3 = _mm_unpacklo_epi16(x0,x12);

    c4 = _mm_unpacklo_epi16(x4,zeros);


    c0 = _mm_madd_epi16(c0,a);
    c1 = _mm_madd_epi16(c1,b);
    c2 = _mm_madd_epi16(c2,c);
    c3 = _mm_madd_epi16(c3,d);

    c4 = _mm_madd_epi16(c4,b);

    x0 = _mm_add_epi32(c3,c0);
    x0 = _mm_add_epi32(x0,c4);

    x8 = _mm_sub_epi32(c3,c2);
    x8 = _mm_sub_epi32(x8,c4);

    x12 = _mm_add_epi32(c2,c0);
    x12 = _mm_sub_epi32(x12,c4);

    x0 =  _mm_add_epi32(x0,add);
    x4 =  _mm_add_epi32(c1,add);
    x8 =  _mm_add_epi32(x8,add);
    x12 = _mm_add_epi32(x12,add);

    x0 =  _mm_srai_epi32(x0,shift);
    x4 =  _mm_srai_epi32(x4,shift);
    x8 =  _mm_srai_epi32(x8,shift);
    x12 = _mm_srai_epi32(x12,shift);

    x0 = _mm_min_epi32(x0,max);
    x0 = _mm_max_epi32(x0,min);

    x4 = _mm_min_epi32(x4,max);
    x4 = _mm_max_epi32(x4,min);

    x8 = _mm_min_epi32(x8,max);
    x8 = _mm_max_epi32(x8,min);

    x12 = _mm_min_epi32(x12,max);
    x12 = _mm_max_epi32(x12,min);

    c0 = _mm_unpacklo_epi32(x0,x8);
    c1 = _mm_unpacklo_epi32(x4,x12);
    c2 = _mm_unpackhi_epi32(x0,x8);
    c3 = _mm_unpackhi_epi32(x4,x12);

    c0 = _mm_packs_epi32(c0,c2);
    c1 = _mm_packs_epi32(c1,c3);

    x0 = _mm_unpacklo_epi16(c0,c1);
    x4 = _mm_unpackhi_epi16(c0,c1);

    _mm_store_si128((__m128i *)&block[0],x0);
    _mm_store_si128((__m128i *)&block[8],x4);

//    fprintf(stderr,"intrinsic\n");
//    for (int i=0;i<16;i++){
//        fprintf(stderr,"%d ",block[i]);
//    }
//    fprintf(stderr,"\n");
}
