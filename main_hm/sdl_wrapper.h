#ifndef _sdl_wrapper_h
#define _sdl_wrapper_h

void Init_Time();
int  Init_SDL(int edge, int frame_width, int frame_height);
void SDL_Display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V);
void CloseSDLDisplay();
int SDL_GetTime();

void initFramerate_SDL();
void setFramerate_SDL(float frate);
void framerateDelay_SDL();

#endif/* _sdl_wrapper_h */
