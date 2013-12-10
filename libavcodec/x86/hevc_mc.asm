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

epel_h_shuffle1_8   DB  0,  1,  2,  3
                    DB  1,  2,  3,  4
                    DB  2,  3,  4,  5
                    DB  3,  4,  5,  6

epel_h_shuffle1_10  DB  0,  1,  2,  3,  4,  5,  6,  7
                    DB  2,  3,  4,  5,  6,  7,  8,  9

epel_h_shuffle2_10  DB  4,  5,  6,  7,  8,  9, 10, 11
                    DB  6,  7,  8,  9, 10, 11, 12, 13

SECTION .text

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro LOOP_INIT 2
    pxor             m15, m15                    ; set register at zero
    mov              r10, 0                      ; set height counter
%1:
    mov               r9, 0                      ; set width counter
%2:
%endmacro

%macro LOOP_END 7
    add               r9, %3                     ; add 4 for width loop
    cmp               r9, widthq                 ; cmp width
    jl                %2                         ; width loop
%ifidn %5, dststride
    lea              %4q, [%4q+2*%5q]           ; dst += dststride
%else
    lea              %4q, [%4q+  %5 ]           ; dst += dststride
%endif
%ifidn %7, srcstride
    lea              %6q, [%6q+  %7q]           ; src += srcstride
%else
    lea              %6q, [%6q+  %7 ]           ; src += srcstride
%endif
    add              r10, 1
    cmp              r10, heightq                ; cmp height
    jl                %1                         ; height loop
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro EPEL_H_FILTER 1
    movsxd           mxq, mxd                    ; extend sign
    sub              mxq, 1
    shl              mxq, 4                      ; multiply by 16
    lea              r11, [hevc_epel_filters]
%if %1 == 8
    movq             m13, [r11+mxq]              ; get 4 first values of filters
    pshufd           m13, m13, 0                 ; cast 32 bit on all register.
    movdqu           m14, [epel_h_shuffle1_8]    ; register containing shuffle
%else
    movq             m15, [r11+mxq]              ; get 4 first values of filters
    pxor             m13, m13
    punpcklbw        m13, m15
    psraw            m13, 8
    punpcklqdq       m13, m13
    movdqu           m14, [epel_h_shuffle1_10]   ; register containing shuffle
    movdqu           m11, [epel_h_shuffle2_10]   ; register containing shuffle
%endif
%endmacro

%macro EPEL_V_FILTER 1
    pxor             m10, m10
    movsxd           myq, myd                    ; extend sign
    sub              myq, 1                      ; my-1
    shl              myq, 4                      ; multiply by 16
    lea              r11, [hevc_epel_filters]
    movq             m12, [r11+myq]              ; filters
    punpcklbw        m10, m12                    ; unpack to 16 bit
    psraw            m10, 8                      ; shift for bit-sign
%if %1 == 8
    punpcklwd        m10, m10                    ; put double values to 32bit.
%else
    pxor             m15, m15
    punpcklwd        m15, m10
    psrad            m10, m15, 16
%endif
    psrldq           m11, m10, 4                 ; filter 1
    psrldq           m12, m10, 8                 ; filter 2
    psrldq           m13, m10, 12                ; filter 3
    pshufd           m10, m10, 0                 ; extend 32bit to whole register
    pshufd           m11, m11, 0
    pshufd           m12, m12, 0
    pshufd           m13, m13, 0
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro INST_SRC1_CST_2 4
    p%1               %2, m0, %4
    p%1               %3, m1, %4
%endmacro
%macro INST_SRC1_CST_4 6
    INST_SRC1_CST_2   %1, %2, %3, %6
    p%1               %4, m2, %6
    p%1               %5, m3, %6
%endmacro
;%macro INST_SRC1_CST_8 10
;    INST_SRC1_CST_4   %1, %2, %3, %4, %5, %10
;    p%1               %6, m4, %10
;    p%1               %7, m5, %10
;    p%1               %8, m6, %10
;    p%1               %9, m7, %10
;%endmacro

%macro MUL_ADD_H_1 2
    p%1               m0, m13
    p%2              m12, m0, m15
%endmacro
%macro MUL_ADD_H_2_2 2
    p%1               m0, m13
    p%1               m1, m13
    p%2              m12, m0, m1
%endmacro

%macro MUL_ADD_V_4 6
    p%1               %3, m10
    p%1               %4, m11
    p%2               %3, %4
    p%1               %5, m12
    p%2               %3, %5
    p%1               %6, m13
    p%2               %3, %6
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro MC_LOAD_PIXEL 1
%if %1 == 8
    movdqu            m0, [srcq+  r9]            ; load data from source
