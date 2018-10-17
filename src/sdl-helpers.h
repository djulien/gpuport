////////////////////////////////////////////////////////////////////////////////
////
/// SDL headers, wrappers, etc
//

#ifndef _SDL_HELPERS_H
#define _SDL_HELPERS_H


#include <map>
#include <mutex>
#include <string>
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
const std::map<Uint32, std::string> SDL_SubSystems =
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
class SDL_Lib
{
public: //ctor/dtor
//    SDL_lib(Uint32 flags) { init(flags); }
//    virtual ~SDL_lib() { quit(); }
//public: //methods
//    void init(Uint32 flags = 0, SrcLine srcline = 0)
    SDL_Lib(Uint32 flags = 0, SrcLine srcline = 0)
    {
        debug(BLUE_MSG "SDL_Lib: init 0x%x" ENDCOLOR_ATLINE(srcline), flags);
        Uint32 inited = SDL_WasInit(SDL_INIT_EVERYTHING);
        for (Uint32 bit = 1; bit; bit <<= 1) //do one at a time
            if (flags & bit) //caller wants this one
                if (!SDL_SubSystems.count(bit)) exc(RED_MSG "SDL_Lib: unknown subsys: 0x%x" ENDCOLOR_ATLINE(srcline)); //throw SDL_Exception("SDL_Init");
                else if (inited & bit) debug(BLUE_MSG "SDL_Lib: subsys '%s' (0x%x) already inited" ENDCOLOR_ATLINE(srcline), SDL_SubSystems[bit], bit);
                else if (!SDL_OK(SDL_InitSubSystem(bit))) exc(RED_MSG "SDL_Lib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems[bit], bit, SDL_GetError(), SDL_LastError);
                else
                {
                    debug(CYAN_MSG "SDL_Lib: subsys '%s' (0x%x) init success" ENDCOLOR_ATLINE(srcline), SDL_SubSystems[bit], bit);
                    std::lock_guard<std::mutex> guard(m_mutex);
                    if (!count()++) atexit(SDL_Quit); //defer cleanup in case caller wants more SDL later
                }
    }
//    void quit()
//private: //helpers
//    void first()
//    {
//        m_inierr = SDL_
//    }
private: //data members
//    static int m_count = 0;
    static int& count() //use wrapper to avoid trailing static decl at global scope
    {
        static int m_count = 0;
        return m_count;
    }
    static std::mutex m_mutex;
//    static std::mutex& mutex() //use wrapper to avoid trailing static decl at global scope
//    {
//        static std::mutex m_mutex;
//        return m_mutex;
//    }
};


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
    SDL_Lib sdllib;
public:
    aclass(): sdllib(SDL_INIT_VIDEO) { debug("aclass ctor"); }
    ~aclass() { debug("class dtor"); }
};

void other(SrcLine srcline = 0)
{
    SDL_Lib sdllib(SDL_INIT_EVERYTHING); //SDL_INIT_VIDEO | SDL_INIT_AUDIO>
//    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR;
//    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
    printf("here1\n");
}


void afunc(SrcLine srcline = 0)
{
    SDL_Lib sdllib(SDL_INIT_AUDIO);
//    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR;
//    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
    other();
}

aclass A;

//int main(int argc, const char* argv[])
void unit_test()
{
    afunc();
    aclass B;

//template <int FLAGS = SDL_INIT_VIDEO | SDL_INIT_AUDIO>
    std::cout << BLUE_MSG << "start" << ENDCOLOR;
//    return 0;
}

#endif //def WANT_UNIT_TEST