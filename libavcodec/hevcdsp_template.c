/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 - 2014 Seppo Tomperi
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

#include "get_bits.h"
#include "hevc.h"

#include "bit_depth_template.c"
#include "hevcdsp.h"


static void FUNC(put_pcm)(uint8_t *_dst, ptrdiff_t stride, int width, int height,
                          GetBitContext *gb, int pcm_bit_depth)
{
    int x, y;
    pixel *dst = (pixel *)_dst;

    stride /= sizeof(pixel);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = get_bits(gb, pcm_bit_depth) << (BIT_DEPTH - pcm_bit_depth);
        dst += stride;
    }
}

static void FUNC(transquant_bypass4x4)(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride)
{
    int x, y;
    pixel *dst = (pixel *)_dst;

    stride /= sizeof(pixel);

    for (y = 0; y < 4; y++) {
        for (x = 0; x < 4; x++) {
            dst[x] = av_clip_pixel(dst[x] + *coeffs);
            coeffs++;
        }
        dst += stride;
    }
}

static void FUNC(transquant_bypass8x8)(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride)
{
    int x, y;
    pixel *dst = (pixel *)_dst;

    stride /= sizeof(pixel);

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            dst[x] = av_clip_pixel(dst[x] + *coeffs);
            coeffs++;
        }
        dst += stride;
    }
}

static void FUNC(transquant_bypass16x16)(uint8_t *_dst, int16_t *coeffs,
                                         ptrdiff_t stride)
{
    int x, y;
    pixel *dst = (pixel *)_dst;

    stride /= sizeof(pixel);

    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            dst[x] = av_clip_pixel(dst[x] + *coeffs);
            coeffs++;
        }
        dst += stride;
    }
}

static void FUNC(transquant_bypass32x32)(uint8_t *_dst, int16_t *coeffs,
                                         ptrdiff_t stride)
{
    int x, y;
    pixel *dst = (pixel *)_dst;

    stride /= sizeof(pixel);

    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            dst[x] = av_clip_pixel(dst[x] + *coeffs);
            coeffs++;
        }
        dst += stride;
    }
}


static void FUNC(transform_rdpcm)(uint8_t *_dst, int16_t *_coeffs,
                                 ptrdiff_t stride, int16_t size, int mode)
{
    pixel *dst = (pixel *)_dst;
    int16_t *coeffs = (int16_t *) _coeffs;
    int shift  = 13 - BIT_DEPTH;
#if BIT_DEPTH <= 13
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif
    int x, y;

    stride /= sizeof(pixel);



    for (x = 0; x < size * size; x ++)
        _coeffs[x] = (_coeffs[x] + offset) >> shift;

    if (mode) {
        coeffs += size;
        for (y = 0; y < size - 1; y++) {
            for (x = 0; x < size; x++)
                coeffs[x] += coeffs[x - size];
            coeffs += size;
        }
    } else {
        for (y = 0; y < size; y++) {
            for (x = 1; x < size; x++)
                coeffs[x] += coeffs[x - 1];
            coeffs += size;
        }
    }


    for (y = 0; y < size; y ++) {
        for (x = 0; x < size; x++)
            dst[x] = av_clip_pixel(dst[x] + _coeffs[size * y + x]);
        dst += stride;
    }
}


static void FUNC(transform_skip)(uint8_t *_dst, int16_t *coeffs,
                                 ptrdiff_t stride, int16_t size)
{
    pixel *dst = (pixel *)_dst;
    int shift  = 13 - BIT_DEPTH;
#if BIT_DEPTH <= 13
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif
    int x, y;

    stride /= sizeof(pixel);

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++)
            dst[x] = av_clip_pixel(dst[x] + ((coeffs[y * size + x] + offset) >> shift));
        dst += stride;
    }
}

#define SET(dst, x)   (dst) = (x)
#define SCALE(dst, x) (dst) = av_clip_int16(((x) + add) >> shift)
#define ADD_AND_SCALE(dst, x)                                           \
    (dst) = av_clip_pixel((dst) + av_clip_int16(((x) + add) >> shift))

#define TR_4x4_LUMA(dst, src, step, assign)                             \
    do {                                                                \
        int c0 = src[0 * step] + src[2 * step];                         \
        int c1 = src[2 * step] + src[3 * step];                         \
        int c2 = src[0 * step] - src[3 * step];                         \
        int c3 = 74 * src[1 * step];                                    \
                                                                        \
        assign(dst[2 * step], 74 * (src[0 * step] -                     \
                                    src[2 * step] +                     \
                                    src[3 * step]));                    \
        assign(dst[0 * step], 29 * c0 + 55 * c1 + c3);                  \
        assign(dst[1 * step], 55 * c2 - 29 * c1 + c3);                  \
        assign(dst[3 * step], 55 * c0 + 29 * c2 - c3);                  \
    } while (0)

static void FUNC(transform_4x4_luma_add)(uint8_t *_dst, int16_t *coeffs,
                                         ptrdiff_t stride)
{
    int i;
    pixel *dst   = (pixel *)_dst;
    int shift    = 7;
    int add      = 1 << (shift - 1);
    int16_t *src = coeffs;

    stride /= sizeof(pixel);

    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(src, src, 4, SCALE);
        src++;
    }

    shift = 20 - BIT_DEPTH;
    add   = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(dst, coeffs, 1, ADD_AND_SCALE);
        coeffs += 4;
        dst    += stride;
    }
}

#undef TR_4x4_LUMA

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define TR_4(dst, src, dstep, sstep, assign, end)                              \
    do {                                                                       \
        const int e0 = 64 * src[0 * sstep] + 64 * src[2 * sstep];              \
        const int e1 = 64 * src[0 * sstep] - 64 * src[2 * sstep];              \
        const int o0 = 83 * src[1 * sstep] + 36 * src[3 * sstep];              \
        const int o1 = 36 * src[1 * sstep] - 83 * src[3 * sstep];              \
                                                                               \
        assign(dst[0 * dstep], e0 + o0);                                       \
        assign(dst[1 * dstep], e1 + o1);                                       \
        assign(dst[2 * dstep], e1 - o1);                                       \
        assign(dst[3 * dstep], e0 - o0);                                       \
    } while (0)

#define TR_8(dst, src, dstep, sstep, assign, end)                              \
    do {                                                                       \
        int i, j;                                                              \
        int e_8[4];                                                            \
        int o_8[4] = { 0 };                                                    \
        for (i = 0; i < 4; i++)                                                \
            for (j = 1; j < end; j += 2)                                       \
                o_8[i] += transform[4 * j][i] * src[j * sstep];                \
        TR_4(e_8, src, 1, 2 * sstep, SET, 4);                                  \
                                                                               \
        for (i = 0; i < 4; i++) {                                              \
            assign(dst[i * dstep], e_8[i] + o_8[i]);                           \
            assign(dst[(7 - i) * dstep], e_8[i] - o_8[i]);                     \
        }                                                                      \
    } while (0)

