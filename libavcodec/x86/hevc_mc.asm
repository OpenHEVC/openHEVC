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

%macro EPEL_TABLE 4
hevc_epel_filters_%4_%1 times %2 d%3 -2, 58
                        times %2 d%3 10, -2
                        times %2 d%3 -4, 54
                        times %2 d%3 16, -2
                        times %2 d%3 -6, 46
                        times %2 d%3 28, -4
                        times %2 d%3 -4, 36
                        times %2 d%3 36, -4
                        times %2 d%3 -4, 28
                        times %2 d%3 46, -6
                        times %2 d%3 -2, 16
                        times %2 d%3 54, -4
                        times %2 d%3 -2, 10
                        times %2 d%3 58, -2
%endmacro

EPEL_TABLE  8, 8, b, sse4
EPEL_TABLE 10, 4, w, sse4

%macro QPEL_TABLE 4
hevc_qpel_filters_%4_%1 times %2 d%3  -1,  4
                        times %2 d%3 -10, 58
                        times %2 d%3  17, -5
                        times %2 d%3   1,  0
                        times %2 d%3  -1,  4
                        times %2 d%3 -11, 40
                        times %2 d%3  40,-11
                        times %2 d%3   4, -1
                        times %2 d%3   0,  1
                        times %2 d%3  -5, 17
                        times %2 d%3  58,-10
                        times %2 d%3   4, -1
%endmacro

QPEL_TABLE  8, 8, b, sse4
QPEL_TABLE 10, 4, w, sse4

%if ARCH_X86_64

%macro EPEL_FILTER 2                             ; bit depth, filter index
%ifdef PIC
    lea         rfilterq, [hevc_epel_filters_sse4_%1]
%else
    %define rfilterq hevc_epel_filters_sse4_%1
%endif
    sub              %2q, 1
    shl              %2q, 5                      ; multiply by 32
    movdqa           m14, [rfilterq + %2q]        ; get 2 first values of filters
    movdqa           m15, [rfilterq + %2q+16]     ; get 2 last values of filters
%endmacro

%macro EPEL_HV_FILTER 1
%ifdef PIC
    lea         rfilterq, [hevc_epel_filters_sse4_%1]
%else
    %define rfilterq hevc_epel_filters_sse4_%1
%endif
    sub              mxq, 1
    sub              myq, 1
    shl              mxq, 5                      ; multiply by 32
    shl              myq, 5                      ; multiply by 32
    movdqa           m14, [rfilterq + mxq]        ; get 2 first values of filters
    movdqa           m15, [rfilterq + mxq+16]     ; get 2 last values of filters
    lea           r3srcq, [srcstrideq*3]

%ifdef PIC
    lea         rfilterq, [hevc_epel_filters_sse4_10]
%else
    %define rfilterq hevc_epel_filters_sse4_10
%endif
    movdqa           m12, [rfilterq + myq]        ; get 2 first values of filters
    movdqa           m13, [rfilterq + myq+16]     ; get 2 last values of filters
%endmacro

%macro QPEL_FILTER 2
%ifdef PIC
    lea         rfilterq, [hevc_qpel_filters_sse4_%1]
%else
    %define rfilterq hevc_qpel_filters_sse4_%1
%endif
    sub              %2q, 1
    shl              %2q, 6                      ; multiply by 16
    movdqa           m12, [rfilterq + %2q]       ; get 4 first values of filters
    movdqa           m13, [rfilterq + %2q + 16]  ; get 4 first values of filters
    movdqa           m14, [rfilterq + %2q + 32]  ; get 4 first values of filters
    movdqa           m15, [rfilterq + %2q + 48]  ; get 4 first values of filters
%endmacro

%macro EPEL_LOAD 4
%ifdef PIC
    lea rfilterq, [%2]
%else
    %define rfilterq %2
%endif
    movdqu            m0, [rfilterq ]            ;load 128bit of x
%ifnum %3
    movdqu            m1, [rfilterq+  %3]        ;load 128bit of x+stride
    movdqu            m2, [rfilterq+2*%3]        ;load 128bit of x+2*stride
    movdqu            m3, [rfilterq+3*%3]        ;load 128bit of x+3*stride
