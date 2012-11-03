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

/** \file     TDecCAVLC.cpp
\brief    CAVLC decoder class
*/

#include "TDecCAVLC.h"
#include "SEIread.h"
#include "TDecSlice.h"

//! \ingroup TLibDecoder
//! \{

#if ENC_DEC_TRACE

#define READ_CODE(length, code, name)     xReadCodeTr ( length, code, name )
#define READ_UVLC(        code, name)     xReadUvlcTr (         code, name )
#define READ_SVLC(        code, name)     xReadSvlcTr (         code, name )
#define READ_FLAG(        code, name)     xReadFlagTr (         code, name )

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


Void  TDecCavlc::xReadCodeTr           (UInt length, UInt& rValue, const Char *pSymbolName)
{
  xReadCode (length, rValue);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s u(%d) : %d\n", pSymbolName, length, rValue ); 
  fflush ( g_hTrace );
}

Void  TDecCavlc::xReadUvlcTr           (UInt& rValue, const Char *pSymbolName)
{
  xReadUvlc (rValue);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s u(v) : %d\n", pSymbolName, rValue ); 
  fflush ( g_hTrace );
}

Void  TDecCavlc::xReadSvlcTr           (Int& rValue, const Char *pSymbolName)
{
  xReadSvlc(rValue);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s s(v) : %d\n", pSymbolName, rValue ); 
  fflush ( g_hTrace );
}

Void  TDecCavlc::xReadFlagTr           (UInt& rValue, const Char *pSymbolName)
{
  xReadFlag(rValue);
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s u(1) : %d\n", pSymbolName, rValue ); 
  fflush ( g_hTrace );
}

#else

#define READ_CODE(length, code, name)     xReadCode ( length, code )
#define READ_UVLC(        code, name)     xReadUvlc (         code )
#define READ_SVLC(        code, name)     xReadSvlc (         code )
#define READ_FLAG(        code, name)     xReadFlag (         code )

#endif



// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TDecCavlc::TDecCavlc()
{
#if !REMOVE_FGS
  m_iSliceGranularity = 0;
#endif
}

TDecCavlc::~TDecCavlc()
{

}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

/**
* unmarshal a sequence of SEI messages from bitstream.
*/
void TDecCavlc::parseSEI(SEImessages& seis)
{
  assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  do
  {
    parseSEImessage(*m_pcBitstream, seis);
    /* SEI messages are an integer number of bytes, something has failed
    * in the parsing if bitstream not byte-aligned */
    assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  } while (0x80 != m_pcBitstream->peekBits(8));
  assert(m_pcBitstream->getNumBitsLeft() == 8); /* rsbp_trailing_bits */
}
void TDecCavlc::parseShortTermRefPicSet( TComSPS* sps, TComReferencePictureSet* rps, Int idx )
{
  UInt code;
  UInt interRPSPred;
  READ_FLAG(interRPSPred, "inter_ref_pic_set_prediction_flag");  rps->setInterRPSPrediction(interRPSPred);
  if (interRPSPred) 
  {
    UInt bit;
#if J0234_INTER_RPS_SIMPL
    if(idx == sps->getRPSList()->getNumberOfReferencePictureSets())
    {
      READ_UVLC(code, "delta_idx_minus1" ); // delta index of the Reference Picture Set used for prediction minus 1
    }
    else
    {
      code = 0;
    }
    assert(code <= idx-1); // delta_idx_minus1 shall not be larger than idx-1, otherwise we will predict from a negative row position that does not exist. When idx equals 0 there is no legal value and interRPSPred must be zero. See J0185-r2
#else
    READ_UVLC(code, "delta_idx_minus1" ); // delta index of the Reference Picture Set used for prediction minus 1
#endif
    Int rIdx =  idx - 1 - code;
#if J0234_INTER_RPS_SIMPL
    assert (rIdx <= idx-1 && rIdx >= 0); // Made assert tighter; if rIdx = idx then prediction is done from itself. rIdx must belong to range 0, idx-1, inclusive, see J0185-r2
#else
    assert (rIdx <= idx && rIdx >= 0);
#endif
    TComReferencePictureSet*   rpsRef = sps->getRPSList()->getReferencePictureSet(rIdx);
    Int k = 0, k0 = 0, k1 = 0;
    READ_CODE(1, bit, "delta_rps_sign"); // delta_RPS_sign
    READ_UVLC(code, "abs_delta_rps_minus1");  // absolute delta RPS minus 1
    Int deltaRPS = (1 - (bit<<1)) * (code + 1); // delta_RPS
    for(Int j=0 ; j <= rpsRef->getNumberOfPictures(); j++)
    {
      READ_CODE(1, bit, "used_by_curr_pic_flag" ); //first bit is "1" if Idc is 1 
      Int refIdc = bit;
      if (refIdc == 0) 
      {
        READ_CODE(1, bit, "use_delta_flag" ); //second bit is "1" if Idc is 2, "0" otherwise.
        refIdc = bit<<1; //second bit is "1" if refIdc is 2, "0" if refIdc = 0.
      }
      if (refIdc == 1 || refIdc == 2)
      {
        Int deltaPOC = deltaRPS + ((j < rpsRef->getNumberOfPictures())? rpsRef->getDeltaPOC(j) : 0);
        rps->setDeltaPOC(k, deltaPOC);
        rps->setUsed(k, (refIdc == 1));

        if (deltaPOC < 0) {
          k0++;
        }
        else 
        {
          k1++;
        }
        k++;
      }  
      rps->setRefIdc(j,refIdc);  
    }
    rps->setNumRefIdc(rpsRef->getNumberOfPictures()+1);  
    rps->setNumberOfPictures(k);
    rps->setNumberOfNegativePictures(k0);
    rps->setNumberOfPositivePictures(k1);
    rps->sortDeltaPOC();
  }
  else
  {
    READ_UVLC(code, "num_negative_pics");           rps->setNumberOfNegativePictures(code);
    READ_UVLC(code, "num_positive_pics");           rps->setNumberOfPositivePictures(code);
    Int prev = 0;
    Int poc;
    for(Int j=0 ; j < rps->getNumberOfNegativePictures(); j++)
    {
      READ_UVLC(code, "delta_poc_s0_minus1");
      poc = prev-code-1;
      prev = poc;
      rps->setDeltaPOC(j,poc);
      READ_FLAG(code, "used_by_curr_pic_s0_flag");  rps->setUsed(j,code);
    }
    prev = 0;
    for(Int j=rps->getNumberOfNegativePictures(); j < rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures(); j++)
    {
      READ_UVLC(code, "delta_poc_s1_minus1");
      poc = prev+code+1;
      prev = poc;
      rps->setDeltaPOC(j,poc);
      READ_FLAG(code, "used_by_curr_pic_s1_flag");  rps->setUsed(j,code);
    }
    rps->setNumberOfPictures(rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures());
  }
#if PRINT_RPS_INFO
  rps->printDeltaPOC();
#endif
}

#if !REMOVE_APS
Void TDecCavlc::parseAPS(TComAPS* aps)
{
#if ENC_DEC_TRACE  
  xTraceAPSHeader(aps);
#endif

  UInt uiCode;
  READ_UVLC(uiCode, "aps_id");                             aps->setAPSID(uiCode);
#if !REMOVE_ALF
  for(Int compIdx=0; compIdx< 3; compIdx++)
  {
    xParseAlfParam( (aps->getAlfParam())[compIdx]);
  }
#endif
  READ_FLAG( uiCode, "aps_extension_flag");
  if (uiCode)
  {
    while ( xMoreRbspData() )
    {
      READ_FLAG( uiCode, "aps_extension_data_flag");
    }
  }

}
#endif

/** copy SAO parameter
* \param dst  
* \param src 
*/
inline Void copySaoOneLcuParam(SaoLcuParam* dst,  SaoLcuParam* src)
{
  Int i;
  dst->partIdx = src->partIdx;
  dst->typeIdx = src->typeIdx;
  if (dst->typeIdx != -1)
  {
#if SAO_TYPE_CODING
    dst->subTypeIdx = src->subTypeIdx ;
#else
    if (dst->typeIdx == SAO_BO)
    {
      dst->bandPosition = src->bandPosition ;
    }
    else
    {
      dst->bandPosition = 0;
    }
#endif
    dst->length  = src->length;
    for (i=0;i<dst->length;i++)
    {
      dst->offset[i] = src->offset[i];
    }
  }
  else
  {
    dst->length  = 0;
    for (i=0;i<SAO_BO_LEN;i++)
    {
      dst->offset[i] = 0;
    }
  }
}

#if !REMOVE_ALF
Void TDecCavlc::xParseAlfParam(ALFParam* pAlfParam)
{
  UInt uiSymbol;
  char syntaxString[50];
  sprintf(syntaxString, "alf_aps_filter_flag[%d]", pAlfParam->componentID);
  READ_FLAG(uiSymbol, syntaxString);
  pAlfParam->alf_flag = uiSymbol;
  if(pAlfParam->alf_flag ==0)
  {
    return;
  }
  Int iSymbol;
  pAlfParam->num_coeff = (Int)ALF_MAX_NUM_COEF;
  switch(pAlfParam->componentID)
  {
  case ALF_Cb:
  case ALF_Cr:
    {
      pAlfParam->filter_shape = ALF_CROSS9x7_SQUARE3x3;
      pAlfParam->filters_per_group = 1;
      for(Int pos=0; pos< pAlfParam->num_coeff; pos++)
      {
        READ_SVLC(iSymbol, "alf_filt_coeff");
        pAlfParam->coeffmulti[0][pos] = iSymbol;
      }
    }
    break;
  case ALF_Y:
    {
      pAlfParam->filters_per_group = 0;
      memset (pAlfParam->filterPattern, 0 , sizeof(Int)*NO_VAR_BINS);
      pAlfParam->filter_shape = 0;
      // filters_per_fr
      READ_UVLC (uiSymbol, "alf_no_filters_minus1");
      pAlfParam->filters_per_group = uiSymbol + 1;

      if(uiSymbol == 1) // filters_per_group == 2
      {
        READ_UVLC (uiSymbol, "alf_start_second_filter");
        pAlfParam->startSecondFilter = uiSymbol;
        pAlfParam->filterPattern [uiSymbol] = 1;
      }
      else if (uiSymbol > 1) // filters_per_group > 2
      {
        pAlfParam->filters_per_group = 1;
        Int numMergeFlags = 16;
        for (Int i=1; i<numMergeFlags; i++) 
        {
          READ_FLAG (uiSymbol,  "alf_filter_pattern");
          pAlfParam->filterPattern[i] = uiSymbol;
          pAlfParam->filters_per_group += uiSymbol;
        }
      }
      for(Int idx = 0; idx < pAlfParam->filters_per_group; ++idx)
      {
        for(Int i = 0; i < pAlfParam->num_coeff; i++)
        {
          pAlfParam->coeffmulti[idx][i] = xGolombDecode(kTableTabShapes[ALF_CROSS9x7_SQUARE3x3][i]);
        }
      }
    }
    break;
  default:
    {
      printf("Not a legal component ID for ALF\n");
      assert(0);
      exit(-1);
    }
  }
}

