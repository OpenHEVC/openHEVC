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

#include "TLibCommon/TComBitCounter.h"
#include "TLibCommon/TComBitStream.h"
#include "TLibCommon/SEI.h"
#include "SEIwrite.h"

//! \ingroup TLibEncoder
//! \{

static void writeSEIuserDataUnregistered(TComBitIf& bs, const SEIuserDataUnregistered &sei);
static void writeSEIpictureDigest(TComBitIf& bs, const SEIpictureDigest& sei);

void writeSEIpayloadData(TComBitIf& bs, const SEI& sei)
{
  switch (sei.payloadType())
  {
  case SEI::USER_DATA_UNREGISTERED:
    writeSEIuserDataUnregistered(bs, *static_cast<const SEIuserDataUnregistered*>(&sei));
    break;
  case SEI::PICTURE_DIGEST:
    writeSEIpictureDigest(bs, *static_cast<const SEIpictureDigest*>(&sei));
    break;
  default:
    assert(!"Unhandled SEI message");
  }
}

/**
 * marshal a single SEI message sei, storing the marshalled representation
 * in bitstream bs.
 */
void writeSEImessage(TComBitIf& bs, const SEI& sei)
{
  /* calculate how large the payload data is */
  /* TODO: this would be far nicer if it used vectored buffers */
  TComBitCounter bs_count;
  bs_count.resetBits();
  writeSEIpayloadData(bs_count, sei);
  unsigned payload_data_num_bits = bs_count.getNumberOfWrittenBits();
  assert(0 == payload_data_num_bits % 8);

  unsigned payloadType = sei.payloadType();
  for (; payloadType >= 0xff; payloadType -= 0xff)
  {
    bs.write(0xff, 8);
  }
  bs.write(payloadType, 8);

  unsigned payloadSize = payload_data_num_bits/8;
  for (; payloadSize >= 0xff; payloadSize -= 0xff)
  {
    bs.write(0xff, 8);
  }
  bs.write(payloadSize, 8);

  /* payloadData */
  writeSEIpayloadData(bs, sei);
}

/**
 * marshal a user_data_unregistered SEI message sei, storing the marshalled
 * representation in bitstream bs.
 */
static void writeSEIuserDataUnregistered(TComBitIf& bs, const SEIuserDataUnregistered &sei)
{
  for (unsigned i = 0; i < 16; i++)
  {
    bs.write(sei.uuid_iso_iec_11578[i], 8);
  }

  for (unsigned i = 0; i < sei.userDataLength; i++)
  {
    bs.write(sei.userData[i], 8);
  }
}

/**
 * marshal a picture_digest SEI message, storing the marshalled
 * representation in bitstream bs.
 */
static void writeSEIpictureDigest(TComBitIf& bs, const SEIpictureDigest& sei)
{
  int numChar=0;
  bs.write(sei.method, 8);
  if(sei.method == SEIpictureDigest::MD5)
  {
    numChar = 16;
  }
  else if(sei.method == SEIpictureDigest::CRC)
  {
    numChar = 2;
  }
  else if(sei.method == SEIpictureDigest::CHECKSUM)
  {
    numChar = 4;
  }
  for(int yuvIdx = 0; yuvIdx < 3; yuvIdx++)
  {
    for (unsigned i = 0; i < numChar; i++)
    {
      bs.write(sei.digest[yuvIdx][i], 8);
    }
  }
}
//! \}
