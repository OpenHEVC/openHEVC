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

epel_extra_before   DB  1                        ;corresponds to EPEL_EXTRA_BEFORE in hevc.h
max_pb_size         DB  64                       ;corresponds to MAX_PB_SIZE in hevc.h

qpel_extra_before   DB  0,  3,  3,  2            ;corresponds to the ff_hevc_qpel_extra_before in hevc.c
qpel_extra          DB  0,  6,  7,  6            ;corresponds to the ff_hevc_qpel_extra in hevc.c
qpel_extra_after    DB  0,  3,  4,  4

epel_h_shuffle1_8   DB  0,  1,  2,  3
                    DB  1,  2,  3,  4
                    DB  2,  3,  4,  5
                    DB  3,  4,  5,  6

epel_h_shuffle1_10  DB  0,  1,  2,  3,  4,  5,  6,  7
                    DB  2,  3,  4,  5,  6,  7,  8,  9

epel_h_shuffle2_10  DB  4,  5,  6,  7,  8,  9, 10, 11
                    DB  6,  7,  8,  9, 10, 11, 12, 13

qpel_h_shuffle1     DB  0,  1,  2,  3,  4,  5,  6,  7
                    DB  1,  2,  3,  4,  5,  6,  7,  8

qpel_h_filter1_8    DB -1,  4,-10, 58, 17, -5,  1,  0
                    DB -1,  4,-10, 58, 17, -5,  1,  0

qpel_h_filter2_8    DB -1,  4,-11, 40, 40,-11,  4, -1
                    DB -1,  4,-11, 40, 40,-11,  4, -1

qpel_h_filter3_8    DB  0,  1, -5, 17, 58,-10,  4, -1
                    DB  0,  1, -5, 17, 58,-10,  4, -1

qpel_h_filter1_10   DW -1,  4,-10, 58, 17, -5,  1,  0

qpel_h_filter2_10   DW -1,  4,-11, 40, 40,-11,  4, -1

qpel_h_filter3_10   DW  0,  1, -5, 17, 58,-10,  4, -1

single_mask_16      DW  0,  0,  0,  0,  0,  0,  0, -1
single_mask_32      DW  0,  0,  0,  0,  0,  0, -1, -1
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

%macro QPEL_H_FILTER 2
    movdqu           m13, [qpel_h_filter%1_%2]
    movdqu           m14, [qpel_h_shuffle1]    ; register containing shuffle
%endmacro

%macro QPEL_V_FILTER 2
    movdqu            m6, [qpel_h_filter%1_10]
    psrldq            m7, m6, 2
    psrldq            m8, m6, 4
    psrldq            m9, m6, 6
    psrldq           m10, m6, 8
    psrldq           m11, m6, 10
    psrldq           m12, m6, 12
    psrldq           m13, m6, 14

    punpcklwd         m6, m6                    ; put double values to 32bit.
    punpcklwd         m7, m7
    punpcklwd         m8, m8
    punpcklwd         m9, m9
    punpcklwd        m10, m10
    punpcklwd        m11, m11
    punpcklwd        m12, m12
    punpcklwd        m13, m13

%if %2 == 10
    psrad             m6, m6, 16
    psrad             m7, m7, 16
    psrad             m8, m8, 16
    psrad             m9, m9, 16
    psrad            m10, m10, 16
   psrad            m11, m11, 16
    psrad            m12, m12, 16
    psrad            m13, m13, 16
%endif

    pshufd            m6, m6, 0
    pshufd            m7, m7, 0
    pshufd            m8, m8, 0
    pshufd            m9, m9, 0
    pshufd           m10, m10, 0
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

%macro MUL_ADD_H_2 2
    MUL_ADD_H_2_2     %1,  %2
    p%2              m12, m15
%endmacro

%macro MUL_ADD_H_4 2
    p%1               m0, m13
    p%1               m1, m13
    p%1               m2, m13
    p%1               m3, m13
    p%2               m0, m1
    p%2               m2, m3
    p%2              m12, m0, m2

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

