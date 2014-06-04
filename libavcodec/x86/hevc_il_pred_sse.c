#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcdsp.h"
#include "libavcodec/bit_depth_template.c"

#if HAVE_SSE2
#include <emmintrin.h>
#endif
#if HAVE_SSSE3
#include <tmmintrin.h>
#endif
#if HAVE_SSE42
#include <smmintrin.h>
#endif
/*      Upsampling filters      */
DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma[16][4])=
{
    {  0,  64,   0,  0},
    { -2,  62,   4,  0},
    { -2,  58,  10, -2},
    { -4,  56,  14, -2},
    { -4,  54,  16, -2},
    { -6,  52,  20, -2},
    { -6,  46,  28, -4},
    { -4,  42,  30, -4},
    { -4,  36,  36, -4},
    { -4,  30,  42, -4},
    { -4,  28,  46, -6},
    { -2,  20,  52, -6},
    { -2,  16,  54, -4},
    { -2,  14,  56, -4},
    { -2,  10,  58, -2},
    {  0,   4,  62, -2}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma[16][8] )=
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    {  0,  1,  -3,  63,   4,  -2,  1,  0},
    { -1,  2,  -5,  62,   8,  -3,  1,  0},
    { -1,  3,  -8,  60,  13,  -4,  1,  0},
    { -1,  4, -10,  58,  17,  -5,  1,  0},
    { -1,  4, -11,  52,  26,  -8,  3, -1},
    { -1,  3,  -9,  47,  31, -10,  4, -1},
    { -1,  4, -11,  45,  34, -10,  4, -1},
    { -1,  4, -11,  40,  40, -11,  4, -1},
    { -1,  4, -10,  34,  45, -11,  4, -1},
    { -1,  4, -10,  31,  47,  -9,  3, -1},
    { -1,  3,  -8,  26,  52, -11,  4, -1},
    {  0,  1,  -5,  17,  58, -10,  4, -1},
    {  0,  1,  -4,  13,  60,  -8,  3, -1},
    {  0,  1,  -3,   8,  62,  -5,  2, -1},
    {  0,  1,  -2,   4,  63,  -3,  1,  0}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x2[2][8] )= /*0 , 8 */
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    { -1,  4, -11,  40,  40, -11,  4, -1}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x1_5[3][8] )= /* 0, 11, 5 */
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    { -1,  3,  -8,  26,  52, -11,  4, -1},
    { -1,  4, -11,  52,  26,  -8,  3, -1}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x1_5[3][4])= /* 0, 11, 5 */
{
    {  0,  64,   0,  0},
    { -2,  20,  52, -6},
    { -6,  52,  20, -2}
};
DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_x1_5chroma[3][4])=
{
    {  0,   4,  62, -2},
    { -4,  30,  42, -4},
    { -4,  54,  16, -2}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x2[2][4])=
{
    {  0,  64,   0,  0},
    { -4,  36,  36, -4}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x2_v[2][4])=
{
    { -2,  10,  58, -2},
    { -6,  46,  28, -4},
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x2_h_sse[4][16] )= /*0 , 8 */
{
    { 0,  0,  -1,   4,  0,  0,  -1,   4, 0,  0,  -1,   4, 0,  0,  -1,   4},
    { 0, 64, -11,  40,  0, 64, -11,  40, 0, 64, -11,  40, 0, 64, -11,  40},
    { 0,  0,  40, -11,  0,  0,  40, -11, 0,  0,  40, -11, 0,  0,  40, -11},
    { 0,  0,   4,  -1,  0,  0,   4,  -1, 0,  0,   4,  -1, 0,  0,   4,  -1}
};

DECLARE_ALIGNED(16, static const int16_t, up_sample_filter_luma_x2_v_sse[5][8] )= /*0 , 8 */
{
    {   0,  64,   0,  64,   0,  64,   0,  64},
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4},
    { -11,  40, -11,  40, -11,  40, -11,  40},
    {  40, -11,  40, -11,  40, -11,  40, -11},
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x2_h_sse[2][16])=
{
    { 0, 64, -4, 36, 0, 64, -4, 36, 0, 64, -4, 36, 0, 64, -4, 36},
    { 0,  0, 36, -4, 0,  0, 36, -4, 0,  0, 36, -4, 0,  0, 36, -4}
};

