////////////////////////////////////////////////////////////////////////////////
////
/// GpuCanvas
//

//This is a Node.js add-on to display a rectangular grid of pixels (a texture) on screen using SDL2 and hardware acceleration (via GPU).
//In essence, RPi GPU provides a 24-bit high-speed parallel port with precision timing.
//Optionally, OpenGL and GLSL shaders can be used for generating effects.
//In dev mode, an SDL window is used.  In live mode, full screen is used (must be configured for desired resolution).
//Screen columns generate data signal for each bit of the 24-bit parallel port.
//Without external mux, there can be 24 "universes" of external LEDs to be controlled.  Screen height defines max universe length.

//Copyright (c) 2015, 2016, 2017 Don Julien, djulien@thejuliens.net


////////////////////////////////////////////////////////////////////////////////
////
/// Setup
//

//0. deps:
//0a. node + nvm
//1. software install:
//1a. git clone this repo
//1b. cd this folder
//1c. npm install
//1d. upgrade node-gyp >= 3.6.2
//     [sudo] npm explore npm -g -- npm install node-gyp@latest
//2. npm test (dev mode test, optional)
//3. config:
//3a. edit /boot/config.txt
//3b. give RPi GPU 256 MB RAM (optional)


//dependencies (pre-built, older):
//1. find latest + install using:
//  apt-cache search libsdl2    #2.0.0 as of 10/12/17
//  apt-get install libsdl2-dev
//??   sudo apt-get install freeglut3-dev
//reinstall: sudo apt-get install (pkg-name) --reinstall
//OR sudo apt-get remove (pkg)  then install again

//(from src, newer):

//troubleshooting:
//g++ name demangling:  c++filt -n  (mangled name)
//to recompile this file:  npm run rebuild   or   npm install


//advantages of SDL2 over webgl, glfw, etc:
//- can run from ssh console
//- fewer dependencies/easier to install

//perf: newtest maxed at 24 x 1024 = 89% idle (single core)

//TODO:
//- add music
//- read Vix2 seq
//- map channels
//- WS281X-to-Renard
//- fix/add JS pivot props
//- fix JS Screen
//- ABGR8888 on RPi

//WS281X:
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

//SDL multi-threading info:
//https://gamedev.stackexchange.com/questions/117041/multi-threading-for-rhythm-game


////////////////////////////////////////////////////////////////////////////////
////
/// Headers, general macros, inline functions
//

//#include <stdio.h>
//#include <stdarg.h>
#include <string.h> //strlen
#include <limits.h> //INT_MAX
#include <stdint.h> //uint*_t types
//#include <math.h> //sqrt
#include <sys/time.h>
#include <sys/stat.h>
#include <stdarg.h> //varargs
//#include <byteswap.h> //bswap_32(x)
//#include <iostream> //std::cin
//using std::cin;
//#include <cstring> //std::string
//using std::string;
//using std::getline;
//#include <sstream> //std::ostringstream
//using std::ostringstream;
//#include <type_traits> //std::conditional, std::enable_if, std::is_same, std::disjunction, etc
//#include <stdexcept> //std::runtime_error
//using std::runtime_error;
//#include <memory> //shared_ptr<>
#include <regex> //regex*, *match
//using std::regex;
//using std::cmatch;
//using std::smatch;
//using std::regex_search;
#include <algorithm> //std::min, std::max, std::find
//using std::min;
#include <vector> //std::vector
#include <mutex> //std::mutex, std::lock_guard
#include <atomic> //std::atomic. std::memory_order

//C++11 implements a lot of SDL functionality in a more C++-friendly way, so let's use it! :)
#if __cplusplus < 201103L
 #pragma message("CAUTION: this file probably needs c++11 to compile correctly")
#endif

#define rdiv(n, d)  int(((n) + ((d) >> 1)) / (d))
#define divup(n, d)  int(((n) + (d) - 1) / (d))

#define SIZE(thing)  int(sizeof(thing) / sizeof((thing)[0]))

#define return_void(expr) { expr; return; } //kludge: avoid warnings about returning void value

//inline int toint(void* val) { return (int)(long)val; }
//#define toint(ptr)  reinterpret_cast<int>(ptr) //gives "loses precision" warning/error
#define toint(expr)  (int)(long)(expr)

#define CONST  //should be const but function signatures aren't defined that way

typedef enum {No = false, Yes = true, Maybe} tristate;


//kludge: need nested macros to stringize correctly:
//https://stackoverflow.com/questions/2849832/c-c-line-number
#define TOSTR(str)  TOSTR_NESTED(str)
#define TOSTR_NESTED(str)  #str

//#define CONCAT(first, second)  CONCAT_NESTED(first, second)
//#define CONCAT_NESTED(first, second)  first ## second

#if 0
//template <typename Type>
inline const char* ifnull(const char* val, const char* defval, const char* fallback = 0)
{
    static const char* Empty = "";
    return val? val: defval? defval: fallback? fallback: Empty;
}
//#define ifnull(val, defval, fallback)  ((val)? val: (defval)? defval: fallback)
#endif
//#define ifnull(str, def)  ((str)? str: def)

//use inline functions instead of macros to avoid redundant param eval:

//int min(int a, int b) { return (a < b)? a: b; }
//int max(int a, int b) { return (a > b)? a: b; }

//placeholder stmt (to avoid warnings):
inline void* noop() { return NULL; }

#if 0
//get str len excluding trailing newline:
inline int strlinelen(const char* str)
{
    if (!str || !str[0]) return 0;
    int len = strlen(str);
    if (str[len - 1] == '\n') --len;
    return len;
}
#endif

//skip over first part of string if it matches:
inline const char* skip(const char* str, const char* prefix)
{
//    size_t preflen = strlen(prefix);
//    return (str && !strncmp(str, prefix, preflen))? str + preflen: str;
    if (str)
        while (*str == *prefix++) ++str;
    return str;
}

//text line out to stream:
inline void fputline(FILE* stm, const char* buf)
{
    fputs(buf, stm);
    fputc('\n', stm); 
    fflush(stm);
}


//debug/error messages:
#define WANT_LEVEL  30 //100 //[0..9] for high-level, [10..19] for main logic, [20..29] for mid logic, [30..39] for lower level alloc/dealloc
#define myprintf(level, ...)  (((level) < WANT_LEVEL)? printf(__VA_ARGS__): noop())

//examples:
//#define eprintf(args…) fprintf (stderr, args)
//#define eprintf(format, …) fprintf (stderr, format, __VA_ARGS__)
//#define eprintf(format, …) fprintf (stderr, format, ##__VA_ARGS__)

#define NOERROR  (const char*)-1
//TODO: SDL and non-SDL versions of exc, err
#define exc(...)  errprintf(stdexc, SDL_GetError(), __VA_ARGS__) //TODO: add red
#define err(...)  errprintf(stdpopup, SDL_GetError(), __VA_ARGS__) //TODO: add red
//TODO #define err(fmt, ...)  errprintf(SDL_GetError(), fmt, __VA_ARGS__)
//#define myerr(reason, ...)  errprintf(reason, __VA_ARGS__)
#define printf(...)  errprintf(stderr, NOERROR, __VA_ARGS__) //stderr to keep separate from data output
//#define logprintf(...)  errprintf(stdlog(), NOERROR, __VA_ARGS__) //stderr to keep separate from data output
void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...); //fwd ref
//pseudo-destinations:
FILE* stdpopup = (FILE*)-2;
FILE* stdexc = (FILE*)-3;


/*
//simpler log to file:
void log(const char* fmt, ...)
{
//    char fmtbuf[500];
    static std::mutex serialize;
    static int count = 0;
    serialize.lock();
    FILE* f = fopen("my.log", "a");
    if (!count++) fprintf(f, "-----------\n");
    va_list args;
    va_start(args, fmt);
//    size_t needlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
    serialize.unlock();
}
*/
//file open/close wrapper to look like stdio:
class stdlog: public std::lock_guard<std::mutex> //derive from lock_guard so lock() and unlock() will be called by ctor dtor
{
private:
    FILE* fp; //TODO: RAII on this one also for open(), close()
    bool auto_newline;
    static std::mutex protect;
    static int count;
public:
    stdlog(const char* path = "std.log", bool want_newlines = false): std::lock_guard<std::mutex>(protect), fp(fopen(path, "a")), auto_newline(want_newlines)
    {
        if (!fp) return;
//        protect.lock();
        if (!count++)
        {
            time_t now;
            time(&now);
            struct tm* tp = localtime(&now);
            fprintf(fp, "-------- %d/%d/%.4d %d:%.2d:%.2d --------\n", tp->tm_mon + 1, tp->tm_mday, tp->tm_year + 1900, tp->tm_hour, tp->tm_min, tp->tm_sec);
        }
//        fputc('>', fp);
    }
    operator FILE*() { return fp; }
    ~stdlog()
    {
        if (!fp) return;
//        fputc('<', fp);
        if (auto_newline) { fputc('\n', fp); fflush(fp); }
//        protect.unlock();
//TODO: leave file open and just flush
        fclose(fp);
        fp = NULL;
    }
};
std::mutex stdlog::protect;
int stdlog::count = 0;


////////////////////////////////////////////////////////////////////////////////
////
/// Color defs
//

#define LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA

//ARGB primary colors:
#define BLACK  0xFF000000 //NOTE: need A
#define WHITE  0xFFFFFFFF
#define RED  0xFFFF0000
#define GREEN  0xFF00FF00
#define BLUE  0xFF0000FF
#define YELLOW  0xFFFFFF00
#define CYAN  0xFF00FFFF
#define MAGENTA  0xFFFF00FF
//other ARGB colors (debug):
//#define SALMON  0xFF8080

//const uint32_t PALETTE[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};

//hard-coded ARGB format:
#pragma message("Compiled for ARGB color format (hard-coded)")
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
// #pragma message("Big endian")
// #define Rmask  0xFF000000
// #define Gmask  0x00FF0000
// #define Bmask  0x0000FF00
// #define Amask  0x000000FF
//#define Abits(a)  (clamp(a, 0, 0xFF) << 24)
//#define Rbits(a)  (clamp(r, 0, 0xFF) << 16)
//#define Gbits(a)  (clamp(g, 0, 0xFF) << 8)
//#define Bbits(a)  clamp(b, 0, 0xFF)
 #define Amask(color)  ((color) & 0xFF000000)
 #define Rmask(color)  ((color) & 0x00FF0000)
 #define Gmask(color)  ((color) & 0x0000FF00)
 #define Bmask(color)  ((color) & 0x000000FF)
 #define A(color)  (((color) >> 24) & 0xFF)
 #define R(color)  (((color) >> 16) & 0xFF)
 #define G(color)  (((color) >> 8) & 0xFF)
 #define B(color)  ((color) & 0xFF)
 #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))
#elif SDL_BYTEORDER == SDL_LITTLE_ENDIAN
// #pragma message("Little endian")
// #define Amask  0xFF000000
// #define Bmask  0x00FF0000
// #define Gmask  0x0000FF00
// #define Rmask  0x000000FF
 #define Amask(color)  ((color) & 0xFF000000)
 #define Rmask(color)  ((color) & 0x00FF0000)
 #define Gmask(color)  ((color) & 0x0000FF00)
 #define Bmask(color)  ((color) & 0x000000FF)
 #define A(color)  (((color) >> 24) & 0xFF)
 #define R(color)  (((color) >> 16) & 0xFF)
 #define G(color)  (((color) >> 8) & 0xFF)
 #define B(color)  ((color) & 0xFF)
 #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))
#else
 #error message("Unknown endian")
#endif
#define R_G_B_bytes(color)  R(color), G(color), B(color)
#define R_G_B_A_bytes(color)  R(color), G(color), B(color), A(color)
#define R_G_B_A_masks(color)  Rmask(color), Gmask(color), Bmask(color), Amask(color)

//convert color ARGB <-> ABGR format:
//OpenGL seems to prefer ABGR format, but RGB order is more readable (for me)
//convert back with same function & 0xffffff
//TODO: drop alpha setting?
//??	if (!Amask(color) /*&& (color & 0xffffff)*/) color |= 0xff000000; //RGB present but no alpha; add full alpha to force color to show
#define ARGB2ABGR(color)  (Amask(color) | (Rmask(color) >> 16) | Gmask(color) | (Bmask(color) << 16)) //swap R, B
//#define SWAP32(uint32)  ((Amask(uint32) >> 24) | (Rmask(uint32) >> 8) | (Gmask(uint32) << 8) | (Bmask(uint32) << 24))


//ANSI color codes (for console output):
//https://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_COLOR(code)  "\x1b[" code "m"
#define RED_LT  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_LT  ANSI_COLOR("1;32")
#define YELLOW_LT  ANSI_COLOR("1;33")
#define BLUE_LT  ANSI_COLOR("1;34")
#define MAGENTA_LT  ANSI_COLOR("1;35")
#define CYAN_LT  ANSI_COLOR("1;36")
#define GRAY_LT  ANSI_COLOR("0;37")
//#define ENDCOLOR  ANSI_COLOR("0")
//append the src line# to make debug easier:
#define ENDCOLOR_ATLINE(n)  " &" TOSTR(n) ANSI_COLOR("0") //"\n"
#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(__LINE__)

const std::regex ANSICC_re("\x1b\\[([0-9;]+)m"); //find ANSI console color codes in a string


//simulate a few GLSL built-in functions (for debug/test patterns):
inline float _fract(float x) { return x - int(x); }
inline float _abs(float x) { return (x < 0)? -x: x; }
inline float _clamp(float x, float minVal, float maxVal) { return (x < minVal)? minVal: (x > maxVal)? maxVal: x; }
inline int _clamp(int x, int minVal, int maxVal) { return (x < minVal)? minVal: (x > maxVal)? maxVal: x; }
inline float _mix(float x, float y, float a) { return (1 - a) * x + a * y; }
//use macros so consts can be folded:
#define fract(x)  ((x) - int(x))
#define abs(x)  (((x) < 0)? -(x): x)
#define clamp(val, min, max)  ((val) < (min)? (min): (val) > (max)? (max): (val))
#define mix(x, y, a)  ((1 - (a)) * (x) + (a) * (y))


////////////////////////////////////////////////////////////////////////////////
////
/// SDL utilities, extensions
//

#include <SDL.h> //must include first before other SDL or GL header files
//#include <SDL_thread.h>
//#include <SDL2/SDL_image.h>
//#include <SDL_opengl.h>
//??#include <GL/GLU.h> //#include <GL/freeglut.h> #include <GL/gl.h> #include <GL/glu.h>

//SDL retval convention:
//0 == Success, < 0 == error, > 0 == data ptr (sometimes)
#define SDL_Success  0
#define OK(retval)  ((retval) >= 0)

//more meaningful names for SDL NULL params:
#define UNUSED  0
#define NORECT  NULL
#define FIRST_MATCH  -1
#define THIS_THREAD  NULL
#define UNDEF_EVTID  0


//reduce verbosity:
#define SDL_PixelFormatShortName(fmt)  skip(SDL_GetPixelFormatName(fmt), "SDL_PIXELFORMAT_")
#define SDL_Ticks()  SDL_GetPerformanceCounter()
#define SDL_TickFreq()  SDL_GetPerformanceFrequency()

//timing stats:
inline uint64_t now() { return SDL_Ticks(); }
inline double elapsed(uint64_t started) { return (double)(now() - started) / SDL_TickFreq(); } //Freq = #ticks/second


#if 0
//include SDL reason when throwing exceptions:
class SDL_Exception: public std::runtime_error
{
public:
    SDL_Exception(const char* fmt, ...) //: std::runtime_error(errmsg(fmt)) {};
    {
        char fmtbuf[500];
        va_list args;
        va_start(args, fmt);
        size_t needlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args);
        va_end(args);
        const char* reason = SDL_GetError();
        if (!reason || !reason[0]) reason = "(no details)";
//	: std::runtime_error(make_what(function, SDL_GetError())),
//        static std::string str; //NOTE: must be static to preserve contents after return
        str = desc + ": " + reason;
        return str.c_str();
    }
};
#endif


/*
inline const char* ThreadName(const SDL_Thread* thr = THIS_THREAD)
{
    static string name; //must be persistent after return
    name = SDL_GetThreadName(thr);
    if (!name.length()) name = "0x" + SDL_GetThreadID(thr);
    if (!name.length()) name = "(no name)";
    return name.c_str();
}
*/


//augmented data types:
//allow inited libs or locked objects to be treated like other allocated SDL objects:
//typedef struct { /*int dummy;*/ } SDL_lib;
typedef struct { uint32_t evtid; } SDL_lib;
//typedef struct { int dummy; } IMG_lib;
typedef struct { /*const*/ SDL_sem* sem; } SDL_LockedSemaphore;
typedef struct { /*const*/ SDL_mutex* mutex; } SDL_LockedMutex;
//cache texture info in Surface struct for easier access:
typedef struct { /*const*/ SDL_Texture* txr; SDL_Surface surf; uint32_t fmt; int acc; } SDL_LockedTexture;


#if 0 //not needed; just use std::atomic instead
//allow atomic value to be used in expressions, assignment:
class AtomicInt
{
private:
    SDL_atomic_t value;
public:
    AtomicInt(int newvalue = 0) { operator=(newvalue); }
    operator int() { return SDL_AtomicGet(&value); }
    Atomic& operator=(int newvalue) { SDL_AtomicSet(&value, newvalue); return *this; } //fluent (chainable)
    Atomic& operator+=(int addvalue) { SDL_AtomicAdd(&value, newvalue); return *this; } //fluent (chainable)
};
#endif


//define type names (for debug or error messages):
inline const char* TypeName(const SDL_lib*) { return "SDL_Init"; }
//inline const char* TypeName(const IMG_lib*) { return "IMG_Init"; }
inline const char* TypeName(const SDL_Window*) { return "SDL_Window"; }
inline const char* TypeName(const SDL_Renderer*) { return "SDL_Renderer"; }
inline const char* TypeName(const SDL_Texture*) { return "SDL_Texture"; }
inline const char* TypeName(const SDL_LockedTexture*) { return "SDL_LockedTexture"; }
inline const char* TypeName(const SDL_Surface*) { return "SDL_Surface"; }
inline const char* TypeName(const SDL_sem*) { return "SDL_sem"; }
inline const char* TypeName(const SDL_LockedSemaphore*) { return "SDL_LockedSemaphore"; }
inline const char* TypeName(const SDL_mutex*) { return "SDL_mutex"; }
inline const char* TypeName(const SDL_LockedMutex*) { return "SDL_LockedMutex"; }
inline const char* TypeName(const SDL_cond*) { return "SDL_cond"; }
inline const char* TypeName(const SDL_Thread*) { return "SDL_Thread"; }


