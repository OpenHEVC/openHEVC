#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcdsp.h"


#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

DECLARE_ALIGNED(16, static const int16_t, transform4x4_luma[8][8] )=
{
    {   29, +84, 29,  +84,  29, +84,  29, +84 },
    {  +74, +55, +74, +55, +74, +55, +74, +55 },
    {   55, -29,  55, -29,  55, -29,  55, -29 },
    {  +74, -84, +74, -84, +74, -84, +74, -84 },
    {   74, -74,  74, -74,  74, -74,  74, -74 },
    {    0, +74,   0, +74,   0, +74,   0, +74 },
    {   84, +55,  84, +55,  84, +55,  84, +55 },
    {  -74, -29, -74, -29, -74, -29, -74, -29 }
};

DECLARE_ALIGNED( 16, static const int16_t, transform4x4[4][8] ) = {
    { 64,  64, 64,  64, 64,  64, 64,  64 },
    { 64, -64, 64, -64, 64, -64, 64, -64 },
    { 83,  36, 83,  36, 83,  36, 83,  36 },
    { 36, -83, 36, -83, 36, -83, 36, -83 }
};

DECLARE_ALIGNED(16, static const int16_t, transform8x8[12][8] )=
{
    {  89,  75,  89,  75, 89,  75, 89,  75 },
    {  50,  18,  50,  18, 50,  18, 50,  18 },
    {  75, -18,  75, -18, 75, -18, 75, -18 },
    { -89, -50, -89, -50,-89, -50,-89, -50 },
    {  50, -89,  50, -89, 50, -89, 50, -89 },
    {  18,  75,  18,  75, 18,  75, 18,  75 },
    {  18, -50,  18, -50, 18, -50, 18, -50 },
    {  75, -89,  75, -89, 75, -89, 75, -89 },
    {  64,  64,  64,  64, 64,  64, 64,  64 },
    {  64, -64,  64, -64, 64, -64, 64, -64 },
    {  83,  36,  83,  36, 83,  36, 83,  36 },
    {  36, -83,  36, -83, 36, -83, 36, -83 }
};

DECLARE_ALIGNED(16, static const int16_t, transform16x16_1[4][8][8] )=
{
    {/*1-3*/ /*2-6*/
        { 90,  87,  90,  87,  90,  87,  90,  87 },
        { 87,  57,  87,  57,  87,  57,  87,  57 },
        { 80,   9,  80,   9,  80,   9,  80,   9 },
        { 70, -43,  70, -43,  70, -43,  70, -43 },
        { 57, -80,  57, -80,  57, -80,  57, -80 },
        { 43, -90,  43, -90,  43, -90,  43, -90 },
        { 25, -70,  25, -70,  25, -70,  25, -70 },
        { 9,  -25,   9, -25,   9, -25,   9, -25 },
    },{ /*5-7*/ /*10-14*/
        {  80,  70,  80,  70,  80,  70,  80,  70 },
        {   9, -43,   9, -43,   9, -43,   9, -43 },
        { -70, -87, -70, -87, -70, -87, -70, -87 },
        { -87,   9, -87,   9, -87,   9, -87,   9 },
        { -25,  90, -25,  90, -25,  90, -25,  90 },
        {  57,  25,  57,  25,  57,  25,  57,  25 },
        {  90, -80,  90, -80,  90, -80,  90, -80 },
        {  43, -57,  43, -57,  43, -57,  43, -57 },
    },{ /*9-11*/ /*18-22*/
        {  57,  43,  57,  43,  57,  43,  57,  43 },
        { -80, -90, -80, -90, -80, -90, -80, -90 },
        { -25,  57, -25,  57, -25,  57, -25,  57 },
        {  90,  25,  90,  25,  90,  25,  90,  25 },
        {  -9,  -87, -9,  -87, -9,  -87, -9, -87 },
        { -87,  70, -87,  70, -87,  70, -87,  70 },
        {  43,   9,  43,   9,  43,   9,  43,   9 },
        {  70, -80,  70, -80,  70, -80,  70, -80 },
    },{/*13-15*/ /*  26-30   */
        {  25,   9,  25,   9,  25,   9,  25,   9 },
        { -70, -25, -70, -25, -70, -25, -70, -25 },
        {  90,  43,  90,  43,  90,  43,  90,  43 },
        { -80, -57, -80, -57, -80, -57, -80, -57 },
        {  43,  70,  43,  70,  43,  70,  43,  70 },
        {  9,  -80,   9, -80,   9, -80,   9, -80 },
        { -57,  87, -57,  87, -57,  87, -57,  87 },
        {  87, -90,  87, -90,  87, -90,  87, -90 },
    }
};
DECLARE_ALIGNED(16, static const int16_t, transform16x16_2[2][4][8] )=
{
    { /*2-6*/ /*4-12*/
        { 89,  75,  89,  75, 89,  75, 89,  75 },
        { 75, -18,  75, -18, 75, -18, 75, -18 },
        { 50, -89,  50, -89, 50, -89, 50, -89 },
        { 18, -50,  18, -50, 18, -50, 18, -50 },
    },{ /*10-14*/  /*20-28*/
        {  50,  18,  50,  18,  50,  18,  50,  18 },
        { -89, -50, -89, -50, -89, -50, -89, -50 },
        {  18,  75,  18,  75,  18,  75,  18,  75 },
        {  75, -89,  75, -89,  75, -89,  75, -89 },
    }
};

DECLARE_ALIGNED(16, static const int16_t, transform16x16_3[2][2][8] )=
{
    {/*4-12*/ /*8-24*/
        {  83,  36,  83,  36,  83,  36,  83,  36 },
        {  36, -83,  36, -83,  36, -83,  36, -83 },
    },{ /*0-8*/  /*0-16*/
        { 64,  64, 64,  64, 64,  64, 64,  64 },
        { 64, -64, 64, -64, 64, -64, 64, -64 },
    }
};


DECLARE_ALIGNED(16, static const int16_t, transform32x32[8][16][8] )=
{
    { /*   1-3     */
        { 90,  90, 90,  90, 90,  90, 90,  90 },
        { 90,  82, 90,  82, 90,  82, 90,  82 },
        { 88,  67, 88,  67, 88,  67, 88,  67 },
        { 85,  46, 85,  46, 85,  46, 85,  46 },
        { 82,  22, 82,  22, 82,  22, 82,  22 },
        { 78,  -4, 78,  -4, 78,  -4, 78,  -4 },
        { 73, -31, 73, -31, 73, -31, 73, -31 },
        { 67, -54, 67, -54, 67, -54, 67, -54 },
        { 61, -73, 61, -73, 61, -73, 61, -73 },
        { 54, -85, 54, -85, 54, -85, 54, -85 },
        { 46, -90, 46, -90, 46, -90, 46, -90 },
        { 38, -88, 38, -88, 38, -88, 38, -88 },
        { 31, -78, 31, -78, 31, -78, 31, -78 },
        { 22, -61, 22, -61, 22, -61, 22, -61 },
        { 13, -38, 13, -38, 13, -38, 13, -38 },
        { 4,  -13,  4, -13,  4, -13,  4, -13 },
    },{/*  5-7 */
        {  88,  85,  88,  85,  88,  85,  88,  85 },
        {  67,  46,  67,  46,  67,  46,  67,  46 },
        {  31, -13,  31, -13,  31, -13,  31, -13 },
        { -13, -67, -13, -67, -13, -67, -13, -67 },
        { -54, -90, -54, -90, -54, -90, -54, -90 },
        { -82, -73, -82, -73, -82, -73, -82, -73 },
        { -90, -22, -90, -22, -90, -22, -90, -22 },
        { -78,  38, -78,  38, -78,  38, -78,  38 },
        { -46,  82, -46,  82, -46,  82, -46,  82 },
        {  -4,  88,  -4,  88,  -4,  88,  -4,  88 },
        {  38,  54,  38,  54,  38,  54,  38,  54 },
        {  73,  -4,  73,  -4,  73,  -4,  73,  -4 },
        {  90, -61,  90, -61,  90, -61,  90, -61 },
        {  85, -90,  85, -90,  85, -90,  85, -90 },
        {  61, -78,  61, -78,  61, -78,  61, -78 },
        {  22, -31,  22, -31,  22, -31,  22, -31 },
    },{/*  9-11   */
        {  82,  78,  82,  78,  82,  78,  82,  78 },
        {  22,  -4,  22,  -4,  22,  -4,  22,  -4 },
        { -54, -82, -54, -82, -54, -82, -54, -82 },
        { -90, -73, -90, -73, -90, -73, -90, -73 },
        { -61,  13, -61,  13, -61,  13, -61,  13 },
        {  13,  85,  13,  85,  13,  85,  13,  85 },
        {  78,  67,  78,  67,  78,  67,  78,  67 },
        {  85, -22,  85, -22,  85, -22,  85, -22 },
        {  31, -88,  31, -88,  31, -88,  31, -88 },
        { -46, -61, -46, -61, -46, -61, -46, -61 },
        { -90,  31, -90,  31, -90,  31, -90,  31 },
        { -67,  90, -67,  90, -67,  90, -67,  90 },
        {   4,  54,   4,  54,   4,  54,   4,  54 },
        {  73, -38,  73, -38,  73, -38,  73, -38 },
        {  88, -90,  88, -90,  88, -90,  88, -90 },
        {  38, -46,  38, -46,  38, -46,  38, -46 },
    },{/*  13-15   */
        {  73,  67,  73,  67,  73,  67,  73,  67 },
        { -31, -54, -31, -54, -31, -54, -31, -54 },
        { -90, -78, -90, -78, -90, -78, -90, -78 },
        { -22,  38, -22,  38, -22,  38, -22,  38 },
        {  78,  85,  78,  85,  78,  85,  78,  85 },
        {  67, -22,  67, -22,  67, -22,  67, -22 },
        { -38, -90, -38, -90, -38, -90, -38, -90 },
        { -90,   4, -90,   4, -90,   4, -90,   4 },
        { -13,  90, -13,  90, -13,  90, -13,  90 },
        {  82,  13,  82,  13,  82,  13,  82,  13 },
        {  61, -88,  61, -88,  61, -88,  61, -88 },
        { -46, -31, -46, -31, -46, -31, -46, -31 },
        { -88,  82, -88,  82, -88,  82, -88,  82 },
        { -4,   46, -4,   46, -4,   46, -4,   46 },
        {  85, -73,  85, -73,  85, -73,  85, -73 },
        {  54, -61,  54, -61,  54, -61,  54, -61 },
    },{/*  17-19   */
        {  61,  54,  61,  54,  61,  54,  61,  54 },
        { -73, -85, -73, -85, -73, -85, -73, -85 },
        { -46,  -4, -46,  -4, -46,  -4, -46,  -4 },
        {  82,  88,  82,  88,  82,  88,  82,  88 },
        {  31, -46,  31, -46,  31, -46,  31, -46 },
        { -88, -61, -88, -61, -88, -61, -88, -61 },
        { -13,  82, -13,  82, -13,  82, -13,  82 },
        {  90,  13,  90,  13,  90,  13,  90,  13 },
        { -4, -90,  -4, -90,  -4, -90,  -4, -90 },
        { -90,  38, -90,  38, -90,  38, -90,  38 },
        {  22,  67,  22,  67,  22,  67,  22,  67 },
        {  85, -78,  85, -78,  85, -78,  85, -78 },
        { -38, -22, -38, -22, -38, -22, -38, -22 },
        { -78,  90, -78,  90, -78,  90, -78,  90 },
        {  54, -31,  54, -31,  54, -31,  54, -31 },
        {  67, -73,  67, -73,  67, -73,  67, -73 },
    },{ /*  21-23   */
        {  46,  38,  46,  38,  46,  38,  46,  38 },
        { -90, -88, -90, -88, -90, -88, -90, -88 },
        {  38,  73,  38,  73,  38,  73,  38,  73 },
        {  54,  -4,  54,  -4,  54,  -4,  54,  -4 },
        { -90, -67, -90, -67, -90, -67, -90, -67 },
        {  31,  90,  31,  90,  31,  90,  31,  90 },
        {  61, -46,  61, -46,  61, -46,  61, -46 },
        { -88, -31, -88, -31, -88, -31, -88, -31 },
        {  22,  85,  22,  85,  22,  85,  22,  85 },
        {  67, -78,  67, -78,  67, -78,  67, -78 },
        { -85,  13, -85,  13, -85,  13, -85,  13 },
        {  13,  61,  13,  61,  13,  61,  13,  61 },
        {  73, -90,  73, -90,  73, -90,  73, -90 },
        { -82,  54, -82,  54, -82,  54, -82,  54 },
        {   4,  22,   4,  22,   4,  22,   4,  22 },
        {  78, -82,  78, -82,  78, -82,  78, -82 },
    },{ /*  25-27   */
        {  31,  22,  31,  22,  31,  22,  31,  22 },
        { -78, -61, -78, -61, -78, -61, -78, -61 },
        {  90,  85,  90,  85,  90,  85,  90,  85 },
        { -61, -90, -61, -90, -61, -90, -61, -90 },
        {   4,  73,   4,  73,   4,  73,   4,  73 },
        {  54, -38,  54, -38,  54, -38,  54, -38 },
        { -88,  -4, -88,  -4, -88,  -4, -88,  -4 },
        {  82,  46,  82,  46,  82,  46,  82,  46 },
        { -38, -78, -38, -78, -38, -78, -38, -78 },
        { -22,  90, -22,  90, -22,  90, -22,  90 },
        {  73, -82,  73, -82,  73, -82,  73, -82 },
        { -90,  54, -90,  54, -90,  54, -90,  54 },
        {  67, -13,  67, -13,  67, -13,  67, -13 },
        { -13, -31, -13, -31, -13, -31, -13, -31 },
        { -46,  67, -46,  67, -46,  67, -46,  67 },
        {  85, -88,  85, -88,  85, -88,  85, -88 },
    },{/*  29-31   */
        {  13,   4,  13,   4,  13,   4,  13,   4 },
        { -38, -13, -38, -13, -38, -13, -38, -13 },
        {  61,  22,  61,  22,  61,  22,  61,  22 },
        { -78, -31, -78, -31, -78, -31, -78, -31 },
        {  88,  38,  88,  38,  88,  38,  88,  38 },
        { -90, -46, -90, -46, -90, -46, -90, -46 },
        {  85,  54,  85,  54,  85,  54,  85,  54 },
        { -73, -61, -73, -61, -73, -61, -73, -61 },
        {  54,  67,  54,  67,  54,  67,  54,  67 },
        { -31, -73, -31, -73, -31, -73, -31, -73 },
        {   4,  78,   4,  78,   4,  78,   4,  78 },
        {  22, -82,  22, -82,  22, -82,  22, -82 },
        { -46,  85, -46,  85, -46,  85, -46,  85 },
        {  67, -88,  67, -88,  67, -88,  67, -88 },
        { -82,  90, -82,  90, -82,  90, -82,  90 },
        {  90, -90,  90, -90,  90, -90,  90, -90 },
    }
};

#define shift_1st 7
#define add_1st (1 << (shift_1st - 1))

static void transpose_8x8(__m128i *array, int stride) {

    __m128i *p_input  = (__m128i*)array;
    __m128i *p_output = (__m128i*)array;
    __m128i m128iS0 = _mm_load_si128(p_input);
    __m128i m128iS1 = _mm_load_si128(p_input+=stride);
    __m128i m128iS2 = _mm_load_si128(p_input+=stride);
    __m128i m128iS3 = _mm_load_si128(p_input+=stride);
    __m128i m128iS4 = _mm_load_si128(p_input+=stride);
    __m128i m128iS5 = _mm_load_si128(p_input+=stride);
    __m128i m128iS6 = _mm_load_si128(p_input+=stride);
    __m128i m128iS7 = _mm_load_si128(p_input+=stride);

    __m128i E0l = _mm_unpacklo_epi16(m128iS0, m128iS4);
    __m128i E1l = _mm_unpacklo_epi16(m128iS1, m128iS5);
    __m128i E2l = _mm_unpacklo_epi16(m128iS2, m128iS6);
    __m128i E3l = _mm_unpacklo_epi16(m128iS3, m128iS7);
    __m128i O0l = _mm_unpackhi_epi16(m128iS0, m128iS4);
    __m128i O1l = _mm_unpackhi_epi16(m128iS1, m128iS5);
    __m128i O2l = _mm_unpackhi_epi16(m128iS2, m128iS6);
    __m128i O3l = _mm_unpackhi_epi16(m128iS3, m128iS7);

    __m128i m128Tmp0 = _mm_unpacklo_epi16(E0l, E2l);
    __m128i m128Tmp1 = _mm_unpacklo_epi16(E1l, E3l);
    __m128i m128Tmp2 = _mm_unpackhi_epi16(E0l, E2l);
    __m128i m128Tmp3 = _mm_unpackhi_epi16(E1l, E3l);
    m128iS0  = _mm_unpacklo_epi16(m128Tmp0, m128Tmp1);
    m128iS1  = _mm_unpackhi_epi16(m128Tmp0, m128Tmp1);
    m128iS2  = _mm_unpacklo_epi16(m128Tmp2, m128Tmp3);
    m128iS3  = _mm_unpackhi_epi16(m128Tmp2, m128Tmp3);
    m128Tmp0 = _mm_unpacklo_epi16(O0l, O2l);
    m128Tmp1 = _mm_unpacklo_epi16(O1l, O3l);
    m128iS4  = _mm_unpacklo_epi16(m128Tmp0, m128Tmp1);
    m128iS5  = _mm_unpackhi_epi16(m128Tmp0, m128Tmp1);
    m128Tmp2 = _mm_unpackhi_epi16(O0l, O2l);
    m128Tmp3 = _mm_unpackhi_epi16(O1l, O3l);
    m128iS6  = _mm_unpacklo_epi16(m128Tmp2, m128Tmp3);
    m128iS7  = _mm_unpackhi_epi16(m128Tmp2, m128Tmp3);

    _mm_store_si128(p_output, m128iS0);
    _mm_store_si128(p_output+=stride, m128iS1);
    _mm_store_si128(p_output+=stride, m128iS2);
    _mm_store_si128(p_output+=stride, m128iS3);
    _mm_store_si128(p_output+=stride, m128iS4);
    _mm_store_si128(p_output+=stride, m128iS5);
    _mm_store_si128(p_output+=stride, m128iS6);
    _mm_store_si128(p_output+=stride, m128iS7);
}



void ff_hevc_transform_skip_8_sse(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    uint8_t *dst = (uint8_t*)_dst;
    ptrdiff_t stride = _stride;
    int shift = 5;
    int offset = 16;
    __m128i r0, r1, r2, r3, r4, r5, r6, r9;

    r9 = _mm_setzero_si128();
    r2 = _mm_set1_epi16(offset);

    r0 = _mm_load_si128((__m128i*)(coeffs));
    r1 = _mm_load_si128((__m128i*)(coeffs + 8));


    r0 = _mm_adds_epi16(r0, r2);
    r1 = _mm_adds_epi16(r1, r2);

    r0 = _mm_srai_epi16(r0, shift);
    r1 = _mm_srai_epi16(r1, shift);

    r3 = _mm_loadl_epi64((__m128i*)(dst));
    r4 = _mm_loadl_epi64((__m128i*)(dst + stride));
    r5 = _mm_loadl_epi64((__m128i*)(dst + 2 * stride));
    r6 = _mm_loadl_epi64((__m128i*)(dst + 3 * stride));

    r3 = _mm_unpacklo_epi8(r3, r9);
    r4 = _mm_unpacklo_epi8(r4, r9);
    r5 = _mm_unpacklo_epi8(r5, r9);
    r6 = _mm_unpacklo_epi8(r6, r9);
    r3 = _mm_unpacklo_epi64(r3, r4);
    r4 = _mm_unpacklo_epi64(r5, r6);


    r3 = _mm_adds_epi16(r3, r0);
    r4 = _mm_adds_epi16(r4, r1);

    r3 = _mm_packus_epi16(r3, r4);

    *((uint32_t *)(dst)) = _mm_cvtsi128_si32(r3);
    dst+=stride;
    *((uint32_t *)(dst)) = _mm_extract_epi32(r3, 1);
    dst+=stride;
    *((uint32_t *)(dst)) = _mm_extract_epi32(r3, 2);
    dst+=stride;
    *((uint32_t *)(dst)) = _mm_extract_epi32(r3, 3);
}

