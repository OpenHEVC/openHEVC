/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 Guillaume Martres
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "get_bits.h"
#include "bit_depth_template.c"
#include "hevcdata.h"
#include "hevcdsp.h"

#define OPTIMIZATION_ENABLE

static void FUNC(put_pcm)(uint8_t *_dst, ptrdiff_t _stride, int size,
                          GetBitContext *gb, int pcm_bit_depth)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++)
            dst[x] = get_bits(gb, pcm_bit_depth) << (BIT_DEPTH - pcm_bit_depth);
        dst += stride;
    }
}

static void FUNC(dequant)(int16_t *coeffs, int log2_size, int qp)
{
    int x, y;
    int size = 1 << log2_size;

    const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

    //TODO: scaling_list_enabled_flag support

    int m = 16;
    int shift  = BIT_DEPTH + log2_size - 5;
    int scale  = level_scale[qp % 6] << (qp / 6);
#ifdef OPTIMIZATION_ENABLE
    int offset = 1 << (shift - 1);
    int scale2 = scale << 4;
    int y_size = 0;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++)
            coeffs[y_size+x] = av_clip_int16_c(((coeffs[y_size+x] * scale2) + offset) >> shift);
        y_size += size;
    }
#else
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++)
            coeffs[size*y+x] = av_clip_int16_c(((coeffs[size*y+x] * m * scale) +
                                                (1 << (shift - 1))) >> shift);
#endif
}

static void FUNC(transquant_bypass)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride, int log2_size)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 1 << log2_size;

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            dst[x] += *coeffs;
            coeffs++;
        }
        dst += stride;
    }
}

static void FUNC(transform_skip)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int size = 4;
    int shift = 13 - BIT_DEPTH;
    int offset = 1 << (shift - 1);
    int x, y;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++)
#if BIT_DEPTH <= 13
            dst[x] = av_clip_pixel(dst[x] + ((coeffs[y * size + x] + offset) >> shift));
#else
            dst[x] = av_clip_pixel(dst[x] + (coeffs[y * size + x] << (-shift)));
#endif
        dst += stride;
    }
}

#define SET(dst, x) (dst) = (x)
#define SCALE(dst, x) (dst) = av_clip_int16_c(((x) + add) >> shift)
#define ADD_AND_SCALE(dst, x) (dst) = av_clip_pixel((dst) + av_clip_int16_c(((x) + add) >> shift))

static void FUNC(transform_4x4_luma_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
#define TR_4x4_LUMA(dst, src, step, assign)                                     \
    do {                                                                        \
        int c0 = src[0*step] + src[2*step];                                     \
        int c1 = src[2*step] + src[3*step];                                     \
        int c2 = src[0*step] - src[3*step];                                     \
        int c3 = 74 * src[1*step];                                              \
                                                                                \
        assign(dst[2*step], 74 * (src[0*step] - src[2*step] + src[3*step]));    \
        assign(dst[0*step], 29 * c0 + 55 * c1 + c3);                            \
        assign(dst[1*step], 55 * c2 - 29 * c1 + c3);                            \
        assign(dst[3*step], 55 * c0 + 29 * c2 - c3);                            \
    } while (0)

    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int shift = 7;
    int add = 1 << (shift - 1);
    int16_t *src = coeffs;

    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(src, src, 4, SCALE);
        src++;
    }

    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(dst, coeffs, 1, ADD_AND_SCALE);
        coeffs += 4;
        dst += stride;
    }

#undef TR_4x4_LUMA
}

#define TR_4(dst, src, dstep, sstep, assign)                                    \
    do {                                                                        \
        const int e0 = transform[8*0][0] * src[0*sstep] +                       \
                       transform[8*2][0] * src[2*sstep];                        \
        const int e1 = transform[8*0][1] * src[0*sstep] +                       \
                       transform[8*2][1] * src[2*sstep];                        \
        const int o0 = transform[8*1][0] * src[1*sstep] +                       \
                       transform[8*3][0] * src[3*sstep];                        \
        const int o1 = transform[8*1][1] * src[1*sstep] +                       \
                       transform[8*3][1] * src[3*sstep];                        \
                                                                                \
        assign(dst[0*dstep], e0 + o0);                                          \
        assign(dst[1*dstep], e1 + o1);                                          \
        assign(dst[2*dstep], e1 - o1);                                          \
        assign(dst[3*dstep], e0 - o0);                                          \
    } while (0)
