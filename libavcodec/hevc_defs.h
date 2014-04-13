/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
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

#ifndef AVCODEC_HEVC_DEF_H
#define AVCODEC_HEVC_DEF_H

#define SVC_EXTENSION   1
#define VPS_EXTN_OFFSET                  1      ///< implementation of vps_extension_offset syntax element

#ifdef SVC_EXTENSION
    #define VPS_EXTENSION   1
    #define VPS_EXTN_MASK_AND_DIM_INFO 1
    #define SCALED_REF_LAYER_OFFSETS   1
    #define MAX_LAYERS  2
    #define PHASE_DERIVATION_IN_INTEGER 1
    #define ILP_DECODED_PICTURE 1
    #define CHROMA_UPSAMPLING   1
    #define REF_IDX_FRAMEWORK   1
    #define MAX_VPS_OP_LAYER_SETS_PLUS1       3
    #ifdef REF_IDX_FRAMEWORK
        #define REF_IDX_ME_ZEROMV                1
        #define REF_IDX_MFM                      1
        #define JCTVC_M0458_INTERLAYER_RPS_SIG   1
        #if JCTVC_M0458_INTERLAYER_RPS_SIG
            #define ZERO_NUM_DIRECT_LAYERS       1
        #endif
    #endif
#endif
#define O0062_POC_LSB_NOT_PRESENT_FLAG   1      ///< JCTVC-O0062: signal poc_lsb_not_present_flag for each layer in VPS extension
#define SPS_EXTENSION                    1      ///< Define sps_extension() syntax structure
#if SPS_EXTENSION
#define O0142_CONDITIONAL_SPS_EXTENSION  1      ///< JCTVC-O0142: Conditional SPS extension
#endif

#define O0098_SCALED_REF_LAYER_ID        1       ///< JCTVC-O0098: signal scaled reference id
#define SCALINGLIST_INFERRING            1       ///< JCTVC-N0371: inter-layer scaling list
#define O0215_PHASE_ALIGNMENT            1       ///< JCTVC_O0215: signal a flag to specify phase alignment case, 0: zero-position-aligned, 1: central-position-aligned,
#define MAX_CPB_CNT                     32       ///< Upper bound of (cpb_cnt_minus1 + 1)
#define MAX_NUM_LAYER_IDS                64
#define POC_RESET_FLAG                   1      ///< JCTVC-N0244: POC reset flag for  layer pictures.
#define O0149_CROSS_LAYER_BLA_FLAG       1      ///< JCTVC-O0149: signal cross_layer_bla_flag in slice header

#define COEF_REMAIN_BIN_REDUCTION        3 ///< indicates the level at which the VLC
#define FRAME_CONCEALMENT                0
#define SIM_ERROR_CONCEALMENT            0

#if FRAME_CONCEALMENT
#define COPY_MV                              1
#endif

#ifdef VPS_EXTENSION
    #define MAX_TLAYER                  8           ///< max number of temporal layer
    #define HB_LAMBDA_FOR_LDC           1           ///< use of B-style lambda for non-key pictures in low-delay mode

    #define MAX_VPS_NUM_SCALABILITY_TYPES       16
    #define MAX_VPS_LAYER_ID_PLUS1              MAX_LAYERS
    #define MAX_VPS_LAYER_SETS_PLUS1            1024
    #define VPS_EXTN_MASK_AND_DIM_INFO          1
    #define VPS_MOVE_DIR_DEPENDENCY_FLAG        1
    #define VPS_EXTN_DIRECT_REF_LAYERS          1
    #define M0457_PREDICTION_INDICATIONS     1
    #define O0096_DEFAULT_DEPENDENCY_TYPE    1      ///< JCTVC-O0096: specify default dependency type for all direct reference layers
    #define O0092_0094_DEPENDENCY_CONSTRAINT 1      ///< JCTVC-O0092: constraint on the layer_id of SPS/PPS
    #if O0092_0094_DEPENDENCY_CONSTRAINT
        #define MAX_REF_LAYERS                   7
    #endif
    #define VPS_EXTN_PROFILE_INFO               1
    #define VPS_PROFILE_OUTPUT_LAYERS           1
    #define VPS_EXTN_OP_LAYER_SETS              1
    #define VIEW_ID_RELATED_SIGNALING           1
    #define VPS_DPB_SIZE_TABLE               1      ///< JCTVC-O0217: DPB operations: signaling DPB-related parameters
    #define O0153_ALT_OUTPUT_LAYER_FLAG      1      ///< JCTVC-O0153: alt output layer flag
    #define REPN_FORMAT_IN_VPS               1      ///< JCTVC-N0092: Signal represenation format (spatial resolution, bit depth, colour format) in the VPS
    #define AUXILIARY_PICTURES               1      ///< JCTVC-O0041: auxiliary picture layers

    #define O0096_REP_FORMAT_INDEX           1      ///< JCTVC-O0096: identify SPS rep_format() with an index into the lists of formats in VPS extension.
    #if REPN_FORMAT_IN_VPS
        #define REPN_FORMAT_CONTROL_FLAG         1      ///< JCTVC-O0179: Add control flag in representation format to control sending of chroma and bitdepth parameters
    #endif

    #define M0040_ADAPTIVE_RESOLUTION_CHANGE 1
    #define VPS_VUI                          1      ///< Include function structure for VPS VUI
    #if M0040_ADAPTIVE_RESOLUTION_CHANGE
        #define HIGHER_LAYER_IRAP_SKIP_FLAG      1      ///< JCTVC-O0199: Indication that higher layer IRAP picture uses skip blocks only
    #endif
    #define VPS_VUI_BITRATE_PICRATE          1      ///< JCTVC-N0085: Signal bit rate and picture in VPS VUI