#define TR_16(dst, src, dstep, sstep, assign, end)                             \
    do {                                                                       \
        int i, j;                                                              \
        int e_16[8];                                                           \
        int o_16[8] = { 0 };                                                   \
        for (i = 0; i < 8; i++)                                                \
            for (j = 1; j < end; j += 2)                                       \
                o_16[i] += transform[2 * j][i] * src[j * sstep];               \
        TR_8(e_16, src, 1, 2 * sstep, SET, 8);                                 \
                                                                               \
        for (i = 0; i < 8; i++) {                                              \
            assign(dst[i * dstep], e_16[i] + o_16[i]);                         \
            assign(dst[(15 - i) * dstep], e_16[i] - o_16[i]);                  \
        }                                                                      \
    } while (0)

#define TR_32(dst, src, dstep, sstep, assign, end)                             \
    do {                                                                       \
        int i, j;                                                              \
        int e_32[16];                                                          \
        int o_32[16] = { 0 };                                                  \
        for (i = 0; i < 16; i++)                                               \
            for (j = 1; j < end; j += 2)                                       \
                o_32[i] += transform[j][i] * src[j * sstep];                   \
        TR_16(e_32, src, 1, 2 * sstep, SET, end/2);                            \
                                                                               \
        for (i = 0; i < 16; i++) {                                             \
            assign(dst[i * dstep], e_32[i] + o_32[i]);                         \
            assign(dst[(31 - i) * dstep], e_32[i] - o_32[i]);                  \
        }                                                                      \
    } while (0)

#define TRANSFORM_ADD_VAR4(H)                                                    \
    int      limit2   = FFMIN(col_limit + 4, H)
#define TRANSFORM_ADD_VAR8(H)                                                    \
        int      limit   = FFMIN(col_limit, H);                                    \
        int      limit2   = FFMIN(col_limit + 4, H)
#define TRANSFORM_ADD_VAR16(H)   TRANSFORM_ADD_VAR8(H)
#define TRANSFORM_ADD_VAR32(H)   TRANSFORM_ADD_VAR8(H)

#define TRANSFORM_ADD(H)                                                       \
static void FUNC(transform_##H ##x ##H ##_add)(                                \
    uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride, int col_limit) {        \
    int i;                                                                     \
    pixel    *dst    = (pixel *)_dst;                                          \
    int      stride  = _stride/sizeof(pixel);                                  \
    int      shift   = 7;                                                      \
    int      add     = 1 << (shift - 1);                                       \
    int16_t *src     = coeffs;                                                 \
    TRANSFORM_ADD_VAR ##H(H);                                                   \
                                                                               \
    for (i = 0; i < H; i++) {                                                  \
        TR_ ## H(src, src, H, H, SCALE, limit2);                               \
        if (limit2 < H && i%4 == 0 && !!i)                                     \
            limit2 -= 4;                                                       \
        src++;                                                                 \
    }                                                                          \
                                                                               \
    shift   = 20 - BIT_DEPTH;                                                  \
    add     = 1 << (shift - 1);                                                \
    for (i = 0; i < H; i++) {                                                  \
        TR_ ## H(dst, coeffs, 1, 1, ADD_AND_SCALE, limit);                     \
        coeffs += H;                                                           \
        dst    += stride;                                                      \
    }                                                                          \
}

#define TRANSFORM_DC_ADD(H)                                                    \
static void FUNC(transform_##H ##x ##H ##_dc_add)(                             \
    uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride) {                       \
    int i, j;                                                                  \
    pixel    *dst    = (pixel *)_dst;                                          \
    int      stride  = _stride/sizeof(pixel);                                  \
    int      shift   = 14 - BIT_DEPTH;                                         \
    int      add     = 1 << (shift - 1);                                       \
    int      coeff   = (((coeffs[0] + 1) >> 1) + add) >> shift;                \
                                                                               \
    for (j = 0; j < H; j++) {                                                  \
        for (i = 0; i < H; i++) {                                              \
            dst[i+j*stride] = av_clip_pixel(dst[i+j*stride] + coeff);          \
        }                                                                      \
    }                                                                          \
}

TRANSFORM_ADD( 4)
TRANSFORM_ADD( 8)
TRANSFORM_ADD(16)
TRANSFORM_ADD(32)

TRANSFORM_DC_ADD( 4)
TRANSFORM_DC_ADD( 8)
TRANSFORM_DC_ADD(16)
TRANSFORM_DC_ADD(32)

#undef TR_4
#undef TR_8
#undef TR_16
#undef TR_32

#undef SET
#undef SCALE
#undef ADD_AND_SCALE

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
static void FUNC(sao_band_filter_0)(uint8_t *_dst, uint8_t *_src,
                                  ptrdiff_t stride, SAOParams *sao,
                                  int *borders, int width, int height,
                                  int c_idx)
{
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int offset_table[32] = { 0 };
    int k, y, x;
    int shift  = BIT_DEPTH - 5;
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t sao_left_class  = sao->band_position[c_idx];

    stride /= sizeof(pixel);

    for (k = 0; k < 4; k++)
        offset_table[(k + sao_left_class) & 31] = sao_offset_val[k + 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(src[x] + offset_table[src[x] >> shift]);
        dst += stride;
        src += stride;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define CMP(a, b) ((a) > (b) ? 1 : ((a) == (b) ? 0 : -1))

static void FUNC(sao_edge_filter)(uint8_t *_dst, uint8_t *_src,
                                  ptrdiff_t stride, SAOParams *sao,
                                  int width, int height,
                                  int c_idx, int init_x, int init_y) {

    static const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };
    static const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t sao_eo_class    = sao->eo_class[c_idx];
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;

    int y_stride = init_y * stride;
    int pos_0_0  = pos[sao_eo_class][0][0];
    int pos_0_1  = pos[sao_eo_class][0][1];
    int pos_1_0  = pos[sao_eo_class][1][0];
    int pos_1_1  = pos[sao_eo_class][1][1];
    int x, y;

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
}

static void FUNC(sao_edge_filter_0)(uint8_t *_dst, uint8_t *_src,
                                    ptrdiff_t stride, SAOParams *sao,
                                    int *borders, int _width, int _height,
                                    int c_idx, uint8_t *vert_edge,
                                    uint8_t *horiz_edge, uint8_t *diag_edge)
{
    int x, y;
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t sao_eo_class    = sao->eo_class[c_idx];
    int init_x = 0, init_y = 0, width = _width, height = _height;

    stride /= sizeof(pixel);

    FUNC(sao_edge_filter)((uint8_t *)dst, (uint8_t *)src, stride, sao, width, height, c_idx, init_x, init_y);

    if (sao_eo_class != SAO_EO_VERT) {
        if (borders[0]) {
            int offset_val = sao_offset_val[0];
            int y_stride   = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride     += stride;
            }
            init_x = 1;
        }
        if (borders[2]) {
            int offset_val = sao_offset_val[0];
            int x_stride   = width - 1;
            for (x = 0; x < height; x++) {
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride     += stride;
            }
            width--;
        }
    }
    if (sao_eo_class != SAO_EO_HORIZ) {
        if (borders[1]) {
            int offset_val = sao_offset_val[0];
            for (x = init_x; x < width; x++)
                dst[x] = av_clip_pixel(src[x] + offset_val);
        }
        if (borders[3]) {
            int offset_val = sao_offset_val[0];
            int y_stride   = stride * (height - 1);
            for (x = init_x; x < width; x++)
                dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + offset_val);
            height--;
        }
    }
}

