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

/** \file     TEncCfg.h
    \brief    encoder configuration class (header)
*/

#ifndef __TENCCFG__
#define __TENCCFG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComSlice.h"
#include <assert.h>

struct GOPEntry
{
  Int m_POC;
  Int m_QPOffset;
  Double m_QPFactor;
  Int m_temporalId;
  Bool m_refPic;
  Int m_numRefPicsActive;
  Char m_sliceType;
  Int m_numRefPics;
  Int m_referencePics[MAX_NUM_REF_PICS];
  Int m_usedByCurrPic[MAX_NUM_REF_PICS];
#if AUTO_INTER_RPS
  Int m_interRPSPrediction;
#else
  Bool m_interRPSPrediction;
#endif
#if !J0234_INTER_RPS_SIMPL
  Int m_deltaRIdxMinus1;
#endif
  Int m_deltaRPS;
  Int m_numRefIdc;
  Int m_refIdc[MAX_NUM_REF_PICS+1];
  GOPEntry()
  : m_POC(-1)
  , m_QPOffset(0)
  , m_QPFactor(0)
  , m_temporalId(0)
  , m_refPic(false)
  , m_numRefPicsActive(0)
  , m_sliceType('P')
  , m_numRefPics(0)
  , m_interRPSPrediction(false)
#if !J0234_INTER_RPS_SIMPL
  , m_deltaRIdxMinus1(0)
#endif
  , m_deltaRPS(0)
  , m_numRefIdc(0)
  {
    ::memset( m_referencePics, 0, sizeof(m_referencePics) );
    ::memset( m_usedByCurrPic, 0, sizeof(m_usedByCurrPic) );
    ::memset( m_refIdc,        0, sizeof(m_refIdc) );
  }
};

std::istringstream &operator>>(std::istringstream &in, GOPEntry &entry);     //input
//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// encoder configuration class
class TEncCfg
{
protected:
  //==== File I/O ========
  Int       m_iFrameRate;
  Int       m_FrameSkip;
  Int       m_iSourceWidth;
  Int       m_iSourceHeight;
  Int       m_croppingMode;
  Int       m_cropLeft;
  Int       m_cropRight;
  Int       m_cropTop;
  Int       m_cropBottom;
  Int       m_iFrameToBeEncoded;
  Double    m_adLambdaModifier[ MAX_TLAYER ];

  //====== Coding Structure ========
  UInt      m_uiIntraPeriod;
  UInt      m_uiDecodingRefreshType;            ///< the type of decoding refresh employed for the random access.
  Int       m_iGOPSize;
  GOPEntry  m_GOPList[MAX_GOP];
  Int       m_extraRPSs;
  Int       m_maxDecPicBuffering[MAX_TLAYER];
  Int       m_numReorderPics[MAX_TLAYER];
  
  Int       m_iQP;                              //  if (AdaptiveQP == OFF)
  
  Int       m_aiPad[2];
  

  Int       m_iMaxRefPicNum;                     ///< this is used to mimic the sliding mechanism used by the decoder
                                                 // TODO: We need to have a common sliding mechanism used by both the encoder and decoder

  Int       m_maxTempLayer;                      ///< Max temporal layer
  Bool m_useAMP;
  //======= Transform =============
  UInt      m_uiQuadtreeTULog2MaxSize;
  UInt      m_uiQuadtreeTULog2MinSize;
  UInt      m_uiQuadtreeTUMaxDepthInter;
  UInt      m_uiQuadtreeTUMaxDepthIntra;
  
#if !REMOVE_NSQT
  Bool      m_useNSQT;
#endif
  
  //====== Loop/Deblock Filter ========
  Bool      m_bLoopFilterDisable;
  Bool      m_loopFilterOffsetInPPS;
  Int       m_loopFilterBetaOffsetDiv2;
  Int       m_loopFilterTcOffsetDiv2;
  Bool      m_DeblockingFilterControlPresent;
  Bool      m_bUseSAO;
  Int       m_maxNumOffsetsPerPic;
#if SAO_LCU_BOUNDARY
  Bool      m_saoLcuBoundary;
#endif
  Bool      m_saoLcuBasedOptimization;

