/*
 * HEVC Parameter Set Decoding
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

#include "golomb.h"
#include "hevc.h"

/**
 * Section 7.3.3.1
 */
int ff_hevc_decode_short_term_rps(HEVCContext *s, int idx, SPS *sps)
{
	int delta_idx = 1;
    int delta_rps;
    uint8_t used_by_curr_pic_flag;
    uint8_t use_delta_flag;
    int delta_poc;
    int k0 = 0;
    int k1 = 0;
    int k  = 0;
    int i;
	GetBitContext *gb = &s->gb;

    ShortTermRPS *rps = &sps->short_term_rps_list[idx];
    ShortTermRPS *rps_rIdx;

    rps->inter_ref_pic_set_prediction_flag = get_bits1(gb);
    if (rps->inter_ref_pic_set_prediction_flag) {
    	if( idx == sps->num_short_term_ref_pic_sets ) {
    	    delta_idx = get_ue_golomb(gb) + 1;
    	}
    	rps_rIdx = &sps->short_term_rps_list[idx - delta_idx];
    	rps->delta_rps_sign = get_bits1(gb);
	    rps->abs_delta_rps = get_ue_golomb(gb) + 1;
	    delta_rps = (1 - (rps_rIdx->delta_rps_sign<<1)) * rps->abs_delta_rps;
	    for( i = 0; i <= rps_rIdx->num_delta_pocs; i++ ) {
    		used_by_curr_pic_flag = get_bits1(gb);
    	    if( !used_by_curr_pic_flag ) {
    	    	use_delta_flag = get_bits1(gb);
    	    }
    	    if (used_by_curr_pic_flag || use_delta_flag) {
    	    	if (i < rps_rIdx->num_delta_pocs)
    	    		delta_poc = delta_rps + rps_rIdx->delta_poc;
    	    	else
    	    		delta_poc = delta_rps;
    	    	if (delta_poc < 0)
    	    		k0++;
    	    	else
    	    		k1++;
    	    	k++;
    	    }
    	}
	    rps->num_delta_pocs    = k;
	    rps->num_negative_pics = k0;
	    rps->num_positive_pics = k1;
    } else {
        rps->num_negative_pics = get_ue_golomb(gb);
        header_printf("          num_negative_pics                        u(v) : %d\n", rps->num_negative_pics);
        rps->num_positive_pics = get_ue_golomb(gb);
        header_printf("          num_positive_pics                        u(v) : %d\n", rps->num_positive_pics);
        rps->num_delta_pocs = rps->num_negative_pics + rps->num_positive_pics;
        if (rps->num_negative_pics || rps->num_positive_pics) {
        	for( i = 0; i < rps->num_negative_pics; i++ ) {
                header_printf("          delta_poc_s0_minus1                      u(v) : %d\n", get_ue_golomb(gb));
                header_printf("          used_by_curr_pic_s0_flag                 u(1) : %d\n", get_bits1(gb));
        	}
        	for( i = 0; i < rps->num_positive_pics; i++ ) {
                header_printf("          delta_poc_s1_minus1                      u(v) : %d\n", get_ue_golomb(gb));
                header_printf("          used_by_curr_pic_s1_flag                 u(1) : %d\n", get_bits1(gb));
        	}
        }
    }

    return 0;
}

