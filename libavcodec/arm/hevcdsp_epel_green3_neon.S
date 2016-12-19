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

.macro epel_filter3_l_16b out=q12
    vmull.u8 q3, d16, d0
    vmull.u8 \out, d17, d1
    vmull.u8 q10, d18, d2
    vadd.s16 \out, q10
    vsub.s16 \out, q3
.endm

.macro epel_filter3_r_16b out=q12
    vmull.u8 q3, d16, d0
    vmull.u8 \out, d17, d1
    vmull.u8 q10, d18, d2
    vsub.s16 \out, q10
    vadd.s16 \out, q3
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


.macro epel_filter3_l_32b
    vmull.s16 q3, d24, d4[0] //q12
    vmull.s16 q4, d25, d4[0]
    vmull.s16 q7, d26, d4[1] // q13
    vmull.s16 q8, d27, d4[1]
    vmull.s16 q9, d28, d4[2] // q14
    vmull.s16 q10, d29, d4[2]
    vsub.s32 q7, q3
    vsub.s32 q8, q4
    vadd.s32 q7, q9
    vadd.s32 q8, q10
    vqshrn.s32  d6, q7, #6
    vqshrn.s32  d7, q8, #6
.endm

.macro epel_filter3_r_32b
    vmull.s16 q3, d24, d4[0] //q12
    vmull.s16 q4, d25, d4[0]
    vmull.s16 q7, d26, d4[1] // q13
    vmull.s16 q8, d27, d4[1]
    vmull.s16 q9, d28, d4[2] // q14
    vmull.s16 q10, d29, d4[2]
    vadd.s32 q7, q3
    vadd.s32 q8, q4
    vsub.s32 q7, q9
    vsub.s32 q8, q10
    vqshrn.s32  d6, q7, #6
    vqshrn.s32  d7, q8, #6
.endm


.macro epel_filter3_l_32b_4
    vmull.s16 q3, d24, d4[0] //q12
    vmull.s16 q7, d26, d4[1] // q13
    vmull.s16 q9, d28, d4[2] // q14
    vsub.s32 q7, q3
    vadd.s32 q7, q9
    vqshrn.s32  d6, q7, #6
.endm

/*
	r0 : dst
	r1 : dststride
	r2 : src
	r3 : srcstride
	r4 : height
	r5 : width
	r6 :
	r7 : mx
	r8 : -
	r9 :
	r10:
	r11:
	r12:
	r13:
	r14:
	coeffs in d0, d1, d2
*/

.macro ff_hevc_put_epel3_h_neon filter r2_sub
        push   {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r7, [sp, #24] // mx
        ldr    r5, [sp, #32] // width
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs3
        add    r7, r12
        sub       r2, \r2_sub
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
        \filter
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
        \filter
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
        \filter
        vst1.32    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r8}
        bx lr
.endm

function ff_hevc_put_epel3_h_l_neon_8, export=1
        ff_hevc_put_epel3_h_neon epel_filter3_l_16b 1
endfunc

function ff_hevc_put_epel3_h_r_neon_8, export=1
        ff_hevc_put_epel3_h_neon epel_filter3_r_16b 0
endfunc

.macro ff_hevc_put_epel3_v_neon filter r2_sub
        push   {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r7, [sp, #28] // my
        ldr    r5, [sp, #32] // width
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs3
        add    r7, r12
        load_coeffs3_16b r7
        sub       r2, \r2_sub
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
        \filter
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
        \filter
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
        \filter
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r8}
        bx lr
.endm

function ff_hevc_put_epel3_v_u_neon_8, export=1
        ff_hevc_put_epel3_v_neon epel_filter3_l_16b r3
endfunc

function ff_hevc_put_epel3_v_d_neon_8, export=1
        ff_hevc_put_epel3_v_neon epel_filter3_r_16b 0
endfunc

.macro ff_hevc_put_epel3_hv_neon filterh filterv r2_sub1 r2_sub2
        push   {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r6, [sp, #24] // mx
        ldr    r7, [sp, #28] // my
        ldr    r5, [sp, #32] // width
        sub    r7, #1
        lsl    r7, #2
        vpush {d8-d15}
        adrl    r12, epel_coeffs3
        sub    r6, #1
        lsl    r6, #2
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        sub       r2, \r2_sub1
        sub       r2, \r2_sub2
        lsl       r1, #1
        load_coeffs3_16b r6
        load_coeffs3_32b r7
        mov   r12, r4
        mov   r6, r0
        mov   r7, r2
0:      pld   [r2]
        vextin_d3
        \filterh q12
        pld   [r2]
        vextin_d3
        \filterh q13
        pld   [r2]
        vextin_d3
        \filterh q14
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d3
        \filterh q15
        subs r4, #1
        \filterv
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
        \filterh q15
        subs r4, #1
        \filterv
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
        \filterh q15
        subs r4, #1
        \filterv
        vst1.32    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r8}
        bx lr
.endm

function ff_hevc_put_epel3_hv_lu_neon_8, export=1
        ff_hevc_put_epel3_hv_neon epel_filter3_l_16b epel_filter3_l_32b 1 r3
endfunc

function ff_hevc_put_epel3_hv_ru_neon_8, export=1
        ff_hevc_put_epel3_hv_neon epel_filter3_r_16b epel_filter3_l_32b 0 r3
endfunc

function ff_hevc_put_epel3_hv_ld_neon_8, export=1
        ff_hevc_put_epel3_hv_neon epel_filter3_l_16b epel_filter3_r_32b 1 0
endfunc

function ff_hevc_put_epel3_hv_rd_neon_8, export=1
        ff_hevc_put_epel3_hv_neon epel_filter3_r_16b epel_filter3_r_32b 0 0
endfunc

epel_coeffs3: // Added 0's for padding
       .byte  3, 61,  6, 0
       .byte  5, 56, 13, 0
       .byte  7, 50, 21, 0
       .byte  7, 41, 30, 0
       .byte 21, 50,  7, 0
       .byte 13, 56,  5, 0
       .byte  6, 61,  3, 0