%else
    movdqu            m0, [srcq+2*r9]            ; load data from source
%endif
%endmacro

%macro EPEL_H_LOAD2 1
%if %1 == 8
    movdqu            m0, [srcq+  r9-1]          ; load data from source
%else
    movdqu            m0, [srcq+2*r9-2]          ; load data from source
%endif
%endmacro
%define EPEL_H_LOAD4 EPEL_H_LOAD2
%macro EPEL_H_LOAD8 1
    EPEL_H_LOAD2      %1
    psrldq            m1, m0, 4
%endmacro

%macro EPEL_V_LOAD 3
%if %1 == 8
    lea              r11, [%2q+  r9]
%else
    lea              r11, [%2q+2*r9]
%endif
    movdqu            m1, [r11      ]            ;load 64bit of x
%ifidn %3, srcstride
    movdqu            m3, [r11+2*%3q]            ;load 64bit of x+2*stride
    sub              r11, %3q
    movdqu            m2, [r11+2*%3q]            ;load 64bit of x+stride
%else
    movdqu            m3, [r11+2*%3 ]            ;load 64bit of x+2*stride
    sub              r11, %3
    movdqu            m2, [r11+2*%3 ]            ;load 64bit of x+stride
%endif
    movdqu            m0, [r11      ]            ;load 64bit of x-stride
%endmacro

%macro PEL_STORE2 3
    movss     [%1q+2*r9], %2
%endmacro
%macro PEL_STORE4 3
    movq      [%1q+2*r9],%2
%endmacro
%macro PEL_STORE8 3
    movdqu    [%1q+2*r9],%2
%endmacro
%macro PEL_STORE16 3
    PEL_STORE8        %1, %2, %3
    movdqu [%1q+2*r9+16], %3
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro UNPACK_SRAI16_4 5
    p%1              m15, m0
    psrad             %2, m15, 16
    pxor             m15, m15
    p%1              m15, m1
    psrad             %3, m15, 16
    pxor             m15, m15
    p%1              m15, m2
    psrad             %4, m15, 16
    pxor             m15, m15
    p%1              m15, m3
    psrad             %5, m15, 16
    pxor             m15, m15
%endmacro

%if ARCH_X86_64
INIT_XMM sse4                                    ; adds ff_ and _sse4 to function name

; ******************************
; void put_hevc_mc_pixels(int16_t *dst, ptrdiff_t dststride,
;                         uint8_t *_src, ptrdiff_t _srcstride,
;                         int width, int height, int mx, int my,
;                         int16_t* mcbuffer)
;
;        r0 : *dst
;        r1 : dststride
;        r2 : *src
;        r3 : srcstride
;        r4 : width
;        r5 : height
;
; ******************************
%macro MC_PIXEL_COMPUTE2_8 0
    punpcklbw         m1, m0, m15
    psllw             m1, 6
%endmacro
%macro MC_PIXEL_COMPUTE4_8 0
    MC_PIXEL_COMPUTE2_8
%endmacro
%macro MC_PIXEL_COMPUTE8_8 0
    MC_PIXEL_COMPUTE2_8
%endmacro
%macro MC_PIXEL_COMPUTE16_8 0
    MC_PIXEL_COMPUTE2_8
    punpckhbw         m2, m0, m15
    psllw             m2, 6
%endmacro

%macro MC_PIXEL_COMPUTE2_10 0
    psllw             m1, m0, 4
%endmacro
%define MC_PIXEL_COMPUTE4_10 MC_PIXEL_COMPUTE2_10
%define MC_PIXEL_COMPUTE8_10 MC_PIXEL_COMPUTE2_10

%macro PUT_HEVC_MC_PIXELS 3
    LOOP_INIT %3_pixels_h_%1_%2, %3_pixels_v_%1_%2
    MC_LOAD_PIXEL     %2
    MC_PIXEL_COMPUTE%1_%2
    PEL_STORE%1      dst, m1, m2
    LOOP_END %3_pixels_h_%1_%2, %3_pixels_v_%1_%2, %1, dst, dststride, src, srcstride
    RET
%endmacro

; ******************************
; void put_hevc_epel_h(int16_t *dst, ptrdiff_t dststride,
;                     uint8_t *_src, ptrdiff_t _srcstride,
;                     int width, int height, int mx, int my,
;                     int16_t* mcbuffer)
;
;      r0 : *dst
;      r1 : dststride
;      r2 : *src
;      r3 : srcstride
;      r4 : width
;      r5 : height
;
; ******************************
%macro EPEL_H_COMPUTE2_8 0
    pshufb            m0, m0, m14                ; shuffle
    MUL_ADD_H_1 maddubsw, haddw
