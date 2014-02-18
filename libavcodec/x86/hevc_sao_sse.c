/*
 * Provide SSE sao functions for HEVC decoding
 * Copyright (c) 2013 Pierre-Edouard LEPERE
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */



#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevc.h"
#include "libavcodec/x86/hevcdsp.h"

#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define BIT_DEPTH 8

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define SAO_BAND_FILTER_INIT()                                                 \
    r0 = _mm_set1_epi16(sao_left_class & 31);                                  \
    r1 = _mm_set1_epi16((sao_left_class + 1) & 31);                            \
    r2 = _mm_set1_epi16((sao_left_class + 2) & 31);                            \
    r3 = _mm_set1_epi16((sao_left_class + 3) & 31);                            \
    sao1 = _mm_set1_epi16(sao_offset_val[1]);                                  \
    sao2 = _mm_set1_epi16(sao_offset_val[2]);                                  \
    sao3 = _mm_set1_epi16(sao_offset_val[3]);                                  \
    sao4 = _mm_set1_epi16(sao_offset_val[4])

#define SAO_BAND_FILTER_LOAD(x)                                                \
    src0 = _mm_loadu_si128((__m128i *) &src[x]);                               \
    src1 = _mm_unpackhi_epi8(src0, _mm_setzero_si128());                       \
    src0 = _mm_unpacklo_epi8(src0, _mm_setzero_si128());                       \
    src3 = _mm_srai_epi16(src1, shift);                                        \
    src2 = _mm_srai_epi16(src0, shift)

#define SAO_BAND_FILTER_COMPUTE(dst, src)                                      \
    x0  = _mm_cmpeq_epi16(src, r0);                                            \
    x1  = _mm_cmpeq_epi16(src, r1);                                            \
    x2  = _mm_cmpeq_epi16(src, r2);                                            \
    x3  = _mm_cmpeq_epi16(src, r3);                                            \
    x0  = _mm_and_si128(x0, sao1);                                             \
    x1  = _mm_and_si128(x1, sao2);                                             \
    x2  = _mm_and_si128(x2, sao3);                                             \
    x3  = _mm_and_si128(x3, sao4);                                             \
    x0  = _mm_or_si128(x0, x1);                                                \
    x2  = _mm_or_si128(x2, x3);                                                \
    x0  = _mm_or_si128(x0, x2);                                                \
    dst = _mm_add_epi16(dst, x0)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ff_hevc_sao_band_filter_0_8_sse(uint8_t *_dst, uint8_t *_src,
                                     ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width,
                                     int height, int c_idx) {
    uint8_t *dst = _dst;
    uint8_t *src = _src;
    ptrdiff_t stride = _stride;

    int y, x;
    int shift = 3;
    int *sao_offset_val = sao->offset_val[c_idx];
    int sao_left_class  = sao->band_position[c_idx];
    __m128i r0, r1, r2, r3, x0, x1, x2, x3, sao1, sao2, sao3, sao4, src0, src1,
    src2, src3;

    SAO_BAND_FILTER_INIT();
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x += 16) {
            SAO_BAND_FILTER_LOAD(x);
            SAO_BAND_FILTER_COMPUTE(src1, src3);
            SAO_BAND_FILTER_COMPUTE(src0, src2);
            src0 = _mm_packus_epi16(src0, src1);
            _mm_storeu_si128((__m128i *) &dst[x], src0);
        }
        dst += stride;
        src += stride;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define SAO_EDGE_FILTER_INIT()                                                 \
    int y_stride = init_y * stride;                                            \
    int pos_0_0 = pos[sao_eo_class][0][0];                                     \
    int pos_0_1 = pos[sao_eo_class][0][1];                                     \
    int pos_1_0 = pos[sao_eo_class][1][0];                                     \
    int pos_1_1 = pos[sao_eo_class][1][1];                                     \
    int y_stride_0_1 = (init_y + pos_0_1) * stride + pos_0_0;                  \
    int y_stride_1_1 = (init_y + pos_1_1) * stride + pos_1_0;                  \
    offset0 = _mm_set1_epi8(sao_offset_val[edge_idx[0]]);                      \
    offset1 = _mm_set1_epi8(sao_offset_val[edge_idx[1]]);                      \
    offset2 = _mm_set1_epi8(sao_offset_val[edge_idx[2]]);                      \
    offset3 = _mm_set1_epi8(sao_offset_val[edge_idx[3]]);                      \
    offset4 = _mm_set1_epi8(sao_offset_val[edge_idx[4]])
