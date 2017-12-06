/*
 * HEVC CABAC decoding
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2012 - 2013 Gildas Cocherel
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

#include "libavutil/attributes.h"
#include "libavutil/common.h"

#include "cabac_functions.h"
#include "hevc_data.h"
#include "hevc.h"
#include "hevcdec.h"

#if OHCONFIG_AMT
#include "hevc_amt_defs.h"
#endif

#define CABAC_MAX_BIN 31

/**
 * number of bin by SyntaxElement.
 */
static const int8_t num_bins_in_se[] = {
     1, // sao_merge_flag
     1, // sao_type_idx
     0, // sao_eo_class
     0, // sao_band_position
     0, // sao_offset_abs
     0, // sao_offset_sign
     0, // end_of_slice_flag
     3, // split_coding_unit_flag
     1, // cu_transquant_bypass_flag
     3, // skip_flag
     3, // cu_qp_delta
     1, // pred_mode
     4, // part_mode
     0, // pcm_flag
     1, // prev_intra_luma_pred_mode
     0, // mpm_idx
     0, // rem_intra_luma_pred_mode
     2, // intra_chroma_pred_mode
     1, // merge_flag
     1, // merge_idx
     5, // inter_pred_idc
     2, // ref_idx_l0
     2, // ref_idx_l1
     2, // abs_mvd_greater0_flag
     2, // abs_mvd_greater1_flag
     0, // abs_mvd_minus2
     0, // mvd_sign_flag
     1, // mvp_lx_flag
     1, // no_residual_data_flag
     3, // split_transform_flag
     2, // cbf_luma
     4, // cbf_cb, cbf_cr
     2, // transform_skip_flag[][]
     2, // explicit_rdpcm_flag[][]
     2, // explicit_rdpcm_dir_flag[][]
    18, // last_significant_coeff_x_prefix
    18, // last_significant_coeff_y_prefix
     0, // last_significant_coeff_x_suffix
     0, // last_significant_coeff_y_suffix
     4, // significant_coeff_group_flag
    44, // significant_coeff_flag
    24, // coeff_abs_level_greater1_flag
     6, // coeff_abs_level_greater2_flag
     0, // coeff_abs_level_remaining
     0, // coeff_sign_flag
     8, // log2_res_scale_abs
     2, // res_scale_sign_flag
     1, // cu_chroma_qp_offset_flag
     1, // cu_chroma_qp_offset_idx
#if OHCONFIG_AMT
     4, // emt_cu_flag
     4, // emt_tu_idx
#endif
};

/**
 * Offset to ctxIdx 0 in init_values and states, indexed by SyntaxElement.
 */
static const int elem_offset[sizeof(num_bins_in_se)] = {
    0, // sao_merge_flag
    1, // sao_type_idx
    2, // sao_eo_class
    2, // sao_band_position
    2, // sao_offset_abs
    2, // sao_offset_sign
    2, // end_of_slice_flag
    2, // split_coding_unit_flag
    5, // cu_transquant_bypass_flag
    6, // skip_flag
    9, // cu_qp_delta
    12, // pred_mode
    13, // part_mode
    17, // pcm_flag
    17, // prev_intra_luma_pred_mode
    18, // mpm_idx
    18, // rem_intra_luma_pred_mode
    18, // intra_chroma_pred_mode
    20, // merge_flag
    21, // merge_idx
    22, // inter_pred_idc
    27, // ref_idx_l0
    29, // ref_idx_l1
    31, // abs_mvd_greater0_flag
    33, // abs_mvd_greater1_flag
    35, // abs_mvd_minus2
    35, // mvd_sign_flag
    35, // mvp_lx_flag
    36, // no_residual_data_flag
    37, // split_transform_flag
    40, // cbf_luma
    42, // cbf_cb, cbf_cr
    46, // transform_skip_flag[][]
    48, // explicit_rdpcm_flag[][]
    50, // explicit_rdpcm_dir_flag[][]
    52, // last_significant_coeff_x_prefix
    70, // last_significant_coeff_y_prefix
    88, // last_significant_coeff_x_suffix
    88, // last_significant_coeff_y_suffix
    88, // significant_coeff_group_flag
    92, // significant_coeff_flag
    136, // coeff_abs_level_greater1_flag
    160, // coeff_abs_level_greater2_flag
    166, // coeff_abs_level_remaining
    166, // coeff_sign_flag
    166, // log2_res_scale_abs
    174, // res_scale_sign_flag
    176, // cu_chroma_qp_offset_flag
    177, // cu_chroma_qp_offset_idx
#if OHCONFIG_AMT
    178, // emt_cu_flag
    182, // emt_tu_idx
#endif
};

#define CNU 154
/**
 * Indexed by init_type
 */
static const uint8_t init_values[3][HEVC_CONTEXTS] = {
    { // sao_merge_flag
      153,
      // sao_type_idx
      200,
      // split_coding_unit_flag
      139, 141, 157,
      // cu_transquant_bypass_flag
      154,
      // skip_flag
      CNU, CNU, CNU,
      // cu_qp_delta
      154, 154, 154,
      // pred_mode
      CNU,
      // part_mode
      184, CNU, CNU, CNU,
      // prev_intra_luma_pred_mode
      184,
      // intra_chroma_pred_mode
      63, 139,
      // merge_flag
      CNU,
      // merge_idx
      CNU,
      // inter_pred_idc
      CNU, CNU, CNU, CNU, CNU,
      // ref_idx_l0
      CNU, CNU,
      // ref_idx_l1
      CNU, CNU,
      // abs_mvd_greater1_flag
      CNU, CNU,
      // abs_mvd_greater1_flag
      CNU, CNU,
      // mvp_lx_flag
      CNU,
      // no_residual_data_flag
      CNU,
      // split_transform_flag
      153, 138, 138,
      // cbf_luma
      111, 141,
      // cbf_cb, cbf_cr
      94, 138, 182, 154,
      // transform_skip_flag
      139, 139,
      // explicit_rdpcm_flag
      139, 139,
      // explicit_rdpcm_dir_flag
      139, 139,
      // last_significant_coeff_x_prefix
      110, 110, 124, 125, 140, 153, 125, 127, 140, 109, 111, 143, 127, 111,
       79, 108, 123,  63,
      // last_significant_coeff_y_prefix
      110, 110, 124, 125, 140, 153, 125, 127, 140, 109, 111, 143, 127, 111,
       79, 108, 123,  63,
      // significant_coeff_group_flag
      91, 171, 134, 141,
      // significant_coeff_flag
      111, 111, 125, 110, 110,  94, 124, 108, 124, 107, 125, 141, 179, 153,
      125, 107, 125, 141, 179, 153, 125, 107, 125, 141, 179, 153, 125, 140,
      139, 182, 182, 152, 136, 152, 136, 153, 136, 139, 111, 136, 139, 111,
      141, 111,
      // coeff_abs_level_greater1_flag
      140,  92, 137, 138, 140, 152, 138, 139, 153,  74, 149,  92, 139, 107,
      122, 152, 140, 179, 166, 182, 140, 227, 122, 197,
      // coeff_abs_level_greater2_flag
      138, 153, 136, 167, 152, 152,
      // log2_res_scale_abs
      154, 154, 154, 154, 154, 154, 154, 154,
      // res_scale_sign_flag
      154, 154,
      // cu_chroma_qp_offset_flag
      154,
      // cu_chroma_qp_offset_idx
      154,
#if OHCONFIG_AMT
      // emt_cu_flag
      CNU, CNU, CNU, CNU,
      // emt_tu_idx
      CNU, CNU, CNU, CNU,
#endif
    },
    { // sao_merge_flag
      153,
      // sao_type_idx
      185,
      // split_coding_unit_flag
      107, 139, 126,
      // cu_transquant_bypass_flag
      154,
      // skip_flag
      197, 185, 201,
      // cu_qp_delta
      154, 154, 154,
      // pred_mode
      149,
      // part_mode
      154, 139, 154, 154,
      // prev_intra_luma_pred_mode
      154,
      // intra_chroma_pred_mode
      152, 139,
      // merge_flag
      110,
      // merge_idx
      122,
      // inter_pred_idc
      95, 79, 63, 31, 31,
      // ref_idx_l0
      153, 153,
      // ref_idx_l1
      153, 153,
      // abs_mvd_greater1_flag
      140, 198,
      // abs_mvd_greater1_flag
      140, 198,
      // mvp_lx_flag
      168,
      // no_residual_data_flag
      79,
      // split_transform_flag
      124, 138, 94,
      // cbf_luma
      153, 111,
      // cbf_cb, cbf_cr
      149, 107, 167, 154,
      // transform_skip_flag
      139, 139,
      // explicit_rdpcm_flag
      139, 139,
      // explicit_rdpcm_dir_flag
      139, 139,
      // last_significant_coeff_x_prefix
      125, 110,  94, 110,  95,  79, 125, 111, 110,  78, 110, 111, 111,  95,
       94, 108, 123, 108,
      // last_significant_coeff_y_prefix
      125, 110,  94, 110,  95,  79, 125, 111, 110,  78, 110, 111, 111,  95,
       94, 108, 123, 108,
      // significant_coeff_group_flag
      121, 140, 61, 154,
      // significant_coeff_flag
      155, 154, 139, 153, 139, 123, 123,  63, 153, 166, 183, 140, 136, 153,
      154, 166, 183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 170,
      153, 123, 123, 107, 121, 107, 121, 167, 151, 183, 140, 151, 183, 140,
      140, 140,
      // coeff_abs_level_greater1_flag
      154, 196, 196, 167, 154, 152, 167, 182, 182, 134, 149, 136, 153, 121,
      136, 137, 169, 194, 166, 167, 154, 167, 137, 182,
      // coeff_abs_level_greater2_flag
      107, 167, 91, 122, 107, 167,
      // log2_res_scale_abs
      154, 154, 154, 154, 154, 154, 154, 154,
      // res_scale_sign_flag
      154, 154,
      // cu_chroma_qp_offset_flag
      154,
      // cu_chroma_qp_offset_idx
      154,
#if OHCONFIG_AMT
      // emt_cu_flag
      CNU, CNU, CNU, CNU,
      // emt_tu_idx
      CNU, CNU, CNU, CNU,
#endif
    },
    { // sao_merge_flag
      153,
      // sao_type_idx
      160,
      // split_coding_unit_flag
      107, 139, 126,
      // cu_transquant_bypass_flag
      154,
      // skip_flag
      197, 185, 201,
      // cu_qp_delta
      154, 154, 154,
      // pred_mode
      134,
      // part_mode
      154, 139, 154, 154,
      // prev_intra_luma_pred_mode
      183,
      // intra_chroma_pred_mode
      152, 139,
      // merge_flag
      154,
      // merge_idx
      137,
      // inter_pred_idc
      95, 79, 63, 31, 31,
      // ref_idx_l0
      153, 153,
      // ref_idx_l1
      153, 153,
      // abs_mvd_greater1_flag
      169, 198,
      // abs_mvd_greater1_flag
      169, 198,
      // mvp_lx_flag
      168,
      // no_residual_data_flag
      79,
      // split_transform_flag
      224, 167, 122,
      // cbf_luma
      153, 111,
      // cbf_cb, cbf_cr
      149, 92, 167, 154,
      // transform_skip_flag
      139, 139,
      // explicit_rdpcm_flag
      139, 139,
      // explicit_rdpcm_dir_flag
      139, 139,
      // last_significant_coeff_x_prefix
      125, 110, 124, 110,  95,  94, 125, 111, 111,  79, 125, 126, 111, 111,
       79, 108, 123,  93,
      // last_significant_coeff_y_prefix
      125, 110, 124, 110,  95,  94, 125, 111, 111,  79, 125, 126, 111, 111,
       79, 108, 123,  93,
      // significant_coeff_group_flag
      121, 140, 61, 154,
      // significant_coeff_flag
      170, 154, 139, 153, 139, 123, 123,  63, 124, 166, 183, 140, 136, 153,
      154, 166, 183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 170,
      153, 138, 138, 122, 121, 122, 121, 167, 151, 183, 140, 151, 183, 140,
      140, 140,
      // coeff_abs_level_greater1_flag
      154, 196, 167, 167, 154, 152, 167, 182, 182, 134, 149, 136, 153, 121,
      136, 122, 169, 208, 166, 167, 154, 152, 167, 182,
      // coeff_abs_level_greater2_flag
      107, 167, 91, 107, 107, 167,
      // log2_res_scale_abs
      154, 154, 154, 154, 154, 154, 154, 154,
      // res_scale_sign_flag
      154, 154,
      // cu_chroma_qp_offset_flag
      154,
      // cu_chroma_qp_offset_idx
      154,
#if OHCONFIG_AMT
      // emt_cu_flag
      CNU, CNU, CNU, CNU,
      // emt_tu_idx
      CNU, CNU, CNU, CNU,
#endif
    },
};

static const uint8_t scan_1x1[1] = {
    0,
};

static const uint8_t horiz_scan2x2_x[4] = {
    0, 1,
    0, 1,
};

static const uint8_t horiz_scan2x2_y[4] = {
    0, 0,
    1, 1
};

static const uint8_t horiz_scan4x4_x[16] = {
    0, 1, 2, 3,
    0, 1, 2, 3,
    0, 1, 2, 3,
    0, 1, 2, 3,
};

static const uint8_t horiz_scan4x4_y[16] = {
    0, 0, 0, 0,
    1, 1, 1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
};

static const uint8_t horiz_scan8x8_inv[8][8] = {
    {  0,  1,  2,  3, 16, 17, 18, 19, },
    {  4,  5,  6,  7, 20, 21, 22, 23, },
    {  8,  9, 10, 11, 24, 25, 26, 27, },
    { 12, 13, 14, 15, 28, 29, 30, 31, },
    { 32, 33, 34, 35, 48, 49, 50, 51, },
    { 36, 37, 38, 39, 52, 53, 54, 55, },
    { 40, 41, 42, 43, 56, 57, 58, 59, },
    { 44, 45, 46, 47, 60, 61, 62, 63, },
};

static const uint8_t diag_scan2x2_x[4] = {
    0, 0,
    1, 1,
};

static const uint8_t diag_scan2x2_y[4] = {
    0, 1,
    0, 1,
};

static const uint8_t diag_scan2x2_inv[2][2] = {
    { 0, 2, },
    { 1, 3, },
};

static const uint8_t diag_scan4x4_inv[4][4] = {
    { 0,  2,  5,  9, },
    { 1,  4,  8, 12, },
    { 3,  7, 11, 14, },
    { 6, 10, 13, 15, },
};

static const uint8_t diag_scan8x8_inv[8][8] = {
    {  0,  2,  5,  9, 14, 20, 27, 35, },
    {  1,  4,  8, 13, 19, 26, 34, 42, },
    {  3,  7, 12, 18, 25, 33, 41, 48, },
    {  6, 11, 17, 24, 32, 40, 47, 53, },
    { 10, 16, 23, 31, 39, 46, 52, 57, },
    { 15, 22, 30, 38, 45, 51, 56, 60, },
    { 21, 29, 37, 44, 50, 55, 59, 62, },
    { 28, 36, 43, 49, 54, 58, 61, 63, },
};

//CG map for 16x16 size and scan for coeff into CG
static const uint8_t diag_scan4x4_inv2[16] = {
    0,  4,  1, 8,
    5,  2, 12, 9,
    6,  3, 13, 10,
    7, 14, 11, 15
};

