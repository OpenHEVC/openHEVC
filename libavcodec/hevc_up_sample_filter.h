/*
 * HEVC data tables
 *
 * Copyright (C) 2012 Guillaume Martres
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_HEVC_UP_SAMPLE_FILTER_H
#define AVCODEC_HEVC_UP_SAMPLE_FILTER_H

#include "hevc.h"
//#ifdef SVC_EXTENSION
#define NTAPS_LUMA 8
#define NTAPS_CHROMA 4
#define US_FILTER_PREC  6

#define MAX_EDGE  4
#define MAX_EDGE_CR  2
#define N_SHIFT (US_FILTER_PREC*2)
#define I_OFFSET (1 << (N_SHIFT - 1))

//#endif
#ifdef SVC_EXTENSION
#if PHASE_DERIVATION_IN_INTEGER
DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma[16][NTAPS_CHROMA])=
{
#if CHROMA_UPSAMPLING
    {  0, 64,  0,  0 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -4, 54, 16, -2 },
    { -6, 52, 20, -2 },
    { -6, 46, 28, -4 },
    { -1, -1, -1, -1 },
    { -4, 36, 36, -4 },
    { -4, 30, 42, -4 },
    { -1, -1, -1, -1 },
    { -2, 20, 52, -6 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -2, 10, 58, -2 },
    {  0,  4, 62, -2 }
};
#else
{  0, 64,  0,  0 },
{ -1, -1, -1, -1 },
{ -1, -1, -1, -1 },
{ -1, -1, -1, -1 },
{ -4, 54, 16, -2 },
{ -5, 50, 22, -3 },
{ -6, 46, 28, -4 },
{ -1, -1, -1, -1 },
{ -4, 36, 36, -4 },
{ -4, 30, 43, -5 },
{ -1, -1, -1, -1 },
{ -3, 22, 50, -5 },
{ -1, -1, -1, -1 },
{ -1, -1, -1, -1 },
{ -2, 10, 58, -2 },
{ -1,  5, 62, -2 }
#endif

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma[16][NTAPS_LUMA] )=
{
    {  0,   0,   0, 64,   0,   0,   0,   0 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   4, -11, 52,  26,  -8,   3,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  4,  -11, 40,  40,  -11,  4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   3,  -8, 26,  52, -11,   4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 }
};
#else
DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_luma[12][NTAPS_LUMA] )=
{
    {  0,   0,   0, 64,   0,   0,   0,   0 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   4, -11, 52,  26,  -8,   3,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  4,  -11, 40,  40,  -11,  4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   3,  -8, 26,  52, -11,   4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 }
};

DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_chroma15[12][NTAPS_CHROMA] )=
{
    {  0, 64,  0,  0 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -4, 54, 16, -2 },
    { -5, 50, 22, -3 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -4, 30, 43, -5 },
    { -3, 22, 50, -5 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -1,  5, 62, -2 }
};

DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_chroma20[8][NTAPS_CHROMA] )=
{
    {  0, 64,  0,  0 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -6, 46, 28, -4 },
    { -4, 36, 36, -4 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -2, 10, 58, -2 }
};
#endif

#if PHASE_DERIVATION_IN_INTEGER
DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_chroma32[16][NTAPS_CHROMA])=
{
#if CHROMA_UPSAMPLING
    {  0, 64,  0,  0 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -4, 54, 16, -2 },
    { -6, 52, 20, -2 },
    { -6, 46, 28, -4 },
    { -1, -1, -1, -1 },
    { -4, 36, 36, -4 },
    { -4, 30, 42, -4 },
    { -1, -1, -1, -1 },
    { -2, 20, 52, -6 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -2, 10, 58, -2 },
    {  0,  4, 62, -2 }
};
#else
{  0, 64,  0,  0 },
{ -1, -1, -1, -1 },
{ -1, -1, -1, -1 },
{ -1, -1, -1, -1 },
{ -4, 54, 16, -2 },
{ -5, 50, 22, -3 },
{ -6, 46, 28, -4 },
{ -1, -1, -1, -1 },
{ -4, 36, 36, -4 },
{ -4, 30, 43, -5 },
{ -1, -1, -1, -1 },
{ -3, 22, 50, -5 },
{ -1, -1, -1, -1 },
{ -1, -1, -1, -1 },
{ -2, 10, 58, -2 },
{ -1,  5, 62, -2 }
#endif

DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_luma32[16][NTAPS_LUMA] )=
{
    {  0,   0,   0, 64,   0,   0,   0,   0 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   4, -11, 52,  26,  -8,   3,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  4,  -11, 40,  40,  -11,  4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   3,  -8, 26,  52, -11,   4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 }
};
#else
DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_luma[12][NTAPS_LUMA] )=
{
    {  0,   0,   0, 64,   0,   0,   0,   0 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   4, -11, 52,  26,  -8,   3,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  4,  -11, 40,  40,  -11,  4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,   3,  -8, 26,  52, -11,   4,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 },
    { -1,  -1,  -1, -1,  -1,  -1,  -1,  -1 }
};

DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_chroma15[12][NTAPS_CHROMA] )=
{
    {  0, 64,  0,  0 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -4, 54, 16, -2 },
    { -5, 50, 22, -3 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -4, 30, 43, -5 },
    { -3, 22, 50, -5 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -1,  5, 62, -2 }
};

DECLARE_ALIGNED(16, static const int32_t, up_sample_filter_chroma20[8][NTAPS_CHROMA] )=
{
    {  0, 64,  0,  0 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -6, 46, 28, -4 },
    { -4, 36, 36, -4 },
    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },
    { -2, 10, 58, -2 }
};
#endif

#endif


#endif /* AVCODEC_HEVC_UP_SAMPLE_FILTER_H */
