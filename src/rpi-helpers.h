////////////////////////////////////////////////////////////////////////////////
////
/// RPi helpers (runnable on non-RPi machines):
//

#if !defined(_RPI_HELPERS_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _RPI_HELPERS_H //CAUTION: put this before defs to prevent loop on cyclic #includes

//NOTE from https://stackoverflow.com/questions/1277627/overhead-of-pthread-mutexes
//about atomic: In practice, you can assume that int and other integer types no longer than int are atomic. You can also assume that pointer types are atomic
// http://axisofeval.blogspot.com/2010/11/numbers-everybody-should-know.html
//go ahead and use std::atomic<> anyway, for safety:
#include <atomic> //std::atomic. std::memory_order
#include <sys/stat.h> //struct stat
#include <stdint.h> //uint*_t
#include <ostream> //std::ostream
#include <memory> //std::unique_ptr<>
#include <linux/fb.h> //struct fb_var_screeninfo
//#include <stdio.h>
#include <fcntl.h> //open(), O_RDONLY
#include <unistd.h> //close()
#include <sys/ioctl.h> //ioctl()

#include "debugexc.h" //debug()
#include "srcline.h" //SrcLine, SRCLINE
#include "ostrfmt.h" //FMT()


#ifndef STATIC
 #define STATIC //dummy keyword for readability
#endif

#ifndef CONST
 #define CONST //dummy keyword for readability
#endif

//accept up to 2 macro args:
#ifndef UPTO_2ARGS
 #define UPTO_2ARGS(one, two, three, ...)  three
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// check for host:
//

const int CFG_LEVEL = 65;

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
    static std::atomic<tristate> cached(tristate::Maybe); //NOTE: doesn't need to be thread_local; only one (bkg) thread should be calling this
//    static std::mutex protect;
//    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

//    myprintf(3, BLUE_MSG "isRPi()" ENDCOLOR);
//    serialize.lock(); //not really needed (low freq api), but just in case
    if (cached == tristate::Maybe) cached = exists("/boot/config.txt")? tristate::Yes: tristate::No;
//    serialize.unlock();
    return (cached == tristate::Yes);
}


///////////////////////////////////////////////////////////////////////////////
////
/// Get video info:
//

//helpful background info: https://elinux.org/RPI_vcgencmd_usage
//vcgencmd measure_clock pixel, h264, hdmi, dpi
//vcgencmd get_mem gpu
//vcgencmd version
//vcgencmd hdmi_timings  #https://www.raspberrypi.org/documentation/configuration/config-txt/video.md

