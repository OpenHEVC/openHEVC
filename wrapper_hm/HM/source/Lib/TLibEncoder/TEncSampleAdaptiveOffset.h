/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2013, ITU/ISO/IEC
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

/** 
 \file     TEncSampleAdaptiveOffset.h
 \brief    estimation part of sample adaptive offset class (header)
 */

#ifndef __TENCSAMPLEADAPTIVEOFFSET__
#define __TENCSAMPLEADAPTIVEOFFSET__

#include "TLibCommon/TComSampleAdaptiveOffset.h"
#include "TLibCommon/TComPic.h"

#include "TEncEntropy.h"
#include "TEncSbac.h"
#include "TLibCommon/TComBitCounter.h"

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

#if HM_CLEANUP_SAO
enum SAOCabacStateLablesRDO //CABAC state labels
{
  SAO_CABACSTATE_PIC_INIT =0,
  SAO_CABACSTATE_BLK_CUR,
  SAO_CABACSTATE_BLK_NEXT,
  SAO_CABACSTATE_BLK_MID,
  SAO_CABACSTATE_BLK_TEMP,
  NUM_SAO_CABACSTATE_LABELS
};

struct SAOStatData //data structure for SAO statistics
{
  Int64 diff[MAX_NUM_SAO_CLASSES];
  Int64 count[MAX_NUM_SAO_CLASSES];

  SAOStatData(){}
  ~SAOStatData(){}
  Void reset()
  {
    ::memset(diff, 0, sizeof(Int64)*MAX_NUM_SAO_CLASSES);
    ::memset(count, 0, sizeof(Int64)*MAX_NUM_SAO_CLASSES);
  }
  const SAOStatData& operator=(const SAOStatData& src)
  {
    ::memcpy(diff, src.diff, sizeof(Int64)*MAX_NUM_SAO_CLASSES);
    ::memcpy(count, src.count, sizeof(Int64)*MAX_NUM_SAO_CLASSES);
    return *this;
  }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  const SAOStatData& operator+= (const SAOStatData& src)
  {
    for(Int i=0; i< MAX_NUM_SAO_CLASSES; i++)
    {
      diff[i] += src.diff[i];
      count[i] += src.count[i];
    }
    return *this;
  }
#endif
};

class TEncSampleAdaptiveOffset : public TComSampleAdaptiveOffset
{
public:
  TEncSampleAdaptiveOffset();
  virtual ~TEncSampleAdaptiveOffset();

  //interface
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  Void createEncData(Bool isPreDBFSamplesUsed);
#else
  Void createEncData();
#endif
  Void destroyEncData();
  Void initRDOCabacCoder(TEncSbac* pcRDGoOnSbacCoder, TComSlice* pcSlice) ;
  Void SAOProcess(TComPic* pPic, Bool* sliceEnabled, const Double *lambdas
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
                , Bool isPreDBFSamplesUsed
#endif
                ); 
public: //methods
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  Void getPreDBFStatistics(TComPic* pPic); 
#endif
private: //methods
  Void getStatistics(SAOStatData*** blkStats, TComPicYuv* orgYuv, TComPicYuv* srcYuv,TComPic* pPic
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
                   , Bool isCalculatePreDeblockSamples = false
#endif
                   );
  Void decidePicParams(Bool* sliceEnabled, Int picTempLayer);
  Void decideBlkParams(TComPic* pic, Bool* sliceEnabled, SAOStatData*** blkStats, TComPicYuv* srcYuv, TComPicYuv* resYuv, SAOBlkParam* reconParams, SAOBlkParam* codedParams);
  Void getBlkStats(Int compIdx, SAOStatData* statsDataTypes, Pel* srcBlk, Pel* orgBlk, Int srcStride, Int orgStride, Int width, Int height, Bool isLeftAvail,  Bool isRightAvail, Bool isAboveAvail, Bool isBelowAvail, Bool isAboveLeftAvail, Bool isAboveRightAvail, Bool isBelowLeftAvail, Bool isBelowRightAvail
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
                  , Bool isCalculatePreDeblockSamples
#endif
                  );
  Void deriveModeNewRDO(Int ctu, std::vector<SAOBlkParam*>& mergeList, Bool* sliceEnabled, SAOStatData*** blkStats, SAOBlkParam& modeParam, Double& modeNormCost, TEncSbac** cabacCoderRDO, Int inCabacLabel);
  Void deriveModeMergeRDO(Int ctu, std::vector<SAOBlkParam*>& mergeList, Bool* sliceEnabled, SAOStatData*** blkStats, SAOBlkParam& modeParam, Double& modeNormCost, TEncSbac** cabacCoderRDO, Int inCabacLabel);
  Int64 getDistortion(Int ctu, Int compIdx, Int typeIdc, Int typeAuxInfo, Int* offsetVal, SAOStatData& statData);
  Void deriveOffsets(Int ctu, Int compIdx, Int typeIdc, SAOStatData& statData, Int* quantOffsets, Int& typeAuxInfo);
  inline Int64 estSaoDist(Int64 count, Int64 offset, Int64 diffSum, Int shift);
  inline Int estIterOffset(Int typeIdx, Int classIdx, Double lambda, Int offsetInput, Int64 count, Int64 diffSum, Int shift, Int bitIncrease, Int64& bestDist, Double& bestCost, Int offsetTh );
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  Void addPreDBFStatistics(SAOStatData*** blkStats);
#endif
private: //members
  //for RDO
  TEncSbac**             m_pppcRDSbacCoder;           
  TEncSbac*              m_pcRDGoOnSbacCoder;
  TEncBinCABACCounter**  m_pppcBinCoderCABAC;    
  Double                 m_labmda[NUM_SAO_COMPONENTS];