int ff_hevc_decode_nal_vps(HEVCContext *s)
{
    int i;
    GetBitContext *gb = &s->gb;
    int vps_id = 0;
    VPS *vps = av_mallocz(sizeof(*vps));

    av_log(s->avctx, AV_LOG_DEBUG, "Decoding VPS\n");
	header_printf("=========== Video Parameter Set ID:   ===========\n");

    if (!vps)
        return -1;
    vps->vps_max_temporal_layers = get_bits(gb, 3) + 1;
    header_printf("          vps_max_temporal_layers_minus1           u(3) : %d\n", vps->vps_max_temporal_layers-1);
    vps->vps_max_layers = get_bits(gb, 5) + 1;
    header_printf("          vps_max_layers_minus1                    u(5) : %d\n", vps->vps_max_layers-1);
    vps_id = get_ue_golomb(gb);
    header_printf("          video_parameter_set_id                   u(v) : %d\n", vps_id);
    header_printf("          vps_temporal_id_nesting_flag             u(1) : %d\n", get_bits1(gb));
    if (vps_id >= MAX_VPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "VPS id out of range: %d\n", vps_id);
        av_free(vps);
        return -1;
    }

    for (i = 0; i < vps->vps_max_temporal_layers; i++) {
        vps->vps_max_dec_pic_buffering[i] = get_ue_golomb(gb);
        header_printf("          vps_max_dec_pic_buffering[i]             u(v) : %d\n", vps->vps_max_dec_pic_buffering[i]);
        vps->vps_num_reorder_pics[i] = get_ue_golomb(gb);
        header_printf("          vps_num_reorder_pics[i]                  u(v) : %d\n", vps->vps_num_reorder_pics[i]);
        vps->vps_max_latency_increase[i] = get_ue_golomb(gb);
        header_printf("          vps_max_latency_increase[i]              u(v) : %d\n", vps->vps_max_latency_increase[i]);
    }
    header_printf("          vps_extension_flag                       u(1) : %d\n", get_bits1(gb));

    av_free(s->vps_list[vps_id]);
    s->vps_list[vps_id] = vps;
    return 0;
}

