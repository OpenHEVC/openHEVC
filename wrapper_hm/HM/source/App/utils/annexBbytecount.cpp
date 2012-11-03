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

#include <stdint.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string.h>

#include "TLibDecoder/AnnexBread.h"

using namespace std;

static struct {
  AnnexBStats expected;
  unsigned data_len;
  const char data[10];
} tests[] = {
  /* trivial cases: startcode, no payload */
  {{0, 0, 3, 0, 0}, 3, {0,0,1}},
  {{0, 1, 3, 0, 0}, 4, {0,0,0,1}},
  {{2, 1, 3, 0, 0}, 6, {0,0,0,0,0,1}},
  /* trivial cases: startcode, payload */
  {{0, 0, 3, 1, 0}, 4, {0,0,1,2}},
  {{0, 0, 3, 2, 0}, 5, {0,0,1,2,0}},
  {{0, 0, 3, 3, 0}, 6, {0,0,1,2,0,0}},
  {{0, 0, 3, 1, 3}, 7, {0,0,1,2,0,0,0}},
  /* trivial cases: two nal units: extract the first */
  {{0, 0, 3, 1, 0}, 8, {0,0,1,2,0,0,1,3}},
  {{0, 0, 3, 1, 0}, 9, {0,0,1,2,0,0,0,1,3}},
  {{0, 0, 3, 1, 1}, 10, {0,0,1,2,0,0,0,0,1,3}},
  /* edge cases with EOF near start*/
  {{0, 0, 0, 0, 0}, 0, {}},
  {{1, 0, 0, 0, 0}, 1, {0}},
  {{2, 0, 0, 0, 0}, 2, {0,0}},
  {{3, 0, 0, 0, 0}, 3, {0,0,0}},
};

void selftest()
{
  /* test */
  for (unsigned i = 0; i < sizeof(tests)/sizeof(*tests); i++)
  {
    istringstream in(string(tests[i].data, tests[i].data_len));
    InputByteStream bs(in);

    AnnexBStats actual = AnnexBStats();
    vector<uint8_t> nalUnit;

    byteStreamNALUnit(bs, nalUnit, actual);

    cout << "Self-Test: " << i << ", {";
    for (unsigned j = 0; j < tests[i].data_len; j++)
    {
      cout << hex << (unsigned int)tests[i].data[j] << dec;
      if (j < tests[i].data_len-1)
        cout << ",";
    }
    cout << "} ";

    bool ok = true;
#define VERIFY(a,b,m) \
  if (a.m != b.m) { \
    ok = false; \
    cout << endl << "  MISSMATCH " #m << ", E(" << b.m << ") != " << a.m; \
  }
    VERIFY(actual, tests[i].expected, m_numLeadingZero8BitsBytes);
    VERIFY(actual, tests[i].expected, m_numZeroByteBytes);
    VERIFY(actual, tests[i].expected, m_numStartCodePrefixBytes);
    VERIFY(actual, tests[i].expected, m_numBytesInNALUnit);
    VERIFY(actual, tests[i].expected, m_numTrailingZero8BitsBytes);
#undef VERIFY
    if (ok)
      cout << "OK";
    cout << endl;
  }
}

