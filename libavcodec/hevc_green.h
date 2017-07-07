/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */

#ifndef HEVC_GREEN_H
#define HEVC_GREEN_H

#include "hevc.h"

void green_update(HEVCContext *s);
void green_logs(HEVCContext *s);
void green_update_context(HEVCContext *s, HEVCContext *s0);
/** Green defines */
#define LUMA1		1
#define LUMA3		3
#define LUMA5		5
#define LUMA7		7
#define LUMA_LEG	8
#define N_LUMA 		9

#define CHROMA1		1
#define CHROMA2		2
#define CHROMA3		3
#define CHROMA_LEG	4
#define N_CHROMA 	5


typedef struct luma_config{
    void (*put_hevc_qpel[10][2][2])(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);

    void (*put_hevc_qpel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride,
    								   uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride,
    									 uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2,
                                         int height, int denom, int wx0, int wx1,
                                         int ox0, int ox1, intptr_t mx, intptr_t my, int width);
} luma_config;

typedef struct chroma_config{
    void (*put_hevc_epel[10][2][2])(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2,
                                         int height, int denom, int wx0, int ox0, int wx1,
                                         int ox1, intptr_t mx, intptr_t my, int width);
} chroma_config;


/** Coef Specific Init Fcts */
void green_reload_filter_luma7_neon(luma_config *c, const int bit_depth);
void green_reload_filter_luma5_neon(luma_config *c, const int bit_depth);
void green_reload_filter_luma3_neon(luma_config *c, const int bit_depth);
void green_reload_filter_luma1_neon(luma_config *c, const int bit_depth);

void green_reload_filter_chroma3_neon(chroma_config *c, const int bit_depth);
void green_reload_filter_chroma2_neon(chroma_config *c, const int bit_depth);
void green_reload_filter_chroma1_neon(chroma_config *c, const int bit_depth);

#endif
