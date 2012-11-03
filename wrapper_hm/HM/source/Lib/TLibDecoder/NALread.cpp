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

#include <vector>
#include <algorithm>
#include <ostream>

#include "NALread.h"
#include "TLibCommon/NAL.h"
#include "TLibCommon/TComBitStream.h"

using namespace std;

//! \ingroup TLibDecoder
//! \{
static void convertPayloadToRBSP(vector<uint8_t>& nalUnitBuf, TComInputBitstream *pcBitstream)
{
  unsigned zeroCount = 0;
  vector<uint8_t>::iterator it_read, it_write;

  for (it_read = it_write = nalUnitBuf.begin(); it_read != nalUnitBuf.end(); it_read++, it_write++)
  {
    if (zeroCount == 2 && *it_read == 0x03)
    {
      it_read++;
      zeroCount = 0;
    }
    zeroCount = (*it_read == 0x00) ? zeroCount+1 : 0;
    *it_write = *it_read;
  }

  nalUnitBuf.resize(it_write - nalUnitBuf.begin());
}

#if NAL_UNIT_HEADER
Void readNalUnitHeader(InputNALUnit& nalu)
{
  TComInputBitstream& bs = *nalu.m_Bitstream;

  bool forbidden_zero_bit = bs.read(1);           // forbidden_zero_bit
  assert(forbidden_zero_bit == 0);
  nalu.m_nalUnitType = (NalUnitType) bs.read(6);  // nal_unit_type
  unsigned reserved_one_6bits = bs.read(6);       // nuh_reserved_zero_6bits
  assert(reserved_one_6bits == 0);
  nalu.m_temporalId = bs.read(3) - 1;             // nuh_temporal_id_plus1

  if ( nalu.m_temporalId )
  {
    assert( nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_CRA
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_CRANT
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_BLA
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_BLANT
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR );
  }
}
#endif
/**
 * create a NALunit structure with given header values and storage for
 * a bitstream
 */
void read(InputNALUnit& nalu, vector<uint8_t>& nalUnitBuf)
{
  /* perform anti-emulation prevention */
  TComInputBitstream *pcBitstream = new TComInputBitstream(NULL);
  convertPayloadToRBSP(nalUnitBuf, pcBitstream);

  nalu.m_Bitstream = new TComInputBitstream(&nalUnitBuf);
  delete pcBitstream;
#if NAL_UNIT_HEADER
  readNalUnitHeader(nalu);
#else
  TComInputBitstream& bs = *nalu.m_Bitstream;

  bool forbidden_zero_bit = bs.read(1);
  assert(forbidden_zero_bit == 0);
#if !REMOVE_NAL_REF_FLAG
  nalu.m_nalRefFlag  = (bs.read(1) != 0 );
#endif
  nalu.m_nalUnitType = (NalUnitType) bs.read(6);
#if REMOVE_NAL_REF_FLAG
  unsigned reserved_one_6bits = bs.read(6);
  assert(reserved_one_6bits == 0);
#endif
#if TEMPORAL_ID_PLUS1
  nalu.m_temporalId = bs.read(3) - 1;
#if !REMOVE_NAL_REF_FLAG
  unsigned reserved_one_5bits = bs.read(5);
  assert(reserved_one_5bits == 0);
#endif
#else
  nalu.m_temporalId = bs.read(3);
  unsigned reserved_one_5bits = bs.read(5);
  assert(reserved_one_5bits == 1);
#endif

  if ( nalu.m_temporalId )
  {
    assert( nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_CRA
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_CRANT
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_BLA
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_BLANT
         && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR );
  }
#endif
}
//! \}
