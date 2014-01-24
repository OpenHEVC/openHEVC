#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
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

    t0   = _mm_set1_epi16(top[4]);
    l0   = _mm_set1_epi16(left[4]);
    add  = _mm_set1_epi16(4);
    tmp1 = _mm_set_epi16(0,1,2,3,0,1,2,3);
    mask = _mm_set_epi32(0,0,0,-1);

    ly   = _mm_loadl_epi64((__m128i*)left);            //get 16 values
    ly   = _mm_unpacklo_epi8(ly,_mm_setzero_si128());  //drop to 8 values 16 bit
    ly   = _mm_unpacklo_epi16(ly, ly);

    tx   = _mm_loadl_epi64((__m128i*)top);             //get 16 value
    tx   = _mm_unpacklo_epi8(tx,_mm_setzero_si128());  //drop to 8 values 16 bit
    tx   = _mm_unpacklo_epi64(tx,tx);

    c1   = _mm_mullo_epi16(_mm_set_epi16(4,3,2,1,4,3,2,1),t0);
    c1   = _mm_add_epi16(c1,add);

    ly1 = _mm_unpacklo_epi32(ly, ly);
    c0  = _mm_mullo_epi16(tmp1,ly1);
    c2  = _mm_mullo_epi16(_mm_set_epi16(2,2,2,2,3,3,3,3),tx);
    c3  = _mm_mullo_epi16(_mm_set_epi16(2,2,2,2,1,1,1,1),l0);

    ly1 = _mm_unpackhi_epi32(ly, ly);
    C0  = _mm_mullo_epi16(tmp1,ly1);
    C2  = _mm_mullo_epi16(_mm_set_epi16(0,0,0,0,1,1,1,1),tx);
    C3  = _mm_mullo_epi16(_mm_set_epi16(4,4,4,4,3,3,3,3),l0);

    c0  = _mm_add_epi16(c0,c1);
    c2  = _mm_add_epi16(c2,c3);
    c0  = _mm_add_epi16(c0,c2);
    c0  = _mm_srli_epi16(c0,3);

    C0  = _mm_add_epi16(C0,c1);
    C2  = _mm_add_epi16(C2,C3);
    C0  = _mm_add_epi16(C0,C2);
    C0  = _mm_srli_epi16(C0,3);

    c0  = _mm_packus_epi16(c0,C0);

    _mm_maskmoveu_si128(c0,mask,(char*)(src)); //store only 4 values
    c0  = _mm_srli_si128(c0,4);
    _mm_maskmoveu_si128(c0,mask,(char*)(src + stride)); //store only 4 values
    c0  = _mm_srli_si128(c0,4);
    _mm_maskmoveu_si128(c0,mask,(char*)(src + 2*stride)); //store only 4 values
    c0  = _mm_srli_si128(c0,4);
    _mm_maskmoveu_si128(c0,mask,(char*)(src + 3*stride)); //store only 4 values

}
void pred_planar_1_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, t0, tx, l0, add, c0,c1,c2,c3, ly1, tmp1;

    t0   = _mm_set1_epi16(top[8]);
    l0   = _mm_set1_epi16(left[8]);
    add  = _mm_set1_epi16(8);

    ly   = _mm_loadl_epi64((__m128i*)left);            //get 16 values
    ly   = _mm_unpacklo_epi8(ly,_mm_setzero_si128());  //drop to 8 values 16 bit

    tx   = _mm_loadl_epi64((__m128i*)top);             //get 16 values
    tx   = _mm_unpacklo_epi8(tx,_mm_setzero_si128());  //drop to 8 values 16 bit

    tmp1 = _mm_set_epi16(0,1,2,3,4,5,6,7);

    c1   = _mm_mullo_epi16(_mm_set_epi16(8,7,6,5,4,3,2,1),t0);
    c1   = _mm_add_epi16(c1,add);

    for (y = 0; y < 8; y++) {
        ly1 = _mm_unpacklo_epi16(ly , ly );
        ly1 = _mm_unpacklo_epi32(ly1, ly1);
        ly1 = _mm_unpacklo_epi64(ly1, ly1);

        c0  = _mm_mullo_epi16(tmp1,ly1);
        c2  = _mm_mullo_epi16(_mm_set1_epi16(7 - y),tx);
        c3  = _mm_mullo_epi16(_mm_set1_epi16(1 + y),l0);

        c0  = _mm_add_epi16(c0,c1);
        c2  = _mm_add_epi16(c2,c3);
        c0  = _mm_add_epi16(c0,c2);

        c0  = _mm_srli_epi16(c0,4);
        c0  = _mm_packus_epi16(c0,_mm_setzero_si128());
        _mm_storel_epi64((__m128i*)(src), c0);   //store only 8
        src+= stride;

        ly  = _mm_srli_si128(ly,2);
    }

}

