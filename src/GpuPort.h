//GPU Port and helpers

#ifndef _GPU_PORT_H
#define _GPU_PORT_H

#include <unistd.h> //sysconf()
#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files

#include "sdl-helpers.h"
#include "msgcolors.h" //*_MSG, ENDCOLOR_*
#include "debugexc.h" //debug(), exc()
#include "srcline.h" //SrcLine, TEMPL_ARGS
#include "shmalloc.h" //AutoShmary<>


#ifndef rndup
 #define rndup(num, den)  (((num) + (den) - 1) / (den) * (den))
#endif

#define CACHE_LEN  64 //CAUTION: use larger of RPi and RPi 2 to ensure fewer hits; //static size_t cache_len = sysconf(_SC_PAGESIZE);
#define cache_pad(raw_len)  rndup(raw_len, CACHE_LEN)


////////////////////////////////////////////////////////////////////////////////
////
/// High-level interface to GpuPort:
//

#define MHz  *1e6


//use template params for W, H to allow type-safe 2D array access:
//NOTE: row pitch is rounded up to a multiple of cache size so there could be gaps
template<unsigned W = 24, unsigned H = 1111, typename NODEVAL = Uint32, unsigned BPN = 24, unsigned HWMUX = 0, unsigned H_PAD = cache_pad(H * sizeof(NODEVAL)) / sizeof(NODEVAL)> //unsigned UnivPadLen = cache_pad(H * sizeof(PIXEL)), unsigned H_PAD = UnivPadLen / sizeof(PIXEL)> //, int BIT_BANGER = none>
//template<unsigned W = 24, unsigned FPS = 30, typename PIXEL = Uint32, unsigned BPN = 24, unsigned HWMUX = 0, unsigned H_PAD = cache_pad(H * sizeof(PIXEL)) / sizeof(PIXEL)> //unsigned UnivPadLen = cache_pad(H * sizeof(PIXEL)), unsigned H_PAD = UnivPadLen / sizeof(PIXEL)> //, int BIT_BANGER = none>
class GpuPort
{
//    using PIXEL = Uint32;
//    using UnivPadLen = cache_pad(H * sizeof(PIXEL)); //univ buf padded up to cache size (for better memory performance while rendering pixels)
//    using H_PAD = UnivPadLen / sizeof(PIXEL); //effective univ len
//    using ROWTYPE = NODEVAL[H_PAD]; //padded out to cache row size
//    using BUFTYPE = ROWTYPE[W];
//    typedef ROWTYPE NODEBUFTYPE[W]; //2D W x H_PAD
    using NODEBUFTYPE = NODEVAL[W][H_PAD]; //2D W x H_PAD
public: //ctors/dtors
    GpuPort(int clock, key_t key = 0, SrcLine srcline = 0): Clock(clock), FPS(clock / 3 / BPN / H ), //NUM_UNIV(W), UNIV_LEN(H), 
//CAUTION: dcl order determines init order, not occurrence order here!
        inout("GpuPort init/exit", SRCLINE), 
        m_txtr(SDL_AutoTexture::create(NAMED{ _.w = 3 * W; _.h = H; _.srcline = NVL(srcline, SRCLINE); })), 
//        inout2("order check 2", SRCLINE), 
        m_shmbuf(1, key, NVL(srcline, SRCLINE)), 
//        inout3("order check 3", SRCLINE),
        nodes(*m_shmbuf.ptr()), //(ROWTYPE*)m_shmbuf),
//        inout4("order check 4", SRCLINE),
        m_srcline(NVL(srcline, SRCLINE)) //m_univlen(cache_pad(H * sizeof(pixels[0]))), m_shmbuf(W * m_univlen / sizeof(pixels[0]),
    {
//        if (!m_txtr.) exc(RED_MSG "texture/wnd alloc failed");
//        if (!m_shmbuf.ptr()) exc(RED_MSG "pixel buf alloc failed");
        debug(GREEN_MSG << "ctor " << *this << ", init took " << inout.restart() << " msec" << ENDCOLOR_ATLINE(m_srcline));
//debug(CYAN_MSG "&clock %p, &fps %p, &num univ %p, &univ len %p, &inout %p, &txtr %p, &pixbits[0] %p, &shmbuf %p, shmbuf ptr %p vs %p, &nodes[0][0] %p, &inout2 %p" ENDCOLOR, 
// &Clock, &FPS, &NUM_UNIV, &UNIV_LEN, &inout, &m_txtr, &m_pixbits[0], &m_shmbuf, m_shmbuf.ptr(), (ROWTYPE*)m_shmbuf, &nodes[0][0], &inout2);
//checknodes(SRCLINE);
    }
    virtual ~GpuPort() { debug(RED_MSG << "dtor " << *this << ", lifespan " << inout.restart() << " msec" << ENDCOLOR_ATLINE(m_srcline)); }
public: //operators
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const GpuPort& that)
    {
        ostrm << "GpuPort" << my_templargs(); //TEMPL_ARGS;
        ostrm << "{" << FMT("%p: ") << &that;
        ostrm << FMT("clock %2.1f MHz") << that.Clock / 1e6;
        ostrm << ", " << W /*NUM_UNIV*/ << " x " << H /*UNIV_LEN*/ << " (" << (W * H) << ") => " << H_PAD << " (" << (W * H_PAD) << ")";
//        ostrm << ", univ pad len " << UnivPadLen;
        ostrm << ", bits/node " << BPN << " (3x)";
        ostrm << FMT(", fps %4.3f") << that.FPS;
        ostrm << ", h/w mux " << HWMUX;
        ostrm << FMT(", nodes %p") << &that.nodes[0][0]; //FMT(" pixels ") << &me.pixels;
        ostrm << FMT("..%p") << &that.nodes[W][0];
        ostrm << "=+" << &that.nodes[W][0] - &that.nodes[0][0];
        ostrm << ", shmbuf " << that.m_shmbuf;
//        ostrm << ", pixels " << that.pixels;
//        ostrm << ", pixel buflen " << me.pixels.
        ostrm << "}";
        return ostrm;        
    }