//overloaded deallocator function for each type:
//avoids logic specialization by object type
//TODO: remove printf
#define debug(ptr)  myprintf(28, YELLOW_LT "dealloc %s 0x%x" ENDCOLOR, TypeName(ptr), toint(ptr))
#define NOdebug(ptr)
inline int Release(/*const*/ SDL_lib* that) { debug(that); SDL_Quit(); return SDL_Success; }
//inline int Release(IMG_lib* that) { IMG_Quit(); return SDL_Success; }
inline int Release(/*const*/ SDL_Window* that) { debug(that); SDL_DestroyWindow(that); return SDL_Success; }
inline int Release(/*const*/ SDL_Renderer* that) { debug(that); SDL_DestroyRenderer(that); return SDL_Success; }
//inline int Release(SDL_GLContext* that) { debug(that); SDL_GL_DeleteContext(that); return SDL_Success; }
inline int Release(/*const*/ SDL_Texture* that) { debug(that); SDL_DestroyTexture(that); return SDL_Success; }
inline int Release(/*const*/ SDL_LockedTexture* that) { NOdebug(that); SDL_UnlockTexture(that->txr); return SDL_Success; }
inline int Release(/*const*/ SDL_Surface* that) { debug(that); SDL_FreeSurface(that); return SDL_Success; }
inline int Release(/*const*/ SDL_sem* that) { debug(that); SDL_DestroySemaphore(that); return SDL_Success; }
inline int Release(/*const*/ SDL_LockedSemaphore* that) { NOdebug(that); return SDL_SemPost(that->sem); } //one of a few that directly returns a success flag
inline int Release(/*const*/ SDL_mutex* that) { debug(that); SDL_DestroyMutex(that); return SDL_Success; }
inline int Release(/*const*/ SDL_LockedMutex* that) { NOdebug(that); return SDL_UnlockMutex(that->mutex); } //one of a few that directly returns a success flag
inline int Release(/*const*/ SDL_cond* that) { debug(that); SDL_DestroyCond(that); return SDL_Success; }
//inline int Release(SDL_PendingCond* that) { Release(that->mutex); Release(that->cond); return SDL_Success; }
inline int Release(/*const*/ SDL_Thread* that) { debug(that); int exitval; SDL_WaitThread(that, &exitval); return exitval; } //this is one of the few dealloc funcs with a ret code, so might as well check it
#undef NOdebug
#undef debug


//shims to work with auto_ptr<> and OK macro:

/*inline*/ SDL_lib* SDL_INIT(uint32_t flags /*= SDL_INIT_VIDEO*/, int where) //= 0)
{
    static SDL_lib ok = {0}; //only needs init once, so static/shared data can be used here

    std::ostringstream subsys;
    if (flags & SDL_INIT_TIMER) subsys << ";TIMER";
    if (flags & SDL_INIT_AUDIO) subsys << ";AUDIO";
    if (flags & SDL_INIT_VIDEO) subsys << ";VIDEO";
    if (flags & ~(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO)) subsys << ";OTHER";
    if (!flags) subsys << ";NONE";

    SDL_lib* retval = NULL;
    uint32_t already = SDL_WasInit(flags);
    if (already == flags) retval = &ok;
    else if (already) retval = OK(SDL_InitSubSystem(flags & ~already))? &ok: NULL;
    else retval = OK(SDL_Init(flags))? &ok: NULL;
    if (!retval) return (SDL_lib*)exc(RED_LT "SDL_Init %s (0x%x) failed" ENDCOLOR_MYLINE, subsys.str().c_str() + 1, flags, where); //throw SDL_Exception("SDL_Init");

    if (ok.evtid); //already inited //!= UNDEF_EVTID)
    else if (!OK(ok.evtid = SDL_RegisterEvents(2))) return (SDL_lib*)exc(RED_LT "SDL_RegisterEvents failed" ENDCOLOR_MYLINE, where);
    else ++ok.evtid; //kludge: ask for 2 evt ids so last one != 0; then we can check for 0 instead of "(uint32_t)-1"

    if (already) myprintf(22, YELLOW_LT "SDL %s (0x%x) was already %sinitialized (0x%x) from %d" ENDCOLOR, subsys.str().c_str() + 1, flags, (already != flags)? "partly ": "", already, where);
    if (where) myprintf(22, YELLOW_LT "SDL_Init %s (0x%x) = 0x%x ok? %d (from %d)" ENDCOLOR, subsys.str().c_str() + 1, flags, toint(retval), !!retval, where);
    return retval;
}
//#define IMG_INIT(flags)  ((IMG_Init(flags) & (flags)) != (flags)) //0 == Success


//"convert" from SDL data types to locked objects:

///*inline*/ SDL_LockedMutex* SDL_LOCK(SDL_mutex* mutex, int where = 0)
/*inline*/ SDL_LockedMutex* SDL_LOCK(SDL_LockedMutex& locked, SDL_mutex* mutex, int where) //= 0)
{
    /*static SDL_LockedMutex*/ locked = {0};
    if (!mutex) return (SDL_LockedMutex*)exc(RED_LT "No SDL_mutex to lock" ENDCOLOR_MYLINE, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_LockMutex(mutex))) return (SDL_LockedMutex*)exc(RED_LT "SDL_LockMutex 0x%x failed" ENDCOLOR_MYLINE, mutex, where); //throw SDL_Exception("SDL_LockMutex");
    if (where) myprintf(32, YELLOW_LT "SDL_LockMutex 0x%x ok (from %d)" ENDCOLOR, toint(mutex), where);
    locked.mutex = mutex;
    return &locked;
}

///*inline*/ SDL_LockedSemaphore* SDL_LOCK(SDL_sem* sem, int where = 0)
/*inline*/ SDL_LockedSemaphore* SDL_LOCK(SDL_LockedSemaphore& locked, SDL_sem* sem, int where) //= 0)
{
    /*static SDL_LockedSemaphore*/ locked = {0};
    if (!sem) return (SDL_LockedSemaphore*)exc(RED_LT "No SDL_sem to lock" ENDCOLOR_MYLINE, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_SemWait(sem))) return (SDL_LockedSemaphore*)exc(RED_LT "SDL_SemWait 0x%x failed" ENDCOLOR_MYLINE, sem, where); //throw SDL_Exception("SDL_LockMutex");
    if (where) myprintf(32, YELLOW_LT "SDL_LockSemaphore 0x%x ok (from %d)" ENDCOLOR, toint(sem), where);
    locked.sem = sem;
    return &locked;
}

///*inline*/ SDL_LockedTexture* SDL_LOCK(SDL_Texture* txr, int where = 0, int chk_w = 0, int chk_h = 0, uint32_t chk_fmt = 0)
/*inline*/ SDL_LockedTexture* SDL_LOCK(SDL_LockedTexture& locked, SDL_Texture* txr, int where /*= 0*/, int chk_w = 0, int chk_h = 0, uint32_t chk_fmt = 0)
{
    /*static SDL_LockedTexture*/ locked = {0};
    if (!txr) return (SDL_LockedTexture*)exc(RED_LT "No SDL_Texture to lock" ENDCOLOR_MYLINE, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_QueryTexture(txr, &locked.fmt, &locked.acc, &locked.surf.w, &locked.surf.h))) return (SDL_LockedTexture*)exc(RED_LT "SDL_QueryTexture 0x%x failed" ENDCOLOR_MYLINE, txr, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_LockTexture(txr, NORECT, &locked.surf.pixels, &locked.surf.pitch))) return (SDL_LockedTexture*)exc(RED_LT "SDL_LockTexture 0x%x failed" ENDCOLOR_MYLINE, txr, where); //throw SDL_Exception("SDL_LockMutex");
    if (where) myprintf(32, YELLOW_LT "SDL_LockTexture 0x%x ok (from %d)" ENDCOLOR, toint(txr), where);
//additional validation:
    if (!locked.surf.w || (chk_w && (locked.surf.w != chk_w)) || !locked.surf.h || (chk_h && (locked.surf.h != chk_h)))
        return (SDL_LockedTexture*)err(RED_LT "Unexpected texture size: %dx%d should be %dx%d" ENDCOLOR_MYLINE, locked.surf.w, locked.surf.h, chk_w, chk_h, where), (SDL_LockedTexture*)NULL; //NUM_UNIV, UNIV_LEN);
    if (!locked.surf.pixels || (toint(locked.surf.pixels) & 7))
        return (SDL_LockedTexture*)err(RED_LT "Texture pixels not aligned on 8-byte boundary" ENDCOLOR_MYLINE, where), (SDL_LockedTexture*)NULL; //*(auto_ptr*)0;
    if ((size_t)locked.surf.pitch != sizeof(uint32_t) * locked.surf.w)
        return (SDL_LockedTexture*)err(RED_LT "Unexpected pitch: %d should be %zu * %d = %zu" ENDCOLOR_MYLINE, locked.surf.pitch, sizeof(uint32_t), locked.surf.w, sizeof(uint32_t) * locked.surf.w, where), (SDL_LockedTexture*)NULL;
    if (!locked.fmt || (chk_fmt && (locked.fmt != chk_fmt)))
        return (SDL_LockedTexture*)err(RED_LT "Unexpected texture format: %i bpp %s should be %i bpp %s" ENDCOLOR_MYLINE, SDL_BITSPERPIXEL(locked.fmt), SDL_PixelFormatShortName(locked.fmt), SDL_BITSPERPIXEL(chk_fmt), SDL_PixelFormatShortName(chk_fmt), where), (SDL_LockedTexture*)NULL;
    locked.txr = txr;
    return &locked;
}

//capture line# for easier debug:
//NOTE: cpp avoids recursion so macro names can match actual function names here
#define SDL_INIT(...)  SDL_INIT(__VA_ARGS__, __LINE__)
//#define SDL_LOCK(...)  SDL_LOCK(__VA_ARGS__, __LINE__) //TODO: arg shuffle
#define lock_HERE(...)  lock(__VA_ARGS__, __LINE__) //TODO: allow other names?


////////////////////////////////////////////////////////////////////////////////
////
/// Safe pointer/wrapper (auto-dealloc resources when they go out of scope)
//

//make debug easier by tagging with name and location:
//#define TRACKED(thing)  thing.newwhere = __LINE__; thing.newname = TOSTR(thing); thing


//type-safe wrapper for SDL ptrs that cleans up when it goes out of scope:
//std::shared_ptr<> doesn't allow assignment or provide casting; this custom one does
template<typename DataType, typename = void>
class auto_ptr
{
//TODO: private:
public:
    DataType* cast;
public:
//ctor/dtor:
    auto_ptr(DataType* that = NULL): cast(that) {}; // myprintf(22, YELLOW_LT "assign %s 0x%x" ENDCOLOR, TypeName(that), toint(that)); };
    ~auto_ptr() { this->release(); }
//cast to regular pointer:
//    operator const PtrType*() { return this->cast; }
//TODO; for now, just make ptr public
//conversion:
//NO; lval bypasses op=    operator /*const*/ DataType*&() { return this->cast; } //allow usage as l-value; TODO: would that preserve correct state?
    operator /*const*/ DataType*() { return this->cast; } //r-value only; l-value must use op=
//assignment:
    auto_ptr& operator=(/*const*/ DataType* /*&*/ that)
    {
        if (that != this->cast) //avoid self-assignment
        {
            this->release();
//set new state:
//            myprintf(22, YELLOW_LT "reassign %s 0x%x" ENDCOLOR, TypeName(that), toint(that));
            this->cast = that; //now i'm responsible for releasing new object (no ref counting)
        }
        return *this; //fluent (chainable)
    }
public:
//nested class for scoped auto-lock:
    class lock //TODO: replace SDL_LOCK and SDL*Locked* with this
    {
		friend class auto_ptr;
    public:
        lock() { myprintf(1, YELLOW_LT "TODO" ENDCOLOR); };
        ~lock() {};
    };
public:
    void release()
    {
        if (this->cast) Release(this->cast); //overloaded
        this->cast = NULL;
    }
    DataType* keep() //scope-crossing helper
    {
        DataType* retval = this->cast;
        this->cast = NULL; //new owner is responsible for dealloc
        return retval;
    }
};
//#define auto_ptr(type)  auto_ptr<type, TOSTR(type)>
//#define auto_ptr  __SRCLINE__ auto_ptr_noline


//auto_ptr template specializations:
//NOTE: compiler chooses more specialized template instead of more general template
//https://stackoverflow.com/questions/44550976/how-stdconditional-works
#define DUMMY_TYPE  int //can be anything except "void"

template<typename DataType>
class auto_ptr<DataType, typename std::enable_if<std::is_same<DataType, SDL_Window>::value>::type>: public auto_ptr<DataType, DUMMY_TYPE>
{
public:
    static DataType* latest;
public:
    auto_ptr(DataType* that = NULL): auto_ptr<DataType, DUMMY_TYPE>(that) { if (that) latest = that; }; //remember most recent
    auto_ptr& operator=(/*const*/ DataType* /*&*/ that)
    {
        if (that) latest = that;
        auto_ptr<DataType, DUMMY_TYPE>::operator=(that); //super
        return *this; //fluent (chainable)
    }
};
template<> SDL_Window* auto_ptr<SDL_Window>::latest = NULL;


//TODO: combine LockedSemaphore, LockedMutex and LockedTexture:
template<typename DataType>
class auto_ptr<DataType, typename std::enable_if<std::is_same<DataType, SDL_LockedSemaphore>::value>::type>: public auto_ptr<DataType, DUMMY_TYPE>
{
public:
    DataType data = {0}; //alloc with auto_ptr to simplify memory mgmt; init in case that == NULL
public:
    auto_ptr(DataType* that): auto_ptr<DataType, DUMMY_TYPE>(that) { if (that) data = *that; }; //make local copy of locked data
//lock ctor:
//    auto_ptr& operator=(/*const*/ SDL_sem*& that)
    auto_ptr(SDL_sem* that, int where)
    {
//        if (SDL_LOCK(data, that)) auto_ptr<DataType, DUMMY_TYPE>::operator=(&data);
        auto_ptr<DataType, DUMMY_TYPE>::operator=(SDL_LOCK(data, that, where));
//        return *this;
    }
};
template<typename DataType>
class auto_ptr<DataType, typename std::enable_if<std::is_same<DataType, SDL_LockedMutex>::value>::type>: public auto_ptr<DataType, DUMMY_TYPE>
{
public:
    DataType data = {0}; //alloc with auto_ptr to simplify memory mgmt; init in case that == NULL
public:
    auto_ptr(DataType* that): auto_ptr<DataType, DUMMY_TYPE>(that) { if (that) data = *that; }; //make local copy of locked data
//lock ctor:
//    auto_ptr& operator=(/*const*/ SDL_mutex*& that)
    auto_ptr(SDL_mutex* that, int where)
    {
//        if (SDL_LOCK(data, that)) auto_ptr<DataType, DUMMY_TYPE>::operator=(&data);
        auto_ptr<DataType, DUMMY_TYPE>::operator=(SDL_LOCK(data, that, where));
//        return *this;
    }
};
template<typename DataType>
class auto_ptr<DataType, typename std::enable_if<std::is_same<DataType, SDL_LockedTexture>::value>::type>: public auto_ptr<DataType, DUMMY_TYPE>
{
public:
    DataType data = {0}; //alloc with auto_ptr to simplify memory mgmt; init in case that == NULL
public:
    auto_ptr(DataType* that): auto_ptr<DataType, DUMMY_TYPE>(that) { if (that) data = *that; }; //make local copy of locked data
//lock ctor:
//    auto_ptr& operator=(/*const*/ SDL_Texture*& that)
    auto_ptr(SDL_Texture* that, int where)
    {
//        if (SDL_LOCK(data, that)) auto_ptr<DataType, DUMMY_TYPE>::operator=(&data);
        auto_ptr<DataType, DUMMY_TYPE>::operator=(SDL_LOCK(data, that, where));
//        return *this;
    }
};


////////////////////////////////////////////////////////////////////////////////
////
/// Multi-threading
//

//TODO: extend SDL2p with Mutex, Cond, Thread

//send or wait for a signal (cond + mutex):
class Signal
{
private:
    std::vector<void*> pending; //SDL discards signal if nobody waiting (CondWait must occur before CondSignal), so remember it
    auto_ptr<SDL_cond> cond;
//TODO: use smart_ptr with ref count
    static auto_ptr<SDL_mutex> mutex; //low usage, so share it across all signals
    static int count;
//NO-doesn't make sense to send-then-rcv immediately, and rcv-then-send needs processing in between:
//    enum Direction {SendOnly = 1, RcvOnly = 2, Both = 3};
public:
    Signal() //: cond(NULL)
    {
        myprintf(22, BLUE_LT "sig ctor: count %d, m 0x%x, c 0x%x" ENDCOLOR, count, toint(mutex.cast), toint(cond.cast));
        if (!count++)
            if (!(mutex = SDL_CreateMutex())) exc(RED_LT "Can't create signal mutex" ENDCOLOR); //throw SDL_Exception("SDL_CreateMutex");
        if (!(cond = SDL_CreateCond())) exc(RED_LT "Can't create signal cond" ENDCOLOR); //throw SDL_Exception("SDL_CreateCond");
        myprintf(22, YELLOW_LT "signal 0x%x has m 0x%x, c 0x%x" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast));
    }
    ~Signal() { if (!--count) mutex = NULL; }
public:
    void* wait()
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_LT "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_LT "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_LT "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
    }
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_LT "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_LT "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
};
auto_ptr<SDL_mutex> Signal::mutex;
int Signal::count = 0;


