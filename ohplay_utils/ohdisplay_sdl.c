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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL/SDL.h>
#include <SDL/SDL_events.h>
#include <stdio.h>
#include <signal.h>
#include "ohtimer_wrapper.h"
#include "ohdisplay_wrapper.h"

/* SDL variables */
SDL_Surface *screen;
SDL_Overlay *yuv_overlay;
SDL_Rect     rect;

OHMouse oh_mouse;
OHMouse oh_display_getMouseEvent(){
    return oh_mouse;
}

OHEvent oh_display_getWindowEvent(void){
    SDL_Event event;

    int ret = OH_NOEVENT;

    event.type = 0;
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
    int screenwidth = 0, screenheight = 0;
    const char *window_title = "SDL Display";
    const SDL_VideoInfo* info;
    Uint8 bpp;
    Uint32 vflags;
    
    struct sigaction action;
    sigaction(SIGINT, NULL, &action);
    sigaction(SIGTERM, NULL, &action);
    sigaction(SIGKILL, NULL, &action);
    sigaction(SIGHUP, NULL, &action);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    if( SDL_Init( SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0 ) {
        /* Failed, exit. */
        printf("Video initialization failed: %s\n", SDL_GetError( ) );
        SDL_Quit();
        exit(0);
    }

    info = SDL_GetVideoInfo();
    if( !info ) {
        printf("SDL ERROR Video query failed: %s\n", SDL_GetError() );
        SDL_Quit();
        exit(0);
    }
    
    bpp = info->vfmt->BitsPerPixel;

    if(info->hw_available)
        vflags = SDL_HWSURFACE;
    else
        vflags = SDL_SWSURFACE;

    // set window title
    SDL_WM_SetCaption(window_title, NULL);

    screenwidth  = frame_width;
    screenheight = frame_height;
    //fprintf(stderr, "Error -----  \n");
    screen = SDL_SetVideoMode(screenwidth, screenheight, bpp, vflags);
    if ( screen == NULL ) {
        printf("SDL: Couldn't set %dx%d: %s", screenwidth, screenheight, SDL_GetError());
        SDL_Quit();
        exit(0);
    }
    yuv_overlay = SDL_CreateYUVOverlay(frame_width + 2 * edge, frame_height, SDL_YV12_OVERLAY, screen);
    if ( yuv_overlay == NULL ) {
        printf("SDL: Couldn't create SDL_yuv_overlay: %s",SDL_GetError());
        SDL_Quit();
        exit(0);
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = screenwidth + 2 * edge;
    rect.h = screenheight;
    SDL_DisplayYUVOverlay(yuv_overlay, &rect);
    return 0;
}

void oh_display_display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V){
	if (SDL_LockYUVOverlay(yuv_overlay) < 0) return;
    if(yuv_overlay->w != frame_width || yuv_overlay->h != frame_height){
        //Resize YUVoverlay
        Uint8 bpp;
        Uint32 vflags;
        const SDL_VideoInfo* info;
        info = SDL_GetVideoInfo();

        if( !info ) {
            printf("SDL ERROR Video query failed: %s %d\n", SDL_GetError(), (int)info );
            SDL_Quit();
            exit(0);
        }

        bpp = info->vfmt->BitsPerPixel;

        if(info->hw_available)
            vflags = SDL_HWSURFACE;
        else
            vflags = SDL_SWSURFACE;

        SDL_FreeSurface(screen);

        screen = SDL_SetVideoMode(frame_width, frame_height, bpp, vflags);

        if ( screen == NULL ) {
            printf("SDL: Couldn't set %dx%d: %s", frame_width, frame_height, SDL_GetError());
            SDL_Quit();
            exit(0);
        }

        SDL_FreeYUVOverlay(yuv_overlay);

        yuv_overlay = SDL_CreateYUVOverlay(frame_width + 2 * edge, frame_height, SDL_YV12_OVERLAY, screen);
        if ( yuv_overlay == NULL ) {
            printf("SDL: Couldn't create SDL_yuv_overlay: %s",SDL_GetError());
            SDL_Quit();
            exit(0);
        }

        rect.x = 0;
        rect.y = 0;
        rect.w = frame_width + 2 * edge;
        rect.h = frame_height;
    }


    // let's draw the data (*yuv[3]) on a SDL screen (*screen)
    memcpy(yuv_overlay->pixels[0], Y, (frame_width + 2 * edge) * frame_height);
    if (V)
        memcpy(yuv_overlay->pixels[1], V, (frame_width + 2 * edge) * frame_height / 4);
    else
        memset(yuv_overlay->pixels[1], 0x80, (frame_width + 2 * edge) * frame_height / 4);


    if (U)
        memcpy(yuv_overlay->pixels[2], U, (frame_width + 2 * edge) * frame_height / 4);
    else
        memset(yuv_overlay->pixels[2], 0x80, (frame_width + 2 * edge) * frame_height / 4);

    SDL_UnlockYUVOverlay(yuv_overlay);
    // Show, baby, show!
    SDL_DisplayYUVOverlay(yuv_overlay, &rect);
}

void oh_display_close(){
    SDL_FreeYUVOverlay(yuv_overlay);
    SDL_FreeSurface(screen);
    SDL_Quit();
}