void pred_planar_2_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int y;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i ly, t0, tx, tl, th, l0, add, c0,c1,c2,c3, ly1, tmp1, tmp2, C0, C1, C2;

    t0   = _mm_set1_epi16(top[16]);
    l0   = _mm_set1_epi16(left[16]);
    add  = _mm_set1_epi16(16);

    ly   = _mm_loadu_si128((__m128i*)left);            //get 16 values
    tx   = _mm_loadu_si128((__m128i*)top);             //get 16 values
    tl   = _mm_unpacklo_epi8(tx,_mm_setzero_si128());
    th   = _mm_unpackhi_epi8(tx,_mm_setzero_si128());

    tmp1 = _mm_set_epi16( 8, 9,10,11,12,13,14,15);
    tmp2 = _mm_set_epi16( 0, 1, 2, 3, 4, 5, 6, 7);

    c1   = _mm_mullo_epi16(_mm_set_epi16( 8, 7, 6, 5, 4, 3, 2, 1),t0);
    C1   = _mm_mullo_epi16(_mm_set_epi16(16,15,14,13,12,11,10, 9),t0);
    c1   = _mm_add_epi16(c1,add);
    C1   = _mm_add_epi16(C1,add);

    for (y = 0; y < 16; y++){
        ly1 = _mm_unpacklo_epi8(ly , _mm_setzero_si128() );
        ly1 = _mm_unpacklo_epi16(ly1, ly1);
        ly1 = _mm_unpacklo_epi32(ly1, ly1);
        ly1 = _mm_unpacklo_epi64(ly1, ly1);

        c3  = _mm_mullo_epi16(_mm_set1_epi16(1+y),l0);

        c0  = _mm_mullo_epi16(tmp1, ly1);
        c2  = _mm_mullo_epi16(_mm_set1_epi16(15 - y), tl);
        c0  = _mm_add_epi16(c0,c1);
        c2  = _mm_add_epi16(c2,c3);
        c0  = _mm_add_epi16(c0,c2);

        C0  = _mm_mullo_epi16(tmp2, ly1);
        C2  = _mm_mullo_epi16(_mm_set1_epi16(15 - y), th);
        C0  = _mm_add_epi16(C0,C1);
        C2  = _mm_add_epi16(C2,c3);
        C0  = _mm_add_epi16(C0,C2);

        c0  = _mm_srli_epi16(c0,5);
        C0  = _mm_srli_epi16(C0,5);
        c0  = _mm_packus_epi16(c0,C0);
        _mm_storeu_si128((__m128i*)(src), c0);
        src+= stride;

        ly  = _mm_srli_si128(ly,1);
    }
}

