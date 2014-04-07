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

.macro regshuffle_d8
    vmov d16, d17
    vmov d17, d18
    vmov d18, d19
    vmov d19, d20
    vmov d20, d21
    vmov d21, d22
    vmov d22, d23
.endm

.macro regshuffle_q8
    vmov q0, q1
    vmov q1, q2
    vmov q2, q3
    vmov q3, q4
    vmov q4, q5
    vmov q5, q6
    vmov q6, q7
.endm

.macro vextin8
        pld       [r2]
        vld1.8    {q11}, [r2], r3
        vext.8    d16, d22, d23, #1
        vext.8    d17, d22, d23, #2
        vext.8    d18, d22, d23, #3
        vext.8    d19, d22, d23, #4
        vext.8    d20, d22, d23, #5
        vext.8    d21, d22, d23, #6
        vext.8    d22, d22, d23, #7
.endm

.macro loadin8
        pld       [r2]
        vld1.8    {d16}, [r2], r3
        pld       [r2]
        vld1.8    {d17}, [r2], r3
        pld       [r2]
        vld1.8    {d18}, [r2], r3
        pld       [r2]
        vld1.8    {d19}, [r2], r3
        pld       [r2]
        vld1.8    {d20}, [r2], r3
        pld       [r2]
        vld1.8    {d21}, [r2], r3
        pld       [r2]
        vld1.8    {d22}, [r2], r3
        pld       [r2]
        vld1.8    {d23}, [r2], r3
.endm

// 29 cycles
// -a + 4*b - 10*c + 58*d + 17*e - 5*f + g
// q0   q1    q2       q3    q4    q5   q6
.macro qpel_filter_1_32b
        vmov.i16   d16, #58
        vmov.i16   d17, #10
        vmull.s16   q9, d6, d16   // 58 * d0
        vmull.s16  q10, d7, d16   // 58 * d1
        vmov.i16   d16, #17
        vmull.s16  q11, d4, d17   // 10 * c0
        vmull.s16  q12, d5, d17   // 10 * c1
        vmov.i16   d17, #5
        vmull.s16  q13, d8, d16   // 17 * e0
        vmull.s16  q14, d9, d16   // 17 * e1
        vmull.s16  q15, d10, d17  //  5 * f0
        vmull.s16   q8, d11, d17  //  5 * f1
        vsub.s32    q9, q11       // 58 * d0 - 10 * c0
        vsub.s32   q10, q12       // 58 * d1 - 10 * c1
        vshll.s16  q11, d2, #2    // 4 * b0
        vshll.s16  q12, d3, #2    // 4 * b1
        vadd.s32    q9, q13       // 58 * d0 - 10 * c0 + 17 * e0
        vadd.s32   q10, q14       // 58 * d1 - 10 * c1 + 17 * e1
        vsubl.s16  q13, d12, d0   // g0 - a0
        vsubl.s16  q14, d13, d1   // g1 - a1
        vadd.s32    q9, q11       // 58 * d0 - 10 * c0 + 17 * e0 + 4 * b0
        vadd.s32   q10, q12       // 58 * d1 - 10 * c1 + 17 * e1 + 4 * b1
        vsub.s32   q13, q15       // g0 - a0 - 5 * f0
        vsub.s32   q14, q8        // g1 - a1 - 5 * f1
        vadd.s32    q9, q13       // 58 * d0 - 10 * c0 + 17 * e0 + 4 * b0 + g0 - a0 - 5 * f0
        vadd.s32   q10, q14       // 58 * d1 - 10 * c1 + 17 * e1 + 4 * b1 + g1 - a1 - 5 * f1
        vqshrn.s32  d16, q9, #6
        vqshrn.s32  d17, q10, #6
.endm

// input  q0 - q7
// output q8
// -a + 4*b - 11*c + 40*d + 40*e -11*f + 4*g - h
// q0   q1    q2     q3     q4    q5     q6    q7

