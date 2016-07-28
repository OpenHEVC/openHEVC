/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 - 2014 Pierre-Edouard Lepere
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "hevc.h"
#include "hevcdsp.h"

#if COM16_C806_EMT

#include "dct.h"

const int emt_Tr_Set_V[35] =
{
	2, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 1, 0, 1, 0, 1, 0
};
const int emt_Tr_Set_H[35] =
{
	2, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0
};
const int g_aiTrSubSetIntra[3][2] =
{
	{DST_VII, DCT_VIII},
	{DST_VII, DST_I},
	{DST_VII, DCT_V}
};
const int g_aiTrSubSetInter[2] =
{DCT_VIII, DST_VII};

InvTrans *fastInvTrans[7][5] =
{
  {fastInverseDCT2_B4, fastInverseDCT2_B8, fastInverseDCT2_B16, fastInverseDCT2_B32, fastInverseDCT2_B64},//DCT_II
  {NULL			     , NULL			     , NULL			      , NULL			   , NULL               },//DCT_III
  {NULL			     , NULL			     , NULL			      , NULL			   , NULL               },//DCT_I
  {fastInverseDST1_B4, fastInverseDST1_B8, fastInverseDST1_B16, fastInverseDST1_B32, NULL               },//DST_I
  {fastInverseDST7_B4, fastInverseDST7_B8, fastInverseDST7_B16, fastInverseDST7_B32, NULL               },//DST_VII
  {fastInverseDCT8_B4, fastInverseDCT8_B8, fastInverseDCT8_B16, fastInverseDCT8_B32, NULL               },//DCT_VIII
  {fastInverseDCT5_B4, fastInverseDCT5_B8, fastInverseDCT5_B16, fastInverseDCT5_B32, NULL               },//DCT_V
};

#define DEFINE_DST4x4_MATRIX(a,b,c,d) \
{ \
  {  a,  b,  c,  d }, \
  {  c,  c,  0, -c }, \
  {  d, -a, -c,  b }, \
  {  b, -d,  c, -a }, \
}

#define DEFINE_DCT4x4_MATRIX(a,b,c) \
{ \
  { a,  a,  a,  a}, \
  { b,  c, -c, -b}, \
  { a, -a, -a,  a}, \
  { c, -b,  b, -c}  \
}

#define DEFINE_DCT8x8_MATRIX(a,b,c,d,e,f,g) \
{ \
  { a,  a,  a,  a,  a,  a,  a,  a}, \
  { d,  e,  f,  g, -g, -f, -e, -d}, \
  { b,  c, -c, -b, -b, -c,  c,  b}, \
  { e, -g, -d, -f,  f,  d,  g, -e}, \
  { a, -a, -a,  a,  a, -a, -a,  a}, \
  { f, -d,  g,  e, -e, -g,  d, -f}, \
  { c, -b,  b, -c, -c,  b, -b,  c}, \
  { g, -f,  e, -d,  d, -e,  f, -g}  \
}

#define DEFINE_DCT16x16_MATRIX(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o) \
{ \
  { a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a}, \
  { h,  i,  j,  k,  l,  m,  n,  o, -o, -n, -m, -l, -k, -j, -i, -h}, \
  { d,  e,  f,  g, -g, -f, -e, -d, -d, -e, -f, -g,  g,  f,  e,  d}, \
  { i,  l,  o, -m, -j, -h, -k, -n,  n,  k,  h,  j,  m, -o, -l, -i}, \
  { b,  c, -c, -b, -b, -c,  c,  b,  b,  c, -c, -b, -b, -c,  c,  b}, \
  { j,  o, -k, -i, -n,  l,  h,  m, -m, -h, -l,  n,  i,  k, -o, -j}, \
  { e, -g, -d, -f,  f,  d,  g, -e, -e,  g,  d,  f, -f, -d, -g,  e}, \
  { k, -m, -i,  o,  h,  n, -j, -l,  l,  j, -n, -h, -o,  i,  m, -k}, \
  { a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a}, \
  { l, -j, -n,  h, -o, -i,  m,  k, -k, -m,  i,  o, -h,  n,  j, -l}, \
  { f, -d,  g,  e, -e, -g,  d, -f, -f,  d, -g, -e,  e,  g, -d,  f}, \
  { m, -h,  l,  n, -i,  k,  o, -j,  j, -o, -k,  i, -n, -l,  h, -m}, \
  { c, -b,  b, -c, -c,  b, -b,  c,  c, -b,  b, -c, -c,  b, -b,  c}, \
  { n, -k,  h, -j,  m,  o, -l,  i, -i,  l, -o, -m,  j, -h,  k, -n}, \
  { g, -f,  e, -d,  d, -e,  f, -g, -g,  f, -e,  d, -d,  e, -f,  g}, \
  { o, -n,  m, -l,  k, -j,  i, -h,  h, -i,  j, -k,  l, -m,  n, -o}  \
}

