#ifndef AVCODEC_X86_HEVCDSP_H
#define AVCODEC_X86_HEVCDSP_H

struct SAOParams;
struct AVFrame;
struct UpsamplInf;
struct HEVCWindow;

//#define OPTI_ASM

#define PEL_LINK_ASM(dst, idx1, idx2, idx3, name, D) \
dst[idx1][idx2][idx3] = ff_hevc_put_hevc_ ## name ## _ ## D ## _sse4
#define PEL_LINK_SSE(dst, idx1, idx2, idx3, name, D) \
dst[idx1][idx2][idx3] = ff_hevc_put_hevc_ ## name ## _ ## D ## _sse; \
dst ## _w[idx1][idx2][idx3][0] = ff_hevc_put_hevc_ ## name ## _w0_ ## D ## _sse; \
dst ## _w[idx1][idx2][idx3][1] = ff_hevc_put_hevc_ ## name ## _w1_ ## D ## _sse; \
dst ## _w[idx1][idx2][idx3][2] = ff_hevc_put_hevc_ ## name ## _w2_ ## D ## _sse; \
dst ## _w[idx1][idx2][idx3][3] = ff_hevc_put_hevc_ ## name ## _w3_ ## D ## _sse

#ifdef OPTI_ASM
#define PEL_LINK(dst, idx1, idx2, idx3, name, D) \
PEL_LINK_ASM(dst, idx1, idx2, idx3, name, D)
#else
#define PEL_LINK(dst, idx1, idx2, idx3, name, D) \
PEL_LINK_SSE(dst, idx1, idx2, idx3, name, D)
#endif

#define EPEL_PROTOTYPE_ASM(name, D) \
void ff_hevc_put_hevc_epel_ ## name ## _ ## D ## _sse4(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer)
#define EPEL_PROTOTYPE_SSE(name, D) \
void ff_hevc_put_hevc_epel_ ## name ## _ ## D ## _sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer); \
void ff_hevc_put_hevc_epel_ ## name ## _w0_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx, int my, int16_t* mcbuffer); \
void ff_hevc_put_hevc_epel_ ## name ## _w1_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx, int my, int16_t* mcbuffer); \
void ff_hevc_put_hevc_epel_ ## name ## _w2_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx, int my, int16_t* mcbuffer); \
void ff_hevc_put_hevc_epel_ ## name ## _w3_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx, int my, int16_t* mcbuffer)


#ifdef OPTI_ASM
#define EPEL_PROTOTYPE(name, D) \
EPEL_PROTOTYPE_ASM(name, D)
#else
#define EPEL_PROTOTYPE(name, D) \
EPEL_PROTOTYPE_SSE(name, D)
#endif

#define QPEL_PROTOTYPE_ASM(name, D) \
void ff_hevc_put_hevc_qpel_ ## name ## _ ## D ## _sse4(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
#define QPEL_PROTOTYPE_SSE(name, D) \
void ff_hevc_put_hevc_qpel_ ## name ## _ ## D ## _sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer); \
void ff_hevc_put_hevc_qpel_ ## name ## _w0_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer); \
void ff_hevc_put_hevc_qpel_ ## name ## _w1_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer); \
void ff_hevc_put_hevc_qpel_ ## name ## _w2_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer); \
void ff_hevc_put_hevc_qpel_ ## name ## _w3_ ## D ## _sse(uint8_t denom, int16_t wlxFlag, int16_t wl1Flag, int16_t olxFlag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride, int16_t *src1, ptrdiff_t src1stride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer)

#ifdef OPTI_ASM
#define QPEL_PROTOTYPE(name, D) \
QPEL_PROTOTYPE_ASM(name, D)
#else
#define QPEL_PROTOTYPE(name, D) \
QPEL_PROTOTYPE_SSE(name, D)
#endif
//IDCT functions

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

// MC functions
void ff_hevc_put_unweighted_pred_8_sse(uint8_t *_dst, ptrdiff_t _dststride,int16_t *src, ptrdiff_t srcstride,int width, int height);

void ff_hevc_weighted_pred_8_sse(uint8_t denom, int16_t wlxFlag, int16_t olxFlag,uint8_t *_dst, ptrdiff_t _dststride,int16_t *src, ptrdiff_t srcstride,int width, int height);
void ff_hevc_put_weighted_pred_avg_8_sse(uint8_t *_dst, ptrdiff_t _dststride,int16_t *src1, int16_t *src2, ptrdiff_t srcstride,int width, int height);

void ff_hevc_weighted_pred_avg_8_sse(uint8_t denom, int16_t wl0Flag, int16_t wl1Flag,int16_t ol0Flag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride,int16_t *src1, int16_t *src2, ptrdiff_t srcstride,int width, int height);

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
EPEL_PROTOTYPE(pixels2 ,  8);
EPEL_PROTOTYPE(pixels4 ,  8);
EPEL_PROTOTYPE(pixels8 ,  8);
EPEL_PROTOTYPE(pixels16,  8);

EPEL_PROTOTYPE(pixels2 , 10);
EPEL_PROTOTYPE(pixels4 , 10);
EPEL_PROTOTYPE(pixels8 , 10);

