C++ class (use shm)
- pix ary
- std::mutex/cond instead of SDL
- msgque<IPC> -> bkg thread

Nan wrapper

JS wrapper

//to check symbols:
//nm -gC yourLib.so
//readelf -Ws lib.so
//ldd lib.so

//TODO?
//#include <time.h>
//int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *request, struct timespec *remain);
//https://stackoverflow.com/questions/7979164/lowest-latency-notification-method-between-process-under-linux
//?? https://nodeaddons.com/streaming-data-from-c-to-node-js/
// https://github.com/paulhauner/example-async-node-addon/blob/master/async-addon/async-addon.cc
// https://github.com/freezer333/nodecpp-demo/tree/master/streaming
// https://nodeaddons.com/c-processing-from-node-js-part-4-asynchronous-addons/

//NOTE from https://stackoverflow.com/questions/1277627/overhead-of-pthread-mutexes
//about atomic: In practice, you can assume that int and other integer types no longer than int are atomic. You can also assume that pointer types are atomic
// http://axisofeval.blogspot.com/2010/11/numbers-everybody-should-know.html

//audio/real-time considerations (consider worst case): http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
//mlock/munlock: http://man7.org/linux/man-pages/man2/mlock.2.html
//lock-free (wait-free) fifo: http://moodycamel.com/blog/2013/a-fast-lock-free-queue-for-c++
// https://github.com/cameron314/concurrentqueue
//or maybe http://docs.libuv.org/en/v1.x/threading.html

//master: on end of encode: ++frnum; wake up all wkers
//wker: on end of last wker render: wake up main; encode_start();
//via mutex + cond, mutex, semaphore, rwlock, or lock-less (atomic)?


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
#include <unistd.h> //usleep
#include <stdarg.h> //varargs
#include <sys/shm.h> //shmatt, shmget, shmctl
//?? #define _MULTI_THREADED
//#include <pthread.h> //rwlock
//#include <linux/fb.h> //fb_var_screeninfo, fb_fix_screeninfo
//#include <sys/ioctl.h> //ioctl
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
#include <queue>

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

//typedef enum {No = false, Yes = true, Maybe} tristate;
enum class tristate: int {No = false, Yes = true, Maybe, Error = Maybe};

#define uint24_t  uint32_t //kludge: use pre-defined type and just ignore first byte


//kludge: need nested macros to stringize correctly:
//https://stackoverflow.com/questions/2849832/c-c-line-number
#define TOSTR(str)  TOSTR_NESTED(str)
#define TOSTR_NESTED(str)  #str

//#define CONCAT(first, second)  CONCAT_NESTED(first, second)
//#define CONCAT_NESTED(first, second)  first ## second

std::mutex atomic_mut;
//use macro so stmt can be nested within scoped lock:
#define ATOMIC(stmt)  { std::unique_lock<std::mutex> lock(atomic_mut); stmt; }


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
#define no_myprintf(...)  //nop
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
//#pragma message("Compiled for ARGB color format (hard-coded)")
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
#define RED_MSG  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_MSG  ANSI_COLOR("1;32")
#define YELLOW_MSG  ANSI_COLOR("1;33")
#define BLUE_MSG  ANSI_COLOR("1;34")
#define MAGENTA_MSG  ANSI_COLOR("1;35")
#define PINK_MSG  MAGENTA_MSG
#define CYAN_MSG  ANSI_COLOR("1;36")
#define GRAY_MSG  ANSI_COLOR("0;37")
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
//#define UNDEF_EVTID  0


//reduce verbosity:
#define SDL_PixelFormatShortName(fmt)  skip(SDL_GetPixelFormatName(fmt), "SDL_PIXELFORMAT_")
#define SDL_Ticks()  SDL_GetPerformanceCounter()
#define SDL_TickFreq()  SDL_GetPerformanceFrequency()

//timing stats:
inline uint64_t now() { return SDL_Ticks(); }
inline double elapsed(uint64_t started, int scaled = 1) { return (double)(now() - started) * scaled / SDL_TickFreq(); } //Freq = #ticks/second
//inline double elapsed_usec(uint64_t started)
//{
////    static uint64_t tick_per_usec = SDL_TickFreq() / 1000000;
//    return (double)(now() - started) * 1000000 / SDL_TickFreq(); //Freq = #ticks/second
//}


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
//typedef struct { int fd; } File;
//typedef struct { /*int dummy;*/ } SDL_lib;
typedef struct { /*uint32_t evtid*/; } SDL_lib;
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
#define debug(ptr)  myprintf(28, YELLOW_MSG "dealloc %s 0x%x" ENDCOLOR, TypeName(ptr), toint(ptr))
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
    static SDL_lib ok; //= {0}; //only needs init once, so static/shared data can be used here

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
    if (!retval) return (SDL_lib*)exc(RED_MSG "SDL_Init %s (0x%x) failed" ENDCOLOR_MYLINE, subsys.str().c_str() + 1, flags, where); //throw SDL_Exception("SDL_Init");
#if 0
    if (ok.evtid); //already inited //!= UNDEF_EVTID)
    else if (!OK(ok.evtid = SDL_RegisterEvents(2))) return (SDL_lib*)exc(RED_MSG "SDL_RegisterEvents failed" ENDCOLOR_MYLINE, where);
    else ++ok.evtid; //kludge: ask for 2 evt ids so last one != 0; then we can check for 0 instead of "(uint32_t)-1"
#endif
    if (already) myprintf(22, YELLOW_MSG "SDL %s (0x%x) was already %sinitialized (0x%x) from %d" ENDCOLOR, subsys.str().c_str() + 1, flags, (already != flags)? "partly ": "", already, where);
    if (where) myprintf(22, YELLOW_MSG "SDL_Init %s (0x%x) = 0x%x ok? %d (from %d)" ENDCOLOR, subsys.str().c_str() + 1, flags, toint(retval), !!retval, where);
    return retval;
}
//#define IMG_INIT(flags)  ((IMG_Init(flags) & (flags)) != (flags)) //0 == Success


//"convert" from SDL data types to locked objects:

///*inline*/ SDL_LockedMutex* SDL_LOCK(SDL_mutex* mutex, int where = 0)
/*inline*/ SDL_LockedMutex* SDL_LOCK(SDL_LockedMutex& locked, SDL_mutex* mutex, int where) //= 0)
{
    /*static SDL_LockedMutex*/ locked = {0};
    if (!mutex) return (SDL_LockedMutex*)exc(RED_MSG "No SDL_mutex to lock" ENDCOLOR_MYLINE, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_LockMutex(mutex))) return (SDL_LockedMutex*)exc(RED_MSG "SDL_LockMutex 0x%x failed" ENDCOLOR_MYLINE, mutex, where); //throw SDL_Exception("SDL_LockMutex");
    if (where) myprintf(32, YELLOW_MSG "SDL_LockMutex 0x%x ok (from %d)" ENDCOLOR, toint(mutex), where);
    locked.mutex = mutex;
    return &locked;
}

///*inline*/ SDL_LockedSemaphore* SDL_LOCK(SDL_sem* sem, int where = 0)
/*inline*/ SDL_LockedSemaphore* SDL_LOCK(SDL_LockedSemaphore& locked, SDL_sem* sem, int where) //= 0)
{
    /*static SDL_LockedSemaphore*/ locked = {0};
    if (!sem) return (SDL_LockedSemaphore*)exc(RED_MSG "No SDL_sem to lock" ENDCOLOR_MYLINE, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_SemWait(sem))) return (SDL_LockedSemaphore*)exc(RED_MSG "SDL_SemWait 0x%x failed" ENDCOLOR_MYLINE, sem, where); //throw SDL_Exception("SDL_LockMutex");
    if (where) myprintf(32, YELLOW_MSG "SDL_LockSemaphore 0x%x ok (from %d)" ENDCOLOR, toint(sem), where);
    locked.sem = sem;
    return &locked;
}

///*inline*/ SDL_LockedTexture* SDL_LOCK(SDL_Texture* txr, int where = 0, int chk_w = 0, int chk_h = 0, uint32_t chk_fmt = 0)
/*inline*/ SDL_LockedTexture* SDL_LOCK(SDL_LockedTexture& locked, SDL_Texture* txr, int where /*= 0*/, int chk_w = 0, int chk_h = 0, uint32_t chk_fmt = 0)
{
    /*static SDL_LockedTexture*/ locked = {0};
    if (!txr) return (SDL_LockedTexture*)exc(RED_MSG "No SDL_Texture to lock" ENDCOLOR_MYLINE, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_QueryTexture(txr, &locked.fmt, &locked.acc, &locked.surf.w, &locked.surf.h))) return (SDL_LockedTexture*)exc(RED_MSG "SDL_QueryTexture 0x%x failed" ENDCOLOR_MYLINE, txr, where); //throw SDL_Exception("SDL_LockMutex");
    if (!OK(SDL_LockTexture(txr, NORECT, &locked.surf.pixels, &locked.surf.pitch))) return (SDL_LockedTexture*)exc(RED_MSG "SDL_LockTexture 0x%x failed" ENDCOLOR_MYLINE, txr, where); //throw SDL_Exception("SDL_LockMutex");
    if (where) myprintf(32, YELLOW_MSG "SDL_LockTexture 0x%x ok (from %d)" ENDCOLOR, toint(txr), where);
//additional validation:
    if (!locked.surf.w || (chk_w && (locked.surf.w != chk_w)) || !locked.surf.h || (chk_h && (locked.surf.h != chk_h)))
        return (SDL_LockedTexture*)err(RED_MSG "Unexpected texture size: %dx%d should be %dx%d" ENDCOLOR_MYLINE, locked.surf.w, locked.surf.h, chk_w, chk_h, where), (SDL_LockedTexture*)NULL; //NUM_UNIV, UNIV_LEN);
    if (!locked.surf.pixels || (toint(locked.surf.pixels) & 7))
        return (SDL_LockedTexture*)err(RED_MSG "Texture pixels not aligned on 8-byte boundary" ENDCOLOR_MYLINE, where), (SDL_LockedTexture*)NULL; //*(auto_ptr*)0;
    if ((size_t)locked.surf.pitch != sizeof(uint32_t) * locked.surf.w)
        return (SDL_LockedTexture*)err(RED_MSG "Unexpected pitch: %d should be %zu * %d = %zu" ENDCOLOR_MYLINE, locked.surf.pitch, sizeof(uint32_t), locked.surf.w, sizeof(uint32_t) * locked.surf.w, where), (SDL_LockedTexture*)NULL;
    if (!locked.fmt || (chk_fmt && (locked.fmt != chk_fmt)))
        return (SDL_LockedTexture*)err(RED_MSG "Unexpected texture format: %i bpp %s should be %i bpp %s" ENDCOLOR_MYLINE, SDL_BITSPERPIXEL(locked.fmt), SDL_PixelFormatShortName(locked.fmt), SDL_BITSPERPIXEL(chk_fmt), SDL_PixelFormatShortName(chk_fmt), where), (SDL_LockedTexture*)NULL;
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


#include "autoptr.h"
#if 0
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
    auto_ptr(DataType* that = NULL): cast(that) {}; // myprintf(22, YELLOW_MSG "assign %s 0x%x" ENDCOLOR, TypeName(that), toint(that)); };
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
//            myprintf(22, YELLOW_MSG "reassign %s 0x%x" ENDCOLOR, TypeName(that), toint(that));
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
        lock() { myprintf(1, YELLOW_MSG "TODO" ENDCOLOR); };
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
#endif

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


#if 0
//shared memory mutex:
//from https://stackoverflow.com/questions/13161153/c11-interprocess-atomics-and-mutexes
//see notes at https://www.daniweb.com/programming/software-development/threads/444483/c-11-mutex-for-ipc
class ShmMutex
{
private:
    pthread_mutex_t* _handle;
public:
    ShmMutex(void* shmMemMutex, bool recursive = false): _handle(shmMemMutex)
    {
        pthread_mutexattr_t attr;
        ::pthread_mutexattr_init(&attr);
        ::pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        ::pthread_mutexattr_settype(&attr, recursive ? PTHREAD_MUTEX_RECURSIVE_NP : PTHREAD_MUTEX_FAST_NP);
        if (::pthread_mutex_init(_handle, &attr) == -1)
        {
            ::free(_handle);
            throw ThreadException("Unable to create mutex");
        }
    }
    virtual ~ShmMutex() { ::pthread_mutex_destroy(_handle); }

    void lock() { if (::pthread_mutex_lock(_handle)) throw ThreadException("Unable to lock mutex"); }
    void unlock() { if (::pthread_mutex_unlock(_handle)) throw ThreadException("Unable to unlock mutex"); }
    bool tryLock()
    {
        int tryResult = ::pthread_mutex_trylock(_handle);
        if (tryResult == EBUSY) return false;
        if (tryResult) throw ThreadException("Unable to lock mutex");
        return true;
    }
};
#endif


//send or wait for a signal (cond + mutex):
//uses FIFO of messages to compensate for SDL_cond limitations
class Signal
{
private:
    std::vector<void*> pending; //SDL discards signal if nobody waiting (CondWait must occur before CondSignal), so remember it
    auto_ptr<SDL_cond> cond;
//TODO: use smart_ptr with ref count
    static auto_ptr<SDL_mutex> mutex; //assume low usage, share across all signals
    static int count;
//NO-doesn't make sense to send-then-rcv immediately, and rcv-then-send needs processing in between:
//    enum Direction {SendOnly = 1, RcvOnly = 2, Both = 3};
public:
    Signal() //: cond(NULL)
    {
        myprintf(22+10, BLUE_MSG "sig ctor: count %d, m 0x%x, c 0x%x" ENDCOLOR, count, toint(mutex.cast), toint(cond.cast));
        if (!count++)
            if (!(mutex = SDL_CreateMutex())) exc(RED_MSG "Can't create signal mutex" ENDCOLOR); //throw SDL_Exception("SDL_CreateMutex");
        if (!(cond = SDL_CreateCond())) exc(RED_MSG "Can't create signal cond" ENDCOLOR); //throw SDL_Exception("SDL_CreateCond");
        myprintf(22+10, YELLOW_MSG "signal 0x%x has m 0x%x, c 0x%x" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast));
    }
    ~Signal() { if (!--count) mutex = NULL; }
public:
    void* wait()
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
    }
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
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
        if (!this->cast) exc(RED_MSG "Can't create thead '%s'" ENDCOLOR, name);
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
            myprintf(22, YELLOW_MSG "reassign %s 0x%x" ENDCOLOR, TypeName(that), toint(that));
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
        myprintf(22, CYAN_MSG "launch thread '%s', async %d" ENDCOLOR, svname, svasync);
//CAUTION: don't call start_thread() until Signals are inited and vtable is populated!
        auto_ptr<SDL_Thread>::operator=(SDL_CreateThread(start_thread, svname, this));
        if (!this->cast) return_void(exc(RED_MSG "Can't create thead '%s'" ENDCOLOR, svname));
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
        that->out.wake(that->main_bkg(that->in.wait()));
        myprintf(33, "child done async 0x%x" ENDCOLOR, toint(that));
        return SDL_Success; //TODO?
    }
    virtual void* main_bkg(void* data) = 0; //must override in derived class
public:
//    void* run(void* data = NULL, bool async = false)
//    {
////        UnlockMutex(mutex_);
//        send(data); //wake up child thread
//        return async? SDL_Success: wait(); //wait for child to reply
////        return this->exitval;
//    }
//public-facing (used by fg caller):
//NOTE: first msg in/out by main thread will start up bkg thread (delayed init)
    void* message(void* msg = NULL) { wake(msg); return wait(); } //send+receive
    void wake(void* msg = NULL) { init_delayed(); in.wake(msg); } //send
    void* wait() { init_delayed(); deque(); return out.wait(); } //receive
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
public:
    static void enque(const char* msg)
    {
        std::lock_guard<std::mutex> lock(msg_mutex);
        msg_que.push(msg);
    }
    void deque(void)
    {
        std::lock_guard<std::mutex> lock(msg_mutex);
//        if (!msg_que.size()) return;
        while (msg_que.size())
        {
            const char* buf = msg_que.front();
            msg_que.pop();
//            fprintf(stderr, "%s\n", buf);
            fputs(buf, stderr);
            delete[] buf;
        }
    }
private:
    static std::mutex msg_mutex;
    static std::queue<const char*> msg_que;
};
std::vector<SDL_threadID> Thread::all;
std::mutex Thread::msg_mutex;
std::queue<const char*> Thread::msg_que;
//std::mutex Thread::protect;
//Signal Thread::ack;


