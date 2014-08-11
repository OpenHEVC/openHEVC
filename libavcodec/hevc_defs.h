/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
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

#ifndef AVCODEC_HEVC_DEF_H
#define AVCODEC_HEVC_DEF_H

#define SVC_EXTENSION   1
#define VPS_EXTN_OFFSET                  1      ///< implementation of vps_extension_offset syntax element

#define VPS_EXTENSION   1
#define MAX_NUM_LAYER_IDS                64
#define MAX_LAYERS                       63
#define MAX_VPS_OP_LAYER_SETS_PLUS1      63
#define MAX_TLAYER                       8           ///< max number of temporal layer

#define MAX_VPS_NUM_SCALABILITY_TYPES    16
#define MAX_VPS_LAYER_ID_PLUS1           MAX_LAYERS
#define MAX_VPS_ADD_OUTPUT_LAYER_SETS    1024

#define FRAME_CONCEALMENT                0
#define SIM_ERROR_CONCEALMENT            0

#if FRAME_CONCEALMENT
#define COPY_MV                              1
#endif

#endif /* AVCODEC_HEVC_DEF_H */