static void FUNC(sao_edge_filter_1)(uint8_t *_dst, uint8_t *_src,
                                    ptrdiff_t stride, SAOParams *sao,
                                    int *borders, int _width, int _height,
                                    int c_idx, uint8_t *vert_edge,
                                    uint8_t *horiz_edge, uint8_t *diag_edge)
{
    int x, y;
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t sao_eo_class    = sao->eo_class[c_idx];
    int init_x = 0, init_y = 0, width = _width, height = _height;

    stride /= sizeof(pixel);

    FUNC(sao_edge_filter)((uint8_t *)dst, (uint8_t *)src, stride, sao, width, height, c_idx, init_x, init_y);

    if (sao_eo_class != SAO_EO_VERT) {
        if (borders[0]) {
            int offset_val = sao_offset_val[0];
            int y_stride   = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride] = av_clip_pixel(src[y_stride] + offset_val);
                y_stride     += stride;
            }
            init_x = 1;
        }
        if (borders[2]) {
            int offset_val = sao_offset_val[0];
            int x_stride   = width - 1;
            for (x = 0; x < height; x++) {
                dst[x_stride] = av_clip_pixel(src[x_stride] + offset_val);
                x_stride     += stride;
            }
            width--;
        }
    }
    if (sao_eo_class != SAO_EO_HORIZ) {
        if (borders[1]) {
            int offset_val = sao_offset_val[0];
            for (x = init_x; x < width; x++)
                dst[x] = av_clip_pixel(src[x] + offset_val);
            init_y = 1;
        }
        if (borders[3]) {
            int offset_val = sao_offset_val[0];
            int y_stride   = stride * (height - 1);
            for (x = init_x; x < width; x++)
                dst[x + y_stride] = av_clip_pixel(src[x + y_stride] + offset_val);
            height--;
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

#undef CMP

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
static void FUNC(put_hevc_pel_pixels)(int16_t *dst, ptrdiff_t dststride,
                                      uint8_t *_src, ptrdiff_t _srcstride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src          = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = src[x] << (14 - BIT_DEPTH);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_pel_uni_pixels)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, intptr_t mx, intptr_t my, int width)
{
    int y;
    pixel *src          = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    for (y = 0; y < height; y++) {
        memcpy(dst, src, width * sizeof(pixel));
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_pel_bi_pixels)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2, ptrdiff_t src2stride,
                                         int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src          = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    int shift = 14  + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((src[x] << (14 - BIT_DEPTH)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_pel_uni_w_pixels)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                            int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src          = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox     = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((src[x] << (14 - BIT_DEPTH)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_pel_bi_w_pixels)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                           int16_t *src2, ptrdiff_t src2stride,
                                           int height, int denom, int wx0, int wx1,
                                           int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src          = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    int shift = 14  + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel(( (src[x] << (14 - BIT_DEPTH)) * wx1 + src2[x] * wx0 + ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        }
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define QPEL_FILTER(src, stride)                                               \
    (filter[0] * src[x - 3 * stride] +                                         \
     filter[1] * src[x - 2 * stride] +                                         \
     filter[2] * src[x -     stride] +                                         \
     filter[3] * src[x             ] +                                         \
     filter[4] * src[x +     stride] +                                         \
     filter[5] * src[x + 2 * stride] +                                         \
     filter[6] * src[x + 3 * stride] +                                         \
     filter[7] * src[x + 4 * stride])

static void FUNC(put_hevc_qpel_h)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_filters[mx - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_v)(int16_t *dst,  ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_filters[my - 1];
    for (y = 0; y < height; y++)  {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_hv)(int16_t *dst,
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
    filter = ff_hevc_qpel_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_filters[my - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = QPEL_FILTER(tmp, MAX_PB_SIZE) >> 6;
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_uni_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                      uint8_t *_src, ptrdiff_t _srcstride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_filters[mx - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_bi_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_qpel_uni_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                     uint8_t *_src, ptrdiff_t _srcstride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_filters[my - 1];
    int shift = 14 - BIT_DEPTH;

#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}


static void FUNC(put_hevc_qpel_bi_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_qpel_uni_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
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
    filter = ff_hevc_qpel_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) + offset) >> shift);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_bi_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
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
    filter = ff_hevc_qpel_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) + src2[x] + offset) >> shift);
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_qpel_uni_w_h)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_filters[mx - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_filters[mx - 1];

    int shift = 14  + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_qpel_uni_w_v)(uint8_t *_dst,  ptrdiff_t _dststride,
                                        uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox,
                                        intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter    = ff_hevc_qpel_filters[my - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel        *src       = (pixel*)_src;
    ptrdiff_t     srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);

    const int8_t *filter    = ff_hevc_qpel_filters[my - 1];

    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_qpel_uni_w_hv)(uint8_t *_dst,  ptrdiff_t _dststride,
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
    filter = ff_hevc_qpel_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_filters[my - 1];

    ox = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((QPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) * wx + offset) >> shift) + ox);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

static void FUNC(put_hevc_qpel_bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
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
    filter = ff_hevc_qpel_filters[mx - 1];
    for (y = 0; y < height + QPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = QPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp    = tmp_array + QPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_qpel_filters[my - 1];

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((QPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define EPEL_FILTER(src, stride)                                               \
    (filter[0] * src[x - stride] +                                             \
     filter[1] * src[x]          +                                             \
     filter[2] * src[x + stride] +                                             \
     filter[3] * src[x + 2 * stride])

static void FUNC(put_hevc_epel_h)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_v)(int16_t *dst, ptrdiff_t dststride,
                                  uint8_t *_src, ptrdiff_t _srcstride,
                                  int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_hv)(int16_t *dst, ptrdiff_t dststride,
                                   uint8_t *_src, ptrdiff_t _srcstride,
                                   int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;

    src -= EPEL_EXTRA_BEFORE * srcstride;

    for (y = 0; y < height + EPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_epel_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6;
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_uni_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int shift = 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_bi_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel(((EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        }
        dst  += dststride;
        src  += srcstride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_epel_uni_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[my - 1];
    int shift = 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) + offset) >> shift);
        src += srcstride;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_bi_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                     int16_t *src2, ptrdiff_t src2stride,
                                     int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[my - 1];
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) + src2[x] + offset) >> shift);
        dst  += dststride;
        src  += srcstride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_epel_uni_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src -= EPEL_EXTRA_BEFORE * srcstride;

    for (y = 0; y < height + EPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_epel_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) + offset) >> shift);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_bi_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                      int16_t *src2, ptrdiff_t src2stride,
                                      int height, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src -= EPEL_EXTRA_BEFORE * srcstride;

    for (y = 0; y < height + EPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_epel_filters[my - 1];

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) + src2[x] + offset) >> shift);
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_epel_uni_w_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox     = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel((((EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        }
        dst += dststride;
        src += srcstride;
    }
}

static void FUNC(put_hevc_epel_bi_w_h)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_epel_uni_w_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[my - 1];
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    ox     = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            dst[x] = av_clip_pixel((((EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) * wx + offset) >> shift) + ox);
        }
        dst += dststride;
        src += srcstride;
    }
}

static void FUNC(put_hevc_epel_bi_w_v)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2, ptrdiff_t src2stride,
                                       int height, int denom, int wx0, int wx1,
                                       int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride  = _srcstride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[my - 1];
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(src, srcstride) >> (BIT_DEPTH - 8)) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        src  += srcstride;
        dst  += dststride;
        src2 += src2stride;
    }
}

