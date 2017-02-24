/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
 *               2015 Morgan LACOUR <morgan.lacour@insa-rennes.fr>
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

.macro vextin_d2
    vld1.8    {q10}, [r2], r3
    vmov      d16, d20
    vext.8    d17, d20, d21, #1
.endm

.macro vextin_d2_8
    vld1.8    d16, [r2], r3
    vext.8    d17, d16, d16, #1
.endm


.macro load_coeffs2_16b coeffs
    ldr      \coeffs, [\coeffs]
    vdup.i8  d0, \coeffs
    lsr      \coeffs, #8
    vdup.i8  d1, \coeffs
.endm

.macro epel_filter2_16b out=q12
    vmull.u8 \out, d16, d0
    vmlal.u8 \out, d17, d1
.endm

.macro load_coeffs2_32b coeffs
    ldr      \coeffs, [\coeffs]
    vmov.i64 d4, #0
    vmov.8   d4[0], \coeffs
    lsr      \coeffs, #8
    vmov.8   d4[2], \coeffs
.endm

.macro epel_filter2_32b
    vmull.s16 q3, d24, d4[0] //q12
    vmull.s16 q4, d25, d4[0]
    vmlal.s16 q3, d26, d4[1] // q13
    vmlal.s16 q4, d27, d4[1]
    vqshrn.s32  d6, q3, #6
    vqshrn.s32  d7, q4, #6
.endm

.macro epel_filter2_32b_4
    vmull.s16 q3, d24, d4[0] //q12
    vmlal.s16 q3, d26, d4[1] // q13
    vqshrn.s32  d6, q3, #6
.endm