class Thread: public auto_ptr<SDL_Thread>
{
protected:
//    static SDL_mutex* mutex;
//    static int count;
//    void* data;
//    int exitval;
//    static Signal ack;
    Signal in, out; //bi-directional sync; name indicates direction relative to thread
private:
//info for deferred init:
    const char* svname;
    bool svasync, started;
public:
    Thread(const char* name, bool async = false): svname(name), svasync(async), started(false) {}; //: auto_ptr<SDL_Thread>(SDL_CreateThread(start_thread, name, this)) //: data(NULL) //, SDL_ThreadFunction& thread_func, void* data = 0)
#if 0 //broken; Thread not constructed yet (vtable incomplete), causes start_thread() to call pure virtual function
    {
        myprintf(22, "here-launch: thread 0x%x, async %d" ENDCOLOR, toint(this->cast), async);
//        if (!count++)
//            if (!(mutex = SDL_CreateMutex())) throw Exception("SDL_CreateMutex");
//        if (!OK(SDL_LockMutex(mutex))) throw Exception("SDL_LockMutex");
//CAUTION: don't call start thread() until Signals are inited!
        auto_ptr<SDL_Thread>::operator=(SDL_CreateThread(start_thread, name, this));
        if (!this->cast) exc(RED_LT "Can't create thead '%s'" ENDCOLOR, name);
//        serialize.lock(); //not really needed (only one bkg thread created), but just in case
        std::lock_guard<std::mutex> lock(protect);
        all.push_back(SDL_GetThreadID(this->cast)); //make it easier to distinguish bkg from main thread; NOTE: only suitable for long-lived threads; list is not cleaned up after thread exits (lazy coder :)
//        serialize.unlock();
//    		throw SDL_Exception("SDL_CreateThread");
 //           SDL_UnlockMutex(mutex);
        if (async) SDL_DetachThread(this->keep()); //thread.cast);
//        {
//            thread = NULL; //thread can exit at any time, so it's not safe to reference it any more
//        }
//        SDL_Delay(1); //give new thread time to start up
    }
#endif
//    /*virtual*/ ~Thread() {};
//    {
////        if (mutex_) { SDL_UnlockMutex(mutex_); SDL_DestroyMutex(mutex_); mutex_ = nullptr; }
//    	if (!thread_) return;
//        int exitcode = -1;
//        SDL_WaitThread(thread_, &exitcode);
//        if (!OK(exitcode)) throw Exception("SDL_WaitThread");
//        thread_ = NULL;
//    }
/*
    Thread& operator=(const Thread& that)
    {
        if (that != this) //avoid self-assignment
        {
            this->release();
//set new state:
            myprintf(22, YELLOW_LT "reassign %s 0x%x" ENDCOLOR, TypeName(that), toint(that));
            this->cast = that; //now i'm responsible for releasing new object (no ref counting)
//            if (that) latest = that;
        }
        return *this; //fluent (chainable)
    }    
*/
private:
//kludge: defer construction until first msg sent in; avoids problem with pure virtual function
    void init_delayed()
    {
        if (this->started) return; //this->cast) return; //thread already started
        myprintf(22, CYAN_LT "launch thread '%s', async %d" ENDCOLOR, svname, svasync);
//CAUTION: don't call start_thread() until Signals are inited and vtable is populated!
        auto_ptr<SDL_Thread>::operator=(SDL_CreateThread(start_thread, svname, this));
        if (!this->cast) return_void(exc(RED_LT "Can't create thead '%s'" ENDCOLOR, svname));
        this->started = true; //don't start it again
//        std::lock_guard<std::mutex> lock(protect);
        all.push_back(SDL_GetThreadID(this->cast)); //make it easier to distinguish bkg from main thread; NOTE: only suitable for long-lived threads; list is not cleaned up after thread exits (lazy coder :)
        if (!svasync) return;
        SDL_DetachThread(this->keep()); //thread can exit at any time, so it's not safe to reference it any more
    }
    static int start_thread(void* data)
    {
        Thread* that = static_cast<Thread*>(data);
        myprintf(33, "kick child thread data 0x%x" ENDCOLOR, toint(that));
        that->out.wake(that->main(that->in.wait()));
        myprintf(33, "child done async 0x%x" ENDCOLOR, toint(that));
        return SDL_Success; //TODO?
    }
    virtual void* main(void* data) = 0; //must override in derived class
public:
//    void* run(void* data = NULL, bool async = false)
//    {
////        UnlockMutex(mutex_);
//        send(data); //wake up child thread
//        return async? SDL_Success: wait(); //wait for child to reply
////        return this->exitval;
//    }
//public-facing:
//NOTE: first msg in/out by main thread will start up bkg thread (delayed init)
    void* message(void* msg = NULL) { wake(msg); return wait(); } //send+receive
    void wake(void* msg = NULL) { init_delayed(); in.wake(msg); } //send
    void* wait() { init_delayed(); return out.wait(); } //receive
public:
    SDL_threadID ID() { return this->cast? SDL_GetThreadID(this->cast): -1; }
//    static std::mutex protect; //TODO: not needed? bkg thread only created once at start
    static std::vector<SDL_threadID> all; //all.size() can be used for thread count
//kludge: SDL_GetThreadName doesn't work on NULL (current thread); need to distinguish based on thead ID instead :(
    static int isBkgThread()
    {
//        std::lock_guard<std::mutex> lock(protect);
        auto pos = std::find(all.begin(), all.end(), SDL_GetThreadID(THIS_THREAD));
        return all.end() - pos; //0 => not found (main thread), > 0 => index in list (can be used as abreviated thread name/#)
    }

};
std::vector<SDL_threadID> Thread::all;
//std::mutex Thread::protect;
//Signal Thread::ack;


////////////////////////////////////////////////////////////////////////////////
////
/// misc global defs
//


//SDL_Init must be called before most other SDL functions and only once, so put it at global scope:
auto_ptr<SDL_lib> SDL(SDL_INIT(SDL_INIT_VIDEO));

typedef struct WH { uint16_t w, h; } WH; //pack width, height into single word for easy return from functions

//fwd refs:
void debug_info(CONST SDL_lib*, int where);
void debug_info(CONST SDL_Window*, int where);
void debug_info(CONST SDL_Renderer*, int where);
void debug_info(CONST SDL_Surface*, int where);
//capture line# for easier debug:
//NOTE: cpp avoids recursion so macro names can match actual function names here
#define debug_info(...)  debug_info(__VA_ARGS__, __LINE__)
WH Screen();
WH MaxFit();
uint32_t limit(uint32_t color);
uint32_t hsv2rgb(float h, float s, float v);
//uint32_t ARGB2ABGR(uint32_t color);
const char* commas(int64_t);
bool exists(const char* path);


////////////////////////////////////////////////////////////////////////////////
////
/// GpuCanvas class, screen functions
//


//check for RPi:
//NOTE: results are cached (outcome won't change)
bool isRPi()
{
//NOTE: mutex not needed here
//main thread will call first, so race conditions won't occur (benign anyway)
    static std::atomic<tristate> isrpi(Maybe);
//    static std::mutex protect;
//    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

//    myprintf(3, BLUE_LT "isRPi()" ENDCOLOR);
//    serialize.lock(); //not really needed (low freq api), but just in case
    if (isrpi == Maybe) isrpi = exists("/boot/config.txt")? Yes: No;
//    serialize.unlock();
    return (isrpi == Yes);
}


//get screen width, height:
//wrapped in a function so it can be used as initializer (optional)
//screen height determines max universe size
//screen width should be configured according to desired data rate (DATA_BITS per node)
WH Screen()
{
//NOTE: mutex not needed here, but std::atomic complains about deleted function
//main thread will call first, so race conditions won't occur (benign anyway)
//    static std::atomic<int> w = 0, h = {0};
    static WH wh = {0};
    static std::mutex protect;
    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

    if (!wh.w || !wh.h)
    {
//        auto_ptr<SDL_lib> sdl(SDL_INIT(SDL_INIT_VIDEO)); //for access to video info; do this in case not already done
        if (!SDL_WasInit(SDL_INIT_VIDEO)) err(RED_LT "ERROR: Tried to get screen info before SDL_Init" ENDCOLOR);
//        if (!sdl && !(sdl = SDL_INIT(SDL_INIT_VIDEO))) err(RED_LT "ERROR: Tried to get screen before SDL_Init" ENDCOLOR);
        myprintf(22, BLUE_LT "%d display(s):" ENDCOLOR, SDL_GetNumVideoDisplays());
        for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
        {
            SDL_DisplayMode mode = {0};
            if (!OK(SDL_GetCurrentDisplayMode(i, &mode))) //NOTE: SDL_GetDesktopDisplayMode returns previous mode if full screen mode
                err(RED_LT "Can't get display[%d/%d]" ENDCOLOR, i, SDL_GetNumVideoDisplays());
            else myprintf(22, BLUE_LT "Display[%d/%d]: %d x %d px @%dHz, %i bbp %s" ENDCOLOR, i, SDL_GetNumVideoDisplays(), mode.w, mode.h, mode.refresh_rate, SDL_BITSPERPIXEL(mode.format), SDL_PixelFormatShortName(mode.format));
            if (!wh.w || !wh.h) { wh.w = mode.w; wh.h = mode.h; } //take first one, continue (for debug)
//            break; //TODO: take first one or last one?
        }
    }

//set reasonable values if can't get info:
    if (!wh.w || !wh.h)
    {
        /*throw std::runtime_error*/ exc(RED_LT "Can't get screen size" ENDCOLOR);
        wh.w = 1536;
        wh.h = wh.w * 3 / 4; //4:3 aspect ratio
        myprintf(22, YELLOW_LT "Using dummy display mode %dx%d" ENDCOLOR, wh.w, wh.h);
    }
    return wh;
}


//#define RENDER_THREAD

#define WS281X_BITS  24 //each WS281X node has 24 data bits
#define TXR_WIDTH  (3 * WS281X_BITS) //- 1) //data signal is generated at 3x bit rate, last bit overlaps H blank

