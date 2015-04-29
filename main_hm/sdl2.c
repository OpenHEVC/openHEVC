/*
 *  yuvplay - play YUV data using SDL
 *
 *  Copyright (C) 2000, Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define SDL_NO_DISPLAY_

#ifndef SDL_NO_DISPLAY
#include <SDL.h>
#include <stdio.h>
#include "SDL_framerate.h"

/* SDL variables */
SDL_Window        *pWindow1;
SDL_Renderer      *pRenderer1;
SDL_Texture       *bmpTex1;
uint8_t           *pixels1;
int               pitch1, size1;
int               ticksSDL;

/* SDL_gfx variable */
FPSmanager   fpsm;
#endif

void Init_Time() {
#ifndef SDL_NO_DISPLAY
    ticksSDL = SDL_GetTicks();
#endif
}

int Init_SDL(int edge, int frame_width, int frame_height){

#ifndef SDL_NO_DISPLAY
    /* First, initialize SDL's video subsystem. */
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
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
#endif
    return 0;
}

void SDL_Display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V){

#ifndef SDL_NO_DISPLAY	
    size1 = (frame_width + 2 * edge) * frame_height;

    SDL_LockTexture(bmpTex1, NULL, (void **)&pixels1, &pitch1);
    memcpy(pixels1,             Y, size1  );
    memcpy(pixels1 + size1,     V, size1/4);
    memcpy(pixels1 + size1*5/4, U, size1/4);
    SDL_UnlockTexture(bmpTex1);
    SDL_UpdateTexture(bmpTex1, NULL, pixels1, pitch1);
    // refresh screen
    //    SDL_RenderClear(pRenderer1);
    SDL_RenderCopy(pRenderer1, bmpTex1, NULL, NULL);
    SDL_RenderPresent(pRenderer1);
#endif
}

void CloseSDLDisplay(){
#ifndef SDL_NO_DISPLAY
    SDL_Quit();
#endif
}
int SDL_GetTime() {
    return SDL_GetTicks() - ticksSDL;
}

// Frame rate managment
void initFramerate_SDL() {
    SDL_initFramerate(&fpsm);
}

void setFramerate_SDL(Uint32 rate) {
    if (SDL_setFramerate(&fpsm,rate) < 0) {
        printf("SDL_glx: Couldn't set frame rate\n");
        SDL_Quit();
        exit(0);
    }
}

void framerateDelay_SDL() {
    if (SDL_framerateDelay(&fpsm) < 0) {
        printf("SDL_glx: Couldn't set frame rate delay\n");
        SDL_Quit();
        exit(0);
    }
}
