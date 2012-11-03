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

/** \file     TEncAdaptiveLoopFilter.cpp
 \brief    estimation part of adaptive loop filter class
 */
#include "TEncAdaptiveLoopFilter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#if !REMOVE_ALF
//! \ingroup TLibEncoder
//! \{
// ====================================================================================================================
// Constructor / destructor
// ====================================================================================================================

///AlfCorrData
AlfCorrData::AlfCorrData()
{
  this->componentID = -1;
  this->ECorr  = NULL;
  this->yCorr  = NULL;
  this->pixAcc = NULL;
}

AlfCorrData::AlfCorrData(Int cIdx)
{
  const Int numCoef = ALF_MAX_NUM_COEF;
  const Int maxNumGroups = NO_VAR_BINS;

  Int numGroups = (cIdx == ALF_Y)?(maxNumGroups):(1);

  this->componentID = cIdx;

  this->ECorr = new Double**[numGroups];
  this->yCorr = new Double*[numGroups];
  this->pixAcc = new Double[numGroups];
  for(Int g= 0; g< numGroups; g++)
  {
    this->yCorr[g] = new Double[numCoef];
    for(Int j=0; j< numCoef; j++)
    {
      this->yCorr[g][j] = 0;
    }

    this->ECorr[g] = new Double*[numCoef];
    for(Int i=0; i< numCoef; i++)
    {
      this->ECorr[g][i] = new Double[numCoef];
      for(Int j=0; j< numCoef; j++)
      {
        this->ECorr[g][i][j] = 0;
      }
    }
    this->pixAcc[g] = 0;  
  }
}

AlfCorrData::~AlfCorrData()
{
  if(this->componentID >=0)
  {
    const Int numCoef = ALF_MAX_NUM_COEF;
    const Int maxNumGroups = NO_VAR_BINS;

    Int numGroups = (this->componentID == ALF_Y)?(maxNumGroups):(1);

    for(Int g= 0; g< numGroups; g++)
    {
      for(Int i=0; i< numCoef; i++)
      {
        delete[] this->ECorr[g][i];
      }
      delete[] this->ECorr[g];
      delete[] this->yCorr[g];
    }
    delete[] this->ECorr;
    delete[] this->yCorr;
    delete[] this->pixAcc;
  }

}

AlfCorrData& AlfCorrData::operator += (const AlfCorrData& src)
{
  if(this->componentID >=0)
  {
    const Int numCoef = ALF_MAX_NUM_COEF;
    const Int maxNumGroups = NO_VAR_BINS;

    Int numGroups = (this->componentID == ALF_Y)?(maxNumGroups):(1);
    for(Int g=0; g< numGroups; g++)
    {
      this->pixAcc[g] += src.pixAcc[g];

      for(Int j=0; j< numCoef; j++)
      {
        this->yCorr[g][j] += src.yCorr[g][j];
        for(Int i=0; i< numCoef; i++)
        {
          this->ECorr[g][j][i] += src.ECorr[g][j][i];
        }
      }
    }
  }

  return *this;
}


Void AlfCorrData::reset()
{
  if(this->componentID >=0)
  {
    const Int numCoef = ALF_MAX_NUM_COEF;
    const Int maxNumGroups = NO_VAR_BINS;

    Int numGroups = (this->componentID == ALF_Y)?(maxNumGroups):(1);
    for(Int g=0; g< numGroups; g++)
    {
      this->pixAcc[g] = 0;

      for(Int j=0; j< numCoef; j++)
      {
        this->yCorr[g][j] = 0;
        for(Int i=0; i< numCoef; i++)
        {
          this->ECorr[g][j][i] = 0;
        }
      }


    }
  }

}

Void AlfCorrData::mergeFrom(const AlfCorrData& src, Int* mergeTable, Bool doPixAccMerge)
{
  assert(componentID == src.componentID);

  reset();

  const Int numCoef = ALF_MAX_NUM_COEF;

  Double **srcE, **dstE;
  Double *srcy, *dsty;

  switch(componentID)
  {
  case ALF_Cb:
  case ALF_Cr:
    {
      srcE = src.ECorr  [0];
      dstE = this->ECorr[0];

      srcy  = src.yCorr[0];
      dsty  = this->yCorr[0];

      for(Int j=0; j< numCoef; j++)
      {
        for(Int i=0; i< numCoef; i++)
        {
          dstE[j][i] += srcE[j][i];
        }

        dsty[j] += srcy[j];
      }
      if(doPixAccMerge)
      {
        this->pixAcc[0] = src.pixAcc[0];
      }
    }
    break;
  case ALF_Y:
    {
      Int maxFilterSetSize = (Int)NO_VAR_BINS;
      for (Int varInd=0; varInd< maxFilterSetSize; varInd++)
      {
        Int filtIdx = (mergeTable == NULL)?(0):(mergeTable[varInd]);
        srcE = src.ECorr  [varInd];
        dstE = this->ECorr[ filtIdx ];
        srcy  = src.yCorr[varInd];
        dsty  = this->yCorr[ filtIdx ];
        for(Int j=0; j< numCoef; j++)
        {
          for(Int i=0; i< numCoef; i++)
          {
            dstE[j][i] += srcE[j][i];
          }
          dsty[j] += srcy[j];
        }
        if(doPixAccMerge)
        {
          this->pixAcc[filtIdx] += src.pixAcc[varInd];
        }
      }
    }
    break;
  default:
    {
      printf("not a legal component ID\n");
      assert(0);
      exit(-1);
    }
  }
}

