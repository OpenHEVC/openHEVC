/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2012, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TypeDef.h
    \brief    Define basic types, new types and enumerations
*/

#ifndef _TYPEDEF__
#define _TYPEDEF__



//! \ingroup TLibCommon
//! \{

#define SAO_LUM_CHROMA_ONOFF_FLAGS       1  ///< J0087: slice-level independent luma/chroma SAO on/off flag 
#define LTRP_IN_SPS                      1  ///< J0116: Include support for signalling LTRP LSBs in the SPS, and index them in the slice header.
#define CHROMA_QP_EXTENSION              1  ///< J0342: Extend mapping table from luma QP to chroma QP, introduce slice-level chroma offsets, apply limits on offset values
#define SIMPLE_LUMA_CBF_CTX_DERIVATION   1  ///< J0303: simplified luma_CBF context derivation

#define COEF_REMAIN_BIN_REDUCTION        3 ///< J0142: Maximum codeword length of coeff_abs_level_remaining reduced to 32.
                                           ///< COEF_REMAIN_BIN_REDUCTION is also used to indicate the level at which the VLC 
                                           ///< transitions from Golomb-Rice to TU+EG(k)

#define CU_DQP_TU_EG                     1 ///< J0089: Bin reduction for delta QP coding
#if (CU_DQP_TU_EG)
#define CU_DQP_TU_CMAX 5 //max number bins for truncated unary
#define CU_DQP_EG_k 0 //expgolomb order
#endif

#define NAL_UNIT_HEADER                  1  ///< J0550: Define nal_unit_header() method
#define REMOVE_NAL_REF_FLAG              1  ///< J0550: Remove nal_ref_flag, and allocate extra bit to reserved bits, and re-order syntax to put reserved bits after nal_unit_type
#define TEMPORAL_ID_PLUS1                1  ///< J0550: Signal temporal_id_plus1 instead of temporal_id in NAL unit, and change reserved_one_5bits
                                            ///<        value to zero
#define REFERENCE_PICTURE_DEFN           1  ///< J0118: Reflect change of defn. of referece picture in semantics of delta_poc_msb_present_flag
#define MOVE_LOOP_FILTER_SLICES_FLAG     1  ///< J0288: Move seq_loop_filter_across_slices_enabled_flag from SPS to PPS
#define SPLICING_FRIENDLY_PARAMS         1  ///< J0108: Remove rap_pic_id and move no_output_prior_pic_flag

#define  SKIP_FLAG                       1  ///< J0336: store skip flag

#define PPS_TS_FLAG                      1  ///< J0184: move transform_skip_enabled_flag from SPS to PPS
#if PPS_TS_FLAG
#define TS_FLAT_QUANTIZATION_MATRIX      1  ///< I0408: set default quantization matrix to be flat if TS is enabled in PPS
#endif
#define INTER_TRANSFORMSKIP              1  ///< J0237: inter transform skipping (inter-TS)
#define INTRA_TRANSFORMSKIP_FAST         1  ///< J0572: fast encoding for intra transform skipping

#define REMOVAL_8x2_2x8_CG               1  ///< J0256: removal of 8x2 / 2x8 coefficient groups
#define REF_IDX_BYPASS                   1  ///< J0098: bypass coding starting from the second bin for reference index

#define RECALCULATE_QP_ACCORDING_LAMBDA  1  ///< J0242: recalculate QP value according to lambda value
#define TU_ZERO_CBF_RDO                  1  ///< J0241: take the bits to represent zero cbf into consideration when doing TU RDO
#define REMOVE_NUM_GREATER1              1  ///< J0408: numGreater1 removal and ctxset decision with c1 

#define INTRA_TRANS_SIMP                 1  ///< J0035: Use DST for 4x4 luma intra TU's (regardless of the intra prediction direction)

