#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevcdata.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcdsp.h"

#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

static void hevc_dequant4x4_sse4(int16_t *coeffs, int qp, int bit_dep) {
    __m128i c0, c1, f0, f1, c2, c3;
    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };
    int shift = bit_dep - 7;
    int scale = level_scale[qp % 6] << (qp / 6);
    int add = 1 << (shift - 1);
    //4x4 = 16 coeffs.
    
    f0 = _mm_set1_epi32(scale);
    
    f1 = _mm_set1_epi32(add);
    c0 = _mm_load_si128((__m128i *) &coeffs[0]); //loads 8 first values
    c2 = _mm_load_si128((__m128i *) &coeffs[8]); //loads 8 last values
    
    c1 = _mm_unpackhi_epi16(_mm_setzero_si128(), c0);
    c3 = _mm_unpackhi_epi16(_mm_setzero_si128(), c2);
    c0 = _mm_unpacklo_epi16(_mm_setzero_si128(), c0);
    c2 = _mm_unpacklo_epi16(_mm_setzero_si128(), c2);
    c0 = _mm_srai_epi32(c0, 16);
    c1 = _mm_srai_epi32(c1, 16);
    c2 = _mm_srai_epi32(c2, 16);
    c3 = _mm_srai_epi32(c3, 16);
    
    c0 = _mm_mullo_epi32(c0, f0);
    c1 = _mm_mullo_epi32(c1, f0);
    c2 = _mm_mullo_epi32(c2, f0);
    c3 = _mm_mullo_epi32(c3, f0);
    
    c0 = _mm_add_epi32(c0, f1);
    c1 = _mm_add_epi32(c1, f1);
    c2 = _mm_add_epi32(c2, f1);
    c3 = _mm_add_epi32(c3, f1);
    
    c0 = _mm_srai_epi32(c0, shift);
    c1 = _mm_srai_epi32(c1, shift);
    c2 = _mm_srai_epi32(c2, shift);
    c3 = _mm_srai_epi32(c3, shift);
    
    c0 = _mm_packs_epi32(c0, c1);
    c2 = _mm_packs_epi32(c2, c3);
    
    _mm_store_si128((__m128i *) &coeffs[0], c0);
    _mm_store_si128((__m128i *) &coeffs[8], c2);
}

