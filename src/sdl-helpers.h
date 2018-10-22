#ifndef _SDL_HELPERS_H
#define _SDL_HELPERS_H


#include <map>
#include <mutex>
#include <string>
#include <memory> //std::unique_ptr<>
#include <utility> //std::forward<>
#include <type_traits> //std::conditional, std::enable_if, std::is_same, std::disjunction, etc
#include <climits> //<limits.h> //*_MIN, *_MAX

#include "msgcolors.h" //*_MSG, ENDCOLOR, ENDCOLOR_ATLINE()
#include "srcline.h" //SrcLine, SRCLINE
#include "debugexc.h" //debug(), exc()
#include "str-helpers.h" //commas()
#include "rpi-helpers.h" //isrpi()
#include "ostrfmt.h" //FMT()


////////////////////////////////////////////////////////////////////////////////
////
/// generic macros:
//

//scale factors (for readability):
//CAUTION: may need () depending on surroundings
#define sec  *1000
#define msec  *1

//other readability defs:
#define VOID
#define CONST

#ifndef clamp
 #define clamp(val, min, max)  ((val) < (min)? (min): (val) > (max)? (max): (val))
#endif
//#define clip_255(val)  ((val) & 0xFF)

//inline int toint(void* val) { return (int)(long)val; }
//#define toint(ptr)  reinterpret_cast<int>(ptr) //gives "loses precision" warning/error
#ifndef toint
 #define toint(expr)  (int)(long)(expr)
#endif

#ifndef SIZEOF
 #define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))
#endif


//int/ gives floor, use this to give ceiling:
//safe to use with MAX_INT
#ifndef divup
 #define divup(num, den)  ((((num) - 1) / (den)) + 1) //(((num) + (den) - 1) / (den)) //((den)? (((num) + (den) - 1) / (den)): -1)
#endif


//get bit-size of a value *at compile time*:
//based on https://hbfs.wordpress.com/2016/03/22/log2-with-c-metaprogramming/
//#if 0 //no worky on RPi
// constexpr size_t log2(unsigned n) { return (n < 2)? 0: (1 + log2(divup(n, 2))); }
// constexpr size_t log10(unsigned n) { return divup(log2(n), log2(10)); }
//#else
// template <int x>
// struct log2 { enum { value = 1 + log2<x/2>::value }; };
// template <> struct log2<1> { enum { value = 1 }; };
//#endif
namespace CompileTime //keep separate from run-time functions in math.h
{
    constexpr unsigned log2_floor(unsigned x) { return (x < 2)? 0: (1 + log2_floor(x >> 1)); }
    constexpr unsigned log2_ceiling(unsigned x) { return (x < 2)? 0: log2_floor(x - 1) + 1; }
    constexpr unsigned log10_ceiling(unsigned x) { return divup(log2_ceiling(x), log2_floor(10)); } //overcompensate (for safety with buf sizes)
//#define log2  log2_ceiling
//#define log10  log10_ceiling
    constexpr unsigned log2(unsigned x) { return log2_ceiling(x); }
    constexpr unsigned log10(unsigned x) { return log10_ceiling(x); }
};


//lookup table (typically int -> string):
//use this function instead of operator[] on const maps (operator[] is not const)
template <typename MAP, typename VAL> //... ARGS>
const char* unmap(MAP&& map, VAL&& value) //Uint32 value) //ARGS&& ... args)
{
    return map.count(value)? map.find(value)->second: "";
}
//const std::map<Uint32, const char*> SDL_SubSystems =
//    SDL_AutoSurface SDL_CreateRGBSurfaceWithFormat(ARGS&& ... args, SrcLine srcline = 0) //UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
//        return SDL_AutoSurface(::SDL_CreateRGBSurfaceWithFormat(std::forward<ARGS>(args) ...), srcline); //perfect fwd


//skip over first part of string if it matches:
inline const char* skip_prefix(const char* str, const char* prefix)
{
//    size_t preflen = strlen(prefix);
//    return (str && !strncmp(str, prefix, preflen))? str + preflen: str;
    if (str)
        while (*str == *prefix++) ++str;
    return str;
}


//define lamba function for named args:
#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ /*&*/ [&](auto& _)
#endif

