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

/** \file     TEncRateCtrl.cpp
    \brief    Rate control manager class
*/
#include "TEncRateCtrl.h"
#include "../TLibCommon/TComPic.h"

#include <cmath>

using namespace std;

#if RATE_CONTROL_LAMBDA_DOMAIN

//sequence level
TEncRCSeq::TEncRCSeq()
{
  m_totalFrames         = 0;
  m_targetRate          = 0;
  m_frameRate           = 0;
  m_targetBits          = 0;
  m_GOPSize             = 0;
  m_picWidth            = 0;
  m_picHeight           = 0;
  m_LCUWidth            = 0;
  m_LCUHeight           = 0;
  m_numberOfLevel       = 0;
  m_numberOfLCU         = 0;
  m_averageBits         = 0;
  m_bitsRatio           = NULL;
  m_GOPID2Level         = NULL;
  m_picPara             = NULL;
  m_LCUPara             = NULL;
  m_numberOfPixel       = 0;
  m_framesLeft          = 0;
  m_bitsLeft            = 0;
  m_useLCUSeparateModel = false;
#if M0036_RC_IMPROVEMENT
  m_adaptiveBit         = 0;
  m_lastLambda          = 0.0;
#endif
}

TEncRCSeq::~TEncRCSeq()
{
  destroy();
}

#if M0036_RC_IMPROVEMENT
Void TEncRCSeq::create( Int totalFrames, Int targetBitrate, Int frameRate, Int GOPSize, Int picWidth, Int picHeight, Int LCUWidth, Int LCUHeight, Int numberOfLevel, Bool useLCUSeparateModel, Int adaptiveBit )
#else
Void TEncRCSeq::create( Int totalFrames, Int targetBitrate, Int frameRate, Int GOPSize, Int picWidth, Int picHeight, Int LCUWidth, Int LCUHeight, Int numberOfLevel, Bool useLCUSeparateModel )
#endif
{
  destroy();
  m_totalFrames         = totalFrames;
  m_targetRate          = targetBitrate;
  m_frameRate           = frameRate;
  m_GOPSize             = GOPSize;
  m_picWidth            = picWidth;
  m_picHeight           = picHeight;
  m_LCUWidth            = LCUWidth;
  m_LCUHeight           = LCUHeight;
  m_numberOfLevel       = numberOfLevel;
  m_useLCUSeparateModel = useLCUSeparateModel;

  m_numberOfPixel   = m_picWidth * m_picHeight;
  m_targetBits      = (Int64)m_totalFrames * (Int64)m_targetRate / (Int64)m_frameRate;
  m_seqTargetBpp = (Double)m_targetRate / (Double)m_frameRate / (Double)m_numberOfPixel;
  if ( m_seqTargetBpp < 0.03 )
  {
    m_alphaUpdate = 0.01;
    m_betaUpdate  = 0.005;
  }
  else if ( m_seqTargetBpp < 0.08 )
  {
    m_alphaUpdate = 0.05;
    m_betaUpdate  = 0.025;
  }
#if M0036_RC_IMPROVEMENT
  else if ( m_seqTargetBpp < 0.2 )
  {
    m_alphaUpdate = 0.1;
    m_betaUpdate  = 0.05;
  }
  else if ( m_seqTargetBpp < 0.5 )
  {
    m_alphaUpdate = 0.2;
    m_betaUpdate  = 0.1;
  }
  else
  {
    m_alphaUpdate = 0.4;
    m_betaUpdate  = 0.2;
  }
#else
  else
  {
    m_alphaUpdate = 0.1;
    m_betaUpdate  = 0.05;
  }
#endif
  m_averageBits     = (Int)(m_targetBits / totalFrames);
  Int picWidthInBU  = ( m_picWidth  % m_LCUWidth  ) == 0 ? m_picWidth  / m_LCUWidth  : m_picWidth  / m_LCUWidth  + 1;
  Int picHeightInBU = ( m_picHeight % m_LCUHeight ) == 0 ? m_picHeight / m_LCUHeight : m_picHeight / m_LCUHeight + 1;
  m_numberOfLCU     = picWidthInBU * picHeightInBU;

  m_bitsRatio   = new Int[m_GOPSize];
  for ( Int i=0; i<m_GOPSize; i++ )
  {
    m_bitsRatio[i] = 1;
  }

  m_GOPID2Level = new Int[m_GOPSize];
  for ( Int i=0; i<m_GOPSize; i++ )
  {
    m_GOPID2Level[i] = 1;
  }

  m_picPara = new TRCParameter[m_numberOfLevel];
  for ( Int i=0; i<m_numberOfLevel; i++ )
  {
    m_picPara[i].m_alpha = 0.0;
    m_picPara[i].m_beta  = 0.0;
  }

  if ( m_useLCUSeparateModel )
  {
    m_LCUPara = new TRCParameter*[m_numberOfLevel];
    for ( Int i=0; i<m_numberOfLevel; i++ )
    {
      m_LCUPara[i] = new TRCParameter[m_numberOfLCU];
      for ( Int j=0; j<m_numberOfLCU; j++)
      {
        m_LCUPara[i][j].m_alpha = 0.0;
        m_LCUPara[i][j].m_beta  = 0.0;
      }
    }
  }

  m_framesLeft = m_totalFrames;
  m_bitsLeft   = m_targetBits;
#if M0036_RC_IMPROVEMENT
  m_adaptiveBit = adaptiveBit;
  m_lastLambda = 0.0;
#endif
}

Void TEncRCSeq::destroy()
{
  if (m_bitsRatio != NULL)
  {
    delete[] m_bitsRatio;
    m_bitsRatio = NULL;
  }

  if ( m_GOPID2Level != NULL )
  {
    delete[] m_GOPID2Level;
    m_GOPID2Level = NULL;
  }

  if ( m_picPara != NULL )
  {
    delete[] m_picPara;
    m_picPara = NULL;
  }

  if ( m_LCUPara != NULL )
  {
    for ( Int i=0; i<m_numberOfLevel; i++ )
    {
      delete[] m_LCUPara[i];
    }
    delete[] m_LCUPara;
    m_LCUPara = NULL;
  }
}

Void TEncRCSeq::initBitsRatio( Int bitsRatio[])
{
  for (Int i=0; i<m_GOPSize; i++)
  {
    m_bitsRatio[i] = bitsRatio[i];
  }
}

Void TEncRCSeq::initGOPID2Level( Int GOPID2Level[] )
{
  for ( Int i=0; i<m_GOPSize; i++ )
  {
    m_GOPID2Level[i] = GOPID2Level[i];
  }
}

Void TEncRCSeq::initPicPara( TRCParameter* picPara )
{
  assert( m_picPara != NULL );

  if ( picPara == NULL )
  {
    for ( Int i=0; i<m_numberOfLevel; i++ )
    {
#if RATE_CONTROL_INTRA
      if (i>0)
      {
        m_picPara[i].m_alpha = 3.2003;
        m_picPara[i].m_beta  = -1.367;
      }
      else
      {
        m_picPara[i].m_alpha = ALPHA;   
        m_picPara[i].m_beta  = BETA2;
      }
#else
      m_picPara[i].m_alpha = 3.2003;
      m_picPara[i].m_beta  = -1.367;
#endif
    }
  }
  else
  {
    for ( Int i=0; i<m_numberOfLevel; i++ )
    {
      m_picPara[i] = picPara[i];
    }
  }
}

Void TEncRCSeq::initLCUPara( TRCParameter** LCUPara )
{
  if ( m_LCUPara == NULL )
  {
    return;
  }
  if ( LCUPara == NULL )
  {
    for ( Int i=0; i<m_numberOfLevel; i++ )
    {
      for ( Int j=0; j<m_numberOfLCU; j++)
      {
#if RATE_CONTROL_INTRA
        m_LCUPara[i][j].m_alpha = m_picPara[i].m_alpha;
        m_LCUPara[i][j].m_beta  = m_picPara[i].m_beta;
#else
        m_LCUPara[i][j].m_alpha = 3.2003;
        m_LCUPara[i][j].m_beta  = -1.367;
#endif
      }
    }
  }
  else
  {
    for ( Int i=0; i<m_numberOfLevel; i++ )
    {
      for ( Int j=0; j<m_numberOfLCU; j++)
      {
        m_LCUPara[i][j] = LCUPara[i][j];
      }
    }
  }
}

Void TEncRCSeq::updateAfterPic ( Int bits )
{
  m_bitsLeft -= bits;
  m_framesLeft--;
}

#if !RATE_CONTROL_INTRA
Int TEncRCSeq::getRefineBitsForIntra( Int orgBits )
{
  Double bpp = ( (Double)orgBits ) / m_picHeight / m_picHeight;
  if ( bpp > 0.2 )
  {
    return orgBits * 5;
  }
  if ( bpp > 0.1 )
  {
    return orgBits * 7;
  }
  return orgBits * 10;
}
#endif

#if M0036_RC_IMPROVEMENT
Void TEncRCSeq::setAllBitRatio( Double basicLambda, Double* equaCoeffA, Double* equaCoeffB )
{
  Int* bitsRatio = new Int[m_GOPSize];
  for ( Int i=0; i<m_GOPSize; i++ )
  {
    bitsRatio[i] = (Int)( equaCoeffA[i] * pow( basicLambda, equaCoeffB[i] ) * m_numberOfPixel );
  }
  initBitsRatio( bitsRatio );
  delete[] bitsRatio;
}
#endif

//GOP level
TEncRCGOP::TEncRCGOP()
{
  m_encRCSeq  = NULL;
  m_picTargetBitInGOP = NULL;
  m_numPic     = 0;
  m_targetBits = 0;
  m_picLeft    = 0;
  m_bitsLeft   = 0;
}