static void hevc_dequant8x8_sse4(int16_t *coeffs, int qp, int bit_dep) {
    __m128i c0, c1, c2, c3, c4, c5, c6, c7, f0, f1;
    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };
    
    //TODO: scaling_list_enabled_flag support
    int shift = bit_dep - 6;
    int scale = level_scale[qp % 6] << (qp / 6);
    int add = 1 << (shift - 1);
    
    //8x8= 64 coeffs.
    f0 = _mm_set1_epi32(scale);
    f1 = _mm_set1_epi32(add);
    c0 = _mm_load_si128((__m128i *) &coeffs[0]); //loads 8 first values
    c2 = _mm_load_si128((__m128i *) &coeffs[8]);
    c4 = _mm_load_si128((__m128i *) &coeffs[16]);
    c6 = _mm_load_si128((__m128i *) &coeffs[24]);
    
    c1 = _mm_unpackhi_epi16(_mm_setzero_si128(), c0);
    c3 = _mm_unpackhi_epi16(_mm_setzero_si128(), c2);
    c5 = _mm_unpackhi_epi16(_mm_setzero_si128(), c4);
    c7 = _mm_unpackhi_epi16(_mm_setzero_si128(), c6);
    c0 = _mm_unpacklo_epi16(_mm_setzero_si128(), c0);
    c2 = _mm_unpacklo_epi16(_mm_setzero_si128(), c2);
    c4 = _mm_unpacklo_epi16(_mm_setzero_si128(), c4);
    c6 = _mm_unpacklo_epi16(_mm_setzero_si128(), c6);
    c0 = _mm_srai_epi32(c0, 16);
    c1 = _mm_srai_epi32(c1, 16);
    c2 = _mm_srai_epi32(c2, 16);
    c3 = _mm_srai_epi32(c3, 16);
    c4 = _mm_srai_epi32(c4, 16);
    c5 = _mm_srai_epi32(c5, 16);
    c6 = _mm_srai_epi32(c6, 16);
    c7 = _mm_srai_epi32(c7, 16);
    
    c0 = _mm_mullo_epi32(c0, f0);
    c1 = _mm_mullo_epi32(c1, f0);
    c2 = _mm_mullo_epi32(c2, f0);
    c3 = _mm_mullo_epi32(c3, f0);
    c4 = _mm_mullo_epi32(c4, f0);
    c5 = _mm_mullo_epi32(c5, f0);
    c6 = _mm_mullo_epi32(c6, f0);
    c7 = _mm_mullo_epi32(c7, f0);
    c0 = _mm_add_epi32(c0, f1);
    c1 = _mm_add_epi32(c1, f1);
    c2 = _mm_add_epi32(c2, f1);
    c3 = _mm_add_epi32(c3, f1);
    c4 = _mm_add_epi32(c4, f1);
    c5 = _mm_add_epi32(c5, f1);
    c6 = _mm_add_epi32(c6, f1);
    c7 = _mm_add_epi32(c7, f1);
    
    c0 = _mm_srai_epi32(c0, shift);
    c2 = _mm_srai_epi32(c2, shift);
    c4 = _mm_srai_epi32(c4, shift);
    c6 = _mm_srai_epi32(c6, shift);
    c1 = _mm_srai_epi32(c1, shift);
    c3 = _mm_srai_epi32(c3, shift);
    c5 = _mm_srai_epi32(c5, shift);
    c7 = _mm_srai_epi32(c7, shift);
    
    c0 = _mm_packs_epi32(c0, c1);
    c2 = _mm_packs_epi32(c2, c3);
    c4 = _mm_packs_epi32(c4, c5);
    c6 = _mm_packs_epi32(c6, c7);
    
    _mm_store_si128((__m128i *) &coeffs[0], c0);
    _mm_store_si128((__m128i *) &coeffs[8], c2);
    _mm_store_si128((__m128i *) &coeffs[16], c4);
    _mm_store_si128((__m128i *) &coeffs[24], c6);
    
    c0 = _mm_load_si128((__m128i *) &coeffs[32]);
    c2 = _mm_load_si128((__m128i *) &coeffs[40]);
    c4 = _mm_load_si128((__m128i *) &coeffs[48]);
    c6 = _mm_load_si128((__m128i *) &coeffs[56]);
    
    c1 = _mm_unpackhi_epi16(_mm_setzero_si128(), c0);
    c3 = _mm_unpackhi_epi16(_mm_setzero_si128(), c2);
    c5 = _mm_unpackhi_epi16(_mm_setzero_si128(), c4);
    c7 = _mm_unpackhi_epi16(_mm_setzero_si128(), c6);
    c0 = _mm_unpacklo_epi16(_mm_setzero_si128(), c0);
    c2 = _mm_unpacklo_epi16(_mm_setzero_si128(), c2);
    c4 = _mm_unpacklo_epi16(_mm_setzero_si128(), c4);
    c6 = _mm_unpacklo_epi16(_mm_setzero_si128(), c6);
    c0 = _mm_srai_epi32(c0, 16);
    c1 = _mm_srai_epi32(c1, 16);
    c2 = _mm_srai_epi32(c2, 16);
    c3 = _mm_srai_epi32(c3, 16);
    c4 = _mm_srai_epi32(c4, 16);
    c5 = _mm_srai_epi32(c5, 16);
    c6 = _mm_srai_epi32(c6, 16);
    c7 = _mm_srai_epi32(c7, 16);
    
    c0 = _mm_mullo_epi32(c0, f0);
    c1 = _mm_mullo_epi32(c1, f0);
    c2 = _mm_mullo_epi32(c2, f0);
    c3 = _mm_mullo_epi32(c3, f0);
    c4 = _mm_mullo_epi32(c4, f0);
    c5 = _mm_mullo_epi32(c5, f0);
    c6 = _mm_mullo_epi32(c6, f0);
    c7 = _mm_mullo_epi32(c7, f0);
    c0 = _mm_add_epi32(c0, f1);
    c1 = _mm_add_epi32(c1, f1);
    c2 = _mm_add_epi32(c2, f1);
    c3 = _mm_add_epi32(c3, f1);
    c4 = _mm_add_epi32(c4, f1);
    c5 = _mm_add_epi32(c5, f1);
    c6 = _mm_add_epi32(c6, f1);
    c7 = _mm_add_epi32(c7, f1);
    
    c0 = _mm_srai_epi32(c0, shift);
    c2 = _mm_srai_epi32(c2, shift);
    c4 = _mm_srai_epi32(c4, shift);
    c6 = _mm_srai_epi32(c6, shift);
    c1 = _mm_srai_epi32(c1, shift);
    c3 = _mm_srai_epi32(c3, shift);
    c5 = _mm_srai_epi32(c5, shift);
    c7 = _mm_srai_epi32(c7, shift);
    
    c0 = _mm_packs_epi32(c0, c1);
    c2 = _mm_packs_epi32(c2, c3);
    c4 = _mm_packs_epi32(c4, c5);
    c6 = _mm_packs_epi32(c6, c7);
    
    _mm_store_si128((__m128i *) &coeffs[32], c0);
    _mm_store_si128((__m128i *) &coeffs[40], c2);
    _mm_store_si128((__m128i *) &coeffs[48], c4);
    _mm_store_si128((__m128i *) &coeffs[56], c6);
}