struct Unpacked {}; //ctor disambiguation tag
template <typename UNPACKED, typename CALLBACK>
static UNPACKED& unpack(UNPACKED& params, CALLBACK&& named_params)
{
//        static struct CtorParams params; //need "static" to preserve address after return
//        struct CtorParams params; //reinit each time; comment out for sticky defaults
//        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
    auto thunk = [](auto get_params, UNPACKED& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
//        MSG(BLUE_MSG << "get params ..." << ENDCOLOR);
    thunk(named_params, params);
//        MSG(BLUE_MSG << "... got params" << ENDCOLOR);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
    debug(BLUE_MSG << "unpack ret" << ENDCOLOR);
    return params;
}


////////////////////////////////////////////////////////////////////////////////
////
/// SDL headers, wrappers, etc
//

//helpful SDL info:
//https://wiki.libsdl.org/MigrationGuide#If_your_game_just_wants_to_get_fully-rendered_frames_to_the_screen


#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
#include <SDL_endian.h> //SDL_BYTE_ORDER

//friendlier names for SDL special param values:
//#define UNUSED  0
#define DONT_CARE  0
#define NO_RECT  NULL
#define NO_PARENT  NULL
//#define FIRST_MATCH  -1
//#define THIS_THREAD  NULL
//#define UNDEF_EVTID  0


//SDL retval conventions:
//0 == Success, < 0 == error, > 0 == data ptr (sometimes)
#define SDL_Success  0
#define SDL_GenericError  -2 //arbitrary; anything < 0
int SDL_LastError = SDL_Success; //mainly for debug msgs
//use overloaded function to handle different SDL retval types:
//#define SDL_OK(retval)  ((SDL_LastError = (retval)) >= 0)
inline bool SDL_OK(int errcode) { return ((SDL_LastError = errcode) >= 0); }
inline bool SDL_OK(Uint32 retval, Uint32 badval) { return ((SDL_LastError = retval) != badval); }
inline bool SDL_OK(SDL_bool ok, const char* why) { return SDL_OK((ok == SDL_TRUE)? SDL_Success: SDL_SetError(why)); }
inline bool SDL_OK(void* ptr) { return SDL_OK(ptr? SDL_Success: SDL_GenericError); } //SDL error text already set; just use dummy value for err code
//inline bool SDL_OK(void dummy) { return true; }
//#define SDL_OK_2ARGS(retval, okval)  ((retval) == (okval))?


#ifndef USE_ARG4
 #define USE_ARG4(one, two, three, four, ...)  four
#endif

//report SDL error (throw exc):
//use optional param to show msg instead of throw exc
//#define SDL_exc(what_failed)  exc(RED_MSG what_failed " failed: %s (error %d)" ENDCOLOR, SDL_GetError(), SDL_LastError)
#define SDL_errmsg(handler, what_failed, srcline)  handler(RED_MSG what_failed " failed: %s (error %d)" ENDCOLOR_ATLINE(srcline), SDL_GetError(), SDL_LastError)
#define SDL_exc_1ARG(what_failed)  SDL_errmsg(exc, what_failed, 0)
#define SDL_exc_2ARGS(what_failed, want_throw)  ((want_throw)? SDL_errmsg(exc, what_failed, 0): SDL_errmsg(debug, what_failed, 0))
#define SDL_exc_3ARGS(what_failed, want_throw, srcline)  ((want_throw)? SDL_errmsg(exc, what_failed, srcline): SDL_errmsg(debug, what_failed, srcline))
#define SDL_exc(...)  USE_ARG4(__VA_ARGS__, SDL_exc_3ARGS, SDL_exc_2ARGS, SDL_exc_1ARG) (__VA_ARGS__)


//reduce verbosity:
#define SDL_PixelFormatShortName(fmt)  skip_prefix(SDL_GetPixelFormatName(fmt), "SDL_PIXELFORMAT_")
#define SDL_TickFreq()  SDL_GetPerformanceFrequency()
#define SDL_Ticks()  SDL_GetPerformanceCounter()

//timing stats:
inline uint64_t now() { return SDL_Ticks(); }
inline double elapsed(uint64_t started, int scaled = 1) { return (double)(now() - started) * scaled / SDL_TickFreq(); } //Freq = #ticks/second
//inline double elapsed_usec(uint64_t started)
//{
////    static uint64_t tick_per_usec = SDL_TickFreq() / 1000000;
//    return (double)(now() - started) * 1000000 / SDL_TickFreq(); //Freq = #ticks/second
//}


////////////////////////////////////////////////////////////////////////////////
////
/// (A)RGB color defs
//

//(A)RGB primary colors:
//NOTE: const format below is processor-independent (hard-coded for ARGB msb..lsb)
//use macros below that to adjust in-memory representation based on processor endianness (RGBA vs. ABGR)
//#pragma message("Compiled for ARGB color format (hard-coded)")
#define RED  0xFFFF0000 //fromRGB(255, 0, 0) //0xFFFF0000
#define GREEN  0xFF00FF00 //fromRGB(0, 255, 0) //0xFF00FF00
#define BLUE  0xFF0000FF //fromRGB(0, 0, 255) //0xFF0000FF
#define YELLOW  (RED | GREEN) //0xFFFFFF00
#define CYAN  (GREEN | BLUE) //0xFF00FFFF
#define MAGENTA  (RED | BLUE) //0xFFFF00FF
#define PINK  MAGENTA //easier to spell :)
#define BLACK  (RED & GREEN & BLUE) //0xFF000000 //NOTE: needs Alpha
#define WHITE  (RED | GREEN | BLUE) //fromRGB(255, 255, 255) //0xFFFFFFFF
//other ARGB colors (debug):
//#define SALMON  0xFF8080
//#define LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA


//set in-memory byte order according to architecture of host processor:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN //Intel (PCs)
 #pragma message("SDL big endian")
 #define Rmask  0xFF000000
 #define Gmask  0x00FF0000
 #define Bmask  0x0000FF00
 #define Amask  0x000000FF
//#define Abits(a)  (clamp(a, 0, 0xFF) << 24)
//#define Rbits(a)  (clamp(r, 0, 0xFF) << 16)
//#define Gbits(a)  (clamp(g, 0, 0xFF) << 8)
//#define Bbits(a)  clamp(b, 0, 0xFF)
// #define Amask(color)  ((color) & 0xFF000000)
// #define Rmask(color)  ((color) & 0x00FF0000)
// #define Gmask(color)  ((color) & 0x0000FF00)
// #define Bmask(color)  ((color) & 0x000000FF)
// #define A(color)  (((color) >> 24) & 0xFF)
// #define R(color)  (((color) >> 16) & 0xFF)
// #define G(color)  (((color) >> 8) & 0xFF)
// #define B(color)  ((color) & 0xFF)
// #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))

#elif SDL_BYTEORDER == SDL_LIL_ENDIAN //ARM (RPi)
// #pragma message("SDL li'l endian")
 #define Rmask  0x000000FF
 #define Gmask  0x0000FF00
 #define Bmask  0x00FF0000
 #define Amask  0xFF000000
// #define Amask(color)  ((color) & 0xFF000000)
// #define Rmask(color)  ((color) & 0x00FF0000)
// #define Gmask(color)  ((color) & 0x0000FF00)
// #define Bmask(color)  ((color) & 0x000000FF)
// #define A(color)  (((color) >> 24) & 0xFF)
// #define R(color)  (((color) >> 16) & 0xFF)
// #define G(color)  (((color) >> 8) & 0xFF)
// #define B(color)  ((color) & 0xFF)
// #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))

#else
 #error message("Unknown SDL endian")
#endif

//color manipulation:
//NOTE: these are mainly meant for use with compile-time consts (arithmetic performed once at compile time)
//could be used with vars also (hopefully compiler optimizes using byte instructions)
#define Rshift  (Rmask / 0xFF)
#define Gshift  (Gmask / 0xFF)
#define Bshift  (Bmask / 0xFF)
#define Ashift  (Amask / 0xFF)
#define fromRGB(r, g, b)  ((255 * Ashift) | (clamp(r, 0, 255) * Rshift) | (clamp(g, 0, 255) * Gshift) | (clamp(b, 0, 255) * Bshift))
#define fromARGB(a, r, g, b)  ((clamp(a, 0, 255) * Ashift) | (clamp(r, 0, 255) * Rshift) | (clamp(g, 0, 255) * Gshift) | (clamp(b, 0, 255) * Bshift))

#define A(color)  (((color) / Ashift) & 0xFF)
#define R(color)  (((color) / Rshift) & 0xFF)
#define G(color)  (((color) / Gshift) & 0xFF)
#define B(color)  (((color) / Bshift) & 0xFF)

#define Abits(color)  ((color) & Amask)
#define Rbits(color)  ((color) & Rmask)
#define Gbits(color)  ((color) & Gmask)
#define Bbits(color)  ((color) & Bmask)

#define R_G_B(color)  R(color), G(color), B(color)
#define A_R_G_B(color)  A(color), R(color), G(color), B(color)
#define R_G_B_A(color)  R(color), G(color), B(color), A(color)
#define B_G_R(color)  B(color), G(color), R(color)
#define A_B_G_R(color)  A(color), B(color), G(color), R(color)
#define B_G_R_A(color)  B(color), G(color), R(color), A(color)

//#define R_G_B_A_masks(color)  Rmask(color), Gmask(color), Bmask(color), Amask(color)

//convert color ARGB <-> ABGR format:
//OpenGL seems to prefer ABGR format, but ARGB order is more readable (for me)
//convert back with same function & 0xffffff
//TODO: drop alpha setting?
//??	if (!Amask(color) /*&& (color & 0xffffff)*/) color |= 0xff000000; //RGB present but no alpha; add full alpha to force color to show
#define ARGB2ABGR(color)  ((color) & (Amask | Gmask) | (R(color) * Bshift) | (B(color) * Rshift)) //swap R <-> B
//#define SWAP32(uint32)  ((Amask(uint32) >> 24) | (Rmask(uint32) >> 8) | (Gmask(uint32) << 8) | (Bmask(uint32) << 24))


//const uint32_t PALETTE[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};

//readable names (mainly for debug msgs):
const std::map<Uint32, const char*> ColorNames =
{
    {RED, "Red"},
    {GREEN, "Green"},
    {BLUE, "Blue"},
    {YELLOW, "Yellow"},
    {CYAN, "Cyan"},
    {MAGENTA, "Pink/Magenta"},
    {BLACK, "Black"},
    {WHITE, "White"},
};


////////////////////////////////////////////////////////////////////////////////
////
/// SDL lib init/quit:
//


//readable names (mainly for debug msgs):
const std::map<Uint32, const char*> SDL_SubSystems =
{
    {SDL_INIT_TIMER, "Timer"},
    {SDL_INIT_AUDIO, "Audio"},
    {SDL_INIT_VIDEO, "Video"},
    {SDL_INIT_JOYSTICK, "Joystick"},
    {SDL_INIT_HAPTIC, "Haptic"}, //force feedback subsystem
    {SDL_INIT_GAMECONTROLLER, "Game Controller"},
    {SDL_INIT_EVENTS, "Events"},
    {SDL_INIT_EVERYTHING, "all"},
};


//SDL_HINT_RENDER_SCALE_QUALITY wrapper:
//use compiler to force correct values
//#define RENDER_SCALE_QUALITY_NEAREST  0 //"nearest"; //nearest pixel sampling
//#define RENDER_SCALE_QUALITY_LINEAR  1 //"linear"; //linear filtering (supported by OpenGL and Direct3D)
//#define RENDER_SCALE_QUALITY_BEST  2 //"best"; //anisotropic filtering (supported by Direct3D)
enum SDL_HINT_RENDER_SCALE_QUALITY_choices: unsigned
{
    Nearest = 0, //"nearest"; //nearest pixel sampling
    Linear = 1, //"linear"; //linear filtering (supported by OpenGL and Direct3D)
    Best = 2, //"best"; //anisotropic filtering (supported by Direct3D)
};
inline int SDL_SetRenderScaleQuality(SDL_HINT_RENDER_SCALE_QUALITY_choices value)
{
//debug(BLUE_MSG "max uint %zu %zx, max uint / 2 %zu, %zx, 3 / 2 %zu, 9 / 4 %zu, 7 / 4 %zu" ENDCOLOR, UINT_MAX, UINT_MAX, UINT_MAX / 2, UINT_MAX / 2, 3 / 2, 9 / 4, 7 / 4);
//debug(BLUE_MSG "log2_floor(10) = %u, log2_ceiling(10) = %u, log2(64) = %u, log10(64) = %u, log10(%u) = %u, log2(1K) = %u, log10(1K) = %u" ENDCOLOR, log2_floor(10), log2_ceiling(10), log2(64), log10(64), UINT_MAX, log10(UINT_MAX), log2(1024), log10(1024));
    char buf[CompileTime::log10(UINT_MAX) + 1]; //one extra char for null terminator
    sprintf(buf, "%zu", value);
    return (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, buf) == SDL_TRUE)? SDL_Success: SDL_GenericError; //SDL_SetHint already called SDL_SetError() so just return a dummy error#
}


std::string& rect_desc(const SDL_Rect* rect)
{
    std::ostringstream ss;
    if (!rect) ss << "all";
    else ss << rect->w * rect->h << " ([" << rect->x << ", " << rect->y << "]..[+" << rect->w << ", +" << rect->h << "])";
    return ss.str();
}