//simple msg que:
//uses mutex (shared) and cond var
//for ipc, mutex + cond var need to be in shared memory; for threads, just needs to be in heap
//this is designed for synchronous usage; msg que holds 1 int ( can be used as bitmap)
//template<int MAXLEN>
//no-base class to share mutex across all template instances:
//template<int MAXDEPTH>
//
//class MsgQue: public MsgQueBase
class MsgQue//Base
{
public:
    explicit MsgQue(const char* name = 0): m_msg(0)
    {
        if (WANT_DETAILS) { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
    }
    ~MsgQue()
    {
        if (m_msg && WANT_DETAILS) ATOMIC(std::cout << RED_MSG << m_name << ".dtor: !empty" << FMT("0x%x") << m_msg << ENDCOLOR); //benign, but might be caller bug so complain
    }
public:
    MsgQue& clear()
    {
        m_msg = 0;
        return *this; //fluent
    }
    MsgQue& send(int msg, bool broadcast = false)
    {
//        std::stringstream ssout;
        scoped_lock lock;
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_msg & msg) throw "MsgQue.send: msg already queued"; // + tostr(msg) + " already queued");
        m_msg |= msg; //use bitmask for multiple msgs
        if (!(m_msg & msg)) throw "MsgQue.send: msg enqueue failed"; // + tostr(msg) + " failed");
        if (WANT_DETAILS) ATOMIC(std::cout << BLUE_MSG << timestamp() << m_name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "1") << ", now qued " << FMT("0x%x") << m_msg << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (WANT_DETAILS) ATOMIC(std::cout << FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { if (WANT_DETAILS) ATOMIC(std::cout << FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
    int rcv(bool (MsgQue::*filter)(int val), int operand = 0, bool remove = false)
    {
//        std::unique_lock<std::mutex> lock(mutex);
        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (WANT_DETAILS) ATOMIC(std::cout << BLUE_MSG << timestamp() << m_name << ".rcv: " << FMT("0x%x") << m_msg << ENDCOLOR);
        while (!(this->*filter)(operand)) m_condvar.wait(lock); //ignore spurious wakeups
        int retval = m_msg;
        if (remove) m_msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
#if 0
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
#endif
//protected:
private:
    class scoped_lock: public std::unique_lock<std::mutex>
    {
    public:
        /*explicit*/ scoped_lock(): std::unique_lock<std::mutex>(m_mutex) {};
//        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
    };
//    static const char* tostr(int val)
//    {
//        static char buf[20];
//        snprintf(buf, sizeof(buf), "0x%x", val);
//        return buf;
//    }
//protected:
private:
    static std::mutex m_mutex; //assume low usage, share across all signals
    std::condition_variable m_condvar;
//    int m_msg[MAXLEN], m_count;
    std::string m_name; //for debug only
    /*volatile*/ int m_msg;
};
std::mutex MsgQue::m_mutex;


////////////////////////////////////////////////////////////////////////////////
////
/// Charlieplexing/chipiplexing encoder
//

#define NUM_SSR  (1 << CHPLEX_RCSIZE)
#define CHPLEX_RCSIZE  3
#define CHPLEX_RCMASK  (NUM_SSR - 1)
#define ROW(rc)  ((rc) >> CHPLEX_RCSIZE)
#define ROW_bits(rc)  ((rc) & ~CHPLEX_RCMASK)
#define COL(rc)  ((rc) & CHPLEX_RCMASK)
#define ISDIAG(rc)  (COL(rc) == ROW(rc))
#define HASDIAG(rc)  (COL(rc) > ROW(rc))

#if NUM_SSR > 8
 #error "[ERROR] Num SSR too big (max 8)"
#endif

template<int todoNUM_SSR>
class ChplexEncoder
{
private:
//sorted indices:
//in order to reduce memory shuffling, only indices are sorted
    uint8_t sorted[NUM_SSR * (NUM_SSR - 1)]; //56 bytes
//statically allocated delay info:
//never moves after allocation, but could be updated
    struct DimRowEntry
    {
        uint8_t delay; //brightness
//    uint8_t rownum_numcols; //#col upper, row# lower; holds up to 16 each [0..15]
//    uint8_t colmap; //bitmap of columns for this row
//   uint8_t rowmap; //bitmap of rows
        uint8_t numrows;
        uint8_t colmaps[NUM_SSR]; //bitmap of columns for each row
    } DimRowList[NUM_SSR * (NUM_SSR - 1)]; //3*56 = 168 bytes    //10*56 = 560 bytes (616 bytes total used during sort, max 169 needed for final list)
    uint8_t total_rows; //= 0;
    uint8_t count; //= 0; //#dim slots allocated
    uint8_t rcinx; //= 0; //raw (row, col) address; ch*plex diagonal address will be skipped
public:
    ChplexEncoder() { init_list(); }
public:
    void write(int val)
    {
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
        if (count >= NUM_SSR * (NUM_SSR - 1)) { printf(RED_MSG "overflow" ENDCOLOR); return; }
        struct DimRowEntry* ptr = &DimRowList[count];
//    ptr->rownum_numcols = ROW_bits(rcinx) | 1;
//    ptr->colmap = 0x80 >> COL(rcinx);
        ptr->delay = val;
//    ptr->rowmap = 0x80 >> ROW(rcinx);
        for (int i = 0; i < NUM_SSR; ++i) ptr->colmaps[i] = 0; //TODO: better to do this 1x at start, or incrementally as needed?
        ptr->colmaps[ROW(rcinx)] = 0x80 >> COL(rcinx);
no_myprintf(14, BLUE_MSG "colmap[0] 0x%x" ENDCOLOR, ptr->colmaps[ROW(rcinx)]);
        ptr->numrows = 1;
        ++total_rows;
        ++count;
        ++rcinx;
    }

    void init_list()
    {
        total_rows = 0;
        count = 0;
        rcinx = 0;
    }

    void insert(int newvalue)
    {
        no_myprintf(18, BLUE_MSG "INS[%d] %d [r %d,c %d]: " ENDCOLOR, count, newvalue, ROW(rcinx), COL(rcinx));
        if (ISDIAG(rcinx)) { ++rcinx; no_myprintf(18, BLUE_MSG "skip diagonal" ENDCOLOR); return; }
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
        if (!newvalue) { /*sorted[count - 1] = 0*/; ++rcinx; no_myprintf(18, BLUE_MSG "skip null" ENDCOLOR); return; } //don't need to store this one
        if (rcinx >= NUM_SSR * NUM_SSR) { myprintf(18, RED_MSG "overflow" ENDCOLOR); return; }
//    if (newvalue == 233) showkeys(newvalue);
        int start, end;
//check if entry already exists using binary search:
        for (start = 0, end = count; start < end;)
        {
            int mid = (start + end) / 2;
            struct DimRowEntry* ptr = &DimRowList[sorted[mid]];
            int cmpto = ptr->delay;
//        if (newvalue == 233) printf("cmp start %d, end %d, mid %d val %d\n", start, end, mid, ptr->delay);
//NOTE: sort in descending order
            if (newvalue > ptr->delay) { end = mid; continue; } //search first half
            if (newvalue < ptr->delay) { start = mid + 1; continue; } //search second half
//printf("new val[%d] %d?, row %d, col %d, vs row %d, cols %d 0x%x, ofs %d" ENDCOLOR, mid, newvalue, ROW(rcinx), COL(rcinx), ROW(ptr->rownum_numcols), COL(ptr->rownum_numcols), ptr->colmap, mid);
//printf("new val[%d] %d?, row %d, col %d, vs cols %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x, ofs %d" ENDCOLOR, mid, newvalue, ROW(rcinx), COL(rcinx), "*" + (ROW(rcinx) != 0), ptr->colmaps[0], "*" + (ROW(rcinx) != 1), ptr->colmaps[1], "*" + (ROW(rcinx) != 2), ptr->colmaps[2], "*" + (ROW(rcinx) != 3), ptr->colmaps[3], "*" + (ROW(rcinx) != 4), ptr->colmaps[4], "*" + (ROW(rcinx) != 5), ptr->colmaps[5], "*" + (ROW(rcinx) != 6), ptr->colmaps[6], "*" + (ROW(rcinx) != 7), ptr->colmaps[7], mid);
//        if (ROW_bits(rcinx) > ROW_bits(ptr->rownum_numcols)) { end = mid; continue; }
//        if (ROW_bits(rcinx) < ROW_bits(ptr->rownum_numcols)) { start = mid + 1; continue; }
//collision:
//        if (!(ptr->rowmap & (0x80 >> ROW(rcinx)))) ptr->colmaps[COL(rcinx)] = 0; //TODO: better to do this incrementally, or 1x when entry first created?
            if (!ptr->colmaps[ROW(rcinx)]) { ++ptr->numrows; ++total_rows; }
            ptr->colmaps[ROW(rcinx)] |= 0x80 >> COL(rcinx);
//        ptr->rowmap |= 0x80 >> ROW(rcinx);
//printf(YELLOW_MSG "found: add col, new count: %d" ENDCOLOR, COL(ptr->rownum_numcols + 1));
//        if (!COL(ptr->rownum_numcols + 1)) printf(RED_MSG "#col wrap" ENDCOLOR);
//        ++ptr->rownum_numcols;
//        ptr->colmap |= 0x80 >> COL(rcinx);
//fill in list tail:
//        sorted[count] = count;
//        write(0); //null (off) entry
            ++rcinx;
            return;
        }
//create a new entry, insert into correct position:
no_myprintf(18, BLUE_MSG "ins new val %d at %d, shift %d entries" ENDCOLOR, newvalue, start, count - start);
        for (int i = count; i > start; --i) sorted[i] = sorted[i - 1];
        sorted[start] = count;
        write(newvalue);
//    if (newvalue == 233) showkeys(newvalue);
    }

    void insert(uint8_t* list56)
    {
        init_list();
        for (int i = 0; i < NUM_SSR * (NUM_SSR - 1); ++i) insert(list56[i]);
//    for (int i = 0; i < 56; ++i) new_insert(list56[i]);
//    for (int i = 0; i < delay_count; ++i) printf("delay[%d/%d]: %d, # %d\n", i, delay_count, dim_list[i].delay, dim_list[i].numrows);
    }

//    struct
//    {
//        uint8_t delay, rowmap, colmap;
//    } DispList[NUM_SSR * (NUM_SSR - 1)]; //+ 1]; //3*56+1 == 169 bytes
    uint8_t DispList[3 * NUM_SSR * (NUM_SSR - 1)];
    uint8_t checksum;
    int disp_count; //= 0;

//resolve delay conflicts (multiple rows competing for same dimming slot):
//assign dimming slots to each row
    void resolve_conflicts()
    {
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
        no_myprintf(18, BLUE_MSG "resolve dups rc %d, # %d" ENDCOLOR, rcinx, count);
        disp_count = checksum = 0;
        uint8_t max = 255, min = total_rows;
        for (int i = 0; i < count; ++i)
        {
//this entry uses dimming slots [delay + (numrows - 1) / 2 .. delay - numrows / 2]
//#define UPSHIFT(ptr)  ((ptr->numrows - 1) / 2)
//#define DOWNSHIFT(ptr)  (ptr->numrows / 2)
//#define FIRST_SLOT(ptr)  (ptr->delay + UPSHIFT(ptr))
//#define LAST_SLOT(ptr)  (ptr->delay - DOWNSHIFT(ptr))
//        struct DimRowEntry* prev = i? &DimRowList[sorted[i - 1]]: 0;
            struct DimRowEntry* ptr = &DimRowList[sorted[i]];
//        struct DimRowEntry* next = (i < count - 1)? &DimRowList[sorted[i + 1]]: 0;
#if 0
        if (prev && (FIRST_SLOT(ptr) >= LAST_SLOT(prev)) //need to shift later
            if (next && (FIRST_SLOT(next) >= LAST_SLOT(ptr)) //need
            if (
        if (ptr->delay + ptr->numrows / 2 > 255) ptr->delay += ptr->numrows / 2;
#endif
            min -= ptr->numrows; //update min for this group
            int adjust = (ptr->numrows - 1) / 2; //UPSHIFT(ptr);
            if (ptr->delay + adjust > max) { adjust = max - ptr->delay; no_myprintf(18, YELLOW_MSG "can only upshift delay[%d/%d] %d by <= %d" ENDCOLOR, i, count, ptr->delay, adjust); }
            else if (ptr->delay + adjust - ptr->numrows < min) { adjust = min - (ptr->delay - ptr->numrows); no_myprintf(18, YELLOW_MSG "must upshift delay[%d/%d] %d by >= %d" ENDCOLOR, i, count, ptr->delay, adjust); }
            else if (adjust) no_myprintf(18, BLUE_MSG "upshifted delay[%d/%d] %d by +%d" ENDCOLOR, i, count, ptr->delay, adjust);
            ptr->delay += adjust;
//TODO: order rows according to #cols?
            bool firstrow = true;
            for (int r = 0; r < NUM_SSR; ++r)
            {
                if (!ptr->colmaps[r]) continue;
                checksum ^= DispList[disp_count++]/*.delay*/ = firstrow? max - ptr->delay + 1: 1; //ptr->delay--;
                checksum ^= DispList[disp_count++]/*.rowmap*/ = 0x80 >> r;
                checksum ^= DispList[disp_count++]/*.colmap*/ = ptr->colmaps[r];
                firstrow = false;
//                ++disp_count;
            }
//        ptr->delay += ptr->numrows; //restore for debug display
            max = ptr->delay - ptr->numrows; //update max for next group
        }
//        DispList[disp_count].delay = 0;
        for (int i = disp_count; i < 3 * NUM_SSR * (NUM_SSR - 1); ++i)
            DispList[i]/*.delay = DispList[i].rowmap = DispList[i].colmap*/ = 0;
#if 1
        no_myprintf(18, "disp list %d ents:" ENDCOLOR, disp_count / 3);
        int dim = 256;
        for (int i = 0; i < disp_count; i += 3)
            no_myprintf(18, "disp[%d/%d]: delay %d (dim %d), rowmap 0x%x (row %d), colmap 0x%x" ENDCOLOR, i, disp_count, DispList[i]/*.delay*/, dim -= DispList[i]/*.delay*/, DispList[i + 1]/*.rowmap*/, Log2(DispList[i + 1]/*.rowmap*/), DispList[i + 2]/*.colmap*/);
    }
//helpers:
private:
    static int Log2(int val)
    {
        for (int i = 0; i < 8; ++i)
            if (val >= 0x80 >> i) return 7 - i;
        return -1;
#endif
    }

//for dev/debug only:
public:
/*
    void showkeys(int newvalue)
    {
        printf(BLUE_MSG "keys now (%d): ", newvalue);
        for (int i = 0; i < count; ++i)
            printf("%s[%d] %d, ", (!i || (DimRowList[sorted[i - 1]].delay > DimRowList[sorted[i]].delay))? GREEN_MSG: RED_MSG, i, DimRowList[sorted[i]].delay);
        printf(ENDCOLOR);
    }
*/

    void show_list(const char* desc)
    {
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
        no_myprintf(18, CYAN_MSG "%s %d entries (%d total rows):" ENDCOLOR, desc, count, total_rows);
        for (int i = 0; i < count; ++i)
        {
//        if (ISDIAG(i)) continue; //skip diagonal ch*plex row/col address
//        int ii = i - ROW(i) - HASDIAG(i); //skip diagonal ch*plex row/col address
            struct DimRowEntry* ptr = &DimRowList[sorted[i]];
//        printf(PINK_MSG "[%d/%d=%d]: delay %d, row# %d, #cols %d, cols 0x%x" ENDCOLOR, i, count, sorted[i], ptr->delay, ROW(ptr->rownum_numcols), COL(ptr->rownum_numcols), ptr->colmap);
            no_myprintf(18, PINK_MSG "[%d/%d=%d]: delay %d, #rows %d, cols 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x" ENDCOLOR, i, count, sorted[i], ptr->delay, ptr->numrows, ptr->colmaps[0], ptr->colmaps[1], ptr->colmaps[2], ptr->colmaps[3], ptr->colmaps[4], ptr->colmaps[5], ptr->colmaps[6], ptr->colmaps[7]);
//        if (!ptr->delay) break; //eof
        }
    }
};
inline int Release(/*const*/ ChplexEncoder<NUM_SSR>* that) { delete that; return SDL_Success; }


////////////////////////////////////////////////////////////////////////////////
////
/// GpuCanvas class, screen functions, global defs
//


//SDL_Init must be called before most other SDL functions and only once, so put it at global scope:
//*however*, RPi is sensitive to resource usage so don't init unless needed
auto_ptr<SDL_lib> SDL; //don't initialize unless needed; //(SDL_INIT(SDL_INIT_VIDEO));

typedef struct WH { uint16_t w, h; } WH; //pack width, height into single word for easy return from functions

//fwd refs:
void debug_info(CONST SDL_lib*, int where);
void debug_info(CONST SDL_Window*, int where);
void debug_info(CONST SDL_Renderer*, int where);
void debug_info(CONST SDL_Surface*, int where);
//capture line# for easier debug:
//NOTE: cpp avoids recursion so macro names can match actual function names here
#define debug_info(...)  debug_info(__VA_ARGS__, __LINE__)
WH ScreenInfo(void);
WH MaxFit(void);
uint32_t limit(uint32_t color);
uint32_t hsv2rgb(float h, float s, float v);
//uint32_t ARGB2ABGR(uint32_t color);
const char* commas(int64_t);
bool exists(const char* path);


//check for RPi:
//NOTE: results are cached (outcome won't change)
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


#if 0 //moved later
//get screen width, height:
//wrapped in a function so it can be used as initializer (optional)
//screen height determines max universe size
//screen width should be configured according to desired data rate (DATA_BITS per node)
WH ScreenInfo()
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
    }

//set reasonable values if can't get info:
    if (!wh.w || !wh.h)
    {
        /*throw std::runtime_error*/ exc(RED_MSG "Can't get screen size" ENDCOLOR);
        wh.w = 1536;
        wh.h = wh.w * 3 / 4; //4:3 aspect ratio
        myprintf(22, YELLOW_MSG "Using dummy display mode %dx%d" ENDCOLOR, wh.w, wh.h);
    }
    return wh;
}
#endif


//#define RENDER_THREAD

#define WS281X_BITS  24 //each WS281X node has 24 data bits; send 24 bits to GPIO pins for each display row
#define TXR_WIDTH  (3 * WS281X_BITS) //- 1) //data signal is generated at 3x bit rate, last bit overlaps H blank

//#define MULTI_THREADED
#define SINGLE_THREADED_BKG
//#ifdef MULTI_THREADED
// #define IFMULTI(stmt)  stmt
// #define IFSINGLE(stmt)  //noop
//#else
// #define IFMULTI(stmt)  //noop
// #define IFSINGLE(stmt)  stmt
//#endif


//window create and redraw:
//MULTI: all GPU work done in bkg thread (asynchronously)
//SINGLE: all GPU work done in fg thread (synchronously)
//BKG (single threaded): all GPU work done in bkg thread
#ifdef SINGLE_THREADED_BKG
class GpuCanvas: public Thread
#else
class GpuCanvas //: public Thread
#endif
{
private:
//    auto_ptr<SDL_lib> sdl; //NOTE: this must occur before thread? most sources say do this once only; TODO: use ref counter?
//set only by fg thread:
    auto_ptr<SDL_Window> window;
//set only by bkg thread:
    auto_ptr<SDL_Renderer> renderer;
//set by bkg + fg threads:
#ifdef MULTI_THREADED
    std::atomic<bool> dirty, done;
    auto_ptr<SDL_Surface> pxbuf;
    auto_ptr<SDL_mutex> busy;
#endif
//    std::atomic<int> txr_busy;
    auto_ptr<SDL_Texture> canvas;
//shared data:
//    static auto_ptr<SDL_lib> sdl;
//    static int count;
//performance stats:
    uint64_t started, reported; //doesn't need to be atomic; won't be modified after fg thread wakes
    uint64_t render_timestamp, frame_rate[30];
#ifdef MULTI_THREADED
    std::atomic<uint32_t> numfr, numerr, num_dirty; //could be updated by another thread
    struct { uint64_t previous; std::atomic<uint64_t> user_time, caller_time, encode_time; } fg; //, lock_time, update_time, unlock_time; } fg;
    struct { uint64_t previous; std::atomic<uint64_t> caller_time, encode_time, lock_time, update_time, unlock_time, copy_time, present_time; } bg;
#else
    SDL_Rect Hclip;
    const bool want_pivot = false;
//NO: could be updated by another thread
    uint32_t numfr, numerr, num_dirty;
    struct { uint64_t previous, caller, encode, update, throttle, render; } times;
#endif //def MULTI_THREADED
//    bool reported;
    int num_univ, univ_len; //caller-defined
public:
    bool DEV_MODE; //WantPivot; //dev/debug vs. live mode; TODO: getter/setter
//    std::string DumpFile;
//    std::atomic<double> PresentTime; //presentation timestamp (set by bkg rendering thread)
    double PresentTime() { return started? elapsed(started): -1; } //presentation timestamp (according to bkg rendering thread)
    void ResetElapsed(double elaps = 0) { started = now() - elaps * SDL_TickFreq(); }
//    int StatsAdjust; //allow caller to tweak stats
//CAUTION: must match firmware
//        unsigned unused: 5; //lsb
//        unsigned RGswap: 1;
//        unsigned IgnoreChksum: 1;
//        unsigned ActiveHigh: 1; //msb
#define UTYPEOF(univtype)  ((univtype) & TYPEBITS)
    enum UniverseTypes { INVALID = -1, NONE = 0, WS281X = 1, PLAIN_SSR = 2, CHPLEX_SSR = 3, SPAREBIT = 0x10, TYPEBITS = 0xF, RGSWAP = 0x20, CHECKSUM = 0x40, POLARITY = 0x80, ACTIVE_HIGH = 0x80, ACTIVE_LOW = 0}; //WS281X is default, but make non-0 to see if explicitly set; TODO: make extensible
    UniverseTypes UnivType(int inx, UniverseTypes newtype = INVALID)
    {
        if ((inx < 0) || (inx >= this->num_univ)) return INVALID;
        UniverseTypes oldtype = this->univ_types[inx];
        if ((newtype != INVALID) && (UTYPEOF(newtype) != UTYPEOF(oldtype)))
        {
//            myprintf(14, (UTYPEOF(newtype) != UTYPEOF(INVALID))? BLUE_MSG "GpuCanvas: UnivType[%d] was %d + flags 0x%d -> is now %d + flags 0x%x" ENDCOLOR: BLUE_MSG "GpuCanvas: UnivType[%d] is %d + flags 0x%d" ENDCOLOR, inx, oldtype & TYPEBITS, oldtype & ~TYPEBITS & 0xFF, newtype & TYPEBITS, newtype & ~TYPEBITS & 0xFF);
            if (UTYPEOF(newtype) == UTYPEOF(CHPLEX_SSR)) encoders[inx] = new ChplexEncoder<NUM_SSR>; //alloc memory while caller is still in prep, prior to playback
            this->univ_types[inx] = newtype;
        }
        return oldtype;
    }
    int width() { return this->num_univ; }
    int height() { return this->univ_len; }
private:
    std::vector<UniverseTypes> univ_types; //universe types
    std::vector<auto_ptr<ChplexEncoder<NUM_SSR>>> encoders;
public:
//ctor/dtor:
#ifdef SINGLE_THREADED_BKG
    /*typedef*/ struct { const char* title; int num_univ; int univ_len; } CtorParams;
//    enum class Tristate: int { False = 0, True = 1, Error = -1};
    typedef void (*callback)(void* data, tristate done);
    /*typedef*/ struct { uint32_t* pixels; uint64_t delay; callback cb; void* cbdata; } PaintParams;
//    CtorParams cp;
//    PaintParams pp;
    GpuCanvas(const char* title, int num_univ, int univ_len): Thread("GpuCanvas-bkg", true), started(now()) //, StatsAdjust(0)
    {
//        CtorParams.title = title;
//        CtorParams.num_univ = num_univ;
//        CtorParams.univ_len = univ_len;
        CtorParams = {title, num_univ, univ_len}; //CAUTION: needs to be on heap, *not* stack (going across threads)
        myprintf(22, BLUE_MSG "GpuCanvas init via bkg thread" ENDCOLOR);
        this->wake(&CtorParams); //(void*)0x1234); //run main_bkg() asynchronously in bkg thread
//        myprintf(22, BLUE_MSG "GpuCanvas wait for bkg" ENDCOLOR);
        bool ok = (bool)this->wait(); //wait for bkg thread to init
        myprintf(22, BLUE_MSG "GpuCanvas bkg thread ready? %d, ret to caller" ENDCOLOR, ok);
    }
//bkg thread main logic:
    void* main_bkg(void* ignored)
    {
//        CtorParams* cp = reinterpret_cast<CtorParams*>(ptr);
//        myprintf(22, BLUE_MSG "main_bkg: title '%s', #univ %d, univ len %d" ENDCOLOR, cp->title, cp->num_univ, cp->univ_len);
        ctor_bkg(CtorParams.title, CtorParams.num_univ, CtorParams.univ_len);
        out.wake((void*)true); //tell main thread i'm ready
        for (;;) //req loop
        {
            myprintf(22, BLUE_MSG "canvas bkg loop waiting for more work" ENDCOLOR);
//            PaintParams* pp = reinterpret_cast<PaintParams*>(in.wait());
            void* req = in.wait();
            myprintf(28, BLUE_MSG "main_bkg: woke @%2.1f msec, req 0x%lx, now paint? %d" ENDCOLOR, PresentTime() * 1000, (long)req, !!req);
            if (!req) break; //eof
            bool ok = Paint(PaintParams.pixels, PaintParams.delay, PaintParams.cb, PaintParams.cbdata);
//no; async completion            out.wake((void*)ok);

            if (PaintParams.cb) PaintParams.cb(PaintParams.cbdata, ok? tristate::Yes: tristate::Error);
        }
        myprintf(8, MAGENTA_MSG "bkg renderer thread: exit after %2.1f msec" ENDCOLOR, elapsed(started));
//        done = true;
        return (void*)true; //SDL_Success (async completion)
    }
    bool Paint_bkg(uint32_t* pixels = 0, uint64_t render_delay = 0, callback cb = 0, void* cbdata = 0)
    {
        PaintParams = {pixels, render_delay, cb, cbdata}; //CAUTION: needs to be on heap, *not* stack (going across threads)
        myprintf(22, BLUE_MSG "GpuCanvas paint via bkg thread @%2.1f msec" ENDCOLOR, PresentTime() * 1000); //, &pp 0x%lx, pixels 0x%lx, cb 0x%lx, cbdata 0x%lx" ENDCOLOR, (long)&PaintParams, (long)PaintParams.pixels, (long)PaintParams.cb, (long)PaintParams.cbdata);
//        PaintParams.pixels = pixels;
//        PaintParams.delay = render_delay;
//        PaintParams.cb = cb;
//        PaintParams.cbdata = cbdata;
        this->wake(&PaintParams); //(void*)0x5678); //run Paint() asynchronously in bkg thread
//        myprintf(22, BLUE_MSG "GpuCanvas wait for bkg" ENDCOLOR);
//no; async completion        bool ok = (bool)this->wait(); //wait for bkg thread to paint
//        myprintf(22, BLUE_MSG "GpuCanvas paint bkg thread done, ret to caller" ENDCOLOR);
        return true; //NOTE: finishes asynchronously; //ok;
    }
    void ctor_bkg(const char* title, int num_univ, int univ_len)
#elif defined(MULTI_THREADED)
    GpuCanvas(const char* title, int num_univ, int univ_len, bool want_pivot = true): Thread("GpuCanvas", true), started(now()), /*StatsAdjust(0),*/ dump_count(0)
#else
    GpuCanvas(const char* title, int num_univ, int univ_len): started(now()) //, StatsAdjust(0)
#endif
    {
        if (!SDL) SDL = SDL_INIT(SDL_INIT_VIDEO);
//        myprintf(33, "GpuCanvas ctor" ENDCOLOR);
        if (!SDL_WasInit(SDL_INIT_VIDEO)) err(RED_MSG "ERROR: Tried to get canvas before SDL_Init" ENDCOLOR);
//        if (!count++) Init();
        if (!title) title = "GpuCanvas";
        myprintf(3, BLUE_MSG "Init: title '%s', #univ %d, univ len %d" ENDCOLOR, title, num_univ, univ_len); //, want_pivot);

//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0") != SDL_TRUE) //set texture filtering to linear; TODO: is this needed?
            err(YELLOW_MSG "Warning: Linear texture filtering not enabled" ENDCOLOR);
//TODO??    SDL_bool SDL_SetHintWithPriority(const char*      name, const char*      value,SDL_HintPriority priority)

#define IGNORED_X_Y_W_H  0, 0, 200, 100 //not used for full screen mode
//leave window on main thread so it can process events:
//https://stackoverflow.com/questions/6172020/opengl-rendering-in-a-secondary-thread
        window = isRPi()?
            SDL_CreateWindow(title, IGNORED_X_Y_W_H, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN): //| SDL_WINDOW_OPENGL): //don't use OpenGL; too slow
            SDL_CreateWindow(title, 10, 10, MaxFit().w, MaxFit().h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN); //| SDL_WINDOW_OPENGL);
        if (!window) return_void(exc(RED_MSG "Create window failed" ENDCOLOR));
        uint32_t fmt = SDL_GetWindowPixelFormat(window); //desktop OpenGL: 24 RGB8888, RPi: 32 ARGB8888
        if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(exc(RED_MSG "Can't get window format" ENDCOLOR));
        int wndw, wndh;
        SDL_GL_GetDrawableSize(window, &wndw, &wndh);
        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        SDL_GetWindowSize(window, &wndw, &wndh);
//        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        SDL_GetWindowMaximumSize(window, &wndw, &wndh);
//        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        int top, left, bottom, right;
//        if (!OK(SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right))) return_void(exc(RED_MSG "Can't get window border size" ENDCOLOR));
//        myprintf(22, BLUE_MSG "wnd border size: t %d, l %d, b %d, r %d" ENDCOLOR, top, left, bottom, right);
        debug_info(window);
#undef IGNORED_X_Y_W_H

        if ((num_univ < 1) || (num_univ > WS281X_BITS)) return_void(exc(RED_MSG "Bad number of universes: %d (should be 1..%d)" ENDCOLOR, num_univ, WS281X_BITS));
        if ((univ_len < 1) || (univ_len > wndh)) return_void(exc(RED_MSG "Bad universe size: %d (should be 1..%d)" ENDCOLOR, univ_len, wndh));
//NOTE: to avoid fractional pixels, screen/window width should be a multiple of 71, height should be a multiple of univ_len
        if (wndw % (TXR_WIDTH - 1)) myprintf(1, YELLOW_MSG "Window width %d !multiple of txr width %d: check for correct edges" ENDCOLOR, wndw, TXR_WIDTH - 1);
        if (wndh % univ_len) myprintf(1, YELLOW_MSG "Window height %d !multiple of univ len %d: check for correct edges" ENDCOLOR, wndh, univ_len);

#ifdef MULTI_THREADED
//create memory buf to hold pixel data:
//NOTE: surface + texture must always be 3 * WS281X_BITS - 1 pixels wide
//data signal is generated at 3x bit rate, last bit overlaps H blank
//surface + texture height determine max # WS281X nodes per universe
//SDL will stretch texture to fill window (V-grouping); OpenGL not needed for this
//use same fmt + depth as window; TODO: is this more efficient?
//        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, 8+8+8+8, SDL_PIXELFORMAT_ARGB8888);
//TODO: is SDL_AllocFormat helpful here?
        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
        if (!pxbuf) return_void(exc(RED_MSG "Can't alloc pixel buf" ENDCOLOR));
        if ((pxbuf.cast->w != TXR_WIDTH) || (pxbuf.cast->h != univ_len)) return_void(exc(RED_MSG "Pixel buf wrong size: got %d x %d, wanted %d x %d" ENDCOLOR, pxbuf.cast->w, pxbuf.cast->h, TXR_WIDTH, univ_len));
        if (toint(pxbuf.cast->pixels) & 3) return_void(exc(RED_MSG "Pixel buf not quad byte aligned" ENDCOLOR));
        if (pxbuf.cast->pitch != 4 * pxbuf.cast->w) return_void(exc(RED_MSG "Pixel buf pitch: got %d, expected %d" ENDCOLOR, pxbuf.cast->pitch, 4 * pxbuf.cast->w));
        debug_info(pxbuf);
        if (!OK(SDL_FillRect(pxbuf, NORECT, MAGENTA | BLACK))) return_void(exc(RED_MSG "Can't clear pixel buf" ENDCOLOR));
//        myprintf(33, "bkg pxbuf ready" ENDCOLOR);

        if (!(busy = SDL_CreateMutex())) return_void(exc(RED_MSG "Can't create signal mutex" ENDCOLOR)); //throw SDL_Exception("SDL_CreateMutex");

        done = false;
        dirty = true; //force initial screen update + notification of canvas available (no paint doesn't block)
//        txr_busy = 0;
        this->DEV_MODE = !want_pivot;
#endif //def MULTI_THREADED
        this->num_univ = num_univ;
        this->univ_len = univ_len;
        this->univ_types.resize(num_univ, NONE); //NOTE: caller needs to call paint() after changing this
        this->encoders.resize(num_univ, NULL);

#ifdef MULTI_THREADED
        fg.user_time = fg.caller_time = fg.encode_time = 0; //fg.update_time = fg.unlock_time = 0;
        myprintf(22, BLUE_MSG "GpuCanvas wake thread" ENDCOLOR);
#if 0 //TODO
        TOBKG();
	    renderer = SDL_CreateRenderer(window, FIRST_MATCH, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); //NOTE: PRESENTVSYNC syncs with V refresh rate (typically 60 Hz)
        canvas = SDL_CreateTexture(renderer, pxbuf.cast->format->format, SDL_TEXTUREACCESS_STREAMING, pxbuf.cast->w, pxbuf.cast->h); //SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, wndw, wndh);
        TOFG();
#else
        this->wake((void*)0x1234); //run main_bkg() asynchronously in bkg thread
        myprintf(22, BLUE_MSG "GpuCanvas wait for bkg" ENDCOLOR);
        this->wait(); //wait for bkg thread to init
#endif
        myprintf(22, BLUE_MSG "GpuCanvas bkg thread ready, ret to caller" ENDCOLOR);
#else //def MULTI_THREADED
	    renderer = SDL_CreateRenderer(window, FIRST_MATCH, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); //NOTE: PRESENTVSYNC syncs with V refresh rate (typically 60 Hz)
        if (!renderer) return_void(err(RED_MSG "Create renderer failed" ENDCOLOR));
        debug_info(renderer);

        canvas = SDL_CreateTexture(renderer, fmt, SDL_TEXTUREACCESS_STREAMING, TXR_WIDTH, univ_len); //SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, wndw, wndh);
        if (!canvas) return_void(err(RED_MSG "Can't create canvas texture" ENDCOLOR));

        Hclip = {0, 0, TXR_WIDTH - 1, univ_len}; //CRITICAL: clip last col (1/3 pixel) so it overlaps with H-sync
        myprintf(22, BLUE_MSG "clip rect (%d, %d, %d, %d)" ENDCOLOR, Hclip.x, Hclip.y, Hclip.w, Hclip.h);

        myprintf(8, MAGENTA_MSG "canvas startup took %2.1f msec" ENDCOLOR, elapsed(started));
        times.caller = times.encode = times.update = times.throttle = times.render = numfr = numerr = num_dirty = 0;
        render_timestamp = started = reported = times.previous = now();
//        render_timestamp = frame_rate = 0;
#endif //def MULTI_THREADED
    }
    ~GpuCanvas()
    {
#ifdef SINGLE_THREADED_BKG
        myprintf(22, YELLOW_MSG "TODO: mv GpuCanvas dtor to bkg thread?" ENDCOLOR);
        this->wake(); //eof; tell bkg thread to quit (if it's waiting)
#endif
//        myprintf(22, BLUE_MSG "GpuCanvas dtor" ENDCOLOR);
//        if (reported != times.previous) stats();
#ifdef MULTI_THREADED
        this->done = true;
        this->wake(); //eof; tell bkg thread to quit (if it's waiting)
#endif
        myprintf(22, YELLOW_MSG "GpuCanvas dtor" ENDCOLOR);
    }
#ifdef MULTI_THREADED
private:
//bkg thread:
    void* main_bkg(void* data)
    {
//        myprintf(33, "bkg thr main start" ENDCOLOR);
        uint64_t delta;
//        started = now();
//        PresentTime = -1; //prior to official start of playback
//        SDL_Window* window = reinterpret_cast<SDL_Window*>(data); //window was created in main thread where events are handled
        myprintf(8, MAGENTA_MSG "bkg thread started: data 0x%x" ENDCOLOR, toint(data));

	    renderer = SDL_CreateRenderer(window, FIRST_MATCH, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); //NOTE: PRESENTVSYNC syncs with V refresh rate (typically 60 Hz)
        if (!renderer) return err(RED_MSG "Create renderer failed" ENDCOLOR);
        debug_info(renderer);

#if 0
//NOTE: surface + texture must always be 3 * WS281X_BITS - 1 pixels wide
//data signal is generated at 3x bit rate, last bit overlaps H blank
//surface + texture height determine max # WS281X nodes per universe
//SDL will stretch texture to fill window (V-grouping); OpenGL not needed for this
//use same fmt + depth as window; TODO: is this more efficient?
//        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, 8+8+8+8, SDL_PIXELFORMAT_ARGB8888);
        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(this->fmt), this->fmt);
        if (!pxbuf) return (void*)err(RED_MSG "Can't alloc pixel buf" ENDCOLOR);
        if ((pxbuf.cast->w != TXR_WIDTH) || (pxbuf.cast->h != univ_len)) return (void*)err(RED_MSG "Pixel buf wrong size: got %d x %d, wanted %d x %d" ENDCOLOR, pxbuf.cast->w, pxbuf.cast->h, TXR_WIDTH, univ_len);
        if (toint(pxbuf.cast->pixels) & 3) return (void*)err(RED_MSG "Pixel buf not quad byte aligned" ENDCOLOR);
        if (pxbuf.cast->pitch != 4 * pxbuf.cast->w) return (void*)err(RED_MSG "Pixel buf pitch: got %d, expected %d" ENDCOLOR, pxbuf.cast->pitch, 4 * pxbuf.cast->w);
        debug_info(pxbuf);
        if (!OK(SDL_FillRect(pxbuf, NORECT, MAGENTA | BLACK))) return (void*)err(RED_MSG "Can't clear pixel buf" ENDCOLOR);
//        myprintf(33, "bkg pxbuf ready" ENDCOLOR);
#endif

        canvas = SDL_CreateTexture(renderer, pxbuf.cast->format->format, SDL_TEXTUREACCESS_STREAMING, pxbuf.cast->w, pxbuf.cast->h); //SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, wndw, wndh);
        if (!canvas) return (void*)err(RED_MSG "Can't create canvas texture" ENDCOLOR);
        myprintf(8, MAGENTA_MSG "bkg startup took %2.1f msec" ENDCOLOR, elapsed(started));

        SDL_Rect Hclip = {0, 0, TXR_WIDTH - 1, this->univ_len}; //CRITICAL: clip last col (1/3 pixel) so it overlaps with H-sync
        myprintf(22, BLUE_MSG "render copy (%d, %d, %d, %d) => target" ENDCOLOR, Hclip.x, Hclip.y, Hclip.w, Hclip.h);

//        Paint(NULL); //start with all pixels dark
        bg.caller_time = bg.encode_time = bg.lock_time = bg.update_time = bg.unlock_time = bg.copy_time = bg.present_time = numfr = numerr = num_dirty = 0;

        render_timestamp = started = fg.previous = bg.previous = now();
//        render_timestamp = frame_rate = 0;
//        PresentTime = 0; //start of official playback
//        myprintf(33, "bkg ack main" ENDCOLOR);
        out.wake(pxbuf); //tell main thread i'm ready
//        out.wake(pxbuf); //tell main thread canvas is available (advanced notice so first paint doesn't block)

        uint32_t pixels_copy[this->num_univ * this->univ_len];
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

//            encode((uint32_t*)pixels);
            memcpy(pixels_copy, pixels, this->num_univ * this->univ_len * sizeof(uint32_t)); //copy caller's data without encode so caller can wake sooner
//            delta = now() - bg.previous; bg.encode_time += delta; bg.previous += delta;
            out.wake((void*)true); //allow fg thread to refill buf while render finishes in bkg
            encode(pixels_copy); //encode while caller does other things (encode is CPU-expensive)
            delta = now() - bg.previous; bg.encode_time += delta; bg.previous += delta;

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
    myprintf(22, BLUE_MSG "paint: %s" ENDCOLOR, buf.str().c_str() + 1);
#endif
            }
            delta = now() - bg.previous; bg.unlock_time += delta; bg.previous += delta;

        	if (!OK(SDL_RenderCopy(renderer, canvas, &Hclip, NORECT))) //&renderQuad, angle, center, flip ))
            {
                err(RED_MSG "Unable to render to screen" ENDCOLOR);
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
//        return err(RED_MSG "Can't unbind current context" ENDCOLOR);
//    uint_least32_t time = now_usec();
//    xfr_time += ELAPSED;
//    render_times[2] += (time2 = now_usec()) - time1;
//    myprintf(22, RED_MSG "TODO: delay music_time - now" ENDCOLOR);
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
//                if (txr_busy++) err(RED_MSG "txr busy, shouldn't be" ENDCOLOR);
            	if (!OK(SDL_RenderCopy(renderer, canvas, &Hclip, NORECT))) //&renderQuad, angle, center, flip ))
                {
                    err(RED_MSG "Unable to render to screen" ENDCOLOR);
                    ++numerr;
                }
                ++num_dirty;
                dirty = false;
//                --txr_busy;
                out.wake(canvas); //tell fg thread canvas is available again for updates
            }
            delta = now() - bg.previous; bg.copy_time += delta; bg.previous += delta;
#endif

//            myprintf(8, MAGENTA_MSG "renderer thread: render+wait" ENDCOLOR);
            SDL_RenderPresent(renderer); //update screen; NOTE: blocks until next V-sync (on RPi)
            delta = now() - bg.previous; bg.present_time += delta; bg.previous += delta; //== now()
            if (render_timestamp) frame_rate += bg.previous - render_timestamp; //now - previous timestamp
            render_timestamp = bg.previous; //now
//            PresentTime = elapsed(started); //update presentation timestamp (in case main thread wants to know)
//myprintf(22, BLUE_MSG "fr[%d] deltas: %lld, %lld, %lld, %lld, %lld" ENDCOLOR, numfr, delta1, delta2, delta3, delta4, delta5);
//            if (!(++numfr % (60 * 10))) stats(); //show stats every 10 sec @60 FPS
        }
//        myprintf(33, "bkg done" ENDCOLOR);
        myprintf(8, MAGENTA_MSG "bkg renderer thread: exit after %2.1f msec" ENDCOLOR, elapsed(started));
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
    myprintf(22, BLUE_MSG "paint: %s" ENDCOLOR, buf.str().c_str() + 1);
}
#endif
//        myprintf(22, "GpuCanvas paint" ENDCOLOR);
        uint64_t delta;
        delta = now() - fg.previous; fg.caller_time += delta; fg.previous += delta;
//        myprintf(6, BLUE_MSG "Paint(pixels 0x%x), %d x %d = %s len (assumed), 0x%x 0x%x 0x%x ..." ENDCOLOR, pixels, this->num_univ, this->univ_len, commas(this->num_univ * this->univ_len), pixels? pixels[0]: fill, pixels? pixels[1]: fill, pixels? pixels[2]: fill);

//NOTE: OpenGL is described as not thread safe
//apparently even texture upload must be in same thread; else get "intel_do_flush_locked_failed: invalid argument" crashes
#if 1 //gpu upload in bg thread
//        struct { uint32_t* pixels; const std::function<void (void*)>& cb; void* data; } info = {pixels, cb, data};
        this->wake(pixels);
        bool ok = this->wait(); //delay until bg thread has grabbed my data, so I can reuse my buf
        delta = now() - fg.previous; fg.encode_time += delta; fg.previous += delta;
//        if (cb) cb(data);
        return ok;
#else //gpu upload in fg thread
//        myprintf(22, "GpuCanvas pivot" ENDCOLOR);
        uint32_t* pxbuf32 = reinterpret_cast<uint32_t*>(this->pxbuf.cast->pixels);
        encode(pixels, pxbuf32);
        delta = now() - fg.previous; fg.encode_time += delta; fg.previous += delta;
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
//            if (txr_busy++) err(RED_MSG "txr busy, shouldn't be" ENDCOLOR);
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
            return err(RED_MSG "Can't update texture" ENDCOLOR);
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
//        uint64_t unknown_time = elapsed - caller_time - encode_time - update_time - unlock_time - copy_time - present_time; //unaccounted for; probably function calls, etc
        uint64_t idle_time = isRPi()? bg.present_time: bg.unlock_time; //kludge: V-sync delay appears to be during unlock on desktop
        double elaps = elapsed(started) + StatsAdjust, fps = numfr / elaps;
//#define avg_ms(val)  (double)(1000 * (val)) / (double)freq / (double)numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
//#define avg_ms(val)  (elapsed(now() - (val)) / numfr)  //ticks / freq / #fr
#define avg_ms(val)  (double)(1000 * (val) / SDL_TickFreq()) / numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
        uint32_t numfr_cpy = numfr, numerr_cpy = numerr, numdirty_cpy = num_dirty, frrate_cpy = frame_rate; //kludge: avoid "deleted function" error on atomic
//        myprintf(12, YELLOW_MSG "#fr %d, #err %d, elapsed %2.1f sec, %2.1f fps: %2.1f msec: fg(caller %2.3f + pivot %2.3f + lock %2.3f + update %2.3f + unlock %2.3f), bg(copy %2.3f + present %2.3f), %2.1f%% idle" ENDCOLOR, numfr_cpy, numerr_cpy, elaps, fps, 1000 / fps, avg_ms(fg.caller_time), avg_ms(fg.encode_time), avg_ms(fg.lock_time), avg_ms(fg.update_time), avg_ms(fg.unlock_time), avg_ms(bg.copy_time), avg_ms(bg.present_time), (double)100 * idle_time / elaps);
//        myprintf(22, BLUE_MSG "raw: elapsed %s, freq %s, fg(caller %s, pivot %s, lock %s, update %s, unlock %s), bg(copy %s, present %s)" ENDCOLOR, commas(now() - started), commas(SDL_TickFreq()), commas(fg.caller_time), commas(fg.encode_time), commas(fg.lock_time), commas(fg.update_time), commas(fg.unlock_time), commas(bg.copy_time), commas(bg.present_time));
//NOTE: need to subtract 1 for actual frame rate (RenderPresent); uses time diff which requires 2 frames
        myprintf(12, YELLOW_MSG "actual frame rate: %2.1f msec" ENDCOLOR, (double)frrate_cpy / (numfr_cpy - 1) / SDL_TickFreq() * 1000);
        myprintf(12, YELLOW_MSG "#fr %d, #err %d, #dirty %d (%2.1f%%), elapsed %2.1f sec, %2.1f fps, %2.1f msec avg: fg(user %2.3f + caller %2.3f + pivot %2.3f), bg(caller %2.3f + pivot %2.3f + lock %2.3f + update %2.3f + unlock %2.3f + copy %2.3f + present %2.3f), bg %2.1f%% idle" ENDCOLOR, 
            numfr_cpy, numerr_cpy, numdirty_cpy, (double)100 * numdirty_cpy / numfr_cpy, elaps, fps, 1000 / fps, 
            avg_ms(fg.user_time), avg_ms(fg.caller_time), avg_ms(fg.encode_time), 
            avg_ms(bg.caller_time), avg_ms(bg.encode_time), avg_ms(bg.lock_time), avg_ms(bg.update_time), avg_ms(bg.unlock_time), avg_ms(bg.copy_time), avg_ms(bg.present_time), 
            (double)100 * idle_time / (now() - started));
        myprintf(22, BLUE_MSG "raw: elapsed %s, freq %s, fg(user %s, caller %s, pivot %s), bg(caller %s, pivot %s, lock %s, update %s, unlock %s, copy %s, present %s)" ENDCOLOR, 
            commas(now() - started), commas(SDL_TickFreq()), 
            commas(fg.user_time), commas(fg.caller_time), commas(fg.encode_time), 
            commas(bg.caller_time), commas(bg.encode_time), commas(bg.lock_time), commas(bg.update_time), commas(bg.unlock_time), commas(bg.copy_time), commas(bg.present_time));
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
#else //def MULTI_THREADED
public:
//repaint screen (fg thread):
//NOTE: xfrs data to canvas then returns
//actual render occurs asynchronously in bkg; texture won't be displayed until next video frame (v-synced at 60 Hz)
//this allows Node.js event loop to remain responsive
//NOTE: caller owns pixel buf; leave it in caller's memory so it can be updated (need to copy here for pivot anyway)
//NOTE: pixel array assumed to be the correct size here (already checked by Javascript wrapper before calling)
//    inline int xyofs(int x, int y) { return x * this->univ_len + y; }
    bool Paint(uint32_t* pixels = 0, uint64_t render_delay = 0, callback cb = 0, void* cbdata = 0) //void (*cb)(void) = 0) //, void* cb)
    {
        uint64_t delta;
myprintf(22, BLUE_MSG "paint here1 @%2.1f msec, pixels 0x%lx, cb 0x%lx, cbdata 0x%lx" ENDCOLOR, PresentTime() * 1000, (long)pixels, (long)cb, (long)cbdata);
        delta = now() - times.previous; times.previous += delta; times.caller += delta;
//myprintf(22, BLUE_MSG "paint here1b" ENDCOLOR, pixels);
        if (pixels) //updated data from caller (optional)
        {
//myprintf(22, BLUE_MSG "paint here1c" ENDCOLOR);
            ++num_dirty;
            { //scope for locked texture
//myprintf(22, BLUE_MSG "paint here2" ENDCOLOR);
                auto_ptr<SDL_LockedTexture> lock_HERE(canvas.cast); //SDL_LOCK(canvas));
//                delta = now() - fg.previous; fg.lock_time += delta; fg.previous += delta;
//NOTE: pixel data must be in correct (texture) format
//NOTE: SDL_UpdateTexture is reportedly slow; use Lock/Unlock and copy pixel data directly for better performance
//            memcpy(lock.data.surf.pixels, pxbuf.cast->pixels, pxbuf.cast->pitch * pxbuf.cast->h);
//myprintf(22, BLUE_MSG "paint here3" ENDCOLOR);
                encode(pixels, (uint32_t*)lock.data.surf.pixels);
//myprintf(22, BLUE_MSG "paint here4" ENDCOLOR);
                delta = now() - times.previous; times.previous += delta; times.encode += delta;
            }
            if (cb) cb(cbdata, tristate::No);
//myprintf(22, BLUE_MSG "paint here5" ENDCOLOR);
//            delta = now() - fg.previous; fg.encode_time += delta; fg.previous += delta;
//slower; doesn't work with streaming texture?
//        if (!OK(SDL_UpdateTexture(canvas, NORECT, pxbuf.cast->pixels, pxbuf.cast->pitch)))
//            return err(RED_MSG "Can't update texture" ENDCOLOR);
        	if (!OK(SDL_RenderCopy(renderer, canvas, &Hclip, NORECT))) //&renderQuad, angle, center, flip ))
            {
                err(RED_MSG "Unable to render to screen" ENDCOLOR);
                ++numerr;
            }
            delta = now() - times.previous; times.previous += delta; times.update += delta;
        }
        else if (cb) cb(cbdata, tristate::No);
//        while (render_delay > 0) //caller wants to throttle back frame rate
//        if (render_delay) render_delay = 
//    double PresentTime() { return started? elapsed(started): -1; } //presentation timestamp (according to bkg rendering thread)
//    void ResetElapsed(double elaps = 0) { started = now() - elaps * SDL_TickFreq(); }
//myprintf(22, BLUE_MSG "paint here6" ENDCOLOR);
        for (;;) //throttle back frame rate
        {
//            int age = 1000 * (times.previous - render_timestamp) / SDL_TickFreq(); //time since previous RenderPresent (msec)
//            if (age >= render_delay - 5) break;
//            usleep()
            long delay = render_delay - times.previous;
            myprintf(22, BLUE_MSG "throttle paint: delay %ld ticks = %ld usec" ENDCOLOR, delay, delay * 1000000 / SDL_TickFreq());
            if (delay < 2000) break; //2 msec close enough (compensate for O/S timing granularity)
            usleep(delay * 1000000 / SDL_TickFreq()); //blocking; NOTE: might wake prematurely (signals, etc)
            delta = now() - times.previous; times.previous += delta; times.throttle += delta;
        }
        myprintf(22, BLUE_MSG "paint: RenderPresent @%2.1f msec" ENDCOLOR, PresentTime() * 1000);
        SDL_RenderPresent(renderer); //update screen; NOTE: blocks until next V-sync (on RPi)
        delta = now() - times.previous; times.previous += delta; times.render += delta;
//myprintf(22, "render present: old ts %2.3f sec, new ts %2.3f, rate %2.3f + %2.3f = %2.3f" ENDCOLOR, 
//(double)render_timestamp / SDL_TickFreq(), (double)times.previous / SDL_TickFreq(), 
//(double)frame_rate / SDL_TickFreq(), (double)(times.previous - render_timestamp) / SDL_TickFreq(), (double)(frame_rate + times.previous - render_timestamp) / SDL_TickFreq());
        /*if (render_timestamp)*/ frame_rate[numfr % SIZE(frame_rate)] = times.previous - render_timestamp; //now - previous timestamp
        render_timestamp = times.previous; //now (presentation time)
//        if (!(++numfr % (60 * 10))) stats(); //show stats every 10 sec @60 FPS
        ++numfr;
//        if (elapsed(reported) >= 10) stats(); //show stats every 10 sec
        return true;
    }
    struct
    {
        double elapsed, fps;
        uint32_t numfr, numerr, num_dirty; //, frrate; //kludge: avoid "deleted function" error on atomic
//        double avg_fr, avg_fps;
        double frrate[SIZE(frame_rate)], avg_fps;
        double caller_time, encode_time, update_time, throttle_time, render_time;
    } stats_report;
//TODO: okay across threads?
    /*bool*/ void stats(bool display = false)
    {
//TODO: skip if numfr already reported
//        uint64_t elapsed = now() - started, freq = SDL_GetPerformanceFrequency(); //#ticks/second
//        uint64_t unknown_time = elapsed - caller_time - encode_time - update_time - unlock_time - copy_time - present_time; //unaccounted for; probably function calls, etc
//        uint64_t idle_time = times.render; //isRPi()? bg.present_time: bg.unlock_time; //kludge: V-sync delay appears to be during unlock on desktop
//        double elaps = elapsed(started) /*+ StatsAdjust*/, fps = numfr / elaps;
        stats_report.elapsed = elapsed(started) /*+ StatsAdjust*/;
        stats_report.fps = numfr / stats_report.elapsed;
//#define avg_ms(val)  (double)(1000 * (val)) / (double)freq / (double)numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
//#define avg_ms(val)  (elapsed(now() - (val)) / numfr)  //ticks / freq / #fr
#define avg_ms(val)  (double)(1000 * (val) / SDL_TickFreq()) / numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
//        uint32_t numfr_cpy = numfr, numerr_cpy = numerr, numdirty_cpy = num_dirty, frrate_cpy = frame_rate; //kludge: avoid "deleted function" error on atomic
        stats_report.numfr = numfr;
        stats_report.numerr = numerr;
        stats_report.num_dirty = num_dirty;
//        stats_report.frrate = frame_rate; //kludge: avoid "deleted function" error on atomic
        stats_report.avg_fps = 0;
        for (int i = 0; i < SIZE(stats_report.frrate); ++i)
        {
            stats_report.avg_fps += (i < stats_report.numfr)? frame_rate[i]: 0;
            stats_report.frrate[i] = (i < stats_report.numfr)? (double)frame_rate[i] * 1000 / SDL_TickFreq(): 0;
        }
        if (stats_report.numfr) stats_report.avg_fps = (double)std::min<int>(SIZE(stats_report.frrate), stats_report.numfr) / stats_report.avg_fps * SDL_TickFreq();
//        myprintf(12, YELLOW_MSG "#fr %d, #err %d, elapsed %2.1f sec, %2.1f fps: %2.1f msec: fg(caller %2.3f + pivot %2.3f + lock %2.3f + update %2.3f + unlock %2.3f), bg(copy %2.3f + present %2.3f), %2.1f%% idle" ENDCOLOR, numfr_cpy, numerr_cpy, elaps, fps, 1000 / fps, avg_ms(fg.caller_time), avg_ms(fg.encode_time), avg_ms(fg.lock_time), avg_ms(fg.update_time), avg_ms(fg.unlock_time), avg_ms(bg.copy_time), avg_ms(bg.present_time), (double)100 * idle_time / elaps);
//        myprintf(22, BLUE_MSG "raw: elapsed %s, freq %s, fg(caller %s, pivot %s, lock %s, update %s, unlock %s), bg(copy %s, present %s)" ENDCOLOR, commas(now() - started), commas(SDL_TickFreq()), commas(fg.caller_time), commas(fg.encode_time), commas(fg.lock_time), commas(fg.update_time), commas(fg.unlock_time), commas(bg.copy_time), commas(bg.present_time));
//        double actual_fr = (double)frrate_cpy / numfr_cpy / SDL_TickFreq() * 1000;
//        double actual_fps = (double)numfr_cpy * SDL_TickFreq() / frrate_cpy);
        stats_report.caller_time = avg_ms(times.caller);
        stats_report.encode_time = avg_ms(times.encode);
        stats_report.update_time = avg_ms(times.update);
        stats_report.throttle_time = avg_ms(times.throttle);
        stats_report.render_time = avg_ms(times.render);
//        stats_report.avg_fr = (double)stats_report.frrate / (stats_report.numfr - 1) / SDL_TickFreq() * 1000;
//        stats_report.avg_fps = (double)(stats_report.numfr - 1) * SDL_TickFreq() / stats_report.frrate;
        if (!display) return;

        myprintf(12, YELLOW_MSG "#fr %d, #err %d, #dirty %d (%2.1f%%), elapsed %2.1f sec, %2.1f fps, avg %2.1f msec = caller %2.3f + encode %2.3f + update %2.3f + throttle %2.3f + render %2.3f" ENDCOLOR, //, %2.1f%% idle" ENDCOLOR, 
//            numfr_cpy, numerr_cpy, numdirty_cpy, (double)100 * numdirty_cpy / numfr_cpy, elaps, fps, 1000 / fps, 
//            avg_ms(times.caller), avg_ms(times.encode), avg_ms(times.update), avg_ms(times.render));
            stats_report.numfr, stats_report.numerr, stats_report.num_dirty, (double)100 * stats_report.num_dirty / stats_report.numfr, stats_report.elapsed, stats_report.fps, 1000 / stats_report.fps, 
            stats_report.caller_time, stats_report.encode_time, stats_report.update_time, stats_report.throttle_time, stats_report.render_time);
//            (double)100 * idle_time / (now() - started));
        char buf[SIZE(stats_report.frrate) * 10 + 2] = ", 0", *bp = buf;
        for (int i = 0; (i < SIZE(stats_report.frrate)) && (i < numfr); ++i)
            bp += sprintf(bp, ", %2.1f", stats_report.frrate[i]);
        myprintf(12, YELLOW_MSG "frame rate: %s msec (%2.1f fps)" ENDCOLOR, buf + 2, stats_report.avg_fps);
        myprintf(22, BLUE_MSG "raw: elapsed %s, freq %s, caller %s, encode %s, update %s, throttle %s, render %s" ENDCOLOR, 
            commas(now() - started), commas(SDL_TickFreq()), //commas(stats_report.frrate), //frrate_cpy),
            commas(times.caller), commas(times.encode), commas(times.update), commas(times.throttle), commas(times.render));
//        myprintf(22, "raw-raw: elapsed %ld, freq %ld" ENDCOLOR, now() - started, SDL_TickFreq());
//        reported = times.previous;
//        return true;
    }
#endif //def MULTI_THREADED
private:
//encode (24-bit pivot):
//CAUTION: expensive CPU loop here
//NOTE: need pixel-by-pixel copy for several reasons:
//- ARGB -> RGBA (desirable)
//- brightness limiting (recommended)
//- blending (optional)
//- 24-bit pivots (required, non-dev mode)
//        memset(pixels, 4 * this->w * this->h, 0);
//TODO: perf compare inner/outer swap
//TODO? locality of reference: keep nodes within a universe close to each other (favors caller)
    void encode(uint32_t* src, uint32_t* dest) //fill = BLACK)
    {
        if (!src) return;
//        uint32_t* pxbuf32 = reinterpret_cast<uint32_t*>(this->pxbuf.cast->pixels);
        uint64_t start = now();
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
//univ types:
//WS281X: send 1 WS281X node (24 bits) per display row, up to #display rows on screen per frame
//SSR: send 2 bytes per display row, multiples of 3 * 8 * 7 + 2 == 170 bytes (85 display rows) per frame
//OR? send 3 bytes per display row, multiples of 57 display rows
//*can't* send 4 bytes per display row; PIC can only rcv 3 bytes per WS281X
//72 display pixels: H-sync = start bit or use inverters?
//        uint32_t leading_edges = BLACK;
//        for (uint32_t x = 0, xmask = 0x800000; (int)x < this->num_univ; ++x, xmask >>= 1)
//            if (UTYPEOF(this->UnivTypes[(int)x]) == WS281X) leading_edges |= xmask; //turn on leading edge of data bit for GPIO pins for WS281X only
//myprintf(22, BLUE_MSG "start bits = 0x%x (based on univ type)" ENDCOLOR, leading_edges);
//        bool rbswap = isRPi();
        col_debug();
        for (int y = 0, yofs = 0; y < this->univ_len; ++y, yofs += TXR_WIDTH) //outer
        {
//initialize 3x signal for this row of 24 WS281X pixels:
//            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
//            {
//                pxbuf32[yofs + x3 + 0] = leading_edges; //WHITE;
//                pxbuf32[yofs + x3 + 1] = BLACK; //data bit body (will be overwritten with pivoted color bits)
////                if (x3) pxbuf32[yofs + x3 - 1] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//                pxbuf32[yofs + x3 + 2] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//            }
            memset(&dest[yofs], 0, TXR_WIDTH * sizeof(uint32_t));
#if 1
//pivot pixel data onto 24 parallel GPIO pins:
//  WS281X = 1, PLAIN_SSR = 2, CHPLEX_SSR = 3,TYPEBITS = 0xF,
// RGSWAP = 0x20, CHECKSUM = 0x40, POLARITY = 0x80};
            if (!this->DEV_MODE)
            {
//NOTE: xmask loop assumes ARGB or ABGR fmt (A in upper byte)
                for (uint32_t x = 0, xofs = 0, xmask = 0x800000; x < (uint32_t)this->num_univ; ++x, xofs += this->univ_len, xmask >>= 1)
            	{
                    uint24_t color_out = src[xofs + y]; //pixels? pixels[xofs + y]: fill;
//TODO: make this extensible, move out to Javascript?
                    switch (univ_types[x])
                    {
                        case WS281X | RGSWAP:
                            color_out = ARGB2ABGR(color_out); //user-requested explicit R <-> G swap
//fall thru
                        case WS281X:
//                            if (!A(color) || (!R(color) && !G(color) && !B(color))) continue; //no data to pivot
                            color_out = limit(color_out); //limit brightness/power
//no                            color = ARGB2ABGR(color); //R <-> G swap doesn't need to be automatic for RPi; user can swap GPIO pins
//24 WS281X data bits spread across 72 screen pixels = 3 pixels per WS281X data bit:
                            for (int bit3 = 0; bit3 < TXR_WIDTH; bit3 += 3, color_out <<= 1)
                            {
                                dest[yofs + bit3 + 0] |= xmask; //leading edge = high
                                if (color_out & 0x800000) dest[yofs + bit3 + 1] |= xmask; //set data bit
//                                pxbuf32[yofs + bit3 + 2] &= ~xmask; //trailing edge = low
                            }
//                            row_debug("ws281x", yofs, xmask, x);
                            break;
                        case PLAIN_SSR:
                        case PLAIN_SSR | CHECKSUM:
                        case PLAIN_SSR | POLARITY:
                        case PLAIN_SSR | CHECKSUM | POLARITY:
                            return_void(err(RED_MSG "GpuCanvas.Encode: Plain SSR TODO" ENDCOLOR));
                            break;
                        case CHPLEX_SSR:
                        case CHPLEX_SSR | CHECKSUM:
                        case CHPLEX_SSR | POLARITY:
                        case CHPLEX_SSR | CHECKSUM | POLARITY:
                        { //kludge: extra scope to avoid "jump to case label" error
//cfg + chksum + 8 * 7 * (delay, rowmap, colmap) == 170 bytes @ 2 bytes / row == 85 display rows of data
//NOTE: disp size expands; can't display all rows; last ctlr might be partial
#define BYTES_PER_DISPROW  2
#define CHPLEX_DISPROWS  divup(1 + 1 + 3 * NUM_SSR * (NUM_SSR - 1), BYTES_PER_DISPROW) //85
#define CHPLEX_CTLRLEN  (NUM_SSR * (NUM_SSR - 1))
                            int ctlr_ofs = y % CHPLEX_DISPROWS, ctlr_adrs = y / CHPLEX_DISPROWS;
                            if (!ctlr_ofs) //get another display list
                            {
                                this->encoders[x].cast->init_list();
                                no_myprintf(14, BLUE_MSG "GpuCanvas: enc[%d, %d] aggregate rows %d..%d" ENDCOLOR, x, y, ctlr_adrs * CHPLEX_CTLRLEN, (ctlr_adrs + 1) * CHPLEX_CTLRLEN - 1);
                                for (int yy = ctlr_adrs * CHPLEX_CTLRLEN; (yy < this->univ_len) && (yy < (ctlr_adrs + 1) * CHPLEX_CTLRLEN); ++yy)
                                {
                                    color_out = src[xofs + yy]; //pixels? pixels[xofs + yy]: fill;
                                    uint8_t brightness = std::max<int>(Rmask(color_out) >> 16, std::max<int>(Gmask(color_out) >> 8, Bmask(color_out))); //use strongest color element
                                    no_myprintf(14, BLUE_MSG "pixel[%d] 0x%x -> br %d" ENDCOLOR, xofs + yy, color_out, brightness);
                                    this->encoders[x].cast->insert(brightness);
                                }
                                this->encoders[x].cast->resolve_conflicts();
                                no_myprintf(14, BLUE_MSG "GpuCanvas: enc[%d, %d] aggregated into %d disp evts" ENDCOLOR, x, y, this->encoders[x].cast->disp_count);
                            }
//2 bytes serial data = 2 * (1 start + 8 data + 1 stop + 2 pad) = 24 data bits spread across 72 screen pixels = 3 pixels per serial data bit:
//pkt contents: ssr_cfg, checksum, display list (brightness, row map, col map)
//                            uint8_t byte_even = (ctlr_ofs < 0)? (uint8_t)univ_types[x]: ((uint8_t*)(&this->encoders[x].cast->DispList[0].delay)[2 * ctlr_ofs + 0];
//                            uint8_t byte_odd = (ctlr_ofs < 0)? this->encoders[x].cast->checksum: (this->encoders[x].cast->DispList[2 * ctlr_ofs + 1];
                            uint8_t byte_even = !ctlr_ofs? (uint8_t)univ_types[x]: this->encoders[x].cast->DispList[2 * ctlr_ofs - 2];
                            uint8_t byte_odd = !ctlr_ofs? this->encoders[x].cast->checksum ^ (uint8_t)univ_types[x]: this->encoders[x].cast->DispList[2 * ctlr_ofs - 1]; //CAUTION: incl univ type in checksum
                            color_out = 0x800000 | (byte_even << (12+3)) | 0x800 | (byte_odd << 3); //NOTE: inverted start + stop bits; using 3 stop bits
//myprintf(14, BLUE_MSG "even 0x%x, odd 0x%x -> color_out 0x%x" ENDCOLOR, byte_even, byte_odd, color_out);
                            for (int bit3 = 0; bit3 < TXR_WIDTH; bit3 += 3, color_out <<= 1)
                                if (color_out & 0x800000) //set data bit
                                {
                                    dest[yofs + bit3 + 0] |= xmask;
                                    dest[yofs + bit3 + 1] |= xmask;
                                    dest[yofs + bit3 + 2] |= xmask;
                                }
//                            row_debug("chplex", yofs, xmask, x);
                            break;
                        }
                        default:
                            return_void(err(RED_MSG "GpuCanvas.Encode: Unknown universe type[%d]: %d flags 0x%x" ENDCOLOR, x, univ_types[x] & TYPEBITS, univ_types[x] & ~TYPEBITS));
                            break;
                    }
            	}
//                row_debug("aggregate", yofs);
                continue; //next row
            }
#endif
//just copy pixels as-is (dev/debug only):
            bool rbswap = isRPi(); //R <-> G swap only matters for as-is display; for pivoted data, user can just swap I/O pins
            for (int x = 0, x3 = 0, xofs = 0; x < this->num_univ; ++x, x3 += 3, xofs += this->univ_len)
            {
                uint32_t color = src[xofs + y]; //pixels? pixels[xofs + y]: fill;
                if (rbswap) color = ARGB2ABGR(color);
                dest[yofs + x3 + 0] = dest[yofs + x3 + 1] = dest[yofs + x3 + 2] = color;
            }
        }
//        if (this->WantPivot) dump("canvas", pixels, elapsed(start));
    }
    void col_debug()
    {
return; //dump to file instead
        char buf[12 * TXR_WIDTH / 3 + 1], *bp = buf;
        for (int x = 0; x < TXR_WIDTH / 3; ++x)
            bp += sprintf(bp, ", %d + 0x%x", this->univ_types[x] & TYPEBITS, this->univ_types[x] & ~TYPEBITS & 0xFF);
        *bp = '\0';
        myprintf(18, BLUE_MSG "Encode: pivot? %d, utypes %s" ENDCOLOR, !this->DEV_MODE, buf + 2);
    }
    void row_debug(const char* desc, int yofs, uint32_t xmask = 0, int col = -1)
    {
return; //dump to file instead
        uint32_t* pxbuf32 = NULL; //TODO: reinterpret_cast<uint32_t*>(this->pxbuf.cast->pixels);
//        char buf[2 * TXR_WIDTH / 3 + 1], *bp = buf;
        char buf[10 * TXR_WIDTH + 1], *bp = buf + (xmask? 1: 0);
        for (int x = 0; x < TXR_WIDTH; ++x)
            if (!xmask) bp += sprintf(bp, (pxbuf32[yofs + x] < 10)? ", %d": ", 0x%x", pxbuf32[yofs + x]); //show hex value (all bits) for each bit
            else if (!(x % 3)) bp += sprintf(bp, " %d", ((pxbuf32[yofs + x + 0] & xmask)? 4: 0) + ((pxbuf32[yofs + x + 1] & xmask)? 2: 0) + ((pxbuf32[yofs + x + 2] & xmask)? 1: 0)); //show as 1 digit per bit
        *bp = '\0';
        myprintf(18, BLUE_MSG "Encode: %s[%d] row[%d/%d]: %s" ENDCOLOR, desc, col, yofs / TXR_WIDTH, this->univ_len, buf + 2);
    }
#if 0
    int dump_count; //= 0;
    void dump(const char* desc, uint32_t* pixels, double duration)
    {
        if (!this->DumpFile.length()) return;
        FILE* fp = fopen(this->DumpFile.c_str(), "a");
        if (!fp) return;
//        static std::mutex protect;
//        std::lock_guard<std::mutex>(protect);
//        protect.lock();

        if (!dump_count++)
        {
            time_t now;
            time(&now);
            struct tm* tp = localtime(&now);
            fprintf(fp, "-------- %d/%d/%.4d %d:%.2d:%.2d --------\n", tp->tm_mon + 1, tp->tm_mday, tp->tm_year + 1900, tp->tm_hour, tp->tm_min, tp->tm_sec);
        }
        fprintf(fp, "frame[%d] hex contents (fmt time %3.4f msec)\n", dump_count, 1000 * duration);

        
        for (int y = 0; y < this->univ_len; ++y)
        {
            char buf[10 * TXR_WIDTH + 1], *bp = buf; //, *last_nz = 0;
            int is_more = 0;
            for (int x = 0, xofs = 0; x < num_univ; ++x, xofs += this->univ_len)
            {
                bp += sprintf(bp, ", %6x", pixels? pixels[xofs + y] & 0xFFFFFF: 0);
//                if (pixels[xofs + y]) last_nz = bp;
                if (!pixels || is_more) continue;
                for (int yy = y + 1; yy < this->univ_len; ++yy)
                    if (pixels[xofs + yy] & 0xFFFFFF) { is_more = xofs + yy; break; }
            }
            *bp = '\0';
//            if (!last_nz) continue;
            fprintf(fp, "row-in*%d[%.2d/%d]: %s\n", dump_count, y, this->univ_len, buf + 2); //, is_more / this->univ_len, is_more % this->univ_len);
            if (!is_more) break;
        }

        uint32_t xmask = 0x400000; //0x800000;
        uint32_t* pxbuf32 = reinterpret_cast<uint32_t*>(this->pxbuf.cast->pixels);
        for (int y = 0, yofs = 0; y < this->univ_len; ++y, yofs += TXR_WIDTH)
        {
            char chbuf[TXR_WIDTH / 3 + 1];
            char buf[10 * TXR_WIDTH + 1], *bp = buf;
            int is_more = 0;
            for (int x = 0; x < TXR_WIDTH; ++x)
            {
                chbuf[x / 3] = (pxbuf32[yofs + x] & xmask)? '1': '0';
                bp += sprintf(bp, ", %6x", pxbuf32[yofs + x] & 0xFFFFFF);
                if (is_more) continue;
                for (int yy = y + 1, yyofs = (y + 1) * TXR_WIDTH; yy < this->univ_len; ++yy, yyofs += TXR_WIDTH)
                    if (pxbuf32[yyofs + x] & 0xFFFFFF) { is_more = yyofs + x; break; }
            }
            *bp = chbuf[TXR_WIDTH / 3] = '\0';
            fprintf(fp, "pivot-out*%d[%.2d/%d]: %s; ch & 0x%x: %.1s %.8s %.4s %.8s %.3s\n", dump_count, y, this->univ_len, buf + 2, xmask, chbuf, chbuf + 1, chbuf + 9, chbuf + 13, chbuf + 21);
            if (!is_more) break;
        }
        fflush(fp);
//        protect.unlock();
//TODO: leave file open and just flush
        fclose(fp);
    }
#endif
#if 0 //some sources say to only do it once
public:
//initialize SDL:
    static bool Init()
    {
	    sdl = SDL_INIT(/*SDL_INIT_AUDIO |*/ SDL_INIT_VIDEO); //| SDL_INIT_EVENTS); //events, file, and threads are always inited, so they don't need to be flagged here; //| ??SDL_INIT_TIMER); //SDL_init(SDL_INIT_EVERYTHING);
        if (!OK(sdl)) exc(RED_MSG "SDL init failed" ENDCOLOR);
        debug_info(sdl);

//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0") != SDL_TRUE) //set texture filtering to linear; TODO: is this needed?
            err(YELLOW_MSG "Warning: Linear texture filtering not enabled" ENDCOLOR);
//TODO??    SDL_bool SDL_SetHintWithPriority(const char*      name, const char*      value,SDL_HintPriority priority)
 
#if 0 //not needed
//use OpenGL 2.1:
        if (!OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2)) || !OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1)))
            return err(RED_MSG "Can't set GL version to 2.1" ENDCOLOR);
        if (!OK(SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8)) || !OK(SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8)) || !OK(SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8))) //|| !OK(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8)))
            return err(RED_MSG "Can't set GL R/G/B to 8 bits" ENDCOLOR);
