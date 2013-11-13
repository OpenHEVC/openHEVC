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
 \file     TEncSampleAdaptiveOffset.cpp
 \brief       estimation part of sample adaptive offset class
 */
#include "TEncSampleAdaptiveOffset.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//! \ingroup TLibEncoder
//! \{


#if HM_CLEANUP_SAO

/** rounding with IBDI
 * \param  x
 */
inline Double xRoundIbdi2(Int bitDepth, Double x)
{
  return ((x)>0) ? (Int)(((Int)(x)+(1<<(bitDepth-8-1)))/(1<<(bitDepth-8))) : ((Int)(((Int)(x)-(1<<(bitDepth-8-1)))/(1<<(bitDepth-8))));
}

inline Double xRoundIbdi(Int bitDepth, Double x)
{
  return (bitDepth > 8 ? xRoundIbdi2(bitDepth, (x)) : ((x)>=0 ? ((Int)((x)+0.5)) : ((Int)((x)-0.5)))) ;
}


TEncSampleAdaptiveOffset::TEncSampleAdaptiveOffset()
{
  m_pppcRDSbacCoder = NULL;           
  m_pcRDGoOnSbacCoder = NULL;
  m_pppcBinCoderCABAC = NULL;    
  m_statData = NULL;
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  m_preDBFstatData = NULL;
#endif
}

TEncSampleAdaptiveOffset::~TEncSampleAdaptiveOffset()
{
  destroyEncData();
}

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
Void TEncSampleAdaptiveOffset::createEncData(Bool isPreDBFSamplesUsed)
#else
Void TEncSampleAdaptiveOffset::createEncData()
#endif
{

  //cabac coder for RDO
  m_pppcRDSbacCoder = new TEncSbac* [NUM_SAO_CABACSTATE_LABELS];
  m_pppcBinCoderCABAC = new TEncBinCABACCounter* [NUM_SAO_CABACSTATE_LABELS];

  for(Int cs=0; cs < NUM_SAO_CABACSTATE_LABELS; cs++)
  {
    m_pppcRDSbacCoder[cs] = new TEncSbac;
    m_pppcBinCoderCABAC[cs] = new TEncBinCABACCounter;
    m_pppcRDSbacCoder   [cs]->init( m_pppcBinCoderCABAC [cs] );
  }


  //statistics
  m_statData = new SAOStatData**[m_numCTUsPic];
  for(Int i=0; i< m_numCTUsPic; i++)
  {
    m_statData[i] = new SAOStatData*[NUM_SAO_COMPONENTS];
    for(Int compIdx=0; compIdx < NUM_SAO_COMPONENTS; compIdx++)
    {
      m_statData[i][compIdx] = new SAOStatData[NUM_SAO_NEW_TYPES];
    }
  }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  if(isPreDBFSamplesUsed)
  {
    m_preDBFstatData = new SAOStatData**[m_numCTUsPic];
    for(Int i=0; i< m_numCTUsPic; i++)
    {
      m_preDBFstatData[i] = new SAOStatData*[NUM_SAO_COMPONENTS];
      for(Int compIdx=0; compIdx < NUM_SAO_COMPONENTS; compIdx++)
      {
        m_preDBFstatData[i][compIdx] = new SAOStatData[NUM_SAO_NEW_TYPES];
      }
    }

  }
#endif

#if SAO_ENCODING_CHOICE
  ::memset(m_saoDisabledRate, 0, sizeof(m_saoDisabledRate));
#endif

  for(Int typeIdc=0; typeIdc < NUM_SAO_NEW_TYPES; typeIdc++)
  {
    m_skipLinesR[SAO_Y ][typeIdc]= 5;
    m_skipLinesR[SAO_Cb][typeIdc]= m_skipLinesR[SAO_Cr][typeIdc]= 3;

    m_skipLinesB[SAO_Y ][typeIdc]= 4;
    m_skipLinesB[SAO_Cb][typeIdc]= m_skipLinesB[SAO_Cr][typeIdc]= 2;

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
    if(isPreDBFSamplesUsed)
    {
      switch(typeIdc)
      {
      case SAO_TYPE_EO_0:
        {
          m_skipLinesR[SAO_Y ][typeIdc]= 5;
          m_skipLinesR[SAO_Cb][typeIdc]= m_skipLinesR[SAO_Cr][typeIdc]= 3;

          m_skipLinesB[SAO_Y ][typeIdc]= 3;
          m_skipLinesB[SAO_Cb][typeIdc]= m_skipLinesB[SAO_Cr][typeIdc]= 1;
        }
        break;
      case SAO_TYPE_EO_90:
        {
          m_skipLinesR[SAO_Y ][typeIdc]= 4;
          m_skipLinesR[SAO_Cb][typeIdc]= m_skipLinesR[SAO_Cr][typeIdc]= 2;

          m_skipLinesB[SAO_Y ][typeIdc]= 4;
          m_skipLinesB[SAO_Cb][typeIdc]= m_skipLinesB[SAO_Cr][typeIdc]= 2;
        }
        break;
      case SAO_TYPE_EO_135:
      case SAO_TYPE_EO_45:
        {
          m_skipLinesR[SAO_Y ][typeIdc]= 5;
          m_skipLinesR[SAO_Cb][typeIdc]= m_skipLinesR[SAO_Cr][typeIdc]= 3;

          m_skipLinesB[SAO_Y ][typeIdc]= 4;
          m_skipLinesB[SAO_Cb][typeIdc]= m_skipLinesB[SAO_Cr][typeIdc]= 2;
        }
        break;
      case SAO_TYPE_BO:
        {
          m_skipLinesR[SAO_Y ][typeIdc]= 4;
          m_skipLinesR[SAO_Cb][typeIdc]= m_skipLinesR[SAO_Cr][typeIdc]= 2;

          m_skipLinesB[SAO_Y ][typeIdc]= 3;
          m_skipLinesB[SAO_Cb][typeIdc]= m_skipLinesB[SAO_Cr][typeIdc]= 1;
        }
        break;
      default:
        {
          printf("Not a supported type");
          assert(0);
          exit(-1);
        }
      }
    }
#endif    
  }

}

Void TEncSampleAdaptiveOffset::destroyEncData()
{
  if(m_pppcRDSbacCoder != NULL)
  {
    for (Int cs = 0; cs < NUM_SAO_CABACSTATE_LABELS; cs ++ )
    {
      delete m_pppcRDSbacCoder[cs];
    }
    delete[] m_pppcRDSbacCoder; m_pppcRDSbacCoder = NULL;
  }

  if(m_pppcBinCoderCABAC != NULL)
  {
    for (Int cs = 0; cs < NUM_SAO_CABACSTATE_LABELS; cs ++ )
    {
      delete m_pppcBinCoderCABAC[cs];
    }
    delete[] m_pppcBinCoderCABAC; m_pppcBinCoderCABAC = NULL;
  }

  if(m_statData != NULL)
  {
    for(Int i=0; i< m_numCTUsPic; i++)
    {
      for(Int compIdx=0; compIdx< NUM_SAO_COMPONENTS; compIdx++)
      {
        delete[] m_statData[i][compIdx];
      }
      delete[] m_statData[i];
    }
    delete[] m_statData; m_statData = NULL;
  }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  if(m_preDBFstatData != NULL)
  {
    for(Int i=0; i< m_numCTUsPic; i++)
    {
      for(Int compIdx=0; compIdx< NUM_SAO_COMPONENTS; compIdx++)
      {
        delete[] m_preDBFstatData[i][compIdx];
      }
      delete[] m_preDBFstatData[i];
    }
    delete[] m_preDBFstatData; m_preDBFstatData = NULL;
  }

#endif
}

Void TEncSampleAdaptiveOffset::initRDOCabacCoder(TEncSbac* pcRDGoOnSbacCoder, TComSlice* pcSlice) 
{
  m_pcRDGoOnSbacCoder = pcRDGoOnSbacCoder;
  m_pcRDGoOnSbacCoder->setSlice(pcSlice);
  m_pcRDGoOnSbacCoder->resetEntropy();
  m_pcRDGoOnSbacCoder->resetBits();

  m_pcRDGoOnSbacCoder->store( m_pppcRDSbacCoder[SAO_CABACSTATE_PIC_INIT]);
}



Void TEncSampleAdaptiveOffset::SAOProcess(TComPic* pPic, Bool* sliceEnabled, const Double *lambdas
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
                                         , Bool isPreDBFSamplesUsed
#endif
                                          )
{
  TComPicYuv* orgYuv= pPic->getPicYuvOrg();
  TComPicYuv* resYuv= pPic->getPicYuvRec();
  m_labmda[SAO_Y]= lambdas[0]; m_labmda[SAO_Cb]= lambdas[1]; m_labmda[SAO_Cr]= lambdas[2];
  TComPicYuv* srcYuv = m_tempPicYuv;
  resYuv->copyToPic(srcYuv);
  srcYuv->setBorderExtension(false);
  srcYuv->extendPicBorder();

  //collect statistics
  getStatistics(m_statData, orgYuv, srcYuv, pPic);
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
  if(isPreDBFSamplesUsed)
  {
    addPreDBFStatistics(m_statData);
  }
#endif
  //slice on/off 
  decidePicParams(sliceEnabled, pPic->getSlice(0)->getDepth()); 

  //block on/off 
  SAOBlkParam* reconParams = new SAOBlkParam[m_numCTUsPic]; //temporary parameter buffer for storing reconstructed SAO parameters
  decideBlkParams(pPic, sliceEnabled, m_statData, srcYuv, resYuv, reconParams, pPic->getPicSym()->getSAOBlkParam());
  delete[] reconParams;

}

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
Void TEncSampleAdaptiveOffset::getPreDBFStatistics(TComPic* pPic)
{
  getStatistics(m_preDBFstatData, pPic->getPicYuvOrg(), pPic->getPicYuvRec(), pPic, true);
}

Void TEncSampleAdaptiveOffset::addPreDBFStatistics(SAOStatData*** blkStats)
{
  for(Int n=0; n< m_numCTUsPic; n++)
  {
    for(Int compIdx=0; compIdx < NUM_SAO_COMPONENTS; compIdx++)
    {
      for(Int typeIdc=0; typeIdc < NUM_SAO_NEW_TYPES; typeIdc++)
      {
        blkStats[n][compIdx][typeIdc] += m_preDBFstatData[n][compIdx][typeIdc];
      }
    }
  }
}

#endif

Void TEncSampleAdaptiveOffset::getStatistics(SAOStatData*** blkStats, TComPicYuv* orgYuv, TComPicYuv* srcYuv, TComPic* pPic
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
                          , Bool isCalculatePreDeblockSamples
#endif
                          )
{
  Bool isLeftAvail,isRightAvail,isAboveAvail,isBelowAvail,isAboveLeftAvail,isAboveRightAvail,isBelowLeftAvail,isBelowRightAvail;

  for(Int ctu= 0; ctu < m_numCTUsPic; ctu++)
  {
    Int yPos   = (ctu / m_numCTUInWidth)*m_maxCUHeight;
    Int xPos   = (ctu % m_numCTUInWidth)*m_maxCUWidth;
    Int height = (yPos + m_maxCUHeight > m_picHeight)?(m_picHeight- yPos):m_maxCUHeight;
    Int width  = (xPos + m_maxCUWidth  > m_picWidth )?(m_picWidth - xPos):m_maxCUWidth;

    pPic->getPicSym()->deriveLoopFilterBoundaryAvailibility(ctu, isLeftAvail,isRightAvail,isAboveAvail,isBelowAvail,isAboveLeftAvail,isAboveRightAvail,isBelowLeftAvail,isBelowRightAvail);

    //NOTE: The number of skipped lines during gathering CTU statistics depends on the slice boundary availabilities.
    //For simplicity, here only picture boundaries are considered.

    isRightAvail      = (xPos + m_maxCUWidth  < m_picWidth );
    isBelowAvail      = (yPos + m_maxCUHeight < m_picHeight);
    isBelowRightAvail = (isRightAvail && isBelowAvail);
    isBelowLeftAvail  = ((xPos > 0) && (isBelowAvail));
    isAboveRightAvail = ((yPos > 0) && (isRightAvail));

    for(Int compIdx=0; compIdx< NUM_SAO_COMPONENTS; compIdx++)
    {
      Bool isLuma     = (compIdx == SAO_Y);
      Int  formatShift= isLuma?0:1;

      Int  srcStride = isLuma?srcYuv->getStride():srcYuv->getCStride();
      Pel* srcBlk    = getPicBuf(srcYuv, compIdx)+ (yPos >> formatShift)*srcStride+ (xPos >> formatShift);

      Int  orgStride  = isLuma?orgYuv->getStride():orgYuv->getCStride();
      Pel* orgBlk     = getPicBuf(orgYuv, compIdx)+ (yPos >> formatShift)*orgStride+ (xPos >> formatShift);

      getBlkStats(compIdx, blkStats[ctu][compIdx]  
                , srcBlk, orgBlk, srcStride, orgStride, (width  >> formatShift), (height >> formatShift)
                , isLeftAvail,  isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail, isBelowLeftAvail, isBelowRightAvail
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
                , isCalculatePreDeblockSamples
#endif
                );

    }
  }
}

Void TEncSampleAdaptiveOffset::decidePicParams(Bool* sliceEnabled, Int picTempLayer)
{
  //decide sliceEnabled[compIdx]
  for (Int compIdx=0; compIdx<NUM_SAO_COMPONENTS; compIdx++)
  {
    // reset flags & counters
    sliceEnabled[compIdx] = true;

#if SAO_ENCODING_CHOICE
#if SAO_ENCODING_CHOICE_CHROMA
    // decide slice-level on/off based on previous results
    if( (picTempLayer > 0) 
      && (m_saoDisabledRate[compIdx][picTempLayer-1] > ((compIdx==SAO_Y) ? SAO_ENCODING_RATE : SAO_ENCODING_RATE_CHROMA)) )
    {
      sliceEnabled[compIdx] = false;
    }
#else
    // decide slice-level on/off based on previous results
    if( (picTempLayer > 0) 
      && (m_saoDisabledRate[SAO_Y][0] > SAO_ENCODING_RATE) )
    {
      sliceEnabled[compIdx] = false;
    }
#endif
#endif
  }
}

Int64 TEncSampleAdaptiveOffset::getDistortion(Int ctu, Int compIdx, Int typeIdc, Int typeAuxInfo, Int* invQuantOffset, SAOStatData& statData)
{
  Int64 dist=0;
  Int inputBitDepth    = (compIdx == SAO_Y) ? g_bitDepthY : g_bitDepthC ;
  Int shift = 2 * DISTORTION_PRECISION_ADJUSTMENT(inputBitDepth-8);

  switch(typeIdc)
  {
    case SAO_TYPE_EO_0:
    case SAO_TYPE_EO_90:
    case SAO_TYPE_EO_135:
    case SAO_TYPE_EO_45:
      {
        for (Int offsetIdx=0; offsetIdx<NUM_SAO_EO_CLASSES; offsetIdx++)
        {
          dist += estSaoDist( statData.count[offsetIdx], invQuantOffset[offsetIdx], statData.diff[offsetIdx], shift);
        }        
      }
      break;
    case SAO_TYPE_BO:
      {
        for (Int offsetIdx=typeAuxInfo; offsetIdx<typeAuxInfo+4; offsetIdx++)
        {
          Int bandIdx = offsetIdx % NUM_SAO_BO_CLASSES ; 
          dist += estSaoDist( statData.count[bandIdx], invQuantOffset[bandIdx], statData.diff[bandIdx], shift);
        }
      }
      break;
    default:
      {
        printf("Not a supported type");
        assert(0);
        exit(-1);
      }
  }

  return dist;
}

inline Int64 TEncSampleAdaptiveOffset::estSaoDist(Int64 count, Int64 offset, Int64 diffSum, Int shift)
{
  return (( count*offset*offset-diffSum*offset*2 ) >> shift);
}


inline Int TEncSampleAdaptiveOffset::estIterOffset(Int typeIdx, Int classIdx, Double lambda, Int offsetInput, Int64 count, Int64 diffSum, Int shift, Int bitIncrease, Int64& bestDist, Double& bestCost, Int offsetTh )
{
  Int iterOffset, tempOffset;
  Int64 tempDist, tempRate;
  Double tempCost, tempMinCost;
  Int offsetOutput = 0;
  iterOffset = offsetInput;
  // Assuming sending quantized value 0 results in zero offset and sending the value zero needs 1 bit. entropy coder can be used to measure the exact rate here. 
  tempMinCost = lambda; 
  while (iterOffset != 0)
  {
    // Calculate the bits required for signaling the offset
    tempRate = (typeIdx == SAO_TYPE_BO) ? (abs((Int)iterOffset)+2) : (abs((Int)iterOffset)+1); 
    if (abs((Int)iterOffset)==offsetTh) //inclusive 
    {  
      tempRate --;
    }
    // Do the dequantization before distortion calculation
    tempOffset  = iterOffset << bitIncrease;
    tempDist    = estSaoDist( count, tempOffset, diffSum, shift);
    tempCost    = ((Double)tempDist + lambda * (Double) tempRate);
    if(tempCost < tempMinCost)
    {
      tempMinCost = tempCost;
      offsetOutput = iterOffset;
      bestDist = tempDist;
      bestCost = tempCost;
    }
    iterOffset = (iterOffset > 0) ? (iterOffset-1):(iterOffset+1);
  }
  return offsetOutput;
}


