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

.macro vextin_d4
    vld1.8    {q10}, [r2], r3
    vmov      d16, d20
    vext.8    d17, d20, d21, #1
    vext.8    d18, d20, d21, #2
    vext.8    d19, d20, d21, #3
.endm

.macro vextin_d4_8
    vld1.8    d16, [r2], r3
    vext.8    d17, d16, d16, #1
    vext.8    d18, d16, d16, #2
    vext.8    d19, d16, d16, #3
.endm


.macro load_coeffs_16b coeffs
    ldr      \coeffs, [\coeffs]
    vdup.i8  d0, \coeffs
    lsr      \coeffs, #8
    vdup.i8  d1, \coeffs
    lsr      \coeffs, #8
    vdup.i8  d2, \coeffs
    lsr      \coeffs, #8
    vdup.i8  d3, \coeffs
.endm

.macro epel_filter_16b out=q12
    vmull.u8 q3, d16, d0
    vmull.u8 q11, d19, d3
    vmull.u8 \out, d17, d1
    vmull.u8 q10, d18, d2
    vadd.s16 q3, q11
    vadd.s16 \out, q10
    vsub.s16 \out, q3
.endm

.macro load_coeffs_32b coeffs
    ldr      \coeffs, [\coeffs]
    vmov.i64 d4, #0
    vmov.8   d4[0], \coeffs
    lsr      \coeffs, #8
    vmov.8   d4[2], \coeffs
    lsr      \coeffs, #8
    vmov.8   d4[4], \coeffs
    lsr      \coeffs, #8
    vmov.8   d4[6], \coeffs
.endm

.macro epel_filter_32b
    vmull.s16 q3, d24, d4[0] //q12
    vmull.s16 q4, d25, d4[0]
    vmull.s16 q5, d30, d4[3] //q15
    vmull.s16 q6, d31, d4[3]

    vmull.s16 q7, d26, d4[1] // q13
    vmull.s16 q8, d27, d4[1]
    vmull.s16 q9, d28, d4[2] // q14
    vmull.s16 q10, d29, d4[2]
    vadd.s32 q3, q5
    vadd.s32 q4, q6
    vadd.s32 q7, q9
    vadd.s32 q8, q10
    vsub.s32 q7, q3
    vsub.s32 q8, q4
    vqshrn.s32  d6, q7, #6
    vqshrn.s32  d7, q8, #6
.endm

.macro epel_filter_32b_4
    vmull.s16 q3, d24, d4[0] //q12
    vmull.s16 q5, d30, d4[3] //q15
    vmull.s16 q7, d26, d4[1] // q13
    vmull.s16 q9, d28, d4[2] // q14
    vadd.s32 q3, q5
    vadd.s32 q7, q9
    vsub.s32 q7, q3
    vqshrn.s32  d6, q7, #6
.endm