//??SDL_GL_BUFFER_SIZE //the minimum number of bits for frame buffer size; defaults to 0
//??SDL_GL_DOUBLEBUFFER //whether the output is single or double buffered; defaults to double buffering on
//??SDL_GL_DEPTH_SIZE //the minimum number of bits in the depth buffer; defaults to 16
//??SDL_GL_STEREO //whether the output is stereo 3D; defaults to off
//??SDL_GL_MULTISAMPLEBUFFERS //the number of buffers used for multisample anti-aliasing; defaults to 0; see Remarks for details
//??SDL_GL_MULTISAMPLESAMPLES //the number of samples used around the current pixel used for multisample anti-aliasing; defaults to 0; see Remarks for details
//??SDL_GL_ACCELERATED_VISUAL //set to 1 to require hardware acceleration, set to 0 to force software rendering; defaults to allow either
        if (!OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES))) //type of GL context (Core, Compatibility, ES)
            return err(RED_MSG "Can't set GLES context profile" ENDCOLOR);
//NO    if (!OK(SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1)))
//        return err(RED_MSG "Can't set shared context attr" ENDCOLOR);
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
        myprintf(3, BLUE_MSG "Quit()" ENDCOLOR);
        return true;
    }
#endif
};
inline int Release(/*const*/ GpuCanvas* that) { delete that; return SDL_Success; }
//auto_ptr<SDL_lib> GpuCanvas::sdl;
//int GpuCanvas::count = 0;


