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

/** \file     TEncAdaptiveLoopFilter.h
 \brief    estimation part of adaptive loop filter class (header)
 */

#ifndef __TENCADAPTIVELOOPFILTER__
#define __TENCADAPTIVELOOPFILTER__

#include "TLibCommon/TComAdaptiveLoopFilter.h"
#include "TLibCommon/TComPic.h"

#include "TEncEntropy.h"
#include "TEncSbac.h"
#include "TLibCommon/TComBitCounter.h"

#if !REMOVE_ALF
//! \ingroup TLibEncoder
//! \{
#define LCUALF_AVOID_USING_BOTTOM_LINES_ENCODER 1 //!< avoid using LCU bottom lines when lcu-based encoder RDO is used
#define LCUALF_AVOID_USING_RIGHT_LINES_ENCODER 1  //!< Avoid using right-most lines per LCU during encoder LCU on/off decision
// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// correlation info
struct AlfCorrData
{
  Double*** ECorr; //!< auto-correlation matrix
  Double**  yCorr; //!< cross-correlation
  Double*   pixAcc;
  Int componentID;

  //constructor & operator
  AlfCorrData();
  AlfCorrData(Int cIdx);
  ~AlfCorrData();
  Void reset();
  Void mergeFrom(const AlfCorrData& src, Int* mergeTable, Bool doPixAccMerge);
  AlfCorrData& operator += (const AlfCorrData& src);
};

/// estimation part of adaptive loop filter class
class TEncAdaptiveLoopFilter : public TComAdaptiveLoopFilter
{
private:
  ///
  /// variables for correlation calculation
  ///
  Double*  m_y_merged[NO_VAR_BINS];
  Double** m_E_merged[NO_VAR_BINS];
  Double   m_pixAcc_merged[NO_VAR_BINS];
  Double   m_y_temp[ALF_MAX_NUM_COEF];
  double **m_E_temp;
  Int    m_lastSliceIdx;
  Bool   m_alfLowLatencyEncoding;  
  Int*   m_numSlicesDataInOneLCU;
  Int*   m_coeffNoFilter[NO_VAR_BINS]; //!< used for RDO
  AlfCorrData** m_alfCorr[NUM_ALF_COMPONENT];
  AlfCorrData*  m_alfCorrMerged[NUM_ALF_COMPONENT]; //!< used for RDO
  AlfCorrData** m_alfNonSkippedCorr[NUM_ALF_COMPONENT];
  ALFParam*** m_alfPictureParam;
  Int         m_gopSize;                
  Double      m_dLambdaLuma;
  Double      m_dLambdaChroma;
private:

  Void initALFEncoderParam();
  Void getStatistics(TComPicYuv* pPicOrg, TComPicYuv* pPicDec);
  Void getOneCompStatistics(AlfCorrData** alfCorrComp, Int compIdx, Pel* imgOrg, Pel* imgDec, Int stride, Int formatShift);
  Void getStatisticsOneLCU(Bool skipLCUBottomLines, Int compIdx, AlfLCUInfo* alfLCU, AlfCorrData* alfCorr, Pel* pPicOrg, Pel* pPicSrc, Int stride, Int formatShift);
  Void decideParameters(ALFParam** alfPictureParam, TComPicYuv* pPicOrg, TComPicYuv* pPicDec, TComPicYuv* pPicRest);
  Void setCurAlfParam(ALFParam** alfPictureParam);
  Int  getTemporalLayerNo(Int poc, Int gopSize);
  Void decideAlfPictureParam(ALFParam** alfPictureParam, Bool useAllLCUs);
  Void estimateLcuControl(ALFParam** alfPictureParam);
  Void deriveFilterInfo(Int compIdx, AlfCorrData* alfCorr, ALFParam* alfFiltParam, Int maxNumFilters, Double lambda);
  Void calcCorrOneCompRegionChma(Pel* imgOrg, Pel* imgPad, Int stride, Int yPos, Int xPos, Int height, Int width, Double **eCorr, Double *yCorr, Bool isSymmCopyBlockMatrix); //!< Calculate correlations for chroma                                        
  Void calcCorrOneCompRegionLuma(Pel* imgOrg, Pel* imgPad, Int stride, Int yPos, Int xPos, Int height, Int width, Double ***eCorr, Double **yCorr, Double *pixAcc, Bool isSymmCopyBlockMatrix);