//central timing parameters:
//xres + yres (including front porch + sync + back porch) and pixel clock determine all other timing
/*typedef*/ struct ScreenConfig
{
    int screen = -1; //which screen (if multiple monitors)
//    double rowtime; //= (double)scfg->mode_line.htotal / scfg->dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
//    double frametime; //= (double)scfg->mode_line.htotal * scfg->mode_line.vtotal / scfg->dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
//    int xres; //retval->Set(JS_STR(iso, "xres"), JS_INT(iso, scfg->mode_line.hdisplay)); //vinfo.xres));
//    int yres; //retval->Set(JS_STR(iso, "yres"), JS_INT(iso, scfg->mode_line.vdisplay)); //vinfo.yres));
//??        retval->Set(JS_STR(iso, "bpp"), JS_INT(iso, vinfo.bits_per_pixel));
//        retval->Set(JS_STR(iso, "linelen"), JS_INT(iso, finfo.line_length));
//    float pixclock_MHz; //retval->Set(JS_STR(iso, "pixclock_MHz"), JS_FLOAT(iso, (double)scfg->dot_clock / 1000)); //MHz //vinfo.pixclock));
    int dot_clock;
//    int hblank; //retval->Set(JS_STR(iso, "hblank"), JS_INT(iso, scfg->mode_line.htotal - scfg->mode_line.hdisplay)); //hblank));
//    int vblank; //retval->Set(JS_STR(iso, "vblank"), JS_INT(iso, scfg->mode_line.vtotal - scfg->mode_line.vdisplay)); //vblank));
//    float rowtime_usec; //retval->Set(JS_STR(iso, "rowtime_usec"), JS_FLOAT(iso, 1000000 * rowtime));
//    float frametime_msec; //retval->Set(JS_STR(iso, "frametime_msec"), JS_FLOAT(iso, 1000 * frametime));
//    float fps; //retval->Set(JS_STR(iso, "fps"), JS_FLOAT(iso, 1 / frametime));
    int hdisplay, hlead, hsync, htrail, htotal; //hcount[4]; //hdisplay + front porch + hsync + back porch = htotal
    int vdisplay, vlead, vsync, vtrail, vtotal; //vcount[4]; //vdisplay + front porch + vsync + back porch = vtotal
    int aspect_ratio; //configured, not calculated
    int frame_rate; //configured, not calculated
//calculated fields:
    double aspect(SrcLine srcline = 0) const { isvalid(srcline); return (double)htotal / vtotal; }
    double row_time(SrcLine srcline = 0) const { isvalid(srcline); return (double)htotal / dot_clock / 1000; } //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
    double frame_time(SrcLine srcline = 0) const { isvalid(srcline); return (double)htotal * vtotal / dot_clock / 1000; } //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
    double fps(SrcLine srcline = 0) const { return (double)1 / frame_time(srcline); } //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
//check for null ptr (can happen with dynamically allocated memory):
    void isvalid(SrcLine srcline = 0) const { if (!this) exc_hard("can't get screen config" << ATLINE(srcline)); }
//operators:
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const ScreenConfig& that) CONST
    {
//    std::ostringstream ss;
//    if (!rect) ss << "all";
//    else ss << (rect->w * rect->h) << " ([" << rect->x << ", " << rect->y << "]..[+" << rect->w << ", +" << rect->h << "])";
//    return ss.str();
//    ostrm << "SDL_Rect";
        if (!&that) { ostrm << "{NULL}"; return ostrm; } //failed to load
        ostrm << "{screen# " << that.screen;
        ostrm << ", " << (that.dot_clock / 1e3) << " Mhz";
        ostrm << ", hres " << that.hdisplay << " + " << that.hlead << "+" << that.hsync << "+" << that.htrail << " = " << that.htotal;
        ostrm << ", vres " << that.vdisplay << " + " << that.vlead << "+" << that.vsync << "+" << that.vtrail << " = " << that.vtotal;
        ostrm << ", aspect " << that.aspect_ratio << FMT(" (config) %4.3f (actual)") << that.aspect();
        ostrm << FMT(", row %4.3f usec") << (that.row_time() * 1e6);
        ostrm << FMT(", frame %4.3f msec") << (that.frame_time() * 1e3);
        ostrm << ", fps " << that.frame_rate << FMT(" (config) %4.3f (actual)") << that.fps();
        ostrm << "}";
        return ostrm;
    }
}; //ScreenConfig;


#if 1
#ifdef RPI_NO_X
// #include "bcm_host.h"
// #include <iostream> 
 #include <fstream> //std::ifstream, std::getline()
 #include <cstdio> //sscanf()
 #include <string>
// #pragma message("RPi, no X")

//example read text file line-by-line: https://stackoverflow.com/questions/13035674/how-to-read-line-by-line-or-a-whole-text-file-at-once

#if 0
 typedef struct
 {
    int dot_clock;
//     XF86VidModeModeLine mode_line;
    struct
    {
        int hdisplay;
        int vdisplay;
//vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
        int hsyncstart, hsyncend, htotal; //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
        int vsyncstart, vsyncend, vtotal; //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
    } mode_line;
 } ScreenConfig;
#endif

