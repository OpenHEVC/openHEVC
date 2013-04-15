//
//  main.c
//  libavHEVC
//
//  Created by Mickaël Raulet on 11/10/12.
//
//
#include "openHevcWrapper.h"
#include "getopt.h"


//#include <OpenGL/gl.h>
//#include <OpenGL/glu.h>
//#include <GLUT/glut.h>


int find_start_code (unsigned char *Buf, int zeros_in_startcode)
{
    int i;
    for (i = 0; i < zeros_in_startcode; i++)
        if(Buf[i] != 0)
            return 0;
    return Buf[i];
}

int get_next_nal(FILE* inpf, unsigned char* Buf)
{
    int pos = 0;
    int StartCodeFound = 0;
    int info2 = 0;
    int info3 = 0;
    while(!feof(inpf)&&(/*Buf[pos++]=*/fgetc(inpf))==0);

    while (pos < 3) Buf[pos++] = fgetc (inpf);
    while (!StartCodeFound)
    {
        if (feof (inpf))
        {
            //            return -1;
            return pos-1;
        }
        Buf[pos++] = fgetc (inpf);
        info3 = find_start_code(&Buf[pos-4], 3);
        if(info3 != 1)
            info2 = find_start_code(&Buf[pos-3], 2);
        StartCodeFound = (info2 == 1 || info3 == 1);
    }
    fseek (inpf, - 4 + info2, SEEK_CUR);
    return pos - 4 + info2;
}