int main(int argc, char*argv[])
{
  selftest();

  if (argc != 2)
    return 0;

  ifstream in(argv[1], ifstream::in | ifstream::binary);
  InputByteStream bs(in);

  AnnexBStats annexBStatsTotal = AnnexBStats();
  AnnexBStats annexBStatsTotal_VCL = AnnexBStats();
  AnnexBStats annexBStatsTotal_Filler = AnnexBStats();
  AnnexBStats annexBStatsTotal_Other = AnnexBStats();
  unsigned numNALUnits = 0;

  cout << "NALUnits:" << endl;
  while (!!in)
  {
    AnnexBStats annexBStatsSingle = AnnexBStats();
    vector<uint8_t> nalUnit;

    byteStreamNALUnit(bs, nalUnit, annexBStatsSingle);

    int nal_unit_type = -1;
    if (annexBStatsSingle.m_numBytesInNALUnit)
    {
      nal_unit_type = nalUnit[0] & 0x1f;
    }

    cout << " - NALU: #" << numNALUnits << " nal_unit_type:" << nal_unit_type << endl
         << "   num_bytes(leading_zero_8bits): " << annexBStatsSingle.m_numLeadingZero8BitsBytes << endl
         << "   num_bytes(zero_byte): " << annexBStatsSingle.m_numZeroByteBytes << endl
         << "   num_bytes(start_code_prefix_one_3bytes): " << annexBStatsSingle.m_numStartCodePrefixBytes << endl
         << "   NumBytesInNALunit: " << annexBStatsSingle.m_numBytesInNALUnit << endl
         << "   num_bytes(trailing_zero_8bits): " << annexBStatsSingle.m_numTrailingZero8BitsBytes << endl
         ;

    annexBStatsTotal += annexBStatsSingle;
    numNALUnits++;

    if (!annexBStatsSingle.m_numBytesInNALUnit)
      continue;

    /* identify the NAL unit type and add stats to the correct
     * accumulators */
    switch (nalUnit[0] & 0x1f) {
    case 1: case 2: case 3: case 4: case 5:
      annexBStatsTotal_VCL += annexBStatsSingle;
      break;
    case 12:
      annexBStatsTotal_Filler += annexBStatsSingle;
      break;
    default:
      annexBStatsTotal_Other += annexBStatsSingle;
    };
  }

  cout << "Summary: " << endl
       << "  num_bytes(leading_zero_8bits): " << annexBStatsTotal.m_numLeadingZero8BitsBytes << endl
       << "  num_bytes(zero_byte): " << annexBStatsTotal.m_numZeroByteBytes << endl
       << "  num_bytes(start_code_prefix_one_3bytes): " << annexBStatsTotal.m_numStartCodePrefixBytes << endl
       << "  NumBytesInNALunit: " << annexBStatsTotal.m_numBytesInNALUnit << endl
       << "  num_bytes(trailing_zero_8bits): " << annexBStatsTotal.m_numTrailingZero8BitsBytes << endl
       ;

  cout << "Summary(VCL): " << endl
       << "  num_bytes(leading_zero_8bits): " << annexBStatsTotal_VCL.m_numLeadingZero8BitsBytes << endl
       << "  num_bytes(zero_byte): " << annexBStatsTotal_VCL.m_numZeroByteBytes << endl
       << "  num_bytes(start_code_prefix_one_3bytes): " << annexBStatsTotal_VCL.m_numStartCodePrefixBytes << endl
       << "  NumBytesInNALunit: " << annexBStatsTotal_VCL.m_numBytesInNALUnit << endl
       << "  num_bytes(trailing_zero_8bits): " << annexBStatsTotal_VCL.m_numTrailingZero8BitsBytes << endl
       ;

  cout << "Summary(Filler): " << endl
       << "  num_bytes(leading_zero_8bits): " << annexBStatsTotal_Filler.m_numLeadingZero8BitsBytes << endl
       << "  num_bytes(zero_byte): " << annexBStatsTotal_Filler.m_numZeroByteBytes << endl
       << "  num_bytes(start_code_prefix_one_3bytes): " << annexBStatsTotal_Filler.m_numStartCodePrefixBytes << endl
       << "  NumBytesInNALunit: " << annexBStatsTotal_Filler.m_numBytesInNALUnit << endl
       << "  num_bytes(trailing_zero_8bits): " << annexBStatsTotal_Filler.m_numTrailingZero8BitsBytes << endl
       ;

  cout << "Summary(Other): " << endl
       << "  num_bytes(leading_zero_8bits): " << annexBStatsTotal_Other.m_numLeadingZero8BitsBytes << endl
       << "  num_bytes(zero_byte): " << annexBStatsTotal_Other.m_numZeroByteBytes << endl
       << "  num_bytes(start_code_prefix_one_3bytes): " << annexBStatsTotal_Other.m_numStartCodePrefixBytes << endl
       << "  NumBytesInNALunit: " << annexBStatsTotal_Other.m_numBytesInNALUnit << endl
       << "  num_bytes(trailing_zero_8bits): " << annexBStatsTotal_Other.m_numTrailingZero8BitsBytes << endl
       ;

  /* The first such type of bitstream, called Type I bitstream, is a
   * NAL unit stream containing only the VCL NAL units and filler data
   * NAL units for all access units in the bitstream.
   */
  unsigned totalBytes_T1HRD = annexBStatsTotal_VCL.m_numBytesInNALUnit + annexBStatsTotal_Filler.m_numBytesInNALUnit;

  /*The second type of bitstream, called a Type II bitstream,
   * contains, in addition to the VCL NAL units and filler data NAL
   * units for all access units in the bitstream, at least one of
   * the following:
   *  (a) additional non-VCL NAL units other than filler data NAL
   *      units.
   */
  unsigned totalBytes_T2aHRD = annexBStatsTotal.m_numBytesInNALUnit;

  /*  (b) all leading_zero_8bits, zero_byte,
   *      start_code_prefix_one_3bytes, and trailing_zero_8bits syntax
   *      elements that form a byte stream from the NAL unit stream (as
   *      specified in Annex B)
   */
  unsigned totalBytes_T2abHRD = 0;
  totalBytes_T2abHRD += annexBStatsTotal.m_numLeadingZero8BitsBytes;
  totalBytes_T2abHRD += annexBStatsTotal.m_numZeroByteBytes;
  totalBytes_T2abHRD += annexBStatsTotal.m_numStartCodePrefixBytes;
  totalBytes_T2abHRD += annexBStatsTotal.m_numBytesInNALUnit;
  totalBytes_T2abHRD += annexBStatsTotal.m_numTrailingZero8BitsBytes;

  cout << "Totals (bytes):" << endl;
  cout << "  Type1 HRD: " << totalBytes_T1HRD << endl;
  cout << "  Type2 HRD: " << totalBytes_T2aHRD << endl;
  cout << "  Type2b HRD: " << totalBytes_T2abHRD << endl;

  return 0;
}
