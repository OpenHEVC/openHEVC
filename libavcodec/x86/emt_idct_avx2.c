#include "libavcodec/x86/hevcdsp.h"
#include "libavcodec/hevc_amt_defs.h"

#if HAVE_AVX2
#include <immintrin.h>
#endif

#if HAVE_SSE2
#include <emmintrin.h>
#endif

#include <time.h>
#include "syscall.h"

#include <smmintrin.h>


#include "libavcodec/bit_depth_template.c"


#define SHIFT_EMT_V (EMT_TRANSFORM_MATRIX_SHIFT + 1 + COM16_C806_TRANS_PREC)
#define ADD_EMT_V (1 << (SHIFT_EMT_V - 1))

#define VAR_DECL_AND_LOAD4X4_AVX2_V()\
    const __m256i add  = _mm256_set1_epi32(ADD_EMT_V);\
    const __m256i max  = _mm256_set1_epi16(clip_max); \
    const __m256i min  = _mm256_set1_epi16(clip_min); \
    __m256i x0,x4,x8,x12, tmp2;                       \
    __m256i _src = _mm256_load_si256((__m256i *)src); \

#define VAR_DECL_AND_LOAD4X4_AVX2_H()\
    const int shift = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;\
    const __m256i add  = _mm256_set1_epi32(1 << (shift - 1));\
    const __m256i max  = _mm256_set1_epi16(clip_max);\
    const __m256i min  = _mm256_set1_epi16(clip_min);\
    __m256i x0,x4,x8,x12;                      \
    __m256i _src = _mm256_load_si256((__m256i *)src);\

#define TRANSPOSE_4X4_AVX2()\
    x0   = _mm256_permute4x64_epi64(_src, 0b00010100); \
    x4   = _mm256_permute4x64_epi64(_src, 0b10111110); \
    x8   = _mm256_unpacklo_epi16(x0, x4);              \
    x12  = _mm256_permute4x64_epi64(x8, 0b00010100);   \
    x4   = _mm256_permute4x64_epi64(x8, 0b10111110);   \
    _src = _mm256_unpacklo_epi16(x12, x4);             \
    x0   = _mm256_permute4x64_epi64(_src, 0b00000000); \
    x4   = _mm256_permute4x64_epi64(_src, 0b01010101); \
    x8   = _mm256_permute4x64_epi64(_src, 0b10101010); \
    x12  = _mm256_permute4x64_epi64(_src, 0b11111111); \

#define COMPUTE4X4_AVX2()\
    x0  = _mm256_madd_epi16(x0 , dct_matrix);  \
    x4  = _mm256_madd_epi16(x4 , dct_matrix);  \
    x8  = _mm256_madd_epi16(x8 , dct_matrix);  \
    x12 = _mm256_madd_epi16(x12, dct_matrix); \
    x0 = _mm256_hadd_epi32(x0, x4);     \
    x0 = _mm256_permute4x64_epi64(x0, 0b11011000);\
    x8 = _mm256_hadd_epi32(x8, x12);    \
    x8 = _mm256_permute4x64_epi64(x8, 0b11011000);\

#define ROUND_4X4_AVX2_V()\
    x0 =  _mm256_add_epi32(x0,add);          \
    x8 =  _mm256_add_epi32(x8,add);          \
    x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V); \
    x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V); \

#define ROUND_4X4_AVX2_H()\
    x0 =  _mm256_add_epi32(x0,add);    \
    x8 =  _mm256_add_epi32(x8,add);    \
    x0 =  _mm256_srai_epi32(x0,shift); \
    x8 =  _mm256_srai_epi32(x8,shift); \

#define PACK_CLIP_AND_STORE_4X4_AVX2()\
    x0 = _mm256_packs_epi32(x0,x8);            \
    /*x0 = _mm256_permutevar8x32_epi32(x0,_mm256_setr_epi32(0,4,1,5,2,6,3,7));*/\
    x0 = _mm256_permute4x64_epi64(x0, 0b11011000);\
    x0 = _mm256_min_epi16(x0,max);             \
    x0 = _mm256_max_epi16(x0,min);             \
    _mm256_store_si256((__m256i *)&dst[0],x0); \

