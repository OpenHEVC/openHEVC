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

static av_always_inline void FUNC(transquant_bypass)(uint8_t *_dst, int16_t *coeffs,
                                                     ptrdiff_t stride, int size)
{
    int x, y;
    pixel *dst = (pixel *)_dst;

    stride /= sizeof(pixel);

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            dst[x] = av_clip_pixel(dst[x] + *coeffs);
            coeffs++;
        }
        dst += stride;
    }
}

static void FUNC(transform_add4x4)(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride)
{
    FUNC(transquant_bypass)(_dst, coeffs, stride, 4);
}

static void FUNC(transform_add8x8)(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride)
{
    FUNC(transquant_bypass)(_dst, coeffs, stride, 8);
}

static void FUNC(transform_add16x16)(uint8_t *_dst, int16_t *coeffs,
                                         ptrdiff_t stride)
{
    FUNC(transquant_bypass)(_dst, coeffs, stride, 16);
}

static void FUNC(transform_add32x32)(uint8_t *_dst, int16_t *coeffs,
                                         ptrdiff_t stride)
{
    FUNC(transquant_bypass)(_dst, coeffs, stride, 32);
}


static void FUNC(transform_rdpcm)(int16_t *_coeffs, int16_t log2_size, int mode)
{
    int16_t *coeffs = (int16_t *) _coeffs;
    int x, y;
    int size = 1 << log2_size;

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
}

static void FUNC(transform_skip)(int16_t *_coeffs, int16_t log2_size)
{
    int shift  = 15 - BIT_DEPTH - log2_size;
    int x, y;
    int size = 1 << log2_size;
    int16_t *coeffs = _coeffs;


    if (shift > 0) {
        int offset = 1 << (shift - 1);
        for (y = 0; y < size; y++) {
            for (x = 0; x < size; x++) {
                *coeffs = (*coeffs + offset) >> shift;
                coeffs++;
            }
        }
    } else {
        for (y = 0; y < size; y++) {
            for (x = 0; x < size; x++) {
                *coeffs = *coeffs << -shift;
                coeffs++;
            }
        }
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

static void FUNC(transform_4x4_luma)(int16_t *coeffs)
{
    int i;
    int shift    = 7;
    int add      = 1 << (shift - 1);
    int16_t *src = coeffs;

    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(src, src, 4, SCALE);
        src++;
    }

    shift = 20 - BIT_DEPTH;
    add   = 1 << (shift - 1);
    for (i = 0; i < 4; i++) {
        TR_4x4_LUMA(coeffs, coeffs, 1, SCALE);
        coeffs += 4;
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

#define IDCT_VAR4(H)                                                          \
    int      limit2   = FFMIN(col_limit + 4, H)
#define IDCT_VAR8(H)                                                          \
        int      limit   = FFMIN(col_limit, H);                               \
        int      limit2   = FFMIN(col_limit + 4, H)
#define IDCT_VAR16(H)   IDCT_VAR8(H)
#define IDCT_VAR32(H)   IDCT_VAR8(H)

#define IDCT(H)                                                              \
static void FUNC(idct_##H ##x ##H )(                                         \
                   int16_t *coeffs, int col_limit) {                         \
    int i;                                                                   \
    int      shift   = 7;                                                    \
    int      add     = 1 << (shift - 1);                                     \
    int16_t *src     = coeffs;                                               \
    IDCT_VAR ##H(H);                                                         \
                                                                             \
    for (i = 0; i < H; i++) {                                                \
        TR_ ## H(src, src, H, H, SCALE, limit2);                             \
        if (limit2 < H && i%4 == 0 && !!i)                                   \
            limit2 -= 4;                                                     \
        src++;                                                               \
    }                                                                        \
                                                                             \
    shift   = 20 - BIT_DEPTH;                                                \
    add     = 1 << (shift - 1);                                              \
    for (i = 0; i < H; i++) {                                                \
        TR_ ## H(coeffs, coeffs, 1, 1, SCALE, limit);                        \
        coeffs += H;                                                         \
    }                                                                        \
}

#define IDCT_DC(H)                                                           \
static void FUNC(idct_##H ##x ##H ##_dc)(                                    \
                   int16_t *coeffs) {                                        \
    int i, j;                                                                \
    int      shift   = 14 - BIT_DEPTH;                                       \
    int      add     = 1 << (shift - 1);                                     \
    int      coeff   = (((coeffs[0] + 1) >> 1) + add) >> shift;              \
                                                                             \
    for (j = 0; j < H; j++) {                                                \
        for (i = 0; i < H; i++) {                                            \
            coeffs[i+j*H] = coeff;                                           \
        }                                                                    \
    }                                                                        \
}

IDCT( 4)
IDCT( 8)
IDCT(16)
IDCT(32)

IDCT_DC( 4)
IDCT_DC( 8)
IDCT_DC(16)
IDCT_DC(32)

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
                                    ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                    SAOParams *sao,
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

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    for (k = 0; k < 4; k++)
        offset_table[(k + sao_left_class) & 31] = sao_offset_val[k + 1];
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            dst[x] = av_clip_pixel(src[x] + offset_table[src[x] >> shift]);
        dst += stride_dst;
        src += stride_src;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define CMP(a, b) ((a) > (b) ? 1 : ((a) == (b) ? 0 : -1))

static void FUNC(sao_edge_filter)(uint8_t *_dst, uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  SAOParams *sao,
                                  int width, int height,
                                  int c_idx) {
    static const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };
    static const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t eo = sao->eo_class[c_idx];
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;

    int a_stride, b_stride;
    int src_offset = 0;
    int dst_offset = 0;
    int x, y;
    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    a_stride = pos[eo][0][0] + pos[eo][0][1] * stride_src;
    b_stride = pos[eo][1][0] + pos[eo][1][1] * stride_src;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int diff0         = CMP(src[x + src_offset], src[x + src_offset + a_stride]);
            int diff1         = CMP(src[x + src_offset], src[x + src_offset + b_stride]);
            int offset_val    = edge_idx[2 + diff0 + diff1];
            dst[x + dst_offset] = av_clip_pixel(src[x + src_offset] + sao_offset_val[offset_val]);
        }
        src_offset += stride_src;
        dst_offset += stride_dst;
    }
}

static void FUNC(sao_edge_restore_0)(uint8_t *_dst, uint8_t *_src,
                                    ptrdiff_t stride_dst,  ptrdiff_t stride_src,
                                    SAOParams *sao,
                                    int *borders, int _width, int _height,
                                    int c_idx, uint8_t *vert_edge,
                                    uint8_t *horiz_edge, uint8_t *diag_edge)
{
    int x, y;
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int16_t *sao_offset_val = sao->offset_val[c_idx];
    uint8_t sao_eo_class    = sao->eo_class[c_idx];
    int init_x = 0, /*init_y = 0,*/ width = _width, height = _height;

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    if (sao_eo_class != SAO_EO_VERT) {
        if (borders[0]) {
            int offset_val = sao_offset_val[0];
            int y_stride_src   = 0;
            int y_stride_dst   = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride_dst] = av_clip_pixel(src[y_stride_src] + offset_val);
                y_stride_src     += stride_src;
                y_stride_dst     += stride_dst;
            }
            init_x = 1;
        }
        if (borders[2]) {
            int offset_val = sao_offset_val[0];
            int x_stride_src   = width - 1;
            int x_stride_dst   = width - 1;
            for (x = 0; x < height; x++) {
                dst[x_stride_dst] = av_clip_pixel(src[x_stride_src] + offset_val);
                x_stride_src     += stride_src;
                x_stride_dst     += stride_dst;
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
            int y_stride_src   = stride_src * (height - 1);
            int y_stride_dst   = stride_dst * (height - 1);
            for (x = init_x; x < width; x++)
                dst[x + y_stride_dst] = av_clip_pixel(src[x + y_stride_src] + offset_val);
            height--;
        }
    }
}

