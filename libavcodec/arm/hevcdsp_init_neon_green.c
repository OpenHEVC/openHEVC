/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */

#include "../hevc_green.h"

static void (*ff_hevc_put_qpel_pixels_neon_green_8_fcts[])
		(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
         int height, intptr_t mx, intptr_t my, int width) = {
                 0,
        		 ff_hevc_put_pixels_w4_neon_8,
        		 0,
        		 ff_hevc_put_pixels_w8_neon_8,
        		 ff_hevc_put_pixels_w12_neon_8,
        		 ff_hevc_put_pixels_w16_neon_8,
        		 ff_hevc_put_pixels_w24_neon_8,
        		 ff_hevc_put_pixels_w32_neon_8,
        		 ff_hevc_put_pixels_w48_neon_8,
        		 ff_hevc_put_pixels_w64_neon_8,
};

static void (*ff_hevc_put_epel_pixels_neon_green_8_fcts[])
		(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
         int height, intptr_t mx, intptr_t my, int width) = {
         ff_hevc_put_pixels_w2_neon_8,
		 ff_hevc_put_pixels_w4_neon_8,
		 ff_hevc_put_pixels_w6_neon_8,
		 ff_hevc_put_pixels_w8_neon_8,
		 ff_hevc_put_pixels_w12_neon_8,
		 ff_hevc_put_pixels_w16_neon_8,
		 ff_hevc_put_pixels_w24_neon_8,
		 ff_hevc_put_pixels_w32_neon_8,
		 0,
		 0
};

static void (*ff_hevc_put_epel_uni_pixels_neon_green_8_fcts[])
		(uint8_t *dst, ptrdiff_t dststride,
		 uint8_t *_src, ptrdiff_t _srcstride,
         int height, intptr_t mx, intptr_t my, int width) = {
        		 ff_hevc_put_epel_uni_pixels_w2_neon_8,
				 ff_hevc_put_epel_uni_pixels_w4_neon_8,
				 ff_hevc_put_epel_uni_pixels_w6_neon_8,
				 ff_hevc_put_epel_uni_pixels_w8_neon_8,
				 ff_hevc_put_epel_uni_pixels_w12_neon_8,
				 ff_hevc_put_epel_uni_pixels_w16_neon_8,
				 ff_hevc_put_epel_uni_pixels_w24_neon_8,
				 ff_hevc_put_epel_uni_pixels_w32_neon_8,
		 0,
		 0
};

static void (*ff_hevc_put_epel_bi_pixels_neon_green_8_fcts[])
    	    (  	uint8_t *dst, ptrdiff_t dststride,
    	    	uint8_t *_src, ptrdiff_t _srcstride,
				int16_t *src2,
    			int height, intptr_t mx, intptr_t my, int width) = {
        		 ff_hevc_put_epel_bi_pixels_w2_neon_8,
				 ff_hevc_put_epel_bi_pixels_w4_neon_8,
				 ff_hevc_put_epel_bi_pixels_w6_neon_8,
				 ff_hevc_put_epel_bi_pixels_w8_neon_8,
				 ff_hevc_put_epel_bi_pixels_w12_neon_8,
				 ff_hevc_put_epel_bi_pixels_w16_neon_8,
				 ff_hevc_put_epel_bi_pixels_w24_neon_8,
				 ff_hevc_put_epel_bi_pixels_w32_neon_8,
		 0,
		 0
};

#define QPEL_FUNC(name) \
    void name(	int16_t *dst, 						\
				uint8_t *src, ptrdiff_t srcstride, 	\
                int height, int width)

#define QPEL_FUNC_UW(name) \
    void name(	uint8_t *dst, ptrdiff_t dststride, 		\
				uint8_t *_src, ptrdiff_t _srcstride, 	\
                int width, int height, 					\
				int16_t* src2);

#define EPEL_FUNC(name) 											\
    void name(	int16_t *dst, 										\
				uint8_t *src, ptrdiff_t srcstride,					\
				int height, intptr_t mx, intptr_t my, int width);

/* Eco3 Filters Morgan */
QPEL_FUNC(ff_hevc_put_qpel3_v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h1v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h1v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h1v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h2v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h2v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h2v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h3v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h3v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel3_h3v3_neon_8);

QPEL_FUNC_UW(ff_hevc_put_qpel_uw_pixels_green1_neon_8);

QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h1v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h1v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h1v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h2v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h2v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h2v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h3v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h3v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_h3v3_neon_8);

/* Eco5 Filters Morgan */
QPEL_FUNC(ff_hevc_put_qpel5_v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h1v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h1v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h1v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h2v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h2v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h2v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h3v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h3v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel5_h3v3_neon_8);

QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h1v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h1v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h1v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h2v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h2v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h2v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h3v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h3v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_h3v3_neon_8);

EPEL_FUNC(ff_hevc_put_epel2_h_neon_8)
EPEL_FUNC(ff_hevc_put_epel2_v_neon_8)
EPEL_FUNC(ff_hevc_put_epel2_hv_neon_8)

EPEL_FUNC(ff_hevc_put_epel3_h_neon_8)
EPEL_FUNC(ff_hevc_put_epel3_v_neon_8)
EPEL_FUNC(ff_hevc_put_epel3_hv_neon_8)

