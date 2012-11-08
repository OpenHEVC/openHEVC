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
#include "TLibCommon/TComSlice.h"
#include "SEIwrite.h"

//! \ingroup TLibEncoder
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
  case SEI::USER_DATA_UNREGISTERED:
    fprintf( g_hTrace, "=========== User Data Unregistered SEI message ===========\n");
    break;
  case SEI::ACTIVE_PARAMETER_SETS:
    fprintf( g_hTrace, "=========== Active Parameter sets SEI message ===========\n");
    break;
  default:
    fprintf( g_hTrace, "=========== Unknown SEI message ===========\n");
    break;
  }
}
#endif

void SEIWriter::xWriteSEIpayloadData(const SEI& sei)
{
  switch (sei.payloadType())
  {
  case SEI::USER_DATA_UNREGISTERED:
    xWriteSEIuserDataUnregistered(*static_cast<const SEIuserDataUnregistered*>(&sei));
    break;
  case SEI::ACTIVE_PARAMETER_SETS:
    xWriteSEIActiveParameterSets(*static_cast<const SEIActiveParameterSets*>(& sei)); 
    break; 
  case SEI::DECODED_PICTURE_HASH:
    xWriteSEIDecodedPictureHash(*static_cast<const SEIDecodedPictureHash*>(&sei));
    break;
  case SEI::BUFFERING_PERIOD:
    xWriteSEIBufferingPeriod(*static_cast<const SEIBufferingPeriod*>(&sei));
    break;
  case SEI::PICTURE_TIMING:
    xWriteSEIPictureTiming(*static_cast<const SEIPictureTiming*>(&sei));
    break;
  case SEI::RECOVERY_POINT:
    xWriteSEIRecoveryPoint(*static_cast<const SEIRecoveryPoint*>(&sei));
    break;
  default:
    assert(!"Unhandled SEI message");
  }
}

/**
 * marshal a single SEI message sei, storing the marshalled representation
 * in bitstream bs.
 */
Void SEIWriter::writeSEImessage(TComBitIf& bs, const SEI& sei)
{
  /* calculate how large the payload data is */
  /* TODO: this would be far nicer if it used vectored buffers */
  TComBitCounter bs_count;
  bs_count.resetBits();
  setBitstream(&bs_count);

#if ENC_DEC_TRACE
  g_HLSTraceEnable = false;
#endif
  xWriteSEIpayloadData(sei);
#if ENC_DEC_TRACE
  g_HLSTraceEnable = true;
#endif
  UInt payload_data_num_bits = bs_count.getNumberOfWrittenBits();
  assert(0 == payload_data_num_bits % 8);

  setBitstream(&bs);

#if ENC_DEC_TRACE
  xTraceSEIHeader();
#endif

  UInt payloadType = sei.payloadType();
  for (; payloadType >= 0xff; payloadType -= 0xff)
  {
    WRITE_CODE(0xff, 8, "payload_type");
  }
  WRITE_CODE(payloadType, 8, "payload_type");

  UInt payloadSize = payload_data_num_bits/8;
  for (; payloadSize >= 0xff; payloadSize -= 0xff)
  {
    WRITE_CODE(0xff, 8, "payload_size");
  }
  WRITE_CODE(payloadSize, 8, "payload_size");

  /* payloadData */
#if ENC_DEC_TRACE
  xTraceSEIMessageType(sei.payloadType());
#endif

  xWriteSEIpayloadData(sei);
}

/**
 * marshal a user_data_unregistered SEI message sei, storing the marshalled
 * representation in bitstream bs.
 */
Void SEIWriter::xWriteSEIuserDataUnregistered(const SEIuserDataUnregistered &sei)
{
  for (UInt i = 0; i < 16; i++)
  {
    WRITE_CODE(sei.uuid_iso_iec_11578[i], 8 , "sei.uuid_iso_iec_11578[i]");
  }

  for (UInt i = 0; i < sei.userDataLength; i++)
  {
    WRITE_CODE(sei.userData[i], 8 , "user_data");
  }
}

/**
 * marshal a decoded picture hash SEI message, storing the marshalled
 * representation in bitstream bs.
 */
Void SEIWriter::xWriteSEIDecodedPictureHash(const SEIDecodedPictureHash& sei)
{
  UInt val;

  WRITE_CODE(sei.method, 8, "hash_type");

  for(Int yuvIdx = 0; yuvIdx < 3; yuvIdx++)
  {
    if(sei.method == SEIDecodedPictureHash::MD5)
    {
      for (UInt i = 0; i < 16; i++)
      {
        WRITE_CODE(sei.digest[yuvIdx][i], 8, "picture_md5");
      }
    }
    else if(sei.method == SEIDecodedPictureHash::CRC)
    {
      val = (sei.digest[yuvIdx][0] << 8)  + sei.digest[yuvIdx][1];
      WRITE_CODE(val, 16, "picture_crc");
    }
    else if(sei.method == SEIDecodedPictureHash::CHECKSUM)
    {
      val = (sei.digest[yuvIdx][0] << 24)  + (sei.digest[yuvIdx][1] << 16) + (sei.digest[yuvIdx][2] << 8) + sei.digest[yuvIdx][3];
      WRITE_CODE(val, 32, "picture_checksum");
    }
  }
}

