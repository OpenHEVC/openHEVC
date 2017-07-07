/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi
 *               2015 Morgan LACOUR <morgan.lacour@insa-rennes.fr
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

.macro load_coeffs_qpel_g3_0 offset dir
    .if \dir == dir_h
        sub       r2, #1
    .endif
    .if \dir == dir_v
        sub       r2, r3
    .endif
    .if \dir == dir_hv
        sub       r2, #1
    .endif
        lsrs      r11, \offset, #2
        lsl       \offset, #2
        ldr       r12, =qpel_coeffs_g3
        add       r12, \offset
        ldr       \offset, [r12]
        vdup.i8   d6, \offset
        lsr       \offset, #8
        vdup.i8   d7, \offset
        lsr       \offset, #8
        vdup.i8   d8, \offset
.endm

.macro load_coeffs_qpel_g3_1 offset dir
        lsrs      r11, \offset, #2
        lsl       \offset, #2
        ldr       r12, =qpel_coeffs_g3
        add       r12, \offset
        ldr       \offset, [r12]
        vdup.i8   d6, \offset
        lsr       \offset, #8
        vdup.i8   d7, \offset
        lsr       \offset, #8
        vdup.i8   d8, \offset
.endm

.macro load_coeffs_32_qpel_g3_0 offset
        sub      r2, r3
        lsl      \offset, #2
        ldr      r12, =qpel_coeffs_g3
        add      r12, \offset
        ldr      \offset, [r12]
        vmov.i64 d0, #0
        vmov.8   d0[0], \offset
        lsr      \offset, #8
        vmov.8   d0[2], \offset
        lsr      \offset, #8
        vmov.8   d0[4], \offset
.endm

.macro load_coeffs_32_qpel_g3_1 offset
        lsl      \offset, #2
        ldr      r12, =qpel_coeffs_g3
        add      r12, \offset
        ldr      \offset, [r12]
        vmov.i64 d0, #0
        vmov.8   d0[0], \offset
        lsr      \offset, #8
        vmov.8   d0[2], \offset
        lsr      \offset, #8
        vmov.8   d0[4], \offset
.endm

.macro preload_data_v_qpel_g3
        pld [r2]
        vld1.8    {d3}, [r2], r3
        pld [r2]
        vld1.8    {d4}, [r2], r3
.endm

.macro preload_data_hv_qpel_g3_0
        load_data_h8_qpel_g3
        exec_filter_qpel_g3_0 q8
        load_data_h8_qpel_g3
        exec_filter_qpel_g3_0 q7
        load_data_h8_qpel_g3
        exec_filter_qpel_g3_0 q6
.endm

.macro preload_data_hv_qpel_g3_1
        load_data_h8_qpel_g3
        exec_filter_qpel_g3_1 q8
        load_data_h8_qpel_g3
        exec_filter_qpel_g3_1 q7
        load_data_h8_qpel_g3
        exec_filter_qpel_g3_1 q6
.endm

.macro load_data_h8_qpel_g3
        pld      [r2]
        vld1.8   {q1}, [r2], r3
        vext.8   d4, d2, d3, #2
        vext.8   d3, d2, d3, #1
.endm

.macro load_data_h42_qpel_g3
        pld      [r2]
        vld1.8   d2, [r2], r3
        vext.8   d3, d2, d2, #1
        vext.8   d4, d2, d2, #2
.endm

.macro load_data_v_qpel_g3
        pld     [r2]
        vmov    d2, d3
        vmov    d3, d4
        vld1.8  {d4}, [r2], r3
.endm

.macro load_data_hv8_qpel_g3
        pld      [r2]
        vld1.8   {q1}, [r2], r3
        vext.8   d4, d2, d3, #2
        vext.8   d3, d2, d3, #1
        vmov     q9, q8
        vmov     q8, q7
        vmov     q7, q6
.endm

.macro load_data_hv42_qpel_g3
        pld      [r2]
        vld1.8   d2, [r2], r3
        vext.8   d3, d2, d2, #1
        vext.8   d4, d2, d2, #2
        vmov     q9, q8
        vmov     q8, q7
        vmov     q7, q6
.endm

.macro exec_filter_qpel_g3_0 out
        vmull.u8  \out, d8, d4
        vmlal.u8  \out, d7, d3
        vmlsl.u8  \out, d6, d2
.endm

.macro exec_filter_qpel_g3_1 out
        vmull.u8  \out, d7, d3
        vmlal.u8  \out, d6, d2
        vmlsl.u8  \out, d8, d4
.endm

.macro exec_filter_hv8_qpel_g3_0 out1 out2
        vmull.s16 q14,d14,d0[2] // q7
        vmull.s16 q15,d15,d0[2]
        vmlal.s16 q14,d16,d0[1] // q8
        vmlal.s16 q15,d17,d0[1]
        vmlsl.s16 q14,d18,d0[0] // q9
        vmlsl.s16 q15,d19,d0[0]
        vqshrn.s32 \out1,q14,#6
        vqshrn.s32 \out2,q15,#6
