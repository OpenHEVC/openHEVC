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

#include "libavutil/arm/asm.S"
#include "neon.S"
/*(uint8_t *_dst, int16_t *coeffs, ptrdiff_t stride) */

tr4f:
.word 0x00240053  // 36 and d1[0] = 83
.word 0x00000000
tr8f:
.word 0x0059004b  // 89, d0[0] = 75
.word 0x00320012  // 50, d0[2] = 18
tr16:
.word 0x005a0057  // 90, d2[0] = 87
.word 0x00500046  // 80, d2[2] = 70
.word 0x0039002b  // 57, d2[0] = 43
.word 0x00190009  // 25, d2[2] = 9


.macro  transpose_16b_8x8   r0, r1, r2, r3, r4, r5, r6, r7
        vtrn.64         \r0, \r4
        vtrn.64         \r1, \r5
        vtrn.64         \r2, \r6
        vtrn.64         \r3, \r7
        vtrn.32         \r0, \r2
        vtrn.32         \r1, \r3
        vtrn.32         \r4, \r6
        vtrn.32         \r5, \r7
        vtrn.16         \r0, \r1
        vtrn.16         \r2, \r3
        vtrn.16         \r4, \r5
        vtrn.16         \r6, \r7
.endm

// in 4 q regs
// output 8 d regs
.macro transpose_16b_4x4    r0, r1, r2, r3
        vtrn.32         \r0, \r2
        vtrn.32         \r1, \r3
        vtrn.16         \r0, \r1
        vtrn.16         \r2, \r3
.endm

/* uses registers q2 - q6 for temp values */
.macro tr4 r0, r1, r2, r3
        vmull.s16  q4, \r1, d0[0]   // 83 * src1
        vmull.s16  q6, \r1, d0[1]   // 36 * src1
        vshll.s16  q2, \r0, #6   // 64 * src0
        vshll.s16  q3, \r2, #6   // 64 * src2
        vadd.s32   q5, q2, q3    // 64 * (src0 + src2)     e0
        vsub.s32   q2, q2, q3    // 64 * (src0 - src2)     e1
        vmlal.s16  q4, \r3, d0[1]   // 83 * src1 + 36 * src3  o0
        vmlsl.s16  q6, \r3, d0[0]   // 36 * src1 - 83 * src3  o1

        vsub.s32   q3, q5, q4    // e0 - o0
        vadd.s32   q4, q5, q4    // e0 + o0
        vadd.s32   q5, q2, q6    // e1 + o1
        vsub.s32   q6, q2, q6    // e1 - o1
.endm

.macro tr4_shift r0, r1, r2, r3, shift
        vmull.s16  q4, \r1, d0[0]   // 83 * src1
        vmull.s16  q6, \r1, d0[1]   // 36 * src1
        vshll.s16  q2, \r0, #6   // 64 * src0
        vshll.s16  q3, \r2, #6   // 64 * src2
        vadd.s32   q5, q2, q3    // 64 * (src0 + src2)     e0
        vsub.s32   q2, q2, q3    // 64 * (src0 - src2)     e1
        vmlal.s16  q4, \r3, d0[1]   // 83 * src1 + 36 * src3  o0
        vmlsl.s16  q6, \r3, d0[0]   // 36 * src1 - 83 * src3  o1

        vsub.s32   q3, q5, q4    // e0 - o0
        vadd.s32   q4, q5, q4    // e0 + o0
        vadd.s32   q5, q2, q6    // e1 + o1
        vsub.s32   q6, q2, q6    // e1 - o1

        vqrshrn.s32   \r0, q4, \shift
        vqrshrn.s32   \r1, q5, \shift
        vqrshrn.s32   \r2, q6, \shift
        vqrshrn.s32   \r3, q3, \shift
.endm


