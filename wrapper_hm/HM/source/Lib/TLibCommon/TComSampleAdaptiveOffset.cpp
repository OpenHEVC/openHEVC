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

/** \file     TComSampleAdaptiveOffset.cpp
    \brief    sample adaptive offset class
*/

#include "TComSampleAdaptiveOffset.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//! \ingroup TLibCommon
//! \{

SAOParam::~SAOParam()
{
  for (Int i = 0 ; i<3; i++)
  {
    if (psSaoPart[i])
    {
      delete [] psSaoPart[i];
    }
  }
}

// ====================================================================================================================
// Tables
// ====================================================================================================================

TComSampleAdaptiveOffset::TComSampleAdaptiveOffset()
{
  m_pClipTable = NULL;
  m_pClipTableBase = NULL;
  m_iOffsetBo = NULL;
  m_lumaTableBo = NULL;
  m_iUpBuff1 = NULL;
  m_iUpBuff2 = NULL;
  m_iUpBufft = NULL;
  ipSwap = NULL;

  m_pTmpU1 = NULL;
  m_pTmpU2 = NULL;
  m_pTmpL1 = NULL;
  m_pTmpL2 = NULL;
  m_iLcuPartIdx = NULL;
}

TComSampleAdaptiveOffset::~TComSampleAdaptiveOffset()
{

}

const Int TComSampleAdaptiveOffset::m_aiNumCulPartsLevel[5] =
{
  1,   //level 0
  5,   //level 1
  21,  //level 2
  85,  //level 3
  341, //level 4
};

const UInt TComSampleAdaptiveOffset::m_auiEoTable[9] =
{
  1, //0    
  2, //1   
  0, //2
  3, //3
  4, //4
  0, //5  
  0, //6  
  0, //7 
  0
};

Int TComSampleAdaptiveOffset::m_iNumClass[MAX_NUM_SAO_TYPE] =
{
  SAO_EO_LEN,
  SAO_EO_LEN,
  SAO_EO_LEN,
  SAO_EO_LEN,
  SAO_BO_LEN
};

UInt TComSampleAdaptiveOffset::m_uiMaxDepth = SAO_MAX_DEPTH;


/** convert Level Row Col to Idx
 * \param   level,  row,  col
 */
Int  TComSampleAdaptiveOffset::convertLevelRowCol2Idx(int level, int row, int col)
{
  Int idx;
  if (level == 0)
  {
    idx = 0;
  }
  else if (level == 1)
  {
    idx = 1 + row*2 + col;
  }
  else if (level == 2)
  {
    idx = 5 + row*4 + col;
  }
  else if (level == 3)
  {
    idx = 21 + row*8 + col;
  }
  else // (level == 4)
  {
    idx = 85 + row*16 + col;
  }
  return idx;
}

/** create SampleAdaptiveOffset memory.
 * \param 
 */
Void TComSampleAdaptiveOffset::create( UInt uiSourceWidth, UInt uiSourceHeight, UInt uiMaxCUWidth, UInt uiMaxCUHeight, UInt uiMaxCUDepth)
{
  m_iPicWidth  = uiSourceWidth;
  m_iPicHeight = uiSourceHeight;

  m_uiMaxCUWidth  = uiMaxCUWidth;
  m_uiMaxCUHeight = uiMaxCUHeight;

  m_iNumCuInWidth  = m_iPicWidth / m_uiMaxCUWidth;
  m_iNumCuInWidth += ( m_iPicWidth % m_uiMaxCUWidth ) ? 1 : 0;

  m_iNumCuInHeight  = m_iPicHeight / m_uiMaxCUHeight;
  m_iNumCuInHeight += ( m_iPicHeight % m_uiMaxCUHeight ) ? 1 : 0;

  Int iMaxSplitLevelHeight = (Int)(logf((float)m_iNumCuInHeight)/logf(2.0));
  Int iMaxSplitLevelWidth  = (Int)(logf((float)m_iNumCuInWidth )/logf(2.0));

  m_uiMaxSplitLevel = (iMaxSplitLevelHeight < iMaxSplitLevelWidth)?(iMaxSplitLevelHeight):(iMaxSplitLevelWidth);
  m_uiMaxSplitLevel = (m_uiMaxSplitLevel< m_uiMaxDepth)?(m_uiMaxSplitLevel):(m_uiMaxDepth);
  /* various structures are overloaded to store per component data.
   * m_iNumTotalParts must allow for sufficient storage in any allocated arrays */
  m_iNumTotalParts  = max(3,m_aiNumCulPartsLevel[m_uiMaxSplitLevel]);

  UInt uiInternalBitDepth = g_uiBitDepth+g_uiBitIncrement;
  UInt uiPixelRange = 1<<uiInternalBitDepth;
  UInt uiBoRangeShift = uiInternalBitDepth - SAO_BO_BITS;

  m_lumaTableBo = new Pel [uiPixelRange];
  for (Int k2=0; k2<uiPixelRange; k2++)
  {
    m_lumaTableBo[k2] = 1 + (k2>>uiBoRangeShift);
  }
  m_iUpBuff1 = new Int[m_iPicWidth+2];
  m_iUpBuff2 = new Int[m_iPicWidth+2];
  m_iUpBufft = new Int[m_iPicWidth+2];

  m_iUpBuff1++;
  m_iUpBuff2++;
  m_iUpBufft++;
  Pel i;

  UInt uiMaxY  = g_uiIBDI_MAX;
  UInt uiMinY  = 0;

  Int iCRangeExt = uiMaxY>>1;

  m_pClipTableBase = new Pel[uiMaxY+2*iCRangeExt];
  m_iOffsetBo      = new Int[uiMaxY+2*iCRangeExt];

  for(i=0;i<(uiMinY+iCRangeExt);i++)
  {
    m_pClipTableBase[i] = uiMinY;
  }

  for(i=uiMinY+iCRangeExt;i<(uiMaxY+  iCRangeExt);i++)
  {
    m_pClipTableBase[i] = i-iCRangeExt;
  }

  for(i=uiMaxY+iCRangeExt;i<(uiMaxY+2*iCRangeExt);i++)
  {
    m_pClipTableBase[i] = uiMaxY;
  }

  m_pClipTable = &(m_pClipTableBase[iCRangeExt]);

  m_iLcuPartIdx = new Int [m_iNumCuInHeight*m_iNumCuInWidth];
  m_pTmpL1 = new Pel [m_uiMaxCUHeight+1];
  m_pTmpL2 = new Pel [m_uiMaxCUHeight+1];
  m_pTmpU1 = new Pel [m_iPicWidth];
  m_pTmpU2 = new Pel [m_iPicWidth];
}

