//Linux framebuffer wrappers and helpers
//NOTE: requires current userid to be a member of video group
//to add user to group: usermod -aG video "$USER"
//**need to log out and back in or "su username -"; see https://unix.stackexchange.com/questions/277240/usermod-a-g-group-user-not-work

//NOTE: code in here does not need to be thread-safe; it's only called by one (bkg) thread anyway, because SDL instead is not thread-safe
//TODO: determine if this code is thread-safe

#if !defined(_FB_HELPERS_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _FB_HELPERS_H //CAUTION: put this before defs to prevent loop on cyclic #includes

//#include <iostream> //std::cout
//#include <unistd.h> //sleep(), usleep()
#include <cstdint> //uint32_t
//#include <stdio.h> //printf()
#include <sstream> //std::ostringstream
#include <fcntl.h> //O_RDWR
#include <linux/fb.h> //struct fb_var_screeninfo, fb_fix_screeninfo
#include <sys/mman.h> //mmap()
#include <sys/ioctl.h> //ioctl()
#include <memory.h> //memmove()
//#include <string.h> //snprintf()
#include <algorithm> //std::min<>(), std::max<>()
//#include <ctype.h> //isxdigit()
//#include <sys/stat.h> //struct stat
#include <time.h> //struct timespec
#ifndef RPI_NO_X
 #include <X11/Xlib.h>
 #include <X11/extensions/xf86vmode.h> //XF86VidModeGetModeLine
 #include <memory> //std::unique_ptr<>
 #define XScreen  Screen //avoid confusion
 #define XDisplay  Display //avoid confusion
 #ifdef SIZEOF
  #undef SIZEOF //avoid conflict with xf86 def
 #endif
#endif

//#include "msgcolors.h" //*_MSG, ENDCOLOR_*
//#include "srcline.h" //SRCLINE
//#include "debugexc.h"
#include "logging.h"
//#include "color.h"

#define CONST  //should be "const" but isn't
#define STATIC  //should be "static" but isn't
#define VOID

#define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))

#ifndef divup
 #define divup(num, den)  (((num) + (den) - 1) / (den))
#endif

#ifndef rdiv
 #define rdiv(num, den)  (((num) + (den) / 2) / (den))
#endif


//define lamba function for named args:
#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ /*&*/ [&](auto& _)
#endif

//unpack named args:
//struct Unpacked {}; //ctor disambiguation tag
//returns unpacked struct to caller
template <typename UNPACKED, typename CALLBACK>
inline static UNPACKED& unpack(UNPACKED&& params, CALLBACK&& named_params)
{
//        static struct CtorParams params; //need "static" to preserve address after return
//        struct CtorParams params; //reinit each time; comment out for sticky defaults
//        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
//    printf("here31\n"); fflush(stdout);
    auto thunk = [](auto get_params, UNPACKED& params){ get_params(params); }; //NOTE: must be captureless (for use as template arg), so wrap it
//        MSG(BLUE_MSG << "get params ..." << ENDCOLOR);
//    printf("here32\n"); fflush(stdout);
    thunk(named_params, params);
//    printf("here33\n"); fflush(stdout);
//        MSG(BLUE_MSG << "... got params" << ENDCOLOR);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
//    debug(BLUE_MSG << "unpacked ret@ " << &params << ENDCOLOR);
    return params;
}