static const uint8_t vert_scan4x4_inv2[16]={
    0, 4,  8, 12,
    1, 5,  9, 13,
    2, 6, 10, 14,
    3, 7, 11, 15,
};

static const uint8_t hor_scan4x4_inv2[16]={
     0,  1,  2,  3,
     4,  5,  6,  7,
     8,  9, 10, 11,
    12, 13, 14, 15
};

//CG map for 8X8 size
static const uint8_t diag_scan2x2_inv2[4] = {
    0, 2,
    1, 3
};

static const uint8_t vert_scan2x2_inv2[4] = {
    0, 2,
    1, 3
};

static const uint8_t hor_scan2x2_inv2[4] = {
    0, 1,
    2, 3
};

//CG map for 32x32 size
static const uint8_t diag_scan8x8_inv2[64]={
    0,  8,  1, 16,  9,  2, 24, 17,
   10,  3, 32, 25, 18, 11,  4, 40,
   33, 26, 19, 12,  5, 48, 41, 34,
   27, 20, 13,  6, 56, 49, 42, 35,
   28, 21, 14,  7, 57, 50, 43, 36,
   29, 22, 15, 58, 51, 44, 37, 30,
   23, 59, 52, 45, 38, 31, 60, 53,
   46, 39, 61, 54, 47, 62, 55, 63
};



#if OHCONFIG_AMT
static const int emt_intra_subset_select[3][2] = {
    {DST_VII, DCT_VIII},
    {DST_VII, DST_I   },
    {DST_VII, DCT_V   }
};

static const int emt_inter_subset_select[2] = {
    DCT_VIII, DST_VII
};

static const int emt_intra_mode2tr_idx_v[35] =
{
    2, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 1, 0, 1, 0, 1, 0
};

static const int emt_intra_mode2tr_idx_h[35] =
{
    2, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0
};
#endif

void ff_hevc_save_states(HEVCContext *s, int ctb_addr_ts)
{
    if (s->ps.pps->entropy_coding_sync_enabled_flag &&
        (s->HEVClc->ctb_tile_rs % s->ps.pps->tile_width[ctb_addr_ts-1] == 2 ||
         (s->ps.pps->tile_width[ctb_addr_ts-1] == 2 &&
          s->HEVClc->ctb_tile_rs % s->ps.pps->tile_width[ctb_addr_ts-1] == 0))) {
        memcpy(s->cabac_state, s->HEVClc->cabac_state, HEVC_CONTEXTS);
    }
}

static void load_states(HEVCContext *s)
{
    memcpy(s->HEVClc->cabac_state, s->cabac_state, HEVC_CONTEXTS);
}

static void cabac_reinit(HEVCLocalContext *lc)
{
    skip_bytes(&lc->cc, 0);
}

static int cabac_init_decoder(HEVCContext *s)
{
    GetBitContext *gb = &s->HEVClc->gb;
    skip_bits(gb, 1);
    align_get_bits(gb);
    return ff_init_cabac_decoder(&s->HEVClc->cc,
                          gb->buffer + get_bits_count(gb) / 8,
                          (get_bits_left(gb) + 7) / 8);
}

static void cabac_init_state(HEVCContext *s)
{
    int init_type = 2 - s->sh.slice_type;
    int i;

    if (s->sh.cabac_init_flag && s->sh.slice_type != HEVC_SLICE_I)
        init_type ^= 3;

    for (i = 0; i < HEVC_CONTEXTS; i++) {
        int init_value = init_values[init_type][i];
        int m = (init_value >> 4) * 5 - 45;
        int n = ((init_value & 15) << 3) - 16;
        int pre = 2 * (((m * av_clip(s->sh.slice_qp, 0, 51)) >> 4) + n) - 127;

        pre ^= pre >> 31;
        if (pre > 124)
            pre = 124 + (pre & 1);
        s->HEVClc->cabac_state[i] = pre;
    }

    for (i = 0; i < 4; i++)
        s->HEVClc->rice_ctx.stat_coeff[i] = 0;
}

int ff_hevc_cabac_init(HEVCContext *s, int ctb_addr_ts)
{
    if (ctb_addr_ts == s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_ctb_addr_rs]) {
        int ret = cabac_init_decoder(s);
        if (ret < 0)
            return ret;
        if (s->sh.dependent_slice_segment_flag == 0 ||
            (s->ps.pps->tiles_enabled_flag &&
             s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[ctb_addr_ts - 1])){
            cabac_init_state(s);

#if OHCONFIG_ENCRYPTION
            //if(s->tile_table_encry)
            if (s->tile_table_encry[s->ps.pps->tile_id[ctb_addr_ts]]){
                InitC(s->HEVClc->dbs_g, s->encrypt_init_val);
                s->HEVClc->prev_pos = 0;
            }
#endif
        }
        if (!s->sh.first_slice_in_pic_flag &&
            s->ps.pps->entropy_coding_sync_enabled_flag) {
             if (s->HEVClc->ctb_tile_rs % s->ps.pps->tile_width[ctb_addr_ts] == 0) {
                if (s->ps.pps->tile_width[ctb_addr_ts] == 1)
                    cabac_init_state(s);
                else if (s->sh.dependent_slice_segment_flag == 1)
                    load_states(s);
            }
        }
    } else {
        if (s->ps.pps->tiles_enabled_flag &&
            s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[ctb_addr_ts - 1]) {

            s->HEVClc->ctb_tile_rs = 0;
            if (s->threads_number == 1)
                cabac_reinit(s->HEVClc);
            else {
                int ret = cabac_init_decoder(s);
                if (ret < 0)
                    return ret;
            }
            cabac_init_state(s);
#if OHCONFIG_ENCRYPTION
            if (s->tile_table_encry[s->ps.pps->tile_id[ctb_addr_ts]]){
                InitC(s->HEVClc->dbs_g, s->encrypt_init_val);
                s->HEVClc->prev_pos = 0;
            }
#endif
        }
        if (s->ps.pps->entropy_coding_sync_enabled_flag) {
            if (s->HEVClc->ctb_tile_rs % s->ps.pps->tile_width[ctb_addr_ts] == 0) {
                if (!s->ps.pps->tiles_enabled_flag ||
                    (s->ps.pps->tile_id[ctb_addr_ts] == s->ps.pps->tile_id[ctb_addr_ts - 1])) {
                    get_cabac_terminate(&s->HEVClc->cc);

                    if (s->threads_number == 1)
                        cabac_reinit(s->HEVClc);
                    else
                        cabac_init_decoder(s);

                    if (s->ps.pps->tile_width[ctb_addr_ts] == 1)
                        cabac_init_state(s);
                    else
                        load_states(s);
                }
            }
        }
    }
    return 0;
}

#define GET_CABAC(ctx) get_cabac(&s->HEVClc->cc, &s->HEVClc->cabac_state[ctx])

int ff_hevc_sao_merge_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[SAO_MERGE_FLAG]);
}

int ff_hevc_sao_type_idx_decode(HEVCContext *s)
{
    if (!GET_CABAC(elem_offset[SAO_TYPE_IDX]))
        return 0;

    if (!get_cabac_bypass(&s->HEVClc->cc))
        return SAO_BAND;
    return SAO_EDGE;
}

int ff_hevc_sao_band_position_decode(HEVCContext *s)
{
    int i;
    int value = get_cabac_bypass(&s->HEVClc->cc);

    for (i = 0; i < 4; i++)
        value = (value << 1) | get_cabac_bypass(&s->HEVClc->cc);
    return value;
}

int ff_hevc_sao_offset_abs_decode(HEVCContext *s)
{
    int i = 0;
    int length = (1 << (FFMIN(s->ps.sps->bit_depth[CHANNEL_TYPE_LUMA], 10) - 5)) - 1;

    while (i < length && get_cabac_bypass(&s->HEVClc->cc))
        i++;
    return i;
}

int ff_hevc_sao_offset_sign_decode(HEVCContext *s)
{
    return get_cabac_bypass(&s->HEVClc->cc);
}

int ff_hevc_sao_eo_class_decode(HEVCContext *s)
{
    int ret = get_cabac_bypass(&s->HEVClc->cc) << 1;
    ret    |= get_cabac_bypass(&s->HEVClc->cc);
    return ret;
}

int ff_hevc_end_of_slice_flag_decode(HEVCContext *s)
{
    return get_cabac_terminate(&s->HEVClc->cc);
}

int ff_hevc_cu_transquant_bypass_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[CU_TRANSQUANT_BYPASS_FLAG]);
}

int ff_hevc_skip_flag_decode(HEVCContext *s, int x0, int y0, int x_cb, int y_cb)
{
    int min_cb_width = s->ps.sps->min_cb_width;
    int inc = 0;
    int x0b = av_mod_uintp2(x0, s->ps.sps->log2_ctb_size);
    int y0b = av_mod_uintp2(y0, s->ps.sps->log2_ctb_size);

    if (s->HEVClc->ctb_left_flag || x0b)
        inc = !!SAMPLE_CTB(s->skip_flag, x_cb - 1, y_cb);
    if (s->HEVClc->ctb_up_flag || y0b)
        inc += !!SAMPLE_CTB(s->skip_flag, x_cb, y_cb - 1);

    return GET_CABAC(elem_offset[SKIP_FLAG] + inc);
}

int ff_hevc_cu_qp_delta_abs(HEVCContext *s)
{
    int prefix_val = 0;
    int suffix_val = 0;
    int inc = 0;

    while (prefix_val < 5 && GET_CABAC(elem_offset[CU_QP_DELTA] + inc)) {
        prefix_val++;
        inc = 1;
    }
    if (prefix_val >= 5) {
        int k = 0;
        while (k < CABAC_MAX_BIN && get_cabac_bypass(&s->HEVClc->cc)) {
            suffix_val += 1 << k;
            k++;
        }
        if (k == CABAC_MAX_BIN)
            av_log(s->avctx, AV_LOG_ERROR, "CABAC_MAX_BIN : %d\n", k);

        while (k--)
            suffix_val += get_cabac_bypass(&s->HEVClc->cc) << k;
    }
    return prefix_val + suffix_val;
}

int ff_hevc_cu_qp_delta_sign_flag(HEVCContext *s)
{
    return get_cabac_bypass(&s->HEVClc->cc);
}

int ff_hevc_cu_chroma_qp_offset_flag(HEVCContext *s)
{
    return GET_CABAC(elem_offset[CU_CHROMA_QP_OFFSET_FLAG]);
}

int ff_hevc_cu_chroma_qp_offset_idx(HEVCContext *s)
{
    int c_max= FFMAX(5, s->ps.pps->chroma_qp_offset_list_len_minus1);
    int i = 0;

    while (i < c_max && GET_CABAC(elem_offset[CU_CHROMA_QP_OFFSET_IDX]))
        i++;

    return i;
}

#if OHCONFIG_AMT
uint8_t ff_hevc_emt_cu_flag_decode(HEVCContext *s, int log2_cb_size, int cbfLuma)
{
    //uint8_t inc = ;
	uint8_t flag_value = 0;
    if ( (s->HEVClc->cu.pred_mode == MODE_INTRA ) && s->ps.sps->use_intra_emt && ( 1 << log2_cb_size <= EMT_INTRA_MAX_CU ) && cbfLuma)
	{
        //inc = ;
        flag_value = GET_CABAC( elem_offset[EMT_CU_FLAG] + 5 - log2_cb_size );
        return flag_value ;
	}
    if ( (s->HEVClc->cu.pred_mode == MODE_INTER ) && s->ps.sps->use_inter_emt && ( 1 << log2_cb_size <= EMT_INTER_MAX_CU ) &&  cbfLuma)
	{
        //inc = 5 - log2_cb_size;
        flag_value = GET_CABAC( elem_offset[EMT_CU_FLAG] + 5 - log2_cb_size );
        return flag_value ;
	}
	return flag_value ;
}

uint8_t ff_hevc_emt_tu_idx_decode(HEVCContext *s, int log2_cb_size)
{
    uint8_t trIdx = 0;

    if ( (s->HEVClc->cu.pred_mode == MODE_INTER) && ((1 << log2_cb_size) <= EMT_INTER_MAX_CU )){
        uint8_t uiSymbol1 = GET_CABAC(elem_offset[EMT_TU_IDX]+2);
        uint8_t uiSymbol2 = GET_CABAC(elem_offset[EMT_TU_IDX]+3);
        trIdx = (uiSymbol2 << 1) | uiSymbol1 ;
        return trIdx ;
    } else if ( (s->HEVClc->cu.pred_mode == MODE_INTRA) && ((1 << log2_cb_size) <= EMT_INTRA_MAX_CU )){
        uint8_t uiSymbol1 = GET_CABAC(elem_offset[EMT_TU_IDX]);
        uint8_t uiSymbol2 = GET_CABAC(elem_offset[EMT_TU_IDX]+1);
        trIdx = (uiSymbol2 << 1) | uiSymbol1 ;
        return trIdx ;
    }
    return trIdx ;
}
#endif

int ff_hevc_pred_mode_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[PRED_MODE_FLAG]);
}

int ff_hevc_split_coding_unit_flag_decode(HEVCContext *s, int ct_depth, int x0, int y0)
{
    int inc = 0, depth_left = 0, depth_top = 0;
    int x0b  = av_mod_uintp2(x0, s->ps.sps->log2_ctb_size);
    int y0b  = av_mod_uintp2(y0, s->ps.sps->log2_ctb_size);
    int x_cb = x0 >> s->ps.sps->log2_min_cb_size;
    int y_cb = y0 >> s->ps.sps->log2_min_cb_size;

    if (s->HEVClc->ctb_left_flag || x0b)
        depth_left = s->tab_ct_depth[(y_cb) * s->ps.sps->min_cb_width + x_cb - 1];
    if (s->HEVClc->ctb_up_flag || y0b)
        depth_top = s->tab_ct_depth[(y_cb - 1) * s->ps.sps->min_cb_width + x_cb];

    inc += (depth_left > ct_depth);
    inc += (depth_top  > ct_depth);

    return GET_CABAC(elem_offset[SPLIT_CODING_UNIT_FLAG] + inc);
}

int ff_hevc_part_mode_decode(HEVCContext *s, int log2_cb_size)
{
    if (GET_CABAC(elem_offset[PART_MODE])) // 1
        return PART_2Nx2N;
    if (log2_cb_size == s->ps.sps->log2_min_cb_size) {
        if (s->HEVClc->cu.pred_mode == MODE_INTRA) // 0
            return PART_NxN;
        if (GET_CABAC(elem_offset[PART_MODE] + 1)) // 01
            return PART_2NxN;
        if (log2_cb_size == 3) // 00
            return PART_Nx2N;
        if (GET_CABAC(elem_offset[PART_MODE] + 2)) // 001
            return PART_Nx2N;
        return PART_NxN; // 000
    }

    if (!s->ps.sps->amp_enabled_flag) {
        if (GET_CABAC(elem_offset[PART_MODE] + 1)) // 01
            return PART_2NxN;
        return PART_Nx2N;
    }

    if (GET_CABAC(elem_offset[PART_MODE] + 1)) { // 01X, 01XX
        if (GET_CABAC(elem_offset[PART_MODE] + 3)) // 011
            return PART_2NxN;
        if (get_cabac_bypass(&s->HEVClc->cc)) // 0101
            return PART_2NxnD;
        return PART_2NxnU; // 0100
    }

    if (GET_CABAC(elem_offset[PART_MODE] + 3)) // 001
        return PART_Nx2N;
    if (get_cabac_bypass(&s->HEVClc->cc)) // 0001
        return PART_nRx2N;
    return PART_nLx2N;  // 0000
}

