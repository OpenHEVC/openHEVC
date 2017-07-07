/*
 * HEVC video energy efficient dgreender
 * Morgan Lacour 2015
 */

#if BIT_DEPTH == 8

#include "get_bits.h"
#include "hevc.h"
#include "config.h"

#include "bit_depth_template.c"
#include "hevcdsp.h"
#include "hevcdsp_green.h" // Green

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////


/** Green Luma 7taps interpolation filter */
#define QPEL_FILTER7(src, stride)                                              \
    (filter[0] * src[x - 3 * stride] +                                         \
     filter[1] * src[x - 2 * stride] +                                         \
     filter[2] * src[x - 1 * stride] +                                         \
     filter[3] * src[x 			   ] +                                         \
     filter[4] * src[x + 	 stride] +                                         \
     filter[5] * src[x + 2 * stride] +                                         \
     filter[6] * src[x + 3 * stride] +                                         \
     filter[7] * src[x + 4 * stride])
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
#define QPEL_GREEN_FILTER(ntaps) QPEL_FILTER##ntaps

#define LUMA_FUNC(NTAP) 			\
		FUNC_QPEL_H(NTAP) 			\
		FUNC_QPEL_V(NTAP) 			\
		FUNC_QPEL_HV(NTAP) 			\
		FUNC_QPEL_UNI_H(NTAP) 		\
		FUNC_QPEL_UNI_V(NTAP) 		\
		FUNC_QPEL_UNI_HV(NTAP) 		\
		FUNC_QPEL_BI_H(NTAP) 		\
		FUNC_QPEL_BI_V(NTAP) 		\
		FUNC_QPEL_BI_HV(NTAP) 		\
		FUNC_QPEL_UNI_W_H(NTAP) 	\
		FUNC_QPEL_UNI_W_V(NTAP) 	\
		FUNC_QPEL_UNI_W_HV(NTAP)	\
		FUNC_QPEL_BI_W_H(NTAP) 		\
		FUNC_QPEL_BI_W_V(NTAP) 		\
		FUNC_QPEL_BI_W_HV(NTAP)

#if BIT_DEPTH < 14
    #define LUMA_OFFSET int offset = 1 << (shift - 1);
#else
	#define LUMA_OFFSET int offset = 0;
#endif

/** Green Luma Ntaps H interpolation filter */
#define FUNC_QPEL_H(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_h)(int16_t *dst,  						\
                                  uint8_t *_src, ptrdiff_t _srcstride,				\
                                  int height, intptr_t mx, intptr_t my, int width)	\
{																					\
    int x, y;																		\
    pixel        *src       = (pixel*)_src;											\
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);							\
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];		\
    for (y = 0; y < height; y++) {													\
        for (x = 0; x < width; x++)													\
            dst[x] = QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8);			\
        src += srcstride;															\
        dst += MAX_PB_SIZE;															\
    }																				\
}

#define FUNC_QPEL_V(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_v)(int16_t *dst,  						\
                                  uint8_t *_src, ptrdiff_t _srcstride,				\
                                  int height, intptr_t mx, intptr_t my, int width)	\
{																					\
    int x, y;																		\
    pixel        *src       = (pixel*)_src;											\
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);							\
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[my - 1];		\
    for (y = 0; y < height; y++)  {													\
        for (x = 0; x < width; x++)													\
            dst[x] = QPEL_GREEN_FILTER(ntaps)(src, srcstride) >> (BIT_DEPTH - 8);	\
        src += srcstride;															\
        dst += MAX_PB_SIZE;															\
    }																				\
}

