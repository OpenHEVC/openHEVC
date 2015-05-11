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
    vmull.u8 q3, d16, d0
    vmull.u8 \out, d17, d1
    vadd.s16 \out, q3
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
    vmull.s16 q7, d26, d4[1] // q13
    vmull.s16 q8, d27, d4[1]
    vadd.s32 q3, q7
    vadd.s32 q4, q8
    vqshrn.s32  d6, q3, #6
    vqshrn.s32  d7, q4, #6
.endm


.macro epel_filter2_32b_4
    vmull.s16 q3, d24, d4[0] //q12
    vmull.s16 q7, d26, d4[1] // q13
    vadd.s32 q7, q3
    vqshrn.s32  d6, q7, #6
.endm



function ff_hevc_put_epel2_h_neon_8, export=1
        push   {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r7, [sp, #24] // mx
        ldr    r5, [sp, #32] // width
        sub    r7, #1       // mx=mx-1
        lsl    r7, #1       // mx * 2 (2 coeffs byte)
        vpush {d8-d15}
        adr    r12, epel_coeffs2
        add    r7, r12
        sub       r2, #1
        lsl       r1, #1    // Seule ligne a changer pour ECO
        load_coeffs2_16b r7  //d0 et d1 coeffs
        mov   r12, r4       // _h
        mov   r6, r0        // _dst
        mov   r7, r2        // _src
        cmp       r5, #6 
        bgt       8f        // b8f si width>6
        cmp       r5, #4
        blt       2f        //b2f si width<4
        b         4f        //sinon b4f
8:      subs r4, #1         // height--
        pld [r2]            //charge le registre d entree
        vextin_d2           // d16 d17 vect in (a b)
        epel_filter2_16b     // q12 = d0*d16 + d1*d17
        vst1.16    {q12}, [r0], r1  //*dst=q12, dst+=dststride
        bne 8b              // si height!=0 boucle
        subs    r5, #8      // width-=8
        beq  99f            //si width==0 fin
        mov       r4, r12   // reload _height
        add       r6, #16   // _dst+=16 (8*2octets d'offset)
        mov       r0, r6    // dst=_dst
        add       r7, #8    // _src += 8 (8*1octet offset)
        mov       r2, r7    // src = _src
        cmp       r5, #4    // r5>4 ?
        bgt       8b        // br 8b si width>4
4:      subs r4, #1         // height--
        pld [r2]
        vextin_d2_8         // d16 d17 vect in (a b)
        epel_filter2_16b     // q12 = d0*d16 + d1*d17
        vst1.16    d24, [r0], r1    // *dst=d24, dst+=dststride
        bne 4b              // si height!=0 boucle
        subs      r5, #4    // width-=4
        beq       99f       // si width==0 fin
        mov       r4, r12   // reload height
        add       r6, #8    // _dst += 8
        mov       r0, r6    // dst = _dst
        add       r7, #4    // _src += 4
        mov       r2, r7    // src = _src
2:      subs r4, #1         // height--
        pld [r2]
        vextin_d2_8
        epel_filter2_16b
        vst1.32    d24[0], [r0], r1
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r8}
        bx lr
endfunc

function ff_hevc_put_epel2_v_neon_8, export=1
        push   {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r7, [sp, #28] // my
        ldr    r5, [sp, #32] // width
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adr    r12, epel_coeffs2
        add    r7, r12
        load_coeffs2_16b r7
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
        epel_filter2_16b
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
        epel_filter2_16b
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
        epel_filter2_16b
        vst1.32    d24[0], [r0], r1
        vmov d16, d17
        vmov d17, d18
        vmov d18, d19
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r8}
        bx lr
endfunc

function ff_hevc_put_epel2_hv_neon_8, export=1
        push   {r4-r8}
        ldr    r4, [sp, #20] // height
        ldr    r6, [sp, #24] // mx
        ldr    r7, [sp, #28] // my
        ldr    r5, [sp, #32] // width
        sub    r7, #1
        lsl    r7, #1
        vpush {d8-d15}
        adr    r12, epel_coeffs2
        sub    r6, #1
        lsl    r6, #1
        add    r6, r12 // mx epel coeff offset
        add    r7, r12
        sub       r2, #1
        sub       r2, r3
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
        vextin_d2
        epel_filter2_16b q14
        cmp       r5, #6
        bgt       8f
        cmp       r5, #4
        blt       2f
        b         4f
8:      pld     [r2]
        vextin_d2
        epel_filter2_16b q15
        subs r4, #1
        epel_filter2_32b
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
        vextin_d2_8
        epel_filter2_16b q15
        subs r4, #1
        epel_filter2_32b_4
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
        vextin_d2_8
        epel_filter2_16b q15
        subs r4, #1
        epel_filter2_32b_4
        vst1.32    d6[0], [r0], r1
        vmov q12, q13
        vmov q13, q14
        vmov q14, q15
        bne 2b
99:     vpop {d8-d15}
        pop {r4-r8}
        bx lr
endfunc

epel_coeffs2:
       .byte 54, 10
       .byte 54, 10
       .byte 54, 10
       .byte 54, 10
       .byte 54, 10
       .byte 54, 10
       .byte 54, 10
