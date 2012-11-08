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

/** 
 \file     SEIread.cpp
 \brief    reading funtionality for SEI messages
 */

#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComBitStream.h"
#include "TLibCommon/SEI.h"
#include "TLibCommon/TComSlice.h"
#include "SyntaxElementParser.h"
#include "SEIread.h"

//! \ingroup TLibDecoder
//! \{

#if ENC_DEC_TRACE
Void  xTraceSEIHeader()
{
  fprintf( g_hTrace, "=========== SEI message ===========\n");
}

Void  xTraceSEIMessageType(SEI::PayloadType payloadType)
{
  switch (payloadType)
  {
  case SEI::DECODED_PICTURE_HASH:
    fprintf( g_hTrace, "=========== Decoded picture hash SEI message ===========\n");
    break;
  case SEI::ACTIVE_PARAMETER_SETS:
    fprintf( g_hTrace, "=========== Active Parameter Sets SEI message ===========\n");
    break;
  case SEI::USER_DATA_UNREGISTERED:
    fprintf( g_hTrace, "=========== User Data Unregistered SEI message ===========\n");
    break;
  default:
    fprintf( g_hTrace, "=========== Unknown SEI message ===========\n");
    break;
  }
}
#endif

/**
 * unmarshal a single SEI message from bitstream bs
 */
void SEIReader::parseSEImessage(TComInputBitstream* bs, SEImessages& seis)
{
  setBitstream(bs);

  assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  do
  {
    xReadSEImessage(seis);
    /* SEI messages are an integer number of bytes, something has failed
    * in the parsing if bitstream not byte-aligned */
    assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  } while (0x80 != m_pcBitstream->peekBits(8));
  assert(m_pcBitstream->getNumBitsLeft() == 8); /* rsbp_trailing_bits */
}

Void SEIReader::xReadSEImessage(SEImessages& seis)
{
#if ENC_DEC_TRACE
  xTraceSEIHeader();
#endif
  Int payloadType = 0;
  UInt val = 0;

  do
  {
    READ_CODE (8, val, "payload_type");
    payloadType += val;
  } while (val==0xFF);

  UInt payloadSize = 0;
  do
  {
    READ_CODE (8, val, "payload_size");
    payloadSize += val;
  } while (val==0xFF);

#if ENC_DEC_TRACE
  xTraceSEIMessageType((SEI::PayloadType)payloadType);
#endif

  switch (payloadType)
  {
  case SEI::USER_DATA_UNREGISTERED:
    seis.user_data_unregistered = new SEIuserDataUnregistered;
    xParseSEIuserDataUnregistered(*seis.user_data_unregistered, payloadSize);
    break;
  case SEI::ACTIVE_PARAMETER_SETS:
    seis.active_parameter_sets = new SEIActiveParameterSets; 
    xParseSEIActiveParameterSets(*seis.active_parameter_sets, payloadSize); 
    break; 
  case SEI::DECODED_PICTURE_HASH:
    seis.picture_digest = new SEIDecodedPictureHash;
    xParseSEIDecodedPictureHash(*seis.picture_digest, payloadSize);
    break;
  case SEI::BUFFERING_PERIOD:
    seis.buffering_period = new SEIBufferingPeriod;
    seis.buffering_period->m_sps = seis.m_pSPS;
    xParseSEIBufferingPeriod(*seis.buffering_period, payloadSize);
    break;
  case SEI::PICTURE_TIMING:
    seis.picture_timing = new SEIPictureTiming;
    seis.picture_timing->m_sps = seis.m_pSPS;
    xParseSEIPictureTiming(*seis.picture_timing, payloadSize);
    break;
  case SEI::RECOVERY_POINT:
    seis.recovery_point = new SEIRecoveryPoint;
    xParseSEIRecoveryPoint(*seis.recovery_point, payloadSize);
    break;
  default:
    assert(!"Unhandled SEI message");
  }
}

/**
 * parse bitstream bs and unpack a user_data_unregistered SEI message
 * of payloasSize bytes into sei.
 */