Void TEncSampleAdaptiveOffset::deriveOffsets(Int ctu, Int compIdx, Int typeIdc, SAOStatData& statData, Int* quantOffsets, Int& typeAuxInfo)
{
  Int bitDepth = (compIdx== SAO_Y) ? g_bitDepthY : g_bitDepthC;
  Int shift = 2 * DISTORTION_PRECISION_ADJUSTMENT(bitDepth-8);
  Int offsetTh = g_saoMaxOffsetQVal[compIdx];  //inclusive

  ::memset(quantOffsets, 0, sizeof(Int)*MAX_NUM_SAO_CLASSES);

  //derive initial offsets 
  Int numClasses = (typeIdc == SAO_TYPE_BO)?((Int)NUM_SAO_BO_CLASSES):((Int)NUM_SAO_EO_CLASSES);
  for(Int classIdx=0; classIdx< numClasses; classIdx++)
  {
    if( (typeIdc != SAO_TYPE_BO) && (classIdx==SAO_CLASS_EO_PLAIN)  ) 
    {
      continue; //offset will be zero
    }

    if(statData.count[classIdx] == 0)
    {
      continue; //offset will be zero
    }

    quantOffsets[classIdx] = (Int) xRoundIbdi(bitDepth, (Double)( statData.diff[classIdx]<<(bitDepth-8)) 
                                                                  / 
                                                          (Double)( statData.count[classIdx]<< m_offsetStepLog2[compIdx])
                                               );
    quantOffsets[classIdx] = Clip3(-offsetTh, offsetTh, quantOffsets[classIdx]);
  }

  // adjust offsets
  switch(typeIdc)
  {
    case SAO_TYPE_EO_0:
    case SAO_TYPE_EO_90:
    case SAO_TYPE_EO_135:
    case SAO_TYPE_EO_45:
      {
        Int64 classDist;
        Double classCost;
        for(Int classIdx=0; classIdx<NUM_SAO_EO_CLASSES; classIdx++)  
        {         
          if(classIdx==SAO_CLASS_EO_FULL_VALLEY && quantOffsets[classIdx] < 0) quantOffsets[classIdx] =0;
          if(classIdx==SAO_CLASS_EO_HALF_VALLEY && quantOffsets[classIdx] < 0) quantOffsets[classIdx] =0;
          if(classIdx==SAO_CLASS_EO_HALF_PEAK   && quantOffsets[classIdx] > 0) quantOffsets[classIdx] =0;
          if(classIdx==SAO_CLASS_EO_FULL_PEAK   && quantOffsets[classIdx] > 0) quantOffsets[classIdx] =0;

          if( quantOffsets[classIdx] != 0 ) //iterative adjustment only when derived offset is not zero
          {
            quantOffsets[classIdx] = estIterOffset( typeIdc, classIdx, m_labmda[compIdx], quantOffsets[classIdx], statData.count[classIdx], statData.diff[classIdx], shift, m_offsetStepLog2[compIdx], classDist , classCost , offsetTh );
          }
        }
      
        typeAuxInfo =0;
      }
      break;
    case SAO_TYPE_BO:
      {
        Int64  distBOClasses[NUM_SAO_BO_CLASSES];
        Double costBOClasses[NUM_SAO_BO_CLASSES];
        ::memset(distBOClasses, 0, sizeof(Int64)*NUM_SAO_BO_CLASSES);
        for(Int classIdx=0; classIdx< NUM_SAO_BO_CLASSES; classIdx++)
        {         
          costBOClasses[classIdx]= m_labmda[compIdx];
          if( quantOffsets[classIdx] != 0 ) //iterative adjustment only when derived offset is not zero
          {
            quantOffsets[classIdx] = estIterOffset( typeIdc, classIdx, m_labmda[compIdx], quantOffsets[classIdx], statData.count[classIdx], statData.diff[classIdx], shift, m_offsetStepLog2[compIdx], distBOClasses[classIdx], costBOClasses[classIdx], offsetTh );
          }
        }

        //decide the starting band index
        Double minCost = MAX_DOUBLE, cost;
        for(Int band=0; band< NUM_SAO_BO_CLASSES- 4+ 1; band++) 
        {
          cost  = costBOClasses[band  ];
          cost += costBOClasses[band+1];
          cost += costBOClasses[band+2];
          cost += costBOClasses[band+3];

          if(cost < minCost)
          {
            minCost = cost;
            typeAuxInfo = band;
          }
        }
        //clear those unused classes
        Int clearQuantOffset[NUM_SAO_BO_CLASSES];
        ::memset(clearQuantOffset, 0, sizeof(Int)*NUM_SAO_BO_CLASSES);
        for(Int i=0; i< 4; i++) 
        {
          Int band = (typeAuxInfo+i)%NUM_SAO_BO_CLASSES;
          clearQuantOffset[band] = quantOffsets[band];
        }
        ::memcpy(quantOffsets, clearQuantOffset, sizeof(Int)*NUM_SAO_BO_CLASSES);        
      }
      break;
    default:
      {
        printf("Not a supported type");
        assert(0);
        exit(-1);
      }

  }


}


Void TEncSampleAdaptiveOffset::deriveModeNewRDO(Int ctu, std::vector<SAOBlkParam*>& mergeList, Bool* sliceEnabled, SAOStatData*** blkStats, SAOBlkParam& modeParam, Double& modeNormCost, TEncSbac** cabacCoderRDO, Int inCabacLabel)
{
  Double minCost, cost;
  Int rate, minRate;
  Int64 dist[NUM_SAO_COMPONENTS], modeDist[NUM_SAO_COMPONENTS];
  SAOOffset testOffset[NUM_SAO_COMPONENTS];
  Int compIdx;
  Int invQuantOffset[MAX_NUM_SAO_CLASSES];

  modeDist[SAO_Y]= modeDist[SAO_Cb] = modeDist[SAO_Cr] = 0;

  //pre-encode merge flags
  modeParam[SAO_Y ].modeIdc = SAO_MODE_OFF;
  m_pcRDGoOnSbacCoder->load(cabacCoderRDO[inCabacLabel]);
  m_pcRDGoOnSbacCoder->codeSAOBlkParam(modeParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), true);
  m_pcRDGoOnSbacCoder->store(cabacCoderRDO[SAO_CABACSTATE_BLK_MID]);

  //------ luma --------//
  compIdx = SAO_Y;
  //"off" case as initial cost
  modeParam[compIdx].modeIdc = SAO_MODE_OFF;
  m_pcRDGoOnSbacCoder->resetBits();
  m_pcRDGoOnSbacCoder->codeSAOOffsetParam(compIdx, modeParam[compIdx], sliceEnabled[compIdx]);
  minRate= m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();
  modeDist[compIdx] = 0;
  minCost= m_labmda[compIdx]*((Double)minRate);
  m_pcRDGoOnSbacCoder->store(cabacCoderRDO[SAO_CABACSTATE_BLK_TEMP]);
  if(sliceEnabled[compIdx])
  {
    for(Int typeIdc=0; typeIdc< NUM_SAO_NEW_TYPES; typeIdc++)
    {
      testOffset[compIdx].modeIdc = SAO_MODE_NEW;
      testOffset[compIdx].typeIdc = typeIdc;

      //derive coded offset
      deriveOffsets(ctu, compIdx, typeIdc, blkStats[ctu][compIdx][typeIdc], testOffset[compIdx].offset, testOffset[compIdx].typeAuxInfo);

      //inversed quantized offsets
      invertQuantOffsets(compIdx, typeIdc, testOffset[compIdx].typeAuxInfo, invQuantOffset, testOffset[compIdx].offset);

      //get distortion
      dist[compIdx] = getDistortion(ctu, compIdx, testOffset[compIdx].typeIdc, testOffset[compIdx].typeAuxInfo, invQuantOffset, blkStats[ctu][compIdx][typeIdc]);

      //get rate
      m_pcRDGoOnSbacCoder->load(cabacCoderRDO[SAO_CABACSTATE_BLK_MID]);
      m_pcRDGoOnSbacCoder->resetBits();
      m_pcRDGoOnSbacCoder->codeSAOOffsetParam(compIdx, testOffset[compIdx], sliceEnabled[compIdx]);
      rate = m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();
      cost = (Double)dist[compIdx] + m_labmda[compIdx]*((Double)rate);
      if(cost < minCost)
      {
        minCost = cost;
        minRate = rate;
        modeDist[compIdx] = dist[compIdx];
        modeParam[compIdx]= testOffset[compIdx];
        m_pcRDGoOnSbacCoder->store(cabacCoderRDO[SAO_CABACSTATE_BLK_TEMP]);
      }
    }
  }
  m_pcRDGoOnSbacCoder->load(cabacCoderRDO[SAO_CABACSTATE_BLK_TEMP]);
  m_pcRDGoOnSbacCoder->store(cabacCoderRDO[SAO_CABACSTATE_BLK_MID]);

  //------ chroma --------//
  assert(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);
  Double chromaLambda = m_labmda[SAO_Cb];
  //"off" case as initial cost
  m_pcRDGoOnSbacCoder->resetBits();
  modeParam[SAO_Cb].modeIdc = SAO_MODE_OFF; 
  m_pcRDGoOnSbacCoder->codeSAOOffsetParam(SAO_Cb, modeParam[SAO_Cb], sliceEnabled[SAO_Cb]);
  modeParam[SAO_Cr].modeIdc = SAO_MODE_OFF; 
  m_pcRDGoOnSbacCoder->codeSAOOffsetParam(SAO_Cr, modeParam[SAO_Cr], sliceEnabled[SAO_Cr]);
  minRate= m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();
  modeDist[SAO_Cb] = modeDist[SAO_Cr]= 0;
  minCost= chromaLambda*((Double)minRate); 

  //doesn't need to store cabac status here since the whole CTU parameters will be re-encoded at the end of this function

  for(Int typeIdc=0; typeIdc< NUM_SAO_NEW_TYPES; typeIdc++)
  {
    for(compIdx= SAO_Cb; compIdx< NUM_SAO_COMPONENTS; compIdx++)
    {
      if(!sliceEnabled[compIdx])
      {
        testOffset[compIdx].modeIdc = SAO_MODE_OFF;
        dist[compIdx]= 0;
        continue;
      }
      testOffset[compIdx].modeIdc = SAO_MODE_NEW;
      testOffset[compIdx].typeIdc = typeIdc;

      //derive offset & get distortion
      deriveOffsets(ctu, compIdx, typeIdc, blkStats[ctu][compIdx][typeIdc], testOffset[compIdx].offset, testOffset[compIdx].typeAuxInfo);
      invertQuantOffsets(compIdx, typeIdc, testOffset[compIdx].typeAuxInfo, invQuantOffset, testOffset[compIdx].offset);
      dist[compIdx]= getDistortion(ctu, compIdx, typeIdc, testOffset[compIdx].typeAuxInfo, invQuantOffset, blkStats[ctu][compIdx][typeIdc]);
    }

    //get rate
    m_pcRDGoOnSbacCoder->load(cabacCoderRDO[SAO_CABACSTATE_BLK_MID]);
    m_pcRDGoOnSbacCoder->resetBits();
    m_pcRDGoOnSbacCoder->codeSAOOffsetParam(SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
    m_pcRDGoOnSbacCoder->codeSAOOffsetParam(SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);
    rate = m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();

    cost = (Double)(dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*((Double)rate);
    if(cost < minCost)
    {
      minCost = cost;
      minRate = rate;
      modeDist[SAO_Cb] = dist[SAO_Cb];
      modeDist[SAO_Cr] = dist[SAO_Cr];
      modeParam[SAO_Cb]= testOffset[SAO_Cb];
      modeParam[SAO_Cr]= testOffset[SAO_Cr];
    }
  }


  //----- re-gen rate & normalized cost----//
  modeNormCost  = (Double)modeDist[SAO_Y]/m_labmda[SAO_Y];
  modeNormCost += (Double)(modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda; 
  m_pcRDGoOnSbacCoder->load(cabacCoderRDO[inCabacLabel]);
  m_pcRDGoOnSbacCoder->resetBits();
  m_pcRDGoOnSbacCoder->codeSAOBlkParam(modeParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);
  modeNormCost += (Double)m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();

}

Void TEncSampleAdaptiveOffset::deriveModeMergeRDO(Int ctu, std::vector<SAOBlkParam*>& mergeList, Bool* sliceEnabled, SAOStatData*** blkStats, SAOBlkParam& modeParam, Double& modeNormCost, TEncSbac** cabacCoderRDO, Int inCabacLabel)
{
  Int mergeListSize = (Int)mergeList.size();
  modeNormCost = MAX_DOUBLE;

  Double cost;
  SAOBlkParam testBlkParam;

  for(Int mergeType=0; mergeType< mergeListSize; mergeType++)
  {
    if(mergeList[mergeType] == NULL)
    {
      continue;
    }

    testBlkParam = *(mergeList[mergeType]);
    //normalized distortion
    Double normDist=0;
    for(Int compIdx=0; compIdx< NUM_SAO_COMPONENTS; compIdx++)
    {
      testBlkParam[compIdx].modeIdc = SAO_MODE_MERGE;
      testBlkParam[compIdx].typeIdc = mergeType;

      SAOOffset& mergedOffsetParam = (*(mergeList[mergeType]))[compIdx];

      if( mergedOffsetParam.modeIdc != SAO_MODE_OFF)
      {
        //offsets have been reconstructed. Don't call inversed quantization function.
        normDist += (((Double)getDistortion(ctu, compIdx, mergedOffsetParam.typeIdc, mergedOffsetParam.typeAuxInfo, mergedOffsetParam.offset, blkStats[ctu][compIdx][mergedOffsetParam.typeIdc]))
                       /m_labmda[compIdx]
                    );
      }

    }

    //rate
    m_pcRDGoOnSbacCoder->load(cabacCoderRDO[inCabacLabel]);
    m_pcRDGoOnSbacCoder->resetBits();
    m_pcRDGoOnSbacCoder->codeSAOBlkParam(testBlkParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);
    Int rate = m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();

    cost = normDist+(Double)rate;

    if(cost < modeNormCost)
    {
      modeNormCost = cost;
      modeParam    = testBlkParam;
      m_pcRDGoOnSbacCoder->store(cabacCoderRDO[SAO_CABACSTATE_BLK_TEMP]);
    }
  }

  m_pcRDGoOnSbacCoder->load(cabacCoderRDO[SAO_CABACSTATE_BLK_TEMP]);


}

Void TEncSampleAdaptiveOffset::decideBlkParams(TComPic* pic, Bool* sliceEnabled, SAOStatData*** blkStats, TComPicYuv* srcYuv, TComPicYuv* resYuv, SAOBlkParam* reconParams, SAOBlkParam* codedParams)
{
  Bool isAllBlksDisabled = false;
  if(!sliceEnabled[SAO_Y] && !sliceEnabled[SAO_Cb] && !sliceEnabled[SAO_Cr])
  {
    isAllBlksDisabled = true;
  }

  m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[ SAO_CABACSTATE_PIC_INIT ]);

  SAOBlkParam modeParam;
  Double minCost, modeCost;

  for(Int ctu=0; ctu< m_numCTUsPic; ctu++)
  {
    if(isAllBlksDisabled)
    {
      codedParams[ctu].reset();
      continue;
    }

    m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[ SAO_CABACSTATE_BLK_CUR ]);

    //get merge list
    std::vector<SAOBlkParam*> mergeList;
    getMergeList(pic, ctu, reconParams, mergeList);

    minCost = MAX_DOUBLE;
    for(Int mode=0; mode < NUM_SAO_MODES; mode++)
    {
      switch(mode)
      {
      case SAO_MODE_OFF:
        {
          continue; //not necessary, since all-off case will be tested in SAO_MODE_NEW case.
        }
        break;
      case SAO_MODE_NEW:
        {
          deriveModeNewRDO(ctu, mergeList, sliceEnabled, blkStats, modeParam, modeCost, m_pppcRDSbacCoder, SAO_CABACSTATE_BLK_CUR);

        }
        break;
      case SAO_MODE_MERGE:
        {
          deriveModeMergeRDO(ctu, mergeList, sliceEnabled, blkStats , modeParam, modeCost, m_pppcRDSbacCoder, SAO_CABACSTATE_BLK_CUR);
        }
        break;
      default:
        {
          printf("Not a supported SAO mode\n");
          assert(0);
          exit(-1);
        }
      }

      if(modeCost < minCost)
      {
        minCost = modeCost;
        codedParams[ctu] = modeParam;
        m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[ SAO_CABACSTATE_BLK_NEXT ]);

      }
    } //mode
    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[ SAO_CABACSTATE_BLK_NEXT ]);

    //apply reconstructed offsets
    reconParams[ctu] = codedParams[ctu];
    reconstructBlkSAOParam(reconParams[ctu], mergeList);
    offsetCTU(ctu, srcYuv, resYuv, reconParams[ctu], pic);
  } //ctu

#if SAO_ENCODING_CHOICE 
  Int picTempLayer = pic->getSlice(0)->getDepth();
  Int numLcusForSAOOff[NUM_SAO_COMPONENTS];
  numLcusForSAOOff[SAO_Y ] = numLcusForSAOOff[SAO_Cb]= numLcusForSAOOff[SAO_Cr]= 0;

  for (Int compIdx=0; compIdx<NUM_SAO_COMPONENTS; compIdx++)
  {
    for(Int ctu=0; ctu< m_numCTUsPic; ctu++)
    {
      if( reconParams[ctu][compIdx].modeIdc == SAO_MODE_OFF)
      {
        numLcusForSAOOff[compIdx]++;
      }
    }
  }
#if SAO_ENCODING_CHOICE_CHROMA
  for (Int compIdx=0; compIdx<NUM_SAO_COMPONENTS; compIdx++)
  {
    m_saoDisabledRate[compIdx][picTempLayer] = (Double)numLcusForSAOOff[compIdx]/(Double)m_numCTUsPic;
  }
#else
  if (picTempLayer == 0)
  {
    m_saoDisabledRate[SAO_Y][0] = (Double)(numLcusForSAOOff[SAO_Y]+numLcusForSAOOff[SAO_Cb]+numLcusForSAOOff[SAO_Cr])/(Double)(m_numCTUsPic*3);
  }
#endif                                              
#endif
}