DECLARE_ALIGNED(16, static const int16_t, up_sample_filter_chroma_x2_v_sse[4][8])=
{
    { -2, 10, -2, 10, -2, 10, -2, 10},
    { 58, -2, 58, -2, 58, -2, 58, -2},
    { -6, 46, -6, 46, -6, 46, -6, 46},
    { 28, -4, 28, -4, 28, -4, 28, -4}
};

DECLARE_ALIGNED(16, static const int16_t, up_sample_filter_chroma_x1_5_v_sse[6][8])=
{
    {  0,  4,  0,  4,  0,  4,  0,  4},
    { 62, -2, 62, -2, 62, -2, 62, -2},
    { -4, 30, -4, 30, -4, 30, -4, 30},
    { 42, -4, 42, -4, 42, -4, 42, -4},
    { -4, 54, -4, 54, -4, 54, -4, 54},
    { 16, -2, 16, -2, 16, -2, 16, -2}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x1_5_h_sse[12][16] )= /* 0, 11, 5 */
{
    {   0,   0,  -1,   3,  -1,   4,   0,   0,  -1,   3,  -1,   4,   0,   0,  -1,   3},
    {   0,  64,  -8,  26, -11,  52,   0,  64,  -8,  26, -11,  52,   0,  64,  -8,  26},
    {   0,   0,  52, -11,  26,  -8,   0,   0,  52, -11,  26,  -8,   0,   0,  52, -11},
    {   0,   0,   4,  -1,   3,  -1,   0,   0,   4,  -1,   3,  -1,   0,   0,   4,  -1},

    {  -1,   3,  -1,   4,   0,   0,  -1,   3,  -1,   4,   0,   0,  -1,   3,  -1,   4},
    {  -8,  26, -11,  52,   0,  64,  -8,  26, -11,  52,   0,  64,  -8,  26, -11,  52},
    {  52, -11,  26,  -8,   0,   0,  52, -11,  26,  -8,   0,   0,  52, -11,  26,  -8},
    {   4,  -1,   3,  -1,   0,   0,   4,  -1,   3,  -1,   0,   0,   4,  -1,   3,  -1},

    {  -1,   4,   0,   0,  -1,   3,  -1,   4,   0,   0,  -1,   3,  -1,   4,   0,   0},
    { -11,  52,   0,  64,  -8,  26, -11,  52,   0,  64,  -8,  26, -11,  52,   0,  64},
    {  26,  -8,   0,   0,  52, -11,  26,  -8,   0,   0,  52, -11,  26,  -8,   0,   0},
    {   3,  -1,   0,   0,   4,  -1,   3,  -1,   0,   0,   4,  -1,   3,  -1,   0,   0},
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x1_5_h_sse[6][16] )= /* 0, 11, 5 */
{
    {  0, 64, -2, 20, -6, 52,  0, 64, -2, 20, -6, 52,  0, 64, -2, 20},
    {  0,  0, 52, -6, 20, -2,  0,  0, 52, -6, 20, -2,  0,  0, 52, -6},
    { -2, 20, -6, 52,  0, 64, -2, 20, -6, 52,  0, 64, -2, 20, -6, 52},
    { 52, -6, 20, -2,  0,  0, 52, -6, 20, -2,  0,  0, 52, -6, 20, -2},
    { -6, 52,  0, 64, -2, 20, -6, 52,  0, 64, -2, 20, -6, 52,  0, 64},
    { 20, -2,  0,  0, 52, -6, 20, -2,  0,  0, 52, -6, 20, -2,  0,  0,}
};

DECLARE_ALIGNED(16, static const uint8_t, masks[3][16] )= {
    {0x00, 0x01, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x04, 0x05},
    {0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x04, 0x05, 0x05, 0x06},
    {0x00, 0x01, 0x01, 0x02, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06}
};

DECLARE_ALIGNED(16, static const int16_t, up_sample_filter_luma_x1_5_v_sse[9][8] )= /* 11, 5, 0 */
{
    {  -1,   3,  -1,   3,  -1,   3,  -1,   3 },
    {  -8,  26,  -8,  26,  -8,  26,  -8,  26 },
    {  52, -11,  52, -11,  52, -11,  52, -11 },
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1 },
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -11,  52, -11,  52, -11,  52, -11,  52 },
    {  26,  -8,  26,  -8,  26,  -8,  26,  -8 },
    {   3,  -1,   3,  -1,   3,  -1,   3,  -1 },
    {   0,  64,   0,  64,   0,  64,   0,  64 }
};

