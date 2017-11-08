#include "config.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/hevc.h"
#include "libavcodec/arm/hevcdsp_arm.h"

#if HAVE_NEON
#include <arm_neon.h>

DECLARE_ALIGNED( 16, static const int16_t, transform4x4[2][8] ) = {
    { 64,  64,  64,  64, 64,  64,  64,  64 },
    { 83,  83,  83,  83, 36,  36,  36,  36 }
};

DECLARE_ALIGNED(16, static const int16_t, transform8x8[8][1][8] )=
{
	{{  89,  89,  89,  89, 75,  75, 75,  75 }},
	{{  50,  50,  50,  50, 18,  18, 18,  18 }},
	{{  75,  75,  75,  75,-18, -18,-18, -18 }},
	{{ -89, -89, -89, -89,-50, -50,-50, -50 }},
    {{  50,  50,  50,  50,-89, -89,-89, -89 }},
    {{  18,  18,  18,  18, 75,  75, 75,  75 }},
    {{  18,  18,  18,  18,-50, -50,-50, -50 }},
    {{  75,  75,  75,  75,-89, -89,-89, -89 }}
};

DECLARE_ALIGNED(16, static const int16_t, transform16x16[4][8][8] )=
{
    {/*1-3*/ /*2-6*/
        { 90,  90,  90,  90,  87,  87,  87,  87 },
        { 87,  87,  87,  87,  57,  57,  57,  57 },
        { 80,  80,  80,  80,   9,   9,   9,   9 },
        { 70,  70,  70,  70, -43, -43, -43, -43 },
        { 57,  57,  57,  57, -80, -80, -80, -80 },
        { 43,  43,  43,  43, -90, -90, -90, -90 },
        { 25,  25,  25,  25, -70, -70, -70, -70 },
        { 9,    9,   9,   9, -25, -25, -25, -25 },
    },{ /*5-7*/ /*10-14*/
        {  80,  80,  80,  80,  70,  70,  70,  70 },
        {   9,   9,   9,   9, -43, -43, -43, -43 },
        { -70, -70, -70, -70, -87, -87, -87, -87 },
        { -87, -87, -87, -87,   9,   9,   9,   9 },
        { -25, -25, -25, -25,  90,  90,  90,  90 },
        {  57,  57,  57,  57,  25,  25,  25,  25 },
        {  90,  90,  90,  90, -80, -80, -80, -80 },
        {  43,  43,  43,  43, -57, -57, -57, -57 },
    },{ /*9-11*/ /*18-22*/
        {  57,  57,  57,  57,  43,  43,  43,  43 },
        { -80, -80, -80, -80, -90, -90, -90, -90 },
        { -25, -25, -25, -25,  57,  57,  57,  57 },
        {  90,  90,  90,  90,  25,  25,  25,  25 },
        {  -9,  -9,  -9,  -9, -87, -87, -87, -87 },
        { -87, -87, -87, -87,  70,  70,  70,  70 },
        {  43,  43,  43,  43,   9,   9,   9,   9 },
        {  70,  70,  70,  70, -80, -80, -80, -80 },
    },{/*13-15*/ /*  26-30   */
        {  25,  25,  25,  25,   9,   9,   9,   9 },
        { -70, -70, -70, -70, -25, -25, -25, -25 },
        {  90,  90,  90,  90,  43,  43,  43,  43 },
        { -80, -80, -80, -80, -57, -57, -57, -57 },
        {  43,  43,  43,  43,  70,  70,  70,  70 },
        {  9,    9,   9,   9, -80, -80, -80, -80 },
        { -57, -57, -57, -57,  87,  87,  87,  87 },
        {  87,  87,  87,  87, -90, -90, -90, -90 },
    }
};