  //====== Lossless ========
  Bool      m_useLossless;
  //====== Motion search ========
  Int       m_iFastSearch;                      //  0:Full search  1:Diamond  2:PMVFAST
  Int       m_iSearchRange;                     //  0:Full frame
  Int       m_bipredSearchRange;

  //====== Quality control ========
  Int       m_iMaxDeltaQP;                      //  Max. absolute delta QP (1:default)
  Int       m_iMaxCuDQPDepth;                   //  Max. depth for a minimum CuDQP (0:default)

  Int       m_chromaCbQpOffset;                 //  Chroma Cb QP Offset (0:default)
  Int       m_chromaCrQpOffset;                 //  Chroma Cr Qp Offset (0:default)

#if ADAPTIVE_QP_SELECTION
  Bool      m_bUseAdaptQpSelect;
#endif

  Bool      m_bUseAdaptiveQP;
  Int       m_iQPAdaptationRange;
  
  //====== Tool list ========
  Bool      m_bUseSBACRD;
#if !REMOVE_ALF
  Bool      m_bUseALF;
  Bool      m_alfLowLatencyEncoding;
#endif
  Bool      m_bUseASR;
  Bool      m_bUseHADME;
  Bool      m_bUseLComb;
  Bool      m_bUseRDOQ;
  Bool      m_bUseFastEnc;
  Bool      m_bUseEarlyCU;
  Bool      m_useFastDecisionForMerge;
  Bool      m_bUseCbfFastMode;
  Bool      m_useEarlySkipDetection;
#if !REMOVE_LMCHROMA
  Bool      m_bUseLMChroma;
#endif
  Bool      m_useTansformSkip;
  Bool      m_useTansformSkipFast;
  Int*      m_aidQP;
  UInt      m_uiDeltaQpRD;
  
  Bool      m_bUseConstrainedIntraPred;
  Bool      m_usePCM;
  UInt      m_pcmLog2MaxSize;
  UInt      m_uiPCMLog2MinSize;
  //====== Slice ========
  Int       m_iSliceMode;
  Int       m_iSliceArgument; 
  //====== Dependent Slice ========
  Int       m_iDependentSliceMode;
  Int       m_iDependentSliceArgument;
#if DEPENDENT_SLICES
  Bool      m_bCabacIndependentFlag;
#endif
#if !REMOVE_FGS
  Int       m_iSliceGranularity;
#endif
  Bool      m_bLFCrossSliceBoundaryFlag;

  Bool      m_bPCMInputBitDepthFlag;
  UInt      m_uiPCMBitDepthLuma;
  UInt      m_uiPCMBitDepthChroma;
  Bool      m_bPCMFilterDisableFlag;
  Bool      m_bLFCrossTileBoundaryFlag;
  Int       m_iUniformSpacingIdr;
  Int       m_iNumColumnsMinus1;
  UInt*     m_puiColumnWidth;
  Int       m_iNumRowsMinus1;
  UInt*     m_puiRowHeight;

  Int       m_iWaveFrontSynchro;
  Int       m_iWaveFrontSubstreams;

  Int m_pictureDigestEnabled;              ///< Checksum(3)/CRC(2)/MD5(1)/disable(0) acting on SEI picture_digest message
  //====== Weighted Prediction ========
  Bool      m_bUseWeightPred;       //< Use of Weighting Prediction (P_SLICE)
  Bool      m_useWeightedBiPred;    //< Use of Bi-directional Weighting Prediction (B_SLICE)
  UInt      m_log2ParallelMergeLevelMinus2;       // Parallel merge estimation region
  Int       m_useScalingListId;            ///< Using quantization matrix i.e. 0=off, 1=default, 2=file.
  char*     m_scalingListFile;          ///< quantization matrix file name
  Int       m_TMVPModeId;
  Int       m_signHideFlag;
  Bool      m_enableRateCtrl;                                ///< Flag for using rate control algorithm
  Int       m_targetBitrate;                                 ///< target bitrate
  Int       m_numLCUInUnit;                                  ///< Total number of LCUs in a frame should be divided by the NumLCUInUnit
  Bool      m_TransquantBypassEnableFlag;                     ///< transquant_bypass_enable_flag setting in PPS.
  Bool      m_CUTransquantBypassFlagValue;                    ///< if transquant_bypass_enable_flag, the fixed value to use for the per-CU cu_transquant_bypass_flag.
  TComVPS                    m_cVPS;
#if RECALCULATE_QP_ACCORDING_LAMBDA
  Bool      m_recalculateQPAccordingToLambda;                 ///< recalculate QP value according to the lambda value
#endif
  
public:
  TEncCfg()
  : m_puiColumnWidth()
  , m_puiRowHeight()
  {}

