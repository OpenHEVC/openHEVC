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

/** \file     TEncRateCtrl.cpp
    \brief    Rate control manager class
*/
#include "TEncRateCtrl.h"
#include "../TLibCommon/TComPic.h"

#include <cmath>

using namespace std;

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
#if J0260
    qStep = 1/( sqrt((bppPerMAD/m_paramHighX1)+((m_paramHighX2*m_paramHighX2)/(4*m_paramHighX1*m_paramHighX1))) - (m_paramHighX2/(2*m_paramHighX1)));
#else
    qStep = 1/( sqrt((bppPerMAD/m_paramHighX1)+((m_paramHighX2*m_paramHighX2)/(4*m_paramHighX1*m_paramHighX1*m_paramHighX1))) - (m_paramHighX2/(2*m_paramHighX1)));
#endif
  }
  else
  {
#if J0260
    qStep = 1/( sqrt((bppPerMAD/m_paramLowX1)+((m_paramLowX2*m_paramLowX2)/(4*m_paramLowX1*m_paramLowX1))) - (m_paramLowX2/(2*m_paramLowX1)));
#else
    qStep = 1/( sqrt((bppPerMAD/m_paramLowX1)+((m_paramLowX2*m_paramLowX2)/(4*m_paramLowX1*m_paramLowX1*m_paramLowX1))) - (m_paramLowX2/(2*m_paramLowX1)));
#endif
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
  m_prevBitrate              = targetKbps*1000;
  m_currBitrate              = targetKbps*1000;
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
  double  costMAD = 0.0;

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

