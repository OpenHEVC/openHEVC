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

/** \file     TEncCavlc.cpp
    \brief    CAVLC encoder class
*/

#include "../TLibCommon/CommonDef.h"
#include "TEncCavlc.h"
#include "SEIwrite.h"

//! \ingroup TLibEncoder
//! \{

#if ENC_DEC_TRACE

#define WRITE_CODE( value, length, name)    xWriteCodeTr ( value, length, name )
#define WRITE_UVLC( value,         name)    xWriteUvlcTr ( value,         name )
#define WRITE_SVLC( value,         name)    xWriteSvlcTr ( value,         name )
#define WRITE_FLAG( value,         name)    xWriteFlagTr ( value,         name )

Void  xWriteUvlcTr          ( UInt value,               const Char *pSymbolName);
Void  xWriteSvlcTr          ( Int  value,               const Char *pSymbolName);
Void  xWriteFlagTr          ( UInt value,               const Char *pSymbolName);

Void  xTraceSPSHeader (TComSPS *pSPS)
{
  fprintf( g_hTrace, "=========== Sequence Parameter Set ID: %d ===========\n", pSPS->getSPSId() );
}

Void  xTracePPSHeader (TComPPS *pPPS)
{
  fprintf( g_hTrace, "=========== Picture Parameter Set ID: %d ===========\n", pPPS->getPPSId() );
}

#if !REMOVE_APS
Void  xTraceAPSHeader (TComAPS *pAPS)
{
  fprintf( g_hTrace, "=========== Adaptation Parameter Set ===========\n");
}
#endif

Void  xTraceSliceHeader (TComSlice *pSlice)
{
  fprintf( g_hTrace, "=========== Slice ===========\n");
}


Void  TEncCavlc::xWriteCodeTr (UInt value, UInt  length, const Char *pSymbolName)
{
  xWriteCode (value,length);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s u(%d) : %d\n", pSymbolName, length, value ); 
}

Void  TEncCavlc::xWriteUvlcTr (UInt value, const Char *pSymbolName)
{
  xWriteUvlc (value);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s u(v) : %d\n", pSymbolName, value ); 
}

Void  TEncCavlc::xWriteSvlcTr (Int value, const Char *pSymbolName)
{
  xWriteSvlc(value);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s s(v) : %d\n", pSymbolName, value ); 
}

Void  TEncCavlc::xWriteFlagTr(UInt value, const Char *pSymbolName)
{
  xWriteFlag(value);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s u(1) : %d\n", pSymbolName, value ); 
}

#else

#define WRITE_CODE( value, length, name)     xWriteCode ( value, length )
#define WRITE_UVLC( value,         name)     xWriteUvlc ( value )
#define WRITE_SVLC( value,         name)     xWriteSvlc ( value )
#define WRITE_FLAG( value,         name)     xWriteFlag ( value )

#endif



// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TEncCavlc::TEncCavlc()
{
  m_pcBitIf           = NULL;
  m_uiCoeffCost       = 0;
#if !REMOVE_FGS
  m_iSliceGranularity = 0;
#endif
}

TEncCavlc::~TEncCavlc()
{
}


// ====================================================================================================================
// Public member functions
// ====================================================================================================================

Void TEncCavlc::resetEntropy()
{
}

/**
 * marshall the SEI message sei.
 */
void TEncCavlc::codeSEI(const SEI& sei)
{
  writeSEImessage(*m_pcBitIf, sei);
}

#if !REMOVE_APS
Void  TEncCavlc::codeAPSInitInfo(TComAPS* pcAPS)
{

#if ENC_DEC_TRACE  
  xTraceAPSHeader(pcAPS);
#endif
  //APS ID
  WRITE_UVLC( pcAPS->getAPSID(), "aps_id" );
}
#endif

Void TEncCavlc::codeDFFlag(UInt uiCode, const Char *pSymbolName)
{
  WRITE_FLAG(uiCode, pSymbolName);
}
Void TEncCavlc::codeDFSvlc(Int iCode, const Char *pSymbolName)
{
  WRITE_SVLC(iCode, pSymbolName);
}

#if J0234_INTER_RPS_SIMPL
Void TEncCavlc::codeShortTermRefPicSet( TComSPS* pcSPS, TComReferencePictureSet* rps, Bool calledFromSliceHeader)
#else
Void TEncCavlc::codeShortTermRefPicSet( TComSPS* pcSPS, TComReferencePictureSet* rps )
#endif
{
#if PRINT_RPS_INFO
  int lastBits = getNumberOfWrittenBits();
#endif
  WRITE_FLAG( rps->getInterRPSPrediction(), "inter_ref_pic_set_prediction_flag" ); // inter_RPS_prediction_flag
  if (rps->getInterRPSPrediction()) 
  {
    Int deltaRPS = rps->getDeltaRPS();
#if J0234_INTER_RPS_SIMPL
    if(calledFromSliceHeader)
    {
      WRITE_UVLC( rps->getDeltaRIdxMinus1(), "delta_idx_minus1" ); // delta index of the Reference Picture Set used for prediction minus 1
    }
#else
    WRITE_UVLC( rps->getDeltaRIdxMinus1(), "delta_idx_minus1" ); // delta index of the Reference Picture Set used for prediction minus 1
#endif

    WRITE_CODE( (deltaRPS >=0 ? 0: 1), 1, "delta_rps_sign" ); //delta_rps_sign
    WRITE_UVLC( abs(deltaRPS) - 1, "abs_delta_rps_minus1"); // absolute delta RPS minus 1

    for(Int j=0; j < rps->getNumRefIdc(); j++)
    {
      Int refIdc = rps->getRefIdc(j);
      WRITE_CODE( (refIdc==1? 1: 0), 1, "used_by_curr_pic_flag" ); //first bit is "1" if Idc is 1 
      if (refIdc != 1) 
      {
        WRITE_CODE( refIdc>>1, 1, "use_delta_flag" ); //second bit is "1" if Idc is 2, "0" otherwise.
      }
    }
  }
  else
  {
    WRITE_UVLC( rps->getNumberOfNegativePictures(), "num_negative_pics" );
    WRITE_UVLC( rps->getNumberOfPositivePictures(), "num_positive_pics" );
    Int prev = 0;
    for(Int j=0 ; j < rps->getNumberOfNegativePictures(); j++)
    {
      WRITE_UVLC( prev-rps->getDeltaPOC(j)-1, "delta_poc_s0_minus1" );
      prev = rps->getDeltaPOC(j);
      WRITE_FLAG( rps->getUsed(j), "used_by_curr_pic_s0_flag"); 
    }
    prev = 0;
    for(Int j=rps->getNumberOfNegativePictures(); j < rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures(); j++)
    {
      WRITE_UVLC( rps->getDeltaPOC(j)-prev-1, "delta_poc_s1_minus1" );
      prev = rps->getDeltaPOC(j);
      WRITE_FLAG( rps->getUsed(j), "used_by_curr_pic_s1_flag" ); 
    }
  }

#if PRINT_RPS_INFO
  printf("irps=%d (%2d bits) ", rps->getInterRPSPrediction(), getNumberOfWrittenBits() - lastBits);
  rps->printDeltaPOC();
#endif
}


