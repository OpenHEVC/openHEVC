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

// NOTE: HEVCWindow is not supported in any function

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
    vmov.s16       d0, #10
    vmov.s16       d1, #11
2:  mov    r4, r0
    mov    r5, r2
    vld1.8 {q12-q13}, [r2]!
0:  subs   r6, #32
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
    vst1.16  {q9-q10}, [r0,:128]!
    vst1.16  {q14-q15}, [r0,:128]!
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
    vld1.16    {q1}, [r2,:128], r3 // a
    vld1.16    {q2}, [r2,:128], r3 // b
    vld1.16    {q3}, [r2,:128], r3 // c
    vld1.16    {q4}, [r2,:128], r3 // d
    vld1.16    {q5}, [r2,:128], r3 // e
    vld1.16    {q6}, [r2,:128], r3 // f
    vld1.16    {q7}, [r2,:128], r3 // g
0:  subs       r7, #2
    vld1.16    {q8}, [r2,:128], r3 // h
    vqadd.s16    q11, q4, q12
    vshr.s16     q11, #6
    vqmovun.s16  d22, q11
    vst1.8       {d22}, [r0,:64], r1
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
    vst1.8       {d30}, [r0,:64], r1
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

// dst0:                    64*d
// dst1:   -a + 3*b - 8*c + 26*d + 52*e - 11*f +  4*g -    h
// dst2:       -1*b + 4*c - 11*d + 52*e + 26*f -  8*g +  3*h - 1*i
// r2 dst0:                               64*f
// r2 dst1:            -c +  3*d + 8*e  + 26*f + 52*g - 11*h + 4*i - j

