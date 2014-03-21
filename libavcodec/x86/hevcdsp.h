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
void ff_hevc_put_hevc_ ## name ## _ ## D ## _sse4(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my,int width)

#define PEL_PROTOTYPE_SSE(name, D) \
void ff_hevc_put_hevc_ ## name ## _ ## D ## _sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my,int width); \
void ff_hevc_put_hevc_bi_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int16_t *src2, ptrdiff_t src2stride, int width, int height, intptr_t mx, intptr_t my); \
void ff_hevc_put_hevc_uni_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int height, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_uni_w_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width); \
void ff_hevc_put_hevc_bi_w_ ## name ## _ ## D ## _sse(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride, int16_t *src2, ptrdiff_t src2stride, int width, int height, intptr_t mx, intptr_t my, int denom, int wx0, int wx1, int ox0, int ox1)

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
///////////////////////////////////////////////////////////////////////////////
// QPEL_PIXELS EPEL_PIXELS
///////////////////////////////////////////////////////////////////////////////
PEL_PROTOTYPE(pel_pixels2 ,  8);
PEL_PROTOTYPE(pel_pixels4 ,  8);
PEL_PROTOTYPE(pel_pixels6 ,  8);
PEL_PROTOTYPE(pel_pixels8 ,  8);
PEL_PROTOTYPE(pel_pixels12,  8);
PEL_PROTOTYPE(pel_pixels16,  8);
PEL_PROTOTYPE(pel_pixels24,  8);
PEL_PROTOTYPE(pel_pixels32,  8);
PEL_PROTOTYPE(pel_pixels48,  8);
PEL_PROTOTYPE(pel_pixels64,  8);

PEL_PROTOTYPE(pel_pixels2 , 10);
PEL_PROTOTYPE(pel_pixels4 , 10);
PEL_PROTOTYPE(pel_pixels6 , 10);
PEL_PROTOTYPE(pel_pixels8 , 10);
PEL_PROTOTYPE(pel_pixels12, 10);
PEL_PROTOTYPE(pel_pixels16, 10);
PEL_PROTOTYPE(pel_pixels24, 10);
PEL_PROTOTYPE(pel_pixels32, 10);
PEL_PROTOTYPE(pel_pixels48, 10);
PEL_PROTOTYPE(pel_pixels64, 10);

///////////////////////////////////////////////////////////////////////////////
// EPEL
///////////////////////////////////////////////////////////////////////////////
PEL_PROTOTYPE(epel_h4 ,  8);
PEL_PROTOTYPE(epel_h8 ,  8);
PEL_PROTOTYPE(epel_h12,  8);
PEL_PROTOTYPE(epel_h16,  8);
PEL_PROTOTYPE(epel_h24,  8);
PEL_PROTOTYPE(epel_h32,  8);
PEL_PROTOTYPE(epel_h48,  8);
PEL_PROTOTYPE(epel_h64,  8);

PEL_PROTOTYPE(epel_h4 , 10);
PEL_PROTOTYPE(epel_h8 , 10);
PEL_PROTOTYPE(epel_h12, 10);
PEL_PROTOTYPE(epel_h16, 10);
PEL_PROTOTYPE(epel_h24, 10);
PEL_PROTOTYPE(epel_h32, 10);
PEL_PROTOTYPE(epel_h48, 10);
PEL_PROTOTYPE(epel_h64, 10);

PEL_PROTOTYPE(epel_v4 ,  8);
PEL_PROTOTYPE(epel_v8 ,  8);
PEL_PROTOTYPE(epel_v12,  8);
PEL_PROTOTYPE(epel_v16,  8);
PEL_PROTOTYPE(epel_v24,  8);
PEL_PROTOTYPE(epel_v32,  8);
PEL_PROTOTYPE(epel_v48,  8);
PEL_PROTOTYPE(epel_v64,  8);

PEL_PROTOTYPE(epel_v4 , 10);
PEL_PROTOTYPE(epel_v8 , 10);
PEL_PROTOTYPE(epel_v12, 10);
PEL_PROTOTYPE(epel_v16, 10);
PEL_PROTOTYPE(epel_v24, 10);
PEL_PROTOTYPE(epel_v32, 10);
PEL_PROTOTYPE(epel_v48, 10);
PEL_PROTOTYPE(epel_v64, 10);

