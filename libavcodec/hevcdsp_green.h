/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */
#ifndef HEVCDSP_GREEN_H
#define HEVCDSP_GREEN_H

#include "hevcdsp.h"

DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green1_filters[7][2]) = {
	{ 64 , 0 },
	{ 64 , 0 },
	{ 64 , 0 },
	{ 64 , 0 },
	{ 0 , 64 },
	{ 0 , 64 },
	{ 0 , 64 },
};

#if GREEN_FILTER_TYPE == OLD
DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green2_filters[7][2]) = {
    { 54, 10 },
    { 54, 10 },
    { 54, 10 },
    { 54, 10 },
    { 10, 54 },
    { 10, 54 },
    { 10, 54 }
};
#elif GREEN_FILTER_TYPE == SYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green2_filters[7][2]) = {
	{ 57,  7 },
	{ 49, 15 },
	{ 41, 23 },
	{ 32, 32 },
	{ 23, 41 },
	{ 15, 49 },
	{  7, 57 },
};
#elif GREEN_FILTER_TYPE == ASYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green2_filters[7][2]) = {
	{ 57,  7 },
	{ 49, 15 },
	{ 41, 23 },
	{ 32, 32 },
	{ 23, 41 },
	{ 15, 49 },
	{  7, 57 },
};
#endif

#if GREEN_FILTER_TYPE == OLD
DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green3_filters[7][4]) = {
	{ -3, 62,  5,  0},
	{ -5, 58, 11,  0},
	{ -7, 51, 20,  0},
	{ -6, 42, 28,  0},
	{  0, 20, 51, -7},
	{  0, 11, 58, -5},
	{  0,  5, 62, -3},
};
#elif GREEN_FILTER_TYPE == SYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green3_filters[7][4]) = {
	{ -3, 61,  6,  0 },
	{ -5, 56, 13,  0 },
	{ -7, 50, 21,  0 },
	{ -7, 41, 30,  0 },
	{  0, 21, 50, -7 },
	{  0, 13, 56, -5 },
	{  0,  6, 61, -3 },
};
#elif GREEN_FILTER_TYPE == ASYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_green3_filters[7][4]) = {
	{ -3, 61,  6,  0 },
	{ -5, 56, 13,  0 },
	{ -7, 50, 21,  0 },
	{ -7, 41, 30,  0 },
	{ -6, 32, 38,  0 },
	{ -4, 21, 47,  0 },
	{ -2, 10, 56,  0 },
};
#endif

#if GREEN_FILTER_TYPE == OLD
DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green5_filters[3][6]) = {
	{ 1, -8, 58, 16, -3, 0 },
	{ 2, -9, 40, 40, -9, 0 },
	{ 1, -4, 17, 54, -4, 0 }
};
#elif GREEN_FILTER_TYPE == SYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green5_filters[3][6]) = {
    { 1, -9, 58, 17, -3, 0 },
    { 2,-10, 41, 37, -6, 0 },
    { 0, -3, 17, 58, -9, 1 }
};
#elif GREEN_FILTER_TYPE == ASYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green5_filters[3][6]) = {
    { 1, -9, 58, 17, -3, 0 },
    { 2,-10, 41, 37, -6, 0 },
    { 1, -6, 19, 55, -5, 0 }
};
#endif


#if GREEN_FILTER_TYPE == OLD
DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green3_filters[3][4]) = {
	{ -8, 58, 14, 0},
	{ -8, 40, 32, 0},
	{ -4, 20, 48, 0}
};
#elif GREEN_FILTER_TYPE == SYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green3_filters[3][4]) = {
    { -6, 58, 12,  0},
    { -7, 42, 29,  0},
    {  0, 12, 58, -6}
};
#elif GREEN_FILTER_TYPE == ASYMMETRIC
DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green3_filters[3][4]) = {
    { -6, 58, 12,  0},
    { -7, 42, 29,  0},
    { -4, 21, 47,  0}
};
#endif

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_green1_filters[3][2]) = {
	{ 64, 0},
	{ 64, 0},
	{ 0, 64}
};

#endif
