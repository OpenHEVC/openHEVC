#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevcdata.h"
#include "libavcodec/hevc.h"

#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define BIT_DEPTH 8

void pred_planar_0_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride)
{
    int x, y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;
    __m128i ly, t0, tx, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2, mask, C0,C2,C3;
    t0= _mm_set1_epi16(top[4]);
    l0= _mm_set1_epi16(left[4]);
    add= _mm_set1_epi16(4);

    ly= _mm_loadu_si128((__m128i*)left);            //get 16 values
    ly= _mm_unpacklo_epi8(ly,_mm_setzero_si128());  //drop to 8 values 16 bit

    tx= _mm_loadu_si128((__m128i*)top);             //get 16 value
    tx= _mm_unpacklo_epi8(tx,_mm_setzero_si128());  //drop to 8 values 16 bit
    tx= _mm_unpacklo_epi64(tx,tx);
    tmp1= _mm_set_epi16(0,1,2,3,0,1,2,3);
    mask= _mm_set_epi8(0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1);

        ly1= _mm_unpacklo_epi64(_mm_set1_epi16(_mm_extract_epi16(ly,0)),_mm_set1_epi16(_mm_extract_epi16(ly,1)));
        ly= _mm_unpacklo_epi64(_mm_set1_epi16(_mm_extract_epi16(ly,2)),_mm_set1_epi16(_mm_extract_epi16(ly,3)));

        c0= _mm_mullo_epi16(tmp1,ly1);
        C0= _mm_mullo_epi16(tmp1,ly);
        c1= _mm_mullo_epi16(_mm_set_epi16(4,3,2,1,4,3,2,1),t0);
        c2= _mm_mullo_epi16(_mm_set_epi16(2,2,2,2,3,3,3,3),tx);

        C2= _mm_mullo_epi16(_mm_set_epi16(0,0,0,0,1,1,1,1),tx);
        c3= _mm_mullo_epi16(_mm_set_epi16(2,2,2,2,1,1,1,1),l0);
        C3= _mm_mullo_epi16(_mm_set_epi16(4,4,4,4,3,3,3,3),l0);

        c0= _mm_add_epi16(c0,c1);
        c2= _mm_add_epi16(c2,c3);
        C2= _mm_add_epi16(C2,C3);
        C0= _mm_add_epi16(C0,c1);
        c2= _mm_add_epi16(c2,add);
        C2= _mm_add_epi16(C2,add);
        c0= _mm_add_epi16(c0,c2);
        C0= _mm_add_epi16(C0,C2);

        c0= _mm_srli_epi16(c0,3);
        C0= _mm_srli_epi16(C0,3);

        c0= _mm_packus_epi16(c0,C0);

        _mm_maskmoveu_si128(c0,mask,(char*)(src)); //store only 4 values
        c0 = _mm_srli_si128(c0,4);

        _mm_maskmoveu_si128(c0,mask,(char*)(src + stride)); //store only 4 values

        c0 = _mm_srli_si128(c0,4);

        _mm_maskmoveu_si128(c0,mask,(char*)(src + 2*stride)); //store only 4 values

        c0 = _mm_srli_si128(c0,4);

        _mm_maskmoveu_si128(c0,mask,(char*)(src + 3*stride)); //store only 4 values



}
void pred_planar_1_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int x, y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, t0, tx, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2;
    t0= _mm_set1_epi16(top[8]);
    l0= _mm_set1_epi16(left[8]);
    add= _mm_set1_epi16(8);

    ly= _mm_loadu_si128((__m128i*)left);            //get 16 values
    ly= _mm_unpacklo_epi8(ly,_mm_setzero_si128());  //drop to 8 values 16 bit

    tx= _mm_loadu_si128((__m128i*)top);             //get 16 values
    tx= _mm_unpacklo_epi8(tx,_mm_setzero_si128());  //drop to 8 values 16 bit
    tmp1= _mm_set_epi16(0,1,2,3,4,5,6,7);
    tmp2= _mm_set_epi16(8,7,6,5,4,3,2,1);

    for (y = 0; y < 8; y++){

        ly1= _mm_set1_epi16(_mm_extract_epi16(ly,0));

        c0= _mm_mullo_epi16(tmp1,ly1);
        c1= _mm_mullo_epi16(tmp2,t0);
        c2= _mm_mullo_epi16(_mm_set1_epi16(7 - y),tx);
        c3= _mm_mullo_epi16(_mm_set1_epi16(1+y),l0);

        c0= _mm_add_epi16(c0,c1);
        c2= _mm_add_epi16(c2,c3);
        c2= _mm_add_epi16(c2,add);
        c0= _mm_add_epi16(c0,c2);

        c0= _mm_srli_epi16(c0,4);

        c0= _mm_packus_epi16(c0,_mm_setzero_si128());

        _mm_storel_epi64((__m128i*)(src + y*stride), c0);   //store only 8

        ly= _mm_srli_si128(ly,2);

    }

}

