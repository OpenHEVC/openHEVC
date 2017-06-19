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

#include "libavcodec/arm/hevcdsp_epel_green_template_neon.S"

/*      r0 : dst
        r1 : dst_stride
        r2 : src
        r3 : src_stride
        r4 : height
        r5 : width
        r6 : mx / @dst save
        r7 : my / @src save
        r8 : src2
        r9 : src2_stride
        r10 : @src2 save
        r11 : 16b_shift_coeff
        r12 : @coeff / height save
 */

.macro load_coeffs_epel_g3_0 offset dir
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
        ldr       r12, =epel_coeffs_g3
        add       r12, \offset
        ldr       \offset, [r12]
        vdup.i8   d0, \offset
        lsr       \offset, #8
        vdup.i8   d1, \offset
        lsr       \offset, #8
        vdup.i8   d2, \offset
.endm

.macro load_coeffs_epel_g3_1 offset dir
        lsrs      r11, \offset, #2
        lsl       \offset, #2
        ldr       r12, =epel_coeffs_g3
        add       r12, \offset
        ldr       \offset, [r12]
        vdup.i8   d0, \offset
        lsr       \offset, #8
        vdup.i8   d1, \offset
        lsr       \offset, #8
        vdup.i8   d2, \offset
.endm

.macro load_coeffs_32_epel_g3_0 offset
        sub      r2, r3
        lsl      \offset, #2
        ldr      r12, =epel_coeffs_g3
        add      r12, \offset
        ldr      \offset, [r12]
        vmov.i64 d4, #0
        vmov.8   d4[0], \offset
        lsr      \offset, #8
        vmov.8   d4[2], \offset
        lsr      \offset, #8
        vmov.8   d4[4], \offset
.endm

.macro load_coeffs_32_epel_g3_1 offset
        lsl      \offset, #2
        ldr      r12, =epel_coeffs_g3
        add      r12, \offset
        ldr      \offset, [r12]
        vmov.i64 d4, #0
        vmov.8   d4[0], \offset
        lsr      \offset, #8
        vmov.8   d4[2], \offset
        lsr      \offset, #8
        vmov.8   d4[4], \offset
.endm

.macro preload_data_v_epel_g3
        pld [r2]
        vld1.8    {d17}, [r2], r3
        pld [r2]
        vld1.8    {d18}, [r2], r3
.endm

.macro preload_data_hv_epel_g3_0
        load_data_h8_epel_g3
        exec_filter_epel_g3_0 q14
        load_data_h8_epel_g3
        exec_filter_epel_g3_0 q13
        load_data_h8_epel_g3
        exec_filter_epel_g3_0 q12
.endm

.macro preload_data_hv_epel_g3_1
        load_data_h8_epel_g3
        exec_filter_epel_g3_1 q14
        load_data_h8_epel_g3
        exec_filter_epel_g3_1 q13
        load_data_h8_epel_g3
        exec_filter_epel_g3_1 q12
.endm

.macro load_data_h8_epel_g3
        pld      [r2]
        vld1.8   {q10}, [r2], r3
        vmov     d16, d20
        vext.8   d17, d20, d21, #1
        vext.8   d18, d20, d21, #2
.endm

.macro load_data_h42_epel_g3
        pld      [r2]
        vld1.8   d16, [r2], r3
        vext.8   d17, d16, d16, #1
        vext.8   d18, d16, d16, #2
.endm

.macro load_data_v_epel_g3
        pld     [r2]
        vmov    d16, d17
        vmov    d17, d18
        vld1.8  {d18}, [r2], r3
.endm

.macro load_data_hv8_epel_g3
        pld       [r2]
        vld1.8    {q10}, [r2], r3
        vmov      d16, d20
        vext.8    d17, d20, d21, #1
        vext.8    d18, d20, d21, #2
        vmov      q15, q14
        vmov      q14, q13
        vmov      q13, q12
.endm

.macro load_data_hv42_epel_g3
        pld      [r2]
        vld1.8    d16, [r2], r3
        vext.8    d17, d16, d16, #1
        vext.8    d18, d16, d16, #2
        vmov      q15, q14
        vmov      q14, q13
        vmov      q13, q12
.endm

.macro exec_filter_epel_g3_0 out
        vmull.u8  \out, d18, d2
        vmlal.u8  \out, d17, d1
        vmlsl.u8  \out, d16, d0
.endm

.macro exec_filter_epel_g3_1 out
        vmull.u8  \out, d17, d1
        vmlal.u8  \out, d16, d0
        vmlsl.u8  \out, d18, d2
.endm

.macro exec_filter_hv8_epel_g3_0 out1 out2
        vmull.s16 q3,d26,d4[2] // q13
        vmull.s16 q4,d27,d4[2]
        vmlal.s16 q3,d28,d4[1] // q14
        vmlal.s16 q4,d29,d4[1]
        vmlsl.s16 q3,d30,d4[0] // q15
        vmlsl.s16 q4,d31,d4[0]
        vqshrn.s32 \out1,q3,#6
        vqshrn.s32 \out2,q4,#6
.endm

.macro exec_filter_hv42_epel_g3_0 out
        vmull.s16 q3,d26,d4[2] // q13
        vmlal.s16 q3,d28,d4[1] // q14
        vmlsl.s16 q3,d30,d4[0] // q15
        vqshrn.s32 \out,q3,#6
.endm

.macro exec_filter_hv8_epel_g3_1 out1 out2
        vmull.s16 q3,d30,d4[0] // q15
        vmull.s16 q4,d31,d4[0]
        vmlal.s16 q3,d28,d4[1] // q14
        vmlal.s16 q4,d29,d4[1]
        vmlsl.s16 q3,d26,d4[2] // q13
        vmlsl.s16 q4,d27,d4[2]
        vqshrn.s32 \out1,q3,#6
        vqshrn.s32 \out2,q4,#6
