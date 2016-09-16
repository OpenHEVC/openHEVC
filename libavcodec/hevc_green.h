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

#endif
