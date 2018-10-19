//TODO: does SDL_Init() call bcm_host_init() on RPi to init VC(GPU)?  http://elinux.org/Raspberry_Pi_VideoCore_APIs

////////////////////////////////////////////////////////////////////////////////
////
/// SDL headers, wrappers, etc
//

#ifndef _SDL_HELPERS_H
#define _SDL_HELPERS_H


#include <map>
#include <mutex>
#include <string>
#include <memory> //std::unique_ptr<>
#include <utility> //std::forward<>
#include <SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files

#include "msgcolors.h"
#include "srcline.h"
#include "debugexc.h"


//SDL retval conventions:
//0 == Success, < 0 == error, > 0 == data ptr (sometimes)
#define SDL_Success  0
int SDL_LastError = 0; //mainly for debug
#define SDL_OK(retval)  ((SDL_LastError = (retval)) >= 0)


//readable names (mainly for debug):
const std::map<Uint32, const char*> SDL_SubSystems =
{
    {SDL_INIT_TIMER, "Timer"},
    {SDL_INIT_AUDIO, "Audio"},
    {SDL_INIT_VIDEO, "Video"},
    {SDL_INIT_JOYSTICK, "Joystick"},
    {SDL_INIT_HAPTIC, "Haptic"}, //force feedback subsystem
    {SDL_INIT_GAMECONTROLLER, "Game Controller"},
    {SDL_INIT_EVENTS, "Events"},
};


//SDL_init wrapper class:
//will only init as needed
//defers cleanup until process exit
//thread safe (although SDL may not be)
class SDL_AutoLib
{
public: //ctor/dtor
//    SDL_lib(Uint32 flags) { init(flags); }
//    void init(Uint32 flags = 0, SrcLine srcline = 0)
    SDL_AutoLib(SrcLine srcline = 0): SDL_Lib(0, srcline) {}
    SDL_AutoLib(Uint32 flags = 0, SrcLine srcline = 0)
    {
        debug(BLUE_MSG "SDL_AutoLib: init 0x%x%s" ENDCOLOR_ATLINE(srcline), flags, (flags == SDL_INIT_EVERYTHING)? " (all)": "");
        Uint32 inited = SDL_WasInit(SDL_INIT_EVERYTHING);
        for (Uint32 bit = 1; bit; bit <<= 1) //do one at a time
            if (flags & bit) //caller wants this one
                if (!SDL_SubSystems.count(bit)) exc(RED_MSG "SDL_AutoLib: unknown subsys: 0x%x" ENDCOLOR_ATLINE(srcline)); //throw SDL_Exception("SDL_Init");
                else if (inited & bit) debug(BLUE_MSG "SDL_AutoLib: subsys '%s' (0x%x) already inited" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit);
                else if (!SDL_OK(SDL_InitSubSystem(bit))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
                else
                {
                    debug(CYAN_MSG "SDL_AutoLib: subsys '%s' (0x%x) init[%d] success" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, count());
                    std::lock_guard<std::mutex> guard(mutex());
                    if (!count()++) atexit(cleanup); //SDL_Quit); //defer cleanup in case caller wants more SDL later
                }
    }
    virtual ~SDL_Lib() { debug("SDL_AutoLib dtor" ENDCOLOR); }
//public: //methods
//    void quit()
private: //helpers
//    void first()
//    {
//        m_inierr = SDL_
//    }
    static void cleanup()
    {
        debug(CYAN_MSG "SDL_Lib: cleanup" ENDCOLOR);
        SDL_Quit();
    }
private: //data members
//    static int m_count = 0;
    static int& count() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static int m_count = 0;
        return m_count;
    }
//    static std::mutex m_mutex;
    static std::mutex& mutex() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::mutex m_mutex;
        return m_mutex;
    }
};


//SDL_Surface auto-cleanup ptr wrapper:
class SDL_AutoSurface: public std::unique_ptr<SDL_Surface, std::function<void(SDL_Surface*)>>
{
public: //ctor/dtor
    template <typename ... ARGS>
    SDL_AutoSurface(ARGS&& ... args, SrcLine = 0): super(0, deleter), sdl(SDL_INIT_VIDEO, SRCLINE)
    {
        debug(BLUE_MSG "SDL_AutoSurface ctor" ENDCOLOR);
        reset(SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...)); //perfect fwding to SDL
    }
    virtual ~SDL_AutoSurface() { debug(BLUE_MSG "SDL_AutoSurface dtor" ENDCOLOR); }
private: //members
    SDL_AutoLib sdl;
private: //helpers
    static void deleter(SDL_Surface* ptr)
    {
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        debug(BLUE_MSG "SDL_AutoSurface: free surface 0x%x" ENDCOLOR, ptr);
        if (ptr) SDL_FreeSurface(ptr);
    }
protected:
    using super = std::unique_ptr; //compiler knows template params; https://www.fluentcpp.com/2017/12/26/emulate-super-base/
};
====
https://wiki.libsdl.org/SDL_Surface?highlight=%28%5CbCategoryStruct%5Cb%29%7C%28SDLStructTemplate%29
    /* Once locked, surface->pixels is safe to access. */
    SDL_LockSurface(surface);

    /* This assumes that color value zero is black. Use
       SDL_MapRGBA() for more robust surface color mapping! */
    /* height times pitch is the size of the surface's whole buffer. */
    SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

    SDL_UnlockSurface(surface);
}
====


#endif //ndef _SDL_HELPERS_H


////////////////////////////////////////////////////////////////////////////////
////
/// unit test:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include <iostream> //std::cout
#include "msgcolors.h"
#include "srcline.h"


class aclass
{
    SDL_AutoLib sdl; //(SRCLINE);
public:
    aclass(): sdl(SDL_INIT_VIDEO, SRCLINE) { debug("aclass ctor" ENDCOLOR); }
    ~aclass() { debug("class dtor" ENDCOLOR); }
};

void other(SrcLine srcline = 0)
{
    SDL_AutoLib sdl(SDL_INIT_EVERYTHING, SRCLINE); //SDL_INIT_VIDEO | SDL_INIT_AUDIO>
//    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR;
//    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
    debug("here1" ENDCOLOR);
}


void afunc(SrcLine srcline = 0)
{
    SDL_AutoLib sdl(SDL_INIT_AUDIO, SRCLINE);
//    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR;
//    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
    other();
}

aclass A;

//int main(int argc, const char* argv[])
void unit_test()
{
    debug(BLUE_MSG << "start" << ENDCOLOR);
    afunc();
    aclass B;

//template <int FLAGS = SDL_INIT_VIDEO | SDL_INIT_AUDIO>
    debug(BLUE_MSG << "finish" << ENDCOLOR);
//    return 0;
}

#endif //def WANT_UNIT_TEST