void pred_planar_3_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                         ptrdiff_t stride)
{
    int x, y, i;
    uint8_t *src = (uint8_t*)_src;
    const uint8_t *top  = (const uint8_t*)_top;
    const uint8_t *left = (const uint8_t*)_left;

    __m128i l0, ly, ly1, tx, TX, c0, c3, tmp1_lo, tmp1_hi, TMP1_LO, TMP1_HI;
    __m128i c1_1, c1_2, C1_1, C1_2, res;


    tmp1_lo = _mm_set_epi16(24,25,26,27,28,29,30,31);
    tmp1_hi = _mm_set_epi16(16,17,18,19,20,21,22,23);
    TMP1_LO = _mm_set_epi16( 8, 9,10,11,12,13,14,15);
    TMP1_HI = _mm_set_epi16( 0, 1, 2, 3, 4, 5, 6, 7);

    tx   = _mm_set1_epi16(top[32]);
    c1_1 = _mm_mullo_epi16(_mm_set_epi16( 8, 7, 6, 5, 4, 3, 2, 1), tx);
    C1_1 = _mm_mullo_epi16(_mm_set_epi16(16,15,14,13,12,11,10, 9), tx);
    c1_2 = _mm_mullo_epi16(_mm_set_epi16(24,23,22,21,20,19,18,17), tx);
    C1_2 = _mm_mullo_epi16(_mm_set_epi16(32,31,30,29,28,27,26,25), tx);
    c0   = _mm_set1_epi16(32);
    c1_1 = _mm_add_epi16(c1_1, c0);
    C1_1 = _mm_add_epi16(C1_1, c0);
    c1_2 = _mm_add_epi16(c1_2, c0);
    C1_2 = _mm_add_epi16(C1_2, c0);

    l0   = _mm_set1_epi16(left[32]);
    ly   = _mm_loadu_si128((__m128i*)left);            //get 16 values
    tx   = _mm_loadu_si128((__m128i*)top);             //get 16 values
    TX   = _mm_loadu_si128((__m128i*)(top + 16));

    for (i = 0; i < 2; i++) {
        for (y = 0+i*16; y < 16+i*16; y++) {
            ly1 = _mm_unpacklo_epi8(ly , _mm_setzero_si128() );
            ly1 = _mm_unpacklo_epi16(ly1, ly1);
            ly1 = _mm_unpacklo_epi32(ly1, ly1);
            ly1 = _mm_unpacklo_epi64(ly1, ly1);

            c3  = _mm_mullo_epi16(_mm_set1_epi16(1+y),l0);

            c0  = _mm_mullo_epi16(tmp1_lo, ly1);
            c0  = _mm_add_epi16(c0, c1_1);
            c0  = _mm_add_epi16(c0,   c3);
            c0  = _mm_add_epi16(c0, _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpacklo_epi8(tx,_mm_setzero_si128())));
            res = _mm_srli_epi16(c0,  6);

            c0  = _mm_mullo_epi16(tmp1_hi, ly1);
            c0  = _mm_add_epi16(c0, C1_1);
            c0  = _mm_add_epi16(c0,   c3);
            c0  = _mm_add_epi16(c0, _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpackhi_epi8(tx,_mm_setzero_si128())));
            c0  = _mm_srli_epi16(c0,   6);

            res = _mm_packus_epi16(res,c0);
            _mm_storeu_si128((__m128i*) src, res);

            // second half of 32

            c0  = _mm_mullo_epi16(TMP1_LO, ly1);
            c0  = _mm_add_epi16(c0, c1_2);
            c0  = _mm_add_epi16(c0,  c3);
            c0  = _mm_add_epi16(c0, _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpacklo_epi8(TX,_mm_setzero_si128())));
            res = _mm_srli_epi16(c0,  6);

            c0  = _mm_mullo_epi16(TMP1_HI, ly1);
            c0  = _mm_add_epi16(c0, C1_2);
            c0  = _mm_add_epi16(c0,   c3);
            c0  = _mm_add_epi16(c0, _mm_mullo_epi16(_mm_set1_epi16(31 - y),_mm_unpackhi_epi8(TX,_mm_setzero_si128())));
            c0  = _mm_srli_epi16(c0,    6);

            res = _mm_packus_epi16(res,c0);
            _mm_storeu_si128((__m128i*)(src + 16), res);
            src+= stride;

            ly  = _mm_srli_si128(ly,1);
        }
        ly= _mm_loadu_si128((__m128i*)(left+16));            //get 16 values
    }
}
#define TRANSPOSE4x4B(in, sstep_in, out, sstep_out)                            \
    do {                                                                       \
        __m128i m0  = _mm_loadl_epi64((__m128i *) &in[0*sstep_in]);            \
        __m128i m1  = _mm_loadl_epi64((__m128i *) &in[1*sstep_in]);            \
        __m128i m2  = _mm_loadl_epi64((__m128i *) &in[2*sstep_in]);            \
        __m128i m3  = _mm_loadl_epi64((__m128i *) &in[3*sstep_in]);            \
                                                                               \
        __m128i m10 = _mm_unpacklo_epi8(m0, m1);                               \
        __m128i m11 = _mm_unpacklo_epi8(m2, m3);                               \
                                                                               \
        m0  = _mm_unpacklo_epi16(m10, m11);                                    \
        m1  = _mm_srli_si128(m0, 4);                                           \
        m2  = _mm_srli_si128(m0, 8);                                           \
        m3  = _mm_srli_si128(m0,12);                                           \
                                                                               \
        _mm_maskmoveu_si128(m0, mask, (char *) &out[0*sstep_out]);             \
        _mm_maskmoveu_si128(m1, mask, (char *) &out[1*sstep_out]);             \
        _mm_maskmoveu_si128(m2, mask, (char *) &out[2*sstep_out]);             \
        _mm_maskmoveu_si128(m3, mask, (char *) &out[3*sstep_out]);             \
    } while (0)
