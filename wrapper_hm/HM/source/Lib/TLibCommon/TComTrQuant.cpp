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

/** \file     TComTrQuant.cpp
    \brief    transform and quantization class
*/

#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "TComTrQuant.h"
#include "TComPic.h"
#include "ContextTables.h"

typedef struct
{
  Int    iNNZbeforePos0;
  Double d64CodedLevelandDist; // distortion and level cost only
  Double d64UncodedDist;    // all zero coded block distortion
  Double d64SigCost;
  Double d64SigCost_0;
} coeffGroupRDStats;

//! \ingroup TLibCommon
//! \{

// ====================================================================================================================
// Constants
// ====================================================================================================================

#define RDOQ_CHROMA                 1           ///< use of RDOQ in chroma

// ====================================================================================================================
// Tables
// ====================================================================================================================

// RDOQ parameter

// ====================================================================================================================
// Qp class member functions
// ====================================================================================================================

QpParam::QpParam()
{
}

// ====================================================================================================================
// TComTrQuant class member functions
// ====================================================================================================================

TComTrQuant::TComTrQuant()
{
  m_cQP.clear();
  
  // allocate temporary buffers
  m_plTempCoeff  = new Int[ MAX_CU_SIZE*MAX_CU_SIZE ];
  
  // allocate bit estimation class  (for RDOQ)
  m_pcEstBitsSbac = new estBitsSbacStruct;
  initScalingList();
}

TComTrQuant::~TComTrQuant()
{
  // delete temporary buffers
  if ( m_plTempCoeff )
  {
    delete [] m_plTempCoeff;
    m_plTempCoeff = NULL;
  }
  
  // delete bit estimation class
  if ( m_pcEstBitsSbac )
  {
    delete m_pcEstBitsSbac;
  }
  destroyScalingList();
}

#if ADAPTIVE_QP_SELECTION
Void TComTrQuant::storeSliceQpNext(TComSlice* pcSlice)
{
  Int qpBase = pcSlice->getSliceQpBase();
  Int sliceQpused = pcSlice->getSliceQp();
  Int sliceQpnext;
  Double alpha = qpBase < 17 ? 0.5 : 1;
  
  Int cnt=0;
  for(int u=1; u<=LEVEL_RANGE; u++)
  { 
    cnt += m_sliceNsamples[u] ;
  }

  if( !m_bUseRDOQ )
  {
    sliceQpused = qpBase;
    alpha = 0.5;
  }

  if( cnt > 120 )
  {
    Double sum = 0;
    Int k = 0;
    for(Int u=1; u<LEVEL_RANGE; u++)
    {
      sum += u*m_sliceSumC[u];
      k += u*u*m_sliceNsamples[u];
    }

    Int v;
    Double q[MAX_QP+1] ;
    for(v=0; v<=MAX_QP; v++)
    {
      q[v] = (Double)(g_invQuantScales[v%6] * (1<<(v/6)))/64 ;
    }

    Double qnext = sum/k * q[sliceQpused] / (1<<ARL_C_PRECISION);

    for(v=0; v<MAX_QP; v++)
    {
      if(qnext < alpha * q[v] + (1 - alpha) * q[v+1] )
      {
        break;
      }
    }
    sliceQpnext = Clip3(sliceQpused - 3, sliceQpused + 3, v);
  }
  else
  {
    sliceQpnext = sliceQpused;
  }

  m_qpDelta[qpBase] = sliceQpnext - qpBase; 
}

Void TComTrQuant::initSliceQpDelta()
{
  for(Int qp=0; qp<=MAX_QP; qp++)
  {
    m_qpDelta[qp] = qp < 17 ? 0 : 1;
  }
}

Void TComTrQuant::clearSliceARLCnt()
{ 
  memset(m_sliceSumC, 0, sizeof(Double)*(LEVEL_RANGE+1));
  memset(m_sliceNsamples, 0, sizeof(Int)*(LEVEL_RANGE+1));
}
#endif


/** Set qP for Quantization.
 * \param qpy QPy
 * \param bLowpass
 * \param eSliceType
 * \param eTxtType
 * \param qpBdOffset
 * \param chromaQPOffset
 *
 * return void  
 */
Void TComTrQuant::setQPforQuant( Int qpy, TextType eTxtType, Int qpBdOffset, Int chromaQPOffset)
{
  Int qpScaled;

  if(eTxtType == TEXT_LUMA)
  {
    qpScaled = qpy + qpBdOffset;
  }
  else
  {
#if CHROMA_QP_EXTENSION
    qpScaled = Clip3( -qpBdOffset, 57, qpy + chromaQPOffset );
#else
    qpScaled = Clip3( -qpBdOffset, 51, qpy + chromaQPOffset );
#endif

    if(qpScaled < 0)
    {
      qpScaled = qpScaled + qpBdOffset;
    }
    else
    {
#if CHROMA_QP_EXTENSION
      qpScaled = g_aucChromaScale[ qpScaled ] + qpBdOffset;
#else
      qpScaled = g_aucChromaScale[ Clip3(0, 51, qpScaled) ] + qpBdOffset;
#endif
    }
  }
  m_cQP.setQpParam( qpScaled );
}

#if MATRIX_MULT
/** NxN forward transform (2D) using brute force matrix multiplication (3 nested loops)
 *  \param block pointer to input data (residual)
 *  \param coeff pointer to output data (transform coefficients)
 *  \param uiStride stride of input data
 *  \param uiTrSize transform size (uiTrSize x uiTrSize)
 *  \param uiMode is Intra Prediction mode used in Mode-Dependent DCT/DST only
 */
void xTr(Pel *block, Int *coeff, UInt uiStride, UInt uiTrSize, UInt uiMode)
{
  Int i,j,k,iSum;
  Int tmp[32*32];
  const short *iT;
  UInt uiLog2TrSize = g_aucConvertToBit[ uiTrSize ] + 2;

  if (uiTrSize==4)
  {
    iT  = g_aiT4[0];
  }
  else if (uiTrSize==8)
  {
    iT = g_aiT8[0];
  }
  else if (uiTrSize==16)
  {
    iT = g_aiT16[0];
  }
  else if (uiTrSize==32)
  {
    iT = g_aiT32[0];
  }
  else
  {
    assert(0);
  }

#if FULL_NBIT
  int shift_1st = uiLog2TrSize - 1 + g_uiBitDepth - 8; // log2(N) - 1 + g_uiBitDepth - 8
#else
  int shift_1st = uiLog2TrSize - 1 + g_uiBitIncrement; // log2(N) - 1 + g_uiBitIncrement
#endif

  int add_1st = 1<<(shift_1st-1);
  int shift_2nd = uiLog2TrSize + 6;
  int add_2nd = 1<<(shift_2nd-1);

  /* Horizontal transform */

  if (uiTrSize==4)
  {
    if (uiMode != REG_DCT && g_aucDCTDSTMode_Hor[uiMode])
    {
      iT  =  g_as_DST_MAT_4[0];
    }
  }
  for (i=0; i<uiTrSize; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += iT[i*uiTrSize+k]*block[j*uiStride+k];
      }
      tmp[i*uiTrSize+j] = (iSum + add_1st)>>shift_1st;
    }
  }
  
  /* Vertical transform */
  if (uiTrSize==4)
  {
    if (uiMode != REG_DCT && g_aucDCTDSTMode_Vert[uiMode])
    {
      iT  =  g_as_DST_MAT_4[0];
    }
    else
    {
      iT  = g_aiT4[0];
    }
  }
  for (i=0; i<uiTrSize; i++)
  {                 
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += iT[i*uiTrSize+k]*tmp[j*uiTrSize+k];        
      }
      coeff[i*uiTrSize+j] = (iSum + add_2nd)>>shift_2nd; 
    }
  }
}

/** NxN inverse transform (2D) using brute force matrix multiplication (3 nested loops)
 *  \param coeff pointer to input data (transform coefficients)
 *  \param block pointer to output data (residual)
 *  \param uiStride stride of output data
 *  \param uiTrSize transform size (uiTrSize x uiTrSize)
 *  \param uiMode is Intra Prediction mode used in Mode-Dependent DCT/DST only
 */
void xITr(Int *coeff, Pel *block, UInt uiStride, UInt uiTrSize, UInt uiMode)
{
  int i,j,k,iSum;
  Int tmp[32*32];
  const short *iT;
  
  if (uiTrSize==4)
  {
    iT  = g_aiT4[0];
  }
  else if (uiTrSize==8)
  {
    iT = g_aiT8[0];
  }
  else if (uiTrSize==16)
  {
    iT = g_aiT16[0];
  }
  else if (uiTrSize==32)
  {
    iT = g_aiT32[0];
  }
  else
  {
    assert(0);
  }
  
  int shift_1st = SHIFT_INV_1ST;
  int add_1st = 1<<(shift_1st-1);  
#if FULL_NBIT
  int shift_2nd = SHIFT_INV_2ND - ((short)g_uiBitDepth - 8);
#else
  int shift_2nd = SHIFT_INV_2ND - g_uiBitIncrement;
#endif
  int add_2nd = 1<<(shift_2nd-1);
  if (uiTrSize==4)
  {
    if (uiMode != REG_DCT && g_aucDCTDSTMode_Vert[uiMode] ) // Check for DCT or DST
    {
      iT  =  g_as_DST_MAT_4[0];
    }
  }
  
  /* Horizontal transform */
  for (i=0; i<uiTrSize; i++)
  {    
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {        
        iSum += iT[k*uiTrSize+i]*coeff[k*uiTrSize+j]; 
      }
      tmp[i*uiTrSize+j] = Clip3(-32768, 32767, (iSum + add_1st)>>shift_1st); // Clipping is normative
    }
  }   
  
  if (uiTrSize==4)
  {
    if (uiMode != REG_DCT && g_aucDCTDSTMode_Hor[uiMode] )   // Check for DCT or DST
    {
      iT  =  g_as_DST_MAT_4[0];
    }
    else  
    {
      iT  = g_aiT4[0];
    }
  }
  
  /* Vertical transform */
  for (i=0; i<uiTrSize; i++)
  {   
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {        
        iSum += iT[k*uiTrSize+j]*tmp[i*uiTrSize+k];
      }
      block[i*uiStride+j] = Clip3(-32768, 32767, (iSum + add_2nd)>>shift_2nd); // Clipping is non-normative
    }
  }
}

#else //MATRIX_MULT

/** 4x4 forward transform implemented using partial butterfly structure (1D)
 *  \param src   input data (residual)
 *  \param dst   output data (transform coefficients)
 *  \param shift specifies right shift after 1D transform
 */

void partialButterfly4(short *src,short *dst,int shift, int line)
{
  int j;  
  int E[2],O[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++)
  {    
    /* E and O */
    E[0] = src[0] + src[3];
    O[0] = src[0] - src[3];
    E[1] = src[1] + src[2];
    O[1] = src[1] - src[2];

    dst[0] = (g_aiT4[0][0]*E[0] + g_aiT4[0][1]*E[1] + add)>>shift;
    dst[2*line] = (g_aiT4[2][0]*E[0] + g_aiT4[2][1]*E[1] + add)>>shift;
    dst[line] = (g_aiT4[1][0]*O[0] + g_aiT4[1][1]*O[1] + add)>>shift;
    dst[3*line] = (g_aiT4[3][0]*O[0] + g_aiT4[3][1]*O[1] + add)>>shift;

    src += 4;
    dst ++;
  }
}

// Fast DST Algorithm. Full matrix multiplication for DST and Fast DST algorithm 
// give identical results
void fastForwardDst(short *block,short *coeff,int shift)  // input block, output coeff
{
  int i, c[4];
  int rnd_factor = 1<<(shift-1);
  for (i=0; i<4; i++)
  {
    // Intermediate Variables
    c[0] = block[4*i+0] + block[4*i+3];
    c[1] = block[4*i+1] + block[4*i+3];
    c[2] = block[4*i+0] - block[4*i+1];
    c[3] = 74* block[4*i+2];

    coeff[   i] =  ( 29 * c[0] + 55 * c[1]         + c[3]               + rnd_factor ) >> shift;
    coeff[ 4+i] =  ( 74 * (block[4*i+0]+ block[4*i+1] - block[4*i+3])   + rnd_factor ) >> shift;
    coeff[ 8+i] =  ( 29 * c[2] + 55 * c[0]         - c[3]               + rnd_factor ) >> shift;
    coeff[12+i] =  ( 55 * c[2] - 29 * c[1]         + c[3]               + rnd_factor ) >> shift;
  }
}

void fastInverseDst(short *tmp,short *block,int shift)  // input tmp, output block
{
  int i, c[4];
  int rnd_factor = 1<<(shift-1);
  for (i=0; i<4; i++)
  {  
    // Intermediate Variables
    c[0] = tmp[  i] + tmp[ 8+i];
    c[1] = tmp[8+i] + tmp[12+i];
    c[2] = tmp[  i] - tmp[12+i];
    c[3] = 74* tmp[4+i];

    block[4*i+0] = Clip3( -32768, 32767, ( 29 * c[0] + 55 * c[1]     + c[3]               + rnd_factor ) >> shift );
    block[4*i+1] = Clip3( -32768, 32767, ( 55 * c[2] - 29 * c[1]     + c[3]               + rnd_factor ) >> shift );
    block[4*i+2] = Clip3( -32768, 32767, ( 74 * (tmp[i] - tmp[8+i]  + tmp[12+i])      + rnd_factor ) >> shift );
    block[4*i+3] = Clip3( -32768, 32767, ( 55 * c[0] + 29 * c[2]     - c[3]               + rnd_factor ) >> shift );
  }
}

void partialButterflyInverse4(short *src,short *dst,int shift, int line)
{
  int j;    
  int E[2],O[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++)
  {    
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */    
    O[0] = g_aiT4[1][0]*src[line] + g_aiT4[3][0]*src[3*line];
    O[1] = g_aiT4[1][1]*src[line] + g_aiT4[3][1]*src[3*line];
    E[0] = g_aiT4[0][0]*src[0] + g_aiT4[2][0]*src[2*line];
    E[1] = g_aiT4[0][1]*src[0] + g_aiT4[2][1]*src[2*line];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
    dst[0] = Clip3( -32768, 32767, (E[0] + O[0] + add)>>shift );
    dst[1] = Clip3( -32768, 32767, (E[1] + O[1] + add)>>shift );
    dst[2] = Clip3( -32768, 32767, (E[1] - O[1] + add)>>shift );
    dst[3] = Clip3( -32768, 32767, (E[0] - O[0] + add)>>shift );
            
    src   ++;
    dst += 4;
  }
}


void partialButterfly8(short *src,short *dst,int shift, int line)
{
  int j,k;  
  int E[4],O[4];
  int EE[2],EO[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++)
  {  
    /* E and O*/
    for (k=0;k<4;k++)
    {
      E[k] = src[k] + src[7-k];
      O[k] = src[k] - src[7-k];
    }    
    /* EE and EO */
    EE[0] = E[0] + E[3];    
    EO[0] = E[0] - E[3];
    EE[1] = E[1] + E[2];
    EO[1] = E[1] - E[2];

    dst[0] = (g_aiT8[0][0]*EE[0] + g_aiT8[0][1]*EE[1] + add)>>shift;
    dst[4*line] = (g_aiT8[4][0]*EE[0] + g_aiT8[4][1]*EE[1] + add)>>shift; 
    dst[2*line] = (g_aiT8[2][0]*EO[0] + g_aiT8[2][1]*EO[1] + add)>>shift;
    dst[6*line] = (g_aiT8[6][0]*EO[0] + g_aiT8[6][1]*EO[1] + add)>>shift; 

    dst[line] = (g_aiT8[1][0]*O[0] + g_aiT8[1][1]*O[1] + g_aiT8[1][2]*O[2] + g_aiT8[1][3]*O[3] + add)>>shift;
    dst[3*line] = (g_aiT8[3][0]*O[0] + g_aiT8[3][1]*O[1] + g_aiT8[3][2]*O[2] + g_aiT8[3][3]*O[3] + add)>>shift;
    dst[5*line] = (g_aiT8[5][0]*O[0] + g_aiT8[5][1]*O[1] + g_aiT8[5][2]*O[2] + g_aiT8[5][3]*O[3] + add)>>shift;
    dst[7*line] = (g_aiT8[7][0]*O[0] + g_aiT8[7][1]*O[1] + g_aiT8[7][2]*O[2] + g_aiT8[7][3]*O[3] + add)>>shift;

    src += 8;
    dst ++;
  }
}


