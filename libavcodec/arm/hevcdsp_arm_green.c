/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */
#if CONFIG_GREEN

#define QPEL_FUNC(name) \
    void name(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride, \
                                   int height, int width)
/* Eco1 Filters Morgan */
QPEL_FUNC(ff_hevc_put_qpel1_v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h1v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h1v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h1v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h2v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h2v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h2v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h3v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h3v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel1_h3v3_neon_8);

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
#undef QPEL_FUNC

#define QPEL_FUNC_UW(name) \
    void name(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, \
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);
/* Eco1 Filters Morgan */
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_pixels_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h1v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h1v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h1v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h2v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h2v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h2v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h3v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h3v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel1_uw_h3v3_neon_8);

/* Eco3 Filters Morgan */
QPEL_FUNC_UW(ff_hevc_put_qpel3_uw_pixels_neon_8);
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
QPEL_FUNC_UW(ff_hevc_put_qpel5_uw_pixels_neon_8);
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
#undef QPEL_FUNC_UW

void ff_hevc_put_epel2_h_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
void ff_hevc_put_epel2_v_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
void ff_hevc_put_epel2_hv_neon_8(int16_t *dst, ptrdiff_t dststride, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);

#if HAVE_NEON
av_cold void green_reload_filter_luma1(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        put_hevc_qpel_neon[1][0]         = ff_hevc_put_qpel1_v1_neon_8;
        put_hevc_qpel_neon[2][0]         = ff_hevc_put_qpel1_v2_neon_8;
        put_hevc_qpel_neon[3][0]         = ff_hevc_put_qpel1_v3_neon_8;
        put_hevc_qpel_neon[0][1]         = ff_hevc_put_qpel1_h1_neon_8;
        put_hevc_qpel_neon[0][2]         = ff_hevc_put_qpel1_h2_neon_8;
        put_hevc_qpel_neon[0][3]         = ff_hevc_put_qpel1_h3_neon_8;
        put_hevc_qpel_neon[1][1]         = ff_hevc_put_qpel1_h1v1_neon_8;
        put_hevc_qpel_neon[1][2]         = ff_hevc_put_qpel1_h2v1_neon_8;
        put_hevc_qpel_neon[1][3]         = ff_hevc_put_qpel1_h3v1_neon_8;
        put_hevc_qpel_neon[2][1]         = ff_hevc_put_qpel1_h1v2_neon_8;
        put_hevc_qpel_neon[2][2]         = ff_hevc_put_qpel1_h2v2_neon_8;
        put_hevc_qpel_neon[2][3]         = ff_hevc_put_qpel1_h3v2_neon_8;
        put_hevc_qpel_neon[3][1]         = ff_hevc_put_qpel1_h1v3_neon_8;
        put_hevc_qpel_neon[3][2]         = ff_hevc_put_qpel1_h2v3_neon_8;
        put_hevc_qpel_neon[3][3]         = ff_hevc_put_qpel1_h3v3_neon_8;
        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_uw_neon[1][0]      = ff_hevc_put_qpel1_uw_v1_neon_8;
        put_hevc_qpel_uw_neon[2][0]      = ff_hevc_put_qpel1_uw_v2_neon_8;
        put_hevc_qpel_uw_neon[3][0]      = ff_hevc_put_qpel1_uw_v3_neon_8;
        put_hevc_qpel_uw_neon[0][1]      = ff_hevc_put_qpel1_uw_h1_neon_8;
        put_hevc_qpel_uw_neon[0][2]      = ff_hevc_put_qpel1_uw_h2_neon_8;
        put_hevc_qpel_uw_neon[0][3]      = ff_hevc_put_qpel1_uw_h3_neon_8;
        put_hevc_qpel_uw_neon[1][1]      = ff_hevc_put_qpel1_uw_h1v1_neon_8;
        put_hevc_qpel_uw_neon[1][2]      = ff_hevc_put_qpel1_uw_h2v1_neon_8;
        put_hevc_qpel_uw_neon[1][3]      = ff_hevc_put_qpel1_uw_h3v1_neon_8;
        put_hevc_qpel_uw_neon[2][1]      = ff_hevc_put_qpel1_uw_h1v2_neon_8;
        put_hevc_qpel_uw_neon[2][2]      = ff_hevc_put_qpel1_uw_h2v2_neon_8;
        put_hevc_qpel_uw_neon[2][3]      = ff_hevc_put_qpel1_uw_h3v2_neon_8;
        put_hevc_qpel_uw_neon[3][1]      = ff_hevc_put_qpel1_uw_h1v3_neon_8;
        put_hevc_qpel_uw_neon[3][2]      = ff_hevc_put_qpel1_uw_h2v3_neon_8;
        put_hevc_qpel_uw_neon[3][3]      = ff_hevc_put_qpel1_uw_h3v3_neon_8;

        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
            //c->put_hevc_qpel_uni[x][0][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            //c->put_hevc_qpel_bi[x][0][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
        }
    }

    c->green_cur_luma=1;
}