#if 0
    delta = now() - bg.previous; bg.unlock_time += delta; bg.previous += delta;

	if (!OK(SDL_RenderCopy(renderer, canvas, &Hclip, NORECT))) //&renderQuad, angle, center, flip ))
    {
        err(RED_MSG "Unable to render to screen" ENDCOLOR);
        ++numerr;
    }
    delta = now() - bg.previous; bg.copy_time += delta; bg.previous += delta;


    SDL_RenderPresent(renderer); //update screen; NOTE: blocks until next V-sync (on RPi)
    delta = now() - bg.previous; bg.present_time += delta; bg.previous += delta; //== now()
#endif

#if 0
//window create and redraw:
//all GPU work done in fg thread (synchronously)
class SimplerCanvas
{
private:
    auto_ptr<SDL_Window> window;
    auto_ptr<SDL_Renderer> renderer;
    auto_ptr<SDL_Texture> canvas;
//    auto_ptr<SDL_mutex> busy;
    SDL_Rect Hclip;
//performance stats:
    uint64_t started; //doesn't need to be atomic; won't be modified after fg thread wakes
    uint64_t render_timestamp, frame_rate;
//NO: could be updated by another thread
    uint32_t numfr, numerr, num_dirty;
    struct { uint64_t previous, caller, update, encode, render; } times;
    int num_univ, univ_len; //caller-defined
public:
    bool WantPivot; //dev/debug vs. live mode; TODO: getter/setter
//    std::string DumpFile;
//    std::atomic<double> PresentTime; //presentation timestamp (set by bkg rendering thread)
    double PresentTime() { return started? elapsed(started): -1; } //presentation timestamp (according to bkg rendering thread)
    int StatsAdjust; //allow caller to tweak stats
//CAUTION: must match firmware
//        unsigned unused: 5; //lsb
//        unsigned RGswap: 1;
//        unsigned IgnoreChksum: 1;
//        unsigned ActiveHigh: 1; //msb
#define UTYPEOF(univtype)  ((univtype) & TYPEBITS)
    enum UniverseTypes { INVALID = -1, NONE = 0, WS281X = 1, PLAIN_SSR = 2, CHPLEX_SSR = 3, SPAREBIT = 0x10, TYPEBITS = 0xF, RGSWAP = 0x20, CHECKSUM = 0x40, POLARITY = 0x80, ACTIVE_HIGH = 0x80, ACTIVE_LOW = 0}; //WS281X is default, but make non-0 to see if explicitly set; TODO: make extensible
    UniverseTypes UnivType(int inx, UniverseTypes newtype = INVALID)
    {
        if ((inx < 0) || (inx >= this->num_univ)) return INVALID;
        UniverseTypes oldtype = this->univ_types[inx];
        if ((newtype != INVALID) && (UTYPEOF(newtype) != UTYPEOF(oldtype)))
        {
//            myprintf(14, (UTYPEOF(newtype) != UTYPEOF(INVALID))? BLUE_MSG "GpuCanvas: UnivType[%d] was %d + flags 0x%d -> is now %d + flags 0x%x" ENDCOLOR: BLUE_MSG "GpuCanvas: UnivType[%d] is %d + flags 0x%d" ENDCOLOR, inx, oldtype & TYPEBITS, oldtype & ~TYPEBITS & 0xFF, newtype & TYPEBITS, newtype & ~TYPEBITS & 0xFF);
            if (UTYPEOF(newtype) == UTYPEOF(CHPLEX_SSR)) encoders[inx] = new ChplexEncoder<NUM_SSR>; //alloc memory while caller is still in prep, prior to playback
            this->univ_types[inx] = newtype;
        }
        return oldtype;
    }
    int width() { return this->num_univ; }
    int height() { return this->univ_len; }
private:
    std::vector<UniverseTypes> univ_types; //universe types
    std::vector<auto_ptr<ChplexEncoder<NUM_SSR>>> encoders;
public:
    SimplerCanvas(const char* title, int num_univ, int univ_len): started(now()), StatsAdjust(0)
    {
        if (!SDL) SDL = SDL_INIT(SDL_INIT_VIDEO);
        if (!SDL_WasInit(SDL_INIT_VIDEO)) err(RED_MSG "ERROR: Tried to get canvas before SDL_Init" ENDCOLOR);
        if (!title) title = "GpuCanvas";
        myprintf(3, BLUE_MSG "Init: title '%s', #univ %d, univ len %d" ENDCOLOR, title, num_univ, univ_len);

//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0") != SDL_TRUE) //set texture filtering to linear; TODO: is this needed?
            err(YELLOW_MSG "Warning: Linear texture filtering not enabled" ENDCOLOR);
//TODO??    SDL_bool SDL_SetHintWithPriority(const char*      name, const char*      value,SDL_HintPriority priority)

#define IGNORED_X_Y_W_H  0, 0, 200, 100 //not used for full screen mode
//leave window on main thread so it can process events:
//https://stackoverflow.com/questions/6172020/opengl-rendering-in-a-secondary-thread
        window = isRPi()?
            SDL_CreateWindow(title, IGNORED_X_Y_W_H, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN): //| SDL_WINDOW_OPENGL): //don't use OpenGL; too slow
            SDL_CreateWindow(title, 10, 10, MaxFit().w, MaxFit().h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN); //| SDL_WINDOW_OPENGL);
        if (!window) return_void(exc(RED_MSG "Create window failed" ENDCOLOR));
        uint32_t fmt = SDL_GetWindowPixelFormat(window); //desktop OpenGL: 24 RGB8888, RPi: 32 ARGB8888
        if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(exc(RED_MSG "Can't get window format" ENDCOLOR));
        int wndw, wndh;
        SDL_GL_GetDrawableSize(window, &wndw, &wndh);
        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        SDL_GetWindowSize(window, &wndw, &wndh);
//        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        SDL_GetWindowMaximumSize(window, &wndw, &wndh);
//        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//        int top, left, bottom, right;
//        if (!OK(SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right))) return_void(exc(RED_MSG "Can't get window border size" ENDCOLOR));
//        myprintf(22, BLUE_MSG "wnd border size: t %d, l %d, b %d, r %d" ENDCOLOR, top, left, bottom, right);
        debug_info(window);
#undef IGNORED_X_Y_W_H

        if ((num_univ < 1) || (num_univ > WS281X_BITS)) return_void(exc(RED_MSG "Bad number of universes: %d (should be 1..%d)" ENDCOLOR, num_univ, WS281X_BITS));
        if ((univ_len < 1) || (univ_len > wndh)) return_void(exc(RED_MSG "Bad universe size: %d (should be 1..%d)" ENDCOLOR, univ_len, wndh));
//NOTE: to avoid fractional pixels, screen/window width should be a multiple of 71, height should be a multiple of univ_len
        if (wndw % (TXR_WIDTH - 1)) myprintf(1, YELLOW_MSG "Window width %d is not a multiple of %d" ENDCOLOR, wndw, TXR_WIDTH - 1);
        if (wndh % univ_len) myprintf(1, YELLOW_MSG "Window height %d is not a multiple of %d" ENDCOLOR, wndh, univ_len);
        this->num_univ = num_univ;
        this->univ_len = univ_len;
        this->univ_types.resize(num_univ, NONE); //NOTE: caller needs to call paint() after changing this
        this->encoders.resize(num_univ, NULL);

	    renderer = SDL_CreateRenderer(window, FIRST_MATCH, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); //NOTE: PRESENTVSYNC syncs with V refresh rate (typically 60 Hz)
        if (!renderer) return_void(err(RED_MSG "Create renderer failed" ENDCOLOR));
        debug_info(renderer);

        canvas = SDL_CreateTexture(renderer, fmt, SDL_TEXTUREACCESS_STREAMING, TXR_WIDTH, univ_len); //SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, wndw, wndh);
        if (!canvas) return_void(err(RED_MSG "Can't create canvas texture" ENDCOLOR));

        Hclip = {0, 0, TXR_WIDTH - 1, univ_len}; //CRITICAL: clip last col (1/3 pixel) so it overlaps with H-sync
        myprintf(22, BLUE_MSG "clip rect (%d, %d, %d, %d)" ENDCOLOR, Hclip.x, Hclip.y, Hclip.w, Hclip.h);

        myprintf(8, MAGENTA_MSG "canvas startup took %2.1f msec" ENDCOLOR, elapsed(started));
        times.caller = times.encode = times.update = times.render = numfr = numerr = num_dirty = 0;
        started = times.previous = now();
        render_timestamp = frame_rate = 0;
    }
    ~SimplerCanvas()
    {
//        myprintf(22, BLUE_MSG "GpuCanvas dtor" ENDCOLOR);
        stats();
//        this->done = true;
//        this->wake(); //eof; tell bkg thread to quit (if it's waiting)
        myprintf(22, YELLOW_MSG "GpuCanvas dtor" ENDCOLOR);
    }
public:
//repaint screen (fg thread):
//NOTE: xfrs data to canvas then returns
//actual render occurs asynchronously in bkg; texture won't be displayed until next video frame (v-synced at 60 Hz)
//this allows Node.js event loop to remain responsive
//NOTE: caller owns pixel buf; leave it in caller's memory so it can be updated (need to copy here for pivot anyway)
//NOTE: pixel array assumed to be the correct size here (already checked by Javascript wrapper before calling)
//    inline int xyofs(int x, int y) { return x * this->univ_len + y; }
    bool Paint(uint32_t* pixels = 0) //, void* cb)
    {
        uint64_t delta;
        delta = now() - times.previous; times.caller += delta; times.previous += delta;
        if (pixels) //updated data from caller (optional)
        {
            { //scope for locked texture
                auto_ptr<SDL_LockedTexture> lock_HERE(canvas.cast); //SDL_LOCK(canvas));
//                delta = now() - fg.previous; fg.lock_time += delta; fg.previous += delta;
//NOTE: pixel data must be in correct (texture) format
//NOTE: SDL_UpdateTexture is reportedly slow; use Lock/Unlock and copy pixel data directly for better performance
//            memcpy(lock.data.surf.pixels, pxbuf.cast->pixels, pxbuf.cast->pitch * pxbuf.cast->h);
                encode(pixels, (uint32_t*)lock.data.surf.pixels);
                delta = now() - times.previous; times.encode += delta; times.previous += delta;
            }
//            delta = now() - fg.previous; fg.encode_time += delta; fg.previous += delta;
//slower; doesn't work with streaming texture?
//        if (!OK(SDL_UpdateTexture(canvas, NORECT, pxbuf.cast->pixels, pxbuf.cast->pitch)))
//            return err(RED_MSG "Can't update texture" ENDCOLOR);
        	if (!OK(SDL_RenderCopy(renderer, canvas, &Hclip, NORECT))) //&renderQuad, angle, center, flip ))
            {
                err(RED_MSG "Unable to render to screen" ENDCOLOR);
                ++numerr;
            }
            delta = now() - times.previous; times.update += delta; times.previous += delta;
        }
        SDL_RenderPresent(renderer); //update screen; NOTE: blocks until next V-sync (on RPi)
        delta = now() - times.previous; times.render += delta; times.previous += delta;
        return true;
    }
    bool stats()
    {
//TODO: skip if numfr already reported
//        uint64_t elapsed = now() - started, freq = SDL_GetPerformanceFrequency(); //#ticks/second
//        uint64_t unknown_time = elapsed - caller_time - encode_time - update_time - unlock_time - copy_time - present_time; //unaccounted for; probably function calls, etc
        uint64_t idle_time = times.render; //isRPi()? bg.present_time: bg.unlock_time; //kludge: V-sync delay appears to be during unlock on desktop
        double elaps = elapsed(started) + StatsAdjust, fps = numfr / elaps;
//#define avg_ms(val)  (double)(1000 * (val)) / (double)freq / (double)numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
//#define avg_ms(val)  (elapsed(now() - (val)) / numfr)  //ticks / freq / #fr
#define avg_ms(val)  (double)(1000 * (val) / SDL_TickFreq()) / numfr //(10.0 * (val) / freq) / 10.0) //(double)caller_time / freq / numfr
        uint32_t numfr_cpy = numfr, numerr_cpy = numerr, numdirty_cpy = num_dirty, frrate_cpy = frame_rate; //kludge: avoid "deleted function" error on atomic
//        myprintf(12, YELLOW_MSG "#fr %d, #err %d, elapsed %2.1f sec, %2.1f fps: %2.1f msec: fg(caller %2.3f + pivot %2.3f + lock %2.3f + update %2.3f + unlock %2.3f), bg(copy %2.3f + present %2.3f), %2.1f%% idle" ENDCOLOR, numfr_cpy, numerr_cpy, elaps, fps, 1000 / fps, avg_ms(fg.caller_time), avg_ms(fg.encode_time), avg_ms(fg.lock_time), avg_ms(fg.update_time), avg_ms(fg.unlock_time), avg_ms(bg.copy_time), avg_ms(bg.present_time), (double)100 * idle_time / elaps);
//        myprintf(22, BLUE_MSG "raw: elapsed %s, freq %s, fg(caller %s, pivot %s, lock %s, update %s, unlock %s), bg(copy %s, present %s)" ENDCOLOR, commas(now() - started), commas(SDL_TickFreq()), commas(fg.caller_time), commas(fg.encode_time), commas(fg.lock_time), commas(fg.update_time), commas(fg.unlock_time), commas(bg.copy_time), commas(bg.present_time));
        myprintf(12, YELLOW_MSG "actual frame rate: %2.1f msec" ENDCOLOR, (double)frrate_cpy / numfr_cpy / SDL_TickFreq() * 1000);
        myprintf(12, YELLOW_MSG "#fr %d, #err %d, #dirty %d (%2.1f%%), elapsed %2.1f sec, %2.1f fps, %2.1f msec avg: caller %2.3f + encode %2.3f + update %2.3f + render %2.3f, %2.1f%% idle" ENDCOLOR, 
            numfr_cpy, numerr_cpy, numdirty_cpy, (double)100 * numdirty_cpy / numfr_cpy, elaps, fps, 1000 / fps, 
            avg_ms(times.caller), avg_ms(times.encode), avg_ms(times.update), avg_ms(times.render), 
            (double)100 * idle_time / (now() - started));
        myprintf(22, BLUE_MSG "raw: elapsed %s, freq %s, caller %s, update %s, encode %s, render %s" ENDCOLOR, 
            commas(now() - started), commas(SDL_TickFreq()), 
            commas(times.caller), commas(times.update), commas(times.encode), commas(times.render));
//        myprintf(22, "raw-raw: elapsed %ld, freq %ld" ENDCOLOR, now() - started, SDL_TickFreq());
        return true;
    }
private:
//encode (24-bit pivot):
//CAUTION: expensive CPU loop here
//NOTE: need pixel-by-pixel copy for several reasons:
//- ARGB -> RGBA (desirable)
//- brightness limiting (recommended)
//- blending (optional)
//- 24-bit pivots (required, non-dev mode)
//        memset(pixels, 4 * this->w * this->h, 0);
//TODO: perf compare inner/outer swap
//TODO? locality of reference: keep nodes within a universe close to each other (favors caller)
    void encode(uint32_t* src, uint32_t* dest)
    {
//        uint64_t start = now();
//just copy pixels as-is (dev/debug only):
        bool rbswap = isRPi(); //R <-> G swap only matters for as-is display; for pivoted data, user can just swap I/O pins
        for (int y = 0, yofs = 0; y < this->univ_len; ++y, yofs += TXR_WIDTH) //outer
        {
//initialize 3x signal for this row of 24 WS281X pixels:
//            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
//            {
//                pxbuf32[yofs + x3 + 0] = leading_edges; //WHITE;
//                pxbuf32[yofs + x3 + 1] = BLACK; //data bit body (will be overwritten with pivoted color bits)
////                if (x3) pxbuf32[yofs + x3 - 1] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//                pxbuf32[yofs + x3 + 2] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//            }
            memset(&dest[yofs], 0, TXR_WIDTH * sizeof(uint32_t));
            for (int x = 0, x3 = 0, xofs = 0; x < this->num_univ; ++x, x3 += 3, xofs += this->univ_len)
            {
                uint32_t color = src[xofs + y];
                if (rbswap) color = ARGB2ABGR(color);
                dest[yofs + x3 + 0] = dest[yofs + x3 + 1] = dest[yofs + x3 + 2] = color;
            }
        }
//        if (this->WantPivot) dump("canvas", pixels, elapsed(start));
    }
};
#endif


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
// #pragma message "compiled for Node.js"
//kludge: name conflicts between XWindows and Nan/Node; use alternate names for Nan items
 #define True  True_not_XWin
 #define False  False_not_XWin
 #define None  None_not_XWin
 #include <nan.h>  // includes v8 too
