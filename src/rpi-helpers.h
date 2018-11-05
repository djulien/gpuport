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

#include "debugexc.h" //debug()
#include "srcline.h" //SrcLine, SRCLINE
#include "ostrfmt.h" //FMT()


///////////////////////////////////////////////////////////////////////////////
////
/// check for host:
//

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
    static std::atomic<tristate> cached(tristate::Maybe);
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

typedef struct ScreenConfig
{
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
    int hcount[4]; //hdisplay + front porch + hsync + back porch = htotal
    int vcount[4]; //vdisplay + front porch + vsync + back porch = vtotal
    int aspect_ratio;
    int frame_rate;
} ScreenConfig;


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

 bool read_config(ScreenConfig* cfg, SrcLine srcline = 0)
 {
    std::string str; 
    std::ifstream file("/boot/config.txt"); //TODO: read from memory; config file could have multiple (conditional) entries
    while (std::getline(file, str))
    {
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
            &cfg->hcount[0], //&h_active_pixels, //horizontal pixels (width)
            &h_sync_polarity, //invert hsync polarity
            &cfg->hcount[1], //&h_front_porch, //horizontal forward padding from DE acitve edge
            &cfg->hcount[2], //&h_sync_pulse, //hsync pulse width in pixel clocks
            &cfg->hcount[3], //&h_back_porch, //vertical back padding from DE active edge
            &cfg->vcount[0], //&v_active_lines, //vertical pixels height (lines)
            &v_sync_polarity, //invert vsync polarity
            &cfg->vcount[1], //&v_front_porch, //vertical forward padding from DE active edge
            &cfg->vcount[2], //&v_sync_pulse, //vsync pulse width in pixel clocks
            &cfg->vcount[3], //&v_back_porch, //vertical back padding from DE active edge
            &v_sync_offset_a, //leave at zero
            &v_sync_offset_b, //leave at zero
            &pixel_rep, //leave at zero
            &cfg->frame_rate, //frame_rate, //screen refresh rate in Hz
            &interlaced, //leave at zero
            &cfg->dot_clock, //&pixel_freq, //clock frequency (width*height*framerate)
            &cfg->aspect_ratio); //The aspect ratio can be set to one of eight values (choose the closest for your screen):
        if (!str.compare(0, 13, "hdmi_timings="))
            debug(BLUE_MSG "get hdmi timing from: '%s'? %d" ENDCOLOR_ATLINE(srcline), str.c_str(), num_found);
        if (num_found == 17) return true;
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
    return false;
 }

#else //def RPI_NO_X
 #include <X11/Xlib.h>
 #include <X11/extensions/xf86vmode.h> //XF86VidModeGetModeLine
 #include <memory> //std::unique_ptr<>
 #define XScreen  Screen //avoid confusion
 #define XDisplay  Display //avoid confusion

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
 bool read_config(ScreenConfig* cfg, SrcLine srcline = 0)
 {
//BROKEN    auto_ptr<XDisplay> display = XOpenDisplay(NULL);
//    memset(&scfg, 0, sizeof(*scfg));
//    bool ok = false;
//    XDisplay* display = XOpenDisplay(NULL);
    std::unique_ptr<XDisplay, std::function<void(XDisplay*)>> display(XOpenDisplay(NULL), XCloseDisplay);
    debug(BLUE_MSG << FMT("got disp %p") << display.get() << ENDCOLOR_ATLINE(srcline));
    int num_screens = display.get()? ScreenCount(display.get()/*.cast*/): 0;
    for (int i = 0; i < num_screens; ++i)
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
        cfg->hcount[0] = mode_line.hdisplay; //&h_active_pixels, //horizontal pixels (width)
        cfg->hcount[1] = mode_line.hsyncstart - mode_line.hdisplay; //&h_front_porch, //horizontal forward padding from DE acitve edge
        cfg->hcount[2] = mode_line.hsyncend - mode_line.hsyncstart; //&h_sync_pulse, //hsync pulse width in pixel clocks
        cfg->hcount[3] = mode_line.htotal - mode_line.hsyncend; //&h_back_porch, //vertical back padding from DE active edge
        cfg->vcount[0] = mode_line.vdisplay; //&v_active_lines, //vertical pixels height (lines)
        cfg->vcount[1] = mode_line.vsyncstart - mode_line.vdisplay; //&v_front_porch, //vertical forward padding from DE active edge
        cfg->vcount[2] = mode_line.vsyncend - mode_line.vsyncstart; //&v_sync_pulse, //vsync pulse width in pixel clocks
        cfg->vcount[3] = mode_line.vtotal - mode_line.vsyncend; //&v_back_porch, //vertical back padding from DE active edge
        cfg->aspect_ratio = 0;
        cfg->frame_rate = mode_line.htotal * mode_line.vtotal / cfg->dot_clock;
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
        return true;
    }