#define EPEL_FUNC_UNI(name) \
    void name(  uint8_t *dst, ptrdiff_t dststride,					\
				uint8_t *_src, ptrdiff_t _srcstride,				\
				int height, intptr_t mx, intptr_t my, int width)

EPEL_FUNC_UNI(ff_hevc_epel2_uni_h_neon_8);
EPEL_FUNC_UNI(ff_hevc_epel2_uni_v_neon_8);
EPEL_FUNC_UNI(ff_hevc_epel2_uni_hv_neon_8);

EPEL_FUNC_UNI(ff_hevc_epel3_uni_h_neon_8);
EPEL_FUNC_UNI(ff_hevc_epel3_uni_v_neon_8);
EPEL_FUNC_UNI(ff_hevc_epel3_uni_hv_neon_8);

#undef EPEL_FUNC_UNI

#define EPEL_FUNC_BI(name) \
    void name(  uint8_t *dst, ptrdiff_t dststride,					\
				uint8_t *_src, ptrdiff_t _srcstride,				\
				int16_t *src2, 										\
				int height, intptr_t mx, intptr_t my, int width)

EPEL_FUNC_BI(ff_hevc_epel2_bi_h_neon_8);
EPEL_FUNC_BI(ff_hevc_epel2_bi_v_neon_8);
EPEL_FUNC_BI(ff_hevc_epel2_bi_hv_neon_8);

EPEL_FUNC_BI(ff_hevc_epel3_bi_h_neon_8);
EPEL_FUNC_BI(ff_hevc_epel3_bi_v_neon_8);
EPEL_FUNC_BI(ff_hevc_epel3_bi_hv_neon_8);

#undef EPEL_FUNC_BI

#undef QPEL_FUNC_UW
#undef QPEL_FUNC
#undef EPEL_FUNC

const uint8_t ff_hevc_green_pel_weight[65] = { [2] = 0, [4] = 1, [6] = 2, [8] = 3, [12] = 4, [16] = 5, [24] = 6, [32] = 7, [48] = 8, [64] = 9 };

static void (*ff_hevc_put_uw_pixels_neon_green_8_fcts[])
		(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
         int height, intptr_t mx, intptr_t my, int width) = {
    0,
	ff_hevc_put_qpel_uw_pixels_w4_neon_8,
	0,
	ff_hevc_put_qpel_uw_pixels_w8_neon_8,
	ff_hevc_put_qpel_uw_pixels_w12_neon_8,
	ff_hevc_put_qpel_uw_pixels_w16_neon_8,
	ff_hevc_put_qpel_uw_pixels_w24_neon_8,
	ff_hevc_put_qpel_uw_pixels_w32_neon_8,
	ff_hevc_put_qpel_uw_pixels_w48_neon_8,
	ff_hevc_put_qpel_uw_pixels_w64_neon_8,
};

static void ff_hevc_put_qpel1_neon_wrapper1(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {
    int idx = ff_hevc_green_pel_weight[width];
	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;

	ff_hevc_put_qpel_pixels_neon_green_8_fcts[idx](dst, src + offset, srcstride, height, mx, my, width);
}

static void ff_hevc_put_qpel1_uni_neon_wrapper1(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {
    int idx = ff_hevc_green_pel_weight[width];
	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;

    ff_hevc_put_uw_pixels_neon_green_8_fcts[idx](dst, dststride, src + offset, srcstride, height, mx, my, width);
}

static void ff_hevc_put_qpel1_bi_neon_wrapper1(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
										int16_t *src2,
										int height, intptr_t mx, intptr_t my, int width) {
    int idx = ff_hevc_green_pel_weight[width];
	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;

    /* No NEON fct are defined here :( */
//	put_hevc_pel_bi_pixels_green_8(dst, dststride, src+offset, srcstride, src2, src2stride, height, mx, my, width);
	ff_hevc_put_qpel_uw_pixels_green1_neon_8(dst, dststride, src + offset, srcstride, width, height, src2);
}

av_cold void green_reload_filter_luma1_neon(luma_config *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_qpel_pixels_neon_green_8_fcts[x];
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel1_neon_wrapper1;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel1_neon_wrapper1;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel1_neon_wrapper1;
            c->put_hevc_qpel_uni[x][0][0]     = ff_hevc_put_qpel1_uni_neon_wrapper1;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel1_uni_neon_wrapper1;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel1_uni_neon_wrapper1;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel1_uni_neon_wrapper1;
            c->put_hevc_qpel_bi[x][0][0]     = ff_hevc_put_qpel1_bi_neon_wrapper1;
            c->put_hevc_qpel_bi[x][1][0]     = ff_hevc_put_qpel1_bi_neon_wrapper1;
            c->put_hevc_qpel_bi[x][0][1]     = ff_hevc_put_qpel1_bi_neon_wrapper1;
            c->put_hevc_qpel_bi[x][1][1]     = ff_hevc_put_qpel1_bi_neon_wrapper1;
        }

        c->put_hevc_qpel_uni[1][0][0]  = ff_hevc_put_qpel_uw_pixels_w4_neon_8;
        c->put_hevc_qpel_uni[3][0][0]  = ff_hevc_put_qpel_uw_pixels_w8_neon_8;
        c->put_hevc_qpel_uni[4][0][0]  = ff_hevc_put_qpel_uw_pixels_w12_neon_8;
        c->put_hevc_qpel_uni[5][0][0]  = ff_hevc_put_qpel_uw_pixels_w16_neon_8;
        c->put_hevc_qpel_uni[6][0][0]  = ff_hevc_put_qpel_uw_pixels_w24_neon_8;
        c->put_hevc_qpel_uni[7][0][0]  = ff_hevc_put_qpel_uw_pixels_w32_neon_8;
        c->put_hevc_qpel_uni[8][0][0]  = ff_hevc_put_qpel_uw_pixels_w48_neon_8;
        c->put_hevc_qpel_uni[9][0][0]  = ff_hevc_put_qpel_uw_pixels_w64_neon_8;

        c->put_hevc_qpel_bi[1][0][0]  = ff_hevc_put_qpel_bi_pixels_w4_neon_8;
        c->put_hevc_qpel_bi[3][0][0]  = ff_hevc_put_qpel_bi_pixels_w8_neon_8;
        c->put_hevc_qpel_bi[4][0][0]  = ff_hevc_put_qpel_bi_pixels_w12_neon_8;
        c->put_hevc_qpel_bi[5][0][0]  = ff_hevc_put_qpel_bi_pixels_w16_neon_8;
        c->put_hevc_qpel_bi[6][0][0]  = ff_hevc_put_qpel_bi_pixels_w24_neon_8;
        c->put_hevc_qpel_bi[7][0][0]  = ff_hevc_put_qpel_bi_pixels_w32_neon_8;
        c->put_hevc_qpel_bi[8][0][0]  = ff_hevc_put_qpel_bi_pixels_w48_neon_8;
        c->put_hevc_qpel_bi[9][0][0]  = ff_hevc_put_qpel_bi_pixels_w64_neon_8;
    }
}