EPEL_PROTOTYPE(h2 ,  8);
EPEL_PROTOTYPE(h4 ,  8);
EPEL_PROTOTYPE(h8 ,  8);

EPEL_PROTOTYPE(h2 , 10);
EPEL_PROTOTYPE(h4 , 10);

EPEL_PROTOTYPE(v2 ,  8);
EPEL_PROTOTYPE(v4 ,  8);
EPEL_PROTOTYPE(v8 ,  8);
EPEL_PROTOTYPE(v16,  8);

EPEL_PROTOTYPE(v2 , 10);
EPEL_PROTOTYPE(v4 , 10);
EPEL_PROTOTYPE(v8 , 10);

EPEL_PROTOTYPE(hv2 ,  8);
EPEL_PROTOTYPE(hv4 ,  8);
EPEL_PROTOTYPE(hv8 ,  8);

EPEL_PROTOTYPE(hv2 , 10);
EPEL_PROTOTYPE(hv4 , 10);

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
QPEL_PROTOTYPE(pixels4 ,  8);
QPEL_PROTOTYPE(pixels8 ,  8);
QPEL_PROTOTYPE(pixels16,  8);

QPEL_PROTOTYPE(pixels4 , 10);
QPEL_PROTOTYPE(pixels8 , 10);

QPEL_PROTOTYPE_SSE(h4_1 ,  8);
QPEL_PROTOTYPE_SSE(h4_2 ,  8);
QPEL_PROTOTYPE_SSE(h4_3 ,  8);
QPEL_PROTOTYPE_SSE(h8_1 ,  8);
QPEL_PROTOTYPE_SSE(h8_2 ,  8);
QPEL_PROTOTYPE_SSE(h8_3 ,  8);

QPEL_PROTOTYPE_SSE(v4_1 ,  8);
QPEL_PROTOTYPE_SSE(v4_2 ,  8);
QPEL_PROTOTYPE_SSE(v4_3 ,  8);
QPEL_PROTOTYPE_SSE(v8_1 ,  8);
QPEL_PROTOTYPE_SSE(v8_2 ,  8);
QPEL_PROTOTYPE_SSE(v8_3 ,  8);
QPEL_PROTOTYPE_SSE(v16_1,  8);
QPEL_PROTOTYPE_SSE(v16_2,  8);
QPEL_PROTOTYPE_SSE(v16_3,  8);

QPEL_PROTOTYPE_SSE(h4_1_v_1 ,  8);
QPEL_PROTOTYPE_SSE(h4_1_v_2 ,  8);
QPEL_PROTOTYPE_SSE(h4_1_v_3 ,  8);
QPEL_PROTOTYPE_SSE(h4_2_v_1 ,  8);
QPEL_PROTOTYPE_SSE(h4_2_v_2 ,  8);
QPEL_PROTOTYPE_SSE(h4_2_v_3 ,  8);
QPEL_PROTOTYPE_SSE(h4_3_v_1 ,  8);
QPEL_PROTOTYPE_SSE(h4_3_v_2 ,  8);
QPEL_PROTOTYPE_SSE(h4_3_v_3 ,  8);

QPEL_PROTOTYPE_SSE(h8_1_v_1 ,  8);
QPEL_PROTOTYPE_SSE(h8_1_v_2 ,  8);
QPEL_PROTOTYPE_SSE(h8_1_v_3 ,  8);
QPEL_PROTOTYPE_SSE(h8_2_v_1 ,  8);
QPEL_PROTOTYPE_SSE(h8_2_v_2 ,  8);
QPEL_PROTOTYPE_SSE(h8_2_v_3 ,  8);
QPEL_PROTOTYPE_SSE(h8_3_v_1 ,  8);
QPEL_PROTOTYPE_SSE(h8_3_v_2 ,  8);
QPEL_PROTOTYPE_SSE(h8_3_v_3 ,  8);

QPEL_PROTOTYPE_SSE(h2_1 , 10);
QPEL_PROTOTYPE_SSE(h2_2 , 10);
//QPEL_PROTOTYPE_SSE(h2_3 , 10);
//QPEL_PROTOTYPE_SSE(v4_1 , 10);
//QPEL_PROTOTYPE_SSE(v4_2 , 10);
//QPEL_PROTOTYPE_SSE(v4_3 , 10);


// SAO functions

void ff_hevc_sao_edge_filter_0_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);
void ff_hevc_sao_edge_filter_1_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);
void ff_hevc_sao_edge_filter_2_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);
void ff_hevc_sao_edge_filter_3_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);

void ff_hevc_sao_band_filter_0_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);
void ff_hevc_sao_band_filter_1_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);
void ff_hevc_sao_band_filter_2_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);
void ff_hevc_sao_band_filter_3_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);

//#ifdef SVC_EXTENSION
void ff_upsample_base_layer_frame_sse(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
void ff_upsample_base_layer_frame_sse_v(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
void ff_upsample_base_layer_frame_sse_h(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
//#endif

#endif // AVCODEC_X86_HEVCDSP_H
