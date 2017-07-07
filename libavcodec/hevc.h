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

#ifndef AVCODEC_HEVC_H
#define AVCODEC_HEVC_H

#include "libavutil/buffer.h"
#include "libavutil/md5.h"

#include "avcodec.h"
#include "bswapdsp.h"
#include "cabac.h"
#include "get_bits.h"
#include "hevcpred.h"
#include "h2645_parse.h"
#include "hevcdsp.h"
#include "internal.h"
#include "thread.h"
#include "videodsp.h"
#include "hevc_defs.h"
#if HEVC_ENCRYPTION
#include "crypto.h"
#endif

#if COM16_C806_EMT
#define EMT_INTRA_MAX_CU		32
#define EMT_INTER_MAX_CU		32
#define EMT_SIGNUM_THR			 2
#define INTER_MODE_IDX		   255
#define MAX_TU_SIZE				32
#define EMT_TRANSFORM_MATRIX_SHIFT 6
#define COM16_C806_TRANS_PREC	 2
#endif

#include "hevcdsp.h"

#define PARALLEL_SLICE   0
#define PARALLEL_FILTERS 0

#define USE_SAO_SMALL_BUFFER /* reduce the memory used by SAO */

#define MAX_SLICES_FRAME 64

#define TEST_MV_POC
#define MAX_DPB_SIZE 16 // A.4.1
#define MAX_REFS 16

#define MAX_NB_THREADS 16
#define SHIFT_CTB_WPP 2
#define MAX_POC      1024
/**
 * 7.4.2.1
 */
#define MAX_SUB_LAYERS 7
#define MAX_VPS_COUNT 16
#define MAX_SPS_COUNT 32
#define MAX_PPS_COUNT 256
#define MAX_SHORT_TERM_RPS_COUNT 64
#define MAX_CU_SIZE 128

//TODO: check if this is really the maximum
#define MAX_TRANSFORM_DEPTH 5
#define MIN_PB_LOG_SIZE 2

#define MAX_TB_SIZE 32
#define MAX_PB_SIZE 64

#define MAX_EDGE_BUFFER_SIZE    ((MAX_PB_SIZE + 20) * (MAX_PB_SIZE+20) * 2)
#define MAX_EDGE_BUFFER_STRIDE  ((MAX_PB_SIZE + 20) * 2)

#define MAX_LOG2_CTB_SIZE 6
#define MAX_QP 51
#define DEFAULT_INTRA_TC_OFFSET 2

#define HEVC_CONTEXTS 199

#define MRG_MAX_NUM_CANDS     5

#define L0 0
#define L1 1

#define EPEL_EXTRA_BEFORE 1
#define EPEL_EXTRA_AFTER  2
#define EPEL_EXTRA        3
#define QPEL_EXTRA_BEFORE 3
#define QPEL_EXTRA_AFTER  4
#define QPEL_EXTRA        7
#define ACTIVE_PU_UPSAMPLING     1
#define ACTIVE_BOTH_FRAME_AND_PU 0

#define EDGE_EMU_BUFFER_STRIDE 80

/**
 * Value of the luma sample at position (x, y) in the 2D array tab.
 */
#define SAMPLE(tab, x, y) ((tab)[(y) * s->sps->width + (x)])
#define SAMPLE_CTB(tab, x, y) ((tab)[(y) * min_cb_width + (x)])

#define IS_IDR(s) ((s)->nal_unit_type == NAL_IDR_W_RADL || (s)->nal_unit_type == NAL_IDR_N_LP)
#define IS_BLA(s) ((s)->nal_unit_type == NAL_BLA_W_RADL || (s)->nal_unit_type == NAL_BLA_W_LP || \
                   (s)->nal_unit_type == NAL_BLA_N_LP)
#define IS_IRAP(s) ((s)->nal_unit_type >= 16 && (s)->nal_unit_type <= 23)

#if COM16_C806_EMT
enum EMT_DCTTransformType {
    DCT_II = 0,
    DCT_III,
    DCT_I,
    DST_I,     // subset 1
    DST_VII,   // subset 0, 1 et 2
    DCT_VIII,  // subset 0
    DCT_V,     // subset 2
    NUM_TRANS_TYPE,
    DCT2_HEVC,
    DCT2_EMT,
};
#endif

enum ScalabilityType
{
    VIEW_ORDER_INDEX  = 1,
    SCALABILITY_ID = 2,
};

/**
 * Table 7-3: NAL unit type codes
 */
enum NALUnitType {
    NAL_TRAIL_N    = 0,
    NAL_TRAIL_R    = 1,
    NAL_TSA_N      = 2,
    NAL_TSA_R      = 3,
    NAL_STSA_N     = 4,
    NAL_STSA_R     = 5,
    NAL_RADL_N     = 6,
    NAL_RADL_R     = 7,
    NAL_RASL_N     = 8,
    NAL_RASL_R     = 9,
    NAL_BLA_W_LP   = 16,
    NAL_BLA_W_RADL = 17,
    NAL_BLA_N_LP   = 18,
    NAL_IDR_W_RADL = 19,
    NAL_IDR_N_LP   = 20,
    NAL_CRA_NUT    = 21,
    NAL_VPS        = 32,
    NAL_SPS        = 33,
    NAL_PPS        = 34,
    NAL_AUD        = 35,
    NAL_EOS_NUT    = 36,
    NAL_EOB_NUT    = 37,
    NAL_FD_NUT     = 38,
    NAL_SEI_PREFIX = 39,
    NAL_SEI_SUFFIX = 40,
};
#if 0
#define print_cabac(string, val) \
    printf(" %s : %d \n", string, val);
#else
#define print_cabac(string, val)
#endif

enum RPSType {
    ST_CURR_BEF = 0,
    ST_CURR_AFT,
    ST_FOLL,
    LT_CURR,
    LT_FOLL,
    IL_REF0,
    IL_REF1,
    NB_RPS_TYPE,
};

enum SliceType {
    B_SLICE = 0,
    P_SLICE = 1,
    I_SLICE = 2,
};

enum SyntaxElement {
    SAO_MERGE_FLAG = 0,
    SAO_TYPE_IDX,
    SAO_EO_CLASS,
    SAO_BAND_POSITION,
    SAO_OFFSET_ABS,
    SAO_OFFSET_SIGN,
    END_OF_SLICE_FLAG,
    SPLIT_CODING_UNIT_FLAG,
    CU_TRANSQUANT_BYPASS_FLAG,
    SKIP_FLAG,
    CU_QP_DELTA,
    PRED_MODE_FLAG,
    PART_MODE,
    PCM_FLAG,
    PREV_INTRA_LUMA_PRED_FLAG,
    MPM_IDX,
    REM_INTRA_LUMA_PRED_MODE,
    INTRA_CHROMA_PRED_MODE,
    MERGE_FLAG,
    MERGE_IDX,
    INTER_PRED_IDC,
    REF_IDX_L0,
    REF_IDX_L1,
    ABS_MVD_GREATER0_FLAG,
    ABS_MVD_GREATER1_FLAG,
    ABS_MVD_MINUS2,
    MVD_SIGN_FLAG,
    MVP_LX_FLAG,
    NO_RESIDUAL_DATA_FLAG,
    SPLIT_TRANSFORM_FLAG,
    CBF_LUMA,
    CBF_CB_CR,
    TRANSFORM_SKIP_FLAG,
    EXPLICIT_RDPCM_FLAG,
    EXPLICIT_RDPCM_DIR_FLAG,
    LAST_SIGNIFICANT_COEFF_X_PREFIX,
    LAST_SIGNIFICANT_COEFF_Y_PREFIX,
    LAST_SIGNIFICANT_COEFF_X_SUFFIX,
    LAST_SIGNIFICANT_COEFF_Y_SUFFIX,
    SIGNIFICANT_COEFF_GROUP_FLAG,
    SIGNIFICANT_COEFF_FLAG,
    COEFF_ABS_LEVEL_GREATER1_FLAG,
    COEFF_ABS_LEVEL_GREATER2_FLAG,
    COEFF_ABS_LEVEL_REMAINING,
    COEFF_SIGN_FLAG,
    LOG2_RES_SCALE_ABS,
    RES_SCALE_SIGN_FLAG,
    CU_CHROMA_QP_OFFSET_FLAG,
    CU_CHROMA_QP_OFFSET_IDX,
#if COM16_C806_EMT
    EMT_CU_FLAG,
    EMT_TU_IDX,
#endif
};