function ff_hevc_transform_4x4_add_neon_8, export=1
        vpush       {d8-d15}
        vld1.16     {q14, q15}, [r1,:128]  // coeffs
        ldr         r3, tr4f  // 36 and 83
        vmov.32     d0[0], r3

        tr4_shift d28, d29, d30, d31, #7

        vtrn.16     d28, d29
        vtrn.16     d30, d31
        vtrn.32     q14, q15

        tr4_shift d28, d29, d30, d31, #12

        vtrn.16     d28, d29
        vtrn.16     d30, d31
        vtrn.32     q14, q15

        vld1.32     {d0[0]}, [r0], r2
        vld1.32     {d0[1]}, [r0], r2
        vld1.32     {d1[0]}, [r0], r2
        vld1.32     {d1[1]}, [r0], r2
        sub         r0, r0, r2, lsl #2
        vaddw.u8    q14, q14, d0
        vaddw.u8    q15, q15, d1
        vqmovun.s16 d28, q14
        vqmovun.s16 d30, q15
        vst1.32     {d28[0]}, [r0], r2
        vst1.32     {d28[1]}, [r0], r2
        vst1.32     {d30[0]}, [r0], r2
        vst1.32     {d30[1]}, [r0]
        vpop        {d8-d15}
        bx lr
endfunc

.macro tr8_begin in0, in1, in2, in3
        vmull.s16  q7, \in0, d1[1]   // 89 * src1
        vmull.s16  q8, \in0, d1[0]   // 75 * src1
        vmull.s16  q9, \in0, d1[3]   // 50 * src1
        vmull.s16  q10, \in0, d1[2]  // 18 * src1

        vmlal.s16  q7, \in1, d1[0]   // 75 * src3
        vmlsl.s16  q8, \in1, d1[2]   //-18 * src3
        vmlsl.s16  q9, \in1, d1[1]   //-89 * src3
        vmlsl.s16  q10, \in1, d1[3]  //-50 * src3

        vmlal.s16  q7, \in2, d1[3]   // 50 * src5
        vmlsl.s16  q8, \in2, d1[1]   //-89 * src5
        vmlal.s16  q9, \in2, d1[2]   // 18 * src5
        vmlal.s16  q10, \in2, d1[0]  // 75 * src5

        vmlal.s16  q7, \in3, d1[2]   // 18 * src7
        vmlsl.s16  q8, \in3, d1[3]   //-50 * src7
        vmlal.s16  q9, \in3, d1[0]   // 75 * src7
        vmlsl.s16  q10, \in3, d1[1]  //-89 * src7
.endm

/* 90,  87,  80,  70,  57,  43,  25,   9,
 87,  57,   9, -43, -80, -90, -70, -25,
 80,   9, -70, -87, -25,  57,  90,  43,
 70, -43, -87,   9,  90,  25, -80, -57,
57, -80, -25,  90,  -9, -87,  43,  70,
43, -90,  57,  25, -87,  70,   9, -80,
 25, -70,  90, -80,  43,   9, -57,  87,
  9, -25,  43, -57,  70, -80,  87, -90,
*/