public: //data members
    enum BlendMode: int
    {
        Node = SDL_BLENDMODE_NONE, //no blending: dstRGBA = srcRGBA
        AlphaBlend = SDL_BLENDMODE_BLEND, //alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB * (1-srcA)); dstA = srcA + (dstA * (1-srcA))
        Additive = SDL_BLENDMODE_ADD, //additive blending: dstRGB = (srcRGB * srcA) + dstRGB; dstA = dstA
        Modulate = SDL_BLENDMODE_MOD, //color modulate: dstRGB = srcRGB * dstRGB; dstA = dstA
    };
//    const size_t UnivPadLen = cache_pad(H, sizeof(PIXEL)); //univ buf padded up to cache size (for better memory performance while rendering pixels)
//    const size_t H_PAD = UnivPadLen / sizeof(PIXEL); //effective univ len
//    PIXEL (&pixels)[W][H_PAD]; //CAUTION: pivoted so nodes within each univ (column) are adjacent (for better cache performance)
    const int Clock;
    const float FPS;
//        FPS(clock / 3 / BPN / H ), 
    STATIC const unsigned Width = W, Height = H; //NUM_UNIV = W, UNIV_LEN = H; //technically these can be static, but that requires dangling dcl so just make them members
//    /*BUFTYPE&*/ ROWTYPE* const& /*const*/ nodes; //CAUTION: pivoted so nodes within each univ (column) are adjacent (for better cache performance); NOTE: "const" ref needed for rvalue
public: //methods
//    float fps() const { return m_clock / 3; }
    void fill(NODEVAL color = BLACK, /*TODO: BlendMode mode = None,*/ SrcLine srcline = 0)
    {
//checknodes(SRCLINE);
        InOutDebug inout("gpu port fill", SRCLINE);
//no        VOID m_txtr.render(color, NVL(srcline, SRCLINE));
//        auto elapsed = -elapsed_msec();
//        for (int i = 0; i < W * H; ++i)
//fill in-memory pixels allows caller to make more changes before GPU render
//        for (int x = 0; x < W; ++x)
//            for (int y = 0; y < H; ++y) //CAUTION: leaves gaps (H_PAD vs. H)
//                pixels[x][y] = color;
//        if (&nodes[0][W * H_PAD] != &nodes[W][0]) exc("&nodes[0][%d x %d = %d] %p != &nodes[%d][0] %p", W, H_PAD, W * H_PAD, &nodes[0][W * H_PAD], W, &nodes[W][0]);
        debug(BLUE_MSG << *this << ENDCOLOR);
        debug(BLUE_MSG "&nodes[0][0] = %p, &[1][0] = %p, [%d][0] = %p" ENDCOLOR, &nodes[0][0], &nodes[1][0], W, &nodes[W][0]);
        for (int i = 0; i < W * H_PAD; ++i) nodes[0][i] = color; //NOTE: fills in H..H_PAD gap as well for simplicity
//checknodes(SRCLINE);
//        elapsed += elapsed_msec();
//        debug(BLUE_MSG << "fill all %d x %d = %d pixels with 0x%x took %ld msec" << ENDCOLOR_ATLINE(srcline), W, H, W * H, elapsed);
    }