Int TDecCavlc::xGolombDecode(Int k)
{
  Int coeff;
  UInt symbol;
  xReadEpExGolomb( symbol, k );
  coeff = symbol;
  if(symbol != 0)
  {
    xReadFlag(symbol);
    if(symbol == 0)
    {
      coeff = -coeff;
    }
  }
#if ENC_DEC_TRACE
  fprintf( g_hTrace, "%8lld  ", g_nSymbolCounter++ );
  fprintf( g_hTrace, "%-40s se(v) : %d\n", "alf_filt_coeff", coeff ); 
#endif
  return coeff;
}
#endif

Void TDecCavlc::parsePPS(TComPPS* pcPPS)
{
#if ENC_DEC_TRACE  
  xTracePPSHeader (pcPPS);
#endif
  UInt  uiCode;

  Int   iCode;

  READ_UVLC( uiCode, "pic_parameter_set_id");                      pcPPS->setPPSId (uiCode);
  READ_UVLC( uiCode, "seq_parameter_set_id");                      pcPPS->setSPSId (uiCode);

  READ_FLAG ( uiCode, "sign_data_hiding_flag" ); pcPPS->setSignHideFlag( uiCode );

  READ_FLAG( uiCode,   "cabac_init_present_flag" );            pcPPS->setCabacInitPresentFlag( uiCode ? true : false );

  READ_UVLC(uiCode, "num_ref_idx_l0_default_active_minus1");       pcPPS->setNumRefIdxL0DefaultActive(uiCode+1);
  READ_UVLC(uiCode, "num_ref_idx_l1_default_active_minus1");       pcPPS->setNumRefIdxL1DefaultActive(uiCode+1);

  READ_SVLC(iCode, "pic_init_qp_minus26" );                        pcPPS->setPicInitQPMinus26(iCode);
  READ_FLAG( uiCode, "constrained_intra_pred_flag" );              pcPPS->setConstrainedIntraPred( uiCode ? true : false );
#if PPS_TS_FLAG  
  READ_FLAG( uiCode, "transform_skip_enabled_flag" );               
  pcPPS->setUseTransformSkip ( uiCode ? true : false ); 
#endif

#if !REMOVE_FGS
  READ_CODE( 2, uiCode, "slice_granularity" );                     pcPPS->setSliceGranularity(uiCode);
#endif
  
  // alf_param() ?
#if CU_DQP_ENABLE_FLAG
  READ_FLAG( uiCode, "cu_qp_delta_enabled_flag" );            pcPPS->setUseDQP( uiCode ? true : false );
  if( pcPPS->getUseDQP() )
  {
    READ_UVLC( uiCode, "diff_cu_qp_delta_depth" );
#if REMOVE_FGS
    pcPPS->setMaxCuDQPDepth( uiCode );
#else
    pcPPS->setMaxCuDQPDepth( uiCode + pcPPS->getSliceGranularity() );
#endif
  }
  else
  {
    pcPPS->setMaxCuDQPDepth( 0 );
  }
#else
  READ_UVLC( uiCode, "diff_cu_qp_delta_depth");
  if(uiCode == 0)
  {
    pcPPS->setUseDQP (false);
    pcPPS->setMaxCuDQPDepth( 0 );
  }
  else
  {
    pcPPS->setUseDQP (true);
#if REMOVE_FGS
    pcPPS->setMaxCuDQPDepth(uiCode - 1);
#else
    pcPPS->setMaxCuDQPDepth(uiCode + pcPPS->getSliceGranularity() - 1);
#endif
  }
#endif
  READ_SVLC( iCode, "cb_qp_offset");
  pcPPS->setChromaCbQpOffset(iCode);
#if CHROMA_QP_EXTENSION
  assert( pcPPS->getChromaCbQpOffset() >= -12 );
  assert( pcPPS->getChromaCbQpOffset() <=  12 );
#endif

  READ_SVLC( iCode, "cr_qp_offset");
  pcPPS->setChromaCrQpOffset(iCode);
#if CHROMA_QP_EXTENSION
  assert( pcPPS->getChromaCrQpOffset() >= -12 );
  assert( pcPPS->getChromaCrQpOffset() <=  12 );
#endif

#if CHROMA_QP_EXTENSION
  READ_FLAG( uiCode, "slicelevel_chroma_qp_flag" );
  pcPPS->setSliceChromaQpFlag( uiCode ? true : false );
#endif

  READ_FLAG( uiCode, "weighted_pred_flag" );          // Use of Weighting Prediction (P_SLICE)
  pcPPS->setUseWP( uiCode==1 );
  READ_FLAG( uiCode, "weighted_bipred_flag" );         // Use of Bi-Directional Weighting Prediction (B_SLICE)
  pcPPS->setWPBiPred( uiCode==1 );
  printf("TDecCavlc::parsePPS():\tm_bUseWeightPred=%d\tm_uiBiPredIdc=%d\n", pcPPS->getUseWP(), pcPPS->getWPBiPred());

  READ_FLAG( uiCode, "output_flag_present_flag" );
  pcPPS->setOutputFlagPresentFlag( uiCode==1 );

#if DEPENDENT_SLICES
  READ_FLAG( uiCode, "dependent_slices_enabled_flag" );
  pcPPS->setDependentSlicesEnabledFlag( uiCode==1 );
#endif

  READ_FLAG( uiCode, "transquant_bypass_enable_flag");
  pcPPS->setTransquantBypassEnableFlag(uiCode ? true : false);

  READ_CODE(2, uiCode, "tiles_or_entropy_coding_sync_idc");         pcPPS->setTilesOrEntropyCodingSyncIdc(uiCode);
  if(pcPPS->getTilesOrEntropyCodingSyncIdc() == 1)
  {
    READ_UVLC ( uiCode, "num_tile_columns_minus1" );   
    pcPPS->setNumColumnsMinus1( uiCode );  
    READ_UVLC ( uiCode, "num_tile_rows_minus1" );  
    pcPPS->setNumRowsMinus1( uiCode );  
    READ_FLAG ( uiCode, "uniform_spacing_flag" );  
    pcPPS->setUniformSpacingIdr( uiCode );

    if( pcPPS->getUniformSpacingIdr() == 0 )
    {
      UInt* columnWidth = (UInt*)malloc(pcPPS->getNumColumnsMinus1()*sizeof(UInt));
      for(UInt i=0; i<pcPPS->getNumColumnsMinus1(); i++)
      { 
        READ_UVLC( uiCode, "column_width" );  
        columnWidth[i] = uiCode;  
      }
      pcPPS->setColumnWidth(columnWidth);
      free(columnWidth);

      UInt* rowHeight = (UInt*)malloc(pcPPS->getNumRowsMinus1()*sizeof(UInt));
      for(UInt i=0; i<pcPPS->getNumRowsMinus1(); i++)
      {
        READ_UVLC( uiCode, "row_height" );  
        rowHeight[i] = uiCode;  
      }
      pcPPS->setRowHeight(rowHeight);
      free(rowHeight);  
    }

    if(pcPPS->getNumColumnsMinus1() !=0 || pcPPS->getNumRowsMinus1() !=0)
    {
      READ_FLAG ( uiCode, "loop_filter_across_tiles_enabled_flag" );  
      pcPPS->setLFCrossTileBoundaryFlag( (uiCode == 1)?true:false );
    }
  }
#if DEPENDENT_SLICES
  else if( pcPPS->getTilesOrEntropyCodingSyncIdc()==3 )
  {
    READ_FLAG ( uiCode, "cabac_independent_flag" );
    pcPPS->setCabacIndependentFlag( (uiCode == 1)? true : false );
  }
#endif
#if MOVE_LOOP_FILTER_SLICES_FLAG
  READ_FLAG( uiCode, "loop_filter_across_slice_flag" );             pcPPS->setLFCrossSliceBoundaryFlag( uiCode ? true : false);
#endif
  READ_FLAG( uiCode, "deblocking_filter_control_present_flag" ); 
  pcPPS->setDeblockingFilterControlPresent( uiCode ? true : false);
  if(pcPPS->getDeblockingFilterControlPresent())
  {
    READ_FLAG( uiCode, "pps_deblocking_filter_flag" );
    pcPPS->setLoopFilterOffsetInPPS( (uiCode==1)?true:false );
    if(pcPPS->getLoopFilterOffsetInPPS())
    {
      READ_FLAG ( uiCode, "disable_deblocking_filter_flag" );
      pcPPS->setLoopFilterDisable(uiCode ? 1 : 0);
      if(!pcPPS->getLoopFilterDisable())
      {
        READ_SVLC ( iCode, "pps_beta_offset_div2" );
        pcPPS->setLoopFilterBetaOffset( iCode );
        READ_SVLC ( iCode, "pps_tc_offset_div2" ); 
        pcPPS->setLoopFilterTcOffset( iCode );
      }
    }
  }
  READ_FLAG( uiCode, "pps_scaling_list_data_present_flag" );                 pcPPS->setScalingListPresentFlag ( (uiCode==1)?true:false );
  if(pcPPS->getScalingListPresentFlag ())
  {
    parseScalingList( pcPPS->getScalingList() );
  }
  READ_UVLC( uiCode, "log2_parallel_merge_level_minus2");
  pcPPS->setLog2ParallelMergeLevelMinus2 (uiCode);

#if SLICE_HEADER_EXTENSION
  READ_FLAG( uiCode, "slice_header_extension_present_flag");
  pcPPS->setSliceHeaderExtensionPresentFlag(uiCode);
#endif

  READ_FLAG( uiCode, "pps_extension_flag");
  if (uiCode)
  {
    while ( xMoreRbspData() )
    {
      READ_FLAG( uiCode, "pps_extension_data_flag");
    }
  }
}