void ff_hevc_transform_4x4_luma_add_8_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    uint8_t shift_2nd = 12; // 20 - Bit depth
    uint16_t add_2nd = 1 << 11; //(1 << (shift_2nd - 1))

    uint8_t *dst = (uint8_t*) _dst;
    ptrdiff_t stride = _stride;
    int16_t *src = coeffs;
    __m128i m128iAdd, S0, S8, m128iTmp1, m128iTmp2, m128iAC, m128iBD, m128iA,
            m128iD;
    m128iAdd = _mm_set1_epi32(64);

    S0 = _mm_load_si128((__m128i *) (src));
    S8 = _mm_load_si128((__m128i *) (src + 8));

    m128iAC = _mm_unpacklo_epi16(S0, S8);
    m128iBD = _mm_unpackhi_epi16(S0, S8);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[0])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[1])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_1st);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[2])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[3])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_1st);

    m128iA = _mm_packs_epi32(S0, S8);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[4])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[5])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_1st);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[6])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[7])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_1st);

    m128iD = _mm_packs_epi32(S0, S8);

    S0 = _mm_unpacklo_epi16(m128iA, m128iD);
    S8 = _mm_unpackhi_epi16(m128iA, m128iD);

    m128iA = _mm_unpacklo_epi16(S0, S8);
    m128iD = _mm_unpackhi_epi16(S0, S8);

    /*   ###################    */
    m128iAdd = _mm_set1_epi32(add_2nd);

    m128iAC = _mm_unpacklo_epi16(m128iA, m128iD);
    m128iBD = _mm_unpackhi_epi16(m128iA, m128iD);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[0])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[1])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_2nd);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[2])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[3])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_2nd);

    m128iA = _mm_packs_epi32(S0, S8);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[4])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[5])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_2nd);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[6])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[7])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_2nd);

    m128iD = _mm_packs_epi32(S0, S8);

    S0 = _mm_move_epi64(m128iA); //contains row 0
    S8 = _mm_move_epi64(m128iD); //row 2
    m128iA = _mm_srli_si128(m128iA, 8); // row 1
    m128iD = _mm_srli_si128(m128iD, 8); // row 3
    m128iTmp1 = _mm_unpacklo_epi16(S0, m128iA);
    m128iTmp2 = _mm_unpacklo_epi16(S8, m128iD);
    S0 = _mm_unpacklo_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_unpackhi_epi32(m128iTmp1, m128iTmp2);

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(S0, m128iA);	//contains first 4 values
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    *((uint32_t *)(dst)) = _mm_cvtsi128_si32(m128iTmp1);

    dst += stride;

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(_mm_srli_si128(S0, 8), m128iA);
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    *((uint32_t *)(dst)) = _mm_cvtsi128_si32(m128iTmp1);

    dst += stride;

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(S8, m128iA);
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    *((uint32_t *)(dst)) = _mm_cvtsi128_si32(m128iTmp1);

    dst += stride;

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(_mm_srli_si128(S8, 8), m128iA);
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    *((uint32_t *)(dst)) = _mm_cvtsi128_si32(m128iTmp1);
}

void ff_hevc_transform_4x4_luma_add_10_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    int i,j;
    uint8_t shift_2nd = 10; // 20 - Bit depth
    uint16_t add_2nd = 1 << 9; //(1 << (shift_2nd - 1))

    uint16_t *dst = (uint16_t*) _dst;
    ptrdiff_t stride = _stride/(sizeof(uint16_t));
    int16_t *src = coeffs;
    __m128i m128iAdd, S0, S8, m128iTmp1, m128iTmp2, m128iAC, m128iBD, m128iA,
            m128iD;

    m128iAdd = _mm_set1_epi32(64);

    S0 = _mm_loadu_si128((__m128i *) (src));
    S8 = _mm_loadu_si128((__m128i *) (src + 8));

    m128iAC = _mm_unpacklo_epi16(S0, S8);
    m128iBD = _mm_unpackhi_epi16(S0, S8);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[0])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[1])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_1st);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[2])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[3])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_1st);

    m128iA = _mm_packs_epi32(S0, S8);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[4])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[5])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_1st);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[6])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_loadu_si128((__m128i *) (transform4x4_luma[7])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_1st);

    m128iD = _mm_packs_epi32(S0, S8);

    S0 = _mm_unpacklo_epi16(m128iA, m128iD);
    S8 = _mm_unpackhi_epi16(m128iA, m128iD);

    m128iA = _mm_unpacklo_epi16(S0, S8);
    m128iD = _mm_unpackhi_epi16(S0, S8);

    /*   ###################    */
    m128iAdd = _mm_set1_epi32(add_2nd);

    m128iAC = _mm_unpacklo_epi16(m128iA, m128iD);
    m128iBD = _mm_unpackhi_epi16(m128iA, m128iD);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[0])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[1])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_2nd);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[2])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[3])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_2nd);

    m128iA = _mm_packs_epi32(S0, S8);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[4])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[5])));
    S0 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S0 = _mm_add_epi32(S0, m128iAdd);
    S0 = _mm_srai_epi32(S0, shift_2nd);

    m128iTmp1 = _mm_madd_epi16(m128iAC,
            _mm_load_si128((__m128i *) (transform4x4_luma[6])));
    m128iTmp2 = _mm_madd_epi16(m128iBD,
            _mm_load_si128((__m128i *) (transform4x4_luma[7])));
    S8 = _mm_add_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_add_epi32(S8, m128iAdd);
    S8 = _mm_srai_epi32(S8, shift_2nd);

    m128iD = _mm_packs_epi32(S0, S8);

    _mm_storeu_si128((__m128i *) (src), m128iA);
    _mm_storeu_si128((__m128i *) (src + 8), m128iD);
    j = 0;
    for (i = 0; i < 2; i++) {
        dst[0] = av_clip_uintp2(dst[0] + src[j],10);
        dst[1] = av_clip_uintp2(dst[1] + src[j + 4],10);
        dst[2] = av_clip_uintp2(dst[2] + src[j + 8],10);
        dst[3] = av_clip_uintp2(dst[3] + src[j + 12],10);
        j += 1;
        dst += stride;
        dst[0] = av_clip_uintp2(dst[0] + src[j],10);
        dst[1] = av_clip_uintp2(dst[1] + src[j + 4],10);
        dst[2] = av_clip_uintp2(dst[2] + src[j + 8],10);
        dst[3] = av_clip_uintp2(dst[3] + src[j + 12],10);
        j += 1;
        dst += stride;
    }

}
#if 1
////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define INIT_8()                                                               \
    uint8_t *p_dst;                                                            \
    uint8_t *dst = (uint8_t*) _dst;                                            \
    ptrdiff_t stride = _stride
#define INIT_10()                                                              \
    uint16_t *p_dst;                                                           \
    uint16_t *dst = (uint16_t*) _dst;                                          \
    ptrdiff_t stride = _stride>>1

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define LOAD_EMPTY(dst, src)
#define LOAD4x4(dst, src)                                                      \
    dst ## 0 = _mm_loadu_si128((__m128i *) &src[0]);                           \
    dst ## 1 = _mm_loadu_si128((__m128i *) &src[8])
#define LOAD4x4_STEP(dst, src, sstep)                                          \
    tmp0 = _mm_loadl_epi64((__m128i *) &src[0 * sstep]);                       \
    tmp1 = _mm_loadl_epi64((__m128i *) &src[1 * sstep]);                       \
    tmp2 = _mm_loadl_epi64((__m128i *) &src[2 * sstep]);                       \
    tmp3 = _mm_loadl_epi64((__m128i *) &src[3 * sstep]);                       \
    dst ## 0 = _mm_unpacklo_epi16(tmp0, tmp2);                                 \
    dst ## 1 = _mm_unpacklo_epi16(tmp1, tmp3)
#define LOAD8x8_E(dst, src, sstep)                                             \
    dst ## 0 = _mm_loadu_si128((__m128i *) &src[0 * sstep]);                   \
    dst ## 1 = _mm_loadu_si128((__m128i *) &src[1 * sstep]);                   \
    dst ## 2 = _mm_loadu_si128((__m128i *) &src[2 * sstep]);                   \
    dst ## 3 = _mm_loadu_si128((__m128i *) &src[3 * sstep])
#define LOAD8x8_O(dst, src, sstep)                                             \
    tmp0 = _mm_loadu_si128((__m128i *) &src[1 * sstep]);                       \
    tmp1 = _mm_loadu_si128((__m128i *) &src[3 * sstep]);                       \
    tmp2 = _mm_loadu_si128((__m128i *) &src[5 * sstep]);                       \
    tmp3 = _mm_loadu_si128((__m128i *) &src[7 * sstep]);                       \
    dst ## 0 = _mm_unpacklo_epi16(tmp0, tmp1);                                 \
    dst ## 1 = _mm_unpackhi_epi16(tmp0, tmp1);                                 \
    dst ## 2 = _mm_unpacklo_epi16(tmp2, tmp3);                                 \
    dst ## 3 = _mm_unpackhi_epi16(tmp2, tmp3)
#define LOAD16x16_O(dst, src, sstep)                                           \
    LOAD8x8_O(dst, src, sstep);                                                \
    tmp0 = _mm_loadu_si128((__m128i *) &src[ 9 * sstep]);                      \
    tmp1 = _mm_loadu_si128((__m128i *) &src[11 * sstep]);                      \
    tmp2 = _mm_loadu_si128((__m128i *) &src[13 * sstep]);                      \
    tmp3 = _mm_loadu_si128((__m128i *) &src[15 * sstep]);                      \
    dst ## 4 = _mm_unpacklo_epi16(tmp0, tmp1);                                 \
    dst ## 5 = _mm_unpackhi_epi16(tmp0, tmp1);                                 \
    dst ## 6 = _mm_unpacklo_epi16(tmp2, tmp3);                                 \
    dst ## 7 = _mm_unpackhi_epi16(tmp2, tmp3)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define ASSIGN_EMPTY(dst, dst_stride, src)
#define SAVE_8x16(dst, dst_stride, src)                                       \
    _mm_storeu_si128((__m128i *) dst, src);                                    \
    dst += dst_stride
#define SAVE_8x32(dst, dst_stride, src0, src1, idx)                            \
    _mm_storeu_si128((__m128i *) &dst[idx*dst_stride]  , src0);                \
    _mm_storeu_si128((__m128i *) &dst[idx*dst_stride+4], src1)
#define ADD_AND_SAVE_4x8(dst, dst_stride, src)                                \
    tmp0 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) dst), _mm_setzero_si128());\
    tmp0 = _mm_packus_epi16(_mm_add_epi16(src, tmp0), _mm_setzero_si128());    \
    *((uint32_t *)(dst)) = _mm_cvtsi128_si32(tmp0);                            \
    dst += dst_stride
#define ADD_AND_SAVE_8x8(dst, dst_stride, src)                                \
    tmp0 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) dst), _mm_setzero_si128());\
    tmp0 = _mm_packus_epi16(_mm_add_epi16(src, tmp0), _mm_setzero_si128());    \
    _mm_storel_epi64((__m128i *) dst, tmp0);                                   \
    dst += dst_stride
#define ADD_AND_SAVE_4x10(dst, dst_stride, src)                               \
    tmp0 = _mm_loadl_epi64((__m128i *) dst);                                   \
    tmp0 = _mm_add_epi16(src, tmp0);                                           \
    tmp1 = _mm_set1_epi16(0x03ff);                                             \
    tmp0 = _mm_max_epi16(tmp0, _mm_setzero_si128());                           \
    tmp0 = _mm_min_epi16(tmp0, tmp1);                                          \
    _mm_storel_epi64((__m128i *) dst, tmp0);                                   \
    dst += dst_stride
#define ADD_AND_SAVE_8x10(dst, dst_stride, src)                                \
    tmp0 = _mm_loadu_si128((__m128i *) dst);                                   \
    tmp0 = _mm_add_epi16(src, tmp0);                                           \
    tmp1 = _mm_set1_epi16(0x03ff);                                             \
    tmp0 = _mm_max_epi16(tmp0, _mm_setzero_si128());                           \
    tmp0 = _mm_min_epi16(tmp0, tmp1);                                          \
    _mm_storeu_si128((__m128i *) dst, tmp0);                                   \
    dst += dst_stride
#define ASSIGN4(dst, dst_stride, src0, src1, src2, src3, assign)               \
    assign(dst, dst_stride, src0);                                             \
    assign(dst, dst_stride, src1);                                             \
    assign(dst, dst_stride, src2);                                             \
    assign(dst, dst_stride, src3)
