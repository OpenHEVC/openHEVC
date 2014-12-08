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

function ff_hevc_sao_band_w8_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // offset_table
        vpush {d8-d15}
        vld1.8  {q0, q1}, [r5] // offset table

1:      subs    r4, #1
        vld1.8   {d24}, [r1], r3
        vshr.u8  d16, d24, #3
        vtbl.8   d16, {q0, q1}, d16
        vmovl.s8 q2, d16
        vmovl.u8 q6, d24
        vadd.s16 q2, q6
        vqmovun.s16 d4, q2
        vst1.8  {d4}, [r0], r2
        bne    1b

        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_band_w16_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // offset_table
        vpush {d8-d15}
        vld1.8  {q0, q1}, [r5] // offset table

1:      subs    r4, #1
        vld1.8  {q12}, [r1], r3

        vshr.u8   q8, q12, #3

        vtbl.8  d16, {q0, q1}, d16
        vtbl.8  d17, {q0, q1}, d17

        vmovl.s8 q2, d16
        vmovl.s8 q3, d17

        vmovl.u8 q6, d24
        vmovl.u8 q7, d25

        vadd.s16 q2, q6
        vadd.s16 q3, q7

        vqmovun.s16 d4, q2
        vqmovun.s16 d5, q3

        vstm.8   r0, {q2}
        add    r0, r2
        bne    1b

        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_band_w32_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // offset_table
        vpush {d8-d15}
        vld1.8  {q0, q1}, [r5] // offset table

1:      subs    r4, #1
        vld1.8  {q12-q13}, [r1], r3

        vshr.u8   q8, q12, #3
        vshr.u8   q9, q13, #3

        vtbl.8  d16, {q0, q1}, d16
        vtbl.8  d17, {q0, q1}, d17
        vtbl.8  d18, {q0, q1}, d18
        vtbl.8  d19, {q0, q1}, d19

        vmovl.s8 q2, d16
        vmovl.s8 q3, d17 // q8 free
        vmovl.s8 q4, d18
        vmovl.s8 q5, d19 // q9 free

        vmovl.u8 q6, d24
        vmovl.u8 q7, d25 // q12 free
        vmovl.u8 q8, d26
        vmovl.u8 q9, d27 // q13 free

        vadd.s16 q2, q6
        vadd.s16 q3, q7
        vadd.s16 q4, q8
        vadd.s16 q5, q9

        vqmovun.s16 d4, q2
        vqmovun.s16 d5, q3
        vqmovun.s16 d6, q4 // q4 free
        vqmovun.s16 d7, q5 // q5 free

        vst1.8 {q2-q3}, [r0], r2
        bne    1b

        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_band_w64_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // offset_table
        vpush {d8-d15}
        vld1.8  {q0, q1}, [r5] // offset table

1:      subs    r4, #1
        vld1.8  {q12-q13}, [r1]!
        vld1.8  {q14-q15}, [r1], r3
        sub     r1, #32

        vshr.u8   q8, q12, #3
        vshr.u8   q9, q13, #3
        vshr.u8  q10, q14, #3
        vshr.u8  q11, q15, #3

        vtbl.8  d16, {q0, q1}, d16
        vtbl.8  d17, {q0, q1}, d17
        vtbl.8  d18, {q0, q1}, d18
        vtbl.8  d19, {q0, q1}, d19
        vtbl.8  d20, {q0, q1}, d20
        vtbl.8  d21, {q0, q1}, d21
        vtbl.8  d22, {q0, q1}, d22
        vtbl.8  d23, {q0, q1}, d23

        vmovl.s8 q2, d16
        vmovl.s8 q3, d17 // q8 free
        vmovl.s8 q4, d18
        vmovl.s8 q5, d19 // q9 free

        vmovl.u8 q6, d24
        vmovl.u8 q7, d25 // q12 free
        vmovl.u8 q8, d26
        vmovl.u8 q9, d27 // q13 free

        vadd.s16 q2, q6
        vadd.s16 q3, q7
        vadd.s16 q4, q8
        vadd.s16 q5, q9

        vqmovun.s16 d4, q2
        vqmovun.s16 d5, q3
        vqmovun.s16 d6, q4 // q4 free
        vqmovun.s16 d7, q5 // q5 free

        // free q4 -q9, q12 - q13
        vmovl.s8 q4, d20
        vmovl.s8 q5, d21 // q10 free
        vmovl.s8 q6, d22
        vmovl.s8 q7, d23 // q11 free

        vmovl.u8  q8, d28
        vmovl.u8  q9, d29 // q14 free
        vmovl.u8 q10, d30
        vmovl.u8 q11, d31 // q15 free

        vadd.s16 q4, q8
        vadd.s16 q5, q9
        vadd.s16 q6, q10
        vadd.s16 q7, q11

        vqmovun.s16  d8, q4
        vqmovun.s16  d9, q5
        vqmovun.s16 d10, q6
        vqmovun.s16 d11, q7

        vstm.8   r0, {q2-q5}
        add    r0, r2
        bne    1b

        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

