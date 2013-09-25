/*
 * HEVC Parameter Set Decoding
 *
 * Copyright (C) 2012 - 2103 Guillaume Martres
 * Copyright (C) 2012 - 2103 Mickael Raulet
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 * Copyright (C) 2013 Vittorio Giovara
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

#include "golomb.h"
#include "libavutil/imgutils.h"
#include "hevc.h"

static const uint8_t diag_scan4x4_x[16] = {
    0, 0, 1, 0,
    1, 2, 0, 1,
    2, 3, 1, 2,
    3, 2, 3, 3,
};

static const uint8_t diag_scan4x4_y[16] = {
    0, 1, 0, 2,
    1, 0, 3, 2,
    1, 0, 3, 2,
    1, 3, 2, 3,
};
static const uint8_t diag_scan8x8_x[64] = {
    0, 0, 1, 0,
    1, 2, 0, 1,
    2, 3, 0, 1,
    2, 3, 4, 0,
    1, 2, 3, 4,
    5, 0, 1, 2,
    3, 4, 5, 6,
    0, 1, 2, 3,
    4, 5, 6, 7,
    1, 2, 3, 4,
    5, 6, 7, 2,
    3, 4, 5, 6,
    7, 3, 4, 5,
    6, 7, 4, 5,
    6, 7, 5, 6,
    7, 6, 7, 7,
};

static const uint8_t diag_scan8x8_y[64] = {
    0, 1, 0, 2,
    1, 0, 3, 2,
    1, 0, 4, 3,
    2, 1, 0, 5,
    4, 3, 2, 1,
    0, 6, 5, 4,
    3, 2, 1, 0,
    7, 6, 5, 4,
    3, 2, 1, 0,
    7, 6, 5, 4,
    3, 2, 1, 7,
    6, 5, 4, 3,
    2, 7, 6, 5,
    4, 3, 7, 6,
    5, 4, 7, 6,
    5, 7, 6, 7,
};
static const uint8_t DefaultScalingListIntra[] = {
        16, 16, 16, 16, 17, 18, 21, 24,
		16, 16, 16, 16, 17, 19, 22, 25, 
		16, 16, 17, 18, 20, 22, 25, 29,
		16, 16, 18, 21, 24, 27, 31, 36,
		17, 17, 20, 24, 30, 35, 41, 47,
		18, 19, 22, 27, 35, 44, 54, 65,
		21, 22, 25, 31, 41, 54, 70, 88,
		24, 25, 29,36, 47, 65, 88, 115 };
static const uint8_t DefaultScalingListInter[] = {
		16, 16, 16, 16, 17, 18, 20, 24,
		16, 16, 16, 17, 18, 20, 24, 25, 
		16, 16, 17, 18, 20, 24, 25, 28,
		16, 17, 18, 20, 24, 25, 28, 33,
		17, 18, 20, 24, 25, 28, 33, 41,
		18, 20, 24, 25, 28, 33, 41, 54,
		20, 24, 25, 28, 33, 41, 54, 71,
		24, 25, 28, 33, 41, 54, 71, 91};

/**
 * Section 7.3.3.1
 */

int ff_hevc_decode_short_term_rps(HEVCLocalContext *lc, int idx, SPS *sps)
{
    int delta_idx = 1;
    int delta_rps;
    uint8_t use_delta_flag = 0;
    int delta_poc;
    int k0 = 0;
    int k1 = 0;
    int k  = 0;
    uint8_t delta_rps_sign;
    int abs_delta_rps;

    int i;

    GetBitContext *gb = &lc->gb;

    ShortTermRPS *rps = &sps->short_term_rps_list[idx];
    ShortTermRPS *rps_ridx;

    if (idx != 0) {
        rps->inter_ref_pic_set_prediction_flag = get_bits1(gb);
    } else {
        rps->inter_ref_pic_set_prediction_flag = 0;
    }
    if (rps->inter_ref_pic_set_prediction_flag) {
        if (idx == sps->num_short_term_ref_pic_sets) {
            delta_idx = get_ue_golomb_long(gb) + 1;
        }
        rps_ridx = &sps->short_term_rps_list[idx - delta_idx];
        delta_rps_sign = get_bits1(gb);
        abs_delta_rps = get_ue_golomb_long(gb) + 1;
        delta_rps = (1 - (delta_rps_sign<<1)) * abs_delta_rps;
        for (i = 0; i <= rps_ridx->num_delta_pocs; i++) {
            int used_by_curr_pic_flag = get_bits1(gb);
            rps->used[k] = used_by_curr_pic_flag;
            if (!used_by_curr_pic_flag)
                use_delta_flag = get_bits1(gb);
            if (used_by_curr_pic_flag || use_delta_flag) {
                if (i < rps_ridx->num_delta_pocs)
                    delta_poc = delta_rps + rps_ridx->delta_poc[i];
                else
                    delta_poc = delta_rps;
                rps->delta_poc[k] = delta_poc;
                if (delta_poc < 0)
                    k0++;
                else
                    k1++;
                k++;
            }
            rps->ref_idc[i] = used_by_curr_pic_flag + use_delta_flag * 2;
        }
        rps->num_ref_idc = rps_ridx->num_delta_pocs + 1;
        rps->num_delta_pocs    = k;
        rps->num_negative_pics = k0;
        rps->num_positive_pics = k1;
        // sort in increasing order (smallest first)
        if (rps->num_delta_pocs != 0) {
            int used, tmp;
            for (i = 1; i < rps->num_delta_pocs; i++) {
                delta_poc = rps->delta_poc[i];
                used      = rps->used[i];
                for (k = i-1 ; k >= 0;  k--) {
                    tmp = rps->delta_poc[k];
                    if (delta_poc < tmp ) {
                        rps->delta_poc[k+1] = tmp;
                        rps->used[k+1]      = rps->used[k];
                        rps->delta_poc[k]   = delta_poc;
                        rps->used[k]        = used;
                    }
                }
            }
        }
        if ((rps->num_negative_pics >> 1) != 0) {
            int used;
            k = rps->num_negative_pics - 1;
            // flip the negative values to largest first
            for (i = 0; i < rps->num_negative_pics>>1; i++) {
                delta_poc          = rps->delta_poc[i];
                used               = rps->used[i];
                rps->delta_poc[i]  = rps->delta_poc[k];
                rps->used[i]       = rps->used[k];
                rps->delta_poc[k]  = delta_poc;
                rps->used[k]       = used;
                k--;
            }
        }
    } else {
        int prev;
        rps->num_negative_pics = get_ue_golomb_long(gb);
        rps->num_positive_pics = get_ue_golomb_long(gb);
        rps->num_delta_pocs = rps->num_negative_pics + rps->num_positive_pics;
        if (rps->num_negative_pics || rps->num_positive_pics) {
            prev = 0;
            for (i = 0; i < rps->num_negative_pics; i++) {
                delta_poc = get_ue_golomb_long(gb) + 1;
                prev -= delta_poc;
                rps->delta_poc[i] = prev;
                rps->used[i] = get_bits1(gb);
            }
            prev = 0;
            for (i = 0; i < rps->num_positive_pics; i++) {
                delta_poc = get_ue_golomb_long(gb) + 1;
                prev += delta_poc;
                rps->delta_poc[rps->num_negative_pics + i] = prev;
                rps->used[rps->num_negative_pics + i] = get_bits1(gb);
            }
        }
    }
    return 0;
}