#define DEFINE_DCT32x32_MATRIX(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,A,B,C,D,E) \
{ \
  { a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a,  a}, \
  { p,  q,  r,  s,  t,  u,  v,  w,  x,  y,  z,  A,  B,  C,  D,  E, -E, -D, -C, -B, -A, -z, -y, -x, -w, -v, -u, -t, -s, -r, -q, -p}, \
  { h,  i,  j,  k,  l,  m,  n,  o, -o, -n, -m, -l, -k, -j, -i, -h, -h, -i, -j, -k, -l, -m, -n, -o,  o,  n,  m,  l,  k,  j,  i,  h}, \
  { q,  t,  w,  z,  C, -E, -B, -y, -v, -s, -p, -r, -u, -x, -A, -D,  D,  A,  x,  u,  r,  p,  s,  v,  y,  B,  E, -C, -z, -w, -t, -q}, \
  { d,  e,  f,  g, -g, -f, -e, -d, -d, -e, -f, -g,  g,  f,  e,  d,  d,  e,  f,  g, -g, -f, -e, -d, -d, -e, -f, -g,  g,  f,  e,  d}, \
  { r,  w,  B, -D, -y, -t, -p, -u, -z, -E,  A,  v,  q,  s,  x,  C, -C, -x, -s, -q, -v, -A,  E,  z,  u,  p,  t,  y,  D, -B, -w, -r}, \
  { i,  l,  o, -m, -j, -h, -k, -n,  n,  k,  h,  j,  m, -o, -l, -i, -i, -l, -o,  m,  j,  h,  k,  n, -n, -k, -h, -j, -m,  o,  l,  i}, \
  { s,  z, -D, -w, -p, -v, -C,  A,  t,  r,  y, -E, -x, -q, -u, -B,  B,  u,  q,  x,  E, -y, -r, -t, -A,  C,  v,  p,  w,  D, -z, -s}, \
  { b,  c, -c, -b, -b, -c,  c,  b,  b,  c, -c, -b, -b, -c,  c,  b,  b,  c, -c, -b, -b, -c,  c,  b,  b,  c, -c, -b, -b, -c,  c,  b}, \
  { t,  C, -y, -p, -x,  D,  u,  s,  B, -z, -q, -w,  E,  v,  r,  A, -A, -r, -v, -E,  w,  q,  z, -B, -s, -u, -D,  x,  p,  y, -C, -t}, \
  { j,  o, -k, -i, -n,  l,  h,  m, -m, -h, -l,  n,  i,  k, -o, -j, -j, -o,  k,  i,  n, -l, -h, -m,  m,  h,  l, -n, -i, -k,  o,  j}, \
  { u, -E, -t, -v,  D,  s,  w, -C, -r, -x,  B,  q,  y, -A, -p, -z,  z,  p,  A, -y, -q, -B,  x,  r,  C, -w, -s, -D,  v,  t,  E, -u}, \
  { e, -g, -d, -f,  f,  d,  g, -e, -e,  g,  d,  f, -f, -d, -g,  e,  e, -g, -d, -f,  f,  d,  g, -e, -e,  g,  d,  f, -f, -d, -g,  e}, \
  { v, -B, -p, -C,  u,  w, -A, -q, -D,  t,  x, -z, -r, -E,  s,  y, -y, -s,  E,  r,  z, -x, -t,  D,  q,  A, -w, -u,  C,  p,  B, -v}, \
  { k, -m, -i,  o,  h,  n, -j, -l,  l,  j, -n, -h, -o,  i,  m, -k, -k,  m,  i, -o, -h, -n,  j,  l, -l, -j,  n,  h,  o, -i, -m,  k}, \
  { w, -y, -u,  A,  s, -C, -q,  E,  p,  D, -r, -B,  t,  z, -v, -x,  x,  v, -z, -t,  B,  r, -D, -p, -E,  q,  C, -s, -A,  u,  y, -w}, \
  { a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a,  a, -a, -a,  a}, \
  { x, -v, -z,  t,  B, -r, -D,  p, -E, -q,  C,  s, -A, -u,  y,  w, -w, -y,  u,  A, -s, -C,  q,  E, -p,  D,  r, -B, -t,  z,  v, -x}, \
  { l, -j, -n,  h, -o, -i,  m,  k, -k, -m,  i,  o, -h,  n,  j, -l, -l,  j,  n, -h,  o,  i, -m, -k,  k,  m, -i, -o,  h, -n, -j,  l}, \
  { y, -s, -E,  r, -z, -x,  t,  D, -q,  A,  w, -u, -C,  p, -B, -v,  v,  B, -p,  C,  u, -w, -A,  q, -D, -t,  x,  z, -r,  E,  s, -y}, \
  { f, -d,  g,  e, -e, -g,  d, -f, -f,  d, -g, -e,  e,  g, -d,  f,  f, -d,  g,  e, -e, -g,  d, -f, -f,  d, -g, -e,  e,  g, -d,  f}, \
  { z, -p,  A,  y, -q,  B,  x, -r,  C,  w, -s,  D,  v, -t,  E,  u, -u, -E,  t, -v, -D,  s, -w, -C,  r, -x, -B,  q, -y, -A,  p, -z}, \
  { m, -h,  l,  n, -i,  k,  o, -j,  j, -o, -k,  i, -n, -l,  h, -m, -m,  h, -l, -n,  i, -k, -o,  j, -j,  o,  k, -i,  n,  l, -h,  m}, \
  { A, -r,  v, -E, -w,  q, -z, -B,  s, -u,  D,  x, -p,  y,  C, -t,  t, -C, -y,  p, -x, -D,  u, -s,  B,  z, -q,  w,  E, -v,  r, -A}, \
  { c, -b,  b, -c, -c,  b, -b,  c,  c, -b,  b, -c, -c,  b, -b,  c,  c, -b,  b, -c, -c,  b, -b,  c,  c, -b,  b, -c, -c,  b, -b,  c}, \
  { B, -u,  q, -x,  E,  y, -r,  t, -A, -C,  v, -p,  w, -D, -z,  s, -s,  z,  D, -w,  p, -v,  C,  A, -t,  r, -y, -E,  x, -q,  u, -B}, \
  { n, -k,  h, -j,  m,  o, -l,  i, -i,  l, -o, -m,  j, -h,  k, -n, -n,  k, -h,  j, -m, -o,  l, -i,  i, -l,  o,  m, -j,  h, -k,  n}, \
  { C, -x,  s, -q,  v, -A, -E,  z, -u,  p, -t,  y, -D, -B,  w, -r,  r, -w,  B,  D, -y,  t, -p,  u, -z,  E,  A, -v,  q, -s,  x, -C}, \
  { g, -f,  e, -d,  d, -e,  f, -g, -g,  f, -e,  d, -d,  e, -f,  g,  g, -f,  e, -d,  d, -e,  f, -g, -g,  f, -e,  d, -d,  e, -f,  g}, \
  { D, -A,  x, -u,  r, -p,  s, -v,  y, -B,  E,  C, -z,  w, -t,  q, -q,  t, -w,  z, -C, -E,  B, -y,  v, -s,  p, -r,  u, -x,  A, -D}, \
  { o, -n,  m, -l,  k, -j,  i, -h,  h, -i,  j, -k,  l, -m,  n, -o, -o,  n, -m,  l, -k,  j, -i,  h, -h,  i, -j,  k, -l,  m, -n,  o}, \
  { E, -D,  C, -B,  A, -z,  y, -x,  w, -v,  u, -t,  s, -r,  q, -p,  p, -q,  r, -s,  t, -u,  v, -w,  x, -y,  z, -A,  B, -C,  D, -E}  \
}

const int16_t g_aiT4[TRANSFORM_NUMBER_OF_DIRECTIONS][4][4]   =
{
  DEFINE_DCT4x4_MATRIX  (   64,    83,    36),
  DEFINE_DCT4x4_MATRIX  (   64,    83,    36)
};

const int16_t g_aiT8[TRANSFORM_NUMBER_OF_DIRECTIONS][8][8]   =
{
  DEFINE_DCT8x8_MATRIX  (   64,    83,    36,    89,    75,    50,    18),
  DEFINE_DCT8x8_MATRIX  (   64,    83,    36,    89,    75,    50,    18)
};

const int16_t g_aiT16[TRANSFORM_NUMBER_OF_DIRECTIONS][16][16] =
{
  DEFINE_DCT16x16_MATRIX(   64,    83,    36,    89,    75,    50,    18,    90,    87,    80,    70,    57,    43,    25,     9),
  DEFINE_DCT16x16_MATRIX(   64,    83,    36,    89,    75,    50,    18,    90,    87,    80,    70,    57,    43,    25,     9)
};

const int16_t g_aiT32[TRANSFORM_NUMBER_OF_DIRECTIONS][32][32] =
{
  DEFINE_DCT32x32_MATRIX(   64,    83,    36,    89,    75,    50,    18,    90,    87,    80,    70,    57,    43,    25,     9,    90,    90,    88,    85,    82,    78,    73,    67,    61,    54,    46,    38,    31,    22,    13,     4),
  DEFINE_DCT32x32_MATRIX(   64,    83,    36,    89,    75,    50,    18,    90,    87,    80,    70,    57,    43,    25,     9,    90,    90,    88,    85,    82,    78,    73,    67,    61,    54,    46,    38,    31,    22,    13,     4)
};

const int16_t g_as_DST_MAT_4[TRANSFORM_NUMBER_OF_DIRECTIONS][4][4] =
{
  DEFINE_DST4x4_MATRIX(   29,    55,    74,    84),
  DEFINE_DST4x4_MATRIX(   29,    55,    74,    84)
};

/*
 * Fast inverse DCT2 4-8-16-32-64
 */