Void TEncSampleAdaptiveOffset::getBlkStats(Int compIdx, SAOStatData* statsDataTypes  
                        , Pel* srcBlk, Pel* orgBlk, Int srcStride, Int orgStride, Int width, Int height
                        , Bool isLeftAvail,  Bool isRightAvail, Bool isAboveAvail, Bool isBelowAvail, Bool isAboveLeftAvail, Bool isAboveRightAvail, Bool isBelowLeftAvail, Bool isBelowRightAvail
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
                        , Bool isCalculatePreDeblockSamples
#endif
                        )
{
  if(m_lineBufWidth != m_maxCUWidth)
  {
    m_lineBufWidth = m_maxCUWidth;

    if (m_signLineBuf1) delete[] m_signLineBuf1; m_signLineBuf1 = NULL;
    m_signLineBuf1 = new Char[m_lineBufWidth+1]; 

    if (m_signLineBuf2) delete[] m_signLineBuf2; m_signLineBuf2 = NULL;
    m_signLineBuf2 = new Char[m_lineBufWidth+1];
  }

  Int x,y, startX, startY, endX, endY, edgeType, firstLineStartX, firstLineEndX;
  Char signLeft, signRight, signDown;
  Int64 *diff, *count;
  Pel *srcLine, *orgLine;
  Int* skipLinesR = m_skipLinesR[compIdx];
  Int* skipLinesB = m_skipLinesB[compIdx];

  for(Int typeIdx=0; typeIdx< NUM_SAO_NEW_TYPES; typeIdx++)
  {
    SAOStatData& statsData= statsDataTypes[typeIdx];
    statsData.reset();

    srcLine = srcBlk;
    orgLine = orgBlk;
    diff    = statsData.diff;
    count   = statsData.count;
    switch(typeIdx)
    {
    case SAO_TYPE_EO_0:
      {
        diff +=2;
        count+=2;
        endY   = (isBelowAvail) ? (height - skipLinesB[typeIdx]) : height;
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        startX = (!isCalculatePreDeblockSamples) ? (isLeftAvail  ? 0 : 1)
                                                 : (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                                 ;
#else
        startX = isLeftAvail ? 0 : 1;
#endif
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        endX   = (!isCalculatePreDeblockSamples) ? (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                                 : (isRightAvail ? width : (width - 1))
                                                 ;
#else
        endX   = isRightAvail ? (width - skipLinesR[typeIdx]): (width - 1);
#endif
        for (y=0; y<endY; y++)
        {
          signLeft = (Char)m_sign[srcLine[startX] - srcLine[startX-1]];
          for (x=startX; x<endX; x++)
          {
            signRight =  (Char)m_sign[srcLine[x] - srcLine[x+1]]; 
            edgeType  =  signRight + signLeft;
            signLeft  = -signRight;

            diff [edgeType] += (orgLine[x] - srcLine[x]);
            count[edgeType] ++;
          }
          srcLine  += srcStride;
          orgLine  += orgStride;
        }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        if(isCalculatePreDeblockSamples)
        {
          if(isBelowAvail)
          {
            startX = isLeftAvail  ? 0 : 1;
            endX   = isRightAvail ? width : (width -1);

            for(y=0; y<skipLinesB[typeIdx]; y++)
            {
              signLeft = (Char)m_sign[srcLine[startX] - srcLine[startX-1]];
              for (x=startX; x<endX; x++)
              {
                signRight =  (Char)m_sign[srcLine[x] - srcLine[x+1]]; 
                edgeType  =  signRight + signLeft;
                signLeft  = -signRight;

                diff [edgeType] += (orgLine[x] - srcLine[x]);
                count[edgeType] ++;
              }
              srcLine  += srcStride;
              orgLine  += orgStride;
            }
          }
        }
#endif
      }
      break;
    case SAO_TYPE_EO_90:
      {
        diff +=2;
        count+=2;
        Char *signUpLine = m_signLineBuf1;

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        startX = (!isCalculatePreDeblockSamples) ? 0
                                                 : (isRightAvail ? (width - skipLinesR[typeIdx]) : width)
                                                 ;
#endif
        startY = isAboveAvail ? 0 : 1;
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        endX   = (!isCalculatePreDeblockSamples) ? (isRightAvail ? (width - skipLinesR[typeIdx]) : width) 
                                                 : width
                                                 ;
#else
        endX   = isRightAvail ? (width - skipLinesR[typeIdx]) : width ;
#endif
        endY   = isBelowAvail ? (height - skipLinesB[typeIdx]) : (height - 1);
        if (!isAboveAvail)
        {
          srcLine += srcStride;
          orgLine += orgStride;
        }

        Pel* srcLineAbove = srcLine - srcStride;
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        for (x=startX; x<endX; x++) 
#else
        for (x=0; x< endX; x++) 
#endif
        {
          signUpLine[x] = (Char)m_sign[srcLine[x] - srcLineAbove[x]];
        }

        Pel* srcLineBelow;
        for (y=startY; y<endY; y++)
        {
          srcLineBelow = srcLine + srcStride;

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
          for (x=startX; x<endX; x++)
#else
          for (x=0; x<endX; x++)
#endif
          {
            signDown  = (Char)m_sign[srcLine[x] - srcLineBelow[x]]; 
            edgeType  = signDown + signUpLine[x];
            signUpLine[x]= -signDown;

            diff [edgeType] += (orgLine[x] - srcLine[x]);
            count[edgeType] ++;
          }
          srcLine += srcStride;
          orgLine += orgStride;
        }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        if(isCalculatePreDeblockSamples)
        {
          if(isBelowAvail)
          {
            startX = 0;
            endX   = width;

            for(y=0; y<skipLinesB[typeIdx]; y++)
            {
              srcLineBelow = srcLine + srcStride;
              srcLineAbove = srcLine - srcStride;

              for (x=startX; x<endX; x++)
              {
                edgeType = m_sign[srcLine[x] - srcLineBelow[x]] + m_sign[srcLine[x] - srcLineAbove[x]];
                diff [edgeType] += (orgLine[x] - srcLine[x]);
                count[edgeType] ++;
              }
              srcLine  += srcStride;
              orgLine  += orgStride;
            }
          }
        }
#endif

      }
      break;
    case SAO_TYPE_EO_135:
      {
        diff +=2;
        count+=2;
        Char *signUpLine, *signDownLine, *signTmpLine;

        signUpLine  = m_signLineBuf1;
        signDownLine= m_signLineBuf2;

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        startX = (!isCalculatePreDeblockSamples) ? (isLeftAvail  ? 0 : 1)
                                                 : (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                                 ;
#else
        startX = isLeftAvail ? 0 : 1 ;
#endif

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        endX   = (!isCalculatePreDeblockSamples) ? (isRightAvail ? (width - skipLinesR[typeIdx]): (width - 1))
                                                 : (isRightAvail ? width : (width - 1))
                                                 ;
#else
        endX   = isRightAvail ? (width - skipLinesR[typeIdx]): (width - 1);
#endif
        endY   = isBelowAvail ? (height - skipLinesB[typeIdx]) : (height - 1);

        //prepare 2nd line's upper sign
        Pel* srcLineBelow = srcLine + srcStride;
        for (x=startX; x<endX+1; x++)
        {
          signUpLine[x] = (Char)m_sign[srcLineBelow[x] - srcLine[x-1]];
        }

        //1st line
        Pel* srcLineAbove = srcLine - srcStride;
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        firstLineStartX = (!isCalculatePreDeblockSamples) ? (isAboveLeftAvail ? 0    : 1) : startX;
        firstLineEndX   = (!isCalculatePreDeblockSamples) ? (isAboveAvail     ? endX : 1) : endX;
#else
        firstLineStartX = isAboveLeftAvail ? 0    : 1;
        firstLineEndX   = isAboveAvail     ? endX : 1;
#endif
        for(x=firstLineStartX; x<firstLineEndX; x++)
        {
          edgeType = m_sign[srcLine[x] - srcLineAbove[x-1]] - signUpLine[x+1];
          diff [edgeType] += (orgLine[x] - srcLine[x]);
          count[edgeType] ++;
        }
        srcLine  += srcStride;
        orgLine  += orgStride;


        //middle lines
        for (y=1; y<endY; y++)
        {
          srcLineBelow = srcLine + srcStride;

          for (x=startX; x<endX; x++)
          {
            signDown = (Char)m_sign[srcLine[x] - srcLineBelow[x+1]] ;
            edgeType = signDown + signUpLine[x];
            diff [edgeType] += (orgLine[x] - srcLine[x]);
            count[edgeType] ++;

            signDownLine[x+1] = -signDown; 
          }
          signDownLine[startX] = (Char)m_sign[srcLineBelow[startX] - srcLine[startX-1]];

          signTmpLine  = signUpLine;
          signUpLine   = signDownLine;
          signDownLine = signTmpLine;

          srcLine += srcStride;
          orgLine += orgStride;
        }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        if(isCalculatePreDeblockSamples)
        {
          if(isBelowAvail)
          {
            startX = isLeftAvail  ? 0     : 1 ;
            endX   = isRightAvail ? width : (width -1);

            for(y=0; y<skipLinesB[typeIdx]; y++)
            {
              srcLineBelow = srcLine + srcStride;
              srcLineAbove = srcLine - srcStride;

              for (x=startX; x< endX; x++)
              {
                edgeType = m_sign[srcLine[x] - srcLineBelow[x+1]] + m_sign[srcLine[x] - srcLineAbove[x-1]];
                diff [edgeType] += (orgLine[x] - srcLine[x]);
                count[edgeType] ++;
              }
              srcLine  += srcStride;
              orgLine  += orgStride;
            }
          }
        }
#endif
      }
      break;
    case SAO_TYPE_EO_45:
      {
        diff +=2;
        count+=2;
        Char *signUpLine = m_signLineBuf1+1;

#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        startX = (!isCalculatePreDeblockSamples) ? (isLeftAvail  ? 0 : 1)
                                                 : (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                                 ;
#else
        startX = isLeftAvail ? 0 : 1;
#endif
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        endX   = (!isCalculatePreDeblockSamples) ? (isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1))
                                                 : (isRightAvail ? width : (width - 1))
                                                 ;
#else
        endX   = isRightAvail ? (width - skipLinesR[typeIdx]) : (width - 1);
#endif
        endY   = isBelowAvail ? (height - skipLinesB[typeIdx]) : (height - 1);

        //prepare 2nd line upper sign
        Pel* srcLineBelow = srcLine + srcStride;
        for (x=startX-1; x<endX; x++)
        {
          signUpLine[x] = (Char)m_sign[srcLineBelow[x] - srcLine[x+1]];
        }


        //first line
        Pel* srcLineAbove = srcLine - srcStride;
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        firstLineStartX = (!isCalculatePreDeblockSamples) ? (isAboveAvail ? startX : endX)
                                                          : startX
                                                          ;
        firstLineEndX   = (!isCalculatePreDeblockSamples) ? ((!isRightAvail && isAboveRightAvail) ? width : endX)
                                                          : endX
                                                          ;
#else
        firstLineStartX = isAboveAvail ? startX : endX;
        firstLineEndX   = (!isRightAvail && isAboveRightAvail) ? width : endX;
#endif
        for(x=firstLineStartX; x<firstLineEndX; x++)
        {
          edgeType = m_sign[srcLine[x] - srcLineAbove[x+1]] - signUpLine[x-1];
          diff [edgeType] += (orgLine[x] - srcLine[x]);
          count[edgeType] ++;
        }

        srcLine += srcStride;
        orgLine += orgStride;

        //middle lines
        for (y=1; y<endY; y++)
        {
          srcLineBelow = srcLine + srcStride;

          for(x=startX; x<endX; x++)
          {
            signDown = (Char)m_sign[srcLine[x] - srcLineBelow[x-1]] ;
            edgeType = signDown + signUpLine[x];

            diff [edgeType] += (orgLine[x] - srcLine[x]);
            count[edgeType] ++;

            signUpLine[x-1] = -signDown; 
          }
          signUpLine[endX-1] = (Char)m_sign[srcLineBelow[endX-1] - srcLine[endX]];
          srcLine  += srcStride;
          orgLine  += orgStride;
        }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        if(isCalculatePreDeblockSamples)
        {
          if(isBelowAvail)
          {
            startX = isLeftAvail  ? 0     : 1 ;
            endX   = isRightAvail ? width : (width -1);

            for(y=0; y<skipLinesB[typeIdx]; y++)
            {
              srcLineBelow = srcLine + srcStride;
              srcLineAbove = srcLine - srcStride;

              for (x=startX; x<endX; x++)
              {
                edgeType = m_sign[srcLine[x] - srcLineBelow[x-1]] + m_sign[srcLine[x] - srcLineAbove[x+1]];
                diff [edgeType] += (orgLine[x] - srcLine[x]);
                count[edgeType] ++;
              }
              srcLine  += srcStride;
              orgLine  += orgStride;
            }
          }
        }
#endif
      }
      break;
    case SAO_TYPE_BO:
      {
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        startX = (!isCalculatePreDeblockSamples)?0
                                                :( isRightAvail?(width- skipLinesR[typeIdx]):width)
                                                ;
        endX   = (!isCalculatePreDeblockSamples)?(isRightAvail ? (width - skipLinesR[typeIdx]) : width )
                                                :width
                                                ;
#else
        endX = isRightAvail ? (width- skipLinesR[typeIdx]) : width;
#endif
        endY = isBelowAvail ? (height- skipLinesB[typeIdx]) : height;
        Int shiftBits = ((compIdx == SAO_Y)?g_bitDepthY:g_bitDepthC)- NUM_SAO_BO_CLASSES_LOG2;
        for (y=0; y< endY; y++)
        {
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
          for (x=startX; x< endX; x++)
#else
          for (x=0; x< endX; x++)
#endif
          {

            Int bandIdx= srcLine[x] >> shiftBits; 
            diff [bandIdx] += (orgLine[x] - srcLine[x]);
            count[bandIdx] ++;
          }
          srcLine += srcStride;
          orgLine += orgStride;
        }
#if SAO_ENCODE_ALLOW_USE_PREDEBLOCK
        if(isCalculatePreDeblockSamples)
        {
          if(isBelowAvail)
          {
            startX = 0;
            endX   = width;

            for(y= 0; y< skipLinesB[typeIdx]; y++)
            {
              for (x=startX; x< endX; x++)
              {
                Int bandIdx= srcLine[x] >> shiftBits; 
                diff [bandIdx] += (orgLine[x] - srcLine[x]);
                count[bandIdx] ++;
              }
              srcLine  += srcStride;
              orgLine  += orgStride;

            }

          }
        }
#endif
      }
      break;
    default:
      {
        printf("Not a supported SAO types\n");
        assert(0);
        exit(-1);
      }
    }
  }
}

#else



TEncSampleAdaptiveOffset::TEncSampleAdaptiveOffset()
{
  m_pcEntropyCoder = NULL;
  m_pppcRDSbacCoder = NULL;
  m_pcRDGoOnSbacCoder = NULL;
  m_pppcBinCoderCABAC = NULL;            
  m_iCount = NULL;     
  m_iOffset = NULL;      
  m_iOffsetOrg = NULL;  
  m_iRate = NULL;       
  m_iDist = NULL;        
  m_dCost = NULL;        
  m_dCostPartBest = NULL; 
  m_iDistOrg = NULL;      
  m_iTypePartBest = NULL; 
#if SAO_ENCODING_CHOICE_CHROMA
  m_depthSaoRate[0][0] = 0;
  m_depthSaoRate[0][1] = 0;
  m_depthSaoRate[0][2] = 0;
  m_depthSaoRate[0][3] = 0;
  m_depthSaoRate[1][0] = 0;
  m_depthSaoRate[1][1] = 0;
  m_depthSaoRate[1][2] = 0;
  m_depthSaoRate[1][3] = 0;
#endif
}
TEncSampleAdaptiveOffset::~TEncSampleAdaptiveOffset()
{

}
// ====================================================================================================================
// Constants
// ====================================================================================================================


// ====================================================================================================================
// Tables
// ====================================================================================================================

inline Double xRoundIbdi2(Int bitDepth, Double x)
{
  return ((x)>0) ? (Int)(((Int)(x)+(1<<(bitDepth-8-1)))/(1<<(bitDepth-8))) : ((Int)(((Int)(x)-(1<<(bitDepth-8-1)))/(1<<(bitDepth-8))));
}

/** rounding with IBDI
 * \param  x
 */
inline Double xRoundIbdi(Int bitDepth, Double x)
{
  return (bitDepth > 8 ? xRoundIbdi2(bitDepth, (x)) : ((x)>=0 ? ((Int)((x)+0.5)) : ((Int)((x)-0.5)))) ;
}



/** process SAO for one partition
 * \param  *psQTPart, iPartIdx, dLambda
 */
Void TEncSampleAdaptiveOffset::rdoSaoOnePart(SAOQTPart *psQTPart, Int iPartIdx, Double dLambda, Int yCbCr)
{
  Int iTypeIdx;
  Int iNumTotalType = MAX_NUM_SAO_TYPE;
  SAOQTPart*  pOnePart = &(psQTPart[iPartIdx]);

  Int64 iEstDist;
  Int iClassIdx;
  Int uiShift = 2 * DISTORTION_PRECISION_ADJUSTMENT((yCbCr == 0 ? g_bitDepthY : g_bitDepthC)-8);
  UInt uiDepth = pOnePart->PartLevel;

  m_iDistOrg [iPartIdx] =  0;

  Double  bestRDCostTableBo = MAX_DOUBLE;
  Int     bestClassTableBo    = 0;
  Int     currentDistortionTableBo[MAX_NUM_SAO_CLASS];
  Double  currentRdCostTableBo[MAX_NUM_SAO_CLASS];

  Int addr;
  Int allowMergeLeft;
  Int allowMergeUp;
  Int frameWidthInCU = m_pcPic->getFrameWidthInCU();
  SaoLcuParam  saoLcuParamRdo;

  for (iTypeIdx=-1; iTypeIdx<iNumTotalType; iTypeIdx++)
  {
    if( m_bUseSBACRD )
    {
      m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[uiDepth][CI_CURR_BEST]);
      m_pcRDGoOnSbacCoder->resetBits();
    }
    else
    {
      m_pcEntropyCoder->resetEntropy();
      m_pcEntropyCoder->resetBits();
    }

    iEstDist = 0;

    if (iTypeIdx == -1)
    {      
      for (Int ry = pOnePart->StartCUY; ry<= pOnePart->EndCUY; ry++)
      {
        for (Int rx = pOnePart->StartCUX; rx <= pOnePart->EndCUX; rx++)
        {
          addr = ry * frameWidthInCU + rx;          

          // get bits for iTypeIdx = -1
          allowMergeLeft = 1;
          allowMergeUp   = 1;
          if (rx != 0)
          { 
            // check tile id and slice id 
            if ( (m_pcPic->getPicSym()->getTileIdxMap(addr-1) != m_pcPic->getPicSym()->getTileIdxMap(addr)) || (m_pcPic->getCU(addr-1)->getSlice()->getSliceIdx() != m_pcPic->getCU(addr)->getSlice()->getSliceIdx()))
            {
              allowMergeLeft = 0;
            }
          }
          if (ry!=0)
          {
            if ( (m_pcPic->getPicSym()->getTileIdxMap(addr-m_iNumCuInWidth) != m_pcPic->getPicSym()->getTileIdxMap(addr)) || (m_pcPic->getCU(addr-m_iNumCuInWidth)->getSlice()->getSliceIdx() != m_pcPic->getCU(addr)->getSlice()->getSliceIdx()))
            {
              allowMergeUp = 0;
            }
          }

          // reset
          resetSaoUnit(&saoLcuParamRdo);

          // set merge flag
          saoLcuParamRdo.mergeUpFlag   = 1;
          saoLcuParamRdo.mergeLeftFlag = 1;

          if (ry == pOnePart->StartCUY) 
          {
            saoLcuParamRdo.mergeUpFlag = 0;
          } 
          
          if (rx == pOnePart->StartCUX) 
          {
            saoLcuParamRdo.mergeLeftFlag = 0;
          }

          m_pcEntropyCoder->encodeSaoUnitInterleaving(yCbCr, 1, rx, ry,  &saoLcuParamRdo, 1,  1,  allowMergeLeft, allowMergeUp);

        }
      }
    }

    if (iTypeIdx>=0)
    {
      iEstDist = estSaoTypeDist(iPartIdx, iTypeIdx, uiShift, dLambda, currentDistortionTableBo, currentRdCostTableBo);
      if( iTypeIdx == SAO_BO )
      {
        // Estimate Best Position
        Double currentRDCost = 0.0;

        for(Int i=0; i< SAO_MAX_BO_CLASSES -SAO_BO_LEN +1; i++)
        {
          currentRDCost = 0.0;
          for(UInt uj = i; uj < i+SAO_BO_LEN; uj++)
          {
            currentRDCost += currentRdCostTableBo[uj];
          }

          if( currentRDCost < bestRDCostTableBo)
          {
            bestRDCostTableBo = currentRDCost;
            bestClassTableBo  = i;
          }
        }

        // Re code all Offsets
        // Code Center
        for(iClassIdx = bestClassTableBo; iClassIdx < bestClassTableBo+SAO_BO_LEN; iClassIdx++)
        {
          iEstDist += currentDistortionTableBo[iClassIdx];
        }
      }

      for (Int ry = pOnePart->StartCUY; ry<= pOnePart->EndCUY; ry++)
      {
        for (Int rx = pOnePart->StartCUX; rx <= pOnePart->EndCUX; rx++)
        {
          addr = ry * frameWidthInCU + rx;          

          // get bits for iTypeIdx = -1
          allowMergeLeft = 1;
          allowMergeUp   = 1;
          if (rx != 0)
          { 
            // check tile id and slice id 
            if ( (m_pcPic->getPicSym()->getTileIdxMap(addr-1) != m_pcPic->getPicSym()->getTileIdxMap(addr)) || (m_pcPic->getCU(addr-1)->getSlice()->getSliceIdx() != m_pcPic->getCU(addr)->getSlice()->getSliceIdx()))
            {
              allowMergeLeft = 0;
            }
          }
          if (ry!=0)
          {
            if ( (m_pcPic->getPicSym()->getTileIdxMap(addr-m_iNumCuInWidth) != m_pcPic->getPicSym()->getTileIdxMap(addr)) || (m_pcPic->getCU(addr-m_iNumCuInWidth)->getSlice()->getSliceIdx() != m_pcPic->getCU(addr)->getSlice()->getSliceIdx()))
            {
              allowMergeUp = 0;
            }
          }

          // reset
          resetSaoUnit(&saoLcuParamRdo);
          
          // set merge flag
          saoLcuParamRdo.mergeUpFlag   = 1;
          saoLcuParamRdo.mergeLeftFlag = 1;

          if (ry == pOnePart->StartCUY) 
          {
            saoLcuParamRdo.mergeUpFlag = 0;
          } 
          
          if (rx == pOnePart->StartCUX) 
          {
            saoLcuParamRdo.mergeLeftFlag = 0;
          }

          // set type and offsets
          saoLcuParamRdo.typeIdx = iTypeIdx;
          saoLcuParamRdo.subTypeIdx = (iTypeIdx==SAO_BO)?bestClassTableBo:0;
          saoLcuParamRdo.length = m_iNumClass[iTypeIdx];
          for (iClassIdx = 0; iClassIdx < saoLcuParamRdo.length; iClassIdx++)
          {
            saoLcuParamRdo.offset[iClassIdx] = (Int)m_iOffset[iPartIdx][iTypeIdx][iClassIdx+saoLcuParamRdo.subTypeIdx+1];
          }

          m_pcEntropyCoder->encodeSaoUnitInterleaving(yCbCr, 1, rx, ry,  &saoLcuParamRdo, 1,  1,  allowMergeLeft, allowMergeUp);

        }
      }

      m_iDist[iPartIdx][iTypeIdx] = iEstDist;
      m_iRate[iPartIdx][iTypeIdx] = m_pcEntropyCoder->getNumberOfWrittenBits();

      m_dCost[iPartIdx][iTypeIdx] = (Double)((Double)m_iDist[iPartIdx][iTypeIdx] + dLambda * (Double) m_iRate[iPartIdx][iTypeIdx]);

      if(m_dCost[iPartIdx][iTypeIdx] < m_dCostPartBest[iPartIdx])
      {
        m_iDistOrg [iPartIdx] = 0;
        m_dCostPartBest[iPartIdx] = m_dCost[iPartIdx][iTypeIdx];
        m_iTypePartBest[iPartIdx] = iTypeIdx;
        if( m_bUseSBACRD )
          m_pcRDGoOnSbacCoder->store( m_pppcRDSbacCoder[pOnePart->PartLevel][CI_TEMP_BEST] );
      }
    }
    else
    {
      if(m_iDistOrg[iPartIdx] < m_dCostPartBest[iPartIdx] )
      {
        m_dCostPartBest[iPartIdx] = (Double) m_iDistOrg[iPartIdx] + m_pcEntropyCoder->getNumberOfWrittenBits()*dLambda ; 
        m_iTypePartBest[iPartIdx] = -1;
        if( m_bUseSBACRD )
        {
          m_pcRDGoOnSbacCoder->store( m_pppcRDSbacCoder[pOnePart->PartLevel][CI_TEMP_BEST] );
        }
      }
    }
  }

  pOnePart->bProcessed = true;
  pOnePart->bSplit     = false;
  pOnePart->iMinDist   =        m_iTypePartBest[iPartIdx] >= 0 ? m_iDist[iPartIdx][m_iTypePartBest[iPartIdx]] : m_iDistOrg[iPartIdx];
  pOnePart->iMinRate   = (Int) (m_iTypePartBest[iPartIdx] >= 0 ? m_iRate[iPartIdx][m_iTypePartBest[iPartIdx]] : 0);
  pOnePart->dMinCost   = pOnePart->iMinDist + dLambda * pOnePart->iMinRate;
  pOnePart->iBestType  = m_iTypePartBest[iPartIdx];
  if (pOnePart->iBestType != -1)
  {
    //     pOnePart->bEnableFlag =  1;
    pOnePart->iLength = m_iNumClass[pOnePart->iBestType];
    Int minIndex = 0;
    if( pOnePart->iBestType == SAO_BO )
    {
      pOnePart->subTypeIdx = bestClassTableBo;
      minIndex = pOnePart->subTypeIdx;
    }
    for (Int i=0; i< pOnePart->iLength ; i++)
    {
      pOnePart->iOffset[i] = (Int) m_iOffset[iPartIdx][pOnePart->iBestType][minIndex+i+1];
    }

  }
  else
  {
    //     pOnePart->bEnableFlag = 0;
    pOnePart->iLength     = 0;
  }
}

/** Run partition tree disable
 */
Void TEncSampleAdaptiveOffset::disablePartTree(SAOQTPart *psQTPart, Int iPartIdx)
{
  SAOQTPart*  pOnePart= &(psQTPart[iPartIdx]);
  pOnePart->bSplit      = false;
  pOnePart->iLength     =  0;
  pOnePart->iBestType   = -1;

  if (pOnePart->PartLevel < m_uiMaxSplitLevel)
  {
    for (Int i=0; i<NUM_DOWN_PART; i++)
    {
      disablePartTree(psQTPart, pOnePart->DownPartsIdx[i]);
    }
  }
}

/** Run quadtree decision function
 * \param  iPartIdx, pcPicOrg, pcPicDec, pcPicRest, &dCostFinal
 */
Void TEncSampleAdaptiveOffset::runQuadTreeDecision(SAOQTPart *psQTPart, Int iPartIdx, Double &dCostFinal, Int iMaxLevel, Double dLambda, Int yCbCr)
{
  SAOQTPart*  pOnePart = &(psQTPart[iPartIdx]);

  UInt uiDepth = pOnePart->PartLevel;
  UInt uhNextDepth = uiDepth+1;

  if (iPartIdx == 0)
  {
    dCostFinal = 0;
  }

  //SAO for this part
  if(!pOnePart->bProcessed)
  {
    rdoSaoOnePart (psQTPart, iPartIdx, dLambda, yCbCr);
  }

  //SAO for sub 4 parts
  if (pOnePart->PartLevel < iMaxLevel)
  {
    Double      dCostNotSplit = dLambda + pOnePart->dMinCost;
    Double      dCostSplit    = dLambda;

    for (Int i=0; i< NUM_DOWN_PART ;i++)
    {
      if( m_bUseSBACRD )  
      {
        if ( 0 == i) //initialize RD with previous depth buffer
        {
          m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uiDepth][CI_CURR_BEST]);
        }
        else
        {
          m_pppcRDSbacCoder[uhNextDepth][CI_CURR_BEST]->load(m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]);
        }
      }  
      runQuadTreeDecision(psQTPart, pOnePart->DownPartsIdx[i], dCostFinal, iMaxLevel, dLambda, yCbCr);
      dCostSplit += dCostFinal;
      if( m_bUseSBACRD )
      {
        m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]->load(m_pppcRDSbacCoder[uhNextDepth][CI_TEMP_BEST]);
      }
    }

    if(dCostSplit < dCostNotSplit)
    {
      dCostFinal = dCostSplit;
      pOnePart->bSplit      = true;
      pOnePart->iLength     =  0;
      pOnePart->iBestType   = -1;
      if( m_bUseSBACRD )
      {
        m_pppcRDSbacCoder[uiDepth][CI_NEXT_BEST]->load(m_pppcRDSbacCoder[uhNextDepth][CI_NEXT_BEST]);
      }
    }
    else
    {
      dCostFinal = dCostNotSplit;
      pOnePart->bSplit = false;
      for (Int i=0; i<NUM_DOWN_PART; i++)
      {
        disablePartTree(psQTPart, pOnePart->DownPartsIdx[i]);
      }
      if( m_bUseSBACRD )
      {
        m_pppcRDSbacCoder[uiDepth][CI_NEXT_BEST]->load(m_pppcRDSbacCoder[uiDepth][CI_TEMP_BEST]);
      }
    }
  }
  else
  {
    dCostFinal = pOnePart->dMinCost;
  }
}

