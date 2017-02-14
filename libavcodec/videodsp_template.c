/*
 * Copyright (c) 2002-2012 Michael Niedermayer
 * Copyright (C) 2012 Ronald S. Bultje
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

#include "bit_depth_template.c"
#include "hevc.h"
void FUNC(ff_emulated_edge_mc)(uint8_t *buf, const uint8_t *src,
                               ptrdiff_t buf_linesize,
                               ptrdiff_t src_linesize,
                               int block_w, int block_h,
                               int src_x, int src_y, int w, int h)
{
    int x, y;
    int start_y, start_x, end_y, end_x;

    if (!w || !h)
        return;

    av_assert2(block_w * sizeof(pixel) <= FFABS(buf_linesize));

    if (src_y >= h) {
        src -= src_y * src_linesize;
        src += (h - 1) * src_linesize;
        src_y = h - 1;
    } else if (src_y <= -block_h) {
        src -= src_y * src_linesize;
        src += (1 - block_h) * src_linesize;
        src_y = 1 - block_h;
    }
    if (src_x >= w) {
        src  += (w - 1 - src_x) * sizeof(pixel);
        src_x = w - 1;
    } else if (src_x <= -block_w) {
        src  += (1 - block_w - src_x) * sizeof(pixel);
        src_x = 1 - block_w;
    }

    start_y = FFMAX(0, -src_y);
    start_x = FFMAX(0, -src_x);
    end_y = FFMIN(block_h, h-src_y);
    end_x = FFMIN(block_w, w-src_x);
    av_assert2(start_y < end_y && block_h);
    av_assert2(start_x < end_x && block_w);

    w    = end_x - start_x;
    src += start_y * src_linesize + start_x * sizeof(pixel);
    buf += start_x * sizeof(pixel);

    // top
    for (y = 0; y < start_y; y++) {
        memcpy(buf, src, w * sizeof(pixel));
        buf += buf_linesize;
    }

    // copy existing part
    for (; y < end_y; y++) {
        memcpy(buf, src, w * sizeof(pixel));
        src += src_linesize;
        buf += buf_linesize;
    }

    // bottom
    src -= src_linesize;
    for (; y < block_h; y++) {
        memcpy(buf, src, w * sizeof(pixel));
        buf += buf_linesize;
    }

    buf -= block_h * buf_linesize + start_x * sizeof(pixel);
    while (block_h--) {
        pixel *bufp = (pixel *) buf;

        // left
        for(x = 0; x < start_x; x++) {
            bufp[x] = bufp[start_x];
        }

        // right
        for (x = end_x; x < block_w; x++) {
            bufp[x] = bufp[end_x - 1];
        }
        buf += buf_linesize;
    }
}

static void FUNC(ff_emulated_edge_up_cgs_h)(uint8_t *src,
                                       int src_width, int src_height,
                                       int edge_left, int edge_right)
{
    int i;
    pixel   *src_tmp = (pixel *)src;
    pixel   *dst_tmp = (pixel *)src;

    if(edge_left){
        dst_tmp -= MAX_EDGE;
        for (i = 0; i <= src_height; ++i){
            dst_tmp[0]=*src_tmp;
            dst_tmp[1]=*src_tmp;
            dst_tmp[2]=*src_tmp;
            dst_tmp[3]=*src_tmp;
            dst_tmp += MAX_EDGE_BUFFER_STRIDE;
            src_tmp += MAX_EDGE_BUFFER_STRIDE;
        }
    }

    if(edge_right) {
        src_tmp = (pixel *)src;
        dst_tmp = (pixel *)src;
        src_tmp += src_width;
        dst_tmp =  src_tmp + 1;
        for (i = 0; i <= src_height; ++i){
            dst_tmp[0]=*src_tmp;
            dst_tmp[1]=*src_tmp;
            dst_tmp[2]=*src_tmp;
            dst_tmp[3]=*src_tmp;
            dst_tmp += MAX_EDGE_BUFFER_STRIDE;
            src_tmp += MAX_EDGE_BUFFER_STRIDE;
        }
    }
}

static void FUNC(ff_emulated_edge_up_h)(uint8_t *dst, uint8_t *src, ptrdiff_t linesize,
                                    int block_w, int block_h,
                                    int bl_edge_left, int bl_edge_right)
{
    int i, j;

    pixel   *src_tmp = (pixel *)src;
    pixel   *dst_tmp = (pixel *)dst;

    if(bl_edge_left){
        for( i = 0; i < block_h; i++) {
            for(j = 0; j < MAX_EDGE - 1; j++){
                dst_tmp--;
                *dst_tmp = *src_tmp;
            }
            dst_tmp += MAX_EDGE - 1;
            memcpy(dst_tmp, src_tmp, block_w * sizeof(pixel));
            src_tmp += linesize;
            dst_tmp += MAX_EDGE_BUFFER_STRIDE;
        }
    }

    if(bl_edge_right){
        for( i = 0; i < block_h; i++) {
            memcpy(dst_tmp, src_tmp, (block_w) * sizeof(pixel));
            src_tmp += block_w - 1;
            dst_tmp += block_w;
            for(j = 0; j < MAX_EDGE; j++){
                *dst_tmp = *src_tmp;
                dst_tmp++;
            }
            src_tmp += linesize - block_w + 1;
            dst_tmp += MAX_EDGE_BUFFER_STRIDE - block_w - MAX_EDGE ;
        }
    }
}

static void FUNC(ff_emulated_edge_up_cr_h)(uint8_t *dst, uint8_t *src, ptrdiff_t linesize,
                                          int block_w, int block_h,
                                          int bl_edge_left, int bl_edge_right)
{
    int i, j;

    pixel   *src_tmp = (pixel *)src;
    pixel   *dst_tmp = (pixel *)dst;

    if(bl_edge_left){
        for( i = 0; i < block_h; i++) {
            for(j = 0; j < MAX_EDGE_CR; j++){
                dst_tmp--;
                *dst_tmp = *src_tmp;
            }
            dst_tmp += MAX_EDGE_CR;
            memcpy(dst_tmp, src_tmp, block_w * sizeof(pixel));
            src_tmp += linesize;
            dst_tmp += MAX_EDGE_BUFFER_STRIDE;
        }
    }

    if(bl_edge_right){
        for( i = 0; i < block_h; i++) {
            memcpy(dst_tmp, src_tmp, (block_w) * sizeof(pixel));
            src_tmp += block_w-1;
            dst_tmp += block_w;
            for(j = 0; j < MAX_EDGE_CR; j++){
                *dst_tmp = *src_tmp;
                dst_tmp++;
            }
            src_tmp += linesize - block_w + 1;
            dst_tmp += MAX_EDGE_BUFFER_STRIDE - block_w - MAX_EDGE_CR;
        }
    }
}

static void FUNC(ff_emulated_edge_up_v)(uint8_t *src,
                                    int block_w, int block_h,
                                    int bl_edge_top, int bl_edge_bottom)
{
    int j;

    uint16_t   *src_tmp = (uint16_t *)src;
    uint16_t   *dst_tmp = (uint16_t *)src;

    if (bl_edge_top){
        for(j = 0; j < MAX_EDGE - 1; j++){
            dst_tmp -= MAX_EDGE_BUFFER_STRIDE;
            memcpy(dst_tmp, src_tmp, block_w * sizeof(uint16_t));
        }
        dst_tmp += (MAX_EDGE - 1) * MAX_EDGE_BUFFER_STRIDE;
    }

    if (bl_edge_bottom){
        dst_tmp += (block_h    ) * MAX_EDGE_BUFFER_STRIDE;
        src_tmp += (block_h - 1) * MAX_EDGE_BUFFER_STRIDE;
        for(j = 0; j < MAX_EDGE; j++){
            memcpy(dst_tmp, src_tmp, block_w * sizeof(uint16_t));
            dst_tmp += MAX_EDGE_BUFFER_STRIDE;
        }
    }
}

static void FUNC(ff_emulated_edge_up_cr_v)(uint8_t *src, int block_w, int block_h,
                                          int bl_edge_top, int bl_edge_bottom)
{
    int j;

    uint16_t   *src_tmp = (uint16_t *)src;
    uint16_t   *dst_tmp = (uint16_t *)src;

    if (bl_edge_top){
        for(j = 0; j < MAX_EDGE_CR; j++){
            dst_tmp -= MAX_EDGE_BUFFER_STRIDE;
            memcpy(dst_tmp, src_tmp, block_w * sizeof(uint16_t));
        }
        dst_tmp += (MAX_EDGE_CR) * MAX_EDGE_BUFFER_STRIDE;
    }

    if (bl_edge_bottom){
        dst_tmp += (block_h    ) * MAX_EDGE_BUFFER_STRIDE;
        src_tmp += (block_h - 1) * MAX_EDGE_BUFFER_STRIDE;
        for(j = 0; j < MAX_EDGE_CR; j++){
            memcpy(dst_tmp, src_tmp, block_w * sizeof(uint16_t));
            dst_tmp += MAX_EDGE_BUFFER_STRIDE;
        }
    }
}

static void FUNC(ff_copy_block)(uint8_t *src, uint8_t * dst, ptrdiff_t bl_stride, ptrdiff_t el_stride, int ePbH, int ePbW) {
    int i;
    for (i = 0; i < ePbH ; i++) {
        memcpy(dst, src, ePbW * sizeof(pixel));
        src += bl_stride * sizeof(pixel);
        dst += el_stride * sizeof(pixel);
    }
}