.macro tr16_begin in0, in1, in2, in3, in4, in5, in6, in7
//TODO: reorder
        vmull.s16  q2, \in0, d2[1]   // 90 * src1
        vmlal.s16  q2, \in1, d2[0]   // 87 * src3
        vmlal.s16  q2, \in2, d2[3]   // 80 * src5
        vmlal.s16  q2, \in3, d2[2]   // 70 * src7
        vmlal.s16  q2, \in4, d3[1]   // 57 * src9
        vmlal.s16  q2, \in5, d3[0]   // 43 * src11
        vmlal.s16  q2, \in6, d3[3]   // 25 * src13
        vmlal.s16  q2, \in7, d3[2]   //  9 * src15

        vmull.s16  q3, \in0, d2[0]   // 87 * src1
        vmlal.s16  q3, \in1, d3[1]   // 57 * src3
        vmlal.s16  q3, \in2, d3[2]   // 9 * src5
        vmlsl.s16  q3, \in3, d3[0]   //-43 * src7
        vmlsl.s16  q3, \in4, d2[3]   //-80 * src9
        vmlsl.s16  q3, \in5, d2[1]   //-90 * src11
        vmlsl.s16  q3, \in6, d2[2]   //-70 * src13
        vmlsl.s16  q3, \in7, d3[3]   //-25 * src15

        vmull.s16  q4, \in0, d2[3]   // 80 * src1
        vmlal.s16  q4, \in1, d3[2]   //  9 * src3
        vmlsl.s16  q4, \in2, d2[2]   //-70 * src5
        vmlsl.s16  q4, \in3, d2[0]   //-87 * src7
        vmlsl.s16  q4, \in4, d3[3]   //-25 * src9
        vmlal.s16  q4, \in5, d3[1]   // 57 * src11
        vmlal.s16  q4, \in6, d2[1]   // 90 * src13
        vmlal.s16  q4, \in7, d3[0]   // 43 * src15

        vmull.s16  q5, \in0, d2[2]   // 70 * src1
        vmlsl.s16  q5, \in1, d3[0]   //-43 * src3
        vmlsl.s16  q5, \in2, d2[0]   //-87 * src5
        vmlal.s16  q5, \in3, d3[2]   //  9 * src7
        vmlal.s16  q5, \in4, d2[1]   // 90 * src9
        vmlal.s16  q5, \in5, d3[3]   // 25 * src11
        vmlsl.s16  q5, \in6, d2[3]   //-80 * src13
        vmlsl.s16  q5, \in7, d3[1]   //-57 * src15

        vmull.s16  q6, \in0, d3[1]   // 57 * src1
        vmlsl.s16  q6, \in1, d2[3]   //-80 * src3
        vmlsl.s16  q6, \in2, d3[3]   //-25 * src5
        vmlal.s16  q6, \in3, d2[1]   // 90 * src7
        vmlsl.s16  q6, \in4, d3[2]   // -9 * src9
        vmlsl.s16  q6, \in5, d2[0]   //-87 * src11
        vmlal.s16  q6, \in6, d3[0]   // 43 * src13
        vmlal.s16  q6, \in7, d2[2]   // 70 * src15

        vmull.s16  q7, \in0, d3[0]   // 43 * src1
        vmlsl.s16  q7, \in1, d2[1]   //-90 * src3
        vmlal.s16  q7, \in2, d3[1]   // 57 * src5
        vmlal.s16  q7, \in3, d3[3]   // 25 * src7
        vmlsl.s16  q7, \in4, d2[0]   //-87 * src9
        vmlal.s16  q7, \in5, d2[2]   // 70 * src11
        vmlal.s16  q7, \in6, d3[2]   //  9 * src13
        vmlsl.s16  q7, \in7, d2[3]   //-80 * src15

        vmull.s16  q8, \in0, d3[3]   // 25 * src1
        vmlsl.s16  q8, \in1, d2[2]   //-70 * src3
        vmlal.s16  q8, \in2, d2[1]   // 90 * src5
        vmlsl.s16  q8, \in3, d2[3]   //-80 * src7
        vmlal.s16  q8, \in4, d3[0]   // 43 * src9
        vmlal.s16  q8, \in5, d3[2]   //  9 * src11
        vmlsl.s16  q8, \in6, d3[1]   //-57 * src13
        vmlal.s16  q8, \in7, d2[0]   // 87 * src15

        vmull.s16  q9, \in0, d3[2]   //  9 * src1
        vmlsl.s16  q9, \in1, d3[3]   //-25 * src3
        vmlal.s16  q9, \in2, d3[0]   // 43 * src5
        vmlsl.s16  q9, \in3, d3[1]   //-57 * src7
        vmlal.s16  q9, \in4, d2[2]   // 70 * src9
        vmlsl.s16  q9, \in5, d2[3]   //-80 * src11
        vmlal.s16  q9, \in6, d2[0]   // 87 * src13
        vmlsl.s16  q9, \in7, d2[1]   //-90 * src15
.endm



