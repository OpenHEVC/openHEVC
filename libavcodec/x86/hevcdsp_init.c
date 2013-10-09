/*
 * Copyright (c) 2013 Seppo Tomperi
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

#include "config.h"
#include "libavutil/cpu.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include "libavcodec/get_bits.h" /* required for hevcdsp.h GetBitContext */
#include "libavcodec/hevcdsp.h"
#include "libavcodec/x86/hevcdsp.h"


/***********************************/
/* deblocking */

#define LFC_FUNC(DIR, DEPTH, OPT)                                        \
void ff_hevc_ ## DIR ## _loop_filter_chroma_ ## DEPTH ## _ ## OPT(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

#define LFL_FUNC(DIR, DEPTH, OPT)                                        \
void ff_hevc_ ## DIR ## _loop_filter_luma_ ## DEPTH ## _ ## OPT(uint8_t *_pix, ptrdiff_t stride, int *_beta, int *_tc, \
uint8_t *_no_p, uint8_t *_no_q);

#define LFC_FUNCS(type, depth) \
LFC_FUNC(h, depth, sse2)  \
LFC_FUNC(v, depth, sse2)

#define LFL_FUNCS(type, depth) \
LFL_FUNC(h, depth, ssse3)  \
LFL_FUNC(v, depth, ssse3)


LFC_FUNCS(uint8_t,   8)
LFL_FUNCS(uint8_t,   8)

//LF_FUNCS(uint16_t, 10)