  //statistics
  SAOStatData***         m_statData; //[ctu][comp][classes]
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  SAOStatData***         m_preDBFstatData;
#endif
#if SAO_ENCODING_CHOICE
  Double                 m_saoDisabledRate[NUM_SAO_COMPONENTS][MAX_TLAYER];
#endif
  Int                    m_skipLinesR[NUM_SAO_COMPONENTS][NUM_SAO_NEW_TYPES];
  Int                    m_skipLinesB[NUM_SAO_COMPONENTS][NUM_SAO_NEW_TYPES];
};
#else


class TEncSampleAdaptiveOffset : public TComSampleAdaptiveOffset
{
private:
  TEncEntropy*      m_pcEntropyCoder;
  TEncSbac***       m_pppcRDSbacCoder;              ///< for CABAC
  TEncSbac*         m_pcRDGoOnSbacCoder;
#if FAST_BIT_EST
  TEncBinCABACCounter*** m_pppcBinCoderCABAC;            ///< temporal CABAC state storage for RD computation
#else
  TEncBinCABAC***   m_pppcBinCoderCABAC;            ///< temporal CABAC state storage for RD computation
#endif
  
  Int64  ***m_iCount;      //[MAX_NUM_SAO_PART][MAX_NUM_SAO_TYPE][MAX_NUM_SAO_CLASS]; 
  Int64  ***m_iOffset;     //[MAX_NUM_SAO_PART][MAX_NUM_SAO_TYPE][MAX_NUM_SAO_CLASS]; 
  Int64  ***m_iOffsetOrg;  //[MAX_NUM_SAO_PART][MAX_NUM_SAO_TYPE]; 
  Int64  ****m_count_PreDblk;      //[LCU][YCbCr][MAX_NUM_SAO_TYPE][MAX_NUM_SAO_CLASS]; 
  Int64  ****m_offsetOrg_PreDblk;  //[LCU][YCbCr][MAX_NUM_SAO_TYPE][MAX_NUM_SAO_CLASS]; 
  Int64  **m_iRate;        //[MAX_NUM_SAO_PART][MAX_NUM_SAO_TYPE]; 
  Int64  **m_iDist;        //[MAX_NUM_SAO_PART][MAX_NUM_SAO_TYPE]; 
  Double **m_dCost;        //[MAX_NUM_SAO_PART][MAX_NUM_SAO_TYPE]; 
  Double *m_dCostPartBest; //[MAX_NUM_SAO_PART]; 
  Int64  *m_iDistOrg;      //[MAX_NUM_SAO_PART]; 
  Int    *m_iTypePartBest; //[MAX_NUM_SAO_PART]; 
  Int     m_iOffsetThY;
  Int     m_iOffsetThC;
  Bool    m_bUseSBACRD;
#if SAO_ENCODING_CHOICE
#if SAO_ENCODING_CHOICE_CHROMA
  Double  m_depthSaoRate[2][4];
#else
  Double  m_depth0SaoRate;
#endif
#endif

public:
  TEncSampleAdaptiveOffset         ();
  virtual ~TEncSampleAdaptiveOffset();

