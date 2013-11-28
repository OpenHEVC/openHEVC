/*
 * Copyright (c) 2002-2004 Michael Niedermayer
 * Copyright (C) 2012 Ronald S. Bultje
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

#include "assert.h"
#include "bit_depth_template.c"
#include "hevc_up_sample_filter.h"
#include "hevc.h"
static void FUNC(ff_emulated_edge_mc)(uint8_t *buf, const uint8_t *src,
                                      ptrdiff_t linesize,
                                      int block_w, int block_h,
                                      int src_x, int src_y, int w, int h)
{
    int x, y;
    int start_y, start_x, end_y, end_x;

    if (src_y >= h) {
        src  += (h - 1 - src_y) * linesize;
        src_y = h - 1;
    } else if (src_y <= -block_h) {
        src  += (1 - block_h - src_y) * linesize;
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
    assert(start_y < end_y && block_h);
    assert(start_x < end_x && block_w);

    w    = end_x - start_x;
    src += start_y * linesize + start_x * sizeof(pixel);
    buf += start_x * sizeof(pixel);

    // top
    for (y = 0; y < start_y; y++) {
        memcpy(buf, src, w * sizeof(pixel));
        buf += linesize;
    }

    // copy existing part
    for (; y < end_y; y++) {
        memcpy(buf, src, w * sizeof(pixel));
        src += linesize;
        buf += linesize;
    }

    // bottom
    src -= linesize;
    for (; y < block_h; y++) {
        memcpy(buf, src, w * sizeof(pixel));
        buf += linesize;
    }

    buf -= block_h * linesize + start_x * sizeof(pixel);
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
        buf += linesize;
    }
}

static int FUNC(ff_emulated_edge_up_h)(uint8_t *buf, const uint8_t *src, ptrdiff_t linesize,
                                    struct HEVCWindow *Enhscal, struct UpsamplInf *up_info,
                                    int block_w, int block_h, int src_x, int wBL, int wEL)
{
    int rightEndL  = wEL - Enhscal->right_offset;
    int leftStartL = Enhscal->left_offset;
    int x = av_clip_c(src_x, leftStartL, rightEndL);
    int refPos16, refPos, i;
    uint8_t *buf_tmp = buf;
    const uint8_t *src_tmp = src;
    
    refPos16 = (((x - leftStartL)*up_info->scaleXLum + up_info->addXLum) >> 12);
    refPos   = refPos16 >> 4;
    refPos -= ((NTAPS_LUMA>>1) - 1);
    if(refPos < 0) {
        for(i=0; i < block_h; i++) {
            memset(buf_tmp, src_tmp[0], -refPos);
            memcpy(buf_tmp-refPos, src_tmp, block_w);
            src_tmp += linesize;
            buf_tmp += linesize;
        }
        return 1;
    }
    if(refPos+NTAPS_LUMA > wBL ){
        buf_tmp = buf;
        src_tmp = src;
        for( i = 0; i < block_h ; i++ )    {
            memcpy(buf_tmp, src_tmp, block_w-refPos);
            memset(buf_tmp+(block_w-refPos), src_tmp[block_w-refPos-1], NTAPS_LUMA-(block_w-refPos));
            src_tmp += linesize;
            buf_tmp += linesize;
        }
        return 1;
    }
    return 0;
}


static int FUNC(ff_emulated_edge_up_v)(short *buf, const short *src, ptrdiff_t linesize,
                                    struct HEVCWindow *Enhscal, struct UpsamplInf *up_info,
                                    int block_w, int block_h, int src_y, int hBL, int hEL)
{
    int topStartL  = Enhscal->top_offset;
    int bottomEndL = hEL - Enhscal->bottom_offset;
    int refPos16, refPos, i, j;
    short *buf_tmp = buf;
    const short *src_tmp = src;
    int y = av_clip_c(src_y, topStartL, bottomEndL-1);
    refPos16 = ((( y - topStartL )*up_info->scaleYLum + up_info->addYLum) >> 12);
    refPos   = refPos16 >> 4;
    refPos -= ((NTAPS_LUMA>>1) - 1);
    if(refPos < 0)  {
        for( i = 0; i < block_w; i++ )	{
            for(j= 0; j<-refPos ; j++)
                buf_tmp[j*linesize] = src_tmp[-refPos*linesize];
            for(j= 0; j< block_h ; j++)
                buf_tmp[(-refPos+j)*linesize] = src_tmp[(-refPos+j)*linesize];
            //if( (i >= leftStartL) && (i <= rightEndL-2) )
            src_tmp++;
            buf_tmp++; 
        }
        return 1;
    }
    if(refPos+NTAPS_LUMA > hBL )    {
        for( i = 0; i < block_w; i++ )	{   // block_w for EL
            for(j= 0; j< block_h ; j++)
                buf_tmp[j*linesize] = src_tmp[j*linesize];
            for(j= 0; j<8-(hBL-refPos) ; j++)
                buf_tmp[(hBL-refPos+j)*linesize] = src_tmp[(hBL-refPos-1)*linesize];
            //if( (i >= leftStartL) && (i <= rightEndL-2) )
            src_tmp++;
            buf_tmp++;
        }
        return 1;
    }
    return 0;
}