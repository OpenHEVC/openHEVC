/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */
#if CONFIG_GREEN
#include "hevc.h"
#include "hevc_green.h"
#include "hevcdsp.h"
#include "hevcdsp_green.h"

static luma_config luma_configs[N_LUMA];
static chroma_config chroma_configs[N_CHROMA];

static void store_luma_config(HEVCDSPContext* src, luma_config* dst){
	memcpy(dst->put_hevc_qpel,       src->put_hevc_qpel,        sizeof(src->put_hevc_qpel));
	memcpy(dst->put_hevc_qpel_uni,   src->put_hevc_qpel_uni,    sizeof(src->put_hevc_qpel_uni));
	memcpy(dst->put_hevc_qpel_uni_w, src->put_hevc_qpel_uni_w,  sizeof(src->put_hevc_qpel_uni_w));
	memcpy(dst->put_hevc_qpel_bi,    src->put_hevc_qpel_bi,     sizeof(src->put_hevc_qpel_bi));
	memcpy(dst->put_hevc_qpel_bi_w,  src->put_hevc_qpel_bi_w,   sizeof(src->put_hevc_qpel_bi_w));
}

static void load_luma_config(luma_config* src, HEVCDSPContext* dst){
	memcpy(dst->put_hevc_qpel,       src->put_hevc_qpel,        sizeof(src->put_hevc_qpel));
	memcpy(dst->put_hevc_qpel_uni,   src->put_hevc_qpel_uni,    sizeof(src->put_hevc_qpel_uni));
	memcpy(dst->put_hevc_qpel_uni_w, src->put_hevc_qpel_uni_w,  sizeof(src->put_hevc_qpel_uni_w));
	memcpy(dst->put_hevc_qpel_bi,    src->put_hevc_qpel_bi,     sizeof(src->put_hevc_qpel_bi));
	memcpy(dst->put_hevc_qpel_bi_w,  src->put_hevc_qpel_bi_w,   sizeof(src->put_hevc_qpel_bi_w));
}

static void store_chroma_config(HEVCDSPContext* src, chroma_config* dst){
	memcpy(dst->put_hevc_epel,       src->put_hevc_epel,        sizeof(src->put_hevc_epel));
	memcpy(dst->put_hevc_epel_uni,   src->put_hevc_epel_uni,    sizeof(src->put_hevc_epel_uni));
	memcpy(dst->put_hevc_epel_uni_w, src->put_hevc_epel_uni_w,  sizeof(src->put_hevc_epel_uni_w));
	memcpy(dst->put_hevc_epel_bi,    src->put_hevc_epel_bi,     sizeof(src->put_hevc_epel_bi));
	memcpy(dst->put_hevc_epel_bi_w,  src->put_hevc_epel_bi_w,   sizeof(src->put_hevc_epel_bi_w));
}

static void load_chroma_config(chroma_config* src, HEVCDSPContext* dst){
	memcpy(dst->put_hevc_epel,       src->put_hevc_epel,        sizeof(src->put_hevc_epel));
	memcpy(dst->put_hevc_epel_uni,   src->put_hevc_epel_uni,    sizeof(src->put_hevc_epel_uni));
	memcpy(dst->put_hevc_epel_uni_w, src->put_hevc_epel_uni_w,  sizeof(src->put_hevc_epel_uni_w));
	memcpy(dst->put_hevc_epel_bi,    src->put_hevc_epel_bi,     sizeof(src->put_hevc_epel_bi));
	memcpy(dst->put_hevc_epel_bi_w,  src->put_hevc_epel_bi_w,   sizeof(src->put_hevc_epel_bi_w));
}

#define PEL_FUNC_GREEN(dst1, idx1, idx2, a, depth)                             \
	for(i = 0 ; i < 10 ; i++)                                                  \
		cfg->dst1[i][idx1][idx2] = a ## _ ## depth;

