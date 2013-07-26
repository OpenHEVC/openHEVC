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

#include "libavutil/pixdesc.h"
#include "bit_depth_template.c"
#include "hevcpred.h"

#define POS(x, y) src[(x) + stride * (y)]

static void FUNC(intra_pred)(HEVCContext *s, int x0, int y0, int log2_size, int c_idx)
{
#define IS_INTRA(x, y) (sc->ref->tab_mvf[((x0+((x)<<hshift)) >> sc->sps->log2_min_pu_size) + (((y0+((y)<<vshift))>> sc->sps->log2_min_pu_size) * pic_width_in_min_pu)].is_intra || !sc->pps->constrained_intra_pred_flag)
#define MIN_TB_ADDR_ZS(x, y)                            \
    sc->pps->min_tb_addr_zs[(y) * sc->sps->pic_width_in_min_tbs + (x)]
#define EXTEND_LEFT(ptr, start, length)                 \
        for (i = (start); i > (start)-(length); i--)    \
            ptr[i-1] = ptr[i]
#define EXTEND_RIGHT(ptr, start, length)                \
        for (i = (start); i < (start)+(length); i++)    \
            ptr[i] = ptr[i-1]
#define EXTEND_UP(ptr, start, length)   EXTEND_LEFT(ptr, start, length)
#define EXTEND_DOWN(ptr, start, length) EXTEND_RIGHT(ptr, start, length)
#define EXTEND_LEFT_CIP(ptr, start, length)             \
        for (i = (start); i > (start)-(length); i--)    \
            if (!IS_INTRA(i-1, -1))           \
                ptr[i-1] = ptr[i]
#define EXTEND_RIGHT_CIP(ptr, start, length)            \
        for (i = (start); i < (start)+(length); i++)    \
            if (!IS_INTRA(i, -1))             \
                ptr[i] = ptr[i-1]
#define EXTEND_UP_CIP(ptr, start, length)               \
        for (i = (start); i > (start)-(length); i--)    \
            if (!IS_INTRA(-1, i-1))           \
                ptr[i-1] = ptr[i]
#define EXTEND_DOWN_CIP(ptr, start, length)             \
        for (i = (start); i < (start)+(length); i++)    \
            if (!IS_INTRA(-1, i))             \
            ptr[i] = ptr[i-1]
    HEVCSharedContext *sc = s->HEVCsc;
    HEVCLocalContext *lc = s->HEVClc;
    int i;
    int hshift = sc->sps->hshift[c_idx];
    int vshift = sc->sps->vshift[c_idx];
    int size = (1 << log2_size);
    int size_in_luma = size << hshift;
    int size_in_tbs = size_in_luma >> sc->sps->log2_min_transform_block_size;
    int x = x0 >> hshift;
    int y = y0 >> vshift;
    int x_tb = x0 >> sc->sps->log2_min_transform_block_size;
    int y_tb = y0 >> sc->sps->log2_min_transform_block_size;
    int cur_tb_addr = MIN_TB_ADDR_ZS(x_tb, y_tb);

    ptrdiff_t stride = sc->frame->linesize[c_idx] / sizeof(pixel);
    pixel *src = (pixel*)sc->frame->data[c_idx] + x + y * stride;

    int pic_width_in_min_pu = s->HEVCsc->sps->pic_width_in_luma_samples >> s->HEVCsc->sps->log2_min_pu_size;

    enum IntraPredMode mode = c_idx ? lc->pu.intra_pred_mode_c :
                              lc->tu.cur_intra_pred_mode;

    pixel left_array[2*MAX_TB_SIZE+1], filtered_left_array[2*MAX_TB_SIZE+1];
    pixel top_array[2*MAX_TB_SIZE+1], filtered_top_array[2*MAX_TB_SIZE+1];
    pixel *left = left_array + 1;
    pixel *top = top_array + 1;
    pixel *filtered_left = filtered_left_array + 1;
    pixel *filtered_top = filtered_top_array + 1;

    int x0b = x0 & ((1 << sc->sps->log2_ctb_size) - 1);
    int y0b = y0 & ((1 << sc->sps->log2_ctb_size) - 1);

    int cand_up       = (lc->ctb_up_flag || y0b);
    int cand_up_right = ((x0b + size_in_luma) == (1 << sc->sps->log2_ctb_size)) ? lc->ctb_up_right_flag && !y0b: cand_up;
    int cand_left     = (lc->ctb_left_flag || x0b);

    int bottom_left_available = cand_left && (y_tb + size_in_tbs) < (lc->end_of_tiles_y>>sc->sps->log2_min_transform_block_size) &&
                                cur_tb_addr > MIN_TB_ADDR_ZS(x_tb - 1, y_tb + size_in_tbs);
    int left_available = cand_left;
    int top_left_available = (!x0b && !y0b) ? lc->ctb_up_left_flag : cand_left && cand_up;
    int top_available = cand_up;
    //FIXME : top_right_available can be available even if cand_up is not 
    int top_right_available = cand_up_right && (x_tb + size_in_tbs) < (lc->end_of_tiles_x>>sc->sps->log2_min_transform_block_size) &&
                              cur_tb_addr > MIN_TB_ADDR_ZS(x_tb + size_in_tbs, y_tb - 1);

    int bottom_left_size = (FFMIN(y0 + 2*size_in_luma, sc->sps->pic_height_in_luma_samples) -
                            (y0 + size_in_luma)) >> vshift;
    int top_right_size = (FFMIN(x0 + 2*size_in_luma, sc->sps->pic_width_in_luma_samples) -
                          (x0 + size_in_luma)) >> hshift;

    if (sc->pps->constrained_intra_pred_flag == 1) {
        int x_left_pu   = (x0-1) >> sc->sps->log2_min_pu_size;
        int y_left_pu   = (y0  ) >> sc->sps->log2_min_pu_size;
        int x_top_pu    = (x0  ) >> sc->sps->log2_min_pu_size;
        int y_top_pu    = (y0-1) >> sc->sps->log2_min_pu_size;
        int x_right_pu  = (x0+size_in_luma) >> sc->sps->log2_min_pu_size;
        int y_bottom_pu = (y0+size_in_luma) >> sc->sps->log2_min_pu_size;
        int size_in_luma_pu = size_in_luma >> sc->sps->log2_min_pu_size;
        if (bottom_left_available == 1) {
            bottom_left_available = 0;
            for(i=0; i< size_in_luma_pu; i++)
                bottom_left_available |= sc->ref->tab_mvf[x_left_pu + (y_bottom_pu+i) * pic_width_in_min_pu].is_intra;
        }
        if (left_available == 1) {
            left_available = 0;
            for(i=0; i< size_in_luma_pu; i++)
                left_available |= sc->ref->tab_mvf[x_left_pu + (y_left_pu+i) * pic_width_in_min_pu].is_intra;
        }
        if (top_left_available == 1)
            top_left_available = sc->ref->tab_mvf[x_left_pu + y_top_pu * pic_width_in_min_pu].is_intra;
        if (top_available == 1) {
            top_available = 0;
            for(i=0; i< size_in_luma_pu; i++)
                top_available |= sc->ref->tab_mvf[(x_top_pu+i) + y_top_pu * pic_width_in_min_pu].is_intra;
        }
        if (top_right_available == 1) {
            top_right_available = 0;
            for(i=0; i< size_in_luma_pu; i++)
                top_right_available |= sc->ref->tab_mvf[(x_right_pu+i) + y_top_pu * pic_width_in_min_pu].is_intra;
        }
        for (i = 0; i < 2*MAX_TB_SIZE+1; i++) {
            left[i] = 128;
            top[i]  = 128;
        }
    }
    if (bottom_left_available) {
        for (i = size + bottom_left_size; i < (size<<1); i++)
            if (IS_INTRA(-1, size + bottom_left_size - 1))
                left[i] = POS(-1, size + bottom_left_size - 1);
        for (i = size + bottom_left_size - 1; i >= size; i--)
            if (IS_INTRA(-1, i))
                left[i] = POS(-1, i);
    }
    if (left_available)
        for (i = size - 1; i >= 0; i--)
            if (IS_INTRA(-1, i))
                left[i] = POS(-1, i);
    if (top_left_available)
        if (IS_INTRA(-1, -1)) {
            left[-1] = POS(-1, -1);
            top[-1]  = left[-1];
        }
    if (top_available)
        for (i = size-1; i >= 0; i--)
            if (IS_INTRA(i, -1))
                top[i] = POS(i, -1);
    if (top_right_available) {
        for (i = size+top_right_size; i < (size<<1); i++)
            if (IS_INTRA(size + top_right_size - 1, -1))
                top[i] = POS(size + top_right_size - 1, -1);
        for (i = size+top_right_size-1; i >= size; i--)
            if (IS_INTRA(i, -1))
                top[i] = POS(i, -1);
   }

    if (sc->pps->constrained_intra_pred_flag == 1) {
        if (bottom_left_available || left_available || top_left_available || top_available || top_right_available) {
            int size_max_x = x0 + ((2*size)<<hshift) < s->HEVCsc->sps->pic_width_in_luma_samples ? 2*size : (s->HEVCsc->sps->pic_width_in_luma_samples - x0)>>hshift;
            int size_max_y = y0 + ((2*size)<<vshift) < s->HEVCsc->sps->pic_height_in_luma_samples ? 2*size : (s->HEVCsc->sps->pic_height_in_luma_samples - y0)>>vshift;
            int j = size + (bottom_left_available? bottom_left_size: 0) -1;
            if (bottom_left_available || left_available || top_left_available) {
                while(j>-1 && !IS_INTRA(-1, j)) j--;
                if (!IS_INTRA(-1, j)) {
                    j = 0;
                    while(j<size_max_x && !IS_INTRA(j, -1)) j++;
                    EXTEND_LEFT_CIP(top, j, j+1);
                    left[-1] = top[-1];
                    j = 0;
                }
            } else {
                j = 0;
                while(j<size_max_x && !IS_INTRA(j, -1)) j++;
                EXTEND_LEFT_CIP(top, j, j+1);
                left[-1] = top[-1];
                j = 0;
            }
            if (bottom_left_available || left_available) {
                EXTEND_DOWN_CIP(left, j, size_max_y-j);
            }
            if (!left_available) {
                EXTEND_DOWN(left, 0, size);
            }
            if (!bottom_left_available) {
                EXTEND_DOWN(left, size, size);
            }
            if (y0 != 0) {
                EXTEND_UP_CIP(left, size_max_y-1, size_max_y);
            } else {
                EXTEND_UP_CIP(left, size_max_y-1, size_max_y-1);
            }
            top[-1] = left[-1];
            if (y0 != 0) {
                EXTEND_RIGHT_CIP(top, 0, size_max_x);
            }
        }
    }
    // Infer the unavailable samples
    if (!bottom_left_available) {
        if (left_available) {
            EXTEND_DOWN(left, size, size);
        } else if (top_left_available) {
            EXTEND_DOWN(left, 0, 2*size);
            left_available = 1;
        } else if (top_available) {
            left[-1] = top[0];
            EXTEND_DOWN(left, 0, 2*size);
            top_left_available = 1;
            left_available = 1;
        } else if (top_right_available) {
            EXTEND_LEFT(top, size, size);
            left[-1] = top[0];
            EXTEND_DOWN(left ,0 , 2*size);
            top_available = 1;
            top_left_available = 1;
            left_available = 1;
        } else { // No samples available
            top[0] = left[-1] = (1 << (BIT_DEPTH - 1));
            EXTEND_RIGHT(top, 1, 2*size-1);
            EXTEND_DOWN(left, 0, 2*size);
        }
    }

    if (!left_available) {
        EXTEND_UP(left, size, size);
    }
    if (!top_left_available) {
        left[-1] = left[0];
    }
    if (!top_available) {
        top[0] = left[-1];
        EXTEND_RIGHT(top, 1, size-1);
    }
    if (!top_right_available) {
        EXTEND_RIGHT(top, size, size);
    }

    top[-1] = left[-1];

#undef EXTEND_LEFT_CIP
#undef EXTEND_RIGHT_CIP
#undef EXTEND_UP_CIP
#undef EXTEND_DOWN_CIP
#undef IS_INTRA
#undef EXTEND_LEFT
#undef EXTEND_RIGHT
#undef EXTEND_UP
#undef EXTEND_DOWN
#undef MIN_TB_ADDR_ZS

    // Filtering process
    if (c_idx == 0 && mode != INTRA_DC && size != 4) {
        int intra_hor_ver_dist_thresh[] = { 7, 1, 0 };
        int min_dist_vert_hor = FFMIN(FFABS((int)mode-26), FFABS((int)mode-10));
        if (min_dist_vert_hor > intra_hor_ver_dist_thresh[log2_size-3]) {
            int thresold = 1 << (BIT_DEPTH - 5);
            if (sc->sps->sps_strong_intra_smoothing_enable_flag && log2_size == 5 &&
                FFABS(top[-1] + top[63] - 2 * top[31]) < thresold &&
                FFABS(left[-1] + left[63] - 2 * left[31]) < thresold) {
                // We can't just overwrite values in top because it could be a pointer into src
                filtered_top[-1] = top[-1];
                filtered_top[63] = top[63];
                for (i = 0; i < 63; i++) {
                    filtered_top[i] = ((64 - (i + 1))*top[-1] + (i + 1) * top[63] + 32) >> 6;
                }
                for (i = 0; i < 63; i++) {
                    left[i] = ((64 - (i + 1))*left[-1] + (i + 1) * left[63] + 32) >> 6;
                }
                top = filtered_top;
            } else {
                filtered_left[2*size-1] = left[2*size-1];
                filtered_top[2*size-1]  = top[2*size-1];
                for (i = 2*size-2; i >= 0; i--) {
                    filtered_left[i] = (left[i+1] + 2*left[i] + left[i-1] + 2) >> 2;
                }
                filtered_top[-1] = filtered_left[-1] = (left[0] + 2*left[-1] + top[0] + 2) >> 2;
                for (i = 2*size-2; i >= 0; i--) {
                    filtered_top[i] = (top[i+1] + 2*top[i] + top[i-1] + 2) >> 2;
                }
                left = filtered_left;
                top = filtered_top;
            }
        }
    }

    switch(mode) {
    case INTRA_PLANAR:
        sc->hpc.pred_planar[log2_size -2]((uint8_t*)src, (uint8_t*)top, (uint8_t*)left, stride);
        break;
    case INTRA_DC:
        sc->hpc.pred_dc((uint8_t*)src, (uint8_t*)top, (uint8_t*)left, stride, log2_size, c_idx);
        break;
    default:
        sc->hpc.pred_angular[log2_size - 2]((uint8_t*)src, (uint8_t*)top, (uint8_t*)left, stride, c_idx, mode);
        break;
    }

}