Void SEIWriter::xWriteSEIActiveParameterSets(const SEIActiveParameterSets& sei)
{
  WRITE_CODE(sei.activeVPSId, 4, "active_vps_id");
  WRITE_CODE(sei.activeSPSIdPresentFlag, 1, "active_sps_id_present_flag");

  if (sei.activeSPSIdPresentFlag) 
  {
    WRITE_UVLC(sei.activeSeqParamSetId, "active_seq_param_set_id"); 
  }

  WRITE_CODE(sei.activeParamSetSEIExtensionFlag, 1, "active_param_set_sei_extension_flag"); 

  UInt uiBits = m_pcBitIf->getNumberOfWrittenBits();
  UInt uiAlignedBits = ( 8 - (uiBits&7) ) % 8;  
  if(uiAlignedBits) 
  {
    WRITE_FLAG(1, "alignment_bit" );
    uiAlignedBits--; 
    while(uiAlignedBits--)
    {
      WRITE_FLAG(0, "alignment_bit" );
    }
  }
}

Void SEIWriter::xWriteSEIBufferingPeriod(const SEIBufferingPeriod& sei)
{
  Int i, nalOrVcl;
  TComVUI *vui = sei.m_sps->getVuiParameters();
  WRITE_UVLC( sei.m_seqParameterSetId, "seq_parameter_set_id" );
  if( !vui->getSubPicCpbParamsPresentFlag() )
  {
    WRITE_FLAG( sei.m_altCpbParamsPresentFlag, "alt_cpb_params_present_flag" );
  }
  for( nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
  {
    if( ( ( nalOrVcl == 0 ) && ( vui->getNalHrdParametersPresentFlag() ) ) ||
        ( ( nalOrVcl == 1 ) && ( vui->getVclHrdParametersPresentFlag() ) ) )
    {
      for( i = 0; i < ( vui->getCpbCntMinus1( 0 ) + 1 ); i ++ )
      {
        WRITE_CODE( sei.m_initialCpbRemovalDelay[i][nalOrVcl],( vui->getInitialCpbRemovalDelayLengthMinus1() + 1 ) ,           "initial_cpb_removal_delay" );
        WRITE_CODE( sei.m_initialCpbRemovalDelayOffset[i][nalOrVcl],( vui->getInitialCpbRemovalDelayLengthMinus1() + 1 ),      "initial_cpb_removal_delay_offset" );
        if( vui->getSubPicCpbParamsPresentFlag() || sei.m_altCpbParamsPresentFlag )
        {
          WRITE_CODE( sei.m_initialAltCpbRemovalDelay[i][nalOrVcl], ( vui->getInitialCpbRemovalDelayLengthMinus1() + 1 ) ,     "initial_alt_cpb_removal_delay" );
          WRITE_CODE( sei.m_initialAltCpbRemovalDelayOffset[i][nalOrVcl], ( vui->getInitialCpbRemovalDelayLengthMinus1() + 1 ),"initial_alt_cpb_removal_delay_offset" );
        }
      }
    }
  }
  xWriteByteAlign();
}
Void SEIWriter::xWriteSEIPictureTiming(const SEIPictureTiming& sei)
{
  Int i;
  TComVUI *vui = sei.m_sps->getVuiParameters();

  if( !vui->getNalHrdParametersPresentFlag() && !vui->getVclHrdParametersPresentFlag() )
    return;

  WRITE_CODE( sei.m_auCpbRemovalDelay, ( vui->getCpbRemovalDelayLengthMinus1() + 1 ),                                         "au_cpb_removal_delay" );
  WRITE_CODE( sei.m_picDpbOutputDelay, ( vui->getDpbOutputDelayLengthMinus1() + 1 ),                                          "pic_dpb_output_delay" );
  if( sei.m_sps->getVuiParameters()->getSubPicCpbParamsPresentFlag() )
  {
    WRITE_UVLC( sei.m_numDecodingUnitsMinus1,     "num_decoding_units_minus1" );
    WRITE_FLAG( sei.m_duCommonCpbRemovalDelayFlag, "du_common_cpb_removal_delay_flag" );
    if( sei.m_duCommonCpbRemovalDelayFlag )
    {
      WRITE_CODE( sei.m_duCommonCpbRemovalDelayMinus1, ( vui->getDuCpbRemovalDelayLengthMinus1() + 1 ),                       "du_common_cpb_removal_delay_minus1" );
    }
    else
    {
      for( i = 0; i < ( sei.m_numDecodingUnitsMinus1 + 1 ); i ++ )
      {
        WRITE_UVLC( sei.m_numNalusInDuMinus1[ i ],  "num_nalus_in_du_minus1");
        WRITE_CODE( sei.m_duCpbRemovalDelayMinus1[ i ], ( vui->getDuCpbRemovalDelayLengthMinus1() + 1 ),                        "du_cpb_removal_delay_minus1" );
      }
    }
  }
  xWriteByteAlign();
}
Void SEIWriter::xWriteSEIRecoveryPoint(const SEIRecoveryPoint& sei)
{
  WRITE_SVLC( sei.m_recoveryPocCnt,    "recovery_poc_cnt"    );
  WRITE_FLAG( sei.m_exactMatchingFlag, "exact_matching_flag" );
  WRITE_FLAG( sei.m_brokenLinkFlag,    "broken_link_flag"    );
  xWriteByteAlign();
}

Void SEIWriter::xWriteByteAlign()
{
  if( m_pcBitIf->getNumberOfWrittenBits() % 8 != 0)
  {
    WRITE_FLAG( 1, "bit_equal_to_one" );
    while( m_pcBitIf->getNumberOfWrittenBits() % 8 != 0 )
    {
      WRITE_FLAG( 0, "bit_equal_to_zero" );
    }
  }
};

//! \}