void ff_hevcdsp_init_x86(HEVCDSPContext *c, const int bit_depth)
{
    int mm_flags = av_get_cpu_flags();

    if (bit_depth == 8) {
        if (EXTERNAL_MMX(mm_flags)) {
            /*if (mm_flags & AV_CPU_FLAG_CMOV)
                c->h264_luma_dc_dequant_idct = ff_h264_luma_dc_dequant_idct_mmx; */

            if (EXTERNAL_MMXEXT(mm_flags)) {
#if ARCH_X86_32 && HAVE_MMXEXT_EXTERNAL
                /* MMEXT optimizations */
#endif /* ARCH_X86_32 && HAVE_MMXEXT_EXTERNAL */

                if (EXTERNAL_SSE2(mm_flags)) {
                    c->hevc_v_loop_filter_chroma = ff_hevc_v_loop_filter_chroma_8_sse2;
                    c->hevc_h_loop_filter_chroma = ff_hevc_h_loop_filter_chroma_8_sse2;
                }
                if (EXTERNAL_SSSE3(mm_flags)) {

                    c->transform_4x4_luma_add = ff_hevc_transform_4x4_luma_add_8_sse4;

                    c->transform_add[0] = ff_hevc_transform_4x4_add_8_sse4;
                    c->transform_add[1] = ff_hevc_transform_8x8_add_8_sse4;
                    c->transform_add[2] = ff_hevc_transform_16x16_add_8_sse4;
                    c->transform_add[3] = ff_hevc_transform_32x32_add_8_sse4;

                    c->put_unweighted_pred = ff_hevc_put_unweighted_pred_8_sse;

                    c->put_hevc_qpel[0][0] = ff_hevc_put_hevc_qpel_pixels_8_sse;
                    c->put_hevc_qpel[0][1] = ff_hevc_put_hevc_qpel_h_1_8_sse;
                    c->put_hevc_qpel[0][2] = ff_hevc_put_hevc_qpel_h_2_8_sse;
                    c->put_hevc_qpel[0][3] = ff_hevc_put_hevc_qpel_h_3_8_sse;
                    c->put_hevc_qpel[1][0] = ff_hevc_put_hevc_qpel_v_1_8_sse;
                    c->put_hevc_qpel[2][0] = ff_hevc_put_hevc_qpel_v_2_8_sse;
                    c->put_hevc_qpel[3][0] = ff_hevc_put_hevc_qpel_v_3_8_sse;



#if ARCH_X86_64
                    c->hevc_v_loop_filter_luma = ff_hevc_v_loop_filter_luma_8_ssse3;
                    c->hevc_h_loop_filter_luma = ff_hevc_h_loop_filter_luma_8_ssse3;
#endif

                }
                if (EXTERNAL_SSE4(mm_flags)) {
#if GCC_VERSION > MIN_GCC_VERSION_MC || __APPLE__
                	c->put_weighted_pred_avg = ff_hevc_put_weighted_pred_avg_8_sse;
                	c->weighted_pred = ff_hevc_weighted_pred_8_sse;
                	c->weighted_pred_avg = ff_hevc_weighted_pred_avg_8_sse;


                	c->put_hevc_epel[0][0] = ff_hevc_put_hevc_epel_pixels_8_sse;
                	c->put_hevc_epel[0][1] = ff_hevc_put_hevc_epel_h_8_sse;
                	c->put_hevc_epel[1][0] = ff_hevc_put_hevc_epel_v_8_sse;
                	c->put_hevc_epel[1][1] = ff_hevc_put_hevc_epel_hv_8_sse;

#endif
                	c->sao_edge_filter[0] = ff_hevc_sao_edge_filter_0_8_sse;
                	c->sao_edge_filter[1] = ff_hevc_sao_edge_filter_1_8_sse;
                	c->sao_edge_filter[2] = ff_hevc_sao_edge_filter_2_8_sse;
                	c->sao_edge_filter[3] = ff_hevc_sao_edge_filter_3_8_sse;

                	c->sao_band_filter[0] = ff_hevc_sao_band_filter_0_8_sse;
                	c->sao_band_filter[1] = ff_hevc_sao_band_filter_1_8_sse;
                	c->sao_band_filter[2] = ff_hevc_sao_band_filter_2_8_sse;
                	c->sao_band_filter[3] = ff_hevc_sao_band_filter_3_8_sse;

                    c->put_hevc_qpel[1][1] = ff_hevc_put_hevc_qpel_h_1_v_1_sse;
                    c->put_hevc_qpel[1][2] = ff_hevc_put_hevc_qpel_h_2_v_1_sse;
                    c->put_hevc_qpel[1][3] = ff_hevc_put_hevc_qpel_h_3_v_1_sse;
                    c->put_hevc_qpel[2][1] = ff_hevc_put_hevc_qpel_h_1_v_2_sse;
                    c->put_hevc_qpel[2][2] = ff_hevc_put_hevc_qpel_h_2_v_2_sse;
                    c->put_hevc_qpel[2][3] = ff_hevc_put_hevc_qpel_h_3_v_2_sse;
                    c->put_hevc_qpel[3][1] = ff_hevc_put_hevc_qpel_h_1_v_3_sse;
                    c->put_hevc_qpel[3][2] = ff_hevc_put_hevc_qpel_h_2_v_3_sse;
                    c->put_hevc_qpel[3][3] = ff_hevc_put_hevc_qpel_h_3_v_3_sse;


                }
                if (EXTERNAL_AVX(mm_flags)) {
                }
            }
        }
    } else if (bit_depth == 10) {
        if (EXTERNAL_MMX(mm_flags)) {
            if (EXTERNAL_MMXEXT(mm_flags)) {
#if ARCH_X86_32
#endif /* ARCH_X86_32 */
                if (EXTERNAL_SSE2(mm_flags)) {
#if HAVE_ALIGNED_STACK
                    /*stuff that requires aligned stack */
#endif /* HAVE_ALIGNED_STACK */
                }
                if (EXTERNAL_SSE4(mm_flags)) {

                    c->transform_4x4_luma_add = ff_hevc_transform_4x4_luma_add_10_sse4;

                    c->transform_add[0] = ff_hevc_transform_4x4_add_10_sse4;
                    c->transform_add[1] = ff_hevc_transform_8x8_add_10_sse4;
                    c->transform_add[2] = ff_hevc_transform_16x16_add_10_sse4;
                    c->transform_add[3] = ff_hevc_transform_32x32_add_10_sse4;

                    c->put_hevc_epel[0][0] = ff_hevc_put_hevc_epel_pixels_10_sse;
                    c->put_hevc_epel[0][1] = ff_hevc_put_hevc_epel_h_10_sse;
                    c->put_hevc_epel[1][0] = ff_hevc_put_hevc_epel_v_10_sse;
                    c->put_hevc_epel[1][1] = ff_hevc_put_hevc_epel_hv_10_sse;

                    c->put_hevc_qpel[0][0] = ff_hevc_put_hevc_qpel_pixels_10_sse;
                    c->put_hevc_qpel[0][1] = ff_hevc_put_hevc_qpel_h_1_10_sse;
                    c->put_hevc_qpel[1][0] = ff_hevc_put_hevc_qpel_v_1_10_sse;
                    c->put_hevc_qpel[2][0] = ff_hevc_put_hevc_qpel_v_2_10_sse;
                    c->put_hevc_qpel[3][0] = ff_hevc_put_hevc_qpel_v_3_10_sse;




                }
                if (EXTERNAL_AVX(mm_flags)) {
                }
            }
        }
    }
}