Void TEncCavlc::codePPS( TComPPS* pcPPS )
{
#if ENC_DEC_TRACE  
  xTracePPSHeader (pcPPS);
#endif
  
  WRITE_UVLC( pcPPS->getPPSId(),                             "pic_parameter_set_id" );
  WRITE_UVLC( pcPPS->getSPSId(),                             "seq_parameter_set_id" );

  WRITE_FLAG( pcPPS->getSignHideFlag(), "sign_data_hiding_flag" );
  WRITE_FLAG( pcPPS->getCabacInitPresentFlag() ? 1 : 0,   "cabac_init_present_flag" );
  WRITE_UVLC( pcPPS->getNumRefIdxL0DefaultActive()-1,     "num_ref_idx_l0_default_active_minus1");
  WRITE_UVLC( pcPPS->getNumRefIdxL1DefaultActive()-1,     "num_ref_idx_l1_default_active_minus1");

  WRITE_SVLC( pcPPS->getPicInitQPMinus26(),                  "pic_init_qp_minus26");
  WRITE_FLAG( pcPPS->getConstrainedIntraPred() ? 1 : 0,      "constrained_intra_pred_flag" );
#if PPS_TS_FLAG
  WRITE_FLAG( pcPPS->getUseTransformSkip() ? 1 : 0,  "transform_skip_enabled_flag" ); 
#endif
#if !REMOVE_FGS
  WRITE_CODE( pcPPS->getSliceGranularity(), 2,               "slice_granularity");
#endif
#if CU_DQP_ENABLE_FLAG
  WRITE_FLAG( pcPPS->getUseDQP() ? 1 : 0, "cu_qp_delta_enabled_flag" );
  if ( pcPPS->getUseDQP() )
  {
#if REMOVE_FGS
    WRITE_UVLC( pcPPS->getMaxCuDQPDepth(), "diff_cu_qp_delta_depth" );
#else
    WRITE_UVLC( pcPPS->getMaxCuDQPDepth() - pcPPS->getSliceGranularity(), "diff_cu_qp_delta_depth" );
#endif
  }
#else
#if REMOVE_FGS
  WRITE_UVLC( pcPPS->getMaxCuDQPDepth() + pcPPS->getUseDQP(),                   "diff_cu_qp_delta_depth" );
#else
  WRITE_UVLC( pcPPS->getMaxCuDQPDepth() - pcPPS->getSliceGranularity() + pcPPS->getUseDQP(),                   "diff_cu_qp_delta_depth" );
#endif
#endif
  WRITE_SVLC( pcPPS->getChromaCbQpOffset(),                   "cb_qp_offset" );
  WRITE_SVLC( pcPPS->getChromaCrQpOffset(),                   "cr_qp_offset" );
#if CHROMA_QP_EXTENSION
  WRITE_FLAG( pcPPS->getSliceChromaQpFlag() ? 1 : 0,          "slicelevel_chroma_qp_flag" );
#endif

  WRITE_FLAG( pcPPS->getUseWP() ? 1 : 0,  "weighted_pred_flag" );   // Use of Weighting Prediction (P_SLICE)
  WRITE_FLAG( pcPPS->getWPBiPred() ? 1 : 0, "weighted_bipred_flag" );  // Use of Weighting Bi-Prediction (B_SLICE)
  WRITE_FLAG( pcPPS->getOutputFlagPresentFlag() ? 1 : 0,  "output_flag_present_flag" );
#if DEPENDENT_SLICES
  WRITE_FLAG( pcPPS->getDependentSlicesEnabledFlag() ? 1 : 0, "dependent_slices_enabled_flag" );
#endif
  WRITE_FLAG( pcPPS->getTransquantBypassEnableFlag() ? 1 : 0, "transquant_bypass_enable_flag" );

  Int tilesOrEntropyCodingSyncIdc = 0;
  if ( pcPPS->getNumColumnsMinus1() > 0 || pcPPS->getNumRowsMinus1() > 0)
  {
    tilesOrEntropyCodingSyncIdc = 1;
  }
  else if ( pcPPS->getNumSubstreams() > 1 )
  {
    tilesOrEntropyCodingSyncIdc = 2;
  }
#if DEPENDENT_SLICES
  else if( pcPPS->getDependentSlicesEnabledFlag() )
  {
    if(pcPPS->getTilesOrEntropyCodingSyncIdc() != 2)
    {
      tilesOrEntropyCodingSyncIdc = 3;
    }
    else
    {
      tilesOrEntropyCodingSyncIdc = 2;
    }
  }
#endif
  pcPPS->setTilesOrEntropyCodingSyncIdc( tilesOrEntropyCodingSyncIdc );
  WRITE_CODE(tilesOrEntropyCodingSyncIdc, 2, "tiles_or_entropy_coding_sync_idc");

  if(pcPPS->getTilesOrEntropyCodingSyncIdc()==1)
  {
    WRITE_UVLC( pcPPS->getNumColumnsMinus1(),                                    "num_tile_columns_minus1" );
    WRITE_UVLC( pcPPS->getNumRowsMinus1(),                                       "num_tile_rows_minus1" );
    WRITE_FLAG( pcPPS->getUniformSpacingIdr(),                                   "uniform_spacing_flag" );
    if( pcPPS->getUniformSpacingIdr() == 0 )
    {
      for(UInt i=0; i<pcPPS->getNumColumnsMinus1(); i++)
      {
        WRITE_UVLC( pcPPS->getColumnWidth(i),                                    "column_width" );
      }
      for(UInt i=0; i<pcPPS->getNumRowsMinus1(); i++)
      {
        WRITE_UVLC( pcPPS->getRowHeight(i),                                      "row_height" );
      }
    }
    if(pcPPS->getNumColumnsMinus1() !=0 || pcPPS->getNumRowsMinus1() !=0)
    {
        WRITE_FLAG( pcPPS->getLFCrossTileBoundaryFlag()?1 : 0,            "loop_filter_across_tiles_enabled_flag");
    }
  }
#if DEPENDENT_SLICES
  else if( pcPPS->getTilesOrEntropyCodingSyncIdc()==3 )
  {
    WRITE_FLAG( pcPPS->getCabacIndependentFlag()? 1 : 0,            "cabac_independent_flag" );
  }
#endif
#if MOVE_LOOP_FILTER_SLICES_FLAG
  WRITE_FLAG( pcPPS->getLFCrossSliceBoundaryFlag()?1 : 0,                            "seq_loop_filter_across_slices_enabled_flag");
#endif
  WRITE_FLAG( pcPPS->getDeblockingFilterControlPresent()?1 : 0, "deblocking_filter_control_present_flag");
  if(pcPPS->getDeblockingFilterControlPresent())
  {
    WRITE_FLAG( pcPPS->getLoopFilterOffsetInPPS() ? 1 : 0,                          "pps_deblocking_filter_flag" ); 
    if(pcPPS->getLoopFilterOffsetInPPS())
    {
      WRITE_FLAG(pcPPS->getLoopFilterDisable(), "loop_filter_disable");  // should be an IDC
      if(!pcPPS->getLoopFilterDisable())
      {
        WRITE_SVLC( pcPPS->getLoopFilterBetaOffset(),                                 "pps_beta_offset_div2");
        WRITE_SVLC( pcPPS->getLoopFilterTcOffset(),                                   "pps_tc_offset_div2");
      }
    }
  }
  WRITE_FLAG( pcPPS->getScalingListPresentFlag() ? 1 : 0,                          "pps_scaling_list_data_present_flag" ); 
  if( pcPPS->getScalingListPresentFlag() )
  {
#if SCALING_LIST_OUTPUT_RESULT
    printf("PPS\n");
#endif
    codeScalingList( m_pcSlice->getScalingList() );
  }
  WRITE_UVLC( pcPPS->getLog2ParallelMergeLevelMinus2(), "log2_parallel_merge_level_minus2");
#if SLICE_HEADER_EXTENSION
  WRITE_FLAG( pcPPS->getSliceHeaderExtensionPresentFlag(), "slice_header_extension_present_flag");
#endif
  WRITE_FLAG( 0, "pps_extension_flag" );
}