int ff_hevc_pcm_flag_decode(HEVCContext *s)
{
    return get_cabac_terminate(&s->HEVClc->cc);
}

int ff_hevc_prev_intra_luma_pred_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[PREV_INTRA_LUMA_PRED_FLAG]);
}

int ff_hevc_mpm_idx_decode(HEVCContext *s)
{
    int i = 0;
    while (i < 2 && get_cabac_bypass(&s->HEVClc->cc))
        i++;
    return i;
}

int ff_hevc_rem_intra_luma_pred_mode_decode(HEVCContext *s)
{
    int i;
    int value = get_cabac_bypass(&s->HEVClc->cc);

    for (i = 0; i < 4; i++)
        value = (value << 1) | get_cabac_bypass(&s->HEVClc->cc);
    return value;
}

int ff_hevc_intra_chroma_pred_mode_decode(HEVCContext *s)
{
    int ret;
    if (!GET_CABAC(elem_offset[INTRA_CHROMA_PRED_MODE]))
        return 4;

    ret  = get_cabac_bypass(&s->HEVClc->cc) << 1;
    ret |= get_cabac_bypass(&s->HEVClc->cc);
    return ret;
}

int ff_hevc_merge_idx_decode(HEVCContext *s)
{
    int i = GET_CABAC(elem_offset[MERGE_IDX]);

    if (i != 0) {
        while (i < s->sh.max_num_merge_cand-1 && get_cabac_bypass(&s->HEVClc->cc))
            i++;
    }
    return i;
}

int ff_hevc_merge_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[MERGE_FLAG]);
}

int ff_hevc_inter_pred_idc_decode(HEVCContext *s, int nPbW, int nPbH)
{
    if (nPbW + nPbH == 12)
        return GET_CABAC(elem_offset[INTER_PRED_IDC] + 4);
    if (GET_CABAC(elem_offset[INTER_PRED_IDC] + s->HEVClc->ct_depth))
        return PRED_BI;

    return GET_CABAC(elem_offset[INTER_PRED_IDC] + 4);
}

int ff_hevc_ref_idx_lx_decode(HEVCContext *s, int num_ref_idx_lx)
{
    int i = 0;
    int max = num_ref_idx_lx - 1;
    int max_ctx = FFMIN(max, 2);

    while (i < max_ctx && GET_CABAC(elem_offset[REF_IDX_L0] + i))
        i++;
    if (i == 2) {
        while (i < max && get_cabac_bypass(&s->HEVClc->cc))
            i++;
    }

    return i;
}

int ff_hevc_mvp_lx_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[MVP_LX_FLAG]);
}

int ff_hevc_no_residual_syntax_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[NO_RESIDUAL_DATA_FLAG]);
}

static av_always_inline int abs_mvd_greater0_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[ABS_MVD_GREATER0_FLAG]);
}

static av_always_inline int abs_mvd_greater1_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[ABS_MVD_GREATER1_FLAG] + 1);
}


#if OHCONFIG_ENCRYPTION
static av_always_inline int mvd_sign_flag_decode(HEVCContext *s);

static av_always_inline int mvd_decode_enc(HEVCContext *s)
{
    int ret = 2, ret0 = 0, sign;
    int k = 1, k0;
    unsigned int key;
    while (k < CABAC_MAX_BIN && get_cabac_bypass(&s->HEVClc->cc)) {
        ret += 1 << k;
        k++;
    }
    k0 = k;
    if (k == CABAC_MAX_BIN)
        av_log(s->avctx, AV_LOG_ERROR, "CABAC_MAX_BIN : %d\n", k);
    key = ff_get_key (&s->HEVClc->dbs_g, k);
    while (k--) {
        unsigned int e = get_cabac_bypass(&s->HEVClc->cc);
        ret0 += e << k;
    }
    s->HEVClc->prev_pos = ret0 - (s->HEVClc->prev_pos^key);
    ret += (s->HEVClc->prev_pos&((1<<k0)-1));
    s->HEVClc->prev_pos = ret0;
    sign = mvd_sign_flag_decode(s);
    ret = sign==-1 ? -ret:ret;
    return ret;
}
#endif
static av_always_inline int mvd_decode(HEVCContext *s)
{
    int ret = 2;
    int k = 1;
#if OHCONFIG_ENCRYPTION
    if( s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_MVs))
      return mvd_decode_enc (s);
#endif
    while (k < CABAC_MAX_BIN && get_cabac_bypass(&s->HEVClc->cc)) {
        ret += 1U << k;
        k++;
    }
    if (k == CABAC_MAX_BIN) {
        av_log(s->avctx, AV_LOG_ERROR, "CABAC_MAX_BIN : %d\n", k);
        return 0;
    }
    while (k--)
        ret += get_cabac_bypass(&s->HEVClc->cc) << k;
    return get_cabac_bypass_sign(&s->HEVClc->cc, -ret);
}

static av_always_inline int mvd_sign_flag_decode(HEVCContext *s)
{
    return get_cabac_bypass_sign(&s->HEVClc->cc, -1);
}

int ff_hevc_split_transform_flag_decode(HEVCContext *s, int log2_trafo_size)
{
    return GET_CABAC(elem_offset[SPLIT_TRANSFORM_FLAG] + 5 - log2_trafo_size);
}

int ff_hevc_cbf_cb_cr_decode(HEVCContext *s, int trafo_depth)
{
    return GET_CABAC(elem_offset[CBF_CB_CR] + trafo_depth);
}

int ff_hevc_cbf_luma_decode(HEVCContext *s, int trafo_depth)
{
    return GET_CABAC(elem_offset[CBF_LUMA] + !trafo_depth);
}

static int hevc_transform_skip_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[TRANSFORM_SKIP_FLAG]);
}

static int hevc_transform_skip_flag_decode_c(HEVCContext *s)
{
    return GET_CABAC(elem_offset[TRANSFORM_SKIP_FLAG] + 1);
}


static int explicit_rdpcm_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[EXPLICIT_RDPCM_FLAG]);
}

static int explicit_rdpcm_flag_decode_c(HEVCContext *s)
{
    return GET_CABAC(elem_offset[EXPLICIT_RDPCM_FLAG] + 1);
}

static int explicit_rdpcm_dir_flag_decode(HEVCContext *s)
{
    return GET_CABAC(elem_offset[EXPLICIT_RDPCM_DIR_FLAG]);
}

static int explicit_rdpcm_dir_flag_decode_c(HEVCContext *s)
{
    return GET_CABAC(elem_offset[EXPLICIT_RDPCM_DIR_FLAG] + 1);
}

int ff_hevc_log2_res_scale_abs(HEVCContext *s, int idx) {
    int i =0;

    while (i < 4 && GET_CABAC(elem_offset[LOG2_RES_SCALE_ABS] + 4 * idx + i))
        i++;

    return i;
}

int ff_hevc_res_scale_sign_flag(HEVCContext *s, int idx) {
    return GET_CABAC(elem_offset[RES_SCALE_SIGN_FLAG] + idx);
}

//FIXME we could probably avoid ctx offset and shift derivation given log2_size here
static av_always_inline void last_significant_coeff_xy_prefix_decode(HEVCContext *s,
                                                   int log2_size, int *last_scx_prefix, int *last_scy_prefix)
{
    int i = 0;
    int max = (log2_size << 1) - 1;

    const int ctx_offset = 3 * (log2_size - 2)  + ((log2_size - 1) >> 2);
    const int ctx_shift = (log2_size + 1) >> 2;

    while (i < max &&
           GET_CABAC(elem_offset[LAST_SIGNIFICANT_COEFF_X_PREFIX] + (i >> ctx_shift) + ctx_offset))
        i++;
    *last_scx_prefix = i;

    i = 0;
    while (i < max &&
           GET_CABAC(elem_offset[LAST_SIGNIFICANT_COEFF_Y_PREFIX] + (i >> ctx_shift) + ctx_offset))
        i++;
    *last_scy_prefix = i;
}

static av_always_inline void last_significant_coeff_xy_prefix_decode_c(HEVCContext *s,
                                                   int log2_size, int *last_scx_prefix, int *last_scy_prefix)
{
    int i = 0;
    int max = (log2_size << 1) - 1;
    //int ctx_offset, ctx_shift;

    const int ctx_offset = 15;
    const int ctx_shift = log2_size - 2;

    while (i < max &&
           GET_CABAC(elem_offset[LAST_SIGNIFICANT_COEFF_X_PREFIX] + (i >> ctx_shift) + ctx_offset))
        i++;
    *last_scx_prefix = i;

    i = 0;
    while (i < max &&
           GET_CABAC(elem_offset[LAST_SIGNIFICANT_COEFF_Y_PREFIX] + (i >> ctx_shift) + ctx_offset))
        i++;
    *last_scy_prefix = i;
}

static av_always_inline int last_significant_coeff_suffix_decode(HEVCContext *s,
                                                 int last_significant_coeff_prefix)
{
    int i;
    int length = (last_significant_coeff_prefix >> 1) - 1;
    int value = get_cabac_bypass(&s->HEVClc->cc);

    for (i = 1; i < length; i++)
        value = (value << 1) | get_cabac_bypass(&s->HEVClc->cc);
    return value;
}

static av_always_inline int significant_coeff_group_flag_decode(HEVCContext *s, int ctx_cg)
{
    int inc;

    inc = FFMIN(ctx_cg, 1);

    return GET_CABAC(elem_offset[SIGNIFICANT_COEFF_GROUP_FLAG] + inc);
}

static av_always_inline int significant_coeff_group_flag_decode_c(HEVCContext *s, int ctx_cg)
{
    int inc;

    inc = FFMIN(ctx_cg, 1) + 2;

    return GET_CABAC(elem_offset[SIGNIFICANT_COEFF_GROUP_FLAG] + inc);
}


static av_always_inline int significant_coeff_flag_decode(HEVCContext *s, int coeff_idx,
                                           int offset, const uint8_t *ctx_idx_map)
{
    int inc = ctx_idx_map[coeff_idx] + offset;
    return GET_CABAC(elem_offset[SIGNIFICANT_COEFF_FLAG] + inc);
}

static av_always_inline int significant_coeff_flag_decode_0(HEVCContext *s, int offset)
{
    return GET_CABAC(elem_offset[SIGNIFICANT_COEFF_FLAG] + offset);
}

static av_always_inline int coeff_abs_level_greater1_flag_decode(HEVCContext *s, int inc)
{
    return GET_CABAC(elem_offset[COEFF_ABS_LEVEL_GREATER1_FLAG] + inc);
}

static av_always_inline int coeff_abs_level_greater1_flag_decode_c(HEVCContext *s, int inc)
{
    return GET_CABAC(elem_offset[COEFF_ABS_LEVEL_GREATER1_FLAG] + inc + 16);
}