static void (*put_hevc_qpel_green3_neon[4][4])(
	int16_t *dst,
	uint8_t *src, ptrdiff_t srcstride,
	int height, int width);

static void (*put_hevc_qpel_green3_uw_neon[4][4])(
	uint8_t *dst, ptrdiff_t dststride,
	uint8_t *_src, ptrdiff_t _srcstride,
	int width, int height,
	int16_t* src2);

static void ff_hevc_put_qpel_green3_neon_wrapper(
		int16_t *dst,
		uint8_t *src, ptrdiff_t srcstride,
		int height, intptr_t mx, intptr_t my, int width) {

	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;
	put_hevc_qpel_green3_neon[my][mx](
		dst, src + offset, srcstride,
		height, width);
}

static void ff_hevc_put_qpel_green3_uni_neon_wrapper(
		uint8_t *dst, ptrdiff_t dststride,
		uint8_t *src, ptrdiff_t srcstride,
		int height, intptr_t mx, intptr_t my, int width) {

	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;
	put_hevc_qpel_green3_uw_neon[my][mx](
		dst, dststride,
		src + offset, srcstride,
		width, height,
		((void *)0));
}

static void ff_hevc_put_qpel_green3_bi_neon_wrapper(
		uint8_t *dst, ptrdiff_t dststride,
		uint8_t *src, ptrdiff_t srcstride,
		int16_t *src2,
		int height, intptr_t mx, intptr_t my, int width){

	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;
	put_hevc_qpel_green3_uw_neon[my][mx](
		dst, dststride,
		src + offset, srcstride,
		width, height,
		src2);
}


