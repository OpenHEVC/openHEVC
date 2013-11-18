; /*
; * Provide SSE luma mc functions for HEVC decoding
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


SECTION .text


INIT_XMM sse4        ; adds ff_ and _sse4 to function name

; ******************************
; void put_hevc_mc_pixels_8(int16_t *dst, ptrdiff_t dststride,
;                                       uint8_t *_src, ptrdiff_t _srcstride,
;                                       int width, int height, int mx, int my,
;                                       int16_t* mcbuffer)
;
;        r0 : *dst
;        r1 : dststride
;        r2 : *src
;         r3 : srcstride
;        r4 : width
;        r5 : height
;
; ******************************
; 1 by 1. Can be done on any processor
cglobal put_hevc_mc_pixels_2_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
                  mov          r6,0                        ; height
mc_pixels_2_h:        ; for height
                  mov          r7, 0                       ; width

mc_pixels_2_w:        ; for width
                  mov          r9,0
                  mov          r9b,[srcq+r7]               ; get byte
                  shl          r9,6                        ; shift
                  mov          [dstq+2*r7],r9w             ; store
                  inc          r7
                  cmp          r7, widthq                  ; cmp width
                  jl           mc_pixels_2_w               ; width loop
                  lea          dstq,[dstq+2*dststrideq]    ; dst += dststride
                  lea          srcq,[srcq+srcstrideq]      ; src += srcstride
                  add          r6,1
                  cmp          r6,heightq                  ; cmp height
                  jl           mc_pixels_2_h               ; height loop
                  RET
; 4 by 4
cglobal put_hevc_mc_pixels_4_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
                  pxor         m0,m0                       ; set register at zero
                  mov          r6,0                        ; height
                  mov          r9,0

        ; 8 by 8
mc_pixels_4_h:        ; for height
                  mov          r7,0                        ; width

mc_pixels_4_w:        ; for width
                  pxor         m1,m1
                  movq         m1,[srcq+r7]                  ; load 64 bits
                  punpcklbw    m2,m1,m0                    ; unpack to 16 bits
                  psllw        m2,6                        ; shift left 6 bits (14 - bit depth) each 16bit element
                  movq         [dstq+2*r7],m2                ; store 64 bits
                  add          r7,4                        ; add 4 for width loop
                  cmp          r7, widthq                  ; cmp width
                  jl           mc_pixels_4_w               ; width loop
                  lea          dstq,[dstq+2*dststrideq]    ; dst += dststride
                  lea          srcq,[srcq+srcstrideq]      ; src += srcstride
                  add          r6,1
                  cmp          r6,heightq                  ; cmp height
                  jl           mc_pixels_4_h               ; height loop
                  RET

; 8 by 8
cglobal put_hevc_mc_pixels_8_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
                  pxor         m0,m0                   ; set register at zero
                  mov          r6,0                        ; height
                  mov          r9,0

        ; 8 by 8
mc_pixels_8_h:        ; for height
                  mov          r7,0                        ; width

mc_pixels_8_w:        ; for width

                  pxor         m1,m1
                  movq         m1,[srcq+r7]                ; load 64 bits
                  punpcklbw    m2,m1,m0              ; unpack to 16 bits
                  psllw        m2,6                      ; shift left 6 bits (14 - bit depth) each 16bit element
                  movdqu       [dstq+2*r7],m2              ; store 128 bits
                  add          r7,8                        ; add 8 for width loop
                  cmp          r7, r4                      ; cmp width
                  cmp          r7, widthq                  ; cmp width
                  jl           mc_pixels_8_w               ; width loop
                  lea          dstq,[dstq+2*dststrideq]    ; dst += dststride
                  lea          srcq,[srcq+srcstrideq]      ; src += srcstride
                  add          r6,1
                  cmp          r6,heightq                  ; cmp height
                  jl           mc_pixels_8_h               ; height loop
                  RET

; 16 by 16
cglobal put_hevc_mc_pixels_16_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
                  pxor         m0,m0                   ; set register at zero
                  mov          r6,0                        ; height
                  mov          r9,0

        ; 8 by 8
mc_pixels_16_h:        ; for height
                  mov          r7,0                        ; width

mc_pixels_16_w:        ; for width

                  pxor         m1,m1
                  movdqu       m1,[srcq+r7]                ; load 128 bits
                  punpcklbw    m2,m1,m0              ; unpack low to 16 bits
                  punpckhbw    m3,m1,m0              ; unpack high to 16 bits
                  psllw        m2,6                      ; shift left 6 bits (14 - bit depth) each 16bit element
                  psllw        m3,6                      ; shift left 6 bits (14 - bit depth) each 16bit element
                  movdqu       [dstq+2*r7],m2              ; store 128 bits
                  movdqu       [dstq+2*r7+16],m3           ; store 128 bits
                  add          r7,16                       ; add 8 for width loop
                  cmp          r7, widthq                  ; cmp width
                  jl           mc_pixels_16_w               ; width loop
                  lea          dstq,[dstq+2*dststrideq]    ; dst += dststride
                  lea          srcq,[srcq+srcstrideq]      ; src += srcstride
                  add          r6,1
                  cmp          r6,heightq                  ; cmp height
                  jl           mc_pixels_16_h               ; height loop
                  RET

; ******************************
; void put_hevc_epel_h_8(int16_t *dst, ptrdiff_t dststride,
;                                       uint8_t *_src, ptrdiff_t _srcstride,
;                                       int width, int height, int mx, int my,
;                                       int16_t* mcbuffer)
;
;        r0 : *dst
;        r1 : dststride
;        r2 : *src
;        r3 : srcstride
;        r4 : width
;        r5 : height
;
; ******************************
cglobal put_hevc_epel_h_4_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my


RET