#if OHCONFIG_ENCRYPTION
static av_always_inline int coeff_abs_level_remaining_decode_enc(HEVCContext *s, int rc_rice_param, int base)
{
    int prefix = 0;
    int suffix = 0;
    int last_coeff_abs_level_remaining;
    int i;
    unsigned int key;

    while (prefix < CABAC_MAX_BIN && get_cabac_bypass(&s->HEVClc->cc))
        prefix++;
    if (prefix == CABAC_MAX_BIN)
        av_log(s->avctx, AV_LOG_ERROR, "CABAC_MAX_BIN : %d\n", prefix);
    if (prefix < 3) {
        unsigned int codeNumber;
        unsigned int res;
        for (i = 0; i < rc_rice_param; i++)
            suffix = (suffix << 1) | get_cabac_bypass(&s->HEVClc->cc);
        codeNumber = (prefix << (rc_rice_param)) + suffix;
        res = suffix;
        if(rc_rice_param==1) {
            if(!(( base ==2 )&& (codeNumber==4 || codeNumber==5) ) ) {
                key = ff_get_key (&s->HEVClc->dbs_g, 1);
                codeNumber=(prefix << (rc_rice_param)) +
                (((s->HEVClc->prev_pos^key ) & 1)^suffix);
                s->HEVClc->prev_pos=res;
            }
        } else
            if(rc_rice_param==2) {
                if( base ==1) {
                    key = ff_get_key (&s->HEVClc->dbs_g, 2);
                    suffix=(suffix+4-((s->HEVClc->prev_pos^key) & 3)) & 3;
                    codeNumber=(prefix << (2)) + suffix;
                    s->HEVClc->prev_pos=res;
                } else
                    if( base ==2) {
                        if(codeNumber<=7 || codeNumber>=12){
                            key = ff_get_key (&s->HEVClc->dbs_g, 2);
                            suffix=(suffix+4-((s->HEVClc->prev_pos^key) & 3)) & 3;
                            codeNumber=(prefix << (2)) + suffix;
                            s->HEVClc->prev_pos=res;
                        }
                        else
                            if(codeNumber<10) {
                                key = ff_get_key (&s->HEVClc->dbs_g, 1);
                                suffix=(suffix+2-((s->HEVClc->prev_pos^key) & 1)) & 1;
                                codeNumber=(prefix << (2)) + suffix;
                                s->HEVClc->prev_pos=res;
                            }
                    } else { //base=3
                        if(codeNumber<=7 || codeNumber>11){
                            key = ff_get_key (&s->HEVClc->dbs_g, 2);
                            suffix=(suffix+4-((s->HEVClc->prev_pos^key) & 3)) & 3;
                            codeNumber=(prefix << (2)) + suffix;
                            s->HEVClc->prev_pos=res;
                        } else {
                            key = ff_get_key (&s->HEVClc->dbs_g, 1);
                            codeNumber=(prefix << (2)) + (suffix&2)
                            +(((s->HEVClc->prev_pos^key ) & 1)^(suffix&1));
                            s->HEVClc->prev_pos=res;
                        }
                    }
            } else
                if(rc_rice_param==3) {
                    if( base ==1) {
                        key = ff_get_key (&s->HEVClc->dbs_g, 3);
                        suffix=(suffix+8-((s->HEVClc->prev_pos^key) & 7)) & 7;
                        codeNumber=(prefix << (3)) + suffix;
                        s->HEVClc->prev_pos=res;
                    }
                    else if( base ==2) {
                        if(codeNumber<=15 || codeNumber>23){
                            key = ff_get_key (&s->HEVClc->dbs_g, 3);
                            suffix=(suffix+8-((s->HEVClc->prev_pos^key) & 7)) & 7;
                            codeNumber=(prefix << (3)) + suffix;
                            s->HEVClc->prev_pos=res;
                        } else
                            if(codeNumber<=19){
                                key = ff_get_key (&s->HEVClc->dbs_g, 2);
                                suffix=(suffix+4-((s->HEVClc->prev_pos^key) & 3)) & 3;
                                codeNumber=(prefix << (3)) + suffix;
                                s->HEVClc->prev_pos=res;
                            } else
                                if(codeNumber<=21){
                                    key = ff_get_key (&s->HEVClc->dbs_g, 1);
                                    suffix=4+(((s->HEVClc->prev_pos^key) & 1)^(suffix&1));
                                    codeNumber=(prefix << (rc_rice_param)) + suffix;
                                    s->HEVClc->prev_pos=res;
                                }
                    } else {//base=3
                        if(codeNumber<=15 || codeNumber>23) {
                            key = ff_get_key (&s->HEVClc->dbs_g, 3);
                            suffix=(suffix+8-((s->HEVClc->prev_pos^key) & 7)) & 7;
                            codeNumber=(prefix << (3)) + suffix;
                            s->HEVClc->prev_pos=res;
                        } else
                            if(codeNumber<=19) {
                                key = ff_get_key (&s->HEVClc->dbs_g, 2);
                                suffix=(suffix+4-((s->HEVClc->prev_pos^key) & 3)) & 3;
                                codeNumber=(prefix << (3)) + suffix;
                                s->HEVClc->prev_pos=res;
                            } else
                                if(codeNumber<=23) {
                                    key = ff_get_key (&s->HEVClc->dbs_g, 1);
                                    suffix=(suffix&6)+(((s->HEVClc->prev_pos^key) & 1)^(suffix&1));
                                    codeNumber=(prefix << (rc_rice_param)) + suffix;
                                    s->HEVClc->prev_pos=res;
                                }
                    }
                } else
                    if(rc_rice_param==4) {
                        if( base ==1) {
                            key = ff_get_key (&s->HEVClc->dbs_g, 4);
                            suffix=(suffix+16-((s->HEVClc->prev_pos^key) & 15)) & 15;
                            codeNumber=(prefix << (4)) + suffix;
                            s->HEVClc->prev_pos=res;
                        }
                        else if( base ==2) {
                            if(codeNumber<=31 || codeNumber>47){
                                key = ff_get_key (&s->HEVClc->dbs_g, 4);
                                suffix=(suffix+16-((s->HEVClc->prev_pos^key) & 15)) & 15;
                                codeNumber=(prefix << (4)) + suffix;
                                s->HEVClc->prev_pos=res;
                            }
                            else
                                if(codeNumber<=39){

                                    key = ff_get_key (&s->HEVClc->dbs_g, 3);
                                    suffix=(suffix+8-((s->HEVClc->prev_pos^key) & 7)) & 7;
                                    codeNumber=(prefix << (4)) + suffix;
                                    s->HEVClc->prev_pos=res;
                                } else
                                    if(codeNumber<=43){
                                        key = ff_get_key (&s->HEVClc->dbs_g, 2);
                                        suffix=8+(((suffix&3)+4-((s->HEVClc->prev_pos^key) & 3)) & 3);
                                        codeNumber=(prefix << (4)) + suffix;
                                        s->HEVClc->prev_pos=res;
                                    } else
                                        if(codeNumber<=45) {
                                            key = ff_get_key (&s->HEVClc->dbs_g, 1);
                                            suffix=12+((suffix&1)^((s->HEVClc->prev_pos^key) & 1));
                                            codeNumber=(prefix << (4)) + suffix;
                                            s->HEVClc->prev_pos=res;
                                        }

                        } else {//base=3
                            if(codeNumber<=31 || codeNumber>47){
                                key = ff_get_key (&s->HEVClc->dbs_g, 4);
                                suffix=(suffix+16-((s->HEVClc->prev_pos^key) & 15)) & 15;
                                codeNumber=(prefix << (4)) + suffix;
                                s->HEVClc->prev_pos=res;
                            }
                            else
                                if(codeNumber<=39) {
                                    key         = ff_get_key (&s->HEVClc->dbs_g, 3);
                                    suffix      = (suffix+8-((s->HEVClc->prev_pos^key) & 7)) & 7;
                                    codeNumber  =(prefix << (4)) + suffix;
                                    s->HEVClc->prev_pos = res;
                                } else
                                    if(codeNumber<=43){
                                        key         = ff_get_key (&s->HEVClc->dbs_g, 2);
                                        suffix      = 8+(((suffix&3)+4-((s->HEVClc->prev_pos^key) & 3)) & 3);
                                        codeNumber  = (prefix << (4)) + suffix;
                                        s->HEVClc->prev_pos = res;
                                    } else
                                        if(codeNumber<=47){
                                            key         = ff_get_key (&s->HEVClc->dbs_g, 1);
                                            suffix      = (suffix&14)+((suffix&1)^((s->HEVClc->prev_pos^key) & 1));
                                            codeNumber  = (prefix << 4) + suffix;
                                            s->HEVClc->prev_pos = res;
                                        }
                        }
                    }
        last_coeff_abs_level_remaining = codeNumber;
    } else { // EG code does not change
        int prefix_minus3 = prefix - 3;
        for (i = 0; i < prefix_minus3 + rc_rice_param; i++)
            suffix = (suffix << 1) | get_cabac_bypass(&s->HEVClc->cc);
        key = ff_get_key (&s->HEVClc->dbs_g, prefix_minus3 + rc_rice_param);
        s->HEVClc->prev_pos = suffix - (s->HEVClc->prev_pos^key);
        key = (s->HEVClc->prev_pos&((1<<(prefix_minus3 + rc_rice_param))-1));
        s->HEVClc->prev_pos = suffix;
        suffix = key;
        last_coeff_abs_level_remaining = (((1 << prefix_minus3) + 3 - 1)
                                          << rc_rice_param) + suffix;
    }
    return last_coeff_abs_level_remaining;
}
#endif
static av_always_inline int coeff_abs_level_greater2_flag_decode(HEVCContext *s, int inc)
{
    return GET_CABAC(elem_offset[COEFF_ABS_LEVEL_GREATER2_FLAG] + inc);
}

static av_always_inline int coeff_abs_level_greater2_flag_decode_c(HEVCContext *s, int inc)
{
    return GET_CABAC(elem_offset[COEFF_ABS_LEVEL_GREATER2_FLAG] + inc + 4);
}

static av_always_inline int coeff_abs_level_remaining_decode(HEVCContext *s, int rc_rice_param)
{
    int prefix = 0;
    int suffix = 0;
    int last_coeff_abs_level_remaining;
    int i;

    while (prefix < CABAC_MAX_BIN && get_cabac_bypass(&s->HEVClc->cc))
        prefix++;
    if (prefix == CABAC_MAX_BIN) {
        av_log(s->avctx, AV_LOG_ERROR, "CABAC_MAX_BIN : %d\n", prefix);
        return 0;
    }
    if (prefix < 3) {
        for (i = 0; i < rc_rice_param; i++)
            suffix = (suffix << 1) | get_cabac_bypass(&s->HEVClc->cc);
        last_coeff_abs_level_remaining = (prefix << rc_rice_param) + suffix;
    } else {
        int prefix_minus3 = prefix - 3;
        for (i = 0; i < prefix_minus3 + rc_rice_param; i++)
            suffix = (suffix << 1) | get_cabac_bypass(&s->HEVClc->cc);
        last_coeff_abs_level_remaining = (((1 << prefix_minus3) + 3 - 1)
                                              << rc_rice_param) + suffix;
    }
    return last_coeff_abs_level_remaining;
}

static av_always_inline int coeff_sign_flag_decode(HEVCContext *s, uint8_t nb)
{
    int i;
    int ret = 0;

    for (i = 0; i < nb; i++)
        ret = (ret << 1) | get_cabac_bypass(&s->HEVClc->cc);
#if OHCONFIG_ENCRYPTION
    if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_TRANSF_COEFF_SIGNS))
      return ret^ff_get_key (&s->HEVClc->dbs_g, nb);
#endif
    return ret;
}

static const int qp_c[] = { 29, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37 };

static const uint8_t rem6[51 + 4 * 6 + 1] = {
    0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2,
    3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5,
    0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3,
    4, 5, 0, 1, 2, 3, 4, 5, 0, 1
};

static const uint8_t div6[51 + 4 * 6 + 1] = {
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3,  3,  3,
    3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6,  6,  6,
    7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10,
    10, 10, 11, 11, 11, 11, 11, 11, 12, 12
};

static const uint8_t level_scale[] = { 40, 45, 51, 57, 64, 72 };

static void av_always_inline derive_quant_parameters(HEVCContext *s,HEVCLocalContext *lc,  HEVCTransformContext *tr_ctx, HEVCQuantContext *quant_ctx){
//    HEVCTransformContext *tr_ctx    = &lc->transform_ctx;
//    HEVCQuantContext     *quant_ctx = &tr_ctx->quant_ctx;


    int qp_y = lc->qp_y;

    quant_ctx->qp = qp_y + s->ps.sps->qp_bd_offset;

    quant_ctx->shift    = s->ps.sps->bit_depth[CHANNEL_TYPE_LUMA] + tr_ctx->log2_trafo_size + 10 - tr_ctx->log2_transform_range;
    quant_ctx->add      = 1 << (quant_ctx->shift - 1);
    quant_ctx->scale    = level_scale[rem6[quant_ctx->qp]] << (div6[quant_ctx->qp]);
    quant_ctx->scale_m  = 16; // default when no custom scaling lists.
    quant_ctx->dc_scale = 16;

    if (s->ps.sps->scaling_list_enabled_flag && !(tr_ctx->transform_skip_flag && tr_ctx->log2_trafo_size > 2)) {
        const ScalingList *sl = s->ps.pps->pps_scaling_list_data_present_flag ?
                    &s->ps.pps->scaling_list : &s->ps.sps->scaling_list;
        int matrix_id = lc->cu.pred_mode != MODE_INTRA;

        matrix_id = 3 * matrix_id;

        tr_ctx->scale_matrix = sl->sl[tr_ctx->log2_tr_size_minus2][matrix_id];
        if (tr_ctx->log2_trafo_size >= 4)
            quant_ctx->dc_scale = sl->sl_dc[tr_ctx->log2_trafo_size - 4][matrix_id];
    }

}

static void av_always_inline derive_quant_parameters_c(HEVCContext *s, HEVCLocalContext *lc, HEVCTransformContext *tr_ctx, HEVCQuantContext *quant_ctx, int c_idx){
//    HEVCTransformContext *tr_ctx    = &lc->transform_ctx;
//    HEVCQuantContext     *quant_ctx = &tr_ctx->quant_ctx;


    int qp_y = lc->qp_y;

    int qp_i, offset;

    if (c_idx == 1)//FIXME : we could avoid to compute slice_offset + PPS offset for each TU
        offset = s->ps.pps->pps_cb_qp_offset + s->sh.slice_cb_qp_offset +
                lc->tu.cu_qp_offset_cb;
    else
        offset = s->ps.pps->pps_cr_qp_offset + s->sh.slice_cr_qp_offset +
                lc->tu.cu_qp_offset_cr;

    qp_i = av_clip(qp_y + offset, - s->ps.sps->qp_bd_offset, 57);

    if (s->ps.sps->chroma_format_idc == 1) {
        if (qp_i < 30)
            quant_ctx->qp = qp_i;
        else if (qp_i > 43)
            quant_ctx->qp = qp_i - 6;
        else
            quant_ctx->qp = qp_c[qp_i - 30];
    } else {
        if (qp_i > 51)
            quant_ctx->qp = 51;
        else
            quant_ctx->qp = qp_i;
    }

    quant_ctx->qp += s->ps.sps->qp_bd_offset;

    quant_ctx->shift    = s->ps.sps->bit_depth[CHANNEL_TYPE_CHROMA] + tr_ctx->log2_trafo_size + 10 - tr_ctx->log2_transform_range;
    quant_ctx->add      = 1 << (quant_ctx->shift - 1);
    quant_ctx->scale    = level_scale[rem6[quant_ctx->qp]] << (div6[quant_ctx->qp]);
    quant_ctx->scale_m  = 16; // default when no custom scaling lists.
    quant_ctx->dc_scale = 16;

    if (s->ps.sps->scaling_list_enabled_flag && !(tr_ctx->transform_skip_flag && tr_ctx->log2_trafo_size > 2)) {
        //FIXME we could avoid selecting scaling list here by deriving it at a higher level
        const ScalingList *sl = s->ps.pps->pps_scaling_list_data_present_flag ?
                    &s->ps.pps->scaling_list : &s->ps.sps->scaling_list;

        int matrix_id = lc->cu.pred_mode != MODE_INTRA;

        matrix_id = 3 * matrix_id + c_idx;

        tr_ctx->scale_matrix = sl->sl[tr_ctx->log2_tr_size_minus2][matrix_id];
        if (tr_ctx->log2_trafo_size >= 4)
            quant_ctx->dc_scale = sl->sl_dc[tr_ctx->log2_trafo_size - 4][matrix_id];
    }
}



static void av_always_inline derive_scanning_direction_vertical(HEVCTransformScanContext *scan_ctx){
    scan_ctx->scan_x_cg = horiz_scan2x2_y;
    scan_ctx->scan_y_cg = horiz_scan2x2_x;
    scan_ctx->scan_x_off = horiz_scan4x4_y;
    scan_ctx->scan_y_off = horiz_scan4x4_x;
    scan_ctx->scan_inv_coeff = vert_scan4x4_inv2;
    scan_ctx->num_coeff = horiz_scan8x8_inv[scan_ctx->last_significant_coeff_x][scan_ctx->last_significant_coeff_y];
    scan_ctx->scan_inv_cg = vert_scan2x2_inv2;
}


static void av_always_inline derive_scanning_direction_diagonal_4x4(HEVCTransformScanContext *scan_ctx){
    int last_x_c = scan_ctx->last_significant_coeff_x & 3;
    int last_y_c = scan_ctx->last_significant_coeff_y & 3;

    scan_ctx->scan_x_off = ff_hevc_diag_scan4x4_x;
    scan_ctx->scan_y_off = ff_hevc_diag_scan4x4_y;
    scan_ctx->num_coeff = diag_scan4x4_inv[last_y_c][last_x_c];

    scan_ctx->scan_x_cg = scan_1x1;
    scan_ctx->scan_y_cg = scan_1x1;
    scan_ctx->scan_inv_coeff = diag_scan4x4_inv2;
    scan_ctx->scan_inv_cg    = hor_scan2x2_inv2;
}

static void av_always_inline derive_scanning_direction_diagonal_8x8(HEVCTransformScanContext *scan_ctx){
    int last_x_c = scan_ctx->last_significant_coeff_x & 3;
    int last_y_c = scan_ctx->last_significant_coeff_y & 3;

    scan_ctx->scan_x_off = ff_hevc_diag_scan4x4_x;
    scan_ctx->scan_y_off = ff_hevc_diag_scan4x4_y;
    scan_ctx->num_coeff = diag_scan4x4_inv[last_y_c][last_x_c];

    scan_ctx->num_coeff += diag_scan2x2_inv[scan_ctx->y_cg_last_sig][scan_ctx->x_cg_last_sig] << 4;
    scan_ctx->scan_x_cg = diag_scan2x2_x;
    scan_ctx->scan_y_cg = diag_scan2x2_y;
    scan_ctx->scan_inv_coeff = diag_scan4x4_inv2;
    scan_ctx->scan_inv_cg    = diag_scan2x2_inv2;

}