%else
    movdqu            m1, [rfilterq+  %3q]       ;load 128bit of x+stride
    movdqu            m2, [rfilterq+2*%3q]       ;load 128bit of x+2*stride
    movdqu            m3, [rfilterq+r3srcq]      ;load 128bit of x+2*stride
%endif

%if %1 == 8
%if %4 > 8
    SBUTTERFLY        bw, 0, 1, 10
    SBUTTERFLY        bw, 2, 3, 10
%else
    punpcklbw         m0, m1
    punpcklbw         m2, m3
%endif
%else
%if %4 > 4
    SBUTTERFLY        wd, 0, 1, 10
    SBUTTERFLY        wd, 2, 3, 10
%else
    punpcklwd         m0, m1
    punpcklwd         m2, m3
%endif
%endif
%endmacro


%macro QPEL_H_LOAD 3
%assign %%stride (%1+7)/8

    movdqu            m0, [%2-3*%%stride]       ; load data from source
    movdqu            m1, [%2-2*%%stride]
    movdqu            m2, [%2-%%stride]
    movdqu            m3, [%2  ]
    movdqu            m4, [%2+%%stride]
    movdqu            m5, [%2+2*%%stride]
    movdqu            m6, [%2+3*%%stride]
    movdqu            m7, [%2+4*%%stride]

%if %1 == 8
%if %3 > 8
    SBUTTERFLY        wd, 0, 1, 10
    SBUTTERFLY        wd, 2, 3, 10
    SBUTTERFLY        wd, 4, 5, 10
    SBUTTERFLY        wd, 6, 7, 10
%else
    punpcklwd         m0, m1
    punpcklwd         m2, m3
    punpcklwd         m4, m5
    punpcklwd         m6, m7
%endif
%else
%if %3 > 4
    SBUTTERFLY        dq, 0, 1, 10
    SBUTTERFLY        dq, 2, 3, 10
    SBUTTERFLY        dq, 4, 5, 10
    SBUTTERFLY        dq, 6, 7, 10
%else
    punpckldq         m0, m1
    punpckldq         m2, m3
    punpckldq         m4, m5
    punpckldq         m6, m7
%endif
%endif
%endmacro

%macro QPEL_V_LOAD 4
    lea             r12q, [%2]
    sub             r12q, r3srcq
    movdqu            m0, [r12            ]      ;load x- 3*srcstride
    movdqu            m1, [r12+   %3q     ]      ;load x- 2*srcstride
    movdqu            m2, [r12+ 2*%3q     ]      ;load x-srcstride
    movdqu            m3, [%2       ]      ;load x
    movdqu            m4, [%2+   %3q]      ;load x+stride
    movdqu            m5, [%2+ 2*%3q]      ;load x+2*stride
    movdqu            m6, [%2+r3srcq]      ;load x+3*stride
    movdqu            m7, [%2+ 4*%3q]      ;load x+4*stride
%if %1 == 8
%if %4 > 8
    SBUTTERFLY        bw, 0, 1, 8
    SBUTTERFLY        bw, 2, 3, 8
    SBUTTERFLY        bw, 4, 5, 8
    SBUTTERFLY        bw, 6, 7, 8
%else
    punpcklbw         m0, m1
    punpcklbw         m2, m3
    punpcklbw         m4, m5
    punpcklbw         m6, m7
%endif
%else
%if %4 > 4
    SBUTTERFLY        wd, 0, 1, 8
    SBUTTERFLY        wd, 2, 3, 8
    SBUTTERFLY        wd, 4, 5, 8
    SBUTTERFLY        wd, 6, 7, 8
%else
    punpcklwd         m0, m1
    punpcklwd         m2, m3
    punpcklwd         m4, m5
    punpcklwd         m6, m7
%endif
%endif
%endmacro

%macro PEL_STORE2 3
    movd           [%1], %2
%endmacro
%macro PEL_STORE4 3
    movq           [%1], %2
%endmacro
%macro PEL_STORE6 3
    movq           [%1], %2
    psrldq            %2, 8
    movd         [%1+8], %2
%endmacro
%macro PEL_STORE8 3
    movdqa         [%1], %2
%endmacro
%macro PEL_STORE12 3
    movdqa         [%1], %2
    movq        [%1+16], %3