static void FUNC(pred_planar_0)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int x, y;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
            POS(x, y) = ((3 - x) * left[y]  + (x + 1) * top[4] +
                         (3 - y) * top[x] + (y + 1) * left[4] + 4) >>
                        (3);
}

static void FUNC(pred_planar_1)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int x, y;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            POS(x, y) = ((7 - x) * left[y]  + (x + 1) * top[8] +
                         (7 - y) * top[x] + (y + 1) * left[8] + 8) >>
                        (4);
}

static void FUNC(pred_planar_2)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int x, y;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            POS(x, y) = ((15 - x) * left[y]  + (x + 1) * top[16] +
                         (15 - y) * top[x] + (y + 1) * left[16] + 16) >>
                        (5);
}

static void FUNC(pred_planar_3)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                               ptrdiff_t stride)
{
    int x, y;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++)
            POS(x, y) = ((31 - x) * left[y]  + (x + 1) * top[32] +
                         (31 - y) * top[x] + (y + 1) * left[32] + 32) >>
                        (6);
}

static void FUNC(pred_dc)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                           ptrdiff_t stride, int log2_size, int c_idx)
{
    int i, j, x, y;
    int size = (1 << log2_size);
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;
    int dc = size;
    pixel4 a;
    for (i = 0; i < size; i++)
        dc += left[i] + top[i];

    dc >>= log2_size + 1;

    a = PIXEL_SPLAT_X4(dc);

    for (i = 0; i < size; i++)
        for (j = 0; j < size / 4; j++)
            AV_WN4PA(&POS(j * 4, i), a);

    if (c_idx == 0 && size < 32) {
        POS(0, 0) = (left[0] + 2 * dc  + top[0] + 2) >> 2;
        for (x = 1; x < size; x++)
            POS(x, 0) = (top[x] + 3 * dc + 2) >> 2;
        for (y = 1; y < size; y++)
            POS(0, y) = (left[y] + 3 * dc + 2) >> 2;
    }
}