//kludge: for now read it from config file
//TODO: read actual values from memory
 bool read_config(int which, ScreenConfig* cfg, SrcLine srcline = 0)
 {
//    cfg->screen = -1;
    if (which) { exc_soft("TODO: get screen#%d config" << ATLINE(srcline), which); return false; }
    int lines = 0;
    std::string str; 
    std::ifstream file("/boot/config.txt"); //TODO: read from memory; config file could have multiple (conditional) entries
    while (std::getline(file, str))
    {
        if (!lines++) str = "hdmi_timings=1488 0 12 12 24   1104 0 12 12 24    0 0 0 30 0 50000000 1"; //DEV ONLY
//printf("got line: '%s'\n", str.c_str()); fflush(stdout);
//https://www.raspberrypi.org/documentation/configuration/config-txt/video.md
//hdmi_ignore_edid=0xa5000080
//dpi_group=2
//dpi_mode=87
//hdmi_timings=1488 0 12 12 24   1104 0 12 12 24    0 0 0 30 0 50000000 1
//#? hdmi_drive=4
//#? hdmi_ignore_cec_init=1
//#x dpi_output_format=0x6f007
//####dpi_output_format=0x117
//dpi_output_format=7
//#? sdtv_aspect=1

//#dtdebug=on
//# dtoverlay=vga666		(this one might also work)
//dtoverlay=dpi24
//enable_dpi_lcd=1
//display_default_lcd=1
//#? config_hdmi_boost=4

//disable_overscan=1
//overscan_left=0
//overscan_right=0
//overscan_top=0
//overscan_bottom=0
//framebuff_width=1488
//framebuff_height=1104
//gpu_mem=128
//        int h_active_pixels; //horizontal pixels (width)
        int h_sync_polarity; //invert hsync polarity
//        int h_front_porch; //horizontal forward padding from DE acitve edge
//        int h_sync_pulse; //hsync pulse width in pixel clocks
//        int h_back_porch; //vertical back padding from DE active edge
//        int v_active_lines; //vertical pixels height (lines)
        int v_sync_polarity; //invert vsync polarity
//        int v_front_porch; //vertical forward padding from DE active edge
//        int v_sync_pulse; //vsync pulse width in pixel clocks
//        int v_back_porch; //vertical back padding from DE active edge
        int v_sync_offset_a; //leave at zero
        int v_sync_offset_b; //leave at zero
        int pixel_rep; //leave at zero
//        int frame_rate; //screen refresh rate in Hz
        int interlaced; //leave at zero
//        int pixel_freq; //clock frequency (width*height*framerate)
//        int aspect_ratio; //The aspect ratio can be set to one of eight values (choose the closest for your screen):
//HDMI_ASPECT_4_3 = 1  
//HDMI_ASPECT_14_9 = 2  
//HDMI_ASPECT_16_9 = 3  
//HDMI_ASPECT_5_4 = 4  
//HDMI_ASPECT_16_10 = 5  
//HDMI_ASPECT_15_9 = 6  
//HDMI_ASPECT_21_9 = 7  
//HDMI_ASPECT_64_27 = 8  
        int num_found = sscanf(str.c_str(), "hdmi_timings=%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
            &cfg->hdisplay, //&h_active_pixels, //horizontal pixels (width)
            &h_sync_polarity, //invert hsync polarity
            &cfg->hlead, //&h_front_porch, //horizontal forward padding from DE acitve edge
            &cfg->hsync, //&h_sync_pulse, //hsync pulse width in pixel clocks
            &cfg->htrail, //&h_back_porch, //vertical back padding from DE active edge
            &cfg->vdisplay, //&v_active_lines, //vertical pixels height (lines)
            &v_sync_polarity, //invert vsync polarity
            &cfg->vlead, //&v_front_porch, //vertical forward padding from DE active edge
            &cfg->vsync, //&v_sync_pulse, //vsync pulse width in pixel clocks
            &cfg->vtrail, //&v_back_porch, //vertical back padding from DE active edge
            &v_sync_offset_a, //leave at zero
            &v_sync_offset_b, //leave at zero
            &pixel_rep, //leave at zero
            &cfg->frame_rate, //frame_rate, //screen refresh rate in Hz
            &interlaced, //leave at zero
            &cfg->dot_clock, //&pixel_freq, //clock frequency (width*height*framerate)
            &cfg->aspect_ratio); //The aspect ratio can be set to one of eight values (choose the closest for your screen):
//        if (!str.compare(0, 13, "hdmi_timings="))
//        debug(BLUE_MSG "get hdmi timing from: '%s'? %d" ENDCOLOR_ATLINE(srcline), str.c_str(), num_found);
        cfg->dot_clock /= 1000; //convert to KHz to reduce chances of arithmetic overflow;
        cfg->htotal = cfg->hdisplay + cfg->hlead + cfg->hsync + cfg->htrail;
        cfg->vtotal = cfg->vdisplay + cfg->vlead + cfg->vsync + cfg->vtrail;
        if (num_found == 17) { cfg->screen = which; return true; } //continue (later entries overwrite earlier); //return true;
//hdmi_timings=1488 0 12 12 24   1104 0 12 12 24    0 0 0 30 0 50000000 1
//        cached.mode_line.hdisplay = h_active_pixels;
//        cached.mode_line.vdisplay = v_active_lines;
//vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
//        cached.mode_line.hsyncstart = h_front_porch; //+ h_active_pixels;
//        cached.mode_line.hsyncend = h_sync_pulse; //+ h_front_porch + h_active_pixels;
//        cached.mode_line.htotal = h_active_pixels + h_front_porch + h_sync_pulse + h_back_porch; //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
//        cached.mode_line.vsyncstart = v_active_pixels + v_front_porch;
//        cached.mode_line.vsyncend = v_active_pixels + v_front_porch + v_sync_pulse;
//        cached.mode_line.vtotal = v_active_pixels + v_front_porch + v_sync_pulse + v_back_porch; //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
//        cached.dot_clock = pixel_freq;
#if 0
        int hblank = cached.hcount[1] + cached.hcount[2] + cached.hcount[3], htotal = hblank + cached.hcount[0];
        int vblank = cached.vcount[1] + cached.vcount[2] + cached.vcount[3], vtotal = vblank + cached.vcount[0];
        double rowtime = (double)htotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
        double frametime = (double)htotal * vtotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
        debug_level(28, BLUE_MSG "hdmi_timings: %d x %d vis (aspect %f vs. %d), pxclk %2.1f MHz, hblank %d+%d+%d = %d (%2.1f%%), vblank = %d+%d+%d = %d (%2.1f%%), row %2.1f usec (%2.1f%% target), frame %2.1f msec (fps %2.1f vs. %d)" ENDCOLOR_ATLINE(srcline),
//            cached.mode_line.hdisplay, cached.mode_line.vdisplay, (double)cached.dot_clock / 1000, //vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
//            cached.mode_line.hsyncstart - cached.mode_line.hdisplay, cached.mode_line.hsyncend - cached.mode_line.hsyncstart, cached.mode_line.htotal - cached.mode_line.hsyncend, cached.mode_line.htotal - cached.mode_line.hdisplay, (double)100 * (cached.mode_line.htotal - cached.mode_line.hdisplay) / cached.mode_line.htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
//            cached.mode_line.vsyncstart - cached.mode_line.vdisplay, cached.mode_line.vsyncend - cached.mode_line.vsyncstart, cached.mode_line.vtotal - cached.mode_line.vsyncend, cached.mode_line.vtotal - cached.mode_line.vdisplay, (double)100 * (cached.mode_line.vtotal - cached.mode_line.vdisplay) / cached.mode_line.vtotal, //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
//            1000000 * rowtime, rowtime / 300000, 1000 * frametime, 1 / frametime);
            cached.hcount[0], cached.vcount[0], (double)cached.hcount[0] / cached.vcount[0], aspect_ratio, (double)cached.dot_clock / 1000,
            cached.hcount[1], cached.hcount[2], cached.hcount[3], hblank, (double)100 * hblank) / htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
            cached.vcount[1], cached.vcount[2], cached.vcount[3], vblank, (double)100 * vblank) / vtotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
            1e6 * rowtime, rowtime / 30e3, 1000 * frametime, 1 / frametime, frame_rate);
        return &cached;