//framebuffer wrapper:
class FB
{
    struct fb_var_screeninfo m_varinfo;
    struct fb_fix_screeninfo m_fixinfo;
public: //members/properties
    typedef uint32_t PIXEL; //using PIXEL = uint32_t;
    const /*auto*/ decltype(m_varinfo.xres)& width; //uint32_t
    const /*auto*/ decltype(m_varinfo.xres)& height; //uint32_t
    const /*auto*/ decltype(m_fixinfo.line_length)& pitch; //bytes; //uint32_t
//    double hscale, vscale; //allow stetch
    inline /*decltype(m_varinfo.pixclock)*/ auto KHz() const { return PICOS2KHZ(m_varinfo.pixclock); } //psec => KHz
    inline size_t rowlen(int num_rows = 1) const { return num_rows * m_fixinfo.line_length / sizeof(PIXEL); } //bytes => uint32's
    inline size_t num_pixels() const { return m_varinfo.yres * m_varinfo.xres; }
    inline size_t htotal() const { return m_varinfo.xres + m_varinfo.left_margin + m_varinfo.hsync_len + m_varinfo.right_margin; }
    inline size_t vtotal() const { return m_varinfo.yres + m_varinfo.upper_margin + m_varinfo.vsync_len + m_varinfo.lower_margin; }
    double fps() const { return (double)htotal() * vtotal() / m_varinfo.pixclock; }
//CAUTION: num_pixels might != fblen if lines are padded
    inline size_t fblen() const { return m_varinfo.yres * m_fixinfo.line_length; } //m_varinfo.xres * m_varinfo.bits_per_pixel / 8; }
protected:
    int m_fd;
//    size_t m_fblen;
    PIXEL* m_fbp;
    bool m_cfg_dirty; //TODO: allow caller to change cfg?
//    struct timespec m_started;
public: //ctor/dtor
    explicit FB(SrcLine srcline = 0): m_fd(-1), m_fbp((PIXEL*)-1), m_cfg_dirty(false), width(m_varinfo.xres), height(m_varinfo.yres), pitch(m_fixinfo.line_length), m_srcline(srcline) //, m_started(now()) //, hscale(1), vscale(1)
    {
//open fb device for read/write:
        m_fd = ::open("/dev/fb0", O_RDWR);
        if (!m_fd || (m_fd == -1)) exc_hard("Error: cannot open framebuffer device");
//store config for quick access and in case caller changes it and wants to revert:
        if (ioctl(m_fd, FBIOGET_FSCREENINFO, &m_fixinfo) == -1) exc_hard("Error reading fixed info");
        if (ioctl(m_fd, FBIOGET_VSCREENINFO, &m_varinfo) == -1) exc_hard("Error reading variable info");
        if (!m_varinfo.pixclock)
        {
#ifdef RPI_NO_X
            exc_soft("TODO: vcgencmd measure_clock pixel");
#else //query video info from X Windows
            std::unique_ptr<XDisplay, std::function<void(XDisplay*)>> display(XOpenDisplay(NULL), XCloseDisplay);
            int num_screens = display.get()? ScreenCount(display.get()/*.cast*/): 0;
            debug(30, "got disp %p, #screens: %d" << ATLINE(srcline), display.get(), num_screens);
//            int dot_clock;
            for (int i = 0; i < num_screens; ++i)
            {
                int dot_clock; //, mode_flags;
//        XF86VidModeModeLine mode_line = {0};
//        XScreen screen = ScreenOfDisplay(display.cast, i);
//see https://ubuntuforums.org/archive/index.php/t-779038.html
//xvidtune-show
//"1366x768"     69.30   1366 1414 1446 1480        768  770  775  780         -hsync -vsync
//             pxclk MHz                h_field_len                v_field_len    
                XF86VidModeModeLine mode_line;
                if (!XF86VidModeGetModeLine(display.get()/*.cast*/, i, &dot_clock, &mode_line)) continue; //&mode_line)); //continue; //return FALSE;
//                debug(0, "X dot clock %d", dot_clock);
                m_varinfo.pixclock = 1e9 / dot_clock; //KHz => psec
                m_varinfo.xres = mode_line.hdisplay; //&h_active_pixels, //horizontal pixels (width)
                m_varinfo.left_margin = mode_line.hsyncstart - mode_line.hdisplay; //&h_front_porch, //horizontal forward padding from DE acitve edge
                m_varinfo.hsync_len = mode_line.hsyncend - mode_line.hsyncstart; //&h_sync_pulse, //hsync pulse width in pixel clocks
                m_varinfo.right_margin = mode_line.htotal - mode_line.hsyncend; //&h_back_porch, //vertical back padding from DE active edge
                m_varinfo.yres = mode_line.vdisplay; //&v_active_lines, //vertical pixels height (lines)
                m_varinfo.upper_margin = mode_line.vsyncstart - mode_line.vdisplay; //&v_front_porch, //vertical forward padding from DE active edge
                m_varinfo.vsync_len = mode_line.vsyncend - mode_line.vsyncstart; //&v_sync_pulse, //vsync pulse width in pixel clocks
                m_varinfo.lower_margin = mode_line.vtotal - mode_line.vsyncend; //&v_back_porch, //vertical back padding from DE active edge
//                cfg->htotal = cfg->hdisplay + cfg->hlead + cfg->hsync + cfg->htrail;
//                cfg->vtotal = cfg->vdisplay + cfg->vlead + cfg->vsync + cfg->vtrail;
                break;
            }
#endif
        }
        if (!m_varinfo.pixclock) { exc_soft("No pixclock cfg"); m_varinfo.pixclock = 1e6; }
        debug(30, "var info %d x %d, %d bpp, linelen %d, pxclk %4.2f MHz, LR/TB marg %d %d %d %d, sync len h %d v %d, fps %4.2f", 
            m_varinfo.xres, m_varinfo.yres, m_varinfo.bits_per_pixel, m_fixinfo.line_length, (double)PICOS2KHZ(m_varinfo.pixclock) / 1000,
            m_varinfo.left_margin, m_varinfo.right_margin, m_varinfo.upper_margin, m_varinfo.lower_margin, m_varinfo.hsync_len, m_varinfo.vsync_len,
            (double)(m_varinfo.xres + m_varinfo.left_margin + m_varinfo.hsync_len + m_varinfo.right_margin) * (m_varinfo.yres + m_varinfo.upper_margin + m_varinfo.vsync_len + m_varinfo.lower_margin ) / m_varinfo.pixclock);
        m_fbp = (PIXEL*)mmap(0, fblen(), PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_fbp == (PIXEL*)-1) exc_hard("Failed to mmap");
//        if (!m_varinfo.pixclock) m_varinfo.pixclock = -1;
        debug(30, GREEN_MSG "Framebuffer device opened successfully");
    	if (m_varinfo.bits_per_pixel / 8 != sizeof(PIXEL)) exc_soft("Expected 32 bpp, got %d bpp @%d", m_varinfo.bits_per_pixel);
        if (rowlen() != width) exc_soft("Line len (pitch) %s != width %s, likely padded", commas(rowlen()), commas(width));
//    // Store for reset (copy vinfo to vinfo_orig)
//    memcpy(&orig_vinfo, &vinfo, sizeof(vinfo)); //struct fb_var_screeninfo));
//map fb to user mem
//        m_fblen = m_varinfo.xres * m_varinfo.yres * m_varinfo.bits_per_pixel / 8;
        elapsed(true);
//        clock_gettime(/*CLOCK_REALTIME*/ CLOCK_MONOTONIC, &m_started);
        INSPECT(GREEN_MSG << "ctor " << *this << ATLINE(srcline));
    }
    /*virtual*/ ~FB() { close(); INSPECT(RED_MSG << "dtor " << *this << ", lifespan " << ((double)elapsed() / 1000) << " sec" << ATLINE(m_srcline)); } //debug(RED_MSG "mySDL_AutoLib(%p) dtor" ENDCOLOR_ATLINE(m_srcline), this); }
public: //operators
    inline PIXEL& operator[](int inx) { return m_fbp[inx]; }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FB& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        ostrm << "{" << commas(sizeof(that)) << ": " << commas((void*)&that);
        if (!&that) return ostrm << " NULL}";
//        printf(BLUE_MSG "Original %dx%d, %d bpp, linelen %d, pxclk %d, lrul marg %d %d %d %d, sync len h %d v %d, fps %f" ENDCOLOR_NEWLINE,
//            m_varinfo.xres, m_varinfo.yres, m_varinfo.bits_per_pixel, m_fixinfo.line_length, m_varinfo.pixclock,
//            m_varinfo.left_margin, m_varinfo.right_margin, m_varinfo.upper_margin, m_varinfo.lower_margin, m_varinfo.hsync_len, m_varinfo.vsync_len,
//            (double)(m_varinfo.xres + m_varinfo.left_margin + m_varinfo.hsync_len + m_varinfo.right_margin) * (m_varinfo.yres + m_varinfo.upper_margin + m_varinfo.vsync_len + m_varinfo.lower_margin) / m_varinfo.pixclock);
        ostrm << ", " << commas(that.fblen()) << ": " << commas((void*)that.m_fbp) << " my mmap";
        ostrm << ", " << commas(that.m_fixinfo.smem_len) << ": " << commas((void*)that.m_fixinfo.smem_start) << " fb mem";
        ostrm << ", " << commas(that.m_fixinfo.mmio_len) << ": " << commas((void*)that.m_fixinfo.mmio_start) << " fb mmap";
        ostrm << ", " << commas(that.width) << " x " << commas(that.height) << " @" << that.m_varinfo.bits_per_pixel << " bpp";
        ostrm << ", " << commas(that.m_fixinfo.line_length) << " line len (bytes)";
        ostrm << ", " << (double)that.KHz() / 1000 << " MHz px clk";
        ostrm << ", " << commas(that.m_varinfo.xres) << " (" << rdiv(100 * that.m_varinfo.xres, that.htotal()) << "%) xvis + " << commas(that.m_varinfo.left_margin) << " + " << commas(that.m_varinfo.hsync_len) << " + " << uint(that.m_varinfo.right_margin) << " (" << rdiv(100 * (that.m_varinfo.left_margin + that.m_varinfo.hsync_len + that.m_varinfo.right_margin), that.htotal()) << "%) hblank";
        ostrm << ", " << commas(that.m_varinfo.yres) << " (" << rdiv(100 * that.m_varinfo.yres, that.vtotal()) << "%) yvis + " << commas(that.m_varinfo.upper_margin) << " + " << commas(that.m_varinfo.vsync_len) << " + " << uint(that.m_varinfo.lower_margin) << " (" << rdiv(100 * (that.m_varinfo.upper_margin + that.m_varinfo.vsync_len + that.m_varinfo.lower_margin), that.vtotal()) << "%) vblank";
        ostrm << ", " << commas(that.fps()) << " fps";
        return ostrm << "}";
    }
