//a Node.js add-on to wrap a GPU to look like a parallel port

////////////////////////////////////////////////////////////////////////////////
////
/// GpuPort
//

//This is a Node.js add-on to display a rectangular grid of pixels (a texture) on screen using SDL2 and hardware acceleration (via GPU).
//In essence, the RPi GPU provides a 24-bit high-speed parallel port with precision timing.
//Optionally, OpenGL and GLSL shaders can be used for generating effects.
//In dev mode, an SDL window is used.  In live mode, full screen is used (must be configured for desired resolution).
//Screen columns generate data signal for each bit of the 24-bit parallel port.
//Without external mux, there can be 24 "universes" of external LEDs to be controlled.  Screen height defines max universe length.

//Copyright (c) 2015, 2016, 2017, 2018 Don Julien, djulien@thejuliens.net


////////////////////////////////////////////////////////////////////////////////
////
/// Setup
//

//1. Dependencies:
//1a. install nvm
//1b. install node v10.12.0  #or later; older versions might work; ymmv
//1c. install git client
//2. Create parent project
//2a. create a folder + cd into it
//2b. npm init
//2c. npm install --save gpuport
//OR
//2a. git clone this repo
//2b. cd this folder
//2c. npm install
//2d. npm test  #dev mode test, optional

//1d. upgrade node-gyp >= 3.6.2
//     [sudo] npm explore npm -g -- npm install node-gyp@latest
//3. config:
//3a. edit /boot/config.txt
//3b. give RPi GPU 256 MB RAM (optional)


//WS281X notes:
//30 usec = 33.3 KHz node rate
//1.25 usec = 800 KHz bit rate; x3 = 2.4 MHz data rate => .417 usec
//AC SSRs:
//120 Hz = 8.3 msec; x256 ~= 32.5 usec (close enough to 30 usec); OR x200 = .0417 usec == 10x WS281X data rate
//~ 1 phase angle dimming time slot per WS281X node
//invert output
//2.7 Mbps serial date rate = SPBRG 2+1
//8+1+1 bits = 3x WS281X data rate; 3 bytes/WS281X node
//10 serial bits compressed into 8 WS281X data bits => 2/3 reliable, 1/3 unreliable bits
//5 serial bits => SPBRG 5+1, 1.35 Mbps; okay since need to encode anyway?


////////////////////////////////////////////////////////////////////////////////
////
/// Headers, general macros, inline functions
//

#include "sdl-helpers.h"


////////////////////////////////////////////////////////////////////////////////
////
/// unit test:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#if 0
//http://lazyfoo.net/tutorials/SDL/01_hello_SDL/linux/cli/index.php
//https://github.com/AndrewFromMelbourne/sdl_image_example/blob/master/test_image.c
int main(int argc, char* argv[])
{
    SDL_Surface *screen = NULL;
    SDL_Surface *image = NULL;
    const SDL_VideoInfo *videoInfo = NULL;
    /*-----------------------------------------------------------------*/
    if (argc != 2)
    {
        fprintf(stderr, "single argument ... name of image to display\n");
        return 1;
    }
    /*-----------------------------------------------------------------*/
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL_Init failed - %s\n", SDL_GetError());
        return 1;
    }
    /*-----------------------------------------------------------------*/
    videoInfo = SDL_GetVideoInfo();
    if (videoInfo == 0)
    {
        fprintf(stderr, "SDL_GetVideoInfo failed - %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    /*-----------------------------------------------------------------*/
    image = IMG_Load(argv[1]);
    if (!image)
    {
        fprintf(stderr, "IMG_Load failed - %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    /*-----------------------------------------------------------------*/
    screen = SDL_SetVideoMode(image->w,
                              image->h,
                              videoInfo->vfmt->BitsPerPixel,
                              SDL_HWSURFACE);
    if (!screen)
    {
        fprintf(stderr, "SetVideoMode failed - %s\n", SDL_GetError());
        SDL_FreeSurface(image);
        SDL_Quit();
        return 1;
    }
    /*-----------------------------------------------------------------*/
    SDL_BlitSurface(image, 0, screen, 0);
    SDL_Delay(5000);
    SDL_FreeSurface(image);
    SDL_Quit();
    return 0;
}
#endif


//#include <iostream> //std::cout
#include "sdl-helpers.h"
#include "msgcolors.h"
#include "srcline.h"


//int main(int argc, const char* argv[])
void unit_test()
{
//    SDL_Lib sdllib(SDL_INIT_VIDEO, SRCLINE);
    SDL_AutoSurface surf(xx);
    debug(BLUE_MSG << "finish" << ENDCOLOR);
//    return 0;
}

#endif //def WANT_UNIT_TEST
//eof