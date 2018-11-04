//SDL2 wrappers and helpers

#ifndef _SDL_HELPERS_H
#define _SDL_HELPERS_H


#include <map>
#include <mutex>
#include <string>
#include <cstdlib> //atexit()
#include <memory> //std::unique_ptr<>
#include <utility> //std::forward<>
#include <type_traits> //std::conditional, std::enable_if, std::is_same, std::disjunction, etc
#include <climits> //<limits.h> //*_MIN, *_MAX
#include <algorithm> //std::max()
#include <sstream> //std::ostringstream

#include "msgcolors.h" //*_MSG, ENDCOLOR, ENDCOLOR_ATLINE()
#include "srcline.h" //SrcLine, SRCLINE, TEMPL_ARGS
#include "debugexc.h" //debug(), exc()
#include "str-helpers.h" //commas()
#include "rpi-helpers.h" //isrpi()
#include "ostrfmt.h" //FMT()
#include "elapsed.h" //elapsed(), timestamp()
//#include "shmalloc.h" //AutoShmary<>


////////////////////////////////////////////////////////////////////////////////
////
/// misc utility helpers, functions, and macros:
//


//dummy defs for readability:
#define VOID
#define CONST
#define STATIC
//#define RET_VOID


//accept variable #3 macro args:
#ifndef UPTO_3ARGS
 #define UPTO_3ARGS(one, two, three, four, ...)  four
#endif
#ifndef UPTO_4ARGS
 #define UPTO_4ARGS(one, two, three, four, five, ...)  five
#endif


//these "functions" are macros to allow compiler folding and other optimizations

//get #elements in array:
#ifndef SIZEOF
 #define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))
#endif
//template version from http://www.cplusplus.com/forum/general/4125/
//template< typename T, size_t N >
//size_t ArraySize( T (& const)[ N ] ) { return N; }


//inline int toint(void* val) { return (int)(long)val; }
//#define toint(ptr)  reinterpret_cast<int>(ptr) //gives "loses precision" warning/error
#ifndef toint
 #define toint(expr)  (int)(long)(expr) //TODO: use static_cast<>?
#endif


//(int) "/"" gives floor(), use this to give ceiling():
//safe to use with MAX_INT
#ifndef divup
 #define divup(num, den)  ((((num) - 1) / (den)) + 1) //(((num) + (den) - 1) / (den)) //((den)? (((num) + (den) - 1) / (den)): -1)
#endif


#ifndef clamp
 #define clamp(val, min, max)  ((val) < (min)? (min): (val) > (max)? (max): (val))
#endif
//#define clip_255(val)  ((val) & 0xFF)


//dim/mix/blend values:
#define dim  mix_2ARGS
#define mix_2ARGS(dim, val)  mix_3ARGS(dim, val, 0)
#define mix_3ARGS(blend, val1, val2)  ((int)((val1) * (blend) + (val2) * (1 - (blend)))) //uses floating point
#define mix_4ARGS(num, den, val1, val2)  (((val1) * (num) + (val2) * (den - num)) / (den)) //use fractions to avoid floating point at compile time
#define mix(...)  UPTO_4ARGS(__VA_ARGS__, mix_4ARGS, mix_3ARGS, mix_2ARGS, mix_1ARG) (__VA_ARGS__)


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
//use this function instead of operator[] on const maps (operator[] is not const - will add an entry)
template <typename MAP, typename VAL> //... ARGS>
const char* unmap(MAP&& map, VAL&& value) //Uint32 value) //ARGS&& ... args)
{
    return map.count(value)? map.find(value)->second: "";
}
//template <typename KEY, typename VAL> //... ARGS>
//VAL unmap(const std::map<KEY, VAL>& map, VAL&& value)
//{
//    return map.count(value)? map.find(value)->second: "";
//}
//const std::map<Uint32, const char*> SDL_SubSystems =
//    SDL_AutoSurface SDL_CreateRGBSurfaceWithFormat(ARGS&& ... args, SrcLine srcline = 0) //UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
//        return SDL_AutoSurface(::SDL_CreateRGBSurfaceWithFormat(std::forward<ARGS>(args) ...), srcline); //perfect fwd


//define lamba function for named args:
#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ /*&*/ [&](auto& _)
#endif

//unpack named args:
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
//    debug(BLUE_MSG << "unpack ret" << ENDCOLOR);
    return params;
}


#if 0
//from https://www.codeproject.com/Articles/16150/Inheriting-a-C-enum-type
template <typename EnumT, typename BaseEnumT>
class InheritEnum
{
public:
  InheritEnum() {}
  InheritEnum(EnumT e)
    : enum_(e)
  {}

  InheritEnum(BaseEnumT e)
    : baseEnum_(e)
  {}

  explicit InheritEnum( int val )
    : enum_(static_cast<EnumT>(val))
  {}

  operator EnumT() const { return enum_; }
private:
  // Note - the value is declared as a union mainly for as a debugging aid. If 
  // the union is undesired and you have other methods of debugging, change it
  // to either of EnumT and do a cast for the constructor that accepts BaseEnumT.
  union
  { 
    EnumT enum_;
    BaseEnumT baseEnum_;
  };
};
enum Fruit { Orange, Mango, Banana };
enum NewFruits { Apple, Pear }; 
typedef InheritEnum< NewFruit, Fruit > MyFruit;
#endif


//utility class for tracing in/out:
class InOutDebug
{
public:
    explicit InOutDebug(const char* label = "", SrcLine srcline = 0): m_started(elapsed_msec()), m_label(label), m_srcline(NVL(srcline, SRCLINE)) { debug(BLUE_MSG << label << ": in" ENDCOLOR_ATLINE(srcline)); }
    virtual ~InOutDebug() { debug(BLUE_MSG << m_label << ": out after %f msec" ENDCOLOR_ATLINE(m_srcline), restart()); }
public: //methods
    double restart() //my_elapsed_msec(bool restart = false)
    {
        double retval = elapsed_msec() - m_started;
        /*if (restart)*/ m_started = elapsed_msec();
        return retval;
    }
private: //data members
    double m_started; //= -elapsed_msec();
    const char* m_label;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
};


////////////////////////////////////////////////////////////////////////////////
////
/// SDL headers, wrappers, utility functions, etc:
//

//helpful SDL info:
//https://wiki.libsdl.org/MigrationGuide#If_your_game_just_wants_to_get_fully-rendered_frames_to_the_screen
//SDL_RenderSetScale
//SDL_BlitScaled
//SDL_BlitSurface
//SDL_LowerBlit
//https://gamedev.stackexchange.com/questions/102870/rescale-pixel-art-scenery-before-rendering-in-sdl2


#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
//#ifndef NO_WANT_GL
//#include <SDL_opengl.h>
//#include <GL/gl.h>
//#endif
#include <SDL_endian.h> //SDL_BYTE_ORDER

//friendlier names for SDL special param values:
//#define UNUSED  0
#define DONT_CARE  0
//#define NO_RECT  NULL
//#define NO_PARENT_WND  NULL
#define FIRST_RENDERER_MATCH  -1
//#define THIS_THREAD  NULL
//#define UNDEF_EVTID  0


//scale factors (for readability):
//CAUTION: may need () depending on surroundings
#define msec  *1
#define sec  *1000


//reduce verbosity:
//#define SDL_Ticks()  SDL_GetPerformanceCounter()
//#define SDL_TickFreq()  SDL_GetPerformanceFrequency()
//#define SDL_PixelFormatShortName(fmt)  skip_prefix(SDL_GetPixelFormatName(fmt), "SDL_PIXELFORMAT_")


//timing stats:
#define SDL_Ticks()  SDL_GetPerformanceCounter()
#define SDL_TickFreq()  SDL_GetPerformanceFrequency()
inline uint64_t now() { return SDL_Ticks(); }
inline double elapsed(uint64_t& started, int scaled = 1) //Freq = #ticks/second
{
    uint64_t delta = now() - started;
    started += delta; //reset to now
    return scaled? (double)delta * scaled / SDL_TickFreq(): delta;
}
//inline double elapsed_usec(uint64_t started)
//{
////    static uint64_t tick_per_usec = SDL_TickFreq() / 1000000;
//    return (double)(now() - started) * 1000000 / SDL_TickFreq(); //Freq = #ticks/second
//}


//SDL retval conventions:
//0 == Success, < 0 == error, > 0 == data ptr (sometimes)
#define SDL_Success  0
#define SDL_OtherError  -2 //arbitrary; anything < 0
int SDL_LastError = SDL_Success; //remember last error (mainly for debug msgs)
//use overloaded function to handle different SDL retval types:
//#define SDL_OK(retval)  ((SDL_LastError = (retval)) >= 0)
inline bool SDL_OK(int errcode) { return ((SDL_LastError = errcode) >= 0); }
//inline bool SDL_OK(Uint32 retval, Uint32 badval) { return ((SDL_LastError = retval) != badval); }
//struct mySDL_Format; //fwd ref
//inline bool SDL_OK(mySDL_Format& retval) { return ((SDL_LastError = retval) != SDL_PIXELFORMAT_UNKNOWN); }
inline bool SDL_OK(SDL_bool ok, const char* why) { return SDL_OK((ok == SDL_TRUE)? SDL_Success: SDL_SetError(why)); }
inline bool SDL_OK(void* ptr) { return SDL_OK(ptr? SDL_Success: SDL_OtherError); } //SDL error text already set; just use dummy value for err code
//inline bool SDL_OK(void dummy) { return true; }
//#define SDL_OK_2ARGS(retval, okval)  ((retval) == (okval))?


//report SDL error (throw exc):
//use optional param to show msg instead of throw exc
//#define SDL_exc(what_failed)  exc(RED_MSG what_failed " failed: %s (error %d)" ENDCOLOR, SDL_GetError(), SDL_LastError)
#define SDL_errmsg(handler, what_failed, srcline)  handler(RED_MSG << what_failed << " failed: %s (error %d)" ENDCOLOR_ATLINE(srcline), NVL(SDL_GetError()), SDL_LastError)
#define SDL_exc_1ARG(what_failed)  SDL_errmsg(exc, what_failed, 0)
//#define SDL_exc_2ARGS(what_failed, want_throw)  ((want_throw)? SDL_errmsg(exc, what_failed, 0): SDL_errmsg(debug, what_failed, 0))
#define SDL_exc_2ARGS(what_failed, srcline)  SDL_errmsg(exc, what_failed, NVL(srcline, SRCLINE))
#define SDL_exc_3ARGS(what_failed, want_throw, srcline)  ((want_throw)? SDL_errmsg(exc, what_failed, NVL(srcline, SRCLINE)): SDL_errmsg(debug, what_failed, NVL(srcline, SRCLINE)))
#define SDL_exc(...)  UPTO_3ARGS(__VA_ARGS__, SDL_exc_3ARGS, SDL_exc_2ARGS, SDL_exc_1ARG) (__VA_ARGS__)


//fwd refs:
//class SDL_AutoLib;
//class templates:
//template <bool> class SDL_AutoWindow;
// /*template <bool>*/ class SDL_AutoTexture;


///////////////////////////////////////////////////////////////////////////////
////
/// augmented data types, formatters:
//

//allow inited libs or locked objects to be treated like other allocated SDL objects:
//typedef struct { int fd; } File;
//typedef struct { /*int dummy;*/ } SDL_lib;
//typedef struct { /*uint32_t evtid*/; } SDL_lib;
//typedef struct { int dummy; } IMG_lib;
//typedef struct { /*const*/ SDL_sem* sem; } SDL_LockedSemaphore;
//typedef struct { /*const*/ SDL_mutex* mutex; } SDL_LockedMutex;
//cache texture info in Surface struct for easier access:
//typedef struct { /*const*/ SDL_Texture* txr; SDL_Surface surf; uint32_t fmt; int acc; } SDL_LockedTexture;


//C++ note on struct vs. typedef: https://stackoverflow.com/questions/612328/difference-between-struct-and-typedef-struct-in-c

/*typedef*/ struct mySDL_Size //no: public SDL_Point
{
//won't support mult inh:    int& w = x; int& h = y; //kludge: rename parent's members, but still allow down-cast
    int w, h;
//    explicit SDL_Size(): w(0), h(0) {} //default ctor
    explicit mySDL_Size(int neww = 0, int newh = 0): w(neww), h(newh) {} //default ctor
    explicit mySDL_Size(const mySDL_Size& that): w(that.w), h(that.h) {} //copy ctor
//operator overload syntax/semantics: https://en.cppreference.com/w/cpp/language/operators
    inline bool operator==(const mySDL_Size& that) //lhs, const mySDL_Size& rhs)
    {
//    mySDL_Size& lhs = *this;
//    if (!&lhs || !&rhs) return (&lhs == &rhs); //handles NO_SIZE
//    return (lhs.w == rhs.w) && (lhs.h == rhs.h);
        return (w == that.w) && (h = that.h);
    }
    inline bool operator!=(const mySDL_Size& that) { return !(*this == that); } //lhs, const mySDL_Size& rhs) { return !(lhs == rhs); }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_Size& that)
    {
        ostrm << that.w << " x " << that.h;
        if (that.w && that.h) ostrm << " = " << commas(that.w * that.h);
        return ostrm;
    }
}; //mySDL_Size;
#define SDL_Size  mySDL_Size //use my def instead of SDL def (in case SDL defines one in future)
//const SDL_Size& NO_SIZE = *(SDL_Size*)0; //&NO_SIZE == 0
//SDL_Size NO_SIZE; //dummy value (settable)
#define NO_SIZE  NULL