enum PartMode {
    PART_2Nx2N = 0,
    PART_2NxN  = 1,
    PART_Nx2N  = 2,
    PART_NxN   = 3,
    PART_2NxnU = 4,
    PART_2NxnD = 5,
    PART_nLx2N = 6,
    PART_nRx2N = 7,
};

enum PredMode {
    MODE_INTER = 0,
    MODE_INTRA,
    MODE_SKIP,
};

enum InterPredIdc {
    PRED_L0 = 0,
    PRED_L1,
    PRED_BI,
};

enum PredFlag {
    PF_INTRA = 0,
    PF_L0,
    PF_L1,
    PF_BI,
};

enum IntraPredMode {
    INTRA_PLANAR = 0,
    INTRA_DC,
    INTRA_ANGULAR_2,
    INTRA_ANGULAR_3,
    INTRA_ANGULAR_4,
    INTRA_ANGULAR_5,
    INTRA_ANGULAR_6,
    INTRA_ANGULAR_7,
    INTRA_ANGULAR_8,
    INTRA_ANGULAR_9,
    INTRA_ANGULAR_10,
    INTRA_ANGULAR_11,
    INTRA_ANGULAR_12,
    INTRA_ANGULAR_13,
    INTRA_ANGULAR_14,
    INTRA_ANGULAR_15,
    INTRA_ANGULAR_16,
    INTRA_ANGULAR_17,
    INTRA_ANGULAR_18,
    INTRA_ANGULAR_19,
    INTRA_ANGULAR_20,
    INTRA_ANGULAR_21,
    INTRA_ANGULAR_22,
    INTRA_ANGULAR_23,
    INTRA_ANGULAR_24,
    INTRA_ANGULAR_25,
    INTRA_ANGULAR_26,
    INTRA_ANGULAR_27,
    INTRA_ANGULAR_28,
    INTRA_ANGULAR_29,
    INTRA_ANGULAR_30,
    INTRA_ANGULAR_31,
    INTRA_ANGULAR_32,
    INTRA_ANGULAR_33,
    INTRA_ANGULAR_34,
};

enum SAOType {
    SAO_NOT_APPLIED = 0,
    SAO_BAND,
    SAO_EDGE,
    SAO_APPLIED
};

enum SAOEOClass {
    SAO_EO_HORIZ = 0,
    SAO_EO_VERT,
    SAO_EO_135D,
    SAO_EO_45D,
};

enum ScanType {
    SCAN_DIAG = 0,
    SCAN_HORIZ,
    SCAN_VERT,
};

#ifdef SVC_EXTENSION
enum {
    DEFAULT=0,
    X2,
    X1_5,
    SNR,
};

enum ChannelType {
    CHANNEL_TYPE_LUMA    = 0,
    CHANNEL_TYPE_CHROMA  = 1,
    MAX_NUM_CHANNEL_TYPE = 2
};

typedef struct UpsamplInf {
    int addXLum;
    int addYLum;
    int scaleXLum;
    int scaleYLum;
    int addXCr;
    int addYCr;
    int scaleXCr;
    int scaleYCr;
    int idx;
    int shift[MAX_NUM_CHANNEL_TYPE];
    int shift_up[MAX_NUM_CHANNEL_TYPE];
} UpsamplInf;
#endif

typedef struct ShortTermRPS {
    unsigned int num_negative_pics;
    int num_delta_pocs;
    int rps_idx_num_delta_pocs;
    int32_t delta_poc[32];
    uint8_t used[32];
} ShortTermRPS;

typedef struct LongTermRPS {
    int     poc[32];
    uint8_t used[32];
    uint8_t nb_refs;
} LongTermRPS;

typedef struct RefPicList {
    struct HEVCFrame *ref[MAX_REFS];
    int list[MAX_REFS];
    int isLongTerm[MAX_REFS];
    int nb_refs;
} RefPicList;

typedef struct RefPicListTab {
    RefPicList refPicList[2];
} RefPicListTab;

typedef struct HEVCWindow {
    int left_offset;
    int right_offset;
    int top_offset;
    int bottom_offset;
} HEVCWindow;

typedef struct VUI {
    AVRational sar;

    int overscan_info_present_flag;
    int overscan_appropriate_flag;

    int video_signal_type_present_flag;
    int video_format;
    int video_full_range_flag;
    int colour_description_present_flag;
    uint8_t colour_primaries;
    uint8_t transfer_characteristic;
    uint8_t matrix_coeffs;

    int chroma_loc_info_present_flag;
    int chroma_sample_loc_type_top_field;
    int chroma_sample_loc_type_bottom_field;
    int neutra_chroma_indication_flag;

    int field_seq_flag;
    int frame_field_info_present_flag;

    int default_display_window_flag;
    HEVCWindow def_disp_win;

    int vui_timing_info_present_flag;
    uint32_t vui_num_units_in_tick;
    uint32_t vui_time_scale;
    int vui_poc_proportional_to_timing_flag;
    int vui_num_ticks_poc_diff_one_minus1;
    int vui_hrd_parameters_present_flag;

    int bitstream_restriction_flag;
    int tiles_fixed_structure_flag;
    int motion_vectors_over_pic_boundaries_flag;
    int restricted_ref_pic_lists_flag;
    int min_spatial_segmentation_idc;
    int max_bytes_per_pic_denom;
    int max_bits_per_min_cu_denom;
    int log2_max_mv_length_horizontal;
    int log2_max_mv_length_vertical;
} VUI;

typedef struct PTLCommon {
    uint8_t profile_space;
    uint8_t tier_flag;
    uint8_t profile_idc;
    uint8_t profile_compatibility_flag[32];
    uint8_t level_idc;
    uint8_t progressive_source_flag;
    uint8_t interlaced_source_flag;
    uint8_t non_packed_constraint_flag;
    uint8_t frame_only_constraint_flag;
    //PTL Ptl_general;
    uint8_t setProfileIdc;
    uint8_t general_inbld_flag; 
} PTLCommon;

typedef struct PTL {
    PTLCommon general_ptl;
    PTLCommon sub_layer_ptl[MAX_SUB_LAYERS];

    uint8_t sub_layer_profile_present_flag[MAX_SUB_LAYERS];
    uint8_t sub_layer_level_present_flag[MAX_SUB_LAYERS];
    //PTL Ptl_sublayer[16];

    int sub_layer_profile_space[MAX_SUB_LAYERS];
    uint8_t sub_layer_tier_flag[MAX_SUB_LAYERS];
    int sub_layer_profile_idc[MAX_SUB_LAYERS];
    uint8_t sub_layer_profile_compatibility_flags[MAX_SUB_LAYERS][32];
    int sub_layer_level_idc[MAX_SUB_LAYERS];
} PTL;




enum ChromaFormat
{
    CHROMA_400  = 0,
    CHROMA_420  = 1,
    CHROMA_422  = 2,
    CHROMA_444  = 3
#if AUXILIARY_PICTURES
    ,NUM_CHROMA_FORMAT = 4
#endif
};

#define MULTIPLE_PTL_SUPPORT 1
enum Profiles   {
    NONE = 0,
    MAIN = 1,
    MAIN10 = 2,
    MAINSTILLPICTURE = 3,
#if MULTIPLE_PTL_SUPPORT
    RANGEEXTENSION = 4,
    RANGEEXTENSIONHIGH = 5,
    MULTIVIEWMAIN = 6,
    SCALABLEMAIN = 7,
    SCALABLEMAIN10 = 8,
#endif
};

enum Level   {
    L_NONE     = 0,
    LEVEL1        = 30,
    LEVEL2        = 60,
    LEVEL2_1      = 63,
    LEVEL3        = 90,
    LEVEL3_1      = 93,
    LEVEL4        = 120,
    LEVEL4_1      = 123,
    LEVEL5        = 150,
    LEVEL5_1      = 153,
    LEVEL5_2      = 156,
    LEVEL6        = 180,
    LEVEL6_1      = 183,
    LEVEL6_2      = 186,
};

enum Tier   {
    T_MAIN = 0,
    T_HIGH = 1,
};

/*
 Encryption configuration
 */
