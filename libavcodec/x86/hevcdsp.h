#ifndef AVCODEC_X86_HEVCDSP_H
#define AVCODEC_X86_HEVCDSP_H

struct SAOParams;
struct AVFrame;
struct UpsamplInf;
struct HEVCWindow;

#define OPTI_ASM

#define PEL_LINK_ASM(dst, idx1, idx2, idx3, name, D) \
dst[idx1][idx2][idx3] = ff_hevc_put_hevc_ ## name ## _ ## D ## _sse4; \
dst ## _bi[idx1][idx2][idx3] = ff_hevc_put_hevc_bi_ ## name ## _ ## D ## _sse4; \
dst ## _uni[idx1][idx2][idx3] = ff_hevc_put_hevc_uni_ ## name ## _ ## D ## _sse4; \
dst ## _uni_w[idx1][idx2][idx3] = ff_hevc_put_hevc_uni_w_ ## name ## _ ## D ## _sse4; \
dst ## _bi_w[idx1][idx2][idx3] = ff_hevc_put_hevc_bi_w_ ## name ## _ ## D ## _sse4

#define PEL_LINK_SSE(dst, idx1, idx2, idx3, name, D) \
dst[idx1][idx2][idx3] = ff_hevc_put_hevc_ ## name ## _ ## D ## _sse; \
dst ## _bi[idx1][idx2][idx3] = ff_hevc_put_hevc_bi_ ## name ## _ ## D ## _sse; \
dst ## _uni[idx1][idx2][idx3] = ff_hevc_put_hevc_uni_ ## name ## _ ## D ## _sse; \
dst ## _uni_w[idx1][idx2][idx3] = ff_hevc_put_hevc_uni_w_ ## name ## _ ## D ## _sse; \
dst ## _bi_w[idx1][idx2][idx3] = ff_hevc_put_hevc_bi_w_ ## name ## _ ## D ## _sse

#ifdef OPTI_ASM
#define PEL_LINK(dst, idx1, idx2, idx3, name, D) \
PEL_LINK_ASM(dst, idx1, idx2, idx3, name, D)

#else
#define PEL_LINK(dst, idx1, idx2, idx3, name, D) \
PEL_LINK_SSE(dst, idx1, idx2, idx3, name, D)
#endif

#define PEL_PROTOTYPE_ASM(name, D) \
void ff_hevc_put_hevc_ ## name ## _ ## D ## _sse4(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my,int width); \
void ff_hevc_put_hevc_bi_ ## name ## _ ## D ## _sse4(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int16_t *src2, ptrdiff_t src2stride, int height, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_uni_ ## name ## _ ## D ## _sse4(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_uni_w_ ## name ## _ ## D ## _sse4(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_bi_w_ ## name ## _ ## D ## _sse4(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int16_t *src2, ptrdiff_t src2stride, int height, int denom, int wx0, int wx1, int ox0, int ox1, intptr_t mx, intptr_t my, int width)

#define PEL_PROTOTYPE_SSE(name, D) \
void ff_hevc_put_hevc_ ## name ## _ ## D ## _sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my,int width); \
void ff_hevc_put_hevc_bi_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int16_t *src2, ptrdiff_t src2stride, int height, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_uni_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_uni_w_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_bi_w_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int16_t *src2, ptrdiff_t src2stride, int height, int denom, int wx0, int wx1, int ox0, int ox1, intptr_t mx, intptr_t my, int width)


#define WEIGHTING_PROTOTYPE(width, bitd) \
void ff_hevc_put_hevc_uni_w##width##_##bitd##_sse4(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride, int height, int denom,  int _wx, int _ox); \
void ff_hevc_put_hevc_bi_w##width##_##bitd##_sse4(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride, int16_t *_src2, ptrdiff_t _src2stride, int height, int denom,  int _wx0,  int _wx1, int _ox0, int _ox1)


#define WEIGHTING_PROTOTYPES(bitd) \
		WEIGHTING_PROTOTYPE(2, bitd); \
		WEIGHTING_PROTOTYPE(4, bitd); \
		WEIGHTING_PROTOTYPE(6, bitd); \
		WEIGHTING_PROTOTYPE(8, bitd); \
		WEIGHTING_PROTOTYPE(12, bitd); \
		WEIGHTING_PROTOTYPE(16, bitd); \
		WEIGHTING_PROTOTYPE(24, bitd); \
		WEIGHTING_PROTOTYPE(32, bitd); \
		WEIGHTING_PROTOTYPE(48, bitd); \
		WEIGHTING_PROTOTYPE(64, bitd)


#ifdef OPTI_ASM
#define PEL_PROTOTYPE(name, D) \
PEL_PROTOTYPE_ASM(name, D)
#else
#define PEL_PROTOTYPE(name, D) \
PEL_PROTOTYPE_SSE(name, D)
#endif

///////////////////////////////////////////////////////////////////////////////
//IDCT functions
///////////////////////////////////////////////////////////////////////////////
void ff_hevc_transform_skip_8_sse(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);

void ff_hevc_transform_4x4_luma_add_8_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_4x4_luma_add_10_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);