DECLARE_ALIGNED(16, static const int16_t, transform32x32[8][16][8] )=
{
    { /*   1-3     */
        {  90,  90,  90,  90,  90,  90,  90,  90 },
        {  90,  90,  90,  90,  82,  82,  82,  82 },
        {  88,  88,  88,  88,  67,  67,  67,  67 },
        {  85,  85,  85,  85,  46,  46,  46,  46 },
        {  82,  82,  82,  82,  22,  22,  22,  22 },
        {  78,  78,  78,  78,  -4,  -4,  -4,  -4 },
        {  73,  73,  73,  73, -31, -31, -31, -31 },
        {  67,  67,  67,  67, -54, -54, -54, -54 },
        {  61,  61,  61,  61, -73, -73, -73, -73 },
        {  54,  54,  54,  54, -85, -85, -85, -85 },
        {  46,  46,  46,  46, -90, -90, -90, -90 },
        {  38,  38,  38,  38, -88, -88, -88, -88 },
        {  31,  31,  31,  31, -78, -78, -78, -78 },
        {  22,  22,  22,  22, -61, -61, -61, -61 },
        {  13,  13,  13,  13, -38, -38, -38, -38 },
        {  4,   4,   4,   4,  -13, -13, -13, -13 },
    },{/*  5-7 */
        {  88,  88,  88,  88,  85,  85,  85,  85 },
        {  67,  67,  67,  67,  46,  46,  46,  46 },
        {  31,  31,  31,  31, -13, -13, -13, -13 },
        { -13, -13, -13, -13, -67, -67, -67, -67 },
        { -54, -54, -54, -54, -90, -90, -90, -90 },
        { -82, -82, -82, -82, -73, -73, -73, -73 },
        { -90, -90, -90, -90, -22, -22, -22, -22 },
        { -78, -78, -78, -78,  38,  38,  38,  38 },
        { -46, -46, -46, -46,  82,  82,  82,  82 },
        {  -4,  -4,  -4,  -4,  88,  88,  88,  88 },
        {  38,  38,  38,  38,  54,  54,  54,  54 },
        {  73,  73,  73,  73,  -4,  -4,  -4,  -4 },
        {  90,  90,  90,  90, -61, -61, -61, -61 },
        {  85,  85,  85,  85, -90, -90, -90, -90 },
        {  61,  61,  61,  61, -78, -78, -78, -78 },
        {  22,  22,  22,  22, -31, -31, -31, -31 },
    },{/*  9-11   */
        {  82,  82,  82,  82,  78,  78,  78,  78 },
        {  22,  22,  22,  22,  -4,  -4,  -4,  -4 },
        { -54, -54, -54, -54, -82, -82, -82, -82 },
        { -90, -90, -90, -90, -73, -73, -73, -73 },
        { -61, -61, -61, -61,  13,  13,  13,  13 },
        {  13,  13,  13,  13,  85,  85,  85,  85 },
        {  78,  78,  78,  78,  67,  67,  67,  67 },
        {  85,  85,  85,  85, -22, -22, -22, -22 },
        {  31,  31,  31,  31, -88, -88, -88, -88 },
        { -46, -46, -46, -46, -61, -61, -61, -61 },
        { -90, -90, -90, -90,  31,  31,  31,  31 },
        { -67, -67, -67, -67,  90,  90,  90,  90 },
        {   4,   4,   4,   4,  54,  54,  54,  54 },
        {  73,  73,  73,  73, -38, -38, -38, -38 },
        {  88,  88,  88,  88, -90, -90, -90, -90 },
        {  38,  38,  38,  38, -46, -46, -46, -46 },
    },{/*  13-15   */
        {  73,  73,  73,  73,  67,  67,  67,  67 },
        { -31, -31, -31, -31, -54, -54, -54, -54 },
        { -90, -90, -90, -90, -78, -78, -78, -78 },
        { -22, -22, -22, -22,  38,  38,  38,  38 },
        {  78,  78,  78,  78,  85,  85,  85,  85 },
        {  67,  67,  67,  67, -22, -22, -22, -22 },
        { -38, -38, -38, -38, -90, -90, -90, -90 },
        { -90, -90, -90, -90,   4,   4,   4,   4 },
        { -13, -13, -13, -13,  90,  90,  90,  90 },
        {  82,  82,  82,  82,  13,  13,  13,  13 },
        {  61,  61,  61,  61, -88, -88, -88, -88 },
        { -46, -46, -46, -46, -31, -31, -31, -31 },
        { -88, -88, -88, -88,  82,  82,  82,  82 },
        { -4,  -4,  -4,  -4,   46,  46,  46,  46 },
        {  85,  85,  85,  85, -73, -73, -73, -73 },
        {  54,  54,  54,  54, -61, -61, -61, -61 },
    },{/*  17-19   */
        {  61,  61,  61,  61,  54,  54,  54,  54 },
        { -73, -73, -73, -73, -85, -85, -85, -85 },
        { -46, -46, -46, -46,  -4,  -4,  -4,  -4 },
        {  82,  82,  82,  82,  88,  88,  88,  88 },
        {  31,  31,  31,  31, -46, -46, -46, -46 },
        { -88, -88, -88, -88, -61, -61, -61, -61 },
        { -13, -13, -13, -13,  82,  82,  82,  82 },
        {  90,  90,  90,  90,  13,  13,  13,  13 },
        {  -4,  -4,  -4,  -4, -90, -90, -90, -90 },
        { -90, -90, -90, -90,  38,  38,  38,  38 },
        {  22,  22,  22,  22,  67,  67,  67,  67 },
        {  85,  85,  85,  85, -78, -78, -78, -78 },
        { -38, -38, -38, -38, -22, -22, -22, -22 },
        { -78, -78, -78, -78,  90,  90,  90,  90 },
        {  54,  54,  54,  54, -31, -31, -31, -31 },
        {  67,  67,  67,  67, -73, -73, -73, -73 },
    },{ /*  21-23   */
        {  46,  46,  46,  46,  38,  38,  38,  38 },
        { -90, -90, -90, -90, -88, -88, -88, -88 },
        {  38,  38,  38,  38,  73,  73,  73,  73 },
        {  54,  54,  54,  54,  -4,  -4,  -4,  -4 },
        { -90, -90, -90, -90, -67, -67, -67, -67 },
        {  31,  31,  31,  31,  90,  90,  90,  90 },
        {  61,  61,  61,  61, -46, -46, -46, -46 },
        { -88, -88, -88, -88, -31, -31, -31, -31 },
        {  22,  22,  22,  22,  85,  85,  85,  85 },
        {  67,  67,  67,  67, -78, -78, -78, -78 },
        { -85, -85, -85, -85,  13,  13,  13,  13 },
        {  13,  13,  13,  13,  61,  61,  61,  61 },
        {  73,  73,  73,  73, -90, -90, -90, -90 },
        { -82, -82, -82, -82,  54,  54,  54,  54 },
        {   4,   4,   4,   4,  22,  22,  22,  22 },
        {  78,  78,  78,  78, -82, -82, -82, -82 },
    },{ /*  25-27   */
        {  31,  31,  31,  31,  22,  22,  22,  22 },
        { -78, -78, -78, -78, -61, -61, -61, -61 },
        {  90,  90,  90,  90,  85,  85,  85,  85 },
        { -61, -61, -61, -61, -90, -90, -90, -90 },
        {   4,   4,   4,   4,  73,  73,  73,  73 },
        {  54,  54,  54,  54, -38, -38, -38, -38 },
        { -88, -88, -88, -88,  -4,  -4,  -4,  -4 },
        {  82,  82,  82,  82,  46,  46,  46,  46 },
        { -38, -38, -38, -38, -78, -78, -78, -78 },
        { -22, -22, -22, -22,  90,  90,  90,  90 },
        {  73,  73,  73,  73, -82, -82, -82, -82 },
        { -90, -90, -90, -90,  54,  54,  54,  54 },
        {  67,  67,  67,  67, -13, -13, -13, -13 },
        { -13, -13, -13, -13, -31, -31, -31, -31 },
        { -46, -46, -46, -46,  67,  67,  67,  67 },
        {  85,  85,  85,  85, -88, -88, -88, -88 },
    },{/*  29-31   */
        {  13,  13,  13,  13,   4,   4,   4,   4 },
        { -38, -38, -38, -38, -13, -13, -13, -13 },
        {  61,  61,  61,  61,  22,  22,  22,  22 },
        { -78, -78, -78, -78, -31, -31, -31, -31 },
        {  88,  88,  88,  88,  38,  38,  38,  38 },
        { -90, -90, -90, -90, -46, -46, -46, -46 },
        {  85,  85,  85,  85,  54,  54,  54,  54 },
        { -73, -73, -73, -73, -61, -61, -61, -61 },
        {  54,  54,  54,  54,  67,  67,  67,  67 },
        { -31, -31, -31, -31, -73, -73, -73, -73 },
        {   4,   4,   4,   4,  78,  78,  78,  78 },
        {  22,  22,  22,  22, -82, -82, -82, -82 },
        { -46, -46, -46, -46,  85,  85,  85,  85 },
        {  67,  67,  67,  67, -88, -88, -88, -88 },
        { -82, -82, -82, -82,  90,  90,  90,  90 },
        {  90,  90,  90,  90, -90, -90, -90, -90 },
    }
};

