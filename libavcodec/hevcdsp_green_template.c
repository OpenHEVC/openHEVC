/*
 * HEVC video energy efficient dgreender
 * Morgan Lacour 2015
 */
#if CONFIG_GREEN
#include "get_bits.h"
#include "hevc.h"

#include "bit_depth_template.c"
#include "hevcdsp.h"
#include "hevcdsp_green.h" // Green

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
/** Green Luma 5taps interpolation filter */
#define QPEL_FILTER5(src, stride)                                               \
    (filter[0] * src[x - 2 * stride] +                                         \
     filter[1] * src[x - 	 stride] +                                         \
     filter[2] * src[x 			   ] +                                         \
     filter[3] * src[x +	 stride] +                                         \
     filter[4] * src[x + 2 * stride] +                                         \
     filter[5] * src[x + 3 * stride])
/** Green Luma 3taps interpolation filter */
#define QPEL_FILTER3(src, stride)                                               \
    (filter[0] * src[x -     stride] +                                         \
     filter[1] * src[x             ] +                                         \
     filter[2] * src[x +     stride] +                                         \
     filter[3] * src[x +   2*stride])
/** Green Luma 1tap interpolation filter */
#define QPEL_FILTER1(src, stride)                                               \
	    (filter[0] * src[x] +                                         \
	     filter[1] * src[x +     stride])