.macro tr8_end shift
        vadd.s32   q1, q4, q7   //  e_8[0] + o_8[0], dst[0]
        vsub.s32   q4, q4, q7   //  e_8[0] - o_8[0], dst[7]

        vadd.s32   q2, q5, q8   // e_8[1] + o_8[1], dst[1]
        vsub.s32   q5, q5, q8   // e_8[1] - o_8[1], dst[6]

        vadd.s32   q11, q6, q9  // e_8[2] + o_8[2], dst[2]
        vsub.s32    q6, q6, q9  // e_8[2] - o_8[2], dst[5]

        vadd.s32   q12, q3, q10 // e_8[3] + o_8[3], dst[3]
        vsub.s32   q3, q3, q10  // e_8[3] - o_8[3], dst[4]
        vqrshrn.s32   d2, q1, \shift
        vqrshrn.s32   d3, q2, \shift
        vqrshrn.s32   d4, q11, \shift
        vqrshrn.s32   d5, q12, \shift
        vqrshrn.s32   d6, q3, \shift
        vqrshrn.s32   d7, q6, \shift
        vqrshrn.s32   d9, q4, \shift
        vqrshrn.s32   d8, q5, \shift
.endm

.macro tr8_end2
        vsub.s32   q15, q4, q7   // e_8[0] - o_8[0], dst[7]
        vsub.s32   q14, q5, q8   // e_8[1] - o_8[1], dst[6]
        vsub.s32   q13, q6, q9   // e_8[2] - o_8[2], dst[5]
        vsub.s32   q12, q3, q10  // e_8[3] - o_8[3], dst[4]
        vadd.s32   q11, q3, q10  // e_8[3] + o_8[3], dst[3]
        vadd.s32   q10, q6, q9   // e_8[2] + o_8[2], dst[2]
        vadd.s32   q9, q5, q8    // e_8[1] + o_8[1], dst[1]
        vadd.s32   q8, q4, q7    // e_8[0] + o_8[0], dst[0]
.endm


.macro tr8_add r0
        vld1.8      {d24}, [r0]
        vaddw.u8    q13, \r0, d24
        vqmovun.s16 d26, q13
        vst1.8      {d26}, [r0], r2
.endm

.macro tr16_add src, dtmp
        vld1.8      {\dtmp}, [r0]
        vaddw.u8    \src, \src, \dtmp
        vqmovun.s16 \dtmp, \src
        vst1.8      {\dtmp}, [r0], r2
.endm