int ff_hevc_decode_nal_sps(HEVCContext *s)
{
    int i;
    GetBitContext *gb = &s->gb;

    int sps_id = 0;
#if REFERENCE_ENCODER_QUIRKS
    int max_cu_depth = 0;
#endif
    SPS *sps = av_mallocz(sizeof(*sps));
    if (!sps)
        goto err;

    av_log(s->avctx, AV_LOG_DEBUG, "Decoding SPS\n");
    header_printf("=========== Sequence Parameter Set ID:   ===========\n");

    memset(sps->short_term_rps_list, 0, sizeof(sps->short_term_rps_list));

    // Coded parameters

    sps->profile_space = get_bits(gb, 3);
    header_printf("          profile_space                            u(3) : %d\n", sps->profile_space);
    sps->profile_idc = get_bits(gb, 5);
    header_printf("          profile_idc                              u(5) : %d\n", sps->profile_idc);
    skip_bits(gb, 16); // constraint_flags
    header_printf("          reserved_indicator_flags                 u(16) : 0\n");
    sps->level_idc = get_bits(gb, 8);
    header_printf("          level_idc                                u(8) : %d\n", sps->level_idc);
    skip_bits(gb, 32); // profile_compability_flag[i]
    header_printf("          profile_compatibility                    u(32) : 0\n");

    sps_id         = get_ue_golomb(gb);
    header_printf("          seq_parameter_set_id                     u(v) : %d\n", sps_id);
    if (sps_id >= MAX_SPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "SPS id out of range: %d\n", sps_id);
        goto err;
    }

    sps->vps_id = get_ue_golomb(gb);
    header_printf("          video_parameter_set_id                   u(v) : %d\n", sps->vps_id);
    if (sps->vps_id >= MAX_VPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "VPS id out of range: %d\n", sps->vps_id);
        goto err;
    }

    sps->chroma_format_idc = get_ue_golomb(gb);
    header_printf("          chroma_format_idc                        u(v) : %d\n", sps->chroma_format_idc);
    if (sps->chroma_format_idc == 3) {
        sps->separate_colour_plane_flag = get_bits1(gb);
        header_printf("          separate_colour_plane_flag               u(1) : %d\n", sps->separate_colour_plane_flag);
    }
    sps->max_temporal_layers = get_bits(gb, 3) + 1;
    header_printf("          max_temporal_layers_minus1               u(3) : %d\n", sps->max_temporal_layers-1);

    sps->pic_width_in_luma_samples  = get_ue_golomb(gb);
    header_printf("          pic_width_in_luma_samples                u(v) : %d\n", sps->pic_width_in_luma_samples);
    sps->pic_height_in_luma_samples = get_ue_golomb(gb);
    header_printf("          pic_height_in_luma_samples               u(v) : %d\n", sps->pic_height_in_luma_samples);

    sps->pic_cropping_flag = get_bits1(gb);
    header_printf("          pic_cropping_flag                        u(1) : %d\n", sps->pic_cropping_flag);
    if (sps->pic_cropping_flag) {
        sps->pic_crop.left_offset   = get_ue_golomb(gb);
        header_printf("          pic_crop_left_offset                     u(v) : %d\n", sps->pic_crop.left_offset);
        sps->pic_crop.right_offset  = get_ue_golomb(gb);
        header_printf("          pic_crop_right_offset                    u(v) : %d\n", sps->pic_crop.right_offset);
        sps->pic_crop.top_offset    = get_ue_golomb(gb);
        header_printf("          pic_crop_top_offset                      u(v) : %d\n", sps->pic_crop.top_offset);
        sps->pic_crop.bottom_offset = get_ue_golomb(gb);
        header_printf("          pic_crop_bottom_offset                   u(v) : %d\n", sps->pic_crop.bottom_offset);
    }

    sps->bit_depth[0] = get_ue_golomb(gb) + 8;
    header_printf("          bit_depth_luma_minus8                    u(v) : %d\n", sps->bit_depth[0]-8);
    sps->bit_depth[2] =
    sps->bit_depth[1] = get_ue_golomb(gb) + 8;
    header_printf("          bit_depth_chroma_minus8                  u(v) : %d\n", sps->bit_depth[1]-8);

    sps->pcm_enabled_flag = get_bits1(gb);
    header_printf("          pcm_enabled_flag                         u(1) : %d\n", sps->pcm_enabled_flag);
    if (sps->pcm_enabled_flag) {
        sps->pcm.bit_depth_luma = get_bits(gb, 4) + 1;
        header_printf("          pcm_bit_depth_luma_minus1                u(4) : %d\n", sps->pcm.bit_depth_luma-1);
        sps->pcm.bit_depth_chroma = get_bits(gb, 4) + 1;
        header_printf("          pcm_bit_depth_chroma_minus1              u(4) : %d\n", sps->pcm.bit_depth_chroma-1);
    }

    sps->log2_max_poc_lsb = get_ue_golomb(gb) + 4;
    header_printf("          log2_max_pic_order_cnt_lsb_minus4        u(v) : %d\n", sps->log2_max_poc_lsb-4);

    for (i = 0; i < sps->max_temporal_layers; i++) {
        sps->temporal_layer[i].max_dec_pic_buffering = get_ue_golomb(gb);
        header_printf("          max_dec_pic_buffering                    u(v) : %d\n", sps->temporal_layer[i].max_dec_pic_buffering);
        sps->temporal_layer[i].num_reorder_pics      = get_ue_golomb(gb);
        header_printf("          num_reorder_pics                         u(v) : %d\n", sps->temporal_layer[i].num_reorder_pics);
        sps->temporal_layer[i].max_latency_increase  = get_ue_golomb(gb);
        header_printf("          max_latency_increase                     u(v) : %d\n", sps->temporal_layer[i].max_latency_increase);
    }

    sps->restricted_ref_pic_lists_flag = get_bits1(gb);
    header_printf("          restricted_ref_pic_lists_flag            u(1) : %d\n", sps->restricted_ref_pic_lists_flag);
    if (sps->restricted_ref_pic_lists_flag) {
        sps->lists_modification_present_flag = get_bits1(gb);
        header_printf("          lists_modification_present_flag          u(1) : %d\n", sps->lists_modification_present_flag);
    }
    sps->log2_min_coding_block_size             = get_ue_golomb(gb) + 3;
    header_printf("          log2_min_coding_block_size_minus3        u(v) : %d\n", sps->log2_min_coding_block_size-3);
    sps->log2_diff_max_min_coding_block_size    = get_ue_golomb(gb);
    header_printf("          log2_diff_max_min_coding_block_size      u(v) : %d\n", sps->log2_diff_max_min_coding_block_size);
    sps->log2_min_transform_block_size          = get_ue_golomb(gb) + 2;
    header_printf("          log2_min_transform_block_size_minus2     u(v) : %d\n", sps->log2_min_transform_block_size-2);
    sps->log2_diff_max_min_transform_block_size = get_ue_golomb(gb);
    header_printf("          log2_diff_max_min_transform_block_size   u(v) : %d\n", sps->log2_diff_max_min_transform_block_size);

    if (sps->pcm_enabled_flag) {
        sps->pcm.log2_min_pcm_coding_block_size          = get_ue_golomb(gb) + 3;
        header_printf("          log2_min_pcm_coding_block_size_minus3    u(v) : %d\n", sps->pcm.log2_min_pcm_coding_block_size-3);
        sps->pcm.log2_diff_max_min_pcm_coding_block_size = get_ue_golomb(gb);
        header_printf("          log2_diff_max_min_pcm_coding_block_size  u(v) : %d\n", sps->pcm.log2_diff_max_min_pcm_coding_block_size);
    }

    sps->max_transform_hierarchy_depth_inter = get_ue_golomb(gb);
    header_printf("          max_transform_hierarchy_depth_inter      u(v) : %d\n", sps->max_transform_hierarchy_depth_inter);
    sps->max_transform_hierarchy_depth_intra = get_ue_golomb(gb);
    header_printf("          max_transform_hierarchy_depth_intra      u(v) : %d\n", sps->max_transform_hierarchy_depth_intra);

    sps->scaling_list_enable_flag = get_bits1(gb);
    header_printf("          scaling_list_enabled_flag                u(1) : %d\n", sps->scaling_list_enable_flag);
    if (sps->scaling_list_enable_flag) {
        av_log(s->avctx, AV_LOG_ERROR, "TODO: scaling_list_enable_flag\n");
        goto err;
    }

    sps->asymmetric_motion_partitions_enabled_flag  = get_bits1(gb);
    header_printf("          amp_enabled_flag                         u(1) : %d\n", sps->asymmetric_motion_partitions_enabled_flag);
    sps->sample_adaptive_offset_enabled_flag        = get_bits1(gb);
    header_printf("          sample_adaptive_offset_enabled_flag      u(1) : %d\n", sps->sample_adaptive_offset_enabled_flag);

    if (sps->pcm_enabled_flag) {
        sps->pcm.loop_filter_disable_flag = get_bits1(gb);
        header_printf("          pcm_loop_filter_disable_flag             u(1) : %d\n", sps->pcm.loop_filter_disable_flag);
    }
    sps->temporal_id_nesting_flag = get_bits1(gb);
    header_printf("          temporal_id_nesting_flag                 u(1) : %d\n", sps->temporal_id_nesting_flag);

    sps->num_short_term_ref_pic_sets = get_ue_golomb(gb);
    header_printf("          num_short_term_ref_pic_sets              u(v) : %d\n", sps->num_short_term_ref_pic_sets);
    for (i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
        if (ff_hevc_decode_short_term_rps(s, i, sps) < 0)
            goto err;
    }

    sps->long_term_ref_pics_present_flag = get_bits1(gb);
    header_printf("          long_term_ref_pics_present_flag           u(1) : %d\n", sps->long_term_ref_pics_present_flag);
    sps->sps_temporal_mvp_enabled_flag   = get_bits1(gb);
    header_printf("          sps_temporal_mvp_enable_flag              u(1) : %d\n", sps->sps_temporal_mvp_enabled_flag);