static void hevc_dequant16x16_sse4(int16_t *coeffs, int qp, int bit_dep) {
    __m128i c0, c1, c2, c3, c4, c5, c6, c7, f0, f1;
    int x;
    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };
    
    //TODO: scaling_list_enabled_flag support
    
    int shift = bit_dep - 5;
    int scale = level_scale[qp % 6] << (qp / 6);
    int add = 1 << (shift - 1);
    f0 = _mm_set1_epi32(scale);
    f1 = _mm_set1_epi32(add);
    for (x = 0; x < 16 * 16; x += 64) {
        c0 = _mm_load_si128((__m128i *) &coeffs[0 + x]); //loads 8 first values
        c2 = _mm_load_si128((__m128i *) &coeffs[8 + x]);
        c4 = _mm_load_si128((__m128i *) &coeffs[16 + x]);
        c6 = _mm_load_si128((__m128i *) &coeffs[24 + x]);
        
        c1 = _mm_unpackhi_epi16(_mm_setzero_si128(), c0);
        c3 = _mm_unpackhi_epi16(_mm_setzero_si128(), c2);
        c5 = _mm_unpackhi_epi16(_mm_setzero_si128(), c4);
        c7 = _mm_unpackhi_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_unpacklo_epi16(_mm_setzero_si128(), c0);
        c2 = _mm_unpacklo_epi16(_mm_setzero_si128(), c2);
        c4 = _mm_unpacklo_epi16(_mm_setzero_si128(), c4);
        c6 = _mm_unpacklo_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_srai_epi32(c0, 16);
        c1 = _mm_srai_epi32(c1, 16);
        c2 = _mm_srai_epi32(c2, 16);
        c3 = _mm_srai_epi32(c3, 16);
        c4 = _mm_srai_epi32(c4, 16);
        c5 = _mm_srai_epi32(c5, 16);
        c6 = _mm_srai_epi32(c6, 16);
        c7 = _mm_srai_epi32(c7, 16);
        
        c0 = _mm_mullo_epi32(c0, f0);
        c1 = _mm_mullo_epi32(c1, f0);
        c2 = _mm_mullo_epi32(c2, f0);
        c3 = _mm_mullo_epi32(c3, f0);
        c4 = _mm_mullo_epi32(c4, f0);
        c5 = _mm_mullo_epi32(c5, f0);
        c6 = _mm_mullo_epi32(c6, f0);
        c7 = _mm_mullo_epi32(c7, f0);
        c0 = _mm_add_epi32(c0, f1);
        c1 = _mm_add_epi32(c1, f1);
        c2 = _mm_add_epi32(c2, f1);
        c3 = _mm_add_epi32(c3, f1);
        c4 = _mm_add_epi32(c4, f1);
        c5 = _mm_add_epi32(c5, f1);
        c6 = _mm_add_epi32(c6, f1);
        c7 = _mm_add_epi32(c7, f1);
        
        c0 = _mm_srai_epi32(c0, shift);
        c2 = _mm_srai_epi32(c2, shift);
        c4 = _mm_srai_epi32(c4, shift);
        c6 = _mm_srai_epi32(c6, shift);
        c1 = _mm_srai_epi32(c1, shift);
        c3 = _mm_srai_epi32(c3, shift);
        c5 = _mm_srai_epi32(c5, shift);
        c7 = _mm_srai_epi32(c7, shift);
        
        c0 = _mm_packs_epi32(c0, c1);
        c2 = _mm_packs_epi32(c2, c3);
        c4 = _mm_packs_epi32(c4, c5);
        c6 = _mm_packs_epi32(c6, c7);
        
        _mm_store_si128((__m128i *) &coeffs[0 + x], c0);
        _mm_store_si128((__m128i *) &coeffs[8 + x], c2);
        _mm_store_si128((__m128i *) &coeffs[16 + x], c4);
        _mm_store_si128((__m128i *) &coeffs[24 + x], c6);
        
        c0 = _mm_load_si128((__m128i *) &coeffs[32 + x]);
        c2 = _mm_load_si128((__m128i *) &coeffs[40 + x]);
        c4 = _mm_load_si128((__m128i *) &coeffs[48 + x]);
        c6 = _mm_load_si128((__m128i *) &coeffs[56 + x]);
        
        c1 = _mm_unpackhi_epi16(_mm_setzero_si128(), c0);
        c3 = _mm_unpackhi_epi16(_mm_setzero_si128(), c2);
        c5 = _mm_unpackhi_epi16(_mm_setzero_si128(), c4);
        c7 = _mm_unpackhi_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_unpacklo_epi16(_mm_setzero_si128(), c0);
        c2 = _mm_unpacklo_epi16(_mm_setzero_si128(), c2);
        c4 = _mm_unpacklo_epi16(_mm_setzero_si128(), c4);
        c6 = _mm_unpacklo_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_srai_epi32(c0, 16);
        c1 = _mm_srai_epi32(c1, 16);
        c2 = _mm_srai_epi32(c2, 16);
        c3 = _mm_srai_epi32(c3, 16);
        c4 = _mm_srai_epi32(c4, 16);
        c5 = _mm_srai_epi32(c5, 16);
        c6 = _mm_srai_epi32(c6, 16);
        c7 = _mm_srai_epi32(c7, 16);
        
        c0 = _mm_mullo_epi32(c0, f0);
        c1 = _mm_mullo_epi32(c1, f0);
        c2 = _mm_mullo_epi32(c2, f0);
        c3 = _mm_mullo_epi32(c3, f0);
        c4 = _mm_mullo_epi32(c4, f0);
        c5 = _mm_mullo_epi32(c5, f0);
        c6 = _mm_mullo_epi32(c6, f0);
        c7 = _mm_mullo_epi32(c7, f0);
        
        c0 = _mm_add_epi32(c0, f1);
        c1 = _mm_add_epi32(c1, f1);
        c2 = _mm_add_epi32(c2, f1);
        c3 = _mm_add_epi32(c3, f1);
        c4 = _mm_add_epi32(c4, f1);
        c5 = _mm_add_epi32(c5, f1);
        c6 = _mm_add_epi32(c6, f1);
        c7 = _mm_add_epi32(c7, f1);
        
        c0 = _mm_srai_epi32(c0, shift);
        c1 = _mm_srai_epi32(c1, shift);
        c2 = _mm_srai_epi32(c2, shift);
        c3 = _mm_srai_epi32(c3, shift);
        c4 = _mm_srai_epi32(c4, shift);
        c5 = _mm_srai_epi32(c5, shift);
        c6 = _mm_srai_epi32(c6, shift);
        c7 = _mm_srai_epi32(c7, shift);
        
        c0 = _mm_packs_epi32(c0, c1);
        c2 = _mm_packs_epi32(c2, c3);
        c4 = _mm_packs_epi32(c4, c5);
        c6 = _mm_packs_epi32(c6, c7);
        
        _mm_store_si128((__m128i *) &coeffs[32 + x], c0);
        _mm_store_si128((__m128i *) &coeffs[40 + x], c2);
        _mm_store_si128((__m128i *) &coeffs[48 + x], c4);
        _mm_store_si128((__m128i *) &coeffs[56 + x], c6);
        
    }
}