Void TDecCavlc::parseSPS(TComSPS* pcSPS)
{
#if ENC_DEC_TRACE  
  xTraceSPSHeader (pcSPS);
#endif

  UInt  uiCode;

  READ_CODE( 3,  uiCode, "profile_space" );                      pcSPS->setProfileSpace( uiCode );
  READ_CODE( 5,  uiCode, "profile_idc" );                        pcSPS->setProfileIdc( uiCode );
  READ_CODE(16,  uiCode, "reserved_indicator_flags" );           pcSPS->setRsvdIndFlags( uiCode );
  READ_CODE( 8,  uiCode, "level_idc" );                          pcSPS->setLevelIdc( uiCode );
  READ_CODE(32,  uiCode, "profile_compatibility");               pcSPS->setProfileCompat( uiCode );
  READ_UVLC(     uiCode, "seq_parameter_set_id" );               pcSPS->setSPSId( uiCode );
  READ_UVLC(     uiCode, "video_parameter_set_id" );             pcSPS->setVPSId( uiCode );
  READ_UVLC(     uiCode, "chroma_format_idc" );                  pcSPS->setChromaFormatIdc( uiCode );
  READ_CODE( 3,  uiCode, "max_temporal_layers_minus1" );         pcSPS->setMaxTLayers( uiCode+1 );
  READ_UVLC (    uiCode, "pic_width_in_luma_samples" );          pcSPS->setPicWidthInLumaSamples ( uiCode    );
  READ_UVLC (    uiCode, "pic_height_in_luma_samples" );         pcSPS->setPicHeightInLumaSamples( uiCode    );
  READ_FLAG(     uiCode, "pic_cropping_flag");                   pcSPS->setPicCroppingFlag ( uiCode ? true : false );
  if (uiCode != 0)
  {
    READ_UVLC(   uiCode, "pic_crop_left_offset" );               pcSPS->setPicCropLeftOffset  ( uiCode * TComSPS::getCropUnitX( pcSPS->getChromaFormatIdc() ) );
    READ_UVLC(   uiCode, "pic_crop_right_offset" );              pcSPS->setPicCropRightOffset ( uiCode * TComSPS::getCropUnitX( pcSPS->getChromaFormatIdc() ) );
    READ_UVLC(   uiCode, "pic_crop_top_offset" );                pcSPS->setPicCropTopOffset   ( uiCode * TComSPS::getCropUnitY( pcSPS->getChromaFormatIdc() ) );
    READ_UVLC(   uiCode, "pic_crop_bottom_offset" );             pcSPS->setPicCropBottomOffset( uiCode * TComSPS::getCropUnitY( pcSPS->getChromaFormatIdc() ) );
  }

#if FULL_NBIT
  READ_UVLC(     uiCode, "bit_depth_luma_minus8" );
  g_uiBitDepth = 8 + uiCode;
  g_uiBitIncrement = 0;
  pcSPS->setBitDepth(g_uiBitDepth);
  pcSPS->setBitIncrement(g_uiBitIncrement);
  UInt m_uiSaoBitIncrease = g_uiBitDepth + (g_uiBitDepth-8) - min((Int)(g_uiBitDepth + (g_uiBitDepth-8)), 10);
#else
  READ_UVLC(     uiCode, "bit_depth_luma_minus8" );
  g_uiBitDepth = 8;
  g_uiBitIncrement = uiCode;
  pcSPS->setBitDepth(g_uiBitDepth);
  pcSPS->setBitIncrement(g_uiBitIncrement);
#endif
  pcSPS->setQpBDOffsetY( (Int) (6*uiCode) );

  g_uiBASE_MAX  = ((1<<(g_uiBitDepth))-1);

#if IBDI_NOCLIP_RANGE
  g_uiIBDI_MAX  = g_uiBASE_MAX << g_uiBitIncrement;
#else
  g_uiIBDI_MAX  = ((1<<(g_uiBitDepth+g_uiBitIncrement))-1);
#endif
  READ_UVLC( uiCode,    "bit_depth_chroma_minus8" );
  pcSPS->setQpBDOffsetC( (Int) (6*uiCode) );

  READ_FLAG( uiCode, "pcm_enabled_flag" ); pcSPS->setUsePCM( uiCode ? true : false );

  if( pcSPS->getUsePCM() )
  {
    READ_CODE( 4, uiCode, "pcm_bit_depth_luma_minus1" );           pcSPS->setPCMBitDepthLuma   ( 1 + uiCode );
    READ_CODE( 4, uiCode, "pcm_bit_depth_chroma_minus1" );         pcSPS->setPCMBitDepthChroma ( 1 + uiCode );
  }

  READ_UVLC( uiCode,    "log2_max_pic_order_cnt_lsb_minus4" );   pcSPS->setBitsForPOC( 4 + uiCode );
  for(UInt i=0; i <= pcSPS->getMaxTLayers()-1; i++)
  {
    READ_UVLC ( uiCode, "max_dec_pic_buffering");
    pcSPS->setMaxDecPicBuffering( uiCode, i);
    READ_UVLC ( uiCode, "num_reorder_pics" );
    pcSPS->setNumReorderPics(uiCode, i);
    READ_UVLC ( uiCode, "max_latency_increase");
    pcSPS->setMaxLatencyIncrease( uiCode, i );
  }

  READ_FLAG( uiCode, "restricted_ref_pic_lists_flag" );
  pcSPS->setRestrictedRefPicListsFlag( uiCode );
  if( pcSPS->getRestrictedRefPicListsFlag() )
  {
    READ_FLAG( uiCode, "lists_modification_present_flag" );
    pcSPS->setListsModificationPresentFlag(uiCode);
  }
  else 
  {
    pcSPS->setListsModificationPresentFlag(true);
  }
  READ_UVLC( uiCode, "log2_min_coding_block_size_minus3" );
  UInt log2MinCUSize = uiCode + 3;
  READ_UVLC( uiCode, "log2_diff_max_min_coding_block_size" );
  UInt uiMaxCUDepthCorrect = uiCode;
  pcSPS->setMaxCUWidth  ( 1<<(log2MinCUSize + uiMaxCUDepthCorrect) ); g_uiMaxCUWidth  = 1<<(log2MinCUSize + uiMaxCUDepthCorrect);
  pcSPS->setMaxCUHeight ( 1<<(log2MinCUSize + uiMaxCUDepthCorrect) ); g_uiMaxCUHeight = 1<<(log2MinCUSize + uiMaxCUDepthCorrect);
  READ_UVLC( uiCode, "log2_min_transform_block_size_minus2" );   pcSPS->setQuadtreeTULog2MinSize( uiCode + 2 );

  READ_UVLC( uiCode, "log2_diff_max_min_transform_block_size" ); pcSPS->setQuadtreeTULog2MaxSize( uiCode + pcSPS->getQuadtreeTULog2MinSize() );
  pcSPS->setMaxTrSize( 1<<(uiCode + pcSPS->getQuadtreeTULog2MinSize()) );
  if( pcSPS->getUsePCM() )
  {
    READ_UVLC( uiCode, "log2_min_pcm_coding_block_size_minus3" );  pcSPS->setPCMLog2MinSize (uiCode+3); 
    READ_UVLC( uiCode, "log2_diff_max_min_pcm_coding_block_size" ); pcSPS->setPCMLog2MaxSize ( uiCode+pcSPS->getPCMLog2MinSize() );
  }

  READ_UVLC( uiCode, "max_transform_hierarchy_depth_inter" );    pcSPS->setQuadtreeTUMaxDepthInter( uiCode+1 );
  READ_UVLC( uiCode, "max_transform_hierarchy_depth_intra" );    pcSPS->setQuadtreeTUMaxDepthIntra( uiCode+1 );
  g_uiAddCUDepth = 0;
  while( ( pcSPS->getMaxCUWidth() >> uiMaxCUDepthCorrect ) > ( 1 << ( pcSPS->getQuadtreeTULog2MinSize() + g_uiAddCUDepth )  ) )
  {
    g_uiAddCUDepth++;
  }
  pcSPS->setMaxCUDepth( uiMaxCUDepthCorrect+g_uiAddCUDepth  ); 
  g_uiMaxCUDepth  = uiMaxCUDepthCorrect+g_uiAddCUDepth;
  // BB: these parameters may be removed completly and replaced by the fixed values
  pcSPS->setMinTrDepth( 0 );
  pcSPS->setMaxTrDepth( 1 );
  READ_FLAG( uiCode, "scaling_list_enabled_flag" );                 pcSPS->setScalingListFlag ( uiCode );
  if(pcSPS->getScalingListFlag())
  {
    READ_FLAG( uiCode, "sps_scaling_list_data_present_flag" );                 pcSPS->setScalingListPresentFlag ( uiCode );
    if(pcSPS->getScalingListPresentFlag ())
    {
      parseScalingList( pcSPS->getScalingList() );
    }
  }
#if !REMOVE_LMCHROMA
  READ_FLAG( uiCode, "chroma_pred_from_luma_enabled_flag" );        pcSPS->setUseLMChroma ( uiCode ? true : false );
#endif
#if !PPS_TS_FLAG
  READ_FLAG( uiCode, "transform_skip_enabled_flag" );               pcSPS->setUseTransformSkip ( uiCode ? true : false );
#endif
#if !MOVE_LOOP_FILTER_SLICES_FLAG
  READ_FLAG( uiCode, "loop_filter_across_slice_flag" );             pcSPS->setLFCrossSliceBoundaryFlag( uiCode ? true : false);
#endif
  READ_FLAG( uiCode, "asymmetric_motion_partitions_enabled_flag" ); pcSPS->setUseAMP( uiCode );
#if !REMOVE_NSQT
  READ_FLAG( uiCode, "non_square_quadtree_enabled_flag" );          pcSPS->setUseNSQT( uiCode );
#endif
  READ_FLAG( uiCode, "sample_adaptive_offset_enabled_flag" );       pcSPS->setUseSAO ( uiCode ? true : false );
#if !REMOVE_ALF
  READ_FLAG( uiCode, "adaptive_loop_filter_enabled_flag" );         pcSPS->setUseALF ( uiCode ? true : false );
#endif
  if( pcSPS->getUsePCM() )
  {
    READ_FLAG( uiCode, "pcm_loop_filter_disable_flag" );           pcSPS->setPCMFilterDisableFlag ( uiCode ? true : false );
  }

  READ_FLAG( uiCode, "temporal_id_nesting_flag" );               pcSPS->setTemporalIdNestingFlag ( uiCode > 0 ? true : false );

  READ_UVLC( uiCode, "num_short_term_ref_pic_sets" );
  pcSPS->createRPSList(uiCode);

  TComRPSList* rpsList = pcSPS->getRPSList();
  TComReferencePictureSet* rps;

  for(UInt i=0; i< rpsList->getNumberOfReferencePictureSets(); i++)
  {
    rps = rpsList->getReferencePictureSet(i);
    parseShortTermRefPicSet(pcSPS,rps,i);
  }
  READ_FLAG( uiCode, "long_term_ref_pics_present_flag" );          pcSPS->setLongTermRefsPresent(uiCode);
#if LTRP_IN_SPS
  if (pcSPS->getLongTermRefsPresent()) 
  {
    READ_UVLC( uiCode, "num_long_term_ref_pic_sps" );
    pcSPS->setNumLongTermRefPicSPS(uiCode);
    for (UInt k = 0; k < pcSPS->getNumLongTermRefPicSPS(); k++)
    {
      READ_CODE( pcSPS->getBitsForPOC(), uiCode, "lt_ref_pic_poc_lsb_sps" );
      pcSPS->setLtRefPicPocLsbSps(uiCode, k);
      READ_FLAG( uiCode,  "used_by_curr_pic_lt_sps_flag[i]");
      pcSPS->setUsedByCurrPicLtSPSFlag(k, uiCode?1:0);
    }
  }
#endif
  READ_FLAG( uiCode, "sps_temporal_mvp_enable_flag" );            pcSPS->setTMVPFlagsPresent(uiCode);
  // AMVP mode for each depth (AM_NONE or AM_EXPL)
  for (Int i = 0; i < pcSPS->getMaxCUDepth(); i++)
  {
    xReadFlag( uiCode );
    pcSPS->setAMVPMode( i, (AMVP_MODE)uiCode );
  }

  READ_FLAG( uiCode, "sps_extension_flag");
  if (uiCode)
  {
    while ( xMoreRbspData() )
    {
      READ_FLAG( uiCode, "sps_extension_data_flag");
    }
  }
}