%endmacro
%define EPEL_H_COMPUTE4_8 EPEL_H_COMPUTE2_8
%macro EPEL_H_COMPUTE8_8 0
    INST_SRC1_CST_2  shufb, m0, m1, m14
    MUL_ADD_H_2_2 maddubsw, haddw
%endmacro

%macro EPEL_H_COMPUTE2_10 0
    pshufb             m0, m0, m14                    ; shuffle
    MUL_ADD_H_1    maddwd, haddd
    psrad             m12, 2
    packssdw          m12, m15
%endmacro
%macro EPEL_H_COMPUTE4_10 0
    pshufb            m1, m0, m11               ; shuffle2
    pshufb            m0, m0, m14               ; shuffle1
    MUL_ADD_H_2_2 maddwd, haddd
    psrad            m12, 2
    packssdw         m12, m15
%endmacro

%macro PUT_HEVC_EPEL_H 2
    EPEL_H_FILTER     %2
    LOOP_INIT  epel_h_h_%1_%2, epel_h_w_%1_%2
    EPEL_H_LOAD%1     %2
    EPEL_H_COMPUTE%1_%2
    PEL_STORE%1       dst, m12, m15
    LOOP_END   epel_h_h_%1_%2, epel_h_w_%1_%2, %1, dst, dststride, src, srcstride
%endmacro

; ******************************
; void put_hevc_epel_v(int16_t *dst, ptrdiff_t dststride,
;                      uint8_t *_src, ptrdiff_t _srcstride,
;                      int width, int height, int mx, int my,
;                      int16_t* mcbuffer)
;
;      r0 : *dst
;      r1 : dststride
;      r2 : *src
;      r3 : srcstride
;      r4 : width
;      r5 : height
;
; ******************************
%macro EPEL_V_COMPUTE2_8 1
    INST_SRC1_CST_4    unpcklbw, m0, m1, m2, m3, m15
    MUL_ADD_V_4    mullw, addsw, m0, m1, m2, m3
%endmacro
%define EPEL_V_COMPUTE4_8 EPEL_V_COMPUTE2_8
%define EPEL_V_COMPUTE8_8 EPEL_V_COMPUTE2_8
%macro EPEL_V_COMPUTE16_8 1
    INST_SRC1_CST_4    unpckhbw, m4, m5, m6, m7, m15
    MUL_ADD_V_4    mullw, addsw, m4, m5, m6, m7
    EPEL_V_COMPUTE2_8 %1
%endmacro

%macro EPEL_V_COMPUTE2_10 1
    UNPACK_SRAI16_4   unpcklwd, m0, m1, m2, m3
    MUL_ADD_V_4    mulld, addd, m0, m1, m2, m3
    psrad             m0, %1
    packssdw          m0, m15
%endmacro
%define EPEL_V_COMPUTE4_10 EPEL_V_COMPUTE2_10
%macro EPEL_V_COMPUTE8_10 1
    UNPACK_SRAI16_4   unpckhwd, m4, m5, m6, m7
    MUL_ADD_V_4    mulld, addd, m4, m5, m6, m7
    UNPACK_SRAI16_4   unpcklwd, m0, m1, m2, m3
    MUL_ADD_V_4    mulld, addd, m0, m1, m2, m3
    psrad             m0, %1
    psrad             m4, %1
    packssdw          m0, m4
%endmacro

%macro PUT_HEVC_EPEL_V 2
    EPEL_V_FILTER     %2
    LOOP_INIT epel_v_h_%1_%2, epel_v_w_%1_%2
    EPEL_V_LOAD       %2, src, srcstride
    EPEL_V_COMPUTE%1_%2 2
    PEL_STORE%1      dst, m0, m4
    LOOP_END  epel_v_h_%1_%2, epel_v_w_%1_%2, %1, dst, dststride, src, srcstride
%endmacro

; ******************************
; void put_hevc_epel_hv(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int mx, int my,
;                       int16_t* mcbuffer)
;
;      r0 : *dst
;      r1 : dststride
;      r2 : *src
;      r3 : srcstride
;      r4 : width
;      r5 : height
;      r6 : mx
;      r7 : my
;      r8 : mcbuffer
;
; ******************************
%macro PUT_HEVC_EPEL_HV 2
    EPEL_H_FILTER     %2
    sub             srcq, srcstrideq             ; src -= srcstride
    lea              r6q, [mcbufferq]
    add          heightq, 3

    LOOP_INIT  epel_hv_h_h_%1_%2, epel_hv_h_w_%1_%2
    EPEL_H_LOAD%1     %2
    EPEL_H_COMPUTE%1_%2
    PEL_STORE%1       r6, m12, m15
    LOOP_END   epel_hv_h_h_%1_%2, epel_hv_h_w_%1_%2, %1, r6, 128, src, srcstride

    lea        mcbufferq, [mcbufferq+128]        ; mcbufferq += EPEL_EXTRA_BEFORE * MAX_PB_SIZE
    sub          heightq, 3

    EPEL_V_FILTER     10
    LOOP_INIT epel_hv_v_h_%1_%2, epel_hv_v_w_%1_%2
    EPEL_V_LOAD       10, mcbuffer, 128
    EPEL_V_COMPUTE%1_10 6
    PEL_STORE%1      dst, m0, m4
    LOOP_END  epel_hv_v_h_%1_%2, epel_hv_v_w_%1_%2, %1, dst, dststride, mcbuffer, 128

