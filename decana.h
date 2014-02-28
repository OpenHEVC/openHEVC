/*
 * DECoder ANAlyzer. This file is part of a set of tools intended to
 * measure video decoders performance on TI digital signal processors.
 *
 * Copyright (C) 2013 jpcano <jp.cano@alumnos.upm.es>
 *
 * decana is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * decana is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef DECANA_H
#define DECANA_H

#define DECANA_PATH_LEN 128

typedef struct _TAutomate {
	char bitstream[DECANA_PATH_LEN];
	char decoded[DECANA_PATH_LEN];
	char emulator_result[DECANA_PATH_LEN];
	int frames;
}TDecana;

void decana_init(TDecana *info, const char *swap);
unsigned int decana_getclk(unsigned int end, unsigned int ini);
void decana_writeresult(TDecana info, unsigned long accum);

#endif
