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

#define MAX_PB_SIZE #64

.set dir_h,  0
.set dir_v,  1
.set dir_hv, 2

.set fun_pre, 0
.set fun_uni, 1
.set fun_bi,  2

/*      r0 : dst
        r1 : dst_stride
        r2 : src
        r3 : src_stride
        r4 : height
        r5 : width
        r6 : mx
        r7 : my
 */
/**  */

.macro store_128
        vst1.16     {q5}, [r0], r1
.endm

.macro store_64
        vst1.16     d10, [r0], r1
.endm

.macro store_32
        vst1.32     d10[0], [r0], r1
.endm

.macro store_16
        vst1.16     d10[0], [r0], r1
.endm

.macro loop_pel_neon_8 funtype dirtype load_coeffs load_coeffs_32 preload_data_v preload_data_hv load_data_h8 load_data_h42 load_data_v load_data_hv8 load_data_hv42 exec_filter exec_filter_hv8 exec_filter_hv42
        // Retreive arguments
        push   {r4-r12}
.if \funtype == fun_pre
        mov r4, r3
        mov r3, r2
        mov r2, r1
        mov r1, MAX_PB_SIZE
        ldr    r5, [sp, #44] // width
    .if \dirtype == dir_h
        ldr    r6, [sp, #36] // mx
    .endif
    .if \dirtype == dir_v
        ldr    r6, [sp, #40] // my
    .endif
    .if \dirtype == dir_hv
        ldr    r6, [sp, #36] // mx
        ldr    r7, [sp, #40] // my
    .endif
.endif
.if \funtype == fun_uni
        ldr    r4, [sp, #36] // height
        ldr    r5, [sp, #48] // width
    .if \dirtype == dir_h
        ldr    r6, [sp, #40] // mx
    .endif
    .if \dirtype == dir_v
        ldr    r6, [sp, #44] // my
    .endif
    .if \dirtype == dir_hv
        ldr    r6, [sp, #40] // mx
        ldr    r7, [sp, #44] // my
    .endif
.endif
.if \funtype == fun_bi
        ldr    r4, [sp, #40] // height
        ldr    r5, [sp, #52] // width
        ldr    r8, [sp, #36] // src2
        mov    r9, MAX_PB_SIZE // src2stride
    .if \dirtype == dir_h
        ldr    r6, [sp, #44] // mx
    .endif
    .if \dirtype == dir_v
        ldr    r6, [sp, #48] // my
    .endif
    .if \dirtype == dir_hv
        ldr    r6, [sp, #44] // mx
        ldr    r7, [sp, #48] // my
    .endif
.endif
        vpush {d8-d15}


        // Initialize Filter Coefficients
        sub  r6, #1
        \load_coeffs r6 \dirtype
.if \dirtype == dir_hv
        sub  r7, #1
        \load_coeffs_32 r7
.endif
        // Initialize Indexes
.if \funtype == fun_pre
        lsl  r1, #1
.endif
        mov  r12, r4
        mov  r6, r0
        mov  r7, r2
.if \funtype == fun_bi
        lsl  r9, #1
        mov  r10, r8
.endif
        // Preload data in memory
.if \dirtype == dir_v
        \preload_data_v
.endif
.if \dirtype == dir_hv
        \preload_data_hv
.endif
        cmp  r5, #6
        bgt  8f
        cmp  r5, #4
        blt  2f
        b    4f
8:
.if \dirtype == dir_h
        \load_data_h8
.endif
.if \dirtype == dir_v
        \load_data_v
.endif
.if \dirtype == dir_hv
        \load_data_hv8
.endif
.if \dirtype != dir_hv
        \exec_filter q5
.else
        \exec_filter q6
        \exec_filter_hv8 d10 d11
.endif
        subs r4, #1
.if \funtype == fun_uni
        vqrshrun.s16   d10, q5, #6
.endif
.if \funtype == fun_bi
        vld1.16        {q14}, [r8], r9
        vqadd.s16      q14, q5
        vqrshrun.s16   d10, q14, #7
.endif
.if \funtype == fun_pre
        store_128
.else
        store_64
.endif
        bne  8b
        subs r5, #8
        beq  99f
        mov  r4, r12
.if \funtype == fun_pre
        add       r6, #16
.else
        add       r6, #8
.endif
        mov       r0, r6
        add       r7, #8
        mov       r2, r7
.if \funtype == fun_bi
        add       r10, #16
        mov       r8, r10
.endif
.if \dirtype == dir_v
        \preload_data_v
.endif
.if \dirtype == dir_hv
        \preload_data_hv
.endif
        cmp       r5, #4
        bgt       8b
4:
.if \dirtype == dir_h
        \load_data_h42
.endif
.if \dirtype == dir_v
        \load_data_v
.endif
.if \dirtype == dir_hv
        \load_data_hv42
.endif
.if \dirtype != dir_hv
        \exec_filter q5
.else
        \exec_filter q6
        \exec_filter_hv42 d10
.endif
        subs r4, #1
.if \funtype == fun_uni
        vqrshrun.s16   d10, q5, #6
.endif
.if \funtype == fun_bi
        vld1.16        {q14}, [r8], r9
        vqadd.s16      q14, q5
        vqrshrun.s16   d10, q14, #7
.endif
.if \funtype == fun_pre
        store_64
.else
        store_32
.endif
        bne 4b
        subs      r5, #4
        beq       99f
        mov       r4, r12
.if \funtype == fun_pre
        add       r6, #8
.else
        add       r6, #4
.endif
        mov       r0, r6
        add       r7, #4
        mov       r2, r7
.if \funtype == fun_bi
        add       r10, #8
        mov       r8, r10
.endif
.if \dirtype == dir_v
        \preload_data_v
.endif
.if \dirtype == dir_hv
        \preload_data_hv
.endif
2:
.if \dirtype == dir_h
        \load_data_h42
.endif
.if \dirtype == dir_v
        \load_data_v
.endif
.if \dirtype == dir_hv
        \load_data_hv42
.endif
.if \dirtype != dir_hv
        \exec_filter q5
.else
        \exec_filter q6
        \exec_filter_hv42 d10
.endif
        subs r4, #1
.if \funtype == fun_uni
        vqrshrun.s16   d10, q5, #6
.endif
.if \funtype == fun_bi
        vld1.16        {q14}, [r8], r9
        vqadd.s16      q14, q5
        vqrshrun.s16   d10, q14, #7
.endif
.if \funtype == fun_pre
        store_32
.else
        store_16
.endif
        bne 2b
        // Exit
99:     vpop {d8-d15}
        pop {r4-r12}
        bx lr
.endm
