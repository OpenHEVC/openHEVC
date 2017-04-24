/*
 * HEVC Parameter Set decoding
 *
 * Copyright (C) 2012 - 2103 Guillaume Martres
 * Copyright (C) 2012 - 2103 Mickael Raulet
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 * Copyright (C) 2013 Vittorio Giovara
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

#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "golomb.h"
#include "hevc.h"

static const uint8_t default_scaling_list_intra[] = {
    16, 16, 16, 16, 17, 18, 21, 24,
    16, 16, 16, 16, 17, 19, 22, 25,
    16, 16, 17, 18, 20, 22, 25, 29,
    16, 16, 18, 21, 24, 27, 31, 36,
    17, 17, 20, 24, 30, 35, 41, 47,
    18, 19, 22, 27, 35, 44, 54, 65,
    21, 22, 25, 31, 41, 54, 70, 88,
    24, 25, 29, 36, 47, 65, 88, 115
};

static const uint8_t default_scaling_list_inter[] = {
    16, 16, 16, 16, 17, 18, 20, 24,
    16, 16, 16, 17, 18, 20, 24, 25,
    16, 16, 17, 18, 20, 24, 25, 28,
    16, 17, 18, 20, 24, 25, 28, 33,
    17, 18, 20, 24, 25, 28, 33, 41,
    18, 20, 24, 25, 28, 33, 41, 54,
    20, 24, 25, 28, 33, 41, 54, 71,
    24, 25, 28, 33, 41, 54, 71, 91
};

static const AVRational vui_sar[] = {
    {  0,   1 },
    {  1,   1 },
    { 12,  11 },
    { 10,  11 },
    { 16,  11 },
    { 40,  33 },
    { 24,  11 },
    { 20,  11 },
    { 32,  11 },
    { 80,  33 },
    { 18,  11 },
    { 15,  11 },
    { 64,  33 },
    { 160, 99 },
    {  4,   3 },
    {  3,   2 },
    {  2,   1 },
};

static void remove_pps(HEVCParamSets *s, int id)
{
    if (s->pps_list[id] && s->pps == (const HEVCPPS*)s->pps_list[id]->data)
        s->pps = NULL;
    av_buffer_unref(&s->pps_list[id]);
}

static void remove_sps(HEVCParamSets *s, int id)
{
    int i;
    if (s->sps_list[id]) {
        if (s->sps == (const HEVCSPS*)s->sps_list[id]->data)
            s->sps = NULL;

        /* drop all PPS that depend on this SPS */
        for (i = 0; i < FF_ARRAY_ELEMS(s->pps_list); i++)
            if (s->pps_list[i] && ((HEVCPPS*)s->pps_list[i]->data)->sps_id == id)
                remove_pps(s, i);

        av_assert0(!(s->sps_list[id] && s->sps == (HEVCSPS*)s->sps_list[id]->data));
    }
    av_buffer_unref(&s->sps_list[id]);
}

static void remove_vps(HEVCParamSets *s, int id)
{
    int i;
    if (s->vps_list[id]) {
        if (s->vps == (const HEVCVPS*)s->vps_list[id]->data)
            s->vps = NULL;

        for (i = 0; i < FF_ARRAY_ELEMS(s->sps_list); i++)
            if (s->sps_list[i] && ((HEVCSPS*)s->sps_list[i]->data)->vps_id == id)
                remove_sps(s, i);
    }
    av_buffer_unref(&s->vps_list[id]);
}