/** destroy SampleAdaptiveOffset memory.
 * \param 
 */
Void TComSampleAdaptiveOffset::destroy()
{
  if (m_pClipTableBase)
  {
    delete [] m_pClipTableBase; m_pClipTableBase = NULL;
  }
  if (m_iOffsetBo)
  {
    delete [] m_iOffsetBo; m_iOffsetBo = NULL;
  }
  if (m_lumaTableBo)
  {
    delete[] m_lumaTableBo; m_lumaTableBo = NULL;
  }

  if (m_iUpBuff1)
  {
    m_iUpBuff1--;
    delete [] m_iUpBuff1; m_iUpBuff1 = NULL;
  }
  if (m_iUpBuff2)
  {
    m_iUpBuff2--;
    delete [] m_iUpBuff2; m_iUpBuff2 = NULL;
  }
  if (m_iUpBufft)
  {
    m_iUpBufft--;
    delete [] m_iUpBufft; m_iUpBufft = NULL;
  }
  if (m_pTmpL1)
  {
    delete [] m_pTmpL1; m_pTmpL1 = NULL;
  }
  if (m_pTmpL2)
  {
    delete [] m_pTmpL2; m_pTmpL2 = NULL;
  }
  if (m_pTmpU1)
  {
    delete [] m_pTmpU1; m_pTmpU1 = NULL;
  }
  if (m_pTmpU2)
  {
    delete [] m_pTmpU2; m_pTmpU2 = NULL;
  }
  if(m_iLcuPartIdx)
  {
    delete []m_iLcuPartIdx; m_iLcuPartIdx = NULL;
  }
}

/** allocate memory for SAO parameters
 * \param    *pcSaoParam
 */
Void TComSampleAdaptiveOffset::allocSaoParam(SAOParam *pcSaoParam)
{
  pcSaoParam->iMaxSplitLevel = m_uiMaxSplitLevel;
  pcSaoParam->psSaoPart[0] = new SAOQTPart[ m_aiNumCulPartsLevel[pcSaoParam->iMaxSplitLevel] ];
  initSAOParam(pcSaoParam, 0, 0, 0, -1, 0, m_iNumCuInWidth-1,  0, m_iNumCuInHeight-1,0);
  pcSaoParam->psSaoPart[1] = new SAOQTPart[ m_aiNumCulPartsLevel[pcSaoParam->iMaxSplitLevel] ];
  pcSaoParam->psSaoPart[2] = new SAOQTPart[ m_aiNumCulPartsLevel[pcSaoParam->iMaxSplitLevel] ];
  initSAOParam(pcSaoParam, 0, 0, 0, -1, 0, m_iNumCuInWidth-1,  0, m_iNumCuInHeight-1,1);
  initSAOParam(pcSaoParam, 0, 0, 0, -1, 0, m_iNumCuInWidth-1,  0, m_iNumCuInHeight-1,2);
  for(Int j=0;j<MAX_NUM_SAO_TYPE;j++)
  {
    pcSaoParam->iNumClass[j] = m_iNumClass[j];
  }
  pcSaoParam->numCuInWidth  = m_iNumCuInWidth;
  pcSaoParam->numCuInHeight = m_iNumCuInHeight;
  pcSaoParam->saoLcuParam[0] = new SaoLcuParam [m_iNumCuInHeight*m_iNumCuInWidth];
  pcSaoParam->saoLcuParam[1] = new SaoLcuParam [m_iNumCuInHeight*m_iNumCuInWidth];
  pcSaoParam->saoLcuParam[2] = new SaoLcuParam [m_iNumCuInHeight*m_iNumCuInWidth];
}

/** initialize SAO parameters
 * \param    *pcSaoParam,  iPartLevel,  iPartRow,  iPartCol,  iParentPartIdx,  StartCUX,  EndCUX,  StartCUY,  EndCUY,  iYCbCr
 */