void partialButterflyInverse8(short *src,short *dst,int shift, int line)
{
  int j,k;    
  int E[4],O[4];
  int EE[2],EO[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++) 
  {    
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    for (k=0;k<4;k++)
    {
      O[k] = g_aiT8[ 1][k]*src[line] + g_aiT8[ 3][k]*src[3*line] + g_aiT8[ 5][k]*src[5*line] + g_aiT8[ 7][k]*src[7*line];
    }

    EO[0] = g_aiT8[2][0]*src[ 2*line ] + g_aiT8[6][0]*src[ 6*line ];
    EO[1] = g_aiT8[2][1]*src[ 2*line ] + g_aiT8[6][1]*src[ 6*line ];
    EE[0] = g_aiT8[0][0]*src[ 0      ] + g_aiT8[4][0]*src[ 4*line ];
    EE[1] = g_aiT8[0][1]*src[ 0      ] + g_aiT8[4][1]*src[ 4*line ];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */ 
    E[0] = EE[0] + EO[0];
    E[3] = EE[0] - EO[0];
    E[1] = EE[1] + EO[1];
    E[2] = EE[1] - EO[1];
    for (k=0;k<4;k++)
    {
      dst[ k   ] = Clip3( -32768, 32767, (E[k] + O[k] + add)>>shift );
      dst[ k+4 ] = Clip3( -32768, 32767, (E[3-k] - O[3-k] + add)>>shift );
    }   
    src ++;
    dst += 8;
  }
}


void partialButterfly16(short *src,short *dst,int shift, int line)
{
  int j,k;
  int E[8],O[8];
  int EE[4],EO[4];
  int EEE[2],EEO[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++) 
  {    
    /* E and O*/
    for (k=0;k<8;k++)
    {
      E[k] = src[k] + src[15-k];
      O[k] = src[k] - src[15-k];
    } 
    /* EE and EO */
    for (k=0;k<4;k++)
    {
      EE[k] = E[k] + E[7-k];
      EO[k] = E[k] - E[7-k];
    }
    /* EEE and EEO */
    EEE[0] = EE[0] + EE[3];    
    EEO[0] = EE[0] - EE[3];
    EEE[1] = EE[1] + EE[2];
    EEO[1] = EE[1] - EE[2];

    dst[ 0      ] = (g_aiT16[ 0][0]*EEE[0] + g_aiT16[ 0][1]*EEE[1] + add)>>shift;        
    dst[ 8*line ] = (g_aiT16[ 8][0]*EEE[0] + g_aiT16[ 8][1]*EEE[1] + add)>>shift;    
    dst[ 4*line ] = (g_aiT16[ 4][0]*EEO[0] + g_aiT16[ 4][1]*EEO[1] + add)>>shift;        
    dst[ 12*line] = (g_aiT16[12][0]*EEO[0] + g_aiT16[12][1]*EEO[1] + add)>>shift;

    for (k=2;k<16;k+=4)
    {
      dst[ k*line ] = (g_aiT16[k][0]*EO[0] + g_aiT16[k][1]*EO[1] + g_aiT16[k][2]*EO[2] + g_aiT16[k][3]*EO[3] + add)>>shift;      
    }

    for (k=1;k<16;k+=2)
    {
      dst[ k*line ] = (g_aiT16[k][0]*O[0] + g_aiT16[k][1]*O[1] + g_aiT16[k][2]*O[2] + g_aiT16[k][3]*O[3] + 
        g_aiT16[k][4]*O[4] + g_aiT16[k][5]*O[5] + g_aiT16[k][6]*O[6] + g_aiT16[k][7]*O[7] + add)>>shift;
    }

    src += 16;
    dst ++; 

  }
}


void partialButterflyInverse16(short *src,short *dst,int shift, int line)
{
  int j,k;  
  int E[8],O[8];
  int EE[4],EO[4];
  int EEE[2],EEO[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++)
  {    
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    for (k=0;k<8;k++)
    {
      O[k] = g_aiT16[ 1][k]*src[ line] + g_aiT16[ 3][k]*src[ 3*line] + g_aiT16[ 5][k]*src[ 5*line] + g_aiT16[ 7][k]*src[ 7*line] + 
        g_aiT16[ 9][k]*src[ 9*line] + g_aiT16[11][k]*src[11*line] + g_aiT16[13][k]*src[13*line] + g_aiT16[15][k]*src[15*line];
    }
    for (k=0;k<4;k++)
    {
      EO[k] = g_aiT16[ 2][k]*src[ 2*line] + g_aiT16[ 6][k]*src[ 6*line] + g_aiT16[10][k]*src[10*line] + g_aiT16[14][k]*src[14*line];
    }
    EEO[0] = g_aiT16[4][0]*src[ 4*line ] + g_aiT16[12][0]*src[ 12*line ];
    EEE[0] = g_aiT16[0][0]*src[ 0      ] + g_aiT16[ 8][0]*src[ 8*line  ];
    EEO[1] = g_aiT16[4][1]*src[ 4*line ] + g_aiT16[12][1]*src[ 12*line ];
    EEE[1] = g_aiT16[0][1]*src[ 0      ] + g_aiT16[ 8][1]*src[ 8*line  ];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */ 
    for (k=0;k<2;k++)
    {
      EE[k] = EEE[k] + EEO[k];
      EE[k+2] = EEE[1-k] - EEO[1-k];
    }    
    for (k=0;k<4;k++)
    {
      E[k] = EE[k] + EO[k];
      E[k+4] = EE[3-k] - EO[3-k];
    }    
    for (k=0;k<8;k++)
    {
      dst[k]   = Clip3( -32768, 32767, (E[k] + O[k] + add)>>shift );
      dst[k+8] = Clip3( -32768, 32767, (E[7-k] - O[7-k] + add)>>shift );
    }   
    src ++; 
    dst += 16;
  }
}


void partialButterfly32(short *src,short *dst,int shift, int line)
{
  int j,k;
  int E[16],O[16];
  int EE[8],EO[8];
  int EEE[4],EEO[4];
  int EEEE[2],EEEO[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++)
  {    
    /* E and O*/
    for (k=0;k<16;k++)
    {
      E[k] = src[k] + src[31-k];
      O[k] = src[k] - src[31-k];
    } 
    /* EE and EO */
    for (k=0;k<8;k++)
    {
      EE[k] = E[k] + E[15-k];
      EO[k] = E[k] - E[15-k];
    }
    /* EEE and EEO */
    for (k=0;k<4;k++)
    {
      EEE[k] = EE[k] + EE[7-k];
      EEO[k] = EE[k] - EE[7-k];
    }
    /* EEEE and EEEO */
    EEEE[0] = EEE[0] + EEE[3];    
    EEEO[0] = EEE[0] - EEE[3];
    EEEE[1] = EEE[1] + EEE[2];
    EEEO[1] = EEE[1] - EEE[2];

    dst[ 0       ] = (g_aiT32[ 0][0]*EEEE[0] + g_aiT32[ 0][1]*EEEE[1] + add)>>shift;
    dst[ 16*line ] = (g_aiT32[16][0]*EEEE[0] + g_aiT32[16][1]*EEEE[1] + add)>>shift;
    dst[ 8*line  ] = (g_aiT32[ 8][0]*EEEO[0] + g_aiT32[ 8][1]*EEEO[1] + add)>>shift; 
    dst[ 24*line ] = (g_aiT32[24][0]*EEEO[0] + g_aiT32[24][1]*EEEO[1] + add)>>shift;
    for (k=4;k<32;k+=8)
    {
      dst[ k*line ] = (g_aiT32[k][0]*EEO[0] + g_aiT32[k][1]*EEO[1] + g_aiT32[k][2]*EEO[2] + g_aiT32[k][3]*EEO[3] + add)>>shift;
    }       
    for (k=2;k<32;k+=4)
    {
      dst[ k*line ] = (g_aiT32[k][0]*EO[0] + g_aiT32[k][1]*EO[1] + g_aiT32[k][2]*EO[2] + g_aiT32[k][3]*EO[3] + 
        g_aiT32[k][4]*EO[4] + g_aiT32[k][5]*EO[5] + g_aiT32[k][6]*EO[6] + g_aiT32[k][7]*EO[7] + add)>>shift;
    }       
    for (k=1;k<32;k+=2)
    {
      dst[ k*line ] = (g_aiT32[k][ 0]*O[ 0] + g_aiT32[k][ 1]*O[ 1] + g_aiT32[k][ 2]*O[ 2] + g_aiT32[k][ 3]*O[ 3] + 
        g_aiT32[k][ 4]*O[ 4] + g_aiT32[k][ 5]*O[ 5] + g_aiT32[k][ 6]*O[ 6] + g_aiT32[k][ 7]*O[ 7] +
        g_aiT32[k][ 8]*O[ 8] + g_aiT32[k][ 9]*O[ 9] + g_aiT32[k][10]*O[10] + g_aiT32[k][11]*O[11] + 
        g_aiT32[k][12]*O[12] + g_aiT32[k][13]*O[13] + g_aiT32[k][14]*O[14] + g_aiT32[k][15]*O[15] + add)>>shift;
    }
    src += 32;
    dst ++;
  }
}


void partialButterflyInverse32(short *src,short *dst,int shift, int line)
{
  int j,k;  
  int E[16],O[16];
  int EE[8],EO[8];
  int EEE[4],EEO[4];
  int EEEE[2],EEEO[2];
  int add = 1<<(shift-1);

  for (j=0; j<line; j++)
  {    
    /* Utilizing symmetry properties to the maximum to minimize the number of multiplications */
    for (k=0;k<16;k++)
    {
      O[k] = g_aiT32[ 1][k]*src[ line  ] + g_aiT32[ 3][k]*src[ 3*line  ] + g_aiT32[ 5][k]*src[ 5*line  ] + g_aiT32[ 7][k]*src[ 7*line  ] + 
        g_aiT32[ 9][k]*src[ 9*line  ] + g_aiT32[11][k]*src[ 11*line ] + g_aiT32[13][k]*src[ 13*line ] + g_aiT32[15][k]*src[ 15*line ] + 
        g_aiT32[17][k]*src[ 17*line ] + g_aiT32[19][k]*src[ 19*line ] + g_aiT32[21][k]*src[ 21*line ] + g_aiT32[23][k]*src[ 23*line ] + 
        g_aiT32[25][k]*src[ 25*line ] + g_aiT32[27][k]*src[ 27*line ] + g_aiT32[29][k]*src[ 29*line ] + g_aiT32[31][k]*src[ 31*line ];
    }
    for (k=0;k<8;k++)
    {
      EO[k] = g_aiT32[ 2][k]*src[ 2*line  ] + g_aiT32[ 6][k]*src[ 6*line  ] + g_aiT32[10][k]*src[ 10*line ] + g_aiT32[14][k]*src[ 14*line ] + 
        g_aiT32[18][k]*src[ 18*line ] + g_aiT32[22][k]*src[ 22*line ] + g_aiT32[26][k]*src[ 26*line ] + g_aiT32[30][k]*src[ 30*line ];
    }
    for (k=0;k<4;k++)
    {
      EEO[k] = g_aiT32[4][k]*src[ 4*line ] + g_aiT32[12][k]*src[ 12*line ] + g_aiT32[20][k]*src[ 20*line ] + g_aiT32[28][k]*src[ 28*line ];
    }
    EEEO[0] = g_aiT32[8][0]*src[ 8*line ] + g_aiT32[24][0]*src[ 24*line ];
    EEEO[1] = g_aiT32[8][1]*src[ 8*line ] + g_aiT32[24][1]*src[ 24*line ];
    EEEE[0] = g_aiT32[0][0]*src[ 0      ] + g_aiT32[16][0]*src[ 16*line ];    
    EEEE[1] = g_aiT32[0][1]*src[ 0      ] + g_aiT32[16][1]*src[ 16*line ];

    /* Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector */
    EEE[0] = EEEE[0] + EEEO[0];
    EEE[3] = EEEE[0] - EEEO[0];
    EEE[1] = EEEE[1] + EEEO[1];
    EEE[2] = EEEE[1] - EEEO[1];    
    for (k=0;k<4;k++)
    {
      EE[k] = EEE[k] + EEO[k];
      EE[k+4] = EEE[3-k] - EEO[3-k];
    }    
    for (k=0;k<8;k++)
    {
      E[k] = EE[k] + EO[k];
      E[k+8] = EE[7-k] - EO[7-k];
    }    
    for (k=0;k<16;k++)
    {
      dst[k]    = Clip3( -32768, 32767, (E[k] + O[k] + add)>>shift );
      dst[k+16] = Clip3( -32768, 32767, (E[15-k] - O[15-k] + add)>>shift );
    }
    src ++;
    dst += 32;
  }
}