public: //methods
//    typedef long elapsed_t;
    typedef /*uint64_t*/ uint32_t elapsed_t; //don't need > 32 bits for perf measurement; 20 bits will hold 5 minutes of msec, or 29 bits will hold 5 minutes of usec
    elapsed_t elapsed(bool reset = false) //msec
    {
        struct timespec now;
        clock_gettime(/*CLOCK_REALTIME*/ CLOCK_MONOTONIC, &now);
        elapsed_t retval = (now.tv_sec - m_started.tv_sec) * 1e3; //sec -> msec
        retval += now.tv_nsec / 1e6 - m_started.tv_nsec / 1e6; //nsec -> msec
        if (reset) { m_started = now; m_numfr = 0; }
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
    static const PIXEL BLACK = 0xFF000000; //ARGB
    FB& fill(uint32_t argb = BLACK) { return fill(0, 0, DEFAULT, DEFAULT, argb); }
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
        debug(0, "dump framebuffer to '%s' ...", filename);
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
    void vsync() //bool want_fallback = false)
    {
        static int mm_numfr = 0;
        ++mm_numfr;
#ifndef FBIO_WAITFORVSYNC
 #define FBIO_WAITFORVSYNC  _IOW('F', 0x20, __u32)
#endif
        int arg = 0;
//        if (fbfd < 0) return -1;
        if (ioctl(m_fd, FBIO_WAITFORVSYNC, &arg) < 0)
        {
#ifdef __ARMEL__ //RPi //__arm__
            /*if (!want_fallback)*/ throw std::runtime_error(strerror(errno)); //let caller deal with it
#else //PC
//            return false;
//try to simultate desired timing (not as critical on dev machine):
            int delay_msec = 1000 * m_numfr / fps() - elapsed();
            if (mm_numfr < 5) exc_soft("ioctl vsync fr#[%s] failed, simulate %s fps with sleep %s msec", commas(m_numfr), commas(fps()), commas(delay_msec));
            if ((delay_msec > 0) && (delay_msec <= 250)) usleep(1000 * delay_msec);
#endif
        }
//        return true;
    }
//cleanup:
    void close()
    {
        if (m_fbp != (PIXEL*)-1) munmap(m_fbp, fblen());
        m_fbp = (PIXEL*)-1;
        if (m_cfg_dirty)
            if (ioctl(m_fd, FBIOPUT_VSCREENINFO, &m_varinfo) < 0)
                exc_hard("Error re-setting variable information");
        m_cfg_dirty = false;
        if (m_fd != -1) ::close(m_fd);
        m_fd = -1;
    }
//protected: //data members
public: //kludge: allow wrapper to reuse
    int m_numfr;
    struct timespec m_started;
//    const elapsed_t m_started;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
};


/////////////////////////////////////////////////////////////////////////////////
////
/// Utilities/helpers:
//

/*typedef*/ struct FB_Size //no: public SDL_Point
{
//won't support mult inh:    int& w = x; int& h = y; //kludge: rename parent's members, but still allow down-cast
    int w, h;
//    explicit SDL_Size(): w(0), h(0) {} //default ctor
    explicit inline FB_Size(int neww = 0, int newh = 0): w(neww), h(newh) {} //default ctor
//    explicit inline FB_Size(FB_Size that): w(that.w), h(that.h) {} //copy ctor for temps
    explicit inline FB_Size(const FB_Size& that): w(that.w), h(that.h) {} //copy ctor
//operators:
//    inline bool isnull() const { return !this; } //kludge: try to fix null ptr check for gcc on RPi
//operator overload syntax/semantics: https://en.cppreference.com/w/cpp/language/operators
//    inline bool operator==(CONST mySDL_Size that) { return operator==(that); }
    inline bool operator==(const FB_Size& that) const //lhs, const mySDL_Size& rhs)
    {
//    mySDL_Size& lhs = *this;
//    if (!&lhs || !&rhs) return (&lhs == &rhs); //handles NO_SIZE
//    return (lhs.w == rhs.w) && (lhs.h == rhs.h);
        return (w == that.w) && (h == that.h);
    }
    inline bool operator!=(const FB_Size& that) const { return !(*this == that); } //lhs, const mySDL_Size& rhs) { return !(lhs == rhs); }
    FB_Size& operator=(const FB_Size& that) { w = that.w; h = that.h; return *this; }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FB_Size& that) CONST
    {
#if 0
        char buf[32];
        snprintf(buf, sizeof(buf), "%p", &that);
        uintptr_t that_addr = (uintptr_t)&that;
printf("&data %p, %lx, %lx, ", &that, &that, that_addr);
printf("null? %d, %d, %d, %d, %d, %d, %d\n", !strcmp(buf, "(nil)"), that.isnull(), !&that, &that == 0, &that == nullptr, &that == NULL, &that == (mySDL_Size*)NULL);
fflush(stdout);
//CAUTION: something is really messed on in gcc on RPi! *none* of the above checks for null work except the strcmp()
//BROKEN on RPi        if (!&that) return ostrm << "NO_SIZE";
//        if (that.isnull()) return ostrm << "NO_SIZE";
#endif
        if (!&that) return ostrm << "NO_SIZE"; //CAUTION: BROKEN on RPi
        ostrm << commas(that.w) << " x " << commas(that.h);
        if (that.w && that.h) ostrm << " = " << commas(that.w * that.h);
        return ostrm;
    }
//utility/helper methods:
//    template <typename DATATYPE=FB::PIXEL> //uint32_t>
//    inline size_t datalen() const { return w * h * sizeof(DATATYPE); }
}; //mySDL_Size;
//const SDL_Size& NO_SIZE = *(SDL_Size*)0; //&NO_SIZE == 0
//SDL_Size NO_SIZE; //dummy value (settable)
#define NO_SIZE  (FB_Size*)NULL


struct FB_Rect { int x, y, w, h; };
#define NO_RECT  (FB_Rect*)NULL


//typedef /*uint64_t*/ /*uint32_t*/ FB::elapsed_t elapsed_t; //don't need > 32 bits for perf measurement; 20 bits will hold 5 minutes of msec, or 29 bits will hold 5 minutes of usec
#define NO_PERF  (FB::elapsed_t*)NULL
#define NO_XFR  NULL