#if REFERENCE_ENCODER_QUIRKS
    max_cu_depth = sps->log2_diff_max_min_coding_block_size
                   + ((sps->log2_min_coding_block_size >
                       sps->log2_min_transform_block_size)
                      ? (sps->log2_min_coding_block_size
                         - sps->log2_min_transform_block_size)
                      : 0);
    for (i = 0; i < max_cu_depth; i++) {
        sps->amvp_mode_flag[i] = get_bits1(gb);
        header_printf("          AMVP_MODE                                 u(1) : %d\n", sps->amvp_mode_flag[i]);
    }
#endif

    sps->sps_extension_flag = get_bits1(gb);
    header_printf("          sps_extension_flag                        u(1) : %d\n", sps->sps_extension_flag);

    // Inferred parameters

    sps->log2_ctb_size = sps->log2_min_coding_block_size
                         + sps->log2_diff_max_min_coding_block_size;
    sps->pic_width_in_ctbs = ROUNDED_DIV(sps->pic_width_in_luma_samples,
                                         (1 << sps->log2_ctb_size));
    sps->pic_height_in_ctbs = ROUNDED_DIV(sps->pic_height_in_luma_samples,
                                          (1 << sps->log2_ctb_size));
    sps->pic_width_in_min_cbs = sps->pic_width_in_luma_samples >>
                                sps->log2_min_coding_block_size;
    sps->pic_height_in_min_cbs = sps->pic_height_in_luma_samples >>
                                 sps->log2_min_coding_block_size;
    sps->pic_width_in_min_tbs = sps->pic_width_in_luma_samples >>
                                sps->log2_min_transform_block_size;
    sps->pic_height_in_min_tbs = sps->pic_height_in_luma_samples >>
                                 sps->log2_min_transform_block_size;
    sps->log2_min_pu_size = sps->log2_min_coding_block_size - 1;

    sps->qp_bd_offset_luma   = 6 * (sps->bit_depth[0] - 8);
    sps->qp_bd_offset_chroma = 6 * (sps->bit_depth[1] - 8);

    av_free(s->sps_list[sps_id]);
    s->sps_list[sps_id] = sps;
    return 0;