#define J0234_INTER_RPS_SIMPL            1  ///< J0234: Do not signal delta_idx_minus1 when building the RPS-list in SPS
#define NUM_WP_LIMIT                     1  ///< J0571: number of total signalled weight flags <=24
#define DISALLOW_BIPRED_IN_8x4_4x8PUS    1  ///< J0086: disallow bi-pred for 8x4 and 4x8 inter PUs
#define SAO_SINGLE_MERGE                 1  ///< J0355: Single SAO merge flag for all color components (per Left and Up merge)
#define SAO_TYPE_SHARING                 1  ///< J0045: SAO types, merge left/up flags are shared between Cr and Cb
#define SAO_TYPE_CODING                  1  ///< J0268: SAO type signalling using 1 ctx on/off flag + 1 bp BO/EO flag + 2 bp bins for EO class
#define SAO_MERGE_ONE_CTX                1  ///< J0041: SAO merge left/up flags share the same ctx
#define SAO_ABS_BY_PASS                  1  ///< J0043: by pass coding for SAO magnitudes 
#define SAO_LCU_BOUNDARY                 1  ///< J0139: SAO parameter estimation using non-deblocked pixels for LCU bottom and right boundary areas
#define MODIFIED_CROSS_SLICE             1  ///< J0266: SAO slice boundary control for GDR
#define CU_DQP_ENABLE_FLAG               1  ///< J0220: cu_qp_delta_enabled_flag in PPS
#define REMOVE_ZIGZAG_SCAN               1  ///< J0150: removal of zigzag scan

#define TRANS_SPLIT_FLAG_CTX_REDUCTION   1  ///< J0133: Reduce the context number of transform split flag to 3

#define WP_PARAM_RANGE_LIMIT             1  ///< J0221: Range limit of delta_weight and delta_offset for chroma.
#define J0260 1 ///< Fix in rate control equations

#define SLICE_HEADER_EXTENSION           1  ///< II0235: Slice header extension mechanism

#define REMOVE_NSQT 1 ///< Disable NSQT-related code
#define REMOVE_LMCHROMA 1 ///< Disable LM_Chroma-related code
#define REMOVE_FGS 1 ///< Disable fine-granularity slices code
#define REMOVE_ALF 1 ///< Disable ALF-related code
#define REMOVE_APS 1 ///< Disable APS-related code

#define PREVREFPIC_DEFN                  0  ///< J0248: Shall be set equal to 0! (prevRefPic definition reverted to CD definition)
#define BYTE_ALIGNMENT                   1  ///< I0330: Add byte_alignment() procedure to end of slice header

#define SBH_THRESHOLD                    4  ///< I0156: value of the fixed SBH controlling threshold
  
#define SEQUENCE_LEVEL_LOSSLESS           0  ///< H0530: used only for sequence or frame-level lossless coding

#define DISABLING_CLIP_FOR_BIPREDME         1  ///< Ticket #175
  
#define C1FLAG_NUMBER               8 // maximum number of largerThan1 flag coded in one chunk :  16 in HM5
#define C2FLAG_NUMBER               1 // maximum number of largerThan2 flag coded in one chunk:  16 in HM5 

#define REMOVE_SAO_LCU_ENC_CONSTRAINTS_3 1  ///< disable the encoder constraint that conditionally disable SAO for chroma for entire slice in interleaved mode

#define SAO_SKIP_RIGHT                   1  ///< H1101: disallow using unavailable pixel during RDO

#define SAO_ENCODING_CHOICE              1  ///< I0184: picture early termination
#define PICTURE_SAO_RDO_FIX              0  ///< J0097: picture-based SAO optimization fix
#if SAO_ENCODING_CHOICE
#define SAO_ENCODING_RATE                0.75
#define SAO_ENCODING_CHOICE_CHROMA       1 ///< J0044: picture early termination Luma and Chroma are handled separatenly
#if SAO_ENCODING_CHOICE_CHROMA
#define SAO_ENCODING_RATE_CHROMA         0.5
#endif
#endif

#define MAX_NUM_SPS                32
#define MAX_NUM_PPS                256
#define MAX_NUM_APS                32         //< !!!KS: number not defined in WD yet

#define MRG_MAX_NUM_CANDS_SIGNALED         5   //<G091: value of maxNumMergeCand signaled in slice header 