//allow a rect to be used as a size or a point:
//typedef struct SDL_Rect: public SDL_Point, SDL_Size
/*typedef*/ struct mySDL_Rect: /*public*/ SDL_Rect
{
//public:
//NOTE: ctor can't access SDL_Rect members after ":", only within "{}" :(
    explicit mySDL_Rect(int newx = 0, int newy = 0, int neww = 0, int newh = 0) { x = newx; y = newy; w = neww; h = newh; } //: x(newx), y(newy), w(neww), h(newh) {} //default ctor
    mySDL_Rect(const mySDL_Rect& that): mySDL_Rect(that.x, that.y, that.w, that.h) {} //x(that.x), y(that.y), w(that.w), h(that.h) {} //copy ctor
    /*explicit*/ mySDL_Rect(const SDL_Point& newxy, const SDL_Size& newwh): mySDL_Rect(newxy.x, newxy.y, newwh.w, newwh.h) {} //x(newxy.x), y(newxy.y), w(newwh.w), h(newwh.h) {}
    SDL_Point& point() const { static SDL_Point xy; xy.x = x; xy.y = y; return xy; }
    SDL_Size& size() const { static SDL_Size wh; wh.w = w; wh.h = h; return wh; }
    /*explicit*/ operator SDL_Point*() const { return &point(); }
    /*explicit*/ operator SDL_Size*() const { return &size(); }
//inspect SDL_Rect (mainly for debug msgs):
//const std::string/*&*/ rect_desc(const SDL_Rect* rect)
    inline bool operator==(const mySDL_Rect& that) //lhs, const mySDL_Rect& rhs)
    {
//    if (!&lhs || !&rhs) return (&lhs == &rhs); //handles NO_RECT
//        return (lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.w == rhs.w) && (lhs.h == rhs.h);
        return (x == that.x) && (y == that.y) && (w == that.w) && (h == that.h);
    }
    inline bool operator!=(const mySDL_Rect& that) { return !(*this == that); } //lhs, const mySDL_Rect& rhs) { return !(lhs == rhs); }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_Rect& that)
    {
//    std::ostringstream ss;
//    if (!rect) ss << "all";
//    else ss << (rect->w * rect->h) << " ([" << rect->x << ", " << rect->y << "]..[+" << rect->w << ", +" << rect->h << "])";
//    return ss.str();
//    ostrm << "SDL_Rect";
        ostrm << "[" << that.x << "," << that.y << ", " << that.size() << "]"; //rect.w << " x " << rect.h << " = " << commas(rect.w * rect.h) << "]";
        return ostrm;
    }
}; //mySDL_Rect;
#define SDL_Rect  mySDL_Rect //use my def instead of SDL def
//const SDL_Rect& NO_RECT = *(SDL_Rect*)0; //&NO_RECT == 0
//SDL_Rect NO_RECT; //dummy value (settable)
#define NO_RECT  NULL


//enum mySDL_Format: Uint32 {};
//no worky: typedef Uint32 mySDL_Format; //for function specialization/overloading
#define SDL_PixelFormatShortName(fmt)  skip_prefix(SDL_GetPixelFormatName(fmt), "SDL_PIXELFORMAT_") //reduce verbosity
#if 0 //no worky
/*typedef*/ struct mySDL_Format //broken: Uint32 //: public Uint32[2]
{
//public:
    Uint32 u32;
    explicit mySDL_Format(Uint32 newfmt = 0): u32(newfmt) {}
//    mySDL_Format& operator=(int newfmt) { u32 = newfmt; return *this; }
//    mySDL_Format& operator=(Uint32 newfmt) { u32 = newfmt; return *this; }
    operator Uint32&() { return u32; }
//    Uint32& uint32() { return fmt; }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_Format& that)
    {
    debug(BLUE_MSG << SDL_PIXELFORMAT_UNKNOWN << ENDCOLOR);
    decltype(SDL_PIXELFORMAT_UNKNOWN) val;
    val = SDL_PIXELFORMAT_UNKNOWN;
        ostrm << /*FMT("%d") <<*/ SDL_BITSPERPIXEL(that.u32) << " bpp " << NVL(SDL_PixelFormatShortName(that.u32)); //kludge: use FMT() to avoid "ambiguous overload" errors
        return ostrm;
    }    
}; //mySDL_Format;
inline bool operator==(const mySDL_Format& lhs, const mySDL_Format rhs)
{
//    if (!&lhs || !&rhs) return (&lhs == &rhs); //handles NO_RECT
    return (lhs.u32 == rhs.u32);
}
inline bool operator!=(const mySDL_Format& lhs, const mySDL_Format& rhs) { return !(lhs == rhs); }
#endif
#define NO_FORMAT  SDL_PIXELFORMAT_UNKNOWN
typedef decltype(NO_FORMAT) mySDL_Format; //use exact type instead of generic Uint32 for type-safety and overloads
#define SDL_Format  mySDL_Format //use my def instead of SDL def (in case SDL defines one in future)
inline bool SDL_OK(mySDL_Format/*&*/ retval) { return ((SDL_LastError = retval) != SDL_PIXELFORMAT_UNKNOWN); }
mySDL_Format mySDL_GetWindowPixelFormat(CONST SDL_Window* wnd) { return (mySDL_Format)SDL_GetWindowPixelFormat(wnd); }
#define SDL_GetWindowPixelFormat  mySDL_GetWindowPixelFormat //use my def instead of SDL def
std::ostream& operator<<(std::ostream& ostrm, const mySDL_Format& that)
{
    ostrm << /*FMT("%d") <<*/ SDL_BITSPERPIXEL(that) << " bpp " << NVL(SDL_PixelFormatShortName(that)); //kludge: use FMT() to avoid "ambiguous overload" errors
    return ostrm;
}    

/*typedef*/ struct mySDL_DisplayMode: /*public*/ SDL_DisplayMode
{
//public:
//    myfakeSDL_format& mfmt;
//    SDL_Format& fmt; //= format; //rename + re-type parent's member
    SDL_Rect bounds; //= {0, 0, 0, 0};
    int num_disp = 0, which_disp = -1;
//    explicit mySDL_DisplayMode(): fmt(*(SDL_Format*)this) { } //kludge: rename + re-type parent's member
//    SDL_Format& fmt() const { SDL_Format conv(format); return conv; } //format; }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_DisplayMode& dispmode)
    {
//    ostrm << "mySDL_DisplayMode";
//printf("here1\n"); fflush(stdout);
        ostrm << "{" << dispmode.num_disp << " disp" << plural(dispmode.num_disp) << ", ";
        ostrm << dispmode.which_disp << ": ";
        ostrm << dispmode.bounds << ", "; //SDL_Size(dispmode.w, dispmode.h) << ", "; //dispmode.w << " x " << dispmode.h << ", ";
        ostrm << dispmode.refresh_rate << " Hz, ";
//printf("here2\n"); fflush(stdout);
        ostrm << "fmt " << /*(SDL_format)*/SDL_Format(dispmode.format);
        ostrm << "}";
//printf("here3\n"); fflush(stdout);
        return ostrm;
    }
}; //mySDL_DisplayMode;
#define SDL_DisplayMode  mySDL_DisplayMode //use my def instead of SDL def


/*typedef*/ struct mySDL_TextureInfo
{
    int access;
    SDL_Size wh;
    SDL_Format fmt;
}; //mySDL_TextureInfo;
#define SDL_TextureInfo  mySDL_TextureInfo //use my def instead of SDL def (in case SDL defines one in future)


//formatters for debug/messages:

/*typedef*/ struct mySDL_Point: /*public*/ SDL_Point
{
//public:
//NOTE: ctor can't access SDL_Rect members after ":", only within "{}" :(
    explicit mySDL_Point(int newx = 0, int newy = 0) { x = newx; y = newy; }
//    mySDL_Rect(const mySDL_Rect& that): mySDL_Rect(that.x, that.y, that.w, that.h) {} //x(that.x), y(that.y), w(that.w), h(that.h) {} //copy ctor
//inspect SDL_Rect (mainly for debug msgs):
//const std::string/*&*/ rect_desc(const SDL_Rect* rect)
#if 0
    inline bool operator==(const mySDL_Rect& that) //lhs, const mySDL_Rect& rhs)
    {
//    if (!&lhs || !&rhs) return (&lhs == &rhs); //handles NO_RECT
//        return (lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.w == rhs.w) && (lhs.h == rhs.h);
        return (x == that.x) && (y == that.y) && (w == that.w) && (h == that.h);
    }
    inline bool operator!=(const mySDL_Rect& that) { return !(*this == that); } //lhs, const mySDL_Rect& rhs) { return !(lhs == rhs); }
#endif
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_Point& that)
    {
//    std::ostringstream ss;
//    if (!rect) ss << "all";
//    else ss << (rect->w * rect->h) << " ([" << rect->x << ", " << rect->y << "]..[+" << rect->w << ", +" << rect->h << "])";
//    return ss.str();
//    ostrm << "SDL_Rect";
        ostrm << "(" << that.x << "," << that.y << ")";
        return ostrm;
    }
}; //mySDL_Rect;
#define SDL_Point  mySDL_Point //use my def instead of SDL def
//#define NO_RECT  NULL


#define SDL_RendererFlags  Uint32 //kludge: enum type doesn't allow bitwise arithmetic so override it

//inspect SDL_RendererInfo (mainly for debug msgs):
//TODO: operator<< ?
//const std::string/*no! &*/ renderer_desc(const SDL_RendererInfo& info)
std::ostream& operator<<(std::ostream& ostrm, const SDL_RendererInfo& rinfo)
{
    static const std::map<SDL_RendererFlags, const char*> SDL_RendererFlagNames =
    {
        {SDL_RENDERER_SOFTWARE, "SW"}, //0x01
        {SDL_RENDERER_ACCELERATED, "ACCEL"}, //0x02
        {SDL_RENDERER_PRESENTVSYNC, "VSYNC"}, //0x04
        {SDL_RENDERER_TARGETTEXTURE, "TOTXR"}, //0x08
//        {~(SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE), "????"},
    };

    std::ostringstream flag_desc, fmts; //, count;
    auto unused_flags = rinfo.flags;
    for (const auto& pair: SDL_RendererFlagNames)
        if (unused_flags & pair.first) { flag_desc << ";" << pair.second; unused_flags &= ~pair.first; }
    if (unused_flags) flag_desc << FMT(";??0x%x??") << unused_flags;
    if (!flag_desc.tellp()) flag_desc << ";";
    for (unsigned int i = 0; i < rinfo.num_texture_formats; ++i)
        fmts << ", " << (SDL_Format)rinfo.texture_formats[i]; //SDL_BITSPERPIXEL(rinfo.texture_formats[i]) << " bpp " << SDL_PixelFormatShortName(rinfo.texture_formats[i]);
//    if (!info.num_texture_formats) { count << "no fmts"; fmts << "  "; }
//    else if (info.num_texture_formats != 1) count << info.num_texture_formats << " fmts: ";
//    else count << "1 fmt: ";
//    std::ostringstream ostrm;
//    ostrm << "SDL_Renderer {" << FMT("rndr %p ") << rndr;
    ostrm << "{";
    ostrm << FMT("'%s'") << NVL(rinfo.name, "(none)");
    ostrm << FMT(", flags 0x%x (") << rinfo.flags << flag_desc.str().substr(1) << ")"; //<< FMT(" %s") << flags.str().c_str() + 1;
    SDL_Size wh(rinfo.max_texture_width, rinfo.max_texture_height);
    ostrm << ", max " << wh; //<< " == " << rinfo.max_texture_width << " x " << rinfo.max_texture_height;
    ostrm << ", " << rinfo.num_texture_formats << " fmt" << plural(rinfo.num_texture_formats) << ": " << fmts.str().substr(2); //c_str() + 2; //<< count.str() << FMT("%s") << fmts.str().c_str() + 2;
    ostrm << "}";
//    return ostrm.str();
    return ostrm;
}


////////////////////////////////////////////////////////////////////////////////
////
/// (A)RGB color defs
//

//(A)RGB primary colors:
//NOTE: consts below are processor-independent (hard-coded for ARGB msb..lsb)
//use later macros to adjust in-memory representation based on processor endianness (RGBA vs. ABGR)
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

#define mixARGB_2ARGS(dim, val)  mixARGB_3ARGS(dim, val, Abits(val)) //preserve Alpha
#define mixARGB_3ARGS(blend, c1, c2)  fromARGB(mix(blend, A(c1), A(c2)), mix(blend, R(c1), R(c2)), mix(blend, G(c1), G(c2)), mix(blend, B(c1), B(c2))) //uses floating point
#define mixARGB_4ARGS(num, den, c1, c2)  fromARGB(mix(num, den, A(c1), A(c2)), mix(num, den, R(c1), R(c2)), mix(num, den, G(c1), G(c2)), mix(num, den, B(c1), B(c2))) //use fractions to avoid floating point at compile time
#define mixARGB(...)  UPTO_4ARGS(__VA_ARGS__, mixARGB_4ARGS, mixARGB_3ARGS, mixARGB_2ARGS, mixARGB_1ARG) (__VA_ARGS__)
#define dimARGB  mixARGB_2ARGS

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
    {WHITE, "White"},
    {BLACK, "Black"},
};


/////////////////////////////////////////////////////////////////////////////////
////
/// SDL helpers, utility functions:
//

//SDL_HINT_RENDER_SCALE_QUALITY wrapper:
//use compiler to force correct values
//#define RENDER_SCALE_QUALITY_NEAREST  0 //"nearest"; //nearest pixel sampling
//#define RENDER_SCALE_QUALITY_LINEAR  1 //"linear"; //linear filtering (supported by OpenGL and Direct3D)
//#define RENDER_SCALE_QUALITY_BEST  2 //"best"; //anisotropic filtering (supported by Direct3D)
enum mySDL_HINT_RENDER_SCALE_QUALITY_choices: unsigned
{
    Nearest = 0, //"nearest"; //nearest pixel sampling
    Linear = 1, //"linear"; //linear filtering (supported by OpenGL and Direct3D)
    Best = 2, //"best"; //anisotropic filtering (supported by Direct3D)
};