//    if (display) Release(display); //XCloseDisplay(display);
    return false;
}
#endif //def RPI_NO_X


const ScreenConfig* getScreenConfig(SrcLine srcline = 0) //ScreenConfig* scfg) //XF86VidModeGetModeLine* mode_line)
{
    static ScreenConfig cached = {0};
    if (cached.dot_clock) return &cached; //return cached data; screen info won't change
    if (!read_config(&cached, srcline)) { cached.dot_clock = 0; return NULL; }
    int hblank = cached.hcount[1] + cached.hcount[2] + cached.hcount[3], htotal = hblank + cached.hcount[0];
    int vblank = cached.vcount[1] + cached.vcount[2] + cached.vcount[3], vtotal = vblank + cached.vcount[0];
    double rowtime = (double)htotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) / vinfo.pixclock; //must be ~ 30 usec for WS281X
    double frametime = (double)htotal * vtotal / cached.dot_clock / 1000; //(vinfo.xres + hblank) * (vinfo.yres + vblank) / vinfo.pixclock;
    debug_level(28, BLUE_MSG "hdmi_timings: %d x %d vis (aspect %f vs. %d), pxclk %2.1f MHz, hblank %d+%d+%d = %d (%2.1f%%), vblank = %d+%d+%d = %d (%2.1f%%), row %2.1f usec (%2.1f%% target), frame %2.1f msec (fps %2.1f vs. %d)" ENDCOLOR_ATLINE(srcline),
//            cached.mode_line.hdisplay, cached.mode_line.vdisplay, (double)cached.dot_clock / 1000, //vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.pixclock,
//            cached.mode_line.hsyncstart - cached.mode_line.hdisplay, cached.mode_line.hsyncend - cached.mode_line.hsyncstart, cached.mode_line.htotal - cached.mode_line.hsyncend, cached.mode_line.htotal - cached.mode_line.hdisplay, (double)100 * (cached.mode_line.htotal - cached.mode_line.hdisplay) / cached.mode_line.htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
//            cached.mode_line.vsyncstart - cached.mode_line.vdisplay, cached.mode_line.vsyncend - cached.mode_line.vsyncstart, cached.mode_line.vtotal - cached.mode_line.vsyncend, cached.mode_line.vtotal - cached.mode_line.vdisplay, (double)100 * (cached.mode_line.vtotal - cached.mode_line.vdisplay) / cached.mode_line.vtotal, //vinfo.upper_margin, vinfo.lower_margin, vinfo.vsync_len,
//            1000000 * rowtime, rowtime / 300000, 1000 * frametime, 1 / frametime);
        cached.hcount[0], cached.vcount[0], (double)cached.hcount[0] / cached.vcount[0], cached.aspect_ratio, (double)cached.dot_clock / 1000,
        cached.hcount[1], cached.hcount[2], cached.hcount[3], hblank, (double)100 * hblank / htotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
        cached.vcount[1], cached.vcount[2], cached.vcount[3], vblank, (double)100 * vblank / vtotal, //vinfo.left_margin, vinfo.right_margin, vinfo.hsync_len, 
        rowtime * 1e6, 100 * rowtime * 1e6 / 30, 1000 * frametime, 1 / frametime, cached.frame_rate);
    return &cached;
}


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
        /*throw std::runtime_error*/ exc(RED_MSG "Can't get screen size" ENDCOLOR);
        wh.w = 1536;
        wh.h = wh.w * 3 / 4; //4:3 aspect ratio
        myprintf(22, YELLOW_MSG "Using dummy display mode %dx%d" ENDCOLOR, wh.w, wh.h);
    }
#endif
    return wh;
}
#endif


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
//    return 0;
    const ScreenConfig* cfg = getScreenConfig(SRCLINE); //ScreenConfig* scfg) //XF86VidModeGetModeLine* mode_line)
}

#endif //def WANT_UNIT_TEST