  virtual ~TEncCfg()
  {
    delete[] m_puiColumnWidth;
    delete[] m_puiRowHeight;
  }
  
  Void      setFrameRate                    ( Int   i )      { m_iFrameRate = i; }
  Void      setFrameSkip                    ( unsigned int i ) { m_FrameSkip = i; }
  Void      setSourceWidth                  ( Int   i )      { m_iSourceWidth = i; }
  Void      setSourceHeight                 ( Int   i )      { m_iSourceHeight = i; }
  Void      setCroppingMode                 ( Int   i )      { m_croppingMode = i; }
  Void      setCropLeft                     ( Int   i )      { m_cropLeft = i; }
  Void      setCropRight                    ( Int   i )      { m_cropRight = i; }
  Void      setCropTop                      ( Int   i )      { m_cropTop = i; }
  Void      setCropBottom                   ( Int   i )      { m_cropBottom = i; }
  Void      setFrameToBeEncoded             ( Int   i )      { m_iFrameToBeEncoded = i; }
  
  //====== Coding Structure ========
  Void      setIntraPeriod                  ( Int   i )      { m_uiIntraPeriod = (UInt)i; }
  Void      setDecodingRefreshType          ( Int   i )      { m_uiDecodingRefreshType = (UInt)i; }
  Void      setGOPSize                      ( Int   i )      { m_iGOPSize = i; }
  Void      setGopList                      ( GOPEntry*  GOPList ) {  for ( Int i = 0; i < MAX_GOP; i++ ) m_GOPList[i] = GOPList[i]; }
  Void      setExtraRPSs                    ( Int   i )      { m_extraRPSs = i; }
  GOPEntry  getGOPEntry                     ( Int   i )      { return m_GOPList[i]; }
  Void      setMaxDecPicBuffering           ( UInt u, UInt tlayer ) { m_maxDecPicBuffering[tlayer] = u;    }
  Void      setNumReorderPics               ( Int  i, UInt tlayer ) { m_numReorderPics[tlayer] = i;    }
  
  Void      setQP                           ( Int   i )      { m_iQP = i; }
  
  Void      setPad                          ( Int*  iPad                   )      { for ( Int i = 0; i < 2; i++ ) m_aiPad[i] = iPad[i]; }
  
  Int       getMaxRefPicNum                 ()                              { return m_iMaxRefPicNum;           }
  Void      setMaxRefPicNum                 ( Int iMaxRefPicNum )           { m_iMaxRefPicNum = iMaxRefPicNum;  }

  Bool      getMaxTempLayer                 ()                              { return m_maxTempLayer;              } 
  Void      setMaxTempLayer                 ( Int maxTempLayer )            { m_maxTempLayer = maxTempLayer;      }
  //======== Transform =============
  Void      setQuadtreeTULog2MaxSize        ( UInt  u )      { m_uiQuadtreeTULog2MaxSize = u; }
  Void      setQuadtreeTULog2MinSize        ( UInt  u )      { m_uiQuadtreeTULog2MinSize = u; }
  Void      setQuadtreeTUMaxDepthInter      ( UInt  u )      { m_uiQuadtreeTUMaxDepthInter = u; }
  Void      setQuadtreeTUMaxDepthIntra      ( UInt  u )      { m_uiQuadtreeTUMaxDepthIntra = u; }
  
#if !REMOVE_NSQT
  Void setUseNSQT( Bool b ) { m_useNSQT = b; }
#endif
  Void setUseAMP( Bool b ) { m_useAMP = b; }
  