%endmacro


; ******************************
; void put_hevc_mc_pixels(int16_t *dst, ptrdiff_t dststride,
;                         uint8_t *_src, ptrdiff_t _srcstride,
;                         int width, int height, int mx, int my,
;                         int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_epel_pixels2_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 2, 8, epel
cglobal hevc_put_hevc_epel_pixels4_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 4, 8, epel
cglobal hevc_put_hevc_epel_pixels8_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 8, 8, epel
cglobal hevc_put_hevc_epel_pixels16_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 16, 8, epel

cglobal hevc_put_hevc_epel_pixels2_10, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 2, 10, epel
cglobal hevc_put_hevc_epel_pixels4_10, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 4, 10, epel
cglobal hevc_put_hevc_epel_pixels8_10, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 8, 10, epel

cglobal hevc_put_hevc_qpel_pixels4_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 4, 8, qpel
cglobal hevc_put_hevc_qpel_pixels8_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 8, 8, qpel
cglobal hevc_put_hevc_qpel_pixels16_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 16, 8, qpel

cglobal hevc_put_hevc_qpel_pixels4_10, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 4, 10, qpel
cglobal hevc_put_hevc_qpel_pixels8_10, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS 8, 10, qpel

; ******************************
; void put_hevc_epel_hX(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int mx, int my,
;                       int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_epel_h2_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my
    PUT_HEVC_EPEL_H    2, 8
    RET
cglobal hevc_put_hevc_epel_h4_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my
    PUT_HEVC_EPEL_H    4, 8
    RET
cglobal hevc_put_hevc_epel_h8_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my
    PUT_HEVC_EPEL_H    8, 8
    RET

cglobal hevc_put_hevc_epel_h2_10, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my
    PUT_HEVC_EPEL_H    2, 10
    RET
cglobal hevc_put_hevc_epel_h4_10, 9, 12, 0 , dst, dststride, src, srcstride,width,height,mx,my
    PUT_HEVC_EPEL_H    4, 10
    RET

; ******************************
; void put_hevc_epel_v(int16_t *dst, ptrdiff_t dststride,
;                      uint8_t *_src, ptrdiff_t _srcstride,
;                      int width, int height, int mx, int my,
;                      int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_epel_v2_8, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
    PUT_HEVC_EPEL_V    2, 8
    RET
cglobal hevc_put_hevc_epel_v4_8, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
    PUT_HEVC_EPEL_V    4, 8
    RET
cglobal hevc_put_hevc_epel_v8_8, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
    PUT_HEVC_EPEL_V    8, 8
    RET
cglobal hevc_put_hevc_epel_v16_8, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
    PUT_HEVC_EPEL_V   16, 8
    RET

cglobal hevc_put_hevc_epel_v2_10, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
    PUT_HEVC_EPEL_V    2, 10
    RET
cglobal hevc_put_hevc_epel_v4_10, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
    PUT_HEVC_EPEL_V    4, 10
    RET
cglobal hevc_put_hevc_epel_v8_10, 8, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my
    PUT_HEVC_EPEL_V    8, 10
    RET

; ******************************
; void put_hevc_epel_hv(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int mx, int my,
;                       int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_epel_hv2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my, mcbuffer
    PUT_HEVC_EPEL_HV   2, 8
    RET
cglobal hevc_put_hevc_epel_hv4_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my, mcbuffer
    PUT_HEVC_EPEL_HV   4, 8
    RET
cglobal hevc_put_hevc_epel_hv8_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my, mcbuffer
    PUT_HEVC_EPEL_HV   8, 8
    RET
cglobal hevc_put_hevc_epel_hv2_10, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my, mcbuffer
    PUT_HEVC_EPEL_HV   2, 10
    RET
cglobal hevc_put_hevc_epel_hv4_10, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mx, my, mcbuffer
    PUT_HEVC_EPEL_HV   4, 10
    RET

%endif ; ARCH_X86_64