#if HAVE_SSE42

void ff_upsample_filter_block_luma_h_all_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int block_w, int block_h, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){

}

void ff_upsample_filter_block_luma_v_all_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){

}

void ff_upsample_filter_block_cr_h_all_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int block_w, int block_h, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){

}

void ff_upsample_filter_block_cr_v_all_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){

}

void ff_upsample_filter_block_luma_h_x2_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t srcstride,
            int x_EL, int x_BL, int width, int height, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){
    int x, y, ref;
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, r1, c1, c2, c3, c4;
    uint8_t  *src       = (uint8_t*) _src - x_BL;

    c1 = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_h_sse[0]);
    c2 = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_h_sse[1]);
    c3 = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_h_sse[2]);
    c4 = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_h_sse[3]);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x += 8) {
            ref = (x+x_EL)>>1;
            x1 = _mm_loadu_si128((__m128i *) &src[ref - 3]);
            x2 = _mm_loadu_si128((__m128i *) &src[ref - 2]);
            x3 = _mm_loadu_si128((__m128i *) &src[ref - 1]);
            x4 = _mm_loadu_si128((__m128i *) &src[ref ]);
            x5 = _mm_loadu_si128((__m128i *) &src[ref + 1]);
            x6 = _mm_loadu_si128((__m128i *) &src[ref + 2]);
            x7 = _mm_loadu_si128((__m128i *) &src[ref + 3]);
            x8 = _mm_loadu_si128((__m128i *) &src[ref + 4]);

            x1 = _mm_unpacklo_epi8(x1, x1);
            x2 = _mm_unpacklo_epi8(x2, x2);
            x3 = _mm_unpacklo_epi8(x3, x3);
            x4 = _mm_unpacklo_epi8(x4, x4);
            x5 = _mm_unpacklo_epi8(x5, x5);
            x6 = _mm_unpacklo_epi8(x6, x6);
            x7 = _mm_unpacklo_epi8(x7, x7);
            x8 = _mm_unpacklo_epi8(x8, x8);

            x1 = _mm_unpacklo_epi8(x1, x2);
            x2 = _mm_unpacklo_epi8(x3, x4);
            x3 = _mm_unpacklo_epi8(x5, x6);
            x4 = _mm_unpacklo_epi8(x7, x8);

            x2 = _mm_maddubs_epi16(x2,c2);
            x3 = _mm_maddubs_epi16(x3,c3);
            x1 = _mm_maddubs_epi16(x1,c1);
            x4 = _mm_maddubs_epi16(x4,c4);

            x1 = _mm_add_epi16(x1, x2);
            x2 = _mm_add_epi16(x3, x4);
            r1 = _mm_add_epi16(x1, x2);
            _mm_store_si128((__m128i *) &dst[x], r1);
        }
        src += srcstride;
        dst += dststride;
    }
}

