/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */

#include "hevc.h"
#include "hevcdsp.h"
#include "hevcdsp_eco.h"

void eco_dsp_init(HEVCDSPContext *hevcdsp)
{
    hevcdsp->eco_cur_luma=7;
    hevcdsp->eco_cur_chroma=4;
    hevcdsp->eco_on = 0;
}

#if !HAVE_NEON
/** ECO filters reload functions */
void eco_reload_filter_luma1(HEVCDSPContext *hevcdsp, int bit_depth)
{
#undef PEL_FUNC
#define PEL_FUNC(dst1, idx1, idx2, a, depth)                                   \
    for(i = 0 ; i < 10 ; i++)                                                  \
{                                                                              \
    hevcdsp->dst1[i][idx1][idx2] = a ## _ ## depth;                            \
}

#undef QPEL_FUNCS
#define QPEL_FUNCS(depth)                                                         \
    PEL_FUNC(put_hevc_qpel, 0, 0, put_hevc_pel_pixels, depth);                    \
    PEL_FUNC(put_hevc_qpel, 0, 1, put_hevc_qpel1_h, depth);                        \
    PEL_FUNC(put_hevc_qpel, 1, 0, put_hevc_qpel1_v, depth);                        \
    PEL_FUNC(put_hevc_qpel, 1, 1, put_hevc_qpel1_hv, depth)

#undef QPEL_UNI_FUNCS
#define QPEL_UNI_FUNCS(depth)                                                     \
    PEL_FUNC(put_hevc_qpel_uni, 0, 0, put_hevc_pel_uni_pixels, depth);            \
    PEL_FUNC(put_hevc_qpel_uni, 0, 1, put_hevc_qpel1_uni_h, depth);                \
    PEL_FUNC(put_hevc_qpel_uni, 1, 0, put_hevc_qpel1_uni_v, depth);                \
    PEL_FUNC(put_hevc_qpel_uni, 1, 1, put_hevc_qpel1_uni_hv, depth);               \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);        \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 1, put_hevc_qpel1_uni_w_h, depth);            \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 0, put_hevc_qpel1_uni_w_v, depth);            \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 1, put_hevc_qpel1_uni_w_hv, depth)

#undef QPEL_BI_FUNCS
#define QPEL_BI_FUNCS(depth)                                                      \
    PEL_FUNC(put_hevc_qpel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);              \
    PEL_FUNC(put_hevc_qpel_bi, 0, 1, put_hevc_qpel1_bi_h, depth);                  \
    PEL_FUNC(put_hevc_qpel_bi, 1, 0, put_hevc_qpel1_bi_v, depth);                  \
    PEL_FUNC(put_hevc_qpel_bi, 1, 1, put_hevc_qpel1_bi_hv, depth);                 \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);          \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 1, put_hevc_qpel1_bi_w_h, depth);              \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 0, put_hevc_qpel1_bi_w_v, depth);              \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 1, put_hevc_qpel1_bi_w_hv, depth)

#undef HEVC_DSP
#define HEVC_DSP(depth)                                                            \
    QPEL_FUNCS(depth);                                                             \
    QPEL_UNI_FUNCS(depth);                                                         \
    QPEL_BI_FUNCS(depth)

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
    default:
        HEVC_DSP(8);
        break;
    }

    hevcdsp->eco_cur_luma=1;
}

void eco_reload_filter_luma3(HEVCDSPContext *hevcdsp, int bit_depth)
{
#undef PEL_FUNC
#define PEL_FUNC(dst1, idx1, idx2, a, depth)                                   \
    for(i = 0 ; i < 10 ; i++)                                                  \
{                                                                              \
    hevcdsp->dst1[i][idx1][idx2] = a ## _ ## depth;                            \
}

#undef QPEL_FUNCS
#define QPEL_FUNCS(depth)                                                         \
    PEL_FUNC(put_hevc_qpel, 0, 0, put_hevc_pel_pixels, depth);                    \
    PEL_FUNC(put_hevc_qpel, 0, 1, put_hevc_qpel3_h, depth);                        \
    PEL_FUNC(put_hevc_qpel, 1, 0, put_hevc_qpel3_v, depth);                        \
    PEL_FUNC(put_hevc_qpel, 1, 1, put_hevc_qpel3_hv, depth)

#undef QPEL_UNI_FUNCS
#define QPEL_UNI_FUNCS(depth)                                                     \
    PEL_FUNC(put_hevc_qpel_uni, 0, 0, put_hevc_pel_uni_pixels, depth);            \
    PEL_FUNC(put_hevc_qpel_uni, 0, 1, put_hevc_qpel3_uni_h, depth);                \
    PEL_FUNC(put_hevc_qpel_uni, 1, 0, put_hevc_qpel3_uni_v, depth);                \
    PEL_FUNC(put_hevc_qpel_uni, 1, 1, put_hevc_qpel3_uni_hv, depth);               \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);        \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 1, put_hevc_qpel3_uni_w_h, depth);            \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 0, put_hevc_qpel3_uni_w_v, depth);            \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 1, put_hevc_qpel3_uni_w_hv, depth)