/** MxN forward transform (2D)
*  \param block input data (residual)
*  \param coeff output data (transform coefficients)
*  \param iWidth input data (width of transform)
*  \param iHeight input data (height of transform)
*/
void xTrMxN(short *block,short *coeff, int iWidth, int iHeight, UInt uiMode)
{
#if FULL_NBIT
  int shift_1st = g_aucConvertToBit[iWidth]  + 1 + g_uiBitDepth - 8; // log2(iWidth) - 1 + g_uiBitDepth - 8
#else
  int shift_1st = g_aucConvertToBit[iWidth]  + 1 + g_uiBitIncrement; // log2(iWidth) - 1 + g_uiBitIncrement
#endif
  int shift_2nd = g_aucConvertToBit[iHeight]  + 8;                   // log2(iHeight) + 6

  short tmp[ 64 * 64 ];

#if !REMOVE_NSQT
  if( iWidth == 16 && iHeight == 4)
  {
    partialButterfly16( block, tmp, shift_1st, iHeight );
    partialButterfly4( tmp, coeff, shift_2nd, iWidth );
  }
  else if( iWidth == 32 && iHeight == 8 )
  {
    partialButterfly32( block, tmp, shift_1st, iHeight );
    partialButterfly8( tmp, coeff, shift_2nd, iWidth );
  }
  else if( iWidth == 4 && iHeight == 16)
  {
    partialButterfly4( block, tmp, shift_1st, iHeight );
    partialButterfly16( tmp, coeff, shift_2nd, iWidth );
  }
  else if( iWidth == 8 && iHeight == 32 )
  {
    partialButterfly8( block, tmp, shift_1st, iHeight );
    partialButterfly32( tmp, coeff, shift_2nd, iWidth );
  }
  else
#endif
  if( iWidth == 4 && iHeight == 4)
  {
#if INTRA_TRANS_SIMP
    if (uiMode != REG_DCT)
    {
      fastForwardDst(block,tmp,shift_1st); // Forward DST BY FAST ALGORITHM, block input, tmp output
      fastForwardDst(tmp,coeff,shift_2nd); // Forward DST BY FAST ALGORITHM, tmp input, coeff output
    }
    else
    {
      partialButterfly4(block, tmp, shift_1st, iHeight);
      partialButterfly4(tmp, coeff, shift_2nd, iWidth);
    }

#else
    if (uiMode != REG_DCT && (!uiMode || (uiMode>=2 && uiMode <= 25)))    // Check for DCT or DST
    {
      fastForwardDst(block,tmp,shift_1st); // Forward DST BY FAST ALGORITHM, block input, tmp output
    }
    else  
    {
      partialButterfly4(block, tmp, shift_1st, iHeight);
    }
    if (uiMode != REG_DCT && (!uiMode || (uiMode>=11 && uiMode <= 34)))    // Check for DCT or DST
    {
      fastForwardDst(tmp,coeff,shift_2nd); // Forward DST BY FAST ALGORITHM, tmp input, coeff output
    }
    else  
    {
      partialButterfly4(tmp, coeff, shift_2nd, iWidth);
    }
#endif
  }
  else if( iWidth == 8 && iHeight == 8)
  {
    partialButterfly8( block, tmp, shift_1st, iHeight );
    partialButterfly8( tmp, coeff, shift_2nd, iWidth );
  }
  else if( iWidth == 16 && iHeight == 16)
  {
    partialButterfly16( block, tmp, shift_1st, iHeight );
    partialButterfly16( tmp, coeff, shift_2nd, iWidth );
  }
  else if( iWidth == 32 && iHeight == 32)
  {
    partialButterfly32( block, tmp, shift_1st, iHeight );
    partialButterfly32( tmp, coeff, shift_2nd, iWidth );
  }
}
/** MxN inverse transform (2D)
*  \param coeff input data (transform coefficients)
*  \param block output data (residual)
*  \param iWidth input data (width of transform)
*  \param iHeight input data (height of transform)
*/
void xITrMxN(short *coeff,short *block, int iWidth, int iHeight, UInt uiMode)
{
  int shift_1st = SHIFT_INV_1ST;
#if FULL_NBIT
  int shift_2nd = SHIFT_INV_2ND - ((short)g_uiBitDepth - 8);
#else
  int shift_2nd = SHIFT_INV_2ND - g_uiBitIncrement;
#endif

  short tmp[ 64*64];
#if !REMOVE_NSQT
  if( iWidth == 16 && iHeight == 4)
  {
    partialButterflyInverse4(coeff,tmp,shift_1st,iWidth);
    partialButterflyInverse16(tmp,block,shift_2nd,iHeight);
  }
  else if( iWidth == 32 && iHeight == 8)
  {
    partialButterflyInverse8(coeff,tmp,shift_1st,iWidth);
    partialButterflyInverse32(tmp,block,shift_2nd,iHeight);
  }
  else if( iWidth == 4 && iHeight == 16)
  {
    partialButterflyInverse16(coeff,tmp,shift_1st,iWidth);
    partialButterflyInverse4(tmp,block,shift_2nd,iHeight);
  }
  else if( iWidth == 8 && iHeight == 32)
  {
    partialButterflyInverse32(coeff,tmp,shift_1st,iWidth);
    partialButterflyInverse8(tmp,block,shift_2nd,iHeight);
  }
  else
#endif
  if( iWidth == 4 && iHeight == 4)
  {
#if INTRA_TRANS_SIMP
    if (uiMode != REG_DCT)
    {
      fastInverseDst(coeff,tmp,shift_1st);    // Inverse DST by FAST Algorithm, coeff input, tmp output
      fastInverseDst(tmp,block,shift_2nd); // Inverse DST by FAST Algorithm, tmp input, coeff output
    }
    else
    {
      partialButterflyInverse4(coeff,tmp,shift_1st,iWidth);
      partialButterflyInverse4(tmp,block,shift_2nd,iHeight);
    }
#else
    if (uiMode != REG_DCT && (!uiMode || (uiMode>=11 && uiMode <= 34)))    // Check for DCT or DST
    {
      fastInverseDst(coeff,tmp,shift_1st);    // Inverse DST by FAST Algorithm, coeff input, tmp output
    }
    else
    {
      partialButterflyInverse4(coeff,tmp,shift_1st,iWidth);    
    } 
    if (uiMode != REG_DCT && (!uiMode || (uiMode>=2 && uiMode <= 25)))    // Check for DCT or DST
    {
      fastInverseDst(tmp,block,shift_2nd); // Inverse DST by FAST Algorithm, tmp input, coeff output
    }
    else
    {
      partialButterflyInverse4(tmp,block,shift_2nd,iHeight);
    } 
#endif
  }
  else if( iWidth == 8 && iHeight == 8)
  {
    partialButterflyInverse8(coeff,tmp,shift_1st,iWidth);
    partialButterflyInverse8(tmp,block,shift_2nd,iHeight);
  }
  else if( iWidth == 16 && iHeight == 16)
  {
    partialButterflyInverse16(coeff,tmp,shift_1st,iWidth);
    partialButterflyInverse16(tmp,block,shift_2nd,iHeight);
  }
  else if( iWidth == 32 && iHeight == 32)
  {
    partialButterflyInverse32(coeff,tmp,shift_1st,iWidth);
    partialButterflyInverse32(tmp,block,shift_2nd,iHeight);
  }
}

#endif //MATRIX_MULT

// To minimize the distortion only. No rate is considered. 
Void TComTrQuant::signBitHidingHDQ( TComDataCU* pcCU, TCoeff* pQCoef, TCoeff* pCoef, UInt const *scan, Int* deltaU, Int width, Int height )
{
  Int lastCG = -1;
  Int absSum = 0 ;
  Int n ;

  for( Int subSet = (width*height-1) >> LOG2_SCAN_SET_SIZE; subSet >= 0; subSet-- )
  {
    Int  subPos     = subSet << LOG2_SCAN_SET_SIZE;
    Int  firstNZPosInCG=SCAN_SET_SIZE , lastNZPosInCG=-1 ;
    absSum = 0 ;

    for(n = SCAN_SET_SIZE-1; n >= 0; --n )
    {
      if( pQCoef[ scan[ n + subPos ]] )
      {
        lastNZPosInCG = n;
        break;
      }
    }

    for(n = 0; n <SCAN_SET_SIZE; n++ )
    {
      if( pQCoef[ scan[ n + subPos ]] )
      {
        firstNZPosInCG = n;
        break;
      }
    }

    for(n = firstNZPosInCG; n <=lastNZPosInCG; n++ )
    {
      absSum += pQCoef[ scan[ n + subPos ]];
    }

    if(lastNZPosInCG>=0 && lastCG==-1) 
    {
      lastCG = 1 ; 
    }

    if( lastNZPosInCG-firstNZPosInCG>=SBH_THRESHOLD )
    {
      UInt signbit = (pQCoef[scan[subPos+firstNZPosInCG]]>0?0:1) ;
      if( signbit!=(absSum&0x1) )  //compare signbit with sum_parity
      {
        Int minCostInc = MAX_INT,  minPos =-1, finalChange=0, curCost=MAX_INT, curChange=0;
        
        for( n = (lastCG==1?lastNZPosInCG:SCAN_SET_SIZE-1) ; n >= 0; --n )
        {
          UInt blkPos   = scan[ n+subPos ];
          if(pQCoef[ blkPos ] != 0 )
          {
            if(deltaU[blkPos]>0)
            {
              curCost = - deltaU[blkPos]; 
              curChange=1 ;
            }
            else 
            {
              //curChange =-1;
              if(n==firstNZPosInCG && abs(pQCoef[blkPos])==1)
              {
                curCost=MAX_INT ; 
              }
              else
              {
                curCost = deltaU[blkPos]; 
                curChange =-1;
              }
            }
          }
          else
          {
            if(n<firstNZPosInCG)
            {
              UInt thisSignBit = (pCoef[blkPos]>=0?0:1);
              if(thisSignBit != signbit )
              {
                curCost = MAX_INT;
              }
              else
              { 
                curCost = - (deltaU[blkPos])  ;
                curChange = 1 ;
              }
            }
            else
            {
              curCost = - (deltaU[blkPos])  ;
              curChange = 1 ;
            }
          }

          if( curCost<minCostInc)
          {
            minCostInc = curCost ;
            finalChange = curChange ;
            minPos = blkPos ;
          }
        } //CG loop

        if(pQCoef[minPos] == 32767 || pQCoef[minPos] == -32768)
        {
          finalChange = -1;
        }

        if(pCoef[minPos]>=0)
        {
          pQCoef[minPos] += finalChange ; 
        }
        else 
        { 
          pQCoef[minPos] -= finalChange ;
        }  
      } // Hide
    }
    if(lastCG==1) 
    {
      lastCG=0 ;
    }
  } // TU loop

  return;
}

Void TComTrQuant::xQuant( TComDataCU* pcCU, 
                          Int*        pSrc, 
                          TCoeff*     pDes, 
#if ADAPTIVE_QP_SELECTION
                          Int*&       pArlDes,
#endif
                          Int         iWidth, 
                          Int         iHeight, 
                          UInt&       uiAcSum, 
                          TextType    eTType, 
                          UInt        uiAbsPartIdx )
{
  Int*   piCoef    = pSrc;
  TCoeff* piQCoef   = pDes;
#if ADAPTIVE_QP_SELECTION
  Int*   piArlCCoef = pArlDes;
#endif
  Int   iAdd = 0;
 
  Bool useRDOQForTransformSkip = !(m_useTansformSkipFast && pcCU->getTransformSkip(uiAbsPartIdx,eTType));
  if ( m_bUseRDOQ && (eTType == TEXT_LUMA || RDOQ_CHROMA) && useRDOQForTransformSkip)
  {
#if ADAPTIVE_QP_SELECTION
    xRateDistOptQuant( pcCU, piCoef, pDes, pArlDes, iWidth, iHeight, uiAcSum, eTType, uiAbsPartIdx );
#else
    xRateDistOptQuant( pcCU, piCoef, pDes, iWidth, iHeight, uiAcSum, eTType, uiAbsPartIdx );
#endif
  }
  else
  {
    const UInt   log2BlockSize   = g_aucConvertToBit[ iWidth ] + 2;

    UInt scanIdx = pcCU->getCoefScanIdx(uiAbsPartIdx, iWidth, eTType==TEXT_LUMA, pcCU->isIntra(uiAbsPartIdx));
    if (scanIdx == SCAN_ZIGZAG)
    {
      scanIdx = SCAN_DIAG;
    }

#if REMOVE_NSQT
    const UInt *scan = g_auiSigLastScan[ scanIdx ][ log2BlockSize - 1 ];
#else
    if (iWidth != iHeight)
    {
      scanIdx = SCAN_DIAG;
    }
    
    const UInt * scan;
    if (iWidth == iHeight)
    {
      scan = g_auiSigLastScan[ scanIdx ][ log2BlockSize - 1 ];
    }
    else
    {
      scan = g_sigScanNSQT[ log2BlockSize - 2 ];
    }
#endif
    
    Int deltaU[32*32] ;

#if ADAPTIVE_QP_SELECTION
    QpParam cQpBase;
    Int iQpBase = pcCU->getSlice()->getSliceQpBase();

    Int qpScaled;
    Int qpBDOffset = (eTType == TEXT_LUMA)? pcCU->getSlice()->getSPS()->getQpBDOffsetY() : pcCU->getSlice()->getSPS()->getQpBDOffsetC();

    if(eTType == TEXT_LUMA)
    {
      qpScaled = iQpBase + qpBDOffset;
    }
    else
    {
#if CHROMA_QP_EXTENSION
      qpScaled = Clip3( -qpBDOffset, 57, iQpBase);
#else
      qpScaled = Clip3( -qpBDOffset, 51, iQpBase);
#endif

      if(qpScaled < 0)
      {
        qpScaled = qpScaled +  qpBDOffset;
      }
      else
      {
#if CHROMA_QP_EXTENSION
        qpScaled = g_aucChromaScale[ qpScaled ] + qpBDOffset;
#else
        qpScaled = g_aucChromaScale[ Clip3(0, 51, qpScaled) ] + qpBDOffset;
#endif
      }
    }
    cQpBase.setQpParam(qpScaled);
#endif

#if !REMOVE_NSQT
    Bool bNonSqureFlag = ( iWidth != iHeight );
#endif
    UInt dir           = SCALING_LIST_SQT;
#if !REMOVE_NSQT
    if( bNonSqureFlag )
    {
      dir = ( iWidth < iHeight )?  SCALING_LIST_VER: SCALING_LIST_HOR;
      UInt uiWidthBit  = g_aucConvertToBit[ iWidth ] + 2;
      UInt uiHeightBit = g_aucConvertToBit[ iHeight ] + 2;
      iWidth  = 1 << ( ( uiWidthBit + uiHeightBit) >> 1 );
      iHeight = iWidth;
    }    
#endif
    
    UInt uiLog2TrSize = g_aucConvertToBit[ iWidth ] + 2;
    Int scalingListType = (pcCU->isIntra(uiAbsPartIdx) ? 0 : 3) + g_eTTable[(Int)eTType];
    assert(scalingListType < 6);
    Int *piQuantCoeff = 0;
    piQuantCoeff = getQuantCoeff(scalingListType,m_cQP.m_iRem,uiLog2TrSize-2, dir);

#if FULL_NBIT
    UInt uiBitDepth = g_uiBitDepth;
#else
    UInt uiBitDepth = g_uiBitDepth + g_uiBitIncrement;
#endif
    Int iTransformShift = MAX_TR_DYNAMIC_RANGE - uiBitDepth - uiLog2TrSize;  // Represents scaling through forward transform

    Int iQBits = QUANT_SHIFT + m_cQP.m_iPer + iTransformShift;                // Right shift of non-RDOQ quantizer;  level = (coeff*uiQ + offset)>>q_bits

    iAdd = (pcCU->getSlice()->getSliceType()==I_SLICE ? 171 : 85) << (iQBits-9);

#if ADAPTIVE_QP_SELECTION
    iQBits = QUANT_SHIFT + cQpBase.m_iPer + iTransformShift;
    iAdd = (pcCU->getSlice()->getSliceType()==I_SLICE ? 171 : 85) << (iQBits-9);
    Int iQBitsC = QUANT_SHIFT + cQpBase.m_iPer + iTransformShift - ARL_C_PRECISION;  
    Int iAddC   = 1 << (iQBitsC-1);
#endif

    Int qBits8 = iQBits-8;
    for( Int n = 0; n < iWidth*iHeight; n++ )
    {
      Int iLevel;
      Int  iSign;
      UInt uiBlockPos = n;
      iLevel  = piCoef[uiBlockPos];
      iSign   = (iLevel < 0 ? -1: 1);      

#if ADAPTIVE_QP_SELECTION
      Int64 tmpLevel = (Int64)abs(iLevel) * piQuantCoeff[uiBlockPos];
      if( m_bUseAdaptQpSelect )
      {
        piArlCCoef[uiBlockPos] = (Int)((tmpLevel + iAddC ) >> iQBitsC);
      }
      iLevel = (Int)((tmpLevel + iAdd ) >> iQBits);
      deltaU[uiBlockPos] = (Int)((tmpLevel - (iLevel<<iQBits) )>> qBits8);
#else
      iLevel = ((Int64)abs(iLevel) * piQuantCoeff[uiBlockPos] + iAdd ) >> iQBits;
      deltaU[uiBlockPos] = (Int)( ((Int64)abs(iLevel) * piQuantCoeff[uiBlockPos] - (iLevel<<iQBits) )>> qBits8 );
#endif
      uiAcSum += iLevel;
      iLevel *= iSign;        
      piQCoef[uiBlockPos] = Clip3( -32768, 32767, iLevel );
    } // for n
    if( pcCU->getSlice()->getPPS()->getSignHideFlag() )
    {
      if(uiAcSum>=2)
      {
        signBitHidingHDQ( pcCU, piQCoef, piCoef, scan, deltaU, iWidth, iHeight ) ;
      }
    }
  } //if RDOQ
  //return;

}