void ff_hevc_transform_4x4_add_8_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_4x4_add_10_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_8x8_add_8_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_8x8_add_10_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_16x16_add_8_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_16x16_add_10_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_32x32_add_8_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);
void ff_hevc_transform_32x32_add_10_sse4(uint8_t *_dst, int16_t *coeffs, ptrdiff_t _stride);

void ff_hevc_transform_4x4_dc_add_8_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_8x8_dc_add_8_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_16x16_dc_add_8_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_32x32_dc_add_8_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);

void ff_hevc_transform_4x4_dc_add_10_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_8x8_dc_add_10_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_16x16_dc_add_10_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);
void ff_hevc_transform_32x32_dc_add_10_sse4(uint8_t *dst, int16_t *coeffs, ptrdiff_t stride);

///////////////////////////////////////////////////////////////////////////////
// MC functions
///////////////////////////////////////////////////////////////////////////////
#define EPEL_PROTOTYPES(fname, bitd) \
        PEL_PROTOTYPE(fname##4,  bitd); \
        PEL_PROTOTYPE(fname##6,  bitd); \
        PEL_PROTOTYPE(fname##8,  bitd); \
        PEL_PROTOTYPE(fname##12, bitd); \
        PEL_PROTOTYPE(fname##16, bitd); \
        PEL_PROTOTYPE(fname##24, bitd); \
        PEL_PROTOTYPE(fname##32, bitd); \
        PEL_PROTOTYPE(fname##48, bitd); \
        PEL_PROTOTYPE(fname##64, bitd)

#define QPEL_PROTOTYPES(fname, bitd) \
        PEL_PROTOTYPE(fname##4,  bitd); \
        PEL_PROTOTYPE(fname##8,  bitd); \
        PEL_PROTOTYPE(fname##12, bitd); \
        PEL_PROTOTYPE(fname##16, bitd); \
        PEL_PROTOTYPE(fname##24, bitd); \
        PEL_PROTOTYPE(fname##32, bitd); \
        PEL_PROTOTYPE(fname##48, bitd); \
        PEL_PROTOTYPE(fname##64, bitd)


///////////////////////////////////////////////////////////////////////////////
// QPEL_PIXELS EPEL_PIXELS
///////////////////////////////////////////////////////////////////////////////
EPEL_PROTOTYPES(pel_pixels ,  8);
EPEL_PROTOTYPES(pel_pixels , 10);
///////////////////////////////////////////////////////////////////////////////
// EPEL
///////////////////////////////////////////////////////////////////////////////
EPEL_PROTOTYPES(epel_h ,  8);
EPEL_PROTOTYPES(epel_h , 10);

EPEL_PROTOTYPES(epel_v ,  8);
EPEL_PROTOTYPES(epel_v , 10);

EPEL_PROTOTYPES(epel_hv ,  8);
EPEL_PROTOTYPES(epel_hv , 10);

///////////////////////////////////////////////////////////////////////////////
// QPEL
///////////////////////////////////////////////////////////////////////////////
QPEL_PROTOTYPES(qpel_h ,  8);
QPEL_PROTOTYPES(qpel_h , 10);

QPEL_PROTOTYPES(qpel_v,  8);
QPEL_PROTOTYPES(qpel_v, 10);

QPEL_PROTOTYPES(qpel_hv,  8);
QPEL_PROTOTYPES(qpel_hv, 10);

WEIGHTING_PROTOTYPES(8);
WEIGHTING_PROTOTYPES(10);

/* ASM wrapper */

///////////////////////////////////////////////////////////////////////////////
// SAO functions
///////////////////////////////////////////////////////////////////////////////
void ff_hevc_sao_edge_filter_0_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);
void ff_hevc_sao_edge_filter_1_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);
void ff_hevc_sao_edge_filter_0_10_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);
void ff_hevc_sao_edge_filter_1_10_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);

void ff_hevc_sao_band_filter_0_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);
void ff_hevc_sao_band_filter_0_10_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);

//#ifdef SVC_EXTENSION

    void ff_upsample_filter_block_luma_h_all_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int block_w, int block_h, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_cr_h_all_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_luma_v_all_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_cr_v_all_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);

    void ff_upsample_filter_block_luma_h_x2_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int block_w, int block_h, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_cr_h_x2_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_luma_v_x2_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_cr_v_x2_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);

    void ff_upsample_filter_block_luma_h_x1_5_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int block_w, int block_h, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_cr_h_x1_5_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                int x_EL, int x_BL, int block_w, int block_h, int widthEL,
                const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_luma_v_x1_5_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_cr_v_x1_5_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);

    void ff_upsample_filter_block_luma_h_8_8_sse( int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int block_w, int block_h, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
    void ff_upsample_filter_block_luma_v_8_8_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
            int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
   void ff_upsample_filter_block_cr_h_8_8_sse( int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
            int x_EL, int x_BL, int block_w, int block_h, int widthEL,
            const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
   void ff_upsample_filter_block_cr_v_8_8_sse(uint8_t *dst, ptrdiff_t dststride, int16_t *_src, ptrdiff_t _srcstride,
           int y_BL, int x_EL, int y_EL, int block_w, int block_h, int widthEL, int heightEL,
           const struct HEVCWindow *Enhscal, struct UpsamplInf *up_info);
//#endif




#endif // AVCODEC_X86_HEVCDSP_H
