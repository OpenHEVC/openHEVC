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

/** \file     TDecSlice.cpp
    \brief    slice decoder class
*/

#include "TDecSlice.h"

//! \ingroup TLibDecoder
//! \{

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TDecSlice::TDecSlice()
{
  m_pcBufferSbacDecoders = NULL;
  m_pcBufferBinCABACs    = NULL;
  m_pcBufferLowLatSbacDecoders = NULL;
  m_pcBufferLowLatBinCABACs    = NULL;
}

TDecSlice::~TDecSlice()
{
}

Void TDecSlice::create( TComSlice* pcSlice, Int iWidth, Int iHeight, UInt uiMaxWidth, UInt uiMaxHeight, UInt uiMaxDepth )
{
}

Void TDecSlice::destroy()
{
  if ( m_pcBufferSbacDecoders )
  {
    delete[] m_pcBufferSbacDecoders;
    m_pcBufferSbacDecoders = NULL;
  }
  if ( m_pcBufferBinCABACs )
  {
    delete[] m_pcBufferBinCABACs;
    m_pcBufferBinCABACs = NULL;
  }
  if ( m_pcBufferLowLatSbacDecoders )
  {
    delete[] m_pcBufferLowLatSbacDecoders;
    m_pcBufferLowLatSbacDecoders = NULL;
  }
  if ( m_pcBufferLowLatBinCABACs )
  {
    delete[] m_pcBufferLowLatBinCABACs;
    m_pcBufferLowLatBinCABACs = NULL;
  }
}

Void TDecSlice::init(TDecEntropy* pcEntropyDecoder, TDecCu* pcCuDecoder)
{
  m_pcEntropyDecoder  = pcEntropyDecoder;
  m_pcCuDecoder       = pcCuDecoder;
}