//SDL_init wrapper class:
//will only init as needed
//defers cleanup until process exit
//thread safe (although SDL may not be)
class SDL_AutoLib
{
public: //ctor/dtor
//    SDL_lib(Uint32 flags) { init(flags); }
//    void init(Uint32 flags = 0, SrcLine srcline = 0)
    explicit SDL_AutoLib(SrcLine srcline = 0): SDL_AutoLib(0, srcline) {}
    explicit SDL_AutoLib(Uint32 flags /*= 0*/, SrcLine srcline = 0): m_srcline(srcline)
    {
//        std::lock_guard<std::mutex> guard(mutex()); //only allow one thread to init at a time
        debug(BLUE_MSG "SDL_AutoLib ctor: init 0x%x (%s)" ENDCOLOR_ATLINE(srcline), flags, unmap(SDL_SubSystems, flags)); //SDL_SubSystems.count(flags)? SDL_SubSystems.find(flags)->second: "");
        Uint32 inited = SDL_WasInit(SDL_INIT_EVERYTHING);
        for (Uint32 bit = 1; bit; bit <<= 1) //do one at a time
            if (flags & bit) //caller wants this one
                if (!SDL_SubSystems.count(bit)) exc(RED_MSG "SDL_AutoLib: unknown subsys: 0x%ux" ENDCOLOR_ATLINE(srcline)); //throw SDL_Exception("SDL_Init");
                else if (inited & bit) debug(BLUE_MSG "SDL_AutoLib: subsys '%s' (0x%ux) already inited" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit);
                else if (!SDL_OK(SDL_InitSubSystem(bit))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%ux) failed: %s (err 0x%d)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
                else
                {
                    std::lock_guard<std::mutex> guard(mutex()); //only allow one thread to init or get count at a time
                    debug(CYAN_MSG "SDL_AutoLib: subsys '%s' (0x%ux) init[%d] success" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, count());
//                    std::lock_guard<std::mutex> guard(mutex());
                    if (!count()++) first(srcline);
                }
    }
    virtual ~SDL_AutoLib() { debug("SDL_AutoLib dtor" ENDCOLOR_ATLINE(m_srcline)); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const SDL_AutoLib& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
        SDL_version ver;
        SDL_GetVersion(&ver);
//        ostrm << "SDL_Lib {version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d}", ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
        ostrm << "SDL_Lib {version " << ver.major << "." << ver.minor << "." << ver.patch << ", ";
        ostrm << FMT("platform: '%s', ") << SDL_GetPlatform();
        ostrm << FMT("#cores %d, ") << SDL_GetCPUCount();
//std::thread::hardware_concurrency()
        ostrm << FMT("ram %s MB, ") << commas(SDL_GetSystemRAM());
        ostrm << FMT("likely isRPi? %d, ") << isRPi();
//        debug_level(12, BLUE_MSG "%d video driver(s):" ENDCOLOR_ATLINE(srcline), SDL_GetNumVideoDrivers());
        ostrm << FMT("%d video driver(s):") << SDL_GetNumVideoDrivers();
        for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
//            debug_level(12, BLUE_MSG "Video driver[%d/%d]: name '%s'" ENDCOLOR_ATLINE(srcline), i, SDL_GetNumVideoDrivers(), SDL_GetVideoDriver(i));
            ostrm << ","[!i] << " [" << i << "/" << SDL_GetNumVideoDrivers() << FMT("]: '%s'") << SDL_GetVideoDriver(i);
        ostrm << "}";
        return ostrm;
    }
//public: //methods
//    void quit()
private: //helpers
    static void first(SrcLine srcline = 0)
    {
//NOTE: SDL_Init() seems to call bcm_host_init() on RPi to init VC(GPU) (or else it's no longer needed);  http://elinux.org/Raspberry_Pi_VideoCore_APIs
        if (!SDL_OK(SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing file", "File is missing. Please reinstall the program.", NO_PARENT))) SDL_exc("simple msg box", false);
//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen:
//no!        if (!SDL_OK(SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"), "SDL_SetHint")) SDL_exc("set hint");  // make the scaled rendering look smoother
        if (!SDL_OK(SDL_SetRenderScaleQuality(Nearest))) SDL_exc("Linear render scale quality");
        atexit(cleanup); //SDL_Quit); //defer cleanup in case caller wants more SDL later
        inspect(srcline);
    }
    static void cleanup()
    {
        debug(CYAN_MSG "SDL_Lib: cleanup" ENDCOLOR);
        SDL_Quit();
    }
    static void inspect(SrcLine srcline = 0)
    {
//        SDL_version ver;
//        SDL_GetVersion(&ver);
//        debug_level(12, BLUE_MSG "SDL version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d" ENDCOLOR_ATLINE(srcline), ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//        debug_level(12, BLUE_MSG "%d video driver(s):" ENDCOLOR_ATLINE(srcline), SDL_GetNumVideoDrivers());
//        for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
//            debug_level(12, BLUE_MSG "Video driver[%d/%d]: name '%s'" ENDCOLOR_ATLINE(srcline), i, SDL_GetNumVideoDrivers(), SDL_GetVideoDriver(i));
        SDL_AutoLib dummy;
        debug_level(12, BLUE_MSG << dummy << ENDCOLOR_ATLINE(srcline));
    }
private: //data members
//    static int m_count = 0;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
    static /*std::atomic<int>*/int& count() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static /*std::atomic<int>*/int m_count = 0;
        return m_count;
    }
//    static std::mutex m_mutex;
    static std::mutex& mutex() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::mutex m_mutex;
        return m_mutex;
    }
};


////////////////////////////////////////////////////////////////////////////////
////
/// SDL Surface:
//