/** Green Luma 5taps H interpolation filter */
static void FUNC(put_hevc_qpel5_h)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green5_filters[mx - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 3taps H interpolation filter */
static void FUNC(put_hevc_qpel3_h)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green3_filters[mx - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}
/** Green Luma 1tap H interpolation filter */
static void FUNC(put_hevc_qpel1_h)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green1_filters[mx - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 5taps V interpolation filter */
static void FUNC(put_hevc_qpel5_v)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green5_filters[my - 1];
    for (y = 0; y < height; y++)  {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER5(src, srcstride) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 3taps V interpolation filter */
static void FUNC(put_hevc_qpel3_v)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green3_filters[my - 1];
    for (y = 0; y < height; y++)  {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER3(src, srcstride) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}
/** Green Luma 1tap V interpolation filter */
static void FUNC(put_hevc_qpel1_v)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green1_filters[my - 1];
    for (y = 0; y < height; y++)  {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER1(src, srcstride) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 5taps HV interpolation filter */
static void FUNC(put_hevc_qpel5_hv)(int16_t *dst,
                                   ptrdiff_t dststride,
                                   uint8_t *_src,
                                   ptrdiff_t _srcstride,
                                   int height, intptr_t mx,
                                   intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green5_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green5_filters[my - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER5(tmp, MAX_PB_SIZE) >> 6;
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

/** Green Luma 3taps HV interpolation filter */
static void FUNC(put_hevc_qpel3_hv)(int16_t *dst,
                                   ptrdiff_t dststride,
                                   uint8_t *_src,
                                   ptrdiff_t _srcstride,
                                   int height, intptr_t mx,
                                   intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green3_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green3_filters[my - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER3(tmp, MAX_PB_SIZE) >> 6;
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}
/** Green Luma 1tap HV interpolation filter */
static void FUNC(put_hevc_qpel1_hv)(int16_t *dst,
                                   ptrdiff_t dststride,
                                   uint8_t *_src,
                                   ptrdiff_t _srcstride,
                                   int height, intptr_t mx,
                                   intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green1_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green1_filters[my - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER1(tmp, MAX_PB_SIZE) >> 6;
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

/** Green Luma 5taps uni H interpolation filter */
static void FUNC(put_hevc_qpel5_uni_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                      uint8_t *_src, ptrdiff_t _srcstride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green5_filters[mx - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 3taps uni H interpolation filter */
static void FUNC(put_hevc_qpel3_uni_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                      uint8_t *_src, ptrdiff_t _srcstride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green3_filters[mx - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}
/** Green Luma 1tap uni H interpolation filter */
static void FUNC(put_hevc_qpel1_uni_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                      uint8_t *_src, ptrdiff_t _srcstride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green1_filters[mx - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 5taps bi H interpolation filter */
static void FUNC(put_hevc_qpel5_bi_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green5_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}


/** Green Luma 3taps bi H interpolation filter */
static void FUNC(put_hevc_qpel3_bi_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green3_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}
/** Green Luma 1tap bi H interpolation filter */
static void FUNC(put_hevc_qpel1_bi_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green1_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 5taps uni V interpolation filter */
static void FUNC(put_hevc_qpel5_uni_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                     uint8_t *_src, ptrdiff_t _srcstride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green5_filters[my - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(src, srcstride) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 3taps uni V interpolation filter */
static void FUNC(put_hevc_qpel3_uni_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                     uint8_t *_src, ptrdiff_t _srcstride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green3_filters[my - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(src, srcstride) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}
/** Green Luma 1tap uni V interpolation filter */
static void FUNC(put_hevc_qpel1_uni_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                     uint8_t *_src, ptrdiff_t _srcstride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green1_filters[my - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(src, srcstride) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 5taps bi V interpolation filter */
static void FUNC(put_hevc_qpel5_bi_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green5_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(src, srcstride) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 3taps bi V interpolation filter */
static void FUNC(put_hevc_qpel3_bi_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green3_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(src, srcstride) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}
/** Green Luma 1tap bi V interpolation filter */
static void FUNC(put_hevc_qpel1_bi_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green1_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(src, srcstride) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 5taps uni HV interpolation filter */
static void FUNC(put_hevc_qpel5_uni_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift =  14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green5_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green5_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(tmp, MAX_PB_SIZE) >> 6) + offset) >> shift);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

/** Green Luma 3taps uni HV interpolation filter */
static void FUNC(put_hevc_qpel3_uni_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift =  14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green3_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green3_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(tmp, MAX_PB_SIZE) >> 6) + offset) >> shift);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}
/** Green Luma 1tap uni HV interpolation filter */
static void FUNC(put_hevc_qpel1_uni_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift =  14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green1_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green1_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(tmp, MAX_PB_SIZE) >> 6) + offset) >> shift);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

/** Green Luma 5taps bi HV interpolation filter */
static void FUNC(put_hevc_qpel5_bi_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                      int16_t *src2, ptrdiff_t src2stride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green5_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green5_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(tmp, MAX_PB_SIZE) >> 6) + src2[x] + offset) >> shift);
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 3taps bi HV interpolation filter */
static void FUNC(put_hevc_qpel3_bi_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                      int16_t *src2, ptrdiff_t src2stride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green3_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green3_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(tmp, MAX_PB_SIZE) >> 6) + src2[x] + offset) >> shift);
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}
/** Green Luma 1tap bi HV interpolation filter */
static void FUNC(put_hevc_qpel1_bi_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                      int16_t *src2, ptrdiff_t src2stride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green1_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green1_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(tmp, MAX_PB_SIZE) >> 6) + src2[x] + offset) >> shift);
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 5taps uni H weighted interpolation filter */
static void FUNC(put_hevc_qpel5_uni_w_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green5_filters[mx - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 3taps uni H weighted interpolation filter */
static void FUNC(put_hevc_qpel3_uni_w_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green3_filters[mx - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}
/** Green Luma 1tap uni H weighted interpolation filter */
static void FUNC(put_hevc_qpel1_uni_w_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green1_filters[mx - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 5taps bi H weighted interpolation filter */
static void FUNC(put_hevc_qpel5_bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green5_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 3taps bi H weighted interpolation filter */
static void FUNC(put_hevc_qpel3_bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green3_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}
/** Green Luma 1tap bi H weighted interpolation filter */
static void FUNC(put_hevc_qpel1_bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green1_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 5taps uni V weighted interpolation filter */
static void FUNC(put_hevc_qpel5_uni_w_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green5_filters[my - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER5(src, srcstride) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 3taps uni V weighted interpolation filter */
static void FUNC(put_hevc_qpel3_uni_w_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green3_filters[my - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER3(src, srcstride) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}
/** Green Luma 1tap uni V weighted interpolation filter */
static void FUNC(put_hevc_qpel1_uni_w_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_green1_filters[my - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER1(src, srcstride) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}

/** Green Luma 5taps bi V weighted interpolation filter */
static void FUNC(put_hevc_qpel5_bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green5_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(src, srcstride) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 3taps bi V weighted interpolation filter */
static void FUNC(put_hevc_qpel3_bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green3_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(src, srcstride) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}
/** Green Luma 1tap bi V weighted interpolation filter */
static void FUNC(put_hevc_qpel1_bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_green1_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(src, srcstride) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 5taps uni HV weighted interpolation filter */
static void FUNC(put_hevc_qpel5_uni_w_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
                                         uint8_t *_src, ptrdiff_t _srcstride,
                                         int height, int denom, int wx, int ox,
                                         intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green5_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green5_filters[my - 1];

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER5(tmp, MAX_PB_SIZE) >> 6) * wx + offset) >> shift) + ox);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

/** Green Luma 3taps uni HV weighted interpolation filter */
static void FUNC(put_hevc_qpel3_uni_w_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
                                         uint8_t *_src, ptrdiff_t _srcstride,
                                         int height, int denom, int wx, int ox,
                                         intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green3_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green3_filters[my - 1];

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER3(tmp, MAX_PB_SIZE) >> 6) * wx + offset) >> shift) + ox);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}
/** Green Luma 1tap uni HV weighted interpolation filter */
static void FUNC(put_hevc_qpel1_uni_w_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
                                         uint8_t *_src, ptrdiff_t _srcstride,
                                         int height, int denom, int wx, int ox,
                                         intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green1_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green1_filters[my - 1];

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER1(tmp, MAX_PB_SIZE) >> 6) * wx + offset) >> shift) + ox);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

/** Green Luma 5taps bi HV weighted interpolation filter */
static void FUNC(put_hevc_qpel5_bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int16_t *src2, ptrdiff_t src2stride,
                                        int height, int denom, int wx0, int wx1,
                                        int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green5_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER5(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green5_filters[my - 1];

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER5(tmp, MAX_PB_SIZE) >> 6) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}

/** Green Luma 3taps bi HV weighted interpolation filter */
static void FUNC(put_hevc_qpel3_bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int16_t *src2, ptrdiff_t src2stride,
                                        int height, int denom, int wx0, int wx1,
                                        int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green3_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER3(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green3_filters[my - 1];

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER3(tmp, MAX_PB_SIZE) >> 6) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}
/** Green Luma 1tap bi HV weighted interpolation filter */
static void FUNC(put_hevc_qpel1_bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int16_t *src2, ptrdiff_t src2stride,
                                        int height, int denom, int wx0, int wx1,
                                        int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    const int8_t *filter;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    src   -= QPEL_EXTRA_BEFORE * srcstride;
    filter = ff_hevc_qpel_green1_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER1(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_green1_filters[my - 1];

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER1(tmp, MAX_PB_SIZE) >> 6) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}

/* Generic CHROMA Functions */

#if BIT_DEPTH < 14
    #define CHROMA_OFFSET int offset = 1 << (shift - 1);
#else
	#define CHROMA_OFFSET int offset = 0;
#endif

#define EPEL_H(NTAP) \
static void FUNC(put_hevc_epel ## NTAP ## _h)(int16_t *dst, ptrdiff_t dststride,	\
								  uint8_t *_src, ptrdiff_t _srcstride,				\
								  int height, intptr_t mx, intptr_t my, int width)	\
{																					\
	int x, y;																		\
	pixel *src = (pixel *)_src;														\
	ptrdiff_t srcstride  = _srcstride / sizeof(pixel);								\
	const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];			\
	for (y = 0; y < height; y++) {													\
		for (x = 0; x < width; x++)													\
			dst[x] = EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8);			\
		src += srcstride;															\
		dst += dststride;															\
	}																				\
}																					\

#define EPEL_V(NTAP) \
static void FUNC(put_hevc_epel ## NTAP ## _v)(int16_t *dst, ptrdiff_t dststride,    \
                                  uint8_t *_src, ptrdiff_t _srcstride,              \
                                  int height, intptr_t mx, intptr_t my, int width)  \
{                                                                                   \
    int x, y;                                                                       \
    pixel *src = (pixel *)_src;                                                     \
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);                               \
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];           \
                                                                                    \
    for (y = 0; y < height; y++) {                                                  \
        for (x = 0; x < width; x++)                                                 \
            dst[x] = EPEL_FILTER_GREEN(NTAP)(src, srcstride) >> (BIT_DEPTH - 8);    \
        src += srcstride;                                                           \
        dst += dststride;                                                           \
    }                                                                               \
}\

#define EPEL_HV(NTAP)\
static void FUNC(put_hevc_epel ## NTAP ## _hv)(int16_t *dst, ptrdiff_t dststride,	\
                                   uint8_t *_src, ptrdiff_t _srcstride,				\
                                   int height, intptr_t mx, intptr_t my, int width)	\
{																					\
    int x, y;																		\
    pixel *src = (pixel *)_src;														\
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);								\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];			\
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];					\
    int16_t *tmp = tmp_array;														\
    																				\
    src -= EPEL_EXTRA_BEFORE * srcstride;											\
    																				\
    for (y = 0; y < height + EPEL_EXTRA; y++) {										\
        for (x = 0; x < width; x++)													\
            tmp[x] = EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8);			\
        src += srcstride;															\
        tmp += MAX_PB_SIZE;															\
    }																				\
																					\
    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;							\
    filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];							\
    																				\
    for (y = 0; y < height; y++) {													\
        for (x = 0; x < width; x++)													\
            dst[x] = EPEL_FILTER_GREEN(NTAP)(tmp, MAX_PB_SIZE) >> 6;				\
        tmp += MAX_PB_SIZE;															\
        dst += dststride;															\
    }																				\
}																					\

#define EPEL_UNI_H(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _uni_h)(uint8_t *_dst, ptrdiff_t _dststride, 	\
									   uint8_t *_src, ptrdiff_t _srcstride, 			\
                                      int height, intptr_t mx, intptr_t my, int width) 	\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int shift = 14 - BIT_DEPTH;															\
    CHROMA_OFFSET																		\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift);\
        src += srcstride;																\
        dst += dststride;																\
    }																					\
}																						\