%endmacro
%macro PEL_STORE16 3
    PEL_STORE8        %1, %2, %3
    movdqa      [%1+16], %3
%endmacro

%macro LOOP_END 4
    lea              %1q, [%1q+2*%2q]            ; dst += dststride
    lea              %3q, [%3q+  %4q]            ; src += srcstride
    dec          heightq                         ; cmp height
    jnz               .loop                      ; height loop
%endmacro


%macro MC_PIXEL_COMPUTE 2 ;width, bitdepth
%if %2 == 8
%if %1 > 8
    movhlps           m1, m0
    pmovzxbw          m1, m1
    psllw             m1, 14-%2
%endif
    pmovzxbw          m0, m0
%endif
    psllw             m0, 14-%2
%endmacro


%macro EPEL_COMPUTE 4 ; bitdepth, width, filter1, filter2
%if %1 == 8
    pmaddubsw         m0, %3   ;x1*c1+x2*c2
    pmaddubsw         m2, %4   ;x3*c3+x4*c4
    paddw             m0, m2    
%if %2 > 8
    pmaddubsw         m1, %3
    pmaddubsw         m3, %4
    paddw             m1, m3
%endif
%else
    pmaddwd           m0, %3
    pmaddwd           m2, %4
    paddd             m0, m2
%if %2 > 4
    pmaddwd           m1, %3
    pmaddwd           m3, %4
    paddd             m1, m3
%endif
    psrad             m0, %1-8 
    psrad             m1, %1-8
    packssdw          m0, m1
%endif
%endmacro


%macro QPEL_COMPUTE 2     ; width, bitdepth
%if %2 == 8
    pmaddubsw         m0, m12   ;x1*c1+x2*c2
    pmaddubsw         m2, m13   ;x3*c3+x4*c4
    pmaddubsw         m4, m14   ;x5*c5+x6*c6
    pmaddubsw         m6, m15   ;x7*c7+x8*c8
    paddw             m0, m2
    paddw             m4, m6
    paddw             m0, m4
%if %1 > 8
    pmaddubsw         m1, m12
    pmaddubsw         m3, m13
    pmaddubsw         m5, m14
    pmaddubsw         m7, m15
    paddw             m1, m3
    paddw             m5, m7
    paddw             m1, m5
%endif
%else
    pmaddwd           m0, m12
    pmaddwd           m2, m13
    pmaddwd           m4, m14
    pmaddwd           m6, m15
    paddd             m0, m2
    paddd             m4, m6
    paddd             m0, m4
    psrad             m0, %2-8
%if %1 > 4
    pmaddwd           m1, m12
    pmaddwd           m3, m13
    pmaddwd           m5, m14
    pmaddwd           m7, m15
    paddd             m1, m3
    paddd             m5, m7
    paddd             m1, m5
    psrad             m1, %2-8

%endif
    packssdw          m0, m1
%endif
%endmacro

INIT_XMM sse4                                    ; adds ff_ and _sse4 to function name
; ******************************
; void put_hevc_mc_pixels(int16_t *dst, ptrdiff_t dststride,
;                         uint8_t *_src, ptrdiff_t _srcstride,
;                         int height, int mx, int my)
; ******************************

%macro HEVC_PUT_HEVC_PEL_PIXELS 2
cglobal hevc_put_hevc_pel_pixels%1_%2, 5, 5, 2, dst, dststride, src, srcstride,height
.loop
%if   %2 == 8
%if   %1 == 4
    movd              m0, [srcq]                                               ; load data from source
%elif %1 == 8
    movq              m0, [srcq]                                               ; load data from source
%else
    movdqu            m0, [srcq]                                               ; load data from source
%endif
%else
%if   %1 == 2
    movd              m0, [srcq]                                               ; load data from source
%elif %1 == 4
    movq              m0, [srcq]                                               ; load data from source
%else
    movdqu            m0, [srcq]                                               ; load data from source
%endif
%endif

    MC_PIXEL_COMPUTE  %1, %2
    PEL_STORE%1     dstq, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET
%endmacro