%macro MUL_ADD_V_LO4 6
    p%1               %3, m6
    p%1               %4, m7
    p%2               %3, %4
    p%1               %5, m8
    p%2               %3, %5
    p%1               %6, m9
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

%macro QPEL_H_LOAD2 0
    movdqu            m0, [srcq+  r9-3]          ; load data from source
    psrldq            m1, m0,2
%endmacro


%macro QPEL_V_LOAD_LO 3
%if %1 == 8
    lea              r11, [%2q+  r9]
%else
    lea              r11, [%2q+2*r9]
%endif
    movdqu            m3, [r11]                  ;load 64bit of x
%ifidn %3, srcstride
    sub              r11, %3q
    movdqu            m2, [r11]                  ;load 64bit of x-stride
    sub              r11, %3q
    movdqu            m1, [r11]                  ;load 64bit of x-2*stride
    sub              r11, %3q
%else
    sub              r11, %3
    movdqu            m2, [r11]                  ;load 64bit of x-stride
    sub              r11, %3
    movdqu            m1, [r11]                  ;load 64bit of x-2*stride
    sub              r11, %3
%endif
    movdqu            m0, [r11]                  ;load 64bit of x-3*stride
%endmacro

%macro QPEL_V_LOAD_HI 3
%if %1 == 8
    lea              r11, [%2q+  r9]
%else
    lea              r11, [%2q+2*r9]
%endif
%ifidn %3, srcstride
    add              r11, %3q
    movdqu            m0, [r11]                  ;load 64bit of x+stride
    add              r11, %3q
    movdqu            m1, [r11]                  ;load 64bit of x+2*stride
    add              r11, %3q
    movdqu            m2, [r11]                  ;load 64bit of x+3*stride
    add              r11, %3q

%else
    add              r11, %3
    movdqu            m0, [r11]                  ;load 64bit of x+stride
    add              r11, %3
    movdqu            m1, [r11]                  ;load 64bit of x+2*stride
    add              r11, %3
    movdqu            m2, [r11]                  ;load 64bit of x+3*stride
    add              r11, %3
%endif
    movdqu            m3, [r11]                  ;load 64bit of x+4*stride
%endmacro


%define QPEL_H_LOAD4 QPEL_H_LOAD2

%macro QPEL_H_LOAD8 0
    QPEL_H_LOAD2
    psrldq            m2, m0,4
    psrldq            m3, m0,6
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
; void put_hevc_qpel_h(int16_t *dst, ptrdiff_t dststride,
;                     uint8_t *_src, ptrdiff_t _srcstride,
;                     int width, int height, int16_t* mcbuffer)
;
;      r0 : *dst
;      r1 : dststride
;      r2 : *src
;      r3 : srcstride
;      r4 : width
;      r5 : height
;
; ******************************
%macro QPEL_H_COMPUTE4_8 0
    INST_SRC1_CST_2  shufb, m0, m1, m14
    MUL_ADD_H_2 maddubsw, haddw
%endmacro
%macro QPEL_H_COMPUTE8_8 0
    INST_SRC1_CST_4  shufb, m0, m1, m2, m3, m14
    MUL_ADD_H_4 maddubsw, haddw
%endmacro

%macro QPEL_H_COMPUTE2_10 0
    MUL_ADD_H_2  maddubsw, haddw
    psrad             m12, 2
    packssdw          m12, m15
%endmacro
%define QPEL_H_COMPUTE4_10 QPEL_H_COMPUTE2_10



%macro PUT_HEVC_QPEL_H 3
    QPEL_H_FILTER     %2, %3
    LOOP_INIT  qpel_h_h_%1_%2_%3, qpel_h_w_%1_%2_%3
    QPEL_H_LOAD%1
    QPEL_H_COMPUTE%1_%3
    PEL_STORE%1       dst, m12, m15
    LOOP_END   qpel_h_h_%1_%2_%3, qpel_h_w_%1_%2_%3, %1, dst, dststride, src, srcstride