static int decode_profile_tier_level(HEVCLocalContext *lc, PTL *ptl, int max_num_sub_layers)
{
    int i, j;
    GetBitContext *gb = &lc->gb;

    ptl->general_profile_space = get_bits(gb, 2);
    ptl->general_tier_flag = get_bits1(gb);
    ptl->general_profile_idc = get_bits(gb, 5);
    for (i = 0; i < 32; i++)
        ptl->general_profile_compatibility_flag[i] = get_bits1(gb);
    skip_bits1(gb);// general_progressive_source_flag
    skip_bits1(gb);// general_interlaced_source_flag
    skip_bits1(gb);// general_non_packed_constraint_flag
    skip_bits1(gb);// general_frame_only_constraint_flag
    if (get_bits(gb, 16) != 0) // XXX_reserved_zero_44bits[0..15]
        return -1;
    if (get_bits(gb, 16) != 0) // XXX_reserved_zero_44bits[16..31]
        return -1;
    if (get_bits(gb, 12) != 0) // XXX_reserved_zero_44bits[32..43]
        return -1;

    ptl->general_level_idc = get_bits(gb, 8);
    for (i = 0; i < max_num_sub_layers - 1; i++) {
        ptl->sub_layer_profile_present_flag[i] = get_bits1(gb);
        ptl->sub_layer_level_present_flag[i] = get_bits1(gb);
    }
    if (max_num_sub_layers - 1 > 0)
        for (i = max_num_sub_layers - 1; i < 8; i++)
            skip_bits(gb, 2); // reserved_zero_2bits[i]
    for (i = 0; i < max_num_sub_layers - 1; i++) {
        if (ptl->sub_layer_profile_present_flag[i]) {
            ptl->sub_layer_profile_space[i] = get_bits(gb, 2);
            ptl->sub_layer_tier_flag[i] = get_bits(gb, 1);
            ptl->sub_layer_profile_idc[i] = get_bits(gb, 5);
            for (j = 0; j < 32; j++)
                ptl->sub_layer_profile_compatibility_flags[i][j] = get_bits1(gb);
            skip_bits1(gb);// sub_layer_progressive_source_flag
            skip_bits1(gb);// sub_layer_interlaced_source_flag
            skip_bits1(gb);// sub_layer_non_packed_constraint_flag
            skip_bits1(gb);// sub_layer_frame_only_constraint_flag

            if (get_bits(gb, 16) != 0) // sub_layer_reserved_zero_44bits[0..15]
                return -1;
            if (get_bits(gb, 16) != 0) // sub_layer_reserved_zero_44bits[16..31]
                return -1;
            if (get_bits(gb, 12) != 0) // sub_layer_reserved_zero_44bits[32..43]
                return -1;
        }
        if (ptl->sub_layer_level_present_flag[i])
            ptl->sub_layer_level_idc[i] = get_bits(gb, 8);
    }
    return 0;
}

static void decode_hrd(HEVCContext *s)
{
    av_log(s->avctx, AV_LOG_ERROR, "HRD parsing not yet implemented\n");
}

int ff_hevc_decode_nal_vps(HEVCContext *s)
{
    int i,j;
    GetBitContext *gb = &s->HEVClc->gb;
    int vps_id = 0;
    VPS *vps;

    av_log(s->avctx, AV_LOG_DEBUG, "Decoding VPS\n");

    vps = av_mallocz(sizeof(*vps));
    if (!vps)
        return AVERROR(ENOMEM);

    vps_id = get_bits(gb, 4);
    if (vps_id >= MAX_VPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "VPS id out of range: %d\n", vps_id);
        goto err;
    }

    if (get_bits(gb, 2) != 3) { // vps_reserved_three_2bits
        av_log(s->avctx, AV_LOG_ERROR, "vps_reserved_three_2bits is not three\n");
        goto err;
    }

    vps->vps_max_layers               = get_bits(gb, 6) + 1;
    vps->vps_max_sub_layers           = get_bits(gb, 3) + 1;
    vps->vps_temporal_id_nesting_flag = get_bits1(gb);

    if (get_bits(gb, 16) != 0xffff) { // vps_reserved_ffff_16bits
        av_log(s->avctx, AV_LOG_ERROR, "vps_reserved_ffff_16bits is not 0xffff\n");
        goto err;
    }

    if (vps->vps_max_sub_layers > MAX_SUB_LAYERS) {
        av_log(s->avctx, AV_LOG_ERROR, "vps_max_sub_layers out of range: %d\n",
               vps->vps_max_sub_layers);
        goto err;
    }

    if (decode_profile_tier_level(s->HEVClc, &vps->ptl, vps->vps_max_sub_layers) < 0) {
        av_log(s->avctx, AV_LOG_ERROR, "error decoding profile tier level");
        goto err;
    }
    vps->vps_sub_layer_ordering_info_present_flag = get_bits1(gb);

    i = vps->vps_sub_layer_ordering_info_present_flag ? 0 : vps->vps_max_sub_layers - 1;
    for (; i < vps->vps_max_sub_layers; i++) {
        vps->vps_max_dec_pic_buffering[i] = get_ue_golomb_long(gb) + 1;
        vps->vps_num_reorder_pics[i]      = get_ue_golomb_long(gb);
        vps->vps_max_latency_increase[i]  = get_ue_golomb_long(gb) - 1;

        if (vps->vps_max_dec_pic_buffering[i] > MAX_DPB_SIZE) {
            av_log(s->avctx, AV_LOG_ERROR, "vps_max_dec_pic_buffering_minus1 out of range: %d\n",
                   vps->vps_max_dec_pic_buffering[i] - 1);
            goto err;
        }
        if (vps->vps_num_reorder_pics[i] > vps->vps_max_dec_pic_buffering[i] - 1) {
            av_log(s->avctx, AV_LOG_ERROR, "vps_max_num_reorder_pics out of range: %d\n",
                   vps->vps_num_reorder_pics[i]);
            goto err;
        }
    }

    vps->vps_max_layer_id   = get_bits(gb, 6);
    vps->vps_num_layer_sets = get_ue_golomb_long(gb) + 1;
    for (i = 1; i < vps->vps_num_layer_sets; i++)
        for (j = 0; j <= vps->vps_max_layer_id; j++)
            skip_bits(gb, 1); // layer_id_included_flag[i][j]

    vps->vps_timing_info_present_flag = get_bits1(gb);
    if (vps->vps_timing_info_present_flag) {
        vps->vps_num_units_in_tick               = get_bits_long(gb, 32);
        vps->vps_time_scale                      = get_bits_long(gb, 32);
        vps->vps_poc_proportional_to_timing_flag = get_bits1(gb);
        if (vps->vps_poc_proportional_to_timing_flag)
            vps->vps_num_ticks_poc_diff_one = get_ue_golomb_long(gb) + 1;
        vps->vps_num_hrd_parameters = get_ue_golomb_long(gb);
        if (vps->vps_num_hrd_parameters != 0) {
            avpriv_report_missing_feature(s->avctx, "support for vps_num_hrd_parameters != 0");
            av_free(vps);
            return AVERROR_PATCHWELCOME;
        }
    }
    get_bits1(gb); /* vps_extension_flag */

    if (s->vps_list[vps_id] != NULL && s->threads_type == FF_THREAD_FRAME ) {
        ff_thread_mutex_lock_dpb(s->avctx);
        if (s->vps_list[vps_id]->threadCnt == 0)
            av_free(s->vps_list[vps_id]);
        ff_thread_mutex_unlock_dpb(s->avctx);
    } else
        av_free(s->vps_list[vps_id]);
    s->vps_list[vps_id] = vps;
    return 0;