enum hevc_crypto_features {
    HEVC_CRYPTO_OFF = 0,
    HEVC_CRYPTO_MVs = (1 << 0),
    HEVC_CRYPTO_MV_SIGNS = (1 << 1),
    HEVC_CRYPTO_TRANSF_COEFFS = (1 << 2),
    HEVC_CRYPTO_TRANSF_COEFF_SIGNS = (1 << 3),
	HEVC_CRYPTO_INTRA_PRED_MODE = (1 << 4),
    HEVC_CRYPTO_ON = (1 << 5) - 1,
};


typedef struct RepFormat
{
    enum ChromaFormat chroma_format_vps_idc;
    int16_t   pic_width_vps_in_luma_samples;
    int16_t   pic_height_vps_in_luma_samples;
    uint8_t   chroma_and_bit_depth_vps_present_flag;
    uint8_t   separate_colour_plane_vps_flag; 
    uint8_t   bit_depth_vps[MAX_NUM_CHANNEL_TYPE]; // coded as minus8


    uint8_t conformance_window_vps_flag;
    int     conf_win_vps_left_offset;
    int     conf_win_vps_right_offset;
    int     conf_win_vps_top_offset;
    int     conf_win_vps_bottom_offset;
} RepFormat;

typedef struct SAOParams {
    uint8_t offset_abs[3][4];   ///< sao_offset_abs
    uint8_t offset_sign[3][4];  ///< sao_offset_sign

    uint8_t band_position[3];   ///< sao_band_position
    int16_t offset_val[3][5];   ///<SaoOffsetVal

    uint8_t eo_class[3];        ///< sao_eo_class
    uint8_t type_idx[3];        ///< sao_type_idx
} SAOParams;

typedef struct SubLayerHRDParameter {
    int bit_rate_value_minus1[16];
    int cpb_size_value_minus1[16];
    int cpb_size_du_value_minus1[16];
    int bit_rate_du_value_minus1[16];
    int cbr_flag[16];
} SubLayerHRDParameter;

typedef struct HRDParameter {
    uint8_t     nal_hrd_parameters_present_flag;
    uint8_t     vcl_hrd_parameters_present_flag;
    uint8_t     sub_pic_hrd_params_present_flag;
    uint8_t     tick_divisor_minus2;
    uint8_t     du_cpb_removal_delay_increment_length_minus1;
    uint8_t     sub_pic_cpb_params_in_pic_timing_sei_flag;
    uint8_t     dpb_output_delay_du_length_minus1;
    uint8_t     bit_rate_scale;
    uint8_t     cpb_size_scale;
    uint8_t     cpb_size_du_scale;
    uint8_t     initial_cpb_removal_delay_length_minus1;
    uint8_t     au_cpb_removal_delay_length_minus1;
    uint8_t     dpb_output_delay_length_minus1;

    uint8_t     fixed_pic_rate_general_flag[16];
    uint8_t     fixed_pic_rate_within_cvs_flag[16];
    int         elemental_duration_in_tc_minus1[16];
    uint8_t     low_delay_hrd_flag[16];
    uint8_t     cpb_cnt_minus1[16];
    
    SubLayerHRDParameter Sublayer_HRDPar[16]; 
} HRDParameter;

typedef struct VideoSignalInfo {
    uint8_t video_vps_format;                
	uint8_t video_full_range_vps_flag;
	uint8_t color_primaries_vps;
	uint8_t transfer_characteristics_vps;
	uint8_t matrix_coeffs_vps;
} VideoSignalInfo;

typedef struct BspHrdParams {
    uint8_t vps_num_add_hrd_params;
    uint8_t cprms_add_present_flag[16];
    uint8_t num_sub_layer_hrd_minus1[16];
    HRDParameter HrdParam[16];
    uint8_t num_signalled_partitioning_schemes[16];
    uint8_t layer_included_in_partition_flag[16][16][16][16];
    uint8_t num_partitions_in_scheme_minus1[16][16];
    uint8_t num_bsp_schedules_minus1[16][16][16];
    uint8_t bsp_hrd_idx[16][16][16][16][16];
    uint8_t bsp_sched_idx[16][16][16][16][16];

} BspHrdParams;

typedef struct VPSVUI {
    uint8_t cross_layer_pic_type_aligned_flag;
    uint8_t cross_layer_irap_aligned_flag;
    uint8_t all_layers_idr_aligned_flag;
    uint8_t bit_rate_present_vps_flag;
    uint8_t pic_rate_present_vps_flag;
    uint8_t bit_rate_present_flag[16][16];
    uint8_t pic_rate_present_flag[16][16];

    uint16_t avg_bit_rate[16][16];
    uint16_t max_bit_rate[16][16];

    uint8_t  constant_pic_rate_idc[16][16];
    uint16_t avg_pic_rate[16][16];

    uint8_t video_signal_info_idx_present_flag;
    uint8_t vps_num_video_signal_info_minus1;
    VideoSignalInfo video_signal_info[16];
    uint8_t vps_video_signal_info_idx[16];
    uint8_t tiles_not_in_use_flag;
    uint8_t tiles_in_use_flag[16];
    uint8_t loop_filter_not_across_tiles_flag[16];
    uint8_t tile_boundaries_aligned_flag[16][16];

    uint8_t wpp_not_in_use_flag; 
    uint8_t wpp_in_use_flag[16];

    uint8_t single_layer_for_non_irap_flag;
    uint8_t higher_layer_irap_skip_flag;    
    uint8_t ilp_restricted_ref_layers_flag;

    uint8_t min_spatial_segment_offset_plus1[16][16];
    uint8_t ctu_based_offset_enabled_flag[16][16];
    uint8_t min_horizontal_ctu_offset_plus1[16][16];

    uint8_t vps_vui_bsp_hrd_present_flag;
    BspHrdParams Bsp_Hrd_Params;
    uint8_t base_layer_parameter_set_compatibility_flag[16]; 
} VPSVUI;

typedef struct DPBSize {
    uint8_t sub_layer_flag_info_present_flag[16];
    uint8_t sub_layer_dpb_info_present_flag[16][16];
    uint16_t max_vps_dec_pic_buffering_minus1[16][16][16];
    uint16_t max_vps_num_reorder_pics[16][16];
    uint16_t max_vps_latency_increase_plus1[16][16];
} DPBSize;

typedef struct HEVCVPSExt {
    PTL   ptl[16];
    uint8_t     splitting_flag;
    uint8_t     scalability_mask[16];
    uint8_t     dimension_id_len[16];
    uint8_t     nuh_layer_id_present_flag;
    uint8_t     layer_id_in_nuh[16];
    uint8_t     layer_id_in_vps[16];
    uint8_t     dimension_id[16][16];
    uint8_t     view_id_len;
    uint8_t     view_id_val[16];
    uint8_t     direct_dependency_flag[16][16];
    uint8_t     direct_dependency_type[16][16];
    uint8_t     ref_layer_id[16][16];
    uint8_t     NumIndependentLayers; 
    uint8_t     num_add_layer_sets;
    uint8_t     tree_partition_layerIdList[16][16];
    uint8_t     num_Layers_in_tree_partition[16];
    uint8_t     num_Layers_in_id_list[16];
    uint8_t     OlsHighestOutputLayerId[16]; 
    uint8_t     layer_SetLayer_IdList[16][16];
    uint8_t     num_direct_ref_layers[16];
    uint8_t     number_ref_layers[16][16];
    uint8_t     num_predicted_layers[16]; 
    uint8_t     predicted_layerId[16][16];
    uint8_t     recursive_refLayer_flag[16][16];
    uint8_t     highest_layer_idx[16][16];
    uint8_t     vps_sub_layers_max_minus1_present_flag;
    uint8_t     sub_layers_vps_max_minus1[16];
    uint8_t     max_sub_layers_in_layer_set_minus1[16];
    uint8_t     max_tid_ref_present_flag;
    uint8_t     max_tid_il_ref_pics_plus1[16][16];
    uint8_t     all_ref_layers_active_flag; // uint8_t     default_ref_layers_active_flag;
    uint8_t     vps_num_profile_tier_level_minus1;
    uint8_t     vps_profile_present_flag[16];
    uint8_t     necessary_Layer_Flag[16][16];
    uint8_t     num_add_olss;
    uint8_t     NumOutputLayerSets; 
    uint8_t     default_output_layer_idc;
    uint8_t     layer_set_idx_for_ols[16];
    uint8_t     output_layer_flag[16][16];
    uint8_t     profile_tier_level_idx[16][16];
    uint8_t     alt_output_layer_flag[16];
    uint8_t     vps_num_rep_formats_minus1;
    RepFormat   rep_format[16];
    uint8_t     rep_format_idx_present_flag;
    uint8_t     vps_rep_format_idx[16];
    uint8_t     max_one_active_ref_layer_flag;
    uint8_t     vps_poc_lsb_aligned_flag;
    uint8_t     poc_lsb_not_present_flag[16];
    DPBSize     DPB_Size; 
    uint8_t     direct_dep_type_len_minus2;
    uint8_t     default_direct_dependency_type_flag;
    uint8_t     default_direct_dependency_type; 
    /*uint8_t     direct_dependency_all_layers_flag;
    uint8_t     direct_dependency_all_layers_type;*/
    VPSVUI VpsVui; 
} HEVCVPSExt;