#define SAO_EDGE_FILTER_LOAD(x)                                                \
    x0   = _mm_loadu_si128((__m128i *) (src + x + y_stride));                  \
    cmp0 = _mm_loadu_si128((__m128i *) (src + x + y_stride_0_1));              \
    cmp1 = _mm_loadu_si128((__m128i *) (src + x + y_stride_1_1));              \
    r2   = _mm_min_epu8(x0, cmp0);                                             \
    x1   = _mm_cmpeq_epi8(cmp0, r2);                                           \
    x2   = _mm_cmpeq_epi8(x0, r2);                                             \
    x1   = _mm_sub_epi8(x2, x1);                                               \
    r2   = _mm_min_epu8(x0, cmp1);                                             \
    x3   = _mm_cmpeq_epi8(cmp1, r2);                                           \
    x2   = _mm_cmpeq_epi8(x0, r2);                                             \
    x3   = _mm_sub_epi8(x2, x3);                                               \
    x1   = _mm_add_epi8(x1, x3)
#define SAO_EDGE_FILTER_COMPUTE()                                              \
    r0 = _mm_cmpeq_epi8(x1, _mm_set1_epi8(-2));                                \
    r1 = _mm_cmpeq_epi8(x1, _mm_set1_epi8(-1));                                \
    r2 = _mm_cmpeq_epi8(x1, _mm_set1_epi8(0));                                 \
    r3 = _mm_cmpeq_epi8(x1, _mm_set1_epi8(1));                                 \
    r4 = _mm_cmpeq_epi8(x1, _mm_set1_epi8(2));                                 \
    r0 = _mm_and_si128(r0, offset0);                                           \
    r1 = _mm_and_si128(r1, offset1);                                           \
    r2 = _mm_and_si128(r2, offset2);                                           \
    r3 = _mm_and_si128(r3, offset3);                                           \
    r4 = _mm_and_si128(r4, offset4);                                           \
    r0 = _mm_add_epi8(r0, r1);                                                 \
    r2 = _mm_add_epi8(r2, r3);                                                 \
    r0 = _mm_add_epi8(r0, r4);                                                 \
    r0 = _mm_add_epi8(r0, r2);                                                 \
    r1 = _mm_unpacklo_epi8(_mm_setzero_si128(), r0);                           \
    r1 = _mm_srai_epi16(r1, 8);                                                \
    r2 = _mm_unpackhi_epi8(_mm_setzero_si128(), r0);                           \
    r2 = _mm_srai_epi16(r2, 8);                                                \
    r3 = _mm_unpacklo_epi8(x0, _mm_setzero_si128());                           \
    r4 = _mm_unpackhi_epi8(x0, _mm_setzero_si128());                           \
    r0 = _mm_add_epi16(r1, r3);                                                \
    r1 = _mm_add_epi16(r2, r4);                                                \
    r0 = _mm_packus_epi16(r0, r1)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ff_hevc_sao_edge_filter_0_8_sse(uint8_t *_dst, uint8_t *_src,
                                     ptrdiff_t _stride, struct SAOParams *sao, int *borders, int _width,
                                     int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge) {
    int x = 0, y = 0;
    uint8_t *dst = _dst;
    uint8_t *src = _src;
    ptrdiff_t stride = _stride;
    int chroma = c_idx != 0;
    //struct SAOParams *sao;
    int *sao_offset_val = sao->offset_val[c_idx];
    int sao_eo_class = sao->eo_class[c_idx];

    const int8_t pos[4][2][2] = { { { -1, 0 }, { 1, 0 } }, // horizontal
        { { 0, -1 }, { 0, 1 } }, // vertical
        { { -1, -1 }, { 1, 1 } }, // 45 degree
        { { 1, -1 }, { -1, 1 } }, // 135 degree
    };
    const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };

    int init_x = 0, init_y = 0, width = _width, height = _height;
    __m128i x0, x1, x2, x3, offset0, offset1, offset2, offset3, offset4, cmp0,
    cmp1, r0, r1, r2, r3, r4;

    dst = dst + (init_y * _stride + init_x);
    src = src + (init_y * _stride + init_x);
    init_y = init_x = 0;
    if (sao_eo_class != SAO_EO_HORIZ) {
        if (borders[1]) {
            x1 = _mm_set1_epi8(sao_offset_val[0]);
            for (x = init_x; x < width; x += 16) {
                x0 = _mm_loadu_si128((__m128i *) (src + x));
                x0 = _mm_add_epi8(x0, x1);
                _mm_storeu_si128((__m128i *) (dst + x), x0);
            }
            init_y = 1;
        }
        if (borders[3]) {
            int y_stride = stride * (_height - 1);
            x1 = _mm_set1_epi8(sao_offset_val[0]);
            for (x = init_x; x < width; x += 16) {
                x0 = _mm_loadu_si128((__m128i *) (src + x + y_stride));
                x0 = _mm_add_epi8(x0, x1);
                _mm_storeu_si128((__m128i *) (dst + x + y_stride), x0);
            }
            height--;
        }
    }

    {
        SAO_EDGE_FILTER_INIT();
        if (!(width & 15)) {
            for (y = init_y; y < height; y++) {
                for (x = init_x; x < width; x += 16) {
                    SAO_EDGE_FILTER_LOAD(x);
                    SAO_EDGE_FILTER_COMPUTE();
                    _mm_storeu_si128((__m128i *) (dst + x + y_stride), r0);
                }
                y_stride += stride;
                y_stride_0_1 += stride;
                y_stride_1_1 += stride;
            }
        } else {
            for (y = init_y; y < height; y++) {
                for (x = init_x; x < width; x += 4) {
                    SAO_EDGE_FILTER_LOAD(x);
                    SAO_EDGE_FILTER_COMPUTE();
                    *((uint32_t *) (dst + x + y_stride)) = _mm_cvtsi128_si32(r0);
                }
                y_stride += stride;
                y_stride_0_1 += stride;
                y_stride_1_1 += stride;
            }
        }
    }

    if (sao_eo_class != SAO_EO_VERT) {
        if (borders[0]) {
            int offset_val = sao_offset_val[0];
            int y_stride = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride] = av_clip_uint8(src[y_stride] + offset_val);
                y_stride += stride;
            }
            init_x = 1;
        }
        if (borders[2]) {
            int offset_val = sao_offset_val[0];
            int x_stride = _width - 1;
            for (x = 0; x < height; x++) {
                dst[x_stride] = av_clip_uint8(src[x_stride] + offset_val);
                x_stride += stride;
            }
            width--;
        }
    }
}