#if 0 //not needed; use in-memory uint32 array instead
//SDL_Surface ptr auto-cleanup wrapper:
//includes a few factory helper methods
template <bool AutoLock = true, bool DebugInfo = true>
class SDL_AutoSurface: public std::unique_ptr<SDL_Surface, std::function<void(SDL_Surface*)>>
{
public: //ctors/dtors
    SDL_AutoSurface(SrcLine srcline = 0): super(0, deleter), m_srcline(srcline) {} //no surface
//    template <typename ... ARGS>
//    SDL_AutoSurface(ARGS&& ... args, SrcLine = 0): super(0, deleter), sdllib(SDL_INIT_VIDEO, SRCLINE)
    SDL_AutoSurface(SDL_Surface* ptr, SrcLine srcline = 0): SDL_AutoSurface(srcline) //, sdllib(SDL_INIT_VIDEO, SRCLINE)
    {
        debug(BLUE_MSG "SDL_AutoSurface ctor 0x%p" ENDCOLOR_ATLINE(srcline), ptr);
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(ptr)) exc(RED_MSG "SDL_AutoSurface: init surface failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_GetError(), SDL_LastError); //required surface
        reset(ptr, srcline); //take ownership of surface
    }
    virtual ~SDL_AutoSurface() { debug(BLUE_MSG "SDL_AutoSurface dtor 0x%p" ENDCOLOR_ATLINE(m_srcline), get()); }
public: //factory methods:
    template <typename ... ARGS>
    SDL_AutoSurface SDL_CreateRGBSurfaceWithFormat(ARGS&& ... args, SrcLine srcline = 0) //UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
    {
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating surface
        return SDL_AutoSurface(::SDL_CreateRGBSurfaceWithFormat(std::forward<ARGS>(args) ...), srcline); //perfect fwd
    }
//obsolete: see https://wiki.libsdl.org/MigrationGuide#If_your_game_just_wants_to_get_fully-rendered_frames_to_the_screen
//    template <typename ... ARGS>
//    SDL_AutoSurface SDL_SetVideoMode(ARGS&& ... args, SrcLine srcline = 0) //image->w, image->h, videoInfo->vfmt->BitsPerPixel, SDL_HWSURFACE);
//    {
//        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating surface
//        return SDL_AutoSurface(::SDL_SetVideoMode(std::forward<ARGS>(args) ...), srcline); //perfect fwd
//    }
public: //operators
    SDL_AutoSurface& operator=(SDL_Surface* ptr) //, SrcLine srcline = 0)
    {
//        if (!srcline) srcline = m_srcline;
        SrcLine srcline = m_srcline; //TODO: where to get this?
        debug(BLUE_MSG "SDL_AutoSurface: old surface 0x%p, new surface 0x%p" ENDCOLOR_ATLINE(srcline), get(), ptr);
        reset(ptr, srcline);
        return *this; //fluent/chainable
    }
public: //methods
    void reset(SDL_Surface* new_ptr, SrcLine srcline = 0)
    {
        if (new_ptr == get()) return; //nothing changed
        if (new_ptr) check(new_ptr, srcline); //validate before acquiring new ptr
        if (new_ptr) inspect(new_ptr, srcline);
        unlock(get(), srcline); //unlock current ptr before acquiring new ptr
        super::reset(new_ptr);
        lock(get(), srcline); //lock after acquiring new ptr
    }
public: //static helper methods
    static void check(SDL_Surface* surf, SrcLine srcline = 0) { check(surf, 0, 0, srcline); }
    static void check(SDL_Surface* surf, int w = 0, int h = 0, SrcLine srcline = 0)
    {
//    if (want_fmts && (numfmt != want_fmts)) err(RED_MSG "Unexpected #formats: %d (wanted %d)" ENDCOLOR, numfmt, want_fmts);
        if ((w && (surf->w != w)) || (h && (surf->h != h))) exc(RED_MSG "Surface wrong size: got %d x %d, wanted %d x %d" ENDCOLOR_ATLINE(srcline), surf->w, surf->h, w, h);
        if (!surf->pixels || (toint(surf->pixels) & 7)) exc(RED_MSG "Surface pixels not aligned on 8-byte boundary: 0x%p" ENDCOLOR_ATLINE(srcline), surf->pixels);
        if ((size_t)surf->pitch != sizeof(uint32_t) * surf->w) exc(RED_MSG "Surface unexpected pitch: %d should be %zu * %d = %zu" ENDCOLOR_ATLINE(srcline), surf->pitch, sizeof(uint32_t), surf->w, sizeof(uint32_t) * surf->w);
    }
    static void lock(SDL_Surface* ptr, SrcLine srcline = 0) {} //noop
    template <bool AutoLock_copy = AutoLock> //for function specialization
    std::enable_if<AutoLock_copy, static void> lock(SDL_Surface* ptr, SrcLine srcline = 0) //SFINAE
    {
//        SDL_Surface* ptr = get(); //non-static method: use current ptr
        if (ptr && SDL_MUSTLOCK(ptr))
            if (!SDL_OK(SDL_LockSurface(ptr)))
                exc(RED_MSG "SDL_AutoSurface: lock failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_GetError(), SDL_LastError);
    }
    static void unlock(SDL_Surface* ptr, SrcLine srcline = 0) {} //noop
    template <bool AutoLock_copy = AutoLock> //for function specialization
    std::enable_if<AutoLock_copy, static void> unlock(SDL_Surface* ptr, SrcLine srcline = 0) //SFINAE
    {
//        SDL_Surface* ptr = get(); //non-static method: use current ptr
        if (ptr && SDL_MUSTLOCK(ptr)) SDL_UnlockSurface(ptr);
    }
    static void inspect(SDL_Surface* ptr, SrcLine srcline = 0) {} //noop
    template <bool DebugInfo_copy = DebugInfo> //for function specialization
    std::enable_if<DebugInfo_copy, static void> inspect(SDL_Surface* ptr, SrcLine srcline = 0) //SFINAE
    static void inspect(SDL_Surface* surf, SrcLine srcline = 0)
    {
        int numfmt = 0;
        std::ostringstream fmts, count;
        for (SDL_PixelFormat* fmtptr = surf->format; fmtptr; fmtptr = fmtptr->next, ++numfmt)
            fmts << ";" << SDL_BITSPERPIXEL(fmtptr->format) << " bpp " << SDL_PixelFormatShortName(fmtptr->format);
        if (!numfmt) { count << "no fmts"; fmts << ";"; }
        else if (numfmt != 1) count << numfmt << " fmts: ";
        debug(18, BLUE_MSG "Surface 0x%p: %d x %d, pitch %s, size %s, must lock? %d, %s%s" ENDCOLOR_ATLINE(srcline), surf, surf->w, surf->h, commas(surf->pitch), commas(surf->h * surf->pitch), SDL_MUSTLOCK(surf), count.str().c_str(), fmts.str().c_str() + 1);
    }
//private: //static helpers
    static void deleter(SDL_Surface* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        debug(BLUE_MSG "SDL_AutoSurface: free surface 0x%p" ENDCOLOR, ptr);
        /*if (ptr)*/ SDL_FreeSurface(ptr);
    }
private: //members
    SDL_AutoLib sdllib;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
protected:
    using super = std::unique_ptr; //no C++ built-in base class (due to multiple inheritance), so define one; compiler already knows template params so they don't need to be repeated here :); https://www.fluentcpp.com/2017/12/26/emulate-super-base/
};
#endif


#if 0
//in-memory SDL surface:
//SDL2 allows any old chunk of memory to be used
//template <bool AutoLock = true, bool DebugInfo = true>
class SDL_MemorSurface: public std::unique_ptr<SDL_Surface, std::function<void(SDL_Surface*)>>
{
public: //ctors/dtors
    SDL_MemorySurface(SrcLine srcline = 0): super(0, deleter), m_srcline(srcline) {} //no surface
//    template <typename ... ARGS>
//    SDL_AutoSurface(ARGS&& ... args, SrcLine = 0): super(0, deleter), sdllib(SDL_INIT_VIDEO, SRCLINE)
    SDL_MemorySurface(SDL_Surface* ptr, SrcLine srcline = 0): SDL_MemorySurface(srcline) //, sdllib(SDL_INIT_VIDEO, SRCLINE)
    {
        debug(BLUE_MSG "SDL_MemorySurface ctor 0x%p" ENDCOLOR_ATLINE(srcline), ptr);
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(ptr)) exc(RED_MSG "SDL_MemorySurface: init surface failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_GetError(), SDL_LastError); //required surface
        reset(ptr, srcline); //take ownership of surface
    }
    virtual ~SDL_MemorySurface() { debug(BLUE_MSG "SDL_MemorySurface dtor 0x%p" ENDCOLOR_ATLINE(m_srcline), get()); }
public: //factory methods:
    template <typename ... ARGS>
    SDL_MemorySurface alloc(int w, int h, SrcLine srcline = 0)
    {
//        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating surface
        return SDL_MemorySurface(malloc(sizeof(SDL_Surface) + w * h * sizeof(Uint32)), srcline); //perfect fwd
    }
public: //operators
    SDL_MemorySurface& operator=(SDL_Surface* ptr) //, SrcLine srcline = 0)
    {
//        if (!srcline) srcline = m_srcline;
        SrcLine srcline = m_srcline; //TODO: where to get this?
        debug(BLUE_MSG "SDL_MemorySurface: old surface 0x%p, new surface 0x%p" ENDCOLOR_ATLINE(srcline), get(), ptr);
        reset(ptr, srcline);
        return *this; //fluent/chainable
    }
public: //methods
    void reset(SDL_Surface* new_ptr, SrcLine srcline = 0)
    {
        if (new_ptr == get()) return; //nothing changed
        if (new_ptr) check(new_ptr, srcline); //validate before acquiring new ptr
        if (new_ptr) inspect(new_ptr, srcline);
        unlock(get(), srcline); //unlock current ptr before acquiring new ptr
        super::reset(new_ptr);
        lock(get(), srcline); //lock after acquiring new ptr
    }
public: //static helper methods
    static void check(SDL_Surface* surf, SrcLine srcline = 0) { check(surf, 0, 0, srcline); }
    static void check(SDL_Surface* surf, int w = 0, int h = 0, SrcLine srcline = 0)
    {
//    if (want_fmts && (numfmt != want_fmts)) err(RED_MSG "Unexpected #formats: %d (wanted %d)" ENDCOLOR, numfmt, want_fmts);
        if ((w && (surf->w != w)) || (h && (surf->h != h))) exc(RED_MSG "Surface wrong size: got %d x %d, wanted %d x %d" ENDCOLOR_ATLINE(srcline), surf->w, surf->h, w, h);
        if (!surf->pixels || (toint(surf->pixels) & 7)) exc(RED_MSG "Surface pixels not aligned on 8-byte boundary: 0x%p" ENDCOLOR_ATLINE(srcline), surf->pixels);
        if ((size_t)surf->pitch != sizeof(uint32_t) * surf->w) exc(RED_MSG "Surface unexpected pitch: %d should be %zu * %d = %zu" ENDCOLOR_ATLINE(srcline), surf->pitch, sizeof(uint32_t), surf->w, sizeof(uint32_t) * surf->w);
    }
    static void lock(SDL_Surface* ptr, SrcLine srcline = 0) {} //noop
    static void unlock(SDL_Surface* ptr, SrcLine srcline = 0) {} //noop
    static void inspect(SDL_Surface* ptr, SrcLine srcline = 0) {} //noop
    template <bool DebugInfo_copy = DebugInfo> //for function specialization
    std::enable_if<DebugInfo_copy, static void> inspect(SDL_Surface* ptr, SrcLine srcline = 0) //SFINAE
    static void inspect(SDL_Surface* surf, SrcLine srcline = 0)
    {
        int numfmt = 0;
        std::ostringstream fmts, count;
        for (SDL_PixelFormat* fmtptr = surf->format; fmtptr; fmtptr = fmtptr->next, ++numfmt)
            fmts << ";" << SDL_BITSPERPIXEL(fmtptr->format) << " bpp " << SDL_PixelFormatShortName(fmtptr->format);
        if (!numfmt) { count << "no fmts"; fmts << ";"; }
        else if (numfmt != 1) count << numfmt << " fmts: ";
        debug(18, BLUE_MSG "Surface 0x%p: %d x %d, pitch %s, size %s, must lock? %d, %s%s" ENDCOLOR_ATLINE(srcline), surf, surf->w, surf->h, commas(surf->pitch), commas(surf->h * surf->pitch), SDL_MUSTLOCK(surf), count.str().c_str(), fmts.str().c_str() + 1);
    }
//private: //static helpers
    static void deleter(SDL_Surface* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        debug(BLUE_MSG "SDL_MemorySurface: free surface 0x%p" ENDCOLOR, ptr);
        /*if (ptr)*/ free(ptr);
    }
private: //members
//    SDL_AutoLib sdllib;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
protected:
    using super = std::unique_ptr; //no C++ built-in base class (due to multiple inheritance), so define one; compiler already knows template params so they don't need to be repeated here :); https://www.fluentcpp.com/2017/12/26/emulate-super-base/
};
#endif


/*
//surface->pixels safe to access when surface locked
class SDL_LockedSurface: public SDL_Surface*
{
public: //ctor/dtor:
    SDL_LockedSurface(SDL_Surface* ptr = 0, SrcLine srcline = 0): super(ptr)
    {
        SDL_LockSurface(surface);
//https://wiki.libsdl.org/SDL_Surface?highlight=%28%5CbCategoryStruct%5Cb%29%7C%28SDLStructTemplate%29
//       SDL_MapRGBA() for more robust surface color mapping!
//height times pitch is the size of the surface's whole buffer.
//SDL_memset(surface->pixels, 0, surface->h * surface->pitch);
    }
    virtual ~SDL_LockedSurface()
    {
        SDL_UnlockSurface(surface);
    }
protected:
//    SDL_Surface m_ptr;
    using super = SDL_Surface*;
};
*/


////////////////////////////////////////////////////////////////////////////////
////
/// SDL Texture:
//


//readable names (mainly for debug msgs):
const std::map<Uint32, const char*> SDL_TextureAccessNames =
{
    {SDL_TEXTUREACCESS_STATIC, "static"}, //changes rarely, not lockable
    {SDL_TEXTUREACCESS_STREAMING, "streaming"}, //changes frequently, lockable
    {SDL_TEXTUREACCESS_TARGET, "target"}, //can be used as a render target
};


//SDL_Texture ptr auto-cleanup wrapper:
//includes a few factory helper methods
template <bool WantPixels = true> //, bool DebugInfo = true>
class SDL_AutoTexture: public std::unique_ptr<SDL_Texture, std::function<void(SDL_Window*)>>
{
public: //ctors/dtors
    explicit SDL_AutoTexture(SrcLine srcline = 0): super(0, deleter), m_srcline(srcline) {} //no surface
//    template <typename ... ARGS>
//    SDL_AutoSurface(ARGS&& ... args, SrcLine = 0): super(0, deleter), sdllib(SDL_INIT_VIDEO, SRCLINE)
    explicit SDL_AutoTexture(SDL_Texture* ptr, SrcLine srcline = 0): SDL_AutoTexture(srcline) //, sdllib(SDL_INIT_VIDEO, SRCLINE)
    {
        debug(RED_MSG "TODO: add shm pixel buf" ENDCOLOR_ATLINE(srcline));
        debug(BLUE_MSG "SDL_AutoTexture ctor 0x%p" ENDCOLOR_ATLINE(srcline), ptr);
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(ptr)) SDL_exc("SDL_AutoTexture: init texture", true, srcline); //required window
        reset(ptr, srcline); //take ownership of window after checking
    }
    virtual ~SDL_AutoTexture() { debug(BLUE_MSG "SDL_AutoTexture dtor 0x%p" ENDCOLOR_ATLINE(m_srcline), get()); }
public: //factory methods:
    template <typename ... ARGS>
    static SDL_AutoWindow& create(ARGS&& ... args, SrcLine srcline = 0) //title, x, y, w, h, mode)
    {
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating window
        return SDL_AutoTexture(SDL_CreateTexture(std::forward<ARGS>(args) ...), srcline); //perfect fwd
    }
    static SDL_AutoTexture& streamed(SDL_Renderer* rndr, int w, int h, SrcLine srcline = 0) { return streamed(rndr, w, h, 0, 0, srcline); }
    static SDL_AutoTexture& streamed(SDL_Renderer* rndr, int w, int h, Uint32 fmt = 0, int access = 0, SrcLine srcline = 0)
    {
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating window
        return SDL_AutoTexture(SDL_CreateTexture(rndr, fmt? fmt: SDL_PIXELFORMAT_ARGB8888, access? access: SDL_TEXTUREACCESS_STREAMING, w, h), srcline);
    }
public: //operators
    SDL_AutoTexture& operator=(SDL_Texture* ptr) //, SrcLine srcline = 0)
    {
//        if (!srcline) srcline = m_srcline;
        SrcLine srcline = m_srcline; //TODO: where to get this?
        debug(BLUE_MSG "SDL_AutoTexture: old texture 0x%p, new texture 0x%p" ENDCOLOR_ATLINE(srcline), get(), ptr);
        reset(ptr, srcline);
        return *this; //fluent/chainable
    }
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const SDL_AutoTexture& me) //CONST SDL_Window* wnd) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        CONST SDL_Texture* txtr = me.get();
        Uint32* fmt; //raw format of texture; actual format may differ, but pixel transfers will use this format
        int* access; //texture access mode (one of the SDL_TextureAccess values)
        int* w; //texture width (in pixels)
        int* h; //texture height (in pixels)
        if (!SDL_OK(SDL_QueryTexture(txtr, &fmt, &access, &w, &h))) SDL_exc("query texture");
