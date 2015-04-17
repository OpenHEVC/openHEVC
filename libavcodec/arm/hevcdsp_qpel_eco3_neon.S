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

.macro regshuffle_d8 	//rename!
    vmov d16, d17
    vmov d17, d18
    vmov d18, d19
.endm

.macro regshuffle_q8
    vmov q0, q1
    vmov q1, q2
    vmov q2, q3
.endm

.macro vextin8
        pld       [r2]
        vld1.8    {q11}, [r2], r3
        vext.8    d16, d22, d23, #0
        vext.8    d17, d22, d23, #1
        vext.8    d18, d22, d23, #2
.endm

.macro loadin8
        pld       [r2]
        vld1.8    {d16}, [r2], r3
        pld       [r2]
        vld1.8    {d17}, [r2], r3
        pld       [r2]
        vld1.8    {d18}, [r2], r3
	pld 	  [r2]
	vld1.8	  {d19}, [r2], r3
.endm



// 14 cycles
// -8*a + 58*b + 14*c
// q0      q1      q2 
// d0,1    d2,3    d4,5
// QPEL32
.macro qpel_filter_1_32b
	vmov.i16   d16, #58
	vmov.i16   d17, #14
	vshll.s16  q11, d0, #3    // 8 * a0
	vshll.s16  q12, d1, #3    // 8 * a1
	vmull.s16   q9, d2, d16   // 58 * b0
	vmull.s16  q10, d3, d16   // 58 * b1
	vmull.s16  q13, d4, d17   // 14 * c0
	vmull.s16  q14, d5, d17   // 14 * c1
	vadd.s32    q9, q13       // 58*b0 + 14*c0
	vadd.s32   q10, q14       // 58*b1 + 14*c1
	vsub.s32   q9, q11       // 58*b0 + 14*c0 - 8*a0
	vsub.s32   q10, q12        // 58*b1 + 14*c1 - 8*a1
	vqshrn.s32  d16, q9, #6
	vqshrn.s32  d17, q10, #6
.endm


// 13 cycles
// -8*a + 40*b + 32*c
// q0      q1      q2 
// d0,1    d2,3    d4,5
// HPEL32
.macro qpel_filter_2_32b
	vmov.i16   d16, #40
	vshll.s16  q11, d4, #5    // 32 * c0
	vshll.s16  q12, d5, #5    // 32 * c1
	vmull.s16   q9, d2, d16   // 40 * b0
	vmull.s16  q10, d3, d16   // 40 * b1
    vshll.s16  q13, d0, #3    // 8 * a0
    vshll.s16  q14, d1, #3    // 8 * a1
	vadd.s32    q9, q11       // 40*b0 + 32*c0
	vadd.s32   q10, q12       // 40*b1 + 32*c1
	vsub.s32   q9, q13       // 40*b0 + 32*c0 - 8*a0
	vsub.s32   q10, q14        // 40*b0 + 32*c0 - 8*a1
	vqshrn.s32  d16, q9, #6
	vqshrn.s32  d17, q10, #6
.endm

.macro qpel_filter_20_32b out=q7
	vmov.s16 q8, q1
.endm

// 14 cycles
// -4*a + 20*b + 48*c
// q0      q1      q2 
// d0,1    d2,3    d4,5
// QPEL32 ret
.macro qpel_filter_3_32b
	vmov.i16   d16, #20
	vmov.i16   d17, #48
	vshll.s16  q11, d0, #2    // 4 * a0
	vshll.s16  q12, d1, #2    // 4 * a1
	vmull.s16   q9, d2, d16   // 20 * b0
	vmull.s16  q10, d3, d16   // 20 * b1
	vmull.s16  q13, d4, d17   // 48 * c0
	vmull.s16  q14, d5, d17   // 48 * c1
	vadd.s32    q9, q13       // 20*b0 + 48*c0
	vadd.s32   q10, q14       // 20*b1 + 48*c1
	vsub.s32   q9, q11       // 20*b0 + 48*c0 - 4*a0
	vsub.s32   q10, q12        // 20*b1 + 48*c1 - 4*a1
	vqshrn.s32  d16, q9, #6
	vqshrn.s32  d17, q10, #6
.endm