Void TDecCavlc::parseVPS(TComVPS* pcVPS)
{
  UInt  uiCode;
  
  READ_CODE( 3, uiCode, "vps_max_temporal_layers_minus1" );   pcVPS->setMaxTLayers( uiCode + 1 );
  READ_CODE( 5, uiCode, "vps_max_layers_minus1" );            pcVPS->setMaxLayers( uiCode + 1 );
  READ_UVLC( uiCode,  "video_parameter_set_id" );             pcVPS->setVPSId( uiCode );
  READ_FLAG( uiCode,  "vps_temporal_id_nesting_flag" );       pcVPS->setTemporalNestingFlag( uiCode ? true:false );
  for(UInt i = 0; i <= pcVPS->getMaxTLayers()-1; i++)
  {
    READ_UVLC( uiCode,  "vps_max_dec_pic_buffering[i]" );     pcVPS->setMaxDecPicBuffering( uiCode, i );
    READ_UVLC( uiCode,  "vps_num_reorder_pics[i]" );          pcVPS->setNumReorderPics( uiCode, i );
    READ_UVLC( uiCode,  "vps_max_latency_increase[i]" );      pcVPS->setMaxLatencyIncrease( uiCode, i );
  }
  
  READ_FLAG( uiCode,  "vps_extension_flag" );          assert(!uiCode);
  //future extensions go here..
  
  return;
}