.macro qpel_filter_2_32b
        vmov.i32   q8, #11
        vaddl.s16   q9, d6, d8  // d0 + e0
        vaddl.s16  q10, d7, d9  // d1 + e1
        vaddl.s16  q11, d4, d10 // c0 + f0
        vaddl.s16  q12, d5, d11 // c1 + f1
        vmul.s32   q11, q8   // 11 * (c0 + f0)
        vmul.s32   q12, q8     // 11 * (c1 + f1)
        vmov.i32   q8, #40
        vaddl.s16  q15, d2, d12  // b0 + g0
        vmul.s32    q9, q8      // 40 * (d0 + e0)
        vmul.s32   q10, q8      // 40 * (d1 + e1)
        vaddl.s16   q8, d3, d13  // b1 + g1
        vaddl.s16  q13, d0, d14  // a0 + h0
        vaddl.s16  q14, d1, d15  // a1 + h1
        vshl.s32   q15, #2       // 4*(b0+g0)
        vshl.s32    q8, #2       // 4*(b1+g1)
        vadd.s32   q11, q13      // 11 * (c0 + f0) + a0 + h0
        vadd.s32   q12, q14      // 11 * (c1 + f1) + a1 + h1
        vadd.s32   q9, q15       // 40 * (d0 + e0) + 4*(b0+g0)
        vadd.s32   q10, q8       // 40 * (d1 + e1) + 4*(b1+g1)
        vsub.s32   q9, q11       // 40 * (d0 + e0) + 4*(b0+g0) - (11 * (c0 + f0) + a0 + h0)
        vsub.s32   q10, q12      // 40 * (d1 + e1) + 4*(b1+g1) - (11 * (c1 + f1) + a1 + h1)
        vqshrn.s32  d16, q9, #6
        vqshrn.s32  d17, q10, #6
.endm


// 29 cycles
// -a + 4*b - 10*c + 58*d + 17*e - 5*f + g
// q0      q1      q2       q3    q4    q5       q6      q7
// d0,1    d2,3    d4,5     d6,7  d8,9  d10,11   d12,13  d14,15
// q7      q6      q5       q4    q3    q2       q1
// d14,15  d12,13  d10,11   d8,9  d6,7  d4,5     d2,3
.macro qpel_filter_3_32b
        vmov.i16   d16, #58
        vmov.i16   d17, #10
        vmull.s16   q9, d8, d16   // 58 * d0
        vmull.s16  q10, d9, d16   // 58 * d1
        vmov.i16   d16, #17
        vmull.s16  q11, d10, d17   // 10 * c0
        vmull.s16  q12, d11, d17   // 10 * c1
        vmov.i16   d17, #5
        vmull.s16  q13, d6, d16   // 17 * e0
        vmull.s16  q14, d7, d16   // 17 * e1
        vmull.s16  q15, d4, d17  //  5 * f0
        vmull.s16   q8, d5, d17  //  5 * f1
        vsub.s32    q9, q11       // 58 * d0 - 10 * c0
        vsub.s32   q10, q12       // 58 * d1 - 10 * c1
        vshll.s16  q11, d12, #2    // 4 * b0
        vshll.s16  q12, d13, #2    // 4 * b1
        vadd.s32    q9, q13       // 58 * d0 - 10 * c0 + 17 * e0
        vadd.s32   q10, q14       // 58 * d1 - 10 * c1 + 17 * e1
        vsubl.s16  q13, d2, d14   // g0 - a0
        vsubl.s16  q14, d3, d15   // g1 - a1
        vadd.s32    q9, q11       // 58 * d0 - 10 * c0 + 17 * e0 + 4 * b0
        vadd.s32   q10, q12       // 58 * d1 - 10 * c1 + 17 * e1 + 4 * b1
        vsub.s32   q13, q15       // g0 - a0 - 5 * f0
        vsub.s32   q14, q8        // g1 - a1 - 5 * f1
        vadd.s32    q9, q13       // 58 * d0 - 10 * c0 + 17 * e0 + 4 * b0 + g0 - a0 - 5 * f0
        vadd.s32   q10, q14       // 58 * d1 - 10 * c1 + 17 * e1 + 4 * b1 + g1 - a1 - 5 * f1
        vqshrn.s32  d16, q9, #6
        vqshrn.s32  d17, q10, #6
.endm

//        a    b   c    d    e    f   g  reserved
// input d16  d17 d18 d19  d20  d21 d22  d23
// old    d0  d2  d4  d6    d8  d10  d12