#define ASSIGN4_LO(dst, dst_stride, src, assign)                               \
    ASSIGN4(dst, dst_stride, src ## 0, src ## 1, src ## 2, src ## 3, assign)
#define ASSIGN4_HI(dst, dst_stride, src, assign)                               \
    ASSIGN4(dst, dst_stride, src ## 4, src ## 5, src ## 6, src ## 7, assign)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define TRANSPOSE4X4_16(dst)                                                   \
    tmp0 = _mm_unpacklo_epi16(dst ## 0, dst ## 1);                             \
    tmp1 = _mm_unpackhi_epi16(dst ## 0, dst ## 1);                             \
    dst ## 0 = _mm_unpacklo_epi16(tmp0, tmp1);                                 \
    dst ## 1 = _mm_unpackhi_epi16(tmp0, tmp1)
#define TRANSPOSE8X8_16(dst)                                                   \
    tmp0 = _mm_unpacklo_epi16(dst ## 0, dst ## 1);                             \
    tmp1 = _mm_unpacklo_epi16(dst ## 2, dst ## 3);                             \
    tmp2 = _mm_unpacklo_epi16(dst ## 4, dst ## 5);                             \
    tmp3 = _mm_unpacklo_epi16(dst ## 6, dst ## 7);                             \
    src0 = _mm_unpacklo_epi32(tmp0, tmp1);                                     \
    src1 = _mm_unpacklo_epi32(tmp2, tmp3);                                     \
    src2 = _mm_unpackhi_epi32(tmp0, tmp1);                                     \
    src3 = _mm_unpackhi_epi32(tmp2, tmp3);                                     \
    tmp0 = _mm_unpackhi_epi16(dst ## 0, dst ## 1);                             \
    tmp1 = _mm_unpackhi_epi16(dst ## 2, dst ## 3);                             \
    tmp2 = _mm_unpackhi_epi16(dst ## 4, dst ## 5);                             \
    tmp3 = _mm_unpackhi_epi16(dst ## 6, dst ## 7);                             \
    dst ## 0 = _mm_unpacklo_epi64(src0 , src1);                                \
    dst ## 1 = _mm_unpackhi_epi64(src0 , src1);                                \
    dst ## 2 = _mm_unpacklo_epi64(src2 , src3);                                \
    dst ## 3 = _mm_unpackhi_epi64(src2 , src3);                                \
    src0 = _mm_unpacklo_epi32(tmp0, tmp1);                                     \
    src1 = _mm_unpacklo_epi32(tmp2, tmp3);                                     \
    src2 = _mm_unpackhi_epi32(tmp0, tmp1);                                     \
    src3 = _mm_unpackhi_epi32(tmp2, tmp3);                                     \
    dst ## 4 = _mm_unpacklo_epi64(src0 , src1);                                \
    dst ## 5 = _mm_unpackhi_epi64(src0 , src1);                                \
    dst ## 6 = _mm_unpacklo_epi64(src2 , src3);                                \
    dst ## 7 = _mm_unpackhi_epi64(src2 , src3)
#define TRANSPOSE8x8_16_S(out, sstep_out, src, assign)                         \
    TRANSPOSE8X8_16(src);                                                      \
    p_dst = out;                                                               \
    ASSIGN4_LO(p_dst, sstep_out, src, assign);                                 \
    ASSIGN4_HI(p_dst, sstep_out, src, assign)
#define TRANSPOSE8x8_16_LS(out, sstep_out, in, sstep_in, assign)               \
    e0  = _mm_load_si128((__m128i *) &in[0*sstep_in]);                         \
    e1  = _mm_load_si128((__m128i *) &in[1*sstep_in]);                         \
    e2  = _mm_load_si128((__m128i *) &in[2*sstep_in]);                         \
    e3  = _mm_load_si128((__m128i *) &in[3*sstep_in]);                         \
    e4  = _mm_load_si128((__m128i *) &in[4*sstep_in]);                         \
    e5  = _mm_load_si128((__m128i *) &in[5*sstep_in]);                         \
    e6  = _mm_load_si128((__m128i *) &in[6*sstep_in]);                         \
    e7  = _mm_load_si128((__m128i *) &in[7*sstep_in]);                         \
    TRANSPOSE8x8_16_S(out, sstep_out, e, assign)
#define TRANSPOSE16x16_LS(out, sstep_out, in, sstep_in, assign)               \
    TRANSPOSE8x8_16_LS((&out[0*sstep_out+0]), sstep_out, (&in[0*sstep_in+0]), sstep_in, assign);\
    TRANSPOSE8x8_16_LS((&out[0*sstep_out+8]), sstep_out, (&in[8*sstep_in+0]), sstep_in, assign);\
    TRANSPOSE8x8_16_LS((&out[8*sstep_out+0]), sstep_out, (&in[0*sstep_in+8]), sstep_in, assign);\
    TRANSPOSE8x8_16_LS((&out[8*sstep_out+8]), sstep_out, (&in[8*sstep_in+8]), sstep_in, assign);\

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_transform_4x4_add_X_sse4
////////////////////////////////////////////////////////////////////////////////
#define COMPUTE4x4(dst0, dst1, dst2, dst3)                                     \
    tmp0 = _mm_load_si128((__m128i *) transform4x4[0]);                        \
    tmp1 = _mm_load_si128((__m128i *) transform4x4[1]);                        \
    tmp2 = _mm_load_si128((__m128i *) transform4x4[2]);                        \
    tmp3 = _mm_load_si128((__m128i *) transform4x4[3]);                        \
    tmp0 = _mm_madd_epi16(e6, tmp0);                                           \
    tmp1 = _mm_madd_epi16(e6, tmp1);                                           \
    tmp2 = _mm_madd_epi16(e7, tmp2);                                           \
    tmp3 = _mm_madd_epi16(e7, tmp3);                                           \
    e6   = _mm_set1_epi32(add);                                                \
    tmp0 = _mm_add_epi32(tmp0, e6);                                            \
    tmp1 = _mm_add_epi32(tmp1, e6);                                            \
    dst0 = _mm_add_epi32(tmp0, tmp2);                                          \
    dst1 = _mm_add_epi32(tmp1, tmp3);                                          \
    dst2 = _mm_sub_epi32(tmp1, tmp3);                                          \
    dst3 = _mm_sub_epi32(tmp0, tmp2)
#define COMPUTE4x4_LO()                                                        \
    COMPUTE4x4(e0, e1, e2, e3)
#define COMPUTE4x4_HI(dst)                                                     \
    COMPUTE4x4(e7, e6, e5, e4)

#define SCALE4x4(dst0, dst1, src0, src1, src2, src3)                           \
    src0 = _mm_srai_epi32(src0, shift);                                        \
    src1 = _mm_srai_epi32(src1, shift);                                        \
    src2 = _mm_srai_epi32(src2, shift);                                        \
    src3 = _mm_srai_epi32(src3, shift);                                        \
    dst0 = _mm_packs_epi32(src0, src1);                                        \
    dst1 = _mm_packs_epi32(src2, src3)

#define TR_4(dst, dst_stride, in, sstep, load, assign)                         \
    load(e, in);                                                               \
    e6 = _mm_unpacklo_epi16(e0, e1);                                           \
    e7 = _mm_unpackhi_epi16(e0, e1);                                           \
    COMPUTE4x4_LO();                                                           \
    SCALE4x4(e0, e1, e0, e1, e2, e3);                                          \
    TRANSPOSE4X4_16(e);                                                        \
    assign(dst, dst_stride, e0);                                               \
    assign(dst, dst_stride, _mm_srli_si128(e0, 8));                            \
    assign(dst, dst_stride, e1);                                               \
    assign(dst, dst_stride, _mm_srli_si128(e1, 8));

#define TR_4_1( dst, dst_stride, src)    TR_4( dst, dst_stride, src,  4, LOAD4x4, ASSIGN_EMPTY)
#define TR_4_2( dst, dst_stride, src, D) TR_4( dst, dst_stride, src,  4, LOAD_EMPTY, ADD_AND_SAVE_4x ## D)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_transform_8x8_add_X_sse4
////////////////////////////////////////////////////////////////////////////////
#define TR_4_set8x4(in, sstep)                                                 \
    LOAD8x8_E(src, in, sstep);                                                 \
    e6 = _mm_unpacklo_epi16(src0, src2);                                       \
    e7 = _mm_unpacklo_epi16(src1, src3);                                       \
    COMPUTE4x4_LO();                                                           \
    e6 = _mm_unpackhi_epi16(src0, src2);                                       \
    e7 = _mm_unpackhi_epi16(src1, src3);                                       \
    COMPUTE4x4_HI()

#define TR_COMPUTE8x8(e0, e1, idx)                                            \
    tmp1 = _mm_load_si128((__m128i *) transform8x8[idx]);                      \
    tmp3 = _mm_load_si128((__m128i *) transform8x8[idx+1]);                    \
    tmp0 = _mm_madd_epi16(src0, tmp1);                                         \
    tmp1 = _mm_madd_epi16(src1, tmp1);                                         \
    tmp2 = _mm_madd_epi16(src2, tmp3);                                         \
    tmp3 = _mm_madd_epi16(src3, tmp3);                                         \
    tmp2 = _mm_add_epi32(tmp0, tmp2);                                          \
    tmp3 = _mm_add_epi32(tmp1, tmp3);                                          \
    tmp0 = _mm_add_epi32(e0, tmp2);                                            \
    tmp1 = _mm_add_epi32(e1, tmp3);                                            \
    tmp3 = _mm_sub_epi32(e1, tmp3);                                            \
    tmp2 = _mm_sub_epi32(e0, tmp2)
#define TR_8(dst, dst_stride, in, sstep, assign)                               \
    TR_4_set8x4(in, 2 * sstep);                                                \
    LOAD8x8_O(src, in, sstep);                                                 \
    TR_COMPUTE8x8(e0, e7, 0);                                                  \
    SCALE4x4(e0, e7, tmp0, tmp1, tmp2, tmp3);                                  \
    TR_COMPUTE8x8(e1, e6, 2);                                                  \
    SCALE4x4(e1, e6, tmp0, tmp1, tmp2, tmp3);                                  \
    TR_COMPUTE8x8(e2, e5, 4);                                                  \
    SCALE4x4(e2, e5, tmp0, tmp1, tmp2, tmp3);                                  \
    TR_COMPUTE8x8(e3, e4, 6);                                                  \
    SCALE4x4(e3, e4, tmp0, tmp1, tmp2, tmp3);                                  \
    TRANSPOSE8x8_16_S(dst, dst_stride, e, assign)

#define TR_8_1( dst, dst_stride, src)    TR_8( dst, dst_stride, src,  8, SAVE_8x16)
#define TR_8_2( dst, dst_stride, src, D) TR_8( dst, dst_stride, src,  8, ADD_AND_SAVE_8x ## D)

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_transform_XxX_add_X_sse4
////////////////////////////////////////////////////////////////////////////////
#define TRANSFORM_ADD(H, D)                                                    \
void ff_hevc_transform_ ## H ## x ## H ## _add_ ## D ## _sse4 (              \
    uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride) {                       \
    int16_t tmp[H*H];                                                          \
    int16_t *p_dst = tmp;                                                      \
    int16_t *src   = coeffs;                                                   \
    int      shift = 7;                                                        \
    int      add   = 1 << (shift - 1);                                         \
    __m128i src0, src1, src2, src3;                                            \
    __m128i tmp0, tmp1, tmp2, tmp3;                                            \
    __m128i e0, e1, e2, e3, e4, e5, e6, e7;                                    \
    TR_ ## H ## _1(p_dst, H, src);                                             \
    p_dst = tmp;                                                               \
    shift   = 20 - D;                                                          \
    add     = 1 << (shift - 1);                                                \
    {                                                                          \
        INIT_ ## D();                                                          \
        TR_ ## H ## _2(dst, stride, tmp, D);                                   \
    }                                                                          \
}
TRANSFORM_ADD(4,  8);
TRANSFORM_ADD(4, 10);
TRANSFORM_ADD(8,  8);
TRANSFORM_ADD(8, 10);

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_transform_16x16_add_X_sse4
////////////////////////////////////////////////////////////////////////////////
#define SCALE16x16_SAVE(dst, dst_stride, src1, src2, j)                       \
    e0   = _mm_loadu_si128((__m128i *) &o16[j*8+0]);                           \
    e7   = _mm_loadu_si128((__m128i *) &o16[j*8+4]);                           \
    tmp4 = _mm_add_epi32(src1, e0);                                            \
    src1 = _mm_sub_epi32(src1, e0);                                            \
    e0   = _mm_add_epi32(src2, e7);                                            \
    src2 = _mm_sub_epi32(src2, e7);                                            \
    SCALE4x4(e0, e7, tmp4, e0, src1, src2);                                  \
    _mm_storeu_si128((__m128i *) &dst[dst_stride*(             j)]  , e0);     \
    _mm_storeu_si128((__m128i *) &dst[dst_stride*(dst_stride-1-j)]  , e7)
#define TR_8_set(dst, dst_stride, in, sstep)                                   \
    TR_4_set8x4(in, 2 * sstep);                                                \
    LOAD8x8_O(src, in, sstep);                                                 \
    TR_COMPUTE8x8(e0, e7, 0);                                                  \
    SCALE16x16_SAVE(dst, dst_stride, tmp0, tmp1, 0);                           \
    SCALE16x16_SAVE(dst, dst_stride, tmp2, tmp3, 7);                           \
    TR_COMPUTE8x8(e1, e6, 2);                                                  \
    SCALE16x16_SAVE(dst, dst_stride, tmp0, tmp1, 1);                           \
    SCALE16x16_SAVE(dst, dst_stride, tmp2, tmp3, 6);                           \
    TR_COMPUTE8x8(e2, e5, 4);                                                  \
    SCALE16x16_SAVE(dst, dst_stride, tmp0, tmp1, 2);                           \
    SCALE16x16_SAVE(dst, dst_stride, tmp2, tmp3, 5);                           \
    TR_COMPUTE8x8(e3, e4, 6);                                                  \
    SCALE16x16_SAVE(dst, dst_stride, tmp0, tmp1, 3);                           \
    SCALE16x16_SAVE(dst, dst_stride, tmp2, tmp3, 4)

#define TR_COMPUTE16x16(dst1, dst2,src0, src1, src2, src3, i, j)               \
    tmp1 = _mm_load_si128((__m128i *) transform16x16_1[i  ][j]);               \
    tmp3 = _mm_load_si128((__m128i *) transform16x16_1[i+1][j]);               \
    tmp0 = _mm_madd_epi16(src0, tmp1);                                         \
    tmp1 = _mm_madd_epi16(src1, tmp1);                                         \
    tmp2 = _mm_madd_epi16(src2, tmp3);                                         \
    tmp3 = _mm_madd_epi16(src3, tmp3);                                         \
    dst1 = _mm_add_epi32(tmp0, tmp2);                                          \
    dst2 = _mm_add_epi32(tmp1, tmp3)
#define TR_COMPUTE16x16_LO(j)                                                  \
    TR_COMPUTE16x16(src0, src1, e0, e1, e2, e3, 0, j)
#define TR_COMPUTE16x16_HI(j)                                                  \
    TR_COMPUTE16x16(src2, src3, e4, e5, e6, e7, 2, j)
#define TR_16(dst, dst_stride, in, sstep)                                      \
    {                                                                          \
        int i;                                                                 \
        int o16[8*8];                                                          \
        LOAD16x16_O(e, in, sstep);                                             \
        for (i = 0; i < 8; i++) {                                              \
            TR_COMPUTE16x16_LO(i);                                             \
            TR_COMPUTE16x16_HI(i);                                             \
            tmp2 = _mm_add_epi32(src0, src2);                                  \
            tmp3 = _mm_add_epi32(src1, src3);                                  \
            SAVE_8x32(o16, 8, tmp2, tmp3, i);                                  \
        }                                                                      \
        TR_8_set(dst, dst_stride, in, 2 * sstep);                             \
    }

////////////////////////////////////////////////////////////////////////////////
// ff_hevc_transform_XxX_add_X_sse4
////////////////////////////////////////////////////////////////////////////////
#define TRANSFORM_ADD2(H, D)                                                \
void ff_hevc_transform_ ## H ## x ## H ## _add_ ## D ## _sse4 (              \
    uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride) {                       \
    int i, k, add;                                                             \
    int      shift = 7;                                                        \
    int16_t *src   = coeffs;                                                   \
    int16_t  tmp[H*H];                                                         \
    int16_t  tmp_2[H*H];                                                       \
    int16_t *p_dst;                                                            \
    __m128i src0, src1, src2, src3;                                            \
    __m128i tmp0, tmp1, tmp2, tmp3, tmp4;                                      \
    __m128i e0, e1, e2, e3, e4, e5, e6, e7;                                    \
    for (k = 0; k < 2; k++) {                                                  \
        p_dst = tmp;                                                           \
        add   = 1 << (shift - 1);                                              \
        for (i = 0; i < H; i+=8) {                                             \
            TR_16(p_dst, H, src, H);                                           \
            src   += 8;                                                        \
            p_dst  = tmp + 8;                                                  \
        }                                                                      \
        if (k == 0) {                                                          \
            TRANSPOSE ## H ## x ## H ## _LS(tmp_2, H, tmp, H, SAVE_8x16);     \
            src     = tmp_2;                                                   \
            shift   = 20 - D;                                                  \
        } else {                                                               \
            INIT_ ## D();                                                      \
            TRANSPOSE ## H ## x ## H ## _LS(dst, stride, tmp, 16, ADD_AND_SAVE_8x ## D);\
        }                                                                      \
    }                                                                          \
}

TRANSFORM_ADD2(16,  8);
TRANSFORM_ADD2(16, 10);

#else


void ff_hevc_transform_4x4_add_8_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    uint8_t shift_2nd = 12; // 20 - Bit depth
    uint16_t add_2nd = 1 << 11; //(1 << (shift_2nd - 1))

    uint8_t *dst = (uint8_t*) _dst;
    ptrdiff_t stride = _stride;
    int16_t *src = coeffs;

    __m128i S0, S8, m128iAdd, m128Tmp, E1, E2, O1, O2, m128iA, m128iD, m128iTmp1,m128iTmp2;
    S0 = _mm_load_si128((__m128i *) (src));
    S8 = _mm_load_si128((__m128i *) (src + 8));
    m128iAdd = _mm_set1_epi32(add_1st);

    m128Tmp = _mm_unpacklo_epi16(S0, S8);
    E1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[0])));
    E1 = _mm_add_epi32(E1, m128iAdd);

    E2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[1])));
    E2 = _mm_add_epi32(E2, m128iAdd);

    m128Tmp = _mm_unpackhi_epi16(S0, S8);
    O1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[2])));
    O2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[3])));

    m128iA = _mm_add_epi32(E1, O1);
    m128iA = _mm_srai_epi32(m128iA, shift_1st);        // Sum = Sum >> iShiftNum
    m128Tmp = _mm_add_epi32(E2, O2);
    m128Tmp = _mm_srai_epi32(m128Tmp, shift_1st);      // Sum = Sum >> iShiftNum
    m128iA = _mm_packs_epi32(m128iA, m128Tmp);

    m128iD = _mm_sub_epi32(E2, O2);
    m128iD = _mm_srai_epi32(m128iD, shift_1st);        // Sum = Sum >> iShiftNum

    m128Tmp = _mm_sub_epi32(E1, O1);
    m128Tmp = _mm_srai_epi32(m128Tmp, shift_1st);      // Sum = Sum >> iShiftNum

    m128iD = _mm_packs_epi32(m128iD, m128Tmp);

    S0 = _mm_unpacklo_epi16(m128iA, m128iD);
    S8 = _mm_unpackhi_epi16(m128iA, m128iD);

    m128iA = _mm_unpacklo_epi16(S0, S8);
    m128iD = _mm_unpackhi_epi16(S0, S8);

    /*  ##########################  */

    m128iAdd = _mm_set1_epi32(add_2nd);
    m128Tmp = _mm_unpacklo_epi16(m128iA, m128iD);
    E1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[0])));
    E1 = _mm_add_epi32(E1, m128iAdd);

    E2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[1])));
    E2 = _mm_add_epi32(E2, m128iAdd);

    m128Tmp = _mm_unpackhi_epi16(m128iA, m128iD);
    O1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[2])));
    O2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[3])));

    m128iA = _mm_add_epi32(E1, O1);
    m128iA = _mm_srai_epi32(m128iA, shift_2nd);
    m128Tmp = _mm_add_epi32(E2, O2);
    m128Tmp = _mm_srai_epi32(m128Tmp, shift_2nd);
    m128iA = _mm_packs_epi32(m128iA, m128Tmp);

    m128iD = _mm_sub_epi32(E2, O2);
    m128iD = _mm_srai_epi32(m128iD, shift_2nd);

    m128Tmp = _mm_sub_epi32(E1, O1);
    m128Tmp = _mm_srai_epi32(m128Tmp, shift_2nd);

    m128iD = _mm_packs_epi32(m128iD, m128Tmp);

    S0 = _mm_move_epi64(m128iA); //contains row 0
    S8 = _mm_move_epi64(m128iD); //row 2
    m128iA = _mm_srli_si128(m128iA, 8); // row 1
    m128iD = _mm_srli_si128(m128iD, 8); // row 3
    m128iTmp1 = _mm_unpacklo_epi16(S0, m128iA);
    m128iTmp2 = _mm_unpacklo_epi16(S8, m128iD);
    S0 = _mm_unpacklo_epi32(m128iTmp1, m128iTmp2);
    S8 = _mm_unpackhi_epi32(m128iTmp1, m128iTmp2);

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(S0, m128iA);	//contains first 4 values
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    _mm_maskmoveu_si128(m128iTmp1, m128iTmp2, (char*) dst);

    dst += stride;

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(_mm_srli_si128(S0, 8), m128iA);
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    _mm_maskmoveu_si128(m128iTmp1, m128iTmp2, (char*) dst);

    dst += stride;

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(S8, m128iA);
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    _mm_maskmoveu_si128(m128iTmp1, m128iTmp2, (char*) dst);

    dst += stride;

    m128iA = _mm_loadl_epi64((__m128i *) dst);
    m128iA = _mm_unpacklo_epi8(m128iA, _mm_setzero_si128());
    m128iTmp1 = _mm_adds_epi16(_mm_srli_si128(S8, 8), m128iA);
    m128iTmp1 = _mm_packus_epi16(m128iTmp1, _mm_setzero_si128());
    _mm_maskmoveu_si128(m128iTmp1, m128iTmp2, (char*) dst);
}

void ff_hevc_transform_4x4_add_10_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    int i;
    uint8_t shift_2nd = 10; // 20 - Bit depth
    uint16_t add_2nd = 1 << 9; //(1 << (shift_2nd - 1))

    uint16_t *dst = (uint16_t*) _dst;
    ptrdiff_t stride = _stride/2;
    int16_t *src = coeffs;

    int j;
        __m128i S0, S8, m128iAdd, m128Tmp, E1, E2, O1, O2, m128iA, m128iD;
        S0 = _mm_load_si128((__m128i *) (src));
        S8 = _mm_load_si128((__m128i *) (src + 8));
        m128iAdd = _mm_set1_epi32(add_1st);

        m128Tmp = _mm_unpacklo_epi16(S0, S8);
        E1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[0])));
        E1 = _mm_add_epi32(E1, m128iAdd);

        E2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[1])));
        E2 = _mm_add_epi32(E2, m128iAdd);

        m128Tmp = _mm_unpackhi_epi16(S0, S8);
        O1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[2])));
        O2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[3])));

        m128iA = _mm_add_epi32(E1, O1);
        m128iA = _mm_srai_epi32(m128iA, shift_1st);        // Sum = Sum >> iShiftNum
        m128Tmp = _mm_add_epi32(E2, O2);
        m128Tmp = _mm_srai_epi32(m128Tmp, shift_1st);      // Sum = Sum >> iShiftNum
        m128iA = _mm_packs_epi32(m128iA, m128Tmp);

        m128iD = _mm_sub_epi32(E2, O2);
        m128iD = _mm_srai_epi32(m128iD, shift_1st);        // Sum = Sum >> iShiftNum

        m128Tmp = _mm_sub_epi32(E1, O1);
        m128Tmp = _mm_srai_epi32(m128Tmp, shift_1st);      // Sum = Sum >> iShiftNum

        m128iD = _mm_packs_epi32(m128iD, m128Tmp);

        S0 = _mm_unpacklo_epi16(m128iA, m128iD);
        S8 = _mm_unpackhi_epi16(m128iA, m128iD);

        m128iA = _mm_unpacklo_epi16(S0, S8);
        m128iD = _mm_unpackhi_epi16(S0, S8);

        /*  ##########################  */

        m128iAdd = _mm_set1_epi32(add_2nd);
        m128Tmp = _mm_unpacklo_epi16(m128iA, m128iD);
        E1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[0])));
        E1 = _mm_add_epi32(E1, m128iAdd);

        E2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[1])));
        E2 = _mm_add_epi32(E2, m128iAdd);

        m128Tmp = _mm_unpackhi_epi16(m128iA, m128iD);
        O1 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[2])));
        O2 = _mm_madd_epi16(m128Tmp, _mm_load_si128((__m128i *) (transform4x4[3])));

        m128iA = _mm_add_epi32(E1, O1);
        m128iA = _mm_srai_epi32(m128iA, shift_2nd);
        m128Tmp = _mm_add_epi32(E2, O2);
        m128Tmp = _mm_srai_epi32(m128Tmp, shift_2nd);
        m128iA = _mm_packs_epi32(m128iA, m128Tmp);

        m128iD = _mm_sub_epi32(E2, O2);
        m128iD = _mm_srai_epi32(m128iD, shift_2nd);

        m128Tmp = _mm_sub_epi32(E1, O1);
        m128Tmp = _mm_srai_epi32(m128Tmp, shift_2nd);

        m128iD = _mm_packs_epi32(m128iD, m128Tmp);
        _mm_storeu_si128((__m128i *) (src), m128iA);
        _mm_storeu_si128((__m128i *) (src + 8), m128iD);
        j = 0;
        for (i = 0; i < 2; i++) {
            dst[0] = av_clip_uintp2(dst[0] + src[j],10);
            dst[1] = av_clip_uintp2(dst[1] + src[j + 4],10);
            dst[2] = av_clip_uintp2(dst[2] + src[j + 8],10);
            dst[3] = av_clip_uintp2(dst[3] + src[j + 12],10);
            j += 1;
            dst += stride;
            dst[0] = av_clip_uintp2(dst[0] + src[j],10);
            dst[1] = av_clip_uintp2(dst[1] + src[j + 4],10);
            dst[2] = av_clip_uintp2(dst[2] + src[j + 8],10);
            dst[3] = av_clip_uintp2(dst[3] + src[j + 12],10);
            j += 1;
            dst += stride;
        }
}


void ff_hevc_transform_8x8_add_8_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    const uint8_t  shift_2nd = 12; // 20 - Bit depth
    const uint16_t add_2nd = 1 << 11; //(1 << (shift_2nd - 1))
    const __m128i  T8      = _mm_load_si128((__m128i *) (transform8x8[8]));
    const __m128i  T9      = _mm_load_si128((__m128i *) (transform8x8[9]));
    const __m128i  T10     = _mm_load_si128((__m128i *) (transform8x8[10]));
    const __m128i  T11     = _mm_load_si128((__m128i *) (transform8x8[11]));
    ptrdiff_t stride = _stride / sizeof(uint8_t);
    uint8_t *dst = (uint8_t*) _dst;
    int16_t *src = coeffs;
    __m128i *m128iS_ptr_in, *m128iS_ptr_out;
    __m128i El[4], Eh[4];
    int16_t tmp[8*8];
    __m128i m128iAdd, m128Tmp0, m128Tmp1, m128Tmp2, m128Tmp3, EE0l, EE1l, E00l, E01l, EE0h, EE1h, E00h, E01h;
    int j, k;
    uint8_t shift;

    m128iAdd = _mm_set1_epi32(add_1st);

    m128iS_ptr_in = (__m128i *) (src);
    m128iS_ptr_out = (__m128i *) (tmp);
    shift = shift_1st;

    for (j = 0; j < 2; j++ ) {
        int kk = 0;

        m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[0], m128iS_ptr_in[4]);
        m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[0], m128iS_ptr_in[4]);

        m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[2], m128iS_ptr_in[6]);
        m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[2], m128iS_ptr_in[6]);

        /*    -------     */
        EE0l = _mm_madd_epi16(m128Tmp0, T8);
        EE0h = _mm_madd_epi16(m128Tmp1, T8);

        EE1l = _mm_madd_epi16(m128Tmp0, T9);
        EE1h = _mm_madd_epi16(m128Tmp1, T9);

        /*    -------     */
        E00l = _mm_madd_epi16(m128Tmp2, T10);
        E00h = _mm_madd_epi16(m128Tmp3, T10);
        E01l = _mm_madd_epi16(m128Tmp2, T11);
        E01h = _mm_madd_epi16(m128Tmp3, T11);

        El[0] = _mm_add_epi32(EE0l, E00l);
        Eh[0] = _mm_add_epi32(EE0h, E00h);
        El[3] = _mm_sub_epi32(EE0l, E00l);
        Eh[3] = _mm_sub_epi32(EE0h, E00h);

        El[1] = _mm_add_epi32(EE1l, E01l);
        Eh[1] = _mm_add_epi32(EE1h, E01h);
        El[2] = _mm_sub_epi32(EE1l, E01l);
        Eh[2] = _mm_sub_epi32(EE1h, E01h);

        m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[1], m128iS_ptr_in[3]);
        m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[1], m128iS_ptr_in[3]);

        m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[5], m128iS_ptr_in[7]);
        m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[5], m128iS_ptr_in[7]);


        /*  Compute Ok  */
        for (k = 0; k < 4; k++) {
            const __m128i T0k = _mm_load_si128((__m128i *) (transform8x8[kk++]));
            const __m128i T1k = _mm_load_si128((__m128i *) (transform8x8[kk++]));
            __m128i Ol, Oh;
            __m128i _El, _Eh;
            __m128i xmm0, xmm1;

            xmm0  = _mm_madd_epi16(m128Tmp0, T0k);
            xmm1  = _mm_madd_epi16(m128Tmp2, T1k);
            Ol    = _mm_add_epi32(xmm0, xmm1);

            xmm0  = _mm_madd_epi16(m128Tmp1, T0k);
            xmm1  = _mm_madd_epi16(m128Tmp3, T1k);
            Oh    = _mm_add_epi32(xmm0, xmm1);

            _El = _mm_add_epi32(El[k], m128iAdd);
            _Eh = _mm_add_epi32(Eh[k], m128iAdd);
            m128iS_ptr_out[k]      = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(_El, Ol), shift),
                                                     _mm_srai_epi32(_mm_add_epi32(_Eh, Oh), shift));
            
            m128iS_ptr_out[7 - k]  = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(_El, Ol), shift),
                                                     _mm_srai_epi32(_mm_sub_epi32(_Eh, Oh), shift));
            
        }
        transpose_8x8(m128iS_ptr_out, 1);
        shift = shift_2nd;
        m128iAdd = _mm_set1_epi32(add_2nd);
        m128iS_ptr_in = (__m128i *) (tmp);
        m128iS_ptr_out = (__m128i *) coeffs;

    }

    m128Tmp0=_mm_setzero_si128();
    m128iS_ptr_out = (__m128i *) coeffs;

    for (k = 0; k < 8; k ++) {
        __m128i xmm0, xmm1, xmm2;
        xmm0  = _mm_loadl_epi64((__m128i*)(dst)); //16 values
        xmm1  = _mm_adds_epi16(m128iS_ptr_out[k], _mm_unpacklo_epi8(xmm0, m128Tmp0));
        xmm2 = _mm_packus_epi16(xmm1, m128Tmp0);
        _mm_storel_epi64((__m128i*)(dst), xmm2);
        dst += stride;
    }

}