function ff_hevc_put_epel_h_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r7, [sp, #40] // mx
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs
        add    r7, r12
        sub       r2, #1
        lsl       r1, #1
        load_coeffs_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      subs r4, #1
        pld [r2]
        vextin_d4
        epel_filter_16b
        vst1.16    {q12}, [r0], r1
        bne 8b
        subs    r5, #8
        beq  99f
        mov       r4, r12
        add       r6, #16
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        cmp       r5, #4
        bgt       8b
4:      subs r4, #1
        pld [r2]
        vextin_d4_8
        epel_filter_16b
		vst1.16    d24, [r0], r1
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
2:      subs r4, #1
        pld [r2]
        vextin_d4_8
        epel_filter_16b
        vst1.32    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel_uni_h_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r7, [sp, #40] // mx
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl   r12, epel_coeffs
        add    r7, r12
        sub       r2, #1
        load_coeffs_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      subs r4, #1
        pld [r2]
        vextin_d4
        epel_filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.8    d24, [r0], r1
        bne       8b
        subs      r5, #8
        beq      99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        cmp       r5, #4
        bgt       8b
4:      subs r4, #1
        pld [r2]
        vextin_d4_8
        epel_filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.32    d24[0], [r0], r1
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #4
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
2:      subs r4, #1
        pld [r2]
        vextin_d4_8
        epel_filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.16    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel_bi_h_neon_8, export=1
        push   {r4-r12}
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        ldr    r4, [sp, #44] // height
        ldr    r7, [sp, #48] // mx
        ldr    r5, [sp, #56] // width
        sub    r7, #1
        lsl    r7, #2
        vpush  {d8-d15}
        adrl   r12, epel_coeffs
        add    r7, r12
        sub    r2, #1
        load_coeffs_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
        mov   r10, r8
        lsl   r9, #1
        cmp   r5, #6
        bgt       8f
        cmp   r5, #4
        blt   2f
        b     4f
8:      subs r4, #1
        pld [r2]
        vextin_d4
        epel_filter_16b
        vld1.16        {q3}, [r8], r9
        vqadd.s16      q3, q12
        vqrshrun.s16   d6, q3, #7
        vst1.8         d6, [r0], r1
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
        bgt       8b
4:      subs r4, #1
        pld [r2]
        vextin_d4_8
        epel_filter_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.32      d6[0], [r0], r1
        bne       4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #4
        add       r10, #8
        mov       r8, r10
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
2:      subs r4, #1
        pld [r2]
        vextin_d4_8
        epel_filter_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.16      d6[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_put_epel_v_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r7, [sp, #44] // my
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs
        add    r7, r12
        load_coeffs_16b r7
        sub       r2, r3
        lsl       r1, #1
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld [r2]
        vld1.8    {d16}, [r2], r3
        pld [r2]
        vld1.8    {d17}, [r2], r3
        pld [r2]
        vld1.8    {d18}, [r2], r3
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld [r2]
        vld1.8    {d19}, [r2], r3
        subs r4, #1
        epel_filter_16b
        vst1.16    {q12}, [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 8b
        subs    r5, #8
        beq  99f
        mov       r4, r12
        add       r6, #16
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        b         0b
4:      pld       [r2]
        vld1.8    {d19}, [r2], r3
        subs r4, #1
        epel_filter_16b
        vst1.16    d24, [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
        b         0b
2:      pld [r2]
        vld1.8    {d19}, [r2], r3
        subs r4, #1
        epel_filter_16b
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel_uni_v_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r7, [sp, #44] // my
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl   r12, epel_coeffs
        add    r7, r12
        sub       r2, r3
        load_coeffs_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld [r2]
        vld1.8    {d16}, [r2], r3
        pld [r2]
        vld1.8    {d17}, [r2], r3
        pld [r2]
        vld1.8    {d18}, [r2], r3
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld [r2]
        vld1.8    {d19}, [r2], r3
        subs r4, #1
        epel_filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.8    d24, [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne       8b
        subs      r5, #8
        beq      99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        b         0b
4:      pld       [r2]
        vld1.8    {d19}, [r2], r3
        subs r4, #1
        epel_filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #4
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
        b         0b
2:      pld [r2]
        vld1.8    {d19}, [r2], r3
        subs r4, #1
        epel_filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.16    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel_bi_v_neon_8, export=1
        push   {r4-r12}
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        ldr    r4, [sp, #44] // height
        ldr    r7, [sp, #52] // my
        ldr    r5, [sp, #56] // width
        sub    r7, #1
        lsl    r7, #2
        vpush  {d8-d15}
        adrl   r12, epel_coeffs
        add    r7, r12
		lsl    r9, #1
        sub    r2, r3
        load_coeffs_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:		pld [r2]
        vld1.8    {d16}, [r2], r3
        pld [r2]
        vld1.8    {d17}, [r2], r3
        pld [r2]
        vld1.8    {d18}, [r2], r3
        mov   r10, r8
        cmp   r5, #6
        bgt       8f
        cmp   r5, #4
        blt   2f
        b     4f
8:      subs r4, #1
        pld [r2]
        vld1.8    {d19}, [r2], r3
        epel_filter_16b
        vld1.16        {q3}, [r8], r9
        vqadd.s16      q3, q12
        vqrshrun.s16   d6, q3, #7
        vst1.8         d6, [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
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
		b 		  0b
4:      subs r4, #1
        pld [r2]
        vld1.8    {d19}, [r2], r3
        epel_filter_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.32      d6[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne       4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #4
        add       r10, #8
        mov       r8, r10
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
        b         0b
2:      subs r4, #1
        pld [r2]
        vld1.8    {d19}, [r2], r3
        epel_filter_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.16      d6[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_put_epel_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r6, [sp, #40] // mx
        ldr    r7, [sp, #44] // my
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs
        sub    r6, #1
        lsl    r6, #2
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        sub       r2, #1
        sub       r2, r3
        lsl       r1, #1
        load_coeffs_16b r6
        load_coeffs_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld   [r2]
        vextin_d4
        epel_filter_16b q12
        pld   [r2]
        vextin_d4
        epel_filter_16b q13
        pld   [r2]
        vextin_d4
        epel_filter_16b q14
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d4
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b
        vst1.16    {q3}, [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 8b
        subs    r5, #8
        beq  99f
        mov       r4, r12
        add       r6, #16
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        b         0b
4:      pld      [r2]
        vextin_d4_8
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b_4
        vst1.16    d6, [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
        b         0b
2:      pld      [r2]
        vextin_d4_8
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b_4
        vst1.32    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel_uni_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r6, [sp, #40] // mx
        ldr    r7, [sp, #44] // my
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs
        sub    r6, #1
        lsl    r6, #2
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        sub       r2, #1
        sub       r2, r3
        load_coeffs_16b r6
        load_coeffs_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld   [r2]
        vextin_d4
        epel_filter_16b q12
        pld   [r2]
        vextin_d4
        epel_filter_16b q13
        pld   [r2]
        vextin_d4
        epel_filter_16b q14
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d4
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b
        vqrshrun.s16   d6, q3, #6
        vst1.8    d6, [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 8b
        subs    r5, #8
        beq  99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        b         0b
4:      pld      [r2]
        vextin_d4_8
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b_4
        vqrshrun.s16   d6, q3, #6
        vst1.32    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #4
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
        b         0b
2:      pld      [r2]
        vextin_d4_8
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b_4
        vqrshrun.s16   d6, q3, #6
        vst1.16    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel_bi_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #44] // height
        ldr    r5, [sp, #56] // width
        ldr    r6, [sp, #48] // mx
        ldr    r7, [sp, #52] // my
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs
        sub    r6, #1
        lsl    r6, #2
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        sub       r2, #1
        sub       r2, r3
        load_coeffs_16b r6
        load_coeffs_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
        lsl   r9, #1
0:      pld   [r2]
        vextin_d4
        epel_filter_16b q12
        pld   [r2]
        vextin_d4
        epel_filter_16b q13
        pld   [r2]
        vextin_d4
        epel_filter_16b q14
        mov   r10, r8
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d4
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b
		vld1.16        {q4}, [r8], r9
        vqadd.s16      q4, q3
        vqrshrun.s16   d8, q4, #7
        vst1.8         d8, [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 8b
        subs    r5, #8
        beq  99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r10, #16
        mov       r8, r10
        add       r7, #8
        mov       r2, r7
        b         0b
4:      pld      [r2]
        vextin_d4_8
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b_4
		vld1.16        {q4}, [r8], r9
        vqadd.s16      q4, q3
        vqrshrun.s16   d8, q4, #7
        vst1.32        d8[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
        add       r6, #4
        mov       r0, r6
        add       r10, #8
        mov       r8, r10
        add       r7, #4
        mov       r2, r7
        b         0b
2:      pld      [r2]
        vextin_d4_8
        epel_filter_16b q15
        subs r4, #1
        epel_filter_32b_4
		vld1.16        {q4}, [r8], r9
        vqadd.s16      q4, q3
        vqrshrun.s16   d8, q4, #7
        vst1.16        d8[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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
        ldr    r10,[sp, #36] // height
        ldr    r8, [sp, #28] // src2
        ldr    r9, [sp, #32] // src2stride
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

epel_coeffs:
       .byte 2, 58, 10, 2
       .byte 4, 54, 16, 2
       .byte 6, 46, 28, 4
       .byte 4, 36, 36, 4
       .byte 4, 28, 46, 6
       .byte 2, 16, 54, 4
       .byte 2, 10, 58, 2