void fastInverseDCT2_B4(int16_t *src, int16_t *dst, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int j;
  int E[2],O[2];
  int add = ( 1<<(shift-1) );

#if COM16_C806_EMT
  const int16_t *iT = use ? g_aiTr4[DCT_II][0] : g_aiT4[0][0];
#else
  const uint16_t *iT = g_aiT4[0][0];
#endif

  for (j=0; j<line; j++)
  {
    O[0] = iT[1*4+0]*src[line] + iT[3*4+0]*src[3*line];
    O[1] = iT[1*4+1]*src[line] + iT[3*4+1]*src[3*line];
    E[0] = iT[0*4+0]*src[0] + iT[2*4+0]*src[2*line];
    E[1] = iT[0*4+1]*src[0] + iT[2*4+1]*src[2*line];

    dst[0] = av_clip(((E[0] + O[0] + add)>>shift), outputMinimum, outputMaximum);
    dst[1] = av_clip(((E[1] + O[1] + add)>>shift), outputMinimum, outputMaximum);
    dst[2] = av_clip(((E[1] - O[1] + add)>>shift), outputMinimum, outputMaximum);
    dst[3] = av_clip(((E[0] - O[0] + add)>>shift), outputMinimum, outputMaximum);

    src   ++;
    dst += 4;
  }
}
void fastInverseDCT2_B8(int16_t *src, int16_t *dst, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int j,k;
  int E[4],O[4];
  int EE[2],EO[2];
  int add = ( 1<<(shift-1) );

#if COM16_C806_EMT
  const int16_t *iT = use ? g_aiTr8[DCT_II][0] : g_aiT8[0][0];
#else
  const uint16_t *iT = g_aiT8[0][0];
#endif

  for (j=0; j<line; j++)
  {
    for (k=0;k<4;k++)
    {
      O[k] = iT[ 1*8+k]*src[line] + iT[ 3*8+k]*src[3*line] + iT[ 5*8+k]*src[5*line] + iT[ 7*8+k]*src[7*line];
    }

    EO[0] = iT[2*8+0]*src[ 2*line ] + iT[6*8+0]*src[ 6*line ];
    EO[1] = iT[2*8+1]*src[ 2*line ] + iT[6*8+1]*src[ 6*line ];
    EE[0] = iT[0*8+0]*src[ 0      ] + iT[4*8+0]*src[ 4*line ];
    EE[1] = iT[0*8+1]*src[ 0      ] + iT[4*8+1]*src[ 4*line ];

    E[0] = EE[0] + EO[0];
    E[3] = EE[0] - EO[0];
    E[1] = EE[1] + EO[1];
    E[2] = EE[1] - EO[1];
    for (k=0;k<4;k++)
    {
      dst[k] = av_clip( ((E[k] + O[k] + add)>>shift), outputMinimum, outputMaximum);
      dst[k+4] = av_clip( ((E[3-k] - O[3-k] + add)>>shift), outputMinimum, outputMaximum);
    }
    src ++;
    dst += 8;
  }
}
void fastInverseDCT2_B16(int16_t *src, int16_t *dst, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int j,k;
  int E[8],O[8];
  int EE[4],EO[4];
  int EEE[2],EEO[2];
  int add = ( 1<<(shift-1) );

#if COM16_C806_EMT
  const int16_t *iT = use ? g_aiTr16[DCT_II][0] : g_aiT16[0][0];
#else
  const uint16_t *iT = g_aiT16[0][0];
#endif

  for (j=0; j<line; j++)
  {
    for (k=0;k<8;k++)
    {
      O[k] = iT[ 1*16+k]*src[ line] + iT[ 3*16+k]*src[ 3*line] + iT[ 5*16+k]*src[ 5*line] + iT[ 7*16+k]*src[ 7*line] + iT[ 9*16+k]*src[ 9*line] + iT[11*16+k]*src[11*line] + iT[13*16+k]*src[13*line] + iT[15*16+k]*src[15*line];
    }
    for (k=0;k<4;k++)
    {
      EO[k] = iT[ 2*16+k]*src[ 2*line] + iT[ 6*16+k]*src[ 6*line] + iT[10*16+k]*src[10*line] + iT[14*16+k]*src[14*line];
    }
    EEO[0] = iT[4*16]*src[ 4*line ] + iT[12*16]*src[ 12*line ];
    EEE[0] = iT[0]*src[ 0 ] + iT[ 8*16]*src[ 8*line ];
    EEO[1] = iT[4*16+1]*src[ 4*line ] + iT[12*16+1]*src[ 12*line ];
    EEE[1] = iT[0*16+1]*src[ 0 ] + iT[ 8*16+1]*src[ 8*line  ];

    for (k=0;k<2;k++)
    {
      EE[k] = EEE[k] + EEO[k];
      EE[k+2] = EEE[1-k] - EEO[1-k];
    }
    for (k=0;k<4;k++)
    {
      E[k] = EE[k] + EO[k];
      E[k+4] = EE[3-k] - EO[3-k];
    }
    for (k=0;k<8;k++)
    {
      dst[k] = av_clip( ((E[k] + O[k] + add)>>shift), outputMinimum, outputMaximum);
      dst[k+8] = av_clip( ((E[7-k] - O[7-k] + add)>>shift), outputMinimum, outputMaximum);
    }
    src ++;
    dst += 16;
  }
}
void fastInverseDCT2_B32(int16_t *src, int16_t *dst, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int j,k;
  int E[16],O[16];
  int EE[8],EO[8];
  int EEE[4],EEO[4];
  int EEEE[2],EEEO[2];
  int add = ( 1<<(shift-1) );

#if COM16_C806_EMT
  const int16_t *iT = use ? g_aiTr32[DCT_II][0] : g_aiT32[0][0];
#else
  const uint16_t *iT = g_aiT32[0][0];
#endif

  for (j=0; j<line; j++)
  {
    for (k=0;k<16;k++)
    {
      O[k] = iT[ 1*32+k]*src[ line  ] + iT[ 3*32+k]*src[ 3*line  ] + iT[ 5*32+k]*src[ 5*line  ] + iT[ 7*32+k]*src[ 7*line  ] +
        iT[ 9*32+k]*src[ 9*line  ] + iT[11*32+k]*src[ 11*line ] + iT[13*32+k]*src[ 13*line ] + iT[15*32+k]*src[ 15*line ] +
        iT[17*32+k]*src[ 17*line ] + iT[19*32+k]*src[ 19*line ] + iT[21*32+k]*src[ 21*line ] + iT[23*32+k]*src[ 23*line ] +
        iT[25*32+k]*src[ 25*line ] + iT[27*32+k]*src[ 27*line ] + iT[29*32+k]*src[ 29*line ] + iT[31*32+k]*src[ 31*line ];
    }
    for (k=0;k<8;k++)
    {
      EO[k] = iT[ 2*32+k]*src[ 2*line  ] + iT[ 6*32+k]*src[ 6*line  ] + iT[10*32+k]*src[ 10*line ] + iT[14*32+k]*src[ 14*line ] +
        iT[18*32+k]*src[ 18*line ] + iT[22*32+k]*src[ 22*line ] + iT[26*32+k]*src[ 26*line ] + iT[30*32+k]*src[ 30*line ];
    }
    for (k=0;k<4;k++)
    {
      EEO[k] = iT[4*32+k]*src[ 4*line ] + iT[12*32+k]*src[ 12*line ] + iT[20*32+k]*src[ 20*line ] + iT[28*32+k]*src[ 28*line ];
    }
    EEEO[0] = iT[8*32+0]*src[ 8*line ] + iT[24*32+0]*src[ 24*line ];
    EEEO[1] = iT[8*32+1]*src[ 8*line ] + iT[24*32+1]*src[ 24*line ];
    EEEE[0] = iT[0*32+0]*src[ 0      ] + iT[16*32+0]*src[ 16*line ];
    EEEE[1] = iT[0*32+1]*src[ 0      ] + iT[16*32+1]*src[ 16*line ];

    EEE[0] = EEEE[0] + EEEO[0];
    EEE[3] = EEEE[0] - EEEO[0];
    EEE[1] = EEEE[1] + EEEO[1];
    EEE[2] = EEEE[1] - EEEO[1];

    for (k=0;k<4;k++)
    {
      EE[k] = EEE[k] + EEO[k];
      EE[k+4] = EEE[3-k] - EEO[3-k];
    }
    for (k=0;k<8;k++)
    {
      E[k] = EE[k] + EO[k];
      E[k+8] = EE[7-k] - EO[7-k];
    }
    for (k=0;k<16;k++)
    {
      dst[k] = av_clip( ((E[k] + O[k] + add)>>shift), outputMinimum, outputMaximum);
      dst[k+16] = av_clip( ((E[15-k] - O[15-k] + add)>>shift), outputMinimum, outputMaximum);
    }
    src ++;
    dst += 32;
  }
}
void fastInverseDCT2_B64(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int rnd_factor = ( 1<<(shift-1) );
  const int uiTrSize = 64;
  const int16_t *iT = NULL;
  assert(0);

  int j, k;
  int E[32],O[32];
  int EE[16],EO[16];
  int EEE[8],EEO[8];
  int EEEE[4],EEEO[4];
  int EEEEE[2],EEEEO[2];
  for (j=0; j<(line>>(2==zo?1:0)); j++)
  {
    for (k=0;k<32;k++)
    {
      O[k] = iT[ 1*64+k]*coeff[ line  ] + iT[ 3*64+k]*coeff[ 3*line  ] + iT[ 5*64+k]*coeff[ 5*line  ] + iT[ 7*64+k]*coeff[ 7*line  ] +
        iT[ 9*64+k]*coeff[ 9*line  ] + iT[11*64+k]*coeff[ 11*line ] + iT[13*64+k]*coeff[ 13*line ] + iT[15*64+k]*coeff[ 15*line ] +
        iT[17*64+k]*coeff[ 17*line ] + iT[19*64+k]*coeff[ 19*line ] + iT[21*64+k]*coeff[ 21*line ] + iT[23*64+k]*coeff[ 23*line ] +
        iT[25*64+k]*coeff[ 25*line ] + iT[27*64+k]*coeff[ 27*line ] + iT[29*64+k]*coeff[ 29*line ] + iT[31*64+k]*coeff[ 31*line ] +
        ( zo ? 0 : (
        iT[33*64+k]*coeff[ 33*line ] + iT[35*64+k]*coeff[ 35*line ] + iT[37*64+k]*coeff[ 37*line ] + iT[39*64+k]*coeff[ 39*line ] +
        iT[41*64+k]*coeff[ 41*line ] + iT[43*64+k]*coeff[ 43*line ] + iT[45*64+k]*coeff[ 45*line ] + iT[47*64+k]*coeff[ 47*line ] +
        iT[49*64+k]*coeff[ 49*line ] + iT[51*64+k]*coeff[ 51*line ] + iT[53*64+k]*coeff[ 53*line ] + iT[55*64+k]*coeff[ 55*line ] +
        iT[57*64+k]*coeff[ 57*line ] + iT[59*64+k]*coeff[ 59*line ] + iT[61*64+k]*coeff[ 61*line ] + iT[63*64+k]*coeff[ 63*line ] ) );
    }
    for (k=0;k<16;k++)
    {
      EO[k] = iT[ 2*64+k]*coeff[ 2*line  ] + iT[ 6*64+k]*coeff[ 6*line  ] + iT[10*64+k]*coeff[ 10*line ] + iT[14*64+k]*coeff[ 14*line ] +
        iT[18*64+k]*coeff[ 18*line ] + iT[22*64+k]*coeff[ 22*line ] + iT[26*64+k]*coeff[ 26*line ] + iT[30*64+k]*coeff[ 30*line ] +
        ( zo ? 0 : (
        iT[34*64+k]*coeff[ 34*line ] + iT[38*64+k]*coeff[ 38*line ] + iT[42*64+k]*coeff[ 42*line ] + iT[46*64+k]*coeff[ 46*line ] +
        iT[50*64+k]*coeff[ 50*line ] + iT[54*64+k]*coeff[ 54*line ] + iT[58*64+k]*coeff[ 58*line ] + iT[62*64+k]*coeff[ 62*line ] ) );
    }
    for (k=0;k<8;k++)
    {
      EEO[k] = iT[4*64+k]*coeff[ 4*line ] + iT[12*64+k]*coeff[ 12*line ] + iT[20*64+k]*coeff[ 20*line ] + iT[28*64+k]*coeff[ 28*line ] +
        ( zo ? 0 : (
        iT[36*64+k]*coeff[ 36*line ] + iT[44*64+k]*coeff[ 44*line ] + iT[52*64+k]*coeff[ 52*line ] + iT[60*64+k]*coeff[ 60*line ] ) );
    }
    for (k=0;k<4;k++)
    {
      EEEO[k] = iT[8*64+k]*coeff[ 8*line ] + iT[24*64+k]*coeff[ 24*line ] + ( zo ? 0 : ( iT[40*64+k]*coeff[ 40*line ] + iT[56*64+k]*coeff[ 56*line ] ) );
    }
    EEEEO[0] = iT[16*64+0]*coeff[ 16*line ] + ( zo ? 0 : iT[48*64+0]*coeff[ 48*line ] );
    EEEEO[1] = iT[16*64+1]*coeff[ 16*line ] + ( zo ? 0 : iT[48*64+1]*coeff[ 48*line ] );
    EEEEE[0] = iT[ 0*64+0]*coeff[  0      ] + ( zo ? 0 : iT[32*64+0]*coeff[ 32*line ] );
    EEEEE[1] = iT[ 0*64+1]*coeff[  0      ] + ( zo ? 0 : iT[32*64+1]*coeff[ 32*line ] );

    for (k=0;k<2;k++)
    {
      EEEE[k] = EEEEE[k] + EEEEO[k];
      EEEE[k+2] = EEEEE[1-k] - EEEEO[1-k];
    }
    for (k=0;k<4;k++)
    {
      EEE[k] = EEEE[k] + EEEO[k];
      EEE[k+4] = EEEE[3-k] - EEEO[3-k];
    }
    for (k=0;k<8;k++)
    {
      EE[k] = EEE[k] + EEO[k];
      EE[k+8] = EEE[7-k] - EEO[7-k];
    }
    for (k=0;k<16;k++)
    {
      E[k] = EE[k] + EO[k];
      E[k+16] = EE[15-k] - EO[15-k];
    }
    for (k=0;k<32;k++)
    {
	  block[k] = av_clip( ((E[k] + O[k] + rnd_factor)>>shift), outputMinimum, outputMaximum);
	  block[k+32] = av_clip( ((E[31-k] - O[31-k] + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    coeff ++;
    block += uiTrSize;
  }
}
/*
 * End DCT2
 */

/*
 * Fast inverse DCT5 4-8-16-32
 */
void fastInverseDCT5_B4(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = 1<<(shift-1);

  const int16_t *iT = g_aiTr4[DCT_V][0];
  const int uiTrSize = 4;

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line]*iT[k*uiTrSize+j];
      }
      block[j] = av_clip(((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}
void fastInverseDCT5_B8(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = 1<<(shift-1);

  const int uiTrSize = 8;
  const int16_t *iT = g_aiTr8[DCT_V][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line]*iT[k*uiTrSize+j];
      }
      block[j] = av_clip(((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}
void fastInverseDCT5_B16(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = (1<<(shift-1));

  const int uiTrSize = 16;
  const int16_t *iT = g_aiTr16[DCT_V][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line]*iT[k*uiTrSize+j];
      }
      block[j] = av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}
void fastInverseDCT5_B32(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = (1<<(shift-1));

  const int uiTrSize = 32;
  const int16_t *iT = g_aiTr32[DCT_V][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += (coeff[k*line] * iT[k*uiTrSize+j]);
      }
      block[j] = av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}
/*
 * End DCT5
 */

/*
 * Fast inverse DCT8 4-8-16-32
 */
void fastInverseDCT8_B4(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i;
  int rnd_factor = ( 1<<(shift-1) );

  const int16_t *iT = g_aiTr4[DCT_VIII][0];

  int c[4];
  for (i=0; i<line; i++)
  {
    c[0] = coeff[ 0] + coeff[12];
    c[1] = coeff[ 8] + coeff[ 0];
    c[2] = coeff[12] - coeff[ 8];
    c[3] = iT[1]* coeff[4];

    block[0] = av_clip( ((iT[3] * c[0] + iT[2] * c[1] + c[3] + rnd_factor ) >> shift), outputMinimum, outputMaximum);
    block[1] = av_clip( ((iT[1] * (coeff[0] - coeff[8] - coeff[12]) + rnd_factor ) >> shift), outputMinimum, outputMaximum);
    block[2] = av_clip( ((iT[3] * c[2] + iT[2] * c[0] - c[3] + rnd_factor) >> shift), outputMinimum, outputMaximum);
    block[3] = av_clip( ((iT[3] * c[1] - iT[2] * c[2] - c[3] + rnd_factor ) >> shift), outputMinimum, outputMaximum);

    block+=4;
    coeff++;
  }
}

void fastInverseDCT8_B8(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 8;
  const int16_t *iT = g_aiTr8[DCT_VIII][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line]*iT[k*uiTrSize+j];
      }
      block[j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}

void fastInverseDCT8_B16(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 16;
  const int16_t *iT = g_aiTr16[DCT_VIII][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line]*iT[k*uiTrSize+j];
      }
      block[j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}

void fastInverseDCT8_B32(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 32;
  const int16_t *iT = g_aiTr32[DCT_VIII][0];

  if ( zo )
  {
    for (i=0; i<(line >> (zo-1) ); i++)
    {
      for (j=0; j<uiTrSize; j++)
      {
        iSum = 0;
        for (k=0; k<( uiTrSize / 2 ); k++)
        {
          iSum += coeff[k*line]*iT[k*uiTrSize+j];
        }
        block[j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
      }
      block+=uiTrSize;
      coeff++;
    }
    if( zo==2 )
    {
      memset( block, 0, sizeof(int16_t)*uiTrSize*uiTrSize/2 );
    }
  }
  else
  {
    for (i=0; i<line; i++)
    {
      for (j=0; j<uiTrSize; j++)
      {
        iSum = 0;
        for (k=0; k<uiTrSize; k++)
        {
          iSum += coeff[k*line]*iT[k*uiTrSize+j];
        }
        block[j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
      }
      block+=uiTrSize;
      coeff++;
    }
  }
}
/*
 * End DCT8
 */

/*
 * Fast inverse DST1 4-8-16-32
 */
void fastInverseDST1_B4(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i;
  int rnd_factor = ( 1<<(shift-1) );

  const int16_t *iT = g_aiTr4[DST_I][0];

  int E[2],O[2];
  for (i=0; i<line; i++)
  {
    E[0] = coeff[0*4] + coeff[3*4];
    O[0] = coeff[0*4] - coeff[3*4];
    E[1] = coeff[1*4] + coeff[2*4];
    O[1] = coeff[1*4] - coeff[2*4];

    block[0] = av_clip( ((E[0]*iT[0] + E[1]*iT[1] + rnd_factor)>>shift), outputMinimum, outputMaximum);
    block[1] = av_clip( ((O[0]*iT[1] + O[1]*iT[0] + rnd_factor)>>shift), outputMinimum, outputMaximum);
    block[2] = av_clip( ((E[0]*iT[1] - E[1]*iT[0] + rnd_factor)>>shift), outputMinimum, outputMaximum);
    block[3] = av_clip( ((O[0]*iT[0] - O[1]*iT[1] + rnd_factor)>>shift), outputMinimum, outputMaximum);

    block += 4;
    coeff ++;
  }
}
void fastInverseDST1_B8(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 8;
  const int16_t *iT = g_aiTr8[DST_I][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line+i]*iT[k*uiTrSize+j];
      }
      block[i*uiTrSize+j] = av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
  }
}
void fastInverseDST1_B16(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 16;
  const int16_t *iT = g_aiTr16[DST_I][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line+i]*iT[k*uiTrSize+j];
      }
      block[i*uiTrSize+j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
  }
}
void fastInverseDST1_B32(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
int i, j, k, iSum;
int rnd_factor = ( 1<<(shift-1) );

const int uiTrSize = 32;
const int16_t *iT = g_aiTr32[DST_I][0];

for (i=0; i<line; i++)
{
  for (j=0; j<uiTrSize; j++)
  {
	iSum = 0;
	for (k=0; k<uiTrSize; k++)
	{
	  iSum += coeff[k*line+i]*iT[k*uiTrSize+j];
	}
	block[i*uiTrSize+j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
  }
}
}
/*
 * End DCT8
 */

/*
 * Fast inverse DSTVII 4-8-16-32
 */
void fastInverseDST7_B4(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, c[4];
  int rnd_factor = ( 1<<(shift-1) );

#if COM16_C806_EMT
  const int16_t *iT = use ? g_aiTr4[DST_VII][0] : g_as_DST_MAT_4[0][0];
#else
  const uint16_t *iT = g_as_DST_MAT_4[0][0];
#endif

  for (i=0; i<line; i++)
  {
    c[0] = coeff[0] + coeff[ 8];
    c[1] = coeff[8] + coeff[12];
    c[2] = coeff[0] - coeff[12];
    c[3] = iT[2]* coeff[4];

    block[0] = av_clip( (( iT[0] * c[0] + iT[1] * c[1] + c[3] + rnd_factor ) >> shift), outputMinimum, outputMaximum);
    block[1] = av_clip( (( iT[1] * c[2] - iT[0] * c[1] + c[3] + rnd_factor ) >> shift), outputMinimum, outputMaximum);
    block[2] = av_clip( (( iT[2] * (coeff[0] - coeff[8]  + coeff[12]) + rnd_factor ) >> shift ), outputMinimum, outputMaximum);
    block[3] = av_clip( (( iT[1] * c[0] + iT[0] * c[2] - c[3] + rnd_factor ) >> shift ), outputMinimum, outputMaximum);

    block+=4;
    coeff++;
  }
}
void fastInverseDST7_B8(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 8;
  const int16_t *iT = g_aiTr8[DST_VII][0];

  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line]*iT[k*uiTrSize+j];
      }
      block[j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}
void fastInverseDST7_B16(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 16;
  const int16_t *iT = g_aiTr16[DST_VII][0];
  for (i=0; i<line; i++)
  {
    for (j=0; j<uiTrSize; j++)
    {
      iSum = 0;
      for (k=0; k<uiTrSize; k++)
      {
        iSum += coeff[k*line]*iT[k*uiTrSize+j];
      }
      block[j] =  av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
    }
    block+=uiTrSize;
    coeff++;
  }
}
void fastInverseDST7_B32(int16_t *coeff, int16_t *block, int shift, int line, int zo, int use, const int outputMinimum, const int outputMaximum)
{
  int i, j, k, iSum;
  int rnd_factor = ( 1<<(shift-1) );

  const int uiTrSize = 32;
  const int16_t *iT = g_aiTr32[DST_VII][0];

  if ( zo )
  {
    for (i=0; i<(line>>(zo-1)); i++)
    {
      for (j=0; j<uiTrSize; j++)
      {
        iSum = 0;
        for (k=0; k<( uiTrSize / 2 ); k++)
        {
          iSum += coeff[k*line]*iT[k*uiTrSize+j];
        }
        block[j] = av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
      }
      block+=uiTrSize;
      coeff++;
    }
  }
  else
  {
    for (i=0; i<line; i++)
    {
      for (j=0; j<uiTrSize; j++)
      {
        iSum = 0;
        for (k=0; k<uiTrSize; k++)
        {
          iSum += coeff[k*line]*iT[k*uiTrSize+j];
        }
        block[j] = av_clip( ((iSum + rnd_factor)>>shift), outputMinimum, outputMaximum);
      }
      block+=uiTrSize;
      coeff++;
    }
  }
}
/*
 * End DSTVII
 */
#endif


static const int8_t transform[32][32] = {
    { 64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
      64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },
    { 90,  90,  88,  85,  82,  78,  73,  67,  61,  54,  46,  38,  31,  22,  13,   4,
      -4, -13, -22, -31, -38, -46, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90 },
    { 90,  87,  80,  70,  57,  43,  25,   9,  -9, -25, -43, -57, -70, -80, -87, -90,
     -90, -87, -80, -70, -57, -43, -25,  -9,   9,  25,  43,  57,  70,  80,  87,  90 },
    { 90,  82,  67,  46,  22,  -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13,
      13,  38,  61,  78,  88,  90,  85,  73,  54,  31,   4, -22, -46, -67, -82, -90 },
    { 89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89,
      89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89 },
    { 88,  67,  31, -13, -54, -82, -90, -78, -46, -4,   38,  73,  90,  85,  61,  22,
     -22, -61, -85, -90, -73, -38,   4,  46,  78,  90,  82,  54,  13, -31, -67, -88 },
    { 87,  57,   9, -43, -80, -90, -70, -25,  25,  70,  90,  80,  43,  -9, -57, -87,
     -87, -57,  -9,  43,  80,  90,  70,  25, -25, -70, -90, -80, -43,   9,  57,  87 },
    { 85,  46, -13, -67, -90, -73, -22,  38,  82,  88,  54,  -4, -61, -90, -78, -31,
      31,  78,  90,  61,   4, -54, -88, -82, -38,  22,  73,  90,  67,  13, -46, -85 },
    { 83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83,
      83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83 },
    { 82,  22, -54, -90, -61,  13,  78,  85,  31, -46, -90, -67,   4,  73,  88,  38,
     -38, -88, -73,  -4,  67,  90,  46, -31, -85, -78, -13,  61,  90,  54, -22, -82 },
    { 80,   9, -70, -87, -25,  57,  90,  43, -43, -90, -57,  25,  87,  70,  -9, -80,
     -80,  -9,  70,  87,  25, -57, -90, -43,  43,  90,  57, -25, -87, -70,   9,  80 },
    { 78,  -4, -82, -73,  13,  85,  67, -22, -88, -61,  31,  90,  54, -38, -90, -46,
      46,  90,  38, -54, -90, -31,  61,  88,  22, -67, -85, -13,  73,  82,   4, -78 },
    { 75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75,
      75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75 },
    { 73, -31, -90, -22,  78,  67, -38, -90, -13,  82,  61, -46, -88,  -4,  85,  54,
     -54, -85,   4,  88,  46, -61, -82,  13,  90,  38, -67, -78,  22,  90,  31, -73 },
    { 70, -43, -87,   9,  90,  25, -80, -57,  57,  80, -25, -90,  -9,  87,  43, -70,
     -70,  43,  87,  -9, -90, -25,  80,  57, -57, -80,  25,  90,   9, -87, -43,  70 },
    { 67, -54, -78,  38,  85, -22, -90,   4,  90,  13, -88, -31,  82,  46, -73, -61,
      61,  73, -46, -82,  31,  88, -13, -90,  -4,  90,  22, -85, -38,  78,  54, -67 },
    { 64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,
      64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64 },
    { 61, -73, -46,  82,  31, -88, -13,  90,  -4, -90,  22,  85, -38, -78,  54,  67,
     -67, -54,  78,  38, -85, -22,  90,   4, -90,  13,  88, -31, -82,  46,  73, -61 },
    { 57, -80, -25,  90,  -9, -87,  43,  70, -70, -43,  87,   9, -90,  25,  80, -57,
     -57,  80,  25, -90,   9,  87, -43, -70,  70,  43, -87,  -9,  90, -25, -80,  57 },
    { 54, -85,  -4,  88, -46, -61,  82,  13, -90,  38,  67, -78, -22,  90, -31, -73,
      73,  31, -90,  22,  78, -67, -38,  90, -13, -82,  61,  46, -88,   4,  85, -54 },
    { 50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50,
      50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50 },
    { 46, -90,  38,  54, -90,  31,  61, -88,  22,  67, -85,  13,  73, -82,   4,  78,
     -78,  -4,  82, -73, -13,  85, -67, -22,  88, -61, -31,  90, -54, -38,  90, -46 },
    { 43, -90,  57,  25, -87,  70,   9, -80,  80,  -9, -70,  87, -25, -57,  90, -43,
     -43,  90, -57, -25,  87, -70,  -9,  80, -80,   9,  70, -87,  25,  57, -90,  43 },
    { 38, -88,  73,  -4, -67,  90, -46, -31,  85, -78,  13,  61, -90,  54,  22, -82,
      82, -22, -54,  90, -61, -13,  78, -85,  31,  46, -90,  67,   4, -73,  88, -38 },
    { 36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36,
      36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36 },
    { 31, -78,  90, -61,   4,  54, -88,  82, -38, -22,  73, -90,  67, -13, -46,  85,
     -85,  46,  13, -67,  90, -73,  22,  38, -82,  88, -54,  -4,  61, -90,  78, -31 },
    { 25, -70,  90, -80,  43,   9, -57,  87, -87,  57,  -9, -43,  80, -90,  70, -25,
     -25,  70, -90,  80, -43,  -9,  57, -87,  87, -57,   9,  43, -80,  90, -70,  25 },
    { 22, -61,  85, -90,  73, -38,  -4,  46, -78,  90, -82,  54, -13, -31,  67, -88,
      88, -67,  31,  13, -54,  82, -90,  78, -46,   4,  38, -73,  90, -85,  61, -22 },
    { 18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18,
      18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18 },
    { 13, -38,  61, -78,  88, -90,  85, -73,  54, -31,   4,  22, -46,  67, -82,  90,
     -90,  82, -67,  46, -22,  -4,  31, -54,  73, -85,  90, -88,  78, -61,  38, -13 },
    {  9, -25,  43, -57,  70, -80,  87, -90,  90, -87,  80, -70,  57, -43,  25, -9,
      -9,  25, -43,  57, -70,  80, -87,  90, -90,  87, -80,  70, -57,  43, -25,   9 },
    {  4, -13,  22, -31,  38, -46,  54, -61,  67, -73,  78, -82,  85, -88,  90, -90,
      90, -90,  88, -85,  82, -78,  73, -67,  61, -54,  46, -38,  31, -22,  13,  -4 },
};

#ifdef SVC_EXTENSION
/*      Upsampling filters      */
DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma[16][4])=
{
    {  0,  64,   0,  0},
    { -2,  62,   4,  0},
    { -2,  58,  10, -2},
    { -4,  56,  14, -2},
    { -4,  54,  16, -2},
    { -6,  52,  20, -2},
    { -6,  46,  28, -4},
    { -4,  42,  30, -4},
    { -4,  36,  36, -4},
    { -4,  30,  42, -4},
    { -4,  28,  46, -6},
    { -2,  20,  52, -6},
    { -2,  16,  54, -4},
    { -2,  14,  56, -4},
    { -2,  10,  58, -2},
    {  0,   4,  62, -2}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma[16][8] )=
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    {  0,  1,  -3,  63,   4,  -2,  1,  0},
    { -1,  2,  -5,  62,   8,  -3,  1,  0},
    { -1,  3,  -8,  60,  13,  -4,  1,  0},
    { -1,  4, -10,  58,  17,  -5,  1,  0},
    { -1,  4, -11,  52,  26,  -8,  3, -1},
    { -1,  3,  -9,  47,  31, -10,  4, -1},
    { -1,  4, -11,  45,  34, -10,  4, -1},
    { -1,  4, -11,  40,  40, -11,  4, -1},
    { -1,  4, -10,  34,  45, -11,  4, -1},
    { -1,  4, -10,  31,  47,  -9,  3, -1},
    { -1,  3,  -8,  26,  52, -11,  4, -1},
    {  0,  1,  -5,  17,  58, -10,  4, -1},
    {  0,  1,  -4,  13,  60,  -8,  3, -1},
    {  0,  1,  -3,   8,  62,  -5,  2, -1},
    {  0,  1,  -2,   4,  63,  -3,  1,  0}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x2[2][8] )= /*0 , 8 */
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    { -1,  4, -11,  40,  40, -11,  4, -1}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_luma_x1_5[3][8] )= /* 0, 11, 5 */
{
    {  0,  0,   0,  64,   0,   0,  0,  0},
    { -1,  3,  -8,  26,  52, -11,  4, -1},
    { -1,  4, -11,  52,  26,  -8,  3, -1}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x1_5[3][4])= /* 0, 11, 5 */
{
    {  0,  64,   0,  0},
    { -2,  20,  52, -6},
    { -6,  52,  20, -2}
};
DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_x1_5chroma[3][4])=
{
    {  0,   4,  62, -2},
    { -4,  30,  42, -4},
    { -4,  54,  16, -2}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x2[2][4])=
{
    {  0,  64,   0,  0},
    { -4,  36,  36, -4}
};

DECLARE_ALIGNED(16, static const int8_t, up_sample_filter_chroma_x2_v[2][4])=
{
    { -2,  10,  58, -2},
    { -6,  46,  28, -4},
};

#endif

DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_filters[7][4]) = {
    { -2, 58, 10, -2},
    { -4, 54, 16, -2},
    { -6, 46, 28, -4},
    { -4, 36, 36, -4},
    { -4, 28, 46, -6},
    { -2, 16, 54, -4},
    { -2, 10, 58, -2},
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_filters[3][16]) = {
    { -1,  4,-10, 58, 17, -5,  1,  0, -1,  4,-10, 58, 17, -5,  1,  0},
    { -1,  4,-11, 40, 40,-11,  4, -1, -1,  4,-11, 40, 40,-11,  4, -1},
    {  0,  1, -5, 17, 58,-10,  4, -1,  0,  1, -5, 17, 58,-10,  4, -1}
};