err:
    av_free(vps);
    return AVERROR_INVALIDDATA;
}

static void decode_vui(HEVCContext *s, SPS *sps)
{
    VUI *vui = &sps->vui;
    GetBitContext *gb = &s->HEVClc->gb;

    av_log(s->avctx, AV_LOG_DEBUG, "Decoding VUI\n");

    vui->aspect_ratio_info_present_flag = get_bits1(gb);
    if (vui->aspect_ratio_info_present_flag) {
        vui->aspect_ratio_idc = get_bits(gb, 8);
        if (vui->aspect_ratio_idc == 255) { // EXTENDED_SAR
            vui->sar_width  = get_bits(gb, 16);
            vui->sar_height = get_bits(gb, 16);
        }
    }

    vui->overscan_info_present_flag = get_bits1(gb);
    if (vui->overscan_info_present_flag)
        vui->overscan_appropriate_flag = get_bits1(gb);

    vui->video_signal_type_present_flag = get_bits1(gb);
    if (vui->video_signal_type_present_flag) {
        vui->video_format                    = get_bits(gb, 3);
        vui->video_full_range_flag           = get_bits1(gb);
        vui->colour_description_present_flag = get_bits1(gb);
        if (vui->colour_description_present_flag) {
            vui->colour_primaries        = get_bits(gb, 8);
            vui->transfer_characteristic = get_bits(gb, 8);
            vui->matrix_coeffs           = get_bits(gb, 8);
        }
    }

    vui->chroma_loc_info_present_flag = get_bits1(gb);
    if (vui->chroma_loc_info_present_flag) {
        vui->chroma_sample_loc_type_top_field    = get_ue_golomb_long(gb);
        vui->chroma_sample_loc_type_bottom_field = get_ue_golomb_long(gb);
    }

    vui->neutra_chroma_indication_flag = get_bits1(gb);
    vui->field_seq_flag                = get_bits1(gb);
    vui->frame_field_info_present_flag = get_bits1(gb);

    vui->default_display_window_flag = get_bits1(gb);
    if (vui->default_display_window_flag) {
        vui->def_disp_win.left_offset   = get_ue_golomb_long(gb);
        vui->def_disp_win.right_offset  = get_ue_golomb_long(gb);
        vui->def_disp_win.top_offset    = get_ue_golomb_long(gb);
        vui->def_disp_win.bottom_offset = get_ue_golomb_long(gb);

        if (s->avctx->flags2 & CODEC_FLAG2_IGNORE_CROP) {
            av_log(s->avctx, AV_LOG_DEBUG,
                   "discarding vui default display window, "
                   "original values are l:%u r:%u t:%u b:%u\n",
                   vui->def_disp_win.left_offset,
                   vui->def_disp_win.right_offset,
                   vui->def_disp_win.top_offset,
                   vui->def_disp_win.bottom_offset);

            vui->def_disp_win.left_offset   =
            vui->def_disp_win.right_offset  =
            vui->def_disp_win.top_offset    =
            vui->def_disp_win.bottom_offset = 0;
        }

        if (s->strict_def_disp_win &&
            vui->def_disp_win.left_offset & (0x1F >> (sps->bit_depth > 8)) &&
            !(s->avctx->flags & CODEC_FLAG_UNALIGNED)) {
            vui->def_disp_win.left_offset &= ~(0x1F >> (sps->bit_depth > 8));
            av_log(s->avctx, AV_LOG_WARNING, "Reducing left default display window "
                   "to %d chroma samples to preserve alignment.\n",
                   vui->def_disp_win.left_offset);
        }
    }

    vui->vui_timing_info_present_flag = get_bits1(gb);
    if (vui->vui_timing_info_present_flag) {
        vui->vui_num_units_in_tick               = get_bits(gb, 32);
        vui->vui_time_scale                      = get_bits(gb, 32);
        vui->vui_poc_proportional_to_timing_flag = get_bits1(gb);
        if (vui->vui_poc_proportional_to_timing_flag)
            vui->vui_num_ticks_poc_diff_one_minus1 = get_ue_golomb_long(gb);
        vui->vui_hrd_parameters_present_flag = get_bits1(gb);
        if (vui->vui_hrd_parameters_present_flag)
            decode_hrd(s);
    }

    vui->bitstream_restriction_flag = get_bits1(gb);
    if (vui->bitstream_restriction_flag) {
        vui->tiles_fixed_structure_flag              = get_bits1(gb);
        vui->motion_vectors_over_pic_boundaries_flag = get_bits1(gb);
        vui->restricted_ref_pic_lists_flag           = get_bits1(gb);
        vui->min_spatial_segmentation_idc            = get_ue_golomb_long(gb);
        vui->max_bytes_per_pic_denom                 = get_ue_golomb_long(gb);
        vui->max_bits_per_min_cu_denom               = get_ue_golomb_long(gb);
        vui->log2_max_mv_length_horizontal           = get_ue_golomb_long(gb);
        vui->log2_max_mv_length_vertical             = get_ue_golomb_long(gb);
    }
}
static void set_default_scaling_list_data(ScalingListData *sld) {
    int matrixId;
    for(matrixId = 0; matrixId < 6; matrixId++) {
        // 4x4 default is 16   
        memset(sld->ScalingList[0][matrixId],16,16);
        sld->ScalingListDC[0][matrixId] = 16; // default for 16x16
        sld->ScalingListDC[1][matrixId] = 16; // default for 32x32
    }
    memcpy(sld->ScalingList[1][0],DefaultScalingListIntra,64);
    memcpy(sld->ScalingList[1][1],DefaultScalingListIntra,64);
    memcpy(sld->ScalingList[1][2],DefaultScalingListIntra,64);
    memcpy(sld->ScalingList[1][3],DefaultScalingListInter,64);
    memcpy(sld->ScalingList[1][4],DefaultScalingListInter,64);
    memcpy(sld->ScalingList[1][5],DefaultScalingListInter,64);
    memcpy(sld->ScalingList[2][0],DefaultScalingListIntra,64);
    memcpy(sld->ScalingList[2][1],DefaultScalingListIntra,64);
    memcpy(sld->ScalingList[2][2],DefaultScalingListIntra,64);
    memcpy(sld->ScalingList[2][3],DefaultScalingListInter,64);
    memcpy(sld->ScalingList[2][4],DefaultScalingListInter,64);
    memcpy(sld->ScalingList[2][5],DefaultScalingListInter,64);
    memcpy(sld->ScalingList[3][0],DefaultScalingListIntra,64);
    memcpy(sld->ScalingList[3][1],DefaultScalingListInter,64);
}