  //====== Loop/Deblock Filter ========
  Void      setLoopFilterDisable            ( Bool  b )      { m_bLoopFilterDisable       = b; }
  Void      setLoopFilterOffsetInPPS        ( Bool  b )      { m_loopFilterOffsetInPPS      = b; }
  Void      setLoopFilterBetaOffset         ( Int   i )      { m_loopFilterBetaOffsetDiv2  = i; }
  Void      setLoopFilterTcOffset           ( Int   i )      { m_loopFilterTcOffsetDiv2    = i; }
  Void      setDeblockingFilterControlPresent ( Bool b ) { m_DeblockingFilterControlPresent = b; }

  //====== Motion search ========
  Void      setFastSearch                   ( Int   i )      { m_iFastSearch = i; }
  Void      setSearchRange                  ( Int   i )      { m_iSearchRange = i; }
  Void      setBipredSearchRange            ( Int   i )      { m_bipredSearchRange = i; }

  //====== Quality control ========
  Void      setMaxDeltaQP                   ( Int   i )      { m_iMaxDeltaQP = i; }
  Void      setMaxCuDQPDepth                ( Int   i )      { m_iMaxCuDQPDepth = i; }

  Void      setChromaCbQpOffset             ( Int   i )      { m_chromaCbQpOffset = i; }
  Void      setChromaCrQpOffset             ( Int   i )      { m_chromaCrQpOffset = i; }

#if ADAPTIVE_QP_SELECTION
  Void      setUseAdaptQpSelect             ( Bool   i ) { m_bUseAdaptQpSelect    = i; }
  Bool      getUseAdaptQpSelect             ()           { return   m_bUseAdaptQpSelect; }
#endif

  Void      setUseAdaptiveQP                ( Bool  b )      { m_bUseAdaptiveQP = b; }
  Void      setQPAdaptationRange            ( Int   i )      { m_iQPAdaptationRange = i; }
  
  //====== Lossless ========
  Void      setUseLossless                  (Bool    b  )        { m_useLossless = b;  }
  //====== Sequence ========
  Int       getFrameRate                    ()      { return  m_iFrameRate; }
  unsigned int getFrameSkip                 ()      { return  m_FrameSkip; }
  Int       getSourceWidth                  ()      { return  m_iSourceWidth; }
  Int       getSourceHeight                 ()      { return  m_iSourceHeight; }
  Int       getCroppingMode                 ()      { return  m_croppingMode; }
  Int       getCropLeft                     ()      { return  m_cropLeft; }
  Int       getCropRight                    ()      { return  m_cropRight; }
  Int       getCropTop                      ()      { return  m_cropTop; }
  Int       getCropBottom                   ()      { return  m_cropBottom; }
  Int       getFrameToBeEncoded             ()      { return  m_iFrameToBeEncoded; }
  void setLambdaModifier                    ( UInt uiIndex, Double dValue ) { m_adLambdaModifier[ uiIndex ] = dValue; }
  Double getLambdaModifier                  ( UInt uiIndex ) const { return m_adLambdaModifier[ uiIndex ]; }

  //==== Coding Structure ========
  UInt      getIntraPeriod                  ()      { return  m_uiIntraPeriod; }
  UInt      getDecodingRefreshType          ()      { return  m_uiDecodingRefreshType; }
  Int       getGOPSize                      ()      { return  m_iGOPSize; }
  Int       getMaxDecPicBuffering           (UInt tlayer) { return m_maxDecPicBuffering[tlayer]; }
  Int       getNumReorderPics               (UInt tlayer) { return m_numReorderPics[tlayer]; }
  Int       getQP                           ()      { return  m_iQP; }
  
  Int       getPad                          ( Int i )      { assert (i < 2 );                      return  m_aiPad[i]; }
  