function ff_upsample_filter_block_luma_h_x1_5_neon, export=1
    push   {r4-r9}
    ldr    r4, [sp, #24] // x_EL
    ldr    r5, [sp, #28] // x_BL

    ldr    r6, [sp, #32] // width
    ldr    r7, [sp, #36] // height
    vpush  {d8-d15}
    // NOTE: does not do src = src - x_BL + ((x_EL << 1)/3)
    // might be required for some clips
    sub    r2, #4   // src - 4
    ldr    r9, =0x5555
    mul    r9, r4
    lsr    r9, #16
    mov    r12, #3
    mul    r9, r12
    sub    r9, r4, r9 // x_EL % 3
    lsl    r1, #1
    mov    r12, r6
    cmp    r9, #3
    bne    33f
    sub    r9, #3      // y_EL % 3
33: cmp    r9, #2
    bne    2f
    sub    r2, #1
2:  mov    r4, r0
    mov    r5, r2
    mov    r8, #3
    vld2.8 {d30,d31}, [r2]!
0:  vmov   q14, q15
    vld2.8 {d30,d31}, [r2]!
    vmov   d2, d29          // a
    vext.8 d3, d28, d30, #1 // b
    vext.8 d4, d29, d31, #1 // c
    vext.8 d5, d28, d30, #2 // d
    vext.8 d6, d29, d31, #2 // e
    vext.8 d7, d28, d30, #3 // f
    vext.8 d8, d29, d31, #3 // g
    vext.8 d9, d28, d30, #4 // h
    vext.8 d10, d29, d31, #4 // i
    vext.8 d11, d28, d30, #5 // j
    cmp    r9, #1
    beq    11f
    cmp    r9, #0
    beq    10f
    vmov.u8    d0, #52
    vmull.u8   q13, d6, d0  // 52*e
    b      12f
10: //dst0
    vmov       q6, q7
    vmov       q7, q8
    vshll.u8   q8, d5, #6   // 64*d
    subs   r8, #1
    bne    11f
    mov    r8, #3
    vst3.16  {d12, d14, d16}, [r0,:64]!
    vst3.16  {d13, d15, d17}, [r0,:64]!

11: //dst1
    vmov.u8    d1, #26
    vaddl.u8   q10, d2, d9  // a + h
    vmull.u8   q11, d5, d1  // 26*d
    vmov.u8    d1, #11
    vshll.u8   q13, d4, #3  // 8*c
    vsub.s16   q11, q10     // -a + 26*d -h
    vsub.s16   q11, q13     // -a - 8*c + 26*d -h
    vmull.u8   q10, d7, d1  // 11*f
    vshll.u8   q13, d8, #2  //  4*g
    vsub.s16   q11, q10     // -a - 8*c + 26*d - 11*f -h
    vmov.u8    d1, #3
    vadd.s16   q11, q13     // -a - 8*c + 26*d - 11*f + 4*g -
    vmov.u8    d0, #52
    vmull.u8   q10, d3, d1  // 3*b
    vmull.u8   q13, d6, d0  // 52*e
    vadd.s16   q11, q10     // -a + 3*b - 8*c + 26*d - 11*f + 4*g -h
    vmov       q6, q7
    vmov       q7, q8
    vadd.s16   q8, q11, q13 // -a + 3*b - 8*c + 26*d + 52*e - 11*f + 4*g -h

    subs   r8, #1
    bne    12f
    mov    r8, #3
    vst3.16  {d12, d14, d16}, [r0,:64]!
    vst3.16  {d13, d15, d17}, [r0,:64]!

    //dst2
12:
    vaddl.u8   q10, d3, d10 // b + i
    vsub.s16   q13, q10     // -b + 52*e -i
    vshll.u8   q10, d4, #2  // 4*c
    vmov.u8    d0, #11
    vadd.s16   q13, q10     // -b + 4*c + 52*e -i
    vmull.u8   q10, d5, d0  // 11*d
    vmov.u8    d0, #26
    vsub.s16   q13, q10     // -b + 4*c - 11*d + 52*e -i
    vmull.u8   q10, d7, d0  // 26*f
    vadd.s16   q13, q10     // -b + 4*c - 11*d + 52*e + 26*f -i
    vshll.u8   q10, d8, #3  // 8*g
    vmov.u8    d0, #3
    vsub.s16   q13, q10     // -b + 4*c - 11*d + 52*e + 26*f -8*g -i
    vmull.u8   q10, d9, d0  // 3*h
    vmov       q6, q7
    vmov       q7, q8
    vadd.s16   q8, q13, q10 // -b + 4*c - 11*d + 52*e + 26*f -8*g + 3*h -i

    subs   r8, #1
    bne    66f
    mov    r8, #3
    vst3.16  {d12, d14, d16}, [r0,:64]!
    vst3.16  {d13, d15, d17}, [r0,:64]!
66: cmp    r9, #0
    beq    99f
20: // r2 dst 0
    vmov       q6, q7
    vmov       q7, q8
    vshll.u8   q8, d7, #6   // 64*d
    subs   r8, #1
    bne    21f
    mov    r8, #3
    vst3.16  {d12, d14, d16}, [r0,:64]!
    vst3.16  {d13, d15, d17}, [r0,:64]!

21: // r2 dst 1
    cmp    r9, #2
    bne    99f
    vmov.u8    d1, #26
    vaddl.u8   q10, d4, d11 // a + h  -> c + j
    vmull.u8   q11, d7, d1  // 26*d -> 26*f
    vmov.u8    d1, #11
    vshll.u8   q13, d6, #3  // 8*c -> 8*e
    vsub.s16   q11, q10     // -c + 26*f -j
    vsub.s16   q11, q13     // -c - 8*e + 26*f -j
    vmull.u8   q10, d9, d1  // 11*f -> 11*h
    vshll.u8   q13, d10, #2 //  4*g -> 4*i
    vsub.s16   q11, q10     // -c - 8*e + 26*f - 11*h -j
    vmov.u8    d1, #3
    vadd.s16   q11, q13     // -c - 8*e + 26*f - 11*h + 4*i -j
    vmov.u8    d0, #52
    vmull.u8   q10, d5, d1  // 3*b -> 3*d
    vmull.u8   q13, d8, d0  // 52*e -> 52*g
    vadd.s16   q11, q10     // -c + 3*d - 8*e + 26*f - 11*h + 4*i -j
    vmov       q6, q7
    vmov       q7, q8
    vadd.s16   q8, q11, q13 // -c + 3*d - 8*e + 26*f + 52*g - 11*h + 4*i -j

    subs   r8, #1
    bne    99f
    mov    r8, #3
    vst3.16  {d12, d14, d16}, [r0,:64]!
    vst3.16  {d13, d15, d17}, [r0,:64]!

99: subs   r6, #24
    bgt    0b
    subs   r7, #1
    add    r0, r4, r1
    add    r2, r5, r3
    mov    r6, r12
    bne    2b
    vpop {d8-d15}
    pop {r4-r9}
    bx     lr
endfunc

// dst0:                    64*d
// dst1:   -a + 3*b - 8*c + 26*d + 52*e - 11*f + 4*g -   h
// dst2:       -1*b + 4*c - 11*d + 52*e + 26*f - 8*g + 3*h - 1*i
function ff_upsample_filter_block_luma_v_x1_5_neon, export=1
    push   {r4-r9}
    ldr    r9, [sp, #24] // y_BL
    ldr    r4, [sp, #28] // x_EL
    ldr    r5, [sp, #32] // y_EL

    ldr    r6, [sp, #36] // width
    ldr    r7, [sp, #40] // height
    vpush  {d8-d15}
    vmov.s32   q15, I_OFFSET
    lsl r3, #1
    add r0, r4      // dst + x_EL
    mul r4, r5, r1  // y_EL * dst_stride
    add r0, r4      // dst + x_EL + y_EL * dst_stride
    ldr r4, =0x5555
    mul r4, r5
    lsr r4, #16
    mov r12, #3
    mul r4, r12
    sub r9, r5, r4  // y_EL % 3
    cmp    r9, #3
    bne    33f
    sub    r9, #3      // y_EL % 3
33: sub r2, r3
    sub r2, r2, r3, lsl #1
    adr        r8, coeffsx1_5
    vld1.16    {d0}, [r8]
    mov        r12, r7
    cmp        r9, #2
    bne        2f
    sub        r2, r3
2:  mov        r4, r0
    mov        r5, r2
    vld1.16    {q1}, [r2,:128], r3 // a
    vld1.16    {q2}, [r2,:128], r3 // b
    vld1.16    {q3}, [r2,:128], r3 // c
    vld1.16    {q4}, [r2,:128], r3 // d
    vld1.16    {q5}, [r2,:128], r3 // e
    vld1.16    {q6}, [r2,:128], r3 // f
    vld1.16    {q7}, [r2,:128], r3 // g
    cmp        r9, #0
    beq        0f
    vld1.16    {q8}, [r2,:128], r3 // h
    vld1.16    {q9}, [r2,:128], r3 // i
    cmp        r9, #1
    beq        11f
    vmull.s16    q13, d10, d0[1]
    vmull.s16    q14, d11, d0[1] // 52*e
    b          12f
0:  vmov.s16   q12, #32
    vld1.16    {q8}, [r2,:128], r3 // h
    vld1.16    {q9}, [r2,:128], r3 // i
    vqadd.s16    q11, q4, q12
    vshr.s16     q11, #6
    vqmovun.s16  d22, q11
    vst1.8       {d22}, [r0,:64], r1
11: vaddl.s16    q10, d2, d16
    vaddl.s16    q1,  d3, d17    // a + h
    vshll.s16    q11, d14, #2
    vshll.s16    q12, d15, #2    // 4*g
    vmull.s16    q13, d8, d0[0]
    vmull.s16    q14, d9, d0[0]  // 26*d
    vsub.s32     q11, q10
    vsub.s32     q12, q1         // -a + 4*g -h
    vshll.s16    q10, d6, #3
    vshll.s16    q1,  d7, #3     // 8*c
    vadd.s32     q11, q13
    vadd.s32     q12, q14        // -a + 26*d + 4*g -h
    vsub.s32     q11, q10
    vsub.s32     q12, q1         // -a - 8*c  + 26*d + 4*g -h
    vmull.s16    q10, d4, d0[3]
    vmull.s16     q1, d5, d0[3]  // 3*b
    vmull.s16    q13, d10, d0[1]
    vmull.s16    q14, d11, d0[1] // 52*e
    vadd.s32     q11, q10
    vadd.s32     q12, q1         // -a + 3*b - 8*c + 26*d + 4*g -h
    vmull.s16    q10, d12, d0[2]
    vmull.s16     q1, d13, d0[2] // 11*f
    vadd.s32     q11, q13
    vadd.s32     q12, q14        // -a + 3*b - 8*c + 26*d + 52*e + 4*g -h
    vsub.s32     q11, q10
    vsub.s32     q12, q1         // -a + 3*b - 8*c + 26*d + 52*e - 11*f + 4*g -h
    vadd.s32     q11, q15        // + I_OFFSET
    vadd.s32     q12, q15        // + I_OFFSET
    vqshrn.s32   d22, q11, N_SHIFT
    vqshrn.s32   d23, q12, N_SHIFT
    vqmovun.s16  d22, q11
    vst1.8       {d22}, [r0:64], r1
12: vaddl.s16    q10, d4, d18
    vaddl.s16     q1, d5, d19    // b + i
    vsub.s32     q13, q10
    vsub.s32     q14, q1         // -b + 52*e -i
    vshll.s16    q10, d6, #2
    vshll.s16     q1, d7, #2     // 4*c
    vmull.s16    q11, d8, d0[2]
    vmull.s16    q12, d9, d0[2]  // 11*d
    vadd.s32     q13, q10
    vadd.s32     q14, q1         // -b + 4*c + 52*e -i
    vmull.s16    q10, d12, d0[0]
    vmull.s16     q1, d13, d0[0] // 26*f
    vsub.s32     q13, q11
    vsub.s32     q14, q12        // -b + 4*c -11*d + 52*e -i
    vshll.s16    q11, d14, #3
    vshll.s16    q12, d15, #3    // 8*g
    vadd.s32     q13, q10
    vadd.s32     q14, q1         // -b + 4*c -11*d + 52*e + 26*f -i
    vmull.s16    q10, d16, d0[3]
    vmull.s16     q1, d17, d0[3] // 3*h
    vsub.s32     q13, q11
    vsub.s32     q14, q12        // -b + 4*c -11*d + 52*e + 26*f -8*g -i
    vadd.s32     q13, q10
    vadd.s32     q14, q1         // -b + 4*c -11*d + 52*e + 26*f -8*g + 3*h -i
    vadd.s32     q11, q13, q15   // + I_OFFSET
    vadd.s32     q12, q14, q15   // + I_OFFSET
    vqshrn.s32   d22, q11, N_SHIFT
    vqshrn.s32   d23, q12, N_SHIFT
    vqmovun.s16  d22, q11
    vst1.8       {d22}, [r0,:64], r1
    subs       r7, #3
    vmov       q1, q3
    vmov       q2, q4
    vmov       q3, q5
    vmov       q4, q6
    vmov       q5, q7
    vmov       q6, q8
    vmov       q7, q9
    bgt        0b
    subs       r6, #8
    mov        r7, r12
    add        r0, r4, #8
    add        r2, r5, #16
    bne        2b
    vpop {d8-d15}
    pop {r4-r9}
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
    vst1.16  {q12-q13}, [r0,:128]!
    vst1.16  {q14-q15}, [r0,:128]
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
    adr        r8, coeffsx2
    vld1.16    {d0}, [r8]
    //mul r8, r3
    //sub r2, r8
    sub    r2, r3
    mov        r12, r7
2:  mov        r4, r0
    mov        r5, r2
    vld1.16    {q1}, [r2,:128], r3 // a
    vld1.16    {q2}, [r2,:128], r3 // b
    vld1.16    {q3}, [r2,:128], r3 // c
    vld1.16    {q4}, [r2,:128], r3 // d
0:  subs       r7, #2
    vld1.16    {q5}, [r2,:128], r3 // e
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
    vst1.8    {d24}, [r0,:64], r1
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
    vst1.8    {d24}, [r0,:64], r1
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

/*
   dst0:        64*b
   dst1: -2*a + 20*b + 52*c -  6*d
   dst2:      -  6*b + 52*c + 20*d - 2*e
r2 dst0:                      64*d
r2 dst1:               -2*c + 20*d + 52*e - 6*f

*/

function ff_upsample_filter_block_cr_h_x1_5_neon, export=1

    push   {r4-r9}
    ldr    r4, [sp, #24] // x_EL
    ldr    r5, [sp, #28] // x_BL
    ldr    r6, [sp, #32] // width
    ldr    r7, [sp, #36] // height
    vpush  {d8-d15}
    sub    r2, #2
    lsl    r1, #1
    ldr    r9, =0x5555
    mul    r9, r4
    lsr    r9, #16
    mov    r12, #3
    mul    r9, r12
    sub    r9, r4, r9 // x_EL % 3
    cmp    r9, #3
    bne    33f
    sub    r9, #3      // x_EL % 3
33: mov    r12, r6
    cmp    r9, #2
    bne    2f
    sub    r2, #1
2:  mov    r4, r0
    mov    r5, r2
    mov    r8, #3
    vld2.8 {d30,d31}, [r2]!
    vmov.u8   d0, #52
    vmov.u8   d1, #20
0:  vmov   q14, q15
    vld2.8 {d30,d31}, [r2]!
    vmov   d2, d29          // a
    vext.8 d3, d28, d30, #1 // b
    vext.8 d4, d29, d31, #1 // c
    vext.8 d5, d28, d30, #2 // d
    vext.8 d6, d29, d31, #2 // e
    vext.8 d7, d28, d30, #3 // f
    cmp  r9, #0
    beq  10f
    cmp  r9, #1
    beq  11f
    vmull.u8 q13, d4, d0    // 52*c
    b    12f

10: vmov     q8, q9
    vmov     q9, q10
    vshll.u8 q10, d3, #6     // 64*b
    subs   r8, #1
    bne    11f
    mov    r8, #3
    vst3.16  {d16, d18, d20}, [r0,:64]!
    vst3.16  {d17, d19, d21}, [r0,:64]!

11: vmov.u8  d22, #6
    vmov     q8, q9
    vmov     q9, q10
    vmull.u8 q12, d5, d22    // 6*d
    vmull.u8 q10, d3, d1     // 20*b
    vmull.u8 q13, d4, d0     // 52*c , also used in 12f
    vshll.u8 q11, d2, #1     // 2*a
    vadd.s16 q10, q13        // 20*b + 52*c
    vsub.s16 q10, q11        // -2*a + 20*b + 52*c
    vsub.s16 q10, q12        // -2*a + 20*b + 52*c - 6*d
    subs   r8, #1
    bne    12f
    mov    r8, #3
    vst3.16  {d16, d18, d20}, [r0,:64]!
    vst3.16  {d17, d19, d21}, [r0,:64]!

12: vmov     q8, q9
    vmov     q9, q10
    vmull.u8 q12, d5, d1     // 20*d, also used in 14f
    vmov.u8  d22, #6
    vmull.u8 q11, d3, d22    // 6*b
    vadd.s16 q10, q12, q13   // 52*c + 20*d
    vshll.u8 q13, d6, #1     // 2*e
    vsub.s16 q10, q13        // 52*c + 20*d - 2*e
    vsub.s16 q10, q11        // -6*b + 52*c + 20*d - 2*e
    subs   r8, #1
    bne    66f
    mov    r8, #3
    vst3.16  {d16, d18, d20}, [r0,:64]!
    vst3.16  {d17, d19, d21}, [r0,:64]!
66: cmp    r9, #0
    beq    99f

13: vmov     q8, q9
    vmov     q9, q10
    vshll.u8 q10, d5, #6     // 64*d
    subs   r8, #1
    bne    67f
    mov    r8, #3
    vst3.16  {d16, d18, d20}, [r0,:64]!
    vst3.16  {d17, d19, d21}, [r0,:64]!
67: cmp    r9, #2
    bne    99f

14: vmov.u8  d22, #6
    vmull.u8 q13, d6, d0     // 52*e
    vmull.u8 q11, d7, d22    // 6*f
    vadd.s16 q12, q13        // 20*d + 52*e
    vshll.u8 q13, d4, #1     // 2*c
    vsub.s16 q12, q13        // -2*c + 20*d + 52*e
    vmov     q8, q9
    vmov     q9, q10
    vsub.s16 q10, q12, q11   // -2*c + 20*d + 52*e - 6*f
    subs   r8, #1
    bne    99f
    mov    r8, #3
    vst3.16  {d16, d18, d20}, [r0,:64]!
    vst3.16  {d17, d19, d21}, [r0,:64]!

99: subs   r6, #24
    bgt    0b
    subs  r7, #1
    add   r0, r4, r1
    add   r2, r5, r3
    mov   r6, r12
    bne   2b
    vpop {d8-d15}
    pop {r4-r9}
    bx lr
endfunc

/*
dst2: -4*a + 54*b + 16*c - 2*d
dst0:         4*b + 62*c - 2*d
dst1:        -4*b + 30*c + 42*d - 4*e
*/
function ff_upsample_filter_block_cr_v_x1_5_neon, export=1
    push   {r4-r9}
    ldr    r8, [sp, #24] // y_BL
    ldr    r4, [sp, #28] // x_EL
    ldr    r5, [sp, #32] // y_EL

    ldr    r6, [sp, #36] // width
    ldr    r7, [sp, #40] // height
    vpush  {d8-d15}
    vmov.s32   q15, I_OFFSET
    lsl    r3, #1
    add    r0, r4  // dst += x_EL

    add    r9, r5, #1  // y_EL + 1
    lsl    r9, #1      // (y_EL+1) <<1
    ldr    r4, =0x5555
    mul    r9, r4
    lsr    r9, #16    // ((y_EL+1)<<1)/3
    sub    r9, #1     // ((y_EL+1)<<1)/3 - 1
    sub    r9, r8     // ((y_EL+1)<<1)/3 - 1 - y_BL
    mul    r9, r3     // refPos * srcStride
    add    r2, r9     // src + refPos * srcStride

    ldr    r4, =0x5555
    mul    r4, r5
    lsr    r4, #16
    mov    r12, #3
    mul    r4, r12
    sub    r9, r5, r4
    cmp    r9, #3
    bne    33f
    sub    r9, #3      // y_EL % 3
33: mul    r5, r1      // y_EL * dst_stride
    add    r0, r5      // dst += y_EL * dst_stride
    adr    r8, coeffs_c_x1_5
    vld1.16    {d0}, [r8]
    mov    r12, r7
    cmp    r9, #2
    beq    2f
    sub    r2, r3
    cmp    r9, #0
    beq    2f
    sub    r2, r3
2:  mov    r4, r0
    mov    r5, r2
    vld1.16    {q1}, [r2,:128], r3 // a
    vld1.16    {q2}, [r2,:128], r3 // b
    vld1.16    {q3}, [r2,:128], r3 // c

    vld1.16    {q4}, [r2,:128], r3 // d
    vld1.16    {q5}, [r2,:128], r3 // e
    cmp    r9, #0
    beq    10f
    cmp    r9, #2
    beq    12f
    b      11f
0:  vld1.16    {q4}, [r2,:128], r3 // d
    vld1.16    {q5}, [r2,:128], r3 // e

12: subs   r7, #1
    vshll.s16  q6, d2, #2
    vshll.s16  q7, d3, #2         // 4*a
    vshll.s16  q8, d6, #4
    vshll.s16  q9, d7, #4         // 16*c
    vmull.s16  q10, d4, d0[1]
    vmull.s16  q11, d5, d0[1]     // 54*b
    vsub.s32   q8, q6
    vsub.s32   q9, q7             // -4*a + 16*c
    vshll.s16  q6, d8, #1
    vshll.s16  q7, d9, #1         // 2*d
    vadd.s32   q8, q10
    vadd.s32   q9, q11            // -4*a + 54*b + 16*c
    vsub.s32   q8, q6
    vsub.s32   q9, q7             // -4*a + 54*b + 16*c - 2*d
    vadd.s32   q8, q15
    vadd.s32   q9, q15            // + I_OFFSET
    vqshrn.s32 d16, q8, N_SHIFT
    vqshrn.s32 d17, q9, N_SHIFT
    vqmovun.s16  d16, q8
    vst1.8    {d16}, [r0,:64], r1
    beq    99f

10: subs   r7, #1
    vshll.s16  q6, d8, #1
    vshll.s16  q7, d9, #1          // 2*d // repeat
    vmull.s16  q8, d6, d0[0]
    vmull.s16  q9, d7, d0[0]       // 62*c
    vshll.s16  q10, d4, #2
    vshll.s16  q11, d5, #2         // 4*b
    vsub.s32   q8, q6
    vsub.s32   q9, q7              // 62*c - 2*d
    vadd.s32   q8, q10
    vadd.s32   q9, q11             // 4*b + 62*c - 2*d
    vadd.s32   q8, q15
    vadd.s32   q9, q15             // + I_OFFSET
    vqshrn.s32 d16, q8, N_SHIFT
    vqshrn.s32 d17, q9, N_SHIFT
    vqmovun.s16  d16, q8
    vst1.8    {d16}, [r0,:64], r1
    beq    99f

11: subs   r7, #1
    vshll.s16  q10, d4, #2
    vshll.s16  q11, d5, #2         // 4*b // repeat
    vmull.s16  q6, d8, d0[2]
    vmull.s16  q7, d9, d0[2]       // 42*d
    vmull.s16  q8, d6, d0[3]
    vmull.s16  q9, d7, d0[3]       // 30*c
    vshll.s16  q12, d10, #2
    vshll.s16  q13, d11, #2        // 4*e
    vsub.s32   q6, q10
    vsub.s32   q7, q11             // -4*b + 42*d
    vadd.s32   q6, q8
    vadd.s32   q7, q9              // -4*b + 30*c + 42*d
    vsub.s32   q6, q12
    vsub.s32   q7, q13             // -4*b + 30*c + 42*d - 4*e
    vadd.s32   q8, q6, q15
    vadd.s32   q9, q7, q15         // + I_OFFSET
    vqshrn.s32 d16, q8, N_SHIFT
    vqshrn.s32 d17, q9, N_SHIFT
    vqmovun.s16 d16, q8
    vst1.8    {d16}, [r0,:64], r1
    vmov       q1, q3
    vmov       q2, q4
    vmov       q3, q5
    bgt    0b
99: subs   r6, #8
    mov    r7, r12
    add    r0, r4, #8
    add    r2, r5, #16
    bgt    2b
    vpop {d8-d15}
    pop {r4-r9}
    bx     lr
endfunc

coeffs_c_x1_5:
.word 0x0036003e  // d0[1] = 54, d0[0] = 62
.word 0x001e002a  // d0[3] = 30, d0[2] = 42
coeffsx1_5:
.word 0x0034001a  // d0[1] = 52, d0[0] = 26
.word 0x0003000b  // d0[3] =  3, d0[2] = 11
coeffsx2:
.word 0x002e003a  // d0[1] = 46, d0[0] = 58
.word 0x0000001c  // d0[2] = 28
