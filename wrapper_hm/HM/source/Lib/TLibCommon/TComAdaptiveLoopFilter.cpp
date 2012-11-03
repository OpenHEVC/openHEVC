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

/** \file     TComAdaptiveLoopFilter.cpp
    \brief    adaptive loop filter class
*/

#include "TComAdaptiveLoopFilter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#if !REMOVE_ALF
//! \ingroup TLibCommon
//! \{


// ====================================================================================================================
// Tables
// ====================================================================================================================

Int TComAdaptiveLoopFilter::weightsShape1Sym[ALF_MAX_NUM_COEF+1] = 
{ 
              2,
              2,
           2, 2, 2,
  2, 2, 2, 2, 1, 
              1
};

Int depthIntShape1Sym[ALF_MAX_NUM_COEF+1] = 
{
              6,
              7,
           7, 8, 7,
  5, 6, 7, 8, 9, 
              9  
};
Int* pDepthIntTabShapes[NUM_ALF_FILTER_SHAPE] =
{ 
  depthIntShape1Sym
};

Int kTableShape1[ALF_MAX_NUM_COEF] = 
{      
              1,
              2,
           3, 4, 3,
  1, 3, 3, 5, 0, 
};

Int* kTableTabShapes[NUM_ALF_FILTER_SHAPE] =
{ 
  kTableShape1
};

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================
/// ALFParam
const ALFParam& ALFParam::operator= (const ALFParam& src)
{
  if(this->componentID < 0)
  {
    this->create(src.componentID);
  }
  this->copy(src);
  return *this;
}