typedef struct HEVCVPS {
    uint8_t  vps_id;
    uint8_t  vps_base_layer_internal_flag;
    uint8_t  vps_base_layer_available_flag;
    uint8_t  vps_nonHEVCBaseLayerFlag; ///< not standard but helpfull
    uint8_t  vps_max_layers;     ///< vps_max_layers_minus1 + 1
    uint8_t  vps_max_sub_layers; ///< vps_max_temporal_layers_minus1 + 1
    uint8_t  vps_temporal_id_nesting_flag;
    uint16_t vps_reserved_0xffff_16bits;

    PTL ptl; //TODO profile their level could be a pointer to a PTL to reduce VPS
    // footprint however we have to be carefull while comparing VPS NAL for
    // removal from vps list

    uint8_t  vps_sub_layer_ordering_info_present_flag;
    //FIXME structure + dyn malloc for sub_layers ordering info ???
    uint8_t    vps_max_dec_pic_buffering[MAX_SUB_LAYERS]; ///< vps_max_dec_pic_buffering_minus1 + 1
    uint8_t    vps_max_num_reorder_pics [MAX_SUB_LAYERS];
    int32_t    vps_max_latency_increase [MAX_SUB_LAYERS]; ///< vps_max_dec_pic_buffering_plus1 - 1

    uint8_t  vps_max_layer_id;
    int16_t  vps_num_layer_sets; ///< vps_num_layer_sets_minus1 + 1
    uint8_t    layer_id_included_flag[16][16];

    uint8_t  vps_timing_info_present_flag;
    //FIXME structure + dyn malloc for vps timing info ???
    uint32_t   vps_num_units_in_tick;
    uint32_t   vps_time_scale;
    uint8_t    vps_poc_proportional_to_timing_flag;
    int          vps_num_ticks_poc_diff_one; ///< vps_num_ticks_poc_diff_one_minus1 + 1
    int16_t    vps_num_hrd_parameters;
    unsigned int hrd_layer_set_idx[16];
    //FIXME keep cprms_present_flag table ???
    HRDParameter HrdParam;

    //FIXME vps_extension2_flag etc..
    int         vps_extension_flag;
    HEVCVPSExt  vps_ext;
    AVBufferRef *nal;
} HEVCVPS;


typedef struct ScalingList {
    /* This is a little wasteful, since sizeID 0 only needs 8 coeffs,
     * and size ID 3 only has 2 arrays, not 6. */
    uint8_t sl[4][6][64];
    uint8_t sl_dc[2][6];
} ScalingList;


typedef struct HEVCSPS {
    uint8_t  vps_id;

    uint8_t  sps_max_sub_layers; ///<sps_max_sub_layers_minus1 + 1
    uint8_t  sps_ext_or_max_sub_layers; ///< sps_ext_or_max_sub_layers_minus1 + 1


    // This is added to SPS in order to know if
    // wether or not we are in a multi_layer context
    //value is set to (nuh_layer_id && max_sub_layers == 7)
    uint8_t is_multi_layer_ext_sps;

    uint8_t sps_temporal_id_nesting_flag;
    uint8_t sps_id;


    PTL       ptl;

    uint8_t chroma_format_idc;
    uint8_t separate_colour_plane_flag;

    //uint16_t width_in_luma_samples == width
    //uint16_t height_in_luma_samples == height

    uint8_t conformance_window_flag;
    HEVCWindow conf_win;

    //FIXME the use of table is sometimes troubling into the code
    // we should use bitdepth and bit depth_c as specified by the standard
    int bit_depth[MAX_NUM_CHANNEL_TYPE];
    int pixel_shift[MAX_NUM_CHANNEL_TYPE]; // Not in the standard but usefull

    unsigned int log2_max_poc_lsb; ///< log2_max_pic_order_cnt_lsb_minus4 + 4

    uint8_t sps_sub_layer_ordering_info_present_flag;

    struct {
        uint16_t max_dec_pic_buffering;
        uint16_t num_reorder_pics;
        uint16_t max_latency_increase;
    } temporal_layer[MAX_SUB_LAYERS];

    // multi_layer_ext_flag == 1
    uint8_t update_rep_format_flag;
    uint8_t sps_rep_format_idx;
    uint8_t sps_infer_scaling_list_flag;
    uint8_t sps_scaling_list_ref_layer_id;

    unsigned int log2_min_cb_size;
    unsigned int log2_diff_max_min_cb_size;
    unsigned int log2_min_tb_size;
    unsigned int log2_diff_max_min_tb_size;

    int max_transform_hierarchy_depth_inter;
    int max_transform_hierarchy_depth_intra;

    uint8_t scaling_list_enabled_flag;

    //TODO  rename those
    uint8_t sps_scaling_list_data_present_flag;
    ScalingList scaling_list;

    uint8_t amp_enabled_flag;
    uint8_t sao_enabled_flag;
    uint8_t pcm_enabled_flag;

    struct {
        uint8_t bit_depth;
        uint8_t bit_depth_chroma;
        unsigned int log2_min_pcm_cb_size;
        unsigned int log2_max_pcm_cb_size;
        uint8_t loop_filter_disable_flag;
    } pcm;

    unsigned int num_short_term_rps;
    ShortTermRPS st_rps[MAX_SHORT_TERM_RPS_COUNT];

    uint8_t  long_term_ref_pics_present_flag;
    uint8_t  num_long_term_ref_pics_sps;
    uint16_t lt_ref_pic_poc_lsb_sps      [32];
    uint8_t  used_by_curr_pic_lt_sps_flag[32];

    uint8_t  sps_temporal_mvp_enabled_flag;
    uint8_t  sps_strong_intra_smoothing_enable_flag;

    uint8_t vui_parameters_present_flag;
    VUI vui;

    uint8_t  sps_extension_present_flag;
    uint8_t  sps_range_extension_flag;
    uint8_t  sps_multilayer_extension_flag;
    uint8_t  sps_3d_extension_flag;
    uint8_t  sps_extension_5bits;

    //TODO HEVCSPSExt struct;

    // Range extensions
    uint8_t transform_skip_rotation_enabled_flag;
    uint8_t transform_skip_context_enabled_flag;
    uint8_t implicit_rdpcm_enabled_flag;
    uint8_t explicit_rdpcm_enabled_flag;
    uint8_t extended_precision_processing_flag;
    uint8_t intra_smoothing_disabled_flag;
    uint8_t high_precision_offsets_enabled_flag; 
    uint8_t persistent_rice_adaptation_enabled_flag;
    uint8_t cabac_bypass_alignment_enabled_flag;

    // Multi-layer extensions
    uint8_t inter_view_mv_vert_constraint_flag;

    //Those are some useful computed values which could be added to the HEVCContext
    //instead
    ///< output (i.e. cropped) values
    int output_width, output_height;
    HEVCWindow output_window;

    enum AVPixelFormat pix_fmt;

    unsigned int log2_ctb_size;
    unsigned int log2_min_pu_size;
    unsigned int log2_max_trafo_size;

    ///< coded frame dimension in various units
    int width;
    int height;
    int ctb_width;
    int ctb_height;
    int ctb_size;
    int min_cb_width;
    int min_cb_height;
    int min_tb_width;
    int min_tb_height;
    int min_pu_width;
    int min_pu_height;
    int tb_mask;

    int hshift[3];
    int vshift[3];

    int qp_bd_offset;

    HRDParameter HrdParam;

    //FIXME scaled_ref_layer is used but not set ???
    HEVCWindow scaled_ref_layer_window[MAX_LAYERS];
    //FIXME: This flag could probably be removed without any harm
    uint8_t    set_mfm_enabled_flag;
    uint8_t    v1_compatible;

#if COM16_C806_EMT
    uint8_t use_intra_emt;
    uint8_t use_inter_emt;
#endif
} HEVCSPS;