TEncRCGOP::~TEncRCGOP()
{
  destroy();
}

Void TEncRCGOP::create( TEncRCSeq* encRCSeq, Int numPic )
{
  destroy();
  Int targetBits = xEstGOPTargetBits( encRCSeq, numPic );

#if M0036_RC_IMPROVEMENT
  if ( encRCSeq->getAdaptiveBits() > 0 && encRCSeq->getLastLambda() > 0.1 )
  {
    Double targetBpp = (Double)targetBits / encRCSeq->getNumPixel();
    Double basicLambda = 0.0;
    Double* lambdaRatio = new Double[encRCSeq->getGOPSize()];
    Double* equaCoeffA = new Double[encRCSeq->getGOPSize()];
    Double* equaCoeffB = new Double[encRCSeq->getGOPSize()];

    if ( encRCSeq->getAdaptiveBits() == 1 )   // for GOP size =4, low delay case
    {
      if ( encRCSeq->getLastLambda() < 120.0 )
      {
        lambdaRatio[1] = 0.725 * log( encRCSeq->getLastLambda() ) + 0.5793;
        lambdaRatio[0] = 1.3 * lambdaRatio[1];
        lambdaRatio[2] = 1.3 * lambdaRatio[1];
        lambdaRatio[3] = 1.0;
      }
      else
      {
        lambdaRatio[0] = 5.0;
        lambdaRatio[1] = 4.0;
        lambdaRatio[2] = 5.0;
        lambdaRatio[3] = 1.0;
      }
    }
    else if ( encRCSeq->getAdaptiveBits() == 2 )  // for GOP size = 8, random access case
    {
      if ( encRCSeq->getLastLambda() < 90.0 )
      {
        lambdaRatio[0] = 1.0;
        lambdaRatio[1] = 0.725 * log( encRCSeq->getLastLambda() ) + 0.7963;
        lambdaRatio[2] = 1.3 * lambdaRatio[1];
        lambdaRatio[3] = 3.25 * lambdaRatio[1];
        lambdaRatio[4] = 3.25 * lambdaRatio[1];
        lambdaRatio[5] = 1.3  * lambdaRatio[1];
        lambdaRatio[6] = 3.25 * lambdaRatio[1];
        lambdaRatio[7] = 3.25 * lambdaRatio[1];
      }
      else
      {
        lambdaRatio[0] = 1.0;
        lambdaRatio[1] = 4.0;
        lambdaRatio[2] = 5.0;
        lambdaRatio[3] = 12.3;
        lambdaRatio[4] = 12.3;
        lambdaRatio[5] = 5.0;
        lambdaRatio[6] = 12.3;
        lambdaRatio[7] = 12.3;
      }
    }

    xCalEquaCoeff( encRCSeq, lambdaRatio, equaCoeffA, equaCoeffB, encRCSeq->getGOPSize() );
    basicLambda = xSolveEqua( targetBpp, equaCoeffA, equaCoeffB, encRCSeq->getGOPSize() );
    encRCSeq->setAllBitRatio( basicLambda, equaCoeffA, equaCoeffB );

    delete []lambdaRatio;
    delete []equaCoeffA;
    delete []equaCoeffB;
  }
#endif

  m_picTargetBitInGOP = new Int[numPic];
  Int i;
  Int totalPicRatio = 0;
  Int currPicRatio = 0;
  for ( i=0; i<numPic; i++ )
  {
    totalPicRatio += encRCSeq->getBitRatio( i );
  }
  for ( i=0; i<numPic; i++ )
  {
    currPicRatio = encRCSeq->getBitRatio( i );
#if M0036_RC_IMPROVEMENT
    m_picTargetBitInGOP[i] = (Int)( ((Double)targetBits) * currPicRatio / totalPicRatio );
#else
    m_picTargetBitInGOP[i] = targetBits * currPicRatio / totalPicRatio;
#endif
  }

  m_encRCSeq    = encRCSeq;
  m_numPic       = numPic;
  m_targetBits   = targetBits;
  m_picLeft      = m_numPic;
  m_bitsLeft     = m_targetBits;
}

#if M0036_RC_IMPROVEMENT
Void TEncRCGOP::xCalEquaCoeff( TEncRCSeq* encRCSeq, Double* lambdaRatio, Double* equaCoeffA, Double* equaCoeffB, Int GOPSize )
{
  for ( Int i=0; i<GOPSize; i++ )
  {
    Int frameLevel = encRCSeq->getGOPID2Level(i);
    Double alpha   = encRCSeq->getPicPara(frameLevel).m_alpha;
    Double beta    = encRCSeq->getPicPara(frameLevel).m_beta;
    equaCoeffA[i] = pow( 1.0/alpha, 1.0/beta ) * pow( lambdaRatio[i], 1.0/beta );
    equaCoeffB[i] = 1.0/beta;
  }
}

Double TEncRCGOP::xSolveEqua( Double targetBpp, Double* equaCoeffA, Double* equaCoeffB, Int GOPSize )
{
  Double solution = 100.0;
  Double minNumber = 0.1;
  Double maxNumber = 10000.0;
  for ( Int i=0; i<g_RCIterationNum; i++ )
  { 
    Double fx = 0.0;
    for ( Int j=0; j<GOPSize; j++ )
    {
      fx += equaCoeffA[j] * pow( solution, equaCoeffB[j] );
    }

    if ( fabs( fx - targetBpp ) < 0.000001 )
    {
      break;
    }

    if ( fx > targetBpp )
    {
      minNumber = solution;
      solution = ( solution + maxNumber ) / 2.0;
    }
    else
    {
      maxNumber = solution;
      solution = ( solution + minNumber ) / 2.0;
    }
  }

  solution = Clip3( 0.1, 10000.0, solution );
  return solution;
}
#endif

Void TEncRCGOP::destroy()
{
  m_encRCSeq = NULL;
  if ( m_picTargetBitInGOP != NULL )
  {
    delete[] m_picTargetBitInGOP;
    m_picTargetBitInGOP = NULL;
  }
}

Void TEncRCGOP::updateAfterPicture( Int bitsCost )
{
  m_bitsLeft -= bitsCost;
  m_picLeft--;
}

Int TEncRCGOP::xEstGOPTargetBits( TEncRCSeq* encRCSeq, Int GOPSize )
{
  Int realInfluencePicture = min( g_RCSmoothWindowSize, encRCSeq->getFramesLeft() );
  Int averageTargetBitsPerPic = (Int)( encRCSeq->getTargetBits() / encRCSeq->getTotalFrames() );
  Int currentTargetBitsPerPic = (Int)( ( encRCSeq->getBitsLeft() - averageTargetBitsPerPic * (encRCSeq->getFramesLeft() - realInfluencePicture) ) / realInfluencePicture );
  Int targetBits = currentTargetBitsPerPic * GOPSize;

  if ( targetBits < 200 )
  {
    targetBits = 200;   // at least allocate 200 bits for one GOP
  }

  return targetBits;
}

//picture level
TEncRCPic::TEncRCPic()
{
  m_encRCSeq = NULL;
  m_encRCGOP = NULL;

  m_frameLevel    = 0;
  m_numberOfPixel = 0;
  m_numberOfLCU   = 0;
  m_targetBits    = 0;
  m_estHeaderBits = 0;
  m_estPicQP      = 0;
  m_estPicLambda  = 0.0;

  m_LCULeft       = 0;
  m_bitsLeft      = 0;
  m_pixelsLeft    = 0;

  m_LCUs         = NULL;
#if !M0036_RC_IMPROVEMENT
  m_lastPicture  = NULL;
#endif
  m_picActualHeaderBits = 0;
#if !M0036_RC_IMPROVEMENT
  m_totalMAD            = 0.0;
#endif
  m_picActualBits       = 0;
  m_picQP               = 0;
  m_picLambda           = 0.0;
}

TEncRCPic::~TEncRCPic()
{
  destroy();
}

Int TEncRCPic::xEstPicTargetBits( TEncRCSeq* encRCSeq, TEncRCGOP* encRCGOP )
{
  Int targetBits        = 0;
  Int GOPbitsLeft       = encRCGOP->getBitsLeft();

  Int i;
  Int currPicPosition = encRCGOP->getNumPic()-encRCGOP->getPicLeft();
  Int currPicRatio    = encRCSeq->getBitRatio( currPicPosition );
  Int totalPicRatio   = 0;
  for ( i=currPicPosition; i<encRCGOP->getNumPic(); i++ )
  {
    totalPicRatio += encRCSeq->getBitRatio( i );
  }

#if M0036_RC_IMPROVEMENT
  targetBits  = Int( ((Double)GOPbitsLeft) * currPicRatio / totalPicRatio );
#else
  targetBits  = Int( GOPbitsLeft * currPicRatio / totalPicRatio );
#endif

  if ( targetBits < 100 )
  {
    targetBits = 100;   // at least allocate 100 bits for one picture
  }

  if ( m_encRCSeq->getFramesLeft() > 16 )
  {
    targetBits = Int( g_RCWeightPicRargetBitInBuffer * targetBits + g_RCWeightPicTargetBitInGOP * m_encRCGOP->getTargetBitInGOP( currPicPosition ) );
  }

  return targetBits;
}

Int TEncRCPic::xEstPicHeaderBits( list<TEncRCPic*>& listPreviousPictures, Int frameLevel )
{
  Int numPreviousPics   = 0;
  Int totalPreviousBits = 0;

  list<TEncRCPic*>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == frameLevel )
    {
      totalPreviousBits += (*it)->getPicActualHeaderBits();
      numPreviousPics++;
    }
  }

  Int estHeaderBits = 0;
  if ( numPreviousPics > 0 )
  {
    estHeaderBits = totalPreviousBits / numPreviousPics;
  }

  return estHeaderBits;
}