av_cold void green_reload_filter_luma3_neon(luma_config *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        put_hevc_qpel_green3_neon[1][0]         = ff_hevc_put_qpel3_v1_neon_8;
        put_hevc_qpel_green3_neon[2][0]         = ff_hevc_put_qpel3_v2_neon_8;
        put_hevc_qpel_green3_neon[3][0]         = ff_hevc_put_qpel3_v3_neon_8;
        put_hevc_qpel_green3_neon[0][1]         = ff_hevc_put_qpel3_h1_neon_8;
        put_hevc_qpel_green3_neon[0][2]         = ff_hevc_put_qpel3_h2_neon_8;
        put_hevc_qpel_green3_neon[0][3]         = ff_hevc_put_qpel3_h3_neon_8;
        put_hevc_qpel_green3_neon[1][1]         = ff_hevc_put_qpel3_h1v1_neon_8;
        put_hevc_qpel_green3_neon[1][2]         = ff_hevc_put_qpel3_h2v1_neon_8;
        put_hevc_qpel_green3_neon[1][3]         = ff_hevc_put_qpel3_h3v1_neon_8;
        put_hevc_qpel_green3_neon[2][1]         = ff_hevc_put_qpel3_h1v2_neon_8;
        put_hevc_qpel_green3_neon[2][2]         = ff_hevc_put_qpel3_h2v2_neon_8;
        put_hevc_qpel_green3_neon[2][3]         = ff_hevc_put_qpel3_h3v2_neon_8;
        put_hevc_qpel_green3_neon[3][1]         = ff_hevc_put_qpel3_h1v3_neon_8;
        put_hevc_qpel_green3_neon[3][2]         = ff_hevc_put_qpel3_h2v3_neon_8;
        put_hevc_qpel_green3_neon[3][3]         = ff_hevc_put_qpel3_h3v3_neon_8;
        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_green3_uw_neon[1][0]      = ff_hevc_put_qpel3_uw_v1_neon_8;
        put_hevc_qpel_green3_uw_neon[2][0]      = ff_hevc_put_qpel3_uw_v2_neon_8;
        put_hevc_qpel_green3_uw_neon[3][0]      = ff_hevc_put_qpel3_uw_v3_neon_8;
        put_hevc_qpel_green3_uw_neon[0][1]      = ff_hevc_put_qpel3_uw_h1_neon_8;
        put_hevc_qpel_green3_uw_neon[0][2]      = ff_hevc_put_qpel3_uw_h2_neon_8;
        put_hevc_qpel_green3_uw_neon[0][3]      = ff_hevc_put_qpel3_uw_h3_neon_8;
        put_hevc_qpel_green3_uw_neon[1][1]      = ff_hevc_put_qpel3_uw_h1v1_neon_8;
        put_hevc_qpel_green3_uw_neon[1][2]      = ff_hevc_put_qpel3_uw_h2v1_neon_8;
        put_hevc_qpel_green3_uw_neon[1][3]      = ff_hevc_put_qpel3_uw_h3v1_neon_8;
        put_hevc_qpel_green3_uw_neon[2][1]      = ff_hevc_put_qpel3_uw_h1v2_neon_8;
        put_hevc_qpel_green3_uw_neon[2][2]      = ff_hevc_put_qpel3_uw_h2v2_neon_8;
        put_hevc_qpel_green3_uw_neon[2][3]      = ff_hevc_put_qpel3_uw_h3v2_neon_8;
        put_hevc_qpel_green3_uw_neon[3][1]      = ff_hevc_put_qpel3_uw_h1v3_neon_8;
        put_hevc_qpel_green3_uw_neon[3][2]      = ff_hevc_put_qpel3_uw_h2v3_neon_8;
        put_hevc_qpel_green3_uw_neon[3][3]      = ff_hevc_put_qpel3_uw_h3v3_neon_8;

        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_qpel_pixels_neon_green_8_fcts[x];
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_green3_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_green3_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_green3_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_green3_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_green3_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_green3_uni_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_green3_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_green3_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_green3_bi_neon_wrapper;
        }

        c->put_hevc_qpel_uni[1][0][0]  = ff_hevc_put_qpel_uw_pixels_w4_neon_8;
        c->put_hevc_qpel_uni[3][0][0]  = ff_hevc_put_qpel_uw_pixels_w8_neon_8;
        c->put_hevc_qpel_uni[4][0][0]  = ff_hevc_put_qpel_uw_pixels_w12_neon_8;
        c->put_hevc_qpel_uni[5][0][0]  = ff_hevc_put_qpel_uw_pixels_w16_neon_8;
        c->put_hevc_qpel_uni[6][0][0]  = ff_hevc_put_qpel_uw_pixels_w24_neon_8;
        c->put_hevc_qpel_uni[7][0][0]  = ff_hevc_put_qpel_uw_pixels_w32_neon_8;
        c->put_hevc_qpel_uni[8][0][0]  = ff_hevc_put_qpel_uw_pixels_w48_neon_8;
        c->put_hevc_qpel_uni[9][0][0]  = ff_hevc_put_qpel_uw_pixels_w64_neon_8;

        c->put_hevc_qpel_bi[1][0][0]  = ff_hevc_put_qpel_bi_pixels_w4_neon_8;
        c->put_hevc_qpel_bi[3][0][0]  = ff_hevc_put_qpel_bi_pixels_w8_neon_8;
        c->put_hevc_qpel_bi[4][0][0]  = ff_hevc_put_qpel_bi_pixels_w12_neon_8;
        c->put_hevc_qpel_bi[5][0][0]  = ff_hevc_put_qpel_bi_pixels_w16_neon_8;
        c->put_hevc_qpel_bi[6][0][0]  = ff_hevc_put_qpel_bi_pixels_w24_neon_8;
        c->put_hevc_qpel_bi[7][0][0]  = ff_hevc_put_qpel_bi_pixels_w32_neon_8;
        c->put_hevc_qpel_bi[8][0][0]  = ff_hevc_put_qpel_bi_pixels_w48_neon_8;
        c->put_hevc_qpel_bi[9][0][0]  = ff_hevc_put_qpel_bi_pixels_w64_neon_8;
    }
}

static void (*put_hevc_qpel_green5_neon[4][4])(
	int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
	int height, int width);

static void (*put_hevc_qpel_green5_uw_neon[4][4])(
	uint8_t *dst, ptrdiff_t dststride,
	uint8_t *_src, ptrdiff_t _srcstride,
	int width, int height,
	int16_t* src2);

static void ff_hevc_put_qpel_green5_neon_wrapper(
		int16_t *dst,
		uint8_t *src, ptrdiff_t srcstride,
		int height, intptr_t mx, intptr_t my, int width) {
	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;
	put_hevc_qpel_green5_neon[my][mx](
		dst,
		src + offset, srcstride,
		height, width);
}

static void ff_hevc_put_qpel_green5_uni_neon_wrapper(
		uint8_t *dst, ptrdiff_t dststride,
		uint8_t *src, ptrdiff_t srcstride,
		int height, intptr_t mx, intptr_t my, int width) {

	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;
	put_hevc_qpel_green5_uw_neon[my][mx](
		dst, dststride,
		src + offset, srcstride,
		width, height,
		(void *)0);
}

static void ff_hevc_put_qpel_green5_bi_neon_wrapper(
		uint8_t *dst, ptrdiff_t dststride,
		uint8_t *src, ptrdiff_t srcstride,
		int16_t *src2,
		int height, intptr_t mx, intptr_t my, int width){

	int offset = ((mx & (mx-1)) >> 1) + ((my & (my-1)) >> 1)*srcstride;
	put_hevc_qpel_green5_uw_neon[my][mx](
		dst, dststride,
		src + offset, srcstride,
		width, height,
		src2);
}