Void TComTrQuant::xDeQuant( const TCoeff* pSrc, Int* pDes, Int iWidth, Int iHeight, Int scalingListType )
{
  
  const TCoeff* piQCoef   = pSrc;
  Int*   piCoef    = pDes;
  UInt dir          = SCALING_LIST_SQT;
#if !REMOVE_NSQT
  if( iWidth != iHeight )
  {
    dir          = ( iWidth < iHeight )? SCALING_LIST_VER: SCALING_LIST_HOR;
    UInt uiWidthBit  = g_aucConvertToBit[ iWidth ]  + 2;
    UInt uiHeightBit = g_aucConvertToBit[ iHeight ] + 2;
    iWidth  = 1 << ( ( uiWidthBit + uiHeightBit) >> 1 );
    iHeight = iWidth;
  }    
#endif
  
  if ( iWidth > (Int)m_uiMaxTrSize )
  {
    iWidth  = m_uiMaxTrSize;
    iHeight = m_uiMaxTrSize;
  }
  
  Int iShift,iAdd,iCoeffQ;
  UInt uiLog2TrSize = g_aucConvertToBit[ iWidth ] + 2;

#if FULL_NBIT
  UInt uiBitDepth = g_uiBitDepth;
#else
  UInt uiBitDepth = g_uiBitDepth + g_uiBitIncrement;
#endif
  Int iTransformShift = MAX_TR_DYNAMIC_RANGE - uiBitDepth - uiLog2TrSize; 

  iShift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - iTransformShift;

  TCoeff clipQCoef;
  const Int bitRange = min( 15, ( Int )( 12 + uiLog2TrSize + uiBitDepth - m_cQP.m_iPer) );
  const Int levelLimit = 1 << bitRange;

  if(getUseScalingList())
  {
    iShift += 4;
    if(iShift > m_cQP.m_iPer)
    {
      iAdd = 1 << (iShift - m_cQP.m_iPer - 1);
    }
    else
    {
      iAdd = 0;
    }
    Int *piDequantCoef = getDequantCoeff(scalingListType,m_cQP.m_iRem,uiLog2TrSize-2,dir);

    if(iShift > m_cQP.m_iPer)
    {
      for( Int n = 0; n < iWidth*iHeight; n++ )
      {
        clipQCoef = Clip3( -32768, 32767, piQCoef[n] );
        iCoeffQ = ((clipQCoef * piDequantCoef[n]) + iAdd ) >> (iShift -  m_cQP.m_iPer);
        piCoef[n] = Clip3(-32768,32767,iCoeffQ);
      }
    }
    else
    {
      for( Int n = 0; n < iWidth*iHeight; n++ )
      {
        clipQCoef = Clip3( -levelLimit, levelLimit - 1, piQCoef[n] );
        iCoeffQ = (clipQCoef * piDequantCoef[n]) << (m_cQP.m_iPer - iShift);
        piCoef[n] = Clip3(-32768,32767,iCoeffQ);
      }
    }
  }
  else
  {
    iAdd = 1 << (iShift-1);
    Int scale = g_invQuantScales[m_cQP.m_iRem] << m_cQP.m_iPer;

    for( Int n = 0; n < iWidth*iHeight; n++ )
    {
      clipQCoef = Clip3( -32768, 32767, piQCoef[n] );
      iCoeffQ = ( clipQCoef * scale + iAdd ) >> iShift;
      piCoef[n] = Clip3(-32768,32767,iCoeffQ);
    }
  }
}

Void TComTrQuant::init( UInt uiMaxWidth, UInt uiMaxHeight, UInt uiMaxTrSize, Int iSymbolMode, UInt *aTableLP4, UInt *aTableLP8, UInt *aTableLastPosVlcIndex,
                       Bool bUseRDOQ,  Bool bEnc, Bool useTransformSkipFast
#if ADAPTIVE_QP_SELECTION
                       , Bool bUseAdaptQpSelect
#endif
                       )
{
  m_uiMaxTrSize  = uiMaxTrSize;
  m_bEnc         = bEnc;
  m_bUseRDOQ     = bUseRDOQ;
#if ADAPTIVE_QP_SELECTION
  m_bUseAdaptQpSelect = bUseAdaptQpSelect;
#endif
  m_useTansformSkipFast = useTransformSkipFast;
}

Void TComTrQuant::transformNxN( TComDataCU* pcCU, 
                               Pel*        pcResidual, 
                               UInt        uiStride, 
                               TCoeff*     rpcCoeff, 
#if ADAPTIVE_QP_SELECTION
                               Int*&       rpcArlCoeff, 
#endif
                               UInt        uiWidth, 
                               UInt        uiHeight, 
                               UInt&       uiAbsSum, 
                               TextType    eTType, 
                               UInt        uiAbsPartIdx,
                               Bool        useTransformSkip
                               )
{
  if (pcCU->getCUTransquantBypass(uiAbsPartIdx))
  {
    uiAbsSum=0;
    for (UInt k = 0; k<uiHeight; k++)
    {
      for (UInt j = 0; j<uiWidth; j++)
      {
        rpcCoeff[k*uiWidth+j]= pcResidual[k*uiStride+j];
        uiAbsSum += abs(pcResidual[k*uiStride+j]);
      }
    }
    return;
  }
  UInt uiMode;  //luma intra pred
  if(eTType == TEXT_LUMA && pcCU->getPredictionMode(uiAbsPartIdx) == MODE_INTRA )
  {
    uiMode = pcCU->getLumaIntraDir( uiAbsPartIdx );
  }
  else
  {
    uiMode = REG_DCT;
  }
  
  uiAbsSum = 0;
  assert( (pcCU->getSlice()->getSPS()->getMaxTrSize() >= uiWidth) );
  if(useTransformSkip)
  {
    xTransformSkip( pcResidual, uiStride, m_plTempCoeff, uiWidth, uiHeight );
  }
  else
  {
    xT( uiMode, pcResidual, uiStride, m_plTempCoeff, uiWidth, uiHeight );
  }
  xQuant( pcCU, m_plTempCoeff, rpcCoeff,
#if ADAPTIVE_QP_SELECTION
       rpcArlCoeff,
#endif
       uiWidth, uiHeight, uiAbsSum, eTType, uiAbsPartIdx );
}

Void TComTrQuant::invtransformNxN( Bool transQuantBypass, TextType eText, UInt uiMode,Pel* rpcResidual, UInt uiStride, TCoeff*   pcCoeff, UInt uiWidth, UInt uiHeight,  Int scalingListType, Bool useTransformSkip )
{
  if(transQuantBypass)
  {
    for (UInt k = 0; k<uiHeight; k++)
    {
      for (UInt j = 0; j<uiWidth; j++)
      {
        rpcResidual[k*uiStride+j] = pcCoeff[k*uiWidth+j];
      }
    } 
    return;
  }
  xDeQuant( pcCoeff, m_plTempCoeff, uiWidth, uiHeight, scalingListType);
  if(useTransformSkip == true)
  {
    xITransformSkip( m_plTempCoeff, rpcResidual, uiStride, uiWidth, uiHeight );
  }
  else
  {
    xIT( uiMode, m_plTempCoeff, rpcResidual, uiStride, uiWidth, uiHeight );
  }
}

Void TComTrQuant::invRecurTransformNxN( TComDataCU* pcCU, UInt uiAbsPartIdx, TextType eTxt, Pel* rpcResidual, UInt uiAddr, UInt uiStride, UInt uiWidth, UInt uiHeight, UInt uiMaxTrMode, UInt uiTrMode, TCoeff* rpcCoeff )
{
  if( !pcCU->getCbf(uiAbsPartIdx, eTxt, uiTrMode) )
  {
    return;
  }
  
  UInt uiLumaTrMode, uiChromaTrMode;
  pcCU->convertTransIdx( uiAbsPartIdx, pcCU->getTransformIdx( uiAbsPartIdx ), uiLumaTrMode, uiChromaTrMode );
  const UInt uiStopTrMode = eTxt == TEXT_LUMA ? uiLumaTrMode : uiChromaTrMode;
  
  if( uiTrMode == uiStopTrMode )
  {
    UInt uiDepth      = pcCU->getDepth( uiAbsPartIdx ) + uiTrMode;
    UInt uiLog2TrSize = g_aucConvertToBit[ pcCU->getSlice()->getSPS()->getMaxCUWidth() >> uiDepth ] + 2;
    if( eTxt != TEXT_LUMA && uiLog2TrSize == 2 )
    {
      UInt uiQPDiv = pcCU->getPic()->getNumPartInCU() >> ( ( uiDepth - 1 ) << 1 );
      if( ( uiAbsPartIdx % uiQPDiv ) != 0 )
      {
        return;
      }
      uiWidth  <<= 1;
      uiHeight <<= 1;
    }
    Pel* pResi = rpcResidual + uiAddr;
#if !REMOVE_NSQT
    if( pcCU->useNonSquareTrans( uiTrMode, uiAbsPartIdx ) )
    {
      Int trWidth  = uiWidth;
      Int trHeight = uiHeight;
      pcCU->getNSQTSize( uiTrMode, uiAbsPartIdx, trWidth, trHeight );

      uiWidth  = trWidth;
      uiHeight = trHeight;
    }
#endif
    Int scalingListType = (pcCU->isIntra(uiAbsPartIdx) ? 0 : 3) + g_eTTable[(Int)eTxt];
    assert(scalingListType < 6);
#if INTER_TRANSFORMSKIP
    invtransformNxN( pcCU->getCUTransquantBypass(uiAbsPartIdx), eTxt, REG_DCT, pResi, uiStride, rpcCoeff, uiWidth, uiHeight, scalingListType, pcCU->getTransformSkip(uiAbsPartIdx, eTxt) );
#else
    invtransformNxN( pcCU->getCUTransquantBypass(uiAbsPartIdx), eTxt, REG_DCT, pResi, uiStride, rpcCoeff, uiWidth, uiHeight, scalingListType );
#endif
  }
  else
  {
    uiTrMode++;
    uiWidth  >>= 1;
    uiHeight >>= 1;
    Int trWidth = uiWidth, trHeight = uiHeight;
#if !REMOVE_NSQT
    Int trLastWidth = uiWidth << 1, trLastHeight = uiHeight << 1;
    pcCU->getNSQTSize ( uiTrMode, uiAbsPartIdx, trWidth, trHeight );
    pcCU->getNSQTSize ( uiTrMode - 1, uiAbsPartIdx, trLastWidth, trLastHeight );
#endif
    UInt uiAddrOffset = trHeight * uiStride;
    UInt uiCoefOffset = trWidth * trHeight;
    UInt uiPartOffset = pcCU->getTotalNumPart() >> ( uiTrMode << 1 );
#if !REMOVE_NSQT
    UInt uiInterTUSplitDirection = pcCU->getInterTUSplitDirection ( trWidth, trHeight, trLastWidth, trLastHeight );
    if( uiInterTUSplitDirection != 2 )
    {
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr                                                                                            , uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff ); rpcCoeff += uiCoefOffset; uiAbsPartIdx += uiPartOffset;
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr +     trWidth * uiInterTUSplitDirection +     uiAddrOffset * ( 1 - uiInterTUSplitDirection), uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff ); rpcCoeff += uiCoefOffset; uiAbsPartIdx += uiPartOffset;
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr + 2 * trWidth * uiInterTUSplitDirection + 2 * uiAddrOffset * ( 1 - uiInterTUSplitDirection), uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff ); rpcCoeff += uiCoefOffset; uiAbsPartIdx += uiPartOffset;
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr + 3 * trWidth * uiInterTUSplitDirection + 3 * uiAddrOffset * ( 1 - uiInterTUSplitDirection), uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff );
    }
    else
#endif
    {
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr                         , uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff ); rpcCoeff += uiCoefOffset; uiAbsPartIdx += uiPartOffset;
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr + trWidth               , uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff ); rpcCoeff += uiCoefOffset; uiAbsPartIdx += uiPartOffset;
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr + uiAddrOffset          , uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff ); rpcCoeff += uiCoefOffset; uiAbsPartIdx += uiPartOffset;
      invRecurTransformNxN( pcCU, uiAbsPartIdx, eTxt, rpcResidual, uiAddr + uiAddrOffset + trWidth, uiStride, uiWidth, uiHeight, uiMaxTrMode, uiTrMode, rpcCoeff );
    }
  }
}

// ------------------------------------------------------------------------------------------------
// Logical transform
// ------------------------------------------------------------------------------------------------

/** Wrapper function between HM interface and core NxN forward transform (2D) 
 *  \param piBlkResi input data (residual)
 *  \param psCoeff output data (transform coefficients)
 *  \param uiStride stride of input residual data
 *  \param iSize transform size (iSize x iSize)
 *  \param uiMode is Intra Prediction mode used in Mode-Dependent DCT/DST only
 */
Void TComTrQuant::xT( UInt uiMode, Pel* piBlkResi, UInt uiStride, Int* psCoeff, Int iWidth, Int iHeight )
{
#if MATRIX_MULT  
  Int iSize = iWidth;
#if !REMOVE_NSQT
  if( iWidth != iHeight)
  {
    xTrMxN( piBlkResi, psCoeff, uiStride, (UInt)iWidth, (UInt)iHeight );
    return;
  }
#endif
  xTr(piBlkResi,psCoeff,uiStride,(UInt)iSize,uiMode);
#else
  Int j;
  {
    short block[ 64 * 64 ];
    short coeff[ 64 * 64 ];
    {
      for (j = 0; j < iHeight; j++)
      {    
        memcpy( block + j * iWidth, piBlkResi + j * uiStride, iWidth * sizeof( short ) );      
      }
    }
    xTrMxN( block, coeff, iWidth, iHeight, uiMode );
    for ( j = 0; j < iHeight * iWidth; j++ )
    {    
      psCoeff[ j ] = coeff[ j ];
    }
    return ;
  }
#endif  
}


/** Wrapper function between HM interface and core NxN inverse transform (2D) 
 *  \param plCoef input data (transform coefficients)
 *  \param pResidual output data (residual)
 *  \param uiStride stride of input residual data
 *  \param iSize transform size (iSize x iSize)
 *  \param uiMode is Intra Prediction mode used in Mode-Dependent DCT/DST only
 */
Void TComTrQuant::xIT( UInt uiMode, Int* plCoef, Pel* pResidual, UInt uiStride, Int iWidth, Int iHeight )
{
#if MATRIX_MULT  
  Int iSize = iWidth;
#if !REMOVE_NSQT
  if( iWidth != iHeight )
  {
    xITrMxN( plCoef, pResidual, uiStride, (UInt)iWidth, (UInt)iHeight );
    return;
  }
#endif
  xITr(plCoef,pResidual,uiStride,(UInt)iSize,uiMode);
#else
  Int j;
  {
    short block[ 64 * 64 ];
    short coeff[ 64 * 64 ];
    for ( j = 0; j < iHeight * iWidth; j++ )
    {    
      coeff[j] = (short)plCoef[j];
    }
    xITrMxN( coeff, block, iWidth, iHeight, uiMode );
    {
      for ( j = 0; j < iHeight; j++ )
      {    
        memcpy( pResidual + j * uiStride, block + j * iWidth, iWidth * sizeof(short) );      
      }
    }
    return ;
  }
#endif  
}
 