#if 1
//kludge: wrapper to avoid trailing static decl at global scope:
//kludge: use "..." to allow "," within INIT; still can't use "," within TYPE, though
#define STATIC_WRAP(TYPE, VAR, /*INIT*/ ...)  \
    /*static*/ inline TYPE& static_##VAR() \
    { \
        static TYPE m_##VAR /*=*/ /*INIT*/ __VA_ARGS__; \
        return m_##VAR; \
    }; \
    TYPE& VAR = static_##VAR() //kludge-2: create ref to avoid the need for "()"
#else
 #include "shmalloc.h"
#endif


//SDL_Texture ptr auto-cleanup wrapper:
//includes a few factory helper methods
//template <bool WantPixelShmbuf = true> //, bool DebugInfo = true>
//using mySDL_AutoTexture_super = std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>; //DRY kludge
template <typename PXTYPE = FB::PIXEL, int EXTRA_STATS = 0, bool FORCE_VSYNC = true> //, typename super = std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>> //2nd arg to help stay DRY; //, super = std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>> //super DRY kludge
class FB_AutoTexture //: public super //mySDL_AutoTexture_super //std::unique_ptr<SDL_Texture, std::function<void(SDL_Texture*)>>
{
    FB m_fb;
    FB_Size m_view;
//    double m_hscale, m_vscale; //stretch txtr to fill entire screen (framebuffer)
public: //data members
    const int& width;
    const int& height;
//    const int width, height;
    typedef PXTYPE PIXEL; //exposed so caller can use it
    enum {CALLER = 0, CPU_TXTR, REND_COPY, REND_PRESENT, NUM_PRESENT, NUM_IDLE, NUM_STATS}; //perf_stats offsets
    /*double*/ FB::elapsed_t perf_stats[NUM_STATS + EXTRA_STATS]; //allow caller to store additional stats here, or to add custom stats
//    static const int NUM_STATS = SIZEOF(perf_stats);
    using wrap_type = std::map<int, const char*>; //kludge: can't use "," in macro param
//kludge: gcc won't allow static member init so wrap in static function/data member:
    static STATIC_WRAP(wrap_type, PerfNames, =
    {
//NOTE: strings should be valid Javascript names
        {/*Enum::*/CALLER, "CALLER"},
        {/*Enum::*/CPU_TXTR, "CPU_TXTR"},
        {/*Enum::*/REND_COPY, "REND_COPY"},
        {/*Enum::*/REND_PRESENT, "REND_PRESENT"},
        {/*Enum::*/NUM_PRESENT, "NUM_PRESENT"},
        {/*Enum::*/NUM_IDLE, "NUM_IDLE"},
        {/*Enum::*/NUM_STATS, "NUM_STATS"}, //caller extras start here
    });
public: //ctor/dtor
    struct NullOkay {}; //ctor disambiguation tag
    explicit FB_AutoTexture(NullOkay): width(m_view.w), height(m_view.h) { debug(20, "empty ctor"); m_view.w = m_view.h = 0; } //: m_started(now()), m_srcline(0) {}
//    explicit FB_AutoTexture(FB_AutoTexture that): FB_AutoTexture(&that.view(), 0, SRCLINE) {} //used by create() retval
    /*explicit*/ FB_AutoTexture(const FB_AutoTexture& that): FB_AutoTexture(&that.m_view, 0, SRCLINE) { debug(20, "copy ctor"); } //delegated; NOTE: needs to be implicit for NAMED create() and create()
    explicit FB_AutoTexture(const FB_Size* view_wh = NO_SIZE, PXTYPE init_color = FB::BLACK, SrcLine srcline = 0): width(m_view.w), height(m_view.h), m_latest(now()), m_started(now()), m_srcline(srcline)
    {
//        m_fb.m_started = now(); m_fb.m_srcline = NVL(srcline, SRCLINE);
//        m_hscale = view_wh? (double)m_fb.width / view_wh->w: 1;
//        m_vscale = view_wh? (double)m_fb.height / view_wh->h: 1;
        debug(20, "ctor view %p " << *view_wh << ATLINE(srcline), view_wh);
        m_view.w = view_wh? view_wh->w: m_fb.width;
        m_view.h = view_wh? view_wh->h: m_fb.height;
        clear(init_color, NVL(srcline, SRCLINE));
        INSPECT(GREEN_MSG << "ctor " << *this << ATLINE(srcline));
    }
    /*virtual*/ ~FB_AutoTexture() { INSPECT(RED_MSG "dtor " << *this << ", lifespan " << (now() - m_started) / 1000 /*m_started, 1000)*/ << " sec" << ATLINE(m_srcline)); }
public: //operators
    FB_AutoTexture& operator=(const FB_AutoTexture& that) //NOTE: FB itself doesn't need to be copied because it's just mmapped
    {
//        m_hscale = that.m_hscale; m_vscale = that.m_vscale;
        m_view = that.m_view;
//        m_fb.m_started = that.m_fb.m_started; m_fb.m_srcline = that.m_fb.m_srcline;
        m_started = that.m_started; m_srcline = that.m_srcline;
        INSPECT("op= " << *this);
        return *this;
    }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FB_AutoTexture& that) //CONST SDL_Window* wnd) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
        ostrm << "FB_AutoTexture" << that.my_templargs; //();
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
        ostrm << ", view " << that.m_view << " => scale " << that.hscale() << " x " << that.vscale() << ", group " << that.hgroup() << " x " << that.vgroup() << " => " << that.m_view.w * that.hgroup() << " x " << that.m_view.h * that.vgroup();
        ostrm << ", fb " << that.m_fb;
        ostrm << "}";
        return ostrm; 
    }
public: //methods
    typedef FB::elapsed_t elapsed_t;
    FB::elapsed_t m_latest; //time of last update (RenderPresent unless caller used this also)
    FB::elapsed_t now() { return m_fb.elapsed(); }
    inline elapsed_t elapsed(bool reset = false) { return m_fb.elapsed(reset); } //msec
    inline FB::elapsed_t perftime() { FB::elapsed_t delta = now() - m_latest; m_latest += delta; return delta; } //latest == now() after this
    inline void clear_stats(FB::elapsed_t* perf = NO_PERF)
    {
        if (!perf) perf = &perf_stats[0];
        perftime(); //flush/reset perf timer
        memset(/*&perf_stats*/ perf, 0, sizeof(perf_stats));
    }
//factory:
//    FB_Size view() const { return FB_Size(m_fb.width / m_hscale, m_fb.height / m_vscale); }
    static FB_AutoTexture/*& not allowed with rval ret; not needed with unique_ptr*/ create(const FB_Size* view_wh = NO_SIZE, PXTYPE init_color = FB::BLACK, SrcLine srcline = 0)
    {
//        debug(0, "view@ %p " << *view_wh, view_wh);
        return FB_AutoTexture(view_wh, init_color, srcline);
    }