void pred_planar_2_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int x, y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, t0, tx, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2,C0,C1,C2,C3;
    t0= _mm_set1_epi16(top[16]);
    l0= _mm_set1_epi16(left[16]);
    add= _mm_set1_epi16(16);

    ly= _mm_loadu_si128((__m128i*)left);            //get 16 values

    tx= _mm_loadu_si128((__m128i*)top);             //get 16 values
    tmp1= _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
    tmp2= _mm_set_epi8(16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1);

    for (y = 0; y < 16; y++){
        ly1= _mm_set1_epi16(_mm_extract_epi8(ly,0));

        c0= _mm_mullo_epi16(_mm_unpacklo_epi8(tmp1,_mm_setzero_si128()),ly1);
        C0= _mm_mullo_epi16(_mm_unpackhi_epi8(tmp1,_mm_setzero_si128()),ly1);

        c1= _mm_mullo_epi16(_mm_unpacklo_epi8(tmp2,_mm_setzero_si128()),t0);
        C1= _mm_mullo_epi16(_mm_unpackhi_epi8(tmp2,_mm_setzero_si128()),t0);

        c2= _mm_mullo_epi16(_mm_set1_epi16(15 - y),_mm_unpacklo_epi8(tx,_mm_setzero_si128()));
        C2= _mm_mullo_epi16(_mm_set1_epi16(15 - y),_mm_unpackhi_epi8(tx,_mm_setzero_si128()));

        c3= _mm_mullo_epi16(_mm_set1_epi16(1+y),l0);

        c0= _mm_add_epi16(c0,c1);
        c2= _mm_add_epi16(c2,c3);
        c2= _mm_add_epi16(c2,add);
        c0= _mm_add_epi16(c0,c2);

        C0= _mm_add_epi16(C0,C1);
        C2= _mm_add_epi16(C2,c3);
        C2= _mm_add_epi16(C2,add);
        C0= _mm_add_epi16(C0,C2);

        c0= _mm_srli_epi16(c0,5);
        C0= _mm_srli_epi16(C0,5);

        c0= _mm_packus_epi16(c0,C0);

        _mm_storeu_si128((__m128i*)(src + y*stride), c0);
        ly= _mm_srli_si128(ly,1);
    }

}
void pred_planar_3_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int x, y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, LY, t0, tx, TX, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2, TMP1, TMP2,C0,C1,C2,C3;
        t0= _mm_set1_epi16(top[32]);
        l0= _mm_set1_epi16(left[32]);
        add= _mm_set1_epi16(32);

        ly= _mm_loadu_si128((__m128i*)left);            //get 16 values
        LY= _mm_loadu_si128((__m128i*)(left+16));

        tx= _mm_loadu_si128((__m128i*)top);             //get 16 values
        TX= _mm_loadu_si128((__m128i*)(top +16));
        TMP1= _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
        tmp1= _mm_set_epi8(16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31);
        tmp2= _mm_set_epi8(16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1);
        TMP2= _mm_set_epi8(32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17);

        for (y = 0; y < 16; y++){
            //first half of 32
            ly1= _mm_set1_epi16(_mm_extract_epi8(ly,0));
            c3= _mm_mullo_epi16(_mm_set1_epi16(1+y),l0);

            c0= _mm_mullo_epi16(_mm_unpacklo_epi8(tmp1,_mm_setzero_si128()),ly1);
            //printf("values check : tmp1 = %d, ly1= ");
            C0= _mm_mullo_epi16(_mm_unpackhi_epi8(tmp1,_mm_setzero_si128()),ly1);

            c1= _mm_mullo_epi16(_mm_unpacklo_epi8(tmp2,_mm_setzero_si128()),t0);
            C1= _mm_mullo_epi16(_mm_unpackhi_epi8(tmp2,_mm_setzero_si128()),t0);

            c2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpacklo_epi8(tx,_mm_setzero_si128()));
            C2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpackhi_epi8(tx,_mm_setzero_si128()));

            c0= _mm_add_epi16(c0,c1);
            c2= _mm_add_epi16(c2,c3);
            c2= _mm_add_epi16(c2,add);
            c0= _mm_add_epi16(c0,c2);

            C0= _mm_add_epi16(C0,C1);
            C2= _mm_add_epi16(C2,c3);
            C2= _mm_add_epi16(C2,add);
            C0= _mm_add_epi16(C0,C2);

            c0= _mm_srli_epi16(c0,6);
            C0= _mm_srli_epi16(C0,6);

            c0= _mm_packus_epi16(c0,C0);

            _mm_storeu_si128((__m128i*)(src + y*stride), c0);

            // second half of 32

            c0= _mm_mullo_epi16(_mm_unpacklo_epi8(TMP1,_mm_setzero_si128()),ly1);
            C0= _mm_mullo_epi16(_mm_unpackhi_epi8(TMP1,_mm_setzero_si128()),ly1);

            c1= _mm_mullo_epi16(_mm_unpacklo_epi8(TMP2,_mm_setzero_si128()),t0);
            C1= _mm_mullo_epi16(_mm_unpackhi_epi8(TMP2,_mm_setzero_si128()),t0);

            c2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpacklo_epi8(TX,_mm_setzero_si128()));
            C2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpackhi_epi8(TX,_mm_setzero_si128()));



            c0= _mm_add_epi16(c0,c1);
            c2= _mm_add_epi16(c2,c3);
            c2= _mm_add_epi16(c2,add);
            c0= _mm_add_epi16(c0,c2);

            C0= _mm_add_epi16(C0,C1);
            C2= _mm_add_epi16(C2,c3);
            C2= _mm_add_epi16(C2,add);
            C0= _mm_add_epi16(C0,C2);

            c0= _mm_srli_epi16(c0,6);
            C0= _mm_srli_epi16(C0,6);

            c0= _mm_packus_epi16(c0,C0);

            _mm_storeu_si128((__m128i*)(src + 16 + y*stride), c0);

            ly= _mm_srli_si128(ly,1);
        }

        for (y = 16; y < 32; y++){
            //first half of 32
            ly1= _mm_set1_epi16(_mm_extract_epi8(LY,0));
            c3= _mm_mullo_epi16(_mm_set1_epi16(1+y),l0);

            c0= _mm_mullo_epi16(_mm_unpacklo_epi8(tmp1,_mm_setzero_si128()),ly1);
            C0= _mm_mullo_epi16(_mm_unpackhi_epi8(tmp1,_mm_setzero_si128()),ly1);

            c1= _mm_mullo_epi16(_mm_unpacklo_epi8(tmp2,_mm_setzero_si128()),t0);
            C1= _mm_mullo_epi16(_mm_unpackhi_epi8(tmp2,_mm_setzero_si128()),t0);

            c2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpacklo_epi8(tx,_mm_setzero_si128()));
            C2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpackhi_epi8(tx,_mm_setzero_si128()));



            c0= _mm_add_epi16(c0,c1);
            c2= _mm_add_epi16(c2,c3);
            c2= _mm_add_epi16(c2,add);
            c0= _mm_add_epi16(c0,c2);

            C0= _mm_add_epi16(C0,C1);
            C2= _mm_add_epi16(C2,c3);
            C2= _mm_add_epi16(C2,add);
            C0= _mm_add_epi16(C0,C2);

            c0= _mm_srli_epi16(c0,6);
            C0= _mm_srli_epi16(C0,6);

            c0= _mm_packus_epi16(c0,C0);

            _mm_storeu_si128((__m128i*)(src + y*stride), c0);

            // second half of 32

            c0= _mm_mullo_epi16(_mm_unpacklo_epi8(TMP1,_mm_setzero_si128()),ly1);
            C0= _mm_mullo_epi16(_mm_unpackhi_epi8(TMP1,_mm_setzero_si128()),ly1);

            c1= _mm_mullo_epi16(_mm_unpacklo_epi8(TMP2,_mm_setzero_si128()),t0);
            C1= _mm_mullo_epi16(_mm_unpackhi_epi8(TMP2,_mm_setzero_si128()),t0);

            c2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpacklo_epi8(TX,_mm_setzero_si128()));
            C2= _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpackhi_epi8(TX,_mm_setzero_si128()));



            c0= _mm_add_epi16(c0,c1);
            c2= _mm_add_epi16(c2,c3);
            c2= _mm_add_epi16(c2,add);
            c0= _mm_add_epi16(c0,c2);

            C0= _mm_add_epi16(C0,C1);
            C2= _mm_add_epi16(C2,c3);
            C2= _mm_add_epi16(C2,add);
            C0= _mm_add_epi16(C0,C2);

            c0= _mm_srli_epi16(c0,6);
            C0= _mm_srli_epi16(C0,6);

            c0= _mm_packus_epi16(c0,C0);

            _mm_storeu_si128((__m128i*)(src + 16 + y*stride), c0);

            LY= _mm_srli_si128(LY,1);
        }


}