Void ALFParam::create(Int cID)
{
  const Int numCoef = ALF_MAX_NUM_COEF;

  this->componentID       = cID;
  this->alf_flag          = 0;
  this->filters_per_group = 1; // this value keeps 1 for chroma componenet
  this->startSecondFilter = -1;
  this->filterPattern     = NULL;
  this->coeffmulti        = NULL;
  this->filter_shape      = 0;
  this->num_coeff         = numCoef;

  switch(cID)
  {
  case ALF_Y:
    {
      this->coeffmulti = new Int*[NO_VAR_BINS];
      for(Int i=0; i< NO_VAR_BINS; i++)
      {
        this->coeffmulti[i] = new Int[numCoef];
        ::memset(this->coeffmulti[i], 0, sizeof(Int)*numCoef);
      }
      this->filterPattern = new Int[NO_VAR_BINS];
      ::memset(this->filterPattern, 0, sizeof(Int)*NO_VAR_BINS);
    }
    break;
  case ALF_Cb:
  case ALF_Cr:
    {
      this->coeffmulti = new Int*[1];
      this->coeffmulti[0] = new Int[numCoef];
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

Void ALFParam::destroy()
{
  if(this->componentID >=0)
  {
    switch(this->componentID)
    {
    case ALF_Y:
      {
        for(Int i=0; i< NO_VAR_BINS; i++)
        {
          delete[] this->coeffmulti[i];
        }
        delete[] this->coeffmulti;
        delete[] this->filterPattern;
      }
      break;
    case ALF_Cb:
    case ALF_Cr:
      {
        delete[] this->coeffmulti[0];
        delete[] this->coeffmulti;
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
}

Void ALFParam::copy(const ALFParam& src)
{
  const Int numCoef = ALF_MAX_NUM_COEF;

  this->componentID       = src.componentID;
  this->alf_flag          = src.alf_flag;
  if(this->alf_flag == 1)
  {
    this->filters_per_group = src.filters_per_group;
    this->filter_shape      = src.filter_shape;
    this->num_coeff         = src.num_coeff;

    switch(this->componentID)
    {
    case ALF_Cb:
    case ALF_Cr:
      {
        ::memcpy(this->coeffmulti[0], src.coeffmulti[0], sizeof(Int)*numCoef);
      }
      break;
    case ALF_Y:
      {
        this->startSecondFilter = src.startSecondFilter;
        ::memcpy(this->filterPattern, src.filterPattern, sizeof(Int)*NO_VAR_BINS);
        for(Int i=0; i< (Int)NO_VAR_BINS; i++)
        {
          ::memcpy(this->coeffmulti[i], src.coeffmulti[i], sizeof(Int)*numCoef);
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
  else
  {
    //reset
    this->filters_per_group = 0;
  }
}

TComAdaptiveLoopFilter::TComAdaptiveLoopFilter()
{
  m_pcTempPicYuv = NULL;
  m_iSGDepth     = 0;
  m_pcPic        = NULL;
  m_ppSliceAlfLCUs = NULL;
  m_pvpAlfLCU    = NULL;
  m_pvpSliceTileAlfLCU = NULL;
  m_varImg = NULL;
  m_filterCoeffSym = NULL;
}

static Pel Clip_post(int high, int val)
{
  return (Pel)(((val > high)? high: val));
}

Void TComAdaptiveLoopFilter::create( Int iPicWidth, Int iPicHeight, UInt uiMaxCUWidth, UInt uiMaxCUHeight, UInt uiMaxCUDepth )
{
  destroy();
  if ( !m_pcTempPicYuv )
  {
    m_pcTempPicYuv = new TComPicYuv;
    m_pcTempPicYuv->create( iPicWidth, iPicHeight, uiMaxCUWidth, uiMaxCUHeight, uiMaxCUDepth );
  }
  m_img_height = iPicHeight;
  m_img_width = iPicWidth;
  m_varImg = new Pel*[m_img_height];
  m_varImg[0] = new Pel[m_img_height*m_img_width];
  for(Int j=1; j< m_img_height; j++)
  {
    m_varImg[j] = m_varImg[0] + (j*m_img_width);
  }

  m_filterCoeffSym = new Int*[NO_VAR_BINS];
  for(Int g=0 ; g< (Int)NO_VAR_BINS; g++)
  {
    m_filterCoeffSym[g] = new Int[ALF_MAX_NUM_COEF];
  }
  UInt uiNumLCUsInWidth   = m_img_width  / uiMaxCUWidth;
  UInt uiNumLCUsInHeight  = m_img_height / uiMaxCUHeight;

  uiNumLCUsInWidth  += ( m_img_width % uiMaxCUWidth ) ? 1 : 0;
  uiNumLCUsInHeight += ( m_img_height % uiMaxCUHeight ) ? 1 : 0;

  m_uiNumCUsInFrame = uiNumLCUsInWidth* uiNumLCUsInHeight; 

  m_numLCUInPicWidth = uiNumLCUsInWidth;
  m_numLCUInPicHeight= uiNumLCUsInHeight;
  m_lcuHeight = uiMaxCUHeight;
  m_lineIdxPadBot = m_lcuHeight - 4 - 3; // DFRegion, Vertical Taps
  m_lineIdxPadTop = m_lcuHeight - 4; // DFRegion

  m_lcuHeightChroma = m_lcuHeight>>1;
  m_lineIdxPadBotChroma = m_lcuHeightChroma - 2 - 3; // DFRegion, Vertical Taps
  m_lineIdxPadTopChroma = m_lcuHeightChroma - 2 ; // DFRegion

  m_lcuWidth = uiMaxCUWidth;
  m_lcuWidthChroma = (m_lcuWidth >>1);

  //calculate RA filter indexes
  Int regionTable[NO_VAR_BINS] = {0, 1, 4, 5, 15, 2, 3, 6, 14, 11, 10, 7, 13, 12,  9,  8}; 
  Int xInterval = ((( (m_img_width +m_lcuWidth -1) / m_lcuWidth) +1) /4 *m_lcuWidth) ;  
  Int yInterval = ((((m_img_height +m_lcuHeight -1) / m_lcuHeight) +1) /4 *m_lcuHeight) ;
  Int shiftH = (Int)(log((double)VAR_SIZE_H)/log(2.0));
  Int shiftW = (Int)(log((double)VAR_SIZE_W)/log(2.0));
  int yIndex, xIndex;
  int yIndexOffset;

  for(Int i = 0; i < m_img_height; i=i+4)
  {
    yIndex = (yInterval == 0)?(3):(Clip_post( 3, i / yInterval));
    yIndexOffset = yIndex * 4 ;
    for(Int j = 0; j < m_img_width; j=j+4)
    {
      xIndex = (xInterval==0)?(3):(Clip_post( 3, j / xInterval));
      m_varImg[i>>shiftH][j>>shiftW] = regionTable[yIndexOffset + xIndex];
    }
  }
}

Void TComAdaptiveLoopFilter::destroy()
{
  if ( m_pcTempPicYuv )
  {
    m_pcTempPicYuv->destroy();
    delete m_pcTempPicYuv;
    m_pcTempPicYuv = NULL;
  }
  if(m_varImg != NULL)
  {
    delete[] m_varImg[0];
    delete[] m_varImg;
    m_varImg = NULL;
  }
  if(m_filterCoeffSym != NULL)
  {
    for(Int g=0 ; g< (Int)NO_VAR_BINS; g++)
    {
      delete[] m_filterCoeffSym[g];
    }
    delete[] m_filterCoeffSym; 
    m_filterCoeffSym = NULL;
  }
}

// --------------------------------------------------------------------------------------------------------------------
// interface function for actual ALF process
// --------------------------------------------------------------------------------------------------------------------

/** ALF reconstruction process for one picture
 * \param [in, out] pcPic the decoded/filtered picture (input: decoded picture; output filtered picture)
 * \param [in] vAlfCUCtrlParam ALF CU-on/off control parameters
 * \param [in] isAlfCoefInSlice ALF coefficient in slice (true) or ALF coefficient in APS (false) 
 */
Void TComAdaptiveLoopFilter::ALFProcess(TComPic* pcPic, ALFParam** alfParam, std::vector<Bool>* sliceAlfEnabled)
{
  TComPicYuv* pcPicYuvRec    = pcPic->getPicYuvRec();
  TComPicYuv* pcPicYuvExtRec = m_pcTempPicYuv;
  pcPicYuvRec   ->copyToPic          ( pcPicYuvExtRec );
  pcPicYuvExtRec->setBorderExtension ( false );
  pcPicYuvExtRec->extendPicBorder    ();

  Int lumaStride   = pcPicYuvExtRec->getStride();
  Int chromaStride = pcPicYuvExtRec->getCStride();

  for(Int compIdx =0; compIdx < NUM_ALF_COMPONENT; compIdx++)
  {
    Pel* pDec         = getPicBuf(pcPicYuvExtRec, compIdx);
    Pel* pRest        = getPicBuf(pcPicYuvRec, compIdx);
    Int  stride       = (compIdx == ALF_Y)?(lumaStride):(chromaStride);
    Int  formatShift = (compIdx == ALF_Y)?(0):(1);

    recALF(compIdx, sliceAlfEnabled[compIdx], alfParam[compIdx], pDec, pRest, stride, formatShift);
  }

}

/** ALF Reconstruction for each component
 * \param [in] compIdx color component index
 * \param [in] alfLCUParams alf parameters 
 * \param [in] pDec decoded picture
 * \param [in, out] pRest filtered picture
 * \param [in] stride picture stride in memory
 * \param [in] formatShift luma component (false) or chroma component (1)
 * \param [in] alfCUCtrlParam ALF CU-on/off control parameters 
 * \param [in] caculateBAIdx calculate BA filter index (true) or BA filter index array is ready (false)
 */
Void TComAdaptiveLoopFilter::recALF(Int compIdx, std::vector<Bool>& sliceAlfEnabled, ALFParam* alfParam, Pel* pDec, Pel* pRest, Int stride, Int formatShift)
{
  for(Int s=0; s< m_uiNumSlicesInPic; s++)
  {
    if((!m_pcPic->getValidSlice(s))||(!sliceAlfEnabled[s]))
    {
      continue;
    }
    assert(alfParam->alf_flag == 1);    
    reconstructCoefInfo(compIdx, alfParam, m_filterCoeffSym, m_varIndTab); //reconstruct ALF coefficients & related parameters 

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
      filterRegion(compIdx, (alfParam->filters_per_group == 1), vpAlfLCU, pSrc, pRest, stride, formatShift);
    } //tile
  } //slice
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

/** 
 \param filter         filter coefficient
 \param filterLength   filter length
 \param isChroma       1: chroma, 0: luma
 */
Void TComAdaptiveLoopFilter::checkFilterCoeffValue( Int *filter, Int filterLength, Bool isChroma )
{
  Int maxValueNonCenter = 1 * (1 << ALF_NUM_BIT_SHIFT) - 1;
  Int minValueNonCenter = 0 - 1 * (1 << ALF_NUM_BIT_SHIFT);
  Int maxValueCenter    = 2 * (1 << ALF_NUM_BIT_SHIFT) - 1;
  Int minValueCenter    = 0 ; 

  for(Int i = 0; i < filterLength-1; i++)
  {
    filter[i] = Clip3(minValueNonCenter, maxValueNonCenter, filter[i]);
  }

  filter[filterLength-1] = Clip3(minValueCenter, maxValueCenter, filter[filterLength-1]);
}


/** Initialize the variables for one ALF LCU
 * \param rAlfLCU to-be-initialized ALF LCU
 * \param sliceID slice index
 * \param tileID tile index
 * \param pcCU CU data pointer
 * \param maxNumSUInLCU maximum number of SUs in one LCU
 */
Void TComAdaptiveLoopFilter::InitAlfLCUInfo(AlfLCUInfo& rAlfLCU, Int sliceID, Int tileID, TComDataCU* pcCU, UInt maxNumSUInLCU)
{
  //pcCU
  rAlfLCU.pcCU     = pcCU;
  //sliceID
  rAlfLCU.sliceID = sliceID;
  //tileID
  rAlfLCU.tileID  = tileID;

  //numSGU, vpAlfBLock;
  std::vector<NDBFBlockInfo>& vNDBFBlock = *(pcCU->getNDBFilterBlocks());
  rAlfLCU.vpAlfBlock.clear();
  rAlfLCU.numSGU = 0;
  for(Int i=0; i< vNDBFBlock.size(); i++)
  {
    if( vNDBFBlock[i].sliceID == sliceID)
    {
      rAlfLCU.vpAlfBlock.push_back( &(vNDBFBlock[i])  );
      rAlfLCU.numSGU ++;
    }
  }
  //startSU
  rAlfLCU.startSU = rAlfLCU.vpAlfBlock.front()->startSU;
  //endSU
  rAlfLCU.endSU   = rAlfLCU.vpAlfBlock.back()->endSU;
  //bAllSUsInLCUInSameSlice
  rAlfLCU.bAllSUsInLCUInSameSlice = (rAlfLCU.startSU == 0)&&( rAlfLCU.endSU == maxNumSUInLCU -1);
}

/** create and initialize variables for picture ALF processing
 * \param pcPic picture-level data pointer
 * \param numSlicesInPic number of slices in picture
 */
Void TComAdaptiveLoopFilter::createPicAlfInfo(TComPic* pcPic, Int numSlicesInPic)
{
  m_uiNumSlicesInPic = numSlicesInPic;
#if REMOVE_FGS
  m_iSGDepth         = 0;
#else
  m_iSGDepth         = pcPic->getSliceGranularityForNDBFilter();
#endif
  
  m_bUseNonCrossALF = ( pcPic->getIndependentSliceBoundaryForNDBFilter() || pcPic->getIndependentTileBoundaryForNDBFilter());
  m_pcPic = pcPic;

  m_ppSliceAlfLCUs = new AlfLCUInfo*[m_uiNumSlicesInPic];
  m_pvpAlfLCU = new std::vector< AlfLCUInfo* >[m_uiNumSlicesInPic];
  m_pvpSliceTileAlfLCU = new std::vector< std::vector< AlfLCUInfo* > >[m_uiNumSlicesInPic];

  for(Int s=0; s< m_uiNumSlicesInPic; s++)
  {
    m_ppSliceAlfLCUs[s] = NULL;
    if(!pcPic->getValidSlice(s))
    {
      continue;
    }

    std::vector< TComDataCU* >& vSliceLCUPointers = pcPic->getOneSliceCUDataForNDBFilter(s);
    Int                         numLCU           = (Int)vSliceLCUPointers.size();

    //create Alf LCU info
    m_ppSliceAlfLCUs[s] = new AlfLCUInfo[numLCU];
    for(Int i=0; i< numLCU; i++)
    {
      TComDataCU* pcCU       = vSliceLCUPointers[i];
      if(pcCU->getPic()==0)
      {
        continue;
      }
      Int         currTileID = pcPic->getPicSym()->getTileIdxMap(pcCU->getAddr());

      InitAlfLCUInfo(m_ppSliceAlfLCUs[s][i], s, currTileID, pcCU, pcPic->getNumPartInCU());
    }

    //distribute Alf LCU info pointers to slice container
    std::vector< AlfLCUInfo* >&    vpSliceAlfLCU     = m_pvpAlfLCU[s]; 
    vpSliceAlfLCU.reserve(numLCU);
    vpSliceAlfLCU.resize(0);
    std::vector< std::vector< AlfLCUInfo* > > &vpSliceTileAlfLCU = m_pvpSliceTileAlfLCU[s];
    Int prevTileID = -1;
    Int numValidTilesInSlice = 0;

    for(Int i=0; i< numLCU; i++)
    {
      AlfLCUInfo* pcAlfLCU = &(m_ppSliceAlfLCUs[s][i]);

      //container of Alf LCU pointers for slice processing
      vpSliceAlfLCU.push_back( pcAlfLCU);

      if(pcAlfLCU->tileID != prevTileID)
      {
        if(prevTileID == -1 || pcPic->getIndependentTileBoundaryForNDBFilter())
        {
          prevTileID = pcAlfLCU->tileID;
          numValidTilesInSlice ++;
          vpSliceTileAlfLCU.resize(numValidTilesInSlice);
        }
      }
      //container of Alf LCU pointers for tile processing 
      vpSliceTileAlfLCU[numValidTilesInSlice-1].push_back(pcAlfLCU);
    }

    assert( vpSliceAlfLCU.size() == numLCU);
  }

  if(m_bUseNonCrossALF)
  {
    m_pcSliceYuvTmp = pcPic->getYuvPicBufferForIndependentBoundaryProcessing();
  }
}

/** Destroy ALF slice units
 */
Void TComAdaptiveLoopFilter::destroyPicAlfInfo()
{
  for(Int s=0; s< m_uiNumSlicesInPic; s++)
  {
    if(m_ppSliceAlfLCUs[s] != NULL)
    {
      delete[] m_ppSliceAlfLCUs[s];
      m_ppSliceAlfLCUs[s] = NULL;
    }
  }
  delete[] m_ppSliceAlfLCUs;
  m_ppSliceAlfLCUs = NULL;

  delete[] m_pvpAlfLCU;
  m_pvpAlfLCU = NULL;

  delete[] m_pvpSliceTileAlfLCU;
  m_pvpSliceTileAlfLCU = NULL;
}

/** Copy region pixels
 * \param vpAlfLCU ALF LCU data container
 * \param pPicDst destination picture buffer
 * \param pPicSrc source picture buffer
 * \param stride stride size of picture buffer
 * \param formatShift region size adjustment according to component size
 */
Void TComAdaptiveLoopFilter::copyRegion(std::vector< AlfLCUInfo* > &vpAlfLCU, Pel* pPicDst, Pel* pPicSrc, Int stride, Int formatShift)
{
  Int extSize = 4;
  Int posX, posY, width, height, offset;
  Pel *pPelDst, *pPelSrc;
  
  for(Int idx =0; idx < vpAlfLCU.size(); idx++)
  {
    AlfLCUInfo& cAlfLCU = *(vpAlfLCU[idx]);
    for(Int n=0; n < cAlfLCU.numSGU; n++)
    {
      NDBFBlockInfo& rSGU = cAlfLCU[n];

      posX     = (Int)(rSGU.posX   >> formatShift);
      posY     = (Int)(rSGU.posY   >> formatShift);
      width    = (Int)(rSGU.width  >> formatShift);
      height   = (Int)(rSGU.height >> formatShift);
      offset   = ( (posY- extSize) * stride)+ (posX -extSize);
      pPelDst  = pPicDst + offset;    
      pPelSrc  = pPicSrc + offset;    

      for(Int j=0; j< (height + (extSize<<1)); j++)
      {
        ::memcpy(pPelDst, pPelSrc, sizeof(Pel)*(width + (extSize<<1)));
        pPelDst += stride;
        pPelSrc += stride;
      }
    }
  }
}

/** Extend region boundary 
 * \param [in] vpAlfLCU ALF LCU data container
 * \param [in,out] pPelSrc picture buffer
 * \param [in] stride stride size of picture buffer
 * \param [in] formatShift region size adjustment according to component size
 */
Void TComAdaptiveLoopFilter::extendRegionBorder(std::vector< AlfLCUInfo* > &vpAlfLCU, Pel* pPelSrc, Int stride, Int formatShift)
{
  UInt extSize = 4;
  UInt width, height;
  UInt posX, posY;
  Pel* pPel;
  Bool* pbAvail;
  for(Int idx = 0; idx < vpAlfLCU.size(); idx++)
  {
    AlfLCUInfo& rAlfLCU = *(vpAlfLCU[idx]);
    for(Int n =0; n < rAlfLCU.numSGU; n++)
    {
      NDBFBlockInfo& rSGU = rAlfLCU[n];
      if(rSGU.allBordersAvailable)
      {
        continue;
      }
      posX     = rSGU.posX >> formatShift;
      posY     = rSGU.posY >> formatShift;
      width    = rSGU.width >> formatShift;
      height   = rSGU.height >> formatShift;
      pbAvail  = rSGU.isBorderAvailable;    
      pPel     = pPelSrc + (posY * stride)+ posX;    
      extendBorderCoreFunction(pPel, stride, pbAvail, width, height, extSize);
    }
  }
}

/** Core function for extending slice/tile boundary 
 * \param [in, out] pPel processing block pointer
 * \param [in] stride picture buffer stride
 * \param [in] pbAvail neighboring blocks availabilities
 * \param [in] width block width
 * \param [in] height block height
 * \param [in] extSize boundary extension size
 */
Void TComAdaptiveLoopFilter::extendBorderCoreFunction(Pel* pPel, Int stride, Bool* pbAvail, UInt width, UInt height, UInt extSize)
{
  Pel* pPelDst;
  Pel* pPelSrc;
  Int i, j;

  for(Int pos =0; pos < NUM_SGU_BORDER; pos++)
  {
    if(pbAvail[pos])
    {
      continue;
    }

    switch(pos)
    {
    case SGU_L:
      {
        pPelDst = pPel - extSize;
        pPelSrc = pPel;
        for(j=0; j< height; j++)
        {
          for(i=0; i< extSize; i++)
          {
            pPelDst[i] = *pPelSrc;
          }
          pPelDst += stride;
          pPelSrc += stride;
        }
      }
      break;
    case SGU_R:
      {
        pPelDst = pPel + width;
        pPelSrc = pPelDst -1;
        for(j=0; j< height; j++)
        {
          for(i=0; i< extSize; i++)
          {
            pPelDst[i] = *pPelSrc;
          }
          pPelDst += stride;
          pPelSrc += stride;
        }

      }
      break;
    case SGU_T:
      {
        pPelSrc = pPel;
        pPelDst = pPel - stride;
        for(j=0; j< extSize; j++)
        {
          ::memcpy(pPelDst, pPelSrc, sizeof(Pel)*width);
          pPelDst -= stride;
        }
      }
      break;
    case SGU_B:
      {
        pPelDst = pPel + height*stride;
        pPelSrc = pPelDst - stride;
        for(j=0; j< extSize; j++)
        {
          ::memcpy(pPelDst, pPelSrc, sizeof(Pel)*width);
          pPelDst += stride;
        }

      }
      break;
    case SGU_TL:
      {
        if( (!pbAvail[SGU_T]) && (!pbAvail[SGU_L]))
        {
          pPelSrc = pPel  - extSize;
          pPelDst = pPelSrc - stride;
          for(j=0; j< extSize; j++)
          {
            ::memcpy(pPelDst, pPelSrc, sizeof(Pel)*extSize);
            pPelDst -= stride;
          }         
        }
      }
      break;
    case SGU_TR:
      {
        if( (!pbAvail[SGU_T]) && (!pbAvail[SGU_R]))
        {
          pPelSrc = pPel + width;
          pPelDst = pPelSrc - stride;
          for(j=0; j< extSize; j++)
          {
            ::memcpy(pPelDst, pPelSrc, sizeof(Pel)*extSize);
            pPelDst -= stride;
          }

        }

      }
      break;
    case SGU_BL:
      {
        if( (!pbAvail[SGU_B]) && (!pbAvail[SGU_L]))
        {
          pPelDst = pPel + height*stride; pPelDst-= extSize;
          pPelSrc = pPelDst - stride;
          for(j=0; j< extSize; j++)
          {
            ::memcpy(pPelDst, pPelSrc, sizeof(Pel)*extSize);
            pPelDst += stride;
          }

        }
      }
      break;
    case SGU_BR:
      {
        if( (!pbAvail[SGU_B]) && (!pbAvail[SGU_R]))
        {
          pPelDst = pPel + height*stride; pPelDst += width;
          pPelSrc = pPelDst - stride;
          for(j=0; j< extSize; j++)
          {
            ::memcpy(pPelDst, pPelSrc, sizeof(Pel)*extSize);
            pPelDst += stride;
          }
        }
      }
      break;
    default:
      {
        printf("Not a legal neighboring availability\n");
        assert(0);
        exit(-1);
      }
    }
  }
}

Void TComAdaptiveLoopFilter::reconstructCoefficients(ALFParam* alfParam, Int** filterCoeff)
{
  for(Int g=0; g< alfParam->filters_per_group; g++)
  {
    Int sum = 0;
    for(Int i=0; i< alfParam->num_coeff-1; i++)
    {
      sum += (2 * alfParam->coeffmulti[g][i]);
      filterCoeff[g][i] = alfParam->coeffmulti[g][i];
    }
    Int coeffPred = (1<<ALF_NUM_BIT_SHIFT) - sum;
    filterCoeff[g][alfParam->num_coeff-1] = coeffPred + alfParam->coeffmulti[g][alfParam->num_coeff-1];
  }
}

Void TComAdaptiveLoopFilter::reconstructCoefInfo(Int compIdx, ALFParam* alfParam, Int** filterCoeff, Int* varIndTab)
{
  if(compIdx == ALF_Y)
  {
    ::memset(varIndTab, 0, NO_VAR_BINS * sizeof(Int));
    if(alfParam->filters_per_group > 1)
    {
      for(Int i = 1; i < NO_VAR_BINS; ++i)
      {
        if(alfParam->filterPattern[i])
        {
          varIndTab[i] = varIndTab[i-1] + 1;
        }
        else
        {
          varIndTab[i] = varIndTab[i-1];
        }
      }
    }
  }
  reconstructCoefficients(alfParam, filterCoeff);
}

/** filter process without CU-On/Off control
 * \param [in] alfLCUParam ALF parameters 
 * \param [in] regionLCUInfo ALF CU-on/off control parameters 
 * \param [in] pDec decoded picture
 * \param [out] pRest filtered picture
 * \param [in] stride picture stride in memory
 * \param [in] formatShift luma component (0) or chroma component (1)
 * \param [in] caculateBAIdx calculate BA filter index (true) or BA filter index array is ready (false)
 */
Void TComAdaptiveLoopFilter::filterRegion(Int compIdx, Bool isSingleFilter, std::vector<AlfLCUInfo*>& regionLCUInfo, Pel* pDec, Pel* pRest, Int stride, Int formatShift)
{
  Int height, width;
  Int ypos =0, xpos=0;
  for(Int i=0; i< regionLCUInfo.size(); i++)
  {
    AlfLCUInfo& alfLCUinfo = *(regionLCUInfo[i]); 
    TComDataCU* pcCU = alfLCUinfo.pcCU;

    if(pcCU->getAlfLCUEnabled(compIdx))
    {
      //filtering process
      for(Int j=0; j< alfLCUinfo.numSGU; j++)
      {
        ypos    = (Int)(alfLCUinfo[j].posY   >> formatShift);
        xpos    = (Int)(alfLCUinfo[j].posX   >> formatShift);
        height = (Int)(alfLCUinfo[j].height >> formatShift);
        width  = (Int)(alfLCUinfo[j].width  >> formatShift);

        filterOneCompRegion(pRest, pDec, stride, (compIdx!=ALF_Y), ypos, ypos+height, xpos, xpos+width, m_filterCoeffSym, m_varIndTab, m_varImg);
      }
    } //alf_flag == 1
  }
}

/** filtering pixels
 * \param [out] imgRes filtered picture
 * \param [in] imgPad decoded picture 
 * \param [in] stride picture stride in memory
 * \param [in] isChroma chroma component (true) or luma component (false)
 * \param [in] yPos y position of the top-left pixel in one to-be-filtered region
 * \param [in] yPosEnd y position of the right-bottom pixel in one to-be-filtered region
 * \param [in] xPos x position of the top-left pixel in one to-be-filtered region
 * \param [in] xPosEnd x position of the right-bottom pixel in one to-be-filtered region
 * \param [in] filterSet filter coefficients
 * \param [in] mergeTable the merged groups in block-based adaptation mode
 * \param [in] varImg BA filter index array 
 */
Void TComAdaptiveLoopFilter::filterOneCompRegion(Pel *imgRes, Pel *imgPad, Int stride, Bool isChroma
                                                , Int yPos, Int yPosEnd, Int xPos, Int xPosEnd
                                                , Int** filterSet, Int* mergeTable, Pel** varImg
                                                )
{
  static Int numBitsMinus1= (Int)ALF_NUM_BIT_SHIFT;
  static Int offset       = (1<<( (Int)ALF_NUM_BIT_SHIFT-1));
  static Int shiftHeight  = (Int)(log((double)VAR_SIZE_H)/log(2.0));
  static Int shiftWidth   = (Int)(log((double)VAR_SIZE_W)/log(2.0));

  Pel *imgPad1,*imgPad2,*imgPad3, *imgPad4, *imgPad5, *imgPad6;
  Pel *var = varImg[yPos>>shiftHeight] + (xPos>>shiftWidth);;
  Int i, j, pixelInt;
  Int *coef = NULL;

  coef    = filterSet[0];
  imgPad += (yPos*stride);
  imgRes += (yPos*stride);

  Int yLineInLCU;
  Int paddingLine;
  //Int varInd = 0;
  Int lcuHeight     = isChroma ? m_lcuHeightChroma     : m_lcuHeight;
  Int lineIdxPadBot = isChroma ? m_lineIdxPadBotChroma : m_lineIdxPadBot;
  Int lineIdxPadTop = isChroma ? m_lineIdxPadTopChroma : m_lineIdxPadTop;
  Int img_height    = isChroma ? m_img_height>>1       : m_img_height;

  for(i= yPos; i< yPosEnd; i++)
  {
    yLineInLCU = i % lcuHeight;

    if(isChroma && yLineInLCU == 0 && i>0)
    {
      paddingLine = yLineInLCU + 2;
      imgPad1 = imgPad + stride;
      imgPad2 = imgPad - stride;
      imgPad3 = imgPad + 2*stride;
      imgPad4 = imgPad - 2*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + 3*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - min(paddingLine, 3)*stride;
    }
    else if(yLineInLCU<lineIdxPadBot || i-yLineInLCU+lcuHeight >= img_height)
    {
      imgPad1 = imgPad +   stride;
      imgPad2 = imgPad -   stride;
      imgPad3 = imgPad + 2*stride;
      imgPad4 = imgPad - 2*stride;
      imgPad5 = imgPad + 3*stride;
      imgPad6 = imgPad - 3*stride;
    }
    else if (yLineInLCU<lineIdxPadTop)
    {
      paddingLine = - yLineInLCU + lineIdxPadTop - 1;
      imgPad1 = (paddingLine < 1) ? imgPad : imgPad + min(paddingLine, 1)*stride;
      imgPad2 = (paddingLine < 1) ? imgPad : imgPad - stride;
      imgPad3 = (paddingLine < 2) ? imgPad : imgPad + min(paddingLine, 2)*stride;
      imgPad4 = (paddingLine < 2) ? imgPad : imgPad - 2*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + min(paddingLine, 3)*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - 3*stride;
    }
    else
    {
      paddingLine = yLineInLCU - lineIdxPadTop ;
      imgPad1 = (paddingLine < 1) ? imgPad : imgPad + stride;
      imgPad2 = (paddingLine < 1) ? imgPad : imgPad - min(paddingLine, 1)*stride;
      imgPad3 = (paddingLine < 2) ? imgPad : imgPad + 2*stride;
      imgPad4 = (paddingLine < 2) ? imgPad : imgPad - min(paddingLine, 2)*stride;
      imgPad5 = (paddingLine < 3) ? imgPad : imgPad + 3*stride;
      imgPad6 = (paddingLine < 3) ? imgPad : imgPad - min(paddingLine, 3)*stride;
    } 

    if(!isChroma)
    {
      var = varImg[i>>shiftHeight] + (xPos>>shiftWidth);
    }

    for(j= xPos; j< xPosEnd ; j++)
    {
      if (!isChroma && j % VAR_SIZE_W==0) 
      {
        coef = filterSet[mergeTable[*(var++)]];
      }

      pixelInt  = coef[0]* (imgPad5[j  ] + imgPad6[j  ]);

      pixelInt += coef[1]* (imgPad3[j  ] + imgPad4[j  ]);

      pixelInt += coef[2]* (imgPad1[j+1] + imgPad2[j-1]);
      pixelInt += coef[3]* (imgPad1[j  ] + imgPad2[j  ]);
      pixelInt += coef[4]* (imgPad1[j-1] + imgPad2[j+1]);

      pixelInt += coef[5]* (imgPad[j+4] + imgPad[j-4]);
      pixelInt += coef[6]* (imgPad[j+3] + imgPad[j-3]);
      pixelInt += coef[7]* (imgPad[j+2] + imgPad[j-2]);
      pixelInt += coef[8]* (imgPad[j+1] + imgPad[j-1]);
      pixelInt += coef[9]* (imgPad[j  ]);

      pixelInt=(Int)((pixelInt+offset) >> numBitsMinus1);
      imgRes[j] = Clip( pixelInt );
    }

    imgPad += stride;
    imgRes += stride;
  }  
}

Pel* TComAdaptiveLoopFilter::getPicBuf(TComPicYuv* pPicYuv, Int compIdx)
{
  Pel* pBuf = NULL;
  switch(compIdx)
  {
  case ALF_Y:
    {
      pBuf = pPicYuv->getLumaAddr();
    }
    break;
  case ALF_Cb:
    {
      pBuf = pPicYuv->getCbAddr();
    }
    break;
  case ALF_Cr:
    {
      pBuf = pPicYuv->getCrAddr();
    }
    break;
  default:
    {
      printf("Not a legal component ID for ALF\n");
      assert(0);
      exit(-1);
    }
  }

  return pBuf;
}



/** PCM LF disable process. 
 * \param pcPic picture (TComPic) pointer
 * \returns Void
 *
 * \note Replace filtered sample values of PCM mode blocks with the transmitted and reconstructed ones.
 */
Void TComAdaptiveLoopFilter::PCMLFDisableProcess (TComPic* pcPic)
{
  xPCMRestoration(pcPic);
}

/** Picture-level PCM restoration. 
 * \param pcPic picture (TComPic) pointer
 * \returns Void
 */
Void TComAdaptiveLoopFilter::xPCMRestoration(TComPic* pcPic)
{
  Bool  bPCMFilter = (pcPic->getSlice(0)->getSPS()->getUsePCM() && pcPic->getSlice(0)->getSPS()->getPCMFilterDisableFlag())? true : false;

  if(bPCMFilter || pcPic->getSlice(0)->getPPS()->getTransquantBypassEnableFlag())
  {
    for( UInt uiCUAddr = 0; uiCUAddr < pcPic->getNumCUsInFrame() ; uiCUAddr++ )
    {
      TComDataCU* pcCU = pcPic->getCU(uiCUAddr);

      xPCMCURestoration(pcCU, 0, 0); 
    } 
  }
}

/** PCM CU restoration. 
 * \param pcCU pointer to current CU
 * \param uiAbsPartIdx part index
 * \param uiDepth CU depth
 * \returns Void
 */
Void TComAdaptiveLoopFilter::xPCMCURestoration ( TComDataCU* pcCU, UInt uiAbsZorderIdx, UInt uiDepth )
{
  TComPic* pcPic     = pcCU->getPic();
  UInt uiCurNumParts = pcPic->getNumPartInCU() >> (uiDepth<<1);
  UInt uiQNumParts   = uiCurNumParts>>2;

  // go to sub-CU
  if( pcCU->getDepth(uiAbsZorderIdx) > uiDepth )
  {
    for ( UInt uiPartIdx = 0; uiPartIdx < 4; uiPartIdx++, uiAbsZorderIdx+=uiQNumParts )
    {
      UInt uiLPelX   = pcCU->getCUPelX() + g_auiRasterToPelX[ g_auiZscanToRaster[uiAbsZorderIdx] ];
      UInt uiTPelY   = pcCU->getCUPelY() + g_auiRasterToPelY[ g_auiZscanToRaster[uiAbsZorderIdx] ];
      if( ( uiLPelX < pcCU->getSlice()->getSPS()->getPicWidthInLumaSamples() ) && ( uiTPelY < pcCU->getSlice()->getSPS()->getPicHeightInLumaSamples() ) )
        xPCMCURestoration( pcCU, uiAbsZorderIdx, uiDepth+1 );
    }
    return;
  }

  // restore PCM samples
  if ((pcCU->getIPCMFlag(uiAbsZorderIdx)) || pcCU->isLosslessCoded( uiAbsZorderIdx))
  {
    xPCMSampleRestoration (pcCU, uiAbsZorderIdx, uiDepth, TEXT_LUMA    );
    xPCMSampleRestoration (pcCU, uiAbsZorderIdx, uiDepth, TEXT_CHROMA_U);
    xPCMSampleRestoration (pcCU, uiAbsZorderIdx, uiDepth, TEXT_CHROMA_V);
  }
}

/** PCM sample restoration. 
 * \param pcCU pointer to current CU
 * \param uiAbsPartIdx part index
 * \param uiDepth CU depth
 * \param ttText texture component type
 * \returns Void
 */
Void TComAdaptiveLoopFilter::xPCMSampleRestoration (TComDataCU* pcCU, UInt uiAbsZorderIdx, UInt uiDepth, TextType ttText)
{
  TComPicYuv* pcPicYuvRec = pcCU->getPic()->getPicYuvRec();
  Pel* piSrc;
  Pel* piPcm;
  UInt uiStride;
  UInt uiWidth;
  UInt uiHeight;
  UInt uiPcmLeftShiftBit; 
  UInt uiX, uiY;
  UInt uiMinCoeffSize = pcCU->getPic()->getMinCUWidth()*pcCU->getPic()->getMinCUHeight();
  UInt uiLumaOffset   = uiMinCoeffSize*uiAbsZorderIdx;
  UInt uiChromaOffset = uiLumaOffset>>2;

  if( ttText == TEXT_LUMA )
  {
    piSrc = pcPicYuvRec->getLumaAddr( pcCU->getAddr(), uiAbsZorderIdx);
    piPcm = pcCU->getPCMSampleY() + uiLumaOffset;
    uiStride  = pcPicYuvRec->getStride();
    uiWidth  = (g_uiMaxCUWidth >> uiDepth);
    uiHeight = (g_uiMaxCUHeight >> uiDepth);
    if ( pcCU->isLosslessCoded(uiAbsZorderIdx) )
    {
      uiPcmLeftShiftBit = 0;
    }
    else
    {
        uiPcmLeftShiftBit = g_uiBitDepth + g_uiBitIncrement - pcCU->getSlice()->getSPS()->getPCMBitDepthLuma();
    }
  }
  else
  {
    if( ttText == TEXT_CHROMA_U )
    {
      piSrc = pcPicYuvRec->getCbAddr( pcCU->getAddr(), uiAbsZorderIdx );
      piPcm = pcCU->getPCMSampleCb() + uiChromaOffset;
    }
    else
    {
      piSrc = pcPicYuvRec->getCrAddr( pcCU->getAddr(), uiAbsZorderIdx );
      piPcm = pcCU->getPCMSampleCr() + uiChromaOffset;
    }

    uiStride = pcPicYuvRec->getCStride();
    uiWidth  = ((g_uiMaxCUWidth >> uiDepth)/2);
    uiHeight = ((g_uiMaxCUWidth >> uiDepth)/2);
    if ( pcCU->isLosslessCoded(uiAbsZorderIdx) )
    {
      uiPcmLeftShiftBit = 0;
    }
    else
    {
      uiPcmLeftShiftBit = g_uiBitDepth + g_uiBitIncrement - pcCU->getSlice()->getSPS()->getPCMBitDepthChroma();
    }
  }

  for( uiY = 0; uiY < uiHeight; uiY++ )
  {
    for( uiX = 0; uiX < uiWidth; uiX++ )
    {
      piSrc[uiX] = (piPcm[uiX] << uiPcmLeftShiftBit);
    }
    piPcm += uiWidth;
    piSrc += uiStride;
  }
}
//! \}
#endif