inline int mySDL_SetRenderScaleQuality(mySDL_HINT_RENDER_SCALE_QUALITY_choices value)
{
//debug(BLUE_MSG "max uint %zu %zx, max uint / 2 %zu, %zx, 3 / 2 %zu, 9 / 4 %zu, 7 / 4 %zu" ENDCOLOR, UINT_MAX, UINT_MAX, UINT_MAX / 2, UINT_MAX / 2, 3 / 2, 9 / 4, 7 / 4);
//debug(BLUE_MSG "log2_floor(10) = %u, log2_ceiling(10) = %u, log2(64) = %u, log10(64) = %u, log10(%u) = %u, log2(1K) = %u, log10(1K) = %u" ENDCOLOR, log2_floor(10), log2_ceiling(10), log2(64), log10(64), UINT_MAX, log10(UINT_MAX), log2(1024), log10(1024));
    char buf[CompileTime::log10(UINT_MAX) + 1]; //one extra char for null terminator
    sprintf(buf, "%zu", value);
//    return (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, buf) == SDL_TRUE)? SDL_Success: SDL_OtherError; //SDL_SetHint already called SDL_SetError() so just return a dummy error#
    return (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, buf) == SDL_TRUE)? SDL_Success: SDL_OtherError; //SDL_SetHint already called SDL_SetError() so just return a dummy error#
}
#define SDL_SetRenderScaleQuality  mySDL_SetRenderScaleQuality //use my def instead of SDL def (in case SDL defines one in future)


#if 0
//window size check:
void SDL_GetDrawableSize_chk(CONST SDL_Window* wnd, SDL_Size* wh, SrcLine srcline = 0) //int* w, int*h) //separate int w, h allows caller to use rect or size; //SDL_Size* wh)
{
    SDL_Size cmp; //int cmpw, cmph;
    VOID SDL_GetWindowSize(wnd, &wh->w, &wh->h);
    VOID SDL_GL_GetDrawableSize(wnd, &cmp.w, &cmp.w); //NOTE: this one doesn't get correct data
    if (/*(cmp.w != wh->w) || (cmp.h != wh->h)*/ cmp != *wh) exc_soft("SDL_GetDrawableSize mismatch: " << *wh << " vs. " << cmp << " GL" ENDCOLOR_ATLINE(srcline));
//    if (w) *w = wh.w;
//    if (h) *h = wh.h;
//    return err;
}
#endif


//get current (prior to SDL changes) screen config:
#define FIRST_SCREEN  0
const mySDL_DisplayMode* ScreenInfo(int which = FIRST_SCREEN, SrcLine srcline = 0)
{
    static mySDL_DisplayMode dm;
    if ((which == dm.which_disp) && dm.num_disp) return &dm; //use cached info; assume won't change
    if (!dm.num_disp && !SDL_OK(dm.num_disp = SDL_GetNumVideoDisplays())) SDL_exc("get #displays");
    if (!SDL_OK(SDL_GetDesktopDisplayMode(which, &dm))) SDL_exc("get desktop display mode");
    if (!SDL_OK(SDL_GetDisplayBounds(which, &dm.bounds))) SDL_exc("get display bounds");
    dm.which_disp = which;
//    debug(BLUE_MSG "screen info: %d x %d, refresh %d Hz, fmt %d bpp %s" ENDCOLOR_ATLINE(srcline), dm.w, dm.h, dm.refresh_rate, SDL_BITSPERPIXEL(dm.format), NVL(SDL_PixelFormatShortName(fmt)));
    return &dm;
}
const mySDL_DisplayMode* ScreenInfo(SrcLine srcline = 0) { return ScreenInfo(FIRST_SCREEN, srcline); }


////////////////////////////////////////////////////////////////////////////////
////
/// SDL lib init/quit:
//


typedef Uint32  SDL_SubSystemFlags;


//SDL_init wrapper class:
//will only init as needed
//defers cleanup until process exit
//thread safe (although SDL may not be)
class mySDL_AutoLib //: public SDL_version //kludge: define some displayable data for operator<<()
{
//readable names (mainly for debug msgs):
//    static const /*std::map<Uint32, const char*>&*/ char* mySDL_SubSystemName(Uint32 key)
    static const std::map<SDL_SubSystemFlags, const char*>& SDL_SubSystemNames()
    {
        static const std::map<SDL_SubSystemFlags, const char*> names =
        {
            {SDL_INIT_TIMER, "Timer"}, //0x0001
            {SDL_INIT_AUDIO, "Audio"}, //0x0010
            {SDL_INIT_VIDEO, "Video"}, //0x0020
            {SDL_INIT_JOYSTICK, "Joystick"}, //0x0200
            {SDL_INIT_HAPTIC, "Haptic"}, //0x1000; force feedback subsystem
            {SDL_INIT_GAMECONTROLLER, "Game Controller"}, //0x2000
            {SDL_INIT_EVENTS, "Events"}, //0x4000
            {SDL_INIT_NOPARACHUTE, "NoParachute"}, //0x100000
            {SDL_INIT_EVERYTHING, "all"}, //0x0x7231
//            {~(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_EVERYTHING), "????"},
        };
        return names; //unmap(names, key); //names;
    }
    static const char* SDL_SubSystemName(SDL_SubSystemFlags key) { return unmap(SDL_SubSystemNames(), key); }
//no super
public: //ctor/dtor
//    SDL_lib(Uint32 flags) { init(flags); }
//    void init(Uint32 flags = 0, SrcLine srcline = 0)
    explicit mySDL_AutoLib(SrcLine srcline = 0): mySDL_AutoLib(0, NVL(srcline, SRCLINE)) {}
    /*explicit*/ mySDL_AutoLib(SDL_SubSystemFlags flags /*= 0*/, SrcLine srcline = 0): m_flags(flags), m_srcline(srcline)
    {
//        VOID SDL_GetVersion(this); //&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
//        std::lock_guard<std::mutex> guard(mutex()); //only allow one thread to init at a time
//        debug(GREEN_MSG "mySDL_AutoLib(%p) ctor: init 0x%x (%s)" ENDCOLOR_ATLINE(srcline), this, flags, NVL(SDL_SubSystemName(flags))); //SDL_SubSystems.count(flags)? SDL_SubSystems.find(flags)->second: "");
        INSPECT(GREEN_MSG << "ctor " << *this, srcline);
        Uint32 inited = SDL_WasInit(SDL_INIT_EVERYTHING);
        for (SDL_SubSystemFlags bit = 1; bit; bit <<= 1) //do one at a time
            if (flags & bit) //caller wants this one
                if (!SDL_SubSystemName(bit)) exc(RED_MSG "SDL_AutoLib: unknown subsys: 0x%x" ENDCOLOR_ATLINE(srcline)); //throw SDL_Exception("SDL_Init");
                else if (inited & bit) debug(BLUE_MSG "SDL_AutoLib: subsys '%s' (0x%x) already inited" ENDCOLOR_ATLINE(srcline), SDL_SubSystemName(bit), bit);
                else if (!SDL_OK(SDL_InitSubSystem(bit))) SDL_exc("SDL_AutoLib: init subsys " << FMT("'%s'") << SDL_SubSystemName(bit) << FMT(" (0x%x)") << bit << " failed", srcline);
                else
                {
                    std::lock_guard<std::mutex> guard(mutex()); //only allow one thread to init or get count at a time
//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen:
//no!        if (!SDL_OK(SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"), "SDL_SetHint")) SDL_exc("set hint");  // make the scaled rendering look smoother
                    if ((bit == SDL_INIT_VIDEO) && !SDL_OK(SDL_SetRenderScaleQuality(Nearest))) SDL_exc("Linear render scale quality", srcline);
                    debug(CYAN_MSG "SDL_AutoLib: subsys '%s' (0x%x) init[%d] success" ENDCOLOR_ATLINE(srcline), SDL_SubSystemName(bit), bit, all().size());
//                    std::lock_guard<std::mutex> guard(mutex());
                    if (!all().size()) first_time(srcline);
//                    if (!all().size()) atexit(cleanup); //defer cleanup in case other threads or processes want to use it
                    all().push_back(this); //cleanup() must be a static member, so make a list and do it all once at the time
                }
    }
    virtual ~mySDL_AutoLib() { INSPECT(RED_MSG << "dtor " << *this, m_srcline); } //debug(RED_MSG "mySDL_AutoLib(%p) dtor" ENDCOLOR_ATLINE(m_srcline), this); }
public: //operators
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_AutoLib& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
//        SDL_version ver;
//        SDL_GetVersion(&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
//        ostrm << "SDL_Lib {version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d}", ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
        SDL_SubSystemFlags flags = that.m_flags;
        std::ostringstream flag_desc;
        for (const auto& pair: SDL_SubSystemNames())
            if (flags & pair.first) { flag_desc << ";" << pair.second; flags &= ~pair.first; }
        if (flags) flag_desc << FMT(";??0x%x??") << flags; //unknown flags?
        if (!flag_desc.tellp()) flag_desc << ";";
//        debug_level(12, BLUE_MSG "SDL_Window %d x %d, fmt %i bpp %s, flags %s" ENDCOLOR_ATLINE(srcline), wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1);
        ostrm << "mySDL_AutoLib"; //<< my_templargs();
        ostrm << "{" << FMT("%p: ") << &that;
        ostrm << FMT("flags 0x%x (") << that.m_flags << flag_desc.str().substr(1) << ")"; //FMT(", flags %s") << flag_desc.str().c_str() + 1;
        ostrm << "}";
        return ostrm;
    }
//public: //methods
//    void quit()
private: //helpers
    static void first_time(/*const SDL_Lib* dummy,*/ SrcLine srcline = 0)
    {
        atexit(cleanup); //SDL_Quit); //defer cleanup in case caller wants more SDL later
//        SDL_AutoLib* dummy = 0;
//        debug_level(12, BLUE_MSG << *dummy << ENDCOLOR_ATLINE(srcline)); //for completeness
//std::thread::hardware_concurrency()
        SDL_version ver;
        VOID SDL_GetVersion(&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
        debug_level(12, BLUE_MSG "SDL version %d.%d.%d, platform '%s', #cores %d, ram %s MB, isRPi? %d" ENDCOLOR_ATLINE(srcline), ver.major, ver.minor, ver.patch, NVL(SDL_GetPlatform()), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//        debug_level(12, BLUE_MSG "platform: '%s', %d core%s, ram %s MB, isRPi? %d" ENDCOLOR_ATLINE(srcline), NVL(SDL_GetPlatform()), SDL_GetCPUCount(), plural(SDL_GetCPUCount()), NVL(commas(SDL_GetSystemRAM())), isRPi());
//TMI?
        debug_level(12, BLUE_MSG "%d video display%s:" ENDCOLOR_ATLINE(srcline), SDL_GetNumVideoDisplays(), plural(SDL_GetNumVideoDisplays()));
        for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
        {
            SDL_Rect bounds, usable;
            if (!SDL_OK(SDL_GetDisplayBounds(i, &bounds))) SDL_exc("get bounds");
            if (!SDL_OK(SDL_GetDisplayUsableBounds(i, &usable))) SDL_exc("get usable bounds");
            debug_level(12, BLUE_MSG "  [%d/%d]: name '%s' " << bounds << " (" << usable << " usable)" ENDCOLOR_ATLINE(srcline), i, SDL_GetNumVideoDisplays(), NVL(SDL_GetDisplayName(i)));
        }
//TMI?
        debug_level(12, BLUE_MSG "%d video driver%s:" ENDCOLOR_ATLINE(srcline), SDL_GetNumVideoDrivers(), plural(SDL_GetNumVideoDrivers()));
        for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
            debug_level(12, BLUE_MSG "  [%d/%d]: name '%s'" ENDCOLOR_ATLINE(srcline), i, SDL_GetNumVideoDrivers(), NVL(SDL_GetVideoDriver(i)));
//TMI?
        debug_level(12, BLUE_MSG "%d render driver%s:" ENDCOLOR_ATLINE(srcline), SDL_GetNumRenderDrivers(), plural(SDL_GetNumRenderDrivers()));
        for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i)
        {
            SDL_RendererInfo info;
//            std::ostringstream which; //, fmts, count, flags;
//            which << i << "/" << SDL_GetNumRenderDrivers();
            if (!SDL_OK(SDL_GetRenderDriverInfo(i, &info))) { SDL_exc("get renderer " << i << "/" << SDL_GetNumRenderDrivers(), srcline); continue; }
//            for (const auto& pair: SDL_RendererFlagNames)
//                if (info.flags & pair.first) flags << ";" << pair.second;
//            if (!flags.tellp()) flags << ";";
//            for (unsigned int i = 0; i < info.num_texture_formats; ++i)
//                fmts << ", " << SDL_BITSPERPIXEL(info.texture_formats[i]) << " bpp " << SDL_PixelFormatShortName(info.texture_formats[i]));
//            if (!info.num_texture_formats) { count << "no fmts"; fmts << "  "; }
//            else if (info.num_texture_formats != 1) count << info.num_texture_formats << " fmts: ";
//            debug_level(12, BLUE_MSG "Renderer[%s]: '%s', flags 0x%x %s, max %d x %d, %s%s" ENDCOLOR, which.str().c_str(), info.name, info.flags, flags.str().c_str() + 1, info.max_texture_width, info.max_texture_height, count.str().c_str(), fmts.str().c_str() + 2);
            debug_level(12, BLUE_MSG "  [%d/%d]: " << info << ENDCOLOR_ATLINE(srcline), i, SDL_GetNumRenderDrivers()); //, NVL(renderer_desc(info).c_str()));
        }
//NOTE: SDL_Init() seems to call bcm_host_init() on RPi to init VC(GPU) (or else it's no longer needed);  http://elinux.org/Raspberry_Pi_VideoCore_APIs
//        if (!SDL_OK(SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing file", "File is missing. Please reinstall the program.", NO_PARENT_WND))) SDL_exc("simple msg box", false);
//        INSPECT(*dummy, srcline);
    }
    static void cleanup()
    {
//        SrcLine srcline = all()[0].m_srcline; //use srcline of first caller
        SDL_SubSystemFlags inited = SDL_WasInit(SDL_INIT_EVERYTHING);
        for (auto it = all().begin(); it != all().end(); ++it)
            debug(CYAN_MSG "SDL_Lib: cleanup 0x%x (%s)" ENDCOLOR_ATLINE(0/*(*it)->m_srcline*/), inited, NVL(SDL_SubSystemName(inited), "multiple"));
        VOID SDL_Quit(); //all inited subsystems (1x only)
    }
#if 0
    static void inspect(SrcLine srcline = 0)
    {
//        SDL_version ver;
//        SDL_GetVersion(&ver);
//        debug_level(12, BLUE_MSG "SDL version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d" ENDCOLOR_ATLINE(srcline), ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//        debug_level(12, BLUE_MSG "%d video driver(s):" ENDCOLOR_ATLINE(srcline), SDL_GetNumVideoDrivers());
//        for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
//            debug_level(12, BLUE_MSG "Video driver[%d/%d]: name '%s'" ENDCOLOR_ATLINE(srcline), i, SDL_GetNumVideoDrivers(), SDL_GetVideoDriver(i));
        SDL_AutoLib dummy(srcline);
        debug_level(12, BLUE_MSG << dummy << ENDCOLOR_ATLINE(srcline));
    }
#endif
private: //data members
//    static int m_count = 0;
    SDL_SubSystemFlags m_flags;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
//    static /*std::atomic<int>*/int& count() //kludge: use wrapper to avoid trailing static decl at global scope
//    {
//        static /*std::atomic<int>*/int m_count = 0;
//        return m_count;
//    }
    static std::vector<mySDL_AutoLib*>& all() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::vector<mySDL_AutoLib*> m_all;
        return m_all;
    }