#define BIT_DEPTH 8
#include "hevcdsp_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 9
#include "hevcdsp_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 10
#include "hevcdsp_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 12
#include "hevcdsp_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 14
#include "hevcdsp_template.c"
#undef BIT_DEPTH

#if COM16_C806_EMT
int16_t g_aiTr4 [8/*NUM_TRANS_TYPE*/][ 4][ 4];
int16_t g_aiTr8 [8/*NUM_TRANS_TYPE*/][ 8][ 8];
int16_t g_aiTr16[8/*NUM_TRANS_TYPE*/][16][16];
int16_t g_aiTr32[8/*NUM_TRANS_TYPE*/][32][32];
#endif

void ff_hevc_dsp_init(HEVCDSPContext *hevcdsp, int bit_depth)
{
#if COM16_C806_EMT
int c = 4;
for ( int i=0; i<4; i++ )
	{
	short *iT = NULL;
	const double s = sqrt((double)c) * ( 64 << COM16_C806_TRANS_PREC );
	const double PI = 3.14159265358979323846;

	switch(i)
	  {
	  case 0: iT = g_aiTr4 [0][0]; break;
	  case 1: iT = g_aiTr8 [0][0]; break;
	  case 2: iT = g_aiTr16[0][0]; break;
	  case 3: iT = g_aiTr32[0][0]; break;
	  case 4: printf("Cas 4\n"); break;
	  }
	for( int k=0; k<c; k++ )
	  {
		for( int n=0; n<c; n++ )
		{
		  double w0, w1, v;

		  // DCT-II
		  w0 = k==0 ? sqrt(0.5) : 1;
		  v = cos(PI*(n+0.5)*k/c ) * w0 * sqrt(2.0/c);
		  iT[(DCT_II)*c*c + k*c + n] = (short) ( s * v + ( v > 0 ? 0.5 : -0.5) );

		  // DCT-V
		  w0 = ( k==0 ) ? sqrt(0.5) : 1.0;
		  w1 = ( n==0 ) ? sqrt(0.5) : 1.0;
		  v = cos(PI*n*k/(c-0.5)) * w0 * w1 * sqrt(2.0/(c-0.5));
		  iT[(DCT_V)*c*c + k*c + n] = (short) ( s * v + ( v > 0 ? 0.5 : -0.5) );
		  //printf("%f \t",v);

		  // DCT-VIII
		  v = cos(PI*(k+0.5)*(n+0.5)/(c+0.5) ) * sqrt(2.0/(c+0.5));
		  iT[(DCT_VIII)*c*c + k*c + n] = (short) ( s * v + ( v > 0 ? 0.5 : -0.5) );

		  // DST-I
		  v = sin(PI*(n+1)*(k+1)/(c+1)) * sqrt(2.0/(c+1));
		  iT[(DST_I)*c*c + k*c + n] = (short) ( s * v + ( v > 0 ? 0.5 : -0.5) );

		  // DST-VII
		  v = sin(PI*(k+0.5)*(n+1)/(c+0.5)) * sqrt(2.0/(c+0.5));
		  iT[(DST_VII)*c*c + k*c + n] = (short) ( s * v + ( v > 0 ? 0.5 : -0.5) );
		}
		//printf("\n\n");
	  }
	c <<= 1;
	}
#endif

#undef FUNC
#define FUNC(a, depth) a ## _ ## depth

#undef PEL_FUNC
#define PEL_FUNC(dst1, idx1, idx2, a, depth)                                   \
    for(i = 0 ; i < 10 ; i++)                                                  \
{                                                                              \
    hevcdsp->dst1[i][idx1][idx2] = a ## _ ## depth;                            \
}

#undef EPEL_FUNCS
#define EPEL_FUNCS(depth)                                                       \
    PEL_FUNC(put_hevc_epel, 0, 0, put_hevc_pel_pixels, depth);                  \
    PEL_FUNC(put_hevc_epel, 0, 1, put_hevc_epel_h, depth);                      \
    PEL_FUNC(put_hevc_epel, 1, 0, put_hevc_epel_v, depth);                      \
    PEL_FUNC(put_hevc_epel, 1, 1, put_hevc_epel_hv, depth)

#undef EPEL_UNI_FUNCS
#define EPEL_UNI_FUNCS(depth)                                                   \
    PEL_FUNC(put_hevc_epel_uni, 0, 0, put_hevc_pel_uni_pixels, depth);          \
    PEL_FUNC(put_hevc_epel_uni, 0, 1, put_hevc_epel_uni_h, depth);              \
    PEL_FUNC(put_hevc_epel_uni, 1, 0, put_hevc_epel_uni_v, depth);              \
    PEL_FUNC(put_hevc_epel_uni, 1, 1, put_hevc_epel_uni_hv, depth);             \
    PEL_FUNC(put_hevc_epel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);      \
    PEL_FUNC(put_hevc_epel_uni_w, 0, 1, put_hevc_epel_uni_w_h, depth);          \
    PEL_FUNC(put_hevc_epel_uni_w, 1, 0, put_hevc_epel_uni_w_v, depth);          \
    PEL_FUNC(put_hevc_epel_uni_w, 1, 1, put_hevc_epel_uni_w_hv, depth)

#undef EPEL_BI_FUNCS
#define EPEL_BI_FUNCS(depth)                                                    \
    PEL_FUNC(put_hevc_epel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);            \
    PEL_FUNC(put_hevc_epel_bi, 0, 1, put_hevc_epel_bi_h, depth);                \
    PEL_FUNC(put_hevc_epel_bi, 1, 0, put_hevc_epel_bi_v, depth);                \
    PEL_FUNC(put_hevc_epel_bi, 1, 1, put_hevc_epel_bi_hv, depth);               \
    PEL_FUNC(put_hevc_epel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);        \
    PEL_FUNC(put_hevc_epel_bi_w, 0, 1, put_hevc_epel_bi_w_h, depth);            \
    PEL_FUNC(put_hevc_epel_bi_w, 1, 0, put_hevc_epel_bi_w_v, depth);            \
    PEL_FUNC(put_hevc_epel_bi_w, 1, 1, put_hevc_epel_bi_w_hv, depth)

#undef QPEL_FUNCS
#define QPEL_FUNCS(depth)                                                         \
    PEL_FUNC(put_hevc_qpel, 0, 0, put_hevc_pel_pixels, depth);                    \
    PEL_FUNC(put_hevc_qpel, 0, 1, put_hevc_qpel_h, depth);                        \
    PEL_FUNC(put_hevc_qpel, 1, 0, put_hevc_qpel_v, depth);                        \
    PEL_FUNC(put_hevc_qpel, 1, 1, put_hevc_qpel_hv, depth)

#undef QPEL_UNI_FUNCS
#define QPEL_UNI_FUNCS(depth)                                                     \
    PEL_FUNC(put_hevc_qpel_uni, 0, 0, put_hevc_pel_uni_pixels, depth);            \
    PEL_FUNC(put_hevc_qpel_uni, 0, 1, put_hevc_qpel_uni_h, depth);                \
    PEL_FUNC(put_hevc_qpel_uni, 1, 0, put_hevc_qpel_uni_v, depth);                \
    PEL_FUNC(put_hevc_qpel_uni, 1, 1, put_hevc_qpel_uni_hv, depth);               \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);        \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 1, put_hevc_qpel_uni_w_h, depth);            \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 0, put_hevc_qpel_uni_w_v, depth);            \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 1, put_hevc_qpel_uni_w_hv, depth)