// q vapaana d23,  d24 -> q12, q13, q14, q15
// 15 cycles
.macro qpel_filter_1 out=q7
        vmov.u8   d24, #58
        vmov.u8   d25, #10
        vshll.u8  q13, d20, #4     // 16*e
        vshll.u8  q14, d21, #2     // 4*f
        vmull.u8   \out, d19, d24    // 58*d
        vaddw.u8  q13, q13, d20    // 17*e
        vmull.u8  q15, d18, d25    // 10*c
        vaddw.u8  q14, q14, d21    // 5*f
        vsubl.u8  q12, d22, d16    // g - a
        vadd.u16   \out, q13         // 58d + 17e
        vshll.u8  q13, d17, #2     // 4*b
        vadd.u16  q15, q14         // 10*c + 5*f
        vadd.s16  q13, q12         // - a + 4*b + g
        vsub.s16   \out, q15         // -10*c + 58*d + 17*e -5*f
        vadd.s16   \out, q13         // -a + 4*b -10*c + 58*d + 17*e -5*f
.endm

//        a    b   c    d    e    f   g   h
// input d16  d17 d18 d19  d20  d21 d22  d23
// old    d0  d2  d4  d6    d8  d10  d12 d14

// q vapaana d24 -> q12, q13, q14, q15

// 16 cycles
.macro qpel_filter_2 out=q7
        vmov.i16   q12, #10
        vmov.i16   q14, #11
        vaddl.u8   q13, d19, d20   // d + e
        vaddl.u8   q15, d18, d21   // c + f
        vmul.u16   q13, q12        // 10 * (d+e)
        vmul.u16   q15, q14        // 11 * ( c + f)
        vaddl.u8  \out, d17, d22   // b + g
        vaddl.u8   q12, d16, d23   // a + h
        vadd.u16  \out, q13        // b + 10 * (d + e) + g
        vadd.s16   q12, q15
        vshl.u16  \out, #2         // 4 * (b + 10 * (d + e) + g)
        vsub.s16  \out, q12
.endm

//        a    b   c    d    e    f   g  reserv
// old  d16  d17 d18 d19  d20  d21 d22  d23 
// new  d23  d22  d21 d20 d19  d18  d17 d16
// old    d0  d2  d4  d6    d8  d10  d12
// q vapaana d23,  d24 -> q12, q13, q14, q15
// 15 cycles
.macro qpel_filter_3 out=q7
        vmov.u8   d24, #58
        vmov.u8   d25, #10
        vshll.u8  q13, d19, #4     // 16*e
        vshll.u8  q14, d18, #2     // 4*f
        vmull.u8   \out, d20, d24    // 58*d
        vaddw.u8  q13, q13, d19    // 17*e
        vmull.u8  q15, d21, d25    // 10*c
        vaddw.u8  q14, q14, d18    // 5*f
        vsubl.u8  q12, d17, d23    // g - a
        vadd.u16   \out, q13         // 58d + 17e
        vshll.u8  q13, d22, #2     // 4*b
        vadd.u16  q15, q14         // 10*c + 5*f
        vadd.s16  q13, q12         // - a + 4*b + g
        vsub.s16   \out, q15         // -10*c + 58*d + 17*e -5*f
        vadd.s16   \out, q13         // -a + 4*b -10*c + 58*d + 17*e -5*f
.endm

