#ifndef AVCODEC_X86_HEVCDSP_H
#define AVCODEC_X86_HEVCDSP_H

void ff_hevc_dequant4x4_8_sse4(int16_t *coeffs, int qp);
void ff_hevc_dequant4x4_10_sse4(int16_t *coeffs, int qp);

void ff_hevc_dequant8x8_8_sse4(int16_t *coeffs, int qp);
void ff_hevc_dequant16x16_8_sse4(int16_t *coeffs, int qp);
void ff_hevc_dequant32x32_8_sse4(int16_t *coeffs, int qp);

void ff_hevc_dequant8x8_10_sse4(int16_t *coeffs, int qp);
void ff_hevc_dequant16x16_10_sse4(int16_t *coeffs, int qp);
void ff_hevc_dequant32x32_10_sse4(int16_t *coeffs, int qp);

//IDCT functions
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
void ff_hevc_put_unweighted_pred_sse(uint8_t *_dst, ptrdiff_t _dststride,int16_t *src, ptrdiff_t srcstride,int width, int height);
void ff_hevc_put_unweighted_pred_8_sse(uint8_t *_dst, ptrdiff_t _dststride,int16_t *src, ptrdiff_t srcstride,int width, int height);

void ff_hevc_weighted_pred_8_sse(uint8_t denom, int16_t wlxFlag, int16_t olxFlag,uint8_t *_dst, ptrdiff_t _dststride,int16_t *src, ptrdiff_t srcstride,int width, int height);
void ff_hevc_weighted_pred_sse(uint8_t denom, int16_t wlxFlag, int16_t olxFlag,uint8_t *_dst, ptrdiff_t _dststride,int16_t *src, ptrdiff_t srcstride,int width, int height);
void ff_hevc_put_weighted_pred_avg_sse(uint8_t *_dst, ptrdiff_t _dststride,int16_t *src1, int16_t *src2, ptrdiff_t srcstride,int width, int height);
void ff_hevc_put_weighted_pred_avg_8_sse(uint8_t *_dst, ptrdiff_t _dststride,int16_t *src1, int16_t *src2, ptrdiff_t srcstride,int width, int height);

void ff_hevc_weighted_pred_avg_sse(uint8_t denom, int16_t wl0Flag, int16_t wl1Flag,int16_t ol0Flag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride,int16_t *src1, int16_t *src2, ptrdiff_t srcstride,int width, int height);
void ff_hevc_weighted_pred_avg_8_sse(uint8_t denom, int16_t wl0Flag, int16_t wl1Flag,int16_t ol0Flag, int16_t ol1Flag, uint8_t *_dst, ptrdiff_t _dststride,int16_t *src1, int16_t *src2, ptrdiff_t srcstride,int width, int height);


void ff_hevc_put_hevc_epel_pixels_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_pixels_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);


void ff_hevc_put_hevc_epel_h_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_h_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);

void ff_hevc_put_hevc_epel_v_10_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_v_8_sse(int16_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);

void ff_hevc_put_hevc_epel_hv_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);
void ff_hevc_put_hevc_epel_hv_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int mx, int my, int16_t* mcbuffer);


void ff_hevc_put_hevc_qpel_pixels_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_1_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_2_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_3_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v_1_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v_2_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v_3_8_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);

void ff_hevc_put_hevc_qpel_h_1_v_1_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_1_v_2_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_1_v_3_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_2_v_1_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_2_v_2_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_2_v_3_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_3_v_1_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_3_v_2_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_3_v_3_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);

void ff_hevc_put_hevc_qpel_pixels_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_h_1_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v_1_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v_2_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);
void ff_hevc_put_hevc_qpel_v_3_10_sse(int16_t *dst, ptrdiff_t dststride,uint8_t *_src, ptrdiff_t _srcstride,int width, int height, int16_t* mcbuffer);



// SAO functions

void ff_hevc_sao_edge_filter_0_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);
void ff_hevc_sao_edge_filter_1_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);
void ff_hevc_sao_edge_filter_2_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);
void ff_hevc_sao_edge_filter_3_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao,int *borders, int _width, int _height, int c_idx, uint8_t vert_edge, uint8_t horiz_edge, uint8_t diag_edge);

void ff_hevc_sao_band_filter_0_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);
void ff_hevc_sao_band_filter_1_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);
void ff_hevc_sao_band_filter_2_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);
void ff_hevc_sao_band_filter_3_8_sse(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride, struct SAOParams *sao, int *borders, int width, int height, int c_idx);

#endif // AVCODEC_X86_HEVCDSP_H