#define WEIGHTED_CHROMA_DISTORTION  1   ///< F386: weighting of chroma for RDO
#define RDOQ_CHROMA_LAMBDA          1   ///< F386: weighting of chroma for RDOQ
#define ALF_CHROMA_LAMBDA           1   ///< F386: weighting of chroma for ALF
#define SAO_CHROMA_LAMBDA           1   ///< F386: weighting of chroma for SAO

#define MIN_SCAN_POS_CROSS          4

#define FAST_BIT_EST                1   ///< G763: Table-based bit estimation for CABAC

#define MLS_GRP_NUM                         64     ///< G644 : Max number of coefficient groups, max(16, 64)
#define MLS_CG_SIZE                         4      ///< G644 : Coefficient group size of 4x4

#define ADAPTIVE_QP_SELECTION               1      ///< G382: Adaptive reconstruction levels, non-normative part for adaptive QP selection
#if ADAPTIVE_QP_SELECTION
#define ARL_C_PRECISION                     7      ///< G382: 7-bit arithmetic precision
#define LEVEL_RANGE                         30     ///< G382: max coefficient level in statistics collection
#endif

#if REMOVE_NSQT
#define NS_HAD                               0
#else
#define NS_HAD                               1
#endif

#define APS_BITS_FOR_SAO_BYTE_LENGTH 12           
#define APS_BITS_FOR_ALF_BYTE_LENGTH 8

#define HHI_RQT_INTRA_SPEEDUP             1           ///< tests one best mode with full rqt
#define HHI_RQT_INTRA_SPEEDUP_MOD         0           ///< tests two best modes with full rqt

#if HHI_RQT_INTRA_SPEEDUP_MOD && !HHI_RQT_INTRA_SPEEDUP
#error
#endif

#define VERBOSE_RATE 0 ///< Print additional rate information in encoder

#define AMVP_DECIMATION_FACTOR            4

#define SCAN_SET_SIZE                     16
#define LOG2_SCAN_SET_SIZE                4

#define FAST_UDI_MAX_RDMODE_NUM               35          ///< maximum number of RD comparison in fast-UDI estimation loop 

#define ZERO_MVD_EST                          0           ///< Zero Mvd Estimation in normal mode

#define NUM_INTRA_MODE 36
#if !REMOVE_LM_CHROMA
#define LM_CHROMA_IDX  35
#endif

#define IBDI_DISTORTION                0           ///< enable/disable SSE modification when IBDI is used (JCTVC-D152)
#define FIXED_ROUNDING_FRAME_MEMORY    0           ///< enable/disable fixed rounding to 8-bitdepth of frame memory when IBDI is used  

#define WRITE_BACK                      1           ///< Enable/disable the encoder to replace the deltaPOC and Used by current from the config file with the values derived by the refIdc parameter.
#define AUTO_INTER_RPS                  1           ///< Enable/disable the automatic generation of refIdc from the deltaPOC and Used by current from the config file.
#define PRINT_RPS_INFO                  0           ///< Enable/disable the printing of bits used to send the RPS.
                                                    // using one nearest frame as reference frame, and the other frames are high quality (POC%4==0) frames (1+X)
                                                    // this should be done with encoder only decision
                                                    // but because of the absence of reference frame management, the related code was hard coded currently

#define RVM_VCEGAM10_M 4

#define PLANAR_IDX             0
#define VER_IDX                26                    // index for intra VERTICAL   mode
#define HOR_IDX                10                    // index for intra HORIZONTAL mode
#define DC_IDX                 1                     // index for intra DC mode
#if REMOVE_LMCHROMA
#define NUM_CHROMA_MODE        5                     // total number of chroma modes
#else
#define NUM_CHROMA_MODE        6                     // total number of chroma modes
#endif
#define DM_CHROMA_IDX          36                    // chroma mode index for derived from luma intra mode


#define FAST_UDI_USE_MPM 1

#define RDO_WITHOUT_DQP_BITS              0           ///< Disable counting dQP bits in RDO-based mode decision

#define FULL_NBIT 0 ///< When enabled, does not use g_uiBitIncrement anymore to support > 8 bit data