// #include <node.h>
 #define main  main_not_XWin
//#endif
// #pragma message "compiled as Node.js addon"
// #include <node.h>
// #include <nan.h> //includes v8 also
// #include <v8.h>

 #ifdef RPI_NO_X
//  #include "bcm_host.h"
  #error "TODO"
 #else //def RPI_NO_X
  #include <X11/Xlib.h>
  #include <X11/extensions/xf86vmode.h> //XF86VidModeGetModeLine
  #define XScreen  Screen //avoid confusion
  #define XDisplay  Display //avoid confusion

  typedef struct { int dot_clock; XF86VidModeModeLine mode_line; } ScreenConfig;

//inline int Release(FILE* that) { return fclose(that); }
  inline int Release(XDisplay* that) { return XCloseDisplay(that); }
//inline int Release(_XDisplay*& that) { return XCloseDisplay(that); }
//inline XDisplay* XOpenDisplay_fixup(const char* name) { return XOpenDisplay(name); }

//see https://stackoverflow.com/questions/1829706/how-to-query-x11-display-resolution
//see https://tronche.com/gui/x/xlib/display/information.html#display
//or use cli xrandr or xwininfo
  const ScreenConfig* getScreenConfig() //ScreenConfig* scfg) //XF86VidModeGetModeLine* mode_line)
  {
//BROKEN    auto_ptr<XDisplay> display = XOpenDisplay(NULL);
      static ScreenConfig scfg = {0};
    if (scfg.dot_clock) return &scfg; //return cached data; screen info won't change
//    memset(&scfg, 0, sizeof(*scfg));
//    bool ok = false;
    XDisplay* display = XOpenDisplay(NULL);
    int num_screens = display? ScreenCount(display/*.cast*/): 0;
    for (int i = 0; i < num_screens; ++i)
    {
//        int dot_clock, mode_flags;
//        XF86VidModeModeLine mode_line = {0};
//        XScreen screen = ScreenOfDisplay(display.cast, i);
//see https://ubuntuforums.org/archive/index.php/t-779038.html
//xvidtune-show
//"1366x768"     69.30   1366 1414 1446 1480        768  770  775  780         -hsync -vsync
//             pxclk MHz                h_field_len                v_field_len    
        if (!XF86VidModeGetModeLine(display/*.cast*/, i, &scfg.dot_clock, &scfg.mode_line)) continue; //&mode_line)); //continue; //return FALSE;
//        myprintf(28, BLUE_MSG "X-screen[%d/%d]: %d x %d, clock %d" ENDCOLOR, i, num_screens, WidthOfScreen(screen), HeightOfScreen(screen), dot_clock); //->width, ->height, screen->);

//    AppRes.field[HDisplay].val = mode_line.hdisplay;
//    AppRes.field[HSyncStart].val = mode_line.hsyncstart;
//    AppRes.field[HSyncEnd].val = mode_line.hsyncend;
//    AppRes.field[HTotal].val = mode_line.htotal;
//    AppRes.field[VDisplay].val = mode_line.vdisplay;
//    AppRes.field[VSyncStart].val = mode_line.vsyncstart;
//    AppRes.field[VSyncEnd].val = mode_line.vsyncend;
//    AppRes.field[VTotal].val = mode_line.vtotal;
//    sprintf(tmpbuf, "\"%dx%d\"",
//         AppRes.field[HDisplay].val, AppRes.field[VDisplay].val);
//    sprintf(modebuf, "%-11s   %6.2f   %4d %4d %4d %4d   %4d %4d %4d %4d",
//         tmpbuf, (float)dot_clock/1000.0,
//         AppRes.field[HDisplay].val,
//         AppRes.field[HSyncStart].val,
//         AppRes.field[HSyncEnd].val,
//         AppRes.field[HTotal].val,
//         AppRes.field[VDisplay].val,
//         AppRes.field[VSyncStart].val,
//         AppRes.field[VSyncEnd].val,
//         AppRes.field[VTotal].val);

//       vinfo.left_margin, vinfo.right_margin, vinfo.upper_margin, vinfo.lower_margin, vinfo.hsync_len, vinfo.vsync_len,

        int hblank = scfg.mode_line.htotal - scfg.mode_line.hdisplay; //vinfo.left_margin + vinfo.hsync_len + vinfo.right_margin;
        int vblank = scfg.mode_line.vtotal - scfg.mode_line.vdisplay; //vinfo.upper_margin + vinfo.vsync_len + vinfo.lower_margin;
        double rowtime = (double)scfg.mode_line.htotal / scfg.dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
        double frametime = (double)scfg.mode_line.htotal * scfg.mode_line.vtotal / scfg.dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;

        myprintf(28, BLUE_MSG "Screen[%d/%d] timing: %d x %d, pxclk %2.1f MHz, hblank %d+%d+%d = %d (%2.1f%%), vblank = %d+%d+%d = %d (%2.1f%%), row %2.1f usec (%2.1f%% target), frame %2.1f msec (fps %2.1f)" ENDCOLOR, i, num_screens,
            scfg.mode_line.hdisplay, scfg.mode_line.vdisplay, (double)scfg.dot_clock / 1000, //vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
            scfg.mode_line.hsyncstart - scfg.mode_line.hdisplay, scfg.mode_line.hsyncend - scfg.mode_line.hsyncstart, scfg.mode_line.htotal - scfg.mode_line.hsyncend, scfg.mode_line.htotal - scfg.mode_line.hdisplay, (double)100 * (scfg.mode_line.htotal - scfg.mode_line.hdisplay) / scfg.mode_line.htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
            scfg.mode_line.vsyncstart - scfg.mode_line.vdisplay, scfg.mode_line.vsyncend - scfg.mode_line.vsyncstart, scfg.mode_line.vtotal - scfg.mode_line.vsyncend, scfg.mode_line.vtotal - scfg.mode_line.vdisplay, (double)100 * (scfg.mode_line.vtotal - scfg.mode_line.vdisplay) / scfg.mode_line.vtotal, //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
            1000000 * rowtime, rowtime / 300000, 1000 * frametime, 1 / frametime);
//    close(fbfd);
//        ok = true;
        Release(display); //XCloseDisplay(display);
        return &scfg;
    }
    if (display) Release(display); //XCloseDisplay(display);
    return NULL;
  }
 #endif //def RPI_NO_X


