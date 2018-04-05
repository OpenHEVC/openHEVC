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
#include "stdint.h"
#include <smmintrin.h>


#include "libavcodec/bit_depth_template.c"

#define SHIFT_EMT_V (EMT_TRANSFORM_MATRIX_SHIFT + 1 + COM16_C806_TRANS_PREC)
#define ADD_EMT_V (1 << (SHIFT_EMT_V - 1))


#define SHIFT_EMT_H ((EMT_TRANSFORM_MATRIX_SHIFT + 15 - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC)
#define ADD_EMT_H (1 << (SHIFT_EMT_H - 1))


#include "../hevcdec.h"
 void hevc_emt_avx2_luma(HEVCContext *s,HEVCLocalContext *lc, HEVCTransformContext *tr_ctx, int16_t *tmp, int h, int v,int size){
s->hevcdsp.idct2_emt_v2[tr_ctx->scan_ctx.x_cg_last_sig][tr_ctx->scan_ctx.y_cg_last_sig][tr_ctx->log2_tr_size_minus2](lc->cg_coeffs[0], tmp,TR_DTT_summary[tr_ctx->log2_tr_size_minus2][v],DTT_summary[tr_ctx->log2_tr_size_minus2][h]);
//s->hevcdsp.idct2_emt_h2[tr_ctx->scan_ctx.x_cg_last_sig][tr_ctx->log2_tr_size_minus2](tmp, lc->tu.coeffs[0],DTT_summary[tr_ctx->log2_tr_size_minus2][h]);
}

 void hevc_emt_avx2_c(HEVCContext *s,HEVCLocalContext *lc, HEVCTransformContext *tr_ctx, int16_t *tmp, int h, int v,int size){
s->hevcdsp.idct2_emt_v2[tr_ctx->scan_ctx.x_cg_last_sig][tr_ctx->scan_ctx.y_cg_last_sig][tr_ctx->log2_tr_size_minus2](lc->cg_coeffs[1], tmp,TR_DTT_summary[tr_ctx->log2_tr_size_minus2][v],DTT_summary[tr_ctx->log2_tr_size_minus2][h]);
//s->hevcdsp.idct2_emt_h2[tr_ctx->scan_ctx.x_cg_last_sig][tr_ctx->log2_tr_size_minus2](tmp, lc->tu.coeffs[1],DTT_summary[tr_ctx->log2_tr_size_minus2][h]);
}

#define CORE_4x4_MULT(src_1_0,src_2_0,result_1,result_2)\
 src_2_0 = _mm256_unpacklo_epi16(src_2_0, _mm256_srli_si256(src_2_0, 8));      \
 src_2_1 = _mm256_permute2x128_si256(src_2_0, src_2_0, 1 + 16);                \
 src_2_0 = _mm256_permute2x128_si256(src_2_0, src_2_0, 0);                     \
 x0  = _mm256_shuffle_epi32(src_1_0, 0);                                       \
 x4  = _mm256_shuffle_epi32(src_1_0, 1 + 4 + 16 + 64);                         \
 x8  = _mm256_shuffle_epi32(src_1_0, 2 + 8 + 32 + 128);                        \
 x12 = _mm256_shuffle_epi32(src_1_0, 3 + 12 + 48 + 192);                       \
 x0  = _mm256_madd_epi16(x0,  src_2_0);                                        \
 x4  = _mm256_madd_epi16(x4,  src_2_1);                                        \
 x8  = _mm256_madd_epi16(x8,  src_2_0);                                        \
 x12 = _mm256_madd_epi16(x12, src_2_1);                                        \
 result_2 = _mm256_add_epi32(x8, x12);                                         \
 result_1 = _mm256_add_epi32(x0, x4);                                          \