function ff_hevc_transform_8x8_add_neon_8, export=1
        mov    r0, r0
        mov    r0, r0
        mov    r0, r0
        push   {r4-r8}
        vpush {d8-d15}
        mov    r5, #16

        adr       r3, tr4f
        vld1.16   {d0, d1}, [r3]

        // left half
        vld1.16 {d24}, [r1], r5
        vld1.16 {d25}, [r1], r5
        vld1.16 {d26}, [r1], r5
        vld1.16 {d27}, [r1], r5
        vld1.16 {d28}, [r1], r5
        vld1.16 {d29}, [r1], r5
        vld1.16 {d30}, [r1], r5
        vld1.16 {d31}, [r1], r5
        sub      r1, #128
        tr8_begin d25, d27, d29, d31
        tr4       d24, d26, d28, d30
        tr8_end   #7
        vst1.16 {d2}, [r1], r5
        vst1.16 {d3}, [r1], r5
        vst1.16 {d4}, [r1], r5
        vst1.16 {d5}, [r1], r5
        vst1.16 {d6}, [r1], r5
        vst1.16 {d7}, [r1], r5
        vst1.16 {d8}, [r1], r5
        vst1.16 {d9}, [r1], r5

        //right half
        sub      r1, #120
        vld1.16 {d24}, [r1], r5
        vld1.16 {d25}, [r1], r5
        vld1.16 {d26}, [r1], r5
        vld1.16 {d27}, [r1], r5
        vld1.16 {d28}, [r1], r5
        vld1.16 {d29}, [r1], r5
        vld1.16 {d30}, [r1], r5
        vld1.16 {d31}, [r1], r5
        sub      r1, #128
        tr8_begin d25, d27, d29, d31
        tr4       d24, d26, d28, d30
        tr8_end   #7
        vst1.16 {d2}, [r1], r5
        vst1.16 {d3}, [r1], r5
        vst1.16 {d4}, [r1], r5
        vst1.16 {d5}, [r1], r5
        vst1.16 {d6}, [r1], r5
        vst1.16 {d7}, [r1], r5
        vst1.16 {d8}, [r1], r5
        vst1.16 {d9}, [r1], r5
        sub      r1, #136
        // top half
        vldm r1!, {q12-q15} // coeffs
        transpose_16b_4x4 d24, d26, d28, d30
        transpose_16b_4x4 d25, d27, d29, d31
        tr8_begin d26, d30, d27, d31
        tr4 d24, d28, d25, d29
        tr8_end #12
        transpose_16b_4x4 d2, d3, d4, d5
        transpose_16b_4x4 d6, d7, d8, d9
        vswp     d7, d5
        vswp     d7, d8
        vswp     d3, d6
        vswp     d6, d4

        tr8_add q1
        tr8_add q2
        tr8_add q3
        tr8_add q4

        // bottom half
        vldm r1, {q12-q15} // coeffs
        transpose_16b_4x4 d24, d26, d28, d30
        transpose_16b_4x4 d25, d27, d29, d31
        tr8_begin d26, d30, d27, d31
        tr4 d24, d28, d25, d29
        tr8_end #12
        transpose_16b_4x4 d2, d3, d4, d5
        transpose_16b_4x4 d6, d7, d8, d9
        vswp     d7, d5
        vswp     d7, d8
        vswp     d3, d6
        vswp     d6, d4
        tr8_add q1
        tr8_add q2
        tr8_add q3
        tr8_add q4
        vpop {d8-d15}
        pop {r4-r8}
        bx lr
endfunc

/* 90,  87,  80,  70,  57,  43,  25,   9,  -9, -25, -43, -57, -70, -80, -87, -90,
 87,  57,   9, -43, -80, -90, -70, -25,  25,  70,  90,  80,  43,  -9, -57, -87,
 80,   9, -70, -87, -25,  57,  90,  43, -43, -90, -57,  25,  87,  70,  -9, -80,
70, -43, -87,   9,  90,  25, -80, -57,  57,  80, -25, -90,  -9,  87,  43, -70,
57, -80, -25,  90,  -9, -87,  43,  70, -70, -43,  87,   9, -90,  25,  80, -57,
43, -90,  57,  25, -87,  70,   9, -80,  80,  -9, -70,  87, -25, -57,  90, -43,
 25, -70,  90, -80,  43,   9, -57,  87, -87,  57,  -9, -43,  80, -90,  70, -25,
  9, -25,  43, -57,  70, -80,  87, -90,  90, -87,  80, -70,  57, -43,  25, -9,
*/
/* 90, 87, 80, 70, 57, 43, 25, 9 */

/* 5a, 57, 50, 46, 39, 2b, 19, 9 */

/*0x005a0057, 0x00500046, 0x0039002b, 0x00190009 */

/*
        ldr     r3, = 0x005a0057  // 90, d2[0] = 87
        vmov.32     d2[0], r3
        ldr     r3, = 0x00500046  // 80, d2[2] = 70
        vmov.32     d2[1], r3

        ldr     r3, = 0x0039002b  // 57, d2[0] = 43
        vmov.32     d3[0], r3
        ldr     r3, = 0x00190009  // 25, d2[2] = 9
        vmov.32     d3[1], r3
*/

function ff_hevc_transform_16x16_add_neon_8, export=1
        mov    r0, r0
        mov    r0, r0
        mov    r0, r0
        push   {r4-r8}
        vpush {d8-d15}

        adr       r3, tr4f
        vld1.16   {q0, q1}, [r3]
        mov     r5, #32
        mov     r3, #4

        // 4 columns
