/*
 * Copyright (c) 2013 Pierre-Edouard LEPERE
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "libavutil/cpu.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include "libavcodec/get_bits.h" /* required for hevcdsp.h GetBitContext */
#include "libavcodec/hevcpred.h"
#include "libavcodec/x86/hevcpred.h"

//function declaration

void ff_hevcpred_init_x86(HEVCPredContext *c, const int bit_depth)
{
    int mm_flags = av_get_cpu_flags();

    if (bit_depth == 8) {
        if (EXTERNAL_MMX(mm_flags)) {


            if (EXTERNAL_MMXEXT(mm_flags)) {


                if (EXTERNAL_SSE2(mm_flags)) {

                }
                if (EXTERNAL_SSSE3(mm_flags)) {

                }
                if (EXTERNAL_SSE4(mm_flags)) {
                	c->intra_pred= ff_hevc_intra_pred_8_sse;

                    c->pred_planar[0]= pred_planar_0_8_sse;
                    c->pred_planar[1]= pred_planar_1_8_sse;
                    c->pred_planar[2]= pred_planar_2_8_sse;
                    c->pred_planar[3]= pred_planar_3_8_sse;

                 //   c->pred_angular[0]= pred_angular_0_8_sse;//removed because too little data = bad performance
                    c->pred_angular[1]= pred_angular_1_8_sse;
                    c->pred_angular[2]= pred_angular_2_8_sse;
                    c->pred_angular[3]= pred_angular_3_8_sse;
                }
                if (EXTERNAL_AVX(mm_flags)) {

                }
            }
        }
    } else if (bit_depth == 10) {
        if (EXTERNAL_MMX(mm_flags)) {
            if (EXTERNAL_MMXEXT(mm_flags)) {

                if (EXTERNAL_SSE2(mm_flags)) {

                }
                if (EXTERNAL_SSE4(mm_flags)) {
                }
                if (EXTERNAL_AVX(mm_flags)) {
                }
            }
        }
    }
}
