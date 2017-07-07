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

.macro load_coeffs_qpel_g5_0 offset dir
    .if \dir == dir_h
        sub       r2, #2
    .endif
    .if \dir == dir_v
        sub       r2, r3
        sub       r2, r3
    .endif
    .if \dir == dir_hv
        sub       r2, #2
        sub       r2, r3
    .endif
        lsrs      r11, \offset, #2
        lsl       \offset, #3
        ldr       r12, =qpel_coeffs_g5
        add       r12, \offset
        ldr       \offset, [r12]
        vdup.i8   d8, \offset
        lsr       \offset, #8
        vdup.i8   d9, \offset
        lsr       \offset, #8
        vdup.i8   d24, \offset
        lsr       \offset, #8
        vdup.i8   d25, \offset
        ldr       \offset, [r12, #4]
        vdup.i8   d26, \offset
.endm

.macro load_coeffs_qpel_g5_1 offset dir
    .if \dir == dir_h
        sub       r2, #1
    .endif
    .if \dir == dir_v
        sub       r2, r3
    .endif
    .if \dir == dir_hv
        sub       r2, #1
        sub       r2, r3
    .endif
        lsrs      r11, \offset, #2
        lsl       \offset, #3
        ldr       r12, =qpel_coeffs_g5
        add       r12, \offset
        ldr       \offset, [r12]
        vdup.i8   d8, \offset
        lsr       \offset, #8
        vdup.i8   d9, \offset
        lsr       \offset, #8
        vdup.i8   d24, \offset
        lsr       \offset, #8
        vdup.i8   d25, \offset
        ldr       \offset, [r12, #4]
        vdup.i8   d26, \offset
.endm

.macro load_coeffs_32_qpel_g5_0 offset
        sub      r2, r3
        lsl      \offset, #3
        ldr      r12, =qpel_coeffs_g5
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
        ldr      \offset, [r12, #4]
        vmov.i64 d1, #0
        vmov.8   d1[0], \offset
.endm

.macro load_coeffs_32_qpel_g5_1 offset
        lsl      \offset, #3
        ldr      r12, =qpel_coeffs_g5
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
        ldr      \offset, [r12, #4]
        vmov.i64 d1, #0
        vmov.8   d1[0], \offset
.endm

.macro preload_data_v_qpel_g5
        pld [r2]
        vld1.8    {d3}, [r2], r3
        pld [r2]
        vld1.8    {d4}, [r2], r3
        pld [r2]
        vld1.8    {d5}, [r2], r3
        pld [r2]
        vld1.8    {d6}, [r2], r3
.endm

.macro preload_data_hv_qpel_g5_0
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_0 q10
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_0 q9
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_0 q8
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_0 q7
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_0 q6
.endm

.macro preload_data_hv_qpel_g5_1
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_1 q10
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_1 q9
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_1 q8
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_1 q7
        load_data_h8_qpel_g5
        exec_filter_qpel_g5_1 q6
.endm

.macro load_data_h8_qpel_g5
        pld      [r2]
        vld1.8   {q1}, [r2], r3
        vext.8   d4, d2, d3, #2
        vext.8   d5, d2, d3, #3
        vext.8   d6, d2, d3, #4
        vext.8   d3, d2, d3, #1
.endm

.macro load_data_h42_qpel_g5
        pld      [r2]
        vld1.8   d2, [r2], r3
        vext.8   d3, d2, d2, #1
        vext.8   d4, d2, d2, #2
        vext.8   d5, d2, d2, #3
        vext.8   d6, d2, d2, #4
.endm

.macro load_data_v_qpel_g5
        pld     [r2]
        vmov    d2, d3
        vmov    d3, d4
        vmov    d4, d5
        vmov    d5, d6
        vld1.8  {d6}, [r2], r3
.endm

.macro load_data_hv8_qpel_g5
        pld       [r2]
        vld1.8   {q1}, [r2], r3
        vext.8   d4, d2, d3, #2
        vext.8   d5, d2, d3, #3
        vext.8   d6, d2, d3, #4
        vext.8   d3, d2, d3, #1
        vmov     q11, q10
        vmov     q10,  q9
        vmov      q9,  q8
        vmov      q8,  q7
        vmov      q7,  q6
.endm

.macro load_data_hv42_qpel_g5
        pld      [r2]
        vld1.8   d2, [r2], r3
        vext.8   d3, d2, d2, #1
        vext.8   d4, d2, d2, #2
        vext.8   d5, d2, d2, #3
        vext.8   d6, d2, d2, #4
        vmov     q11, q10
        vmov     q10,  q9
        vmov      q9,  q8
        vmov      q8,  q7
        vmov      q7,  q6
.endm

.macro exec_filter_qpel_g5_0 out
        vmull.u8  \out, d8, d2
        vmlsl.u8  \out, d9, d3
        vmlal.u8  \out, d24, d4
        vmlal.u8  \out, d25, d5
        vmlsl.u8  \out, d26, d6
.endm

.macro exec_filter_qpel_g5_1 out
        vmull.u8  \out, d9, d3
        vmlsl.u8  \out, d8, d2
        vmlal.u8  \out, d24, d4
        vmlsl.u8  \out, d25, d5
        vmlal.u8  \out, d26, d6
.endm

.macro exec_filter_hv8_qpel_g5_0 out1 out2
        vmull.s16 q14,d22,d0[0] // q11
        vmull.s16 q15,d23,d0[0]
        vmlsl.s16 q14,d20,d0[1] // q10
        vmlsl.s16 q15,d21,d0[1]
        vmlal.s16 q14,d18,d0[2] // q9
        vmlal.s16 q15,d19,d0[2]
        vmlal.s16 q14,d16,d0[3] // q8
        vmlal.s16 q15,d17,d0[3]
        vmlsl.s16 q14,d14,d1[0] // q7
        vmlsl.s16 q15,d15,d1[0]
        vqshrn.s32 \out1,q14,#6
        vqshrn.s32 \out2,q15,#6
.endm

.macro exec_filter_hv42_qpel_g5_0 out
        vmull.s16 q14,d22,d0[0] // q11
        vmlsl.s16 q14,d20,d0[1] // q10
        vmlal.s16 q14,d18,d0[2] // q9
        vmlal.s16 q14,d16,d0[3] // q8
        vmlsl.s16 q14,d14,d1[0] // q7
        vqshrn.s32 \out,q14,#6
.endm

.macro exec_filter_hv8_qpel_g5_1 out1 out2
        vmull.s16 q14,d20,d0[1] // q10
        vmull.s16 q15,d21,d0[1]
        vmlsl.s16 q14,d22,d0[0] // q11
        vmlsl.s16 q15,d23,d0[0]
        vmlal.s16 q14,d18,d0[2] // q9
        vmlal.s16 q15,d19,d0[2]
        vmlsl.s16 q14,d16,d0[3] // q8
        vmlsl.s16 q15,d17,d0[3]
        vmlal.s16 q14,d14,d1[0] // q7
        vmlal.s16 q15,d15,d1[0]
        vqshrn.s32 \out1,q14,#6
        vqshrn.s32 \out2,q15,#6
.endm

.macro exec_filter_hv42_qpel_g5_1 out
        vmull.s16 q14,d20,d0[1] // q10
        vmlsl.s16 q14,d22,d0[0] // q11
        vmlal.s16 q14,d18,d0[2] // q9
        vmlsl.s16 q14,d16,d0[3] // q8
        vmlal.s16 q14,d14,d1[0] // q7
        vqshrn.s32 \out,q14,#6
.endm

function ff_hevc_put_qpel5_h_neon_8, export=1

        push {r6}
        ldr r6, [sp, #4] // mx
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_pre dir_h  \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
11:     loop_pel_neon_8 fun_pre dir_h  \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
            
           /*
 10:
   push {r4-r12}

   mov r4,r3
   mov r3,r2
   mov r2,r1
   mov r1,#64
   ldr r5,[sp,#44]

   ldr r6,[sp,#36]

   vpush {d8-d15}
  
   sub r6,#1

   sub r2,#2 ///MODIF

   lsrs r11,r6,#2
   lsl r6,#3
   ldr r12,=qpel_coeffs_g5
   add r12,r6
   ldr r6,[r12]
   vdup.i8 d0,r6
   lsr r6,#8
   vdup.i8 d1,r6
   lsr r6,#8
   vdup.i8 d2,r6
   lsr r6,#8
   vdup.i8 d3,r6
   ldr r6,[r12,#4]
   vdup.i8 d4,r6

   lsl r1,#1

   mov r12,r4
   mov r6,r0
   mov r7,r2

   cmp r5,#6
   bgt 8f
   cmp r5,#4
   blt 2f
   b 4f
8:
   pld [r2]
   vld1.8 {q5},[r2],r3
   vmov d12,d10
   vext.8 d13,d10,d11,#1
   vext.8 d14,d10,d11,#2
   vext.8 d15,d10,d11,#3
   vext.8 d16,d10,d11,#4

   vmull.u8 q9,d12,d0
   vmlsl.u8 q9,d13,d1 ///MODIF
   vmlal.u8 q9,d14,d2
   vmlal.u8 q9,d15,d3
   vmlsl.u8 q9,d16,d4

   subs r4,#1

   vst1.16 {q9},[r0],r1 ///MODIF

   bne 8b

   subs r5,#8
   beq 99f
   mov r4,r12

   add r6,#16

   mov r0,r6
   add r7,#8
   mov r2,r7

   cmp r5,#4
   bgt 8b

4:
   pld [r2]
   vld1.8 d12,[r2],r3
   vext.8 d13,d10,d10,#1
   vext.8 d14,d10,d10,#2
   vext.8 d15,d10,d10,#3
   vext.8 d16,d10,d10,#4

   vmull.u8 q11,d12,d0
   vmlsl.u8 q11,d13,d1
   vmlal.u8 q11,d14,d2
   vmlal.u8 q11,d15,d3
   vmlsl.u8 q11,d16,d4

   subs r4,#1

   vst1.16 d22,[r0],r1

   bne 4b
   subs r5,#4
   beq 99f
   mov r4,r12

   add r6,#8

   mov r0,r6
   add r7,#4
   mov r2,r7

2:
   pld [r2]
   vld1.8 d12,[r2],r3
   vext.8 d13,d10,d10,#1
   vext.8 d14,d10,d10,#2
   vext.8 d15,d10,d10,#3
   vext.8 d16,d10,d10,#4

   vmull.u8 q11,d12,d0
   vmlsl.u8 q11,d13,d1
   vmlal.u8 q11,d14,d2
   vmlal.u8 q11,d15,d3
   vmlsl.u8 q11,d16,d4

   vst1.32 d22[0],[r0],r1
   bne 2b
  
99:vpop {d8-d15}
   pop {r4-r12}
   bx lr

11: 
   push {r4-r12}

   mov r4,r3
   mov r3,r2
   mov r2,r1
   mov r1,#64
   ldr r5,[sp,#44]

   ldr r6,[sp,#36]

   vpush {d8-d15}
 
   sub r6,#1

   lsrs r11,r6,#2
   lsl r6,#3
   ldr r12,=qpel_coeffs_g5
   add r12,r6
   ldr r6,[r12]
   vdup.i8 d0,r6
   lsr r6,#8
   vdup.i8 d1,r6
   lsr r6,#8
   vdup.i8 d2,r6
   lsr r6,#8
   vdup.i8 d3,r6
   ldr r6,[r12,#4]
   vdup.i8 d4,r6

   lsl r1,#1

   mov r12,r4
   mov r6,r0
   mov r7,r2

   cmp r5,#6
   bgt 8f
   cmp r5,#4
   blt 2f
   b 4f

8:
   pld [r2]
   vld1.8 {q5},[r2],r3
   vmov d12,d10
   vext.8 d13,d10,d11,#1
   vext.8 d14,d10,d11,#2
   vext.8 d15,d10,d11,#3
   vext.8 d16,d10,d11,#4

   vmull.u8 q11,d13,d1
   vmlsl.u8 q11,d12,d0
   vmlal.u8 q11,d14,d2
   vmlsl.u8 q11,d15,d3
   vmlal.u8 q11,d16,d4

   subs r4,#1

   vst1.16 {q11},[r0],r1

   bne 8b
   subs r5,#8
   beq 99f
   mov r4,r12

   add r6,#16

   mov r0,r6
   add r7,#8
   mov r2,r7

   cmp r5,#4
   bgt 8b
4:
   pld [r2]
   vld1.8 d12,[r2],r3
   vext.8 d13,d10,d10,#1
   vext.8 d14,d10,d10,#2
   vext.8 d15,d10,d10,#3
   vext.8 d16,d10,d10,#4

   vmull.u8 q11,d13,d1
   vmlsl.u8 q11,d12,d0
   vmlal.u8 q11,d14,d2
   vmlsl.u8 q11,d15,d3
   vmlal.u8 q11,d16,d4

   subs r4,#1

   vst1.16 d22,[r0],r1

   bne 4b

   subs r5,#4
   beq 99f
   mov r4,r12

   add r6,#8

   mov r0,r6
   add r7,#4
   mov r2,r7

2:
   pld [r2]
   vld1.8 d12,[r2],r3
   vext.8 d13,d10,d10,#1
   vext.8 d14,d10,d10,#2
   vext.8 d15,d10,d10,#3
   vext.8 d16,d10,d10,#4

   vmull.u8 q11,d13,d1
   vmlsl.u8 q11,d12,d0
   vmlal.u8 q11,d14,d2
   vmlsl.u8 q11,d15,d3
   vmlal.u8 q11,d16,d4

   subs r4,#1

   vst1.32 d22[0],[r0],r1

   bne 2b

99: vpop {d8-d15}
   pop {r4-r12}
   bx lr
*/
endfunc

function ff_hevc_qpel5_uni_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #8] // mx
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_uni dir_h  \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
11:     loop_pel_neon_8 fun_uni dir_h  \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
endfunc

function ff_hevc_qpel5_bi_h_neon_8, export=1
        push {r6}
        ldr r6, [sp, #12] // mx
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_bi dir_h   \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
11:     loop_pel_neon_8 fun_bi dir_h   \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
endfunc

.ltorg

function ff_hevc_put_qpel5_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #8] // my
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_pre dir_v  \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
11:     loop_pel_neon_8 fun_pre dir_v  \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
endfunc

function ff_hevc_qpel5_uni_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #12] // my
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_uni dir_v  \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
11:     loop_pel_neon_8 fun_uni dir_v  \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
19:
endfunc

function ff_hevc_qpel5_bi_v_neon_8, export=1
        push {r6}
        ldr r6, [sp, #16] // my
        sub r6, #1
        lsrs r6, r6, #1
        pop {r6}
        bne 11f

10:     loop_pel_neon_8 fun_bi dir_v   \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
11:     loop_pel_neon_8 fun_bi dir_v   \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
endfunc

.ltorg

function ff_hevc_put_qpel5_hv_neon_8, export=1
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
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
            
           /*
      push {r4-r12}

      mov r4,r3
      mov r3,r2
      mov r2,r1
      mov r1,#64
      ldr r5,[sp,#44]

      ldr r6,[sp,#36]
      ldr r7,[sp,#40]

      vpush {d8-d15}

      sub r6,#1

      sub r2,r3
      sub r2,r3

      sub r2,#2

      lsrs r11,r6,#2
      lsl r6,#3
      ldr r12,=qpel_coeffs_g5
      add r12,r6
      ldr r6,[r12]
      vdup.i8 d8,r6
      lsr r6,#8
      vdup.i8 d9,r6
      lsr r6,#8
      vdup.i8 d24,r6
      lsr r6,#8
      vdup.i8 d25,r6
      ldr r6,[r12,#4]
      vdup.i8 d26,r6

      sub r7,#1

      sub r2,r3
      lsl r7,#3

      add r12,r7
      ldr r7,[r12]
      vmov.i64 d0,#0
      vmov.8 d0[0],r7
      lsr r7,#8
      vmov.8 d0[2],r7
      lsr r7,#8
      vmov.8 d0[4],r7
      lsr r7,#8
      vmov.8 d0[6],r7
      ldr r7,[r12,#4]
      vmov.i64 d1,#0
      vmov.8 d1[0],r7

      lsl r1,#1

      mov r12,r4
      mov r6,r0
      mov r7,r2

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q10,d8,d2
      vmlsl.u8 q10,d9,d3
      vmlal.u8 q10,d24,d4
      vmlal.u8 q10,d25,d5
      vmlsl.u8 q10,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q9,d8,d2
      vmlsl.u8 q9,d9,d3
      vmlal.u8 q9,d24,d4
      vmlal.u8 q9,d25,d5
      vmlsl.u8 q9,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q8,d8,d2
      vmlsl.u8 q8,d9,d3
      vmlal.u8 q8,d24,d4
      vmlal.u8 q8,d25,d5
      vmlsl.u8 q8,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q7,d8,d2
      vmlsl.u8 q7,d9,d3
      vmlal.u8 q7,d24,d4
      vmlal.u8 q7,d25,d5
      vmlsl.u8 q7,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q6,d8,d2
      vmlsl.u8 q6,d9,d3
      vmlal.u8 q6,d24,d4
      vmlal.u8 q6,d25,d5
      vmlsl.u8 q6,d26,d6

      cmp r5,#6
      bgt 8f
      cmp r5,#4
      blt 2f
      b 4f
8:
      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1
      vmov q11,q10
      vmov q10,q9
      vmov q9,q8
      vmov q8,q7
      vmov q7,q6

      vmull.u8 q6,d8,d2
      vmlsl.u8 q6,d9,d3
      vmlal.u8 q6,d24,d4
      vmlal.u8 q6,d25,d5
      vmlsl.u8 q6,d26,d6

      vmull.s16 q14,d22,d0[0]
      vmull.s16 q15,d23,d0[0]
      vmlsl.s16 q14,d20,d0[1]
      vmlsl.s16 q15,d21,d0[1]
      vmlal.s16 q14,d18,d0[2]
      vmlal.s16 q15,d19,d0[2]
      vmlal.s16 q14,d16,d0[3]
      vmlal.s16 q15,d17,d0[3]
      vmlsl.s16 q14,d14,d1[0]
      vmlsl.s16 q15,d15,d1[0]
      vqshrn.s32 d10,q14,#6
      vqshrn.s32 d11,q15,#6

      subs r4,#1

      vst1.16 {q5},[r0],r1

      bne 8b
      subs r5,#8
      beq 99f
      mov r4,r12

      add r6,#16

      mov r0,r6
      add r7,#8
      mov r2,r7

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q10,d8,d2
      vmlsl.u8 q10,d9,d3
      vmlal.u8 q10,d24,d4
      vmlal.u8 q10,d25,d5
      vmlsl.u8 q10,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q9,d8,d2
      vmlsl.u8 q9,d9,d3
      vmlal.u8 q9,d24,d4
      vmlal.u8 q9,d25,d5
      vmlsl.u8 q9,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q8,d8,d2
      vmlsl.u8 q8,d9,d3
      vmlal.u8 q8,d24,d4
      vmlal.u8 q8,d25,d5
      vmlsl.u8 q8,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q7,d8,d2
      vmlsl.u8 q7,d9,d3
      vmlal.u8 q7,d24,d4
      vmlal.u8 q7,d25,d5
      vmlsl.u8 q7,d26,d6

      pld [r2]
      vld1.8 {q1},[r2],r3
      vext.8 d4,d2,d3,#2
      vext.8 d5,d2,d3,#3
      vext.8 d6,d2,d3,#4
      vext.8 d3,d2,d3,#1

      vmull.u8 q6,d8,d2
      vmlsl.u8 q6,d9,d3
      vmlal.u8 q6,d24,d4
      vmlal.u8 q6,d25,d5
      vmlsl.u8 q6,d26,d6

      cmp r5,#4
      bgt 8b

     99:vpop {d8-d15}
      pop {r4-r12}
      bx lr*/
201:    loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
.ltorg
210:    loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
211:    loop_pel_neon_8 fun_pre dir_hv \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
endfunc

.ltorg

function ff_hevc_qpel5_uni_hv_neon_8, export=1
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
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
201:    loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
.ltorg
210:    loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
211:    loop_pel_neon_8 fun_uni dir_hv \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
endfunc
.ltorg

function ff_hevc_qpel5_bi_hv_neon_8, export=1
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
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0
201:    loop_pel_neon_8 fun_bi dir_hv  \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_0    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_0   \
            exec_filter_hv42_qpel_g5_0

.ltorg
210:    loop_pel_neon_8 fun_bi dir_hv  \
            load_coeffs_qpel_g5_0       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_0   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_0       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
211:    loop_pel_neon_8 fun_bi dir_hv  \
            load_coeffs_qpel_g5_1       \
            load_coeffs_32_qpel_g5_1    \
            preload_data_v_qpel_g5      \
            preload_data_hv_qpel_g5_1   \
            load_data_h8_qpel_g5        \
            load_data_h42_qpel_g5       \
            load_data_v_qpel_g5         \
            load_data_hv8_qpel_g5       \
            load_data_hv42_qpel_g5      \
            exec_filter_qpel_g5_1       \
            exec_filter_hv8_qpel_g5_1   \
            exec_filter_hv42_qpel_g5_1
endfunc

qpel_coeffs_g5: // Added 0's for padding
       .byte   1,  9, 58, 17,  3, 0, 0, 0
       .byte   2, 10, 41, 37,  6, 0, 0, 0
       .byte   3, 17, 58,  9,  1, 0, 0, 0
