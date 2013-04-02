#include <SFML/Graphics.h>

sfRenderWindow* window;
sfTexture *im_video;
sfSprite *sp_video;
unsigned char *Data;

int Init_SDL(int edge, int frame_width, int frame_height){
	sfVideoMode mode;
    mode.width = frame_width + 2 * edge;
    mode.height = frame_height;
    Data = malloc(4 * (frame_width + 2 * edge) * frame_height* sizeof(unsigned char));

    im_video = sfTexture_create(mode.width, mode.height);
	window = sfRenderWindow_create(mode, "SFML window", sfResize | sfClose, NULL);

    sp_video = sfSprite_create();

	return 0;
}


void SDL_Display(int edge, int frame_width, int frame_height, unsigned char *Y, unsigned char *U, unsigned char *V){

#ifndef SDL_NO_DISPLAY
    for(int i = 0 ; i < (frame_width + 2 *edge) * frame_height ; i++)
    {
        Data[4*i] = Y[i];
        Data[4*i+1] = Y[i];
        Data[4*i+2] = Y[i];
        Data[4*i+3] = 255;
    }
    sfTexture_updateFromImage(<#sfTexture *texture#>, <#const sfImage *image#>, <#unsigned int x#>, <#unsigned int y#>)
    /* Draw the sprite */
    sfRenderWindow_drawSprite(window, sp_video, NULL);
    /* Update the window */
    sfRenderWindow_display(window);
    
#endif
}

void CloseSDLDisplay(){
#ifndef SDL_NO_DISPLAY	
    sfSprite_destroy(sp_video);
    sfRenderWindow_destroy(window);
#endif
}
