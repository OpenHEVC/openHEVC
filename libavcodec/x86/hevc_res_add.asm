; /*
; * Provide intrinsics for transform_add functions for HEVC decoding
; * Copyright (c) 2014 Pierre-Edouard LEPERE
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

SECTION_RODATA 32
max_pixels_10:          times 16  dw ((1 << 10)-1)
tr_add_10:              times 4 dd ((1 << 14-10) + 1)


SECTION .text

;the tr_add macros and functions were largely inspired by x264 project's code in the h264_idct.asm file
%macro TR_ADD_INIT_MMX 2
    mova              m2, [r1]
    mova              m4, [r1+8]
    pxor              m3, m3
    psubw             m3, m2
    packuswb          m2, m2
    packuswb          m3, m3
    pxor              m5, m5
    psubw             m5, m4
    packuswb          m4, m4
    packuswb          m5, m5
%endmacro

%macro TR_ADD_OP_MMX 4
    %1                m0, [%2     ]
    %1                m1, [%2+%3  ]
    paddusb           m0, m2
    paddusb           m1, m4
    psubusb           m0, m3
    psubusb           m1, m5
    %1         [%2     ], m0
    %1         [%2+%3  ], m1
%endmacro

%macro TR_ADD_INIT_SSE_8 2
    mova              m4, [r1]
    mova              m6, [r1+16]
    mova              m8, [r1+32]
    mova             m10, [r1+48]
    lea               %1, [%2*3]
    pxor              m5, m5
    psubw             m5, m4
    packuswb          m4, m4
    packuswb          m5, m5
    pxor              m7, m7
    psubw             m7, m6
    packuswb          m6, m6
    packuswb          m7, m7
    pxor              m9, m9
    psubw             m9, m8
    packuswb          m8, m8
    packuswb          m9, m9
    pxor             m11, m11
    psubw            m11, m10
    packuswb         m10, m10
    packuswb         m11, m11
%endmacro

%macro TR_ADD_INIT_SSE_16 2
    lea               %1, [%2*3]
    mova              m4, [r1]
    mova              m6, [r1+16]
    pxor              m5, m5
    psubw             m7, m5, m6
    psubw             m5, m4
    packuswb          m4, m6
    packuswb          m5, m7

    mova              m6, [r1+32]
    mova              m8, [r1+48]
    pxor              m7, m7
    psubw             m9, m7, m8
    psubw             m7, m6
    packuswb          m6, m8
    packuswb          m7, m9

    mova              m8, [r1+64]
    mova             m10, [r1+80]
    pxor              m9, m9
    psubw            m11, m9, m10
    psubw             m9, m8
    packuswb          m8, m10
    packuswb          m9, m11

    mova             m10, [r1+96]
    mova             m12, [r1+112]
    pxor             m11, m11
    psubw            m13, m11, m12
    psubw            m11, m10
    packuswb         m10, m12
    packuswb         m11, m13
%endmacro

%macro TR_ADD_OP_SSE 4
    %1                m0, [%2     ]
    %1                m1, [%2+%3  ]
    %1                m2, [%2+%3*2]
    %1                m3, [%2+%4  ]
    paddusb           m0, m4
    paddusb           m1, m6
    paddusb           m2, m8
    paddusb           m3, m10
    psubusb           m0, m5
    psubusb           m1, m7
    psubusb           m2, m9
    psubusb           m3, m11
    %1         [%2     ], m0
    %1         [%2+%3  ], m1
    %1         [%2+2*%3], m2
    %1         [%2+%4  ], m3
%endmacro

%macro TR_ADD_OP_SSE_32 4
    %1                m0, [%2      ]
    %1                m1, [%2+16   ]
    %1                m2, [%2+%3   ]
    %1                m3, [%2+%3+16]
    paddusb           m0, m4
    paddusb           m1, m6
    paddusb           m2, m8
    paddusb           m3, m10
    psubusb           m0, m5
    psubusb           m1, m7
    psubusb           m2, m9
    psubusb           m3, m11
    %1        [%2      ], m0
    %1        [%2+16   ], m1
    %1        [%2+%3   ], m2
    %1        [%2+%3+16], m3
%endmacro


INIT_MMX mmxext
; void ff_hevc_tranform_add_8_mmxext(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
cglobal hevc_transform_add4_8, 3, 4, 6
    TR_ADD_INIT_MMX   r3, r2
    TR_ADD_OP_MMX   movh, r0, r2, r3
    lea               r1, [r1+16]
    lea               r0, [r0+r2*2]
    TR_ADD_INIT_MMX   r3, r2
    TR_ADD_OP_MMX   movh, r0, r2, r3
    RET

%if ARCH_X86_64
INIT_XMM sse2
; void ff_hevc_transform_add8_8_sse2(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
cglobal hevc_transform_add8_8, 3, 4, 6
    TR_ADD_INIT_SSE_8 r3, r2
    TR_ADD_OP_SSE   movh, r0, r2, r3
    lea               r1, [r1+8*8]
    lea               r0, [r0+r2*4]
    TR_ADD_INIT_SSE_8 r3, r2
    TR_ADD_OP_SSE   movh, r0, r2, r3
    RET


; void ff_hevc_transform_add16_8_sse2(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
cglobal hevc_transform_add16_8, 3, 4, 6
    TR_ADD_INIT_SSE_16 r3, r2
    TR_ADD_OP_SSE    mova, r0, r2, r3
%rep 3
    lea                r1, [r1+16*8]
    lea                r0, [r0+r2*4]
    TR_ADD_INIT_SSE_16 r3, r2
    TR_ADD_OP_SSE    mova, r0, r2, r3
%endrep
    RET

; void ff_hevc_transform_add16_8_sse2(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride)
cglobal hevc_transform_add32_8, 3, 4, 6
    TR_ADD_INIT_SSE_16 r3, r2
    TR_ADD_OP_SSE_32 mova, r0, r2, r3
%rep 15
    lea                r1, [r1+16*8]
    lea                r0, [r0+r2*2]
    TR_ADD_INIT_SSE_16 r3, r2
    TR_ADD_OP_SSE_32 mova, r0, r2, r3
%endrep
    RET

%endif ;ARCH_X86_64
;-----------------------------------------------------------------------------
; void ff_hevc_transform_add_10(pixel *dst, int16_t *block, int stride)
;-----------------------------------------------------------------------------
%macro TR_ADD_OP_10 4
    mova              m0, [%4]
    mova              m1, [%4+16]
    mova              m2, [%4+32]
    mova              m3, [%4+48]
    paddw             m0, [%1+0   ]
    paddw             m1, [%1+%2  ]
    paddw             m2, [%1+%2*2]
    paddw             m3, [%1+%3  ]
    CLIPW             m0, m4, m5
    CLIPW             m1, m4, m5
    CLIPW             m2, m4, m5
    CLIPW             m3, m4, m5
    mova       [%1+0   ], m0
    mova       [%1+%2  ], m1
    mova       [%1+%2*2], m2
    mova       [%1+%3  ], m3
%endmacro

%macro TR_ADD_MMX_10 3
    mova              m4, [%3]
    mova              m5, [%3+8]
    mova              m0, [%1+0   ]
    mova              m1, [%1+%2  ]
    paddw             m0, m4
    paddw             m1, m5
    CLIPW             m0, m2, m3
    CLIPW             m1, m2, m3
    mova       [%1+0   ], m0
    mova       [%1+%2  ], m1
%endmacro

%macro TRANS_ADD16_10 3
    mova              m0, [%3]
    mova              m1, [%3+16]
    mova              m2, [%3+32]
    mova              m3, [%3+48]
    paddw             m0, [%1      ]
    paddw             m1, [%1+16   ]
    paddw             m2, [%1+%2   ]
    paddw             m3, [%1+%2+16]
    CLIPW             m0, m4, m5
    CLIPW             m1, m4, m5
    CLIPW             m2, m4, m5
    CLIPW             m3, m4, m5
    mova      [%1      ], m0
    mova      [%1+16   ], m1
    mova      [%1+%2   ], m2
    mova      [%1+%2+16], m3
%endmacro

%macro TRANS_ADD32_10 2
    mova              m0, [%2]
    mova              m1, [%2+16]
    mova              m2, [%2+32]
    mova              m3, [%2+48]

    paddw             m0, [%1   ]
    paddw             m1, [%1+16]
    paddw             m2, [%1+32]
    paddw             m3, [%1+48]
    CLIPW             m0, m4, m5
    CLIPW             m1, m4, m5
    CLIPW             m2, m4, m5
    CLIPW             m3, m4, m5
    mova         [%1   ], m0
    mova         [%1+16], m1
    mova         [%1+32], m2
    mova         [%1+48], m3
%endmacro

%macro TRANS_ADD16_AVX2 4
    mova              m0, [%4]
    mova              m1, [%4+32]
    mova              m2, [%4+64]
    mova              m3, [%4+96]

    paddw             m0, [%1+0   ]
    paddw             m1, [%1+%2  ]
    paddw             m2, [%1+%2*2]
    paddw             m3, [%1+%3  ]

    CLIPW             m0, m4, m5
    CLIPW             m1, m4, m5
    CLIPW             m2, m4, m5
    CLIPW             m3, m4, m5
    mova       [%1+0   ], m0
    mova       [%1+%2  ], m1
    mova       [%1+%2*2], m2
    mova       [%1+%3  ], m3
%endmacro

%macro TRANS_ADD32_AVX2 3
    mova              m0, [%3]
    mova              m1, [%3+32]
    mova              m2, [%3+64]
    mova              m3, [%3+96]

    paddw             m0, [%1      ]
    paddw             m1, [%1+32   ]
    paddw             m2, [%1+%2   ]
    paddw             m3, [%1+%2+32]

    CLIPW             m0, m4, m5
    CLIPW             m1, m4, m5
    CLIPW             m2, m4, m5
    CLIPW             m3, m4, m5
    mova      [%1      ], m0
    mova      [%1+32   ], m1
    mova      [%1+%2   ], m2
    mova      [%1+%2+32], m3
%endmacro


INIT_MMX mmxext
cglobal hevc_transform_add4_10,3,4, 6
    pxor              m2, m2
    mova              m3, [max_pixels_10]
    TR_ADD_MMX_10     r0, r2, r1
    lea               r1, [r1+16]
    lea               r0, [r0+2*r2]
    TR_ADD_MMX_10     r0, r2, r1
    RET

;-----------------------------------------------------------------------------
; void ff_hevc_transform_add_10(pixel *dst, int16_t *block, int stride)
;-----------------------------------------------------------------------------
%macro TR_ADD8 0
cglobal hevc_transform_add8_10,3,4,7
    pxor              m4, m4
    mova              m5, [max_pixels_10]
    lea               r3, [r2*3]

    TR_ADD_OP_10      r0, r2, r3, r1
    lea               r0, [r0+r2*4]
    lea               r1, [r1+64]
    TR_ADD_OP_10      r0, r2, r3, r1
    RET
%endmacro

%macro TRANS_ADD16 0
cglobal hevc_transform_add16_10,3,4,7
    pxor              m4, m4
    mova              m5, [max_pixels_10]

    TRANS_ADD16_10    r0, r2, r1
%rep 7
    lea               r0, [r0+r2*2]
    lea               r1, [r1+64]
    TRANS_ADD16_10    r0, r2, r1
%endrep
    RET
%endmacro


%macro TRANS_ADD32 0
cglobal hevc_transform_add32_10,3,4,7
    pxor              m4, m4
    mova              m5, [max_pixels_10]

    TRANS_ADD32_10    r0, r1
%rep 31
    lea               r0, [r0+r2]
    lea               r1, [r1+64]
    TRANS_ADD32_10    r0, r1
%endrep
    RET
%endmacro
%if ARCH_X86_64
INIT_XMM sse2
TR_ADD8
TRANS_ADD16
TRANS_ADD32

%if HAVE_AVX2_EXTERNAL
INIT_YMM avx2

cglobal hevc_transform_add16_10,3,4,10
    pxor              m4, m4
    mova              m5, [max_pixels_10]
    lea               r3, [r2*3]

    TRANS_ADD16_AVX2  r0, r2, r3, r1
%rep 3
    lea               r0, [r0+r2*4]
    lea               r1, [r1+128]
    TRANS_ADD16_AVX2  r0, r2, r3, r1
%endrep
    RET

cglobal hevc_transform_add32_10,3,4,10
    pxor              m4, m4
    mova              m5, [max_pixels_10]

    TRANS_ADD32_AVX2  r0, r2, r1
%rep 15
    lea               r0, [r0+r2*2]
    lea               r1, [r1+128]
    TRANS_ADD32_AVX2  r0, r2, r1
%endrep
    RET
%endif ;HAVE_AVX_EXTERNAL
%endif ;ARCH_X86_64