Void TEncCavlc::codeSPS( TComSPS* pcSPS )
{
#if ENC_DEC_TRACE  
  xTraceSPSHeader (pcSPS);
#endif
  WRITE_CODE( pcSPS->getProfileSpace (),   3,       "profile_space" );
  WRITE_CODE( pcSPS->getProfileIdc (),     5,       "profile_idc" );
  WRITE_CODE( pcSPS->getRsvdIndFlags(),   16,       "reserved_indicator_flags" );
  WRITE_CODE( pcSPS->getLevelIdc (),       8,       "level_idc" );
  WRITE_CODE( pcSPS->getProfileCompat (), 32,       "profile_compatibility" );
  WRITE_UVLC( pcSPS->getSPSId (),                   "seq_parameter_set_id" );
  WRITE_UVLC( pcSPS->getVPSId (),                   "video_parameter_set_id" );
  WRITE_UVLC( pcSPS->getChromaFormatIdc (),         "chroma_format_idc" );
  WRITE_CODE( pcSPS->getMaxTLayers() - 1,  3,       "max_temporal_layers_minus1" );
  WRITE_UVLC( pcSPS->getPicWidthInLumaSamples (),   "pic_width_in_luma_samples" );
  WRITE_UVLC( pcSPS->getPicHeightInLumaSamples(),   "pic_height_in_luma_samples" );
  WRITE_FLAG( pcSPS->getPicCroppingFlag(),          "pic_cropping_flag" );
  if (pcSPS->getPicCroppingFlag())
  {
    WRITE_UVLC( pcSPS->getPicCropLeftOffset()   / TComSPS::getCropUnitX(pcSPS->getChromaFormatIdc() ), "pic_crop_left_offset" );
    WRITE_UVLC( pcSPS->getPicCropRightOffset()  / TComSPS::getCropUnitX(pcSPS->getChromaFormatIdc() ), "pic_crop_right_offset" );
    WRITE_UVLC( pcSPS->getPicCropTopOffset()    / TComSPS::getCropUnitY(pcSPS->getChromaFormatIdc() ), "pic_crop_top_offset" );
    WRITE_UVLC( pcSPS->getPicCropBottomOffset() / TComSPS::getCropUnitY(pcSPS->getChromaFormatIdc() ), "pic_crop_bottom_offset" );
  }

#if FULL_NBIT
  WRITE_UVLC( pcSPS->getBitDepth() - 8,             "bit_depth_luma_minus8" );
#else
  WRITE_UVLC( pcSPS->getBitIncrement(),             "bit_depth_luma_minus8" );
#endif
#if FULL_NBIT
  WRITE_UVLC( pcSPS->getBitDepth() - 8,             "bit_depth_chroma_minus8" );
#else
  WRITE_UVLC( pcSPS->getBitIncrement(),             "bit_depth_chroma_minus8" );
#endif

  WRITE_FLAG( pcSPS->getUsePCM() ? 1 : 0,                   "pcm_enabled_flag");

  if( pcSPS->getUsePCM() )
  {
  WRITE_CODE( pcSPS->getPCMBitDepthLuma() - 1, 4,   "pcm_bit_depth_luma_minus1" );
  WRITE_CODE( pcSPS->getPCMBitDepthChroma() - 1, 4, "pcm_bit_depth_chroma_minus1" );
  }

  WRITE_UVLC( pcSPS->getBitsForPOC()-4,                 "log2_max_pic_order_cnt_lsb_minus4" );
  for(UInt i=0; i <= pcSPS->getMaxTLayers()-1; i++)
  {
    WRITE_UVLC( pcSPS->getMaxDecPicBuffering(i),           "max_dec_pic_buffering[i]" );
    WRITE_UVLC( pcSPS->getNumReorderPics(i),               "num_reorder_pics[i]" );
    WRITE_UVLC( pcSPS->getMaxLatencyIncrease(i),           "max_latency_increase[i]" );
  }
  assert( pcSPS->getMaxCUWidth() == pcSPS->getMaxCUHeight() );
  
  UInt MinCUSize = pcSPS->getMaxCUWidth() >> ( pcSPS->getMaxCUDepth()-g_uiAddCUDepth );
  UInt log2MinCUSize = 0;
  while(MinCUSize > 1)
  {
    MinCUSize >>= 1;
    log2MinCUSize++;
  }

  WRITE_FLAG( pcSPS->getRestrictedRefPicListsFlag(),                                 "restricted_ref_pic_lists_flag" );
  if( pcSPS->getRestrictedRefPicListsFlag() )
  {
    WRITE_FLAG( pcSPS->getListsModificationPresentFlag(),                            "lists_modification_present_flag" );
  }
  WRITE_UVLC( log2MinCUSize - 3,                                                     "log2_min_coding_block_size_minus3" );
  WRITE_UVLC( pcSPS->getMaxCUDepth()-g_uiAddCUDepth,                                 "log2_diff_max_min_coding_block_size" );
  WRITE_UVLC( pcSPS->getQuadtreeTULog2MinSize() - 2,                                 "log2_min_transform_block_size_minus2" );
  WRITE_UVLC( pcSPS->getQuadtreeTULog2MaxSize() - pcSPS->getQuadtreeTULog2MinSize(), "log2_diff_max_min_transform_block_size" );
  if( pcSPS->getUsePCM() )
  {
    WRITE_UVLC( pcSPS->getPCMLog2MinSize() - 3,                                      "log2_min_pcm_coding_block_size_minus3" );
    WRITE_UVLC( pcSPS->getPCMLog2MaxSize() - pcSPS->getPCMLog2MinSize(),             "log2_diff_max_min_pcm_coding_block_size" );
  }
  WRITE_UVLC( pcSPS->getQuadtreeTUMaxDepthInter() - 1,                               "max_transform_hierarchy_depth_inter" );
  WRITE_UVLC( pcSPS->getQuadtreeTUMaxDepthIntra() - 1,                               "max_transform_hierarchy_depth_intra" );
  WRITE_FLAG( pcSPS->getScalingListFlag() ? 1 : 0,                                   "scaling_list_enabled_flag" ); 
  if(pcSPS->getScalingListFlag())
  {
    WRITE_FLAG( pcSPS->getScalingListPresentFlag() ? 1 : 0,                          "sps_scaling_list_data_present_flag" ); 
    if(pcSPS->getScalingListPresentFlag())
    {
#if SCALING_LIST_OUTPUT_RESULT
    printf("SPS\n");
#endif
      codeScalingList( m_pcSlice->getScalingList() );
    }
  }
#if !REMOVE_LMCHROMA
  WRITE_FLAG( pcSPS->getUseLMChroma () ? 1 : 0,                                      "chroma_pred_from_luma_enabled_flag" );
#endif
#if !PPS_TS_FLAG
  WRITE_FLAG( pcSPS->getUseTransformSkip () ? 1 : 0,                                 "transform_skip_enabled_flag" );
#endif
#if !MOVE_LOOP_FILTER_SLICES_FLAG
  WRITE_FLAG( pcSPS->getLFCrossSliceBoundaryFlag()?1 : 0,                            "seq_loop_filter_across_slices_enabled_flag");
#endif
  WRITE_FLAG( pcSPS->getUseAMP(),                                                    "asymmetric_motion_partitions_enabled_flag" );
#if !REMOVE_NSQT
  WRITE_FLAG( pcSPS->getUseNSQT(),                                                   "non_square_quadtree_enabled_flag" );
#endif
  WRITE_FLAG( pcSPS->getUseSAO() ? 1 : 0,                                            "sample_adaptive_offset_enabled_flag");
#if !REMOVE_ALF
  WRITE_FLAG( pcSPS->getUseALF () ? 1 : 0,                                           "adaptive_loop_filter_enabled_flag");
#endif
  if( pcSPS->getUsePCM() )
  {
  WRITE_FLAG( pcSPS->getPCMFilterDisableFlag()?1 : 0,                                "pcm_loop_filter_disable_flag");
  }

  assert( pcSPS->getMaxTLayers() > 0 );         

  WRITE_FLAG( pcSPS->getTemporalIdNestingFlag() ? 1 : 0,                             "temporal_id_nesting_flag" );

  TComRPSList* rpsList = pcSPS->getRPSList();
  TComReferencePictureSet*      rps;

  WRITE_UVLC(rpsList->getNumberOfReferencePictureSets(), "num_short_term_ref_pic_sets" );
  for(Int i=0; i < rpsList->getNumberOfReferencePictureSets(); i++)
  {
    rps = rpsList->getReferencePictureSet(i);
#if J0234_INTER_RPS_SIMPL
    codeShortTermRefPicSet(pcSPS,rps,false);
#else
    codeShortTermRefPicSet(pcSPS,rps);
#endif
  }    
  WRITE_FLAG( pcSPS->getLongTermRefsPresent() ? 1 : 0,         "long_term_ref_pics_present_flag" );
#if LTRP_IN_SPS
  if (pcSPS->getLongTermRefsPresent()) 
  {
    WRITE_UVLC(pcSPS->getNumLongTermRefPicSPS(), "num_long_term_ref_pic_sps" );
    for (UInt k = 0; k < pcSPS->getNumLongTermRefPicSPS(); k++)
    {
      WRITE_CODE( pcSPS->getLtRefPicPocLsbSps(k), pcSPS->getBitsForPOC(), "lt_ref_pic_poc_lsb_sps");
      WRITE_FLAG( pcSPS->getUsedByCurrPicLtSPSFlag(k), "used_by_curr_pic_lt_sps_flag");
    }
  }
#endif
  WRITE_FLAG( pcSPS->getTMVPFlagsPresent()  ? 1 : 0,           "sps_temporal_mvp_enable_flag" );
  // AMVP mode for each depth
  for (Int i = 0; i < pcSPS->getMaxCUDepth(); i++)
  {
    xWriteFlag( pcSPS->getAMVPMode(i) ? 1 : 0);
  }

  WRITE_FLAG( 0, "sps_extension_flag" );
}