Void TComSampleAdaptiveOffset::initSAOParam(SAOParam *pcSaoParam, Int iPartLevel, Int iPartRow, Int iPartCol, Int iParentPartIdx, Int StartCUX, Int EndCUX, Int StartCUY, Int EndCUY, Int iYCbCr)
{
  Int j;
  Int iPartIdx = convertLevelRowCol2Idx(iPartLevel, iPartRow, iPartCol);

  SAOQTPart* pSaoPart;

  pSaoPart = &(pcSaoParam->psSaoPart[iYCbCr][iPartIdx]);

  pSaoPart->PartIdx   = iPartIdx;
  pSaoPart->PartLevel = iPartLevel;
  pSaoPart->PartRow   = iPartRow;
  pSaoPart->PartCol   = iPartCol;

  pSaoPart->StartCUX  = StartCUX;
  pSaoPart->EndCUX    = EndCUX;
  pSaoPart->StartCUY  = StartCUY;
  pSaoPart->EndCUY    = EndCUY;

  pSaoPart->UpPartIdx = iParentPartIdx;
  pSaoPart->iBestType   = -1;
  pSaoPart->iLength     =  0;

#if SAO_TYPE_CODING
  pSaoPart->subTypeIdx = 0;
#else
  pSaoPart->bandPosition = 0;
#endif

  for (j=0;j<MAX_NUM_SAO_OFFSETS;j++)
  {
    pSaoPart->iOffset[j] = 0;
  }

  if(pSaoPart->PartLevel != m_uiMaxSplitLevel)
  {
    Int DownLevel    = (iPartLevel+1 );
    Int DownRowStart = (iPartRow << 1);
    Int DownColStart = (iPartCol << 1);

    Int iDownRowIdx, iDownColIdx;
    Int NumCUWidth,  NumCUHeight;
    Int NumCULeft;
    Int NumCUTop;

    Int DownStartCUX, DownStartCUY;
    Int DownEndCUX, DownEndCUY;

    NumCUWidth  = EndCUX - StartCUX +1;
    NumCUHeight = EndCUY - StartCUY +1;
    NumCULeft   = (NumCUWidth  >> 1);
    NumCUTop    = (NumCUHeight >> 1);

    DownStartCUX= StartCUX;
    DownEndCUX  = DownStartCUX + NumCULeft - 1;
    DownStartCUY= StartCUY;
    DownEndCUY  = DownStartCUY + NumCUTop  - 1;
    iDownRowIdx = DownRowStart + 0;
    iDownColIdx = DownColStart + 0;

    pSaoPart->DownPartsIdx[0]= convertLevelRowCol2Idx(DownLevel, iDownRowIdx, iDownColIdx);

    initSAOParam(pcSaoParam, DownLevel, iDownRowIdx, iDownColIdx, iPartIdx, DownStartCUX, DownEndCUX, DownStartCUY, DownEndCUY, iYCbCr);

    DownStartCUX = StartCUX + NumCULeft;
    DownEndCUX   = EndCUX;
    DownStartCUY = StartCUY;
    DownEndCUY   = DownStartCUY + NumCUTop -1;
    iDownRowIdx  = DownRowStart + 0;
    iDownColIdx  = DownColStart + 1;

    pSaoPart->DownPartsIdx[1] = convertLevelRowCol2Idx(DownLevel, iDownRowIdx, iDownColIdx);

    initSAOParam(pcSaoParam, DownLevel, iDownRowIdx, iDownColIdx, iPartIdx,  DownStartCUX, DownEndCUX, DownStartCUY, DownEndCUY, iYCbCr);

    DownStartCUX = StartCUX;
    DownEndCUX   = DownStartCUX + NumCULeft -1;
    DownStartCUY = StartCUY + NumCUTop;
    DownEndCUY   = EndCUY;
    iDownRowIdx  = DownRowStart + 1;
    iDownColIdx  = DownColStart + 0;

    pSaoPart->DownPartsIdx[2] = convertLevelRowCol2Idx(DownLevel, iDownRowIdx, iDownColIdx);

    initSAOParam(pcSaoParam, DownLevel, iDownRowIdx, iDownColIdx, iPartIdx, DownStartCUX, DownEndCUX, DownStartCUY, DownEndCUY, iYCbCr);

    DownStartCUX = StartCUX+ NumCULeft;
    DownEndCUX   = EndCUX;
    DownStartCUY = StartCUY + NumCUTop;
    DownEndCUY   = EndCUY;
    iDownRowIdx  = DownRowStart + 1;
    iDownColIdx  = DownColStart + 1;

    pSaoPart->DownPartsIdx[3] = convertLevelRowCol2Idx(DownLevel, iDownRowIdx, iDownColIdx);

    initSAOParam(pcSaoParam, DownLevel, iDownRowIdx, iDownColIdx, iPartIdx,DownStartCUX, DownEndCUX, DownStartCUY, DownEndCUY, iYCbCr);
  }
  else
  {
    pSaoPart->DownPartsIdx[0]=pSaoPart->DownPartsIdx[1]= pSaoPart->DownPartsIdx[2]= pSaoPart->DownPartsIdx[3]= -1; 
  }
}

/** free memory of SAO parameters
 * \param   pcSaoParam
 */
Void TComSampleAdaptiveOffset::freeSaoParam(SAOParam *pcSaoParam)
{
  delete [] pcSaoParam->psSaoPart[0];
  delete [] pcSaoParam->psSaoPart[1];
  delete [] pcSaoParam->psSaoPart[2];
  pcSaoParam->psSaoPart[0] = 0;
  pcSaoParam->psSaoPart[1] = 0;
  pcSaoParam->psSaoPart[2] = 0;
  if( pcSaoParam->saoLcuParam[0]) 
  {
    delete [] pcSaoParam->saoLcuParam[0]; pcSaoParam->saoLcuParam[0] = NULL;
  }
  if( pcSaoParam->saoLcuParam[1]) 
  {
    delete [] pcSaoParam->saoLcuParam[1]; pcSaoParam->saoLcuParam[1] = NULL;
  }
  if( pcSaoParam->saoLcuParam[2]) 
  {
    delete [] pcSaoParam->saoLcuParam[2]; pcSaoParam->saoLcuParam[2] = NULL;
  }
} 

/** reset SAO parameters
 * \param   pcSaoParam
 */
Void TComSampleAdaptiveOffset::resetSAOParam(SAOParam *pcSaoParam)
{
  Int iNumComponet = 3;
  for(Int c=0; c<iNumComponet; c++)
  {
#if SAO_TYPE_SHARING
if (c<2)
  {
#endif
    pcSaoParam->bSaoFlag[c] = 0;
#if SAO_TYPE_SHARING
  }
#endif
    for(Int i=0; i< m_aiNumCulPartsLevel[m_uiMaxSplitLevel]; i++)
    {
      pcSaoParam->psSaoPart[c][i].iBestType     = -1;
      pcSaoParam->psSaoPart[c][i].iLength       =  0;
      pcSaoParam->psSaoPart[c][i].bSplit        = false; 
      pcSaoParam->psSaoPart[c][i].bProcessed    = false;
      pcSaoParam->psSaoPart[c][i].dMinCost      = MAX_DOUBLE;
      pcSaoParam->psSaoPart[c][i].iMinDist      = MAX_INT;
      pcSaoParam->psSaoPart[c][i].iMinRate      = MAX_INT;
#if SAO_TYPE_CODING
      pcSaoParam->psSaoPart[c][i].subTypeIdx    = 0;
#else
      pcSaoParam->psSaoPart[c][i].bandPosition = 0;
#endif
      for (Int j=0;j<MAX_NUM_SAO_OFFSETS;j++)
      {
        pcSaoParam->psSaoPart[c][i].iOffset[j] = 0;
        pcSaoParam->psSaoPart[c][i].iOffset[j] = 0;
        pcSaoParam->psSaoPart[c][i].iOffset[j] = 0;
      }
    }
    pcSaoParam->oneUnitFlag[0]   = 0;
    pcSaoParam->oneUnitFlag[1]   = 0;
    pcSaoParam->oneUnitFlag[2]   = 0;
    resetLcuPart(pcSaoParam->saoLcuParam[0]);
    resetLcuPart(pcSaoParam->saoLcuParam[1]);
    resetLcuPart(pcSaoParam->saoLcuParam[2]);
  }
}

/** get the sign of input variable
 * \param   x
 */
inline int xSign(int x)
{
  return ((x >> 31) | ((int)( (((unsigned int) -x)) >> 31)));
}