//updates:
//clear => screen, fill => pixel buf
    inline void clear(PXTYPE color = FB::BLACK, SrcLine srcline = 0)
    {
        m_fb.fill(color); //, NVL(srcline, SRCLINE)); //pitch);
    }
    static void fill(void* dest, PXTYPE src, size_t len) { fill(dest, &src, len); }
    static void fill(void* dest, const void* src, size_t len) //memcpy-compatible fill, for use with update() above
    {
        PXTYPE* ptr = static_cast<PXTYPE*>(dest);
        while (len--) *ptr++ = *(PXTYPE*)src; //supposedly a loop is faster than an overlapping memcpy
    }
    void idle(FB::elapsed_t* perf = NO_PERF, SrcLine srcline = 0)
    {
        if (!perf) perf = &perf_stats[0];
        ++perf[NUM_IDLE]; //count #render presents; gives max theoretical frame rate (vsynced)
        m_fb.vsync(); //CAUTION: waits up to 1 frame time (~17 msec @60 FPS)
    }
    typedef std::function<void(void* dest, const void* src, size_t len)> XFR; //void* (*XFR)(void* dest, const void* src, size_t len); //NOTE: must match memcpy sig; //decltype(memcpy);
    void update(const PXTYPE* pixels, /*const SDL_Rect* rect = NO_RECT,*/ FB::elapsed_t* perf = NO_PERF, XFR xfr = NO_XFR, /*REFILL refill = NO_REFILL,*/ SrcLine srcline = 0)
    {
        if (!perf) perf = &perf_stats[0];
        perf[CALLER] += perftime(); //time caller spent rendering (sec); could be long (caller determines)
        if (pixels)
        {
            size_t xfrlen = m_view.w * m_view.h * sizeof(PXTYPE); //m_fb.width * m_fb.height / m_hscale / m_vscale;
            if (!xfr) exc_hard("no xfr cb"); //{ xfr = memcpy; xfrlen /= 3; } //kludge: txtr is probably 3x node size so avoid segv
            xfr(&m_fb[0], pixels, xfrlen); //render/pivot into beginning part of framebuf
            stretch(); //stretch to fill entire framebuf
            perf[CPU_TXTR] += perftime(); //1000); //CPU-side data xfr time (msec)
        }
        m_fb.vsync(); //CAUTION: waits up to 1 frame time (~17 msec @60 FPS)
        ++perf[NUM_PRESENT]; //#render presents
        perf[REND_PRESENT] += perftime(); //1000); //vsync wait time (idle time, in msec); should align with fps
    }
protected: //helpers
    double hscale() const { return (double)m_view.w / m_fb.width; }
    double vscale() const { return (double)m_view.h / m_fb.height; }
    int hgroup() const { return rdiv(m_fb.width, m_view.w); } //NOTE: a little x overscan is okay (want 1/3 pixel h overscan)
    int vgroup() const { return rdiv(m_fb.height, m_view.h); } //don't want v overscan (only affects univ tails so probably doesn't matter)
    void stretch()
    {
//        if (m_vscale == 1) //horizontal stretch only
//        FB::PIXEL* pbp = &m_fb[0];
//            for (int x = m_fb.width; x > 0; --x)
//                int viewx = x / m_hscale;
//                if (viewy != y) memcpy(&m_fb[m_fb.xy()]
//        double hreduce = hscale(), vreduce = vscale();
        FB::PIXEL* framep = &m_fb[0];
        if (!m_view.w || !m_view.h) return;
static int count = 0;
bool want_debug = !count++;
        size_t rowlen = m_fb.rowlen(); //sizeof(FB::PIXEL)); //row len (quad-bytes); CAUTION: might != fb.width due to padding
        int rptx = hgroup(), rpty = vgroup(); //rdiv(m_fb.width, m_view.w), rpty = m_fb.height / m_view.h; //NOTE: a little x overscan is okay (want 1/3 pixel h overscan, no v overscan)
        if (want_debug) debug(50, "stretch: rowlen %d qb, view (%d, %d) -> screen (%d, %d) = scale (%4.3f, %4.3f), rpt (%d, %d) -> padded scr (%d, %d)",
            rowlen, m_view.w, m_view.h, m_fb.width, m_fb.height, hscale(), vscale(), rptx, rpty, m_view.w * rptx, m_view.h * rpty);
#if 1
//CAUTION: caller's pixel buf is xy packed; use linear addresses to unpack in reverse order to allow overlap
        for (int desty = (m_view.h - 1) * rpty, srcxy = m_view.h * m_view.w - 1; desty >= 0; desty -= rpty)
        {
//            if (want_debug) debug(0, "row[%d] loop? %d", desty / rpty, desty < m_view.h);
            FB::PIXEL* rowp = framep + desty * rowlen; //&m_fb[m_fb.xy(0, fromy)];
            if (desty < m_fb.height) //vertical clip
            for (int destx = (m_view.w - 1) * rptx; destx >= 0; destx -= rptx, --srcxy)
            {
//                if (want_debug) debug(0, "col[%d] loop? %d", destx / rptx, destx < m_view.w);
//                if (want_debug) debug(0, "destx %s, desty %s, srcxy %s, rowlen %s, framep %s, rowp %s ='%s, copy %s..%s to %s..%s", 
//                    commas(destx), commas(desty), commas(srcxy), commas(rowlen), commas(framep), commas(rowp), commas(desty * rowlen),
//                    commas(srcxy), commas(srcxy - (m_view.w - 1) * rptx), commas(rowp - framep + destx +i), commas());
                if (want_debug) debug(50, "col[%s]? %d, copy [%s] to %s..%s", commas(destx / rptx), destx < m_fb.width,
                    commas(srcxy), commas(desty * rowlen + destx), commas(desty * rowlen + destx + std::min(rptx, (int)m_fb.width - destx) - 1));
                for (int i = 0; (i < rptx) && (i < m_fb.width - destx); ++i) rowp[destx + i] = framep[srcxy]; //horizontal stretch
            }
            if (want_debug) debug(50, "row[%s]? %d, copy [%s] to %s..%s", commas(desty / rpty), desty < m_fb.height,
                commas(desty * rowlen), commas((desty + 1) * rowlen), commas((desty + std::min(rpty, (int)m_fb.height - desty) - 1) * rowlen));
            for (int i = 1; (i < rpty) && (i < m_fb.height - desty); ++i) //skip first row (already unpacked); NOTE: this loop only applies when rpty (ie, vgroup) > 1
            {
//                if (want_debug) debug(0, "destx %s, desty %s, srcxy %s, rowlen %s, framep %s, rowp %s ='%s", commas(destx), commas(desty), commas(srcxy), commas(rowlen), commas(framep), commas(rowp), commas(desty * rowlen));
                memcpy(framep + (desty + i) * rowlen, rowp, rowlen * sizeof(*framep)); //vertical stretch
            }
        }
#endif
#if 0 //slow
        for (int y = m_fb.height; y > 0; --y)
            for (int x = m_fb.width; x > 0; --x)
//                int firstx = (x - 1) * m_hscale, lastx = x * m_hscale;
//                for (int i = firstx; i < lastx; ++i)
            {
                m_fb[m_fb.xy((x - 1) * m_view.w / m_fb.width, (y - 1) * m_view.h / m_fb.height)] = m_fb[m_fb.xy(x - 1, y - 1)];
if (want_debug)
    if ((x == 1) || (y == 1) || (x == m_fb.width) || (y == m_fb.height)) debug(0, "stretch: [%d, %d] <- [%d, %d] = 0x%x", (x - 1) * m_view.w / m_fb.width, (y - 1) * m_view.h / m_fb.height, x - 1, y - 1);
            }
#endif
    }
