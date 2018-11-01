////////////////////////////////////////////////////////////////////////////////
////
/// RPi helpers (runnable on non-RPi machines):
//

#ifndef _RPI_HELPERS_H
#define _RPI_HELPERS_H

//NOTE from https://stackoverflow.com/questions/1277627/overhead-of-pthread-mutexes
//about atomic: In practice, you can assume that int and other integer types no longer than int are atomic. You can also assume that pointer types are atomic
// http://axisofeval.blogspot.com/2010/11/numbers-everybody-should-know.html
//go ahead and use std::atomic<> anyway, for safety:
#include <atomic> //std::atomic. std::memory_order
#include <sys/stat.h> //struct stat
#include <stdint.h> //uint*_t


enum class tristate: int {No = false, Yes = true, Maybe, Error = Maybe};

//check for file existence:
bool exists(const char* path)
{
    struct stat info;
    return !stat(path, &info); //file exists
}


//check for RPi:
//NOTE: results are cached (outcome won't change until reboot)
bool isRPi()
{
//NOTE: mutex not needed here
//main thread will call first, so race conditions won't occur (benign anyway)
    static std::atomic<tristate> isrpi(tristate::Maybe);
//    static std::mutex protect;
//    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

//    myprintf(3, BLUE_MSG "isRPi()" ENDCOLOR);
//    serialize.lock(); //not really needed (low freq api), but just in case
    if (isrpi == tristate::Maybe) isrpi = exists("/boot/config.txt")? tristate::Yes: tristate::No;
//    serialize.unlock();
    return (isrpi == tristate::Yes);
}


typedef struct WH { uint16_t w, h; } WH; //pack width, height into single word for easy return from functions

//get screen width, height:
//wrapped in a function so it can be used as initializer (optional)
//screen height determines max universe size
//screen width should be configured according to desired data rate (DATA_BITS per node)
WH ScreenInfo()
{
//NOTE: mutex not needed here, but std::atomic complains about deleted function
//main thread will call first, so race conditions won't occur (benign anyway)
//    static std::atomic<int> w = 0, h = {0};
    static std::atomic<WH> wh = {0};
//    static std::mutex protect;
//    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

    if (!wh.w || !wh.h)
    {
        const ScreenConfig* scfg = getScreenConfig();
//        if (!scfg) //return_void(errjs(iso, "Screen: can't get screen info"));
        if (!scfg) /*throw std::runtime_error*/ exc("Can't get screen size");
        wh.w = scfg->mode_line.hdisplay;
        wh.h = scfg->mode_line.vdisplay;
#if 0
//        auto_ptr<SDL_lib> sdl(SDL_INIT(SDL_INIT_VIDEO)); //for access to video info; do this in case not already done
        if (!SDL) SDL = SDL_INIT(SDL_INIT_VIDEO);

        if (!SDL_WasInit(SDL_INIT_VIDEO)) err(RED_MSG "ERROR: Tried to get screen info before SDL_Init" ENDCOLOR);
//        if (!sdl && !(sdl = SDL_INIT(SDL_INIT_VIDEO))) err(RED_MSG "ERROR: Tried to get screen before SDL_Init" ENDCOLOR);
        myprintf(22, BLUE_MSG "%d display(s):" ENDCOLOR, SDL_GetNumVideoDisplays());
        for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
        {
            SDL_DisplayMode mode = {0};
            if (!OK(SDL_GetCurrentDisplayMode(i, &mode))) //NOTE: SDL_GetDesktopDisplayMode returns previous mode if full screen mode
                err(RED_MSG "Can't get display[%d/%d]" ENDCOLOR, i, SDL_GetNumVideoDisplays());
            else myprintf(22, BLUE_MSG "Display[%d/%d]: %d x %d px @%dHz, %i bbp %s" ENDCOLOR, i, SDL_GetNumVideoDisplays(), mode.w, mode.h, mode.refresh_rate, SDL_BITSPERPIXEL(mode.format), SDL_PixelFormatShortName(mode.format));
            if (!wh.w || !wh.h) { wh.w = mode.w; wh.h = mode.h; } //take first one, continue (for debug)
//            break; //TODO: take first one or last one?
        }
#endif
    }

#if 0
//set reasonable values if can't get info:
    if (!wh.w || !wh.h)
    {
        /*throw std::runtime_error*/ exc(RED_MSG "Can't get screen size" ENDCOLOR);
        wh.w = 1536;
        wh.h = wh.w * 3 / 4; //4:3 aspect ratio
        myprintf(22, YELLOW_MSG "Using dummy display mode %dx%d" ENDCOLOR, wh.w, wh.h);
    }
#endif
    return wh;
}


#endif //ndef _RPI_HELPERS_H



///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include "msgcolors.h"
#include "debugexc.h"


//int main(int argc, const char* argv[])
void unit_test()
{
    debug(CYAN_MSG "is RPi? %d" ENDCOLOR, isRPi());
    WH wh = GetScreenConfig();
    debug(BLUE_MSG "screen %d x %d" ENDCOLOR, wh.w, wh.h);
//    return 0;
}

#endif //def WANT_UNIT_TEST