/** initialize variables for SAO process
 * \param  pcPic picture data pointer
 * \param  numSlicesInPic number of slices in picture
 */
Void TComSampleAdaptiveOffset::createPicSaoInfo(TComPic* pcPic, Int numSlicesInPic)
{
  m_pcPic   = pcPic;
  m_uiNumSlicesInPic = numSlicesInPic;
#if REMOVE_FGS
  m_iSGDepth = 0;
#else
  m_iSGDepth         = pcPic->getSliceGranularityForNDBFilter();
#endif
  m_bUseNIF = ( pcPic->getIndependentSliceBoundaryForNDBFilter() || pcPic->getIndependentTileBoundaryForNDBFilter() );
  if(m_bUseNIF)
  {
    m_pcYuvTmp = pcPic->getYuvPicBufferForIndependentBoundaryProcessing();
  }
}

Void TComSampleAdaptiveOffset::destroyPicSaoInfo()
{

}

/** sample adaptive offset process for one LCU
 * \param   iAddr, iSaoType, iYCbCr
 */
Void TComSampleAdaptiveOffset::processSaoCu(Int iAddr, Int iSaoType, Int iYCbCr)
{
  if(!m_bUseNIF)
  {
    processSaoCuOrg( iAddr, iSaoType, iYCbCr);
  }
  else
  {  
    Int  isChroma = (iYCbCr != 0)? 1:0;
    Int  stride   = (iYCbCr != 0)?(m_pcPic->getCStride()):(m_pcPic->getStride());
    Pel* pPicRest = getPicYuvAddr(m_pcPic->getPicYuvRec(), iYCbCr);
    Pel* pPicDec  = getPicYuvAddr(m_pcYuvTmp, iYCbCr);

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

      processSaoBlock(pPicDec+ posOffset, pPicRest+ posOffset, stride, iSaoType, xPos, yPos, width, height, pbBorderAvail);
    }
  }
}

/** Perform SAO for non-cross-slice or non-cross-tile process
 * \param  pDec to-be-filtered block buffer pointer
 * \param  pRest filtered block buffer pointer
 * \param  stride picture buffer stride
 * \param  saoType SAO offset type
 * \param  xPos x coordinate
 * \param  yPos y coordinate
 * \param  width block width
 * \param  height block height
 * \param  pbBorderAvail availabilities of block border pixels
 */
Void TComSampleAdaptiveOffset::processSaoBlock(Pel* pDec, Pel* pRest, Int stride, Int saoType, UInt xPos, UInt yPos, UInt width, UInt height, Bool* pbBorderAvail)
{
  //variables
  Int startX, startY, endX, endY, x, y;
  Int signLeft,signRight,signDown,signDown1;
  UInt edgeType;

  switch (saoType)
  {
  case SAO_EO_0: // dir: -
    {

      startX = (pbBorderAvail[SGU_L]) ? 0 : 1;
      endX   = (pbBorderAvail[SGU_R]) ? width : (width -1);
      for (y=0; y< height; y++)
      {
        signLeft = xSign(pDec[startX] - pDec[startX-1]);
        for (x=startX; x< endX; x++)
        {
          signRight =  xSign(pDec[x] - pDec[x+1]); 
          edgeType =  signRight + signLeft + 2;
          signLeft  = -signRight;

          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
        }
        pDec  += stride;
        pRest += stride;
      }
      break;
    }
  case SAO_EO_1: // dir: |
    {
      startY = (pbBorderAvail[SGU_T]) ? 0 : 1;
      endY   = (pbBorderAvail[SGU_B]) ? height : height-1;
      if (!pbBorderAvail[SGU_T])
      {
        pDec  += stride;
        pRest += stride;
      }
      for (x=0; x< width; x++)
      {
        m_iUpBuff1[x] = xSign(pDec[x] - pDec[x-stride]);
      }
      for (y=startY; y<endY; y++)
      {
        for (x=0; x< width; x++)
        {
          signDown  = xSign(pDec[x] - pDec[x+stride]); 
          edgeType = signDown + m_iUpBuff1[x] + 2;
          m_iUpBuff1[x]= -signDown;

          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
        }
        pDec  += stride;
        pRest += stride;
      }
      break;
    }
  case SAO_EO_2: // dir: 135
    {
      Int posShift= stride + 1;

      startX = (pbBorderAvail[SGU_L]) ? 0 : 1 ;
      endX   = (pbBorderAvail[SGU_R]) ? width : (width-1);

      //prepare 2nd line upper sign
      pDec += stride;
      for (x=startX; x< endX+1; x++)
      {
        m_iUpBuff1[x] = xSign(pDec[x] - pDec[x- posShift]);
      }

      //1st line
      pDec -= stride;
      if(pbBorderAvail[SGU_TL])
      {
        x= 0;
        edgeType      =  xSign(pDec[x] - pDec[x- posShift]) - m_iUpBuff1[x+1] + 2;
        pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];

      }
      if(pbBorderAvail[SGU_T])
      {
        for(x= 1; x< endX; x++)
        {
          edgeType      =  xSign(pDec[x] - pDec[x- posShift]) - m_iUpBuff1[x+1] + 2;
          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
        }
      }
      pDec   += stride;
      pRest  += stride;


      //middle lines
      for (y= 1; y< height-1; y++)
      {
        for (x=startX; x<endX; x++)
        {
          signDown1      =  xSign(pDec[x] - pDec[x+ posShift]) ;
          edgeType      =  signDown1 + m_iUpBuff1[x] + 2;
          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];

          m_iUpBufft[x+1] = -signDown1; 
        }
        m_iUpBufft[startX] = xSign(pDec[stride+startX] - pDec[startX-1]);

        ipSwap     = m_iUpBuff1;
        m_iUpBuff1 = m_iUpBufft;
        m_iUpBufft = ipSwap;

        pDec  += stride;
        pRest += stride;
      }

      //last line
      if(pbBorderAvail[SGU_B])
      {
        for(x= startX; x< width-1; x++)
        {
          edgeType =  xSign(pDec[x] - pDec[x+ posShift]) + m_iUpBuff1[x] + 2;
          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
        }
      }
      if(pbBorderAvail[SGU_BR])
      {
        x= width -1;
        edgeType =  xSign(pDec[x] - pDec[x+ posShift]) + m_iUpBuff1[x] + 2;
        pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
      }
      break;
    } 
  case SAO_EO_3: // dir: 45
    {
      Int  posShift     = stride - 1;
      startX = (pbBorderAvail[SGU_L]) ? 0 : 1;
      endX   = (pbBorderAvail[SGU_R]) ? width : (width -1);

      //prepare 2nd line upper sign
      pDec += stride;
      for (x=startX-1; x< endX; x++)
      {
        m_iUpBuff1[x] = xSign(pDec[x] - pDec[x- posShift]);
      }


      //first line
      pDec -= stride;
      if(pbBorderAvail[SGU_T])
      {
        for(x= startX; x< width -1; x++)
        {
          edgeType = xSign(pDec[x] - pDec[x- posShift]) -m_iUpBuff1[x-1] + 2;
          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
        }
      }
      if(pbBorderAvail[SGU_TR])
      {
        x= width-1;
        edgeType = xSign(pDec[x] - pDec[x- posShift]) -m_iUpBuff1[x-1] + 2;
        pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
      }
      pDec  += stride;
      pRest += stride;

      //middle lines
      for (y= 1; y< height-1; y++)
      {
        for(x= startX; x< endX; x++)
        {
          signDown1      =  xSign(pDec[x] - pDec[x+ posShift]) ;
          edgeType      =  signDown1 + m_iUpBuff1[x] + 2;

          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
          m_iUpBuff1[x-1] = -signDown1; 
        }
        m_iUpBuff1[endX-1] = xSign(pDec[endX-1 + stride] - pDec[endX]);

        pDec  += stride;
        pRest += stride;
      }

      //last line
      if(pbBorderAvail[SGU_BL])
      {
        x= 0;
        edgeType = xSign(pDec[x] - pDec[x+ posShift]) + m_iUpBuff1[x] + 2;
        pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];

      }
      if(pbBorderAvail[SGU_B])
      {
        for(x= 1; x< endX; x++)
        {
          edgeType = xSign(pDec[x] - pDec[x+ posShift]) + m_iUpBuff1[x] + 2;
          pRest[x] = m_pClipTable[pDec[x] + m_iOffsetEo[edgeType]];
        }
      }
      break;
    }   
  case SAO_BO:
    {
      for (y=0; y< height; y++)
      {
        for (x=0; x< width; x++)
        {
          pRest[x] = m_iOffsetBo[pDec[x]];
        }
        pRest += stride;
        pDec  += stride;
      }
      break;
    }
  default: break;
  }

}