#undef QPEL_BI_FUNCS
#define QPEL_BI_FUNCS(depth)                                                      \
    PEL_FUNC(put_hevc_qpel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);              \
    PEL_FUNC(put_hevc_qpel_bi, 0, 1, put_hevc_qpel_bi_h, depth);                  \
    PEL_FUNC(put_hevc_qpel_bi, 1, 0, put_hevc_qpel_bi_v, depth);                  \
    PEL_FUNC(put_hevc_qpel_bi, 1, 1, put_hevc_qpel_bi_hv, depth);                 \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);          \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 1, put_hevc_qpel_bi_w_h, depth);              \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 0, put_hevc_qpel_bi_w_v, depth);              \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 1, put_hevc_qpel_bi_w_hv, depth)

#if COM16_C806_EMT
#define HEVC_DSP(depth)                                                            \
    hevcdsp->put_pcm                = FUNC(put_pcm, depth);                        \
    hevcdsp->transform_add[0]       = FUNC(transform_add4x4, depth);               \
    hevcdsp->transform_add[1]       = FUNC(transform_add8x8, depth);               \
    hevcdsp->transform_add[2]       = FUNC(transform_add16x16, depth);             \
    hevcdsp->transform_add[3]       = FUNC(transform_add32x32, depth);             \
    hevcdsp->transform_skip         = FUNC(transform_skip, depth);                 \
    hevcdsp->transform_rdpcm        = FUNC(transform_rdpcm, depth);                \
    hevcdsp->idct_4x4_luma          = FUNC(transform_4x4_luma, depth);             \
    																			   \
    hevcdsp->idct[0]                = FUNC(idct_4x4, depth);                       \
    hevcdsp->idct[1]                = FUNC(idct_8x8, depth);                       \
    hevcdsp->idct[2]                = FUNC(idct_16x16, depth);                     \
    hevcdsp->idct[3]                = FUNC(idct_32x32, depth);                     \
    hevcdsp->idct_dc[0]             = FUNC(idct_4x4_dc, depth);                    \
    hevcdsp->idct_dc[1]             = FUNC(idct_8x8_dc, depth);                    \
    hevcdsp->idct_dc[2]             = FUNC(idct_16x16_dc, depth);                  \
    hevcdsp->idct_dc[3]             = FUNC(idct_32x32_dc, depth);				   \
    hevcdsp->sao_band_filter   		= FUNC(sao_band_filter_0, depth);              \
    hevcdsp->sao_edge_filter[0] 	= FUNC(sao_edge_filter_0, depth);              \
    hevcdsp->sao_edge_filter[1] 	= FUNC(sao_edge_filter_1, depth);              \
    																			   \
    QPEL_FUNCS(depth);                                                             \
    QPEL_UNI_FUNCS(depth);                                                         \
    QPEL_BI_FUNCS(depth);                                                          \
    EPEL_FUNCS(depth);                                                             \
    EPEL_UNI_FUNCS(depth);                                                         \
    EPEL_BI_FUNCS(depth);                                                          \
                                                                                   \
    hevcdsp->hevc_h_loop_filter_luma     = FUNC(hevc_h_loop_filter_luma, depth);   \
    hevcdsp->hevc_v_loop_filter_luma     = FUNC(hevc_v_loop_filter_luma, depth);   \
    hevcdsp->hevc_h_loop_filter_chroma   = FUNC(hevc_h_loop_filter_chroma, depth); \
    hevcdsp->hevc_v_loop_filter_chroma   = FUNC(hevc_v_loop_filter_chroma, depth); \
    hevcdsp->hevc_h_loop_filter_luma_c   = FUNC(hevc_h_loop_filter_luma, depth);   \
    hevcdsp->hevc_v_loop_filter_luma_c   = FUNC(hevc_v_loop_filter_luma, depth);   \
    hevcdsp->hevc_h_loop_filter_chroma_c = FUNC(hevc_h_loop_filter_chroma, depth); \
    hevcdsp->hevc_v_loop_filter_chroma_c = FUNC(hevc_v_loop_filter_chroma, depth); \
	hevcdsp->idct_emt					 = FUNC(idct_emt, depth);                  