static void hevc_dequant32x32_sse4(int16_t *coeffs, int qp, int bit_dep) {
    int x;
    __m128i c0, c1, c2, c3, c4, c5, c6, c7, f0, f1;
    
    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };
    
    //TODO: scaling_list_enabled_flag support
    
    int shift = bit_dep - 4;
    int scale = level_scale[qp % 6] << (qp / 6);
    int add = 1 << (shift - 1);
    f0 = _mm_set1_epi32(scale);
    f1 = _mm_set1_epi32(add);
    for (x = 0; x < 32 * 32; x += 64) {
        
        c0 = _mm_load_si128((__m128i *) &coeffs[0 + x]); //loads 8 first values
        c2 = _mm_load_si128((__m128i *) &coeffs[8 + x]);
        c4 = _mm_load_si128((__m128i *) &coeffs[16 + x]);
        c6 = _mm_load_si128((__m128i *) &coeffs[24 + x]);
        
        c1 = _mm_unpackhi_epi16(_mm_setzero_si128(), c0);
        c3 = _mm_unpackhi_epi16(_mm_setzero_si128(), c2);
        c5 = _mm_unpackhi_epi16(_mm_setzero_si128(), c4);
        c7 = _mm_unpackhi_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_unpacklo_epi16(_mm_setzero_si128(), c0);
        c2 = _mm_unpacklo_epi16(_mm_setzero_si128(), c2);
        c4 = _mm_unpacklo_epi16(_mm_setzero_si128(), c4);
        c6 = _mm_unpacklo_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_srai_epi32(c0, 16);
        c1 = _mm_srai_epi32(c1, 16);
        c2 = _mm_srai_epi32(c2, 16);
        c3 = _mm_srai_epi32(c3, 16);
        c4 = _mm_srai_epi32(c4, 16);
        c5 = _mm_srai_epi32(c5, 16);
        c6 = _mm_srai_epi32(c6, 16);
        c7 = _mm_srai_epi32(c7, 16);
        
        c0 = _mm_mullo_epi32(c0, f0);
        c1 = _mm_mullo_epi32(c1, f0);
        c2 = _mm_mullo_epi32(c2, f0);
        c3 = _mm_mullo_epi32(c3, f0);
        c4 = _mm_mullo_epi32(c4, f0);
        c5 = _mm_mullo_epi32(c5, f0);
        c6 = _mm_mullo_epi32(c6, f0);
        c7 = _mm_mullo_epi32(c7, f0);
        c0 = _mm_add_epi32(c0, f1);
        c1 = _mm_add_epi32(c1, f1);
        c2 = _mm_add_epi32(c2, f1);
        c3 = _mm_add_epi32(c3, f1);
        c4 = _mm_add_epi32(c4, f1);
        c5 = _mm_add_epi32(c5, f1);
        c6 = _mm_add_epi32(c6, f1);
        c7 = _mm_add_epi32(c7, f1);
        
        c0 = _mm_srai_epi32(c0, shift);
        c2 = _mm_srai_epi32(c2, shift);
        c4 = _mm_srai_epi32(c4, shift);
        c6 = _mm_srai_epi32(c6, shift);
        c1 = _mm_srai_epi32(c1, shift);
        c3 = _mm_srai_epi32(c3, shift);
        c5 = _mm_srai_epi32(c5, shift);
        c7 = _mm_srai_epi32(c7, shift);
        
        c0 = _mm_packs_epi32(c0, c1);
        c2 = _mm_packs_epi32(c2, c3);
        c4 = _mm_packs_epi32(c4, c5);
        c6 = _mm_packs_epi32(c6, c7);
        
        _mm_store_si128((__m128i *) &coeffs[0 + x], c0);
        _mm_store_si128((__m128i *) &coeffs[8 + x], c2);
        _mm_store_si128((__m128i *) &coeffs[16 + x], c4);
        _mm_store_si128((__m128i *) &coeffs[24 + x], c6);
        
        c0 = _mm_load_si128((__m128i *) &coeffs[32 + x]);
        c2 = _mm_load_si128((__m128i *) &coeffs[40 + x]);
        c4 = _mm_load_si128((__m128i *) &coeffs[48 + x]);
        c6 = _mm_load_si128((__m128i *) &coeffs[56 + x]);
        
        c1 = _mm_unpackhi_epi16(_mm_setzero_si128(), c0);
        c3 = _mm_unpackhi_epi16(_mm_setzero_si128(), c2);
        c5 = _mm_unpackhi_epi16(_mm_setzero_si128(), c4);
        c7 = _mm_unpackhi_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_unpacklo_epi16(_mm_setzero_si128(), c0);
        c2 = _mm_unpacklo_epi16(_mm_setzero_si128(), c2);
        c4 = _mm_unpacklo_epi16(_mm_setzero_si128(), c4);
        c6 = _mm_unpacklo_epi16(_mm_setzero_si128(), c6);
        c0 = _mm_srai_epi32(c0, 16);
        c1 = _mm_srai_epi32(c1, 16);
        c2 = _mm_srai_epi32(c2, 16);
        c3 = _mm_srai_epi32(c3, 16);
        c4 = _mm_srai_epi32(c4, 16);
        c5 = _mm_srai_epi32(c5, 16);
        c6 = _mm_srai_epi32(c6, 16);
        c7 = _mm_srai_epi32(c7, 16);
        
        c0 = _mm_mullo_epi32(c0, f0);
        c1 = _mm_mullo_epi32(c1, f0);
        c2 = _mm_mullo_epi32(c2, f0);
        c3 = _mm_mullo_epi32(c3, f0);
        c4 = _mm_mullo_epi32(c4, f0);
        c5 = _mm_mullo_epi32(c5, f0);
        c6 = _mm_mullo_epi32(c6, f0);
        c7 = _mm_mullo_epi32(c7, f0);
        
        c0 = _mm_add_epi32(c0, f1);
        c1 = _mm_add_epi32(c1, f1);
        c2 = _mm_add_epi32(c2, f1);
        c3 = _mm_add_epi32(c3, f1);
        c4 = _mm_add_epi32(c4, f1);
        c5 = _mm_add_epi32(c5, f1);
        c6 = _mm_add_epi32(c6, f1);
        c7 = _mm_add_epi32(c7, f1);
        
        c0 = _mm_srai_epi32(c0, shift);
        c1 = _mm_srai_epi32(c1, shift);
        c2 = _mm_srai_epi32(c2, shift);
        c3 = _mm_srai_epi32(c3, shift);
        c4 = _mm_srai_epi32(c4, shift);
        c5 = _mm_srai_epi32(c5, shift);
        c6 = _mm_srai_epi32(c6, shift);
        c7 = _mm_srai_epi32(c7, shift);
        
        c0 = _mm_packs_epi32(c0, c1);
        c2 = _mm_packs_epi32(c2, c3);
        c4 = _mm_packs_epi32(c4, c5);
        c6 = _mm_packs_epi32(c6, c7);
        
        _mm_store_si128((__m128i *) &coeffs[32 + x], c0);
        _mm_store_si128((__m128i *) &coeffs[40 + x], c2);
        _mm_store_si128((__m128i *) &coeffs[48 + x], c4);
        _mm_store_si128((__m128i *) &coeffs[56 + x], c6);
    }
}