Void TEncCavlc::codeVPS( TComVPS* pcVPS )
{
  WRITE_CODE( pcVPS->getMaxTLayers() - 1,     3,        "vps_max_temporal_layers_minus1" );
  WRITE_CODE( pcVPS->getMaxLayers() - 1,      5,        "vps_max_layers_minus1" );
  WRITE_UVLC( pcVPS->getVPSId(),                        "video_parameter_set_id" );
  WRITE_FLAG( pcVPS->getTemporalNestingFlag() - 1,      "vps_temporal_id_nesting_flag" );
  for(UInt i=0; i <= pcVPS->getMaxTLayers()-1; i++)
  {
    WRITE_UVLC( pcVPS->getMaxDecPicBuffering(i),           "vps_max_dec_pic_buffering[i]" );
    WRITE_UVLC( pcVPS->getNumReorderPics(i),               "vps_num_reorder_pics[i]" );
    WRITE_UVLC( pcVPS->getMaxLatencyIncrease(i),           "vps_max_latency_increase[i]" );
  }
  
  WRITE_FLAG( 0,                     "vps_extension_flag" );
  
  //future extensions here..
  
  return;
}

Void TEncCavlc::codeSliceHeader         ( TComSlice* pcSlice )
{
#if ENC_DEC_TRACE  
  xTraceSliceHeader (pcSlice);
#endif

  //calculate number of bits required for slice address
  Int maxAddrOuter = pcSlice->getPic()->getNumCUsInFrame();
  Int reqBitsOuter = 0;
  while(maxAddrOuter>(1<<reqBitsOuter)) 
  {
    reqBitsOuter++;
  }
  Int maxAddrInner = pcSlice->getPic()->getNumPartInCU()>>(2);
#if REMOVE_FGS
  maxAddrInner = 1;
#else
  maxAddrInner = (1<<(pcSlice->getPPS()->getSliceGranularity()<<1));
#endif
  Int reqBitsInner = 0;
  
  while(maxAddrInner>(1<<reqBitsInner))
  {
    reqBitsInner++;
  }
  Int lCUAddress;
  Int innerAddress;
  if (pcSlice->isNextSlice())
  {
    // Calculate slice address
    lCUAddress = (pcSlice->getSliceCurStartCUAddr()/pcSlice->getPic()->getNumPartInCU());
#if REMOVE_FGS
    innerAddress = 0;
#else
    innerAddress = (pcSlice->getSliceCurStartCUAddr()%(pcSlice->getPic()->getNumPartInCU()))>>((pcSlice->getSPS()->getMaxCUDepth()-pcSlice->getPPS()->getSliceGranularity())<<1);
#endif
  }
  else
  {
    // Calculate slice address
    lCUAddress = (pcSlice->getDependentSliceCurStartCUAddr()/pcSlice->getPic()->getNumPartInCU());
#if REMOVE_FGS
    innerAddress = 0;
#else
    innerAddress = (pcSlice->getDependentSliceCurStartCUAddr()%(pcSlice->getPic()->getNumPartInCU()))>>((pcSlice->getSPS()->getMaxCUDepth()-pcSlice->getPPS()->getSliceGranularity())<<1);
#endif
    
  }
  //write slice address
  Int address = (pcSlice->getPic()->getPicSym()->getCUOrderMap(lCUAddress) << reqBitsInner) + innerAddress;
  WRITE_FLAG( address==0, "first_slice_in_pic_flag" );
#if SPLICING_FRIENDLY_PARAMS
    if(   pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLANT
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRANT
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )
    {
      WRITE_FLAG( 0, "no_output_of_prior_pics_flag" );
    }
#endif
  WRITE_UVLC( pcSlice->getPPS()->getPPSId(), "pic_parameter_set_id" );
  if(address>0) 
  {
    WRITE_CODE( address, reqBitsOuter+reqBitsInner, "slice_address" );
  }

  WRITE_UVLC( pcSlice->getSliceType(),       "slice_type" );
  Bool bDependentSlice = (!pcSlice->isNextSlice());
  WRITE_FLAG( bDependentSlice ? 1 : 0, "dependent_slice_flag" );
  
#if DEPENDENT_SLICES
  if( pcSlice->getPPS()->getDependentSlicesEnabledFlag() && bDependentSlice )
    return;
#endif

  if (!bDependentSlice)
  {
    if( pcSlice->getPPS()->getOutputFlagPresentFlag() )
    {
      WRITE_FLAG( pcSlice->getPicOutputFlag() ? 1 : 0, "pic_output_flag" );
    }
#if !SPLICING_FRIENDLY_PARAMS
    if(   pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLANT
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRANT
       || pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )
    {
      WRITE_UVLC( 0, "rap_pic_id" );
      WRITE_FLAG( 0, "no_output_of_prior_pics_flag" );
    }
#endif
    if( pcSlice->getNalUnitType() != NAL_UNIT_CODED_SLICE_IDR )
    {
      Int picOrderCntLSB = (pcSlice->getPOC()-pcSlice->getLastIDR()+(1<<pcSlice->getSPS()->getBitsForPOC()))%(1<<pcSlice->getSPS()->getBitsForPOC());
      WRITE_CODE( picOrderCntLSB, pcSlice->getSPS()->getBitsForPOC(), "pic_order_cnt_lsb");
      TComReferencePictureSet* rps = pcSlice->getRPS();
      if(pcSlice->getRPSidx() < 0)
      {
        WRITE_FLAG( 0, "short_term_ref_pic_set_sps_flag");
#if J0234_INTER_RPS_SIMPL
        codeShortTermRefPicSet(pcSlice->getSPS(), rps, true);
#else
        codeShortTermRefPicSet(pcSlice->getSPS(), rps);
#endif
      }
      else
      {
        WRITE_FLAG( 1, "short_term_ref_pic_set_sps_flag");
        WRITE_UVLC( pcSlice->getRPSidx(), "short_term_ref_pic_set_idx" );
      }
      if(pcSlice->getSPS()->getLongTermRefsPresent())
      {
#if LTRP_IN_SPS
        //Int maxPocLsb = 1<<pcSlice->getSPS()->getBitsForPOC();
        Int numLtrpInSH = rps->getNumberOfLongtermPictures();
        Int ltrpInSPS[MAX_NUM_REF_PICS];
        //Int ltrpInSH[MAX_NUM_REF_PICS];
        Int numLtrpInSPS = 0;
        UInt ltrpIndex;
        Int counter = 0;
        for(Int k = rps->getNumberOfPictures()-1; k > rps->getNumberOfPictures()-rps->getNumberOfLongtermPictures()-1; k--) 
        {
          if (findMatchingLTRP(pcSlice, &ltrpIndex, rps->getPOC(k), rps->getUsed(k))) 
          {
            ltrpInSPS[numLtrpInSPS] = ltrpIndex;
            numLtrpInSPS++;
          }
          else
          {
            //ltrpInSH[counter] = rps->getPOC(k) % maxPocLsb;
            counter++;
          }
        }
        numLtrpInSH -= numLtrpInSPS;

        Int bitsForLtrpInSPS = 1;
        while (pcSlice->getSPS()->getNumLongTermRefPicSPS() > (1 << bitsForLtrpInSPS))
        {
          bitsForLtrpInSPS++;
        }
        if (pcSlice->getSPS()->getNumLongTermRefPicSPS() > 0) 
        {
          WRITE_UVLC( numLtrpInSPS, "num_long_term_sps");
        }
        WRITE_UVLC( numLtrpInSH, "num_long_term_pics");


#else
        WRITE_UVLC( rps->getNumberOfLongtermPictures(), "num_long_term_pics");
#endif
        // Note that the LSBs of the LT ref. pic. POCs must be sorted before.
        // Not sorted here because LT ref indices will be used in setRefPicList()
        Int prevDeltaMSB = 0, prevLSB = 0;
        Int offset = rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures();
        for(Int i=rps->getNumberOfPictures()-1 ; i > offset-1; i--)
        {
#if LTRP_IN_SPS
          if (counter < numLtrpInSPS)
          {
            WRITE_CODE( ltrpInSPS[counter], bitsForLtrpInSPS, "lt_idx_sps[i]");
          }
          else 
          {
            WRITE_CODE( rps->getPocLSBLT(i), pcSlice->getSPS()->getBitsForPOC(), "poc_lsb_lt");
            WRITE_FLAG( rps->getUsed(i), "used_by_curr_pic_lt_flag"); 
          }
#else
          WRITE_CODE( rps->getPocLSBLT(i), pcSlice->getSPS()->getBitsForPOC(), "poc_lsb_lt");
#endif
          WRITE_FLAG( rps->getDeltaPocMSBPresentFlag(i), "delta_poc_msb_present_flag");

          if(rps->getDeltaPocMSBPresentFlag(i))
          {
            Bool deltaFlag = false;
#if LTRP_IN_SPS
            //  First LTRP from SPS                 ||  First LTRP from SH                              || curr LSB            != prev LSB
            if( (i == rps->getNumberOfPictures()-1) || (i == rps->getNumberOfPictures()-1-numLtrpInSPS) || (rps->getPocLSBLT(i) != prevLSB) )
#else
            //   First LTRP                         ||  curr LSB            != prev LSB
            if( (i == rps->getNumberOfPictures()-1) || (rps->getPocLSBLT(i) != prevLSB) )
#endif
            {
              deltaFlag = true;
            }
            if(deltaFlag)
            {
              WRITE_UVLC( rps->getDeltaPocMSBCycleLT(i), "delta_poc_msb_cycle_lt[i]" );
            }
            else
            {              
              Int differenceInDeltaMSB = rps->getDeltaPocMSBCycleLT(i) - prevDeltaMSB;
              assert(differenceInDeltaMSB >= 0);
              WRITE_UVLC( differenceInDeltaMSB, "delta_poc_msb_cycle_lt[i]" );
            }
            prevLSB = rps->getPocLSBLT(i);
            prevDeltaMSB = rps->getDeltaPocMSBCycleLT(i);
          }
#if !LTRP_IN_SPS
          WRITE_FLAG( rps->getUsed(i), "used_by_curr_pic_lt_flag"); 
#endif
        }
      }
    }
#if REMOVE_ALF
    if(pcSlice->getSPS()->getUseSAO())
#else
    if(pcSlice->getSPS()->getUseSAO() || pcSlice->getSPS()->getUseALF())
#endif
    {
      if (pcSlice->getSPS()->getUseSAO())
      {
         WRITE_FLAG( pcSlice->getSaoEnabledFlag(), "SAO on/off flag in slice header" );
#if !SAO_LUM_CHROMA_ONOFF_FLAGS
         if (pcSlice->getSaoEnabledFlag() )
#endif
         {
#if REMOVE_APS
           SAOParam *saoParam = pcSlice->getPic()->getPicSym()->getSaoParam();
#else
           SAOParam *saoParam = pcSlice->getAPS()->getSaoParam();
#endif
#if SAO_TYPE_SHARING 
          WRITE_FLAG( saoParam->bSaoFlag[1], "SAO on/off flag for Cb&Cr in slice header" );
#else
          WRITE_FLAG( saoParam->bSaoFlag[1], "SAO on/off flag for Cb in slice header" );
          WRITE_FLAG( saoParam->bSaoFlag[2], "SAO on/off flag for Cr in slice header" );
#endif
         }
      }
#if !REMOVE_APS
      WRITE_UVLC( pcSlice->getAPS()->getAPSID(), "aps_id");
#endif
    }

    //check if numrefidxes match the defaults. If not, override
    if (!pcSlice->isIntra())
    {
      if (pcSlice->getSPS()->getTMVPFlagsPresent())
      {
        WRITE_FLAG( pcSlice->getEnableTMVPFlag(), "enable_temporal_mvp_flag" );
      }
      Bool overrideFlag = (pcSlice->getNumRefIdx( REF_PIC_LIST_0 )!=pcSlice->getPPS()->getNumRefIdxL0DefaultActive()||(pcSlice->isInterB()&&pcSlice->getNumRefIdx( REF_PIC_LIST_1 )!=pcSlice->getPPS()->getNumRefIdxL1DefaultActive()));
      WRITE_FLAG( overrideFlag ? 1 : 0,                               "num_ref_idx_active_override_flag");
      if (overrideFlag) 
      {
        WRITE_UVLC( pcSlice->getNumRefIdx( REF_PIC_LIST_0 ) - 1,      "num_ref_idx_l0_active_minus1" );
        if (pcSlice->isInterB())
        {
          WRITE_UVLC( pcSlice->getNumRefIdx( REF_PIC_LIST_1 ) - 1,    "num_ref_idx_l1_active_minus1" );
        }
        else
        {
          pcSlice->setNumRefIdx(REF_PIC_LIST_1, 0);
        }
      }
    }
    else
    {
      pcSlice->setNumRefIdx(REF_PIC_LIST_0, 0);
      pcSlice->setNumRefIdx(REF_PIC_LIST_1, 0);
    }
    if( pcSlice->getSPS()->getListsModificationPresentFlag() )
    {
      TComRefPicListModification* refPicListModification = pcSlice->getRefPicListModification();
      if(!pcSlice->isIntra())
      {
        WRITE_FLAG(pcSlice->getRefPicListModification()->getRefPicListModificationFlagL0() ? 1 : 0,       "ref_pic_list_modification_flag_l0" );
        if (pcSlice->getRefPicListModification()->getRefPicListModificationFlagL0())
        {
          Int numRpsCurrTempList0 = pcSlice->getNumRpsCurrTempList();
          if (numRpsCurrTempList0 > 1)
          {
            Int length = 1;
            numRpsCurrTempList0 --;
            while ( numRpsCurrTempList0 >>= 1) 
            {
              length ++;
            }
            for(Int i = 0; i < pcSlice->getNumRefIdx( REF_PIC_LIST_0 ); i++)
            {
              WRITE_CODE( refPicListModification->getRefPicSetIdxL0(i), length, "list_entry_l0");
            }
          }
        }
      }
      if(pcSlice->isInterB())
      {    
        WRITE_FLAG(pcSlice->getRefPicListModification()->getRefPicListModificationFlagL1() ? 1 : 0,       "ref_pic_list_modification_flag_l1" );
        if (pcSlice->getRefPicListModification()->getRefPicListModificationFlagL1())
        {
          Int numRpsCurrTempList1 = pcSlice->getNumRpsCurrTempList();
          if ( numRpsCurrTempList1 > 1 )
          {
            Int length = 1;
            numRpsCurrTempList1 --;
            while ( numRpsCurrTempList1 >>= 1)
            {
              length ++;
            }
            for(Int i = 0; i < pcSlice->getNumRefIdx( REF_PIC_LIST_1 ); i++)
            {
              WRITE_CODE( refPicListModification->getRefPicSetIdxL1(i), length, "list_entry_l1");
            }
          }
        }
      }
    }
  }
    
  if (pcSlice->isInterB())
  {
    WRITE_FLAG( pcSlice->getMvdL1ZeroFlag() ? 1 : 0,   "mvd_l1_zero_flag");
  }

  if(!pcSlice->isIntra())
  {
    if (!pcSlice->isIntra() && pcSlice->getPPS()->getCabacInitPresentFlag())
    {
      SliceType sliceType   = pcSlice->getSliceType();
      Int  encCABACTableIdx = pcSlice->getPPS()->getEncCABACTableIdx();
      Bool encCabacInitFlag = (sliceType!=encCABACTableIdx && encCABACTableIdx!=I_SLICE) ? true : false;
      pcSlice->setCabacInitFlag( encCabacInitFlag );
      WRITE_FLAG( encCabacInitFlag?1:0, "cabac_init_flag" );
    }
  }

  // if( !lightweight_slice_flag ) {
  if (!bDependentSlice)
  {
    Int iCode = pcSlice->getSliceQp() - ( pcSlice->getPPS()->getPicInitQPMinus26() + 26 );
    WRITE_SVLC( iCode, "slice_qp_delta" ); 
#if CHROMA_QP_EXTENSION
    if (pcSlice->getPPS()->getSliceChromaQpFlag())
    {
      iCode = pcSlice->getSliceQpDeltaCb();
      WRITE_SVLC( iCode, "slice_qp_delta_cb" );
      iCode = pcSlice->getSliceQpDeltaCr();
      WRITE_SVLC( iCode, "slice_qp_delta_cr" );
    }
#endif
    if (pcSlice->getPPS()->getDeblockingFilterControlPresent())
    {
      if (pcSlice->getPPS()->getLoopFilterOffsetInPPS() )
      {
        WRITE_FLAG(pcSlice->getInheritDblParamFromPPS(), "inherit_dbl_param_from_PPS_flag");
      }
      if (!pcSlice->getInheritDblParamFromPPS())
      {
        WRITE_FLAG(pcSlice->getLoopFilterDisable(), "loop_filter_disable");  // should be an IDC
        if(!pcSlice->getLoopFilterDisable())
        {
          WRITE_SVLC (pcSlice->getLoopFilterBetaOffset(), "beta_offset_div2");
          WRITE_SVLC (pcSlice->getLoopFilterTcOffset(), "tc_offset_div2");
        }
      }
    }
    if ( pcSlice->getEnableTMVPFlag() )
    {
    if ( pcSlice->getSliceType() == B_SLICE )
    {
      WRITE_FLAG( pcSlice->getColDir(), "collocated_from_l0_flag" );
    }

    if ( pcSlice->getSliceType() != I_SLICE &&
      ((pcSlice->getColDir()==0 && pcSlice->getNumRefIdx(REF_PIC_LIST_0)>1)||
      (pcSlice->getColDir()==1  && pcSlice->getNumRefIdx(REF_PIC_LIST_1)>1)))
    {
      WRITE_UVLC( pcSlice->getColRefIdx(), "collocated_ref_idx" );
    }
    }
    if ( (pcSlice->getPPS()->getUseWP() && pcSlice->getSliceType()==P_SLICE) || (pcSlice->getPPS()->getWPBiPred() && pcSlice->getSliceType()==B_SLICE) )
    {
      xCodePredWeightTable( pcSlice );
    }
  }

  // !!!! sytnax elements not in the WD !!!!
  
  assert(pcSlice->getMaxNumMergeCand()<=MRG_MAX_NUM_CANDS_SIGNALED);
  assert(MRG_MAX_NUM_CANDS_SIGNALED<=MRG_MAX_NUM_CANDS);
  WRITE_UVLC(MRG_MAX_NUM_CANDS - pcSlice->getMaxNumMergeCand(), "maxNumMergeCand");

#if !REMOVE_ALF
  if (!bDependentSlice)
  {
    if (pcSlice->getSPS()->getUseALF())
    {
      char syntaxString[50];
      for(Int compIdx=0; compIdx< 3; compIdx++)
      {
        sprintf(syntaxString, "alf_slice_filter_flag[%d]", compIdx);
        WRITE_FLAG( pcSlice->getAlfEnabledFlag(compIdx)?1:0, syntaxString );
      }
    }
  }
#endif
  if(!bDependentSlice)
  {
#if !REMOVE_ALF
    Bool isAlfEnabled = (!pcSlice->getSPS()->getUseALF())?(false):(pcSlice->getAlfEnabledFlag(0)||pcSlice->getAlfEnabledFlag(1)||pcSlice->getAlfEnabledFlag(2));
#endif
#if !SAO_LUM_CHROMA_ONOFF_FLAGS
    Bool isSAOEnabled = (!pcSlice->getSPS()->getUseSAO())?(false):(pcSlice->getSaoEnabledFlag());
#else
    Bool isSAOEnabled = (!pcSlice->getSPS()->getUseSAO())?(false):(pcSlice->getSaoEnabledFlag()||pcSlice->getSaoEnabledFlagChroma());
#endif
    Bool isDBFEnabled = (!pcSlice->getLoopFilterDisable());

#if REMOVE_ALF
#if MOVE_LOOP_FILTER_SLICES_FLAG
    if(pcSlice->getPPS()->getLFCrossSliceBoundaryFlag() && ( isSAOEnabled || isDBFEnabled ))
#else
    if(pcSlice->getSPS()->getLFCrossSliceBoundaryFlag() && ( isSAOEnabled || isDBFEnabled ))
#endif
#else
    if(pcSlice->getSPS()->getLFCrossSliceBoundaryFlag() && ( isAlfEnabled || isSAOEnabled || isDBFEnabled ))
#endif
    {
      WRITE_FLAG(pcSlice->getLFCrossSliceBoundaryFlag()?1:0, "slice_loop_filter_across_slices_enabled_flag");
    }
  }
#if SLICE_HEADER_EXTENSION
  if(pcSlice->getPPS()->getSliceHeaderExtensionPresentFlag())
  {
    WRITE_UVLC(0,"slice_header_extension_length");
  }
#endif
}