/** delete allocated memory of TEncSampleAdaptiveOffset class.
 */
Void TEncSampleAdaptiveOffset::destroyEncBuffer()
{
  for (Int i=0;i<m_iNumTotalParts;i++)
  {
    for (Int j=0;j<MAX_NUM_SAO_TYPE;j++)
    {
      if (m_iCount [i][j])
      {
        delete [] m_iCount [i][j]; 
      }
      if (m_iOffset[i][j])
      {
        delete [] m_iOffset[i][j]; 
      }
      if (m_iOffsetOrg[i][j])
      {
        delete [] m_iOffsetOrg[i][j]; 
      }
    }
    if (m_iRate[i])
    {
      delete [] m_iRate[i];
    }
    if (m_iDist[i])
    {
      delete [] m_iDist[i]; 
    }
    if (m_dCost[i])
    {
      delete [] m_dCost[i]; 
    }
    if (m_iCount [i])
    {
      delete [] m_iCount [i]; 
    }
    if (m_iOffset[i])
    {
      delete [] m_iOffset[i]; 
    }
    if (m_iOffsetOrg[i])
    {
      delete [] m_iOffsetOrg[i]; 
    }

  }
  if (m_iDistOrg)
  {
    delete [] m_iDistOrg ; m_iDistOrg = NULL;
  }
  if (m_dCostPartBest)
  {
    delete [] m_dCostPartBest ; m_dCostPartBest = NULL;
  }
  if (m_iTypePartBest)
  {
    delete [] m_iTypePartBest ; m_iTypePartBest = NULL;
  }
  if (m_iRate)
  {
    delete [] m_iRate ; m_iRate = NULL;
  }
  if (m_iDist)
  {
    delete [] m_iDist ; m_iDist = NULL;
  }
  if (m_dCost)
  {
    delete [] m_dCost ; m_dCost = NULL;
  }
  if (m_iCount)
  {
    delete [] m_iCount  ; m_iCount = NULL;
  }
  if (m_iOffset)
  {
    delete [] m_iOffset ; m_iOffset = NULL;
  }
  if (m_iOffsetOrg)
  {
    delete [] m_iOffsetOrg ; m_iOffsetOrg = NULL;
  }
  Int numLcu = m_iNumCuInWidth * m_iNumCuInHeight;

  for (Int i=0;i<numLcu;i++)
  {
    for (Int j=0;j<3;j++)
    {
      for (Int k=0;k<MAX_NUM_SAO_TYPE;k++)
      {
        if (m_count_PreDblk [i][j][k])
        {
          delete [] m_count_PreDblk [i][j][k]; 
        }
        if (m_offsetOrg_PreDblk[i][j][k])
        {
          delete [] m_offsetOrg_PreDblk[i][j][k];
        }
      }
      if (m_count_PreDblk [i][j])
      {
        delete [] m_count_PreDblk [i][j]; 
      }
      if (m_offsetOrg_PreDblk[i][j])
      {
        delete [] m_offsetOrg_PreDblk[i][j]; 
      }
    }
    if (m_count_PreDblk [i])
    {
      delete [] m_count_PreDblk [i]; 
    }
    if (m_offsetOrg_PreDblk[i])
    {
      delete [] m_offsetOrg_PreDblk[i]; 
    }
  }
  if (m_count_PreDblk)
  {
    delete [] m_count_PreDblk  ; m_count_PreDblk = NULL;
  }
  if (m_offsetOrg_PreDblk)
  {
    delete [] m_offsetOrg_PreDblk ; m_offsetOrg_PreDblk = NULL;
  }

  Int iMaxDepth = 4;
  Int iDepth;
  for ( iDepth = 0; iDepth < iMaxDepth+1; iDepth++ )
  {
    for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
    {
      delete m_pppcRDSbacCoder[iDepth][iCIIdx];
      delete m_pppcBinCoderCABAC[iDepth][iCIIdx];
    }
  }

  for ( iDepth = 0; iDepth < iMaxDepth+1; iDepth++ )
  {
    delete [] m_pppcRDSbacCoder[iDepth];
    delete [] m_pppcBinCoderCABAC[iDepth];
  }

  delete [] m_pppcRDSbacCoder;
  delete [] m_pppcBinCoderCABAC;
}

/** create Encoder Buffer for SAO
 * \param 
 */
Void TEncSampleAdaptiveOffset::createEncBuffer()
{
  m_iDistOrg = new Int64 [m_iNumTotalParts]; 
  m_dCostPartBest = new Double [m_iNumTotalParts]; 
  m_iTypePartBest = new Int [m_iNumTotalParts]; 

  m_iRate = new Int64* [m_iNumTotalParts];
  m_iDist = new Int64* [m_iNumTotalParts];
  m_dCost = new Double*[m_iNumTotalParts];

  m_iCount  = new Int64 **[m_iNumTotalParts];
  m_iOffset = new Int64 **[m_iNumTotalParts];
  m_iOffsetOrg = new Int64 **[m_iNumTotalParts];

  for (Int i=0;i<m_iNumTotalParts;i++)
  {
    m_iRate[i] = new Int64  [MAX_NUM_SAO_TYPE];
    m_iDist[i] = new Int64  [MAX_NUM_SAO_TYPE]; 
    m_dCost[i] = new Double [MAX_NUM_SAO_TYPE]; 

    m_iCount [i] = new Int64 *[MAX_NUM_SAO_TYPE]; 
    m_iOffset[i] = new Int64 *[MAX_NUM_SAO_TYPE]; 
    m_iOffsetOrg[i] = new Int64 *[MAX_NUM_SAO_TYPE]; 

    for (Int j=0;j<MAX_NUM_SAO_TYPE;j++)
    {
      m_iCount [i][j]   = new Int64 [MAX_NUM_SAO_CLASS]; 
      m_iOffset[i][j]   = new Int64 [MAX_NUM_SAO_CLASS]; 
      m_iOffsetOrg[i][j]= new Int64 [MAX_NUM_SAO_CLASS]; 
    }
  }
  Int numLcu = m_iNumCuInWidth * m_iNumCuInHeight;
  m_count_PreDblk  = new Int64 ***[numLcu];
  m_offsetOrg_PreDblk = new Int64 ***[numLcu];
  for (Int i=0; i<numLcu; i++)
  {
    m_count_PreDblk[i]  = new Int64 **[3];
    m_offsetOrg_PreDblk[i] = new Int64 **[3];

    for (Int j=0;j<3;j++)
    {
      m_count_PreDblk [i][j] = new Int64 *[MAX_NUM_SAO_TYPE]; 
      m_offsetOrg_PreDblk[i][j] = new Int64 *[MAX_NUM_SAO_TYPE]; 

      for (Int k=0;k<MAX_NUM_SAO_TYPE;k++)
      {
        m_count_PreDblk [i][j][k]   = new Int64 [MAX_NUM_SAO_CLASS]; 
        m_offsetOrg_PreDblk[i][j][k]= new Int64 [MAX_NUM_SAO_CLASS]; 
      }
    }
  }

  Int iMaxDepth = 4;
  m_pppcRDSbacCoder = new TEncSbac** [iMaxDepth+1];
#if FAST_BIT_EST
  m_pppcBinCoderCABAC = new TEncBinCABACCounter** [iMaxDepth+1];
#else
  m_pppcBinCoderCABAC = new TEncBinCABAC** [iMaxDepth+1];
#endif

  for ( Int iDepth = 0; iDepth < iMaxDepth+1; iDepth++ )
  {
    m_pppcRDSbacCoder[iDepth] = new TEncSbac* [CI_NUM];
#if FAST_BIT_EST
    m_pppcBinCoderCABAC[iDepth] = new TEncBinCABACCounter* [CI_NUM];
#else
    m_pppcBinCoderCABAC[iDepth] = new TEncBinCABAC* [CI_NUM];
#endif
    for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
    {
      m_pppcRDSbacCoder[iDepth][iCIIdx] = new TEncSbac;
#if FAST_BIT_EST
      m_pppcBinCoderCABAC [iDepth][iCIIdx] = new TEncBinCABACCounter;
#else
      m_pppcBinCoderCABAC [iDepth][iCIIdx] = new TEncBinCABAC;
#endif
      m_pppcRDSbacCoder   [iDepth][iCIIdx]->init( m_pppcBinCoderCABAC [iDepth][iCIIdx] );
    }
  }
}

/** Start SAO encoder
 * \param pcPic, pcEntropyCoder, pppcRDSbacCoder, pcRDGoOnSbacCoder 
 */
