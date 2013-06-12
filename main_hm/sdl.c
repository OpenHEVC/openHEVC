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

#include <SDL.h>


/* SDL variables */
SDL_Surface *screen;
SDL_Overlay *yuv_overlay;
SDL_Rect     rect;
int          ticksSDL;

void Init_Time() {
#ifndef SDL_NO_DISPLAY
    ticksSDL = SDL_GetTicks();
#endif
}

int Init_SDL(int edge, int frame_width, int frame_height){

#ifndef SDL_NO_DISPLAY
    int screenwidth = 0, screenheight = 0;
    char *window_title = "SDL Display";
    const SDL_VideoInfo* info;
    Uint8 bpp;
    Uint32 vflags;
    
    /* First, initialize SDL's video subsystem. */
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
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
#endif
    return 0;
}

void SDL_Display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V){

#ifndef SDL_NO_DISPLAY
    if (SDL_LockYUVOverlay(yuv_overlay) < 0) return;
    // let's draw the data (*yuv[3]) on a SDL screen (*screen)
    memcpy(yuv_overlay->pixels[0], Y, (frame_width + 2 * edge) * frame_height);
    memcpy(yuv_overlay->pixels[1], V, (frame_width + 2 * edge) * frame_height / 4);
    memcpy(yuv_overlay->pixels[2], U, (frame_width + 2 * edge) * frame_height / 4);
    SDL_UnlockYUVOverlay(yuv_overlay);
    // Show, baby, show!
    SDL_DisplayYUVOverlay(yuv_overlay, &rect);
#endif
}

void CloseSDLDisplay(){
#ifndef SDL_NO_DISPLAY
    printf("time : %d ms\n", SDL_GetTicks() - ticksSDL);
    SDL_FreeYUVOverlay(yuv_overlay);
    SDL_Quit();
#endif
}