#define shift_1st 7
#define add_1st (1 << (shift_1st - 1))

#define CLIP_PIXEL_MAX_10 0x03FF
#define CLIP_PIXEL_MAX_12 0x0FFF

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define INIT_8()                                                               \
    uint8_t *dst = (uint8_t*) _dst;                                            \
    ptrdiff_t stride = _stride
#define INIT_10()                                                              \
    uint16_t *dst = (uint16_t*) _dst;                                          \
    ptrdiff_t stride = _stride>>1

#define INIT_12() INIT_10()
#define INIT8_12() INIT8_10()

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define LOAD_EMPTY(dst, src)
#define LOAD4x4(dst, src)         	\
	dst ## 0 = vld1q_s16(&src[0]);	\
	dst ## 1 = vld1q_s16(&src[8])
#define LOAD4x4_STEP(dst, src, sstep)                                          \
	tmp0 = vld1q_s16(&src[0 * sstep]);                       				   \
	tmp1 = vld1q_s16(&src[1 * sstep]);                       				   \
	tmp2 = vld1q_s16(&src[2 * sstep]);                       				   \
	tmp3 = vld1q_s16(&src[3 * sstep])
#define LOAD8x8_E(dst, src, sstep)                                             \
	dst ## 0 = vld1q_s16(&src[0 * sstep]);                       				   \
	dst ## 1 = vld1q_s16(&src[1 * sstep]);                       				   \
	dst ## 2 = vld1q_s16(&src[2 * sstep]);                       				   \
	dst ## 3 = vld1q_s16(&src[3 * sstep])
#define LOAD8x8_O(dst, src, sstep)                                             \
	dst ## 0 = vld1q_s16(&src[1 * sstep]);                       				   \
	dst ## 1 = vld1q_s16(&src[3 * sstep]);                       				   \
	dst ## 2 = vld1q_s16(&src[5 * sstep]);                       				   \
	dst ## 3 = vld1q_s16(&src[7 * sstep])
#define LOAD16x16_O(dst, src, sstep)                                           \
    LOAD8x8_O(dst, src, sstep);                                                \
    dst ## 4 = vld1q_s16(&src[ 9 * sstep]);                       				   \
    dst ## 5 = vld1q_s16(&src[11 * sstep]);                       				   \
    dst ## 6 = vld1q_s16(&src[13 * sstep]);                       				   \
    dst ## 7 = vld1q_s16(&src[15 * sstep])

#define LOAD_8x32(dst, dst_stride, src0, src1, idx)                            \
	src0 ## _32 = vld1q_s32(&dst[idx*dst_stride]);                       				   \
	src1 ## _32 = vld1q_s32(&dst[idx*dst_stride+4])

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define ASSIGN_EMPTY(dst, dst_stride, src)
#define SAVE_8x16(dst, dst_stride, src)                                        \
	vst1q_s16(dst, src);                                    					\
    dst += dst_stride