err:
    for (i = 0; i < MAX_SHORT_TERM_RPS_COUNT; i++)
        av_free(sps->short_term_rps_list[i]);

    av_free(sps);
    return -1;
}

int ff_hevc_decode_nal_pps(HEVCContext *s)
{
    int i, j, x, y, ctb_addr_rs, tile_id;
    GetBitContext *gb = &s->gb;

    SPS *sps = 0;
    int pps_id = 0;
    int log2_diff_ctb_min_tb_size;

    PPS *pps = av_mallocz(sizeof(*pps));
    if (!pps)
        goto err;

    av_log(s->avctx, AV_LOG_DEBUG, "Decoding PPS\n");
	cabac_printf("=========== Picture Parameter Set ID:   ===========\n");

    // Default values
    pps->cabac_independant_flag = 0;
    pps->loop_filter_across_tiles_enabled_flag = 1;
    pps->num_tile_columns     = 1;
    pps->num_tile_rows        = 1;
    pps->uniform_spacing_flag = 1;


    // Coded parameters
    pps_id = get_ue_golomb(gb);
    header_printf("          pic_parameter_set_id                      u(v) : %d\n", pps_id);

    if (pps_id >= MAX_PPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS id out of range: %d\n", pps_id);
        goto err;
    }
    pps->sps_id = get_ue_golomb(gb);
    header_printf("          seq_parameter_set_id                      u(v) : %d\n", pps->sps_id);

    if (pps->sps_id >= MAX_SPS_COUNT) {
        av_log(s->avctx, AV_LOG_ERROR, "SPS id out of range: %d\n", pps->sps_id);
        goto err;
    }
    sps = s->sps_list[pps->sps_id];

    pps->sign_data_hiding_flag = get_bits1(gb);
    header_printf("          sign_data_hiding_flag                     u(1) : %d\n", pps->sign_data_hiding_flag);
    pps->cabac_init_present_flag = get_bits1(gb);
    header_printf("          cabac_init_present_flag                   u(1) : %d\n", pps->cabac_init_present_flag);
    pps->num_ref_idx_l0_default_active = get_ue_golomb(gb) + 1;
    header_printf("          num_ref_idx_l0_default_active_minus1      u(v) : %d\n", pps->num_ref_idx_l0_default_active-1);
    pps->num_ref_idx_l1_default_active = get_ue_golomb(gb) + 1;
    header_printf("          num_ref_idx_l1_default_active_minus1      u(v) : %d\n", pps->num_ref_idx_l1_default_active-1);
    pps->pic_init_qp_minus26 = get_se_golomb(gb);
    header_printf("          pic_init_qp_minus26                       s(v) : %d\n", pps->pic_init_qp_minus26);
    pps->constrained_intra_pred_flag = get_bits1(gb);
    header_printf("          constrained_intra_pred_flag               u(1) : %d\n", pps->constrained_intra_pred_flag);
    pps->transform_skip_enabled_flag = get_bits1(gb);
    header_printf("          transform_skip_enabled_flag               u(1) : %d\n", pps->transform_skip_enabled_flag);


    pps->cu_qp_delta_enabled_flag = get_bits1(gb);
    header_printf("          cu_qp_delta_enabled_flag                  u(1) : %d\n", pps->cu_qp_delta_enabled_flag);
    if (pps->cu_qp_delta_enabled_flag) {
        pps->diff_cu_qp_delta_depth = get_ue_golomb(gb);
        header_printf("          diff_cu_qp_delta_depth                    u(v) : %d\n", pps->diff_cu_qp_delta_depth);
    }
    pps->cb_qp_offset = get_se_golomb(gb);
    header_printf("          cb_qp_offset                              s(v) : %d\n", pps->cb_qp_offset);
    pps->cr_qp_offset = get_se_golomb(gb);
    header_printf("          cr_qp_offset                              s(v) : %d\n", pps->cr_qp_offset);
    pps->pic_slice_level_chroma_qp_offsets_present_flag = get_bits1(gb);
    header_printf("          slicelevel_chroma_qp_flag                 u(1) : %d\n", pps->pic_slice_level_chroma_qp_offsets_present_flag);
    pps->weighted_pred_flag            = get_bits1(gb);
    header_printf("          weighted_pred_flag                        u(1) : %d\n", pps->weighted_pred_flag);
    pps->weighted_bipred_flag          = get_bits1(gb);
    header_printf("          weighted_bipred_flag                      u(1) : %d\n", pps->weighted_bipred_flag);
    pps->output_flag_present_flag      = get_bits1(gb);
    header_printf("          output_flag_present_flag                  u(1) : %d\n", pps->output_flag_present_flag);
#if REFERENCE_ENCODER_QUIRKS
    pps->dependant_slices_enabled_flag = get_bits1(gb);
    header_printf("          dependent_slices_enabled_flag             u(1) : %d\n", pps->dependant_slices_enabled_flag);
    pps->transquant_bypass_enable_flag = get_bits1(gb);
    header_printf("          transquant_bypass_enable_flag             u(1) : %d\n", pps->transquant_bypass_enable_flag);
#else
    pps->transquant_bypass_enable_flag = get_bits1(gb);
    pps->dependant_slices_enabled_flag = get_bits1(gb);
#endif

    pps->tiles_or_entropy_coding_sync_idc = get_bits(gb, 2);
    header_printf("          tiles_or_entropy_coding_sync_idc          u(2) : %d\n", pps->tiles_or_entropy_coding_sync_idc);
    if (pps->tiles_or_entropy_coding_sync_idc == 1) {
        pps->num_tile_columns     = get_ue_golomb(gb) + 1;
        header_printf("          num_tile_columns_minus1                   u(v) : %d\n", pps->num_tile_columns-1);
        pps->num_tile_rows        = get_ue_golomb(gb) + 1;
        header_printf("          num_tile_rows_minus1                      u(v) : %d\n", pps->num_tile_rows-1);

        pps->column_width = av_malloc(pps->num_tile_columns * sizeof(*pps->column_width));
        pps->row_height   = av_malloc(pps->num_tile_rows * sizeof(*pps->row_height));
        if (!pps->column_width || !pps->row_height)
            goto err;

        pps->uniform_spacing_flag = get_bits1(gb);
        header_printf("          uniform_spacing_flag                      u(1) : %d\n", pps->uniform_spacing_flag);

        if (!pps->uniform_spacing_flag) {
            for (i = 0; i < pps->num_tile_columns - 1; i++) {
                pps->column_width[i] = get_ue_golomb(gb);
                header_printf("          column_width[ ]                          u(v) : %d\n", pps->column_width[i]);
            }
            for (i = 0; i < pps->num_tile_rows - 1; i++) {
                pps->row_height[i] = get_ue_golomb(gb);
                header_printf("          row_height[ ]                            u(v) : %d\n", pps->row_height[i]);
            }
        }
        pps->loop_filter_across_tiles_enabled_flag = get_bits1(gb);
        header_printf("          loop_filter_across_tiles_enabled_flag     u(1) : %d\n", pps->loop_filter_across_tiles_enabled_flag);
    } else if (pps->tiles_or_entropy_coding_sync_idc == 2) {
        pps->cabac_independant_flag = get_bits1(gb);
        header_printf("          num_substreams_minus1                     u(1) : %d\n", pps->cabac_independant_flag);
    }

    pps->seq_loop_filter_across_slices_enabled_flag = get_bits1(gb);
    header_printf("          loop_filter_across_slice_flag             u(1) : %d\n", pps->seq_loop_filter_across_slices_enabled_flag);

    pps->deblocking_filter_control_present_flag = get_bits1(gb);
    header_printf("          deblocking_filter_control_present_flag    u(1) : %d\n", pps->deblocking_filter_control_present_flag);
    if (pps->deblocking_filter_control_present_flag) {
        pps->deblocking_filter_override_enabled_flag = get_bits1(gb);
        header_printf("          deblocking_filter_override_enabled_flag   u(1) : %d\n", pps->deblocking_filter_override_enabled_flag);
        pps->pps_disable_deblocking_filter_flag = get_bits1(gb);
        header_printf("          pps_deblocking_filter_flag                u(1) : %d\n", pps->pps_disable_deblocking_filter_flag);
        if (!pps->pps_disable_deblocking_filter_flag) {
            pps->beta_offset = get_se_golomb(gb) * 2;
            header_printf("          beta_offset_div2                          s(v) : %d\n", pps->beta_offset/2);
            pps->tc_offset = get_se_golomb(gb) * 2;
            header_printf("          tc_offset_div2                            s(v) : %d\n", pps->tc_offset/2);
        }
    }

    pps->pps_scaling_list_data_present_flag = get_bits1(gb);
    header_printf("          pps_scaling_list_data_present_flag        u(1) : %d\n", pps->pps_scaling_list_data_present_flag);
    if (pps->pps_scaling_list_data_present_flag) {
        av_log(s->avctx, AV_LOG_ERROR, "TODO: scaling_list_data_present_flag\n");
        goto err;
    }

    pps->log2_parallel_merge_level = get_ue_golomb(gb) + 2;
    header_printf("          log2_parallel_merge_level_minus2          u(v) : %d\n", pps->log2_parallel_merge_level-2);
    pps->slice_header_extension_present_flag = get_bits1(gb);
    header_printf("          slice_header_extension_present_flag       u(1) : %d\n", pps->slice_header_extension_present_flag);
    pps->pps_extension_flag = get_bits1(gb);
    header_printf("          pps_extension_flag                        u(1) : %d\n", pps->pps_extension_flag);

    // Inferred parameters
    pps->col_bd = av_malloc((pps->num_tile_columns + 1) * sizeof(*pps->col_bd));
    pps->row_bd = av_malloc((pps->num_tile_rows + 1) * sizeof(*pps->row_bd));
    if (!pps->col_bd || !pps->row_bd)
        goto err;

    if (pps->uniform_spacing_flag) {
        pps->column_width = av_malloc(pps->num_tile_columns * sizeof(*pps->column_width));
        pps->row_height   = av_malloc(pps->num_tile_rows * sizeof(*pps->row_height));
        if (!pps->column_width || !pps->row_height)
            goto err;

        for (i = 0; i < pps->num_tile_columns; i++) {
            pps->column_width[i] = ((i + 1) * sps->pic_width_in_ctbs) / (pps->num_tile_columns) -
                                   (i * sps->pic_width_in_ctbs) / (pps->num_tile_columns);
        }

        for (i = 0; i < pps->num_tile_rows; i++) {
            pps->row_height[i] = ((i + 1) * sps->pic_height_in_ctbs) / (pps->num_tile_rows) -
                                 (i * sps->pic_height_in_ctbs) / (pps->num_tile_rows);
        }
    }

    pps->col_bd[0] = 0;
    for (i = 0; i < pps->num_tile_columns; i++)
        pps->col_bd[i+1] = pps->col_bd[i] + pps->column_width[i];

    pps->row_bd[0] = 0;
    for (i = 0; i < pps->num_tile_rows; i++)
        pps->row_bd[i+1] = pps->row_bd[i] + pps->row_height[i];

    /**
     * 6.5
     */
    pps->ctb_addr_rs_to_ts = av_malloc(sps->pic_width_in_ctbs *
                                       sps->pic_height_in_ctbs * sizeof(*pps->ctb_addr_rs_to_ts));
    pps->ctb_addr_ts_to_rs = av_malloc(sps->pic_width_in_ctbs *
                                       sps->pic_height_in_ctbs * sizeof(*pps->ctb_addr_ts_to_rs));
    pps->tile_id = av_malloc(sps->pic_width_in_ctbs *
                             sps->pic_height_in_ctbs * sizeof(*pps->tile_id));
    pps->min_cb_addr_zs = av_malloc(sps->pic_width_in_min_cbs *
                                    sps->pic_height_in_min_cbs * sizeof(*pps->min_cb_addr_zs));

    pps->min_tb_addr_zs = av_malloc(sps->pic_width_in_min_tbs *
                                    sps->pic_height_in_min_tbs * sizeof(*pps->min_tb_addr_zs));
    if (!pps->ctb_addr_rs_to_ts || !pps->ctb_addr_ts_to_rs ||
        !pps->tile_id || !pps->min_cb_addr_zs ||
        !pps->min_tb_addr_zs)
        goto err;

    for (ctb_addr_rs = 0;
         ctb_addr_rs < sps->pic_width_in_ctbs * sps->pic_height_in_ctbs;
         ctb_addr_rs++) {
        int tb_x = ctb_addr_rs % sps->pic_width_in_ctbs;
        int tb_y = ctb_addr_rs / sps->pic_width_in_ctbs;
        int tile_x = 0;
        int tile_y = 0;
        int val = 0;

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
            for (y = pps->row_bd[j]; y < pps->row_bd[j+1]; y++)
                for (x = pps->col_bd[j]; x < pps->col_bd[j+1]; x++)
                    pps->tile_id[pps->ctb_addr_rs_to_ts[y * sps->pic_width_in_ctbs + x]] = tile_id;

    for (y = 0; y < sps->pic_height_in_min_cbs; y++) {
        for (x = 0; x < sps->pic_width_in_min_cbs; x++) {
            int tb_x = x >> sps->log2_diff_max_min_coding_block_size;
            int tb_y = y >> sps->log2_diff_max_min_coding_block_size;
            int ctb_addr_rs = sps->pic_width_in_ctbs * tb_y + tb_x;
            int val = pps->ctb_addr_rs_to_ts[ctb_addr_rs] <<
                      (sps->log2_diff_max_min_coding_block_size * 2);
            for (i = 0; i < sps->log2_diff_max_min_coding_block_size; i++) {
                int m = 1 << i;
                val += (m & x ? m*m : 0) + (m & y ? 2*m*m : 0);
            }
            pps->min_cb_addr_zs[y * sps->pic_width_in_min_cbs + x] = val;
        }
    }

    log2_diff_ctb_min_tb_size = sps->log2_ctb_size - sps->log2_min_transform_block_size;
    for (y = 0; y < sps->pic_height_in_min_tbs; y++) {
        for (x = 0; x < sps->pic_width_in_min_tbs; x++) {
            int tb_x = x >> log2_diff_ctb_min_tb_size;
            int tb_y = y >> log2_diff_ctb_min_tb_size;
            int ctb_addr_rs = sps->pic_width_in_ctbs * tb_y + tb_x;
            int val = pps->ctb_addr_rs_to_ts[ctb_addr_rs] <<
                      (log2_diff_ctb_min_tb_size * 2);
            for (i = 0; i < log2_diff_ctb_min_tb_size; i++) {
                int m = 1 << i;
                val += (m & x ? m*m : 0) + (m & y ? 2*m*m : 0);
            }
            pps->min_tb_addr_zs[y * sps->pic_width_in_min_tbs + x] = val;
        }
    }

    av_free(s->pps_list[pps_id]);
    s->pps_list[pps_id] = pps;
    return 0;

err:
    av_free(pps->column_width);
    av_free(pps->row_height);
    av_free(pps->col_bd);
    av_free(pps->row_bd);
    av_free(pps->ctb_addr_rs_to_ts);
    av_free(pps->ctb_addr_ts_to_rs);
    av_free(pps->tile_id);
    av_free(pps->min_cb_addr_zs);
    av_free(pps->min_tb_addr_zs);

    av_free(pps);
    return -1;
}

static int decode_nal_sei_message(HEVCContext *s)
{
    GetBitContext *gb = &s->gb;

    int payload_type = 0;
    int payload_size = 0;
    int byte = 0xFF;
    av_log(s->avctx, AV_LOG_DEBUG, "Decoding SEI\n");

    while (byte == 0xFF)
        payload_type += (byte = get_bits(gb, 8));

    byte = 0xFF;
    while (byte == 0xFF)
        payload_size += (byte = get_bits(gb, 8));

    skip_bits(gb, 8*payload_size);
    return 0;
}

static int more_rbsp_data(GetBitContext *gb)
{
    return get_bits_left(gb) > 0 && show_bits(gb, 8) != 0x80;
}

int ff_hevc_decode_nal_sei(HEVCContext *s)
{
    do {
        decode_nal_sei_message(s);
    } while (more_rbsp_data(&s->gb));
    return 0;
}