#define TR_4_1(dst, src) TR_4(dst, src, 4, 4, SCALE)
#define TR_4_2(dst, src) TR_4(dst, src, 1, 1, ADD_AND_SCALE)

static void FUNC(transform_4x4_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int shift = 7;
    int add = 1 << (shift - 1);
    int16_t *src = coeffs;

    for (i = 0; i < 4; i++) {
        TR_4_1(src, src);
        src++;
    }

    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4_2(dst, coeffs);
        coeffs += 4;
        dst += stride;
    }
}

#define TR_8(dst, src, dstep, sstep, assign)                \
    do {                                                    \
        int i, j;                                           \
        int e_8[4];                                         \
        int o_8[4] = { 0 };                                 \
        for (i = 0; i < 4; i++)                             \
            for (j = 1; j < 8; j += 2)                      \
                o_8[i] += transform[4*j][i] * src[j*sstep]; \
        TR_4(e_8, src, 1, 2*sstep, SET);                    \
                                                            \
        for (i = 0; i < 4; i++) {                           \
            assign(dst[i*dstep], e_8[i] + o_8[i]);          \
            assign(dst[(7-i)*dstep], e_8[i] - o_8[i]);      \
        }                                                   \
    } while (0)

#define TR_16(dst, src, dstep, sstep, assign)                   \
    do {                                                        \
        int i, j;                                               \
        int e_16[8];                                            \
        int o_16[8] = { 0 };                                    \
        for (i = 0; i < 8; i++)                                 \
            for (j = 1; j < 16; j += 2)                         \
                o_16[i] += transform[2*j][i] * src[j*sstep];    \
        TR_8(e_16, src, 1, 2*sstep, SET);                       \
                                                                \
        for (i = 0; i < 8; i++) {                               \
            assign(dst[i*dstep], e_16[i] + o_16[i]);            \
            assign(dst[(15-i)*dstep], e_16[i] - o_16[i]);       \
        }                                                       \
    } while (0)

#define TR_32(dst, src, dstep, sstep, assign)               \
    do {                                                    \
        int i, j;                                           \
        int e_32[16];                                       \
        int o_32[16] = { 0 };                               \
        for (i = 0; i < 16; i++)                            \
            for (j = 1; j < 32; j += 2)                     \
                o_32[i] += transform[j][i] * src[j*sstep];  \
        TR_16(e_32, src, 1, 2*sstep, SET);                  \
                                                            \
        for (i = 0; i < 16; i++) {                          \
            assign(dst[i*dstep], e_32[i] + o_32[i]);        \
            assign(dst[(31-i)*dstep], e_32[i] - o_32[i]);   \
        }                                                   \
    } while (0)

#define TR_8_1(dst, src) TR_8(dst, src, 8, 8, SCALE)
#define TR_16_1(dst, src) TR_16(dst, src, 16, 16, SCALE)
#define TR_32_1(dst, src) TR_32(dst, src, 32, 32, SCALE)

#define TR_8_2(dst, src) TR_8(dst, src, 1, 1, ADD_AND_SCALE)
#define TR_16_2(dst, src) TR_16(dst, src, 1, 1, ADD_AND_SCALE)
#define TR_32_2(dst, src) TR_32(dst, src, 1, 1, ADD_AND_SCALE)

static void FUNC(transform_8x8_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int shift = 7;
    int add = 1 << (shift - 1);
    int16_t *src = coeffs;

    for (i = 0; i < 8; i++) {
        TR_8_1(src, src);
        src++;
    }

    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 8; i++) {
        TR_8_2(dst, coeffs);
        coeffs += 8;
        dst += stride;
    }
}

static void FUNC(transform_16x16_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int shift = 7;
    int add = 1 << (shift - 1);
    int16_t *src = coeffs;

    for (i = 0; i < 16; i++) {
        TR_16_1(src, src);
        src++;
    }

    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 16; i++) {
        TR_16_2(dst, coeffs);
        coeffs += 16;
        dst += stride;
    }
}

static void FUNC(transform_32x32_add)(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride)
{
    int i;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t stride = _stride / sizeof(pixel);
    int shift = 7;
    int add = 1 << (shift - 1);
    int16_t *src = coeffs;

    for (i = 0; i < 32; i++) {
        TR_32_1(src, src);
        src++;
    }

    shift = 20 - BIT_DEPTH;
    add = 1 << (shift - 1);
    for (i = 0; i < 32; i++) {
        TR_32_2(dst, coeffs);
        coeffs += 32;
        dst += stride;
    }
}