Void TDecCavlc::parseSliceHeader (TComSlice*& rpcSlice, ParameterSetManagerDecoder *parameterSetManager)
{
  UInt  uiCode;
  Int   iCode;

#if ENC_DEC_TRACE
  xTraceSliceHeader(rpcSlice);
#endif
  TComPPS* pps = NULL;
  TComSPS* sps = NULL;

  UInt firstSliceInPic;
  READ_FLAG( firstSliceInPic, "first_slice_in_pic_flag" );
#if SPLICING_FRIENDLY_PARAMS
    if(   rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLANT
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRANT
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )
    { 
      READ_FLAG( uiCode, "no_output_of_prior_pics_flag" );  //ignored
    }
#endif
  READ_UVLC (    uiCode, "pic_parameter_set_id" );  rpcSlice->setPPSId(uiCode);
  pps = parameterSetManager->getPrefetchedPPS(uiCode);
  //!KS: need to add error handling code here, if PPS is not available
  assert(pps!=0);
  sps = parameterSetManager->getPrefetchedSPS(pps->getSPSId());
  //!KS: need to add error handling code here, if SPS is not available
  assert(sps!=0);
  rpcSlice->setSPS(sps);
  rpcSlice->setPPS(pps);

  Int numCUs = ((sps->getPicWidthInLumaSamples()+sps->getMaxCUWidth()-1)/sps->getMaxCUWidth())*((sps->getPicHeightInLumaSamples()+sps->getMaxCUHeight()-1)/sps->getMaxCUHeight());
  Int maxParts = (1<<(sps->getMaxCUDepth()<<1));
#if REMOVE_FGS
  Int numParts = 0;
#else
  Int numParts = (1<<(pps->getSliceGranularity()<<1));
#endif
  UInt lCUAddress = 0;
  Int reqBitsOuter = 0;
  while(numCUs>(1<<reqBitsOuter))
  {
    reqBitsOuter++;
  }
  Int reqBitsInner = 0;
  while((numParts)>(1<<reqBitsInner)) 
  {
    reqBitsInner++;
  }

  UInt innerAddress = 0;
  if(!firstSliceInPic)
  {
    UInt address;
    READ_CODE( reqBitsOuter+reqBitsInner, address, "slice_address" );
    lCUAddress = address >> reqBitsInner;
    innerAddress = address - (lCUAddress<<reqBitsInner);
  }
  //set uiCode to equal slice start address (or dependent slice start address)
#if REMOVE_FGS
  uiCode=(maxParts*lCUAddress)+(innerAddress);
#else
  uiCode=(maxParts*lCUAddress)+(innerAddress*(maxParts>>(pps->getSliceGranularity()<<1)));
#endif

  rpcSlice->setDependentSliceCurStartCUAddr( uiCode );
  rpcSlice->setDependentSliceCurEndCUAddr(numCUs*maxParts);

  //   slice_type
  READ_UVLC (    uiCode, "slice_type" );            rpcSlice->setSliceType((SliceType)uiCode);
  // lightweight_slice_flag
  READ_FLAG( uiCode, "dependent_slice_flag" );
  Bool bDependentSlice = uiCode ? true : false;
#if DEPENDENT_SLICES
  if( rpcSlice->getPPS()->getDependentSlicesEnabledFlag())
  {
    if(bDependentSlice)
    {
      rpcSlice->setNextSlice        ( false );
      rpcSlice->setNextDependentSlice( true  );
#if BYTE_ALIGNMENT
      m_pcBitstream->readByteAlignment();
#else
      m_pcBitstream->readOutTrailingBits();
#endif
      return;
    }
  }
#endif

  if (bDependentSlice)
  {
    rpcSlice->setNextSlice        ( false );
    rpcSlice->setNextDependentSlice ( true  );
  }
  else
  {
    rpcSlice->setNextSlice        ( true  );
    rpcSlice->setNextDependentSlice ( false );

#if REMOVE_FGS
    uiCode=(maxParts*lCUAddress)+(innerAddress);
#else
    uiCode=(maxParts*lCUAddress)+(innerAddress*(maxParts>>(pps->getSliceGranularity()<<1)));
#endif
    rpcSlice->setSliceCurStartCUAddr(uiCode);
    rpcSlice->setSliceCurEndCUAddr(numCUs*maxParts);
  }

  if (!bDependentSlice)
  {
    if( pps->getOutputFlagPresentFlag() )
    {
      READ_FLAG( uiCode, "pic_output_flag" );
      rpcSlice->setPicOutputFlag( uiCode ? true : false );
    }
    else
    {
      rpcSlice->setPicOutputFlag( true );
    }
#if !SPLICING_FRIENDLY_PARAMS
    if(   rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLANT
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRANT
      || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_CRA )
    { 
      READ_UVLC( uiCode, "rap_pic_id" );  //ignored
      READ_FLAG( uiCode, "no_output_of_prior_pics_flag" );  //ignored
    }
#endif
    if( rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR )
    {
      rpcSlice->setPOC(0);
      TComReferencePictureSet* rps = rpcSlice->getLocalRPS();
      rps->setNumberOfNegativePictures(0);
      rps->setNumberOfPositivePictures(0);
      rps->setNumberOfLongtermPictures(0);
      rps->setNumberOfPictures(0);
      rpcSlice->setRPS(rps);
    }
    else
    {
      READ_CODE(sps->getBitsForPOC(), uiCode, "pic_order_cnt_lsb");  
      Int iPOClsb = uiCode;
      Int iPrevPOC = rpcSlice->getPrevPOC();
      Int iMaxPOClsb = 1<< sps->getBitsForPOC();
      Int iPrevPOClsb = iPrevPOC%iMaxPOClsb;
      Int iPrevPOCmsb = iPrevPOC-iPrevPOClsb;
      Int iPOCmsb;
      if( ( iPOClsb  <  iPrevPOClsb ) && ( ( iPrevPOClsb - iPOClsb )  >=  ( iMaxPOClsb / 2 ) ) )
      {
        iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
      }
      else if( (iPOClsb  >  iPrevPOClsb )  && ( (iPOClsb - iPrevPOClsb )  >  ( iMaxPOClsb / 2 ) ) ) 
      {
        iPOCmsb = iPrevPOCmsb - iMaxPOClsb;
      }
      else
      {
        iPOCmsb = iPrevPOCmsb;
      }
      if(   rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA
        || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLANT )
      {
        // For BLA/BLANT, POCmsb is set to 0.
        iPOCmsb = 0;
      }
      rpcSlice->setPOC              (iPOCmsb+iPOClsb);

      TComReferencePictureSet* rps;
      READ_FLAG( uiCode, "short_term_ref_pic_set_sps_flag" );
      if(uiCode == 0) // use short-term reference picture set explicitly signalled in slice header
      {
        rps = rpcSlice->getLocalRPS();
        parseShortTermRefPicSet(sps,rps, sps->getRPSList()->getNumberOfReferencePictureSets());
        rpcSlice->setRPS(rps);
      }
      else // use reference to short-term reference picture set in PPS
      {
        READ_UVLC( uiCode, "short_term_ref_pic_set_idx"); rpcSlice->setRPS(sps->getRPSList()->getReferencePictureSet(uiCode));
        rps = rpcSlice->getRPS();
      }
      if(sps->getLongTermRefsPresent())
      {
        Int offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();
#if LTRP_IN_SPS
        UInt numOfLtrp = 0;
        UInt numLtrpInSPS = 0;
        if (rpcSlice->getSPS()->getNumLongTermRefPicSPS() > 0)
        {
          READ_UVLC( uiCode, "num_long_term_sps");
          numLtrpInSPS = uiCode;
          numOfLtrp += numLtrpInSPS;
          rps->setNumberOfLongtermPictures(numOfLtrp);
        }
        Int bitsForLtrpInSPS = 1;
        while (rpcSlice->getSPS()->getNumLongTermRefPicSPS() > (1 << bitsForLtrpInSPS))
          bitsForLtrpInSPS++;
        READ_UVLC( uiCode, "num_long_term_pics");             rps->setNumberOfLongtermPictures(uiCode);
        numOfLtrp += uiCode;
        rps->setNumberOfLongtermPictures(numOfLtrp);
#else
        READ_UVLC( uiCode, "num_long_term_pics");             rps->setNumberOfLongtermPictures(uiCode);
#endif
        Int maxPicOrderCntLSB = 1 << rpcSlice->getSPS()->getBitsForPOC();
        Int prevLSB = 0, prevDeltaMSB = 0, deltaPocMSBCycleLT = 0;;
#if LTRP_IN_SPS
        for(Int j=offset+rps->getNumberOfLongtermPictures()-1, k = 0; k < numOfLtrp; j--, k++)
#else
        for(Int j=offset+rps->getNumberOfLongtermPictures()-1 ; j > offset-1; j--)
#endif
        {
#if LTRP_IN_SPS
          if (k < numLtrpInSPS)
          {
            READ_CODE(bitsForLtrpInSPS, uiCode, "lt_idx_sps[i]");
            Int usedByCurrFromSPS=rpcSlice->getSPS()->getUsedByCurrPicLtSPSFlag(uiCode);

            uiCode = rpcSlice->getSPS()->getLtRefPicPocLsbSps(uiCode);
            rps->setUsed(j,usedByCurrFromSPS);
          }
          else
          {
            READ_CODE(rpcSlice->getSPS()->getBitsForPOC(), uiCode, "poc_lsb_lt"); 
            READ_FLAG( uiCode, "used_by_curr_pic_lt_flag");     rps->setUsed(j,uiCode);
          }
#else
          READ_CODE(rpcSlice->getSPS()->getBitsForPOC(), uiCode, "poc_lsb_lt"); 
#endif
          Int poc_lsb_lt = uiCode;
          READ_FLAG(uiCode,"delta_poc_msb_present_flag");
          Bool mSBPresentFlag = uiCode ? true : false;
          if(mSBPresentFlag)                  
          {
            READ_UVLC( uiCode, "delta_poc_msb_cycle_lt[i]" );
            Bool deltaFlag = false;
#if LTRP_IN_SPS
            //            First LTRP                               || First LTRP from SH           || curr LSB    != prev LSB
            if( (j == offset+rps->getNumberOfLongtermPictures()-1) || (j == offset+(numOfLtrp-numLtrpInSPS)-1) || (poc_lsb_lt != prevLSB) )
#else
            //            First LTRP                               || curr LSB    != prev LSB
            if( (j == offset+rps->getNumberOfLongtermPictures()-1) || (poc_lsb_lt != prevLSB) )
#endif
            {
              deltaFlag = true;
            }
            if(deltaFlag)
            {
              deltaPocMSBCycleLT = uiCode;
            }
            else
            {
              deltaPocMSBCycleLT = uiCode + prevDeltaMSB;              
            }

            Int pocLTCurr = rpcSlice->getPOC() - deltaPocMSBCycleLT * maxPicOrderCntLSB 
                                        - iPOClsb + poc_lsb_lt;                                      
            rps->setPOC     (j, pocLTCurr); 
            rps->setDeltaPOC(j, - rpcSlice->getPOC() + pocLTCurr);
            rps->setCheckLTMSBPresent(j,true);  
          }
          else
          {
            rps->setPOC     (j, poc_lsb_lt);
            rps->setDeltaPOC(j, - rpcSlice->getPOC() + poc_lsb_lt);
            rps->setCheckLTMSBPresent(j,false);  
          }
#if !LTRP_IN_SPS
        READ_FLAG( uiCode, "used_by_curr_pic_lt_flag");     rps->setUsed(j,uiCode);
#endif
          prevLSB = poc_lsb_lt;
          prevDeltaMSB = deltaPocMSBCycleLT;
        }
        offset += rps->getNumberOfLongtermPictures();
        rps->setNumberOfPictures(offset);        
      }  
      if(   rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLA
        || rpcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_BLANT )
      {
        // In the case of BLA/BLANT, rps data is read from slice header but ignored
        rps = rpcSlice->getLocalRPS();
        rps->setNumberOfNegativePictures(0);
        rps->setNumberOfPositivePictures(0);
        rps->setNumberOfLongtermPictures(0);
        rps->setNumberOfPictures(0);
        rpcSlice->setRPS(rps);
      }
    }
#if REMOVE_ALF
    if(sps->getUseSAO())
#else
    if(sps->getUseSAO() || sps->getUseALF())
#endif
    {
      if (sps->getUseSAO())
      {
        READ_FLAG(uiCode, "slice_sample_adaptive_offset_flag");  rpcSlice->setSaoEnabledFlag((Bool)uiCode);
#if !SAO_LUM_CHROMA_ONOFF_FLAGS
        if (rpcSlice->getSaoEnabledFlag() )
#endif
        {
#if SAO_TYPE_SHARING 
          READ_FLAG(uiCode, "sao_chroma_enable_flag");  rpcSlice->setSaoEnabledFlagChroma((Bool)uiCode);
#else
          READ_FLAG(uiCode, "sao_cb_enable_flag");  rpcSlice->setSaoEnabledFlagCb((Bool)uiCode);
          READ_FLAG(uiCode, "sao_cr_enable_flag");  rpcSlice->setSaoEnabledFlagCr((Bool)uiCode);
#endif
        }
#if !SAO_LUM_CHROMA_ONOFF_FLAGS
        else
        {
#if SAO_TYPE_SHARING
          rpcSlice->setSaoEnabledFlagChroma(0);
#else
          rpcSlice->setSaoEnabledFlagCb(0);
          rpcSlice->setSaoEnabledFlagCr(0);
#endif
        }
#endif
      }
#if !REMOVE_APS
      READ_UVLC (    uiCode, "aps_id" );  rpcSlice->setAPSId(uiCode);
#endif
    }
    if (!rpcSlice->isIntra())
    {
      if (rpcSlice->getSPS()->getTMVPFlagsPresent())
      {
        READ_FLAG( uiCode, "enable_temporal_mvp_flag" );
        rpcSlice->setEnableTMVPFlag(uiCode); 
      }
      else
      {
        rpcSlice->setEnableTMVPFlag(false);
      }
      READ_FLAG( uiCode, "num_ref_idx_active_override_flag");
      if (uiCode)
      {
        READ_UVLC (uiCode, "num_ref_idx_l0_active_minus1" );  rpcSlice->setNumRefIdx( REF_PIC_LIST_0, uiCode + 1 );
        if (rpcSlice->isInterB())
        {
          READ_UVLC (uiCode, "num_ref_idx_l1_active_minus1" );  rpcSlice->setNumRefIdx( REF_PIC_LIST_1, uiCode + 1 );
        }
        else
        {
          rpcSlice->setNumRefIdx(REF_PIC_LIST_1, 0);
        }
      }
      else
      {
        rpcSlice->setNumRefIdx(REF_PIC_LIST_0, rpcSlice->getPPS()->getNumRefIdxL0DefaultActive());
        if (rpcSlice->isInterB())
        {
          rpcSlice->setNumRefIdx(REF_PIC_LIST_1, rpcSlice->getPPS()->getNumRefIdxL1DefaultActive());
        }
        else
        {
          rpcSlice->setNumRefIdx(REF_PIC_LIST_1,0);
        }
      }
    }
    // }
    TComRefPicListModification* refPicListModification = rpcSlice->getRefPicListModification();
    if(!rpcSlice->isIntra())
    {
      if( !rpcSlice->getSPS()->getListsModificationPresentFlag() )
      {
        refPicListModification->setRefPicListModificationFlagL0( 0 );
      }
      else
      {
        READ_FLAG( uiCode, "ref_pic_list_modification_flag_l0" ); refPicListModification->setRefPicListModificationFlagL0( uiCode ? 1 : 0 );
      }

      if(refPicListModification->getRefPicListModificationFlagL0())
      {
        uiCode = 0;
        Int i = 0;
        Int numRpsCurrTempList0 = rpcSlice->getNumRpsCurrTempList();
        if ( numRpsCurrTempList0 > 1 )
        {
          Int length = 1;
          numRpsCurrTempList0 --;
          while ( numRpsCurrTempList0 >>= 1) 
          {
            length ++;
          }
          for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_0); i ++)
          {
            READ_CODE( length, uiCode, "list_entry_l0" );
            refPicListModification->setRefPicSetIdxL0(i, uiCode );
          }
        }
        else
        {
          for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_0); i ++)
          {
            refPicListModification->setRefPicSetIdxL0(i, 0 );
          }
        }
      }
    }
    else
    {
      refPicListModification->setRefPicListModificationFlagL0(0);
    }
    if(rpcSlice->isInterB())
    {
      if( !rpcSlice->getSPS()->getListsModificationPresentFlag() )
      {
        refPicListModification->setRefPicListModificationFlagL1( 0 );
      }
      else
      {
        READ_FLAG( uiCode, "ref_pic_list_modification_flag_l1" ); refPicListModification->setRefPicListModificationFlagL1( uiCode ? 1 : 0 );
      }
      if(refPicListModification->getRefPicListModificationFlagL1())
      {
        uiCode = 0;
        Int i = 0;
        Int numRpsCurrTempList1 = rpcSlice->getNumRpsCurrTempList();
        if ( numRpsCurrTempList1 > 1 )
        {
          Int length = 1;
          numRpsCurrTempList1 --;
          while ( numRpsCurrTempList1 >>= 1) 
          {
            length ++;
          }
          for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_1); i ++)
          {
            READ_CODE( length, uiCode, "list_entry_l1" );
            refPicListModification->setRefPicSetIdxL1(i, uiCode );
          }
        }
        else
        {
          for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_1); i ++)
          {
            refPicListModification->setRefPicSetIdxL1(i, 0 );
          }
        }
      }
    }  
    else
    {
      refPicListModification->setRefPicListModificationFlagL1(0);
    }
  }
  else
  {
    // initialize from previous slice
    pps = rpcSlice->getPPS();
    sps = rpcSlice->getSPS();
  }

  if (rpcSlice->isInterB())
  {
    READ_FLAG( uiCode, "mvd_l1_zero_flag" );       rpcSlice->setMvdL1ZeroFlag( (uiCode ? true : false) );
  }

  rpcSlice->setCabacInitFlag( false ); // default
  if(pps->getCabacInitPresentFlag() && !rpcSlice->isIntra())
  {
    READ_FLAG(uiCode, "cabac_init_flag");
    rpcSlice->setCabacInitFlag( uiCode ? true : false );
  }

  if(!bDependentSlice)
  {
    READ_SVLC( iCode, "slice_qp_delta" ); 
    rpcSlice->setSliceQp (26 + pps->getPicInitQPMinus26() + iCode);

    assert( rpcSlice->getSliceQp() >= -sps->getQpBDOffsetY() );
    assert( rpcSlice->getSliceQp() <=  51 );

#if CHROMA_QP_EXTENSION
    if (rpcSlice->getPPS()->getSliceChromaQpFlag())
    {
      READ_SVLC( iCode, "slice_qp_delta_cb" );
      rpcSlice->setSliceQpDeltaCb( iCode );
      assert( rpcSlice->getSliceQpDeltaCb() >= -12 );
      assert( rpcSlice->getSliceQpDeltaCb() <=  12 );
      assert( (rpcSlice->getPPS()->getChromaCbQpOffset() + rpcSlice->getSliceQpDeltaCb()) >= -12 );
      assert( (rpcSlice->getPPS()->getChromaCbQpOffset() + rpcSlice->getSliceQpDeltaCb()) <=  12 );

      READ_SVLC( iCode, "slice_qp_delta_cr" );
      rpcSlice->setSliceQpDeltaCr( iCode );
      assert( rpcSlice->getSliceQpDeltaCr() >= -12 );
      assert( rpcSlice->getSliceQpDeltaCr() <=  12 );
      assert( (rpcSlice->getPPS()->getChromaCrQpOffset() + rpcSlice->getSliceQpDeltaCr()) >= -12 );
      assert( (rpcSlice->getPPS()->getChromaCrQpOffset() + rpcSlice->getSliceQpDeltaCr()) <=  12 );
    }
#endif

    if (rpcSlice->getPPS()->getDeblockingFilterControlPresent())
    {
      if(rpcSlice->getPPS()->getLoopFilterOffsetInPPS())
      {
        READ_FLAG ( uiCode, "inherit_dbl_param_from_PPS_flag" );
        rpcSlice->setInheritDblParamFromPPS(uiCode ? 1 : 0);
      }
      else
      {  
        rpcSlice->setInheritDblParamFromPPS(0);
      }
      if(!rpcSlice->getInheritDblParamFromPPS()){
        READ_FLAG ( uiCode, "disable_deblocking_filter_flag" );
        rpcSlice->setLoopFilterDisable(uiCode ? 1 : 0);
        if(!rpcSlice->getLoopFilterDisable())
        {
          READ_SVLC( iCode, "beta_offset_div2" ); rpcSlice->setLoopFilterBetaOffset(iCode);
          READ_SVLC( iCode, "tc_offset_div2" ); rpcSlice->setLoopFilterTcOffset(iCode);
        }
      }
      else
      {
        rpcSlice->setLoopFilterDisable(rpcSlice->getPPS()->getLoopFilterDisable());
        rpcSlice->setLoopFilterBetaOffset(rpcSlice->getPPS()->getLoopFilterBetaOffset());
        rpcSlice->setLoopFilterTcOffset(rpcSlice->getPPS()->getLoopFilterTcOffset());
      }
    }
    if ( rpcSlice->getEnableTMVPFlag() )
    {
      if ( rpcSlice->getSliceType() == B_SLICE )
      {
        READ_FLAG( uiCode, "collocated_from_l0_flag" );
        rpcSlice->setColDir(uiCode);
      }

      if ( rpcSlice->getSliceType() != I_SLICE &&
        ((rpcSlice->getColDir()==0 && rpcSlice->getNumRefIdx(REF_PIC_LIST_0)>1)||
        (rpcSlice->getColDir() ==1 && rpcSlice->getNumRefIdx(REF_PIC_LIST_1)>1)))
      {
        READ_UVLC( uiCode, "collocated_ref_idx" );
        rpcSlice->setColRefIdx(uiCode);
      }
    }
    if ( (pps->getUseWP() && rpcSlice->getSliceType()==P_SLICE) || (pps->getWPBiPred() && rpcSlice->getSliceType()==B_SLICE) )
    {
      xParsePredWeightTable(rpcSlice);
      rpcSlice->initWpScaling();
    }
  }

  READ_UVLC( uiCode, "5_minus_max_num_merge_cand");
  rpcSlice->setMaxNumMergeCand(MRG_MAX_NUM_CANDS - uiCode);
  assert(rpcSlice->getMaxNumMergeCand()==MRG_MAX_NUM_CANDS_SIGNALED);

  if (!bDependentSlice)
  {
#if !REMOVE_ALF
    if(sps->getUseALF())
    {
      char syntaxString[50];
      for(Int compIdx=0; compIdx< 3; compIdx++)
      {
        sprintf(syntaxString, "alf_slice_filter_flag[%d]", compIdx);
        READ_FLAG(uiCode, syntaxString);
        rpcSlice->setAlfEnabledFlag( (uiCode ==1), compIdx);
      }
    }
    Bool isAlfEnabled = (!rpcSlice->getSPS()->getUseALF())?(false):(rpcSlice->getAlfEnabledFlag(0)||rpcSlice->getAlfEnabledFlag(1)||rpcSlice->getAlfEnabledFlag(2));
#endif
#if !SAO_LUM_CHROMA_ONOFF_FLAGS
    Bool isSAOEnabled = (!rpcSlice->getSPS()->getUseSAO())?(false):(rpcSlice->getSaoEnabledFlag());
#else
    Bool isSAOEnabled = (!rpcSlice->getSPS()->getUseSAO())?(false):(rpcSlice->getSaoEnabledFlag()||rpcSlice->getSaoEnabledFlagChroma());
#endif
    Bool isDBFEnabled = (!rpcSlice->getLoopFilterDisable());

#if REMOVE_ALF
#if MOVE_LOOP_FILTER_SLICES_FLAG
    if(rpcSlice->getPPS()->getLFCrossSliceBoundaryFlag() && ( isSAOEnabled || isDBFEnabled ))
#else
    if(rpcSlice->getSPS()->getLFCrossSliceBoundaryFlag() && ( isSAOEnabled || isDBFEnabled ))
#endif
#else
    if(rpcSlice->getSPS()->getLFCrossSliceBoundaryFlag() && ( isAlfEnabled || isSAOEnabled || isDBFEnabled ))
#endif
    {
      READ_FLAG( uiCode, "slice_loop_filter_across_slices_enabled_flag");
    }
    else
    {
#if MOVE_LOOP_FILTER_SLICES_FLAG
      uiCode = rpcSlice->getPPS()->getLFCrossSliceBoundaryFlag()?1:0;
#else
      uiCode = rpcSlice->getSPS()->getLFCrossSliceBoundaryFlag()?1:0;
#endif
    }
    rpcSlice->setLFCrossSliceBoundaryFlag( (uiCode==1)?true:false);
  }