Void TEncSampleAdaptiveOffset::startSaoEnc( TComPic* pcPic, TEncEntropy* pcEntropyCoder, TEncSbac*** pppcRDSbacCoder, TEncSbac* pcRDGoOnSbacCoder)
{
    m_bUseSBACRD = true;
  m_pcPic = pcPic;
  m_pcEntropyCoder = pcEntropyCoder;

  m_pcRDGoOnSbacCoder = pcRDGoOnSbacCoder;
  m_pcEntropyCoder->setEntropyCoder(m_pcRDGoOnSbacCoder, pcPic->getSlice(0));
  m_pcEntropyCoder->resetEntropy();
  m_pcEntropyCoder->resetBits();

  if( m_bUseSBACRD )
  {
    m_pcRDGoOnSbacCoder->store( m_pppcRDSbacCoder[0][CI_NEXT_BEST]);
    m_pppcRDSbacCoder[0][CI_CURR_BEST]->load( m_pppcRDSbacCoder[0][CI_NEXT_BEST]);
  }
}

/** End SAO encoder
 */
Void TEncSampleAdaptiveOffset::endSaoEnc()
{
  m_pcPic = NULL;
  m_pcEntropyCoder = NULL;
}

inline Int xSign(Int x)
{
  return ((x >> 31) | ((Int)( (((UInt) -x)) >> 31)));
}

/** Calculate SAO statistics for non-cross-slice or non-cross-tile processing
 * \param  pRecStart to-be-filtered block buffer pointer
 * \param  pOrgStart original block buffer pointer
 * \param  stride picture buffer stride
 * \param  ppStat statistics buffer
 * \param  ppCount counter buffer
 * \param  width block width
 * \param  height block height
 * \param  pbBorderAvail availabilities of block border pixels
 */
Void TEncSampleAdaptiveOffset::calcSaoStatsBlock( Pel* pRecStart, Pel* pOrgStart, Int stride, Int64** ppStats, Int64** ppCount, UInt width, UInt height, Bool* pbBorderAvail, Int iYCbCr)
{
  Int64 *stats, *count;
  Int classIdx, posShift, startX, endX, startY, endY, signLeft,signRight,signDown,signDown1;
  Pel *pOrg, *pRec;
  UInt edgeType;
  Int x, y;
  Pel *pTableBo = (iYCbCr==0)?m_lumaTableBo:m_chromaTableBo;

  //--------- Band offset-----------//
  stats = ppStats[SAO_BO];
  count = ppCount[SAO_BO];
  pOrg   = pOrgStart;
  pRec   = pRecStart;
  for (y=0; y< height; y++)
  {
    for (x=0; x< width; x++)
    {
      classIdx = pTableBo[pRec[x]];
      if (classIdx)
      {
        stats[classIdx] += (pOrg[x] - pRec[x]); 
        count[classIdx] ++;
      }
    }
    pOrg += stride;
    pRec += stride;
  }
  //---------- Edge offset 0--------------//
  stats = ppStats[SAO_EO_0];
  count = ppCount[SAO_EO_0];
  pOrg   = pOrgStart;
  pRec   = pRecStart;


  startX = (pbBorderAvail[SGU_L]) ? 0 : 1;
  endX   = (pbBorderAvail[SGU_R]) ? width : (width -1);
  for (y=0; y< height; y++)
  {
    signLeft = xSign(pRec[startX] - pRec[startX-1]);
    for (x=startX; x< endX; x++)
    {
      signRight =  xSign(pRec[x] - pRec[x+1]); 
      edgeType =  signRight + signLeft + 2;
      signLeft  = -signRight;

      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;
    }
    pRec  += stride;
    pOrg += stride;
  }

  //---------- Edge offset 1--------------//
  stats = ppStats[SAO_EO_1];
  count = ppCount[SAO_EO_1];
  pOrg   = pOrgStart;
  pRec   = pRecStart;

  startY = (pbBorderAvail[SGU_T]) ? 0 : 1;
  endY   = (pbBorderAvail[SGU_B]) ? height : height-1;
  if (!pbBorderAvail[SGU_T])
  {
    pRec  += stride;
    pOrg  += stride;
  }

  for (x=0; x< width; x++)
  {
    m_iUpBuff1[x] = xSign(pRec[x] - pRec[x-stride]);
  }
  for (y=startY; y<endY; y++)
  {
    for (x=0; x< width; x++)
    {
      signDown     =  xSign(pRec[x] - pRec[x+stride]); 
      edgeType    =  signDown + m_iUpBuff1[x] + 2;
      m_iUpBuff1[x] = -signDown;

      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;
    }
    pOrg += stride;
    pRec += stride;
  }
  //---------- Edge offset 2--------------//
  stats = ppStats[SAO_EO_2];
  count = ppCount[SAO_EO_2];
  pOrg   = pOrgStart;
  pRec   = pRecStart;

  posShift= stride + 1;

  startX = (pbBorderAvail[SGU_L]) ? 0 : 1 ;
  endX   = (pbBorderAvail[SGU_R]) ? width : (width-1);

  //prepare 2nd line upper sign
  pRec += stride;
  for (x=startX; x< endX+1; x++)
  {
    m_iUpBuff1[x] = xSign(pRec[x] - pRec[x- posShift]);
  }

  //1st line
  pRec -= stride;
  if(pbBorderAvail[SGU_TL])
  {
    x= 0;
    edgeType      =  xSign(pRec[x] - pRec[x- posShift]) - m_iUpBuff1[x+1] + 2;
    stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
    count[m_auiEoTable[edgeType]] ++;
  }
  if(pbBorderAvail[SGU_T])
  {
    for(x= 1; x< endX; x++)
    {
      edgeType      =  xSign(pRec[x] - pRec[x- posShift]) - m_iUpBuff1[x+1] + 2;
      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;
    }
  }
  pRec   += stride;
  pOrg   += stride;

  //middle lines
  for (y= 1; y< height-1; y++)
  {
    for (x=startX; x<endX; x++)
    {
      signDown1      =  xSign(pRec[x] - pRec[x+ posShift]) ;
      edgeType      =  signDown1 + m_iUpBuff1[x] + 2;
      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;

      m_iUpBufft[x+1] = -signDown1; 
    }
    m_iUpBufft[startX] = xSign(pRec[stride+startX] - pRec[startX-1]);

    ipSwap     = m_iUpBuff1;
    m_iUpBuff1 = m_iUpBufft;
    m_iUpBufft = ipSwap;

    pRec  += stride;
    pOrg  += stride;
  }

  //last line
  if(pbBorderAvail[SGU_B])
  {
    for(x= startX; x< width-1; x++)
    {
      edgeType =  xSign(pRec[x] - pRec[x+ posShift]) + m_iUpBuff1[x] + 2;
      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;
    }
  }
  if(pbBorderAvail[SGU_BR])
  {
    x= width -1;
    edgeType =  xSign(pRec[x] - pRec[x+ posShift]) + m_iUpBuff1[x] + 2;
    stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
    count[m_auiEoTable[edgeType]] ++;
  }

  //---------- Edge offset 3--------------//

  stats = ppStats[SAO_EO_3];
  count = ppCount[SAO_EO_3];
  pOrg   = pOrgStart;
  pRec   = pRecStart;

  posShift     = stride - 1;
  startX = (pbBorderAvail[SGU_L]) ? 0 : 1;
  endX   = (pbBorderAvail[SGU_R]) ? width : (width -1);

  //prepare 2nd line upper sign
  pRec += stride;
  for (x=startX-1; x< endX; x++)
  {
    m_iUpBuff1[x] = xSign(pRec[x] - pRec[x- posShift]);
  }


  //first line
  pRec -= stride;
  if(pbBorderAvail[SGU_T])
  {
    for(x= startX; x< width -1; x++)
    {
      edgeType = xSign(pRec[x] - pRec[x- posShift]) -m_iUpBuff1[x-1] + 2;
      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;
    }
  }
  if(pbBorderAvail[SGU_TR])
  {
    x= width-1;
    edgeType = xSign(pRec[x] - pRec[x- posShift]) -m_iUpBuff1[x-1] + 2;
    stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
    count[m_auiEoTable[edgeType]] ++;
  }
  pRec  += stride;
  pOrg  += stride;

  //middle lines
  for (y= 1; y< height-1; y++)
  {
    for(x= startX; x< endX; x++)
    {
      signDown1      =  xSign(pRec[x] - pRec[x+ posShift]) ;
      edgeType      =  signDown1 + m_iUpBuff1[x] + 2;

      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;
      m_iUpBuff1[x-1] = -signDown1; 

    }
    m_iUpBuff1[endX-1] = xSign(pRec[endX-1 + stride] - pRec[endX]);

    pRec  += stride;
    pOrg  += stride;
  }

  //last line
  if(pbBorderAvail[SGU_BL])
  {
    x= 0;
    edgeType = xSign(pRec[x] - pRec[x+ posShift]) + m_iUpBuff1[x] + 2;
    stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
    count[m_auiEoTable[edgeType]] ++;

  }
  if(pbBorderAvail[SGU_B])
  {
    for(x= 1; x< endX; x++)
    {
      edgeType = xSign(pRec[x] - pRec[x+ posShift]) + m_iUpBuff1[x] + 2;
      stats[m_auiEoTable[edgeType]] += (pOrg[x] - pRec[x]);
      count[m_auiEoTable[edgeType]] ++;
    }
  }
}

/** Calculate SAO statistics for current LCU
 * \param  iAddr,  iPartIdx,  iYCbCr
 */
Void TEncSampleAdaptiveOffset::calcSaoStatsCu(Int iAddr, Int iPartIdx, Int iYCbCr)
{
  if(!m_bUseNIF)
  {
    calcSaoStatsCuOrg( iAddr, iPartIdx, iYCbCr);
  }
  else
  {
    Int64** ppStats = m_iOffsetOrg[iPartIdx];
    Int64** ppCount = m_iCount    [iPartIdx];

    //parameters
    Int  isChroma = (iYCbCr != 0)? 1:0;
    Int  stride   = (iYCbCr != 0)?(m_pcPic->getCStride()):(m_pcPic->getStride());
    Pel* pPicOrg = getPicYuvAddr (m_pcPic->getPicYuvOrg(), iYCbCr);
    Pel* pPicRec  = getPicYuvAddr(m_pcYuvTmp, iYCbCr);

    std::vector<NDBFBlockInfo>& vFilterBlocks = *(m_pcPic->getCU(iAddr)->getNDBFilterBlocks());

    //variables
    UInt  xPos, yPos, width, height;
    Bool* pbBorderAvail;
    UInt  posOffset;

    for(Int i=0; i< vFilterBlocks.size(); i++)
    {
      xPos        = vFilterBlocks[i].posX   >> isChroma;
      yPos        = vFilterBlocks[i].posY   >> isChroma;
      width       = vFilterBlocks[i].width  >> isChroma;
      height      = vFilterBlocks[i].height >> isChroma;
      pbBorderAvail = vFilterBlocks[i].isBorderAvailable;

      posOffset = (yPos* stride) + xPos;

      calcSaoStatsBlock(pPicRec+ posOffset, pPicOrg+ posOffset, stride, ppStats, ppCount,width, height, pbBorderAvail, iYCbCr);
    }
  }

}

/** Calculate SAO statistics for current LCU without non-crossing slice
 * \param  iAddr,  iPartIdx,  iYCbCr
 */
Void TEncSampleAdaptiveOffset::calcSaoStatsCuOrg(Int iAddr, Int iPartIdx, Int iYCbCr)
{
  Int x,y;
  TComDataCU *pTmpCu = m_pcPic->getCU(iAddr);
  TComSPS *pTmpSPS =  m_pcPic->getSlice(0)->getSPS();

  Pel* pOrg;
  Pel* pRec;
  Int iStride;
  Int iLcuHeight = pTmpSPS->getMaxCUHeight();
  Int iLcuWidth  = pTmpSPS->getMaxCUWidth();
  UInt uiLPelX   = pTmpCu->getCUPelX();
  UInt uiTPelY   = pTmpCu->getCUPelY();
  UInt uiRPelX;
  UInt uiBPelY;
  Int64* iStats;
  Int64* iCount;
  Int iClassIdx;
  Int iPicWidthTmp;
  Int iPicHeightTmp;
  Int iStartX;
  Int iStartY;
  Int iEndX;
  Int iEndY;
  Pel* pTableBo = (iYCbCr==0)?m_lumaTableBo:m_chromaTableBo;

  Int iIsChroma = (iYCbCr!=0)? 1:0;
  Int numSkipLine = iIsChroma? 2:4;
  if (m_saoLcuBasedOptimization == 0)
  {
    numSkipLine = 0;
  }

  Int numSkipLineRight = iIsChroma? 3:5;
  if (m_saoLcuBasedOptimization == 0)
  {
    numSkipLineRight = 0;
  }

  iPicWidthTmp  = m_iPicWidth  >> iIsChroma;
  iPicHeightTmp = m_iPicHeight >> iIsChroma;
  iLcuWidth     = iLcuWidth    >> iIsChroma;
  iLcuHeight    = iLcuHeight   >> iIsChroma;
  uiLPelX       = uiLPelX      >> iIsChroma;
  uiTPelY       = uiTPelY      >> iIsChroma;
  uiRPelX       = uiLPelX + iLcuWidth  ;
  uiBPelY       = uiTPelY + iLcuHeight ;
  uiRPelX       = uiRPelX > iPicWidthTmp  ? iPicWidthTmp  : uiRPelX;
  uiBPelY       = uiBPelY > iPicHeightTmp ? iPicHeightTmp : uiBPelY;
  iLcuWidth     = uiRPelX - uiLPelX;
  iLcuHeight    = uiBPelY - uiTPelY;

  iStride    =  (iYCbCr == 0)? m_pcPic->getStride(): m_pcPic->getCStride();

//if(iSaoType == BO_0 || iSaoType == BO_1)
  {
    if( m_saoLcuBasedOptimization && m_saoLcuBoundary )
    {
      numSkipLine = iIsChroma? 1:3;
      numSkipLineRight = iIsChroma? 2:4;
    }
    iStats = m_iOffsetOrg[iPartIdx][SAO_BO];
    iCount = m_iCount    [iPartIdx][SAO_BO];

    pOrg = getPicYuvAddr(m_pcPic->getPicYuvOrg(), iYCbCr, iAddr);
    pRec = getPicYuvAddr(m_pcPic->getPicYuvRec(), iYCbCr, iAddr);

    iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth : iLcuWidth-numSkipLineRight;
    iEndY   = (uiBPelY == iPicHeightTmp) ? iLcuHeight : iLcuHeight-numSkipLine;
    for (y=0; y<iEndY; y++)
    {
      for (x=0; x<iEndX; x++)
      {
        iClassIdx = pTableBo[pRec[x]];
        if (iClassIdx)
        {
          iStats[iClassIdx] += (pOrg[x] - pRec[x]); 
          iCount[iClassIdx] ++;
        }
      }
      pOrg += iStride;
      pRec += iStride;
    }

  }
  Int iSignLeft;
  Int iSignRight;
  Int iSignDown;
  Int iSignDown1;
  Int iSignDown2;

  UInt uiEdgeType;

//if (iSaoType == EO_0  || iSaoType == EO_1 || iSaoType == EO_2 || iSaoType == EO_3)
  {
  //if (iSaoType == EO_0)
    {
      if( m_saoLcuBasedOptimization && m_saoLcuBoundary )
      {
        numSkipLine = iIsChroma? 1:3;
        numSkipLineRight = iIsChroma? 3:5;
      }
      iStats = m_iOffsetOrg[iPartIdx][SAO_EO_0];
      iCount = m_iCount    [iPartIdx][SAO_EO_0];

      pOrg = getPicYuvAddr(m_pcPic->getPicYuvOrg(), iYCbCr, iAddr);
      pRec = getPicYuvAddr(m_pcPic->getPicYuvRec(), iYCbCr, iAddr);

      iStartX = (uiLPelX == 0) ? 1 : 0;
      iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth-1 : iLcuWidth-numSkipLineRight;
      for (y=0; y<iLcuHeight-numSkipLine; y++)
      {
        iSignLeft = xSign(pRec[iStartX] - pRec[iStartX-1]);
        for (x=iStartX; x< iEndX; x++)
        {
          iSignRight =  xSign(pRec[x] - pRec[x+1]); 
          uiEdgeType =  iSignRight + iSignLeft + 2;
          iSignLeft  = -iSignRight;

          iStats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
          iCount[m_auiEoTable[uiEdgeType]] ++;
        }
        pOrg += iStride;
        pRec += iStride;
      }
    }

  //if (iSaoType == EO_1)
    {
      if( m_saoLcuBasedOptimization && m_saoLcuBoundary )
      {
        numSkipLine = iIsChroma? 2:4;
        numSkipLineRight = iIsChroma? 2:4;
      }
      iStats = m_iOffsetOrg[iPartIdx][SAO_EO_1];
      iCount = m_iCount    [iPartIdx][SAO_EO_1];

      pOrg = getPicYuvAddr(m_pcPic->getPicYuvOrg(), iYCbCr, iAddr);
      pRec = getPicYuvAddr(m_pcPic->getPicYuvRec(), iYCbCr, iAddr);

      iStartY = (uiTPelY == 0) ? 1 : 0;
      iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth : iLcuWidth-numSkipLineRight;
      iEndY   = (uiBPelY == iPicHeightTmp) ? iLcuHeight-1 : iLcuHeight-numSkipLine;
      if (uiTPelY == 0)
      {
        pOrg += iStride;
        pRec += iStride;
      }

      for (x=0; x< iLcuWidth; x++)
      {
        m_iUpBuff1[x] = xSign(pRec[x] - pRec[x-iStride]);
      }
      for (y=iStartY; y<iEndY; y++)
      {
        for (x=0; x<iEndX; x++)
        {
          iSignDown     =  xSign(pRec[x] - pRec[x+iStride]); 
          uiEdgeType    =  iSignDown + m_iUpBuff1[x] + 2;
          m_iUpBuff1[x] = -iSignDown;

          iStats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
          iCount[m_auiEoTable[uiEdgeType]] ++;
        }
        pOrg += iStride;
        pRec += iStride;
      }
    }
  //if (iSaoType == EO_2)
    {
      if( m_saoLcuBasedOptimization && m_saoLcuBoundary )
      {
        numSkipLine = iIsChroma? 2:4;
        numSkipLineRight = iIsChroma? 3:5;
      }
      iStats = m_iOffsetOrg[iPartIdx][SAO_EO_2];
      iCount = m_iCount    [iPartIdx][SAO_EO_2];

      pOrg = getPicYuvAddr(m_pcPic->getPicYuvOrg(), iYCbCr, iAddr);
      pRec = getPicYuvAddr(m_pcPic->getPicYuvRec(), iYCbCr, iAddr);

      iStartX = (uiLPelX == 0) ? 1 : 0;
      iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth-1 : iLcuWidth-numSkipLineRight;

      iStartY = (uiTPelY == 0) ? 1 : 0;
      iEndY   = (uiBPelY == iPicHeightTmp) ? iLcuHeight-1 : iLcuHeight-numSkipLine;
      if (uiTPelY == 0)
      {
        pOrg += iStride;
        pRec += iStride;
      }

      for (x=iStartX; x<iEndX; x++)
      {
        m_iUpBuff1[x] = xSign(pRec[x] - pRec[x-iStride-1]);
      }
      for (y=iStartY; y<iEndY; y++)
      {
        iSignDown2 = xSign(pRec[iStride+iStartX] - pRec[iStartX-1]);
        for (x=iStartX; x<iEndX; x++)
        {
          iSignDown1      =  xSign(pRec[x] - pRec[x+iStride+1]) ;
          uiEdgeType      =  iSignDown1 + m_iUpBuff1[x] + 2;
          m_iUpBufft[x+1] = -iSignDown1; 
          iStats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
          iCount[m_auiEoTable[uiEdgeType]] ++;
        }
        m_iUpBufft[iStartX] = iSignDown2;
        ipSwap     = m_iUpBuff1;
        m_iUpBuff1 = m_iUpBufft;
        m_iUpBufft = ipSwap;

        pRec += iStride;
        pOrg += iStride;
      }
    } 
  //if (iSaoType == EO_3  )
    {
      if( m_saoLcuBasedOptimization && m_saoLcuBoundary )
      {
        numSkipLine = iIsChroma? 2:4;
        numSkipLineRight = iIsChroma? 3:5;
      }
      iStats = m_iOffsetOrg[iPartIdx][SAO_EO_3];
      iCount = m_iCount    [iPartIdx][SAO_EO_3];

      pOrg = getPicYuvAddr(m_pcPic->getPicYuvOrg(), iYCbCr, iAddr);
      pRec = getPicYuvAddr(m_pcPic->getPicYuvRec(), iYCbCr, iAddr);

      iStartX = (uiLPelX == 0) ? 1 : 0;
      iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth-1 : iLcuWidth-numSkipLineRight;

      iStartY = (uiTPelY == 0) ? 1 : 0;
      iEndY   = (uiBPelY == iPicHeightTmp) ? iLcuHeight-1 : iLcuHeight-numSkipLine;
      if (iStartY == 1)
      {
        pOrg += iStride;
        pRec += iStride;
      }

      for (x=iStartX-1; x<iEndX; x++)
      {
        m_iUpBuff1[x] = xSign(pRec[x] - pRec[x-iStride+1]);
      }

      for (y=iStartY; y<iEndY; y++)
      {
        for (x=iStartX; x<iEndX; x++)
        {
          iSignDown1      =  xSign(pRec[x] - pRec[x+iStride-1]) ;
          uiEdgeType      =  iSignDown1 + m_iUpBuff1[x] + 2;
          m_iUpBuff1[x-1] = -iSignDown1; 
          iStats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
          iCount[m_auiEoTable[uiEdgeType]] ++;
        }
        m_iUpBuff1[iEndX-1] = xSign(pRec[iEndX-1 + iStride] - pRec[iEndX]);

        pRec += iStride;
        pOrg += iStride;
      } 
    } 
  }
}