0:      add     r1, r5
        lsl     r5, #1
        vld1.16 {d24}, [r1], r5
        vld1.16 {d25}, [r1], r5
        vld1.16 {d26}, [r1], r5
        vld1.16 {d27}, [r1], r5
        vld1.16 {d28}, [r1], r5
        vld1.16 {d29}, [r1], r5
        vld1.16 {d30}, [r1], r5
        vld1.16 {d31}, [r1], r5
        sub     r1, r1, r5, lsl #3
        sub     r1, r1, r5, lsr #1
        tr16_begin d24, d25, d26, d27, d28, d29, d30, d31 // -> q2 - q9
        vpush   {q6-q9}
        vpush   {q2-q5}

        vld1.16 {d24}, [r1], r5
        vld1.16 {d25}, [r1], r5
        vld1.16 {d26}, [r1], r5
        vld1.16 {d27}, [r1], r5
        vld1.16 {d28}, [r1], r5
        vld1.16 {d29}, [r1], r5
        vld1.16 {d30}, [r1], r5
        vld1.16 {d31}, [r1], r5
        sub     r1, r1, r5, lsl #3
        lsr     r5, #1

        tr8_begin d24, d26, d28, d30 // q7 -> q10
        tr4       d25, d27, d29, d31 // q3 -> q6
        tr8_end2  // q8 - q15  , e_16
        vpop {q0-q3} // 0 - 3  , o_16

        vadd.s32  q4, q8, q0   // e_16[0] + o_16[0]  // dst[0]
        vsub.s32  q8, q0       // e_16[0] + o_16[0]  // dst[15]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5

        vadd.s32  q4, q9, q1   // e_16[1] + o_16[1]  // dst[1]
        vsub.s32  q9, q1       // e_16[1] - o_16[1]  // dst[14]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5


        vadd.s32  q4, q10, q2  // e_16[2] + o_16[2]  // dst[2]
        vsub.s32  q10, q2      // e_16[2] - o_16[2]  // dst[13]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5

        vadd.s32  q4, q11, q3  // e_16[3] + o_16[3]  // dst[3]
        vsub.s32  q11, q3      // e_16[3] - o_16[3]  // dst[12]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5

        vpop      {q0-q3}  // 4 - 7

        vadd.s32  q4, q12, q0  // e_16[4] + o_16[4]  // dst[4]
        vsub.s32  q12, q0      // e_16[4] - o_16[4]  // dst[11]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5


        vadd.s32  q4, q13, q1  // e_16[5] + o_16[5]  // dst[5]
        vsub.s32  q13, q1      // e_16[5] - o_16[5]  // dst[10]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5


        vadd.s32  q4, q14, q2  // e_16[6] + o_16[6]  // dst[6]
        vsub.s32  q14, q2      // e_16[6] - o_16[6]  // dst[9]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5

        vadd.s32  q4, q15, q3  // e_16[7] + o_16[7]  // dst[7]
        vsub.s32  q15, q3      // e_16[7] - o_16[7]  // dst[8]
        vqrshrn.s32  d10, q4, #7
        vst1.16 {d10}, [r1], r5

        vqrshrn.s32  d10, q15, #7
        vst1.16 {d10}, [r1], r5
        vqrshrn.s32  d10, q14, #7
        vst1.16 {d10}, [r1], r5
        vqrshrn.s32  d10, q13, #7
        vst1.16 {d10}, [r1], r5
        vqrshrn.s32  d10, q12, #7
        vst1.16 {d10}, [r1], r5
        vqrshrn.s32  d10, q11, #7
        vst1.16 {d10}, [r1], r5
        vqrshrn.s32  d10, q10, #7
        vst1.16 {d10}, [r1], r5
        vqrshrn.s32  d10, q9, #7
        vst1.16 {d10}, [r1], r5
        vqrshrn.s32  d10, q8, #7
        vst1.16 {d10}, [r1], r5

        sub       r0, r0, r2, lsl #4
        add       r0, #4
        add       r1, #8
        subs      r3, #1
        bne       0b

        vpop {d8-d15}
        pop {r4-r8}
        bx lr
endfunc