void ff_hevc_dequant4x4_8_sse4(int16_t *coeffs, int qp) {
    hevc_dequant4x4_sse4(coeffs,qp,8);
}

void ff_hevc_dequant4x4_10_sse4(int16_t *coeffs, int qp) {
    hevc_dequant4x4_sse4(coeffs,qp,10);
}

void ff_hevc_dequant8x8_8_sse4(int16_t *coeffs, int qp) {
    hevc_dequant8x8_sse4(coeffs,qp,8);
}

void ff_hevc_dequant8x8_10_sse4(int16_t *coeffs, int qp) {
    hevc_dequant8x8_sse4(coeffs,qp,10);
}

void ff_hevc_dequant16x16_8_sse4(int16_t *coeffs, int qp) {
    hevc_dequant16x16_sse4(coeffs,qp,8);
}

void ff_hevc_dequant16x16_10_sse4(int16_t *coeffs, int qp) {
    hevc_dequant16x16_sse4(coeffs,qp,10);
}

void ff_hevc_dequant32x32_8_sse4(int16_t *coeffs, int qp) {
    hevc_dequant32x32_sse4(coeffs,qp,8);
}

void ff_hevc_dequant32x32_10_sse4(int16_t *coeffs, int qp) {
    hevc_dequant32x32_sse4(coeffs,qp,10);
}