//      -8*a  +  58*b +  14*c
// input  d16      d17     d18
// QPEL
// 7 cycles
.macro qpel_filter_1 out=q7
	vmov.u8   d24, #58
	vmov.u8   d25, #14
	vshll.u8  q13, d16, #3     // 8*a
	vmull.u8   \out, d17, d24    // 58*b
	vmull.u8  q15, d18, d25    // 14*c
	vadd.u16  \out, q15    // 58*b + 14*c
	vsub.s16  \out, q13    // 58*b + 14*c - 8*a
.endm


//      -8*a  + 40*b +  32*c  d e f g
// input  d16    d17     d18
// HPEL
// 6 cycles
.macro qpel_filter_2 out=q7
	vmov.u8   d24, #40
	vshll.u8  q13, d18, #5     // 32*c
	vmull.u8   \out, d17, d24    // 40*b
    vshll.u8  q15, d16, #3     // 8*a
	vadd.u16  \out, q13    // 40*b + 32*c
	vsub.s16  \out, q15    // 40*b + 32*c - 8*a
.endm


//      -4*a  +  20*b +  48*c 
// input  d16      d17     d18
// QPEL ret
// 7 cycles
.macro qpel_filter_3 out=q7
	vmov.u8   d24, #20
	vmov.u8   d25, #48
	vshll.u8  q13, d16, #2     // 4*a
	vmull.u8   \out, d17, d24    // 20*b
	vmull.u8  q15, d18, d25    // 48*c
	vadd.u16  \out, q15    // 20*b + 48*c
	vsub.s16  \out, q13    // 20*b + 48*c - 4*a
.endm