Void SEIReader::xParseSEIuserDataUnregistered(SEIuserDataUnregistered &sei, UInt payloadSize)
{
  assert(payloadSize >= 16);
  UInt val;

  for (UInt i = 0; i < 16; i++)
  {
    READ_CODE (8, val, "uuid_iso_iec_11578");
    sei.uuid_iso_iec_11578[i] = val;
  }

  sei.userDataLength = payloadSize - 16;
  if (!sei.userDataLength)
  {
    sei.userData = 0;
    return;
  }

  sei.userData = new UChar[sei.userDataLength];
  for (UInt i = 0; i < sei.userDataLength; i++)
  {
    READ_CODE (8, val, "user_data" );
    sei.userData[i] = val;
  }
}

/**
 * parse bitstream bs and unpack a decoded picture hash SEI message
 * of payloadSize bytes into sei.
 */
Void SEIReader::xParseSEIDecodedPictureHash(SEIDecodedPictureHash& sei, UInt payloadSize)
{
  UInt val;
  READ_CODE (8, val, "hash_type");
  sei.method = static_cast<SEIDecodedPictureHash::Method>(val);
  for(Int yuvIdx = 0; yuvIdx < 3; yuvIdx++)
  {
    if(SEIDecodedPictureHash::MD5 == sei.method)
    {
      for (UInt i = 0; i < 16; i++)
      {
        READ_CODE(8, val, "picture_md5");
        sei.digest[yuvIdx][i] = val;
      }
    }
    else if(SEIDecodedPictureHash::CRC == sei.method)
    {
      READ_CODE(16, val, "picture_crc");
      sei.digest[yuvIdx][0] = val >> 8 & 0xFF;
      sei.digest[yuvIdx][1] = val & 0xFF;
    }
    else if(SEIDecodedPictureHash::CHECKSUM == sei.method)
    {
      READ_CODE(32, val, "picture_checksum");
      sei.digest[yuvIdx][0] = (val>>24) & 0xff;
      sei.digest[yuvIdx][1] = (val>>16) & 0xff;
      sei.digest[yuvIdx][2] = (val>>8)  & 0xff;
      sei.digest[yuvIdx][3] =  val      & 0xff;
    }
  }
}
Void SEIReader::xParseSEIActiveParameterSets(SEIActiveParameterSets& sei, UInt payloadSize)
{
  UInt val; 
  READ_CODE(4, val, "active_vps_id");
  sei.activeVPSId = val; 

  READ_CODE(1, val, "active_sps_id_present_flag");
  sei.activeSPSIdPresentFlag = val; 

  if(sei.activeSPSIdPresentFlag)
  {
    READ_UVLC(val, "active_seq_param_set_id");
    sei.activeSeqParamSetId = val; 
  }

  READ_CODE(1, val, "active_param_set_sei_extension_flag");
  sei.activeParamSetSEIExtensionFlag = val; 
  
  UInt uibits = m_pcBitstream->getNumBitsUntilByteAligned(); 
  
  while(uibits--)
  {
    READ_FLAG(val, "alignment_bit");
  }
}