void ff_hevc_transform_8x8_add_10_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    int i;
    uint16_t *dst = (uint16_t*) _dst;
    ptrdiff_t stride = _stride / sizeof(uint16_t);
    int16_t *src = coeffs;
    uint8_t shift_2nd = 10; // 20 - Bit depth
    uint16_t add_2nd = 1 << 9; //(1 << (shift_2nd - 1))

    __m128i m128iS0, m128iS1, m128iS2, m128iS3, m128iS4, m128iS5, m128iS6,
            m128iS7, m128iAdd, m128Tmp0, m128Tmp1, m128Tmp2, m128Tmp3, E0h, E1h,
            E2h, E3h, E0l, E1l, E2l, E3l, O0h, O1h, O2h, O3h, O0l, O1l, O2l,
            O3l, EE0l, EE1l, E00l, E01l, EE0h, EE1h, E00h, E01h;
    int j;
    m128iAdd = _mm_set1_epi32(add_1st);

    m128iS1 = _mm_load_si128((__m128i *) (src + 8));
    m128iS3 = _mm_load_si128((__m128i *) (src + 24));
    m128Tmp0 = _mm_unpacklo_epi16(m128iS1, m128iS3);
    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[0])));
    m128Tmp1 = _mm_unpackhi_epi16(m128iS1, m128iS3);
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[0])));
    m128iS5 = _mm_load_si128((__m128i *) (src + 40));
    m128iS7 = _mm_load_si128((__m128i *) (src + 56));
    m128Tmp2 = _mm_unpacklo_epi16(m128iS5, m128iS7);
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[1])));
    m128Tmp3 = _mm_unpackhi_epi16(m128iS5, m128iS7);
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[1])));
    O0l = _mm_add_epi32(E1l, E2l);
    O0h = _mm_add_epi32(E1h, E2h);

    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[2])));
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[2])));
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[3])));
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[3])));

    O1l = _mm_add_epi32(E1l, E2l);
    O1h = _mm_add_epi32(E1h, E2h);

    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[4])));
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[4])));
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[5])));
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[5])));
    O2l = _mm_add_epi32(E1l, E2l);
    O2h = _mm_add_epi32(E1h, E2h);

    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[6])));
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[6])));
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[7])));
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[7])));
    O3h = _mm_add_epi32(E1h, E2h);
    O3l = _mm_add_epi32(E1l, E2l);

    /*    -------     */

    m128iS0 = _mm_load_si128((__m128i *) (src + 0));
    m128iS4 = _mm_load_si128((__m128i *) (src + 32));
    m128Tmp0 = _mm_unpacklo_epi16(m128iS0, m128iS4);
    EE0l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[8])));
    m128Tmp1 = _mm_unpackhi_epi16(m128iS0, m128iS4);
    EE0h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[8])));

    EE1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[9])));
    EE1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[9])));

    /*    -------     */

    m128iS2 = _mm_load_si128((__m128i *) (src + 16));
    m128iS6 = _mm_load_si128((__m128i *) (src + 48));
    m128Tmp0 = _mm_unpacklo_epi16(m128iS2, m128iS6);
    E00l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[10])));
    m128Tmp1 = _mm_unpackhi_epi16(m128iS2, m128iS6);
    E00h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[10])));
    E01l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[11])));
    E01h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[11])));
    E0l = _mm_add_epi32(EE0l, E00l);
    E0l = _mm_add_epi32(E0l, m128iAdd);
    E0h = _mm_add_epi32(EE0h, E00h);
    E0h = _mm_add_epi32(E0h, m128iAdd);
    E3l = _mm_sub_epi32(EE0l, E00l);
    E3l = _mm_add_epi32(E3l, m128iAdd);
    E3h = _mm_sub_epi32(EE0h, E00h);
    E3h = _mm_add_epi32(E3h, m128iAdd);

    E1l = _mm_add_epi32(EE1l, E01l);
    E1l = _mm_add_epi32(E1l, m128iAdd);
    E1h = _mm_add_epi32(EE1h, E01h);
    E1h = _mm_add_epi32(E1h, m128iAdd);
    E2l = _mm_sub_epi32(EE1l, E01l);
    E2l = _mm_add_epi32(E2l, m128iAdd);
    E2h = _mm_sub_epi32(EE1h, E01h);
    E2h = _mm_add_epi32(E2h, m128iAdd);
    m128iS0 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E0l, O0l), shift_1st),
            _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift_1st));
    m128iS1 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E1l, O1l), shift_1st),
            _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift_1st));
    m128iS2 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E2l, O2l), shift_1st),
            _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift_1st));
    m128iS3 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E3l, O3l), shift_1st),
            _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift_1st));
    m128iS4 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E3l, O3l), shift_1st),
            _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift_1st));
    m128iS5 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E2l, O2l), shift_1st),
            _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift_1st));
    m128iS6 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E1l, O1l), shift_1st),
            _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift_1st));
    m128iS7 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E0l, O0l), shift_1st),
            _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift_1st));
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
    m128iS0 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp1);
    m128iS1 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp1);
    m128Tmp2 = _mm_unpackhi_epi16(E0l, E2l);
    m128Tmp3 = _mm_unpackhi_epi16(E1l, E3l);
    m128iS2 = _mm_unpacklo_epi16(m128Tmp2, m128Tmp3);
    m128iS3 = _mm_unpackhi_epi16(m128Tmp2, m128Tmp3);
    m128Tmp0 = _mm_unpacklo_epi16(O0l, O2l);
    m128Tmp1 = _mm_unpacklo_epi16(O1l, O3l);
    m128iS4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp1);
    m128iS5 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp1);
    m128Tmp2 = _mm_unpackhi_epi16(O0l, O2l);
    m128Tmp3 = _mm_unpackhi_epi16(O1l, O3l);
    m128iS6 = _mm_unpacklo_epi16(m128Tmp2, m128Tmp3);
    m128iS7 = _mm_unpackhi_epi16(m128Tmp2, m128Tmp3);

    m128iAdd = _mm_set1_epi32(add_2nd);

    m128Tmp0 = _mm_unpacklo_epi16(m128iS1, m128iS3);
    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[0])));
    m128Tmp1 = _mm_unpackhi_epi16(m128iS1, m128iS3);
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[0])));
    m128Tmp2 = _mm_unpacklo_epi16(m128iS5, m128iS7);
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[1])));
    m128Tmp3 = _mm_unpackhi_epi16(m128iS5, m128iS7);
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[1])));
    O0l = _mm_add_epi32(E1l, E2l);
    O0h = _mm_add_epi32(E1h, E2h);
    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[2])));
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[2])));
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[3])));
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[3])));
    O1l = _mm_add_epi32(E1l, E2l);
    O1h = _mm_add_epi32(E1h, E2h);
    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[4])));
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[4])));
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[5])));
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[5])));
    O2l = _mm_add_epi32(E1l, E2l);
    O2h = _mm_add_epi32(E1h, E2h);
    E1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[6])));
    E1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[6])));
    E2l = _mm_madd_epi16(m128Tmp2,
            _mm_load_si128((__m128i *) (transform8x8[7])));
    E2h = _mm_madd_epi16(m128Tmp3,
            _mm_load_si128((__m128i *) (transform8x8[7])));
    O3h = _mm_add_epi32(E1h, E2h);
    O3l = _mm_add_epi32(E1l, E2l);

    m128Tmp0 = _mm_unpacklo_epi16(m128iS0, m128iS4);
    EE0l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[8])));
    m128Tmp1 = _mm_unpackhi_epi16(m128iS0, m128iS4);
    EE0h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[8])));
    EE1l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[9])));
    EE1h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[9])));

    m128Tmp0 = _mm_unpacklo_epi16(m128iS2, m128iS6);
    E00l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[10])));
    m128Tmp1 = _mm_unpackhi_epi16(m128iS2, m128iS6);
    E00h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[10])));
    E01l = _mm_madd_epi16(m128Tmp0,
            _mm_load_si128((__m128i *) (transform8x8[11])));
    E01h = _mm_madd_epi16(m128Tmp1,
            _mm_load_si128((__m128i *) (transform8x8[11])));
    E0l = _mm_add_epi32(EE0l, E00l);
    E0l = _mm_add_epi32(E0l, m128iAdd);
    E0h = _mm_add_epi32(EE0h, E00h);
    E0h = _mm_add_epi32(E0h, m128iAdd);
    E3l = _mm_sub_epi32(EE0l, E00l);
    E3l = _mm_add_epi32(E3l, m128iAdd);
    E3h = _mm_sub_epi32(EE0h, E00h);
    E3h = _mm_add_epi32(E3h, m128iAdd);
    E1l = _mm_add_epi32(EE1l, E01l);
    E1l = _mm_add_epi32(E1l, m128iAdd);
    E1h = _mm_add_epi32(EE1h, E01h);
    E1h = _mm_add_epi32(E1h, m128iAdd);
    E2l = _mm_sub_epi32(EE1l, E01l);
    E2l = _mm_add_epi32(E2l, m128iAdd);
    E2h = _mm_sub_epi32(EE1h, E01h);
    E2h = _mm_add_epi32(E2h, m128iAdd);

    m128iS0 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E0l, O0l), shift_2nd),
            _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift_2nd));
    m128iS1 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E1l, O1l), shift_2nd),
            _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift_2nd));
    m128iS2 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E2l, O2l), shift_2nd),
            _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift_2nd));
    m128iS3 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_add_epi32(E3l, O3l), shift_2nd),
            _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift_2nd));
    m128iS4 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E3l, O3l), shift_2nd),
            _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift_2nd));
    m128iS5 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E2l, O2l), shift_2nd),
            _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift_2nd));
    m128iS6 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E1l, O1l), shift_2nd),
            _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift_2nd));
    m128iS7 = _mm_packs_epi32(
            _mm_srai_epi32(_mm_sub_epi32(E0l, O0l), shift_2nd),
            _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift_2nd));

    _mm_store_si128((__m128i *) (src), m128iS0);
    _mm_store_si128((__m128i *) (src + 8), m128iS1);
    _mm_store_si128((__m128i *) (src + 16), m128iS2);
    _mm_store_si128((__m128i *) (src + 24), m128iS3);
    _mm_store_si128((__m128i *) (src + 32), m128iS4);
    _mm_store_si128((__m128i *) (src + 40), m128iS5);
    _mm_store_si128((__m128i *) (src + 48), m128iS6);
    _mm_store_si128((__m128i *) (src + 56), m128iS7);

    j = 0;
    for (i = 0; i < 4; i++) {
        dst[0] = av_clip_uintp2(dst[0] + src[j],10);
        dst[1] = av_clip_uintp2(dst[1] + src[j + 8],10);
        dst[2] = av_clip_uintp2(dst[2] + src[j + 16],10);
        dst[3] = av_clip_uintp2(dst[3] + src[j + 24],10);
        dst[4] = av_clip_uintp2(dst[4] + src[j + 32],10);
        dst[5] = av_clip_uintp2(dst[5] + src[j + 40],10);
        dst[6] = av_clip_uintp2(dst[6] + src[j + 48],10);
        dst[7] = av_clip_uintp2(dst[7] + src[j + 56],10);
        j += 1;
        dst += stride;
        dst[0] = av_clip_uintp2(dst[0] + src[j],10);
        dst[1] = av_clip_uintp2(dst[1] + src[j + 8],10);
        dst[2] = av_clip_uintp2(dst[2] + src[j + 16],10);
        dst[3] = av_clip_uintp2(dst[3] + src[j + 24],10);
        dst[4] = av_clip_uintp2(dst[4] + src[j + 32],10);
        dst[5] = av_clip_uintp2(dst[5] + src[j + 40],10);
        dst[6] = av_clip_uintp2(dst[6] + src[j + 48],10);
        dst[7] = av_clip_uintp2(dst[7] + src[j + 56],10);
        j += 1;
        dst += stride;
    }

}


void ff_hevc_transform_16x16_add_8_sse4(uint8_t *_dst, int16_t *coeffs,
                                        ptrdiff_t _stride) {
    uint8_t shift_2nd = 12; // 20 - Bit depth
    uint16_t add_2nd = 1 << 11; //(1 << (shift_2nd - 1))
    int i, j, k;
    uint8_t *dst = (uint8_t*) _dst;
    ptrdiff_t stride = _stride / sizeof(uint8_t);
    int16_t *src = coeffs;
    int32_t shift;
    __m128i m128iAdd, m128Tmp0, m128Tmp1, m128Tmp2,
    m128Tmp3, m128Tmp4, m128Tmp5, m128Tmp6, m128Tmp7, E0h, E1h, E2h,
    E3h, E0l, E1l, E2l, E3l;

    __m128i Ol[8], O_tmp;
    __m128i Oh[8];
    __m128i El[8], E_tmp, E_tmp1;
    __m128i Eh[8];
    __m128i EEl[8];
    __m128i EEh[8];


    int16_t tmp[16*16];

    __m128i *m128iS_ptr_in;
    __m128i *m128iS_ptr_out;

    shift = shift_1st;
    m128iAdd = _mm_set1_epi32(add_1st);
    m128iS_ptr_in = (__m128i *) (src);
    m128iS_ptr_out = (__m128i *) tmp;

    for (j = 0; j < 2; j ++) {
        for (i = 0; i < 2; i ++) {


            m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 1], m128iS_ptr_in[2 * 3]);
            m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 1], m128iS_ptr_in[2 * 3]);

            m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 5], m128iS_ptr_in[2 * 7]);
            m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 5], m128iS_ptr_in[2 * 7]);

            m128Tmp4 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 9], m128iS_ptr_in[2 * 11]);
            m128Tmp5 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 9], m128iS_ptr_in[2 * 11]);

            m128Tmp6 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 13], m128iS_ptr_in[2 * 15]);
            m128Tmp7 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 13], m128iS_ptr_in[2 * 15]);

            /*  Compute Ok  */
            for (k = 0; k < 8; k++) {
                const __m128i T0k = _mm_load_si128((__m128i *) (transform16x16_1[0][k]));
                const __m128i T1k = _mm_load_si128((__m128i *) (transform16x16_1[1][k]));
                const __m128i T2k = _mm_load_si128((__m128i *) (transform16x16_1[2][k]));
                const __m128i T3k = _mm_load_si128((__m128i *) (transform16x16_1[3][k]));

                E0l  = _mm_madd_epi16(m128Tmp0, T0k);
                E1l  = _mm_madd_epi16(m128Tmp2, T1k);
                E2l  = _mm_madd_epi16(m128Tmp4, T2k);
                E3l  = _mm_madd_epi16(m128Tmp6, T3k);

                O_tmp = _mm_add_epi32(E0l, E1l);
                O_tmp = _mm_add_epi32(O_tmp, E2l);
                Ol[k] = _mm_add_epi32(O_tmp, E3l);

                E0h  = _mm_madd_epi16(m128Tmp1, T0k);
                E1h  = _mm_madd_epi16(m128Tmp3, T1k);
                E2h  = _mm_madd_epi16(m128Tmp5, T2k);
                E3h  = _mm_madd_epi16(m128Tmp7, T3k);

                O_tmp = _mm_add_epi32(E0h, E1h);
                O_tmp = _mm_add_epi32(O_tmp, E2h);
                Oh[k] = _mm_add_epi32(O_tmp, E3h);
            }
            /*  Compute EE0 and EEE */
            m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 4], m128iS_ptr_in[2 * 12]);
            m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 4], m128iS_ptr_in[2 * 12]);

            m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 0], m128iS_ptr_in[2 * 8]);
            m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 0], m128iS_ptr_in[2 * 8]);

            /*  Compute EE    */
            for (k = 0; k < 2; k++) {
                const __m128i V0k  = _mm_load_si128((__m128i *) (transform16x16_3[0][k]));
                const __m128i V1k  = _mm_load_si128((__m128i *) (transform16x16_3[1][k]));
                __m128i El_tmp;
                __m128i EEl_tmp;
                El_tmp        = _mm_madd_epi16(m128Tmp0, V0k);
                EEl_tmp       = _mm_madd_epi16(m128Tmp2, V1k);

                EEl[3 - k]    = _mm_sub_epi32(EEl_tmp, El_tmp);
                EEl[k]        = _mm_add_epi32(EEl_tmp, El_tmp);

                El_tmp        = _mm_madd_epi16(m128Tmp1,V0k);
                EEl_tmp       = _mm_madd_epi16(m128Tmp3,V1k);


                EEh[3 - k]    = _mm_sub_epi32(EEl_tmp, El_tmp);
                EEh[k]        = _mm_add_epi32(EEl_tmp, El_tmp);
            }

            /*  Compute Ek  */

            m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 2], m128iS_ptr_in[2 * 6]);
            m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 2], m128iS_ptr_in[2 * 6]);

            m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[2 * 10], m128iS_ptr_in[2 * 14]);
            m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[2 * 10], m128iS_ptr_in[2 * 14]);


            /*      Compute E       */
            for (k = 0; k < 4; k++) {
                const __m128i U0k   = _mm_load_si128((__m128i *) (transform16x16_2[0][k]));
                const __m128i U1k   = _mm_load_si128((__m128i *) (transform16x16_2[1][k]));

                E_tmp     = _mm_madd_epi16(m128Tmp0, U0k);
                E_tmp1    = _mm_add_epi32(E_tmp,
                                       _mm_madd_epi16(m128Tmp2, U1k));

                E_tmp     = _mm_add_epi32(EEl[k], E_tmp1);
                El[k]     = _mm_add_epi32(E_tmp , m128iAdd);

                E_tmp     = _mm_sub_epi32(EEl[k], E_tmp1);
                El[7 - k] = _mm_add_epi32(E_tmp , m128iAdd);

                E_tmp     = _mm_madd_epi16(m128Tmp1, U0k);
                E_tmp1    = _mm_add_epi32(E_tmp,
                                          _mm_madd_epi16(m128Tmp3, U1k));

                E_tmp     = _mm_add_epi32(EEh[k], E_tmp1);
                Eh[k]     = _mm_add_epi32(E_tmp , m128iAdd);

                E_tmp     = _mm_sub_epi32(EEh[k], E_tmp1);
                Eh[7 - k] = _mm_add_epi32(E_tmp , m128iAdd);
            }
            
            for (k = 0; k < 8; k++) {
                m128iS_ptr_out[2 * k]      = _mm_packs_epi32(_mm_srai_epi32(_mm_add_epi32(El[k], Ol[k]), shift),
                                                             _mm_srai_epi32(_mm_add_epi32(Eh[k], Oh[k]), shift));
                
                m128iS_ptr_out[15 - 2 * k] = _mm_packs_epi32(_mm_srai_epi32(_mm_sub_epi32(El[k], Ol[k]), shift),
                                                             _mm_srai_epi32(_mm_sub_epi32(Eh[k], Oh[k]), shift));
                
            }
            transpose_8x8(m128iS_ptr_out, 2);
            transpose_8x8(m128iS_ptr_out + 1, 2);
            m128iS_ptr_in ++;
            m128iS_ptr_out +=16;
        }
        shift = shift_2nd;
        m128iAdd = _mm_set1_epi32(add_2nd);
        m128iS_ptr_in = (__m128i *) (tmp);
        m128iS_ptr_out = (__m128i *) coeffs;
    }

    m128Tmp0=_mm_setzero_si128();
    m128iS_ptr_out = (__m128i *) coeffs;

    for (k = 0; k < 16; k ++) {
        __m128i tmp, tmp1, tmp2;
        E0l  = _mm_load_si128((__m128i*)(dst)); //16 values
        tmp  = _mm_adds_epi16(*m128iS_ptr_out, _mm_unpacklo_epi8(E0l, m128Tmp0));
        m128iS_ptr_out++;
        tmp1 = _mm_adds_epi16(*m128iS_ptr_out, _mm_unpackhi_epi8(E0l, m128Tmp0));
        m128iS_ptr_out++;
        tmp2 = _mm_packus_epi16(tmp, tmp1);
        _mm_store_si128((__m128i*)(dst), tmp2);
        dst += stride;
    }
}

