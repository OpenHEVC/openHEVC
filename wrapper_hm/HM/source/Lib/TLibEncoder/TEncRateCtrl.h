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

/** \file     TEncRateCtrl.h
    \brief    Rate control manager class
*/

#ifndef _HM_TENCRATECTRL_H_
#define _HM_TENCRATECTRL_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "../TLibCommon/CommonDef.h"
#include "../TLibCommon/TComDataCU.h"

#include <vector>
#include <algorithm>

using namespace std;

//! \ingroup TLibEncoder
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================
#define MAX_DELTA_QP    2
#define MAX_CUDQP_DEPTH 0 

typedef struct FrameData
{
  Bool       m_isReferenced;
  Int        m_qp;
  Int        m_bits;
  Double     m_costMAD;
}FrameData;

typedef struct LCUData
{
  Int     m_qp;                ///<  coded QP
  Int     m_bits;              ///<  actually generated bits
  Int     m_pixels;            ///<  number of pixels for a unit
  Int     m_widthInPixel;      ///<  number of pixels for width
  Int     m_heightInPixel;     ///<  number of pixels for height
  Double  m_costMAD;           ///<  texture complexity for a unit
}LCUData;

class MADLinearModel
{
private:
  Bool   m_activeOn;
  Double m_paramY1;
  Double m_paramY2;
  Double m_costMADs[3];

public:
  MADLinearModel ()   {};
  ~MADLinearModel()   {};
  
  Void    initMADLinearModel      ();
  Double  getMAD                  ();
  Void    updateMADLiearModel     ();
  Void    updateMADHistory        (Double costMAD);
  Bool    IsUpdateAvailable       ()              { return m_activeOn; }
};

class PixelBaseURQQuadraticModel
{
private:
  Double m_paramHighX1;
  Double m_paramHighX2;
  Double m_paramLowX1;
  Double m_paramLowX2;
public:
  PixelBaseURQQuadraticModel () {};
  ~PixelBaseURQQuadraticModel() {};

  Void    initPixelBaseQuadraticModel       ();
  Int     getQP                             (Int qp, Int targetBits, Int numberOfPixels, Double costPredMAD);
  Void    updatePixelBasedURQQuadraticModel (Int qp, Int bits, Int numberOfPixels, Double costMAD);
  Bool    checkUpdateAvailable              (Int qpReference );
  Double  xConvertQP2QStep                  (Int qp );
  Int     xConvertQStep2QP                  (Double qStep );
};

class TEncRateCtrl
{
private:
  Bool            m_isLowdelay;
  Int             m_prevBitrate;
  Int             m_currBitrate;
  Int             m_frameRate;
  Int             m_refFrameNum;
  Int             m_nonRefFrameNum;
  Int             m_numOfPixels;
  Int             m_sourceWidthInLCU;
  Int             m_sourceHeightInLCU;      
  Int             m_sizeGOP;
  Int             m_indexGOP;
  Int             m_indexFrame;
  Int             m_indexLCU;
  Int             m_indexUnit;
  Int             m_indexRefFrame;
  Int             m_indexNonRefFrame;
  Int             m_indexPOCInGOP;
  Int             m_indexPrevPOCInGOP;
  Int             m_occupancyVB;
  Int             m_initialOVB;
  Int             m_targetBufLevel;
  Int             m_initialTBL;
  Int             m_remainingBitsInGOP;
  Int             m_remainingBitsInFrame;
  Int             m_occupancyVBInFrame;
  Int             m_targetBits;
  Int             m_numUnitInFrame;
  Int             m_codedPixels;
  Bool            m_activeUnitLevelOn;
  Double          m_costNonRefAvgWeighting;
  Double          m_costRefAvgWeighting;
  Double          m_costAvgbpp;         
  
  FrameData*      m_pcFrameData;
  LCUData*        m_pcLCUData;

  MADLinearModel              m_cMADLinearModel;
  PixelBaseURQQuadraticModel  m_cPixelURQQuadraticModel;
  
public:
  TEncRateCtrl         () {};
  virtual ~TEncRateCtrl() {};

  Void          create                (Int sizeIntraPeriod, Int sizeGOP, Int frameRate, Int targetKbps, Int qp, Int numLCUInBasicUnit, Int sourceWidth, Int sourceHeight, Int maxCUWidth, Int maxCUHeight);
  Void          destroy               ();

  Void          initFrameData         (Int qp = 0);
  Void          initUnitData          (Int qp = 0);
  Int           getFrameQP            (Bool isReferenced, Int POC);
  Bool          calculateUnitQP       ();
  Int           getUnitQP             ()                                          { return m_pcLCUData[m_indexLCU].m_qp;  }
  Void          updateRCGOPStatus     ();
  Void          updataRCFrameStatus   (Int frameBits, SliceType eSliceType);
  Void          updataRCUnitStatus    ();
  Void          updateLCUData         (TComDataCU* pcCU, UInt64 actualLCUBits, Int qp);
  Void          updateFrameData       (UInt64 actualFrameBits);
  Double        xAdjustmentBits       (Int& reductionBits, Int& compensationBits);
  Int           getGOPId              ()                                          { return m_indexFrame; }
};
#endif