static void FUNC(sao_band_filter)(uint8_t * _dst, uint8_t *_src, ptrdiff_t _stride, int *sao_offset_val,
                                  int sao_left_class, int width, int height)
{
    pixel *dst = (pixel*)_dst;
    pixel *src = (pixel*)_src;
    ptrdiff_t stride = _stride/sizeof(pixel);
    int band_table[32] = { 0 };
    int k, y, x;
    int shift = BIT_DEPTH - 5;

    for (k = 0; k < 4; k++)
        band_table[(k + sao_left_class) & 31] = k + 1;
    for (y = 0; y < height; y++) {
#ifdef OPTIMIZATION_ENABLE
        int x2;
        for (x = 0; x < width; x+= 4) {
            x2      = x;
            dst[x2] = av_clip_pixel(src[x2] + sao_offset_val[band_table[src[x2] >> shift]]);
            x2++;
            dst[x2] = av_clip_pixel(src[x2] + sao_offset_val[band_table[src[x2] >> shift]]);
            x2++;
            dst[x2] = av_clip_pixel(src[x2] + sao_offset_val[band_table[src[x2] >> shift]]);
            x2++;
            dst[x2] = av_clip_pixel(src[x2] + sao_offset_val[band_table[src[x2] >> shift]]);
        }
#else
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(src[x] + sao_offset_val[band_table[src[x] >> shift]]);
#endif
        dst += stride;
        src += stride;
    }
}

static void FUNC(sao_edge_filter)(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, int *sao_offset_val,
                                  int sao_eo_class, int at_top_border, int at_bottom_border,
                                  int at_left_border, int at_right_border,
                                  int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    pixel *src = (pixel*)_src;
    ptrdiff_t stride = _stride/sizeof(pixel);

    const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };

    int init_x = 0, init_y = 0;
    int border_edge_idx = 0;

#ifndef OPTIMIZATION_ENABLE
#define DST(x, y) dst[(x) + stride * (y)]
#define SRC(x, y) src[(x) + stride * (y)]

#define FILTER(x, y, edge_idx)                                      \
    DST(x, y) = av_clip_pixel(SRC(x, y) + sao_offset_val[edge_idx])

#define DIFF(x, y, k) CMP(SRC(x, y), SRC((x) + pos[sao_eo_class][(k)][0],       \
                                         (y) + pos[sao_eo_class][(k)][1]))
#endif
#define CMP(a, b) ((a) > (b) ? 1 : ((a) == (b) ? 0 : -1))

    if (sao_eo_class != SAO_EO_VERT) {
        if (at_left_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            int y_stride   = 0;
            for (y = 0; y < height; y+=4) {
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride += stride;
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride += stride;
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride += stride;
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride += stride;
            }
#else
            for (y = 0; y < height; y++) {
                FILTER(0, y, border_edge_idx);
            }
#endif
            init_x = 1;
        }
        if (at_right_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            int x_stride   = width - 1;
            for (x = 0; x < height; x+=4) {
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride += stride;
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride += stride;
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride += stride;
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride += stride;
            }
#else
            for (x = 0; x < height; x++)
                FILTER(width - 1, x, border_edge_idx);
#endif
            width--;
        }
    }
    if (sao_eo_class != SAO_EO_HORIZ) {
        if (at_top_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            for (x = init_x; x < width; x++) {
                dst[x] = av_clip_pixel(src[x] + offset_val);
            }
#else
            for (x = init_x; x < width; x++)
                FILTER(x, 0, border_edge_idx);
#endif
            init_y = 1;
        }
        if (at_bottom_border) {
#ifdef OPTIMIZATION_ENABLE
            int offset_val = sao_offset_val[0];
            int y_stride   = stride * (height - 1);
            for (x = init_x; x < width; x++) {
                dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + offset_val);
            }
#else
            for (x = init_x; x < width; x++)
                FILTER(x, height - 1, border_edge_idx);
#endif
            height--;
        }
    }
#ifdef OPTIMIZATION_ENABLE
    int y_stride     = init_y * stride;
    int pos_0_0      = pos[sao_eo_class][0][0];
    int pos_0_1      = pos[sao_eo_class][0][1];
    int pos_1_0      = pos[sao_eo_class][1][0];
    int pos_1_1      = pos[sao_eo_class][1][1];
    int y_stride_0_1 = (init_y + pos_0_1) * stride;
    int y_stride_1_1 = (init_y + pos_1_1) * stride;
    for (y = init_y; y < height; y++) {
        for (x = init_x; x < width; x++) {
            int diff0         = CMP(src[x + y_stride], src[x + pos_0_0 + y_stride_0_1]);
            int diff1         = CMP(src[x + y_stride], src[x + pos_1_0 + y_stride_1_1]);
            int offset_val    = edge_idx[2 + diff0 + diff1];
            dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + sao_offset_val[offset_val]);
        }
        y_stride     += stride;
        y_stride_0_1 += stride;
        y_stride_1_1 += stride;
    }
