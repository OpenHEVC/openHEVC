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

#define MAX_PB_SIZE #64

.macro init_put_pixels
        pld    [r1]
        pld    [r1, r2]
        mov    r12, MAX_PB_SIZE
        lsl    r12, #1
.endm

function ff_hevc_put_pixels_w2_neon_8, export=1
        init_put_pixels
        vmov.u8      d5, #255
        vshr.u64     d5, #32
0:      subs r3, #1
        vld1.32     {d0[0]}, [r1], r2
        pld [r1]
        vld1.32     d6, [r0]
        vshll.u8    q0, d0, #6
        vbit        d6, d0, d5
        vst1.32     d6, [r0], r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w4_neon_8, export=1
        init_put_pixels
0:      subs r3, #2
        vld1.32   {d0[0]}, [r1], r2
        vld1.32   {d0[1]}, [r1], r2
        pld       [r1]
        pld       [r1, r2]
        vshll.u8   q0, d0, #6
        vst1.64   {d0}, [r0], r12
        vst1.64   {d1}, [r0], r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w6_neon_8, export=1
        init_put_pixels
        vmov.u8      q10, #255
        vshr.u64     d21, #32
0:      subs r3, #1
        vld1.16     {d0}, [r1], r2
        pld [r1]
        vshll.u8    q0, d0, #6
        vld1.8      {q12}, [r0]
        vbit        q12, q0, q10
        vst1.8      {q12}, [r0], r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w8_neon_8, export=1
        init_put_pixels
0:      subs r3, #2
        vld1.8   {d0}, [r1], r2
        vld1.8   {d2}, [r1], r2
        pld        [r1]
        pld        [r1, r2]
        vshll.u8   q0, d0, #6
        vshll.u8   q1, d2, #6
        vst1.16   {q0}, [r0], r12
        vst1.16   {q1}, [r0], r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w12_neon_8, export=1
        init_put_pixels