#define TRANSPOSE8x8B(in, sstep_in, out, sstep_out)                            \
    do {                                                                       \
        __m128i m0  = _mm_loadl_epi64((__m128i *) &in[0*sstep_in]);            \
        __m128i m1  = _mm_loadl_epi64((__m128i *) &in[1*sstep_in]);            \
        __m128i m2  = _mm_loadl_epi64((__m128i *) &in[2*sstep_in]);            \
        __m128i m3  = _mm_loadl_epi64((__m128i *) &in[3*sstep_in]);            \
        __m128i m4  = _mm_loadl_epi64((__m128i *) &in[4*sstep_in]);            \
        __m128i m5  = _mm_loadl_epi64((__m128i *) &in[5*sstep_in]);            \
        __m128i m6  = _mm_loadl_epi64((__m128i *) &in[6*sstep_in]);            \
        __m128i m7  = _mm_loadl_epi64((__m128i *) &in[7*sstep_in]);            \
                                                                               \
        __m128i m10 = _mm_unpacklo_epi8(m0, m1);                               \
        __m128i m11 = _mm_unpacklo_epi8(m2, m3);                               \
        __m128i m12 = _mm_unpacklo_epi8(m4, m5);                               \
        __m128i m13 = _mm_unpacklo_epi8(m6, m7);                               \
                                                                               \
        m0  = _mm_unpacklo_epi16(m10, m11);                                    \
        m1  = _mm_unpacklo_epi16(m12, m13);                                    \
        m2  = _mm_unpackhi_epi16(m10, m11);                                    \
        m3  = _mm_unpackhi_epi16(m12, m13);                                    \
                                                                               \
        m10 = _mm_unpacklo_epi32(m0 , m1 );                                    \
        m11 = _mm_unpacklo_epi32(m2 , m3 );                                    \
        m12 = _mm_unpackhi_epi32(m0 , m1 );                                    \
        m13 = _mm_unpackhi_epi32(m2 , m3 );                                    \
                                                                               \
        _mm_storel_epi64((__m128i *) &out[0*sstep_out], m10);                  \
        _mm_storel_epi64((__m128i *) &out[1*sstep_out], _mm_srli_si128(m10,8));\
        _mm_storel_epi64((__m128i *) &out[2*sstep_out], m12);                  \
        _mm_storel_epi64((__m128i *) &out[3*sstep_out], _mm_srli_si128(m12,8));\
        _mm_storel_epi64((__m128i *) &out[4*sstep_out], m11);                  \
        _mm_storel_epi64((__m128i *) &out[5*sstep_out], _mm_srli_si128(m11,8));\
        _mm_storel_epi64((__m128i *) &out[6*sstep_out], m13);                  \
        _mm_storel_epi64((__m128i *) &out[7*sstep_out], _mm_srli_si128(m13,8));\
    } while (0)