Void TEncRCPic::addToPictureLsit( list<TEncRCPic*>& listPreviousPictures )
{
  if ( listPreviousPictures.size() > g_RCMaxPicListSize )
  {
    TEncRCPic* p = listPreviousPictures.front();
    listPreviousPictures.pop_front();
    p->destroy();
    delete p;
  }

  listPreviousPictures.push_back( this );
}

Void TEncRCPic::create( TEncRCSeq* encRCSeq, TEncRCGOP* encRCGOP, Int frameLevel, list<TEncRCPic*>& listPreviousPictures )
{
  destroy();
  m_encRCSeq = encRCSeq;
  m_encRCGOP = encRCGOP;

  Int targetBits    = xEstPicTargetBits( encRCSeq, encRCGOP );
  Int estHeaderBits = xEstPicHeaderBits( listPreviousPictures, frameLevel );

  if ( targetBits < estHeaderBits + 100 )
  {
    targetBits = estHeaderBits + 100;   // at least allocate 100 bits for picture data
  }

  m_frameLevel       = frameLevel;
  m_numberOfPixel    = encRCSeq->getNumPixel();
  m_numberOfLCU      = encRCSeq->getNumberOfLCU();
  m_estPicLambda     = 100.0;
  m_targetBits       = targetBits;
  m_estHeaderBits    = estHeaderBits;
  m_bitsLeft         = m_targetBits;
  Int picWidth       = encRCSeq->getPicWidth();
  Int picHeight      = encRCSeq->getPicHeight();
  Int LCUWidth       = encRCSeq->getLCUWidth();
  Int LCUHeight      = encRCSeq->getLCUHeight();
  Int picWidthInLCU  = ( picWidth  % LCUWidth  ) == 0 ? picWidth  / LCUWidth  : picWidth  / LCUWidth  + 1;
  Int picHeightInLCU = ( picHeight % LCUHeight ) == 0 ? picHeight / LCUHeight : picHeight / LCUHeight + 1;

  m_LCULeft         = m_numberOfLCU;
  m_bitsLeft       -= m_estHeaderBits;
  m_pixelsLeft      = m_numberOfPixel;

  m_LCUs           = new TRCLCU[m_numberOfLCU];
  Int i, j;
  Int LCUIdx;
  for ( i=0; i<picWidthInLCU; i++ )
  {
    for ( j=0; j<picHeightInLCU; j++ )
    {
      LCUIdx = j*picWidthInLCU + i;
      m_LCUs[LCUIdx].m_actualBits = 0;
      m_LCUs[LCUIdx].m_QP         = 0;
      m_LCUs[LCUIdx].m_lambda     = 0.0;
      m_LCUs[LCUIdx].m_targetBits = 0;
#if M0036_RC_IMPROVEMENT
      m_LCUs[LCUIdx].m_bitWeight  = 1.0;
#else
      m_LCUs[LCUIdx].m_MAD        = 0.0;
#endif
      Int currWidth  = ( (i == picWidthInLCU -1) ? picWidth  - LCUWidth *(picWidthInLCU -1) : LCUWidth  );
      Int currHeight = ( (j == picHeightInLCU-1) ? picHeight - LCUHeight*(picHeightInLCU-1) : LCUHeight );
      m_LCUs[LCUIdx].m_numberOfPixel = currWidth * currHeight;
    }
  }
  m_picActualHeaderBits = 0;
#if !M0036_RC_IMPROVEMENT
  m_totalMAD            = 0.0;
#endif
  m_picActualBits       = 0;
  m_picQP               = 0;
  m_picLambda           = 0.0;

#if !M0036_RC_IMPROVEMENT
  m_lastPicture = NULL;
  list<TEncRCPic*>::reverse_iterator it;
  for ( it = listPreviousPictures.rbegin(); it != listPreviousPictures.rend(); it++ )
  {
    if ( (*it)->getFrameLevel() == m_frameLevel )
    {
      m_lastPicture = (*it);
      break;
    }
  }
#endif
}

Void TEncRCPic::destroy()
{
  if( m_LCUs != NULL )
  {
    delete[] m_LCUs;
    m_LCUs = NULL;
  }
  m_encRCSeq = NULL;
  m_encRCGOP = NULL;
}


#if RATE_CONTROL_INTRA
Double TEncRCPic::estimatePicLambda( list<TEncRCPic*>& listPreviousPictures, SliceType eSliceType)
#else
Double TEncRCPic::estimatePicLambda( list<TEncRCPic*>& listPreviousPictures )
#endif
{
  Double alpha         = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  Double beta          = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
  Double bpp       = (Double)m_targetBits/(Double)m_numberOfPixel;
#if RATE_CONTROL_INTRA
  Double estLambda;
  if (eSliceType == I_SLICE)
  {
    estLambda = calculateLambdaIntra(alpha, beta, pow(m_totalCostIntra/(Double)m_numberOfPixel, BETA1), bpp); 
  }
  else
  {
    estLambda = alpha * pow( bpp, beta );
  }
#else
  Double estLambda = alpha * pow( bpp, beta );
#endif  
  
  Double lastLevelLambda = -1.0;
  Double lastPicLambda   = -1.0;
  Double lastValidLambda = -1.0;
  list<TEncRCPic*>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == m_frameLevel )
    {
      lastLevelLambda = (*it)->getPicActualLambda();
    }
    lastPicLambda     = (*it)->getPicActualLambda();

    if ( lastPicLambda > 0.0 )
    {
      lastValidLambda = lastPicLambda;
    }
  }

  if ( lastLevelLambda > 0.0 )
  {
    lastLevelLambda = Clip3( 0.1, 10000.0, lastLevelLambda );
    estLambda = Clip3( lastLevelLambda * pow( 2.0, -3.0/3.0 ), lastLevelLambda * pow( 2.0, 3.0/3.0 ), estLambda );
  }

  if ( lastPicLambda > 0.0 )
  {
    lastPicLambda = Clip3( 0.1, 2000.0, lastPicLambda );
    estLambda = Clip3( lastPicLambda * pow( 2.0, -10.0/3.0 ), lastPicLambda * pow( 2.0, 10.0/3.0 ), estLambda );
  }
  else if ( lastValidLambda > 0.0 )
  {
    lastValidLambda = Clip3( 0.1, 2000.0, lastValidLambda );
    estLambda = Clip3( lastValidLambda * pow(2.0, -10.0/3.0), lastValidLambda * pow(2.0, 10.0/3.0), estLambda );
  }
  else
  {
    estLambda = Clip3( 0.1, 10000.0, estLambda );
  }

  if ( estLambda < 0.1 )
  {
    estLambda = 0.1;
  }

  m_estPicLambda = estLambda;

#if M0036_RC_IMPROVEMENT
  Double totalWeight = 0.0;
  // initial BU bit allocation weight
  for ( Int i=0; i<m_numberOfLCU; i++ )
  {
#if RC_FIX
    Double alphaLCU, betaLCU;
    if ( m_encRCSeq->getUseLCUSeparateModel() )
    {
      alphaLCU = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_alpha;
      betaLCU  = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_beta;
    }
    else
    {
      alphaLCU = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
      betaLCU  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
    }
#else
    Double alphaLCU = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_alpha;
    Double betaLCU  = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_beta;
#endif

    m_LCUs[i].m_bitWeight =  m_LCUs[i].m_numberOfPixel * pow( estLambda/alphaLCU, 1.0/betaLCU );

    if ( m_LCUs[i].m_bitWeight < 0.01 )
    {
      m_LCUs[i].m_bitWeight = 0.01;
    }
    totalWeight += m_LCUs[i].m_bitWeight;
  }
  for ( Int i=0; i<m_numberOfLCU; i++ )
  {
    Double BUTargetBits = m_targetBits * m_LCUs[i].m_bitWeight / totalWeight;
    m_LCUs[i].m_bitWeight = BUTargetBits;
  }
#endif

  return estLambda;
}

Int TEncRCPic::estimatePicQP( Double lambda, list<TEncRCPic*>& listPreviousPictures )
{
  Int QP = Int( 4.2005 * log( lambda ) + 13.7122 + 0.5 ); 

  Int lastLevelQP = g_RCInvalidQPValue;
  Int lastPicQP   = g_RCInvalidQPValue;
  Int lastValidQP = g_RCInvalidQPValue;
  list<TEncRCPic*>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == m_frameLevel )
    {
      lastLevelQP = (*it)->getPicActualQP();
    }
    lastPicQP = (*it)->getPicActualQP();
    if ( lastPicQP > g_RCInvalidQPValue )
    {
      lastValidQP = lastPicQP;
    }
  }

  if ( lastLevelQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastLevelQP - 3, lastLevelQP + 3, QP );
  }

  if( lastPicQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastPicQP - 10, lastPicQP + 10, QP );
  }
  else if( lastValidQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastValidQP - 10, lastValidQP + 10, QP );
  }

  return QP;
}