static void FUNC(put_hevc_epel_uni_w_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = denom + 14 - BIT_DEPTH;
#if BIT_DEPTH < 14
    int offset = 1 << (shift - 1);
#else
    int offset = 0;
#endif

    src -= EPEL_EXTRA_BEFORE * srcstride;

    for (y = 0; y < height + EPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_epel_filters[my - 1];

    ox     = ox * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel((((EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) * wx + offset) >> shift) + ox);
        tmp += MAX_PB_SIZE;
        dst += dststride;
    }
}

static void FUNC(put_hevc_epel_bi_w_hv)(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int16_t *src2, ptrdiff_t src2stride,
                                        int height, int denom, int wx0, int wx1,
                                        int ox0, int ox1, intptr_t mx, intptr_t my, int width)
{
    int x, y;
    pixel *src = (pixel *)_src;
    ptrdiff_t srcstride = _srcstride / sizeof(pixel);
    pixel *dst          = (pixel *)_dst;
    ptrdiff_t dststride = _dststride / sizeof(pixel);
    const int8_t *filter = ff_hevc_epel_filters[mx - 1];
    int16_t tmp_array[(MAX_PB_SIZE + EPEL_EXTRA) * MAX_PB_SIZE];
    int16_t *tmp = tmp_array;
    int shift = 14 + 1 - BIT_DEPTH;
    int log2Wd = denom + shift - 1;

    src -= EPEL_EXTRA_BEFORE * srcstride;

    for (y = 0; y < height + EPEL_EXTRA; y++) {
        for (x = 0; x < width; x++)
            tmp[x] = EPEL_FILTER(src, 1) >> (BIT_DEPTH - 8);
        src += srcstride;
        tmp += MAX_PB_SIZE;
    }

    tmp      = tmp_array + EPEL_EXTRA_BEFORE * MAX_PB_SIZE;
    filter = ff_hevc_epel_filters[my - 1];

    ox0     = ox0 * (1 << (BIT_DEPTH - 8));
    ox1     = ox1 * (1 << (BIT_DEPTH - 8));
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(((EPEL_FILTER(tmp, MAX_PB_SIZE) >> 6) * wx1 + src2[x] * wx0 +
                                    ((ox0 + ox1 + 1) << log2Wd)) >> (log2Wd + 1));
        tmp  += MAX_PB_SIZE;
        dst  += dststride;
        src2 += src2stride;
    }
}// line zero
#define P3 pix[-4 * xstride]
#define P2 pix[-3 * xstride]
#define P1 pix[-2 * xstride]
#define P0 pix[-1 * xstride]
#define Q0 pix[0 * xstride]
#define Q1 pix[1 * xstride]
#define Q2 pix[2 * xstride]
#define Q3 pix[3 * xstride]

// line three. used only for deblocking decision
#define TP3 pix[-4 * xstride + 3 * ystride]
#define TP2 pix[-3 * xstride + 3 * ystride]
#define TP1 pix[-2 * xstride + 3 * ystride]
#define TP0 pix[-1 * xstride + 3 * ystride]
#define TQ0 pix[0  * xstride + 3 * ystride]
#define TQ1 pix[1  * xstride + 3 * ystride]
#define TQ2 pix[2  * xstride + 3 * ystride]
#define TQ3 pix[3  * xstride + 3 * ystride]