//window create and redraw:
//all GPU work done in bkg thread (asynchronously)
class GpuCanvas: public Thread
{
private:
//    auto_ptr<SDL_lib> sdl; //NOTE: this must occur before thread? most sources say do this once only; TODO: use ref counter?
//set only by fg thread:
    std::atomic<bool> dirty, done;
    auto_ptr<SDL_Window> window;
    auto_ptr<SDL_Surface> pxbuf;
//set only by bkg thread:
    auto_ptr<SDL_Renderer> renderer;
//set by bkg + fg threads:
    auto_ptr<SDL_mutex> busy;
//    std::atomic<int> txr_busy;
    auto_ptr<SDL_Texture> canvas;
//shared data:
//    static auto_ptr<SDL_lib> sdl;
//    static int count;
//performance stats:
    uint64_t started; //doesn't need to be atomic; won't be modified after fg thread wakes
    std::atomic<uint32_t> numfr, numerr, num_dirty; //could be updated by another thread
    struct { uint64_t previous; std::atomic<uint64_t> user_time, caller_time, pivot_time; } fg; //, lock_time, update_time, unlock_time; } fg;
    struct { uint64_t previous; std::atomic<uint64_t> caller_time, pivot_time, lock_time, update_time, unlock_time, copy_time, present_time; } bg;
public:
    bool WantPivot; //dev/debug vs. live mode
    int num_univ, univ_len; //called-defined
    std::atomic<double> PresentTime; //presentation timestamp (set by bkg rendering thread)
    enum UniverseTypes { WS281X = 0, BARE_SSR = 1, CHIPIPLEXED_SSR = 2}; //TODO: make extensible
    std::vector<UniverseTypes> Types; //universe types
    int width() { return this->num_univ; }
    int height() { return this->univ_len; }
public:
//ctor/dtor:
    GpuCanvas(const char* title, int num_univ, int univ_len, bool want_pivot = true): Thread("GpuCanvas", true)
    {
//        myprintf(33, "GpuCanvas ctor" ENDCOLOR);
        if (!SDL_WasInit(SDL_INIT_VIDEO)) err(RED_LT "ERROR: Tried to get canvas before SDL_Init" ENDCOLOR);
//        if (!count++) Init();
        if (!title) title = "GpuCanvas";
        myprintf(3, BLUE_LT "Init: title '%s', #univ %d, univ len %d, pivot? %d" ENDCOLOR, title, num_univ, univ_len, want_pivot);

//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0") != SDL_TRUE) //set texture filtering to linear; TODO: is this needed?
            err(YELLOW_LT "Warning: Linear texture filtering not enabled" ENDCOLOR);
//TODO??    SDL_bool SDL_SetHintWithPriority(const char*      name, const char*      value,SDL_HintPriority priority)

#define IGNORED_X_Y_W_H  0, 0, 200, 100 //not used for full screen mode
//leave window on main thread so it can process events:
//https://stackoverflow.com/questions/6172020/opengl-rendering-in-a-secondary-thread
        int wndw, wndh;
        window = isRPi()?
            SDL_CreateWindow(title, IGNORED_X_Y_W_H, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN): //| SDL_WINDOW_OPENGL): //don't use OpenGL; too slow
            SDL_CreateWindow(title, 10, 10, MaxFit().w, MaxFit().h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN); //| SDL_WINDOW_OPENGL);
        if (!window) return_void(exc(RED_LT "Create window failed" ENDCOLOR));
        uint32_t fmt = SDL_GetWindowPixelFormat(window); //desktop OpenGL: 24 RGB8888, RPi: 32 ARGB8888
        if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(exc(RED_LT "Can't get window format" ENDCOLOR));
        SDL_GL_GetDrawableSize(window, &wndw, &wndh);
        myprintf(22, BLUE_LT "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        SDL_GetWindowSize(window, &wndw, &wndh);
//        myprintf(22, BLUE_LT "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        SDL_GetWindowMaximumSize(window, &wndw, &wndh);
//        myprintf(22, BLUE_LT "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        int top, left, bottom, right;
//        if (!OK(SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right))) return_void(exc(RED_LT "Can't get window border size" ENDCOLOR));
//        myprintf(22, BLUE_LT "wnd border size: t %d, l %d, b %d, r %d" ENDCOLOR, top, left, bottom, right);
        debug_info(window);

        if ((num_univ < 1) || (num_univ > WS281X_BITS)) return_void(exc(RED_LT "Bad number of universes: %d (should be 1..%d)" ENDCOLOR, num_univ, WS281X_BITS));
        if ((univ_len < 1) || (univ_len > wndh)) return_void(exc(RED_LT "Bad universe size: %d (should be 1..%d)" ENDCOLOR, univ_len, wndh));
//NOTE: to avoid fractional pixels, screen/window width should be a multiple of 71, height should be a multiple of univ_len
        if (wndw % (TXR_WIDTH - 1)) myprintf(1, YELLOW_LT "Window width %d is not a multiple of %d" ENDCOLOR, wndw, TXR_WIDTH - 1);
        if (wndh % univ_len) myprintf(1, YELLOW_LT "Window height %d is not a multiple of %d" ENDCOLOR, wndh, univ_len);

//create memory buf to hold pixel data:
//NOTE: surface + texture must always be 3 * WS281X_BITS - 1 pixels wide
//data signal is generated at 3x bit rate, last bit overlaps H blank
//surface + texture height determine max # WS281X nodes per universe
//SDL will stretch texture to fill window (V-grouping); OpenGL not needed for this
//use same fmt + depth as window; TODO: is this more efficient?
//        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, 8+8+8+8, SDL_PIXELFORMAT_ARGB8888);
//TODO: is SDL_AllocFormat helpful here?
        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
        if (!pxbuf) return_void(exc(RED_LT "Can't alloc pixel buf" ENDCOLOR));
        if ((pxbuf.cast->w != TXR_WIDTH) || (pxbuf.cast->h != univ_len)) return_void(exc(RED_LT "Pixel buf wrong size: got %d x %d, wanted %d x %d" ENDCOLOR, pxbuf.cast->w, pxbuf.cast->h, TXR_WIDTH, univ_len));
        if (toint(pxbuf.cast->pixels) & 3) return_void(exc(RED_LT "Pixel buf not quad byte aligned" ENDCOLOR));
        if (pxbuf.cast->pitch != 4 * pxbuf.cast->w) return_void(exc(RED_LT "Pixel buf pitch: got %d, expected %d" ENDCOLOR, pxbuf.cast->pitch, 4 * pxbuf.cast->w));
        debug_info(pxbuf);
        if (!OK(SDL_FillRect(pxbuf, NORECT, MAGENTA | BLACK))) return_void(exc(RED_LT "Can't clear pixel buf" ENDCOLOR));
//        myprintf(33, "bkg pxbuf ready" ENDCOLOR);

        if (!(busy = SDL_CreateMutex())) return_void(exc(RED_LT "Can't create signal mutex" ENDCOLOR)); //throw SDL_Exception("SDL_CreateMutex");

        done = false;
        dirty = true; //force initial screen update + notification of canvas available (no paint doesn't block)
//        txr_busy = 0;
        this->num_univ = num_univ;
        this->univ_len = univ_len;
        this->WantPivot = want_pivot;
        this->Types.resize(num_univ, WS281X); //NOTE: caller needs to call paint() after changing this
        fg.user_time = fg.caller_time = fg.pivot_time = 0; //fg.update_time = fg.unlock_time = 0;
        myprintf(22, BLUE_LT "GpuCanvas wake thread" ENDCOLOR);
        this->wake((void*)0x1234); //run main() asynchronously in bkg thread
        myprintf(22, BLUE_LT "GpuCanvas wait for bkg" ENDCOLOR);
        this->wait(); //wait for bkg thread to init
        myprintf(22, BLUE_LT "GpuCanvas bkg thread ready, ret to caller" ENDCOLOR);
    }
    ~GpuCanvas()
    {
        myprintf(22, BLUE_LT "GpuCanvas dtor" ENDCOLOR);
        stats();
        this->done = true;
        this->wake(); //eof; tell bkg thread to quit (if it's waiting)
        myprintf(22, YELLOW_LT "GpuCanvas dtor" ENDCOLOR);
    }
private:
//bkg thread:
    void* main(void* data)
    {
//        myprintf(33, "bkg thr main start" ENDCOLOR);
        uint64_t delta;
        started = now();
        PresentTime = -1; //prior to official start of playback
//        SDL_Window* window = reinterpret_cast<SDL_Window*>(data); //window was created in main thread where events are handled
        myprintf(8, MAGENTA_LT "bkg thread started: data 0x%x" ENDCOLOR, toint(data));

	    renderer = SDL_CreateRenderer(window, FIRST_MATCH, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); //NOTE: PRESENTVSYNC syncs with V refresh rate (typically 60 Hz)
        if (!renderer) return err(RED_LT "Create renderer failed" ENDCOLOR);
        debug_info(renderer);

#if 0
//NOTE: surface + texture must always be 3 * WS281X_BITS - 1 pixels wide
//data signal is generated at 3x bit rate, last bit overlaps H blank
//surface + texture height determine max # WS281X nodes per universe
//SDL will stretch texture to fill window (V-grouping); OpenGL not needed for this
//use same fmt + depth as window; TODO: is this more efficient?
//        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, 8+8+8+8, SDL_PIXELFORMAT_ARGB8888);
        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(this->fmt), this->fmt);
        if (!pxbuf) return (void*)err(RED_LT "Can't alloc pixel buf" ENDCOLOR);
        if ((pxbuf.cast->w != TXR_WIDTH) || (pxbuf.cast->h != univ_len)) return (void*)err(RED_LT "Pixel buf wrong size: got %d x %d, wanted %d x %d" ENDCOLOR, pxbuf.cast->w, pxbuf.cast->h, TXR_WIDTH, univ_len);
        if (toint(pxbuf.cast->pixels) & 3) return (void*)err(RED_LT "Pixel buf not quad byte aligned" ENDCOLOR);
        if (pxbuf.cast->pitch != 4 * pxbuf.cast->w) return (void*)err(RED_LT "Pixel buf pitch: got %d, expected %d" ENDCOLOR, pxbuf.cast->pitch, 4 * pxbuf.cast->w);
        debug_info(pxbuf);
        if (!OK(SDL_FillRect(pxbuf, NORECT, MAGENTA | BLACK))) return (void*)err(RED_LT "Can't clear pixel buf" ENDCOLOR);
//        myprintf(33, "bkg pxbuf ready" ENDCOLOR);
#endif

        canvas = SDL_CreateTexture(renderer, pxbuf.cast->format->format, SDL_TEXTUREACCESS_STREAMING, pxbuf.cast->w, pxbuf.cast->h); //SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, wndw, wndh);
        if (!canvas) return (void*)err(RED_LT "Can't create canvas texture" ENDCOLOR);
        myprintf(8, MAGENTA_LT "bkg startup took %2.1f msec" ENDCOLOR, elapsed(started));

        SDL_Rect Hclip = {0, 0, TXR_WIDTH - 1, this->univ_len}; //last col overlaps with H-sync; clip
        myprintf(22, BLUE_LT "render copy (%d, %d, %d, %d) => target" ENDCOLOR, Hclip.x, Hclip.y, Hclip.w, Hclip.h);

//        Paint(NULL); //start with all pixels dark
        bg.caller_time = bg.pivot_time = bg.lock_time = bg.update_time = bg.unlock_time = bg.copy_time = bg.present_time = numfr = numerr = num_dirty = 0;

        started = fg.previous = bg.previous = now();
        PresentTime = 0; //start of official playback
//        myprintf(33, "bkg ack main" ENDCOLOR);
        out.wake(pxbuf); //tell main thread i'm ready
//        out.wake(pxbuf); //tell main thread canvas is available (advanced notice so first paint doesn't block)

        for (;;) //render loop; runs continuously at screen rate (30 - 60 Hz)
        {
//TODO: change to:
//          if (done) break;
//          if (pxbuf_time <= latest_render_time) { lock+copy; RenderCopy(); }
//          RenderPresent; ++numfr; stats(); //keeps loop running at fixed fps, gives timestamp

//NOTE: OpenGL is described as not thread safe
//apparently even texture upload must be in same thread; else get "intel_do_flush_locked_failed: invalid argument" crashes
#if 1 //gpu upload in bg thread
            void* pixels = in.wait();
            if (!pixels) break; //eof
            delta = now() - bg.previous; bg.caller_time += delta; bg.previous += delta;
            ++num_dirty;

            pivot((uint32_t*)pixels);
            delta = now() - bg.previous; bg.pivot_time += delta; bg.previous += delta;
            out.wake((void*)true); //allow fg thread to refill buf while render finishes in bkg

            { //scope for locked texture
                auto_ptr<SDL_LockedTexture> lock_HERE(canvas.cast); //SDL_LOCK(canvas));
                delta = now() - bg.previous; bg.lock_time += delta; bg.previous += delta;
//NOTE: pixel data must be in correct (texture) format
//NOTE: SDL_UpdateTexture is reportedly slow; use Lock/Unlock and copy pixel data directly for better performance
                memcpy(lock.data.surf.pixels, pxbuf.cast->pixels, pxbuf.cast->pitch * pxbuf.cast->h);
                delta = now() - bg.previous; bg.update_time += delta; bg.previous += delta;
#if 0
    std::ostringstream buf; //char buf[1024]
    buf << std::hex;
    for (int x = 0; x < lock.data.surf.w; ++x)
        for (int y = 0, yofs = 0; y < lock.data.surf.h; ++y, yofs += lock.data.surf.pitch / 4)
            buf << "," << ("0x" + ((((uint32_t*)lock.data.surf.pixels)[yofs + x] > 9)? 0: 2)) << ((uint32_t*)lock.data.surf.pixels)[yofs + x];
    myprintf(22, BLUE_LT "paint: %s" ENDCOLOR, buf.str().c_str() + 1);
#endif
            }
            delta = now() - bg.previous; bg.unlock_time += delta; bg.previous += delta;

        	if (!OK(SDL_RenderCopy(renderer, canvas, &Hclip, NORECT))) //&renderQuad, angle, center, flip ))
            {
                err(RED_LT "Unable to render to screen" ENDCOLOR);
                ++numerr;
            }
            delta = now() - bg.previous; bg.copy_time += delta; bg.previous += delta;

#else //gpu upload in fg thread
//??            out.wake(pxbuf); //ask main thread for (more) work
//            void* msg;
//            if (!(msg = in.wait())) break; //ask main thread for (more) work
            if (done) break; //NOTE: need non-blocking check here; TODO: make atomic
//            myprintf(33, "bkg loop" ENDCOLOR);

//OpenGL contexts are effectively thread-local. 
//    if (!OK(SDL_GL_MakeCurrent(gpu.wnd, NULL)))
//        return err(RED_LT "Can't unbind current context" ENDCOLOR);
//    uint_least32_t time = now_usec();
//    xfr_time += ELAPSED;
//    render_times[2] += (time2 = now_usec()) - time1;
//    myprintf(22, RED_LT "TODO: delay music_time - now" ENDCOLOR);
//NOTE: RenderPresent() doesn't return until next frame if V-synced; this gives accurate 60 FPS (avoids jitter; O/S wait is +/- 10 msec)
//TODO: allow caller to resume in parallel

//TODO: only do this if canvas changed?
//for now, do it unconditionally (canvas probably changes most of the time anyway)
//this makes timing consistent
            if (dirty) //fg thread updated canvas; upload to GPU; NOTE: only updates when need to
            { //scope for locked mutex
//TODO: is it better to render on-demand (less workload), or unconditionally (for uniform timing)?
//NOTE: if dirty, canvas not busy; don't need mutex here?
                auto_ptr<SDL_LockedMutex> lock_HERE(busy.cast); //SDL_LOCK(busy));
//                if (txr_busy++) err(RED_LT "txr busy, shouldn't be" ENDCOLOR);
            	if (!OK(SDL_RenderCopy(renderer, canvas, &Hclip, NORECT))) //&renderQuad, angle, center, flip ))
                {
                    err(RED_LT "Unable to render to screen" ENDCOLOR);
                    ++numerr;
                }
                ++num_dirty;
                dirty = false;
//                --txr_busy;
                out.wake(canvas); //tell fg thread canvas is available again for updates
            }
            delta = now() - bg.previous; bg.copy_time += delta; bg.previous += delta;
#endif

//            myprintf(8, MAGENTA_LT "renderer thread: render+wait" ENDCOLOR);
            SDL_RenderPresent(renderer); //update screen; NOTE: blocks until next V-sync (on RPi)
            delta = now() - bg.previous; bg.present_time += delta; bg.previous += delta;
            PresentTime = elapsed(started); //update presentation timestamp (in case main thread wants to know)
//myprintf(22, BLUE_LT "fr[%d] deltas: %lld, %lld, %lld, %lld, %lld" ENDCOLOR, numfr, delta1, delta2, delta3, delta4, delta5);
            if (!(++numfr % (60 * 10))) stats(); //show stats every 10 sec @60 FPS
        }
//        myprintf(33, "bkg done" ENDCOLOR);
        myprintf(8, MAGENTA_LT "bkg renderer thread: exit after %2.1f msec" ENDCOLOR, elapsed(started));
//        done = true;
        return pxbuf; //SDL_Success
    }
public:
//repaint screen (fg thread):
//NOTE: xfrs data to canvas then returns
//actual render occurs asynchronously in bkg; texture won't be displayed until next video frame (v-synced at 60 Hz)
//this allows Node.js event loop to remain responsive
//NOTE: caller owns pixel buf; leave it in caller's memory so it can be updated (need to copy here for pivot anyway)
//NOTE: pixel array assumed to be the correct size here (already checked by Javascript wrapper before calling)
//    inline int xyofs(int x, int y) { return x * this->univ_len + y; }
    bool Paint(uint32_t* pixels) //, void* cb, void* data) //uint32_t fill = BLACK)
//    bool Paint(uint32_t* pixels, const std::function<void (void*)>& cb = 0, void* data = 0) //uint32_t fill = BLACK)
//TODO: https://stackoverflow.com/questions/2938571/how-to-declare-a-function-that-accepts-a-lambda
//    template <typename CBType, typename ArgType>
//    bool Paint(uint32_t* pixels, CBType cb = 0, ArgType data = 0) //uint32_t fill = BLACK)
    {
#if 0
if (pixels)
{
    std::ostringstream buf; //char buf[1024]
    buf << std::hex;
    for (int x = 0; x < this->num_univ; ++x)
        for (int y = 0; y < this->univ_len; ++y)
            buf << "," << ("0x" + ((pixels[xyofs(x, y)] > 9)? 0: 2)) << pixels[xyofs(x, y)];
    myprintf(22, BLUE_LT "paint: %s" ENDCOLOR, buf.str().c_str() + 1);
}
#endif
//        myprintf(22, "GpuCanvas paint" ENDCOLOR);
        uint64_t delta;
        delta = now() - fg.previous; fg.caller_time += delta; fg.previous += delta;
//        myprintf(6, BLUE_LT "Paint(pixels 0x%x), %d x %d = %s len (assumed), 0x%x 0x%x 0x%x ..." ENDCOLOR, pixels, this->num_univ, this->univ_len, commas(this->num_univ * this->univ_len), pixels? pixels[0]: fill, pixels? pixels[1]: fill, pixels? pixels[2]: fill);

//NOTE: OpenGL is described as not thread safe
//apparently even texture upload must be in same thread; else get "intel_do_flush_locked_failed: invalid argument" crashes
#if 1 //gpu upload in bg thread
//        struct { uint32_t* pixels; const std::function<void (void*)>& cb; void* data; } info = {pixels, cb, data};
        this->wake(pixels);
        bool ok = this->wait(); //delay until bg thread has grabbed my data, so I can reuse my buf
        delta = now() - fg.previous; fg.pivot_time += delta; fg.previous += delta;
//        if (cb) cb(data);
        return ok;
#else //gpu upload in fg thread
//        myprintf(22, "GpuCanvas pivot" ENDCOLOR);
        pivot(pixels, fill);
        delta = now() - fg.previous; fg.pivot_time += delta; fg.previous += delta;
//TODO? void* ok = this->wait(); //check if bkg thread init'ed okay

        /*if (numfr)*/ this->wait(); //NOTE: don't want to block main thread (bad for libuv), but need to wait until renderer releases canvas
//NOTE: texture must be owned by render thread (according to various sources):
//this copies pixel data to GPU memory?
//TODO: move this to bkg thread?
#if 1
//NOTE: below assumes that Texture lock works across threads, and that Renderer uses it internally also
        { //scope for locked mutex
#define lock  lock2 //kludge: avoid duplicate var name
            auto_ptr<SDL_LockedMutex> lock_HERE(busy.cast); //(SDL_LOCK(busy));
#undef lock
//            if (txr_busy++) err(RED_LT "txr busy, shouldn't be" ENDCOLOR);
//        { //scope for locked texture
            auto_ptr<SDL_LockedTexture> lock_HERE(canvas.cast); //SDL_LOCK(canvas));
            delta = now() - fg.previous; fg.lock_time += delta; fg.previous += delta;
//NOTE: pixel data must be in correct (texture) format
//NOTE: SDL_UpdateTexture is reportedly slow; use Lock/Unlock and copy pixel data directly for better performance
            memcpy(lock.data.surf.pixels, pxbuf.cast->pixels, pxbuf.cast->pitch * pxbuf.cast->h);
            delta = now() - fg.previous; fg.update_time += delta; fg.previous += delta;
//        }
//            --txr_busy;
        }
#else //slower; doesn't work with streaming texture?
        delta = now() - fg.previous; fg.lock_time += delta; fg.previous += delta;
        if (!OK(SDL_UpdateTexture(canvas, NORECT, pxbuf.cast->pixels, pxbuf.cast->pitch)))
            return err(RED_LT "Can't update texture" ENDCOLOR);
        delta = now() - fg.previous; fg.update_time += delta; fg.previous += delta;
#endif
        delta = now() - fg.previous; fg.unlock_time += delta; fg.previous += delta;
        this->dirty = true; //tell bkg thread canvas changed + wake me when it's safe to change canvas again
#endif

//TODO? void* ok = this->wait(); //check if bkg thread init'ed okay
//        myprintf(22, "GpuCanvas ret from paint" ENDCOLOR);
        return true;
    }
public:
//misc methods:
//    inline double avg_ms(uint64_t val) { return (double)(1000 * val / SDL_TickFreq()) / (double)numfr; } //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
    bool stats()
    {
//TODO: skip if numfr already reported
//        uint64_t elapsed = now() - started, freq = SDL_GetPerformanceFrequency(); //#ticks/second
//        uint64_t unknown_time = elapsed - caller_time - pivot_time - update_time - unlock_time - copy_time - present_time; //unaccounted for; probably function calls, etc
        uint64_t idle_time = isRPi()? bg.present_time: bg.unlock_time; //kludge: V-sync delay appears to be during unlock on desktop
        double elaps = elapsed(started), fps = numfr / elaps;
//#define avg_ms(val)  (double)(1000 * (val)) / (double)freq / (double)numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
//#define avg_ms(val)  (elapsed(now() - (val)) / numfr)  //ticks / freq / #fr
#define avg_ms(val)  (double)(1000 * (val) / SDL_TickFreq()) / numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
        uint32_t numfr_cpy = numfr, numerr_cpy = numerr, numdirty_cpy = num_dirty; //kludge: avoid "deleted function" error on atomic
//        myprintf(12, YELLOW_LT "#fr %d, #err %d, elapsed %2.1f sec, %2.1f fps: %2.1f msec: fg(caller %2.3f + pivot %2.3f + lock %2.3f + update %2.3f + unlock %2.3f), bg(copy %2.3f + present %2.3f), %2.1f%% idle" ENDCOLOR, numfr_cpy, numerr_cpy, elaps, fps, 1000 / fps, avg_ms(fg.caller_time), avg_ms(fg.pivot_time), avg_ms(fg.lock_time), avg_ms(fg.update_time), avg_ms(fg.unlock_time), avg_ms(bg.copy_time), avg_ms(bg.present_time), (double)100 * idle_time / elaps);
//        myprintf(22, BLUE_LT "raw: elapsed %s, freq %s, fg(caller %s, pivot %s, lock %s, update %s, unlock %s), bg(copy %s, present %s)" ENDCOLOR, commas(now() - started), commas(SDL_TickFreq()), commas(fg.caller_time), commas(fg.pivot_time), commas(fg.lock_time), commas(fg.update_time), commas(fg.unlock_time), commas(bg.copy_time), commas(bg.present_time));
        myprintf(12, YELLOW_LT "#fr %d, #err %d, #dirty %d (%2.1f%%), elapsed %2.1f sec, %2.1f fps, %2.1f msec avg: fg(user %2.3f + caller %2.3f + pivot %2.3f), bg(caller %2.3f + pivot %2.3f + lock %2.3f + update %2.3f + unlock %2.3f + copy %2.3f + present %2.3f), bg %2.1f%% idle" ENDCOLOR, 
            numfr_cpy, numerr_cpy, numdirty_cpy, (double)100 * numdirty_cpy / numfr_cpy, elaps, fps, 1000 / fps, 
            avg_ms(fg.user_time), avg_ms(fg.caller_time), avg_ms(fg.pivot_time), 
            avg_ms(bg.caller_time), avg_ms(bg.pivot_time), avg_ms(bg.lock_time), avg_ms(bg.update_time), avg_ms(bg.unlock_time), avg_ms(bg.copy_time), avg_ms(bg.present_time), 
            (double)100 * idle_time / (now() - started));
        myprintf(22, BLUE_LT "raw: elapsed %s, freq %s, fg(user %s, caller %s, pivot %s), bg(caller %s, pivot %s, lock %s, update %s, unlock %s, copy %s, present %s)" ENDCOLOR, 
            commas(now() - started), commas(SDL_TickFreq()), 
            commas(fg.user_time), commas(fg.caller_time), commas(fg.pivot_time), 
            commas(bg.caller_time), commas(bg.pivot_time), commas(bg.lock_time), commas(bg.update_time), commas(bg.unlock_time), commas(bg.copy_time), commas(bg.present_time));
//        myprintf(22, "raw-raw: elapsed %ld, freq %ld" ENDCOLOR, now() - started, SDL_TickFreq());
        return true;
    }
/*
    bool Release()
    {
        num_univ = univ_len = 0;
        pxbuf.release();
        canvas.release();
        renderer.release();
        window.release();
        return true;
    }
*/
private:
//24-bit pivot:
//CAUTION: expensive CPU loop here
//NOTE: need pixel-by-pixel copy for several reasons:
//- ARGB -> RGBA (desirable)
//- brightness limiting (recommended)
//- blending (optional)
//- 24-bit pivots (required, non-dev mode)
//        memset(pixels, 4 * this->w * this->h, 0);
//TODO: perf compare inner/outer swap
//TODO? locality of reference: keep nodes within a universe close to each other (favors caller)
    void pivot(uint32_t* pixels, uint32_t fill = BLACK)
    {
        uint32_t* pxbuf32 = reinterpret_cast<uint32_t*>(this->pxbuf.cast->pixels);
//myprintf(22, "paint pxbuf 0x%x pxbuf32 0x%x" ENDCOLOR, toint(this->pxbuf.cast->pixels), toint(pxbuf32));
#if 0 //test
        for (int y = 0, yofs = 0; y < this->univ_len; ++y, yofs += TXR_WIDTH) //outer
            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
            {
//if (!y || (y >= this->univ_len - 1) || !x3 || (x3 >= TXR_WIDTH - 3)) myprintf(22, "px[%d] @(%d/%d,%d/%d)" ENDCOLOR, yofs + x3, y, this->univ_len, x3, TXR_WIDTH);
                pxbuf32[yofs + x3 + 0] = RED;
                pxbuf32[yofs + x3 + 1] = GREEN;
                if (x3 < TXR_WIDTH - 1) pxbuf32[yofs + x3 + 2] = BLUE;
            }
        return;
#endif
#if 0 //NO; RAM is slower than CPU
        uint32_t rowbuf[TXR_WIDTH + 1], start_bits = this->WantPivot? WHITE: BLACK;
//set up row template with start bits, cleared data + stop bits:
        for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
        {
            rowbuf[x3 + 0] = start_bits; //WHITE; //start bits
            rowbuf[x3 + 1] = BLACK; //data bits (will be overwritten with pivoted color bits)
            rowbuf[x3 + 2] = BLACK; //stop bits (right-most overlaps H-blank)
        }
//            memcpy(&pxbuf32[yofs], rowbuf, TXR_WIDTH * sizeof(uint32_t)); //initialze start, data, stop bits
#endif
        uint32_t leading_edges = BLACK;
        for (uint32_t x = 0, xmask = 0x800000; (int)x < this->num_univ; ++x, xmask >>= 1)
            if (this->Types[(int)x] == WS281X) leading_edges |= xmask; //turn on leading edge of data bit for WS281X
//myprintf(22, BLUE_LT "start bits = 0x%x (based on univ type)" ENDCOLOR, leading_edges);
        bool rbswap = isRPi();
        for (int y = 0, yofs = 0; y < this->univ_len; ++y, yofs += TXR_WIDTH) //outer
        {
            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
            {
                pxbuf32[yofs + x3 + 0] = leading_edges; //WHITE;
                pxbuf32[yofs + x3 + 1] = BLACK; //data bit body (will be overwritten with pivoted color bits)
//                if (x3) pxbuf32[yofs + x3 - 1] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
                pxbuf32[yofs + x3 + 2] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
            }
#if 1
//fill with pivoted data bits:
            if (this->WantPivot)
            {
//NOTE: xmask loop assumes ARGB or ABGR fmt (A in upper byte)
                for (uint32_t x = 0, xofs = 0, xmask = 0x800000; x < (uint32_t)this->num_univ; ++x, xofs += this->univ_len, xmask >>= 1)
            	{
                    uint32_t color = pixels? pixels[xofs + y]: fill;
                    if (rbswap) color = ARGB2ABGR(color); //bswap_32(x)
//                    if (!A(color) || (!R(color) && !G(color) && !B(color))) continue; //no data to pivot
                    color = limit(color); //limit brightness/power
//                color = ARGB2ABGR(color);
                    for (int bit3 = 1; bit3 < TXR_WIDTH; bit3 += 3, color <<= 1)
                        if (color & 0x800000) pxbuf32[yofs + bit3] |= xmask; //set data bit
            	}
                continue; //next row
            }
#endif
//just copy pixels as-is (dev/debug only):
            for (int x = 0, x3 = 0, xofs = 0; x < this->num_univ; ++x, x3 += 3, xofs += this->univ_len)
            {
                uint32_t color = pixels? pixels[xofs + y]: fill;
                if (rbswap) color = ARGB2ABGR(color);
                pxbuf32[yofs + x3 + 0] = pxbuf32[yofs + x3 + 1] = pxbuf32[yofs + x3 + 2] = color;
            }
        }
    }
#if 0 //some sources say to only do it once
public:
//initialize SDL:
    static bool Init()
    {
	    sdl = SDL_INIT(/*SDL_INIT_AUDIO |*/ SDL_INIT_VIDEO); //| SDL_INIT_EVENTS); //events, file, and threads are always inited, so they don't need to be flagged here; //| ??SDL_INIT_TIMER); //SDL_init(SDL_INIT_EVERYTHING);
        if (!OK(sdl)) exc(RED_LT "SDL init failed" ENDCOLOR);
        debug_info(sdl);

//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0") != SDL_TRUE) //set texture filtering to linear; TODO: is this needed?
            err(YELLOW_LT "Warning: Linear texture filtering not enabled" ENDCOLOR);
//TODO??    SDL_bool SDL_SetHintWithPriority(const char*      name, const char*      value,SDL_HintPriority priority)
 
#if 0 //not needed
//use OpenGL 2.1:
        if (!OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2)) || !OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1)))
            return err(RED_LT "Can't set GL version to 2.1" ENDCOLOR);
        if (!OK(SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8)) || !OK(SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8)) || !OK(SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8))) //|| !OK(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8)))
            return err(RED_LT "Can't set GL R/G/B to 8 bits" ENDCOLOR);