#if RATE_CONTROL_INTRA
Double TEncRCPic::getLCUTargetBpp(SliceType eSliceType)  
#else 
Double TEncRCPic::getLCUTargetBpp()
#endif
{
  Int   LCUIdx    = getLCUCoded();
  Double bpp      = -1.0;
  Int avgBits     = 0;
#if !M0036_RC_IMPROVEMENT
  Double totalMAD = -1.0;
  Double MAD      = -1.0;
#endif

#if RATE_CONTROL_INTRA
  if (eSliceType == I_SLICE){
    Int noOfLCUsLeft = m_numberOfLCU - LCUIdx + 1;
    Int bitrateWindow = min(4,noOfLCUsLeft);
    Double MAD      = getLCU(LCUIdx).m_costIntra;

    if (m_remainingCostIntra > 0.1 )
    {
      Double weightedBitsLeft = (m_bitsLeft*bitrateWindow+(m_bitsLeft-getLCU(LCUIdx).m_targetBitsLeft)*noOfLCUsLeft)/(Double)bitrateWindow;
      avgBits = Int( MAD*weightedBitsLeft/m_remainingCostIntra );
    }
    else
    {
      avgBits = Int( m_bitsLeft / m_LCULeft );
    }
    m_remainingCostIntra -= MAD;
  }
  else
  {
#endif
#if M0036_RC_IMPROVEMENT
  Double totalWeight = 0;
  for ( Int i=LCUIdx; i<m_numberOfLCU; i++ )
  {
    totalWeight += m_LCUs[i].m_bitWeight;
  }
  Int realInfluenceLCU = min( g_RCLCUSmoothWindowSize, getLCULeft() );
  avgBits = (Int)( m_LCUs[LCUIdx].m_bitWeight - ( totalWeight - m_bitsLeft ) / realInfluenceLCU + 0.5 );
#else
  if ( m_lastPicture == NULL )
  {
    avgBits = Int( m_bitsLeft / m_LCULeft );
  }
  else
  {
    MAD = m_lastPicture->getLCU(LCUIdx).m_MAD;
    totalMAD = m_lastPicture->getTotalMAD();
    for ( Int i=0; i<LCUIdx; i++ )
    {
      totalMAD -= m_lastPicture->getLCU(i).m_MAD;
    }

    if ( totalMAD > 0.1 )
    {
      avgBits = Int( m_bitsLeft * MAD / totalMAD );
    }
    else
    {
      avgBits = Int( m_bitsLeft / m_LCULeft );
    }
  }
#endif
#if RATE_CONTROL_INTRA
  }
#endif

  if ( avgBits < 1 )
  {
    avgBits = 1;
  }

  bpp = ( Double )avgBits/( Double )m_LCUs[ LCUIdx ].m_numberOfPixel;
  m_LCUs[ LCUIdx ].m_targetBits = avgBits;

  return bpp;
}

Double TEncRCPic::getLCUEstLambda( Double bpp )
{
  Int   LCUIdx = getLCUCoded();
  Double alpha;
  Double beta;
  if ( m_encRCSeq->getUseLCUSeparateModel() )
  {
    alpha = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_alpha;
    beta  = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_beta;
  }
  else
  {
    alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
    beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
  }

  Double estLambda = alpha * pow( bpp, beta );
  //for Lambda clip, picture level clip
  Double clipPicLambda = m_estPicLambda;

  //for Lambda clip, LCU level clip
  Double clipNeighbourLambda = -1.0;
  for ( int i=LCUIdx - 1; i>=0; i-- )
  {
    if ( m_LCUs[i].m_lambda > 0 )
    {
      clipNeighbourLambda = m_LCUs[i].m_lambda;
      break;
    }
  }

  if ( clipNeighbourLambda > 0.0 )
  {
    estLambda = Clip3( clipNeighbourLambda * pow( 2.0, -1.0/3.0 ), clipNeighbourLambda * pow( 2.0, 1.0/3.0 ), estLambda );
  }  

  if ( clipPicLambda > 0.0 )
  {
    estLambda = Clip3( clipPicLambda * pow( 2.0, -2.0/3.0 ), clipPicLambda * pow( 2.0, 2.0/3.0 ), estLambda );
  }
  else
  {
    estLambda = Clip3( 10.0, 1000.0, estLambda );
  }

  if ( estLambda < 0.1 )
  {
    estLambda = 0.1;
  }

  return estLambda;
}

Int TEncRCPic::getLCUEstQP( Double lambda, Int clipPicQP )
{
  Int LCUIdx = getLCUCoded();
  Int estQP = Int( 4.2005 * log( lambda ) + 13.7122 + 0.5 );

  //for Lambda clip, LCU level clip
  Int clipNeighbourQP = g_RCInvalidQPValue;
  for ( int i=LCUIdx - 1; i>=0; i-- )
  {
    if ( (getLCU(i)).m_QP > g_RCInvalidQPValue )
    {
      clipNeighbourQP = getLCU(i).m_QP;
      break;
    }
  }

  if ( clipNeighbourQP > g_RCInvalidQPValue )
  {
    estQP = Clip3( clipNeighbourQP - 1, clipNeighbourQP + 1, estQP );
  }

  estQP = Clip3( clipPicQP - 2, clipPicQP + 2, estQP );

  return estQP;
}

Void TEncRCPic::updateAfterLCU( Int LCUIdx, Int bits, Int QP, Double lambda, Bool updateLCUParameter )
{
  m_LCUs[LCUIdx].m_actualBits = bits;
  m_LCUs[LCUIdx].m_QP         = QP;
  m_LCUs[LCUIdx].m_lambda     = lambda;

  m_LCULeft--;
  m_bitsLeft   -= bits;
  m_pixelsLeft -= m_LCUs[LCUIdx].m_numberOfPixel;

  if ( !updateLCUParameter )
  {
    return;
  }

  if ( !m_encRCSeq->getUseLCUSeparateModel() )
  {
    return;
  }

  Double alpha = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_alpha;
  Double beta  = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_beta;

  Int LCUActualBits   = m_LCUs[LCUIdx].m_actualBits;
  Int LCUTotalPixels  = m_LCUs[LCUIdx].m_numberOfPixel;
  Double bpp         = ( Double )LCUActualBits/( Double )LCUTotalPixels;
  Double calLambda   = alpha * pow( bpp, beta );
  Double inputLambda = m_LCUs[LCUIdx].m_lambda;

  if( inputLambda < 0.01 || calLambda < 0.01 || bpp < 0.0001 )
  {
    alpha *= ( 1.0 - m_encRCSeq->getAlphaUpdate() / 2.0 );
    beta  *= ( 1.0 - m_encRCSeq->getBetaUpdate() / 2.0 );

#if M0036_RC_IMPROVEMENT
    alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
    beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );
#else
    alpha = Clip3( 0.05, 20.0, alpha );
    beta  = Clip3( -3.0, -0.1, beta  );
#endif

    TRCParameter rcPara;
    rcPara.m_alpha = alpha;
    rcPara.m_beta  = beta;
    m_encRCSeq->setLCUPara( m_frameLevel, LCUIdx, rcPara );

    return;
  }

  calLambda = Clip3( inputLambda / 10.0, inputLambda * 10.0, calLambda );
  alpha += m_encRCSeq->getAlphaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * alpha;
  double lnbpp = log( bpp );
#if M0036_RC_IMPROVEMENT
  lnbpp = Clip3( -5.0, -0.1, lnbpp );
#else
  lnbpp = Clip3( -5.0, 1.0, lnbpp );
#endif
  beta  += m_encRCSeq->getBetaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * lnbpp;

#if M0036_RC_IMPROVEMENT
  alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
  beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );
#else
  alpha = Clip3( 0.05, 20.0, alpha );
  beta  = Clip3( -3.0, -0.1, beta  );
#endif
  TRCParameter rcPara;
  rcPara.m_alpha = alpha;
  rcPara.m_beta  = beta;
  m_encRCSeq->setLCUPara( m_frameLevel, LCUIdx, rcPara );

}

#if !M0036_RC_IMPROVEMENT
Double TEncRCPic::getEffectivePercentage()
{
  Int effectivePiexels = 0;
  Int totalPixels = 0;

  for ( Int i=0; i<m_numberOfLCU; i++ )
  {
    totalPixels += m_LCUs[i].m_numberOfPixel;
    if ( m_LCUs[i].m_QP > 0 )
    {
      effectivePiexels += m_LCUs[i].m_numberOfPixel;
    }
  }

  Double effectivePixelPercentage = (Double)effectivePiexels/(Double)totalPixels;
  return effectivePixelPercentage;
}
#endif

Double TEncRCPic::calAverageQP()
{
  Int totalQPs = 0;
  Int numTotalLCUs = 0;

  Int i;
  for ( i=0; i<m_numberOfLCU; i++ )
  {
    if ( m_LCUs[i].m_QP > 0 )
    {
      totalQPs += m_LCUs[i].m_QP;
      numTotalLCUs++;
    }
  }

  Double avgQP = 0.0;
  if ( numTotalLCUs == 0 )
  {
    avgQP = g_RCInvalidQPValue;
  }
  else
  {
    avgQP = ((Double)totalQPs) / ((Double)numTotalLCUs);
  }
  return avgQP;
}

Double TEncRCPic::calAverageLambda()
{
  Double totalLambdas = 0.0;
  Int numTotalLCUs = 0;

  Int i;
  for ( i=0; i<m_numberOfLCU; i++ )
  {
    if ( m_LCUs[i].m_lambda > 0.01 )
    {
      totalLambdas += log( m_LCUs[i].m_lambda );
      numTotalLCUs++;
    }
  }

  Double avgLambda; 
  if( numTotalLCUs == 0 )
  {
    avgLambda = -1.0;
  }
  else
  {
    avgLambda = pow( 2.7183, totalLambdas / numTotalLCUs );
  }
  return avgLambda;
}