#define SCALE_AND_PACK(x0_tmp,x8_tmp,DIR)\
 x0_tmp =  _mm256_add_epi32(x0_tmp,_mm256_set1_epi32(ADD_EMT_##DIR));          \
 x8_tmp =  _mm256_add_epi32(x8_tmp,_mm256_set1_epi32(ADD_EMT_##DIR));          \
 x0_tmp =  _mm256_srai_epi32(x0_tmp,SHIFT_EMT_##DIR);                          \
 x8_tmp =  _mm256_srai_epi32(x8_tmp,SHIFT_EMT_##DIR);                          \
 x0_tmp =  _mm256_packs_epi32(x0_tmp,x8_tmp);                                  \

#define IN_LOOP_LOAD_H(i,j,k,num_cg)\
 src_1_0 = CG[num_cg*i+k];                 \
 src_2_0 = _mm256_load_si256((__m256i *) &dtt_matrix_h[16*(num_cg*k+j)]);\

#define IN_LOOP_MULT_H(i,j,k,num_cg)\
 IN_LOOP_LOAD_H(i,j,k,num_cg)                                    \
 CORE_4x4_MULT(src_1_0,src_2_0,result_1,result_2)                              \
 x0_tmp = _mm256_add_epi32(result_1,x0_tmp);                                   \
 x8_tmp = _mm256_add_epi32(result_2,x8_tmp);                                   \

#define IN_LOOP_STORE_H(i,j,k,num_cg)\
    ((int64_t *)dst)[num_cg*4*i+j]     = _mm256_extract_epi64(x0_tmp,0);       \
    ((int64_t *)dst)[num_cg*(4*i+1)+j] = _mm256_extract_epi64(x0_tmp,1);       \
    ((int64_t *)dst)[num_cg*(4*i+2)+j] = _mm256_extract_epi64(x0_tmp,2);       \
    ((int64_t *)dst)[num_cg*(4*i+3)+j] = _mm256_extract_epi64(x0_tmp,3);       \

#define IN_LOOP_LOAD_V(i,j,k,num_cg)\
src_1_0 = _mm256_load_si256((__m256i *) &dtt_matrix_v[16*(num_cg*i+k)]);\
src_2_0 = _mm256_load_si256((__m256i*) &src[16*(num_cg*k+j)]);                 \

 #define IN_LOOP_MULT_V(i,j,k,num_cg)\
 IN_LOOP_LOAD_V(i,j,k,num_cg)                                    \
 CORE_4x4_MULT(src_1_0,src_2_0,result_1,result_2)                              \
 x0_tmp = _mm256_add_epi32(result_1,x0_tmp);                                   \
 x8_tmp = _mm256_add_epi32(result_2,x8_tmp);                                   \

#define IN_LOOP_STORE_V(i,j,k,num_cg)\
 CG[num_cg*i+j]=x0_tmp;                  \

#define DECL()                                                                     \
__m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_1_0,src_2_0,src_2_1,result_1,result_2;\


#define DECL_0()                                                                   \
__m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_1_0,src_2_0,src_2_1;                  \

//______________________________________________________________________________
//4x4
/*
#define IDCT4X4_V(DCT_type,DCT_num)                                            \
void FUNC(emt_idct_##DCT_num##_4x4_v_avx2)(int16_t *restrict src, int16_t *restrict dst)       \
{                                                                              \
    __m256i x0,x4,x8,x12,src_1_0, src_2_0, src_2_1,result_1,result_2;          \
                                                                               \
    src_1_0 = _mm256_load_si256((__m256i *) TR_##DCT_type##_4x4_per_CG);       \
    src_2_0 = _mm256_load_si256((__m256i*) src);                               \
                                                                               \
    CORE_4x4_MULT(src_1_0,src_2_0,result_1,result_2)                           \
    SCALE_AND_PACK(result_1,result_2,V)                                        \
                                                                               \
    _mm256_store_si256((__m256i*)dst, result_1);                               \
}                                                                              \


#define IDCT4X4_H(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_4x4_h_avx2)(int16_t *restrict src, int16_t *restrict dst)       \
{                                                                              \
    __m256i x0,x4,x8,x12,src_1_0, src_2_0, src_2_1,result_1,result_2;          \
                                                                               \
    src_1_0 = _mm256_load_si256((__m256i*) src);                               \
    src_2_0 = _mm256_load_si256((__m256i*) DCT_type##_4x4_per_CG);             \
                                                                               \
    CORE_4x4_MULT(src_1_0,src_2_0,result_1,result_2)                           \
    SCALE_AND_PACK(result_1,result_2,H)                                        \
                                                                               \
    _mm256_store_si256((__m256i*)dst, result_1);                               \
}                                                                              \

//______________________________________________________________________________
//8x8

#define IDCT8X8_V(DCT_type,DCT_num)                                            \
void FUNC(emt_idct_##DCT_num##_8x8_v_avx2)(int16_t *restrict src, int16_t *restrict dst)       \
{                                                                              \
    int i,j;                                                                   \
    for(i = 0; i < 2; i++){                                                    \
        for (j = 0; j < 2; j++){                                               \
            __m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_2_1,result_1,result_2;      \
            __m256i     src_1_0 = _mm256_load_si256((__m256i *) TR_##DCT_type##_8x8_per_CG[2*i]);\
            __m256i     src_2_0 = _mm256_load_si256((__m256i*) &src[16*j]);    \
            CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                       \
            IN_LOOP_MULT_V(i,j,1,DCT_type,8,2)                                 \
            SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                    \
            _mm256_store_si256((__m256i *)&dst[(2*i+j)*16],x0_tmp);            \
        }                                                                      \
    }                                                                          \
}                                                                              \


#define IDCT8X8_H(DCT_type,DCT_num)                                            \
void FUNC(emt_idct_##DCT_num##_8x8_h_avx2)(int16_t *restrict  src, int16_t *restrict  dst)     \
{                                                                              \
    int i,j;                                                                   \
    for(i = 0; i < 2; i++){                                                    \
        for (j = 0; j < 2; j++){                                               \
    __m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_2_1,result_1,result_2;              \
    __m256i src_1_0 = _mm256_load_si256((__m256i*) &src[16*(2*i)]);            \
    __m256i src_2_0 = _mm256_load_si256((__m256i *) DCT_type##_8x8_per_CG[j]); \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,DCT_type,8,2)                                         \
            SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                    \
            ((int64_t *)dst)[2*4*i+j]     = _mm256_extract_epi64(x0_tmp,0);    \
            ((int64_t *)dst)[2*(4*i+1)+j] = _mm256_extract_epi64(x0_tmp,1);    \
            ((int64_t *)dst)[2*(4*i+2)+j] = _mm256_extract_epi64(x0_tmp,2);    \
            ((int64_t *)dst)[2*(4*i+3)+j] = _mm256_extract_epi64(x0_tmp,3);    \
        }                                                                      \
    }                                                                          \
}                                                                              \


//______________________________________________________________________________
//16x16

#define IDCT16X16_V(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_16x16_v_avx2)(int16_t *restrict src, int16_t *restrict dst)\
{                                                                              \
    int i,j;                                                                   \
         for (j = 0; j < 4; j++){                                              \
    for(i = 0; i < 4; i++){                                                    \
    __m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_2_1,result_1,result_2;              \
           __m256i     src_1_0 = _mm256_load_si256((__m256i *) TR_##DCT_type##_16x16_per_CG[4*i]);\
           __m256i     src_2_0 = _mm256_load_si256((__m256i*) &src[16*j]);     \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,DCT_type,16,4)                                        \
    IN_LOOP_MULT_V(i,j,2,DCT_type,16,4)                                        \
    IN_LOOP_MULT_V(i,j,3,DCT_type,16,4)                                        \
            SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                    \
            _mm256_store_si256((__m256i *)&dst[(4*i+j)*16],x0_tmp);            \
        }                                                                      \
    }                                                                          \
}                                                                              \

#define IDCT16X16_H(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_16x16_h_avx2)(int16_t * restrict  src, int16_t * restrict dst)\
{                                                                              \
    int i,j;                                                                   \
    for(i = 0; i < 4; i++){                                                    \
        for (j = 0; j < 4; j++){                                               \
    __m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_2_1,result_1,result_2;              \
    __m256i src_1_0 = _mm256_load_si256((__m256i*) &src[16*(4*i)]);            \
    __m256i src_2_0 = _mm256_load_si256((__m256i *) DCT_type##_16x16_per_CG[j]);\
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,DCT_type,16,4)                                        \
    IN_LOOP_MULT_H(i,j,2,DCT_type,16,4)                                        \
    IN_LOOP_MULT_H(i,j,3,DCT_type,16,4)                                        \
            SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                    \
            ((int64_t *)dst)[4*4*i+j]     = _mm256_extract_epi64(x0_tmp,0);    \
            ((int64_t *)dst)[4*(4*i+1)+j] = _mm256_extract_epi64(x0_tmp,1);    \
            ((int64_t *)dst)[4*(4*i+2)+j] = _mm256_extract_epi64(x0_tmp,2);    \
            ((int64_t *)dst)[4*(4*i+3)+j] = _mm256_extract_epi64(x0_tmp,3);    \
       }                                                                       \
    }                                                                          \
}                                                                              \

//______________________________________________________________________________
//32x32

#define IDCT32x32_V(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_32x32_v_avx2)(int16_t *restrict src, int16_t *restrict dst)\
{                                                                              \
    int i,j;                                                                   \
    for(i = 0; i < 8; i++){                                                    \
        for (j = 0; j < 8; j++){                                               \
    __m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_2_1,result_1,result_2;              \
           __m256i     src_1_0 = _mm256_load_si256((__m256i *) TR_##DCT_type##_32x32_per_CG[8*i]);\
            __m256i    src_2_0 = _mm256_load_si256((__m256i*) &src[16*j]);     \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,DCT_type,32,8)                                        \
    IN_LOOP_MULT_V(i,j,2,DCT_type,32,8)                                        \
    IN_LOOP_MULT_V(i,j,3,DCT_type,32,8)                                        \
    IN_LOOP_MULT_V(i,j,4,DCT_type,32,8)                                        \
    IN_LOOP_MULT_V(i,j,5,DCT_type,32,8)                                        \
    IN_LOOP_MULT_V(i,j,6,DCT_type,32,8)                                        \
    IN_LOOP_MULT_V(i,j,7,DCT_type,32,8)                                        \
            SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                    \
            _mm256_store_si256((__m256i *)&dst[(8*i+j)*16],x0_tmp);            \
        }                                                                      \
    }                                                                          \
}                                                                              \



#define IDCT32x32_H(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_32x32_h_avx2)(int16_t * restrict src, int16_t * restrict dst)\
{                                                                              \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
        while ( j < 8){                                                        \
            __m256i x0_tmp,x8_tmp,x0,x4,x8,x12,src_2_1,result_1,result_2;      \
            __m256i src_1_0 = _mm256_load_si256((__m256i*) &src[16*(8*i)]);    \
            __m256i src_2_0 = _mm256_load_si256((__m256i *) DCT_type##_32x32_per_CG[j]);\
            CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                       \
            IN_LOOP_MULT_H(i,j,1,DCT_type,32,8)                                \
            IN_LOOP_MULT_H(i,j,2,DCT_type,32,8)                                \
            IN_LOOP_MULT_H(i,j,3,DCT_type,32,8)                                \
            IN_LOOP_MULT_H(i,j,4,DCT_type,32,8)                                \
            IN_LOOP_MULT_H(i,j,5,DCT_type,32,8)                                \
            IN_LOOP_MULT_H(i,j,6,DCT_type,32,8)                                \
            IN_LOOP_MULT_H(i,j,7,DCT_type,32,8)                                \
            SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                    \
            ((int64_t *)dst)[8*4*i+j]     = _mm256_extract_epi64(x0_tmp,0);    \
            ((int64_t *)dst)[8*(4*i+1)+j] = _mm256_extract_epi64(x0_tmp,1);    \
            ((int64_t *)dst)[8*(4*i+2)+j] = _mm256_extract_epi64(x0_tmp,2);    \
            ((int64_t *)dst)[8*(4*i+3)+j] = _mm256_extract_epi64(x0_tmp,3);    \
       j++;                                                                    \
       }                                                                       \
    i++;                                                                       \
    }                                                                          \
}                                                                              \

*/


 /* Pruned versions
 */

 //______________________________________________________________________________
 // 4x4
#define IDCT4X4_PRUNED_H()\
void FUNC(emt_idct_4x4_0_h_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    __m256i x0,x4,x8,x12, src_2_1,result_1,result_2;                           \
    __m256i src_1_0 = _mm256_load_si256((__m256i*) src);                       \
    __m256i src_2_0 = _mm256_load_si256((__m256i*) dtt_matrix);     \
    CORE_4x4_MULT(src_1_0,src_2_0,result_1,result_2)                           \
    SCALE_AND_PACK(result_1,result_2,H)                                        \
    _mm256_store_si256((__m256i*)dst, result_1);                               \
    }

 //______________________________________________________________________________
 // 8x8

#define IDCT8X8_PRUNED_H()                                     \
void FUNC(emt_idct_8x8_1_h_avx2)(int16_t *restrict  src, int16_t *restrict  dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    for(i = 0; i < 2; i++){                                                    \
    for (j = 0; j < 2; j++){                                                   \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,2)                                         \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,2)                                         \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,2)                                        \
    }                                                                          \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_8x8_0_h_avx2)(int16_t *restrict  src, int16_t *restrict  dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    for(i = 0; i < 2; i++){                                                    \
    for (j = 0; j < 2; j++){                                                   \
    DECL_0()                                                                   \
    IN_LOOP_LOAD_H(i,j,0,2)                                         \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,2)                                        \
    }                                                                          \
    }                                                                          \
    }

 //______________________________________________________________________________
 //16x16

#define IDCT16X16_PRUNED_H()\
void FUNC(emt_idct_16x16_0_h_avx2)(int16_t * restrict  src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < 4; j++){                                                   \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,4)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,4)                                       \
    }                                                                          \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_16x16_1_h_avx2)(int16_t * restrict  src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < 4; j++){                                                   \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,4)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,4)                                       \
    }                                                                          \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_16x16_2_h_avx2)(int16_t * restrict  src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < 4; j++){                                                   \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,4)                                        \
    IN_LOOP_MULT_H(i,j,2,4)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,4)                                       \
    }                                                                          \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_16x16_3_h_avx2)(int16_t * restrict  src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < 4; j++){                                                   \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,4)                                        \
    IN_LOOP_MULT_H(i,j,2,4)                                        \
    IN_LOOP_MULT_H(i,j,3,4)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,4)                                       \
    }                                                                          \
    }                                                                          \
    }


 //______________________________________________________________________________
 //32x32