/** Wrapper function between HM interface and core 4x4 transform skipping
 *  \param piBlkResi input data (residual)
 *  \param psCoeff output data (transform coefficients)
 *  \param uiStride stride of input residual data
 *  \param iSize transform size (iSize x iSize)
 */
Void TComTrQuant::xTransformSkip( Pel* piBlkResi, UInt uiStride, Int* psCoeff, Int width, Int height )
{
  assert( width == height );
  UInt uiLog2TrSize = g_aucConvertToBit[ width ] + 2;
#if FULL_NBIT
  UInt uiBitDepth = g_uiBitDepth;
#else
  UInt uiBitDepth = g_uiBitDepth + g_uiBitIncrement;
#endif
  Int  shift = MAX_TR_DYNAMIC_RANGE - uiBitDepth - uiLog2TrSize;
  UInt transformSkipShift;
  Int  j,k;
  if(shift >= 0)
  {
    transformSkipShift = shift;
    for (j = 0; j < height; j++)
    {    
      for(k = 0; k < width; k ++)
      {
        psCoeff[j*height + k] = piBlkResi[j * uiStride + k] << transformSkipShift;      
      }
    }
  }
  else
  {
    //The case when uiBitDepth > 13
    Int offset;
    transformSkipShift = -shift;
    offset = (1 << (transformSkipShift - 1));
    for (j = 0; j < height; j++)
    {    
      for(k = 0; k < width; k ++)
      {
        psCoeff[j*height + k] = (piBlkResi[j * uiStride + k] + offset) >> transformSkipShift;      
      }
    }
  }
}

/** Wrapper function between HM interface and core NxN transform skipping 
 *  \param plCoef input data (coefficients)
 *  \param pResidual output data (residual)
 *  \param uiStride stride of input residual data
 *  \param iSize transform size (iSize x iSize)
 */
Void TComTrQuant::xITransformSkip( Int* plCoef, Pel* pResidual, UInt uiStride, Int width, Int height )
{
  assert( width == height );
  UInt uiLog2TrSize = g_aucConvertToBit[ width ] + 2;
#if FULL_NBIT
  UInt uiBitDepth = g_uiBitDepth;
#else
  UInt uiBitDepth = g_uiBitDepth + g_uiBitIncrement;
#endif
  Int  shift = MAX_TR_DYNAMIC_RANGE - uiBitDepth - uiLog2TrSize; 
  UInt transformSkipShift; 
  Int  j,k;
  if(shift > 0)
  {
    Int offset;
    transformSkipShift = shift;
    offset = (1 << (transformSkipShift -1));
    for ( j = 0; j < height; j++ )
    {    
      for(k = 0; k < width; k ++)
      {
        pResidual[j * uiStride + k] =  (plCoef[j*width+k] + offset) >> transformSkipShift;
      } 
    }
  }
  else
  {
    //The case when uiBitDepth >= 13
    transformSkipShift = - shift;
    for ( j = 0; j < height; j++ )
    {    
      for(k = 0; k < width; k ++)
      {
        pResidual[j * uiStride + k] =  plCoef[j*width+k] << transformSkipShift;
      }
    }
  }
}

/** RDOQ with CABAC
 * \param pcCU pointer to coding unit structure
 * \param plSrcCoeff pointer to input buffer
 * \param piDstCoeff reference to pointer to output buffer
 * \param uiWidth block width
 * \param uiHeight block height
 * \param uiAbsSum reference to absolute sum of quantized transform coefficient
 * \param eTType plane type / luminance or chrominance
 * \param uiAbsPartIdx absolute partition index
 * \returns Void
 * Rate distortion optimized quantization for entropy
 * coding engines using probability models like CABAC
 */
