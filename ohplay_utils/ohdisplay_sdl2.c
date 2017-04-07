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

#include <SDL.h>
#include <SDL_events.h>
#include <stdio.h>
#include <signal.h>
#include "ohtimer_wrapper.h"
#include "ohdisplay_wrapper.h"

/* SDL variables */
SDL_Window        *pWindow1;
SDL_Renderer      *pRenderer1;
SDL_Texture       *bmpTex1;
uint8_t           *pixels1;
int               pitch1, size1;

OHMouse oh_mouse;

OHMouse oh_display_getMouseEvent(){
    return oh_mouse;
}

OHEvent oh_display_getWindowEvent(){
    int ret = OH_NOEVENT;
    SDL_Event event;
    while(SDL_PollEvent(&event)){

        switch( event.type ){
            /* Keyboard event */
            case SDL_KEYDOWN:
                if( event.key.keysym.sym == SDLK_UP) {
                    ret = OH_LAYER1;
                } else if( event.key.keysym.sym == SDLK_DOWN) {
                    ret = OH_LAYER0;
                }
                break;

            /* SDL_QUIT event (window close) */
            case SDL_QUIT:
                ret = OH_QUIT;
                break;
            case SDL_MOUSEBUTTONDOWN:
                oh_mouse.on = 1;
                oh_mouse.x=event.button.x;
                oh_mouse.y=event.button.y;
                ret = OH_MOUSE;
                break;
            default:
                break;
        }
    }

    return ret;
}

int oh_display_init(int edge, int frame_width, int frame_height){
    /* First, initialize SDL's video subsystem. */
    struct sigaction action;
    sigaction(SIGINT, NULL, &action);
    sigaction(SIGTERM, NULL, &action);
    sigaction(SIGKILL, NULL, &action);
    sigaction(SIGHUP, NULL, &action);
    SDL_Init(SDL_INIT_EVERYTHING);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS ) < 0 ) {
        /* Failed, exit. */
        printf("Video initialization failed: %s\n", SDL_GetError( ) );
    }
    // allocate window, renderer, texture
    pWindow1    = SDL_CreateWindow( "YUV", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                    (frame_width + 2 * edge), frame_height, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    pRenderer1  = SDL_CreateRenderer(pWindow1, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    bmpTex1     = SDL_CreateTexture(pRenderer1, SDL_PIXELFORMAT_YV12,
                    SDL_TEXTUREACCESS_STREAMING, (frame_width + 2 * edge), frame_height);
    if(pWindow1==NULL || pRenderer1==NULL || bmpTex1==NULL) {
        printf("Could not open window1\n");
        return -1;
    }
    return 0;
}

void oh_display_display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V){
	int win_height, win_width;
	SDL_GetWindowSize(pWindow1, &win_width, &win_height );

	if(frame_width != win_width || frame_height != win_height){
		SDL_SetWindowSize(pWindow1, frame_width, frame_height );
		SDL_DestroyTexture(bmpTex1);
		bmpTex1 = SDL_CreateTexture(pRenderer1, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, (frame_width + 2 * edge), frame_height);
	}

    size1 = (frame_width + 2 * edge) * frame_height;

    SDL_LockTexture(bmpTex1, NULL, (void **)&pixels1, &pitch1);
    memcpy(pixels1,             Y, size1  );
    if (V)
        memcpy(pixels1 + size1,     V, size1/4);
    else
        memset(pixels1 + size1,     0x80, size1/4);
    if (U)
        memcpy(pixels1 + size1*5/4, U, size1/4);
    else
        memset(pixels1 + size1*5/4, 0x80, size1/4);
    SDL_UnlockTexture(bmpTex1);
    SDL_UpdateTexture(bmpTex1, NULL, pixels1, pitch1);
    // refresh screen
    //    SDL_RenderClear(pRenderer1);
    SDL_RenderCopy(pRenderer1, bmpTex1, NULL, NULL);
    SDL_RenderPresent(pRenderer1);
}

void oh_display_close(){
    SDL_Quit();
}