/** sample adaptive offset process for one LCU crossing LCU boundary
 * \param   iAddr, iSaoType, iYCbCr
 */
Void TComSampleAdaptiveOffset::processSaoCuOrg(Int iAddr, Int iSaoType, Int iYCbCr)
{
  Int x,y;
  TComDataCU *pTmpCu = m_pcPic->getCU(iAddr);
  Pel* pRec;
  Int  iStride;
  Int  iLcuWidth  = m_uiMaxCUWidth;
  Int  iLcuHeight = m_uiMaxCUHeight;
  UInt uiLPelX    = pTmpCu->getCUPelX();
  UInt uiTPelY    = pTmpCu->getCUPelY();
  UInt uiRPelX;
  UInt uiBPelY;
  Int  iSignLeft;
  Int  iSignRight;
  Int  iSignDown;
  Int  iSignDown1;
  Int  iSignDown2;
  UInt uiEdgeType;
  Int iPicWidthTmp;
  Int iPicHeightTmp;
  Int iStartX;
  Int iStartY;
  Int iEndX;
  Int iEndY;
  Int iIsChroma = (iYCbCr!=0)? 1:0;
  Int iShift;
  Int iCuHeightTmp;
  Pel *pTmpLSwap;
  Pel *pTmpL;
  Pel *pTmpU;

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

  if(pTmpCu->getPic()==0)
  {
    return;
  }
  if (iYCbCr == 0)
  {
    pRec       = m_pcPic->getPicYuvRec()->getLumaAddr(iAddr);
    iStride    = m_pcPic->getStride();
  } 
  else if (iYCbCr == 1)
  {
    pRec       = m_pcPic->getPicYuvRec()->getCbAddr(iAddr);
    iStride    = m_pcPic->getCStride();
  }
  else 
  {
    pRec       = m_pcPic->getPicYuvRec()->getCrAddr(iAddr);
    iStride    = m_pcPic->getCStride();
  }

//   if (iSaoType!=SAO_BO_0 || iSaoType!=SAO_BO_1)
  {
    iCuHeightTmp = (m_uiMaxCUHeight >> iIsChroma);
    iShift = (m_uiMaxCUWidth>> iIsChroma)-1;
    for (Int i=0;i<iCuHeightTmp+1;i++)
    {
      m_pTmpL2[i] = pRec[iShift];
      pRec += iStride;
    }
    pRec -= (iStride*(iCuHeightTmp+1));

    pTmpL = m_pTmpL1; 
    pTmpU = &(m_pTmpU1[uiLPelX]); 
  }

  switch (iSaoType)
  {
  case SAO_EO_0: // dir: -
    {
      iStartX = (uiLPelX == 0) ? 1 : 0;
      iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth-1 : iLcuWidth;
      for (y=0; y<iLcuHeight; y++)
      {
        iSignLeft = xSign(pRec[iStartX] - pTmpL[y]);
        for (x=iStartX; x< iEndX; x++)
        {
          iSignRight =  xSign(pRec[x] - pRec[x+1]); 
          uiEdgeType =  iSignRight + iSignLeft + 2;
          iSignLeft  = -iSignRight;

          pRec[x] = m_pClipTable[pRec[x] + m_iOffsetEo[uiEdgeType]];
        }
        pRec += iStride;
      }
      break;
    }
  case SAO_EO_1: // dir: |
    {
      iStartY = (uiTPelY == 0) ? 1 : 0;
      iEndY   = (uiBPelY == iPicHeightTmp) ? iLcuHeight-1 : iLcuHeight;
      if (uiTPelY == 0)
      {
        pRec += iStride;
      }
      for (x=0; x< iLcuWidth; x++)
      {
        m_iUpBuff1[x] = xSign(pRec[x] - pTmpU[x]);
      }
      for (y=iStartY; y<iEndY; y++)
      {
        for (x=0; x<iLcuWidth; x++)
        {
          iSignDown  = xSign(pRec[x] - pRec[x+iStride]); 
          uiEdgeType = iSignDown + m_iUpBuff1[x] + 2;
          m_iUpBuff1[x]= -iSignDown;

          pRec[x] = m_pClipTable[pRec[x] + m_iOffsetEo[uiEdgeType]];
        }
        pRec += iStride;
      }
      break;
    }
  case SAO_EO_2: // dir: 135
    {
      iStartX = (uiLPelX == 0)            ? 1 : 0;
      iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth-1 : iLcuWidth;

      iStartY = (uiTPelY == 0) ?             1 : 0;
      iEndY   = (uiBPelY == iPicHeightTmp) ? iLcuHeight-1 : iLcuHeight;

      if (uiTPelY == 0)
      {
        pRec += iStride;
      }

      for (x=iStartX; x<iEndX; x++)
      {
        m_iUpBuff1[x] = xSign(pRec[x] - pTmpU[x-1]);
      }
      for (y=iStartY; y<iEndY; y++)
      {
        iSignDown2 = xSign(pRec[iStride+iStartX] - pTmpL[y]);
        for (x=iStartX; x<iEndX; x++)
        {
          iSignDown1      =  xSign(pRec[x] - pRec[x+iStride+1]) ;
          uiEdgeType      =  iSignDown1 + m_iUpBuff1[x] + 2;
          m_iUpBufft[x+1] = -iSignDown1; 
          pRec[x] = m_pClipTable[pRec[x] + m_iOffsetEo[uiEdgeType]];
        }
        m_iUpBufft[iStartX] = iSignDown2;

        ipSwap     = m_iUpBuff1;
        m_iUpBuff1 = m_iUpBufft;
        m_iUpBufft = ipSwap;

        pRec += iStride;
      }
      break;
    } 
  case SAO_EO_3: // dir: 45
    {
      iStartX = (uiLPelX == 0) ? 1 : 0;
      iEndX   = (uiRPelX == iPicWidthTmp) ? iLcuWidth-1 : iLcuWidth;

      iStartY = (uiTPelY == 0) ? 1 : 0;
      iEndY   = (uiBPelY == iPicHeightTmp) ? iLcuHeight-1 : iLcuHeight;

      if (iStartY == 1)
      {
        pRec += iStride;
      }

      for (x=iStartX-1; x<iEndX; x++)
      {
        m_iUpBuff1[x] = xSign(pRec[x] - pTmpU[x+1]);
      }
      for (y=iStartY; y<iEndY; y++)
      {
        x=iStartX;
        iSignDown1      =  xSign(pRec[x] - pTmpL[y+1]) ;
        uiEdgeType      =  iSignDown1 + m_iUpBuff1[x] + 2;
        m_iUpBuff1[x-1] = -iSignDown1; 
        pRec[x] = m_pClipTable[pRec[x] + m_iOffsetEo[uiEdgeType]];
        for (x=iStartX+1; x<iEndX; x++)
        {
          iSignDown1      =  xSign(pRec[x] - pRec[x+iStride-1]) ;
          uiEdgeType      =  iSignDown1 + m_iUpBuff1[x] + 2;
          m_iUpBuff1[x-1] = -iSignDown1; 
          pRec[x] = m_pClipTable[pRec[x] + m_iOffsetEo[uiEdgeType]];
        }
        m_iUpBuff1[iEndX-1] = xSign(pRec[iEndX-1 + iStride] - pRec[iEndX]);

        pRec += iStride;
      } 
      break;
    }   
  case SAO_BO:
    {
      for (y=0; y<iLcuHeight; y++)
      {
        for (x=0; x<iLcuWidth; x++)
        {
          pRec[x] = m_iOffsetBo[pRec[x]];
        }
        pRec += iStride;
      }
      break;
    }
  default: break;
  }
//   if (iSaoType!=SAO_BO_0 || iSaoType!=SAO_BO_1)
  {
    pTmpLSwap = m_pTmpL1;
    m_pTmpL1  = m_pTmpL2;
    m_pTmpL2  = pTmpLSwap;
  }
}
/** Sample adaptive offset process
 * \param pcPic, pcSaoParam  
 */
