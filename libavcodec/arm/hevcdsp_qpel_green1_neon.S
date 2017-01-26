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

.macro regshuffle_d2
    vmov d16, d17
    vmov d17, d18
.endm

.macro regshuffle_q2
    vmov q0, q1
    vmov q1, q2
.endm

.macro vextin2
        pld       [r2]
        vld1.8    {q11}, [r2], r3
        vext.8    d16, d22, d23, #0
        vext.8    d17, d22, d23, #1
.endm

.macro loadin2
        pld       [r2]
        vld1.8    {d16}, [r2], r3
        pld       [r2]
        vld1.8    {d17}, [r2], r3
.endm



.macro qpel_filter1_1_32b
	vmov.u8	d16, d0
	vmov.u8 d17, d1		
.endm

.macro qpel_filter1_2_32b
	vmov.u8	d16, d0
	vmov.u8 d17, d1		
.endm

.macro qpel_filter1_3_32b
	vmov.u8	d16, d2
	vmov.u8 d17, d3		
.endm

.macro qpel_filter1_1 out=q7
	vshll.u8  \out, d16, #6     // 64*a
.endm

.macro qpel_filter1_2 out=q7
	vshll.u8  \out, d16, #6     // 64*a
.endm

.macro qpel_filter1_3 out=q7
	vshll.u8  \out, d17, #6     // 64*a
.endm