//    static std::mutex m_mutex;
    static std::mutex& mutex() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::mutex m_mutex;
        return m_mutex;
    }
};
#define SDL_AutoLib  mySDL_AutoLib //use my def instead of SDL def (in case SDL defines one in future)


////////////////////////////////////////////////////////////////////////////////
////
/// SDL Window:
//


#define NO_WINDOW_FLAGS  (SDL_WINDOW_FULLSCREEN & ~SDL_WINDOW_FULLSCREEN)
#define SDL_WindowFlags  Uint32 //kludge: enum type doesn't allow bitwise arithmetic so override it


//#define FIRST_SCREEN  (SDL_Rect*)-1
//#define SECOND_SCREEN  (SDL_Rect*)-2

//SDL_Window ptr auto-cleanup wrapper:
//includes a few factory helper methods
//includes Renderer option only because there is a CreateWindow variant for it; otherwise should be a child class
//        debug(RED_MSG "TODO: add streaming texture" ENDCOLOR_ATLINE(srcline));
//        debug(RED_MSG "TODO: add shm pixels" ENDCOLOR_ATLINE(srcline));
template <bool WantRenderer = true> //, WantTexture = true, WantPixels = true> //, bool DebugInfo = true>
class mySDL_AutoWindow: public std::unique_ptr<SDL_Window, std::function<void(SDL_Window*)>>
{
//readable names (mainly for debug msgs):
    static const std::map<SDL_WindowFlags, const char*> SDL_WindowFlagNames()
    {
        static const std::map<SDL_WindowFlags, const char*> names =
        {
            {SDL_WINDOW_FULLSCREEN, "FULLSCR"}, //0x0001
            {SDL_WINDOW_OPENGL, "OPENGL"}, //0x0002
            {SDL_WINDOW_SHOWN, "SHOWN"}, //0x0004
            {SDL_WINDOW_HIDDEN, "HIDDEN"}, //x0008
            {SDL_WINDOW_BORDERLESS, "BORDERLESS"}, //0x0010
            {SDL_WINDOW_RESIZABLE, "RESIZABLE"}, //0x0020
            {SDL_WINDOW_MINIMIZED, "MIN"}, //0x0040
            {SDL_WINDOW_MAXIMIZED, "MAX"}, //0x0080
            {SDL_WINDOW_INPUT_GRABBED, "GRABBED"}, //0x0100
            {SDL_WINDOW_INPUT_FOCUS, "FOCUS"}, //0x0200
            {SDL_WINDOW_MOUSE_FOCUS, "MOUSE"}, //0x0400
            {SDL_WINDOW_FOREIGN, "FOREIGN"}, //0x0800
            {SDL_WINDOW_FULLSCREEN_DESKTOP, "FULLDESK"}, //0x1001
//    {~(SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_INPUT_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_FOREIGN), "????"},
//    SDL_WINDOW_ALLOW_HIGHDPI = 0x00002000,      /**< window should be created in high-DPI mode if supported. On macOS NSHighResolutionCapable must be set true in the application's Info.plist for this to have any effect. */
//    SDL_WINDOW_MOUSE_CAPTURE = 0x00004000,      /**< window has mouse captured (unrelated to INPUT_GRABBED) */
//    SDL_WINDOW_ALWAYS_ON_TOP = 0x00008000,      /**< window should always be above others */
//    SDL_WINDOW_SKIP_TASKBAR  = 0x00010000,      /**< window should not be added to the taskbar */
//    SDL_WINDOW_UTILITY       = 0x00020000,      /**< window should be treated as a utility window */
//    SDL_WINDOW_TOOLTIP       = 0x00040000,      /**< window should be treated as a tooltip */
//    SDL_WINDOW_POPUP_MENU    = 0x00080000,      /**< window should be treated as a popup menu */
//    SDL_WINDOW_VULKAN        = 0x10000000 
        };
        return names; //unmap(names, key);
    }
protected:
//no worky :(    using super = std::unique_ptr; //no C++ built-in base class (due to multiple inheritance), so define one; compiler already knows template params so they don't need to be repeated here :); https://www.fluentcpp.com/2017/12/26/emulate-super-base/
    using super = std::unique_ptr<SDL_Window, std::function<void(SDL_Window*)>>;
public: //ctors/dtors
//    explicit SDL_AutoWindow(SrcLine srcline = 0): super(0, deleter), m_srcline(srcline) {} //InOutDebug inout("auto wnd def ctor", SRCLINE); } //no surface
//    template <typename ... ARGS>
//    SDL_AutoSurface(ARGS&& ... args, SrcLine = 0): super(0, deleter), sdllib(SDL_INIT_VIDEO, SRCLINE)
    explicit mySDL_AutoWindow(SDL_Window* wnd, SrcLine srcline = 0): super(0, deleter), m_srcline(srcline) //SDL_AutoWindow(srcline) //, sdllib(SDL_INIT_VIDEO, SRCLINE)
    {
//printf("here1\n"); fflush(stdout);
//        InOutDebug inout("auto wnd ctor", SRCLINE);
//        if (++count() > 30) exc("recursion?");
//        debug(GREEN_MSG "SDL_AutoWindow%s(%p) ctor %p" ENDCOLOR_ATLINE(srcline), my_templargs().c_str(), this, ptr);
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(wnd)) SDL_exc("SDL_AutoWindow: init window", srcline); //required window
        reset(wnd, NVL(srcline, SRCLINE)); //take ownership of window after checking
        INSPECT(GREEN_MSG << "ctor " << *this, srcline);
    }
    virtual ~mySDL_AutoWindow() { INSPECT(RED_MSG << "dtor " << *this, m_srcline); } //debug(RED_MSG "SDL_AutoWindow(%p) dtor %p" ENDCOLOR_ATLINE(m_srcline), this, get()); }
//for deleted function explanation see: https://www.ibm.com/developerworks/community/blogs/5894415f-be62-4bc0-81c5-3956e82276f3/entry/deleted_functions_in_c_11?lang=en
//NOTE: need to explicitly define this to avoid "use of deleted function" errors
    mySDL_AutoWindow(const mySDL_AutoWindow& that) { this->reset(((mySDL_AutoWindow)that).release()); } //kludge: wants rval, but value could change; //operator=(that.get()); } //copy ctor; //= delete; //deleted copy constructor
//amg with ctor    SDL_AutoWindow(/*const*/ SDL_Window* that) { this->reset(that); } //operator=(that); } //*this = that; } //copy ctor; //= delete; //deleted copy constructor
public: //factory methods:
//    template <typename ... ARGS>
//    static SDL_AutoWindow/*&*/ create(ARGS&& ... args, SrcLine srcline = 0) //title, x, y, w, h, mode)
    static mySDL_AutoWindow/*&*/ create(const char* title = 0, /*int x = 0, int y = 0, int w = 0, int h = 0,*/ const mySDL_Rect* rect = NO_RECT, /*Uint32*/ SDL_WindowFlags flags = NO_WINDOW_FLAGS, SrcLine srcline = 0)
    {
//        InOutDebug inout("auto wnd factory: create", SRCLINE);
//        if (++count() > 30) exc("recursion?");
        SDL_Window* wnd;
        SDL_Renderer* rndr;
        if (!flags) flags = SDL_WINDOW_FULLSCREEN_DESKTOP; //isRPi()? SDL_WINDOW_FULLSCREEN_DESKTOP: SDL_WINDOW_RESIZABLE; //TODO; SDL_WINDOW_FULLSCREEN/*_DESKTOP*/;
//        bool want_full = flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
//        Uint32 def_flags = isRPi()? SDL_WINDOW_FULLSCREEN_DESKTOP: SDL_WINDOW_RESIZABLE; //TODO
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, NVL(srcline, SRCLINE)); //init lib before creating window
//        if (!WantRenderer)
//            return SDL_AutoWindow(SDL_CreateWindow(title? title: "GpuPort", x? x: SDL_WINDOWPOS_UNDEFINED, y? y: SDL_WINDOWPOS_UNDEFINED, w? w: DONT_CARE, h? h: DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN), NVL(srcline, SRCLINE)); //std::forward<ARGS>(args) ...), //no-perfect fwd
//        else
//            return SDL_AutoWindow(!SDL_CreateWindowAndRenderer(w? w: DONT_CARE, h? h: DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN, &wnd, &rndr)? wnd: NULL, NVL(srcline, SRCLINE));
//        if (x < 0) x = 0; //let SDL handle the case of x >= w
//        if (y < 0) y = 0;
        SDL_Rect wrect = (rect != NO_RECT)? *rect: ScreenInfo(FIRST_SCREEN, NVL(srcline, SRCLINE))->bounds;
//        if (w <= 0) w += ScreenInfo(SRCLINE)->w; //- x; //default to whole screen; xlate < 0 to relative size
//        if (h <= 0) h += ScreenInfo(SRCLINE)->h; //- y;
        wnd = SDL_CreateWindow(NVL(title, "GpuPort"), wrect.x /*? x: SDL_WINDOWPOS_UNDEFINED*/, wrect.y /*? y: SDL_WINDOWPOS_UNDEFINED*/, wrect.w /*? w: DONT_CARE*/, wrect.h /*? h: DONT_CARE*/, flags | SDL_WINDOW_SHOWN); //std::forward<ARGS>(args) ...), //no-perfect fwd
        if (SDL_OK(wnd) && WantRenderer)
        {
            rndr = SDL_CreateRenderer(wnd, FIRST_RENDERER_MATCH, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); //use SDL_RENDERER_PRESENTVSYNC to get precise refresh timing
            if (!SDL_OK(rndr)) SDL_exc("cre rndr");
//no worky:
//to avoid letter boxing during scaling: 
//https://www.gamedev.net/articles/programming/engines-and-middleware/stretching-your-game-to-fit-the-screen-without-letterboxing-sdl2-r3547/
//            SDL_Rect rect = {0, 0, 10, 10};
//            if (!SDL_OK(SDL_RenderSetViewport(rndr, &rect))) SDL_exc("set rndr vwpt");
        }
        return mySDL_AutoWindow(wnd, NVL(srcline, SRCLINE));
//!supported?        if (flags & SDL_WINDOW_OPENGL) //create GL context
    }
//full screen example at: see https://wiki.libsdl.org/MigrationGuide#If_your_game_just_wants_to_get_fully-rendered_frames_to_the_screen
//    static SDL_AutoWindow/*&*/ fullscreen(SrcLine srcline = 0) { return fullscreen(0, NVL(srcline, SRCLINE)); }
#if 0 //overloaded SFINAE broken; use in-line logic instead (hopefully compiler will still optimize it)
//kludge: seem to need to create overloaded SFINAE as well?
    template <bool WantRenderer_copy = WantRenderer> //for function specialization
    static std::enable_if<WantRenderer_copy, SDL_AutoWindow/*&*/> fullscreen(SrcLine srcline = 0) { return fullscreen(0, NVL(srcline, SRCLINE)); } //SFINAE
//paranoid; no worky   template <bool WantRenderer_copy = WantRenderer> //for function specialization
//no    static std::enable_if<!WantRenderer_copy, SDL_AutoWindow/*&*/> fullscreen(Uint32 flags = 0, SrcLine srcline = 0) //SFINAE
    static SDL_AutoWindow/*& not allowed with rval ret; not needed with unique_ptr*/ fullscreen(Uint32 flags = 0, SrcLine srcline = 0)
    {
        debug(BLUE_MSG "want_renderer? " << WantRenderer << ENDCOLOR_ATLINE(srcline));
        InOutDebug inout("auto wnd withOUT renderer fullscreen", SRCLINE);
//        if (++count() > 30) exc("recursion?");
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, NVL(srcline, SRCLINE)); //init lib before creating window
        return SDL_AutoWindow(SDL_CreateWindow("GpuPort", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DONT_CARE, DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP), NVL(srcline, SRCLINE));