static void FUNC(sao_edge_restore_1)(uint8_t *_dst, uint8_t *_src,
                                    ptrdiff_t stride_dst, ptrdiff_t stride_src, 
                                    SAOParams *sao,
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

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    if (sao_eo_class != SAO_EO_VERT) {
        if (borders[0]) {
            int offset_val = sao_offset_val[0];
            int y_stride_src   = 0;
            int y_stride_dst   = 0;
            for (y = 0; y < height; y++) {
                dst[y_stride_dst] = av_clip_pixel(src[y_stride_src] + offset_val);
                y_stride_src     += stride_src;
                y_stride_dst     += stride_dst;
            }
            init_x = 1;
        }
        if (borders[2]) {
            int offset_val = sao_offset_val[0];
            int x_stride_src   = width - 1;
            int x_stride_dst   = width - 1;
            for (x = 0; x < height; x++) {
                dst[x_stride_dst] = av_clip_pixel(src[x_stride_src] + offset_val);
                x_stride_src     += stride_src;
                x_stride_dst     += stride_dst;
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
            int y_stride_src   = stride_src * (height - 1);
            int y_stride_dst   = stride_dst * (height - 1);
            for (x = init_x; x < width; x++)
                dst[x + y_stride_dst] = av_clip_pixel(src[x + y_stride_src] + offset_val);
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
                dst[y*stride_dst] = src[y*stride_src];
        }
        if(vert_edge[1] && sao_eo_class != SAO_EO_VERT) {
            for(y = init_y+save_upper_right; y< height-save_lower_right; y++)
                dst[y*stride_dst+width-1] = src[y*stride_src+width-1];
        }

        if(horiz_edge[0] && sao_eo_class != SAO_EO_HORIZ) {
            for(x = init_x+save_upper_left; x < width-save_upper_right; x++)
                dst[x] = src[x];
        }
        if(horiz_edge[1] && sao_eo_class != SAO_EO_HORIZ) {
            for(x = init_x+save_lower_left; x < width-save_lower_right; x++)
                dst[(height-1)*stride_dst+x] = src[(height-1)*stride_src+x];
        }
        if(diag_edge[0] && sao_eo_class == SAO_EO_135D)
            dst[0] = src[0];
        if(diag_edge[1] && sao_eo_class == SAO_EO_45D)
            dst[width-1] = src[width-1];
        if(diag_edge[2] && sao_eo_class == SAO_EO_135D)
            dst[stride_dst*(height-1)+width-1] = src[stride_src*(height-1)+width-1];
        if(diag_edge[3] && sao_eo_class == SAO_EO_45D)
            dst[stride_dst*(height-1)] = src[stride_src*(height-1)];

    }
}

#undef CMP

#if COM16_C806_EMT
#if BIT_DEPTH < 9
DECLARE_ALIGNED(16, static const int16_t, DCT_II_4x4[4][4]) =
{
    { 256,  256,  256,  256 },
    { 334,  139, -139, -334 },
    { 256, -256, -256,  256 },
    { 139, -334,  334, -139 }
};

DECLARE_ALIGNED(16, static const int16_t, DCT_II_8x8[8][8]) =
{
    { 256,  256,  256,  256,  256,  256,  256,  256 },
    { 355,  301,  201,   71,  -71, -201, -301, -355 },
    { 334,  139, -139, -334, -334, -139,  139,  334 },
    { 301,  -71, -355, -201,  201,  355,   71, -301 },
    { 256, -256, -256,  256,  256, -256, -256,  256 },
    { 201, -355,   71,  301, -301,  -71,  355, -201 },
    { 139, -334,  334, -139, -139,  334, -334,  139 },
    {  71, -201,  301, -355,  355, -301,  201,  -71 }
};

DECLARE_ALIGNED(16, static const int16_t, DCT_II_16x16[16][16]) =
{
    { 256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256 },
    { 360,  346,  319,  280,  230,  171,  105,   35,  -35, -105, -171, -230, -280, -319, -346, -360 },
    { 355,  301,  201,   71,  -71, -201, -301, -355, -355, -301, -201,  -71,   71,  201,  301,  355 },
    { 346,  230,   35, -171, -319, -360, -280, -105,  105,  280,  360,  319,  171, -35,  -230, -346 },
    { 334,  139, -139, -334, -334, -139,  139,  334,  334,  139, -139, -334, -334, -139,  139,  334 },
    { 319,   35, -280, -346, -105,  230,  360,  171, -171, -360, -230,  105,  346,  280,  -35, -319 },
    { 301,  -71, -355, -201,  201,  355,   71, -301, -301,   71,  355,  201, -201, -355,  -71,  301 },
    { 280, -171, -346,   35,  360,  105, -319, -230,  230,  319, -105, -360,  -35,  346,  171, -280 },
    { 256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256 },
    { 230, -319, -105,  360,  -35, -346,  171,  280, -280, -171,  346,   35, -360,  105,  319, -230 },
    { 201, -355,   71,  301, -301,  -71,  355, -201, -201,  355,  -71, -301,  301,   71, -355,  201 },
    { 171, -360,  230,  105, -346,  280,   35, -319,  319,  -35, -280,  346, -105, -230,  360, -171 },
    { 139, -334,  334, -139, -139,  334, -334,  139,  139, -334,  334, -139, -139,  334, -334,  139 },
    { 105, -280,  360, -319,  171,   35, -230,  346, -346,  230,  -35, -171,  319, -360,  280, -105 },
    {  71, -201,  301, -355,  355, -301,  201,  -71,  -71,  201, -301,  355, -355,  301, -201,   71 },
    {  35, -105,  171, -230,  280, -319,  346, -360,  360, -346,  319, -280,  230, -171,  105,  -35 }
};

DECLARE_ALIGNED(16, static const int16_t, DCT_II_32x32[32][32]) =
{
    { 256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256 },
    { 362,  358,  351,  341,  327,  311,  291,  268,  243,  216,  186,  155,  122,   88,   53,   18,  -18,  -53,  -88, -122, -155, -186, -216, -243, -268, -291, -311, -327, -341, -351, -358, -362 },
    { 360,  346,  319,  280,  230,  171,  105,   35,  -35, -105, -171, -230, -280, -319, -346, -360, -360, -346, -319, -280, -230, -171, -105,  -35,   35,  105,  171,  230,  280,  319,  346,  360 },
    { 358,  327,  268,  186,   88,  -18, -122, -216, -291, -341, -362, -351, -311, -243, -155,  -53,   53,  155,  243,  311,  351,  362,  341,  291,  216,  122,   18,  -88, -186, -268, -327, -358 },
    { 355,  301,  201,   71,  -71, -201, -301, -355, -355, -301, -201,  -71,   71,  201,  301,  355,  355,  301,  201,   71,  -71, -201, -301, -355, -355, -301, -201,  -71,   71,  201,  301,  355 },
    { 351,  268,  122,  -53, -216, -327, -362, -311, -186,  -18,  155,  291,  358,  341,  243,   88,  -88, -243, -341, -358, -291, -155,   18,  186,  311,  362,  327,  216,   53, -122, -268, -351 },
    { 346,  230,   35, -171, -319, -360, -280, -105,  105,  280,  360,  319,  171,  -35, -230, -346, -346, -230,  -35,  171,  319,  360,  280,  105, -105, -280, -360, -319, -171,   35,  230,  346 },
    { 341,  186,  -53, -268, -362, -291,  -88,  155,  327,  351,  216,  -18, -243, -358, -311, -122,  122,  311,  358,  243,   18, -216, -351, -327, -155,   88,  291,  362,  268,   53, -186, -341 },
    { 334,  139, -139, -334, -334, -139,  139,  334,  334,  139, -139, -334, -334, -139,  139,  334,  334,  139, -139, -334, -334, -139,  139,  334,  334,  139, -139, -334, -334, -139,  139,  334 },
    { 327,   88, -216, -362, -243,   53,  311,  341,  122, -186, -358, -268,   18,  291,  351,  155, -155, -351, -291,  -18,  268,  358,  186, -122, -341, -311,  -53,  243,  362,  216,  -88, -327 },
    { 319,   35, -280, -346, -105,  230,  360,  171, -171, -360, -230,  105,  346,  280,  -35, -319, -319,  -35,  280,  346,  105, -230, -360, -171,  171,  360,  230, -105, -346, -280,   35,  319 },
    { 311,  -18, -327, -291,   53,  341,  268,  -88, -351, -243,  122,  358,  216, -155, -362, -186,  186,  362,  155, -216, -358, -122,  243,  351,   88, -268, -341,  -53,  291,  327,   18, -311 },
    { 301,  -71, -355, -201,  201,  355,   71, -301, -301,   71,  355,  201, -201, -355,  -71,  301,  301,  -71, -355, -201,  201,  355,   71, -301, -301,   71,  355,  201, -201, -355,  -71,  301 },
    { 291, -122, -362,  -88,  311,  268, -155, -358,  -53,  327,  243, -186, -351,  -18,  341,  216, -216, -341,   18,  351,  186, -243, -327,   53,  358,  155, -268, -311,   88,  362,  122, -291 },
    { 280, -171, -346,   35,  360,  105, -319, -230,  230,  319, -105, -360,  -35,  346,  171, -280, -280,  171,  346,  -35, -360, -105,  319,  230, -230, -319,  105,  360,   35, -346, -171,  280 },
    { 268, -216, -311,  155,  341,  -88, -358,   18,  362,   53, -351, -122,  327,  186, -291, -243,  243,  291, -186, -327,  122,  351,  -53, -362,  -18,  358,   88, -341, -155,  311,  216, -268 },
    { 256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256,  256, -256, -256,  256 },
    { 243, -291, -186,  327,  122, -351,  -53,  362,  -18, -358,   88,  341, -155, -311,  216,  268, -268, -216,  311,  155, -341,  -88,  358,   18, -362,   53,  351, -122, -327,  186,  291, -243 },
    { 230, -319, -105,  360,  -35, -346,  171,  280, -280, -171,  346,   35, -360,  105,  319, -230, -230,  319,  105, -360,   35,  346, -171, -280,  280,  171, -346,  -35,  360, -105, -319,  230 },
    { 216, -341,  -18,  351, -186, -243,  327,   53, -358,  155,  268, -311,  -88,  362, -122, -291,  291,  122, -362,   88,  311, -268, -155,  358,  -53, -327,  243,  186, -351,   18,  341, -216 },
    { 201, -355,   71,  301, -301,  -71,  355, -201, -201,  355,  -71, -301,  301,   71, -355,  201,  201, -355,   71,  301, -301,  -71,  355, -201, -201,  355,  -71, -301,  301,   71, -355,  201 },
    { 186, -362,  155,  216, -358,  122,  243, -351,   88,  268, -341,   53,  291, -327,   18,  311, -311,  -18,  327, -291,  -53,  341, -268,  -88,  351, -243, -122,  358, -216, -155,  362, -186 },
    { 171, -360,  230,  105, -346,  280,   35, -319,  319,  -35, -280,  346, -105, -230,  360, -171, -171,  360, -230, -105,  346, -280,  -35,  319, -319,   35,  280, -346,  105,  230, -360,  171 },
    { 155, -351,  291,  -18, -268,  358, -186, -122,  341, -311,   53,  243, -362,  216,   88, -327,  327,  -88, -216,  362, -243,  -53,  311, -341,  122,  186, -358,  268,   18, -291,  351, -155 },
    { 139, -334,  334, -139, -139,  334, -334,  139,  139, -334,  334, -139, -139,  334, -334,  139,  139, -334,  334, -139, -139,  334, -334,  139,  139, -334,  334, -139, -139,  334, -334,  139 },
    { 122, -311,  358, -243,   18,  216, -351,  327, -155,  -88,  291, -362,  268,  -53, -186,  341, -341,  186,   53, -268,  362, -291,   88,  155, -327,  351, -216,  -18,  243, -358,  311, -122 },
    { 105, -280,  360, -319,  171,   35, -230,  346, -346,  230,  -35, -171,  319, -360,  280, -105, -105,  280, -360,  319, -171,  -35,  230, -346,  346, -230,   35,  171, -319,  360, -280,  105 },
    {  88, -243,  341, -358,  291, -155,  -18,  186, -311,  362, -327,  216,  -53, -122,  268, -351,  351, -268,  122,   53, -216,  327, -362,  311, -186,   18,  155, -291,  358, -341,  243,  -88 },
    {  71, -201,  301, -355,  355, -301,  201,  -71,  -71,  201, -301,  355, -355,  301, -201,   71,   71, -201,  301, -355,  355, -301,  201,  -71,  -71,  201, -301,  355, -355,  301, -201,   71 },
    {  53, -155,  243, -311,  351, -362,  341, -291,  216, -122,   18,   88, -186,  268, -327,  358, -358,  327, -268,  186,  -88,  -18,  122, -216,  291, -341,  362, -351,  311, -243,  155,  -53 },
    {  35, -105,  171, -230,  280, -319,  346, -360,  360, -346,  319, -280,  230, -171,  105,  -35,  -35,  105, -171,  230, -280,  319, -346,  360, -360,  346, -319,  280, -230,  171, -105,   35 },
    {  18,  -53,   88, -122,  155, -186,  216, -243,  268, -291,  311, -327,  341, -351,  358, -362,  362, -358,  351, -341,  327, -311,  291, -268,  243, -216,  186, -155,  122,  -88,   53,  -18 }
};

DECLARE_ALIGNED(16, static const int16_t, DCT_V_4x4[4][4]) =
{
    {194,  274,  274,  274},
    {274,  241,  -86, -349},
    {274,  -86, -349,  241},
    {274, -349,  241,  -86}
};

DECLARE_ALIGNED(16, static const int16_t, DCT_V_8x8[8][8]) =
{
    {187,  264,  264,  264,  264,  264,  264,  264},
    {264,  342,  250,  116,  -39, -187, -303, -366},
    {264,  250,  -39, -303, -366, -187,  116,  342},
    {264,  116, -303, -303,  116,  374,  116, -303},
    {264,  -39, -366,  116,  342, -187, -303,  250},
    {264, -187, -187,  374, -187, -187,  374, -187},
    {264, -303,  116,  116, -303,  374, -303,  116},
    {264, -366,  342, -303,  250, -187,  116,  -39}
};

DECLARE_ALIGNED(16, static const int16_t, DCT_V_16x16[16][16]) =
{
    {184,  260,  260,  260,  260,  260,  260,  260,  260,  260,  260,  260,  260,  260,  260,  260},
    {260,  360,  338,  302,  253,  195,  128,   56,  -19,  -92, -162, -225, -279, -322, -351, -366},
    {260,  338,  253,  128,  -19, -162, -279, -351, -366, -322, -225,  -92,   56,  195,  302,  360},
    {260,  302,  128,  -92, -279, -366, -322, -162,   56,  253,  360,  338,  195,  -19, -225, -351},
    {260,  253,  -19, -279, -366, -225,   56,  302,  360,  195,  -92, -322, -351, -162,  128,  338},
    {260,  195, -162, -366, -225,  128,  360,  253,  -92, -351, -279,   56,  338,  302,  -19, -322},
    {260,  128, -279, -322,   56,  360,  195, -225, -351,  -19,  338,  253, -162, -366,  -92,  302},
    {260,   56, -351, -162,  302,  253, -225, -322,  128,  360,  -19, -366,  -92,  338,  195, -279},
    {260,  -19, -366,   56,  360,  -92, -351,  128,  338, -162, -322,  195,  302, -225, -279,  253},
    {260,  -92, -322,  253,  195, -351,  -19,  360, -162, -279,  302,  128, -366,   56,  338, -225},
    {260, -162, -225,  360,  -92, -279,  338,  -19, -322,  302,   56, -351,  253,  128, -366,  195},
    {260, -225,  -92,  338, -322,   56,  253, -366,  195,  128, -351,  302,  -19, -279,  360, -162},
    {260, -279,   56,  195, -351,  338, -162,  -92,  302, -366,  253,  -19, -225,  360, -322,  128},
    {260, -322,  195,  -19, -162,  302, -366,  338, -225,   56,  128, -279,  360, -351,  253,  -92},
    {260, -351,  302, -225,  128,  -19,  -92,  195, -279,  338, -366,  360, -322,  253, -162,   56},
    {260, -366,  360, -351,  338, -322,  302, -279,  253, -225,  195, -162,  128,  -92,   56,  -19}
};

DECLARE_ALIGNED(16, static const int16_t, DCT_V_32x32[32][32]) =
{
    {182,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258,  258},
    {258,  363,  358,  349,  336,  320,  301,  280,  255,  228,  198,  166,  133,   99,   63,   27,   -9,  -45,  -81, -116, -150, -182, -213, -241, -267, -291, -311, -329, -343, -354, -361, -364},
    {258,  358,  336,  301,  255,  198,  133,   63,   -9,  -81, -150, -213, -267, -311, -343, -361, -364, -354, -329, -291, -241, -182, -116,  -45,   27,   99,  166,  228,  280,  320,  349,  363},
    {258,  349,  301,  228,  133,   27,  -81, -182, -267, -329, -361, -361, -329, -267, -182,  -81,   27,  133,  228,  301,  349,  365,  349,  301,  228,  133,   27,  -81, -182, -267, -329, -361},
    {258,  336,  255,  133,   -9, -150, -267, -343, -364, -329, -241, -116,   27,  166,  280,  349,  363,  320,  228,   99,  -45, -182, -291, -354, -361, -311, -213,  -81,   63,  198,  301,  358},
    {258,  320,  198,   27, -150, -291, -361, -343, -241,  -81,   99,  255,  349,  358,  280,  133,  -45, -213, -329, -364, -311, -182,   -9,  166,  301,  363,  336,  228,   63, -116, -267, -354},
    {258,  301,  133,  -81, -267, -361, -329, -182,   27,  228,  349,  349,  228,   27, -182, -329, -361, -267,  -81,  133,  301,  365,  301,  133,  -81, -267, -361, -329, -182,   27,  228,  349},
    {258,  280,   63, -182, -343, -343, -182,   63,  280,  365,  280,   63, -182, -343, -343, -182,   63,  280,  365,  280,   63, -182, -343, -343, -182,   63,  280,  365,  280,   63, -182, -343},
    {258,  255,   -9, -267, -364, -241,   27,  280,  363,  228,  -45, -291, -361, -213,   63,  301,  358,  198,  -81, -311, -354, -182,   99,  320,  349,  166, -116, -329, -343, -150,  133,  336},
    {258,  228,  -81, -329, -329,  -81,  228,  365,  228,  -81, -329, -329,  -81,  228,  365,  228,  -81, -329, -329,  -81,  228,  365,  228,  -81, -329, -329,  -81,  228,  365,  228,  -81, -329},
    {258,  198, -150, -361, -241,   99,  349,  280,  -45, -329, -311,   -9,  301,  336,   63, -267, -354, -116,  228,  363,  166, -182, -364, -213,  133,  358,  255,  -81, -343, -291,   27,  320},
    {258,  166, -213, -361, -116,  255,  349,   63, -291, -329,   -9,  320,  301,  -45, -343, -267,   99,  358,  228, -150, -364, -182,  198,  363,  133, -241, -354,  -81,  280,  336,   27, -311},
    {258,  133, -267, -329,   27,  349,  228, -182, -361,  -81,  301,  301,  -81, -361, -182,  228,  349,   27, -329, -267,  133,  365,  133, -267, -329,   27,  349,  228, -182, -361,  -81,  301},
    {258,   99, -311, -267,  166,  358,   27, -343, -213,  228,  336,  -45, -361, -150,  280,  301, -116, -364,  -81,  320,  255, -182, -354,   -9,  349,  198, -241, -329,   63,  363,  133, -291},
    {258,   63, -343, -182,  280,  280, -182, -343,   63,  365,   63, -343, -182,  280,  280, -182, -343,   63,  365,   63, -343, -182,  280,  280, -182, -343,   63,  365,   63, -343, -182,  280},
    {258,   27, -361,  -81,  349,  133, -329, -182,  301,  228, -267, -267,  228,  301, -182, -329,  133,  349,  -81, -361,   27,  365,   27, -361,  -81,  349,  133, -329, -182,  301,  228, -267},
    {258,   -9, -364,   27,  363,  -45, -361,   63,  358,  -81, -354,   99,  349, -116, -343,  133,  336, -150, -329,  166,  320, -182, -311,  198,  301, -213, -291,  228,  280, -241, -267,  255},
    {258,  -45, -354,  133,  320, -213, -267,  280,  198, -329, -116,  358,   27, -364,   63,  349, -150, -311,  228,  255, -291, -182,  336,   99, -361,   -9,  363,  -81, -343,  166,  301, -241},
    {258,  -81, -329,  228,  228, -329,  -81,  365,  -81, -329,  228,  228, -329,  -81,  365,  -81, -329,  228,  228, -329,  -81,  365,  -81, -329,  228,  228, -329,  -81,  365,  -81, -329,  228},
    {258, -116, -291,  301,   99, -364,  133,  280, -311,  -81,  363, -150, -267,  320,   63, -361,  166,  255, -329,  -45,  358, -182, -241,  336,   27, -354,  198,  228, -343,   -9,  349, -213},
    {258, -150, -241,  349,  -45, -311,  301,   63, -354,  228,  166, -364,  133,  255, -343,   27,  320, -291,  -81,  358, -213, -182,  363, -116, -267,  336,   -9, -329,  280,   99, -361,  198},
    {258, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182, -182,  365, -182},
    {258, -213, -116,  349, -291,   -9,  301, -343,   99,  228, -364,  198,  133, -354,  280,   27, -311,  336,  -81, -241,  363, -182, -150,  358, -267,  -45,  320, -329,   63,  255, -361,  166},
    {258, -241,  -45,  301, -354,  166,  133, -343,  320,  -81, -213,  363, -267,   -9,  280, -361,  198,   99, -329,  336, -116, -182,  358, -291,   27,  255, -364,  228,   63, -311,  349, -150},
    {258, -267,   27,  228, -361,  301,  -81, -182,  349, -329,  133,  133, -329,  349, -182,  -81,  301, -361,  228,   27, -267,  365, -267,   27,  228, -361,  301,  -81, -182,  349, -329,  133},
    {258, -291,   99,  133, -311,  363, -267,   63,  166, -329,  358, -241,   27,  198, -343,  349, -213,   -9,  228, -354,  336, -182,  -45,  255, -361,  320, -150,  -81,  280, -364,  301, -116},
    {258, -311,  166,   27, -213,  336, -361,  280, -116,  -81,  255, -354,  349, -241,   63,  133, -291,  363, -329,  198,   -9, -182,  320, -364,  301, -150,  -45,  228, -343,  358, -267,   99},
    {258, -329,  228,  -81,  -81,  228, -329,  365, -329,  228,  -81,  -81,  228, -329,  365, -329,  228,  -81,  -81,  228, -329,  365, -329,  228,  -81,  -81,  228, -329,  365, -329,  228,  -81},
    {258, -343,  280, -182,   63,   63, -182,  280, -343,  365, -343,  280, -182,   63,   63, -182,  280, -343,  365, -343,  280, -182,   63,   63, -182,  280, -343,  365, -343,  280, -182,   63},
    {258, -354,  320, -267,  198, -116,   27,   63, -150,  228, -291,  336, -361,  363, -343,  301, -241,  166,  -81,   -9,   99, -182,  255, -311,  349, -364,  358, -329,  280, -213,  133,  -45},
    {258, -361,  349, -329,  301, -267,  228, -182,  133,  -81,   27,   27,  -81,  133, -182,  228, -267,  301, -329,  349, -361,  365, -361,  349, -329,  301, -267,  228, -182,  133,  -81,   27},
    {258, -364,  363, -361,  358, -354,  349, -343,  336, -329,  320, -311,  301, -291,  280, -267,  255, -241,  228, -213,  198, -182,  166, -150,  133, -116,   99,  -81,   63,  -45,   27,   -9}
};

DECLARE_ALIGNED(16, static const int16_t, DCT_VIII_4x4[4][4]) =
{
    {  336,  296,  219,  117 },
    {  296,    0, -296, -296 },
    {  219, -296, -117,  336 },
    {  117, -296,  336, -219 }
};

DECLARE_ALIGNED(16, static const int16_t, DCT_VIII_8x8[8][8]) =
{
    {  350,  338,  314,  280,  237,  185,  127,   65 },
    {  338,  237,   65, -127, -280, -350, -314, -185 },
    {  314,   65, -237, -350, -185,  127,  338,  280 },
    {  280, -127, -350,  -65,  314,  237, -185, -338 },
    {  237, -280, -185,  314,  127, -338,  -65,  350 },
    {  185, -350,  127,  237, -338,   65,  280, -314 },
    {  127, -314,  338, -185,  -65,  280, -350,  237 },
    {   65, -185,  280, -338,  350, -314,  237, -127 }
};

DECLARE_ALIGNED(16, static const int16_t, DCT_VIII_16x16[16][16]) =
{
    {  356,  353,  346,  337,  324,  309,  290,  269,  246,  220,  193,  163,  133,  100,   67,   34 },
    {  353,  324,  269,  193,  100,    0, -100, -193, -269, -324, -353, -353, -324, -269, -193, -100 },
    {  346,  269,  133,  -34, -193, -309, -356, -324, -220,  -67,  100,  246,  337,  353,  290,  163 },
    {  337,  193,  -34, -246, -353, -309, -133,  100,  290,  356,  269,   67, -163, -324, -346, -220 },
    {  324,  100, -193, -353, -269,    0,  269,  353,  193, -100, -324, -324, -100,  193,  353,  269 },
    {  309,    0, -309, -309,    0,  309,  309,    0, -309, -309,    0,  309,  309,    0, -309, -309 },
    {  290, -100, -356, -133,  269,  309,  -67, -353, -163,  246,  324,  -34, -346, -193,  220,  337 },
    {  269, -193, -324,  100,  353,    0, -353, -100,  324,  193, -269, -269,  193,  324, -100, -353 },
    {  246, -269, -220,  290,  193, -309, -163,  324,  133, -337, -100,  346,   67, -353,  -34,  356 },
    {  220, -324,  -67,  356, -100, -309,  246,  193, -337,  -34,  353, -133, -290,  269,  163, -346 },
    {  193, -353,  100,  269, -324,    0,  324, -269, -100,  353, -193, -193,  353, -100, -269,  324 },
    {  163, -353,  246,   67, -324,  309,  -34, -269,  346, -133, -193,  356, -220, -100,  337, -290 },
    {  133, -324,  337, -163, -100,  309, -346,  193,   67, -290,  353, -220,  -34,  269, -356,  246 },
    {  100, -269,  353, -324,  193,    0, -193,  324, -353,  269, -100, -100,  269, -353,  324, -193 },
    {   67, -193,  290, -346,  353, -309,  220, -100,  -34,  163, -269,  337, -356,  324, -246,  133 },
    {   34, -100,  163, -220,  269, -309,  337, -353,  356, -346,  324, -290,  246, -193,  133,  -67 }
};

DECLARE_ALIGNED(16, static const int16_t, DCT_VIII_32x32[32][32]) =
{
    {  359,  358,  357,  354,  351,  347,  342,  336,  329,  322,  314,  305,  296,  285,  275,  263,  251,  238,  225,  211,  197,  182,  167,  151,  135,  119,  103,   86,   69,   52,   35,   17 },
    {  358,  351,  336,  314,  285,  251,  211,  167,  119,   69,   17,  -35,  -86, -135, -182, -225, -263, -296, -322, -342, -354, -359, -357, -347, -329, -305, -275, -238, -197, -151, -103,  -52 },
    {  357,  336,  296,  238,  167,   86,    0,  -86, -167, -238, -296, -336, -357, -357, -336, -296, -238, -167,  -86,    0,   86,  167,  238,  296,  336,  357,  357,  336,  296,  238,  167,   86 },
    {  354,  314,  238,  135,   17, -103, -211, -296, -347, -358, -329, -263, -167,  -52,   69,  182,  275,  336,  359,  342,  285,  197,   86,  -35, -151, -251, -322, -357, -351, -305, -225, -119 },
    {  351,  285,  167,   17, -135, -263, -342, -357, -305, -197,  -52,  103,  238,  329,  359,  322,  225,   86,  -69, -211, -314, -358, -336, -251, -119,   35,  182,  296,  354,  347,  275,  151 },
    {  347,  251,   86, -103, -263, -351, -342, -238,  -69,  119,  275,  354,  336,  225,   52, -135, -285, -357, -329, -211,  -35,  151,  296,  358,  322,  197,   17, -167, -305, -359, -314, -182 },
    {  342,  211,    0, -211, -342, -342, -211,    0,  211,  342,  342,  211,    0, -211, -342, -342, -211,    0,  211,  342,  342,  211,    0, -211, -342, -342, -211,    0,  211,  342,  342,  211 },
    {  336,  167,  -86, -296, -357, -238,    0,  238,  357,  296,   86, -167, -336, -336, -167,   86,  296,  357,  238,    0, -238, -357, -296,  -86,  167,  336,  336,  167,  -86, -296, -357, -238 },
    {  329,  119, -167, -347, -305,  -69,  211,  357,  275,   17, -251, -359, -238,   35,  285,  354,  197,  -86, -314, -342, -151,  135,  336,  322,  103, -182, -351, -296,  -52,  225,  358,  263 },
    {  322,   69, -238, -358, -197,  119,  342,  296,   17, -275, -351, -151,  167,  354,  263,  -35, -305, -336, -103,  211,  359,  225,  -86, -329, -314,  -52,  251,  357,  182, -135, -347, -285 },
    {  314,   17, -296, -329,  -52,  275,  342,   86, -251, -351, -119,  225,  357,  151, -197, -359, -182,  167,  358,  211, -135, -354, -238,  103,  347,  263,  -69, -336, -285,   35,  322,  305 },
    {  305,  -35, -336, -263,  103,  354,  211, -167, -359, -151,  225,  351,   86, -275, -329,  -17,  314,  296,  -52, -342, -251,  119,  357,  197, -182, -358, -135,  238,  347,   69, -285, -322 },
    {  296,  -86, -357, -167,  238,  336,    0, -336, -238,  167,  357,   86, -296, -296,   86,  357,  167, -238, -336,    0,  336,  238, -167, -357,  -86,  296,  296,  -86, -357, -167,  238,  336 },
    {  285, -135, -357,  -52,  329,  225, -211, -336,   35,  354,  151, -275, -296,  119,  358,   69, -322, -238,  197,  342,  -17, -351, -167,  263,  305, -103, -359,  -86,  314,  251, -182, -347 },
    {  275, -182, -336,   69,  359,   52, -342, -167,  285,  263, -197, -329,   86,  358,   35, -347, -151,  296,  251, -211, -322,  103,  357,   17, -351, -135,  305,  238, -225, -314,  119,  354 },
    {  263, -225, -296,  182,  322, -135, -342,   86,  354,  -35, -359,  -17,  357,   69, -347, -119,  329,  167, -305, -211,  275,  251, -238, -285,  197,  314, -151, -336,  103,  351,  -52, -358 },
    {  251, -263, -238,  275,  225, -285, -211,  296,  197, -305, -182,  314,  167, -322, -151,  329,  135, -336, -119,  342,  103, -347,  -86,  351,   69, -354,  -52,  357,   35, -358,  -17,  359 },
    {  238, -296, -167,  336,   86, -357,    0,  357,  -86, -336,  167,  296, -238, -238,  296,  167, -336,  -86,  357,    0, -357,   86,  336, -167, -296,  238,  238, -296, -167,  336,   86, -357 },
    {  225, -322,  -86,  359,  -69, -329,  211,  238, -314, -103,  358,  -52, -336,  197,  251, -305, -119,  357,  -35, -342,  182,  263, -296, -135,  354,  -17, -347,  167,  275, -285, -151,  351 },
    {  211, -342,    0,  342, -211, -211,  342,    0, -342,  211,  211, -342,    0,  342, -211, -211,  342,    0, -342,  211,  211, -342,    0,  342, -211, -211,  342,    0, -342,  211,  211, -342 },
    {  197, -354,   86,  285, -314,  -35,  342, -238, -151,  359, -135, -251,  336,  -17, -322,  275,  103, -357,  182,  211, -351,   69,  296, -305,  -52,  347, -225, -167,  358, -119, -263,  329 },
    {  182, -359,  167,  197, -358,  151,  211, -357,  135,  225, -354,  119,  238, -351,  103,  251, -347,   86,  263, -342,   69,  275, -336,   52,  285, -329,   35,  296, -322,   17,  305, -314 },
    {  167, -357,  238,   86, -336,  296,    0, -296,  336,  -86, -238,  357, -167, -167,  357, -238,  -86,  336, -296,    0,  296, -336,   86,  238, -357,  167,  167, -357,  238,   86, -336,  296 },
    {  151, -347,  296,  -35, -251,  358, -211,  -86,  322, -329,  103,  197, -357,  263,   17, -285,  351, -167, -135,  342, -305,   52,  238, -359,  225,   69, -314,  336, -119, -182,  354, -275 },
    {  135, -329,  336, -151, -119,  322, -342,  167,  103, -314,  347, -182,  -86,  305, -351,  197,   69, -296,  354, -211,  -52,  285, -357,  225,   35, -275,  358, -238,  -17,  263, -359,  251 },
    {  119, -305,  357, -251,   35,  197, -342,  336, -182,  -52,  263, -358,  296, -103, -135,  314, -354,  238,  -17, -211,  347, -329,  167,   69, -275,  359, -285,   86,  151, -322,  351, -225 },
    {  103, -275,  357, -322,  182,   17, -211,  336, -351,  251,  -69, -135,  296, -359,  305, -151,  -52,  238, -347,  342, -225,   35,  167, -314,  358, -285,  119,   86, -263,  354, -329,  197 },
    {   86, -238,  336, -357,  296, -167,    0,  167, -296,  357, -336,  238,  -86,  -86,  238, -336,  357, -296,  167,    0, -167,  296, -357,  336, -238,   86,   86, -238,  336, -357,  296, -167 },
    {   69, -197,  296, -351,  354, -305,  211,  -86,  -52,  182, -285,  347, -357,  314, -225,  103,   35, -167,  275, -342,  358, -322,  238, -119,  -17,  151, -263,  336, -359,  329, -251,  135 },
    {   52, -151,  238, -305,  347, -359,  342, -296,  225, -135,   35,   69, -167,  251, -314,  351, -358,  336, -285,  211, -119,   17,   86, -182,  263, -322,  354, -357,  329, -275,  197, -103 },
    {   35, -103,  167, -225,  275, -314,  342, -357,  358, -347,  322, -285,  238, -182,  119,  -52,  -17,   86, -151,  211, -263,  305, -336,  354, -359,  351, -329,  296, -251,  197, -135,   69 },
    {   17,  -52,   86, -119,  151, -182,  211, -238,  263, -285,  305, -322,  336, -347,  354, -358,  359, -357,  351, -342,  329, -314,  296, -275,  251, -225,  197, -167,  135, -103,   69,  -35 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_I_4x4[4][4]) =
{
    {  190,  308,  308,  190 },
    {  308,  190, -190, -308 },
    {  308, -190, -190,  308 },
    {  190, -308,  308, -190 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_I_8x8[8][8]) =
{
    {  117,  219,  296,  336,  336,  296,  219,  117 },
    {  219,  336,  296,  117, -117, -296, -336, -219 },
    {  296,  296,    0, -296, -296,    0,  296,  296 },
    {  336,  117, -296, -219,  219,  296, -117, -336 },
    {  336, -117, -296,  219,  219, -296, -117,  336 },
    {  296, -296,    0,  296, -296,    0,  296, -296 },
    {  219, -336,  296, -117, -117,  296, -336,  219 },
    {  117, -219,  296, -336,  336, -296,  219, -117 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_I_16x16[16][16]) =
{
    {   65,  127,  185,  237,  280,  314,  338,  350,  350,  338,  314,  280,  237,  185,  127,   65 },
    {  127,  237,  314,  350,  338,  280,  185,   65,  -65, -185, -280, -338, -350, -314, -237, -127 },
    {  185,  314,  350,  280,  127,  -65, -237, -338, -338, -237,  -65,  127,  280,  350,  314,  185 },
    {  237,  350,  280,   65, -185, -338, -314, -127,  127,  314,  338,  185,  -65, -280, -350, -237 },
    {  280,  338,  127, -185, -350, -237,   65,  314,  314,   65, -237, -350, -185,  127,  338,  280 },
    {  314,  280,  -65, -338, -237,  127,  350,  185, -185, -350, -127,  237,  338,   65, -280, -314 },
    {  338,  185, -237, -314,   65,  350,  127, -280, -280,  127,  350,   65, -314, -237,  185,  338 },
    {  350,   65, -338, -127,  314,  185, -280, -237,  237,  280, -185, -314,  127,  338,  -65, -350 },
    {  350,  -65, -338,  127,  314, -185, -280,  237,  237, -280, -185,  314,  127, -338,  -65,  350 },
    {  338, -185, -237,  314,   65, -350,  127,  280, -280, -127,  350,  -65, -314,  237,  185, -338 },
    {  314, -280,  -65,  338, -237, -127,  350, -185, -185,  350, -127, -237,  338,  -65, -280,  314 },
    {  280, -338,  127,  185, -350,  237,   65, -314,  314,  -65, -237,  350, -185, -127,  338, -280 },
    {  237, -350,  280,  -65, -185,  338, -314,  127,  127, -314,  338, -185,  -65,  280, -350,  237 },
    {  185, -314,  350, -280,  127,   65, -237,  338, -338,  237,  -65, -127,  280, -350,  314, -185 },
    {  127, -237,  314, -350,  338, -280,  185,  -65,  -65,  185, -280,  338, -350,  314, -237,  127 },
    {   65, -127,  185, -237,  280, -314,  338, -350,  350, -338,  314, -280,  237, -185,  127,  -65 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_I_32x32[32][32]) =
{
    {   34,   67,  100,  133,  163,  193,  220,  246,  269,  290,  309,  324,  337,  346,  353,  356,  356,  353,  346,  337,  324,  309,  290,  269,  246,  220,  193,  163,  133,  100,   67,   34 },
    {   67,  133,  193,  246,  290,  324,  346,  356,  353,  337,  309,  269,  220,  163,  100,   34,  -34, -100, -163, -220, -269, -309, -337, -353, -356, -346, -324, -290, -246, -193, -133,  -67 },
    {  100,  193,  269,  324,  353,  353,  324,  269,  193,  100,    0, -100, -193, -269, -324, -353, -353, -324, -269, -193, -100,    0,  100,  193,  269,  324,  353,  353,  324,  269,  193,  100 },
    {  133,  246,  324,  356,  337,  269,  163,   34, -100, -220, -309, -353, -346, -290, -193,  -67,   67,  193,  290,  346,  353,  309,  220,  100,  -34, -163, -269, -337, -356, -324, -246, -133 },
    {  163,  290,  353,  337,  246,  100,  -67, -220, -324, -356, -309, -193,  -34,  133,  269,  346,  346,  269,  133,  -34, -193, -309, -356, -324, -220,  -67,  100,  246,  337,  353,  290,  163 },
    {  193,  324,  353,  269,  100, -100, -269, -353, -324, -193,    0,  193,  324,  353,  269,  100, -100, -269, -353, -324, -193,    0,  193,  324,  353,  269,  100, -100, -269, -353, -324, -193 },
    {  220,  346,  324,  163,  -67, -269, -356, -290, -100,  133,  309,  353,  246,   34, -193, -337, -337, -193,   34,  246,  353,  309,  133, -100, -290, -356, -269,  -67,  163,  324,  346,  220 },
    {  246,  356,  269,   34, -220, -353, -290,  -67,  193,  346,  309,  100, -163, -337, -324, -133,  133,  324,  337,  163, -100, -309, -346, -193,   67,  290,  353,  220,  -34, -269, -356, -246 },
    {  269,  353,  193, -100, -324, -324, -100,  193,  353,  269,    0, -269, -353, -193,  100,  324,  324,  100, -193, -353, -269,    0,  269,  353,  193, -100, -324, -324, -100,  193,  353,  269 },
    {  290,  337,  100, -220, -356, -193,  133,  346,  269,  -34, -309, -324,  -67,  246,  353,  163, -163, -353, -246,   67,  324,  309,   34, -269, -346, -133,  193,  356,  220, -100, -337, -290 },
    {  309,  309,    0, -309, -309,    0,  309,  309,    0, -309, -309,    0,  309,  309,    0, -309, -309,    0,  309,  309,    0, -309, -309,    0,  309,  309,    0, -309, -309,    0,  309,  309 },
    {  324,  269, -100, -353, -193,  193,  353,  100, -269, -324,    0,  324,  269, -100, -353, -193,  193,  353,  100, -269, -324,    0,  324,  269, -100, -353, -193,  193,  353,  100, -269, -324 },
    {  337,  220, -193, -346,  -34,  324,  246, -163, -353,  -67,  309,  269, -133, -356, -100,  290,  290, -100, -356, -133,  269,  309,  -67, -353, -163,  246,  324,  -34, -346, -193,  220,  337 },
    {  346,  163, -269, -290,  133,  353,   34, -337, -193,  246,  309, -100, -356,  -67,  324,  220, -220, -324,   67,  356,  100, -309, -246,  193,  337,  -34, -353, -133,  290,  269, -163, -346 },
    {  353,  100, -324, -193,  269,  269, -193, -324,  100,  353,    0, -353, -100,  324,  193, -269, -269,  193,  324, -100, -353,    0,  353,  100, -324, -193,  269,  269, -193, -324,  100,  353 },
    {  356,   34, -353,  -67,  346,  100, -337, -133,  324,  163, -309, -193,  290,  220, -269, -246,  246,  269, -220, -290,  193,  309, -163, -324,  133,  337, -100, -346,   67,  353,  -34, -356 },
    {  356,  -34, -353,   67,  346, -100, -337,  133,  324, -163, -309,  193,  290, -220, -269,  246,  246, -269, -220,  290,  193, -309, -163,  324,  133, -337, -100,  346,   67, -353,  -34,  356 },
    {  353, -100, -324,  193,  269, -269, -193,  324,  100, -353,    0,  353, -100, -324,  193,  269, -269, -193,  324,  100, -353,    0,  353, -100, -324,  193,  269, -269, -193,  324,  100, -353 },
    {  346, -163, -269,  290,  133, -353,   34,  337, -193, -246,  309,  100, -356,   67,  324, -220, -220,  324,   67, -356,  100,  309, -246, -193,  337,   34, -353,  133,  290, -269, -163,  346 },
    {  337, -220, -193,  346,  -34, -324,  246,  163, -353,   67,  309, -269, -133,  356, -100, -290,  290,  100, -356,  133,  269, -309,  -67,  353, -163, -246,  324,   34, -346,  193,  220, -337 },
    {  324, -269, -100,  353, -193, -193,  353, -100, -269,  324,    0, -324,  269,  100, -353,  193,  193, -353,  100,  269, -324,    0,  324, -269, -100,  353, -193, -193,  353, -100, -269,  324 },
    {  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309,    0,  309, -309 },
    {  290, -337,  100,  220, -356,  193,  133, -346,  269,   34, -309,  324,  -67, -246,  353, -163, -163,  353, -246,  -67,  324, -309,   34,  269, -346,  133,  193, -356,  220,  100, -337,  290 },
    {  269, -353,  193,  100, -324,  324, -100, -193,  353, -269,    0,  269, -353,  193,  100, -324,  324, -100, -193,  353, -269,    0,  269, -353,  193,  100, -324,  324, -100, -193,  353, -269 },
    {  246, -356,  269,  -34, -220,  353, -290,   67,  193, -346,  309, -100, -163,  337, -324,  133,  133, -324,  337, -163, -100,  309, -346,  193,   67, -290,  353, -220,  -34,  269, -356,  246 },
    {  220, -346,  324, -163,  -67,  269, -356,  290, -100, -133,  309, -353,  246,  -34, -193,  337, -337,  193,   34, -246,  353, -309,  133,  100, -290,  356, -269,   67,  163, -324,  346, -220 },
    {  193, -324,  353, -269,  100,  100, -269,  353, -324,  193,    0, -193,  324, -353,  269, -100, -100,  269, -353,  324, -193,    0,  193, -324,  353, -269,  100,  100, -269,  353, -324,  193 },
    {  163, -290,  353, -337,  246, -100,  -67,  220, -324,  356, -309,  193,  -34, -133,  269, -346,  346, -269,  133,   34, -193,  309, -356,  324, -220,   67,  100, -246,  337, -353,  290, -163 },
    {  133, -246,  324, -356,  337, -269,  163,  -34, -100,  220, -309,  353, -346,  290, -193,   67,   67, -193,  290, -346,  353, -309,  220, -100,  -34,  163, -269,  337, -356,  324, -246,  133 },
    {  100, -193,  269, -324,  353, -353,  324, -269,  193, -100,    0,  100, -193,  269, -324,  353, -353,  324, -269,  193, -100,    0,  100, -193,  269, -324,  353, -353,  324, -269,  193, -100 },
    {   67, -133,  193, -246,  290, -324,  346, -356,  353, -337,  309, -269,  220, -163,  100,  -34,  -34,  100, -163,  220, -269,  309, -337,  353, -356,  346, -324,  290, -246,  193, -133,   67 },
    {   34,  -67,  100, -133,  163, -193,  220, -246,  269, -290,  309, -324,  337, -346,  353, -356,  356, -353,  346, -337,  324, -309,  290, -269,  246, -220,  193, -163,  133, -100,   67,  -34 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_VII_4x4[4][4]) =
{
    {  117,  219,  296,  336 },
    {  296,  296,    0, -296 },
    {  336, -117, -296,  219 },
    {  219, -336,  296, -117 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_VII_8x8[8][8]) =
{
    {   65,  127,  185,  237,  280,  314,  338,  350 },
    {  185,  314,  350,  280,  127,  -65, -237, -338 },
    {  280,  338,  127, -185, -350, -237,   65,  314 },
    {  338,  185, -237, -314,   65,  350,  127, -280 },
    {  350,  -65, -338,  127,  314, -185, -280,  237 },
    {  314, -280,  -65,  338, -237, -127,  350, -185 },
    {  237, -350,  280,  -65, -185,  338, -314,  127 },
    {  127, -237,  314, -350,  338, -280,  185,  -65 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_VII_16x16[16][16]) =
{
    {   34,   67,  100,  133,  163,  193,  220,  246,  269,  290,  309,  324,  337,  346,  353,  356 },
    {  100,  193,  269,  324,  353,  353,  324,  269,  193,  100,    0, -100, -193, -269, -324, -353 },
    {  163,  290,  353,  337,  246,  100,  -67, -220, -324, -356, -309, -193,  -34,  133,  269,  346 },
    {  220,  346,  324,  163,  -67, -269, -356, -290, -100,  133,  309,  353,  246,   34, -193, -337 },
    {  269,  353,  193, -100, -324, -324, -100,  193,  353,  269,    0, -269, -353, -193,  100,  324 },
    {  309,  309,    0, -309, -309,    0,  309,  309,    0, -309, -309,    0,  309,  309,    0, -309 },
    {  337,  220, -193, -346,  -34,  324,  246, -163, -353,  -67,  309,  269, -133, -356, -100,  290 },
    {  353,  100, -324, -193,  269,  269, -193, -324,  100,  353,    0, -353, -100,  324,  193, -269 },
    {  356,  -34, -353,   67,  346, -100, -337,  133,  324, -163, -309,  193,  290, -220, -269,  246 },
    {  346, -163, -269,  290,  133, -353,   34,  337, -193, -246,  309,  100, -356,   67,  324, -220 },
    {  324, -269, -100,  353, -193, -193,  353, -100, -269,  324,    0, -324,  269,  100, -353,  193 },
    {  290, -337,  100,  220, -356,  193,  133, -346,  269,   34, -309,  324,  -67, -246,  353, -163 },
    {  246, -356,  269,  -34, -220,  353, -290,   67,  193, -346,  309, -100, -163,  337, -324,  133 },
    {  193, -324,  353, -269,  100,  100, -269,  353, -324,  193,    0, -193,  324, -353,  269, -100 },
    {  133, -246,  324, -356,  337, -269,  163,  -34, -100,  220, -309,  353, -346,  290, -193,   67 },
    {   67, -133,  193, -246,  290, -324,  346, -356,  353, -337,  309, -269,  220, -163,  100,  -34 }
};

DECLARE_ALIGNED(16, static const int16_t, DST_VII_32x32[32][32]) =
{
    {   17,   35,   52,   69,   86,  103,  119,  135,  151,  167,  182,  197,  211,  225,  238,  251,  263,  275,  285,  296,  305,  314,  322,  329,  336,  342,  347,  351,  354,  357,  358,  359 },
    {   52,  103,  151,  197,  238,  275,  305,  329,  347,  357,  359,  354,  342,  322,  296,  263,  225,  182,  135,   86,   35,  -17,  -69, -119, -167, -211, -251, -285, -314, -336, -351, -358 },
    {   86,  167,  238,  296,  336,  357,  357,  336,  296,  238,  167,   86,    0,  -86, -167, -238, -296, -336, -357, -357, -336, -296, -238, -167,  -86,    0,   86,  167,  238,  296,  336,  357 },
    {  119,  225,  305,  351,  357,  322,  251,  151,   35,  -86, -197, -285, -342, -359, -336, -275, -182,  -69,   52,  167,  263,  329,  358,  347,  296,  211,  103,  -17, -135, -238, -314, -354 },
    {  151,  275,  347,  354,  296,  182,   35, -119, -251, -336, -358, -314, -211,  -69,   86,  225,  322,  359,  329,  238,  103,  -52, -197, -305, -357, -342, -263, -135,   17,  167,  285,  351 },
    {  182,  314,  359,  305,  167,  -17, -197, -322, -358, -296, -151,   35,  211,  329,  357,  285,  135,  -52, -225, -336, -354, -275, -119,   69,  238,  342,  351,  263,  103,  -86, -251, -347 },
    {  211,  342,  342,  211,    0, -211, -342, -342, -211,    0,  211,  342,  342,  211,    0, -211, -342, -342, -211,    0,  211,  342,  342,  211,    0, -211, -342, -342, -211,    0,  211,  342 },
    {  238,  357,  296,   86, -167, -336, -336, -167,   86,  296,  357,  238,    0, -238, -357, -296,  -86,  167,  336,  336,  167,  -86, -296, -357, -238,    0,  238,  357,  296,   86, -167, -336 },
    {  263,  358,  225,  -52, -296, -351, -182,  103,  322,  336,  135, -151, -342, -314,  -86,  197,  354,  285,   35, -238, -359, -251,   17,  275,  357,  211,  -69, -305, -347, -167,  119,  329 },
    {  285,  347,  135, -182, -357, -251,   52,  314,  329,   86, -225, -359, -211,  103,  336,  305,   35, -263, -354, -167,  151,  351,  275,  -17, -296, -342, -119,  197,  358,  238,  -69, -322 },
    {  305,  322,   35, -285, -336,  -69,  263,  347,  103, -238, -354, -135,  211,  358,  167, -182, -359, -197,  151,  357,  225, -119, -351, -251,   86,  342,  275,  -52, -329, -296,   17,  314 },
    {  322,  285,  -69, -347, -238,  135,  358,  182, -197, -357, -119,  251,  342,   52, -296, -314,   17,  329,  275,  -86, -351, -225,  151,  359,  167, -211, -354, -103,  263,  336,   35, -305 },
    {  336,  238, -167, -357,  -86,  296,  296,  -86, -357, -167,  238,  336,    0, -336, -238,  167,  357,   86, -296, -296,   86,  357,  167, -238, -336,    0,  336,  238, -167, -357,  -86,  296 },
    {  347,  182, -251, -314,   86,  359,  103, -305, -263,  167,  351,   17, -342, -197,  238,  322,  -69, -358, -119,  296,  275, -151, -354,  -35,  336,  211, -225, -329,   52,  357,  135, -285 },
    {  354,  119, -314, -225,  238,  305, -135, -351,   17,  357,  103, -322, -211,  251,  296, -151, -347,   35,  358,   86, -329, -197,  263,  285, -167, -342,   52,  359,   69, -336, -182,  275 },
    {  358,   52, -351, -103,  336,  151, -314, -197,  285,  238, -251, -275,  211,  305, -167, -329,  119,  347,  -69, -357,   17,  359,   35, -354,  -86,  342,  135, -322, -182,  296,  225, -263 },
    {  359,  -17, -358,   35,  357,  -52, -354,   69,  351,  -86, -347,  103,  342, -119, -336,  135,  329, -151, -322,  167,  314, -182, -305,  197,  296, -211, -285,  225,  275, -238, -263,  251 },
    {  357,  -86, -336,  167,  296, -238, -238,  296,  167, -336,  -86,  357,    0, -357,   86,  336, -167, -296,  238,  238, -296, -167,  336,   86, -357,    0,  357,  -86, -336,  167,  296, -238 },
    {  351, -151, -285,  275,  167, -347,  -17,  354, -135, -296,  263,  182, -342,  -35,  357, -119, -305,  251,  197, -336,  -52,  358, -103, -314,  238,  211, -329,  -69,  359,  -86, -322,  225 },
    {  342, -211, -211,  342,    0, -342,  211,  211, -342,    0,  342, -211, -211,  342,    0, -342,  211,  211, -342,    0,  342, -211, -211,  342,    0, -342,  211,  211, -342,    0,  342, -211 },
    {  329, -263, -119,  358, -167, -225,  347,  -52, -305,  296,   69, -351,  211,  182, -357,  103,  275, -322,  -17,  336, -251, -135,  359, -151, -238,  342,  -35, -314,  285,   86, -354,  197 },
    {  314, -305,  -17,  322, -296,  -35,  329, -285,  -52,  336, -275,  -69,  342, -263,  -86,  347, -251, -103,  351, -238, -119,  354, -225, -135,  357, -211, -151,  358, -197, -167,  359, -182 },
    {  296, -336,   86,  238, -357,  167,  167, -357,  238,   86, -336,  296,    0, -296,  336,  -86, -238,  357, -167, -167,  357, -238,  -86,  336, -296,    0,  296, -336,   86,  238, -357,  167 },
    {  275, -354,  182,  119, -336,  314,  -69, -225,  359, -238,  -52,  305, -342,  135,  167, -351,  285,  -17, -263,  357, -197, -103,  329, -322,   86,  211, -358,  251,   35, -296,  347, -151 },
    {  251, -359,  263,  -17, -238,  358, -275,   35,  225, -357,  285,  -52, -211,  354, -296,   69,  197, -351,  305,  -86, -182,  347, -314,  103,  167, -342,  322, -119, -151,  336, -329,  135 },
    {  225, -351,  322, -151,  -86,  285, -359,  275,  -69, -167,  329, -347,  211,   17, -238,  354, -314,  135,  103, -296,  358, -263,   52,  182, -336,  342, -197,  -35,  251, -357,  305, -119 },
    {  197, -329,  354, -263,   86,  119, -285,  358, -314,  167,   35, -225,  342, -347,  238,  -52, -151,  305, -359,  296, -135,  -69,  251, -351,  336, -211,   17,  182, -322,  357, -275,  103 },
    {  167, -296,  357, -336,  238,  -86,  -86,  238, -336,  357, -296,  167,    0, -167,  296, -357,  336, -238,   86,   86, -238,  336, -357,  296, -167,    0,  167, -296,  357, -336,  238,  -86 },
    {  135, -251,  329, -359,  336, -263,  151,  -17, -119,  238, -322,  358, -342,  275, -167,   35,  103, -225,  314, -357,  347, -285,  182,  -52,  -86,  211, -305,  354, -351,  296, -197,   69 },
    {  103, -197,  275, -329,  357, -354,  322, -263,  182,  -86,  -17,  119, -211,  285, -336,  358, -351,  314, -251,  167,  -69,  -35,  135, -225,  296, -342,  359, -347,  305, -238,  151,  -52 },
    {   69, -135,  197, -251,  296, -329,  351, -359,  354, -336,  305, -263,  211, -151,   86,  -17,  -52,  119, -182,  238, -285,  322, -347,  358, -357,  342, -314,  275, -225,  167, -103,   35 },
    {   35,  -69,  103, -135,  167, -197,  225, -251,  275, -296,  314, -329,  342, -351,  357, -359,  358, -354,  347, -336,  322, -305,  285, -263,  238, -211,  182, -151,  119,  -86,   52,  -17 }
};
#endif

#define SHIFT_EMT_V (EMT_TRANSFORM_MATRIX_SHIFT + 1 + COM16_C806_TRANS_PREC)
#define ADD_EMT_V (1 << (SHIFT_EMT_V - 1))

static void FUNC(emt_idct_II_4x4_h)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int j;
    int E[2],O[2];
    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    const int16_t *iT = DCT_II_4x4[0];

    for (j=0; j<CB_SIZE; j++)
    {
        O[0] = iT[1*4+0]*src[CB_SIZE] + iT[3*4+0]*src[3*CB_SIZE];
        O[1] = iT[1*4+1]*src[CB_SIZE] + iT[3*4+1]*src[3*CB_SIZE];
        E[0] = iT[0*4+0]*src[0] + iT[2*4+0]*src[2*CB_SIZE];
        E[1] = iT[0*4+1]*src[0] + iT[2*4+1]*src[2*CB_SIZE];

        dst[0] = av_clip(((E[0] + O[0] + add) >> shift), clip_min, clip_max);
        dst[1] = av_clip(((E[1] + O[1] + add) >> shift), clip_min, clip_max);
        dst[2] = av_clip(((E[1] - O[1] + add) >> shift), clip_min, clip_max);
        dst[3] = av_clip(((E[0] - O[0] + add) >> shift), clip_min, clip_max);

        src   ++;
        dst += 4;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_II_4x4_v)(int16_t *src, int16_t *dst, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int j;
    int E[2],O[2];

    const int16_t *iT = DCT_II_4x4[0];

    for (j=0; j<CB_SIZE; j++)
    {
        O[0] = iT[1*4+0]*src[CB_SIZE] + iT[3*4+0]*src[3*CB_SIZE];
        O[1] = iT[1*4+1]*src[CB_SIZE] + iT[3*4+1]*src[3*CB_SIZE];
        E[0] = iT[0*4+0]*src[0] + iT[2*4+0]*src[2*CB_SIZE];
        E[1] = iT[0*4+1]*src[0] + iT[2*4+1]*src[2*CB_SIZE];

        dst[0] = av_clip(((E[0] + O[0] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        dst[1] = av_clip(((E[1] + O[1] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        dst[2] = av_clip(((E[1] - O[1] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        dst[3] = av_clip(((E[0] - O[0] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);

        src   ++;
        dst += 4;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_II_8x8_h)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int j,k;
    int E[4],O[4];
    int EE[2],EO[2];
    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    const int16_t *iT = DCT_II_8x8[0];

    for (j=0; j<CB_SIZE; j++)
    {
        for (k=0;k<4;k++)
        {
            O[k] = iT[ 1*8+k]*src[CB_SIZE] + iT[ 3*8+k]*src[3*CB_SIZE] + iT[ 5*8+k]*src[5*CB_SIZE] + iT[ 7*8+k]*src[7*CB_SIZE];
        }

        EO[0] = iT[2*8+0]*src[ 2*CB_SIZE ] + iT[6*8+0]*src[ 6*CB_SIZE ];
        EO[1] = iT[2*8+1]*src[ 2*CB_SIZE ] + iT[6*8+1]*src[ 6*CB_SIZE ];
        EE[0] = iT[0*8+0]*src[ 0      ] + iT[4*8+0]*src[ 4*CB_SIZE ];
        EE[1] = iT[0*8+1]*src[ 0      ] + iT[4*8+1]*src[ 4*CB_SIZE ];

        E[0] = EE[0] + EO[0];
        E[3] = EE[0] - EO[0];
        E[1] = EE[1] + EO[1];
        E[2] = EE[1] - EO[1];
        for (k=0;k<4;k++)
        {
            dst[k]   = av_clip( ((E[k] + O[k]     + add) >> shift), clip_min, clip_max);
            dst[k+4] = av_clip( ((E[3-k] - O[3-k] + add) >> shift), clip_min, clip_max);
        }
        src ++;
        dst += 8;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_II_8x8_v)(int16_t *src, int16_t *dst, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int j,k;
    int E[4],O[4];
    int EE[2],EO[2];

    const int16_t *iT = DCT_II_8x8[0];

    for (j=0; j<CB_SIZE; j++)
    {
        for (k=0;k<4;k++)
        {
            O[k] = iT[ 1*8+k]*src[CB_SIZE] + iT[ 3*8+k]*src[3*CB_SIZE] + iT[ 5*8+k]*src[5*CB_SIZE] + iT[ 7*8+k]*src[7*CB_SIZE];
        }

        EO[0] = iT[2*8+0]*src[ 2*CB_SIZE ] + iT[6*8+0]*src[ 6*CB_SIZE ];
        EO[1] = iT[2*8+1]*src[ 2*CB_SIZE ] + iT[6*8+1]*src[ 6*CB_SIZE ];
        EE[0] = iT[0*8+0]*src[ 0      ] + iT[4*8+0]*src[ 4*CB_SIZE ];
        EE[1] = iT[0*8+1]*src[ 0      ] + iT[4*8+1]*src[ 4*CB_SIZE ];

        E[0] = EE[0] + EO[0];
        E[3] = EE[0] - EO[0];
        E[1] = EE[1] + EO[1];
        E[2] = EE[1] - EO[1];
        for (k=0;k<4;k++)
        {
            dst[k]   = av_clip(((E[k] + O[k]     + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
            dst[k+4] = av_clip(((E[3-k] - O[3-k] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        src ++;
        dst += 8;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_II_16x16_h)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int j,k;
    int E[8],O[8];
    int EE[4],EO[4];
    int EEE[2],EEO[2];
    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    const int16_t *iT = DCT_II_16x16[0];

    for (j=0; j<CB_SIZE; j++)
    {
        for (k=0;k<8;k++)
        {
            O[k] = iT[ 1*16+k]*src[ CB_SIZE] + iT[ 3*16+k]*src[ 3*CB_SIZE] + iT[ 5*16+k]*src[ 5*CB_SIZE] + iT[ 7*16+k]*src[ 7*CB_SIZE] + iT[ 9*16+k]*src[ 9*CB_SIZE] + iT[11*16+k]*src[11*CB_SIZE] + iT[13*16+k]*src[13*CB_SIZE] + iT[15*16+k]*src[15*CB_SIZE];
        }
        for (k=0;k<4;k++)
        {
            EO[k] = iT[ 2*16+k]*src[ 2*CB_SIZE] + iT[ 6*16+k]*src[ 6*CB_SIZE] + iT[10*16+k]*src[10*CB_SIZE] + iT[14*16+k]*src[14*CB_SIZE];
        }
        EEO[0] = iT[4*16]*src[ 4*CB_SIZE ] + iT[12*16]*src[ 12*CB_SIZE ];
        EEE[0] = iT[0]*src[ 0 ] + iT[ 8*16]*src[ 8*CB_SIZE ];
        EEO[1] = iT[4*16+1]*src[ 4*CB_SIZE ] + iT[12*16+1]*src[ 12*CB_SIZE ];
        EEE[1] = iT[0*16+1]*src[ 0 ] + iT[ 8*16+1]*src[ 8*CB_SIZE  ];

        for (k=0;k<2;k++)
        {
            EE[k] = EEE[k] + EEO[k];
            EE[k+2] = EEE[1-k] - EEO[1-k];
        }
        for (k=0;k<4;k++)
        {
            E[k] = EE[k] + EO[k];
            E[k+4] = EE[3-k] - EO[3-k];
        }
        for (k=0;k<8;k++)
        {
            dst[k] = av_clip( ((E[k] + O[k]       + add) >> shift), clip_min, clip_max);
            dst[k+8] = av_clip( ((E[7-k] - O[7-k] + add) >> shift), clip_min, clip_max);
        }
        src ++;
        dst += 16;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_II_16x16_v)(int16_t *src, int16_t *dst, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int j,k;
    int E[8],O[8];
    int EE[4],EO[4];
    int EEE[2],EEO[2];

    const int16_t *iT = DCT_II_16x16[0];

    for (j=0; j<CB_SIZE; j++)
    {
        for (k=0;k<8;k++)
        {
            O[k] = iT[ 1*16+k]*src[ CB_SIZE] + iT[ 3*16+k]*src[ 3*CB_SIZE] + iT[ 5*16+k]*src[ 5*CB_SIZE] + iT[ 7*16+k]*src[ 7*CB_SIZE] + iT[ 9*16+k]*src[ 9*CB_SIZE] + iT[11*16+k]*src[11*CB_SIZE] + iT[13*16+k]*src[13*CB_SIZE] + iT[15*16+k]*src[15*CB_SIZE];
        }
        for (k=0;k<4;k++)
        {
            EO[k] = iT[ 2*16+k]*src[ 2*CB_SIZE] + iT[ 6*16+k]*src[ 6*CB_SIZE] + iT[10*16+k]*src[10*CB_SIZE] + iT[14*16+k]*src[14*CB_SIZE];
        }
        EEO[0] = iT[4*16]*src[ 4*CB_SIZE ] + iT[12*16]*src[ 12*CB_SIZE ];
        EEE[0] = iT[0]*src[ 0 ] + iT[ 8*16]*src[ 8*CB_SIZE ];
        EEO[1] = iT[4*16+1]*src[ 4*CB_SIZE ] + iT[12*16+1]*src[ 12*CB_SIZE ];
        EEE[1] = iT[0*16+1]*src[ 0 ] + iT[ 8*16+1]*src[ 8*CB_SIZE  ];

        for (k=0;k<2;k++)
        {
            EE[k] = EEE[k] + EEO[k];
            EE[k+2] = EEE[1-k] - EEO[1-k];
        }
        for (k=0;k<4;k++)
        {
            E[k] = EE[k] + EO[k];
            E[k+4] = EE[3-k] - EO[3-k];
        }
        for (k=0;k<8;k++)
        {
            dst[k]   = av_clip( ((E[k] + O[k]     + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
            dst[k+8] = av_clip( ((E[7-k] - O[7-k] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        src ++;
        dst += 16;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_II_32x32_h)(int16_t *src, int16_t *dst, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int j,k;
    int E[16],O[16];
    int EE[8],EO[8];
    int EEE[4],EEO[4];
    int EEEE[2],EEEO[2];
    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    const int16_t *iT = DCT_II_32x32[0];

    for (j=0; j<CB_SIZE; j++)
    {
        for (k=0;k<16;k++)
        {
            O[k] = iT[ 1*32+k]*src[ CB_SIZE  ] + iT[ 3*32+k]*src[ 3*CB_SIZE  ] + iT[ 5*32+k]*src[ 5*CB_SIZE  ] + iT[ 7*32+k]*src[ 7*CB_SIZE  ] +
                    iT[ 9*32+k]*src[ 9*CB_SIZE  ] + iT[11*32+k]*src[ 11*CB_SIZE ] + iT[13*32+k]*src[ 13*CB_SIZE ] + iT[15*32+k]*src[ 15*CB_SIZE ] +
                    iT[17*32+k]*src[ 17*CB_SIZE ] + iT[19*32+k]*src[ 19*CB_SIZE ] + iT[21*32+k]*src[ 21*CB_SIZE ] + iT[23*32+k]*src[ 23*CB_SIZE ] +
                    iT[25*32+k]*src[ 25*CB_SIZE ] + iT[27*32+k]*src[ 27*CB_SIZE ] + iT[29*32+k]*src[ 29*CB_SIZE ] + iT[31*32+k]*src[ 31*CB_SIZE ];
        }
        for (k=0;k<8;k++)
        {
            EO[k] = iT[ 2*32+k]*src[ 2*CB_SIZE  ] + iT[ 6*32+k]*src[ 6*CB_SIZE  ] + iT[10*32+k]*src[ 10*CB_SIZE ] + iT[14*32+k]*src[ 14*CB_SIZE ] +
                    iT[18*32+k]*src[ 18*CB_SIZE ] + iT[22*32+k]*src[ 22*CB_SIZE ] + iT[26*32+k]*src[ 26*CB_SIZE ] + iT[30*32+k]*src[ 30*CB_SIZE ];
        }
        for (k=0;k<4;k++)
        {
            EEO[k] = iT[4*32+k]*src[ 4*CB_SIZE ] + iT[12*32+k]*src[ 12*CB_SIZE ] + iT[20*32+k]*src[ 20*CB_SIZE ] + iT[28*32+k]*src[ 28*CB_SIZE ];
        }
        EEEO[0] = iT[8*32+0]*src[ 8*CB_SIZE ] + iT[24*32+0]*src[ 24*CB_SIZE ];
        EEEO[1] = iT[8*32+1]*src[ 8*CB_SIZE ] + iT[24*32+1]*src[ 24*CB_SIZE ];
        EEEE[0] = iT[0*32+0]*src[ 0      ] + iT[16*32+0]*src[ 16*CB_SIZE ];
        EEEE[1] = iT[0*32+1]*src[ 0      ] + iT[16*32+1]*src[ 16*CB_SIZE ];

        EEE[0] = EEEE[0] + EEEO[0];
        EEE[3] = EEEE[0] - EEEO[0];
        EEE[1] = EEEE[1] + EEEO[1];
        EEE[2] = EEEE[1] - EEEO[1];

        for (k=0;k<4;k++)
        {
            EE[k] = EEE[k] + EEO[k];
            EE[k+4] = EEE[3-k] - EEO[3-k];
        }
        for (k=0;k<8;k++)
        {
            E[k] = EE[k] + EO[k];
            E[k+8] = EE[7-k] - EO[7-k];
        }
        for (k=0;k<16;k++)
        {
            dst[k]    = av_clip( ((E[k] + O[k]       + add) >> shift), clip_min, clip_max);
            dst[k+16] = av_clip( ((E[15-k] - O[15-k] + add) >> shift), clip_min, clip_max);
        }
        src ++;
        dst += 32;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_II_32x32_v)(int16_t *src, int16_t *dst, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int j,k;
    int E[16],O[16];
    int EE[8],EO[8];
    int EEE[4],EEO[4];
    int EEEE[2],EEEO[2];

    const int16_t *iT = DCT_II_32x32[0];

    for (j=0; j<CB_SIZE; j++)
    {
        for (k=0;k<16;k++)
        {
            O[k] = iT[ 1*32+k]*src[ CB_SIZE  ] + iT[ 3*32+k]*src[ 3*CB_SIZE  ] + iT[ 5*32+k]*src[ 5*CB_SIZE  ] + iT[ 7*32+k]*src[ 7*CB_SIZE  ] +
                    iT[ 9*32+k]*src[ 9*CB_SIZE  ] + iT[11*32+k]*src[ 11*CB_SIZE ] + iT[13*32+k]*src[ 13*CB_SIZE ] + iT[15*32+k]*src[ 15*CB_SIZE ] +
                    iT[17*32+k]*src[ 17*CB_SIZE ] + iT[19*32+k]*src[ 19*CB_SIZE ] + iT[21*32+k]*src[ 21*CB_SIZE ] + iT[23*32+k]*src[ 23*CB_SIZE ] +
                    iT[25*32+k]*src[ 25*CB_SIZE ] + iT[27*32+k]*src[ 27*CB_SIZE ] + iT[29*32+k]*src[ 29*CB_SIZE ] + iT[31*32+k]*src[ 31*CB_SIZE ];
        }
        for (k=0;k<8;k++)
        {
            EO[k] = iT[ 2*32+k]*src[ 2*CB_SIZE  ] + iT[ 6*32+k]*src[ 6*CB_SIZE  ] + iT[10*32+k]*src[ 10*CB_SIZE ] + iT[14*32+k]*src[ 14*CB_SIZE ] +
                    iT[18*32+k]*src[ 18*CB_SIZE ] + iT[22*32+k]*src[ 22*CB_SIZE ] + iT[26*32+k]*src[ 26*CB_SIZE ] + iT[30*32+k]*src[ 30*CB_SIZE ];
        }
        for (k=0;k<4;k++)
        {
            EEO[k] = iT[4*32+k]*src[ 4*CB_SIZE ] + iT[12*32+k]*src[ 12*CB_SIZE ] + iT[20*32+k]*src[ 20*CB_SIZE ] + iT[28*32+k]*src[ 28*CB_SIZE ];
        }
        EEEO[0] = iT[8*32+0]*src[ 8*CB_SIZE ] + iT[24*32+0]*src[ 24*CB_SIZE ];
        EEEO[1] = iT[8*32+1]*src[ 8*CB_SIZE ] + iT[24*32+1]*src[ 24*CB_SIZE ];
        EEEE[0] = iT[0*32+0]*src[ 0      ] + iT[16*32+0]*src[ 16*CB_SIZE ];
        EEEE[1] = iT[0*32+1]*src[ 0      ] + iT[16*32+1]*src[ 16*CB_SIZE ];

        EEE[0] = EEEE[0] + EEEO[0];
        EEE[3] = EEEE[0] - EEEO[0];
        EEE[1] = EEEE[1] + EEEO[1];
        EEE[2] = EEEE[1] - EEEO[1];

        for (k=0;k<4;k++)
        {
            EE[k] = EEE[k] + EEO[k];
            EE[k+4] = EEE[3-k] - EEO[3-k];
        }
        for (k=0;k<8;k++)
        {
            E[k] = EE[k] + EO[k];
            E[k+8] = EE[7-k] - EO[7-k];
        }
        for (k=0;k<16;k++)
        {
            dst[k]    = av_clip( ((E[k] + O[k]       + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
            dst[k+16] = av_clip( ((E[15-k] - O[15-k] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        src ++;
        dst += 32;
    }
#undef CB_SIZE
}

// We don't use these function yet since we don't use CTUs larger than 64x64
static void FUNC(emt_idct_II_64x64_h_intra)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
//#define CB_SIZE 64
//    const int uiTrSize = 64;
//    const int16_t *iT = NULL;
//    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
//    const int add = 1 << (shift - 1);

//    int j, k;
//    int E[32],O[32];
//    int EE[16],EO[16];
//    int EEE[8],EEO[8];
//    int EEEE[4],EEEO[4];
//    int EEEEE[2],EEEEO[2];
//    for (j=0; j<CB_SIZE; j++)
//    {
//        for (k=0;k<32;k++)
//        {
//            O[k] = iT[ 1*64+k]*coeff[ CB_SIZE  ] + iT[ 3*64+k]*coeff[ 3*CB_SIZE  ] + iT[ 5*64+k]*coeff[ 5*CB_SIZE  ] + iT[ 7*64+k]*coeff[ 7*CB_SIZE  ] +
//                    iT[ 9*64+k]*coeff[ 9*CB_SIZE  ] + iT[11*64+k]*coeff[ 11*CB_SIZE ] + iT[13*64+k]*coeff[ 13*CB_SIZE ] + iT[15*64+k]*coeff[ 15*CB_SIZE ] +
//                    iT[17*64+k]*coeff[ 17*CB_SIZE ] + iT[19*64+k]*coeff[ 19*CB_SIZE ] + iT[21*64+k]*coeff[ 21*CB_SIZE ] + iT[23*64+k]*coeff[ 23*CB_SIZE ] +
//                    iT[25*64+k]*coeff[ 25*CB_SIZE ] + iT[27*64+k]*coeff[ 27*CB_SIZE ] + iT[29*64+k]*coeff[ 29*CB_SIZE ] + iT[31*64+k]*coeff[ 31*CB_SIZE ] +
//                    iT[33*64+k]*coeff[ 33*CB_SIZE ] + iT[35*64+k]*coeff[ 35*CB_SIZE ] + iT[37*64+k]*coeff[ 37*CB_SIZE ] + iT[39*64+k]*coeff[ 39*CB_SIZE ] +
//                    iT[41*64+k]*coeff[ 41*CB_SIZE ] + iT[43*64+k]*coeff[ 43*CB_SIZE ] + iT[45*64+k]*coeff[ 45*CB_SIZE ] + iT[47*64+k]*coeff[ 47*CB_SIZE ] +
//                    iT[49*64+k]*coeff[ 49*CB_SIZE ] + iT[51*64+k]*coeff[ 51*CB_SIZE ] + iT[53*64+k]*coeff[ 53*CB_SIZE ] + iT[55*64+k]*coeff[ 55*CB_SIZE ] +
//                    iT[57*64+k]*coeff[ 57*CB_SIZE ] + iT[59*64+k]*coeff[ 59*CB_SIZE ] + iT[61*64+k]*coeff[ 61*CB_SIZE ] + iT[63*64+k]*coeff[ 63*CB_SIZE ];
//        }
//        for (k=0;k<16;k++)
//        {
//            EO[k] = iT[ 2*64+k]*coeff[ 2*CB_SIZE  ] + iT[ 6*64+k]*coeff[ 6*CB_SIZE  ] + iT[10*64+k]*coeff[ 10*CB_SIZE ] + iT[14*64+k]*coeff[ 14*CB_SIZE ] +
//                    iT[18*64+k]*coeff[ 18*CB_SIZE ] + iT[22*64+k]*coeff[ 22*CB_SIZE ] + iT[26*64+k]*coeff[ 26*CB_SIZE ] + iT[30*64+k]*coeff[ 30*CB_SIZE ] +
//                    iT[34*64+k]*coeff[ 34*CB_SIZE ] + iT[38*64+k]*coeff[ 38*CB_SIZE ] + iT[42*64+k]*coeff[ 42*CB_SIZE ] + iT[46*64+k]*coeff[ 46*CB_SIZE ] +
//                    iT[50*64+k]*coeff[ 50*CB_SIZE ] + iT[54*64+k]*coeff[ 54*CB_SIZE ] + iT[58*64+k]*coeff[ 58*CB_SIZE ] + iT[62*64+k]*coeff[ 62*CB_SIZE ];
//        }
//        for (k=0;k<8;k++)
//        {
//            EEO[k] = iT[4*64+k]*coeff[ 4*CB_SIZE ] + iT[12*64+k]*coeff[ 12*CB_SIZE ] + iT[20*64+k]*coeff[ 20*CB_SIZE ] + iT[28*64+k]*coeff[ 28*CB_SIZE ] +
//                      iT[36*64+k]*coeff[ 36*CB_SIZE ] + iT[44*64+k]*coeff[ 44*CB_SIZE ] + iT[52*64+k]*coeff[ 52*CB_SIZE ] + iT[60*64+k]*coeff[ 60*CB_SIZE ] ;
//        }
//        for (k=0;k<4;k++)
//        {
//            EEEO[k] = iT[8*64+k]*coeff[ 8*CB_SIZE ] + iT[24*64+k]*coeff[ 24*CB_SIZE ] +  iT[40*64+k]*coeff[ 40*CB_SIZE ] + iT[56*64+k]*coeff[ 56*CB_SIZE ];
//        }
//        EEEEO[0] = iT[16*64+0]*coeff[ 16*CB_SIZE ] +   iT[48*64+0]*coeff[ 48*CB_SIZE ] ;
//        EEEEO[1] = iT[16*64+1]*coeff[ 16*CB_SIZE ] +   iT[48*64+1]*coeff[ 48*CB_SIZE ] ;
//        EEEEE[0] = iT[ 0*64+0]*coeff[  0      ] +  iT[32*64+0]*coeff[ 32*CB_SIZE ] ;
//        EEEEE[1] = iT[ 0*64+1]*coeff[  0      ] +  iT[32*64+1]*coeff[ 32*CB_SIZE ] ;

//        for (k=0;k<2;k++)
//        {
//            EEEE[k] = EEEEE[k] + EEEEO[k];
//            EEEE[k+2] = EEEEE[1-k] - EEEEO[1-k];
//        }
//        for (k=0;k<4;k++)
//        {
//            EEE[k] = EEEE[k] + EEEO[k];
//            EEE[k+4] = EEEE[3-k] - EEEO[3-k];
//        }
//        for (k=0;k<8;k++)
//        {
//            EE[k] = EEE[k] + EEO[k];
//            EE[k+8] = EEE[7-k] - EEO[7-k];
//        }
//        for (k=0;k<16;k++)
//        {
//            E[k] = EE[k] + EO[k];
//            E[k+16] = EE[15-k] - EO[15-k];
//        }
//        for (k=0;k<32;k++)
//        {
//            block[k]    = av_clip( ((E[k] + O[k]       + add) >> shift), clip_min, clip_max);
//            block[k+32] = av_clip( ((E[31-k] - O[31-k] + add) >> shift), clip_min, clip_max);
//        }
//        coeff ++;
//        block += uiTrSize;
//    }
//#undef CB_SIZE
}

static void FUNC(emt_idct_II_64x64_v_intra)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
//#define CB_SIZE 64
//    const int uiTrSize = 64;
//    const int16_t *iT = NULL;

//    int j, k;
//    int E[32],O[32];
//    int EE[16],EO[16];
//    int EEE[8],EEO[8];
//    int EEEE[4],EEEO[4];
//    int EEEEE[2],EEEEO[2];
//    for (j=0; j<CB_SIZE; j++)
//    {
//        for (k=0;k<32;k++)
//        {
//            O[k] = iT[ 1*64+k]*coeff[ CB_SIZE  ] + iT[ 3*64+k]*coeff[ 3*CB_SIZE  ] + iT[ 5*64+k]*coeff[ 5*CB_SIZE  ] + iT[ 7*64+k]*coeff[ 7*CB_SIZE  ] +
//                    iT[ 9*64+k]*coeff[ 9*CB_SIZE  ] + iT[11*64+k]*coeff[ 11*CB_SIZE ] + iT[13*64+k]*coeff[ 13*CB_SIZE ] + iT[15*64+k]*coeff[ 15*CB_SIZE ] +
//                    iT[17*64+k]*coeff[ 17*CB_SIZE ] + iT[19*64+k]*coeff[ 19*CB_SIZE ] + iT[21*64+k]*coeff[ 21*CB_SIZE ] + iT[23*64+k]*coeff[ 23*CB_SIZE ] +
//                    iT[25*64+k]*coeff[ 25*CB_SIZE ] + iT[27*64+k]*coeff[ 27*CB_SIZE ] + iT[29*64+k]*coeff[ 29*CB_SIZE ] + iT[31*64+k]*coeff[ 31*CB_SIZE ] +
//                    iT[33*64+k]*coeff[ 33*CB_SIZE ] + iT[35*64+k]*coeff[ 35*CB_SIZE ] + iT[37*64+k]*coeff[ 37*CB_SIZE ] + iT[39*64+k]*coeff[ 39*CB_SIZE ] +
//                    iT[41*64+k]*coeff[ 41*CB_SIZE ] + iT[43*64+k]*coeff[ 43*CB_SIZE ] + iT[45*64+k]*coeff[ 45*CB_SIZE ] + iT[47*64+k]*coeff[ 47*CB_SIZE ] +
//                    iT[49*64+k]*coeff[ 49*CB_SIZE ] + iT[51*64+k]*coeff[ 51*CB_SIZE ] + iT[53*64+k]*coeff[ 53*CB_SIZE ] + iT[55*64+k]*coeff[ 55*CB_SIZE ] +
//                    iT[57*64+k]*coeff[ 57*CB_SIZE ] + iT[59*64+k]*coeff[ 59*CB_SIZE ] + iT[61*64+k]*coeff[ 61*CB_SIZE ] + iT[63*64+k]*coeff[ 63*CB_SIZE ];
//        }
//        for (k=0;k<16;k++)
//        {
//            EO[k] = iT[ 2*64+k]*coeff[ 2*CB_SIZE  ] + iT[ 6*64+k]*coeff[ 6*CB_SIZE  ] + iT[10*64+k]*coeff[ 10*CB_SIZE ] + iT[14*64+k]*coeff[ 14*CB_SIZE ] +
//                    iT[18*64+k]*coeff[ 18*CB_SIZE ] + iT[22*64+k]*coeff[ 22*CB_SIZE ] + iT[26*64+k]*coeff[ 26*CB_SIZE ] + iT[30*64+k]*coeff[ 30*CB_SIZE ] +
//                    iT[34*64+k]*coeff[ 34*CB_SIZE ] + iT[38*64+k]*coeff[ 38*CB_SIZE ] + iT[42*64+k]*coeff[ 42*CB_SIZE ] + iT[46*64+k]*coeff[ 46*CB_SIZE ] +
//                    iT[50*64+k]*coeff[ 50*CB_SIZE ] + iT[54*64+k]*coeff[ 54*CB_SIZE ] + iT[58*64+k]*coeff[ 58*CB_SIZE ] + iT[62*64+k]*coeff[ 62*CB_SIZE ];
//        }
//        for (k=0;k<8;k++)
//        {
//            EEO[k] = iT[4*64+k]*coeff[ 4*CB_SIZE ] + iT[12*64+k]*coeff[ 12*CB_SIZE ] + iT[20*64+k]*coeff[ 20*CB_SIZE ] + iT[28*64+k]*coeff[ 28*CB_SIZE ] +
//                     iT[36*64+k]*coeff[ 36*CB_SIZE ] + iT[44*64+k]*coeff[ 44*CB_SIZE ] + iT[52*64+k]*coeff[ 52*CB_SIZE ] + iT[60*64+k]*coeff[ 60*CB_SIZE ];
//        }
//        for (k=0;k<4;k++)
//        {
//            EEEO[k] = iT[8*64+k]*coeff[ 8*CB_SIZE ] + iT[24*64+k]*coeff[ 24*CB_SIZE ] + iT[40*64+k]*coeff[ 40*CB_SIZE ] + iT[56*64+k]*coeff[ 56*CB_SIZE ];
//        }
//        EEEEO[0] = iT[16*64+0]*coeff[ 16*CB_SIZE ] + iT[48*64+0]*coeff[ 48*CB_SIZE ];
//        EEEEO[1] = iT[16*64+1]*coeff[ 16*CB_SIZE ] + iT[48*64+1]*coeff[ 48*CB_SIZE ];
//        EEEEE[0] = iT[ 0*64+0]*coeff[  0      ] + iT[32*64+0]*coeff[ 32*CB_SIZE ];
//        EEEEE[1] = iT[ 0*64+1]*coeff[  0      ] + iT[32*64+1]*coeff[ 32*CB_SIZE ];

//        for (k=0;k<2;k++)
//        {
//            EEEE[k] = EEEEE[k] + EEEEO[k];
//            EEEE[k+2] = EEEEE[1-k] - EEEEO[1-k];
//        }
//        for (k=0;k<4;k++)
//        {
//            EEE[k] = EEEE[k] + EEEO[k];
//            EEE[k+4] = EEEE[3-k] - EEEO[3-k];
//        }
//        for (k=0;k<8;k++)
//        {
//            EE[k] = EEE[k] + EEO[k];
//            EE[k+8] = EEE[7-k] - EEO[7-k];
//        }
//        for (k=0;k<16;k++)
//        {
//            E[k] = EE[k] + EO[k];
//            E[k+16] = EE[15-k] - EO[15-k];
//        }
//        for (k=0;k<32;k++)
//        {
//            block[k]    = av_clip( ((E[k] + O[k]       + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
//            block[k+32] = av_clip( ((E[31-k] - O[31-k] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
//        }
//        coeff ++;
//        block += uiTrSize;
//    }
//#undef CB_SIZE
}

// We don't use these function yet since we don't use CTUs larger than 64x64
static void FUNC(emt_idct_II_64x64_h_inter)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
//#define CB_SIZE 64
//    const int uiTrSize = 64;
//    const int16_t *iT = NULL;
//    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
//    const int add = 1 << (shift - 1);


//    int j, k;
//    int E[32],O[32];
//    int EE[16],EO[16];
//    int EEE[8],EEO[8];
//    int EEEE[4],EEEO[4];
//    int EEEEE[2],EEEEO[2];
//    for (j=0; j<CB_SIZE; j++)
//    {
//        for (k=0;k<32;k++)
//        {
//            O[k] = iT[ 1*64+k]*coeff[ CB_SIZE  ] + iT[ 3*64+k]*coeff[ 3*CB_SIZE  ] + iT[ 5*64+k]*coeff[ 5*CB_SIZE  ] + iT[ 7*64+k]*coeff[ 7*CB_SIZE  ] +
//                    iT[ 9*64+k]*coeff[ 9*CB_SIZE  ] + iT[11*64+k]*coeff[ 11*CB_SIZE ] + iT[13*64+k]*coeff[ 13*CB_SIZE ] + iT[15*64+k]*coeff[ 15*CB_SIZE ] +
//                    iT[17*64+k]*coeff[ 17*CB_SIZE ] + iT[19*64+k]*coeff[ 19*CB_SIZE ] + iT[21*64+k]*coeff[ 21*CB_SIZE ] + iT[23*64+k]*coeff[ 23*CB_SIZE ] +
//                    iT[25*64+k]*coeff[ 25*CB_SIZE ] + iT[27*64+k]*coeff[ 27*CB_SIZE ] + iT[29*64+k]*coeff[ 29*CB_SIZE ] + iT[31*64+k]*coeff[ 31*CB_SIZE ];
//        }
//        for (k=0;k<16;k++)
//        {
//            EO[k] = iT[ 2*64+k]*coeff[ 2*CB_SIZE  ] + iT[ 6*64+k]*coeff[ 6*CB_SIZE  ] + iT[10*64+k]*coeff[ 10*CB_SIZE ] + iT[14*64+k]*coeff[ 14*CB_SIZE ] +
//                    iT[18*64+k]*coeff[ 18*CB_SIZE ] + iT[22*64+k]*coeff[ 22*CB_SIZE ] + iT[26*64+k]*coeff[ 26*CB_SIZE ] + iT[30*64+k]*coeff[ 30*CB_SIZE ];
//        }
//        for (k=0;k<8;k++)
//        {
//            EEO[k] = iT[4*64+k]*coeff[ 4*CB_SIZE ] + iT[12*64+k]*coeff[ 12*CB_SIZE ] + iT[20*64+k]*coeff[ 20*CB_SIZE ] + iT[28*64+k]*coeff[ 28*CB_SIZE ];
//        }
//        for (k=0;k<4;k++)
//        {
//            EEEO[k] = iT[8*64+k]*coeff[ 8*CB_SIZE ] + iT[24*64+k]*coeff[ 24*CB_SIZE ];
//        }
//        EEEEO[0] = iT[16*64+0]*coeff[ 16*CB_SIZE ];
//        EEEEO[1] = iT[16*64+1]*coeff[ 16*CB_SIZE ];
//        EEEEE[0] = iT[ 0*64+0]*coeff[  0      ];
//        EEEEE[1] = iT[ 0*64+1]*coeff[  0      ];

//        for (k=0;k<2;k++)
//        {
//            EEEE[k] = EEEEE[k] + EEEEO[k];
//            EEEE[k+2] = EEEEE[1-k] - EEEEO[1-k];
//        }
//        for (k=0;k<4;k++)
//        {
//            EEE[k] = EEEE[k] + EEEO[k];
//            EEE[k+4] = EEEE[3-k] - EEEO[3-k];
//        }
//        for (k=0;k<8;k++)
//        {
//            EE[k] = EEE[k] + EEO[k];
//            EE[k+8] = EEE[7-k] - EEO[7-k];
//        }
//        for (k=0;k<16;k++)
//        {
//            E[k] = EE[k] + EO[k];
//            E[k+16] = EE[15-k] - EO[15-k];
//        }
//        for (k=0;k<32;k++)
//        {
//            block[k]    = av_clip( ((E[k] + O[k]       + add) >> shift), clip_min, clip_max);
//            block[k+32] = av_clip( ((E[31-k] - O[31-k] + add) >> shift), clip_min, clip_max);
//        }
//        coeff ++;
//        block += uiTrSize;
//    }
//#undef CB_SIZE
}

// Unused
static void FUNC(emt_idct_II_64x64_v_inter)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
//#define CB_SIZE 64

//    const int uiTrSize = 64;
//    const int16_t *iT = NULL;

//    int j, k;
//    int E[32],O[32];
//    int EE[16],EO[16];
//    int EEE[8],EEO[8];
//    int EEEE[4],EEEO[4];
//    int EEEEE[2],EEEEO[2];
//    for (j=0; j < (CB_SIZE >> 1); j++)
//    {
//        for (k=0;k<32;k++)
//        {
//            O[k] = iT[ 1*64+k]*coeff[ CB_SIZE  ] + iT[ 3*64+k]*coeff[ 3*CB_SIZE  ] + iT[ 5*64+k]*coeff[ 5*CB_SIZE  ] + iT[ 7*64+k]*coeff[ 7*CB_SIZE  ] +
//                    iT[ 9*64+k]*coeff[ 9*CB_SIZE  ] + iT[11*64+k]*coeff[ 11*CB_SIZE ] + iT[13*64+k]*coeff[ 13*CB_SIZE ] + iT[15*64+k]*coeff[ 15*CB_SIZE ] +
//                    iT[17*64+k]*coeff[ 17*CB_SIZE ] + iT[19*64+k]*coeff[ 19*CB_SIZE ] + iT[21*64+k]*coeff[ 21*CB_SIZE ] + iT[23*64+k]*coeff[ 23*CB_SIZE ] +
//                    iT[25*64+k]*coeff[ 25*CB_SIZE ] + iT[27*64+k]*coeff[ 27*CB_SIZE ] + iT[29*64+k]*coeff[ 29*CB_SIZE ] + iT[31*64+k]*coeff[ 31*CB_SIZE ];
//        }
//        for (k=0;k<16;k++)
//        {
//            EO[k] = iT[ 2*64+k]*coeff[ 2*CB_SIZE  ] + iT[ 6*64+k]*coeff[ 6*CB_SIZE  ] + iT[10*64+k]*coeff[ 10*CB_SIZE ] + iT[14*64+k]*coeff[ 14*CB_SIZE ] +
//                    iT[18*64+k]*coeff[ 18*CB_SIZE ] + iT[22*64+k]*coeff[ 22*CB_SIZE ] + iT[26*64+k]*coeff[ 26*CB_SIZE ] + iT[30*64+k]*coeff[ 30*CB_SIZE ];
//        }
//        for (k=0;k<8;k++)
//        {
//            EEO[k] = iT[4*64+k]*coeff[ 4*CB_SIZE ] + iT[12*64+k]*coeff[ 12*CB_SIZE ] + iT[20*64+k]*coeff[ 20*CB_SIZE ] + iT[28*64+k]*coeff[ 28*CB_SIZE ];
//        }
//        for (k=0;k<4;k++)
//        {
//            EEEO[k] = iT[8*64+k]*coeff[ 8*CB_SIZE ] + iT[24*64+k]*coeff[ 24*CB_SIZE ];
//        }
//        EEEEO[0] = iT[16*64+0]*coeff[ 16*CB_SIZE ];
//        EEEEO[1] = iT[16*64+1]*coeff[ 16*CB_SIZE ];
//        EEEEE[0] = iT[ 0*64+0]*coeff[  0      ];
//        EEEEE[1] = iT[ 0*64+1]*coeff[  0      ];

//        for (k=0;k<2;k++)
//        {
//            EEEE[k] = EEEEE[k] + EEEEO[k];
//            EEEE[k+2] = EEEEE[1-k] - EEEEO[1-k];
//        }
//        for (k=0;k<4;k++)
//        {
//            EEE[k] = EEEE[k] + EEEO[k];
//            EEE[k+4] = EEEE[3-k] - EEEO[3-k];
//        }
//        for (k=0;k<8;k++)
//        {
//            EE[k] = EEE[k] + EEO[k];
//            EE[k+8] = EEE[7-k] - EEO[7-k];
//        }
//        for (k=0;k<16;k++)
//        {
//            E[k] = EE[k] + EO[k];
//            E[k+16] = EE[15-k] - EO[15-k];
//        }
//        for (k=0;k<32;k++)
//        {
//            block[k]    = av_clip( ((E[k] + O[k]       + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
//            block[k+32] = av_clip( ((E[31-k] - O[31-k] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
//        }
//        coeff ++;
//        block += uiTrSize;
//    }
//#undef CB_SIZE
}

static void FUNC(emt_idct_V_4x4_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_V_4x4[k][j];
            }
            block[j] = av_clip(((iSum + add)>>shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_V_4x4_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_V_4x4[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block+= CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_V_8x8_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_V_8x8[k][j];
            }
            block[j] = av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_V_8x8_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_V_8x8[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_V_16x16_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_V_16x16[k][j];
            }
            block[j] = av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_V_16x16_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_V_16x16[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_V_32x32_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_V_32x32[k][j];
            }
            block[j] = av_clip( ((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_V_32x32_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += (coeff[k * CB_SIZE] * DCT_V_32x32[k][j]);
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_4x4_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    const int16_t *iT = DCT_VIII_4x4[0];

    int c[4];
    for (i=0; i<CB_SIZE; i++)
    {
        c[0] = coeff[ 0] + coeff[12];
        c[1] = coeff[ 8] + coeff[ 0];
        c[2] = coeff[12] - coeff[ 8];
        c[3] = iT[1]* coeff[4];

        block[0] = av_clip( ((iT[3] * c[0] + iT[2] * c[1] + c[3]        + add) >> shift), clip_min, clip_max);
        block[1] = av_clip( ((iT[1] * (coeff[0] - coeff[8] - coeff[12]) + add) >> shift), clip_min, clip_max);
        block[2] = av_clip( ((iT[3] * c[2] + iT[2] * c[0] - c[3]        + add) >> shift), clip_min, clip_max);
        block[3] = av_clip( ((iT[3] * c[1] - iT[2] * c[2] - c[3]        + add) >> shift), clip_min, clip_max);

        block+=4;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_4x4_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i;

    const int16_t *iT = DCT_VIII_4x4[0];

    int c[4];
    for (i=0; i<CB_SIZE; i++)
    {
        c[0] = coeff[ 0] + coeff[12];
        c[1] = coeff[ 8] + coeff[ 0];
        c[2] = coeff[12] - coeff[ 8];
        c[3] = iT[1]* coeff[4];

        block[0] = av_clip(((iT[3] * c[0] + iT[2] * c[1] + c[3]        + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[1] = av_clip(((iT[1] * (coeff[0] - coeff[8] - coeff[12]) + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[2] = av_clip(((iT[3] * c[2] + iT[2] * c[0] - c[3]        + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[3] = av_clip(((iT[3] * c[1] - iT[2] * c[2] - c[3]        + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);

        block+=4;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_8x8_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_8x8[k][j];
            }
            block[j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_8x8_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_8x8[k][j];
            }
            block[j] =  av_clip( ((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_16x16_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_16x16[k][j];
            }
            block[j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_16x16_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_16x16[k][j];
            }
            block[j] =  av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_32x32_h_intra)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i=0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_32x32[k][j];
            }
            block[j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }

#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_32x32_v_intra)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k=0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_32x32[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_32x32_h_inter)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < (CB_SIZE >> 1); k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_32x32[k][j];
            }
            block[j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idct_VIII_32x32_v_inter)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    for (i = 0; i < (CB_SIZE >> 1); i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < (CB_SIZE >> 1); k++){
                iSum += coeff[k * CB_SIZE] * DCT_VIII_32x32[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_4x4_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    const int16_t *iT = DST_I_4x4[0];

    int E[2],O[2];
    for (i=0; i<CB_SIZE; i++)
    {
        E[0] = coeff[0*4] + coeff[3*4];
        O[0] = coeff[0*4] - coeff[3*4];
        E[1] = coeff[1*4] + coeff[2*4];
        O[1] = coeff[1*4] - coeff[2*4];

        block[0] = av_clip( ((E[0]*iT[0] + E[1]*iT[1] + add) >> shift), clip_min, clip_max);
        block[1] = av_clip( ((O[0]*iT[1] + O[1]*iT[0] + add) >> shift), clip_min, clip_max);
        block[2] = av_clip( ((E[0]*iT[1] - E[1]*iT[0] + add) >> shift), clip_min, clip_max);
        block[3] = av_clip( ((O[0]*iT[0] - O[1]*iT[1] + add) >> shift), clip_min, clip_max);

        block += 4;
        coeff ++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_4x4_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i;

    const int16_t *iT = DST_I_4x4[0];

    int E[2],O[2];
    for (i=0; i<CB_SIZE; i++)
    {
        E[0] = coeff[0*4] + coeff[3*4];
        O[0] = coeff[0*4] - coeff[3*4];
        E[1] = coeff[1*4] + coeff[2*4];
        O[1] = coeff[1*4] - coeff[2*4];

        block[0] = av_clip( ((E[0]*iT[0] + E[1]*iT[1] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[1] = av_clip( ((O[0]*iT[1] + O[1]*iT[0] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[2] = av_clip( ((E[0]*iT[1] - E[1]*iT[0] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[3] = av_clip( ((O[0]*iT[0] - O[1]*iT[1] + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);

        block += 4;
        coeff ++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_8x8_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j<CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE + i] * DST_I_8x8[k][j];
            }
            block[i * CB_SIZE + j] = av_clip( ((iSum + add) >> shift), clip_min, clip_max);
        }
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_8x8_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE + i] * DST_I_8x8[k][j];
            }
            block[i * CB_SIZE + j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_16x16_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE + i] * DST_I_16x16[k][j];
            }
            block[i * CB_SIZE + j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_16x16_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    for (i = 0; i<CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE + i] * DST_I_16x16[k][j];
            }
            block[i * CB_SIZE + j] =  av_clip( ((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_32x32_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i  = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE + i] * DST_I_32x32[k][j];
            }
            block[i * CB_SIZE + j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_I_32x32_v)(int16_t *coeff, int16_t *block, int shift,  const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE + i] * DST_I_32x32[k][j];
            }
            block[i * CB_SIZE + j] =  av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_4x4_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i, c[4];

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    const int16_t *iT = DST_VII_4x4[0];

    for (i=0; i<CB_SIZE; i++)
    {
        c[0] = coeff[0] + coeff[ 8];
        c[1] = coeff[8] + coeff[12];
        c[2] = coeff[0] - coeff[12];
        c[3] = iT[2]* coeff[4];

        block[0] = av_clip( (( iT[0] * c[0] + iT[1] * c[1] + c[3]         + add ) >> shift), clip_min, clip_max);
        block[1] = av_clip( (( iT[1] * c[2] - iT[0] * c[1] + c[3]         + add ) >> shift), clip_min, clip_max);
        block[2] = av_clip( (( iT[2] * (coeff[0] - coeff[8]  + coeff[12]) + add ) >> shift), clip_min, clip_max);
        block[3] = av_clip( (( iT[1] * c[0] + iT[0] * c[2] - c[3]         + add ) >> shift), clip_min, clip_max);

        block+=4;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_4x4_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 4
    int i, c[4];

    const int16_t *iT = DST_VII_4x4[0];

    for (i=0; i<CB_SIZE; i++)
    {
        c[0] = coeff[0] + coeff[ 8];
        c[1] = coeff[8] + coeff[12];
        c[2] = coeff[0] - coeff[12];
        c[3] = iT[2]* coeff[4];

        block[0] = av_clip( (( iT[0] * c[0] + iT[1] * c[1] + c[3]         + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[1] = av_clip( (( iT[1] * c[2] - iT[0] * c[1] + c[3]         + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[2] = av_clip( (( iT[2] * (coeff[0] - coeff[8]  + coeff[12]) + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        block[3] = av_clip( (( iT[1] * c[0] + iT[0] * c[2] - c[3]         + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);

        block+=4;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_8x8_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_8x8[k][j];
            }
            block[j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_8x8_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 8
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_8x8[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_16x16_h)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_16x16[k][j];
            }
            block[j] =  av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_16x16_v)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 16
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_16x16[k][j];
            }
            block[j] =  av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_32x32_h_intra)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_32x32[k][j];
            }
            block[j] = av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_32x32_h_inter)(int16_t *coeff, int16_t *block, int log2_transform_range, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    const int shift    = (EMT_TRANSFORM_MATRIX_SHIFT + log2_transform_range - 1) - BIT_DEPTH + COM16_C806_TRANS_PREC;
    const int add = 1 << (shift - 1);

    for (i = 0; i< CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < (CB_SIZE >> 1); k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_32x32[k][j];
            }
            block[j] = av_clip(((iSum + add) >> shift), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_32x32_v_intra)(int16_t *coeff, int16_t *block, int shift, const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    for (i = 0; i < CB_SIZE; i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < CB_SIZE; k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_32x32[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

static void FUNC(emt_idst_VII_32x32_v_inter)(int16_t *coeff, int16_t *block, int shift, /*int line,*/ /*int zo,*/ /*int use,*/ const int clip_min, const int clip_max)
{
#define CB_SIZE 32
    int i, j, k, iSum;

    for (i = 0; i < (CB_SIZE >> 1); i++){
        for (j = 0; j < CB_SIZE; j++){
            iSum = 0;
            for (k = 0; k < (CB_SIZE >> 1); k++){
                iSum += coeff[k * CB_SIZE] * DST_VII_32x32[k][j];
            }
            block[j] = av_clip(((iSum + ADD_EMT_V) >> SHIFT_EMT_V), clip_min, clip_max);
        }
        block += CB_SIZE;
        coeff++;
    }
#undef CB_SIZE
}

#endif

static void FUNC(put_hevc_pel_pixels)(int16_t *dst,
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
        dst += MAX_PB_SIZE;
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
                                         int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                           int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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

static void FUNC(put_hevc_qpel_h)(int16_t *dst,
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
        dst += MAX_PB_SIZE;
    }
}

static void FUNC(put_hevc_qpel_v)(int16_t *dst,
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
        dst += MAX_PB_SIZE;
    }
}

static void FUNC(put_hevc_qpel_hv)(int16_t *dst,
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
        dst += MAX_PB_SIZE;
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
                                     int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                     int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                      int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                       int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                       int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                        int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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

static void FUNC(put_hevc_epel_h)(int16_t *dst,
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
        dst += MAX_PB_SIZE;
    }
}

static void FUNC(put_hevc_epel_v)(int16_t *dst,
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
        dst += MAX_PB_SIZE;
    }
}

static void FUNC(put_hevc_epel_hv)(int16_t *dst,
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
        dst += MAX_PB_SIZE;
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
                                     int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                     int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                      int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                       int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                       int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                        int16_t *src2,
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
        src2 += MAX_PB_SIZE;
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
                                        int beta, int *_tc,
                                        uint8_t *_no_p, uint8_t *_no_q)
{
    int d, j;
    pixel *pix        = (pixel *)_pix;
    ptrdiff_t xstride = _xstride / sizeof(pixel);
    ptrdiff_t ystride = _ystride / sizeof(pixel);

    beta <<= BIT_DEPTH - 8;

    for (j = 0; j < 2; j++) {
        const int dp0  = abs(P2  - 2 * P1  + P0);
        const int dq0  = abs(Q2  - 2 * Q1  + Q0);
        const int dp3  = abs(TP2 - 2 * TP1 + TP0);
        const int dq3  = abs(TQ2 - 2 * TQ1 + TQ0);
        const int d0   = dp0 + dq0;
        const int d3   = dp3 + dq3;
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
                                          int beta, int *tc, uint8_t *no_p,
                                          uint8_t *no_q)
{
    FUNC(hevc_loop_filter_luma)(pix, stride, sizeof(pixel),
                                beta, tc, no_p, no_q);
}

static void FUNC(hevc_v_loop_filter_luma)(uint8_t *pix, ptrdiff_t stride,
                                          int beta, int *tc, uint8_t *no_p,
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
    int x, i, j, phase, refPos16, refPos, shift = up_info->shift_up[0];
    int16_t*   dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src;
    const int8_t*   coeff;
    //short * srcY1;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartL, rightEndL);
        refPos16 = (((x - leftStartL)*up_info->scaleXLum - up_info->addXLum) >> 12);
        phase    = refPos16 & 15;
        //printf("x %d phase %d \n", x, phase);
        coeff    = up_sample_filter_luma[phase];
        refPos   = (refPos16 >> 4) - x_BL;
        dst_tmp  = _dst  + i;
        src_tmp  = src   + refPos;
       // srcY1 = buffer_frame + y_BL*widthEL+ x_EL+i;
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = (LumHor_FILTER_Block(src_tmp, coeff)>>shift);
            //if(*srcY1 != *dst_tmp)
                //printf("--- %d %d %d %d %d %d %d %d %d \n",refPos, i, j, *srcY1, *dst_tmp, src_tmp[-3], src_tmp[-2], src_tmp[-1], src_tmp[0]);
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
            //srcY1    += widthEL;
        }
    }
}

static void FUNC(upsample_filter_block_luma_h_all_8)( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                        const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info/*, int y_BL, short * buffer_frame*/) {
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j, phase, refPos16, refPos, shift = up_info->shift_up[0];
    int16_t*   dst_tmp;
    uint8_t*   src_tmp, *src = (uint8_t *) _src;
    const int8_t*   coeff;
    //short * srcY1;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartL, rightEndL);
        refPos16 = (((x - leftStartL)*up_info->scaleXLum - up_info->addXLum) >> 12);
        phase    = refPos16 & 15;
        //printf("x %d phase %d \n", x, phase);
        coeff    = up_sample_filter_luma[phase];
        refPos   = (refPos16 >> 4) - x_BL;
        dst_tmp  = _dst  + i;
        src_tmp  = src   + refPos;
       // srcY1 = buffer_frame + y_BL*widthEL+ x_EL+i;
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = (LumHor_FILTER_Block(src_tmp, coeff)>>shift);
            //if(*srcY1 != *dst_tmp)
                //printf("--- %d %d %d %d %d %d %d %d %d \n",refPos, i, j, *srcY1, *dst_tmp, src_tmp[-3], src_tmp[-2], src_tmp[-1], src_tmp[0]);
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
    int x, i, j, phase, refPos16, refPos, shift = up_info->shift_up[1];
    int16_t*  dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src;
    const int8_t*  coeff;
    
    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartC, rightEndC);
        refPos16 = (((x - leftStartC)*up_info->scaleXCr - up_info->addXCr) >> 12);
        phase    = refPos16 & 15;
        coeff    = up_sample_filter_chroma[phase];
        refPos   = (refPos16 >> 4) - (x_BL);
        dst_tmp  = dst  + i;
        src_tmp  = src + refPos;
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  (CroHor_FILTER_Block(src_tmp, coeff)>>shift);
            src_tmp  +=  _srcstride;
            dst_tmp   +=  dststride;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_all_8)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                 int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                 const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j, phase, refPos16, refPos, shift = up_info->shift_up[1];
    int16_t*  dst_tmp;
    uint8_t*   src_tmp, *src = (uint8_t *) _src;
    const int8_t*  coeff;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartC, rightEndC);
        refPos16 = (((x - leftStartC)*up_info->scaleXCr - up_info->addXCr) >> 12);
        phase    = refPos16 & 15;
        coeff    = up_sample_filter_chroma[phase];
        refPos   = (refPos16 >> 4) - (x_BL);
        dst_tmp  = dst  + i;
        src_tmp  = src + refPos;
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  (CroHor_FILTER_Block(src_tmp, coeff)>>shift);
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
    int16_t *   src_tmp;
    uint16_t *dst_tmp, *dst    = (uint16_t *)_dst;
    _dststride /= sizeof(pixel);
    for( j = 0; j < block_h; j++ )	{
    	y        =   av_clip_c(y_EL+j, topStartL, bottomEndL-1);
    	refPos16 = ((( y - topStartL )* up_info->scaleYLum - up_info->addYLum) >> 12);
        phase    = refPos16 & 15;
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
    dststride /= sizeof(pixel);
    for( j = 0; j < block_h; j++ ) {
        y =   av_clip_c(y_EL+j, topStartC, bottomEndC-1);
        refPos16 = ((( y - topStartC )* up_info->scaleYCr - up_info->addYCr) >> 12); //-4;
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
    //int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j, shift = up_info->shift_up[0];
    int16_t*   dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src - x_BL;
    const int8_t*   coeff;
    for( i = 0; i < block_w; i++ )	{
        x        = i+x_EL;  //av_clip_c(i+x_EL, leftStartL, rightEndL);
        coeff    = up_sample_filter_luma_x2[x&0x01];
        dst_tmp  = _dst  + i;
        src_tmp  = src + ((x-leftStartL)>>1);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = (LumHor_FILTER_Block(src_tmp, coeff)>>shift);
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
        }
    }
}

static void FUNC(upsample_filter_block_luma_h_x2_8)( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    //int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j, shift = up_info->shift_up[0];
    int16_t*   dst_tmp;
    uint8_t*   src_tmp, *src = (uint8_t *) _src - x_BL;
    const int8_t*   coeff;
    for( i = 0; i < block_w; i++ )	{
        x        = i+x_EL;  //av_clip_c(i+x_EL, leftStartL, rightEndL);
        coeff    = up_sample_filter_luma_x2[x&0x01];
        dst_tmp  = _dst  + i;
        src_tmp  = src + ((x-leftStartL)>>1);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = (LumHor_FILTER_Block(src_tmp, coeff)>>shift);
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_x2)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    //int leftStartC = Enhscal->left_offset>>1;
    //int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j, shift = up_info->shift_up[1];
    int16_t*  dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src - x_BL;
    const int8_t*  coeff;
    
    for( i = 0; i < block_w; i++ )	{
        x        = i+x_EL; //av_clip_c(i+x_EL, leftStartC, rightEndC);
        coeff    = up_sample_filter_chroma_x2_h[x&0x01];
        dst_tmp  = dst  + i;
        src_tmp  = src + (x>>1);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  (CroHor_FILTER_Block(src_tmp, coeff)>>shift);
            src_tmp  +=  _srcstride;
            dst_tmp   +=  dststride;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_x2_8)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    //int leftStartC = Enhscal->left_offset>>1;
    //int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j, shift = up_info->shift_up[1];
    int16_t*  dst_tmp;
    uint8_t*   src_tmp, *src = (uint8_t *) _src - x_BL;
    const int8_t*  coeff;

    for( i = 0; i < block_w; i++ )	{
        x        = i+x_EL; //av_clip_c(i+x_EL, leftStartC, rightEndC);
        coeff    = up_sample_filter_chroma_x2_h[x&0x01];
        dst_tmp  = dst  + i;
        src_tmp  = src + (x>>1);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  (CroHor_FILTER_Block(src_tmp, coeff)>>shift);
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
    //int bottomEndL = heightEL - Enhscal->bottom_offset;
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int y, i, j;
    const int8_t  *   coeff;
    pixel *dst_tmp, *dst;
    int16_t *   src_tmp;

    _dststride /= sizeof(pixel);
    dst = (pixel *)_dst + y_EL * _dststride + x_EL;

    for( j = 0; j < block_h; j++ ) {
    	y        = y_EL+j; //av_clip_c(y_EL+j, topStartL, bottomEndL-1);
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
    //int bottomEndC = heightEL - (Enhscal->bottom_offset>>1);
    int y, i, j, refPos16, refPos;
    const int8_t* coeff;
    int16_t *   src_tmp;
    pixel *dst_tmp, *dst    = (pixel *)_dst;
    dststride /= sizeof(pixel);
    for( j = 0; j < block_h; j++ ) {
        y =   y_EL+j; //av_clip_c(y_EL+j, topStartC, bottomEndC-1);
        refPos16 = ((( y - topStartC )* up_info->scaleYCr - up_info->addYCr) >> 12); //-4;
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
    int x, i, j, shift = up_info->shift_up[0];
    int16_t*   dst_tmp;
    pixel*   src_tmp, *src = (pixel *) _src - x_BL;
    const int8_t*   coeff;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartL, rightEndL);
        coeff    = up_sample_filter_luma_x1_5[(x-leftStartL)%3];
        dst_tmp  = _dst  + i;
        src_tmp  = src + (((x-leftStartL)<<1)/3);

        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = (LumHor_FILTER_Block(src_tmp, coeff)>>shift);
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
        }
    }
}

static void FUNC(upsample_filter_block_luma_h_x1_5_8)( int16_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int rightEndL  = widthEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x, i, j, shift = up_info->shift_up[0];
    int16_t*   dst_tmp;
    uint8_t*   src_tmp, *src = (uint8_t *) _src - x_BL;
    const int8_t*   coeff;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartL, rightEndL);
        coeff    = up_sample_filter_luma_x1_5[(x-leftStartL)%3];
        dst_tmp  = _dst  + i;
        src_tmp  = src + (((x-leftStartL)<<1)/3);

        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp  = (LumHor_FILTER_Block(src_tmp, coeff)>>shift);
            src_tmp  += _srcstride;
            dst_tmp  += _dststride;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_x1_5)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int16_t*  dst_tmp;
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j;
    const int8_t*  coeff;

    int shift = up_info->shift_up[1];
    pixel*   src_tmp, *src;
    src = (pixel *) _src - x_BL;
    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartC, rightEndC);
        coeff    = up_sample_filter_chroma_x1_5_h[(x-leftStartC)%3];
        dst_tmp  = dst  + i;
        src_tmp  = src + (((x-leftStartC)<<1)/3);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  (CroHor_FILTER_Block(src_tmp, coeff)>>shift);
            src_tmp  +=  _srcstride;
            dst_tmp   +=  dststride;
        }
    }
}

static void FUNC(upsample_filter_block_cr_h_x1_5_8)(  int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                                  int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                                                  const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info) {
    int16_t*  dst_tmp;
    int leftStartC = Enhscal->left_offset>>1;
    int rightEndC  = widthEL - (Enhscal->right_offset>>1);
    int x, i, j, shift;
    uint8_t*   src_tmp, *src;
    const int8_t*  coeff;
    shift = up_info->shift_up[1];
    src = (uint8_t *) _src - x_BL;

    for( i = 0; i < block_w; i++ )	{
        x        = av_clip_c(i+x_EL, leftStartC, rightEndC);
        coeff    = up_sample_filter_chroma_x1_5_h[(x-leftStartC)%3];
        dst_tmp  = dst  + i;
        src_tmp  = src + (((x-leftStartC)<<1)/3);
        for( j = 0; j < block_h ; j++ ) {
            *dst_tmp   =  (CroHor_FILTER_Block(src_tmp, coeff)>>shift);
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
    pixel *dst_tmp, *dst;
    int16_t *   src_tmp;
    _dststride /= sizeof(pixel);
    dst    = (pixel *)_dst + x_EL + y_EL * _dststride;

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
    dststride /= sizeof(pixel);
    for ( j = 0; j < block_h; j++ ) {
        y        = av_clip_c(y_EL+j, topStartC, bottomEndC-1);
        refPos16 = ((( y - topStartC )* up_info->scaleYCr + up_info->addYCr) >> 12)-4;
        coeff    = up_sample_filter_chroma_x1_5_v[y%3];
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

    int widthBL =  FrameBL->width;
    int heightBL = FrameBL->height;
    int strideBL = FrameBL->linesize[0]/sizeof(pixel);
    int widthEL =  FrameEL->width - Enhscal->left_offset - Enhscal->right_offset;
    int heightEL = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
    int strideEL = FrameEL->linesize[0]/sizeof(pixel);
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
    int rightEndL  = FrameEL->width - Enhscal->right_offset;
    //int topStartL  = Enhscal->top_offset;
    //int bottomEndL = FrameEL->height - Enhscal->bottom_offset;
    pixel buffer[8];

    const int nShift = 20-BIT_DEPTH;// TO DO ass the appropiate bit depth  bit  depth

    int iOffset = 1 << (nShift - 1);
    short buffer1[8];

    int leftStartC = Enhscal->left_offset>>1;
    //int rightEndC  = (FrameEL->width>>1) - (Enhscal->right_offset>>1);
    int topStartC  = Enhscal->top_offset>>1;
    //int bottomEndC = (FrameEL->height>>1) - (Enhscal->bottom_offset>>1);
    int shift1 = up_info->shift_up[0];

    widthEL   = FrameEL->width;  //pcUsPic->getWidth ();
    heightEL  = FrameEL->height; //pcUsPic->getHeight();

    widthBL   = FrameBL->width;
    heightBL  = FrameBL->height <= heightEL ? FrameBL->height:heightEL;  // min( FrameBL->height, heightEL);

    for( i = 0; i < widthEL; i++ ) {
        int x = i; //av_clip_c(i, leftStartL, rightEndL);
        refPos16 = ((x *up_info->scaleXLum - up_info->addXLum) >> 12);
        phase    = refPos16 & 15;
        refPos   = refPos16 >> 4;
        coeff = up_sample_filter_luma[phase];
        refPos -= ((NTAPS_LUMA>>1) - 1);
        srcY = srcBufY + refPos;
        dstY1 = tempBufY + i;
        if(refPos < 0)
            for( j = 0; j < heightBL ; j++ ) {
                for(k=0; k<-refPos; k++ )
                  buffer[k] = srcY[-refPos];
                for(k=-refPos; k<NTAPS_LUMA; k++ )
                  buffer[k] = srcY[k];
                *dstY1 = LumHor_FILTER(buffer, coeff)>>shift1;
                srcY += strideBL;
                dstY1 += widthEL;//strideEL;
            } else if (refPos+8 > widthBL )
                for ( j = 0; j < heightBL ; j++ ) {
                    memcpy(buffer, srcY, (widthBL-refPos)*sizeof(pixel));
                    for(k=widthBL-refPos; k<NTAPS_LUMA; k++ )
                      buffer[k] = srcY[widthBL-refPos-1];
                    *dstY1 = LumHor_FILTER(buffer, coeff)>>shift1;
                    srcY += strideBL;
                    dstY1 += widthEL;//strideEL;
                } else
                    for ( j = 0; j < heightBL ; j++ ) {
                      *dstY1 = LumHor_FILTER(srcY, coeff)>>shift1;
                      srcY  += strideBL;
                      dstY1 += widthEL;//strideEL;
                    }
    }
    for ( j = 0; j < heightEL; j++ ) {
        int y = j; //av_clip_c(j, topStartL, bottomEndL-1);
        refPos16 = (( y *up_info->scaleYLum - up_info->addYLum) >> 12);

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
                    srcY1++;
                    dstY++;
                } else
                    for ( i = 0; i < widthEL; i++ ) {
                        *dstY = av_clip_pixel( (LumVer_FILTER1(srcY1, coeff, widthEL) + iOffset) >> (nShift));
                        srcY1++;
                        dstY++;
                    }
    }
    widthBL   = FrameBL->width;
    heightBL  = FrameBL->height;
    
    widthEL   = FrameEL->width - Enhscal->right_offset - Enhscal->left_offset;
    heightEL  = FrameEL->height - Enhscal->top_offset - Enhscal->bottom_offset;
    
    shift1 = up_info->shift_up[1];

    widthEL  >>= 1;
    heightEL >>= 1;
    widthBL  >>= 1;
    heightBL >>= 1;
    strideBL  = FrameBL->linesize[1]/sizeof(pixel);
    strideEL  = FrameEL->linesize[1]/sizeof(pixel);
    widthEL   = FrameEL->width >> 1;
    heightEL  = FrameEL->height >> 1;
    widthBL   = FrameBL->width >> 1;
    heightBL  = FrameBL->height > heightEL ? FrameBL->height:heightEL;
    
    
    heightBL >>= 1;
    
    //========== horizontal upsampling ===========
    for( i = 0; i < widthEL; i++ )	{
        int x = i; //av_clip_c(i, leftStartC, rightEndC - 1);
        refPos16 = (((x - leftStartC)*up_info->scaleXCr - up_info->addXCr) >> 12);
        phase    = refPos16 & 15;
        refPos   = refPos16 >> 4;
        coeff = up_sample_filter_chroma[phase];

        refPos -= ((NTAPS_CHROMA>>1) - 1);
        srcU = srcBufU + refPos; // -((NTAPS_CHROMA>>1) - 1);
        srcV = srcBufV + refPos; // -((NTAPS_CHROMA>>1) - 1);
        dstU1 = tempBufU + i;
        dstV1 = tempBufV + i;
        
        if(refPos < 0)
            for( j = 0; j < heightBL ; j++ ) {
                for(k=0; k < -refPos; k++)
                  buffer[k] = srcU[-refPos];
                for(k=-refPos; k < 4; k++)
                  buffer[k] = srcU[k];
                for(k=0; k < -refPos; k++)
                  buffer[k+4] = srcV[-refPos];
                for(k=-refPos; k < 4; k++)
                  buffer[k+4] = srcV[k];

                *dstU1 = CroHor_FILTER(buffer, coeff)>>shift1;
                *dstV1 = CroHor_FILTER((buffer+4), coeff)>>shift1;

                srcU += strideBL;
                srcV += strideBL;
                dstU1 += widthEL;
                dstV1 += widthEL;
            }else if(refPos+4 > widthBL )
                for( j = 0; j < heightBL ; j++ ) {
                    for(k=0; k < widthBL-refPos; k++)
                      buffer[k] = srcU[k];
                    for(k=0; k < 4-(widthBL-refPos); k++)
                      buffer[widthBL-refPos+k] = srcU[widthBL-refPos-1];

                    for(k=0; k < widthBL-refPos; k++)
                      buffer[k+4] = srcV[k];
                    for(k=0; k < 4-(widthBL-refPos); k++)
                      buffer[widthBL-refPos+k+4] = srcV[widthBL-refPos-1];

                    *dstU1 = CroHor_FILTER(buffer, coeff)>>shift1;
                    *dstV1 = CroHor_FILTER((buffer+4), coeff)>>shift1;
                    srcU += strideBL;
                    srcV += strideBL;
                    dstU1 += widthEL;
                    dstV1 += widthEL;
                } else
                    for ( j = 0; j < heightBL ; j++ ) {
                        *dstU1 = CroHor_FILTER(srcU, coeff)>>shift1;
                        *dstV1 = CroHor_FILTER(srcV, coeff)>>shift1;
                        srcU  += strideBL;
                        srcV  += strideBL;
                        dstU1 += widthEL;
                        dstV1 += widthEL;
                    }
    }
    for( j = 0; j < heightEL; j++ )	{
        int y = j; //av_clip_c(j, topStartC, bottomEndC - 1);
        refPos16 = (((y - topStartC)*up_info->scaleYCr - up_info->addYCr) >> 12); // - 4;
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

                srcU1++;
                srcV1++;
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
                    srcU1++;
                    srcV1++;

                    dstU++;
                    dstV++;
                } else
                    for ( i = 0; i < widthEL; i++ ) {
                        *dstU = av_clip_pixel( (CroVer_FILTER1(srcU1, coeff, widthEL) + iOffset) >> (nShift));
                        *dstV = av_clip_pixel( (CroVer_FILTER1(srcV1, coeff, widthEL) + iOffset) >> (nShift));

                        srcU1++;
                        srcV1++;
                        dstU++;
                        dstV++;
                    }
    }
}

#undef LumHor_FILTER
#undef LumCro_FILTER
#undef LumVer_FILTER
#undef CroVer_FILTER

static void FUNC(colorMapping)(void * pc3DAsymLUT_, struct AVFrame *src, struct AVFrame *dst) {

    TCom3DAsymLUT *pc3DAsymLUT = (TCom3DAsymLUT *)pc3DAsymLUT_;

    int i, j, k;

    const int width  = src->width;
    const int height = src->height;

    const int src_stride  = src->linesize[0]/sizeof(pixel);
    const int src_stridec = src->linesize[1]/sizeof(pixel);

    const int dst_stride  = dst->linesize[0]/sizeof(pixel);
    const int dst_stridec = dst->linesize[1]/sizeof(pixel);

    pixel srcYaver, tmpU, tmpV;

    pixel *src_Y = (pixel*)src->data[0];
    pixel *src_U = (pixel*)src->data[1];
    pixel *src_V = (pixel*)src->data[2];

    pixel *dst_Y = (pixel*)dst->data[0];
    pixel *dst_U = (pixel*)dst->data[1];
    pixel *dst_V = (pixel*)dst->data[2];

    pixel *src_U_prev = (pixel*)src->data[1];
    pixel *src_V_prev = (pixel*)src->data[2];

    pixel *src_U_next = (pixel*)src->data[1] + src_stridec;
    pixel *src_V_next = (pixel*)src->data[2] + src_stridec;

    const int octant_depth1 = pc3DAsymLUT->cm_octant_depth == 1 ? 1 : 0;

    const int YShift2Idx = pc3DAsymLUT->YShift2Idx;
    const int UShift2Idx = pc3DAsymLUT->UShift2Idx;
    const int VShift2Idx = pc3DAsymLUT->VShift2Idx;

    const int nAdaptCThresholdU = pc3DAsymLUT->nAdaptCThresholdU;
    const int nAdaptCThresholdV = pc3DAsymLUT->nAdaptCThresholdV;

    const int nMappingOffset = pc3DAsymLUT->nMappingOffset;
    const int nMappingShift  = pc3DAsymLUT->nMappingShift;

    const int iMaxValY = (1 << pc3DAsymLUT->cm_output_luma_bit_depth  ) - 1;
    const int iMaxValC = (1 << pc3DAsymLUT->cm_output_chroma_bit_depth) - 1;

    for(i = 0; i < height; i += 2){
        for(j = 0, k = 0; j < width; j += 2, k++){
            SCuboid rCuboid;
            SYUVP dstUV;
            short a, b;

            int knext = (k == (width >> 1) - 1) ? k : k+1;

            uint16_t val[6], val_dst[6], val_prev[2];

            val[0] = src_Y[j];
            val[1] = src_Y[j+1];

            val[2] = src_Y[j + src_stride];
            val[3] = src_Y[j + src_stride + 1];

            val[4] = src_U[k];
            val[5] = src_V[k];

            srcYaver = (val[0] + val[2] + 1 ) >> 1;;

            val_prev[0]  = src_U_prev[k];
            val_prev[1]  = src_V_prev[k];

            tmpU =  (val_prev[0] + val[4] + (val[4] << 1) + 2 ) >> 2;
            tmpV =  (val_prev[1] + val[5] + (val[5] << 1) + 2 ) >> 2;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[0] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[0] = ((rCuboid.P[0].Y * val[0] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            a = src_U[knext] + val[4];
            b = src_V[knext] + val[5];

            tmpU =  ((a << 1) + a + val_prev[0] + src_U_prev[knext] + 4 ) >> 3;
            tmpV =  ((b << 1) + b + val_prev[1] + src_V_prev[knext] + 4 ) >> 3;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[1] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[1] = ((rCuboid.P[0].Y * val[1] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            tmpU =  (src_U_next[k] + val[4] + (val[4]<<1) + 2 ) >> 2;
            tmpV =  (src_V_next[k] + val[5] + (val[5]<<1) + 2 ) >> 2;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[2] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[2] = ((rCuboid.P[0].Y * val[2] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            tmpU =  ((a << 1) + a + src_U_next[k] + src_U_next[knext] + 4 ) >> 3;
            tmpV =  ((b << 1) + b + src_V_next[k] + src_V_next[knext] + 4 ) >> 3;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[3] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[3] = ((rCuboid.P[0].Y * val[3] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            rCuboid = pc3DAsymLUT->S_Cuboid[srcYaver >> YShift2Idx]
                    [octant_depth1 ? val[4] >= nAdaptCThresholdU : val[4] >> UShift2Idx]
                    [octant_depth1 ? val[5] >= nAdaptCThresholdV : val[5] >> VShift2Idx];

            dstUV.Y = 0;

            dstUV.U = ((rCuboid.P[0].U * srcYaver + rCuboid.P[1].U * val[4]
                    + rCuboid.P[2].U * val[5] + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].U;

            dstUV.V = ((rCuboid.P[0].V * srcYaver + rCuboid.P[1].V * val[4]
                    + rCuboid.P[2].V * val[5] + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].V;

            dst_Y[j]     = av_clip(val_dst[0], 0, iMaxValY);
            dst_Y[j + 1] = av_clip(val_dst[1], 0, iMaxValY);

            dst_Y[j + dst_stride]     = av_clip(val_dst[2] , 0, iMaxValY);
            dst_Y[j + dst_stride + 1] = av_clip(val_dst[3] , 0, iMaxValY);

            dst_U[k] = av_clip(dstUV.U, 0, iMaxValC);
            dst_V[k] = av_clip(dstUV.V, 0, iMaxValC);
        }

        src_Y += src_stride << 1;

        src_U_prev = src_U;
        src_V_prev = src_V;

        src_U = src_U_next;
        src_V = src_V_next;

        if((i < height - 4)){
            src_U_next += src_stridec;
            src_V_next += src_stridec;
        }

        dst_Y += dst_stride << 1;
        dst_U += dst_stridec;
        dst_V += dst_stridec;
    }
}

static void FUNC(map_color_block)(const void *pc3DAsymLUT_,
                                   uint8_t *src_y, uint8_t *src_u, uint8_t *src_v,
                                   uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v,
                                   int src_stride, int src_stride_c,
                                   int dst_stride, int dst_stride_c,
                                   int dst_width, int dst_height,
                                   int is_bound_r,int is_bound_b, int is_bound_t,
                                   int is_bound_l){

    TCom3DAsymLUT *pc3DAsymLUT = (TCom3DAsymLUT *)pc3DAsymLUT_;

    int i, j, k;

//    const int width  = src->width;
//    const int height = src->height;

//    const int src_stride  = src->linesize[0]/sizeof(pixel);
//    const int src_stridec = src->linesize[1]/sizeof(pixel);

//    const int dst_stride  = dst->linesize[0]/sizeof(pixel);
//    const int dst_stridec = dst->linesize[1]/sizeof(pixel);

    pixel srcYaver, tmpU, tmpV;

    pixel *src_Y = (pixel*)src_y;
    pixel *src_U = (pixel*)src_u;
    pixel *src_V = (pixel*)src_v;

    pixel *dst_Y = (pixel*)dst_y;
    pixel *dst_U = (pixel*)dst_u;
    pixel *dst_V = (pixel*)dst_v;

    pixel *src_U_prev;
    pixel *src_V_prev;

    pixel *src_U_next = (pixel*)src_u + src_stride_c;
    pixel *src_V_next = (pixel*)src_v + src_stride_c;

    const int octant_depth1 = pc3DAsymLUT->cm_octant_depth == 1 ? 1 : 0;

    const int YShift2Idx = pc3DAsymLUT->YShift2Idx;
    const int UShift2Idx = pc3DAsymLUT->UShift2Idx;
    const int VShift2Idx = pc3DAsymLUT->VShift2Idx;

    const int nAdaptCThresholdU = pc3DAsymLUT->nAdaptCThresholdU;
    const int nAdaptCThresholdV = pc3DAsymLUT->nAdaptCThresholdV;

    const int nMappingOffset = pc3DAsymLUT->nMappingOffset;
    const int nMappingShift  = pc3DAsymLUT->nMappingShift;

    const int iMaxValY = (1 << pc3DAsymLUT->cm_output_luma_bit_depth  ) - 1;
    const int iMaxValC = (1 << pc3DAsymLUT->cm_output_chroma_bit_depth) - 1;

    if(!is_bound_t){
        src_U_prev = (pixel*)src_u - src_stride_c;
        src_V_prev = (pixel*)src_v - src_stride_c;
    } else {
        src_U_prev = (pixel*)src_u;
        src_V_prev = (pixel*)src_v;
    }

    for(i = 0; i < dst_height; i += 2){
        for(j = 0, k = 0; j < dst_width; j += 2, k++){
            SCuboid rCuboid;
            SYUVP dstUV;
            short a, b;

            int knext = (is_bound_r && (k == (dst_width >> 1) - 1)) ? k : k+1;

            int16_t val[6], val_dst[6], val_prev[2];

            val[0] = src_Y[j];
            val[1] = src_Y[j+1];

            val[2] = src_Y[j + src_stride];
            val[3] = src_Y[j + src_stride + 1];

            val[4] = src_U[k];
            val[5] = src_V[k];

            srcYaver = (val[0] + val[2] + 1 ) >> 1;;

            val_prev[0]  = src_U_prev[k];
            val_prev[1]  = src_V_prev[k];

            tmpU =  (val_prev[0] + val[4] + (val[4] << 1) + 2 ) >> 2;
            tmpV =  (val_prev[1] + val[5] + (val[5] << 1) + 2 ) >> 2;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[0] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[0] = ((rCuboid.P[0].Y * val[0] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            a = src_U[knext] + val[4];
            b = src_V[knext] + val[5];

            tmpU =  ((a << 1) + a + val_prev[0] + src_U_prev[knext] + 4 ) >> 3;
            tmpV =  ((b << 1) + b + val_prev[1] + src_V_prev[knext] + 4 ) >> 3;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[1] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[1] = ((rCuboid.P[0].Y * val[1] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            tmpU =  (src_U_next[k] + val[4] + (val[4]<<1) + 2 ) >> 2;
            tmpV =  (src_V_next[k] + val[5] + (val[5]<<1) + 2 ) >> 2;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[2] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[2] = ((rCuboid.P[0].Y * val[2] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            tmpU =  ((a << 1) + a + src_U_next[k] + src_U_next[knext] + 4 ) >> 3;
            tmpV =  ((b << 1) + b + src_V_next[k] + src_V_next[knext] + 4 ) >> 3;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[3] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[3] = ((rCuboid.P[0].Y * val[3] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            rCuboid = pc3DAsymLUT->S_Cuboid[srcYaver >> YShift2Idx]
                    [octant_depth1 ? val[4] >= nAdaptCThresholdU : val[4] >> UShift2Idx]
                    [octant_depth1 ? val[5] >= nAdaptCThresholdV : val[5] >> VShift2Idx];

            dstUV.Y = 0;

            dstUV.U = ((rCuboid.P[0].U * srcYaver + rCuboid.P[1].U * val[4]
                    + rCuboid.P[2].U * val[5] + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].U;

            dstUV.V = ((rCuboid.P[0].V * srcYaver + rCuboid.P[1].V * val[4]
                    + rCuboid.P[2].V * val[5] + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].V;

            dst_Y[j]     = av_clip(val_dst[0], 0, iMaxValY);
            dst_Y[j + 1] = av_clip(val_dst[1], 0, iMaxValY);

            dst_Y[j + dst_stride]     = av_clip(val_dst[2] , 0, iMaxValY);
            dst_Y[j + dst_stride + 1] = av_clip(val_dst[3] , 0, iMaxValY);

            dst_U[k] = av_clip(dstUV.U, 0, iMaxValC);
            dst_V[k] = av_clip(dstUV.V, 0, iMaxValC);
        }

        src_Y += src_stride << 1;

        src_U_prev = src_U;
        src_V_prev = src_V;

        src_U = src_U_next;
        src_V = src_V_next;

        if(!is_bound_b || (is_bound_b && (i < dst_height - 4))){
            src_U_next += src_stride_c;
            src_V_next += src_stride_c;
        }

        dst_Y += dst_stride << 1;
        dst_U += dst_stride_c;
        dst_V += dst_stride_c;
    }
}

static void FUNC(map_color_block_8)(const void *pc3DAsymLUT_,
                                   uint8_t *src_y, uint8_t *src_u, uint8_t *src_v,
                                   uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v,
                                   int src_stride, int src_stride_c,
                                   int dst_stride, int dst_stride_c,
                                   int dst_width, int dst_height,
                                   int is_bound_r,int is_bound_b, int is_bound_t,
                                   int is_bound_l){

    TCom3DAsymLUT *pc3DAsymLUT = (TCom3DAsymLUT *)pc3DAsymLUT_;

    int i, j, k;

//    const int width  = src->width;
//    const int height = src->height;

//    const int src_stride  = src->linesize[0]/sizeof(pixel);
//    const int src_stridec = src->linesize[1]/sizeof(pixel);

//    const int dst_stride  = dst->linesize[0]/sizeof(pixel);
//    const int dst_stridec = dst->linesize[1]/sizeof(pixel);

    pixel srcYaver, tmpU, tmpV;

    uint8_t *src_Y = (uint8_t*)src_y;
    uint8_t *src_U = (uint8_t*)src_u;
    uint8_t *src_V = (uint8_t*)src_v;

    uint16_t *dst_Y = (uint16_t*)dst_y;
    uint16_t *dst_U = (uint16_t*)dst_u;
    uint16_t *dst_V = (uint16_t*)dst_v;

    uint8_t *src_U_prev;
    uint8_t *src_V_prev;

    uint8_t *src_U_next = (uint8_t*)src_u + src_stride_c;
    uint8_t *src_V_next = (uint8_t*)src_v + src_stride_c;

    const int octant_depth1 = pc3DAsymLUT->cm_octant_depth == 1 ? 1 : 0;

    const int YShift2Idx = pc3DAsymLUT->YShift2Idx;
    const int UShift2Idx = pc3DAsymLUT->UShift2Idx;
    const int VShift2Idx = pc3DAsymLUT->VShift2Idx;

    const int nAdaptCThresholdU = pc3DAsymLUT->nAdaptCThresholdU;
    const int nAdaptCThresholdV = pc3DAsymLUT->nAdaptCThresholdV;

    const int nMappingOffset = pc3DAsymLUT->nMappingOffset;
    const int nMappingShift  = pc3DAsymLUT->nMappingShift;

    const int iMaxValY = (1 << pc3DAsymLUT->cm_output_luma_bit_depth  ) - 1;
    const int iMaxValC = (1 << pc3DAsymLUT->cm_output_chroma_bit_depth) - 1;

    if(!is_bound_t){
        src_U_prev = (uint8_t*)src_u - src_stride_c;
        src_V_prev = (uint8_t*)src_v - src_stride_c;
    } else {
        src_U_prev = (uint8_t*)src_u;
        src_V_prev = (uint8_t*)src_v;
    }

    for(i = 0; i < dst_height; i += 2){
        for(j = 0, k = 0; j < dst_width; j += 2, k++){
            SCuboid rCuboid;
            SYUVP dstUV;
            short a, b;

            int knext = (is_bound_r && (k == (dst_width >> 1) - 1)) ? k : k+1;

            int16_t val[6], val_dst[6], val_prev[2];

            val[0] = src_Y[j];
            val[1] = src_Y[j+1];

            val[2] = src_Y[j + src_stride];
            val[3] = src_Y[j + src_stride + 1];

            val[4] = src_U[k];
            val[5] = src_V[k];

            srcYaver = (val[0] + val[2] + 1 ) >> 1;;

            val_prev[0]  = src_U_prev[k];
            val_prev[1]  = src_V_prev[k];

            tmpU =  (val_prev[0] + val[4] + (val[4] << 1) + 2 ) >> 2;
            tmpV =  (val_prev[1] + val[5] + (val[5] << 1) + 2 ) >> 2;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[0] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[0] = ((rCuboid.P[0].Y * val[0] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            a = src_U[knext] + val[4];
            b = src_V[knext] + val[5];

            tmpU =  ((a << 1) + a + val_prev[0] + src_U_prev[knext] + 4 ) >> 3;
            tmpV =  ((b << 1) + b + val_prev[1] + src_V_prev[knext] + 4 ) >> 3;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[1] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[1] = ((rCuboid.P[0].Y * val[1] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            tmpU =  (src_U_next[k] + val[4] + (val[4]<<1) + 2 ) >> 2;
            tmpV =  (src_V_next[k] + val[5] + (val[5]<<1) + 2 ) >> 2;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[2] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[2] = ((rCuboid.P[0].Y * val[2] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            tmpU =  ((a << 1) + a + src_U_next[k] + src_U_next[knext] + 4 ) >> 3;
            tmpV =  ((b << 1) + b + src_V_next[k] + src_V_next[knext] + 4 ) >> 3;

            rCuboid = pc3DAsymLUT->S_Cuboid[val[3] >> YShift2Idx]
                    [octant_depth1 ? tmpU >= nAdaptCThresholdU : tmpU >> UShift2Idx]
                    [octant_depth1 ? tmpV >= nAdaptCThresholdV : tmpV >> VShift2Idx];

            val_dst[3] = ((rCuboid.P[0].Y * val[3] + rCuboid.P[1].Y * tmpU
                    + rCuboid.P[2].Y * tmpV + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].Y;

            rCuboid = pc3DAsymLUT->S_Cuboid[srcYaver >> YShift2Idx]
                    [octant_depth1 ? val[4] >= nAdaptCThresholdU : val[4] >> UShift2Idx]
                    [octant_depth1 ? val[5] >= nAdaptCThresholdV : val[5] >> VShift2Idx];

            dstUV.Y = 0;

            dstUV.U = ((rCuboid.P[0].U * srcYaver + rCuboid.P[1].U * val[4]
                    + rCuboid.P[2].U * val[5] + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].U;

            dstUV.V = ((rCuboid.P[0].V * srcYaver + rCuboid.P[1].V * val[4]
                    + rCuboid.P[2].V * val[5] + nMappingOffset) >> nMappingShift)
                    + rCuboid.P[3].V;

            dst_Y[j]     = av_clip(val_dst[0], 0, iMaxValY);
            dst_Y[j + 1] = av_clip(val_dst[1], 0, iMaxValY);

            dst_Y[j + dst_stride]     = av_clip(val_dst[2] , 0, iMaxValY);
            dst_Y[j + dst_stride + 1] = av_clip(val_dst[3] , 0, iMaxValY);

            dst_U[k] = av_clip(dstUV.U, 0, iMaxValC);
            dst_V[k] = av_clip(dstUV.V, 0, iMaxValC);
        }

        src_Y += src_stride << 1;

        src_U_prev = src_U;
        src_V_prev = src_V;

        src_U = src_U_next;
        src_V = src_V_next;

        if(!is_bound_b || (is_bound_b && (i < dst_height - 4))){
            src_U_next += src_stride_c;
            src_V_next += src_stride_c;
        }

        dst_Y += dst_stride << 1;
        dst_U += dst_stride_c;
        dst_V += dst_stride_c;
    }
}
#endif