#endif
    }
    return false; //cfg->dot_clock;
 }

#else //def RPI_NO_X
 #include <X11/Xlib.h>
 #include <X11/extensions/xf86vmode.h> //XF86VidModeGetModeLine
 #include <memory> //std::unique_ptr<>
 #define XScreen  Screen //avoid confusion
 #define XDisplay  Display //avoid confusion
#ifdef SIZEOF
 #undef SIZEOF //avoid conflict with xf86 def
#endif

// #pragma message("X11")

// typedef struct { int dot_clock; XF86VidModeModeLine mode_line; } ScreenConfig;

//inline int Release(FILE* that) { return fclose(that); }
// inline int Release(XDisplay* that) { return XCloseDisplay(that); }
//inline int Release(_XDisplay*& that) { return XCloseDisplay(that); }
//inline XDisplay* XOpenDisplay_fixup(const char* name) { return XOpenDisplay(name); }

#if 0
static void deleter(SDL_Window* ptr)
{
    if (!ptr) return;
//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
    delete_renderer(ptr); //delete renderer first (get it from window)
    debug(RED_MSG "SDL_AutoWindow: destroy window %p" ENDCOLOR, ptr);
    VOID SDL_DestroyWindow(ptr);
}
#endif


//see https://stackoverflow.com/questions/1829706/how-to-query-x11-display-resolution
//see https://tronche.com/gui/x/xlib/display/information.html#display
//or use cli xrandr or xwininfo
//NOTE: xrandr is the recommended api for multiple screens
//xrandr --query
// https://github.com/raboof/xrandr
 bool read_config(int screen, ScreenConfig* cfg, SrcLine srcline = 0)
 {
//BROKEN    auto_ptr<XDisplay> display = XOpenDisplay(NULL);
//    memset(&scfg, 0, sizeof(*scfg));
//    bool ok = false;
//    XDisplay* display = XOpenDisplay(NULL);
    std::unique_ptr<XDisplay, std::function<void(XDisplay*)>> display(XOpenDisplay(NULL), XCloseDisplay);
//    cfg->screen = -1;
    int num_screens = display.get()? ScreenCount(display.get()/*.cast*/): 0;
    debug(CFG_LEVEL, FMT("got disp %p") << display.get() << ", #screens: " << num_screens << ATLINE(srcline));
    int first = (screen != -1)? screen: 0, last = (screen != -1)? screen + 1: num_screens;
    for (int i = first; i < last; ++i)
    {
//        int dot_clock, mode_flags;
//        XF86VidModeModeLine mode_line = {0};
//        XScreen screen = ScreenOfDisplay(display.cast, i);
//see https://ubuntuforums.org/archive/index.php/t-779038.html
//xvidtune-show
//"1366x768"     69.30   1366 1414 1446 1480        768  770  775  780         -hsync -vsync
//             pxclk MHz                h_field_len                v_field_len    
        XF86VidModeModeLine mode_line;
        if (!XF86VidModeGetModeLine(display.get()/*.cast*/, i, &cfg->dot_clock, &mode_line)) continue; //&mode_line)); //continue; //return FALSE;
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
        cfg->hdisplay = mode_line.hdisplay; //&h_active_pixels, //horizontal pixels (width)
        cfg->hlead = mode_line.hsyncstart - mode_line.hdisplay; //&h_front_porch, //horizontal forward padding from DE acitve edge
        cfg->hsync = mode_line.hsyncend - mode_line.hsyncstart; //&h_sync_pulse, //hsync pulse width in pixel clocks
        cfg->htrail = mode_line.htotal - mode_line.hsyncend; //&h_back_porch, //vertical back padding from DE active edge
        cfg->vdisplay = mode_line.vdisplay; //&v_active_lines, //vertical pixels height (lines)
        cfg->vlead = mode_line.vsyncstart - mode_line.vdisplay; //&v_front_porch, //vertical forward padding from DE active edge
        cfg->vsync = mode_line.vsyncend - mode_line.vsyncstart; //&v_sync_pulse, //vsync pulse width in pixel clocks
        cfg->vtrail = mode_line.vtotal - mode_line.vsyncend; //&v_back_porch, //vertical back padding from DE active edge
        cfg->htotal = cfg->hdisplay + cfg->hlead + cfg->hsync + cfg->htrail;
        cfg->vtotal = cfg->vdisplay + cfg->vlead + cfg->vsync + cfg->vtrail;
//    int aspect_ratio;
//    int frame_rate;
        cfg->aspect_ratio = 0; //no config entry; //mode_line.hdisplay / mode_line.vdisplay;
        cfg->frame_rate = 0; //no config entry; //mode_line.htotal * mode_line.vtotal / cfg->dot_clock;
#if 0
        int hblank = cached.mode_line.htotal - cached.mode_line.hdisplay; //vinfo.left_margin + vinfo.hsync_len + vinfo.right_margin;
        int vblank = cached.mode_line.vtotal - cached.mode_line.vdisplay; //vinfo.upper_margin + vinfo.vsync_len + vinfo.lower_margin;
        double rowtime = (double)cached.mode_line.htotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
        double frametime = (double)cached.mode_line.htotal * cached.mode_line.vtotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;

        debug_level(28, BLUE_MSG "Screen[%d/%d] timing: %d x %d, pxclk %2.1f MHz, hblank %d+%d+%d = %d (%2.1f%%), vblank = %d+%d+%d = %d (%2.1f%%), row %2.1f usec (%2.1f%% target), frame %2.1f msec (fps %2.1f)" ENDCOLOR, i, num_screens,
            cached.mode_line.hdisplay, cached.mode_line.vdisplay, (double)cached.dot_clock / 1000, //vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
            cached.mode_line.hsyncstart - cached.mode_line.hdisplay, cached.mode_line.hsyncend - cached.mode_line.hsyncstart, cached.mode_line.htotal - cached.mode_line.hsyncend, cached.mode_line.htotal - cached.mode_line.hdisplay, (double)100 * (cached.mode_line.htotal - cached.mode_line.hdisplay) / cached.mode_line.htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
            cached.mode_line.vsyncstart - cached.mode_line.vdisplay, cached.mode_line.vsyncend - cached.mode_line.vsyncstart, cached.mode_line.vtotal - cached.mode_line.vsyncend, cached.mode_line.vtotal - cached.mode_line.vdisplay, (double)100 * (cached.mode_line.vtotal - cached.mode_line.vdisplay) / cached.mode_line.vtotal, //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
            1000000 * rowtime, rowtime / 300000, 1000 * frametime, 1 / frametime);
#endif
//    close(fbfd);
//        ok = true;
//        Release(display); //XCloseDisplay(display);
        cfg->screen = i;
        return true;
    }
//    if (display) Release(display); //XCloseDisplay(display);
    return false;
}
#endif //def RPI_NO_X