//        window = isRPi()?
//            SDL_CreateWindow(title, IGNORED_X_Y_W_H, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN): //| SDL_WINDOW_OPENGL): //don't use OpenGL; too slow
//            SDL_CreateWindow(title, 10, 10, MaxFit().w, MaxFit().h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN); //| SDL_WINDOW_OPENGL);
    }
    template <bool WantRenderer_copy = WantRenderer> //for function specialization
    static std::enable_if<WantRenderer_copy, SDL_AutoWindow/*&*/> fullscreen(Uint32 flags = 0, SrcLine srcline = 0) //SFINAE
    {
        debug(BLUE_MSG "want_renderer? " << WantRenderer << ENDCOLOR_ATLINE(srcline));
        InOutDebug inout("auto wnd with renderer fullscreen", SRCLINE);
        SDL_Window* wnd;
        SDL_Renderer* rndr;
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, NVL(srcline, SRCLINE)); //init lib before creating window
        return SDL_AutoWindow(!SDL_CreateWindowAndRenderer(DONT_CARE, DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP, &wnd, &rndr)? wnd: NULL, NVL(srcline, SRCLINE));
    }
    static SDL_AutoWindow/*& not allowed with rval ret; not needed with unique_ptr*/ fullscreen(Uint32 flags = 0, SrcLine srcline = 0)
    {
//        InOutDebug inout(WantRenderer? "auto wnd with renderer fullscreen": "auto wnd withOUT renderer fullscreen", SRCLINE);
//        if (++count() > 30) exc("recursion?");
//            return SDL_AutoWindow(SDL_CreateWindow("GpuPort", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DONT_CARE, DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP), NVL(srcline, SRCLINE));
        if (!WantRenderer) return create(0, 0, 0, 0, 0, 0, NVL(srcline, SRCLINE));
//        window = isRPi()?
//            SDL_CreateWindow(title, IGNORED_X_Y_W_H, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN): //| SDL_WINDOW_OPENGL): //don't use OpenGL; too slow
//            SDL_CreateWindow(title, 10, 10, MaxFit().w, MaxFit().h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN); //| SDL_WINDOW_OPENGL);
        SDL_Window* wnd;
        SDL_Renderer* rndr;
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, NVL(srcline, SRCLINE)); //init lib before creating window
        return SDL_AutoWindow(!SDL_CreateWindowAndRenderer(DONT_CARE, DONT_CARE, flags? flags: SDL_WINDOW_FULLSCREEN_DESKTOP, &wnd, &rndr)? wnd: NULL, NVL(srcline, SRCLINE));
    }
#endif
//    static SDL_AutoTexture& texture(int w, int h, SrcLine srcline = 0)
//    {
//TODO:
//        wnd.texture(W, H, SRCLINE);
//        VOID wnd.virtsize(W, H, SRCLINE);
//        SDL_AutoTexture txtr(SDL_AutoTexture::streaming(wnd.renderer(), SRCLINE));
//    }
public: //operators
    operator SDL_Window*() const { return get(); }
    mySDL_AutoWindow& operator=(mySDL_AutoWindow& that) { return operator=(that.release()); } //copy asst op; //, SrcLine srcline = 0)
    mySDL_AutoWindow& operator=(SDL_Window* ptr) //, SrcLine srcline = 0)
    {
//        SDL_Window* ptr = that.release();
//        if (!srcline) srcline = m_srcline;
        SrcLine srcline = m_srcline; //TODO: where to get this?
        debug(BLUE_MSG "SDL_AutoWindow: old window %p, new window %p" ENDCOLOR_ATLINE(srcline), get(), ptr);
        reset(ptr, NVL(srcline, SRCLINE));
        return *this; //fluent/chainable
    }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_AutoWindow& that) //CONST SDL_Window* wnd) //causes recursion via inspect: const SDL_AutoWindow& me) //CONST SDL_Window* wnd) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
        CONST SDL_Window* wnd = that.get();
        ostrm << "SDL_Window" << my_templargs();
        if (!wnd) return ostrm << "{wnd 0}";
//        if (wnd == (SDL_Window*)0) return ostrm << "{WND 0}";
//        int wndx, wndy, wndw, wndh;
        mySDL_Rect wrect;
        VOID SDL_GetWindowPosition(wnd, &wrect.x, &wrect.y);
        VOID SDL_GetWindowSize(wnd, &wrect.w, &wrect.h);
        /*Uint32*/ mySDL_Format fmt = SDL_GetWindowPixelFormat(wnd);
        if (!SDL_OK(fmt/*, SDL_PIXELFORMAT_UNKNOWN*/)) SDL_exc("Can't get window format", false, srcline);
//    if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_MSG "Can't get window format" ENDCOLOR));
        SDL_RendererInfo info = {0};
        SDL_Renderer* rndr = renderer(wnd, srcline); //SDL_GetRenderer(wnd);
        if (rndr && !SDL_OK(SDL_GetRendererInfo(/*renderer(wnd)*/ rndr, &info))) SDL_exc("can't get renderer info", srcline);
        std::ostringstream flag_desc;
        /*Uint32*/ SDL_WindowFlags flags = SDL_GetWindowFlags(wnd), svflags = flags;
        for (const auto& pair: SDL_WindowFlagNames())
            if (flags & pair.first) { flag_desc << ";" << pair.second; flags &= ~pair.first; }
        if (flags) flag_desc << FMT(";??0x%x??") << flags; //unknown flags?
        if (!flag_desc.tellp()) flag_desc << ";";
//        debug_level(12, BLUE_MSG "SDL_Window %d x %d, fmt %i bpp %s, flags %s" ENDCOLOR_ATLINE(srcline), wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1);
        ostrm << "\n{" << FMT("wnd %p: ") << wnd;
        ostrm << wrect; //wndw << " x " << wndh;
        ostrm << ", fmt " << fmt; //SDL_BITSPERPIXEL(fmt); //FMT(", fmt %i") << SDL_BITSPERPIXEL(fmt);
//        ostrm << " bpp " << NVL(SDL_PixelFormatShortName(fmt)); //FMT(" bpp %s") << NVL(SDL_PixelFormatShortName(fmt));
        ostrm << FMT(", flags 0x%x (") << svflags << flag_desc.str().substr(1) << ")"; //FMT(", flags %s") << flag_desc.str().c_str() + 1;
//        ostrm << *renderer(wnd); //FMT(", rndr %p") << renderer(wnd);
        ostrm << FMT(", rndr %p ") << rndr; //<< (rndr? renderer_desc(info): "(none)");
        if (rndr) ostrm << info;
        ostrm << "}";
        return ostrm; 
    }
#if 0
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const SDL_Renderer& rndr) //CONST SDL_Window* wnd) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        SDL_RendererInfo info;
        if (!SDL_OK(SDL_GetRendererInfo(rndr, &info))) SDL_exc("can't get renderer info");
        std::stringstream flags, fmts, count;
        for (const auto& pair: SDL_RendererFlagNames)
            if (info.flags & pair.first) flags << ";" << pair.second;
        if (!flags.tellp()) flags << ";";
        for (unsigned int i = 0; i < info.num_texture_formats; ++i)
            fmts << ", " << SDL_BITSPERPIXEL(info.texture_formats[i]) << " bpp " << SDL_PixelFormatShortName(info.texture_formats[i]));
        if (!info.num_texture_formats) { count << "no fmts"; fmts << "  "; }
        else if (info.num_texture_formats != 1) count << info.num_texture_formats << " fmts: ";
        ostrm << "SDL_Renderer {" << FMT("rndr %p ") << rndr;
        ostrm << FMT("'%s'") << info.name;
        ostrm << FMT(", flags 0x%x") << info.flags << FMT(" %s") << flags.str().c_str() + 1;
        ostrm << ", max " << info.max_texture_width << " x " << info.max_texture_height;
        ostrm << ", " << count.str() << FMT("%s") << fmts.str().c_str() + 2;
        ostrm << "}";
        return ostrm; 
    }
#endif
public: //methods; mostly just wrappers for static utility methods
    void virtsize(/*int w, int h,*/ const SDL_Size& wh, SrcLine srcline = 0) { VOID virtsize(get(), wh, srcline); }
    void reset(SDL_Window* new_ptr, SrcLine srcline = 0)
    {
//printf("reset here1\n"); fflush(stdout);
        if (new_ptr == get()) return; //nothing changed
//printf("reset here2\n"); fflush(stdout);
        if (new_ptr) check(new_ptr, NO_SIZE, NO_FORMAT, NVL(srcline, SRCLINE)); //validate before acquiring new ptr
//        if (new_ptr) INSPECT(*new_ptr, srcline); //inspect(new_ptr, NVL(srcline, SRCLINE));
        debug(BLUE_MSG << FMT("AutoWindow(%p)") << this << " taking ownership of " << FMT("%p") << new_ptr << ENDCOLOR_ATLINE(srcline));
        super::reset(new_ptr);
//printf("reset here3\n"); fflush(stdout);
        VOID INSPECT(*this, srcline);
//printf("reset here4\n"); fflush(stdout);
    }
    SDL_Renderer* renderer(SrcLine srcline = 0) { return renderer(get(), NVL(srcline, SRCLINE)); }
    void clear(Uint32 color = BLACK, SrcLine srcline = 0) { VOID clear(get(), color, srcline); }
//just use Named args rather than all these overloads:
//    void render(SDL_Texture* txtr, SrcLine srcline = 0) { render(txtr, true, NVL(srcline, SRCLINE)); }
//    void render(SDL_Texture* txtr, bool clearfb = true, SrcLine srcline = 0) { render(txtr, NO_RECT, NO_RECT, clearfb, NVL(srcline, SRCLINE)); }
//    void render(SDL_Texture* txtr, const SDL_Rect* src = NO_RECT, const SDL_Rect* dest = NO_RECT, bool clearfb = true, SrcLine srcline = 0)
    void render(SDL_Texture* txtr, const SDL_Rect* src = NO_RECT, const SDL_Rect* dest = NO_RECT, /*bool clearfb = true,*/ SrcLine srcline = 0) { VOID render(get(), txtr, src, dest, /*clearfb,*/ srcline); }
//    void update(void* pixels, SDL_Rect* rect = NO_RECT, SrcLine srcline = 0)
//    {
//TODO:
//        txtr.update(NULL, myPixels, SRCLINE); //W * sizeof (Uint32)); //no rect, pitch = row length
//        VOID wnd.render(txtr, false, SRCLINE); //put new texture on screen
//    }
public: //named arg variants
        struct CreateParams
        {
            const char* title = "GpuPort";
//            int x = 0, y = 0;
//            int w = 0, h = 0;
            const SDL_Rect* rect = NO_RECT; //{0, 0, 0, 0};
            SDL_WindowFlags flags = NO_WINDOW_FLAGS;
            SrcLine srcline = 0;
        };
    template <typename CALLBACK>
    static auto create(CALLBACK&& named_params)
    {
        struct CreateParams create_params;
        return create(unpack(create_params, named_params), Unpacked{});
    }

        struct RenderParams
        {
            CONST SDL_Texture* txtr; //no default
            const SDL_Rect* src = NO_RECT;
            const SDL_Rect* dest = NO_RECT;
//            bool clearfb = true;
//            Uint32 color = BLACK;
            SrcLine srcline = 0;
        };
    template <typename CALLBACK>
    auto render(CALLBACK&& named_params)
    {
        struct RenderParams render_params;
        return render(unpack(render_params, named_params), Unpacked{});
    }
private: //named arg variants
    static auto create(const CreateParams& params, Unpacked) { return create(params.title, /*params.x, params.y, params.w, params.h,*/ params.rect, params.flags, params.srcline); }
    auto render(const RenderParams& params, Unpacked) { VOID render(params.txtr, params.src, params.dest, /*params.clearfb,*/ params.srcline); }
public: //static utility methods
//    static void virtsize(SDL_Window* wnd, int w, int h, SrcLine srcline = 0) { VOID virtsize(renderer(wnd), w, h, srcline); }
//    static void virtsize(SDL_Renderer* rndr, int w, int h, SrcLine srcline = 0)
    static void virtsize(CONST SDL_Window* wnd, /*int w, int h,*/ const SDL_Size* want_wh = 0, SrcLine srcline = 0)
    {
//note about aspect ratio: https://forums.libsdl.org/viewtopic.php?p=38664
//        debug(BLUE_MSG "set wnd render logical size to %d x %d" ENDCOLOR_ATLINE(srcline), w, h);
//??        if (!SDL_OK(SDL_RenderSetIntegerScale(rndr, true))) SDL_exc("set render int scale", srcline);
//NOTE: this causes letter-boxing:
//don't use:        if (!SDL_OK(SDL_RenderSetLogicalSize(rndr, w, h))) SDL_exc("set render logical size", srcline); //use GPU to scale up to full screen
        SDL_Size wh; //int wndw, wndh;
        VOID SDL_GetWindowSize(wnd, &wh.w, &wh.h);
        float hscale = (want_wh && want_wh->w)? (float)want_wh->w / wh.w: 1, vscale = (want_wh && want_wh->h)? (float)want_wh->h / wh.h: 1;
        SDL_Renderer* rndr = renderer(wnd, srcline); //SDL_GetRenderer(wnd); //get());
//        if (!SDL_OK(rndr)) SDL_exc("get renderer");
        if (!SDL_OK(SDL_RenderSetScale(rndr, hscale, vscale))) SDL_exc("set render scale");
        debug(BLUE_MSG "virt size: %d x %d / %d x %d => scale %f x %f" ENDCOLOR_ATLINE(srcline), want_wh->w, want_wh->h, wh.w, wh.h, hscale, vscale);
    }
//NOTE: leaves in-memory copy stale, so not too useful
    static void clear(SDL_Window* wnd, Uint32 color = BLACK, SrcLine srcline = 0) { VOID clear(renderer(wnd, srcline), color, srcline); }
    static void clear(SDL_Renderer* rndr, Uint32 color = BLACK, SrcLine srcline = 0)
    {
//printf("render color here1\n"); fflush(stdout);
//        SDL_Renderer* rndr = renderer(wnd);
//no worky        for (int i = 0; i < 2; ++i) //SDL is double buffered by default; clear all copies
        if (!SDL_OK(rndr)) SDL_exc("get renderer");
        if (!SDL_OK(SDL_SetRenderDrawColor(rndr, R_G_B_A(color)))) SDL_exc("set render draw color", srcline);
        if (!SDL_OK(SDL_RenderClear(rndr))) SDL_exc("render clear", srcline);
        exc_soft(RED_MSG "broken %d,%d,%d,%d" ENDCOLOR, R_G_B_A(color));
//        SDL_RendererInfo rinfo = {0};
//        if (!SDL_OK(SDL_GetRendererInfo(rndr, &rinfo))) SDL_exc("can't get renderer info", srcline);
//        SDL_Size wh(rinfo.max_texture_width, rinfo.max_texture_height);
        SDL_Size wh;
//        if (!SDL_) //TODO
        debug(BLUE_MSG "set all " << wh << " pixels in window to color 0x%x" ENDCOLOR_ATLINE(srcline), color);
//printf("hello2 %p\n", rndr); fflush(stdout);
        VOID SDL_RenderPresent(rndr); //flips texture to screen
//printf("hello3 %p\n", rndr); fflush(stdout);
    }
    static void render(CONST SDL_Window* wnd, CONST SDL_Texture* txtr, const SDL_Rect* src = NO_RECT, const SDL_Rect* dest = NO_RECT, /*bool clearfb = true,*/ SrcLine srcline = 0) { VOID render(renderer(wnd, srcline), txtr, src, dest, /*clearfb,*/ srcline); }
    static void render(SDL_Renderer* rndr, CONST SDL_Texture* txtr, const SDL_Rect* src = NO_RECT, const SDL_Rect* dest = NO_RECT, /*bool clearfb = true,*/ SrcLine srcline = 0)
    {
#if 0
        uint64_t delta;
        delta = now() - times.previous; times.previous += delta; times.caller += delta;
        work();
        delta = now() - times.previous; times.previous += delta; times.encode += delta;
#endif
//        SDL_Renderer* rndr = renderer(wnd);
#if 0 //not needed if entire screen is copied
        bool clearfb = false;
        if (clearfb && !SDL_OK(SDL_RenderClear(rndr))) SDL_exc("render fbclear", srcline); //clear previous framebuffer
#endif
//        debug(BLUE_MSG "copy %s pixels from texture %p to %s pixels in window %p" ENDCOLOR_ATLINE(srcline), NVL(rect_desc(src).c_str()), txtr, NVL(rect_desc(dest).c_str()), get());
        if (!SDL_OK(SDL_RenderCopy(rndr, txtr, src, dest))) SDL_exc("render fbcopy", srcline); //copy texture to video framebuffer
        VOID SDL_RenderPresent(rndr); //update screen; NOTE: blocks until next V-sync (if SDL_RENDERER_PRESENTVSYNC is on)
    }
