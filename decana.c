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

#include <stdio.h>
#include <stdlib.h>
#include "decana.h"

static void readline(FILE *f, char *line) {
	size_t len;

	fgets (line , DECANA_PATH_LEN , f);
	len = strlen(line) - 1;
	if (line[len] == '\n')
		line[len] = '\0';
}

void decana_init(TDecana *info, const char *swap) {
	FILE *f;
	char buf[DECANA_PATH_LEN];

	f = fopen (swap , "r");
	if (f == NULL) {
		printf("It is not possible to open swap file\n");
		exit(-1);
	}
	readline(f, info->bitstream);
	readline(f, info->decoded);
	readline(f, buf);
	info->frames = atoi(buf);
#ifdef EMULATOR
	readline(f, info->emulator_result);
#endif
	fclose(f);
}

unsigned int decana_getclk(unsigned int end, unsigned int ini) {
	if (end > ini)
		return end - ini;
	else {
		printf("Timer overflow corrected!\n");
		return 0xFFFFFFFF - ini + end + 1;
	}
}

void decana_writeresult(TDecana info, unsigned long accum) {
	FILE *f;

	f = fopen (info.emulator_result , "a");
	if (f == NULL) {
		printf("It is not possible to open emulator result file\n");
		exit(-1);
	}
	fprintf(f, "%s %u %lu\n", info.bitstream, info.frames, accum);
	fclose(f);
}
