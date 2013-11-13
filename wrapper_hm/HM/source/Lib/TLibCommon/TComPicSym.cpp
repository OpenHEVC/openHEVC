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

/** \file     TComPicSym.cpp
    \brief    picture symbol class
*/

#include "TComPicSym.h"
#include "TComSampleAdaptiveOffset.h"

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TComPicSym::TComPicSym()
:m_uiWidthInCU(0)
,m_uiHeightInCU(0)
,m_uiMaxCUWidth(0)
,m_uiMaxCUHeight(0)
,m_uiMinCUWidth(0)
,m_uiMinCUHeight(0)
,m_uhTotalDepth(0)
,m_uiNumPartitions(0)
,m_uiNumPartInWidth(0)
,m_uiNumPartInHeight(0)
,m_uiNumCUsInFrame(0)
,m_apcTComSlice(NULL)
,m_uiNumAllocatedSlice (0)
,m_apcTComDataCU (NULL)
,m_iNumColumnsMinus1 (0)
,m_iNumRowsMinus1(0)
,m_apcTComTile(NULL)
,m_puiCUOrderMap(0)
,m_puiTileIdxMap(NULL)
,m_puiInverseCUOrderMap(NULL)
#if HM_CLEANUP_SAO
,m_saoBlkParams(NULL)
#endif
{};


Void TComPicSym::create  ( Int iPicWidth, Int iPicHeight, UInt uiMaxWidth, UInt uiMaxHeight, UInt uiMaxDepth )
{
  UInt i;

  m_uhTotalDepth      = uiMaxDepth;
  m_uiNumPartitions   = 1<<(m_uhTotalDepth<<1);
  
  m_uiMaxCUWidth      = uiMaxWidth;
  m_uiMaxCUHeight     = uiMaxHeight;
  
  m_uiMinCUWidth      = uiMaxWidth  >> m_uhTotalDepth;
  m_uiMinCUHeight     = uiMaxHeight >> m_uhTotalDepth;
  
  m_uiNumPartInWidth  = m_uiMaxCUWidth  / m_uiMinCUWidth;
  m_uiNumPartInHeight = m_uiMaxCUHeight / m_uiMinCUHeight;
  
  m_uiWidthInCU       = ( iPicWidth %m_uiMaxCUWidth  ) ? iPicWidth /m_uiMaxCUWidth  + 1 : iPicWidth /m_uiMaxCUWidth;
  m_uiHeightInCU      = ( iPicHeight%m_uiMaxCUHeight ) ? iPicHeight/m_uiMaxCUHeight + 1 : iPicHeight/m_uiMaxCUHeight;
  
  m_uiNumCUsInFrame   = m_uiWidthInCU * m_uiHeightInCU;
  m_apcTComDataCU     = new TComDataCU*[m_uiNumCUsInFrame];
  
  if (m_uiNumAllocatedSlice>0)
  {
    for ( i=0; i<m_uiNumAllocatedSlice ; i++ )
    {
      delete m_apcTComSlice[i];
    }
    delete [] m_apcTComSlice;
  }
  m_apcTComSlice      = new TComSlice*[m_uiNumCUsInFrame*m_uiNumPartitions];  
  m_apcTComSlice[0]   = new TComSlice;
  m_uiNumAllocatedSlice = 1;
  for ( i=0; i<m_uiNumCUsInFrame ; i++ )
  {
    m_apcTComDataCU[i] = new TComDataCU;
    m_apcTComDataCU[i]->create( m_uiNumPartitions, m_uiMaxCUWidth, m_uiMaxCUHeight, false, m_uiMaxCUWidth >> m_uhTotalDepth
#if ADAPTIVE_QP_SELECTION
      , true
#endif     
      );
  }

  m_puiCUOrderMap = new UInt[m_uiNumCUsInFrame+1];
  m_puiTileIdxMap = new UInt[m_uiNumCUsInFrame];
  m_puiInverseCUOrderMap = new UInt[m_uiNumCUsInFrame+1];

  for( i=0; i<m_uiNumCUsInFrame; i++ )
  {
    m_puiCUOrderMap[i] = i;
    m_puiInverseCUOrderMap[i] = i;
  }

#if HM_CLEANUP_SAO
  m_saoBlkParams = new SAOBlkParam[m_uiNumCUsInFrame];
#else
  m_saoParam = NULL;
#endif
}