int ff_hevc_decode_short_term_rps(GetBitContext *gb, AVCodecContext *avctx,
                                  ShortTermRPS *rps, const HEVCSPS *sps, int is_slice_header)
{
    uint8_t rps_predict = 0;
    int delta_poc;
    int k0 = 0;
    int k1 = 0;
    int k  = 0;
    int i;

    if (rps != sps->st_rps && sps->num_short_term_rps)
        rps_predict = get_bits1(gb);

    if (rps_predict) {
        const ShortTermRPS *rps_ridx;
        int delta_rps;
        unsigned abs_delta_rps;
        uint8_t use_delta_flag = 0;
        uint8_t delta_rps_sign;

        if (is_slice_header) {
            unsigned int delta_idx = get_ue_golomb_long(gb) + 1;
            if (delta_idx > sps->num_short_term_rps) {
                av_log(avctx, AV_LOG_ERROR,
                       "Invalid value of delta_idx in slice header RPS: %d > %d.\n",
                       delta_idx, sps->num_short_term_rps);
                return AVERROR_INVALIDDATA;
            }
            rps_ridx = &sps->st_rps[sps->num_short_term_rps - delta_idx];
            rps->rps_idx_num_delta_pocs = rps_ridx->num_delta_pocs;
        } else
            rps_ridx = &sps->st_rps[rps - sps->st_rps - 1];

        delta_rps_sign = get_bits1(gb);
        abs_delta_rps  = get_ue_golomb_long(gb) + 1;
        if (abs_delta_rps < 1 || abs_delta_rps > 32768) {
            av_log(avctx, AV_LOG_ERROR,
                   "Invalid value of abs_delta_rps: %d\n",
                   abs_delta_rps);
            return AVERROR_INVALIDDATA;
        }
        delta_rps      = (1 - (delta_rps_sign << 1)) * abs_delta_rps;
        for (i = 0; i <= rps_ridx->num_delta_pocs; i++) {
            int used = rps->used[k] = get_bits1(gb);

            if (!used)
                use_delta_flag = get_bits1(gb);

            if (used || use_delta_flag) {
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
        }

        rps->num_delta_pocs    = k;
        rps->num_negative_pics = k0;
        // sort in increasing order (smallest first)
        if (rps->num_delta_pocs != 0) {
            int used, tmp;
            for (i = 1; i < rps->num_delta_pocs; i++) {
                delta_poc = rps->delta_poc[i];
                used      = rps->used[i];
                for (k = i - 1; k >= 0; k--) {
                    tmp = rps->delta_poc[k];
                    if (delta_poc < tmp) {
                        rps->delta_poc[k + 1] = tmp;
                        rps->used[k + 1]      = rps->used[k];
                        rps->delta_poc[k]     = delta_poc;
                        rps->used[k]          = used;
                    }
                }
            }
        }
        if ((rps->num_negative_pics >> 1) != 0) {
            int used;
            k = rps->num_negative_pics - 1;
            // flip the negative values to largest first
            for (i = 0; i < rps->num_negative_pics >> 1; i++) {
                delta_poc         = rps->delta_poc[i];
                used              = rps->used[i];
                rps->delta_poc[i] = rps->delta_poc[k];
                rps->used[i]      = rps->used[k];
                rps->delta_poc[k] = delta_poc;
                rps->used[k]      = used;
                k--;
            }
        }
    } else {
        unsigned int prev, nb_positive_pics;
        rps->num_negative_pics = get_ue_golomb_long(gb);
        nb_positive_pics       = get_ue_golomb_long(gb);

        if (rps->num_negative_pics >= MAX_REFS ||
            nb_positive_pics >= MAX_REFS) {
            av_log(avctx, AV_LOG_ERROR, "Too many refs in a short term RPS.\n");
            return AVERROR_INVALIDDATA;
        }

        rps->num_delta_pocs = rps->num_negative_pics + nb_positive_pics;
        if (rps->num_delta_pocs) {
            prev = 0;
            for (i = 0; i < rps->num_negative_pics; i++) {
                delta_poc = get_ue_golomb_long(gb) + 1;
                prev -= delta_poc;
                rps->delta_poc[i] = prev;
                rps->used[i]      = get_bits1(gb);
            }
            prev = 0;
            for (i = 0; i < nb_positive_pics; i++) {
                delta_poc = get_ue_golomb_long(gb) + 1;
                prev += delta_poc;
                rps->delta_poc[rps->num_negative_pics + i] = prev;
                rps->used[rps->num_negative_pics + i]      = get_bits1(gb);
            }
        }
    }
    return 0;
}


static int decode_profile_tier_level(GetBitContext *gb, AVCodecContext *avctx,
                                      PTLCommon *ptl)
{
    int i;

    if (get_bits_left(gb) < 2+1+5 + 32 + 4 + 16 + 16 + 12)
        return -1;

    ptl->profile_space = get_bits(gb, 2);
    ptl->tier_flag     = get_bits1(gb);
    ptl->profile_idc   = get_bits(gb, 5);

    print_cabac("XXX_profile_idc", ptl->profile_idc);
    print_cabac("XXX_profile_space", ptl->profile_space);
    print_cabac("XXX_tier_flag", ptl->tier_flag);

    //TODO: Add handled profiles
    if (ptl->profile_idc == FF_PROFILE_HEVC_MAIN)
        av_log(avctx, AV_LOG_DEBUG, "Main profile bitstream\n");
    else if (ptl->profile_idc == FF_PROFILE_HEVC_MAIN_10)
        av_log(avctx, AV_LOG_DEBUG, "Main 10 profile bitstream\n");
    else if (ptl->profile_idc == FF_PROFILE_HEVC_MAIN_STILL_PICTURE)
        av_log(avctx, AV_LOG_DEBUG, "Main Still Picture profile bitstream\n");
    else if (ptl->profile_idc == FF_PROFILE_HEVC_REXT)
        av_log(avctx, AV_LOG_DEBUG, "Range Extension profile bitstream\n");
    else
        av_log(avctx, AV_LOG_WARNING, "Unknown HEVC profile: %d\n", ptl->profile_idc);

    for (i = 0; i < 32; i++) {
        ptl->profile_compatibility_flag[i] = get_bits1(gb);
        print_cabac("XXX_profile_compatibility_flag", ptl->profile_compatibility_flag[i]);
    }
    ptl->progressive_source_flag    = get_bits1(gb);
    ptl->interlaced_source_flag     = get_bits1(gb);
    ptl->non_packed_constraint_flag = get_bits1(gb);
    ptl->frame_only_constraint_flag = get_bits1(gb);

    print_cabac("general_frame_only_constraint_flag", ptl->frame_only_constraint_flag);
    print_cabac("general_non_packed_constraint_flag", ptl->non_packed_constraint_flag);
    print_cabac("general_progressive_source_flag", ptl->progressive_source_flag);
    print_cabac("general_interlaced_source_flag", ptl->interlaced_source_flag);

#if MULTIPLE_PTL_SUPPORT
    if( ptl->profile_idc == FF_PROFILE_HEVC_REXT || ptl->profile_compatibility_flag[4] ||
        ptl->profile_idc == FF_PROFILE_HEVC_HIGHTHROUGHPUTREXT || ptl->profile_compatibility_flag[5] ||
        ptl->profile_idc == FF_PROFILE_HEVC_MULTIVIEWMAIN || ptl->profile_compatibility_flag[6] ||
        ptl->profile_idc == FF_PROFILE_HEVC_SCALABLEMAIN || ptl->profile_compatibility_flag[7] ) {
        get_bits1(gb); // general_max_12bit_constraint_flag
        get_bits1(gb); //general_max_10bit_constraint_flag
        ptl->setProfileIdc = (get_bits1(gb)) ? FF_PROFILE_HEVC_SCALABLEMAIN : FF_PROFILE_HEVC_SCALABLEMAIN10; //general_max_8bit_constraint_flag
        print_cabac("general_max_8bit_constraint_flag", ptl->setProfileIdc);
        get_bits1(gb);   //general_max_422chroma_constraint_flag
        get_bits1(gb);   //general_max_420chroma_constraint_flag
        get_bits1(gb);   //general_max_monochrome_constraint_flag
        get_bits1(gb);   //general_intra_constraint_flag
        get_bits1(gb);   //general_one_picture_only_constraint_flag
        get_bits1(gb);   //general_lower_bit_rate_constraint_flag
       
        skip_bits(gb, 32); //general_reserved_zero_34bits
        skip_bits(gb, 2);//general_reserved_zero_34bits
    } else {
        skip_bits(gb, 32); // general_reserved_zero_43bits
        skip_bits(gb, 11); // general_reserved_zero_43bits
    }
    if( ( ptl->profile_idc >= 1 && ptl->profile_idc <= 5 ) ||
        ptl->profile_compatibility_flag[1] || ptl->profile_compatibility_flag[2] ||
        ptl->profile_compatibility_flag[3] || ptl->profile_compatibility_flag[4] ||
        ptl->profile_compatibility_flag[5]) {

        ptl->general_inbld_flag = get_bits1(gb);
        print_cabac("general_inbld_flag", ptl->general_inbld_flag)
    } else {
        get_bits1(gb);// general_reserved_zero_bit
    }
#else
    skip_bits(gb, 16); // XXX_reserved_zero_44bits[0..15]
    skip_bits(gb, 16); // XXX_reserved_zero_44bits[16..31]
    skip_bits(gb, 12); // XXX_reserved_zero_44bits[32..43]
#endif
    return 0;
}

static int parse_ptl(GetBitContext *gb, AVCodecContext *avctx,
                      PTL *ptl, int max_num_sub_layers, int profile_present_flag)
{
    int i;
    if (profile_present_flag){
        if (decode_profile_tier_level(gb, avctx, &ptl->general_ptl) < 0 ||
            get_bits_left(gb) < 8 + (8*2 * (max_num_sub_layers - 1 > 0))) {
            av_log(avctx, AV_LOG_ERROR, "PTL information too short\n");
            return -1;
        }
    }

    ptl->general_ptl.level_idc = get_bits(gb, 8);

    for (i = 0; i < max_num_sub_layers - 1; i++) {
        ptl->sub_layer_profile_present_flag[i] = get_bits1(gb);
        ptl->sub_layer_level_present_flag[i]   = get_bits1(gb);
    }

    if (max_num_sub_layers - 1> 0)
        for (i = max_num_sub_layers - 1; i < 8; i++)
            skip_bits(gb, 2); // reserved_zero_2bits[i]
    for (i = 0; i < max_num_sub_layers - 1; i++) {
        if (ptl->sub_layer_profile_present_flag[i] &&
            decode_profile_tier_level(gb, avctx, &ptl->sub_layer_ptl[i]) < 0) {
            av_log(avctx, AV_LOG_ERROR,
                   "PTL information for sublayer %i too short\n", i);
            return -1;
        }
        if (ptl->sub_layer_level_present_flag[i]) {
            if (get_bits_left(gb) < 8) {
                av_log(avctx, AV_LOG_ERROR,
                       "Not enough data for sublayer %i level_idc\n", i);
                return -1;
            } else
                ptl->sub_layer_ptl[i].level_idc = get_bits(gb, 8);
        }
    }

    return 0;
}

static void sub_layer_hrd_parameters(GetBitContext *gb, SubLayerHRDParameter *Sublayer_HRDPar, int CpbCnt, int sub_pic_hrd_params_present_flag ) {
    int i;
	for( i = 0; i  <=  CpbCnt; i++ ) {
		Sublayer_HRDPar->bit_rate_value_minus1[i] = get_ue_golomb_long(gb);
        print_cabac("bit_rate_value_minus1", Sublayer_HRDPar->bit_rate_value_minus1[i]);
		Sublayer_HRDPar->cpb_size_value_minus1[i] = get_ue_golomb_long(gb);
		if( sub_pic_hrd_params_present_flag ) {
			Sublayer_HRDPar->cpb_size_du_value_minus1[i] = get_ue_golomb_long(gb);
            print_cabac("cpb_size_du_value_minus1", Sublayer_HRDPar->cpb_size_du_value_minus1[i]);
			Sublayer_HRDPar->bit_rate_du_value_minus1[i] = get_ue_golomb_long(gb);
            print_cabac("bit_rate_du_value_minus1", Sublayer_HRDPar->bit_rate_du_value_minus1[i]);
		}
		Sublayer_HRDPar->cbr_flag[i] = get_bits1(gb);
        print_cabac("cbr_flag", Sublayer_HRDPar->cbr_flag[i]);
	}
}

static int decode_hrd(GetBitContext *gb, AVCodecContext *avctx, HRDParameter *HrdParam, int common_inf_present,
                       int max_sublayers)
{
    int i;
    //GetBitContext   *gb   = &s->HEVClc->gb;  
    if (common_inf_present) {
        HrdParam->nal_hrd_parameters_present_flag = get_bits1(gb);
        HrdParam->vcl_hrd_parameters_present_flag = get_bits1(gb);
        print_cabac("vcl_hrd_parameters_present_flag", HrdParam->vcl_hrd_parameters_present_flag);
        print_cabac("nal_hrd_parameters_present_flag", HrdParam->nal_hrd_parameters_present_flag);
        if (HrdParam->nal_hrd_parameters_present_flag  ||  HrdParam->vcl_hrd_parameters_present_flag ) {
            HrdParam->sub_pic_hrd_params_present_flag = get_bits1(gb);
            print_cabac("sub_pic_hrd_params_present_flag", HrdParam->sub_pic_hrd_params_present_flag);
            if( HrdParam->sub_pic_hrd_params_present_flag ) {
                HrdParam->tick_divisor_minus2 = get_bits(gb, 8);
                HrdParam->du_cpb_removal_delay_increment_length_minus1 = get_bits(gb, 5);
                HrdParam->sub_pic_cpb_params_in_pic_timing_sei_flag = get_bits1(gb);
                HrdParam->dpb_output_delay_du_length_minus1 = get_bits(gb, 5);
                print_cabac("dpb_output_delay_du_length_minus1", HrdParam->dpb_output_delay_du_length_minus1);
                print_cabac("tick_divisor_minus2", HrdParam->tick_divisor_minus2);
                print_cabac("du_cpb_removal_delay_increment_length_minus1", HrdParam->du_cpb_removal_delay_increment_length_minus1);
                print_cabac("sub_pic_cpb_params_in_pic_timing_sei_flag", HrdParam->sub_pic_cpb_params_in_pic_timing_sei_flag);
            }
            HrdParam->bit_rate_scale = get_bits(gb, 4);
            HrdParam->cpb_size_scale = get_bits(gb, 4);
            print_cabac("bit_rate_scale", HrdParam->bit_rate_scale);
            print_cabac("cpb_size_scale", HrdParam->cpb_size_scale);
            if( HrdParam->sub_pic_hrd_params_present_flag ) {
                HrdParam->cpb_size_du_scale = get_bits(gb, 4);
                print_cabac("sub_pic_hrd_params_present_flag", HrdParam->sub_pic_hrd_params_present_flag);
            }
            HrdParam->initial_cpb_removal_delay_length_minus1 = get_bits(gb, 5);
            HrdParam->au_cpb_removal_delay_length_minus1 = get_bits(gb, 5);
            HrdParam->dpb_output_delay_length_minus1 = get_bits(gb, 5);
            print_cabac("initial_cpb_removal_delay_length_minus1", HrdParam->initial_cpb_removal_delay_length_minus1);
            print_cabac("au_cpb_removal_delay_length_minus1", HrdParam->au_cpb_removal_delay_length_minus1);
            print_cabac("dpb_output_delay_length_minus1", HrdParam->dpb_output_delay_length_minus1);
        } else
            HrdParam->initial_cpb_removal_delay_length_minus1 = 23;
    }
    for( i = 0; i  <=  max_sublayers; i++ ) {
        HrdParam->fixed_pic_rate_general_flag[i] = get_bits1(gb);
        print_cabac("fixed_pic_rate_general_flag", HrdParam->fixed_pic_rate_general_flag[i]);
        if(!HrdParam->fixed_pic_rate_general_flag[i]) {
            HrdParam->fixed_pic_rate_within_cvs_flag[i] = get_bits1(gb);
            print_cabac("fixed_pic_rate_within_cvs_flag", HrdParam->fixed_pic_rate_within_cvs_flag[i]);
        } else {
            HrdParam->fixed_pic_rate_within_cvs_flag[i] = 1;
        }
        HrdParam->low_delay_hrd_flag [i] = 0;
        HrdParam->cpb_cnt_minus1     [i] = 0;

        if( HrdParam->fixed_pic_rate_within_cvs_flag[i] ) {
            HrdParam->elemental_duration_in_tc_minus1[i] = get_ue_golomb_long(gb);
            print_cabac("elemental_duration_in_tc_minus1", HrdParam->elemental_duration_in_tc_minus1[i]);
        } else {
            HrdParam->low_delay_hrd_flag[i] = get_bits1(gb);
            print_cabac("low_delay_hrd_flag", HrdParam->low_delay_hrd_flag[i]);
        }

        if(!HrdParam->low_delay_hrd_flag[i]) {
            HrdParam->cpb_cnt_minus1[i] = get_ue_golomb_long(gb);
            print_cabac("cpb_cnt_minus1", HrdParam->cpb_cnt_minus1[i]);
            if (HrdParam->cpb_cnt_minus1[i] < 0 || HrdParam->cpb_cnt_minus1[i] > 31) {
                av_log(avctx, AV_LOG_ERROR, "nb_cpb %d invalid\n", HrdParam->cpb_cnt_minus1[i]);
                return AVERROR_INVALIDDATA;
            }
        }
        if( HrdParam->nal_hrd_parameters_present_flag )
            sub_layer_hrd_parameters(gb, &HrdParam->Sublayer_HRDPar[i], HrdParam->cpb_cnt_minus1[i], HrdParam->sub_pic_hrd_params_present_flag);

        if( HrdParam->vcl_hrd_parameters_present_flag )
            sub_layer_hrd_parameters(gb, &HrdParam->Sublayer_HRDPar[i], HrdParam->cpb_cnt_minus1[i], HrdParam->sub_pic_hrd_params_present_flag);
    }
    return 0; 
}


static void vps_vui_bsp_hrd_params(GetBitContext *gb, AVCodecContext *avctx, HEVCVPS *vps, BspHrdParams *Bsp_Hrd_Params) {
    int i, k, h, j, r, t;
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    //GetBitContext   *gb             = &s->HEVClc->gb;  
	Bsp_Hrd_Params->vps_num_add_hrd_params  = get_ue_golomb_long(gb);
	for( i = vps->vps_num_hrd_parameters; i < vps->vps_num_hrd_parameters + Bsp_Hrd_Params->vps_num_add_hrd_params; i++ ) {
        if( i > 0 )
            Bsp_Hrd_Params->cprms_add_present_flag[i] = get_bits1(gb);
        else {
            if(vps->vps_num_hrd_parameters == 0) {
                Bsp_Hrd_Params->cprms_add_present_flag[0] = 1;
            }
        }
        Bsp_Hrd_Params->num_sub_layer_hrd_minus1[i] = get_ue_golomb_long(gb);
        decode_hrd(gb, avctx, &Bsp_Hrd_Params->HrdParam[i], Bsp_Hrd_Params->cprms_add_present_flag[i], vps->vps_max_sub_layers );
    }
	if( (vps->vps_num_hrd_parameters + Bsp_Hrd_Params->vps_num_add_hrd_params) > 0 )
        for( h = 1; h < Hevc_VPS_Ext->NumOutputLayerSets; h++ ) {
            int lsIdx = Hevc_VPS_Ext->layer_set_idx_for_ols[h]; 
            Bsp_Hrd_Params->num_signalled_partitioning_schemes[h] = get_ue_golomb_long(gb);
			for( j = 1; j < Bsp_Hrd_Params->num_signalled_partitioning_schemes[h] + 1; j++ ) {
                Bsp_Hrd_Params->num_partitions_in_scheme_minus1[h][j] = get_ue_golomb_long(gb);
				for( k = 0; k  <=  Bsp_Hrd_Params->num_partitions_in_scheme_minus1[h][j]; k++ )
                    for( r = 0; r < Hevc_VPS_Ext->num_Layers_in_id_list[ lsIdx ]; r++ )
                        Bsp_Hrd_Params->layer_included_in_partition_flag[h][j][k][r] = get_bits1(gb);
            }
			for( i = 0; i < Bsp_Hrd_Params->num_signalled_partitioning_schemes[ h ] + 1; i++ )
				for( t = 0; t  <=  Hevc_VPS_Ext->max_sub_layers_in_layer_set_minus1[ lsIdx ]; t++ ) {
					Bsp_Hrd_Params->num_bsp_schedules_minus1[h][i][t] = get_ue_golomb_long(gb);
					for( j = 0; j  <=  Bsp_Hrd_Params->num_bsp_schedules_minus1[h][i][t]; j++ )
						for( k = 0; k  <=  Bsp_Hrd_Params->num_partitions_in_scheme_minus1[h][i]; k++ ) {
							if( vps->vps_num_hrd_parameters + Bsp_Hrd_Params->vps_num_add_hrd_params > 1 ) {
                                int numBits = 1;
                                while ((1 << numBits) < (vps->vps_num_hrd_parameters + Bsp_Hrd_Params->vps_num_add_hrd_params))
                                    numBits++;
								Bsp_Hrd_Params->bsp_hrd_idx[h][i][t][j][k ] = get_bits(gb, numBits);
                            }
                            Bsp_Hrd_Params->bsp_sched_idx[h][i][t][j][k] = get_ue_golomb_long(gb);
                        }
				}
		}
}



static int scal_type_to_scal_idx(HEVCVPS *vps, enum ScalabilityType scal_type) {
    int scal_idx = 0, curScalType;
    for( curScalType = 0; curScalType < scal_type; curScalType++ )
        scal_idx += ( vps->vps_ext.scalability_mask[curScalType] ? 1 : 0 );
    return scal_idx;
}

static int get_scalability_id(HEVCVPS * vps, int layer_id_in_vps, enum ScalabilityType scal_type) {
    int scal_idx = scal_type_to_scal_idx(vps, scal_type);
    return vps->vps_ext.scalability_mask[scal_type] ? vps->vps_ext.dimension_id[layer_id_in_vps][scal_idx] : 0;
}

static int get_view_index(HEVCVPS *vps, int id){

    return get_scalability_id(vps, vps->vps_ext.layer_id_in_vps[id], VIEW_ORDER_INDEX);
}

static int get_num_views(HEVCVPS *vps) {
    int num_views = 1, i;
    for (i = 0; i < vps->vps_max_layers; i++) {
        int lId = vps->vps_ext.layer_id_in_nuh[i];
        if (i > 0 && (get_view_index(vps, lId) != get_scalability_id(vps, i - 1, VIEW_ORDER_INDEX)))
            num_views++;
    }
    return num_views;
}

static void parse_rep_format(RepFormat *rep_format, GetBitContext *gb) {

    rep_format->pic_width_vps_in_luma_samples = get_bits(gb, 16);
    rep_format->pic_height_vps_in_luma_samples = get_bits(gb, 16);
    rep_format->chroma_and_bit_depth_vps_present_flag = get_bits1(gb);

    print_cabac(" --- parse RepFormat  --- ", 0);
    print_cabac("pic_width_in_luma_samples", rep_format->pic_width_vps_in_luma_samples);
    print_cabac("pic_height_in_luma_samples", rep_format->pic_height_vps_in_luma_samples);
    print_cabac("chroma_and_bit_depth_vps_present_flag", rep_format->chroma_and_bit_depth_vps_present_flag);

    if (rep_format->chroma_and_bit_depth_vps_present_flag) {

        rep_format->chroma_format_vps_idc = get_bits(gb, 2);
        print_cabac("chroma_format_vps_idc", rep_format->chroma_format_vps_idc);

        if (rep_format->chroma_format_vps_idc == 3) {

            rep_format->separate_colour_plane_vps_flag = get_bits1(gb);
            print_cabac("separate_colour_plane_vps_flag", rep_format->separate_colour_plane_vps_flag);
        }
        rep_format->bit_depth_vps[CHANNEL_TYPE_LUMA] =   get_bits(gb, 4) + 8;  
        rep_format->bit_depth_vps[CHANNEL_TYPE_CHROMA] = get_bits(gb, 4) + 8;

        print_cabac("bit_depth_vps_luma_minus8", rep_format->bit_depth_vps[CHANNEL_TYPE_LUMA] - 8);
        print_cabac("bit_depth_vps_chroma_minus8", rep_format->bit_depth_vps[CHANNEL_TYPE_CHROMA] - 8);
    }
    rep_format->conformance_window_vps_flag = get_bits1(gb);
    print_cabac("conformance_window_vps_flag", rep_format->conformance_window_vps_flag);
    if( rep_format->conformance_window_vps_flag ) {
        rep_format->conf_win_vps_left_offset   = get_ue_golomb_long(gb);
        rep_format->conf_win_vps_right_offset  = get_ue_golomb_long(gb);
        rep_format->conf_win_vps_top_offset    = get_ue_golomb_long(gb);
        rep_format->conf_win_vps_bottom_offset = get_ue_golomb_long(gb);

        print_cabac("conf_win_vps_left_offset", rep_format->conf_win_vps_left_offset);
        print_cabac("conf_win_vps_right_offset", rep_format->conf_win_vps_right_offset);
        print_cabac("conf_win_vps_top_offset", rep_format->conf_win_vps_top_offset);
        print_cabac("conf_win_vps_bottom_offset", rep_format->conf_win_vps_bottom_offset);
    }
}

static void Video_Signal_Info (GetBitContext *gb, VideoSignalInfo *video_signal_info ) {
	video_signal_info->video_vps_format                = get_bits(gb, 3);
	video_signal_info->video_full_range_vps_flag       = get_bits1(gb);
    video_signal_info->color_primaries_vps             = get_bits(gb, 8);
	video_signal_info->transfer_characteristics_vps    = get_bits(gb, 8);
	video_signal_info->matrix_coeffs_vps               = get_bits(gb, 8);
    print_cabac("video_vps_format", video_signal_info->video_vps_format);
    print_cabac("video_full_range_vps_flag", video_signal_info->video_full_range_vps_flag);
    print_cabac("color_primaries_vps", video_signal_info->color_primaries_vps);
    print_cabac("transfer_characteristics_vps", video_signal_info->transfer_characteristics_vps);
    print_cabac("matrix_coeffs_vps", video_signal_info->matrix_coeffs_vps);
}


static void parse_vps_vui(GetBitContext *gb, AVCodecContext *avctx, HEVCVPS *vps) {
    int i,j;
    VPSVUI *vps_vui     = &vps->vps_ext.VpsVui;

    print_cabac(" \n --- parse vps vui --- \n", 0);
    vps_vui->cross_layer_pic_type_aligned_flag  = get_bits1(gb);
    print_cabac("cross_layer_pic_type_aligned_flag", vps_vui->cross_layer_pic_type_aligned_flag); 
    if (!vps_vui->cross_layer_pic_type_aligned_flag) {
        vps_vui->cross_layer_irap_aligned_flag  = get_bits1(gb);
        print_cabac("cross_layer_irap_aligned_flag", vps_vui->cross_layer_irap_aligned_flag);
    } else {
        vps_vui->cross_layer_irap_aligned_flag  = 1;
        vps_vui->all_layers_idr_aligned_flag    = get_bits1(gb);
        print_cabac("all_layers_idr_aligned_flag", vps_vui->all_layers_idr_aligned_flag);
    }
    vps_vui->bit_rate_present_vps_flag = get_bits1(gb);
    vps_vui->pic_rate_present_vps_flag = get_bits1(gb);

    print_cabac("bit_rate_present_vps_flag", vps_vui->bit_rate_present_vps_flag);
    print_cabac("pic_rate_present_vps_flag", vps_vui->pic_rate_present_vps_flag);

	if( vps_vui->bit_rate_present_vps_flag  ||  vps_vui->pic_rate_present_vps_flag )
		for( i = vps->vps_base_layer_internal_flag ? 0 : 1; i < vps->vps_num_layer_sets; i++ )
            for( j = 0; j  <=  vps->vps_ext.sub_layers_vps_max_minus1[i]; j++ ) {
				if( vps_vui->bit_rate_present_vps_flag ) {
					vps_vui->bit_rate_present_flag[i][j] = get_bits1(gb);
                    print_cabac("bit_rate_present_flag", vps_vui->bit_rate_present_flag[i][j]);
                }
                if( vps_vui->pic_rate_present_vps_flag ) {
                    vps_vui->pic_rate_present_flag[i][j] = get_bits1(gb);
                    print_cabac("pic_rate_present_flag", vps_vui->pic_rate_present_flag[i][j]);
                }
                if( vps_vui->bit_rate_present_flag[i][j]) {
                    vps_vui->avg_bit_rate[i][j] = get_bits(gb, 16);
                    print_cabac("avg_bit_rate", vps_vui->avg_bit_rate[i][j]);
                    vps_vui->max_bit_rate[i][j] = get_bits(gb, 16);
                    print_cabac("max_bit_rate", vps_vui->max_bit_rate[i][j]);
                }
				if( vps_vui->pic_rate_present_flag[i][j] ) {
					vps_vui->constant_pic_rate_idc[i][j] = get_bits(gb, 2);
					vps_vui->avg_pic_rate[i][j]          = get_bits(gb, 16);
                    print_cabac("constant_pic_rate_idc", vps_vui->constant_pic_rate_idc[i][j]);
                    print_cabac("avg_pic_rate", vps_vui->avg_pic_rate[i][j]);
				}
			}
    vps_vui->video_signal_info_idx_present_flag = get_bits1(gb);
    print_cabac("video_signal_info_idx_present_flag", vps_vui->video_signal_info_idx_present_flag);
	if( vps_vui->video_signal_info_idx_present_flag ) {
		vps_vui->vps_num_video_signal_info_minus1 = get_bits(gb, 4);
        print_cabac("vps_num_video_signal_info_minus1", vps_vui->vps_num_video_signal_info_minus1);
    }
    for( i = 0; i  <=  vps_vui->vps_num_video_signal_info_minus1; i++ )
        Video_Signal_Info(gb, &vps_vui->video_signal_info[i] );
    if( vps_vui->video_signal_info_idx_present_flag  &&  vps_vui->vps_num_video_signal_info_minus1 > 0 )
        for( i = vps->vps_base_layer_internal_flag ? 0 : 1; i  <  vps->vps_max_layers; i++ ) {
            vps_vui->vps_video_signal_info_idx[ i ]  = get_bits(gb, 4);
            print_cabac("vps_video_signal_info_idx", vps_vui->vps_video_signal_info_idx[i]);
        }
    vps_vui->tiles_not_in_use_flag           = get_bits1(gb);
    print_cabac("tiles_not_in_use_flag", vps_vui->tiles_not_in_use_flag);
    if( !vps_vui->tiles_not_in_use_flag ) {
        for( i = vps->vps_base_layer_internal_flag ? 0 : 1; i  <  vps->vps_max_layers; i++ ) {
            vps_vui->tiles_in_use_flag[ i ]  = get_bits1(gb);
            print_cabac("tiles_in_use_flag", vps_vui->tiles_in_use_flag[ i ]);
            if( vps_vui->tiles_in_use_flag[ i ] ) {
                vps_vui->loop_filter_not_across_tiles_flag[ i ] = get_bits1(gb);
                print_cabac("loop_filter_not_across_tiles_flag", vps_vui->loop_filter_not_across_tiles_flag[ i ]);
            }
        }
        for( i = vps->vps_base_layer_internal_flag ? 1 : 2; i < vps->vps_max_layers; i++ )
            for( j = 0; j < vps->vps_ext.num_direct_ref_layers[ vps->vps_ext.layer_id_in_nuh[ i ] ]; j++ ) {
                int layerIdx = vps->vps_ext.layer_id_in_vps[vps->vps_ext.ref_layer_id[vps->vps_ext.layer_id_in_nuh[i]][j]];
                if( vps_vui->tiles_in_use_flag[ i ]  &&  vps_vui->tiles_in_use_flag[ layerIdx ] ) {
                    vps_vui->tile_boundaries_aligned_flag[ i ][ j ] = get_bits1(gb);
                    print_cabac("tile_boundaries_aligned_flag", vps_vui->tile_boundaries_aligned_flag[ i ][ j ]);
                }
            }
    }
	vps_vui->wpp_not_in_use_flag = get_bits1(gb);
    print_cabac("wpp_not_in_use_flag", vps_vui->wpp_not_in_use_flag);
    if( !vps_vui->wpp_not_in_use_flag )
		for( i = vps->vps_base_layer_internal_flag ? 0 : 1; i  < vps->vps_max_layers; i++ ) {
            vps_vui->wpp_in_use_flag[ i ]   = get_bits1(gb);
            print_cabac("wpp_in_use_flag", vps_vui->wpp_in_use_flag[ i ]);
        }
    vps_vui->single_layer_for_non_irap_flag = get_bits1(gb);
    vps_vui->higher_layer_irap_skip_flag    = get_bits1(gb);
    vps_vui->ilp_restricted_ref_layers_flag = get_bits1(gb);

    print_cabac("single_layer_for_non_irap_flag", vps_vui->single_layer_for_non_irap_flag);
    print_cabac("higher_layer_irap_skip_flag", vps_vui->higher_layer_irap_skip_flag);
    print_cabac("ilp_restricted_ref_layers_flag", vps_vui->ilp_restricted_ref_layers_flag);
    if( vps_vui->ilp_restricted_ref_layers_flag )
        for( i = 1; i  < vps->vps_max_layers; i++ )
            for( j = 0; j < vps->vps_ext.num_direct_ref_layers[ vps->vps_ext.layer_id_in_nuh[ i ] ]; j++ )
                if( vps->vps_base_layer_internal_flag || vps->vps_ext.ref_layer_id[ vps->vps_ext.layer_id_in_nuh[i] ][j ] > 0 ) {
                    vps_vui->min_spatial_segment_offset_plus1[i][j] = get_ue_golomb_long(gb);
                    print_cabac("min_spatial_segment_offset_plus1", vps_vui->min_spatial_segment_offset_plus1[i][j]);
                    if( vps_vui->min_spatial_segment_offset_plus1[i][j] > 0 ) {
                        vps_vui->ctu_based_offset_enabled_flag[i][j] = get_bits1(gb);
                        print_cabac("ctu_based_offset_enabled_flag", vps_vui->ctu_based_offset_enabled_flag[i][j]);
                        if( vps_vui->ctu_based_offset_enabled_flag[i][j] ) {
                            vps_vui->min_horizontal_ctu_offset_plus1[i][j] = get_ue_golomb_long(gb);
                            print_cabac("min_horizontal_ctu_offset_plus1", vps_vui->min_horizontal_ctu_offset_plus1[i][j]);
                        }
                    }
                }
    vps_vui->vps_vui_bsp_hrd_present_flag = get_bits1(gb);
    print_cabac("vps_vui_bsp_hrd_present_flag", vps_vui->vps_vui_bsp_hrd_present_flag);
    if( vps_vui->vps_vui_bsp_hrd_present_flag )
        vps_vui_bsp_hrd_params(gb, avctx, vps, & vps_vui->Bsp_Hrd_Params );

    for(i = 1; i < vps->vps_max_layers; i++ )
        if( vps->vps_ext.num_direct_ref_layers[ vps->vps_ext.layer_id_in_nuh[ i ] ]  ==  0 ) {
            vps_vui->base_layer_parameter_set_compatibility_flag[ i ] = get_bits1(gb);
            print_cabac("base_layer_parameter_set_compatibility_flag", vps_vui->base_layer_parameter_set_compatibility_flag[i]);
        }
}
static void calculateMaxSLInLayerSets(HEVCVPS *vps) {
    int k, lId, lsIdx, maxSLMinus1;
    for(lsIdx = 0; lsIdx < vps->vps_num_layer_sets ; lsIdx++) {
        maxSLMinus1 = 0;
        for( k = 0; k < vps->vps_ext.num_Layers_in_id_list[lsIdx]; k++ ) {
            lId = vps->vps_ext.layer_SetLayer_IdList[lsIdx][k];
            maxSLMinus1 = maxSLMinus1> vps->vps_ext.sub_layers_vps_max_minus1[vps->vps_ext.layer_id_in_vps[lId]] ? maxSLMinus1: vps->vps_ext.sub_layers_vps_max_minus1[vps->vps_ext.layer_id_in_vps[lId]];
        }
        vps->vps_ext.max_sub_layers_in_layer_set_minus1[lsIdx] = maxSLMinus1;
    }
}
static void dpb_size(GetBitContext *gb, HEVCVPS *vps ) {
    int i, j, k;
    DPBSize *DPB_Size = &vps->vps_ext.DPB_Size;
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    calculateMaxSLInLayerSets(vps);
	for( i = 1; i < Hevc_VPS_Ext->NumOutputLayerSets; i++ ) {
        int currLsIdx = Hevc_VPS_Ext->layer_set_idx_for_ols[i];
		DPB_Size->sub_layer_flag_info_present_flag[i] = get_bits1(gb);
        print_cabac("sub_layer_flag_info_present_flag", DPB_Size->sub_layer_flag_info_present_flag[i]);
        for( j = 0; j  <=  Hevc_VPS_Ext->sub_layers_vps_max_minus1[currLsIdx]; j++ ) {
            if( j > 0  &&  DPB_Size->sub_layer_flag_info_present_flag[i] ) {
                DPB_Size->sub_layer_dpb_info_present_flag[i][j] = get_bits1(gb);
                print_cabac("sub_layer_dpb_info_present_flag", DPB_Size->sub_layer_dpb_info_present_flag[i][j]);
            } else {
                if( !j )
                    DPB_Size->sub_layer_dpb_info_present_flag[i][j] = 1;
                else
                    DPB_Size->sub_layer_dpb_info_present_flag[i][j] = 0;
            }
            if( DPB_Size->sub_layer_dpb_info_present_flag[i][j] ) {
                for( k = 0; k < Hevc_VPS_Ext->num_Layers_in_id_list[currLsIdx]; k++ )
                    if( Hevc_VPS_Ext->necessary_Layer_Flag[i][k]  &&  ( vps->vps_base_layer_internal_flag  || ( Hevc_VPS_Ext->layer_SetLayer_IdList[currLsIdx][k] ) ) ) {
                        DPB_Size->max_vps_dec_pic_buffering_minus1[i][k][j] = get_ue_golomb_long(gb);
                        print_cabac("max_vps_dec_pic_buffering_minus1", DPB_Size->max_vps_dec_pic_buffering_minus1[i][k][j]);
                    }
                    DPB_Size->max_vps_num_reorder_pics[i][j]       = get_ue_golomb_long(gb);
                    DPB_Size->max_vps_latency_increase_plus1[i][j] = get_ue_golomb_long(gb);

                    print_cabac("max_vps_num_reorder_pics", DPB_Size->max_vps_num_reorder_pics[i][j]);
                    print_cabac("max_vps_latency_increase_plus1", DPB_Size->max_vps_latency_increase_plus1[i][j]);
            }
		}
	}
}


static void set_ref_layers_flags(HEVCVPS *vps, int currLayerId) {
    int i, k, refLayerId;
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    for (i = 0; i < Hevc_VPS_Ext->num_direct_ref_layers[currLayerId]; i++) {
        refLayerId = Hevc_VPS_Ext->ref_layer_id[currLayerId][i];
        Hevc_VPS_Ext->recursive_refLayer_flag[currLayerId][refLayerId] = 1;
        for (k = 0; k < MAX_NUM_LAYER_IDS; k++)
            Hevc_VPS_Ext->recursive_refLayer_flag[currLayerId][k] = Hevc_VPS_Ext->recursive_refLayer_flag[currLayerId][k] | Hevc_VPS_Ext->recursive_refLayer_flag[refLayerId][k];
    }
}

static void set_num_ref_layers(HEVCVPS *vps) {
    int i, j, iNuhLId;
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    memset( Hevc_VPS_Ext->number_ref_layers, 0, sizeof( uint8_t )*16*16 );
    for (i = 0; i < vps->vps_max_layers; i++) {
        iNuhLId = Hevc_VPS_Ext->layer_id_in_nuh[i];
        set_ref_layers_flags(vps, iNuhLId);
        for (j = 0; j < MAX_NUM_LAYER_IDS; j++)
            Hevc_VPS_Ext->number_ref_layers[i][iNuhLId] += Hevc_VPS_Ext->recursive_refLayer_flag[iNuhLId][j];
    }
}
static void set_predicted_layerIds(HEVCVPS *vps) {
    int i, iNuhLId, predIdx, j;
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    for (i = 0; i < vps->vps_max_layers - 1; i++) {
        iNuhLId = Hevc_VPS_Ext->layer_id_in_nuh[i];
        predIdx = 0;
        for (j = iNuhLId + 1; j < MAX_NUM_LAYER_IDS; j++) {
            if( Hevc_VPS_Ext->recursive_refLayer_flag[j][iNuhLId] ) {
                Hevc_VPS_Ext->predicted_layerId[iNuhLId][predIdx] = j;
                predIdx++;
            }
        }
        Hevc_VPS_Ext->num_predicted_layers[iNuhLId] = predIdx;
    }
    Hevc_VPS_Ext->num_predicted_layers[Hevc_VPS_Ext->layer_id_in_nuh[vps->vps_max_layers - 1]] = 0;
}

static void set_tree_partition_layer_IdList(HEVCVPS *vps) {
    int i, j; 
    uint8_t countedLayerIdxFlag[MAX_NUM_LAYER_IDS];
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    memset( countedLayerIdxFlag, 0, sizeof(uint8_t)*MAX_NUM_LAYER_IDS );

    Hevc_VPS_Ext->NumIndependentLayers = 0;

    for (i = 0; i < vps->vps_max_layers; i++) {
        int iNuhLId = Hevc_VPS_Ext->layer_id_in_nuh[i];
        if( Hevc_VPS_Ext->num_direct_ref_layers[iNuhLId] == 0 ) {
            Hevc_VPS_Ext->tree_partition_layerIdList[Hevc_VPS_Ext->NumIndependentLayers][0] = iNuhLId;
            Hevc_VPS_Ext->num_Layers_in_tree_partition[Hevc_VPS_Ext->NumIndependentLayers] = 1;
            for( j = 0; j < Hevc_VPS_Ext->num_predicted_layers[iNuhLId]; j++ ) {
                if( !countedLayerIdxFlag[Hevc_VPS_Ext->layer_id_in_vps[iNuhLId]] ) {
                    Hevc_VPS_Ext->tree_partition_layerIdList[Hevc_VPS_Ext->NumIndependentLayers][Hevc_VPS_Ext->num_Layers_in_tree_partition[Hevc_VPS_Ext->NumIndependentLayers]] = Hevc_VPS_Ext->predicted_layerId[iNuhLId][j];
                    Hevc_VPS_Ext->num_Layers_in_tree_partition[Hevc_VPS_Ext->NumIndependentLayers] = Hevc_VPS_Ext->num_Layers_in_tree_partition[Hevc_VPS_Ext->NumIndependentLayers] + 1;
                    countedLayerIdxFlag[Hevc_VPS_Ext->layer_id_in_vps[Hevc_VPS_Ext->predicted_layerId[iNuhLId][j]]];
                }
            }
            Hevc_VPS_Ext->NumIndependentLayers++;
        }
    }
}

static void derive_layerIdList_variablesForAddLayerSets(HEVCVPS *vps) {
    int i, layerNum, lsIdx, layerCnt, treeIdx;
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    for (i = 0; i < Hevc_VPS_Ext->num_add_layer_sets; i++) {
        layerNum = 0;
        lsIdx = vps->vps_num_layer_sets + i;
        for (treeIdx = 1; treeIdx < Hevc_VPS_Ext->NumIndependentLayers; treeIdx++) {
            for (layerCnt = 0; layerCnt < (Hevc_VPS_Ext->highest_layer_idx[i][treeIdx]+1); layerCnt++) {
                Hevc_VPS_Ext->layer_SetLayer_IdList[i][lsIdx] =  Hevc_VPS_Ext->tree_partition_layerIdList[treeIdx][layerCnt];
                layerNum++;
            }
        }
        Hevc_VPS_Ext->num_Layers_in_id_list[i] = layerNum;
    }
}



static void derive_layerIdList_variables(HEVCVPS *vps) {
    int i, m, k;
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    Hevc_VPS_Ext->num_Layers_in_id_list[0] = 1;
    Hevc_VPS_Ext->layer_SetLayer_IdList[0][0] = 0;

    for (i = 1; i < vps->vps_num_layer_sets; i++) {
        k=0;
        for(m = 0; m <= vps->vps_max_layer_id; m++) {
            if( vps->layer_id_included_flag[i][m] ) {
                Hevc_VPS_Ext->layer_SetLayer_IdList[i][k] = m; //m_layerSetLayerIdList[i].push_back(m);
                k++;
            }
        }
        Hevc_VPS_Ext->num_Layers_in_id_list[i] = k;
    }
}


static void derive_necessary_layer_flag(HEVCVPS *vps) {
    HEVCVPSExt  *Hevc_VPS_Ext = &vps->vps_ext;
    int olsIdx, lsLayerIdx, currNuhLayerId, rLsLayerIdx, refNuhLayerId;
    memset(Hevc_VPS_Ext->necessary_Layer_Flag, 0, sizeof(uint8_t)*16*16);

    for(olsIdx = 0; olsIdx < Hevc_VPS_Ext->NumOutputLayerSets; olsIdx++) {
        int lsIdx           = Hevc_VPS_Ext->layer_set_idx_for_ols[olsIdx];
        int numLayersInLs   = Hevc_VPS_Ext->num_Layers_in_id_list[lsIdx];
        for( lsLayerIdx = 0; lsLayerIdx < numLayersInLs; lsLayerIdx++ ) {
            if( Hevc_VPS_Ext->output_layer_flag[olsIdx][lsLayerIdx] ) {
                Hevc_VPS_Ext->necessary_Layer_Flag[olsIdx][lsLayerIdx] = 1;
                currNuhLayerId = Hevc_VPS_Ext->layer_SetLayer_IdList[lsIdx][lsLayerIdx];
                for(  rLsLayerIdx = 0; rLsLayerIdx < lsLayerIdx; rLsLayerIdx++ ) {
                    refNuhLayerId = Hevc_VPS_Ext->layer_SetLayer_IdList[lsIdx][rLsLayerIdx];
                    if( Hevc_VPS_Ext->recursive_refLayer_flag[currNuhLayerId][refNuhLayerId] )
                        Hevc_VPS_Ext->necessary_Layer_Flag[olsIdx][rLsLayerIdx] = 1;
                }
            }
        }
    }
}

#define MAX_VPS_NUM_SCALABILITY_TYPES 16

static void parse_vps_extension (GetBitContext *gb, AVCodecContext *avctx, HEVCVPS *vps)  {
    int i, j, /*k,*/ num_scalability_types = 0;
    //GetBitContext   *gb             = &s->HEVClc->gb;
    HEVCVPSExt      *Hevc_VPS_Ext   = &vps->vps_ext;
    int NumOutputLayersInOutputLayerSet[16],  layerSetIdxForOutputLayerSet;
    int vps_non_vui_extension_length, vps_vui_present_flag;
    print_cabac(" \n --- parse vps extention  --- \n ", s->nuh_layer_id);

    if( vps->vps_max_layers > 1  &&  vps->vps_base_layer_internal_flag )
        //profile_tier_level(gb, avctx, 0, &Hevc_VPS_Ext->ptl[0], vps->vps_max_sub_layers);
        parse_ptl(gb, avctx, &Hevc_VPS_Ext->ptl[0], vps->vps_max_sub_layers, 0);
    Hevc_VPS_Ext->splitting_flag = get_bits1(gb);
    print_cabac("splitting_flag", Hevc_VPS_Ext->splitting_flag);
    
    for (i = 0; i < MAX_VPS_NUM_SCALABILITY_TYPES; i++) {
        Hevc_VPS_Ext->scalability_mask[i] = get_bits1(gb); // scalability_mask[i] 
        print_cabac("scalability_mask", Hevc_VPS_Ext->scalability_mask[i]);
        num_scalability_types += Hevc_VPS_Ext->scalability_mask[i];
    }

    for (i = 0; i < num_scalability_types - Hevc_VPS_Ext->splitting_flag; i++) {
        Hevc_VPS_Ext->dimension_id_len[i] = get_bits(gb, 3) + 1;
        print_cabac("dimension_id_len_minus1", Hevc_VPS_Ext->dimension_id_len[i] - 1);
    }

    if(Hevc_VPS_Ext->splitting_flag) {
        int numBits = 0;
        for(j = 0; j < num_scalability_types - 1; j++)
            numBits += Hevc_VPS_Ext->dimension_id_len[j];
        Hevc_VPS_Ext->dimension_id_len[num_scalability_types-1] = 6 - numBits;
        numBits = 6;
    }
    
    Hevc_VPS_Ext->nuh_layer_id_present_flag = get_bits1(gb);
    print_cabac("vps_nuh_layer_id_present_flag", Hevc_VPS_Ext->nuh_layer_id_present_flag);
    Hevc_VPS_Ext->layer_id_in_nuh[0] = 0;
    Hevc_VPS_Ext->layer_id_in_vps[0] = 0;

    for (i = 1; i < vps->vps_max_layers; i++) {
        if (Hevc_VPS_Ext->nuh_layer_id_present_flag) {
            Hevc_VPS_Ext->layer_id_in_nuh[i] = get_bits(gb, 6);
            print_cabac("layer_id_in_nuh", Hevc_VPS_Ext->layer_id_in_nuh[i]);
        } else
            Hevc_VPS_Ext->layer_id_in_nuh[i] = i;

        Hevc_VPS_Ext->layer_id_in_vps[Hevc_VPS_Ext->layer_id_in_nuh[i]] = i;
        if ( !Hevc_VPS_Ext->splitting_flag )
            for (j = 0; j < num_scalability_types; j++) {
                Hevc_VPS_Ext->dimension_id[i][j] = get_bits(gb, Hevc_VPS_Ext->dimension_id_len[j]);
                print_cabac("dimension_id", Hevc_VPS_Ext->dimension_id[i][j]);
            }
    }

    Hevc_VPS_Ext->view_id_len = get_bits(gb, 4);
    print_cabac("view_id_len", Hevc_VPS_Ext->view_id_len);

    if (Hevc_VPS_Ext->view_id_len)
        for (i = 0; i < get_num_views(vps); i++) {
            Hevc_VPS_Ext->view_id_val[i] = get_bits(gb, Hevc_VPS_Ext->view_id_len);
            print_cabac("view_id_val", Hevc_VPS_Ext->view_id_val[i]);
        }

    Hevc_VPS_Ext->num_direct_ref_layers[0] = 0;
    for (i = 1; i < vps->vps_max_layers; i++) {
        Hevc_VPS_Ext->num_direct_ref_layers[i] = 0;
        for (j = 0; j < i; j++) {
            Hevc_VPS_Ext->direct_dependency_flag[i][j] = get_bits1(gb);
            print_cabac("direct_dependency_flag", Hevc_VPS_Ext->direct_dependency_flag[i][j]);

            if(Hevc_VPS_Ext->direct_dependency_flag[i][j])
                Hevc_VPS_Ext->ref_layer_id[i][Hevc_VPS_Ext->num_direct_ref_layers[i]] = j;
            Hevc_VPS_Ext->num_direct_ref_layers[i] += Hevc_VPS_Ext->direct_dependency_flag[i][j];
        }
    }

    set_num_ref_layers(vps);
    set_predicted_layerIds(vps);
    set_tree_partition_layer_IdList(vps);
  
    if( Hevc_VPS_Ext->NumIndependentLayers > 1 ) {
        Hevc_VPS_Ext->num_add_layer_sets = get_ue_golomb_long(gb);
        print_cabac("num_add_layer_sets", Hevc_VPS_Ext->num_add_layer_sets);
        for( i = 0; i < Hevc_VPS_Ext->num_add_layer_sets; i++ )
            for( j = 1; j < Hevc_VPS_Ext->NumIndependentLayers; j++ ) {
                int len = 1;
                while ((1 << len) < (Hevc_VPS_Ext->num_Layers_in_tree_partition[j] + 1))
                    len++;
                Hevc_VPS_Ext->highest_layer_idx[i][j] = get_bits(gb, len) - 1;  // TODO: compute len
                print_cabac("highest_layer_idx", Hevc_VPS_Ext->highest_layer_idx[i][j]);
            }
        derive_layerIdList_variablesForAddLayerSets(vps);
    } else
        Hevc_VPS_Ext->num_add_layer_sets = 0;
    vps->vps_num_layer_sets += Hevc_VPS_Ext->num_add_layer_sets;
   

    Hevc_VPS_Ext->vps_sub_layers_max_minus1_present_flag = get_bits1(gb);
    print_cabac("vps_sub_layers_max_minus1_present_flag", Hevc_VPS_Ext->vps_sub_layers_max_minus1_present_flag);

    if( Hevc_VPS_Ext->vps_sub_layers_max_minus1_present_flag )
        for( i = 0; i  <  vps->vps_max_layers ; i++ ) {
            Hevc_VPS_Ext->sub_layers_vps_max_minus1[i] = get_bits(gb, 3);
            print_cabac("sub_layers_vps_max_minus1", Hevc_VPS_Ext->sub_layers_vps_max_minus1[ i ]);
        }
    else
        for( i = 0; i  <  vps->vps_max_layers ; i++ )
            Hevc_VPS_Ext->sub_layers_vps_max_minus1[i] = vps->vps_max_sub_layers - 1;

    Hevc_VPS_Ext->max_tid_ref_present_flag = get_bits1(gb);
    print_cabac("max_tid_ref_present_flag", Hevc_VPS_Ext->max_tid_ref_present_flag);
	if( Hevc_VPS_Ext->max_tid_ref_present_flag ) {
		for( i = 0; i < vps->vps_max_layers-1; i++ )
			for( j = i + 1; j  <  vps->vps_max_layers; j++ )
				if( Hevc_VPS_Ext->direct_dependency_flag[j][i] ) {
					Hevc_VPS_Ext->max_tid_il_ref_pics_plus1[i][j] = get_bits(gb, 3);
                    print_cabac("max_tid_il_ref_pics_plus1", Hevc_VPS_Ext->max_tid_il_ref_pics_plus1[i][j]);
                }
    } else {
        for( i = 0; i < vps->vps_max_layers-1; i++ )
			for( j = i + 1; j  <  vps->vps_max_layers; j++ )
					Hevc_VPS_Ext->max_tid_il_ref_pics_plus1[i][j] = 7;
    }
	Hevc_VPS_Ext->all_ref_layers_active_flag = get_bits1(gb);
	Hevc_VPS_Ext->vps_num_profile_tier_level_minus1 = get_ue_golomb_long(gb);
    print_cabac("all_ref_layers_active_flag", Hevc_VPS_Ext->all_ref_layers_active_flag);
    print_cabac("vps_num_profile_tier_level_minus1", Hevc_VPS_Ext->vps_num_profile_tier_level_minus1);

	for( i = vps->vps_base_layer_internal_flag ? 2 : 1; i  <=  Hevc_VPS_Ext->vps_num_profile_tier_level_minus1; i++ ) {
        //  TO Do  Copy profile from previous one 
		Hevc_VPS_Ext->vps_profile_present_flag[i] = get_bits1(gb);
        print_cabac("vps_profile_present_flag", Hevc_VPS_Ext->vps_profile_present_flag[i]);
        parse_ptl(gb, avctx, &Hevc_VPS_Ext->ptl[0], vps->vps_max_sub_layers, Hevc_VPS_Ext->vps_profile_present_flag[i]);
	}

    
    if( vps->vps_num_layer_sets > 1 ) {
		Hevc_VPS_Ext->num_add_olss              = get_ue_golomb_long(gb);
        print_cabac("num_add_olss", Hevc_VPS_Ext->num_add_olss);
		Hevc_VPS_Ext->default_output_layer_idc  = get_bits(gb, 2);
        print_cabac("default_output_layer_idc", Hevc_VPS_Ext->default_output_layer_idc);
	} else
        Hevc_VPS_Ext->num_add_olss = 0;
 
    Hevc_VPS_Ext->NumOutputLayerSets = Hevc_VPS_Ext->num_add_olss + vps->vps_num_layer_sets;
	for(i=1; i < Hevc_VPS_Ext->NumOutputLayerSets; i++ ) {
        if( vps->vps_num_layer_sets > 2 && i >= vps->vps_num_layer_sets ) {
            int numBits = 1;
            while ((1 << numBits) < (vps->vps_num_layer_sets - 1))
                numBits++;
            Hevc_VPS_Ext->layer_set_idx_for_ols[i] = get_bits(gb, numBits)+1;
            print_cabac("layer_set_idx_for_ols_minus1", Hevc_VPS_Ext->layer_set_idx_for_ols[i]);
        } else
            Hevc_VPS_Ext->layer_set_idx_for_ols[i] = i;
        layerSetIdxForOutputLayerSet = Hevc_VPS_Ext->layer_set_idx_for_ols[i];
        if( i > (vps->vps_num_layer_sets-1)  ||  Hevc_VPS_Ext->default_output_layer_idc  ==  2 ) {
            for( j = 0; j < Hevc_VPS_Ext->num_Layers_in_id_list[layerSetIdxForOutputLayerSet]; j++ ) {
                Hevc_VPS_Ext->output_layer_flag[i][j] = get_bits1(gb);
                print_cabac("output_layer_flag", Hevc_VPS_Ext->output_layer_flag[i][j]);
            }
        } else {
            if(Hevc_VPS_Ext->default_output_layer_idc == 1) {
                for( j = 0; j < Hevc_VPS_Ext->num_Layers_in_id_list[layerSetIdxForOutputLayerSet]; j++ )
                    Hevc_VPS_Ext->output_layer_flag[i][j] = (j == (Hevc_VPS_Ext->num_Layers_in_id_list[layerSetIdxForOutputLayerSet]-1));
            } else
                if(Hevc_VPS_Ext->default_output_layer_idc == 0)
                for( j = 0; j < Hevc_VPS_Ext->num_Layers_in_id_list[layerSetIdxForOutputLayerSet]; j++ )
                    Hevc_VPS_Ext->output_layer_flag[i][j] = 1;
        }


        derive_necessary_layer_flag(vps);
        for( j = 0; j < Hevc_VPS_Ext->num_Layers_in_id_list[ layerSetIdxForOutputLayerSet ]; j++ )
            if( Hevc_VPS_Ext->necessary_Layer_Flag[ i ][ j ]  &&  Hevc_VPS_Ext->vps_num_profile_tier_level_minus1 > 0 ) {
                int numBits = 1;
                while ( (1 << numBits) < (Hevc_VPS_Ext->vps_num_profile_tier_level_minus1+1) )
                    numBits++;
                Hevc_VPS_Ext->profile_tier_level_idx[i][j] = get_bits(gb, numBits);
                print_cabac("profile_tier_level_idx", Hevc_VPS_Ext->profile_tier_level_idx[ i ][ j ]);
            }
        NumOutputLayersInOutputLayerSet[i] = 0;
        for (j = 0; j < Hevc_VPS_Ext->num_Layers_in_id_list[layerSetIdxForOutputLayerSet]; j++) {
            NumOutputLayersInOutputLayerSet[i] += Hevc_VPS_Ext->output_layer_flag[i][j];
            if (Hevc_VPS_Ext->output_layer_flag[i][j])
                Hevc_VPS_Ext->OlsHighestOutputLayerId[i] = Hevc_VPS_Ext->layer_SetLayer_IdList[layerSetIdxForOutputLayerSet][j];

        }
        if( (NumOutputLayersInOutputLayerSet[ i ] == 1) &&  Hevc_VPS_Ext->num_direct_ref_layers[ Hevc_VPS_Ext->OlsHighestOutputLayerId[i] ] > 0 ) {
            Hevc_VPS_Ext->alt_output_layer_flag[i] = get_bits1(gb);
            print_cabac("alt_output_layer_flag", Hevc_VPS_Ext->alt_output_layer_flag[i]);
        }
    }
    
    Hevc_VPS_Ext->vps_num_rep_formats_minus1 = get_ue_golomb_long(gb);
    print_cabac("vps_num_rep_formats_minus1", Hevc_VPS_Ext->vps_num_rep_formats_minus1);
    
	for( i = 0; i  <=  Hevc_VPS_Ext->vps_num_rep_formats_minus1; i++ )
        parse_rep_format(&Hevc_VPS_Ext->rep_format[i], gb);
    if( Hevc_VPS_Ext->vps_num_rep_formats_minus1 > 0 ) {
        Hevc_VPS_Ext->rep_format_idx_present_flag = get_bits1(gb);
        print_cabac("rep_format_idx_present_flag", Hevc_VPS_Ext->rep_format_idx_present_flag);
    } else
        Hevc_VPS_Ext->rep_format_idx_present_flag = 0;

    if( Hevc_VPS_Ext->rep_format_idx_present_flag ) {
        int numBits = 1;
        while ((1 << numBits) < (Hevc_VPS_Ext->vps_num_rep_formats_minus1+1))
            numBits++;
        for( i = vps->vps_base_layer_internal_flag ? 1 : 0; i  <  vps->vps_max_layers; i++ ) {
            Hevc_VPS_Ext->vps_rep_format_idx[i] = get_bits(gb, numBits);
            print_cabac("vps_rep_format_idx", Hevc_VPS_Ext->vps_rep_format_idx[i]);
        }
    }
    
    Hevc_VPS_Ext->max_one_active_ref_layer_flag = get_bits1(gb);
    Hevc_VPS_Ext->vps_poc_lsb_aligned_flag = get_bits1(gb);
    print_cabac("max_one_active_ref_layer_flag", Hevc_VPS_Ext->max_one_active_ref_layer_flag);
    print_cabac("vps_poc_lsb_aligned_flag", Hevc_VPS_Ext->vps_poc_lsb_aligned_flag);
    for( i = 1; i  <  vps->vps_max_layers; i++ )
        if( Hevc_VPS_Ext->num_direct_ref_layers[Hevc_VPS_Ext->layer_id_in_nuh[i]] == 0 ) {
            Hevc_VPS_Ext->poc_lsb_not_present_flag[i] = get_bits1(gb);
            print_cabac("poc_lsb_not_present_flag", Hevc_VPS_Ext->poc_lsb_not_present_flag[i]);
        }

    dpb_size(gb, vps);
    Hevc_VPS_Ext->direct_dep_type_len_minus2 = get_ue_golomb_long(gb);
    print_cabac("direct_dep_type_len_minus2", Hevc_VPS_Ext->direct_dep_type_len_minus2);

    Hevc_VPS_Ext->default_direct_dependency_type_flag = get_bits1(gb);
    print_cabac("default_direct_dependency_type_flag", Hevc_VPS_Ext->default_direct_dependency_type_flag);
    
    if (Hevc_VPS_Ext->default_direct_dependency_type_flag)
    {
        Hevc_VPS_Ext->default_direct_dependency_type = get_bits(gb, Hevc_VPS_Ext->direct_dep_type_len_minus2+2);
        print_cabac("default_direct_dependency_type", Hevc_VPS_Ext->default_direct_dependency_type);
    }

    for( i = vps->vps_base_layer_internal_flag ? 1 : 2; i  <  vps->vps_max_layers; i++ )
        for( j = vps->vps_base_layer_internal_flag ? 0 : 1; j < i; j++ )
            if( Hevc_VPS_Ext->direct_dependency_flag[i][j] ) {

                if(Hevc_VPS_Ext->default_direct_dependency_type_flag)
                        Hevc_VPS_Ext->direct_dependency_type[i][j] = Hevc_VPS_Ext->default_direct_dependency_type;
                else {
                    Hevc_VPS_Ext->direct_dependency_type[i][j] = get_bits(gb, Hevc_VPS_Ext->direct_dep_type_len_minus2+2);
                    print_cabac("direct_dependency_type", Hevc_VPS_Ext->direct_dependency_type[i][j]);
                }
            }
    

	vps_non_vui_extension_length = get_ue_golomb_long(gb);
    print_cabac("vps_non_vui_extension_length", vps_non_vui_extension_length);

    for( i = 1; i  <=  vps_non_vui_extension_length; i++ )
      get_bits(gb, 8); // vps_non_vui_extension_data_byte 
    vps_vui_present_flag = get_bits1(gb);

    if( vps_vui_present_flag ) {
		align_get_bits(gb);
        parse_vps_vui(gb, avctx, vps);
    }
}

int ff_hevc_decode_nal_vps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps)
{
    int i,j;
    HEVCVPS *vps;
    AVBufferRef *vps_buf = av_buffer_allocz(sizeof(*vps));
    av_log(NULL, AV_LOG_INFO, "Parsing VPS : size:%d %d\n", gb->size_in_bits, gb->buffer_end - gb->buffer);
    if (!vps_buf)
        return AVERROR(ENOMEM);

    vps = (HEVCVPS*)vps_buf->data;

    av_log(avctx, AV_LOG_DEBUG, "Decoding VPS\n");

    vps->vps_id = get_bits(gb, 4);
    if (vps->vps_id >= MAX_VPS_COUNT) {
        av_log(avctx, AV_LOG_ERROR, "VPS id out of range: %d\n", vps->vps_id);
        goto err;
    }
    av_log(NULL, AV_LOG_INFO, "Parsing VPS : id:%d \n", vps->vps_id);
    print_cabac("vps_video_parameter_set_id", vps->vps_id);

    vps->vps_base_layer_internal_flag  = get_bits(gb, 1);
    vps->vps_base_layer_available_flag = get_bits(gb, 1);
    vps->vps_nonHEVCBaseLayerFlag = (vps->vps_base_layer_available_flag && !vps->vps_base_layer_internal_flag);
    
    print_cabac("vps_base_layer_internal_flag",  vps->vps_base_layer_internal_flag);
    print_cabac("vps_base_layer_available_flag", vps->vps_base_layer_available_flag);
    print_cabac("vps_base_layer_available_flag", vps->vps_base_layer_available_flag);

    vps->vps_max_layers               = get_bits(gb, 6) + 1;
    vps->vps_max_sub_layers           = get_bits(gb, 3) + 1;
    vps->vps_temporal_id_nesting_flag = get_bits1(gb);

    print_cabac("vps_max_layers_minus1",        vps->vps_max_layers-1);
    print_cabac("vps_max_sub_layers_minus1",    vps->vps_max_sub_layers-1);
    print_cabac("vps_temporal_id_nesting_flag", vps->vps_temporal_id_nesting_flag);

    vps->vps_reserved_0xffff_16bits = get_bits(gb, 16);
    if (vps->vps_reserved_0xffff_16bits != 0xffff) { // vps_reserved_ffff_16bits
        av_log(avctx, AV_LOG_ERROR, "vps_reserved_ffff_16bits is not 0xffff\n");
        goto err;
    }

    if (vps->vps_max_sub_layers > MAX_SUB_LAYERS) {
        av_log(avctx, AV_LOG_ERROR, "vps_max_sub_layers out of range: %d\n",
               vps->vps_max_sub_layers);
        goto err;
    }

    if (parse_ptl(gb, avctx, &vps->ptl, vps->vps_max_sub_layers, 1) < 0)
        goto err;

    vps->vps_sub_layer_ordering_info_present_flag = get_bits1(gb);
    print_cabac("vps_sub_layer_ordering_info_present_flag", vps->vps_sub_layer_ordering_info_present_flag);

    i = vps->vps_sub_layer_ordering_info_present_flag ? 0 : vps->vps_max_sub_layers - 1; // Difference with the code, i is always starts from 0
    for (; i < vps->vps_max_sub_layers; i++) {
        vps->vps_max_dec_pic_buffering[i] = get_ue_golomb_long(gb) + 1;
        vps->vps_max_num_reorder_pics[i]  = get_ue_golomb_long(gb);
        vps->vps_max_latency_increase[i]  = get_ue_golomb_long(gb) - 1;

        print_cabac("vps_max_dec_pic_buffering_minus1", vps->vps_max_dec_pic_buffering[i]-1);
        print_cabac("vps_num_reorder_pics", vps->vps_max_num_reorder_pics[i]);
        print_cabac("vps_max_latency_increase_plus1", vps->vps_max_latency_increase[i]+1);

        if (vps->vps_max_dec_pic_buffering[i] > MAX_DPB_SIZE || !vps->vps_max_dec_pic_buffering[i]) {
            av_log(avctx, AV_LOG_ERROR, "vps_max_dec_pic_buffering_minus1 out of range: %d\n",
                   vps->vps_max_dec_pic_buffering[i] - 1);
            goto err;
        }
        if (vps->vps_max_num_reorder_pics[i] > vps->vps_max_dec_pic_buffering[i] - 1) {
            av_log(avctx, AV_LOG_WARNING, "vps_max_num_reorder_pics out of range: %d\n",
                   vps->vps_max_num_reorder_pics[i]);
            if (avctx->err_recognition & AV_EF_EXPLODE)
                goto err;
        }
        // Set all sub layers picture ordering info based on first layer
        if (!vps->vps_sub_layer_ordering_info_present_flag) {
            //FIXME: i should start from 1 here since we should already be at i = last iteration;
          for (i++; i < vps->vps_max_sub_layers; i++) {
              vps->vps_max_dec_pic_buffering[i] = vps->vps_max_dec_pic_buffering[0];
              vps->vps_max_num_reorder_pics[i]  = vps->vps_max_num_reorder_pics[0];
              vps->vps_max_latency_increase[i]  = vps->vps_max_latency_increase[0];
          }
          break;
        }
    }

    vps->vps_max_layer_id   = get_bits(gb, 6);
    vps->vps_num_layer_sets = get_ue_golomb_long(gb) + 1;

    if (vps->vps_num_layer_sets < 1 || vps->vps_num_layer_sets > 1024 ||
        (vps->vps_num_layer_sets - 1LL) * (vps->vps_max_layer_id + 1LL) > get_bits_left(gb)) {
        av_log(avctx, AV_LOG_ERROR, "vps_num_layer_sets out of range: %d\n",
               vps->vps_num_layer_sets - 1);
        goto err;
    }
    print_cabac("vps_max_layer_id", vps->vps_max_layer_id);
    print_cabac("vps_num_layer_sets_minus1", vps->vps_num_layer_sets - 1);

    for (i = 1; i < vps->vps_num_layer_sets; i++)
        for (j = 0; j <= vps->vps_max_layer_id; j++) {
            vps->layer_id_included_flag[i][j] = get_bits1(gb);
            print_cabac("layer_id_included_flag", vps->layer_id_included_flag[i][j]);
        }

    derive_layerIdList_variables(vps);
 
    vps->vps_timing_info_present_flag = get_bits1(gb);
    print_cabac("vps_timing_info_present_flag", vps->vps_timing_info_present_flag);
    if (vps->vps_timing_info_present_flag) {
        vps->vps_num_units_in_tick               = get_bits_long(gb, 32);
        vps->vps_time_scale                      = get_bits_long(gb, 32);
        vps->vps_poc_proportional_to_timing_flag = get_bits1(gb);

        print_cabac("vps_num_units_in_tick", vps->vps_num_units_in_tick);
        print_cabac("vps_time_scale", vps->vps_time_scale);
        print_cabac("vps_poc_proportional_to_timing_flag", vps->vps_poc_proportional_to_timing_flag);

        if (vps->vps_poc_proportional_to_timing_flag) {
            vps->vps_num_ticks_poc_diff_one = get_ue_golomb_long(gb) + 1;
            print_cabac("vps_num_ticks_poc_diff_one_minus1", vps->vps_num_ticks_poc_diff_one);
        }

        vps->vps_num_hrd_parameters = get_ue_golomb_long(gb);

        print_cabac("vps_num_hrd_parameters", vps->vps_num_hrd_parameters);

        if (vps->vps_num_layer_sets >= 1024) {
            av_log(avctx, AV_LOG_ERROR, "vps_num_hrd_parameters out of range: %d\n",
                   vps->vps_num_layer_sets - 1);
            goto err;
        }

        for (i = 0; i < vps->vps_num_hrd_parameters; i++) {
            int common_inf_present = 1;
            vps->hrd_layer_set_idx[i] = get_ue_golomb_long(gb);// hrd_layer_set_idx
            print_cabac("hrd_op_set_idx", vps->hrd_layer_set_idx[i]);     
            if (i) {
                common_inf_present = get_bits1(gb);
                print_cabac("cprms_present_flag", common_inf_present); 
            }
            decode_hrd(gb, avctx, &vps->HrdParam, common_inf_present, vps->vps_max_sub_layers -1);
        }
    }
    vps->vps_extension_flag = get_bits1(gb);
    print_cabac("vps_extension_flag", vps->vps_extension_flag);

    if (get_bits_left(gb) < 0) {
        av_log(avctx, AV_LOG_ERROR,
               "Overread VPS by %d bits\n", -get_bits_left(gb));
        if (ps->vps_list[vps->vps_id])
            goto err;
    }

    if(vps->vps_extension_flag){ // vps_extension_flag
        align_get_bits(gb);
        parse_vps_extension(gb, avctx, vps);
    }

    if (ps->vps_list[vps->vps_id] &&
        !memcmp(ps->vps_list[vps->vps_id]->data, vps_buf->data, vps_buf->size)) {
        av_buffer_unref(&vps_buf);
        av_log(avctx, AV_LOG_DEBUG, "ignore VPS duplicated\n");
    } else {
        remove_vps(ps, vps->vps_id);
        ps->vps_list[vps->vps_id] = vps_buf;
    }

    return 0;

err:
    av_buffer_unref(&vps_buf);
    return AVERROR_INVALIDDATA;
}

