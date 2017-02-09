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

.macro vextin_d3
    vld1.8    {q10}, [r2], r3
    vmov      d16, d20
    vext.8    d17, d20, d21, #1
    vext.8    d18, d20, d21, #2
.endm

.macro vextin_d3_8
    vld1.8    d16, [r2], r3
    vext.8    d17, d16, d16, #1
    vext.8    d18, d16, d16, #2
.endm

.macro load_coeffs3_16b coeffs
    ldr      \coeffs, [\coeffs]
    vdup.i8  d0, \coeffs
    lsr      \coeffs, #8
    vdup.i8  d1, \coeffs
    lsr      \coeffs, #8
    vdup.i8  d2, \coeffs
.endm

.macro load_coeffs3_32b coeffs
    ldr      \coeffs, [\coeffs]
    vmov.i64 d4, #0
    vmov.8   d4[0], \coeffs
    lsr      \coeffs, #8
    vmov.8   d4[2], \coeffs
    lsr      \coeffs, #8
    vmov.8   d4[4], \coeffs
.endm


// Signess correction stored in q15
.macro filter_16b out=q12 sig=q15
        vmull.u8 \out, d16, d0
		vmlsl.u8 \out, d18, d2
		vmul.i16 \out, \sig
        vmlal.u8 \out, d17, d1
.endm

// Signess correction stored in q15
.macro filter_32b sig
    	vmull.s16 q7, d24, d4[0]
    	vmull.s16 q8, d25, d4[0]
    	vmlsl.s16 q7, d28, d4[2]
    	vmlsl.s16 q8, d29, d4[2]
    	vmul.i32  q7, \sig
    	vmul.i32  q8, \sig
    	vmlal.s16 q7, d26, d4[1]
    	vmlal.s16 q8, d27, d4[1]
	    vqshrn.s32  d6, q7, #6
	    vqshrn.s32  d7, q8, #6
.endm