public: //named arg variants
    template <typename CALLBACK>
    inline static /*auto*/ FB_AutoTexture create(CALLBACK&& named_params)
    {
//        struct CreateParams params;
//        return create(unpack(create_params, named_params), Unpacked{});
        struct //CreateParams
        {
//            CONST SDL_Window* wnd = NO_WINDOW; //(SDL_Window*)-1; //special value to create new window
  //          int w = 0, h = 0;
            const FB_Size* wh = NO_SIZE; //not used
            const FB_Size* view_wh = NO_SIZE;
//            int w_padded = 0;
//            int& w = size.w; //allor caller to set individually as well
//            int& h = size.h;
//            SDL_Format fmt = NO_FORMAT;
//            SDL_TextureAccess access = NO_ACCESS;
            int screen = 0; //FIRST_SCREEN; //not used
            PXTYPE init_color = FB::BLACK;
            SrcLine srcline = 0;
        } params;
        FB_Size sv_wh;
        unpack(params, named_params);
        if (params.view_wh) { sv_wh = *params.view_wh; params.view_wh = &sv_wh; } //kludge: copy wh in case callee used a temp
//        debug(0, "view@ %p " << *params.view_wh, params.view_wh);
        return create(params.view_wh, params.init_color, params.srcline);
    }
    template <typename CALLBACK>
    inline auto update(CALLBACK&& named_params)
    {
//        struct UpdateParams params;
//        VOID update(unpack(update_params, named_params), Unpacked{});
        struct //UpdateParams
        {
            /*const*/ PXTYPE* pixels = 0; //no default unless WantPixelShmbuf
//            const SDL_Rect rect; //= NO_RECT;
            const FB_Rect* rect = NO_RECT; //not used
//            int& w = size.w; //allor caller to set individually as well
//            int& h = size.h;
//            int pitch = NO_PITCH;
            FB::elapsed_t* perf = NO_PERF;
            XFR xfr = NO_XFR; //memcpy;
//            REFILL refill = NO_REFILL; //memcpy;
            SrcLine srcline = 0;
        } params;
//printf("here20\n"); fflush(stdout);
        FB_Rect sv_rect;
        unpack(params, named_params);
        if (params.rect) { sv_rect = *params.rect; params.rect = &sv_rect; } //kludge: copy wh in case callee used a temp
//printf("here21\n"); fflush(stdout);
        VOID update(params.pixels, params.perf, params.xfr, params.srcline);
//printf("here22\n"); fflush(stdout);
    }
protected:
    static STATIC_WRAP(std::string, my_templargs, = TEMPL_ARGS);
    FB::elapsed_t m_started;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
};


//accept variable #3 macro args:
#ifndef UPTO_4ARGS
 #define UPTO_4ARGS(one, two, three, four, five, ...)  five
#endif
//#ifndef UPTO_3ARGS
// #define UPTO_3ARGS(one, two, three, four, ...)  four
//#endif


////////////////////////////////////////////////////////////////////////////////
////
/// SDL shims:
//

typedef uint32_t Uint32;
#define SDL_Size  FB_Size
#define SDL_AutoTexture  FB_AutoTexture
#define SDL_QuitRequested()  false
#define FIRST_SCREEN  0 //ignored


struct FB_DisplayMode
{
    FB_Size wh;
    FB_Rect bounds; //= {0, 0, 0, 0};
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FB_DisplayMode& that) CONST
    {
//        ostrm << "{" << dispmode.num_disp << " disp" << plural(dispmode.num_disp) << ", ";
//        ostrm << dispmode.which_disp << ": ";
//        ostrm << dispmode.bounds << ", "; //SDL_Size(dispmode.w, dispmode.h) << ", "; //dispmode.w << " x " << dispmode.h << ", ";
//        ostrm << dispmode.refresh_rate << " Hz, ";
//        ostrm << "fmt " << /*(SDL_format)*/SDL_Format(dispmode.format);
//        ostrm << "}";
        return ostrm << that.wh;
    }
//ctors/dtors:
    FB_DisplayMode()
    {
        FB fb;
        bounds.x = bounds.y = 0;
        bounds.w = wh.w = fb.width;
        bounds.h = wh.h = fb.height;
    }
}; //mySDL_DisplayMode;
#define SDL_DisplayMode  FB_DisplayMode //use my def instead of SDL def

const FB_DisplayMode* ScreenInfo(int screen = FIRST_SCREEN, SrcLine srcline = 0)
{
    static FB_DisplayMode dm;
    return &dm;
}


//scale factors (for readability):
//base time = msec
//CAUTION: may need () depending on surroundings
#define usec  /1000
#define msec  *1
#define sec  *1000

//TODO: get from elapsed.h
typedef FB::elapsed_t elapsed_t;
//int64_t now_msec()
//{
//    using namespace std::chrono;
//    return /*std::chrono::*/duration_cast</*std::chrono::*/milliseconds>(/*std::chrono::*/system_clock::now().time_since_epoch()).count();
//}
//from https://stackoverflow.com/questions/19555121/how-to-get-current-timestamp-in-milliseconds-since-1970-just-the-way-java-gets
#if 0
int64_t now() //msec
{
    using namespace std::chrono;
    return /*std::chrono::*/duration_cast</*std::chrono::*/milliseconds>(/*std::chrono::*/system_clock::now().time_since_epoch()).count();
}
//elapsed_t now()
//{
//    struct timespec now;
//    clock_gettime(/*CLOCK_REALTIME*/ CLOCK_MONOTONIC, &now);
//    elapsed_t retval = (now.tv_sec - m_started.tv_sec) * 1e3; //sec -> msec
//    retval += now.tv_nsec / 1e6 - m_started.tv_nsec / 1e6; //nsec -> msec
//    if (reset) { m_started = now; m_numfr = 0; }
//    return retval;
//}