#define EPEL_BI_H(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_h)(uint8_t *_dst, ptrdiff_t _dststride,   \
									 uint8_t *_src, ptrdiff_t _srcstride,				\
                                     int16_t *src2, ptrdiff_t src2stride,				\
                                     int height, intptr_t mx, intptr_t my, int width)	\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int shift = 14 + 1 - BIT_DEPTH;														\
    CHROMA_OFFSET																		\
																						\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++) {													\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);\
        }																				\
        dst  += dststride;																\
        src  += srcstride;																\
        src2 += src2stride;																\
    }																					\
}																						\

#define EPEL_UNI_V(NTAP) \
static void FUNC(put_hevc_epel ## NTAP ## _uni_v)(uint8_t *_dst, ptrdiff_t _dststride,	\
												  uint8_t *_src, ptrdiff_t _srcstride,	\
												  int height, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];				\
    int shift = 14 - BIT_DEPTH;															\
    CHROMA_OFFSET																		\
																						\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(src, srcstride) >> (BIT_DEPTH - 8)) + offset) >> shift);\
        src += srcstride;																\
        dst += dststride;																\
    }																					\
}																						\

#define EPEL_BI_V(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_v)(uint8_t *_dst, ptrdiff_t _dststride,	\
										uint8_t *_src, ptrdiff_t _srcstride,			\
										int16_t *src2, ptrdiff_t src2stride,			\
										int height, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];				\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    int shift = 14 + 1 - BIT_DEPTH;														\
    CHROMA_OFFSET																		\
																						\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(src, srcstride) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);\
        dst  += dststride;																\
        src  += srcstride;																\
        src2 += src2stride;																\
    }																					\
}																						\