av_cold void green_reload_filter_luma5_neon(luma_config *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        put_hevc_qpel_green5_neon[1][0]         = ff_hevc_put_qpel5_v1_neon_8;
        put_hevc_qpel_green5_neon[2][0]         = ff_hevc_put_qpel5_v2_neon_8;
        put_hevc_qpel_green5_neon[3][0]         = ff_hevc_put_qpel5_v3_neon_8;
        put_hevc_qpel_green5_neon[0][1]         = ff_hevc_put_qpel5_h1_neon_8;
        put_hevc_qpel_green5_neon[0][2]         = ff_hevc_put_qpel5_h2_neon_8;
        put_hevc_qpel_green5_neon[0][3]         = ff_hevc_put_qpel5_h3_neon_8;
        put_hevc_qpel_green5_neon[1][1]         = ff_hevc_put_qpel5_h1v1_neon_8;
        put_hevc_qpel_green5_neon[1][2]         = ff_hevc_put_qpel5_h2v1_neon_8;
        put_hevc_qpel_green5_neon[1][3]         = ff_hevc_put_qpel5_h3v1_neon_8;
        put_hevc_qpel_green5_neon[2][1]         = ff_hevc_put_qpel5_h1v2_neon_8;
        put_hevc_qpel_green5_neon[2][2]         = ff_hevc_put_qpel5_h2v2_neon_8;
        put_hevc_qpel_green5_neon[2][3]         = ff_hevc_put_qpel5_h3v2_neon_8;
        put_hevc_qpel_green5_neon[3][1]         = ff_hevc_put_qpel5_h1v3_neon_8;
        put_hevc_qpel_green5_neon[3][2]         = ff_hevc_put_qpel5_h2v3_neon_8;
        put_hevc_qpel_green5_neon[3][3]         = ff_hevc_put_qpel5_h3v3_neon_8;
        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_green5_uw_neon[1][0]      = ff_hevc_put_qpel5_uw_v1_neon_8;
        put_hevc_qpel_green5_uw_neon[2][0]      = ff_hevc_put_qpel5_uw_v2_neon_8;
        put_hevc_qpel_green5_uw_neon[3][0]      = ff_hevc_put_qpel5_uw_v3_neon_8;
        put_hevc_qpel_green5_uw_neon[0][1]      = ff_hevc_put_qpel5_uw_h1_neon_8;
        put_hevc_qpel_green5_uw_neon[0][2]      = ff_hevc_put_qpel5_uw_h2_neon_8;
        put_hevc_qpel_green5_uw_neon[0][3]      = ff_hevc_put_qpel5_uw_h3_neon_8;
        put_hevc_qpel_green5_uw_neon[1][1]      = ff_hevc_put_qpel5_uw_h1v1_neon_8;
        put_hevc_qpel_green5_uw_neon[1][2]      = ff_hevc_put_qpel5_uw_h2v1_neon_8;
        put_hevc_qpel_green5_uw_neon[1][3]      = ff_hevc_put_qpel5_uw_h3v1_neon_8;
        put_hevc_qpel_green5_uw_neon[2][1]      = ff_hevc_put_qpel5_uw_h1v2_neon_8;
        put_hevc_qpel_green5_uw_neon[2][2]      = ff_hevc_put_qpel5_uw_h2v2_neon_8;
        put_hevc_qpel_green5_uw_neon[2][3]      = ff_hevc_put_qpel5_uw_h3v2_neon_8;
        put_hevc_qpel_green5_uw_neon[3][1]      = ff_hevc_put_qpel5_uw_h1v3_neon_8;
        put_hevc_qpel_green5_uw_neon[3][2]      = ff_hevc_put_qpel5_uw_h2v3_neon_8;
        put_hevc_qpel_green5_uw_neon[3][3]      = ff_hevc_put_qpel5_uw_h3v3_neon_8;

        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_qpel_pixels_neon_green_8_fcts[x];
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_green5_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_green5_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_green5_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_green5_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_green5_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_green5_uni_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_green5_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_green5_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_green5_bi_neon_wrapper;
        }

        c->put_hevc_qpel_uni[1][0][0]  = ff_hevc_put_qpel_uw_pixels_w4_neon_8;
        c->put_hevc_qpel_uni[3][0][0]  = ff_hevc_put_qpel_uw_pixels_w8_neon_8;
        c->put_hevc_qpel_uni[4][0][0]  = ff_hevc_put_qpel_uw_pixels_w12_neon_8;
        c->put_hevc_qpel_uni[5][0][0]  = ff_hevc_put_qpel_uw_pixels_w16_neon_8;
        c->put_hevc_qpel_uni[6][0][0]  = ff_hevc_put_qpel_uw_pixels_w24_neon_8;
        c->put_hevc_qpel_uni[7][0][0]  = ff_hevc_put_qpel_uw_pixels_w32_neon_8;
        c->put_hevc_qpel_uni[8][0][0]  = ff_hevc_put_qpel_uw_pixels_w48_neon_8;
        c->put_hevc_qpel_uni[9][0][0]  = ff_hevc_put_qpel_uw_pixels_w64_neon_8;

        c->put_hevc_qpel_bi[1][0][0]  = ff_hevc_put_qpel_bi_pixels_w4_neon_8;
        c->put_hevc_qpel_bi[3][0][0]  = ff_hevc_put_qpel_bi_pixels_w8_neon_8;
        c->put_hevc_qpel_bi[4][0][0]  = ff_hevc_put_qpel_bi_pixels_w12_neon_8;
        c->put_hevc_qpel_bi[5][0][0]  = ff_hevc_put_qpel_bi_pixels_w16_neon_8;
        c->put_hevc_qpel_bi[6][0][0]  = ff_hevc_put_qpel_bi_pixels_w24_neon_8;
        c->put_hevc_qpel_bi[7][0][0]  = ff_hevc_put_qpel_bi_pixels_w32_neon_8;
        c->put_hevc_qpel_bi[8][0][0]  = ff_hevc_put_qpel_bi_pixels_w48_neon_8;
        c->put_hevc_qpel_bi[9][0][0]  = ff_hevc_put_qpel_bi_pixels_w64_neon_8;
    }
}