%endmacro


; ******************************
; void put_hevc_qpel_v(int16_t *dst, ptrdiff_t dststride,
;                     uint8_t *_src, ptrdiff_t _srcstride,
;                     int width, int height, int16_t* mcbuffer)
;
;      r0 : *dst
;      r1 : dststride
;      r2 : *src
;      r3 : srcstride
;      r4 : width
;      r5 : height
;
; ******************************
%macro QPEL_V_COMPUTE_LO4_8 1
    INST_SRC1_CST_4    unpcklbw, m0, m1, m2, m3, m15
    MUL_ADD_V_LO4    mullw, addsw, m0, m1, m2, m3
    movdqu            m5, m0                     ;store intermediate result in m4
%endmacro
%define QPEL_V_COMPUTE_LO8_8 QPEL_V_COMPUTE_LO4_8

%macro QPEL_V_COMPUTE_HI4_8 1
    INST_SRC1_CST_4    unpcklbw, m0, m1, m2, m3, m15
    MUL_ADD_V_4    mullw, addsw, m0, m1, m2, m3
    movdqu            m4, m0                     ;store temp result in m5
%endmacro
%define QPEL_V_COMPUTE_HI8_8 QPEL_V_COMPUTE_HI4_8

%macro QPEL_V_MERGE4_8 1
    paddw             m0, m4, m5                       ;merge results in m0
%endmacro
%define QPEL_V_MERGE8_8 QPEL_V_MERGE4_8

%macro QPEL_V_COMPUTE_LO4_10 1
    UNPACK_SRAI16_4   unpcklwd, m0, m1, m2, m3
    MUL_ADD_V_LO4  mulld, addd, m0, m1, m2, m3
    movdqu            m5, m0

%endmacro

%macro QPEL_V_COMPUTE_HI4_10 1
    UNPACK_SRAI16_4   unpcklwd, m0, m1, m2, m3
    MUL_ADD_V_4    mulld, addd, m0, m1, m2, m3
    movdqu            m4, m0
%endmacro

%macro QPEL_V_MERGE4_10 1
    paddd             m0, m4, m5                       ;merge results in m0
    psrad             m0, %1
    packssdw          m0, m15
%endmacro


%macro PUT_HEVC_QPEL_V 3
    QPEL_V_FILTER     %2, %3
    LOOP_INIT qpel_v_h_%1_%2_%3, qpel_v_w_%1_%2_%3
    QPEL_V_LOAD_LO    %3, src, srcstride
    QPEL_V_COMPUTE_LO%1_%3 2
    QPEL_V_LOAD_HI    %3, src, srcstride
    QPEL_V_COMPUTE_HI%1_%3 2
    QPEL_V_MERGE%1_%3  0
    PEL_STORE%1      dst, m0, m4
    LOOP_END  qpel_v_h_%1_%2_%3, qpel_v_w_%1_%2_%3, %1, dst, dststride, src, srcstride
%endmacro