Void TComPicSym::destroy()
{
  if (m_uiNumAllocatedSlice>0)
  {
    for (Int i = 0; i<m_uiNumAllocatedSlice ; i++ )
    {
      delete m_apcTComSlice[i];
    }
    delete [] m_apcTComSlice;
  }
  m_apcTComSlice = NULL;
  
  for (Int i = 0; i < m_uiNumCUsInFrame; i++)
  {
    m_apcTComDataCU[i]->destroy();
    delete m_apcTComDataCU[i];
    m_apcTComDataCU[i] = NULL;
  }
  delete [] m_apcTComDataCU;
  m_apcTComDataCU = NULL;

  for(Int i = 0; i < (m_iNumColumnsMinus1+1)*(m_iNumRowsMinus1+1); i++ )
  {
    delete m_apcTComTile[i];
  }
  delete [] m_apcTComTile;

  m_apcTComTile = NULL;

  delete [] m_puiCUOrderMap;
  m_puiCUOrderMap = NULL;

  delete [] m_puiTileIdxMap;
  m_puiTileIdxMap = NULL;

  delete [] m_puiInverseCUOrderMap;
  m_puiInverseCUOrderMap = NULL;

#if HM_CLEANUP_SAO
  if(m_saoBlkParams)
  {
    delete[] m_saoBlkParams; m_saoBlkParams = NULL;
  }
#else
  if (m_saoParam)
  {
    TComSampleAdaptiveOffset::freeSaoParam(m_saoParam);
    delete m_saoParam;
    m_saoParam = NULL;
  }
#endif
}

Void TComPicSym::allocateNewSlice()
{
  m_apcTComSlice[m_uiNumAllocatedSlice ++] = new TComSlice;
  if (m_uiNumAllocatedSlice>=2)
  {
    m_apcTComSlice[m_uiNumAllocatedSlice-1]->copySliceInfo( m_apcTComSlice[m_uiNumAllocatedSlice-2] );
    m_apcTComSlice[m_uiNumAllocatedSlice-1]->initSlice();
  }
}

Void TComPicSym::clearSliceBuffer()
{
  UInt i;
  for (i = 1; i < m_uiNumAllocatedSlice; i++)
  {
    delete m_apcTComSlice[i];
  }
  m_uiNumAllocatedSlice = 1;
}

UInt TComPicSym::getPicSCUEncOrder( UInt SCUAddr )
{ 
  return getInverseCUOrderMap(SCUAddr/m_uiNumPartitions)*m_uiNumPartitions + SCUAddr%m_uiNumPartitions; 
}

UInt TComPicSym::getPicSCUAddr( UInt SCUEncOrder )
{
  return getCUOrderMap(SCUEncOrder/m_uiNumPartitions)*m_uiNumPartitions + SCUEncOrder%m_uiNumPartitions;
}

Void TComPicSym::xCreateTComTileArray()
{
  m_apcTComTile = new TComTile*[(m_iNumColumnsMinus1+1)*(m_iNumRowsMinus1+1)];
  for( UInt i=0; i<(m_iNumColumnsMinus1+1)*(m_iNumRowsMinus1+1); i++ )
  {
    m_apcTComTile[i] = new TComTile;
  }
}

Void TComPicSym::xInitTiles()
{
  UInt  uiTileIdx;
  UInt  uiColumnIdx = 0;
  UInt  uiRowIdx = 0;
  UInt  uiRightEdgePosInCU;
  UInt  uiBottomEdgePosInCU;
  Int   i, j;

  //initialize each tile of the current picture
  for( uiRowIdx=0; uiRowIdx < m_iNumRowsMinus1+1; uiRowIdx++ )
  {
    for( uiColumnIdx=0; uiColumnIdx < m_iNumColumnsMinus1+1; uiColumnIdx++ )
    {
      uiTileIdx = uiRowIdx * (m_iNumColumnsMinus1+1) + uiColumnIdx;

      //initialize the RightEdgePosInCU for each tile
      uiRightEdgePosInCU = 0;
      for( i=0; i <= uiColumnIdx; i++ )
      {
        uiRightEdgePosInCU += this->getTComTile(uiRowIdx * (m_iNumColumnsMinus1+1) + i)->getTileWidth();
      }
      this->getTComTile(uiTileIdx)->setRightEdgePosInCU(uiRightEdgePosInCU-1);

      //initialize the BottomEdgePosInCU for each tile
      uiBottomEdgePosInCU = 0;
      for( i=0; i <= uiRowIdx; i++ )
      {
        uiBottomEdgePosInCU += this->getTComTile(i * (m_iNumColumnsMinus1+1) + uiColumnIdx)->getTileHeight();
      }
      this->getTComTile(uiTileIdx)->setBottomEdgePosInCU(uiBottomEdgePosInCU-1);

      //initialize the FirstCUAddr for each tile
      this->getTComTile(uiTileIdx)->setFirstCUAddr( (this->getTComTile(uiTileIdx)->getBottomEdgePosInCU() - this->getTComTile(uiTileIdx)->getTileHeight() +1)*m_uiWidthInCU + 
        this->getTComTile(uiTileIdx)->getRightEdgePosInCU() - this->getTComTile(uiTileIdx)->getTileWidth() + 1);
    }
  }

  //initialize the TileIdxMap
  for( i=0; i<m_uiNumCUsInFrame; i++)
  {
    for(j=0; j < m_iNumColumnsMinus1+1; j++)
    {
      if(i % m_uiWidthInCU <= this->getTComTile(j)->getRightEdgePosInCU())
      {
        uiColumnIdx = j;
        j = m_iNumColumnsMinus1+1;
      }
    }
    for(j=0; j < m_iNumRowsMinus1+1; j++)
    {
      if(i/m_uiWidthInCU <= this->getTComTile(j*(m_iNumColumnsMinus1 + 1))->getBottomEdgePosInCU())
      {
        uiRowIdx = j;
        j = m_iNumRowsMinus1 + 1;
      }
    }
    m_puiTileIdxMap[i] = uiRowIdx * (m_iNumColumnsMinus1 + 1) + uiColumnIdx;
  }

}

