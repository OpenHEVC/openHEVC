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

#include "TLibCommon/TComBitStream.h"
#include "TLibCommon/SEI.h"
#include "SEIread.h"

//! \ingroup TLibDecoder
//! \{

static void parseSEIuserDataUnregistered(TComInputBitstream& bs, SEIuserDataUnregistered &sei, unsigned payloadSize);
static void parseSEIpictureDigest(TComInputBitstream& bs, SEIpictureDigest& sei, unsigned payloadSize);

/**
 * unmarshal a single SEI message from bitstream bs
 */
void parseSEImessage(TComInputBitstream& bs, SEImessages& seis)
{
  unsigned payloadType = 0;
  for (unsigned char byte = 0xff; 0xff == byte; )
  {
    payloadType += byte = bs.read(8);
  }

  unsigned payloadSize = 0;
  for (unsigned char byte = 0xff; 0xff == byte; )
  {
    payloadSize += byte = bs.read(8);
  }

  switch (payloadType)
  {
  case SEI::USER_DATA_UNREGISTERED:
    seis.user_data_unregistered = new SEIuserDataUnregistered;
    parseSEIuserDataUnregistered(bs, *seis.user_data_unregistered, payloadSize);
    break;
  case SEI::PICTURE_DIGEST:
    seis.picture_digest = new SEIpictureDigest;
    parseSEIpictureDigest(bs, *seis.picture_digest, payloadSize);
    break;
  default:
    assert(!"Unhandled SEI message");
  }
}

/**
 * parse bitstream bs and unpack a user_data_unregistered SEI message
 * of payloasSize bytes into sei.
 */
static void parseSEIuserDataUnregistered(TComInputBitstream& bs, SEIuserDataUnregistered &sei, unsigned payloadSize)
{
  assert(payloadSize >= 16);
  for (unsigned i = 0; i < 16; i++)
  {
    sei.uuid_iso_iec_11578[i] = bs.read(8);
  }

  sei.userDataLength = payloadSize - 16;
  if (!sei.userDataLength)
  {
    sei.userData = 0;
    return;
  }

  sei.userData = new unsigned char[sei.userDataLength];
  for (unsigned i = 0; i < sei.userDataLength; i++)
  {
    sei.userData[i] = bs.read(8);
  }
}

/**
 * parse bitstream bs and unpack a picture_digest SEI message
 * of payloadSize bytes into sei.
 */
static void parseSEIpictureDigest(TComInputBitstream& bs, SEIpictureDigest& sei, unsigned payloadSize)
{
  int numChar=0;

  sei.method = static_cast<SEIpictureDigest::Method>(bs.read(8));
  if(SEIpictureDigest::MD5 == sei.method)
  {
    numChar = 16;
  }
  else if(SEIpictureDigest::CRC == sei.method)
  {
    numChar = 2;
  }
  else if(SEIpictureDigest::CHECKSUM == sei.method)
  {
    numChar = 4;
  }

  for(int yuvIdx = 0; yuvIdx < 3; yuvIdx++)
  {
    for (unsigned i = 0; i < numChar; i++)
    {
      sei.digest[yuvIdx][i] = bs.read(8);
    }
  }
}

//! \}