//get screen width, height:
//wrapped in a function so it can be used as initializer (optional)
//screen height determines max universe size
//screen width should be configured according to desired data rate (DATA_BITS per node)
WH ScreenInfo()
{
//NOTE: mutex not needed here, but std::atomic complains about deleted function
//main thread will call first, so race conditions won't occur (benign anyway)
//    static std::atomic<int> w = 0, h = {0};
    static WH wh = {0};
    static std::mutex protect;
    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

    if (!wh.w || !wh.h)
    {
        const ScreenConfig* scfg = getScreenConfig();
//        if (!scfg) //return_void(errjs(iso, "Screen: can't get screen info"));
        if (!scfg) /*throw std::runtime_error*/ exc(RED_MSG "Can't get screen size" ENDCOLOR);
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


namespace NodeAddon //namespace wrapper for Node.js functions; TODO: is this needed?
{
//#define JS_STR(...) Nan::New<v8::String>(__VA_ARGS__).ToLocalChecked()
//#define JS_STR(iso, ...) v8::String::NewFromUtf8(iso, __VA_ARGS__)
//TODO: convert to Nan?
#define JS_STR(iso, val) v8::String::NewFromUtf8(iso, val)
//#define JS_INT(iso, val) Nan::New<v8::Integer>(iso, val)
#define JS_INT(iso, val) v8::Integer::New(iso, val)
#define JS_FLOAT(iso, val) v8::Number::New(iso, val)
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


//void Screen_js(v8::Local<v8::String>& name, const Nan::PropertyCallbackInfo<v8::Value>& info)
//void isRPi_js(const Nan::FunctionCallbackInfo<v8::Value>& args)
//void isRPi_js(const v8::FunctionCallbackInfo<v8::Value>& args)
//NAN_METHOD(isRPi_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(isRPi_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
//    Nan::HandleScope scope;
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
//    if (info.Length()) return_void(errjs(iso, "isRPi: expected 0 args, got %d", info.Length()));

//    v8::Local<v8::Boolean> retval = JS_BOOL(iso, isRPi()); //v8::Boolean::New(iso, isRPi());
//    myprintf(3, "isRPi? %d" ENDCOLOR, isRPi());
    info.GetReturnValue().Set(JS_BOOL(iso, isRPi()));
}


//void Screen_js(v8::Local<v8::String>& name, const Nan::PropertyCallbackInfo<v8::Value>& info)
//int Screen_js() {}
//void Screen_js(v8::Local<const v8::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(Screen_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(Screen_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
//    if (info.Length()) return_void(errjs(iso, "Screen: expected 0 args, got %d", info.Length()));

    WH wh = isRPi()? ScreenInfo(): MaxFit(); //kludge: give max size caller can use, not actual screen size
//    struct { int w, h; } wh = {Screen().w, Screen().h};
//    v8::Local<v8::Object> retval = Nan::New<v8::Object>();
    v8::Local<v8::Object> retval = v8::Object::New(iso);
//    v8::Local<v8::String> w_name = Nan::New<v8::String>("width").ToLocalChecked();
//    v8::Local<v8::String> h_name = Nan::New<v8::String>("height").ToLocalChecked();
//    v8::Local<v8::Number> retval = v8::Number::New(iso, ScreenWidth());
    retval->Set(v8::String::NewFromUtf8(iso, "width"), v8::Number::New(iso, wh.w));    
    retval->Set(v8::String::NewFromUtf8(iso, "height"), v8::Number::New(iso, wh.h));
//    retval->Set(JS_STR(iso, "width"), JS_INT(iso, wh.w));
//    retval->Set(JS_STR(iso, "height"), JS_INT(iso, wh.h));
//    myprintf(3, "screen: %d x %d" ENDCOLOR, wh.w, wh.h);
//    Nan::Set(retval, w_name, Nan::New<v8::Number>(wh.w));
//    Nan::Set(retval, h_name, Nan::New<v8::Number>(wh.h));

    const ScreenConfig* scfg = getScreenConfig();
    if (!scfg) return_void(errjs(iso, "Screen: can't get screen info"));
    double rowtime = (double)scfg->mode_line.htotal / scfg->dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
    double frametime = (double)scfg->mode_line.htotal * scfg->mode_line.vtotal / scfg->dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
    retval->Set(JS_STR(iso, "xres"), JS_INT(iso, scfg->mode_line.hdisplay)); //vinfo.xres));
    retval->Set(JS_STR(iso, "yres"), JS_INT(iso, scfg->mode_line.vdisplay)); //vinfo.yres));
//??        retval->Set(JS_STR(iso, "bpp"), JS_INT(iso, vinfo.bits_per_pixel));
//        retval->Set(JS_STR(iso, "linelen"), JS_INT(iso, finfo.line_length));
    retval->Set(JS_STR(iso, "pixclock_MHz"), JS_FLOAT(iso, (double)scfg->dot_clock / 1000)); //MHz //vinfo.pixclock));

    retval->Set(JS_STR(iso, "hblank"), JS_INT(iso, scfg->mode_line.htotal - scfg->mode_line.hdisplay)); //hblank));
    retval->Set(JS_STR(iso, "vblank"), JS_INT(iso, scfg->mode_line.vtotal - scfg->mode_line.vdisplay)); //vblank));
    retval->Set(JS_STR(iso, "rowtime_usec"), JS_FLOAT(iso, 1000000 * rowtime));
    retval->Set(JS_STR(iso, "frametime_msec"), JS_FLOAT(iso, 1000 * frametime));
    retval->Set(JS_STR(iso, "fps"), JS_FLOAT(iso, 1 / frametime));
#if 0
//screen info:
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

//http://www.uruk.org/projects/cvt/
    auto_ptr<FILE> fb = fopen("/dev/fb0", "r"); //O_RDONLY);
    if (!fb) return_void(errjs(iso, "Screen: can't open frame buffer: %s (errno %d)", strerror(errno), errno));
    if (ioctl(fileno(fb), FBIOGET_FSCREENINFO, &finfo)) return_void(errjs(iso, "Screen: can't get fixed screen info: %s (errno %d)", strerror(errno), errno));
    if (ioctl(fileno(fb), FBIOGET_VSCREENINFO, &vinfo)) return_void(errjs(iso, "Screen: can't get variable screen info: %s (errno %d)", strerror(errno), errno));

    if (!vinfo.pixclock) vinfo.pixclock = 50000000; //kludge: pick a reasonable value; TODO: get real value
    int hblank = vinfo.left_margin + vinfo.hsync_len + vinfo.right_margin;
    int vblank = vinfo.upper_margin + vinfo.vsync_len + vinfo.lower_margin;
    double rowtime = (vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
    double frametime = (vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;

    myprintf(28, BLUE_MSG "Screen ioctl: %d x %d, %d bpp, linelen %d, pxclk %d, margins lrtb %d %d %d %d, sync len h %d v %d, row time %2.1f usec, frame time %2.1f msec, fps %2.1f" ENDCOLOR,
       vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length, vinfo.pixclock,
       vinfo.left_margin, vinfo.right_margin, vinfo.upper_margin, vinfo.lower_margin, vinfo.hsync_len, vinfo.vsync_len,
       1000000 * rowtime, 1000 * frametime, 1 / frametime);
//    close(fbfd);
    retval->Set(JS_STR(iso, "xres"), JS_INT(iso, vinfo.xres));
    retval->Set(JS_STR(iso, "yres"), JS_INT(iso, vinfo.yres));
    retval->Set(JS_STR(iso, "bpp"), JS_INT(iso, vinfo.bits_per_pixel));
    retval->Set(JS_STR(iso, "linelen"), JS_INT(iso, finfo.line_length));
    retval->Set(JS_STR(iso, "pixclock"), JS_INT(iso, vinfo.pixclock));

//       vinfo.left_margin, vinfo.right_margin, vinfo.upper_margin, vinfo.lower_margin, vinfo.hsync_len, vinfo.vsync_len,
    retval->Set(JS_STR(iso, "hblank"), JS_INT(iso, hblank));
    retval->Set(JS_STR(iso, "vblank"), JS_INT(iso, vblank));
    retval->Set(JS_STR(iso, "rowtime"), JS_FLOAT(iso, 1000000 * rowtime));
    retval->Set(JS_STR(iso, "frametime"), JS_FLOAT(iso, 1000 * frametime));
    retval->Set(JS_STR(iso, "fps"), JS_FLOAT(iso, 1 / frametime));
#endif

    info.GetReturnValue().Set(retval);
}


//alloc shared memory buffer:
//usage: shm_buffer = shmbuf(key, byte_length)
//this reduces the need for expensive memory copying:
// memcpy.100k > cc Buffer -> Buffer: 22.756ms
// memcpy.100k > cc Buffer -> ArrayBuffer: 23.861ms
//from ws281x-gpu.cpp 2016
//based on https://github.com/vpj/node_shm/blob/master/shm_addon.cpp
//bkg info: https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
//to see shm segs:  ipcs -a
//to delete:  ipcrm -M key
NAN_METHOD(shmbuf_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
//    v8::Isolate* isolate = v8::Isolate::GetCurrent();
//    v8::HandleScope scope(isolate);
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length() < 2) return_void(errjs(iso, "shmbuf: missing key, size args"));
//	if (!args[0]->IsNumber()) Nan::ThrowTypeError("Pixel: 1st arg should be number");
	int key = info[0]->Uint32Value(); //NumberValue();
//	if (!args[1]->IsNumber()) Nan::ThrowTypeError("Pixel: 2nd arg should be number");
	int size = info[1]->Int32Value(); //NumberValue();
	if (/*(size < 1) ||*/ (size >= 10000000)) return_void(errjs(iso, "shmbuf: size %d out of range 1..10M", size));

    int shmid = shmget(key, (size > 0)? size: 1, (size > 0)? IPC_CREAT | 0666: 0666);
    if ((shmid < 0) && (size > 0) && (errno == EINVAL))
        if (!info[2]->IsUndefined() && info[2]->BooleanValue()) //retry
        {
            myprintf(22, YELLOW_MSG "retry shmget, want size %d for key 0x%x" ENDCOLOR, size, key);
            if ((shmid = shmget(key, 1, 0666)) >= 0)
                if (!shmctl(shmid, IPC_RMID, NULL /*ignored*/)) //try deleting shm seg first
                    shmid = shmget(key, size, IPC_CREAT | 0666);
        }
        else myprintf(22, RED_MSG "no retry shmget: #args %d, undef? %d, bool val %d" ENDCOLOR, info.Length(), info[2]->IsUndefined(), !info[2]->IsUndefined()? info[2]->BooleanValue(): -1);
    if (shmid < 0) return_void(errjs(iso, "shmbuf: can't alloc(%d) shmem: %d (errno %d %s)", size, shmid, errno, strerror(errno)))
    else myprintf(22, BLUE_MSG "shmget key 0x%x, size %d okay: 0x%x" ENDCOLOR, key, size, shmid);
//if (!data) //don't attach again
//TODO: need Detach and delete: if (shmdt(data) == -1) fprintf(stderr, "shmdt failed\n");
    uint8_t* data = (size > 0)? (uint8_t*)shmat(shmid, NULL, 0): (uint8_t*)shmctl(shmid, IPC_RMID, NULL);
    if (data < 0) return_void(errjs(iso, "shbuf: att sh mem(%d) failed: %d (errno %d %s)", shmid, data, errno, strerror(int errno)));
    if (size < 1) { info.GetReturnValue().SetUndefined(); return; } //.Set(0); return; }

//Create ArrayBuffer:
//NOTE (from v8 docs): The created array buffer is immediately in externalized state. The memory block will not be reclaimed when a created ArrayBuffer is garbage-collected.
    v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(iso, (void*)data, size);
    info.GetReturnValue().Set(buffer);
//    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.UnivType(inx, !info[1]->IsUndefined()? (GpuCanvas::UniverseTypes)info[1]->IntegerValue(): GpuCanvas::UniverseTypes::INVALID))); //return old type, optionally set new type
}


#if 0 //don't use; no worky with node.js shared memory
//atomic add:
//usage: old_value = AtomicAdd(uint32array, offset, value_to_add)
//allows multiple threads to safely read/write shared data
NAN_METHOD(AtomicAdd_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
//    v8::Isolate* isolate = v8::Isolate::GetCurrent();
//    v8::HandleScope scope(isolate);
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length() != 3) return_void(errjs(iso, "AtomicAdd: expected 3 params, got %d", info.Length()));
	if (!info[0]->IsUint32Array()) return_void(errjs(iso, "AtomicAdd: expected first param of uint32 array"));
    v8::Local<v8::Uint32Array> aryp = info[0].As<v8::Uint32Array>();
    int ofs = info[1]->IntegerValue(), updval = info[2]->IntegerValue();
    if ((ofs < 0) || (ofs >= aryp->Length())) return_void(errjs(iso, "AtomicAdd: array ofs bad: is %d, should be 0..%d", ofs, aryp->Length()));
    uint32_t* data = (uint32_t*)aryp->Buffer()->GetContents().Data() + aryp->ByteOffset();
//    std::atomic<uint32_t> value& = aryp[ofs];
//    SDL_atomic_t value = aryp[ofs];
//typedef struct { int value; } SDL_atomic_t;

//    static bool init = false;
//    uint32_t* ptr = mmap(NULL, sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

//    static std::atomic<uint32_t> value; //= 0;
//    if (!init) { value = 0; init = true; } //kludge: avoid "deleted function" error
//    info.GetReturnValue().Set(JS_INT(iso, value)); //return old value
//    if (!info[0]->IsUndefined()) value += info[0]->IntegerValue(); //update

//    info.GetReturnValue().Set(JS_INT(iso, value)); //return old value
//    aryp[ofs] += updval; //update

//    uint32_t& value = data[ofs];
    info.GetReturnValue().Set(JS_INT(iso, SDL_AtomicAdd((SDL_atomic_t*)&data[ofs], updval))); //return old value
}
#endif //0


//#define WANT_RWLOCK //turned out not to help
#define WANT_SIGNAL //try this one instead

#ifdef WANT_RWLOCK
#include <pthread.h> //rwlock

//rwlock consts:
enum RwlockOps { Init = 1, RdLock = 2, RdLockTry = 3, WrLock = 4, WrLockTry = 5, Unlock = 6, Destroy = 7};
NAN_GETTER(RwlockOps_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap

    v8::Local<v8::Object> retval = v8::Object::New(iso);
    retval->Set(JS_STR(iso, "INIT"), JS_INT(iso, RwlockOps::Init));
    retval->Set(JS_STR(iso, "RDLOCK"), JS_INT(iso, RwlockOps::RdLock));
    retval->Set(JS_STR(iso, "RDLOCK_TRY"), JS_INT(iso, RwlockOps::RdLockTry));
    retval->Set(JS_STR(iso, "WRLOCK"), JS_INT(iso, RwlockOps::WrLock));
    retval->Set(JS_STR(iso, "WRLOCK_TRY"), JS_INT(iso, RwlockOps::WrLockTry));
    retval->Set(JS_STR(iso, "UNLOCK"), JS_INT(iso, RwlockOps::Unlock));
    retval->Set(JS_STR(iso, "DESTROY"), JS_INT(iso, RwlockOps::Destroy));

//    CONST_thing("RWLOCK_SIZE32", divup(__SIZEOF_PTHREAD_RWLOCK_T, sizeof(uint32_t)));
    retval->Set(JS_STR(iso, "SIZE32"), JS_INT(iso, __SIZEOF_PTHREAD_RWLOCK_T));
//    Nan::Export(target, "UnivTypes", UnivTypes);
//    CONST_thing("UnivTypes", UnivTypes);
    info.GetReturnValue().Set(retval);
}


//read/write lock:
//NOTE: this works across processes if uint32array is in shared memeory
//usage: ok = rwlock(uint32array, offset, op); //op == 1 for init, 2 for lock, 3 for unlock, 4 for destroy
//for example code see https://www.ibm.com/support/knowledgecenter/en/ssw_aix_72/com.ibm.aix.genprogc/using_readwrite_locks.htm
NAN_METHOD(rwlock_js)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length() != 3) return_void(errjs(iso, "rwlock: expected 3 params, got %d", info.Length()));
	if (!info[0]->IsUint32Array()) return_void(errjs(iso, "rwlock: expected first param of uint32 array"));
    v8::Local<v8::Uint32Array> aryp = info[0].As<v8::Uint32Array>();
    int ofs = info[1]->IntegerValue(), op = info[2]->IntegerValue();
    if ((ofs < 0) || (ofs >= aryp->Length())) return_void(errjs(iso, "rwlock: array ofs bad: is %d, should be 0..%d", ofs, aryp->Length()));
    uint32_t* data = (uint32_t*)aryp->Buffer()->GetContents().Data() + aryp->ByteOffset(); //CAUTION: might be slice
//    pthread_rwlockattr_t attr;
    /*volatile*/ pthread_rwlock_t& rwlock = *(pthread_rwlock_t*)&data[ofs]; //CAUTION: can be up to 56 bytes long
    const int RETRIES = 10; //TODO: allow caller to set
    const char* opname;
    int retval;

    switch (op)
    {
        case RwlockOps::Init: //init (unlocked state)
//            pthread_rwlockattr_init(&attr);
//            pthread_rwlock_init(&data[ofs], &attr);
//            pthread_rwlockattr_destroy(&attr);
            retval = pthread_rwlock_init(&rwlock, NULL); //equiv to: pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
            opname = "Init";
            break;
        case RwlockOps::RdLock: //read lock (blocking, recursive); succeeds if not locked for write
            retval = pthread_rwlock_rdlock(&rwlock);
            opname = "RdLock";
            break;
        case RwlockOps::RdLockTry: //read lock (non-blocking)
            for (int retry = 0; retry < RETRIES; ++retry)
                if ((retval = pthread_rwlock_tryrdlock(&rwlock)) != EBUSY) break;
            opname = "RdLockTry";
            break;
        case RwlockOps::WrLock: //write lock (blocking); succeeds if no other locks
            retval = pthread_rwlock_wrlock(&rwlock);
            opname = "WrLock";
            break;
        case RwlockOps::WrLockTry: //write lock (non-blocking)
            for (int retry = 0; retry < RETRIES; ++retry)
                if ((retval = pthread_rwlock_trywrlock(&rwlock)) != EBUSY) break;
            opname = "WrLockTry";
            break;
        case RwlockOps::Unlock: //unlock
            retval = pthread_rwlock_unlock(&rwlock);
            opname = "Unlock";
            break;
        case RwlockOps::Destroy: //destroy
            retval = pthread_rwlock_destroy(&rwlock);
            opname = "Destroy";
            break;
        default:
            return_void(errjs(iso, "rwlock: unknown op %d", op));
    }
    myprintf(33, BLUE_MSG "rwlock: %s (op %d), result %d (%s)" ENDCOLOR, opname, op, retval, strerror(retval));
    if (retval) info.GetReturnValue().Set(JS_STR(iso, strerror(retval)));
    else info.GetReturnValue().Set(JS_BOOL(iso, false)); //0 == success, !0 == errno
//    info.GetReturnValue().SetUndefined();
}
#endif


#ifdef WANT_SIGNAL
//#include <unistd.h> //getpid
//#include <sys/types.h>
//send or wait for a signal (cond + mutex):
//signals are an array of process pids in shared memory, used as mailboxes
//only one msg is allowed per pid, indicated by pid != 0

#define U32SIZE(thing)  divup(sizeof(thing), sizeof(uint32_t))

//signal consts:
enum SignalOps { Init = 1, Set = 2, Reset = 3, Destroy = 4};
NAN_GETTER(SignalOps_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap

    v8::Local<v8::Object> retval = v8::Object::New(iso);
    retval->Set(JS_STR(iso, "INIT"), JS_INT(iso, SignalOps::Init));
    retval->Set(JS_STR(iso, "SET"), JS_INT(iso, SignalOps::Set));
    retval->Set(JS_STR(iso, "RESET"), JS_INT(iso, SignalOps::Reset));
    retval->Set(JS_STR(iso, "DESTROY"), JS_INT(iso, SignalOps::Destroy));

//    CONST_thing("RWLOCK_SIZE32", divup(__SIZEOF_PTHREAD_RWLOCK_T, sizeof(uint32_t)));
    retval->Set(JS_STR(iso, "SIZE32"), JS_INT(iso, U32SIZE(SDL_mutex) + U32SIZE(SDL_cond));
//    Nan::Export(target, "UnivTypes", UnivTypes);
//    CONST_thing("UnivTypes", UnivTypes);
    info.GetReturnValue().Set(retval);
}


//set/reset signal:
//NOTE: this works across processes if uint32array is in shared memeory
//usage: signal(uint32array, key, op); //op == 1 for init, 2 for set, 3 for reset, 4 for destroy
NAN_METHOD(signal_js)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length() != 3) return_void(errjs(iso, "signal: expected 3 params, got %d", info.Length()));
    int key = info[1]->IntegerValue(), op = info[2]->IntegerValue();
    if (!key) return_void(errjs(iso, "signal: expected second param of non-0 int"));
	if (!info[0]->IsUint32Array()) return_void(errjs(iso, "signal: expected first param of uint32 array"));
    int numsig = aryp->Length() / sizeof(uint32_t) - U32SIZE(std::mutex) - U32SIZE(std::cond);
    if (numsig < 2) return_void(errjs(iso, "signal: array length bad: is %d, should be at least %d", aryp->Length(), (U32SIZE(std::mutex) + U32SIZE(std::cond) + 2) * sizeof(uint32_t)));
    v8::Local<v8::Uint32Array> aryp = info[0].As<v8::Uint32Array>();
    uint32_t* data = (uint32_t*)aryp->Buffer()->GetContents().Data() + aryp->ByteOffset(); //CAUTION: might be slice
//NOTE: need to use std::* here due to "incomplete type" errors on SDL_* types
//    SDL_mutex& mutex = *(SDL_mutex*)&data[0];
//    SDL_cond& cond = *(SDL_cond*)&data[U32SIZE(SDL_mutex)];
//http://en.cppreference.com/w/cpp/thread/condition_variable
    std::mutex& mutex = *(std::mutex*)&data[0];
    std::condition_variable& cond = *(std::condition_variable*)&data[U32SIZE(std::mutex)];
    uint32_t* pids = &data[U32SIZE(std::mutex) + U32SIZE(std::condition_variable)];