av_cold void green_reload_filter_luma7_neon(luma_config *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        /* TODO Update with real 7 taps, not Legacy */
//        put_hevc_qpel_neon[1][0]         = ff_hevc_put_qpel_v1_neon_8;
//        put_hevc_qpel_neon[2][0]         = ff_hevc_put_qpel_v2_neon_8;
//        put_hevc_qpel_neon[3][0]         = ff_hevc_put_qpel_v3_neon_8;
//        put_hevc_qpel_neon[0][1]         = ff_hevc_put_qpel_h1_neon_8;
//        put_hevc_qpel_neon[0][2]         = ff_hevc_put_qpel_h2_neon_8;
//        put_hevc_qpel_neon[0][3]         = ff_hevc_put_qpel_h3_neon_8;
//        put_hevc_qpel_neon[1][1]         = ff_hevc_put_qpel_h1v1_neon_8;
//        put_hevc_qpel_neon[1][2]         = ff_hevc_put_qpel_h2v1_neon_8;
//        put_hevc_qpel_neon[1][3]         = ff_hevc_put_qpel_h3v1_neon_8;
//        put_hevc_qpel_neon[2][1]         = ff_hevc_put_qpel_h1v2_neon_8;
//        put_hevc_qpel_neon[2][2]         = ff_hevc_put_qpel_h2v2_neon_8;
//        put_hevc_qpel_neon[2][3]         = ff_hevc_put_qpel_h3v2_neon_8;
//        put_hevc_qpel_neon[3][1]         = ff_hevc_put_qpel_h1v3_neon_8;
//        put_hevc_qpel_neon[3][2]         = ff_hevc_put_qpel_h2v3_neon_8;
//        put_hevc_qpel_neon[3][3]         = ff_hevc_put_qpel_h3v3_neon_8;
//        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
//        put_hevc_qpel_uw_neon[1][0]      = ff_hevc_put_qpel_uw_v1_neon_8;
//        put_hevc_qpel_uw_neon[2][0]      = ff_hevc_put_qpel_uw_v2_neon_8;
//        put_hevc_qpel_uw_neon[3][0]      = ff_hevc_put_qpel_uw_v3_neon_8;
//        put_hevc_qpel_uw_neon[0][1]      = ff_hevc_put_qpel_uw_h1_neon_8;
//        put_hevc_qpel_uw_neon[0][2]      = ff_hevc_put_qpel_uw_h2_neon_8;
//        put_hevc_qpel_uw_neon[0][3]      = ff_hevc_put_qpel_uw_h3_neon_8;
//        put_hevc_qpel_uw_neon[1][1]      = ff_hevc_put_qpel_uw_h1v1_neon_8;
//        put_hevc_qpel_uw_neon[1][2]      = ff_hevc_put_qpel_uw_h2v1_neon_8;
//        put_hevc_qpel_uw_neon[1][3]      = ff_hevc_put_qpel_uw_h3v1_neon_8;
//        put_hevc_qpel_uw_neon[2][1]      = ff_hevc_put_qpel_uw_h1v2_neon_8;
//        put_hevc_qpel_uw_neon[2][2]      = ff_hevc_put_qpel_uw_h2v2_neon_8;
//        put_hevc_qpel_uw_neon[2][3]      = ff_hevc_put_qpel_uw_h3v2_neon_8;
//        put_hevc_qpel_uw_neon[3][1]      = ff_hevc_put_qpel_uw_h1v3_neon_8;
//        put_hevc_qpel_uw_neon[3][2]      = ff_hevc_put_qpel_uw_h2v3_neon_8;
//        put_hevc_qpel_uw_neon[3][3]      = ff_hevc_put_qpel_uw_h3v3_neon_8;
//
//        for (x = 0; x < 10; x++) {
//            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_pixels_neon_8_fcts[x];
//            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
//            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
//            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
//            //c->put_hevc_qpel_uni[x][0][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
//            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
//            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
//            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
//            //c->put_hevc_qpel_bi[x][0][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
//            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
//            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
//            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
//        }
    }
}