void ff_upsample_filter_block_luma_v_x2_sse(uint8_t *_dst, ptrdiff_t _dststride, int16_t *_src, ptrdiff_t srcstride,
            int y_BL, int x_EL, int y_EL, int width, int height, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){
    int x, y, ret;
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, x0, c[5], add;
    int16_t  *src;
    uint8_t  *dst    = (uint8_t *)_dst + y_EL * _dststride + x_EL;
    uint8_t  shift = 12;
    add  = _mm_set1_epi32((1 << (shift - 1)));

    c[0] = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_v_sse[0]);
    c[1] = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_v_sse[1]);
    c[2] = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_v_sse[2]);
    c[3] = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_v_sse[3]);
    c[4] = _mm_load_si128((__m128i *) up_sample_filter_luma_x2_v_sse[4]);

    for (y = 0; y < height; y++) {
        src  = _src  + (((y_EL+y)>>1) - y_BL)  * srcstride;
        ret = ((y+y_EL)&0x1)*2;
        for (x = 0; x < width; x += 8) {
            x1 = _mm_loadu_si128((__m128i *) &src[x - 3 * srcstride]);
            x2 = _mm_loadu_si128((__m128i *) &src[x - 2 * srcstride]);
            x3 = _mm_loadu_si128((__m128i *) &src[x -     srcstride]);
            x4 = _mm_loadu_si128((__m128i *) &src[x                ]);
            x5 = _mm_loadu_si128((__m128i *) &src[x +     srcstride]);
            x6 = _mm_loadu_si128((__m128i *) &src[x + 2 * srcstride]);
            x7 = _mm_loadu_si128((__m128i *) &src[x + 3 * srcstride]);
            x8 = _mm_loadu_si128((__m128i *) &src[x + 4 * srcstride]);

            x0 = x1;
            x1 = _mm_unpacklo_epi16(x0, x2);
            x2 = _mm_unpackhi_epi16(x0, x2);
            x0 = x3;
            x3 = _mm_unpacklo_epi16(x0, x4);
            x4 = _mm_unpackhi_epi16(x0, x4);
            x0 = x5;
            x5 = _mm_unpacklo_epi16(x0, x6);
            x6 = _mm_unpackhi_epi16(x0, x6);
            x0 = x7;
            x7 = _mm_unpacklo_epi16(x0, x8);
            x8 = _mm_unpackhi_epi16(x0, x8);

            if(!ret) {
                x3 = _mm_madd_epi16(x3,c[0]);
                x4 = _mm_madd_epi16(x4,c[0]);

                x1 = x3;
                x3 = _mm_setzero_si128();
                x2 =  x4;
                x4 = x3;
            } else {
                x1 = _mm_madd_epi16(x1,c[1]);
                x3 = _mm_madd_epi16(x3,c[2]);
                x5 = _mm_madd_epi16(x5,c[3]);
                x7 = _mm_madd_epi16(x7,c[4]);
                x2 = _mm_madd_epi16(x2,c[1]);
                x4 = _mm_madd_epi16(x4,c[2]);
                x6 = _mm_madd_epi16(x6,c[3]);
                x8 = _mm_madd_epi16(x8,c[4]);

                x1 = _mm_add_epi32(x1, x3);
                x3 = _mm_add_epi32(x5, x7);
                x2 = _mm_add_epi32(x2, x4);
                x4 = _mm_add_epi32(x6, x8);
            }
            x8 = _mm_add_epi32(x1, x3);
            x7 = _mm_add_epi32(x2, x4);
            x8 = _mm_add_epi32(x8, add);
            x7 = _mm_add_epi32(x7, add);

            x8 = _mm_srai_epi32(x8, shift);
            x7 = _mm_srai_epi32(x7, shift);

            x8 = _mm_packus_epi32(x8, x7);
            x8 = _mm_packus_epi16(x8, x8);

            _mm_storel_epi64((__m128i *) &dst[x], x8);
        }
        dst += _dststride;
    }
}

void ff_upsample_filter_block_cr_h_x2_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int width, int height, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int x, y;
    __m128i x1, x2, x3, x4, f1, f2, r1;
    uint8_t  *src = ((uint8_t*) _src) - x_BL;
    ptrdiff_t srcstride = _srcstride;

    f1 = _mm_load_si128((__m128i *) up_sample_filter_chroma_x2_h_sse[0]);
    f2 = _mm_load_si128((__m128i *) up_sample_filter_chroma_x2_h_sse[1]);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x += 8) {
            int e = (x+x_EL)>>1;
                x1 = _mm_loadu_si128((__m128i *) &src[e - 1]);
                x2 = _mm_loadu_si128((__m128i *) &src[e ]);
                x3 = _mm_loadu_si128((__m128i *) &src[e + 1]);
                x4 = _mm_loadu_si128((__m128i *) &src[e + 2]);

                x1 = _mm_unpacklo_epi8(x1, x1);
                x2 = _mm_unpacklo_epi8(x2, x2);
                x3 = _mm_unpacklo_epi8(x3, x3);
                x4 = _mm_unpacklo_epi8(x4, x4);

                x1 = _mm_unpacklo_epi8(x1, x2);
                x2 = _mm_unpacklo_epi8(x3, x4);

                x2 = _mm_maddubs_epi16(x2,f2);
                x1 = _mm_maddubs_epi16(x1,f1);

                r1 = _mm_add_epi16(x1, x2);
                _mm_store_si128((__m128i *) &dst[x], r1);
            }
            src += srcstride;
            dst += dststride;
        }
}