/**
 - write wavefront substreams sizes for the slice header.
 .
 \param pcSlice Where we find the substream size information.
 */
Void  TEncCavlc::codeTilesWPPEntryPoint( TComSlice* pSlice )
{
  Int tilesOrEntropyCodingSyncIdc = pSlice->getPPS()->getTilesOrEntropyCodingSyncIdc();
#if DEPENDENT_SLICES
  if ( (tilesOrEntropyCodingSyncIdc == 0) || pSlice->getPPS()->getDependentSlicesEnabledFlag() )
#else
  if ( tilesOrEntropyCodingSyncIdc == 0 )
#endif
  {
    return;
  }

  UInt numEntryPointOffsets = 0, offsetLenMinus1 = 0, maxOffset = 0;
  UInt *entryPointOffset = NULL;
  if (tilesOrEntropyCodingSyncIdc == 1) // tiles
  {
    numEntryPointOffsets = pSlice->getTileLocationCount();
    entryPointOffset     = new UInt[numEntryPointOffsets];
    for (Int idx=0; idx<pSlice->getTileLocationCount(); idx++)
    {
      if ( idx == 0 )
      {
        entryPointOffset [ idx ] = pSlice->getTileLocation( 0 );
      }
      else
      {
        entryPointOffset [ idx ] = pSlice->getTileLocation( idx ) - pSlice->getTileLocation( idx-1 );
      }

      if ( entryPointOffset[ idx ] > maxOffset )
      {
        maxOffset = entryPointOffset[ idx ];
      }
    }
  }
  else if (tilesOrEntropyCodingSyncIdc == 2) // wavefront
  {
    Int  numZeroSubstreamsAtEndOfSlice  = 0;
    UInt* pSubstreamSizes               = pSlice->getSubstreamSizes();
    // Find number of zero substreams at the end of slice
    for (Int idx=pSlice->getPPS()->getNumSubstreams()-2; idx>=0; idx--)
    {
      if ( pSubstreamSizes[ idx ] ==  0 )
      {
        numZeroSubstreamsAtEndOfSlice++; 
      }
      else
      {
        break;
      }
    }
    numEntryPointOffsets       = pSlice->getPPS()->getNumSubstreams() - 1 - numZeroSubstreamsAtEndOfSlice;
    entryPointOffset           = new UInt[numEntryPointOffsets];
    for (Int idx=0; idx<numEntryPointOffsets; idx++)
    {
      entryPointOffset[ idx ] = ( pSubstreamSizes[ idx ] >> 3 ) ;
      if ( entryPointOffset[ idx ] > maxOffset )
      {
        maxOffset = entryPointOffset[ idx ];
      }
    }
  }
  // Determine number of bits "offsetLenMinus1+1" required for entry point information
  offsetLenMinus1 = 0;
  while (maxOffset >= (1u << (offsetLenMinus1 + 1)))
  {
    offsetLenMinus1++;
    assert(offsetLenMinus1 + 1 < 32);    
  }

  WRITE_UVLC(numEntryPointOffsets, "num_entry_point_offsets");
  if (numEntryPointOffsets>0)
  {
    WRITE_UVLC(offsetLenMinus1, "offset_len_minus1");
  }

  for (UInt idx=0; idx<numEntryPointOffsets; idx++)
  {
    WRITE_CODE(entryPointOffset[ idx ], offsetLenMinus1+1, "entry_point_offset");
  }

  delete [] entryPointOffset;
}

