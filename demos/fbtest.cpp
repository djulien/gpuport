#!/bin/bash
#Linux framebuffer test
BIN="${BASH_SOURCE%.*}"
CFLAGS="-I../src -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -O3 -std=c++11  -w -Wall -pedantic -Wvariadic-macros -fno-omit-frame-pointer -fno-rtti -fexceptions  -MMD -MF -g -x c++"
#CLIBS="`sdl2-config --libs`" #-lGL
rm $BIN 2> /dev/null #don't leave stale binary
set -x
g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" $CFLAGS - $CLIBS -o $BIN <<//EOF
#line 10 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
#pragma message("here")

//WS281X test using Linux framebuffer:
//build:  gcc -std=c++11 fbtest.cpp -o fbtest
//run:  fbtest
//NOTE: requires current userid to be a member of video group
//to add user to group: usermod -aG video "$USER"
//**need to log out and back in or "su username -"; see https://unix.stackexchange.com/questions/277240/usermod-a-g-group-user-not-work


#include <iostream> //std::cout
#include <unistd.h> //sleep(), usleep()
#include <cstdint> //uint32_t
#include <stdio.h> //printf()
#include <sstream> //std::ostringstream
#include <fcntl.h> //O_RDWR
#include <linux/fb.h> //struct fb_var_screeninfo, fb_fix_screeninfo
#include <sys/mman.h> //mmap()
#include <sys/ioctl.h> //ioctl()
#include <memory.h> //memmove()
#include <string.h> //snprintf()
#include <algorithm> //std::min<>(), std::max<>()
#include <ctype.h> //isxdigit()
#include <sys/stat.h> //struct stat

#include "msgcolors.h" //*_MSG, ENDCOLOR_*


#define CONST  //should be "const" but isn't
#define STATIC  //should be "static" but isn't

#define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))


#if 0 //broken
//kludge: wrapper to avoid trailing static decl at global scope:
//#define INIT_NONE  //dummy arg for macro
//kludge: use "..." to allow "," within INIT; still can't use "," within TYPE, though
#define STATIC_WRAP(TYPE, VAR, /*INIT*/ ...)  \
    /*static*/ inline TYPE& static_##VAR() \
    { \
        static TYPE m_##VAR /*=*/ /*INIT*/ __VA_ARGS__; \
        return m_##VAR; \
    }; \
    TYPE& VAR = static_##VAR() //kludge-2: create ref to avoid the need for "()"
//#define STATIC_WRAP_2ARGS(TYPE, VAR)  STATIC_WRAP_3ARGS(TYPE, VAR, INIT_NONE) //optional third param
//#define STATIC_WRAP(...)  UPTO_3ARGS(__VA_ARGS__, STATIC_WRAP_3ARGS, STATIC_WRAP_2ARGS, STATIC_WRAP_1ARG) (__VA_ARGS__)
//#define STATIC_WRAP  STATIC_WRAP_3ARGS
#endif


template <int BUFLEN, int COUNT>
#if 0
class StaticPoolBuf
{
    typedef char BUFTYPE[BUFLEN];
//    typedef BUFTYPE BUFPOOL[COUNT];
//broken    static STATIC_WRAP(BUFPOOL, m_pool);
//broken    static STATIC_WRAP(int, ff); //init val doesn't matter, wraps anyway
    BUFTYPE m_buf;
public: //ctor/dtor
    explicit StaticPoolBuf() {}
    ~StaticPoolBuf() {}
public: //operators
//    operator char*() { return this; }
    STATIC void* operator new(size_t size) //, size_t ents)
    {
//        size_t ents = 1;
        static BUFTYPE m_pool[COUNT];
        static int ff; //init value doesn't matter - pool is circular
        return m_pool[++ff % SIZEOF(m_pool)];
    }
    STATIC void operator delete(void* ptr) {} //noop - circular pool reuses memory
};
#else
char* StaticPoolBuf()
{
    static char m_pool[COUNT][BUFLEN];
    static int m_ff; //init value doesn't matter - pool is circular
//static int count = 0;
//if (!count++) printf("pool %p %p %p, [%d], %d\n", m_pool[0], m_pool[1], m_pool[2], ff % SIZEOF(m_pool), SIZEOF(m_pool));
//printf(" >>pool[%d/%d] %p<< ", m_ff, SIZEOF(m_pool), m_pool[(m_ff + 1) % SIZEOF(m_pool)]);
    return m_pool[++m_ff % SIZEOF(m_pool)];
}
#endif