Void SEIReader::xParseSEIBufferingPeriod(SEIBufferingPeriod& sei, UInt payloadSize)
{
  Int i, nalOrVcl;
  UInt code;

  TComVUI *pVUI = sei.m_sps->getVuiParameters();

  READ_UVLC( code, "seq_parameter_set_id" );                            sei.m_seqParameterSetId     = code;
  if( !pVUI->getSubPicCpbParamsPresentFlag() )
  {
    READ_FLAG( code, "alt_cpb_params_present_flag" );                   sei.m_altCpbParamsPresentFlag = code;
  }

  for( nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
  {
    if( ( ( nalOrVcl == 0 ) && ( pVUI->getNalHrdParametersPresentFlag() ) ) ||
        ( ( nalOrVcl == 1 ) && ( pVUI->getVclHrdParametersPresentFlag() ) ) )
    {
      for( i = 0; i < ( pVUI->getCpbCntMinus1( 0 ) + 1 ); i ++ )
      {
        READ_CODE( ( pVUI->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_cpb_removal_delay" );
        sei.m_initialCpbRemovalDelay[i][nalOrVcl] = code;
        READ_CODE( ( pVUI->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_cpb_removal_delay_offset" );
        sei.m_initialCpbRemovalDelayOffset[i][nalOrVcl] = code;
        if( pVUI->getSubPicCpbParamsPresentFlag() || sei.m_altCpbParamsPresentFlag )
        {
          READ_CODE( ( pVUI->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_alt_cpb_removal_delay" );
          sei.m_initialAltCpbRemovalDelay[i][nalOrVcl] = code;
          READ_CODE( ( pVUI->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_alt_cpb_removal_delay_offset" );
          sei.m_initialAltCpbRemovalDelayOffset[i][nalOrVcl] = code;
        }
      }
    }
  }
  xParseByteAlign();
}
Void SEIReader::xParseSEIPictureTiming(SEIPictureTiming& sei, UInt payloadSize)
{
  Int i;
  UInt code;

  TComVUI *vui = sei.m_sps->getVuiParameters();

  if( !vui->getNalHrdParametersPresentFlag() && !vui->getVclHrdParametersPresentFlag() )
  {
    return;
  }

  READ_CODE( ( vui->getCpbRemovalDelayLengthMinus1() + 1 ), code, "au_cpb_removal_delay" );
  sei.m_auCpbRemovalDelay = code;
  READ_CODE( ( vui->getDpbOutputDelayLengthMinus1() + 1 ), code, "pic_dpb_output_delay" );
  sei.m_picDpbOutputDelay = code;

  if( sei.m_sps->getVuiParameters()->getSubPicCpbParamsPresentFlag() )
  {
    READ_UVLC( code, "num_decoding_units_minus1");
    sei.m_numDecodingUnitsMinus1 = code;
    READ_FLAG( code, "du_common_cpb_removal_delay_flag" );
    sei.m_duCommonCpbRemovalDelayFlag = code;
    if( sei.m_duCommonCpbRemovalDelayFlag )
    {
      READ_CODE( ( vui->getDuCpbRemovalDelayLengthMinus1() + 1 ), code, "du_common_cpb_removal_delay_minus1" );
      sei.m_duCommonCpbRemovalDelayMinus1 = code;
    }
    else
    {
      if( sei.m_numNalusInDuMinus1 != NULL )
      {
        delete sei.m_numNalusInDuMinus1;
      }
      sei.m_numNalusInDuMinus1 = new UInt[ ( sei.m_numDecodingUnitsMinus1 + 1 ) ];
      if( sei.m_duCpbRemovalDelayMinus1  != NULL )
      {
        delete sei.m_duCpbRemovalDelayMinus1;
      }
      sei.m_duCpbRemovalDelayMinus1  = new UInt[ ( sei.m_numDecodingUnitsMinus1 + 1 ) ];

      for( i = 0; i < ( sei.m_numDecodingUnitsMinus1 + 1 ); i ++ )
      {
        READ_UVLC( code, "num_nalus_in_du_minus1");
        sei.m_numNalusInDuMinus1[ i ] = code;
        READ_CODE( ( vui->getDuCpbRemovalDelayLengthMinus1() + 1 ), code, "du_cpb_removal_delay_minus1" );
        sei.m_duCpbRemovalDelayMinus1[ i ] = code;
      }
    }
  }
  xParseByteAlign();
}
Void SEIReader::xParseSEIRecoveryPoint(SEIRecoveryPoint& sei, UInt payloadSize)
{
  Int  iCode;
  UInt uiCode;
  READ_SVLC( iCode,  "recovery_poc_cnt" );      sei.m_recoveryPocCnt     = iCode;
  READ_FLAG( uiCode, "exact_matching_flag" );   sei.m_exactMatchingFlag  = uiCode;
  READ_FLAG( uiCode, "broken_link_flag" );      sei.m_brokenLinkFlag     = uiCode;
  xParseByteAlign();
}

Void SEIReader::xParseByteAlign()
{
  UInt code;
  if( m_pcBitstream->getNumBitsRead() % 8 != 0 )
  {
    READ_FLAG( code, "bit_equal_to_one" );          assert( code == 1 );
  }
  while( m_pcBitstream->getNumBitsRead() % 8 != 0 )
  {
    READ_FLAG( code, "bit_equal_to_zero" );         assert( code == 0 );
  }
}
//! \}