static void FUNC(hevc_loop_filter_luma)(uint8_t *_pix,
                                        ptrdiff_t _xstride, ptrdiff_t _ystride,
                                        int *_beta, int *_tc,
                                        uint8_t *_no_p, uint8_t *_no_q)
{
    int d, j;
    pixel *pix        = (pixel *)_pix;
    ptrdiff_t xstride = _xstride / sizeof(pixel);
    ptrdiff_t ystride = _ystride / sizeof(pixel);

    for (j = 0; j < 2; j++) {
        const int dp0  = abs(P2  - 2 * P1  + P0);
        const int dq0  = abs(Q2  - 2 * Q1  + Q0);
        const int dp3  = abs(TP2 - 2 * TP1 + TP0);
        const int dq3  = abs(TQ2 - 2 * TQ1 + TQ0);
        const int d0   = dp0 + dq0;
        const int d3   = dp3 + dq3;
        const int beta = _beta[j] << (BIT_DEPTH - 8);
        const int tc   = _tc[j]   << (BIT_DEPTH - 8);
        const int no_p = _no_p[j];
        const int no_q = _no_q[j];

        if (d0 + d3 >= beta) {
            pix += 4 * ystride;
            continue;
        } else {
            const int beta_3 = beta >> 3;
            const int beta_2 = beta >> 2;
            const int tc25   = ((tc * 5 + 1) >> 1);

            if (abs(P3  -  P0) + abs(Q3  -  Q0) < beta_3 && abs(P0  -  Q0) < tc25 &&
                abs(TP3 - TP0) + abs(TQ3 - TQ0) < beta_3 && abs(TP0 - TQ0) < tc25 &&
                                      (d0 << 1) < beta_2 &&      (d3 << 1) < beta_2) {
                // strong filtering
                const int tc2 = tc << 1;
                for (d = 0; d < 4; d++) {
                    const int p3 = P3;
                    const int p2 = P2;
                    const int p1 = P1;
                    const int p0 = P0;
                    const int q0 = Q0;
                    const int q1 = Q1;
                    const int q2 = Q2;
                    const int q3 = Q3;
                    if (!no_p) {
                        P0 = p0 + av_clip(((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3) - p0, -tc2, tc2);
                        P1 = p1 + av_clip(((p2 + p1 + p0 + q0 + 2) >> 2) - p1, -tc2, tc2);
                        P2 = p2 + av_clip(((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3) - p2, -tc2, tc2);
                    }
                    if (!no_q) {
                        Q0 = q0 + av_clip(((p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3) - q0, -tc2, tc2);
                        Q1 = q1 + av_clip(((p0 + q0 + q1 + q2 + 2) >> 2) - q1, -tc2, tc2);
                        Q2 = q2 + av_clip(((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3) - q2, -tc2, tc2);
                    }
                    pix += ystride;
                }
            } else { // normal filtering
                int nd_p = 1;
                int nd_q = 1;
                const int tc_2 = tc >> 1;
                if (dp0 + dp3 < ((beta + (beta >> 1)) >> 3))
                    nd_p = 2;
                if (dq0 + dq3 < ((beta + (beta >> 1)) >> 3))
                    nd_q = 2;

                for (d = 0; d < 4; d++) {
                    const int p2 = P2;
                    const int p1 = P1;
                    const int p0 = P0;
                    const int q0 = Q0;
                    const int q1 = Q1;
                    const int q2 = Q2;
                    int delta0   = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;
                    if (abs(delta0) < 10 * tc) {
                        delta0 = av_clip(delta0, -tc, tc);
                        if (!no_p)
                            P0 = av_clip_pixel(p0 + delta0);
                        if (!no_q)
                            Q0 = av_clip_pixel(q0 - delta0);
                        if (!no_p && nd_p > 1) {
                            const int deltap1 = av_clip((((p2 + p0 + 1) >> 1) - p1 + delta0) >> 1, -tc_2, tc_2);
                            P1 = av_clip_pixel(p1 + deltap1);
                        }
                        if (!no_q && nd_q > 1) {
                            const int deltaq1 = av_clip((((q2 + q0 + 1) >> 1) - q1 - delta0) >> 1, -tc_2, tc_2);
                            Q1 = av_clip_pixel(q1 + deltaq1);
                        }
                    }
                    pix += ystride;
                }
            }
        }
    }
}

static void FUNC(hevc_loop_filter_chroma)(uint8_t *_pix, ptrdiff_t _xstride,
                                          ptrdiff_t _ystride, int *_tc,
                                          uint8_t *_no_p, uint8_t *_no_q)
{
    int d, j, no_p, no_q;
    pixel *pix        = (pixel *)_pix;
    ptrdiff_t xstride = _xstride / sizeof(pixel);
    ptrdiff_t ystride = _ystride / sizeof(pixel);

    for (j = 0; j < 2; j++) {
        const int tc = _tc[j] << (BIT_DEPTH - 8);
        if (tc <= 0) {
            pix += 4 * ystride;
            continue;
        }
        no_p = _no_p[j];
        no_q = _no_q[j];

        for (d = 0; d < 4; d++) {
            int delta0;
            const int p1 = P1;
            const int p0 = P0;
            const int q0 = Q0;
            const int q1 = Q1;
            delta0 = av_clip((((q0 - p0) * 4) + p1 - q1 + 4) >> 3, -tc, tc);
            if (!no_p)
                P0 = av_clip_pixel(p0 + delta0);
            if (!no_q)
                Q0 = av_clip_pixel(q0 - delta0);
            pix += ystride;
        }
    }
}

static void FUNC(hevc_h_loop_filter_chroma)(uint8_t *pix, ptrdiff_t stride,
                                            int *tc, uint8_t *no_p,
                                            uint8_t *no_q)
{
    FUNC(hevc_loop_filter_chroma)(pix, stride, sizeof(pixel), tc, no_p, no_q);
}

static void FUNC(hevc_v_loop_filter_chroma)(uint8_t *pix, ptrdiff_t stride,
                                            int *tc, uint8_t *no_p,
                                            uint8_t *no_q)
{
    FUNC(hevc_loop_filter_chroma)(pix, sizeof(pixel), stride, tc, no_p, no_q);
}

static void FUNC(hevc_h_loop_filter_luma)(uint8_t *pix, ptrdiff_t stride,
                                          int *beta, int *tc, uint8_t *no_p,
                                          uint8_t *no_q)
{
    FUNC(hevc_loop_filter_luma)(pix, stride, sizeof(pixel),
                                beta, tc, no_p, no_q);
}

static void FUNC(hevc_v_loop_filter_luma)(uint8_t *pix, ptrdiff_t stride,
                                          int *beta, int *tc, uint8_t *no_p,
                                          uint8_t *no_q)
{
    FUNC(hevc_loop_filter_luma)(pix, sizeof(pixel), stride,
                                beta, tc, no_p, no_q);
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

#ifdef SVC_EXTENSION


#define LumVer_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + pel[3]*coeff[3] + pel[4]*coeff[4] + pel[5]*coeff[5] + pel[6]*coeff[6] + pel[7]*coeff[7])
#define CroVer_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + pel[3]*coeff[3])
#define CroVer_FILTER1(pel, coeff, widthEL) \
(pel[0]*coeff[0] + pel[widthEL]*coeff[1] + pel[widthEL*2]*coeff[2] + pel[widthEL*3]*coeff[3])
#define LumVer_FILTER1(pel, coeff, width) \
(pel[0]*coeff[0] + pel[width]*coeff[1] + pel[width*2]*coeff[2] + pel[width*3]*coeff[3] + pel[width*4]*coeff[4] + pel[width*5]*coeff[5] + pel[width*6]*coeff[6] + pel[width*7]*coeff[7])

// Define the function for up-sampling
#define LumHor_FILTER_Block(pel, coeff) \
(pel[-3]*coeff[0] + pel[-2]*coeff[1] + pel[-1]*coeff[2] + pel[0]*coeff[3] + pel[1]*coeff[4] + pel[2]*coeff[5] + pel[3]*coeff[6] + pel[4]*coeff[7])
#define CroHor_FILTER_Block(pel, coeff) \
(pel[-1]*coeff[0] + pel[0]*coeff[1] + pel[1]*coeff[2] + pel[2]*coeff[3])
#define LumVer_FILTER_Block(pel, coeff, width) \
(pel[-3*width]*coeff[0] + pel[-2*width]*coeff[1] + pel[-width]*coeff[2] + pel[0]*coeff[3] + pel[width]*coeff[4] + pel[2*width]*coeff[5] + pel[3*width]*coeff[6] + pel[4*width]*coeff[7])
#define CroVer_FILTER_Block(pel, coeff, width) \
(pel[-width]*coeff[0] + pel[0]*coeff[1] + pel[width]*coeff[2] + pel[2*width]*coeff[3])
#define LumHor_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + pel[3]*coeff[3] + pel[4]*coeff[4] + pel[5]*coeff[5] + pel[6]*coeff[6] + pel[7]*coeff[7])
#define CroHor_FILTER(pel, coeff) \
(pel[0]*coeff[0] + pel[1]*coeff[1] + pel[2]*coeff[2] + pel[3]*coeff[3])

/*      ------- Spatial horizontal upsampling filter  --------    */
static void FUNC(upsample_filter_block_luma_h_all)( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                        const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info/*, int y_BL, short * buffer_frame*/) {
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j, phase, refPos16, refPos;
    int16_t*   dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src;
    const int8_t*   coeff;
    //short * srcY1;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartL, rightEndL);
        refPos16 = (((x - leftStartL)*up_info->scaleXLum + up_info->addXLum) >> 12);
        phase    = refPos16 & 15;
        //printf("x %d phase %d \n", x, phase);
        coeff    = up_sample_filter_luma[phase];
        refPos   = (refPos16 >> 4) - x_BL;
        dst_tmp  = _dst  + i;
        src_tmp  = src   + refPos;
        //srcY1 = buffer_frame + y_BL*widthEL+ x_EL+i;
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = LumHor_FILTER_Block(src_tmp, coeff);
            /*if(*srcY1 != *dst_tmp)
                printf("--- %d %d %d %d %d %d %d %d %d \n",refPos, i, j, *srcY1, *dst_tmp, src_tmp[-3], src_tmp[-2], src_tmp[-1], src_tmp[0]);*/
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
            //srcY1    += widthEL;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_all)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                 int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                 const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j, phase, refPos16, refPos;
    int16_t*  dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src;
    const int8_t*  coeff;
    
    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartC, rightEndC);
        refPos16 = (((x - leftStartC)*up_info->scaleXCr + up_info->addXCr) >> 12);
        phase    = refPos16 & 15;
        coeff    = up_sample_filter_chroma[phase];
        refPos   = (refPos16 >> 4) - (x_BL);
        dst_tmp  = dst  + i;
        src_tmp  = src + refPos;
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  CroHor_FILTER_Block(src_tmp, coeff);
            src_tmp  +=  _srcstride;
            dst_tmp   +=  dststride;
        }
    }
}

/*      ------- Spatial vertical upsampling filter  --------    */
static void FUNC(upsample_filter_block_luma_v_all)( uint8_t *_dst, ptrdiff_t _dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                   int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                   const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = heightEL - Enhscal->bottom_offset;
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;

    int y, i, j, phase, refPos16, refPos;
    const int8_t  *   coeff;
    pixel *dst_tmp, *dst    = (pixel *)_dst;
    int16_t *   src_tmp;
    for( j = 0; j < block_h; j++ )	{
    	y        =   av_clip_c(y_EL+j, topStartL, bottomEndL-1);
    	refPos16 = ((( y - topStartL )* up_info->scaleYLum + up_info->addYLum) >> 12);
        phase    = refPos16 & 15;
        //printf("y %d phase %d \n", y, phase);

        coeff    = up_sample_filter_luma[phase];
        refPos   = (refPos16 >> 4) - y_BL;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + (y_EL+j) * _dststride + x_EL;
        for( i = 0; i < block_w; i++ )	{
            *dst_tmp = av_clip_pixel( (LumVer_FILTER_Block(src_tmp, coeff, _srcstride) + I_OFFSET) >> (N_SHIFT));

           /* uint8_t dst_tmp0;
            dst_tmp0 = av_clip_pixel( (LumVer_FILTER_Block(src_tmp, coeff, _srcstride) + I_OFFSET) >> (N_SHIFT));
            if(dst_tmp0 != *dst_tmp)
                printf("%d %d   --  %d %d \n", j, i, dst_tmp0, *dst_tmp);
            */
            if( ((x_EL+i) >= leftStartL) && ((x_EL+i) <= rightEndL-2) ){
                src_tmp++;
            }
            dst_tmp++;
        }
    }
}

static void FUNC(upsample_filter_block_cr_v_all)( uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                 int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                 const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int topStartC  = Enhscal->top_offset>>1;
    int bottomEndC = heightEL - (Enhscal->bottom_offset>>1);
    int y, i, j, phase, refPos16, refPos;
    const int8_t* coeff;
    int16_t *   src_tmp;
    pixel *dst_tmp, *dst    = (pixel *)_dst;
    for( j = 0; j < block_h; j++ )	{
        y =   av_clip_c(y_EL+j, topStartC, bottomEndC-1);
        refPos16 = ((( y - topStartC )* up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        phase    = refPos16 & 15;
        coeff    = up_sample_filter_chroma[phase];
        refPos   = (refPos16>>4) - y_BL;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + y* dststride + x_EL;
        for( i = 0; i < block_w; i++ )	{
            *dst_tmp = av_clip_pixel( (CroVer_FILTER_Block(src_tmp, coeff, _srcstride) + I_OFFSET) >> (N_SHIFT));
            if( ((x_EL+i) >= leftStartC) && ((x_EL+i) <= rightEndC-2) )
                src_tmp++;
            dst_tmp++;
        }
    }
}

/*      ------- x2 spatial horizontal upsampling filter  --------    */
static void FUNC(upsample_filter_block_luma_h_x2)( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j;
    int16_t*   dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src - x_BL;
    const int8_t*   coeff;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartL, rightEndL);
        coeff    = up_sample_filter_luma_x2[x&0x01];
        dst_tmp  = _dst  + i;
        src_tmp  = src + ((x-leftStartL)>>1);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = LumHor_FILTER_Block(src_tmp, coeff);
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_x2)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j;
    int16_t*  dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src - x_BL;
    const int8_t*  coeff;
    
    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartC, rightEndC);
        coeff    = up_sample_filter_chroma_x2[x&0x01];
        dst_tmp  = dst  + i;
        src_tmp  = src + (x>>1) ;
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  CroHor_FILTER_Block(src_tmp, coeff);
            src_tmp  +=  _srcstride;
            dst_tmp   +=  dststride;
        }
    }
}