//??SDL_GL_BUFFER_SIZE //the minimum number of bits for frame buffer size; defaults to 0
//??SDL_GL_DOUBLEBUFFER //whether the output is single or double buffered; defaults to double buffering on
//??SDL_GL_DEPTH_SIZE //the minimum number of bits in the depth buffer; defaults to 16
//??SDL_GL_STEREO //whether the output is stereo 3D; defaults to off
//??SDL_GL_MULTISAMPLEBUFFERS //the number of buffers used for multisample anti-aliasing; defaults to 0; see Remarks for details
//??SDL_GL_MULTISAMPLESAMPLES //the number of samples used around the current pixel used for multisample anti-aliasing; defaults to 0; see Remarks for details
//??SDL_GL_ACCELERATED_VISUAL //set to 1 to require hardware acceleration, set to 0 to force software rendering; defaults to allow either
        if (!OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES))) //type of GL context (Core, Compatibility, ES)
            return err(RED_LT "Can't set GLES context profile" ENDCOLOR);
//NO    if (!OK(SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1)))
//        return err(RED_LT "Can't set shared context attr" ENDCOLOR);
//??SDL_GL_SHARE_WITH_CURRENT_CONTEXT
//??SDL_GL_FRAMEBUFFER_SRGB_CAPABLE not needed
//??SDL_GL_CONTEXT_RELEASE_BEHAVIOR
#endif
       return true;
    }
//clean up SDL:
//nothing to do; auto_ptr does it all :)
    static bool Quit()
    {
        myprintf(3, BLUE_LT "Quit()" ENDCOLOR);
        return true;
    }
#endif
};
//auto_ptr<SDL_lib> GpuCanvas::sdl;
//int GpuCanvas::count = 0;


////////////////////////////////////////////////////////////////////////////////
////
/// Node.js interface
//

//see https://nodejs.org/docs/latest/api/addons.html
//https://stackoverflow.com/questions/3066833/how-do-you-expose-a-c-class-in-the-v8-javascript-engine-so-it-can-be-created-u

//refs:
//https://community.risingstack.com/using-buffers-node-js-c-plus-plus/
//https://nodeaddons.com/building-an-asynchronous-c-addon-for-node-js-using-nan/
//https://nodeaddons.com/c-processing-from-node-js-part-4-asynchronous-addons/
//example: https://github.com/Automattic/node-canvas/blob/b470ce81aabe2a78d7cdd53143de2bee46b966a7/src/CanvasRenderingContext2d.cc#L764
//https://github.com/nodejs/nan/blob/master/doc/v8_misc.md#api_nan_typedarray_contents
//http://bespin.cz/~ondras/html/classv8_1_1Object.html
#if 0
//see https://nodejs.org/api/addons.html
//and http://stackabuse.com/how-to-create-c-cpp-addons-in-node/
//parameter passing:
// http://www.puritys.me/docs-blog/article-286-How-to-pass-the-paramater-of-Node.js-or-io.js-into-native-C/C++-function..html
// http://luismreis.github.io/node-bindings-guide/docs/arguments.html
// https://github.com/nodejs/nan/blob/master/doc/methods.md
//build:
// npm install -g node-gyp
// npm install --save nan
// node-gyp configure
// node-gyp build
//#define NODEJS_ADDON //TODO: selected from gyp bindings?
#endif


#ifdef BUILDING_NODE_EXTENSION //set by node-gyp
// #pragma message "compiled as Node.js addon"
// #include <node.h>
 #include <nan.h> //includes v8 also
// #include <v8.h>

namespace NodeAddon //namespace wrapper for Node.js functions; TODO: is this needed?
{
//#define JS_STR(...) Nan::New<v8::String>(__VA_ARGS__).ToLocalChecked()
//#define JS_STR(iso, ...) v8::String::NewFromUtf8(iso, __VA_ARGS__)
//TODO: convert to Nan?
#define JS_STR(iso, val) v8::String::NewFromUtf8(iso, val)
//#define JS_INT(iso, val) Nan::New<v8::Integer>(iso, val)
#define JS_INT(iso, val) v8::Integer::New(iso, val)
//#define JS_FLOAT(iso, val) Nan::New<v8::Number>(iso, val)
//#define JS_BOOL(iso, val) Nan::New<v8::Boolean>(iso, val)
#define JS_BOOL(iso, val) v8::Boolean::New(iso, val)


//display/throw Javascript err msg:
void errjs(v8::Isolate* iso, const char* errfmt, ...)
{
	va_list params;
	char buf[BUFSIZ];
	va_start(params, errfmt);
	vsnprintf(buf, sizeof(buf), errfmt, params); //TODO: send to stderr?
	va_end(params);
//	printf("THROW: %s\n", buf);
//	Nan::ThrowTypeError(buf);
//    iso->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(iso, buf)));
    fputline(stderr, buf); //send to console in case not seen by Node.js; don't mix with stdout
    iso->ThrowException(v8::Exception::TypeError(JS_STR(iso, buf)));
//    return;    
}


#if 0 //TODO: getter
void Screen_js(v8::Local<v8::String>& name, const Nan::PropertyCallbackInfo<v8::Value>& info)
{
#else
//void isRPi_js(const Nan::FunctionCallbackInfo<v8::Value>& args)
//void isRPi_js(const v8::FunctionCallbackInfo<v8::Value>& args)
NAN_METHOD(isRPi_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length()) return_void(errjs(iso, "isRPi: expected 0 args, got %d", info.Length()));

//    v8::Local<v8::Boolean> retval = JS_BOOL(iso, isRPi()); //v8::Boolean::New(iso, isRPi());
//    myprintf(3, "isRPi? %d" ENDCOLOR, isRPi());
    info.GetReturnValue().Set(JS_BOOL(iso, isRPi()));
}
#endif


#if 0 //TODO: getter
void Screen_js(v8::Local<v8::String>& name, const Nan::PropertyCallbackInfo<v8::Value>& info)
{
#else
//int Screen_js() {}
//void Screen_js(v8::Local<const v8::FunctionCallbackInfo<v8::Value>& info)
NAN_METHOD(Screen_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length()) return_void(errjs(iso, "Screen: expected 0 args, got %d", info.Length()));

    WH wh = isRPi()? Screen(): MaxFit(); //kludge: give max size caller can use, not actual screen size
//    struct { int w, h; } wh = {Screen().w, Screen().h};
//    v8::Local<v8::Object> retval = Nan::New<v8::Object>();
    v8::Local<v8::Object> retval = v8::Object::New(iso);
//    v8::Local<v8::String> w_name = Nan::New<v8::String>("width").ToLocalChecked();
//    v8::Local<v8::String> h_name = Nan::New<v8::String>("height").ToLocalChecked();
//    v8::Local<v8::Number> retval = v8::Number::New(iso, ScreenWidth());
//    retval->Set(v8::String::NewFromUtf8(iso, "width"), v8::Number::New(iso, wh.w));    
//    retval->Set(v8::String::NewFromUtf8(iso, "height"), v8::Number::New(iso, wh.h));
    retval->Set(JS_STR(iso, "width"), JS_INT(iso, wh.w));
    retval->Set(JS_STR(iso, "height"), JS_INT(iso, wh.h));
//    myprintf(3, "screen: %d x %d" ENDCOLOR, wh.w, wh.h);
//    Nan::Set(retval, w_name, Nan::New<v8::Number>(wh.w));
//    Nan::Set(retval, h_name, Nan::New<v8::Number>(wh.h));
    info.GetReturnValue().Set(retval);
}
#endif


//   String::Utf8Value fileName(args[0]->ToString());
//const char* dbg(const char* str) { myprintf(22, "str %s" ENDCOLOR, str? str: "(none)"); return str; }
//int dbg(int val) { myprintf(22, "int %s" ENDCOLOR, val); return val; }

//?? https://nodejs.org/docs/latest/api/addons.html#addons_wrapping_c_objects
//https://github.com/nodejs/nan/blob/master/doc/methods.md
class GpuCanvas_js: public Nan::ObjectWrap
{
//private:
public:
    GpuCanvas inner;
public:
//    static void Init(v8::Handle<v8::Object> exports);
    static void Init(v8::Local<v8::Object> exports);
    static void Quit(void* ignored); //signature reqd for AtExit
    static std::vector<GpuCanvas_js*> all; //keep track of currently existing instances
//protected:
private:
    explicit GpuCanvas_js(const char* title, int w, int h, bool pivot): inner(title, w, h, pivot) { all.push_back(this); };
//    virtual ~GpuCanvas_js();
    ~GpuCanvas_js() { all.erase(std::find(all.begin(), all.end(), this)); };

//    static NAN_METHOD(New);
    static void New(const v8::FunctionCallbackInfo<v8::Value>& info); //TODO: convert to Nan?
//    static NAN_GETTER(WidthGetter);
//    static NAN_GETTER(PitchGetter);
    static NAN_METHOD(paint);
    static NAN_METHOD(stats);
//??    static NAN_PROPERTY_GETTER(get_pivot);
//??    static NAN_PROPERTY_SETTER(set_pivot);
//    static NAN_GETTER(get_pivot);
//    static NAN_SETTER(set_pivot);
    static NAN_METHOD(width_tofix); //TODO: change to accessor/getter; can't figure out how to do that
    static NAN_METHOD(height_tofix); //TODO: change to accessor/getter; can't figure out how to do that
    static NAN_METHOD(pivot_tofix); //TODO: change to accessor/getter/setter; can't figure out how to do that
//    static NAN_METHOD(release);
//    static void paint(const Nan::FunctionCallbackInfo<v8::Value>& info);
//private:
    static Nan::Persistent<v8::Function> constructor; //v8:: //TODO: Nan macro?
//    void *data;
};
Nan::Persistent<v8::Function> GpuCanvas_js::constructor;
std::vector<GpuCanvas_js*> GpuCanvas_js::all;


//export class to Javascript:
//TODO: convert to Nan?
//void GpuCanvas_js::Init(v8::Handle<v8::Object> exports)
void GpuCanvas_js::Init(v8::Local<v8::Object> exports)
{
    v8::Isolate* iso = exports->GetIsolate(); //~vm heap
//??    Nan::HandleScope scope;

//ctor:
    v8::Local<v8::FunctionTemplate> ctor = /*Nan::New<*/v8::FunctionTemplate::New(iso, GpuCanvas_js::New);
//    v8::Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(GpuCanvas_js::New);
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(JS_STR(iso, "GpuCanvas"));
//    ctor->SetClassName(Nan::New("GpuCanvas").ToLocalChecked());

//prototype:
//    Nan::SetPrototypeMethod(ctor, "paint", save);// NODE_SET_PROTOTYPE_METHOD(ctor, "save", save);
//    v8::Local<v8::ObjectTemplate> proto = ctor->PrototypeTemplate();
//    Nan::SetPrototypeMethod(proto, "paint", paint);
//    NODE_SET_PROTOTYPE_METHOD(ctor, "paint", GpuCanvas_js::paint);
    Nan::SetPrototypeMethod(ctor, "paint", paint);
    Nan::SetPrototypeMethod(ctor, "stats", stats);
    Nan::SetPrototypeMethod(ctor, "width_tofix", width_tofix);
    Nan::SetPrototypeMethod(ctor, "height_tofix", height_tofix);
//TODO    Nan::SetPrototypeMethod(ctor, "utype", GpuCanvas_js::utype);
    Nan::SetPrototypeMethod(ctor, "pivot_tofix", pivot_tofix); //TODO: fix this
//    Nan::SetPrototypeMethod(ctor, "release", GpuCanvas_js::release);
//    Nan::SetAccessor(proto,JS_STR("width"), WidthGetter);
//    Nan::SetAccessor(ctor, JS_STR(iso, "pivotprop"), GpuCanvas_js::getprop_pivot, GpuCanvas_js::setprop_pivot);
//ambiguous:    Nan::SetAccessor(ctor, JS_STR(iso, "pivot"), GpuCanvas_js::get_pivot, GpuCanvas_js::set_pivot);
//    ctor->SetAccessor(JS_STR(iso, "pivot"), GpuCanvas_js::get_pivot, GpuCanvas_js::set_pivot);
//    Nan::SetAccessor(proto,JS_STR("height"), HeightGetter);
//    Nan::SetAccessor(proto,JS_STR("pitch"), PitchGetter);
//    Nan::SetAccessor(proto,JS_STR("src"), SrcGetter, SrcSetter);
    constructor.Reset(/*iso,*/ ctor->GetFunction()); //?? v8::Isolate::GetCurrent(), ctor->GetFunction());
//??    Nan::Set(exports, JS_STR("GpuCanvas"), ctor->GetFunction());
    exports->Set(JS_STR(iso, "GpuCanvas"), ctor->GetFunction());
//    exports->Set(Nan::New("GpuCanvas").ToLocalChecked(), ctor->GetFunction());

//  FreeImage_Initialise(true);
//NO    GpuCanvas::Init(); //will be done by inner
    node::AtExit(GpuCanvas_js::Quit);
}


//instantiate new instance for Javascript:
//TODO: convert to Nan?
void GpuCanvas_js::New(const v8::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::New) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
#if 0
//int inval = To<int>(info[0]).FromJust();
//v8::Local<v8::Array> results = New<v8::Array>(len);
//    registerImage(image);
#endif
    if (info.IsConstructCall()) //Invoked as constructor: `new MyObject(...)`
    {
//    	if (info.Length() != 3) return_void(errjs(iso, "GpuCanvas.New: expected 3 params, got: %d", info.Length()));
        v8::String::Utf8Value title(!info[0]->IsUndefined()? info[0]->ToString(): JS_STR(iso, ""));
//        double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
//	    int w = info[1].As<v8::Int32Value>->Value(); //args[0]->IsUndefined()
	    int w = info[1]->IntegerValue(); //ToInt32()->Value(); //args[0]->IsUndefined()
//myprintf(22, "int? %d, num? %d" ENDCOLOR, info[1]->IsInt32(), info[1]->IsNumber());
//myprintf(22, "int %lld" ENDCOLOR, info[1]->IntegerValue());
//myprintf(22, "int32 %d" ENDCOLOR, info[1]->Int32Value());
//myprintf(22, "num %f" ENDCOLOR, info[1]->NumberValue());
        int h = info[2]->IntegerValue(); //ToInt32()->Value(); //args[0]->IsUndefined()
//        bool pivot = !info[3]->IsUndefined()? info[3]->BooleanValue(): true;
        bool pivot = !info[3]->IsUndefined()? info[3]->BooleanValue(): true;
//myprintf(22, "GpuCanvas_js('%s', %d, %d, %d)" ENDCOLOR, *title? *title: "(none)", w, h, pivot);
        GpuCanvas_js* canvas = new GpuCanvas_js(*title? *title: NULL, w, h, pivot);
        canvas->Wrap(info.This());
//??        Nan::SetAccessor(*canvas, JS_STR(iso, "pivot"), GpuCanvas_js::get_pivot, GpuCanvas_js::set_pivot);
        info.GetReturnValue().Set(info.This());
    }
    else //Invoked as plain function `MyObject(...)`, turn into constructor call.
    {
        const int argc = info.Length();
//        v8::Local<v8::Value> argv[argc] = { info[0]};
//myprintf(22, "ctor called as func with %d params" ENDCOLOR, argc);
        std::vector<v8::Local<v8::Value>> argv; //TODO: can info be passed directly here?
        for (int i = 0; i < argc; ++i) argv.push_back(info[i]);
        v8::Local<v8::Context> context = iso->GetCurrentContext();
        v8::Local<v8::Function> consobj = v8::Local<v8::Function>::New(iso, constructor);
        v8::Local<v8::Object> result = consobj->NewInstance(context, argc, argv.data()).ToLocalChecked();
        info.GetReturnValue().Set(result);
    }
}