#define IDCT32x32_PRUNED_H()\
void FUNC(emt_idct_32x32_0_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL_0()                                                                   \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_32x32_1_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_32x32_2_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,8)                                        \
    IN_LOOP_MULT_H(i,j,2,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_32x32_3_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,8)                                        \
    IN_LOOP_MULT_H(i,j,2,8)                                        \
    IN_LOOP_MULT_H(i,j,3,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_32x32_4_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,8)                                        \
    IN_LOOP_MULT_H(i,j,2,8)                                        \
    IN_LOOP_MULT_H(i,j,3,8)                                        \
    IN_LOOP_MULT_H(i,j,4,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_32x32_5_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,8)                                        \
    IN_LOOP_MULT_H(i,j,2,8)                                        \
    IN_LOOP_MULT_H(i,j,3,8)                                        \
    IN_LOOP_MULT_H(i,j,4,8)                                        \
    IN_LOOP_MULT_H(i,j,5,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_32x32_6_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    __m256i CG[64];                                                            \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,8)                                        \
    IN_LOOP_MULT_H(i,j,2,8)                                        \
    IN_LOOP_MULT_H(i,j,3,8)                                        \
    IN_LOOP_MULT_H(i,j,4,8)                                        \
    IN_LOOP_MULT_H(i,j,5,8)                                        \
    IN_LOOP_MULT_H(i,j,6,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \
void FUNC(emt_idct_32x32_7_h_avx2)(int16_t * restrict src, int16_t * restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j=0;                                                                 \
    while(i < 8){ j=0;                                                         \
    while ( j < 8){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,8)                                        \
    IN_LOOP_MULT_H(i,j,2,8)                                        \
    IN_LOOP_MULT_H(i,j,3,8)                                        \
    IN_LOOP_MULT_H(i,j,4,8)                                        \
    IN_LOOP_MULT_H(i,j,5,8)                                        \
    IN_LOOP_MULT_H(i,j,6,8)                                        \
    IN_LOOP_MULT_H(i,j,7,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,8)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \
    }                                                                          \


#define DO_H_7(num_cg)\
    i = 0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,2,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,3,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,4,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,5,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,6,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,7,num_cg)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \

#define DO_H_6(num_cg)\
    i =  0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,2,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,3,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,4,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,5,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,6,num_cg)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }                                                                          \