#undef QPEL_BI_FUNCS
#define QPEL_BI_FUNCS(depth)                                                      \
    PEL_FUNC(put_hevc_qpel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);              \
    PEL_FUNC(put_hevc_qpel_bi, 0, 1, put_hevc_qpel3_bi_h, depth);                  \
    PEL_FUNC(put_hevc_qpel_bi, 1, 0, put_hevc_qpel3_bi_v, depth);                  \
    PEL_FUNC(put_hevc_qpel_bi, 1, 1, put_hevc_qpel3_bi_hv, depth);                 \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);          \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 1, put_hevc_qpel3_bi_w_h, depth);              \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 0, put_hevc_qpel3_bi_w_v, depth);              \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 1, put_hevc_qpel3_bi_w_hv, depth)

#undef HEVC_DSP
#define HEVC_DSP(depth)                                                            \
    QPEL_FUNCS(depth);                                                             \
    QPEL_UNI_FUNCS(depth);                                                         \
    QPEL_BI_FUNCS(depth)

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
    default:
        HEVC_DSP(8);
        break;
    }
    hevcdsp->eco_cur_luma=3;
}

void eco_reload_filter_luma7(HEVCDSPContext *hevcdsp, int bit_depth)
{
#undef PEL_FUNC
#define PEL_FUNC(dst1, idx1, idx2, a, depth)                                   \
    for(i = 0 ; i < 10 ; i++)                                                  \
{                                                                              \
    hevcdsp->dst1[i][idx1][idx2] = a ## _ ## depth;                            \
}

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

#undef HEVC_DSP
#define HEVC_DSP(depth)                                                            \
    QPEL_FUNCS(depth);                                                             \
    QPEL_UNI_FUNCS(depth);                                                         \
    QPEL_BI_FUNCS(depth)

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
    default:
        HEVC_DSP(8);
        break;
    }
    hevcdsp->eco_cur_luma=7;
}

void eco_reload_filter_chroma1(HEVCDSPContext *hevcdsp, int bit_depth)
{
#undef PEL_FUNC
#define PEL_FUNC(dst1, idx1, idx2, a, depth)                                   \
    for(i = 0 ; i < 10 ; i++)                                                  \
{                                                                              \
    hevcdsp->dst1[i][idx1][idx2] = a ## _ ## depth;                            \
}

#undef EPEL_FUNCS
#define EPEL_FUNCS(depth)                                                       \
    PEL_FUNC(put_hevc_epel, 0, 0, put_hevc_pel_pixels, depth);                  \
    PEL_FUNC(put_hevc_epel, 0, 1, put_hevc_pel_pixels, depth);                      \
    PEL_FUNC(put_hevc_epel, 1, 0, put_hevc_pel_pixels, depth);                      \
    PEL_FUNC(put_hevc_epel, 1, 1, put_hevc_pel_pixels, depth)


#undef HEVC_DSP
#define HEVC_DSP(depth)                                                            \
    EPEL_FUNCS(depth);                                                             \

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
    default:
        HEVC_DSP(8);
        break;
    }
    hevcdsp->eco_cur_chroma=1;
}


void eco_reload_filter_chroma2(HEVCDSPContext *hevcdsp, int bit_depth)
{
#undef PEL_FUNC
#define PEL_FUNC(dst1, idx1, idx2, a, depth)                                   \
    for(i = 0 ; i < 10 ; i++)                                                  \
{                                                                              \
    hevcdsp->dst1[i][idx1][idx2] = a ## _ ## depth;                            \
}

#undef EPEL_FUNCS
#define EPEL_FUNCS(depth)                                                       \
	    PEL_FUNC(put_hevc_epel, 0, 0, put_hevc_pel_pixels, depth);                  \
	    PEL_FUNC(put_hevc_epel, 0, 1, put_hevc_epel2_h, depth);                      \
	    PEL_FUNC(put_hevc_epel, 1, 0, put_hevc_epel2_v, depth);                      \
	    PEL_FUNC(put_hevc_epel, 1, 1, put_hevc_epel2_hv, depth)


#undef HEVC_DSP
#define HEVC_DSP(depth)                                                            \
    EPEL_FUNCS(depth);                                                             \

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
    default:
        HEVC_DSP(8);
        break;
    }
    hevcdsp->eco_cur_chroma=2;
}


void eco_reload_filter_chroma4(HEVCDSPContext *hevcdsp, int bit_depth)
{
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

#undef HEVC_DSP
#define HEVC_DSP(depth)                                                            \
    EPEL_FUNCS(depth);                                                             \

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
    default:
        HEVC_DSP(8);
        break;
    }
    hevcdsp->eco_cur_chroma=4;
}
#endif