const ScreenConfig* getScreenConfig(int which = 0, SrcLine srcline = 0) //ScreenConfig* scfg) //XF86VidModeGetModeLine* mode_line)
{
    static ScreenConfig cached; //NOTE: doesn't need to be thread_local; only one (bkg) thread should be calling this
    if (cached.screen == which) return &cached; //return cached data; screen info won't change
    if (!read_config(which, &cached, srcline))
    {
        cached.screen = -1; //invalidate cache
        exc("video[%d] config not found" << ATLINE(srcline), which);
//        cached.dot_clock = 0;
        return NULL;
    }
    int hblank = cached.hlead + cached.hsync + cached.htrail, htotal = hblank + cached.hdisplay;
    int vblank = cached.vlead + cached.vsync + cached.vtrail, vtotal = vblank + cached.vdisplay;
//    double rowtime = (double)htotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
//    double frametime = (double)htotal * vtotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
    debug(CFG_LEVEL, "hdmi timing[%d]: %d x %d vis (aspect %f vs. %d), pxclk %2.1f MHz, hblank %d+%d+%d = %d (%2.1f%%), vblank = %d+%d+%d = %d (%2.1f%%), row %2.1f usec (%2.1f%% target), frame %2.1f msec (fps %2.1f vs. %d)" << ATLINE(srcline),
//            cached.mode_line.hdisplay, cached.mode_line.vdisplay, (double)cached.dot_clock / 1000, //vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
//            cached.mode_line.hsyncstart - cached.mode_line.hdisplay, cached.mode_line.hsyncend - cached.mode_line.hsyncstart, cached.mode_line.htotal - cached.mode_line.hsyncend, cached.mode_line.htotal - cached.mode_line.hdisplay, (double)100 * (cached.mode_line.htotal - cached.mode_line.hdisplay) / cached.mode_line.htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
//            cached.mode_line.vsyncstart - cached.mode_line.vdisplay, cached.mode_line.vsyncend - cached.mode_line.vsyncstart, cached.mode_line.vtotal - cached.mode_line.vsyncend, cached.mode_line.vtotal - cached.mode_line.vdisplay, (double)100 * (cached.mode_line.vtotal - cached.mode_line.vdisplay) / cached.mode_line.vtotal, //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
//            1000000 * rowtime, rowtime / 300000, 1000 * frametime, 1 / frametime);
        cached.screen, cached.hdisplay, cached.vdisplay, (double)cached.hdisplay / cached.vdisplay, cached.aspect_ratio, (double)cached.dot_clock / 1000,
        cached.hlead, cached.hsync, cached.htrail, hblank, (double)100 * hblank / htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
        cached.vlead, cached.vsync, cached.vtrail, vblank, (double)100 * vblank / vtotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
        cached.row_time() * 1e6, 100 * cached.row_time() * 1e6 / 30, 1000 * cached.frame_time(), /*1 / cached.frame_time()*/ cached.fps(), cached.frame_rate);
    return &cached;
}
const ScreenConfig* getScreenConfig(SrcLine srcline = 0) { return getScreenConfig(0, srcline); }