typedef struct SYUVP {
    int16_t Y;
    int16_t U;
    int16_t V;
} SYUVP;

typedef struct SCuboid {
  SYUVP P[4];
} SCuboid;

typedef struct TCom3DAsymLUT {
    uint16_t  num_cm_ref_layers_minus1;
    uint8_t   uiRefLayerId[16];
    uint8_t   cm_octant_depth;              // m_nMaxOctantDepth = nMaxOctantDepth;
    uint8_t   cm_y_part_num_log2;           //m_nMaxYPartNumLog2 = nMaxYPartNumLog2;
    uint16_t  cm_input_luma_bit_depth;     // m_nInputBitDepthY = nInputBitDepth;
    uint16_t  cm_input_chroma_bit_depth;   // m_nInputBitDepthC = nInputBitDepthC;
    uint16_t  cm_output_luma_bit_depth;
    uint16_t  cm_output_chroma_bit_depth;  // m_nOutputBitDepthC = nOutputBitDepthC;
    uint8_t   cm_res_quant_bit;
    uint8_t   cm_flc_bits;
    int  cm_adapt_threshold_u_delta;
    int  cm_adapt_threshold_v_delta;
    int  nAdaptCThresholdU;
    int  nAdaptCThresholdV;

    int16_t  delta_bit_depth;   //    m_nDeltaBitDepthC = m_nOutputBitDepthC - m_nInputBitDepthC;
    int16_t  delta_bit_depth_C; //    m_nDeltaBitDepth = m_nOutputBitDepthY - m_nInputBitDepthY;
    uint16_t  max_part_num_log2; //3 * m_nMaxOctantDepth + m_nMaxYPartNumLog2;

    int16_t YShift2Idx;
    int16_t UShift2Idx;
    int16_t VShift2Idx;
    int16_t nMappingShift;
    int16_t nMappingOffset;
    SCuboid ***S_Cuboid;

} TCom3DAsymLUT;

typedef struct HEVCPPS {
    uint8_t pps_id;
    uint8_t sps_id; ///< seq_parameter_set_id

    uint8_t dependent_slice_segments_enabled_flag;
    uint8_t output_flag_present_flag;

    int     num_extra_slice_header_bits;

    uint8_t sign_data_hiding_flag;
    uint8_t cabac_init_present_flag;

    int num_ref_idx_l0_default_active; ///< num_ref_idx_l0_default_active_minus1 + 1
    int num_ref_idx_l1_default_active; ///< num_ref_idx_l1_default_active_minus1 + 1

    int init_qp_minus26;

    uint8_t constrained_intra_pred_flag;
    uint8_t transform_skip_enabled_flag;

    uint8_t cu_qp_delta_enabled_flag;
    int     diff_cu_qp_delta_depth;

    int pps_cb_qp_offset;
    int pps_cr_qp_offset;

    uint8_t pps_slice_chroma_qp_offsets_present_flag;

    uint8_t weighted_pred_flag;
    uint8_t weighted_bipred_flag;

    uint8_t transquant_bypass_enable_flag;
    uint8_t tiles_enabled_flag;
    uint8_t entropy_coding_sync_enabled_flag;

    int num_tile_columns;   ///< num_tile_columns_minus1 + 1
    int num_tile_rows;      ///< num_tile_rows_minus1 + 1

    uint8_t uniform_spacing_flag;
    unsigned int *column_width;  ///< ColumnWidth
    unsigned int *row_height;    ///< RowHeight

    uint8_t loop_filter_across_tiles_enabled_flag;
    uint8_t pps_loop_filter_across_slices_enabled_flag;

    uint8_t deblocking_filter_control_present_flag;
    uint8_t deblocking_filter_override_enabled_flag;
    uint8_t pps_deblocking_filter_disabled_flag;

    int8_t pps_beta_offset;    ///< beta_offset_div2 * 2
    int8_t pps_tc_offset;      ///< tc_offset_div2 * 2

    uint8_t pps_scaling_list_data_present_flag;
    ScalingList scaling_list;

    uint8_t lists_modification_present_flag;
    int     log2_parallel_merge_level; ///< log2_parallel_merge_level_minus2 + 2

    uint8_t slice_segment_header_extension_present_flag;

    uint8_t pps_extension_present_flag;
    uint8_t pps_range_extension_flag;
    uint8_t pps_multilayer_extension_flag;
    uint8_t pps_3d_extension_flag;
    uint8_t pps_extension_5bits;


    //Range extensions
    uint8_t log2_max_transform_skip_block_size;
    uint8_t cross_component_prediction_enabled_flag;
    uint8_t chroma_qp_offset_list_enabled_flag;
    uint8_t diff_cu_chroma_qp_offset_depth;

    uint8_t chroma_qp_offset_list_len_minus1;
    int8_t  cb_qp_offset_list[5];
    int8_t  cr_qp_offset_list[5];

    uint8_t log2_sao_offset_scale_luma;
    uint8_t log2_sao_offset_scale_chroma;

    // Multilayer_extensions
    uint8_t poc_reset_info_present_flag;
    uint8_t pps_infer_scaling_list_flag;
    uint8_t pps_scaling_list_ref_layer_id;

    uint16_t num_ref_loc_offsets;
    //those should be tables are in tables
    uint8_t ref_loc_offset_layer_id;
    uint8_t scaled_ref_layer_offset_present_flag;
    HEVCWindow scaled_ref_window[16];

    uint8_t ref_region_offset_present_flag;
    HEVCWindow ref_window[16];

    uint8_t resample_phase_set_present_flag;

    uint16_t phase_hor_luma[16];
    uint16_t phase_ver_luma[16];
    uint16_t phase_hor_chroma[16];
    uint16_t phase_ver_chroma[16];

    uint8_t colour_mapping_enabled_flag;



    TCom3DAsymLUT pc3DAsymLUT;
    uint8_t m_nCGSOutputBitDepth[MAX_NUM_CHANNEL_TYPE];

    // Derived parameters

    unsigned int *col_bd;        ///< ColBd
    unsigned int *row_bd;        ///< RowBd

    int *col_idxX;
    int *ctb_addr_rs_to_ts; ///< CtbAddrRSToTS
    int *ctb_addr_ts_to_rs; ///< CtbAddrTSToRS
    int *tile_id;           ///< TileId
    int *tile_width;           ///< TileWidth
    int *tile_pos_rs;       ///< TilePosRS
    int *wpp_pos_ts;
    int *min_tb_addr_zs;    ///< MinTbAddrZS
    int *min_tb_addr_zs_tab;///< MinTbAddrZS

    uint8_t is_setup;

} HEVCPPS;

typedef struct HEVCParamSets {
    AVBufferRef *vps_list[MAX_VPS_COUNT];
    AVBufferRef *sps_list[MAX_SPS_COUNT];
    AVBufferRef *pps_list[MAX_PPS_COUNT];

    /* currently active parameter sets */
    const HEVCVPS *vps;
    const HEVCSPS *sps;
    const HEVCPPS *pps;
} HEVCParamSets;