static void scaling_list_data(HEVCContext *s, ScalingListData *sld) {
    GetBitContext *gb = &s->HEVClc->gb;
    uint8_t scaling_list_pred_mode_flag[4][6];
    int32_t scaling_list_dc_coef_minus8[2][6];
    
    int size_id, matrix_id, i, pos, delta;
	for (size_id = 0; size_id < 4; size_id++)
		for (matrix_id = 0; matrix_id < ((size_id == 3) ? 2 : 6); matrix_id++) {
			scaling_list_pred_mode_flag[size_id][matrix_id] = get_bits1(gb);
			if (!scaling_list_pred_mode_flag[size_id][matrix_id]) {
				delta = get_ue_golomb_long(gb);
                // Only need to handle non-zero delta. Zero means default, which should already be in the arrays.
                if(delta != 0) {
                    // Copy from previous array.
                    memcpy(sld->ScalingList[ size_id ][ matrix_id ], sld->ScalingList[ size_id ][ matrix_id - delta], size_id > 0 ? 64 : 16);
                    if(size_id > 1)
                        sld->ScalingListDC[size_id-2][matrix_id] = sld->ScalingListDC[size_id-2][matrix_id-delta];
                }
            } else {
                int next_coef;
                int coef_num;
                int32_t scaling_list_delta_coef;
                next_coef = 8;
                coef_num = FFMIN(64, (1  <<  (4 + (size_id  <<  1))));
                if( size_id > 1 ) {
                    scaling_list_dc_coef_minus8[size_id - 2][matrix_id] = get_se_golomb(gb);
                    next_coef = scaling_list_dc_coef_minus8[size_id - 2][matrix_id] + 8;
                    sld->ScalingListDC[ size_id - 2 ][ matrix_id ] = next_coef;
                }
                for( i = 0; i < coef_num; i++) {
                    if(size_id == 0) {
                        pos = 4*diag_scan4x4_y[ i ] + diag_scan4x4_x[ i ];
                    } else {
                        pos = 8*diag_scan8x8_y[ i ] + diag_scan8x8_x[ i ];
                    }
                    scaling_list_delta_coef = get_se_golomb(gb);
                    next_coef = (next_coef + scaling_list_delta_coef + 256 ) % 256;
                    sld->ScalingList[ size_id ][ matrix_id ][ pos ] = next_coef;
                }
            }
		}
}