Void TComTrQuant::xRateDistOptQuant                 ( TComDataCU*                     pcCU,
                                                      Int*                            plSrcCoeff,
                                                      TCoeff*                         piDstCoeff,
#if ADAPTIVE_QP_SELECTION
                                                      Int*&                           piArlDstCoeff,
#endif
                                                      UInt                            uiWidth,
                                                      UInt                            uiHeight,
                                                      UInt&                           uiAbsSum,
                                                      TextType                        eTType,
                                                      UInt                            uiAbsPartIdx )
{
  Int    iQBits      = m_cQP.m_iBits;
  Double dTemp       = 0;
  UInt dir         = SCALING_LIST_SQT;
  UInt uiLog2TrSize = g_aucConvertToBit[ uiWidth ] + 2;
  Int uiQ = g_quantScales[m_cQP.rem()];
#if !REMOVE_NSQT
  if (uiWidth != uiHeight)
  {
    uiLog2TrSize += (uiWidth > uiHeight) ? -1 : 1;
    dir            = ( uiWidth < uiHeight )?  SCALING_LIST_VER: SCALING_LIST_HOR;
  }
#endif
  
#if FULL_NBIT
  UInt uiBitDepth = g_uiBitDepth;
#else
  UInt uiBitDepth = g_uiBitDepth + g_uiBitIncrement;  
#endif
  Int iTransformShift = MAX_TR_DYNAMIC_RANGE - uiBitDepth - uiLog2TrSize;  // Represents scaling through forward transform
  UInt       uiGoRiceParam       = 0;
  Double     d64BlockUncodedCost = 0;
  const UInt uiLog2BlkSize       = g_aucConvertToBit[ uiWidth ] + 2;
  const UInt uiMaxNumCoeff       = uiWidth * uiHeight;
  Int scalingListType = (pcCU->isIntra(uiAbsPartIdx) ? 0 : 3) + g_eTTable[(Int)eTType];
  assert(scalingListType < 6);
  
  iQBits = QUANT_SHIFT + m_cQP.m_iPer + iTransformShift;                   // Right shift of non-RDOQ quantizer;  level = (coeff*uiQ + offset)>>q_bits
  double dErrScale   = 0;
  double *pdErrScaleOrg = getErrScaleCoeff(scalingListType,uiLog2TrSize-2,m_cQP.m_iRem,dir);
  Int *piQCoefOrg = getQuantCoeff(scalingListType,m_cQP.m_iRem,uiLog2TrSize-2,dir);
  Int *piQCoef = piQCoefOrg;
  double *pdErrScale = pdErrScaleOrg;
#if ADAPTIVE_QP_SELECTION
  Int iQBitsC = iQBits - ARL_C_PRECISION;
  Int iAddC =  1 << (iQBitsC-1);
#endif
  UInt uiScanIdx = pcCU->getCoefScanIdx(uiAbsPartIdx, uiWidth, eTType==TEXT_LUMA, pcCU->isIntra(uiAbsPartIdx));
  if (uiScanIdx == SCAN_ZIGZAG)
  {
    // Map value zigzag to diagonal scan
    uiScanIdx = SCAN_DIAG;
  }
  Int blockType = uiLog2BlkSize;
#if !REMOVE_NSQT
  if (uiWidth != uiHeight)
  {
    uiScanIdx = SCAN_DIAG;
    blockType = 4;
  }
#endif
  
#if ADAPTIVE_QP_SELECTION
  memset(piArlDstCoeff, 0, sizeof(Int) *  uiMaxNumCoeff);
#endif
  
  Double pdCostCoeff [ 32 * 32 ];
  Double pdCostSig   [ 32 * 32 ];
  Double pdCostCoeff0[ 32 * 32 ];
  ::memset( pdCostCoeff, 0, sizeof(Double) *  uiMaxNumCoeff );
  ::memset( pdCostSig,   0, sizeof(Double) *  uiMaxNumCoeff );
  Int rateIncUp   [ 32 * 32 ];
  Int rateIncDown [ 32 * 32 ];
  Int sigRateDelta[ 32 * 32 ];
  Int deltaU      [ 32 * 32 ];
  ::memset( rateIncUp,    0, sizeof(Int) *  uiMaxNumCoeff );
  ::memset( rateIncDown,  0, sizeof(Int) *  uiMaxNumCoeff );
  ::memset( sigRateDelta, 0, sizeof(Int) *  uiMaxNumCoeff );
  ::memset( deltaU,       0, sizeof(Int) *  uiMaxNumCoeff );
  
  const UInt * scanCG;
#if !REMOVE_NSQT
  if (uiWidth == uiHeight)
#endif
  {
    scanCG = g_auiSigLastScan[ uiScanIdx ][ uiLog2BlkSize > 3 ? uiLog2BlkSize-2-1 : 0  ];
    if( uiLog2BlkSize == 3 )
    {
      scanCG = g_sigLastScan8x8[ uiScanIdx ];
    }
    else if( uiLog2BlkSize == 5 )
    {
      scanCG = g_sigLastScanCG32x32;
    }
  }
#if !REMOVE_NSQT
  else
  {
    scanCG = g_sigCGScanNSQT[ uiLog2BlkSize - 2 ];
  }
#endif
  const UInt uiCGSize = (1 << MLS_CG_SIZE);         // 16
  Double pdCostCoeffGroupSig[ MLS_GRP_NUM ];
  UInt uiSigCoeffGroupFlag[ MLS_GRP_NUM ];
  UInt uiNumBlkSide = uiWidth / MLS_CG_SIZE;
  Int iCGLastScanPos = -1;
  
  UInt    uiCtxSet            = 0;
  Int     c1                  = 1;
  Int     c2                  = 0;
#if !REMOVE_NUM_GREATER1
  UInt    uiNumOne            = 0;
#endif
  Double  d64BaseCost         = 0;
  Int     iLastScanPos        = -1;
  dTemp                       = dErrScale;
  
  UInt    c1Idx     = 0;
  UInt    c2Idx     = 0;
  Int     baseLevel;
  
#if REMOVE_NSQT
  const UInt *scan = g_auiSigLastScan[ uiScanIdx ][ uiLog2BlkSize - 1 ];
#else
  const UInt * scan;
  if (uiWidth == uiHeight)
  {
    scan = g_auiSigLastScan[ uiScanIdx ][ uiLog2BlkSize - 1 ];    
  }
  else
  {
    scan = g_sigScanNSQT[ uiLog2BlkSize - 2 ];
  }
#endif
  
  ::memset( pdCostCoeffGroupSig,   0, sizeof(Double) * MLS_GRP_NUM );
  ::memset( uiSigCoeffGroupFlag,   0, sizeof(UInt) * MLS_GRP_NUM );
  
  UInt uiCGNum = uiWidth * uiHeight >> MLS_CG_SIZE;
  Int iScanPos;
  coeffGroupRDStats rdStats;     
  
  for (Int iCGScanPos = uiCGNum-1; iCGScanPos >= 0; iCGScanPos--)
  {
    UInt uiCGBlkPos = scanCG[ iCGScanPos ];
    UInt uiCGPosY   = uiCGBlkPos / uiNumBlkSide;
    UInt uiCGPosX   = uiCGBlkPos - (uiCGPosY * uiNumBlkSide);
#if !REMOVAL_8x2_2x8_CG
    if( uiWidth == 8 && uiHeight == 8 && (uiScanIdx == SCAN_HOR || uiScanIdx == SCAN_VER) )
    {
      uiCGPosY = (uiScanIdx == SCAN_HOR ? uiCGBlkPos : 0);
      uiCGPosX = (uiScanIdx == SCAN_VER ? uiCGBlkPos : 0);
    }
#endif
    ::memset( &rdStats, 0, sizeof (coeffGroupRDStats));
    
    const Int patternSigCtx = TComTrQuant::calcPatternSigCtx(uiSigCoeffGroupFlag, uiCGPosX, uiCGPosY, uiWidth, uiHeight);
    for (Int iScanPosinCG = uiCGSize-1; iScanPosinCG >= 0; iScanPosinCG--)
    {
      iScanPos = iCGScanPos*uiCGSize + iScanPosinCG;
      //===== quantization =====
      UInt    uiBlkPos          = scan[iScanPos];
      // set coeff
      uiQ  = piQCoef[uiBlkPos];
      dTemp = pdErrScale[uiBlkPos];
      Int lLevelDouble          = plSrcCoeff[ uiBlkPos ];
      lLevelDouble              = (Int)min<Int64>((Int64)abs((Int)lLevelDouble) * uiQ , MAX_INT - (1 << (iQBits - 1)));
#if ADAPTIVE_QP_SELECTION
      if( m_bUseAdaptQpSelect )
      {
        piArlDstCoeff[uiBlkPos]   = (Int)(( lLevelDouble + iAddC) >> iQBitsC );
      }
#endif
      UInt uiMaxAbsLevel        = (lLevelDouble + (1 << (iQBits - 1))) >> iQBits;
      
      Double dErr               = Double( lLevelDouble );
      pdCostCoeff0[ iScanPos ]  = dErr * dErr * dTemp;
      d64BlockUncodedCost      += pdCostCoeff0[ iScanPos ];
      piDstCoeff[ uiBlkPos ]    = uiMaxAbsLevel;
      
      if ( uiMaxAbsLevel > 0 && iLastScanPos < 0 )
      {
        iLastScanPos            = iScanPos;
        uiCtxSet                = (iScanPos < SCAN_SET_SIZE || eTType!=TEXT_LUMA) ? 0 : 2;
        iCGLastScanPos          = iCGScanPos;
      }
      
      if ( iLastScanPos >= 0 )
      {
        //===== coefficient level estimation =====
        UInt  uiLevel;
        UInt  uiOneCtx         = 4 * uiCtxSet + c1;
        UInt  uiAbsCtx         = uiCtxSet + c2;
        
        if( iScanPos == iLastScanPos )
        {
          uiLevel              = xGetCodedLevel( pdCostCoeff[ iScanPos ], pdCostCoeff0[ iScanPos ], pdCostSig[ iScanPos ], 
                                                lLevelDouble, uiMaxAbsLevel, 0, uiOneCtx, uiAbsCtx, uiGoRiceParam, 
                                                c1Idx, c2Idx, iQBits, dTemp, 1 );
        }
        else
        {
          UInt   uiPosY        = uiBlkPos >> uiLog2BlkSize;
          UInt   uiPosX        = uiBlkPos - ( uiPosY << uiLog2BlkSize );
#if REMOVAL_8x2_2x8_CG
          UShort uiCtxSig      = getSigCtxInc( patternSigCtx, uiScanIdx, uiPosX, uiPosY, blockType, uiWidth, uiHeight, eTType );
#else
          UShort uiCtxSig      = getSigCtxInc( patternSigCtx, uiPosX, uiPosY, blockType, uiWidth, uiHeight, eTType );
#endif
          uiLevel              = xGetCodedLevel( pdCostCoeff[ iScanPos ], pdCostCoeff0[ iScanPos ], pdCostSig[ iScanPos ],
                                                lLevelDouble, uiMaxAbsLevel, uiCtxSig, uiOneCtx, uiAbsCtx, uiGoRiceParam, 
                                                c1Idx, c2Idx, iQBits, dTemp, 0 );
          sigRateDelta[ uiBlkPos ] = m_pcEstBitsSbac->significantBits[ uiCtxSig ][ 1 ] - m_pcEstBitsSbac->significantBits[ uiCtxSig ][ 0 ];
        }
        deltaU[ uiBlkPos ]        = (lLevelDouble - ((Int)uiLevel << iQBits)) >> (iQBits-8);
        if( uiLevel > 0 )
        {
          Int rateNow = xGetICRate( uiLevel, uiOneCtx, uiAbsCtx, uiGoRiceParam, c1Idx, c2Idx );
          rateIncUp   [ uiBlkPos ] = xGetICRate( uiLevel+1, uiOneCtx, uiAbsCtx, uiGoRiceParam, c1Idx, c2Idx ) - rateNow;
          rateIncDown [ uiBlkPos ] = xGetICRate( uiLevel-1, uiOneCtx, uiAbsCtx, uiGoRiceParam, c1Idx, c2Idx ) - rateNow;
        }
        else // uiLevel == 0
        {
          rateIncUp   [ uiBlkPos ] = m_pcEstBitsSbac->m_greaterOneBits[ uiOneCtx ][ 0 ];
        }
        piDstCoeff[ uiBlkPos ] = uiLevel;
        d64BaseCost           += pdCostCoeff [ iScanPos ];
        
        
        baseLevel = (c1Idx < C1FLAG_NUMBER) ? (2 + (c2Idx < C2FLAG_NUMBER)) : 1;
        if( uiLevel >= baseLevel )
        {
          if(uiLevel  > 3*(1<<uiGoRiceParam))
          {
            uiGoRiceParam = min<UInt>(uiGoRiceParam+ 1, 4);
          }
        }
        if ( uiLevel >= 1)
        {
          c1Idx ++;
        }
        
        //===== update bin model =====
        if( uiLevel > 1 )
        {
          c1 = 0; 
          c2 += (c2 < 2);
#if !REMOVE_NUM_GREATER1
          uiNumOne++;
#endif
          c2Idx ++;
        }
        else if( (c1 < 3) && (c1 > 0) && uiLevel)
        {
          c1++;
        }
        
        //===== context set update =====
        if( ( iScanPos % SCAN_SET_SIZE == 0 ) && ( iScanPos > 0 ) )
        {
#if !REMOVE_NUM_GREATER1
          c1                = 1;
#endif
          c2                = 0;
          uiGoRiceParam     = 0;
          
          c1Idx   = 0;
          c2Idx   = 0; 
          uiCtxSet          = (iScanPos == SCAN_SET_SIZE || eTType!=TEXT_LUMA) ? 0 : 2;
#if REMOVE_NUM_GREATER1
          if( c1 == 0 )
#else
          if( uiNumOne > 0 )
#endif
          {
            uiCtxSet++;
          }
#if REMOVE_NUM_GREATER1
          c1 = 1;
#else
          uiNumOne    >>= 1;
#endif
        }
      }
      else
      {
        d64BaseCost    += pdCostCoeff0[ iScanPos ];
      }
      rdStats.d64SigCost += pdCostSig[ iScanPos ];
      if (iScanPosinCG == 0 )
      {
        rdStats.d64SigCost_0 = pdCostSig[ iScanPos ];
      }
      if (piDstCoeff[ uiBlkPos ] )
      {
        uiSigCoeffGroupFlag[ uiCGBlkPos ] = 1;
        rdStats.d64CodedLevelandDist += pdCostCoeff[ iScanPos ] - pdCostSig[ iScanPos ];
        rdStats.d64UncodedDist += pdCostCoeff0[ iScanPos ];
        if ( iScanPosinCG != 0 )
        {
          rdStats.iNNZbeforePos0++;
        }
      }
    } //end for (iScanPosinCG)
    
    if (iCGLastScanPos >= 0) 
    {
      if( iCGScanPos )
      {
        if (uiSigCoeffGroupFlag[ uiCGBlkPos ] == 0)
        {
          UInt  uiCtxSig = getSigCoeffGroupCtxInc( uiSigCoeffGroupFlag, uiCGPosX, uiCGPosY, uiScanIdx, uiWidth, uiHeight);
          d64BaseCost += xGetRateSigCoeffGroup(0, uiCtxSig) - rdStats.d64SigCost;;  
          pdCostCoeffGroupSig[ iCGScanPos ] = xGetRateSigCoeffGroup(0, uiCtxSig);  
        } 
        else
        {
          if (iCGScanPos < iCGLastScanPos) //skip the last coefficient group, which will be handled together with last position below.
          {
            if ( rdStats.iNNZbeforePos0 == 0 ) 
            {
              d64BaseCost -= rdStats.d64SigCost_0;
              rdStats.d64SigCost -= rdStats.d64SigCost_0;
            }
            // rd-cost if SigCoeffGroupFlag = 0, initialization
            Double d64CostZeroCG = d64BaseCost;
            
            // add SigCoeffGroupFlag cost to total cost
            UInt  uiCtxSig = getSigCoeffGroupCtxInc( uiSigCoeffGroupFlag, uiCGPosX, uiCGPosY, uiScanIdx, uiWidth, uiHeight);
            if (iCGScanPos < iCGLastScanPos)
            {
              d64BaseCost  += xGetRateSigCoeffGroup(1, uiCtxSig); 
              d64CostZeroCG += xGetRateSigCoeffGroup(0, uiCtxSig);  
              pdCostCoeffGroupSig[ iCGScanPos ] = xGetRateSigCoeffGroup(1, uiCtxSig); 
            }
            
            // try to convert the current coeff group from non-zero to all-zero
            d64CostZeroCG += rdStats.d64UncodedDist;  // distortion for resetting non-zero levels to zero levels
            d64CostZeroCG -= rdStats.d64CodedLevelandDist;   // distortion and level cost for keeping all non-zero levels
            d64CostZeroCG -= rdStats.d64SigCost;     // sig cost for all coeffs, including zero levels and non-zerl levels
            
            // if we can save cost, change this block to all-zero block
            if ( d64CostZeroCG < d64BaseCost )      
            {
              uiSigCoeffGroupFlag[ uiCGBlkPos ] = 0;
              d64BaseCost = d64CostZeroCG;
              if (iCGScanPos < iCGLastScanPos)
              {
                pdCostCoeffGroupSig[ iCGScanPos ] = xGetRateSigCoeffGroup(0, uiCtxSig); 
              }
              // reset coeffs to 0 in this block                
              for (Int iScanPosinCG = uiCGSize-1; iScanPosinCG >= 0; iScanPosinCG--)
              {
                iScanPos      = iCGScanPos*uiCGSize + iScanPosinCG;
                UInt uiBlkPos = scan[ iScanPos ];
                
                if (piDstCoeff[ uiBlkPos ])
                {
                  piDstCoeff [ uiBlkPos ] = 0;
                  pdCostCoeff[ iScanPos ] = pdCostCoeff0[ iScanPos ];
                  pdCostSig  [ iScanPos ] = 0;
                }
              }
            } // end if ( d64CostAllZeros < d64BaseCost )      
          }
        } // end if if (uiSigCoeffGroupFlag[ uiCGBlkPos ] == 0)
      }
      else
      {
        uiSigCoeffGroupFlag[ uiCGBlkPos ] = 1;
      }
    }
  } //end for (iCGScanPos)
  
  //===== estimate last position =====
  if ( iLastScanPos < 0 )
  {
    return;
  }
  
  Double  d64BestCost         = 0;
  Int     ui16CtxCbf          = 0;
  Int     iBestLastIdxP1      = 0;
  if( !pcCU->isIntra( uiAbsPartIdx ) && eTType == TEXT_LUMA && pcCU->getTransformIdx( uiAbsPartIdx ) == 0 )
  {
    ui16CtxCbf   = 0;
    d64BestCost  = d64BlockUncodedCost + xGetICost( m_pcEstBitsSbac->blockRootCbpBits[ ui16CtxCbf ][ 0 ] );
    d64BaseCost += xGetICost( m_pcEstBitsSbac->blockRootCbpBits[ ui16CtxCbf ][ 1 ] );
  }
  else
  {
    ui16CtxCbf   = pcCU->getCtxQtCbf( uiAbsPartIdx, eTType, pcCU->getTransformIdx( uiAbsPartIdx ) );
    ui16CtxCbf   = ( eTType ? TEXT_CHROMA : eTType ) * NUM_QT_CBF_CTX + ui16CtxCbf;
    d64BestCost  = d64BlockUncodedCost + xGetICost( m_pcEstBitsSbac->blockCbpBits[ ui16CtxCbf ][ 0 ] );
    d64BaseCost += xGetICost( m_pcEstBitsSbac->blockCbpBits[ ui16CtxCbf ][ 1 ] );
  }
  
  Bool bFoundLast = false;
  for (Int iCGScanPos = iCGLastScanPos; iCGScanPos >= 0; iCGScanPos--)
  {
    UInt uiCGBlkPos = scanCG[ iCGScanPos ];
    
    d64BaseCost -= pdCostCoeffGroupSig [ iCGScanPos ]; 
    if (uiSigCoeffGroupFlag[ uiCGBlkPos ])
    {     
      for (Int iScanPosinCG = uiCGSize-1; iScanPosinCG >= 0; iScanPosinCG--)
      {
        iScanPos = iCGScanPos*uiCGSize + iScanPosinCG;
        if (iScanPos > iLastScanPos) continue;
        UInt   uiBlkPos     = scan[iScanPos];
        
        if( piDstCoeff[ uiBlkPos ] )
        {
          UInt   uiPosY       = uiBlkPos >> uiLog2BlkSize;
          UInt   uiPosX       = uiBlkPos - ( uiPosY << uiLog2BlkSize );
          
          Double d64CostLast= uiScanIdx == SCAN_VER ? xGetRateLast( uiPosY, uiPosX, uiWidth ) : xGetRateLast( uiPosX, uiPosY, uiWidth );
          Double totalCost = d64BaseCost + d64CostLast - pdCostSig[ iScanPos ];
          
          if( totalCost < d64BestCost )
          {
            iBestLastIdxP1  = iScanPos + 1;
            d64BestCost     = totalCost;
          }
          if( piDstCoeff[ uiBlkPos ] > 1 )
          {
            bFoundLast = true;
            break;
          }
          d64BaseCost      -= pdCostCoeff[ iScanPos ];
          d64BaseCost      += pdCostCoeff0[ iScanPos ];
        }
        else
        {
          d64BaseCost      -= pdCostSig[ iScanPos ];
        }
      } //end for 
      if (bFoundLast)
      {
        break;
      }
    } // end if (uiSigCoeffGroupFlag[ uiCGBlkPos ])
  } // end for 
  
  for ( Int scanPos = 0; scanPos < iBestLastIdxP1; scanPos++ )
  {
    Int blkPos = scan[ scanPos ];
    Int level  = piDstCoeff[ blkPos ];
    uiAbsSum += level;
    piDstCoeff[ blkPos ] = ( plSrcCoeff[ blkPos ] < 0 ) ? -level : level;
  }
  
  //===== clean uncoded coefficients =====
  for ( Int scanPos = iBestLastIdxP1; scanPos <= iLastScanPos; scanPos++ )
  {
    piDstCoeff[ scan[ scanPos ] ] = 0;
  }
  
  if( pcCU->getSlice()->getPPS()->getSignHideFlag() && uiAbsSum>=2)
  {
    Int64 rdFactor = (Int64)((Double)(g_invQuantScales[m_cQP.rem()])*(Double)(g_invQuantScales[m_cQP.rem()])*(Double)(1<<(2*m_cQP.m_iPer))/m_dLambda/16/(Double)(1<<(2*g_uiBitIncrement)) + 0.5);
    Int lastCG = -1;
    Int absSum = 0 ;
    Int n ;
    
    for( Int subSet = (uiWidth*uiHeight-1) >> LOG2_SCAN_SET_SIZE; subSet >= 0; subSet-- )
    {
      Int  subPos     = subSet << LOG2_SCAN_SET_SIZE;
      Int  firstNZPosInCG=SCAN_SET_SIZE , lastNZPosInCG=-1 ;
      absSum = 0 ;
      
      for(n = SCAN_SET_SIZE-1; n >= 0; --n )
      {
        if( piDstCoeff[ scan[ n + subPos ]] )
        {
          lastNZPosInCG = n;
          break;
        }
      }
      
      for(n = 0; n <SCAN_SET_SIZE; n++ )
      {
        if( piDstCoeff[ scan[ n + subPos ]] )
        {
          firstNZPosInCG = n;
          break;
        }
      }
      
      for(n = firstNZPosInCG; n <=lastNZPosInCG; n++ )
      {
        absSum += piDstCoeff[ scan[ n + subPos ]];
      }
      
      if(lastNZPosInCG>=0 && lastCG==-1)
      {
        lastCG = 1; 
      } 
      
      if( lastNZPosInCG-firstNZPosInCG>=SBH_THRESHOLD )
      {
        UInt signbit = (piDstCoeff[scan[subPos+firstNZPosInCG]]>0?0:1);
        if( signbit!=(absSum&0x1) )  // hide but need tune
        {
          // calculate the cost 
          Int64 minCostInc = MAX_INT64, curCost=MAX_INT64;
          Int minPos =-1, finalChange=0, curChange=0;
          
          for( n = (lastCG==1?lastNZPosInCG:SCAN_SET_SIZE-1) ; n >= 0; --n )
          {
            UInt uiBlkPos   = scan[ n + subPos ];
            if(piDstCoeff[ uiBlkPos ] != 0 )
            {
              Int64 costUp   = rdFactor * ( - deltaU[uiBlkPos] ) + rateIncUp[uiBlkPos] ;
              Int64 costDown = rdFactor * (   deltaU[uiBlkPos] ) + rateIncDown[uiBlkPos] 
              -   ( abs(piDstCoeff[uiBlkPos])==1?((1<<15)+sigRateDelta[uiBlkPos]):0 );
              
              if(lastCG==1 && lastNZPosInCG==n && abs(piDstCoeff[uiBlkPos])==1)
              {
                costDown -= (4<<15) ;
              }
              
              if(costUp<costDown)
              {  
                curCost = costUp;
                curChange =  1 ;
              }
              else               
              {
                curChange = -1 ;
                if(n==firstNZPosInCG && abs(piDstCoeff[uiBlkPos])==1)
                {
                  curCost = MAX_INT64 ;
                }
                else
                {
                  curCost = costDown ; 
                }
              }
            }
            else
            {
              curCost = rdFactor * ( - (abs(deltaU[uiBlkPos])) ) + (1<<15) + rateIncUp[uiBlkPos] + sigRateDelta[uiBlkPos] ; 
              curChange = 1 ;
              
              if(n<firstNZPosInCG)
              {
                UInt thissignbit = (plSrcCoeff[uiBlkPos]>=0?0:1);
                if(thissignbit != signbit )
                {
                  curCost = MAX_INT64;
                }
              }
            }
            
            if( curCost<minCostInc)
            {
              minCostInc = curCost ;
              finalChange = curChange ;
              minPos = uiBlkPos ;
            }
          }
          
          if(piQCoef[minPos] == 32767 || piQCoef[minPos] == -32768)
          {
            finalChange = -1;
          }
          
          if(plSrcCoeff[minPos]>=0)
          {
            piDstCoeff[minPos] += finalChange ;
          }
          else
          {
            piDstCoeff[minPos] -= finalChange ; 
          }          
        }
      }
      
      if(lastCG==1)
      {
        lastCG=0 ;  
      }
    }
  }
}

/** Pattern decision for context derivation process of significant_coeff_flag
 * \param sigCoeffGroupFlag pointer to prior coded significant coeff group
 * \param posXCG column of current coefficient group
 * \param posYCG row of current coefficient group
 * \param width width of the block
 * \param height height of the block
 * \returns pattern for current coefficient group
 */