#define DO_H_5(num_cg)\
    i =  0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,2,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,3,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,4,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,5,num_cg)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }

#define DO_H_4(num_cg)\
    i =  0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,2,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,3,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,4,num_cg)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }

#define DO_H_3(num_cg)\
    i = 0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,2,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,3,num_cg)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }

#define DO_H_2(num_cg)\
    i =  0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,num_cg)                                        \
    IN_LOOP_MULT_H(i,j,2,num_cg)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }

#define DO_H_1(num_cg)\
    i =  0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_H(i,j,1,num_cg)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }

#define DO_H_0(num_cg)\
    i =  0;                                                                 \
    while(i < num_cg){ j=0;                                                         \
    while ( j < num_cg){                                                            \
    DECL_0()                                                                     \
    IN_LOOP_LOAD_H(i,j,0,num_cg)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    SCALE_AND_PACK(x0_tmp,x8_tmp,H)                                            \
    IN_LOOP_STORE_H(i,j,k,num_cg)                                       \
    j++;                                                                       \
    }                                                                          \
    i++;                                                                       \
    }

#define DO_H(maxx,num_cg)\
    DO_H_##maxx(num_cg)\


 //______________________________________________________________________________
 //

#define IDCT32x32_PRUNED_V_MAC(maxx,num_cg)\
void FUNC(emt_idct_32x32_##maxx##_0_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL_0()                                                                   \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                       \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \
void FUNC(emt_idct_32x32_##maxx##_1_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                       \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \
void FUNC(emt_idct_32x32_##maxx##_2_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,8)                                        \
    IN_LOOP_MULT_V(i,j,2,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                       \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \
void FUNC(emt_idct_32x32_##maxx##_3_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,8)                                        \
    IN_LOOP_MULT_V(i,j,2,8)                                        \
    IN_LOOP_MULT_V(i,j,3,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                       \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \
void FUNC(emt_idct_32x32_##maxx##_4_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,8)                                        \
    IN_LOOP_MULT_V(i,j,2,8)                                        \
    IN_LOOP_MULT_V(i,j,3,8)                                        \
    IN_LOOP_MULT_V(i,j,4,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                       \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \
void FUNC(emt_idct_32x32_##maxx##_5_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,8)                                        \
    IN_LOOP_MULT_V(i,j,2,8)                                        \
    IN_LOOP_MULT_V(i,j,3,8)                                        \
    IN_LOOP_MULT_V(i,j,4,8)                                        \
    IN_LOOP_MULT_V(i,j,5,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                       \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \
void FUNC(emt_idct_32x32_##maxx##_6_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,8)                                        \
    IN_LOOP_MULT_V(i,j,2,8)                                        \
    IN_LOOP_MULT_V(i,j,3,8)                                        \
    IN_LOOP_MULT_V(i,j,4,8)                                        \
    IN_LOOP_MULT_V(i,j,5,8)                                        \
    IN_LOOP_MULT_V(i,j,6,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \
void FUNC(emt_idct_32x32_##maxx##_7_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[64];                                                            \
    for(i = 0; i < 8; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,8)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,8)                                        \
    IN_LOOP_MULT_V(i,j,2,8)                                        \
    IN_LOOP_MULT_V(i,j,3,8)                                        \
    IN_LOOP_MULT_V(i,j,4,8)                                        \
    IN_LOOP_MULT_V(i,j,5,8)                                        \
    IN_LOOP_MULT_V(i,j,6,8)                                        \
    IN_LOOP_MULT_V(i,j,7,8)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,8)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,8)\
    }                                                                          \


#define IDCT32x32_PRUNED_V()\
    IDCT32x32_PRUNED_V_MAC(0,8)\
    IDCT32x32_PRUNED_V_MAC(1,8)\
    IDCT32x32_PRUNED_V_MAC(2,8)\
    IDCT32x32_PRUNED_V_MAC(3,8)\
    IDCT32x32_PRUNED_V_MAC(4,8)\
    IDCT32x32_PRUNED_V_MAC(5,8)\
    IDCT32x32_PRUNED_V_MAC(6,8)\
    IDCT32x32_PRUNED_V_MAC(7,8)


#define IDCT16x16_PRUNED_V_MAC(maxx,num_cg)\
void FUNC(emt_idct_16x16_##maxx##_0_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[16];                                                             \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                                  \
    DECL_0()                                                                   \
    IN_LOOP_LOAD_V(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,4)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,4)\
    }                                                                          \
void FUNC(emt_idct_16x16_##maxx##_1_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[16];                                                             \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,4)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,4)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,4)\
    }                                                                          \
void FUNC(emt_idct_16x16_##maxx##_2_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[16];                                                             \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,4)                                        \
    IN_LOOP_MULT_V(i,j,2,4)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,4)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,4)\
    }                                                                          \
void FUNC(emt_idct_16x16_##maxx##_3_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[16];                                                             \
    for(i = 0; i < 4; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,4)                                        \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,4)                                        \
    IN_LOOP_MULT_V(i,j,2,4)                                        \
    IN_LOOP_MULT_V(i,j,3,4)                                        \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,4)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,4)\
    }

#define IDCT16X16_PRUNED_V()\
    IDCT16x16_PRUNED_V_MAC(0,4)\
    IDCT16x16_PRUNED_V_MAC(1,4)\
    IDCT16x16_PRUNED_V_MAC(2,4)\
    IDCT16x16_PRUNED_V_MAC(3,4)


#define IDCT8X8_PRUNED_V_MAC( maxx,num_cg)              \
void FUNC(emt_idct_8x8_##maxx##_0_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[4];                                                             \
    for(i = 0; i < 2; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL_0()                                                                   \
    IN_LOOP_LOAD_V(i,j,0,2)                                         \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,2)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,2)\
    }                                                                          \
void FUNC(emt_idct_8x8_##maxx##_1_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    int i,j;                                                                   \
    __m256i CG[4];                                                             \
    for(i = 0; i < 2; i++){                                                    \
    for (j = 0; j < maxx + 1 ; j++){                                           \
    DECL()                                                                     \
    IN_LOOP_LOAD_V(i,j,0,2)                                         \
    CORE_4x4_MULT(src_1_0,src_2_0,x0_tmp,x8_tmp)                               \
    IN_LOOP_MULT_V(i,j,1,2)                                         \
    SCALE_AND_PACK(x0_tmp,x8_tmp,V)                                            \
    IN_LOOP_STORE_V(i,j,k,2)                                     \
    }                                                                          \
    }                                                                          \
    DO_H(maxx,2)\
    }

#define IDCT8X8_PRUNED_V()\
    IDCT8X8_PRUNED_V_MAC(0,2)\
    IDCT8X8_PRUNED_V_MAC(1,2)\



#define IDCT4x4_PRUNED_MAC(maxx)                            \
void FUNC(emt_idct_4x4_##maxx##_0_v_avx2)(int16_t *restrict src, int16_t *restrict dst, const int16_t *restrict  dtt_matrix_v, const int16_t *restrict  dtt_matrix_h)\
 {                                                                             \
    __m256i x0,x4,x8,x12,src_1_0, src_2_0, src_2_1,result_1,result_2;          \
    src_1_0 = _mm256_load_si256((__m256i *) dtt_matrix_v);                     \
    src_2_0 = _mm256_load_si256((__m256i*) src);                               \
    CORE_4x4_MULT(src_1_0,src_2_0,result_1,result_2)                           \
    SCALE_AND_PACK(result_1,result_2,V)                                        \
    src_2_0 = _mm256_load_si256((__m256i*) dtt_matrix_h);                      \
    CORE_4x4_MULT(result_1,src_2_0,result_1,result_2)                          \
    SCALE_AND_PACK(result_1,result_2,H)                                        \
    _mm256_store_si256((__m256i*)dst, result_1);                               \
    }

#define IDCT4X4_PRUNED_V()\
    IDCT4x4_PRUNED_MAC(0)\


//______________________________________________________________________________
//

#undef BIT_DEPTH
#define BIT_DEPTH 10

 IDCT4X4_PRUNED_V()
// IDCT4X4_PRUNED_H()
// IDCT4X4_PRUNED_V(DST_VII,VII)
// IDCT4X4_PRUNED_H(DST_VII,VII)
// IDCT4X4_PRUNED_V(DCT_VIII,VIII)
// IDCT4X4_PRUNED_H(DCT_VIII,VIII)
// IDCT4X4_PRUNED_V(DCT_V,V)
// IDCT4X4_PRUNED_H(DCT_V,V)
// IDCT4X4_PRUNED_V(DST_I,I)
// IDCT4X4_PRUNED_H(DST_I,I)

 IDCT8X8_PRUNED_V()
// IDCT8X8_PRUNED_H()
// IDCT8X8_PRUNED_V(DST_VII,VII)
// IDCT8X8_PRUNED_H(DST_VII,VII)
// IDCT8X8_PRUNED_V(DCT_VIII,VIII)
// IDCT8X8_PRUNED_H(DCT_VIII,VIII)
// IDCT8X8_PRUNED_V(DCT_V,V)
// IDCT8X8_PRUNED_H(DCT_V,V)
// IDCT8X8_PRUNED_V(DST_I,I)
// IDCT8X8_PRUNED_H(DST_I,I)

 IDCT16X16_PRUNED_V()
// IDCT16X16_PRUNED_H()
// IDCT16X16_PRUNED_V(DST_VII,VII)
// IDCT16X16_PRUNED_H(DST_VII,VII)
// IDCT16X16_PRUNED_V(DCT_VIII,VIII)
// IDCT16X16_PRUNED_H(DCT_VIII,VIII)
// IDCT16X16_PRUNED_V(DCT_V,V)
// IDCT16X16_PRUNED_H(DCT_V,V)
// IDCT16X16_PRUNED_V(DST_I,I)
// IDCT16X16_PRUNED_H(DST_I,I)

 IDCT32x32_PRUNED_V()
// IDCT32x32_PRUNED_H()
// IDCT32x32_PRUNED_V(DST_VII,VII)
// IDCT32x32_PRUNED_H(DST_VII,VII)
// IDCT32x32_PRUNED_V(DCT_VIII,VIII)
// IDCT32x32_PRUNED_H(DCT_VIII,VIII)
// IDCT32x32_PRUNED_V(DCT_V,V)
// IDCT32x32_PRUNED_H(DCT_V,V)
// IDCT32x32_PRUNED_V(DST_I,I)
// IDCT32x32_PRUNED_H(DST_I,I)

#undef BIT_DEPTH

#define BIT_DEPTH 8

 IDCT4X4_PRUNED_V()
// IDCT4X4_PRUNED_H()
// IDCT4X4_PRUNED_V(DST_VII,VII)
// IDCT4X4_PRUNED_H(DST_VII,VII)
// IDCT4X4_PRUNED_V(DCT_VIII,VIII)
// IDCT4X4_PRUNED_H(DCT_VIII,VIII)
// IDCT4X4_PRUNED_V(DCT_V,V)
// IDCT4X4_PRUNED_H(DCT_V,V)
// IDCT4X4_PRUNED_V(DST_I,I)
// IDCT4X4_PRUNED_H(DST_I,I)

 IDCT8X8_PRUNED_V()
// IDCT8X8_PRUNED_H()
// IDCT8X8_PRUNED_V(DST_VII,VII)
// IDCT8X8_PRUNED_H(DST_VII,VII)
// IDCT8X8_PRUNED_V(DCT_VIII,VIII)
// IDCT8X8_PRUNED_H(DCT_VIII,VIII)
// IDCT8X8_PRUNED_V(DCT_V,V)
// IDCT8X8_PRUNED_H(DCT_V,V)
// IDCT8X8_PRUNED_V(DST_I,I)
// IDCT8X8_PRUNED_H(DST_I,I)

 IDCT16X16_PRUNED_V()
// IDCT16X16_PRUNED_H()
// IDCT16X16_PRUNED_V(DST_VII,VII)
// IDCT16X16_PRUNED_H(DST_VII,VII)
// IDCT16X16_PRUNED_V(DCT_VIII,VIII)
// IDCT16X16_PRUNED_H(DCT_VIII,VIII)
// IDCT16X16_PRUNED_V(DCT_V,V)
// IDCT16X16_PRUNED_H(DCT_V,V)
// IDCT16X16_PRUNED_V(DST_I,I)
// IDCT16X16_PRUNED_H(DST_I,I)

 IDCT32x32_PRUNED_V()
// IDCT32x32_PRUNED_H()
// IDCT32x32_PRUNED_V(DST_VII,VII)
// IDCT32x32_PRUNED_H(DST_VII,VII)
// IDCT32x32_PRUNED_V(DCT_VIII,VIII)
// IDCT32x32_PRUNED_H(DCT_VIII,VIII)
// IDCT32x32_PRUNED_V(DCT_V,V)
// IDCT32x32_PRUNED_H(DCT_V,V)
// IDCT32x32_PRUNED_V(DST_I,I)
// IDCT32x32_PRUNED_H(DST_I,I)

#undef BIT_DEPTH
/*
#define IDCT8X8_V2(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_8x8_v_avx2)(int16_t *restrict src, int16_t * restrict dst)\
{                                                                              \
    int i,j,k;                                                                 \
    __m256i x0,x4,x8,x12,_src2;                                                \
    for(i = 0; i < 2; i++){                                                    \
        for (j = 0; j < 2; j++){                                               \
            __m256i x0_tmp = _mm256_setzero_si256();                           \
            __m256i x8_tmp = _mm256_setzero_si256();                           \
            for (k = 0; k < 2; k++ ){                                          \
                __m256i _src =_mm256_load_si256((__m256i *)&src[16*(2*k+i)]);  \
                __m256i dct_matrix = _mm256_load_si256((__m256i *) TR_##DCT_type##_8x8_per_CG[2*j+k]);\
                const __m256i _D0 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(0,0,5,5,1,1,4,4)),0b01010000);\
                const __m256i _D1 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(0,0,5,5,1,1,4,4)),0b11111010);\
                const __m256i _D2 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(2,2,7,7,3,3,6,6)),0b01010000);\
                const __m256i _D3 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(2,2,7,7,3,3,6,6)),0b11111010);\
                x0    = _mm256_permute4x64_epi64(_src,0b10110001);             \
                _src  = _mm256_unpacklo_epi16(_src,x0);                        \
                _src2 = _mm256_permute4x64_epi64(_src,0b01001110);             \
                x0  = _mm256_madd_epi16(_src,  _D0);                           \
                x4  = _mm256_madd_epi16(_src2, _D1);                           \
                x8  = _mm256_madd_epi16(_src,  _D2);                           \
                x12 = _mm256_madd_epi16(_src2, _D3);                           \
                x0 =  _mm256_add_epi32(x0,x4);                                 \
                x8 =  _mm256_add_epi32(x8,x12);                                \
                x0_tmp = _mm256_add_epi32(x0,x0_tmp);                          \
                x8_tmp = _mm256_add_epi32(x8,x8_tmp);                          \
            }                                                                  \
            x0 =  _mm256_add_epi32(x0_tmp,_mm256_set1_epi32(ADD_EMT_V));       \
            x8 =  _mm256_add_epi32(x8_tmp,_mm256_set1_epi32(ADD_EMT_V));       \
            x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V);                           \
            x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V);                           \
            x0 =  _mm256_packs_epi32(x0,x8);                                   \
            _mm256_store_si256((__m256i *)&dst[(2*j+i)*16],x0);                \
        }                                                                      \
    }                                                                          \
}                                                                              \


#define IDCT8X8_H2(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_8x8_h_avx2)(int16_t * restrict src, int16_t * restrict dst)\
{                                                                              \
    int i,j,k;                                                                 \
    __m256i x0,x4,x8,x12;                                                      \
    for(i = 0; i < 2; i++){                                                    \
        for (j = 0; j < 2; j++){                                               \
            __m256i x0_tmp = _mm256_setzero_si256();                           \
            __m256i x8_tmp = _mm256_setzero_si256();                           \
            for (k = 0; k < 2 ; k++ ){      \
                __m256i _src =_mm256_load_si256((__m256i *)&src[16*(2*i+k)]);  \
                __m256i dct_matrix = _mm256_load_si256((__m256i *) TR_##DCT_type##_8x8_per_CG[2*j+k]);\
                __m256i _D0 =_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(0,2,5,7,0,2,5,7));\
                __m256i _D2 =_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(1,3,4,6,1,3,4,6));\
                x0 = _mm256_unpacklo_epi32(_src, _src);                        \
                x4 = _mm256_permute4x64_epi64(x0, 0b10110001);                 \
                x8 = _mm256_unpackhi_epi32(_src, _src);                        \
                x12 = _mm256_permute4x64_epi64(x8, 0b10110001);                \
                x0  = _mm256_madd_epi16(x0,_D0);                               \
                x4  = _mm256_madd_epi16(x4,_D2);                               \
                x8  = _mm256_madd_epi16(x8,_D0);                               \
                x12 = _mm256_madd_epi16(x12,_D2);                              \
                x0 = _mm256_add_epi32(x0,x4);                                  \
                x8 = _mm256_add_epi32(x8,x12);                                 \
                x0_tmp = _mm256_add_epi32(x0,x0_tmp);                          \
                x8_tmp = _mm256_add_epi32(x8,x8_tmp);                          \
            }                                                                  \
            x0 =  _mm256_add_epi32(x0_tmp,_mm256_set1_epi32(ADD_EMT_H));       \
            x8 =  _mm256_add_epi32(x8_tmp,_mm256_set1_epi32(ADD_EMT_H));       \
            x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_H);                           \
            x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_H);                           \
            x0 = _mm256_packs_epi32(x0,x8);                                    \
    ((int64_t *)dst)[2*4*i+j] = _mm256_extract_epi64(x0,0); \
    ((int64_t *)dst)[2*(4*i+1)+j] = _mm256_extract_epi64(x0,1); \
    ((int64_t *)dst)[2*(4*i+2)+j] = _mm256_extract_epi64(x0,2); \
    ((int64_t *)dst)[2*(4*i+3)+j] = _mm256_extract_epi64(x0,3); \
       }                                                                       \
    }                                                                          \
}                                                                              \

#define IDCT16X16_V2(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_16x16_v_avx2)(int16_t *restrict src, int16_t *restrict dst)\
{                                                                             \
    int i,j,k;                                                                 \
    __m256i x0,x4,x8,x12,_src2;                                                \
    for(i = 0; i < 4; i++){                                                    \
        for (j = 0; j < 4; j++){                                               \
            __m256i x0_tmp = _mm256_setzero_si256();                           \
            __m256i x8_tmp = _mm256_setzero_si256();                           \
            for (k = 0; k < 4; k++ ){                                          \
                __m256i _src =_mm256_load_si256((__m256i *)&src[16*(4*k+i)]);  \
                __m256i dct_matrix = _mm256_load_si256((__m256i *) TR_##DCT_type##_16x16_per_CG[4*j+k]);\
                const __m256i _D0 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(0,0,5,5,1,1,4,4)),0b01010000);\
                const __m256i _D1 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(0,0,5,5,1,1,4,4)),0b11111010);\
                const __m256i _D2 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(2,2,7,7,3,3,6,6)),0b01010000);\
                const __m256i _D3 =_mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(2,2,7,7,3,3,6,6)),0b11111010);\
                x0    = _mm256_permute4x64_epi64(_src,0b10110001);             \
                _src  = _mm256_unpacklo_epi16(_src,x0);                        \
                _src2 = _mm256_permute4x64_epi64(_src,0b01001110);             \
                x0  = _mm256_madd_epi16(_src,  _D0);                           \
                x4  = _mm256_madd_epi16(_src2, _D1);                           \
                x8  = _mm256_madd_epi16(_src,  _D2);                           \
                x12 = _mm256_madd_epi16(_src2, _D3);                           \
                x0 =  _mm256_add_epi32(x0,x4);                                 \
                x8 =  _mm256_add_epi32(x8,x12);                                \
                x0_tmp = _mm256_add_epi32(x0,x0_tmp);                          \
                x8_tmp = _mm256_add_epi32(x8,x8_tmp);                          \
            }                                                                  \
            x0 =  _mm256_add_epi32(x0_tmp,_mm256_set1_epi32(ADD_EMT_V));       \
            x8 =  _mm256_add_epi32(x8_tmp,_mm256_set1_epi32(ADD_EMT_V));       \
            x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V);                           \
            x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V);                           \
            x0 =  _mm256_packs_epi32(x0,x8);                                   \
            _mm256_store_si256((__m256i *)&dst[(4*j+i)*16],x0);                \
        }                                                                      \
    }                                                                          \
}                                                                              \


#define IDCT16X16_H2(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_16x16_h_avx2)(int16_t * restrict src, int16_t * restrict dst)\
{                                                                              \
    int i,j,k;                                                                 \
    __m256i x0,x4,x8,x12;                                                      \
    for(i = 0; i < 4; i++){                                                    \
        for (j = 0; j < 4; j++){                                               \
            __m256i x0_tmp = _mm256_setzero_si256();                           \
            __m256i x8_tmp = _mm256_setzero_si256();                           \
            for (k = 0; k < 4 ; k++ ){                                         \
                __m256i _src =_mm256_load_si256((__m256i *)&src[16*(4*i+k)]);  \
                __m256i dct_matrix = _mm256_load_si256((__m256i *) TR_##DCT_type##_16x16_per_CG[4*j+k]);\
                __m256i _D0 =_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(0,2,5,7,0,2,5,7));\
                __m256i _D2 =_mm256_permutevar8x32_epi32(dct_matrix,_mm256_setr_epi32(1,3,4,6,1,3,4,6));\
                x0 = _mm256_unpacklo_epi32(_src, _src);                        \
                x4 = _mm256_permute4x64_epi64(x0, 0b10110001);                 \
                x8 = _mm256_unpackhi_epi32(_src, _src);                        \
                x12 = _mm256_permute4x64_epi64(x8, 0b10110001);                \
                x0  = _mm256_madd_epi16(x0,_D0);                               \
                x4  = _mm256_madd_epi16(x4,_D2);                               \
                x8  = _mm256_madd_epi16(x8,_D0);                               \
                x12 = _mm256_madd_epi16(x12,_D2);                              \
                x0 = _mm256_add_epi32(x0,x4);                                  \
                x8 = _mm256_add_epi32(x8,x12);                                 \
                x0_tmp = _mm256_add_epi32(x0,x0_tmp);                          \
                x8_tmp = _mm256_add_epi32(x8,x8_tmp);                          \
            }                                                                  \
            x0 =  _mm256_add_epi32(x0_tmp,_mm256_set1_epi32(ADD_EMT_H));       \
            x8 =  _mm256_add_epi32(x8_tmp,_mm256_set1_epi32(ADD_EMT_H));       \
            x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_H);                           \
            x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_H);                           \
            x0 = _mm256_packs_epi32(x0,x8);                                    \
    ((int64_t *)dst)[4*4*i+j] = _mm256_extract_epi64(x0,0);                    \
    ((int64_t *)dst)[4*(4*i+1)+j] = _mm256_extract_epi64(x0,1);                \
    ((int64_t *)dst)[4*(4*i+2)+j] = _mm256_extract_epi64(x0,2);                \
    ((int64_t *)dst)[4*(4*i+3)+j] = _mm256_extract_epi64(x0,3);                \
       }                                                                       \
    }                                                                          \
}                                                                              \

#define IDCT32x32_H2(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_32x32_h_avx2)(int16_t * restrict src, int16_t * restrict dst)\
{                                                                              \
    int i,j,k;                                                                 \
    __m256i x0,x4,x8,x12,src_1, src_2_0, src_2_1, result;                      \
    for(i = 0; i < 8; i++){                                                    \
        for (j = 0; j < 8; j++){                                               \
            __m256i x0_tmp = _mm256_setzero_si256();                           \
            __m256i x8_tmp = _mm256_setzero_si256();                           \
            for (k = 0; k < 8; k++ ){                                          \
                src_1 = _mm256_load_si256((__m256i*) &src[16*(8*i+k)]);        \
                src_2_0 = _mm256_load_si256((__m256i *) DCT_type##_32x32_per_CG[8*k+j]);\
                                                                               \
                src_2_0 = _mm256_unpacklo_epi16(src_2_0, _mm256_srli_si256(src_2_0, 8));\
                src_2_1 = _mm256_permute2x128_si256(src_2_0, src_2_0, 1 + 16); \
                src_2_0 = _mm256_permute2x128_si256(src_2_0, src_2_0, 0);      \
                                                                               \
                x0  = _mm256_shuffle_epi32(src_1, 0);                          \
                x4  = _mm256_shuffle_epi32(src_1, 1 + 4 + 16 + 64);            \
                x8  = _mm256_shuffle_epi32(src_1, 2 + 8 + 32 + 128);           \
                x12 = _mm256_shuffle_epi32(src_1, 3 + 12 + 48 + 192);          \
                                                                               \
                x0  = _mm256_madd_epi16(x0,  src_2_0);                         \
                x4  = _mm256_madd_epi16(x4,  src_2_1);                         \
                x8  = _mm256_madd_epi16(x8,  src_2_0);                         \
                x12 = _mm256_madd_epi16(x12, src_2_1);                         \
                x12 =    _mm256_add_epi32(x8, x12);                            \
                result = _mm256_add_epi32(x0, x4);                             \
                x0_tmp = _mm256_add_epi32(result,x0_tmp);                      \
                x8_tmp = _mm256_add_epi32(x12,x8_tmp);                         \
            }                                                                  \
            x0 =  _mm256_add_epi32(x0_tmp, _mm256_set1_epi32(ADD_EMT_H));      \
            x8 =  _mm256_add_epi32(x8_tmp, _mm256_set1_epi32(ADD_EMT_H));      \
            x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_H);                           \
            x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_H);                           \
            x0 = _mm256_packs_epi32(x0,x8);                                    \
    ((int64_t *)dst)[8*4*i+j] = _mm256_extract_epi64(x0,0); \
    ((int64_t *)dst)[8*(4*i+1)+j] = _mm256_extract_epi64(x0,1); \
    ((int64_t *)dst)[8*(4*i+2)+j] = _mm256_extract_epi64(x0,2); \
    ((int64_t *)dst)[8*(4*i+3)+j] = _mm256_extract_epi64(x0,3); \
       }                                                                       \
    }                                                                          \
}                                                                              \

#define IDCT8X8_V3(DCT_type,DCT_num)\
void FUNC(emt_idct_##DCT_num##_8x8_v_avx2)(int16_t * restrict src, int16_t * restrict dst)\
{\
    register const __m256i perm1 =_mm256_setr_epi32(0,0,5,5,1,1,4,4);\
    register const __m256i perm2 =_mm256_setr_epi32(2,2,7,7,3,3,6,6);\
    register const __m256i cg_dct_matrix_0 = _mm256_load_si256((__m256i *)TR_##DCT_type##_8x8_per_CG[0]);\
    register const __m256i cg_dct_matrix_1 = _mm256_load_si256((__m256i *)TR_##DCT_type##_8x8_per_CG[1]);\
    register const __m256i cg_dct_matrix_2 = _mm256_load_si256((__m256i *)TR_##DCT_type##_8x8_per_CG[2]);\
    register const __m256i cg_dct_matrix_3 = _mm256_load_si256((__m256i *)TR_##DCT_type##_8x8_per_CG[3]);\
        register __m256i cg_src_0, cg_src_2;\
        register __m256i x0, x4, x8, x12, x0_tmp, x8_tmp, cg_src_0_2, cg_src_2_2;\
        cg_src_0 = _mm256_load_si256((__m256i *)&src[0]);\
        cg_src_2 = _mm256_load_si256((__m256i *)&src[32]);\
            \
            x0 = _mm256_permute4x64_epi64(cg_src_0,0b10110001);\
            cg_src_0   = _mm256_unpacklo_epi16(cg_src_0,x0);\
            cg_src_0_2 = _mm256_permute4x64_epi64(cg_src_0,0b01001110);\
                x0  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm1),0b01010000));\
                x4  = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm1),0b11111010));\
                x8  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm2),0b01010000));\
                x12 = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm2),0b11111010));\
                x0_tmp =  _mm256_add_epi32(x0,x4);\
                x8_tmp =  _mm256_add_epi32(x8,x12);\
            \
            x8    = _mm256_permute4x64_epi64(cg_src_2,0b10110001);\
            cg_src_2   = _mm256_unpacklo_epi16(cg_src_2,x8);\
            cg_src_2_2 = _mm256_permute4x64_epi64(cg_src_2,0b01001110);\
                x0  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm1),0b01010000));\
                x4  = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm1),0b11111010));\
                x8  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm2),0b01010000));\
                x12 = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm2),0b11111010));\
                x0 =  _mm256_add_epi32(x0,x4);\
                x8 =  _mm256_add_epi32(x8,x12);\
                x0_tmp = _mm256_add_epi32(x0,x0_tmp);\
                x8_tmp = _mm256_add_epi32(x8,x8_tmp);\
            \
            x0 =  _mm256_add_epi32(x0_tmp, _mm256_set1_epi32(ADD_EMT_V));\
            x8 =  _mm256_add_epi32(x8_tmp, _mm256_set1_epi32(ADD_EMT_V));\
            x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V);\
            x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V);\
            x0 = _mm256_packs_epi32(x0,x8);\
            _mm256_store_si256((__m256i *)&dst[0],x0);\
        \
        \
                x0  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm1),0b01010000));\
                x4  = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm1),0b11111010));\
                x8  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm2),0b01010000));\
                x12 = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm2),0b11111010));\
                x0_tmp =  _mm256_add_epi32(x0,x4);\
                x8_tmp =  _mm256_add_epi32(x8,x12);\
            \
            \
                x0  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm1),0b01010000));\
                x4  = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm1),0b11111010));\
                x8  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm2),0b01010000));\
                x12 = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm2),0b11111010));\
                x0 =  _mm256_add_epi32(x0,x4);\
                x8 =  _mm256_add_epi32(x8,x12);\
                x0_tmp = _mm256_add_epi32(x0,x0_tmp);\
                x8_tmp = _mm256_add_epi32(x8,x8_tmp);\
            \
            x0 =  _mm256_add_epi32(x0_tmp, _mm256_set1_epi32(ADD_EMT_V));\
            x8 =  _mm256_add_epi32(x8_tmp, _mm256_set1_epi32(ADD_EMT_V));\
            x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V);\
            x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V);\
            x0 = _mm256_packs_epi32(x0,x8);\
            _mm256_store_si256((__m256i *)&dst[32],x0);\
    cg_src_0 = _mm256_load_si256((__m256i *)&src[16]);\
    cg_src_2 = _mm256_load_si256((__m256i *)&src[48]);\
        \
        x0 = _mm256_permute4x64_epi64(cg_src_0,0b10110001);\
        cg_src_0   = _mm256_unpacklo_epi16(cg_src_0,x0);\
        cg_src_0_2 = _mm256_permute4x64_epi64(cg_src_0,0b01001110);\
            x0  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm1),0b01010000));\
            x4  = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm1),0b11111010));\
            x8  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm2),0b01010000));\
            x12 = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_0,perm2),0b11111010));\
            x0_tmp =  _mm256_add_epi32(x0,x4);\
            x8_tmp =  _mm256_add_epi32(x8,x12);\
        \
        x8    = _mm256_permute4x64_epi64(cg_src_2,0b10110001);\
        cg_src_2   = _mm256_unpacklo_epi16(cg_src_2,x8);\
        cg_src_2_2 = _mm256_permute4x64_epi64(cg_src_2,0b01001110);\
            x0  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm1),0b01010000));\
            x4  = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm1),0b11111010));\
            x8  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm2),0b01010000));\
            x12 = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_1,perm2),0b11111010));\
            x0 =  _mm256_add_epi32(x0,x4);\
            x8 =  _mm256_add_epi32(x8,x12);\
            x0_tmp = _mm256_add_epi32(x0,x0_tmp);\
            x8_tmp = _mm256_add_epi32(x8,x8_tmp);\
        \
        x0 =  _mm256_add_epi32(x0_tmp, _mm256_set1_epi32(ADD_EMT_V));\
        x8 =  _mm256_add_epi32(x8_tmp, _mm256_set1_epi32(ADD_EMT_V));\
        x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V);\
        x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V);\
        x0 = _mm256_packs_epi32(x0,x8);\
        _mm256_store_si256((__m256i *)&dst[16],x0);\
    \
    \
            x0  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm1),0b01010000));\
            x4  = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm1),0b11111010));\
            x8  = _mm256_madd_epi16(cg_src_0,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm2),0b01010000));\
            x12 = _mm256_madd_epi16(cg_src_0_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_2,perm2),0b11111010));\
            x0_tmp =  _mm256_add_epi32(x0,x4);\
            x8_tmp =  _mm256_add_epi32(x8,x12);\
        \
        \
            x0  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm1),0b01010000));\
            x4  = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm1),0b11111010));\
            x8  = _mm256_madd_epi16(cg_src_2,   _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm2),0b01010000));\
            x12 = _mm256_madd_epi16(cg_src_2_2, _mm256_permute4x64_epi64(_mm256_permutevar8x32_epi32(cg_dct_matrix_3,perm2),0b11111010));\
            x0 =  _mm256_add_epi32(x0,x4);\
            x8 =  _mm256_add_epi32(x8,x12);\
            x0_tmp = _mm256_add_epi32(x0,x0_tmp);\
            x8_tmp = _mm256_add_epi32(x8,x8_tmp);\
        \
        x0 =  _mm256_add_epi32(x0_tmp, _mm256_set1_epi32(ADD_EMT_V));\
        x8 =  _mm256_add_epi32(x8_tmp, _mm256_set1_epi32(ADD_EMT_V));\
        x0 =  _mm256_srai_epi32(x0,SHIFT_EMT_V);\
        x8 =  _mm256_srai_epi32(x8,SHIFT_EMT_V);\
        x0 = _mm256_packs_epi32(x0,x8);\
        _mm256_store_si256((__m256i *)&dst[48],x0);\
    \
}\

*/
