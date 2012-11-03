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

#pragma once

#include <vector>
#include <sstream>
#include "CommonDef.h"

class TComOutputBitstream;

/**
 * Represents a single NALunit header and the associated RBSPayload
 */
struct NALUnit
{
  NalUnitType m_nalUnitType; ///< nal_unit_type
#if !REMOVE_NAL_REF_FLAG
  Bool        m_nalRefFlag;  ///< nal_ref_flag
#endif
  unsigned    m_temporalId;  ///< temporal_id

  /** construct an NALunit structure with given header values. */
  NALUnit(
    NalUnitType nalUnitType,
#if !REMOVE_NAL_REF_FLAG
    Bool        nalRefFlag,
#endif
    Int         temporalId = 0)
    :m_nalUnitType (nalUnitType)
#if !REMOVE_NAL_REF_FLAG
    ,m_nalRefFlag  (nalRefFlag)
#endif
    ,m_temporalId  (temporalId)
  {}

  /** default constructor - no initialization; must be perfomed by user */
  NALUnit() {}

  /** returns true if the NALunit is a slice NALunit */
  bool isSlice()
  {
    return m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR
        || m_nalUnitType == NAL_UNIT_CODED_SLICE_BLANT
        || m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA
        || m_nalUnitType == NAL_UNIT_CODED_SLICE_CRANT
        || m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA
        || m_nalUnitType == NAL_UNIT_CODED_SLICE_TLA
        || m_nalUnitType == NAL_UNIT_CODED_SLICE_TFD
        || m_nalUnitType == NAL_UNIT_CODED_SLICE;
  }
};

struct OutputNALUnit;

/**
 * A single NALunit, with complete payload in EBSP format.
 */
struct NALUnitEBSP : public NALUnit
{
  std::ostringstream m_nalUnitData;

  /**
   * convert the OutputNALUnit #nalu# into EBSP format by writing out
   * the NALUnit header, then the rbsp_bytes including any
   * emulation_prevention_three_byte symbols.
   */
  NALUnitEBSP(OutputNALUnit& nalu);
};
//! \}
//! \}
