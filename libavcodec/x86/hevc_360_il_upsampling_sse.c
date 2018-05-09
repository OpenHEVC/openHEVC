#include "stdint.h"
#include "smmintrin.h"
#include "emmintrin.h"

#include "hevcdsp.h"

#if OHCONFIG_UPSAMPLING360
void ohevc_upsample_360_il_block_8_sse(uint8_t *restrict src, uint8_t *restrict dst, int *restrict offset_bl_lut, int16_t *restrict weight_idx_lut, int16_t **restrict weight_lut_luma, int bl_stride, int el_stride ){
     const int offset_x = SHVC360_LANCZOS_PARAM_LUMA << 1;
     const int round_shift = S_INTERPOLATE_PrecisionBD;
     register const __m128i round_add = _mm_set1_epi32(1 << (round_shift - 1));
     register const __m128i mask = _mm_set_epi16(0x0000,0x0000,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff);
     register const __m128i zeros = _mm_setzero_si128();
     __m128i line[8];
     __m128i dst_tmp[8];
     int *offset_bl = offset_bl_lut;
     int16_t *weight_idx = weight_idx_lut;

     for (int i = 0 ; i < 64 ; i++) {
         __m128i *_dst = (__m128i*) dst;
         for (int l = 0 ; l < 8 ; l++) {
             for (int k = 0 ; k < 8; k++,weight_idx++,offset_bl++){
                 int16_t *weight_lut = weight_lut_luma[*weight_idx];
                 uint8_t *pix = src + *offset_bl;
                 __m128i l0 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)pix)              ,zeros);
                 __m128i l1 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride])  ,zeros);
                 __m128i l2 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride*2]),zeros);
                 __m128i l3 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride*3]),zeros);
                 __m128i l4 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride*4]),zeros);
                 __m128i l5 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride*5]),zeros);

                 __m128i w0 = _mm_loadu_si128((__m128i*)weight_lut);
                 __m128i w1 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x]);
                 __m128i w2 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*2]);
                 __m128i w3 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*3]);
                 __m128i w4 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*4]);
                 __m128i w5 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*5]);

                 l0 = _mm_madd_epi16(l0,w0);
                 l1 = _mm_madd_epi16(l1,w1);
                 l2 = _mm_madd_epi16(l2,w2);
                 l3 = _mm_madd_epi16(l3,w3);
                 l4 = _mm_madd_epi16(l4,w4);
                 l5 = _mm_madd_epi16(l5,w5);

                 l0 = _mm_add_epi32(l0,l1);
                 l1 = _mm_add_epi32(l2,l3);
                 l2 = _mm_add_epi32(l4,l5);

                 l0 = _mm_add_epi32(l0,l1);
                 l0 = _mm_add_epi32(l0,l2);

                 dst_tmp[k] = _mm_and_si128(l0,mask);
             }
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);
             dst_tmp[2] = _mm_hadd_epi32(dst_tmp[4],dst_tmp[5]);
             dst_tmp[3] = _mm_hadd_epi32(dst_tmp[6],dst_tmp[7]);
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);

             //round
             dst_tmp[0] = _mm_add_epi32(dst_tmp[0],round_add);
             dst_tmp[1] = _mm_add_epi32(dst_tmp[1],round_add);
             dst_tmp[0] = _mm_srai_epi32(dst_tmp[0],round_shift);
             dst_tmp[1] = _mm_srai_epi32(dst_tmp[1],round_shift);

             line[l] = _mm_packs_epi32(dst_tmp[0],dst_tmp[1]);
         }
         _mm_store_si128(   _dst , _mm_packus_epi16(line[0],line[1]));
         _mm_store_si128( ++_dst , _mm_packus_epi16(line[2],line[3]));
         _mm_store_si128( ++_dst , _mm_packus_epi16(line[4],line[5]));
         _mm_store_si128( ++_dst , _mm_packus_epi16(line[6],line[7]));
         dst += el_stride;
         offset_bl  += el_stride - 64;
         weight_idx += el_stride - 64;
     }

 }