#else
#define HEVC_DSP(depth)                                                            \
	hevcdsp->put_pcm                = FUNC(put_pcm, depth);                        \
	hevcdsp->transform_add[0]       = FUNC(transform_add4x4, depth);               \
	hevcdsp->transform_add[1]       = FUNC(transform_add8x8, depth);               \
	hevcdsp->transform_add[2]       = FUNC(transform_add16x16, depth);             \
	hevcdsp->transform_add[3]       = FUNC(transform_add32x32, depth);             \
	hevcdsp->transform_skip         = FUNC(transform_skip, depth);                 \
	hevcdsp->transform_rdpcm        = FUNC(transform_rdpcm, depth);                \
	hevcdsp->idct_4x4_luma          = FUNC(transform_4x4_luma, depth);             \
																				   \
	hevcdsp->idct[0]                = FUNC(idct_4x4, depth);                       \
	hevcdsp->idct[1]                = FUNC(idct_8x8, depth);                       \
	hevcdsp->idct[2]                = FUNC(idct_16x16, depth);                     \
	hevcdsp->idct[3]                = FUNC(idct_32x32, depth);                     \
	hevcdsp->idct_dc[0]             = FUNC(idct_4x4_dc, depth);                    \
	hevcdsp->idct_dc[1]             = FUNC(idct_8x8_dc, depth);                    \
	hevcdsp->idct_dc[2]             = FUNC(idct_16x16_dc, depth);                  \
	hevcdsp->idct_dc[3]             = FUNC(idct_32x32_dc, depth);				   \
	hevcdsp->sao_band_filter   		= FUNC(sao_band_filter_0, depth);              \
	hevcdsp->sao_edge_filter[0] 	= FUNC(sao_edge_filter_0, depth);              \
	hevcdsp->sao_edge_filter[1] 	= FUNC(sao_edge_filter_1, depth);              \
																				   \
	QPEL_FUNCS(depth);                                                             \
	QPEL_UNI_FUNCS(depth);                                                         \
	QPEL_BI_FUNCS(depth);                                                          \
	EPEL_FUNCS(depth);                                                             \
	EPEL_UNI_FUNCS(depth);                                                         \
	EPEL_BI_FUNCS(depth);                                                          \
																				   \
	hevcdsp->hevc_h_loop_filter_luma     = FUNC(hevc_h_loop_filter_luma, depth);   \
	hevcdsp->hevc_v_loop_filter_luma     = FUNC(hevc_v_loop_filter_luma, depth);   \
	hevcdsp->hevc_h_loop_filter_chroma   = FUNC(hevc_h_loop_filter_chroma, depth); \
	hevcdsp->hevc_v_loop_filter_chroma   = FUNC(hevc_v_loop_filter_chroma, depth); \
	hevcdsp->hevc_h_loop_filter_luma_c   = FUNC(hevc_h_loop_filter_luma, depth);   \
	hevcdsp->hevc_v_loop_filter_luma_c   = FUNC(hevc_v_loop_filter_luma, depth);   \
	hevcdsp->hevc_h_loop_filter_chroma_c = FUNC(hevc_h_loop_filter_chroma, depth); \
	hevcdsp->hevc_v_loop_filter_chroma_c = FUNC(hevc_v_loop_filter_chroma, depth)