static void ff_hevc_put_epel1_neon_wrapper(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {
    int idx = ff_hevc_green_pel_weight[width];
    int offset = ((mx & (mx-1)) >> 2) + ((my & (my-1)) >> 2)*srcstride;

	ff_hevc_put_epel_pixels_neon_green_8_fcts[idx](dst, src + offset, srcstride, height, mx, my, width);
}


static void ff_hevc_put_epel1_uni_neon_wrapper(
		uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {
    int idx = ff_hevc_green_pel_weight[width];
    int offset = ((mx & (mx-1)) >> 2) + ((my & (my-1)) >> 2)*srcstride;

    ff_hevc_put_epel_uni_pixels_neon_green_8_fcts[idx](dst, dststride, src + offset, srcstride, height, mx, my, width);
}

static void ff_hevc_put_epel1_bi_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
										int16_t *src2, int height, intptr_t mx, intptr_t my, int width) {
    int idx = ff_hevc_green_pel_weight[width];
    int offset = ((mx & (mx-1)) >> 2) + ((my & (my-1)) >> 2)*srcstride;

    /* No NEON fct are defined here :( */
//	put_hevc_pel_bi_pixels_green_8(dst, dststride, src+offset, srcstride, src2, src2stride, height, mx, my, width);
    ff_hevc_put_epel_bi_pixels_neon_green_8_fcts[idx](dst, dststride, src+offset, srcstride, src2, height, mx, my, width);
}

av_cold void green_reload_filter_chroma1_neon(chroma_config *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;

        for (x = 0; x < 10; x++) {
            c->put_hevc_epel[x][0][0]         = ff_hevc_put_epel_pixels_neon_green_8_fcts[x];
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel1_neon_wrapper;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel1_neon_wrapper;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel1_neon_wrapper;
            c->put_hevc_epel_uni[x][0][0]	  = ff_hevc_put_epel1_uni_neon_wrapper;
            c->put_hevc_epel_uni[x][1][0]	  = ff_hevc_put_epel1_uni_neon_wrapper;
            c->put_hevc_epel_uni[x][0][1]	  = ff_hevc_put_epel1_uni_neon_wrapper;
            c->put_hevc_epel_uni[x][1][1]	  = ff_hevc_put_epel1_uni_neon_wrapper;
            c->put_hevc_epel_bi[x][0][0]	  = ff_hevc_put_epel1_bi_neon_wrapper;
            c->put_hevc_epel_bi[x][1][0]	  = ff_hevc_put_epel1_bi_neon_wrapper;
            c->put_hevc_epel_bi[x][0][1]	  = ff_hevc_put_epel1_bi_neon_wrapper;
            c->put_hevc_epel_bi[x][1][1]	  = ff_hevc_put_epel1_bi_neon_wrapper;
        }

        c->put_hevc_epel_uni[0][0][0]  = ff_hevc_put_epel_uni_pixels_w2_neon_8;
        c->put_hevc_epel_uni[1][0][0]  = ff_hevc_put_epel_uni_pixels_w4_neon_8;
        c->put_hevc_epel_uni[2][0][0]  = ff_hevc_put_epel_uni_pixels_w6_neon_8;
        c->put_hevc_epel_uni[3][0][0]  = ff_hevc_put_epel_uni_pixels_w8_neon_8;
        c->put_hevc_epel_uni[4][0][0]  = ff_hevc_put_epel_uni_pixels_w12_neon_8;
        c->put_hevc_epel_uni[5][0][0]  = ff_hevc_put_epel_uni_pixels_w16_neon_8;
        c->put_hevc_epel_uni[6][0][0]  = ff_hevc_put_epel_uni_pixels_w24_neon_8;
        c->put_hevc_epel_uni[7][0][0]  = ff_hevc_put_epel_uni_pixels_w32_neon_8;

        c->put_hevc_epel_bi[0][0][0]  = ff_hevc_put_epel_bi_pixels_w2_neon_8;
        c->put_hevc_epel_bi[1][0][0]  = ff_hevc_put_epel_bi_pixels_w4_neon_8;
        c->put_hevc_epel_bi[2][0][0]  = ff_hevc_put_epel_bi_pixels_w6_neon_8;
        c->put_hevc_epel_bi[3][0][0]  = ff_hevc_put_epel_bi_pixels_w8_neon_8;
        c->put_hevc_epel_bi[4][0][0]  = ff_hevc_put_epel_bi_pixels_w12_neon_8;
        c->put_hevc_epel_bi[5][0][0]  = ff_hevc_put_epel_bi_pixels_w16_neon_8;
        c->put_hevc_epel_bi[6][0][0]  = ff_hevc_put_epel_bi_pixels_w24_neon_8;
        c->put_hevc_epel_bi[7][0][0]  = ff_hevc_put_epel_bi_pixels_w32_neon_8;
    }
}