//    pid_t mypid = getpid();
//    const char* opname;
    int retval = 0;

    info.GetReturnValue().SetUndefined();
    switch (op)
    {
        case SignalOps::Init: //init (unlocked state)
//            if (!(mutex = SDL_CreateMutex())) return_void(errjs(iso, RED_MSG "Can't create signal mutex" ENDCOLOR)); //throw SDL_Exception("SDL_CreateMutex");
//            if (!(cond = SDL_CreateCond())) return_void(errjs(iso, RED_MSG "Can't create signal cond" ENDCOLOR)); //throw SDL_Exception("SDL_CreateCond");
            mutex.std::mutex::std::mutex(); //call ctor
            cond.std::condition_variable::std::condition_variable();
            for (int i = 0; i < numsig; ++i) pids[i] = 0; //start all cleared
//            opname = "Init";
            return;
//            break;
        case SignalOps::Set: //wake
            { //scope for locked mutex
//                auto_ptr<SDL_LockedMutex> lock_HERE(mutex); //SDL_LOCK(mutex));
                std::lock_guard<std::mutex> lock(mutex); //to modify the cond var; ctor locks mutex
//                for (;;) //NOTE: need loop in order to handle "spurious wakeups"
                for (int i = 0; i < numsig; ++i)
                    if (pids[i] == key) return_void(errjs(iso, "signal: key '%d' is already set", key)); //protect against backlogged signals; also ensures uniqueness
                for (int i = 0; i < numsig; ++i)
                    if (!pids[i])
                    {
                        pids[i] = key;
                        if (!OK(SDL_CondSignal(cond))) return_void(errjs(RED_MSG "Send signal for key %d failed" ENDCOLOR, key)); //throw SDL_Exception("SDL_CondSignal");
                        myprintf(30-5, BLUE_MSG "signal: set key[%d/%d] to 0x%x, sent signal" ENDCOLOR, i, numsig, key);
                        return;
                    }
                return_void(errjs(iso, "signal: no empty slots (%d already set)", numsig));
            }
//            opname = "Set (wake)";
            break;
        case SignalOps::Reset: //wait
            { //scope for locked mutex
//                auto_ptr<SDL_LockedMutex> lock_HERE(mutex); //SDL_LOCK(mutex));
                std::unique_lock<std::mutex> lock(mutex); //to wait on the cond var; allows manual un/lock
                for (bool first = true;; first = false) //NOTE: need loop in order to handle "spurious wakeups"
                {
                    for (int i = 0; i < numsig; ++i)
                        if (pids[i] == key)
                        {
                            if (first) return_void(errjs(iso, "signal: key '%d' is already set", key)); //protect against premature signals
                            myprintf(30-5, BLUE_MSG "signal: reset key[%d/%d] 0x%x, wake up now" ENDCOLOR, i, numsig, key);
                            pids[i] = 0;
                            return;
                        }
//                    if (!OK(SDL_CondWait(cond, mutex))) return_void(errjs(iso, RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(data), toint(mutex), toint(cond))); //throw SDL_Exception("SDL_CondWait");
                    cond.wait(mutex);
//                    if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
                }
            }
//            opname = "Reset (wait)";
            break;
        case SignalOps::Destroy: //destroy
//            mutex = cond = NULL;
            cond.~std::condition_variable(); //call dtor
            mutex.~std::mutex();
            break;
        default:
            return_void(errjs(iso, "signal: unknown op %d", op));
    }
//    myprintf(33-5, BLUE_MSG "signal: %s (op %d), result %d (%s)" ENDCOLOR, opname, op, retval, strerror(retval));
//    if (retval) info.GetReturnValue().Set(JS_STR(iso, strerror(retval)));
//    else info.GetReturnValue().Set(JS_BOOL(iso, false)); //0 == success, !0 == errno
}
#endif


//short sleep (usec):
//usage: actual_usec = usleep(usec_to_sleep)
NAN_METHOD(usleep_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length() != 1) return_void(errjs(iso, "usleep: expected 1 param, got %d", info.Length()));
    int delay = info[0]->IntegerValue() - 2; //usec; subtract some for overhead

    uint64_t started = now(); //, delay = info[0]->IntegerValue() * SDL_TickFreq() / 1000000; //#ticks
    for (;;)
    {
        uint32_t usec = elapsed(started, 1000000);
        if (usec >= delay) //overdue or close enough
        {
            info.GetReturnValue().Set(JS_INT(iso, usec)); //return actual delay time (usec)
            return;
        }
//        myprintf(24, BLUE_MSG "usleep(%d -> %d)" ENDCOLOR, delay, delay - usec);
        usleep(delay - usec); //NOTE: might wake up early due to signal
    }
}


//void UnivTypes_js(v8::Local<v8::String>& name, const Nan::PropertyCallbackInfo<v8::Value>& info)
//void UnivTypes_js(v8::Local<const v8::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(UnivTypes_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(UnivTypes_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap

    v8::Local<v8::Object> retval = v8::Object::New(iso);
//TODO: make extensible:
//    enum UniverseTypes { NONE = 0, WS281X = 1, PLAIN_SSR = 2, CHPLEX_SSR = 3, SPAREBIT = 0x10, TYPEBITS = 0xF, RGSWAP = 0x20, CHECKSUM = 0x40, POLARITY = 0x80}; 
    retval->Set(JS_STR(iso, "NONE"), JS_INT(iso, GpuCanvas::UniverseTypes::NONE));
    retval->Set(JS_STR(iso, "WS281X"), JS_INT(iso, GpuCanvas::UniverseTypes::WS281X));
    retval->Set(JS_STR(iso, "PLAIN_SSR"), JS_INT(iso, GpuCanvas::UniverseTypes::PLAIN_SSR));
    retval->Set(JS_STR(iso, "CHPLEX_SSR"), JS_INT(iso, GpuCanvas::UniverseTypes::CHPLEX_SSR));
    retval->Set(JS_STR(iso, "TYPEBITS"), JS_INT(iso, GpuCanvas::UniverseTypes::TYPEBITS));
    retval->Set(JS_STR(iso, "RGSWAP"), JS_INT(iso, GpuCanvas::UniverseTypes::RGSWAP));
    retval->Set(JS_STR(iso, "CHECKSUM"), JS_INT(iso, GpuCanvas::UniverseTypes::CHECKSUM));
    retval->Set(JS_STR(iso, "POLARITY"), JS_INT(iso, GpuCanvas::UniverseTypes::POLARITY));
    retval->Set(JS_STR(iso, "ACTIVE_LOW"), JS_INT(iso, GpuCanvas::UniverseTypes::ACTIVE_LOW));
    retval->Set(JS_STR(iso, "ACTIVE_HIGH"), JS_INT(iso, GpuCanvas::UniverseTypes::ACTIVE_HIGH));
//    Nan::Export(target, "UnivTypes", UnivTypes);
//    CONST_thing("UnivTypes", UnivTypes);
    info.GetReturnValue().Set(retval);
}


//   String::Utf8Value fileName(args[0]->ToString());
//const char* dbg(const char* str) { myprintf(22, "str %s" ENDCOLOR, str? str: "(none)"); return str; }
//int dbg(int val) { myprintf(22, "int %s" ENDCOLOR, val); return val; }

//#define INNER_CLASS  GpuCanvas
//#define INNER_CLASS  SimplerCanvas

//?? https://nodejs.org/docs/latest/api/addons.html#addons_wrapping_c_objects
//https://github.com/nodejs/nan/blob/master/doc/methods.md
class GpuCanvas_js: public Nan::ObjectWrap
{
//private:
public:
    auto_ptr<GpuCanvas> inner; //need ptr to allow dtor to be called separately (Node.js GC will not call dtor)
    uint32_t* pixels; //pixel buf
    uint64_t render_delay; //throttle frame rate
    struct
    {
        v8::Persistent<v8::Function> cb; //async paint callback
//        Nan::Callback* callback;
        uv_async_t req;
        int seqnum, err;
        tristate done;
        uint64_t ticks; //timestamp for performance tracking
    } async;
//    uv_work_t uv_bkg_shim;
    static void async_msg(uv_async_t* req);
public:
//    static void Init(v8::Handle<v8::Object> exports);
    static void Init(v8::Local<v8::Object> exports);
    static void Quit(void* ignored); //signature reqd for AtExit
    static std::vector<GpuCanvas_js*> all; //keep track of currently existing instances
//protected:
private:
//    explicit GpuCanvas_js(const char* title, int w, int h, bool pivot): inner(title, w, h, pivot) { all.push_back(this); };
    /*explicit*/ GpuCanvas_js(const char* title, int w, int h/*, bool pivot*/): inner(new GpuCanvas(title, w, h/*, pivot*/))
    {
        all.push_back(this);
        async.req.data = this; //find myself later
        async.seqnum = -1;
        async.err = uv_async_init(uv_default_loop(), &async.req, async_msg); //NOTE: must be called on main thread (with libuv loop)
    };
//    virtual ~GpuCanvas_js();
    ~GpuCanvas_js()
    {
//        uv_close((uv_handle_t*) &async_uv, NULL); //NOTE: must also be called on main (libuv loop) thread
        uv_close((uv_handle_t*)&async.req, [](uv_handle_t* handle) //NOTE: don't dealloc handle until cb called
        {
//            delete handle;
            myprintf(22, YELLOW_MSG "GpuCanvas_js uv_close cb: now safe to dealloc async_uv" ENDCOLOR);
        });
        all.erase(std::find(all.begin(), all.end(), this));
    };

//    static NAN_METHOD(New);
    static void New(const v8::FunctionCallbackInfo<v8::Value>& info); //TODO: convert to Nan?
//    static NAN_GETTER(WidthGetter);
//    static NAN_GETTER(PitchGetter);
    static NAN_METHOD(paint);
//    static NAN_METHOD(stats);
    static NAN_GETTER(stats_getter);
//??    static NAN_PROPERTY_GETTER(get_pivot);
//??    static NAN_PROPERTY_SETTER(set_pivot);
//    static NAN_GETTER(get_pivot);
//    static NAN_SETTER(set_pivot);
    static NAN_GETTER(width_getter);
    static NAN_GETTER(height_getter);
    static NAN_GETTER(elapsed_getter);
    static NAN_SETTER(elapsed_setter);
//    static NAN_METHOD(devmode_tofix);
    static NAN_GETTER(devmode_getter);
    static NAN_SETTER(devmode_setter);
//    static NAN_GETTER(StatsAdjust_getter);
//    static NAN_SETTER(StatsAdjust_setter);
//    static NAN_GETTER(DumpFile_getter);
//    static NAN_SETTER(DumpFile_setter);
    static NAN_METHOD(UnivType_tofix); //TODO: change to accessor/getter/setter; can't figure out how to do that with 2 parameters
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
    v8::Local<v8::ObjectTemplate> proto = ctor->PrototypeTemplate();
//    Nan::SetPrototypeMethod(proto, "paint", paint);
//    NODE_SET_PROTOTYPE_METHOD(ctor, "paint", GpuCanvas_js::paint);
    Nan::SetPrototypeMethod(ctor, "paint", paint);
//    Nan::SetPrototypeMethod(ctor, "stats", stats);
    Nan::SetAccessor(proto, JS_STR(iso, "render_stats"), stats_getter);
//    Nan::SetPrototypeMethod(ctor, "width", width_getter);
//    Nan::SetPrototypeMethod(ctor, "height", height_getter);
    Nan::SetAccessor(proto, JS_STR(iso, "width"), width_getter);
    Nan::SetAccessor(proto, JS_STR(iso, "height"), height_getter);
    Nan::SetAccessor(proto, JS_STR(iso, "elapsed"), elapsed_getter, elapsed_setter);
//TODO    Nan::SetPrototypeMethod(ctor, "utype", GpuCanvas_js::utype);
//    Nan::SetPrototypeMethod(ctor, "devmode_tofix", devmode_tofix);
    Nan::SetAccessor(proto, JS_STR(iso, "devmode"), devmode_getter, devmode_setter);
//    Nan::SetAccessor(proto, JS_STR(iso, "StatsAdjust"), StatsAdjust_getter, StatsAdjust_setter);
//    Nan::SetAccessor(proto, JS_STR(iso, "DumpFile"), DumpFile_getter, DumpFile_setter);
    Nan::SetPrototypeMethod(ctor, "UnivType_tofix", UnivType_tofix); //TODO: fix this
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
//        bool pivot = !info[3]->IsUndefined()? info[3]->BooleanValue(): true;
//myprintf(22, "GpuCanvas_js('%s', %d, %d, %d)" ENDCOLOR, *title? *title: "(none)", w, h, pivot);
        GpuCanvas_js* canvas = new GpuCanvas_js(*title? *title: NULL, w, h); //, pivot);
        if (canvas->async.err < 0) return_void(errjs(iso, "GpuCanvas ctor: uv_async_init failed: %d", canvas->async.err));
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


//#if 1
//get width:
//void GpuCanvas_js::width_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(GpuCanvas_js::width_getter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.cast->width()));
}

//get height:
///void GpuCanvas_js::height_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(GpuCanvas_js::height_getter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.cast->height()));
}

//elapsed time:
NAN_GETTER(GpuCanvas_js::elapsed_getter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

    info.GetReturnValue().Set(JS_FLOAT(iso, canvas->inner.cast->PresentTime()));
}

NAN_SETTER(GpuCanvas_js::elapsed_setter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

    if (!value->IsUndefined()) canvas->inner.cast->ResetElapsed(value->NumberValue());
}

//get/set pivot flag:
//void GpuCanvas_js::devmode_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(GpuCanvas_js::devmode_getter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

    info.GetReturnValue().Set(JS_BOOL(iso, canvas->inner.cast->DEV_MODE)); //return old value
//    if (!info[0]->IsUndefined()) canvas->inner.WantPivot = info[0]->BooleanValue(); //set new value
//if (!info[0]->IsUndefined()) myprintf(1, "set pivot value %d %d %d => %d" ENDCOLOR, info[3]->BooleanValue(), info[3]->IntegerValue(), info[3]->Uint32Value(), canvas->inner.WantPivot);
//else myprintf(1, "get pivot value %d" ENDCOLOR, canvas->inner.WantPivot);
}
NAN_SETTER(GpuCanvas_js::devmode_setter) //defines "info" and "value"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

//    info.GetReturnValue().Set(JS_BOOL(iso, canvas->inner.WantPivot)); //return old value
    if (!value->IsUndefined()) canvas->inner.cast->DEV_MODE = value->BooleanValue();
}


#if 0
//get/set stats adjust:
//void GpuCanvas_js::devmode_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(GpuCanvas_js::StatsAdjust_getter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.cast->StatsAdjust)); //return old value
//    if (!info[0]->IsUndefined()) canvas->inner.WantPivot = info[0]->BooleanValue(); //set new value
//if (!info[0]->IsUndefined()) myprintf(1, "set pivot value %d %d %d => %d" ENDCOLOR, info[3]->BooleanValue(), info[3]->IntegerValue(), info[3]->Uint32Value(), canvas->inner.WantPivot);
//else myprintf(1, "get pivot value %d" ENDCOLOR, canvas->inner.WantPivot);
}
NAN_SETTER(GpuCanvas_js::StatsAdjust_setter) //defines "info" and "value"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

//    info.GetReturnValue().Set(JS_BOOL(iso, canvas->inner.WantPivot)); //return old value
    if (!value->IsUndefined()) canvas->inner.cast->StatsAdjust = value->IntegerValue();
}
#endif


#if 0
//get/set dump (debug) file:
//void GpuCanvas_js::devmode_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(GpuCanvas_js::DumpFile_getter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

    info.GetReturnValue().Set(JS_STR(iso, canvas->inner.DumpFile.c_str())); //return old value
//    if (!info[0]->IsUndefined()) canvas->inner.WantPivot = info[0]->BooleanValue(); //set new value
//if (!info[0]->IsUndefined()) myprintf(1, "set pivot value %d %d %d => %d" ENDCOLOR, info[3]->BooleanValue(), info[3]->IntegerValue(), info[3]->Uint32Value(), canvas->inner.WantPivot);
//else myprintf(1, "get pivot value %d" ENDCOLOR, canvas->inner.WantPivot);
}
NAN_SETTER(GpuCanvas_js::DumpFile_setter) //defines "info" and "value"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());

//    info.GetReturnValue().Set(JS_BOOL(iso, canvas->inner.WantPivot)); //return old value
    v8::String::Utf8Value filename(!value->IsUndefined()? value->ToString(): JS_STR(iso, ""));
    canvas->inner.DumpFile = *filename;
}
#endif


//get/set univ types:
//void GpuCanvas_js::UnivType_tofix(const Nan::FunctionCallbackInfo<v8::Value>& info)
NAN_METHOD(GpuCanvas_js::UnivType_tofix) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.Holder()); //info.This());
    int inx = info[0]->IntegerValue();
    if ((inx < 0) || (inx >= canvas->inner.cast->width())) return_void(errjs(iso, "GpuCanvas.UnivType: invalid inx %d (expected 0..%d)", inx, canvas->inner.cast->width()));

//myprintf(1, "set univ type[%d] to %d? %d" ENDCOLOR, inx, !info[1]->IsUndefined()? (GpuCanvas::UniverseTypes)info[1]->IntegerValue(): GpuCanvas::UniverseTypes::INVALID, !info[1]->IsUndefined());
    info.GetReturnValue().Set(JS_INT(iso, canvas->inner.cast->UnivType(inx, !info[1]->IsUndefined()? (GpuCanvas::UniverseTypes)info[1]->IntegerValue(): GpuCanvas::UniverseTypes::INVALID))); //return old type, optionally set new type
//    if (!info[1]->IsUndefined()) canvas->inner.UnivType[inx] = (GpuCanvas::UniverseTypes)info[1]->IntegerValue();
//if (!info[0]->IsUndefined()) myprintf(1, "set pivot value %d %d %d => %d" ENDCOLOR, info[3]->BooleanValue(), info[3]->IntegerValue(), info[3]->Uint32Value(), canvas->inner.WantPivot);
//else myprintf(1, "get pivot value %d" ENDCOLOR, canvas->inner.WantPivot);
}
//#endif


//async callback wrapper:
//used from any thread to trigger async callbacks in main node.js thread
//see http://nikhilm.github.io/uvbook/threads.html
//and https://stackoverflow.com/questions/36987273/callback-nodejs-javascript-function-from-multithreaded-c-addon
//and https://stackoverflow.com/questions/15685793/callback-from-different-thread-in-nodejs-native-extension
void js_cbwrapper(void* ptr, tristate done) //GpuCanvas_js* canvas, void* cbdata)
{
//    uv_queue_work(uv_default_loop(), ptr, WorkAsync, WorkAsyncComplete);
//    GpuCanvas::Tristate done = reinterpret_cast<GpuCanvas::Tristate&>(cbdata); //https://stackoverflow.com/questions/19387647/c-invalid-cast-from-type-void-to-type-double
    GpuCanvas_js* canvas = reinterpret_cast<GpuCanvas_js*>(ptr);
    ++canvas->async.seqnum; //check for coallesced events
    canvas->async.done = done;
    canvas->async.ticks = now(); //uv_hrtime(); //only for debug
    myprintf(22, BLUE_MSG "cb wrapper: canv ptr 0x%lx, send msg[%d], done? %d @%2.1f msec, timebase %ld" ENDCOLOR, (long)canvas, canvas->async.seqnum, done, canvas->inner.cast->PresentTime() * 1000, now() - uv_hrtime());
//    v8::Isolate* iso = v8::Isolate::GetCurrent();
    myprintf(22, YELLOW_MSG "TODO: post ipc msg to child procs here to bypass 2nd async delay" ENDCOLOR);
    int ok = canvas->async.err = uv_async_send(&canvas->async.req); //NOTE: safe to call this from any thread
//    if (ok < 0) return_void(jserr(iso, "cbwrapper: uv_async_send failed: %d", ok));
    myprintf(22, "%scb wrapper: msg[%d] sent ok? %d, retcode %d" ENDCOLOR, (ok < 0)? RED_MSG: BLUE_MSG, canvas->async.seqnum, (ok >= 0), ok);
}


//async responder:
//called by libuv in main thread after wker thread calls uv_async_send()
//since it runs in main thread, safe to call back to the JS function
void GpuCanvas_js::async_msg(uv_async_t* req)
{
    GpuCanvas_js* canvas = reinterpret_cast<GpuCanvas_js*>(req->data);
    tristate done = canvas->async.done;
    myprintf(22, BLUE_MSG "async responder: canv ptr 0x%lx, seqnum %d, err %d, done? %d @%2.1f msec, latency %ld" ENDCOLOR, (long)canvas, canvas->async.seqnum, canvas->async.err, canvas->async.done, canvas->inner.cast->PresentTime() * 1000, now() - canvas->async.ticks);
    v8::Isolate* iso = v8::Isolate::GetCurrent(); //TODO: store in GpuCanvas_js?
    v8::HandleScope scope(iso); //for Node 4.x; TODO: is this needed?
//    v8::Handle<v8::Value> argv[] = (done != tristate::Error)? { JS_BOOL(iso, done == tristate::Yes) }: { JS_STR(iso, "error") };
//    Local<Value> argv[] = { v8::String::NewFromUtf8(iso, "Hello world") };
    std::vector<v8::Handle<v8::Value>> argv;
    if (done != tristate::Error) argv.push_back(JS_BOOL(iso, done == tristate::Yes));
    else argv.push_back(JS_STR(iso, "error"));
//myprintf(22, BLUE_MSG "cb wrapper here2" ENDCOLOR);
//    cbPeriodic->Call(1, argv);
    v8::Local<v8::Function>::New(iso, canvas->async.cb)->Call(iso->GetCurrentContext()->Global(), 1, argv.data());
    if (done != tristate::No) canvas->async.cb.Reset(); //free up persistent function callback
}


enum class ParseStates: int { NONE = 0, HAS_PX = 1, HAS_DELAY = 2, HAS_CB = 4};
//    ParseStates& operator|(ParseStates& lhs, ParseStates& rhs) { return (int)lhs | (int)rhs; }
//    ParseStates& operator&(ParseStates& lhs, ParseStates& rhs) { return (int)lhs & (int)rhs; }
#define TOTYPE(thing)  static_cast<std::underlying_type<thing>::type>
/*const ParseStates&*/ ParseStates operator |=(ParseStates& lhs, const ParseStates& rhs)
{
    return lhs = static_cast<ParseStates>(TOTYPE(ParseStates)(lhs) | TOTYPE(ParseStates)(rhs));
}
// /*const ParseStates&*/ ParseStates operator &(ParseStates lhs, ParseStates rhs)  
/*const ParseStates&*/ /*ParseStates*/ bool operator &(const ParseStates& lhs, const ParseStates& rhs)
{
    return (static_cast<ParseStates>(TOTYPE(ParseStates)(lhs) & TOTYPE(ParseStates)(rhs)) != ParseStates::NONE);
}
//bool operator!(ParseStates val) { return (val == ParseStates::NONE); }
//operator bool(const ParseStates& val) { return (val != ParseStates::NONE); }
//operator int(ParseStates& val) { return (int)val; }


//xfr/xfm Javascript array to GPU:
//void GpuCanvas_js::paint(const Nan::FunctionCallbackInfo<v8::Value>& info)
NAN_METHOD(GpuCanvas_js::paint) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
//    if (info.Length() != 1) return_void(errjs(iso, "GpuCanvas.paint: expected 1 param, got %d", info.Length()));
//	if (info.Length() && !info[0]->IsUint32Array()) return_void(errjs(iso, "GpuCanvas.paint: missing uint32 array param"));
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
//http://brendanashworth.github.io/v8-docs/classv8_1_1_typed_array.html

//check for optional args:
//    uint32_t* pixels; //pixel buf
//    v8::Persistent<v8::Function> cb; //async paint callback
//https://nodeaddons.com/c-processing-from-node-js-part-4-asynchronous-addons/
//https://gist.github.com/dmh2000/9519489
    canvas->pixels = 0;
    canvas->render_delay = 0;
    canvas->async.cb.Reset();