PEL_PROTOTYPE(epel_hv4 ,  8);
PEL_PROTOTYPE(epel_hv8 ,  8);
PEL_PROTOTYPE(epel_hv12,  8);
PEL_PROTOTYPE(epel_hv16,  8);
PEL_PROTOTYPE(epel_hv24,  8);
PEL_PROTOTYPE(epel_hv32,  8);
PEL_PROTOTYPE(epel_hv48,  8);
PEL_PROTOTYPE(epel_hv64,  8);

PEL_PROTOTYPE(epel_hv4 , 10);
PEL_PROTOTYPE(epel_hv8 , 10);
PEL_PROTOTYPE(epel_hv12, 10);
PEL_PROTOTYPE(epel_hv16, 10);
PEL_PROTOTYPE(epel_hv24, 10);
PEL_PROTOTYPE(epel_hv32, 10);
PEL_PROTOTYPE(epel_hv48, 10);
PEL_PROTOTYPE(epel_hv64, 10);
///////////////////////////////////////////////////////////////////////////////
// QPEL
///////////////////////////////////////////////////////////////////////////////
PEL_PROTOTYPE(qpel_h4 ,  8);
PEL_PROTOTYPE(qpel_h8 ,  8);
PEL_PROTOTYPE(qpel_h12,  8);
PEL_PROTOTYPE(qpel_h16,  8);
PEL_PROTOTYPE(qpel_h24,  8);
PEL_PROTOTYPE(qpel_h32,  8);
PEL_PROTOTYPE(qpel_h48,  8);
PEL_PROTOTYPE(qpel_h64,  8);

PEL_PROTOTYPE(qpel_h4 , 10);
PEL_PROTOTYPE(qpel_h8 , 10);
PEL_PROTOTYPE(qpel_h12, 10);
PEL_PROTOTYPE(qpel_h16, 10);
PEL_PROTOTYPE(qpel_h24, 10);
PEL_PROTOTYPE(qpel_h32, 10);
PEL_PROTOTYPE(qpel_h48, 10);
PEL_PROTOTYPE(qpel_h64, 10);

PEL_PROTOTYPE(qpel_v4 ,  8);
PEL_PROTOTYPE(qpel_v8 ,  8);
PEL_PROTOTYPE(qpel_v12,  8);
PEL_PROTOTYPE(qpel_v16,  8);
PEL_PROTOTYPE(qpel_v24,  8);
PEL_PROTOTYPE(qpel_v32,  8);
PEL_PROTOTYPE(qpel_v48,  8);
PEL_PROTOTYPE(qpel_v64,  8);

PEL_PROTOTYPE(qpel_v4 , 10);
PEL_PROTOTYPE(qpel_v8 , 10);
PEL_PROTOTYPE(qpel_v12, 10);
PEL_PROTOTYPE(qpel_v16, 10);
PEL_PROTOTYPE(qpel_v24, 10);
PEL_PROTOTYPE(qpel_v32, 10);
PEL_PROTOTYPE(qpel_v48, 10);
PEL_PROTOTYPE(qpel_v64, 10);

PEL_PROTOTYPE_SSE(qpel_hv4 ,  8);
PEL_PROTOTYPE_SSE(qpel_hv8 ,  8);
PEL_PROTOTYPE_SSE(qpel_hv12,  8);
PEL_PROTOTYPE_SSE(qpel_hv16,  8);
PEL_PROTOTYPE_SSE(qpel_hv24,  8);
PEL_PROTOTYPE_SSE(qpel_hv32,  8);
PEL_PROTOTYPE_SSE(qpel_hv48,  8);
PEL_PROTOTYPE_SSE(qpel_hv64,  8);

PEL_PROTOTYPE_SSE(qpel_hv4 , 10);
PEL_PROTOTYPE_SSE(qpel_hv8 , 10);
PEL_PROTOTYPE_SSE(qpel_hv12, 10);
PEL_PROTOTYPE_SSE(qpel_hv16, 10);
PEL_PROTOTYPE_SSE(qpel_hv24, 10);
PEL_PROTOTYPE_SSE(qpel_hv32, 10);
PEL_PROTOTYPE_SSE(qpel_hv48, 10);
PEL_PROTOTYPE_SSE(qpel_hv64, 10);


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
void ff_upsample_base_layer_frame_sse(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
void ff_upsample_base_layer_frame_sse_v(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
void ff_upsample_base_layer_frame_sse_h(struct AVFrame *FrameEL, struct AVFrame *FrameBL, short *Buffer[3], const int32_t enabled_up_sample_filter_luma[16][8], const int32_t enabled_up_sample_filter_chroma[16][4], struct HEVCWindow *Enhscal, struct UpsamplInf *up_info, int channel);
//#endif




#endif // AVCODEC_X86_HEVCDSP_H