TEncAdaptiveLoopFilter::TEncAdaptiveLoopFilter()
{
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================
/** create global buffers for ALF encoding
 */
Void TEncAdaptiveLoopFilter::createAlfGlobalBuffers()
{
  for(Int compIdx =0; compIdx < NUM_ALF_COMPONENT; compIdx++)
  {
    m_alfNonSkippedCorr[compIdx] = new AlfCorrData*[m_uiNumCUsInFrame];
    m_alfCorr[compIdx] = new AlfCorrData*[m_uiNumCUsInFrame];
    for(Int n=0; n< m_uiNumCUsInFrame; n++)
    {
      m_alfCorr[compIdx][n]= new AlfCorrData(compIdx);
      m_alfCorr[compIdx][n]->reset();
      m_alfNonSkippedCorr[compIdx][n] = new AlfCorrData(compIdx);
      m_alfNonSkippedCorr[compIdx][n]->reset();
    }

    m_alfCorrMerged[compIdx] = new AlfCorrData(compIdx);

  }

  const Int numCoef = (Int)ALF_MAX_NUM_COEF;

  // temporary buffer for filter merge
  for(Int g=0; g< (Int)NO_VAR_BINS; g++)
  {
    m_y_merged[g] = new Double[numCoef];

    m_E_merged[g] = new Double*[numCoef];
    for(Int i=0; i< numCoef; i++)
    {
      m_E_merged[g][i]= new Double[numCoef];
    }
  }
  m_E_temp = new Double*[numCoef];
  for(Int i=0; i< numCoef; i++)
  {
    m_E_temp[i] = new Double[numCoef];
  }

  //alf params for temporal layers
  Int maxNumTemporalLayer =   (Int)(logf((float)(m_gopSize))/logf(2.0) + 1);
  m_alfPictureParam = new ALFParam**[ maxNumTemporalLayer ];
  for(Int t=0; t< maxNumTemporalLayer; t++)
  {
    m_alfPictureParam[t] = new ALFParam*[NUM_ALF_COMPONENT];
    for(Int compIdx=0; compIdx < NUM_ALF_COMPONENT; compIdx++)
    {
      m_alfPictureParam[t][compIdx] = new ALFParam(compIdx);
    }
  }

  // identity filter to estimate ALF off distortion
  for(Int i=0; i< (Int)NO_VAR_BINS; i++)
  {
    m_coeffNoFilter[i] = new Int[numCoef];
    ::memset(&(m_coeffNoFilter[i][0]), 0, sizeof(Int)*numCoef);
    m_coeffNoFilter[i][numCoef-1] = (1 << ALF_NUM_BIT_SHIFT);
  }
  m_numSlicesDataInOneLCU = new Int[m_uiNumCUsInFrame];

}

/** destroy ALF global buffers
 * This function is used to destroy the global ALF encoder buffers
 */
Void TEncAdaptiveLoopFilter::destroyAlfGlobalBuffers()
{
  for(Int compIdx =0; compIdx < NUM_ALF_COMPONENT; compIdx++)
  {
    for(Int n=0; n< m_uiNumCUsInFrame; n++)
    {
      delete m_alfCorr[compIdx][n];
      delete m_alfNonSkippedCorr[compIdx][n];
    }

    delete[] m_alfCorr[compIdx];

    m_alfCorr[compIdx] = NULL;
    delete[] m_alfNonSkippedCorr[compIdx];
    m_alfNonSkippedCorr[compIdx] = NULL;
    delete m_alfCorrMerged[compIdx];
  }
  const Int numCoef = (Int)ALF_MAX_NUM_COEF;

  // temporary buffer for filter merge
  for(Int g=0; g< (Int)NO_VAR_BINS; g++)
  {
    delete[] m_y_merged[g]; m_y_merged[g] = NULL;
    for(Int i=0; i< numCoef; i++)
    {
      delete[] m_E_merged[g][i];
    }
    delete[] m_E_merged[g];            m_E_merged[g] = NULL;
  }
  for(Int i=0; i< numCoef; i++)
  {
    delete[] m_E_temp[i];
  }
  delete[] m_E_temp; m_E_temp = NULL;

  Int maxNumTemporalLayer =   (Int)(logf((float)(m_gopSize))/logf(2.0) + 1);
  for(Int t=0; t< maxNumTemporalLayer; t++)
  {
    for(Int compIdx=0; compIdx < NUM_ALF_COMPONENT; compIdx++)
    {
      delete m_alfPictureParam[t][compIdx];
    }
    delete[] m_alfPictureParam[t];
  }
  delete[] m_alfPictureParam; m_alfPictureParam = NULL;
  //const Int numCoef = (Int)ALF_MAX_NUM_COEF;

  for(Int i=0; i< (Int)NO_VAR_BINS; i++)
  {
    delete[] m_coeffNoFilter[i];
  }

  delete[] m_numSlicesDataInOneLCU;

}

/** initialize ALF encoder configurations
 * \param [in, out] alfParamSet ALF parameter set
 * \param [in, out] alfCtrlParam ALF CU-on/off control parameters
 */
Void TEncAdaptiveLoopFilter::initALFEncoderParam()
{
  //get last valid slice index
  for(Int s=0; s< m_uiNumSlicesInPic; s++)
  {
    if(m_pcPic->getValidSlice(s))
    {
      m_lastSliceIdx = s;
    }
  }
  //get number slices in each LCU
  if(m_uiNumSlicesInPic == 1 || m_iSGDepth == 0)
  {
    for(Int n=0; n< m_uiNumCUsInFrame; n++)
    {
      m_numSlicesDataInOneLCU[n] = 1;
    }
  }
  else
  {
    Int count;
    Int prevSliceID = -1;

    for(Int n=0; n< m_uiNumCUsInFrame; n++)
    {
      std::vector<NDBFBlockInfo>& vNDBFBlock = *(m_pcPic->getCU(n)->getNDBFilterBlocks());

      count = 0;

      for(Int i=0; i< (Int)vNDBFBlock.size(); i++)
      {
        if(vNDBFBlock[i].sliceID != prevSliceID)
        {
          prevSliceID = vNDBFBlock[i].sliceID;
          count++;
        }
      }

      m_numSlicesDataInOneLCU[n] = count;
    }
  }
}

/** ALF encoding process top function
 * \param [in, out] alfParamSet ALF parameter set
 * \param [in, out] alfCtrlParam ALF CU-on/off control parameters
 * \param [in] dLambdaLuma lambda value for luma RDO
 * \param [in] dLambdaChroma lambda value for chroma RDO
 */
#if ALF_CHROMA_LAMBDA
Void TEncAdaptiveLoopFilter::ALFProcess(ALFParam** alfPictureParam, Double lambdaLuma, Double lambdaChroma)
#else
Void TEncAdaptiveLoopFilter::ALFProcess(ALFParam** alfPictureParam,  Double lambda)
#endif
{
#if ALF_CHROMA_LAMBDA
  m_dLambdaLuma   = lambdaLuma;
  m_dLambdaChroma = lambdaChroma;
#else
  m_dLambdaLuma   = lambda;
  m_dLambdaChroma = lambda;
#endif
  TComPicYuv* yuvOrg    = m_pcPic->getPicYuvOrg();
  TComPicYuv* yuvRec    = m_pcPic->getPicYuvRec();
  TComPicYuv* yuvExtRec = m_pcTempPicYuv;

  //picture boundary padding
  yuvRec->copyToPic(yuvExtRec);
  yuvExtRec->setBorderExtension( false );
  yuvExtRec->extendPicBorder   ();

  //initialize encoder parameters
  initALFEncoderParam();
  //get LCU statistics
  getStatistics(yuvOrg, yuvExtRec);

  //decide ALF parameters
  decideParameters(alfPictureParam, yuvOrg, yuvExtRec, yuvRec);

  if(m_alfLowLatencyEncoding)
  {
    //derive time-delayed filter for next picture
    decideAlfPictureParam(m_alfPictureParam[getTemporalLayerNo(m_pcPic->getPOC(), m_gopSize)], false);
  }
}

/** Derive filter coefficients
 * \param [in, out] alfPicQTPart picture quad-tree partition information
 * \param [in, out] alfPicLCUCorr correlations for LCUs
 * \param [int] partIdx partition index
 * \param [int] partLevel partition level
 */
Void TEncAdaptiveLoopFilter::deriveFilterInfo(Int compIdx, AlfCorrData* alfCorr, ALFParam* alfFiltParam, Int maxNumFilters, Double lambda)
{
  const Int filtNo = 0; 
  const Int numCoeff = ALF_MAX_NUM_COEF;

  switch(compIdx)
  {
  case ALF_Y:
    {
      Int lambdaForMerge = ((Int) lambda) * (1<<(2*g_uiBitIncrement));
      Int numFilters;

      ::memset(m_varIndTab, 0, sizeof(Int)*NO_VAR_BINS);
      xfindBestFilterVarPred(alfCorr->yCorr, alfCorr->ECorr, alfCorr->pixAcc, m_filterCoeffSym, &numFilters, m_varIndTab, lambdaForMerge, maxNumFilters);
      xcodeFiltCoeff(m_filterCoeffSym,  m_varIndTab, numFilters, alfFiltParam);
    }
    break;
  case ALF_Cb:
  case ALF_Cr:
    {
      static Double coef[ALF_MAX_NUM_COEF];

      alfFiltParam->filters_per_group = 1;

      gnsSolveByChol(alfCorr->ECorr[0], alfCorr->yCorr[0], coef, numCoeff);
      xQuantFilterCoef(coef, m_filterCoeffSym[0], filtNo, g_uiBitDepth + g_uiBitIncrement);
      ::memcpy(alfFiltParam->coeffmulti[0], m_filterCoeffSym[0], sizeof(Int)*numCoeff);
      predictALFCoeff(alfFiltParam->coeffmulti, numCoeff, alfFiltParam->filters_per_group);
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

/** Estimate filtering distortion
 * \param [in] compIdx luma/chroma component index
 * \param [in] alfCorr correlations
 * \param [in] coeffSet filter coefficients
 * \param [in] filterSetSize number of filter set
 * \param [in] mergeTable merge table of filter set (only for luma BA)
 * \param [in] doPixAccMerge calculate pixel squared value (true) or not (false)
 */
Int64 TEncAdaptiveLoopFilter::estimateFilterDistortion(Int compIdx, AlfCorrData* alfCorr, Int** coeffSet, Int filterSetSize, Int* mergeTable, Bool doPixAccMerge)
{
  const Int numCoeff = (Int)ALF_MAX_NUM_COEF;
  AlfCorrData* alfMerged = m_alfCorrMerged[compIdx];

  alfMerged->mergeFrom(*alfCorr, mergeTable, doPixAccMerge);

  Int**     coeff = (coeffSet == NULL)?(m_coeffNoFilter):(coeffSet);
  Int64     iDist = 0;
  for(Int f=0; f< filterSetSize; f++)
  {
    iDist += xFastFiltDistEstimation(alfMerged->ECorr[f], alfMerged->yCorr[f], coeff[f], numCoeff);
  }
  return iDist;
}

/** Calculate distortion for ALF LCU
 * \param [in] skipLCUBottomLines true for considering skipping bottom LCU lines
 * \param [in] compIdx luma/chroma component index
 * \param [in] alfLCUInfo ALF LCU information
 * \param [in] picSrc source picture buffer
 * \param [in] picCmp to-be-compared picture buffer
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] formatShift 0 for luma and 1 for chroma (4:2:0)
 * \return the distortion
 */
Int64 TEncAdaptiveLoopFilter::calcAlfLCUDist(Bool skipLCUBottomLines, Int compIdx, AlfLCUInfo& alfLCUInfo, Pel* picSrc, Pel* picCmp, Int stride, Int formatShift)
{
  Int64 dist = 0;  
  Int  posOffset, ypos, xpos, height, width;
  Pel* pelCmp;
  Pel* pelSrc;
#if LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER || LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
  Int endypos;
  Bool notSkipLinesBelowVB = true;
  Int lcuAddr = alfLCUInfo.pcCU->getAddr();
  if(skipLCUBottomLines)
  {
    if(lcuAddr + m_numLCUInPicWidth < m_uiNumCUsInFrame)
    {
      notSkipLinesBelowVB = false;
    }
  }
#endif
#if LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
  Bool notSkipLinesRightVB = true;
  Int endxpos;
  if(skipLCUBottomLines)
  {
    if((lcuAddr + 1) % m_numLCUInPicWidth != 0 )
    {
      notSkipLinesRightVB = false;
    }
  }
#endif

  switch(compIdx)
  {
  case ALF_Cb:
  case ALF_Cr:
    {
      for(Int n=0; n< alfLCUInfo.numSGU; n++)
      {
        ypos    = (Int)(alfLCUInfo[n].posY   >> formatShift);
        xpos    = (Int)(alfLCUInfo[n].posX   >> formatShift);
        height  = (Int)(alfLCUInfo[n].height >> formatShift);
        width   = (Int)(alfLCUInfo[n].width  >> formatShift);

#if LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER
        if(!notSkipLinesBelowVB )
        {
          endypos = ypos+ height -1;
          Int iLineVBPos = m_lcuHeightChroma - 2;
          Int yEndLineInLCU = endypos % m_lcuHeightChroma;
          height = (yEndLineInLCU >= iLineVBPos) ? (height - 2) : height ; 
        }
#endif
#if LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
        if( !notSkipLinesRightVB)
        {
          endxpos = xpos+ width -1;
          Int iLineVBPos = m_lcuWidthChroma - 7;
          Int xEndLineInLCU = endxpos % m_lcuWidthChroma;
          width = (xEndLineInLCU >= iLineVBPos) ? (width - 7) : width ; 
        }
#endif
        posOffset = (ypos * stride) + xpos;
        pelCmp    = picCmp + posOffset;    
        pelSrc    = picSrc + posOffset;    


        dist  += xCalcSSD( pelSrc, pelCmp,  width, height, stride );
      }

    }
    break;
  case ALF_Y:
    {
      for(Int n=0; n< alfLCUInfo.numSGU; n++)
      {
        ypos    = (Int)(alfLCUInfo[n].posY);
        xpos    = (Int)(alfLCUInfo[n].posX);
        height  = (Int)(alfLCUInfo[n].height);
        width   = (Int)(alfLCUInfo[n].width);

#if LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER
        if(!notSkipLinesBelowVB)
        {
          endypos = ypos+ height -1;
          Int iLineVBPos = m_lcuHeight - 4;
          Int yEndLineInLCU = endypos % m_lcuHeight;
          height = (yEndLineInLCU >= iLineVBPos) ? (height - 4) : height ; 
        }
#endif
#if LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
        if( !notSkipLinesRightVB)
        {
          endxpos = xpos+ width -1;
          Int iLineVBPos = m_lcuWidth - 9;
          Int xEndLineInLCU = endxpos % m_lcuWidth;
          width = (xEndLineInLCU >= iLineVBPos) ? (width - 9) : width ; 
        }
#endif
        posOffset = (ypos * stride) + xpos;
        pelCmp    = picCmp + posOffset;    
        pelSrc    = picSrc + posOffset;    

        dist  += xCalcSSD( pelSrc, pelCmp,  width, height, stride );
      }

    }
    break;
  default:
    {
      printf("not a legal component ID for ALF \n");
      assert(0);
      exit(-1);
    }
  }

  return dist;
}

/** Copy one ALF LCU region
 * \param [in] alfLCUInfo ALF LCU information
 * \param [out] picDst to-be-compared picture buffer
 * \param [in] picSrc source picture buffer
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] formatShift 0 for luma and 1 for chroma (4:2:0)
 */
Void TEncAdaptiveLoopFilter::copyOneAlfLCU(AlfLCUInfo& alfLCUInfo, Pel* picDst, Pel* picSrc, Int stride, Int formatShift)
{
  Int posOffset, ypos, xpos, height, width;
  Pel* pelDst;
  Pel* pelSrc;

  for(Int n=0; n< alfLCUInfo.numSGU; n++)
  {
    ypos    = (Int)(alfLCUInfo[n].posY   >> formatShift);
    xpos    = (Int)(alfLCUInfo[n].posX   >> formatShift);
    height  = (Int)(alfLCUInfo[n].height >> formatShift);
    width   = (Int)(alfLCUInfo[n].width  >> formatShift);

    posOffset  = ( ypos * stride)+ xpos;
    pelDst   = picDst  + posOffset;    
    pelSrc   = picSrc  + posOffset;    

    for(Int j=0; j< height; j++)
    {
      ::memcpy(pelDst, pelSrc, sizeof(Pel)*width);
      pelDst += stride;
      pelSrc += stride;
    }
  }

}

/** Reconstruct ALF LCU pixels
 * \param [in] compIdx luma/chroma component index
 * \param [in] alfLCUInfo ALF LCU information
 * \param [in] alfUnitParam ALF unit parameters
 * \param [in] picDec picture buffer for un-filtered picture 
 * \param [out] picRest picture buffer for reconstructed picture
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] formatShift 0 for luma and 1 for chroma (4:2:0)
 */
Void TEncAdaptiveLoopFilter::reconstructOneAlfLCU(Int compIdx, AlfLCUInfo& alfLCUInfo, Bool alfEnabled, ALFParam* alfParam, Pel* picDec, Pel* picRest, Int stride, Int formatShift)
{
  Int ypos, xpos, height, width;

  if(alfEnabled)
  {
    assert(alfParam->alf_flag == 1);

    //reconstruct ALF coefficients & related parameters 
    reconstructCoefInfo(compIdx, alfParam, m_filterCoeffSym, m_varIndTab);

    //filtering process
    for(Int n=0; n< alfLCUInfo.numSGU; n++)
    {
      ypos    = (Int)(alfLCUInfo[n].posY   >> formatShift);
      xpos    = (Int)(alfLCUInfo[n].posX   >> formatShift);
      height  = (Int)(alfLCUInfo[n].height >> formatShift);
      width   = (Int)(alfLCUInfo[n].width  >> formatShift);

      filterOneCompRegion(picRest, picDec, stride, (compIdx!=ALF_Y), ypos, ypos+height, xpos, xpos+width, m_filterCoeffSym, m_varIndTab, m_varImg);
    }
  }
  else
  {
    copyOneAlfLCU(alfLCUInfo, picRest, picDec, stride, formatShift);
  }
}

/** LCU-based mode decision
 * \param [in, out] alfParamSet ALF parameter set
 * \param [in] compIdx luma/chroma component index
 * \param [in] pOrg picture buffer for original picture
 * \param [in] pDec picture buffer for un-filtered picture 
 * \param [out] pRest picture buffer for reconstructed picture
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] formatShift 0 for luma and 1 for chroma (4:2:0)
 * \param [in] alfCorrLCUs correlations for LCUs
 */
Void TEncAdaptiveLoopFilter::executeLCUOnOffDecision(Int compIdx, ALFParam* alfParam, Pel* pOrg, Pel* pDec, Pel* pRest, Int stride, Int formatShift,AlfCorrData** alfCorrLCUs)
{
  Double lambda = (compIdx == ALF_Y)?(m_dLambdaLuma):(m_dLambdaChroma);
  static Int* isProcessed = NULL;

  Int64  distEnc, distOff;
  Int    rateEnc, rateOff;
  Double costEnc, costOff;
  isProcessed = new Int[m_uiNumCUsInFrame];
  ::memset(isProcessed, 0, sizeof(Int)*m_uiNumCUsInFrame);

  //reset LCU enabled flags
  for(Int i=0; i< m_uiNumCUsInFrame; i++)
  {
    m_pcPic->getCU(i)->setAlfLCUEnabled(false, compIdx);
  }
  for(Int s=0; s<= m_lastSliceIdx; s++)
  {
    if(!m_pcPic->getValidSlice(s))
    {
      continue;
    }
    Int numTilesInSlice = (Int)m_pvpSliceTileAlfLCU[s].size();
    for(Int t=0; t< numTilesInSlice; t++)
    {
      std::vector<AlfLCUInfo*> & vpAlfLCU = m_pvpSliceTileAlfLCU[s][t];
      Pel* pSrc = pDec;

      if(m_bUseNonCrossALF)
      {
        pSrc = getPicBuf(m_pcSliceYuvTmp, compIdx);
        copyRegion(vpAlfLCU, pSrc, pDec, stride, formatShift);
        extendRegionBorder(vpAlfLCU, pSrc, stride, formatShift);
      }

      Int numLCUs = (Int)vpAlfLCU.size();
      for(Int n=0; n< numLCUs; n++)
      {
        AlfLCUInfo*   alfLCU       = vpAlfLCU[n];                  //ALF LCU information
        TComDataCU*   pcCU         = alfLCU->pcCU;
        Int           addr         = pcCU->getAddr();              //real LCU addr
        if(isProcessed[addr] == 0)
        {
          Bool lcuAlfDisabled = true;
          if(alfParam->alf_flag == 1)
          {
            //ALF on
            reconstructOneAlfLCU(compIdx, *alfLCU, true, alfParam, pSrc, pRest, stride, formatShift);
            distEnc = calcAlfLCUDist(m_alfLowLatencyEncoding, compIdx, *alfLCU, pOrg, pRest, stride, formatShift);
            rateEnc = 1;
            costEnc = (Double)distEnc + lambda*((Double)rateEnc);
            costEnc += ((lambda* 1.5)*1.0);  //RDCO

            //ALF off
            distOff = calcAlfLCUDist(m_alfLowLatencyEncoding, compIdx, *alfLCU, pOrg, pSrc, stride, formatShift);
            rateOff = 1;
            costOff = (Double)distOff + lambda*((Double)rateOff);

            lcuAlfDisabled = (costOff < costEnc);

            pcCU->setAlfLCUEnabled( (lcuAlfDisabled?0:1) , compIdx);
          }

          if(lcuAlfDisabled)
          {
            copyOneAlfLCU(*alfLCU, pRest, pSrc, stride, formatShift);
          }
        }
        else
        {
          reconstructOneAlfLCU(compIdx, *alfLCU, pcCU->getAlfLCUEnabled(compIdx), alfParam, pSrc, pRest, stride, formatShift);
        }
      } //LCU
    } //tile

  } //slice


  delete[] isProcessed;
  isProcessed = NULL;
}

Int TEncAdaptiveLoopFilter::getTemporalLayerNo(Int poc, Int picDistance)
{
  Int layer = 0;
  while(picDistance > 0)
  {
    if(poc % picDistance == 0)
    {
      break;
    }
    picDistance = (Int)(picDistance/2);
    layer++;
  }
  return layer;
}

Void TEncAdaptiveLoopFilter::setCurAlfParam(ALFParam** alfPictureParam)
{
  if(m_alfLowLatencyEncoding)
  {
    Int temporalLayer = getTemporalLayerNo(m_pcPic->getPOC(), m_gopSize);
    for(Int compIdx=0; compIdx< 3; compIdx++)
    {
      *(alfPictureParam[compIdx]) = *(m_alfPictureParam[temporalLayer][compIdx]);
    }
  }
  else
  {
    decideAlfPictureParam(alfPictureParam, true);
    estimateLcuControl(alfPictureParam);
    decideAlfPictureParam(alfPictureParam, false);    
  }
}

Void TEncAdaptiveLoopFilter::decideAlfPictureParam(ALFParam** alfPictureParam, Bool useAllLCUs)
{
  AlfCorrData*** lcuCorr = (m_alfLowLatencyEncoding?m_alfNonSkippedCorr:m_alfCorr);
  for(Int compIdx =0; compIdx < NUM_ALF_COMPONENT; compIdx++)
  {
    Double      lambda   = ((compIdx == ALF_Y)?m_dLambdaLuma:m_dLambdaChroma)*1.5;
    ALFParam *alfParam   = alfPictureParam[compIdx];
    AlfCorrData* picCorr = new AlfCorrData(compIdx);

    if(!useAllLCUs)
    {
      Int numStatLCU =0;
      for(Int addr=0; addr < m_uiNumCUsInFrame; addr++)
      {
        if(m_pcPic->getCU(addr)->getAlfLCUEnabled(compIdx))
        {
          numStatLCU++;
          break;
        }
      }
      if(numStatLCU ==0)
      {
        useAllLCUs = true;
      }
    }

    for(Int addr=0; addr< (Int)m_uiNumCUsInFrame; addr++)
    {
      if(useAllLCUs || (m_pcPic->getCU(addr)->getAlfLCUEnabled(compIdx)) )
      {
        *picCorr += *(lcuCorr[compIdx][addr]);
      }
    }

    Int64 offDist = estimateFilterDistortion(compIdx, picCorr);
    alfParam->alf_flag = 1;
    deriveFilterInfo(compIdx, picCorr, alfParam, NO_VAR_BINS, lambda);
    Int64 dist = estimateFilterDistortion(compIdx, picCorr, m_filterCoeffSym, alfParam->filters_per_group, m_varIndTab, false);
    UInt rate = ALFParamBitrateEstimate(alfParam);
    Double cost = dist - offDist + lambda * rate;
    if(cost >= 0)
    {
      alfParam->alf_flag = 0;
    }
    delete picCorr;
  }
}

Void TEncAdaptiveLoopFilter::estimateLcuControl(ALFParam** alfPictureParam)
{
  for(Int compIdx = 0; compIdx < NUM_ALF_COMPONENT; compIdx++)
  {
    if(alfPictureParam[compIdx]->alf_flag == 1)
    {
      reconstructCoefInfo(compIdx, alfPictureParam[compIdx], m_filterCoeffSym, m_varIndTab);
      for(Int addr = 0; addr < m_uiNumCUsInFrame; addr++)
      {
        Int64 offDist = estimateFilterDistortion(compIdx, m_alfCorr[compIdx][addr]);
        Int64 dist    = estimateFilterDistortion(compIdx, m_alfCorr[compIdx][addr], m_filterCoeffSym, alfPictureParam[compIdx]->filters_per_group, m_varIndTab, false);
        m_pcPic->getCU(addr)->setAlfLCUEnabled( ((offDist <= dist)?0:1) , compIdx);
      }
    }
  }
}

/** Decide ALF parameter set for luma/chroma components (top function) 
 * \param [in] pPicOrg picture buffer for original picture
 * \param [in] pPicDec picture buffer for un-filtered picture 
 * \param [out] pPicRest picture buffer for reconstructed picture
 * \param [in, out] alfParamSet ALF parameter set
 * \param [in, out] alfCtrlParam ALF CU-on/off control parameters
 */
Void TEncAdaptiveLoopFilter::decideParameters(ALFParam** alfPictureParam, TComPicYuv* pPicOrg, TComPicYuv* pPicDec, TComPicYuv* pPicRest)
{
  static Int lumaStride        = pPicOrg->getStride();
  static Int chromaStride      = pPicOrg->getCStride();

  Pel *pOrg, *pDec, *pRest;
  Int stride, formatShift;

  setCurAlfParam(alfPictureParam);
  for(Int compIdx = 0; compIdx < NUM_ALF_COMPONENT; compIdx++)
  {
    pOrg        = getPicBuf(pPicOrg, compIdx);
    pDec        = getPicBuf(pPicDec, compIdx);
    pRest       = getPicBuf(pPicRest, compIdx);
    stride      = (compIdx == ALF_Y)?(lumaStride):(chromaStride);
    formatShift = (compIdx == ALF_Y)?(0):(1);

    AlfCorrData** alfCorrComp     = m_alfCorr[compIdx];
    executeLCUOnOffDecision(compIdx, alfPictureParam[compIdx], pOrg, pDec, pRest, stride, formatShift, alfCorrComp);
  } //component

}

/** Gather correlations for all LCUs in picture
 * \param [in] pPicOrg picture buffer for original picture
 * \param [in] pPicDec picture buffer for un-filtered picture 
 */
Void TEncAdaptiveLoopFilter::getStatistics(TComPicYuv* pPicOrg, TComPicYuv* pPicDec)
{
  Int lumaStride   = pPicOrg->getStride();
  Int chromaStride = pPicOrg->getCStride();
  const  Int chromaFormatShift = 1;
  for(Int compIdx = 0; compIdx < NUM_ALF_COMPONENT; compIdx++)
  {
    AlfCorrData** alfCorrComp = m_alfCorr[compIdx];
    Int          formatShift = (compIdx == ALF_Y)?(0):(chromaFormatShift);
    Int          stride      = (compIdx == ALF_Y)?(lumaStride):(chromaStride);
    getOneCompStatistics(alfCorrComp, compIdx, getPicBuf(pPicOrg, compIdx), getPicBuf(pPicDec, compIdx), stride, formatShift);
  } 
}

/** Gather correlations for all LCUs of one luma/chroma component in picture
 * \param [out] alfCorrComp correlations for LCUs
 * \param [in] compIdx luma/chroma component index
 * \param [in] imgOrg picture buffer for original picture
 * \param [in] imgDec picture buffer for un-filtered picture 
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] formatShift 0 for luma and 1 for chroma (4:2:0)
 * \param [in] isRedesignPhase at re-design filter stage (true) or not (false)
 */
Void TEncAdaptiveLoopFilter::getOneCompStatistics(AlfCorrData** alfCorrComp, Int compIdx, Pel* imgOrg, Pel* imgDec, Int stride, Int formatShift)
{

  // initialize to zero
  for(Int n=0; n< m_uiNumCUsInFrame; n++)
  {
    alfCorrComp[n]->reset();
    if(m_alfLowLatencyEncoding)
    {
      m_alfNonSkippedCorr[compIdx][n]->reset();
    }
  }

  for(Int s=0; s<= m_lastSliceIdx; s++)
  {
    if(!m_pcPic->getValidSlice(s))
    {
      continue;
    }
    Int numTilesInSlice = (Int)m_pvpSliceTileAlfLCU[s].size();
    for(Int t=0; t< numTilesInSlice; t++)
    {
      std::vector<AlfLCUInfo*> & vpAlfLCU = m_pvpSliceTileAlfLCU[s][t];
      Pel* pSrc = imgDec;

      if(m_bUseNonCrossALF)
      {
        pSrc = getPicBuf(m_pcSliceYuvTmp, compIdx);
        copyRegion(vpAlfLCU, pSrc, imgDec, stride, formatShift);
        extendRegionBorder(vpAlfLCU, pSrc, stride, formatShift);
      }

      Int numLCUs = (Int)vpAlfLCU.size();
      for(Int n=0; n< numLCUs; n++)
      {
        AlfLCUInfo* alfLCU = vpAlfLCU[n];
        Int addr = alfLCU->pcCU->getAddr();
        getStatisticsOneLCU(m_alfLowLatencyEncoding, compIdx, alfLCU, alfCorrComp[addr], imgOrg, pSrc, stride, formatShift);
        if(m_alfLowLatencyEncoding)
        {
          getStatisticsOneLCU( false, compIdx, alfLCU, m_alfNonSkippedCorr[compIdx][addr], imgOrg, pSrc, stride, formatShift);
        }
      } //LCU
    } //tile
  } //slice

}

/** Gather correlations for one LCU
 * \param [out] alfCorrComp correlations for LCUs
 * \param [in] compIdx luma/chroma component index
 * \param [in] imgOrg picture buffer for original picture
 * \param [in] imgDec picture buffer for un-filtered picture 
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] formatShift 0 for luma and 1 for chroma (4:2:0)
 * \param [in] isRedesignPhase at re-design filter stage (true) or not (false)
 */
Void TEncAdaptiveLoopFilter::getStatisticsOneLCU(Bool skipLCUBottomLines, Int compIdx, AlfLCUInfo* alfLCU, AlfCorrData* alfCorr, Pel* pPicOrg, Pel* pPicSrc, Int stride, Int formatShift)
{
  Int numBlocks = alfLCU->numSGU;
#if LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER || LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
  Int  lcuAddr = alfLCU->pcCU->getAddr();
  Bool notSkipLinesBelowVB = true;
  Int  endypos;
#endif
#if LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
  Bool notSkipLinesRightVB = true;
  Int endxpos;
#endif
  Bool isLastBlock;
  Int ypos, xpos, height, width;

#if LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER
  if(skipLCUBottomLines)
  {
    if(lcuAddr + m_numLCUInPicWidth < m_uiNumCUsInFrame)
    {
      notSkipLinesBelowVB = false;
    }
  }
#endif
#if LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
  if(skipLCUBottomLines)
  {
    if((lcuAddr + 1) % m_numLCUInPicWidth != 0 )
    {
      notSkipLinesRightVB = false;
    }
  }
#endif
  switch(compIdx)
  {
  case ALF_Cb:
  case ALF_Cr:
    {
      for(Int n=0; n< numBlocks; n++)
      {
        isLastBlock = (n== numBlocks-1);
        NDBFBlockInfo& AlfSGU = (*alfLCU)[n];

        ypos   = (Int)(AlfSGU.posY  >> formatShift);
        xpos   = (Int)(AlfSGU.posX  >> formatShift);
        height = (Int)(AlfSGU.height>> formatShift);
        width  = (Int)(AlfSGU.width >> formatShift);

#if LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER
        if(!notSkipLinesBelowVB )
        {
          endypos = ypos+ height -1;
          Int iLineVBPos = m_lcuHeightChroma - 2;
          Int yEndLineInLCU = endypos % m_lcuHeightChroma;
          height = (yEndLineInLCU >= iLineVBPos) ? (height - 2) : height ; 
        }
#endif
#if LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
        if( !notSkipLinesRightVB)
        {
          endxpos = xpos+ width -1;
          Int iLineVBPos = m_lcuWidthChroma - 7;
          Int xEndLineInLCU = endxpos % m_lcuWidthChroma;
          width = (xEndLineInLCU >= iLineVBPos) ? (width - 7) : width ; 
        }
#endif
        calcCorrOneCompRegionChma(pPicOrg, pPicSrc, stride, ypos, xpos, height, width, alfCorr->ECorr[0], alfCorr->yCorr[0], isLastBlock);
      }
    }
    break;
  case ALF_Y:
    {
      for(Int n=0; n< numBlocks; n++)
      {
        isLastBlock = (n== numBlocks-1);
        NDBFBlockInfo& AlfSGU = (*alfLCU)[n];

        ypos   = (Int)(AlfSGU.posY  );
        xpos   = (Int)(AlfSGU.posX  );
        height = (Int)(AlfSGU.height);
        width  = (Int)(AlfSGU.width );

#if LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER
        endypos = ypos+ height -1;
        if(!notSkipLinesBelowVB)
        {
          Int iLineVBPos = m_lcuHeight - 4;
          Int yEndLineInLCU = endypos % m_lcuHeight;
          height = (yEndLineInLCU >= iLineVBPos) ? (height - 4) : height ; 
        }
#endif
#if LCUALF_AVOID_USING_RIGHT_LINES_ENCODER
        if( !notSkipLinesRightVB)
        {
          endxpos = xpos+ width -1;
          Int iLineVBPos = m_lcuWidth - 9;
          Int xEndLineInLCU = endxpos % m_lcuWidth;
          width = (xEndLineInLCU >= iLineVBPos) ? (width - 9) : width ; 
        }
#endif
        calcCorrOneCompRegionLuma(pPicOrg, pPicSrc, stride, ypos, xpos, height, width, alfCorr->ECorr, alfCorr->yCorr, alfCorr->pixAcc, isLastBlock);
      }
    }
    break;
  default:
    {
      printf("Not a legal component index for ALF\n");
      assert(0);
      exit(-1);
    }
  }
}


/** Gather correlations for one region for chroma component
 * \param [in] imgOrg picture buffer for original picture
 * \param [in] imgPad picture buffer for un-filtered picture 
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] yPos region starting y position
 * \param [in] xPos region starting x position
 * \param [in] height region height
 * \param [in] width region width
 * \param [out] eCorr auto-correlation matrix
 * \param [out] yCorr cross-correlation array
 * \param [in] isSymmCopyBlockMatrix symmetrically copy correlation values in eCorr (true) or not (false)
 */
Void TEncAdaptiveLoopFilter::calcCorrOneCompRegionChma(Pel* imgOrg, Pel* imgPad, Int stride 
                                                     , Int yPos, Int xPos, Int height, Int width
                                                     , Double **eCorr, Double *yCorr, Bool isSymmCopyBlockMatrix
                                                      )
{
  Int yPosEnd = yPos + height;
  Int xPosEnd = xPos + width;
  Int N = ALF_MAX_NUM_COEF; //m_sqrFiltLengthTab[0];

  Int imgHeightChroma = m_img_height>>1;

  Int yLineInLCU, paddingLine;
  Int ELocal[ALF_MAX_NUM_COEF];
  Pel *imgPad1, *imgPad2, *imgPad3, *imgPad4, *imgPad5, *imgPad6;
  Int i, j, k, l, yLocal;

  imgPad += (yPos*stride);
  imgOrg += (yPos*stride);

  for (i= yPos; i< yPosEnd; i++)
  {
    yLineInLCU = i % m_lcuHeightChroma;

    if (yLineInLCU==0 && i>0)
    {
      paddingLine = yLineInLCU + 2 ;
      imgPad1 = imgPad + stride;
      imgPad2 = imgPad - stride;
      imgPad3 = imgPad + 2*stride;
      imgPad4 = imgPad - 2*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + 3*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - min(paddingLine, 3)*stride;;
    }
    else if (yLineInLCU < m_lineIdxPadBotChroma || i-yLineInLCU+m_lcuHeightChroma >= imgHeightChroma )
    {
      imgPad1 = imgPad + stride;
      imgPad2 = imgPad - stride;
      imgPad3 = imgPad + 2*stride;
      imgPad4 = imgPad - 2*stride;
      imgPad5 = imgPad + 3*stride;
      imgPad6 = imgPad - 3*stride;
    }
    else if (yLineInLCU < m_lineIdxPadTopChroma)
    {
      paddingLine = - yLineInLCU + m_lineIdxPadTopChroma - 1;
      imgPad1 = (paddingLine < 1) ? imgPad : imgPad + min(paddingLine, 1)*stride;
      imgPad2 = (paddingLine < 1) ? imgPad : imgPad - stride;
      imgPad3 = (paddingLine < 2) ? imgPad : imgPad + min(paddingLine, 2)*stride;
      imgPad4 = (paddingLine < 2) ? imgPad : imgPad - 2*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + min(paddingLine, 3)*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - 3*stride;
    }
    else
    {
      paddingLine = yLineInLCU - m_lineIdxPadTopChroma ;
      imgPad1 = (paddingLine < 1) ? imgPad : imgPad + stride;
      imgPad2 = (paddingLine < 1) ? imgPad : imgPad - min(paddingLine, 1)*stride;
      imgPad3 = (paddingLine < 2) ? imgPad : imgPad + 2*stride;
      imgPad4 = (paddingLine < 2) ? imgPad : imgPad - min(paddingLine, 2)*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + 3*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - min(paddingLine, 3)*stride;
    }

    for (j= xPos; j< xPosEnd; j++)
    {
      memset(ELocal, 0, N*sizeof(Int));

      ELocal[0] = (imgPad5[j] + imgPad6[j]);

      ELocal[1] = (imgPad3[j] + imgPad4[j]);

      ELocal[2] = (imgPad1[j+1] + imgPad2[j-1]);
      ELocal[3] = (imgPad1[j  ] + imgPad2[j  ]);
      ELocal[4] = (imgPad1[j-1] + imgPad2[j+1]);

      ELocal[5] = (imgPad[j+4] + imgPad[j-4]);
      ELocal[6] = (imgPad[j+3] + imgPad[j-3]);
      ELocal[7] = (imgPad[j+2] + imgPad[j-2]);
      ELocal[8] = (imgPad[j+1] + imgPad[j-1]);
      ELocal[9] = (imgPad[j  ]);

      yLocal= (Int)imgOrg[j];

      for(k=0; k<N; k++)
      {
        eCorr[k][k] += ELocal[k]*ELocal[k];
        for(l=k+1; l<N; l++)
        {
          eCorr[k][l] += ELocal[k]*ELocal[l];
        }

        yCorr[k] += yLocal*ELocal[k];
      }
    }

    imgPad+= stride;
    imgOrg+= stride;
  }

  if(isSymmCopyBlockMatrix)
  {
    for(j=0; j<N-1; j++)
    {
      for(i=j+1; i<N; i++)
      {
        eCorr[i][j] = eCorr[j][i];
      }
    }
  }
}

/** Gather correlations for one region for luma component
 * \param [in] imgOrg picture buffer for original picture
 * \param [in] imgPad picture buffer for un-filtered picture 
 * \param [in] stride buffer stride size for 1-D pictrue memory
 * \param [in] yPos region starting y position
 * \param [in] xPos region starting x position
 * \param [in] height region height
 * \param [in] width region width
 * \param [out] eCorr auto-correlation matrix
 * \param [out] yCorr cross-correlation array
 * \param [out] pixAcc pixel squared value
 * \param [in] isforceCollection all pixel are used for correlation calculation (true) or not (false)
 * \param [in] isSymmCopyBlockMatrix symmetrically copy correlation values in eCorr (true) or not (false)
 */
Void TEncAdaptiveLoopFilter::calcCorrOneCompRegionLuma(Pel* imgOrg, Pel* imgPad, Int stride,Int yPos, Int xPos, Int height, Int width
                                                      ,Double ***eCorr, Double **yCorr, Double *pixAcc
                                                      ,Bool isSymmCopyBlockMatrix
                                                      )

{
  Int yPosEnd = yPos + height;
  Int xPosEnd = xPos + width;
  Int yLineInLCU;
  Int paddingLine ;
  Int N = ALF_MAX_NUM_COEF; //m_sqrFiltLengthTab[0];

  Int ELocal[ALF_MAX_NUM_COEF];
  Pel *imgPad1, *imgPad2, *imgPad3, *imgPad4, *imgPad5, *imgPad6;
  Int i, j, k, l, yLocal, varInd;
  Double **E;
  Double *yy;

  imgPad += (yPos*stride);
  imgOrg += (yPos*stride);

  for (i= yPos; i< yPosEnd; i++)
  {
    yLineInLCU = i % m_lcuHeight;

    if (yLineInLCU<m_lineIdxPadBot || i-yLineInLCU+m_lcuHeight >= m_img_height)
    {
      imgPad1 = imgPad + stride;
      imgPad2 = imgPad - stride;
      imgPad3 = imgPad + 2*stride;
      imgPad4 = imgPad - 2*stride;
      imgPad5 = imgPad + 3*stride;
      imgPad6 = imgPad - 3*stride;
    }
    else if (yLineInLCU<m_lineIdxPadTop)
    {
      paddingLine = - yLineInLCU + m_lineIdxPadTop - 1;
      imgPad1 = (paddingLine < 1) ? imgPad : imgPad + min(paddingLine, 1)*stride;
      imgPad2 = (paddingLine < 1) ? imgPad : imgPad - stride;
      imgPad3 = (paddingLine < 2) ? imgPad : imgPad + min(paddingLine, 2)*stride;
      imgPad4 = (paddingLine < 2) ? imgPad : imgPad - 2*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + min(paddingLine, 3)*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - 3*stride;
    }
    else
    {
      paddingLine = yLineInLCU - m_lineIdxPadTop;
      imgPad1 = (paddingLine < 1) ? imgPad : imgPad + stride;
      imgPad2 = (paddingLine < 1) ? imgPad : imgPad - min(paddingLine, 1)*stride;
      imgPad3 = (paddingLine < 2) ? imgPad : imgPad + 2*stride;
      imgPad4 = (paddingLine < 2) ? imgPad : imgPad - min(paddingLine, 2)*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + 3*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - min(paddingLine, 3)*stride;
    }         

    for (j= xPos; j< xPosEnd; j++)
    {
        varInd = m_varImg[i/VAR_SIZE_H][j/VAR_SIZE_W];
        memset(ELocal, 0, N*sizeof(Int));

        ELocal[0] = (imgPad5[j] + imgPad6[j]);
        ELocal[1] = (imgPad3[j] + imgPad4[j]);

        ELocal[2] = (imgPad1[j+1] + imgPad2[j-1]);
        ELocal[3] = (imgPad1[j  ] + imgPad2[j  ]);
        ELocal[4] = (imgPad1[j-1] + imgPad2[j+1]);

        ELocal[5] = (imgPad[j+4] + imgPad[j-4]);
        ELocal[6] = (imgPad[j+3] + imgPad[j-3]);
        ELocal[7] = (imgPad[j+2] + imgPad[j-2]);
        ELocal[8] = (imgPad[j+1] + imgPad[j-1]);
        ELocal[9] = (imgPad[j  ]);

        yLocal= imgOrg[j];
        pixAcc[varInd] += (yLocal*yLocal);
        E  = eCorr[varInd];
        yy = yCorr[varInd];

        for (k=0; k<N; k++)
        {
          for (l=k; l<N; l++)
          {
            E[k][l]+=(double)(ELocal[k]*ELocal[l]);
          }
          yy[k]+=(double)(ELocal[k]*yLocal);
        }
    }
    imgPad += stride;
    imgOrg += stride;
  }

  if(isSymmCopyBlockMatrix)
  {
    for (varInd=0; varInd<NO_VAR_BINS; varInd++)
    {
      E = eCorr[varInd];
      for (k=1; k<N; k++)
      {
        for (l=0; l<k; l++)
        {
          E[k][l] = E[l][k];
        }
      }
    }
  }

}

/** PCM LF disable process.
 * \param pcPic picture (TComPic) pointer
 * \returns Void
 *
 * \note Replace filtered sample values of PCM mode blocks with the transmitted and reconstructed ones.
 */
Void TEncAdaptiveLoopFilter::PCMLFDisableProcess (TComPic* pcPic)
{
  xPCMRestoration(pcPic);
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

// ====================================================================================================================
// Private member functions
// ====================================================================================================================

#if IBDI_DISTORTION
UInt64 TEncAdaptiveLoopFilter::xCalcSSD(Pel* pOrg, Pel* pCmp, Int iWidth, Int iHeight, Int iStride )
{
  UInt64 uiSSD = 0;
  Int x, y;

  Int iShift = g_uiBitIncrement;
  Int iOffset = (g_uiBitIncrement>0)? (1<<(g_uiBitIncrement-1)):0;
  Int iTemp;

  for( y = 0; y < iHeight; y++ )
  {
    for( x = 0; x < iWidth; x++ )
    {
      iTemp = ((pOrg[x]+iOffset)>>iShift) - ((pCmp[x]+iOffset)>>iShift); uiSSD += iTemp * iTemp;
    }
    pOrg += iStride;
    pCmp += iStride;
  }

  return uiSSD;;
}
#else
UInt64 TEncAdaptiveLoopFilter::xCalcSSD(Pel* pOrg, Pel* pCmp, Int iWidth, Int iHeight, Int iStride )
{
  UInt64 uiSSD = 0;
  Int x, y;
  
  UInt uiShift = g_uiBitIncrement<<1;
  Int iTemp;
  
  for( y = 0; y < iHeight; y++ )
  {
    for( x = 0; x < iWidth; x++ )
    {
      iTemp = pOrg[x] - pCmp[x]; uiSSD += ( iTemp * iTemp ) >> uiShift;
    }
    pOrg += iStride;
    pCmp += iStride;
  }
  
  return uiSSD;;
}
#endif

Int TEncAdaptiveLoopFilter::xGauss(Double **a, Int N)
{
  Int i, j, k;
  Double t;
  
  for(k=0; k<N; k++)
  {
    if (a[k][k] <0.000001)
    {
      return 1;
    }
  }
  
  for(k=0; k<N-1; k++)
  {
    for(i=k+1;i<N; i++)
    {
      t=a[i][k]/a[k][k];
      for(j=k+1; j<=N; j++)
      {
        a[i][j] -= t * a[k][j];
        if(i==j && fabs(a[i][j])<0.000001) return 1;
      }
    }
  }
  for(i=N-1; i>=0; i--)
  {
    t = a[i][N];
    for(j=i+1; j<N; j++)
    {
      t -= a[i][j] * a[j][N];
    }
    a[i][N] = t / a[i][i];
  }
  return 0;
}

Void TEncAdaptiveLoopFilter::xFilterCoefQuickSort( Double *coef_data, Int *coef_num, Int upper, Int lower )
{
  Double mid, tmp_data;
  Int i, j, tmp_num;
  
  i = upper;
  j = lower;
  mid = coef_data[(lower+upper)>>1];
  do
  {
    while( coef_data[i] < mid ) i++;
    while( mid < coef_data[j] ) j--;
    if( i <= j )
    {
      tmp_data = coef_data[i];
      tmp_num  = coef_num[i];
      coef_data[i] = coef_data[j];
      coef_num[i]  = coef_num[j];
      coef_data[j] = tmp_data;
      coef_num[j]  = tmp_num;
      i++;
      j--;
    }
  } while( i <= j );
  if ( upper < j ) 
  {
    xFilterCoefQuickSort(coef_data, coef_num, upper, j);
  }
  if ( i < lower ) 
  {
    xFilterCoefQuickSort(coef_data, coef_num, i, lower);
  }
}

Void TEncAdaptiveLoopFilter::xQuantFilterCoef(Double* h, Int* qh, Int tap, int bit_depth)
{
  Int i, N;
  Int max_value, min_value;
  Double dbl_total_gain;
  Int total_gain, q_total_gain;
  Int upper, lower;
  Double *dh;
  Int    *nc;
  const Int    *pFiltMag;
  N = (Int)ALF_MAX_NUM_COEF;
  pFiltMag = weightsShape1Sym;

  dh = new Double[N];
  nc = new Int[N];
  max_value =   (1<<(1+ALF_NUM_BIT_SHIFT))-1;
  min_value = 0-(1<<(1+ALF_NUM_BIT_SHIFT));
  dbl_total_gain=0.0;
  q_total_gain=0;
  for(i=0; i<N; i++)
  {
    if(h[i]>=0.0)
    {
      qh[i] =  (Int)( h[i]*(1<<ALF_NUM_BIT_SHIFT)+0.5);
    }
    else
    {
      qh[i] = -(Int)(-h[i]*(1<<ALF_NUM_BIT_SHIFT)+0.5);
    }
    dh[i] = (Double)qh[i]/(Double)(1<<ALF_NUM_BIT_SHIFT) - h[i];
    dh[i]*=pFiltMag[i];
    dbl_total_gain += h[i]*pFiltMag[i];
    q_total_gain   += qh[i]*pFiltMag[i];
    nc[i] = i;
  }
  
  // modification of quantized filter coefficients
  total_gain = (Int)(dbl_total_gain*(1<<ALF_NUM_BIT_SHIFT)+0.5);
  if( q_total_gain != total_gain )
  {
    xFilterCoefQuickSort(dh, nc, 0, N-1);
    if( q_total_gain > total_gain )
    {
      upper = N-1;
      while( q_total_gain > total_gain+1 )
      {
        i = nc[upper%N];
        qh[i]--;
        q_total_gain -= pFiltMag[i];
        upper--;
      }
      if( q_total_gain == total_gain+1 )
      {
        if(dh[N-1]>0)
        {
          qh[N-1]--;
        }
        else
        {
          i=nc[upper%N];
          qh[i]--;
          qh[N-1]++;
        }
      }
    }
    else if( q_total_gain < total_gain )
    {
      lower = 0;
      while( q_total_gain < total_gain-1 )
      {
        i=nc[lower%N];
        qh[i]++;
        q_total_gain += pFiltMag[i];
        lower++;
      }
      if( q_total_gain == total_gain-1 )
      {
        if(dh[N-1]<0)
        {
          qh[N-1]++;
        }
        else
        {
          i=nc[lower%N];
          qh[i]++;
          qh[N-1]--;
        }
      }
    }
  }
  
  // set of filter coefficients
  for(i=0; i<N; i++)
  {
    qh[i] = max(min_value,min(max_value, qh[i]));
  }

  checkFilterCoeffValue(qh, N, true);

  delete[] dh;
  dh = NULL;
  
  delete[] nc;
  nc = NULL;
}


double TEncAdaptiveLoopFilter::xfindBestCoeffCodMethod(int **filterCoeffSymQuant, int filter_shape, int sqrFiltLength, int filters_per_fr, double errorForce0CoeffTab[NO_VAR_BINS][2], 
  double lambda)
{
  Int coeffBits, i;
  Double error=0, lagrangian;
  static Bool  isFirst = true;
  static Int** coeffmulti = NULL;
  if(isFirst)
  {
    coeffmulti = new Int*[NO_VAR_BINS];
    for(Int g=0; g< NO_VAR_BINS; g++)
    {
      coeffmulti[g] = new Int[ALF_MAX_NUM_COEF];
    }
    isFirst = false;
  }

  for(Int g=0; g< filters_per_fr; g++)
  {
    for(i=0; i< sqrFiltLength; i++)
    {
      coeffmulti[g][i] = filterCoeffSymQuant[g][i];
    }
  }
  predictALFCoeff(coeffmulti, sqrFiltLength, filters_per_fr);
  //golomb encode bitrate estimation
  coeffBits = 0;
  for(Int g=0; g< filters_per_fr; g++)
  {
    coeffBits += filterCoeffBitrateEstimate(ALF_Y, coeffmulti[g]);
  }
  for(i=0;i<filters_per_fr;i++)
  {
    error += errorForce0CoeffTab[i][1];
  }
  lagrangian = error + lambda * coeffBits;
  return (lagrangian);
}

Void TEncAdaptiveLoopFilter::predictALFCoeff(Int** coeff, Int numCoef, Int numFilters)
{
  for(Int g=0; g< numFilters; g++ )
  {
    Int sum=0;
    for(Int i=0; i< numCoef-1;i++)
    {
      sum += (2* coeff[g][i]);
    }

    Int pred = (1<<ALF_NUM_BIT_SHIFT) - (sum);
    coeff[g][numCoef-1] = coeff[g][numCoef-1] - pred;
  }
}

Void TEncAdaptiveLoopFilter::xfindBestFilterVarPred(double **ySym, double ***ESym, double *pixAcc, Int **filterCoeffSym, Int *filters_per_fr_best, Int varIndTab[], double lambda_val, Int numMaxFilters)
{
  static Bool isFirst = true;
  static Int* filterCoeffSymQuant[NO_VAR_BINS];
  if(isFirst)
  {
    for(Int g=0; g< NO_VAR_BINS; g++)
    {
      filterCoeffSymQuant[g] = new Int[ALF_MAX_NUM_COEF];
    }
    isFirst = false;
  }
  Int filter_shape = 0;
  Int filters_per_fr, firstFilt, interval[NO_VAR_BINS][2], intervalBest[NO_VAR_BINS][2];
  int i;
  double  lagrangian, lagrangianMin;
  int sqrFiltLength;
  int *weights;
  double errorForce0CoeffTab[NO_VAR_BINS][2];
  
  sqrFiltLength= (Int)ALF_MAX_NUM_COEF;
  weights = weightsShape1Sym;
  // zero all variables 
  memset(varIndTab,0,sizeof(int)*NO_VAR_BINS);

  for(i = 0; i < NO_VAR_BINS; i++)
  {
    memset(filterCoeffSym[i],0,sizeof(int)*ALF_MAX_NUM_COEF);
    memset(filterCoeffSymQuant[i],0,sizeof(int)*ALF_MAX_NUM_COEF);
  }

  firstFilt=1;  lagrangianMin=0;
  filters_per_fr=NO_FILTERS;

  while(filters_per_fr>=1)
  {
    mergeFiltersGreedy(ySym, ESym, pixAcc, interval, sqrFiltLength, filters_per_fr);
    findFilterCoeff(ESym, ySym, pixAcc, filterCoeffSym, filterCoeffSymQuant, interval,
      varIndTab, sqrFiltLength, filters_per_fr, weights, errorForce0CoeffTab);

    lagrangian=xfindBestCoeffCodMethod(filterCoeffSymQuant, filter_shape, sqrFiltLength, filters_per_fr, errorForce0CoeffTab, lambda_val);
    if (lagrangian<lagrangianMin || firstFilt==1 || filters_per_fr == numMaxFilters)
    {
      firstFilt=0;
      lagrangianMin=lagrangian;

      (*filters_per_fr_best)=filters_per_fr;
      memcpy(intervalBest, interval, NO_VAR_BINS*2*sizeof(int));
    }
    filters_per_fr--;
  }
  findFilterCoeff(ESym, ySym, pixAcc, filterCoeffSym, filterCoeffSymQuant, intervalBest,
    varIndTab, sqrFiltLength, (*filters_per_fr_best), weights, errorForce0CoeffTab);

  if( *filters_per_fr_best == 1)
  {
    ::memset(varIndTab, 0, sizeof(Int)*NO_VAR_BINS);
  }
}

/** code filter coefficients
 * \param filterCoeffSymQuant filter coefficients buffer
 * \param filtNo filter No.
 * \param varIndTab[] merge index information
 * \param filters_per_fr_best the number of filters used in this picture
 * \param frNo 
 * \param ALFp ALF parameters
 * \returns bitrate
 */
Void TEncAdaptiveLoopFilter::xcodeFiltCoeff(Int **filterCoeff, Int* varIndTab, Int numFilters, ALFParam* alfParam)
{
  Int filterPattern[NO_VAR_BINS], startSecondFilter=0;
  ::memset(filterPattern, 0, NO_VAR_BINS * sizeof(Int)); 

  alfParam->filter_shape=0;
  alfParam->num_coeff = (Int)ALF_MAX_NUM_COEF;
  alfParam->filters_per_group = numFilters;

  //merge table assignment
  if(alfParam->filters_per_group > 1)
  {
    for(Int i = 1; i < NO_VAR_BINS; ++i)
    {
      if(varIndTab[i] != varIndTab[i-1])
      {
        filterPattern[i] = 1;
        startSecondFilter = i;
      }
    }
  }
  ::memcpy (alfParam->filterPattern, filterPattern, NO_VAR_BINS * sizeof(Int));
  alfParam->startSecondFilter = startSecondFilter;

  //coefficient prediction
  for(Int g=0; g< alfParam->filters_per_group; g++)
  {
    for(Int i=0; i< alfParam->num_coeff; i++)
    {
      alfParam->coeffmulti[g][i] = filterCoeff[g][i];
    }
  }
  predictALFCoeff(alfParam->coeffmulti, alfParam->num_coeff, alfParam->filters_per_group);
}

#define ROUND(a)  (((a) < 0)? (int)((a) - 0.5) : (int)((a) + 0.5))
#define REG              0.0001
#define REG_SQR          0.0000001

//Find filter coeff related
Int TEncAdaptiveLoopFilter::gnsCholeskyDec(Double **inpMatr, Double outMatr[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], Int noEq)
{ 
  Int i, j, k;     /* Looping Variables */
  Double scale;       /* scaling factor for each row */
  Double invDiag[ALF_MAX_NUM_COEF];  /* Vector of the inverse of diagonal entries of outMatr */
  
  //  Cholesky decomposition starts
  
  for(i = 0; i < noEq; i++)
  {
    for(j = i; j < noEq; j++)
    {
      /* Compute the scaling factor */
      scale = inpMatr[i][j];
      if ( i > 0) 
      {
        for( k = i - 1 ; k >= 0 ; k--)
        {
          scale -= outMatr[k][j] * outMatr[k][i];
        }
      }
      /* Compute i'th row of outMatr */
      if(i == j)
      {
        if(scale <= REG_SQR ) // if(scale <= 0 )  /* If inpMatr is singular */
        {
          return 0;
        }
        else
        {
           /* Normal operation */
           invDiag[i] =  1.0 / (outMatr[i][i] = sqrt(scale));
        }
      }
      else
      {
        outMatr[i][j] = scale * invDiag[i]; /* Upper triangular part          */
        outMatr[j][i] = 0.0;              /* Lower triangular part set to 0 */
      }                    
    }
  }
  return 1; /* Signal that Cholesky factorization is successfully performed */
}


Void TEncAdaptiveLoopFilter::gnsTransposeBacksubstitution(Double U[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], Double rhs[], Double x[], Int order)
{
  Int i,j;              /* Looping variables */
  Double sum;              /* Holds backsubstitution from already handled rows */
  
  /* Backsubstitution starts */
  x[0] = rhs[0] / U[0][0];               /* First row of U'                   */
  for (i = 1; i < order; i++)
  {         /* For the rows 1..order-1           */
    
    for (j = 0, sum = 0.0; j < i; j++) /* Backsubst already solved unknowns */
    {
      sum += x[j] * U[j][i];
    }
    x[i] = (rhs[i] - sum) / U[i][i];       /* i'th component of solution vect.  */
  }
}
Void  TEncAdaptiveLoopFilter::gnsBacksubstitution(Double R[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], Double z[ALF_MAX_NUM_COEF], Int R_size, Double A[ALF_MAX_NUM_COEF])
{
  Int i, j;
  Double sum;
  
  R_size--;
  
  A[R_size] = z[R_size] / R[R_size][R_size];
  
  for (i = R_size-1; i >= 0; i--)
  {
    for (j = i + 1, sum = 0.0; j <= R_size; j++)
    {
      sum += R[i][j] * A[j];
    }
    
    A[i] = (z[i] - sum) / R[i][i];
  }
}


Int TEncAdaptiveLoopFilter::gnsSolveByChol(Double **LHS, Double *rhs, Double *x, Int noEq)
{
  assert(noEq > 0);

  Double aux[ALF_MAX_NUM_COEF];     /* Auxiliary vector */
  Double U[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF];    /* Upper triangular Cholesky factor of LHS */
  Int  i, singular;          /* Looping variable */
  
  /* The equation to be solved is LHSx = rhs */
  
  /* Compute upper triangular U such that U'*U = LHS */
  if(gnsCholeskyDec(LHS, U, noEq)) /* If Cholesky decomposition has been successful */
  {
    singular = 1;
    /* Now, the equation is  U'*U*x = rhs, where U is upper triangular
     * Solve U'*aux = rhs for aux
     */
    gnsTransposeBacksubstitution(U, rhs, aux, noEq);         
    
    /* The equation is now U*x = aux, solve it for x (new motion coefficients) */
    gnsBacksubstitution(U, aux, noEq, x);   
    
  }
  else /* LHS was singular */ 
  {
    singular = 0;
    
    /* Regularize LHS */
    for(i=0; i < noEq; i++)
    {
      LHS[i][i] += REG;
    }
    /* Compute upper triangular U such that U'*U = regularized LHS */
    singular = gnsCholeskyDec(LHS, U, noEq);
    if ( singular == 1 )
    {
      /* Solve  U'*aux = rhs for aux */  
      gnsTransposeBacksubstitution(U, rhs, aux, noEq);   
      
      /* Solve U*x = aux for x */
      gnsBacksubstitution(U, aux, noEq, x);      
    }
    else
    {
      x[0] = 1.0;
      for (i = 1; i < noEq; i++ )
      {
        x[i] = 0.0;
      }
    }
  }  
  return singular;
}

Void TEncAdaptiveLoopFilter::add_A(Double **Amerged, Double ***A, Int start, Int stop, Int size)
{ 
  Int i, j, ind;          /* Looping variable */
  
  for (i = 0; i < size; i++)
  {
    for (j = 0; j < size; j++)
    {
      Amerged[i][j] = 0;
      for (ind = start; ind <= stop; ind++)
      {
        Amerged[i][j] += A[ind][i][j];
      }
    }
  }
}

Void TEncAdaptiveLoopFilter::add_b(Double *bmerged, Double **b, Int start, Int stop, Int size)
{ 
  Int i, ind;          /* Looping variable */
  
  for (i = 0; i < size; i++)
  {
    bmerged[i] = 0;
    for (ind = start; ind <= stop; ind++)
    {
      bmerged[i] += b[ind][i];
    }
  }
}

Double TEncAdaptiveLoopFilter::calculateErrorCoeffProvided(Double **A, Double *b, Double *c, Int size)
{
  Int i, j;
  Double error, sum = 0;
  
  error = 0;
  for (i = 0; i < size; i++)   //diagonal
  {
    sum = 0;
    for (j = i + 1; j < size; j++)
    {
      sum += (A[j][i] + A[i][j]) * c[j];
    }
    error += (A[i][i] * c[i] + sum - 2 * b[i]) * c[i];
  }
  
  return error;
}

Double TEncAdaptiveLoopFilter::calculateErrorAbs(Double **A, Double *b, Double y, Int size)
{
  Int i;
  Double error, sum;
  Double c[ALF_MAX_NUM_COEF];
  
  gnsSolveByChol(A, b, c, size);
  
  sum = 0;
  for (i = 0; i < size; i++)
  {
    sum += c[i] * b[i];
  }
  error = y - sum;
  
  return error;
}

Double TEncAdaptiveLoopFilter::mergeFiltersGreedy(Double **yGlobalSeq, Double ***EGlobalSeq, Double *pixAccGlobalSeq, Int intervalBest[NO_VAR_BINS][2], Int sqrFiltLength, Int noIntervals)
{
  Int first, ind, ind1, ind2, i, j, bestToMerge ;
  Double error, error1, error2, errorMin;
  static Double pixAcc_temp, error_tab[NO_VAR_BINS],error_comb_tab[NO_VAR_BINS];
  static Int indexList[NO_VAR_BINS], available[NO_VAR_BINS], noRemaining;
  if (noIntervals == NO_FILTERS)
  {
    noRemaining = NO_VAR_BINS;
    for (ind=0; ind<NO_VAR_BINS; ind++)
    {
      indexList[ind] = ind; 
      available[ind] = 1;
      m_pixAcc_merged[ind] = pixAccGlobalSeq[ind];
      memcpy(m_y_merged[ind], yGlobalSeq[ind], sizeof(Double)*sqrFiltLength);
      for (i=0; i < sqrFiltLength; i++)
      {
        memcpy(m_E_merged[ind][i], EGlobalSeq[ind][i], sizeof(Double)*sqrFiltLength);
      }
    }
  }
  // Try merging different matrices
  if (noIntervals == NO_FILTERS)
  {
    for (ind = 0; ind < NO_VAR_BINS; ind++)
    {
      error_tab[ind] = calculateErrorAbs(m_E_merged[ind], m_y_merged[ind], m_pixAcc_merged[ind], sqrFiltLength);
    }
    for (ind = 0; ind < NO_VAR_BINS - 1; ind++)
    {
      ind1 = indexList[ind];
      ind2 = indexList[ind+1];
      
      error1 = error_tab[ind1];
      error2 = error_tab[ind2];
      
      pixAcc_temp = m_pixAcc_merged[ind1] + m_pixAcc_merged[ind2];
      for (i = 0; i < sqrFiltLength; i++)
      {
        m_y_temp[i] = m_y_merged[ind1][i] + m_y_merged[ind2][i];
        for (j = 0; j < sqrFiltLength; j++)
        {
          m_E_temp[i][j] = m_E_merged[ind1][i][j] + m_E_merged[ind2][i][j];
        }
      }
      error_comb_tab[ind1] = calculateErrorAbs(m_E_temp, m_y_temp, pixAcc_temp, sqrFiltLength) - error1 - error2;
    }
  }
  while (noRemaining > noIntervals)
  {
    errorMin = 0; 
    first = 1;
    bestToMerge = 0;
    for (ind = 0; ind < noRemaining - 1; ind++)
    {
      error = error_comb_tab[indexList[ind]];
      if ((error < errorMin || first == 1))
      {
        errorMin = error;
        bestToMerge = ind;
        first = 0;
      }
    }
    ind1 = indexList[bestToMerge];
    ind2 = indexList[bestToMerge+1];
    m_pixAcc_merged[ind1] += m_pixAcc_merged[ind2];
    for (i = 0; i < sqrFiltLength; i++)
    {
      m_y_merged[ind1][i] += m_y_merged[ind2][i];
      for (j = 0; j < sqrFiltLength; j++)
      {
        m_E_merged[ind1][i][j] += m_E_merged[ind2][i][j];
      }
    }
    available[ind2] = 0;
    
    //update error tables
    error_tab[ind1] = error_comb_tab[ind1] + error_tab[ind1] + error_tab[ind2];
    if (indexList[bestToMerge] > 0)
    {
      ind1 = indexList[bestToMerge-1];
      ind2 = indexList[bestToMerge];
      error1 = error_tab[ind1];
      error2 = error_tab[ind2];
      pixAcc_temp = m_pixAcc_merged[ind1] + m_pixAcc_merged[ind2];
      for (i = 0; i < sqrFiltLength; i++)
      {
        m_y_temp[i] = m_y_merged[ind1][i] + m_y_merged[ind2][i];
        for (j = 0; j < sqrFiltLength; j++)
        {
          m_E_temp[i][j] = m_E_merged[ind1][i][j] + m_E_merged[ind2][i][j];
        }
      }
      error_comb_tab[ind1] = calculateErrorAbs(m_E_temp, m_y_temp, pixAcc_temp, sqrFiltLength) - error1 - error2;
    }
    if (indexList[bestToMerge+1] < NO_VAR_BINS - 1)
    {
      ind1 = indexList[bestToMerge];
      ind2 = indexList[bestToMerge+2];
      error1 = error_tab[ind1];
      error2 = error_tab[ind2];
      pixAcc_temp = m_pixAcc_merged[ind1] + m_pixAcc_merged[ind2];
      for (i=0; i<sqrFiltLength; i++)
      {
        m_y_temp[i] = m_y_merged[ind1][i] + m_y_merged[ind2][i];
        for (j=0; j < sqrFiltLength; j++)
        {
          m_E_temp[i][j] = m_E_merged[ind1][i][j] + m_E_merged[ind2][i][j];
        }
      }
      error_comb_tab[ind1] = calculateErrorAbs(m_E_temp, m_y_temp, pixAcc_temp, sqrFiltLength) - error1 - error2;
    }
    
    ind=0;
    for (i = 0; i < NO_VAR_BINS; i++)
    {
      if (available[i] == 1)
      {
        indexList[ind] = i;
        ind++;
      }
    }
    noRemaining--;
  }
  
  errorMin = 0;
  for (ind = 0; ind < noIntervals; ind++)
  {
    errorMin += error_tab[indexList[ind]];
  }
  
  for (ind = 0; ind < noIntervals - 1; ind++)
  {
    intervalBest[ind][0] = indexList[ind]; 
    intervalBest[ind][1] = indexList[ind+1] - 1;
  }
  
  intervalBest[noIntervals-1][0] = indexList[noIntervals-1]; 
  intervalBest[noIntervals-1][1] = NO_VAR_BINS-1;
  
  return(errorMin);
}

Void TEncAdaptiveLoopFilter::roundFiltCoeff(Int *FilterCoeffQuan, Double *FilterCoeff, Int sqrFiltLength, Int factor)
{
  Int i;
  Double diff; 
  Int diffInt, sign; 
  
  for(i = 0; i < sqrFiltLength; i++)
  {
    sign = (FilterCoeff[i] > 0)? 1 : -1; 
    diff = FilterCoeff[i] * sign; 
    diffInt = (Int)(diff * (Double)factor + 0.5); 
    FilterCoeffQuan[i] = diffInt * sign;
  }
}

Double TEncAdaptiveLoopFilter::QuantizeIntegerFilterPP(Double *filterCoeff, Int *filterCoeffQuant, Double **E, Double *y, Int sqrFiltLength, Int *weights)
{
  double error;
  static Bool isFirst = true;
  static Int* filterCoeffQuantMod= NULL;
  if(isFirst)
  {
    filterCoeffQuantMod = new Int[ALF_MAX_NUM_COEF];
    isFirst = false;
  }
  Int factor = (1<<  ((Int)ALF_NUM_BIT_SHIFT)  );
  Int i;
  int quantCoeffSum, minInd, targetCoeffSumInt, k, diff;
  double targetCoeffSum, errMin;
  
  gnsSolveByChol(E, y, filterCoeff, sqrFiltLength);
  targetCoeffSum=0;
  for (i=0; i<sqrFiltLength; i++)
  {
    targetCoeffSum+=(weights[i]*filterCoeff[i]*factor);
  }
  targetCoeffSumInt=ROUND(targetCoeffSum);
  roundFiltCoeff(filterCoeffQuant, filterCoeff, sqrFiltLength, factor);
  quantCoeffSum=0;
  for (i=0; i<sqrFiltLength; i++)
  {
    quantCoeffSum+=weights[i]*filterCoeffQuant[i];
  }
  
  int count=0;
  while(quantCoeffSum!=targetCoeffSumInt && count < 10)
  {
    if (quantCoeffSum>targetCoeffSumInt)
    {
      diff=quantCoeffSum-targetCoeffSumInt;
      errMin=0; minInd=-1;
      for (k=0; k<sqrFiltLength; k++)
      {
        if (weights[k]<=diff)
        {
          for (i=0; i<sqrFiltLength; i++)
          {
            filterCoeffQuantMod[i]=filterCoeffQuant[i];
          }
          filterCoeffQuantMod[k]--;
          for (i=0; i<sqrFiltLength; i++)
          {
            filterCoeff[i]=(double)filterCoeffQuantMod[i]/(double)factor;
          }
          error=calculateErrorCoeffProvided(E, y, filterCoeff, sqrFiltLength);
          if (error<errMin || minInd==-1)
          {
            errMin=error;
            minInd=k;
          }
        } // if (weights(k)<=diff)
      } // for (k=0; k<sqrFiltLength; k++)
      filterCoeffQuant[minInd]--;
    }
    else
    {
      diff=targetCoeffSumInt-quantCoeffSum;
      errMin=0; minInd=-1;
      for (k=0; k<sqrFiltLength; k++)
      {
        if (weights[k]<=diff)
        {
          for (i=0; i<sqrFiltLength; i++)
          {
            filterCoeffQuantMod[i]=filterCoeffQuant[i];
          }
          filterCoeffQuantMod[k]++;
          for (i=0; i<sqrFiltLength; i++)
          {
            filterCoeff[i]=(double)filterCoeffQuantMod[i]/(double)factor;
          }
          error=calculateErrorCoeffProvided(E, y, filterCoeff, sqrFiltLength);
          if (error<errMin || minInd==-1)
          {
            errMin=error;
            minInd=k;
          }
        } // if (weights(k)<=diff)
      } // for (k=0; k<sqrFiltLength; k++)
      filterCoeffQuant[minInd]++;
    }
    
    quantCoeffSum=0;
    for (i=0; i<sqrFiltLength; i++)
    {
      quantCoeffSum+=weights[i]*filterCoeffQuant[i];
    }
  }
  if( count == 10 )
  {
    for (i=0; i<sqrFiltLength; i++)
    {
      filterCoeffQuant[i] = 0;
    }
  }
  
  checkFilterCoeffValue(filterCoeffQuant, sqrFiltLength, false);

  for (i=0; i<sqrFiltLength; i++)
  {
    filterCoeff[i]=(double)filterCoeffQuant[i]/(double)factor;
  }
  
  error=calculateErrorCoeffProvided(E, y, filterCoeff, sqrFiltLength);
  return(error);
}
Double TEncAdaptiveLoopFilter::findFilterCoeff(double ***EGlobalSeq, double **yGlobalSeq, double *pixAccGlobalSeq, int **filterCoeffSeq, int **filterCoeffQuantSeq, int intervalBest[NO_VAR_BINS][2], int varIndTab[NO_VAR_BINS], int sqrFiltLength, int filters_per_fr, int *weights, double errorTabForce0Coeff[NO_VAR_BINS][2])
{
  static double pixAcc_temp;
  static Bool isFirst = true;
  static Int* filterCoeffQuant = NULL;
  static Double* filterCoeff = NULL;
  if(isFirst)
  {
    filterCoeffQuant = new Int[ALF_MAX_NUM_COEF];
    filterCoeff = new Double[ALF_MAX_NUM_COEF];
    isFirst = false;
  }
  double error;
  int k, filtNo;
  
  error = 0;
  for(filtNo = 0; filtNo < filters_per_fr; filtNo++)
  {
    add_A(m_E_temp, EGlobalSeq, intervalBest[filtNo][0], intervalBest[filtNo][1], sqrFiltLength);
    add_b(m_y_temp, yGlobalSeq, intervalBest[filtNo][0], intervalBest[filtNo][1], sqrFiltLength);
    
    pixAcc_temp = 0;    
    for(k = intervalBest[filtNo][0]; k <= intervalBest[filtNo][1]; k++)
      pixAcc_temp += pixAccGlobalSeq[k];
    
    // Find coeffcients
    errorTabForce0Coeff[filtNo][1] = pixAcc_temp + QuantizeIntegerFilterPP(filterCoeff, filterCoeffQuant, m_E_temp, m_y_temp, sqrFiltLength, weights);
    errorTabForce0Coeff[filtNo][0] = pixAcc_temp;
    error += errorTabForce0Coeff[filtNo][1];
    
    for(k = 0; k < sqrFiltLength; k++)
    {
      filterCoeffSeq[filtNo][k] = filterCoeffQuant[k];
      filterCoeffQuantSeq[filtNo][k] = filterCoeffQuant[k];
    }
  }
  
  for(filtNo = 0; filtNo < filters_per_fr; filtNo++)
  {
    for(k = intervalBest[filtNo][0]; k <= intervalBest[filtNo][1]; k++)
      varIndTab[k] = filtNo;
  }
  
  return(error);
}


/** Estimate filtering distortion by correlation values and filter coefficients
 * \param ppdE auto-correlation matrix
 * \param pdy cross-correlation array
 * \param piCoeff  filter coefficients
 * \param iFiltLength numbr of filter taps
 * \returns estimated distortion
 */
Int64 TEncAdaptiveLoopFilter::xFastFiltDistEstimation(Double** ppdE, Double* pdy, Int* piCoeff, Int iFiltLength)
{
  //static memory
  Double pdcoeff[ALF_MAX_NUM_COEF];
  //variable
  Int    i,j;
  Int64  iDist;
  Double dDist, dsum;

  for(i=0; i< iFiltLength; i++)
  {
    pdcoeff[i]= (Double)piCoeff[i] / (Double)(1<< ((Int)ALF_NUM_BIT_SHIFT) );
  }

  dDist =0;
  for(i=0; i< iFiltLength; i++)
  {
    dsum= ((Double)ppdE[i][i]) * pdcoeff[i];
    for(j=i+1; j< iFiltLength; j++)
    {
      dsum += (Double)(2*ppdE[i][j])* pdcoeff[j];
    }

    dDist += ((dsum - 2.0 * pdy[i])* pdcoeff[i] );
  }


  UInt uiShift = g_uiBitIncrement<<1;
  if(dDist < 0)
  {
    iDist = -(((Int64)(-dDist + 0.5)) >> uiShift);
  }
  else //dDist >=0
  {
    iDist= ((Int64)(dDist+0.5)) >> uiShift;
  }

  return iDist;

}

UInt TEncAdaptiveLoopFilter::uvlcBitrateEstimate(Int val)
{
  val++;
  assert ( val );
  UInt length = 1;
  while( 1 != val )
  {
    val >>= 1;
    length += 2;
  }
  return ((length >> 1) + ((length+1) >> 1));
}

UInt TEncAdaptiveLoopFilter::golombBitrateEstimate(Int coeff, Int k)
{
  UInt symbol = (UInt)abs(coeff);
  UInt bitcnt = 0;

  while( symbol >= (UInt)(1<<k) )
  {
    bitcnt++;
    symbol -= (1<<k);
    k  ++;
  }
  bitcnt++;
  while( k-- )
  {
    bitcnt++;
  }
  if(coeff != 0)
  {
    bitcnt++;
  }
  return bitcnt;
}

UInt TEncAdaptiveLoopFilter::filterCoeffBitrateEstimate(Int compIdx, Int* coeff)
{
  UInt bitrate =0;
  for(Int i=0; i< (Int)ALF_MAX_NUM_COEF; i++)
  {
    bitrate += (compIdx == ALF_Y)?(golombBitrateEstimate(coeff[i], kTableTabShapes[ALF_CROSS9x7_SQUARE3x3][i])):(svlcBitrateEsitmate(coeff[i]));
  }
  return bitrate;
}

UInt TEncAdaptiveLoopFilter::ALFParamBitrateEstimate(ALFParam* alfParam)
{
  UInt bitrate = 1; //alf enabled flag
  if(alfParam->alf_flag == 1)
  {
    if(alfParam->componentID == ALF_Y)
    {
      Int noFilters = min(alfParam->filters_per_group-1, 2);
      bitrate += uvlcBitrateEstimate(noFilters);
      if(noFilters == 1)
      {
        bitrate += uvlcBitrateEstimate(alfParam->startSecondFilter);
      }
      else if (noFilters == 2)
      {
        bitrate += ((Int)NO_VAR_BINS -1);
      }
    }
    for(Int g=0; g< alfParam->filters_per_group; g++)
    {
      bitrate += filterCoeffBitrateEstimate(alfParam->componentID, alfParam->coeffmulti[g]);
    }
  }
  return bitrate;
}
#endif
//! \}