#if 0 //obsolete; just use SDL for this
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
    static std::atomic<WH> wh(0, 0);
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
        /*throw std::runtime_error*/ exc("Can't get screen size" ENDCOLOR);Screenshot at 2018-11-05 08:28:23
        wh.w = 1536;
        wh.h = wh.w * 3 / 4; //4:3 aspect ratio
        myprintf(22, YELLOW_MSG "Using dummy display mode %dx%d" ENDCOLOR, wh.w, wh.h);
    }
#endif
    return wh;
}
#endif
#else


///////////////////////////////////////////////////////////////////////////////
////
/// Get framebuffer info (any Linux box):
//

//auto-close a file descriptor:
//using AutoFD_super = std::unique_ptr<int, std::function<void(SDL_Window*)>>; //DRY kludge
class AutoFD
{
    int m_fd;
public: //ctors/dtors
    AutoFD(int fd): m_fd(fd) {}
    ~AutoFD() { if (m_fd) close(m_fd); }
public: //operators:
    operator int() const { return m_fd; }
};


#define ERR_2ARGS(msg, srcline)  exc(msg << ": %s (error %d)" ENDCOLOR_ATLINE(srcline), strerror(errno), errno)
#define ERR_1ARG(msg)  ERR_2ARGS(msg, 0)
#define ERR(...)  UPTO_2ARGS(__VA_ARGS__, ERR_2ARGS, ERR_1ARG) (__VA_ARGS__)