#define debug4X4_16(x) \
    _mm256_store_si256((__m256i *)&dst[0],x);\
    fprintf(stderr,"temp %s\n",#x);         \
    for (int j=0; j < 4; j++){\
    for (int i=0;i<4;i++){                    \
    fprintf(stderr,"%d ",dst[j*4+i]);     \
    }                                         \
    fprintf(stderr,"\n");                     \
    }\

#define debug_32(x) \
    _mm256_store_si256((__m256i *)&dst[0],x);\
    fprintf(stderr,"temp %s\n",#x);         \
    for (int i=0;i<8;i++){                    \
    fprintf(stderr,"%d ",*((int32_t *)&dst[2*i]));     \
    }\
    fprintf(stderr,"\n");

//extern __inline__ uint64_t rdtsc(void) {
//  uint64_t x;
//  x = clock_gettime(CLOCK_MONOTONIC)
//  //__asm__ volatile ("rdtsc" : "=A" (x));
//  return x;
//}


//void FUNC(emt_idst_VII_4x4_v_avx2) (int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
//{
//    //const __m128i zeros = _mm_setzero_si128();
//    //const __m128i add  = _mm_set1_epi32(ADD_EMT_V);

//    //const __m128i max  = _mm_set1_epi16(clip_max);
//    //const __m128i min  = _mm_set1_epi16(clip_min);
//    //fprintf(stderr,"AMT\n");

////    const __m128i a_VII = _mm_set1_epi16(-336);
////    const __m128i b_VII = _mm_set1_epi16(296);
////    const __m128i c_VII = _mm_set1_epi16(219);
////    const __m128i d_VII = _mm_set1_epi16(117);
////     struct timespec t1,t2;

////     clock_gettime(CLOCK_REALTIME,&t1);
//    __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;

//      x0  = _mm_load_si128((__m128i *)x);
//      x8  = _mm_loadl_epi64((__m128i *)&x[8]);
//      x12 = _mm_loadl_epi64((__m128i *)&x[12]);
//      //x4  = _mm_loadl_epi64((__m128i *)&x[4]);
//      //x12 = _mm_shuffle_epi32(x8,0b01001110);

//      c0 = _mm_unpacklo_epi16(x8,x12);
//      c2 = _mm_unpacklo_epi16(x0,x8);

//      //c3 = _mm_sub_epi16(_mm_setzero_si128(),x12);
//      c3 = _mm_unpacklo_epi16(x0,x12);

//      c4 = _mm_unpackhi_epi16(x0,_mm_setzero_si128());

//      c1 = _mm_sub_epi16(x0,x8);
//      c1 = _mm_unpacklo_epi16(c1,x12);

//      c0 = _mm_madd_epi16(c0,_mm_set1_epi16(-336));
//      c1 = _mm_madd_epi16(c1,_mm_set1_epi16(296));
//      c2 = _mm_madd_epi16(c2,_mm_set1_epi16(219));
//      c3 = _mm_madd_epi16(c3,_mm_set_epi16(-117,117,-117,117,-117,117,-117,117));

//      c4 = _mm_madd_epi16(c4,_mm_set1_epi16(296));

//      x0 = _mm_sub_epi32(c3,c0);
//      x0 = _mm_add_epi32(x0,c4);

//      x12 = _mm_add_epi32(c2,c3);
//      x12 = _mm_sub_epi32(x12,c4);

//      x4 = _mm_add_epi32(c2,c0);
//      x4 = _mm_add_epi32(x4,c4);

//      x0 =  _mm_add_epi32(x0, _mm_set1_epi32(ADD_EMT_V));
//      x4 =  _mm_add_epi32(x4, _mm_set1_epi32(ADD_EMT_V));
//      x8 =  _mm_add_epi32(c1, _mm_set1_epi32(ADD_EMT_V));
//      x12 = _mm_add_epi32(x12,_mm_set1_epi32(ADD_EMT_V));

//      x0 =  _mm_srai_epi32(x0, SHIFT_EMT_V);
//      x4 =  _mm_srai_epi32(x4, SHIFT_EMT_V);
//      x8 =  _mm_srai_epi32(x8, SHIFT_EMT_V);
//      x12 = _mm_srai_epi32(x12,SHIFT_EMT_V);

////      c0 = _mm_unpacklo_epi32(x0,x8);
////      c1 = _mm_unpacklo_epi32(x4,x12);

////      c2 = _mm_unpackhi_epi32(x0,x8);
////      c3 = _mm_unpackhi_epi32(x4,x12);

////      c0 = _mm_packs_epi32(c0,c2);
////      c1 = _mm_packs_epi32(c1,c3);
//      c0 = _mm_packs_epi32(x0,x8);
//      c1 = _mm_packs_epi32(x4,x12);

//      c0 = _mm_shuffle_epi32(c0,0b11011000);
//      c1 = _mm_shuffle_epi32(c1,0b11011000);

//// We should not need clipping here since a last clip is done in h funct and 16
//// we already used 16 bits saturation
////      c0 = _mm_min_epi16(c0,max);
////      c0 = _mm_max_epi16(c0,min);

////      c1 = _mm_min_epi16(c1,max);
////      c1 = _mm_max_epi16(c1,min);

//      x0 = _mm_unpacklo_epi16(c0,c1);
//      x4 = _mm_unpackhi_epi16(c0,c1);

//      x0 = _mm_shuffle_epi32(x0,0b11011000);
//      x4 = _mm_shuffle_epi32(x4,0b11011000);

//      _mm_store_si128((__m128i *)&block[0],x0);
//      _mm_store_si128((__m128i *)&block[8],x4);

//      //uint64_t t2 = rdtsc();
////      clock_gettime(CLOCK_REALTIME,&t2);
////      fprintf(stderr,"num cycles:%ld, %ld\n",t2.tv_nsec-t1.tv_nsec,t2.tv_sec-t1.tv_sec);
//}

//void FUNC(emt_idst_VII_4x4_h_avx2)(int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
//{
//  const int shift = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
//  const __m128i zeros = _mm_setzero_si128();
//  const __m128i add  = _mm_set1_epi32(1 << (shift - 1));

//  const __m128i max  = _mm_set1_epi16(clip_max);
//  const __m128i min  = _mm_set1_epi16(clip_min);

//  const __m128i a_VII = _mm_set1_epi16(-336);
//  const __m128i b_VII = _mm_set1_epi16(296);
//  const __m128i c_VII = _mm_set1_epi16(219);
//  const __m128i d_VII = _mm_set_epi16(-117,117,-117,117,-117,117,-117,117);

//  __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;
//  //fprintf(stderr,"AMT\n");

//    x0  = _mm_load_si128((__m128i *)x);
//    x8  = _mm_loadl_epi64((__m128i *)&x[8]);
//    x12 = _mm_loadl_epi64((__m128i *)&x[12]);
//    //x4  = _mm_loadl_epi64((__m128i *)&x[4]);
//    //x12 = _mm_shuffle_epi32(x8,0b01001110);

//    c0 = _mm_unpacklo_epi16(x8,x12);
//    c2 = _mm_unpacklo_epi16(x0,x8);

//    //c3 = _mm_sub_epi16(zeros,x12);
//    c3 = _mm_unpacklo_epi16(x0,x12);

//    c4 = _mm_unpackhi_epi16(x0,zeros);

//    c1 = _mm_sub_epi16(x0,x8);
//    c1 = _mm_unpacklo_epi16(c1,x12);

//    c0 = _mm_madd_epi16(c0,a_VII);
//    c1 = _mm_madd_epi16(c1,b_VII);
//    c2 = _mm_madd_epi16(c2,c_VII);
//    c3 = _mm_madd_epi16(c3,d_VII);

//    c4 = _mm_madd_epi16(c4,b_VII);

//    x0 = _mm_sub_epi32(c3,c0);
//    x0 = _mm_add_epi32(x0,c4);

//    x12 = _mm_add_epi32(c2,c3);
//    x12 = _mm_sub_epi32(x12,c4);

//    x4 = _mm_add_epi32(c2,c0);
//    x4 = _mm_add_epi32(x4,c4);

//    x0 =  _mm_add_epi32(x0,add);
//    x4 =  _mm_add_epi32(x4,add);
//    x8 =  _mm_add_epi32(c1,add);
//    x12 = _mm_add_epi32(x12,add);

//    x0 =  _mm_srai_epi32(x0,shift);
//    x4 =  _mm_srai_epi32(x4,shift);
//    x8 =  _mm_srai_epi32(x8,shift);
//    x12 = _mm_srai_epi32(x12,shift);

//    c0 = _mm_unpacklo_epi32(x0,x8);
//    c1 = _mm_unpacklo_epi32(x4,x12);
//    c2 = _mm_unpackhi_epi32(x0,x8);
//    c3 = _mm_unpackhi_epi32(x4,x12);

//    c0 = _mm_packs_epi32(c0,c2);
//    c1 = _mm_packs_epi32(c1,c3);

//    c0 = _mm_min_epi16(c0,max);
//    c0 = _mm_max_epi16(c0,min);

//    c1 = _mm_min_epi16(c1,max);
//    c1 = _mm_max_epi16(c1,min);

//    x0 = _mm_unpacklo_epi16(c0,c1);
//    x4 = _mm_unpackhi_epi16(c0,c1);

//    _mm_store_si128((__m128i *)&block[0],x0);
//    _mm_store_si128((__m128i *)&block[8],x4);
//}

//void FUNC(emt_idct_VIII_4x4_v_avx2) (int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
//{
//    const __m128i zeros = _mm_setzero_si128();
//    const __m128i add  = _mm_set1_epi32(ADD_EMT_V);

//    const __m128i max  = _mm_set1_epi16(clip_max);
//    const __m128i min  = _mm_set1_epi16(clip_min);

//    const __m128i a_VIII = _mm_set_epi16(-219,219,-219,219,-219,219,-219,219);
//    const __m128i b_VIII = _mm_set_epi16(-296,296,-296,296,-296,296,-296,296);
//    const __m128i c_VIII = _mm_set1_epi16(117);
//    const __m128i d_VIII = _mm_set1_epi16(336);

//    __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;
//    fprintf(stderr,"AMT\n");


//    x0  = _mm_load_si128((__m128i *)x);
//    x8  = _mm_loadl_epi64((__m128i *)&x[8]);
//    x12 = _mm_loadl_epi64((__m128i *)&x[12]);
//    //x4  = _mm_loadl_epi64((__m128i *)&x[4]);
//    //x12 = _mm_shuffle_epi32(x8,0b01001110);

//    //FIXME: risk of integer overflow?
//    c1 = _mm_sub_epi16(x0,x8);

//    //c0 used to store the opposit of x12 used 2x
//    //c0 = _mm_sub_epi16(zeros,x12);

//    c1 = _mm_unpacklo_epi16(c1,x12);
//    c0 = _mm_unpacklo_epi16(x8,x12);
//    c2 = _mm_unpacklo_epi16(x0,x8);
//    c3 = _mm_unpacklo_epi16(x0,x12);

//    c4 = _mm_unpackhi_epi16(x0,zeros);

//    c0 = _mm_madd_epi16(c0,a_VIII);
//    c1 = _mm_madd_epi16(c1,b_VIII);
//    c2 = _mm_madd_epi16(c2,c_VIII);
//    c3 = _mm_madd_epi16(c3,d_VIII);
//    c4 = _mm_madd_epi16(c4,b_VIII);

//    x0 = _mm_add_epi32(c3,c0);
//    x0 = _mm_add_epi32(x0,c4);

//    x8 = _mm_sub_epi32(c3,c2);
//    x8 = _mm_sub_epi32(x8,c4);

//    x12 = _mm_add_epi32(c2,c0);
//    x12 = _mm_sub_epi32(x12,c4);

//    x0 =  _mm_add_epi32(x0,add);
//    x4 =  _mm_add_epi32(c1,add);
//    x8 =  _mm_add_epi32(x8,add);
//    x12 = _mm_add_epi32(x12,add);

//    x0 =  _mm_srai_epi32(x0,SHIFT_EMT_V);
//    x4 =  _mm_srai_epi32(x4,SHIFT_EMT_V);
//    x8 =  _mm_srai_epi32(x8,SHIFT_EMT_V);
//    x12 = _mm_srai_epi32(x12,SHIFT_EMT_V);

//    c0 = _mm_unpacklo_epi32(x0,x8);
//    c1 = _mm_unpacklo_epi32(x4,x12);

//    c2 = _mm_unpackhi_epi32(x0,x8);
//    c3 = _mm_unpackhi_epi32(x4,x12);

//    c0 = _mm_packs_epi32(c0,c2);
//    c1 = _mm_packs_epi32(c1,c3);

//// We should not need to clip here since a last clip is done in h funct
////    c0 = _mm_min_epi16(c0,max);
////    c0 = _mm_max_epi16(c0,min);

////    c1 = _mm_min_epi16(c1,max);
////    c1 = _mm_max_epi16(c1,min);

//    x0 = _mm_unpacklo_epi16(c0,c1);
//    x4 = _mm_unpackhi_epi16(c0,c1);

//    _mm_store_si128((__m128i *)&block[0],x0);
//    _mm_store_si128((__m128i *)&block[8],x4);
//    fprintf(stderr,"intrinsic\n");
//    for (int i=0;i<16;i++){
//        fprintf(stderr,"%d ",block[i]);
//    }
//    fprintf(stderr,"\n");
//}



//void FUNC(emt_idct_VIII_4x4_h_avx2)(int16_t *x, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
//{
//    int shift = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
//  const __m128i zeros = _mm_setzero_si128();
//  const __m128i add  = _mm_set1_epi32(1 << (shift - 1));

//  const __m128i max  = _mm_set1_epi16(clip_max);
//  const __m128i min  = _mm_set1_epi16(clip_min);

//  const __m128i a_VIII = _mm_set_epi16(-219,219,-219,219,-219,219,-219,219);
//  const __m128i b_VIII = _mm_set_epi16(-296,296,-296,296,-296,296,-296,296);
//  const __m128i c_VIII = _mm_set1_epi16(117);
//  const __m128i d_VIII = _mm_set1_epi16(336);
//  fprintf(stderr,"intrinsic\n");
//  for (int i=0;i<16;i++){
//      fprintf(stderr,"%d ",x[i]);
//  }
//  fprintf(stderr,"\n");
//  __m128i c0, c1,c2,c3,c4, x0, x8, x12, x4;

//    x0  = _mm_load_si128((__m128i *)x);
//    x8  = _mm_loadl_epi64((__m128i *)&x[8]);
//    x12 = _mm_loadl_epi64((__m128i *)&x[12]);
//    //x4  = _mm_loadl_epi64((__m128i *)&x[4]);
//    //x12 = _mm_shuffle_epi32(x8,0b01001110);

//    //FIXME: might overflow?
//    c1 = _mm_sub_epi16(x0,x8);

//    //c0 = _mm_sub_epi16(zeros,x12);

//    c1 = _mm_unpacklo_epi16(c1,x12);
//    c0 = _mm_unpacklo_epi16(x8,x12);

//    c2 = _mm_unpacklo_epi16(x0,x8);

//    c3 = _mm_unpacklo_epi16(x0,x12);

//    c4 = _mm_unpackhi_epi16(x0,zeros);

//    c0 = _mm_madd_epi16(c0,a_VIII);
//    c1 = _mm_madd_epi16(c1,b_VIII);
//    c2 = _mm_madd_epi16(c2,c_VIII);
//    c3 = _mm_madd_epi16(c3,d_VIII);

//    c4 = _mm_madd_epi16(c4,b_VIII);

//    x0 = _mm_add_epi32(c3,c0);
//    x0 = _mm_add_epi32(x0,c4);

//    x8 = _mm_sub_epi32(c3,c2);
//    x8 = _mm_sub_epi32(x8,c4);

//    x12 = _mm_add_epi32(c2,c0);
//    x12 = _mm_sub_epi32(x12,c4);

//    x0 =  _mm_add_epi32(x0,add);
//    x4 =  _mm_add_epi32(c1,add);
//    x8 =  _mm_add_epi32(x8,add);
//    x12 = _mm_add_epi32(x12,add);

//    x0 =  _mm_srai_epi32(x0,shift);
//    x4 =  _mm_srai_epi32(x4,shift);
//    x8 =  _mm_srai_epi32(x8,shift);
//    x12 = _mm_srai_epi32(x12,shift);

//    c0 = _mm_unpacklo_epi32(x0,x8);
//    c1 = _mm_unpacklo_epi32(x4,x12);
//    c2 = _mm_unpackhi_epi32(x0,x8);
//    c3 = _mm_unpackhi_epi32(x4,x12);

//    c0 = _mm_packs_epi32(c0,c2);
//    c1 = _mm_packs_epi32(c1,c3);

//    c0 = _mm_min_epi16(c0,max);
//    c0 = _mm_max_epi16(c0,min);

//    c1 = _mm_min_epi16(c1,max);
//    c1 = _mm_max_epi16(c1,min);

//    x0 = _mm_unpacklo_epi16(c0,c1);
//    x4 = _mm_unpackhi_epi16(c0,c1);

//    _mm_store_si128((__m128i *)&block[0],x0);
//    _mm_store_si128((__m128i *)&block[8],x4);


//    fprintf(stderr,"intrinsic\n");
//    for (int i=0;i<16;i++){
//        fprintf(stderr,"%d ",block[i]);
//    }
//    fprintf(stderr,"\n");
//}


void FUNC(emt_idct_VIII_4x4_v_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max)
{
  //int shift = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
  const __m256i add  = _mm256_set1_epi32(ADD_EMT_V);

  const __m256i max  = _mm256_set1_epi16(clip_max);
  const __m256i min  = _mm256_set1_epi16(clip_min);

//  const int16_t __attribute__ ((aligned (32))) matrix1[16] = { 336,  296,  219,  117 ,
//                                           296,    0, -296, -296,
//                                           219, -296, -117,  336,
//                                           117, -296,  336, -219 };
//  __m256i matrix = _mm256_load_si256((__m256i *)src);
////  debug4X4_16(matrix);

//  __m256i x0,x4,x8,x12, tmp2;
//  __m128i transpose1,transpose2,tmp;
//  transpose1 = _mm_load_si128((__m128i *)src);
//  transpose2 = _mm_load_si128((__m128i *)&src[8]);

//  tmp        = _mm_unpacklo_epi16(transpose1,transpose2);
//  transpose1 = _mm_unpackhi_epi16(transpose1,transpose2);

//  transpose2 = _mm_unpacklo_epi16(tmp,transpose1);
//  transpose1 = _mm_unpackhi_epi16(tmp,transpose1);

//  matrix = _mm256_insertf128_si256(_mm256_castsi128_si256(transpose2), (transpose1), 0x1);
//  _mm256_store_si256((__m256i *)&src[0],matrix);

//  debug4X4_16(matrix);
//  const int16_t __attribute__ ((aligned (32))) matrix2[16] = { 1, 0, 0, 0,
//                                0, 1, 0, 0,
//                                0, 0, 1, 0,
//                                0, 0, 0, 1 };
  __m256i x0,x4,x8,x12, tmp2;
  __m256i _src = _mm256_load_si256((__m256i *)src);
//  debug4X4_16(_src);


  x0 = _mm256_permute4x64_epi64(_src,0b00010100);
  x4 = _mm256_permute4x64_epi64(_src,0b10111110);
//  debug4X4_16(x0);
//  debug4X4_16(x4);

  x8 = _mm256_unpacklo_epi16(x0,x4);
//  debug4X4_16(x8);

  x12 = _mm256_permute4x64_epi64(x8,0b00010100);
  x4 = _mm256_permute4x64_epi64(x8,0b10111110);
//  debug4X4_16(x0);
//  debug4X4_16(x4);

  _src = _mm256_unpacklo_epi16(x12,x4);
//  debug4X4_16(_src);

  x0 = _mm256_permute4x64_epi64(_src, 0b00000000);
  x4 = _mm256_permute4x64_epi64(_src, 0b01010101);
  x8 = _mm256_permute4x64_epi64(_src, 0b10101010);
  x12 = _mm256_permute4x64_epi64(_src, 0b11111111);


   //tmp2 = _mm256_load_si256(matrix1);
  tmp2 =_mm256_setr_epi16(336,  296,  219,  117 ,
                          296,    0, -296, -296,
                          219, -296, -117,  336,
                          117, -296,  336, -219);// _mm256_setr_epi16(-219,336,-296,117,336,-117,-296,219,-296,-296,0,296,117,219,296,336);

//    x0  = _mm256_set1_epi64x(*((long long *)&src[0]));
//    x4  = _mm256_set1_epi64x(*((long long *)&src[4]));
//    x8  = _mm256_set1_epi64x(*((long long *)&src[8]));
//    x12 = _mm256_set1_epi64x(*((long long *)&src[12]));


//    debug4X4_16(x0);
//    debug4X4_16(x4);
//    debug4X4_16(x8);
//    debug4X4_16(x12);
//    debug4X4_16(matrix);

    //debug4X4_16(x0)

    x0  = _mm256_madd_epi16(x0,tmp2);
    x4  = _mm256_madd_epi16(x4,tmp2);
    x8  = _mm256_madd_epi16(x8,tmp2);
    x12 = _mm256_madd_epi16(x12,tmp2);
//    debug_32(x0);
//    debug_32(x4);
//    debug_32(x8);
//    debug_32(x12);

    x0 = _mm256_hadd_epi32(x0,x4);
    x8 = _mm256_hadd_epi32(x8,x12);
//    debug_32(x0);
//    debug_32(x8);

    x0 =  _mm256_add_epi32(x0,add);
    x8 =  _mm256_add_epi32(x8,add);
//    debug_32(x0);
//    debug_32(x8);

    x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V);
    x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V);
//    debug_32(x0);
//    debug_32(x8);

    x0 = _mm256_packs_epi32(x0,x8);
    x0 = _mm256_permutevar8x32_epi32(x0,_mm256_setr_epi32(0,4,1,5,2,6,3,7));

    // clip
    x0 = _mm256_min_epi16(x0,max);
    x0 = _mm256_max_epi16(x0,min);

//    debug4X4_16(x0);

    _mm256_store_si256((__m256i *)&dst[0],x0);

}