static void av_always_inline derive_scanning_direction_diagonal_16x16(HEVCTransformScanContext *scan_ctx){
    int last_x_c = scan_ctx->last_significant_coeff_x & 3;
    int last_y_c = scan_ctx->last_significant_coeff_y & 3;

    scan_ctx->scan_x_off = ff_hevc_diag_scan4x4_x;
    scan_ctx->scan_y_off = ff_hevc_diag_scan4x4_y;
    scan_ctx->num_coeff = diag_scan4x4_inv[last_y_c][last_x_c];

    scan_ctx->num_coeff += diag_scan4x4_inv[scan_ctx->y_cg_last_sig][scan_ctx->x_cg_last_sig] << 4;
    scan_ctx->scan_x_cg = ff_hevc_diag_scan4x4_x;
    scan_ctx->scan_y_cg = ff_hevc_diag_scan4x4_y;
    scan_ctx->scan_inv_coeff = diag_scan4x4_inv2;
    scan_ctx->scan_inv_cg    = diag_scan4x4_inv2;
}

static void av_always_inline derive_scanning_direction_diagonal_32x32(HEVCTransformScanContext *scan_ctx){
    int last_x_c = scan_ctx->last_significant_coeff_x & 3;
    int last_y_c = scan_ctx->last_significant_coeff_y & 3;

    scan_ctx->scan_x_off = ff_hevc_diag_scan4x4_x;
    scan_ctx->scan_y_off = ff_hevc_diag_scan4x4_y;
    scan_ctx->num_coeff = diag_scan4x4_inv[last_y_c][last_x_c];

    scan_ctx->num_coeff += diag_scan8x8_inv[scan_ctx->y_cg_last_sig][scan_ctx->x_cg_last_sig] << 4;
    scan_ctx->scan_x_cg = ff_hevc_diag_scan8x8_x;
    scan_ctx->scan_y_cg = ff_hevc_diag_scan8x8_y;
    scan_ctx->scan_inv_coeff = diag_scan4x4_inv2;
    scan_ctx->scan_inv_cg    = diag_scan8x8_inv2;
}


static void av_always_inline derive_scanning_direction_horizontal(HEVCTransformScanContext *scan_ctx){
    scan_ctx->scan_x_cg  = horiz_scan2x2_x;
    scan_ctx->scan_y_cg  = horiz_scan2x2_y;
    scan_ctx->scan_x_off = horiz_scan4x4_x;
    scan_ctx->scan_y_off = horiz_scan4x4_y;
    scan_ctx->scan_inv_coeff = hor_scan4x4_inv2;
    scan_ctx->scan_inv_cg    = hor_scan2x2_inv2;
    scan_ctx->num_coeff = horiz_scan8x8_inv[scan_ctx->last_significant_coeff_y][scan_ctx->last_significant_coeff_x];
}

static void av_always_inline derive_scanning_direction(HEVCTransformScanContext *scan_ctx, int tr_size, int scan_idx){
    if(scan_idx == SCAN_DIAG){
        switch (tr_size) {
        case 0:
            derive_scanning_direction_diagonal_4x4(scan_ctx);
            break;
        case 1:
            derive_scanning_direction_diagonal_8x8(scan_ctx);
            break;
        case 2:
            derive_scanning_direction_diagonal_16x16(scan_ctx);
            break;
        case 3:
            derive_scanning_direction_diagonal_32x32(scan_ctx);
            break;
        default:
            break;
        }
    } else {
        switch (scan_idx){
        case SCAN_HORIZ:
            derive_scanning_direction_horizontal(scan_ctx);
            break;
        default:
            derive_scanning_direction_vertical(scan_ctx);
            break;
        }
    }
}

//static void av_always_inline (*derive_scan[3][4])(HEVCTransformScanContext *scan_ctx)={
//{derive_scanning_direction_diagonal_4x4, derive_scanning_direction_diagonal_8x8, derive_scanning_direction_diagonal_16x16, derive_scanning_direction_diagonal_32x32},
//{derive_scanning_direction_horizontal, derive_scanning_direction_horizontal, derive_scanning_direction_horizontal, derive_scanning_direction_horizontal},
//{derive_scanning_direction_vertical, derive_scanning_direction_vertical, derive_scanning_direction_vertical, derive_scanning_direction_vertical},
//        };

static void av_always_inline decode_and_derive_scanning_params(HEVCContext *s, HEVCLocalContext *lc, HEVCTransformScanContext *scan_ctx,int scan_idx){

    if (scan_ctx->last_significant_coeff_x > 3) {
        int suffix = last_significant_coeff_suffix_decode(s, scan_ctx->last_significant_coeff_x);
        scan_ctx->last_significant_coeff_x = (1 << ((scan_ctx->last_significant_coeff_x >> 1) - 1)) *
                (2 + (scan_ctx->last_significant_coeff_x & 1)) +
                suffix;
    }

    if (scan_ctx->last_significant_coeff_y > 3) {
        int suffix = last_significant_coeff_suffix_decode(s, scan_ctx->last_significant_coeff_y);
        scan_ctx->last_significant_coeff_y = (1 << ((scan_ctx->last_significant_coeff_y >> 1) - 1)) *
                (2 + (scan_ctx->last_significant_coeff_y & 1)) +
                suffix;
    }

    if (scan_idx == SCAN_VERT)
        FFSWAP(int, scan_ctx->last_significant_coeff_x, scan_ctx->last_significant_coeff_y);

    //derive last significant cg
    scan_ctx->x_cg_last_sig = scan_ctx->last_significant_coeff_x >> 2;
    scan_ctx->y_cg_last_sig = scan_ctx->last_significant_coeff_y >> 2;

}

void ff_hevc_hls_transform(HEVCContext *s,HEVCLocalContext *lc,int x0,int y0,int log2_cb_size){

    HEVCTransformContext     *tr_ctx   = &lc->transform_ctx;
    HEVCTransformScanContext *scan_ctx = &tr_ctx->scan_ctx;

    int pred_mode_intra = lc->tu.intra_pred_mode;

    int i;
    ptrdiff_t stride = s->frame->linesize[0];
    int hshift = s->ps.sps->hshift[0];
    int vshift = s->ps.sps->vshift[0];

    uint8_t *dst = &s->frame->data[0][(y0 >> vshift) * stride +
            ((x0 >> hshift) << s->ps.sps->pixel_shift[0])];

    int16_t *coeffs = lc->tu.coeffs[0];

#if OHCONFIG_AMT
    if (!tr_ctx->transform_skip_flag && s->HEVClc->cu.emt_cu_flag )
    {
        if (s->HEVClc->cu.pred_mode == MODE_INTER){
            s->HEVClc->tu.emt_tu_idx = ff_hevc_emt_tu_idx_decode(s, log2_cb_size);
        } else if (s->HEVClc->cu.pred_mode == MODE_INTRA) {
            s->HEVClc->tu.emt_tu_idx =  tr_ctx->num_significant_coeffs > EMT_SIGNUM_THR ? ff_hevc_emt_tu_idx_decode(s, log2_cb_size) : 0;
        }
    }
#endif

    if (lc->cu.cu_transquant_bypass_flag) {
        if (tr_ctx->explicit_rdpcm_flag || (s->ps.sps->implicit_rdpcm_enabled_flag &&
                                            (pred_mode_intra == 10 || pred_mode_intra == 26))) {
            int mode = s->ps.sps->implicit_rdpcm_enabled_flag ? (pred_mode_intra == 26) : tr_ctx->explicit_rdpcm_dir_flag;

            s->hevcdsp.transform_rdpcm(coeffs, tr_ctx->log2_trafo_size, mode);
        }
    } else {
        if (tr_ctx->transform_skip_flag) {
            int rot = s->ps.sps->transform_skip_rotation_enabled_flag &&
                    tr_ctx->log2_trafo_size == 2 &&
                    lc->cu.pred_mode == MODE_INTRA;
            if (rot) {
                for (i = 0; i < 8; i++)
                    FFSWAP(int16_t, coeffs[i], coeffs[16 - i - 1]);
            }

            s->hevcdsp.transform_skip(coeffs, tr_ctx->log2_trafo_size);

            if (tr_ctx->explicit_rdpcm_flag) {
                s->hevcdsp.transform_rdpcm(coeffs, tr_ctx->log2_trafo_size, tr_ctx->explicit_rdpcm_dir_flag);
            } else if ((s->ps.sps->implicit_rdpcm_enabled_flag &&
                        lc->cu.pred_mode == MODE_INTRA &&
                        (pred_mode_intra == 10 || pred_mode_intra == 26))) {
                int mode = s->ps.sps->implicit_rdpcm_enabled_flag ? (pred_mode_intra == 26) : 0;

                s->hevcdsp.transform_rdpcm(coeffs, tr_ctx->log2_trafo_size, mode);
            }
#if OHCONFIG_AMT
        } else if ( s->HEVClc->cu.emt_cu_flag || s->ps.sps->use_intra_emt == 1 || s->ps.sps->use_inter_emt == 1 ) {
            enum IntraPredMode ucMode = INTER_MODE_IDX;
            DECLARE_ALIGNED(32, int16_t, tmp[MAX_TU_SIZE * MAX_TU_SIZE])={0};
            int tu_emt_Idx = (!s->HEVClc->cu.emt_cu_flag ) ? DCT2_EMT : s->HEVClc->tu.emt_tu_idx ;
            int tr_idx_h  = DCT_II;
            int tr_idx_v  = DCT_II;
            const int clip_min  = -(1 << tr_ctx->log2_transform_range);
            const int clip_max  =  (1 << tr_ctx->log2_transform_range) - 1;

            if (s->HEVClc->cu.pred_mode == MODE_INTRA){
                ucMode = pred_mode_intra;
            }
            if (tu_emt_Idx != DCT2_EMT){
                if ( ucMode != INTER_MODE_IDX){
                    tr_idx_h = emt_intra_subset_select[emt_intra_mode2tr_idx_h[ucMode]][(tu_emt_Idx) & 1];
                    tr_idx_v = emt_intra_subset_select[emt_intra_mode2tr_idx_v[ucMode]][(tu_emt_Idx) >> 1];
                } else {
                    tr_idx_h = emt_inter_subset_select[(tu_emt_Idx) & 1];
                    tr_idx_v = emt_inter_subset_select[(tu_emt_Idx) >> 1];
                }
            }
#define TEST_AVX2 0
 #if !TEST_AVX2
            s->hevcdsp.idct2_emt_v[tr_idx_v][tr_ctx->log2_tr_size_minus2](coeffs, tmp, 0, clip_min, clip_max);
            s->hevcdsp.idct2_emt_h[tr_idx_h][tr_ctx->log2_tr_size_minus2](tmp, coeffs, tr_ctx->log2_transform_range, clip_min, clip_max);
#else
            s->hevcdsp.idct2_emt_v2[tr_idx_v][tr_ctx->log2_tr_size_minus2](lc->cg_coeffs[0], tmp, tr_ctx->significant_cg_flag[0], 0, clip_min, clip_max);
            s->hevcdsp.idct2_emt_h2[tr_idx_h][tr_ctx->log2_tr_size_minus2](tmp, coeffs,tr_ctx->significant_cg_flag[0], tr_ctx->log2_transform_range, clip_min, clip_max);
#endif
#endif
        } else if (lc->cu.pred_mode == MODE_INTRA && tr_ctx->log2_trafo_size == 2) {
            s->hevcdsp.idct_4x4_luma(coeffs);
        } else {
            int max_xy = FFMAX(scan_ctx->last_significant_coeff_x, scan_ctx->last_significant_coeff_y);
            if (max_xy == 0)
                s->hevcdsp.idct_dc[tr_ctx->log2_tr_size_minus2](coeffs);
            else {
                int col_limit = scan_ctx->last_significant_coeff_x + scan_ctx->last_significant_coeff_y + 4;
                if (max_xy < 4)
                    col_limit = FFMIN(4, col_limit);
                else if (max_xy < 8)
                    col_limit = FFMIN(8, col_limit);
                else if (max_xy < 12)
                    col_limit = FFMIN(24, col_limit);
                s->hevcdsp.idct[tr_ctx->log2_tr_size_minus2](coeffs, col_limit);
            }
        }
    }
    if (lc->tu.cross_pf) {
        int16_t *coeffs_y = lc->tu.coeffs[0];

        for (i = 0; i < (tr_ctx->transform_size * tr_ctx->transform_size); i++) {
            coeffs[i] = coeffs[i] + ((lc->tu.res_scale_val * coeffs_y[i]) >> 3);
        }
    }
    s->hevcdsp.transform_add[tr_ctx->log2_trafo_size-2](dst, coeffs, stride);
}

void ff_hevc_hls_transform_c(HEVCContext *s,HEVCLocalContext *lc,int x0,int y0, int c_idx,int log2_cb_size){

    HEVCTransformContext     *tr_ctx   = &lc->transform_ctx;
    HEVCTransformScanContext *scan_ctx = &tr_ctx->scan_ctx;

    int pred_mode_intra = lc->tu.intra_pred_mode_c;

    int i;
    ptrdiff_t stride = s->frame->linesize[1];
    int hshift = s->ps.sps->hshift[1];
    int vshift = s->ps.sps->vshift[1];

    uint8_t *dst = &s->frame->data[c_idx][(y0 >> vshift) * stride +
            ((x0 >> hshift) << s->ps.sps->pixel_shift[1])];

    int16_t *coeffs = lc->tu.coeffs[1];

    if (lc->cu.cu_transquant_bypass_flag) {
        if (tr_ctx->explicit_rdpcm_flag || (s->ps.sps->implicit_rdpcm_enabled_flag &&
                                            (pred_mode_intra == 10 || pred_mode_intra == 26))) {
            int mode = s->ps.sps->implicit_rdpcm_enabled_flag ? (pred_mode_intra == 26) : tr_ctx->explicit_rdpcm_dir_flag;

            s->hevcdsp.transform_rdpcm(coeffs, tr_ctx->log2_trafo_size, mode);
        }
    } else {
        if (tr_ctx->transform_skip_flag) {
            int rot = s->ps.sps->transform_skip_rotation_enabled_flag &&
                    tr_ctx->log2_trafo_size == 2 &&
                    lc->cu.pred_mode == MODE_INTRA;
            if (rot) {
                for (i = 0; i < 8; i++)
                    FFSWAP(int16_t, coeffs[i], coeffs[16 - i - 1]);
            }

            s->hevcdsp.transform_skip(coeffs, tr_ctx->log2_trafo_size);

            if (tr_ctx->explicit_rdpcm_flag) {
                s->hevcdsp.transform_rdpcm(coeffs, tr_ctx->log2_trafo_size, tr_ctx->explicit_rdpcm_dir_flag);
            } else if ((s->ps.sps->implicit_rdpcm_enabled_flag &&
                        lc->cu.pred_mode == MODE_INTRA &&
                        (pred_mode_intra == 10 || pred_mode_intra == 26))) {
                int mode = s->ps.sps->implicit_rdpcm_enabled_flag ? (pred_mode_intra == 26) : 0;

                s->hevcdsp.transform_rdpcm(coeffs, tr_ctx->log2_trafo_size, mode);
            }
#if OHCONFIG_AMT
        } else if ( s->HEVClc->cu.emt_cu_flag || s->ps.sps->use_intra_emt == 1 || s->ps.sps->use_inter_emt == 1 ) {
            enum IntraPredMode ucMode = INTER_MODE_IDX;
            DECLARE_ALIGNED(32, int16_t, tmp[MAX_TU_SIZE * MAX_TU_SIZE]);
            int tu_emt_Idx = DCT2_EMT;
            int tr_idx_h  = DCT_II;
            int tr_idx_v  = DCT_II;
            const int clip_min  = -(1 << tr_ctx->log2_transform_range);
            const int clip_max  =  (1 << tr_ctx->log2_transform_range) - 1;

            if (s->HEVClc->cu.pred_mode == MODE_INTRA){
                ucMode = pred_mode_intra;
            }
            if (tu_emt_Idx != DCT2_EMT){
                if ( ucMode != INTER_MODE_IDX){
                    tr_idx_h = emt_intra_subset_select[emt_intra_mode2tr_idx_h[ucMode]][(tu_emt_Idx) & 1];
                    tr_idx_v = emt_intra_subset_select[emt_intra_mode2tr_idx_v[ucMode]][(tu_emt_Idx) >> 1];
                } else {
                    tr_idx_h = emt_inter_subset_select[(tu_emt_Idx) & 1];
                    tr_idx_v = emt_inter_subset_select[(tu_emt_Idx) >> 1];
                }
            }
//#define TEST_AVX2 1
 #if !TEST_AVX2
                s->hevcdsp.idct2_emt_v[tr_idx_v][tr_ctx->log2_tr_size_minus2](coeffs, tmp, 0, clip_min, clip_max);
                s->hevcdsp.idct2_emt_h[tr_idx_h][tr_ctx->log2_tr_size_minus2](tmp, coeffs, tr_ctx->log2_transform_range, clip_min, clip_max);
 #else
                s->hevcdsp.idct2_emt_v2[tr_idx_v][tr_ctx->log2_tr_size_minus2](lc->cg_coeffs[1], tmp, tr_ctx->significant_cg_flag[1], 0, clip_min, clip_max);
                s->hevcdsp.idct2_emt_h2[tr_idx_h][tr_ctx->log2_tr_size_minus2](tmp, coeffs,tr_ctx->significant_cg_flag[1], tr_ctx->log2_transform_range, clip_min, clip_max);
#endif
#endif
        } else {
            int max_xy = FFMAX(scan_ctx->last_significant_coeff_x, scan_ctx->last_significant_coeff_y);
            if (max_xy == 0)
                s->hevcdsp.idct_dc[tr_ctx->log2_tr_size_minus2](coeffs);
            else {
                int col_limit = scan_ctx->last_significant_coeff_x + scan_ctx->last_significant_coeff_y + 4;
                if (max_xy < 4)
                    col_limit = FFMIN(4, col_limit);
                else if (max_xy < 8)
                    col_limit = FFMIN(8, col_limit);
                else if (max_xy < 12)
                    col_limit = FFMIN(24, col_limit);
                s->hevcdsp.idct[tr_ctx->log2_tr_size_minus2](coeffs, col_limit);
            }
        }
    }
    if (lc->tu.cross_pf) {
        int16_t *coeffs_y = lc->tu.coeffs[0];

        for (i = 0; i < (tr_ctx->transform_size * tr_ctx->transform_size); i++) {
            coeffs[i] = coeffs[i] + ((lc->tu.res_scale_val * coeffs_y[i]) >> 3);
        }
    }
    s->hevcdsp.transform_add[tr_ctx->log2_trafo_size-2](dst, coeffs, stride);
}




