/*
 * Provide registration of all codecs, parsers and bitstream filters for libavcodec.
 * Copyright (c) 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Provide registration of all codecs, parsers and bitstream filters for libavcodec.
 */

#include "config.h"
#include "avcodec.h"
#include "version.h"

#define REGISTER_HWACCEL(X, x)                                          \
    {                                                                   \
        extern AVHWAccel ff_##x##_hwaccel;                              \
        if (CONFIG_##X##_HWACCEL)                                       \
            av_register_hwaccel(&ff_##x##_hwaccel);                     \
    }

#define REGISTER_ENCODER(X, x)                                          \
    {                                                                   \
        extern AVCodec ff_##x##_encoder;                                \
        if (CONFIG_##X##_ENCODER)                                       \
            avcodec_register(&ff_##x##_encoder);                        \
    }

#define REGISTER_DECODER(X, x)                                          \
    {                                                                   \
        extern AVCodec ff_##x##_decoder;                                \
        if (CONFIG_##X##_DECODER)                                       \
            avcodec_register(&ff_##x##_decoder);                        \
    }

#define REGISTER_ENCDEC(X, x) REGISTER_ENCODER(X, x); REGISTER_DECODER(X, x)

#define REGISTER_PARSER(X, x)                                           \
    {                                                                   \
        extern AVCodecParser ff_##x##_parser;                           \
        if (CONFIG_##X##_PARSER)                                        \
            av_register_codec_parser(&ff_##x##_parser);                 \
    }

void avcodec_register_all(void)
{
    static int initialized;

    if (initialized)
        return;
    initialized = 1;

    /* video codecs */
    //REGISTER_DECODER(H261,              h261);
    //REGISTER_DECODER(H263,              h263);
    REGISTER_DECODER(H264,              h264);
#if FF_API_VDPAU
    //REGISTER_DECODER(H264_VDPAU,        h264_vdpau);
#endif


    REGISTER_DECODER(HEVC,              hevc);
    REGISTER_DECODER(SHVC,              shvc);

    //REGISTER_DECODER(MPEGVIDEO,         mpegvideo);
    //REGISTER_DECODER(MPEG4,             mpeg4);

    //REGISTER_DECODER(VP3,               vp3);
    //REGISTER_ENCODER(SNOW,              snow);
    

    /* hardware acceleration */
    //REGISTER_HWACCEL(H264_D3D11VA,      h264_d3d11va);
    //REGISTER_HWACCEL(H264_DXVA2,        h264_dxva2);
    //REGISTER_HWACCEL(H264_VAAPI,        h264_vaapi);
    //REGISTER_HWACCEL(H264_VDA,          h264_vda);
    //REGISTER_HWACCEL(H264_VDPAU,        h264_vdpau);
    //REGISTER_HWACCEL(H264_VIDEOTOOLBOX, h264_videotoolbox);


    /* parsers */
    REGISTER_PARSER(H264,               h264);
    REGISTER_PARSER(HEVC,               hevc);
    REGISTER_PARSER(SHVC,               shvc);

}