#if DEPENDENT_SLICES
  if( pps->getDependentSlicesEnabledFlag()== false )
#endif
  {
    Int tilesOrEntropyCodingSyncIdc = pps->getTilesOrEntropyCodingSyncIdc();
    UInt *entryPointOffset          = NULL;
    UInt numEntryPointOffsets, offsetLenMinus1;

    rpcSlice->setNumEntryPointOffsets ( 0 ); // default

    if (tilesOrEntropyCodingSyncIdc>0)
    {
      READ_UVLC(numEntryPointOffsets, "num_entry_point_offsets"); rpcSlice->setNumEntryPointOffsets ( numEntryPointOffsets );
      if (numEntryPointOffsets>0)
      {
        READ_UVLC(offsetLenMinus1, "offset_len_minus1");
      }
      entryPointOffset = new UInt[numEntryPointOffsets];
      for (UInt idx=0; idx<numEntryPointOffsets; idx++)
      {
        READ_CODE(offsetLenMinus1+1, uiCode, "entry_point_offset");
        entryPointOffset[ idx ] = uiCode;
      }
    }

    if ( tilesOrEntropyCodingSyncIdc == 1 ) // tiles
    {
      rpcSlice->setTileLocationCount( numEntryPointOffsets );

      UInt prevPos = 0;
      for (Int idx=0; idx<rpcSlice->getTileLocationCount(); idx++)
      {
        rpcSlice->setTileLocation( idx, prevPos + entryPointOffset [ idx ] );
        prevPos += entryPointOffset[ idx ];
      }
    }
    else if ( tilesOrEntropyCodingSyncIdc == 2 ) // wavefront
    {
      Int numSubstreams = pps->getNumSubstreams();
      rpcSlice->allocSubstreamSizes(numSubstreams);
      UInt *pSubstreamSizes       = rpcSlice->getSubstreamSizes();
      for (Int idx=0; idx<numSubstreams-1; idx++)
      {
        if ( idx < numEntryPointOffsets )
        {
          pSubstreamSizes[ idx ] = ( entryPointOffset[ idx ] << 3 ) ;
        }
        else
        {
          pSubstreamSizes[ idx ] = 0;
        }
      }
    }

    if (entryPointOffset)
    {
      delete [] entryPointOffset;
    }
  }