typedef struct SliceHeader {
    unsigned int pps_id;

    ///< address (in raster order) of the first block in the current slice segment
    unsigned int   slice_segment_addr;
    ///< address (in raster order) of the first block in the current slice
    unsigned int   slice_addr;

    enum SliceType slice_type;

    int pic_order_cnt_lsb;

    uint8_t first_slice_in_pic_flag;
    uint8_t dependent_slice_segment_flag;
    uint8_t pic_output_flag;
    uint8_t colour_plane_id;

    ///< RPS coded in the slice header itself is stored here
    int short_term_ref_pic_set_sps_flag;
    int short_term_ref_pic_set_size;
    ShortTermRPS slice_rps;
    const ShortTermRPS *short_term_rps;
    int long_term_ref_pic_set_size;
    LongTermRPS long_term_rps;
    unsigned int list_entry_lx[2][32];

    uint8_t rpl_modification_flag[2];
    uint8_t no_output_of_prior_pics_flag;
    uint8_t slice_temporal_mvp_enabled_flag;

    unsigned int nb_refs[2];

    uint8_t slice_sample_adaptive_offset_flag[3];
    uint8_t mvd_l1_zero_flag;

    uint8_t cabac_init_flag;
    uint8_t disable_deblocking_filter_flag; ///< slice_header_disable_deblocking_filter_flag
    uint8_t slice_loop_filter_across_slices_enabled_flag;
    uint8_t collocated_list;

    unsigned int collocated_ref_idx;

    int slice_qp_delta;
    int slice_cb_qp_offset;
    int slice_cr_qp_offset;

    uint8_t cu_chroma_qp_offset_enabled_flag;

    int8_t beta_offset;    ///< beta_offset_div2 * 2
    int8_t tc_offset;      ///< tc_offset_div2 * 2

    unsigned int max_num_merge_cand; ///< 5 - 5_minus_max_num_merge_cand

    int *entry_point_offset;
    int * offset;
    int * size;
    int num_entry_point_offsets;

    int8_t slice_qp;

    uint8_t luma_log2_weight_denom;
    int16_t chroma_log2_weight_denom;

    int16_t luma_weight_l0[16];
    int16_t chroma_weight_l0[16][2];
    int16_t chroma_weight_l1[16][2];
    int16_t luma_weight_l1[16];

    int16_t luma_offset_l0[16];
    int16_t chroma_offset_l0[16][2];

    int16_t luma_offset_l1[16];
    int16_t chroma_offset_l1[16][2];

    int inter_layer_pred_enabled_flag;

    int     active_num_ILR_ref_idx;        //< Active inter-layer reference pictures
    int     inter_layer_pred_layer_idc[MAX_LAYERS];

#ifdef SVC_EXTENSION
    int ScalingFactor[MAX_LAYERS][2];
    int MvScalingFactor[MAX_LAYERS][2];
    int Bit_Depth[MAX_LAYERS][MAX_NUM_CHANNEL_TYPE];

    uint8_t m_bPocResetFlag;
    uint8_t m_bCrossLayerBLAFlag; 
#endif

    int slice_ctb_addr_rs;
    uint8_t discardable_flag;
} SliceHeader;

typedef struct CodingTree {
    int depth; ///< ctDepth
} CodingTree;

typedef struct CodingUnit {
    int x;
    int y;

    enum PredMode pred_mode;    ///< PredMode
    enum PartMode part_mode;    ///< PartMode

    uint8_t rqt_root_cbf;

    uint8_t pcm_flag;
#if COM16_C806_EMT
    uint8_t emt_cu_flag;
#endif
    // Inferred parameters
    uint8_t intra_split_flag;   ///< IntraSplitFlag
    uint8_t max_trafo_depth;    ///< MaxTrafoDepth
    uint8_t cu_transquant_bypass_flag;
} CodingUnit;

typedef struct Mv {
    int16_t x;  ///< horizontal component of motion vector
    int16_t y;  ///< vertical component of motion vector
} Mv;

typedef struct MvField {
    DECLARE_ALIGNED(4, Mv, mv)[2];
#ifdef TEST_MV_POC
    int32_t poc[2];
    uint8_t pred_flag;
#else
    uint8_t pred_flag;
#endif
    uint8_t ref_idx[2];
} MvField;

typedef struct NeighbourAvailable {
    int cand_bottom_left;
    int cand_left;
    int cand_up;
    int cand_up_left;
    int cand_up_right;
    int cand_up_right_sap;
} NeighbourAvailable;

typedef struct PredictionUnit {
    int mpm_idx;
    int rem_intra_luma_pred_mode;
    uint8_t intra_pred_mode[4];
    Mv mvd;
    uint8_t merge_flag;
    uint8_t intra_pred_mode_c[4];
    uint8_t chroma_mode_c[4];
} PredictionUnit;

typedef struct TransformUnit {
    DECLARE_ALIGNED(32, int16_t, coeffs[2][MAX_TB_SIZE * MAX_TB_SIZE]);

    int cu_qp_delta;

    int res_scale_val;

    // Inferred parameters;
    int intra_pred_mode;
    int intra_pred_mode_c;
    int chroma_mode_c;
    uint8_t is_cu_qp_delta_coded;
    uint8_t is_cu_chroma_qp_offset_coded;
    int8_t  cu_qp_offset_cb;
    int8_t  cu_qp_offset_cr;
    uint8_t cross_pf;
#if COM16_C806_EMT
    uint8_t emt_tu_idx;
#endif
} TransformUnit;

typedef struct DBParams {
    int8_t beta_offset;
    int8_t tc_offset;
} DBParams;

#define HEVC_FRAME_FLAG_OUTPUT    (1 << 0)
#define HEVC_FRAME_FLAG_SHORT_REF (1 << 1)
#define HEVC_FRAME_FLAG_LONG_REF  (1 << 2)
#define HEVC_FRAME_FLAG_BUMPING   (1 << 3)
#define HEVC_FRAME_FIRST_FIELD    (1 << 4)
#define MAX_SLICES_IN_FRAME       64
typedef struct HEVCFrame {
    AVFrame *frame;
    ThreadFrame tf;
    MvField *tab_mvf;
    RefPicList *refPicList[MAX_SLICES_IN_FRAME];
    RefPicListTab **rpl_tab;
    int ctb_count;
    int poc;
    struct HEVCFrame *collocated_ref;

    HEVCWindow window;

    AVBufferRef *tab_mvf_buf;
    AVBufferRef *rpl_tab_buf;
    AVBufferRef *rpl_buf;

    AVBufferRef *hwaccel_priv_buf;
    void *hwaccel_picture_private;

    /**
     * A sequence counter, so that old frames are output first
     * after a POC reset
     */
    uint16_t sequence;

    /**
     * A combination of HEVC_FRAME_FLAG_*
     */
    uint8_t flags;
    uint8_t field_order;
    enum NALUnitType nal_unit_type;
    uint8_t temporal_layer_id;
#if FRAME_CONCEALMENT
    uint8_t is_concealment_frame;
#endif

} HEVCFrame;

typedef struct HEVCNAL {
    uint8_t *rbsp_buffer;
    int rbsp_buffer_size;

    int size;
    const uint8_t *data;
} HEVCNAL;

typedef struct HEVCLocalContext {
    CodingTree          ct;
#ifdef USE_SAO_SMALL_BUFFER
    uint8_t *sao_pixel_buffer;
#endif
    uint8_t cabac_state[HEVC_CONTEXTS];

    uint8_t stat_coeff[4];

    uint8_t first_qp_group;

    GetBitContext gb;
    CABACContext cc;

    int8_t qp_y;
    int8_t curr_qp_y;

    int qPy_pred;

    TransformUnit tu;

    uint8_t ctb_left_flag;
    uint8_t ctb_up_flag;
    uint8_t ctb_up_right_flag;
    uint8_t ctb_up_left_flag;
    int     end_of_tiles_x;
    int     end_of_tiles_y;
    /* +7 is for subpixel interpolation, *2 for high bit depths */
    DECLARE_ALIGNED(32, uint8_t, edge_emu_buffer)[(MAX_PB_SIZE + 7) * EDGE_EMU_BUFFER_STRIDE * 2];
    DECLARE_ALIGNED(32, uint8_t, edge_emu_buffer2)[(MAX_PB_SIZE + 7) * EDGE_EMU_BUFFER_STRIDE * 2];
    DECLARE_ALIGNED(32, int16_t, tmp [MAX_PB_SIZE * MAX_PB_SIZE]);

    DECLARE_ALIGNED(32, int16_t, edge_emu_buffer_up_v[MAX_EDGE_BUFFER_SIZE]);
    DECLARE_ALIGNED(32, int16_t, edge_emu_buffer_up_h[MAX_EDGE_BUFFER_SIZE]);

    DECLARE_ALIGNED(32, int16_t, color_mapping_cgs_y[MAX_EDGE_BUFFER_SIZE]);
    DECLARE_ALIGNED(32, int16_t, color_mapping_cgs_u[MAX_EDGE_BUFFER_SIZE]);
    DECLARE_ALIGNED(32, int16_t, color_mapping_cgs_v[MAX_EDGE_BUFFER_SIZE]);
    uint8_t slice_or_tiles_left_boundary;
    uint8_t slice_or_tiles_up_boundary;

    int ctb_tile_rs;
#if HEVC_ENCRYPTION
    Crypto_Handle       dbs_g;
    uint32_t prev_pos;
#endif
    int ct_depth;
    CodingUnit cu;
    PredictionUnit pu;
    NeighbourAvailable na;

#define BOUNDARY_LEFT_SLICE     (1 << 0)
#define BOUNDARY_LEFT_TILE      (1 << 1)
#define BOUNDARY_UPPER_SLICE    (1 << 2)
#define BOUNDARY_UPPER_TILE     (1 << 3)
    /* properties of the boundary of the current CTB for the purposes
     * of the deblocking filter */
    int boundary_flags;
    int tile_id;
} HEVCLocalContext;