#define EPEL_UNI_HV(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _uni_hv)(uint8_t *_dst, ptrdiff_t _dststride, \
												uint8_t *_src, ptrdiff_t _srcstride,	\
                                       int height, intptr_t mx, intptr_t my, int width)	\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];						\
    int16_t *tmp = tmp_array;															\
    int shift = 14 - BIT_DEPTH;															\
    CHROMA_OFFSET																		\
																						\
    src -= EPEL_EXTRA_BEFORE * srcstride;												\
    																					\
    for (y = 0; y < height + EPEL_EXTRA; y++) {											\
        for (x = 0; x < width; x++)														\
            tmp[x] = EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8);				\
        src += srcstride;																\
        tmp += MAX_PB_SIZE;																\
    }																					\
																						\
    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;								\
    filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];								\
    																					\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(tmp, MAX_PB_SIZE) >> 6) + offset) >> shift);\
        tmp += MAX_PB_SIZE;																\
        dst += dststride;																\
    }																					\
}																						\

#define EPEL_BI_HV(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_hv)(uint8_t *_dst, ptrdiff_t _dststride, 	\
										uint8_t *_src, ptrdiff_t _srcstride,			\
										int16_t *src2, ptrdiff_t src2stride,			\
										int height, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];						\
    int16_t *tmp = tmp_array;															\
    int shift = 14 + 1 - BIT_DEPTH;														\
    CHROMA_OFFSET																		\
																						\
    src -= EPEL_EXTRA_BEFORE * srcstride;												\
    																					\
    for (y = 0; y < height + EPEL_EXTRA; y++) {											\
        for (x = 0; x < width; x++)														\
            tmp[x] = EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8);				\
        src += srcstride;																\
        tmp += MAX_PB_SIZE;																\
    }																					\
																						\
    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;								\
    filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];								\
    																					\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(tmp, MAX_PB_SIZE) >> 6) + src2[x] + offset) >> shift);\
        tmp  += MAX_PB_SIZE;															\
        dst  += dststride;																\
        src2 += src2stride;																\
    }																					\
}																						\