Int  TComTrQuant::calcPatternSigCtx( const UInt* sigCoeffGroupFlag, UInt posXCG, UInt posYCG, Int width, Int height )
{
#if REMOVAL_8x2_2x8_CG
  if( width == 4 && height == 4 ) return -1;
#else
  if( width == height && width <= 8 ) return -1;
#endif

  UInt sigRight = 0;
  UInt sigLower = 0;

  width >>= 2;
  height >>= 2;
  if( posXCG < width - 1 )
  {
    sigRight = (sigCoeffGroupFlag[ posYCG * width + posXCG + 1 ] != 0);
  }
  if (posYCG < height - 1 )
  {
    sigLower = (sigCoeffGroupFlag[ (posYCG  + 1 ) * width + posXCG ] != 0);
  }
  return sigRight + (sigLower<<1);
}

/** Context derivation process of coeff_abs_significant_flag
 * \param patternSigCtx pattern for current coefficient group
 * \param posX column of current scan position
 * \param posY row of current scan position
 * \param blockType log2 value of block size if square block, or 4 otherwise
 * \param width width of the block
 * \param height height of the block
 * \param textureType texture type (TEXT_LUMA...)
 * \returns ctxInc for current scan position
 */
Int TComTrQuant::getSigCtxInc    (
                                   Int                             patternSigCtx,
#if REMOVAL_8x2_2x8_CG
                                   UInt                            scanIdx,
#endif
                                   Int                             posX,
                                   Int                             posY,
                                   Int                             blockType,
                                   Int                             width
                                  ,Int                             height
                                  ,TextType                        textureType
                                  )
{
  const Int ctxIndMap[16] =
  {
    0, 1, 4, 5,
    2, 3, 4, 5,
    6, 6, 8, 8,
    7, 7, 8, 8
  };

  if( posX + posY == 0 )
  {
    return 0;
  }

  if ( blockType == 2 )
  {
    return ctxIndMap[ 4 * posY + posX ];
  }

#if !REMOVAL_8x2_2x8_CG
  if ( blockType == 3 )
  {
    return 9 + ctxIndMap[ 4 * (posY >> 1) + (posX >> 1) ];
  }

  Int offset = 18;
#else
  Int offset = blockType == 3 ? (scanIdx==SCAN_DIAG ? 9 : 15) : (textureType == TEXT_LUMA ? 21 : 12);
#endif

  Int posXinSubset = posX-((posX>>2)<<2);
  Int posYinSubset = posY-((posY>>2)<<2);
  Int cnt = 0;
  if(patternSigCtx==0)
  {
#if REMOVAL_8x2_2x8_CG
    cnt = posXinSubset+posYinSubset<=2 ? (posXinSubset+posYinSubset==0 ? 2 : 1) : 0;
#else
    cnt = posXinSubset+posYinSubset<=2 ? 1 : 0;
#endif
  }
  else if(patternSigCtx==1)
  {
#if REMOVAL_8x2_2x8_CG
    cnt = posYinSubset<=1 ? (posYinSubset==0 ? 2 : 1) : 0;
#else
    cnt = posYinSubset<=1 ? 1 : 0;
#endif
  }
  else if(patternSigCtx==2)
  {
#if REMOVAL_8x2_2x8_CG
    cnt = posXinSubset<=1 ? (posXinSubset==0 ? 2 : 1) : 0;
#else
    cnt = posXinSubset<=1 ? 1 : 0;
#endif
  }
  else
  {
#if REMOVAL_8x2_2x8_CG
    cnt = 2;
#else
    cnt = posXinSubset+posYinSubset<=4 ? 2 : 1;
#endif
  }

  return (( textureType == TEXT_LUMA && ((posX>>2) + (posY>>2)) > 0 ) ? 3 : 0) + offset + cnt;
}

/** Get the best level in RD sense
 * \param rd64CodedCost reference to coded cost
 * \param rd64CodedCost0 reference to cost when coefficient is 0
 * \param rd64CodedCostSig reference to cost of significant coefficient
 * \param lLevelDouble reference to unscaled quantized level
 * \param uiMaxAbsLevel scaled quantized level
 * \param ui16CtxNumSig current ctxInc for coeff_abs_significant_flag
 * \param ui16CtxNumOne current ctxInc for coeff_abs_level_greater1 (1st bin of coeff_abs_level_minus1 in AVC)
 * \param ui16CtxNumAbs current ctxInc for coeff_abs_level_greater2 (remaining bins of coeff_abs_level_minus1 in AVC)
 * \param ui16AbsGoRice current Rice parameter for coeff_abs_level_minus3
 * \param iQBits quantization step size
 * \param dTemp correction factor
 * \param bLast indicates if the coefficient is the last significant
 * \returns best quantized transform level for given scan position
 * This method calculates the best quantized transform level for a given scan position.
 */
__inline UInt TComTrQuant::xGetCodedLevel ( Double&                         rd64CodedCost,
                                            Double&                         rd64CodedCost0,
                                            Double&                         rd64CodedCostSig,
                                            Int                             lLevelDouble,
                                            UInt                            uiMaxAbsLevel,
                                            UShort                          ui16CtxNumSig,
                                            UShort                          ui16CtxNumOne,
                                            UShort                          ui16CtxNumAbs,
                                            UShort                          ui16AbsGoRice,
                                            UInt                            c1Idx,
                                            UInt                            c2Idx,
                                            Int                             iQBits,
                                            Double                          dTemp,
                                            Bool                            bLast        ) const
{
  Double dCurrCostSig   = 0; 
  UInt   uiBestAbsLevel = 0;
  
  if( !bLast && uiMaxAbsLevel < 3 )
  {
    rd64CodedCostSig    = xGetRateSigCoef( 0, ui16CtxNumSig ); 
    rd64CodedCost       = rd64CodedCost0 + rd64CodedCostSig;
    if( uiMaxAbsLevel == 0 )
    {
      return uiBestAbsLevel;
    }
  }
  else
  {
    rd64CodedCost       = MAX_DOUBLE;
  }

  if( !bLast )
  {
    dCurrCostSig        = xGetRateSigCoef( 1, ui16CtxNumSig );
  }

  UInt uiMinAbsLevel    = ( uiMaxAbsLevel > 1 ? uiMaxAbsLevel - 1 : 1 );
  for( Int uiAbsLevel  = uiMaxAbsLevel; uiAbsLevel >= uiMinAbsLevel ; uiAbsLevel-- )
  {
    Double dErr         = Double( lLevelDouble  - ( uiAbsLevel << iQBits ) );
    Double dCurrCost    = dErr * dErr * dTemp + xGetICRateCost( uiAbsLevel, ui16CtxNumOne, ui16CtxNumAbs, ui16AbsGoRice, c1Idx, c2Idx );
    dCurrCost          += dCurrCostSig;

    if( dCurrCost < rd64CodedCost )
    {
      uiBestAbsLevel    = uiAbsLevel;
      rd64CodedCost     = dCurrCost;
      rd64CodedCostSig  = dCurrCostSig;
    }
  }

  return uiBestAbsLevel;
}

/** Calculates the cost for specific absolute transform level
 * \param uiAbsLevel scaled quantized level
 * \param ui16CtxNumOne current ctxInc for coeff_abs_level_greater1 (1st bin of coeff_abs_level_minus1 in AVC)
 * \param ui16CtxNumAbs current ctxInc for coeff_abs_level_greater2 (remaining bins of coeff_abs_level_minus1 in AVC)
 * \param ui16AbsGoRice Rice parameter for coeff_abs_level_minus3
 * \returns cost of given absolute transform level
 */
__inline Double TComTrQuant::xGetICRateCost  ( UInt                            uiAbsLevel,
                                               UShort                          ui16CtxNumOne,
                                               UShort                          ui16CtxNumAbs,
                                               UShort                          ui16AbsGoRice
                                            ,  UInt                            c1Idx,
                                               UInt                            c2Idx
                                               ) const
{
  Double iRate = xGetIEPRate();
  UInt baseLevel  =  (c1Idx < C1FLAG_NUMBER)? (2 + (c2Idx < C2FLAG_NUMBER)) : 1;

  if ( uiAbsLevel >= baseLevel )
  {    
    UInt symbol     = uiAbsLevel - baseLevel;
    UInt length;
#if COEF_REMAIN_BIN_REDUCTION
    if (symbol < (COEF_REMAIN_BIN_REDUCTION << ui16AbsGoRice))
#else
    if (symbol < (8 << ui16AbsGoRice))
#endif
    {
      length = symbol>>ui16AbsGoRice;
      iRate += (length+1+ui16AbsGoRice)<< 15;
    }
    else
    {
      length = ui16AbsGoRice;
#if COEF_REMAIN_BIN_REDUCTION
      symbol  = symbol - ( COEF_REMAIN_BIN_REDUCTION << ui16AbsGoRice); 
#else
      symbol  = symbol - ( 8 << ui16AbsGoRice);    
#endif
      while (symbol >= (1<<length))
      {
        symbol -=  (1<<(length++));    
      }
#if COEF_REMAIN_BIN_REDUCTION
      iRate += (COEF_REMAIN_BIN_REDUCTION+length+1-ui16AbsGoRice+length)<< 15;
#else
      iRate += (8+length+1-ui16AbsGoRice+length)<< 15;
#endif
    }
    if (c1Idx < C1FLAG_NUMBER)
    {
      iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 1 ];

      if (c2Idx < C2FLAG_NUMBER)
      {
        iRate += m_pcEstBitsSbac->m_levelAbsBits[ ui16CtxNumAbs ][ 1 ];
      }
    }
  }
  else
  if( uiAbsLevel == 1 )
  {
    iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 0 ];
  }
  else if( uiAbsLevel == 2 )
  {
    iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 1 ];
    iRate += m_pcEstBitsSbac->m_levelAbsBits[ ui16CtxNumAbs ][ 0 ];
  }
  else
  {
    assert (0);
  }
  return xGetICost( iRate );
}

__inline Int TComTrQuant::xGetICRate  ( UInt                            uiAbsLevel,
                                       UShort                          ui16CtxNumOne,
                                       UShort                          ui16CtxNumAbs,
                                       UShort                          ui16AbsGoRice
                                     , UInt                            c1Idx,
                                       UInt                            c2Idx
                                       ) const
{
  Int iRate = 0;
  UInt baseLevel  =  (c1Idx < C1FLAG_NUMBER)? (2 + (c2Idx < C2FLAG_NUMBER)) : 1;

  if ( uiAbsLevel >= baseLevel )
  {
    UInt uiSymbol     = uiAbsLevel - baseLevel;
    UInt uiMaxVlc     = g_auiGoRiceRange[ ui16AbsGoRice ];
    Bool bExpGolomb   = ( uiSymbol > uiMaxVlc );

    if( bExpGolomb )
    {
      uiAbsLevel  = uiSymbol - uiMaxVlc;
      int iEGS    = 1;  for( UInt uiMax = 2; uiAbsLevel >= uiMax; uiMax <<= 1, iEGS += 2 );
      iRate      += iEGS << 15;
      uiSymbol    = min<UInt>( uiSymbol, ( uiMaxVlc + 1 ) );
    }

    UShort ui16PrefLen = UShort( uiSymbol >> ui16AbsGoRice ) + 1;
    UShort ui16NumBins = min<UInt>( ui16PrefLen, g_auiGoRicePrefixLen[ ui16AbsGoRice ] ) + ui16AbsGoRice;

    iRate += ui16NumBins << 15;

    if (c1Idx < C1FLAG_NUMBER)
    {
      iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 1 ];

      if (c2Idx < C2FLAG_NUMBER)
      {
        iRate += m_pcEstBitsSbac->m_levelAbsBits[ ui16CtxNumAbs ][ 1 ];
      }
    }
  }
  else
  if( uiAbsLevel == 0 )
  {
    return 0;
  }
  else if( uiAbsLevel == 1 )
  {
    iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 0 ];
  }
  else if( uiAbsLevel == 2 )
  {
    iRate += m_pcEstBitsSbac->m_greaterOneBits[ ui16CtxNumOne ][ 1 ];
    iRate += m_pcEstBitsSbac->m_levelAbsBits[ ui16CtxNumAbs ][ 0 ];
  }
  else
  {
    assert(0);
  }
  return iRate;
}

__inline Double TComTrQuant::xGetRateSigCoeffGroup  ( UShort                    uiSignificanceCoeffGroup,
                                                UShort                          ui16CtxNumSig ) const
{
  return xGetICost( m_pcEstBitsSbac->significantCoeffGroupBits[ ui16CtxNumSig ][ uiSignificanceCoeffGroup ] );
}

/** Calculates the cost of signaling the last significant coefficient in the block
 * \param uiPosX X coordinate of the last significant coefficient
 * \param uiPosY Y coordinate of the last significant coefficient
 * \returns cost of last significant coefficient
 */
/*
 * \param uiWidth width of the transform unit (TU)
*/
__inline Double TComTrQuant::xGetRateLast   ( const UInt                      uiPosX,
                                              const UInt                      uiPosY,
                                              const UInt                      uiBlkWdth     ) const
{
  UInt uiCtxX   = g_uiGroupIdx[uiPosX];
  UInt uiCtxY   = g_uiGroupIdx[uiPosY];
  Double uiCost = m_pcEstBitsSbac->lastXBits[ uiCtxX ] + m_pcEstBitsSbac->lastYBits[ uiCtxY ];
  if( uiCtxX > 3 )
  {
    uiCost += xGetIEPRate() * ((uiCtxX-2)>>1);
  }
  if( uiCtxY > 3 )
  {
    uiCost += xGetIEPRate() * ((uiCtxY-2)>>1);
  }
  return xGetICost( uiCost );
}

 /** Calculates the cost for specific absolute transform level
 * \param uiAbsLevel scaled quantized level
 * \param ui16CtxNumOne current ctxInc for coeff_abs_level_greater1 (1st bin of coeff_abs_level_minus1 in AVC)
 * \param ui16CtxNumAbs current ctxInc for coeff_abs_level_greater2 (remaining bins of coeff_abs_level_minus1 in AVC)
 * \param ui16CtxBase current global offset for coeff_abs_level_greater1 and coeff_abs_level_greater2
 * \returns cost of given absolute transform level
 */
__inline Double TComTrQuant::xGetRateSigCoef  ( UShort                          uiSignificance,
                                                UShort                          ui16CtxNumSig ) const
{
  return xGetICost( m_pcEstBitsSbac->significantBits[ ui16CtxNumSig ][ uiSignificance ] );
}

/** Get the cost for a specific rate
 * \param dRate rate of a bit
 * \returns cost at the specific rate
 */
__inline Double TComTrQuant::xGetICost        ( Double                          dRate         ) const
{
  return m_dLambda * dRate;
}

/** Get the cost of an equal probable bit
 * \returns cost of equal probable bit
 */
__inline Double TComTrQuant::xGetIEPRate      (                                               ) const
{
  return 32768;
}

/** Context derivation process of coeff_abs_significant_flag
 * \param uiSigCoeffGroupFlag significance map of L1
 * \param uiBlkX column of current scan position
 * \param uiBlkY row of current scan position
 * \param uiLog2BlkSize log2 value of block size
 * \returns ctxInc for current scan position
 */
