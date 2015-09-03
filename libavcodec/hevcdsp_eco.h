/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */
#ifndef HEVCDSP_ECO_H
#define HEVCDSP_ECO_H

#include "hevcdsp.h"

DECLARE_ALIGNED(16, const int8_t, ff_hevc_epel_eco2_filters[7][2]) = {
    { 54, 10},
    { 54, 10},
    { 54, 10},
    { 54, 10},
    { 54, 10},
    { 54, 10},
    { 54, 10}
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_eco3_filters[3][3]) = {
    { -8, 58, 14},
    { -8, 40, 32},
    {  -4, 20, 48}
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_qpel_eco1_filters[3][2]) = {
	{ 64, 0},
	{ 64, 0},
	{ 0, 64}
};


/** ECO reload functions */
void eco_reload_filter_luma1(HEVCDSPContext *c, const int bit_depth);
void eco_reload_filter_luma3(HEVCDSPContext *c, const int bit_depth);
void eco_reload_filter_luma7(HEVCDSPContext *c, const int bit_depth);
void eco_reload_filter_chroma1(HEVCDSPContext *c, const int bit_depth);
void eco_reload_filter_chroma2(HEVCDSPContext *c, const int bit_depth);
void eco_reload_filter_chroma4(HEVCDSPContext *c, const int bit_depth);

#endif
