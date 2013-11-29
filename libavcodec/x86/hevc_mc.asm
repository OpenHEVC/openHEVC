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
cextern hevc_epel_filters

epel_extra_before   DB 1    ;corresponds to EPEL_EXTRA_BEFORE in hevc.h
max_pb_size         DB 64   ;corresponds to MAX_PB_SIZE in hevc.h

epel_h_shuffle1     DB 0
                    DB 1
                    DB 2
                    DB 3
                    DB 1
                    DB 2
                    DB 3
                    DB 4
                    DB 2
                    DB 3
                    DB 4
                    DB 5
                    DB 3
                    DB 4
                    DB 5
                    DB 6

epel_h_shuffle2     DB 4
                    DB 5
                    DB 6
                    DB 7
                    DB 5
                    DB 6
                    DB 7
                    DB 8
                    DB 6
                    DB 7
                    DB 8
                    DB 9
                    DB 7
                    DB 8
                    DB 9
                    DB 10

SECTION .text

%if ARCH_X86_64
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
cglobal put_hevc_mc_pixels_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
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
                  movq         m1,[srcq+r7]                ; load 64 bits
                  punpcklbw    m2,m1,m0                    ; unpack to 16 bits
                  psllw        m2,6                        ; shift left 6 bits (14 - bit depth) each 16bit element
                  movq         [dstq+2*r7],m2              ; store 64 bits
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
                  pxor         m0,m0                       ; set register at zero
                  mov          r6,0                        ; height
                  mov          r9,0

        ; 8 by 8
mc_pixels_8_h:        ; for height
                  mov          r7,0                        ; width

mc_pixels_8_w:        ; for width

                  pxor         m1,m1
                  movq         m1,[srcq+r7]                ; load 64 bits
                  punpcklbw    m2,m1,m0                    ; unpack to 16 bits
                  psllw        m2,6                        ; shift left 6 bits (14 - bit depth) each 16bit element
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
                  punpcklbw    m2,m1,m0                    ; unpack low to 16 bits
                  punpckhbw    m3,m1,m0                    ; unpack high to 16 bits
                  psllw        m2,6                        ; shift left 6 bits (14 - bit depth) each 16bit element
                  psllw        m3,6                        ; shift left 6 bits (14 - bit depth) each 16bit element
                  movdqu       [dstq+2*r7],m2              ; store 128 bits
                  movdqu       [dstq+2*r7+16],m3           ; store 128 bits
                  add          r7,16                       ; add 16 for width loop
                  cmp          r7, widthq                  ; cmp width
                  jl           mc_pixels_16_w              ; width loop
                  lea          dstq,[dstq+2*dststrideq]    ; dst += dststride
                  lea          srcq,[srcq+srcstrideq]      ; src += srcstride
                  add          r6,1
                  cmp          r6,heightq                  ; cmp height
                  jl           mc_pixels_16_h              ; height loop
                  RET

;function to call other mc_pixels functions according to width value
cglobal put_hevc_mc_pixels_master_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
cmp  r4,8
je  goto_mc_pixels_8_8
cmp  r4,16
je  goto_mc_pixels_16_8
cmp  r4,4
je  goto_mc_pixels_4_8
cmp  r4,32
je  goto_mc_pixels_16_8
cmp  r4,12
je  goto_mc_pixels_4_8
cmp  r4,24
je  goto_mc_pixels_8_8
cmp  r4,6
je  goto_mc_pixels_2_8
cmp  r4,64
je  goto_mc_pixels_16_8
cmp  r4,2
je  goto_mc_pixels_2_8

jmp  goto_mc_pixels_2_8

goto_mc_pixels_2_8:
call put_hevc_mc_pixels_2_8
RET

goto_mc_pixels_4_8:
call put_hevc_mc_pixels_4_8
RET

goto_mc_pixels_8_8:
call put_hevc_mc_pixels_8_8
RET

goto_mc_pixels_16_8:
call put_hevc_mc_pixels_16_8
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