.macro  hevc_put_qpel_vX_neon_8 filter
        push   {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width
        vpush {d8-d15}
        sub       r2, r3	 	// pointe le premier pixel
        mov       r12, r4 		//_height
        mov       r6, r0 		// _dst
        mov       r7, r2 		// _src
        lsl       r1, #1 		// dststride*2
0:      loadin8 			//d16 d17 d18 (a b c)
        cmp       r5, #4 	// width==4?
        beq       4f 		// br 4f si width=4
8:      subs r4, #1 		// height--
        \filter 			// q7 = vect filtre
        vst1.16    {q7}, [r0], r1 	// *dst=q7, dst+=dststride
        regshuffle_d8 		// a=b, b=c
        vld1.8    {d19}, [r2], r3 // charge d19 (c), src+=srcstride //MODIF ECO
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
        regshuffle_d8		//veillissement
        vld1.32    {d19[0]}, [r2], r3 	// MODIF ECO
        bne 4b 
99:     vpop {d8-d15}
        pop {r4, r5, r6, r7}
        bx lr
.endm

.macro  hevc_put_qpel_uw_vX_neon_8 filter
        push   {r4-r10}
        ldr    r5, [sp, #28] // width
        ldr    r4, [sp, #32] // height
        ldr    r8, [sp, #36] // src2
        ldr    r9, [sp, #40] // src2stride
        vpush {d8-d15}
        sub       r2, r3
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r8, #0
        bne       .Lbi\@
        //lsl       r1, #1
0:      loadin8
        cmp       r5, #4
        beq       4f
8:      subs r4, #1
        \filter
        vqrshrun.s16   d0, q7, #6
        vst1.8    d0, [r0], r1
        regshuffle_d8
        vld1.8    {d19}, [r2], r3 	// MODIF ECO
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
        regshuffle_d8
        vld1.32    {d19[0]}, [r2], r3 	//MODIF ECO
        bne 4b
        b   99f
.Lbi\@: lsl       r9, #1
        mov       r10, r8 	// _src2
0:      loadin8
        cmp       r5, #4
        beq       4f
8:      subs r4, #1
        \filter
        vld1.16        {q0}, [r8], r9
        vqadd.s16      q0, q7
        vqrshrun.s16   d0, q0, #7
        vst1.8         d0, [r0], r1
        regshuffle_d8
        vld1.8    {d19}, [r2], r3	//MODIF ECO
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
        regshuffle_d8
        vld1.32    {d19[0]}, [r2], r3	//MODIF ECO
        bne 4b
99:     vpop {d8-d15}
        pop {r4-r10}
        bx lr
.endm


function ff_hevc_put_qpel_v1_neon_8, export=1
        hevc_put_qpel_vX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_v2_neon_8, export=1
        hevc_put_qpel_vX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_v3_neon_8, export=1
        hevc_put_qpel_vX_neon_8 qpel_filter_3
endfunc


function ff_hevc_put_qpel_uw_v1_neon_8, export=1
        hevc_put_qpel_uw_vX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_uw_v2_neon_8, export=1
        hevc_put_qpel_uw_vX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_uw_v3_neon_8, export=1
        hevc_put_qpel_uw_vX_neon_8 qpel_filter_3
endfunc




.macro hevc_put_qpel_hX_neon_8 filter
        push     {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width

        vpush    {d8-d15}
        sub       r2, #1
        lsl       r1, #1
        mov      r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r5, #4
        beq       4f
8:      subs      r4, #1
        vextin8
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
        vextin8
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


.macro hevc_put_qpel_uw_hX_neon_8 filter
        push     {r4-r10}
        ldr       r5, [sp, #28] // width
        ldr       r4, [sp, #32] // height
        ldr       r8, [sp, #36] // src2
        ldr       r9, [sp, #40] // src2stride
        vpush    {d8-d15}
        sub       r2, #1
        mov      r12, r4
        mov       r6, r0
        mov       r7, r2
        cmp       r8, #0
        bne       .Lbi\@
        cmp       r5, #4
        beq       4f
8:      subs      r4, #1
        vextin8
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
        vextin8
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
        vextin8
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
        vextin8
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


function ff_hevc_put_qpel_h1_neon_8, export=1
        hevc_put_qpel_hX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_h2_neon_8, export=1
        hevc_put_qpel_hX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_h3_neon_8, export=1
        hevc_put_qpel_hX_neon_8 qpel_filter_3
endfunc


function ff_hevc_put_qpel_uw_h1_neon_8, export=1
        hevc_put_qpel_uw_hX_neon_8 qpel_filter_1
endfunc

function ff_hevc_put_qpel_uw_h2_neon_8, export=1
        hevc_put_qpel_uw_hX_neon_8 qpel_filter_2
endfunc

function ff_hevc_put_qpel_uw_h3_neon_8, export=1
        hevc_put_qpel_uw_hX_neon_8 qpel_filter_3
endfunc

//hevc.c:41:const uint8_t ff_hevc_qpel_extra_before[4] = { 0, 3, 3, 2 };
//hevc.c:42:const uint8_t ff_hevc_qpel_extra_after[4]  = { 0, 3, 4, 4 };
//hevc.c:43:const uint8_t ff_hevc_qpel_extra[4]        = { 0, 6, 7, 6 };

.macro hevc_put_qpel_hXvY_neon_8 filterh filterv
        push   {r4, r5, r6, r7}
        ldr    r4, [sp, #16] // height
        ldr    r5, [sp, #20] // width

        vpush {d8-d15}
        sub       r2, #1
	sub       r2, r3
        lsl       r1, #1
        mov       r12, r4
        mov       r6, r0
        mov       r7, r2
0:      vextin8
        \filterh q0
        vextin8
        \filterh q1
        vextin8
        \filterh q2
	vextin8
	\filterh q3 	//MODIF ECO

        cmp r5, #4
        beq 4f
8:      subs  r4, #1
        \filterv
        vst1.16    {q8}, [r0], r1
        regshuffle_q8
        vextin8
        \filterh q3 	//MODIF ECO
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
        regshuffle_q8
        vextin8
        \filterh q3 	// MODIF ECO
        bne 4b
99:     vpop {d8-d15}
        pop {r4, r5, r6, r7}
        bx lr
.endm

.macro hevc_put_qpel_uw_hXvY_neon_8 filterh filterv
        push     {r4-r10}
        ldr       r5, [sp, #28] // width
        ldr       r4, [sp, #32] // height
        ldr       r8, [sp, #36] // src2
        ldr       r9, [sp, #40] // src2stride
        vpush {d8-d15}
        sub       r2, #1
        sub       r2, r3  // extra_before 1
        mov       r12, r4 		// _height
        mov       r6, r0 		// _dst
        mov       r7, r2 		// _src
        cmp       r8, #0 		// src2?
        bne       .Lbi\@ 		//src2 branch
0:      vextin8
        \filterh q0
        vextin8
        \filterh q1
        vextin8
        \filterh q2
		vextin8
		\filterh q3	//MODIF ECO

        cmp r5, #4
        beq 4f
8:      subs  r4, #1
        \filterv
        vqrshrun.s16   d0, q8, #6
        vst1.8    d0, [r0], r1
        regshuffle_q8
        vextin8
        \filterh q3	//MODIF ECO
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
        regshuffle_q8
        vextin8
        \filterh q3 	//MODIF ECO
        bne 4b
        b   99f
.Lbi\@: lsl      r9, #1 	// BIDIR PROCESS
        mov      r10, r8 	//_src2
0:      vextin8 		// load src1
        \filterh q0
        vextin8
        \filterh q1
        vextin8
        \filterh q2
		vextin8
		\filterh q3		//MODIF ECO

        cmp r5, #4
        beq 4f
8:      subs  r4, #1 	//height--
        \filterv 		// q8 out
        vld1.16        {q0}, [r8], r9 	//q0=src2[0-15], src2+=src2stride
        vqadd.s16      q0, q8 			// q0+=q8
        vqrshrun.s16   d0, q0, #7 		// recadrage
        vst1.8         d0, [r0], r1 	// *dst=d0, dst+=dststride
        regshuffle_q8
        vextin8
        \filterh q3 	//MODIF ECO
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
        regshuffle_q8
        vextin8
        \filterh q3 	//MODIF ECO
        bne 4b
99:     vpop {d8-d15}
        pop {r4-r10}
        bx lr
.endm



function ff_hevc_put_qpel_h1v1_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_1 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_h2v1_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_2 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_h3v1_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_3 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_h1v2_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_1 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_h2v2_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_2 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_h3v2_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_3 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_h1v3_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_1 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_h2v3_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_2 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_h3v3_neon_8, export=1
        hevc_put_qpel_hXvY_neon_8 qpel_filter_3 qpel_filter_3_32b
endfunc


function ff_hevc_put_qpel_uw_h1v1_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_1 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_uw_h2v1_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_2 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_uw_h3v1_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_3 qpel_filter_1_32b
endfunc

function ff_hevc_put_qpel_uw_h1v2_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_1 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_uw_h2v2_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_2 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_uw_h3v2_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_3 qpel_filter_2_32b
endfunc

function ff_hevc_put_qpel_uw_h1v3_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_1 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_uw_h2v3_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_2 qpel_filter_3_32b
endfunc

function ff_hevc_put_qpel_uw_h3v3_neon_8, export=1
        hevc_put_qpel_uw_hXvY_neon_8 qpel_filter_3 qpel_filter_3_32b
endfunc



function ff_hevc_put_pixels_neon_8, export=1
        pld    [r2]
        push   {r4, r5, r6}
        pld    [r2, r3]
        ldr    r4, [sp, #12] // height
        ldr    r5, [sp, #24] // width

        lsl    r1, #1
        cmp    r5, #2
        beq    .Lw2
        cmp    r5, #6
        beq    .Lw6
        cmp    r5, #48
        beq    .Lw48
        bgt    .Lw64
        adr    r6, .Ltable
        add    r6, r6, r5, lsr #1
        mov    pc, r6
.Ltable:
        b .Lendput
        b .Lw4
        b .Lw8
        b .Lw12
        b .Lw16
        b .Lendput
        b .Lw24
        b .Lendput
        b .Lw32

.Lw2:
        vmov.u8      d5, #255
        vshr.u64     d5, #32
.LwX2:
        subs r4, #1
        vld1.32     {d0[0]}, [r2], r3
        pld [r2]
        vld1.32     d6, [r0]
        vshll.u8    q0, d0, #6
        vbit        d6, d0, d5
        vst1.32     d6, [r0], r1
        bne .LwX2
        b .Lendput
.Lw6:
        vmov.u8      q10, #255
        vshr.u64     d21, #32
.LwX6:
        subs r4, #1
        vld1.16     {d0}, [r2], r3
        pld [r2]
        vshll.u8    q0, d0, #6
        vld1.8      {q12}, [r0]
        vbit        q12, q0, q10
        vst1.8      {q12}, [r0], r1
        bne .LwX6
        b .Lendput

.Lw4:
        subs r4, #2
        vld1.32   {d0[0]}, [r2], r3
        vld1.32   {d0[1]}, [r2], r3
        pld       [r2]
        pld       [r2, r3]
        vshll.u8   q0, d0, #6
        vst1.64   {d0}, [r0], r1
        vst1.64   {d1}, [r0], r1
        bne .Lw4
        b .Lendput
.Lw8:
        subs r4, #2
        vld1.8   {d0}, [r2], r3
        vld1.8   {d2}, [r2], r3
        pld        [r2]
        pld        [r2, r3]
        vshll.u8   q0, d0, #6
        vshll.u8   q1, d2, #6
        vst1.16   {q0}, [r0], r1
        vst1.16   {q1}, [r0], r1
        bne .Lw8
        b .Lendput
.Lw12:
        subs r4, #2
        vld1.64    {d0}, [r2]
        add       r2, #8
        vld1.32   {d1[0]}, [r2], r3
        sub       r2, #8
        vld1.64    {d2}, [r2]
        add       r2, #8
        vld1.32   {d1[1]}, [r2], r3
        sub       r2, #8
        pld       [r2]
        pld       [r2, r3]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vmov      d22, d19
        vst1.64   {d16, d17, d18}, [r0], r1
        vst1.64   {d20, d21, d22}, [r0], r1
        bne       .Lw12
        b       .Lendput
.Lw16:
        subs r4, #2
        vld1.8   {q0}, [r2], r3
        vld1.8   {q1}, [r2], r3
        pld       [r2]
        pld       [r2, r3]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vst1.8    {q8, q9}, [r0], r1
        vst1.8    {q10, q11}, [r0], r1
        bne .Lw16
        b .Lendput
.Lw24:
        subs r4, #1
        vld1.8   {d0, d1, d2}, [r2], r3
        pld       [r2]
        vshll.u8  q10, d0, #6
        vshll.u8  q11, d1, #6
        vshll.u8  q12, d2, #6
        vstm     r0, {q10, q11, q12}
        add      r0, r1
        bne .Lw24
        b .Lendput
.Lw32:
        subs r4, #1
        vld1.8 {q0, q1}, [r2], r3
        pld       [r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vstm    r0, {q8, q9, q10, q11}
        add     r0, r1
        bne .Lw32
        b .Lendput
.Lw48:
        subs r4, #1
        vld1.8    {q0, q1}, [r2]
        add r2, #32
        vld1.8    {q2}, [r2], r3
        sub r2, #32
        pld       [r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vshll.u8  q12, d4, #6
        vshll.u8  q13, d5, #6
        vstm r0, {q8, q9, q10, q11, q12, q13}
        add  r0, r1
        bne .Lw48
        b .Lendput
.Lw64:
        subs r4, #1
        vld1.8    {q0, q1}, [r2]
        add r2, #32
        vld1.8    {q2, q3}, [r2], r3
        sub      r2, #32
        pld       [r2]
        vshll.u8  q8, d0, #6
        vshll.u8  q9, d1, #6
        vshll.u8  q10, d2, #6
        vshll.u8  q11, d3, #6
        vshll.u8  q12, d4, #6
        vshll.u8  q13, d5, #6
        vshll.u8  q14, d6, #6
        vshll.u8  q15, d7, #6
        vstm    r0, {q8, q9, q10, q11, q12, q13, q14, q15}
        add r0, r1
        bne .Lw64
        b .Lendput
.Lendput:
        pop {r4, r5, r6}
        bx lr
endfunc

halt:
        b halt

function ff_hevc_put_qpel_uw_pixels_neon_8, export=1
        push   {r4-r9}
        ldr    r5, [sp, #24] // width
        ldr    r4, [sp, #28] // height
        ldr    r8, [sp, #32] // src2
        ldr    r9, [sp, #36] // src2stride
        vpush {d8-d15}
        cmp    r8, #0
        bne    2f
1:      subs r4, #1
        vld1.8     {d0}, [r2], r3
        vst1.8      d0, [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r9}
        bx lr
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

.macro put_qpel_uw_pixels width regs
function ff_hevc_put_qpel_uw_pixels_neon_8_w\width, export=1
        push   {r4-r9}
        ldr    r5, [sp, #36] // width
        ldr    r4, [sp, #24] // height
        vpush {d8-d15}
1:      subs r4, #1
        pld    [r2]
        vld1.32     \regs , [r2], r3
        pld    [r0]
        vst1.32     \regs , [r0], r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r9}
        bx lr
endfunc
.endm

.macro put_qpel_uw_pixels_m width regs regs2
function ff_hevc_put_qpel_uw_pixels_neon_8_w\width, export=1
        push   {r4-r9}
        ldr    r5, [sp, #36] // width
        ldr    r4, [sp, #24] // height
        vpush {d8-d15}
1:      subs r4, #1
        pld    [r2]
        mov      r6, r2
        vld1.32   \regs , [r2]!
        vld1.32   \regs2, [r2]
        add      r2, r6, r3
        mov      r7, r0
        pld    [r0]
        vst1.32   \regs , [r0]!
        vst1.32   \regs2, [r0]
        add      r0, r7, r1
        bne 1b
        vpop {d8-d15}
        pop   {r4-r9}
        bx lr
endfunc
.endm

put_qpel_uw_pixels    4 "{d0[0]}"
put_qpel_uw_pixels    8 "{d0}"
put_qpel_uw_pixels_m 12 "{d0}" {d1[0]}
put_qpel_uw_pixels   16 "{q0}"
put_qpel_uw_pixels   24 "{d0-d2}"
put_qpel_uw_pixels   32 "{q0,q1}"
put_qpel_uw_pixels_m 48 "{q0,q1}" {q2}
put_qpel_uw_pixels_m 64 "{q0-q1}" {q2-q3}

function ff_hevc_put_weighted_pred_avg_neon_8, export=1
        pld    [r2]
        pld    [r3]
        push   {r4, r5, r6, r7}
        ldr    r7, [sp, #16] // srcstride
        ldr    r5, [sp, #20] // width
        ldr    r4, [sp, #24] // height
        lsl    r7, #1

        cmp    r5, #48
        blt    .Ll48
        vpush {d8-d15}
        bgt    .La64
        b      .La48
.Ll48:
        cmp    r5, #2
        beq    .La2
        cmp    r5, #6
        beq    .La6
        adr    r6, .Latable
        add    r6, r6, r5, lsr #1
        mov    pc, r6
.Latable:
        b .Lendavg
        b .La4
        b .La8
        b .La12
        b .La16
        b .Lendavg
        b .La24
        b .Lendavg
        b .La32
.La2:
        vmov.u8      d5, #255
        vshr.u64     d5, #48
.LaX2:
        subs r4, #1
        vld1.16     {d0}, [r2], r7
        pld [r2]
        vld1.16     {d1}, [r3], r7
        pld [r3]
        vld1.32     d6[0], [r0]
        vqadd.s16   d0, d1
        vqrshrun.s16 d0, q0, #7
        vbit        d6, d0, d5
        vst1.32     d6[0], [r0], r1
        bne .LaX2
        b .Lendavg
.La6:
        vmov.u8      d5, #255
        vshr.u64     d5, #16
.LaX6:
        subs r4, #1
        vld1.16     {q0}, [r2,:128], r7
        pld [r2]
        vld1.16     {q1}, [r3,:128], r7
        pld [r3]
        vld1.8       d6, [r0]
        vqadd.s16   q0, q1
        vqrshrun.s16 d0, q0, #7
        vbit        d6, d0, d5
        vst1.8      d6, [r0], r1
        bne .LaX6
        b .Lendavg
.La4:
        subs r4, #2
        vld1.16     {d0}, [r2], r7
        vld1.16     {d1}, [r2], r7
        pld         [r2]
        pld         [r2, r7]
        vld1.16     {d2}, [r3], r7
        vld1.16     {d3}, [r3], r7
        pld         [r3]
        pld         [r3, r7]
        vqadd.s16   q0, q1
        vqrshrun.s16 d0, q0, #7
        vst1.32     d0[0], [r0], r1
        vst1.32     d0[1], [r0], r1
        bne .La4
        b .Lendavg
.La8:
        subs r4, #1
        vld1.16     {q0}, [r2], r7
        pld [r2]
        vld1.16     {q1}, [r3], r7
        pld [r3]
        vqadd.s16   q0, q1
        vqrshrun.s16 d0, q0, #7
        vst1.8      d0, [r0], r1
        bne .La8
        b .Lendavg
.La12:
        subs r4, #1
        vld1.16     {d0, d1, d2}, [r2], r7
        pld [r2]
        vld1.16     {d4, d5, d6}, [r3], r7
        pld [r3]
        vqadd.s16   q0, q2
        vqadd.s16   d2, d6
        vqrshrun.s16 d0, q0, #7
        vqrshrun.s16 d2, q1, #7
        vst1.8      d0, [r0]
        add         r0, #8
        vst1.32     d2[0], [r0], r1
        sub         r0, #8
        bne .La12
        b .Lendavg
.La16:
        subs r4, #1
        vld1.16   {q0, q1}, [r2,:128], r7
        pld       [r2]
        vld1.16   {q2, q3}, [r3,:128], r7
        pld       [r3]
        vqadd.s16  q0, q2
        vqadd.s16  q1, q3
        vqrshrun.s16   d20, q0, #7
        vqrshrun.s16   d21, q1, #7
        vst1.8    {d20, d21}, [r0], r1
        bne .La16
        b .Lendavg
.La24:
        subs r4, #1
        vldm      r2, {q0-q2}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q8-q10}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q8
        vqadd.s16  q1, q9
        vqadd.s16  q2, q10
        vqrshrun.s16   d20, q0, #7
        vqrshrun.s16   d21, q1, #7
        vqrshrun.s16   d22, q2, #7
        vst1.8    {d20-d22}, [r0], r1
        bne .La24
        b .Lendavg
.La32:
        subs r4, #1
        vldm      r2, {q0-q3}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q8-q11}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q8
        vqadd.s16  q1, q9
        vqadd.s16  q2, q10
        vqadd.s16  q3, q11
        vqrshrun.s16   d20, q0, #7
        vqrshrun.s16   d21, q1, #7
        vqrshrun.s16   d22, q2, #7
        vqrshrun.s16   d23, q3, #7
        vst1.8    {d20-d23}, [r0], r1
        bne .La32
        b .Lendavg
.La48:
        subs r4, #1
        vldm      r2, {q0-q5}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q10-q15}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q10
        vqadd.s16  q1, q11
        vqadd.s16  q2, q12
        vqadd.s16  q3, q13
        vqadd.s16  q4, q14
        vqadd.s16  q5, q15
        vqrshrun.s16   d0, q0, #7
        vqrshrun.s16   d1, q1, #7
        vqrshrun.s16   d2, q2, #7
        vqrshrun.s16   d3, q3, #7
        vqrshrun.s16   d4, q4, #7
        vqrshrun.s16   d5, q5, #7
        vstm    r0, {q0-q2}
        add r0, r1
        bne .La48
        b   .Lendavgl
.La64:
        subs r4, #1
        vldm      r2, {q0-q7}
        add       r2, r7
        pld       [r2]
        vldm      r3, {q8-q15}
        add       r3, r7
        pld       [r3]
        vqadd.s16  q0, q8
        vqadd.s16  q1, q9
        vqadd.s16  q2, q10
        vqadd.s16  q3, q11
        vqadd.s16  q4, q12
        vqadd.s16  q5, q13
        vqadd.s16  q6, q14
        vqadd.s16  q7, q15
        vqrshrun.s16   d24, q0, #7
        vqrshrun.s16   d25, q1, #7
        vqrshrun.s16   d26, q2, #7
        vqrshrun.s16   d27, q3, #7
        vqrshrun.s16   d28, q4, #7
        vqrshrun.s16   d29, q5, #7
        vqrshrun.s16   d30, q6, #7
        vqrshrun.s16   d31, q7, #7
        vstm    r0, {q12-q15}
        add r0, r1
        bne .La64
        b .Lendavgl
.Lendavgl:
        vpop {d8-d15}
.Lendavg:
        pop {r4, r5, r6, r7}
        bx lr
endfunc
