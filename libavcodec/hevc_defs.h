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

//#define SVC_EXTENSION

#ifdef SVC_EXTENSION
    #define VPS_EXTENSION
    #define SCALED_REF_LAYER_OFFSETS 1
    #define MAX_LAYERS  2
    #define PHASE_DERIVATION_IN_INTEGER 1
    #define ILP_DECODED_PICTURE 1
    #define CHROMA_UPSAMPLING   1
    #define REF_IDX_FRAMEWORK   1
    #ifdef REF_IDX_FRAMEWORK
        #define REF_IDX_ME_ZEROMV                1
        #define REF_IDX_MFM                      1
        #define JCTVC_M0458_INTERLAYER_RPS_SIG   1
        #if JCTVC_M0458_INTERLAYER_RPS_SIG
            #define ZERO_NUM_DIRECT_LAYERS       1
        #endif
    #endif
#endif

#ifdef VPS_EXTENSION
    #define MAX_VPS_NUM_SCALABILITY_TYPES       16
    #define MAX_VPS_LAYER_ID_PLUS1              MAX_LAYERS
    #define MAX_VPS_LAYER_SETS_PLUS1            1024
    #define VPS_EXTN_MASK_AND_DIM_INFO          1
    #define VPS_MOVE_DIR_DEPENDENCY_FLAG        1
    #define VPS_EXTN_DIRECT_REF_LAYERS          1
    #define VPS_EXTN_PROFILE_INFO               1
    #define VPS_PROFILE_OUTPUT_LAYERS           1
    #define VPS_EXTN_OP_LAYER_SETS              1
#endif
#define DERIVE_LAYER_ID_LIST_VARIABLES      1

#endif /* AVCODEC_HEVC_DEF_H */
