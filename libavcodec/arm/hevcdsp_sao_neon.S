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

function ff_hevc_sao_edge_eo1_w64_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x02
        vpush {d8-d15}
1:      subs    r4, #1
        // load a
        sub     r1, r3
        vld1.8  {q0-q1}, [r1]!
        vld1.8  {q2-q3}, [r1], r3
        sub     r1, #32
        // load c
        vld1.8  {q4-q5}, [r1]!
        vld1.8  {q6-q7}, [r1], r3
        sub     r1, #32
        // load b
        vld1.8  {q8-q9}, [r1]!
        vld1.8  {q10-q11}, [r1], r3
        sub     r1, #32

        vcgt.u8 q12, q4, q0 // c > a -> -1 , otherwise 0
        vcgt.u8 q0,  q0, q4 // a > c -> -1 , otherwise 0
        vcgt.u8 q13, q5, q1
        vcgt.u8 q1,  q1, q5
        vcgt.u8 q14, q6, q2
        vcgt.u8 q2,  q2, q6
        vcgt.u8 q15, q7, q3
        vcgt.u8 q3,  q3, q7

        vsub.s8 q12, q0, q12 // diff0
        vsub.s8 q13, q1, q13
        vsub.s8 q14, q2, q14
        vsub.s8 q15, q3, q15

        vcgt.u8  q0,  q4, q8 // c > b
        vcgt.u8  q8,  q8, q4 // b > c
        vcgt.u8  q1,  q5, q9
        vcgt.u8  q9,  q9, q5
        vcgt.u8  q2,  q6, q10
        vcgt.u8 q10, q10, q6
        vcgt.u8  q3,  q7, q11
        vcgt.u8 q11, q11, q7

        vsub.s8 q0, q8, q0 // diff1
        vsub.s8 q1, q9, q1
        vsub.s8 q2, q10, q2
        vsub.s8 q3, q11, q3

        veor.u8 q8, q8  // zero register
        vdup.s8 q9, r6  // 2 to all elements
        add     r6, #1
        vdup.s8 q10, r6 // 3 to all elements
        sub     r6, #1

        vadd.s8 q0, q12 //diff0 + diff1
        vadd.s8 q1, q13
        vadd.s8 q2, q14
        vadd.s8 q3, q15

        vcgt.s8 q4, q0, q8 // diff0 + diff1 > 0
        vcgt.s8 q5, q1, q8
        vcgt.s8 q6, q2, q8
        vcgt.s8 q7, q3, q8

        vclt.s8 q11, q0, q8 // diff0 + diff1 < 0
        vclt.s8 q12, q1, q8
        vclt.s8 q13, q2, q8
        vclt.s8 q14, q3, q8

        vadd.s8  q8,  q0, q9  // diff0 + diff1 + 2
        vand.8  q15,  q8, q4
        vadd.s8  q8,  q0, q10 // diff0 + diff1 + 3
        vand.8   q8,  q8, q11
        vadd.s8  q0, q15, q8  // offset_idx

        vadd.s8  q8,  q1, q9  // diff0 + diff1 + 2
        vand.8  q15,  q8, q5
        vadd.s8  q8,  q1, q10 // diff0 + diff1 + 3
        vand.8   q8,  q8, q12
        vadd.s8  q1, q15, q8  // offset_idx

        vadd.s8  q8,  q2, q9  // diff0 + diff1 + 2 + 2
        vand.8  q15,  q8, q6
        vadd.s8  q8,  q2, q10 // diff0 + diff1 + 2 + 3
        vand.8   q8,  q8, q13
        vadd.s8  q2, q15, q8  // offset_idx

        vadd.s8  q8,  q3, q9  // diff0 + diff1 + 2 + 2
        vand.8  q15,  q8, q7
        vadd.s8  q8,  q3, q10 // diff0 + diff1 + 2 + 3
        vand.8   q8,  q8, q14
        vadd.s8  q3, q15, q8  // offset_idx
        // TODO: load only once
        vld1.8   d16, [r5]

        vtbl.8   d0, {d16}, d0
        vtbl.8   d1, {d16}, d1
        vtbl.8   d2, {d16}, d2
        vtbl.8   d3, {d16}, d3
        vtbl.8   d4, {d16}, d4
        vtbl.8   d5, {d16}, d5
        vtbl.8   d6, {d16}, d6
        vtbl.8   d7, {d16}, d7

        // TODO: load only once
        // load c again
        sub     r1, r3
        sub     r1, r3
        vld1.8  {q4-q5}, [r1]!
        vld1.8  {q6-q7}, [r1], r3
        sub     r1, #32

        vmovl.u8   q8, d8
        vmovl.u8   q9, d9
        vmovl.u8  q10, d10
        vmovl.u8  q11, d11
        vmovl.u8  q12, d12
        vmovl.u8  q13, d13
        vmovl.u8  q14, d14
        vmovl.u8  q15, d15

        vaddw.s8  q8, d0
        vaddw.s8  q9, d1
        vaddw.s8 q10, d2
        vaddw.s8 q11, d3
        vaddw.s8 q12, d4
        vaddw.s8 q13, d5
        vaddw.s8 q14, d6
        vaddw.s8 q15, d7

        vqmovun.s16  d0, q8
        vqmovun.s16  d1, q9
        vqmovun.s16  d2, q10
        vqmovun.s16  d3, q11
        vqmovun.s16  d4, q12
        vqmovun.s16  d5, q13
        vqmovun.s16  d6, q14
        vqmovun.s16  d7, q15

        vstm r0, {q0-q3}
        add  r0, r2
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc
