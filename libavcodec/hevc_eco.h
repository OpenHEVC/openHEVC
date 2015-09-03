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

#endif