Void TComSampleAdaptiveOffset::SAOProcess(TComPic* pcPic, SAOParam* pcSaoParam)
{
#if SAO_LUM_CHROMA_ONOFF_FLAGS && SAO_TYPE_SHARING
  if (pcSaoParam->bSaoFlag[0] || pcSaoParam->bSaoFlag[1])
#elif SAO_LUM_CHROMA_ONOFF_FLAGS
  if (pcSaoParam->bSaoFlag[0] || pcSaoParam->bSaoFlag[1] || pcSaoParam->bSaoFlag[2])
#else
  if (pcSaoParam->bSaoFlag[0])
#endif
  {
#if FULL_NBIT
    m_uiSaoBitIncrease = g_uiBitDepth + (g_uiBitDepth-8) - min((Int)(g_uiBitDepth + (g_uiBitDepth-8)), 10);
#else
    m_uiSaoBitIncrease = g_uiBitDepth + g_uiBitIncrement - min((Int)(g_uiBitDepth + g_uiBitIncrement), 10);
#endif

    if(m_bUseNIF)
    {
      m_pcPic->getPicYuvRec()->copyToPic(m_pcYuvTmp);
    }
    if (m_saoLcuBasedOptimization)
    {
      pcSaoParam->oneUnitFlag[0] = 0;  
      pcSaoParam->oneUnitFlag[1] = 0;  
      pcSaoParam->oneUnitFlag[2] = 0;  
    }
    Int iY  = 0;
#if SAO_LUM_CHROMA_ONOFF_FLAGS
    if (pcSaoParam->bSaoFlag[0])
#endif
    {
      processSaoUnitAll( pcSaoParam->saoLcuParam[iY], pcSaoParam->oneUnitFlag[iY], iY);
    }
#if SAO_TYPE_SHARING
    if(pcSaoParam->bSaoFlag[1])
    {
       processSaoUnitAll( pcSaoParam->saoLcuParam[1], pcSaoParam->oneUnitFlag[1], 1);//Cb
       processSaoUnitAll( pcSaoParam->saoLcuParam[2], pcSaoParam->oneUnitFlag[2], 2);//Cr
    }
#else
    Int iCb = 1;
    Int iCr = 2;
    if (pcSaoParam->bSaoFlag[iCb])
    {
      processSaoUnitAll( pcSaoParam->saoLcuParam[iCb], pcSaoParam->oneUnitFlag[iCb], iCb);
    }
    if (pcSaoParam->bSaoFlag[iCr])
    {
      processSaoUnitAll( pcSaoParam->saoLcuParam[iCr], pcSaoParam->oneUnitFlag[iCr], iCr);
    }
#endif
    m_pcPic = NULL;
  }
}