void ff_upsample_filter_block_cr_v_x2_sse(uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t srcstride,
        int y_BL, int x_EL, int y_EL, int width, int height, int widthEL, int heightEL,
        const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){
    int x, y, ret;
    __m128i x1, x2, x3, x4, x0, c[4], add;
    int16_t  *src;
    uint8_t  *dst    = (uint8_t *)_dst + y_EL * dststride + x_EL;
    uint8_t  shift = 12;
    add  = _mm_set1_epi32((1 << (shift - 1)));

    c[0] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x2_v_sse[0]);
    c[1] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x2_v_sse[1]);
    c[2] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x2_v_sse[2]);
    c[3] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x2_v_sse[3]);

    for (y = 0; y < height; y++) {
        int refPos16 = ( ((y+y_EL) * up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        src  = _src  + ((refPos16>>4) - y_BL)  * srcstride;
        ret = ((y+y_EL)&0x1)*2;
        for (x = 0; x < width; x += 8) {
            x1 = _mm_loadu_si128((__m128i *) &src[x -     srcstride]);
            x2 = _mm_loadu_si128((__m128i *) &src[x                ]);
            x3 = _mm_loadu_si128((__m128i *) &src[x +     srcstride]);
            x4 = _mm_loadu_si128((__m128i *) &src[x + 2 * srcstride]);

            x0 = x1;
            x1 = _mm_unpacklo_epi16(x0, x2);
            x2 = _mm_unpackhi_epi16(x0, x2);
            x0 = x3;
            x3 = _mm_unpacklo_epi16(x0, x4);
            x4 = _mm_unpackhi_epi16(x0, x4);

            x1 = _mm_madd_epi16(x1,c[ret]);
            x3 = _mm_madd_epi16(x3,c[ret+1]);
            x2 = _mm_madd_epi16(x2,c[ret]);
            x4 = _mm_madd_epi16(x4,c[ret+1]);

            x1 = _mm_add_epi32(x1, x3);
            x2 = _mm_add_epi32(x2, x4);

            x1 = _mm_add_epi32(x1, add);
            x2 = _mm_add_epi32(x2, add);

            x1 = _mm_srai_epi32(x1, shift);
            x2 = _mm_srai_epi32(x2, shift);

            x1 = _mm_packus_epi32(x1, x2);
            x1 = _mm_packus_epi16(x1, x1);
            _mm_storel_epi64((__m128i *) &dst[x], x1);
        }
        dst += dststride;
    }
}

void ff_upsample_filter_block_luma_h_x1_5_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t srcstride,
            int x_EL, int x_BL, int width, int height, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int x, y, ref, ret;
    __m128i x1, x2, x3, x4, c[12], mask;
    uint8_t  *src       = (uint8_t*) _src - x_BL;

    c[0]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[0]);
    c[1]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[1]);
    c[2]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[2]);
    c[3]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[3]);

    c[4]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[4]);
    c[5]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[5]);
    c[6]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[6]);
    c[7]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[7]);

    c[8]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[8]  );
    c[9]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[9]  );
    c[10] = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[10]);
    c[11] = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_h_sse[11]);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x += 8) {
            ref = (((x+x_EL)<<1)/3);
            ret = (( x+x_EL)%3);

            x1 = _mm_loadu_si128((__m128i *) &src[ref - 3]);
            x2 = _mm_loadu_si128((__m128i *) &src[ref - 1]);
            x3 = _mm_loadu_si128((__m128i *) &src[ref + 1]);
            x4 = _mm_loadu_si128((__m128i *) &src[ref + 3]);

            mask = _mm_load_si128((__m128i *) masks[ret]);

            x1 = _mm_shuffle_epi8(x1, mask);
            x2 = _mm_shuffle_epi8(x2, mask);
            x3 = _mm_shuffle_epi8(x3, mask);
            x4 = _mm_shuffle_epi8(x4, mask);

            ret *= 4;
            x1 = _mm_maddubs_epi16(x1,c[ret]  );
            x2 = _mm_maddubs_epi16(x2,c[ret+1]);
            x3 = _mm_maddubs_epi16(x3,c[ret+2]);
            x4 = _mm_maddubs_epi16(x4,c[ret+3]);

            x1 = _mm_add_epi16(x1, x2);
            x2 = _mm_add_epi16(x3, x4);

            x4 = _mm_add_epi16(x1, x2);
            _mm_store_si128((__m128i *) &dst[x], x4);
        }
        src += srcstride;
        dst += dststride;
    }
}