#define FUNC_QPEL_HV(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_hv)(int16_t *dst,							\
                                   uint8_t *_src,									\
                                   ptrdiff_t _srcstride,							\
                                   int height, intptr_t mx,							\
                                   intptr_t my, int width)							\
{																					\
    int x, y;																		\
    const int8_t *filter;															\
    pixel *src = (pixel*)_src;														\
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);								\
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];					\
    int16_t *tmp = tmp_array;														\
    																				\
    src   -= QPEL_EXTRA_BEFORE * srcstride;											\
    filter = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];							\
    for (y = 0; y < height + QPEL_EXTRA; y++) {										\
        for (x = 0; x < width; x++)													\
            tmp[x] = QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8);			\
        src += srcstride;															\
        tmp += MAX_PB_SIZE;															\
    }																				\
																					\
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;							\
    filter = ff_hevc_qpel_green## ntaps ##_filters[my - 1];							\
    for (y = 0; y < height; y++) {													\
        for (x = 0; x < width; x++)													\
            dst[x] = QPEL_GREEN_FILTER(ntaps)(tmp, MAX_PB_SIZE) >> 6;				\
        tmp += MAX_PB_SIZE;															\
        dst += MAX_PB_SIZE;															\
    }																				\
}

#define FUNC_QPEL_UNI_H(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_uni_h)(uint8_t *_dst,  ptrdiff_t _dststride,	\
                                      uint8_t *_src, ptrdiff_t _srcstride,				\
                                      int height, intptr_t mx, intptr_t my, int width)	\
{																						\
    int x, y;																			\
    pixel        *src       = (pixel*)_src;												\
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);								\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];			\
    int shift = 14 - BIT_DEPTH;															\
    																					\
    LUMA_OFFSET																			\
																						\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift);	\
        src += srcstride;																\
        dst += dststride;																\
    }																					\
}

#define FUNC_QPEL_BI_H(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_bi_h)(uint8_t *_dst, ptrdiff_t _dststride,	\
									 uint8_t *_src, ptrdiff_t _srcstride,				\
                                     int16_t *src2, 									\
                                     int height, intptr_t mx, intptr_t my, int width)	\
{																						\
    int x, y;																			\
    pixel        *src       = (pixel*)_src;												\
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);								\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    																					\
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];			\
    																					\
    int shift = 14  + 1 - BIT_DEPTH;													\
    LUMA_OFFSET																			\
																						\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);	\
        src  += srcstride;																\
        dst  += dststride;																\
        src2 += MAX_PB_SIZE;															\
    }																					\
}

#define FUNC_QPEL_UNI_V(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_uni_v)(uint8_t *_dst,  ptrdiff_t _dststride,	\
                                      uint8_t *_src, ptrdiff_t _srcstride,				\
                                      int height, intptr_t mx, intptr_t my, int width)	\
{																						\
    int x, y;																			\
    pixel        *src       = (pixel*)_src;												\
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);								\
    pixel *dst          = (pixel *)_dst;												\
    ptrdiff_t dststride = _dststride / sizeof(pixel);									\
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[my - 1];			\
    int shift = 14 - BIT_DEPTH;															\
    LUMA_OFFSET																			\
																						\
    for (y = 0; y < height; y++) {														\
        for (x = 0; x < width; x++)														\
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(src, srcstride) >> (BIT_DEPTH - 8)) + offset) >> shift);	\
        src += srcstride;																\
        dst += dststride;																\
    }																					\
}


#define FUNC_QPEL_BI_V(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_bi_v)(uint8_t *_dst, ptrdiff_t _dststride,	\
									 uint8_t *_src, ptrdiff_t _srcstride,				\
                                     int16_t *src2, 									\
                                     int height, intptr_t mx, intptr_t my, int width)	\
{																						\
    int x, y;                                                                           \
    pixel        *src       = (pixel*)_src;                                             \
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);                               \
    pixel *dst          = (pixel *)_dst;                                                \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                   \
                                                                                        \
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[my - 1];            \
                                                                                        \
    int shift = 14 + 1 - BIT_DEPTH;                                                     \
    LUMA_OFFSET																			\
                                                                                        \
    for (y = 0; y < height; y++) {                                                      \
        for (x = 0; x < width; x++)                                                     \
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(src, srcstride) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);  \
        src  += srcstride;                                                              \
        dst  += dststride;                                                              \
        src2 += MAX_PB_SIZE;                                                            \
    }                                                                                   \
}