#else
    for (y = init_y; y < height; y++) {
        for (x = init_x; x < width; x++) {
            FILTER(x, y, edge_idx[2 + DIFF(x, y, 0) + DIFF(x, y, 1)]);
        }
    }
#endif
#undef DST
#undef SRC
#undef FILTER
#undef DIFF
#undef CMP
}

#undef SET
#undef SCALE
#undef ADD_AND_SCALE
#undef TR_4
#undef TR_4_1
#undef TR_4_2
#undef TR_8
#undef TR_8_1
#undef TR_8_2
#undef TR_16
#undef TR_16_1
#undef TR_16_2
#undef TR_32
#undef TR_32_1
#undef TR_32_2

static void FUNC(put_hevc_qpel_pixels)(int16_t *dst, ptrdiff_t dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int width, int height)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);

    for (y = 0; y < height; y++) {
#ifdef OPTIMIZATION_ENABLE
        int x2;
        for (x = 0; x < width; x+=4) {
            x2      = x;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
            x2++;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
            x2++;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
            x2++;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
        }
#else
        for (x = 0; x < width; x++)
            dst[x] = src[x] << (14 - BIT_DEPTH);
#endif
        src += srcstride;
        dst += dststride;
    }
}

#define QPEL_FILTER_1(src, stride)                                              \
    (-src[x-3*stride] + 4*src[x-2*stride] - 10*src[x-stride] + 58*src[x] +      \
     17*src[x+stride] - 5*src[x+2*stride] + 1*src[x+3*stride])
#define QPEL_FILTER_2(src, stride)                                              \
    (-src[x-3*stride] + 4*src[x-2*stride] - 11*src[x-stride] + 40*src[x] +      \
     40*src[x+stride] - 11*src[x+2*stride] + 4*src[x+3*stride] - src[x+4*stride])
#define QPEL_FILTER_3(src, stride)                                              \
    (src[x-2*stride] - 5*src[x-stride] + 17*src[x] + 58*src[x+stride]           \
     - 10*src[x+2*stride] + 4*src[x+3*stride] - src[x+4*stride])

#define PUT_HEVC_QPEL_H(H)                                                      \
static void FUNC(put_hevc_qpel_h ## H)(int16_t *dst, ptrdiff_t dststride,       \
                                          uint8_t *_src, ptrdiff_t _srcstride,  \
                                          int width, int height)                \
{                                                                               \
    int x, y;                                                                   \
    pixel *src = (pixel*)_src;                                                  \
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);                             \
                                                                                \
    for (y = 0; y < height; y++) {                                              \
        for (x = 0; x < width; x++)                                             \
            dst[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);             \
        src += srcstride;                                                       \
        dst += dststride;                                                       \
    }                                                                           \
}
#define PUT_HEVC_QPEL_V(V)                                                      \
static void FUNC(put_hevc_qpel_v ## V)(int16_t *dst, ptrdiff_t dststride,       \
                                          uint8_t *_src, ptrdiff_t _srcstride,  \
                                          int width, int height)                \
{                                                                               \
    int x, y;                                                                   \
    pixel *src = (pixel*)_src;                                                  \
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);                             \
                                                                                \
    for (y = 0; y < height; y++)  {                                             \
        for (x = 0; x < width; x++)                                             \
            dst[x] = QPEL_FILTER_ ## V (src, srcstride) >> (BIT_DEPTH - 8);     \
        src += srcstride;                                                       \
        dst += dststride;                                                       \
    }                                                                           \
}
#define PUT_HEVC_QPEL_HV(H, V)                                                            \
static void FUNC(put_hevc_qpel_h ## H ## v ## V )(int16_t *dst, ptrdiff_t dststride,      \
                                                  uint8_t *_src, ptrdiff_t _srcstride,    \
                                                  int width, int height)                  \
{                                                                                         \
    int x, y;                                                                             \
    pixel *src = (pixel*)_src;                                                            \
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);                                       \
                                                                                          \
    int tmpstride = MAX_PB_SIZE;                                                          \
    int16_t tmp_array[(MAX_PB_SIZE+7)*MAX_PB_SIZE];                                       \
    int16_t *tmp = tmp_array;                                                             \
                                                                                          \
    src -= qpel_extra_before[V] * srcstride;                                              \
                                                                                          \
    for (y = 0; y < height + qpel_extra[V]; y++) {                                        \
        for (x = 0; x < width; x++)                                                       \
            tmp[x] = QPEL_FILTER_ ## H (src, 1) >> (BIT_DEPTH - 8);                       \
        src += srcstride;                                                                 \
        tmp += tmpstride;                                                                 \
    }                                                                                     \
                                                                                          \
    tmp = tmp_array + qpel_extra_before[V] * tmpstride;                                   \
                                                                                          \
    for (y = 0; y < height; y++) {                                                        \
        for (x = 0; x < width; x++)                                                       \
            dst[x] = QPEL_FILTER_ ## V (tmp, tmpstride) >> 6;                             \
        tmp += tmpstride;                                                                 \
        dst += dststride;                                                                 \
    }                                                                                     \
}