static void FUNC(pred_angular_0)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                                ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 1 << 2;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;

    const int intra_pred_angle[] = {
        32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
        -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    pixel ref_array[3*MAX_TB_SIZE+1];
    const pixel *ref;
    int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = top[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (y = 0; y < size; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ((32 - fact) * ref[x + idx + 1] + fact * ref[x + idx + 2] + 16) >> 5;
                }
            } else {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ref[x + idx + 1];
                }
            }
        }
        if (mode == 26 && c_idx == 0 && size < 32) {
            for (y = 0; y < size; y++)
                POS(0, y) = av_clip_pixel(top[0] + ((left[y] - left[-1]) >> 1));
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = left[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ref[y + idx + 1];
                }
            }
        }
        if (mode == 10 && c_idx == 0 && size < 32) {
            for (x = 0; x < size; x++)
                POS(x, 0) = av_clip_pixel(left[0] + ((top[x] - top[-1]) >> 1));
        }
    }
}
static void FUNC(pred_angular_1)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                                ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 1 << 3;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;

    const int intra_pred_angle[] = {
        32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
        -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    pixel ref_array[3*MAX_TB_SIZE+1];
    const pixel *ref;
    int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = top[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (y = 0; y < size; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ((32 - fact) * ref[x + idx + 1] + fact * ref[x + idx + 2] + 16) >> 5;
                }
            } else {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ref[x + idx + 1];
                }
            }
        }
        if (mode == 26 && c_idx == 0 && size < 32) {
            for (y = 0; y < size; y++)
                POS(0, y) = av_clip_pixel(top[0] + ((left[y] - left[-1]) >> 1));
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = left[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ref[y + idx + 1];
                }
            }
        }
        if (mode == 10 && c_idx == 0 && size < 32) {
            for (x = 0; x < size; x++)
                POS(x, 0) = av_clip_pixel(left[0] + ((top[x] - top[-1]) >> 1));
        }
    }
}
static void FUNC(pred_angular_2)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                                ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 1 << 4;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;

    const int intra_pred_angle[] = {
        32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
        -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    pixel ref_array[3*MAX_TB_SIZE+1];
    const pixel *ref;
    int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = top[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (y = 0; y < size; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ((32 - fact) * ref[x + idx + 1] + fact * ref[x + idx + 2] + 16) >> 5;
                }
            } else {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ref[x + idx + 1];
                }
            }
        }
        if (mode == 26 && c_idx == 0 && size < 32) {
            for (y = 0; y < size; y++)
                POS(0, y) = av_clip_pixel(top[0] + ((left[y] - left[-1]) >> 1));
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = left[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ref[y + idx + 1];
                }
            }
        }
        if (mode == 10 && c_idx == 0 && size < 32) {
            for (x = 0; x < size; x++)
                POS(x, 0) = av_clip_pixel(left[0] + ((top[x] - top[-1]) >> 1));
        }
    }
}
static void FUNC(pred_angular_3)(uint8_t *_src, const uint8_t *_top, const uint8_t *_left,
                                ptrdiff_t stride, int c_idx, int mode)
{
    int x, y;
    int size = 1 << 5;
    pixel *src = (pixel*)_src;
    const pixel *top = (const pixel*)_top;
    const pixel *left = (const pixel*)_left;

    const int intra_pred_angle[] = {
        32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
        -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32
    };
    const int inv_angle[] = {
        -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
        -630, -910, -1638, -4096
    };

    int angle = intra_pred_angle[mode-2];
    pixel ref_array[3*MAX_TB_SIZE+1];
    const pixel *ref;
    int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = top[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = left[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (y = 0; y < size; y++) {
            int idx = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ((32 - fact) * ref[x + idx + 1] + fact * ref[x + idx + 2] + 16) >> 5;
                }
            } else {
                for (x = 0; x < size; x++) {
                    POS(x, y) = ref[x + idx + 1];
                }
            }
        }
        if (mode == 26 && c_idx == 0 && size < 32) {
            for (y = 0; y < size; y++)
                POS(0, y) = av_clip_pixel(top[0] + ((left[y] - left[-1]) >> 1));
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x++)
                (ref_array + size)[x] = left[x - 1];
            for (x = last; x <= -1; x++)
                (ref_array + size)[x] = top[-1 + ((x * inv_angle[mode-11] + 128) >> 8)];
            ref = ref_array + size;
        }

        for (x = 0; x < size; x++) {
            int idx = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ((32 - fact) * ref[y + idx + 1] + fact * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ref[y + idx + 1];
                }
            }
        }
        if (mode == 10 && c_idx == 0 && size < 32) {
            for (x = 0; x < size; x++)
                POS(x, 0) = av_clip_pixel(left[0] + ((top[x] - top[-1]) >> 1));
        }
    }
}
#undef POS
