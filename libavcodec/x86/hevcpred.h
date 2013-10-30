#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/get_bits.h"
#include "libavcodec/hevcdata.h"
#include "libavcodec/hevc.h"


#ifndef AVCODEC_X86_HEVCPRED_H
#define AVCODEC_X86_HEVCPRED_H

void ff_hevc_intra_pred_8_sse(HEVCContext *s, int x0, int y0, int log2_size, int c_idx);

void pred_planar_0_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride);
void pred_planar_1_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride);
void pred_planar_2_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride);
void pred_planar_3_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride);

void pred_angular_0_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride, int c_idx, int mode);
void pred_angular_1_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride, int c_idx, int mode);
void pred_angular_2_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride, int c_idx, int mode);
void pred_angular_3_8_sse(uint8_t *_src, const uint8_t *_top, const uint8_t *_left, ptrdiff_t stride, int c_idx, int mode);

#endif // AVCODEC_X86_HEVCPRED_H