void ohevc_upsample_360_il_block_chroma_8_sse(uint8_t *restrict src, uint8_t *restrict dst, int *restrict offset_bl_lut, int16_t *restrict weight_idx_lut, int16_t **restrict weight_lut_chroma, int bl_stride, int el_stride ){

     const int round_shift = S_INTERPOLATE_PrecisionBD;
     const int offset_x = SHVC360_LANCZOS_PARAM_CHROMA << 1;
     register const __m128i round_add = _mm_set1_epi32(1 << (round_shift - 1));
     register const __m128i zeros = _mm_setzero_si128();
     __m128i dst_tmp[8], line[4];
     int *offset_bl = offset_bl_lut;
     int16_t *weight_idx = weight_idx_lut;

     for (int i= 0 ; i < 32 ; i++) {
         __m128i *_dst = (__m128i*) dst;
         for (int  l = 0 ; l < 4 ; l++) {
             for (int k = 0 ; k < 8; k++, offset_bl++, weight_idx++){
                 int16_t *weight = weight_lut_chroma[*weight_idx];
                 uint8_t *pix = src + *offset_bl;
                 __m128i l0 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)pix)              ,zeros);
                 __m128i l1 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride])  ,zeros);
                 __m128i l2 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride*2]),zeros);
                 __m128i l3 = _mm_unpacklo_epi8(_mm_loadu_si128((__m128i*)&pix[bl_stride*3]),zeros);
                 __m128i w0 = _mm_loadu_si128((__m128i*)weight);
                 __m128i w2 = _mm_loadu_si128((__m128i*)&weight[offset_x*2]);

                 l0 = _mm_unpacklo_epi64(l0,l1);
                 l2 = _mm_unpacklo_epi64(l2,l3);

                 l0 = _mm_madd_epi16(l0,w0);
                 l2 = _mm_madd_epi16(l2,w2);

                 dst_tmp[k] =  _mm_add_epi32(l0,l2);;
             }
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);
             dst_tmp[2] = _mm_hadd_epi32(dst_tmp[4],dst_tmp[5]);
             dst_tmp[3] = _mm_hadd_epi32(dst_tmp[6],dst_tmp[7]);
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);
             //round
             dst_tmp[0] = _mm_add_epi32(dst_tmp[0],round_add);
             dst_tmp[1] = _mm_add_epi32(dst_tmp[1],round_add);
             dst_tmp[0] = _mm_srai_epi32(dst_tmp[0],round_shift);
             dst_tmp[1] = _mm_srai_epi32(dst_tmp[1],round_shift);

             line[l] = _mm_packs_epi32(dst_tmp[0],dst_tmp[1]);
         }
         _mm_store_si128(   _dst , _mm_packus_epi16(line[0],line[1]));
         _mm_store_si128( ++_dst , _mm_packus_epi16(line[2],line[3]));
         dst += el_stride;
         offset_bl  += el_stride - 32;
         weight_idx += el_stride - 32;
     }
 }

void ohevc_upsample_360_il_block_10_sse(uint8_t *restrict src, uint8_t *restrict dst, int *restrict offset_bl_lut, int16_t *restrict weight_idx_lut, int16_t **restrict weight_lut_luma, int bl_stride, int el_stride ){
     const int offset_x = SHVC360_LANCZOS_PARAM_LUMA << 1;
     const int round_shift = S_INTERPOLATE_PrecisionBD;
     register const __m128i round_add = _mm_set1_epi32(1 << (round_shift - 1));
     register const __m128i mask = _mm_set_epi16(0x0000,0x0000,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff);
     __m128i line[8];
     __m128i dst_tmp[8];
     int *offset_bl = offset_bl_lut;
     int16_t *weight_idx = weight_idx_lut;

     for (int i = 0 ; i < 64 ; i++) {
         __m128i *_dst = (__m128i*) dst;
         for (int l = 0 ; l < 8 ; l++) {
             for (int k = 0 ; k < 8; k++,weight_idx++,offset_bl++){
                 int16_t *weight_lut = weight_lut_luma[*weight_idx];
                 uint16_t *pix = (uint16_t *)src + *offset_bl;
                 __m128i l0 = _mm_loadu_si128((__m128i*)pix);
                 __m128i l1 = _mm_loadu_si128((__m128i*)&pix[bl_stride]);
                 __m128i l2 = _mm_loadu_si128((__m128i*)&pix[bl_stride*2]);
                 __m128i l3 = _mm_loadu_si128((__m128i*)&pix[bl_stride*3]);
                 __m128i l4 = _mm_loadu_si128((__m128i*)&pix[bl_stride*4]);
                 __m128i l5 = _mm_loadu_si128((__m128i*)&pix[bl_stride*5]);

                 __m128i w0 = _mm_loadu_si128((__m128i*)weight_lut);
                 __m128i w1 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x]);
                 __m128i w2 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*2]);
                 __m128i w3 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*3]);
                 __m128i w4 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*4]);
                 __m128i w5 = _mm_loadu_si128((__m128i*)&weight_lut[offset_x*5]);

                 l0 = _mm_madd_epi16(l0,w0);
                 l1 = _mm_madd_epi16(l1,w1);
                 l2 = _mm_madd_epi16(l2,w2);
                 l3 = _mm_madd_epi16(l3,w3);
                 l4 = _mm_madd_epi16(l4,w4);
                 l5 = _mm_madd_epi16(l5,w5);

                 l0 = _mm_add_epi32(l0,l1);
                 l1 = _mm_add_epi32(l2,l3);
                 l2 = _mm_add_epi32(l4,l5);

                 l0 = _mm_add_epi32(l0,l1);
                 l0 = _mm_add_epi32(l0,l2);

                 dst_tmp[k] = _mm_and_si128(l0,mask);
             }
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);
             dst_tmp[2] = _mm_hadd_epi32(dst_tmp[4],dst_tmp[5]);
             dst_tmp[3] = _mm_hadd_epi32(dst_tmp[6],dst_tmp[7]);
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);

             //round
             dst_tmp[0] = _mm_add_epi32(dst_tmp[0],round_add);
             dst_tmp[1] = _mm_add_epi32(dst_tmp[1],round_add);
             dst_tmp[0] = _mm_srai_epi32(dst_tmp[0],round_shift);
             dst_tmp[1] = _mm_srai_epi32(dst_tmp[1],round_shift);

             line[l] = _mm_packus_epi32(dst_tmp[0],dst_tmp[1]);
         }
         _mm_store_si128(   _dst ,line[0]);
         _mm_store_si128( ++_dst ,line[1]);
         _mm_store_si128( ++_dst ,line[2]);
         _mm_store_si128( ++_dst ,line[3]);
         _mm_store_si128( ++_dst ,line[4]);
         _mm_store_si128( ++_dst ,line[5]);
         _mm_store_si128( ++_dst ,line[6]);
         _mm_store_si128( ++_dst ,line[7]);
         dst += el_stride*2;
         offset_bl  += el_stride - 64;
         weight_idx += el_stride - 64;
     }

 }