Pel* TComSampleAdaptiveOffset::getPicYuvAddr(TComPicYuv* pcPicYuv, Int iYCbCr, Int iAddr)
{
  switch (iYCbCr)
  {
  case 0:
    return pcPicYuv->getLumaAddr(iAddr);
    break;
  case 1:
    return pcPicYuv->getCbAddr(iAddr);
    break;
  case 2:
    return pcPicYuv->getCrAddr(iAddr);
    break;
  default:
    return NULL;
    break;
  }
}
/** Process SAO all units 
 * \param saoLcuParam SAO LCU parameters
 * \param oneUnitFlag one unit flag
 * \param yCbCr color componet index
 */
Void TComSampleAdaptiveOffset::processSaoUnitAll(SaoLcuParam* saoLcuParam, Bool oneUnitFlag, Int yCbCr)
{
  Pel *pRec;
  Int picWidthTmp;

  if (yCbCr == 0)
  {
    pRec        = m_pcPic->getPicYuvRec()->getLumaAddr();
    picWidthTmp = m_iPicWidth;
  } 
  else if (yCbCr == 1)
  {
    pRec        = m_pcPic->getPicYuvRec()->getCbAddr();
    picWidthTmp = m_iPicWidth>>1;
  }
  else 
  {
    pRec        = m_pcPic->getPicYuvRec()->getCrAddr();
    picWidthTmp = m_iPicWidth>>1;
  }

  memcpy(m_pTmpU1, pRec, sizeof(Pel)*picWidthTmp);

  int  i;
  UInt edgeType;
  Pel* ppLumaTable = NULL;
  Int  typeIdx;

  Int offset[LUMA_GROUP_NUM+1];
  Int idxX;
  Int idxY;
  Int addr;
  Int frameWidthInCU = m_pcPic->getFrameWidthInCU();
  Int frameHeightInCU = m_pcPic->getFrameHeightInCU();
  Int stride;
  Pel *tmpUSwap;
  Int isChroma = (yCbCr == 0) ? 0:1;
  Bool mergeLeftFlag;

  offset[0] = 0;
  for (idxY = 0; idxY< frameHeightInCU; idxY++)
  { 
    addr = idxY * frameWidthInCU;
    if (yCbCr == 0)
    {
      pRec  = m_pcPic->getPicYuvRec()->getLumaAddr(addr);
      stride = m_pcPic->getStride();
      picWidthTmp = m_iPicWidth;
    }
    else if (yCbCr == 1)
    {
      pRec  = m_pcPic->getPicYuvRec()->getCbAddr(addr);
      stride = m_pcPic->getCStride();
      picWidthTmp = m_iPicWidth>>1;
    }
    else
    {
      pRec  = m_pcPic->getPicYuvRec()->getCrAddr(addr);
      stride = m_pcPic->getCStride();
      picWidthTmp = m_iPicWidth>>1;
    }

    //     pRec += iStride*(m_uiMaxCUHeight-1);
    for (i=0;i<(m_uiMaxCUHeight>>isChroma)+1;i++)
    {
      m_pTmpL1[i] = pRec[0];
      pRec+=stride;
    }
    pRec-=(stride<<1);

    memcpy(m_pTmpU2, pRec, sizeof(Pel)*picWidthTmp);

    for (idxX = 0; idxX < frameWidthInCU; idxX++)
    {
      addr = idxY * frameWidthInCU + idxX;

      if (oneUnitFlag)
      {
        typeIdx = saoLcuParam[0].typeIdx;
        mergeLeftFlag = (addr == 0)? 0:1;
      }
      else
      {
        typeIdx = saoLcuParam[addr].typeIdx;
        mergeLeftFlag = saoLcuParam[addr].mergeLeftFlag;
      }
      if (typeIdx>=0)
      {
        if (!mergeLeftFlag)
        {

          if (typeIdx == SAO_BO)
          {
            for (i=0; i<SAO_MAX_BO_CLASSES+1;i++)
            {
              offset[i] = 0;
            }
            for (i=0; i<saoLcuParam[addr].length; i++)
            {
#if SAO_TYPE_CODING
              offset[ (saoLcuParam[addr].subTypeIdx +i)%SAO_MAX_BO_CLASSES  +1] = saoLcuParam[addr].offset[i] << m_uiSaoBitIncrease;
#else
              offset[ (saoLcuParam[addr].bandPosition +i)%SAO_MAX_BO_CLASSES  +1] = saoLcuParam[addr].offset[i] << m_uiSaoBitIncrease;
#endif
            }

            ppLumaTable = m_lumaTableBo;

#if FULL_NBIT
            for (i=0;i<(1<<(g_uiBitDepth));i++)
#else
            for (i=0;i<(1<<(g_uiBitIncrement+8));i++)
#endif
            {
              m_iOffsetBo[i] = m_pClipTable[i + offset[ppLumaTable[i]]];
            }

          }
          if (typeIdx == SAO_EO_0 || typeIdx == SAO_EO_1 || typeIdx == SAO_EO_2 || typeIdx == SAO_EO_3)
          {
            for (i=0;i<saoLcuParam[addr].length;i++)
            {
              offset[i+1] = saoLcuParam[addr].offset[i] << m_uiSaoBitIncrease;
            }
            for (edgeType=0;edgeType<6;edgeType++)
            {
              m_iOffsetEo[edgeType]= offset[m_auiEoTable[edgeType]];
            }
          }
        }
        processSaoCu(addr, typeIdx, yCbCr);
      }
      else
      {
        if (idxX != (frameWidthInCU-1))
        {
          if (yCbCr == 0)
          {
            pRec  = m_pcPic->getPicYuvRec()->getLumaAddr(addr);
            stride = m_pcPic->getStride();
          }
          else if (yCbCr == 1)
          {
            pRec  = m_pcPic->getPicYuvRec()->getCbAddr(addr);
            stride = m_pcPic->getCStride();
          }
          else
          {
            pRec  = m_pcPic->getPicYuvRec()->getCrAddr(addr);
            stride = m_pcPic->getCStride();
          }
          Int widthShift = m_uiMaxCUWidth>>isChroma;
          for (i=0;i<(m_uiMaxCUHeight>>isChroma)+1;i++)
          {
            m_pTmpL1[i] = pRec[widthShift-1];
            pRec+=stride;
          }
        }
      }
    }
    tmpUSwap = m_pTmpU1;
    m_pTmpU1 = m_pTmpU2;
    m_pTmpU2 = tmpUSwap;
  }

}
/** Reset SAO LCU part 
 * \param saoLcuParam
 */