static void decode_vui(GetBitContext *gb, AVCodecContext *avctx,
                       int apply_defdispwin, HEVCSPS *sps)
{
    VUI *vui          = &sps->vui;
    GetBitContext backup;
    int sar_present, alt = 0;

    av_log(avctx, AV_LOG_DEBUG, "Decoding VUI\n");

    sar_present = get_bits1(gb);
    print_cabac("aspect_ratio_info_present_flag", sar_present);
    if (sar_present) {
        uint8_t sar_idx = get_bits(gb, 8);
        print_cabac("aspect_ratio_idc", sar_idx);
        if (sar_idx < FF_ARRAY_ELEMS(vui_sar))
            vui->sar = vui_sar[sar_idx];
        else if (sar_idx == 255) {
            vui->sar.num = get_bits(gb, 16);
            vui->sar.den = get_bits(gb, 16);
            print_cabac("sar_width", vui->sar.num);
            print_cabac("sar_height",  vui->sar.den);
        } else
            av_log(avctx, AV_LOG_WARNING,
                   "Unknown SAR index: %u.\n", sar_idx);
    }

    vui->overscan_info_present_flag = get_bits1(gb);
    print_cabac("overscan_info_present_flag", vui->overscan_info_present_flag);
    if (vui->overscan_info_present_flag){
        vui->overscan_appropriate_flag = get_bits1(gb);
        print_cabac("overscan_appropriate_flag", vui->overscan_appropriate_flag);
    }

    vui->video_signal_type_present_flag = get_bits1(gb);
    print_cabac("video_signal_type_present_flag", vui->video_signal_type_present_flag);
    if (vui->video_signal_type_present_flag) {
        vui->video_format                    = get_bits(gb, 3);
        vui->video_full_range_flag           = get_bits1(gb);
        vui->colour_description_present_flag = get_bits1(gb);
        print_cabac("video_format", vui->video_format);
        print_cabac("video_full_range_flag", vui->video_full_range_flag);
        print_cabac("colour_description_present_flag", vui->colour_description_present_flag);
        if (vui->video_full_range_flag && sps->pix_fmt == AV_PIX_FMT_YUV420P)
            sps->pix_fmt = AV_PIX_FMT_YUVJ420P;
        if (vui->colour_description_present_flag) {
            vui->colour_primaries        = 9; get_bits(gb, 8);
            vui->transfer_characteristic = 2; get_bits(gb, 8);
            vui->matrix_coeffs           = 2; get_bits(gb, 8);
            print_cabac("colour_primaries", vui->colour_primaries);
            print_cabac("transfer_characteristics", vui->transfer_characteristic);
            print_cabac("matrix_coefficients", vui->matrix_coeffs);

            // Set invalid values to "unspecified"
            if (vui->colour_primaries >= AVCOL_PRI_NB)
                vui->colour_primaries = AVCOL_PRI_UNSPECIFIED;
            if (vui->transfer_characteristic >= AVCOL_TRC_NB)
                vui->transfer_characteristic = AVCOL_TRC_UNSPECIFIED;
            if (vui->matrix_coeffs >= AVCOL_SPC_NB)
                vui->matrix_coeffs = AVCOL_SPC_UNSPECIFIED;
            if (vui->matrix_coeffs == AVCOL_SPC_RGB) {
                switch (sps->pix_fmt) {
                case AV_PIX_FMT_YUV444P:
                    sps->pix_fmt = AV_PIX_FMT_GBRP;
                    break;
                case AV_PIX_FMT_YUV444P10:
                    sps->pix_fmt = AV_PIX_FMT_GBRP10;
                    break;
                case AV_PIX_FMT_YUV444P12:
                    sps->pix_fmt = AV_PIX_FMT_GBRP12;
                    break;
                }
            }
        }
    }

    vui->chroma_loc_info_present_flag = get_bits1(gb);
    print_cabac("chroma_loc_info_present_flag", vui->chroma_loc_info_present_flag);
    if (vui->chroma_loc_info_present_flag) {
        vui->chroma_sample_loc_type_top_field    = get_ue_golomb_long(gb);
        vui->chroma_sample_loc_type_bottom_field = get_ue_golomb_long(gb);
        print_cabac("chroma_sample_loc_type_top_field", vui->chroma_sample_loc_type_top_field);
        print_cabac("chroma_sample_loc_type_bottom_field", vui->chroma_sample_loc_type_bottom_field);
    }

    vui->neutra_chroma_indication_flag = get_bits1(gb);
    vui->field_seq_flag                = get_bits1(gb);
    vui->frame_field_info_present_flag = get_bits1(gb);

    if (get_bits_left(gb) >= 68 && show_bits_long(gb, 21) == 0x100000) {
        vui->default_display_window_flag = 0;
        av_log(avctx, AV_LOG_WARNING, "Invalid default display window\n");
    } else
        vui->default_display_window_flag = get_bits1(gb);
    // Backup context in case an alternate header is detected
    memcpy(&backup, gb, sizeof(backup));

    print_cabac("neutral_chroma_indication_flag", vui->neutra_chroma_indication_flag);
    print_cabac("field_seq_flag", vui->field_seq_flag);
    print_cabac("frame_field_info_present_flag", vui->frame_field_info_present_flag);
    print_cabac("default_display_window_flag", vui->default_display_window_flag);
    if (vui->default_display_window_flag) {
        int vert_mult  = 1 + (sps->chroma_format_idc < 2);
        int horiz_mult = 1 + (sps->chroma_format_idc < 3);
        vui->def_disp_win.left_offset   = get_ue_golomb_long(gb) * horiz_mult;
        vui->def_disp_win.right_offset  = get_ue_golomb_long(gb) * horiz_mult;
        vui->def_disp_win.top_offset    = get_ue_golomb_long(gb) *  vert_mult;
        vui->def_disp_win.bottom_offset = get_ue_golomb_long(gb) *  vert_mult;
        print_cabac("def_disp_win_left_offset", vui->def_disp_win.left_offset);
        print_cabac("def_disp_win_right_offset", vui->def_disp_win.right_offset);
        print_cabac("def_disp_win_top_offset", vui->def_disp_win.top_offset);
        print_cabac("def_disp_win_bottom_offset", vui->def_disp_win.bottom_offset);

        if (apply_defdispwin &&
            avctx->flags2 & AV_CODEC_FLAG2_IGNORE_CROP) {
            av_log(avctx, AV_LOG_DEBUG,
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
    }

    vui->vui_timing_info_present_flag = get_bits1(gb);
    print_cabac("vui_timing_info_present_flag", vui->vui_timing_info_present_flag);

    if (vui->vui_timing_info_present_flag) {
        if( get_bits_left(gb) < 66) {
            // The alternate syntax seem to have timing info located
            // at where def_disp_win is normally located
            av_log(avctx, AV_LOG_WARNING,
                   "Strange VUI timing information, retrying...\n");
            vui->default_display_window_flag = 0;
            memset(&vui->def_disp_win, 0, sizeof(vui->def_disp_win));
            memcpy(gb, &backup, sizeof(backup));
            alt = 1;
        }
        vui->vui_num_units_in_tick               = get_bits_long(gb, 32);
        vui->vui_time_scale                      = get_bits_long(gb, 32);
        if (alt) {
            av_log(avctx, AV_LOG_INFO, "Retry got %i/%i fps\n",
                   vui->vui_time_scale, vui->vui_num_units_in_tick);
        }
        vui->vui_poc_proportional_to_timing_flag = get_bits1(gb);

        print_cabac("vui_num_units_in_tick", vui->vui_num_units_in_tick);
        print_cabac("vui_time_scale", vui->vui_time_scale);
        print_cabac("vui_poc_proportional_to_timing_flag", vui->vui_poc_proportional_to_timing_flag);

        if (vui->vui_poc_proportional_to_timing_flag) {
            vui->vui_num_ticks_poc_diff_one_minus1 = get_ue_golomb_long(gb);
            print_cabac("vui_num_ticks_poc_diff_one_minus1", vui->vui_num_ticks_poc_diff_one_minus1);
        }
        vui->vui_hrd_parameters_present_flag = get_bits1(gb);
        print_cabac("hrd_parameters_present_flag", vui->vui_hrd_parameters_present_flag);
        if (vui->vui_hrd_parameters_present_flag)
            decode_hrd(gb, avctx, &sps->HrdParam, vui->vui_hrd_parameters_present_flag, sps->sps_max_sub_layers -1 );
    }

    vui->bitstream_restriction_flag = get_bits1(gb);
    print_cabac("bitstream_restriction_flag", vui->bitstream_restriction_flag);
    if (vui->bitstream_restriction_flag) {
        vui->tiles_fixed_structure_flag              = get_bits1(gb);
        vui->motion_vectors_over_pic_boundaries_flag = get_bits1(gb);
        vui->restricted_ref_pic_lists_flag           = get_bits1(gb);
        vui->min_spatial_segmentation_idc            = get_ue_golomb_long(gb);
        vui->max_bytes_per_pic_denom                 = get_ue_golomb_long(gb);
        vui->max_bits_per_min_cu_denom               = get_ue_golomb_long(gb);
        vui->log2_max_mv_length_horizontal           = get_ue_golomb_long(gb);
        vui->log2_max_mv_length_vertical             = get_ue_golomb_long(gb);

        print_cabac("tiles_fixed_structure_flag", vui->tiles_fixed_structure_flag);
        print_cabac("motion_vectors_over_pic_boundaries_flag", vui->motion_vectors_over_pic_boundaries_flag );
        print_cabac("restricted_ref_pic_lists_flag", vui->restricted_ref_pic_lists_flag);
        print_cabac("min_spatial_segmentation_idc", vui->min_spatial_segmentation_idc);
        print_cabac("max_bytes_per_pic_denom", vui->max_bytes_per_pic_denom );
        print_cabac("max_bits_per_mincu_denom", vui->max_bits_per_min_cu_denom );
        print_cabac("log2_max_mv_length_horizontal", vui->log2_max_mv_length_horizontal );
        print_cabac("log2_max_mv_length_vertical",  vui->log2_max_mv_length_vertical);

    }
}

static void set_default_scaling_list_data(ScalingList *sl)
{
    int matrixId;

    for (matrixId = 0; matrixId < 6; matrixId++) {
        // 4x4 default is 16
        memset(sl->sl[0][matrixId], 16, 16);
        sl->sl_dc[0][matrixId] = 16; // default for 16x16
        sl->sl_dc[1][matrixId] = 16; // default for 32x32
    }
    memcpy(sl->sl[1][0], default_scaling_list_intra, 64);
    memcpy(sl->sl[1][1], default_scaling_list_intra, 64);
    memcpy(sl->sl[1][2], default_scaling_list_intra, 64);
    memcpy(sl->sl[1][3], default_scaling_list_inter, 64);
    memcpy(sl->sl[1][4], default_scaling_list_inter, 64);
    memcpy(sl->sl[1][5], default_scaling_list_inter, 64);
    memcpy(sl->sl[2][0], default_scaling_list_intra, 64);
    memcpy(sl->sl[2][1], default_scaling_list_intra, 64);
    memcpy(sl->sl[2][2], default_scaling_list_intra, 64);
    memcpy(sl->sl[2][3], default_scaling_list_inter, 64);
    memcpy(sl->sl[2][4], default_scaling_list_inter, 64);
    memcpy(sl->sl[2][5], default_scaling_list_inter, 64);
    memcpy(sl->sl[3][0], default_scaling_list_intra, 64);
    memcpy(sl->sl[3][1], default_scaling_list_intra, 64);
    memcpy(sl->sl[3][2], default_scaling_list_intra, 64);
    memcpy(sl->sl[3][3], default_scaling_list_inter, 64);
    memcpy(sl->sl[3][4], default_scaling_list_inter, 64);
    memcpy(sl->sl[3][5], default_scaling_list_inter, 64);
}

static int scaling_list_data(GetBitContext *gb, AVCodecContext *avctx, ScalingList *sl, HEVCSPS *sps)
{
    uint8_t scaling_list_pred_mode_flag;
    int32_t scaling_list_dc_coef[2][6];
    int size_id, matrix_id, pos;
    int i;

    for (size_id = 0; size_id < 4; size_id++)
        for (matrix_id = 0; matrix_id < 6; matrix_id += ((size_id == 3) ? 3 : 1)) {
            scaling_list_pred_mode_flag = get_bits1(gb);
            if (!scaling_list_pred_mode_flag) {
                unsigned int delta = get_ue_golomb_long(gb);
                /* Only need to handle non-zero delta. Zero means default,
                 * which should already be in the arrays. */
                if (delta) {
                    // Copy from previous array.
                    if (matrix_id < delta) {
                        av_log(avctx, AV_LOG_ERROR,
                               "Invalid delta in scaling list data: %d.\n", delta);
                        return AVERROR_INVALIDDATA;
                    }

                    memcpy(sl->sl[size_id][matrix_id],
                           sl->sl[size_id][matrix_id - delta],
                           size_id > 0 ? 64 : 16);
                    if (size_id > 1)
                        sl->sl_dc[size_id - 2][matrix_id] = sl->sl_dc[size_id - 2][matrix_id - delta];
                }
            } else {
                int next_coef, coef_num;
                int32_t scaling_list_delta_coef;

                next_coef = 8;
                coef_num  = FFMIN(64, 1 << (4 + (size_id << 1)));
                if (size_id > 1) {
                    scaling_list_dc_coef[size_id - 2][matrix_id] = get_se_golomb(gb) + 8;
                    next_coef = scaling_list_dc_coef[size_id - 2][matrix_id];
                    sl->sl_dc[size_id - 2][matrix_id] = next_coef;
                }
                for (i = 0; i < coef_num; i++) {
                    if (size_id == 0)
                        pos = 4 * ff_hevc_diag_scan4x4_y[i] +
                                  ff_hevc_diag_scan4x4_x[i];
                    else
                        pos = 8 * ff_hevc_diag_scan8x8_y[i] +
                                  ff_hevc_diag_scan8x8_x[i];

                    scaling_list_delta_coef = get_se_golomb(gb);
                    next_coef = (next_coef + scaling_list_delta_coef + 256) % 256;
                    sl->sl[size_id][matrix_id][pos] = next_coef;
                }
            }
        }

    if (sps && sps->chroma_format_idc == 3) {
        for (i = 0; i < 64; i++) {
            sl->sl[3][1][i] = sl->sl[2][1][i];
            sl->sl[3][2][i] = sl->sl[2][2][i];
            sl->sl[3][4][i] = sl->sl[2][4][i];
            sl->sl[3][5][i] = sl->sl[2][5][i];
        }
        sl->sl_dc[1][1] = sl->sl_dc[0][1];
        sl->sl_dc[1][2] = sl->sl_dc[0][2];
        sl->sl_dc[1][4] = sl->sl_dc[0][4];
        sl->sl_dc[1][5] = sl->sl_dc[0][5];
    }


    return 0;
}


static int sps_range_extensions(GetBitContext *gb, AVCodecContext *avctx, HEVCSPS *sps)
{
    sps->transform_skip_rotation_enabled_flag    = get_bits1(gb);
    sps->transform_skip_context_enabled_flag     = get_bits1(gb);
    sps->implicit_rdpcm_enabled_flag             = get_bits1(gb);
    sps->explicit_rdpcm_enabled_flag             = get_bits1(gb);
    sps->extended_precision_processing_flag      = get_bits1(gb);
    sps->intra_smoothing_disabled_flag           = get_bits1(gb);
    sps->high_precision_offsets_enabled_flag     = get_bits1(gb);
    sps->persistent_rice_adaptation_enabled_flag = get_bits1(gb);
    sps->cabac_bypass_alignment_enabled_flag     = get_bits1(gb);

    print_cabac("transform_skip_rotation_enabled_flag ", sps->transform_skip_rotation_enabled_flag);
    print_cabac("transform_skip_context_enabled_flag ", sps->transform_skip_context_enabled_flag);
    print_cabac("implicit_rdpcm_enabled_flag ", sps->implicit_rdpcm_enabled_flag);
    print_cabac("explicit_rdpcm_enabled_flag ", sps->explicit_rdpcm_enabled_flag);
    print_cabac("extended_precision_processing_flag ", sps->extended_precision_processing_flag);
    print_cabac("intra_smoothing_disabled_flag ", sps->intra_smoothing_disabled_flag);
    print_cabac("high_precision_offsets_enabled_flag ", sps->high_precision_offsets_enabled_flag);
    print_cabac("persistent_rice_adaptation_enabled_flag ", sps->persistent_rice_adaptation_enabled_flag);
    print_cabac("cabac_bypass_alignment_enabled_flag ", sps->cabac_bypass_alignment_enabled_flag);

    if (sps->extended_precision_processing_flag)
        av_log(avctx, AV_LOG_WARNING,
              "extended_precision_processing_flag not yet implemented\n");

    if (sps->high_precision_offsets_enabled_flag)
        av_log(avctx, AV_LOG_WARNING,
               "high_precision_offsets_enabled_flag not yet implemented\n");

    if (sps->cabac_bypass_alignment_enabled_flag)
        av_log(avctx, AV_LOG_WARNING,
               "cabac_bypass_alignment_enabled_flag not yet implemented\n");
    return 0;
}

//FIXME: this functions only reads one element and returns nothing
static int sps_multilayer_extensions(GetBitContext *gb, AVCodecContext *avctx, HEVCSPS *sps)
{
    uint8_t inter_view_mv_vert_constraint_flag = get_bits1(gb);
    print_cabac("inter_view_mv_vert_constraint_flag",  inter_view_mv_vert_constraint_flag);
    return 0;
}

int ff_hevc_parse_sps(HEVCSPS *sps, GetBitContext *gb, unsigned int *sps_id,
                      int apply_defdispwin, AVBufferRef **vps_list, AVCodecContext *avctx,
                      int nuh_layer_id)
{
    const AVPixFmtDescriptor *desc;
    HEVCVPS *vps;
    int ret    = 0;
    int start;
    int i;
    av_log(NULL, AV_LOG_INFO, "Parsing SPS : size:%d %d \n", gb->size_in_bits >> 3, gb->buffer_end - gb->buffer);
    print_cabac(" \n --- parse sps --- \n ", nuh_layer_id);

    //FIXME: We should not need to init those since sps use allocz
    sps->v1_compatible = 1;
    sps->chroma_format_idc = 1; //FIXME shouldn't it be passing from BL

    av_log(avctx, AV_LOG_DEBUG, "Decoding SPS\n");

    sps->vps_id = get_bits(gb, 4);

    print_cabac("sps_video_parameter_set_id", sps->vps_id); 

    if (sps->vps_id >= MAX_VPS_COUNT) {
        av_log(avctx, AV_LOG_ERROR, "VPS id out of range: %d\n", sps->vps_id);
        return AVERROR_INVALIDDATA;
    }

    if (vps_list && !vps_list[sps->vps_id]) {
        av_log(avctx, AV_LOG_ERROR, "VPS %d does not exist\n",
               sps->vps_id);
        //return AVERROR_INVALIDDATA;
    }
    if((HEVCVPS *) vps_list[sps->vps_id])
        vps = (HEVCVPS *) vps_list[sps->vps_id]->data;
    
    if (!nuh_layer_id) {
        sps->sps_max_sub_layers = get_bits(gb, 3) + 1;
        print_cabac("sps_max_sub_layers_minus1", sps->sps_max_sub_layers - 1);
        if (sps->sps_max_sub_layers > MAX_SUB_LAYERS) {
            av_log(avctx, AV_LOG_ERROR, "sps_max_sub_layers out of range: %d\n",
                   sps->sps_max_sub_layers);
            return AVERROR_INVALIDDATA;
        }
    } else {
        sps->sps_ext_or_max_sub_layers = get_bits(gb, 3) + 1;
        print_cabac("sps_ext_or_max_sub_layers_minus1", sps->sps_ext_or_max_sub_layers - 1);
        sps->v1_compatible = sps->sps_ext_or_max_sub_layers - 1;
        //FIXME This should be done outside of SPS decoding in case the VPS is
        //not available yet
        if ( vps && (sps->sps_ext_or_max_sub_layers - 1) == 7 )
            sps->sps_max_sub_layers = vps->vps_max_sub_layers;
         else
            sps->sps_max_sub_layers = sps->sps_ext_or_max_sub_layers;
    }

    sps->is_multi_layer_ext_sps = ( nuh_layer_id != 0 && sps->v1_compatible == 7 );

    if(!sps->is_multi_layer_ext_sps) {
        sps->sps_temporal_id_nesting_flag = get_bits1(gb);
        print_cabac("sps_temporal_id_nesting_flag", sps->sps_temporal_id_nesting_flag);
        if ((ret = parse_ptl(gb, avctx, &sps->ptl, sps->sps_max_sub_layers, 1)) < 0)
            return ret;
    } else { // Not sure for this
        if (vps && sps->sps_max_sub_layers > 1)
            //FIXME This should be done outside of SPS decoding in case the VPS is
            //not available yet
            sps->sps_temporal_id_nesting_flag = vps->vps_temporal_id_nesting_flag;
        else
            sps->sps_temporal_id_nesting_flag =1;
    }

    sps->sps_id = *sps_id = get_ue_golomb_long(gb);

    av_log(NULL, AV_LOG_INFO, "Parsing SPS : id:%d \n", sps->sps_id);

    print_cabac("sps_seq_parameter_set_id", sps_id);

    if (*sps_id >= MAX_SPS_COUNT) {
        av_log(avctx, AV_LOG_ERROR, "SPS id out of range: %d\n", *sps_id);
        return AVERROR_INVALIDDATA;
    }

     if(sps->is_multi_layer_ext_sps) {
         sps->update_rep_format_flag = get_bits1(gb);
         print_cabac("update_rep_format_flag", sps->update_rep_format_flag);
         if(sps->update_rep_format_flag) {
             sps->sps_rep_format_idx = get_bits(gb, 8);
              print_cabac("sps_rep_format_idx", sps->sps_rep_format_idx);
         }
     } else
         sps->update_rep_format_flag = 0;

    if(!sps->is_multi_layer_ext_sps) {

        sps->chroma_format_idc = get_ue_golomb_long(gb);

        print_cabac("chroma_format_idc", sps->chroma_format_idc);
        if (!(sps->chroma_format_idc == 0 || sps->chroma_format_idc == 1 || sps->chroma_format_idc == 2 || sps->chroma_format_idc == 3)) {
            avpriv_report_missing_feature(avctx, "chroma_format_idc != {0, 1, 2, 3}\n");
            ret = AVERROR_PATCHWELCOME;
            //goto err;
        }

        if(sps->chroma_format_idc == 3) {
            sps->separate_colour_plane_flag = get_bits1(gb);
            print_cabac("separate_colour_plane_flag", sps->separate_colour_plane_flag);
        }

        // FIXME why? (we should use a chroma_array_type variable elsewhere instead
        // of overwriting chroma_format_idc)

        if (sps->separate_colour_plane_flag)
            sps->chroma_format_idc = 0;

        sps->width  = get_ue_golomb_long(gb);
        sps->height = get_ue_golomb_long(gb);
        print_cabac("pic_width_in_luma_samples", sps->width);
        print_cabac("pic_height_in_luma_samples", sps->height);
        if ((ret = av_image_check_size(sps->width,
                                       sps->height, 0, avctx)) < 0)
            return ret;

        sps->conformance_window_flag = get_bits1(gb);

        print_cabac("conformance_window_flag", sps->conformance_window_flag);

        if (sps->conformance_window_flag) { // pic_conformance_flag

            int vert_mult  = 1 + (sps->chroma_format_idc < 2);
            int horiz_mult = 1 + (sps->chroma_format_idc < 3);

            sps->conf_win.left_offset   = get_ue_golomb_long(gb) * horiz_mult;
            sps->conf_win.right_offset  = get_ue_golomb_long(gb) * horiz_mult;
            sps->conf_win.top_offset    = get_ue_golomb_long(gb) *  vert_mult;
            sps->conf_win.bottom_offset = get_ue_golomb_long(gb) *  vert_mult;

            print_cabac("conf_win_left_offset",   sps->conf_win.left_offset);
            print_cabac("conf_win_right_offset",  sps->conf_win.right_offset);
            print_cabac("conf_win_top_offset",    sps->conf_win.top_offset );
            print_cabac("conf_win_bottom_offset", sps->conf_win.bottom_offset);

            if (avctx->flags2 & AV_CODEC_FLAG2_IGNORE_CROP) {
                av_log(avctx, AV_LOG_DEBUG,
                       "discarding sps conformance window, "
                       "original values are l:%u r:%u t:%u b:%u\n",
                       sps->conf_win.left_offset,
                       sps->conf_win.right_offset,
                       sps->conf_win.top_offset,
                       sps->conf_win.bottom_offset);

                sps->conf_win.left_offset   =
                sps->conf_win.right_offset  =
                sps->conf_win.top_offset    =
                sps->conf_win.bottom_offset = 0;
            }
            sps->output_window = sps->conf_win;
        }
        sps->bit_depth[CHANNEL_TYPE_LUMA]   = get_ue_golomb_long(gb) + 8;
        sps->bit_depth[CHANNEL_TYPE_CHROMA] = get_ue_golomb_long(gb) + 8;

        print_cabac("bit_depth_luma_minus8",   sps->bit_depth[CHANNEL_TYPE_LUMA]   - 8);
        print_cabac("bit_depth_chroma_minus8", sps->bit_depth[CHANNEL_TYPE_CHROMA] - 8);

        if (sps->chroma_format_idc && sps->bit_depth[CHANNEL_TYPE_LUMA] != sps->bit_depth[CHANNEL_TYPE_CHROMA]) {
            av_log(avctx, AV_LOG_ERROR,
                   "Luma bit depth (%d) is different from chroma bit depth (%d), "
                   "this is unsupported.\n",
                   sps->bit_depth[CHANNEL_TYPE_LUMA], sps->bit_depth[CHANNEL_TYPE_CHROMA]);
            ret = AVERROR_INVALIDDATA;
            //goto err;
        }

        switch (sps->bit_depth[CHANNEL_TYPE_CHROMA]) {
        case 8:
            if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY8;
            if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P;
            if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P;
            if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P;
            break;
        case 9:
            if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY16;
            if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P9;
            if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P9;
            if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P9;
            break;
        case 10:
            if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY16;
            if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P10;
            if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P10;
            if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P10;
            break;
        case 12:
            if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY16;
            if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P12;
            if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P12;
            if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P12;
            break;
        case 14:
            if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P14;
            if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P14;
            if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P14;
            break;
        default:
            av_log(avctx, AV_LOG_ERROR,
                   "4:2:0, 4:2:2, 4:4:4 supports are currently specified for 8, 10, 12 and 14 bits.\n");
            return AVERROR_PATCHWELCOME;
        }
    } else if(vps) {
        RepFormat Rep;
        if (vps && sps->update_rep_format_flag)
            Rep = vps->vps_ext.rep_format[sps->sps_rep_format_idx];
        else {
            if (vps && (vps->vps_ext.vps_num_rep_formats_minus1+1) > 1)
                Rep = vps->vps_ext.rep_format[vps->vps_ext.vps_rep_format_idx[nuh_layer_id]];
            else
                Rep = vps->vps_ext.rep_format[0];
        }
        sps->width  = Rep.pic_width_vps_in_luma_samples;
        sps->height = Rep.pic_height_vps_in_luma_samples;
        sps->bit_depth[CHANNEL_TYPE_LUMA]   = Rep.bit_depth_vps[CHANNEL_TYPE_LUMA];
        sps->bit_depth[CHANNEL_TYPE_CHROMA] = Rep.bit_depth_vps[CHANNEL_TYPE_CHROMA];
        sps->chroma_format_idc = Rep.chroma_format_vps_idc;

        if(Rep.chroma_format_vps_idc) {
            switch (Rep.bit_depth_vps[CHANNEL_TYPE_LUMA]) {
            case 8:  sps->pix_fmt = AV_PIX_FMT_YUV420P;   break;
            case 9:  sps->pix_fmt = AV_PIX_FMT_YUV420P9;  break;
            case 10: sps->pix_fmt = AV_PIX_FMT_YUV420P10; break;
            default:
                av_log(avctx, AV_LOG_ERROR, "-- Unsupported bit depth: %d\n",
                		sps->bit_depth[CHANNEL_TYPE_LUMA]);
                ret = AVERROR_PATCHWELCOME;
                //goto err;
            }
        } else {
            av_log(avctx, AV_LOG_ERROR,
                    "non-4:2:0 support is currently unspecified.\n");
            return AVERROR_PATCHWELCOME;
        }
    }

    desc = av_pix_fmt_desc_get(sps->pix_fmt);
    if (!desc) {
        ret = AVERROR(EINVAL);
        //goto err;
    }
    sps->hshift[0] = sps->vshift[0] = 0;
    sps->hshift[2] = sps->hshift[1] = desc->log2_chroma_w;
    sps->vshift[2] = sps->vshift[1] = desc->log2_chroma_h;
    sps->pixel_shift[CHANNEL_TYPE_LUMA] = sps->bit_depth[CHANNEL_TYPE_LUMA] > 8;
    sps->pixel_shift[CHANNEL_TYPE_CHROMA] = sps->bit_depth[CHANNEL_TYPE_CHROMA] > 8;
/*end map_pix_format*/
    sps->log2_max_poc_lsb = get_ue_golomb_long(gb) + 4;
    print_cabac("log2_max_pic_order_cnt_lsb_minus4", sps->log2_max_poc_lsb-4);
    if (sps->log2_max_poc_lsb > 16) {
        av_log(avctx, AV_LOG_ERROR, "log2_max_pic_order_cnt_lsb_minus4 out range: %d\n",
               sps->log2_max_poc_lsb - 4);
        ret = AVERROR_INVALIDDATA;
        //goto err;
    }

    if(!sps->is_multi_layer_ext_sps) {

        sps->sps_sub_layer_ordering_info_present_flag = get_bits1(gb);

        print_cabac("sublayer_ordering_info", sps->sps_sub_layer_ordering_info_present_flag);

        start = sps->sps_sub_layer_ordering_info_present_flag ? 0 : sps->sps_max_sub_layers - 1;
        for (i = start; i < sps->sps_max_sub_layers; i++) {
            sps->temporal_layer[i].max_dec_pic_buffering = get_ue_golomb_long(gb) + 1;
            sps->temporal_layer[i].num_reorder_pics      = get_ue_golomb_long(gb);
            sps->temporal_layer[i].max_latency_increase  = get_ue_golomb_long(gb) - 1;

            print_cabac("sps_max_dec_pic_buffering_minus1", sps->temporal_layer[i].max_dec_pic_buffering - 1);
            print_cabac("sps_num_reorder_pics", sps->temporal_layer[i].num_reorder_pics);
            print_cabac("sps_max_latency_increase_plus1", sps->temporal_layer[i].max_latency_increase + 1);

            if (sps->temporal_layer[i].max_dec_pic_buffering > MAX_DPB_SIZE) {
                av_log(avctx, AV_LOG_ERROR, "sps_max_dec_pic_buffering_minus1 out of range: %d\n",
                       sps->temporal_layer[i].max_dec_pic_buffering - 1);
                return AVERROR_INVALIDDATA;
                //goto err;
            }
            
            if (sps->temporal_layer[i].num_reorder_pics > sps->temporal_layer[i].max_dec_pic_buffering - 1) {
                av_log(avctx, AV_LOG_WARNING, "sps_max_num_reorder_pics out of range: %d\n",
                       sps->temporal_layer[i].num_reorder_pics);
                if (avctx->err_recognition & AV_EF_EXPLODE ||
                    sps->temporal_layer[i].num_reorder_pics > MAX_DPB_SIZE - 1) {
                    return AVERROR_INVALIDDATA;
                }
                sps->temporal_layer[i].max_dec_pic_buffering = sps->temporal_layer[i].num_reorder_pics + 1;
            }
            if (!sps->sps_sub_layer_ordering_info_present_flag) {
                for (start; i < start; i++) {
                    sps->temporal_layer[i].max_dec_pic_buffering = sps->temporal_layer[start].max_dec_pic_buffering;
                    sps->temporal_layer[i].num_reorder_pics      = sps->temporal_layer[start].num_reorder_pics;
                    sps->temporal_layer[i].max_latency_increase  = sps->temporal_layer[start].max_latency_increase;
                }
                break;
            }
        }
    }
    sps->log2_min_cb_size                    = get_ue_golomb_long(gb) + 3;
    sps->log2_diff_max_min_cb_size           = get_ue_golomb_long(gb);
    sps->log2_min_tb_size                    = get_ue_golomb_long(gb) + 2;
    sps->log2_diff_max_min_tb_size           = get_ue_golomb_long(gb);

    sps->log2_max_trafo_size                 = sps->log2_diff_max_min_tb_size +
                                               sps->log2_min_tb_size;

    print_cabac("log2_min_coding_block_size_minus3",      sps->log2_min_cb_size-3);
    print_cabac("log2_diff_max_min_coding_block_size",    sps->log2_diff_max_min_cb_size);
    print_cabac("log2_min_transform_block_size_minus2",   sps->log2_min_tb_size  -2);
    print_cabac("log2_diff_max_min_transform_block_size", sps->log2_diff_max_min_tb_size);

    if (sps->log2_min_cb_size < 3 || sps->log2_min_cb_size > 30) {
        av_log(avctx, AV_LOG_ERROR, "Invalid value %d for log2_min_cb_size", sps->log2_min_cb_size);
        return AVERROR_INVALIDDATA;
    }

    if (sps->log2_diff_max_min_cb_size > 30) {
        av_log(avctx, AV_LOG_ERROR, "Invalid value %d for log2_diff_max_min_coding_block_size", sps->log2_diff_max_min_cb_size);
        return AVERROR_INVALIDDATA;
    }

    if (sps->log2_min_tb_size >= sps->log2_min_cb_size || sps->log2_min_tb_size < 2) {
        av_log(avctx, AV_LOG_ERROR, "Invalid value for log2_min_tb_size");
        return AVERROR_INVALIDDATA;
    }

    if (sps->log2_diff_max_min_tb_size < 0 || sps->log2_diff_max_min_tb_size > 30) {
        av_log(avctx, AV_LOG_ERROR, "Invalid value %d for log2_diff_max_min_transform_block_size", sps->log2_diff_max_min_tb_size);
        return AVERROR_INVALIDDATA;
    }

    sps->max_transform_hierarchy_depth_inter = get_ue_golomb_long(gb);
    sps->max_transform_hierarchy_depth_intra = get_ue_golomb_long(gb);

    sps->scaling_list_enabled_flag = get_bits1(gb);

    print_cabac("max_transform_hierarchy_depth_inter", sps->max_transform_hierarchy_depth_inter);
    print_cabac("max_transform_hierarchy_depth_intra", sps->max_transform_hierarchy_depth_intra);
    print_cabac("scaling_list_enabled_flag", sps->scaling_list_enabled_flag);

    if (sps->scaling_list_enabled_flag) {
        if (sps->is_multi_layer_ext_sps) {
            sps->sps_infer_scaling_list_flag =  get_bits1(gb);
            print_cabac("sps_infer_scaling_list_flag", sps->sps_infer_scaling_list_flag);
        }
        if (sps->sps_infer_scaling_list_flag) {
            sps->sps_scaling_list_ref_layer_id = get_bits(gb, 6);
            print_cabac("sps_scaling_list_ref_layer_id", sps->sps_scaling_list_ref_layer_id);
            sps->scaling_list_enabled_flag = 0;
        } else {
            set_default_scaling_list_data(&sps->scaling_list);
            sps->sps_scaling_list_data_present_flag = get_bits1(gb);
            print_cabac("sps_scaling_list_data_present_flag", sps->sps_scaling_list_data_present_flag);
            if (sps->sps_scaling_list_data_present_flag) {
                ret = scaling_list_data(gb, avctx, &sps->scaling_list, sps);
                if (ret < 0)
                    return ret;
            }
        }
    }

    sps->amp_enabled_flag = get_bits1(gb);
    sps->sao_enabled_flag = get_bits1(gb);
    sps->pcm_enabled_flag = get_bits1(gb);

    print_cabac("amp_enabled_flag", sps->amp_enabled_flag);
    print_cabac("sample_adaptive_offset_enabled_flag", sps->sao_enabled_flag);
    print_cabac("pcm_enabled_flag", sps->pcm_enabled_flag);

    if (sps->sao_enabled_flag)
        av_log(avctx, AV_LOG_DEBUG, "SAO enabled\n");

    if (sps->pcm_enabled_flag) {
        sps->pcm.bit_depth        = get_bits(gb, 4) + 1;
        sps->pcm.bit_depth_chroma = get_bits(gb, 4) + 1;
        sps->pcm.log2_min_pcm_cb_size = get_ue_golomb_long(gb) + 3;
        sps->pcm.log2_max_pcm_cb_size = sps->pcm.log2_min_pcm_cb_size +
                                        get_ue_golomb_long(gb);

        if (sps->pcm.bit_depth > sps->bit_depth[CHANNEL_TYPE_LUMA]) {
            av_log(avctx, AV_LOG_ERROR,
                   "PCM bit depth (%d) is greater than normal bit depth (%d)\n",
                   sps->pcm.bit_depth, sps->bit_depth[CHANNEL_TYPE_LUMA]);
            return AVERROR_INVALIDDATA;
        }
        sps->pcm.loop_filter_disable_flag = get_bits1(gb);

        print_cabac("pcm_sample_bit_depth_luma_minus1",   sps->pcm.bit_depth-1);
        print_cabac("pcm_sample_bit_depth_chroma_minus1", sps->pcm.bit_depth_chroma-1);
        print_cabac("log2_min_pcm_luma_coding_block_size_minus3",   sps->pcm.log2_min_pcm_cb_size-3);
        print_cabac("log2_diff_max_min_pcm_luma_coding_block_size", sps->pcm.log2_max_pcm_cb_size-sps->pcm.log2_min_pcm_cb_size);
        print_cabac("pcm_loop_filter_disable_flag", sps->pcm.loop_filter_disable_flag);
    }

    sps->num_short_term_rps = get_ue_golomb_long(gb);

    print_cabac("num_short_term_ref_pic_sets", sps->num_short_term_rps);

    if (sps->num_short_term_rps > MAX_SHORT_TERM_RPS_COUNT) {
        av_log(avctx, AV_LOG_ERROR, "Too many short term RPS: %d.\n",
               sps->num_short_term_rps);
        return AVERROR_INVALIDDATA;
    }

    for (i = 0; i < sps->num_short_term_rps; i++) {
        if ((ret = ff_hevc_decode_short_term_rps(gb, avctx, &sps->st_rps[i],
                                                 sps, 0)) < 0)
            return ret;
    }

    sps->long_term_ref_pics_present_flag = get_bits1(gb);

    print_cabac("long_term_ref_pics_present_flag", sps->long_term_ref_pics_present_flag);

    if (sps->long_term_ref_pics_present_flag) {
        sps->num_long_term_ref_pics_sps = get_ue_golomb_long(gb);

        print_cabac("long_term_ref_pics_present_flag", sps->num_long_term_ref_pics_sps);

        if (sps->num_long_term_ref_pics_sps > 31U) {
            av_log(avctx, AV_LOG_ERROR, "num_long_term_ref_pics_sps %d is out of range.\n",
                   sps->num_long_term_ref_pics_sps);
            return AVERROR_INVALIDDATA;
        }
        for (i = 0; i < sps->num_long_term_ref_pics_sps; i++) {
            sps->lt_ref_pic_poc_lsb_sps[i]       = get_bits(gb, sps->log2_max_poc_lsb);
            sps->used_by_curr_pic_lt_sps_flag[i] = get_bits1(gb);

            print_cabac("lt_ref_pic_poc_lsb_sps", sps->lt_ref_pic_poc_lsb_sps[i]);
            print_cabac("used_by_curr_pic_lt_sps_flag", sps->used_by_curr_pic_lt_sps_flag[i]);
        }
    }

    if(nuh_layer_id > 0)
        sps->set_mfm_enabled_flag = 1;
    else
        sps->set_mfm_enabled_flag = 0;

    sps->sps_temporal_mvp_enabled_flag          = get_bits1(gb);
    sps->sps_strong_intra_smoothing_enable_flag = get_bits1(gb);

    //FIXME: why here? We don't even know if we have some vui yet.
    sps->vui.sar = (AVRational){0, 1};

    sps->vui_parameters_present_flag = get_bits1(gb);

    print_cabac("vui_parameters_present_flag", sps->vui_parameters_present_flag);
    print_cabac("sps_temporal_mvp_enable_flag", sps->sps_temporal_mvp_enabled_flag);
    print_cabac("sps_strong_intra_smoothing_enable_flag", sps->sps_strong_intra_smoothing_enable_flag);

    if (sps->vui_parameters_present_flag)
        decode_vui(gb, avctx, apply_defdispwin, sps);

#if COM16_C806_EMT
    sps->use_intra_emt = get_bits1(gb);
    print_cabac(" use_intra_emt ",sps->use_intra_emt);
    sps->use_inter_emt = get_bits1(gb);
    print_cabac(" use_inter_emt ",sps->use_inter_emt);
#endif

    sps->sps_extension_present_flag = get_bits1(gb);
    print_cabac("sps_extension_flag", sps->sps_extension_present_flag);
    if (sps->sps_extension_present_flag) {

        sps->sps_range_extension_flag      = get_bits1(gb);
        sps->sps_multilayer_extension_flag = get_bits1(gb);
        sps->sps_3d_extension_flag         = get_bits1(gb);
        sps->sps_extension_5bits = get_bits(gb, 5);


        print_cabac("sps_range_extension_flag",      sps->sps_range_extension_flag);
        print_cabac("sps_multilayer_extension_flag", sps->sps_multilayer_extension_flag);
        print_cabac("sps_3d__extension_flag",        sps->sps_3d_extension_flag );
        print_cabac("sps_extension_6bits",           sps->sps_extension_5bits);

        if (sps->sps_range_extension_flag)
            sps_range_extensions(gb, avctx, sps);

        if (sps->sps_multilayer_extension_flag)
            sps_multilayer_extensions(gb, avctx, sps);

    } else {
        // Read more RSB data 
    }

    //TODO Clean this part
    if (apply_defdispwin) {
        sps->output_window.left_offset   += sps->vui.def_disp_win.left_offset;
        sps->output_window.right_offset  += sps->vui.def_disp_win.right_offset;
        sps->output_window.top_offset    += sps->vui.def_disp_win.top_offset;
        sps->output_window.bottom_offset += sps->vui.def_disp_win.bottom_offset;
    }
    if (sps->output_window.left_offset & (0x1F >> (sps->pixel_shift[CHANNEL_TYPE_LUMA])) &&
        !(avctx->flags & AV_CODEC_FLAG_UNALIGNED)) {
        sps->output_window.left_offset &= ~(0x1F >> (sps->pixel_shift[CHANNEL_TYPE_LUMA]));
        av_log(avctx, AV_LOG_WARNING, "Reducing left output window to %d "
               "chroma samples to preserve alignment.\n",
               sps->output_window.left_offset);
    }
    sps->output_width  = sps->width -
                         (sps->output_window.left_offset + sps->output_window.right_offset);
    sps->output_height = sps->height -
                         (sps->output_window.top_offset + sps->output_window.bottom_offset);
    if (sps->width  <= sps->output_window.left_offset + (int64_t)sps->output_window.right_offset  ||
        sps->height <= sps->output_window.top_offset  + (int64_t)sps->output_window.bottom_offset) {
        av_log(avctx, AV_LOG_WARNING, "Invalid visible frame dimensions: %dx%d.\n",
               sps->output_width, sps->output_height);
        if (avctx->err_recognition & AV_EF_EXPLODE) {
            return AVERROR_INVALIDDATA;
        }
        av_log(avctx, AV_LOG_WARNING,
               "Displaying the whole video surface.\n");
        memset(&sps->conf_win, 0, sizeof(sps->conf_win));
        memset(&sps->output_window, 0, sizeof(sps->output_window));
        sps->output_width               = sps->width;
        sps->output_height              = sps->height;
    }

    // Inferred parameters
    sps->log2_ctb_size = sps->log2_min_cb_size +
                         sps->log2_diff_max_min_cb_size;
    sps->log2_min_pu_size = sps->log2_min_cb_size - 1;

    if (sps->log2_ctb_size > MAX_LOG2_CTB_SIZE) {
        av_log(avctx, AV_LOG_ERROR, "CTB size out of range: 2^%d\n", sps->log2_ctb_size);
        return AVERROR_INVALIDDATA;
    }
    if (sps->log2_ctb_size < 4) {
        av_log(avctx,
               AV_LOG_ERROR,
               "log2_ctb_size %d differs from the bounds of any known profile\n",
               sps->log2_ctb_size);
        avpriv_request_sample(avctx, "log2_ctb_size %d", sps->log2_ctb_size);
        return AVERROR_INVALIDDATA;
    }

    sps->ctb_width  = (sps->width  + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
    sps->ctb_height = (sps->height + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
    sps->ctb_size   = sps->ctb_width * sps->ctb_height;

    sps->min_cb_width  = sps->width  >> sps->log2_min_cb_size;
    sps->min_cb_height = sps->height >> sps->log2_min_cb_size;
    sps->min_tb_width  = sps->width  >> sps->log2_min_tb_size;
    sps->min_tb_height = sps->height >> sps->log2_min_tb_size;
    sps->min_pu_width  = sps->width  >> sps->log2_min_pu_size;
    sps->min_pu_height = sps->height >> sps->log2_min_pu_size;
    sps->tb_mask       = (1 << (sps->log2_ctb_size - sps->log2_min_tb_size)) - 1;

    sps->qp_bd_offset = 6 * (sps->bit_depth[CHANNEL_TYPE_LUMA] - 8);

    if (av_mod_uintp2(sps->width,  sps->log2_min_cb_size) ||
        av_mod_uintp2(sps->height, sps->log2_min_cb_size)) {
        av_log(avctx, AV_LOG_ERROR, "Invalid coded frame dimensions.\n");
        return AVERROR_INVALIDDATA;
    }
    if (sps->log2_ctb_size > MAX_LOG2_CTB_SIZE) {
        av_log(avctx, AV_LOG_ERROR, "CTB size out of range: 2^%d\n", sps->log2_ctb_size);
        return AVERROR_INVALIDDATA;
        //goto err;
    }
    if (sps->max_transform_hierarchy_depth_inter > sps->log2_ctb_size - sps->log2_min_tb_size) {
        av_log(avctx, AV_LOG_ERROR, "max_transform_hierarchy_depth_inter out of range: %d\n",
               sps->max_transform_hierarchy_depth_inter);
        return AVERROR_INVALIDDATA;
    }
    if (sps->max_transform_hierarchy_depth_intra > sps->log2_ctb_size - sps->log2_min_tb_size) {
        av_log(avctx, AV_LOG_ERROR, "max_transform_hierarchy_depth_intra out of range: %d\n",
               sps->max_transform_hierarchy_depth_intra);
        return AVERROR_INVALIDDATA;
    }
    if (sps->log2_max_trafo_size > FFMIN(sps->log2_ctb_size, 5)) {
        av_log(avctx, AV_LOG_ERROR,
               "max transform block size out of range: %d\n",
               sps->log2_max_trafo_size);
        return AVERROR_INVALIDDATA;
    }
    if (get_bits_left(gb) < 0) {
        av_log(avctx, AV_LOG_ERROR,
               "Overread SPS by %d bits\n", -get_bits_left(gb));
        return AVERROR_INVALIDDATA;
    }
    return 0;
}

int ff_hevc_decode_nal_sps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps, int apply_defdispwin, int nuh_layer_id)
{
    unsigned int sps_id;
    int ret;
    HEVCSPS     *sps;
    AVBufferRef *sps_buf;

    sps_buf = av_buffer_allocz(sizeof(*sps));

    if (!sps_buf)
        return AVERROR(ENOMEM);

    sps = (HEVCSPS*)sps_buf->data;

    av_log(avctx, AV_LOG_DEBUG, "Decoding SPS\n");

    ret = ff_hevc_parse_sps(sps, gb, &sps_id,
                            apply_defdispwin,
                            ps->vps_list, avctx, nuh_layer_id);
    if (ret < 0) {
        av_buffer_unref(&sps_buf);
        return ret;
    }

    if (avctx->debug & FF_DEBUG_BITSTREAM) {
        av_log(avctx, AV_LOG_DEBUG,
               "Parsed SPS: id %d; coded wxh: %dx%d; "
               "cropped wxh: %dx%d; pix_fmt: %s.\n",
               sps_id, sps->width, sps->height,
               sps->output_width, sps->output_height,
               av_get_pix_fmt_name(sps->pix_fmt));
    }

    /* check if this is a repeat of an already parsed SPS, then keep the
     * original one.
     * otherwise drop all PPSes that depend on it */
    if (ps->sps_list[sps_id] && !memcmp(ps->sps_list[sps_id]->data, sps_buf->data, sps_buf->size)){
        av_buffer_unref(&sps_buf);
    } else {
        remove_sps(ps, sps_id);
        ps->sps_list[sps_id] = sps_buf;
    }
    return 0;
}

static void hevc_pps_free(void *opaque, uint8_t *data)
{
    HEVCPPS *pps = (HEVCPPS*)data;

    av_freep(&pps->column_width);
    av_freep(&pps->row_height);
    av_freep(&pps->col_bd);
    av_freep(&pps->row_bd);
    av_freep(&pps->col_idxX);
    av_freep(&pps->ctb_addr_rs_to_ts);
    av_freep(&pps->ctb_addr_ts_to_rs);
    av_freep(&pps->tile_pos_rs);
    av_freep(&pps->tile_id);
    av_freep(&pps->wpp_pos_ts);
    av_freep(&pps->tile_width);
    av_freep(&pps->min_tb_addr_zs_tab);
    av_freep(&pps);
}

static int pps_range_extensions(GetBitContext *gb, AVCodecContext *avctx,
                                HEVCPPS *pps) {
    int i;

    if (pps->transform_skip_enabled_flag) {
        pps->log2_max_transform_skip_block_size = get_ue_golomb_long(gb) + 2;
        print_cabac("log2_max_transform_skip_block_size", pps->log2_max_transform_skip_block_size);
        if (pps->log2_max_transform_skip_block_size > 2) {
            av_log(avctx, AV_LOG_ERROR,
                   "log2_max_transform_skip_block_size_minus2 is partially implemented.\n");
        }
    }
    pps->cross_component_prediction_enabled_flag = get_bits1(gb);
    pps->chroma_qp_offset_list_enabled_flag = get_bits1(gb);

    print_cabac("cross_component_prediction_enabled_flag", pps->cross_component_prediction_enabled_flag);
    print_cabac("chroma_qp_offset_list_enabled_flag", pps->chroma_qp_offset_list_enabled_flag);

    if (pps->chroma_qp_offset_list_enabled_flag) {
        pps->diff_cu_chroma_qp_offset_depth = get_ue_golomb_long(gb);
        pps->chroma_qp_offset_list_len_minus1 = get_ue_golomb_long(gb);

        print_cabac("diff_cu_chroma_qp_offset_depth", pps->diff_cu_chroma_qp_offset_depth);
        print_cabac("chroma_qp_offset_list_len_minus1", pps->chroma_qp_offset_list_len_minus1);

        if (pps->chroma_qp_offset_list_len_minus1 && pps->chroma_qp_offset_list_len_minus1 >= 5) {
            av_log(avctx, AV_LOG_ERROR,
                   "chroma_qp_offset_list_len_minus1 shall be in the range [0, 5].\n");
            return AVERROR_INVALIDDATA;
        }
        for (i = 0; i <= pps->chroma_qp_offset_list_len_minus1; i++) {
            pps->cb_qp_offset_list[i] = get_se_golomb_long(gb);
            pps->cr_qp_offset_list[i] = get_se_golomb_long(gb);

            print_cabac("cb_offset_list[i]", pps->cb_qp_offset_list[i]);
            print_cabac("cr_qp_offset_list[i]", pps->cr_qp_offset_list[i]);

            if (pps->cb_qp_offset_list[i]) {
                av_log(avctx, AV_LOG_WARNING,
                       "cb_qp_offset_list not tested yet.\n");
            }

            if (pps->cr_qp_offset_list[i]) {
                av_log(avctx, AV_LOG_WARNING,
                       "cb_qp_offset_list not tested yet.\n");
            }
        }
    }

    pps->log2_sao_offset_scale_luma   = get_ue_golomb_long(gb);
    pps->log2_sao_offset_scale_chroma = get_ue_golomb_long(gb);

    print_cabac("sao_luma_bit_shift", pps->log2_sao_offset_scale_luma);
    print_cabac("lsao_chroma_bit_shift", pps->log2_sao_offset_scale_chroma);

    return 0;
}

int setup_pps(AVCodecContext *avctx,
                            HEVCPPS *pps, HEVCSPS *sps)
{
    int log2_diff;
    int pic_area_in_ctbs;
    int i, j, x, y, ctb_addr_rs, tile_id;
if(!pps->is_setup && sps){
    // Inferred parameters
    pps->col_bd   = av_malloc_array(pps->num_tile_columns + 1, sizeof(*pps->col_bd));
    pps->row_bd   = av_malloc_array(pps->num_tile_rows    + 1, sizeof(*pps->row_bd));
    pps->col_idxX = av_malloc_array(sps->ctb_width,            sizeof(*pps->col_idxX));

    if (!pps->col_bd || !pps->row_bd || !pps->col_idxX)
        return AVERROR(ENOMEM);

    if (pps->uniform_spacing_flag) {

        if (!pps->column_width) {
            pps->column_width = av_malloc_array(pps->num_tile_columns, sizeof(*pps->column_width));
            pps->row_height   = av_malloc_array(pps->num_tile_rows,    sizeof(*pps->row_height));
        }

        if (!pps->column_width || !pps->row_height)
            return AVERROR(ENOMEM);

        for (i = 0; i < pps->num_tile_columns; i++) {
            pps->column_width[i] = ((i + 1) * sps->ctb_width) / pps->num_tile_columns -
                                   ( i * sps->ctb_width)      / pps->num_tile_columns;
        }

        for (i = 0; i < pps->num_tile_rows; i++) {
            pps->row_height[i] = ((i + 1) * sps->ctb_height) / pps->num_tile_rows -
                                 (i * sps->ctb_height)       / pps->num_tile_rows;
        }
    }

    pps->col_bd[0] = 0;

    for (i = 0; i < pps->num_tile_columns; i++)
        pps->col_bd[i + 1] = pps->col_bd[i] + pps->column_width[i];

    pps->row_bd[0] = 0;

    for (i = 0; i < pps->num_tile_rows; i++)
        pps->row_bd[i + 1] = pps->row_bd[i] + pps->row_height[i];

    for (i = 0, j = 0; i < sps->ctb_width; i++) {
        if (i > pps->col_bd[j])
            j++;
        pps->col_idxX[i] = j;
    }

    /**
     * 6.5
     */
    //FIXME: we might not have any sps yet
    pic_area_in_ctbs     = sps->ctb_width    * sps->ctb_height;

    pps->ctb_addr_rs_to_ts  = av_malloc_array(pic_area_in_ctbs, sizeof(*pps->ctb_addr_rs_to_ts));
    pps->ctb_addr_ts_to_rs  = av_malloc_array(pic_area_in_ctbs, sizeof(*pps->ctb_addr_ts_to_rs));
    pps->tile_id            = av_malloc_array(pic_area_in_ctbs, sizeof(*pps->tile_id));
    pps->wpp_pos_ts         = av_malloc_array(pic_area_in_ctbs, sizeof(*pps->wpp_pos_ts));
    pps->min_tb_addr_zs_tab = av_malloc_array((sps->tb_mask+2) * (sps->tb_mask+2), sizeof(*pps->min_tb_addr_zs_tab));
    pps->tile_width         = av_malloc_array(pic_area_in_ctbs, sizeof(*pps->tile_width));

    if (!pps->ctb_addr_rs_to_ts || !pps->ctb_addr_ts_to_rs ||
        !pps->tile_id || !pps->min_tb_addr_zs_tab || !pps->tile_width) {
        return AVERROR(ENOMEM);
    }

    for (ctb_addr_rs = 0; ctb_addr_rs < pic_area_in_ctbs; ctb_addr_rs++) {
        int tb_x   = ctb_addr_rs % sps->ctb_width;
        int tb_y   = ctb_addr_rs / sps->ctb_width;
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

        for (i = 0; i < tile_x; i++)
            val += pps->row_height[tile_y] * pps->column_width[i];
        for (i = 0; i < tile_y; i++)
            val += sps->ctb_width * pps->row_height[i];

        val += (tb_y - pps->row_bd[tile_y]) * pps->column_width[tile_x] +
               tb_x - pps->col_bd[tile_x];

        pps->ctb_addr_rs_to_ts[ctb_addr_rs] = val;
        pps->ctb_addr_ts_to_rs[val]         = ctb_addr_rs;
    }

    int row = 0, wpp_pos = 0;
    for (j = 0, tile_id = 0; j < pps->num_tile_rows; j++){
        for (i = 0; i < pps->num_tile_columns; i++, tile_id++){
            for (y = pps->row_bd[j]; y < pps->row_bd[j + 1]; y++){
                for (x = pps->col_bd[i]; x < pps->col_bd[i + 1]; x++) {
                    pps->tile_id[pps->ctb_addr_rs_to_ts[y * sps->ctb_width + x]] = tile_id;
                    pps->tile_width[pps->ctb_addr_rs_to_ts[y * sps->ctb_width + x]] = pps->column_width[tile_id % pps->num_tile_columns];
                }
                pps->wpp_pos_ts[row++] = wpp_pos;
                wpp_pos += pps->column_width[tile_id % pps->num_tile_columns];
            }
        }
    }

    pps->tile_pos_rs = av_malloc_array(tile_id, sizeof(*pps->tile_pos_rs));

    if (!pps->tile_pos_rs)
        return AVERROR(ENOMEM);

    for (j = 0; j < pps->num_tile_rows; j++)
        for (i = 0; i < pps->num_tile_columns; i++)
            pps->tile_pos_rs[j * pps->num_tile_columns + i] =
                pps->row_bd[j] * sps->ctb_width + pps->col_bd[i];

    log2_diff = sps->log2_ctb_size - sps->log2_min_tb_size;
    pps->min_tb_addr_zs = &pps->min_tb_addr_zs_tab[1*(sps->tb_mask+2)+1];
    for (y = 0; y < sps->tb_mask+2; y++) {
        pps->min_tb_addr_zs_tab[y*(sps->tb_mask+2)] = -1;
        pps->min_tb_addr_zs_tab[y]    = -1;
    }
    for (y = 0; y < sps->tb_mask+1; y++) {
        for (x = 0; x < sps->tb_mask+1; x++) {
            int tb_x = x >> log2_diff;
            int tb_y = y >> log2_diff;
            int rs   = sps->ctb_width * tb_y + tb_x;
            int val  = pps->ctb_addr_rs_to_ts[rs] << (log2_diff * 2);
            for (i = 0; i < log2_diff; i++) {
                int m = 1 << i;
                val += (m & x ? m * m : 0) + (m & y ? 2 * m * m : 0);
            }
            pps->min_tb_addr_zs[y * (sps->tb_mask+2) + x] = val;
        }
    }
    pps->is_setup = 1;
}
    return 0;
}

SYUVP  GetCuboidVertexPredAll(TCom3DAsymLUT * pc3DAsymLUT, int yIdx , int uIdx , int vIdx , int nVertexIdx){
  SCuboid***  pCuboid = pc3DAsymLUT->S_Cuboid;

  SYUVP sPred;
  if( yIdx == 0 ) {
    sPred.Y = nVertexIdx == 0 ? 1024 : 0;
    sPred.U = nVertexIdx == 1 ? 1024 : 0;
    sPred.V = nVertexIdx == 2 ? 1024 : 0;
  }
  else
    sPred = pCuboid[yIdx-1][uIdx][vIdx].P[nVertexIdx];
  return sPred;
}

static void setCuboidVertexResTree( TCom3DAsymLUT * pc3DAsymLUT, int yIdx , int uIdx , int vIdx , int nVertexIdx , int deltaY , int deltaU , int deltaV ) {
    SYUVP *sYuvp = &pc3DAsymLUT->S_Cuboid[yIdx][uIdx][vIdx].P[nVertexIdx], sPred;
    sPred  = GetCuboidVertexPredAll( pc3DAsymLUT, yIdx , uIdx , vIdx , nVertexIdx);

    sYuvp->Y = sPred.Y + ( deltaY << pc3DAsymLUT->cm_res_quant_bit );
    sYuvp->U = sPred.U + ( deltaU << pc3DAsymLUT->cm_res_quant_bit );
    sYuvp->V = sPred.V + ( deltaV << pc3DAsymLUT->cm_res_quant_bit );
}

static int ReadParam( GetBitContext   *gb, int rParam ) {
  unsigned int prefix, codeWord, rSymbol, sign;
  int param;
  prefix   = get_ue_golomb_long(gb);
  print_cabac("quotient", prefix);
  codeWord = get_bits(gb, rParam);
  print_cabac("rParam", rParam);
  print_cabac("remainder", codeWord);
  rSymbol = (prefix<<rParam) + codeWord;

  if(rSymbol) {
    sign = get_bits1(gb);
    param = sign ? -(int)(rSymbol) : (int)(rSymbol);
    return param;
  }
  else
    return 0;
}

static void xParse3DAsymLUTOctant( GetBitContext *gb , TCom3DAsymLUT * pc3DAsymLUT , int nDepth , int yIdx , int uIdx , int vIdx , int length) {
    uint8_t split_octant_flag = nDepth < pc3DAsymLUT->cm_octant_depth;
    int l,m,n, nYPartNum;
    if(split_octant_flag){
      split_octant_flag = get_bits1(gb);
      print_cabac("split_octant_flag", split_octant_flag);
    }
    nYPartNum = 1 << pc3DAsymLUT->cm_y_part_num_log2;
    if( split_octant_flag ) {
        int nHalfLength = length >> 1;
        for(l = 0 ; l < 2 ; l++ )
          for(m = 0 ; m < 2 ; m++ )
            for(n = 0 ; n < 2 ; n++ )
              xParse3DAsymLUTOctant( gb, pc3DAsymLUT , nDepth + 1 , yIdx + l * nHalfLength * nYPartNum , uIdx + m * nHalfLength , vIdx + n * nHalfLength , nHalfLength );
    } else {
        int l, m, u, v, y, nVertexIdx, nFLCbits = pc3DAsymLUT->nMappingShift - pc3DAsymLUT->cm_res_quant_bit -pc3DAsymLUT->cm_flc_bits;

        nFLCbits = nFLCbits >= 0 ? nFLCbits:0;
        for(l = 0 ; l < nYPartNum ; l++ ) {
          int shift = pc3DAsymLUT->cm_octant_depth - nDepth;
          for(nVertexIdx = 0 ; nVertexIdx < 4 ; nVertexIdx++ ) {
            uint8_t  coded_vertex_flag = 0;
            int deltaY = 0 , deltaU = 0 , deltaV = 0;
            coded_vertex_flag = get_bits1(gb);
            print_cabac("coded_vertex_flag", coded_vertex_flag);
            if( coded_vertex_flag ) {
                deltaY = ReadParam(gb, nFLCbits );
                deltaU = ReadParam(gb, nFLCbits );
                deltaV = ReadParam(gb, nFLCbits );
            }
            setCuboidVertexResTree( pc3DAsymLUT, yIdx + (l<<shift) , uIdx , vIdx , nVertexIdx , deltaY , deltaU , deltaV );
            for ( m = 1; m < (1<<shift); m++) {
              setCuboidVertexResTree( pc3DAsymLUT, yIdx + (l<<shift) + m , uIdx , vIdx , nVertexIdx , 0 , 0 , 0 );
            }
          }
        }

        for (u=0 ; u<length ; u++ )
              for ( v=0 ; v<length ; v++ )
                if ( u || v )
                  for ( y=0 ; y<length*nYPartNum ; y++ )
                    for( nVertexIdx = 0 ; nVertexIdx < 4 ; nVertexIdx++ )
                      setCuboidVertexResTree( pc3DAsymLUT, yIdx + y , uIdx + u , vIdx + v , nVertexIdx , 0 , 0 , 0 );
    }
}

static void Allocate3DArray(TCom3DAsymLUT * pc3DAsymLUT, int xSize, int ySize, int zSize) {
  int x, y;

  pc3DAsymLUT->S_Cuboid    = av_malloc(xSize*sizeof(SCuboid**)) ;
  pc3DAsymLUT->S_Cuboid[0] = av_malloc(xSize*ySize*sizeof(SCuboid*)) ;
  for( x = 1 ; x < xSize ; x++ )
    pc3DAsymLUT->S_Cuboid[x] = pc3DAsymLUT->S_Cuboid[x-1] + ySize;

  pc3DAsymLUT->S_Cuboid[0][0] = av_mallocz(xSize*ySize*zSize*sizeof(SCuboid));
  for( x = 0 ; x < xSize ; x++ )
    for(y = 0 ; y < ySize ; y++ )
        pc3DAsymLUT->S_Cuboid[x][y] = pc3DAsymLUT->S_Cuboid[0][0] + x * ySize * zSize + y * zSize;
}

/*
static void Display(TCom3DAsymLUT * pc3DAsymLUT, int xSize, int ySize, int zSize) {
  int i, j, k;
  for( i = 0 ; i < xSize ; i++ )
    for(j = 0 ; j < ySize ; j++ )
      for(k = 0 ; k < zSize ; k++ )
        printf("%d %d %d %d - %d %d %d %d - %d %d %d %d \n", pc3DAsymLUT->S_Cuboid[i][j][k].P[0].Y, pc3DAsymLUT->S_Cuboid[i][j][k].P[1].Y, pc3DAsymLUT->S_Cuboid[i][j][k].P[2].Y, pc3DAsymLUT->S_Cuboid[i][j][k].P[3].Y, pc3DAsymLUT->S_Cuboid[i][j][k].P[0].U, pc3DAsymLUT->S_Cuboid[i][j][k].P[1].U, pc3DAsymLUT->S_Cuboid[i][j][k].P[2].U, pc3DAsymLUT->S_Cuboid[i][j][k].P[3].U, pc3DAsymLUT->S_Cuboid[i][j][k].P[0].V, pc3DAsymLUT->S_Cuboid[i][j][k].P[1].V, pc3DAsymLUT->S_Cuboid[i][j][k].P[2].V, pc3DAsymLUT->S_Cuboid[i][j][k].P[3].V);
}*/

void Free3DArray(HEVCPPS * pps) {
  if(pps && 0) {
    av_freep(&pps->pc3DAsymLUT.S_Cuboid[0][0]);
    av_freep(&pps->pc3DAsymLUT.S_Cuboid[0]);
    av_freep(&pps->pc3DAsymLUT.S_Cuboid);
  }
}
static void xParse3DAsymLUT(GetBitContext *gb, TCom3DAsymLUT * pc3DAsymLUT) {
    int i, YSize, CSize;

    pc3DAsymLUT->num_cm_ref_layers_minus1 = get_ue_golomb_long(gb);
    print_cabac("num_cm_ref_layers_minus1", pc3DAsymLUT->num_cm_ref_layers_minus1);

    for(  i = 0 ; i <= pc3DAsymLUT->num_cm_ref_layers_minus1; i++ ) {
        pc3DAsymLUT->uiRefLayerId[i] = get_bits(gb, 6);
        print_cabac("cm_ref_layer_id", pc3DAsymLUT->uiRefLayerId[i]);
    }

    pc3DAsymLUT->cm_octant_depth    = get_bits(gb, 2);
    pc3DAsymLUT->cm_y_part_num_log2 = get_bits(gb, 2);

    pc3DAsymLUT->cm_input_luma_bit_depth      = get_ue_golomb_long(gb) + 8;
    pc3DAsymLUT->cm_input_chroma_bit_depth    = get_ue_golomb_long(gb) + 8;
    pc3DAsymLUT->cm_output_luma_bit_depth     = get_ue_golomb_long(gb) + 8;
    pc3DAsymLUT->cm_output_chroma_bit_depth   = get_ue_golomb_long(gb) + 8;

    pc3DAsymLUT->cm_res_quant_bit = get_bits(gb, 2);
    pc3DAsymLUT->cm_flc_bits      = get_bits(gb, 2) + 1;

    print_cabac("cm_octant_depth", pc3DAsymLUT->cm_octant_depth);
    print_cabac("cm_y_part_num_log2", pc3DAsymLUT->cm_y_part_num_log2);
    print_cabac("cm_input_luma_bit_depth_minus8", pc3DAsymLUT->cm_input_luma_bit_depth - 8);
    print_cabac("cm_input_chroma_bit_depth_minus8", pc3DAsymLUT->cm_input_chroma_bit_depth - 8);
    print_cabac("cm_output_luma_bit_depth_minus8", pc3DAsymLUT->cm_output_luma_bit_depth - 8);
    print_cabac("cm_output_chroma_bit_depth_minus8", pc3DAsymLUT->cm_output_chroma_bit_depth - 8);
    print_cabac("cm_res_quant_bit", pc3DAsymLUT->cm_res_quant_bit);
    print_cabac("cm_flc_bits", pc3DAsymLUT->cm_flc_bits-1);

    pc3DAsymLUT->nAdaptCThresholdU = 1 << ( pc3DAsymLUT->cm_input_chroma_bit_depth - 1 );
    pc3DAsymLUT-> nAdaptCThresholdV = 1 << ( pc3DAsymLUT->cm_input_chroma_bit_depth - 1 );

    if( pc3DAsymLUT->cm_octant_depth == 1 ) {
        pc3DAsymLUT->cm_adapt_threshold_u_delta = get_se_golomb(gb);
        pc3DAsymLUT->cm_adapt_threshold_v_delta = get_se_golomb(gb);

        pc3DAsymLUT->nAdaptCThresholdU += pc3DAsymLUT->cm_adapt_threshold_u_delta;
        pc3DAsymLUT->nAdaptCThresholdV += pc3DAsymLUT->cm_adapt_threshold_v_delta;

        print_cabac("cm_adapt_threshold_u_delta", pc3DAsymLUT->cm_adapt_threshold_u_delta);
        print_cabac("cm_adapt_threshold_v_delta", pc3DAsymLUT->cm_adapt_threshold_v_delta);
    }
    pc3DAsymLUT->delta_bit_depth   = pc3DAsymLUT->cm_output_luma_bit_depth   - pc3DAsymLUT->cm_input_luma_bit_depth;
    pc3DAsymLUT->delta_bit_depth_C = pc3DAsymLUT->cm_output_chroma_bit_depth - pc3DAsymLUT->cm_input_chroma_bit_depth;
    pc3DAsymLUT->max_part_num_log2 = 3*pc3DAsymLUT->cm_octant_depth          + pc3DAsymLUT->cm_y_part_num_log2;

    pc3DAsymLUT->YShift2Idx = pc3DAsymLUT->cm_input_luma_bit_depth - pc3DAsymLUT->cm_octant_depth - pc3DAsymLUT->cm_y_part_num_log2;
    pc3DAsymLUT->UShift2Idx = pc3DAsymLUT->VShift2Idx = pc3DAsymLUT->cm_input_chroma_bit_depth - pc3DAsymLUT->cm_octant_depth;

    pc3DAsymLUT->nMappingShift = 10 + pc3DAsymLUT->cm_input_luma_bit_depth - pc3DAsymLUT->cm_output_luma_bit_depth;

    pc3DAsymLUT->nMappingOffset = 1 << ( pc3DAsymLUT->nMappingShift - 1 );

    YSize = 1 << ( pc3DAsymLUT->cm_octant_depth + pc3DAsymLUT->cm_y_part_num_log2 );
    CSize = 1 << pc3DAsymLUT->cm_octant_depth;

    Allocate3DArray( pc3DAsymLUT , YSize , CSize , CSize );
    xParse3DAsymLUTOctant( gb , pc3DAsymLUT , 0 , 0 ,0 , 0 , 1<<pc3DAsymLUT->cm_octant_depth);
//    Display( pc3DAsymLUT , YSize , CSize , CSize );
}

static int pps_multilayer_extensions(GetBitContext *gb, AVCodecContext *avctx,
                                HEVCPPS *pps) {
    int i;

    pps->poc_reset_info_present_flag = get_bits1(gb);
    pps->pps_infer_scaling_list_flag = get_bits1(gb);

    print_cabac("poc_reset_info_present_flag", pps->poc_reset_info_present_flag);
    print_cabac("pps_infer_scaling_list_flag", pps->pps_infer_scaling_list_flag);

    if( pps->pps_infer_scaling_list_flag ) {
        pps->pps_scaling_list_ref_layer_id = get_bits(gb, 6);
        print_cabac("pps_scaling_list_ref_layer_id", pps->pps_scaling_list_ref_layer_id);
        pps->scaled_ref_layer_offset_present_flag = 0;
    }
    pps->num_ref_loc_offsets = get_ue_golomb_long(gb);
    print_cabac("num_ref_loc_offsets", pps->num_ref_loc_offsets);

    for(i = 0; i < pps->num_ref_loc_offsets; i++) {
        pps->ref_loc_offset_layer_id = get_bits(gb, 6);
        pps->scaled_ref_layer_offset_present_flag = get_bits1(gb);

        print_cabac("ref_loc_offset_layer_id", pps->ref_loc_offset_layer_id);
        print_cabac("scaled_ref_layer_offset_present_flag", pps->scaled_ref_layer_offset_present_flag);

        if (pps->scaled_ref_layer_offset_present_flag) {

            pps->scaled_ref_window[i].left_offset   = (get_se_golomb(gb)<< 1);
            pps->scaled_ref_window[i].top_offset    = (get_se_golomb(gb)<< 1);
            pps->scaled_ref_window[i].right_offset  = (get_se_golomb(gb)<< 1);
            pps->scaled_ref_window[i].bottom_offset = (get_se_golomb(gb)<< 1);

            print_cabac("scaled_ref_layer_left_offset",    pps->scaled_ref_window[i].left_offset );
            print_cabac("scaled_ref_layer_top_offset",     pps->scaled_ref_window[i].top_offset);
            print_cabac("scaled_ref_layer_right_offset",   pps->scaled_ref_window[i].right_offset);
            print_cabac("scaled_ref_layer_bottom_offset",  pps->scaled_ref_window[i].bottom_offset);
        }
        pps->ref_region_offset_present_flag = get_bits1(gb);
        print_cabac("ref_region_offset_present_flag", pps->ref_region_offset_present_flag);
        if (pps->ref_region_offset_present_flag) {
            pps->ref_window[i].left_offset   = (get_se_golomb(gb)<< 1);
            pps->ref_window[i].top_offset    = (get_se_golomb(gb)<< 1);
            pps->ref_window[i].right_offset  = (get_se_golomb(gb)<< 1);
            pps->ref_window[i].bottom_offset = (get_se_golomb(gb)<< 1);

            print_cabac("ref_region_left_offset",    pps->ref_window[i].left_offset );
            print_cabac("ref_region_top_offset",     pps->ref_window[i].top_offset);
            print_cabac("ref_region_right_offset",   pps->ref_window[i].right_offset);
            print_cabac("ref_region_bottom_offset",  pps->ref_window[i].bottom_offset);
        }
        pps->resample_phase_set_present_flag = get_bits1(gb);
        print_cabac("resample_phase_set_present_flag", pps->resample_phase_set_present_flag);
        if (pps->resample_phase_set_present_flag) {
            pps->phase_hor_luma[i] = get_ue_golomb_long(gb);
            pps->phase_ver_luma[i] = get_ue_golomb_long(gb);
            pps->phase_hor_chroma[i] = get_ue_golomb_long(gb) - 8;
            pps->phase_ver_chroma[i] = get_ue_golomb_long(gb) - 8;

            print_cabac("phase_hor_luma", pps->phase_hor_luma[i]);
            print_cabac("phase_ver_luma", pps->phase_ver_luma[i]);
            print_cabac("phase_hor_chroma_plus8", pps->phase_hor_chroma[i] + 8);
            print_cabac("phase_ver_chroma_plus8", pps->phase_ver_chroma[i] + 8);
        }
    }
    pps->colour_mapping_enabled_flag = get_bits1(gb);
    print_cabac("colour_mapping_enabled_flag", pps->colour_mapping_enabled_flag);
    if (pps->colour_mapping_enabled_flag) {
        xParse3DAsymLUT(gb, &pps->pc3DAsymLUT);
        pps->m_nCGSOutputBitDepth[0]  =  pps->pc3DAsymLUT.cm_output_luma_bit_depth;
        pps->m_nCGSOutputBitDepth[1] =   pps->pc3DAsymLUT.cm_output_chroma_bit_depth;
    }
    return 0;
}
/*pps_infer_scaling_list_flag	u(1)
if( pps_infer_scaling_list_flag )
pps_scaling_list_ref_layer_id	u(6)
num_ref_loc_offsets	ue(v)
for( i = 0; i < num_ref_loc_offsets; i++) { [Ed. (JC): can insert a new sub-clause for all the offsets]
    ref_loc_offset_layer_id[ i ]	u(6)
    scaled_ref_layer_offset_present_flag[ i ]	u(1)
    if(scaled_ref_layer_offset_present_flag[ i ]) {
        scaled_ref_layer_left_offset[ ref_loc_offset_layer_id[ i ] ]	se(v)
        scaled_ref_layer_top_offset[ ref_loc_offset_layer_id[ i ] ]	se(v)
        scaled_ref_layer_right_offset[ ref_loc_offset_layer_id[ i ] ]	se(v)
        scaled_ref_layer_bottom_offset[ ref_loc_offset_layer_id[ i ] ]	se(v)
    }
    ref_region_offset_present_flag[ i ]	u(1)
    if(ref_region_offset_present_flag[ i ]) {
        ref_region_left_offset[ ref_loc_offset_layer_id[ i ] ]	se(v)
        ref_region_top_offset[ ref_loc_offset_layer_id[ i ] ]	se(v)
        ref_region_right_offset[ref_loc_offset_layer_id[ i ] ]	se(v)
        ref_region_bottom_offset[ref_loc_offset_layer_id[ i ] ]	se(v)
    }
    resample_phase_set_present_flag[ i ]	u(1)
    if(resample_phase_set_prsent_flag[ i ]) {
        phase_hor_luma[ ref_loc_offset_layer_id[ i ] ]	ue(v)
        phase_ver_luma[ ref_loc_offset_layer_id[ i ] ]	ue(v)
        phase_hor_chroma_plus8[ ref_loc_offset_layer_id[ i ] ]	ue(v)
        phase_ver_chroma_plus8[ ref_loc_offset_layer_id[ i ] ]	ue(v)
    }
}*/

int ff_hevc_decode_nal_pps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps)
{
    AVBufferRef *pps_buf;
    HEVCSPS     *sps = NULL;
    HEVCPPS     *pps = av_mallocz(sizeof(*pps));

    av_log(NULL, AV_LOG_INFO, "Parsing PPS : size:%d %d\n", gb->size_in_bits, gb->buffer_end - gb->buffer);

    int i, ret = 0;
    unsigned int pps_id = 0;

    uint8_t pps_extension_present_flag;

    print_cabac(" --- parse pps --- ", s->nuh_layer_id);

    if (!pps)
        return AVERROR(ENOMEM);

    pps_buf = av_buffer_create((uint8_t *)pps, sizeof(*pps),
                               hevc_pps_free, NULL, 0);
    if (!pps_buf) {
        av_freep(&pps);
        return AVERROR(ENOMEM);
    }

    av_log(avctx, AV_LOG_DEBUG, "Decoding PPS\n");

    // Default values
    pps->loop_filter_across_tiles_enabled_flag = 1;
    pps->num_tile_columns                      = 1;
    pps->num_tile_rows                         = 1;
    pps->uniform_spacing_flag                  = 1;
    pps->pps_deblocking_filter_disabled_flag                           = 0;
    pps->pps_beta_offset                           = 0;
    pps->pps_tc_offset                             = 0;
    pps->log2_max_transform_skip_block_size    = 2;

    // Coded parameters
    pps->pps_id = get_ue_golomb_long(gb);
    av_log(NULL, AV_LOG_INFO, "Parsing PPS : id:%d \n", pps->pps_id);

    print_cabac("pps_pic_parameter_set_id", pps->pps_id);
    if (pps->pps_id >= MAX_PPS_COUNT) {
        av_log(avctx, AV_LOG_ERROR, "PPS id out of range: %d\n", pps->pps_id);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    pps->sps_id = get_ue_golomb_long(gb);
    print_cabac("pps_seq_parameter_set_id", pps->sps_id);
    if (pps->sps_id >= MAX_SPS_COUNT) {
        av_log(avctx, AV_LOG_ERROR, "SPS id out of range: %d\n", pps->sps_id);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    if (!ps->sps_list[pps->sps_id]) {
        av_log(avctx, AV_LOG_ERROR, "SPS %u does not exist.\n", pps->sps_id);
        //ret = AVERROR_INVALIDDATA;
        //goto err;
    }
    if ((HEVCSPS *)ps->sps_list[pps->sps_id])
        sps = (HEVCSPS *)ps->sps_list[pps->sps_id]->data;

    pps->dependent_slice_segments_enabled_flag = get_bits1(gb);
    pps->output_flag_present_flag              = get_bits1(gb);
    pps->num_extra_slice_header_bits           = get_bits(gb, 3);
    pps->sign_data_hiding_flag                 = get_bits1(gb);
    pps->cabac_init_present_flag               = get_bits1(gb);
    pps->num_ref_idx_l0_default_active = get_ue_golomb_long(gb) + 1;
    pps->num_ref_idx_l1_default_active = get_ue_golomb_long(gb) + 1;
    pps->init_qp_minus26           = get_se_golomb(gb);
    pps->constrained_intra_pred_flag = get_bits1(gb);
    pps->transform_skip_enabled_flag = get_bits1(gb);
    pps->cu_qp_delta_enabled_flag    = get_bits1(gb);

    print_cabac("dependent_slice_segments_enabled_flag", pps->dependent_slice_segments_enabled_flag);
    print_cabac("output_flag_present_flag", pps->output_flag_present_flag);
    print_cabac("num_extra_slice_header_bits", pps->num_extra_slice_header_bits);
    print_cabac("sign_data_hiding_flag", pps->sign_data_hiding_flag);
    print_cabac("cabac_init_present_flag", pps->cabac_init_present_flag);
    print_cabac("num_ref_idx_l0_default_active_minus1", pps->num_ref_idx_l0_default_active-1);
    print_cabac("num_ref_idx_l1_default_active_minus1", pps->num_ref_idx_l1_default_active-1);
    print_cabac("init_qp_minus26", pps->init_qp_minus26);
    print_cabac("constrained_intra_pred_flag", pps->constrained_intra_pred_flag);
    print_cabac("transform_skip_enabled_flag", pps->transform_skip_enabled_flag);
    print_cabac("cu_qp_delta_enabled_flag", pps->cu_qp_delta_enabled_flag);

    pps->diff_cu_qp_delta_depth   = 0;
    if (pps->cu_qp_delta_enabled_flag) {
        pps->diff_cu_qp_delta_depth = get_ue_golomb_long(gb);
        print_cabac("diff_cu_qp_delta_depth", pps->diff_cu_qp_delta_depth);
    }   

    if (sps && (pps->diff_cu_qp_delta_depth < 0 ||
        pps->diff_cu_qp_delta_depth > sps->log2_diff_max_min_cb_size)) {
        av_log(avctx, AV_LOG_ERROR, "diff_cu_qp_delta_depth %d is invalid\n",
               pps->diff_cu_qp_delta_depth);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }

    pps->pps_cb_qp_offset = get_se_golomb(gb);
    if (pps->pps_cb_qp_offset < -12 || pps->pps_cb_qp_offset > 12) {
        av_log(avctx, AV_LOG_ERROR, "pps_cb_qp_offset out of range: %d\n",
               pps->pps_cb_qp_offset);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    pps->pps_cr_qp_offset = get_se_golomb(gb);
    if (pps->pps_cr_qp_offset < -12 || pps->pps_cr_qp_offset > 12) {
        av_log(avctx, AV_LOG_ERROR, "pps_cr_qp_offset out of range: %d\n",
               pps->pps_cr_qp_offset);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }
    pps->pps_slice_chroma_qp_offsets_present_flag = get_bits1(gb);
    pps->weighted_pred_flag                             = get_bits1(gb);
    pps->weighted_bipred_flag                           = get_bits1(gb);
    pps->transquant_bypass_enable_flag                  = get_bits1(gb);
    pps->tiles_enabled_flag                             = get_bits1(gb);
    pps->entropy_coding_sync_enabled_flag               = get_bits1(gb);

    print_cabac("pps_cb_qp_offset", pps->pps_cb_qp_offset);
    print_cabac("pps_cr_qp_offset", pps->pps_cr_qp_offset);
    print_cabac("pps_slice_chroma_qp_offsets_present_flag", pps->pps_slice_chroma_qp_offsets_present_flag);
    print_cabac("weighted_pred_flag", pps->weighted_pred_flag);
    print_cabac("weighted_bipred_flag", pps->weighted_bipred_flag);
    print_cabac("transquant_bypass_enable_flag", pps->transquant_bypass_enable_flag);
    print_cabac("tiles_enabled_flag", pps->tiles_enabled_flag);
    print_cabac("entropy_coding_sync_enabled_flag", pps->entropy_coding_sync_enabled_flag);

    if (pps->entropy_coding_sync_enabled_flag)
        av_log(avctx, AV_LOG_DEBUG, "WPP enabled\n");

    if (pps->tiles_enabled_flag) {
        av_log(avctx, AV_LOG_DEBUG, "Tiles enabled\n");
        pps->num_tile_columns = get_ue_golomb_long(gb) + 1;
        pps->num_tile_rows    = get_ue_golomb_long(gb) + 1;

        print_cabac("num_tile_columns_minus1", pps->num_tile_columns - 1);
        print_cabac("num_tile_rows_minus1", pps->num_tile_rows - 1);

        if (sps && pps->num_tile_columns <= 0 ||
            pps->num_tile_columns >= sps->width) {
            av_log(avctx, AV_LOG_ERROR, "num_tile_columns_minus1 out of range: %d\n",
                   pps->num_tile_columns - 1);
            ret = AVERROR_INVALIDDATA;
            goto err;
        }
        if (sps && pps->num_tile_rows <= 0 ||
            pps->num_tile_rows >= sps->height) {
            av_log(avctx, AV_LOG_ERROR, "num_tile_rows_minus1 out of range: %d\n",
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
        print_cabac("uniform_spacing_flag", pps->uniform_spacing_flag);

        if (!pps->uniform_spacing_flag) {
            uint64_t sum = 0;
            for (i = 0; i < pps->num_tile_columns - 1; i++) {
                pps->column_width[i] = get_ue_golomb_long(gb) + 1;
                sum                 += pps->column_width[i];
                print_cabac("column_width_minus1", pps->column_width[i]-1);

            }
            if (sps && sum >= sps->ctb_width) {
                av_log(avctx, AV_LOG_ERROR, "Invalid tile widths.\n");
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
            if(sps)
                pps->column_width[pps->num_tile_columns - 1] = sps->ctb_width - sum;

            sum = 0;
            for (i = 0; i < pps->num_tile_rows - 1; i++) {
                pps->row_height[i] = get_ue_golomb_long(gb) + 1;
                print_cabac("row_height_minus1", pps->row_height[i]-1);
                sum               += pps->row_height[i];
            }
            if (sps && sum >= sps->ctb_height) {
                av_log(avctx, AV_LOG_ERROR, "Invalid tile heights.\n");
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
            if(sps)
                pps->row_height[pps->num_tile_rows - 1] = sps->ctb_height - sum;
        }
        pps->loop_filter_across_tiles_enabled_flag = get_bits1(gb);
        print_cabac("loop_filter_across_tiles_enabled_flag",  pps->loop_filter_across_tiles_enabled_flag);
    }
    pps->pps_loop_filter_across_slices_enabled_flag = get_bits1(gb);
    pps->deblocking_filter_control_present_flag     = get_bits1(gb);

    print_cabac("loop_filter_across_slices_enabled_flag", pps->pps_loop_filter_across_slices_enabled_flag);
    print_cabac("deblocking_filter_control_present_flag", pps->deblocking_filter_control_present_flag);

    if (pps->deblocking_filter_control_present_flag) {
        pps->deblocking_filter_override_enabled_flag = get_bits1(gb);
        pps->pps_deblocking_filter_disabled_flag                             = get_bits1(gb);

        print_cabac("deblocking_filter_override_enabled_flag", pps->deblocking_filter_override_enabled_flag);
        print_cabac("pps_disable_deblocking_filter_flag",  pps->pps_deblocking_filter_disabled_flag);

        if (!pps->pps_deblocking_filter_disabled_flag) {
            pps->pps_beta_offset = get_se_golomb(gb) * 2;
            pps->pps_tc_offset   = get_se_golomb(gb) * 2;

            print_cabac("pps_beta_offset_div2", pps->pps_beta_offset);
            print_cabac("pps_tc_offset_div2", pps->pps_tc_offset);

            if (pps->pps_beta_offset/2 < -6 || pps->pps_beta_offset/2 > 6) {
                av_log(avctx, AV_LOG_ERROR, "pps_beta_offset_div2 out of range: %d\n",
                       pps->pps_beta_offset/2);
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
            if (pps->pps_tc_offset/2 < -6 || pps->pps_tc_offset/2 > 6) {
                av_log(avctx, AV_LOG_ERROR, "pps_tc_offset_div2 out of range: %d\n",
                       pps->pps_tc_offset/2);
                ret = AVERROR_INVALIDDATA;
                goto err;
            }
        }
    }
    pps->pps_scaling_list_data_present_flag = get_bits1(gb);

    if (pps->pps_scaling_list_data_present_flag) {
        set_default_scaling_list_data(&pps->scaling_list);
        ret = scaling_list_data(gb, avctx, &pps->scaling_list, sps);
        if (ret < 0)
            goto err;
    }
    pps->lists_modification_present_flag = get_bits1(gb);
    pps->log2_parallel_merge_level       = get_ue_golomb_long(gb) + 2;

    print_cabac("lists_modification_present_flag", pps->lists_modification_present_flag);
    print_cabac("log2_parallel_merge_level_minus2", pps->log2_parallel_merge_level-2 );

    if (sps && pps->log2_parallel_merge_level > sps->log2_ctb_size) {
        av_log(avctx, AV_LOG_ERROR, "log2_parallel_merge_level_minus2 out of range: %d\n",
               pps->log2_parallel_merge_level - 2);
        ret = AVERROR_INVALIDDATA;
        goto err;
    }

    pps->slice_segment_header_extension_present_flag = get_bits1(gb);
    pps_extension_present_flag = get_bits1(gb);

    print_cabac("slice_segment_header_extension_present_flag", pps->slice_segment_header_extension_present_flag);
    print_cabac("pps_extension_present_flag", pps_extension_present_flag);

        av_log(avctx, AV_LOG_ERROR,
               "Remaining bits PPS before extensions %d bits\n", get_bits_left(gb));

    if (pps_extension_present_flag) { // pps_extension_present_flag
        int pps_range_extensions_flag     = get_bits1(gb);
        int pps_multilayer_extension_flag = get_bits1(gb);
        int pps_extension_6bits           = get_bits(gb, 6); // For next versions

        print_cabac("pps_range_extensions_flag",     pps_range_extensions_flag);
        print_cabac("pps_multilayer_extension_flag", pps_multilayer_extension_flag);
        print_cabac("pps_extension_6bits",           pps_extension_6bits);

        if (sps && sps->ptl.general_ptl.profile_idc == FF_PROFILE_HEVC_REXT && pps_range_extensions_flag) {
            av_log(avctx, AV_LOG_ERROR,
                   "PPS extension flag is partially implemented.\n");
            if ((ret = pps_range_extensions(gb, avctx, pps)) < 0)
                goto err;
        }
        if (pps_multilayer_extension_flag) {
            av_log(avctx, AV_LOG_ERROR,
                   "hack with the spec.\n"
                   "use pps_range_extensions_flag instead of pps_multilayer_extension_flag\n."
                   "PPS multilayer extension flag is partially implemented.\n");
            pps_multilayer_extensions(gb, avctx, pps);
        }
    }

    ret = setup_pps(avctx, pps, sps);
    if (ret < 0)
        goto err;

    if (get_bits_left(gb) < 0) {
        av_log(avctx, AV_LOG_ERROR,
               "Overread PPS by %d bits\n", -get_bits_left(gb));
      /*  goto err; */ /* TODO with  EXT_A_ericsson_4.bit */
    }

    remove_pps(ps, pps->pps_id);
    ps->pps_list[pps->pps_id] = pps_buf;

    return 0;

err:
    av_buffer_unref(&pps_buf);
    return ret;
}