cglobal hevc_put_hevc_pel_pixels24_8, 5, 5, 2, dst, dststride, src, srcstride,height
.loop
    movdqu            m0, [srcq]                 ; load data from source
    movhlps           m1, m0
    pmovzxbw          m0, m0
    pmovzxbw          m1, m1
    psllw             m0, 6
    psllw             m1, 6
    movdqa        [dstq], m0
    movdqa   [dstq + 16], m1                     ; store 16
    movq              m0, [srcq + 16]
    pmovzxbw          m0, m0
    psllw             m0, 6
    movdqa   [dstq + 32], m0
    LOOP_END         dst, dststride, src, srcstride
    RET

cglobal hevc_put_hevc_pel_pixels12_10, 5, 5, 1, dst, dststride, src, srcstride,height
.loop
    movdqu            m0, [srcq]            ; load data from source
    psllw             m0, 4
    movdqa        [dstq], m0
    movq              m0, [srcq + 16]
    psllw             m0, 4
    movq     [dstq + 16], m0
    LOOP_END         dst, dststride, src, srcstride
    RET

; ******************************
; void put_hevc_epel_hX(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int mx, int my,
;                       int16_t* mcbuffer)
; ******************************


%macro HEVC_PUT_HEVC_EPEL_H 2
cglobal hevc_put_hevc_epel_h%1_%2, 6, 7, 15 , dst, dststride, src, srcstride, height, mx, rfilter
%assign %%stride ((%2 + 7)/8)
    sub             srcq, %%stride
    EPEL_FILTER       %2, mx
.loop
    EPEL_LOAD         %2, srcq, %%stride, %1
    EPEL_COMPUTE      %2, %1, m14, m15
    PEL_STORE%1      dstq, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET
%endmacro

cglobal hevc_put_hevc_epel_h24_8, 6, 7, 15 , dst, dststride, src, srcstride, height, mx, rfilter
    sub             srcq, 1
    EPEL_FILTER        8, mx
.loop
    EPEL_LOAD          8, srcq, 1, 24
    EPEL_COMPUTE       8, 16, m14, m15
    PEL_STORE16     dstq, m0, m1
    EPEL_LOAD          8, srcq + 16, 1, 24
    EPEL_COMPUTE       8, 8, m14, m15
    PEL_STORE8   dstq+32, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

cglobal hevc_put_hevc_epel_h12_10, 6, 7, 15 , dst, dststride, src, srcstride, height, mx, rfilter
    sub             srcq, 2
    EPEL_FILTER       10, mx
.loop
    EPEL_LOAD         10, srcq, 2, 12
    EPEL_COMPUTE      10, 8, m14, m15
    PEL_STORE8      dstq, m0, m1
    EPEL_LOAD         10, srcq + 16, 2, 12
    EPEL_COMPUTE      10, 4, m14, m15
    PEL_STORE4   dstq+16, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

; ******************************
; void put_hevc_epel_v(int16_t *dst, ptrdiff_t dststride,
;                      uint8_t *_src, ptrdiff_t _srcstride,
;                      int width, int height, int mx, int my,
;                      int16_t* mcbuffer)
; ******************************

%macro HEVC_PUT_HEVC_EPEL_V 2
cglobal hevc_put_hevc_epel_v%1_%2, 7, 9, 15 , dst, dststride, src, srcstride, height, mx, my, r3src, rfilter
    lea           r3srcq, [srcstrideq*3]
    sub             srcq, srcstrideq
    EPEL_FILTER        %2, my
.loop
    EPEL_LOAD         %2, srcq, srcstride, %1
    EPEL_COMPUTE      %2, %1, m14, m15
    PEL_STORE%1     dstq, m0, m1
    LOOP_END          dst, dststride, src, srcstride
    RET
%endmacro

cglobal hevc_put_hevc_epel_v24_8, 7, 9, 0 , dst, dststride, src, srcstride, height, mx, my, r3src, rfilter
    lea           r3srcq, [srcstrideq*3]
    sub               srcq, srcstrideq
    EPEL_FILTER        8, my
.loop
    EPEL_LOAD          8, srcq, srcstride, 24
    EPEL_COMPUTE       8, 16, m14, m15
    PEL_STORE16     dstq, m0, m1
    EPEL_LOAD          8, srcq + 16, srcstride, 24
    EPEL_COMPUTE       8, 8, m14, m15
    PEL_STORE8   dstq+32, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