typedef struct HEVCContext {
    const AVClass *c;  // needed by private avoptions
    AVCodecContext *avctx;

    struct HEVCContext  *sList[MAX_NB_THREADS];

    HEVCLocalContext    *HEVClcList[MAX_NB_THREADS];
    HEVCLocalContext    *HEVClc;

    uint8_t             threads_type;
    uint8_t             threads_number;

    int                 width;
    int                 height;

    uint8_t *cabac_state;

    /** 1 if the independent slice segment header was successfully parsed */
    uint8_t slice_initialized;

    AVFrame *frame;
    AVFrame *output_frame;
#ifdef USE_SAO_SMALL_BUFFER
    uint8_t *sao_pixel_buffer_h[3];
    uint8_t *sao_pixel_buffer_v[3];
#else
    AVFrame *tmp_frame;
    AVFrame *sao_frame;
#endif

    HEVCParamSets ps;

    AVBufferPool *tab_mvf_pool;
    AVBufferPool *rpl_tab_pool;

    ///< candidate references for the current frame
    RefPicList rps[5+2]; // 2 for inter layer reference pictures

    SliceHeader sh;
    SAOParams *sao;
    DBParams *deblock;
    enum NALUnitType nal_unit_type;
    int temporal_id;  ///< temporal_id_plus1 - 1
    uint8_t force_first_slice_in_pic;
    int64_t last_frame_pts;
    HEVCFrame *ref;//fixme : //if base layer
    HEVCFrame DPB[32];
    HEVCFrame Add_ref[2];

    int au_poc;
    int poc;
    int poc_id;
    int poc_id2;
    int pocTid0;
    int slice_idx; ///< number of the slice being currently decoded
    int eos;       ///< current packet contains an EOS/EOB NAL
    int last_eos;  ///< last packet contains an EOS/EOB NAL
    int max_ra;
    int bs_width;
    int bs_height;

    int is_decoded;
    int no_rasl_output_flag;

    HEVCPredContext hpc;
    HEVCDSPContext hevcdsp;
    VideoDSPContext vdsp;
    BswapDSPContext bdsp;
    int8_t *qp_y_tab;
    uint8_t *horizontal_bs;
    uint8_t *vertical_bs;

    int32_t *tab_slice_address;

    //  CU
    uint8_t *skip_flag;
    uint8_t *tab_ct_depth;
    // PU
    uint8_t *tab_ipm;

#if HEVC_ENCRYPTION
    uint8_t *tab_ipm_encry;
#endif

    uint8_t *cbf_luma; // cbf_luma of colocated TU
    uint8_t *is_pcm;

    // CTB-level flags affecting loop filter operation
    uint8_t *filter_slice_edges;

    /** used on BE to byteswap the lines for checksumming */
    uint8_t *checksum_buf;
    int      checksum_buf_size;

    /**
     * Sequence counters for decoded and output frames, so that old
     * frames are output first after a POC reset
     */
    uint16_t seq_decode;
    uint16_t seq_output;

    int enable_parallel_tiles;
    int wpp_err;
    int skipped_bytes;
    int *skipped_bytes_pos;
    int skipped_bytes_pos_size;

    int *skipped_bytes_nal;
    int **skipped_bytes_pos_nal;
    int *skipped_bytes_pos_size_nal;

    const uint8_t *data;

    H2645Packet pkt;
    HEVCNAL *nals;
    int nb_nals;
    int nals_allocated;
    // type of the first VCL NAL of the current frame
    enum NALUnitType first_nal_type;

    // for checking the frame checksums
    struct AVMD5 *md5_ctx;
    uint8_t       md5[3][16];
    uint8_t is_md5;

    uint8_t context_initialized;
    uint8_t is_nalff;       ///< this flag is != 0 if bitstream is encapsulated
                            ///< as a format defined in 14496-15
    int apply_defdispwin;


    AVFrame     *EL_frame;
    short       *buffer_frame[3];
    UpsamplInf  up_filter_inf;
    //BL_frame is void * since BL can also be H264 or others
    void   *BL_frame;
    HEVCFrame   *inter_layer_ref;

    uint8_t         bl_decoder_el_exist;
    uint8_t         el_decoder_el_exist; // wheither the el exist or not at the el decoder
    uint8_t         el_decoder_bl_exist;
    uint8_t     *is_upsampled;
#if !ACTIVE_PU_UPSAMPLING || ACTIVE_BOTH_FRAME_AND_PU
    AVFrame *Ref_color_mapped_frame;
#endif
    int temporal_layer_id;
    int decoder_id;
    int quality_layer_id;
    int active_seq_parameter_set_id;

    int nal_length_size;    ///< Number of bytes used for nal length (1, 2 or 4)
    int nuh_layer_id;

    /** frame packing arrangement variables */
    int sei_frame_packing_present;
    int frame_packing_arrangement_type;
    int content_interpretation_type;
    int quincunx_subsampling;

    /** display orientation */
    int sei_display_orientation_present;
    int sei_anticlockwise_rotation;
    int sei_hflip, sei_vflip;

    int picture_struct;
    int field_order;
    int interlaced;

    uint8_t* a53_caption;
    int a53_caption_size;

    /** mastering display */
    int sei_mastering_display_info_present;
    uint16_t display_primaries[3][2];
    uint16_t white_point[2];
    uint32_t max_mastering_luminance;
    uint32_t min_mastering_luminance;

#if FRAME_CONCEALMENT
    int prev_display_poc;
    int no_display_pic;
#endif
    int     decode_checksum_sei;

#if PARALLEL_SLICE
    int NALListOrder[MAX_SLICES_FRAME];
    int slice_segment_addr[MAX_SLICES_FRAME]; 
    int NbListElement;
    int self_id;
    int job;
    int max_slices;
    uint8_t *decoded_rows; 
#endif

	//uint8_t force_first_slice_in_pic;
	//int64_t last_frame_pts;
    int BL_width;
    int BL_height;
#if HEVC_ENCRYPTION
    uint8_t encrypt_params;
    uint8_t *encrypt_init_val;
    int encrypt_init_val_length;
    AVRational last_click_pos;
    uint8_t prev_num_tile_rows;
    uint8_t prev_num_tile_columns;
    uint8_t *tile_table_encry;
#endif
} HEVCContext;

int ff_hevc_decode_short_term_rps(GetBitContext *gb, AVCodecContext *avctx,
                                  ShortTermRPS *rps, const HEVCSPS *sps, int is_slice_header);

/**
 * Parse the SPS from the bitstream into the provided HEVCSPS struct.
 *
 * @param sps_id the SPS id will be written here
 * @param apply_defdispwin if set 1, the default display window from the VUI
 *                         will be applied to the video dimensions
 * @param vps_list if non-NULL, this function will validate that the SPS refers
 *                 to an existing VPS
 */
int ff_hevc_parse_sps(HEVCSPS *sps, GetBitContext *gb, unsigned int *sps_id,
                      int apply_defdispwin, AVBufferRef **vps_list, AVCodecContext *avctx, int nuh_layer_id);

int ff_hevc_decode_nal_vps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps);
int ff_hevc_decode_nal_sps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps, int apply_defdispwin, int nuh_layer_id);
int ff_hevc_decode_nal_pps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps);
int ff_hevc_decode_nal_sei(HEVCContext *s);