UInt TComTrQuant::getSigCoeffGroupCtxInc  ( const UInt*               uiSigCoeffGroupFlag,
                                           const UInt                      uiCGPosX,
                                           const UInt                      uiCGPosY,
                                           const UInt                      scanIdx,
                                           Int width, Int height)
{
  UInt uiRight = 0;
  UInt uiLower = 0;

  width >>= 2;
  height >>= 2;
#if !REMOVAL_8x2_2x8_CG
  if( width == 2 && height == 2 ) // 8x8
  {
    if( scanIdx == SCAN_HOR )  
    {
      width = 1;
      height = 4;
    }
    else if( scanIdx == SCAN_VER )
    {
      width = 4;
      height = 1;
    }
  }
#endif
  if( uiCGPosX < width - 1 )
  {
    uiRight = (uiSigCoeffGroupFlag[ uiCGPosY * width + uiCGPosX + 1 ] != 0);
  }
  if (uiCGPosY < height - 1 )
  {
    uiLower = (uiSigCoeffGroupFlag[ (uiCGPosY  + 1 ) * width + uiCGPosX ] != 0);
  }
  return (uiRight || uiLower);

}
/** set quantized matrix coefficient for encode
 * \param scalingList quantaized matrix address
 */
Void TComTrQuant::setScalingList(TComScalingList *scalingList)
{
  UInt size,list;
  UInt qp;

  for(size=0;size<SCALING_LIST_SIZE_NUM;size++)
  {
    for(list = 0; list < g_scalingListNum[size]; list++)
    {
      for(qp=0;qp<SCALING_LIST_REM_NUM;qp++)
      {
        xSetScalingListEnc(scalingList,list,size,qp);
        xSetScalingListDec(scalingList,list,size,qp);
        setErrScaleCoeff(list,size,qp,SCALING_LIST_SQT);
        if(size == SCALING_LIST_32x32 || size == SCALING_LIST_16x16)
        {
          setErrScaleCoeff(list,size-1,qp,SCALING_LIST_HOR);
          setErrScaleCoeff(list,size-1,qp,SCALING_LIST_VER);
        }
      }
    }
  }
}
/** set quantized matrix coefficient for decode
 * \param scalingList quantaized matrix address
 */
Void TComTrQuant::setScalingListDec(TComScalingList *scalingList)
{
  UInt size,list;
  UInt qp;

  for(size=0;size<SCALING_LIST_SIZE_NUM;size++)
  {
    for(list = 0; list < g_scalingListNum[size]; list++)
    {
      for(qp=0;qp<SCALING_LIST_REM_NUM;qp++)
      {
        xSetScalingListDec(scalingList,list,size,qp);
      }
    }
  }
}
/** set error scale coefficients
 * \param list List ID
 * \param uiSize Size
 * \param uiQP Quantization parameter
 */
Void TComTrQuant::setErrScaleCoeff(UInt list,UInt size, UInt qp, UInt dir)
{

  UInt uiLog2TrSize = g_aucConvertToBit[ g_scalingListSizeX[size] ] + 2;
#if FULL_NBIT
  UInt uiBitDepth = g_uiBitDepth;
#else
  UInt uiBitDepth = g_uiBitDepth + g_uiBitIncrement;  
#endif

  Int iTransformShift = MAX_TR_DYNAMIC_RANGE - uiBitDepth - uiLog2TrSize;  // Represents scaling through forward transform

  UInt i,uiMaxNumCoeff = g_scalingListSize[size];
  Int *piQuantcoeff;
  double *pdErrScale;
  piQuantcoeff   = getQuantCoeff(list, qp,size,dir);
  pdErrScale     = getErrScaleCoeff(list, size, qp,dir);

  double dErrScale = (double)(1<<SCALE_BITS);                              // Compensate for scaling of bitcount in Lagrange cost function
  dErrScale = dErrScale*pow(2.0,-2.0*iTransformShift);                     // Compensate for scaling through forward transform
  for(i=0;i<uiMaxNumCoeff;i++)
  {
    pdErrScale[i] =  dErrScale/(double)piQuantcoeff[i]/(double)piQuantcoeff[i]/(double)(1<<(2*g_uiBitIncrement));
  }
}

/** set quantized matrix coefficient for encode
 * \param scalingList quantaized matrix address
 * \param listId List index
 * \param sizeId size index
 * \param uiQP Quantization parameter
 */
Void TComTrQuant::xSetScalingListEnc(TComScalingList *scalingList, UInt listId, UInt sizeId, UInt qp)
{
  UInt width = g_scalingListSizeX[sizeId];
  UInt height = g_scalingListSizeX[sizeId];
  UInt ratio = g_scalingListSizeX[sizeId]/min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]);
  Int *quantcoeff;
  Int *coeff = scalingList->getScalingListAddress(sizeId,listId);
  quantcoeff   = getQuantCoeff(listId, qp, sizeId, SCALING_LIST_SQT);

  processScalingListEnc(coeff,quantcoeff,g_quantScales[qp]<<4,height,width,ratio,min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]),scalingList->getScalingListDC(sizeId,listId));

  if(sizeId == SCALING_LIST_32x32 || sizeId == SCALING_LIST_16x16) //for NSQT
  {
    quantcoeff   = getQuantCoeff(listId, qp, sizeId-1,SCALING_LIST_VER);
    processScalingListEnc(coeff,quantcoeff,g_quantScales[qp]<<4,height,width>>2,ratio,min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]),scalingList->getScalingListDC(sizeId,listId));

    quantcoeff   = getQuantCoeff(listId, qp, sizeId-1,SCALING_LIST_HOR);
    processScalingListEnc(coeff,quantcoeff,g_quantScales[qp]<<4,height>>2,width,ratio,min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]),scalingList->getScalingListDC(sizeId,listId));
  }
}
/** set quantized matrix coefficient for decode
 * \param scalingList quantaized matrix address
 * \param list List index
 * \param size size index
 * \param uiQP Quantization parameter
 */
Void TComTrQuant::xSetScalingListDec(TComScalingList *scalingList, UInt listId, UInt sizeId, UInt qp)
{
  UInt width = g_scalingListSizeX[sizeId];
  UInt height = g_scalingListSizeX[sizeId];
  UInt ratio = g_scalingListSizeX[sizeId]/min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]);
  Int *dequantcoeff;
  Int *coeff = scalingList->getScalingListAddress(sizeId,listId);

  dequantcoeff = getDequantCoeff(listId, qp, sizeId,SCALING_LIST_SQT);
  processScalingListDec(coeff,dequantcoeff,g_invQuantScales[qp],height,width,ratio,min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]),scalingList->getScalingListDC(sizeId,listId));

  if(sizeId == SCALING_LIST_32x32 || sizeId == SCALING_LIST_16x16)
  {
    dequantcoeff   = getDequantCoeff(listId, qp, sizeId-1,SCALING_LIST_VER);
    processScalingListDec(coeff,dequantcoeff,g_invQuantScales[qp],height,width>>2,ratio,min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]),scalingList->getScalingListDC(sizeId,listId));

    dequantcoeff   = getDequantCoeff(listId, qp, sizeId-1,SCALING_LIST_HOR);

    processScalingListDec(coeff,dequantcoeff,g_invQuantScales[qp],height>>2,width,ratio,min(MAX_MATRIX_SIZE_NUM,(Int)g_scalingListSizeX[sizeId]),scalingList->getScalingListDC(sizeId,listId));
  }
}

/** set flat matrix value to quantized coefficient
 */
Void TComTrQuant::setFlatScalingList()
{
  UInt size,list;
  UInt qp;

  for(size=0;size<SCALING_LIST_SIZE_NUM;size++)
  {
    for(list = 0; list <  g_scalingListNum[size]; list++)
    {
      for(qp=0;qp<SCALING_LIST_REM_NUM;qp++)
      {
        xsetFlatScalingList(list,size,qp);
        setErrScaleCoeff(list,size,qp,SCALING_LIST_SQT);
        if(size == SCALING_LIST_32x32 || size == SCALING_LIST_16x16)
        {
          setErrScaleCoeff(list,size-1,qp,SCALING_LIST_HOR);
          setErrScaleCoeff(list,size-1,qp,SCALING_LIST_VER);
        }
      }
    }
  }
}

/** set flat matrix value to quantized coefficient
 * \param list List ID
 * \param uiQP Quantization parameter
 * \param uiSize Size
 */
Void TComTrQuant::xsetFlatScalingList(UInt list, UInt size, UInt qp)
{
  UInt i,num = g_scalingListSize[size];
  UInt numDiv4 = num>>2;
  Int *quantcoeff;
  Int *dequantcoeff;
  Int quantScales = g_quantScales[qp];
  Int invQuantScales = g_invQuantScales[qp]<<4;

  quantcoeff   = getQuantCoeff(list, qp, size,SCALING_LIST_SQT);
  dequantcoeff = getDequantCoeff(list, qp, size,SCALING_LIST_SQT);

  for(i=0;i<num;i++)
  { 
    *quantcoeff++ = quantScales;
    *dequantcoeff++ = invQuantScales;
  }

  if(size == SCALING_LIST_32x32 || size == SCALING_LIST_16x16)
  {
    quantcoeff   = getQuantCoeff(list, qp, size-1, SCALING_LIST_HOR);
    dequantcoeff = getDequantCoeff(list, qp, size-1, SCALING_LIST_HOR);

    for(i=0;i<numDiv4;i++)
    {
      *quantcoeff++ = quantScales;
      *dequantcoeff++ = invQuantScales;
    }
    quantcoeff   = getQuantCoeff(list, qp, size-1 ,SCALING_LIST_VER);
    dequantcoeff = getDequantCoeff(list, qp, size-1 ,SCALING_LIST_VER);

    for(i=0;i<numDiv4;i++)
    {
      *quantcoeff++ = quantScales;
      *dequantcoeff++ = invQuantScales;
    }
  }
}

/** set quantized matrix coefficient for encode
 * \param coeff quantaized matrix address
 * \param quantcoeff quantaized matrix address
 * \param quantScales Q(QP%6)
 * \param height height
 * \param width width
 * \param ratio ratio for upscale
 * \param sizuNum matrix size
 * \param dc dc parameter
 */
Void TComTrQuant::processScalingListEnc( Int *coeff, Int *quantcoeff, Int quantScales, UInt height, UInt width, UInt ratio, Int sizuNum, UInt dc)
{
  Int nsqth = (height < width) ? 4: 1; //height ratio for NSQT
  Int nsqtw = (width < height) ? 4: 1; //width ratio for NSQT
  for(UInt j=0;j<height;j++)
  {
    for(UInt i=0;i<width;i++)
    {
      quantcoeff[j*width + i] = quantScales / coeff[sizuNum * (j * nsqth / ratio) + i * nsqtw /ratio];
    }
  }
  if(ratio > 1)
  {
    quantcoeff[0] = quantScales / dc;
  }
}
/** set quantized matrix coefficient for decode
 * \param coeff quantaized matrix address
 * \param dequantcoeff quantaized matrix address
 * \param invQuantScales IQ(QP%6))
 * \param height height
 * \param width width
 * \param ratio ratio for upscale
 * \param sizuNum matrix size
 * \param dc dc parameter
 */
Void TComTrQuant::processScalingListDec( Int *coeff, Int *dequantcoeff, Int invQuantScales, UInt height, UInt width, UInt ratio, Int sizuNum, UInt dc)
{
#if !REMOVE_NSQT
  Int nsqth = (height < width) ? 4: 1; //height ratio for NSQT
  Int nsqtw = (width < height) ? 4: 1; //width ratio for NSQT
#endif
  for(UInt j=0;j<height;j++)
  {
    for(UInt i=0;i<width;i++)
    {
#if REMOVE_NSQT
      dequantcoeff[j*width + i] = invQuantScales * coeff[sizuNum * (j / ratio) + i / ratio];
#else
      dequantcoeff[j*width + i] = invQuantScales * coeff[sizuNum * (j * nsqth / ratio) + i * nsqtw /ratio];
#endif
    }
  }
  if(ratio > 1)
  {
    dequantcoeff[0] = invQuantScales * dc;
  }
}

/** initialization process of scaling list array
 */
Void TComTrQuant::initScalingList()
{
  for(UInt sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
  {
    for(UInt listId = 0; listId < g_scalingListNum[sizeId]; listId++)
    {
      for(UInt qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
      {
        m_quantCoef   [sizeId][listId][qp][SCALING_LIST_SQT] = new Int [g_scalingListSize[sizeId]];
        m_dequantCoef [sizeId][listId][qp][SCALING_LIST_SQT] = new Int [g_scalingListSize[sizeId]];
        m_errScale    [sizeId][listId][qp][SCALING_LIST_SQT] = new double [g_scalingListSize[sizeId]];
        
        if(sizeId == SCALING_LIST_8x8 || (sizeId == SCALING_LIST_16x16 && listId < 2))
        {
          for(UInt dir = SCALING_LIST_VER; dir < SCALING_LIST_DIR_NUM; dir++)
          {
            m_quantCoef   [sizeId][listId][qp][dir] = new Int [g_scalingListSize[sizeId]];
            m_dequantCoef [sizeId][listId][qp][dir] = new Int [g_scalingListSize[sizeId]];
            m_errScale    [sizeId][listId][qp][dir] = new double [g_scalingListSize[sizeId]];
          }
        }
      }
    }
  }
  //copy for NSQT
  for(UInt qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
  {
    for(UInt dir = SCALING_LIST_VER; dir < SCALING_LIST_DIR_NUM; dir++)
    {
      m_quantCoef   [SCALING_LIST_16x16][3][qp][dir] = m_quantCoef   [SCALING_LIST_16x16][1][qp][dir];
      m_dequantCoef [SCALING_LIST_16x16][3][qp][dir] = m_dequantCoef [SCALING_LIST_16x16][1][qp][dir];
      m_errScale    [SCALING_LIST_16x16][3][qp][dir] = m_errScale    [SCALING_LIST_16x16][1][qp][dir];
    }
    m_quantCoef   [SCALING_LIST_32x32][3][qp][SCALING_LIST_SQT] = m_quantCoef   [SCALING_LIST_32x32][1][qp][SCALING_LIST_SQT];
    m_dequantCoef [SCALING_LIST_32x32][3][qp][SCALING_LIST_SQT] = m_dequantCoef [SCALING_LIST_32x32][1][qp][SCALING_LIST_SQT];
    m_errScale    [SCALING_LIST_32x32][3][qp][SCALING_LIST_SQT] = m_errScale    [SCALING_LIST_32x32][1][qp][SCALING_LIST_SQT];
  }
}
/** destroy quantization matrix array
 */
Void TComTrQuant::destroyScalingList()
{
  for(UInt sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
  {
    for(UInt listId = 0; listId < g_scalingListNum[sizeId]; listId++)
    {
      for(UInt qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
      {
        if(m_quantCoef   [sizeId][listId][qp][SCALING_LIST_SQT]) delete [] m_quantCoef   [sizeId][listId][qp][SCALING_LIST_SQT];
        if(m_dequantCoef [sizeId][listId][qp][SCALING_LIST_SQT]) delete [] m_dequantCoef [sizeId][listId][qp][SCALING_LIST_SQT];
        if(m_errScale    [sizeId][listId][qp][SCALING_LIST_SQT]) delete [] m_errScale    [sizeId][listId][qp][SCALING_LIST_SQT];
        if(sizeId == SCALING_LIST_8x8 || (sizeId == SCALING_LIST_16x16 && listId < 2))
        {
          for(UInt dir = SCALING_LIST_VER; dir < SCALING_LIST_DIR_NUM; dir++)
          {
            if(m_quantCoef   [sizeId][listId][qp][dir]) delete [] m_quantCoef   [sizeId][listId][qp][dir];
            if(m_dequantCoef [sizeId][listId][qp][dir]) delete [] m_dequantCoef [sizeId][listId][qp][dir];
            if(m_errScale    [sizeId][listId][qp][dir]) delete [] m_errScale    [sizeId][listId][qp][dir];
          }
        }
      }
    }
  }
}

//! \}