static const uint8_t ctx_idx_map[] = {
    0, 1, 4, 5, 2, 3, 4, 5, 6, 6, 8, 8, 7, 7, 8, 8, // log2_trafo_size == 2 //FIXME I don't understand the order derivation for 4x4
    1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, // prev_sig == 0
    2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, // prev_sig == 1
    2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, // prev_sig == 2
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2  // default
};

static void av_always_inline decode_significance_map_c(HEVCContext *av_restrict s, HEVCTransformContext *av_restrict tr_ctx, CGContext *av_restrict cg, int n_end, int tr_skip_or_bypass)
{
    const uint8_t *ctx_idx_map_p;
    int scf_offset;
    int n;

    if (tr_skip_or_bypass) {
        ctx_idx_map_p = (uint8_t*) &ctx_idx_map[4 * 16];//default line
        scf_offset = 14 + 27;
    } else {

        if (tr_ctx->log2_trafo_size == 2) {
            ctx_idx_map_p = (uint8_t*) &ctx_idx_map[0];
            scf_offset = 27;
        } else {
            ctx_idx_map_p = (uint8_t*) &ctx_idx_map[(cg->prev_sig + 1) << 4]; //select ctx_map based on prev_sig
            if (tr_ctx->log2_trafo_size == 3)
                scf_offset += 27 + 9;
            else
                scf_offset += 27 + 12;
        }
    }

    //decode coeffs significance map in current CG
    for (n = n_end; n > 0; n--) {
        int coeff_idx = tr_ctx->scan_ctx.scan_inv_coeff[n];
        if (significant_coeff_flag_decode(s, coeff_idx, scf_offset, ctx_idx_map_p)) {
            cg->significant_coeff_flag_idx[cg->num_significant_coeff_in_cg] = n; // store _id in decoding order
            cg->num_significant_coeff_in_cg++;
            cg->implicit_non_zero_coeff = 0;//there isn't any implicit nz coeff since we already read one
        }
    }

    // if there isn't any nz coeff which was implicit,
    if (cg->implicit_non_zero_coeff == 0) {
        if (tr_skip_or_bypass) {//transform is skipped
            scf_offset = 16 + 27;
        } else { // transform will be performed
            if (cg->is_dc_cg) {
                scf_offset = 27;
            } else {
                scf_offset += 2;
            }
        }//decode significant coeff flag

        if (significant_coeff_flag_decode_0(s, scf_offset) == 1) {
            cg->significant_coeff_flag_idx[cg->num_significant_coeff_in_cg] = 0;
            cg->num_significant_coeff_in_cg++;
        }
        //FIXME we could count significant coeffs per CGs here
    } else {//There exists a nz implicit coeff (only first coeff is nz)
        cg->significant_coeff_flag_idx[cg->num_significant_coeff_in_cg] = 0; // store 0 idx into sig_coeff id_map.
        cg->num_significant_coeff_in_cg++;
    }
}

static void av_always_inline decode_significance_map(HEVCContext *s, HEVCTransformContext *av_restrict tr_ctx, CGContext *av_restrict cg, int n_end, int tr_skip_or_bypass, int scan_idx)
{
    const uint8_t *ctx_idx_map_p;
    int scf_offset = 0;
    int n;

    if (tr_skip_or_bypass) {
        ctx_idx_map_p = (uint8_t*) &ctx_idx_map[4 * 16];//default line
        scf_offset = 40;
    } else {
        if (tr_ctx->log2_trafo_size == 2) {
            ctx_idx_map_p = (uint8_t*) &ctx_idx_map[0];
        } else {
            ctx_idx_map_p = (uint8_t*) &ctx_idx_map[(cg->prev_sig + 1) << 4]; //select ctx_map based on prev_sig
            if (!cg->is_dc_cg)//not DC_cg
                scf_offset += 3;
            if (tr_ctx->log2_trafo_size == 3) {
                scf_offset += (scan_idx == SCAN_DIAG) ? 9 : 15;
            } else {
                scf_offset += 21;
            }
        }
    }

    //decode coeffs significance map in current CG
    for (n = n_end; n > 0; n--) {
        int coeff_idx = tr_ctx->scan_ctx.scan_inv_coeff[n];
        if (significant_coeff_flag_decode(s, coeff_idx, scf_offset, ctx_idx_map_p)) {
            cg->significant_coeff_flag_idx[cg->num_significant_coeff_in_cg] = n; // store _id in decoding order
            cg->num_significant_coeff_in_cg++;
            cg->implicit_non_zero_coeff = 0;//there isn't any implicit nz coeff since we already read one
        }
    }

    if (!cg->implicit_non_zero_coeff) {
        if (tr_skip_or_bypass) {//transform is skipped
            scf_offset = 42;
        } else { // transform will be performed
            if (cg->is_dc_cg) {
                scf_offset = 0;
            } else {
                scf_offset += 2;
            }
        }
        if (significant_coeff_flag_decode_0(s, scf_offset) == 1) {
            cg->significant_coeff_flag_idx[cg->num_significant_coeff_in_cg] = 0;
            cg->num_significant_coeff_in_cg++;
        }
        //FIXME we could count significant coeffs per CGs here
    } else {// there exists at least one implicit non zero coeffs in last cg,
        cg->significant_coeff_flag_idx[cg->num_significant_coeff_in_cg] = 0; // store 0 idx into sig_coeff id_map.
        cg->num_significant_coeff_in_cg++;
    }
}

static int64_t av_always_inline scale_and_clip_coeff(HEVCContext *av_restrict s,HEVCTransformContext *av_restrict tr_ctx, HEVCQuantContext *av_restrict quant_ctx, int64_t trans_coeff_level, int x_c, int y_c ){
    if (s->ps.sps->scaling_list_enabled_flag && !(tr_ctx->transform_skip_flag &&
                                                  tr_ctx->log2_trafo_size > 2)) {
        if(y_c || x_c || tr_ctx->log2_trafo_size < 4) {
            int pos;
            switch(tr_ctx->log2_trafo_size) {
            case 3: pos = (y_c << 3) + x_c; break;
            case 4: pos = ((y_c >> 1) << 3) + (x_c >> 1); break;
            case 5: pos = ((y_c >> 2) << 3) + (x_c >> 2); break;
            default: pos = (y_c << 2) + x_c; break;
            }
            quant_ctx->scale_m = tr_ctx->scale_matrix[pos];
        } else {
            quant_ctx->scale_m = quant_ctx->dc_scale;
        }
    }
    trans_coeff_level = (trans_coeff_level * (int64_t)quant_ctx->scale * (int64_t)quant_ctx->scale_m + quant_ctx->add) >> quant_ctx->shift;
    if(trans_coeff_level < 0) {
        if((~trans_coeff_level) & 0xFffffffffff8000)
            trans_coeff_level = -32768;
    } else {
        if(trans_coeff_level & 0xffffffffffff8000)
            trans_coeff_level = 32767;
    }
    return trans_coeff_level;
}

static void av_always_inline update_rice_statistics(HEVCPersistentRiceContext *av_restrict rice_ctx,int last_coeff_abs_level_remaining ){
    if(!rice_ctx->rice_init) {
        int c_rice_p_init = *(rice_ctx->current_coeff) >> 2;
        if (last_coeff_abs_level_remaining >= (3 << c_rice_p_init))
            *(rice_ctx->current_coeff)+=1;
        else if (2 * last_coeff_abs_level_remaining < (1 << c_rice_p_init))
            if (*(rice_ctx->current_coeff) > 0)
                *(rice_ctx->current_coeff)-=1;
        rice_ctx->rice_init = 1;
    }
}

static const uint8_t last_cg_line[4]={1,2,12,56};

