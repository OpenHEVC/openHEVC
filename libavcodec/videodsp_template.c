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

#include <assert.h>

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

static int FUNC(ff_emulated_edge_up_h)(uint8_t *src, ptrdiff_t linesize,
                                    const struct HEVCWindow *Enhscal,
                                    int block_w, int block_h, int bl_edge_left, int bl_edge_right, int shift)
{
    int i;
    uint8_t   *src_tmp = src;
    
    if(bl_edge_left < shift) {
      //  printf("------------ bl_edge_left %d \n", bl_edge_left);
        for(i=0; i < block_h; i++) {
            memset(src_tmp-(shift), src_tmp[0], shift);
            src_tmp += linesize;
        }
        return 0;
    }
    
    if(bl_edge_right<(shift+1)) {
        //printf("------------  bl_edge_right %d \n", bl_edge_right);
        for( i = 0; i < block_h ; i++ ) {
            memset(src_tmp+block_w,               src_tmp[block_w-1], shift+1);
            src_tmp += linesize;
        }
    }
    return 1;
}


static int FUNC(ff_emulated_edge_up_v)(int16_t *src, ptrdiff_t linesize,
                                    const struct HEVCWindow *Enhscal,
                                    int block_w, int block_h, int src_x, int bl_edge_up, int bl_edge_bottom, int wEL, int shift)
{
    int rightEndL  = wEL - (Enhscal->right_offset >> (shift==(MAX_EDGE_CR-1)?1:0));
    int leftStartL = (Enhscal->left_offset>> (shift==(MAX_EDGE_CR-1)));
    int  i, j;
    
    int16_t *src_tmp = src;
    int16_t *dst     = src;
    
    if(bl_edge_up < shift)  {
      //  printf("------------ bl_edge_up %d \n", bl_edge_up);
        for( i = 0; i < block_w; i++ )	{
            for(j= bl_edge_up; j<(shift) ; j++)
                dst[(-j-1)*linesize] = src_tmp[-bl_edge_up*linesize];
            if( ((src_x+i) >= leftStartL) && ((src_x+i) <= rightEndL-2) )
                src_tmp++;
            dst++; 
        }
        return 0;
    }
    
    if(bl_edge_bottom < (shift+1) )    {
       // printf("------------ bl_edge_bottom %d \n", bl_edge_bottom);
        for( i = 0; i < block_w; i++ )	{
            for(j= 0; j< shift+1 ; j++){
                dst[(block_h+j)*linesize] = src_tmp[(block_h-1)*linesize];
            }
            if( ((src_x+i) >= leftStartL) && ((src_x+i) <= rightEndL-2) )
                src_tmp++;
            dst++;
        }
    }

    return 1;
}