;8 by 8
cglobal put_hevc_epel_h_8_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my
    movdqu  m4,[epel_h_shuffle2]
epel_h_8_h:
    mov r8,0                            ; width counter
epel_h_8_w:
    movdqu   m2,[srcq+r8-1]             ; load data from source
    pshufb  m5,m4,m0                    ; shuffle2
    pshufb  m2,m2,m0                    ; shuffle
    pmaddubsw   m2,m1                   ; maddubs (see SSE instruction set for details)
    pmaddubsw   m5,m1                   ; maddubs (see SSE instruction set for details)
    phaddw  m2,m5                       ; horizontal add
    movdqu    [dstq+2*r8],m2              ; store data to dst
    add     r8,4
    cmp     r8,widthq                   ;
    jl      epel_h_4_w
    lea     dstq,[dstq+2*dststrideq]    ; dst += dststride
    lea     srcq,[srcq+srcstrideq]      ; src += srcstride
    add     r9,1
    cmp     r9,heightq                  ; cmp height
    jl      epel_h_4_h                  ; height loop

RET

;4 by 4
cglobal put_hevc_epel_h_4_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my

epel_h_4_h:
    mov r8,0                            ; width counter
epel_h_4_w:
    movdqu   m2,[srcq+r8-1]             ; load data from source
    pshufb  m2,m2,m0                    ; shuffle
    pmaddubsw   m2,m1                   ; maddubs (see SSE instruction set for details)
    phaddw  m2,m3                       ; horizontal add
    movq    [dstq+2*r8],m2              ; store data to dst
    add     r8,4
    cmp     r8,widthq                   ;
    jl      epel_h_8_w
    lea     dstq,[dstq+2*dststrideq]    ; dst += dststride
    lea     srcq,[srcq+srcstrideq]      ; src += srcstride
    add     r9,1
    cmp     r9,heightq                  ; cmp height
    jl      epel_h_8_h                  ; height loop

RET

cglobal put_hevc_epel_h_master_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my

    movsxd   mxq,mxd                    ; extend sign
    pxor     m3,m3                      ; set zero

    lea     r10,[hevc_epel_filters]
    sub     mxq,1
    shl     mxq,4                       ; multiply by 16
    movq    m0,[r10+mxq]                ; get 4 first values of filters
    pshufd  m1,m0,0                     ; cast 32 bit on all register.
    movdqu   m0,[epel_h_shuffle1]       ; register containing shuffle
    mov     r9,0                        ; height counter

cmp     widthq,2
je      goto_epel_h_4_8
cmp     widthq,4
je      goto_epel_h_4_8
cmp     widthq,6
je      goto_epel_h_4_8
cmp     widthq,8
je      goto_epel_h_8_8
cmp     widthq,12
je      goto_epel_h_4_8
cmp     widthq,16
je      goto_epel_h_8_8
cmp     widthq,24
je      goto_epel_h_8_8
cmp     widthq,32
je      goto_epel_h_8_8
cmp     widthq,64
je      goto_epel_h_8_8

jmp     goto_epel_h_4_8

RET
goto_epel_h_4_8:
call put_hevc_epel_h_4_8
RET

goto_epel_h_8_8:
call put_hevc_epel_h_8_8
RET

;TODO : epel_h_2