#if SLICE_HEADER_EXTENSION
  if(pps->getSliceHeaderExtensionPresentFlag())
  {
    READ_UVLC(uiCode,"slice_header_extension_length");
    for(Int i=0; i<uiCode; i++)
    {
      UInt ignore;
      READ_CODE(8,ignore,"slice_header_extension_data_byte");
    }
  }
#endif
#if BYTE_ALIGNMENT
  m_pcBitstream->readByteAlignment();
#else
  if (!bDependentSlice)
  {
    // Reading location information
    // read out trailing bits
    m_pcBitstream->readOutTrailingBits();
  }
#endif
  return;
}
  
Void TDecCavlc::parseTerminatingBit( UInt& ruiBit )
{
  ruiBit = false;
  Int iBitsLeft = m_pcBitstream->getNumBitsLeft();
  if(iBitsLeft <= 8)
  {
    UInt uiPeekValue = m_pcBitstream->peekBits(iBitsLeft);
    if (uiPeekValue == (1<<(iBitsLeft-1)))
    {
      ruiBit = true;
    }
  }
}

Void TDecCavlc::parseSkipFlag( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parseCUTransquantBypassFlag( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parseMVPIdx( Int& riMVPIdx )
{
  assert(0);
}

Void TDecCavlc::parseSplitFlag     ( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parsePartSize( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parsePredMode( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

/** Parse I_PCM information. 
* \param pcCU pointer to CU
* \param uiAbsPartIdx CU index
* \param uiDepth CU depth
* \returns Void
*
* If I_PCM flag indicates that the CU is I_PCM, parse its PCM alignment bits and codes.  
*/
Void TDecCavlc::parseIPCMInfo( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parseIntraDirLumaAng  ( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{ 
  assert(0);
}

Void TDecCavlc::parseIntraDirChroma( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parseInterDir( TComDataCU* pcCU, UInt& ruiInterDir, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parseRefFrmIdx( TComDataCU* pcCU, Int& riRefFrmIdx, UInt uiAbsPartIdx, UInt uiDepth, RefPicList eRefList )
{
  assert(0);
}

Void TDecCavlc::parseMvd( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiPartIdx, UInt uiDepth, RefPicList eRefList )
{
  assert(0);
}

Void TDecCavlc::parseDeltaQP( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth )
{
  Int qp;
  Int  iDQp;

  xReadSvlc( iDQp );

  Int qpBdOffsetY = pcCU->getSlice()->getSPS()->getQpBDOffsetY();
  qp = (((Int) pcCU->getRefQP( uiAbsPartIdx ) + iDQp + 52 + 2*qpBdOffsetY )%(52+ qpBdOffsetY)) -  qpBdOffsetY;

  UInt uiAbsQpCUPartIdx = (uiAbsPartIdx>>((g_uiMaxCUDepth - pcCU->getSlice()->getPPS()->getMaxCuDQPDepth())<<1))<<((g_uiMaxCUDepth - pcCU->getSlice()->getPPS()->getMaxCuDQPDepth())<<1) ;
  UInt uiQpCUDepth =   min(uiDepth,pcCU->getSlice()->getPPS()->getMaxCuDQPDepth()) ;

  pcCU->setQPSubParts( qp, uiAbsQpCUPartIdx, uiQpCUDepth );
}

Void TDecCavlc::parseCoeffNxN( TComDataCU* pcCU, TCoeff* pcCoef, UInt uiAbsPartIdx, UInt uiWidth, UInt uiHeight, UInt uiDepth, TextType eTType )
{
  assert(0);
}

Void TDecCavlc::parseTransformSubdivFlag( UInt& ruiSubdivFlag, UInt uiLog2TransformBlockSize )
{
  assert(0);
}

Void TDecCavlc::parseQtCbf( TComDataCU* pcCU, UInt uiAbsPartIdx, TextType eType, UInt uiTrDepth, UInt uiDepth )
{
  assert(0);
}

Void TDecCavlc::parseQtRootCbf( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth, UInt& uiQtRootCbf )
{
  assert(0);
}

Void TDecCavlc::parseTransformSkipFlags (TComDataCU* pcCU, UInt uiAbsPartIdx, UInt width, UInt height, UInt uiDepth, TextType eTType)
{
  assert(0);
}

Void TDecCavlc::parseMergeFlag ( TComDataCU* pcCU, UInt uiAbsPartIdx, UInt uiDepth, UInt uiPUIdx )
{
  assert(0);
}

Void TDecCavlc::parseMergeIndex ( TComDataCU* pcCU, UInt& ruiMergeIndex, UInt uiAbsPartIdx, UInt uiDepth )
{
  assert(0);
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

Void TDecCavlc::xReadCode (UInt uiLength, UInt& ruiCode)
{
  assert ( uiLength > 0 );
  m_pcBitstream->read (uiLength, ruiCode);
}

Void TDecCavlc::xReadUvlc( UInt& ruiVal)
{
  UInt uiVal = 0;
  UInt uiCode = 0;
  UInt uiLength;
  m_pcBitstream->read( 1, uiCode );

  if( 0 == uiCode )
  {
    uiLength = 0;

    while( ! ( uiCode & 1 ))
    {
      m_pcBitstream->read( 1, uiCode );
      uiLength++;
    }

    m_pcBitstream->read( uiLength, uiVal );

    uiVal += (1 << uiLength)-1;
  }

  ruiVal = uiVal;
}

Void TDecCavlc::xReadSvlc( Int& riVal)
{
  UInt uiBits = 0;
  m_pcBitstream->read( 1, uiBits );
  if( 0 == uiBits )
  {
    UInt uiLength = 0;

    while( ! ( uiBits & 1 ))
    {
      m_pcBitstream->read( 1, uiBits );
      uiLength++;
    }

    m_pcBitstream->read( uiLength, uiBits );

    uiBits += (1 << uiLength);
    riVal = ( uiBits & 1) ? -(Int)(uiBits>>1) : (Int)(uiBits>>1);
  }
  else
  {
    riVal = 0;
  }
}

Void TDecCavlc::xReadFlag (UInt& ruiCode)
{
  m_pcBitstream->read( 1, ruiCode );
}

/** Parse PCM alignment zero bits.
* \returns Void
*/
Void TDecCavlc::xReadPCMAlignZero( )
{
  UInt uiNumberOfBits = m_pcBitstream->getNumBitsUntilByteAligned();

  if(uiNumberOfBits)
  {
    UInt uiBits;
    UInt uiSymbol;

    for(uiBits = 0; uiBits < uiNumberOfBits; uiBits++)
    {
      xReadFlag( uiSymbol );

      if(uiSymbol)
      {
        printf("\nWarning! pcm_align_zero include a non-zero value.\n");
      }
    }
  }
}

Void TDecCavlc::xReadUnaryMaxSymbol( UInt& ruiSymbol, UInt uiMaxSymbol )
{
  if (uiMaxSymbol == 0)
  {
    ruiSymbol = 0;
    return;
  }

  xReadFlag( ruiSymbol );

  if (ruiSymbol == 0 || uiMaxSymbol == 1)
  {
    return;
  }

  UInt uiSymbol = 0;
  UInt uiCont;

  do
  {
    xReadFlag( uiCont );
    uiSymbol++;
  }
  while( uiCont && (uiSymbol < uiMaxSymbol-1) );

  if( uiCont && (uiSymbol == uiMaxSymbol-1) )
  {
    uiSymbol++;
  }

  ruiSymbol = uiSymbol;
}

Void TDecCavlc::xReadExGolombLevel( UInt& ruiSymbol )
{
  UInt uiSymbol ;
  UInt uiCount = 0;
  do
  {
    xReadFlag( uiSymbol );
    uiCount++;
  }
  while( uiSymbol && (uiCount != 13));

  ruiSymbol = uiCount-1;

  if( uiSymbol )
  {
    xReadEpExGolomb( uiSymbol, 0 );
    ruiSymbol += uiSymbol+1;
  }

  return;
}

Void TDecCavlc::xReadEpExGolomb( UInt& ruiSymbol, UInt uiCount )
{
  UInt uiSymbol = 0;
  UInt uiBit = 1;


  while( uiBit )
  {
    xReadFlag( uiBit );
    uiSymbol += uiBit << uiCount++;
  }

  uiCount--;
  while( uiCount-- )
  {
    xReadFlag( uiBit );
    uiSymbol += uiBit << uiCount;
  }

  ruiSymbol = uiSymbol;

  return;
}

UInt TDecCavlc::xGetBit()
{
  UInt ruiCode;
  m_pcBitstream->read( 1, ruiCode );
  return ruiCode;
}


/** parse explicit wp tables
* \param TComSlice* pcSlice
* \returns Void
*/
Void TDecCavlc::xParsePredWeightTable( TComSlice* pcSlice )
{
  wpScalingParam  *wp;
  Bool            bChroma     = true; // color always present in HEVC ?
  TComPPS*        pps         = pcSlice->getPPS();
  SliceType       eSliceType  = pcSlice->getSliceType();
  Int             iNbRef       = (eSliceType == B_SLICE ) ? (2) : (1);
  UInt            uiLog2WeightDenomLuma, uiLog2WeightDenomChroma;
  UInt            uiMode      = 0;
#if NUM_WP_LIMIT
  UInt            uiTotalSignalledWeightFlags = 0;
#endif
  if ( (eSliceType==P_SLICE && pps->getUseWP()) || (eSliceType==B_SLICE && pps->getWPBiPred()) )
  {
    uiMode = 1; // explicit
  }
  if ( uiMode == 1 )  // explicit
  {
    printf("\nTDecCavlc::xParsePredWeightTable(poc=%d) explicit...\n", pcSlice->getPOC());
    Int iDeltaDenom;
    // decode delta_luma_log2_weight_denom :
    READ_UVLC( uiLog2WeightDenomLuma, "luma_log2_weight_denom" );     // ue(v): luma_log2_weight_denom
    if( bChroma ) 
    {
      READ_SVLC( iDeltaDenom, "delta_chroma_log2_weight_denom" );     // se(v): delta_chroma_log2_weight_denom
      assert((iDeltaDenom + (Int)uiLog2WeightDenomLuma)>=0);
      uiLog2WeightDenomChroma = (UInt)(iDeltaDenom + uiLog2WeightDenomLuma);
    }

    for ( Int iNumRef=0 ; iNumRef<iNbRef ; iNumRef++ ) 
    {
      RefPicList  eRefPicList = ( iNumRef ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
      for ( Int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ ) 
      {
        pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);

        wp[0].uiLog2WeightDenom = uiLog2WeightDenomLuma;
        wp[1].uiLog2WeightDenom = uiLog2WeightDenomChroma;
        wp[2].uiLog2WeightDenom = uiLog2WeightDenomChroma;

        UInt  uiCode;
        READ_FLAG( uiCode, "luma_weight_lX_flag" );           // u(1): luma_weight_l0_flag
        wp[0].bPresentFlag = ( uiCode == 1 );
#if NUM_WP_LIMIT
        uiTotalSignalledWeightFlags += wp[0].bPresentFlag;
      }
      if ( bChroma ) 
      {
        UInt  uiCode;
        for ( Int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ ) 
        {
          pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
          READ_FLAG( uiCode, "chroma_weight_lX_flag" );      // u(1): chroma_weight_l0_flag
          wp[1].bPresentFlag = ( uiCode == 1 );
          wp[2].bPresentFlag = ( uiCode == 1 );
          uiTotalSignalledWeightFlags += 2*wp[1].bPresentFlag;
        }
      }
      for ( Int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ ) 
      {
        pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
#endif
        if ( wp[0].bPresentFlag ) 
        {
          Int iDeltaWeight;
          READ_SVLC( iDeltaWeight, "delta_luma_weight_lX" );  // se(v): delta_luma_weight_l0[i]
          wp[0].iWeight = (iDeltaWeight + (1<<wp[0].uiLog2WeightDenom));
          READ_SVLC( wp[0].iOffset, "luma_offset_lX" );       // se(v): luma_offset_l0[i]
        }
        else 
        {
          wp[0].iWeight = (1 << wp[0].uiLog2WeightDenom);
          wp[0].iOffset = 0;
        }
        if ( bChroma ) 
        {
#if !NUM_WP_LIMIT
          READ_FLAG( uiCode, "chroma_weight_lX_flag" );      // u(1): chroma_weight_l0_flag
          wp[1].bPresentFlag = ( uiCode == 1 );
          wp[2].bPresentFlag = ( uiCode == 1 );
#endif
          if ( wp[1].bPresentFlag ) 
          {
            for ( Int j=1 ; j<3 ; j++ ) 
            {
              Int iDeltaWeight;
              READ_SVLC( iDeltaWeight, "delta_chroma_weight_lX" );  // se(v): chroma_weight_l0[i][j]
              wp[j].iWeight = (iDeltaWeight + (1<<wp[1].uiLog2WeightDenom));

              Int iDeltaChroma;
              READ_SVLC( iDeltaChroma, "delta_chroma_offset_lX" );  // se(v): delta_chroma_offset_l0[i][j]
              Int shift = ((1<<(g_uiBitDepth+g_uiBitIncrement-1)));
              Int pred = ( shift - ( ( shift*wp[j].iWeight)>>(wp[j].uiLog2WeightDenom) ) );
#if WP_PARAM_RANGE_LIMIT
              wp[j].iOffset = Clip3(-128, 127, (iDeltaChroma + pred) );
#else
              wp[j].iOffset = iDeltaChroma + pred;
#endif
            }
          }
          else 
          {
            for ( Int j=1 ; j<3 ; j++ ) 
            {
              wp[j].iWeight = (1 << wp[j].uiLog2WeightDenom);
              wp[j].iOffset = 0;
            }
          }
        }
      }

      for ( Int iRefIdx=pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx<MAX_NUM_REF ; iRefIdx++ ) 
      {
        pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);

        wp[0].bPresentFlag = false;
        wp[1].bPresentFlag = false;
        wp[2].bPresentFlag = false;
      }
    }
#if NUM_WP_LIMIT
    assert(uiTotalSignalledWeightFlags<=24);
#endif
  }
  else
  {
    printf("\n wrong weight pred table syntax \n ");
    assert(0);
  }
}

/** decode quantization matrix
* \param scalingList quantization matrix information
*/
Void TDecCavlc::parseScalingList(TComScalingList* scalingList)
{
  UInt  code, sizeId, listId;
  Bool scalingListPredModeFlag;
  //for each size
  for(sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
  {
    for(listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
    {
      READ_FLAG( code, "scaling_list_pred_mode_flag");
      scalingListPredModeFlag = (code) ? true : false;
      if(!scalingListPredModeFlag) //Copy Mode
      {
        READ_UVLC( code, "scaling_list_pred_matrix_id_delta");
        scalingList->setRefMatrixId (sizeId,listId,(UInt)((Int)(listId)-(code)));
        if( sizeId > SCALING_LIST_8x8 )
        {
          scalingList->setScalingListDC(sizeId,listId,((listId == scalingList->getRefMatrixId (sizeId,listId))? 16 :scalingList->getScalingListDC(sizeId, scalingList->getRefMatrixId (sizeId,listId))));
        }
        scalingList->processRefMatrix( sizeId, listId, scalingList->getRefMatrixId (sizeId,listId));

      }
      else //DPCM Mode
      {
        xDecodeScalingList(scalingList, sizeId, listId);
      }
    }
  }

  return;
}
/** decode DPCM
* \param scalingList  quantization matrix information
* \param sizeId size index
* \param listId list index
*/
Void TDecCavlc::xDecodeScalingList(TComScalingList *scalingList, UInt sizeId, UInt listId)
{
  Int i,coefNum = min(MAX_MATRIX_COEF_NUM,(Int)g_scalingListSize[sizeId]);
  Int data;
  Int scalingListDcCoefMinus8 = 0;
  Int nextCoef = SCALING_LIST_START_VALUE;
#if REMOVE_ZIGZAG_SCAN
  UInt* scan  = (sizeId == 0) ? g_auiSigLastScan [ SCAN_DIAG ] [ 1 ] :  g_sigLastScanCG32x32;
#else
  UInt* scan  = g_auiFrameScanXY [ (sizeId == 0)? 1 : 2 ];
#endif
  Int *dst = scalingList->getScalingListAddress(sizeId, listId);

  if( sizeId > SCALING_LIST_8x8 )
  {
    READ_SVLC( scalingListDcCoefMinus8, "scaling_list_dc_coef_minus8");
    scalingList->setScalingListDC(sizeId,listId,scalingListDcCoefMinus8 + 8);
    nextCoef = scalingList->getScalingListDC(sizeId,listId);
  }

  for(i = 0; i < coefNum; i++)
  {
    READ_SVLC( data, "scaling_list_delta_coef");
    nextCoef = (nextCoef + data + 256 ) % 256;
    dst[scan[i]] = nextCoef;
  }
}

Bool TDecCavlc::xMoreRbspData()
{ 
  Int bitsLeft = m_pcBitstream->getNumBitsLeft();

  // if there are more than 8 bits, it cannot be rbsp_trailing_bits
  if (bitsLeft > 8)
  {
    return true;
  }

  UChar lastByte = m_pcBitstream->peekBits(bitsLeft);
  Int cnt = bitsLeft;

  // remove trailing bits equal to zero
  while ((cnt>0) && ((lastByte & 1) == 0))
  {
    lastByte >>= 1;
    cnt--;
  }
  // remove bit equal to one
  cnt--;

  // we should not have a negative number of bits
  assert (cnt>=0);

  // we have more data, if cnt is not zero
  return (cnt>0);
}

//! \}