function ff_hevc_put_epel2_h_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r7, [sp, #40] // mx
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adrl    r12, epel_coeffs2
        add    r7, r12
        lsl       r1, #1
        load_coeffs2_16b r7
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
        vextin_d2
        epel_filter2_16b
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
        vextin_d2_8
        epel_filter2_16b
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
        vextin_d2_8
        epel_filter2_16b
        vst1.32    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel2_uni_h_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r7, [sp, #40] // mx
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adrl   r12, epel_coeffs2
        add    r7, r12
        load_coeffs2_16b r7
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
        vextin_d2
        epel_filter2_16b
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
        vextin_d2_8
        epel_filter2_16b
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
        vextin_d2_8
        epel_filter2_16b
        vqrshrun.s16   d24, q12, #6
        vst1.16    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel2_bi_h_neon_8, export=1
        push   {r4-r12}
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        ldr    r4, [sp, #44] // height
        ldr    r7, [sp, #48] // mx
        ldr    r5, [sp, #56] // width
        sub    r7, #1
        lsl    r7, #1
        vpush  {d8-d15}
        adrl   r12, epel_coeffs2
        add    r7, r12
        load_coeffs2_16b r7
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
        vextin_d2
        epel_filter2_16b
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
        vextin_d2_8
        epel_filter2_16b
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
        vextin_d2_8
        epel_filter2_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.16      d6[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_put_epel2_v_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r7, [sp, #44] // my
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adrl    r12, epel_coeffs2
        add    r7, r12
        load_coeffs2_16b r7
        lsl       r1, #1
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld [r2]
        vld1.8    {d16}, [r2], r3
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld [r2]
        vld1.8    {d17}, [r2], r3
        subs r4, #1
        epel_filter2_16b
        vst1.16    {q12}, [r0], r1
        vmov d16, d17
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
        vld1.8    {d17}, [r2], r3
        subs r4, #1
        epel_filter2_16b
        vst1.16    d24, [r0], r1
        vmov d16, d17
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
        vld1.8    {d17}, [r2], r3
        subs r4, #1
        epel_filter2_16b
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel2_uni_v_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r7, [sp, #44] // my
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adrl   r12, epel_coeffs2
        add    r7, r12
        //sub       r2, r3
        load_coeffs2_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld [r2]
        vld1.8    {d16}, [r2], r3
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld [r2]
        vld1.8    {d17}, [r2], r3
        subs r4, #1
        epel_filter2_16b
        vqrshrun.s16   d24, q12, #6
        vst1.8    d24, [r0], r1
        vmov d16, d17
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
        vld1.8    {d17}, [r2], r3
        subs r4, #1
        epel_filter2_16b
        vqrshrun.s16   d24, q12, #6
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
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
        vld1.8    {d17}, [r2], r3
        subs r4, #1
        epel_filter2_16b
        vqrshrun.s16   d24, q12, #6
        vst1.16    d24[0], [r0], r1
        vmov d16, d17
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel2_bi_v_neon_8, export=1
        push   {r4-r12}
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        ldr    r4, [sp, #44] // height
        ldr    r7, [sp, #52] // my
        ldr    r5, [sp, #56] // width
        sub    r7, #1
        lsl    r7, #1
        vpush  {d8-d15}
        adrl   r12, epel_coeffs2
        add    r7, r12
		lsl    r9, #1
        //sub    r2, r3
        load_coeffs2_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:		pld [r2]
        vld1.8    {d16}, [r2], r3
        mov   r10, r8
        cmp   r5, #6
        bgt       8f
        cmp   r5, #4
        blt   2f
        b     4f
8:      subs r4, #1
        pld [r2]
        vld1.8    {d17}, [r2], r3
        epel_filter2_16b
        vld1.16        {q3}, [r8], r9
        vqadd.s16      q3, q12
        vqrshrun.s16   d6, q3, #7
        vst1.8         d6, [r0], r1
        vmov d16, d17
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
        vld1.8    {d17}, [r2], r3
        epel_filter2_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.32      d6[0], [r0], r1
        vmov d16, d17
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
        vld1.8    {d17}, [r2], r3
        epel_filter2_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.16      d6[0], [r0], r1
        vmov d16, d17
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_put_epel2_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r6, [sp, #40] // mx
        ldr    r7, [sp, #44] // my
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adrl    r12, epel_coeffs2
        sub    r6, #1
        lsl    r6, #1
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        lsl       r1, #1
        load_coeffs2_16b r6
        load_coeffs2_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld   [r2]
        vextin_d2
        epel_filter2_16b q12
        pld   [r2]
        vextin_d2
        epel_filter2_16b q13
        pld   [r2]
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d2
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b
        vst1.16    {q3}, [r0], r1
        vmov q12, q13
        vmov q13, q14
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
        vextin_d2_8
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b_4
        vst1.16    d6, [r0], r1
        vmov q12, q13
        vmov q13, q14
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
        vextin_d2_8
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b_4
        vst1.32    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel2_uni_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r6, [sp, #40] // mx
        ldr    r7, [sp, #44] // my
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adrl    r12, epel_coeffs2
        sub    r6, #1
        lsl    r6, #1
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        load_coeffs2_16b r6
        load_coeffs2_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld   [r2]
        vextin_d2
        epel_filter2_16b q12
        pld   [r2]
        vextin_d2
        epel_filter2_16b q13
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d2
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b
        vqrshrun.s16   d6, q3, #6
        vst1.8    d6, [r0], r1
        vmov q12, q13
        vmov q13, q14
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
        vextin_d2_8
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b_4
        vqrshrun.s16   d6, q3, #6
        vst1.32    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
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
        vextin_d2_8
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b_4
        vqrshrun.s16   d6, q3, #6
        vst1.16    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel2_bi_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #44] // height
        ldr    r5, [sp, #56] // width
        ldr    r6, [sp, #48] // mx
        ldr    r7, [sp, #52] // my
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adrl    r12, epel_coeffs2
        sub    r6, #1
        lsl    r6, #1
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        load_coeffs2_16b r6
        load_coeffs2_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
        lsl   r9, #1
0:      pld   [r2]
        vextin_d2
        epel_filter2_16b q12
        pld   [r2]
        vextin_d2
        epel_filter2_16b q13
        mov   r10, r8
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d2
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b
		vld1.16        {q4}, [r8], r9
        vqadd.s16      q4, q3
        vqrshrun.s16   d8, q4, #7
        vst1.8         d8, [r0], r1
        vmov q12, q13
        vmov q13, q14
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
        vextin_d2_8
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b_4
		vld1.16        {q4}, [r8], r9
        vqadd.s16      q4, q3
        vqrshrun.s16   d8, q4, #7
        vst1.32        d8[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
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
        vextin_d2_8
        epel_filter2_16b q14
        subs r4, #1
        epel_filter2_32b_4
		vld1.16        {q4}, [r8], r9
        vqadd.s16      q4, q3
        vqrshrun.s16   d8, q4, #7
        vst1.16        d8[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

epel_coeffs2:
       .byte 57,  7
       .byte 49, 15
       .byte 41, 23
       .byte 32, 32
       .byte 23, 41
       .byte 15, 49
       .byte  7, 57