#define SAVE_8x32(dst, dst_stride, src0, src1, idx)                            \
	vst1q_s32(&dst[idx*dst_stride], src0 ## _32);                                    					\
	vst1q_s32(&dst[idx*dst_stride+4], src1 ## _32)

#define ASSIGN2(dst, dst_stride, src0, src1, assign)                           \
    assign(dst, dst_stride, src0);                                             \
    assign(dst, dst_stride, _mm_srli_si128(src0, 8));                          \
    assign(dst, dst_stride, src1);                                             \
    assign(dst, dst_stride, _mm_srli_si128(src1, 8))
#define ASSIGN4(dst, dst_stride, src0, src1, src2, src3, assign)               \
    assign(dst, dst_stride, src0);                                             \
    assign(dst, dst_stride, src1);                                             \
    assign(dst, dst_stride, src2);                                             \
    assign(dst, dst_stride, src3)
#define ASSIGN4_LO(dst, dst_stride, src, assign)                               \
    ASSIGN4(dst, dst_stride, src ## 0, src ## 1, src ## 2, src ## 3, assign)
#define ASSIGN4_HI(dst, dst_stride, src, assign)                               \
    ASSIGN4(dst, dst_stride, src ## 4, src ## 5, src ## 6, src ## 7, assign)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define TRANSPOSE4X4_16(dst)                                                   	\
		tmpVzip = vzipq_s16(dst ## 0, dst ## 1);							   	\
		dst ## 0 = tmpVzip.val[0];											   	\
		dst ## 1 = tmpVzip.val[1];											   	\
		tmpVzip = vzipq_s16(dst ## 0, dst ## 1);							 	\
		dst ## 0 = tmpVzip.val[0];												\
		dst ## 1 = tmpVzip.val[1];
#define TRANSPOSE4X4_16_S(dst, dst_stride, src, assign)                        \
    TRANSPOSE4X4_16(src);                                                      \
    ASSIGN2(dst, dst_stride, src ## 0, src ## 1, assign)

#define TRANS_64(eX, eY)											\
		tmpTrn64 = vget_high_s16(eX);								\
		eX = vcombine_s16(vget_low_s16(eX), vget_low_s16(eY));	\
		eY = vcombine_s16(tmpTrn64, vget_high_s16 (eY))

#define TRANS_32(eX, eY) 			\
		tmpTrn32 = vtrnq_s32(vreinterpretq_s32_s16(eX), vreinterpretq_s32_s16(eY));	\
		eX = vreinterpretq_s16_s32(tmpTrn32.val[0]);			\
		eY = vreinterpretq_s16_s32(tmpTrn32.val[1])
#define TRANS_16(eX, eY) 			\
		tmpTrn16 = vtrnq_s16(eX, eY);	\
		eX = tmpTrn16.val[0];			\
		eY = tmpTrn16.val[1]

#define TRANSPOSE8X8_16(dst)     	\
	TRANS_64(dst ## 0, dst ## 4);	\
	TRANS_64(dst ## 1, dst ## 5);	\
	TRANS_64(dst ## 2, dst ## 6);	\
	TRANS_64(dst ## 3, dst ## 7);	\
	TRANS_32(dst ## 0, dst ## 2);	\
	TRANS_32(dst ## 1, dst ## 3);	\
	TRANS_32(dst ## 4, dst ## 6);	\
	TRANS_32(dst ## 5, dst ## 7);	\
	TRANS_16(dst ## 0, dst ## 1);	\
	TRANS_16(dst ## 2, dst ## 3);	\
	TRANS_16(dst ## 4, dst ## 5);	\
	TRANS_16(dst ## 6, dst ## 7)

#define TRANSPOSE8x8_16_S(out, sstep_out, src, assign)                         \
    TRANSPOSE8X8_16(src);                                                      \
    p_dst = out;                                                               \
    ASSIGN4_LO(p_dst, sstep_out, src, assign);                                 \
    ASSIGN4_HI(p_dst, sstep_out, src, assign)
#define TRANSPOSE8x8_16_LS(out, sstep_out, in, sstep_in, assign)               \
    e0  = vld1q_s16(&in[0*sstep_in]);                         \
    e1  = vld1q_s16(&in[1*sstep_in]);                         \
    e2  = vld1q_s16(&in[2*sstep_in]);                         \
    e3  = vld1q_s16(&in[3*sstep_in]);                         \
    e4  = vld1q_s16(&in[4*sstep_in]);                         \
    e5  = vld1q_s16(&in[5*sstep_in]);                         \
    e6  = vld1q_s16(&in[6*sstep_in]);                         \
    e7  = vld1q_s16(&in[7*sstep_in]);                         \
    TRANSPOSE8x8_16_S(out, sstep_out, e, assign)

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
#define TR_COMPUTE_TRANFORM(dst1, dst2, src0, src1, src2, src3, i, j, transform)\
	tmp0 = vld1q_s16(transform[i  ][j]);\
	tmp1 = vld1q_s16(transform[i+1][j]);\
	tmp0_32 = vmull_s16(         vget_low_s16 (src0), vget_low_s16 (tmp0));		\
	tmp0_32 = vmlal_s16(tmp0_32, vget_low_s16 (src1), vget_high_s16(tmp0));		\
	tmp1_32 = vmull_s16(         vget_high_s16(src0), vget_low_s16 (tmp0));		\
	tmp1_32 = vmlal_s16(tmp1_32, vget_high_s16(src1), vget_high_s16(tmp0));		\
	tmp2_32 = vmull_s16(         vget_low_s16 (src2), vget_low_s16 (tmp1));		\
	tmp2_32 = vmlal_s16(tmp2_32, vget_low_s16 (src3), vget_high_s16(tmp1));		\
	tmp3_32 = vmull_s16(         vget_high_s16(src2), vget_low_s16 (tmp1));		\
	tmp3_32 = vmlal_s16(tmp3_32, vget_high_s16(src3), vget_high_s16(tmp1));		\
	dst1 ## _32 = vaddq_s32(tmp0_32, tmp2_32);								   \
	dst2 ## _32 = vaddq_s32(tmp1_32, tmp3_32)

#define SCALE8x8_2x32(dst0, src0, src1) 	\
	dst0 = vcombine_s16(					\
		vqrshrn_n_s32(src0 ## _32, shift),	\
		vqrshrn_n_s32(src1 ## _32, shift))
#define SCALE_4x32(dst0, dst1, src0, src1, src2, src3)                         \
    SCALE8x8_2x32(dst0, src0, src1);                                           \
    SCALE8x8_2x32(dst1, src2, src3)
#define SCALE16x16_2x32(dst, dst_stride, src0, src1, j)                        \
    e0_32   = vld1q_s32(&o16[j*8+0]);                           \
    e7_32   = vld1q_s32(&o16[j*8+4]);                           \
    tmp4_32 = vaddq_s32(src0 ## _32, e0_32);                                            \
    src0 ## _32 = vsubq_s32(src0 ## _32, e0_32);                                            \
    e0_32   = vaddq_s32(src1 ## _32, e7_32);                                            \
    src1 ## _32   = vsubq_s32(src1 ## _32, e7_32);                                            \
    SCALE_4x32(e0, e7, tmp4, e0, src0, src1);                                  \
    vst1q_s16(&dst[dst_stride*(             j)]  , e0);     \
    vst1q_s16(&dst[dst_stride*(dst_stride-1-j)]  , e7)

#define SCALE32x32_2x32(dst, dst_stride, j)                                    \
    e0_32   = vld1q_s32(&e32[j*16+0]);                          \
    e1_32   = vld1q_s32(&e32[j*16+4]);                          \
    e4_32   = vld1q_s32(&o32[j*16+0]);                          \
    e5_32   = vld1q_s32(&o32[j*16+4]);                          \
    tmp0_32 = vaddq_s32(e0_32, e4_32);                                              \
    tmp1_32 = vaddq_s32(e1_32, e5_32);                                              \
    tmp2_32 = vsubq_s32(e1_32, e5_32);                                              \
    tmp3_32 = vsubq_s32(e0_32, e4_32);                                              \
    SCALE_4x32(tmp0, tmp1, tmp0, tmp1, tmp3, tmp2);                            \
	vst1q_s16(&dst[dst_stride*i+0]  , tmp0);                \
	vst1q_s16(&dst[dst_stride*(dst_stride-1-i)+0]  , tmp1)

#define SAVE16x16_2x32(dst, dst_stride, src0, src1, j)                        \
	e0_32   = vld1q_s32(&o16[j*8+0]);                           \
	e7_32   = vld1q_s32(&o16[j*8+4]);                           \
    tmp4_32 = vaddq_s32(src0 ## _32, e0_32);                                            \
    src0 ## _32 = vsubq_s32(src0 ## _32, e0_32);                                            \
    e0_32   = vaddq_s32(src1 ## _32, e7_32);                                            \
    src1 ## _32 = vsubq_s32(src1 ## _32, e7_32);                                            \
    vst1q_s32(&dst[dst_stride*(             j)]  , tmp4_32);   \
    vst1q_s32(&dst[dst_stride*(             j)+4], e0_32);     \
    vst1q_s32(&dst[dst_stride*(dst_stride-1-j)]  , src0 ## _32);   \
    vst1q_s32(&dst[dst_stride*(dst_stride-1-j)+4], src1 ## _32)


#define SCALE8x8_2x32_WRAPPER(dst, dst_stride, dst0, src0, src1, idx)          \
    SCALE8x8_2x32(dst0, src0, src1)
#define SCALE16x16_2x32_WRAPPER(dst, dst_stride, dst0, src0, src1, idx)        \
    SCALE16x16_2x32(dst, dst_stride, src0, src1, idx)
#define SAVE16x16_2x32_WRAPPER(dst, dst_stride, dst0, src0, src1, idx)         \
    SAVE16x16_2x32(dst, dst_stride, src0, src1, idx)

////////////////////////////////////////////////////////////////////////////////
// oh_hevc_transform_4x4_X_neon
////////////////////////////////////////////////////////////////////////////////
#define COMPUTE4x4_LO2(src0, src1, src2, src3, dst0, dst1, dst2, dst3)                                     \
	tmp0 = vld1q_s16(transform4x4[0]);										   	\
	tmp1 = vld1q_s16(transform4x4[1]);										   	\
	tmp0_32 = vmull_s16(         vget_low_s16 (src0), vget_low_s16 (tmp0));		\
	tmp0_32 = vmlal_s16(tmp0_32, vget_low_s16 (src2), vget_high_s16(tmp0));		\
	tmp1_32 = vmull_s16(         vget_low_s16 (src0), vget_high_s16(tmp0));		\
	tmp1_32 = vmlsl_s16(tmp1_32, vget_low_s16 (src2), vget_low_s16 (tmp0));		\
	tmp2_32 = vmull_s16(         vget_low_s16 (src1), vget_low_s16 (tmp1));		\
	tmp2_32 = vmlal_s16(tmp2_32, vget_low_s16 (src3), vget_high_s16(tmp1));		\
	tmp3_32 = vmull_s16(         vget_low_s16 (src1), vget_high_s16(tmp1));		\
	tmp3_32 = vmlsl_s16(tmp3_32, vget_low_s16 (src3), vget_low_s16 (tmp1));		\
    dst0 ## _32 = vaddq_s32(tmp0_32, tmp2_32);                                 	\
    dst1 ## _32 = vaddq_s32(tmp1_32, tmp3_32);                                 	\
    dst2 ## _32 = vsubq_s32(tmp1_32, tmp3_32);                                 	\
    dst3 ## _32 = vsubq_s32(tmp0_32, tmp2_32)

#define COMPUTE4x4_HI2(src0, src1, src2, src3, dst0, dst1, dst2, dst3)                                     \
	tmp0 = vld1q_s16(transform4x4[0]);										   	\
	tmp1 = vld1q_s16(transform4x4[1]);										   	\
	tmp0_32 = vmull_s16(         vget_high_s16(src0), vget_low_s16 (tmp0));		\
	tmp0_32 = vmlal_s16(tmp0_32, vget_high_s16(src2), vget_high_s16(tmp0));		\
	tmp1_32 = vmull_s16(         vget_high_s16(src0), vget_high_s16(tmp0));		\
	tmp1_32 = vmlsl_s16(tmp1_32, vget_high_s16(src2), vget_low_s16 (tmp0));		\
	tmp2_32 = vmull_s16(         vget_high_s16(src1), vget_low_s16 (tmp1));		\
	tmp2_32 = vmlal_s16(tmp2_32, vget_high_s16(src3), vget_high_s16(tmp1));		\
	tmp3_32 = vmull_s16(         vget_high_s16(src1), vget_high_s16(tmp1));		\
	tmp3_32 = vmlsl_s16(tmp3_32, vget_high_s16(src3), vget_low_s16 (tmp1));		\
	dst0 ## _32 = vaddq_s32(tmp0_32, tmp2_32);                                 	\
	dst1 ## _32 = vaddq_s32(tmp1_32, tmp3_32);                                 	\
	dst2 ## _32 = vsubq_s32(tmp1_32, tmp3_32);                                 	\
	dst3 ## _32 = vsubq_s32(tmp0_32, tmp2_32)

#define COMPUTE4x4_HILO2(src0, src1, src2, src3, dst0, dst1, dst2, dst3)                                     \
	tmp0 = vld1q_s16(transform4x4[0]);										   	\
	tmp1 = vld1q_s16(transform4x4[1]);										   	\
	tmp0_32 = vmull_s16(         vget_low_s16 (src0), vget_low_s16 (tmp0));		\
	tmp0_32 = vmlal_s16(tmp0_32, vget_low_s16 (src2), vget_high_s16(tmp0));		\
	tmp1_32 = vmull_s16(         vget_low_s16 (src0), vget_high_s16(tmp0));		\
	tmp1_32 = vmlsl_s16(tmp1_32, vget_low_s16 (src2), vget_low_s16 (tmp0));		\
	tmp2_32 = vmull_s16(         vget_high_s16(src1), vget_low_s16 (tmp1));		\
	tmp2_32 = vmlal_s16(tmp2_32, vget_high_s16(src3), vget_high_s16(tmp1));		\
	tmp3_32 = vmull_s16(         vget_high_s16(src1), vget_high_s16(tmp1));		\
	tmp3_32 = vmlsl_s16(tmp3_32, vget_high_s16(src3), vget_low_s16 (tmp1));		\
	dst0 ## _32 = vaddq_s32(tmp0_32, tmp2_32);                                 	\
	dst1 ## _32 = vaddq_s32(tmp1_32, tmp3_32);                                 	\
	dst2 ## _32 = vsubq_s32(tmp1_32, tmp3_32);                                 	\
	dst3 ## _32 = vsubq_s32(tmp0_32, tmp2_32)

#define COMPUTE4x4(dst0, dst1, dst2, dst3)                                     \
	tmp0 = vld1q_s16(transform4x4[0]);										   \
	tmp1 = vld1q_s16(transform4x4[1]);										   \
	tmp2 = vld1q_s16(transform4x4[2]);										   \
	tmp3 = vld1q_s16(transform4x4[3]);										   \
    MADD_NEON(e6, tmp0, tmp0);                                                   \
    MADD_NEON(e6, tmp1, tmp1);                                                   \
    MADD_NEON(e7, tmp2, tmp2);                                                   \
    MADD_NEON(e7, tmp3, tmp3);                                                   \
    dst0 ## _32 = vaddq_s32(tmp0_32, tmp2_32);                                 \
    dst1 ## _32 = vaddq_s32(tmp1_32, tmp3_32);                                 \
    dst2 ## _32 = vsubq_s32(tmp1_32, tmp3_32);                                 \
    dst3 ## _32 = vsubq_s32(tmp0_32, tmp2_32)
#define COMPUTE4x4_LO()                                                        \
    COMPUTE4x4(e0, e1, e2, e3)
#define COMPUTE4x4_HI(dst)                                                     \
    COMPUTE4x4(e7, e6, e5, e4)

#define TR_4(dst, dst_stride, in, sstep, load, assign)                         \
    load(e, in);                                                               \
    COMPUTE4x4_HILO2(e0,e0,e1,e1,e0,e1,e2,e3);                                                           \
    SCALE_4x32(e0, e1, e0, e1, e2, e3);                                        \
    TRANSPOSE4X4_16_S(dst, dst_stride, e, assign)                              \

#define TR_4_1( dst, dst_stride, src)    TR_4( dst, dst_stride, src,  4, LOAD4x4, ASSIGN_EMPTY)
#define TR_4_2( dst, dst_stride, src, D) TR_4( dst, dst_stride, src,  4, LOAD_EMPTY, ASSIGN_EMPTY)

////////////////////////////////////////////////////////////////////////////////
// oh_hevc_transform_8x8_X_neon
////////////////////////////////////////////////////////////////////////////////
#define TR_4_set8x4(in, sstep)                                                 \
    LOAD8x8_E(src, in, sstep);                                                 \
    COMPUTE4x4_LO2(src0, src1, src2, src3, e0, e1, e2, e3);                                                           \
    COMPUTE4x4_HI2(src0, src1, src2, src3, e7, e6, e5, e4)

#define TR_COMPUTE8x8(e0, e1, i)                                               \
    TR_COMPUTE_TRANFORM(tmp2, tmp3, src0, src1, src2, src3, i, 0, transform8x8);\
    tmp0_32 = vaddq_s32(e0 ## _32, tmp2_32); \
    tmp1_32 = vaddq_s32(e1 ## _32, tmp3_32); \
    tmp3_32 = vsubq_s32(e1 ## _32, tmp3_32); \
    tmp2_32 = vsubq_s32(e0 ## _32, tmp2_32)

#define TR_8(dst, dst_stride, in, sstep, assign)                               \
    TR_4_set8x4(in, 2 * sstep);                                                \
    LOAD8x8_O(src, in, sstep);                                                 \
    TR_COMPUTE8x8(e0, e7, 0);                                                  \
    assign(dst, dst_stride, e0, tmp0, tmp1, 0);                                \
    assign(dst, dst_stride, e7, tmp2, tmp3, 7);                                \
    TR_COMPUTE8x8(e1, e6, 2);                                                  \
    assign(dst, dst_stride, e1, tmp0, tmp1, 1);                                \
    assign(dst, dst_stride, e6, tmp2, tmp3, 6);                                \
    TR_COMPUTE8x8(e2, e5, 4);                                                  \
    assign(dst, dst_stride, e2, tmp0, tmp1, 2);                                \
    assign(dst, dst_stride, e5, tmp2, tmp3, 5);                                \
    TR_COMPUTE8x8(e3, e4, 6);                                                  \
    assign(dst, dst_stride, e3, tmp0, tmp1, 3);                                \
    assign(dst, dst_stride, e4, tmp2, tmp3, 4);                                \

#define TR_8_1( dst, dst_stride, src)                                         \
    TR_8( dst, dst_stride, src,  8, SCALE8x8_2x32_WRAPPER);                    \
    TRANSPOSE8x8_16_S(dst, dst_stride, e, SAVE_8x16)

////////////////////////////////////////////////////////////////////////////////
// oh_hevc_transform_XxX_X_neon
////////////////////////////////////////////////////////////////////////////////

#define TRANSFORM_4x4(D)                                                       \
void oh_hevc_transform_4x4_ ## D ## _neon (int16_t *_coeffs, int col_limit) {  \
    int16_t *src = _coeffs;                                                    \
    int16_t *dst = _coeffs;                                                    \
    int      shift  = 7;                                                       \
    int16x8_t tmp0, tmp1;                                          \
    int32x4_t tmp0_32, tmp1_32, tmp2_32, tmp3_32;							   \
    int16x8_t e0, e1;                                      \
    int32x4_t e0_32, e1_32, e2_32, e3_32;									   \
    int16x8x2_t tmpVzip;													   \
    TR_4_1(p_dst1, 4, src);                                                    \
    shift   = 20 - D;                                                          \
    TR_4_2(dst, 8, tmp, D);                                                    \
    vst1q_s16(&dst[0], e0);													   \
    vst1q_s16(&dst[8], e1);													   \
}

#define TRANSFORM_8x8(D)                                                       \
void oh_hevc_transform_8x8_ ## D ## _neon (int16_t *coeffs, int col_limit) {   \
    int16_t tmp[8*8];                                                          \
    int16_t *src    = coeffs;                                                  \
    int16_t *dst    = coeffs;                                                  \
    int16_t *p_dst1 = tmp;                                                     \
    int16_t *p_dst;                                                            \
    int      shift  = 7;                                                       \
    int16x8_t src0, src1, src2, src3;                                            \
    int16x8_t tmp0, tmp1;                                          \
    int32x4_t tmp0_32, tmp1_32, tmp2_32, tmp3_32;							   \
    int16x8_t e0, e1, e2, e3, e4, e5, e6, e7;                              \
    int32x4_t e0_32, e1_32, e2_32, e3_32, e4_32, e5_32, e6_32, e7_32;		   \
	int16x4_t   tmpTrn64;			\
	int32x4x2_t tmpTrn32;			\
	int16x8x2_t tmpTrn16;			\
    TR_8_1(p_dst1, 8, src);                                                    \
    shift   = 20 - D;                                                          \
    TR_8_1(dst, 8, tmp);                                                       \
}

/* Use faster assembly version */
//TRANSFORM_4x4( 8)
//TRANSFORM_8x8( 8)

////////////////////////////////////////////////////////////////////////////////
// oh_hevc_transform_16x16_X_neon
////////////////////////////////////////////////////////////////////////////////
#define TR_COMPUTE16x16(dst1, dst2,src0, src1, src2, src3, i, j)              \
    TR_COMPUTE_TRANFORM(dst1, dst2,src0, src1, src2, src3, i, j, transform16x16)
#define TR_COMPUTE16x16_FIRST(j)                                               \
    TR_COMPUTE16x16(src0, src1, e0, e1, e2, e3, 0, j)
#define TR_COMPUTE16x16_NEXT(i, j)                                             \
    TR_COMPUTE16x16(tmp0, tmp1, e4, e5, e6, e7, i, j);                         \
    src0_32 = vaddq_s32(src0_32, tmp0_32);                                     \
    src1_32 = vaddq_s32(src1_32, tmp1_32)

#define TR_16(dst, dst_stride, in, sstep, assign)                              \
    {                                                                          \
        int i;                                                                 \
        int o16[8*8];                                                          \
        LOAD16x16_O(e, in, sstep);                                             \
        for (i = 0; i < 8; i++) {                                              \
            TR_COMPUTE16x16_FIRST(i);                                          \
            TR_COMPUTE16x16_NEXT(2, i);                                        \
            SAVE_8x32(o16, 8, src0, src1, i);                                  \
        }                                                                      \
        TR_8(dst, dst_stride, in, 2 * sstep, assign);                          \
    }

#define TR_16_1( dst, dst_stride, src)        TR_16( dst, dst_stride, src,     16, SCALE16x16_2x32_WRAPPER)
#define TR_16_2( dst, dst_stride, src, sstep) TR_16( dst, dst_stride, src,  sstep, SAVE16x16_2x32_WRAPPER )

////////////////////////////////////////////////////////////////////////////////
// oh_hevc_transform_32x32_X_neon
////////////////////////////////////////////////////////////////////////////////
#define TR_COMPUTE32x32(dst1, dst2,src0, src1, src2, src3, i, j)              \
    TR_COMPUTE_TRANFORM(dst1, dst2, src0, src1, src2, src3, i, j, transform32x32)
#define TR_COMPUTE32x32_FIRST(i, j)                                            \
    TR_COMPUTE32x32(tmp0, tmp1, e0, e1, e2, e3, i, j);                         \
    src0_32 = vaddq_s32(src0_32, tmp0 ## _32);                                          \
    src1_32 = vaddq_s32(src1_32, tmp1 ## _32)
#define TR_COMPUTE32x32_NEXT(i, j)                                             \
    TR_COMPUTE32x32(tmp0, tmp1, e4, e5, e6, e7, i, j);                         \
    src0_32 = vaddq_s32(src0_32, tmp0 ## _32);                                          \
    src1_32 = vaddq_s32(src1_32, tmp1 ## _32)

#define TR_32(dst, dst_stride, in, sstep)                                      \
    {                                                                          \
        int i;                                                                 \
        int e32[16*16];                                                        \
        int o32[16*16];                                                        \
        LOAD16x16_O(e, in, sstep);                                             \
        for (i = 0; i < 16; i++) {                                             \
            src0_32 = vdupq_n_s32(0);   	                                       \
            src1_32 = vdupq_n_s32(0);   	                                       \
            TR_COMPUTE32x32_FIRST(0, i);                                       \
            TR_COMPUTE32x32_NEXT(2, i);                                        \
            SAVE_8x32(o32, 16, src0, src1, i);                                 \
        }                                                                      \
        LOAD16x16_O(e, (&in[16*sstep]), sstep);                                \
        for (i = 0; i < 16; i++) {                                             \
            LOAD_8x32(o32, 16, src0, src1, i);                                 \
            TR_COMPUTE32x32_FIRST(4, i);                                       \
            TR_COMPUTE32x32_NEXT(6, i);                                        \
            SAVE_8x32(o32, 16, src0, src1, i);                                 \
        }                                                                      \
        TR_16_2(e32, 16, in, 2 * sstep);                                       \
        for (i = 0; i < 16; i++) {                                             \
            SCALE32x32_2x32(dst, dst_stride, i);                               \
        }                                                                      \
    }

#define TR_32_1( dst, dst_stride, src)        TR_32( dst, dst_stride, src, 32)

////////////////////////////////////////////////////////////////////////////////
// oh_hevc_transform_XxX_X_neon
////////////////////////////////////////////////////////////////////////////////
#define TRANSFORM2(H, D)                                                   \
void oh_hevc_transform_ ## H ## x ## H ## _ ## D ## _neon (                \
    int16_t *coeffs, int col_limit) {                                          \
    int i, j;                                                          \
    int16_t *src   = coeffs;                                                   \
    int16_t  tmp[H*H];                                                         \
    int16_t  tmp_2[H*H];                                                       \
    int16_t *p_dst, *p_tra = tmp_2;                                            \
    int32x4_t src0_32, src1_32;                                            \
    int16x8_t src0, src1, src2, src3;                                            \
    int16x8_t tmp0, tmp1;                                          \
    int32x4_t tmp0_32, tmp1_32, tmp2_32, tmp3_32, tmp4_32;							   \
	int16x4_t   tmpTrn64;			\
	int32x4x2_t tmpTrn32;			\
	int16x8x2_t tmpTrn16;			\
    int16x8_t e0, e1, e2, e3, e4, e5, e6, e7;                              \
    int32x4_t e0_32, e1_32, e2_32, e3_32, e4_32, e5_32, e6_32, e7_32;		   \
    {                                                  						   \
        const int shift = 7;		                                               \
        for (i = 0; i < H; i+=8) {                                             \
            p_dst = tmp + i;                                                   \
            TR_ ## H ## _1(p_dst, H, src);                                     \
            src   += 8;                                                        \
            for (j = 0; j < H; j+=8) {                                         \
               TRANSPOSE8x8_16_LS((&p_tra[i*H+j]), H, (&tmp[j*H+i]), H, SAVE_8x16);\
            }                                                                  \
        }                                                                      \
        src   = tmp_2;                                                         \
        p_tra = coeffs;                                                         \
    }                                                                          \
    {                                                  						   \
        const int shift = 20-D;		                                               \
        for (i = 0; i < H; i+=8) {                                             \
            p_dst = tmp + i;                                                   \
            TR_ ## H ## _1(p_dst, H, src);                                     \
            src   += 8;                                                        \
            for (j = 0; j < H; j+=8) {                                         \
               TRANSPOSE8x8_16_LS((&p_tra[i*H+j]), H, (&tmp[j*H+i]), H, SAVE_8x16);\
            }                                                                  \
        }                                                                      \
        src   = tmp_2;                                                         \
        p_tra = coeffs;                                                         \
    }                                                                          \
}

TRANSFORM2(16 , 8);
TRANSFORM2(32,  8);

#endif