function ff_hevc_put_epel3_h_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r7, [sp, #40] // mx
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsrs   r8, r7, #2
		bne h1
		sub    r2, #1
        vmov.i16 q15, #0xFFFF
        b h2
h1:     vmov.i16 q15, #1
h2:		lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs3
        add    r7, r12
        lsl       r1, #1
        load_coeffs3_16b r7
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
        vextin_d3
        filter_16b
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
        vextin_d3_8
        filter_16b
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
        vextin_d3_8
        filter_16b
        vst1.32    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel3_uni_h_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r7, [sp, #40] // mx
        sub    r7, #1
        lsrs   r8, r7, #2
		bne hu1
		sub    r2, #1
        vmov.i16 q5, #0xFFFF
        b hu2
hu1:    vmov.i16 q5, #1
hu2:	lsl    r7, #2
        vpush {d8-d15}
        adrl   r12, epel_coeffs3
        add    r7, r12
        load_coeffs3_16b r7
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
        vextin_d3
        filter_16b q12 q5
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
        vextin_d3_8
        filter_16b q12 q5
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
        vextin_d3_8
        filter_16b q12 q5
        vqrshrun.s16   d24, q12, #6
        vst1.16    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel3_bi_h_neon_8, export=1
        push   {r4-r12}
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        ldr    r4, [sp, #44] // height
        ldr    r7, [sp, #48] // mx
        ldr    r5, [sp, #56] // width
        sub    r7, #1
        lsrs   r10, r7, #2
		bne hb1
		sub    r2, #1
        vmov.i16 q5, #0xFFFF
        b hb2
hb1:    vmov.i16 q5, #1
hb2:	lsl    r7, #2
        vpush  {d8-d15}
        adrl   r12, epel_coeffs3
        add    r7, r12
        load_coeffs3_16b r7
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
        vextin_d3
        filter_16b q12 q5
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
        vextin_d3_8
        filter_16b q12 q5
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
        vextin_d3_8
        filter_16b q12 q5
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.32      d6[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_put_epel3_v_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r7, [sp, #44] // my
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsrs   r8, r7, #2
		bne v1
		sub  r2, r3
        vmov.i16 q15, #0xFFFF
        b v2
v1:     vmov.i16 q15, #1
v2:		lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs3
        add    r7, r12
        load_coeffs3_16b r7
        lsl       r1, #1
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld [r2]
        vld1.8    {d16}, [r2], r3
        pld [r2]
        vld1.8    {d17}, [r2], r3
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld [r2]
        vld1.8    {d18}, [r2], r3
        subs r4, #1
        filter_16b
        vst1.16    {q12}, [r0], r1
        vmov d16, d17
        vmov d17, d18
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
        vld1.8    {d18}, [r2], r3
        subs r4, #1
        filter_16b
        vst1.16    d24, [r0], r1
        vmov d16, d17
        vmov d17, d18
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
        vld1.8    {d18}, [r2], r3
        subs r4, #1
        filter_16b
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel3_uni_v_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r7, [sp, #44] // my
        sub    r7, #1
        lsrs   r8, r7, #2
		bne vu1
		sub  r2, r3
        vmov.i16 q15, #0xFFFF
        b vu2
vu1:    vmov.i16 q15, #1
vu2:	lsl    r7, #2
        vpush {d8-d15}
        adrl   r12, epel_coeffs3
        add    r7, r12
        load_coeffs3_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld [r2]
        vld1.8    {d16}, [r2], r3
        pld [r2]
        vld1.8    {d17}, [r2], r3
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld [r2]
        vld1.8    {d18}, [r2], r3
        subs r4, #1
        filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.8    d24, [r0], r1
        vmov d16, d17
        vmov d17, d18
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
        vld1.8    {d18}, [r2], r3
        subs r4, #1
        filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
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
        vld1.8    {d18}, [r2], r3
        subs r4, #1
        filter_16b
        vqrshrun.s16   d24, q12, #6
        vst1.16    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel3_bi_v_neon_8, export=1
        push   {r4-r12}
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        ldr    r4, [sp, #44] // height
        ldr    r7, [sp, #52] // my
        ldr    r5, [sp, #56] // width
        sub    r7, #1
        lsrs   r10, r7, #2
		bne vb1
		sub  r2, r3
        vmov.i16 q15, #0xFFFF
        b vb2
vb1:    vmov.i16 q15, #1
vb2:	lsl    r7, #2
        vpush  {d8-d15}
        adrl   r12, epel_coeffs3
        add    r7, r12
		lsl    r9, #1
        load_coeffs3_16b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:		pld [r2]
        vld1.8    {d16}, [r2], r3
        pld [r2]
        vld1.8    {d17}, [r2], r3
        mov   r10, r8
        cmp   r5, #6
        bgt       8f
        cmp   r5, #4
        blt   2f
        b     4f
8:      subs r4, #1
        pld [r2]
        vld1.8    {d18}, [r2], r3
        filter_16b
        vld1.16        {q3}, [r8], r9
        vqadd.s16      q3, q12
        vqrshrun.s16   d6, q3, #7
        vst1.8         d6, [r0], r1
        vmov d16, d17
        vmov d17, d18
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
        vld1.8    {d18}, [r2], r3
        filter_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.32      d6[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
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
        vld1.8    {d18}, [r2], r3
        filter_16b
        vld1.16      d6, [r8], r9
        vqadd.s16    d6, d24
        vqrshrun.s16 d6, q3, #7
        vst1.32      d6[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_put_epel3_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r6, [sp, #40] // mx
        ldr    r7, [sp, #44] // my
        ldr    r5, [sp, #48] // width
        sub    r7, #1
        lsrs   r8, r7, #2
		bne hv0
		sub  r2, r3
        vmov.i32 q5, #0xFFFFFFFF
        b hv1
hv0:    vmov.i32 q5, #1
hv1:    lsl    r7, #2
        vpush {d8-d15}
        adrl   r12, epel_coeffs3
        sub    r6, #1
        lsrs   r8, r6, #2
        bne hv2
        sub  r2, 1
        vmov.i16 q6, #0xFFFF
		b hv3
hv2:    vmov.i16 q6, #1
hv3:    lsl    r6, #2
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        lsl       r1, #1
        load_coeffs3_16b r6
        load_coeffs3_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld   [r2]
        vextin_d3
        filter_16b q12 q6
        pld   [r2]
        vextin_d3
        filter_16b q13 q6
        pld   [r2]
        vextin_d3
        filter_16b q14 q6
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d3
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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
        vextin_d3_8
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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
        vextin_d3_8
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
        vst1.32    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
endfunc

function ff_hevc_epel3_uni_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
        ldr    r6, [sp, #40] // mx
        ldr    r7, [sp, #44] // my
        sub    r7, #1
        lsrs   r8, r7, #2
		bne hvu0
		sub  r2, r3
        vmov.i32 q5, #0xFFFFFFFF
        b hvu1
hvu0:   vmov.i32 q5, #1
hvu1:   lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs3
        sub    r6, #1
        lsrs   r8, r6, #2
        bne hvu2
        sub  r2, 1
        vmov.i16 q6, #0xFFFF
		b hvu3
hvu2:   vmov.i16 q6, #1
hvu3:   lsl    r6, #2
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        load_coeffs3_16b r6
        load_coeffs3_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld   [r2]
        vextin_d3
        filter_16b q12 q6
        pld   [r2]
        vextin_d3
        filter_16b q13 q6
        pld   [r2]
        vextin_d3
        filter_16b q14 q6
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d3
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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
        vextin_d3_8
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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
        vextin_d3_8
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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

function ff_hevc_epel3_bi_hv_neon_8, export=1
        push   {r4-r12}
        ldr    r4, [sp, #44] // height
        ldr    r5, [sp, #56] // width
        ldr    r6, [sp, #48] // mx
        ldr    r7, [sp, #52] // my
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        sub    r7, #1
        lsrs   r10, r7, #2
		bne hvb0
		sub  r2, r3
        vmov.i32 q5, #0xFFFFFFFF
        b hvb1
hvb0:   vmov.i32 q5, #1
hvb1:   lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs3
        sub    r6, #1
        lsrs   r10, r6, #2
        bne hvb2
        sub  r2, #1
        vmov.i16 q6, #0xFFFF
		b hvb3
hvb2:   vmov.i16 q6, #1
hvb3:   lsl    r6, #2
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        load_coeffs3_16b r6
        load_coeffs3_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
        lsl   r9, #1
0:      pld   [r2]
        vextin_d3
        filter_16b q12 q6
        pld   [r2]
        vextin_d3
        filter_16b q13 q6
        pld   [r2]
        vextin_d3
        filter_16b q14 q6
        mov   r10, r8
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d3
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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
        vextin_d3_8
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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
        vextin_d3_8
        filter_16b q15 q6
        subs r4, #1
        filter_32b q5
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

epel_coeffs3: // Added 0's for padding
       .byte  3, 61,  6, 0
       .byte  5, 56, 13, 0
       .byte  7, 50, 21, 0
       .byte  7, 41, 30, 0
       .byte 21, 50,  7, 0
       .byte 13, 56,  5, 0
       .byte  6, 61,  3, 0