#if 0 //TODO?
    void dirty(ROWTYPE& univ, SrcLine srcline = 0) //PIXEL (&univ)[H_PAD]
    {
//printf("hello1\n"); fflush(stdout);
        debug(BLUE_MSG "mark row " << (&univ - &pixels[0]) << "/" << W << " dirty" << ENDCOLOR_ATLINE(srcline));
//printf("hello2\n"); fflush(stdout);
    }
#endif
    void refresh(SrcLine srcline = 0)
    {

//checknodes(SRCLINE);
        InOutDebug inout("gpu port refresh", SRCLINE);
        debug(BLUE_MSG << *this << ENDCOLOR);
        VOID bitbang(nodes, m_pixbits, NVL(srcline, SRCLINE));
//checknodes(SRCLINE);
        VOID m_txtr.update(NAMED{ _.pixels = m_pixbits; _.srcline = NVL(srcline, SRCLINE); }); //, true, SRCLINE); //W * sizeof (Uint32)); //no rect, pitch = row length
//checknodes(SRCLINE);
#if 0 //TODO
        render_timestamp = times.previous; //now (presentation time)
        ++numfr;
#endif
    }
public: //static utility methods
#if 0
//round up data sizes for better memory performance:
//from https://www.raspberrypi.org/forums/viewtopic.php?t=114228
//RPi I-Cache line size 32 bytes, D-Cache line size 32 bytes Pi, 64 bytes Pi2
//MMU page size 4K
//see also https://softwareengineering.stackexchange.com/questions/328775/how-important-is-memory-alignment-does-it-still-matter
    static size_t page_pad(size_t wanted, SrcLine srcline = 0)
    {
        static size_t page_len = sysconf(_SC_PAGESIZE);
        size_t retval = rndup(wanted, page_len);
//#define ROWPAD(len)  rndup(len, PAGE)
        debug(BLUE_MSG "page_pad: %zu round up %zu => %zu" ENDCOLOR_ATLINE(srcline), wanted, page_len, retval);
        return retval;
    }
    static size_t cache_pad(size_t wanted, SrcLine srcline = 0)
    {
        const size_t cache_len = 64; //CAUTION: use larger of RPi and RPi 2 to ensure fewer hits; //static size_t cache_len = sysconf(_SC_PAGESIZE);
        size_t retval = rndup(wanted, cache_len);
        debug(BLUE_MSG "cache_pad: %zu round up %zu => %zu" ENDCOLOR_ATLINE(srcline), wanted, cache_len, retval);
        return retval;
    }
#endif
private: //helpers
//PIXEL[H_PAD]* const& pixels;
    static void bitbang(NODEBUFTYPE& nodes, Uint32* pixbits, SrcLine srcline = 0)
    {
        InOutDebug inout("gpu port bitbang", SRCLINE);
//        auto timer = -elapsed_msec();
//TODO: use optimal pad/pitch in pixbits texture
//        debug(BLUE_MSG "bitbang: &nodes[0][0] = %p, &pixbits[0] = %p" ENDCOLOR, &nodes[0][0], &pixbits[0]);
//        debug(BLUE_MSG "bitbang: &nodes[%d][0] = %p, &pixbits[%d] = %p" ENDCOLOR, W, &nodes[W][0], 3 * W * H, &pixbits[3 * W * H]);
        for (int x = 0, i = 0; x < W; ++x)
            for (int y = 0; y < H; ++y) //CAUTION: y must be inner loop
            {
                pixbits[i++] = dimARGB(0.75, WHITE);
                pixbits[i++] = nodes[x][y];
                pixbits[i++] = dimARGB(0.25, WHITE);
            }
//        timer += elapsed_msec();
//        debug(BLUE_MSG "bitbang %d x %d took %ld msec" << ENDCOLOR_ATLINE(srcline), W, H, timer);
    }
#if 0
    void checknodes(SrcLine srcline = 0)
    {
        static NODEVAL* svnodes = 0;
        if (&nodes[0][0] == svnodes) { debug(YELLOW_MSG "nodes stable at %p" ENDCOLOR_ATLINE(srcline), &nodes[0][0]); return; }
        if (svnodes) exc_soft("nodes moved from %p to %p @%s?", svnodes, &nodes[0][0], NVL(srcline, SRCLINE));
        svnodes = &nodes[0][0];
    }
#endif
private: //data members
//    Uint32* m_shmptr;
//    int m_clock;
//    const size_t m_univlen; //univ buf padded up to cache size (for better memory performance while rendering pixels)
    InOutDebug inout; //put this before AutoTexture
    SDL_AutoTexture m_txtr;
    Uint32 m_pixbits[3 * W * H];