.endm

.macro exec_filter_hv42_qpel_g3_0 out
        vmull.s16 q14,d14,d0[2] // q7
        vmlal.s16 q14,d16,d0[1] // q8
        vmlsl.s16 q14,d18,d0[0] // q9
        vqshrn.s32 \out,q14,#6
.endm

.macro exec_filter_hv8_qpel_g3_1 out1 out2
        vmull.s16 q14,d18,d0[0] // q9
        vmull.s16 q15,d19,d0[0]
        vmlal.s16 q14,d16,d0[1] // q8
        vmlal.s16 q15,d17,d0[1]
        vmlsl.s16 q14,d14,d0[2] // q7
        vmlsl.s16 q15,d15,d0[2]
        vqshrn.s32 \out1,q14,#6
        vqshrn.s32 \out2,q15,#6
.endm

.macro exec_filter_hv42_qpel_g3_1 out
        vmull.s16 q14,d18,d0[0] // q9
        vmlal.s16 q14,d16,d0[1] // q8
        vmlsl.s16 q14,d14,d0[2] // q7
        vqshrn.s32 \out,q14,#6
.endm

function ff_hevc_put_qpel3_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #4] // mx
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_pre dir_h  \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
11:     loop_pel_neon_8 fun_pre dir_h  \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc

function ff_hevc_qpel3_uni_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #8] // mx
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_uni dir_h  \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
11:     loop_pel_neon_8 fun_uni dir_h  \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc

function ff_hevc_qpel3_bi_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #12] // mx
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_bi dir_h   \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
11:     loop_pel_neon_8 fun_bi dir_h   \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc

.ltorg

function ff_hevc_put_qpel3_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #8] // my
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_pre dir_v  \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
11:     loop_pel_neon_8 fun_pre dir_v  \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc

function ff_hevc_qpel3_uni_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #12] // my
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_uni dir_v  \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
11:     loop_pel_neon_8 fun_uni dir_v  \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
19:
endfunc

function ff_hevc_qpel3_bi_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #16] // my
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_bi dir_v   \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
11:     loop_pel_neon_8 fun_bi dir_v   \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc

.ltorg

function ff_hevc_put_qpel3_hv_neon_8, export=1
        push   {r6-r7}
        ldr    r6, [sp, #8] // mx
        ldr    r7, [sp, #12] // my
        sub    r6, #1
        sub    r7, #1
        lsrs   r7, #1
        bne 21f
20:     lsrs   r6, #1
        pop   {r6-r7}
        bne 201f
	    b 200f
21:     lsrs   r6, #1
        pop   {r6-r7}
        bne 211f
	    b 210f

200:    loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
201:    loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
210:    loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
211:    loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc
.ltorg

function ff_hevc_qpel3_uni_hv_neon_8, export=1
        push   {r6-r7}
        ldr    r6, [sp, #12] // mx
        ldr    r7, [sp, #16] // my
        sub    r6, #1
        sub    r7, #1
        lsrs   r7, #1
        bne 21f
20:     lsrs   r6, #1
        pop   {r6-r7}
        bne 201f
        b 200f
21:     lsrs   r6, #1
        pop   {r6-r7}
        bne 211f
        b 210f

200:    loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
201:    loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
210:    loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
211:    loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc
.ltorg

function ff_hevc_qpel3_bi_hv_neon_8, export=1
        push   {r6-r7}
        ldr    r6, [sp, #16] // mx
        ldr    r7, [sp, #20] // my
        sub    r6, #1
        sub    r7, #1
        lsrs   r7, #1
        bne 21f
20:     lsrs   r6, #1
        pop   {r6-r7}
        bne 201f
        b 200f
21:     lsrs   r6, #1
        pop   {r6-r7}
        bne 211f
        b 210f

200:    loop_pel_neon_8 fun_bi dir_hv  \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
201:    loop_pel_neon_8 fun_bi dir_hv  \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_0    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_0   \
            exec_filter_hv42_qpel_g3_0
210:    loop_pel_neon_8 fun_bi dir_hv  \
            load_coeffs_qpel_g3_0       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_0   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_0       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
211:    loop_pel_neon_8 fun_bi dir_hv  \
            load_coeffs_qpel_g3_1       \
            load_coeffs_32_qpel_g3_1    \
            preload_data_v_qpel_g3      \
            preload_data_hv_qpel_g3_1   \
            load_data_h8_qpel_g3        \
            load_data_h42_qpel_g3       \
            load_data_v_qpel_g3         \
            load_data_hv8_qpel_g3       \
            load_data_hv42_qpel_g3      \
            exec_filter_qpel_g3_1       \
            exec_filter_hv8_qpel_g3_1   \
            exec_filter_hv42_qpel_g3_1
endfunc

qpel_coeffs_g3: // Added 0's for padding
       .byte   6, 58, 12, 0
       .byte   7, 42, 29, 0
       .byte  12, 58,  6, 0