#if 0
NAN_PROPERTY_GETTER(GpuCanvas_js::get_pivot)
//void GpuCanvas_js::get_pivot(v8::Local<v8::String> property, Nan::NAN_PROPERTY_GETTER_ARGS_TYPE info)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
myprintf(1, "pivot was = %d" ENDCOLOR, canvas->inner.WantPivot);
    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.WantPivot));
}

NAN_PROPERTY_SETTER(GpuCanvas_js::set_pivot)
//void GpuCanvas_js::set_pivot(v8::Local<v8::String> property, v8::Local<v8::Value> value, Nan::NAN_PROPERTY_SETTER_ARGS_TYPE info)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
    canvas->inner.WantPivot = value->BooleanValue();
myprintf(1, "pivot is now = %d" ENDCOLOR, canvas->inner.WantPivot);
//return_void(errjs(iso, "GpuCanvas.paint: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}
#endif

#if 0
NAN_GETTER(GpuCanvas_js::get_pivot)
//void GpuCanvas_js::get_pivot(v8::Local<v8::String> property, Nan::NAN_PROPERTY_GETTER_ARGS_TYPE info)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
myprintf(1, "pivot was = %d" ENDCOLOR, canvas->inner.WantPivot);
    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.WantPivot));
}


NAN_SETTER(GpuCanvas_js::set_pivot)
//void GpuCanvas_js::set_pivot(v8::Local<v8::String> property, v8::Local<v8::Value> value, Nan::NAN_PROPERTY_SETTER_ARGS_TYPE info)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
    canvas->inner.WantPivot = value->BooleanValue();
myprintf(1, "pivot is now = %d" ENDCOLOR, canvas->inner.WantPivot);
//return_void(errjs(iso, "GpuCanvas.paint: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}
#endif


#if 1
//get width:
void GpuCanvas_js::width_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());

    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.width()));
}

//get height:
void GpuCanvas_js::height_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());

    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.height()));
}

//get/set pivot flag:
void GpuCanvas_js::pivot_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());

    info.GetReturnValue().Set(JS_BOOL(iso, canvas->inner.WantPivot)); //return old value
    if (!info[0]->IsUndefined()) canvas->inner.WantPivot = info[0]->BooleanValue();
//if (!info[0]->IsUndefined()) myprintf(1, "set pivot value %d %d %d => %d" ENDCOLOR, info[3]->BooleanValue(), info[3]->IntegerValue(), info[3]->Uint32Value(), canvas->inner.WantPivot);
//else myprintf(1, "get pivot value %d" ENDCOLOR, canvas->inner.WantPivot);
}
#endif


//xfr/xfm Javascript array to GPU:
void GpuCanvas_js::paint(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::paint) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length() != 1) return_void(errjs(iso, "GpuCanvas.paint: expected 1 param, got %d", info.Length()));
	if (!info.Length() || !info[0]->IsUint32Array()) return_void(errjs(iso, "GpuCanvas.paint: missing uint32 array param"));
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
//void* p = handle->GetAlignedPointerFromInternalField(0); 

//https://stackoverflow.com/questions/28585387/node-c-addon-how-do-i-access-a-typed-array-float32array-when-its-beenn-pa
#if 0
//    Local<Array> input = Local<Array>::Cast(args[0]);
//    unsigned int num_locations = input->Length();
//    for (unsigned int i = 0; i < num_locations; i++) {
//      locations.push_back(
//        unpack_location(isolate, Local<Object>::Cast(input->Get(i)))
  void *buffer = node::Buffer::Data(info[1]);
	v8::Local<v8::Array> new_pixels = v8::Local<v8::Array>::Cast(args[0]);
	if (new_pixels->Length() != pixels.cast->w * pixels.cast->h) return_void(noderr("Paint: pixel array wrong length: %d (should be %d)", new_pixels->Length(), pixels.cast->w * pixels.cast->h));
  Local<ArrayBuffer> buffer = ArrayBuffer::New(Isolate::GetCurrent(), size);
  Local<Uint8ClampedArray> clampedArray = Uint8ClampedArray::New(buffer, 0, size);
 void *ptr= JSTypedArrayGetDataPtr(array);
    size_t length = JSTypedArrayGetLength(array);
    glBufferData(GL_ARRAY_BUFFER, length, ptr, GL_STATIC_DRAW);    
#endif
//    v8::Local<v8::Uint32Array> ary = args[0].As<Uint32Array>();
//    Nan::TypedArrayContents<uint32_t> pixels(info[0].As<v8::Uint32Array>());
//https://github.com/casualjavascript/blog/issues/12
    v8::Local<v8::Uint32Array> aryp = info[0].As<v8::Uint32Array>();
    if (aryp->Length() != canvas->inner.num_univ * canvas->inner.univ_len) return_void(errjs(iso, "GpuCanvas.paint: array param bad length: is %d, should be %d", aryp->Length(), canvas->inner.num_univ * canvas->inner.univ_len));
    void *data = aryp->Buffer()->GetContents().Data();
    uint32_t* pixels = static_cast<uint32_t*>(data);