const ScreenConfig* getScreenConfig(int which = 0, SrcLine srcline = 0) //ScreenConfig* scfg) //XF86VidModeGetModeLine* mode_line)
{
    static ScreenConfig cached;
    if (cached.screen == which) return &cached; //return cached data; screen info won't change
    cached.screen = -1; //invalidate cache

//based on example from http://betteros.org/tut/graphics1.php
//    struct fb_fix_screeninfo finfo; //don't need this info
    struct fb_var_screeninfo vinfo;
    AutoFD fb(open("/dev/fb0", O_RDONLY)); //O_RDWR);
    if (fb < 0) { ERR("can't open FB; need sudo?", srcline); return 0; }
//for perms error, run with sudo or add pi user to video group ("sudo usermod -a -G video pi")
//more info at https://www.raspberrypi.org/forums/viewtopic.php?t=6568
//get variable screen info:
	if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) < 0) { ERR("can't get var fb info", srcline); return 0; }
//	vinfo.grayscale = 0;
//	vinfo.bits_per_pixel = 32;
//	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
//	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
//	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    cached.dot_clock = vinfo.pixclock; //pixel clock (pico seconds)

    cached.hdisplay = vinfo.xres; //visible resolution
    cached.hlead = vinfo.right_margin; //time from picture to sync
    cached.hsync = vinfo.hsync_len; //length of horizontal sync
    cached.htrail = vinfo.left_margin; //time from sync to picture
    cached.htotal = vinfo.xres_virtual; //virtual resolution

    cached.vdisplay = vinfo.yres;
    cached.vlead = vinfo.lower_margin; //time from picture to sync
    cached.vsync = vinfo.vsync_len;
    cached.vtrail = vinfo.upper_margin; //time from sync to picture
    cached.vtotal = vinfo.yres_virtual;
//__u32 height;	//height of picture in mm
//__u32 width; //width of picture in mm
//__u32 xoffset;			/* offset from virtual to visible */
//__u32 yoffset;			/* resolution			*/

    cached.aspect_ratio = 0; //configured, not calculated
    cached.frame_rate = 0; //configured, not calculated
    cached.screen = which;

    int hblank = cached.hlead + cached.hsync + cached.htrail; //, htotal = hblank + cached.hdisplay;
    int vblank = cached.vlead + cached.vsync + cached.vtrail; //, vtotal = vblank + cached.vdisplay;