//    int px_arg = 0, cb_arg = 0; //index of arg found ; 1-based for terse checking
    ParseStates state = ParseStates::NONE;
    const char* REASONS[] =
    {
        "uint32 array, int delay, or callback function expected", //NONE
        "int delay, or callback function expected", //HAS_PX
        "uint32 array or callback function expected", //HAS_DELAY
        "callback function expected", //PX + DELAY
        "uint32 array or int delay expected", //HAS_CB
        "int delay expected", //PX + CB
        "uint32 array expected", //PX + DELAY
        "unrecognized arg" //all
    };
//    bool has_px = false, has_delay = false, has_cb = false;
    for (int i = 0; i < info.Length(); ++i)
    	if (info[i]->IsUint32Array() && !(state & ParseStates::HAS_PX)) //has_px) //update pixels
        {
            v8::Local<v8::Uint32Array> aryp = info[i].As<v8::Uint32Array>();
            if (aryp->ByteLength() < 4 * canvas->inner.cast->width() * canvas->inner.cast->height()) return_void(errjs(iso, "GpuCanvas.paint: array param bad length: is %d, should be %d", aryp->ByteLength(), 4 * canvas->inner.cast->width() * canvas->inner.cast->height()));
            void* data = aryp->Buffer()->GetContents().Data() + aryp->ByteOffset(); //CAUTION: buf might be a slice; need to add ofs here
            canvas->pixels = static_cast<uint32_t*>(data); //CAUTION: assumes memory is outside Node.js heap (ie, shared memory) and won't move
myprintf(33-5, BLUE_MSG "js paint arg[%d/%d] state %d: pixels %d:0x%x 0x%x 0x%x ..." ENDCOLOR, i, info.Length(), state, aryp->ByteLength() / 4, canvas->pixels[0], canvas->pixels[1], canvas->pixels[2]);
//            has_px = true;
            state |= ParseStates::HAS_PX;
        }
#ifdef SINGLE_THREADED_BKG
        else if (info[i]->IsNumber() && !(state & ParseStates::HAS_DELAY)) //has_delay) //throttle frame rate
        {
//    double PresentTime() { return started? elapsed(started): -1; } //presentation timestamp (according to bkg rendering thread)
//    void ResetElapsed(double elaps = 0) { started = now() - elaps * SDL_TickFreq(); }
//    if (!value->IsUndefined()) canvas->inner.cast->ResetElapsed(value->NumberValue());
//        	canvas->render_delay = info[i]->NumberValue() * SDL_TickFreq() + started; //- now(); //elapsed time (sec) => tick count
        	canvas->render_delay = now() + (info[i]->NumberValue() - canvas->inner.cast->PresentTime()) * SDL_TickFreq(); //- now(); //elapsed time (sec) => tick count
//            uint64_t started = now() - elapsed()
//elapsed = (now - started) / freq
//elapsed * freq + started = now
//PresentTime = elapsed(started)
//now = ticks()
//            has_delay = true;
myprintf(33-5, BLUE_MSG "js paint arg[%d/%d] state %d: wake up at elapsed %2.1f msec = %" PRIu64 " ticks = %2.1f msec" ENDCOLOR, i, info.Length(), state, (double)1000 * info[i]->NumberValue(), canvas->render_delay, elapsed(canvas->render_delay) * 1000);
            state |= ParseStates::HAS_DELAY;
        }
        else if (info[i]->IsFunction() && !(state & ParseStates::HAS_CB)) //has_cb) //async callback
        {
//??            v8::Callback* cb = new v8::Callback(info[0].As<v8::Function>());
            v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(info[i]);
            v8::String::Utf8Value funcname(callback->GetName()); //.ToString()); //scallback->GetName()->ToString());
            const char* fnname = *funcname? *funcname: 0;
//callback->GetName()
//callback->GetInferredName()
//callback->GetDisplayName()
            v8::ScriptOrigin script = callback->GetScriptOrigin();
            v8::String::Utf8Value srcname(script.ResourceName()); //.ToString()); //scallback->GetName()->ToString());
            const char* shortname = *srcname? strrchr(*srcname, '/'): 0;
            uint32_t line = script.ResourceLineOffset()->Uint32Value(); //IntegerValue(); //callback->GetScriptLineNumber();
            uint32_t col = script.ResourceColumnOffset()->Uint32Value(); //IntegerValue(); //callback->GetScriptColumnNumber();
            canvas->async.cb.Reset(iso, callback);
myprintf(33-5, BLUE_MSG "js paint arg[%d/%d] state %d: callback %s from %s:%d:%d" ENDCOLOR, i, info.Length(), state, (fnname && *fnname)? fnname: "??", (shortname && *shortname)? shortname + 1: "??", line, col);
//            has_cb = true;
            state |= ParseStates::HAS_CB;
//myprintf(22, BLUE_MSG "%d" ENDCOLOR, state);
        }
#endif
//        else return_void(errjs(iso, "GpuCanvas.paint: invalid arg[%d]: %s", i, (!has_px && !has_cb)? "uint32 array or callback function expected": !has_px? "uint32 array expected": !has_cb? "callback function expected": "unrecognized arg"));
        else return_void(errjs(iso, "GpuCanvas.paint: invalid arg[%d]: %s", i, REASONS[(int)state]));

#ifdef SINGLE_THREADED_BKG
//myprintf(22, BLUE_MSG "%d" ENDCOLOR, state & ParseStates::HAS_CB);
    if (state & ParseStates::HAS_CB) //has_cb) //async return
    {
//        myprintf(33-5, BLUE_MSG "js paint async callback: canvas 0x%lx, pixels 0x%lx, cb 0x%lx, cbdata 0x%lx" ENDCOLOR, (long)canvas, (long)canvas->pixels, (long)&js_cbwrapper, (long)canvas);
        if (!canvas->inner.cast->Paint_bkg(canvas->pixels, canvas->render_delay, js_cbwrapper, canvas)) return_void(errjs(iso, "GpuCanvas.paint_bkg: failed"));
//        canvas->uv_bkg_shim.data = canvas;
//        uv_queue_work(uv_default_loop(), &canvas->uv_bkg_shim, WorkAsync_shim, WorkAsyncComplete_shim);
        info.GetReturnValue().SetUndefined(); //iso);
        return;
    }
#endif
    if (!canvas->inner.cast->Paint_bkg(canvas->pixels, canvas->render_delay)) return_void(errjs(iso, "GpuCanvas.paint: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}


//show performance stats:
//void GpuCanvas_js::stats(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(GpuCanvas_js::setget_pivot) //defines "info"; implicit HandleScope (~ v8 stack frame)
NAN_GETTER(GpuCanvas_js::stats_getter) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    GpuCanvas_js* canvas = Nan::ObjectWrap::Unwrap<GpuCanvas_js>(info.This()); //Holder()); //info.This());
//    if (!canvas->inner.cast->stats()) return_void(errjs(iso, "GpuCanvas.stats: failed"));
    canvas->inner.cast->stats();

    v8::Local<v8::Object> retval = v8::Object::New(iso);
    retval->Set(JS_STR(iso, "elapsed"), JS_FLOAT(iso, canvas->inner.cast->stats_report.elapsed));
    retval->Set(JS_STR(iso, "fps"), JS_FLOAT(iso, canvas->inner.cast->stats_report.fps));
    retval->Set(JS_STR(iso, "numfr"), JS_INT(iso, canvas->inner.cast->stats_report.numfr));
    retval->Set(JS_STR(iso, "numerr"), JS_INT(iso, canvas->inner.cast->stats_report.numerr));
    retval->Set(JS_STR(iso, "num_dirty"), JS_INT(iso, canvas->inner.cast->stats_report.num_dirty));
//    retval->Set(JS_STR(iso, "frrate"), JS_INT(iso, canvas->inner.cast->stats_report.frrate));
//    retval->Set(JS_STR(iso, "avg_fr"), JS_FLOAT(iso, canvas->inner.cast->stats_report.avg_fr));
    v8::Local<v8::Array> frrate = v8::Array::New(iso);
    for (int i = 0; (i < SIZE(canvas->inner.cast->stats_report.frrate)) && (i < canvas->inner.cast->stats_report.numfr); ++i)
        frrate->Set(i, JS_FLOAT(iso, canvas->inner.cast->stats_report.frrate[i]));
    retval->Set(JS_STR(iso, "frrate"), frrate);
    retval->Set(JS_STR(iso, "avg_fps"), JS_FLOAT(iso, canvas->inner.cast->stats_report.avg_fps));
    retval->Set(JS_STR(iso, "caller_time"), JS_FLOAT(iso, canvas->inner.cast->stats_report.caller_time));
    retval->Set(JS_STR(iso, "encode_time"), JS_FLOAT(iso, canvas->inner.cast->stats_report.encode_time));
    retval->Set(JS_STR(iso, "update_time"), JS_FLOAT(iso, canvas->inner.cast->stats_report.update_time));
    retval->Set(JS_STR(iso, "render_time"), JS_FLOAT(iso, canvas->inner.cast->stats_report.render_time));
    info.GetReturnValue().Set(retval);
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


//see https://v8docs.nodesource.com/node-0.8/d2/d78/classv8_1_1_persistent.html
//see also https://github.com/nodejs/node/blob/master/src/node_object_wrap.h
void GpuCanvas_js::Quit(void* ignored)
{
//    GpuCanvas::Quit();
    myprintf(22, BLUE_MSG "js cleanup: %d instance(s) to destroy" ENDCOLOR, all.size());
//    while (all.size())
    for (int i = 0; i < all.size(); ++i)
    {
//        delete ptr; //dtor will remove self from list
//getting "near death" node errors; from node.js doc at https://v8docs.nodesource.com/node-0.8/d2/d78/classv8_1_1_persistent.html
//IsNearDeath Checks if the handle holds the only reference to an object. 
//so it looks like the wrapped object pointer must be empty to avoid this error
//New() above did a Wrap() which did a MakeWeak(), which does a MarkIndependent()
        all[i]->inner = NULL; //call GpuCanvas dtor to free up SDL resources
    }
//    myprintf(22, BLUE_MSG "js cleanup: %d instance(s) remaining" ENDCOLOR, all.size());
}


//#define CONST_INT(name, value)  
#define CONST_thing(name, value)  \
  Nan::ForceSet(target, Nan::New(name).ToLocalChecked(), Nan::New(value), static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete))


//tell Node.js about my entry points:
NAN_MODULE_INIT(exports_js) //defines target
//void exports_js(v8::Local<v8::Object> exports, v8::Local<v8::Object> module)
{
//??    SDL_SetMainReady();
    v8::Isolate* iso = target->GetIsolate(); //~vm heap
//    NODE_SET_METHOD(exports, "isRPi", isRPi_js);
//    NODE_SET_METHOD(exports, "Screen", Screen_js); //TODO: property instead of method
//    Nan::Export(target, "isRPi_tofix", isRPi_js);
//    Nan::Export(target, "Screen_tofix", Screen_js);
//    Nan::Export(target, "UnivTypes_tofix", UnivTypes_js);
    Nan::SetAccessor(target, JS_STR(iso, "isRPi"), isRPi_js); //, DirtySetter);
    Nan::SetAccessor(target, JS_STR(iso, "Screen"), Screen_js);
//    NAN_METHOD(shmbuf_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
//	exports->Set(Nan::New("shmatt").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(shmatt_entpt)->GetFunction());
//    Nan::Export(target, "AtomicAdd", AtomicAdd_js);
    Nan::Export(target, "shmbuf", shmbuf_js);
#ifdef WANT_RWLOCK
    Nan::Export(target, "rwlock", rwlock_js);
    Nan::SetAccessor(target, JS_STR(iso, "RwlockOps"), RwlockOps_js);
#endif
#ifdef WANT_SIGNAL
    Nan::Export(target, "signal", signal_js);
    Nan::SetAccessor(target, JS_STR(iso, "SignalOps"), SignalOps_js);
#endif
    Nan::Export(target, "usleep", usleep_js);
    Nan::SetAccessor(target, JS_STR(iso, "UnivTypes"), UnivTypes_js);
//    target->SetAccessor(JS_STR(iso, "Screen"), Screen_js);
    GpuCanvas_js::Init(target);
//    SimplerCanvas_js::Init(target);
//  Nan::SetAccessor(proto, Nan::New("fillColor").ToLocalChecked(), GetFillColor);
//    NAN_GETTER(Screen_js);
//    Nan::SetAccessor(exports, Nan::New("Screen").ToLocalChecked(), Screen_js);
//    NAN_PROPERTY_GETTER(getter_js);
//    NODE_SET_METHOD(exports, "GpuCanvas", GpuCanvas_js);
//https://github.com/Automattic/node-canvas/blob/b470ce81aabe2a78d7cdd53143de2bee46b966a7/src/CanvasRenderingContext2d.cc#L764
//    NODE_SET_METHOD(module, "exports", CreateObject);

//    CONST_INT("WS281X", GpuCanvas::UniverseTypes::WS281X);
//    CONST_INT("BARE_SSR", GpuCanvas::UniverseTypes::BARE_SSR);
//    CONST_INT("CHPLEX_SSR", GpuCanvas::UniverseTypes::CHPLEX_SSR); //TODO: make extensible
//    v8::Local<v8::Object> UnivTypes = v8::Object::New(iso);
//TODO: make extensible:
//    UnivTypes->Set(JS_STR(iso, "WS281X"), JS_INT(iso, GpuCanvas::UniverseTypes::WS281X));
//    UnivTypes->Set(JS_STR(iso, "BARE_SSR"), JS_INT(iso, GpuCanvas::UniverseTypes::BARE_SSR));
//    UnivTypes->Set(JS_STR(iso, "CHPLEX_SSR"), JS_INT(iso, GpuCanvas::UniverseTypes::CHPLEX_SSR));
//    Nan::Export(target, "UnivTypes", UnivTypes);
//    CONST_thing("UnivTypes", UnivTypes);

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
#define UNIV_LEN  ScreenInfo().h
#endif


int main(int argc, const char* argv[])
{
    myprintf(1, CYAN_MSG "test/standalone routine" ENDCOLOR);
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
    myprintf(1, GREEN_MSG "loop for %d sec (%d frames) ..." ENDCOLOR, duration, duration * 60);
    for (int xy = 0; xy < duration * 60; ++xy) //xy < 10 * 10; ++xy)
    {
        int x = (xy / UNIV_LEN) % NUM_UNIV, y = xy % UNIV_LEN; //cycle thru [0..9,0..9]
//        myprintf(33, "evth" ENDCOLOR);
        if (eventh(1)) break; //user quit
        uint32_t color = PALETTE[(x + y + xy / pixels.size()) % SIZE(PALETTE)]; //vary each cycle
//        myprintf(1, BLUE_MSG "px[%d, %d] = 0x%x" ENDCOLOR, x, y, color);
        pixels[xy % pixels.size()] = color;
        double PresentTime_cpy = canvas.PresentTime; //kludge: avoid "deleted function" error on atomic
        myprintf(33, BLUE_MSG "paint[%d, %d] @%2.1f msec" ENDCOLOR, x, y, PresentTime_cpy); //canvas.PresentTime);
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
    myprintf(1, GREEN_MSG "done, wait 5 sec" ENDCOLOR);
    SDL_Delay(5000);
    return 0;
}


//handle windowing system events:
//NOTE: SDL_Event handling loop must be in main() thread
//(mainly for debug)
bool eventh(int max /*= INT_MAX*/)
{
//    myprintf(14, BLUE_MSG "evth max %d" ENDCOLOR, max);
    while (max)
    {
    	SDL_Event evt;
//        SDL_PumpEvents(void); //not needed if calling SDL_PollEvent or SDL_WaitEvent
//       if (SDL_WaitEvent(&evt)) //execution suspends here while waiting on an event
		if (SDL_PollEvent(&evt))
		{
//            myprintf(14, BLUE_MSG "evt type 0x%x" ENDCOLOR, evt.type);
			if (evt.type == SDL_QUIT) return true; //quit = true; //return;
			if (evt.type == SDL_KEYDOWN)
            {
				myprintf(14, CYAN_MSG "got key down 0x%x" ENDCOLOR, evt.key.keysym.sym);
				if (evt.key.keysym.sym == SDLK_ESCAPE) return true; //quit = true; //return; //key codes defined in /usr/include/SDL2/SDL_keycode.h
			}
			if (evt.type == SDL_PRESSED)
            {
				myprintf(14, CYAN_MSG "got key press 0x%x" ENDCOLOR, evt.key.keysym.sym);
				if (evt.key.keysym.sym == SDLK_ESCAPE) return true; //quit = true; //return; //key codes defined in /usr/include/SDL2/SDL_keycode.h
			}
#if 0 //no worky (evt queue not polled by libuv)
            if (SDL.cast->evtid && (evt.type == SDL.cast->evtid)) //custom event: printf from bkg thread
            {
                const char* buf = reinterpret_cast<const char*>(evt.user.data1);
                fputs(buf, stderr);
                delete[] buf;
            }
#endif
/*
            if (kbhit())
            {
                char ch = getch();
                myprintf(14, "char 0x%x pressed\n", ch);
            }
*/
		}
//        const Uint8* keyst = SDL_GetKeyboardState(NULL);
//        if (keyst[SDLK_RETURN]) { myprintf(8, CYAN_MSG "Return key pressed." ENDCOLOR); return true; }
//        if (keyst[SDLK_ESCAPE]) { myprintf(8, CYAN_MSG "Escape key pressed." ENDCOLOR); return true; }
        if (max != INT_MAX) --max;
//        myprintf(14, BLUE_MSG "no evts" ENDCOLOR);
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
    myprintf(1, CYAN_MSG "Debug detail level = %d (from %d)" ENDCOLOR, WANT_LEVEL, where);

    SDL_version ver;
    SDL_GetVersion(&ver);
    myprintf(12, BLUE_MSG "SDL version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, isRPi? %d" ENDCOLOR, ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());

    myprintf(12, BLUE_MSG "%d video driver(s):" ENDCOLOR, SDL_GetNumVideoDrivers());
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
        myprintf(12, BLUE_MSG "Video driver[%d/%d]: name '%s'" ENDCOLOR, i, SDL_GetNumVideoDrivers(), SDL_GetVideoDriver(i));
}


//SDL window info:
void debug_info(SDL_Window* window, int where)
{
#if 0
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    myprintf(20, BLUE_MSG "GL_VERSION: %s" ENDCOLOR, glGetString(GL_VERSION));
    myprintf(20, BLUE_MSG "GL_RENDERER: %s" ENDCOLOR, glGetString(GL_RENDERER));
    myprintf(20, BLUE_MSG "GL_SHADING_LANGUAGE_VERSION: %s" ENDCOLOR, glGetString(GL_SHADING_LANGUAGE_VERSION));
    myprintf(20, "GL_EXTENSIONS: %s" ENDCOLOR, glGetString(GL_EXTENSIONS));
    SDL_GL_DeleteContext(gl_context);
#endif

    int wndw, wndh;
    SDL_GL_GetDrawableSize(window, &wndw, &wndh);
//        return err(RED_MSG "Can't get drawable window size" ENDCOLOR);
    uint32_t fmt = SDL_GetWindowPixelFormat(window);
//    if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_MSG "Can't get window format" ENDCOLOR));
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
    myprintf(12, BLUE_MSG "window %d x %d, fmt %i bpp %s, flags %s (from %d)" ENDCOLOR, wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1, where);

#if 0
    myprintf(22, BLUE_MSG "SDL_WINDOW_FULLSCREEN    [%c]" ENDCOLOR, (flags & SDL_WINDOW_FULLSCREEN) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_OPENGL        [%c]" ENDCOLOR, (flags & SDL_WINDOW_OPENGL) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_SHOWN         [%c]" ENDCOLOR, (flags & SDL_WINDOW_SHOWN) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_HIDDEN        [%c]" ENDCOLOR, (flags & SDL_WINDOW_HIDDEN) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_BORDERLESS    [%c]" ENDCOLOR, (flags & SDL_WINDOW_BORDERLESS) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_RESIZABLE     [%c]" ENDCOLOR, (flags & SDL_WINDOW_RESIZABLE) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_MINIMIZED     [%c]" ENDCOLOR, (flags & SDL_WINDOW_MINIMIZED) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_MAXIMIZED     [%c]" ENDCOLOR, (flags & SDL_WINDOW_MAXIMIZED) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_INPUT_GRABBED [%c]" ENDCOLOR, (flags & SDL_WINDOW_INPUT_GRABBED) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_INPUT_FOCUS   [%c]" ENDCOLOR, (flags & SDL_WINDOW_INPUT_FOCUS) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_MOUSE_FOCUS   [%c]" ENDCOLOR, (flags & SDL_WINDOW_MOUSE_FOCUS) ? 'X' : ' ');
    myprintf(22, BLUE_MSG "SDL_WINDOW_FOREIGN       [%c]" ENDCOLOR, (flags & SDL_WINDOW_FOREIGN) ? 'X' : ' '); 
#endif

//NO
//    SDL_Surface* wnd_surf = SDL_GetWindowSurface(window); //NOTE: wnd will dealloc, so don't need auto_ptr here
//    if (!wnd_surf) return_void(err(RED_MSG "Can't get window surface" ENDCOLOR));
//NOTE: wnd_surf info is gone after SDL_CreateRenderer! (benign if info was saved already)
//    debug_info(wnd_surf);
}


//SDL_Renderer info:
void debug_info(SDL_Renderer* renderer, int where)
{
return; //TMI
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
//    if (want_fmts && (numfmt != want_fmts)) err(RED_MSG "Unexpected #formats: %d (wanted %d)" ENDCOLOR, numfmt, want_fmts);
    if (!surf->pixels || (toint(surf->pixels) & 7)) err(RED_MSG "Surface pixels not aligned on 8-byte boundary: 0x%x" ENDCOLOR, toint(surf->pixels));
    if ((size_t)surf->pitch != sizeof(uint32_t) * surf->w) err(RED_MSG "Unexpected pitch: %d should be %zu * %d = %zu" ENDCOLOR, surf->pitch, sizeof(uint32_t), surf->w, sizeof(uint32_t) * surf->w);
    myprintf(18, BLUE_MSG "Surface 0x%x: %d x %d, pitch %s, size %s, %s%s (from %d)" ENDCOLOR, toint(surf), surf->w, surf->h, commas(surf->pitch), commas(surf->h * surf->pitch), count.str().c_str(), fmts.str().c_str() + 1, where);
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
    WH wh = ScreenInfo();
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
//    myprintf(33, BLUE_MSG "hsv(%f, %f, %f) => argb 0x%x" ENDCOLOR, h, s, v, color);
    return color;
}


//TODO?
//uint32_t ARGB2ABGR(uint32_t color)
//{
//    return color;
//}


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
    char fmtbuf[600];
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
#if 0
//#ifdef BUILDING_NODE_EXTENSION //set by node-gyp
// #pragma message("TODO: node console.log")
//#else //stand-alone (XWindows)
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
//        fprintf(stdlog(), "nowhere to send" ENDCOLOR);
//#endif
//        dest = stdexc;
        return NULL; //nowhere to send; just discard
#else
        Thread::enque(fwdbuf); //send to fg thread to write to console
#endif
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
            printf(RED_MSG "Show msg failed: %s" ENDCOLOR, SDL_GetError()); //CAUTION: recursion
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