void ff_hevc_transform_16x16_add_10_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    int i;
    uint16_t *dst = (uint16_t*) _dst;
    ptrdiff_t stride = _stride / 2;
    int16_t *src = coeffs;
    int32_t shift;
    uint8_t shift_2nd = 10; //20 - bit depth
    uint16_t add_2nd = 1 << 9; //shift - 1;
    __m128i m128iS0, m128iS1, m128iS2, m128iS3, m128iS4, m128iS5, m128iS6,
            m128iS7, m128iS8, m128iS9, m128iS10, m128iS11, m128iS12, m128iS13,
            m128iS14, m128iS15, m128iAdd, m128Tmp0, m128Tmp1, m128Tmp2,
            m128Tmp3, m128Tmp4, m128Tmp5, m128Tmp6, m128Tmp7, E0h, E1h, E2h,
            E3h, E0l, E1l, E2l, E3l, O0h, O1h, O2h, O3h, O4h, O5h, O6h, O7h,
            O0l, O1l, O2l, O3l, O4l, O5l, O6l, O7l, EE0l, EE1l, EE2l, EE3l,
            E00l, E01l, EE0h, EE1h, EE2h, EE3h, E00h, E01h;
    __m128i E4l, E5l, E6l, E7l;
    __m128i E4h, E5h, E6h, E7h;
    int j;
    m128iS0 = _mm_load_si128((__m128i *) (src));
    m128iS1 = _mm_load_si128((__m128i *) (src + 16));
    m128iS2 = _mm_load_si128((__m128i *) (src + 32));
    m128iS3 = _mm_load_si128((__m128i *) (src + 48));
    m128iS4 = _mm_loadu_si128((__m128i *) (src + 64));
    m128iS5 = _mm_load_si128((__m128i *) (src + 80));
    m128iS6 = _mm_load_si128((__m128i *) (src + 96));
    m128iS7 = _mm_load_si128((__m128i *) (src + 112));
    m128iS8 = _mm_load_si128((__m128i *) (src + 128));
    m128iS9 = _mm_load_si128((__m128i *) (src + 144));
    m128iS10 = _mm_load_si128((__m128i *) (src + 160));
    m128iS11 = _mm_load_si128((__m128i *) (src + 176));
    m128iS12 = _mm_loadu_si128((__m128i *) (src + 192));
    m128iS13 = _mm_load_si128((__m128i *) (src + 208));
    m128iS14 = _mm_load_si128((__m128i *) (src + 224));
    m128iS15 = _mm_load_si128((__m128i *) (src + 240));
    shift = shift_1st;
    m128iAdd = _mm_set1_epi32(add_1st);

    for (j = 0; j < 2; j++) {
        for (i = 0; i < 16; i += 8) {

            m128Tmp0 = _mm_unpacklo_epi16(m128iS1, m128iS3);
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][0])));
            m128Tmp1 = _mm_unpackhi_epi16(m128iS1, m128iS3);
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][0])));

            m128Tmp2 = _mm_unpacklo_epi16(m128iS5, m128iS7);
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][0])));
            m128Tmp3 = _mm_unpackhi_epi16(m128iS5, m128iS7);
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][0])));

            m128Tmp4 = _mm_unpacklo_epi16(m128iS9, m128iS11);
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][0])));
            m128Tmp5 = _mm_unpackhi_epi16(m128iS9, m128iS11);
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][0])));

            m128Tmp6 = _mm_unpacklo_epi16(m128iS13, m128iS15);
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][0])));
            m128Tmp7 = _mm_unpackhi_epi16(m128iS13, m128iS15);
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][0])));

            O0l = _mm_add_epi32(E0l, E1l);
            O0l = _mm_add_epi32(O0l, E2l);
            O0l = _mm_add_epi32(O0l, E3l);

            O0h = _mm_add_epi32(E0h, E1h);
            O0h = _mm_add_epi32(O0h, E2h);
            O0h = _mm_add_epi32(O0h, E3h);

            /* Compute O1*/
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][1])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][1])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][1])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][1])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][1])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][1])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][1])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][1])));
            O1l = _mm_add_epi32(E0l, E1l);
            O1l = _mm_add_epi32(O1l, E2l);
            O1l = _mm_add_epi32(O1l, E3l);
            O1h = _mm_add_epi32(E0h, E1h);
            O1h = _mm_add_epi32(O1h, E2h);
            O1h = _mm_add_epi32(O1h, E3h);

            /* Compute O2*/
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][2])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][2])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][2])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][2])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][2])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][2])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][2])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][2])));
            O2l = _mm_add_epi32(E0l, E1l);
            O2l = _mm_add_epi32(O2l, E2l);
            O2l = _mm_add_epi32(O2l, E3l);

            O2h = _mm_add_epi32(E0h, E1h);
            O2h = _mm_add_epi32(O2h, E2h);
            O2h = _mm_add_epi32(O2h, E3h);

            /* Compute O3*/
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][3])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][3])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][3])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][3])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][3])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][3])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][3])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][3])));

            O3l = _mm_add_epi32(E0l, E1l);
            O3l = _mm_add_epi32(O3l, E2l);
            O3l = _mm_add_epi32(O3l, E3l);

            O3h = _mm_add_epi32(E0h, E1h);
            O3h = _mm_add_epi32(O3h, E2h);
            O3h = _mm_add_epi32(O3h, E3h);

            /* Compute O4*/

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][4])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][4])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][4])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][4])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][4])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][4])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][4])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][4])));

            O4l = _mm_add_epi32(E0l, E1l);
            O4l = _mm_add_epi32(O4l, E2l);
            O4l = _mm_add_epi32(O4l, E3l);

            O4h = _mm_add_epi32(E0h, E1h);
            O4h = _mm_add_epi32(O4h, E2h);
            O4h = _mm_add_epi32(O4h, E3h);

            /* Compute O5*/
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][5])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][5])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][5])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][5])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][5])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][5])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][5])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][5])));

            O5l = _mm_add_epi32(E0l, E1l);
            O5l = _mm_add_epi32(O5l, E2l);
            O5l = _mm_add_epi32(O5l, E3l);

            O5h = _mm_add_epi32(E0h, E1h);
            O5h = _mm_add_epi32(O5h, E2h);
            O5h = _mm_add_epi32(O5h, E3h);

            /* Compute O6*/

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][6])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][6])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][6])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][6])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][6])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][6])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][6])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][6])));

            O6l = _mm_add_epi32(E0l, E1l);
            O6l = _mm_add_epi32(O6l, E2l);
            O6l = _mm_add_epi32(O6l, E3l);

            O6h = _mm_add_epi32(E0h, E1h);
            O6h = _mm_add_epi32(O6h, E2h);
            O6h = _mm_add_epi32(O6h, E3h);

            /* Compute O7*/

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][7])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][7])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][7])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_1[1][7])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][7])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform16x16_1[2][7])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][7])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform16x16_1[3][7])));

            O7l = _mm_add_epi32(E0l, E1l);
            O7l = _mm_add_epi32(O7l, E2l);
            O7l = _mm_add_epi32(O7l, E3l);

            O7h = _mm_add_epi32(E0h, E1h);
            O7h = _mm_add_epi32(O7h, E2h);
            O7h = _mm_add_epi32(O7h, E3h);

            /*  Compute E0  */

            m128Tmp0 = _mm_unpacklo_epi16(m128iS2, m128iS6);
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][0])));
            m128Tmp1 = _mm_unpackhi_epi16(m128iS2, m128iS6);
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][0])));

            m128Tmp2 = _mm_unpacklo_epi16(m128iS10, m128iS14);
            E0l = _mm_add_epi32(E0l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][0]))));
            m128Tmp3 = _mm_unpackhi_epi16(m128iS10, m128iS14);
            E0h = _mm_add_epi32(E0h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][0]))));

            /*  Compute E1  */
            E1l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][1])));
            E1h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][1])));
            E1l = _mm_add_epi32(E1l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][1]))));
            E1h = _mm_add_epi32(E1h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][1]))));

            /*  Compute E2  */
            E2l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][2])));
            E2h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][2])));
            E2l = _mm_add_epi32(E2l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][2]))));
            E2h = _mm_add_epi32(E2h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][2]))));
            /*  Compute E3  */
            E3l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][3])));
            E3h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][3])));
            E3l = _mm_add_epi32(E3l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][3]))));
            E3h = _mm_add_epi32(E3h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][3]))));

            /*  Compute EE0 and EEE */

            m128Tmp0 = _mm_unpacklo_epi16(m128iS4, m128iS12);
            E00l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][0])));
            m128Tmp1 = _mm_unpackhi_epi16(m128iS4, m128iS12);
            E00h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][0])));

            m128Tmp2 = _mm_unpacklo_epi16(m128iS0, m128iS8);
            EE0l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][0])));
            m128Tmp3 = _mm_unpackhi_epi16(m128iS0, m128iS8);
            EE0h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][0])));

            E01l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][1])));
            E01h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][1])));

            EE1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][1])));
            EE1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][1])));

            /*  Compute EE    */
            EE2l = _mm_sub_epi32(EE1l, E01l);
            EE3l = _mm_sub_epi32(EE0l, E00l);
            EE2h = _mm_sub_epi32(EE1h, E01h);
            EE3h = _mm_sub_epi32(EE0h, E00h);

            EE0l = _mm_add_epi32(EE0l, E00l);
            EE1l = _mm_add_epi32(EE1l, E01l);
            EE0h = _mm_add_epi32(EE0h, E00h);
            EE1h = _mm_add_epi32(EE1h, E01h);

            /*      Compute E       */

            E4l = _mm_sub_epi32(EE3l, E3l);
            E4l = _mm_add_epi32(E4l, m128iAdd);

            E5l = _mm_sub_epi32(EE2l, E2l);
            E5l = _mm_add_epi32(E5l, m128iAdd);

            E6l = _mm_sub_epi32(EE1l, E1l);
            E6l = _mm_add_epi32(E6l, m128iAdd);

            E7l = _mm_sub_epi32(EE0l, E0l);
            E7l = _mm_add_epi32(E7l, m128iAdd);

            E4h = _mm_sub_epi32(EE3h, E3h);
            E4h = _mm_add_epi32(E4h, m128iAdd);

            E5h = _mm_sub_epi32(EE2h, E2h);
            E5h = _mm_add_epi32(E5h, m128iAdd);

            E6h = _mm_sub_epi32(EE1h, E1h);
            E6h = _mm_add_epi32(E6h, m128iAdd);

            E7h = _mm_sub_epi32(EE0h, E0h);
            E7h = _mm_add_epi32(E7h, m128iAdd);

            E0l = _mm_add_epi32(EE0l, E0l);
            E0l = _mm_add_epi32(E0l, m128iAdd);

            E1l = _mm_add_epi32(EE1l, E1l);
            E1l = _mm_add_epi32(E1l, m128iAdd);

            E2l = _mm_add_epi32(EE2l, E2l);
            E2l = _mm_add_epi32(E2l, m128iAdd);

            E3l = _mm_add_epi32(EE3l, E3l);
            E3l = _mm_add_epi32(E3l, m128iAdd);

            E0h = _mm_add_epi32(EE0h, E0h);
            E0h = _mm_add_epi32(E0h, m128iAdd);

            E1h = _mm_add_epi32(EE1h, E1h);
            E1h = _mm_add_epi32(E1h, m128iAdd);

            E2h = _mm_add_epi32(EE2h, E2h);
            E2h = _mm_add_epi32(E2h, m128iAdd);

            E3h = _mm_add_epi32(EE3h, E3h);
            E3h = _mm_add_epi32(E3h, m128iAdd);

            m128iS0 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E0l, O0l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift));
            m128iS1 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E1l, O1l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift));
            m128iS2 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E2l, O2l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift));
            m128iS3 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E3l, O3l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift));

            m128iS4 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E4l, O4l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E4h, O4h), shift));
            m128iS5 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E5l, O5l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E5h, O5h), shift));
            m128iS6 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E6l, O6l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E6h, O6h), shift));
            m128iS7 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E7l, O7l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E7h, O7h), shift));

            m128iS15 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E0l, O0l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift));
            m128iS14 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E1l, O1l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift));
            m128iS13 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E2l, O2l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift));
            m128iS12 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E3l, O3l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift));

            m128iS11 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E4l, O4l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E4h, O4h), shift));
            m128iS10 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E5l, O5l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E5h, O5h), shift));
            m128iS9 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E6l, O6l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E6h, O6h), shift));
            m128iS8 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E7l, O7l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E7h, O7h), shift));

            if (!j) {
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
                m128iS0 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS1 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS2 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS3 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpackhi_epi16(E0l, E4l);
                m128Tmp1 = _mm_unpackhi_epi16(E1l, E5l);
                m128Tmp2 = _mm_unpackhi_epi16(E2l, E6l);
                m128Tmp3 = _mm_unpackhi_epi16(E3l, E7l);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS4 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS5 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS6 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS7 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpacklo_epi16(O0l, O4l);
                m128Tmp1 = _mm_unpacklo_epi16(O1l, O5l);
                m128Tmp2 = _mm_unpacklo_epi16(O2l, O6l);
                m128Tmp3 = _mm_unpacklo_epi16(O3l, O7l);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS8 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS9 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS10 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS11 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpackhi_epi16(O0l, O4l);
                m128Tmp1 = _mm_unpackhi_epi16(O1l, O5l);
                m128Tmp2 = _mm_unpackhi_epi16(O2l, O6l);
                m128Tmp3 = _mm_unpackhi_epi16(O3l, O7l);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS12 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS13 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS14 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS15 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                /*  */
                _mm_store_si128((__m128i *) (src + i), m128iS0);
                _mm_store_si128((__m128i *) (src + 16 + i), m128iS1);
                _mm_store_si128((__m128i *) (src + 32 + i), m128iS2);
                _mm_store_si128((__m128i *) (src + 48 + i), m128iS3);
                _mm_store_si128((__m128i *) (src + 64 + i), m128iS4);
                _mm_store_si128((__m128i *) (src + 80 + i), m128iS5);
                _mm_store_si128((__m128i *) (src + 96 + i), m128iS6);
                _mm_store_si128((__m128i *) (src + 112 + i), m128iS7);
                _mm_store_si128((__m128i *) (src + 128 + i), m128iS8);
                _mm_store_si128((__m128i *) (src + 144 + i), m128iS9);
                _mm_store_si128((__m128i *) (src + 160 + i), m128iS10);
                _mm_store_si128((__m128i *) (src + 176 + i), m128iS11);
                _mm_store_si128((__m128i *) (src + 192 + i), m128iS12);
                _mm_store_si128((__m128i *) (src + 208 + i), m128iS13);
                _mm_store_si128((__m128i *) (src + 224 + i), m128iS14);
                _mm_store_si128((__m128i *) (src + 240 + i), m128iS15);

                if (!i) {
                    m128iS0 = _mm_load_si128((__m128i *) (src + 8));
                    m128iS1 = _mm_load_si128((__m128i *) (src + 24));
                    m128iS2 = _mm_load_si128((__m128i *) (src + 40));
                    m128iS3 = _mm_load_si128((__m128i *) (src + 56));
                    m128iS4 = _mm_loadu_si128((__m128i *) (src + 72));
                    m128iS5 = _mm_load_si128((__m128i *) (src + 88));
                    m128iS6 = _mm_load_si128((__m128i *) (src + 104));
                    m128iS7 = _mm_load_si128((__m128i *) (src + 120));
                    m128iS8 = _mm_load_si128((__m128i *) (src + 136));
                    m128iS9 = _mm_load_si128((__m128i *) (src + 152));
                    m128iS10 = _mm_load_si128((__m128i *) (src + 168));
                    m128iS11 = _mm_load_si128((__m128i *) (src + 184));
                    m128iS12 = _mm_loadu_si128((__m128i *) (src + 200));
                    m128iS13 = _mm_load_si128((__m128i *) (src + 216));
                    m128iS14 = _mm_load_si128((__m128i *) (src + 232));
                    m128iS15 = _mm_load_si128((__m128i *) (src + 248));
                } else {
                    m128iS0 = _mm_load_si128((__m128i *) (src));
                    m128iS1 = _mm_load_si128((__m128i *) (src + 32));
                    m128iS2 = _mm_load_si128((__m128i *) (src + 64));
                    m128iS3 = _mm_load_si128((__m128i *) (src + 96));
                    m128iS4 = _mm_loadu_si128((__m128i *) (src + 128));
                    m128iS5 = _mm_load_si128((__m128i *) (src + 160));
                    m128iS6 = _mm_load_si128((__m128i *) (src + 192));
                    m128iS7 = _mm_load_si128((__m128i *) (src + 224));
                    m128iS8 = _mm_load_si128((__m128i *) (src + 8));
                    m128iS9 = _mm_load_si128((__m128i *) (src + 32 + 8));
                    m128iS10 = _mm_load_si128((__m128i *) (src + 64 + 8));
                    m128iS11 = _mm_load_si128((__m128i *) (src + 96 + 8));
                    m128iS12 = _mm_loadu_si128((__m128i *) (src + 128 + 8));
                    m128iS13 = _mm_load_si128((__m128i *) (src + 160 + 8));
                    m128iS14 = _mm_load_si128((__m128i *) (src + 192 + 8));
                    m128iS15 = _mm_load_si128((__m128i *) (src + 224 + 8));
                    shift = shift_2nd;
                    m128iAdd = _mm_set1_epi32(add_2nd);
                }

            } else {
                int k, m = 0;
                _mm_storeu_si128((__m128i *) (src), m128iS0);
                _mm_storeu_si128((__m128i *) (src + 8), m128iS1);
                _mm_storeu_si128((__m128i *) (src + 32), m128iS2);
                _mm_storeu_si128((__m128i *) (src + 40), m128iS3);
                _mm_storeu_si128((__m128i *) (src + 64), m128iS4);
                _mm_storeu_si128((__m128i *) (src + 72), m128iS5);
                _mm_storeu_si128((__m128i *) (src + 96), m128iS6);
                _mm_storeu_si128((__m128i *) (src + 104), m128iS7);
                _mm_storeu_si128((__m128i *) (src + 128), m128iS8);
                _mm_storeu_si128((__m128i *) (src + 136), m128iS9);
                _mm_storeu_si128((__m128i *) (src + 160), m128iS10);
                _mm_storeu_si128((__m128i *) (src + 168), m128iS11);
                _mm_storeu_si128((__m128i *) (src + 192), m128iS12);
                _mm_storeu_si128((__m128i *) (src + 200), m128iS13);
                _mm_storeu_si128((__m128i *) (src + 224), m128iS14);
                _mm_storeu_si128((__m128i *) (src + 232), m128iS15);
                dst = (uint16_t*) _dst + (i * stride);

                for (k = 0; k < 8; k++) {
                    dst[0] = av_clip_uintp2(dst[0] + src[m],10);
                    dst[1] = av_clip_uintp2(dst[1] + src[m + 8],10);
                    dst[2] = av_clip_uintp2(dst[2] + src[m + 32],10);
                    dst[3] = av_clip_uintp2(dst[3] + src[m + 40],10);
                    dst[4] = av_clip_uintp2(dst[4] + src[m + 64],10);
                    dst[5] = av_clip_uintp2(dst[5] + src[m + 72],10);
                    dst[6] = av_clip_uintp2(dst[6] + src[m + 96],10);
                    dst[7] = av_clip_uintp2(dst[7] + src[m + 104],10);

                    dst[8] = av_clip_uintp2(dst[8] + src[m + 128],10);
                    dst[9] = av_clip_uintp2(dst[9] + src[m + 136],10);
                    dst[10] = av_clip_uintp2(dst[10] + src[m + 160],10);
                    dst[11] = av_clip_uintp2(dst[11] + src[m + 168],10);
                    dst[12] = av_clip_uintp2(dst[12] + src[m + 192],10);
                    dst[13] = av_clip_uintp2(dst[13] + src[m + 200],10);
                    dst[14] = av_clip_uintp2(dst[14] + src[m + 224],10);
                    dst[15] = av_clip_uintp2(dst[15] + src[m + 232],10);
                    m += 1;
                    dst += stride;
                }
                if (!i) {
                    m128iS0 = _mm_load_si128((__m128i *) (src + 16));
                    m128iS1 = _mm_load_si128((__m128i *) (src + 48));
                    m128iS2 = _mm_load_si128((__m128i *) (src + 80));
                    m128iS3 = _mm_loadu_si128((__m128i *) (src + 112));
                    m128iS4 = _mm_load_si128((__m128i *) (src + 144));
                    m128iS5 = _mm_load_si128((__m128i *) (src + 176));
                    m128iS6 = _mm_load_si128((__m128i *) (src + 208));
                    m128iS7 = _mm_load_si128((__m128i *) (src + 240));
                    m128iS8 = _mm_load_si128((__m128i *) (src + 24));
                    m128iS9 = _mm_load_si128((__m128i *) (src + 56));
                    m128iS10 = _mm_load_si128((__m128i *) (src + 88));
                    m128iS11 = _mm_loadu_si128((__m128i *) (src + 120));
                    m128iS12 = _mm_load_si128((__m128i *) (src + 152));
                    m128iS13 = _mm_load_si128((__m128i *) (src + 184));
                    m128iS14 = _mm_load_si128((__m128i *) (src + 216));
                    m128iS15 = _mm_load_si128((__m128i *) (src + 248));
                }
            }
        }
    }

}
#endif



