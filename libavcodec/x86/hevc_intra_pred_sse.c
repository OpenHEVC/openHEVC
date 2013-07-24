#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevcdata.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcpred.h"

#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define BIT_DEPTH 8

void pred_planar_0_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride)
{
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;
    __m128i ly, t0, tx, l0, add, c0,c1,c2,c3, ly1, tmp1, mask, C0,C2,C3;
    t0= _mm_set1_epi16(top[4]);
    l0= _mm_set1_epi16(left[4]);
    add= _mm_set1_epi16(4);

    ly= _mm_loadl_epi64((__m128i*)left);            //get 16 values
    ly= _mm_unpacklo_epi8(ly,_mm_setzero_si128());  //drop to 8 values 16 bit

    tx= _mm_loadl_epi64((__m128i*)top);             //get 16 value
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
    int y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, t0, tx, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2;
    t0= _mm_set1_epi16(top[8]);
    l0= _mm_set1_epi16(left[8]);
    add= _mm_set1_epi16(8);

    ly= _mm_loadl_epi64((__m128i*)left);            //get 16 values
    ly= _mm_unpacklo_epi8(ly,_mm_setzero_si128());  //drop to 8 values 16 bit

    tx= _mm_loadl_epi64((__m128i*)top);             //get 16 values
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
    int y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, t0, tx, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2, C0, C1, C2;
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
    int y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, LY, t0, tx, TX, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2, TMP1, TMP2, C0, C1, C2;
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

void pred_angular_0_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
        ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 4;
    __m128i r0,r1,r2,r3,r5,r9,r10,r11;

    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    const int intra_pred_angle[] = {
            32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
            -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
            -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
            -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    uint8_t ref_array[3*5];
    const uint8_t *ref;
    int last = (size * angle) >> 5;

    r10= _mm_set_epi8(0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1);
    r11= _mm_set_epi8(0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1);

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = last; x <= -1; x++)
                (ref_array + 4)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];

                r0= _mm_loadl_epi64((__m128i*)(top-1));
                _mm_maskmoveu_si128(r0,r10,(char*)(ref_array+4));

            ref = ref_array + 4;
        }

        for (y = 0; y < 4; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {

                r0=_mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r2=_mm_set1_epi16(16);
                r9= _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
                r3= _mm_loadu_si128((__m128i*)(ref+idx+1));
                r5=_mm_shuffle_epi8(r3,r9);
                r5= _mm_maddubs_epi16(r5,r0);
                r5=_mm_adds_epi16(r5,r2);
                r5=_mm_srai_epi16(r5,5);


                r5= _mm_packus_epi16(r5,r5);
                _mm_maskmoveu_si128(r5,r11,(char*)(src+stride*y));

            } else {

                    r0= _mm_loadl_epi64((__m128i*)(ref+idx+1));
                    _mm_maskmoveu_si128(r0,r11,(char*)(src+y*stride));

            }
        }
        if (mode == 26 && c_idx == 0) {
            for (y = 0; y < size; y++)
                src[stride * (y)] = av_clip_uint8(top[0] + ((left[y] - left[-1]) >> 1));
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {

            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];

            r0= _mm_loadl_epi64((__m128i*)(left-1));
            _mm_maskmoveu_si128(r0,r10,(char*)(ref_array+4));

            ref = ref_array + size;
        }

        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    src[(x) + stride * (y)] = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++) {
                    src[(x) + stride * (y)] = ref[y + idx + 1];
                }
            }
        }
        if (mode == 10 && c_idx == 0) {

            r0= _mm_loadl_epi64((__m128i*)top);
            r3= _mm_unpacklo_epi8(r0,_mm_setzero_si128());
            r1= _mm_set1_epi16(top[-1]);
            r2= _mm_set1_epi16(left[0]);


            r3= _mm_subs_epi16(r3,r1);

            r3= _mm_srai_epi16(r3,1);

            r3= _mm_add_epi16(r3,r2);

            r3= _mm_packus_epi16(r3,r3);
            _mm_maskmoveu_si128(r3,r11,(char*)(src));
        }
    }
}
void pred_angular_1_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
        ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 8;
    __m128i r0, r1, r2, r3, r5, r9;

    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    const int intra_pred_angle[] = {
            32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
            -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
            -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
            -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    uint8_t ref_array[3*9];
    const uint8_t *ref;
    int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];

            r0= _mm_loadl_epi64((__m128i*)(top-1));
            _mm_storel_epi64((__m128i *)(ref_array+8),r0);
                (ref_array + size)[8] = top[7];

            ref = ref_array + size;
        }

        for (y = 0; y < size; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {

                r0=_mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r2=_mm_set1_epi16(16);
                r9= _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
                r3= _mm_loadu_si128((__m128i*)(ref+idx+1));
                r5=_mm_shuffle_epi8(r3,r9);
                r5= _mm_maddubs_epi16(r5,r0);
                r5=_mm_adds_epi16(r5,r2);
                r5=_mm_srai_epi16(r5,5);

                r5= _mm_packus_epi16(r5,r5);

                _mm_storel_epi64((__m128i*)(src+stride*y),r5);


            } else {
                    r0= _mm_loadl_epi64((__m128i*)(ref+idx + 1));
                    _mm_storel_epi64((__m128i *)(src + stride*y),r0);
            }
        }
        if (mode == 26 && c_idx == 0) {
            for (y = 0; y < size; y++)
                src[stride * (y)] = av_clip_uint8(top[0] + ((left[y] - left[-1]) >> 1));
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];


                r0= _mm_loadl_epi64((__m128i*)(left-1));
                _mm_storel_epi64((__m128i *)(ref_array+8),r0);
                (ref_array + size)[8] = left[7];

            ref = ref_array + size;
        }

        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    src[(x) + stride * (y)] = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++) {
                    src[(x) + stride * (y)] = ref[y + idx + 1];
                }
            }
        }
        if (mode == 10 && c_idx == 0) {
            r0= _mm_loadl_epi64((__m128i*)top);
            r3= _mm_unpacklo_epi8(r0,_mm_setzero_si128());
            r1= _mm_set1_epi16(top[-1]);
            r2= _mm_set1_epi16(left[0]);


            r3= _mm_subs_epi16(r3,r1);

            r3= _mm_srai_epi16(r3,1);

            r3= _mm_add_epi16(r3,r2);

            r3= _mm_packus_epi16(r3,r3);
            _mm_storel_epi64((__m128i*)src,r3);
        }
    }
}
void pred_angular_2_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
        ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 16;
    __m128i r0, r1, r2, r3, r4, r5, r6, r9;

    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    const int intra_pred_angle[] = {
            32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
            -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
            -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
            -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    uint8_t ref_array[3*17];
    const uint8_t *ref;
    int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];

                r0= _mm_loadu_si128((__m128i*)(top-1));
                _mm_storeu_si128((__m128i *)(ref_array+16),r0);
                (ref_array + size)[16] = top[15];


            ref = ref_array + size;
        }

        for (y = 0; y < size; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {

                r0=_mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r2=_mm_set1_epi16(16);
                r9= _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
                r3= _mm_loadu_si128((__m128i*)(ref+idx+1));
                r4= _mm_loadu_si128((__m128i*)(ref+idx+17));
                r5=_mm_shuffle_epi8(r3,r9);
                r5= _mm_maddubs_epi16(r5,r0);
                r5=_mm_adds_epi16(r5,r2);
                r5=_mm_srai_epi16(r5,5);

                r6=_mm_unpackhi_epi64(r3,_mm_slli_si128(r4,8));
                r6=_mm_shuffle_epi8(r6,r9);
                r6= _mm_maddubs_epi16(r6,r0);
                r6=_mm_adds_epi16(r6,r2);
                r6=_mm_srai_epi16(r6,5);

                r5= _mm_packus_epi16(r5,r6);

                _mm_store_si128((__m128i*)(src+stride*y),r5);


            } else {
                    r0= _mm_loadu_si128((__m128i*)(ref+idx+1));
                    _mm_storeu_si128((__m128i *)(src+y*stride),r0);

            }
        }
        if (mode == 26 && c_idx == 0) {
            for (y = 0; y < size; y++)
                src[stride * (y)] = av_clip_uint8(top[0] + ((left[y] - left[-1]) >> 1));
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];

                r0= _mm_loadu_si128((__m128i*)(left-1));
                _mm_storeu_si128((__m128i *)(ref_array+16),r0);
                (ref_array + size)[16] = left[15];
            ref = ref_array + size;
        }

        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    src[(x) + stride * (y)] = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++) {
                    src[(x) + stride * (y)] = ref[y + idx + 1];
                }
            }
        }
        if (mode == 10 && c_idx == 0) {

            r0= _mm_loadu_si128((__m128i*)top);
            r3= _mm_unpacklo_epi8(r0,_mm_setzero_si128());
            r4= _mm_unpackhi_epi8(r0,_mm_setzero_si128());
            r1= _mm_set1_epi16(top[-1]);
            r2= _mm_set1_epi16(left[0]);


            r3= _mm_subs_epi16(r3,r1);
            r4= _mm_subs_epi16(r4,r1);

            r3= _mm_srai_epi16(r3,1);
            r4= _mm_srai_epi16(r4,1);

            r3= _mm_add_epi16(r3,r2);
            r4= _mm_add_epi16(r4,r2);

            r3= _mm_packus_epi16(r3,r4);
            _mm_storeu_si128((__m128i*)src,r3);
        }
    }
}
void pred_angular_3_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
        ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 32;
    __m128i r0, r2, r3, r4, r5, r6, r9;

    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    const int intra_pred_angle[] = {
            32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
            -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
            -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
            -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    DECLARE_ALIGNED(16,uint8_t, ref_array[3*33]);
    const uint8_t *ref;
    int last = (32 * angle) >> 5;
    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];

            r0= _mm_loadu_si128((__m128i*)(top-1));
            _mm_store_si128((__m128i *)(ref_array+32),r0);
            r0= _mm_loadu_si128((__m128i*)(top+15));
            _mm_store_si128((__m128i *)(ref_array+48),r0);
            (ref_array + size)[32] = top[31];
            ref = ref_array + 32;
        }

        for (y = 0; y < 32; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                r0=_mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r2=_mm_set1_epi16(16);
                r9= _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
                r3= _mm_loadu_si128((__m128i*)(ref+idx+1));
                r4= _mm_loadu_si128((__m128i*)(ref+idx+17));
                r5=_mm_shuffle_epi8(r3,r9);
                r5= _mm_maddubs_epi16(r5,r0);
                r5=_mm_adds_epi16(r5,r2);
                r5=_mm_srai_epi16(r5,5);

                r6=_mm_unpackhi_epi64(r3,_mm_slli_si128(r4,8));
                r6=_mm_shuffle_epi8(r6,r9);
                r6= _mm_maddubs_epi16(r6,r0);
                r6=_mm_adds_epi16(r6,r2);
                r6=_mm_srai_epi16(r6,5);

                r5= _mm_packus_epi16(r5,r6);

                _mm_store_si128((__m128i*)(src+stride*y),r5);

                r3=_mm_loadu_si128((__m128i*)(ref+idx+33));
                r5=_mm_shuffle_epi8(r4,r9);
                r5= _mm_maddubs_epi16(r5,r0);
                r5=_mm_adds_epi16(r5,r2);
                r5=_mm_srai_epi16(r5,5);

                r6=_mm_unpackhi_epi64(r4,_mm_slli_si128(r3,8));
                r6=_mm_shuffle_epi8(r6,r9);
                r6= _mm_maddubs_epi16(r6,r0);
                r6=_mm_adds_epi16(r6,r2);
                r6=_mm_srai_epi16(r6,5);

                r5= _mm_packus_epi16(r5,r6);

                _mm_store_si128((__m128i*)(src+stride*y+16),r5);

            } else {
                    r0= _mm_loadu_si128((__m128i*)(ref+idx+1));
                    _mm_storeu_si128((__m128i *)(src+y*stride),r0);

                    r0= _mm_loadu_si128((__m128i*)(ref+idx+17));
                    _mm_storeu_si128((__m128i *)(src+y*stride+16),r0);
            }
        }

    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];


                r0= _mm_loadu_si128((__m128i*)(left-1));
                _mm_storeu_si128((__m128i *)(ref_array+32),r0);

                r0= _mm_loadu_si128((__m128i*)(left+15));
                _mm_storeu_si128((__m128i *)(ref_array+48),r0);
                (ref_array + size)[32] = left[31];

            ref = ref_array + size;
        }
        for (x = 0; x < 32; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < 32; y++) {
                    src[(x) + stride * (y)] = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < 32; y++) {
                    src[(x) + stride * (y)] = ref[y + idx + 1];
                }
            }
        }
        /*for (y = 0; y < 32; y++) {
            for (x = 0; x < 32; x++) {
                int idx = ((x + 1) * angle) >> 5;
                int fact = ((x + 1) * angle) & 31;


                if (fact!=0) {
                    src[(x) + stride * (y)] = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                } else {
                    src[(x) + stride * (y)] = ref[y + idx + 1];

                }
            }
        }*/
    }
}
