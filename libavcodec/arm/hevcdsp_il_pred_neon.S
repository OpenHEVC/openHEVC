/*
 * Copyright (c) 2015 Seppo Tomperi <seppo.tomperi@vtt.fi>
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

#include "libavutil/arm/asm.S"
#include "neon.S"
//#define I_OFFSET (1 << (N_SHIFT - 1))
#define I_OFFSET #2048
#define N_SHIFT #12

// -a + 4*b -11*c + 40*d + 40*e -11f +4*g -h
function ff_upsample_filter_block_luma_h_x2_neon, export=1
    push   {r4-r8}
    ldr    r4, [sp, #20] // x_EL
    ldr    r5, [sp, #24] // x_BL

    ldr    r6, [sp, #28] // width
    ldr    r7, [sp, #32] // height
    vpush  {d8-d15}
    lsr    r4, #1
    sub    r2, r5
    sub    r2, #4
    add    r2, r4
    lsl    r1, #1
    mov    r12, r6
2:  mov    r4, r0
    mov    r5, r2
    vld1.8 {q12-q13}, [r2]!
0:  subs   r6, #32
    vmov.s16       d0, #10
    vmov.s16       d1, #11
    vext.8 q1, q12, q13, #1 // a
    vext.8 q2, q12, q13, #2 // b
    vext.8 q3, q12, q13, #3 // c
    vext.8 q4, q12, q13, #4 // d
    vext.8 q5, q12, q13, #5 // e
    vext.8 q6, q12, q13, #6 // f
    vext.8 q7, q12, q13, #7 // g
    vext.8 q8, q12, q13, #8 // h
    vaddl.u8 q14, d8, d10
    vaddl.u8 q15, d9, d11 // d + e
    vmul.s16 q14, d0[0]
    vmul.s16 q15, d0[0]   // 10*d + 10*e
    vaddw.u8 q14, d4
    vaddw.u8 q15, d5      // b + 10*d + 10*e
    vaddw.u8 q14, d14
    vaddw.u8 q15, d15     // b + 10*d + 10*e + g
    vshl.s16 q14, #2
    vshl.s16 q15, #2      // 4*b + 40*d + 40*e + 4*g
    vaddl.u8  q9, d6, d12
    vaddl.u8 q10, d7, d13 // c + f
    vmul.s16  q9, d1[1]
    vmul.s16 q10, d1[1]   // 11*c + 11*f
    vsub.s16 q14, q9
    vsub.s16 q15, q10     // 4*b - 11*c + 40*d + 40*e -11*f + 4g
    vaddl.u8  q9, d2, d16
    vaddl.u8 q10, d3, d17 // a + h
    vsub.s16 q15, q10     // -a + 4*b - 11*c + 40*d + 40*e -11*f + 4*g -h
    vsub.s16 q10, q14, q9
    vshll.u8  q9, d8, #6  // 64*d
    vshll.u8 q14, d9, #6  // 64*d
    vzip.16  q9, q10
    vzip.16  q14, q15
    vst1.16  {q9-q10}, [r0:128]!
    vst1.16  {q14-q15}, [r0:128]!
    vmov   q12, q13
    vld1.8 {q13}, [r2]!
    bne   0b
    subs  r7, #1
    add   r0, r4, r1
    add   r2, r5, r3
    mov   r6, r12
    bne   2b
    vpop {d8-d15}
    pop {r4-r8}
    bx lr
endfunc

// -a + 4*b -11*c + 40*d + 40*e -11f +4*g -h
function ff_upsample_filter_block_luma_v_x2_neon, export=1
    push   {r4-r8}
    //ldr    r4, [sp, #20] //
    ldr    r4, [sp, #24] // x_EL
    ldr    r5, [sp, #28] // y_EL

    ldr    r6, [sp, #32] // width
    ldr    r7, [sp, #36] // height
    vpush  {d8-d15}
    vmov.s16   q12, #32
    vmov.s32   q13, I_OFFSET
    lsl r3, #1
    add r0, r4
    mul r5, r1
    add r0, r5
    sub r2, r3
    sub r2, r2, r3, lsl #1
    vmov.s32       d0, #10
    vmov.s32       d1, #11
    mov        r12, r7
2:  mov        r4, r0
    mov        r5, r2
    vld1.16    {q1}, [r2:128], r3 // a
    vld1.16    {q2}, [r2:128], r3 // b
    vld1.16    {q3}, [r2:128], r3 // c
    vld1.16    {q4}, [r2:128], r3 // d
    vld1.16    {q5}, [r2:128], r3 // e
    vld1.16    {q6}, [r2:128], r3 // f
    vld1.16    {q7}, [r2:128], r3 // g
0:  subs       r7, #2
    vld1.16    {q8}, [r2:128], r3 // h
    vqadd.s16    q11, q4, q12
    vshr.s16     q11, #6
    vqmovun.s16  d22, q11
    vst1.8       {d22}, [r0:64], r1
    vaddl.s16    q9, d8, d10
    vaddl.s16    q10, d9, d11   // d + e
    vmul.s32     q9, d0[0]
    vmul.s32     q10, d0[0]     // 10*d + 10*e
    vaddw.s16    q9, q9, d4
    vaddw.s16    q10, q10, d5   // b + 10*d + 10*e
    vaddw.s16    q9, q9, d14
    vaddw.s16    q10, q10, d15  // b + 10*d + 10*e + g
    vshl.s32     q9, #2
    vshl.s32     q10, #2        // 4*b + 40*d + 40*e + 4*g
    vaddl.s16    q14, d6, d12
    vaddl.s16    q15, d7, d13   // c + f
    vmul.s32     q14, d1[0]
    vmul.s32     q15, d1[0]     // 11*c + 11*f
    vsub.s32     q9, q14
    vsub.s32     q10, q15       // 4*b - 11*c + 40*d + 40*e -11*f + 4g
    vaddl.s16    q14, d2, d16
    vaddl.s16    q15, d3, d17   // a + h
    vsub.s32     q9, q14
    vsub.s32     q10, q15       // -a + 4*b - 11*c + 40*d + 40*e -11*f + 4*g -h
    vadd.s32     q9, q13
    vadd.s32     q10, q13       // + I_OFFSET
    vqshrn.s32   d18, q9, N_SHIFT
    vqshrn.s32   d19, q10, N_SHIFT
    vqmovun.s16  d30, q9
    vst1.8       {d30}, [r0:64], r1
    vmov       q1, q2
    vmov       q2, q3
    vmov       q3, q4
    vmov       q4, q5
    vmov       q5, q6
    vmov       q6, q7
    vmov       q7, q8
    bne        0b
    subs       r6, #8
    mov        r7, r12
    add        r0, r4, #8
    add        r2, r5, #16
    bne        2b
    vpop {d8-d15}
    pop {r4-r8}
    bx lr
endfunc