//    double rowtime = (double)htotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
//    double frametime = (double)htotal * vtotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
    debug_level(28, BLUE_MSG "hdmi timing[%d]: %d x %d vis (aspect %f vs. %d), pxclk %2.1f MHz, hblank %d+%d+%d = %d (%2.1f%%), vblank = %d+%d+%d = %d (%2.1f%%), row %2.1f usec (%2.1f%% target), frame %2.1f msec (fps %2.1f vs. %d)" ENDCOLOR_ATLINE(srcline),
//            cached.mode_line.hdisplay, cached.mode_line.vdisplay, (double)cached.dot_clock / 1000, //vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
//            cached.mode_line.hsyncstart - cached.mode_line.hdisplay, cached.mode_line.hsyncend - cached.mode_line.hsyncstart, cached.mode_line.htotal - cached.mode_line.hsyncend, cached.mode_line.htotal - cached.mode_line.hdisplay, (double)100 * (cached.mode_line.htotal - cached.mode_line.hdisplay) / cached.mode_line.htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
//            cached.mode_line.vsyncstart - cached.mode_line.vdisplay, cached.mode_line.vsyncend - cached.mode_line.vsyncstart, cached.mode_line.vtotal - cached.mode_line.vsyncend, cached.mode_line.vtotal - cached.mode_line.vdisplay, (double)100 * (cached.mode_line.vtotal - cached.mode_line.vdisplay) / cached.mode_line.vtotal, //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
//            1000000 * rowtime, rowtime / 300000, 1000 * frametime, 1 / frametime);
        cached.screen, cached.hdisplay, cached.vdisplay, (double)cached.hdisplay / cached.vdisplay, cached.aspect_ratio, (double)cached.dot_clock / 1000,
        cached.hlead, cached.hsync, cached.htrail, hblank, (double)100 * hblank / cached.htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
        cached.vlead, cached.vsync, cached.vtrail, vblank, (double)100 * vblank / cached.vtotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
        cached.row_time() * 1e6, 100 * cached.row_time() * 1e6 / 30, 1000 * cached.frame_time(), /*1 / cached.frame_time()*/ cached.fps(), cached.frame_rate);
    return &cached;
}
const ScreenConfig* getScreenConfig(SrcLine srcline = 0) { return getScreenConfig(0, srcline); }
#endif


//from https://forums.libsdl.org/viewtopic.php?p=33010
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

int fbdev = -1;

void fbopen()
{
    if (fbdev >= 0) return;
    fbdev = open ("/dev/fb0", O_RDONLY /* O_RDWR */ );
    if ( fbdev < 0 ) printf( "Couldn't open /dev/fb0 for vsync\n" );
}

//call before SDL update/flip:
#ifndef FBIO_WAITFORVSYNC
 #define FBIO_WAITFORVSYNC  _IOW('F', 0x20, __u32)
#endif
void pre_update()
{
    if ( fbdev < 0 ) return;
    int arg = 0;
    ioctl( fbdev, FBIO_WAITFORVSYNC, &arg );
}

void fbclose()
{
    if ( fbdev < 0 ) return;
    close(fbdev);
    fbdev = -1; 
}

#endif //ndef _RPI_HELPERS_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

//#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
//#include "sdl-helpers.h"
#include "msgcolors.h"
#include "debugexc.h"
#include "srcline.h"

#include "rpi-helpers.h"


#define SDL_GetNumVideoDisplays()  1 //TODO

//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    debug(0, CYAN_MSG "is RPi? %d", isRPi());
//    return 0;
//    for (int screen = 0;; ++screen)
//    SDL_AutoLib sdl(SDL_INIT_VIDEO, SRCLINE);
//    debug(BLUE_MSG "get info for %d screens ..." ENDCOLOR, SDL_GetNumVideoDisplays());
    for (int screen = 0; screen < SDL_GetNumVideoDisplays(); ++screen)
    {
        const ScreenConfig* cfg = getScreenConfig(screen, SRCLINE); //ScreenConfig* scfg) //XF86VidModeGetModeLine* mode_line)
//        if (!cfg) break;
        INSPECT("ScreenConfig[" << screen << "]" << *cfg);
    }
}

#endif //def WANT_UNIT_TEST