//TODO most of the time transform range is 15 and so are clipping values we might want to hard code those values for further
// optimizations
void FUNC(emt_idct_VIII_4x4_h_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max)
{
  int shift = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
  const __m256i add  = _mm256_set1_epi32(1 << (shift - 1));

  const __m256i max  = _mm256_set1_epi16(clip_max);
  const __m256i min  = _mm256_set1_epi16(clip_min);

  const __m256i   dct_matrix =_mm256_setr_epi16(336,  296,  219,  117 ,
                                          296,    0, -296, -296,
                                          219, -296, -117,  336,
                                          117, -296,  336, -219);

  __m256i _src = _mm256_load_si256((__m256i *)src);

//  debug4X4_16(_src);
  __m256i x0,x4,x8,x12;

  x0 = _mm256_permute4x64_epi64(_src,0b00010100);
  x4 = _mm256_permute4x64_epi64(_src,0b10111110);
//  debug4X4_16(x0);
//  debug4X4_16(x4);

  x8 = _mm256_unpacklo_epi16(x0,x4);
//  debug4X4_16(x8);

  x12 = _mm256_permute4x64_epi64(x8,0b00010100);
  x4 = _mm256_permute4x64_epi64(x8,0b10111110);
//  debug4X4_16(x0);
//  debug4X4_16(x4);

  _src = _mm256_unpacklo_epi16(x12,x4);
//  debug4X4_16(_src);

  x0 = _mm256_permute4x64_epi64(_src, 0b00000000);
  x4 = _mm256_permute4x64_epi64(_src, 0b01010101);
  x8 = _mm256_permute4x64_epi64(_src, 0b10101010);
  x12 = _mm256_permute4x64_epi64(_src, 0b11111111);
//  debug4X4_16(x0);
//  debug4X4_16(x4);
//  debug4X4_16(x8);
//  debug4X4_16(x12);

    x0  = _mm256_madd_epi16(x0,dct_matrix);
    x4  = _mm256_madd_epi16(x4,dct_matrix);
    x8  = _mm256_madd_epi16(x8,dct_matrix);
    x12 = _mm256_madd_epi16(x12,dct_matrix);
//    debug_32(x0);
//    debug_32(x4);
//    debug_32(x8);
//    debug_32(x12);

    x0 = _mm256_hadd_epi32(x0,x4);
    x0 = _mm256_permute4x64_epi64(x0, 0b11011000);
    x8 = _mm256_hadd_epi32(x8,x12);
    x8 = _mm256_permute4x64_epi64(x8, 0b11011000);
//    debug_32(x0);
//    debug_32(x8);

    x0 =  _mm256_add_epi32(x0,add);
    x8 =  _mm256_add_epi32(x8,add);
//    debug_32(x0);
//    debug_32(x8);

    x0 =  _mm256_srai_epi32(x0,shift);
    x8 =  _mm256_srai_epi32(x8,shift);
//    debug_32(x0);
//    debug_32(x8);
//__m256i t1,t2;
//t1=_mm256_setr_epi32(0,1,2,3,4,5,6,7);
//debug_32(t1);
//t2=_mm256_setr_epi32(8,9,10,11,12,13,14,15);
//debug_32(t2);
//t2 = _mm256_hadd_epi32(t1,t2);
////t1=_mm256_packs_epi32(t1,t2);
////    t1 = _mm256_permute4x64_epi64(t1, 0b11011000);
//debug_32(t2);

    x0 = _mm256_packs_epi32(x0,x8);

    //FIXME we might be able to avoid this permutation.
    //x0 = _mm256_permutevar8x32_epi32(x0,_mm256_setr_epi32(0,4,1,5,2,6,3,7));
    x0 = _mm256_permute4x64_epi64(x0, 0b11011000);
    // clip
    x0 = _mm256_min_epi16(x0,max);
    x0 = _mm256_max_epi16(x0,min);

    _mm256_store_si256((__m256i *)&dst[0],x0);

//    fprintf(stderr,"intrinsic\n");
//    for (int i=0;i<16;i++){
//        fprintf(stderr,"%d ",dst[i]);
//    }
//    fprintf(stderr,"\n");
}

