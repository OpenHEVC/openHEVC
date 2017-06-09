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

#include "libavcodec/arm/hevcdsp_epel_green_template_neon.S"
#include "libavcodec/arm/hevcdsp_epel_put_neon.S"

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
        vdup.i8   d0, \offset
        lsr       \offset, #8
        vdup.i8   d1, \offset
        lsr       \offset, #8
        vdup.i8   d2, \offset
        lsr       \offset, #8
        vdup.i8   d3, \offset
.endm

.macro load_coeffs_32_epel offset
        lsr      r6, \offset, #2
        add      r11, r11, r6, lsl 1
        tst      r11, #2
        lsl      \offset, #2
        ldr      r12, =epel_coeffs
        add      r12, \offset
        ldr      \offset, [r12]
        vmov.i64 d4, #0
        vmov.8   d4[0], \offset
        lsr      \offset, #8
        vmov.8   d4[2], \offset
        lsr      \offset, #8
        vmov.8   d4[4], \offset
        lsr      \offset, #8
        vmov.8   d4[6], \offset
.endm

.macro preload_data_v_epel
        pld [r2]
        vld1.8    {d17}, [r2], r3
        pld [r2]
        vld1.8    {d18}, [r2], r3
        pld [r2]
        vld1.8    {d19}, [r2], r3
.endm

.macro preload_data_hv_epel
        load_data_h8_epel
        exec_filter_epel q14
        load_data_h8_epel
        exec_filter_epel q13
        load_data_h8_epel
        exec_filter_epel q12
.endm

.macro load_data_h8_epel
        pld      [r2]
        vld1.8   {q10}, [r2], r3
        vmov     d16, d20
        vext.8   d17, d20, d21, #1
        vext.8   d18, d20, d21, #2
        vext.8   d19, d20, d21, #3
.endm

.macro load_data_h42_epel
        pld      [r2]
        vld1.8   d16, [r2], r3
        vext.8   d17, d16, d16, #1
        vext.8   d18, d16, d16, #2
        vext.8   d19, d16, d16, #3
.endm

.macro load_data_v_epel
        pld     [r2]
        vmov    d16, d17
        vmov    d17, d18
        vmov    d18, d19
        vld1.8  {d19}, [r2], r3
.endm

.macro load_data_hv8_epel
        pld       [r2]
        vld1.8    {q10}, [r2], r3
        vmov      d16, d20
        vext.8    d17, d20, d21, #1
        vext.8    d18, d20, d21, #2
        vext.8    d19, d20, d21, #3
        vmov      q15, q14
        vmov      q14, q13
        vmov      q13, q12
.endm

.macro load_data_hv42_epel
        pld      [r2]
        vld1.8    d16, [r2], r3
        vext.8    d17, d16, d16, #1
        vext.8    d18, d16, d16, #2
        vext.8    d19, d16, d16, #3
        vmov      q15, q14
        vmov      q14, q13
        vmov      q13, q12
.endm

.macro exec_filter_epel out
        vmull.u8  \out, d18, d2
        vmlal.u8  \out, d17, d1
        vmlsl.u8  \out, d16, d0
        vmlsl.u8  \out, d19, d3
.endm

.macro exec_filter_hv8_epel out1 out2
        vmull.s16 q3,d28,d4[1] // q14
        vmull.s16 q4,d29,d4[1]
        vmlal.s16 q3,d26,d4[2] // q13
        vmlal.s16 q4,d27,d4[2]
        vmlsl.s16 q3,d30,d4[0] // q15
        vmlsl.s16 q4,d31,d4[0]
        vmlsl.s16 q3,d24,d4[3] // q12
        vmlsl.s16 q4,d25,d4[3]
        vqshrn.s32 \out1,q3,#6
        vqshrn.s32 \out2,q4,#6
.endm

.macro exec_filter_hv42_epel out
        vmull.s16 q3,d28,d4[1] // q14
        vmlal.s16 q3,d26,d4[2] // q13
        vmlsl.s16 q3,d30,d4[0] // q15
        vmlsl.s16 q3,d24,d4[3] // q12
        vqshrn.s32 \out,q3,#6
.endm

function ff_hevc_put_epel_h_neon_8, export=1
        loop_epel_neon_8 fun_pre dir_h \
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
        loop_epel_neon_8 fun_uni dir_h \
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
        loop_epel_neon_8 fun_bi dir_h \
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
        loop_epel_neon_8 fun_pre dir_v \
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
        loop_epel_neon_8 fun_uni dir_v \
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
        loop_epel_neon_8 fun_bi dir_v \
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
       loop_epel_neon_8 fun_pre dir_hv \
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
        loop_epel_neon_8 fun_uni dir_hv \
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
        loop_epel_neon_8 fun_bi dir_hv \
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