/*      ------- x2 spatial vertical upsampling filter  --------    */

static void FUNC(upsample_filter_block_luma_v_x2)( uint8_t *_dst, ptrdiff_t _dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                  int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = heightEL - Enhscal->bottom_offset;
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int y, i, j;
    const int8_t  *   coeff;
    pixel *dst_tmp, *dst    = (pixel *)_dst + y_EL * _dststride + x_EL;
    int16_t *   src_tmp;

    for( j = 0; j < block_h; j++ )	{
    	y        = av_clip_c(y_EL+j, topStartL, bottomEndL-1);
        coeff    = up_sample_filter_luma_x2[(y-topStartL)&0x01];
        src_tmp  = _src  + (((y-topStartL)>>1)-y_BL)  * _srcstride;
        dst_tmp  =  dst;
        for( i = 0; i < block_w; i++ )	{
            *dst_tmp = av_clip_pixel( (LumVer_FILTER_Block(src_tmp, coeff, _srcstride) + I_OFFSET) >> (N_SHIFT));
            if( ((x_EL+i) >= leftStartL) && ((x_EL+i) <= rightEndL-2) )
                src_tmp++;
            dst_tmp++;
        }
        dst  +=  _dststride;
    }
}


static void FUNC(upsample_filter_block_cr_v_x2)( uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int topStartC  = Enhscal->top_offset>>1;
    int bottomEndC = heightEL - (Enhscal->bottom_offset>>1);
    int y, i, j, refPos16, refPos;
    const int8_t* coeff;
    int16_t *   src_tmp;
    pixel *dst_tmp, *dst    = (pixel *)_dst;
    for( j = 0; j < block_h; j++ )	{
        y =   av_clip_c(y_EL+j, topStartC, bottomEndC-1);
        refPos16 = ((( y - topStartC )* up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        coeff = up_sample_filter_chroma_x2_v[y&0x01];
        refPos   = (refPos16>>4) - y_BL;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + y* dststride + x_EL;
        for( i = 0; i < block_w; i++ ) {
            *dst_tmp = av_clip_pixel( (CroVer_FILTER_Block(src_tmp, coeff, _srcstride) + I_OFFSET) >> (N_SHIFT));
            if( ((x_EL+i) >= leftStartC) && ((x_EL+i) <= rightEndC-2) )
                src_tmp++;
            dst_tmp++;
        }
    }
}
/*      ------- x1.5 spatial horizontal upsampling filter  --------    */
static void FUNC(upsample_filter_block_luma_h_x1_5)( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j;
    int16_t*   dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src - x_BL;
    const int8_t*   coeff;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartL, rightEndL);
        coeff    = up_sample_filter_luma_x1_5[(x-leftStartL)%3];
        dst_tmp  = _dst  + i;
        src_tmp  = src + (((x-leftStartL)<<1)/3);

        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = LumHor_FILTER_Block(src_tmp, coeff);
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_x1_5)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j;
    int16_t*  dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src - x_BL;
    const int8_t*  coeff;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartC, rightEndC);
        coeff    = up_sample_filter_chroma_x1_5[(x-leftStartC)%3];
        dst_tmp  = dst  + i;
        src_tmp  = src + (((x-leftStartC)<<1)/3);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  CroHor_FILTER_Block(src_tmp, coeff);
            src_tmp  +=  _srcstride;
            dst_tmp   +=  dststride;
        }
    }
}