.endm

.macro exec_filter_hv42_epel_g3_1 out
        vmull.s16 q3,d30,d4[0] // q15
        vmlal.s16 q3,d28,d4[1] // q14
        vmlsl.s16 q3,d26,d4[2] // q13
        vqshrn.s32 \out,q3,#6
.endm

function ff_hevc_put_epel3_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #4] // mx
        sub r6, #1
        lsrs r6, r6, #2
        pop {r6}
        bne 11f

10:     loop_epel_neon_8 fun_pre dir_h  \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
11:     loop_epel_neon_8 fun_pre dir_h  \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc

function ff_hevc_epel3_uni_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #8] // mx
        sub r6, #1
        lsrs r6, r6, #2
        pop {r6}
        bne 11f

10:     loop_epel_neon_8 fun_uni dir_h  \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
11:     loop_epel_neon_8 fun_uni dir_h  \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc

function ff_hevc_epel3_bi_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #12] // mx
        sub r6, #1
        lsrs r6, r6, #2
        pop {r6}
        bne 11f

10:     loop_epel_neon_8 fun_bi dir_h   \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
11:     loop_epel_neon_8 fun_bi dir_h   \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc

.ltorg

function ff_hevc_put_epel3_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #8] // my
        sub r6, #1
        lsrs r6, r6, #2
        pop {r6}
        bne 11f

10:     loop_epel_neon_8 fun_pre dir_v  \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
11:     loop_epel_neon_8 fun_pre dir_v  \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc

function ff_hevc_epel3_uni_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #12] // my
        sub r6, #1
        lsrs r6, r6, #2
        pop {r6}
        bne 11f

10:     loop_epel_neon_8 fun_uni dir_v  \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
11:     loop_epel_neon_8 fun_uni dir_v  \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
19:
endfunc

function ff_hevc_epel3_bi_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #16] // my
        sub r6, #1
        lsrs r6, r6, #2
        pop {r6}
        bne 11f

10:     loop_epel_neon_8 fun_bi dir_v   \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
11:     loop_epel_neon_8 fun_bi dir_v   \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc

.ltorg

function ff_hevc_put_epel3_hv_neon_8, export=1
        push   {r6-r7}
        ldr    r6, [sp, #8] // mx
        ldr    r7, [sp, #12] // my
        sub    r6, #1
        sub    r7, #1
        lsrs   r7, #2
        bne 21f
20:     lsrs   r6, #2
        pop   {r6-r7}
        bne 201f
	    b 200f
21:     lsrs   r6, #2
        pop   {r6-r7}
        bne 211f
	    b 210f

200:    loop_epel_neon_8 fun_pre dir_hv \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
201:    loop_epel_neon_8 fun_pre dir_hv \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
210:    loop_epel_neon_8 fun_pre dir_hv \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
211:    loop_epel_neon_8 fun_pre dir_hv \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc
.ltorg

function ff_hevc_epel3_uni_hv_neon_8, export=1
        push   {r6-r7}
        ldr    r6, [sp, #12] // mx
        ldr    r7, [sp, #16] // my
        sub    r6, #1
        sub    r7, #1
        lsrs   r7, #2
        bne 21f
20:     lsrs   r6, #2
        pop   {r6-r7}
        bne 201f
        b 200f
21:     lsrs   r6, #2
        pop   {r6-r7}
        bne 211f
        b 210f

200:    loop_epel_neon_8 fun_uni dir_hv \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
201:    loop_epel_neon_8 fun_uni dir_hv \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
210:    loop_epel_neon_8 fun_uni dir_hv \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
211:    loop_epel_neon_8 fun_uni dir_hv \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc
.ltorg

function ff_hevc_epel3_bi_hv_neon_8, export=1
        push   {r6-r7}
        ldr    r6, [sp, #16] // mx
        ldr    r7, [sp, #20] // my
        sub    r6, #1
        sub    r7, #1
        lsrs   r7, #2
        bne 21f
20:     lsrs   r6, #2
        pop   {r6-r7}
        bne 201f
        b 200f
21:     lsrs   r6, #2
        pop   {r6-r7}
        bne 211f
        b 210f

200:    loop_epel_neon_8 fun_bi dir_hv  \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
201:    loop_epel_neon_8 fun_bi dir_hv  \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_0    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_0   \
            exec_filter_hv42_epel_g3_0
210:    loop_epel_neon_8 fun_bi dir_hv  \
            load_coeffs_epel_g3_0       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_0   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_0       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
211:    loop_epel_neon_8 fun_bi dir_hv  \
            load_coeffs_epel_g3_1       \
            load_coeffs_32_epel_g3_1    \
            preload_data_v_epel_g3      \
            preload_data_hv_epel_g3_1   \
            load_data_h8_epel_g3        \
            load_data_h42_epel_g3       \
            load_data_v_epel_g3         \
            load_data_hv8_epel_g3       \
            load_data_hv42_epel_g3      \
            exec_filter_epel_g3_1       \
            exec_filter_hv8_epel_g3_1   \
            exec_filter_hv42_epel_g3_1
endfunc

epel_coeffs_g3: // Added 0's for padding
       .byte   3, 61,  6, 0
       .byte   5, 56, 13, 0
       .byte   7, 50, 21, 0
       .byte   7, 41, 30, 0
       .byte  21, 50,  7, 0
       .byte  13, 56,  5, 0
       .byte   6, 61,  3, 0

