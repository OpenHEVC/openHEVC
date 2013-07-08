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


void ff_hevc_put_hevc_epel_pixels_sse(int16_t *dst, ptrdiff_t dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int width, int height, int mx, int my, int16_t* mcbuffer)
{
    int x, y;
    __m128i x1, x2;
    uint8_t *src = (uint8_t*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(uint8_t);
    if(width == 4){
        for (y = 0; y < height; y++) {
            x1= _mm_loadu_si128((__m128i*)&src[0]);
            x2 = _mm_unpacklo_epi8(x1,_mm_set1_epi8(0));
            x2= _mm_slli_epi16(x2,14 - BIT_DEPTH);

            _mm_storel_epi64((__m128i*)&dst[0],x2);
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
                _mm_store_si128((__m128i*)&dst[x], x2);
                _mm_store_si128((__m128i*)&dst[x+8], x1);

            }
            src += srcstride;
            dst += dststride;
        }

}


void ff_hevc_put_hevc_epel_h_sse(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my, int16_t* mcbuffer)
{
    int x, y;
    uint8_t *src = (uint8_t*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(uint8_t);
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
            _mm_storel_epi64((__m128i*)&dst[0],x2);

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
                _mm_store_si128((__m128i*)&dst[x],x2);
            }
            src += srcstride;
            dst += dststride;
        }
    }
}

void ff_hevc_put_hevc_epel_v_sse(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my, int16_t* mcbuffer)
{
    int x, y;
    __m128i x0,x1,x2,x3,t0,t1,t2,t3,r0,f0,f1,f2,f3,r1;
    uint8_t *src = (uint8_t*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(uint8_t);
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
            _mm_storel_epi64((__m128i*)&dst[0], r0);\

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
                _mm_store_si128((__m128i*)&dst[x], r0);
                _mm_storeu_si128((__m128i*)&dst[x+8], r1);
            }
            src += srcstride;
            dst += dststride;
        }

}

void ff_hevc_put_hevc_epel_hv_sse(int16_t *dst, ptrdiff_t dststride,
                                   uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int mx, int my, int16_t* mcbuffer)
{
    int x, y;
    uint8_t *src = (uint8_t*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(uint8_t);
    const int8_t *filter_h = epel_filters[mx-1];
    const int8_t *filter_v = epel_filters[my-1];
    __m128i r0, bshuffle1, bshuffle2, x0, x1, x2,x3, t0, t1, t2, t3, f0, f1, f2, f3,r1,r2;
    int8_t filter_0 = filter_h[0];
    int8_t filter_1 = filter_h[1];
    int8_t filter_2 = filter_h[2];
    int8_t filter_3 = filter_h[3];
    r0= _mm_set_epi8(filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0,filter_3,filter_2,filter_1, filter_0);
    bshuffle1=_mm_set_epi8(6,5,4,3,5,4,3,2,4,3,2,1,3,2,1,0);
    //DECLARE_ALIGNED( 16, int16_t, tmp_array[(MAX_PB_SIZE+3)*MAX_PB_SIZE] );
    //#ifndef OPTIMIZATION_ENABLE
    // int16_t tmp_array[(MAX_PB_SIZE+3)*MAX_PB_SIZE];
    //#endif
     int16_t *tmp = mcbuffer;

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
            _mm_storel_epi64((__m128i*)&tmp[0], x1);

            tmp += MAX_PB_SIZE;
            _mm_storel_epi64((__m128i*)&tmp[0], x2);

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
                _mm_store_si128((__m128i*)&tmp[x],x2);
            }
            src += srcstride;
            tmp += MAX_PB_SIZE;
        }
    }

    tmp = mcbuffer + epel_extra_before * MAX_PB_SIZE;

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
            _mm_store_si128((__m128i*)&dst[x],r0);
        }
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }

}