void ff_hevc_hls_coefficients_coding_c(HEVCContext *av_restrict s,
                                int log2_trafo_size, enum ScanType scan_idx,
                                int c_idx
)
{
    const HEVCSPS             *av_restrict sps = s->ps.sps;
    const HEVCPPS             *av_restrict pps = s->ps.pps;
    HEVCLocalContext          *av_restrict lc  = s->HEVClc;
    HEVCTransformContext      *av_restrict tr_ctx   = &lc->transform_ctx;
    HEVCTransformScanContext  *av_restrict scan_ctx = &tr_ctx->scan_ctx;
    HEVCPersistentRiceContext *av_restrict rice_ctx;
    HEVCQuantContext          *av_restrict quant_ctx;
    CGContext                 *av_restrict current_cg = &lc->cg_ctx;

    int tr_skip_or_bypass;
    //could be set into tr_ctx
    int pps_sign_data_hiding_flag;
    int sps_persistent_rice_adaptation_enabled_flag;
    int cu_tr_transquant_bypass_flag = lc->cu.cu_transquant_bypass_flag;
    int sps_explicit_rdpcm_enabled_flag = sps->explicit_rdpcm_enabled_flag;
    int sign_always_hidden;
    int sign_hidden;
    int n_end;
    int greater1_ctx = 1;
    int16_t cg_coeffs[64][16]={{0}};

    int i;
    int num_cg;
    int tr_size_in_cg;
    int16_t * av_restrict coeffs = lc->tu.coeffs[1];

    uint8_t *significant_cg_flag = tr_ctx->significant_cg_flag[1]; // significant CG map;

    //Reset transform context
    //memset(tr_ctx, 0, sizeof(HEVCTransformContext));
    tr_ctx->transform_skip_flag     = 0,
    tr_ctx->num_significant_coeffs  = 0;
    tr_ctx->explicit_rdpcm_dir_flag = 0;
    tr_ctx->explicit_rdpcm_flag     = 0;

    //FIXME Those values could be set in a higher context.
    tr_ctx->log2_transform_range = sps->extended_precision_processing_flag ? FFMAX(15, (sps->bit_depth[CHANNEL_TYPE_CHROMA] + 6) ) : 15;//15;
    tr_ctx->log2_trafo_size      = log2_trafo_size;
    tr_ctx->log2_tr_size_minus2  = log2_trafo_size - 2;
    tr_ctx->transform_size       = 1 << log2_trafo_size;
    tr_size_in_cg = 1 << tr_ctx->log2_tr_size_minus2;

    memset(significant_cg_flag,0,tr_size_in_cg*tr_size_in_cg*sizeof(uint8_t));
    memset(&lc->cg_coeffs[1],0,tr_size_in_cg*tr_size_in_cg*sizeof(uint16_t)*16);

    if (!cu_tr_transquant_bypass_flag && pps->transform_skip_enabled_flag &&
            tr_ctx->log2_trafo_size <= pps->log2_max_transform_skip_block_size) {
        tr_ctx->transform_skip_flag = hevc_transform_skip_flag_decode_c(s);
    }

    tr_skip_or_bypass = sps->transform_skip_context_enabled_flag &&
                    (tr_ctx->transform_skip_flag || cu_tr_transquant_bypass_flag);
    //Reset coeffs buffer
    memset(coeffs, 0, tr_ctx->transform_size * tr_ctx->transform_size * sizeof(int16_t));

    // Derive QP for dequant
    //FIXME this could probably be called outside of the scope of residual coding
    if (!cu_tr_transquant_bypass_flag){
        quant_ctx   = &tr_ctx->quant_ctx;
        derive_quant_parameters_c(s,lc, tr_ctx, quant_ctx, c_idx);
    }

    if (lc->cu.pred_mode == MODE_INTER && sps_explicit_rdpcm_enabled_flag &&
            (tr_ctx->transform_skip_flag || cu_tr_transquant_bypass_flag)) {
        tr_ctx->explicit_rdpcm_flag = explicit_rdpcm_flag_decode_c(s);
        if (tr_ctx->explicit_rdpcm_flag) {
            tr_ctx->explicit_rdpcm_dir_flag = explicit_rdpcm_dir_flag_decode_c(s);
        }
    }

    last_significant_coeff_xy_prefix_decode_c(s, tr_ctx->log2_trafo_size,
                                            &scan_ctx->last_significant_coeff_x, &scan_ctx->last_significant_coeff_y);
    // decode and derive last significant coeff (tu scanning ctx)
    decode_and_derive_scanning_params(s, lc, scan_ctx, scan_idx);

    //derive scanning parameters
    //derive_scan[scan_idx][tr_ctx->log2_tr_size_minus2](scan_ctx);
    derive_scanning_direction(scan_ctx,tr_ctx->log2_tr_size_minus2,scan_idx);

    scan_ctx->num_coeff++;

    num_cg = (scan_ctx->num_coeff - 1) >> 4;

    pps_sign_data_hiding_flag = pps->sign_data_hiding_flag;
    sps_persistent_rice_adaptation_enabled_flag = sps->persistent_rice_adaptation_enabled_flag;

    if(sps_persistent_rice_adaptation_enabled_flag){
        int sb_type;
        rice_ctx = &lc->rice_ctx;
        if (!tr_ctx->transform_skip_flag && !cu_tr_transquant_bypass_flag)
            sb_type = 0;
        else
            sb_type = 1;
        rice_ctx->current_coeff= &rice_ctx->stat_coeff[sb_type];
    }


    sign_always_hidden = (cu_tr_transquant_bypass_flag ||
                          (lc->cu.pred_mode ==  MODE_INTRA  && sps->implicit_rdpcm_enabled_flag
                           &&  tr_ctx->transform_skip_flag  &&
                           (lc->tu.intra_pred_mode_c == 10 || lc->tu.intra_pred_mode_c  ==  26 ))
                           || tr_ctx->explicit_rdpcm_flag);
    if(sign_always_hidden)
        sign_hidden = 0;

    //decode CGs
    for (i = num_cg; i >= 0; i--) {
        int n, m;
        int x_cg, y_cg;
        int x_c, y_c;

        int cg_id = scan_ctx->scan_inv_cg[i];


        current_cg->is_dc_cg = (i == 0);
        current_cg->is_last_cg = (i == num_cg);
        current_cg->is_last_or_dc_cg = (current_cg->is_dc_cg || current_cg->is_last_cg);
        current_cg->implicit_non_zero_coeff = 0;
        current_cg->num_significant_coeff_in_cg = 0;
        current_cg->prev_sig = 0;

        //get current cg position
        x_cg = scan_ctx->scan_x_cg[i];
        y_cg = scan_ctx->scan_y_cg[i];

        //current_cg->x_cg = x_cg;
        //current_cg->y_cg = y_cg;

        // if not last cg and not first cg
        if (!current_cg->is_last_or_dc_cg) {
            int ctx_cg = 0;
            if ((cg_id + 1) % tr_size_in_cg){
                ctx_cg += significant_cg_flag[cg_id + 1];
            }
            if (cg_id < (int)last_cg_line[tr_ctx->log2_tr_size_minus2]){
                ctx_cg += significant_cg_flag[cg_id + tr_size_in_cg];
            }

            significant_cg_flag[cg_id] =
                    significant_coeff_group_flag_decode_c(s, ctx_cg);
            current_cg->implicit_non_zero_coeff = 1; // default init to 1
        } else {//test for implicit non zero cg
            significant_cg_flag[cg_id] = 1;
        }

        // on first iteration we read based on last coeff
        //FIXME we could avoid this in loop check
        if (current_cg->is_last_cg) {
            int last_scan_pos = scan_ctx->num_coeff - (i << 4) - 1;
            n_end = last_scan_pos - 1;
            current_cg->significant_coeff_flag_idx[0] = last_scan_pos;
            current_cg->num_significant_coeff_in_cg = 1;
        } else {//we read from last coeff in cg
            n_end = 15;
        }

        //derive global cabac ctx id ids for the whole CG according to other CGs significance
        if (x_cg < ((tr_ctx->transform_size - 1) >> 2))
            current_cg->prev_sig = !!significant_cg_flag[cg_id + 1];
        if (y_cg < ((tr_ctx->transform_size - 1) >> 2))
            current_cg->prev_sig += (!!significant_cg_flag[cg_id + tr_size_in_cg] << 1);

        if (significant_cg_flag[cg_id] && n_end >= 0)
            decode_significance_map_c(s, tr_ctx, current_cg, n_end, tr_skip_or_bypass);

        n_end = current_cg->num_significant_coeff_in_cg;

        //decode coeffs values
        if (n_end) {
            int c_rice_param = 0;
            int first_greater1_coeff_idx = -1;
            uint8_t coeff_abs_level_greater1_flag[8];
            uint16_t coeff_sign_flag;
            int sum_abs = 0;
            //int sign_hidden;

            // initialize first elem of coeff_bas_level_greater1_flag
            int ctx_set = 0;

            if (!current_cg->is_last_cg && greater1_ctx == 0)
                ctx_set++;

            greater1_ctx = 1;

            //decode coeff_abs_level_greater1_flags
            for (m = 0; m < (n_end > 8 ? 8 : n_end); m++) {//only first 8 coeffs
                int inc = (ctx_set << 2) + greater1_ctx;
                coeff_abs_level_greater1_flag[m] = coeff_abs_level_greater1_flag_decode_c(s, inc);
                if (coeff_abs_level_greater1_flag[m]) {
                    greater1_ctx = 0;
                    if (first_greater1_coeff_idx == -1)
                        first_greater1_coeff_idx = m;
                } else if (greater1_ctx > 0 && greater1_ctx < 3) {
                    greater1_ctx++;
                }
            }

            //pass nb
            if (first_greater1_coeff_idx != -1) {
                coeff_abs_level_greater1_flag[first_greater1_coeff_idx] += coeff_abs_level_greater2_flag_decode_c(s, ctx_set);
            }

            if (!sign_always_hidden){
                current_cg->last_nz_pos_in_cg = current_cg->significant_coeff_flag_idx[0];;
                current_cg->first_nz_pos_in_cg = current_cg->significant_coeff_flag_idx[n_end - 1];
                sign_hidden = (current_cg->last_nz_pos_in_cg - current_cg->first_nz_pos_in_cg >= 4);
            }

            if (!(pps_sign_data_hiding_flag && sign_hidden) ) {
                coeff_sign_flag = coeff_sign_flag_decode(s, current_cg->num_significant_coeff_in_cg) << (16 - current_cg->num_significant_coeff_in_cg);
            } else {
                coeff_sign_flag = coeff_sign_flag_decode(s, current_cg->num_significant_coeff_in_cg - 1) << (16 - (current_cg->num_significant_coeff_in_cg - 1));
            }

            if (sps_persistent_rice_adaptation_enabled_flag) {
                c_rice_param = *(rice_ctx->current_coeff) >> 2;
                rice_ctx->rice_init = 0;
            } else {
                c_rice_param = 0;
            }

            //decode scale and store coeffs values
            for (m = 0; m <  (n_end < 8 ? n_end : 8); m++) {
                int64_t trans_coeff_level;
                n = current_cg->significant_coeff_flag_idx[m];

                x_c = (x_cg << 2) + scan_ctx->scan_x_off[n];
                y_c = (y_cg << 2) + scan_ctx->scan_y_off[n];

                trans_coeff_level = 1 + coeff_abs_level_greater1_flag[m];
                if (trans_coeff_level == ((m == first_greater1_coeff_idx) ? 3 : 2)) {
                    int last_coeff_abs_level_remaining;
#if OHCONFIG_ENCRYPTION
                    if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_TRANSF_COEFFS))
                        last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode_enc(s, c_rice_param, trans_coeff_level);
                    else
#endif
                        last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode(s, c_rice_param);


                    trans_coeff_level += last_coeff_abs_level_remaining;

                    if (sps_persistent_rice_adaptation_enabled_flag){
                        if (trans_coeff_level > (3 << c_rice_param))
                            c_rice_param =  c_rice_param + 1;
                        update_rice_statistics(rice_ctx, last_coeff_abs_level_remaining);
                    } else if (trans_coeff_level > (3 << c_rice_param)){
                        c_rice_param = FFMIN(c_rice_param + 1, 4);
                    }
                }

                if (pps_sign_data_hiding_flag && sign_hidden) {
                    sum_abs += trans_coeff_level;
                    if (n == current_cg->first_nz_pos_in_cg && (sum_abs & 1))
                        trans_coeff_level = -trans_coeff_level;
                }

                if (coeff_sign_flag >> 15)
                    trans_coeff_level = -trans_coeff_level;
                coeff_sign_flag <<= 1;

                //scale (inverse quant) clip and store coeff
                if(!cu_tr_transquant_bypass_flag) {
                    //FIXme add scale function (the scale can be done on the whole coeffs table)
                    trans_coeff_level = scale_and_clip_coeff(s ,tr_ctx, quant_ctx, trans_coeff_level, x_c, y_c);
                }
                //fprintf(stderr,"store coeff xc %d, yc =%d, x_cg: %d, y_cg: %d val: %d\n", x_c, y_c,x_cg,y_cg,trans_coeff_level  );
                coeffs[y_c * tr_ctx->transform_size + x_c] = trans_coeff_level;
                lc->cg_coeffs[1][scan_ctx->scan_inv_cg[i]*16 + scan_ctx->scan_inv_coeff[n]] = trans_coeff_level;
            }

            for (m = 8; m < n_end ; m++) {
                int64_t trans_coeff_level;
                int last_coeff_abs_level_remaining;

                n = current_cg->significant_coeff_flag_idx[m];

                x_c = (x_cg << 2) + scan_ctx->scan_x_off[n];
                y_c = (y_cg << 2) + scan_ctx->scan_y_off[n];

#if OHCONFIG_ENCRYPTION
                if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_TRANSF_COEFFS))
                    last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode_enc(s, c_rice_param, 1);
                else
#endif
                    last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode(s, c_rice_param);

                trans_coeff_level = 1 + last_coeff_abs_level_remaining;
                if (sps_persistent_rice_adaptation_enabled_flag){
                    if (trans_coeff_level > (3 << c_rice_param))
                        c_rice_param =  c_rice_param + 1;
                    update_rice_statistics(rice_ctx, last_coeff_abs_level_remaining);
                } else if (trans_coeff_level > (3 << c_rice_param)){
                    c_rice_param = FFMIN(c_rice_param + 1, 4);
                }

                if (pps_sign_data_hiding_flag && sign_hidden) {
                    sum_abs += trans_coeff_level;
                    if (n == current_cg->first_nz_pos_in_cg && (sum_abs & 1))
                        trans_coeff_level = -trans_coeff_level;
                }

                if (coeff_sign_flag >> 15)
                    trans_coeff_level = -trans_coeff_level;
                coeff_sign_flag <<= 1;

                //scale (inverse quant) clip and store coeff
                if(!cu_tr_transquant_bypass_flag) {
                    //FIXme add scale function (the scale can be done on the whole coeffs table before inverse transform)
                    trans_coeff_level = scale_and_clip_coeff(s ,tr_ctx, quant_ctx, trans_coeff_level, x_c, y_c);
                }
                //fprintf(stderr,"store coeff xc %d, yc =%d, x_cg: %d, y_cg: %d val: %d\n", x_c, y_c,x_cg,y_cg,trans_coeff_level  );
                coeffs[y_c * tr_ctx->transform_size + x_c] = trans_coeff_level;
                lc->cg_coeffs[1][scan_ctx->scan_inv_cg[i]*16 + scan_ctx->scan_inv_coeff[n]] = trans_coeff_level;
            }
        } //(do next CG)
        //if(scan_idx==SCAN_HORIZ){
//            printf("CG %d %d\n",y_cg*(tr_ctx->transform_size >> 2) + x_cg,scan_ctx->scan_inv_cg[i]);
//            for (int k= 0; k < 4; k++){
//                for(int l=0; l<4; l++){
//                    printf("%*d ",5,cg_coeffs[scan_ctx->scan_inv_cg[i]][k*4+l]);
//                }
//                printf("\n");
//            }
//            printf("\n");
        //}
#if OHCONFIG_AMT
       tr_ctx->num_significant_coeffs += n_end ;
#endif
    }// end of coeffs decoding
    //if(scan_idx==SCAN_HORIZ){
//        printf("\n");
//        for (int k= 0; k < tr_ctx->transform_size; k++){
//            for(int l=0; l<tr_ctx->transform_size; l++){
//                printf("%*d ",5,coeffs[k*tr_ctx->transform_size+l]);
//            }
//            printf("\n");
//        }
//        printf(" END coeffs \n\n");
    //}
}