//    static void check(SDL_Window* wnd, SrcLine srcline = 0) { check(wnd, 0, 0, 0, NVL(srcline, SRCLINE)); }
    static void check(CONST SDL_Window* wnd, /*int want_w = 0, int want_h = 0,*/ const SDL_Size* want_wh = 0, /*Uint32*/ SDL_Format want_fmt = NO_FORMAT, SrcLine srcline = 0)
    {
        /*Uint32*/ SDL_Format fmt = SDL_GetWindowPixelFormat(wnd); //desktop OpenGL: 24 RGB8888, RPi: 32 ARGB8888
        if (!SDL_OK(fmt/*, SDL_PIXELFORMAT_UNKNOWN*/)) SDL_exc("Can't get window format", srcline);
//        if (want_fmts && (numfmt != want_fmts)) err(RED_MSG "Unexpected #formats: %d (wanted %d)" ENDCOLOR, numfmt, want_fmts);
        if (want_fmt && (fmt != want_fmt)) exc(RED_MSG "unexpected window format: got " << fmt << ", expected " << want_fmt << ENDCOLOR_ATLINE(srcline)); //, fmt, want_fmt);
        SDL_Size wh; //int wndw, wndh;
        VOID SDL_GetWindowSize(wnd, &wh.w, &wh.h);
        if (want_wh && (/*(wsz.w != want_sz.w) || (wsz.h != want_sz.h)*/ wh != *want_wh)) exc(RED_MSG "window mismatch: expected " << *want_wh << ", got " << wh << ENDCOLOR_ATLINE(srcline)); //, want_w, want_h, wndw, wndh);
//        myprintf(22, BLUE_MSG "cre wnd: max fit %d x %d => wnd %d x %d, vtx size %2.1f x %2.1f" ENDCOLOR, MaxFit().w, MaxFit().h, wndw, wndh, (double)wndw / (TXR_WIDTH - 1), (double)wndh / univ_len);
//TODO: check renderer also?
    }
//    static void inspect(SDL_Window* ptr, SrcLine srcline = 0) {} //noop
//    template <bool DebugInfo_copy = DebugInfo> //for function specialization
//    std::enable_if<DebugInfo_copy, static void> inspect(SDL_Window* ptr, SrcLine srcline = 0) //SFINAE
#if 0 //broken (recursion, no conv)
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
//        super no_ctor(wnd, [](SDL_Window* wnd){});
//        SDL_AutoWindow<false>& super autownd(wnd, srcline); //CAUTION: can't call ctor here (causes recursion)
        super wrapper(wnd, [](SDL_Window* wnd){}); //kludge: need unique_ptr<>, don't delete ptr
        debug_level(12, BLUE_MSG << /*static_cast<SDL_AutoWindow<>>(no_ctor)*/ wrapper << ENDCOLOR_ATLINE(srcline));
    }
#endif
//get renderer from window:
//this allows one window ptr to be used with/out AutoRenderer or a caller-supplied renderer
    static CONST SDL_Renderer* renderer(CONST SDL_Window* wnd, /*bool want_throw = true,*/ SrcLine srcline = 0)
    {
        CONST SDL_Renderer* rndr = SDL_GetRenderer(wnd);
        if (!SDL_OK(rndr)) SDL_exc("can't get renderer for window", /*want_throw,*/ srcline);
        return rndr;
    }
//private: //static helpers
    static void deleter(SDL_Window* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        delete_renderer(ptr); //delete renderer first (get it from window)
        debug(RED_MSG "SDL_AutoWindow: destroy window %p" ENDCOLOR, ptr);
        VOID SDL_DestroyWindow(ptr);
    }
#if 0 //overloaded SFINAE broken; use in-line logic instead (hopefully compiler will still optimize it)
//paranoid; no worky    template <bool WantRenderer_copy = WantRenderer> //for function specialization
    static void delete_renderer(SDL_Window* ptr) {} //noop
//    static std::enable_if<!WantRenderer_copy, void> delete_renderer(SDL_Window* ptr) {} //SFINAE; noop
    template <bool WantRenderer_copy = WantRenderer> //for function specialization
    static std::enable_if<WantRenderer_copy, void> delete_renderer(SDL_Window* ptr) //SFINAE
    {
        if (!ptr) return NULL; //kludge: compiler wants a return value here (due to enable_if<>?); //std::enable_if<WantRenderer_copy, void> NULL;
        SDL_Renderer* rndr = renderer(ptr, false); //CAUTION: don't throw(); don't want to interfere with window deleter()
        if (!rndr) return NULL;
        debug(BLUE_MSG "SDL_AutoWindow: destroy renderer %p" ENDCOLOR, ptr);
        VOID SDL_DestroyRenderer(rndr);
    }
#endif
    static void delete_renderer(SDL_Window* ptr)
    {
        if (!ptr || !WantRenderer) return;
        SDL_Renderer* rndr = SDL_GetRenderer(ptr); //renderer(ptr, false); //CAUTION: don't throw(); don't want to interfere with window deleter()
        if (!rndr) return;
        debug(RED_MSG "SDL_AutoWindow: destroy renderer %p" ENDCOLOR, ptr);
        VOID SDL_DestroyRenderer(rndr);
    }
private: //members
//    SDL_AutoLib sdllib;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
//    static /*std::atomic<int>*/int& count() //kludge: use wrapper to avoid trailing static decl at global scope
//    {
//        static /*std::atomic<int>*/int m_count = 0;
//        return m_count;
//    }
    static std::string& my_templargs() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::string m_templ_args(TEMPL_ARGS); //, dummy = m_templ_args.append("\n"); //only used for debug msgs
        return m_templ_args;
    }
};
#define SDL_AutoWindow  mySDL_AutoWindow //use my def instead of SDL def (in case SDL defines one in future)


////////////////////////////////////////////////////////////////////////////////
////
/// SDL Surface: OBSOLETE - just use uint32 array
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
        debug(BLUE_MSG "SDL_AutoSurface ctor %p" ENDCOLOR_ATLINE(srcline), ptr);
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(ptr)) exc(RED_MSG "SDL_AutoSurface: init surface failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_GetError(), SDL_LastError); //required surface
        reset(ptr, srcline); //take ownership of surface
    }
    virtual ~SDL_AutoSurface() { debug(BLUE_MSG "SDL_AutoSurface dtor %p" ENDCOLOR_ATLINE(m_srcline), get()); }
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
        debug(BLUE_MSG "SDL_AutoSurface: old surface %p, new surface %p" ENDCOLOR_ATLINE(srcline), get(), ptr);
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
        if (!surf->pixels || (toint(surf->pixels) & 7)) exc(RED_MSG "Surface pixels not aligned on 8-byte boundary: %p" ENDCOLOR_ATLINE(srcline), surf->pixels);
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
        debug(18, BLUE_MSG "Surface %p: %d x %d, pitch %s, size %s, must lock? %d, %s%s" ENDCOLOR_ATLINE(srcline), surf, surf->w, surf->h, commas(surf->pitch), commas(surf->h * surf->pitch), SDL_MUSTLOCK(surf), count.str().c_str(), fmts.str().c_str() + 1);
    }
//private: //static helpers
    static void deleter(SDL_Surface* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        debug(BLUE_MSG "SDL_AutoSurface: free surface %p" ENDCOLOR, ptr);
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
        debug(BLUE_MSG "SDL_MemorySurface ctor %p" ENDCOLOR_ATLINE(srcline), ptr);
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(ptr)) exc(RED_MSG "SDL_MemorySurface: init surface failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_GetError(), SDL_LastError); //required surface
        reset(ptr, srcline); //take ownership of surface
    }
    virtual ~SDL_MemorySurface() { debug(BLUE_MSG "SDL_MemorySurface dtor %p" ENDCOLOR_ATLINE(m_srcline), get()); }
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
        debug(BLUE_MSG "SDL_MemorySurface: old surface %p, new surface %p" ENDCOLOR_ATLINE(srcline), get(), ptr);
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
        if (!surf->pixels || (toint(surf->pixels) & 7)) exc(RED_MSG "Surface pixels not aligned on 8-byte boundary: %p" ENDCOLOR_ATLINE(srcline), surf->pixels);
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
        debug(18, BLUE_MSG "Surface %p: %d x %d, pitch %s, size %s, must lock? %d, %s%s" ENDCOLOR_ATLINE(srcline), surf, surf->w, surf->h, commas(surf->pitch), commas(surf->h * surf->pitch), SDL_MUSTLOCK(surf), count.str().c_str(), fmts.str().c_str() + 1);
    }
