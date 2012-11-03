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


#ifdef WIN32
#include <SDL.h>
#else
#include "SDL.h"
#endif


/* SDL variables */
SDL_Surface *screen;
SDL_Overlay *yuv_overlay;
SDL_Rect rect;

static int got_sigint = 0;





int Init_SDL(int edge, int frame_width, int frame_height){
	
#ifndef SDL_NO_DISPLAY	
	int screenwidth = 0, screenheight = 0;
	unsigned char *yuv[3];
	char *window_title = "SDL Display";


	/* First, initialize SDL's video subsystem. */
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		/* Failed, exit. */
		printf("Video initialization failed: %s\n", SDL_GetError( ) );
	}

	// set window title 
	SDL_WM_SetCaption(window_title, NULL);

	// yuv params
	yuv[0] = malloc((frame_width + 2 * edge) * frame_height * sizeof(unsigned char));
	yuv[1] = malloc((frame_width + edge) * frame_height / 4 * sizeof(unsigned char));
	yuv[2] = malloc((frame_width + edge) * frame_height / 4 * sizeof(unsigned char));

	screenwidth = frame_width;
	screenheight = frame_height;

	
	screen = SDL_SetVideoMode(screenwidth, screenheight, 24, SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL);

	if ( screen == NULL ) {
		printf("SDL: Couldn't set %dx%d: %s", screenwidth, screenheight, SDL_GetError());
		exit(1);
	}
	else {
		printf("SDL: Set %dx%d @ %d bpp \n",screenwidth, screenheight, screen->format->BitsPerPixel);
	}

	// since IYUV ordering is not supported by Xv accel on maddog's system
	//  (Matrox G400 --- although, the alias I420 is, but this is not
	//  recognized by SDL), we use YV12 instead, which is identical,
	//  except for ordering of Cb and Cr planes...
	// we swap those when we copy the data to the display buffer...

	yuv_overlay = SDL_CreateYUVOverlay(frame_width + 2 * edge, frame_height, SDL_YV12_OVERLAY, screen);

	if ( yuv_overlay == NULL ) {
		printf("SDL: Couldn't create SDL_yuv_overlay: %s",SDL_GetError());
		exit(1);
	}


	rect.x = 0;
	rect.y = 0;
	rect.w = screenwidth + 2 * edge;
	rect.h = screenheight;
	SDL_UnlockYUVOverlay(yuv_overlay);

	SDL_DisplayYUVOverlay(yuv_overlay, &rect);
#endif
	return 0;
}


void SDL_Display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V){

#ifndef SDL_NO_DISPLAY	

	// Lock SDL_yuv_overlay 
	if ( SDL_MUSTLOCK(screen) ) {
		if ( SDL_LockSurface(screen) < 0 ) return;
	}
	if (SDL_LockYUVOverlay(yuv_overlay) < 0) return;

	if (frame_width != screen -> w || frame_height != screen -> h){
		screen -> clip_rect . w = screen -> w = frame_width;
		screen -> clip_rect . h = screen -> h = frame_height;
		screen = SDL_SetVideoMode(frame_width, frame_height, 24, SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL);
		yuv_overlay -> w = rect . w = frame_width + 2 * edge;
		yuv_overlay -> h = rect . h = frame_height;
        yuv_overlay = SDL_CreateYUVOverlay(frame_width + 2 * edge, frame_height, SDL_YV12_OVERLAY, screen);
        
	}
		
	if ( screen == NULL ) {
		printf("SDL: Couldn't set %dx%d: %s", frame_width, frame_height, SDL_GetError());
		exit(1);
	}

	// let's draw the data (*yuv[3]) on a SDL screen (*screen) 
	memcpy(yuv_overlay->pixels[0], Y, (frame_width + 2 * edge) * frame_height);
	memcpy(yuv_overlay->pixels[1], V, (frame_width + 2 * edge) * frame_height / 4);
	memcpy(yuv_overlay->pixels[2], U, (frame_width + 2 * edge) * frame_height / 4);

	// Unlock SDL_yuv_overlay 
	if ( SDL_MUSTLOCK(screen) ) {
		SDL_UnlockSurface(screen);
	}
	SDL_UnlockYUVOverlay(yuv_overlay);

	// Show, baby, show!
	SDL_DisplayYUVOverlay(yuv_overlay, &rect);

#endif
}

void CloseSDLDisplay(){
#ifndef SDL_NO_DISPLAY	
	SDL_FreeYUVOverlay(yuv_overlay);
	SDL_Quit();
#endif
}



/**
IETR Stuffs
*/
void SDL_Display_preesm(int edge, unsigned char *display_image)
{
	int XDIM = ((unsigned int *) display_image)[0];
	int YDIM = ((unsigned int *) display_image)[1];
	unsigned char *Y = display_image + 8;
	unsigned char *U = Y + (XDIM + 32) * YDIM;
	unsigned char *V = U + (XDIM + 32) * YDIM/4 ;
	SDL_Display(edge, XDIM, YDIM, Y, U, V);
}