Void TDecSlice::decompressSlice(TComInputBitstream* pcBitstream, TComInputBitstream** ppcSubstreams, TComPic*& rpcPic, TDecSbac* pcSbacDecoder, TDecSbac* pcSbacDecoders)
{
  TComDataCU* pcCU;
  UInt        uiIsLast = 0;
  Int   iStartCUEncOrder = max(rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getSliceCurStartCUAddr()/rpcPic->getNumPartInCU(), rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getDependentSliceCurStartCUAddr()/rpcPic->getNumPartInCU());
  Int   iStartCUAddr = rpcPic->getPicSym()->getCUOrderMap(iStartCUEncOrder);

  // decoder don't need prediction & residual frame buffer
  rpcPic->setPicYuvPred( 0 );
  rpcPic->setPicYuvResi( 0 );
  
#if ENC_DEC_TRACE
  g_bJustDoIt = g_bEncDecTraceEnable;
#endif
  DTRACE_CABAC_VL( g_nSymbolCounter++ );
  DTRACE_CABAC_T( "\tPOC: " );
  DTRACE_CABAC_V( rpcPic->getPOC() );
  DTRACE_CABAC_T( "\n" );

#if ENC_DEC_TRACE
  g_bJustDoIt = g_bEncDecTraceDisable;
#endif

  UInt uiTilesAcross   = rpcPic->getPicSym()->getNumColumnsMinus1()+1;
  TComSlice*  pcSlice = rpcPic->getSlice(rpcPic->getCurrSliceIdx());
  Int  iNumSubstreams = pcSlice->getPPS()->getNumSubstreams();

  // delete decoders if already allocated in previous slice
  if (m_pcBufferSbacDecoders)
  {
    delete [] m_pcBufferSbacDecoders;
  }
  if (m_pcBufferBinCABACs) 
  {
    delete [] m_pcBufferBinCABACs;
  }
  // allocate new decoders based on tile numbaer
  m_pcBufferSbacDecoders = new TDecSbac    [uiTilesAcross];  
  m_pcBufferBinCABACs    = new TDecBinCABAC[uiTilesAcross];
  for (UInt ui = 0; ui < uiTilesAcross; ui++)
  {
    m_pcBufferSbacDecoders[ui].init(&m_pcBufferBinCABACs[ui]);
  }
  //save init. state
  for (UInt ui = 0; ui < uiTilesAcross; ui++)
  {
    m_pcBufferSbacDecoders[ui].load(pcSbacDecoder);
  }

  // free memory if already allocated in previous call
  if (m_pcBufferLowLatSbacDecoders)
  {
    delete [] m_pcBufferLowLatSbacDecoders;
  }
  if (m_pcBufferLowLatBinCABACs)
  {
    delete [] m_pcBufferLowLatBinCABACs;
  }
  m_pcBufferLowLatSbacDecoders = new TDecSbac    [uiTilesAcross];  
  m_pcBufferLowLatBinCABACs    = new TDecBinCABAC[uiTilesAcross];
  for (UInt ui = 0; ui < uiTilesAcross; ui++)
  {
    m_pcBufferLowLatSbacDecoders[ui].init(&m_pcBufferLowLatBinCABACs[ui]);
  }
  //save init. state
  for (UInt ui = 0; ui < uiTilesAcross; ui++)
  {
    m_pcBufferLowLatSbacDecoders[ui].load(pcSbacDecoder);
  }

  UInt uiWidthInLCUs  = rpcPic->getPicSym()->getFrameWidthInCU();
  //UInt uiHeightInLCUs = rpcPic->getPicSym()->getFrameHeightInCU();
  UInt uiCol=0, uiLin=0, uiSubStrm=0;

  UInt uiTileCol;
  UInt uiTileStartLCU;
  UInt uiTileLCUX;
  UInt uiTileLCUY;
  UInt uiTileWidth;
  UInt uiTileHeight;
  Int iNumSubstreamsPerTile = 1; // if independent.
#if DEPENDENT_SLICES
  Bool bAllowDependence = false;
  if( rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getPPS()->getDependentSlicesEnabledFlag()&& (!rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getPPS()->getCabacIndependentFlag()) )
  {
    bAllowDependence = true;
  }
  if( bAllowDependence )
  {
    if( !rpcPic->getSlice(rpcPic->getCurrSliceIdx())->isNextSlice() )
    {
      uiTileCol = 0;
      if(pcSlice->getPPS()->getTilesOrEntropyCodingSyncIdc()==2)
      {
        m_pcBufferSbacDecoders[uiTileCol].loadContexts( rpcPic->getSlice(rpcPic->getCurrSliceIdx()-1)->getCTXMem_dec( 0 ) );//2.LCU
      }
      pcSbacDecoder->loadContexts( rpcPic->getSlice( rpcPic->getCurrSliceIdx() - 1 )->getCTXMem_dec( 1 ) ); //end of depSlice-1
      pcSbacDecoders[uiSubStrm].loadContexts(pcSbacDecoder);
    }
  }
#endif
  for( Int iCUAddr = iStartCUAddr; !uiIsLast && iCUAddr < rpcPic->getNumCUsInFrame(); iCUAddr = rpcPic->getPicSym()->xCalculateNxtCUAddr(iCUAddr) )
  {
    pcCU = rpcPic->getCU( iCUAddr );
    pcCU->initCU( rpcPic, iCUAddr );
    uiTileCol = rpcPic->getPicSym()->getTileIdxMap(iCUAddr) % (rpcPic->getPicSym()->getNumColumnsMinus1()+1); // what column of tiles are we in?
    uiTileStartLCU = rpcPic->getPicSym()->getTComTile(rpcPic->getPicSym()->getTileIdxMap(iCUAddr))->getFirstCUAddr();
    uiTileLCUX = uiTileStartLCU % uiWidthInLCUs;
    uiTileLCUY = uiTileStartLCU / uiWidthInLCUs;
    uiTileWidth = rpcPic->getPicSym()->getTComTile(rpcPic->getPicSym()->getTileIdxMap(iCUAddr))->getTileWidth();
    uiTileHeight = rpcPic->getPicSym()->getTComTile(rpcPic->getPicSym()->getTileIdxMap(iCUAddr))->getTileHeight();
    uiCol     = iCUAddr % uiWidthInLCUs;
    uiLin     = iCUAddr / uiWidthInLCUs;
    // inherit from TR if necessary, select substream to use.
#if DEPENDENT_SLICES
    if( (pcSlice->getPPS()->getNumSubstreams() > 1) || ( bAllowDependence  && (uiCol == uiTileLCUX)&&(pcSlice->getPPS()->getTilesOrEntropyCodingSyncIdc()==2) ))
#else
    if( pcSlice->getPPS()->getNumSubstreams() > 1 )
#endif
    {
      // independent tiles => substreams are "per tile".  iNumSubstreams has already been multiplied.
      iNumSubstreamsPerTile = iNumSubstreams/rpcPic->getPicSym()->getNumTiles();
      uiSubStrm = rpcPic->getPicSym()->getTileIdxMap(iCUAddr)*iNumSubstreamsPerTile
                  + uiLin%iNumSubstreamsPerTile;
      m_pcEntropyDecoder->setBitstream( ppcSubstreams[uiSubStrm] );
      // Synchronize cabac probabilities with upper-right LCU if it's available and we're at the start of a line.
#if DEPENDENT_SLICES
      if (((pcSlice->getPPS()->getNumSubstreams() > 1) || bAllowDependence ) && (uiCol == uiTileLCUX)&&(pcSlice->getPPS()->getTilesOrEntropyCodingSyncIdc()==2))
#else
      if (pcSlice->getPPS()->getNumSubstreams() > 1 && uiCol == uiTileLCUX)
#endif
      {
        // We'll sync if the TR is available.
        TComDataCU *pcCUUp = pcCU->getCUAbove();
        UInt uiWidthInCU = rpcPic->getFrameWidthInCU();
        TComDataCU *pcCUTR = NULL;
        if ( pcCUUp && ((iCUAddr%uiWidthInCU+1) < uiWidthInCU)  )
        {
          pcCUTR = rpcPic->getCU( iCUAddr - uiWidthInCU + 1 );
        }
        UInt uiMaxParts = 1<<(pcSlice->getSPS()->getMaxCUDepth()<<1);

        if ( (true/*bEnforceSliceRestriction*/ &&
             ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || 
             ((pcCUTR->getSCUAddr()+uiMaxParts-1) < pcSlice->getSliceCurStartCUAddr()) ||
             ((rpcPic->getPicSym()->getTileIdxMap( pcCUTR->getAddr() ) != rpcPic->getPicSym()->getTileIdxMap(iCUAddr)))
             ))||
             (true/*bEnforceDependentSliceRestriction*/ &&
             ((pcCUTR==NULL) || (pcCUTR->getSlice()==NULL) || 
             ((pcCUTR->getSCUAddr()+uiMaxParts-1) < pcSlice->getDependentSliceCurStartCUAddr()) ||
             ((rpcPic->getPicSym()->getTileIdxMap( pcCUTR->getAddr() ) != rpcPic->getPicSym()->getTileIdxMap(iCUAddr)))
             ))
           )
        {
#if DEPENDENT_SLICES
          if( (iCUAddr!=0) && ((pcCUTR->getSCUAddr()+uiMaxParts-1) >= pcSlice->getSliceCurStartCUAddr()) && bAllowDependence)
          {
             pcSbacDecoders[uiSubStrm].loadContexts( &m_pcBufferSbacDecoders[uiTileCol] ); 
          }
#endif
          // TR not available.
        }
        else
        {
          // TR is available, we use it.
          pcSbacDecoders[uiSubStrm].loadContexts( &m_pcBufferSbacDecoders[uiTileCol] );
        }
      }
      pcSbacDecoder->load(&pcSbacDecoders[uiSubStrm]);  //this load is used to simplify the code (avoid to change all the call to pcSbacDecoders)
    }
    else if ( pcSlice->getPPS()->getNumSubstreams() <= 1 )
    {
      // Set variables to appropriate values to avoid later code change.
      iNumSubstreamsPerTile = 1;
    }

    if ( (iCUAddr == rpcPic->getPicSym()->getTComTile(rpcPic->getPicSym()->getTileIdxMap(iCUAddr))->getFirstCUAddr()) && // 1st in tile.
         (iCUAddr!=0) && (iCUAddr!=rpcPic->getPicSym()->getPicSCUAddr(rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getSliceCurStartCUAddr())/rpcPic->getNumPartInCU())
#if DEPENDENT_SLICES
         && (iCUAddr!=rpcPic->getPicSym()->getPicSCUAddr(rpcPic->getSlice(rpcPic->getCurrSliceIdx())->getDependentSliceCurStartCUAddr())/rpcPic->getNumPartInCU())
#endif
         ) // !1st in frame && !1st in slice
    {
      if (pcSlice->getPPS()->getNumSubstreams() > 1)
      {
        // We're crossing into another tile, tiles are independent.
        // When tiles are independent, we have "substreams per tile".  Each substream has already been terminated, and we no longer
        // have to perform it here.
        // For TILES_DECODER, there can be a header at the start of the 1st substream in a tile.  These are read when the substreams
        // are extracted, not here.
      }
      else
      {
        SliceType sliceType  = pcSlice->getSliceType();
        if (pcSlice->getCabacInitFlag())
        {
          switch (sliceType)
          {
          case P_SLICE:           // change initialization table to B_SLICE intialization
            sliceType = B_SLICE; 
            break;
          case B_SLICE:           // change initialization table to P_SLICE intialization
            sliceType = P_SLICE; 
            break;
          default     :           // should not occur
            assert(0);
          }
        }
        m_pcEntropyDecoder->updateContextTables( sliceType, pcSlice->getSliceQp() );
      }
      
    }

#if ENC_DEC_TRACE
    g_bJustDoIt = g_bEncDecTraceEnable;
#endif
#if !SAO_LUM_CHROMA_ONOFF_FLAGS
    if ( pcSlice->getSPS()->getUseSAO() && pcSlice->getSaoEnabledFlag() )
#else
    if ( pcSlice->getSPS()->getUseSAO() && (pcSlice->getSaoEnabledFlag()||pcSlice->getSaoEnabledFlagChroma()) )
#endif
    {
#if REMOVE_APS
      SAOParam *saoParam = rpcPic->getPicSym()->getSaoParam();
#else
      SAOParam *saoParam = pcSlice->getAPS()->getSaoParam();
#endif
      saoParam->bSaoFlag[0] = pcSlice->getSaoEnabledFlag();
      if (iCUAddr == iStartCUAddr)
      {
#if SAO_TYPE_SHARING
        saoParam->bSaoFlag[1] = pcSlice->getSaoEnabledFlagChroma();
#else
        saoParam->bSaoFlag[1] = pcSlice->getSaoEnabledFlagCb();
        saoParam->bSaoFlag[2] = pcSlice->getSaoEnabledFlagCr();
#endif
      }
      Int numCuInWidth     = saoParam->numCuInWidth;
      Int cuAddrInSlice = iCUAddr - rpcPic->getPicSym()->getCUOrderMap(pcSlice->getSliceCurStartCUAddr()/rpcPic->getNumPartInCU());
      Int cuAddrUpInSlice  = cuAddrInSlice - numCuInWidth;
      Int rx = iCUAddr % numCuInWidth;
      Int ry = iCUAddr / numCuInWidth;
      Int allowMergeLeft = 1;
      Int allowMergeUp   = 1;
      if (rx!=0)
      {
        if (rpcPic->getPicSym()->getTileIdxMap(iCUAddr-1) != rpcPic->getPicSym()->getTileIdxMap(iCUAddr))
        {
          allowMergeLeft = 0;
        }
      }
      if (ry!=0)
      {
        if (rpcPic->getPicSym()->getTileIdxMap(iCUAddr-numCuInWidth) != rpcPic->getPicSym()->getTileIdxMap(iCUAddr))
        {
          allowMergeUp = 0;
        }
      }
      pcSbacDecoder->parseSaoOneLcuInterleaving(rx, ry, saoParam,pcCU, cuAddrInSlice, cuAddrUpInSlice, allowMergeLeft, allowMergeUp);
    }
#if !REMOVE_ALF
    if(pcSlice->getSPS()->getUseALF())
    {
      UInt alfEnabledFlag;
      for(Int compIdx=0; compIdx< 3; compIdx++)
      {
        alfEnabledFlag = 0;
        if(pcSlice->getAlfEnabledFlag(compIdx))
        {
          pcSbacDecoder->parseAlfCtrlFlag(compIdx, alfEnabledFlag);
        }
        pcCU->setAlfLCUEnabled((alfEnabledFlag==1)?true:false, compIdx);
      }
    }
#endif
    m_pcCuDecoder->decodeCU     ( pcCU, uiIsLast );
    m_pcCuDecoder->decompressCU ( pcCU );
    
#if ENC_DEC_TRACE
    g_bJustDoIt = g_bEncDecTraceDisable;
#endif
    /*If at the end of a LCU line but not at the end of a substream, perform CABAC flush*/
    if (!uiIsLast && pcSlice->getPPS()->getNumSubstreams() > 1)
    {
      if ((uiCol == uiTileLCUX+uiTileWidth-1) && (uiLin+iNumSubstreamsPerTile < uiTileLCUY+uiTileHeight))
      {
        m_pcEntropyDecoder->decodeFlush();
      }
    }
    pcSbacDecoders[uiSubStrm].load(pcSbacDecoder);

    //Store probabilities of second LCU in line into buffer
#if DEPENDENT_SLICES
    if ( (uiCol == uiTileLCUX+1)&& (bAllowDependence || (pcSlice->getPPS()->getNumSubstreams() > 1)) && (pcSlice->getPPS()->getTilesOrEntropyCodingSyncIdc() == 2))
#else
    if (pcSlice->getPPS()->getNumSubstreams() > 1 && (uiCol == uiTileLCUX+1))
#endif
    {
      m_pcBufferSbacDecoders[uiTileCol].loadContexts( &pcSbacDecoders[uiSubStrm] );
    }
#if DEPENDENT_SLICES
    if( uiIsLast && bAllowDependence )
    {
       if (pcSlice->getPPS()->getTilesOrEntropyCodingSyncIdc()==2)
       {
         rpcPic->getSlice( rpcPic->getCurrSliceIdx() )->getCTXMem_dec( 0 )->loadContexts( &m_pcBufferSbacDecoders[uiTileCol] );//ctx 2.LCU
       }
      rpcPic->getSlice( rpcPic->getCurrSliceIdx() )->getCTXMem_dec( 1 )->loadContexts( pcSbacDecoder );//ctx end of dep.slice
      return;
    }
#endif
  }
}