void ff_upsample_filter_block_luma_v_x1_5_sse(uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t srcstride,
            int y_BL, int x_EL, int y_EL, int width, int height, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){
    int x, y, ret, ret0;
    __m128i x1, x2, x3, x4, x5, x6, x7, x8, x0, c[9], add;
    int16_t  *src;
    uint8_t  *dst    = (uint8_t *)_dst + y_EL * dststride + x_EL;
    uint8_t  shift = 12;
    add  = _mm_set1_epi32((1 << (shift - 1)));

    c[0]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[0]);
    c[1]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[1]);
    c[2]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[2]);
    c[3]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[3]);
    c[4]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[4]);
    c[5]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[5]);
    c[6]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[6]);
    c[7]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[7]);
    c[8]  = _mm_load_si128((__m128i *) up_sample_filter_luma_x1_5_v_sse[8]);

    for (y = 0; y < height; y++) {
        ret = (y_EL+y) % 3;
        ret0 = (ret-1)*4;
        src  = _src  + ((((y_EL+y)<<1)/3) - y_BL)  * srcstride;
        for (x = 0; x < width; x += 8) {
            x1 = _mm_loadu_si128((__m128i *) &src[x - 3 * srcstride]);
            x2 = _mm_loadu_si128((__m128i *) &src[x - 2 * srcstride]);
            x3 = _mm_loadu_si128((__m128i *) &src[x -     srcstride]);
            x4 = _mm_loadu_si128((__m128i *) &src[x                ]);
            x5 = _mm_loadu_si128((__m128i *) &src[x +     srcstride]);
            x6 = _mm_loadu_si128((__m128i *) &src[x + 2 * srcstride]);
            x7 = _mm_loadu_si128((__m128i *) &src[x + 3 * srcstride]);
            x8 = _mm_loadu_si128((__m128i *) &src[x + 4 * srcstride]);

            x0 = x1;
            x1 = _mm_unpacklo_epi16(x0, x2);
            x2 = _mm_unpackhi_epi16(x0, x2);
            x0 = x3;
            x3 = _mm_unpacklo_epi16(x0, x4);
            x4 = _mm_unpackhi_epi16(x0, x4);
            x0 = x5;
            x5 = _mm_unpacklo_epi16(x0, x6);
            x6 = _mm_unpackhi_epi16(x0, x6);
            x0 = x7;
            x7 = _mm_unpacklo_epi16(x0, x8);
            x8 = _mm_unpackhi_epi16(x0, x8);

            if(!ret) {
                x3 = _mm_madd_epi16(x3,c[8]);
                x4 = _mm_madd_epi16(x4,c[8]);

                x1 = x3;
                x3 = _mm_setzero_si128();
                x2 =  x4;
                x4 =  x3;
            } else {
                x1 = _mm_madd_epi16(x1,c[ret0    ]);
                x3 = _mm_madd_epi16(x3,c[ret0 + 1]);
                x5 = _mm_madd_epi16(x5,c[ret0 + 2]);
                x7 = _mm_madd_epi16(x7,c[ret0 + 3]);
                x2 = _mm_madd_epi16(x2,c[ret0    ]);
                x4 = _mm_madd_epi16(x4,c[ret0 + 1]);
                x6 = _mm_madd_epi16(x6,c[ret0 + 2]);
                x8 = _mm_madd_epi16(x8,c[ret0 + 3]);

                x1 = _mm_add_epi32(x1, x3);
                x3 = _mm_add_epi32(x5, x7);
                x2 = _mm_add_epi32(x2, x4);
                x4 = _mm_add_epi32(x6, x8);
            }
            x8 = _mm_add_epi32(x1, x3);
            x7 = _mm_add_epi32(x2, x4);
            x8 = _mm_add_epi32(x8, add);
            x7 = _mm_add_epi32(x7, add);

            x8 = _mm_srai_epi32(x8, shift);
            x7 = _mm_srai_epi32(x7, shift);

            x8 = _mm_packus_epi32(x8, x7);
            x8 = _mm_packus_epi16(x8, x8);

            _mm_storel_epi64((__m128i *) &dst[x], x8);
        }
        dst += dststride;
    }
}