void ff_hevc_transform_32x32_add_8_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    uint8_t shift_2nd = 12; // 20 - Bit depth
    uint16_t add_2nd = 1 << 11; //(1 << (shift_2nd - 1))
    int i, j, k, l;
    uint8_t *dst = (uint8_t*) _dst;
    ptrdiff_t stride = _stride / sizeof(uint8_t);
    int shift;
    int16_t *src = coeffs;

    __m128i m128iAdd, m128Tmp0, m128Tmp1, m128Tmp2,
            m128Tmp3, m128Tmp4, m128Tmp5, m128Tmp6, m128Tmp7, E0h, E1h, E2h,
            E3h, E0l, E1l, E2l, E3l;
    __m128i E4l, E5l, E6l, E7l, E15l;
    __m128i E4h, E5h, E6h, E7h, E15h;
    __m128i m128Tmp8, m128Tmp9,
            m128Tmp10, m128Tmp11, m128Tmp12, m128Tmp13, m128Tmp14, m128Tmp15;

    __m128i Ol[16], Ol_tmp, Oh_tmp;
    __m128i Oh[16];

    __m128i El[16], E_tmp;
    __m128i Eh[16];
    __m128i El0[4];
    __m128i Eh0[4];
    __m128i EEl[8];
    __m128i EEh[8];

    __m128i *m128iS_ptr_in;
    __m128i *m128iS_ptr_out;
    int16_t tmp[32*32];

    shift = shift_1st;
    m128iAdd = _mm_set1_epi32(add_1st);
    m128iS_ptr_in = (__m128i *) (src);
    m128iS_ptr_out = (__m128i *) tmp;


    for (j = 0; j < 2; j++) {
        for (i = 0; i < 4; i ++) {
/*            for (l = 0; l < 8; l++) {
                m128Tmp[2 * l] = _mm_unpacklo_epi16(m128iS[4 * l + 1], m128iS[4 * l + 3]);
                m128Tmp[2 * l + 1] = _mm_unpackhi_epi16(m128iS[4 * l + 1], m128iS[4 * l + 3]);
            }

            for (k = 0; k < 16; k++) {
                Ol_tmp = _mm_setzero_si128();
                Oh_tmp = _mm_setzero_si128();
                for (l = 0; l < 8; l++) {
                    m128Tmp0 = m128Tmp[2 * l];
                    m128Tmp1 = m128Tmp[2 * l + 1];
                    E0l      = _mm_madd_epi16(m128Tmp0,
                                         _mm_load_si128((__m128i *) (transform32x32[l][k])));
                    Ol_tmp   = _mm_add_epi32(Ol_tmp, E0l);
                    E0h      = _mm_madd_epi16(m128Tmp1,
                                         _mm_load_si128((__m128i *) (transform32x32[l][k])));
                    Oh_tmp   = _mm_add_epi32(Oh_tmp, E0h);
                }
                Ol[k] = Ol_tmp;
                Oh[k] = Oh_tmp;
            }
*/

            /*  Compute Ok  */
            m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 1], m128iS_ptr_in[4 * 3]);
            m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 1], m128iS_ptr_in[4 * 3]);
            m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 5], m128iS_ptr_in[4 * 7]);
            m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 5], m128iS_ptr_in[4 * 7]);
            m128Tmp4 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 9], m128iS_ptr_in[4 * 11]);
            m128Tmp5 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 9], m128iS_ptr_in[4 * 11]);
            m128Tmp6 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 13], m128iS_ptr_in[4 * 15]);
            m128Tmp7 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 13], m128iS_ptr_in[4 * 15]);
            m128Tmp8 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 17], m128iS_ptr_in[4 * 19]);
            m128Tmp9 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 17], m128iS_ptr_in[4 * 19]);
            m128Tmp10 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 21], m128iS_ptr_in[4 * 23]);
            m128Tmp11 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 21], m128iS_ptr_in[4 * 23]);
            m128Tmp12 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 25], m128iS_ptr_in[4 * 27]);
            m128Tmp13 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 25], m128iS_ptr_in[4 * 27]);
            m128Tmp14 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 29], m128iS_ptr_in[4 * 31]);
            m128Tmp15 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 29], m128iS_ptr_in[4 * 31]);
            for (k = 0; k < 16; k++) {
                Ol_tmp = _mm_setzero_si128();
                Oh_tmp = _mm_setzero_si128();
                E0l = _mm_madd_epi16(m128Tmp0,
                                     _mm_load_si128((__m128i *) (transform32x32[0][k])));
                E0h = _mm_madd_epi16(m128Tmp1,
                                     _mm_load_si128((__m128i *) (transform32x32[0][k])));

                E1l = _mm_madd_epi16(m128Tmp2,
                                     _mm_load_si128((__m128i *) (transform32x32[1][k])));
                E1h = _mm_madd_epi16(m128Tmp3,
                                     _mm_load_si128((__m128i *) (transform32x32[1][k])));

                E2l = _mm_madd_epi16(m128Tmp4,
                                     _mm_load_si128((__m128i *) (transform32x32[2][k])));
                E2h = _mm_madd_epi16(m128Tmp5,
                                     _mm_load_si128((__m128i *) (transform32x32[2][k])));

                E3l = _mm_madd_epi16(m128Tmp6,
                                     _mm_load_si128((__m128i *) (transform32x32[3][k])));
                E3h = _mm_madd_epi16(m128Tmp7,
                                     _mm_load_si128((__m128i *) (transform32x32[3][k])));

                E4l = _mm_madd_epi16(m128Tmp8,
                                     _mm_load_si128((__m128i *) (transform32x32[4][k])));
                E4h = _mm_madd_epi16(m128Tmp9,
                                     _mm_load_si128((__m128i *) (transform32x32[4][k])));

                E5l = _mm_madd_epi16(m128Tmp10,
                                     _mm_load_si128((__m128i *) (transform32x32[5][k])));
                E5h = _mm_madd_epi16(m128Tmp11,
                                     _mm_load_si128((__m128i *) (transform32x32[5][k])));

                E6l = _mm_madd_epi16(m128Tmp12,
                                     _mm_load_si128((__m128i *) (transform32x32[6][k])));
                E6h = _mm_madd_epi16(m128Tmp13,
                                     _mm_load_si128((__m128i *) (transform32x32[6][k])));

                E7l = _mm_madd_epi16(m128Tmp14,
                                     _mm_load_si128((__m128i *) (transform32x32[7][k])));
                E7h = _mm_madd_epi16(m128Tmp15,
                                     _mm_load_si128((__m128i *) (transform32x32[7][k])));

                Ol_tmp = _mm_add_epi32(Ol_tmp, E0l);
                Ol_tmp = _mm_add_epi32(Ol_tmp, E1l);
                Ol_tmp = _mm_add_epi32(Ol_tmp, E2l);
                Ol_tmp = _mm_add_epi32(Ol_tmp, E3l);
                Ol_tmp = _mm_add_epi32(Ol_tmp, E4l);
                Ol_tmp = _mm_add_epi32(Ol_tmp, E5l);
                Ol_tmp = _mm_add_epi32(Ol_tmp, E6l);
                Ol[k] = _mm_add_epi32(Ol_tmp, E7l);
                
                Oh_tmp = _mm_add_epi32(E0h, E1h);
                Oh_tmp = _mm_add_epi32(Oh_tmp, E2h);
                Oh_tmp = _mm_add_epi32(Oh_tmp, E3h);
                Oh_tmp = _mm_add_epi32(Oh_tmp, E4h);
                Oh_tmp = _mm_add_epi32(Oh_tmp, E5h);
                Oh_tmp = _mm_add_epi32(Oh_tmp, E6h);
                Oh[k] = _mm_add_epi32(Oh_tmp, E7h);
            }


            m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 2], m128iS_ptr_in[4 * 6]);
            m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 2], m128iS_ptr_in[4 * 6]);
            m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 10], m128iS_ptr_in[4 * 14]);
            m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 10], m128iS_ptr_in[4 * 14]);
            m128Tmp4 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 18], m128iS_ptr_in[4 * 22]);
            m128Tmp5 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 18], m128iS_ptr_in[4 * 22]);
            m128Tmp6 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 26], m128iS_ptr_in[4 * 30]);
            m128Tmp7 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 26], m128iS_ptr_in[4 * 30]);

            for (k = 0; k < 8; k++) {
                E_tmp = _mm_madd_epi16(m128Tmp0,
                                     _mm_load_si128((__m128i *) (transform16x16_1[0][k])));
                E_tmp = _mm_add_epi32(E_tmp,
                                    _mm_madd_epi16(m128Tmp2,
                                                   _mm_load_si128(
                                                                  (__m128i *) (transform16x16_1[1][k]))));

                E_tmp = _mm_add_epi32(E_tmp,
                                    _mm_madd_epi16(m128Tmp4,
                                                   _mm_load_si128(
                                                                  (__m128i *) (transform16x16_1[2][k]))));

                El[k] = _mm_add_epi32(E_tmp,
                                      _mm_madd_epi16(m128Tmp6,
                                                     _mm_load_si128(
                                                                    (__m128i *) (transform16x16_1[3][k]))));

                E_tmp = _mm_madd_epi16(m128Tmp1,
                                     _mm_load_si128((__m128i *) (transform16x16_1[0][k])));

                E_tmp = _mm_add_epi32(E_tmp,
                                    _mm_madd_epi16(m128Tmp3,
                                                   _mm_load_si128(
                                                                  (__m128i *) (transform16x16_1[1][k]))));

                E_tmp = _mm_add_epi32(E_tmp,
                                    _mm_madd_epi16(m128Tmp5,
                                                   _mm_load_si128(
                                                                  (__m128i *) (transform16x16_1[2][k]))));
                Eh[k] = _mm_add_epi32(E_tmp,
                                    _mm_madd_epi16(m128Tmp7,
                                                   _mm_load_si128(
                                                                  (__m128i *) (transform16x16_1[3][k]))));
            }

            /*  Compute E*/

            /*  Compute EE0 and EEE */

            m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 4], m128iS_ptr_in[4 * 12]);
            m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 4], m128iS_ptr_in[4 * 12]);
            m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 20], m128iS_ptr_in[4 * 28]);
            m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 20], m128iS_ptr_in[4 * 28]);

            /*  Compute EE    */
            for (k = 0; k < 4; k++) {

                E_tmp = _mm_madd_epi16(m128Tmp0,
                                      _mm_load_si128((__m128i *) (transform16x16_2[0][k])));
                El0[k] = _mm_add_epi32(E_tmp,
                                     _mm_madd_epi16(m128Tmp2,
                                                    _mm_load_si128(
                                                                   (__m128i *) (transform16x16_2[1][k]))));

                E_tmp = _mm_madd_epi16(m128Tmp1,
                                      _mm_load_si128((__m128i *) (transform16x16_2[0][k])));
                Eh0[k] = _mm_add_epi32(E_tmp,
                                     _mm_madd_epi16(m128Tmp3,
                                                    _mm_load_si128(
                                                                   (__m128i *) (transform16x16_2[1][k]))));
            }

            /*  Compute EE0 and EEE */
            /*  Compute EE    */

            m128Tmp0 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 8], m128iS_ptr_in[4 * 24]);
            m128Tmp1 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 8], m128iS_ptr_in[4 * 24]);
            m128Tmp2 = _mm_unpacklo_epi16(m128iS_ptr_in[4 * 0], m128iS_ptr_in[4 * 16]);
            m128Tmp3 = _mm_unpackhi_epi16(m128iS_ptr_in[4 * 0], m128iS_ptr_in[4 * 16]);

            for (k = 0; k < 2; k++) {
                __m128i El_tmp;
                __m128i EEl_tmp;
                __m128i EEl_tmp1;
                __m128i EEl_tmp2;
                El_tmp   = _mm_madd_epi16(m128Tmp0,
                                      _mm_load_si128((__m128i *) (transform16x16_3[0][k])));
                EEl_tmp  = _mm_madd_epi16(m128Tmp2,
                                         _mm_load_si128((__m128i *) (transform16x16_3[1][k])));

                EEl_tmp1   = _mm_add_epi32(EEl_tmp, El_tmp);
                EEl_tmp2 = _mm_sub_epi32(EEl_tmp, El_tmp);

                EEl[k]   = _mm_add_epi32(EEl_tmp1, El0[k]);
                EEl[3-k] = _mm_add_epi32(EEl_tmp2, El0[3 - k]);

                EEl[4+k] = _mm_sub_epi32(EEl_tmp2, El0[3 - k]);
                EEl[7-k] = _mm_sub_epi32(EEl_tmp1, El0[k]);


                El_tmp   = _mm_madd_epi16(m128Tmp1,
                                      _mm_load_si128((__m128i *) (transform16x16_3[0][k])));

                EEl_tmp  = _mm_madd_epi16(m128Tmp3,
                                       _mm_load_si128((__m128i *) (transform16x16_3[1][k])));

                EEl_tmp1 = _mm_add_epi32(EEl_tmp, El_tmp);
                EEl_tmp2 = _mm_sub_epi32(EEl_tmp, El_tmp);

                EEh[k]   = _mm_add_epi32(EEl_tmp1, Eh0[k]);
                EEh[3-k] = _mm_add_epi32(EEl_tmp2, Eh0[3 - k]);
                EEh[4+k] = _mm_sub_epi32(EEl_tmp2, Eh0[3 - k]);
                EEh[7-k] = _mm_sub_epi32(EEl_tmp1, Eh0[k]);
             }

            /*      Compute E       */
            for (k = 0; k < 8; k++) {
                E15l = _mm_sub_epi32(EEl[k], El[k]);
                El[15 - k] = _mm_add_epi32(E15l, m128iAdd);
                E0l = _mm_add_epi32(EEl[k], El[k]);
                El[k] = _mm_add_epi32(E0l,  m128iAdd);

                E15h = _mm_sub_epi32(EEh[k], Eh[k]);
                Eh[15 - k] = _mm_add_epi32(E15h, m128iAdd);
                E0h = _mm_add_epi32(EEh[k], Eh[k]);
                Eh[k] = _mm_add_epi32(E0h,  m128iAdd);
            }

            for (l = 0; l < 2; l++) {
                for (k = 0; k < 8; k++) {
                    m128iS_ptr_out[4 * k + l] = _mm_packs_epi32(
                                                    _mm_srai_epi32(_mm_add_epi32(El[k + 8 * l], Ol[k + 8 * l]), shift),
                                                    _mm_srai_epi32(_mm_add_epi32(Eh[k + 8 * l], Oh[k + 8 * l]), shift));
                    m128iS_ptr_out[31 - l - 4 * k] = _mm_packs_epi32(
                                                         _mm_srai_epi32(_mm_sub_epi32(El[k + 8 * l], Ol[k + 8 * l]), shift),
                                                         _mm_srai_epi32(_mm_sub_epi32(Eh[k + 8 * l], Oh[k + 8 * l]), shift));
                }
            }
            transpose_8x8(m128iS_ptr_out, 4);
            transpose_8x8(m128iS_ptr_out + 1, 4);
            transpose_8x8(m128iS_ptr_out + 2, 4);
            transpose_8x8(m128iS_ptr_out + 3, 4);
            m128iS_ptr_in ++;
            m128iS_ptr_out +=32;
        }
        shift = shift_2nd;
        m128iAdd = _mm_set1_epi32(add_2nd);
        m128iS_ptr_in = (__m128i *) (tmp);
        m128iS_ptr_out = (__m128i *) coeffs;
    }

    m128Tmp0=_mm_setzero_si128();
    m128iS_ptr_out = (__m128i *) coeffs;

    for (k = 0; k < 32; k ++) {
        __m128i tmp, tmp1, tmp2;
        E0l  = _mm_load_si128((__m128i*)(dst)); //16 values
        tmp  = _mm_adds_epi16(m128iS_ptr_out[4 * k], _mm_unpacklo_epi8(E0l, m128Tmp0));
        tmp1 = _mm_adds_epi16(m128iS_ptr_out[4 * k + 1], _mm_unpackhi_epi8(E0l, m128Tmp0));
        tmp2 = _mm_packus_epi16(tmp, tmp1);
        _mm_store_si128((__m128i*)(dst), tmp2);
        E0l  = _mm_load_si128((__m128i*)(dst + 16)); //16 values
        tmp  = _mm_adds_epi16(m128iS_ptr_out[4 * k + 2], _mm_unpacklo_epi8(E0l, m128Tmp0));
        tmp1 = _mm_adds_epi16(m128iS_ptr_out[4 * k + 3], _mm_unpackhi_epi8(E0l, m128Tmp0));
        tmp2 = _mm_packus_epi16(tmp, tmp1);
        _mm_store_si128((__m128i*)(dst + 16), tmp2);
        dst += stride;
    }
}

