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


.macro put_epel_uni_pixels width, regs, regs2
function ff_hevc_put_epel_uni_pixels_w\width\()_neon_8, export=1
        ldr    r12, [sp] // height
1:      subs   r12, #2
        vld1.32     {\regs}  , [r2], r3
        vld1.32     {\regs2} , [r2], r3
        vst1.32     {\regs}  , [r0], r1
        vst1.32     {\regs2} , [r0], r1
        bne 1b
        bx lr
endfunc
.endm

function ff_hevc_put_epel_uni_pixels_w2_neon_8, export=1
        ldr    r12, [sp] // height
1:      subs   r12, #1
        vld1.16     d0[0] , [r2], r3
        vst1.16     d0[0] , [r0], r1
        bne 1b
        bx lr
endfunc

function ff_hevc_put_epel_uni_pixels_w6_neon_8, export=1
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

function ff_hevc_put_epel_uni_pixels_w12_neon_8, export=1
        push   {r4-r5}
        ldr    r12, [sp, #8] // height
1:      subs r12, #2
        mov      r4, r2
        vld1.32   {d0} , [r2]!
        vld1.32   {d1[0]} , [r2]
        add      r2, r4, r3
        mov      r4, r2
        vld1.32   {d2} , [r2]!
        vld1.32   {d3[0]} , [r2]
        add      r2, r4, r3
        mov      r5, r0
        vst1.32   {d0} , [r0]!
        vst1.32   {d1[0]} , [r0]
        add      r0, r5, r1
        mov      r5, r0
        vst1.32   {d2} , [r0]!
        vst1.32   {d3[0]} , [r0]
        add      r0, r5, r1
        bne 1b
        pop   {r4-r5}
        bx lr
endfunc

put_epel_uni_pixels    4, d0[0], d0[1]
put_epel_uni_pixels    8, d0,    d1,
put_epel_uni_pixels   16, q0,    q1,
put_epel_uni_pixels   24, d0-d2, d3-d5
put_epel_uni_pixels   32, q0-q1, q2-q3


function ff_hevc_put_epel_bi_pixels_w2_neon_8, export=1
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

function ff_hevc_put_epel_bi_pixels_w4_neon_8, export=1
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

function ff_hevc_put_epel_bi_pixels_w6_neon_8, export=1
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

function ff_hevc_put_epel_bi_pixels_w8_neon_8, export=1
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

function ff_hevc_put_epel_bi_pixels_w12_neon_8, export=1
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

function ff_hevc_put_epel_bi_pixels_w16_neon_8, export=1
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

function ff_hevc_put_epel_bi_pixels_w24_neon_8, export=1
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

function ff_hevc_put_epel_bi_pixels_w32_neon_8, export=1
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