#define FUNC_QPEL_UNI_HV(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_uni_hv)(uint8_t *_dst,  ptrdiff_t _dststride, \
                                       uint8_t *_src, ptrdiff_t _srcstride,             \
                                       int height, intptr_t mx, intptr_t my, int width) \
{                                                                                       \
    int x, y;                                                                           \
    const int8_t *filter;                                                               \
    pixel *src = (pixel*)_src;                                                          \
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);                                   \
    pixel *dst          = (pixel *)_dst;                                                \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                   \
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];                        \
    int16_t *tmp = tmp_array;                                                           \
    int shift =  14 - BIT_DEPTH;                                                        \
    LUMA_OFFSET																			\
                                                                                        \
    src   -= QPEL_EXTRA_BEFORE * srcstride;                                             \
    filter = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];                             \
    for (y = 0; y < height + QPEL_EXTRA; y++) {                                         \
        for (x = 0; x < width; x++)                                                     \
            tmp[x] = QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8);               \
        src += srcstride;                                                               \
        tmp += MAX_PB_SIZE;                                                             \
    }                                                                                   \
                                                                                        \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                               \
    filter = ff_hevc_qpel_green## ntaps ##_filters[my - 1];                             \
                                                                                        \
    for (y = 0; y < height; y++) {                                                      \
        for (x = 0; x < width; x++)                                                     \
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(tmp, MAX_PB_SIZE) >> 6) + offset) >> shift); \
        tmp += MAX_PB_SIZE;                                                             \
        dst += dststride;                                                               \
    }                                                                                   \
}


#define FUNC_QPEL_BI_HV(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_bi_hv)(uint8_t *_dst, ptrdiff_t _dststride,   \
                                      uint8_t *_src, ptrdiff_t _srcstride,              \
                                      int16_t *src2, 									\
                                      int height, intptr_t mx, intptr_t my, int width)  \
{                                                                                       \
    int x, y;                                                                           \
    const int8_t *filter;                                                               \
    pixel *src = (pixel*)_src;                                                          \
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);                                   \
    pixel *dst          = (pixel *)_dst;                                                \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                   \
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];                        \
    int16_t *tmp = tmp_array;                                                           \
    int shift = 14 + 1 - BIT_DEPTH;                                                     \
    LUMA_OFFSET																			\
                                                                                        \
    src   -= QPEL_EXTRA_BEFORE * srcstride;                                             \
    filter = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];                             \
    for (y = 0; y < height + QPEL_EXTRA; y++) {                                         \
        for (x = 0; x < width; x++)                                                     \
            tmp[x] = QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8);               \
        src += srcstride;                                                               \
        tmp += MAX_PB_SIZE;                                                             \
    }                                                                                   \
                                                                                        \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                               \
    filter = ff_hevc_qpel_green## ntaps ##_filters[my - 1];                             \
                                                                                        \
    for (y = 0; y < height; y++) {                                                      \
        for (x = 0; x < width; x++)                                                     \
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(tmp, MAX_PB_SIZE) >> 6) + src2[x] + offset) >> shift);    \
        tmp  += MAX_PB_SIZE;                                                            \
        dst  += dststride;                                                              \
        src2 += MAX_PB_SIZE;                                                             \
    }                                                                                   \
}

#define FUNC_QPEL_UNI_W_H(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_uni_w_h)(uint8_t *_dst,  ptrdiff_t _dststride,\
                                        uint8_t *_src, ptrdiff_t _srcstride,            \
                                        int height, int denom, int wx, int ox,          \
                                        intptr_t mx, intptr_t my, int width)            \
{                                                                                       \
    int x, y;                                                                           \
    pixel        *src       = (pixel*)_src;                                             \
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);                               \
    pixel *dst          = (pixel *)_dst;                                                \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                   \
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];            \
    int shift = denom + 14 - BIT_DEPTH;                                                 \
    LUMA_OFFSET																			\
                                                                                        \
    ox = ox * (1 << (BIT_DEPTH - 8));                                                   \
    for (y = 0; y < height; y++) {                                                      \
        for (x = 0; x < width; x++)                                                     \
            dst[x] = av_clip_pixel((((QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox); \
        src += srcstride;                                                               \
        dst += dststride;                                                               \
    }                                                                                   \
}

#define FUNC_QPEL_BI_W_H(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride,         \
                                       uint8_t *_src, ptrdiff_t _srcstride,                    \
                                       int16_t *src2, 										   \
                                       int height, int denom, int wx0, int wx1,                \
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)  \
{                                                                                              \
    int x, y;                                                                                  \
    pixel        *src       = (pixel*)_src;                                                    \
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);                                      \
    pixel *dst          = (pixel *)_dst;                                                       \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                          \
                                                                                               \
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];                   \
                                                                                               \
    int shift = 14  + 1 - BIT_DEPTH;                                                           \
    int log2Wd = denom + shift - 1;                                                            \
                                                                                               \
    ox0     = ox0 * (1 << (BIT_DEPTH - 8));                                                    \
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));                                                    \
    for (y = 0; y < height; y++) {                                                             \
        for (x = 0; x < width; x++)                                                            \
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 + \
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));             \
        src  += srcstride;                                                                     \
        dst  += dststride;                                                                     \
        src2 += MAX_PB_SIZE;                                                                   \
    }                                                                                          \
}