#define AD_HOC_SLICES_FIXED_NUMBER_OF_LCU_IN_SLICE      1          ///< OPTION IDENTIFIER. mode==1 -> Limit maximum number of largest coding tree blocks in a slice
#define AD_HOC_SLICES_FIXED_NUMBER_OF_BYTES_IN_SLICE    2          ///< OPTION IDENTIFIER. mode==2 -> Limit maximum number of bins/bits in a slice
#define AD_HOC_SLICES_FIXED_NUMBER_OF_TILES_IN_SLICE    3

#define DEPENDENT_SLICES       1 ///< JCTVC-I0229
// Dependent slice options
#define SHARP_FIXED_NUMBER_OF_LCU_IN_DEPENDENT_SLICE            1          ///< OPTION IDENTIFIER. Limit maximum number of largest coding tree blocks in an dependent slice
#define SHARP_MULTIPLE_CONSTRAINT_BASED_DEPENDENT_SLICE         2          ///< OPTION IDENTIFIER. Limit maximum number of bins/bits in an dependent slice
#if DEPENDENT_SLICES
#define FIXED_NUMBER_OF_TILES_IN_DEPENDENT_SLICE          3 // JCTVC-I0229
#endif

#define LOG2_MAX_NUM_COLUMNS_MINUS1        7
#define LOG2_MAX_NUM_ROWS_MINUS1           7
#define LOG2_MAX_COLUMN_WIDTH              13
#define LOG2_MAX_ROW_HEIGHT                13

#define MATRIX_MULT                             0   // Brute force matrix multiplication instead of partial butterfly

#define REG_DCT 65535

#define AMP_SAD                               1           ///< dedicated SAD functions for AMP
#define AMP_ENC_SPEEDUP                       1           ///< encoder only speed-up by AMP mode skipping
#if AMP_ENC_SPEEDUP
#define AMP_MRG                               1           ///< encoder only force merge for AMP partition (no motion search for AMP)
#endif

#define SCALING_LIST_OUTPUT_RESULT    0 //JCTVC-G880/JCTVC-G1016 quantization matrices

#define CABAC_INIT_PRESENT_FLAG     1

// ====================================================================================================================
// VPS constants
// ====================================================================================================================
#define MAX_LAYER_NUM                     10
#define MAX_NUM_VPS                16

// ====================================================================================================================
// Basic type redefinition
// ====================================================================================================================

typedef       void                Void;
typedef       bool                Bool;

typedef       char                Char;
typedef       unsigned char       UChar;
typedef       short               Short;
typedef       unsigned short      UShort;
typedef       int                 Int;
typedef       unsigned int        UInt;
typedef       double              Double;

// ====================================================================================================================
// 64-bit integer type
// ====================================================================================================================

#ifdef _MSC_VER
typedef       __int64             Int64;

#if _MSC_VER <= 1200 // MS VC6
typedef       __int64             UInt64;   // MS VC6 does not support unsigned __int64 to double conversion
#else
typedef       unsigned __int64    UInt64;
#endif

#else

typedef       long long           Int64;
typedef       unsigned long long  UInt64;

#endif

// ====================================================================================================================
// Type definition
// ====================================================================================================================

typedef       UChar           Pxl;        ///< 8-bit pixel type
typedef       Short           Pel;        ///< 16-bit pixel type
typedef       Int             TCoeff;     ///< transform coefficient

/// parameters for adaptive loop filter
class TComPicSym;

#define NUM_DOWN_PART 4

enum SAOTypeLen
{
  SAO_EO_LEN    = 4, 
  SAO_BO_LEN    = 4,
  SAO_MAX_BO_CLASSES = 32
};

enum SAOType
{
  SAO_EO_0 = 0, 
  SAO_EO_1,
  SAO_EO_2, 
  SAO_EO_3,
  SAO_BO,
  MAX_NUM_SAO_TYPE
};

typedef struct _SaoQTPart
{
  Int         iBestType;
  Int         iLength;
#if SAO_TYPE_CODING
  Int         subTypeIdx ;                 ///< indicates EO class or BO band position
#else
  Int         bandPosition ;
#endif
  Int         iOffset[4];
  Int         StartCUX;
  Int         StartCUY;
  Int         EndCUX;
  Int         EndCUY;

  Int         PartIdx;
  Int         PartLevel;
  Int         PartCol;
  Int         PartRow;

  Int         DownPartsIdx[NUM_DOWN_PART];
  Int         UpPartIdx;

  Bool        bSplit;

  //---- encoder only start -----//
  Bool        bProcessed;
  Double      dMinCost;
  Int64       iMinDist;
  Int         iMinRate;
  //---- encoder only end -----//
} SAOQTPart;

