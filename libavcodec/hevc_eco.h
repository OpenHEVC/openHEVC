/*
 * HEVC video energy efficient decoder
 * Morgan Lacour 2015
 */

#ifndef HEVC_ECO_H
#define HEVC_ECO_H

#include "hevc.h"

void eco_get_activation(HEVCContext *s);
void eco_set_activation(HEVCContext *s);

void eco_logs(HEVCContext *s);

void eco_update_context(HEVCContext *s, HEVCContext *s0);

/** ECO defines */
#define LUMA1 1
#define LUMA3 3
#define LUMA5 5
#define LUMA7 7
#define CHROMA1 1
#define CHROMA2 2
#define CHROMA4 4

#endif
