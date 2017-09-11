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

#ifndef AVCODEC_HEVCDEC_H
#define AVCODEC_HEVCDEC_H

#include <stdatomic.h>

#include "libavutil/buffer.h"

#include "config.h"

#include "avcodec.h"
#include "bswapdsp.h"
#include "cabac.h"
#include "get_bits.h"
#include "hevcpred.h"
#include "h2645_parse.h"
#include "hevc.h"
#include "hevc_ps.h"
#include "hevc_sei.h"
#include "hevcdsp.h"
#include "internal.h"
#include "thread.h"
#include "videodsp.h"
#include "hevc_defs.h"
#if OHCONFIG_ENCRYPTION
#include "crypto.h"
#endif
#include "hevcdsp.h"
#define MAX_TB_SIZE 32
#define MAX_PB_SIZE 64

#define MAX_NB_THREADS 16
#define SHIFT_CTB_WPP 2
#define MAX_POC      1024


//TODO: check if this is really the maximum
#define MAX_TRANSFORM_DEPTH 5
#define MIN_PB_LOG_SIZE 2

#define MAX_EDGE_BUFFER_SIZE    ((MAX_PB_SIZE + 20) * (MAX_PB_SIZE+20) * 2)
#define MAX_EDGE_BUFFER_STRIDE  ((MAX_PB_SIZE + 20) * 2)

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

#define IS_IDR(s) ((s)->nal_unit_type == HEVC_NAL_IDR_W_RADL || (s)->nal_unit_type == HEVC_NAL_IDR_N_LP)
#define IS_BLA(s) ((s)->nal_unit_type == HEVC_NAL_BLA_W_RADL || (s)->nal_unit_type == HEVC_NAL_BLA_W_LP || \
                   (s)->nal_unit_type == HEVC_NAL_BLA_N_LP)
#define IS_IRAP(s) ((s)->nal_unit_type >= 16 && (s)->nal_unit_type <= 23)

#if OHCONFIG_ENCRYPTION
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
#if OHCONFIG_AMT
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

typedef struct RefPicList {
    struct HEVCFrame *ref[HEVC_MAX_REFS];
    int list[HEVC_MAX_REFS];
    int isLongTerm[HEVC_MAX_REFS];
    int nb_refs;
} RefPicList;

typedef struct RefPicListTab {
    RefPicList refPicList[2];
} RefPicListTab;

typedef struct CodingUnit {
    int x;
    int y;

    enum PredMode pred_mode;    ///< PredMode
    enum PartMode part_mode;    ///< PartMode

    uint8_t rqt_root_cbf;

    uint8_t pcm_flag;
#if OHCONFIG_AMT
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
#if OHCONFIG_AMT
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
    enum HEVCNALUnitType nal_unit_type;
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
#if OHCONFIG_ENCRYPTION
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
    enum HEVCNALUnitType nal_unit_type;
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

#if OHCONFIG_ENCRYPTION
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
    atomic_int wpp_err;


    const uint8_t *data;

    H2645Packet pkt;
    // type of the first VCL NAL of the current frame
    enum HEVCNALUnitType first_nal_type;

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

    HEVCSEIContext sei;

    int picture_struct;
    int field_order;
    int interlaced;

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
    int BL_width;
    int BL_height;
    int bl_available;
#if OHCONFIG_ENCRYPTION
    uint8_t encrypt_params;
    AVRational last_click_pos;
    uint8_t prev_num_tile_rows;
    uint8_t prev_num_tile_columns;
    uint8_t *tile_table_encry;
    uint8_t *encrypt_init_val;
    int encrypt_init_val_length;
#endif
} HEVCContext;

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
int ff_hevc_cabac_init(HEVCContext *s, int ctb_addr_ts);
int ff_hevc_sao_merge_flag_decode(HEVCContext *s);
int ff_hevc_sao_type_idx_decode(HEVCContext *s);
int ff_hevc_sao_band_position_decode(HEVCContext *s);
int ff_hevc_sao_offset_abs_decode(HEVCContext *s);
int ff_hevc_sao_offset_sign_decode(HEVCContext *s);
int ff_hevc_sao_eo_class_decode(HEVCContext *s);
int ff_hevc_end_of_slice_flag_decode(HEVCContext *s);
int ff_hevc_cu_transquant_bypass_flag_decode(HEVCContext *s);
int ff_hevc_skip_flag_decode(HEVCContext *s, int x0, int y0,
                             int x_cb, int y_cb);

#if OHCONFIG_AMT
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
#if OHCONFIG_AMT
                                 , int log2_cb_size
#endif
);

void ff_hevc_hls_mvd_coding(HEVCContext *s, int x0, int y0, int log2_cb_size);

int ff_hevc_extract_rbsp(HEVCContext *s, const uint8_t *src, int length,
                         HEVCNAL *nal);


extern const uint8_t ff_hevc_qpel_extra_before[4];
extern const uint8_t ff_hevc_qpel_extra_after[4];
extern const uint8_t ff_hevc_qpel_extra[4];

#endif /* AVCODEC_HEVCDEC_H */