PUT_HEVC_QPEL_H(1)
PUT_HEVC_QPEL_H(2)
PUT_HEVC_QPEL_H(3)
PUT_HEVC_QPEL_V(1)
PUT_HEVC_QPEL_V(2)
PUT_HEVC_QPEL_V(3)
PUT_HEVC_QPEL_HV(1, 1)
PUT_HEVC_QPEL_HV(1, 2)
PUT_HEVC_QPEL_HV(1, 3)
PUT_HEVC_QPEL_HV(2, 1)
PUT_HEVC_QPEL_HV(2, 2)
PUT_HEVC_QPEL_HV(2, 3)
PUT_HEVC_QPEL_HV(3, 1)
PUT_HEVC_QPEL_HV(3, 2)
PUT_HEVC_QPEL_HV(3, 3)

static void FUNC(put_hevc_epel_pixels)(int16_t *dst, ptrdiff_t dststride,
                                       uint8_t *_src, ptrdiff_t _srcstride,
                                       int width, int height, int mx, int my)
{
    int x, x2, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);

    for (y = 0; y < height; y++) {
#ifdef OPTIMIZATION_ENABLE
        for (x = 0; x < width; x+=4) {
            x2      = x;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
            x2++;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
            x2++;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
            x2++;
            dst[x2] = src[x2] << (14 - BIT_DEPTH);
        }
#else
        for (x = 0; x < width; x++)
            dst[x] = src[x] << (14 - BIT_DEPTH);
#endif
        src += srcstride;
        dst += dststride;
    }
}

#define EPEL_FILTER(src, stride, F) \
    (F[0]*src[x-stride] + F[1]*src[x] + F[2]*src[x+stride] + F[3]*src[x+2*stride])

static void FUNC(put_hevc_epel_h)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my)
{
    int x, x2, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);
    const int8_t *filter = epel_filters[mx-1];
#ifdef OPTIMIZATION_ENABLE
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
    for (y = 0; y < height; y++) {
        int src_minus1 = src[-1];
        int src_x      = src[0];
        int src_plus1  = src[1];
        int src_plus2;
        int src_plus3;
        int src_plus4;
        int src_plus5;
        for (x = 0; x < width; x+=4) {
            src_plus2  = src[x+2];
            src_plus3  = src[x+3];
            src_plus4  = src[x+4];
            src_plus5  = src[x+5];
            dst[x  ]   = (filter_0*src_minus1 + filter_1*src_x     + filter_2*src_plus1 + filter_3*src_plus2)  >> (BIT_DEPTH - 8);
            dst[x+1]   = (filter_0*src_x      + filter_1*src_plus1 + filter_2*src_plus2 + filter_3*src_plus3)  >> (BIT_DEPTH - 8);
            dst[x+2]   = (filter_0*src_plus1  + filter_1*src_plus2 + filter_2*src_plus3 + filter_3*src_plus4)  >> (BIT_DEPTH - 8);
            dst[x+3]   = (filter_0*src_plus2  + filter_1*src_plus3 + filter_2*src_plus4 + filter_3*src_plus5)  >> (BIT_DEPTH - 8);
            src_minus1 = src_plus3;
            src_x      = src_plus4;
            src_plus1  = src_plus5;
        }
#else
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = EPEL_FILTER(src, 1, filter) >> (BIT_DEPTH - 8);
#endif
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_v)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);

    const int8_t *filter = epel_filters[my-1];