#define EPEL_UNI_W_H(NTAP) \
static void FUNC(put_hevc_epel ## NTAP ## _uni_w_h)(uint8_t *_dst, ptrdiff_t _dststride,\
													uint8_t *_src, ptrdiff_t _srcstride,\
													int height, int denom, int wx, int ox, \
													intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int shift = denom + 14 - BIT_DEPTH;													\
    CHROMA_OFFSET																		\
																						\
    ox     = ox * (1 << (BIT_DEPTH - 8));												\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++) {													\
            dst[x] = av_clip_pixel((((EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);\
        }																				\
        dst += dststride;																\
        src += srcstride;																\
    }																					\
}																						\

#define EPEL_BI_W_H(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride, \
												uint8_t *_src, ptrdiff_t _srcstride,	\
												int16_t *src2, ptrdiff_t src2stride,	\
												int height, int denom, int wx0, int wx1,\
												int ox0, int ox1, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int shift = 14 + 1 - BIT_DEPTH;														\
    int log2Wd = denom + shift - 1;														\
    																					\
    ox0     = ox0 * (1 << (BIT_DEPTH - 8));												\
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));												\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +\
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));		\
        src  += srcstride;																\
        dst  += dststride;																\
        src2 += src2stride;																\
    }																					\
}																						\

#define EPEL_UNI_W_V(NTAP) \
static void FUNC(put_hevc_epel ## NTAP ## _uni_w_v)(uint8_t *_dst, ptrdiff_t _dststride,\
													uint8_t *_src, ptrdiff_t _srcstride,\
                                        int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];				\
    int shift = denom + 14 - BIT_DEPTH;													\
    CHROMA_OFFSET																		\
																						\
    ox     = ox * (1 << (BIT_DEPTH - 8));												\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++) {													\
            dst[x] = av_clip_pixel((((EPEL_FILTER_GREEN(NTAP)(src, srcstride) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);\
        }																				\
        dst += dststride;																\
        src += srcstride;																\
    }																					\
}																						\

#define EPEL_BI_W_V(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride, \
													uint8_t *_src, ptrdiff_t _srcstride,\
													int16_t *src2, ptrdiff_t src2stride,\
												int height, int denom, int wx0, int wx1,\
												int ox0, int ox1, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];				\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    int shift = 14 + 1 - BIT_DEPTH;														\
    int log2Wd = denom + shift - 1;														\
    																					\
    ox0     = ox0 * (1 << (BIT_DEPTH - 8));												\
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));												\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(src, srcstride) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +\
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));		\
        src  += srcstride;																\
        dst  += dststride;																\
        src2 += src2stride;																\
    }																					\
}																						\