  //======== Transform =============
  UInt      getQuadtreeTULog2MaxSize        ()      const { return m_uiQuadtreeTULog2MaxSize; }
  UInt      getQuadtreeTULog2MinSize        ()      const { return m_uiQuadtreeTULog2MinSize; }
  UInt      getQuadtreeTUMaxDepthInter      ()      const { return m_uiQuadtreeTUMaxDepthInter; }
  UInt      getQuadtreeTUMaxDepthIntra      ()      const { return m_uiQuadtreeTUMaxDepthIntra; }
  
  //==== Loop/Deblock Filter ========
  Bool      getLoopFilterDisable            ()      { return  m_bLoopFilterDisable;       }
  Bool      getLoopFilterOffsetInPPS        ()      { return m_loopFilterOffsetInPPS; }
  Int       getLoopFilterBetaOffset         ()      { return m_loopFilterBetaOffsetDiv2; }
  Int       getLoopFilterTcOffset           ()      { return m_loopFilterTcOffsetDiv2; }
  Bool      getDeblockingFilterControlPresent()  { return  m_DeblockingFilterControlPresent; }

  //==== Motion search ========
  Int       getFastSearch                   ()      { return  m_iFastSearch; }
  Int       getSearchRange                  ()      { return  m_iSearchRange; }

  //==== Quality control ========
  Int       getMaxDeltaQP                   ()      { return  m_iMaxDeltaQP; }
  Int       getMaxCuDQPDepth                ()      { return  m_iMaxCuDQPDepth; }
  Bool      getUseAdaptiveQP                ()      { return  m_bUseAdaptiveQP; }
  Int       getQPAdaptationRange            ()      { return  m_iQPAdaptationRange; }
  //====== Lossless ========
  Bool      getUseLossless                  ()      { return  m_useLossless;  }
  
