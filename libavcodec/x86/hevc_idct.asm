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
add_8:                  dw 32
add_10:                 dw  8
max_pixels_10:          times 8  dw ((1 << 10)-1)

SECTION .text

INIT_XMM sse2

%macro TRANSFORM_DC_ADD 2
cglobal hevc_put_transform%1x%1_dc_add_%2, 4, 6, 4, dst, coeffs, stride, col_limit, temp
    xor            tempw, tempw
    mov            tempw, [coeffsq]
    add            tempw, 1
    sar            tempw, 1
    add            tempw, [add_%2]
    sar            tempw, 14-%2
    movd              m0, tempd
    punpcklwd         m0, m0
    pshufd            m0, m0, 0
    pxor              m1, m1
    xor            tempq, tempq
    mov            tempd, %1
.loop
    pxor              m2, m2
%if %1 == 2 || (%2 == 8 && %1 <= 4)
    movd              m2, [dstq]                                               ; load data from source
%elif %1 == 4 || (%2 == 8 && %1 <= 8)
    movq              m2, [dstq]                                               ; load data from source
%else
    movdqu            m2, [dstq]                                               ; load data from source
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
  ;  CLIPW             m2, m1, [max_pixels_10]                  @TODO fix seg fault when enabled
%endif
%if %1 == 2 || (%2 == 8 && %1 <= 4)
    movd          [dstq], m2
%elif %1 == 4 || (%2 == 8 && %1 <= 8)
    movq          [dstq], m2
%else
    movdqu        [dstq], m2
%endif
    lea             dstq, [dstq+strideq]      ; dst += dststride
    dec            tempd
    jnz                 .loop                 ; height loop
    RET
%endmacro


TRANSFORM_DC_ADD 4, 8
TRANSFORM_DC_ADD 8, 8
TRANSFORM_DC_ADD 16, 8


TRANSFORM_DC_ADD 4, 10
TRANSFORM_DC_ADD 8, 10
