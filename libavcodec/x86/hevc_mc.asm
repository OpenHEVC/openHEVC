;/*
; * Provide SSE luma mc functions for HEVC decoding
; * Copyright (c) 2013 Pierre-Edouard LEPERE
; *
; * This file is part of Libav.
; *
; * Libav is free software; you can redistribute it and/or
; * modify it under the terms of the GNU Lesser General Public
; * License as published by the Free Software Foundation; either
; * version 2.1 of the License, or (at your option) any later version.
; *
; * Libav is distributed in the hope that it will be useful,
; * but WITHOUT ANY WARRANTY; without even the implied warranty of
; * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; * Lesser General Public License for more details.
; *
; * You should have received a copy of the GNU Lesser General Public
; * License along with Libav; if not, write to the Free Software
; * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
; */
;%include "libavutil/x86/x86inc.asm"
%include "libavutil/x86/x86util.asm"

SECTION_RODATA


SECTION .text


INIT_XMM sse4	; adds ff_ and _sse4 to function name

;******************************
;void put_hevc_mc_pixels_8(int16_t *dst, ptrdiff_t dststride,
;                                       uint8_t *_src, ptrdiff_t _srcstride,
;                                       int width, int height, int mx, int my,
;                                       int16_t* mcbuffer)
;
;	r0 : *dst
;	r1 : dststride
;	r2 : *src
; 	r3 : srcstride
;	r4 : width
;	r5 : height
;
;******************************
cglobal put_hevc_mc_pixels_8, 9, 12
	pxor		xmm0,xmm0		;set register at zero
	mov			r6,0			;height
	mov 		r9,0

	;8 by 8
mc_pixels_h:	;for height
	mov			r7,0			;width

mc_pixels_w:	;for width

	pxor		xmm1,xmm1
	movq		xmm1,[r2+r7]	;load 64 bits
	punpcklbw	xmm2,xmm1,xmm0	;unpack to 16 bits
	psllw		xmm2,6			;shift left 6 bits (14 - bit depth) each 16bit element
	movdqu		[r0+2*r7],xmm2	;store 128 bits
	add			r7,8			;add 8 for width loop
	cmp			r7, r4			;cmp width
	jl			mc_pixels_w		;width loop
	lea			r0,[r0+2*r1]	;dst += dststride
	lea			r2,[r2+r3]		;src += srcstride
	add			r6,1
	cmp			r6,r5			;cmp height
	jl			mc_pixels_h		;height loop
    RET