0:      subs r3, #2
        vld1.64    {d0}, [r1]
        add       r1, #8
        vld1.32   {d1[0]}, [r1], r2
        sub       r1, #8
        vld1.64    {d2}, [r1]
        add       r1, #8
        vld1.32   {d1[1]}, [r1], r2
        sub       r1, #8
        pld       [r1]
        pld       [r1, r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vmov      d22, d19
        vst1.64   {d16, d17, d18}, [r0], r12
        vst1.64   {d20, d21, d22}, [r0], r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w16_neon_8, export=1
        init_put_pixels
0:      subs r3, #2
        vld1.8   {q0}, [r1], r2
        vld1.8   {q1}, [r1], r2
        pld       [r1]
        pld       [r1, r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vst1.8    {q8, q9}, [r0], r12
        vst1.8    {q10, q11}, [r0], r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w24_neon_8, export=1
        init_put_pixels
0:      subs r3, #1
        vld1.8   {d0, d1, d2}, [r1], r2
        pld       [r1]
        vshll.u8  q10, d0, #6
        vshll.u8  q11, d1, #6
        vshll.u8  q12, d2, #6
        vstm     r0, {q10, q11, q12}
        add      r0, r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w32_neon_8, export=1
        init_put_pixels
0:      subs r3, #1
        vld1.8 {q0, q1}, [r1], r2
        pld       [r1]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vstm    r0, {q8, q9, q10, q11}
        add     r0, r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w48_neon_8, export=1
        init_put_pixels
0:      subs r3, #1
        vld1.8    {q0, q1}, [r1]
        add r1, #32
        vld1.8    {q2}, [r1], r2
        sub r1, #32
        pld       [r1]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vshll.u8  q12, d4, #6
        vshll.u8  q13, d5, #6
        vstm r0, {q8, q9, q10, q11, q12, q13}
        add  r0, r12
        bne 0b
        bx lr
endfunc

function ff_hevc_put_pixels_w64_neon_8, export=1
        init_put_pixels
0:      subs r3, #1
        vld1.8    {q0, q1}, [r1]
        add      r1, #32
        vld1.8    {q2, q3}, [r1], r2
        sub      r1, #32
        pld       [r1]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vshll.u8  q12, d4, #6
        vshll.u8  q13, d5, #6
        vshll.u8  q14, d6, #6
        vshll.u8  q15, d7, #6
        vstm    r0, {q8, q9, q10, q11, q12, q13, q14, q15}
        add r0, r12
        bne 0b
        bx lr
endfunc

.macro put_uni_pixels width, regs, regs2, regs3, regs4
function ff_hevc_put_uni_pixels_w\width\()_neon_8, export=1
        ldr    r12, [sp] // height
1:      subs   r12, #4
        vld1.32     {\regs}  , [r2], r3
        vld1.32     {\regs2} , [r2], r3
        vld1.32     {\regs3} , [r2], r3
        vld1.32     {\regs4} , [r2], r3
        vst1.32     {\regs}  , [r0], r1
        vst1.32     {\regs2} , [r0], r1
        vst1.32     {\regs3} , [r0], r1
        vst1.32     {\regs4} , [r0], r1
        bne 1b
        bx lr
endfunc
.endm

.macro put_uni_pixels_m width, regs, regs2, regs3, regs4
function ff_hevc_put_uni_pixels_w\width\()_neon_8, export=1
        push   {r4-r5}
        ldr    r12, [sp, #8] // height
1:      subs r12, #2
        mov      r4, r2
        vld1.32   {\regs} , [r2]!
        vld1.32   {\regs2} , [r2]
        add      r2, r4, r3
        mov      r4, r2
        vld1.32   {\regs3} , [r2]!
        vld1.32   {\regs4} , [r2]
        add      r2, r4, r3
        mov      r5, r0
        vst1.32   {\regs} , [r0]!
        vst1.32   {\regs2} , [r0]
        add      r0, r5, r1
        mov      r5, r0
        vst1.32   {\regs3} , [r0]!
        vst1.32   {\regs4} , [r0]
        add      r0, r5, r1
        bne 1b
        pop   {r4-r5}
        bx lr
endfunc
.endm

function ff_hevc_put_uni_pixels_w2_neon_8, export=1
        ldr    r12, [sp] // height
1:      subs   r12, #1
        vld1.16     d0[0] , [r2], r3
        vst1.16     d0[0] , [r0], r1
        bne 1b
        bx lr
endfunc

function ff_hevc_put_uni_pixels_w4_neon_8, export=1
        ldr    r12, [sp] // height
1:      subs   r12, #2
        vld1.32     d0[0] , [r2], r3
        vld1.32     d1[0] , [r2], r3
        vst1.32     d0[0] , [r0], r1
        vst1.32     d1[0] , [r0], r1
        bne 1b
        bx lr
endfunc

function ff_hevc_put_uni_pixels_w6_neon_8, export=1
        push   {r4-r5}
        ldr    r12, [sp, #8] // height
1:      subs r12, #1
        mov      r4, r2
        vld1.32   d0[0] , [r2]!
        vld1.16   d1[0] , [r2]
        add      r2, r4, r3
        mov      r5, r0
        vst1.32   d0[0] , [r0]!
        vst1.16   d1[0] , [r0]
        add      r0, r5, r1
        bne 1b
        pop   {r4-r5}
        bx lr
endfunc

function ff_hevc_put_uni_pixels_w8_neon_8, export=1
        ldr    r12, [sp] // height
1:      subs   r12, #2
        vld1.32     d0 , [r2], r3
        vld1.32     d1 , [r2], r3
        vst1.32     d0 , [r0], r1
        vst1.32     d1 , [r0], r1
        bne 1b
        bx lr
endfunc

put_uni_pixels_m 12, d0,    d1[0], d2,    d3[0]
put_uni_pixels   16, q0,    q1,    q2,    q3
put_uni_pixels   24, d0-d2, d3-d5, d16-d18, d19-d21
put_uni_pixels   32, q0-q1, q2-q3, q8-q9, q10-q11
put_uni_pixels_m 48, q0-q1, q2,    q8-q9, q10
put_uni_pixels_m 64, q0-q1, q2-q3, q8-q9, q10-q11


function ff_hevc_put_bi_pixels_w2_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
1:      subs   r10, #1
        vld1.16     d0[0] , [r2], r3
        vld1.16     {d2}, [r8], r9
        vshll.u8       q0, d0, #6
        vqadd.s16      d0, d2
        vqrshrun.s16   d0, q0, #7
        vst1.16     d0[0] , [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w4_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
1:      subs   r10, #1
        vld1.8   {d0}, [r2], r3
        vld1.8   {q1}, [r8], r9
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q1
        vqrshrun.s16   d0, q0, #7
        vst1.32  {d0[0]}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w6_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
1:      subs r10, #1
        mov      r5, r0
        vld1.8   {d0}, [r2], r3
        vld1.8   {q2}, [r8], r9
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q2
        vqrshrun.s16   d0, q0, #7
        vst1.32  {d0[0]}, [r0]!
        vst1.16  {d0[2]}, [r0], r1
        add      r0, r5, r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w8_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
1:      subs r10, #1
        vld1.8   {d0}, [r2], r3
        vld1.8   {q2}, [r8], r9
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q2
        vqrshrun.s16   d0, q0, #7
        vst1.8   {d0}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w12_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
        sub    r1, #8
1:      subs r10, #1
        vld1.8   {d0,d1}, [r2], r3
        vld1.8   {d6,d7,d8}, [r8], r9
        vshll.u8  q1, d1, #6
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q3
        vqadd.s16 q1, q4
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vst1.8   {d0}, [r0]!
        vst1.32  {d1[0]}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w16_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
1:      subs r10, #1
        vld1.8   {q0}, [r2], r3
        vld1.8   {q4,q5}, [r8], r9
        vshll.u8  q1, d1, #6
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q4
        vqadd.s16 q1, q5
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vst1.8   {q0}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w24_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
        sub    r9, #32
1:      subs r10, #1
        vld1.8   {d0,d1,d2}, [r2], r3
        vld1.8   {q6,q7}, [r8]!
        vld1.8   {q8}, [r8], r9
        vshll.u8  q2, d2, #6
        vshll.u8  q1, d1, #6
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q6
        vqadd.s16 q1, q7
        vqadd.s16 q2, q8
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vqrshrun.s16   d2, q2, #7
        vst1.8   {d0,d1,d2}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w32_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE // src2stride
        vpush {d8-d15}
        lsl	   r9, #1
        sub    r9, #32
1:      subs r10, #1
        vld1.8   {q0,q1}, [r2], r3
        vld1.8   {q8,q9}, [r8]!
        vld1.8   {q10,q11}, [r8], r9
        vshll.u8  q3, d3, #6
        vshll.u8  q2, d2, #6
        vshll.u8  q1, d1, #6
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q8
        vqadd.s16 q1, q9
        vqadd.s16 q2, q10
        vqadd.s16 q3, q11
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vqrshrun.s16   d2, q2, #7
        vqrshrun.s16   d3, q3, #7
        vst1.8   {q0,q1}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop  {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w48_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE
        //ldr    r9, [sp, #32] // src2stride
        vpush {d8-d15}
        lsl        r9, #1
        sub    r3, #32
        sub    r1, #32
        sub    r9, #64
1:      subs r10, #1
        vld1.8   {q0,q1}, [r2]!
        vld1.8   {q2}, [r2], r3
        vld1.8   {q8,q9}, [r8]!
        vld1.8   {q10,q11}, [r8]!
        vld1.8   {q12,q13}, [r8], r9
        vshll.u8  q5, d5, #6
        vshll.u8  q4, d4, #6
        vshll.u8  q3, d3, #6
        vshll.u8  q2, d2, #6
        vshll.u8  q1, d1, #6
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q8
        vqadd.s16 q1, q9
        vqadd.s16 q2, q10
        vqadd.s16 q3, q11
        vqadd.s16 q4, q12
        vqadd.s16 q5, q13
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vqrshrun.s16   d2, q2, #7
        vqrshrun.s16   d3, q3, #7
        vqrshrun.s16   d4, q4, #7
        vqrshrun.s16   d5, q5, #7
        vst1.8   {q0,q1}, [r0]!
        vst1.8   {q2}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop  {r4-r10}
        bx lr
endfunc

function ff_hevc_put_bi_pixels_w64_neon_8, export=1
        push   {r4-r10}
        ldr    r10,[sp, #32] // height
        ldr    r8, [sp, #28] // src2
        mov    r9, MAX_PB_SIZE
        //ldr    r9, [sp, #32] // src2stride
        vpush {d8-d15}
        lsl        r9, #1
        sub    r3, #32
        sub    r1, #32
        sub    r9, #96
1:      subs r10, #1
        vld1.8   {q0,q1}, [r2]!
        vld1.8   {q2,q3}, [r2], r3
        vld1.8   {q8,q9}, [r8]!
        vld1.8   {q10,q11}, [r8]!
        vld1.8   {q12,q13}, [r8]!
        vld1.8   {q14,q15}, [r8], r9
        vshll.u8  q7, d7, #6
        vshll.u8  q6, d6, #6
        vshll.u8  q5, d5, #6
        vshll.u8  q4, d4, #6
        vshll.u8  q3, d3, #6
        vshll.u8  q2, d2, #6
        vshll.u8  q1, d1, #6
        vshll.u8  q0, d0, #6
        vqadd.s16 q0, q8
        vqadd.s16 q1, q9
        vqadd.s16 q2, q10
        vqadd.s16 q3, q11
        vqadd.s16 q4, q12
        vqadd.s16 q5, q13
        vqadd.s16 q6, q14
        vqadd.s16 q7, q15
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vqrshrun.s16   d2, q2, #7
        vqrshrun.s16   d3, q3, #7
        vqrshrun.s16   d4, q4, #7
        vqrshrun.s16   d5, q5, #7
        vqrshrun.s16   d6, q6, #7
        vqrshrun.s16   d7, q7, #7
        vst1.8   {q0,q1}, [r0]!
        vst1.8   {q2,q3}, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop  {r4-r10}
        bx lr
endfunc