//    if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_MSG "Can't get window format" ENDCOLOR));
        ostrm << "SDL_Texture {" << FMT("txtr 0x%p ") << txtr;
        ostrm << w << " x " << h;
        ostrm << FMT(", fmt %i") << SDL_BITSPERPIXEL(fmt);
        ostrm << FMT(" bpp %s") << SDL_PixelFormatShortName(fmt);
        ostrm << FMT(", %s") << unmap(SDL_TextureAccessNames, access);
        ostrm << "}";
        return ostrm; 
    }
public: //methods
    void SetAlpha(Uint8 alpha, SrcLine srcline = 0)
    {
        if (!SDL_OK(SDL_SetTextureAlphaMod(get(), alpha))) SDL_exc("set texture alpha");
    }
    void SetBlendMode(SDL_BlendNode blend, SrcLine srcline = 0)
    {
        if (!SDL_OK(SDL_SetTextureBlendMode(get(), blend))) SDL_exc("set texture blend mode");
    }
    void SetColorMod(Uint8 r, Uint8 g, Uint8 b, SrcLine srcline = 0)
    {
        if (!SDL_OK(SDL_SetTextureColorMod(get(), r, g, b))) SDL_exc("set texture color mod");
    }
    void update(const void* pixels, int pitch = 0, const SDL_Rect* rect = NO_RECT, SrcLine srcline = 0)
    {
        if (!pitch) pitch = get()->width * sizeof(Uint32);
        debug(BLUE_MSG "update %s pixels from texture 0x%p, pixels 0x%p, pitch %d" ENDCOLOR_ATLINE(srcline), rect_desc(rect).c_str(), get(), pixels, pitch);
        if (pitch != get()->width * sizeof(Uint32)) exc(RED_MSG "pitch: expected %d, got d" ENDCOLOR_ATLINE(srcline), get()->width * sizeof(Uint32), pitch);
        if (!SDL_OK(SDL_UpdateTexture(get(), rect, pixels, pitch))) SDL_exc("update texture");
//        if (!SDL_OK(SDL_UpdateTexture(sdlTexture, NULL, myPixels, sizeof(myPixels[0])))) SDL_exc("update texture"); //W * sizeof (Uint32)); //no rect, pitch = row length
    }
    void reset(SDL_Texture* new_ptr, SrcLine srcline = 0)
    {
        if (new_ptr == get()) return; //nothing changed
        if (new_ptr) check(new_ptr, srcline); //validate before acquiring new ptr
        if (new_ptr) inspect(new_ptr, srcline);
        super::reset(new_ptr);
    }
public: //named arg variants
        struct UpdateParams
        {
            const void* pixels;
            int pitch;
            const SDL_Rect* rect = NO_RECT;
            SrcLine srcline = 0;
        };
    template <typename CALLBACK>
    auto update(CALLBACK&& named_params)
    {
        struct UpdateParams update_params;
        VOID update(unpack(update_params, named_params), Unpacked{});
    }
public: //static helper methods
    static void check(SDL_Texture* txtr, SrcLine srcline = 0) { check(txtr, 0, 0, srcline); }
    static void check(SDL_Texture* txtr, int w = 0, int h = 0, SrcLine srcline = 0)
    {
        Uint32* fmt; //raw format of texture; actual format may differ, but pixel transfers will use this format
        int* access; //texture access mode (one of the SDL_TextureAccess values)
        int* w; //texture width (in pixels)
        int* h; //texture height (in pixels)
        if (!SDL_OK(SDL_QueryTexture(txtr, &fmt, &access, &w, &h))) SDL_exc("query texture");
    }