ParameterSetManagerDecoder::ParameterSetManagerDecoder()
: m_vpsBuffer(MAX_NUM_VPS)
,m_spsBuffer(256)
, m_ppsBuffer(16)
#if !REMOVE_APS
, m_apsBuffer(64)
#endif
{

}

ParameterSetManagerDecoder::~ParameterSetManagerDecoder()
{

}

TComVPS* ParameterSetManagerDecoder::getPrefetchedVPS  (Int vpsId)
{
  if (m_vpsBuffer.getPS(vpsId) != NULL )
  {
    return m_vpsBuffer.getPS(vpsId);
  }
  else
  {
    return getVPS(vpsId);
  }
}


TComSPS* ParameterSetManagerDecoder::getPrefetchedSPS  (Int spsId)
{
  if (m_spsBuffer.getPS(spsId) != NULL )
  {
    return m_spsBuffer.getPS(spsId);
  }
  else
  {
    return getSPS(spsId);
  }
}

TComPPS* ParameterSetManagerDecoder::getPrefetchedPPS  (Int ppsId)
{
  if (m_ppsBuffer.getPS(ppsId) != NULL )
  {
    return m_ppsBuffer.getPS(ppsId);
  }
  else
  {
    return getPPS(ppsId);
  }
}

#if !REMOVE_APS
TComAPS* ParameterSetManagerDecoder::getPrefetchedAPS  (Int apsId)
{
  if (m_apsBuffer.getPS(apsId) != NULL )
  {
    return m_apsBuffer.getPS(apsId);
  }
  else
  {
    return getAPS(apsId);
  }
}
#endif

Void     ParameterSetManagerDecoder::applyPrefetchedPS()
{
  m_vpsMap.mergePSList(m_vpsBuffer);
#if !REMOVE_APS
  m_apsMap.mergePSList(m_apsBuffer);
#endif
  m_ppsMap.mergePSList(m_ppsBuffer);
  m_spsMap.mergePSList(m_spsBuffer);
}

//! \}
