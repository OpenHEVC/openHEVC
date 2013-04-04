#include <SFML/Graphics.h>
#include <SFML/System/Clock.h>
#include <stdio.h>
#include <stdlib.h>
sfVideoMode     mode;
sfRenderWindow *window;
sfTexture      *im_video;
sfSprite       *sp_video;
unsigned char  *Data;
sfClock        *sf_clock;

void Init_Time() {
    sf_clock = sfClock_create ();
    sfClock_restart(sf_clock);
}

int Init_SDL(int edge, int frame_width, int frame_height){
    mode.width        = frame_width + 2 * edge;
    mode.height       = frame_height;
    mode.bitsPerPixel = 24;
    Data = malloc(4 * (frame_width + 2 * edge) * frame_height* sizeof(unsigned char));

    /* Create the main window */
    window = sfRenderWindow_create(mode, "SFML window", sfResize | sfClose, NULL);
    if (!window)
        return -1;
    /* Load a sprite to display */
    im_video = sfTexture_create(mode.width, mode.height);
    if (!im_video)
        return -1;
    sp_video = sfSprite_create();
    return 0;
}


void SDL_Display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V){

#ifndef SDL_NO_DISPLAY
    int i;
    for(i = 0 ; i < (frame_width + 2 *edge) * frame_height ; i++) {
        Data[4*i]   = Y[i];
        Data[4*i+1] = Y[i];
        Data[4*i+2] = Y[i];
        Data[4*i+3] = 255;
    }
    sfTexture_updateFromPixels  ( im_video, (sfUint8*)Data, mode.width, mode.height, 0, 0);
    sfSprite_setTexture(sp_video, im_video, sfTrue);
    /* Draw the sprite */
    sfRenderWindow_drawSprite(window, sp_video, NULL);
    /* Update the window */
    sfRenderWindow_display(window);
    
#endif
}

void CloseSDLDisplay(){
#ifndef SDL_NO_DISPLAY
    printf("time : %d ms\n", (int)(sfClock_getElapsedTime(sf_clock).microseconds/1000));
    sfSprite_destroy(sp_video);
    sfRenderWindow_destroy(window);
#endif
}
