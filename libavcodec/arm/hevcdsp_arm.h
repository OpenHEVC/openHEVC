/*
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

#ifndef AVCODEC_ARM_HEVCDSP_ARM_H
#define AVCODEC_ARM_HEVCDSP_ARM_H

#include "libavcodec/hevcdsp.h"

void ff_hevcdsp_init_neon(HEVCDSPContext *c, const int bit_depth);

/* hevcdsp_idct_neon_itr.c */
void oh_hevc_transform_16x16_8_neon(int16_t *coeffs, int col_limit);
void oh_hevc_transform_32x32_8_neon(int16_t *coeffs, int col_limit);

#endif /* AVCODEC_ARM_HEVCDSP_ARM_H */