#endif

#if VPS_VUI
#define VPS_VUI_TILES_NOT_IN_USE__FLAG    1      ///< JCTVC-O0226: VPS VUI flag to indicate tile not in use
#define VPS_VUI_WPP_NOT_IN_USE__FLAG    1      ///< JCTVC-O0226: VPS VUI flag to indicate tile not in use
#define TILE_BOUNDARY_ALIGNED_FLAG       1      ///< JCTVC-N0160/JCTVC-N0199 proposal 2 variant 2: VPS VUI flag to indicate tile boundary alignment
#define N0160_VUI_EXT_ILP_REF            1      ///< VUI extension inter-layer dependency offset signalling
#define VPS_VUI_BITRATE_PICRATE          1      ///< JCTVC-N0085: Signal bit rate and picture in VPS VUI
#if M0040_ADAPTIVE_RESOLUTION_CHANGE
#define HIGHER_LAYER_IRAP_SKIP_FLAG      1      ///< JCTVC-O0199: Indication that higher layer IRAP picture uses skip blocks only
#endif
#define VPS_VUI_VIDEO_SIGNAL             1      ///< JCTVC-O0118 video signal information
#endif //VPS_VUI

//#endif
#define JCTVC_M0203_INTERLAYER_PRED_IDC  1      ///< JCTVC-M0203: implementation of Inter-layer Prediction Indication
#if JCTVC_M0203_INTERLAYER_PRED_IDC
#define EARLY_REF_PIC_MARKING            1      ///< Decoded picture marking of sub-layer non-reference pictures
#define N0120_MAX_TID_REF_PRESENT_FLAG   1      ///< JCTVC-N0120: max_tid_ref_pics_plus1_present_flag
#define N0120_MAX_TID_REF_CFG            1      ///< set max_tid_il_ref_pics_plus1 and max_tid_ref_present_flag in the config. file (configuration setting)
#define O0225_MAX_TID_FOR_REF_LAYERS     1
#define O0225_TID_BASED_IL_RPS_DERIV     1

#define VPS_TSLAYERS                     1      ///< JCTVC-O0120 signal max temporal sub-layers for each layer
#define TSLAYERS_IL_RPS                  1      ///< JCTVC-O0120 IL RPS based on max temporal sub-layers
#endif
#define ILP_SSH_SIG                      1      ///< JCTVC-N0195 proposal 2, JCTVC-N0118: add presence flag in VPS ext to condition inter-layer prediction signaling in slice segment header

#if !VPS_EXTN_DIRECT_REF_LAYERS || !M0457_PREDICTION_INDICATIONS || !JCTVC_M0458_INTERLAYER_RPS_SIG
#define M0457_IL_SAMPLE_PRED_ONLY_FLAG   0      ///< shall be 0, JCTVC-N0107
#else
#define M0457_IL_SAMPLE_PRED_ONLY_FLAG   0      ///< shall be 0, JCTVC-N0107
#endif

#define O0223_PICTURE_TYPES_ALIGN_FLAG   1  ///< a flag to indicatate whether picture types are aligned across layers.

#define N0147_IRAP_ALIGN_FLAG            1      ///< a flag to indicatate whether IRAPs are aligned across layers
#if N0147_IRAP_ALIGN_FLAG
#define O0223_O0139_IRAP_ALIGN_NO_CONTRAINTS  1  ///< Remove IRAP align depedency constraints on poc_Reset_flag.
#define IRAP_ALIGN_FLAG_IN_VPS_VUI       1       ///< Move IRAP align flag to VPS VUI
#endif
#if !N0147_IRAP_ALIGN_FLAG
#define IDR_ALIGNMENT                    1      ///< align IDR picures across layers : As per JCTVC-N0373, IDR are not required to be aligned.
#endif
#define FAST_INTRA_SHVC                  1      ///< JCTVC-M0115: reduction number of intra modes in the EL (encoder only)
#if FAST_INTRA_SHVC
#define NB_REMAIN_MODES                  2      ///< JCTVC-M0115: nb of remaining modes
#endif

#define DERIVE_LAYER_ID_LIST_VARIABLES      1

#endif /* AVCODEC_HEVC_DEF_H */
