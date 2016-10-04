#ifndef _sdl_wrapper_h
#define _sdl_wrapper_h

typedef enum oh_event_t {OH_NOEVENT=0, OH_LAYER0, OH_LAYER1, OH_QUIT} oh_event;

oh_event IsCloseWindowEvent();
void Init_Time();
int  Init_SDL(int edge, int frame_width, int frame_height);
void SDL_Display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V);
void CloseSDLDisplay();
int SDL_GetTime();

void initFramerate_SDL();
void setFramerate_SDL(float frate);
void framerateDelay_SDL();

#endif/* _sdl_wrapper_h */