#define TRANSPOSE16x16B(in, sstep_in, out, sstep_out)                          \
    do {                                                                       \
        TRANSPOSE8x8B((&in[0*sstep_in+0]), sstep_in, (&out[0*sstep_out+0]), sstep_out);\
        TRANSPOSE8x8B((&in[0*sstep_in+8]), sstep_in, (&out[8*sstep_out+0]), sstep_out);\
        TRANSPOSE8x8B((&in[8*sstep_in+0]), sstep_in, (&out[0*sstep_out+8]), sstep_out);\
        TRANSPOSE8x8B((&in[8*sstep_in+8]), sstep_in, (&out[8*sstep_out+8]), sstep_out);\
    } while (0)
#define TRANSPOSE32x32B(in, sstep_in, out, sstep_out)                          \
    do {                                                                       \
        TRANSPOSE16x16B((&in[ 0*sstep_in+ 0]), sstep_in, (&out[ 0*sstep_out+ 0]), sstep_out);\
        TRANSPOSE16x16B((&in[ 0*sstep_in+16]), sstep_in, (&out[16*sstep_out+ 0]), sstep_out);\
        TRANSPOSE16x16B((&in[16*sstep_in+ 0]), sstep_in, (&out[ 0*sstep_out+16]), sstep_out);\
        TRANSPOSE16x16B((&in[16*sstep_in+16]), sstep_in, (&out[16*sstep_out+16]), sstep_out);\
    } while (0)

#define ANGULAR_COMPUTE(dst, src)                                              \
    dst = _mm_shuffle_epi8(src, shuffle);                                      \
    dst = _mm_maddubs_epi16(dst, r3);                                          \
    dst = _mm_adds_epi16(dst, add);                                            \
    dst = _mm_srai_epi16(dst, 5)
#define CLIP_PIXEL(src1, src2)                                                 \
    r3  = _mm_loadu_si128((__m128i*)src1);                                     \
    r1  = _mm_set1_epi16(src1[-1]);                                            \
    r2  = _mm_set1_epi16(src2[0]);                                             \
    r0  = _mm_unpacklo_epi8(r3,_mm_setzero_si128());                           \
    r0  = _mm_subs_epi16(r0, r1);                                              \
    r0  = _mm_srai_epi16(r0, 1);                                               \
    r0  = _mm_add_epi16(r0, r2)
#define CLIP_PIXEL_HI()                                                        \
    r3  = _mm_unpackhi_epi8(r3,_mm_setzero_si128());                           \
    r3  = _mm_subs_epi16(r3, r1);                                              \
    r3  = _mm_srai_epi16(r3, 1);                                               \
    r3  = _mm_add_epi16(r3, r2)

void pred_angular_0_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                          ptrdiff_t stride, int c_idx, int mode)
{
    const int intra_pred_angle[] = {
         32, 26, 21, 17, 13,  9,  5,  2,  0, -2, -5, -9,-13,-17,-21,-26,
        -32,-26,-21,-17,-13, -9, -5, -2,  0,  2,  5,  9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int x, y;
    __m128i r0, r1, r2, r3;
    const __m128i mask    = _mm_set_epi32(0,0,0,-1);
    const __m128i mask2   = _mm_set_epi32(0,0,255,-1);
    const __m128i add     = _mm_set1_epi16(16);
    const __m128i shuffle = _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
    const uint8_t *top    = (const uint8_t*)_top;
    const uint8_t *left   = (const uint8_t*)_left;
    const int     size   = 4;
    uint8_t *ref;
    uint8_t  src_tmp[4*4];
    uint8_t  ref_array[3*(4+1)];
    uint8_t *src   = (uint8_t*)_src;
    uint8_t *p_src = &src_tmp[0];
    int      angle = intra_pred_angle[mode-2];
    int      last  = (size * angle) >> 5;

    if (mode >= 18) {
        ref = (uint8_t*) (top - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            r0 = _mm_loadl_epi64((__m128i*)(top-1));
            _mm_maskmoveu_si128(r0, mask2, (char*) ref);
        }
        for (y = 0; y < size; y++) {
            int idx  = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+1));
                ANGULAR_COMPUTE(r0, r1);
                r0 = _mm_packus_epi16(r0,r0);
            } else {
                r0= _mm_loadl_epi64((__m128i*)(ref+idx+1));
            }
            _mm_maskmoveu_si128(r0,mask,(char*) src);
            src += stride;
        }
        if (mode == 26 && c_idx == 0) {
            src = (uint8_t*)_src;
            CLIP_PIXEL(left, top);
            r0  = _mm_packus_epi16(r0, r0);
            for (y = 0; y < size; y++) {
                _mm_maskmoveu_si128(r0, _mm_set_epi32(0,0,0,255), (char*) src);
                src += stride;
                r0  = _mm_srli_si128(r0,1);
            }
        }
    } else {
        ref = (uint8_t*) (left - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            r0 = _mm_loadl_epi64((__m128i*)(left-1));
            _mm_maskmoveu_si128(r0, mask2, (char*) ref);
        }
        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                ANGULAR_COMPUTE(r0, r1);
                r0 = _mm_packus_epi16(r0, r0);
            } else {
                r0= _mm_loadl_epi64((__m128i*)(ref+idx+1));
            }
            _mm_maskmoveu_si128(r0,mask,(char*) p_src);
            p_src += size;
        }
        TRANSPOSE4x4B(src_tmp, size, src, stride);
        if (mode == 10 && c_idx == 0) {
            CLIP_PIXEL(top, left);
            r0  = _mm_packus_epi16(r0, r0);
           _mm_maskmoveu_si128(r0, mask, (char*)(src));
        }
    }
}