; ******************************
; void put_hevc_qpel_hv(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int16_t* mcbuffer)
;
;      r0 : *dst
;      r1 : dststride
;      r2 : *src
;      r3 : srcstride
;      r4 : width
;      r5 : height
;      r6 : mcbuffer
;
; ******************************
%macro PUT_HEVC_QPEL_HV 4

    xor               r9, r9
    mov              r9b, [ qpel_extra_before + %3 ]
    imul              r9, srcstrideq
    sub             srcq, r9                     ; src -= ff_hevc_qpel_extra_before[FV] * srcstride;
    lea              r7q, [mcbufferq]
    xor               r9, r9
    mov              r9b, [qpel_extra + %3 ]
    add          heightq, r9                     ; height += ff_hevc_qpel_extra[FV]
    xor               r9, r9

    QPEL_H_FILTER     %2, %4
    LOOP_INIT  qpel_hv_h_h_%1_%2_%3_%4, qpel_hv_h_w_%1_%2_%3_%4
    QPEL_H_LOAD%1
    QPEL_H_COMPUTE%1_%4
    PEL_STORE%1       r7, m12, m15
    LOOP_END   qpel_hv_h_h_%1_%2_%3_%4, qpel_hv_h_w_%1_%2_%3_%4, %1, r7, 128, src, srcstride

    xor               r9, r9
    mov              r9b, [qpel_extra_before + %3]
    imul              r9, 128
    add               mcbufferq, r9              ; mcbuffer+= + ff_hevc_qpel_extra_before[FV] * MAX_PB_SIZE;
    xor               r9, r9
    mov              r9b, [qpel_extra + %3 ]
    sub          heightq, r9                     ; reset height.
    xor               r9, r9

    QPEL_V_FILTER     %3, 10
    LOOP_INIT qpel_hv_v_h_%1_%2_%3_%4, qpel_hv_v_w_%1_%2_%3_%4
    QPEL_V_LOAD_LO    10, mcbuffer, 128
    QPEL_V_COMPUTE_LO4_10 2
    QPEL_V_LOAD_HI    10, mcbuffer, 128
    QPEL_V_COMPUTE_HI4_10 2
    QPEL_V_MERGE4_10  6
    PEL_STORE4      dst, m0, m4
    LOOP_END  qpel_hv_v_h_%1_%2_%3_%4, qpel_hv_v_w_%1_%2_%3_%4, 4, dst, dststride, mcbuffer, 128        ;forced loop to 4.

%endmacro


; ******************************
; void put_hevc_mc_pixels(int16_t *dst, ptrdiff_t dststride,
;                         uint8_t *_src, ptrdiff_t _srcstride,
;                         int width, int height, int mx, int my,
;                         int16_t* mcbuffer, uint8_t denom,
;                             int16_t wlxFlag, int16_t wl1Flag,
;                             int16_t olxFlag, int16_t ol1Flag)
;
;        r0 : *dst
;        r1 : dststride
;        r2 : *src
;        r3 : srcstride
;        r4 : width
;        r5 : height
;        r6 : mx                       ;unused
;        r7 : my                       ;unused
;        r8 : mcbuffer                 ;unused
;        r9 : denom
;        r10: wlxFlag
;        r11: wl1Flag
;        r12: olxFlag
;        r13: ol1Flag
;
; ******************************
%macro WEIGHTED_INIT2 0
    movdqu  m6,
%endmacro

%macro WEIGHTED_INIT_0 2
    WEIGHTED_INIT%1
    xor              r10,r10
    sub              r10,13,%2
    mov               m5,1
    pslliw            m5,r10
    add              r10,1                                                     ;shift
%endmacro

%macro WEIGHTED_COMPUTE_0 1
    paddw             %1, m5
    psraw             %1,r10
%endmacro

%define WEIGHTED_COMPUTE2_0 WEIGHTED_COMPUTE_0
%define WEIGHTED_COMPUTE4_0 WEIGHTED_COMPUTE_0
%define WEIGHTED_COMPUTE8_0 WEIGHTED_COMPUTE_0

%macro WEIGHTED_LOAD2 2
    movq              m1,  [%1+r9]
%endmacro
%define WEIGHTED_LOAD4 WEIGHTED_LOAD2
%macro WEIGHTED_LOAD8 2
    movq              m1,  [%1+r9]
%endmacro
%macro WEIGHTED_LOAD16 2
    WEIGHTED_LOAD8 %1, %2
    movq              m2,  [%1+r9+8]
%endmacro

%macro WEIGHTED_LOAD2_1 2
    movq              m3,  [%1+r9]
%endmacro
%define WEIGHTED_LOAD4_1 WEIGHTED_LOAD2_1
%macro WEIGHTED_LOAD8_1 2
    movq              m3,  [%1+r9]