void ff_hevc_sao_edge_filter_1_8_sse(uint8_t *_dst, uint8_t *_src,
                                     ptrdiff_t _stride, struct SAOParams *sao, int *borders, int _width,
                                     int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge) {
    int x = 0, y = 0;
    uint8_t *dst = _dst;
    uint8_t *src = _src;
    ptrdiff_t stride = _stride;
    int chroma = c_idx != 0;
    //struct SAOParams *sao;
    int *sao_offset_val = sao->offset_val[c_idx];
    int sao_eo_class = sao->eo_class[c_idx];

    const int8_t pos[4][2][2] = { { { -1, 0 }, { 1, 0 } }, // horizontal
        { { 0, -1 }, { 0, 1 } }, // vertical
        { { -1, -1 }, { 1, 1 } }, // 45 degree
        { { 1, -1 }, { -1, 1 } }, // 135 degree
    };
    const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };

    int init_x = 0, init_y = 0, width = _width, height = _height;
    __m128i x0, x1, x2, x3, offset0, offset1, offset2, offset3, offset4, cmp0,
    cmp1, r0, r1, r2, r3, r4;

    dst = dst + (init_y * _stride + init_x);
    src = src + (init_y * _stride + init_x);
    init_y = init_x = 0;
    if (sao_eo_class != SAO_EO_HORIZ) {
        if (borders[1]) {
            x1 = _mm_set1_epi8(sao_offset_val[0]);
            for (x = init_x; x < width; x += 16) {
                x0 = _mm_loadu_si128((__m128i *) (src + x));
                x0 = _mm_add_epi8(x0, x1);
                _mm_storeu_si128((__m128i *) (dst + x), x0);
            }
            init_y = 1;
        }
        if (borders[3]) {
            int y_stride = stride * (_height - 1);
            x1 = _mm_set1_epi8(sao_offset_val[0]);
            for (x = init_x; x < width; x += 16) {
                x0 = _mm_loadu_si128((__m128i *) (src + x + y_stride));
                x0 = _mm_add_epi8(x0, x1);
                _mm_storeu_si128((__m128i *) (dst + x + y_stride), x0);
            }
            height--;
        }
    }

    {
        SAO_EDGE_FILTER_INIT();
        if (!(width & 15)) {
            for (y = init_y; y < height; y++) {
                for (x = init_x; x < width; x += 16) {
                    SAO_EDGE_FILTER_LOAD(x);
                    SAO_EDGE_FILTER_COMPUTE();
                    _mm_storeu_si128((__m128i *) (dst + x + y_stride), r0);
                }
                y_stride += stride;
                y_stride_0_1 += stride;
                y_stride_1_1 += stride;
            }
        } else {
            for (y = init_y; y < height; y++) {
                for (x = init_x; x < width; x += 4) {
                    SAO_EDGE_FILTER_LOAD(x);
                    SAO_EDGE_FILTER_COMPUTE();
                    *((uint32_t *) (dst + x + y_stride)) = _mm_cvtsi128_si32(r0);
                }
                y_stride += stride;
                y_stride_0_1 += stride;
                y_stride_1_1 += stride;
            }
        }
    }

    if (sao_eo_class != SAO_EO_VERT) {
        if (borders[0]) {
            int offset_val = sao_offset_val[0];
            int y_stride = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride] = av_clip_uint8(src[y_stride] + offset_val);
                y_stride += stride;
            }
            init_x = 1;
        }
        if (borders[2]) {
            int offset_val = sao_offset_val[0];
            int x_stride = _width - 1;
            for (x = 0; x < height; x++) {
                dst[x_stride] = av_clip_uint8(src[x_stride] + offset_val);
                x_stride += stride;
            }
            width--;
        }
    }

    {
        int save_upper_left  = !diag_edge[0] && sao_eo_class == SAO_EO_135D && !borders[0] && !borders[1];
        int save_upper_right = !diag_edge[1] && sao_eo_class == SAO_EO_45D  && !borders[1] && !borders[2];
        int save_lower_right = !diag_edge[2] && sao_eo_class == SAO_EO_135D && !borders[2] && !borders[3];
        int save_lower_left  = !diag_edge[3] && sao_eo_class == SAO_EO_45D  && !borders[0] && !borders[3];

        // Restore pixels that can't be modified
        if(vert_edge[0] && sao_eo_class != SAO_EO_VERT) {
            for(y = init_y+save_upper_left; y< height-save_lower_left; y++)
                dst[y*stride] = src[y*stride];
        }
        if(vert_edge[1] && sao_eo_class != SAO_EO_VERT) {
            for(y = init_y+save_upper_right; y< height-save_lower_right; y++)
                dst[y*stride+width-1] = src[y*stride+width-1];
        }

        if(horiz_edge[0] && sao_eo_class != SAO_EO_HORIZ) {
            for(x = init_x+save_upper_left; x < width-save_upper_right; x++)
                dst[x] = src[x];
        }
        if(horiz_edge[1] && sao_eo_class != SAO_EO_HORIZ) {
            for(x = init_x+save_lower_left; x < width-save_lower_right; x++)
                dst[(height-1)*stride+x] = src[(height-1)*stride+x];
        }
        if(diag_edge[0] && sao_eo_class == SAO_EO_135D)
            dst[0] = src[0];
        if(diag_edge[1] && sao_eo_class == SAO_EO_45D)
            dst[width-1] = src[width-1];
        if(diag_edge[2] && sao_eo_class == SAO_EO_135D)
            dst[stride*(height-1)+width-1] = src[stride*(height-1)+width-1];
        if(diag_edge[3] && sao_eo_class == SAO_EO_45D)
            dst[stride*(height-1)] = src[stride*(height-1)];
    }
}