int ff_hevc_decode_nal_sps(HEVCContext *s)
{
    GetBitContext *gb = &s->HEVClc->gb;
    int ret    = 0;
    int sps_id = 0;
    int log2_diff_max_min_transform_block_size;
    int bit_depth_chroma, start;
    int i;

    SPS *sps = av_mallocz(sizeof(*sps));
    if (!sps)
        return AVERROR(ENOMEM);

    av_log(s->avctx, AV_LOG_DEBUG, "Decoding SPS\n");

    memset(sps->short_term_rps_list, 0, sizeof(sps->short_term_rps_list));

    // Coded parameters

    sps->vps_id = get_bits(gb, 4);
    if (sps->vps_id >= MAX_VPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "VPS id out of range: %d\n", sps->vps_id);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }

    sps->sps_max_sub_layers = get_bits(gb, 3) + 1;
    if (sps->sps_max_sub_layers > MAX_SUB_LAYERS) {
        av_log(s->avctx, AV_LOG_ERROR, "vps_max_sub_layers out of range: %d\n",
               sps->sps_max_sub_layers);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }

    sps->temporal_id_nesting_flag = get_bits1(gb);
    if (decode_profile_tier_level(s->HEVClc, &sps->ptl, sps->sps_max_sub_layers) < 0) {
        av_log(s->avctx, AV_LOG_ERROR, "error decoding profile tier level\n");
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    sps_id = get_ue_golomb_long(gb);
    if (sps_id >= MAX_SPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "SPS id out of range: %d\n", sps_id);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }

    sps->chroma_format_idc = get_ue_golomb_long(gb);
    if (sps->chroma_format_idc != 1) {
        avpriv_report_missing_feature(s->avctx, "chroma_format_idc != 1\n");
        ret = AVERROR_INVALIDDATA;
        goto err;
    }

    if (sps->chroma_format_idc == 3)
        sps->separate_colour_plane_flag = get_bits1(gb);

    sps->pic_width_in_luma_samples  = get_ue_golomb_long(gb);
    sps->pic_height_in_luma_samples = get_ue_golomb_long(gb);
    if ((ret = av_image_check_size(sps->pic_width_in_luma_samples,
                                   sps->pic_height_in_luma_samples, 0, s->avctx)) < 0)
        goto err;

    sps->pic_conformance_flag = get_bits1(gb);
    if (sps->pic_conformance_flag) {
        //TODO: * 2 is only valid for 420
        sps->pic_conf_win.left_offset   = get_ue_golomb_long(gb) * 2;
        sps->pic_conf_win.right_offset  = get_ue_golomb_long(gb) * 2;
        sps->pic_conf_win.top_offset    = get_ue_golomb_long(gb) * 2;
        sps->pic_conf_win.bottom_offset = get_ue_golomb_long(gb) * 2;

        if (s->avctx->flags2 & CODEC_FLAG2_IGNORE_CROP) {
            av_log(s->avctx, AV_LOG_DEBUG,
                   "discarding sps conformance window, "
                   "original values are l:%u r:%u t:%u b:%u\n",
                   sps->pic_conf_win.left_offset,
                   sps->pic_conf_win.right_offset,
                   sps->pic_conf_win.top_offset,
                   sps->pic_conf_win.bottom_offset);

            sps->pic_conf_win.left_offset   =
            sps->pic_conf_win.right_offset  =
            sps->pic_conf_win.top_offset    =
            sps->pic_conf_win.bottom_offset = 0;
        }
    }
    if (s->avctx->flags2 & CODEC_FLAG2_IGNORE_CROP) {
        sps->pic_conf_win.left_offset   =
        sps->pic_conf_win.right_offset  =
        sps->pic_conf_win.top_offset    =
        sps->pic_conf_win.bottom_offset = 0;
    }

    sps->bit_depth   = get_ue_golomb_long(gb) + 8;
    bit_depth_chroma = get_ue_golomb_long(gb) + 8;
    if (bit_depth_chroma != sps->bit_depth) {
        av_log(s->avctx, AV_LOG_ERROR,
               "Luma bit depth (%d) is different from chroma bit depth (%d), this is unsupported.\n",
               sps->bit_depth, bit_depth_chroma);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    if (sps->bit_depth > 10) {
        av_log(s->avctx, AV_LOG_ERROR, "Unsupported bit depth: %d\n",
               sps->bit_depth);
        ret = AVERROR_PATCHWELCOME;
        goto err;
    }

    if (sps->pic_conformance_flag &&
        sps->pic_conf_win.left_offset & (0x1F >> (sps->bit_depth > 8)) &&
        !(s->avctx->flags & CODEC_FLAG_UNALIGNED)) {
        sps->pic_conf_win.left_offset &= ~(0x1F >> (sps->bit_depth > 8));
        av_log(s->avctx, AV_LOG_WARNING, "Reducing left conformance window to %d "
               "chroma samples to preserve alignment.\n",
               sps->pic_conf_win.left_offset);
    }

    sps->log2_max_poc_lsb = get_ue_golomb_long(gb) + 4;
    if (sps->log2_max_poc_lsb > 16) {
        av_log(s->avctx, AV_LOG_ERROR, "log2_max_pic_order_cnt_lsb_minus4 out range: %d\n",
               sps->log2_max_poc_lsb - 4);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    sps->sps_sub_layer_ordering_info_present_flag = get_bits1(gb);

    start = (sps->sps_sub_layer_ordering_info_present_flag ? 0 : (sps->sps_max_sub_layers - 1));
    for (i = start; i < sps->sps_max_sub_layers; i++) {
        sps->temporal_layer[i].max_dec_pic_buffering = get_ue_golomb_long(gb) + 1;
        sps->temporal_layer[i].num_reorder_pics      = get_ue_golomb_long(gb);
        sps->temporal_layer[i].max_latency_increase  = get_ue_golomb_long(gb) - 1;
        if (sps->temporal_layer[i].max_dec_pic_buffering > MAX_DPB_SIZE) {
            av_log(s->avctx, AV_LOG_ERROR, "sps_max_dec_pic_buffering_minus1 out of range: %d\n",
                   sps->temporal_layer[i].max_dec_pic_buffering - 1);
            ret = AVERROR_INVALIDDATA;
            goto err;
        }
        if (sps->temporal_layer[i].num_reorder_pics > sps->temporal_layer[i].max_dec_pic_buffering - 1) {
            av_log(s->avctx, AV_LOG_ERROR, "sps_max_num_reorder_pics out of range: %d\n",
                   sps->temporal_layer[i].num_reorder_pics);
            ret = AVERROR_INVALIDDATA;
            goto err;
        }
    }

    if (!sps->sps_sub_layer_ordering_info_present_flag) {
        for (i = 0; i < start; i++){
            sps->temporal_layer[i].max_dec_pic_buffering = sps->temporal_layer[start].max_dec_pic_buffering;
            sps->temporal_layer[i].num_reorder_pics      = sps->temporal_layer[start].num_reorder_pics;
            sps->temporal_layer[i].max_latency_increase  = sps->temporal_layer[start].max_latency_increase;
        }
    }

    sps->log2_min_coding_block_size             = get_ue_golomb_long(gb) + 3;
    sps->log2_diff_max_min_coding_block_size    = get_ue_golomb_long(gb);
    sps->log2_min_transform_block_size          = get_ue_golomb_long(gb) + 2;
    log2_diff_max_min_transform_block_size      = get_ue_golomb_long(gb);
    sps->log2_max_trafo_size                    = log2_diff_max_min_transform_block_size + sps->log2_min_transform_block_size;

    if (sps->log2_min_transform_block_size >= sps->log2_min_coding_block_size) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid value for log2_min_transform_block_size");
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    sps->max_transform_hierarchy_depth_inter = get_ue_golomb_long(gb);
    sps->max_transform_hierarchy_depth_intra = get_ue_golomb_long(gb);

    sps->scaling_list_enable_flag = get_bits1(gb);
    if (sps->scaling_list_enable_flag) {
        sps->scaling_list_data_present_flag = get_bits1(gb);
        set_default_scaling_list_data(&sps->scalingList);
        if (sps->scaling_list_data_present_flag)
            scaling_list_data(s, &sps->scalingList);
    }

    sps->amp_enabled_flag                    = get_bits1(gb);
    sps->sample_adaptive_offset_enabled_flag = get_bits1(gb);

    sps->pcm_enabled_flag = get_bits1(gb);
    if (sps->pcm_enabled_flag) {
        sps->pcm.bit_depth   = get_bits(gb, 4) + 1;
        sps->pcm.bit_depth_chroma = get_bits(gb, 4) + 1;

        sps->pcm.log2_min_pcm_cb_size = get_ue_golomb_long(gb) + 3;
        sps->pcm.log2_max_pcm_cb_size = sps->pcm.log2_min_pcm_cb_size +
                                        get_ue_golomb_long(gb);
        if (sps->pcm.bit_depth > sps->bit_depth) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "PCM bit depth (%d) is greater than normal bit depth (%d)\n",
                   sps->pcm.bit_depth, sps->bit_depth);
            ret = AVERROR_INVALIDDATA;
            goto err;
        }

        sps->pcm.loop_filter_disable_flag = get_bits1(gb);
    }

    sps->num_short_term_ref_pic_sets = get_ue_golomb_long(gb);
    for (i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
        if ((ret = ff_hevc_decode_short_term_rps(s->HEVClc, i, sps)) < 0)
            goto err;
    }

    sps->long_term_ref_pics_present_flag = get_bits1(gb);
    if (sps->long_term_ref_pics_present_flag) {
        sps->num_long_term_ref_pics_sps = get_ue_golomb_long(gb);
        for (i = 0; i < sps->num_long_term_ref_pics_sps; i++) {
            sps->lt_ref_pic_poc_lsb_sps[i]       = get_bits(gb, sps->log2_max_poc_lsb);
            sps->used_by_curr_pic_lt_sps_flag[i] = get_bits1(gb);
        }
    }

    sps->sps_temporal_mvp_enabled_flag          = get_bits1(gb);
    sps->sps_strong_intra_smoothing_enable_flag = get_bits1(gb);
    sps->vui_parameters_present_flag            = get_bits1(gb);
    if (sps->vui_parameters_present_flag)
        decode_vui(s, sps);
    sps->sps_extension_flag = get_bits1(gb);

    // Inferred parameters
    sps->log2_ctb_size = sps->log2_min_coding_block_size
                         + sps->log2_diff_max_min_coding_block_size;
    sps->pic_width_in_ctbs  = (sps->pic_width_in_luma_samples  + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
    sps->pic_height_in_ctbs = (sps->pic_height_in_luma_samples + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
    sps->pic_width_in_min_cbs  = sps->pic_width_in_luma_samples  >> sps->log2_min_coding_block_size;
    sps->pic_height_in_min_cbs = sps->pic_height_in_luma_samples >> sps->log2_min_coding_block_size;
    sps->pic_width_in_min_tbs  = sps->pic_width_in_luma_samples  >> sps->log2_min_transform_block_size;
    sps->pic_height_in_min_tbs = sps->pic_height_in_luma_samples >> sps->log2_min_transform_block_size;
    sps->log2_min_pu_size      = sps->log2_min_coding_block_size - 1;
    sps->pic_width_in_min_pus  = sps->pic_width_in_luma_samples  >> sps->log2_min_pu_size;
    sps->pic_height_in_min_pus = sps->pic_height_in_luma_samples >> sps->log2_min_pu_size;
    sps->log2_diff_ctb_min_tb_size = sps->log2_ctb_size - sps->log2_min_transform_block_size;

    sps->qp_bd_offset = 6 * (sps->bit_depth - 8);
    if ((1 << sps->log2_ctb_size) > MAX_CTB_SIZE) {
        av_log(s->avctx, AV_LOG_ERROR, "CTB size out of range: %d\n", 1 << sps->log2_ctb_size);
        goto err;
    }
    if (sps->max_transform_hierarchy_depth_inter > sps->log2_diff_ctb_min_tb_size) {
        av_log(s->avctx, AV_LOG_ERROR, "max_transform_hierarchy_depth_inter out of range: %d\n",
               sps->max_transform_hierarchy_depth_inter);
        goto err;
    }
    if (sps->max_transform_hierarchy_depth_intra > sps->log2_diff_ctb_min_tb_size) {
        av_log(s->avctx, AV_LOG_ERROR, "max_transform_hierarchy_depth_intra out of range: %d\n",
               sps->max_transform_hierarchy_depth_intra);
        goto err;
    }
    if (s->sps_list[sps_id] != NULL && s->threads_type == FF_THREAD_FRAME ) {
        ff_thread_mutex_lock_dpb(s->avctx);
        if (s->sps_list[sps_id]->threadCnt == 0)
            av_free(s->sps_list[sps_id]);
        ff_thread_mutex_unlock_dpb(s->avctx);
    } else
    	av_free(s->sps_list[sps_id]);
    s->sps_list[sps_id] = sps;
    return 0;
err:

    av_free(sps);
    return ret;
}

void ff_hevc_pps_free(PPS **ppps)
{
    PPS *pps = *ppps;

    if (!pps)
        return;

    av_freep(&pps->column_width);
    av_freep(&pps->row_height);
    av_freep(&pps->col_bd);
    av_freep(&pps->row_bd);
    av_freep(&pps->col_idxX);
    av_freep(&pps->ctb_addr_rs_to_ts);
    av_freep(&pps->ctb_addr_ts_to_rs);
    av_freep(&pps->tile_pos_rs);
    av_freep(&pps->tile_id);
    av_freep(&pps->min_cb_addr_zs);
    av_freep(&pps->min_tb_addr_zs);

    av_freep(ppps);
}

int ff_hevc_decode_nal_pps(HEVCContext *s)
{
    GetBitContext *gb = &s->HEVClc->gb;
    SPS          *sps = NULL;
    int pic_area_in_ctbs, pic_area_in_min_cbs, pic_area_in_min_tbs;
    int i, j, x, y, ctb_addr_rs, tile_id;
    int ret    = 0;
    int pps_id = 0;

    PPS *pps = av_mallocz(sizeof(*pps));
    if (!pps)
        return AVERROR(ENOMEM);

    av_log(s->avctx, AV_LOG_DEBUG, "Decoding PPS\n");

    // Default values
    pps->loop_filter_across_tiles_enabled_flag = 1;
    pps->num_tile_columns                      = 1;
    pps->num_tile_rows                         = 1;
    pps->uniform_spacing_flag                  = 1;
    pps->pps_disable_deblocking_filter_flag    = 0;
    pps->beta_offset                           = 0;
    pps->tc_offset                             = 0;

    // Coded parameters
    pps_id = get_ue_golomb_long(gb);
    if (pps_id >= MAX_PPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS id out of range: %d\n", pps_id);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    pps->sps_id = get_ue_golomb_long(gb);
    if (pps->sps_id >= MAX_SPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "SPS id out of range: %d\n", pps->sps_id);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    sps = s->sps_list[pps->sps_id];
    if (!sps) {
        av_log(s->avctx, AV_LOG_ERROR, "SPS does not exist \n");
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    pps->dependent_slice_segments_enabled_flag = get_bits1(gb);
    pps->output_flag_present_flag              = get_bits1(gb);
    pps->num_extra_slice_header_bits           = get_bits(gb, 3);

    pps->sign_data_hiding_flag = get_bits1(gb);

    pps->cabac_init_present_flag = get_bits1(gb);

    pps->num_ref_idx_l0_default_active = get_ue_golomb_long(gb) + 1;
    pps->num_ref_idx_l1_default_active = get_ue_golomb_long(gb) + 1;

    pps->pic_init_qp_minus26 = get_se_golomb(gb);

    pps->constrained_intra_pred_flag = get_bits1(gb);
    pps->transform_skip_enabled_flag = get_bits1(gb);

    pps->cu_qp_delta_enabled_flag = get_bits1(gb);
    pps->diff_cu_qp_delta_depth   = 0;
    if (pps->cu_qp_delta_enabled_flag)
        pps->diff_cu_qp_delta_depth = get_ue_golomb_long(gb);

    pps->cb_qp_offset = get_se_golomb(gb);
    if (pps->cb_qp_offset < -12 || pps->cb_qp_offset > 12) {
        av_log(s->avctx, AV_LOG_ERROR, "pps_cb_qp_offset out of range: %d\n",
               pps->cb_qp_offset);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    pps->cr_qp_offset = get_se_golomb(gb);
    if (pps->cr_qp_offset < -12 || pps->cr_qp_offset > 12) {
        av_log(s->avctx, AV_LOG_ERROR, "pps_cr_qp_offset out of range: %d\n",
               pps->cr_qp_offset);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    pps->pic_slice_level_chroma_qp_offsets_present_flag = get_bits1(gb);

    pps->weighted_pred_flag   = get_bits1(gb);
    pps->weighted_bipred_flag = get_bits1(gb);

    pps->transquant_bypass_enable_flag    = get_bits1(gb);
    pps->tiles_enabled_flag               = get_bits1(gb);
    pps->entropy_coding_sync_enabled_flag = get_bits1(gb);

    if (pps->tiles_enabled_flag) {
        if (s->threads_type == FF_THREAD_FRAME) {
            av_log(s->avctx, AV_LOG_ERROR, "Frame base and tiles enabled not yet implemented\n");
            ret = AVERROR_INVALIDDATA;
            goto err;
        }
        pps->num_tile_columns     = get_ue_golomb_long(gb) + 1;
        pps->num_tile_rows        = get_ue_golomb_long(gb) + 1;
        if (pps->num_tile_columns == 0 ||
            pps->num_tile_columns >= sps->pic_width_in_luma_samples) {
            av_log(s->avctx, AV_LOG_ERROR, "num_tile_columns_minus1 out of range: %d\n",
                   pps->num_tile_columns - 1);
            ret = AVERROR_INVALIDDATA;
            goto err;
        }
        if (pps->num_tile_rows == 0 ||
            pps->num_tile_rows >= sps->pic_height_in_luma_samples) {
            av_log(s->avctx, AV_LOG_ERROR, "num_tile_rows_minus1 out of range: %d\n",
                   pps->num_tile_rows - 1);
            ret = AVERROR_INVALIDDATA;
            goto err;
        }

        pps->column_width = av_malloc_array(pps->num_tile_columns, sizeof(*pps->column_width));
        pps->row_height   = av_malloc_array(pps->num_tile_rows,    sizeof(*pps->row_height));
        if (!pps->column_width || !pps->row_height) {
            ret = AVERROR(ENOMEM);
            goto err;
        }

        pps->uniform_spacing_flag = get_bits1(gb);
        if (!pps->uniform_spacing_flag) {
            int sum = 0;
            for (i = 0; i < pps->num_tile_columns - 1; i++) {
                pps->column_width[i] = get_ue_golomb_long(gb) + 1;
                sum += pps->column_width[i];
            }
            if (sum >= sps->pic_width_in_ctbs) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid tile widths.\n");
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
            pps->column_width[pps->num_tile_columns - 1] = sps->pic_width_in_ctbs - sum;

            sum = 0;
            for (i = 0; i < pps->num_tile_rows - 1; i++) {
                pps->row_height[i] = get_ue_golomb_long(gb) + 1;
                sum += pps->row_height[i];
            }
            if (sum >= sps->pic_height_in_ctbs) {
                av_log(s->avctx, AV_LOG_ERROR, "Invalid tile heights.\n");
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
            pps->row_height[pps->num_tile_rows - 1] = sps->pic_height_in_ctbs - sum;
        }
        pps->loop_filter_across_tiles_enabled_flag = get_bits1(gb);
    }

    pps->seq_loop_filter_across_slices_enabled_flag = get_bits1(gb);

    pps->deblocking_filter_control_present_flag = get_bits1(gb);
    if (pps->deblocking_filter_control_present_flag) {
        pps->deblocking_filter_override_enabled_flag = get_bits1(gb);
        pps->pps_disable_deblocking_filter_flag = get_bits1(gb);
        if (!pps->pps_disable_deblocking_filter_flag) {
            pps->beta_offset = get_se_golomb(gb) * 2;
            pps->tc_offset = get_se_golomb(gb) * 2;
            if (pps->beta_offset/2 < -6 || pps->beta_offset/2 > 6) {
                av_log(s->avctx, AV_LOG_ERROR, "pps_beta_offset_div2 out of range: %d\n",
                       pps->beta_offset/2);
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
            if (pps->tc_offset/2 < -6 || pps->tc_offset/2 > 6) {
                av_log(s->avctx, AV_LOG_ERROR, "pps_tc_offset_div2 out of range: %d\n",
                       pps->tc_offset/2);
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
        }
    }

    pps->pps_scaling_list_data_present_flag = get_bits1(gb);
    if (pps->pps_scaling_list_data_present_flag) {
        set_default_scaling_list_data(&pps->scalingList);
        scaling_list_data(s,&pps->scalingList);
    }
    pps->lists_modification_present_flag = get_bits1(gb);
    pps->log2_parallel_merge_level       = get_ue_golomb_long(gb) + 2;
    if (pps->log2_parallel_merge_level > sps->log2_ctb_size) {
        av_log(s->avctx, AV_LOG_ERROR, "log2_parallel_merge_level_minus2 out of range: %d\n",
               pps->log2_parallel_merge_level - 2);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }

    pps->slice_header_extension_present_flag = get_bits1(gb);
    pps->pps_extension_flag                  = get_bits1(gb);

    // Inferred parameters
    pps->col_bd   = av_malloc_array(pps->num_tile_columns + 1, sizeof(*pps->col_bd));
    pps->row_bd   = av_malloc_array(pps->num_tile_rows + 1,    sizeof(*pps->row_bd));
    pps->col_idxX = av_malloc_array(sps->pic_width_in_ctbs,    sizeof(*pps->col_idxX));
    if (!pps->col_bd || !pps->row_bd || !pps->col_idxX) {
        ret = AVERROR(ENOMEM);
        goto err;
    }

    if (pps->uniform_spacing_flag) {
        pps->column_width = av_malloc_array(pps->num_tile_columns, sizeof(*pps->column_width));
        pps->row_height   = av_malloc_array(pps->num_tile_rows,    sizeof(*pps->row_height));
        if (!pps->column_width || !pps->row_height) {
            ret = AVERROR(ENOMEM);
            goto err;
        }

        for (i = 0; i < pps->num_tile_columns; i++) {
            pps->column_width[i] = ((i + 1) * sps->pic_width_in_ctbs) / pps->num_tile_columns -
                                   (i * sps->pic_width_in_ctbs) / pps->num_tile_columns;
        }

        for (i = 0; i < pps->num_tile_rows; i++) {
            pps->row_height[i] = ((i + 1) * sps->pic_height_in_ctbs) / pps->num_tile_rows -
                                 (i * sps->pic_height_in_ctbs) / pps->num_tile_rows;
        }
    }

    pps->col_bd[0] = 0;
    for (i = 0; i < pps->num_tile_columns; i++)
        pps->col_bd[i + 1] = pps->col_bd[i] + pps->column_width[i];

    pps->row_bd[0] = 0;
    for (i = 0; i < pps->num_tile_rows; i++)
        pps->row_bd[i + 1] = pps->row_bd[i] + pps->row_height[i];

    for (i = 0, j = 0; i < sps->pic_width_in_ctbs; i++) {
         if (i > pps->col_bd[j])
             j++;
         pps->col_idxX[i] = j;
    }

    /**
     * 6.5
     */
    pic_area_in_ctbs     = sps->pic_width_in_ctbs    * sps->pic_height_in_ctbs;
    pic_area_in_min_cbs  = sps->pic_width_in_min_cbs * sps->pic_height_in_min_cbs;
    pic_area_in_min_tbs  = sps->pic_width_in_min_tbs * sps->pic_height_in_min_tbs;

    pps->ctb_addr_rs_to_ts = av_malloc_array(pic_area_in_ctbs,    sizeof(*pps->ctb_addr_rs_to_ts));
    pps->ctb_addr_ts_to_rs = av_malloc_array(pic_area_in_ctbs,    sizeof(*pps->ctb_addr_ts_to_rs));
    pps->tile_id           = av_malloc_array(pic_area_in_ctbs,    sizeof(*pps->tile_id));
    pps->min_cb_addr_zs    = av_malloc_array(pic_area_in_min_cbs, sizeof(*pps->min_cb_addr_zs));
    pps->min_tb_addr_zs    = av_malloc_array(pic_area_in_min_tbs, sizeof(*pps->min_tb_addr_zs));
    if (!pps->ctb_addr_rs_to_ts || !pps->ctb_addr_ts_to_rs ||
        !pps->tile_id || !pps->min_cb_addr_zs || !pps->min_tb_addr_zs) {
        ret = AVERROR(ENOMEM);
        goto err;
    }

    for (ctb_addr_rs = 0; ctb_addr_rs < pic_area_in_ctbs; ctb_addr_rs++) {
        int tb_x   = ctb_addr_rs % sps->pic_width_in_ctbs;
        int tb_y   = ctb_addr_rs / sps->pic_width_in_ctbs;
        int tile_x = 0;
        int tile_y = 0;
        int val    = 0;

        for (i = 0; i < pps->num_tile_columns; i++) {
            if (tb_x < pps->col_bd[i + 1]) {
                tile_x = i;
                break;
            }
        }

        for (i = 0; i < pps->num_tile_rows; i++) {
            if (tb_y < pps->row_bd[i + 1]) {
                tile_y = i;
                break;
            }
        }

        for (i = 0; i < tile_x; i++ )
            val += pps->row_height[tile_y] * pps->column_width[i];
        for (i = 0; i < tile_y; i++ )
            val += sps->pic_width_in_ctbs * pps->row_height[i];

        val += (tb_y - pps->row_bd[tile_y]) * pps->column_width[tile_x] +
               tb_x - pps->col_bd[tile_x];

        pps->ctb_addr_rs_to_ts[ctb_addr_rs] = val;
        pps->ctb_addr_ts_to_rs[val] = ctb_addr_rs;
    }

    for (j = 0, tile_id = 0; j < pps->num_tile_rows; j++)
        for (i = 0; i < pps->num_tile_columns; i++, tile_id++)
            for (y = pps->row_bd[j]; y < pps->row_bd[j + 1]; y++)
                for (x = pps->col_bd[i]; x < pps->col_bd[i + 1]; x++)
                    pps->tile_id[pps->ctb_addr_rs_to_ts[y * sps->pic_width_in_ctbs + x]] = tile_id;

    pps->tile_pos_rs = av_malloc_array(tile_id, sizeof(*pps->tile_pos_rs));
    if (!pps->tile_pos_rs) {
        ret = AVERROR(ENOMEM);
        goto err;
    }

    for (j = 0; j < pps->num_tile_rows; j++)
        for (i = 0; i < pps->num_tile_columns; i++)
            pps->tile_pos_rs[j * pps->num_tile_columns + i] = pps->row_bd[j] * sps->pic_width_in_ctbs + pps->col_bd[i];

    for (y = 0; y < sps->pic_height_in_min_cbs; y++) {
        for (x = 0; x < sps->pic_width_in_min_cbs; x++) {
            int tb_x = x >> sps->log2_diff_max_min_coding_block_size;
            int tb_y = y >> sps->log2_diff_max_min_coding_block_size;
            int ctb_addr_rs = sps->pic_width_in_ctbs * tb_y + tb_x;
            int val = pps->ctb_addr_rs_to_ts[ctb_addr_rs] <<
                      (sps->log2_diff_max_min_coding_block_size * 2);
            for (i = 0; i < sps->log2_diff_max_min_coding_block_size; i++) {
                int m = 1 << i;
                val += (m & x ? m * m : 0) + (m & y ? 2 * m * m : 0);
            }
            pps->min_cb_addr_zs[y * sps->pic_width_in_min_cbs + x] = val;
        }
    }

    for (y = 0; y < sps->pic_height_in_min_tbs; y++) {
        for (x = 0; x < sps->pic_width_in_min_tbs; x++) {
            int tb_x = x >> sps->log2_diff_ctb_min_tb_size;
            int tb_y = y >> sps->log2_diff_ctb_min_tb_size;
            int ctb_addr_rs = sps->pic_width_in_ctbs * tb_y + tb_x;
            int val = pps->ctb_addr_rs_to_ts[ctb_addr_rs] <<
                      (sps->log2_diff_ctb_min_tb_size * 2);
            for (i = 0; i < sps->log2_diff_ctb_min_tb_size; i++) {
                int m = 1 << i;
                val += (m & x ? m * m : 0) + (m & y ? 2 * m * m : 0);
            }
            pps->min_tb_addr_zs[y * sps->pic_width_in_min_tbs + x] = val;
        }
    }

    if (s->pps_list[pps_id] != NULL && s->threads_type == FF_THREAD_FRAME ) {
        ff_thread_mutex_lock_dpb(s->avctx);
        if (s->pps_list[pps_id]->threadCnt == 0)
            ff_hevc_pps_free(&s->pps_list[pps_id]);
        ff_thread_mutex_unlock_dpb(s->avctx);
    } else if (s->pps_list[pps_id] != NULL)
        ff_hevc_pps_free(&s->pps_list[pps_id]);

    s->pps_list[pps_id] = pps;
    return 0;

err:
    ff_hevc_pps_free(&pps);

    return ret;
}
