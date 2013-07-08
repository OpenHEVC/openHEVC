#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevcdata.h"
#include "libavcodec/hevc.h"


#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define BIT_DEPTH 8

void ff_hevc_put_unweighted_pred_sse(uint8_t *_dst, ptrdiff_t _dststride,
                                      int16_t *src, ptrdiff_t srcstride,
                                      int width, int height)
{
    int x, y;
    uint8_t *dst = (uint8_t*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(uint8_t);
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
            r0= _mm_load_si128((__m128i*)&src[x]);

            r1= _mm_load_si128((__m128i*)&src[x+8]);
            r0= _mm_adds_epi16(r0,f0);

            r1= _mm_adds_epi16(r1,f0);
            r0= _mm_srai_epi16(r0,shift);
            r1= _mm_srai_epi16(r1,shift);
            r0= _mm_packus_epi16(r0,r1);

            _mm_storeu_si128((__m128i*)&dst[x],r0);
        }
        dst += dststride;
        src += srcstride;
    }
}

void ff_hevc_put_weighted_pred_avg_sse(uint8_t *_dst, ptrdiff_t _dststride,
                                        int16_t *src1, int16_t *src2, ptrdiff_t srcstride,
                                        int width, int height)
{
    int x, y;
    uint8_t *dst = (uint8_t*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(uint8_t);
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
            r0= _mm_load_si128((__m128i*)&src1[x]);
            r1= _mm_load_si128((__m128i*)&src1[x+8]);
            r2=_mm_load_si128((__m128i*)&src2[x]);
            r3= _mm_load_si128((__m128i*)&src2[x+8]);

            r0= _mm_adds_epi16(r0,f0);
            r1= _mm_adds_epi16(r1,f0);
            r0= _mm_adds_epi16(r0,r2);
            r1= _mm_adds_epi16(r1,r3);
            r0= _mm_srai_epi16(r0,shift);
            r1= _mm_srai_epi16(r1,shift);
            r0= _mm_packus_epi16(r0,r1);

            _mm_storeu_si128((__m128i*)(dst + x),r0);
        }
        dst  += dststride;
        src1 += srcstride;
        src2 += srcstride;
    }
}


void ff_hevc_weighted_pred_sse(uint8_t denom, int16_t wlxFlag, int16_t olxFlag,
                                     uint8_t *_dst, ptrdiff_t _dststride,
                                     int16_t *src, ptrdiff_t srcstride,
                                     int width, int height)
{

    int log2Wd;
    int x , y;

    uint8_t *dst = (uint8_t*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(uint8_t);
    __m128i x0,x1,x2,x3,c0,add,add2;

    log2Wd = denom + 14 - BIT_DEPTH;

    add= _mm_set1_epi32(olxFlag * ( 1 << ( BIT_DEPTH - 8 ) ));
    add2= _mm_set1_epi32(1 << (log2Wd - 1));
    c0= _mm_set1_epi16(wlxFlag);
    if (log2Wd >= 1)
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=16) {
            x0= _mm_load_si128((__m128i*)&src[x]);
            x2= _mm_load_si128((__m128i*)&src[x+8]);
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

            _mm_storeu_si128((__m128i*)(dst+x),x0);

        }
        dst  += dststride;
        src  += srcstride;
    }else
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=16) {

            x0= _mm_load_si128((__m128i*)&src[x]);
            x2= _mm_load_si128((__m128i*)&src[x+8]);
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

            _mm_storeu_si128((__m128i*)(dst +x),x0);

        }
        dst  += dststride;
        src  += srcstride;
    }
}

void ff_hevc_weighted_pred_avg_sse(uint8_t denom, int16_t wl0Flag, int16_t wl1Flag,
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
    uint8_t *dst = (uint8_t*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(uint8_t);
    __m128i x0,x1,x2,x3,r0,r1,r2,r3,c0,c1,c2;
    shift = 14 - BIT_DEPTH;
    log2Wd = denom + shift;

    o0 = (ol0Flag) * ( 1 << ( BIT_DEPTH - 8 ) );
    o1 = (ol1Flag) * ( 1 << ( BIT_DEPTH - 8 ) );
    shift2= (log2Wd + 1);
    w0 = wl0Flag;
    w1 = wl1Flag;
    c0= _mm_set1_epi16(wl0Flag);
    c1= _mm_set1_epi16(wl1Flag);
    c2= _mm_set1_epi32((o0 + o1 + 1) << log2Wd);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=16) {
            x0= _mm_load_si128((__m128i*)&src1[x]);
            x1= _mm_load_si128((__m128i*)&src1[x+8]);
            x2= _mm_load_si128((__m128i*)&src2[x]);
            x3= _mm_load_si128((__m128i*)&src2[x+8]);

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

            r0= _mm_packus_epi32(r0,r2);
            r1= _mm_packus_epi32(r1,r3);
            r0= _mm_packus_epi16(r0,r1);

            _mm_storeu_si128((__m128i*)(dst+x),r0);


        }
        dst  += dststride;
        src1 += srcstride;
        src2 += srcstride;
    }
}