void ff_hevc_hls_coefficients_coding(HEVCContext *av_restrict s,
                                int log2_trafo_size, enum ScanType scan_idx,
                                int c_idx
                                )
{
    const HEVCSPS             *av_restrict sps = s->ps.sps;
    const HEVCPPS             *av_restrict pps = s->ps.pps;
    HEVCLocalContext          *av_restrict lc  = s->HEVClc;
    HEVCTransformContext      *av_restrict tr_ctx   = &lc->transform_ctx;
    HEVCTransformScanContext  *av_restrict scan_ctx = &tr_ctx->scan_ctx;
    HEVCPersistentRiceContext *av_restrict rice_ctx;
    HEVCQuantContext          *av_restrict quant_ctx;
    CGContext                 *av_restrict current_cg = &lc->cg_ctx;

    int tr_skip_or_bypass;
    //could be set into tr_ctx
    int pps_sign_data_hiding_flag;
    int sps_persistent_rice_adaptation_enabled_flag;
    int cu_tr_transquant_bypass_flag = lc->cu.cu_transquant_bypass_flag;
    int sps_explicit_rdpcm_enabled_flag = sps->explicit_rdpcm_enabled_flag;
    int sign_always_hidden;
    int sign_hidden;
    int n_end;
    int greater1_ctx = 1;
    int16_t cg_coeffs[64][16]={{0}};

    int i;
    int num_cg;
    int tr_size_in_cg;

    int16_t *av_restrict coeffs = lc->tu.coeffs[0];

    uint8_t *av_restrict significant_cg_flag = tr_ctx->significant_cg_flag[0]; // significant CG map;


    //Reset transform context
    //memset(tr_ctx, 0, sizeof(HEVCTransformContext));
    tr_ctx->transform_skip_flag     = 0,
    tr_ctx->num_significant_coeffs  = 0;
    tr_ctx->explicit_rdpcm_dir_flag = 0;
    tr_ctx->explicit_rdpcm_flag     = 0;

    //FIXME Those values could be set in a higher context.
    tr_ctx->log2_transform_range = sps->extended_precision_processing_flag ? FFMAX(15, (sps->bit_depth[CHANNEL_TYPE_LUMA] + 6) ) : 15;//15;
    tr_ctx->log2_trafo_size      = log2_trafo_size;
    tr_ctx->log2_tr_size_minus2  = log2_trafo_size - 2;
    tr_ctx->transform_size       = 1 << log2_trafo_size;
    tr_size_in_cg = 1 << tr_ctx->log2_tr_size_minus2;

    memset(significant_cg_flag,0,64*sizeof(uint8_t));
    memset(&lc->cg_coeffs[0],0,tr_size_in_cg*tr_size_in_cg*sizeof(uint16_t)*16);

    if (!cu_tr_transquant_bypass_flag && pps->transform_skip_enabled_flag &&
            tr_ctx->log2_trafo_size <= pps->log2_max_transform_skip_block_size) {
        tr_ctx->transform_skip_flag = hevc_transform_skip_flag_decode(s);
    }

    tr_skip_or_bypass = sps->transform_skip_context_enabled_flag &&
                    (tr_ctx->transform_skip_flag || cu_tr_transquant_bypass_flag);
    //Reset coeffs buffer
    memset(coeffs, 0, tr_ctx->transform_size * tr_ctx->transform_size * sizeof(int16_t));

    // Derive QP for dequant
    //FIXME this could probably be called outside of the scope of residual coding
    if (!cu_tr_transquant_bypass_flag){
        quant_ctx   = &tr_ctx->quant_ctx;
        derive_quant_parameters(s, lc, tr_ctx, quant_ctx);
    }

    if (lc->cu.pred_mode == MODE_INTER && sps_explicit_rdpcm_enabled_flag &&
            (tr_ctx->transform_skip_flag || cu_tr_transquant_bypass_flag)) {
        tr_ctx->explicit_rdpcm_flag = explicit_rdpcm_flag_decode(s);
        if (tr_ctx->explicit_rdpcm_flag) {
            tr_ctx->explicit_rdpcm_dir_flag = explicit_rdpcm_dir_flag_decode(s);
        }
    }

    last_significant_coeff_xy_prefix_decode(s, tr_ctx->log2_trafo_size,
                                            &scan_ctx->last_significant_coeff_x, &scan_ctx->last_significant_coeff_y);
    // decode and derive last significant coeff (tu scanning ctx)
    decode_and_derive_scanning_params(s, lc, scan_ctx, scan_idx);

    //derive scanning parameters
    //derive_scan[scan_idx][tr_ctx->log2_tr_size_minus2](scan_ctx);
    derive_scanning_direction(scan_ctx,tr_ctx->log2_tr_size_minus2,scan_idx);

    scan_ctx->num_coeff++;

    num_cg = (scan_ctx->num_coeff - 1) >> 4;

    pps_sign_data_hiding_flag = pps->sign_data_hiding_flag;
    sps_persistent_rice_adaptation_enabled_flag = sps->persistent_rice_adaptation_enabled_flag;

    if(sps_persistent_rice_adaptation_enabled_flag){
        int sb_type;
        rice_ctx = &lc->rice_ctx;
        if (!tr_ctx->transform_skip_flag && !cu_tr_transquant_bypass_flag)
            sb_type = 2;
        else
            sb_type = 3;
        rice_ctx->current_coeff = &rice_ctx->stat_coeff[sb_type];
    }

    sign_always_hidden = (cu_tr_transquant_bypass_flag ||
                          (lc->cu.pred_mode ==  MODE_INTRA  && sps->implicit_rdpcm_enabled_flag
                           &&  tr_ctx->transform_skip_flag  &&
                           (lc->tu.intra_pred_mode == 10 || lc->tu.intra_pred_mode  ==  26 ))
                           || tr_ctx->explicit_rdpcm_flag);
    if(sign_always_hidden)
        sign_hidden = 0;

    //decode CGs
    for (i = num_cg; i >= 0; i--) {
        int n, m;
        int x_cg, y_cg;
        int x_c, y_c;

        int cg_id = scan_ctx->scan_inv_cg[i];


        current_cg->is_dc_cg = (i == 0);
        current_cg->is_last_cg = (i == num_cg);
        current_cg->is_last_or_dc_cg = (current_cg->is_dc_cg || current_cg->is_last_cg);
        current_cg->implicit_non_zero_coeff = 0;
        current_cg->num_significant_coeff_in_cg = 0;
        current_cg->prev_sig = 0;

        //get current cg position
        x_cg = scan_ctx->scan_x_cg[i];
        y_cg = scan_ctx->scan_y_cg[i];

        //current_cg->x_cg = x_cg;
        //current_cg->y_cg = y_cg;

        // if not last cg and not first cg
        if (!current_cg->is_last_or_dc_cg) {
            int ctx_cg = 0;
            if ((cg_id + 1) % tr_size_in_cg){
                ctx_cg += significant_cg_flag[cg_id + 1];
            }
            if (cg_id < (int)last_cg_line[tr_ctx->log2_tr_size_minus2]){
                ctx_cg += significant_cg_flag[cg_id + tr_size_in_cg];
            }

            significant_cg_flag[cg_id] =
                    significant_coeff_group_flag_decode(s, ctx_cg);
            current_cg->implicit_non_zero_coeff = 1; // default init to 1
        } else {//test for implicit non zero cg
            significant_cg_flag[cg_id] = 1;
        }

        // on first iteration we read based on last coeff
        //FIXME we could avoid this in loop check
        if (current_cg->is_last_cg) {
            int last_scan_pos = scan_ctx->num_coeff - (i << 4) - 1;
            n_end = last_scan_pos - 1;
            current_cg->significant_coeff_flag_idx[0] = last_scan_pos;
            current_cg->num_significant_coeff_in_cg = 1;
        } else {//we read from last coeff in cg
            n_end = 15;
        }

        //derive global cabac ctx id ids for the whole CG according to other CGs significance
        if ((cg_id + 1) % tr_size_in_cg)
            current_cg->prev_sig = !!significant_cg_flag[cg_id + 1];
        if (cg_id < (int)last_cg_line[tr_ctx->log2_tr_size_minus2])
            current_cg->prev_sig += (!!significant_cg_flag[cg_id + tr_size_in_cg] << 1);

        if (significant_cg_flag[cg_id] && n_end >= 0)
            decode_significance_map(s, tr_ctx, current_cg, n_end, tr_skip_or_bypass,scan_idx);

        n_end = current_cg->num_significant_coeff_in_cg;

        //decode coeffs values
        if (n_end) {
            int c_rice_param = 0;
            int first_greater1_coeff_idx = -1;
            uint8_t coeff_abs_level_greater1_flag[8];
            uint16_t coeff_sign_flag;
            int sum_abs = 0;

            // initialize first elem of coeff_bas_level_greater1_flag
            int ctx_set = (i > 0) ? 2 : 0;


            if (!current_cg->is_last_cg && greater1_ctx == 0)
                ctx_set++;

            greater1_ctx = 1;

            //decode coeff_abs_level_greater1_flags
            for (m = 0; m < (n_end > 8 ? 8 : n_end); m++) {//only first 8 coeffs
                int inc = (ctx_set << 2) + greater1_ctx;
                coeff_abs_level_greater1_flag[m] = coeff_abs_level_greater1_flag_decode(s, inc);
                if (coeff_abs_level_greater1_flag[m]) {
                    greater1_ctx = 0;
                    if (first_greater1_coeff_idx == -1)
                        first_greater1_coeff_idx = m;
                } else if (greater1_ctx > 0 && greater1_ctx < 3) {
                    greater1_ctx++;
                }
            }

            //pass nb
            if (first_greater1_coeff_idx != -1) {
                coeff_abs_level_greater1_flag[first_greater1_coeff_idx] += coeff_abs_level_greater2_flag_decode(s, ctx_set);
            }

            if (!sign_always_hidden){
                current_cg->last_nz_pos_in_cg = current_cg->significant_coeff_flag_idx[0];;
                current_cg->first_nz_pos_in_cg = current_cg->significant_coeff_flag_idx[n_end - 1];
                sign_hidden = (current_cg->last_nz_pos_in_cg - current_cg->first_nz_pos_in_cg >= 4);
            }

            if (!(pps_sign_data_hiding_flag && sign_hidden) ) {
                coeff_sign_flag = coeff_sign_flag_decode(s, current_cg->num_significant_coeff_in_cg) << (16 - current_cg->num_significant_coeff_in_cg);
            } else {
                coeff_sign_flag = coeff_sign_flag_decode(s, current_cg->num_significant_coeff_in_cg - 1) << (16 - (current_cg->num_significant_coeff_in_cg - 1));
            }

            if (sps_persistent_rice_adaptation_enabled_flag) {
                c_rice_param = *(rice_ctx->current_coeff) >> 2;
                rice_ctx->rice_init = 0;
            } else {
                c_rice_param = 0;
            }

            //decode scale and store coeffs values
            for (m = 0; m <  (n_end < 8 ? n_end : 8); m++) {
                int64_t trans_coeff_level;
                n = current_cg->significant_coeff_flag_idx[m];

                x_c = (x_cg << 2) + scan_ctx->scan_x_off[n];
                y_c = (y_cg << 2) + scan_ctx->scan_y_off[n];

                trans_coeff_level = 1 + coeff_abs_level_greater1_flag[m];
                if (trans_coeff_level == ((m == first_greater1_coeff_idx) ? 3 : 2)) {
                    int last_coeff_abs_level_remaining;
#if OHCONFIG_ENCRYPTION
                    if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_TRANSF_COEFFS))
                        last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode_enc(s, c_rice_param, trans_coeff_level);
                    else
#endif
                        last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode(s, c_rice_param);


                    trans_coeff_level += last_coeff_abs_level_remaining;

                    if (sps_persistent_rice_adaptation_enabled_flag){
                        if (trans_coeff_level > (3 << c_rice_param))
                            c_rice_param =  c_rice_param + 1;
                        update_rice_statistics(rice_ctx, last_coeff_abs_level_remaining);
                    } else if (trans_coeff_level > (3 << c_rice_param)){
                        c_rice_param = FFMIN(c_rice_param + 1, 4);
                    }
                }

                if (pps_sign_data_hiding_flag && sign_hidden) {
                    sum_abs += trans_coeff_level;
                    if (n == current_cg->first_nz_pos_in_cg && (sum_abs & 1))
                        trans_coeff_level = -trans_coeff_level;
                }

                if (coeff_sign_flag >> 15)
                    trans_coeff_level = -trans_coeff_level;
                coeff_sign_flag <<= 1;

                //scale (inverse quant) clip and store coeff
                if(!cu_tr_transquant_bypass_flag) {
                    //FIXme add scale function (the scale can be done on the whole coeffs table)
                    trans_coeff_level = scale_and_clip_coeff(s ,tr_ctx, quant_ctx, trans_coeff_level, x_c, y_c);
                }
                //fprintf(stderr,"store coeff xc %d, yc =%d, x_cg: %d, y_cg: %d val: %d\n", x_c, y_c,x_cg,y_cg,trans_coeff_level  );
                coeffs[y_c * tr_ctx->transform_size + x_c] = trans_coeff_level;
                lc->cg_coeffs[0][scan_ctx->scan_inv_cg[i]*16 + scan_ctx->scan_inv_coeff[n]] = trans_coeff_level;
            }

            for (m = 8; m < n_end ; m++) {
                int64_t trans_coeff_level;
                int last_coeff_abs_level_remaining;

                n = current_cg->significant_coeff_flag_idx[m];

                x_c = (x_cg << 2) + scan_ctx->scan_x_off[n];
                y_c = (y_cg << 2) + scan_ctx->scan_y_off[n];

#if OHCONFIG_ENCRYPTION
                if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_TRANSF_COEFFS))
                    last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode_enc(s, c_rice_param, 1);
                else
#endif
                    last_coeff_abs_level_remaining = coeff_abs_level_remaining_decode(s, c_rice_param);

                trans_coeff_level = 1 + last_coeff_abs_level_remaining;
                if (sps_persistent_rice_adaptation_enabled_flag){
                    if (trans_coeff_level > (3 << c_rice_param))
                        c_rice_param =  c_rice_param + 1;
                    update_rice_statistics(rice_ctx, last_coeff_abs_level_remaining);
                } else if (trans_coeff_level > (3 << c_rice_param)){
                    c_rice_param = FFMIN(c_rice_param + 1, 4);
                }

                if (pps_sign_data_hiding_flag && sign_hidden) {
                    sum_abs += trans_coeff_level;
                    if (n == current_cg->first_nz_pos_in_cg && (sum_abs & 1))
                        trans_coeff_level = -trans_coeff_level;
                }

                if (coeff_sign_flag >> 15)
                    trans_coeff_level = -trans_coeff_level;
                coeff_sign_flag <<= 1;

                //scale (inverse quant) clip and store coeff
                if(!cu_tr_transquant_bypass_flag) {
                    //FIXme add scale function (the scale can be done on the whole coeffs table before inverse transform)
                    trans_coeff_level = scale_and_clip_coeff(s ,tr_ctx, quant_ctx, trans_coeff_level, x_c, y_c);
                }
                //fprintf(stderr,"store coeff xc %d, yc =%d, x_cg: %d, y_cg: %d val: %d\n", x_c, y_c,x_cg,y_cg,trans_coeff_level  );
                //coeffs[y_c * tr_ctx->transform_size + x_c] = trans_coeff_level;
                lc->cg_coeffs[0][cg_id * 16 + scan_ctx->scan_inv_coeff[n]] = trans_coeff_level;
            }

        } //(do next CG)


#if OHCONFIG_AMT
       tr_ctx->num_significant_coeffs += n_end ;
#endif
    }
//    printf("\n");
//    for (int k= 0; k < tr_ctx->transform_size; k++){
//        for(int l=0; l<tr_ctx->transform_size; l++){
//            printf("%*d ",5,coeffs[k*tr_ctx->transform_size+l]);
//        }
//        printf("\n");
//    }
//    printf(" END coeffs \n\n");

    for (i = num_cg; i >=0 ; i--){

        int cg_id = scan_ctx->scan_inv_cg[i];
        if(significant_cg_flag[cg_id]){
            int x_cg, y_cg;
            int x_c, y_c;
            int m;

            x_cg = cg_id % tr_size_in_cg;
            y_cg = cg_id / tr_size_in_cg;

            x_c = (x_cg << 2) ;
            y_c = (y_cg << 2) ;

            int64_t *pos = &coeffs[x_c + y_c*tr_ctx->transform_size];
            int64_t *cg_data= &lc->cg_coeffs[0][cg_id*16];

            for (m = 0; m < 4; m++){
                pos[m]= cg_data[m];
                pos += tr_size_in_cg-1;
            }

        }
    }

}

void ff_hevc_hls_mvd_coding(HEVCContext *s, int x0, int y0, int log2_cb_size)
{
#if OHCONFIG_ENCRYPTION
    unsigned int mvd_sign_flag_x=0, mvd_sign_flag_y=0;
#endif
    HEVCLocalContext *lc = s->HEVClc;
    int x = abs_mvd_greater0_flag_decode(s);
    int y = abs_mvd_greater0_flag_decode(s);

    if (x)
        x += abs_mvd_greater1_flag_decode(s);
    if (y)
        y += abs_mvd_greater1_flag_decode(s);

    switch (x) {
    case 2: lc->pu.mvd.x = mvd_decode(s);           break;
    case 1: lc->pu.mvd.x = mvd_sign_flag_decode(s); break;
    case 0: lc->pu.mvd.x = 0;                       break;
    }

#if OHCONFIG_ENCRYPTION
    if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_MV_SIGNS)) {
      if(x) {
        mvd_sign_flag_x = lc->pu.mvd.x < 0 ? 1:0;
        mvd_sign_flag_x = mvd_sign_flag_x^(ff_get_key (&s->HEVClc->dbs_g, 1));
      }
    }
#endif
    switch (y) {
    case 2: lc->pu.mvd.y = mvd_decode(s);           break;
    case 1: lc->pu.mvd.y = mvd_sign_flag_decode(s); break;
    case 0: lc->pu.mvd.y = 0;                       break;
    }
#if OHCONFIG_ENCRYPTION
    if(s->tile_table_encry[s->HEVClc->tile_id] && (s->encrypt_params & HEVC_CRYPTO_MV_SIGNS)) {
      if(y) {
        mvd_sign_flag_y = lc->pu.mvd.y < 0 ? 1:0;
        mvd_sign_flag_y = mvd_sign_flag_y^(ff_get_key (&s->HEVClc->dbs_g, 1));
      }
      lc->pu.mvd.x = mvd_sign_flag_x==1 ? -abs(lc->pu.mvd.x):abs(lc->pu.mvd.x);
      lc->pu.mvd.y = mvd_sign_flag_y==1 ? -abs(lc->pu.mvd.y):abs(lc->pu.mvd.y);
    }
#endif
}