static void FUNC(upsample_filter_block_luma_v_x1_5)( uint8_t *_dst, ptrdiff_t _dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                    int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                    const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = heightEL - Enhscal->bottom_offset;
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int y, i, j;
    const int8_t  *   coeff;
    pixel *dst_tmp, *dst    = (pixel *)_dst + x_EL + y_EL * _dststride;
    int16_t *   src_tmp;

    for( j = 0; j < block_h; j++ )	{
    	y        =   av_clip_c(y_EL+j, topStartL, bottomEndL-1);
        coeff    = up_sample_filter_luma_x1_5[(y - topStartL)%3];
        src_tmp  = _src  + ((( y - topStartL )<<1)/3 - y_BL )  * _srcstride;
        dst_tmp  =  dst;
        for( i = 0; i < block_w; i++ )	{
            *dst_tmp = av_clip_pixel( (LumVer_FILTER_Block(src_tmp, coeff, _srcstride) + I_OFFSET) >> (N_SHIFT));
            if( ((x_EL+i) >= leftStartL) && ((x_EL+i) <= rightEndL-2) )
                src_tmp++;
            dst_tmp++;
        }
        dst  += _dststride;
    }
}

static void FUNC(upsample_filter_block_cr_v_x1_5)( uint8_t *_dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
                                                  int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int topStartC  = Enhscal->top_offset>>1;
    int bottomEndC = heightEL - (Enhscal->bottom_offset>>1);
    int y, i, j, refPos16, refPos;
    const int8_t* coeff;
    int16_t *   src_tmp;
    pixel *dst_tmp, *dst    = (pixel *)_dst;
    for ( j = 0; j < block_h; j++ ) {
        y        = av_clip_c(y_EL+j, topStartC, bottomEndC-1);
        refPos16 = ((( y - topStartC )* up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        coeff    = up_sample_filter_x1_5chroma[y%3];
        refPos   = (refPos16>>4) - y_BL;
        src_tmp  = _src  + refPos  * _srcstride;
        dst_tmp  =  dst  + y* dststride + x_EL;
        for ( i = 0; i < block_w; i++ ) {
            *dst_tmp = av_clip_pixel( (CroVer_FILTER_Block(src_tmp, coeff, _srcstride) + I_OFFSET) >> (N_SHIFT));
            if( ((x_EL+i) >= leftStartC) && ((x_EL+i) <= rightEndC-2) )
                src_tmp++;
            dst_tmp++;
        }
    }
}

static void FUNC(upsample_base_layer_frame)(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel)
{
    int i,j, k;
    
    int widthBL =  FrameBL->coded_width;
    int heightBL = FrameBL->coded_height;
    int strideBL = FrameBL->linesize[0];
    int widthEL =  FrameEL->coded_width - Enhscal->left_offset - Enhscal->right_offset;
    int heightEL = FrameEL->coded_height - Enhscal->top_offset - Enhscal->bottom_offset;
    int strideEL = FrameEL->linesize[0];
    pixel *srcBufY = (pixel*)FrameBL->data[0];
    pixel *dstBufY = (pixel*)FrameEL->data[0];
    short *tempBufY = Buffer[0];
    pixel *srcY;
    pixel *dstY;
    short *dstY1;
    short *srcY1;
    pixel *srcBufU = (pixel*)FrameBL->data[1];
    pixel *dstBufU = (pixel*)FrameEL->data[1];
    short *tempBufU = Buffer[1];
    pixel *srcU;
    pixel *dstU;
    short *dstU1;
    short *srcU1;
    
    pixel *srcBufV = (pixel*)FrameBL->data[2];
    pixel *dstBufV = (pixel*)FrameEL->data[2];
    short *tempBufV = Buffer[2];
    pixel *srcV;
    pixel *dstV;
    short *dstV1;
    short *srcV1;
    
    int refPos16 = 0;
    int phase    = 0;
    int refPos   = 0;
    const int8_t* coeff;
    int leftStartL = Enhscal->left_offset;
    int rightEndL  = FrameEL->coded_width - Enhscal->right_offset;
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = FrameEL->coded_height - Enhscal->bottom_offset;
    pixel buffer[8];

    const int nShift = 20-8;
    int iOffset = 1 << (nShift - 1);
    short buffer1[8];

    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = (FrameEL->coded_width>>1) - (Enhscal->right_offset>>1);
    int topStartC  = Enhscal->top_offset>>1;
    int bottomEndC = (FrameEL->coded_height>>1) - (Enhscal->bottom_offset>>1);

    widthEL   = FrameEL->coded_width;  //pcUsPic->getWidth ();
    heightEL  = FrameEL->coded_height; //pcUsPic->getHeight();

    widthBL   = FrameBL->coded_width;
    heightBL  = FrameBL->coded_height <= heightEL ? FrameBL->coded_height:heightEL;  // min( FrameBL->height, heightEL);

    for( i = 0; i < widthEL; i++ )	{
        int x = av_clip_c(i, leftStartL, rightEndL);
        refPos16 = (((x - leftStartL)*up_info->scaleXLum + up_info->addXLum) >> 12);
        phase    = refPos16 & 15;
        refPos   = refPos16 >> 4;
        coeff = up_sample_filter_luma[phase];
        refPos -= ((NTAPS_LUMA>>1) - 1);
        srcY = srcBufY + refPos;
        dstY1 = tempBufY + i;
        if(refPos < 0)
            for( j = 0; j < heightBL ; j++ ) {

                memset(buffer, srcY[-refPos], -refPos);
                memcpy(buffer-refPos, srcY-refPos, 8+refPos);
                *dstY1 = LumHor_FILTER(buffer, coeff);

                srcY += strideBL;
                dstY1 += widthEL;//strideEL;
            } else if (refPos+8 > widthBL )
                for ( j = 0; j < heightBL ; j++ ) {
                    
                    memcpy(buffer, srcY, widthBL-refPos);
                    memset(buffer+(widthBL-refPos), srcY[widthBL-refPos-1], 8-(widthBL-refPos));
                    *dstY1 = LumHor_FILTER(buffer, coeff);
                    
                    srcY += strideBL;
                    dstY1 += widthEL;//strideEL;
                } else
                    for ( j = 0; j < heightBL ; j++ ) {
                        *dstY1 = LumHor_FILTER(srcY, coeff);
                        srcY += strideBL;
                        dstY1 += widthEL;//strideEL;
                    }
    }
    for ( j = 0; j < heightEL; j++ ) {
        int y = av_clip_c(j, topStartL, bottomEndL-1);
        refPos16 = ((( y - topStartL )*up_info->scaleYLum + up_info->addYLum) >> 12);
        phase    = refPos16 & 15;
        refPos   = refPos16 >> 4;
        coeff = up_sample_filter_luma[phase];
        refPos -= ((NTAPS_LUMA>>1) - 1);
        srcY1 = tempBufY + refPos *widthEL;
        dstY = dstBufY + j * strideEL;
        if (refPos < 0)
            for ( i = 0; i < widthEL; i++ ) {
                
                for(k= 0; k<-refPos ; k++)
                    buffer1[k] = srcY1[-refPos*widthEL]; //srcY1[(-refPos+k)*strideEL];
                for(k= 0; k<8+refPos ; k++)
                    buffer1[-refPos+k] = srcY1[(-refPos+k)*widthEL];
                *dstY = av_clip_pixel( (LumVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));

                if( (i >= leftStartL) && (i <= rightEndL-2) )
                    srcY1++;
                dstY++;
            } else if(refPos+8 > heightBL )
                for( i = 0; i < widthEL; i++ ) {
                    
                    for(k= 0; k<heightBL-refPos ; k++)
                        buffer1[k] = srcY1[k*widthEL];
                    for(k= 0; k<8-(heightBL-refPos) ; k++)
                        buffer1[heightBL-refPos+k] = srcY1[(heightBL-refPos-1)*widthEL];
                    *dstY = av_clip_pixel( (LumVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                    
                    if( (i >= leftStartL) && (i <= rightEndL-2) )
                        srcY1++;
                    dstY++;
                } else
                    for ( i = 0; i < widthEL; i++ ) {
                        *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                        if( (i >= leftStartL) && (i <= rightEndL-2) )
                            srcY1++;
                        dstY++;
                    }
    }
    
    widthBL   = FrameBL->coded_width;
    heightBL  = FrameBL->coded_height;
    
    widthEL   = FrameEL->coded_width - Enhscal->right_offset - Enhscal->left_offset;
    heightEL  = FrameEL->coded_height - Enhscal->top_offset - Enhscal->bottom_offset;
    
    widthEL  >>= 1;
    heightEL >>= 1;
    widthBL  >>= 1;
    heightBL >>= 1;
    strideBL  = FrameBL->linesize[1];
    strideEL  = FrameEL->linesize[1];
    widthEL   = FrameEL->coded_width >> 1;
    heightEL  = FrameEL->coded_height >> 1;
    widthBL   = FrameBL->coded_width >> 1;
    heightBL  = FrameBL->coded_height > heightEL ? FrameBL->coded_height:heightEL;
    
    
    heightBL >>= 1;
    
    //========== horizontal upsampling ===========
    for( i = 0; i < widthEL; i++ )	{
    	int x = av_clip_c(i, leftStartC, rightEndC - 1);
        refPos16 = (((x - leftStartC)*up_info->scaleXCr + up_info->addXCr) >> 12);
        phase    = refPos16 & 15;
        refPos   = refPos16 >> 4;
        coeff = up_sample_filter_chroma[phase];
        
        refPos -= ((NTAPS_CHROMA>>1) - 1);
        srcU = srcBufU + refPos; // -((NTAPS_CHROMA>>1) - 1);
        srcV = srcBufV + refPos; // -((NTAPS_CHROMA>>1) - 1);
        dstU1 = tempBufU + i;
        dstV1 = tempBufV + i;
        
        if(refPos < 0)
            for( j = 0; j < heightBL ; j++ )	{
                
        		memset(buffer, srcU[-refPos], -refPos);
                memcpy(buffer-refPos, srcU-refPos, 4+refPos);
                memset(buffer+4, srcV[-refPos], -refPos);
                memcpy(buffer-refPos+4, srcV-refPos, 4+refPos);
                
                *dstU1 = CroHor_FILTER(buffer, coeff);
                
                *dstV1 = CroHor_FILTER((buffer+4), coeff);
                
                
                srcU += strideBL;
                srcV += strideBL;
                dstU1 += widthEL;
                dstV1 += widthEL;
        	}else if(refPos+4 > widthBL )
                for( j = 0; j < heightBL ; j++ )	{
                    
                    memcpy(buffer, srcU, widthBL-refPos);
                    memset(buffer+(widthBL-refPos), srcU[widthBL-refPos-1], 4-(widthBL-refPos));
                    
                    memcpy(buffer+4, srcV, widthBL-refPos);
                    memset(buffer+(widthBL-refPos)+4, srcV[widthBL-refPos-1], 4-(widthBL-refPos));
                    
                    *dstU1 = CroHor_FILTER(buffer, coeff);
                    
                    *dstV1 = CroHor_FILTER((buffer+4), coeff);
                    
                	srcU += strideBL;
                    srcV += strideBL;
                    dstU1 += widthEL;
                    dstV1 += widthEL;
                } else
                    for ( j = 0; j < heightBL ; j++ ) {
                        *dstU1 = CroHor_FILTER(srcU, coeff);
                        *dstV1 = CroHor_FILTER(srcV, coeff);
                        srcU  += strideBL;
                        srcV  += strideBL;
                        dstU1 += widthEL;
                        dstV1 += widthEL;
                    }
    }

    for( j = 0; j < heightEL; j++ )	{
        int y = av_clip_c(j, topStartC, bottomEndC - 1);
        refPos16 = (((y - topStartC)*up_info->scaleYCr + up_info->addYCr) >> 12) - 4;
     //   printf("j %d refPos16 %d phase %d refPos %d refPos %d \n", j, refPos16, phase, refPos, refPos - ((NTAPS_CHROMA>>1) - 1) );
        phase    = refPos16 & 15;
        refPos   = refPos16 >> 4;
         
        coeff   = up_sample_filter_chroma[phase];
        refPos -= ((NTAPS_CHROMA>>1) - 1);
        srcU1   = tempBufU  + refPos *widthEL;
        srcV1   = tempBufV  + refPos *widthEL;
        dstU    = dstBufU + j*strideEL;
        dstV    = dstBufV + j*strideEL;
        if (refPos < 0)
            for ( i = 0; i < widthEL; i++ ) {
                for(k= 0; k<-refPos ; k++){
                    buffer1[k] = srcU1[(-refPos)*widthEL];
                    buffer1[k+4] = srcV1[(-refPos)*widthEL];
                }
                for(k= 0; k<4+refPos ; k++){
                    buffer1[-refPos+k] = srcU1[(-refPos+k)*widthEL];
                    buffer1[-refPos+k+4] = srcV1[(-refPos+k)*widthEL];
                }
                *dstU = av_clip_pixel( (CroVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                *dstV = av_clip_pixel( (CroVer_FILTER((buffer1+4), coeff) + iOffset) >> (nShift));
                if ( (i >= leftStartC) && (i <= rightEndC-2) ) {
                    srcU1++;
                    srcV1++;
                }
                dstU++;
                dstV++;
            } else if(refPos+4 > heightBL )
                for( i = 0; i < widthEL; i++ ) {
                    for (k= 0; k< heightBL-refPos ; k++) {
                        buffer1[k] = srcU1[k*widthEL];
                        buffer1[k+4] = srcV1[k*widthEL];
                    }
                    for (k= 0; k<4-(heightBL-refPos) ; k++) {
                        buffer1[heightBL-refPos+k] = srcU1[(heightBL-refPos-1)*widthEL];
                        buffer1[heightBL-refPos+k+4] = srcV1[(heightBL-refPos-1)*widthEL];
                    }
                    *dstU = av_clip_pixel( (CroVer_FILTER(buffer1, coeff) + iOffset) >> (nShift));
                    *dstV = av_clip_pixel( (CroVer_FILTER((buffer1+4), coeff) + iOffset) >> (nShift));
                    if ( (i >= leftStartC) && (i <= rightEndC-2) ) {
                        srcU1++;
                        srcV1++;
                    }
                    dstU++;
                    dstV++;
                } else
                    for ( i = 0; i < widthEL; i++ ) {
                        *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                        *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));
                        if ( (i >= leftStartC) && (i <= rightEndC-2) ) {
                            srcU1++;
                            srcV1++;
                        }
                        dstU++;
                        dstV++;
                    }
    }
}

#undef LumHor_FILTER
#undef LumCro_FILTER
#undef LumVer_FILTER
#undef CroVer_FILTER
#endif