typedef struct _SaoLcuParam
{
  Bool       mergeUpFlag;
  Bool       mergeLeftFlag;
  Int        typeIdx;
#if SAO_TYPE_CODING
  Int        subTypeIdx;                  ///< indicates EO class or BO band position
#else
  Int        bandPosition;
#endif
  Int        offset[4];
  Int        partIdx;
  Int        partIdxTmp;
  Int        length;
} SaoLcuParam;

struct SAOParam
{
#if SAO_TYPE_SHARING
  Bool       bSaoFlag[2];
#else
  Bool       bSaoFlag[3];
#endif
  SAOQTPart* psSaoPart[3];
  Int        iMaxSplitLevel;
  Int        iNumClass[MAX_NUM_SAO_TYPE];
  Bool         oneUnitFlag[3];
  SaoLcuParam* saoLcuParam[3];
  Int          numCuInHeight;
  Int          numCuInWidth;
  ~SAOParam();
};

#if !REMOVE_ALF
struct ALFParam
{
  Int alf_flag;                           ///< indicates use of ALF
  Int num_coeff;                          ///< number of filter coefficients
  Int filter_shape;
  Int *filterPattern;
  Int startSecondFilter;
  Int filters_per_group;
  Int **coeffmulti;
  Int componentID;
  //constructor, operator
  ALFParam():componentID(-1){}
  ALFParam(Int cID){create(cID);}
  ALFParam(const ALFParam& src) {*this = src;}
  ~ALFParam(){destroy();}
  const ALFParam& operator= (const ALFParam& src);
private:
  Void create(Int cID);
  Void destroy();
  Void copy(const ALFParam& src);
};
#endif

/// parameters for deblocking filter
typedef struct _LFCUParam
{
  Bool bInternalEdge;                     ///< indicates internal edge
  Bool bLeftEdge;                         ///< indicates left edge
  Bool bTopEdge;                          ///< indicates top edge
} LFCUParam;

// ====================================================================================================================
// Enumeration
// ====================================================================================================================

/// supported slice type
enum SliceType
{
  B_SLICE,
  P_SLICE,
  I_SLICE
};

/// chroma formats (according to semantics of chroma_format_idc)
enum ChromaFormat
{
  CHROMA_400  = 0,
  CHROMA_420  = 1,
  CHROMA_422  = 2,
  CHROMA_444  = 3
};

/// supported partition shape
enum PartSize
{
  SIZE_2Nx2N,           ///< symmetric motion partition,  2Nx2N
  SIZE_2NxN,            ///< symmetric motion partition,  2Nx N
  SIZE_Nx2N,            ///< symmetric motion partition,   Nx2N
  SIZE_NxN,             ///< symmetric motion partition,   Nx N
  SIZE_2NxnU,           ///< asymmetric motion partition, 2Nx( N/2) + 2Nx(3N/2)
  SIZE_2NxnD,           ///< asymmetric motion partition, 2Nx(3N/2) + 2Nx( N/2)
  SIZE_nLx2N,           ///< asymmetric motion partition, ( N/2)x2N + (3N/2)x2N
  SIZE_nRx2N,           ///< asymmetric motion partition, (3N/2)x2N + ( N/2)x2N
  SIZE_NONE = 15
};

/// supported prediction type
enum PredMode
{
  MODE_INTER,           ///< inter-prediction mode
  MODE_INTRA,           ///< intra-prediction mode
  MODE_NONE = 15
};

/// texture component type
enum TextType
{
  TEXT_LUMA,            ///< luma
  TEXT_CHROMA,          ///< chroma (U+V)
  TEXT_CHROMA_U,        ///< chroma U
  TEXT_CHROMA_V,        ///< chroma V
  TEXT_ALL,             ///< Y+U+V
  TEXT_NONE = 15
};