cglobal hevc_put_hevc_epel_v12_10, 7, 9, 15 , dst, dststride, src, srcstride, height, mx, my, r3src, rfilter
    lea           r3srcq, [srcstrideq*3]
    sub             srcq, srcstrideq
    EPEL_FILTER       10, my
.loop
    EPEL_LOAD         10, srcq, srcstride, 12
    EPEL_COMPUTE      10, 8, m14, m15
    PEL_STORE8      dstq, m0, m1
    EPEL_LOAD         10, srcq + 16, srcstride, 12
    EPEL_COMPUTE      10, 4, m14, m15
    PEL_STORE4   dstq+16, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

; ******************************
; void put_hevc_epel_hv(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int mx, int my)
; ******************************

%macro HEVC_PUT_HEVC_EPEL_HV 2
cglobal hevc_put_hevc_epel_hv%1_%2, 7, 9, 12 , dst, dststride, src, srcstride, height, mx, my, r3src, rfilter
%assign %%stride ((%2 + 7)/8)
    sub             srcq, %%stride
    sub             srcq, srcstrideq
    EPEL_HV_FILTER    %2
    EPEL_LOAD         %2, srcq, %%stride, %1
    EPEL_COMPUTE      %2, %1, m14, m15
    SWAP              m4, m0
    lea             srcq, [srcq + srcstrideq]
    EPEL_LOAD         %2, srcq, %%stride, %1
    EPEL_COMPUTE      %2, %1, m14, m15
    SWAP              m5, m0
    lea             srcq, [srcq + srcstrideq]
    EPEL_LOAD         %2, srcq, %%stride, %1
    EPEL_COMPUTE      %2, %1, m14, m15
    SWAP              m6, m0
    lea             srcq, [srcq + srcstrideq]
.loop
    EPEL_LOAD         %2, srcq, %%stride, %1
    EPEL_COMPUTE      %2, %1, m14, m15
    SWAP              m7, m0
    punpcklwd         m0, m4, m5
    punpcklwd         m2, m6, m7
%if %1 > 4
    punpckhwd         m1, m4, m5
    punpckhwd         m3, m6, m7
%endif
    EPEL_COMPUTE      14, %1, m12, m13
    PEL_STORE%1     dstq, m0, m1
    movdqa            m4, m5
    movdqa            m5, m6
    movdqa            m6, m7
    LOOP_END         dst, dststride, src, srcstride
    RET
%endmacro

cglobal hevc_put_hevc_epel_hv12_8, 7, 11, 12 , dst, dststride, src, srcstride, height, mx, my, r3src, rfilter
    sub             srcq, 1
    sub             srcq, srcstrideq
    EPEL_HV_FILTER     8
.loop
    EPEL_LOAD          8, srcq, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
    SWAP              m4, m0
    EPEL_LOAD          8, srcq + srcstrideq, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
    SWAP              m5, m0
    EPEL_LOAD          8, srcq + 2*srcstrideq, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
    SWAP              m6, m0
    EPEL_LOAD          8, srcq + r3srcq, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
    SWAP              m7, m0
    punpcklwd         m0, m4, m5
    punpckhwd         m1, m4, m5
    punpcklwd         m2, m6, m7
    punpckhwd         m3, m6, m7
    EPEL_COMPUTE      14, 8, m12, m13
    PEL_STORE8      dstq, m0, m1

    EPEL_LOAD          8, srcq + 8, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
    SWAP              m4, m0
    EPEL_LOAD          8, srcq + 8 + srcstrideq, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
   SWAP               m5, m0
    EPEL_LOAD          8, srcq + 8 + 2*srcstrideq, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
    SWAP              m6, m0
    EPEL_LOAD          8, srcq + 8 + r3srcq, 1, 12
    EPEL_COMPUTE       8, 6, m14, m15
    SWAP              m7, m0
    punpcklwd         m0, m4, m5
    punpcklwd         m2, m6, m7
    EPEL_COMPUTE      14, 4 , m12, m13
    PEL_STORE4   dstq+16, m0, m1
    
    LOOP_END         dst, dststride, src, srcstride
    RET