; ******************************
; void put_hevc_epel_v_8(int16_t *dst, ptrdiff_t dststride,
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

cglobal put_hevc_epel_v_16_8, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
;filters and initialization is done in master
epel_v_16_h:
    mov r9,0                                ;set width counter
epel_v_16_v:
    lea  r10,[srcq+r9]
    mov  r11,r10
    sub  r11,srcstrideq
    movdqu m5,[r11]           ;load 64bit of x-stride
    movdqu m6,[r10]                       ;load 64bit of x
    movdqu m7,[r10+srcstrideq]            ;load 64bit of x+stride
    movdqu m8,[r10+2*srcstrideq]          ;load 64bit of x+2*stride

    punpckhbw m9,m5,m0                         ;unpack 8bit to 16bit
    punpckhbw m10,m6,m0                         ;unpack 8bit to 16bit
    punpckhbw m11,m7,m0                         ;unpack 8bit to 16bit
    punpckhbw m12,m8,m0                         ;unpack 8bit to 16bit

    punpcklbw m5,m0                         ;unpack 8bit to 16bit
    punpcklbw m6,m0                         ;unpack 8bit to 16bit
    punpcklbw m7,m0                         ;unpack 8bit to 16bit
    punpcklbw m8,m0                         ;unpack 8bit to 16bit


    pmullw  m9,m1                           ;multiply values with filter
    pmullw  m10,m2
    pmullw  m11,m3
    pmullw  m12,m4

    pmullw  m5,m1                           ;multiply values with filter
    pmullw  m6,m2
    pmullw  m7,m3
    pmullw  m8,m4

    paddsw  m9,m10                           ;add the different values
    paddsw  m11,m12
    paddsw  m9,m11

    paddsw  m5,m6                           ;add the different values
    paddsw  m7,m8
    paddsw  m5,m7

    movdqu    [dstq+2*r9],m5              ;store 128bit to dst
    movdqu    [dstq+2*r9+16],m9           ;store 128bit to dst

    add     r9,16                        ; add 16 for width loop
    cmp     r9, widthq                  ; cmp width
    jl      epel_v_16_v                  ; width loop
    lea     dstq,[dstq+2*dststrideq]    ; dst += dststride
    lea     srcq,[srcq+srcstrideq]      ; src += srcstride
    add     r8,1
    cmp     r8,heightq                  ; cmp height
    jl      epel_v_16_h                  ; height loop

RET


cglobal put_hevc_epel_v_8_8, 8, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my
;filters and initialization is done in master
epel_v_8_h:
    mov r9,0                                ;set width counter
epel_v_8_v:
    lea  r10,[srcq+r9]
    mov  r11,r10
    sub  r11,srcstrideq
    movq m5,[r11]           ;load 64bit of x-stride
    movq m6,[r10]                       ;load 64bit of x
    movq m7,[r10+srcstrideq]            ;load 64bit of x+stride
    movq m8,[r10+2*srcstrideq]          ;load 64bit of x+2*stride

    punpcklbw m5,m0                         ;unpack 8bit to 16bit
    punpcklbw m6,m0                         ;unpack 8bit to 16bit
    punpcklbw m7,m0                         ;unpack 8bit to 16bit
    punpcklbw m8,m0                         ;unpack 8bit to 16bit

    pmullw  m5,m1                           ;multiply values with filter
    pmullw  m6,m2
    pmullw  m7,m3
    pmullw  m8,m4

    paddsw  m5,m6                           ;add the different values
    paddsw  m7,m8
    paddsw  m5,m7

    movdqu    [dstq+2*r9],m5              ;store 64bit to dst
    add     r9,8                        ; add 4 for width loop
    cmp     r9, widthq                  ; cmp width
    jl      epel_v_8_v                  ; width loop
    lea     dstq,[dstq+2*dststrideq]    ; dst += dststride
    lea     srcq,[srcq+srcstrideq]      ; src += srcstride
    add     r8,1
    cmp     r8,heightq                  ; cmp height
    jl      epel_v_8_h                  ; height loop

RET

cglobal put_hevc_epel_v_4_8, 8, 12, 0, dst, dststride, src, srcstride, width, height, mx, my

epel_v_4_h:
    mov r9,0                             ;set width counter
epel_v_4_v:
    lea     r10,[srcq+r9]
    mov     r11,r10
    sub     r11,srcstrideq
    movq m5,[r11]                        ;load 64bit of x-stride
    movq m6,[r10]                        ;load 64bit of x
    movq m7,[r10+srcstrideq]             ;load 64bit of x+stride
    movq m8,[r10+2*srcstrideq]           ;load 64bit of x+2*stride

    punpcklbw m5,m0                      ;unpack 8bit to 16bit
    punpcklbw m6,m0                      ;unpack 8bit to 16bit
    punpcklbw m7,m0                      ;unpack 8bit to 16bit
    punpcklbw m8,m0                      ;unpack 8bit to 16bit

    pmullw    m5,m1                      ;multiply values with filter
    pmullw    m6,m2
    pmullw    m7,m3
    pmullw    m8,m4

    paddsw    m5,m6                      ;add the different values
    paddsw    m7,m8
    paddsw    m5,m7

    movq    [dstq+2*r9],m5               ;store 64bit to dst
    add     r9,4                         ; add 4 for width loop
    cmp     r9, widthq                   ; cmp width
    jl      epel_v_4_v                   ; width loop
    lea     dstq,[dstq+2*dststrideq]     ; dst += dststride
    lea     srcq,[srcq+srcstrideq]       ; src += srcstride
    add     r8,1
    cmp     r8,heightq                   ; cmp height
    jl      epel_v_4_h                   ; height loop

RET

;function to call other epel_v functions according to width value
cglobal put_hevc_epel_v_master_8, 8, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my

    pxor    m1,m1                               ;set zero
    movsxd   myq,myd                           ;extend sign
    sub    myq,1                                ;my-1
    shl    myq,4                                ;multiply by 16
    lea     r10,[hevc_epel_filters]
    movq    m0,[r10+myq]                ;filters

    punpcklbw   m1,m0                   ;unpack to 16 bit
    psraw       m1,8                    ;shift for bit-sign
    punpcklwd   m1,m1                   ;put double values to 32bit.
    psrldq      m2,m1,4                 ;filter 1
    psrldq      m3,m1,8                 ;filter 2
    psrldq      m4,m1,12                ;filter 3
    pshufd      m1,m1,0                 ;extend 32bit to whole register
    pshufd      m2,m2,0
    pshufd      m3,m3,0
    pshufd      m4,m4,0

    pxor m0,m0                          ;set zero

    mov r8,0                                ;set height counter


cmp  r4,8
je  goto_epel_v_8_8
cmp  r4,16
je  goto_epel_v_16_8
cmp  r4,4
je  goto_epel_v_4_8
cmp  r4,32
je  goto_epel_v_16_8
cmp  r4,12
je  goto_epel_v_4_8
cmp  r4,24
je  goto_epel_v_8_8
cmp  r4,6
je  goto_epel_v_2_8
cmp  r4,64
je  goto_epel_v_16_8
cmp  r4,2
je  goto_epel_v_2_8

jmp  goto_epel_v_2_8
RET

goto_epel_v_4_8:
call put_hevc_epel_v_4_8
RET

goto_epel_v_8_8:
call put_hevc_epel_v_8_8
RET

goto_epel_v_16_8:
call put_hevc_epel_v_16_8
RET

goto_epel_v_2_8:
call put_hevc_epel_v_4_8
RET

;TODO : put_hevc_epel_v_2_8.


; ******************************
; void put_hevc_epel_hv_8(int16_t *dst, ptrdiff_t dststride,
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
;        r6 : mx
;        r7 : my
;        r8 : mcbuffer
;
; ******************************

cglobal put_hevc_epel_hv_4_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my, mcbuffer

    movsxd   mxq,mxd                    ; extend sign
    movsxd   myq,myd                    ; extend sign
    movsxd   srcstrideq,srcstrided      ; extend sign
    movsxd   dststrideq,dststrided      ; extend sign
    movsxd   widthq,widthd              ; extend sign
    movsxd   heightq,heightd            ; extend sign

    pxor     m3,m3                      ; set zero

    lea     r10,[hevc_epel_filters]
    sub     mxq,1
    shl     mxq,4                       ; multiply by 16
    movq    m0,[r10+mxq]                ; get 4 first values of filters
    pshufd  m1,m0,0                     ; cast 32 bit on all register.
    movdqu  m0,[epel_h_shuffle1]        ; register containing shuffle1

    mov     r15, srcstrideq
;    imul    r15, epel_extra_before
;    sub     srcq,srcstrideq                    ; src -= EPEL_EXTRA_BEFORE * srcstride
;    mov     r14,max_pb_size
    shl     r14,2                       ;double because it's used for 16bit adressing
    lea     r13,[mcbufferq]

    mov     r9,0                        ; height counter

epel_hv_4_h_h:
    xor r10,r10                            ; width counter
epel_hv_4_h_w:
    movdqu   m2,[srcq+r10]            ; load data from source
    pshufb  m2,m2,m0                    ; shuffle
    pmaddubsw   m2,m1                   ; maddubs (see SSE instruction set for details)
    phaddw  m2,m3                       ; horizontal add
   movq    [r13+2*r10],m2            ; store data to dst
    add     r10,4
    cmp     r10,widthq                  ;
    jl      epel_hv_4_h_w
    lea     dstq,[r13+r14]              ; dst += dststride
    lea     srcq,[srcq+srcstrideq]      ; src += srcstride
    add     r9,1
    cmp     r9,heightq                  ; cmp height
    jl      epel_hv_4_h_h                  ; height loop

;end H treatment
;    imul    r14,r15;
    add     mcbufferq,r14               ; mcbuffer+= EPEL_EXTRA_BEFORE * MAX_PB_SIZE


    pxor    m1,m1                               ;set zero
    movsxd   myq,myd                           ;extend sign
    sub    myq,1                                ;my-1
    shl    myq,4                                ;multiply by 16
    lea     r10,[hevc_epel_filters]
    movq    m0,[r10+myq]                ;filters

    punpcklbw   m1,m0                   ;unpack to 16 bit
    psraw       m1,8                    ;shift for bit-sign
    punpcklwd   m1,m1                   ;put double values to 32bit.
    psrldq      m2,m1,4                 ;filter 1
    psrldq      m3,m1,8                 ;filter 2
    psrldq      m4,m1,12                ;filter 3
    pshufd      m1,m1,0                 ;extend 32bit to whole register
    pshufd      m2,m2,0
    pshufd      m3,m3,0
    pshufd      m4,m4,0

    pxor m0,m0                          ;set zero
    mov r8,0                            ;set height counter

epel_hv_4_v_h:
    mov r9,0                            ;set width counter
epel_hv_4_v_w:
    lea  r10,[mcbufferq+r9]
    mov  r11,r10
    sub  r11,srcstrideq
;    movq m5,[r11]                       ;load 64bit of x-stride
;    movq m6,[r10]                       ;load 64bit of x
;    movq m7,[r10+srcstrideq]            ;load 64bit of x+stride
;    movq m8,[r10+2*srcstrideq]          ;load 64bit of x+2*stride


    punpcklbw   m5,m0                   ;unpack to 16 bit
    psraw       m5,16                   ;shift for bit-sign
    punpcklbw   m6,m0                   ;unpack to 16 bit
    psraw       m6,16                   ;shift for bit-sign
    punpcklbw   m7,m0                   ;unpack to 16 bit
    psraw       m7,16                   ;shift for bit-sign
    punpcklbw   m8,m0                   ;unpack to 16 bit
    psraw       m8,16                   ;shift for bit-sign


    pmulld    m5,m1                      ;multiply values with filter - SSE4.2 function
    pmulld    m6,m2
    pmulld    m7,m3
    pmulld    m8,m4

    paddd    m5,m6                      ;add the different values
    paddd    m7,m8
    paddd    m5,m7

    psrld   m5,2                        ; >> BIT_DEPTH-2
    packssdw    m5,m0                   ;back to 16bit
    pxor    m5,m5

;    movdqu    [dstq+2*r9],m5              ;store 128bit to dst
    add     r9,4                        ; add 4 for width loop
    cmp     r9, widthq                  ; cmp width
    jl      epel_hv_4_v_w                  ; width loop
    lea     dstq,[dstq+2*dststrideq]    ; dst += dststride
    lea     srcq,[mcbufferq+r15]      ; src += srcstride
    add     r8,1
    cmp     r8,heightq                  ; cmp height
    jl      epel_hv_4_v_h                  ; height loop

RET


%endif ; ARCH_X86_64