#ifdef OPTIMIZATION_ENABLE
    int8_t filter_0 = filter[0];
    int8_t filter_1 = filter[1];
    int8_t filter_2 = filter[2];
    int8_t filter_3 = filter[3];
    int srcstride_2 = srcstride << 1;
    int x2;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=4) {
            x2      = x;
            dst[x2] = (filter_0*src[x2-srcstride] + filter_1*src[x2] + filter_2*src[x2+srcstride] + filter_3*src[x2+srcstride_2]) >> (BIT_DEPTH - 8);
            x2++;
            dst[x2] = (filter_0*src[x2-srcstride] + filter_1*src[x2] + filter_2*src[x2+srcstride] + filter_3*src[x2+srcstride_2]) >> (BIT_DEPTH - 8);
            x2++;
            dst[x2] = (filter_0*src[x2-srcstride] + filter_1*src[x2] + filter_2*src[x2+srcstride] + filter_3*src[x2+srcstride_2]) >> (BIT_DEPTH - 8);
            x2++;
            dst[x2] = (filter_0*src[x2-srcstride] + filter_1*src[x2] + filter_2*src[x2+srcstride] + filter_3*src[x2+srcstride_2]) >> (BIT_DEPTH - 8);
        }
#else
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = EPEL_FILTER(src, srcstride, filter) >> (BIT_DEPTH - 8);
#endif
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_hv)(int16_t *dst, ptrdiff_t dststride,
                                   uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int mx, int my)
{
    int x, y;
    pixel *src = (pixel*)_src;
    ptrdiff_t srcstride = _srcstride/sizeof(pixel);

    const int8_t *filter_h = epel_filters[mx-1];
    const int8_t *filter_v = epel_filters[my-1];
#ifdef OPTIMIZATION_ENABLE
    int8_t filter_h_0 = filter_h[0];
    int8_t filter_h_1 = filter_h[1];
    int8_t filter_h_2 = filter_h[2];
    int8_t filter_h_3 = filter_h[3];

    int8_t filter_v_0 = filter_v[0];
    int8_t filter_v_1 = filter_v[1];
    int8_t filter_v_2 = filter_v[2];
    int8_t filter_v_3 = filter_v[3];
#endif

    int tmpstride = MAX_PB_SIZE;
    int16_t tmp_array[(MAX_PB_SIZE+3)*MAX_PB_SIZE];
    int16_t *tmp = tmp_array;

    src -= epel_extra_before * srcstride;

#ifdef OPTIMIZATION_ENABLE
    for (y = 0; y < height + epel_extra; y++) {
        int src_minus1 = src[-1];
        int src_x      = src[0];
        int src_plus1  = src[1];
        int src_plus2;
        int src_plus3;
        int src_plus4;
        int src_plus5;
        for (x = 0; x < width; x+=4) {
            src_plus2  = src[x+2];
            src_plus3  = src[x+3];
            src_plus4  = src[x+4];
            src_plus5  = src[x+5];
            tmp[x  ]   = (filter_h_0*src_minus1 + filter_h_1*src_x     + filter_h_2*src_plus1 + filter_h_3*src_plus2)  >> (BIT_DEPTH - 8);
            tmp[x+1]   = (filter_h_0*src_x      + filter_h_1*src_plus1 + filter_h_2*src_plus2 + filter_h_3*src_plus3)  >> (BIT_DEPTH - 8);
            tmp[x+2]   = (filter_h_0*src_plus1  + filter_h_1*src_plus2 + filter_h_2*src_plus3 + filter_h_3*src_plus4)  >> (BIT_DEPTH - 8);
            tmp[x+3]   = (filter_h_0*src_plus2  + filter_h_1*src_plus3 + filter_h_2*src_plus4 + filter_h_3*src_plus5)  >> (BIT_DEPTH - 8);
            src_minus1 = src_plus3;
            src_x      = src_plus4;
            src_plus1  = src_plus5;
        }
#else
    for (y = 0; y < height + epel_extra; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = EPEL_FILTER(src, 1, filter_h) >> (BIT_DEPTH - 8);
#endif
        src += srcstride;
        tmp += tmpstride;
    }

    tmp = tmp_array + epel_extra_before * tmpstride;
#ifdef OPTIMIZATION_ENABLE
    int tmpstride_2 = tmpstride << 1;
    int x2;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x+=4) {
            x2      = x;
            dst[x2] = (filter_v_0*tmp[x2-tmpstride] + filter_v_1*tmp[x2] + filter_v_2*tmp[x2+tmpstride] + filter_v_3*tmp[x2+tmpstride_2]) >> 6;
            x2++;
            dst[x2] = (filter_v_0*tmp[x2-tmpstride] + filter_v_1*tmp[x2] + filter_v_2*tmp[x2+tmpstride] + filter_v_3*tmp[x2+tmpstride_2]) >> 6;
            x2++;
            dst[x2] = (filter_v_0*tmp[x2-tmpstride] + filter_v_1*tmp[x2] + filter_v_2*tmp[x2+tmpstride] + filter_v_3*tmp[x2+tmpstride_2]) >> 6;
            x2++;
            dst[x2] = (filter_v_0*tmp[x2-tmpstride] + filter_v_1*tmp[x2] + filter_v_2*tmp[x2+tmpstride] + filter_v_3*tmp[x2+tmpstride_2]) >> 6;
        }
#else
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = EPEL_FILTER(tmp, tmpstride, filter_v) >> 6;
#endif
        tmp += tmpstride;
        dst += dststride;
    }
}