#if M0036_RC_IMPROVEMENT
#if RATE_CONTROL_INTRA
Void TEncRCPic::updateAfterPicture( Int actualHeaderBits, Int actualTotalBits, Double averageQP, Double averageLambda, SliceType eSliceType)
#else
Void TEncRCPic::updateAfterPicture( Int actualHeaderBits, Int actualTotalBits, Double averageQP, Double averageLambda )
#endif
#else
Void TEncRCPic::updateAfterPicture( Int actualHeaderBits, Int actualTotalBits, Double averageQP, Double averageLambda, Double effectivePercentage )
#endif
{
  m_picActualHeaderBits = actualHeaderBits;
  m_picActualBits       = actualTotalBits;
  if ( averageQP > 0.0 )
  {
    m_picQP             = Int( averageQP + 0.5 );
  }
  else
  {
    m_picQP             = g_RCInvalidQPValue;
  }
  m_picLambda           = averageLambda;
#if !M0036_RC_IMPROVEMENT
  for ( Int i=0; i<m_numberOfLCU; i++ )
  {
    m_totalMAD += m_LCUs[i].m_MAD;
  }
#endif

  Double alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  Double beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
#if RATE_CONTROL_INTRA
  if (eSliceType == I_SLICE)
  {
    updateAlphaBetaIntra(&alpha, &beta);
  }
  else
  {
#endif
  // update parameters
  Double picActualBits = ( Double )m_picActualBits;
  Double picActualBpp  = picActualBits/(Double)m_numberOfPixel;
  Double calLambda     = alpha * pow( picActualBpp, beta );
  Double inputLambda   = m_picLambda;

#if M0036_RC_IMPROVEMENT
  if ( inputLambda < 0.01 || calLambda < 0.01 || picActualBpp < 0.0001 )
#else
  if ( inputLambda < 0.01 || calLambda < 0.01 || picActualBpp < 0.0001 || effectivePercentage < 0.05 )
#endif
  {
    alpha *= ( 1.0 - m_encRCSeq->getAlphaUpdate() / 2.0 );
    beta  *= ( 1.0 - m_encRCSeq->getBetaUpdate() / 2.0 );

#if M0036_RC_IMPROVEMENT
    alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
    beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );
#else
    alpha = Clip3( 0.05, 20.0, alpha );
    beta  = Clip3( -3.0, -0.1, beta  );
#endif
    TRCParameter rcPara;
    rcPara.m_alpha = alpha;
    rcPara.m_beta  = beta;
    m_encRCSeq->setPicPara( m_frameLevel, rcPara );

    return;
  }

  calLambda = Clip3( inputLambda / 10.0, inputLambda * 10.0, calLambda );
  alpha += m_encRCSeq->getAlphaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * alpha;
  double lnbpp = log( picActualBpp );
#if M0036_RC_IMPROVEMENT
  lnbpp = Clip3( -5.0, -0.1, lnbpp );
#else
  lnbpp = Clip3( -5.0, 1.0, lnbpp );
#endif
  beta  += m_encRCSeq->getBetaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * lnbpp;

#if M0036_RC_IMPROVEMENT
  alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
  beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );
#else
  alpha = Clip3( 0.05, 20.0, alpha );
  beta  = Clip3( -3.0, -0.1, beta  );
#endif
#if RATE_CONTROL_INTRA
  }
#endif

  TRCParameter rcPara;
  rcPara.m_alpha = alpha;
  rcPara.m_beta  = beta;

  m_encRCSeq->setPicPara( m_frameLevel, rcPara );

#if M0036_RC_IMPROVEMENT
  if ( m_frameLevel == 1 )
  {
    Double currLambda = Clip3( 0.1, 10000.0, m_picLambda );
    Double updateLastLambda = g_RCWeightHistoryLambda * m_encRCSeq->getLastLambda() + g_RCWeightCurrentLambda * currLambda;
    m_encRCSeq->setLastLambda( updateLastLambda );
  }
#endif
}

#if RATE_CONTROL_INTRA
Int TEncRCPic::getRefineBitsForIntra( Int orgBits )
{
  Double alpha=0.25, beta=0.5582;
  Int iIntraBits;

  if (orgBits*40 < m_numberOfPixel)
  {
    alpha=0.25;
  }
  else
  {
    alpha=0.30;
  }

  iIntraBits = (Int)(alpha* pow(m_totalCostIntra*4.0/(Double)orgBits, beta)*(Double)orgBits+0.5);
  
  return iIntraBits;
}

Double TEncRCPic::calculateLambdaIntra(double alpha, double beta, double MADPerPixel, double bitsPerPixel)
{
  return ( (alpha/256.0) * pow( MADPerPixel/bitsPerPixel, beta ) );
}

Void TEncRCPic::updateAlphaBetaIntra(double *alpha, double *beta)
{
  Double lnbpp = log(pow(m_totalCostIntra / (Double)m_numberOfPixel, BETA1));
  Double diffLambda = (*beta)*(log((Double)m_picActualBits)-log((Double)m_targetBits));

  diffLambda = Clip3(-0.125, 0.125, 0.25*diffLambda);
  *alpha    =  (*alpha) * exp(diffLambda);
  *beta     =  (*beta) + diffLambda / lnbpp;
}


Void TEncRCPic::getLCUInitTargetBits()  
{
  Int iAvgBits     = 0;

  m_remainingCostIntra = m_totalCostIntra;
  for (Int i=m_numberOfLCU-1; i>=0; i--)
  {
    iAvgBits += Int(m_targetBits * getLCU(i).m_costIntra/m_totalCostIntra);
    getLCU(i).m_targetBitsLeft = iAvgBits;
  }
}


Double TEncRCPic::getLCUEstLambdaAndQP(Double bpp, Int clipPicQP, Int *estQP) 
{
  Int   LCUIdx = getLCUCoded();

  Double   alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  Double   beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;

  Double costPerPixel = getLCU(LCUIdx).m_costIntra/(Double)getLCU(LCUIdx).m_numberOfPixel;
  costPerPixel = pow(costPerPixel, BETA1);
  Double estLambda = calculateLambdaIntra(alpha, beta, costPerPixel, bpp);

  Int clipNeighbourQP = g_RCInvalidQPValue;
  for (int i=LCUIdx-1; i>=0; i--)
  {
    if ((getLCU(i)).m_QP > g_RCInvalidQPValue)
    {
      clipNeighbourQP = getLCU(i).m_QP;
      break;
    }
  }

  Int minQP = clipPicQP - 2;
  Int maxQP = clipPicQP + 2;

  if ( clipNeighbourQP > g_RCInvalidQPValue )
  {
    maxQP = min(clipNeighbourQP + 1, maxQP); 
    minQP = max(clipNeighbourQP - 1, minQP); 
  }

  Double maxLambda=exp(((Double)(maxQP+0.49)-13.7122)/4.2005);
  Double minLambda=exp(((Double)(minQP-0.49)-13.7122)/4.2005);

  estLambda = Clip3(minLambda, maxLambda, estLambda);

  *estQP = Int( 4.2005 * log(estLambda) + 13.7122 + 0.5 );
  *estQP = Clip3(minQP, maxQP, *estQP);

  return estLambda;
}
#endif

TEncRateCtrl::TEncRateCtrl()
{
  m_encRCSeq = NULL;
  m_encRCGOP = NULL;
  m_encRCPic = NULL;
}

TEncRateCtrl::~TEncRateCtrl()
{
  destroy();
}

Void TEncRateCtrl::destroy()
{
  if ( m_encRCSeq != NULL )
  {
    delete m_encRCSeq;
    m_encRCSeq = NULL;
  }
  if ( m_encRCGOP != NULL )
  {
    delete m_encRCGOP;
    m_encRCGOP = NULL;
  }
  while ( m_listRCPictures.size() > 0 )
  {
    TEncRCPic* p = m_listRCPictures.front();
    m_listRCPictures.pop_front();
    delete p;
  }
}