#define EPEL_UNI_W_HV(NTAP) 															\
static void FUNC(put_hevc_epel ## NTAP ## _uni_w_hv)(uint8_t *_dst, ptrdiff_t _dststride,\
													uint8_t *_src, ptrdiff_t _srcstride,\
                                         int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];						\
    int16_t *tmp = tmp_array;															\
    int shift = denom + 14 - BIT_DEPTH;													\
    CHROMA_OFFSET																		\
																						\
    src -= EPEL_EXTRA_BEFORE * srcstride;												\
    																					\
    for (y = 0; y < height + EPEL_EXTRA; y++) {											\
        for (x = 0; x < width; x++)														\
            tmp[x] = EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8);				\
        src += srcstride;																\
        tmp += MAX_PB_SIZE;																\
    }																					\
																						\
    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;								\
    filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];								\
    																					\
    ox     = ox * (1 << (BIT_DEPTH - 8));												\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel((((EPEL_FILTER_GREEN(NTAP)(tmp, MAX_PB_SIZE) >> 6) * wx + offset) >> shift) + ox);\
        tmp += MAX_PB_SIZE;																\
        dst += dststride;																\
    }																					\
}																						\

#define EPEL_BI_W_HV(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride,\
										uint8_t *_src, ptrdiff_t _srcstride,			\
                                        int16_t *src2, ptrdiff_t src2stride,			\
                                        int height, int denom, int wx0, int wx1,		\
                                        int ox0, int ox1, intptr_t mx, intptr_t my, int width)\
{																						\
    int x, y;																			\
    pixel *src = (pixel *)_src;															\
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);									\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter = ff_hevc_epel_green ## NTAP ##_filters[mx - 1];				\
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];						\
    int16_t *tmp = tmp_array;															\
    int shift = 14 + 1 - BIT_DEPTH;														\
    int log2Wd = denom + shift - 1;														\
    																					\
    src -= EPEL_EXTRA_BEFORE * srcstride;												\
    																					\
    for (y = 0; y < height + EPEL_EXTRA; y++) {											\
        for (x = 0; x < width; x++)														\
            tmp[x] = EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8);				\
        src += srcstride;																\
        tmp += MAX_PB_SIZE;																\
    }																					\
																						\
    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;								\
    filter = ff_hevc_epel_green ## NTAP ##_filters[my - 1];								\
    																					\
    ox0     = ox0 * (1 << (BIT_DEPTH - 8));												\
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));												\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(tmp, MAX_PB_SIZE) >> 6) * wx1 + src2[x] * wx0 +\
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));		\
        tmp  += MAX_PB_SIZE;															\
        dst  += dststride;																\
        src2 += src2stride;																\
    }																					\
}																						\

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

#define CHROMA_FUNC(NTAP) 	\
		EPEL_H(NTAP) 		\
		EPEL_V(NTAP) 		\
		EPEL_HV(NTAP) 		\
		EPEL_UNI_H(NTAP) 	\
		EPEL_UNI_V(NTAP) 	\
		EPEL_UNI_HV(NTAP) 	\
		EPEL_BI_H(NTAP) 	\
		EPEL_BI_V(NTAP) 	\
		EPEL_BI_HV(NTAP) 	\
		EPEL_UNI_W_H(NTAP) 	\
		EPEL_UNI_W_V(NTAP) 	\
		EPEL_UNI_W_HV(NTAP) \
		EPEL_BI_W_H(NTAP) 	\
		EPEL_BI_W_V(NTAP) 	\
		EPEL_BI_W_HV(NTAP)

#define EPEL_FILTER_GREEN(N) EPEL_FILTER_GREEN ## N

#define EPEL_FILTER_GREEN1(src, stride)                                               \
	    (filter[0] * src[x] +                                             	\
	     filter[1] * src[x + stride])

#define EPEL_FILTER_GREEN2(src, stride) \
		(filter[0] * src[x		   ] +  \
		 filter[1] * src[x + stride])

#define EPEL_FILTER_GREEN3(src, stride)                                    \
    (filter[0] * src[x -     stride] +                                     \
	 filter[1] * src[x] 		     +	                                   \
	 filter[2] * src[x +     stride] +	                                   \
	 filter[3] * src[x + 2 * stride])

CHROMA_FUNC(1)
CHROMA_FUNC(2)
CHROMA_FUNC(3)

#endif