//    static void inspect(SDL_Window* ptr, SrcLine srcline = 0) {} //noop
//    template <bool DebugInfo_copy = DebugInfo> //for function specialization
//    std::enable_if<DebugInfo_copy, static void> inspect(SDL_Window* ptr, SrcLine srcline = 0) //SFINAE
    static void inspect(SDL_Texture* txtr, SrcLine srcline = 0)
    {
//        int wndw, wndh;
//        VOID SDL_GL_GetDrawableSize(wnd, &wndw, &wndh);
//        Uint32 fmt = SDL_GetWindowPixelFormat(wnd);
//        if (!SDL_OK(fmt, SDL_PIXELFORMAT_UNKNOWN)) SDL_exc("Can't get window format", false);
//        std::ostringstream desc;
//        Uint32 flags = SDL_GetWindowFlags(wnd);
//        for (const auto& pair: SDL_WindowFlagNames)
//            if (flags & pair.first) desc << ";" << pair.second;
//        if (!desc.tellp()) desc << ";";
//        debug_level(12, BLUE_MSG "SDL_Window %d x %d, fmt %i bpp %s, flags %s" ENDCOLOR_ATLINE(srcline), wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1);
        SDL_AutoTexture at(txtr, srcline);
        debug_level(12, BLUE_MSG << at << ENDCOLOR_ATLINE(srcline));
    }
//private: //static helpers
    static void deleter(SDL_Texture* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        debug(BLUE_MSG "SDL_AutoTexture: free texture 0x%p" ENDCOLOR, ptr);
        VOID SDL_DestroyTexture(ptr);
    }
private: //members
    SDL_AutoLib sdllib;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
private: //named arg variant helpers
    auto update(const UpdateParams& params, Unpacked) { VOID update(params.pixels, params.pitch, params.rect, params.srcline); }
protected:
//no worky :(    using super = std::unique_ptr; //no C++ built-in base class (due to multiple inheritance), so define one; compiler already knows template params so they don't need to be repeated here :); https://www.fluentcpp.com/2017/12/26/emulate-super-base/
    using super = std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>;
};


////////////////////////////////////////////////////////////////////////////////
////
/// SDL Window:
//

//readable names (mainly for debug msgs):
const std::map<Uint32, const char*> SDL_WindowFlagNames =
{
    {SDL_WINDOW_FULLSCREEN, "FULLSCR"},
    {SDL_WINDOW_OPENGL, "OPENGL"},
    {SDL_WINDOW_SHOWN, "SHOWN"},
    {SDL_WINDOW_HIDDEN, "HIDDEN"},
    {SDL_WINDOW_BORDERLESS, "BORDERLESS"},
    {SDL_WINDOW_RESIZABLE, "RESIZABLE"},
    {SDL_WINDOW_MINIMIZED, "MIN"},
    {SDL_WINDOW_MAXIMIZED, "MAX"},
    {SDL_WINDOW_INPUT_GRABBED, "GRABBED"},
    {SDL_WINDOW_INPUT_FOCUS, "FOCUS"},
    {SDL_WINDOW_MOUSE_FOCUS, "MOUSE"},
    {SDL_WINDOW_FOREIGN, "FOREIGN"},
};


//SDL_Window ptr auto-cleanup wrapper:
//includes a few factory helper methods
template <bool WantRenderer = true, WantTexture = true, WantPixels = true> //, bool DebugInfo = true>
class SDL_AutoWindow: public std::unique_ptr<SDL_Window, std::function<void(SDL_Window*)>>
{
public: //ctors/dtors
    explicit SDL_AutoWindow(SrcLine srcline = 0): super(0, deleter), m_srcline(srcline) {} //no surface
//    template <typename ... ARGS>
//    SDL_AutoSurface(ARGS&& ... args, SrcLine = 0): super(0, deleter), sdllib(SDL_INIT_VIDEO, SRCLINE)
    explicit SDL_AutoWindow(SDL_Window* ptr, SrcLine srcline = 0): SDL_AutoWindow(srcline) //, sdllib(SDL_INIT_VIDEO, SRCLINE)
    {
        debug(RED_MSG "TODO: add streaming texture" ENDCOLOR_ATLINE(srcline));
        debug(RED_MSG "TODO: add shm pixels" ENDCOLOR_ATLINE(srcline));
        debug(BLUE_MSG "SDL_AutoWindow ctor 0x%p" ENDCOLOR_ATLINE(srcline), ptr);
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(ptr)) SDL_exc("SDL_AutoWindow: init window", true, srcline); //required window
        reset(ptr, srcline); //take ownership of window after checking
    }
    virtual ~SDL_AutoWindow() { debug(BLUE_MSG "SDL_AutoWindow dtor 0x%p" ENDCOLOR_ATLINE(m_srcline), get()); }
public: //factory methods:
    template <typename ... ARGS>
    static SDL_AutoWindow& create(ARGS&& ... args, SrcLine srcline = 0) //title, x, y, w, h, mode)
    {
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating window
        return SDL_AutoWindow(SDL_CreateWindow(std::forward<ARGS>(args) ...), srcline); //perfect fwd
    }
//full screen example at: see https://wiki.libsdl.org/MigrationGuide#If_your_game_just_wants_to_get_fully-rendered_frames_to_the_screen
    static SDL_AutoWindow& fullscreen(SrcLine srcline = 0) { return fullscreen(0, srcline); }
    static SDL_AutoWindow& fullscreen(Uint32 flags = 0, SrcLine srcline = 0)
    {
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating window
        return SDL_AutoWindow(SDL_CreateWindow("GpuPort", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DONT_CARE, DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP), srcline);
//        window = isRPi()?
//            SDL_CreateWindow(title, IGNORED_X_Y_W_H, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN): //| SDL_WINDOW_OPENGL): //don't use OpenGL; too slow
//            SDL_CreateWindow(title, 10, 10, MaxFit().w, MaxFit().h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN); //| SDL_WINDOW_OPENGL);
    }
    template <bool WantRenderer_copy = WantRenderer> //for function specialization
    static std::enable_if<WantRenderer_copy, SDL_AutoWindow&> fullsceen(Uint32 flags = 0, SrcLine srcline = 0) //SFINAE
    {
        SDL_Window* wnd;
        SDL_Renderer* rndr;
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, srcline); //init lib before creating window
        return SDL_AutoWindow(!SDL_CreateWindowAndRenderer(DONT_CARE, DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP, &wnd, &rndr)? wnd: NULL, srcline);
    }
    static SDL_AutoTexture& texture(int w, int h, SrcLine srcline = 0)
    {
//TODO:
//        wnd.texture(W, H, SRCLINE);
//        VOID wnd.virtsize(W, H, SRCLINE);
//        SDL_AutoTexture txtr(SDL_AutoTexture::streaming(wnd.renderer(), SRCLINE));
    }
public: //operators
    SDL_AutoWindow& operator=(SDL_Window* ptr) //, SrcLine srcline = 0)
    {
//        if (!srcline) srcline = m_srcline;
        SrcLine srcline = m_srcline; //TODO: where to get this?
        debug(BLUE_MSG "SDL_AutoWindow: old window 0x%p, new window 0x%p" ENDCOLOR_ATLINE(srcline), get(), ptr);
        reset(ptr, srcline);
        return *this; //fluent/chainable
    }
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const SDL_AutoWindow& me) //CONST SDL_Window* wnd) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        CONST SDL_Window* wnd = me.get();
        int wndw, wndh;
        VOID SDL_GL_GetDrawableSize(wnd, &wndw, &wndh);
//        return err(RED_MSG "Can't get drawable window size" ENDCOLOR);
        Uint32 fmt = SDL_GetWindowPixelFormat(wnd);
        if (!SDL_OK(fmt, SDL_PIXELFORMAT_UNKNOWN)) SDL_exc("Can't get window format", false);
//    if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_MSG "Can't get window format" ENDCOLOR));
        std::ostringstream desc;
        Uint32 flags = SDL_GetWindowFlags(wnd);
        for (const auto& pair: SDL_WindowFlagNames)
            if (flags & pair.first) desc << ";" << pair.second;
        if (!desc.tellp()) desc << ";";
