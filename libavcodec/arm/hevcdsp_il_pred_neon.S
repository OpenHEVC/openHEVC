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

//-4*a + 36*b + 36*c -4*d
function ff_upsample_filter_block_cr_h_x2_neon, export=1
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
    vld1.8   {q12-q13}, [r2]!
    vext.8    q1, q12, q13, #3 // a
    vext.8    q2, q12, q13, #4 // b
    vext.8    q3, q12, q13, #5 // c
    vext.8    q4, q12, q13, #6 // d
    vaddl.u8  q5, d4, d6
    vaddl.u8  q6, d5, d7       // b + c
    vaddl.u8  q7, d2, d8
    vaddl.u8  q8, d3, d9       // a + d
    vshl.s16  q9, q5, #5
    vshl.s16  q10, q6, #5      // 32*b + 32*c
    vsub.s16  q5, q7
    vsub.s16  q6, q8           // -a + b + c -d
    vshl.s16  q5, #2
    vshl.s16  q6, #2           // -4*a + 4*b + 4*c -4*d
    vadd.s16  q13, q5, q9
    vadd.s16  q15, q6, q10     // -4*a + 36*b + 36*c -4*d
    vshll.u8  q12, d4, #6
    vshll.u8  q14, d5, #6      // 64*b
    vzip.16   q12, q13
    vzip.16   q14, q15
    vst1.16  {q12-q13}, [r0:128]!
    vst1.16  {q14-q15}, [r0:128]
    subs  r7, #1
    add   r0, r4, r1
    add   r2, r5, r3
    mov   r6, r12
    bne   2b

    vpop {d8-d15}
    pop {r4-r8}
    bx lr
endfunc

//dst0: -2*a + 10*b + 58*c -  2*d
//dst1:        -6*b + 46*c + 28*d - 4*e
function ff_upsample_filter_block_cr_v_x2_neon, export=1
    push   {r4-r8}
    ldr    r8, [sp, #20] // y_BL
    ldr    r4, [sp, #24] // x_EL
    ldr    r5, [sp, #28] // y_EL

    ldr    r6, [sp, #32] // width
    ldr    r7, [sp, #36] // height
    vpush  {d8-d15}
    vmov.s32   q15, I_OFFSET
    lsl r3, #1
    add r0, r4
    mul r5, r1
    add r0, r5
    adr        r8, coeffs
    vld1.16    {d0}, [r8]
    //mul r8, r3
    //sub r2, r8
    sub    r2, r3
    mov        r12, r7
2:  mov        r4, r0
    mov        r5, r2
    vld1.16    {q1}, [r2:128], r3 // a
    vld1.16    {q2}, [r2:128], r3 // b
    vld1.16    {q3}, [r2:128], r3 // c
    vld1.16    {q4}, [r2:128], r3 // d
0:  subs       r7, #2
    vld1.16    {q5}, [r2:128], r3 // e
    vaddl.s16  q6, d2, d8
    vaddl.s16  q7, d3, d9     // a + d
    vshl.s16   q6, #1
    vshl.s16   q7, #1         // 2*a + 2*d
    vshll.s16  q8, d4, #1
    vshll.s16  q9, d5, #1     // 2*b
    vshll.s16  q10, d4, #3
    vshll.s16  q11, d5, #3    // 8*b
    vsub.s32   q12, q10, q6
    vsub.s32   q13, q11, q7   // -2*a + 8*b -2*d
    vadd.s32   q12, q8
    vadd.s32   q13, q9        // -2*a + 10*b -2*d
    vsub.s32   q8, q10
    vsub.s32   q9, q11        // -6*b
    vmull.s16  q6, d6, d0[0]
    vmull.s16  q7, d7, d0[0]  // 58*c
    vadd.s32   q12, q6
    vadd.s32   q13, q7        // -2*a + 10*b + 58*c -2*d
    vadd.s32   q12, q15
    vadd.s32   q13, q15       // + I_OFFSET
    vqshrn.s32 d24, q12, N_SHIFT
    vqshrn.s32 d25, q13, N_SHIFT
    vqmovun.s16  d24, q12
    vst1.8    {d24}, [r0:64], r1
    vshll.s16  q6, d10, #2
    vshll.s16  q7, d11, #2    // 4*e
    vsub.s32   q8, q6
    vsub.s32   q9, q7         // -6*b - 4*e
    vmull.s16  q6, d6, d0[1]
    vmull.s16  q7, d7, d0[1]  //  46*c
    vmull.s16  q10, d8, d0[2]
    vmull.s16  q11, d9, d0[2] //  28*d
    vadd.s32   q8, q6
    vadd.s32   q9, q7         // -6*b + 46*c - 4*e
    vadd.s32   q8, q10
    vadd.s32   q9, q11        // -6*b + 46*c + 28*d - 4*e
    vadd.s32   q8, q15
    vadd.s32   q9, q15        // + I_OFFSET
    vqshrn.s32 d24, q8, N_SHIFT
    vqshrn.s32 d25, q9, N_SHIFT
    vqmovun.s16  d24, q12
    vst1.8    {d24}, [r0:64], r1
    vmov       q1, q2
    vmov       q2, q3
    vmov       q3, q4
    vmov       q4, q5
    bne 0b
    subs  r6, #8
    mov   r7, r12
    add   r0, r4, #8
    add   r2, r5, #16
    bne 2b
    vpop {d8-d15}
    pop {r4-r8}
    bx lr
endfunc

coeffs:
.word 0x002e003a  // d0[1] = 46, d0[0] = 58
.word 0x0000001c  // d0[2] = 28