  Void startSaoEnc( TComPic* pcPic, TEncEntropy* pcEntropyCoder, TEncSbac*** pppcRDSbacCoder, TEncSbac* pcRDGoOnSbacCoder);
  Void endSaoEnc();
  Void resetStats();
#if SAO_ENCODING_CHOICE
  Void SAOProcess(SAOParam *pcSaoParam, Double dLambda, Double dLambdaChroma, Int depth);
#else
  Void SAOProcess(SAOParam *pcSaoParam, Double dLambda, Double dLambdaChroma);
#endif

  Void runQuadTreeDecision(SAOQTPart *psQTPart, Int iPartIdx, Double &dCostFinal, Int iMaxLevel, Double dLambda, Int yCbCr);
  Void rdoSaoOnePart(SAOQTPart *psQTPart, Int iPartIdx, Double dLambda, Int yCbCr);
  
  Void disablePartTree(SAOQTPart *psQTPart, Int iPartIdx);
  Void getSaoStats(SAOQTPart *psQTPart, Int iYCbCr);
  Void calcSaoStatsCu(Int iAddr, Int iPartIdx, Int iYCbCr);
  Void calcSaoStatsBlock( Pel* pRecStart, Pel* pOrgStart, Int stride, Int64** ppStats, Int64** ppCount, UInt width, UInt height, Bool* pbBorderAvail, Int iYCbCr);
  Void calcSaoStatsCuOrg(Int iAddr, Int iPartIdx, Int iYCbCr);
  Void calcSaoStatsCu_BeforeDblk( TComPic* pcPic );
  Void destroyEncBuffer();
  Void createEncBuffer();
  Void assignSaoUnitSyntax(SaoLcuParam* saoLcuParam,  SAOQTPart* saoPart, Bool &oneUnitFlag, Int yCbCr);
  Void checkMerge(SaoLcuParam * lcuParamCurr,SaoLcuParam * lcuParamCheck, Int dir);
#if SAO_ENCODING_CHOICE
  Void rdoSaoUnitAll(SAOParam *saoParam, Double lambda, Double lambdaChroma, Int depth);
#else
  Void rdoSaoUnitAll(SAOParam *saoParam, Double lambda, Double lambdaChroma);
#endif
  Void saoComponentParamDist(Int allowMergeLeft, Int allowMergeUp, SAOParam *saoParam, Int addr, Int addrUp, Int addrLeft, Int yCbCr, Double lambda, SaoLcuParam *compSaoParam, Double *distortion);
  Void sao2ChromaParamDist(Int allowMergeLeft, Int allowMergeUp, SAOParam *saoParam, Int addr, Int addrUp, Int addrLeft, Double lambda, SaoLcuParam *crSaoParam, SaoLcuParam *cbSaoParam, Double *distortion);
  inline Int64 estSaoDist(Int64 count, Int64 offset, Int64 offsetOrg, Int shift);
  inline Int64 estIterOffset(Int typeIdx, Int classIdx, Double lambda, Int64 offsetInput, Int64 count, Int64 offsetOrg, Int shift, Int bitIncrease, Int *currentDistortionTableBo, Double *currentRdCostTableBo, Int offsetTh );
  inline Int64 estSaoTypeDist(Int compIdx, Int typeIdx, Int shift, Double lambda, Int *currentDistortionTableBo, Double *currentRdCostTableBo);
  Void setMaxNumOffsetsPerPic(Int iVal) {m_maxNumOffsetsPerPic = iVal; }
  Int  getMaxNumOffsetsPerPic() {return m_maxNumOffsetsPerPic; }
};
#endif
//! \}

#endif