//        debug_level(12, BLUE_MSG "SDL_Window %d x %d, fmt %i bpp %s, flags %s" ENDCOLOR_ATLINE(srcline), wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1);
        ostrm << "SDL_Window {" << FMT("wnd 0x%p ") << wnd;
        ostrm << wndw << " x " << wndh;
        ostrm << FMT(", fmt %i") << SDL_BITSPERPIXEL(fmt);
        ostrm << FMT(" bpp %s") << SDL_PixelFormatShortName(fmt);
        ostrm << FMT(", flags %s") << desc.str().c_str() + 1;
        ostrm << FMT(", rndr 0x%p") << renderer(wnd);
        ostrm << "}";
        return ostrm; 
    }
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const SDL_Texture* txtr) //CONST SDL_Window* wnd) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
#if 0 //TMI
        myprintf(12, BLUE_MSG "%d render driver(s): (from %d)" ENDCOLOR, SDL_GetNumRenderDrivers(), where);
        for (int i = 0; i <= SDL_GetNumRenderDrivers(); ++i)
        {
            SDL_RendererInfo info;
            std::ostringstream which, fmts, count, flags;
            if (!i) which << "active";
            else which << i << "/" << SDL_GetNumRenderDrivers();
            if (!OK(i? SDL_GetRenderDriverInfo(i - 1, &info): SDL_GetRendererInfo(renderer, &info))) { err(RED_MSG "Can't get renderer[%s] info" ENDCOLOR, which.str().c_str()); continue; }
            if (info.flags & SDL_RENDERER_SOFTWARE) flags << ";SW";
            if (info.flags & SDL_RENDERER_ACCELERATED) flags << ";ACCEL";
            if (info.flags & SDL_RENDERER_PRESENTVSYNC) flags << ";VSYNC";
            if (info.flags & SDL_RENDERER_TARGETTEXTURE) flags << ";TOTXR";
            if (info.flags & ~(SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE)) flags << ";????";
            if (!flags.tellp()) flags << ";";
            for (unsigned int i = 0; i < info.num_texture_formats; ++i) fmts << ", " << SDL_BITSPERPIXEL(info.texture_formats[i]) << " bpp " << skip(SDL_GetPixelFormatName(info.texture_formats[i]), "SDL_PIXELFORMAT_");
            if (!info.num_texture_formats) { count << "no fmts"; fmts << "  "; }
            else if (info.num_texture_formats != 1) count << info.num_texture_formats << " fmts: ";
            myprintf(12, BLUE_MSG "Renderer[%s]: '%s', flags 0x%x %s, max %d x %d, %s%s" ENDCOLOR, which.str().c_str(), info.name, info.flags, flags.str().c_str() + 1, info.max_texture_width, info.max_texture_height, count.str().c_str(), fmts.str().c_str() + 2);
        }
#endif
    }
public: //methods
    void virtsize(int w, int h, SrcLine srcline = 0)
    {
        SDL_Renderer* rndr = renderer(get());
        debug(BLUE_MSG "set render logical size to %d x %d" ENDCOLOR_ATLINE(srcline), w, h);
        if (!SDL_OK(SDL_RenderSetLogicalSize(rndr, w, h))) SDL_exc("set render logical size"); //use GPU to scale up to full screen
    }
    void reset(SDL_Window* new_ptr, SrcLine srcline = 0)
    {
        if (new_ptr == get()) return; //nothing changed
        if (new_ptr) check(new_ptr, srcline); //validate before acquiring new ptr
        if (new_ptr) inspect(new_ptr, srcline);
        super::reset(new_ptr);
    }
    SDL_Renderer* renderer(SrcLine srcline = 0) { return renderer(get(), srcline); }
//ambiguous    void render(SDL_Texture* txtr, SrcLine srcline = 0) { render(txtr, true, srcline); }
    void render(SDL_Texture* txtr, bool clearfb = true, SrcLine srcline = 0) { render(txtr, NO_RECT, NO_RECT, clearfb, srcline); }
    void render(SDL_Texture* txtr, const SDL_Rect* src = NO_RECT, const SDL_Rect* dest = NO_RECT, bool clearfb = true, SrcLine srcline = 0)
    {
        SDL_Renderer* rndr = renderer(get());
        if (clearfb && !SDL_OK(SDL_RenderClear(rndr))) SDL_exc("render fbclear"); //clear previous framebuffer
        debug(BLUE_MSG "copy %s pixels from texture 0x%p to %s pixels in window 0x%p" ENDCOLOR_ATLINE(srcline), rect_desc(src).c_str(), txtr, rect_desc(dest).c_str(), get());
        if (!SDL_OK(SDL_RenderCopy(rndr, txtr, NO_RECT, NO_RECT))) SDL_exc("render fbcopy"); //copy texture to video framebuffer
        VOID SDL_RenderPresent(rndr); //put new texture on screen
    }
    void update(void* pixels, SDL_Rect* rect = NO_RECT, SrcLine srcline = 0)
    {
//TODO:
//        txtr.update(NULL, myPixels, SRCLINE); //W * sizeof (Uint32)); //no rect, pitch = row length
//        VOID wnd.render(txtr, false, SRCLINE); //put new texture on screen
    }
    void render(Uint32 color = BLACK, SrcLine srcline = 0)
    {
        SDL_Renderer* rndr = renderer(get());
        if (!SDL_OK(SDL_SetRenderDrawColor(rndr, R_G_B_A(color)))) SDL_exc("set render draw color");
        if (!SDL_OK(SDL_RenderClear(rndr))) SDL_exc("render clear");
        debug(BLUE_MSG "set all pixels in window 0x%p to 0x%x" ENDCOLOR_ATLINE(srcline), get(), color);
        VOID SDL_RenderPresent(rndr); //flips texture to screen
    }
public: //named arg variants
        struct RenderParams
        {
            SDL_Texture* txtr;
            const SDL_Rect* src = NO_RECT;
            const SDL_Rect* dest = NO_RECT;
            bool clearfb = true;
//            Uint32 color = BLACK;
            SrcLine srcline = 0;
        };
    template <typename CALLBACK>
    auto render(CALLBACK&& named_params)
    {
        struct RenderParams render_params;
        return render(unpack(render_params, named_params), Unpacked{});
    }
public: //static helper methods
    static void check(SDL_Window* wnd, SrcLine srcline = 0) { check(wnd, 0, 0, srcline); }
    static void check(SDL_Window* wnd, int w = 0, int h = 0, SrcLine srcline = 0)
    {
//    if (want_fmts && (numfmt != want_fmts)) err(RED_MSG "Unexpected #formats: %d (wanted %d)" ENDCOLOR, numfmt, want_fmts);
        Uint32 fmt = SDL_GetWindowPixelFormat(wnd); //desktop OpenGL: 24 RGB8888, RPi: 32 ARGB8888
        if (!SDL_OK(fmt, SDL_PIXELFORMAT_UNKNOWN)) SDL_exc("Can't get window format");
//        int wndw, wndh;
//        SDL_GL_GetDrawableSize(window, &wndw, &wndh);
//        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
    }
//    static void inspect(SDL_Window* ptr, SrcLine srcline = 0) {} //noop
//    template <bool DebugInfo_copy = DebugInfo> //for function specialization
//    std::enable_if<DebugInfo_copy, static void> inspect(SDL_Window* ptr, SrcLine srcline = 0) //SFINAE
    static void inspect(SDL_Window* wnd, SrcLine srcline = 0)
    {
//        int wndw, wndh;
//        VOID SDL_GL_GetDrawableSize(wnd, &wndw, &wndh);
//        Uint32 fmt = SDL_GetWindowPixelFormat(wnd);
//        if (!SDL_OK(fmt, SDL_PIXELFORMAT_UNKNOWN)) SDL_exc("Can't get window format", false);
//        std::ostringstream desc;
//        Uint32 flags = SDL_GetWindowFlags(wnd);
//        for (const auto& pair: SDL_WindowFlagNames)
//            if (flags & pair.first) desc << ";" << pair.second;
//        if (!desc.tellp()) desc << ";";
//        debug_level(12, BLUE_MSG "SDL_Window %d x %d, fmt %i bpp %s, flags %s" ENDCOLOR_ATLINE(srcline), wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1);
        SDL_AutoWindow<false> aw(wnd, srcline);
        debug_level(12, BLUE_MSG << aw << ENDCOLOR_ATLINE(srcline));
    }
//get renderer from window:
//this allows one window ptr to be used with/out AutoRenderer or a caller-supplied renderer
    static SDL_Renderer* renderer(SDL_Window* wnd, bool want_throw = true, SrcLine srcline = 0)
    {
        SDL_Renderer* rndr = SDL_GetRenderer(wnd);
        if (!SDL_OK(rndr)) SDL_exc("can't get renderer for window", want_throw);
        return rndr;
    }
//private: //static helpers
    static void deleter(SDL_Window* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        delete_renderer(ptr); //delete renderer first (get it from window)
        debug(BLUE_MSG "SDL_AutoWindow: free window 0x%p" ENDCOLOR, ptr);
        VOID SDL_DestroyWindow(ptr);
    }
    static void delete_renderer(SDL_Window* ptr) {} //noop
    template <bool WantRenderer_copy = WantRenderer> //for function specialization
    static std::enable_if<WantRenderer_copy, void> delete_renderer(SDL_Window* ptr) //SFINAE
    {
//no worky - compiler bug?        if (!ptr) return;
        if (ptr)
        {
            SDL_Renderer* rndr = renderer(ptr, false); //CAUTION: don't throw(); don't want to interfere with window deleter()
            debug(BLUE_MSG "SDL_AutoWindow: free window 0x%p" ENDCOLOR, ptr);
            VOID SDL_DestroyRenderer(rndr);
        }
    }
private: //members
    SDL_AutoLib sdllib;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
private: //named arg variant helpers
    auto render(const RenderParams& params, Unpacked) { VOID render(params.txtr, params.src, params.dest, params.clearfb, params.srcline); }
protected:
//no worky :(    using super = std::unique_ptr; //no C++ built-in base class (due to multiple inheritance), so define one; compiler already knows template params so they don't need to be repeated here :); https://www.fluentcpp.com/2017/12/26/emulate-super-base/
    using super = std::unique_ptr<SDL_Window, std::function<void(SDL_Window*)>>;
};


////////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions
//


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