void FUNC(emt_idct_V_4x4_v_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max){
    VAR_DECL_AND_LOAD4X4_AVX2_V();
    const __m256i dct_matrix = _mm256_setr_epi16(194,  274,  274,  274,
                                                 274,  241,  -86, -349,
                                                 274,  -86, -349,  241,
                                                 274, -349,  241,  -86);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_V();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}

void FUNC(emt_idct_V_4x4_h_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max){
    VAR_DECL_AND_LOAD4X4_AVX2_H();
    const __m256i dct_matrix = _mm256_setr_epi16(194,  274,  274,  274,
                                                 274,  241,  -86, -349,
                                                 274,  -86, -349,  241,
                                                 274, -349,  241,  -86);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_H();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}


void FUNC(emt_idct_II_4x4_v_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max){
    VAR_DECL_AND_LOAD4X4_AVX2_V();
    const __m256i dct_matrix = _mm256_setr_epi16(256,  334,  256,  139 ,
                                                 256,  139, -256, -334 ,
                                                 256, -139, -256,  334 ,
                                                 256, -334,  256, -139);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_V();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}

void FUNC(emt_idct_II_4x4_h_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max){
    VAR_DECL_AND_LOAD4X4_AVX2_H();
    const __m256i dct_matrix = _mm256_setr_epi16(256,  334,  256,  139 ,
                                                 256,  139, -256, -334 ,
                                                 256, -139, -256,  334 ,
                                                 256, -334,  256, -139);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_H();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}