av_cold void green_reload_filter_chroma2_neon(chroma_config *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;

        for (x = 0; x < 10; x++) {
            c->put_hevc_epel[x][0][0]         = ff_hevc_put_epel_pixels_neon_green_8_fcts[x];
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel2_v_neon_8;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel2_h_neon_8;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel2_hv_neon_8;
            c->put_hevc_epel_uni[x][0][1]	  = ff_hevc_epel2_uni_h_neon_8;
            c->put_hevc_epel_uni[x][1][0]	  = ff_hevc_epel2_uni_v_neon_8;
            c->put_hevc_epel_uni[x][1][1]	  = ff_hevc_epel2_uni_hv_neon_8;
            c->put_hevc_epel_bi[x][0][1]	  = ff_hevc_epel2_bi_h_neon_8;
            c->put_hevc_epel_bi[x][1][0]	  = ff_hevc_epel2_bi_v_neon_8;
            c->put_hevc_epel_bi[x][1][1]	  = ff_hevc_epel2_bi_hv_neon_8;
        }

        c->put_hevc_epel_uni[0][0][0]  = ff_hevc_put_epel_uni_pixels_w2_neon_8;
        c->put_hevc_epel_uni[1][0][0]  = ff_hevc_put_epel_uni_pixels_w4_neon_8;
        c->put_hevc_epel_uni[2][0][0]  = ff_hevc_put_epel_uni_pixels_w6_neon_8;
        c->put_hevc_epel_uni[3][0][0]  = ff_hevc_put_epel_uni_pixels_w8_neon_8;
        c->put_hevc_epel_uni[4][0][0]  = ff_hevc_put_epel_uni_pixels_w12_neon_8;
        c->put_hevc_epel_uni[5][0][0]  = ff_hevc_put_epel_uni_pixels_w16_neon_8;
        c->put_hevc_epel_uni[6][0][0]  = ff_hevc_put_epel_uni_pixels_w24_neon_8;
        c->put_hevc_epel_uni[7][0][0]  = ff_hevc_put_epel_uni_pixels_w32_neon_8;

        c->put_hevc_epel_bi[0][0][0]  = ff_hevc_put_epel_bi_pixels_w2_neon_8;
        c->put_hevc_epel_bi[1][0][0]  = ff_hevc_put_epel_bi_pixels_w4_neon_8;
        c->put_hevc_epel_bi[2][0][0]  = ff_hevc_put_epel_bi_pixels_w6_neon_8;
        c->put_hevc_epel_bi[3][0][0]  = ff_hevc_put_epel_bi_pixels_w8_neon_8;
        c->put_hevc_epel_bi[4][0][0]  = ff_hevc_put_epel_bi_pixels_w12_neon_8;
        c->put_hevc_epel_bi[5][0][0]  = ff_hevc_put_epel_bi_pixels_w16_neon_8;
        c->put_hevc_epel_bi[6][0][0]  = ff_hevc_put_epel_bi_pixels_w24_neon_8;
        c->put_hevc_epel_bi[7][0][0]  = ff_hevc_put_epel_bi_pixels_w32_neon_8;
    }
}

av_cold void green_reload_filter_chroma3_neon(chroma_config *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;

        for (x = 0; x < 10; x++) {
            c->put_hevc_epel[x][0][0]         = ff_hevc_put_epel_pixels_neon_green_8_fcts[x];
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel3_v_neon_8;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel3_h_neon_8;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel3_hv_neon_8;
            c->put_hevc_epel_uni[x][0][1]	  = ff_hevc_epel3_uni_h_neon_8;
            c->put_hevc_epel_uni[x][1][0]	  = ff_hevc_epel3_uni_v_neon_8;
            c->put_hevc_epel_uni[x][1][1]	  = ff_hevc_epel3_uni_hv_neon_8;
            c->put_hevc_epel_bi[x][0][1]	  = ff_hevc_epel3_bi_h_neon_8;
            c->put_hevc_epel_bi[x][1][0]	  = ff_hevc_epel3_bi_v_neon_8;
            c->put_hevc_epel_bi[x][1][1]	  = ff_hevc_epel3_bi_hv_neon_8;
        }

        c->put_hevc_epel_uni[0][0][0]  = ff_hevc_put_epel_uni_pixels_w2_neon_8;
        c->put_hevc_epel_uni[1][0][0]  = ff_hevc_put_epel_uni_pixels_w4_neon_8;
        c->put_hevc_epel_uni[2][0][0]  = ff_hevc_put_epel_uni_pixels_w6_neon_8;
        c->put_hevc_epel_uni[3][0][0]  = ff_hevc_put_epel_uni_pixels_w8_neon_8;
        c->put_hevc_epel_uni[4][0][0]  = ff_hevc_put_epel_uni_pixels_w12_neon_8;
        c->put_hevc_epel_uni[5][0][0]  = ff_hevc_put_epel_uni_pixels_w16_neon_8;
        c->put_hevc_epel_uni[6][0][0]  = ff_hevc_put_epel_uni_pixels_w24_neon_8;
        c->put_hevc_epel_uni[7][0][0]  = ff_hevc_put_epel_uni_pixels_w32_neon_8;

        c->put_hevc_epel_bi[0][0][0]  = ff_hevc_put_epel_bi_pixels_w2_neon_8;
        c->put_hevc_epel_bi[1][0][0]  = ff_hevc_put_epel_bi_pixels_w4_neon_8;
        c->put_hevc_epel_bi[2][0][0]  = ff_hevc_put_epel_bi_pixels_w6_neon_8;
        c->put_hevc_epel_bi[3][0][0]  = ff_hevc_put_epel_bi_pixels_w8_neon_8;
        c->put_hevc_epel_bi[4][0][0]  = ff_hevc_put_epel_bi_pixels_w12_neon_8;
        c->put_hevc_epel_bi[5][0][0]  = ff_hevc_put_epel_bi_pixels_w16_neon_8;
        c->put_hevc_epel_bi[6][0][0]  = ff_hevc_put_epel_bi_pixels_w24_neon_8;
        c->put_hevc_epel_bi[7][0][0]  = ff_hevc_put_epel_bi_pixels_w32_neon_8;
    }
}