void ff_hevc_transform_32x32_add_10_sse4(uint8_t *_dst, int16_t *coeffs,
        ptrdiff_t _stride) {
    int i, j;
    uint16_t *dst = (uint16_t*) _dst;
    ptrdiff_t stride = _stride / 2;
    int shift;
    uint8_t shift_2nd = 10; //20 - bit depth
    uint16_t add_2nd = 1<<9; //shift2 - 1
    int16_t *src = coeffs;

    __m128i m128iS0, m128iS1, m128iS2, m128iS3, m128iS4, m128iS5, m128iS6,
            m128iS7, m128iS8, m128iS9, m128iS10, m128iS11, m128iS12, m128iS13,
            m128iS14, m128iS15, m128iAdd, m128Tmp0, m128Tmp1, m128Tmp2,
            m128Tmp3, m128Tmp4, m128Tmp5, m128Tmp6, m128Tmp7, E0h, E1h, E2h,
            E3h, E0l, E1l, E2l, E3l, O0h, O1h, O2h, O3h, O4h, O5h, O6h, O7h,
            O0l, O1l, O2l, O3l, O4l, O5l, O6l, O7l, EE0l, EE1l, EE2l, EE3l,
            E00l, E01l, EE0h, EE1h, EE2h, EE3h, E00h, E01h;
    __m128i E4l, E5l, E6l, E7l, E8l, E9l, E10l, E11l, E12l, E13l, E14l, E15l;
    __m128i E4h, E5h, E6h, E7h, E8h, E9h, E10h, E11h, E12h, E13h, E14h, E15h,
            EEE0l, EEE1l, EEE0h, EEE1h;
    __m128i m128iS16, m128iS17, m128iS18, m128iS19, m128iS20, m128iS21,
            m128iS22, m128iS23, m128iS24, m128iS25, m128iS26, m128iS27,
            m128iS28, m128iS29, m128iS30, m128iS31, m128Tmp8, m128Tmp9,
            m128Tmp10, m128Tmp11, m128Tmp12, m128Tmp13, m128Tmp14, m128Tmp15,
            O8h, O9h, O10h, O11h, O12h, O13h, O14h, O15h, O8l, O9l, O10l, O11l,
            O12l, O13l, O14l, O15l, E02l, E02h, E03l, E03h, EE7l, EE6l, EE5l,
            EE4l, EE7h, EE6h, EE5h, EE4h;
    m128iS0 = _mm_load_si128((__m128i *) (src));
    m128iS1 = _mm_load_si128((__m128i *) (src + 32));
    m128iS2 = _mm_load_si128((__m128i *) (src + 64));
    m128iS3 = _mm_load_si128((__m128i *) (src + 96));
    m128iS4 = _mm_loadu_si128((__m128i *) (src + 128));
    m128iS5 = _mm_load_si128((__m128i *) (src + 160));
    m128iS6 = _mm_load_si128((__m128i *) (src + 192));
    m128iS7 = _mm_load_si128((__m128i *) (src + 224));
    m128iS8 = _mm_load_si128((__m128i *) (src + 256));
    m128iS9 = _mm_load_si128((__m128i *) (src + 288));
    m128iS10 = _mm_load_si128((__m128i *) (src + 320));
    m128iS11 = _mm_load_si128((__m128i *) (src + 352));
    m128iS12 = _mm_loadu_si128((__m128i *) (src + 384));
    m128iS13 = _mm_load_si128((__m128i *) (src + 416));
    m128iS14 = _mm_load_si128((__m128i *) (src + 448));
    m128iS15 = _mm_load_si128((__m128i *) (src + 480));
    m128iS16 = _mm_load_si128((__m128i *) (src + 512));
    m128iS17 = _mm_load_si128((__m128i *) (src + 544));
    m128iS18 = _mm_load_si128((__m128i *) (src + 576));
    m128iS19 = _mm_load_si128((__m128i *) (src + 608));
    m128iS20 = _mm_load_si128((__m128i *) (src + 640));
    m128iS21 = _mm_load_si128((__m128i *) (src + 672));
    m128iS22 = _mm_load_si128((__m128i *) (src + 704));
    m128iS23 = _mm_load_si128((__m128i *) (src + 736));
    m128iS24 = _mm_load_si128((__m128i *) (src + 768));
    m128iS25 = _mm_load_si128((__m128i *) (src + 800));
    m128iS26 = _mm_load_si128((__m128i *) (src + 832));
    m128iS27 = _mm_load_si128((__m128i *) (src + 864));
    m128iS28 = _mm_load_si128((__m128i *) (src + 896));
    m128iS29 = _mm_load_si128((__m128i *) (src + 928));
    m128iS30 = _mm_load_si128((__m128i *) (src + 960));
    m128iS31 = _mm_load_si128((__m128i *) (src + 992));

    shift = shift_1st;
    m128iAdd = _mm_set1_epi32(add_1st);

    for (j = 0; j < 2; j++) {
        for (i = 0; i < 32; i += 8) {
            m128Tmp0 = _mm_unpacklo_epi16(m128iS1, m128iS3);
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][0])));
            m128Tmp1 = _mm_unpackhi_epi16(m128iS1, m128iS3);
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][0])));

            m128Tmp2 = _mm_unpacklo_epi16(m128iS5, m128iS7);
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][0])));
            m128Tmp3 = _mm_unpackhi_epi16(m128iS5, m128iS7);
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][0])));

            m128Tmp4 = _mm_unpacklo_epi16(m128iS9, m128iS11);
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][0])));
            m128Tmp5 = _mm_unpackhi_epi16(m128iS9, m128iS11);
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][0])));

            m128Tmp6 = _mm_unpacklo_epi16(m128iS13, m128iS15);
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][0])));
            m128Tmp7 = _mm_unpackhi_epi16(m128iS13, m128iS15);
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][0])));

            m128Tmp8 = _mm_unpacklo_epi16(m128iS17, m128iS19);
            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][0])));
            m128Tmp9 = _mm_unpackhi_epi16(m128iS17, m128iS19);
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][0])));

            m128Tmp10 = _mm_unpacklo_epi16(m128iS21, m128iS23);
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][0])));
            m128Tmp11 = _mm_unpackhi_epi16(m128iS21, m128iS23);
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][0])));

            m128Tmp12 = _mm_unpacklo_epi16(m128iS25, m128iS27);
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][0])));
            m128Tmp13 = _mm_unpackhi_epi16(m128iS25, m128iS27);
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][0])));

            m128Tmp14 = _mm_unpacklo_epi16(m128iS29, m128iS31);
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][0])));
            m128Tmp15 = _mm_unpackhi_epi16(m128iS29, m128iS31);
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][0])));

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
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][1])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][1])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][1])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][1])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][1])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][1])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][1])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][1])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][1])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][1])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][1])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][1])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][1])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][1])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][1])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][1])));

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
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][2])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][2])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][2])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][2])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][2])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][2])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][2])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][2])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][2])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][2])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][2])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][2])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][2])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][2])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][2])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][2])));

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
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][3])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][3])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][3])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][3])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][3])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][3])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][3])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][3])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][3])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][3])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][3])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][3])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][3])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][3])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][3])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][3])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][4])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][4])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][4])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][4])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][4])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][4])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][4])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][4])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][4])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][4])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][4])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][4])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][4])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][4])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][4])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][4])));

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
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][5])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][5])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][5])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][5])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][5])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][5])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][5])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][5])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][5])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][5])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][5])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][5])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][5])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][5])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][5])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][5])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][6])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][6])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][6])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][6])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][6])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][6])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][6])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][6])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][6])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][6])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][6])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][6])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][6])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][6])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][6])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][6])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][7])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][7])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][7])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][7])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][7])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][7])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][7])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][7])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][7])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][7])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][7])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][7])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][7])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][7])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][7])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][7])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][8])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][8])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][8])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][8])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][8])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][8])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][8])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][8])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][8])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][8])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][8])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][8])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][8])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][8])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][8])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][8])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][9])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][9])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][9])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][9])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][9])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][9])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][9])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][9])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][9])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][9])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][9])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][9])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][9])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][9])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][9])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][9])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][10])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][10])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][10])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][10])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][10])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][10])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][10])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][10])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][10])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][10])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][10])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][10])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][10])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][10])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][10])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][10])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][11])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][11])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][11])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][11])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][11])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][11])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][11])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][11])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][11])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][11])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][11])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][11])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][11])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][11])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][11])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][11])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][12])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][12])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][12])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][12])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][12])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][12])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][12])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][12])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][12])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][12])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][12])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][12])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][12])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][12])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][12])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][12])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][13])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][13])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][13])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][13])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][13])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][13])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][13])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][13])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][13])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][13])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][13])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][13])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][13])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][13])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][13])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][13])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][14])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][14])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][14])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][14])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][14])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][14])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][14])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][14])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][14])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][14])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][14])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][14])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][14])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][14])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][14])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][14])));

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

            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform32x32[0][15])));
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform32x32[0][15])));
            E1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform32x32[1][15])));
            E1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform32x32[1][15])));
            E2l = _mm_madd_epi16(m128Tmp4,
                    _mm_load_si128((__m128i *) (transform32x32[2][15])));
            E2h = _mm_madd_epi16(m128Tmp5,
                    _mm_load_si128((__m128i *) (transform32x32[2][15])));
            E3l = _mm_madd_epi16(m128Tmp6,
                    _mm_load_si128((__m128i *) (transform32x32[3][15])));
            E3h = _mm_madd_epi16(m128Tmp7,
                    _mm_load_si128((__m128i *) (transform32x32[3][15])));

            E4l = _mm_madd_epi16(m128Tmp8,
                    _mm_load_si128((__m128i *) (transform32x32[4][15])));
            E4h = _mm_madd_epi16(m128Tmp9,
                    _mm_load_si128((__m128i *) (transform32x32[4][15])));
            E5l = _mm_madd_epi16(m128Tmp10,
                    _mm_load_si128((__m128i *) (transform32x32[5][15])));
            E5h = _mm_madd_epi16(m128Tmp11,
                    _mm_load_si128((__m128i *) (transform32x32[5][15])));
            E6l = _mm_madd_epi16(m128Tmp12,
                    _mm_load_si128((__m128i *) (transform32x32[6][15])));
            E6h = _mm_madd_epi16(m128Tmp13,
                    _mm_load_si128((__m128i *) (transform32x32[6][15])));
            E7l = _mm_madd_epi16(m128Tmp14,
                    _mm_load_si128((__m128i *) (transform32x32[7][15])));
            E7h = _mm_madd_epi16(m128Tmp15,
                    _mm_load_si128((__m128i *) (transform32x32[7][15])));

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

            m128Tmp0 = _mm_unpacklo_epi16(m128iS2, m128iS6);
            E0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][0])));
            m128Tmp1 = _mm_unpackhi_epi16(m128iS2, m128iS6);
            E0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][0])));

            m128Tmp2 = _mm_unpacklo_epi16(m128iS10, m128iS14);
            E0l = _mm_add_epi32(E0l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][0]))));
            m128Tmp3 = _mm_unpackhi_epi16(m128iS10, m128iS14);
            E0h = _mm_add_epi32(E0h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][0]))));

            m128Tmp4 = _mm_unpacklo_epi16(m128iS18, m128iS22);
            E0l = _mm_add_epi32(E0l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][0]))));
            m128Tmp5 = _mm_unpackhi_epi16(m128iS18, m128iS22);
            E0h = _mm_add_epi32(E0h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][0]))));

            m128Tmp6 = _mm_unpacklo_epi16(m128iS26, m128iS30);
            E0l = _mm_add_epi32(E0l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][0]))));
            m128Tmp7 = _mm_unpackhi_epi16(m128iS26, m128iS30);
            E0h = _mm_add_epi32(E0h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][0]))));

            /*  Compute E1  */
            E1l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][1])));
            E1h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][1])));
            E1l = _mm_add_epi32(E1l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][1]))));
            E1h = _mm_add_epi32(E1h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][1]))));
            E1l = _mm_add_epi32(E1l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][1]))));
            E1h = _mm_add_epi32(E1h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][1]))));
            E1l = _mm_add_epi32(E1l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][1]))));
            E1h = _mm_add_epi32(E1h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][1]))));

            /*  Compute E2  */
            E2l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][2])));
            E2h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][2])));
            E2l = _mm_add_epi32(E2l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][2]))));
            E2h = _mm_add_epi32(E2h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][2]))));
            E2l = _mm_add_epi32(E2l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][2]))));
            E2h = _mm_add_epi32(E2h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][2]))));
            E2l = _mm_add_epi32(E2l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][2]))));
            E2h = _mm_add_epi32(E2h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][2]))));

            /*  Compute E3  */
            E3l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][3])));
            E3h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][3])));
            E3l = _mm_add_epi32(E3l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][3]))));
            E3h = _mm_add_epi32(E3h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][3]))));
            E3l = _mm_add_epi32(E3l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][3]))));
            E3h = _mm_add_epi32(E3h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][3]))));
            E3l = _mm_add_epi32(E3l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][3]))));
            E3h = _mm_add_epi32(E3h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][3]))));

            /*  Compute E4  */
            E4l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][4])));
            E4h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][4])));
            E4l = _mm_add_epi32(E4l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][4]))));
            E4h = _mm_add_epi32(E4h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][4]))));
            E4l = _mm_add_epi32(E4l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][4]))));
            E4h = _mm_add_epi32(E4h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][4]))));
            E4l = _mm_add_epi32(E4l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][4]))));
            E4h = _mm_add_epi32(E4h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][4]))));

            /*  Compute E3  */
            E5l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][5])));
            E5h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][5])));
            E5l = _mm_add_epi32(E5l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][5]))));
            E5h = _mm_add_epi32(E5h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][5]))));
            E5l = _mm_add_epi32(E5l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][5]))));
            E5h = _mm_add_epi32(E5h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][5]))));
            E5l = _mm_add_epi32(E5l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][5]))));
            E5h = _mm_add_epi32(E5h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][5]))));

            /*  Compute E6  */
            E6l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][6])));
            E6h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][6])));
            E6l = _mm_add_epi32(E6l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][6]))));
            E6h = _mm_add_epi32(E6h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][6]))));
            E6l = _mm_add_epi32(E6l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][6]))));
            E6h = _mm_add_epi32(E6h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][6]))));
            E6l = _mm_add_epi32(E6l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][6]))));
            E6h = _mm_add_epi32(E6h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][6]))));

            /*  Compute E7  */
            E7l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][7])));
            E7h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_1[0][7])));
            E7l = _mm_add_epi32(E7l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][7]))));
            E7h = _mm_add_epi32(E7h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[1][7]))));
            E7l = _mm_add_epi32(E7l,
                    _mm_madd_epi16(m128Tmp4,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][7]))));
            E7h = _mm_add_epi32(E7h,
                    _mm_madd_epi16(m128Tmp5,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[2][7]))));
            E7l = _mm_add_epi32(E7l,
                    _mm_madd_epi16(m128Tmp6,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][7]))));
            E7h = _mm_add_epi32(E7h,
                    _mm_madd_epi16(m128Tmp7,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_1[3][7]))));

            /*  Compute EE0 and EEE */

            m128Tmp0 = _mm_unpacklo_epi16(m128iS4, m128iS12);
            E00l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][0])));
            m128Tmp1 = _mm_unpackhi_epi16(m128iS4, m128iS12);
            E00h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][0])));

            m128Tmp2 = _mm_unpacklo_epi16(m128iS20, m128iS28);
            E00l = _mm_add_epi32(E00l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][0]))));
            m128Tmp3 = _mm_unpackhi_epi16(m128iS20, m128iS28);
            E00h = _mm_add_epi32(E00h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][0]))));

            E01l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][1])));
            E01h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][1])));
            E01l = _mm_add_epi32(E01l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][1]))));
            E01h = _mm_add_epi32(E01h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][1]))));

            E02l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][2])));
            E02h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][2])));
            E02l = _mm_add_epi32(E02l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][2]))));
            E02h = _mm_add_epi32(E02h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][2]))));

            E03l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][3])));
            E03h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_2[0][3])));
            E03l = _mm_add_epi32(E03l,
                    _mm_madd_epi16(m128Tmp2,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][3]))));
            E03h = _mm_add_epi32(E03h,
                    _mm_madd_epi16(m128Tmp3,
                            _mm_load_si128(
                                    (__m128i *) (transform16x16_2[1][3]))));

            /*  Compute EE0 and EEE */

            m128Tmp0 = _mm_unpacklo_epi16(m128iS8, m128iS24);
            EE0l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][0])));
            m128Tmp1 = _mm_unpackhi_epi16(m128iS8, m128iS24);
            EE0h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][0])));

            m128Tmp2 = _mm_unpacklo_epi16(m128iS0, m128iS16);
            EEE0l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][0])));
            m128Tmp3 = _mm_unpackhi_epi16(m128iS0, m128iS16);
            EEE0h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][0])));

            EE1l = _mm_madd_epi16(m128Tmp0,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][1])));
            EE1h = _mm_madd_epi16(m128Tmp1,
                    _mm_load_si128((__m128i *) (transform16x16_3[0][1])));

            EEE1l = _mm_madd_epi16(m128Tmp2,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][1])));
            EEE1h = _mm_madd_epi16(m128Tmp3,
                    _mm_load_si128((__m128i *) (transform16x16_3[1][1])));

            /*  Compute EE    */

            EE2l = _mm_sub_epi32(EEE1l, EE1l);
            EE3l = _mm_sub_epi32(EEE0l, EE0l);
            EE2h = _mm_sub_epi32(EEE1h, EE1h);
            EE3h = _mm_sub_epi32(EEE0h, EE0h);

            EE0l = _mm_add_epi32(EEE0l, EE0l);
            EE1l = _mm_add_epi32(EEE1l, EE1l);
            EE0h = _mm_add_epi32(EEE0h, EE0h);
            EE1h = _mm_add_epi32(EEE1h, EE1h);
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

            E15l = _mm_sub_epi32(EE0l, E0l);
            E15l = _mm_add_epi32(E15l, m128iAdd);
            E14l = _mm_sub_epi32(EE1l, E1l);
            E14l = _mm_add_epi32(E14l, m128iAdd);
            E13l = _mm_sub_epi32(EE2l, E2l);
            E13l = _mm_add_epi32(E13l, m128iAdd);
            E12l = _mm_sub_epi32(EE3l, E3l);
            E12l = _mm_add_epi32(E12l, m128iAdd);
            E11l = _mm_sub_epi32(EE4l, E4l);
            E11l = _mm_add_epi32(E11l, m128iAdd);
            E10l = _mm_sub_epi32(EE5l, E5l);
            E10l = _mm_add_epi32(E10l, m128iAdd);
            E9l = _mm_sub_epi32(EE6l, E6l);
            E9l = _mm_add_epi32(E9l, m128iAdd);
            E8l = _mm_sub_epi32(EE7l, E7l);
            E8l = _mm_add_epi32(E8l, m128iAdd);

            E0l = _mm_add_epi32(EE0l, E0l);
            E0l = _mm_add_epi32(E0l, m128iAdd);
            E1l = _mm_add_epi32(EE1l, E1l);
            E1l = _mm_add_epi32(E1l, m128iAdd);
            E2l = _mm_add_epi32(EE2l, E2l);
            E2l = _mm_add_epi32(E2l, m128iAdd);
            E3l = _mm_add_epi32(EE3l, E3l);
            E3l = _mm_add_epi32(E3l, m128iAdd);
            E4l = _mm_add_epi32(EE4l, E4l);
            E4l = _mm_add_epi32(E4l, m128iAdd);
            E5l = _mm_add_epi32(EE5l, E5l);
            E5l = _mm_add_epi32(E5l, m128iAdd);
            E6l = _mm_add_epi32(EE6l, E6l);
            E6l = _mm_add_epi32(E6l, m128iAdd);
            E7l = _mm_add_epi32(EE7l, E7l);
            E7l = _mm_add_epi32(E7l, m128iAdd);

            E15h = _mm_sub_epi32(EE0h, E0h);
            E15h = _mm_add_epi32(E15h, m128iAdd);
            E14h = _mm_sub_epi32(EE1h, E1h);
            E14h = _mm_add_epi32(E14h, m128iAdd);
            E13h = _mm_sub_epi32(EE2h, E2h);
            E13h = _mm_add_epi32(E13h, m128iAdd);
            E12h = _mm_sub_epi32(EE3h, E3h);
            E12h = _mm_add_epi32(E12h, m128iAdd);
            E11h = _mm_sub_epi32(EE4h, E4h);
            E11h = _mm_add_epi32(E11h, m128iAdd);
            E10h = _mm_sub_epi32(EE5h, E5h);
            E10h = _mm_add_epi32(E10h, m128iAdd);
            E9h = _mm_sub_epi32(EE6h, E6h);
            E9h = _mm_add_epi32(E9h, m128iAdd);
            E8h = _mm_sub_epi32(EE7h, E7h);
            E8h = _mm_add_epi32(E8h, m128iAdd);

            E0h = _mm_add_epi32(EE0h, E0h);
            E0h = _mm_add_epi32(E0h, m128iAdd);
            E1h = _mm_add_epi32(EE1h, E1h);
            E1h = _mm_add_epi32(E1h, m128iAdd);
            E2h = _mm_add_epi32(EE2h, E2h);
            E2h = _mm_add_epi32(E2h, m128iAdd);
            E3h = _mm_add_epi32(EE3h, E3h);
            E3h = _mm_add_epi32(E3h, m128iAdd);
            E4h = _mm_add_epi32(EE4h, E4h);
            E4h = _mm_add_epi32(E4h, m128iAdd);
            E5h = _mm_add_epi32(EE5h, E5h);
            E5h = _mm_add_epi32(E5h, m128iAdd);
            E6h = _mm_add_epi32(EE6h, E6h);
            E6h = _mm_add_epi32(E6h, m128iAdd);
            E7h = _mm_add_epi32(EE7h, E7h);
            E7h = _mm_add_epi32(E7h, m128iAdd);

            m128iS0 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E0l, O0l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E0h, O0h), shift));
            m128iS1 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E1l, O1l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E1h, O1h), shift));
            m128iS2 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E2l, O2l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E2h, O2h), shift));
            m128iS3 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E3l, O3l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E3h, O3h), shift));
            m128iS4 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E4l, O4l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E4h, O4h), shift));
            m128iS5 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E5l, O5l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E5h, O5h), shift));
            m128iS6 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E6l, O6l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E6h, O6h), shift));
            m128iS7 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E7l, O7l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E7h, O7h), shift));
            m128iS8 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E8l, O8l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E8h, O8h), shift));
            m128iS9 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E9l, O9l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E9h, O9h), shift));
            m128iS10 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E10l, O10l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E10h, O10h), shift));
            m128iS11 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E11l, O11l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E11h, O11h), shift));
            m128iS12 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E12l, O12l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E12h, O12h), shift));
            m128iS13 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E13l, O13l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E13h, O13h), shift));
            m128iS14 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E14l, O14l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E14h, O14h), shift));
            m128iS15 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_add_epi32(E15l, O15l), shift),
                    _mm_srai_epi32(_mm_add_epi32(E15h, O15h), shift));

            m128iS31 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E0l, O0l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E0h, O0h), shift));
            m128iS30 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E1l, O1l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E1h, O1h), shift));
            m128iS29 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E2l, O2l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E2h, O2h), shift));
            m128iS28 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E3l, O3l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E3h, O3h), shift));
            m128iS27 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E4l, O4l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E4h, O4h), shift));
            m128iS26 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E5l, O5l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E5h, O5h), shift));
            m128iS25 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E6l, O6l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E6h, O6h), shift));
            m128iS24 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E7l, O7l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E7h, O7h), shift));
            m128iS23 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E8l, O8l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E8h, O8h), shift));
            m128iS22 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E9l, O9l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E9h, O9h), shift));
            m128iS21 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E10l, O10l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E10h, O10h), shift));
            m128iS20 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E11l, O11l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E11h, O11h), shift));
            m128iS19 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E12l, O12l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E12h, O12h), shift));
            m128iS18 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E13l, O13l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E13h, O13h), shift));
            m128iS17 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E14l, O14l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E14h, O14h), shift));
            m128iS16 = _mm_packs_epi32(
                    _mm_srai_epi32(_mm_sub_epi32(E15l, O15l), shift),
                    _mm_srai_epi32(_mm_sub_epi32(E15h, O15h), shift));

            if (!j) {
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

                E0h = _mm_unpacklo_epi16(E0l, E8l);
                E1h = _mm_unpacklo_epi16(E1l, E9l);
                E2h = _mm_unpacklo_epi16(E2l, E10l);
                E3h = _mm_unpacklo_epi16(E3l, E11l);
                E4h = _mm_unpacklo_epi16(E4l, E12l);
                E5h = _mm_unpacklo_epi16(E5l, E13l);
                E6h = _mm_unpacklo_epi16(E6l, E14l);
                E7h = _mm_unpacklo_epi16(E7l, E15l);

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
                m128iS0 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS1 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS2 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS3 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpackhi_epi16(E0h, E4h);
                m128Tmp1 = _mm_unpackhi_epi16(E1h, E5h);
                m128Tmp2 = _mm_unpackhi_epi16(E2h, E6h);
                m128Tmp3 = _mm_unpackhi_epi16(E3h, E7h);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS4 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS5 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS6 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS7 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpacklo_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpacklo_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpacklo_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpacklo_epi16(E11h, E15h);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS8 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS9 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS10 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS11 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpackhi_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpackhi_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpackhi_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpackhi_epi16(E11h, E15h);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS12 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS13 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS14 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS15 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                /*  */
                E0h = _mm_unpacklo_epi16(O0l, O8l);
                E1h = _mm_unpacklo_epi16(O1l, O9l);
                E2h = _mm_unpacklo_epi16(O2l, O10l);
                E3h = _mm_unpacklo_epi16(O3l, O11l);
                E4h = _mm_unpacklo_epi16(O4l, O12l);
                E5h = _mm_unpacklo_epi16(O5l, O13l);
                E6h = _mm_unpacklo_epi16(O6l, O14l);
                E7h = _mm_unpacklo_epi16(O7l, O15l);

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
                m128iS16 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS17 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS18 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS19 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpackhi_epi16(E0h, E4h);
                m128Tmp1 = _mm_unpackhi_epi16(E1h, E5h);
                m128Tmp2 = _mm_unpackhi_epi16(E2h, E6h);
                m128Tmp3 = _mm_unpackhi_epi16(E3h, E7h);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS20 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS21 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS22 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS23 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpacklo_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpacklo_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpacklo_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpacklo_epi16(E11h, E15h);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS24 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS25 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS26 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS27 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp0 = _mm_unpackhi_epi16(E8h, E12h);
                m128Tmp1 = _mm_unpackhi_epi16(E9h, E13h);
                m128Tmp2 = _mm_unpackhi_epi16(E10h, E14h);
                m128Tmp3 = _mm_unpackhi_epi16(E11h, E15h);

                m128Tmp4 = _mm_unpacklo_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpacklo_epi16(m128Tmp1, m128Tmp3);
                m128iS28 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS29 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);

                m128Tmp4 = _mm_unpackhi_epi16(m128Tmp0, m128Tmp2);
                m128Tmp5 = _mm_unpackhi_epi16(m128Tmp1, m128Tmp3);
                m128iS30 = _mm_unpacklo_epi16(m128Tmp4, m128Tmp5);
                m128iS31 = _mm_unpackhi_epi16(m128Tmp4, m128Tmp5);
                /*  */
                _mm_store_si128((__m128i *) (src + i), m128iS0);
                _mm_store_si128((__m128i *) (src + 32 + i), m128iS1);
                _mm_store_si128((__m128i *) (src + 64 + i), m128iS2);
                _mm_store_si128((__m128i *) (src + 96 + i), m128iS3);
                _mm_store_si128((__m128i *) (src + 128 + i), m128iS4);
                _mm_store_si128((__m128i *) (src + 160 + i), m128iS5);
                _mm_store_si128((__m128i *) (src + 192 + i), m128iS6);
                _mm_store_si128((__m128i *) (src + 224 + i), m128iS7);
                _mm_store_si128((__m128i *) (src + 256 + i), m128iS8);
                _mm_store_si128((__m128i *) (src + 288 + i), m128iS9);
                _mm_store_si128((__m128i *) (src + 320 + i), m128iS10);
                _mm_store_si128((__m128i *) (src + 352 + i), m128iS11);
                _mm_store_si128((__m128i *) (src + 384 + i), m128iS12);
                _mm_store_si128((__m128i *) (src + 416 + i), m128iS13);
                _mm_store_si128((__m128i *) (src + 448 + i), m128iS14);
                _mm_store_si128((__m128i *) (src + 480 + i), m128iS15);
                _mm_store_si128((__m128i *) (src + 512 + i), m128iS16);
                _mm_store_si128((__m128i *) (src + 544 + i), m128iS17);
                _mm_store_si128((__m128i *) (src + 576 + i), m128iS18);
                _mm_store_si128((__m128i *) (src + 608 + i), m128iS19);
                _mm_store_si128((__m128i *) (src + 640 + i), m128iS20);
                _mm_store_si128((__m128i *) (src + 672 + i), m128iS21);
                _mm_store_si128((__m128i *) (src + 704 + i), m128iS22);
                _mm_store_si128((__m128i *) (src + 736 + i), m128iS23);
                _mm_store_si128((__m128i *) (src + 768 + i), m128iS24);
                _mm_store_si128((__m128i *) (src + 800 + i), m128iS25);
                _mm_store_si128((__m128i *) (src + 832 + i), m128iS26);
                _mm_store_si128((__m128i *) (src + 864 + i), m128iS27);
                _mm_store_si128((__m128i *) (src + 896 + i), m128iS28);
                _mm_store_si128((__m128i *) (src + 928 + i), m128iS29);
                _mm_store_si128((__m128i *) (src + 960 + i), m128iS30);
                _mm_store_si128((__m128i *) (src + 992 + i), m128iS31);

                if (i <= 16) {
                    int k = i + 8;
                    m128iS0 = _mm_load_si128((__m128i *) (src + k));
                    m128iS1 = _mm_load_si128((__m128i *) (src + 32 + k));
                    m128iS2 = _mm_load_si128((__m128i *) (src + 64 + k));
                    m128iS3 = _mm_load_si128((__m128i *) (src + 96 + k));
                    m128iS4 = _mm_load_si128((__m128i *) (src + 128 + k));
                    m128iS5 = _mm_load_si128((__m128i *) (src + 160 + k));
                    m128iS6 = _mm_load_si128((__m128i *) (src + 192 + k));
                    m128iS7 = _mm_load_si128((__m128i *) (src + 224 + k));
                    m128iS8 = _mm_load_si128((__m128i *) (src + 256 + k));
                    m128iS9 = _mm_load_si128((__m128i *) (src + 288 + k));
                    m128iS10 = _mm_load_si128((__m128i *) (src + 320 + k));
                    m128iS11 = _mm_load_si128((__m128i *) (src + 352 + k));
                    m128iS12 = _mm_load_si128((__m128i *) (src + 384 + k));
                    m128iS13 = _mm_load_si128((__m128i *) (src + 416 + k));
                    m128iS14 = _mm_load_si128((__m128i *) (src + 448 + k));
                    m128iS15 = _mm_load_si128((__m128i *) (src + 480 + k));

                    m128iS16 = _mm_load_si128((__m128i *) (src + 512 + k));
                    m128iS17 = _mm_load_si128((__m128i *) (src + 544 + k));
                    m128iS18 = _mm_load_si128((__m128i *) (src + 576 + k));
                    m128iS19 = _mm_load_si128((__m128i *) (src + 608 + k));
                    m128iS20 = _mm_load_si128((__m128i *) (src + 640 + k));
                    m128iS21 = _mm_load_si128((__m128i *) (src + 672 + k));
                    m128iS22 = _mm_load_si128((__m128i *) (src + 704 + k));
                    m128iS23 = _mm_load_si128((__m128i *) (src + 736 + k));
                    m128iS24 = _mm_load_si128((__m128i *) (src + 768 + k));
                    m128iS25 = _mm_load_si128((__m128i *) (src + 800 + k));
                    m128iS26 = _mm_load_si128((__m128i *) (src + 832 + k));
                    m128iS27 = _mm_load_si128((__m128i *) (src + 864 + k));
                    m128iS28 = _mm_load_si128((__m128i *) (src + 896 + k));
                    m128iS29 = _mm_load_si128((__m128i *) (src + 928 + k));
                    m128iS30 = _mm_load_si128((__m128i *) (src + 960 + k));
                    m128iS31 = _mm_load_si128((__m128i *) (src + 992 + k));
                } else {
                    m128iS0 = _mm_load_si128((__m128i *) (src));
                    m128iS1 = _mm_load_si128((__m128i *) (src + 128));
                    m128iS2 = _mm_load_si128((__m128i *) (src + 256));
                    m128iS3 = _mm_load_si128((__m128i *) (src + 384));
                    m128iS4 = _mm_loadu_si128((__m128i *) (src + 512));
                    m128iS5 = _mm_load_si128((__m128i *) (src + 640));
                    m128iS6 = _mm_load_si128((__m128i *) (src + 768));
                    m128iS7 = _mm_load_si128((__m128i *) (src + 896));
                    m128iS8 = _mm_load_si128((__m128i *) (src + 8));
                    m128iS9 = _mm_load_si128((__m128i *) (src + 128 + 8));
                    m128iS10 = _mm_load_si128((__m128i *) (src + 256 + 8));
                    m128iS11 = _mm_load_si128((__m128i *) (src + 384 + 8));
                    m128iS12 = _mm_loadu_si128((__m128i *) (src + 512 + 8));
                    m128iS13 = _mm_load_si128((__m128i *) (src + 640 + 8));
                    m128iS14 = _mm_load_si128((__m128i *) (src + 768 + 8));
                    m128iS15 = _mm_load_si128((__m128i *) (src + 896 + 8));
                    m128iS16 = _mm_load_si128((__m128i *) (src + 16));
                    m128iS17 = _mm_load_si128((__m128i *) (src + 128 + 16));
                    m128iS18 = _mm_load_si128((__m128i *) (src + 256 + 16));
                    m128iS19 = _mm_load_si128((__m128i *) (src + 384 + 16));
                    m128iS20 = _mm_loadu_si128((__m128i *) (src + 512 + 16));
                    m128iS21 = _mm_load_si128((__m128i *) (src + 640 + 16));
                    m128iS22 = _mm_load_si128((__m128i *) (src + 768 + 16));
                    m128iS23 = _mm_load_si128((__m128i *) (src + 896 + 16));
                    m128iS24 = _mm_load_si128((__m128i *) (src + 24));
                    m128iS25 = _mm_load_si128((__m128i *) (src + 128 + 24));
                    m128iS26 = _mm_load_si128((__m128i *) (src + 256 + 24));
                    m128iS27 = _mm_load_si128((__m128i *) (src + 384 + 24));
                    m128iS28 = _mm_loadu_si128((__m128i *) (src + 512 + 24));
                    m128iS29 = _mm_load_si128((__m128i *) (src + 640 + 24));
                    m128iS30 = _mm_load_si128((__m128i *) (src + 768 + 24));
                    m128iS31 = _mm_load_si128((__m128i *) (src + 896 + 24));
                    shift = shift_2nd;
                    m128iAdd = _mm_set1_epi32(add_2nd);
                }

            } else {
                int k, m = 0;
                _mm_storeu_si128((__m128i *) (src), m128iS0);
                _mm_storeu_si128((__m128i *) (src + 8), m128iS1);
                _mm_storeu_si128((__m128i *) (src + 16), m128iS2);
                _mm_storeu_si128((__m128i *) (src + 24), m128iS3);
                _mm_storeu_si128((__m128i *) (src + 128), m128iS4);
                _mm_storeu_si128((__m128i *) (src + 128 + 8), m128iS5);
                _mm_storeu_si128((__m128i *) (src + 128 + 16), m128iS6);
                _mm_storeu_si128((__m128i *) (src + 128 + 24), m128iS7);
                _mm_storeu_si128((__m128i *) (src + 256), m128iS8);
                _mm_storeu_si128((__m128i *) (src + 256 + 8), m128iS9);
                _mm_storeu_si128((__m128i *) (src + 256 + 16), m128iS10);
                _mm_storeu_si128((__m128i *) (src + 256 + 24), m128iS11);
                _mm_storeu_si128((__m128i *) (src + 384), m128iS12);
                _mm_storeu_si128((__m128i *) (src + 384 + 8), m128iS13);
                _mm_storeu_si128((__m128i *) (src + 384 + 16), m128iS14);
                _mm_storeu_si128((__m128i *) (src + 384 + 24), m128iS15);

                _mm_storeu_si128((__m128i *) (src + 512), m128iS16);
                _mm_storeu_si128((__m128i *) (src + 512 + 8), m128iS17);
                _mm_storeu_si128((__m128i *) (src + 512 + 16), m128iS18);
                _mm_storeu_si128((__m128i *) (src + 512 + 24), m128iS19);
                _mm_storeu_si128((__m128i *) (src + 640), m128iS20);
                _mm_storeu_si128((__m128i *) (src + 640 + 8), m128iS21);
                _mm_storeu_si128((__m128i *) (src + 640 + 16), m128iS22);
                _mm_storeu_si128((__m128i *) (src + 640 + 24), m128iS23);
                _mm_storeu_si128((__m128i *) (src + 768), m128iS24);
                _mm_storeu_si128((__m128i *) (src + 768 + 8), m128iS25);
                _mm_storeu_si128((__m128i *) (src + 768 + 16), m128iS26);
                _mm_storeu_si128((__m128i *) (src + 768 + 24), m128iS27);
                _mm_storeu_si128((__m128i *) (src + 896), m128iS28);
                _mm_storeu_si128((__m128i *) (src + 896 + 8), m128iS29);
                _mm_storeu_si128((__m128i *) (src + 896 + 16), m128iS30);
                _mm_storeu_si128((__m128i *) (src + 896 + 24), m128iS31);
                dst = (uint16_t*) _dst + (i * stride);
                for (k = 0; k < 8; k++) {
                    dst[0] = av_clip_uintp2(dst[0] + src[m],10);
                    dst[1] = av_clip_uintp2(dst[1] + src[m + 8],10);
                    dst[2] = av_clip_uintp2(dst[2] + src[m + 16],10);
                    dst[3] = av_clip_uintp2(dst[3] + src[m + 24],10);
                    dst[4] = av_clip_uintp2(
                            dst[4] + src[m + 128],10);
                    dst[5] = av_clip_uintp2(
                            dst[5] + src[m + 128 + 8],10);
                    dst[6] = av_clip_uintp2(
                            dst[6] + src[m + 128 + 16],10);
                    dst[7] = av_clip_uintp2(
                            dst[7] + src[m + 128 + 24],10);

                    dst[8] = av_clip_uintp2(
                            dst[8] + src[m + 256],10);
                    dst[9] = av_clip_uintp2(
                            dst[9] + src[m + 256 + 8],10);
                    dst[10] = av_clip_uintp2(
                            dst[10] + src[m + 256 + 16],10);
                    dst[11] = av_clip_uintp2(
                            dst[11] + src[m + 256 + 24],10);
                    dst[12] = av_clip_uintp2(
                            dst[12] + src[m + 384],10);
                    dst[13] = av_clip_uintp2(
                            dst[13] + src[m + 384 + 8],10);
                    dst[14] = av_clip_uintp2(
                            dst[14] + src[m + 384 + 16],10);
                    dst[15] = av_clip_uintp2(
                            dst[15] + src[m + 384 + 24],10);

                    dst[16] = av_clip_uintp2(
                            dst[16] + src[m + 512],10);
                    dst[17] = av_clip_uintp2(
                            dst[17] + src[m + 512 + 8],10);
                    dst[18] = av_clip_uintp2(
                            dst[18] + src[m + 512 + 16],10);
                    dst[19] = av_clip_uintp2(
                            dst[19] + src[m + 512 + 24],10);
                    dst[20] = av_clip_uintp2(
                            dst[20] + src[m + 640],10);
                    dst[21] = av_clip_uintp2(
                            dst[21] + src[m + 640 + 8],10);
                    dst[22] = av_clip_uintp2(
                            dst[22] + src[m + 640 + 16],10);
                    dst[23] = av_clip_uintp2(
                            dst[23] + src[m + 640 + 24],10);

                    dst[24] = av_clip_uintp2(
                            dst[24] + src[m + 768],10);
                    dst[25] = av_clip_uintp2(
                            dst[25] + src[m + 768 + 8],10);
                    dst[26] = av_clip_uintp2(
                            dst[26] + src[m + 768 + 16],10);
                    dst[27] = av_clip_uintp2(
                            dst[27] + src[m + 768 + 24],10);
                    dst[28] = av_clip_uintp2(
                            dst[28] + src[m + 896],10);
                    dst[29] = av_clip_uintp2(
                            dst[29] + src[m + 896 + 8],10);
                    dst[30] = av_clip_uintp2(
                            dst[30] + src[m + 896 + 16],10);
                    dst[31] = av_clip_uintp2(
                            dst[31] + src[m + 896 + 24],10);

                    m += 1;
                    dst += stride;
                }
                if (i <= 16) {
                    int k = (i + 8) * 4;
                    m128iS0 = _mm_load_si128((__m128i *) (src + k));
                    m128iS1 = _mm_load_si128((__m128i *) (src + 128 + k));
                    m128iS2 = _mm_load_si128((__m128i *) (src + 256 + k));
                    m128iS3 = _mm_load_si128((__m128i *) (src + 384 + k));
                    m128iS4 = _mm_loadu_si128((__m128i *) (src + 512 + k));
                    m128iS5 = _mm_load_si128((__m128i *) (src + 640 + k));
                    m128iS6 = _mm_load_si128((__m128i *) (src + 768 + k));
                    m128iS7 = _mm_load_si128((__m128i *) (src + 896 + k));
                    m128iS8 = _mm_load_si128((__m128i *) (src + 8 + k));
                    m128iS9 = _mm_load_si128((__m128i *) (src + 128 + 8 + k));
                    m128iS10 = _mm_load_si128((__m128i *) (src + 256 + 8 + k));
                    m128iS11 = _mm_load_si128((__m128i *) (src + 384 + 8 + k));
                    m128iS12 = _mm_loadu_si128((__m128i *) (src + 512 + 8 + k));
                    m128iS13 = _mm_load_si128((__m128i *) (src + 640 + 8 + k));
                    m128iS14 = _mm_load_si128((__m128i *) (src + 768 + 8 + k));
                    m128iS15 = _mm_load_si128((__m128i *) (src + 896 + 8 + k));
                    m128iS16 = _mm_load_si128((__m128i *) (src + 16 + k));
                    m128iS17 = _mm_load_si128((__m128i *) (src + 128 + 16 + k));
                    m128iS18 = _mm_load_si128((__m128i *) (src + 256 + 16 + k));
                    m128iS19 = _mm_load_si128((__m128i *) (src + 384 + 16 + k));
                    m128iS20 = _mm_loadu_si128(
                            (__m128i *) (src + 512 + 16 + k));
                    m128iS21 = _mm_load_si128((__m128i *) (src + 640 + 16 + k));
                    m128iS22 = _mm_load_si128((__m128i *) (src + 768 + 16 + k));
                    m128iS23 = _mm_load_si128((__m128i *) (src + 896 + 16 + k));
                    m128iS24 = _mm_load_si128((__m128i *) (src + 24 + k));
                    m128iS25 = _mm_load_si128((__m128i *) (src + 128 + 24 + k));
                    m128iS26 = _mm_load_si128((__m128i *) (src + 256 + 24 + k));
                    m128iS27 = _mm_load_si128((__m128i *) (src + 384 + 24 + k));
                    m128iS28 = _mm_loadu_si128(
                            (__m128i *) (src + 512 + 24 + k));
                    m128iS29 = _mm_load_si128((__m128i *) (src + 640 + 24 + k));
                    m128iS30 = _mm_load_si128((__m128i *) (src + 768 + 24 + k));
                    m128iS31 = _mm_load_si128((__m128i *) (src + 896 + 24 + k));
                }
            }
        }
    }
}


