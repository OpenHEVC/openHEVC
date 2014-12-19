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
        ldr      r12, [sp, #4]    // offset_table address
        vld1.8   {q0, q1}, [r12]  // offset table
        ldr      r12, [sp, #0]    // height

1:      subs     r12, #1
        vld1.8   {d24}, [r1,:64], r3
        vshr.u8  d16, d24, #3
        vtbl.8   d16, {q0, q1}, d16
        vmovl.u8 q6, d24
        vaddw.s8 q6, d16
        vqmovun.s16 d4, q2
        vst1.8  {d4}, [r0,:64], r2
        bne    1b

        bx lr
endfunc

function ff_hevc_sao_band_w16_neon_8, export=1
        ldr      r12, [sp, #4]    // offset_table address
        vld1.8   {q0, q1}, [r12]  // offset table
        ldr      r12, [sp, #0]    // height

1:      subs     r12, #1
        vld1.8  {q12}, [r1,:128], r3
        vshr.u8   q8, q12, #3
        vtbl.8  d16, {q0, q1}, d16
        vtbl.8  d17, {q0, q1}, d17
        vmovl.u8 q10, d24
        vmovl.u8 q11, d25
        vaddw.s8 q10, d16
        vaddw.s8 q11, d17
        vqmovun.s16 d4, q10
        vqmovun.s16 d5, q11
        vst1.8   {q2}, [r0,:128], r2
        bne    1b

        bx lr
endfunc

function ff_hevc_sao_band_w32_neon_8, export=1
        ldr      r12, [sp, #4]    // offset_table address
        vld1.8   {q0, q1}, [r12]  // offset table
        ldr      r12, [sp, #0]    // height

1:      subs     r12, #1
        vld1.8   {q2-q3}, [r1,:128], r3
        vshr.u8  q8, q2, #3
        vshr.u8  q9, q3, #3
        vtbl.8   d16, {q0, q1}, d16
        vtbl.8   d17, {q0, q1}, d17
        vtbl.8   d18, {q0, q1}, d18
        vtbl.8   d19, {q0, q1}, d19
        vmovl.u8 q12, d4
        vmovl.u8 q13, d5
        vmovl.u8 q14, d6
        vmovl.u8 q15, d7
        vaddw.s8 q12, d16
        vaddw.s8 q13, d17
        vaddw.s8 q14, d18
        vaddw.s8 q15, d19
        vqmovun.s16 d4, q12
        vqmovun.s16 d5, q13
        vqmovun.s16 d6, q14
        vqmovun.s16 d7, q15
        vst1.8   {q2-q3}, [r0,:128], r2
        bne      1b

        bx       lr
endfunc

function ff_hevc_sao_band_w64_neon_8, export=1
        ldr      r12, [sp, #4]    // offset_table address
        vld1.8   {q0, q1}, [r12]  // offset table
        ldr      r12, [sp, #0]    // height

1:      subs     r12, #1
        vld1.8  {q12-q13}, [r1,:128]!
        vld1.8  {q14-q15}, [r1,:128], r3
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

        vmovl.u8 q2, d24
        vmovl.u8 q3, d25
        vmovl.u8 q12, d26
        vmovl.u8 q13, d27

        vaddw.s8 q2, d16
        vaddw.s8 q3, d17
        vaddw.s8 q12, d18
        vaddw.s8 q13, d19

        vqmovun.s16 d4, q2
        vqmovun.s16 d5, q3
        vqmovun.s16 d6, q12
        vqmovun.s16 d7, q13

        vmovl.u8 q12, d28
        vmovl.u8 q13, d29
        vmovl.u8 q14, d30
        vmovl.u8 q15, d31

        vaddw.s8 q12, d20
        vaddw.s8 q13, d21
        vaddw.s8 q14, d22
        vaddw.s8 q15, d23

        vqmovun.s16  d8, q12
        vqmovun.s16  d9, q13
        vqmovun.s16 d10, q14
        vqmovun.s16 d11, q15

        vst1.8     {q2-q3}, [r0,:128]!
        vst1.8     {q4-q5}, [r0,:128], r2
        sub    r0, #32
        bne    1b

        bx lr
endfunc

.macro edge_w64_body
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

        vadd.s8 q0, q12 //diff0 + diff1
        vadd.s8 q1, q13
        vadd.s8 q2, q14
        vadd.s8 q3, q15

        vdup.s8 q9, r6 // 3 to all elements
        sub     r6, #1

        vclt.s8 q12, q0, #0 // diff0 + diff1 < 0
        vclt.s8 q13, q1, #0
        vclt.s8 q14, q2, #0
        vclt.s8 q15, q3, #0

        vadd.s8  q8,  q0, q9 // diff0 + diff1 + 3
        vadd.s8  q10,  q1, q9
        vand.8   q12, q8, q12 // if (diff0 + diff1 < 0) then (diff0 + diff1 + 3) else 0
        vand.8   q13, q10, q13
        vadd.s8  q8,  q2, q9
        vadd.s8  q10,  q3, q9
        vand.8   q14, q8, q14
        vand.8   q15, q10, q15

        vdup.s8 q9, r6  // 2 to all elements
        add     r6, #1

        vcgt.s8  q10, q0, #0 // diff0 + diff1 > 0
        vadd.s8   q8, q0, q9 // diff0 + diff1 + 2
        vand.8   q11, q8, q10 // if (diff0 + diff1 > 0) then (diff0 + diff1 + 2) else 0
        vcgt.s8  q10, q1, #0
        vadd.s8   q0, q11, q12  // offset_idx

        vadd.s8   q8, q1, q9 // diff0 + diff1 + 2
        vcgt.s8  q12, q2, #0
        vand.8   q11, q8, q10 // if (diff0 + diff1 > 0) then (diff0 + diff1 + 2) else 0
        vadd.s8   q8, q2, q9 // diff0 + diff1 + 2
        vadd.s8   q1, q11, q13

        vand.8   q11, q8, q12 // if (diff0 + diff1 > 0) then (diff0 + diff1 + 2) else 0
        vcgt.s8  q10, q3, #0
        vadd.s8   q2, q11, q14

        vadd.s8   q8, q3, q9 // diff0 + diff1 + 2
        vmov.32  d18[0], r7  // load offset table from general registers
        vand.8   q11, q8, q10 // if (diff0 + diff1 > 0) then (diff0 + diff1 + 2) else 0
        vmov.32  d18[1], r5  // load rest of offset table
        vadd.s8   q3, q11, q15

        vtbl.8   d0, {d18}, d0
        vtbl.8   d1, {d18}, d1
        vtbl.8   d2, {d18}, d2
        vtbl.8   d3, {d18}, d3
        vtbl.8   d4, {d18}, d4
        vtbl.8   d5, {d18}, d5
        vtbl.8   d6, {d18}, d6
        vtbl.8   d7, {d18}, d7

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
.endm

.macro edge_w32_body
        vcgt.u8 q12, q4, q0 // c > a -> -1 , otherwise 0
        vcgt.u8 q0,  q0, q4 // a > c -> -1 , otherwise 0
        vcgt.u8 q13, q5, q1
        vcgt.u8 q1,  q1, q5

        vsub.s8 q12, q0, q12 // diff0
        vcgt.u8  q0,  q4, q8 // c > b
        vsub.s8 q13, q1, q13 // diff0 part 2

        vcgt.u8  q6,  q8, q4 // b > c
        vcgt.u8  q1,  q5, q9
        vcgt.u8  q7,  q9, q5

        vsub.s8 q0, q6, q0 // diff1
        vsub.s8 q1, q7, q1 // diff1 part 2
        vadd.s8 q0, q12 //diff0 + diff1

        vdup.s8 q7, r6 // 3 to all elements
        sub     r6, #1
        vadd.s8 q1, q13

        vclt.s8 q12, q0, #0 // diff0 + diff1 < 0
        vclt.s8 q13, q1, #0

        vadd.s8  q6,  q0, q7 // diff0 + diff1 + 3
        vadd.s8  q10,  q1, q7
        vdup.s8 q7, r6  // 2 to all elements
        add     r6, #1
        vand.8   q12, q6, q12 // if (diff0 + diff1 < 0) then (diff0 + diff1 + 3) else 0
        vand.8   q13, q10, q13


        vcgt.s8  q10, q0, #0 // diff0 + diff1 > 0
        vadd.s8   q6, q0, q7 // diff0 + diff1 + 2
        vand.8   q11, q6, q10 // if (diff0 + diff1 > 0) then (diff0 + diff1 + 2) else 0
        vcgt.s8  q10, q1, #0
        vadd.s8   q0, q11, q12  // offset_idx

        vadd.s8   q6, q1, q7 // diff0 + diff1 + 2
        vmov.32  d14[0], r7  // load offset table from general registers
        vand.8   q11, q6, q10 // if (diff0 + diff1 > 0) then (diff0 + diff1 + 2) else 0
        vmov.32  d14[1], r5  // load rest of offset table
        vadd.s8   q1, q11, q13

        vtbl.8   d0, {d14}, d0
        vtbl.8   d1, {d14}, d1
        vtbl.8   d2, {d14}, d2
        vtbl.8   d3, {d14}, d3

        vmovl.u8   q6, d8
        vmovl.u8   q7, d9
        vmovl.u8  q10, d10
        vmovl.u8  q11, d11

        vaddw.s8  q6, d0
        vaddw.s8  q7, d1
        vaddw.s8 q10, d2
        vaddw.s8 q11, d3

        vqmovun.s16  d0, q6
        vqmovun.s16  d1, q7
        vqmovun.s16  d2, q10
        vqmovun.s16  d3, q11

        vstm r0, {q0-q1}
        add  r0, r2
.endm

function ff_hevc_sao_edge_eo0_w64_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        ldr    r5, [r5]
        vpush {d8-d15}
        sub    r1, #8
1:      subs    r4, #1
        vld1.64  {q10-q11}, [r1]!
        vld1.64  {q12-q13}, [r1]!
        vld1.64  {q14}, [r1], r3
        sub      r1, #64
        // load a
        vext.8 q0, q10, q11, #7
        vext.8 q1, q11, q12, #7
        vext.8 q2, q12, q13, #7
        vext.8 q3, q13, q14, #7
        // load c
        vext.8 q4, q10, q11, #8
        vext.8 q5, q11, q12, #8
        vext.8 q6, q12, q13, #8
        vext.8 q7, q13, q14, #8
        // load b
        vext.8 q8, q10, q11, #9
        vext.8 q9, q11, q12, #9
        vext.8 q10, q12, q13, #9
        vext.8 q11, q13, q14, #9
        edge_w64_body
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_edge_eo1_w64_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        ldr    r5, [r5]
        vpush {d8-d15}
        sub     r1, r3
        // load a
        vld1.8  {q0-q1}, [r1]!
        vld1.8  {q2-q3}, [r1], r3
        sub     r1, #32
1:      subs    r4, #1
        // load c
        vld1.8  {q4-q5}, [r1]!
        vld1.8  {q6-q7}, [r1], r3
        sub     r1, #32
        // load b
        vld1.8  {q8-q9}, [r1]!
        vld1.8  {q10-q11}, [r1]
        sub     r1, #32
        edge_w64_body
        // copy c to a
        vmov.64 q0, q4
        vmov.64 q1, q5
        vmov.64 q2, q6
        vmov.64 q3, q7
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_edge_eo2_w64_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        ldr    r5, [r5]
        vpush {d8-d15}
1:      sub     r1, r3
        // load a
        // TODO: fix unaligned load
        //       don't reload a like in eo1
        sub     r1, #1
        vld1.8  {q0-q1}, [r1]!
        vld1.8  {q2-q3}, [r1], r3
        sub     r1, #31
        subs    r4, #1
        // load c
        vld1.8  {q4-q5}, [r1]!
        vld1.8  {q6-q7}, [r1], r3
        sub     r1, #32
        // load b
        add     r1, #1
        vld1.8  {q8-q9}, [r1]!
        vld1.8  {q10-q11}, [r1]
        sub     r1, #33
        edge_w64_body
        // copy c to a
        vmov.64 q0, q4
        vmov.64 q1, q5
        vmov.64 q2, q6
        vmov.64 q3, q7
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_edge_eo3_w64_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        ldr    r5, [r5]
        vpush {d8-d15}
1:      sub     r1, r3
        // load a
        // TODO: fix unaligned load
        //       don't reload a like in eo1
        add     r1, #1
        vld1.8  {q0-q1}, [r1]!
        vld1.8  {q2-q3}, [r1], r3
        sub     r1, #33
        subs    r4, #1
        // load c
        vld1.8  {q4-q5}, [r1]!
        vld1.8  {q6-q7}, [r1], r3
        sub     r1, #32
        // load b
        sub     r1, #1
        vld1.8  {q8-q9}, [r1]!
        vld1.8  {q10-q11}, [r1]
        sub     r1, #31
        edge_w64_body
        // copy c to a
        vmov.64 q0, q4
        vmov.64 q1, q5
        vmov.64 q2, q6
        vmov.64 q3, q7
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_edge_eo0_w32_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        ldr    r5, [r5]
        vpush {d8-d15}
        sub    r1, #8 // load 8 extra bytes
1:      subs    r4, #1
        vld1.8  {q10-q11}, [r1]
        add    r1, #32
        vld1.8  {q12}, [r1], r3 // only first 9 bytes are used
        sub    r1, #32
        // a
        vext.8  q0, q10, q11, #7
        vext.8  q1, q11, q12, #7
        // c
        vext.8  q4, q10, q11, #8
        vext.8  q5, q11, q12, #8
        // b
        vext.8  q8, q10, q11, #9
        vext.8  q9, q11, q12, #9
        edge_w32_body
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_edge_eo1_w32_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        ldr    r5, [r5]
        vpush {d8-d15}
        // load a
        sub     r1, r3
        vld1.8  {q0-q1}, [r1], r3
        // load c
        vld1.8  {q4-q5}, [r1], r3
1:      subs    r4, #1
        // load b
        vld1.8  {q8-q9}, [r1], r3
        edge_w32_body
        // inputs for next loop iteration
        // a
        vmov.64 q0, q4
        vmov.64 q1, q5
        // c
        vmov.64 q4, q8
        vmov.64 q5, q9
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_edge_eo2_w32_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        ldr    r5, [r5]
        vpush {d8-d15}
        // load a
        sub     r1, r3
        sub    r1, #8
        vld1.8  {q10-q11}, [r1]
        add    r1, #32
        vld1.8  {q12}, [r1], r3
        sub    r1, #32
        vext.8  q0, q10, q11, #7
        vext.8  q1, q11, q12, #7
        // load c
        vld1.8  {q10-q11}, [r1]
        add    r1, #32
        vld1.8  {q12}, [r1], r3
        sub    r1, #32
        vext.8  q4, q10, q11, #8
        vext.8  q5, q11, q12, #8
        vext.8  q2, q10, q11, #7
1:      subs    r4, #1
        // load b
        vld1.8  {q10-q11}, [r1]
        add    r1, #32
        vld1.8  {q12}, [r1], r3
        sub    r1, #32
        vext.8  q8, q10, q11, #9
        vext.8  q9, q11, q12, #9
        vext.8  q14, q10, q11, #8
        vext.8  q15, q11, q12, #8
        vext.8  q3, q10, q11, #7
        edge_w32_body
        // inputs for next loop iteration
        // a
        vmov.8 q0, q2
        vext.8 q1, q4, q5, #15
        // c
        vmov.8  q4, q14
        vmov.8  q5, q15
        vmov.8  q2, q3
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

function ff_hevc_sao_edge_eo3_w32_neon_8, export=1
        push  {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r5, [sp, #24] // sao_offset_val_table
        ldr    r6, =0x03
        ldr    r7, [r5]
        add    r5, #4
        sub    r1, r3
        ldr    r5, [r5]
        sub    r1, #8
        vpush {d8-d15}
        // load a
        vld1.8  {q10-q11}, [r1]
        add    r1, #32
        vld1.8  {q12}, [r1], r3
        sub    r1, #32
        vext.8  q0, q10, q11, #9
        vext.8  q1, q11, q12, #9
        // load c
        vld1.8  {q10-q11}, [r1]
        add    r1, #32
        vld1.8  {q12}, [r1], r3
        sub    r1, #32
        vext.8  q4, q10, q11, #8
        vext.8  q5, q11, q12, #8
        vext.8  q2, q12, q11, #8
1:      subs    r4, #1
        // load b
        vld1.8  {q10-q11}, [r1]
        add    r1, #32
        vld1.8  {q12}, [r1], r3
        sub    r1, #32
        vext.8  q8, q10, q11, #7
        vext.8  q9, q11, q12, #7
        vext.8  q3, q12, q10, #7
        edge_w32_body
        // inputs for next loop iteration
        // a
        vext.8 q0, q4, q5, #1
        vext.8 q1, q5, q2, #1
        // c
        vext.8  q4, q8, q9, #1
        vext.8  q5, q9, q3, #1
        vext.8  q2, q3, q1, #1
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r8}
        bx lr
endfunc