//private: //static helpers
    static void deleter(SDL_Surface* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        debug(BLUE_MSG "SDL_MemorySurface: free surface %p" ENDCOLOR, ptr);
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


#define MAKE_WINDOW  (SDL_Window*)-1
#define NO_ACCESS  (SDL_TEXTUREACCESS_STREAMING & ~SDL_TEXTUREACCESS_STREAMING)
#define SDL_TextureAccess  Uint32 //kludge: enum type doesn't allow bitwise arithmetic so override it

//SDL_Texture ptr auto-cleanup wrapper:
//includes a few factory helper methods
//template <bool WantPixelShmbuf = true> //, bool DebugInfo = true>
class mySDL_AutoTexture: public std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>
{
//readable names (mainly for debug msgs):
    static const /*std::map<Uint32, const char*>*/ char* SDL_TextureAccessName(SDL_TextureAccess key)
    {
        static const std::map<SDL_TextureAccess, const char*> names =
        {
            {SDL_TEXTUREACCESS_STATIC, "static"}, //changes rarely, not lockable
            {SDL_TEXTUREACCESS_STREAMING, "streaming"}, //changes frequently, lockable
            {SDL_TEXTUREACCESS_TARGET, "target"}, //can be used as a render target
            {~(SDL_TEXTUREACCESS_STATIC | SDL_TEXTUREACCESS_STREAMING | SDL_TEXTUREACCESS_TARGET), "????"},
        };
        return unmap(names, key); //names;
    }
protected:
//no worky :(    using super = std::unique_ptr; //no C++ built-in base class (due to multiple inheritance), so define one; compiler already knows template params so they don't need to be repeated here :); https://www.fluentcpp.com/2017/12/26/emulate-super-base/
    using super = std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>;
public: //ctors/dtors
//    explicit SDL_AutoTexture(SrcLine srcline = 0): super(0, deleter), m_srcline(srcline) {} //no surface
//    template <typename ... ARGS>
//    SDL_AutoSurface(ARGS&& ... args, SrcLine = 0): super(0, deleter), sdllib(SDL_INIT_VIDEO, SRCLINE)
    explicit mySDL_AutoTexture(SDL_Texture* txtr, SDL_Window* wnd, SrcLine srcline = 0): super(0, deleter), m_wnd(wnd, NVL(srcline, SRCLINE)), /*m_shmbuf(??),*/ m_srcline(srcline) //CAUTION: AutoWindow ctor needs a window value; //SDL_AutoTexture(NVL(srcline, SRCLINE)) //, sdllib(SDL_INIT_VIDEO, SRCLINE)
    {
//    (SDL_AutoWindow<true>::create(NAMED{ SRCLINE; }));
//        debug(RED_MSG "TODO: add shm pixel buf" ENDCOLOR_ATLINE(srcline));
//        if (!SDL_OK(SDL_get_surface(std::forward<ARGS>(args) ...))) exc(RED_MSG "SDL_AutoLib: init subsys '%s' (0x%x) failed: %s (err 0x%x)" ENDCOLOR_ATLINE(srcline), SDL_SubSystems.find(bit)->second, bit, SDL_GetError(), SDL_LastError);
        if (!SDL_OK(txtr)) SDL_exc("SDL_AutoTexture: init texture", NVL(srcline, SRCLINE)); //required window
//        m_wnd.reset(wnd); //CAUTION: caller loses ownership
        reset(txtr, NVL(srcline, SRCLINE)); //take ownership of window after checking
        debug(GREEN_MSG "SDL_AutoTexture(%p) ctor txtr %p, wnd %p" ENDCOLOR_ATLINE(srcline), /*TEMPL_ARGS.c_str(),*/ this, txtr, wnd);
    }
    virtual ~mySDL_AutoTexture() { debug(RED_MSG "SDL_AutoTexture(%p) dtor %p" ENDCOLOR_ATLINE(m_srcline), this, get()); }
//for deleted function explanation see: https://www.ibm.com/developerworks/community/blogs/5894415f-be62-4bc0-81c5-3956e82276f3/entry/deleted_functions_in_c_11?lang=en
//NOTE: need to explicitly define this to avoid "use of deleted function" errors
//    SDL_AutoTexture(const SDL_AutoTexture& that) { *this = that; } //operator=(that.get()); } //copy ctor; //= delete; //deleted copy constructor
//    SDL_AutoTexture(SDL_Texture* that) { operator=(that); } //*this = that; } //copy ctor; //= delete; //deleted copy constructor
    mySDL_AutoTexture(const mySDL_AutoTexture& that): m_wnd(that.m_wnd, SRCLINE) { this->reset(((mySDL_AutoTexture)that).release()); } //kludge: wants rval, but value could change; //operator=(that.get()); } //copy ctor; //= delete; //deleted copy constructor
//    SDL_AutoTexture(/*const*/ SDL_Texture* that) { this->reset(that); } //operator=(that); } //*this = that; } //copy ctor; //= delete; //deleted copy constructor
//    {
//        SrcLine srcline = m_srcline; //TODO: where to get this?
//        debug(BLUE_MSG "SDL_AutoTexture copy ctor: old texture %p, new texture %p" ENDCOLOR_ATLINE(srcline), get(), that.get());
//        reset(ptr, srcline);
//        return *this; //fluent/chainable
//    }
public: //factory methods:
#if 0
    template <typename ... ARGS>
    static SDL_AutoTexture/*&*/ create(ARGS&& ... args, SrcLine srcline = 0) //SDL_Renderer*, uint32 PixelFormat, int AccessMode, int width, int height)
    {
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, NVL(srcline, SRCLINE)); //init lib before creating window
        return SDL_AutoTexture(SDL_CreateTexture(std::forward<ARGS>(args) ...), NVL(srcline, SRCLINE)); //perfect fwd
    }
//    static SDL_AutoTexture/*&*/ streaming(/*SDL_Renderer* rndr*/ SDL_Window* wnd, int w, int h, SrcLine srcline = 0) { return streaming(/*rndr*/ wnd, w, h, 0, 0, NVL(srcline, SRCLINE)); }
#endif
    static mySDL_AutoTexture/*& not allowed with rval ret; not needed with unique_ptr*/ create(/*SDL_Renderer* rndr*/ CONST SDL_Window* wnd = MAKE_WINDOW, int screen = FIRST_SCREEN, /*int w = 0, int h = 0,*/ const SDL_Size* want_wh = 0, /*Uint32*/ SDL_Format want_fmt = NO_FORMAT, SDL_TextureAccess access = NO_ACCESS, /*key_t shmkey = 0,*/ SrcLine srcline = 0)
    {
//        bool wnd_owner = false;
        SDL_AutoLib sdllib(SDL_INIT_VIDEO, NVL(srcline, SRCLINE)); //init lib before creating window
        if (wnd == MAKE_WINDOW) //(SDL_Window*)-1)
        {
            SDL_Rect rect = ScreenInfo(screen, SRCLINE)->bounds; //NOTE: need to set (x, y) to select monitor
            if (want_wh) { rect.w = want_wh->w; rect.h = want_wh->h; }
            /*SDL_AutoWindow<true>*/ auto new_wnd(SDL_AutoWindow<true>::create(NAMED{ _.rect = &rect; SRCLINE; }));
            wnd = new_wnd.release(); //take ownership of new window before end of scope destroys it
//            wnd_owner = true;
        }
//        if (WantPixelShmbuf)
//            AutoShmary<Uint32, false> shmbuf((w + 2) * h, shmkey, srcline);
//    explicit AutoShmary(size_t ents, key_t key, SrcLine srcline = 0): m_ptr((key != KEY_NONE)? shmalloc(ents * sizeof(TYPE) + (WANT_MUTEX? sizeof(std::mutex): 0), key, srcline): 0), /*len(shmsize(m_ptr) / sizeof(TYPE)), key(shmkey(m_ptr)), existed(shmexisted(m_ptr)),*/ m_srcline(srcline)
        SDL_Size wh; //int wndw, wndh;
        if (want_wh) wh = *want_wh;
        else VOID SDL_GetWindowSize(wnd, &wh.w, &wh.h);
//        if (!w) w = wndw;
//        if (!h) h = wndh;
        SDL_Renderer* rndr = SDL_AutoWindow<>::renderer(wnd, srcline); //SDL_GetRenderer(wnd);
//        if (!SDL_OK(rndr)) SDL_exc("get window renderer", srcline);
//        if (!SDL_OK(SDL_RenderSetLogicalSize(rndr, w, h))) SDL_exc("set render logical size", srcline); //use GPU to scale up to full screen
        VOID SDL_AutoWindow<>::virtsize(wnd, &wh, SRCLINE);
        return mySDL_AutoTexture(SDL_CreateTexture(rndr, want_fmt? want_fmt: SDL_PIXELFORMAT_ARGB8888, access? access: SDL_TEXTUREACCESS_STREAMING, /*w? w: DONT_CARE, h? h: DONT_CARE*/ wh.w, wh.h), wnd, NVL(srcline, SRCLINE));
//        auto retval = SDL_AutoTexture(SDL_CreateTexture(rndr, fmt? fmt: SDL_PIXELFORMAT_ARGB8888, access? access: SDL_TEXTUREACCESS_STREAMING, w? w: DONT_CARE, h? h: DONT_CARE), /*wnd,*/ NVL(srcline, SRCLINE));
//        /*if (wnd_owner)*/ retval.m_wnd.reset(wnd); //take ownership; TODO: allow caller to keep ownership?
//        return retval;
    }
public: //operators
    operator SDL_Texture*() const { return get(); }
    mySDL_AutoTexture& operator=(mySDL_AutoTexture& that) { return operator=(that.release()); } //copy asst op; //, SrcLine srcline = 0)
    mySDL_AutoTexture& operator=(SDL_Texture* ptr) //, SrcLine srcline = 0)
    {
//        if (!srcline) srcline = m_srcline;
//        SDL_Texture* ptr = that.release();
        SrcLine srcline = m_srcline; //TODO: where to get this?
        debug(BLUE_MSG "SDL_AutoTexture: old texture %p, new texture %p" ENDCOLOR_ATLINE(srcline), get(), ptr);
        reset(ptr, NVL(srcline, SRCLINE));
        return *this; //fluent/chainable
    }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const mySDL_AutoTexture& that) //CONST SDL_Window* wnd) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
        CONST SDL_Texture* txtr = that.get();
        ostrm << "SDL_Texture"; //<< my_templargs(txtr);
        if (!txtr) return ostrm << "{txtr 0}";
/*
        int w, h; //texture width, height (in pixels)
        int access; //texture access mode (one of the SDL_TextureAccess values)
        Uint32 fmt; //raw format of texture; actual format may differ, but pixel transfers will use this format
        if (!SDL_OK(SDL_QueryTexture(txtr, &fmt, &access, &w, &h))) SDL_exc("query texture");
*/
//        int& cached_access = *(int*)&that.cached.userdata; //kludge: reuse element
//        SDL_Format& cached_fmt = *(SDL_Format*)&that.cached.format; //kludge: reuse ptr as actual value
//    if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_MSG "Can't get window format" ENDCOLOR));
//        ostrm << "SDL_Texture" << /*TEMPL_ARGS <<*/ 
        ostrm << "{" << FMT("txtr %p ") << txtr;
        ostrm << that.cached.wh; //me.cached.w << " x " << me.cached.h;
        ostrm << ", fmt " << that.cached.fmt; //FMT(", fmt %i") << SDL_BITSPERPIXEL(cached_fmt);
//        ostrm << FMT(" bpp %s") << NVL(SDL_PixelFormatShortName(cached_fmt));
        ostrm << FMT(", %s") << NVL(SDL_TextureAccessName(that.cached.access));
        ostrm << "}";
        return ostrm; 
    }
public: //methods
//    Uint32* Shmbuf() { return m_shmbuf; }
//    key_t Shmkey() { return m_shmbuf.smhkey(); }
#if 0
    void SetAlpha(Uint8 alpha, SrcLine srcline = 0)
    {
        if (!SDL_OK(SDL_SetTextureAlphaMod(get(), alpha))) SDL_exc("set texture alpha", srcline);
    }
    void SetBlendMode(SDL_BlendMode blend, SrcLine srcline = 0)
    {
        if (!SDL_OK(SDL_SetTextureBlendMode(get(), blend))) SDL_exc("set texture blend mode", srcline);
    }
    void SetColorMod(Uint8 r, Uint8 g, Uint8 b, SrcLine srcline = 0)
    {
        if (!SDL_OK(SDL_SetTextureColorMod(get(), r, g, b))) SDL_exc("set texture color mod", srcline);
    }
#endif
    void clear(Uint32 color = BLACK, SrcLine srcline = 0) //{ VOID render(get(), color, srcline); }
    {
        VOID SDL_AutoWindow<>::clear(m_wnd, color, NVL(srcline, SRCLINE));
    }
//use Named args rather than a bunch of overloads:
//    void update(const Uint32* pixels, SrcLine srcline = 0) { update(pixels, NO_RECT, NVL(srcline, SRCLINE)); }
//    void update(const Uint32* pixels, const SDL_Rect* rect = NO_RECT, SrcLine srcline = 0) { update(pixels, rect, 0, NVL(srcline, SRCLINE)); }
    void update(const Uint32* pixels, const SDL_Rect* rect = NO_RECT, int want_pitch = 0, SrcLine srcline = 0)
    {
//        if (!pixels) pixels = m_shmbuf;
//        if (!pitch) pitch = cached.bounds.w * sizeof(pixels[0]); //Uint32);
//        debug(BLUE_MSG "update %s pixels from texture %p, pixels %p, pitch %d" ENDCOLOR_ATLINE(srcline), NVL(rect_desc(rect).c_str()), get(), pixels, pitch);
        if (want_pitch && (want_pitch != cached.wh.w * sizeof(pixels[0]))) exc(RED_MSG "pitch mismatch: expected %d, got d" ENDCOLOR_ATLINE(srcline), cached.wh.w * sizeof(pixels[0]), want_pitch);
        if (!SDL_OK(SDL_UpdateTexture(get(), rect, pixels, cached.wh.w * sizeof(pixels[0])))) SDL_exc("update texture", srcline);
//        if (!SDL_OK(SDL_UpdateTexture(sdlTexture, NULL, myPixels, sizeof(myPixels[0])))) SDL_exc("update texture"); //W * sizeof (Uint32)); //no rect, pitch = row length
        VOID SDL_AutoWindow<>::render(m_wnd, get(), NO_RECT, NO_RECT, /*false,*/ SRCLINE); //put new texture on screen
    }
//    std::enable_if<WantPixelShmbuf_copy, void> update(const SDL_Rect* rect = NO_RECT, int pitch = 0, SrcLine srcline = 0) //SFINAE
//    {
//        VOID update(Shmbuf(), rect, pitch, srcline);
//    }
    void reset(SDL_Texture* new_ptr, SrcLine srcline = 0)
    {
        if (new_ptr == get()) return; //nothing changed
        /*SDL_Surface*/ SDL_TextureInfo new_cached;
        if (new_ptr) check(new_ptr, &new_cached, 0, NVL(srcline, SRCLINE)); //validate before acquiring new ptr
//        if (new_ptr) INSPECT(*new_ptr, srcline); //inspect(new_ptr, NVL(srcline, SRCLINE));
        super::reset(new_ptr);
        cached = new_cached; //keep cached info in sync with ptr
        VOID INSPECT(*this, srcline);
    }
public: //named arg variants
        struct CreateParams
        {
            CONST SDL_Window* wnd = MAKE_WINDOW; //(SDL_Window*)-1; //special value to create new window
  //          int w = 0, h = 0;
            const SDL_Size* wh = NO_SIZE;
//            int& w = size.w; //allor caller to set individually as well
//            int& h = size.h;
            SDL_Format fmt = NO_FORMAT;
            SDL_TextureAccess access = NO_ACCESS;
            int screen = FIRST_SCREEN;
            SrcLine srcline = 0;
        };
    template <typename CALLBACK>
    static auto create(CALLBACK&& named_params)
    {
        struct CreateParams create_params;
        return create(unpack(create_params, named_params), Unpacked{});
    }

        struct UpdateParams
        {
            /*const*/ Uint32* pixels = 0; //no default unless WantPixelShmbuf
//            const SDL_Rect rect; //= NO_RECT;
            const SDL_Rect* rect = NO_RECT;
//            int& w = size.w; //allor caller to set individually as well
//            int& h = size.h;
            int pitch = 0;
            SrcLine srcline = 0;
        };
    template <typename CALLBACK>
    auto update(CALLBACK&& named_params)
    {
        struct UpdateParams update_params;
        VOID update(unpack(update_params, named_params), Unpacked{});
    }
private: //named arg variant helpers
    auto update(const UpdateParams& params, Unpacked) { VOID update(params.pixels, params.rect, params.pitch, params.srcline); }
    static auto create(const CreateParams& params, Unpacked) { return create(params.wnd, params.screen, params.wh, params.fmt, params.access, params.srcline); }