Void TEncCavlc::codeTerminatingBit      ( UInt uilsLast )
{
}

Void TEncCavlc::codeSliceFinish ()
{
}

Void TEncCavlc::codeMVPIdx ( TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefList )
{
  assert(0);
}

Void TEncCavlc::codePartSize( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TEncCavlc::codePredMode( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

Void TEncCavlc::codeMergeFlag    ( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

Void TEncCavlc::codeMergeIndex    ( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

Void TEncCavlc::codeApsExtensionFlag ()
{
  WRITE_FLAG(0, "aps_extension_flag");
}

Void TEncCavlc::codeInterModeFlag( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth, UInt uiEncMode )
{
  assert(0);
}

Void TEncCavlc::codeCUTransquantBypassFlag( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

Void TEncCavlc::codeSkipFlag( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

Void TEncCavlc::codeSplitFlag   ( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TEncCavlc::codeTransformSubdivFlag( UInt uiSymbol, UInt uiCtx )
{
  assert(0);
}

Void TEncCavlc::codeQtCbf( TComDataCU* pcCU, UInt uiAbsPartIdx, TextType eType, UInt uiTrDepth )
{
  assert(0);
}

Void TEncCavlc::codeQtRootCbf( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

#if TU_ZERO_CBF_RDO
Void TEncCavlc::codeQtCbfZero( TComDataCU* pcCU, UInt uiAbsPartIdx, TextType eType, UInt uiTrDepth )
{
  assert(0);
}
Void TEncCavlc::codeQtRootCbfZero( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}
#endif

Void TEncCavlc::codeTransformSkipFlags (TComDataCU* pcCU, UInt uiAbsPartIdx, UInt width, UInt height, UInt uiDepth, TextType eTType )
{
  assert(0);
}

/** Code I_PCM information. 
 * \param pcCU pointer to CU
 * \param uiAbsPartIdx CU index
 * \param numIPCM the number of succesive IPCM blocks with the same size 
 * \param firstIPCMFlag 
 * \returns Void
 */
Void TEncCavlc::codeIPCMInfo( TComDataCU* pcCU, UInt uiAbsPartIdx, Int numIPCM, Bool firstIPCMFlag)
{
  assert(0);
}

Void TEncCavlc::codeIntraDirLumaAng( TComDataCU* pcCU, UInt uiAbsPartIdx, Bool isMultiple)
{
  assert(0);
}

Void TEncCavlc::codeIntraDirChroma( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

Void TEncCavlc::codeInterDir( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  assert(0);
}

Void TEncCavlc::codeRefFrmIdx( TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefList )
{
  assert(0);
}

Void TEncCavlc::codeMvd( TComDataCU* pcCU, UInt uiAbsPartIdx, RefPicList eRefList )
{
  assert(0);
}

Void TEncCavlc::codeDeltaQP( TComDataCU* pcCU, UInt uiAbsPartIdx )
{
  Int iDQp  = pcCU->getQP( uiAbsPartIdx ) - pcCU->getRefQP( uiAbsPartIdx );

  Int qpBdOffsetY =  pcCU->getSlice()->getSPS()->getQpBDOffsetY();
  iDQp = (iDQp + 78 + qpBdOffsetY + (qpBdOffsetY/2)) % (52 + qpBdOffsetY) - 26 - (qpBdOffsetY/2);

  xWriteSvlc( iDQp );
  
  return;
}

Void TEncCavlc::codeCoeffNxN    ( TComDataCU* pcCU, TCoeff* pcCoef, UInt uiAbsPartIdx, UInt uiWidth, UInt uiHeight, UInt uiDepth, TextType eTType )
{
  assert(0);
}

Void TEncCavlc::xGolombEncode(Int coeff, Int k)
{
  xWriteEpExGolomb((UInt)abs(coeff), k);
  if(coeff != 0)
  {
    Int sign = (coeff > 0)? 1: 0;
    xWriteFlag(sign);
  }
#if ENC_DEC_TRACE
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s se(v) : %d\n", "alf_filt_coeff", coeff ); 
#endif

}

#if !REMOVE_ALF
Void TEncCavlc::codeAlfParam(ALFParam* alfParam)
{
  char syntaxString[50];
  sprintf(syntaxString, "alf_aps_filter_flag[%d]", alfParam->componentID);
  WRITE_FLAG(alfParam->alf_flag, syntaxString);
  if(alfParam->alf_flag == 0)
  {
    return;
  }

  const Int numCoeff = (Int)ALF_MAX_NUM_COEF;
  switch(alfParam->componentID)
  {
  case ALF_Cb:
  case ALF_Cr:
    {
      for(Int pos=0; pos< numCoeff; pos++)
      {
        WRITE_SVLC(alfParam->coeffmulti[0][pos], "alf_filt_coeff");
      }
    }
    break;
  case ALF_Y:
    {
      Int noFilters = min(alfParam->filters_per_group-1, 2);
      WRITE_UVLC(noFilters, "alf_no_filters_minus1");
      if(noFilters == 1)
      {
        WRITE_UVLC(alfParam->startSecondFilter, "alf_start_second_filter");
      }
      else if (noFilters == 2)
      {
        Int numMergeFlags = 16;
        for (Int i=1; i<numMergeFlags; i++) 
        {
          WRITE_FLAG(alfParam->filterPattern[i], "alf_filter_pattern");
        }
      }
      for(Int f=0; f< alfParam->filters_per_group; f++)
      {
        for(Int pos=0; pos< numCoeff; pos++)
        {
          xGolombEncode(alfParam->coeffmulti[f][pos], kTableTabShapes[ALF_CROSS9x7_SQUARE3x3][pos]);
        }
      }
    }
    break;
  default:
    {
      printf("Not a legal component ID\n");
      assert(0);
      exit(-1);
    }
  }

}
#endif

Void TEncCavlc::estBit( estBitsSbacStruct* pcEstBitsCabac, Int width, Int height, TextType eTType )
{
  // printf("error : no VLC mode support in this version\n");
  return;
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

Void TEncCavlc::xWriteCode     ( UInt uiCode, UInt uiLength )
{
  assert ( uiLength > 0 );
  m_pcBitIf->write( uiCode, uiLength );
}

Void TEncCavlc::xWriteUvlc     ( UInt uiCode )
{
  UInt uiLength = 1;
  UInt uiTemp = ++uiCode;
  
  assert ( uiTemp );
  
  while( 1 != uiTemp )
  {
    uiTemp >>= 1;
    uiLength += 2;
  }
  
  //m_pcBitIf->write( uiCode, uiLength );
  // Take care of cases where uiLength > 32
  m_pcBitIf->write( 0, uiLength >> 1);
  m_pcBitIf->write( uiCode, (uiLength+1) >> 1);
}

Void TEncCavlc::xWriteSvlc     ( Int iCode )
{
  UInt uiCode;
  
  uiCode = xConvertToUInt( iCode );
  xWriteUvlc( uiCode );
}

Void TEncCavlc::xWriteFlag( UInt uiCode )
{
  m_pcBitIf->write( uiCode, 1 );
}

/** Write PCM alignment bits. 
 * \returns Void
 */
Void  TEncCavlc::xWritePCMAlignZero    ()
{
  m_pcBitIf->writeAlignZero();
}

Void TEncCavlc::xWriteUnaryMaxSymbol( UInt uiSymbol, UInt uiMaxSymbol )
{
  if (uiMaxSymbol == 0)
  {
    return;
  }
  xWriteFlag( uiSymbol ? 1 : 0 );
  if ( uiSymbol == 0 )
  {
    return;
  }
  
  Bool bCodeLast = ( uiMaxSymbol > uiSymbol );
  
  while( --uiSymbol )
  {
    xWriteFlag( 1 );
  }
  if( bCodeLast )
  {
    xWriteFlag( 0 );
  }
  return;
}

Void TEncCavlc::xWriteExGolombLevel( UInt uiSymbol )
{
  if( uiSymbol )
  {
    xWriteFlag( 1 );
    UInt uiCount = 0;
    Bool bNoExGo = (uiSymbol < 13);
    
    while( --uiSymbol && ++uiCount < 13 )
    {
      xWriteFlag( 1 );
    }
    if( bNoExGo )
    {
      xWriteFlag( 0 );
    }
    else
    {
      xWriteEpExGolomb( uiSymbol, 0 );
    }
  }
  else
  {
    xWriteFlag( 0 );
  }
  return;
}

Void TEncCavlc::xWriteEpExGolomb( UInt uiSymbol, UInt uiCount )
{
  while( uiSymbol >= (UInt)(1<<uiCount) )
  {
    xWriteFlag( 1 );
    uiSymbol -= 1<<uiCount;
    uiCount  ++;
  }
  xWriteFlag( 0 );
  while( uiCount-- )
  {
    xWriteFlag( (uiSymbol>>uiCount) & 1 );
  }
  return;
}

/** code explicit wp tables
 * \param TComSlice* pcSlice
 * \returns Void
 */
Void TEncCavlc::xCodePredWeightTable( TComSlice* pcSlice )
{
  wpScalingParam  *wp;
  Bool            bChroma     = true; // color always present in HEVC ?
  Int             iNbRef       = (pcSlice->getSliceType() == B_SLICE ) ? (2) : (1);
  Bool            bDenomCoded  = false;
  UInt            uiMode = 0;
#if NUM_WP_LIMIT
  UInt            uiTotalSignalledWeightFlags = 0;
#endif
  if ( (pcSlice->getSliceType()==P_SLICE && pcSlice->getPPS()->getUseWP()) || (pcSlice->getSliceType()==B_SLICE && pcSlice->getPPS()->getWPBiPred()) )
  {
    uiMode = 1; // explicit
  }
  if(uiMode == 1)
  {

    for ( Int iNumRef=0 ; iNumRef<iNbRef ; iNumRef++ ) 
    {
      RefPicList  eRefPicList = ( iNumRef ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

      for ( Int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ ) 
      {
        pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
        if ( !bDenomCoded ) 
        {
          Int iDeltaDenom;
          WRITE_UVLC( wp[0].uiLog2WeightDenom, "luma_log2_weight_denom" );     // ue(v): luma_log2_weight_denom

          if( bChroma )
          {
            iDeltaDenom = (wp[1].uiLog2WeightDenom - wp[0].uiLog2WeightDenom);
            WRITE_SVLC( iDeltaDenom, "delta_chroma_log2_weight_denom" );       // se(v): delta_chroma_log2_weight_denom
          }
          bDenomCoded = true;
        }
        WRITE_FLAG( wp[0].bPresentFlag, "luma_weight_lX_flag" );               // u(1): luma_weight_lX_flag
#if NUM_WP_LIMIT
        uiTotalSignalledWeightFlags += wp[0].bPresentFlag;
      }
      if (bChroma) 
      {
        for ( Int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ ) 
        {
          pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
          WRITE_FLAG( wp[1].bPresentFlag, "chroma_weight_lX_flag" );           // u(1): chroma_weight_lX_flag
          uiTotalSignalledWeightFlags += 2*wp[1].bPresentFlag;
        }
      }

      for ( Int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ ) 
      {
        pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
#endif
        if ( wp[0].bPresentFlag ) 
        {
          Int iDeltaWeight = (wp[0].iWeight - (1<<wp[0].uiLog2WeightDenom));
          WRITE_SVLC( iDeltaWeight, "delta_luma_weight_lX" );                  // se(v): delta_luma_weight_lX
          WRITE_SVLC( wp[0].iOffset, "luma_offset_lX" );                       // se(v): luma_offset_lX
        }

        if ( bChroma ) 
        {
#if !NUM_WP_LIMIT
          WRITE_FLAG( wp[1].bPresentFlag, "chroma_weight_lX_flag" );           // u(1): chroma_weight_lX_flag
#endif
          if ( wp[1].bPresentFlag )
          {
            for ( Int j=1 ; j<3 ; j++ ) 
            {
              Int iDeltaWeight = (wp[j].iWeight - (1<<wp[1].uiLog2WeightDenom));
              WRITE_SVLC( iDeltaWeight, "delta_chroma_weight_lX" );            // se(v): delta_chroma_weight_lX

              Int shift = ((1<<(g_uiBitDepth+g_uiBitIncrement-1)));
              Int pred = ( shift - ( ( shift*wp[j].iWeight)>>(wp[j].uiLog2WeightDenom) ) );
              Int iDeltaChroma = (wp[j].iOffset - pred);
              WRITE_SVLC( iDeltaChroma, "delta_chroma_offset_lX" );            // se(v): delta_chroma_offset_lX
            }
          }
        }
      }
    }
#if NUM_WP_LIMIT
    assert(uiTotalSignalledWeightFlags<=24);
#endif
  }
}

/** code quantization matrix
 *  \param scalingList quantization matrix information
 */
Void TEncCavlc::codeScalingList( TComScalingList* scalingList )
{
  UInt listId,sizeId;
  Bool scalingListPredModeFlag;

#if SCALING_LIST_OUTPUT_RESULT
  Int startBit;
  Int startTotalBit;
  startBit = m_pcBitIf->getNumberOfWrittenBits();
  startTotalBit = m_pcBitIf->getNumberOfWrittenBits();
#endif

    //for each size
    for(sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
      for(listId = 0; listId < g_scalingListNum[sizeId]; listId++)
      {
#if SCALING_LIST_OUTPUT_RESULT
        startBit = m_pcBitIf->getNumberOfWrittenBits();
#endif
        scalingListPredModeFlag = scalingList->checkPredMode( sizeId, listId );
        WRITE_FLAG( scalingListPredModeFlag, "scaling_list_pred_mode_flag" );
        if(!scalingListPredModeFlag)// Copy Mode
        {
          WRITE_UVLC( (Int)listId - (Int)scalingList->getRefMatrixId (sizeId,listId), "scaling_list_pred_matrix_id_delta");
        }
        else// DPCM Mode
        {
          xCodeScalingList(scalingList, sizeId, listId);
        }
#if SCALING_LIST_OUTPUT_RESULT
        printf("Matrix [%d][%d] Bit %d\n",sizeId,listId,m_pcBitIf->getNumberOfWrittenBits() - startBit);
#endif
      }
    }
#if SCALING_LIST_OUTPUT_RESULT
  printf("Total Bit %d\n",m_pcBitIf->getNumberOfWrittenBits()-startTotalBit);
#endif
  return;
}
/** code DPCM
 * \param scalingList quantization matrix information
 * \param sizeIdc size index
 * \param listIdc list index
 */
Void TEncCavlc::xCodeScalingList(TComScalingList* scalingList, UInt sizeId, UInt listId)
{
  Int coefNum = min(MAX_MATRIX_COEF_NUM,(Int)g_scalingListSize[sizeId]);
#if REMOVE_ZIGZAG_SCAN
  UInt* scan  = (sizeId == 0) ? g_auiSigLastScan [ SCAN_DIAG ] [ 1 ] :  g_sigLastScanCG32x32;
#else
  UInt* scan    = g_auiFrameScanXY [ (sizeId == 0)? 1 : 2];
#endif
  Int nextCoef = SCALING_LIST_START_VALUE;
  Int data;
  Int *src = scalingList->getScalingListAddress(sizeId, listId);
    if( sizeId > SCALING_LIST_8x8 )
    {
      WRITE_SVLC( scalingList->getScalingListDC(sizeId,listId) - 8, "scaling_list_dc_coef_minus8");
      nextCoef = scalingList->getScalingListDC(sizeId,listId);
    }
    for(Int i=0;i<coefNum;i++)
    {
      data = src[scan[i]] - nextCoef;
      nextCoef = src[scan[i]];
      if(data > 127)
      {
        data = data - 256;
      }
      if(data < -128)
      {
        data = data + 256;
      }

      WRITE_SVLC( data,  "scaling_list_delta_coef");
    }
}
#if LTRP_IN_SPS
Bool TEncCavlc::findMatchingLTRP ( TComSlice* pcSlice, UInt *ltrpsIndex, Int ltrpPOC, Bool usedFlag )
{
  // bool state = true, state2 = false;
  Int lsb = ltrpPOC % (1<<pcSlice->getSPS()->getBitsForPOC());
  for (Int k = 0; k < pcSlice->getSPS()->getNumLongTermRefPicSPS(); k++)
  {
    if ( (lsb == pcSlice->getSPS()->getLtRefPicPocLsbSps(k)) && (usedFlag == pcSlice->getSPS()->getUsedByCurrPicLtSPSFlag(k)) )
    {
      *ltrpsIndex = k;
      return true;
    }
  }
  return false;
}
#endif
Bool TComScalingList::checkPredMode(UInt sizeId, UInt listId)
{
  for(Int predListIdx = (Int)listId ; predListIdx >= 0; predListIdx--)
  {
    if( !memcmp(getScalingListAddress(sizeId,listId),((listId == predListIdx) ?
      getScalingListDefaultAddress(sizeId, predListIdx): getScalingListAddress(sizeId, predListIdx)),sizeof(Int)*min(MAX_MATRIX_COEF_NUM,(Int)g_scalingListSize[sizeId])) // check value of matrix
     && ((sizeId < SCALING_LIST_16x16) || (getScalingListDC(sizeId,listId) == getScalingListDC(sizeId,predListIdx)))) // check DC value
    {
      setRefMatrixId(sizeId, listId, predListIdx);
      return false;
    }
  }
  return true;
}
//! \}