  //==== Tool list ========
  Void      setUseSBACRD                    ( Bool  b )     { m_bUseSBACRD  = b; }
  Void      setUseASR                       ( Bool  b )     { m_bUseASR     = b; }
  Void      setUseHADME                     ( Bool  b )     { m_bUseHADME   = b; }
#if !REMOVE_ALF
  Void      setUseALF                       ( Bool  b )     { m_bUseALF   = b; }
#endif
  Void      setUseLComb                     ( Bool  b )     { m_bUseLComb   = b; }
  Void      setUseRDOQ                      ( Bool  b )     { m_bUseRDOQ    = b; }
  Void      setUseFastEnc                   ( Bool  b )     { m_bUseFastEnc = b; }
  Void      setUseEarlyCU                   ( Bool  b )     { m_bUseEarlyCU = b; }
  Void      setUseFastDecisionForMerge      ( Bool  b )     { m_useFastDecisionForMerge = b; }
  Void      setUseCbfFastMode            ( Bool  b )     { m_bUseCbfFastMode = b; }
  Void      setUseEarlySkipDetection        ( Bool  b )     { m_useEarlySkipDetection = b; }
  Void      setUseConstrainedIntraPred      ( Bool  b )     { m_bUseConstrainedIntraPred = b; }
  Void      setPCMInputBitDepthFlag         ( Bool  b )     { m_bPCMInputBitDepthFlag = b; }
  Void      setPCMFilterDisableFlag         ( Bool  b )     {  m_bPCMFilterDisableFlag = b; }
  Void      setUsePCM                       ( Bool  b )     {  m_usePCM = b;               }
  Void      setPCMLog2MaxSize               ( UInt u )      { m_pcmLog2MaxSize = u;      }
  Void      setPCMLog2MinSize               ( UInt u )     { m_uiPCMLog2MinSize = u;      }
  Void      setdQPs                         ( Int*  p )     { m_aidQP       = p; }
  Void      setDeltaQpRD                    ( UInt  u )     {m_uiDeltaQpRD  = u; }
  Bool      getUseSBACRD                    ()      { return m_bUseSBACRD;  }
  Bool      getUseASR                       ()      { return m_bUseASR;     }
  Bool      getUseHADME                     ()      { return m_bUseHADME;   }
#if !REMOVE_ALF
  Bool      getUseALF                       ()      { return m_bUseALF;     }
  Void      setALFLowLatencyEncoding        (Bool b) {m_alfLowLatencyEncoding = b;    }
  Bool      getALFLowLatencyEncoding        ()       { return m_alfLowLatencyEncoding;}
#endif
  Bool      getUseLComb                     ()      { return m_bUseLComb;   }
  Bool      getUseRDOQ                      ()      { return m_bUseRDOQ;    }
  Bool      getUseFastEnc                   ()      { return m_bUseFastEnc; }
  Bool      getUseEarlyCU                   ()      { return m_bUseEarlyCU; }
  Bool      getUseFastDecisionForMerge      ()      { return m_useFastDecisionForMerge; }
  Bool      getUseCbfFastMode           ()      { return m_bUseCbfFastMode; }
  Bool      getUseEarlySkipDetection        ()      { return m_useEarlySkipDetection; }
  Bool      getUseConstrainedIntraPred      ()      { return m_bUseConstrainedIntraPred; }
#if !REMOVE_NSQT
#if NS_HAD
  Bool      getUseNSQT                      ()      { return m_useNSQT; }
#endif
#endif
  Bool      getPCMInputBitDepthFlag         ()      { return m_bPCMInputBitDepthFlag;   }
  Bool      getPCMFilterDisableFlag         ()      { return m_bPCMFilterDisableFlag;   } 
  Bool      getUsePCM                       ()      { return m_usePCM;                 }
  UInt      getPCMLog2MaxSize               ()      { return m_pcmLog2MaxSize;  }
  UInt      getPCMLog2MinSize               ()      { return  m_uiPCMLog2MinSize;  }

#if !REMOVE_LMCHROMA
  Bool getUseLMChroma                       ()      { return m_bUseLMChroma;        }
  Void setUseLMChroma                       ( Bool b ) { m_bUseLMChroma  = b;       }
#endif
  Bool getUseTransformSkip                             ()      { return m_useTansformSkip;        }
  Void setUseTransformSkip                             ( Bool b ) { m_useTansformSkip  = b;       }
  Bool getUseTransformSkipFast                         ()      { return m_useTansformSkipFast;    }
  Void setUseTransformSkipFast                         ( Bool b ) { m_useTansformSkipFast  = b;   }
  Int*      getdQPs                         ()      { return m_aidQP;       }
  UInt      getDeltaQpRD                    ()      { return m_uiDeltaQpRD; }

  //====== Slice ========
  Void  setSliceMode                   ( Int  i )       { m_iSliceMode = i;              }
  Void  setSliceArgument               ( Int  i )       { m_iSliceArgument = i;          }
  Int   getSliceMode                   ()              { return m_iSliceMode;           }
  Int   getSliceArgument               ()              { return m_iSliceArgument;       }
  //====== Dependent Slice ========
  Void  setDependentSliceMode            ( Int  i )      { m_iDependentSliceMode = i;       }
  Void  setDependentSliceArgument        ( Int  i )      { m_iDependentSliceArgument = i;   }
  Int   getDependentSliceMode            ()              { return m_iDependentSliceMode;    }
  Int   getDependentSliceArgument        ()              { return m_iDependentSliceArgument;}
#if DEPENDENT_SLICES
  Void  setCabacIndependentFlag            ( Bool  i )      { m_bCabacIndependentFlag = i;       }
  Bool  getCabacIndependentFlag     ()                    { return m_bCabacIndependentFlag;   }
#endif
#if !REMOVE_FGS
  Void  setSliceGranularity            ( Int  i )      { m_iSliceGranularity = i;       }
  Int   getSliceGranularity            ()              { return m_iSliceGranularity;    }
#endif
  Void      setLFCrossSliceBoundaryFlag     ( Bool   bValue  )    { m_bLFCrossSliceBoundaryFlag = bValue; }
  Bool      getLFCrossSliceBoundaryFlag     ()                    { return m_bLFCrossSliceBoundaryFlag;   }