#define FUNC_QPEL_UNI_W_V(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_uni_w_v)(uint8_t *_dst,  ptrdiff_t _dststride,                                             \
                                        uint8_t *_src, ptrdiff_t _srcstride,                                                         \
                                        int height, int denom, int wx, int ox,                                                       \
                                        intptr_t mx, intptr_t my, int width)                                                         \
{                                                                                                                                    \
    int x, y;                                                                                                                        \
    pixel        *src       = (pixel*)_src;                                                                                          \
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);                                                                            \
    pixel *dst          = (pixel *)_dst;                                                                                             \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                                                                \
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[my - 1];                                                         \
    int shift = denom + 14 - BIT_DEPTH;                                                                                              \
    LUMA_OFFSET																														 \
                                                                                                                                     \
    ox = ox * (1 << (BIT_DEPTH - 8));                                                                                                \
    for (y = 0; y < height; y++) {                                                                                                   \
        for (x = 0; x < width; x++)                                                                                                  \
            dst[x] = av_clip_pixel((((QPEL_GREEN_FILTER(ntaps)(src, srcstride) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);   \
        src += srcstride;                                                                                                            \
        dst += dststride;                                                                                                            \
    }                                                                                                                                \
}

#define FUNC_QPEL_BI_W_V(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride,                                               \
                                       uint8_t *_src, ptrdiff_t _srcstride,                                                          \
                                       int16_t *src2, 						                                                         \
                                       int height, int denom, int wx0, int wx1,                                                      \
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)                                        \
{                                                                                                                                    \
    int x, y;                                                                                                                        \
    pixel        *src       = (pixel*)_src;                                                                                          \
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);                                                                            \
    pixel *dst          = (pixel *)_dst;                                                                                             \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                                                                \
                                                                                                                                     \
    const int8_t *filter    = ff_hevc_qpel_green## ntaps ##_filters[my - 1];                                                         \
                                                                                                                                     \
    int shift = 14 + 1 - BIT_DEPTH;                                                                                                  \
    int log2Wd = denom + shift - 1;                                                                                                  \
                                                                                                                                     \
    ox0     = ox0 * (1 << (BIT_DEPTH - 8));                                                                                          \
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));                                                                                          \
    for (y = 0; y < height; y++) {                                                                                                   \
        for (x = 0; x < width; x++)                                                                                                  \
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(src, srcstride) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +            \
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));                                                   \
        src  += srcstride;                                                                                                           \
        dst  += dststride;                                                                                                           \
        src2 += MAX_PB_SIZE;                                                                                                         \
    }                                                                                                                                \
}