Void TEncSampleAdaptiveOffset::calcSaoStatsCu_BeforeDblk( TComPic* pcPic )
{
  Int addr, yCbCr;
  Int x,y;
  TComSPS *pTmpSPS =  pcPic->getSlice(0)->getSPS();

  Pel* pOrg;
  Pel* pRec;
  Int stride;
  Int lcuHeight = pTmpSPS->getMaxCUHeight();
  Int lcuWidth  = pTmpSPS->getMaxCUWidth();
  UInt rPelX;
  UInt bPelY;
  Int64* stats;
  Int64* count;
  Int classIdx;
  Int picWidthTmp = 0;
  Int picHeightTmp = 0;
  Int startX;
  Int startY;
  Int endX;
  Int endY;
  Int firstX, firstY;

  Int idxY;
  Int idxX;
  Int frameHeightInCU = m_iNumCuInHeight;
  Int frameWidthInCU  = m_iNumCuInWidth;
  Int j, k;

  Int isChroma;
  Int numSkipLine, numSkipLineRight;

  UInt lPelX, tPelY;
  TComDataCU *pTmpCu;
  Pel* pTableBo;

  for (idxY = 0; idxY< frameHeightInCU; idxY++)
  {
    for (idxX = 0; idxX< frameWidthInCU; idxX++)
    {
      lcuHeight = pTmpSPS->getMaxCUHeight();
      lcuWidth  = pTmpSPS->getMaxCUWidth();
      addr     = idxX  + frameWidthInCU*idxY;
      pTmpCu = pcPic->getCU(addr);
      lPelX   = pTmpCu->getCUPelX();
      tPelY   = pTmpCu->getCUPelY();
      for( yCbCr = 0; yCbCr < 3; yCbCr++ )
      {
        isChroma = (yCbCr!=0)? 1:0;

        for ( j=0;j<MAX_NUM_SAO_TYPE;j++)
        {
          for ( k=0;k< MAX_NUM_SAO_CLASS;k++)
          {
            m_count_PreDblk    [addr][yCbCr][j][k] = 0;
            m_offsetOrg_PreDblk[addr][yCbCr][j][k] = 0;
          }  
        }
        if( yCbCr == 0 )
        {
          picWidthTmp  = m_iPicWidth;
          picHeightTmp = m_iPicHeight;
        }
        else if( yCbCr == 1 )
        {
          picWidthTmp  = m_iPicWidth  >> isChroma;
          picHeightTmp = m_iPicHeight >> isChroma;
          lcuWidth     = lcuWidth    >> isChroma;
          lcuHeight    = lcuHeight   >> isChroma;
          lPelX       = lPelX      >> isChroma;
          tPelY       = tPelY      >> isChroma;
        }
        rPelX       = lPelX + lcuWidth  ;
        bPelY       = tPelY + lcuHeight ;
        rPelX       = rPelX > picWidthTmp  ? picWidthTmp  : rPelX;
        bPelY       = bPelY > picHeightTmp ? picHeightTmp : bPelY;
        lcuWidth     = rPelX - lPelX;
        lcuHeight    = bPelY - tPelY;

        stride    =  (yCbCr == 0)? pcPic->getStride(): pcPic->getCStride();
        pTableBo = (yCbCr==0)?m_lumaTableBo:m_chromaTableBo;

        //if(iSaoType == BO)

        numSkipLine = isChroma? 1:3;
        numSkipLineRight = isChroma? 2:4;

        stats = m_offsetOrg_PreDblk[addr][yCbCr][SAO_BO];
        count = m_count_PreDblk[addr][yCbCr][SAO_BO];

        pOrg = getPicYuvAddr(pcPic->getPicYuvOrg(), yCbCr, addr);
        pRec = getPicYuvAddr(pcPic->getPicYuvRec(), yCbCr, addr);

        startX   = (rPelX == picWidthTmp) ? lcuWidth : lcuWidth-numSkipLineRight;
        startY   = (bPelY == picHeightTmp) ? lcuHeight : lcuHeight-numSkipLine;

        for (y=0; y<lcuHeight; y++)
        {
          for (x=0; x<lcuWidth; x++)
          {
            if( x < startX && y < startY )
              continue;

            classIdx = pTableBo[pRec[x]];
            if (classIdx)
            {
              stats[classIdx] += (pOrg[x] - pRec[x]); 
              count[classIdx] ++;
            }
          }
          pOrg += stride;
          pRec += stride;
        }

        Int signLeft;
        Int signRight;
        Int signDown;
        Int signDown1;
        Int signDown2;

        UInt uiEdgeType;

        //if (iSaoType == EO_0)

        numSkipLine = isChroma? 1:3;
        numSkipLineRight = isChroma? 3:5;

        stats = m_offsetOrg_PreDblk[addr][yCbCr][SAO_EO_0];
        count = m_count_PreDblk[addr][yCbCr][SAO_EO_0];

        pOrg = getPicYuvAddr(pcPic->getPicYuvOrg(), yCbCr, addr);
        pRec = getPicYuvAddr(pcPic->getPicYuvRec(), yCbCr, addr);

        startX   = (rPelX == picWidthTmp) ? lcuWidth-1 : lcuWidth-numSkipLineRight;
        startY   = (bPelY == picHeightTmp) ? lcuHeight : lcuHeight-numSkipLine;
        firstX   = (lPelX == 0) ? 1 : 0;
        endX   = (rPelX == picWidthTmp) ? lcuWidth-1 : lcuWidth;

        for (y=0; y<lcuHeight; y++)
        {
          signLeft = xSign(pRec[firstX] - pRec[firstX-1]);
          for (x=firstX; x< endX; x++)
          {
            signRight =  xSign(pRec[x] - pRec[x+1]); 
            uiEdgeType =  signRight + signLeft + 2;
            signLeft  = -signRight;

            if( x < startX && y < startY )
              continue;

            stats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
            count[m_auiEoTable[uiEdgeType]] ++;
          }
          pOrg += stride;
          pRec += stride;
        }

        //if (iSaoType == EO_1)

        numSkipLine = isChroma? 2:4;
        numSkipLineRight = isChroma? 2:4;

        stats = m_offsetOrg_PreDblk[addr][yCbCr][SAO_EO_1];
        count = m_count_PreDblk[addr][yCbCr][SAO_EO_1];

        pOrg = getPicYuvAddr(pcPic->getPicYuvOrg(), yCbCr, addr);
        pRec = getPicYuvAddr(pcPic->getPicYuvRec(), yCbCr, addr);

        startX   = (rPelX == picWidthTmp) ? lcuWidth : lcuWidth-numSkipLineRight;
        startY   = (bPelY == picHeightTmp) ? lcuHeight-1 : lcuHeight-numSkipLine;
        firstY = (tPelY == 0) ? 1 : 0;
        endY   = (bPelY == picHeightTmp) ? lcuHeight-1 : lcuHeight;
        if (firstY == 1)
        {
          pOrg += stride;
          pRec += stride;
        }

        for (x=0; x< lcuWidth; x++)
        {
          m_iUpBuff1[x] = xSign(pRec[x] - pRec[x-stride]);
        }
        for (y=firstY; y<endY; y++)
        {
          for (x=0; x<lcuWidth; x++)
          {
            signDown     =  xSign(pRec[x] - pRec[x+stride]); 
            uiEdgeType    =  signDown + m_iUpBuff1[x] + 2;
            m_iUpBuff1[x] = -signDown;

            if( x < startX && y < startY )
              continue;

            stats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
            count[m_auiEoTable[uiEdgeType]] ++;
          }
          pOrg += stride;
          pRec += stride;
        }

        //if (iSaoType == EO_2)

        numSkipLine = isChroma? 2:4;
        numSkipLineRight = isChroma? 3:5;

        stats = m_offsetOrg_PreDblk[addr][yCbCr][SAO_EO_2];
        count = m_count_PreDblk[addr][yCbCr][SAO_EO_2];

        pOrg = getPicYuvAddr(pcPic->getPicYuvOrg(), yCbCr, addr);
        pRec = getPicYuvAddr(pcPic->getPicYuvRec(), yCbCr, addr);

        startX   = (rPelX == picWidthTmp) ? lcuWidth-1 : lcuWidth-numSkipLineRight;
        startY   = (bPelY == picHeightTmp) ? lcuHeight-1 : lcuHeight-numSkipLine;
        firstX   = (lPelX == 0) ? 1 : 0;
        firstY = (tPelY == 0) ? 1 : 0;
        endX   = (rPelX == picWidthTmp) ? lcuWidth-1 : lcuWidth;
        endY   = (bPelY == picHeightTmp) ? lcuHeight-1 : lcuHeight;
        if (firstY == 1)
        {
          pOrg += stride;
          pRec += stride;
        }

        for (x=firstX; x<endX; x++)
        {
          m_iUpBuff1[x] = xSign(pRec[x] - pRec[x-stride-1]);
        }
        for (y=firstY; y<endY; y++)
        {
          signDown2 = xSign(pRec[stride+startX] - pRec[startX-1]);
          for (x=firstX; x<endX; x++)
          {
            signDown1      =  xSign(pRec[x] - pRec[x+stride+1]) ;
            uiEdgeType      =  signDown1 + m_iUpBuff1[x] + 2;
            m_iUpBufft[x+1] = -signDown1; 

            if( x < startX && y < startY )
              continue;

            stats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
            count[m_auiEoTable[uiEdgeType]] ++;
          }
          m_iUpBufft[firstX] = signDown2;
          ipSwap     = m_iUpBuff1;
          m_iUpBuff1 = m_iUpBufft;
          m_iUpBufft = ipSwap;

          pRec += stride;
          pOrg += stride;
        }

        //if (iSaoType == EO_3)

        numSkipLine = isChroma? 2:4;
        numSkipLineRight = isChroma? 3:5;

        stats = m_offsetOrg_PreDblk[addr][yCbCr][SAO_EO_3];
        count = m_count_PreDblk[addr][yCbCr][SAO_EO_3];

        pOrg = getPicYuvAddr(pcPic->getPicYuvOrg(), yCbCr, addr);
        pRec = getPicYuvAddr(pcPic->getPicYuvRec(), yCbCr, addr);

        startX   = (rPelX == picWidthTmp) ? lcuWidth-1 : lcuWidth-numSkipLineRight;
        startY   = (bPelY == picHeightTmp) ? lcuHeight-1 : lcuHeight-numSkipLine;
        firstX   = (lPelX == 0) ? 1 : 0;
        firstY = (tPelY == 0) ? 1 : 0;
        endX   = (rPelX == picWidthTmp) ? lcuWidth-1 : lcuWidth;
        endY   = (bPelY == picHeightTmp) ? lcuHeight-1 : lcuHeight;
        if (firstY == 1)
        {
          pOrg += stride;
          pRec += stride;
        }

        for (x=firstX-1; x<endX; x++)
        {
          m_iUpBuff1[x] = xSign(pRec[x] - pRec[x-stride+1]);
        }

        for (y=firstY; y<endY; y++)
        {
          for (x=firstX; x<endX; x++)
          {
            signDown1      =  xSign(pRec[x] - pRec[x+stride-1]) ;
            uiEdgeType      =  signDown1 + m_iUpBuff1[x] + 2;
            m_iUpBuff1[x-1] = -signDown1; 

            if( x < startX && y < startY )
              continue;

            stats[m_auiEoTable[uiEdgeType]] += (pOrg[x] - pRec[x]);
            count[m_auiEoTable[uiEdgeType]] ++;
          }
          m_iUpBuff1[endX-1] = xSign(pRec[endX-1 + stride] - pRec[endX]);

          pRec += stride;
          pOrg += stride;
        }
      }
    }
  }
}


/** get SAO statistics
 * \param  *psQTPart,  iYCbCr
 */
Void TEncSampleAdaptiveOffset::getSaoStats(SAOQTPart *psQTPart, Int iYCbCr)
{
  Int iLevelIdx, iPartIdx, iTypeIdx, iClassIdx;
  Int i;
  Int iNumTotalType = MAX_NUM_SAO_TYPE;
  Int LcuIdxX;
  Int LcuIdxY;
  Int iAddr;
  Int iFrameWidthInCU = m_pcPic->getFrameWidthInCU();
  Int iDownPartIdx;
  Int iPartStart;
  Int iPartEnd;
  SAOQTPart*  pOnePart; 

  if (m_uiMaxSplitLevel == 0)
  {
    iPartIdx = 0;
    pOnePart = &(psQTPart[iPartIdx]);
    for (LcuIdxY = pOnePart->StartCUY; LcuIdxY<= pOnePart->EndCUY; LcuIdxY++)
    {
      for (LcuIdxX = pOnePart->StartCUX; LcuIdxX<= pOnePart->EndCUX; LcuIdxX++)
      {
        iAddr = LcuIdxY*iFrameWidthInCU + LcuIdxX;
        calcSaoStatsCu(iAddr, iPartIdx, iYCbCr);
      }
    }
  }
  else
  {
    for(iPartIdx=m_aiNumCulPartsLevel[m_uiMaxSplitLevel-1]; iPartIdx<m_aiNumCulPartsLevel[m_uiMaxSplitLevel]; iPartIdx++)
    {
      pOnePart = &(psQTPart[iPartIdx]);
      for (LcuIdxY = pOnePart->StartCUY; LcuIdxY<= pOnePart->EndCUY; LcuIdxY++)
      {
        for (LcuIdxX = pOnePart->StartCUX; LcuIdxX<= pOnePart->EndCUX; LcuIdxX++)
        {
          iAddr = LcuIdxY*iFrameWidthInCU + LcuIdxX;
          calcSaoStatsCu(iAddr, iPartIdx, iYCbCr);
        }
      }
    }
    for (iLevelIdx = m_uiMaxSplitLevel-1; iLevelIdx>=0; iLevelIdx-- )
    {
      iPartStart = (iLevelIdx > 0) ? m_aiNumCulPartsLevel[iLevelIdx-1] : 0;
      iPartEnd   = m_aiNumCulPartsLevel[iLevelIdx];

      for(iPartIdx = iPartStart; iPartIdx < iPartEnd; iPartIdx++)
      {
        pOnePart = &(psQTPart[iPartIdx]);
        for (i=0; i< NUM_DOWN_PART; i++)
        {
          iDownPartIdx = pOnePart->DownPartsIdx[i];
          for (iTypeIdx=0; iTypeIdx<iNumTotalType; iTypeIdx++)
          {
            for (iClassIdx=0; iClassIdx< (iTypeIdx < SAO_BO ? m_iNumClass[iTypeIdx] : SAO_MAX_BO_CLASSES) +1; iClassIdx++)
            {
              m_iOffsetOrg[iPartIdx][iTypeIdx][iClassIdx] += m_iOffsetOrg[iDownPartIdx][iTypeIdx][iClassIdx];
              m_iCount [iPartIdx][iTypeIdx][iClassIdx]    += m_iCount [iDownPartIdx][iTypeIdx][iClassIdx];
            }
          }
        }
      }
    }
  }
}

/** reset offset statistics 
 * \param 
 */
