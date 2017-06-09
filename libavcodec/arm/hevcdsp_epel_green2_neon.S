/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
 *               2015 Morgan LACOUR <morgan.lacour@insa-rennes.fr>
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

#include "libavcodec/arm/hevcdsp_epel_green_template_neon.S"

.macro load_coeffs_epel_g2 offset dir
        ldr     r12, =epel_coeffs
        lsl     \offset, #1
        add     \offset, r12
        ldr     \offset, [\offset]
        vdup.i8 d0, \offset
        lsr     \offset, #8
        vdup.i8 d1, \offset
.endm

.macro load_coeffs_32_epel_g2 offset
        lsl      \offset, #1
        add      \offset, r12
        ldr      \offset, [\offset]
        vmov.i64 d4, #0
        vmov.8   d4[0], \offset
        lsr      \offset, #8
        vmov.8   d4[2], \offset
.endm

.macro preload_data_v_epel_g2
        pld     [r2]
        vld1.8  {d17}, [r2], r3
.endm

.macro preload_data_hv_epel_g2
        load_data_h8_epel_g2
        exec_filter_epel_g2 q13
        load_data_h8_epel_g2
        exec_filter_epel_g2 q12
.endm

.macro load_data_h8_epel_g2
        pld     [r2]
        vld1.8  {q10}, [r2], r3
        vmov    d16, d20
        vext.8  d17, d20, d21, #1
.endm

.macro load_data_h42_epel_g2
        pld     [r2]
        vld1.8  d16, [r2], r3
        vext.8  d17, d16, d16, #1
.endm

.macro load_data_v8_epel_g2
        pld     [r2]
        vmov    d16, d17
        vld1.8  {d17}, [r2], r3
.endm

.macro load_data_hv8_epel_g2
        pld     [r2]
        vld1.8  {q10}, [r2], r3
        vmov    d16, d20
        vext.8  d17, d20, d21, #1
        vmov    q14, q13
        vmov    q13, q12
.endm

.macro load_data_hv42_epel_g2
        pld     [r2]
        vld1.8  d16, [r2], r3
        vext.8  d17, d16, d16, #1
        vmov    q14, q13
        vmov    q13, q12
.endm

.macro exec_filter_epel_g2 out
        vmull.u8 \out, d16, d0
        vmlal.u8 \out, d17, d1
.endm

.macro exec_filter_hv8_epel_g2 out1 out2
        vmull.s16  q3, d28, d4[0]
        vmull.s16  q4, d29, d4[0]
        vmlal.s16  q3, d26, d4[1]
        vmlal.s16  q4, d27, d4[1]
        vqshrn.s32 \out1, q3, #6
        vqshrn.s32 \out2, q4, #6
.endm

.macro exec_filter_hv42_epel_g2 out
        vmull.s16  q3, d28, d4[0]
        vmlal.s16  q3, d26, d4[1]
        vqshrn.s32 \out, q3, #6
.endm

function ff_hevc_put_epel2_h_neon_8, export=1
        loop_epel_neon_8 fun_pre dir_h \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_epel2_uni_h_neon_8, export=1
        loop_epel_neon_8 fun_uni dir_h \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_epel2_bi_h_neon_8, export=1
        loop_epel_neon_8 fun_bi dir_h \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_put_epel2_v_neon_8, export=1
        loop_epel_neon_8 fun_pre dir_v \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_epel2_uni_v_neon_8, export=1
        loop_epel_neon_8 fun_uni dir_v \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_epel2_bi_v_neon_8, export=1
        loop_epel_neon_8 fun_bi dir_v \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_put_epel2_hv_neon_8, export=1
        loop_epel_neon_8 fun_pre dir_hv \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_epel2_uni_hv_neon_8, export=1
        loop_epel_neon_8 fun_uni dir_hv \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

function ff_hevc_epel2_bi_hv_neon_8, export=1
        loop_epel_neon_8 fun_bi dir_hv \
            load_coeffs_epel_g2     \
            load_coeffs_32_epel_g2  \
            preload_data_v_epel_g2  \
            preload_data_hv_epel_g2 \
            load_data_h8_epel_g2    \
            load_data_h42_epel_g2   \
            load_data_v8_epel_g2     \
            load_data_hv8_epel_g2   \
            load_data_hv42_epel_g2  \
            exec_filter_epel_g2     \
            exec_filter_hv8_epel_g2 \
            exec_filter_hv42_epel_g2
endfunc

epel_coeffs:
       .byte 57,  7
       .byte 49, 15
       .byte 41, 23
       .byte 32, 32
       .byte 23, 41
       .byte 15, 49
       .byte  7, 57