#endif

int i = 0;

    switch (bit_depth) {
    case 9:
        HEVC_DSP(9);
        break;
    case 10:
        HEVC_DSP(10);
        break;
    case 12:
        HEVC_DSP(12);
        break;
    case 14:
        HEVC_DSP(14);
        break;
    default:
        HEVC_DSP(8);
        break;
    }

#ifdef SVC_EXTENSION
#define HEVC_DSP_UP(depth)                                                 \
    hevcdsp->upsample_base_layer_frame       = FUNC(upsample_base_layer_frame, depth); \
    hevcdsp->upsample_filter_block_luma_h[0] = FUNC(upsample_filter_block_luma_h_all, depth); \
    hevcdsp->upsample_filter_block_luma_h[1] = FUNC(upsample_filter_block_luma_h_x2, depth); \
    hevcdsp->upsample_filter_block_luma_h[2] = FUNC(upsample_filter_block_luma_h_x1_5, depth); \
    hevcdsp->upsample_filter_block_luma_v[0] = FUNC(upsample_filter_block_luma_v_all, depth); \
    hevcdsp->upsample_filter_block_luma_v[1] = FUNC(upsample_filter_block_luma_v_x2, depth); \
    hevcdsp->upsample_filter_block_luma_v[2] = FUNC(upsample_filter_block_luma_v_x1_5, depth); \
    hevcdsp->upsample_filter_block_cr_h[0]   = FUNC(upsample_filter_block_cr_h_all, depth); \
    hevcdsp->upsample_filter_block_cr_h[1]   = FUNC(upsample_filter_block_cr_h_x2, depth); \
    hevcdsp->upsample_filter_block_cr_h[2]   = FUNC(upsample_filter_block_cr_h_x1_5, depth); \
    hevcdsp->upsample_filter_block_cr_v[0]   = FUNC(upsample_filter_block_cr_v_all, depth); \
    hevcdsp->upsample_filter_block_cr_v[1]   = FUNC(upsample_filter_block_cr_v_x2, depth); \
    hevcdsp->upsample_filter_block_cr_v[2]   = FUNC(upsample_filter_block_cr_v_x1_5, depth); \
    
    switch (bit_depth) {
    case 9:
        HEVC_DSP_UP(9);
        break;
    case 10:
        HEVC_DSP_UP(10);
        break;
    case 12:
        HEVC_DSP_UP(12);
        break;
    case 14:
        HEVC_DSP_UP(14);
        break;
    default:
        HEVC_DSP_UP(8);
        break;
    }
#endif
    if (ARCH_X86) ff_hevcdsp_init_x86(hevcdsp, bit_depth);
    if (ARCH_ARM) ff_hevcdsp_init_arm(hevcdsp, bit_depth);
}