av_cold void green_reload_filter_luma3(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        put_hevc_qpel_neon[1][0]         = ff_hevc_put_qpel3_v1_neon_8;
        put_hevc_qpel_neon[2][0]         = ff_hevc_put_qpel3_v2_neon_8;
        put_hevc_qpel_neon[3][0]         = ff_hevc_put_qpel3_v3_neon_8;
        put_hevc_qpel_neon[0][1]         = ff_hevc_put_qpel3_h1_neon_8;
        put_hevc_qpel_neon[0][2]         = ff_hevc_put_qpel3_h2_neon_8;
        put_hevc_qpel_neon[0][3]         = ff_hevc_put_qpel3_h3_neon_8;
        put_hevc_qpel_neon[1][1]         = ff_hevc_put_qpel3_h1v1_neon_8;
        put_hevc_qpel_neon[1][2]         = ff_hevc_put_qpel3_h2v1_neon_8;
        put_hevc_qpel_neon[1][3]         = ff_hevc_put_qpel3_h3v1_neon_8;
        put_hevc_qpel_neon[2][1]         = ff_hevc_put_qpel3_h1v2_neon_8;
        put_hevc_qpel_neon[2][2]         = ff_hevc_put_qpel3_h2v2_neon_8;
        put_hevc_qpel_neon[2][3]         = ff_hevc_put_qpel3_h3v2_neon_8;
        put_hevc_qpel_neon[3][1]         = ff_hevc_put_qpel3_h1v3_neon_8;
        put_hevc_qpel_neon[3][2]         = ff_hevc_put_qpel3_h2v3_neon_8;
        put_hevc_qpel_neon[3][3]         = ff_hevc_put_qpel3_h3v3_neon_8;
        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_uw_neon[1][0]      = ff_hevc_put_qpel3_uw_v1_neon_8;
        put_hevc_qpel_uw_neon[2][0]      = ff_hevc_put_qpel3_uw_v2_neon_8;
        put_hevc_qpel_uw_neon[3][0]      = ff_hevc_put_qpel3_uw_v3_neon_8;
        put_hevc_qpel_uw_neon[0][1]      = ff_hevc_put_qpel3_uw_h1_neon_8;
        put_hevc_qpel_uw_neon[0][2]      = ff_hevc_put_qpel3_uw_h2_neon_8;
        put_hevc_qpel_uw_neon[0][3]      = ff_hevc_put_qpel3_uw_h3_neon_8;
        put_hevc_qpel_uw_neon[1][1]      = ff_hevc_put_qpel3_uw_h1v1_neon_8;
        put_hevc_qpel_uw_neon[1][2]      = ff_hevc_put_qpel3_uw_h2v1_neon_8;
        put_hevc_qpel_uw_neon[1][3]      = ff_hevc_put_qpel3_uw_h3v1_neon_8;
        put_hevc_qpel_uw_neon[2][1]      = ff_hevc_put_qpel3_uw_h1v2_neon_8;
        put_hevc_qpel_uw_neon[2][2]      = ff_hevc_put_qpel3_uw_h2v2_neon_8;
        put_hevc_qpel_uw_neon[2][3]      = ff_hevc_put_qpel3_uw_h3v2_neon_8;
        put_hevc_qpel_uw_neon[3][1]      = ff_hevc_put_qpel3_uw_h1v3_neon_8;
        put_hevc_qpel_uw_neon[3][2]      = ff_hevc_put_qpel3_uw_h2v3_neon_8;
        put_hevc_qpel_uw_neon[3][3]      = ff_hevc_put_qpel3_uw_h3v3_neon_8;

        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
            //c->put_hevc_qpel_uni[x][0][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            //c->put_hevc_qpel_bi[x][0][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
        }
    }
    c->green_cur_luma=3;
}