void lib_test()
{
    debug(BLUE_MSG << "start" << ENDCOLOR);
    afunc();
    aclass B;
}


#if 0 //SDL1.2 - obsolete
void surface_test()
{
    SDL_AutoLib sdllib(SDL_INIT_VIDEO, SRCLINE);
    /*const SDL_VideoInfo**/ auto videoInfo = SDL_GetVideoInfo();
    if (!SDL_OK(videoInfo)) exc(RED_MSG "get video failed: %s (error %d)" ENDCOLOR, SDL_GetError(), SDL_LastError);
//    SDL_AutoSurface<> screen
    const int w = 72, h = 64; //, mode = SDL_HWSURFACE;
//    auto screen = SDL_AutoSurface<>::SDL_SetVideoMode(w, h, videoInfo->vfmt->BitsPerPixel, mode, SRCLINE);
    auto screen = SDL_MemoryWindow<>::alloc(w, h, SRCLINE);
    if (!SDL_OK(SDL_FillRect(screen.get(), NORECT, MAGENTA | BLACK))) exc(RED_MSG "Can't fill surface" ENDCOLOR));
    SDL_Delay(5 sec);
    if (!SDL_OK(SDL_FillRect(screen.get(), NORECT, RED | BLACK))) exc(RED_MSG "Can't fill surface" ENDCOLOR));
    SDL_Delay(5 sec);

#if 0 //OBSOLETE; for more info https://wiki.libsdl.org/MigrationGuide
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
}
#endif


//based on example code at https://wiki.libsdl.org/MigrationGuide
//using "fully rendered frames" style
void sdl_api_test()
{
    SDL_AutoLib sdllib(SDL_INIT_VIDEO, SRCLINE);

//give me the whole screen and don't change the resolution:
//    SDL_Window* sdlWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Window* sdlWindow;
    SDL_Renderer* sdlRenderer;
    const int W = 3 * 24, H = 64; //1111;
    if (!SDL_OK(SDL_CreateWindowAndRenderer(DONT_CARE, DONT_CARE, SDL_WINDOW_FULLSCREEN_DESKTOP, &sdlWindow, &sdlRenderer))) SDL_exc("cre wnd & rndr");
    if (!SDL_OK(SDL_RenderSetLogicalSize(sdlRenderer, W, H))) SDL_exc("set render logical size"); //use GPU to scale up to full screen

    if (!SDL_OK(SDL_SetRenderDrawColor(sdlRenderer, R_G_B_A(BLACK)))) SDL_exc("set render draw color");
    if (!SDL_OK(SDL_RenderClear(sdlRenderer))) SDL_exc("render clear");
    VOID SDL_RenderPresent(sdlRenderer); //flips texture to screen; no retval to check

    SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H); //tells GPU contents will change frequently; //640, 480);
    if (!SDL_OK(sdlTexture)) SDL_exc("create texture");
//    Uint32* myPixels = malloc
    Uint32 myPixels[H][W]; //TODO: [W][H]; //NOTE: put nodes in same universe adjacent for better cache performance
//    memset(myPixels, 0, sizeof(myPixels));
//    for (int x = 0; x < W; ++x)
//        for (int y = 0; y < H; ++y)
//            myPixels[y][x] = BLACK;
#if 1 //check if 1D index can be used instead of (X,Y); useful for fill() loops
    myPixels[0][W] = RED;
    myPixels[0][2 * W] = GREEN;
    myPixels[0][3 * W] = BLUE;
    debug(BLUE_MSG "px[1,0] = 0x%x %s, [2,0] = 0x%x %s, [3,0] = 0x%x %s" ENDCOLOR, myPixels[1][0], unmap(ColorNames, myPixels[1][0]), myPixels[2][0], unmap(ColorNames, myPixels[2][0]), myPixels[3][0], unmap(ColorNames, myPixels[3][0]));
#endif
//primary color test:
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        for (int i = 0; i < W * H; ++i) myPixels[0][i] = palette[c]; //asRGBA(PINK);

        if (!SDL_OK(SDL_UpdateTexture(sdlTexture, NULL, myPixels, sizeof(myPixels[0])))) SDL_exc("update texture"); //W * sizeof (Uint32)); //no rect, pitch = row length
        if (!SDL_OK(SDL_RenderClear(sdlRenderer))) SDL_exc("render clear"); //clear previous framebuffer
        if (!SDL_OK(SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL))) SDL_exc("render copy"); //copy texture to video framebuffer
        debug(BLUE_MSG "set all %d pixels to 0x%x %s " ENDCOLOR, W * H, palette[c], unmap(ColorNames, palette[c]));
        VOID SDL_RenderPresent(sdlRenderer); //put new texture on screen; no retval to check

        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
        VOID SDL_Delay(2 sec);
    }

//pixel test:
    for (int i = 0; i < W * H; ++i) myPixels[0][i] = BLACK;
    for (int x = 0 + W-3; x < W; ++x)
        for (int y = 0 + H-3; y < H; ++y)
        {
            myPixels[y][x] = palette[(x + y) % SIZEOF(palette)];

            if (!SDL_OK(SDL_UpdateTexture(sdlTexture, NULL, myPixels, sizeof(myPixels[0])))) SDL_exc("update texture"); //W * sizeof (Uint32)); //no rect, pitch = row length
            if (!SDL_OK(SDL_RenderClear(sdlRenderer))) SDL_exc("render clear"); //clear previous framebuffer
            if (!SDL_OK(SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL))) SDL_exc("render copy"); //copy texture to video framebuffer
            debug(BLUE_MSG "set pixel[%d, %d] to 0x%x %s " ENDCOLOR, x, y, myPixels[y][x], unmap(ColorNames, myPixels[y][x]));
            VOID SDL_RenderPresent(sdlRenderer); //put new texture on screen; no retval to check

            if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
            VOID SDL_Delay(0.01 sec);
        }
    
    VOID SDL_DestroyTexture(sdlTexture);
    VOID SDL_DestroyRenderer(sdlRenderer);
    VOID SDL_DestroyWindow(sdlWindow);
}


//based on example code at https://wiki.libsdl.org/MigrationGuide
//using "fully rendered frames" style
void fullscreen_test()
{
//    SDL_AutoLib sdllib(SDL_INIT_VIDEO, SRCLINE);
//give me the whole screen and don't change the resolution:
    const int W = 3 * 24, H = 64; //1111;
    SDL_AutoWindow<> wnd(SDL_AutoWindow<>::fullscreen(SRCLINE));
    VOID wnd.render(BLACK, SRCLINE);
//TODO: combine these:
    wnd.texture(W, H, SRCLINE);
    VOID wnd.virtsize(W, H, SRCLINE);
    SDL_AutoTexture txtr(SDL_AutoTexture::streaming(wnd.renderer(), SRCLINE));
//    Uint32* myPixels = malloc
    Uint32 myPixels[H][W]; //TODO: [W][H]; //NOTE: put nodes in same universe adjacent for better cache performance
//    memset(myPixels, 0, sizeof(myPixels));
//    for (int x = 0; x < W; ++x)
//        for (int y = 0; y < H; ++y)
//            myPixels[y][x] = BLACK;
#if 1 //check if 1D index can be used instead of (X,Y); useful for fill() loops
    myPixels[0][W] = RED;
    myPixels[0][2 * W] = GREEN;
    myPixels[0][3 * W] = BLUE;
    debug(BLUE_MSG "px[1,0] = 0x%x %s, [2,0] = 0x%x %s, [3,0] = 0x%x %s" ENDCOLOR, myPixels[1][0], unmap(ColorNames, myPixels[1][0]), myPixels[2][0], unmap(ColorNames, myPixels[2][0]), myPixels[3][0], unmap(ColorNames, myPixels[3][0]));
#endif
//primary color test:
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        VOID SDL_Delay(2 sec);
        for (int i = 0; i < W * H; ++i) myPixels[0][i] = palette[c]; //asRGBA(PINK);
//TODO: combine
        txtr.update(NULL, myPixels, SRCLINE); //, sizeof(myPixels[0]); //W * sizeof (Uint32)); //no rect, pitch = row length
        VOID wnd.render(txtr.get(), false, SRCLINE); //put new texture on screen
        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
    }

//pixel test:
    for (int i = 0; i < W * H; ++i) myPixels[0][i] = BLACK;
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
        {
            VOID SDL_Delay(0.01 sec);
            myPixels[y][x] = palette[(x + y) % SIZEOF(palette)];
//TODO: combine
            txtr.update(NULL, myPixels, SRCLINE); //W * sizeof (Uint32)); //no rect, pitch = row length
            VOID wnd.render(txtr, false, SRCLINE); //put new texture on screen
            if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
        }
}


//int main(int argc, const char* argv[])
void unit_test()
{
    lib_test();
//    surface_test();
//    window_test();
    sdl_api_test();
    fullscreen_test();

//template <int FLAGS = SDL_INIT_VIDEO | SDL_INIT_AUDIO>
    debug(BLUE_MSG << "finish" << ENDCOLOR);
//    return 0;
}

#endif //def WANT_UNIT_TEST