inline elapsed_t elapsed(const elapsed_t& started) //, int scaled = 1) //Freq = #ticks/second
{
    elapsed_t delta = now() - started; //nsec
//    started += delta; //reset to now() each time called
    return /*scaled? (double)delta * scaled / SDL_TickFreq():*/ delta; //return actual time vs. #ticks
}
#endif

void SDL_Delay(int delay_msec)
{
    usleep(delay_msec * 1e3);
}

int SDL_TickFreq()
{
    return 1e3;
}

#endif //ndef _FB_HELPERS_H


////////////////////////////////////////////////////////////////////////////////
////
/// unit test:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

//#include <iostream> //std::cout
//#include <unistd.h> //sleep(), usleep()
//#include <cstdint> //uint32_t
//#include <stdio.h> //printf()
//#include <sstream> //std::ostringstream
//#include <fcntl.h> //O_RDWR
//#include <linux/fb.h> //struct fb_var_screeninfo, fb_fix_screeninfo
//#include <sys/mman.h> //mmap()
//#include <sys/ioctl.h> //ioctl()
//#include <memory.h> //memmove()
//#include <string.h> //snprintf()
//#include <algorithm> //std::min<>(), std::max<>()
//#include <ctype.h> //isxdigit()
//#include <sys/stat.h> //struct stat

//#include "msgcolors.h" //*_MSG, ENDCOLOR_*
//#include "debugexc.h"
#include "logging.h"

#include "fb-helpers.h"
#include "rgb-helpers.h" //must come after fb-helpers.h

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


#if 1 //reuse sdl-helpers.h test for FB
//based on example code at https://wiki.libsdl.org/MigrationGuide
//using "fully rendered frames" style
void wrapper_api_test(ARGS& args)
{
    using TXTR = SDL_AutoTexture<FB::PIXEL, 0, true>; //false>;
//    SDL_AutoTexture<> txtr(SDL_AutoTexture</*true*/>::NullOkay{});
    TXTR txtr(TXTR::NullOkay{});

    int screen = 0; //default
//    const int W = 4, H = 5;
    const bool full = false; //true;
    const int W = 24, H = full? 1111: 1111/50;
    const SDL_Size wh(W, H); //int W = 4, H = 5; //W = 3 * 24, H = 32; //1111;

    for (auto arg : args) //int i = 0; i < args.size(); ++i)
        if (!arg.find("-s")) screen = atoi(arg.substr(2).c_str());
    debug(0, PINK_MSG << /*timestamp() <<*/ "fullscreen[" << screen << "] " << wh << " test start");
    SDL_Size view(3 * W, H);
    auto other_txtr(TXTR::create(NAMED{ _.wh = &wh; _.view_wh = &view; _.screen = screen; SRCLINE; }));
    txtr = other_txtr;
    VOID txtr.clear(mixARGB(0.75, BLACK, WHITE), SRCLINE); //gray; bypass txtr and go direct to window
    {
        DebugInOut("frame rate test");
        txtr.clear_stats(); //txtr.perftime(); //flush perf timer
        for (int i = 0; i < 300; ++i) //5 sec @60 fps, 10 sec @30 fps
            VOID txtr.idle(NO_PERF, SRCLINE); //no updated, just vsynced for timing
    }
    TXTR::elapsed_t duration_msec = txtr.perftime();
    debug(0, CYAN_MSG "max frame rate (synced): %d fr / %4.3f s => %4.3f fps", txtr.perf_stats[SDL_AutoTexture<>::NUM_IDLE], (double)duration_msec / 1000, (double)1000 * txtr.perf_stats[SDL_AutoTexture<>::NUM_IDLE] / duration_msec); //txtr.perftime_msec());

//primary color test:
//    int numfr = 0;
//    elapsed_t perf_stats[SIZEOF(txtr.perf_stats) + 1]; //, total_stats[SIZEOF(perf_stats)] = {0};
    auto show_stats = [/*&perf_stats, &numfr*/&txtr](const char* msg_color = BLUE_MSG, SrcLine srcline = 0)
    {
        static int count = 0;
        const int numfr = txtr.perf_stats[SDL_AutoTexture<>::NUM_PRESENT];
        if (!count++) debug(0, CYAN_MSG "perf: [caller, txtr bb, txtr xfr, upd+vsync]"); //, #sampl]"); //show legend
        debug(0, msg_color << "perf: [%4.3f s, %4.3f ms, %4.3f ms, %4.3f ms]" << ATLINE(srcline), numfr? txtr.perf_stats[SDL_AutoTexture<>::CALLER] / numfr / 1e6: 0, numfr? txtr.perf_stats[SDL_AutoTexture<>::CPU_TXTR] / numfr / 1e3: 0, numfr? txtr.perf_stats[SDL_AutoTexture<>::REND_COPY] / numfr / 1e3: 0, numfr? txtr.perf_stats[SDL_AutoTexture<>::REND_PRESENT] / numfr / 1e3: 0); //, txtr.perf_stats[SDL_AutoTexture<>::NUM_PRESENT]); //, numfr? perf_stats[4] / numfr / 1e3: 0);
    };

    TXTR::PIXEL myPixels[H][W]; //NOTE: pixels are adjacent on inner dimension since texture is sent to GPU row by row
    const TXTR::PIXEL palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        VOID SDL_Delay(1 sec); //at top of loop for caller time consistency
        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
        SDL_AutoTexture<>::fill(&myPixels[0][0], palette[c], wh.w * wh.h); //for (int i = 0; i < wh.w * wh.h; ++i) (&myPixels[0][0])[i] = palette[c]; //(i & 1)? BLACK: palette[c]; //asRGBA(PINK);
        VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; _.xfr = memcpy; /*_.perf = &perf_stats[1];*/ SRCLINE; }); //, true, SRCLINE); //, sizeof(myPixels[0]); //W * sizeof (Uint32)); //no rect, pitch = row length
        if (c) show_stats(BLUE_MSG, SRCLINE);
        if (!c) txtr.clear_stats(); //exclude first iteration in case it has extra startup time; //txtr.perftime(); //flush perf timer
    }
    show_stats(CYAN_MSG, SRCLINE);

//pixel test:
    SDL_AutoTexture<>::fill(&myPixels[0][0], BLACK, wh.w * wh.h); //for (int i = 0; i < wh.w * wh.h; ++i) (&myPixels[0][0])[i] = BLACK; //bypass compiler index limits
    txtr.clear_stats(); //txtr.perftime(); //flush perf timer
    for (int y = !full? std::max(wh.h-5, 0): 0, c = 0; y < wh.h; ++y) //fill in GPU xfr order (for debug/test only)
        for (int x = !full? std::max(wh.w-5, 0): 0; x < wh.w; ++x, ++c)
        {
            if (!full) VOID SDL_Delay(0.25 sec); //at top of loop for caller time consistency
            if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
            myPixels[y][x] = palette[c % SIZEOF(palette)]; //NOTE: inner dimension = X due to order of GPU data xfr
            VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; _.xfr = memcpy; /*_.perf = &perf_stats[1];*/ SRCLINE; }); //, true, SRCLINE); //W * sizeof (Uint32)); //no rect, pitch = row length
            if (!(c % 100)) show_stats(BLUE_MSG, SRCLINE);
        }
    show_stats(CYAN_MSG, SRCLINE);
}
#endif