static void FUNC(put_unweighted_pred)(uint8_t *_dst, ptrdiff_t _dststride,
                                      int16_t *src, ptrdiff_t srcstride,
                                      int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);

    int shift = 14 - BIT_DEPTH;
    #if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
    #else
    int offset = 0;
    #endif
    for (y = 0; y < height; y++) {
#ifdef OPTIMIZATION_ENABLE
        int x2;
        for (x = 0; x < width; x+=4) {
            x2      = x;
            dst[x2] = av_clip_pixel((src[x2] + offset) >> shift);
            x2++;
            dst[x2] = av_clip_pixel((src[x2] + offset) >> shift);
            x2++;
            dst[x2] = av_clip_pixel((src[x2] + offset) >> shift);
            x2++;
            dst[x2] = av_clip_pixel((src[x2] + offset) >> shift);
        }
#else
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel((src[x] + offset) >> shift);
//            printf("%d\n", dst[x]);
        }
#endif
        dst += dststride;
        src += srcstride;
    }
}

static void FUNC(put_weighted_pred_avg)(uint8_t *_dst, ptrdiff_t _dststride,
                                        int16_t *src1, int16_t *src2, ptrdiff_t srcstride,
                                        int width, int height)
{
    int x, y;
    pixel *dst = (pixel*)_dst;
    ptrdiff_t dststride = _dststride/sizeof(pixel);

    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel((src1[x] + src2[x] + offset) >> shift);
//            printf("%d\n", dst[x]);
        }
        dst  += dststride;
        src1 += srcstride;
        src2 += srcstride;
    }
}


// line zero
#define P3 pix[-4*xstride]
#define P2 pix[-3*xstride]
#define P1 pix[-2*xstride]
#define P0 pix[-xstride]
#define Q0 pix[0]
#define Q1 pix[xstride]
#define Q2 pix[2*xstride]
#define Q3 pix[3*xstride]

// line three. used only for deblocking decision
#define TP3 pix[-4*xstride+3*ystride]
#define TP2 pix[-3*xstride+3*ystride]
#define TP1 pix[-2*xstride+3*ystride]
#define TP0 pix[-xstride+3*ystride]
#define TQ0 pix[3*ystride]
#define TQ1 pix[xstride+3*ystride]
#define TQ2 pix[2*xstride+3*ystride]
#define TQ3 pix[3*xstride+3*ystride]