  Void      setUseSAO                  (Bool bVal)     {m_bUseSAO = bVal;}
  Bool      getUseSAO                  ()              {return m_bUseSAO;}
  Void  setMaxNumOffsetsPerPic                   (Int iVal)            { m_maxNumOffsetsPerPic = iVal; }
  Int   getMaxNumOffsetsPerPic                   ()                    { return m_maxNumOffsetsPerPic; }
#if SAO_LCU_BOUNDARY
  Void  setSaoLcuBoundary              (bool bVal)     { m_saoLcuBoundary = bVal; }
  Bool  getSaoLcuBoundary              ()              { return m_saoLcuBoundary; }
#endif
  Void  setSaoLcuBasedOptimization               (bool bVal)           { m_saoLcuBasedOptimization = bVal; }
  Bool  getSaoLcuBasedOptimization               ()                    { return m_saoLcuBasedOptimization; }
  Void  setLFCrossTileBoundaryFlag               ( Bool   bValue  )    { m_bLFCrossTileBoundaryFlag = bValue; }
  Bool  getLFCrossTileBoundaryFlag               ()                    { return m_bLFCrossTileBoundaryFlag;   }
  Void  setUniformSpacingIdr           ( Int i )           { m_iUniformSpacingIdr = i; }
  Int   getUniformSpacingIdr           ()                  { return m_iUniformSpacingIdr; }
  Void  setNumColumnsMinus1            ( Int i )           { m_iNumColumnsMinus1 = i; }
  Int   getNumColumnsMinus1            ()                  { return m_iNumColumnsMinus1; }
  Void  setColumnWidth ( char* str )
  {
    char *columnWidth;
    int  i=0;
    Int  m_iWidthInCU = ( m_iSourceWidth%g_uiMaxCUWidth ) ? m_iSourceWidth/g_uiMaxCUWidth + 1 : m_iSourceWidth/g_uiMaxCUWidth;

    if( m_iUniformSpacingIdr == 0 && m_iNumColumnsMinus1 > 0 )
    {
      m_puiColumnWidth = new UInt[m_iNumColumnsMinus1];

      columnWidth = strtok(str, " ,-");
      while(columnWidth!=NULL)
      {
        if( i>=m_iNumColumnsMinus1 )
        {
          printf( "The number of columns whose width are defined is larger than the allowed number of columns.\n" );
          exit( EXIT_FAILURE );
        }
        *( m_puiColumnWidth + i ) = atoi( columnWidth );
        printf("col: m_iWidthInCU= %4d i=%4d width= %4d\n",m_iWidthInCU,i,m_puiColumnWidth[i]); //AFU
        columnWidth = strtok(NULL, " ,-");
        i++;
      }
      if( i<m_iNumColumnsMinus1 )
      {
        printf( "The width of some columns is not defined.\n" );
        exit( EXIT_FAILURE );
      }
    }
  }
  UInt  getColumnWidth                 ( UInt columnidx )  { return *( m_puiColumnWidth + columnidx ); }
  Void  setNumRowsMinus1               ( Int i )           { m_iNumRowsMinus1 = i; }
  Int   getNumRowsMinus1               ()                  { return m_iNumRowsMinus1; }
  Void  setRowHeight (char* str)
  {
    char *rowHeight;
    int  i=0;
    Int  m_iHeightInCU = ( m_iSourceHeight%g_uiMaxCUHeight ) ? m_iSourceHeight/g_uiMaxCUHeight + 1 : m_iSourceHeight/g_uiMaxCUHeight;

    if( m_iUniformSpacingIdr == 0 && m_iNumRowsMinus1 > 0 )
    {
      m_puiRowHeight = new UInt[m_iNumRowsMinus1];

      rowHeight = strtok(str, " ,-");
      while(rowHeight!=NULL)
      {
        if( i>=m_iNumRowsMinus1 )
        {
          printf( "The number of rows whose height are defined is larger than the allowed number of rows.\n" );
          exit( EXIT_FAILURE );
        }
        *( m_puiRowHeight + i ) = atoi( rowHeight );
        printf("row: m_iHeightInCU=%4d i=%4d height=%4d\n",m_iHeightInCU,i,m_puiRowHeight[i]); //AFU
        rowHeight = strtok(NULL, " ,-");
        i++;
      }
      if( i<m_iNumRowsMinus1 )
      {
        printf( "The height of some rows is not defined.\n" );
        exit( EXIT_FAILURE );
     }
    }
  }
  UInt  getRowHeight                   ( UInt rowIdx )     { return *( m_puiRowHeight + rowIdx ); }
  Void  xCheckGSParameters();
  Void  setWaveFrontSynchro(Int iWaveFrontSynchro)       { m_iWaveFrontSynchro = iWaveFrontSynchro; }
  Int   getWaveFrontsynchro()                            { return m_iWaveFrontSynchro; }
  Void  setWaveFrontSubstreams(Int iWaveFrontSubstreams) { m_iWaveFrontSubstreams = iWaveFrontSubstreams; }
  Int   getWaveFrontSubstreams()                         { return m_iWaveFrontSubstreams; }
  void setPictureDigestEnabled(Int b) { m_pictureDigestEnabled = b; }
  Int getPictureDigestEnabled() { return m_pictureDigestEnabled; }

