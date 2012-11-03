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

//! \ingroup TLibCommon
//! \{

/**
 * Abstract class representing an SEI message with lightweight RTTI.
 */
class SEI
{
public:
  enum PayloadType
  {
    USER_DATA_UNREGISTERED = 5,
    PICTURE_DIGEST = 256,
  };
  
  SEI() {}
  virtual ~SEI() {}
  
  virtual PayloadType payloadType() const = 0;
};

class SEIuserDataUnregistered : public SEI
{
public:
  PayloadType payloadType() const { return USER_DATA_UNREGISTERED; }

  SEIuserDataUnregistered()
    : userData(0)
    {}

  virtual ~SEIuserDataUnregistered()
  {
    delete userData;
  }

  unsigned char uuid_iso_iec_11578[16];
  unsigned userDataLength;
  unsigned char *userData;
};

class SEIpictureDigest : public SEI
{
public:
  PayloadType payloadType() const { return PICTURE_DIGEST; }

  SEIpictureDigest() {}
  virtual ~SEIpictureDigest() {}
  
  enum Method
  {
    MD5,
    CRC,
    CHECKSUM,
    RESERVED,
  } method;

  unsigned char digest[3][16];
};

/**
 * A structure to collate all SEI messages.  This ought to be replaced
 * with a list of std::list<SEI*>.  However, since there is only one
 * user of the SEI framework, this will do initially */
class SEImessages
{
public:
  SEImessages()
    : user_data_unregistered(0)
    , picture_digest(0)
    {}

  ~SEImessages()
  {
    delete user_data_unregistered;
    delete picture_digest;
  }

  SEIuserDataUnregistered* user_data_unregistered;
  SEIpictureDigest* picture_digest;
};
//! \}
