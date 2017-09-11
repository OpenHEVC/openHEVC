/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */
#ifndef HEVCDSP_GREEN_H
#define HEVCDSP_GREEN_H

#include "hevcdsp.h"

void green_dsp_init(HEVCDSPContext *hevcdsp);
void green_update_filter_luma(HEVCDSPContext *c, int type);
void green_update_filter_chroma(HEVCDSPContext *c, int type);

DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green1_filters[7][2]) = {
	{ 64 , 0 },
	{ 64 , 0 },
	{ 64 , 0 },
	{ 64 , 0 },
	{ 0 , 64 },
	{ 0 , 64 },
	{ 0 , 64 },
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green2_filters[7][2]) = {
	{ 57,  7 },
	{ 49, 15 },
	{ 41, 23 },
	{ 32, 32 },
	{ 23, 41 },
	{ 15, 49 },
	{  7, 57 },
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green3_filters[7][4]) = {
	{ -3, 61,  6,  0 },
	{ -5, 56, 13,  0 },
	{ -7, 50, 21,  0 },
	{ -7, 41, 30,  0 },
	{  0, 21, 50, -7 },
	{  0, 13, 56, -5 },
	{  0,  6, 61, -3 },
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green7_filters[3][8]) = {
	{ -1,  4,-10, 58, 17, -5,  1,  0},
	{ -1,  4,-11, 40, 40,-11,  3,  0},
	{  0,  1, -5, 17, 58,-10,  4, -1}
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green5_filters[3][6]) = {
    { 1, -9, 58, 17, -3, 0 },
    { 2,-10, 41, 37, -6, 0 },
    { 0, -3, 17, 58, -9, 1 }
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green3_filters[3][4]) = {
    { -6, 58, 12,  0},
    { -7, 42, 29,  0},
    {  0, 12, 58, -6}
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green1_filters[3][2]) = {
	{ 64, 0},
	{ 64, 0},
	{ 0, 64}
};

#endif