#define FUNC_QPEL_UNI_W_HV(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_uni_w_hv)(uint8_t *_dst,  ptrdiff_t _dststride,                                            \
                                         uint8_t *_src, ptrdiff_t _srcstride,                                                        \
                                         int height, int denom, int wx, int ox,                                                      \
                                         intptr_t mx, intptr_t my, int width)                                                        \
{                                                                                                                                    \
    int x, y;                                                                                                                        \
    const int8_t *filter;                                                                                                            \
    pixel *src = (pixel*)_src;                                                                                                       \
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);                                                                                \
    pixel *dst          = (pixel *)_dst;                                                                                             \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                                                                \
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];                                                                     \
    int16_t *tmp = tmp_array;                                                                                                        \
    int shift = denom + 14 - BIT_DEPTH;                                                                                              \
    LUMA_OFFSET																														 \
                                                                                                                                     \
    src   -= QPEL_EXTRA_BEFORE * srcstride;                                                                                          \
    filter = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];                                                                          \
    for (y = 0; y < height + QPEL_EXTRA; y++) {                                                                                      \
        for (x = 0; x < width; x++)                                                                                                  \
            tmp[x] = QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8);                                                            \
        src += srcstride;                                                                                                            \
        tmp += MAX_PB_SIZE;                                                                                                          \
    }                                                                                                                                \
                                                                                                                                     \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                                                                            \
    filter = ff_hevc_qpel_green## ntaps ##_filters[my - 1];                                                                          \
                                                                                                                                     \
    ox = ox * (1 << (BIT_DEPTH - 8));                                                                                                \
    for (y = 0; y < height; y++) {                                                                                                   \
        for (x = 0; x < width; x++)                                                                                                  \
            dst[x] = av_clip_pixel((((QPEL_GREEN_FILTER(ntaps)(tmp, MAX_PB_SIZE) >> 6) * wx + offset) >> shift) + ox);               \
        tmp += MAX_PB_SIZE;                                                                                                          \
        dst += dststride;                                                                                                            \
    }                                                                                                                                \
}

#define FUNC_QPEL_BI_W_HV(ntaps) \
static void FUNC(put_hevc_qpel## ntaps ##_bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride,                                              \
                                        uint8_t *_src, ptrdiff_t _srcstride,                                                         \
                                        int16_t *src2, 						                                                         \
                                        int height, int denom, int wx0, int wx1,                                                     \
                                        int ox0, int ox1, intptr_t mx, intptr_t my, int width)                                       \
{                                                                                                                                    \
    int x, y;                                                                                                                        \
    const int8_t *filter;                                                                                                            \
    pixel *src = (pixel*)_src;                                                                                                       \
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);                                                                                \
    pixel *dst          = (pixel *)_dst;                                                                                             \
    ptrdiff_t dststride = _dststride / sizeof(pixel);                                                                                \
    int16_t tmp_array[(MAX_PB_SIZE + QPEL_EXTRA) * MAX_PB_SIZE];                                                                     \
    int16_t *tmp = tmp_array;                                                                                                        \
    int shift = 14 + 1 - BIT_DEPTH;                                                                                                  \
    int log2Wd = denom + shift - 1;                                                                                                  \
                                                                                                                                     \
    src   -= QPEL_EXTRA_BEFORE * srcstride;                                                                                          \
    filter = ff_hevc_qpel_green## ntaps ##_filters[mx - 1];                                                                          \
    for (y = 0; y < height + QPEL_EXTRA; y++) {                                                                                      \
        for (x = 0; x < width; x++)                                                                                                  \
            tmp[x] = QPEL_GREEN_FILTER(ntaps)(src, 1) >> (BIT_DEPTH - 8);                                                            \
        src += srcstride;                                                                                                            \
        tmp += MAX_PB_SIZE;                                                                                                          \
    }                                                                                                                                \
                                                                                                                                     \
    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;                                                                            \
    filter = ff_hevc_qpel_green## ntaps ##_filters[my - 1];                                                                          \
                                                                                                                                     \
    ox0     = ox0 * (1 << (BIT_DEPTH - 8));                                                                                          \
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));                                                                                          \
    for (y = 0; y < height; y++) {                                                                                                   \
        for (x = 0; x < width; x++)                                                                                                  \
            dst[x] = av_clip_pixel(((QPEL_GREEN_FILTER(ntaps)(tmp, MAX_PB_SIZE) >> 6) * wx1 + src2[x] * wx0 +                        \
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));                                                   \
        tmp  += MAX_PB_SIZE;                                                                                                         \
        dst  += dststride;                                                                                                           \
        src2 += MAX_PB_SIZE;                                                                                                         \
    }                                                                                                                                \
}

LUMA_FUNC(1)
LUMA_FUNC(3)
LUMA_FUNC(5)
LUMA_FUNC(7)

/* Generic CHROMA Functions */

#if BIT_DEPTH < 14
    #define CHROMA_OFFSET int offset = 1 << (shift - 1);