av_cold void green_reload_filter_luma5(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        put_hevc_qpel_neon[1][0]         = ff_hevc_put_qpel5_v1_neon_8;
        put_hevc_qpel_neon[2][0]         = ff_hevc_put_qpel5_v2_neon_8;
        put_hevc_qpel_neon[3][0]         = ff_hevc_put_qpel5_v3_neon_8;
        put_hevc_qpel_neon[0][1]         = ff_hevc_put_qpel5_h1_neon_8;
        put_hevc_qpel_neon[0][2]         = ff_hevc_put_qpel5_h2_neon_8;
        put_hevc_qpel_neon[0][3]         = ff_hevc_put_qpel5_h3_neon_8;
        put_hevc_qpel_neon[1][1]         = ff_hevc_put_qpel5_h1v1_neon_8;
        put_hevc_qpel_neon[1][2]         = ff_hevc_put_qpel5_h2v1_neon_8;
        put_hevc_qpel_neon[1][3]         = ff_hevc_put_qpel5_h3v1_neon_8;
        put_hevc_qpel_neon[2][1]         = ff_hevc_put_qpel5_h1v2_neon_8;
        put_hevc_qpel_neon[2][2]         = ff_hevc_put_qpel5_h2v2_neon_8;
        put_hevc_qpel_neon[2][3]         = ff_hevc_put_qpel5_h3v2_neon_8;
        put_hevc_qpel_neon[3][1]         = ff_hevc_put_qpel5_h1v3_neon_8;
        put_hevc_qpel_neon[3][2]         = ff_hevc_put_qpel5_h2v3_neon_8;
        put_hevc_qpel_neon[3][3]         = ff_hevc_put_qpel5_h3v3_neon_8;
        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_uw_neon[1][0]      = ff_hevc_put_qpel5_uw_v1_neon_8;
        put_hevc_qpel_uw_neon[2][0]      = ff_hevc_put_qpel5_uw_v2_neon_8;
        put_hevc_qpel_uw_neon[3][0]      = ff_hevc_put_qpel5_uw_v3_neon_8;
        put_hevc_qpel_uw_neon[0][1]      = ff_hevc_put_qpel5_uw_h1_neon_8;
        put_hevc_qpel_uw_neon[0][2]      = ff_hevc_put_qpel5_uw_h2_neon_8;
        put_hevc_qpel_uw_neon[0][3]      = ff_hevc_put_qpel5_uw_h3_neon_8;
        put_hevc_qpel_uw_neon[1][1]      = ff_hevc_put_qpel5_uw_h1v1_neon_8;
        put_hevc_qpel_uw_neon[1][2]      = ff_hevc_put_qpel5_uw_h2v1_neon_8;
        put_hevc_qpel_uw_neon[1][3]      = ff_hevc_put_qpel5_uw_h3v1_neon_8;
        put_hevc_qpel_uw_neon[2][1]      = ff_hevc_put_qpel5_uw_h1v2_neon_8;
        put_hevc_qpel_uw_neon[2][2]      = ff_hevc_put_qpel5_uw_h2v2_neon_8;
        put_hevc_qpel_uw_neon[2][3]      = ff_hevc_put_qpel5_uw_h3v2_neon_8;
        put_hevc_qpel_uw_neon[3][1]      = ff_hevc_put_qpel5_uw_h1v3_neon_8;
        put_hevc_qpel_uw_neon[3][2]      = ff_hevc_put_qpel5_uw_h2v3_neon_8;
        put_hevc_qpel_uw_neon[3][3]      = ff_hevc_put_qpel5_uw_h3v3_neon_8;

        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
            //c->put_hevc_qpel_uni[x][0][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            //c->put_hevc_qpel_bi[x][0][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
        }
    }
    c->green_cur_luma=5;
}