Void TEncSampleAdaptiveOffset::resetStats()
{
  for (Int i=0;i<m_iNumTotalParts;i++)
  {
    m_dCostPartBest[i] = MAX_DOUBLE;
    m_iTypePartBest[i] = -1;
    m_iDistOrg[i] = 0;
    for (Int j=0;j<MAX_NUM_SAO_TYPE;j++)
    {
      m_iDist[i][j] = 0;
      m_iRate[i][j] = 0;
      m_dCost[i][j] = 0;
      for (Int k=0;k<MAX_NUM_SAO_CLASS;k++)
      {
        m_iCount [i][j][k] = 0;
        m_iOffset[i][j][k] = 0;
        m_iOffsetOrg[i][j][k] = 0;
      }  
    }
  }
}

/** Sample adaptive offset process
 * \param pcSaoParam
 * \param dLambdaLuma
 * \param dLambdaChroma
 */
#if SAO_ENCODING_CHOICE
Void TEncSampleAdaptiveOffset::SAOProcess(SAOParam *pcSaoParam, Double dLambdaLuma, Double dLambdaChroma, Int depth)
#else
Void TEncSampleAdaptiveOffset::SAOProcess(SAOParam *pcSaoParam, Double dLambdaLuma, Double dLambdaChroma)
#endif
{
  if(m_bUseNIF)
  {
    m_pcPic->getPicYuvRec()->copyToPic(m_pcYuvTmp);
  }

  m_uiSaoBitIncreaseY = max(g_bitDepthY - 10, 0);
  m_uiSaoBitIncreaseC = max(g_bitDepthC - 10, 0);
  m_iOffsetThY = 1 << min(g_bitDepthY - 5, 5);
  m_iOffsetThC = 1 << min(g_bitDepthC - 5, 5);
  resetSAOParam(pcSaoParam);
  if( !m_saoLcuBasedOptimization || !m_saoLcuBoundary )
  {
    resetStats();
  }
  Double dCostFinal = 0;
  if ( m_saoLcuBasedOptimization)
  {
#if SAO_ENCODING_CHOICE
    rdoSaoUnitAll(pcSaoParam, dLambdaLuma, dLambdaChroma, depth);
#else
    rdoSaoUnitAll(pcSaoParam, dLambdaLuma, dLambdaChroma);
#endif
  }
  else
  {
    pcSaoParam->bSaoFlag[0] = 1;
    pcSaoParam->bSaoFlag[1] = 0;
    dCostFinal = 0;
    Double lambdaRdo =  dLambdaLuma;
    resetStats();
    getSaoStats(pcSaoParam->psSaoPart[0], 0);
    runQuadTreeDecision(pcSaoParam->psSaoPart[0], 0, dCostFinal, m_uiMaxSplitLevel, lambdaRdo, 0);
    pcSaoParam->bSaoFlag[0] = dCostFinal < 0 ? 1:0;
    if(pcSaoParam->bSaoFlag[0])
    {
      convertQT2SaoUnit(pcSaoParam, 0, 0);
      assignSaoUnitSyntax(pcSaoParam->saoLcuParam[0],  pcSaoParam->psSaoPart[0], pcSaoParam->oneUnitFlag[0], 0);
    }
  }
  if (pcSaoParam->bSaoFlag[0])
  {
    processSaoUnitAll( pcSaoParam->saoLcuParam[0], pcSaoParam->oneUnitFlag[0], 0);
  }
  if (pcSaoParam->bSaoFlag[1])
  {
    processSaoUnitAll( pcSaoParam->saoLcuParam[1], pcSaoParam->oneUnitFlag[1], 1);
    processSaoUnitAll( pcSaoParam->saoLcuParam[2], pcSaoParam->oneUnitFlag[2], 2);
  }
}
/** Check merge SAO unit
 * \param saoUnitCurr current SAO unit 
 * \param saoUnitCheck SAO unit tobe check
 * \param dir direction
 */
Void TEncSampleAdaptiveOffset::checkMerge(SaoLcuParam * saoUnitCurr, SaoLcuParam * saoUnitCheck, Int dir)
{
  Int i ;
  Int countDiff = 0;
  if (saoUnitCurr->partIdx != saoUnitCheck->partIdx)
  {
    if (saoUnitCurr->typeIdx !=-1)
    {
      if (saoUnitCurr->typeIdx == saoUnitCheck->typeIdx)
      {
        for (i=0;i<saoUnitCurr->length;i++)
        {
          countDiff += (saoUnitCurr->offset[i] != saoUnitCheck->offset[i]);
        }
        countDiff += (saoUnitCurr->subTypeIdx != saoUnitCheck->subTypeIdx);
        if (countDiff ==0)
        {
          saoUnitCurr->partIdx = saoUnitCheck->partIdx;
          if (dir == 1)
          {
            saoUnitCurr->mergeUpFlag = 1;
            saoUnitCurr->mergeLeftFlag = 0;
          }
          else
          {
            saoUnitCurr->mergeUpFlag = 0;
            saoUnitCurr->mergeLeftFlag = 1;
          }
        }
      }
    }
    else
    {
      if (saoUnitCurr->typeIdx == saoUnitCheck->typeIdx)
      {
        saoUnitCurr->partIdx = saoUnitCheck->partIdx;
        if (dir == 1)
        {
          saoUnitCurr->mergeUpFlag = 1;
          saoUnitCurr->mergeLeftFlag = 0;
        }
        else
        {
          saoUnitCurr->mergeUpFlag = 0;
          saoUnitCurr->mergeLeftFlag = 1;
        }
      }
    }
  }
}
/** Assign SAO unit syntax from picture-based algorithm
 * \param saoLcuParam SAO LCU parameters
 * \param saoPart SAO part
 * \param oneUnitFlag SAO one unit flag
 * \param iYCbCr color component Index
 */
Void TEncSampleAdaptiveOffset::assignSaoUnitSyntax(SaoLcuParam* saoLcuParam,  SAOQTPart* saoPart, Bool &oneUnitFlag, Int yCbCr)
{
  if (saoPart->bSplit == 0)
  {
    oneUnitFlag = 1;
  }
  else
  {
    Int i,j, addr, addrUp, addrLeft,  idx, idxUp, idxLeft,  idxCount;

    oneUnitFlag = 0;

    idxCount = -1;
    saoLcuParam[0].mergeUpFlag = 0;
    saoLcuParam[0].mergeLeftFlag = 0;

    for (j=0;j<m_iNumCuInHeight;j++)
    {
      for (i=0;i<m_iNumCuInWidth;i++)
      {
        addr     = i + j*m_iNumCuInWidth;
        addrLeft = (addr%m_iNumCuInWidth == 0) ? -1 : addr - 1;
        addrUp   = (addr<m_iNumCuInWidth)      ? -1 : addr - m_iNumCuInWidth;
        idx      = saoLcuParam[addr].partIdxTmp;
        idxLeft  = (addrLeft == -1) ? -1 : saoLcuParam[addrLeft].partIdxTmp;
        idxUp    = (addrUp == -1)   ? -1 : saoLcuParam[addrUp].partIdxTmp;

        if(idx!=idxLeft && idx!=idxUp)
        {
          saoLcuParam[addr].mergeUpFlag   = 0; idxCount++;
          saoLcuParam[addr].mergeLeftFlag = 0;
          saoLcuParam[addr].partIdx = idxCount;
        }
        else if (idx==idxLeft)
        {
          saoLcuParam[addr].mergeUpFlag   = 1;
          saoLcuParam[addr].mergeLeftFlag = 1;
          saoLcuParam[addr].partIdx = saoLcuParam[addrLeft].partIdx;
        }
        else if (idx==idxUp)
        {
          saoLcuParam[addr].mergeUpFlag   = 1;
          saoLcuParam[addr].mergeLeftFlag = 0;
          saoLcuParam[addr].partIdx = saoLcuParam[addrUp].partIdx;
        }
        if (addrUp != -1)
        {
          checkMerge(&saoLcuParam[addr], &saoLcuParam[addrUp], 1);
        }
        if (addrLeft != -1)
        {
          checkMerge(&saoLcuParam[addr], &saoLcuParam[addrLeft], 0);
        }
      }
    }
  }
}
/** rate distortion optimization of all SAO units
 * \param saoParam SAO parameters
 * \param lambda 
 * \param lambdaChroma
 */
#if SAO_ENCODING_CHOICE
Void TEncSampleAdaptiveOffset::rdoSaoUnitAll(SAOParam *saoParam, Double lambda, Double lambdaChroma, Int depth)
#else
Void TEncSampleAdaptiveOffset::rdoSaoUnitAll(SAOParam *saoParam, Double lambda, Double lambdaChroma)
#endif
{

  Int idxY;
  Int idxX;
  Int frameHeightInCU = saoParam->numCuInHeight;
  Int frameWidthInCU  = saoParam->numCuInWidth;
  Int j, k;
  Int addr = 0;
  Int addrUp = -1;
  Int addrLeft = -1;
  Int compIdx = 0;
  SaoLcuParam mergeSaoParam[3][2];
  Double compDistortion[3];

  saoParam->bSaoFlag[0] = true;
  saoParam->bSaoFlag[1] = true;
  saoParam->oneUnitFlag[0] = false;
  saoParam->oneUnitFlag[1] = false;
  saoParam->oneUnitFlag[2] = false;

#if SAO_ENCODING_CHOICE
#if SAO_ENCODING_CHOICE_CHROMA
  Int numNoSao[2];
  numNoSao[0] = 0;// Luma 
  numNoSao[1] = 0;// Chroma 
  if( depth > 0 && m_depthSaoRate[0][depth-1] > SAO_ENCODING_RATE )
  {
    saoParam->bSaoFlag[0] = false;
  }
  if( depth > 0 && m_depthSaoRate[1][depth-1] > SAO_ENCODING_RATE_CHROMA )
  {
    saoParam->bSaoFlag[1] = false;
  }
#else
  Int numNoSao = 0;

  if( depth > 0 && m_depth0SaoRate > SAO_ENCODING_RATE )
  {
    saoParam->bSaoFlag[0] = false;
    saoParam->bSaoFlag[1] = false;
  }
#endif
#endif

  for (idxY = 0; idxY< frameHeightInCU; idxY++)
  {
    for (idxX = 0; idxX< frameWidthInCU; idxX++)
    {
      addr     = idxX  + frameWidthInCU*idxY;
      addrUp   = addr < frameWidthInCU ? -1:idxX   + frameWidthInCU*(idxY-1);
      addrLeft = idxX == 0               ? -1:idxX-1 + frameWidthInCU*idxY;
      Int allowMergeLeft = 1;
      Int allowMergeUp   = 1;
      UInt rate;
      Double bestCost, mergeCost;
      if (idxX!=0)
      { 
        // check tile id and slice id 
        if ( (m_pcPic->getPicSym()->getTileIdxMap(addr-1) != m_pcPic->getPicSym()->getTileIdxMap(addr)) || (m_pcPic->getCU(addr-1)->getSlice()->getSliceIdx() != m_pcPic->getCU(addr)->getSlice()->getSliceIdx()))
        {
          allowMergeLeft = 0;
        }
      }
      else
      {
        allowMergeLeft = 0;
      }
      if (idxY!=0)
      {
        if ( (m_pcPic->getPicSym()->getTileIdxMap(addr-m_iNumCuInWidth) != m_pcPic->getPicSym()->getTileIdxMap(addr)) || (m_pcPic->getCU(addr-m_iNumCuInWidth)->getSlice()->getSliceIdx() != m_pcPic->getCU(addr)->getSlice()->getSliceIdx()))
        {
          allowMergeUp = 0;
        }
      }
      else
      {
        allowMergeUp = 0;
      }

      compDistortion[0] = 0; 
      compDistortion[1] = 0; 
      compDistortion[2] = 0;
      m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_CURR_BEST]);
      if (allowMergeLeft)
      {
        m_pcEntropyCoder->m_pcEntropyCoderIf->codeSaoMerge(0); 
      }
      if (allowMergeUp)
      {
        m_pcEntropyCoder->m_pcEntropyCoderIf->codeSaoMerge(0);
      }
      m_pcRDGoOnSbacCoder->store( m_pppcRDSbacCoder[0][CI_TEMP_BEST] );
      // reset stats Y, Cb, Cr
      for ( compIdx=0;compIdx<3;compIdx++)
      {
        for ( j=0;j<MAX_NUM_SAO_TYPE;j++)
        {
          for ( k=0;k< MAX_NUM_SAO_CLASS;k++)
          {
            m_iOffset   [compIdx][j][k] = 0;
            if( m_saoLcuBasedOptimization && m_saoLcuBoundary ){
              m_iCount    [compIdx][j][k] = m_count_PreDblk    [addr][compIdx][j][k];
              m_iOffsetOrg[compIdx][j][k] = m_offsetOrg_PreDblk[addr][compIdx][j][k];
            }
            else
            {
              m_iCount    [compIdx][j][k] = 0;
              m_iOffsetOrg[compIdx][j][k] = 0;
            }
          }  
        }
        saoParam->saoLcuParam[compIdx][addr].typeIdx       =  -1;
        saoParam->saoLcuParam[compIdx][addr].mergeUpFlag   = 0;
        saoParam->saoLcuParam[compIdx][addr].mergeLeftFlag = 0;
        saoParam->saoLcuParam[compIdx][addr].subTypeIdx    = 0;
#if SAO_ENCODING_CHOICE
  if( (compIdx ==0 && saoParam->bSaoFlag[0])|| (compIdx >0 && saoParam->bSaoFlag[1]) )
#endif
        {
          calcSaoStatsCu(addr, compIdx,  compIdx);

       }
      }
      saoComponentParamDist(allowMergeLeft, allowMergeUp, saoParam, addr, addrUp, addrLeft, 0,  lambda, &mergeSaoParam[0][0], &compDistortion[0]);
      sao2ChromaParamDist(allowMergeLeft, allowMergeUp, saoParam, addr, addrUp, addrLeft, lambdaChroma, &mergeSaoParam[1][0], &mergeSaoParam[2][0], &compDistortion[0]);
     if( saoParam->bSaoFlag[0] || saoParam->bSaoFlag[1] )
      {
        // Cost of new SAO_params
        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_CURR_BEST]);
        m_pcRDGoOnSbacCoder->resetBits();
        if (allowMergeLeft)
        {
          m_pcEntropyCoder->m_pcEntropyCoderIf->codeSaoMerge(0); 
        }
        if (allowMergeUp)
        {
          m_pcEntropyCoder->m_pcEntropyCoderIf->codeSaoMerge(0);
        }
        for ( compIdx=0;compIdx<3;compIdx++)
        {
        if( (compIdx ==0 && saoParam->bSaoFlag[0]) || (compIdx >0 && saoParam->bSaoFlag[1]))
          {
           m_pcEntropyCoder->encodeSaoOffset(&saoParam->saoLcuParam[compIdx][addr], compIdx);
          }
        }

        rate = m_pcEntropyCoder->getNumberOfWrittenBits();
        bestCost = compDistortion[0] + (Double)rate;
        m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);

        // Cost of Merge
        for(Int mergeUp=0; mergeUp<2; ++mergeUp)
        {
          if ( (allowMergeLeft && (mergeUp==0)) || (allowMergeUp && (mergeUp==1)) )
          {
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_CURR_BEST]);
            m_pcRDGoOnSbacCoder->resetBits();
            if (allowMergeLeft)
            {
              m_pcEntropyCoder->m_pcEntropyCoderIf->codeSaoMerge(1-mergeUp); 
            }
            if ( allowMergeUp && (mergeUp==1) )
            {
              m_pcEntropyCoder->m_pcEntropyCoderIf->codeSaoMerge(1); 
            }

            rate = m_pcEntropyCoder->getNumberOfWrittenBits();
            mergeCost = compDistortion[mergeUp+1] + (Double)rate;
            if (mergeCost < bestCost)
            {
              bestCost = mergeCost;
              m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);              
              for ( compIdx=0;compIdx<3;compIdx++)
              {
                mergeSaoParam[compIdx][mergeUp].mergeLeftFlag = 1-mergeUp;
                mergeSaoParam[compIdx][mergeUp].mergeUpFlag = mergeUp;
                if( (compIdx==0 && saoParam->bSaoFlag[0]) || (compIdx>0 && saoParam->bSaoFlag[1]))
                {
                  copySaoUnit(&saoParam->saoLcuParam[compIdx][addr], &mergeSaoParam[compIdx][mergeUp] );             
                }
              }
            }
          }
        }
#if SAO_ENCODING_CHOICE
#if SAO_ENCODING_CHOICE_CHROMA
if( saoParam->saoLcuParam[0][addr].typeIdx == -1)
{
  numNoSao[0]++;
}
if( saoParam->saoLcuParam[1][addr].typeIdx == -1)
{
  numNoSao[1]+=2;
}
#else
        for ( compIdx=0;compIdx<3;compIdx++)
        {
          if( depth == 0 && saoParam->saoLcuParam[compIdx][addr].typeIdx == -1)
          {
            numNoSao++;
          }
        }
#endif
#endif
        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);
        m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[0][CI_CURR_BEST]);
      }
    }
  }
#if SAO_ENCODING_CHOICE
#if SAO_ENCODING_CHOICE_CHROMA
  if( !saoParam->bSaoFlag[0])
  {
    m_depthSaoRate[0][depth] = 1.0;
  }
  else
  {
    m_depthSaoRate[0][depth] = numNoSao[0]/((Double) frameHeightInCU*frameWidthInCU);
  }
  if( !saoParam->bSaoFlag[1]) 
  {
    m_depthSaoRate[1][depth] = 1.0;
  }
  else 
  {
    m_depthSaoRate[1][depth] = numNoSao[1]/((Double) frameHeightInCU*frameWidthInCU*2);
  }
#else
  if( depth == 0)
  {
    // update SAO Rate
    m_depth0SaoRate = numNoSao/((Double) frameHeightInCU*frameWidthInCU*3);
  }
#endif
#endif

}
/** rate distortion optimization of SAO unit 
 * \param saoParam SAO parameters
 * \param addr address 
 * \param addrUp above address
 * \param addrLeft left address 
 * \param yCbCr color component index
 * \param lambda 
 */