Void TComSampleAdaptiveOffset::resetLcuPart(SaoLcuParam* saoLcuParam)
{
  Int i,j;
  for (i=0;i<m_iNumCuInWidth*m_iNumCuInHeight;i++)
  {
    saoLcuParam[i].mergeUpFlag     =  1;
    saoLcuParam[i].mergeLeftFlag =  0;
    saoLcuParam[i].partIdx   =  0;
    saoLcuParam[i].typeIdx      = -1;
    for (j=0;j<MAX_NUM_SAO_OFFSETS;j++)
    {
      saoLcuParam[i].offset[j] = 0;
    }
#if SAO_TYPE_CODING
    saoLcuParam[i].subTypeIdx = 0;
#else
    saoLcuParam[i].bandPosition = 0;
#endif
  }
}

/** convert QP part to SAO unit 
* \param saoParam SAO parameter 
* \param partIdx SAO part index
* \param yCbCr color component index
 */
Void TComSampleAdaptiveOffset::convertQT2SaoUnit(SAOParam *saoParam, UInt partIdx, Int yCbCr)
{

  SAOQTPart*  saoPart= &(saoParam->psSaoPart[yCbCr][partIdx]);
  if (!saoPart->bSplit)
  {
    convertOnePart2SaoUnit(saoParam, partIdx, yCbCr);
    return;
  }

  if (saoPart->PartLevel < m_uiMaxSplitLevel)
  {
    convertQT2SaoUnit(saoParam, saoPart->DownPartsIdx[0], yCbCr);
    convertQT2SaoUnit(saoParam, saoPart->DownPartsIdx[1], yCbCr);
    convertQT2SaoUnit(saoParam, saoPart->DownPartsIdx[2], yCbCr);
    convertQT2SaoUnit(saoParam, saoPart->DownPartsIdx[3], yCbCr);
  }
}
/** convert one SAO part to SAO unit 
* \param saoParam SAO parameter 
* \param partIdx SAO part index
* \param yCbCr color component index
 */
Void TComSampleAdaptiveOffset::convertOnePart2SaoUnit(SAOParam *saoParam, UInt partIdx, Int yCbCr)
{
  Int j;
  Int idxX;
  Int idxY;
  Int addr;
  Int frameWidthInCU = m_pcPic->getFrameWidthInCU();
  SAOQTPart* saoQTPart = saoParam->psSaoPart[yCbCr];
  SaoLcuParam* saoLcuParam = saoParam->saoLcuParam[yCbCr];

  for (idxY = saoQTPart[partIdx].StartCUY; idxY<= saoQTPart[partIdx].EndCUY; idxY++)
  {
    for (idxX = saoQTPart[partIdx].StartCUX; idxX<= saoQTPart[partIdx].EndCUX; idxX++)
    {
      addr = idxY * frameWidthInCU + idxX;
      saoLcuParam[addr].partIdxTmp = (Int)partIdx; 
      saoLcuParam[addr].typeIdx    = saoQTPart[partIdx].iBestType;
#if SAO_TYPE_CODING
      saoLcuParam[addr].subTypeIdx = saoQTPart[partIdx].subTypeIdx;
#else
      saoLcuParam[addr].bandPosition = saoQTPart[partIdx].bandPosition;
#endif
      if (saoLcuParam[addr].typeIdx!=-1)
      {
        saoLcuParam[addr].length    = saoQTPart[partIdx].iLength;
        for (j=0;j<MAX_NUM_SAO_OFFSETS;j++)
        {
          saoLcuParam[addr].offset[j] = saoQTPart[partIdx].iOffset[j];
        }
      }
      else
      {
        saoLcuParam[addr].length    = 0;
#if SAO_TYPE_CODING
        saoLcuParam[addr].subTypeIdx = saoQTPart[partIdx].subTypeIdx;
#else
        saoLcuParam[addr].bandPosition = saoQTPart[partIdx].bandPosition;
#endif
        for (j=0;j<MAX_NUM_SAO_OFFSETS;j++)
        {
          saoLcuParam[addr].offset[j] = 0;
        }
      }
    }
  }
}

Void TComSampleAdaptiveOffset::resetSaoUnit(SaoLcuParam* saoUnit)
{
  saoUnit->partIdx       = 0;
  saoUnit->partIdxTmp    = 0;
  saoUnit->mergeLeftFlag = 0;
  saoUnit->mergeUpFlag   = 0;
  saoUnit->typeIdx       = -1;
  saoUnit->length        = 0;
#if SAO_TYPE_CODING
  saoUnit->subTypeIdx    = 0;
#else
  saoUnit->bandPosition  = 0;
#endif

  for (Int i=0;i<4;i++)
  {
    saoUnit->offset[i] = 0;
  }
}

#if SAO_SINGLE_MERGE
Void TComSampleAdaptiveOffset::copySaoUnit(SaoLcuParam* saoUnitDst, SaoLcuParam* saoUnitSrc )
{
  saoUnitDst->mergeLeftFlag = saoUnitSrc->mergeLeftFlag;
  saoUnitDst->mergeUpFlag   = saoUnitSrc->mergeUpFlag;
  saoUnitDst->typeIdx       = saoUnitSrc->typeIdx;
  saoUnitDst->length        = saoUnitSrc->length;

#if SAO_TYPE_CODING
  saoUnitDst->subTypeIdx  = saoUnitSrc->subTypeIdx;
#else
  saoUnitDst->bandPosition  = saoUnitSrc->bandPosition;
#endif
  for (Int i=0;i<4;i++)
  {
    saoUnitDst->offset[i] = saoUnitSrc->offset[i];
  }
}
#endif

//! \}
