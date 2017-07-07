/*
 * Copyright (c) 2017, IETR/INSA of Rennes
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of the IETR/INSA of Rennes nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdlib.h>

#include "ohtimer_wrapper.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

typedef struct {
	uint32_t framecount;
	float rateticks;
	uint32_t baseticks;
	uint32_t lastticks;
	float rate;
} OHTimer;

static OHTimer timer;

static uint32_t _getTicks(void){
#ifdef WIN32
    /* Windows */
    FILETIME ft;
    LARGE_INTEGER li;

    /* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
     * to a LARGE_INTEGER structure. */
    GetSystemTimeAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;

    uint64_t ret = li.QuadPart;
    ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
    ret /= 10000; /* From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals */

    return ret;
#else
    /* Linux */
    struct timeval tv;
    unsigned long int ret;

    gettimeofday(&tv, NULL);

    ret = tv.tv_usec;
    /* Convert from micro seconds (10^-6) to milliseconds (10^-3) */
    ret /= 1000;

    /* Adds the seconds (10^0) after converting them to milliseconds (10^-3) */
    ret += (tv.tv_sec * 1000);

    return ret;
#endif
}

static void _delay(uint32_t delay){
#ifdef WIN32
	Sleep(delay);
#else
	usleep(delay*1000);
#endif
}

void oh_timer_init(void){
	timer.framecount = 0;
	timer.rate = FPS_DEFAULT;
	timer.rateticks = (1000.0f / (float) FPS_DEFAULT);
	timer.baseticks = _getTicks();
	timer.lastticks = timer.baseticks;
}

int oh_timer_getTimeMs(void){
	return (_getTicks() - timer.baseticks);
}

int oh_timer_setFPS(float rate){
	if ((rate >= FPS_LOWER_LIMIT) && (rate <= FPS_UPPER_LIMIT)) {
		timer.framecount = 0;
		timer.rate = rate;
		timer.rateticks = (1000.0f / rate);
		return (0);
	} else {
		return (-1);
	}
}

int oh_timer_getFPS(void){
	return ((int)timer.rate);
}

int oh_timer_getFrameCount(void){
	return ((int)timer.framecount);
}

uint32_t oh_timer_delay(void){
	uint32_t current_ticks;
	uint32_t target_ticks;
	uint32_t the_delay;
	uint32_t time_passed = 0;

	timer.framecount++;

	current_ticks = _getTicks();
	time_passed = current_ticks - timer.lastticks;
	timer.lastticks = current_ticks;
	target_ticks = timer.baseticks + (uint32_t) ((float) timer.framecount * timer.rateticks);

	if (current_ticks <= target_ticks) {
		the_delay = target_ticks - current_ticks;
		_delay(the_delay);
	} 

	return time_passed;
}
