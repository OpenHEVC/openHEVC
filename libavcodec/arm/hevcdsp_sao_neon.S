/*
 * Copyright (c) 2014 - 2015 Seppo Tomperi <seppo.tomperi@vtt.fi>
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

.macro init_sao_band
        ldr      r12, [sp, #0]    // offset_table address
        pld      [r1]
        vld1.8   {q0, q1}, [r12]  // offset table
        ldr      r12, [sp, #4]    // height
        vmov.u8  q3, #128
.endm

// 128 in q3
// input q8 - q11
// 32 cycles
.macro sao_band_64
        vshr.u8  q12, q8, #3
        vshr.u8  q13, q9, #3
        vshr.u8  q14, q10, #3
        vshr.u8  q15, q11, #3
        vtbl.8   d24, {d0, d1, d2, d3}, d24
        vadd.s8  q8, q3
        vtbl.8   d25, {d0, d1, d2, d3}, d25
        vadd.s8  q9, q3
        vtbl.8   d26, {d0, d1, d2, d3}, d26
        vadd.s8  q10, q3
        vtbl.8   d27, {d0, d1, d2, d3}, d27
        vadd.s8  q11, q3
        vtbl.8   d28, {d0, d1, d2, d3}, d28
        vqadd.s8 q8, q12
        vtbl.8   d29, {d0, d1, d2, d3}, d29
        vqadd.s8 q9, q13
        vtbl.8   d30, {d0, d1, d2, d3}, d30
        vqadd.s8 q10, q14
        vtbl.8   d31, {d0, d1, d2, d3}, d31
        vqadd.s8 q11, q15
        vsub.s8  q8, q3
        vsub.s8  q9, q3
        vsub.s8  q10, q3
        vsub.s8  q11, q3
.endm

function ff_hevc_sao_band_w8_neon_8, export=1
        init_sao_band
1:      subs     r12, #8
        vld1.8   {d16}, [r1, :64], r3
        vld1.8   {d17}, [r1, :64], r3
        vld1.8   {d18}, [r1, :64], r3
        vld1.8   {d19}, [r1, :64], r3
        vld1.8   {d20}, [r1, :64], r3
        vld1.8   {d21}, [r1, :64], r3
        vld1.8   {d22}, [r1, :64], r3
        vld1.8   {d23}, [r1, :64], r3
        sao_band_64
        vst1.8  {d16}, [r0, :64], r2
        vst1.8  {d17}, [r0, :64], r2
        vst1.8  {d18}, [r0, :64], r2
        vst1.8  {d19}, [r0, :64], r2
        vst1.8  {d20}, [r0, :64], r2
        vst1.8  {d21}, [r0, :64], r2
        vst1.8  {d22}, [r0, :64], r2
        vst1.8  {d23}, [r0, :64], r2
        bne    1b

        bx lr
endfunc

function ff_hevc_sao_band_w16_neon_8, export=1
        init_sao_band
1:      subs     r12, #4
        vld1.8  {q8}, [r1, :128], r3
        vld1.8  {q9}, [r1, :128], r3
        vld1.8  {q10}, [r1, :128], r3
        vld1.8  {q11}, [r1, :128], r3
        sao_band_64
        vst1.8   {q8}, [r0, :128], r2
        vst1.8   {q9}, [r0, :128], r2
        vst1.8   {q10}, [r0, :128], r2
        vst1.8   {q11}, [r0, :128], r2
        bne    1b

        bx lr
endfunc

function ff_hevc_sao_band_w32_neon_8, export=1
        init_sao_band
1:      subs     r12, #2
        vld1.8   {q8-q9}, [r1, :128], r3
        vld1.8   {q10-q11}, [r1, :128], r3
        sao_band_64
        vst1.8   {q8-q9}, [r0, :128], r2
        vst1.8   {q10-q11}, [r0, :128], r2
        bne      1b

        bx       lr
endfunc

function ff_hevc_sao_band_w64_neon_8, export=1
        init_sao_band
1:      subs      r12, #1
        pld       [r1, r3]
        vld1.8    {q8-q9}, [r1, :128]!
        vld1.8    {q10-q11}, [r1, :128], r3
        sub       r1, #32
        sao_band_64
        vst1.8    {q8-q9}, [r0, :128]!
        vst1.8    {q10-q11}, [r0, :128], r2
        sub       r0, #32
        bne       1b

        bx lr
endfunc
// input
// a in q0 - q3
// c in q4 - q7
// b in q8 - q11
// offset table in r7 and r5
// output in q0 - q3
// clobbers q12 - q15
.macro edge_w64_body
        vcgt.u8 q12,  q4, q0 // c > a -> -1 , otherwise 0
        vcgt.u8  q0,  q0, q4 // a > c -> -1 , otherwise 0
        vcgt.u8 q13,  q5, q1
        vcgt.u8  q1,  q1, q5
        vsub.s8 q12,  q0, q12 // diff0
        vcgt.u8  q0,  q4, q8 // c > b
        vsub.s8 q13,  q1, q13

        vcgt.u8 q14,  q8, q4 // b > c
        vcgt.u8  q1,  q5, q9
        vcgt.u8 q15,  q9, q5
        vsub.s8  q0, q14, q0 // diff1
        vsub.s8  q1, q15, q1

        vadd.s8  q0, q12 //diff0 + diff1
        vadd.s8  q1, q13

        vcgt.u8 q14,  q6, q2
        vcgt.u8  q2,  q2, q6
        vcgt.u8 q15,  q7, q3
        vcgt.u8  q3,  q3, q7

        vsub.s8 q14,  q2, q14
        vcgt.u8  q2,  q6, q10
        vsub.s8 q15,  q3, q15

        vcgt.u8 q12, q10, q6
        vcgt.u8  q3,  q7, q11
        vcgt.u8 q13, q11, q7
        vsub.s8  q2, q12, q2
        vsub.s8  q3, q13, q3

        vmov.s8 q13, #2 // 2 to all elements

        vadd.s8  q2, q14
        vadd.s8  q3, q15

        vmov.32  d24[0], r4  // load offset table from general registers
        vmov.32  d24[1], r5  // load rest of offset table

        vadd.s8 q0, q13
        vadd.s8 q1, q13
        vadd.s8 q2, q13
        vadd.s8 q3, q13

        vmov.u8  q15, #128 // s8 #-128
        vtbl.8   d0, {d24}, d0
        vadd.s8  q13,  q4, q15
        vtbl.8   d1, {d24}, d1
        vadd.s8  q14,  q5, q15
        vtbl.8   d2, {d24}, d2
        vqadd.s8 q0, q13
        vtbl.8   d3, {d24}, d3
        vqadd.s8 q1, q14
        vtbl.8   d4, {d24}, d4
        vadd.s8  q13,  q6, q15
        vtbl.8   d5, {d24}, d5
        vadd.s8  q14,  q7, q15
        vtbl.8   d6, {d24}, d6
        vqadd.s8 q2, q13
        vtbl.8   d7, {d24}, d7
        vqadd.s8 q3, q14
        vsub.s8   q0, q15
        vsub.s8   q1, q15
        vsub.s8   q2, q15
        vsub.s8   q3, q15
        vst1.8  {q0-q1}, [r0, :128]!
        vst1.8  {q2-q3}, [r0, :128], r2
        sub     r0, #32
.endm

.macro init_edge_64
        push   {r4-r5}
        ldr    r12, [sp, #8] // height
        ldr    r5, [sp, #12] // sao_offset_val_table
        ldr    r4, [r5]
        add    r5, #4
        ldr    r5, [r5]
.endm

function ff_hevc_sao_edge_eo0_w64_neon_8, export=1
        init_edge_64
        vpush {d8-d15}
        sub    r1, #8
1:      subs    r12, #1
        vld1.64  {d7}, [r1, :64]!
        vld1.64  {q4-q5}, [r1, :128]! // load c
        vld1.64  {q6-q7}, [r1, :128]!
        vld1.64  {d24}, [r1, :64], r3
        sub      r1, #72
        // load a
        vext.8 q0, q3, q4, #15
        vext.8 q1, q4, q5, #15
        vext.8 q2, q5, q6, #15
        vext.8 q3, q6, q7, #15
        // load b
        vext.8 q8, q4, q5, #1
        vext.8 q9, q5, q6, #1
        vext.8 q10, q6, q7, #1
        vext.8 q11, q7, q12, #1
        edge_w64_body
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r5}
        bx lr
endfunc

function ff_hevc_sao_edge_eo1_w64_neon_8, export=1
        init_edge_64
        vpush {d8-d15}
        sub     r1, r3
        // load a
        vld1.8  {q0-q1}, [r1, :128]!
        vld1.8  {q2-q3}, [r1, :128], r3
        sub     r1, #32
        // load c
        vld1.8  {q4-q5}, [r1, :128]!
        vld1.8  {q6-q7}, [r1, :128], r3
        sub     r1, #32
1:      subs    r12, #1
        // load b
        vld1.8  {q8-q9}, [r1, :128]!
        vld1.8  {q10-q11}, [r1, :128], r3
        sub     r1, #32
        edge_w64_body
        // copy c to a
        vmov.64 q0, q4
        vmov.64 q1, q5
        vmov.64 q2, q6
        vmov.64 q3, q7
        // copy b to c
        vmov.64 q4, q8
        vmov.64 q5, q9
        vmov.64 q6, q10
        vmov.64 q7, q11
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r5}
        bx lr
endfunc

function ff_hevc_sao_edge_eo2_w64_neon_8, export=1
        init_edge_64
        vpush {d8-d15}
1:      sub     r1, r3
        // load a
        // TODO: fix unaligned load
        //       don't reload a like in eo1
        sub     r1, #1
        vld1.8  {q0-q1}, [r1]!
        vld1.8  {q2-q3}, [r1], r3
        sub     r1, #31
        subs    r12, #1
        // load c
        vld1.8  {q4-q5}, [r1, :128]!
        vld1.8  {q6-q7}, [r1, :128], r3
        sub     r1, #32
        // load b
        add     r1, #1
        vld1.8  {q8-q9}, [r1]!
        vld1.8  {q10-q11}, [r1]
        sub     r1, #33
        edge_w64_body
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r5}
        bx lr
endfunc

function ff_hevc_sao_edge_eo3_w64_neon_8, export=1
        init_edge_64
        vpush {d8-d15}
1:      sub     r1, r3
        // load a
        // TODO: fix unaligned load
        //       don't reload a like in eo1
        add     r1, #1
        vld1.8  {q0-q1}, [r1]!
        vld1.8  {q2-q3}, [r1], r3
        sub     r1, #33
        subs    r12, #1
        // load c
        vld1.8  {q4-q5}, [r1, :128]!
        vld1.8  {q6-q7}, [r1, :128], r3
        sub     r1, #32
        // load b
        sub     r1, #1
        vld1.8  {q8-q9}, [r1]!
        vld1.8  {q10-q11}, [r1]
        sub     r1, #31
        edge_w64_body
        bne   1b
        vpop  {d8-d15}
        pop   {r4-r5}
        bx lr
endfunc

// inputs:
// a in q0, q1
// c in q2, q3
// b in q8, q9
// offset table in d31
// clobbered registers q0, q1, q10, q11, q12, q13
// output q0, q1
.macro edge_w32_body
        vcgt.u8 q12, q2, q0 // c > a -> -1 , otherwise 0
        vcgt.u8 q0,  q0, q2 // a > c -> -1 , otherwise 0
        vcgt.u8 q13, q3, q1
        vcgt.u8 q1,  q1, q3

        vsub.s8 q12, q0, q12 // diff0
        vcgt.u8  q0,  q2, q8 // c > b
        vsub.s8 q13, q1, q13 // diff0 part 2

        vcgt.u8  q10,  q8, q2 // b > c
        vcgt.u8  q1,  q3, q9
        vcgt.u8  q11,  q9, q3

        vsub.s8 q0, q10, q0 // diff1

        vmov.s8 q10, #2 // 2 to all elements
        vsub.s8 q1, q11, q1 // diff1 part 2
        vadd.s8 q0, q12 //diff0 + diff1
        vadd.s8 q1, q13

        vadd.s8 q0, q10
        vadd.s8 q1, q10

        vmov.u8  q10, #128
        vtbl.8   d0, {d31}, d0
        vadd.s8  q11, q2, q10
        vtbl.8   d1, {d31}, d1
        vadd.s8  q12, q3, q10
        vtbl.8   d2, {d31}, d2
        vqadd.s8 q11, q0
        vtbl.8   d3, {d31}, d3
        vqadd.s8   q12, q1
        vsub.s8    q0, q11, q10
        vsub.s8    q1, q12, q10
        vst1.8   {q0-q1}, [r0, :128], r2
.endm

.macro init_edge_32
        ldr     r12, [sp, #4] // sao_offset_val_table
        vld1.32 {d31}, [r12]
        ldr     r12, [sp] // height
.endm

function ff_hevc_sao_edge_eo0_w32_neon_8, export=1
        init_edge_32
        sub     r1, #4 // load 4 extra bytes
1:      subs    r12, #1
        vld1.32 d3[1], [r1]!
        vld1.8  {q2-q3}, [r1, :128]! // c
        vld1.32 d20[0], [r1], r3
        sub     r1, #36
        // a
        vext.8  q0, q1, q2, #15
        vext.8  q1, q2, q3, #15
        // b
        vext.8  q8, q2, q3, #1
        vext.8  q9, q3, q10, #1
        edge_w32_body
        bne     1b
        bx      lr
endfunc

function ff_hevc_sao_edge_eo1_w32_neon_8, export=1
        init_edge_32
        // load a
        sub     r1, r3
        vld1.8  {q0-q1}, [r1, :128], r3
        // load c
        vld1.8  {q2-q3}, [r1, :128], r3
1:      subs    r12, #1
        // load b
        vld1.8  {q8-q9}, [r1, :128], r3
        edge_w32_body
        // inputs for next loop iteration
        // a
        vmov.64 q0, q2
        vmov.64 q1, q3
        // c
        vmov.64 q2, q8
        vmov.64 q3, q9
        bne     1b
        bx      lr
endfunc

function ff_hevc_sao_edge_eo2_w32_neon_8, export=1
        init_edge_32
        vpush   {d8-d15}
        // load a
        sub     r1, r3
        sub     r1, #8
        vld1.8  {q10-q11}, [r1, :64]!
        vld1.8  {d24}, [r1, :64], r3
        sub     r1, #32
        vext.8  q0, q10, q11, #7
        vext.8  q1, q11, q12, #7
        // load c
        vld1.8  {d9}, [r1, :64]!
        vld1.8  {q2-q3}, [r1, :64], r3
        sub     r1, #8
        vext.8  q4, q4, q2, #15
1:      subs    r12, #1
        // load b
        vld1.8  {q10-q11}, [r1, :64]!
        vld1.8  {q12}, [r1, :64], r3
        sub     r1, #32
        vext.8  q8, q10, q11, #9
        vext.8  q9, q11, q12, #9
        vext.8  q6, q10, q11, #8
        vext.8  q7, q11, q12, #8
        vext.8  q5, q10, q11, #7
        edge_w32_body
        // inputs for next loop iteration
        // a
        vmov.8  q0, q4
        vext.8  q1, q2, q3, #15
        // c
        vmov.8  q2, q6
        vmov.8  q3, q7
        vmov.8  q4, q5
        bne     1b
        vpop    {d8-d15}
        bx      lr
endfunc

function ff_hevc_sao_edge_eo3_w32_neon_8, export=1
        init_edge_32
        sub     r1, r3
        // load a
        vld1.8  {q10-q11}, [r1, :64]!
        vld1.8  {d24}, [r1, :64], r3
        sub     r1, #32
        vext.8  q0, q10, q11, #1
        vext.8  q1, q11, q12, #1
        // load c
        vld1.8  {q2-q3}, [r1, :64]!
        vld1.8  {d30}, [r1, :64], r3
        sub     r1, #40
1:      subs    r12, #1
        // load b
        vld1.8  {q10-q11}, [r1, :64]!
        vld1.8  {q12}, [r1, :64], r3
        sub     r1, #32
        vext.8  q8, q10, q11, #7
        vext.8  q9, q11, q12, #7
        vext.8  q14, q12, q10, #7
        edge_w32_body
        // inputs for next loop iteration
        // a
        vext.8  q0, q2, q3, #1
        vext.8  q1, q3, q15, #1
        // c
        vext.8  q2, q8, q9, #1
        vext.8  q3, q9, q14, #1
        vext.8  d30, d28, d2, #1
        bne     1b
        bx      lr
endfunc