.macro  hevc_put_qpel_vX_neon_8 filter
        push   {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width
        vpush {d8-d15}
        sub       r2, r2, r3, lsl #1
        sub       r2, r3
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
        lsl       r1, #1
0:      loadin8
        cmp       r5, #4
        beq       4f
8:      subs r4, #1
        \filter
        vst1.16    {q7}, [r0], r1
        regshuffle_d8
        vld1.8    {d23}, [r2], r3
        bne 8b
        subs  r5, #8
        beq       99f
        mov r4, r12
        add r6, #16
        mov r0, r6
        add r7, #8
        mov r2, r7
        b     0b
4:      subs r4, #1
        \filter
        vst1.16    d14, [r0], r1
        regshuffle_d8
        vld1.32    {d23[0]}, [r2], r3
        bne 4b
99:     vpop {d8-d15}
        pop {r4, r5, r6, r7}
        bx lr
.endm

.macro  hevc_put_qpel_uw_vX_neon_8 filter
        push   {r4-r10}
        ldr    r5, [sp, #28] // width
        ldr    r4, [sp, #32] // height
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        vpush {d8-d15}
        sub       r2, r2, r3, lsl #1
        sub       r2, r3
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r8, #0
        bne       .Lbi\@
        //lsl       r1, #1
0:      loadin8
        cmp       r5, #4
        beq       4f
8:      subs r4, #1
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.8    d0, [r0], r1
        regshuffle_d8
        vld1.8    {d23}, [r2], r3
        bne 8b
        subs  r5, #8
        beq       99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r7, #8
        mov r2, r7
        b     0b
4:      subs r4, #1
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.32    d0[0], [r0], r1
        regshuffle_d8
        vld1.32    {d23[0]}, [r2], r3
        bne 4b
        b   99f
.Lbi\@: lsl       r9, #1
        mov       r10, r8
0:      loadin8
        cmp       r5, #4
        beq       4f
8:      subs r4, #1
        \filter
        vld1.16        {q0}, [r8], r9
        vqadd.s16      q0, q7
        vqrshrun.s16   d0, q0, #7
        vst1.8         d0, [r0], r1
        regshuffle_d8
        vld1.8    {d23}, [r2], r3
        bne 8b
        subs  r5, #8
        beq       99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r10, #16
        mov r8, r10
        add r7, #8
        mov r2, r7
        b     0b
4:      subs r4, #1
        \filter
        vld1.16      d0, [r8], r9
        vqadd.s16    d0, d14
        vqrshrun.s16 d0, q0, #7
        vst1.32      d0[0], [r0], r1
        regshuffle_d8
        vld1.32    {d23[0]}, [r2], r3
        bne 4b
99:     vpop {d8-d15}
        pop {r4-r10}
        bx lr
.endm


function ff_hevc_put_qpel_v1_neon_8, export=1
        hevc_put_qpel_vX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_v2_neon_8, export=1
        hevc_put_qpel_vX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_v3_neon_8, export=1
        hevc_put_qpel_vX_neon_8 qpel_filter_3
endfunc


function ff_hevc_put_qpel_uw_v1_neon_8, export=1
        hevc_put_qpel_uw_vX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_uw_v2_neon_8, export=1
        hevc_put_qpel_uw_vX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_uw_v3_neon_8, export=1
        hevc_put_qpel_uw_vX_neon_8 qpel_filter_3
endfunc


.macro hevc_put_qpel_hX_neon_8 filter
        push     {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width

        vpush    {d8-d15}
        sub       r2, #4
        lsl       r1, #1
        mov      r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r5, #4
        beq       4f
8:      subs      r4, #1
        vextin8
        \filter
        vst1.16   {q7}, [r0], r1
        bne       8b
        subs      r5, #8
        beq      99f
        mov       r4, r12
        add       r6, #16
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        cmp       r5, #4
        bne       8b
4:      subs      r4, #1
        vextin8
        \filter
        vst1.16  d14, [r0], r1
        bne       4b
99:     vpop     {d8-d15}
        pop      {r4, r5, r6, r7}
        bx lr
.endm

/*(uint8_t *dst,  ptrdiff_t dststride,     \
                                       uint8_t *_src, ptrdiff_t _srcstride,    \
                                       int width, int height,                  \
                                       int16_t* src2, ptrdiff_t src2stride)*/


.macro hevc_put_qpel_uw_hX_neon_8 filter
        push     {r4-r10}
        ldr       r5, [sp, #28] // width
        ldr       r4, [sp, #32] // height
        ldr       r8, [sp, #36] // src2
        ldr       r9, [sp, #40] // src2stride
        vpush    {d8-d15}
        sub       r2, #4
        mov      r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r8, #0
        bne       .Lbi\@
        cmp       r5, #4
        beq       4f
8:      subs      r4, #1
        vextin8
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.8    d0, [r0], r1
        bne       8b
        subs      r5, #8
        beq      99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        cmp       r5, #4
        bne       8b
4:      subs      r4, #1
        vextin8
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.32  d0[0], [r0], r1
        bne       4b
        b         99f
.Lbi\@:
        lsl       r9, #1
        cmp       r5, #4
        beq       4f
        mov       r10, r8
8:      subs      r4, #1
        vextin8
        \filter
        vld1.16        {q0}, [r8], r9
        vqadd.s16      q0, q7
        vqrshrun.s16   d0, q0, #7
        vst1.8         d0, [r0], r1
        bne       8b
        subs      r5, #8
        beq      99f
        mov       r4, r12
        add       r6, #8
        add       r10, #16
        mov       r8, r10
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        cmp       r5, #4
        bne       8b
4:      subs      r4, #1
        vextin8
        \filter
        vld1.16      d0, [r8], r9
        vqadd.s16    d0, d14
        vqrshrun.s16 d0, q0, #7
        vst1.32      d0[0], [r0], r1
        bne       4b
99:     vpop     {d8-d15}
        pop      {r4-r10}
        bx lr
.endm

function ff_hevc_put_qpel_h1_neon_8, export=1
        hevc_put_qpel_hX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_h2_neon_8, export=1
        hevc_put_qpel_hX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_h3_neon_8, export=1
        hevc_put_qpel_hX_neon_8 qpel_filter_3
endfunc


function ff_hevc_put_qpel_uw_h1_neon_8, export=1
        hevc_put_qpel_uw_hX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_uw_h2_neon_8, export=1
        hevc_put_qpel_uw_hX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_uw_h3_neon_8, export=1
        hevc_put_qpel_uw_hX_neon_8 qpel_filter_3
endfunc


//hevc.c:41:const uint8_t ff_hevc_qpel_extra_before[4] = { 0, 3, 3, 2 };
//hevc.c:42:const uint8_t ff_hevc_qpel_extra_after[4]  = { 0, 3, 4, 4 };
//hevc.c:43:const uint8_t ff_hevc_qpel_extra[4]        = { 0, 6, 7, 6 };

.macro hevc_put_qpel_hXvY_neon_8 filterh filterv
        push   {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width

        vpush {d8-d15}
        sub       r2, #4
        sub       r2, r2, r3, lsl #1
        sub       r2, r3  // extra_before 3
        lsl       r1, #1
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
0:      vextin8
        \filterh q0
        vextin8
        \filterh q1
        vextin8
        \filterh q2
        vextin8
        \filterh q3
        vextin8
        \filterh q4
        vextin8
        \filterh q5
        vextin8
        \filterh q6
        vextin8
        \filterh q7
        cmp r5, #4
        beq 4f
8:      subs  r4, #1
        \filterv
        vst1.16    {q8}, [r0], r1
        regshuffle_q8
        vextin8
        \filterh q7
        bne 8b
        subs  r5, #8
        beq 99f
        mov r4, r12
        add r6, #16
        mov r0, r6
        add r7, #8
        mov r2, r7
        b 0b
4:      subs  r4, #1
        \filterv
        vst1.16    d16, [r0], r1
        regshuffle_q8
        vextin8
        \filterh q7
        bne 4b
99:     vpop {d8-d15}
        pop {r4, r5, r6, r7}
        bx lr
.endm

.macro hevc_put_qpel_uw_hXvY_neon_8 filterh filterv
        push     {r4-r10}
        ldr       r5, [sp, #28] // width
        ldr       r4, [sp, #32] // height
        ldr       r8, [sp, #36] // src2
        ldr       r9, [sp, #40] // src2stride
        vpush {d8-d15}
        sub       r2, #4
        sub       r2, r2, r3, lsl #1
        sub       r2, r3  // extra_before 3
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r8, #0
        bne       .Lbi\@
0:      vextin8
        \filterh q0
        vextin8
        \filterh q1
        vextin8
        \filterh q2
        vextin8
        \filterh q3
        vextin8
        \filterh q4
        vextin8
        \filterh q5
        vextin8
        \filterh q6
        vextin8
        \filterh q7
        cmp r5, #4
        beq 4f
8:      subs  r4, #1
        \filterv
        vqrshrun.s16   d0, q8, #6
        vst1.8    d0, [r0], r1
        regshuffle_q8
        vextin8
        \filterh q7
        bne 8b
        subs  r5, #8
        beq 99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r7, #8
        mov r2, r7
        b 0b
4:      subs  r4, #1
        \filterv
        vqrshrun.s16   d0, q8, #6
        vst1.32        d0[0], [r0], r1
        regshuffle_q8
        vextin8
        \filterh q7
        bne 4b
        b   99f
.Lbi\@: lsl      r9, #1
        mov      r10, r8
0:      vextin8
        \filterh q0
        vextin8
        \filterh q1
        vextin8
        \filterh q2
        vextin8
        \filterh q3
        vextin8
        \filterh q4
        vextin8
        \filterh q5
        vextin8
        \filterh q6
        vextin8
        \filterh q7
        cmp r5, #4
        beq 4f
8:      subs  r4, #1
        \filterv
        vld1.16        {q0}, [r8], r9
        vqadd.s16      q0, q8
        vqrshrun.s16   d0, q0, #7
        vst1.8         d0, [r0], r1
        regshuffle_q8
        vextin8
        \filterh q7
        bne 8b
        subs  r5, #8
        beq 99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r10, #16
        mov r8, r10
        add r7, #8
        mov r2, r7
        b 0b
4:      subs  r4, #1
        \filterv
        vld1.16      d0, [r8], r9
        vqadd.s16    d0, d16
        vqrshrun.s16 d0, q0, #7
        vst1.32      d0[0], [r0], r1
        regshuffle_q8
        vextin8
        \filterh q7
        bne 4b
99:     vpop {d8-d15}
        pop {r4-r10}
        bx lr
.endm


function ff_hevc_put_qpel_h1v1_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_1 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_h2v1_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_2 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_h3v1_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_3 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_h1v2_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_1 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_h2v2_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_2 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_h3v2_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_3 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_h1v3_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_1 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_h2v3_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_2 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_h3v3_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_3 qpel_filter_3_32b
endfunc


function ff_hevc_put_qpel_uw_h1v1_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_1 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_uw_h2v1_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_2 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_uw_h3v1_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_3 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_uw_h1v2_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_1 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_uw_h2v2_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_2 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_uw_h3v2_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_3 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_uw_h1v3_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_1 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_uw_h2v3_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_2 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_uw_h3v3_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_3 qpel_filter_3_32b
endfunc



function ff_hevc_put_pixels_neon_8, export=1
        pld    [r2]
        push   {r4, r5, r6}
        pld    [r2, r3]
        ldr    r4, [sp, #12] // height
        ldr    r5, [sp, #24] // width

        lsl    r1, #1
        cmp    r5, #2
        beq    .Lw2
        cmp    r5, #6
        beq    .Lw6
        cmp    r5, #48
        beq    .Lw48
        bgt    .Lw64
        adr    r6, .Ltable
        add    r6, r6, r5, lsr #1
        mov    pc, r6
.Ltable:
        b .Lendput
        b .Lw4
        b .Lw8
        b .Lw12
        b .Lw16
        b .Lendput
        b .Lw24
        b .Lendput
        b .Lw32

.Lw2:
        vmov.u8      d5, #255
        vshr.u64     d5, #32
.LwX2:
        subs r4, #1
        vld1.32     {d0[0]}, [r2], r3
        pld [r2]
        vld1.32     d6, [r0]
        vshll.u8    q0, d0, #6
        vbit        d6, d0, d5
        vst1.32     d6, [r0], r1
        bne .LwX2
        b .Lendput
.Lw6:
        vmov.u8      q10, #255
        vshr.u64     d21, #32
.LwX6:
        subs r4, #1
        vld1.16     {d0}, [r2], r3
        pld [r2]
        vshll.u8    q0, d0, #6
        vld1.8      {q12}, [r0]
        vbit        q12, q0, q10
        vst1.8      {q12}, [r0], r1
        bne .LwX6
        b .Lendput

.Lw4:
        subs r4, #2
        vld1.32   {d0[0]}, [r2], r3
        vld1.32   {d0[1]}, [r2], r3
        pld       [r2]
        pld       [r2, r3]
        vshll.u8   q0, d0, #6
        vst1.64   {d0}, [r0], r1
        vst1.64   {d1}, [r0], r1
        bne .Lw4
        b .Lendput
.Lw8:
        subs r4, #2
        vld1.8   {d0}, [r2], r3
        vld1.8   {d2}, [r2], r3
        pld        [r2]
        pld        [r2, r3]
        vshll.u8   q0, d0, #6
        vshll.u8   q1, d2, #6
        vst1.16   {q0}, [r0], r1
        vst1.16   {q1}, [r0], r1
        bne .Lw8
        b .Lendput
.Lw12:
        subs r4, #2
        vld1.64    {d0}, [r2]
        add       r2, #8
        vld1.32   {d1[0]}, [r2], r3
        sub       r2, #8
        vld1.64    {d2}, [r2]
        add       r2, #8
        vld1.32   {d1[1]}, [r2], r3
        sub       r2, #8
        pld       [r2]
        pld       [r2, r3]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vmov      d22, d19
        vst1.64   {d16, d17, d18}, [r0], r1
        vst1.64   {d20, d21, d22}, [r0], r1
        bne       .Lw12
        b       .Lendput
.Lw16:
        subs r4, #2
        vld1.8   {q0}, [r2], r3
        vld1.8   {q1}, [r2], r3
        pld       [r2]
        pld       [r2, r3]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vst1.8    {q8, q9}, [r0], r1
        vst1.8    {q10, q11}, [r0], r1
        bne .Lw16
        b .Lendput
.Lw24:
        subs r4, #1
        vld1.8   {d0, d1, d2}, [r2], r3
        pld       [r2]
        vshll.u8  q10, d0, #6
        vshll.u8  q11, d1, #6
        vshll.u8  q12, d2, #6
        vstm     r0, {q10, q11, q12}
        add      r0, r1
        bne .Lw24
        b .Lendput
.Lw32:
        subs r4, #1
        vld1.8 {q0, q1}, [r2], r3
        pld       [r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vstm    r0, {q8, q9, q10, q11}
        add     r0, r1
        bne .Lw32
        b .Lendput
.Lw48:
        subs r4, #1
        vld1.8    {q0, q1}, [r2]
        add r2, #32
        vld1.8    {q2}, [r2], r3
        sub r2, #32
        pld       [r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vshll.u8  q12, d4, #6
        vshll.u8  q13, d5, #6
        vstm r0, {q8, q9, q10, q11, q12, q13}
        add  r0, r1
        bne .Lw48
        b .Lendput
.Lw64:
        subs r4, #1
        vld1.8    {q0, q1}, [r2]
        add r2, #32
        vld1.8    {q2, q3}, [r2], r3
        sub      r2, #32
        pld       [r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vshll.u8  q12, d4, #6
        vshll.u8  q13, d5, #6
        vshll.u8  q14, d6, #6
        vshll.u8  q15, d7, #6
        vstm    r0, {q8, q9, q10, q11, q12, q13, q14, q15}
        add r0, r1
        bne .Lw64
        b .Lendput
.Lendput:
        pop {r4, r5, r6}
        bx lr
endfunc

halt:
        b halt

function ff_hevc_put_qpel_uw_pixels_neon_8, export=1
        push   {r4-r9}
        ldr    r5, [sp, #24] // width
        ldr    r4, [sp, #28] // height
        ldr    r8, [sp, #32] // src2
        ldr    r9, [sp, #36] // src2stride
        vpush {d8-d15}
        cmp    r8, #0
        bne    2f
1:      subs r4, #1
        vld1.8     {d0}, [r2], r3
        vst1.8      d0, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r9}
        bx lr
2:      subs  r4, #1
        vld1.8         {d0}, [r2], r3
        vld1.16        {q1}, [r8], r9
        vshll.u8       q0, d0, #6
        vqadd.s16      q0, q1
        vqrshrun.s16   d0, q0, #7
        vst1.8      d0, [r0], r1
        bne 2b
        vpop {d8-d15}
        pop   {r4-r9}
        bx lr
endfunc






function ff_hevc_put_weighted_pred_avg_neon_8, export=1
        pld    [r2]
        pld    [r3]
        push   {r4, r5, r6, r7}
        ldr    r7, [sp, #16] // srcstride
        ldr    r5, [sp, #20] // width
        ldr    r4, [sp, #24] // height
        lsl    r7, #1

        cmp    r5, #48
        blt    .Ll48
        vpush {d8-d15}
        bgt    .La64
        b      .La48
.Ll48:
        cmp    r5, #2
        beq    .La2
        cmp    r5, #6
        beq    .La6
        adr    r6, .Latable
        add    r6, r6, r5, lsr #1
        mov    pc, r6
.Latable:
        b .Lendavg
        b .La4
        b .La8
        b .La12
        b .La16
        b .Lendavg
        b .La24
        b .Lendavg
        b .La32
.La2:
        vmov.u8      d5, #255
        vshr.u64     d5, #48
.LaX2:
        subs r4, #1
        vld1.16     {d0}, [r2], r7
        pld [r2]
        vld1.16     {d1}, [r3], r7
        pld [r3]
        vld1.32     d6[0], [r0]
        vqadd.s16   d0, d1
        vqrshrun.s16 d0, q0, #7
        vbit        d6, d0, d5
        vst1.32     d6[0], [r0], r1
        bne .LaX2
        b .Lendavg
.La6:
        vmov.u8      d5, #255
        vshr.u64     d5, #16
.LaX6:
        subs r4, #1
        vld1.16     {q0}, [r2,:128], r7
        pld [r2]
        vld1.16     {q1}, [r3,:128], r7
        pld [r3]
        vld1.8       d6, [r0]
        vqadd.s16   q0, q1
        vqrshrun.s16 d0, q0, #7
        vbit        d6, d0, d5
        vst1.8      d6, [r0], r1
        bne .LaX6
        b .Lendavg
.La4:
        subs r4, #2
        vld1.16     {d0}, [r2], r7
        vld1.16     {d1}, [r2], r7
        pld         [r2]
        pld         [r2, r7]
        vld1.16     {d2}, [r3], r7
        vld1.16     {d3}, [r3], r7
        pld         [r3]
        pld         [r3, r7]
        vqadd.s16   q0, q1
        vqrshrun.s16 d0, q0, #7
        vst1.32     d0[0], [r0], r1
        vst1.32     d0[1], [r0], r1
        bne .La4
        b .Lendavg
.La8:
        subs r4, #1
        vld1.16     {q0}, [r2], r7
        pld [r2]
        vld1.16     {q1}, [r3], r7
        pld [r3]
        vqadd.s16   q0, q1
        vqrshrun.s16 d0, q0, #7
        vst1.8      d0, [r0], r1
        bne .La8
        b .Lendavg
.La12:
        subs r4, #1
        vld1.16     {d0, d1, d2}, [r2], r7
        pld [r2]
        vld1.16     {d4, d5, d6}, [r3], r7
        pld [r3]
        vqadd.s16   q0, q2
        vqadd.s16   d2, d6
        vqrshrun.s16 d0, q0, #7
        vqrshrun.s16 d2, q1, #7
        vst1.8      d0, [r0]
        add         r0, #8
        vst1.32     d2[0], [r0], r1
        sub         r0, #8
        bne .La12
        b .Lendavg
.La16:
        subs r4, #1
        vld1.16   {q0, q1}, [r2,:128], r7
        pld       [r2]
        vld1.16   {q2, q3}, [r3,:128], r7
        pld       [r3]
        vqadd.s16  q0, q2
        vqadd.s16  q1, q3
        vqrshrun.s16   d20, q0, #7
        vqrshrun.s16   d21, q1, #7
        vst1.8    {d20, d21}, [r0], r1
        bne .La16
        b .Lendavg
.La24:
        subs r4, #1
        vldm      r2, {q0-q2}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q8-q10}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q8
        vqadd.s16  q1, q9
        vqadd.s16  q2, q10
        vqrshrun.s16   d20, q0, #7
        vqrshrun.s16   d21, q1, #7
        vqrshrun.s16   d22, q2, #7
        vst1.8    {d20-d22}, [r0], r1
        bne .La24
        b .Lendavg
.La32:
        subs r4, #1
        vldm      r2, {q0-q3}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q8-q11}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q8
        vqadd.s16  q1, q9
        vqadd.s16  q2, q10
        vqadd.s16  q3, q11
        vqrshrun.s16   d20, q0, #7
        vqrshrun.s16   d21, q1, #7
        vqrshrun.s16   d22, q2, #7
        vqrshrun.s16   d23, q3, #7
        vst1.8    {d20-d23}, [r0], r1
        bne .La32
        b .Lendavg
.La48:
        subs r4, #1
        vldm      r2, {q0-q5}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q10-q15}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q10
        vqadd.s16  q1, q11
        vqadd.s16  q2, q12
        vqadd.s16  q3, q13
        vqadd.s16  q4, q14
        vqadd.s16  q5, q15
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vqrshrun.s16   d2, q2, #7
        vqrshrun.s16   d3, q3, #7
        vqrshrun.s16   d4, q4, #7
        vqrshrun.s16   d5, q5, #7
        vstm    r0, {q0-q2}
        add r0, r1
        bne .La48
        b   .Lendavgl
.La64:
        subs r4, #1
        vldm      r2, {q0-q7}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q8-q15}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q8
        vqadd.s16  q1, q9
        vqadd.s16  q2, q10
        vqadd.s16  q3, q11
        vqadd.s16  q4, q12
        vqadd.s16  q5, q13
        vqadd.s16  q6, q14
        vqadd.s16  q7, q15
        vqrshrun.s16   d24, q0, #7
        vqrshrun.s16   d25, q1, #7
        vqrshrun.s16   d26, q2, #7
        vqrshrun.s16   d27, q3, #7
        vqrshrun.s16   d28, q4, #7
        vqrshrun.s16   d29, q5, #7
        vqrshrun.s16   d30, q6, #7
        vqrshrun.s16   d31, q7, #7
        vstm    r0, {q12-q15}
        add r0, r1
        bne .La64
        b .Lendavgl
.Lendavgl:
        vpop {d8-d15}
.Lendavg:
        pop {r4, r5, r6, r7}
        bx lr
endfunc