int ff_hevc_extract_rbsp(HEVCContext *s, const uint8_t *src, int length,
                         HEVCNAL *nal);

/**
 * Mark all frames in DPB as unused for reference.
 */
void ff_hevc_clear_refs(HEVCContext *s);

/**
 * Drop all frames currently in DPB.
 */
void ff_hevc_flush_dpb(HEVCContext *s);

/**
 * Compute POC of the current frame and return it.
 */
int ff_hevc_compute_poc(HEVCContext *s, int poc_lsb);

RefPicList *ff_hevc_get_ref_list(HEVCContext *s, HEVCFrame *frame,
                                 int x0, int y0);

/**
 * Construct the reference picture sets for the current frame.
 */
int ff_hevc_frame_rps(HEVCContext *s);

/**
 * Construct the reference picture list(s) for the current slice.
 */
int ff_hevc_slice_rpl(HEVCContext *s);

void ff_hevc_save_states(HEVCContext *s, int ctb_addr_ts);
void ff_hevc_cabac_init(HEVCContext *s, int ctb_addr_ts);
int ff_hevc_sao_merge_flag_decode(HEVCContext *s);
int ff_hevc_sao_type_idx_decode(HEVCContext *s);
uint8_t ff_hevc_sao_band_position_decode(HEVCContext *s);
uint8_t ff_hevc_sao_offset_abs_decode(HEVCContext *s);
uint8_t ff_hevc_sao_offset_sign_decode(HEVCContext *s);
uint8_t ff_hevc_sao_eo_class_decode(HEVCContext *s);
int ff_hevc_end_of_slice_flag_decode(HEVCContext *s);
int ff_hevc_cu_transquant_bypass_flag_decode(HEVCContext *s);
int ff_hevc_skip_flag_decode(HEVCContext *s, int x0, int y0,
                             int x_cb, int y_cb);

#if COM16_C806_EMT
uint8_t ff_hevc_emt_cu_flag_decode(HEVCContext *s, int log2_cb_size, int cbfLuma);
uint8_t ff_hevc_emt_tu_idx_decode(HEVCContext *s, int log2_cb_size);
#endif

int ff_hevc_pred_mode_decode(HEVCContext *s);
int ff_hevc_split_coding_unit_flag_decode(HEVCContext *s, int ct_depth,
                                          int x0, int y0);
int ff_hevc_part_mode_decode(HEVCContext *s, int log2_cb_size);
int ff_hevc_pcm_flag_decode(HEVCContext *s);
int ff_hevc_prev_intra_luma_pred_flag_decode(HEVCContext *s);
int ff_hevc_mpm_idx_decode(HEVCContext *s);
int ff_hevc_rem_intra_luma_pred_mode_decode(HEVCContext *s);
int ff_hevc_intra_chroma_pred_mode_decode(HEVCContext *s);
int ff_hevc_merge_idx_decode(HEVCContext *s);
int ff_hevc_merge_flag_decode(HEVCContext *s);
int ff_hevc_inter_pred_idc_decode(HEVCContext *s, int nPbW, int nPbH);
int ff_hevc_ref_idx_lx_decode(HEVCContext *s, int num_ref_idx_lx);
int ff_hevc_mvp_lx_flag_decode(HEVCContext *s);
int ff_hevc_no_residual_syntax_flag_decode(HEVCContext *s);
int ff_hevc_split_transform_flag_decode(HEVCContext *s, int log2_trafo_size);
int ff_hevc_cbf_cb_cr_decode(HEVCContext *s, int trafo_depth);
int ff_hevc_cbf_luma_decode(HEVCContext *s, int trafo_depth);
int ff_hevc_log2_res_scale_abs(HEVCContext *s, int idx);
int ff_hevc_res_scale_sign_flag(HEVCContext *s, int idx);

/**
 * Get the number of candidate references for the current frame.
 */
int ff_hevc_frame_nb_refs(HEVCContext *s);

int ff_hevc_set_new_ref(HEVCContext *s, AVFrame **frame, int poc);
int ff_hevc_set_new_iter_layer_ref(HEVCContext *s, AVFrame **frame, int poc);

SYUVP  GetCuboidVertexPredAll(TCom3DAsymLUT * pc3DAsymLUT, int yIdx , int uIdx , int vIdx , int nVertexIdx);
/**
 * Find next frame in output order and put a reference to it in frame.
 * @return 1 if a frame was output, 0 otherwise
 */
int ff_hevc_output_frame(HEVCContext *s, AVFrame *frame, int flush);

void ff_hevc_bump_frame(HEVCContext *s);

void ff_hevc_unref_frame(HEVCContext *s, HEVCFrame *frame, int flags);

void ff_hevc_set_neighbour_available(HEVCContext *s, int x0, int y0,
                                     int nPbW, int nPbH);
void ff_hevc_luma_mv_merge_mode(HEVCContext *s, int x0, int y0,
                                int nPbW, int nPbH, int log2_cb_size,
                                int part_idx, int merge_idx, MvField *mv);
void ff_hevc_luma_mv_mvp_mode(HEVCContext *s, int x0, int y0,
                              int nPbW, int nPbH, int log2_cb_size,
                              int part_idx, int merge_idx,
                              MvField *mv, int mvp_lx_flag, int LX);
void ff_hevc_set_qPy(HEVCContext *s, int xBase, int yBase,
                     int log2_cb_size);
void ff_hevc_deblocking_boundary_strengths(HEVCContext *s, int x0, int y0,
                                           int log2_trafo_size);
void ff_hevc_deblocking_boundary_strengths_h(HEVCContext *s, int x0, int y0,
                                           int slice_up_boundary);
void ff_hevc_deblocking_boundary_strengths_v(HEVCContext *s, int x0, int y0,
                                           int slice_left_boundary);

void ff_upscale_mv_block(HEVCContext *s, int ctb_x, int ctb_y);
int set_el_parameter(HEVCContext *s);
int ff_hevc_cu_qp_delta_sign_flag(HEVCContext *s);
int ff_hevc_cu_qp_delta_abs(HEVCContext *s);
int ff_hevc_cu_chroma_qp_offset_flag(HEVCContext *s);
int ff_hevc_cu_chroma_qp_offset_idx(HEVCContext *s);
void ff_hevc_hls_filter(HEVCContext *s, int x, int y, int ctb_size);
void ff_hevc_hls_filters(HEVCContext *s, int x_ctb, int y_ctb, int ctb_size);
#if PARALLEL_FILTERS
void ff_hevc_hls_filters_slice( HEVCContext *s, int x_ctb, int y_ctb, int ctb_size);
void ff_hevc_hls_filter_slice(  HEVCContext *s, int x, int y, int ctb_size);
#endif
void ff_upsample_block(HEVCContext *s, HEVCFrame *ref0, int x0, int y0, int nPbW, int nPbH);
void ff_hevc_hls_residual_coding(HEVCContext *s, int x0, int y0,
                                 int log2_trafo_size, enum ScanType scan_idx,
                                 int c_idx
#if COM16_C806_EMT
                                 , int log2_cb_size
#endif
);

void ff_hevc_hls_mvd_coding(HEVCContext *s, int x0, int y0, int log2_cb_size);


int ff_hevc_encode_nal_vps(HEVCVPS *vps, unsigned int id,
                           uint8_t *buf, int buf_size);

/**
 * Reset SEI values that are stored on the Context.
 * e.g. Caption data that was extracted during NAL
 * parsing.
 *
 * @param s HEVCContext.
 */
void ff_hevc_reset_sei(HEVCContext *s);
void Free3DArray(HEVCPPS * pc3DAsymLUT);

extern const uint8_t ff_hevc_qpel_extra_before[4];
extern const uint8_t ff_hevc_qpel_extra_after[4];
extern const uint8_t ff_hevc_qpel_extra[4];

extern const uint8_t ff_hevc_diag_scan4x4_x[16];
extern const uint8_t ff_hevc_diag_scan4x4_y[16];
extern const uint8_t ff_hevc_diag_scan8x8_x[64];
extern const uint8_t ff_hevc_diag_scan8x8_y[64];
 int setup_pps(AVCodecContext *avctx,
                            HEVCPPS *pps, HEVCSPS *sps);
#endif /* AVCODEC_HEVC_H */