  Void      setUseWP               ( Bool  b )   { m_bUseWeightPred    = b;    }
  Void      setWPBiPred            ( Bool b )    { m_useWeightedBiPred = b;    }
  Bool      getUseWP               ()            { return m_bUseWeightPred;    }
  Bool      getWPBiPred            ()            { return m_useWeightedBiPred; }
  Void      setLog2ParallelMergeLevelMinus2   ( UInt u )    { m_log2ParallelMergeLevelMinus2       = u;    }
  UInt      getLog2ParallelMergeLevelMinus2   ()            { return m_log2ParallelMergeLevelMinus2;       }
  Void      setUseScalingListId    ( Int  u )    { m_useScalingListId       = u;   }
  Int       getUseScalingListId    ()            { return m_useScalingListId;      }
  Void      setScalingListFile     ( char*  pch ){ m_scalingListFile     = pch; }
  char*     getScalingListFile     ()            { return m_scalingListFile;    }
  Void      setTMVPModeId ( Int  u ) { m_TMVPModeId = u;    }
  Int       getTMVPModeId ()         { return m_TMVPModeId; }
  Void      setSignHideFlag( Int signHideFlag ) { m_signHideFlag = signHideFlag; }
  Int       getSignHideFlag()                    { return m_signHideFlag; }
  Bool      getUseRateCtrl    ()                { return m_enableRateCtrl;    }
  Void      setUseRateCtrl    (Bool flag)       { m_enableRateCtrl = flag;    }
  Int       getTargetBitrate  ()                { return m_targetBitrate;     }
  Void      setTargetBitrate  (Int target)      { m_targetBitrate  = target;  }
  Int       getNumLCUInUnit   ()                { return m_numLCUInUnit;      }
  Void      setNumLCUInUnit   (Int numLCUs)     { m_numLCUInUnit   = numLCUs; }
  Bool      getTransquantBypassEnableFlag()           { return m_TransquantBypassEnableFlag; }
  Void      setTransquantBypassEnableFlag(Bool flag)  { m_TransquantBypassEnableFlag = flag; }
  Bool      getCUTransquantBypassFlagValue()          { return m_CUTransquantBypassFlagValue; }
  Void      setCUTransquantBypassFlagValue(Bool flag) { m_CUTransquantBypassFlagValue = flag; }
  Void setVPS(TComVPS *p) { m_cVPS = *p; }
  TComVPS *getVPS() { return &m_cVPS; }
#if RECALCULATE_QP_ACCORDING_LAMBDA
  Void      setUseRecalculateQPAccordingToLambda ( Bool b ) { m_recalculateQPAccordingToLambda = b;    }
  Bool      getUseRecalculateQPAccordingToLambda ()         { return m_recalculateQPAccordingToLambda; }
#endif
};

//! \}

#endif // !defined(AFX_TENCCFG_H__6B99B797_F4DA_4E46_8E78_7656339A6C41__INCLUDED_)
