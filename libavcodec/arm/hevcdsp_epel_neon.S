/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
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

#include "libavcodec/arm/hevcdsp_pel_template_neon.S"

.macro load_coeffs_epel offset dir
    .if \dir != dir_v
        sub       r2, #1
    .endif
    .if \dir != dir_h
        sub       r2, r3
    .endif
        lsrs      r11, \offset, #2
        lsl       \offset, #2
        ldr       r12, =epel_coeffs
        add       r12, \offset
        ldr       \offset, [r12]
        vdup.i8   d6, \offset
        lsr       \offset, #8
        vdup.i8   d7, \offset
        lsr       \offset, #8
        vdup.i8   d8, \offset
        lsr       \offset, #8
        vdup.i8   d9, \offset
.endm

.macro load_coeffs_32_epel offset
        lsr      r6, \offset, #2
        add      r11, r11, r6, lsl 1
        lsl      \offset, #2
        ldr      r12, =epel_coeffs
        add      r12, \offset
        ldr      \offset, [r12]
        vmov.i64 d0, #0
        vmov.8   d0[0], \offset
        lsr      \offset, #8
        vmov.8   d0[2], \offset
        lsr      \offset, #8
        vmov.8   d0[4], \offset
        lsr      \offset, #8
        vmov.8   d0[6], \offset
.endm

.macro preload_data_v_epel
        pld [r2]
        vld1.8    {d3}, [r2], r3
        pld [r2]
        vld1.8    {d4}, [r2], r3
        pld [r2]
        vld1.8    {d5}, [r2], r3
.endm

.macro preload_data_hv_epel
        load_data_h8_epel
        exec_filter_epel q9
        load_data_h8_epel
        exec_filter_epel q8
        load_data_h8_epel
        exec_filter_epel q7
        load_data_h8_epel
        exec_filter_epel q6
.endm

.macro load_data_h8_epel
        pld      [r2]
        vld1.8   {q1}, [r2], r3
        vext.8   d4, d2, d3, #2
        vext.8   d5, d2, d3, #3
        vext.8   d3, d2, d3, #1
.endm

.macro load_data_h42_epel
        pld      [r2]
        vld1.8   d2, [r2], r3
        vext.8   d3, d2, d2, #1
        vext.8   d4, d2, d2, #2
        vext.8   d5, d2, d2, #3
.endm

.macro load_data_v_epel
        pld     [r2]
        vmov    d2, d3
        vmov    d3, d4
        vmov    d4, d5
        vld1.8  {d5}, [r2], r3
.endm

.macro load_data_hv8_epel
        pld      [r2]
        vld1.8   {q1}, [r2], r3
        vext.8   d4, d2, d3, #2
        vext.8   d5, d2, d3, #3
        vext.8   d3, d2, d3, #1
        vmov     q10, q9
        vmov     q9, q8
        vmov     q8, q7
        vmov     q7, q6
.endm

.macro load_data_hv42_epel
        pld      [r2]
        vld1.8   d2, [r2], r3
        vext.8   d3, d2, d2, #1
        vext.8   d4, d2, d2, #2
        vext.8   d5, d2, d2, #3
        vmov     q10, q9
        vmov     q9, q8
        vmov     q8, q7
        vmov     q7, q6
.endm

.macro exec_filter_epel out
        vmull.u8  \out, d4, d8
        vmlal.u8  \out, d3, d7
        vmlsl.u8  \out, d2, d6
        vmlsl.u8  \out, d5, d9
.endm

.macro exec_filter_hv8_epel out1 out2
        vmull.s16 q14,d18,d0[1] // q9
        vmull.s16 q15,d19,d0[1]
        vmlal.s16 q14,d16,d0[2] // q8
        vmlal.s16 q15,d17,d0[2]
        vmlsl.s16 q14,d20,d0[0] // q10
        vmlsl.s16 q15,d21,d0[0]
        vmlsl.s16 q14,d14,d0[3] // q7
        vmlsl.s16 q15,d15,d0[3]
        vqshrn.s32 \out1,q14,#6
        vqshrn.s32 \out2,q15,#6
.endm

.macro exec_filter_hv42_epel out
        vmull.s16 q14,d18,d0[1] // q9
        vmlal.s16 q14,d16,d0[2] // q8
        vmlsl.s16 q14,d20,d0[0] // q10
        vmlsl.s16 q14,d14,d0[3] // q7
        vqshrn.s32 \out,q14,#6
.endm

function ff_hevc_put_epel_h_neon_8, export=1
        loop_pel_neon_8 fun_pre dir_h \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

function ff_hevc_epel_uni_h_neon_8, export=1
        loop_pel_neon_8 fun_uni dir_h \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

function ff_hevc_epel_bi_h_neon_8, export=1
        loop_pel_neon_8 fun_bi dir_h \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

.ltorg

function ff_hevc_put_epel_v_neon_8, export=1
        loop_pel_neon_8 fun_pre dir_v \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

function ff_hevc_epel_uni_v_neon_8, export=1
        loop_pel_neon_8 fun_uni dir_v \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

function ff_hevc_epel_bi_v_neon_8, export=1
        loop_pel_neon_8 fun_bi dir_v \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

.ltorg

function ff_hevc_put_epel_hv_neon_8, export=1
       loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

function ff_hevc_epel_uni_hv_neon_8, export=1
        loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

function ff_hevc_epel_bi_hv_neon_8, export=1
        loop_pel_neon_8 fun_bi dir_hv \
            load_coeffs_epel     \
            load_coeffs_32_epel  \
            preload_data_v_epel  \
            preload_data_hv_epel \
            load_data_h8_epel    \
            load_data_h42_epel   \
            load_data_v_epel     \
            load_data_hv8_epel   \
            load_data_hv42_epel  \
            exec_filter_epel     \
            exec_filter_hv8_epel \
            exec_filter_hv42_epel
endfunc

epel_coeffs:
       .byte 2, 58, 10, 2
       .byte 4, 54, 16, 2
       .byte 6, 46, 28, 4
       .byte 4, 36, 36, 4
       .byte 4, 28, 46, 6
       .byte 2, 16, 54, 4
       .byte 2, 10, 58, 2