/// reference list index
enum RefPicList
{
  REF_PIC_LIST_0 = 0,   ///< reference list 0
  REF_PIC_LIST_1 = 1,   ///< reference list 1
  REF_PIC_LIST_C = 2,   ///< combined reference list for uni-prediction in B-Slices
  REF_PIC_LIST_X = 100  ///< special mark
};

/// distortion function index
enum DFunc
{
  DF_DEFAULT  = 0,
  DF_SSE      = 1,      ///< general size SSE
  DF_SSE4     = 2,      ///<   4xM SSE
  DF_SSE8     = 3,      ///<   8xM SSE
  DF_SSE16    = 4,      ///<  16xM SSE
  DF_SSE32    = 5,      ///<  32xM SSE
  DF_SSE64    = 6,      ///<  64xM SSE
  DF_SSE16N   = 7,      ///< 16NxM SSE
  
  DF_SAD      = 8,      ///< general size SAD
  DF_SAD4     = 9,      ///<   4xM SAD
  DF_SAD8     = 10,     ///<   8xM SAD
  DF_SAD16    = 11,     ///<  16xM SAD
  DF_SAD32    = 12,     ///<  32xM SAD
  DF_SAD64    = 13,     ///<  64xM SAD
  DF_SAD16N   = 14,     ///< 16NxM SAD
  
  DF_SADS     = 15,     ///< general size SAD with step
  DF_SADS4    = 16,     ///<   4xM SAD with step
  DF_SADS8    = 17,     ///<   8xM SAD with step
  DF_SADS16   = 18,     ///<  16xM SAD with step
  DF_SADS32   = 19,     ///<  32xM SAD with step
  DF_SADS64   = 20,     ///<  64xM SAD with step
  DF_SADS16N  = 21,     ///< 16NxM SAD with step
  
  DF_HADS     = 22,     ///< general size Hadamard with step
  DF_HADS4    = 23,     ///<   4xM HAD with step
  DF_HADS8    = 24,     ///<   8xM HAD with step
  DF_HADS16   = 25,     ///<  16xM HAD with step
  DF_HADS32   = 26,     ///<  32xM HAD with step
  DF_HADS64   = 27,     ///<  64xM HAD with step
  DF_HADS16N  = 28,     ///< 16NxM HAD with step
  
#if AMP_SAD
  DF_SAD12    = 43,
  DF_SAD24    = 44,
  DF_SAD48    = 45,

  DF_SADS12   = 46,
  DF_SADS24   = 47,
  DF_SADS48   = 48,

  DF_SSE_FRAME = 50     ///< Frame-based SSE
#else
  DF_SSE_FRAME = 33     ///< Frame-based SSE
#endif
};

/// index for SBAC based RD optimization
enum CI_IDX
{
  CI_CURR_BEST = 0,     ///< best mode index
  CI_NEXT_BEST,         ///< next best index
  CI_TEMP_BEST,         ///< temporal index
  CI_CHROMA_INTRA,      ///< chroma intra index
  CI_QT_TRAFO_TEST,
  CI_QT_TRAFO_ROOT,
  CI_NUM,               ///< total number
};

/// motion vector predictor direction used in AMVP
enum MVP_DIR
{
  MD_LEFT = 0,          ///< MVP of left block
  MD_ABOVE,             ///< MVP of above block
  MD_ABOVE_RIGHT,       ///< MVP of above right block
  MD_BELOW_LEFT,        ///< MVP of below left block
  MD_ABOVE_LEFT         ///< MVP of above left block
};

/// motion vector prediction mode used in AMVP
enum AMVP_MODE
{
  AM_NONE = 0,          ///< no AMVP mode
  AM_EXPL,              ///< explicit signalling of motion vector index
};

/// coefficient scanning type used in ACS
enum COEFF_SCAN_TYPE
{
  SCAN_ZIGZAG = 0,      ///< typical zigzag scan
  SCAN_HOR,             ///< horizontal first scan
  SCAN_VER,              ///< vertical first scan
  SCAN_DIAG              ///< up-right diagonal scan
};

//! \}

#endif