inline Int64 TEncSampleAdaptiveOffset::estSaoTypeDist(Int compIdx, Int typeIdx, Int shift, Double lambda, Int *currentDistortionTableBo, Double *currentRdCostTableBo)
{
  Int64 estDist = 0;
  Int classIdx;
  Int bitDepth = (compIdx==0) ? g_bitDepthY : g_bitDepthC;
  Int saoBitIncrease = (compIdx==0) ? m_uiSaoBitIncreaseY : m_uiSaoBitIncreaseC;
  Int saoOffsetTh = (compIdx==0) ? m_iOffsetThY : m_iOffsetThC;

  for(classIdx=1; classIdx < ( (typeIdx < SAO_BO) ?  m_iNumClass[typeIdx]+1 : SAO_MAX_BO_CLASSES+1); classIdx++)
  {
    if( typeIdx == SAO_BO)
    {
      currentDistortionTableBo[classIdx-1] = 0;
      currentRdCostTableBo[classIdx-1] = lambda;
    }
    if(m_iCount [compIdx][typeIdx][classIdx])
    {
      m_iOffset[compIdx][typeIdx][classIdx] = (Int64) xRoundIbdi(bitDepth, (Double)(m_iOffsetOrg[compIdx][typeIdx][classIdx]<<(bitDepth-8)) / (Double)(m_iCount [compIdx][typeIdx][classIdx]<<saoBitIncrease));
      m_iOffset[compIdx][typeIdx][classIdx] = Clip3(-saoOffsetTh+1, saoOffsetTh-1, (Int)m_iOffset[compIdx][typeIdx][classIdx]);
      if (typeIdx < 4)
      {
        if ( m_iOffset[compIdx][typeIdx][classIdx]<0 && classIdx<3 )
        {
          m_iOffset[compIdx][typeIdx][classIdx] = 0;
        }
        if ( m_iOffset[compIdx][typeIdx][classIdx]>0 && classIdx>=3)
        {
          m_iOffset[compIdx][typeIdx][classIdx] = 0;
        }
      }
      m_iOffset[compIdx][typeIdx][classIdx] = estIterOffset( typeIdx, classIdx, lambda, m_iOffset[compIdx][typeIdx][classIdx], m_iCount [compIdx][typeIdx][classIdx], m_iOffsetOrg[compIdx][typeIdx][classIdx], shift, saoBitIncrease, currentDistortionTableBo, currentRdCostTableBo, saoOffsetTh );
    }
    else
    {
      m_iOffsetOrg[compIdx][typeIdx][classIdx] = 0;
      m_iOffset[compIdx][typeIdx][classIdx] = 0;
    }
    if( typeIdx != SAO_BO )
    {
      estDist   += estSaoDist( m_iCount [compIdx][typeIdx][classIdx], m_iOffset[compIdx][typeIdx][classIdx] << saoBitIncrease, m_iOffsetOrg[compIdx][typeIdx][classIdx], shift);
    }

  }
  return estDist;
}

inline Int64 TEncSampleAdaptiveOffset::estSaoDist(Int64 count, Int64 offset, Int64 offsetOrg, Int shift)
{
  return (( count*offset*offset-offsetOrg*offset*2 ) >> shift);
}
inline Int64 TEncSampleAdaptiveOffset::estIterOffset(Int typeIdx, Int classIdx, Double lambda, Int64 offsetInput, Int64 count, Int64 offsetOrg, Int shift, Int bitIncrease, Int *currentDistortionTableBo, Double *currentRdCostTableBo, Int offsetTh )
{
  //Clean up, best_q_offset.
  Int64 iterOffset, tempOffset;
  Int64 tempDist, tempRate;
  Double tempCost, tempMinCost;
  Int64 offsetOutput = 0;
  iterOffset = offsetInput;
  // Assuming sending quantized value 0 results in zero offset and sending the value zero needs 1 bit. entropy coder can be used to measure the exact rate here. 
  tempMinCost = lambda; 
  while (iterOffset != 0)
  {
    // Calculate the bits required for signalling the offset
    tempRate = (typeIdx == SAO_BO) ? (abs((Int)iterOffset)+2) : (abs((Int)iterOffset)+1);
    if (abs((Int)iterOffset)==offsetTh-1) 
    {  
      tempRate --;
    }
    // Do the dequntization before distorion calculation
    tempOffset  = iterOffset << bitIncrease;
    tempDist    = estSaoDist( count, tempOffset, offsetOrg, shift);
    tempCost    = ((Double)tempDist + lambda * (Double) tempRate);
    if(tempCost < tempMinCost)
    {
      tempMinCost = tempCost;
      offsetOutput = iterOffset;
      if(typeIdx == SAO_BO)
      {
        currentDistortionTableBo[classIdx-1] = (Int) tempDist;
        currentRdCostTableBo[classIdx-1] = tempCost;
      }
    }
    iterOffset = (iterOffset > 0) ? (iterOffset-1):(iterOffset+1);
  }
  return offsetOutput;
}


Void TEncSampleAdaptiveOffset::saoComponentParamDist(Int allowMergeLeft, Int allowMergeUp, SAOParam *saoParam, Int addr, Int addrUp, Int addrLeft, Int yCbCr, Double lambda, SaoLcuParam *compSaoParam, Double *compDistortion)
{
  Int typeIdx;

  Int64 estDist;
  Int classIdx;
  Int shift = 2 * DISTORTION_PRECISION_ADJUSTMENT(((yCbCr==0)?g_bitDepthY:g_bitDepthC)-8);
  Int64 bestDist;

  SaoLcuParam*  saoLcuParam = &(saoParam->saoLcuParam[yCbCr][addr]);
  SaoLcuParam*  saoLcuParamNeighbor = NULL; 

  resetSaoUnit(saoLcuParam);
  resetSaoUnit(&compSaoParam[0]);
  resetSaoUnit(&compSaoParam[1]);


  Double dCostPartBest = MAX_DOUBLE;

  Double  bestRDCostTableBo = MAX_DOUBLE;
  Int     bestClassTableBo    = 0;
  Int     currentDistortionTableBo[MAX_NUM_SAO_CLASS];
  Double  currentRdCostTableBo[MAX_NUM_SAO_CLASS];


  SaoLcuParam   saoLcuParamRdo;   
  Double   estRate = 0;

  resetSaoUnit(&saoLcuParamRdo);

  m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);
  m_pcRDGoOnSbacCoder->resetBits();
 m_pcEntropyCoder->encodeSaoOffset(&saoLcuParamRdo, yCbCr);
  
  dCostPartBest = m_pcEntropyCoder->getNumberOfWrittenBits()*lambda ; 
  copySaoUnit(saoLcuParam, &saoLcuParamRdo );
  bestDist = 0;
  


  for (typeIdx=0; typeIdx<MAX_NUM_SAO_TYPE; typeIdx++)
  {
    estDist = estSaoTypeDist(yCbCr, typeIdx, shift, lambda, currentDistortionTableBo, currentRdCostTableBo);

    if( typeIdx == SAO_BO )
    {
      // Estimate Best Position
      Double currentRDCost = 0.0;

      for(Int i=0; i< SAO_MAX_BO_CLASSES -SAO_BO_LEN +1; i++)
      {
        currentRDCost = 0.0;
        for(UInt uj = i; uj < i+SAO_BO_LEN; uj++)
        {
          currentRDCost += currentRdCostTableBo[uj];
        }

        if( currentRDCost < bestRDCostTableBo)
        {
          bestRDCostTableBo = currentRDCost;
          bestClassTableBo  = i;
        }
      }

      // Re code all Offsets
      // Code Center
      estDist = 0;
      for(classIdx = bestClassTableBo; classIdx < bestClassTableBo+SAO_BO_LEN; classIdx++)
      {
        estDist += currentDistortionTableBo[classIdx];
      }
    }
    resetSaoUnit(&saoLcuParamRdo);
    saoLcuParamRdo.length = m_iNumClass[typeIdx];
    saoLcuParamRdo.typeIdx = typeIdx;
    saoLcuParamRdo.mergeLeftFlag = 0;
    saoLcuParamRdo.mergeUpFlag   = 0;
    saoLcuParamRdo.subTypeIdx = (typeIdx == SAO_BO) ? bestClassTableBo : 0;
    for (classIdx = 0; classIdx < saoLcuParamRdo.length; classIdx++)
    {
      saoLcuParamRdo.offset[classIdx] = (Int)m_iOffset[yCbCr][typeIdx][classIdx+saoLcuParamRdo.subTypeIdx+1];
    }
    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);
    m_pcRDGoOnSbacCoder->resetBits();
    m_pcEntropyCoder->encodeSaoOffset(&saoLcuParamRdo, yCbCr);

    estRate = m_pcEntropyCoder->getNumberOfWrittenBits();
    m_dCost[yCbCr][typeIdx] = (Double)((Double)estDist + lambda * (Double) estRate);

    if(m_dCost[yCbCr][typeIdx] < dCostPartBest)
    {
      dCostPartBest = m_dCost[yCbCr][typeIdx];
      copySaoUnit(saoLcuParam, &saoLcuParamRdo );
      bestDist = estDist;       
    }
  }
  compDistortion[0] += ((Double)bestDist/lambda);
  m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);
 m_pcEntropyCoder->encodeSaoOffset(saoLcuParam, yCbCr);
  m_pcRDGoOnSbacCoder->store( m_pppcRDSbacCoder[0][CI_TEMP_BEST] );


  // merge left or merge up

  for (Int idxNeighbor=0;idxNeighbor<2;idxNeighbor++) 
  {
    saoLcuParamNeighbor = NULL;
    if (allowMergeLeft && addrLeft>=0 && idxNeighbor ==0)
    {
      saoLcuParamNeighbor = &(saoParam->saoLcuParam[yCbCr][addrLeft]);
    }
    else if (allowMergeUp && addrUp>=0 && idxNeighbor ==1)
    {
      saoLcuParamNeighbor = &(saoParam->saoLcuParam[yCbCr][addrUp]);
    }
    if (saoLcuParamNeighbor!=NULL)
    {
      estDist = 0;
      typeIdx = saoLcuParamNeighbor->typeIdx;
      if (typeIdx>=0) 
      {
        Int mergeBandPosition = (typeIdx == SAO_BO)?saoLcuParamNeighbor->subTypeIdx:0;
        Int   merge_iOffset;
        for(classIdx = 0; classIdx < m_iNumClass[typeIdx]; classIdx++)
        {
          merge_iOffset = saoLcuParamNeighbor->offset[classIdx];
          estDist   += estSaoDist(m_iCount [yCbCr][typeIdx][classIdx+mergeBandPosition+1], merge_iOffset, m_iOffsetOrg[yCbCr][typeIdx][classIdx+mergeBandPosition+1],  shift);
        }
      }
      else
      {
        estDist = 0;
      }

      copySaoUnit(&compSaoParam[idxNeighbor], saoLcuParamNeighbor );
      compSaoParam[idxNeighbor].mergeUpFlag   = idxNeighbor;
      compSaoParam[idxNeighbor].mergeLeftFlag = !idxNeighbor;

      compDistortion[idxNeighbor+1] += ((Double)estDist/lambda);
    } 
  } 
}
Void TEncSampleAdaptiveOffset::sao2ChromaParamDist(Int allowMergeLeft, Int allowMergeUp, SAOParam *saoParam, Int addr, Int addrUp, Int addrLeft, Double lambda, SaoLcuParam *crSaoParam, SaoLcuParam *cbSaoParam, Double *distortion)
{
  Int typeIdx;

  Int64 estDist[2];
  Int classIdx;
  Int shift = 2 * DISTORTION_PRECISION_ADJUSTMENT(g_bitDepthC-8);
  Int64 bestDist = 0;

  SaoLcuParam*  saoLcuParam[2] = {&(saoParam->saoLcuParam[1][addr]), &(saoParam->saoLcuParam[2][addr])};
  SaoLcuParam*  saoLcuParamNeighbor[2] = {NULL, NULL}; 
  SaoLcuParam*  saoMergeParam[2][2];
  saoMergeParam[0][0] = &crSaoParam[0];
  saoMergeParam[0][1] = &crSaoParam[1]; 
  saoMergeParam[1][0] = &cbSaoParam[0];
  saoMergeParam[1][1] = &cbSaoParam[1];

  resetSaoUnit(saoLcuParam[0]);
  resetSaoUnit(saoLcuParam[1]);
  resetSaoUnit(saoMergeParam[0][0]);
  resetSaoUnit(saoMergeParam[0][1]);
  resetSaoUnit(saoMergeParam[1][0]);
  resetSaoUnit(saoMergeParam[1][1]);


  Double costPartBest = MAX_DOUBLE;

  Double  bestRDCostTableBo;
  Int     bestClassTableBo[2]    = {0, 0};
  Int     currentDistortionTableBo[MAX_NUM_SAO_CLASS];
  Double  currentRdCostTableBo[MAX_NUM_SAO_CLASS];

  SaoLcuParam   saoLcuParamRdo[2];   
  Double   estRate = 0;

  resetSaoUnit(&saoLcuParamRdo[0]);
  resetSaoUnit(&saoLcuParamRdo[1]);

  m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);
  m_pcRDGoOnSbacCoder->resetBits();
  m_pcEntropyCoder->encodeSaoOffset(&saoLcuParamRdo[0], 1);
  m_pcEntropyCoder->encodeSaoOffset(&saoLcuParamRdo[1], 2);
  
  costPartBest = m_pcEntropyCoder->getNumberOfWrittenBits()*lambda ; 
  copySaoUnit(saoLcuParam[0], &saoLcuParamRdo[0] );
  copySaoUnit(saoLcuParam[1], &saoLcuParamRdo[1] );

  for (typeIdx=0; typeIdx<MAX_NUM_SAO_TYPE; typeIdx++)
  {
    if( typeIdx == SAO_BO )
    {
      // Estimate Best Position
      for(Int compIdx = 0; compIdx < 2; compIdx++)
      {
        Double currentRDCost = 0.0;
        bestRDCostTableBo = MAX_DOUBLE;
        estDist[compIdx] = estSaoTypeDist(compIdx+1, typeIdx, shift, lambda, currentDistortionTableBo, currentRdCostTableBo);
        for(Int i=0; i< SAO_MAX_BO_CLASSES -SAO_BO_LEN +1; i++)
        {
          currentRDCost = 0.0;
          for(UInt uj = i; uj < i+SAO_BO_LEN; uj++)
          {
            currentRDCost += currentRdCostTableBo[uj];
          }

          if( currentRDCost < bestRDCostTableBo)
          {
            bestRDCostTableBo = currentRDCost;
            bestClassTableBo[compIdx]  = i;
          }
        }

        // Re code all Offsets
        // Code Center
        estDist[compIdx] = 0;
        for(classIdx = bestClassTableBo[compIdx]; classIdx < bestClassTableBo[compIdx]+SAO_BO_LEN; classIdx++)
        {
          estDist[compIdx] += currentDistortionTableBo[classIdx];
        }
      }
    }
    else
    {
      estDist[0] = estSaoTypeDist(1, typeIdx, shift, lambda, currentDistortionTableBo, currentRdCostTableBo);
      estDist[1] = estSaoTypeDist(2, typeIdx, shift, lambda, currentDistortionTableBo, currentRdCostTableBo);
    }

    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);
    m_pcRDGoOnSbacCoder->resetBits();

    for(Int compIdx = 0; compIdx < 2; compIdx++)
    {
      resetSaoUnit(&saoLcuParamRdo[compIdx]);
      saoLcuParamRdo[compIdx].length = m_iNumClass[typeIdx];
      saoLcuParamRdo[compIdx].typeIdx = typeIdx;
      saoLcuParamRdo[compIdx].mergeLeftFlag = 0;
      saoLcuParamRdo[compIdx].mergeUpFlag   = 0;
      saoLcuParamRdo[compIdx].subTypeIdx = (typeIdx == SAO_BO) ? bestClassTableBo[compIdx] : 0;
      for (classIdx = 0; classIdx < saoLcuParamRdo[compIdx].length; classIdx++)
      {
        saoLcuParamRdo[compIdx].offset[classIdx] = (Int)m_iOffset[compIdx+1][typeIdx][classIdx+saoLcuParamRdo[compIdx].subTypeIdx+1];
      }

      m_pcEntropyCoder->encodeSaoOffset(&saoLcuParamRdo[compIdx], compIdx+1);
    }
    estRate = m_pcEntropyCoder->getNumberOfWrittenBits();
    m_dCost[1][typeIdx] = (Double)((Double)(estDist[0] + estDist[1])  + lambda * (Double) estRate);
    
    if(m_dCost[1][typeIdx] < costPartBest)
    {
      costPartBest = m_dCost[1][typeIdx];
      copySaoUnit(saoLcuParam[0], &saoLcuParamRdo[0] );
      copySaoUnit(saoLcuParam[1], &saoLcuParamRdo[1] );
      bestDist = (estDist[0]+estDist[1]);       
    }
  }

  distortion[0] += ((Double)bestDist/lambda);
  m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[0][CI_TEMP_BEST]);
  m_pcEntropyCoder->encodeSaoOffset(saoLcuParam[0], 1);
  m_pcEntropyCoder->encodeSaoOffset(saoLcuParam[1], 2);
  m_pcRDGoOnSbacCoder->store( m_pppcRDSbacCoder[0][CI_TEMP_BEST] );

  // merge left or merge up

  for (Int idxNeighbor=0;idxNeighbor<2;idxNeighbor++) 
  {
    for(Int compIdx = 0; compIdx < 2; compIdx++)
    {
      saoLcuParamNeighbor[compIdx] = NULL;
      if (allowMergeLeft && addrLeft>=0 && idxNeighbor ==0)
      {
        saoLcuParamNeighbor[compIdx] = &(saoParam->saoLcuParam[compIdx+1][addrLeft]);
      }
      else if (allowMergeUp && addrUp>=0 && idxNeighbor ==1)
      {
        saoLcuParamNeighbor[compIdx] = &(saoParam->saoLcuParam[compIdx+1][addrUp]);
      }
      if (saoLcuParamNeighbor[compIdx]!=NULL)
      {
        estDist[compIdx] = 0;
        typeIdx = saoLcuParamNeighbor[compIdx]->typeIdx;
        if (typeIdx>=0) 
        {
          Int mergeBandPosition = (typeIdx == SAO_BO)?saoLcuParamNeighbor[compIdx]->subTypeIdx:0;
          Int   merge_iOffset;
          for(classIdx = 0; classIdx < m_iNumClass[typeIdx]; classIdx++)
          {
            merge_iOffset = saoLcuParamNeighbor[compIdx]->offset[classIdx];
            estDist[compIdx]   += estSaoDist(m_iCount [compIdx+1][typeIdx][classIdx+mergeBandPosition+1], merge_iOffset, m_iOffsetOrg[compIdx+1][typeIdx][classIdx+mergeBandPosition+1],  shift);
          }
        }
        else
        {
          estDist[compIdx] = 0;
        }

        copySaoUnit(saoMergeParam[compIdx][idxNeighbor], saoLcuParamNeighbor[compIdx] );
        saoMergeParam[compIdx][idxNeighbor]->mergeUpFlag   = idxNeighbor;
        saoMergeParam[compIdx][idxNeighbor]->mergeLeftFlag = !idxNeighbor;
        distortion[idxNeighbor+1] += ((Double)estDist[compIdx]/lambda);
      }
    } 
  } 
}

#endif

//! \}