#if M0036_RC_IMPROVEMENT
Void TEncRateCtrl::init( Int totalFrames, Int targetBitrate, Int frameRate, Int GOPSize, Int picWidth, Int picHeight, Int LCUWidth, Int LCUHeight, Int keepHierBits, Bool useLCUSeparateModel, GOPEntry  GOPList[MAX_GOP] )
#else
Void TEncRateCtrl::init( Int totalFrames, Int targetBitrate, Int frameRate, Int GOPSize, Int picWidth, Int picHeight, Int LCUWidth, Int LCUHeight, Bool keepHierBits, Bool useLCUSeparateModel, GOPEntry  GOPList[MAX_GOP] )
#endif
{
  destroy();

  Bool isLowdelay = true;
  for ( Int i=0; i<GOPSize-1; i++ )
  {
    if ( GOPList[i].m_POC > GOPList[i+1].m_POC )
    {
      isLowdelay = false;
      break;
    }
  }

  Int numberOfLevel = 1;
#if M0036_RC_IMPROVEMENT
  Int adaptiveBit = 0;
  if ( keepHierBits > 0 )
#else
  if ( keepHierBits )
#endif
  {
    numberOfLevel = Int( log((Double)GOPSize)/log(2.0) + 0.5 ) + 1;
  }
  if ( !isLowdelay && GOPSize == 8 )
  {
    numberOfLevel = Int( log((Double)GOPSize)/log(2.0) + 0.5 ) + 1;
  }
  numberOfLevel++;    // intra picture
  numberOfLevel++;    // non-reference picture


  Int* bitsRatio;
  bitsRatio = new Int[ GOPSize ];
  for ( Int i=0; i<GOPSize; i++ )
  {
    bitsRatio[i] = 10;
    if ( !GOPList[i].m_refPic )
    {
      bitsRatio[i] = 2;
    }
  }

#if M0036_RC_IMPROVEMENT
  if ( keepHierBits > 0 )
#else
  if ( keepHierBits )
#endif
  {
    Double bpp = (Double)( targetBitrate / (Double)( frameRate*picWidth*picHeight ) );
    if ( GOPSize == 4 && isLowdelay )
    {
      if ( bpp > 0.2 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 6;
      }
      else if( bpp > 0.1 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 10;
      }
      else if ( bpp > 0.05 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 12;
      }
      else
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 14;
      }
#if M0036_RC_IMPROVEMENT
      if ( keepHierBits == 2 )
      {
        adaptiveBit = 1;
      }
#endif
    }
    else if ( GOPSize == 8 && !isLowdelay )
    {
      if ( bpp > 0.2 )
      {
        bitsRatio[0] = 15;
        bitsRatio[1] = 5;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else if ( bpp > 0.1 )
      {
        bitsRatio[0] = 20;
        bitsRatio[1] = 6;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else if ( bpp > 0.05 )
      {
        bitsRatio[0] = 25;
        bitsRatio[1] = 7;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else
      {
        bitsRatio[0] = 30;
        bitsRatio[1] = 8;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
#if M0036_RC_IMPROVEMENT
      if ( keepHierBits == 2 )
      {
        adaptiveBit = 2;
      }
#endif
    }
    else
    {
#if M0036_RC_IMPROVEMENT
      printf( "\n hierarchical bit allocation is not support for the specified coding structure currently.\n" );
#else
      printf( "\n hierarchical bit allocation is not support for the specified coding structure currently." );
#endif
    }
  }

  Int* GOPID2Level = new int[ GOPSize ];
  for ( int i=0; i<GOPSize; i++ )
  {
    GOPID2Level[i] = 1;
    if ( !GOPList[i].m_refPic )
    {
      GOPID2Level[i] = 2;
    }
  }
#if M0036_RC_IMPROVEMENT
  if ( keepHierBits > 0 )
#else
  if ( keepHierBits )
#endif
  {
    if ( GOPSize == 4 && isLowdelay )
    {
      GOPID2Level[0] = 3;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 1;
    }
    else if ( GOPSize == 8 && !isLowdelay )
    {
      GOPID2Level[0] = 1;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 4;
      GOPID2Level[4] = 4;
      GOPID2Level[5] = 3;
      GOPID2Level[6] = 4;
      GOPID2Level[7] = 4;
    }
  }

  if ( !isLowdelay && GOPSize == 8 )
  {
    GOPID2Level[0] = 1;
    GOPID2Level[1] = 2;
    GOPID2Level[2] = 3;
    GOPID2Level[3] = 4;
    GOPID2Level[4] = 4;
    GOPID2Level[5] = 3;
    GOPID2Level[6] = 4;
    GOPID2Level[7] = 4;
  }

  m_encRCSeq = new TEncRCSeq;
#if M0036_RC_IMPROVEMENT
  m_encRCSeq->create( totalFrames, targetBitrate, frameRate, GOPSize, picWidth, picHeight, LCUWidth, LCUHeight, numberOfLevel, useLCUSeparateModel, adaptiveBit );
#else
  m_encRCSeq->create( totalFrames, targetBitrate, frameRate, GOPSize, picWidth, picHeight, LCUWidth, LCUHeight, numberOfLevel, useLCUSeparateModel );
#endif
  m_encRCSeq->initBitsRatio( bitsRatio );
  m_encRCSeq->initGOPID2Level( GOPID2Level );
  m_encRCSeq->initPicPara();
  if ( useLCUSeparateModel )
  {
    m_encRCSeq->initLCUPara();
  }

  delete[] bitsRatio;
  delete[] GOPID2Level;
}

Void TEncRateCtrl::initRCPic( Int frameLevel )
{
  m_encRCPic = new TEncRCPic;
  m_encRCPic->create( m_encRCSeq, m_encRCGOP, frameLevel, m_listRCPictures );
}

Void TEncRateCtrl::initRCGOP( Int numberOfPictures )
{
  m_encRCGOP = new TEncRCGOP;
  m_encRCGOP->create( m_encRCSeq, numberOfPictures );
}

Void TEncRateCtrl::destroyRCGOP()
{
  delete m_encRCGOP;
  m_encRCGOP = NULL;
}

#else

#define ADJUSTMENT_FACTOR       0.60
#define HIGH_QSTEP_THRESHOLD    9.5238
#define HIGH_QSTEP_ALPHA        4.9371
#define HIGH_QSTEP_BETA         0.0922
#define LOW_QSTEP_ALPHA         16.7429
#define LOW_QSTEP_BETA          -1.1494

#define MAD_PRED_Y1             1.0
#define MAD_PRED_Y2             0.0

enum MAD_HISOTRY {
  MAD_PPPrevious = 0,
  MAD_PPrevious  = 1,
  MAD_Previous   = 2
};

Void    MADLinearModel::initMADLinearModel()
{
  m_activeOn = false;
  m_paramY1  = 1.0;
  m_paramY2  = 0.0;
  m_costMADs[0] = m_costMADs[1] = m_costMADs[2] = 0.0;
}

Double  MADLinearModel::getMAD()
{
  Double costPredMAD = m_paramY1 * m_costMADs[MAD_Previous] + m_paramY2;

  if(costPredMAD < 0)
  {
    costPredMAD = m_costMADs[MAD_Previous];
    m_paramY1   = MAD_PRED_Y1;
    m_paramY2   = MAD_PRED_Y2;
  } 
  return costPredMAD;
}

Void    MADLinearModel::updateMADLiearModel()
{
  Double dNewY1 = ((m_costMADs[MAD_Previous] - m_costMADs[MAD_PPrevious]) / (m_costMADs[MAD_PPrevious] - m_costMADs[MAD_PPPrevious]));
  Double dNewY2 =  (m_costMADs[MAD_Previous] - (dNewY1*m_costMADs[MAD_PPrevious]));
  
  m_paramY1 = 0.70+0.20*m_paramY1+ 0.10*dNewY1;
  m_paramY2 =      0.20*m_paramY2+ 0.10*dNewY2;
}

Void    MADLinearModel::updateMADHistory(Double dMAD)
{
  m_costMADs[MAD_PPPrevious] = m_costMADs[MAD_PPrevious];
  m_costMADs[MAD_PPrevious ] = m_costMADs[MAD_Previous ];
  m_costMADs[MAD_Previous  ] = dMAD;
  m_activeOn = (m_costMADs[MAD_Previous  ] && m_costMADs[MAD_PPrevious ] && m_costMADs[MAD_PPPrevious]);
}


Void    PixelBaseURQQuadraticModel::initPixelBaseQuadraticModel()
{
  m_paramHighX1 = HIGH_QSTEP_ALPHA;
  m_paramHighX2 = HIGH_QSTEP_BETA;
  m_paramLowX1  = LOW_QSTEP_ALPHA;
  m_paramLowX2  = LOW_QSTEP_BETA;
}

Int     PixelBaseURQQuadraticModel::getQP(Int qp, Int targetBits, Int numberOfPixels, Double costPredMAD)
{
  Double qStep;
  Double bppPerMAD = (Double)(targetBits/(numberOfPixels*costPredMAD));
  
  if(xConvertQP2QStep(qp) >= HIGH_QSTEP_THRESHOLD)
  {
    qStep = 1/( sqrt((bppPerMAD/m_paramHighX1)+((m_paramHighX2*m_paramHighX2)/(4*m_paramHighX1*m_paramHighX1))) - (m_paramHighX2/(2*m_paramHighX1)));
  }
  else
  {
    qStep = 1/( sqrt((bppPerMAD/m_paramLowX1)+((m_paramLowX2*m_paramLowX2)/(4*m_paramLowX1*m_paramLowX1))) - (m_paramLowX2/(2*m_paramLowX1)));
  }
  
  return xConvertQStep2QP(qStep);
}

Void    PixelBaseURQQuadraticModel::updatePixelBasedURQQuadraticModel (Int qp, Int bits, Int numberOfPixels, Double costMAD)
{
  Double qStep     = xConvertQP2QStep(qp);
  Double invqStep = (1/qStep);
  Double paramNewX1, paramNewX2;
  
  if(qStep >= HIGH_QSTEP_THRESHOLD)
  {
    paramNewX2    = (((bits/(numberOfPixels*costMAD))-(23.3772*invqStep*invqStep))/((1-200*invqStep)*invqStep));
    paramNewX1    = (23.3772-200*paramNewX2);
    m_paramHighX1 = 0.70*HIGH_QSTEP_ALPHA + 0.20 * m_paramHighX1 + 0.10 * paramNewX1;
    m_paramHighX2 = 0.70*HIGH_QSTEP_BETA  + 0.20 * m_paramHighX2 + 0.10 * paramNewX2;
  }
  else
  {
    paramNewX2   = (((bits/(numberOfPixels*costMAD))-(5.8091*invqStep*invqStep))/((1-9.5455*invqStep)*invqStep));
    paramNewX1   = (5.8091-9.5455*paramNewX2);
    m_paramLowX1 = 0.90*LOW_QSTEP_ALPHA + 0.09 * m_paramLowX1 + 0.01 * paramNewX1;
    m_paramLowX2 = 0.90*LOW_QSTEP_BETA  + 0.09 * m_paramLowX2 + 0.01 * paramNewX2;
  }
}

Bool    PixelBaseURQQuadraticModel::checkUpdateAvailable(Int qpReference )
{ 
  Double qStep = xConvertQP2QStep(qpReference);

  if (qStep > xConvertQP2QStep(MAX_QP) 
    ||qStep < xConvertQP2QStep(MIN_QP) )
  {
    return false;
  }

  return true;
}

Double  PixelBaseURQQuadraticModel::xConvertQP2QStep(Int qp )
{
  Int i;
  Double qStep;
  static const Double mapQP2QSTEP[6] = { 0.625, 0.703, 0.797, 0.891, 1.000, 1.125 };

  qStep = mapQP2QSTEP[qp % 6];
  for( i=0; i<(qp/6); i++)
  {
    qStep *= 2;
  }

  return qStep;
}

Int     PixelBaseURQQuadraticModel::xConvertQStep2QP(Double qStep )
{
  Int per = 0, rem = 0;

  if( qStep < xConvertQP2QStep(MIN_QP))
  {
    return MIN_QP;
  }
  else if (qStep > xConvertQP2QStep(MAX_QP) )
  {
    return MAX_QP;
  }

  while( qStep > xConvertQP2QStep(5) )
  {
    qStep /= 2.0;
    per++;
  }

  if (qStep <= 0.625)
  {
    rem = 0;
  }
  else if (qStep <= 0.703)
  {
    rem = 1;
  }
  else if (qStep <= 0.797)
  {
    rem = 2;
  }
  else if (qStep <= 0.891)
  {
    rem = 3;
  }
  else if (qStep <= 1.000)
  {
    rem = 4;
  }
  else
  {
    rem = 5;
  }
  return (per * 6 + rem);
}


Void  TEncRateCtrl::create(Int sizeIntraPeriod, Int sizeGOP, Int frameRate, Int targetKbps, Int qp, Int numLCUInBasicUnit, Int sourceWidth, Int sourceHeight, Int maxCUWidth, Int maxCUHeight)
{
  Int leftInHeight, leftInWidth;

  m_sourceWidthInLCU         = (sourceWidth  / maxCUWidth  ) + (( sourceWidth  %  maxCUWidth ) ? 1 : 0);
  m_sourceHeightInLCU        = (sourceHeight / maxCUHeight) + (( sourceHeight %  maxCUHeight) ? 1 : 0);  
  m_isLowdelay               = (sizeIntraPeriod == -1) ? true : false;
  m_prevBitrate              = ( targetKbps << 10 );  // in units of 1,024 bps
  m_currBitrate              = ( targetKbps << 10 );
  m_frameRate                = frameRate;
  m_refFrameNum              = m_isLowdelay ? (sizeGOP) : (sizeGOP>>1);
  m_nonRefFrameNum           = sizeGOP-m_refFrameNum;
  m_sizeGOP                  = sizeGOP;
  m_numOfPixels              = ((sourceWidth*sourceHeight*3)>>1);
  m_indexGOP                 = 0;
  m_indexFrame               = 0;
  m_indexLCU                 = 0;
  m_indexUnit                = 0;
  m_indexRefFrame            = 0;
  m_indexNonRefFrame         = 0;
  m_occupancyVB              = 0;
  m_initialOVB               = 0;
  m_targetBufLevel           = 0;
  m_initialTBL               = 0;
  m_occupancyVBInFrame       = 0;
  m_remainingBitsInGOP       = (m_currBitrate*sizeGOP/m_frameRate);
  m_remainingBitsInFrame     = 0;
  m_numUnitInFrame           = m_sourceWidthInLCU*m_sourceHeightInLCU;
  m_cMADLinearModel.        initMADLinearModel();
  m_cPixelURQQuadraticModel.initPixelBaseQuadraticModel();

  m_costRefAvgWeighting      = 0.0;
  m_costNonRefAvgWeighting   = 0.0;
  m_costAvgbpp               = 0.0;  
  m_activeUnitLevelOn        = false;

  m_pcFrameData              = new FrameData   [sizeGOP+1];         initFrameData(qp);
  m_pcLCUData                = new LCUData     [m_numUnitInFrame];  initUnitData (qp);

  for(Int i = 0, addressUnit = 0; i < m_sourceHeightInLCU*maxCUHeight; i += maxCUHeight)  
  {
    leftInHeight = sourceHeight - i;
    leftInHeight = min(leftInHeight, maxCUHeight);
    for(Int j = 0; j < m_sourceWidthInLCU*maxCUWidth; j += maxCUWidth, addressUnit++)
    {
      leftInWidth = sourceWidth - j;
      leftInWidth = min(leftInWidth, maxCUWidth);
      m_pcLCUData[addressUnit].m_widthInPixel = leftInWidth;
      m_pcLCUData[addressUnit].m_heightInPixel= leftInHeight;
      m_pcLCUData[addressUnit].m_pixels       = ((leftInHeight*leftInWidth*3)>>1);
    }
  }
}

Void  TEncRateCtrl::destroy()
{
  if(m_pcFrameData)
  {
    delete [] m_pcFrameData;
    m_pcFrameData = NULL;
  }
  if(m_pcLCUData)
  {
    delete [] m_pcLCUData;
    m_pcLCUData = NULL;
  }
}

Void  TEncRateCtrl::initFrameData   (Int qp)
{
  for(Int i = 0 ; i <= m_sizeGOP; i++)
  {
    m_pcFrameData[i].m_isReferenced = false;
    m_pcFrameData[i].m_costMAD      = 0.0;
    m_pcFrameData[i].m_bits         = 0;
    m_pcFrameData[i].m_qp           = qp;
  }
}

Void  TEncRateCtrl::initUnitData    (Int qp)
{
  for(Int i = 1 ; i < m_numUnitInFrame; i++)
  {
    m_pcLCUData[i].m_qp            = qp;
    m_pcLCUData[i].m_bits          = 0;
    m_pcLCUData[i].m_pixels        = 0;
    m_pcLCUData[i].m_widthInPixel  = 0;
    m_pcLCUData[i].m_heightInPixel = 0;
    m_pcLCUData[i].m_costMAD       = 0.0;
  }
}

Int  TEncRateCtrl::getFrameQP(Bool isReferenced, Int POC)
{
  Int numofReferenced = 0;
  Int finalQP = 0;
  FrameData* pcFrameData;

  m_indexPOCInGOP = (POC%m_sizeGOP) == 0 ? m_sizeGOP : (POC%m_sizeGOP);
  pcFrameData     = &m_pcFrameData[m_indexPOCInGOP];
    
  if(m_indexFrame != 0)
  {
    if(isReferenced)
    {
      Double gamma = m_isLowdelay ? 0.5 : 0.25;
      Double beta  = m_isLowdelay ? 0.9 : 0.6;
      Int    numRemainingRefFrames  = m_refFrameNum    - m_indexRefFrame;
      Int    numRemainingNRefFrames = m_nonRefFrameNum - m_indexNonRefFrame;
      
      Double targetBitsOccupancy  = (m_currBitrate/(Double)m_frameRate) + gamma*(m_targetBufLevel-m_occupancyVB - (m_initialOVB/(Double)m_frameRate));
      Double targetBitsLeftBudget = ((m_costRefAvgWeighting*m_remainingBitsInGOP)/((m_costRefAvgWeighting*numRemainingRefFrames)+(m_costNonRefAvgWeighting*numRemainingNRefFrames)));

      m_targetBits = (Int)(beta * targetBitsLeftBudget + (1-beta) * targetBitsOccupancy);
  
      if(m_targetBits <= 0 || m_remainingBitsInGOP <= 0)
      {
        finalQP = m_pcFrameData[m_indexPrevPOCInGOP].m_qp + 2;
      }
      else
      {
        Double costPredMAD   = m_cMADLinearModel.getMAD();
        Int    qpLowerBound = m_pcFrameData[m_indexPrevPOCInGOP].m_qp-2;
        Int    qpUpperBound = m_pcFrameData[m_indexPrevPOCInGOP].m_qp+2;
        finalQP = m_cPixelURQQuadraticModel.getQP(m_pcFrameData[m_indexPrevPOCInGOP].m_qp, m_targetBits, m_numOfPixels, costPredMAD);
        finalQP = max(qpLowerBound, min(qpUpperBound, finalQP));
        m_activeUnitLevelOn    = true;
        m_remainingBitsInFrame = m_targetBits;
        m_costAvgbpp           = (m_targetBits/(Double)m_numOfPixels);
      }

      m_indexRefFrame++;
    }
    else
    {
      Int bwdQP = m_pcFrameData[m_indexPOCInGOP-1].m_qp;
      Int fwdQP = m_pcFrameData[m_indexPOCInGOP+1].m_qp;
       
      if( (fwdQP+bwdQP) == m_pcFrameData[m_indexPOCInGOP-1].m_qp
        ||(fwdQP+bwdQP) == m_pcFrameData[m_indexPOCInGOP+1].m_qp)
      {
        finalQP = (fwdQP+bwdQP);
      }
      else if(bwdQP != fwdQP)
      {
        finalQP = ((bwdQP+fwdQP+2)>>1);
      }
      else
      {
        finalQP = bwdQP+2;
      }
      m_indexNonRefFrame++;
    }
  }
  else
  {
    Int lastQPminus2 = m_pcFrameData[0].m_qp - 2;
    Int lastQPplus2  = m_pcFrameData[0].m_qp + 2;

    for(Int idx = 1; idx <= m_sizeGOP; idx++)
    {
      if(m_pcFrameData[idx].m_isReferenced)
      {
        finalQP += m_pcFrameData[idx].m_qp;
        numofReferenced++;
      }
    }
    
    finalQP = (numofReferenced == 0) ? m_pcFrameData[0].m_qp : ((finalQP + (1<<(numofReferenced>>1)))/numofReferenced);
    finalQP = max( lastQPminus2, min( lastQPplus2, finalQP));

    Double costAvgFrameBits = m_remainingBitsInGOP/(Double)m_sizeGOP;
    Int    bufLevel  = m_occupancyVB + m_initialOVB;

    if(abs(bufLevel) > costAvgFrameBits)
    {
      if(bufLevel < 0)
      {
        finalQP -= 2;
      }
      else
      {
        finalQP += 2;
      }
    }
    m_indexRefFrame++;
  }
  finalQP = max(MIN_QP, min(MAX_QP, finalQP));

  for(Int indexLCU = 0 ; indexLCU < m_numUnitInFrame; indexLCU++)
  {
    m_pcLCUData[indexLCU].m_qp = finalQP;
  }

  pcFrameData->m_isReferenced = isReferenced;
  pcFrameData->m_qp           = finalQP;

  return finalQP;
}

Bool  TEncRateCtrl::calculateUnitQP ()
{
  if(!m_activeUnitLevelOn || m_indexLCU == 0)
  {
    return false;
  }
  Int upperQPBound, lowerQPBound, finalQP;
  Int    colQP        = m_pcLCUData[m_indexLCU].m_qp;
  Double colMAD       = m_pcLCUData[m_indexLCU].m_costMAD;
  Double budgetInUnit = m_pcLCUData[m_indexLCU].m_pixels*m_costAvgbpp;


  Int targetBitsOccupancy = (Int)(budgetInUnit - (m_occupancyVBInFrame/(m_numUnitInFrame-m_indexUnit)));
  Int targetBitsLeftBudget= (Int)((m_remainingBitsInFrame*m_pcLCUData[m_indexLCU].m_pixels)/(Double)(m_numOfPixels-m_codedPixels));
  Int targetBits = (targetBitsLeftBudget>>1) + (targetBitsOccupancy>>1);
  

  if( m_indexLCU >= m_sourceWidthInLCU)
  {
    upperQPBound = ( (m_pcLCUData[m_indexLCU-1].m_qp + m_pcLCUData[m_indexLCU - m_sourceWidthInLCU].m_qp)>>1) + MAX_DELTA_QP;
    lowerQPBound = ( (m_pcLCUData[m_indexLCU-1].m_qp + m_pcLCUData[m_indexLCU - m_sourceWidthInLCU].m_qp)>>1) - MAX_DELTA_QP;
  }
  else
  {
    upperQPBound = m_pcLCUData[m_indexLCU-1].m_qp + MAX_DELTA_QP;
    lowerQPBound = m_pcLCUData[m_indexLCU-1].m_qp - MAX_DELTA_QP;
  }

  if(targetBits < 0)
  {
    finalQP = m_pcLCUData[m_indexLCU-1].m_qp + 1;
  }
  else
  {
    finalQP = m_cPixelURQQuadraticModel.getQP(colQP, targetBits, m_pcLCUData[m_indexLCU].m_pixels, colMAD);
  }
  
  finalQP = max(lowerQPBound, min(upperQPBound, finalQP));
  m_pcLCUData[m_indexLCU].m_qp = max(MIN_QP, min(MAX_QP, finalQP));
  
  return true;
}

Void  TEncRateCtrl::updateRCGOPStatus()
{
  m_remainingBitsInGOP = ((m_currBitrate/m_frameRate)*m_sizeGOP) - m_occupancyVB;
  
  FrameData cFrameData = m_pcFrameData[m_sizeGOP];
  initFrameData();

  m_pcFrameData[0]   = cFrameData;
  m_indexGOP++;
  m_indexFrame       = 0;
  m_indexRefFrame    = 0;
  m_indexNonRefFrame = 0;
}

Void  TEncRateCtrl::updataRCFrameStatus(Int frameBits, SliceType eSliceType)
{
  FrameData* pcFrameData = &m_pcFrameData[m_indexPOCInGOP];
  Int occupancyBits;
  Double adjustmentBits;

  m_remainingBitsInGOP = m_remainingBitsInGOP + ( ((m_currBitrate-m_prevBitrate)/m_frameRate)*(m_sizeGOP-m_indexFrame) ) - frameBits;
  occupancyBits        = (Int)((Double)frameBits - (m_currBitrate/(Double)m_frameRate));
  
  if( (occupancyBits < 0) && (m_initialOVB > 0) )
  {
    adjustmentBits = xAdjustmentBits(occupancyBits, m_initialOVB );

    if(m_initialOVB < 0)
    {
      adjustmentBits = m_initialOVB;
      occupancyBits += (Int)adjustmentBits;
      m_initialOVB   =  0;
    }
  }
  else if( (occupancyBits > 0) && (m_initialOVB < 0) )
  {
    adjustmentBits = xAdjustmentBits(m_initialOVB, occupancyBits );
    
    if(occupancyBits < 0)
    {
      adjustmentBits = occupancyBits;
      m_initialOVB  += (Int)adjustmentBits;
      occupancyBits  =  0;
    }
  }

  if(m_indexGOP == 0)
  {
    m_initialOVB = occupancyBits;
  }
  else
  {
    m_occupancyVB= m_occupancyVB + occupancyBits;
  }

  if(pcFrameData->m_isReferenced)
  {
    m_costRefAvgWeighting  = ((pcFrameData->m_bits*pcFrameData->m_qp)/8.0) + (7.0*(m_costRefAvgWeighting)/8.0);

    if(m_indexFrame == 0)
    {
      m_initialTBL = m_targetBufLevel  = (frameBits - (m_currBitrate/m_frameRate));
    }
    else
    {
      Int distance = (m_costNonRefAvgWeighting == 0) ? 0 : 1;
      m_targetBufLevel =  m_targetBufLevel 
                            - (m_initialTBL/(m_refFrameNum-1)) 
                            + (Int)((m_costRefAvgWeighting*(distance+1)*m_currBitrate)/(m_frameRate*(m_costRefAvgWeighting+(m_costNonRefAvgWeighting*distance)))) 
                            - (m_currBitrate/m_frameRate);
    }

    if(m_cMADLinearModel.IsUpdateAvailable())
    {
      m_cMADLinearModel.updateMADLiearModel();
    }

    if(eSliceType != I_SLICE &&
       m_cPixelURQQuadraticModel.checkUpdateAvailable(pcFrameData->m_qp))
    {
      m_cPixelURQQuadraticModel.updatePixelBasedURQQuadraticModel(pcFrameData->m_qp, pcFrameData->m_bits, m_numOfPixels, pcFrameData->m_costMAD);
    }
  }
  else
  {
    m_costNonRefAvgWeighting = ((pcFrameData->m_bits*pcFrameData->m_qp)/8.0) + (7.0*(m_costNonRefAvgWeighting)/8.0);
  }

  m_indexFrame++;
  m_indexLCU             = 0;
  m_indexUnit            = 0;
  m_occupancyVBInFrame   = 0;
  m_remainingBitsInFrame = 0;
  m_codedPixels          = 0;
  m_activeUnitLevelOn    = false;
  m_costAvgbpp           = 0.0;
}
Void  TEncRateCtrl::updataRCUnitStatus ()
{
  if(!m_activeUnitLevelOn || m_indexLCU == 0)
  {
    return;
  }

  m_codedPixels  += m_pcLCUData[m_indexLCU-1].m_pixels;
  m_remainingBitsInFrame = m_remainingBitsInFrame - m_pcLCUData[m_indexLCU-1].m_bits;
  m_occupancyVBInFrame   = (Int)(m_occupancyVBInFrame + m_pcLCUData[m_indexLCU-1].m_bits - m_pcLCUData[m_indexLCU-1].m_pixels*m_costAvgbpp);

  if( m_cPixelURQQuadraticModel.checkUpdateAvailable(m_pcLCUData[m_indexLCU-1].m_qp) )
  {
    m_cPixelURQQuadraticModel.updatePixelBasedURQQuadraticModel(m_pcLCUData[m_indexLCU-1].m_qp, m_pcLCUData[m_indexLCU-1].m_bits, m_pcLCUData[m_indexLCU-1].m_pixels, m_pcLCUData[m_indexLCU-1].m_costMAD);
  }

  m_indexUnit++;
}

Void  TEncRateCtrl::updateFrameData(UInt64 actualFrameBits)
{
  Double costMAD = 0.0;
  
  for(Int i = 0; i < m_numUnitInFrame; i++)
  {
    costMAD    += m_pcLCUData[i].m_costMAD;
  }
  
  m_pcFrameData[m_indexPOCInGOP].m_costMAD = (costMAD/(Double)m_numUnitInFrame);
  m_pcFrameData[m_indexPOCInGOP].m_bits    = (Int)actualFrameBits;
  
  if(m_pcFrameData[m_indexPOCInGOP].m_isReferenced)
  {
    m_indexPrevPOCInGOP = m_indexPOCInGOP;
    m_cMADLinearModel.updateMADHistory(m_pcFrameData[m_indexPOCInGOP].m_costMAD);
  }
}

Void  TEncRateCtrl::updateLCUData(TComDataCU* pcCU, UInt64 actualLCUBits, Int qp)
{
  Int     x, y;
  Double  costMAD = 0.0;

  Pel*  pOrg   = pcCU->getPic()->getPicYuvOrg()->getLumaAddr(pcCU->getAddr(), 0);
  Pel*  pRec   = pcCU->getPic()->getPicYuvRec()->getLumaAddr(pcCU->getAddr(), 0);
  Int   stride = pcCU->getPic()->getStride();

  Int   width  = m_pcLCUData[m_indexLCU].m_widthInPixel;
  Int   height = m_pcLCUData[m_indexLCU].m_heightInPixel;

  for( y = 0; y < height; y++ )
  {
    for( x = 0; x < width; x++ )
    {
      costMAD += abs( pOrg[x] - pRec[x] );
    }
    pOrg += stride;
    pRec += stride;
  }
  m_pcLCUData[m_indexLCU  ].m_qp      = qp;
  m_pcLCUData[m_indexLCU  ].m_costMAD = (costMAD /(Double)(width*height));
  m_pcLCUData[m_indexLCU++].m_bits    = (Int)actualLCUBits;
}

Double TEncRateCtrl::xAdjustmentBits(Int& reductionBits, Int& compensationBits)
{
  Double adjustment  = ADJUSTMENT_FACTOR*reductionBits;
  reductionBits     -= (Int)adjustment;
  compensationBits  += (Int)adjustment;

  return adjustment;
}

#endif