void FUNC(emt_idst_I_4x4_v_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max){
    VAR_DECL_AND_LOAD4X4_AVX2_V();
    const __m256i dct_matrix = _mm256_setr_epi16(190,  308,  308,  190 ,
                                                 308,  190, -190, -308 ,
                                                 308, -190, -190,  308 ,
                                                 190, -308,  308, -190);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_V();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}

void FUNC(emt_idst_I_4x4_h_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max){
    VAR_DECL_AND_LOAD4X4_AVX2_H();
    const __m256i dct_matrix = _mm256_setr_epi16(190,  308,  308,  190 ,
                                                 308,  190, -190, -308 ,
                                                 308, -190, -190,  308 ,
                                                 190, -308,  308, -190);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_H();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}

void FUNC(emt_idst_VII_4x4_v_avx2) (int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max){
    VAR_DECL_AND_LOAD4X4_AVX2_V();
    const __m256i dct_matrix = _mm256_setr_epi16(117,  296,  336,  219 ,
                                                 219,  296, -117, -336 ,
                                                 296,    0, -296,  296 ,
                                                 336, -296,  219, -117);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_V();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}

void FUNC(emt_idst_VII_4x4_h_avx2)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max)
{
    VAR_DECL_AND_LOAD4X4_AVX2_H();
    const __m256i dct_matrix = _mm256_setr_epi16 (117,  296,  336,  219 ,
                                                  219,  296, -117, -336 ,
                                                  296,    0, -296,  296 ,
                                                  336, -296,  219, -117);
    TRANSPOSE_4X4_AVX2();
    COMPUTE4X4_AVX2();
    ROUND_4X4_AVX2_H();
    PACK_CLIP_AND_STORE_4X4_AVX2();
}