%endmacro
%macro WEIGHTED_LOAD16_1 2
    WEIGHTED_LOAD8_1 %1, %2
    movq              m4,  [%1+r9+8]
%endmacro

%macro WEIGHTED_STORE2 2
    packuswb          %1, %1
    movss     [%1q+2*r9], %2

%endmacro
%define WEIGHTED_STORE4 WEIGHTED_STORE2
%macro WEIGHTED_STORE8 2

%endmacro

%macro PUT_HEVC_MC_PIXELS_W 4

    LOOP_INIT %4_pixels_h_%1_w_%2_%3, %4_pixels_v_%1_w_%2_%3
    MC_LOAD_PIXEL     %2
    MC_PIXEL_COMPUTE%1_%2
%if %3 == 4
    PEL_STORE%1      dst, m1, m2
%else

%endif
    LOOP_END %4_pixels_h_%1_w_%2_%3, %4_pixels_v_%1_w_%2_%3, %1, dst, dststride, src, srcstride
    RET
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

; ******************************
; void put_hevc_qpel_hX_X_X(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_qpel_h4_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_H    4, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h4_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_H    4, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h4_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_H    4, 3, 8
    RET

cglobal hevc_put_hevc_qpel_h8_1_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_QPEL_H    8, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h8_2_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_QPEL_H    8, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h8_3_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_QPEL_H    8, 3, 8
    RET


    ; ******************************
; void put_hevc_qpel_vX_X_X(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_qpel_v4_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_V    4, 1, 8
    RET
cglobal hevc_put_hevc_qpel_v4_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_V    4, 2, 8
    RET
cglobal hevc_put_hevc_qpel_v4_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_V    4, 3, 8
    RET

cglobal hevc_put_hevc_qpel_v8_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_V    8, 1, 8
    RET
cglobal hevc_put_hevc_qpel_v8_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_V    8, 2, 8
    RET
cglobal hevc_put_hevc_qpel_v8_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height
    PUT_HEVC_QPEL_V    8, 3, 8
    RET


    ; ******************************
; void put_hevc_qpel_hX_X_v_X(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_qpel_h4_1_v_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 1, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h4_1_v_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 1, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h4_1_v_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 1, 3, 8
    RET

cglobal hevc_put_hevc_qpel_h4_2_v_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 2, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h4_2_v_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 2, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h4_2_v_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 2, 3, 8
    RET

cglobal hevc_put_hevc_qpel_h4_3_v_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 3, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h4_3_v_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 3, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h4_3_v_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    4, 3, 3, 8
    RET


cglobal hevc_put_hevc_qpel_h8_1_v_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 1, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h8_1_v_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 1, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h8_1_v_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 1, 3, 8
    RET

cglobal hevc_put_hevc_qpel_h8_2_v_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 2, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h8_2_v_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 2, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h8_2_v_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 2, 3, 8
    RET

cglobal hevc_put_hevc_qpel_h8_3_v_1_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 3, 1, 8
    RET
cglobal hevc_put_hevc_qpel_h8_3_v_2_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 3, 2, 8
    RET
cglobal hevc_put_hevc_qpel_h8_3_v_3_8, 9, 12, 0 , dst, dststride, src, srcstride, width, height, mcbuffer
    PUT_HEVC_QPEL_HV    8, 3, 3, 8
    RET


; ******************************
; void put_hevc_mc_pixels_w(int16_t *dst, ptrdiff_t dststride,
;                         uint8_t *_src, ptrdiff_t _srcstride,
;                         int width, int height, int mx, int my,
;                         int16_t* mcbuffer)
; ******************************
cglobal hevc_put_hevc_epel_pixels2_w_0_8, 9, 12, 0 , dst, dststride, src, srcstride,width,height
    PUT_HEVC_MC_PIXELS_W 2, 0, 8, epel

%endif ; ARCH_X86_64