UInt TComPicSym::xCalculateNxtCUAddr( UInt uiCurrCUAddr )
{
  UInt  uiNxtCUAddr;
  UInt  uiTileIdx;
  
  //get the tile index for the current LCU
  uiTileIdx = this->getTileIdxMap(uiCurrCUAddr);

  //get the raster scan address for the next LCU
  if( uiCurrCUAddr % m_uiWidthInCU == this->getTComTile(uiTileIdx)->getRightEdgePosInCU() && uiCurrCUAddr / m_uiWidthInCU == this->getTComTile(uiTileIdx)->getBottomEdgePosInCU() )
  //the current LCU is the last LCU of the tile
  {
    if(uiTileIdx == (m_iNumColumnsMinus1+1)*(m_iNumRowsMinus1+1)-1)
    {
      uiNxtCUAddr = m_uiNumCUsInFrame;
    }
    else
    {
      uiNxtCUAddr = this->getTComTile(uiTileIdx+1)->getFirstCUAddr();
    }
  } 
  else //the current LCU is not the last LCU of the tile
  {
    if( uiCurrCUAddr % m_uiWidthInCU == this->getTComTile(uiTileIdx)->getRightEdgePosInCU() )  //the current LCU is on the rightmost edge of the tile
    {
      uiNxtCUAddr = uiCurrCUAddr + m_uiWidthInCU - this->getTComTile(uiTileIdx)->getTileWidth() + 1;
    }
    else
    {
      uiNxtCUAddr = uiCurrCUAddr + 1;
    }
  }

  return uiNxtCUAddr;
}