//check screen addressing:
int calibrate()
{
    using TXTR = SDL_AutoTexture<>; //FB::PIXEL, 0, true>; //false>;
//    SDL_AutoTexture<> txtr(SDL_AutoTexture</*true*/>::NullOkay{});
    TXTR txtr(TXTR::NullOkay{});

    const int W = 24, H = 1111/20;
//    const int W = 4, H = 4;
//    const SDL_Size view(W, H);
    txtr = TXTR::create(NAMED{ SDL_Size view(W, H); debug(0, "view " << view); _.view_wh = &view; SRCLINE; });
//    VOID txtr.clear(mixARGB(0.5, BLACK, WHITE), SRCLINE); //gray; bypass txtr and go direct to window

//NOTE: pixel array dims will be as requested; no-pixel array dimensions might be different than requested; use actual
    TXTR::PIXEL myPixels[H][W]; //NOTE: pixels are adjacent on inner dimension since texture is sent to GPU row by row
    TXTR::fill(&myPixels[0][0], mixARGB(0.5, BLACK, WHITE), W * H); //txtr.width * txtr.height); //for (int i = 0; i < wh.w * wh.h; ++i) (&myPixels[0][0])[i] = palette[c]; //(i & 1)? BLACK: palette[c]; //asRGBA(PINK);
    SDL_Delay(2 sec);

    myPixels[0][0] = RED;
    VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; _.xfr = memcpy; SRCLINE; });
    debug(0, "red (%d, %d)", 0, 0);
    SDL_Delay(2 sec);

    myPixels[0][W - 1] = GREEN;
    VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; _.xfr = memcpy; SRCLINE; });
    debug(0, "green (%d, %d)", 0, W - 1);
    SDL_Delay(2 sec);

    myPixels[H - 1][W - 1] = YELLOW;
    VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; _.xfr = memcpy; SRCLINE; });
    debug(0, "yellow (%d, %d)", H - 1, W - 1);
    SDL_Delay(2 sec);

    myPixels[H - 1][0] = BLUE;
    VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; _.xfr = memcpy; SRCLINE; });
    debug(0, "blue (%d, %d)", H - 1, 0);
    SDL_Delay(2 sec);

    txtr.elapsed(true); //reset timer/frame count
    TXTR::fill(&myPixels[0][0], mixARGB(0.5, BLACK, WHITE), W * H); //txtr.width * txtr.height); //for (int i = 0; i < wh.w * wh.h; ++i) (&myPixels[0][0])[i] = palette[c]; //(i & 1)? BLACK: palette[c]; //asRGBA(PINK);
    const TXTR::PIXEL palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    for (int y = 0, c = 0; y < H; ++y) //fill in GPU xfr order (for debug/test only)
        for (int x = 0; x < W; ++x, ++c)
        {
            myPixels[y][x] = palette[c % SIZEOF(palette)]; //NOTE: inner dimension = X due to order of GPU data xfr
            VOID txtr.update(NAMED{ _.pixels = &myPixels[0][0]; _.xfr = memcpy; SRCLINE; });
//            VOID SDL_Delay(0.25 sec); //at top of loop for caller time consistency
            if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
            if (!(c % 50)) debug(0, "#fr %d/%d", c, H * W);
        }
}


//frame-rate test:
int frame_test() //FB& fb) //(*vsync(void)))
{
	const FB::PIXEL palette[] = {RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE}; //RGSWAP(RED), RGSWAP(GREEN), BLUE, YELLOW, RGSWAP(CYAN), RGSWAP(MAGENTA), WHITE};
//for (int i = 0; i < SIZEOF(colors); ++i) printf("color[%d/%d]: 0x%x\n", i, SIZEOF(colors), colors[i]);
//	const int scrsize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
//    const int pitch32 = finfo.line_length / sizeof(uint32_t);
//    const int num_pixels = vinfo.xres * vinfo.yres;
//    uint32_t* const fb32p = (uint32_t*)fbp;
    FB fb;
//    fb.dump("before.dat");
//    printf("sleep(5) @%d\n", __LINE__);
//    sleep(5);
    int numfr = 0;
    const int RECTSIZE = 64; //32; //16
//    for (int i = 0; i < fb.num_pixels(); ++i) fb[i] = BLACK;
    fb.fill(BLACK);
    for (int loop = 0; loop < 10; ++loop) //10; ++loop)
    {
        FB::PIXEL color = palette[loop % SIZEOF(palette)];
//#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ //ARM (RPi)
//        color = ARGB2ABGR(color); //R <-> B swap on RPi
//#endif
        for (int y = 0; y < fb.height; y += RECTSIZE)
	        for (int b = 0; b < fb.width; b += RECTSIZE)
            {
//                fb[fb.xy(b, y)] = palette[loop % SIZEOF(palette)];
//                if (!(numfr % 100))
                fb.fill(b, y, RECTSIZE, RECTSIZE, color);
//                if (!(b % 100)) printf("numfr %d\n", numfr);
//                if (b & 0xF) continue;
//                sleep(1); //sec
                fb.vsync();
                ++numfr;
                if (!(numfr % 120)) debug(0, "#fr %d/%d", numfr, fb.height * fb.width);
            }
    }
    // now this is about the same as 'fbp[pix_offset] = value'
//    *((char*)(fbp + pix_offset)) = b;
//    *((char*)(fbp + pix_offset + 1)) = g;
//    *((char*)(fbp + pix_offset + 2)) = r;
//    *((char*)(fbp + pix_offset + 3)) = 0xff;
    debug(0, "%d fr of %d px in %d ms\n", numfr, fb.num_pixels(), fb.elapsed());
    debug(0, "%s frames of %s pixels in %s msec = %s fps", commas(numfr), commas(fb.num_pixels()), commas(fb.elapsed()), commas((double)1000 * numfr / fb.elapsed()));
}


// application entry point
//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    debug(0, "sz(int) %d, sz(ui32) %d, sz(*) %d, sz(ui64) %d", sizeof(int), sizeof(FB::PIXEL), sizeof(void*), sizeof(uint64_t));

//    frame_test();
    calibrate();
//    wrapper_api_test(args);
//    fb.dump("after.dat");
    debug(0, "done");
//    return 0; 
}

#endif //def WANT_UNIT_TEST

//eof