void ohevc_upsample_360_il_block_chroma_10_sse(uint8_t *restrict src, uint8_t *restrict dst, int *restrict offset_bl_lut, int16_t *restrict weight_idx_lut, int16_t **restrict weight_lut_chroma, int bl_stride, int el_stride ){

     const int round_shift = S_INTERPOLATE_PrecisionBD;
     const int offset_x = SHVC360_LANCZOS_PARAM_CHROMA << 1;
     register const __m128i round_add = _mm_set1_epi32(1 << (round_shift - 1));
     __m128i dst_tmp[8], line[4];
     int *offset_bl = offset_bl_lut;
     int16_t *weight_idx = weight_idx_lut;

     for (int i= 0 ; i < 32 ; i++) {
         __m128i *_dst = (__m128i*) dst;
         for (int  l = 0 ; l < 4 ; l++) {
             for (int k = 0 ; k < 8; k++, offset_bl++, weight_idx++){
                 int16_t *weight = weight_lut_chroma[*weight_idx];
                 uint16_t *pix = (uint16_t *)src + *offset_bl;
                 __m128i l0 = _mm_loadu_si128((__m128i*)pix);
                 __m128i l1 = _mm_loadu_si128((__m128i*)&pix[bl_stride]);
                 __m128i l2 = _mm_loadu_si128((__m128i*)&pix[bl_stride*2]);
                 __m128i l3 = _mm_loadu_si128((__m128i*)&pix[bl_stride*3]);
                 __m128i w0 = _mm_loadu_si128((__m128i*)weight);
                 __m128i w2 = _mm_loadu_si128((__m128i*)&weight[offset_x*2]);

                 l0 = _mm_unpacklo_epi64(l0,l1);
                 l2 = _mm_unpacklo_epi64(l2,l3);

                 l0 = _mm_madd_epi16(l0,w0);
                 l2 = _mm_madd_epi16(l2,w2);

                 dst_tmp[k] =  _mm_add_epi32(l0,l2);;
             }
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);
             dst_tmp[2] = _mm_hadd_epi32(dst_tmp[4],dst_tmp[5]);
             dst_tmp[3] = _mm_hadd_epi32(dst_tmp[6],dst_tmp[7]);
             dst_tmp[0] = _mm_hadd_epi32(dst_tmp[0],dst_tmp[1]);
             dst_tmp[1] = _mm_hadd_epi32(dst_tmp[2],dst_tmp[3]);
             //round
             dst_tmp[0] = _mm_add_epi32(dst_tmp[0],round_add);
             dst_tmp[1] = _mm_add_epi32(dst_tmp[1],round_add);
             dst_tmp[0] = _mm_srai_epi32(dst_tmp[0],round_shift);
             dst_tmp[1] = _mm_srai_epi32(dst_tmp[1],round_shift);

             line[l] = _mm_packus_epi32(dst_tmp[0],dst_tmp[1]);
         }
         _mm_store_si128(   _dst ,line[0]);
         _mm_store_si128( ++_dst ,line[1]);
         _mm_store_si128( ++_dst ,line[2]);
         _mm_store_si128( ++_dst ,line[3]);
         dst += el_stride*2;
         offset_bl  += el_stride - 32;
         weight_idx += el_stride - 32;
     }
 }
#endif