av_cold void green_reload_filter_luma7(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        put_hevc_qpel_neon[1][0]         = ff_hevc_put_qpel_v1_neon_8;
        put_hevc_qpel_neon[2][0]         = ff_hevc_put_qpel_v2_neon_8;
        put_hevc_qpel_neon[3][0]         = ff_hevc_put_qpel_v3_neon_8;
        put_hevc_qpel_neon[0][1]         = ff_hevc_put_qpel_h1_neon_8;
        put_hevc_qpel_neon[0][2]         = ff_hevc_put_qpel_h2_neon_8;
        put_hevc_qpel_neon[0][3]         = ff_hevc_put_qpel_h3_neon_8;
        put_hevc_qpel_neon[1][1]         = ff_hevc_put_qpel_h1v1_neon_8;
        put_hevc_qpel_neon[1][2]         = ff_hevc_put_qpel_h2v1_neon_8;
        put_hevc_qpel_neon[1][3]         = ff_hevc_put_qpel_h3v1_neon_8;
        put_hevc_qpel_neon[2][1]         = ff_hevc_put_qpel_h1v2_neon_8;
        put_hevc_qpel_neon[2][2]         = ff_hevc_put_qpel_h2v2_neon_8;
        put_hevc_qpel_neon[2][3]         = ff_hevc_put_qpel_h3v2_neon_8;
        put_hevc_qpel_neon[3][1]         = ff_hevc_put_qpel_h1v3_neon_8;
        put_hevc_qpel_neon[3][2]         = ff_hevc_put_qpel_h2v3_neon_8;
        put_hevc_qpel_neon[3][3]         = ff_hevc_put_qpel_h3v3_neon_8;
        //put_hevc_qpel_uw_neon[0][0]      = ff_hevc_put_qpel_uw_pixels_neon_8;
        put_hevc_qpel_uw_neon[1][0]      = ff_hevc_put_qpel_uw_v1_neon_8;
        put_hevc_qpel_uw_neon[2][0]      = ff_hevc_put_qpel_uw_v2_neon_8;
        put_hevc_qpel_uw_neon[3][0]      = ff_hevc_put_qpel_uw_v3_neon_8;
        put_hevc_qpel_uw_neon[0][1]      = ff_hevc_put_qpel_uw_h1_neon_8;
        put_hevc_qpel_uw_neon[0][2]      = ff_hevc_put_qpel_uw_h2_neon_8;
        put_hevc_qpel_uw_neon[0][3]      = ff_hevc_put_qpel_uw_h3_neon_8;
        put_hevc_qpel_uw_neon[1][1]      = ff_hevc_put_qpel_uw_h1v1_neon_8;
        put_hevc_qpel_uw_neon[1][2]      = ff_hevc_put_qpel_uw_h2v1_neon_8;
        put_hevc_qpel_uw_neon[1][3]      = ff_hevc_put_qpel_uw_h3v1_neon_8;
        put_hevc_qpel_uw_neon[2][1]      = ff_hevc_put_qpel_uw_h1v2_neon_8;
        put_hevc_qpel_uw_neon[2][2]      = ff_hevc_put_qpel_uw_h2v2_neon_8;
        put_hevc_qpel_uw_neon[2][3]      = ff_hevc_put_qpel_uw_h3v2_neon_8;
        put_hevc_qpel_uw_neon[3][1]      = ff_hevc_put_qpel_uw_h1v3_neon_8;
        put_hevc_qpel_uw_neon[3][2]      = ff_hevc_put_qpel_uw_h2v3_neon_8;
        put_hevc_qpel_uw_neon[3][3]      = ff_hevc_put_qpel_uw_h3v3_neon_8;

        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
            //c->put_hevc_qpel_uni[x][0][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            //c->put_hevc_qpel_bi[x][0][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
        }
    }
    c->green_cur_luma=7;
}

av_cold void green_reload_filter_chroma1(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;

        for (x = 0; x < 10; x++) {
            c->put_hevc_epel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_pixels_neon_8;
        }
    }
    c->green_cur_chroma=1;
}

av_cold void green_reload_filter_chroma2(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;

        for (x = 0; x < 10; x++) {
            c->put_hevc_epel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel2_v_neon_8;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel2_h_neon_8;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel2_hv_neon_8;
        }
    }
    c->green_cur_chroma=2;
}

av_cold void green_reload_filter_chroma4(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;

        for (x = 0; x < 10; x++) {
            c->put_hevc_epel[x][0][0]         = ff_hevc_put_pixels_neon_8;
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel_v_neon_8;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel_h_neon_8;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel_hv_neon_8;
        }
    }
    c->green_cur_chroma=4;
}

#endif
#endif