static int YUV2RGB  (   unsigned char   *picture,
                        unsigned char   *picture_u,
                        unsigned char   *picture_v,
                        unsigned char   *picture_RGB,
                        unsigned int    width,
                        unsigned int    height  )	// convert YUV_888 to RGB_888
{
    unsigned char clipping[1024];
    for(int n=-512; n<512; n++)
    {
        if(n<=0)
        {
            clipping[512+n]=0;
        }
        else if (n>=255)
        {
            clipping[512+n]=255;
        }
        else
        {
            clipping[512+n]=n;
        }
    }
    unsigned char* clip = clipping+512;
    for(int y=0; y<height; y++)
    {
        int ycr = y/2;

        unsigned char *pos_y=picture   + y   * width;
        unsigned char *pos_u=picture_u + ycr * (width>>1);
        unsigned char *pos_v=picture_v + ycr * (width>>1);

        unsigned char *pos_rgb= picture_RGB + y*width*3;
        for(int x=0; x<width; x+=2)
        {
            int c = *pos_y-16;
            int d = *pos_u-128;
            int e = *pos_v-128;
            pos_rgb[0]=clip[(298*c + 409*e + 128)>>8];
            pos_rgb[1]=clip[(298*c -100*d - 208*e + 128)>>8];
            pos_rgb[2]=clip[(298*c + 516*d + 128)>>8];
            pos_y++;
            pos_rgb +=3;
            c = *pos_y-16;
            pos_rgb[0]=clip[(298*c + 409*e + 128)>>8];
            pos_rgb[1]=clip[(298*c -100*d - 208*e + 128)>>8];
            pos_rgb[2]=clip[(298*c + 516*d + 128)>>8];
            pos_y++;
            pos_u++;
            pos_v++;
            pos_rgb +=3;
        }
    }
    return 1;
}
/*
static int createWindow(int width, int height, int bpp, int fullscreen, const char* title)
{
    if( SDL_Init( SDL_INIT_VIDEO ) != 0 )
        return 0;
    
        //all values are "at least"!
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    // Set the title.
    SDL_WM_SetCaption(title, title);
    // Flags tell SDL about the type of window we are creating.
    int flags = SDL_OPENGL | SDL_RESIZABLE ; //| SDL_NOFRAME
    if(fullscreen == true)
    {
        flags |= SDL_FULLSCREEN;
    }
    // Create the window
    //putenv(strdup("SDL_VIDEO_CENTERED=1"));
    _putenv("SDL_VIDEO_WINDOW_POS=10,10");
    screen = SDL_SetVideoMode( width, height, bpp, flags );
    m_bpp = bpp;
    m_flags = flags;
    if(screen == 0)
        return 0;
    
    return true;
}
*/
/*
static int InitGL  (   unsigned char   *picture,
                unsigned char   *picture_u,
                unsigned char   *picture_v,
                unsigned char   width,
                unsigned char   height  )										// All Setup For OpenGL Goes Here
{
    if (!m_window.createWindow(width, height, 32, false, "HEVC Decoder"))
    {
        return false;
    }
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);							// Enable Texture Mapping
    glClearColor(0.0f, 0.0f, 0.0f, 0.5f);				// Black Background
    glClear(GL_COLOR_BUFFER_BIT );	// Clear The Screen And The Depth Buffer
    //glShadeModel(GL_FLAT);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment
    glLoadIdentity(); // Reset The View
    glGenTextures(1, (GLuint *)(&texture[0])); // Create The Texture
    // get OpenGL info
    glInfo glInfo;
    glInfo.getInfo();
#ifdef _WIN32
    // check PBO is supported by your video card
    if(glInfo.isExtensionSupported("GL_ARB_pixel_buffer_object"))
    {
        // get pointers to GL functions
        glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)wglGetProcAddress("glGenBuffersARB");
        glBindBufferARB = (PFNGLBINDBUFFERARBPROC)wglGetProcAddress("glBindBufferARB");
        glBufferDataARB = (PFNGLBUFFERDATAARBPROC)wglGetProcAddress("glBufferDataARB");
        glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)wglGetProcAddress("glBufferSubDataARB");
        glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)wglGetProcAddress("glDeleteBuffersARB");
        glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)wglGetProcAddress("glGetBufferParameterivARB");
        glMapBufferARB = (PFNGLMAPBUFFERARBPROC)wglGetProcAddress("glMapBufferARB");
        glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)wglGetProcAddress("glUnmapBufferARB");
        
        // check once again PBO extension
        if(glGenBuffersARB && glBindBufferARB && glBufferDataARB && glBufferSubDataARB &&
           glMapBufferARB && glUnmapBufferARB && glDeleteBuffersARB && glGetBufferParameterivARB)
        {
            flag_vbo= true;
            std::cout << "Video card supports GL_ARB_pixel_buffer_object. \n" ;
        }
        else
        {
            flag_vbo= false;
            std::cout << "Video card does NOT support GL_ARB_pixel_buffer_object. Please do not use -d option ! \n" ;
            exit(1);
        }
    }
#else // for linux, do not need to get function pointers, it is up-to-date
    if(glInfo.isExtensionSupported("GL_ARB_pixel_buffer_object"))
    {
        flag_vbo = true;
        std::cout << "Video card supports GL_ARB_pixel_buffer_object. \n" ;
    }
    else
    {
        flag_vbo = false;
        std::cout << "Video card does NOT support GL_ARB_pixel_buffer_object. \n" ;
    }
#endif
    //flag_vbo=false;  // deactivate VBO
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    picture_RGB = (unsigned char*) calloc(width*height*3,sizeof(unsigned char));
    if(flag_vbo == true)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, 3 , width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, picture_RGB);
        free(picture_RGB);
        glGenBuffersARB(1, pboIds); //only one buffer version
        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIds[0]);
        glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, width*height*3, 0, GL_STREAM_DRAW_ARB); // not sure if sizeof(GLubyte) is needed...
        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);  // reserve memory space
        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pboIds[0]); // rebind
#if !REMAP_UBO
        ptr_vbo = (GLubyte*)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
#endif
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, 3 , width, height/NB_TILES_DISPLAY, 0, GL_RGB, GL_UNSIGNED_BYTE, picture_RGB);
    }
    nb_frame=0;
    flag_pause = false;
    flag_next = false;
    return TRUE;										// Initialization Went OK
}
*/
static void video_decode_example(const char *filename)
{
    FILE *f;
    int init    = 1;
    int nbFrame = 0;
    unsigned int width, height, stride;
    unsigned char * buf, *Y, *U, *V;
    
    libOpenHevcInit();
    libOpenHevcSetCheckMD5(check_md5_flags);
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "could not open %s\n", filename);
        exit(1);
    }
    buf = calloc ( 1000000, sizeof(char));
    if (display_flags == DISPLAY_ENABLE) {
        while(!feof(f)) {
            int got_picture = libOpenHevcDecode(buf, get_next_nal(f, buf));
            libOpenHevcGetOuptut(got_picture, &Y, &U, &V);
            if (got_picture != 0) {
                fflush(stdout);
                if (init == 1 ) {
                    libOpenHevcGetPictureSize2(&width, &height, &stride);
                    Init_SDL((stride - width)/2, width, height);
                    Init_Time();
                    init = 0;
                }
                SDL_Display((stride - width)/2, width, height, Y, U, V);
                // Use open GL to display
               // YUV2RGB(Y, U, V, picture_RGB, width, height);
                
                nbFrame++;
            }
        }
    } else {
        while(!feof(f)) {
            int got_picture = libOpenHevcDecode(buf, get_next_nal(f, buf));
            libOpenHevcGetOuptut(got_picture, &Y, &U, &V);
            if (got_picture != 0) {
                if (init == 1 ) {
                    Init_Time();
                    init = 0;
                }
                nbFrame++;
            }
        }
     }
    CloseSDLDisplay();
    fclose(f);
    libOpenHevcClose();
    printf("nbFrame : %d\n", nbFrame);
}


int main(int argc, char *argv[]) {
    init_main(argc, argv);
    video_decode_example(input_file);
    return 0;
}