//myprintf(33, "js pixels 0x%x 0x%x 0x%x ..." ENDCOLOR, pixels[0], pixels[1], pixels[2]);
    if (!canvas->inner.Paint(pixels)) return_void(errjs(iso, "GpuCanvas.paint: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}


//show performance stats:
void GpuCanvas_js::stats(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
    if (!canvas->inner.stats()) return_void(errjs(iso, "GpuCanvas.stats: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}


/*
void GpuCanvas_js::release(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::release) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length()) return_void(errjs(iso, "GpuCanvas.release: expected 0 args, got %d", info.Length()));
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
    if (!canvas->inner.Release()) return_void(errjs(iso, "GpuCanvas.release: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}
*/


void GpuCanvas_js::Quit(void* ignored)
{
//    GpuCanvas::Quit();
    myprintf(22, RED_LT "js cleanup: %d instances to destroy" ENDCOLOR, all.size());
    while (all.size()) { delete all.back(); } //dtor will remove self from list
}


//#define CONST_INT(name, value)  
//  Nan::ForceSet(target, Nan::New(name).ToLocalChecked(), Nan::New(value),  
//      static_cast<PropertyAttribute>(ReadOnly|DontDelete));


//tell Node.js about my entry points:
NAN_MODULE_INIT(exports_js) //defines target
//void exports_js(v8::Local<v8::Object> exports, v8::Local<v8::Object> module)
{
//??    SDL_SetMainReady();
    v8::Isolate* iso = target->GetIsolate(); //~vm heap
//    NODE_SET_METHOD(exports, "isRPi", isRPi_js);
//    NODE_SET_METHOD(exports, "Screen", Screen_js); //TODO: property instead of method
    Nan::Export(target, "isRPi_tofix", isRPi_js);
    Nan::Export(target, "Screen_tofix", Screen_js);
//    target->SetAccessor(JS_STR(iso, "Screen"), Screen_js);
    GpuCanvas_js::Init(target);
//  Nan::SetAccessor(proto, Nan::New("fillColor").ToLocalChecked(), GetFillColor);
//    NAN_GETTER(Screen_js);
//    Nan::SetAccessor(exports, Nan::New("Screen").ToLocalChecked(), Screen_js);
//    NAN_PROPERTY_GETTER(getter_js);
//    NODE_SET_METHOD(exports, "GpuCanvas", GpuCanvas_js);
//https://github.com/Automattic/node-canvas/blob/b470ce81aabe2a78d7cdd53143de2bee46b966a7/src/CanvasRenderingContext2d.cc#L764
//    NODE_SET_METHOD(module, "exports", CreateObject);

//    CONST_INT("api_version", 1.0);
//    CONST_INT("name", "data-canvas");
//    CONST_INT("description", "GPU data canvas for WS281X");

//    Init();
//    node::AtExit(Quit_js);
}

} //namespace

NODE_MODULE(data_canvas, NodeAddon::exports_js) //tells Node.js how to find my entry points
//NODE_MODULE(NODE_GYP_MODULE_NAME, NodeAddon::exports_js)
//NOTE: can't use special chars in module name here, but bindings.gyp overrides it anyway?
#endif //def BUILDING_NODE_EXTENSION


////////////////////////////////////////////////////////////////////////////////
////
/// Standalone/test interface
//

#ifndef BUILDING_NODE_EXTENSION //not being compiled by node-gyp
// #pragma message "compiled as stand-alone"

bool eventh(int = INT_MAX); //fwd ref

uint32_t PALETTE[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};

#if 0 //small load
#define NUM_UNIV  20
#define UNIV_LEN  24
#else //full load
#define NUM_UNIV  24
#define UNIV_LEN  Screen().h
#endif


int main(int argc, const char* argv[])
{
    myprintf(1, CYAN_LT "test/standalone routine" ENDCOLOR);
//??    SDL_SetMainReady();
//    myprintf(33, "hello" ENDCOLOR);
{
    GpuCanvas canvas(0, NUM_UNIV, UNIV_LEN, false);
//    myprintf(33, "canvas opened" ENDCOLOR);
//    uint32_t pixels[NUM_UNIV * UNIV_LEN] = {0};
    std::vector<uint32_t> pixels(NUM_UNIV * UNIV_LEN); //kludge: vector allows non-const size
#if 0 //gradient fade
    static int hue = 0;
    uint32_t color = hsv2rgb((double)(hue++ % 360) / 360, 1, 1); //gradient fade on hue
    surf.FillRect(RECT_ALL, color);
#endif
#if 1 //one-by-one, palette colors
    int duration = 30; //sec
    myprintf(1, GREEN_LT "loop for %d sec (%d frames) ..." ENDCOLOR, duration, duration * 60);
    for (int xy = 0; xy < duration * 60; ++xy) //xy < 10 * 10; ++xy)
    {
        int x = (xy / UNIV_LEN) % NUM_UNIV, y = xy % UNIV_LEN; //cycle thru [0..9,0..9]
//        myprintf(33, "evth" ENDCOLOR);
        if (eventh(1)) break; //user quit
        uint32_t color = PALETTE[(x + y + xy / pixels.size()) % SIZE(PALETTE)]; //vary each cycle
//        myprintf(1, BLUE_LT "px[%d, %d] = 0x%x" ENDCOLOR, x, y, color);
        pixels[xy % pixels.size()] = color;
        double PresentTime_cpy = canvas.PresentTime; //kludge: avoid "deleted function" error on atomic
        myprintf(33, BLUE_LT "paint[%d, %d] @%2.1f msec" ENDCOLOR, x, y, PresentTime_cpy); //canvas.PresentTime);
//not needed: use lamba function as call-back (try to monitor paint latency):
//        canvas.Paint(pixels, [](void* started) { canvas.fg.usertime += SDL_GetTicks() - (uint64_t)started; }, (void*)now()); //blocks until canvas released by bkg thread
//        uint64_t latency = SDL_GetTicks();
        canvas.Paint(pixels.data()); //blocks until pixel buf released by bkg thread
//        canvas.fg.usertime += SDL_GetTicks() - latency;
        SDL_Delay(10); //15+10); //1000 / 60);
    }
    SDL_Delay(17-10); //kludge: allow last frame out before closing
#endif
    myprintf(33, "done" ENDCOLOR);
} //force GpuCanvas out of scope before delay
//    canvas.Paint(pixels);
    myprintf(1, GREEN_LT "done, wait 5 sec" ENDCOLOR);
    SDL_Delay(5000);
    return 0;
}


//handle windowing system events:
//NOTE: SDL_Event handling loop must be in main() thread
//(mainly for debug)
bool eventh(int max /*= INT_MAX*/)
{
//    myprintf(14, BLUE_LT "evth max %d" ENDCOLOR, max);
    while (max)
    {
    	SDL_Event evt;
//        SDL_PumpEvents(void); //not needed if calling SDL_PollEvent or SDL_WaitEvent
//       if (SDL_WaitEvent(&evt)) //execution suspends here while waiting on an event
		if (SDL_PollEvent(&evt))
		{
//            myprintf(14, BLUE_LT "evt type 0x%x" ENDCOLOR, evt.type);
			if (evt.type == SDL_QUIT) return true; //quit = true; //return;
			if (evt.type == SDL_KEYDOWN)
            {
				myprintf(14, CYAN_LT "got key down 0x%x" ENDCOLOR, evt.key.keysym.sym);
				if (evt.key.keysym.sym == SDLK_ESCAPE) return true; //quit = true; //return; //key codes defined in /usr/include/SDL2/SDL_keycode.h
			}
			if (evt.type == SDL_PRESSED)
            {
				myprintf(14, CYAN_LT "got key press 0x%x" ENDCOLOR, evt.key.keysym.sym);
				if (evt.key.keysym.sym == SDLK_ESCAPE) return true; //quit = true; //return; //key codes defined in /usr/include/SDL2/SDL_keycode.h
			}
            if (SDL.cast->evtid && (evt.type == SDL.cast->evtid)) //custom event: printf from bkg thread
            {
                const char* buf = reinterpret_cast<const char*>(evt.user.data1);
                fputs(buf, stderr);
                delete[] buf;
            }
/*
            if (kbhit())
            {
                char ch = getch();
                myprintf(14, "char 0x%x pressed\n", ch);
            }
*/
		}
//        const Uint8* keyst = SDL_GetKeyboardState(NULL);
//        if (keyst[SDLK_RETURN]) { myprintf(8, CYAN_LT "Return key pressed." ENDCOLOR); return true; }
//        if (keyst[SDLK_ESCAPE]) { myprintf(8, CYAN_LT "Escape key pressed." ENDCOLOR); return true; }
        if (max != INT_MAX) --max;
//        myprintf(14, BLUE_LT "no evts" ENDCOLOR);
    }
    return false; //no evt
}
#endif //ndef BUILDING_NODE_EXTENSION


////////////////////////////////////////////////////////////////////////////////
////
/// Graphics info debug
//

#undef debug_info

//SDL lib info:
void debug_info(SDL_lib* ignored, int where)
{
    myprintf(1, CYAN_LT "Debug detail level = %d (from %d)" ENDCOLOR, WANT_LEVEL, where);

    SDL_version ver;
    SDL_GetVersion(&ver);
    myprintf(12, BLUE_LT "SDL version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, isRPi? %d" ENDCOLOR, ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());

    myprintf(12, BLUE_LT "%d video driver(s):" ENDCOLOR, SDL_GetNumVideoDrivers());
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
        myprintf(12, BLUE_LT "Video driver[%d/%d]: name '%s'" ENDCOLOR, i, SDL_GetNumVideoDrivers(), SDL_GetVideoDriver(i));
}


//SDL window info:
void debug_info(SDL_Window* window, int where)
{
#if 0
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    myprintf(20, BLUE_LT "GL_VERSION: %s" ENDCOLOR, glGetString(GL_VERSION));
    myprintf(20, BLUE_LT "GL_RENDERER: %s" ENDCOLOR, glGetString(GL_RENDERER));
    myprintf(20, BLUE_LT "GL_SHADING_LANGUAGE_VERSION: %s" ENDCOLOR, glGetString(GL_SHADING_LANGUAGE_VERSION));
    myprintf(20, "GL_EXTENSIONS: %s" ENDCOLOR, glGetString(GL_EXTENSIONS));
    SDL_GL_DeleteContext(gl_context);
#endif

    int wndw, wndh;
    SDL_GL_GetDrawableSize(window, &wndw, &wndh);
//        return err(RED_LT "Can't get drawable window size" ENDCOLOR);
    uint32_t fmt = SDL_GetWindowPixelFormat(window);
//    if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_LT "Can't get window format" ENDCOLOR));
    uint32_t flags = SDL_GetWindowFlags(window);
    std::ostringstream desc;
    if (flags & SDL_WINDOW_FULLSCREEN) desc << ";FULLSCR";
    if (flags & SDL_WINDOW_OPENGL) desc << ";OPENGL";
    if (flags & SDL_WINDOW_SHOWN) desc << ";SHOWN";
    if (flags & SDL_WINDOW_HIDDEN) desc << ";HIDDEN";
    if (flags & SDL_WINDOW_BORDERLESS) desc << ";BORDERLESS";
    if (flags & SDL_WINDOW_RESIZABLE) desc << ";RESIZABLE";
    if (flags & SDL_WINDOW_MINIMIZED) desc << ";MIN";
    if (flags & SDL_WINDOW_MAXIMIZED) desc << ";MAX";
    if (flags & SDL_WINDOW_INPUT_GRABBED) desc << ";GRABBED";
    if (flags & SDL_WINDOW_INPUT_FOCUS) desc << ";FOCUS";
    if (flags & SDL_WINDOW_MOUSE_FOCUS) desc << ";MOUSE";
    if (flags & SDL_WINDOW_FOREIGN) desc << ";FOREIGN";
    if (!desc.tellp()) desc << ";";
    myprintf(12, BLUE_LT "window %dx%d, fmt %i bpp %s %s (from %d)" ENDCOLOR, wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1, where);

#if 0
    myprintf(22, BLUE_LT "SDL_WINDOW_FULLSCREEN    [%c]" ENDCOLOR, (flags & SDL_WINDOW_FULLSCREEN) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_OPENGL        [%c]" ENDCOLOR, (flags & SDL_WINDOW_OPENGL) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_SHOWN         [%c]" ENDCOLOR, (flags & SDL_WINDOW_SHOWN) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_HIDDEN        [%c]" ENDCOLOR, (flags & SDL_WINDOW_HIDDEN) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_BORDERLESS    [%c]" ENDCOLOR, (flags & SDL_WINDOW_BORDERLESS) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_RESIZABLE     [%c]" ENDCOLOR, (flags & SDL_WINDOW_RESIZABLE) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_MINIMIZED     [%c]" ENDCOLOR, (flags & SDL_WINDOW_MINIMIZED) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_MAXIMIZED     [%c]" ENDCOLOR, (flags & SDL_WINDOW_MAXIMIZED) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_INPUT_GRABBED [%c]" ENDCOLOR, (flags & SDL_WINDOW_INPUT_GRABBED) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_INPUT_FOCUS   [%c]" ENDCOLOR, (flags & SDL_WINDOW_INPUT_FOCUS) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_MOUSE_FOCUS   [%c]" ENDCOLOR, (flags & SDL_WINDOW_MOUSE_FOCUS) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_FOREIGN       [%c]" ENDCOLOR, (flags & SDL_WINDOW_FOREIGN) ? 'X' : ' '); 
#endif

//NO
//    SDL_Surface* wnd_surf = SDL_GetWindowSurface(window); //NOTE: wnd will dealloc, so don't need auto_ptr here
//    if (!wnd_surf) return_void(err(RED_LT "Can't get window surface" ENDCOLOR));
//NOTE: wnd_surf info is gone after SDL_CreateRenderer! (benign if info was saved already)
//    debug_info(wnd_surf);
}


//SDL_Renderer info:
void debug_info(SDL_Renderer* renderer, int where)
{
    myprintf(12, BLUE_LT "%d render driver(s): (from %d)" ENDCOLOR, SDL_GetNumRenderDrivers(), where);
    for (int i = 0; i <= SDL_GetNumRenderDrivers(); ++i)
    {
        SDL_RendererInfo info;
        std::ostringstream which, fmts, count, flags;
        if (!i) which << "active";
        else which << i << "/" << SDL_GetNumRenderDrivers();
        if (!OK(i? SDL_GetRenderDriverInfo(i - 1, &info): SDL_GetRendererInfo(renderer, &info))) { err(RED_LT "Can't get renderer[%s] info" ENDCOLOR, which.str().c_str()); continue; }
        if (info.flags & SDL_RENDERER_SOFTWARE) flags << ";SW";
        if (info.flags & SDL_RENDERER_ACCELERATED) flags << ";ACCEL";
        if (info.flags & SDL_RENDERER_PRESENTVSYNC) flags << ";VSYNC";
        if (info.flags & SDL_RENDERER_TARGETTEXTURE) flags << ";TOTXR";
        if (info.flags & ~(SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE)) flags << ";????";
        if (!flags.tellp()) flags << ";";
        for (unsigned int i = 0; i < info.num_texture_formats; ++i) fmts << ", " << SDL_BITSPERPIXEL(info.texture_formats[i]) << " bpp " << skip(SDL_GetPixelFormatName(info.texture_formats[i]), "SDL_PIXELFORMAT_");
        if (!info.num_texture_formats) { count << "no fmts"; fmts << "  "; }
        else if (info.num_texture_formats != 1) count << info.num_texture_formats << " fmts: ";
        myprintf(12, BLUE_LT "Renderer[%s]: '%s', flags 0x%x %s, max %dx%d, %s%s" ENDCOLOR, which.str().c_str(), info.name, info.flags, flags.str().c_str() + 1, info.max_texture_width, info.max_texture_height, count.str().c_str(), fmts.str().c_str() + 2);
    }
}


//SDL_Surface info:
void debug_info(SDL_Surface* surf, int where)
{
    int numfmt = 0;
    std::ostringstream fmts, count;
    for (SDL_PixelFormat* fmtptr = surf->format; fmtptr; fmtptr = fmtptr->next, ++numfmt)
        fmts << ";" << SDL_BITSPERPIXEL(fmtptr->format) << " bpp " << SDL_PixelFormatShortName(fmtptr->format);
    if (!numfmt) { count << "no fmts"; fmts << ";"; }
    else if (numfmt != 1) count << numfmt << " fmts: ";
//    if (want_fmts && (numfmt != want_fmts)) err(RED_LT "Unexpected #formats: %d (wanted %d)" ENDCOLOR, numfmt, want_fmts);
    if (!surf->pixels || (toint(surf->pixels) & 7)) err(RED_LT "Surface pixels not aligned on 8-byte boundary: 0x%x" ENDCOLOR, toint(surf->pixels));
    if ((size_t)surf->pitch != sizeof(uint32_t) * surf->w) err(RED_LT "Unexpected pitch: %d should be %zu * %d = %zu" ENDCOLOR, surf->pitch, sizeof(uint32_t), surf->w, sizeof(uint32_t) * surf->w);
    myprintf(18, BLUE_LT "Surface 0x%x: %d x %d, pitch %s, size %s, %s%s (from %d)" ENDCOLOR, toint(surf), surf->w, surf->h, commas(surf->pitch), commas(surf->h * surf->pitch), count.str().c_str(), fmts.str().c_str() + 1, where);
}


////////////////////////////////////////////////////////////////////////////////
////
/// Graphics helpers
//

//get max window size that will fit on screen:
//try to maintain 4:3 aspect ratio
#define VPAD  (3 * 24) //kludge: allow room for top and bottom app bars + window bar
WH MaxFit()
{
    WH wh = Screen();
    wh.h = std::min((uint16_t)(wh.h - VPAD), (uint16_t)(wh.w * 3 * 24 / 4 / 23.25));
    wh.w = std::min(wh.w, (uint16_t)(wh.h * 4 / 3));
    return wh;
}


///////////////////////////////////////////////////////////////////////////////
////
/// color helpers
//

uint32_t limit(uint32_t color)
{
#ifdef LIMIT_BRIGHTNESS
// #pragma message "limiting R+G+B brightness to " TOSTR(LIMIT_BRIGHTNESS)
    unsigned int r = R(color), g = G(color), b = B(color);
    unsigned int sum = r + g + b; //max = 3 * 255 = 765
    if (sum > LIMIT_BRIGHTNESS) //reduce brightness, try to keep relative colors
    {
//GLuint sv = color;
        r = rdiv(r * LIMIT_BRIGHTNESS, sum);
        g = rdiv(g * LIMIT_BRIGHTNESS, sum);
        b = rdiv(b * LIMIT_BRIGHTNESS, sum);
        color = (color & Amask(0xFF)) | Rmask(r) | Gmask(g) | Bmask(b);
//printf("REDUCE: 0x%x, sum %d, R %d, G %d, B %d => r %d, g %d, b %d, 0x%x\n", sv, sum, R(sv), G(sv), B(sv), r, g, b, color);
    }
#endif //def LIMIT_BRIGHTNESS
    return color;
}


//based on http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl:
uint32_t hsv2rgb(float h, float s, float v)
{
//    static const float K[4] = {1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0};
    float p[3] = {_abs(_fract(h + 1.) * 6 - 3), _abs(_fract(h + 2/3.) * 6 - 3), _abs(_fract(h + 1/3.) * 6 - 3)};
    float rgb[3] = {v * _mix(h, _clamp(p[0] - 1, 0., 1.), s), v * _mix(h, _clamp(p[1] - 1, 0., 1.), s), v * _mix(1, _clamp(p[2] - 1, 0., 1.), s)};
    uint32_t color = toARGB(255, 255 * rgb[0], 255 * rgb[1], 255 * rgb[2]);
//    myprintf(33, BLUE_LT "hsv(%f, %f, %f) => argb 0x%x" ENDCOLOR, h, s, v, color);
    return color;
}


//TODO?
//uint32_t ARGB2ABGR(uint32_t color)
//{
//    return color;
//}


///////////////////////////////////////////////////////////////////////////////
////
/// Chipiplexing encoder
//

#if 0
	Private Sub ChipiplexedEvent(ByRef channelValues As Byte())
		on error goto errh
		If channelValues.Length <> Me.m_numch Then Throw New Exception(String.Format("event received {0} channels but configured for {1}", channelValues.Length, Me.m_numch))
		Me.total_inlen += channelValues.Length
		Me.total_evts += 1
''TODO: show more or less data
		If Me.m_trace Then LogMsg(String.Format("Chipiplex[{0}] {1} bytes in: ", Me.total_evts, channelValues.Length) & DumpHex(channelValues, channelValues.Length, 80))
''format output buf for each PIC (groups of up to 56 chipiplexed channels):
		Dim buflen As Integer, protolen As Integer 
		Dim overflow_retry As Integer, overflow_adjust As Integer, merge_retry As Integer 
		Dim brlevel As Integer, brinx As Integer, row As Integer, col As Integer 
		Dim i As Integer
		Dim pic As Integer
''NOTE to style critics: I hate line numbers but I also hate cluttering up source code with a lot of state/location info
''I''m only using line#s because they are the most compact way to show the location of errors (kinda like C++''s __LINE__ macro), and there is no additional overhead
12:
''send out data to PICs in *reverse* order (farthest PICs first, closest PICs last):
''this will allow downstream PICs to get their data earlier and start processing while closer PICs are getting their data
''if we send data to closer PICs first, they will have processed all their channels before downstream PICs even get theirs, which could lead to partially refreshed frames
''at 115k baud, 100 chars take ~ 8.7 mec, which is > 1 AC half-cycle at 60 Hz; seems like this leads to slight sync problems
''TODO: make forward vs. reverse processing a config option
		For pic = Me.m_numch - (Me.m_numch - 1) Mod 56 To 1 Step -56 ''TODO: config #channels per PIC?
			Dim status As String = "" ''show special processing flags in trace file
			Dim numbr As Integer = 0 ''#brightness levels
			Dim numrows As Integer = 0, numcols As Integer ''#rows, columns occupied for this frame
			For i = 0 To 255 ''initialize brightness level index to all empty
				Me.m_brindex(i) = 0 ''empty
			Next i
14:
			Dim ch As Integer
			For ch = pic To pic+55 ''each channel for this PIC (8*7 = 56 for a regular Renard PIC); 1-based
				If ch > Me.m_numch Then Exit For
				brlevel = channelValues(ch-1)
16:
				If brlevel < Me.m_minbright Then ''don''t need to send this channel to controller (it''s off)
					If brlevel = 0 then
						Me.total_realnulls += 1
					Else
						Me.total_nearnulls += 1
					End If
				Else ''need to send this channel to the controller (it''s not off)
18:
					Dim overflow_limit As Integer = Me.m_closeness ''allow overflow into this range of values
					If brlevel >= Me.m_maxbright Then ''treat as full on
						If brlevel < 255 Then Me.total_nearfulls += 1
						overflow_limit += Me.m_fullbright - Me.m_maxbright ''at full-on, expand the allowable overflow range
						brlevel = Me.m_fullbright ''255
					End If
					row = (ch - pic)\7 ''row# relative to this PIC (0..7); do not round up
					col = (ch - pic) Mod 7 ''column# relative to this row
					If col >= row Then col += 1 ''skip over row address line (chipiplexing matrix excludes diagonal row/column down the middle)
#If BOARD1_INCOMPAT Then ''whoops; wired the pins differently between boards; rotate them here rather than messing with channel reordering in Vixen
''					Dim svch As Integer = ch - pic: ch = 0
''					If svch And &h80 Then ch += &h10
''					If svch And &h40 Then ch += 1
''					If svch And &h20 Then ch += &h40
''					If svch And &h10 Then ch += &h20
''					If svch And 8 Then ch += &h80
''					If svch And 4 Then ch += 8
''					If svch And 2 Then ch += 4
''					If svch And 1 Then ch += 2
''					ch += pic
					Select Case row
						Case 0: row = 4''3
						Case 1: row = 2''7
						Case 2: row = 3''1
						Case 3: row = 0''2
						Case 4: row = 5''0
						Case 5: row = 6''4
						Case 6: row = 7''5
						Case 7: row = 1''6
					End Select
					Select Case col
						Case 0: col = 4''3
						Case 1: col = 2''7
						Case 2: col = 3''1
						Case 3: col = 0''2
						Case 4: col = 5''0
						Case 5: col = 6''4
						Case 6: col = 7''5
						Case 7: col = 1''6
					End Select
#End If
20:
					For overflow_retry = 0 To 2 * overflow_limit
						If (overflow_retry And 1) = 0 Then ''try next higher (brighter) level
							overflow_adjust = brlevel + overflow_retry\2 ''do not round up
							If overflow_adjust > Me.m_fullbright Then Continue For
						Else ''try next lower (dimmer) value
							overflow_adjust = brlevel - overflow_retry\2 ''do not round up
							If overflow_adjust < Me.m_minbright Then Continue For
						End If
22:
						brinx = Me.m_brindex(overflow_adjust)
						If brinx = 0 Then ''allocate a new brightness level
							If Me.IsRenardByte(overflow_adjust) Then Me.num_avoids += 1: status &= "@": Continue For ''avoid special protocol chars
							numbr += 1: brinx = numbr ''entry 0 is used as an empty; skip it
							Me.m_brindex(overflow_adjust) = brinx ''indexing by brightness level automatically sorts them by brightness
''							Me.m_levels(brinx).level = brlevel ''set brightness level for this entry
							Me.m_rowindex(brinx) = 0 ''no rows yet for this brightness level
''							Me.m_levels(brinx).numrows = 0
							For i = 0 To 7 ''no columns yet for this brightness level
								Me.m_columns(brinx, i) = 0
							Next i
						End If
24:
						If Me.m_columns(brinx, row) = 0 Then ''allocate a new row
''							If Me.m_levels(brinx).numrows >= Me.m_maxrpl Then Continue For ''this level is full; try another one that is close (lossy dimming)
''enforce maxRPL here to avoid over-fillings brightness levels; lossy dimming will degrade with multiple full levels near each other
							If Me.NumBits(Me.m_rowindex(brinx)) >= Me.m_maxrpl Then Continue For ''this level is full; try another one that is close (lossy dimming)
''maxRPL can''t be more than 2 in this version, so we don''t need to check for reserved bytes here (the only one that matters has 7 bits on)
''							If Me.IsRenardByte(overflow_adjust) Then Me.num_avoids += 1: status &= "@": Continue For ''avoid special protocol chars
							Me.m_rowindex(brinx) = Me.m_rowindex(brinx) Or 1<<(7 - row)
''							Me.m_levels(brinx).numrows += 1
							numrows += 1
						End If
						Dim newcols As Byte = Me.m_columns(brinx, row) Or 1<<(7 - col)
						If Me.IsRenardByte(newcols) Then Me.num_avoids += 1: status &= "@": Continue For ''avoid special protocol chars; treat as full row
						Me.m_columns(brinx, row) = newcols ''Me.m_columns(brinx, row) Or 1<<(7 - col)
						If overflow_retry <> 0 Then Me.total_rowsoverflowed += 1: status &= "*"
						Exit For
					Next overflow_retry
26:
					If overflow_retry > 2 * overflow_limit Then
						If Me.m_nodiscard Then Throw New Exception(String.Format("Unable to overflow full row after {0} tries.", 2 * overflow_limit))
						Me.total_rowsdropped += 1
						status &= "-"
					End If
				End If
			Next ch
28:
#If False Then ''obsolete merge/reduction code
''			Dim brlimit As Integer = Me.m_maxbright + 8\Me.m_maxrpl - 1 ''allow for overflow of max bright level
''			If brlimit > 255 Then brlimit = 255 ''don''t overshoot real top end of brightness range
			For merge_retry = 0 To Me.m_closeness
				If 2+2 + 2*numbr + numrows <= Me.m_outbuf.Length Then Exit For ''no need to merge brightness levels
				If merge_retry = 0 Then Continue For ''dummy loop entry to perform length check first time
				For brlevel = Me.m_minbright To Me.m_fullbright 
					Dim tobrinx As Byte = Me.m_brindex(brlevel)
					If tobrinx = 0 Then Continue For
					Dim otherlevel As Integer 
					Dim mergelimit As Integer = brlevel + merge_retry
					if brlevel >= Me.m_maxbright Then mergelimit = 255
30:
					For otherlevel = brlevel - merge_retry To merge_limit Step 2*merge_retry
						If (otherlevel < Me.m_minbright) Or (otherlevel > brlimit) Then Continue For
						Dim frombrinx As Byte = Me.m_brindex(otherlevel)
						If (frombrinx = 0) or (Me.numbits(Me.m_rowindex(tobrinx) And Me.m_rowindex(frombrinx)) = 0) Then continue for ''can''t save some space by coalescing
						numrows -= Me.numbits(Me.m_rowindex(tobrinx) And Me.m_rowindex(frombrinx))
						Me.m_rowindex(tobrinx) = Me.m_rowindex(tobrinx) Or Me.m_rowindex(frombrinx) ''merge row index
						For row = 0 To 7 ''merge columns
							Me.m_columns(tobrinx, row) = Me.m_columns(tobrinx, row) Or Me.m_columns(frombrinx, row)
						Next row
32:
						Me.m_brindex(otherlevel) = 0 ''hide entry so it won''t be used again
						numbr -= 1
						Me.total_levelsmerged += 1
						status &= "^"
						If 2+2 + 2*numbr + numrows <= Me.m_outbuf.Length Then Exit For ''no need to coalesce more brightness levels
						Continue For
					Next otherlevel
				Next brlevel
			Next merge_retry
34:
			If merge_retry > Me.m_closeness Then
				If Me.m_nodiscard Then Throw New Exception(String.Format("Unable to merge brightness level after {0} tries.", 2*Me.m_closeness))
				Me.total_levelsdropped += 1
				status &= "X"
			End If
#End If
			Dim prevlevel As Integer = 0, prevbrinx As Integer 
''TODO: use MaxData length here
			While 3+2 + 2*numbr + numrows > Me.m_outbuf.Length ''need to merge brightness levels
''this is expensive, so config params should be set to try to avoid it
''TODO: this could be smarter; prioritize and then merge rows that are closest or sparsest first
				For brlevel = Me.m_minbright To Me.m_fullbright 
					brinx = Me.m_brindex(brlevel)
					If brinx = 0 Then Continue For
					If prevlevel < 256 Then ''merge row with previous to save space
30:
						If prevlevel = 0 Then prevlevel = brlevel: prevbrinx = brinx: Continue For ''need 2 levels to compare
						If (prevlevel + Me.m_closeness < brlevel) And (prevlevel + Me.m_closeness < Me.m_maxbright) Then Continue For ''too far apart to merge
						Dim newrows As Byte = Me.m_rowindex(brinx) Or Me.m_rowindex(prevbrinx) ''merge row index
						If Me.NumBits(newrows) > Me.m_maxrpl Then
''							If Not String.IsNullOrEmpty(Me.m_logfile) Then LogMsg("row " & brlevel & "->" & brinx & " full; can''t merge")
							Continue For ''can''t merge; row is already full
						End If
						If Me.IsRenardByte(newrows) Then Me.num_avoids += 1: status &= "@": Continue For ''can''t merge (would general special protocol chars)
						Dim newcols(8 - 1) As Byte
						For row = 0 To 7 ''merge columns
							newcols(row) = Me.m_columns(brinx, row) Or Me.m_columns(prevbrinx, row)
							If Me.IsRenardByte(newcols(row)) Then Me.num_avoids += 1: status &= "@": Exit For ''can''t merge (would general special protocol chars)
						Next row
						If row <= 7 Then Continue For ''can''t merge (would general special protocol chars)
						numrows -= Me.NumBits(Me.m_rowindex(brinx) And Me.m_rowindex(prevbrinx))
						Me.m_rowindex(brinx) = newrows ''Me.m_rowindex(brinx) Or Me.m_rowindex(prevbrinx) ''merge row index
						For row = 0 To 7 ''merge columns
							Me.m_columns(brinx, row) = newcols(row) ''Me.m_columns(brinx, row) Or Me.m_columns(prevbrinx, row)
						Next row
						If Me.m_trace Then LogMsg(String.Format("merged level {0} with {1}", prevlevel, brlevel))
						Me.m_brindex(prevlevel) = 0 ''hide this entry so it won''t be used again
						Me.total_levelsmerged += 1
						status &= "^"
					Else ''last try; just drop the row
						If Me.m_trace Then LogMsg(String.Format("dropped level {0}", brlevel))
						Me.m_brindex(brlevel) = 0 ''hide this entry so it won''t be used again
						numrows -= Me.NumBits(Me.m_rowindex(brinx))
						Me.total_levelsdropped += 1
						status &= "!"
					End If
32:
					numbr -= 1
					If 3+2 + 2*numbr + numrows <= Me.m_outbuf.Length Then Exit While ''no need to merge/drop more brightness levels
				Next brlevel
				If Me.m_nodiscard Then Throw New Exception(String.Format("Unable to merge brightness level after {0} tries.", 2*Me.m_closeness))
				prevlevel = 256 ''couldn''t find rows to merge, so start dropping them
34:
			End While
''TODO: resend data after a while even if it hasn''t changed?
''no, unless dup:			If numbr < 1 Then Continue For ''no channels to send this PIC
''format data to send to PIC:
''TODO: send more than 1 SYNC the first time, in case baud rate not yet selected
			buflen = 0
			protolen = 0
''			If pic = 1 Then ''only needed for first PIC in chain?  TODO
				Me.m_outbuf(buflen) = Renard_SyncByte: buflen += 1 ''&h7E
				protolen += 1
''			End If
			Me.m_outbuf(buflen) = Renard_CmdAdrsByte + pic\56: buflen += 1 ''&h80
''			Me.m_outbuf(buflen) = Renard_PadByte: buflen += 1 ''&h7D ''TODO?
			Me.m_outbuf(buflen) = Me.m_cfg: buflen += 1 ''config byte comes first
			protolen += 2
			For brlevel = Me.m_fullbright To Me.m_minbright Step -1 ''send out brightess levels in reverse order (brightest first)
36:
				brinx = Me.m_brindex(brlevel)
				If brinx = 0 Then Continue For
38:
				If (brlevel = 0) Or (Me.m_rowindex(brinx) = 0) Then SentBadByte(IIf(brlevel = 0, brlevel, Me.m_rowindex(brinx)), buflen, pic\56) ''paranoid
				If Me.IsRenardByte(brlevel) Then SentBadByte(brlevel, buflen, pic\56) ''paranoid
				If buflen < m_outbuf.Length Then Me.m_outbuf(buflen) = brlevel ''send brightness level; avoid outbuf overflow
				buflen += 1 ''maintain count, even if outbuf overflows (so we know how bad is was)
				If Me.IsRenardByte(Me.m_rowindex(brinx)) Then SentBadByte(Me.m_rowindex(brinx), buflen, pic\56) ''paranoid
				If buflen < m_outbuf.Length Then Me.m_outbuf(buflen) = Me.m_rowindex(brinx) ''send row index byte; avoid outbuf overflow
				buflen += 1 ''maintain count, even if outbuf overflows (so we know how bad is was)
40:
				For row = 0 To 7
''					If buflen >= m_outbuf.Length Then LogMsg("ERROR: outbuf overflow at row " & row & " of brinx " & brinx & ", brlevel " & brlevel & ", thought I needed 4+2*" & numbr & "+" & numrows & "=" & (2+2 + 2*numbr + numrows) & ", outbuf so far is " & buflen & ":" & DumpHex(Me.m_outbuf, buflen, 80))
''					If Not String.IsNullOrEmpty(Me.m_logfile) Then LogMsg("err 9 debug: brinx " & brinx & ", row " & row & ", buflen " & buflen)
					If Me.m_columns(brinx, row) <> 0 Then ''send columns for this row
						If Me.IsRenardByte(Me.m_columns(brinx, row)) Then SentBadByte(Me.m_columns(brinx, row), buflen, pic\56) ''paranoid
						If buflen < m_outbuf.Length Then Me.m_outbuf(buflen) = Me.m_columns(brinx, row) ''avoid outbuf overflow
						buflen += 1 ''maintain count, even if outbuf overflows (so we know how bad is was)
					End If
					numcols += Me.NumBits(Me.m_columns(brinx, row))
				Next row
			Next brlevel
42:
			If buflen < m_outbuf.Length Then Me.m_outbuf(buflen) = 0: ''end of list indicator; avoid outbuf overflow
			buflen += 1 ''maintain count, even if outbuf overflows (so we know how bad is was)
''			If pic = 1 Then ''last packet; send sync to kick out of packet receive loop (to allow console/debug commands); send this on all, in case last one is dropped as a dup
''TODO: trailing Sync might not be needed any more
				If buflen < m_outbuf.Length Then Me.m_outbuf(buflen) = Renard_SyncByte: ''&h7E; avoid outbuf overflow
				buflen += 1 ''maintain count, even if outbuf overflows (so we know how bad is was)
				protolen += 1
''			End If
			If buflen > m_outbuf.Length Then ''outbuf overflowed; this is a bug if it happens
				Throw New Exception(String.Format("ERROR: outbuf overflow, thought I needed 5+2*{0}+{1}={2}, but really needed {3}:", numbr, numrows, 3+2 + 2*numbr + numrows, buflen) & DumpHex(Me.m_outbuf, Math.Min(buflen, Me.m_outbuf.Length), 80))
			End If
			If (pic\56 = Me.m_bufdedup) And (buflen = Me.m_prevbuflen) Then ''compare current buffer to previous buffer
				For i = 1 To buflen
					If Me.m_outbuf(i-1) <> Me.m_prevbuf(i-1) Then Exit For ''need to send output buffer
				Next i
				If i > buflen Then ''outbuf was same as last time; skip it
					Me.num_dups += 1
					If Me.m_trace Then LogMsg(String.Format("Chipiplex[{0}]= duplicate buffer on {1} discarded", Me.total_evts, Me.m_bufdedup))
					Continue For
				End If
			End If
			Me.total_outlen += buflen
			Me.protocol_outlen += protolen
			Me.total_levels += numbr
			Me.total_rows += numrows
			Me.total_columns += numcols
			If (numbr > 0) And (numbr < Me.min_levels) Then Me.min_levels = numbr
			If numbr > Me.max_levels Then Me.max_levels = numbr
			If (numrows > 0) And (numrows < Me.min_rows) Then Me.min_rows = numrows
			If numrows > Me.max_rows Then Me.max_rows = numrows
			If (numcols > 0) And (numcols < Me.min_columns) Then Me.min_columns = numcols
			If numcols > Me.max_columns Then Me.max_columns = numcols
			If (buflen > 0) And (buflen < Me.min_outlen) Then Me.min_outlen = buflen
			If buflen > Me.max_outlen Then Me.max_outlen = buflen
44:
''TODO: append multiple bufs together, and only write once
			Me.m_selectedPort.Write(Me.m_outbuf, 0, buflen)
			If pic\56 = Me.m_bufdedup Then ''save current buffer to compare to next time
				For i = 1 To buflen 
					Me.m_prevbuf(i-1) = Me.m_outbuf(i-1)
				Next i
				Me.m_prevbuflen = buflen
			End If
46:
			If Me.m_trace Then LogMsg(String.Format("Chipiplex[{0}]{1} {2}+{3} bytes out: ", Me.total_evts, status, protolen, buflen - protolen) & DumpHex(Me.m_outbuf, buflen, 80))
		Next pic
		Exit Sub
	errh:
		ReportError("ChipiplexedEvent")
	End Sub
#End If
#endif


////////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions
//

//display a message:
//popup + console for dev/debug only
//#undef printf //want real printf from now on
void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...)
{
//    bool want_popup = (reason != NOERROR);
    std::string details; //need to save err msg in case error below changes it
    if (reason != NOERROR)
    {
//        if (!reason) reason = SDL_GetError();
        if (!reason || !reason[0]) reason = "(no details)";
        details = ": "; details += reason; //need to save err msg in case MessageBox error below changes it
    }
    char fmtbuf[500];
    va_list args;
    va_start(args, fmt);
    size_t needlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args);
    va_end(args);
//NOTE: assumea all Escape chars are for colors codes
//find last color code:
    size_t lastcolor_ofs = strlen(fmtbuf); //0;
    for (const char* bp = fmtbuf; (bp = strchr(bp, *ANSI_COLOR("?"))); lastcolor_ofs = bp++ - fmtbuf);
//find insertion point for details:
    char srcline[4]; //too short, but doesn't matter
    strncpy(srcline, ENDCOLOR, sizeof(srcline));
    *strpbrk(srcline, "0123456789") = '\0'; //trim after numeric part
    size_t srcline_ofs = strstr(fmtbuf, srcline) - fmtbuf; //, srcline_len);
    if (!srcline_ofs || (srcline_ofs > lastcolor_ofs)) srcline_ofs = lastcolor_ofs;
//    if (!lastcolor_ofs) lastcolor_ofs = strlen(fmtbuf);
//    printf("err: '%.*s', ofs %zu, reason: '%s'\n", strlinelen(fmt), fmt, ofs, reason.c_str());
//show warning if fmtbuf too short:
    std::ostringstream tooshort;
    if (needlen >= sizeof(fmtbuf)) tooshort << " (fmt buf too short: needed " << needlen << " bytes) ";
//get abreviated thread name:
//    const char* thrname = ThreadName(THIS_THREAD);
//    const char* lastchar = thrname + strlen(thrname) - 1;
//    char shortname[3] = "";
//    if (strstr(thrname, "no name")) strcpy(shortname, "M");
//    else if (strstr(thrname, "unknown")) strcpy(shortname, "?");
//    else { if (!isdigit(*lastchar)) ++lastchar; shortname[0] = thrname[0]; strcpy(shortname + 1, lastchar); }
//    bool locked = console.busy/*.cast*/ && OK(SDL_LockMutex(console.busy/*.cast*/)); //can't use auto_ptr<> (recursion); NOTE: mutex might not be created yet
//insert details, fmtbuf warning (if applicable), and abreviated thread name:
    FILE* stm = ((dest == stdpopup) || (dest == stdexc))? stderr: dest;
//TODO: DRY printf; rework to -> file -> optional targets (console, screen, exc)?
    /*if (stm == stderr)*/ fprintf(stdlog(), "%.*s%s%.*s%s!%d%s\n", (int)srcline_ofs, fmtbuf, details.c_str(), (int)(lastcolor_ofs - srcline_ofs), fmtbuf + srcline_ofs, tooshort.str().c_str(), /*shortname*/ Thread::isBkgThread(), fmtbuf + lastcolor_ofs);
    if (Thread::isBkgThread()) //&& (stm == stderr)) //output will be lost; send it elsewhere
    {
        int buflen = needlen + details.length() + tooshort.tellp() + /*strlen(shortname)*/ 2 + 2+1;
        char* fwdbuf = new char[buflen];
        snprintf(fwdbuf, buflen, "%.*s%s%.*s%s!%d%s\n", (int)srcline_ofs, fmtbuf, details.c_str(), (int)(lastcolor_ofs - srcline_ofs), fmtbuf + srcline_ofs, tooshort.str().c_str(), /*shortname*/ Thread::isBkgThread(), fmtbuf + lastcolor_ofs);
#ifdef BUILDING_NODE_EXTENSION //set by node-gyp
 #pragma message("TODO: node console.log")
#else //stand-alone (XWindows)
        if (SDL.cast->evtid) //fwd to fg thread
        {
            SDL_Event evt = {0};
            evt.type = SDL.cast->evtid;
//            event.user.code = my_event_code;
            evt.user.data1 = fwdbuf; //recipient must dealloc
//            evt.user.data2 = 0;
            if (OK(SDL_PushEvent(&evt))) { fprintf(stdlog(), "sent to evt que ok" ENDCOLOR); return NULL; }
            fprintf(stdlog(), "failed to send to evt que: %s" ENDCOLOR, SDL_GetError());
        }
#endif
//        dest = stdexc;
        return NULL; //nowhere to send; just discard
    }
    else
    {
//    static std::mutex serialize; serialize.lock(); //only main thread can send to stdout anyway
        fprintf(stm, "%.*s%s%.*s%s!%d%s\n", (int)srcline_ofs, fmtbuf, details.c_str(), (int)(lastcolor_ofs - srcline_ofs), fmtbuf + srcline_ofs, tooshort.str().c_str(), /*shortname*/ Thread::isBkgThread(), fmtbuf + lastcolor_ofs);
        if (dest != stm) fflush(stm); //make sure it gets out
//    serialize.unlock();
    }
#ifndef BUILDING_NODE_EXTENSION //set by node-gyp
    if (dest == stdexc) throw std::runtime_error(fmtbuf); //TODO: add details, etc
#endif
    if ((dest == stdpopup) || (dest == stdexc)) //want_popup)
    {
//strip ANSI color codes from string:
        std::smatch match;
        std::string nocolor;
        std::string str(fmtbuf);
        while (std::regex_search(str, match, ANSICC_re))
        {
            nocolor += match.prefix().str();
            str = match.suffix().str();
        }
        nocolor += str;
        if (!OK(SDL_ShowSimpleMessageBox(0, nocolor.c_str(), details.c_str() + 2, auto_ptr<SDL_Window>::latest)))
            printf(RED_LT "Show msg failed: %s" ENDCOLOR, SDL_GetError()); //CAUTION: recursion
    }
//    if (locked) locked = !OK(SDL_UnlockMutex(console.busy/*.cast*/)); //CAUTION: stays locked across ShowMessage(); okay if system-modal?
    return NULL; //probable failed ret val for caller
}


//insert commas into a numeric string (for readability):
//CAUTION: uses static data to preserve data after return; semaphore arbitrates a pool of 12 ret values
const char* commas(int64_t val)
{
    const int LIMIT = 4; //max #commas to insert
    static std::atomic<int> ff;
    static char buf[12][16 + LIMIT]; //allow 12 simultaneous calls (10+ needed by stats())
    static auto_ptr<SDL_sem> acquire(SDL_CreateSemaphore(SIZE(buf)));
    auto_ptr<SDL_LockedSemaphore> lock_HERE(acquire.cast); //SDL_LOCK(acquire));

    char* bufp = buf[++ff % SIZE(buf)] + LIMIT; //alloc ret val from pool; don't overwrite other values within same printf, allow space for commas
    for (int grplen = std::min(sprintf(bufp, "%ld", val), LIMIT * 3) - 3; grplen > 0; grplen -= 3)
    {
        memmove(bufp - 1, bufp, grplen);
        (--bufp)[grplen] = ',';
    }
    return bufp;
}


//check for file existence:
bool exists(const char* path)
{
    struct stat info;
    return !stat(path, &info); //file exists
}


//EOF