#else
	#define CHROMA_OFFSET int offset = 0;
#endif

#define FUNC_EPEL_H(NTAP) \
static void FUNC(put_hevc_epel ## NTAP ## _h)(int16_t *dst, 						\
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
		dst += MAX_PB_SIZE;															\
	}																				\
}

#define FUNC_EPEL_V(NTAP) \
static void FUNC(put_hevc_epel ## NTAP ## _v)(int16_t *dst,						    \
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
        dst += MAX_PB_SIZE;                                                         \
    }                                                                               \
}

#define FUNC_EPEL_HV(NTAP)\
static void FUNC(put_hevc_epel ## NTAP ## _hv)(int16_t *dst, 						\
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
        dst += MAX_PB_SIZE;															\
    }																				\
}

#define FUNC_EPEL_UNI_H(NTAP) 															\
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
            dst[x] = av_clip_pixel(((EPEL_FILTER_GREEN(NTAP)(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift); \
        src += srcstride;																\
        dst += dststride;																\
    }																					\
}

#define FUNC_EPEL_BI_H(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_h)(uint8_t *_dst, ptrdiff_t _dststride,   \
									 uint8_t *_src, ptrdiff_t _srcstride,				\
                                     int16_t *src2, 									\
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
        src2 += MAX_PB_SIZE;															\
    }																					\
}

#define FUNC_EPEL_UNI_V(NTAP) \
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

#define FUNC_EPEL_BI_V(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_v)(uint8_t *_dst, ptrdiff_t _dststride,	\
										uint8_t *_src, ptrdiff_t _srcstride,			\
										int16_t *src2,									\
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
        src2 += MAX_PB_SIZE;															\
    }																					\
}

#define FUNC_EPEL_UNI_HV(NTAP) 																\
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
}

#define FUNC_EPEL_BI_HV(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_hv)(uint8_t *_dst, ptrdiff_t _dststride, 	\
										uint8_t *_src, ptrdiff_t _srcstride,			\
										int16_t *src2,									\
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
        src2 += MAX_PB_SIZE;															\
    }																					\
}

#define FUNC_EPEL_UNI_W_H(NTAP) \
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
}

#define FUNC_EPEL_BI_W_H(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride, \
												uint8_t *_src, ptrdiff_t _srcstride,	\
												int16_t *src2, 							\
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
        src2 += MAX_PB_SIZE;															\
    }																					\
}

#define FUNC_EPEL_UNI_W_V(NTAP) \
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
}

#define FUNC_EPEL_BI_W_V(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride, \
													uint8_t *_src, ptrdiff_t _srcstride,\
													int16_t *src2, 						\
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
        src2 += MAX_PB_SIZE;															\
    }																					\
}

#define FUNC_EPEL_UNI_W_HV(NTAP) 															\
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
}

#define FUNC_EPEL_BI_W_HV(NTAP) 																\
static void FUNC(put_hevc_epel ## NTAP ## _bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride,\
										uint8_t *_src, ptrdiff_t _srcstride,			\
                                        int16_t *src2, 									\
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
        src2 += MAX_PB_SIZE;															\
    }																					\
}


////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

#define CHROMA_FUNC(NTAP) 			\
		FUNC_EPEL_H(NTAP) 			\
		FUNC_EPEL_V(NTAP) 			\
		FUNC_EPEL_HV(NTAP) 			\
		FUNC_EPEL_UNI_H(NTAP) 		\
		FUNC_EPEL_UNI_V(NTAP) 		\
		FUNC_EPEL_UNI_HV(NTAP) 		\
		FUNC_EPEL_BI_H(NTAP) 		\
		FUNC_EPEL_BI_V(NTAP) 		\
		FUNC_EPEL_BI_HV(NTAP) 		\
		FUNC_EPEL_UNI_W_H(NTAP) 	\
		FUNC_EPEL_UNI_W_V(NTAP) 	\
		FUNC_EPEL_UNI_W_HV(NTAP) 	\
		FUNC_EPEL_BI_W_H(NTAP) 		\
		FUNC_EPEL_BI_W_V(NTAP) 		\
		FUNC_EPEL_BI_W_HV(NTAP)

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