void pred_angular_1_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
        ptrdiff_t stride, int c_idx, int mode)
{
    const int intra_pred_angle[] = {
         32, 26, 21, 17, 13,  9,  5,  2,  0, -2, -5, -9,-13,-17,-21,-26,
        -32,-26,-21,-17,-13, -9, -5, -2,  0,  2,  5,  9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int x, y;
    __m128i r0, r1, r2, r3;
    const __m128i mask    = _mm_set_epi32(0,0,0,-1);
    const __m128i mask2   = _mm_set_epi32(0,255,-1,-1);
    const __m128i add     = _mm_set1_epi16(16);
    const __m128i shuffle = _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
    const uint8_t *top    = (const uint8_t*)_top;
    const uint8_t *left   = (const uint8_t*)_left;
    const int     size   = 8;
    uint8_t *ref;
    uint8_t  src_tmp[8*8];
    uint8_t  ref_array[3*(8+1)];
    uint8_t *src   = (uint8_t*)_src;
    uint8_t *p_src = &src_tmp[0];
    int      angle = intra_pred_angle[mode-2];
    int      last  = (size * angle) >> 5;

    if (mode >= 18) {
        ref = (uint8_t*) (top - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            r0 = _mm_loadu_si128((__m128i*)(top-1));
            _mm_maskmoveu_si128(r0, mask2, (char*) ref);
        }
        for (y = 0; y < size; y++) {
            int idx  = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+1));
                ANGULAR_COMPUTE(r0, r1);
                r0 = _mm_packus_epi16(r0,r0);
            } else {
                r0= _mm_loadl_epi64((__m128i*)(ref+idx+1));
            }
            _mm_storel_epi64((__m128i*) src, r0);
            src += stride;
        }
        if (mode == 26 && c_idx == 0) {
            src = (uint8_t*)_src;
            CLIP_PIXEL(left, top);
            r0  = _mm_packus_epi16(r0, r0);
            for (y = 0; y < size; y++) {
                _mm_maskmoveu_si128(r0, _mm_set_epi32(0,0,0,255), (char*) src);
                src += stride;
                r0  = _mm_srli_si128(r0,1);
            }
        }
    } else {
        ref = (uint8_t*) (left - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            r0 = _mm_loadu_si128((__m128i*)(left-1));
            _mm_maskmoveu_si128(r0, mask2, (char*) ref);
        }
        for (x = 0; x < size; x++) {
            int idx  = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+1));
                ANGULAR_COMPUTE(r0, r1);
                r0 = _mm_packus_epi16(r0, r0);
            } else {
                r0= _mm_loadl_epi64((__m128i*)(ref+idx+1));
            }
            _mm_storel_epi64((__m128i*)p_src, r0);
            p_src += size;
        }
        TRANSPOSE8x8B(src_tmp, size, src, stride);
        if (mode == 10 && c_idx == 0) {
            CLIP_PIXEL(top, left);
            r0  = _mm_packus_epi16(r0, r0);
            _mm_storel_epi64((__m128i*)src, r0);
        }
    }
}
void pred_angular_2_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
        ptrdiff_t stride, int c_idx, int mode)
{
    const int intra_pred_angle[] = {
         32, 26, 21, 17, 13,  9,  5,  2,  0, -2, -5, -9,-13,-17,-21,-26,
        -32,-26,-21,-17,-13, -9, -5, -2,  0,  2,  5,  9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int x, y;
    __m128i r0, r1, r2, r3;
    const __m128i mask    = _mm_set_epi32(0,0,0,-1);
    const __m128i mask2   = _mm_set_epi32(0,255,-1,-1);
    const __m128i add     = _mm_set1_epi16(16);
    const __m128i shuffle = _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
    const uint8_t *top    = (const uint8_t*)_top;
    const uint8_t *left   = (const uint8_t*)_left;
    const int     size   = 16;
    uint8_t *ref;
    uint8_t  src_tmp[16*16];
    uint8_t  ref_array[3*(16+1)];
    uint8_t *src   = (uint8_t*)_src;
    uint8_t *p_src = &src_tmp[0];
    int      angle = intra_pred_angle[mode-2];
    int      last  = (size * angle) >> 5;

    if (mode >= 18) {
        ref = (uint8_t*) (top - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
             r0 = _mm_loadu_si128((__m128i*)(top-1));
             _mm_storeu_si128((__m128i *) ref, r0);
             ref[16] = top[15];
        }
        for (y = 0; y < size; y++) {
            int idx  = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+1));
                ANGULAR_COMPUTE(r0, r1);
                r2 = _mm_loadu_si128((__m128i*)(ref+idx+17));
                r1 = _mm_unpackhi_epi64(r1,_mm_slli_si128(r2,8));
                ANGULAR_COMPUTE(r1, r1);
                r0 = _mm_packus_epi16(r0,r1);
            } else {
                r0 = _mm_loadu_si128((__m128i*)(ref+idx+1));
            }
            _mm_storeu_si128((__m128i *) src, r0);
            src += stride;
        }
        if (mode == 26 && c_idx == 0) {
            src = (uint8_t*)_src;
            CLIP_PIXEL(left, top);
            CLIP_PIXEL_HI();
            r0  = _mm_packus_epi16(r0, r3);
            for (y = 0; y < size; y++) {
                _mm_maskmoveu_si128(r0, _mm_set_epi32(0,0,0,255), (char*) src);
                src += stride;
                r0  = _mm_srli_si128(r0,1);
            }
        }
    } else {
        ref = (uint8_t*) (left - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            r0 = _mm_loadu_si128((__m128i*)(left-1));
            _mm_storeu_si128((__m128i *) ref, r0);
            ref[16] = left[15];
        }
        for (x = 0; x < size; x++) {
            int idx  = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+1));
                ANGULAR_COMPUTE(r0, r1);
                r2 = _mm_loadu_si128((__m128i*)(ref+idx+17));
                r1 = _mm_unpackhi_epi64(r1,_mm_slli_si128(r2,8));
                ANGULAR_COMPUTE(r1, r1);
                r0 = _mm_packus_epi16(r0,r1);
            } else {
                r0 = _mm_loadu_si128((__m128i*)(ref+idx+1));
            }
            _mm_storeu_si128((__m128i *) p_src, r0);
            p_src += size;
        }
        TRANSPOSE16x16B(src_tmp, size, src, stride);
        if (mode == 10 && c_idx == 0) {
            CLIP_PIXEL(top, left);
            CLIP_PIXEL_HI();
            r0 = _mm_packus_epi16(r0, r3);
            _mm_storeu_si128((__m128i*) src , r0);
        }
    }
}
void pred_angular_3_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
        ptrdiff_t stride, int c_idx, int mode)
{
    const int intra_pred_angle[] = {
         32, 26, 21, 17, 13,  9,  5,  2,  0, -2, -5, -9,-13,-17,-21,-26,
        -32,-26,-21,-17,-13, -9, -5, -2,  0,  2,  5,  9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int x, y;
    __m128i r0, r1, r2, r3, r4;
    const __m128i mask    = _mm_set_epi32(0,0,0,-1);
    const __m128i mask2   = _mm_set_epi32(0,255,-1,-1);
    const __m128i add     = _mm_set1_epi16(16);
    const __m128i shuffle = _mm_set_epi8(8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0);
    const uint8_t *top    = (const uint8_t*)_top;
    const uint8_t *left   = (const uint8_t*)_left;
    const int     size   = 32;
    uint8_t *ref;
    uint8_t  src_tmp[32*32];
    uint8_t  ref_array[3*(32+1)];
    uint8_t *src   = (uint8_t*)_src;
    uint8_t *p_src = &src_tmp[0];
    int      angle = intra_pred_angle[mode-2];
    int      last  = (size * angle) >> 5;

    if (mode >= 18) {
        ref = (uint8_t*) (top - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            r0= _mm_loadu_si128((__m128i*)(top-1));
            _mm_store_si128((__m128i *) ref, r0);
            r0= _mm_loadu_si128((__m128i*)(top+15));
            _mm_store_si128((__m128i *) (ref + 16),r0);
            ref[32] = top[31];
        }
        for (y = 0; y < size; y++) {
            int idx  = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+1));
                ANGULAR_COMPUTE(r0, r1);
                r2 = _mm_loadu_si128((__m128i*)(ref+idx+17));
                r4 = _mm_unpackhi_epi64(r1,_mm_slli_si128(r2,8));
                ANGULAR_COMPUTE(r4, r4);
                r0 = _mm_packus_epi16(r0, r4);
                _mm_store_si128((__m128i*) src, r0);
                ANGULAR_COMPUTE(r0, r2);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+33));
                r4 = _mm_unpackhi_epi64(r2, _mm_slli_si128(r1,8));
                ANGULAR_COMPUTE(r4, r4);
                r0 = _mm_packus_epi16(r0, r4);
            } else {
                r0 = _mm_loadu_si128((__m128i*) (ref+idx+1));
                _mm_storeu_si128((__m128i *) src ,r0);
                r0 = _mm_loadu_si128((__m128i*) (ref+idx+17));
            }
            _mm_storeu_si128((__m128i *) (src+16), r0);
            src += stride;
        }
    } else {
        ref = (uint8_t*) (left - 1);
        if (angle < 0 && last < -1) {
            ref = ref_array + size;
            for (x = last; x <= -1; x++)
                ref[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            r0 = _mm_loadu_si128((__m128i*)(left-1));
            _mm_storeu_si128((__m128i *) ref, r0);
            r0= _mm_loadu_si128((__m128i*)(left+15));
            _mm_storeu_si128((__m128i *) (ref+16), r0);
            ref[32] = left[31];
        }
        for (x = 0; x < size; x++) {
            int idx  = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                r3 = _mm_set_epi8(fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact,fact,32-fact);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+1));
                ANGULAR_COMPUTE(r0, r1);
                r2 = _mm_loadu_si128((__m128i*)(ref+idx+17));
                r4 = _mm_unpackhi_epi64(r1,_mm_slli_si128(r2,8));
                ANGULAR_COMPUTE(r4, r4);
                r0 = _mm_packus_epi16(r0, r4);
                _mm_store_si128((__m128i*) p_src, r0);
                ANGULAR_COMPUTE(r0, r2);
                r1 = _mm_loadu_si128((__m128i*)(ref+idx+33));
                r4 = _mm_unpackhi_epi64(r2, _mm_slli_si128(r1,8));
                ANGULAR_COMPUTE(r4, r4);
                r0 = _mm_packus_epi16(r0, r4);
            } else {
                r0 = _mm_loadu_si128((__m128i*) (ref+idx+1));
                _mm_storeu_si128((__m128i *) p_src ,r0);
                r0 = _mm_loadu_si128((__m128i*) (ref+idx+17));
            }
            _mm_storeu_si128((__m128i *) (p_src+16), r0);
            p_src += size;
        }
        TRANSPOSE32x32B(src_tmp, size, src, stride);
    }
}