//insert commas into a numeric string (for readability):
//CAUTION: uses static data to preserve data after return; semaphore arbitrates a pool of 12 ret values
/*const*/ char* commas(char* const buf, int GRPLEN) //= 3)
{
//buflen  #commas
// 0 - 3   0
// 4 - 6   1
// 7 - 9   2
//printf("commas('%s', %d) => ", buf, GRPLEN);
    char* bufend = buf + strlen(buf);
    char* sep = strrchr(buf, '.');
    for (char* bp = (sep? sep: bufend) - GRPLEN; bp > buf; bp -= GRPLEN)
    {
//        printf("buf[%d]: '%c' is hex digit? %d\n", bp - buf, *bp, !isxdigit(*bp));
        if (!isxdigit(bp[-1])) break;
        memmove(bp + 1, bp, ++bufend - bp); //CAUTION: incl null term
        *bp = ',';
    }
//printf("'%s' @%d\n", buf, __LINE__);
    return buf;
}

/*const*/ char* commas_ptr(void* ptr)
{
    static const int GRPLEN = 4;
    static const int NUMBUF = 4;
    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%p", ptr);
printf(" >>%p %s ptr %p<< ", bufp, bufp, ptr);
    return commas(bufp, GRPLEN);
}

/*const*/ char* commas_double(double val)
{
    static const int GRPLEN = 3;
    static const int NUMBUF = 12;
    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%f", val);
printf(" >>%p %s dbl %f<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}

/*const*/ char* commas_uint(uint32_t val)
{
    static const int GRPLEN = 3;
    static const int NUMBUF = 12;
    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%lu", val);
printf(" >>%p %s ui %lu<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}

/*const*/ char* commas_int(int32_t val)
{
#if 0
#define LIMIT  4 //    static const int LIMIT = 4; //max #commas to insert
    /*thread_local*/ static int ff; //std::atomic<int> ff; //use TLS to avoid the need for mutex (don't need atomic either)
    /*thread_local*/ static char buf[12][16 + LIMIT]; //allow 12 simultaneous calls (each thread if using TLS)
//    static auto_ptr<SDL_sem> acquire(SDL_CreateSemaphore(SIZE(buf)));
//    auto_ptr<SDL_LockedSemaphore> lock_HERE(acquire.cast); //SDL_LOCK(acquire));

    char* bufp = buf[++ff % SIZEOF(buf)] + LIMIT; //alloc ret val from pool; don't overwrite other values within same printf, allow space for commas
    for (int grplen = std::min(sprintf(bufp, "%ld", val), LIMIT * 3) - 3; grplen > 0; grplen -= 3)
    {
        memmove(bufp - 1, bufp, grplen);
        (--bufp)[grplen] = ',';
    }
    return bufp;
#else
    static const int GRPLEN = 3;
    static const int NUMBUF = 12;
    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%ld", val);
printf(" >>%p %s i %ld<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
#endif
}


///(A)RGB primary colors:
//NOTE: consts below are processor-independent (hard-coded for ARGB msb..lsb)
//internal SDL_Color is RGBA
//use later macros to adjust in-memory representation based on processor endianness (RGBA vs. ABGR)
//#pragma message("Compiled for ARGB color format (hard-coded)")
#define RED  (Amask | Rmask) //0xFFFF0000 //fromRGB(255, 0, 0) //0xFFFF0000
#define GREEN  (Amask | Gmask) //0xFF00FF00 //fromRGB(0, 255, 0) //0xFF00FF00
#define BLUE  (Amask | Bmask) //0xFF0000FF //fromRGB(0, 0, 255) //0xFF0000FF
#define YELLOW  (RED | GREEN) //0xFFFFFF00
#define CYAN  (GREEN | BLUE) //0xFF00FFFF
#define MAGENTA  (RED | BLUE) //0xFFFF00FF
#define PINK  MAGENTA //easier to spell :)
#define BLACK  (RED & GREEN & BLUE) //0xFF000000 //NOTE: needs Alpha
#define WHITE  (RED | GREEN | BLUE) //fromRGB(255, 255, 255) //0xFFFFFFFF
//#define ALPHA  0xFF000000
//use low brightness to reduce eye burn during testing:
//#define BLACK 0
//#define RED  0x1f0000
//#define GREEN  0x001f00
//#define BLUE  0x00001f
//#define YELLOW  0x1f1f00
//#define CYAN  0x001f1f
//#define MAGENTA  0x1f001f
//#define WHITE  0x1f1f1f

//set in-memory byte order according to architecture of host processor:
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN //Intel (PCs)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ //Intel (PCs)
 #pragma message("big endian")
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

#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ //ARM (RPi)
 #pragma message("little endian")
 #define Rmask  0x00FF0000 //0x000000FF
 #define Gmask  0x0000FF00
 #define Bmask  0x000000FF //0x00FF0000
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
 #error message("Unknown endian")
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

//#define R_G_B(color)  R(color), G(color), B(color)
//#define A_R_G_B(color)  A(color), R(color), G(color), B(color)
//#define R_G_B_A(color)  R(color), G(color), B(color), A(color)
//#define B_G_R(color)  B(color), G(color), R(color)
//#define A_B_G_R(color)  A(color), B(color), G(color), R(color)
//#define B_G_R_A(color)  B(color), G(color), R(color), A(color)

//#define mixARGB_2ARGS(dim, val)  mixARGB_3ARGS(dim, val, Abits(val)) //preserve Alpha
//#define mixARGB_3ARGS(blend, c1, c2)  fromARGB(mix(blend, A(c1), A(c2)), mix(blend, R(c1), R(c2)), mix(blend, G(c1), G(c2)), mix(blend, B(c1), B(c2))) //uses floating point
//#define mixARGB_4ARGS(num, den, c1, c2)  fromARGB(mix(num, den, A(c1), A(c2)), mix(num, den, R(c1), R(c2)), mix(num, den, G(c1), G(c2)), mix(num, den, B(c1), B(c2))) //use fractions to avoid floating point at compile time
//#define mixARGB(...)  UPTO_4ARGS(__VA_ARGS__, mixARGB_4ARGS, mixARGB_3ARGS, mixARGB_2ARGS, mixARGB_1ARG) (__VA_ARGS__)
//#define dimARGB  mixARGB_2ARGS

//#define R_G_B_A_masks(color)  Rmask(color), Gmask(color), Bmask(color), Amask(color)

//convert color ARGB <-> ABGR format:
//OpenGL seems to prefer ABGR format, but ARGB order is more readable (for me)
//convert back with same function & 0xffffff
//TODO: drop alpha setting?
//??	if (!Amask(color) /*&& (color & 0xffffff)*/) color |= 0xff000000; //RGB present but no alpha; add full alpha to force color to show
#define ARGB2ABGR(color)  ((color) & (Amask | Gmask) | (R(color) * Bshift) | (B(color) * Rshift)) //swap R <-> B
//#define SWAP32(uint32)  ((Amask(uint32) >> 24) | (Rmask(uint32) >> 8) | (Gmask(uint32) << 8) | (Bmask(uint32) << 24))


//framebuffer wrapper:
class FB
{
    typedef uint32_t PIXEL; //using PIXEL = uint32_t;
    int m_fd;
//    size_t m_fblen;
    PIXEL* m_fbp;
    bool m_cfg_dirty; //TODO: allow caller to change cfg?
    struct fb_var_screeninfo m_varinfo;
    struct fb_fix_screeninfo m_fixinfo;
    struct timespec m_started;
public: //members/properties
    const uint32_t& width;
    const uint32_t& height;
    const uint32_t& pitch; //bytes
//    double hscale, vscale; //allow stetch
    inline int KHz() const { return PICOS2KHZ(m_varinfo.pixclock); } //psec => KHz
    inline size_t rowlen(int num_rows = 1) const { return num_rows * m_fixinfo.line_length / sizeof(PIXEL); } //bytes => uint32's
    inline size_t num_pixels() const { return m_varinfo.yres * m_varinfo.xres; }
    inline size_t htotal() const { return m_varinfo.xres + m_varinfo.left_margin + m_varinfo.hsync_len + m_varinfo.right_margin; }
    inline size_t vtotal() const { return m_varinfo.yres + m_varinfo.upper_margin + m_varinfo.vsync_len + m_varinfo.lower_margin; }
    double fps() const { return (double)htotal() * vtotal() / m_varinfo.pixclock; }
//CAUTION: num_pixels might != fblen if lines are padded
    inline size_t fblen() const { return m_varinfo.yres * m_fixinfo.line_length; } //m_varinfo.xres * m_varinfo.bits_per_pixel / 8; }
public: //ctor/dtor
    explicit FB(): m_fd(-1), m_fbp((PIXEL*)-1), m_cfg_dirty(false), width(m_varinfo.xres), height(m_varinfo.yres), pitch(m_fixinfo.line_length) //, hscale(1), vscale(1)
    {
//open fb device for read/write:
        m_fd = ::open("/dev/fb0", O_RDWR);
        if (!m_fd || (m_fd == -1)) throw RED_MSG "Error: cannot open framebuffer device." ENDCOLOR_NOLINE;
//store config for quick access and in case caller changes it and wants to revert:
        if (ioctl(m_fd, FBIOGET_FSCREENINFO, &m_fixinfo) == -1) throw RED_MSG "Error reading fixed info." ENDCOLOR_NOLINE;
        if (ioctl(m_fd, FBIOGET_VSCREENINFO, &m_varinfo) == -1) throw RED_MSG "Error reading variable info." ENDCOLOR_NOLINE;
        printf("var info %d x %d, %d bpp, linelen %d, pxclk %f MHz, lrul marg %d %d %d %d, sync len h %d v %d, fps %f\n", 
            m_varinfo.xres, m_varinfo.yres, m_varinfo.bits_per_pixel, m_fixinfo.line_length, (double)PICOS2KHZ(m_varinfo.pixclock) / 1000,
            m_varinfo.left_margin, m_varinfo.right_margin, m_varinfo.upper_margin, m_varinfo.lower_margin, m_varinfo.hsync_len, m_varinfo.vsync_len,
            (double)(m_varinfo.xres + m_varinfo.left_margin + m_varinfo.hsync_len + m_varinfo.right_margin) * (m_varinfo.yres + m_varinfo.upper_margin + m_varinfo.vsync_len + m_varinfo.lower_margin ) / m_varinfo.pixclock);
        m_fbp = (PIXEL*)mmap(0, fblen(), PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_fbp == (PIXEL*)-1) throw RED_MSG "Failed to mmap." ENDCOLOR_NOLINE;
//        if (!m_varinfo.pixclock) m_varinfo.pixclock = -1;
        printf(GREEN_MSG "Framebuffer device opened successfully. @%d" ENDCOLOR_NEWLINE, __LINE__);
    	if (m_varinfo.bits_per_pixel / 8 != sizeof(PIXEL)) printf(YELLOW_MSG "Expected 32 bpp, got %d bpp @%d" ENDCOLOR_NEWLINE, m_varinfo.bits_per_pixel, __LINE__);
        if (rowlen() != width) printf(YELLOW_MSG "Line len (pitch) %s != width %s, likely padded @%d" ENDCOLOR_NEWLINE, commas_uint(rowlen()), commas_uint(width), __LINE__);
//    // Store for reset (copy vinfo to vinfo_orig)
//    memcpy(&orig_vinfo, &vinfo, sizeof(vinfo)); //struct fb_var_screeninfo));
//map fb to user mem
//        m_fblen = m_varinfo.xres * m_varinfo.yres * m_varinfo.bits_per_pixel / 8;
        clock_gettime(/*CLOCK_REALTIME*/ CLOCK_MONOTONIC, &m_started);
        std::cout << GREEN_MSG "FB ctor " << *this << " @" << __LINE__ << ENDCOLOR_NEWLINE;
    }
    ~FB() { close(); }
public: //operators
    inline PIXEL& operator[](int inx) { return m_fbp[inx]; }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FB& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        ostrm << "{" << commas_uint(sizeof(that)) << ": " << commas_ptr((void*)&that);
        if (!&that) return ostrm << " NULL}";
//        printf(BLUE_MSG "Original %dx%d, %d bpp, linelen %d, pxclk %d, lrul marg %d %d %d %d, sync len h %d v %d, fps %f" ENDCOLOR_NEWLINE,
//            m_varinfo.xres, m_varinfo.yres, m_varinfo.bits_per_pixel, m_fixinfo.line_length, m_varinfo.pixclock,
//            m_varinfo.left_margin, m_varinfo.right_margin, m_varinfo.upper_margin, m_varinfo.lower_margin, m_varinfo.hsync_len, m_varinfo.vsync_len,
//            (double)(m_varinfo.xres + m_varinfo.left_margin + m_varinfo.hsync_len + m_varinfo.right_margin) * (m_varinfo.yres + m_varinfo.upper_margin + m_varinfo.vsync_len + m_varinfo.lower_margin) / m_varinfo.pixclock);
        ostrm << ", " << commas_uint(that.fblen()) << ": " << commas_ptr((void*)that.m_fbp) << " my mmap";
        ostrm << ", " << commas_uint(that.m_fixinfo.smem_len) << ": " << commas_ptr((void*)that.m_fixinfo.smem_start) << " fb mem";
        ostrm << ", " << commas_uint(that.m_fixinfo.mmio_len) << ": " << commas_ptr((void*)that.m_fixinfo.mmio_start) << " fb mmap";
        ostrm << ", " << commas_uint(that.width) << " x " << commas_uint(that.height) << " @" << that.m_varinfo.bits_per_pixel << " bpp";
        ostrm << ", " << commas_uint(that.m_fixinfo.line_length) << " line len (bytes)";
        ostrm << ", " << (double)that.KHz() / 1000 << " MHz px clk";
        ostrm << ", " << commas_uint(that.m_varinfo.xres) << " (" << 100 * that.m_varinfo.xres / that.htotal() << "%) xvis + " << commas_uint(that.m_varinfo.left_margin) << " + " << commas_uint(that.m_varinfo.hsync_len) << " + " << commas_uint(that.m_varinfo.right_margin) << " (" << 100 * (that.m_varinfo.left_margin + that.m_varinfo.hsync_len + that.m_varinfo.right_margin) / that.htotal() << " %) hblank";
        ostrm << ", " << commas_uint(that.m_varinfo.yres) << " (" << 100 * that.m_varinfo.yres / that.vtotal() << "%) yvis + " << commas_uint(that.m_varinfo.upper_margin) << " + " << commas_uint(that.m_varinfo.vsync_len) << " + " << commas_uint(that.m_varinfo.lower_margin) << " (" << 100 * (that.m_varinfo.upper_margin + that.m_varinfo.vsync_len + that.m_varinfo.lower_margin) / that.vtotal() << " %) vblank";
        ostrm << ", " << commas_double(that.fps()) << " fps";
        return ostrm << "}";
    }
public: //methods
    long elapsed(bool reset = false) //msec
    {
        struct timespec now;
        clock_gettime(/*CLOCK_REALTIME*/ CLOCK_MONOTONIC, &now);
        long retval = (now.tv_sec - m_started.tv_sec) * 1e3; //sec -> msec
        retval += now.tv_nsec / 1e6 - m_started.tv_nsec / 1e6; //nsec -> msec
        if (reset) m_started = now;
        return retval;
    }
    inline size_t xy(int x, int y) const { return rowlen(y) + x; }
    PIXEL& pixel(int x, int y, uint32_t argb = 0)
    {
//        return m_fbp[y * m_fixinfo.line_length / sizeof(uint32_t) + x] = rgb << 8 | 0xFF; //fluent; TODO: alpha blending?
        PIXEL* pbp = &m_fbp[xy(x, y)];
        if (argb) *pbp = argb; //<< 8 | 0xFF; //ARGB => BGRA byte order; TODO: alpha blending?
        return *pbp; //fluent
    }
#if 0
    void pixel(int x, int y, int r, int g, int b)
    {
        uint8_t* pbp = (uint8_t*)&m_fbp[xy(x, y)]; //m_fbp + y * m_fixinfo.line_length + x * sizeof(uint32_t);
//BGRA byte order:
        *pbp++ = b;
        *pbp++ = g;
        *pbp++ = r;
        *pbp++ = 0xFF; //TODO: alpha blending?
    }
#endif
    static const int DEFAULT = -1; //kludge: can't use non-static members for arg defaults
    FB& fill(int x = 0, int y = 0, int w = DEFAULT, int h = DEFAULT, uint32_t argb = BLACK)
    {
        int xlimit = std::min(x + ((w == DEFAULT)? width: w), width), ylimit = std::min(y + ((h == DEFAULT)? height: h), height); //clip
//        PIXEL bgra = rgb; //| Amask; // << 8 | 0xFF; //ARGB => BGRA byte order; TODO: alpha blending?
  //      printf("fill(x %d, y %d, w %d, h %d) => %d..%d, %d..%d with 0x%lx @%d\n", x, y, w, h, x, xlimit, y, ylimit, bgra, __LINE__);
        for (int yofs = rowlen(y); y < ylimit; ++y, yofs += rowlen())
            for (int xofs = x; xofs < xlimit; ++xofs)
                m_fbp[yofs + xofs] = argb;
        return *this; //fluent
    }
    static void dump(const char* filename)
    {
        printf("dump framebuffer to '%s' ...\n", filename);
        std::ostringstream ss;
        ss << "cat /dev/fb0 > " << filename;
        system(ss.str().c_str());
    }
//explicit call to wait for vsync:
//various posts suggest that RPi video driver doesn't support vsync, so use this to force it
//NOTE: need sudo or be a member of video group to use this
//list groups: more /etc/group
//which groups am i a member of:  groups
//add user to group: usermod -aG video "$USER"
//**need to log out and back in or "su username -"; see https://unix.stackexchange.com/questions/277240/usermod-a-g-group-user-not-work
//vsync based on sample code from https://forums.libsdl.org/viewtopic.php?p=33010
//#include <unistd.h>
//#include <fcntl.h> //open(), close()
//#include <sys/ioctl.h> //ioctl()
//#include <linux/fb.h>
//#include <cstdlib> //atexit()
//#include <stdexcept> //std::runtime_error
#ifndef FBIO_WAITFORVSYNC
 #define FBIO_WAITFORVSYNC  _IOW('F', 0x20, __u32)
#endif
    void vsync()
    {
        int arg = 0;
//        if (fbfd < 0) return -1;
        if (ioctl(m_fd, FBIO_WAITFORVSYNC, &arg) < 0)
            /*if (want_exc)*/ throw std::runtime_error(strerror(errno)); //let caller deal with it
    }
//cleanup:
    void close()
    {
        if (m_fbp != (PIXEL*)-1) munmap(m_fbp, fblen());
        m_fbp = (PIXEL*)-1;
        if (m_cfg_dirty)
            if (ioctl(m_fd, FBIOPUT_VSCREENINFO, &m_varinfo) < 0)
                throw RED_MSG "Error re-setting variable information." ENDCOLOR_NOLINE;
        m_cfg_dirty = false;
        if (m_fd != -1) ::close(m_fd);
        m_fd = -1;
    }
};


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
    static tristate cached(tristate::Maybe); //NOTE: doesn't need to be thread_local; only one (bkg) thread should be calling this
//    static std::mutex protect;
//    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

//    myprintf(3, BLUE_MSG "isRPi()" ENDCOLOR);
//    serialize.lock(); //not really needed (low freq api), but just in case
    if (cached == tristate::Maybe) cached = exists("/boot/config.txt")? tristate::Yes: tristate::No;
//    serialize.unlock();
    return (cached == tristate::Yes);
}


#if 0
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <inttypes.h>
#include <errno.h>
#include <time.h>


#define _0H  16
#define _0L  48
#define _1H  36
#define _1L  28
#define _H(b)  ((b)? _1H: _0H)
#define _L(b)  ((b)? _1L: _0L)
#define BITW(b)  (((b) < 23)? 64: 48) //last bit is partially hidden
#define RGSWAP(rgb24)  ((((rgb24) >> 8) & 0xff00) | (((rgb24) << 8) & 0xff0000) | ((rgb24) & 0xff))


void draw(FB& fb)
{
	uint32_t colors[] = {RGSWAP(RED), RGSWAP(GREEN), BLUE, YELLOW, RGSWAP(CYAN), RGSWAP(MAGENTA), WHITE};
//for (int i = 0; i < SIZEOF(colors); ++i) printf("color[%d/%d]: 0x%x\n", i, SIZEOF(colors), colors[i]);
	long int scrsize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	memset(fbp, 0, scrsize);
//set first 10 nodes (24-1 bits):
//	uint32_t color = 0xff00ff; //R <-> G; //0x00ffff; //cyan (RGB)
for (int loop = 0; loop <= 10; ++loop)
{
for (int y = 0; y < 37; ++y)
	for (int b = 0; b < 24; ++b) //NOTE: last bit is partially hidden by hsync
	{
		uint32_t color = colors[(y + loop) % SIZEOF(colors)];
		if (loop == 10) color = 0;
//if (!b) printf("node[%d]: 0x%x\n", y, color);
		uint32_t bv = color & (0x800000 >> b);
		for (int i = 0; i < BITW(b); ++i)
		{
			int onoff = (i < _H(bv))? 0xff: 0;
			put_pixel(BITW(0) * b + i, y, onoff, onoff, onoff);
		}
	}
sleep(1);
}
}


// helper function for drawing - no more need to go mess with
// the main function when just want to change what to draw...
#define sqrt(x)  0
void old_draw() {

    int x, y;
    int r, g, b;
    int dr;
    int cr = vinfo.yres / 3;
    int cg = vinfo.yres / 3 + vinfo.yres / 4;
    int cb = vinfo.yres / 3 + vinfo.yres / 4 + vinfo.yres / 4;

    for (y = 0; y < (vinfo.yres); y++) {
        for (x = 0; x < vinfo.xres; x++) {
            dr = (int)sqrt((cr - x)*(cr - x)+(cr - y)*(cr - y));
            r = 255 - 256 * dr / cr;
            r = (r >= 0) ? r : 0;
            dr = (int)sqrt((cg - x)*(cg - x)+(cr - y)*(cr - y));
            g = 255 - 256 * dr / cr;
            g = (g >= 0) ? g : 0;
            dr = (int)sqrt((cb - x)*(cb - x)+(cr - y)*(cr - y));
            b = 255 - 256 * dr / cr;
            b = (b >= 0) ? b : 0;

                put_pixel(x, y, r, g, b);
        }
    }
}
#endif


//frame-rate test:
int test(FB& fb) //(*vsync(void)))
{
	const uint32_t palette[] = {RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE}; //RGSWAP(RED), RGSWAP(GREEN), BLUE, YELLOW, RGSWAP(CYAN), RGSWAP(MAGENTA), WHITE};
//for (int i = 0; i < SIZEOF(colors); ++i) printf("color[%d/%d]: 0x%x\n", i, SIZEOF(colors), colors[i]);
//	const int scrsize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
//    const int pitch32 = finfo.line_length / sizeof(uint32_t);
//    const int num_pixels = vinfo.xres * vinfo.yres;
//    uint32_t* const fb32p = (uint32_t*)fbp;

    int numfr = 0;
    for (int i = 0; i < fb.num_pixels(); ++i) fb[i] = BLACK;
    for (int loop = 0; loop < 10; ++loop) //10; ++loop)
    {
        uint32_t color = palette[loop % SIZEOF(palette)];
//#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ //ARM (RPi)
//        color = ARGB2ABGR(color); //R <-> B swap on RPi
//#endif
        for (int y = 0; y < fb.height; y += 16)
	        for (int b = 0; b < fb.width; b += 16)
            {
//                fb[fb.xy(b, y)] = palette[loop % SIZEOF(palette)];
//                if (!(numfr % 100))
                fb.fill(b, y, 16, 16, color);
//                if (!(b % 100)) printf("numfr %d\n", numfr);
//                if (b & 0xF) continue;
//                sleep(1); //sec
                fb.vsync();
                ++numfr;
            }
    }
    // now this is about the same as 'fbp[pix_offset] = value'
//    *((char*)(fbp + pix_offset)) = b;
//    *((char*)(fbp + pix_offset + 1)) = g;
//    *((char*)(fbp + pix_offset + 2)) = r;
//    *((char*)(fbp + pix_offset + 3)) = 0xff;
    return numfr;
}


// application entry point
int main(int argc, char* argv[])
{
    printf("sz(int) %d, sz(ui32) %d, sz(*) %d @%d\n", sizeof(int), sizeof(uint32_t), sizeof(void*), __LINE__);

    FB fb;
//    fb.dump("before.dat");
    int32_t numfr = test(fb);
//    printf("sleep(5) @%d\n", __LINE__);
//    sleep(5);
    printf("%d fr of %d px in %d ms\n", numfr, fb.num_pixels(), fb.elapsed());
    printf("%s frames of %s pixels in %s msec = %s fps @%d\n", commas_int(numfr), commas_uint(fb.num_pixels()), commas_int(fb.elapsed()), commas_double((double)1000 * numfr / fb.elapsed()), __LINE__);
//    fb.dump("after.dat");
    printf("done @%d\n", __LINE__);
    return 0; 
}

//EOF
#run if compiled successfully:
set -x
#if [ $? -eq 0 ] && [ -f $BIN ]; then
    $BIN
#fi
#eof