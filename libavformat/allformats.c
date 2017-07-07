/*
 * Register all the formats and protocols
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
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

#include "avformat.h"
#include "rtp.h"
#include "rdt.h"
#include "url.h"
#include "version.h"

#define REGISTER_MUXER(X, x)                                            \
    {                                                                   \
        extern AVOutputFormat ff_##x##_muxer;                           \
        if (CONFIG_##X##_MUXER)                                         \
            av_register_output_format(&ff_##x##_muxer);                 \
    }

#define REGISTER_DEMUXER(X, x)                                          \
    {                                                                   \
        extern AVInputFormat ff_##x##_demuxer;                          \
        if (CONFIG_##X##_DEMUXER)                                       \
            av_register_input_format(&ff_##x##_demuxer);                \
    }

#define REGISTER_MUXDEMUX(X, x) REGISTER_MUXER(X, x); REGISTER_DEMUXER(X, x)

void av_register_all(void)
{
    static int initialized;

    if (initialized)
        return;

    avcodec_register_all();

    /* (de)muxers */

    //REGISTER_MUXDEMUX(DATA,             data);

    //REGISTER_MUXDEMUX(DV,               dv);

    //REGISTER_MUXDEMUX(H261,             h261);
    //REGISTER_MUXDEMUX(H263,             h263);
    REGISTER_DEMUXER(H264,             h264);
    //REGISTER_MUXER   (HASH,             hash);
    //REGISTER_MUXER   (HDS,              hds);
    REGISTER_DEMUXER(HEVC,             hevc);
    //REGISTER_DEMUXER(SHVC,             shvc);
    //REGISTER_MUXDEMUX(HLS,              hls);
    //REGISTER_DEMUXER (HNM,              hnm);

    //REGISTER_MUXDEMUX(IMAGE2,           image2);

    //REGISTER_DEMUXER (LOAS,             loas);

    //REGISTER_MUXDEMUX(MJPEG,            mjpeg);

    REGISTER_DEMUXER (MPEGVIDEO,        mpegvideo);
    REGISTER_DEMUXER (MOV,        mov);
    REGISTER_DEMUXER (MPEGTS,        mpegts);



    initialized = 1;
}