static void FUNC(hevc_loop_filter_luma)(uint8_t *_pix, ptrdiff_t _xstride, ptrdiff_t _ystride,
                                        int no_p, int no_q, int _beta, int _tc)
{
    int d;
    pixel *pix = (pixel*)_pix;
    ptrdiff_t xstride = _xstride/sizeof(pixel);
    ptrdiff_t ystride = _ystride/sizeof(pixel);
    const int dp0 = abs(P2 - 2 * P1 +  P0);
    const int dq0 = abs(Q2 - 2 * Q1 +  Q0);
    const int dp3 = abs(TP2 - 2 * TP1 + TP0);
    const int dq3 = abs(TQ2 - 2 * TQ1 + TQ0);
    const int d0 = dp0 + dq0;
    const int d3 = dp3 + dq3;

    const int beta = _beta << (BIT_DEPTH - 8);
    const int tc = _tc << (BIT_DEPTH - 8);

    if (d0 + d3 < beta) {
        const int beta_3 = beta >> 3;
        const int beta_2 = beta >> 2;
        const int tc25 = ((tc * 5 + 1) >> 1);

        if(abs( P3 -  P0) + abs( Q3 -  Q0) < beta_3 && abs( P0 -  Q0) < tc25 &&
           abs(TP3 - TP0) + abs(TQ3 - TQ0) < beta_3 && abs(TP0 - TQ0) < tc25 &&
                                 (d0 << 1) < beta_2 &&      (d3 << 1) < beta_2) {
            // strong filtering
            const int tc2 = tc << 1;
            for(d = 0; d < 4; d++) {
                const int p3 = P3;
                const int p2 = P2;
                const int p1 = P1;
                const int p0 = P0;
                const int q0 = Q0;
                const int q1 = Q1;
                const int q2 = Q2;
                const int q3 = Q3;
                if(!no_p) {
                    P0 = av_clip_c(( p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4 ) >> 3, p0-tc2, p0+tc2);
                    P1 = av_clip_c(( p2 + p1 + p0 + q0 + 2 ) >> 2, p1-tc2, p1+tc2);
                    P2 = av_clip_c(( 2*p3 + 3*p2 + p1 + p0 + q0 + 4 ) >> 3, p2-tc2, p2+tc2);
                }
                if(!no_q) {
                    Q0 = av_clip_c(( p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4 ) >> 3, q0-tc2, q0+tc2);
                    Q1 = av_clip_c(( p0 + q0 + q1 + q2 + 2 ) >> 2, q1-tc2, q1+tc2);
                    Q2 = av_clip_c(( 2*q3 + 3*q2 + q1 + q0 + p0 + 4 ) >> 3, q2-tc2, q2+tc2);
                }
                pix += ystride;
            }
        } else { // normal filtering
            int nd_p = 1;
            int nd_q = 1;
            const int tc_2 = tc >> 1;
            if (dp0 + dp3 < ((beta+(beta>>1))>>3))
                nd_p = 2;
            if (dq0 + dq3 < ((beta+(beta>>1))>>3))
                nd_q = 2;

            for(d = 0; d < 4; d++) {
                const int p2 = P2;
                const int p1 = P1;
                const int p0 = P0;
                const int q0 = Q0;
                const int q1 = Q1;
                const int q2 = Q2;
                int delta0 = (9*(q0 - p0) - 3*(q1 - p1) + 8) >> 4;
                if (abs(delta0) < 10 * tc) {
                    delta0 = av_clip_c(delta0, -tc, tc);
                    if(!no_p)
                        P0 = av_clip_pixel(p0 + delta0);
                    if(!no_q)
                        Q0 = av_clip_pixel(q0 - delta0);
                    if(!no_p && nd_p > 1) {
                        const int deltap1 = av_clip_c((((p2 + p0 + 1) >> 1) - p1 + delta0) >> 1, -tc_2, tc_2);
                        P1 = av_clip_pixel(p1 + deltap1);
                    }
                    if(!no_q && nd_q > 1) {
                        const int deltaq1 = av_clip_c((((q2 + q0 + 1) >> 1) - q1 - delta0) >> 1, -tc_2, tc_2);
                        Q1 = av_clip_pixel(q1 + deltaq1);
                    }
                }
                pix += ystride;
            }
        }
    }
}

static void FUNC(hevc_loop_filter_chroma)(uint8_t *_pix, ptrdiff_t _xstride, ptrdiff_t _ystride, int no_p, int no_q, int _tc)
{
    int d;
    pixel *pix = (pixel*)_pix;
    ptrdiff_t xstride = _xstride/sizeof(pixel);
    ptrdiff_t ystride = _ystride/sizeof(pixel);
    const int tc = _tc << (BIT_DEPTH - 8);

    for(d = 0; d < 4; d++) {
        int delta0;
        const int p1 = P1;
        const int p0 = P0;
        const int q0 = Q0;
        const int q1 = Q1;
        delta0 = av_clip_c((((q0 - p0) << 2) + p1 - q1 + 4) >> 3, -tc, tc);
        if(!no_p)
            P0 = av_clip_pixel(p0 + delta0);
        if(!no_q)
            Q0 = av_clip_pixel(q0 - delta0);
        pix += ystride;
    }
}

#undef P3
#undef P2
#undef P1
#undef P0
#undef Q0
#undef Q1
#undef Q2
#undef Q3

#undef TP3
#undef TP2
#undef TP1
#undef TP0
#undef TQ0
#undef TQ1
#undef TQ2
#undef TQ3
