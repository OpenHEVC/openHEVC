; /*
; * Provide SSE luma and chroma mc functions for HEVC decoding
; * Copyright (c) 2013 Pierre-Edouard LEPERE
; *
; * This file is part of FFmpeg.
; *
; * FFmpeg is free software; you can redistribute it and/or
; * modify it under the terms of the GNU Lesser General Public
; * License as published by the Free Software Foundation; either
; * version 2.1 of the License, or (at your option) any later version.
; *
; * FFmpeg is distributed in the hope that it will be useful,
; * but WITHOUT ANY WARRANTY; without even the implied warranty of
; * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; * Lesser General Public License for more details.
; *
; * You should have received a copy of the GNU Lesser General Public
; * License along with FFmpeg; if not, write to the Free Software
; * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
; */
%include "libavutil/x86/x86util.asm"

SECTION_RODATA

max_pixels_10:          times 8  dw ((1 << 10)-1)

SECTION .text

%macro TRANSFORM_DC_ADD 2
cglobal hevc_put_transform%1x%1_dc_add_%2, 3, 4, 4, dst, coeffs, stride, temp
    mov            tempw, [coeffsq]
    add            tempw, 1
    sar            tempw, 1
    add            tempw, (1 << 13-%2)
    sar            tempw, 14-%2
    movd              m0, tempd
    SPLATW            m0, m0, 0
    pxor              m1, m1
    mov            tempd, %1
.loop
%if (%2 == 8 && %1 <= 8)
    movh              m2, [dstq]
%else
    movu              m2, [dstq]
%endif
%if %2 == 8
%if %1 > 8
    punpckhbw         m3, m2, m1
%endif
    punpcklbw         m2, m1
%endif
    paddw             m2, m0
%if (%1 > 8 && %2 == 8)
    paddw             m3, m0
%endif
%if %2 == 8
    packuswb          m2, m3
%else
    CLIPW             m2, m1, [max_pixels_%2]
%endif
%if (%2 == 8 && %1 <= 8)
    movh          [dstq], m2
%else
    movu          [dstq], m2
%endif
    lea             dstq, [dstq+strideq]
    dec            tempd
    jnz                 .loop                 ; height loop
    RET
%endmacro

INIT_XMM sse2

TRANSFORM_DC_ADD 8, 8
TRANSFORM_DC_ADD 16, 8

TRANSFORM_DC_ADD 8, 10

INIT_MMX mmx

TRANSFORM_DC_ADD 4, 8
TRANSFORM_DC_ADD 4, 10