cglobal hevc_put_hevc_epel_hv12_10, 7, 11, 12 , dst, dststride, src, srcstride, height, mx, my, r3src, rfilter
    sub             srcq, 2
    sub             srcq, srcstrideq
    EPEL_HV_FILTER    10
.loop
    EPEL_LOAD         10, srcq, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m4, m0
    EPEL_LOAD         10, srcq + srcstrideq, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m5, m0
    EPEL_LOAD         10, srcq + 2*srcstrideq, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m6, m0
    EPEL_LOAD         10, srcq + r3srcq, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m7, m0
    punpcklwd         m0, m4, m5
    punpckhwd         m1, m4, m5
    punpcklwd         m2, m6, m7
    punpckhwd         m3, m6, m7
    EPEL_COMPUTE      14, 8, m12, m13
    PEL_STORE8      dstq, m0, m1


    EPEL_LOAD         10, srcq +16, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m4, m0
    EPEL_LOAD         10, srcq+16+srcstrideq, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m5, m0
    EPEL_LOAD         10, srcq+16+2*srcstrideq, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m6, m0
    EPEL_LOAD         10, srcq+16+r3srcq, 2, 12
    EPEL_COMPUTE      10, 6, m14, m15
    SWAP              m7, m0
    punpcklwd         m0, m4, m5
    punpcklwd         m2, m6, m7
    EPEL_COMPUTE      14,  4, m12, m13
    PEL_STORE4   dstq+16, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

; ******************************
; void put_hevc_qpel_hX_X_X(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int mx, int my)
; ******************************

%macro HEVC_PUT_HEVC_QPEL_H 2
cglobal hevc_put_hevc_qpel_h%1_%2, 6, 7, 15 , dst, dststride, src, srcstride, height, mx, rfilter
    QPEL_FILTER       %2, mx
.loop
    QPEL_H_LOAD       %2, srcq, %1
    QPEL_COMPUTE      %1, %2
    PEL_STORE%1     dstq, m0, m1
    LOOP_END          dst, dststride, src, srcstride
    RET
%endmacro

cglobal hevc_put_hevc_qpel_h24_8, 6, 7, 15 , dst, dststride, src, srcstride, height, mx, rfilter
    QPEL_FILTER        8, mx
.loop
    QPEL_H_LOAD        8, srcq, 24
    QPEL_COMPUTE      16, 8
    PEL_STORE16     dstq, m0, m1
    QPEL_H_LOAD        8, srcq+16, 24
    QPEL_COMPUTE       8, 8
    PEL_STORE8   dstq+32, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

cglobal hevc_put_hevc_qpel_h12_10, 6, 7, 15 , dst, dststride, src, srcstride, height, mx, rfilter
    QPEL_FILTER       10, mx
.loop
    QPEL_H_LOAD       10, srcq, 12
    QPEL_COMPUTE       8, 10
    PEL_STORE8      dstq, m0, m1
    QPEL_H_LOAD       10, srcq+16, 12
    QPEL_COMPUTE       4, 10
    PEL_STORE4   dstq+16, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

; ******************************
; void put_hevc_qpel_vX_X_X(int16_t *dst, ptrdiff_t dststride,
;                       uint8_t *_src, ptrdiff_t _srcstride,
;                       int width, int height, int mx, int my)
; ******************************

%macro HEVC_PUT_HEVC_QPEL_V 2
cglobal hevc_put_hevc_qpel_v%1_%2, 7, 14, 15 , dst, dststride, src, srcstride, height, r3src, my, rfilter
    lea           r3srcq, [srcstrideq*3]
    QPEL_FILTER       %2, my
.loop
    QPEL_V_LOAD       %2, srcq, srcstride, %1
    QPEL_COMPUTE      %1, %2
    PEL_STORE%1     dstq, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET
%endmacro

cglobal hevc_put_hevc_qpel_v24_8, 7, 14, 15 , dst, dststride, src, srcstride, height, r3src, my, rfilter
    lea           r3srcq, [srcstrideq*3]
    QPEL_FILTER        8, my
.loop
    QPEL_V_LOAD        8, srcq, srcstride, 24
    QPEL_COMPUTE      16, 8
    PEL_STORE16     dstq, m0, m1
    QPEL_V_LOAD        8, srcq+16, srcstride, 24
    QPEL_COMPUTE       8, 8
    PEL_STORE8   dstq+32, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET

cglobal hevc_put_hevc_qpel_v12_10, 7, 14, 15 , dst, dststride, src, srcstride, height, r3src, my, rfilter
    lea           r3srcq, [srcstrideq*3]
    QPEL_FILTER       10, my
.loop
    QPEL_V_LOAD       10, srcq, srcstride, 12
    QPEL_COMPUTE       8, 10
    PEL_STORE8      dstq, m0, m1
    QPEL_V_LOAD       10, srcq+16, srcstride, 12
    QPEL_COMPUTE       4, 10
    PEL_STORE4   dstq+16, m0, m1
    LOOP_END         dst, dststride, src, srcstride
    RET


HEVC_PUT_HEVC_PEL_PIXELS  2, 8
HEVC_PUT_HEVC_PEL_PIXELS  4, 8
HEVC_PUT_HEVC_PEL_PIXELS  6, 8
HEVC_PUT_HEVC_PEL_PIXELS  8, 8
HEVC_PUT_HEVC_PEL_PIXELS 12, 8
HEVC_PUT_HEVC_PEL_PIXELS 16, 8

HEVC_PUT_HEVC_PEL_PIXELS 2, 10
HEVC_PUT_HEVC_PEL_PIXELS 4, 10
HEVC_PUT_HEVC_PEL_PIXELS 6, 10
HEVC_PUT_HEVC_PEL_PIXELS 8, 10


HEVC_PUT_HEVC_EPEL_H 2,  8
HEVC_PUT_HEVC_EPEL_H 4,  8
HEVC_PUT_HEVC_EPEL_H 6,  8
HEVC_PUT_HEVC_EPEL_H 8,  8
HEVC_PUT_HEVC_EPEL_H 12, 8
HEVC_PUT_HEVC_EPEL_H 16, 8

HEVC_PUT_HEVC_EPEL_H 2, 10
HEVC_PUT_HEVC_EPEL_H 4, 10
HEVC_PUT_HEVC_EPEL_H 6, 10
HEVC_PUT_HEVC_EPEL_H 8, 10


HEVC_PUT_HEVC_EPEL_V 2,  8
HEVC_PUT_HEVC_EPEL_V 4,  8
HEVC_PUT_HEVC_EPEL_V 6,  8
HEVC_PUT_HEVC_EPEL_V 8,  8
HEVC_PUT_HEVC_EPEL_V 12, 8
HEVC_PUT_HEVC_EPEL_V 16, 8

HEVC_PUT_HEVC_EPEL_V 2, 10
HEVC_PUT_HEVC_EPEL_V 4, 10
HEVC_PUT_HEVC_EPEL_V 6, 10
HEVC_PUT_HEVC_EPEL_V 8, 10


HEVC_PUT_HEVC_EPEL_HV 2,  8
HEVC_PUT_HEVC_EPEL_HV 4,  8
HEVC_PUT_HEVC_EPEL_HV 6,  8
HEVC_PUT_HEVC_EPEL_HV 8,  8

HEVC_PUT_HEVC_EPEL_HV 2, 10
HEVC_PUT_HEVC_EPEL_HV 4, 10
HEVC_PUT_HEVC_EPEL_HV 6, 10
HEVC_PUT_HEVC_EPEL_HV 8, 10


HEVC_PUT_HEVC_QPEL_H 4,  8
HEVC_PUT_HEVC_QPEL_H 8,  8
HEVC_PUT_HEVC_QPEL_H 12, 8
HEVC_PUT_HEVC_QPEL_H 16, 8

HEVC_PUT_HEVC_QPEL_H 4, 10
HEVC_PUT_HEVC_QPEL_H 8, 10


HEVC_PUT_HEVC_QPEL_V 4,  8
HEVC_PUT_HEVC_QPEL_V 8,  8
HEVC_PUT_HEVC_QPEL_V 12, 8
HEVC_PUT_HEVC_QPEL_V 16, 8

HEVC_PUT_HEVC_QPEL_V 4, 10
HEVC_PUT_HEVC_QPEL_V 8, 10

%endif ; ARCH_X86_64