void ff_upsample_filter_block_cr_h_x1_5_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t srcstride,
            int x_EL, int x_BL, int width, int height, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){
    int x, y, ref, ret;
    __m128i x1, x2, r1, c[6], mask;
    uint8_t  *src       = (uint8_t*) _src - x_BL;

    c[0]  = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_h_sse[0]);
    c[1]  = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_h_sse[1]);
    c[2]  = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_h_sse[2]);
    c[3]  = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_h_sse[3]);
    c[4]  = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_h_sse[4]);
    c[5]  = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_h_sse[5]);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x += 8) {
            ref = (((x+x_EL)<<1)/3);
            ret = (( x+x_EL)%3);

            x1 = _mm_loadu_si128((__m128i *) &src[ref - 1]);
            x2 = _mm_loadu_si128((__m128i *) &src[ref + 1]);

            mask = _mm_load_si128((__m128i *) masks[ret]);
            x1 = _mm_shuffle_epi8(x1, mask);
            x2 = _mm_shuffle_epi8(x2, mask);

            ret *= 2;
            x1 = _mm_maddubs_epi16(x1,c[ret]  );
            x2 = _mm_maddubs_epi16(x2,c[ret+1]);

            r1 = _mm_add_epi16(x1, x2);
            _mm_store_si128((__m128i *) &dst[x], r1);
        }
        src += srcstride;
        dst += dststride;
    }
}

void ff_upsample_filter_block_cr_v_x1_5_sse(uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t srcstride,
        int y_BL, int x_EL, int y_EL, int width, int height, int widthEL, int heightEL,
        const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info){

    int x, y, ret;
    __m128i x1, x2, x3, x4, x0, c[6], add;
    int16_t  *src;
    uint8_t  *dst    = (uint8_t *)_dst + y_EL * dststride + x_EL;
    uint8_t  shift = 12;
    add  = _mm_set1_epi32((1 << (shift - 1)));

    c[0] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_v_sse[0]);
    c[1] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_v_sse[1]);
    c[2] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_v_sse[2]);
    c[3] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_v_sse[3]);
    c[4] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_v_sse[4]);
    c[5] = _mm_load_si128((__m128i *) up_sample_filter_chroma_x1_5_v_sse[5]);

    for (y = 0; y < height; y++) {
        int refPos16 = ( ((y+y_EL) * up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        src  = _src  + ((refPos16>>4) - y_BL)  * srcstride;
        ret = ((y+y_EL)%3)*2;
        for (x = 0; x < width; x += 8) {
            x1 = _mm_loadu_si128((__m128i *) &src[x -     srcstride]);
            x2 = _mm_loadu_si128((__m128i *) &src[x                ]);
            x3 = _mm_loadu_si128((__m128i *) &src[x +     srcstride]);
            x4 = _mm_loadu_si128((__m128i *) &src[x + 2 * srcstride]);

            x0 = x1;
            x1 = _mm_unpacklo_epi16(x0, x2);
            x2 = _mm_unpackhi_epi16(x0, x2);
            x0 = x3;
            x3 = _mm_unpacklo_epi16(x0, x4);
            x4 = _mm_unpackhi_epi16(x0, x4);

            x1 = _mm_madd_epi16(x1,c[ret]);
            x3 = _mm_madd_epi16(x3,c[ret+1]);
            x2 = _mm_madd_epi16(x2,c[ret]);
            x4 = _mm_madd_epi16(x4,c[ret+1]);

            x1 = _mm_add_epi32(x1, x3);
            x2 = _mm_add_epi32(x2, x4);

            x1 = _mm_add_epi32(x1, add);
            x2 = _mm_add_epi32(x2, add);

            x1 = _mm_srai_epi32(x1, shift);
            x2 = _mm_srai_epi32(x2, shift);

            x1 = _mm_packus_epi32(x1, x2);
            x1 = _mm_packus_epi16(x1, x1);
            _mm_storel_epi64((__m128i *) &dst[x], x1);
        }
        dst += dststride;
    }
}
#endif //HAVE_SSE42