public: //static helper methods
//    static void render(SDL_Window* wnd, Uint32 color = BLACK, SrcLine srcline = 0) { VOID render(renderer(wnd), color, srcline); }
//    static void render(SDL_Renderer* rndr, Uint32 color = BLACK, SrcLine srcline = 0)
//    static void check(SDL_Texture* txtr, SDL_Surface* cached, SrcLine srcline = 0) { check(txtr, cached, 0, 0, NVL(srcline, SRCLINE)); }
    static void check(SDL_Texture* txtr, SDL_TextureInfo* cached, /*int want_w = 0, int want_h = 0,*/ SDL_Size* want_wh = 0, SrcLine srcline = 0)
    {
        /*SDL_Surface*/ SDL_TextureInfo my_cache;
        if (!cached) cached = &my_cache; //cached.format = &my_cache.userdata; }
//        SDL_Size& cached_wh = *(SDL_Size*)&cached->w;
//        int& cached_access = *(int*)&cached->userdata; //kludge: reuse element
//        SDL_Format& cached_fmt = *(SDL_Format*)&cached->format; //kludge: reuse ptr as actual value
//        int w, h; //texture width, height (in pixels)
//        int access; //texture access mode (one of the SDL_TextureAccess values)
//        Uint32 fmt; //raw format of texture; actual format may differ, but pixel transfers will use this format
        if (!SDL_OK(SDL_QueryTexture(txtr, (Uint32*)&cached->fmt, &cached->access, &cached->wh.w, &cached->wh.h))) SDL_exc("query texture", srcline);
        if (want_wh && (/*(cached_wh.w != want_wh->w) || (cached_wh.h != want_wh->h)*/ cached->wh != *want_wh)) exc(RED_MSG "texture mismatch: expected " << *want_wh << " , got " << cached->wh << ENDCOLOR_ATLINE(srcline)); //, want_w, want_h, cached->w, cached->h);
        if (cached->access == SDL_TEXTUREACCESS_STREAMING) //lock() only used with streamed textures
        {
//            int pitch;
            Uint32* pixels; //don't give this back to caller; not valid unless texture is locked anyway
            void* pixels_void; //kludge: C++ doesn't like casting Uint32* to void* due to potential alignment issues
            int cached_pitch;
            if (!SDL_OK(SDL_LockTexture(txtr, NO_RECT, &pixels_void, &cached_pitch))) SDL_exc("lock texture", srcline);
            VOID SDL_UnlockTexture(txtr);
            if (cached_pitch != cached->wh.w * sizeof(pixels[0])) exc(RED_MSG "pitch mismatch: expected %d, got %d" ENDCOLOR_ATLINE(srcline), cached->wh.w * sizeof(pixels[0]), cached_pitch);
        }
    }
//    static void inspect(SDL_Window* ptr, SrcLine srcline = 0) {} //noop
//    template <bool DebugInfo_copy = DebugInfo> //for function specialization
//    std::enable_if<DebugInfo_copy, static void> inspect(SDL_Window* ptr, SrcLine srcline = 0) //SFINAE
#if 0 //broken (recursion, no conv)
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
        SDL_AutoTexture autotxtr(txtr, NVL(srcline, SRCLINE));
        debug_level(12, BLUE_MSG << autotxtr << ENDCOLOR_ATLINE(srcline));
    }
#endif
//private: //static helpers
    static void deleter(SDL_Texture* ptr)
    {
        if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        debug(BLUE_MSG "SDL_AutoTexture: free texture %p" ENDCOLOR, ptr);
        VOID SDL_DestroyTexture(ptr);
    }
private: //member vars
//    SDL_AutoLib sdllib;
//    Uint32* m_shmbuf; //shared memory pixel array
//    AutoShmary<Uint32, true> m_shmbuf;
    /*SDL_Surface*/ SDL_TextureInfo cached; //cached texture info to avoid lock() just to get descr; format, w, h, pitch, pixels
//    typedef struct { SDL_Surface surf; uint32_t fmt; int acc; } SDL_CachedTextureInfo;
//    SDL_Window* m_wnd;
    SDL_AutoWindow<true> m_wnd;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
};
#define SDL_AutoTexture  mySDL_AutoTexture //use my def instead of SDL def (in case SDL defines one in future)


////////////////////////////////////////////////////////////////////////////////
////
/// OpenGL:
//



////////////////////////////////////////////////////////////////////////////////
////
/// Bkg render loop:
//

//#pragma message("TODO: bkg render loop");


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
#include <sstream> //std::ostringstream

#include "msgcolors.h"
#include "srcline.h"


class aclass
{
    SDL_AutoLib sdl;
    SrcLine m_srcline;
public:
    aclass(SrcLine srcline = 0): sdl(SDL_INIT_VIDEO, NVL(srcline, SRCLINE)), m_srcline(srcline) { debug(GREEN_MSG "aclass(%p) ctor" ENDCOLOR_ATLINE(srcline), this); }
    ~aclass() { debug(RED_MSG "aclass(%p) dtor" ENDCOLOR_ATLINE(m_srcline), this); }
};

void other(SrcLine srcline = 0)
{
    SDL_AutoLib sdl(SDL_INIT_EVERYTHING, SRCLINE); //SDL_INIT_VIDEO | SDL_INIT_AUDIO>
//    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR;
//    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
}


void afunc(SrcLine srcline = 0)
{
    SDL_AutoLib sdl(SDL_INIT_AUDIO, SRCLINE);
//    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR;
//    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
    other();
}

aclass AA(SRCLINE); //CAUTION: "A" conflicts with color macro; use another name

void lib_test()
{
    debug(PINK_MSG << "lib_test start" << ENDCOLOR);
    afunc();
    aclass BB(SRCLINE);
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
    debug(PINK_MSG << "api_test start" << ENDCOLOR);
    SDL_AutoLib sdllib(SDL_INIT_VIDEO, SRCLINE);

//give me the whole screen and don't change the resolution:
//    SDL_Window* sdlWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Window* sdlWindow;
    SDL_Renderer* sdlRenderer;
    const int W = 3 * 24, H = 64; //1111;
    if (!SDL_OK(SDL_CreateWindowAndRenderer(DONT_CARE, DONT_CARE, SDL_WINDOW_FULLSCREEN_DESKTOP, &sdlWindow, &sdlRenderer))) SDL_exc("cre wnd & rndr");
    debug(BLUE_MSG "wnd renderer %p already set? %d" ENDCOLOR, SDL_GetRenderer(sdlWindow), (SDL_GetRenderer(sdlWindow) == sdlRenderer));
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
    debug(BLUE_MSG "px[1,0] = 0x%x %s, [2,0] = 0x%x %s, [3,0] = 0x%x %s" ENDCOLOR, myPixels[1][0], NVL(unmap(ColorNames, myPixels[1][0])), myPixels[2][0], NVL(unmap(ColorNames, myPixels[2][0])), myPixels[3][0], NVL(unmap(ColorNames, myPixels[3][0])));
#endif
//primary color test:
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        for (int i = 0; i < W * H; ++i) myPixels[0][i] = palette[c]; //asRGBA(PINK);

        if (!SDL_OK(SDL_UpdateTexture(sdlTexture, NULL, myPixels, sizeof(myPixels[0])))) SDL_exc("update texture"); //W * sizeof (Uint32)); //no rect, pitch = row length
        if (!SDL_OK(SDL_RenderClear(sdlRenderer))) SDL_exc("render clear"); //clear previous framebuffer
        if (!SDL_OK(SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL))) SDL_exc("render copy"); //copy texture to video framebuffer
        debug(BLUE_MSG "set all %d pixels to 0x%x %s " ENDCOLOR, W * H, palette[c], NVL(unmap(ColorNames, palette[c])));
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
            debug(BLUE_MSG "set pixel[%d, %d] to 0x%x %s " ENDCOLOR, x, y, myPixels[y][x], NVL(unmap(ColorNames, myPixels[y][x])));
            VOID SDL_RenderPresent(sdlRenderer); //put new texture on screen; no retval to check

            if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
            VOID SDL_Delay(0.1 sec);
        }
    
    VOID SDL_DestroyTexture(sdlTexture);
    VOID SDL_DestroyRenderer(sdlRenderer);
    VOID SDL_DestroyWindow(sdlWindow);
}


//based on example code at https://wiki.libsdl.org/MigrationGuide
//using "fully rendered frames" style
void fullscreen_test()
{
    const int W = 4, H = 5;
    const SDL_Size wh(W, H); //int W = 4, H = 5; //W = 3 * 24, H = 32; //1111;
    debug(PINK_MSG << timestamp() << "fullscreen " << wh << " test start" << ENDCOLOR);
    /*SDL_AutoTexture<>*/ auto txtr(SDL_AutoTexture/*<true>*/::create(NAMED{ /*_.wnd = wnd; _.w = W; _.h = H;*/ _.wh = &wh; _.screen = 1-1; SRCLINE; }));
    VOID txtr.clear(mixARGB(1, 2, BLACK, WHITE), SRCLINE);
    VOID SDL_Delay(2 sec);

    Uint32 myPixels[H][W]; //NOTE: pixels are adjacent on inner dimension since texture is sent to GPU row by row
#if 0 //check if 1D index can be used instead of (X,Y); useful for fill() loops
//    myPixels[0][W] = RED;
//    myPixels[0][2 * W] = GREEN;
//    myPixels[0][3 * W] = BLUE;
//    debug(BLUE_MSG "px[1,0] = 0x%x %s, [2,0] = 0x%x %s, [3,0] = 0x%x %s" ENDCOLOR, myPixels[1][0], NVL(unmap(ColorNames, myPixels[1][0])), myPixels[2][0], NVL(unmap(ColorNames, myPixels[2][0])), myPixels[3][0], NVL(unmap(ColorNames, myPixels[3][0])));
    std::ostringstream buf;
    for (int i = 0; i < wh.w * wh.h; ++i) buf << ", [" << i << FMT("] %p") << &myPixels[0][i];
    debug(BLUE_MSG << buf.str().substr(2) << ENDCOLOR);
    buf.str("");
    for (int y = 0; y < wh.h; ++y) //fill in GPU xfr order (for debug/test only)
        for (int x = 0; x < wh.w; ++x)
            buf << ", " << SDL_Point(y, x) << FMT(" %p") << &myPixels[y][x];
    debug(BLUE_MSG << buf.str().substr(2) << ENDCOLOR);
    return;
#endif
//primary color test:
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        for (int i = 0; i < wh.w * wh.h; ++i) (&myPixels[0][0])[i] = palette[c]; //(i & 1)? BLACK: palette[c]; //asRGBA(PINK);
        debug(BLUE_MSG << timestamp() << "0x%x => all " << wh << " pixels" ENDCOLOR, myPixels[0][0]);
        VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; SRCLINE; }); //, true, SRCLINE); //, sizeof(myPixels[0]); //W * sizeof (Uint32)); //no rect, pitch = row length
        VOID SDL_Delay(2 sec);
        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
    }

//pixel test:
    for (int i = 0; i < wh.w * wh.h; ++i) (&myPixels[0][0])[i] = BLACK; //bypass compiler index limits
    for (int y = 0 + std::max(wh.h-5, 0), c = 0; y < wh.h; ++y) //fill in GPU xfr order (for debug/test only)
        for (int x = 0 + std::max(wh.w-5, 0); x < wh.w; ++x, ++c)
        {
            myPixels[y][x] = palette[c % SIZEOF(palette)]; //NOTE: inner dimension = X due to order of GPU data xfr
            debug(BLUE_MSG << timestamp() << "0x%x => [r %d, c %d]" ENDCOLOR, myPixels[y][x], y, x);
            VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; SRCLINE; }); //, true, SRCLINE); //W * sizeof (Uint32)); //no rect, pitch = row length
            VOID SDL_Delay(0.5 sec);
            if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
        }
}


//from https://gist.github.com/jordandee/94b187bcc51df9528a2f
//no-based on example code at https://wiki.libsdl.org/MigrationGuide
//no-and https://wiki.libsdl.org/SDL_GL_SwapWindow?highlight=%28%5CbCategoryAPI%5Cb%29%7C%28SDLFunctionTemplate%29
//no-using "OpenGL" style
void gl_test()
{
#if 0 //OpenGL needs desktop; not present in Stretch Lite
    SDL_AutoLib sdl(SDL_INIT_VIDEO, SRCLINE);
    SDL_Window* wnd = SDL_CreateWindow("SDL2/OpenGL Demo", 30, 30, 640, 480, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if (!SDL_OK(wnd)) SDL_exc("cre wnd");
    SDL_GLContext glctx = SDL_GL_CreateContext(wnd);
    if (!SDL_OK(glctx)) SDL_exc("cre gl ctx");
    const int VSYNC = 1; //for updates synchronized with the vertical retrace
    if !(SDL_OK(SDL_GL_SetSwapInterval(VSYNC))) SDL_exc("set swap intv"); //makes our buffer swap syncronized with the monitor's vertical refresh
//??    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP); //SDL_WINDOW_FULLSCREEN ?  to preserve GL context when toggle windowed/fullscreen and back with OpenGL

/* Clear context */
//NOTE: default = double-buffered OpenGL contexts
//    glClearColor(0, 0, 0, 1);
//    glClear(GL_COLOR_BUFFER_BIT);
/* <Extra drawing fuctions here> */ 

/* Swap our buffer to display the current contents of buffer on screen */ 
//    SDL_GL_SwapWindow(window);

    for (;;)
    {
        static bool FullScreen = false;
        static Uint32 WindowFlags = SDL_WINDOW_OPENGL;
        bool running = true;
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
        {
            if (evt.type == SDL_QUIT) running = false;
            else if (evt.type == SDL_KEYDOWN)
                switch (evt.key.keysym.sym)
                {
                    case SDLK_ESCAPE: running = false; break;
                    case 'f':
                        if (FullScreen = !FullScreen)
                            SDL_SetWindowFullscreen(wnd, WindowFlags | SDL_WINDOW_FULLSCREEN_DESKTOP);
                        else
                            SDL_SetWindowFullscreen(wnd, WindowFlags);
                        break;
                    default: break;
                }
        }
        glViewport(0, 0, WinWidth, WinHeight);
        glClearColor(1.f, 0.f, 1.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);

        SDL_GL_SwapWindow(wnd);
        if (!running) break;
    }
#endif
}


//int main(int argc, const char* argv[])
void unit_test()
{
    debug(BLUE_MSG << FMT("75%% 256 = 0x%x") << dim(0.75, 256) << FMT(", 25%% 256 0x%x") << dim(0.25, 256) << ENDCOLOR);
    debug(BLUE_MSG << FMT("75%% white = 0x%x") << dimARGB(0.75, WHITE) << FMT(", 25%% white 0x%x") << dimARGB(0.25, WHITE) << ENDCOLOR);
    debug(BLUE_MSG << *ScreenInfo(SRCLINE) << ENDCOLOR);

    lib_test();
//    surface_test();
//    window_test();
//    sdl_api_test();
    fullscreen_test();
    gl_test();

//template <int FLAGS = SDL_INIT_VIDEO | SDL_INIT_AUDIO>
    debug(BLUE_MSG << "finish" << ENDCOLOR);
//    return 0;
}

#endif //def WANT_UNIT_TEST