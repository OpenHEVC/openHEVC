#ifndef AVCODEC_X86_HEVCDSP_H
#define AVCODEC_X86_HEVCDSP_H

struct SAOParams;
struct AVFrame;
struct UpsamplInf;
struct HEVCWindow;

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

void ff_hevc_put_hevc_epel_pixels2_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_pixels4_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_pixels8_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_pixels16_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);

void ff_hevc_put_hevc_epel_pixels2_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_pixels4_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_pixels8_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);

void ff_hevc_put_hevc_epel_h2_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_h4_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_h2_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_h4_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_h8_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);

void ff_hevc_put_hevc_epel_v2_10_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_v4_10_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_v8_10_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_v2_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_v4_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_v8_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_v16_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);

void ff_hevc_put_hevc_epel_hv2_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_hv4_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_hv8_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_hv2_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_hv4_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);


void ff_put_hevc_mc_pixels_master_8_sse4 (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_pixels4_8_sse (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_pixels8_8_sse (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_pixels16_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);

void ff_hevc_put_hevc_qpel_h4_1_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_2_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_3_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_1_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_2_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_3_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);

void ff_hevc_put_hevc_qpel_v4_1_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v4_2_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v4_3_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v8_1_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v8_2_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v8_3_8_sse  (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v16_1_8_sse (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v16_2_8_sse (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v16_3_8_sse (int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);

void ff_hevc_put_hevc_qpel_h4_1_v_1_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_1_v_2_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_1_v_3_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_1_v_1_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_1_v_2_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_1_v_3_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_2_v_1_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_2_v_2_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_2_v_3_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_2_v_1_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_2_v_2_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_2_v_3_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_3_v_1_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_3_v_2_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h4_3_v_3_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_3_v_1_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_3_v_2_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h8_3_v_3_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, int width, int height, int16_t* mcbuffer);

void ff_hevc_put_hevc_qpel_pixels4_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_pixels8_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h2_1_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
//void ff_hevc_put_hevc_qpel_h2_2_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
//void ff_hevc_put_hevc_qpel_h2_3_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
//void ff_hevc_put_hevc_qpel_v4_1_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
//void ff_hevc_put_hevc_qpel_v4_2_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
//void ff_hevc_put_hevc_qpel_v4_3_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);

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