  //LCU-based mode decision
  Void  executeLCUOnOffDecision(Int compIdx, ALFParam* alfParam, Pel* pOrg, Pel* pDec, Pel* pRest, Int stride, Int formatShift, AlfCorrData** alfCorrLCUs);
  Int64 estimateFilterDistortion(Int compIdx, AlfCorrData* alfCorr, Int** coeff = NULL, Int filterSetSize = 1, Int* mergeTable = NULL, Bool doPixAccMerge = false);
  Int64 calcAlfLCUDist(Bool skipLCUBottomLines, Int compIdx, AlfLCUInfo& alfLCUInfo, Pel* picSrc, Pel* picCmp, Int stride, Int formatShift);
  Void  reconstructOneAlfLCU(Int compIdx, AlfLCUInfo& alfLCUInfo, Bool alfEnabled, ALFParam* alfParam, Pel* picDec, Pel* picRest, Int stride, Int formatShift);
  Void  copyOneAlfLCU(AlfLCUInfo& alfLCUInfo, Pel* picDst, Pel* picSrc, Int stride, Int formatShift);
  // functions related to filtering
  Void xFilterCoefQuickSort   ( Double *coef_data, Int *coef_num, Int upper, Int lower );
  Void xQuantFilterCoef       ( Double* h, Int* qh, Int tap, int bit_depth );
  // distortion / misc functions
  UInt64 xCalcSSD             ( Pel* pOrg, Pel* pCmp, Int iWidth, Int iHeight, Int iStride );
  Int64 xFastFiltDistEstimation(Double** ppdE, Double* pdy, Int* piCoeff, Int iFiltLength); //!< Estimate filtering distortion by correlation values and filter coefficients

  /// code filter coefficients
  Void xcodeFiltCoeff(Int **filterCoeff, Int* varIndTab, Int numFilters, ALFParam* alfParam);
  Void xfindBestFilterVarPred(double **ySym, double ***ESym, double *pixAcc, Int **filterCoeffSym, Int *filters_per_fr_best, Int varIndTab[], double lambda_val, Int numMaxFilters);
  double xfindBestCoeffCodMethod(int **filterCoeffSymQuant, int filter_shape, int sqrFiltLength, int filters_per_fr, double errorForce0CoeffTab[NO_VAR_BINS][2], double lambda);
  UInt uvlcBitrateEstimate(Int val);
  UInt svlcBitrateEsitmate(Int val) {  return uvlcBitrateEstimate (( val <= 0) ? (-val<<1) : ((val<<1)-1));}
  UInt golombBitrateEstimate(Int coeff, Int k);
  UInt ALFParamBitrateEstimate(ALFParam* alfParam);
  UInt filterCoeffBitrateEstimate(Int compIdx, Int* coeff);
  Void predictALFCoeff(Int** coeff, Int numCoef, Int numFilters);
  //cholesky related
  Int   xGauss( Double **a, Int N );
  Double findFilterCoeff(double ***EGlobalSeq, double **yGlobalSeq, double *pixAccGlobalSeq, int **filterCoeffSeq,int **filterCoeffQuantSeq, int intervalBest[NO_VAR_BINS][2], int varIndTab[NO_VAR_BINS], int sqrFiltLength, int filters_per_fr, int *weights, double errorTabForce0Coeff[NO_VAR_BINS][2]);
  Double QuantizeIntegerFilterPP(Double *filterCoeff, Int *filterCoeffQuant, Double **E, Double *y, Int sqrFiltLength, Int *weights);
  Void roundFiltCoeff(int *FilterCoeffQuan, double *FilterCoeff, int sqrFiltLength, int factor);
  double mergeFiltersGreedy(double **yGlobalSeq, double ***EGlobalSeq, double *pixAccGlobalSeq, int intervalBest[NO_VAR_BINS][2], int sqrFiltLength, int noIntervals);
  double calculateErrorAbs(double **A, double *b, double y, int size);
  double calculateErrorCoeffProvided(double **A, double *b, double *c, int size);
  Void add_b(double *bmerged, double **b, int start, int stop, int size);
  Void add_A(double **Amerged, double ***A, int start, int stop, int size);
  Int  gnsSolveByChol(double **LHS, double *rhs, double *x, int noEq);
  Void gnsBacksubstitution(double R[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], double z[ALF_MAX_NUM_COEF], int R_size, double A[ALF_MAX_NUM_COEF]);
  Void gnsTransposeBacksubstitution(double U[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], double rhs[], double x[],int order);
  Int  gnsCholeskyDec(double **inpMatr, double outMatr[ALF_MAX_NUM_COEF][ALF_MAX_NUM_COEF], int noEq);

public:
  TEncAdaptiveLoopFilter          ();
  virtual ~TEncAdaptiveLoopFilter () {}
  Void setALFLowLatencyEncoding(Bool b) {m_alfLowLatencyEncoding = b;}
#if ALF_CHROMA_LAMBDA
  Void ALFProcess(ALFParam** alfPictureParam, Double lambdaLuma, Double lambdaChroma);
#else
  Void ALFProcess(ALFParam** alfPictureParam, Double lambda);
#endif
  Void setGOPSize(Int val) { m_gopSize = val; } //!< set GOP size
  Void createAlfGlobalBuffers(); //!< create ALF global buffers
  Void destroyAlfGlobalBuffers(); //!< destroy ALF global buffers
  Void PCMLFDisableProcess (TComPic* pcPic);
};

//! \}
#endif
#endif