#undef HEVC_DSP_LUMA_GREEN
#define HEVC_DSP_LUMA_GREEN(size, depth)											\
	/* EPEL Funcs */																\
	PEL_FUNC_GREEN(put_hevc_qpel, 0, 0, put_hevc_pel_pixels, depth);				\
	PEL_FUNC_GREEN(put_hevc_qpel, 0, 1, put_hevc_qpel##size##_h, depth);			\
	PEL_FUNC_GREEN(put_hevc_qpel, 1, 0, put_hevc_qpel##size##_v, depth);			\
	PEL_FUNC_GREEN(put_hevc_qpel, 1, 1, put_hevc_qpel##size##_hv, depth);			\
																					\
	/* EPEL Uni Funcs */															\
	PEL_FUNC_GREEN(put_hevc_qpel_uni,   0, 0, put_hevc_pel_uni_pixels, depth);		\
	PEL_FUNC_GREEN(put_hevc_qpel_uni,   0, 1, put_hevc_qpel##size##_uni_h, depth);  \
	PEL_FUNC_GREEN(put_hevc_qpel_uni,   1, 0, put_hevc_qpel##size##_uni_v, depth);  \
	PEL_FUNC_GREEN(put_hevc_qpel_uni,   1, 1, put_hevc_qpel##size##_uni_hv, depth); \
    PEL_FUNC_GREEN(put_hevc_qpel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);    \
    PEL_FUNC_GREEN(put_hevc_qpel_uni_w, 0, 1, put_hevc_qpel##size##_uni_w_h, depth);\
    PEL_FUNC_GREEN(put_hevc_qpel_uni_w, 1, 0, put_hevc_qpel##size##_uni_w_v, depth);\
    PEL_FUNC_GREEN(put_hevc_qpel_uni_w, 1, 1, put_hevc_qpel##size##_uni_w_hv, depth)\
																					\
	/* EPEL Bi Funcs */																\
	PEL_FUNC_GREEN(put_hevc_qpel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);          \
	PEL_FUNC_GREEN(put_hevc_qpel_bi, 0, 1, put_hevc_qpel##size##_bi_h, depth);      \
	PEL_FUNC_GREEN(put_hevc_qpel_bi, 1, 0, put_hevc_qpel##size##_bi_v, depth);      \
	PEL_FUNC_GREEN(put_hevc_qpel_bi, 1, 1, put_hevc_qpel##size##_bi_hv, depth);     \
	PEL_FUNC_GREEN(put_hevc_qpel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);      \
	PEL_FUNC_GREEN(put_hevc_qpel_bi_w, 0, 1, put_hevc_qpel##size##_bi_w_h, depth);  \
	PEL_FUNC_GREEN(put_hevc_qpel_bi_w, 1, 0, put_hevc_qpel##size##_bi_w_v, depth);  \
	PEL_FUNC_GREEN(put_hevc_qpel_bi_w, 1, 1, put_hevc_qpel##size##_bi_w_hv, depth);

static void init_green_filter_luma1(luma_config* cfg){
	int i;
	HEVC_DSP_LUMA_GREEN(1, 8)
}
static void init_green_filter_luma3(luma_config* cfg){
	int i;
	HEVC_DSP_LUMA_GREEN(3, 8)
}
static void init_green_filter_luma5(luma_config* cfg){
	int i;
	HEVC_DSP_LUMA_GREEN(5, 8)
}
static void init_green_filter_luma7(luma_config* cfg){
	int i;
	HEVC_DSP_LUMA_GREEN(7, 8)
}

#undef HEVC_DSP_CHROMA_GREEN
#define HEVC_DSP_CHROMA_GREEN(size, depth)											\
	/* EPEL Funcs */																\
	PEL_FUNC_GREEN(put_hevc_epel, 0, 0, put_hevc_pel_pixels, depth);				\
	PEL_FUNC_GREEN(put_hevc_epel, 0, 1, put_hevc_epel##size##_h, depth);			\
	PEL_FUNC_GREEN(put_hevc_epel, 1, 0, put_hevc_epel##size##_v, depth);			\
	PEL_FUNC_GREEN(put_hevc_epel, 1, 1, put_hevc_epel##size##_hv, depth);			\
																					\
	/* EPEL Uni Funcs */															\
	PEL_FUNC_GREEN(put_hevc_epel_uni,   0, 0, put_hevc_pel_uni_pixels, depth);		\
	PEL_FUNC_GREEN(put_hevc_epel_uni,   0, 1, put_hevc_epel##size##_uni_h, depth);  \
	PEL_FUNC_GREEN(put_hevc_epel_uni,   1, 0, put_hevc_epel##size##_uni_v, depth);  \
	PEL_FUNC_GREEN(put_hevc_epel_uni,   1, 1, put_hevc_epel##size##_uni_hv, depth); \
    PEL_FUNC_GREEN(put_hevc_epel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);    \
    PEL_FUNC_GREEN(put_hevc_epel_uni_w, 0, 1, put_hevc_epel##size##_uni_w_h, depth);\
    PEL_FUNC_GREEN(put_hevc_epel_uni_w, 1, 0, put_hevc_epel##size##_uni_w_v, depth);\
    PEL_FUNC_GREEN(put_hevc_epel_uni_w, 1, 1, put_hevc_epel##size##_uni_w_hv, depth)\
																					\
	/* EPEL Bi Funcs */																\
	PEL_FUNC_GREEN(put_hevc_epel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);          \
	PEL_FUNC_GREEN(put_hevc_epel_bi, 0, 1, put_hevc_epel##size##_bi_h, depth);      \
	PEL_FUNC_GREEN(put_hevc_epel_bi, 1, 0, put_hevc_epel##size##_bi_v, depth);      \
	PEL_FUNC_GREEN(put_hevc_epel_bi, 1, 1, put_hevc_epel##size##_bi_hv, depth);     \
	PEL_FUNC_GREEN(put_hevc_epel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);      \
	PEL_FUNC_GREEN(put_hevc_epel_bi_w, 0, 1, put_hevc_epel##size##_bi_w_h, depth);  \
	PEL_FUNC_GREEN(put_hevc_epel_bi_w, 1, 0, put_hevc_epel##size##_bi_w_v, depth);  \
	PEL_FUNC_GREEN(put_hevc_epel_bi_w, 1, 1, put_hevc_epel##size##_bi_w_hv, depth);

static void init_green_filter_chroma1(chroma_config* cfg){
	int i;
	HEVC_DSP_CHROMA_GREEN(1, 8)
}
static void init_green_filter_chroma2(chroma_config* cfg){
	int i;
	HEVC_DSP_CHROMA_GREEN(2, 8)
}
static void init_green_filter_chroma3(chroma_config* cfg){
	int i;
	HEVC_DSP_CHROMA_GREEN(3, 8)
}

void green_dsp_init(HEVCDSPContext *hevcdsp)
{
    hevcdsp->green_cur_luma = LUMA_LEG;
    hevcdsp->green_cur_chroma = CHROMA_LEG;
    hevcdsp->green_on = 0;

    static const char* green_coeffs_text[3] = {
		"OLD",
		"ASYMMETRIC",
		"SYMMETRIC"
    };

    /* Store Legacy Config */
    store_luma_config(hevcdsp, &luma_configs[LUMA_LEG]);
    store_chroma_config(hevcdsp, &chroma_configs[CHROMA_LEG]);

    /* Init Green Config */
    init_green_filter_luma1(&luma_configs[LUMA1]);
    init_green_filter_luma3(&luma_configs[LUMA3]);
    init_green_filter_luma5(&luma_configs[LUMA5]);
    init_green_filter_luma7(&luma_configs[LUMA7]);

    init_green_filter_chroma1(&chroma_configs[CHROMA1]);
    init_green_filter_chroma2(&chroma_configs[CHROMA2]);
    init_green_filter_chroma3(&chroma_configs[CHROMA3]);

    printf("Current Coefs config: %s\n", green_coeffs_text[GREEN_FILTER_TYPE]);
    printf("Legacy config %d:%d\n", LUMA_TAPS, CHROMA_TAPS);
}

void green_update_filter_luma(HEVCDSPContext *c, int type){
	load_luma_config(&luma_configs[type], c);
}

void green_update_filter_chroma(HEVCDSPContext *c, int type){
	load_chroma_config(&chroma_configs[type], c);
}

#endif