#if HM_CLEANUP_SAO
Void TComPicSym::deriveLoopFilterBoundaryAvailibility(Int ctu,
                                                      Bool& isLeftAvail,
                                                      Bool& isRightAvail,
                                                      Bool& isAboveAvail,
                                                      Bool& isBelowAvail,
                                                      Bool& isAboveLeftAvail,
                                                      Bool& isAboveRightAvail,
                                                      Bool& isBelowLeftAvail,
                                                      Bool& isBelowRightAvail
                                                      )
{

  isLeftAvail      = (ctu % m_uiWidthInCU != 0);
  isRightAvail     = (ctu % m_uiWidthInCU != m_uiWidthInCU-1);
  isAboveAvail     = (ctu >= m_uiWidthInCU );
  isBelowAvail     = (ctu <  m_uiNumCUsInFrame - m_uiWidthInCU);
  isAboveLeftAvail = (isAboveAvail && isLeftAvail);
  isAboveRightAvail= (isAboveAvail && isRightAvail);
  isBelowLeftAvail = (isBelowAvail && isLeftAvail);
  isBelowRightAvail= (isBelowAvail && isRightAvail);

  Bool isLoopFiltAcrossTilePPS = getCU(ctu)->getSlice()->getPPS()->getLoopFilterAcrossTilesEnabledFlag();

  {
    TComDataCU* ctuCurr  = getCU(ctu);
    TComDataCU* ctuLeft  = isLeftAvail ?getCU(ctu-1):NULL;
    TComDataCU* ctuRight = isRightAvail?getCU(ctu+1):NULL;
    TComDataCU* ctuAbove = isAboveAvail?getCU(ctu-m_uiWidthInCU):NULL;
    TComDataCU* ctuBelow = isBelowAvail?getCU(ctu+m_uiWidthInCU):NULL;
    TComDataCU* ctuAboveLeft  = isAboveLeftAvail ? getCU(ctu-m_uiWidthInCU-1):NULL;
    TComDataCU* ctuAboveRigtht= isAboveRightAvail? getCU(ctu-m_uiWidthInCU+1):NULL;
    TComDataCU* ctuBelowLeft  = isBelowLeftAvail ? getCU(ctu+m_uiWidthInCU-1):NULL;
    TComDataCU* ctuBelowRight = isBelowRightAvail? getCU(ctu+m_uiWidthInCU+1):NULL;

    {
      //left
      if(ctuLeft != NULL)
      {
        isLeftAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuLeft->getSlice()->getSliceCurStartCUAddr())?ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //above
      if(ctuAbove != NULL)
      {
        isAboveAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuAbove->getSlice()->getSliceCurStartCUAddr())?ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //right
      if(ctuRight != NULL)
      {
        isRightAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuRight->getSlice()->getSliceCurStartCUAddr())?ctuRight->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //below
      if(ctuBelow != NULL)
      {
        isBelowAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuBelow->getSlice()->getSliceCurStartCUAddr())?ctuBelow->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //above-left
      if(ctuAboveLeft != NULL)
      {
        isAboveLeftAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuAboveLeft->getSlice()->getSliceCurStartCUAddr())?ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }
      //below-right
      if(ctuBelowRight != NULL)
      {
        isBelowRightAvail = (ctuCurr->getSlice()->getSliceCurStartCUAddr() != ctuBelowRight->getSlice()->getSliceCurStartCUAddr())?ctuBelowRight->getSlice()->getLFCrossSliceBoundaryFlag():true;
      }


      //above-right
      if(ctuAboveRigtht != NULL)
      {
        Int curSliceStartTS  = getCUOrderMap(ctuCurr->getSlice()->getSliceCurStartCUAddr()/m_uiNumPartitions);
        Int aboveRigthtSliceStartTS = getCUOrderMap(ctuAboveRigtht->getSlice()->getSliceCurStartCUAddr()/m_uiNumPartitions);

        isAboveRightAvail = (curSliceStartTS == aboveRigthtSliceStartTS)?(true):
          (
          (curSliceStartTS > aboveRigthtSliceStartTS)?(ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag())
          :(ctuAboveRigtht->getSlice()->getLFCrossSliceBoundaryFlag())
          );          
      }
      //below-left
      if(ctuBelowLeft != NULL)
      {
        Int curSliceStartTS       = getCUOrderMap(ctuCurr->getSlice()->getSliceCurStartCUAddr()/m_uiNumPartitions);
        Int belowLeftSliceStartTS = getCUOrderMap(ctuBelowLeft->getSlice()->getSliceCurStartCUAddr()/m_uiNumPartitions);

        isBelowLeftAvail = (curSliceStartTS == belowLeftSliceStartTS)?(true):
          (
          (curSliceStartTS > belowLeftSliceStartTS)?(ctuCurr->getSlice()->getLFCrossSliceBoundaryFlag())
          :(ctuBelowLeft->getSlice()->getLFCrossSliceBoundaryFlag())
          );
      }        
    }

    if(!isLoopFiltAcrossTilePPS)
    {      
      isLeftAvail      = (!isLeftAvail      ) ?false:(getTileIdxMap( ctuLeft->getAddr()         ) == getTileIdxMap( ctu ));
      isAboveAvail     = (!isAboveAvail     ) ?false:(getTileIdxMap( ctuAbove->getAddr()        ) == getTileIdxMap( ctu ));
      isRightAvail     = (!isRightAvail     ) ?false:(getTileIdxMap( ctuRight->getAddr()        ) == getTileIdxMap( ctu ));
      isBelowAvail     = (!isBelowAvail     ) ?false:(getTileIdxMap( ctuBelow->getAddr()        ) == getTileIdxMap( ctu ));
      isAboveLeftAvail = (!isAboveLeftAvail ) ?false:(getTileIdxMap( ctuAboveLeft->getAddr()    ) == getTileIdxMap( ctu ));
      isAboveRightAvail= (!isAboveRightAvail) ?false:(getTileIdxMap( ctuAboveRigtht->getAddr()  ) == getTileIdxMap( ctu ));
      isBelowLeftAvail = (!isBelowLeftAvail ) ?false:(getTileIdxMap( ctuBelowLeft->getAddr()    ) == getTileIdxMap( ctu ));
      isBelowRightAvail= (!isBelowRightAvail) ?false:(getTileIdxMap( ctuBelowRight->getAddr()   ) == getTileIdxMap( ctu ));
    }
  }

}
#else
Void TComPicSym::allocSaoParam(TComSampleAdaptiveOffset *sao)
{
  m_saoParam = new SAOParam;
  sao->allocSaoParam(m_saoParam);
}
#endif

TComTile::TComTile()
{
}

TComTile::~TComTile()
{
}
//! \}