.macro  hevc_put_qpel1_vX_neon_8 filter
        push   {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width
        vpush {d8-d15}
        mov       r12, r4 		//_height
        mov       r6, r0 		// _dst
        mov       r7, r2 		// _src
        lsl       r1, #1 		// dststride*2
0:      loadin2 			//d16 d17 d18 (a b c)
        cmp       r5, #4 	// width==4?
        beq       4f 		// br 4f si width=4
8:      subs r4, #1 		// height--
        \filter 			// q7 = vect filtre
        vst1.16    {q7}, [r0], r1 	// *dst=q7, dst+=dststride
        regshuffle_d2 		// a=b, b=c
        vld1.8    {d17}, [r2], r3 // charge d17, src+=srcstride //MODIF Green
        bne 8b 				// height!=0 boucle
        subs  r5, #8 		// width-=8
        beq       99f 		// si width==0 fin
        mov r4, r12 		// reload height
        add r6, #16 		// _dst += 16 (8*2octets)
        mov r0, r6 			// dst = _dst
        add r7, #8 			// _src += 8
        mov r2, r7 			// src = _src
        b     0b 			// br 0b
4:      subs r4, #1 		// height--
        \filter 			//q7 = vect filtre
        vst1.16    d14, [r0], r1 	// *dst=d14, dst+=dststride
        regshuffle_d2		//veillissement
        vld1.32    {d17[0]}, [r2], r3 	// MODIF Green
        bne 4b 
99:     vpop {d8-d15}
        pop {r4, r5, r6, r7}
        bx lr
.endm

.macro  hevc_put_qpel1_uw_vX_neon_8 filter
        push   {r4-r10}
        ldr    r5, [sp, #28] // width
        ldr    r4, [sp, #32] // height
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        vpush {d8-d15}
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r8, #0
        bne       .Lbi\@
0:      loadin2
        cmp       r5, #4
        beq       4f
8:      subs r4, #1
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.8    d0, [r0], r1
        regshuffle_d2
        vld1.8    {d17}, [r2], r3 	// MODIF Green
        bne 8b
        subs  r5, #8
        beq       99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r7, #8
        mov r2, r7
        b     0b
4:      subs r4, #1
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.32    d0[0], [r0], r1
        regshuffle_d2
        vld1.32    {d17[0]}, [r2], r3 	//MODIF Green
        bne 4b
        b   99f
.Lbi\@: lsl       r9, #1
        mov       r10, r8 	// _src2
0:      loadin2
        cmp       r5, #4
        beq       4f
8:      subs r4, #1
        \filter
        vld1.16        {q0}, [r8], r9
        vqadd.s16      q0, q7
        vqrshrun.s16   d0, q0, #7
        vst1.8         d0, [r0], r1
        regshuffle_d2
        vld1.8    {d17}, [r2], r3	//MODIF Green
        bne 8b
        subs  r5, #8
        beq       99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r10, #16
        mov r8, r10
        add r7, #8
        mov r2, r7
        b     0b
4:      subs r4, #1
        \filter
        vld1.16      d0, [r8], r9
        vqadd.s16    d0, d14
        vqrshrun.s16 d0, q0, #7
        vst1.32      d0[0], [r0], r1
        regshuffle_d2
        vld1.32    {d17[0]}, [r2], r3	//MODIF Green
        bne 4b
99:     vpop {d8-d15}
        pop {r4-r10}
        bx lr
.endm


function ff_hevc_put_qpel1_v1_neon_8, export=1
        hevc_put_qpel1_vX_neon_8 qpel_filter1_1
endfunc

function ff_hevc_put_qpel1_v2_neon_8, export=1
        hevc_put_qpel1_vX_neon_8 qpel_filter1_2
endfunc

function ff_hevc_put_qpel1_v3_neon_8, export=1
        hevc_put_qpel1_vX_neon_8 qpel_filter1_3
endfunc


function ff_hevc_put_qpel1_uw_v1_neon_8, export=1
        hevc_put_qpel1_uw_vX_neon_8 qpel_filter1_1
endfunc

function ff_hevc_put_qpel1_uw_v2_neon_8, export=1
        hevc_put_qpel1_uw_vX_neon_8 qpel_filter1_2
endfunc

function ff_hevc_put_qpel1_uw_v3_neon_8, export=1
        hevc_put_qpel1_uw_vX_neon_8 qpel_filter1_3
endfunc




.macro hevc_put_qpel1_hX_neon_8 filter
        push     {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width
        vpush    {d8-d15}
        lsl       r1, #1
        mov      r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r5, #4
        beq       4f
8:      subs      r4, #1
        vextin2
        \filter
        vst1.16   {q7}, [r0], r1
        bne       8b
        subs      r5, #8
        beq      99f
        mov       r4, r12
        add       r6, #16
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        cmp       r5, #4
        bne       8b
4:      subs      r4, #1
        vextin2
        \filter
        vst1.16  d14, [r0], r1
        bne       4b
99:     vpop     {d8-d15}
        pop      {r4, r5, r6, r7}
        bx lr
.endm

/*(uint8_t *dst,  ptrdiff_t dststride,     \
                                       uint8_t *_src, ptrdiff_t _srcstride,    \
                                       int width, int height,                  \
                                       int16_t* src2, ptrdiff_t src2stride)*/


.macro hevc_put_qpel1_uw_hX_neon_8 filter
        push     {r4-r10}
        ldr       r5, [sp, #28] // width
        ldr       r4, [sp, #32] // height
        ldr       r8, [sp, #36] // src2
        ldr       r9, [sp, #40] // src2stride
        vpush    {d8-d15}
        mov      r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r8, #0
        bne       .Lbi\@
        cmp       r5, #4
        beq       4f
8:      subs      r4, #1
        vextin2
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.8    d0, [r0], r1
        bne       8b
        subs      r5, #8
        beq      99f
        mov       r4, r12
        add       r6, #8
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
        cmp       r5, #4
        bne       8b
4:      subs      r4, #1
        vextin2
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.32  d0[0], [r0], r1
        bne       4b
        b         99f
.Lbi\@:
        lsl       r9, #1
        cmp       r5, #4
        beq       4f
        mov       r10, r8
8:      subs      r4, #1
        vextin2
        \filter
        vld1.16        {q0}, [r8], r9
        vqadd.s16      q0, q7
        vqrshrun.s16   d0, q0, #7
        vst1.8         d0, [r0], r1
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
        bne       8b
4:      subs      r4, #1
        vextin2
        \filter
        vld1.16      d0, [r8], r9
        vqadd.s16    d0, d14
        vqrshrun.s16 d0, q0, #7
        vst1.32      d0[0], [r0], r1
        bne       4b
99:     vpop     {d8-d15}
        pop      {r4-r10}
        bx lr
.endm


function ff_hevc_put_qpel1_h1_neon_8, export=1
        hevc_put_qpel1_hX_neon_8 qpel_filter1_1
endfunc

function ff_hevc_put_qpel1_h2_neon_8, export=1
        hevc_put_qpel1_hX_neon_8 qpel_filter1_2
endfunc

function ff_hevc_put_qpel1_h3_neon_8, export=1
        hevc_put_qpel1_hX_neon_8 qpel_filter1_3
endfunc


function ff_hevc_put_qpel1_uw_h1_neon_8, export=1
        hevc_put_qpel1_uw_hX_neon_8 qpel_filter1_1
endfunc

function ff_hevc_put_qpel1_uw_h2_neon_8, export=1
        hevc_put_qpel1_uw_hX_neon_8 qpel_filter1_2
endfunc

function ff_hevc_put_qpel1_uw_h3_neon_8, export=1
        hevc_put_qpel1_uw_hX_neon_8 qpel_filter1_3
endfunc

//hevc.c:41:const uint8_t ff_hevc_qpel_extra_before[4] = { 0, 3, 3, 2 };
//hevc.c:42:const uint8_t ff_hevc_qpel_extra_after[4]  = { 0, 3, 4, 4 };
//hevc.c:43:const uint8_t ff_hevc_qpel_extra[4]        = { 0, 6, 7, 6 };

.macro hevc_put_qpel1_hXvY_neon_8 filterh filterv
        push   {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width

        vpush {d8-d15}
        lsl       r1, #1
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
0:      vextin2
        \filterh q0
        vextin2
        \filterh q1

        cmp r5, #4
        beq 4f
8:      subs  r4, #1
        \filterv
        vst1.16    {q8}, [r0], r1
        regshuffle_q2
        vextin2
        \filterh q1 	//MODIF Green
        bne 8b
        subs  r5, #8
        beq 99f
        mov r4, r12
        add r6, #16
        mov r0, r6
        add r7, #8
        mov r2, r7
        b 0b
4:      subs  r4, #1
        \filterv
        vst1.16    d16, [r0], r1
        regshuffle_q2
        vextin2
        \filterh q1 	// MODIF Green
        bne 4b
99:     vpop {d8-d15}
        pop {r4, r5, r6, r7}
        bx lr
.endm

.macro hevc_put_qpel1_uw_hXvY_neon_8 filterh filterv
        push     {r4-r10}
        ldr       r5, [sp, #28] // width
        ldr       r4, [sp, #32] // height
        ldr       r8, [sp, #36] // src2
        ldr       r9, [sp, #40] // src2stride
        vpush {d8-d15}
        mov       r12, r4 		// _height
        mov       r6, r0 		// _dst
        mov       r7, r2 		// _src
        cmp       r8, #0 		// src2?
        bne       .Lbi\@ 		//src2 branch
0:      vextin2
        \filterh q0
        vextin2
        \filterh q1

        cmp r5, #4
        beq 4f
8:      subs  r4, #1
        \filterv
        vqrshrun.s16   d0, q8, #6
        vst1.8    d0, [r0], r1
        regshuffle_q2
        vextin2
        \filterh q1	//MODIF Green
        bne 8b
        subs  r5, #8
        beq 99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r7, #8
        mov r2, r7
        b 0b
4:      subs  r4, #1 	// height--
        \filterv
        vqrshrun.s16   d0, q8, #6
        vst1.32        d0[0], [r0], r1
        regshuffle_q2
        vextin2
        \filterh q1 	//MODIF Green
        bne 4b
        b   99f
.Lbi\@: lsl      r9, #1 	// BIDIR PROCESS
        mov      r10, r8 	//_src2
0:      vextin2 		// load src1
        \filterh q0
        vextin2
        \filterh q1

        cmp r5, #4
        beq 4f
8:      subs  r4, #1 	//height--
        \filterv 		// q8 out
        vld1.16        {q0}, [r8], r9 	//q0=src2[0-15], src2+=src2stride
        vqadd.s16      q0, q8 			// q0+=q8
        vqrshrun.s16   d0, q0, #7 		// recadrage
        vst1.8         d0, [r0], r1 	// *dst=d0, dst+=dststride
        regshuffle_q2
        vextin2
        \filterh q1 	//MODIF Green
        bne 8b
        subs  r5, #8
        beq 99f
        mov r4, r12
        add r6, #8
        mov r0, r6
        add r10, #16
        mov r8, r10
        add r7, #8
        mov r2, r7
        b 0b
4:      subs  r4, #1
        \filterv
        vld1.16      d0, [r8], r9
        vqadd.s16    d0, d16
        vqrshrun.s16 d0, q0, #7
        vst1.32      d0[0], [r0], r1
        regshuffle_q2
        vextin2
        \filterh q1 	//MODIF Green
        bne 4b
99:     vpop {d8-d15}
        pop {r4-r10}
        bx lr
.endm


function ff_hevc_put_qpel_uw_pixels_green1_neon_8, export=1
        push     {r4-r10}
        ldr       r5, [sp, #28] // width
        ldr       r4, [sp, #32] // height
        ldr       r8, [sp, #36] // src2
        ldr       r9, [sp, #40] // src2stride
        vpush    {d8-d15}
        mov      r12, r4
        mov       r6, r0
        mov       r7, r2

        lsl       r9, #1
        cmp       r5, #4
        beq       4f
        mov       r10, r8
8:      subs      r4, #1
        vextin2
		vshll.u8  q7, d16, #6
        vld1.16        {q0}, [r8], r9
        vqadd.s16      q0, q7
        vqrshrun.s16   d0, q0, #7
        vst1.8         d0, [r0], r1
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
        bne       8b
4:      subs      r4, #1
        vextin2
        vshll.u8  q7, d16, #6
        vld1.16      d0, [r8], r9
        vqadd.s16    d0, d14
        vqrshrun.s16 d0, q0, #7
        vst1.32      d0[0], [r0], r1
        bne       4b
99:     vpop     {d8-d15}
        pop      {r4-r10}
        bx lr
endfunc

/*
function ff_hevc_put_qpel_uw_pixels_green1_neon_8, export=1
        push   {r4-r9}
        // r0 = dst (uint8)
        // r1 = dst_stride
        // r2 = src (uint8)
        // r3 = src_stride
        ldr    r5, [sp, #24] // r5 = width
        ldr    r4, [sp, #28] // r4 = height
        ldr    r8, [sp, #32] // r8 = src2 (uint16)
        ldr    r9, [sp, #36] // r9 = src2stride
        vpush {d8-d15}
2:      subs  r4, #1
        vld1.8         {d0}, [r2], r3
        vld1.16        {q1}, [r8], r9
        vshll.u8       q0, d0, #6
        vqadd.s16      q0, q1
        vqrshrun.s16   d0, q0, #7
        vst1.8      d0, [r0], r1
        bne 2b
        vpop {d8-d15}
        pop   {r4-r9}
        bx lr
endfunc
*/

function ff_hevc_put_qpel1_h1v1_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_1 qpel_filter1_1_32b
endfunc

function ff_hevc_put_qpel1_h2v1_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_2 qpel_filter1_1_32b
endfunc

function ff_hevc_put_qpel1_h3v1_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_3 qpel_filter1_1_32b
endfunc

function ff_hevc_put_qpel1_h1v2_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_1 qpel_filter1_2_32b
endfunc

function ff_hevc_put_qpel1_h2v2_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_2 qpel_filter1_2_32b
endfunc

function ff_hevc_put_qpel1_h3v2_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_3 qpel_filter1_2_32b
endfunc

function ff_hevc_put_qpel1_h1v3_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_1 qpel_filter1_3_32b
endfunc

function ff_hevc_put_qpel1_h2v3_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_2 qpel_filter1_3_32b
endfunc

function ff_hevc_put_qpel1_h3v3_neon_8, export=1
        hevc_put_qpel1_hXvY_neon_8 qpel_filter1_3 qpel_filter1_3_32b
endfunc


function ff_hevc_put_qpel1_uw_h1v1_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_1 qpel_filter1_1_32b
endfunc

function ff_hevc_put_qpel1_uw_h2v1_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_2 qpel_filter1_1_32b
endfunc

function ff_hevc_put_qpel1_uw_h3v1_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_3 qpel_filter1_1_32b
endfunc

function ff_hevc_put_qpel1_uw_h1v2_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_1 qpel_filter1_2_32b
endfunc

function ff_hevc_put_qpel1_uw_h2v2_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_2 qpel_filter1_2_32b
endfunc

function ff_hevc_put_qpel1_uw_h3v2_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_3 qpel_filter1_2_32b
endfunc

function ff_hevc_put_qpel1_uw_h1v3_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_1 qpel_filter1_3_32b
endfunc

function ff_hevc_put_qpel1_uw_h2v3_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_2 qpel_filter1_3_32b
endfunc

function ff_hevc_put_qpel1_uw_h3v3_neon_8, export=1
        hevc_put_qpel1_uw_hXvY_neon_8 qpel_filter1_3 qpel_filter1_3_32b
endfunc