//to look at shm: ipcs -m 
//detailde info:  ipcs -m -i <shmid>
//to delete shm: ipcrm -M <key>
    AutoShmary<NODEBUFTYPE, false> m_shmbuf; //PIXEL[H_PAD] //CAUTION: must occur before nodes
public:
//    InOutDebug inout3; //inout4, inout2, inout3; //check init order
//wrong!    /*BUFTYPE&*/ ROWTYPE* const& /*const*/ nodes; //CAUTION: pivoted so nodes within each univ (column) are adjacent (for better cache performance); NOTE: "const" ref needed for rvalue
//    ROWTYPE (&nodes)[W]; //ref to array of W ROWTYPEs; CAUTION: "()" required
    NODEBUFTYPE& nodes; //CAUTION: must come after m_shmbuf (init order)
private:
//    InOutDebug inout3; //inout4, inout2, inout3; //check init order
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
    static std::string& my_templargs() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::string m_templ_args(TEMPL_ARGS), dummy = m_templ_args.append("\n"); //only used for debug msgs
        return m_templ_args;
    }
};

#endif //ndef _GPU_PORT_H


////////////////////////////////////////////////////////////////////////////////
////
/// unit test:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

//#include <iostream> //std::cout
#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files

#include "debugexc.h"
#include "msgcolors.h"
#include "elapsed.h" //timestamp()
#include "GpuPort.h"


//int main(int argc, const char* argv[])
void unit_test()
{
    const int NUM_UNIV = 30, UNIV_LEN = 100; //24, 1111
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE, dimARGB(0.75, WHITE), dimARGB(0.25, WHITE)};

    GpuPort<NUM_UNIV, UNIV_LEN/*, raw WS281X*/> gp(2.4 MHz, 0, SRCLINE);
//    Uint32* pixels = gp.Shmbuf();
//    Uint32 myPixels[H][W]; //TODO: [W][H]; //NOTE: put nodes in same universe adjacent for better cache performance
//    auto& pixels = gp.pixels; //NOTE: this is shared memory, so any process can update it
//    debug(CYAN_MSG "&nodes[0] %p, &nodes[0][0] = %p, sizeof gp %zu, sizeof nodes %zu" ENDCOLOR, &gp.nodes[0], &gp.nodes[0][0], sizeof(gp), sizeof(gp.nodes));

    debug(BLUE_MSG << timestamp() << "render" << ENDCOLOR);
    for (int x = 0; x < gp.Width /*NUM_UNIV*/; ++x)
        for (int y = 0; y < gp.Height /*UNIV_LEN*/; ++y)
            gp.nodes[x][y] = palette[(x + y) % SIZEOF(palette)]; //((x + c) & 1)? BLACK: palette[(y + c) % SIZEOF(palette)]; //((x + y) & 3)? BLACK: palette[c % SIZEOF(palette)];
    debug(BLUE_MSG << timestamp() << "refresh" << ENDCOLOR);
    gp.refresh(SRCLINE);
    debug(BLUE_MSG << timestamp() << "wait" << ENDCOLOR);
    VOID SDL_Delay(10 sec);
    debug(BLUE_MSG << timestamp() << "quit" << ENDCOLOR);
    return;

    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        gp.fill(palette[c], SRCLINE);
        gp.refresh(SRCLINE);
        VOID SDL_Delay(1 sec);
        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
    }

//    debug(BLUE_MSG << "GpuPort" << ENDCOLOR);
    for (int c;; ++c)
//    {
        for (int x = 0; x < gp.Width /*NUM_UNIV*/; ++x)
//        {
            for (int y = 0; y < gp.Height /*UNIV_LEN*/; ++y)
{
                Uint32 color = palette[c % SIZEOF(palette)]; //((x + c) & 1)? BLACK: palette[(y + c) % SIZEOF(palette)]; //((x + y) & 3)? BLACK: palette[c % SIZEOF(palette)];
if ((x < 4) && (y < 4)) printf("%sset pixel[%d,%d] @%p = 0x%x...\n", timestamp().c_str(), x, y, &gp.nodes[x][y], color); fflush(stdout);
                gp.nodes[x][y] = color;
                gp.refresh(SRCLINE);
                VOID SDL_Delay(1 sec);
                if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
}
//printf("%sdirty[%d] ...\n", timestamp().c_str(), x); fflush(stdout);
//            gp.dirty(gp.pixels[x], SRCLINE);
//        }
//        gp.refresh(SRCLINE);
//        VOID SDL_Delay(1 sec);
//        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
//    }
    debug(BLUE_MSG << "finish" << ENDCOLOR);
//    return 0;
}

#endif //def WANT_UNIT_TEST
