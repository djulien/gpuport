//GPU Port and helpers

#ifndef _GPU_PORT_H
#define _GPU_PORT_H

#include <unistd.h> //sysconf()
#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
#include <functional> //std::function<>, std::bind(), std::placeholders
#include <algorithm> //std::min()
#include <unistd.h> //usleep()
#include <thread> //std::thread::get_id()

#include "sdl-helpers.h"
#include "rpi-helpers.h"
#include "msgcolors.h" //*_MSG, ENDCOLOR_*
#include "debugexc.h" //debug(), exc()
#include "srcline.h" //SrcLine, TEMPL_ARGS
#include "shmalloc.h" //AutoShmary<>


#ifndef rndup
 #define rndup(num, den)  (((num) + (den) - 1) / (den) * (den))
#endif

#ifndef SIZEOF
 #define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))
#endif

//accept variable # up to 2 - 3 macro args:
//#ifndef UPTO_2ARGS
// #define UPTO_2ARGS(one, two, three, ...)  three
//#endif
#ifndef UPTO_3ARGS
 #define UPTO_3ARGS(one, two, three, four, ...)  four
#endif


////////////////////////////////////////////////////////////////////////////////
////
/// High-level interface to GpuPort:
//

#define MHz  *1000000 //must be int for template param; //*1e6

#define LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA; safely allows 300 LEDs per 20A at "full" (83%) white

#define CACHE_LEN  64 //CAUTION: use larger of RPi and RPi 2 size to ensure fewer conflicts across processors; //static size_t cache_len = sysconf(_SC_PAGESIZE);
#define cache_pad(raw_len)  rndup(raw_len, CACHE_LEN)


//timing constraints:
//hres / clock (MHz) = node time (usec):
//calculate 3th timing param using other 2 as constraints
//#define CLOCK_HRES2NODE(clock, hres)  ((hres) / (clock))
//#define CLOCK_NODE2HRES(clock, node)  ((clock) * (node))
//#define HRES_NODE2CLOCK(hres, node)  ((hres) / (node))

//clock (MHz) / hres / vres = fps:
//calculate 4th timing param using other 3 as constraints
//#define CLOCK_HRES_VRES2FPS(clock, hres, vres)  ((clock) / (hres) / (vres))
//#define CLOCK_HRES_FPS2VRES(clock, hres, fps)  ((clock) / (hres) / (fps))
//#define CLOCK_VRES_FPS2HRES(clock, vres, fps)  ((clock) / (vres) / (fps))
//#define HRES_VRES_FPS2CLOCK(hres, vres, fps)  ((hres) * (vres) * (fps))
//#define VRES_LIMIT(clock, hres, fps)  ((clock) / (hres) / (fps))
//#define HRES_LIMIT(clock, vres, fps)  ((clock) / (vres) / (fps))
//#define FPS_LIMIT(clock, hres, vres)  ((clock) / (hres) / (vres))
//#define CLOCK_LIMIT(hres, vres, fps)  ((hres) * (vres) * (fps))

#define CLOCK_CONSTRAINT_2ARGS(hres, nodetime)  ((hres) / (nodetime))
#define CLOCK_CONSTRAINT_3ARGS(hres, vres, fps)  ((hres) * (vres) * (fps))
#define CLOCK_CONSTRAINT(...)  UPTO_3ARGS(__VA_ARGS__, CLOCK_CONSTRAINT_3ARGS, CLOCK_CONSTRAINT_2ARGS, CLOCK_CONSTRAINT_1ARG) (__VA_ARGS__)

#define HRES_CONSTRAINT_2ARGS(clock, nodetime)  ((clock) * (nodetime))
#define HRES_CONSTRAINT_3ARGS(clock, vres, fps)  ((clock) / (vres) / (fps))
#define HRES_CONSTRAINT(...)  UPTO_3ARGS(__VA_ARGS__, HRES_CONSTRAINT_3ARGS, HRES_CONSTRAINT_2ARGS, HRES_CONSTRAINT_1ARG) (__VA_ARGS__)

#define NODETIME_CONSTRAINT(clock, hres)  ((hres) / (clock))
#define VRES_CONSTRAINT(clock, hres, fps)  ((clock) / (hres) / (fps))
#define FPS_CONSTRAINT(clock, hres, vres)  ((clock) / (hres) / (vres))



//use template params for W, H to allow type-safe 2D array access:
//NOTE: row pitch is rounded up to a multiple of cache size so there could be gaps
//always use full screen height; clock determined by w / BPN
//kludge: use template arg as vres placeholder (depends on run-time config info)
template<
//settings that must match config:
//default values are for my layout
    int CLOCK = 52 MHz, //pixel clock speed (constrained by GPU)
    int HTOTAL = 1536, //total x res including blank/sync (might be contrained by GPU); 
    int FPS = 30, //target #frames/sec
//settings that must match h/w:
    int IOPINS = 24, //total #I/O pins available (h/w dependent)
    int HWMUX = 0, //#I/O pins to use for external h/w mux
    int NODEBITS = 24, //# bits to send for each WS281X node (protocol dependent)
//settings determined by s/w:
    typename NODEVAL = Uint32, //data type for node colors
    typename XFRTYPE = Uint32, //data type for bit banged node bits (ARGB)
    typename UNIVMAP = XFRTYPE, //cross-univ bitmaps
//    int NUM_THREADS = 0, //0 = fg processing, 1 = bkg thread (in-process), > 1 for multiple threads/processes
//calculated values based on above constraints:
//these must be defined here because they affect NODEBUF compile-time type
    int NUM_UNIV = (1 << HWMUX) * (IOPINS - HWMUX), //max #univ with external h/w mux
    int XFRW = NODEBITS * 3 - 1, //last 1/3 of last node bit can overlap hsync so exclude it from display width
    int XFRW_PADDED = cache_pad(XFRW * sizeof(XFRTYPE)) / sizeof(XFRTYPE),
    int UNIV_MAX = VRES_CONSTRAINT(CLOCK, HTOTAL, FPS), //max #nodes per univ
    int H_PADDED = cache_pad(UNIV_MAX * sizeof(NODEVAL)) / sizeof(NODEVAL)> //avoid cross-universe memory cache contention
//, unsigned H_PADDED = cache_pad(H * sizeof(NODEVAL)) / sizeof(NODEVAL)> //unsigned UnivPadLen = cache_pad(H * sizeof(PIXEL)), unsigned H_PADDED = UnivPadLen / sizeof(PIXEL)> //, int BIT_BANGER = none>
//template<unsigned W = 24, unsigned FPS = 30, typename PIXEL = Uint32, unsigned BPN = 24, unsigned HWMUX = 0, unsigned H_PADDED = cache_pad(H * sizeof(PIXEL)) / sizeof(PIXEL)> //unsigned UnivPadLen = cache_pad(H * sizeof(PIXEL)), unsigned H_PADDED = UnivPadLen / sizeof(PIXEL)> //, int BIT_BANGER = none>
class GpuPort
{
protected: //helper classes + structs
//template <typename PXTYPE = Uint32>
#if 0
//class mySDL_AutoTexture_bitbang: public mySDL_AutoTexture<PXTYPE>
//override txtr xfr with bit banging:
//txtr needs to be locked to update anyway, so just bit bang into txtr rather than an additional buffer + mem xfr
    using txtr_bb_super = SDL_AutoTexture<XFRTYPE>; //DRY kludge
    class txtr_bb: public txtr_bb_super //SDL_AutoTexture<XFRTYPE>
    {
        using super = txtr_bb_super; //SDL_AutoTexture<XFRTYPE>;
//        const SDL_Size m_wh;
public: //ctors/dtors
//        template <typename ... ARGS>
//        explicit txtr_bb(/*int w, int h,*/ ARGS&& ... args): super(std::forward<ARGS>(args) ...) {} //perfect fwd; need this to prevent "deleted function" errors
        /*explicit*/ txtr_bb(SDL_Texture* txtr, SDL_Window* wnd, SrcLine srcline2 = 0): super(txtr, wnd, srcline2) {}
        txtr_bb(const super& that): super(that) {} //m_wnd(that.m_wnd, SRCLINE) { reset(((mySDL_AutoTexture)that).release()); } //kludge: wants rval, but value could change; //operator=(that.get()); } //copy ctor; //= delete; //deleted copy constructor
public: //static helper methods
//override for bit-banged xfr:
        static void xfr(void* txtrbuf, const void* nodebuf, size_t xfrlen, SrcLine srcline2 = 0) //h * pitch(NUM_UNIV)
        {
//            VOID memcpy(pxbuf, pixels, xfrlen);
//            SDL_Size wh(NUM_UNIV, m_cached.wh.h); //use univ len from txtr; nodebuf is oversized (due to H cache padding, vzoom, and compile-time guess on max univ len)
//            int wh = xfrlen; //TODO
            SDL_Size wh_bb(NUM_UNIV, H_PADDED), wh_txtr(XFRW_PADDED, xfrlen / XFRW_PADDED / sizeof(XFRTYPE)); //NOTE: txtr w is XFRW_PADDED, not XFRW
            debug(BLUE_MSG "bit bang xfr " << wh_bb << " node buf => " << wh_txtr << " txtr, limit " << std::min(wh_bb.h, wh_txtr.h) << " rows" ENDCOLOR_ATLINE(srcline2));
//TODO: bit bang here
            VOID memcpy(txtrbuf, nodebuf, xfrlen);
            wake(); //wake other threads/processes that are waiting to update more nodes
        }
    };
#endif
    const ScreenConfig* const m_cfg; //CAUTION: must be initialized before txtr and frame_time (below)
    const int UNIV_LEN; //= divup(m_cfg->vdisplay, vzoom);
    SDL_Size m_wh; //kludge: need param to txtr ctor; CAUTION: must occur before m_txtr; //(XFRW, UNIV_LEN);
//kludge: need class member for SIZEOF, so define it up here:
    /*txtr_bb*/ SDL_AutoTexture<XFRTYPE> m_txtr;
//    typedef double PERF_STATS[SIZEOF(txtr_bb::perf_stats) + 2]; //2 extra counters for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
//    using PERF_STATS = decltype(m_txtr.perf_stats); //double[SIZEOF(txtr_bb.perf_stats) + 2]; //2 extra counters for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
    double perf_stats[SIZEOF(m_txtr.perf_stats) + 2]; //2 extra counters for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
    struct FrameInfo
    {
        std::mutex mutex; //TODO: multiple render threads/processes
        std::thread::id owner; //window/renderer owner
        std::atomic<UNIVMAP> bits; //= 0; //one Ready bit for each universe; NOTE: need to init to avoid "deleted function" errors
        std::atomic<uint32_t> numfr; //= 0; //#frames rendered / next frame#
        std::atomic<uint32_t> nexttime; //= 0; //time of next frame (msec)
        uint64_t times[SIZEOF(perf_stats)]; //total init/sleep (sec), render (msec), upd txtr (msec), xfr txtr (msec), present/sync (msec)
        static const uint32_t FRINFO_MAGIC = 0x12345678;
        const uint32_t sentinel = FRINFO_MAGIC;
        bool isvalid() const { return (sentinel == FRINFO_MAGIC); }
public: //operators
        STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FrameInfo& that)
        {
            ostrm << "{" << &that;
            if (!&that) ostrm << " (NULL)";
            else
            {
                ostrm << ", bits " << std::hex << that.bits;
                ostrm << ", #fr " << that.numfr;
                ostrm << ", time " << that.nexttime;
                ostrm << ", magic " << std::hex << that.sentinel;
                if (that.isvalid()) ostrm << " INVALID";
            }
            return ostrm;
        }
    };
//    using FrameInfo_pad = char[divup(sizeof(FrameInfo), sizeof(NODEROW))]; //#extra NODEROWs needed to hold FrameInfo; kludge: "using" needs struct/type; use sizeof() to get value
//    union NODEROW_or_FrameInfo //kludge: allow cast to either type
//    {
//        FrameInfo frinfo;
//        NODEROW noderow;
//    }; //FrameInfo;
//    const int W = NODEBITS * 3 - 1, H = divup(m_cfg->vdisplay, vzoom), H_PADDED = cache_pad(H * sizeof(NODEVAL)) / sizeof(NODEVAL); //pixel buf sizes
    using NODEROW = NODEVAL[H_PADDED]; //"universe" of nodes
    using NODEBUF = NODEVAL[NUM_UNIV][H_PADDED]; //NODEROW[NUM_UNIV]; //caller-rendered 2D node buffer (pixels)
//    using NODEBUF_FrameInfo = NODEVAL[sizeof(FrameInfo_pad) + NUM_UNIV][H_PADDED]; //add extra row(s) for frame info
//combine nodes and frame info into one struct for simpler shm mgmt:
    struct NODEBUF_FrameInfo
    {
        union
        {
            FrameInfo frinfo; //put frame info < nodes to reduce chance of overwrites with faulty bounds checking
            char pad[cache_pad(sizeof(FrameInfo))]; //pad to reduce memory cache contention
        } frinfo_padded; //TODO: use unnamed element
        NODEBUF nodes;
    };
//    using XFRBUF = NODEVAL[W][UNIV_MAX]; //node buf bit banged according to protocol; staging area for txtr xfr to GPU
//    using NODEBUF_FrameInfo = NODEROW_or_FrameInfo[HPAD + 1]; //1 extra row to hold frame info
//    const std::function<int(void*)> shmfree_shim; //CAUTION: must be initialized before m_nodebuf
//    class NODEBUF_debug; //fwd ref for shmfree_shim
//    static void shmfree_shim(NODEBUF_debug* ptr, SrcLine srcline = 0) { shmfree(ptr, srcline); }
    const std::function<int(NODEBUF_FrameInfo*)>& shmfree_shim;
 //    using NODEBUF_debug_super = std::unique_ptr<NODEBUF_FrameInfo, std::function<int(NODEBUF_FrameInfo*)>>; //DRY kludge
    using NODEBUF_debug_super = std::unique_ptr<NODEBUF_FrameInfo, std::function<int(NODEBUF_FrameInfo*)>>; //DRY kludge
    class NODEBUF_debug: public NODEBUF_debug_super //std::unique_ptr<NODEBUF_FrameInfo, std::function<int(NODEBUF_FrameInfo*)>>
    {
        using deleter_t = std::function<int(NODEBUF_FrameInfo*)>; //shmfree signature
        using super = NODEBUF_debug_super; //std::unique_ptr<NODEBUF_FrameInfo, deleter_t>; //std::function<int(NODEBUF_FrameInfo*)>>; //NOTE: shmfree returns int, not void
public: //ctors/dtors
//        template <typename ... ARGS>
//        NODEBUF_debug(ARGS&& ... args):
//            super(std::forward<ARGS>(args) ...), //perfect fwd; avoids "no matching function" error in ctor init above
        NODEBUF_debug(NODEBUF_FrameInfo* ptr, deleter_t deleter): //std::function<void(void*)> deleter):
            super(ptr, deleter),
            nodes(this->get()->nodes), //CAUTION: "this" needed; compiler forgot about base class methods?
            frinfo(this->get()->frinfo_padded.frinfo) {}
public: //data members
        NODEBUF& nodes; //() { return this->get()->nodes; } //CAUTION: "this" needed; compiler forgot about base class methods?
        FrameInfo& frinfo; //() { return this->get()->frinfo; }
public: //operators
        STATIC friend std::ostream& operator<<(std::ostream& ostrm, const NODEBUF_debug& that)
        {
//            ostrm << "NODEBUF"; //<< my_templargs();
            ostrm << "{" << &that;
//            if (!that.frinfo.isvalid()) ostrm << " INVALID";
            ostrm << ": key " << std::hex << shmkey(that.get());
            ostrm << ", size " << shmsize(that.get()) << " (" << (shmsize(that.get()) / sizeof(NODEROW)) << " rows)";
            ostrm << ", #attch " << shmnattch(that.get());
            ostrm << " (existed? " << shmexisted(that.get()) << ")";
            ostrm << ", frinfo " << that.frinfo;
            ostrm << "}";
            return ostrm; 
        }
    };
//    using XFR = std::function<void(void* dest, const void* src, size_t len)>; //memcpy sig; //decltype(memcpy);
 protected: //data members
//    /*SDL_AutoTexture<XFRTYPE>*/ txtr_bb m_txtr;
//    AutoShmary<NODEBUF, false> m_nodebuf; //initialized to 0
//    std::unique_ptr<NODEROW, std::function<void(NODEROW*)>> m_nodebuf; //CAUTION: must be initialized before nodes, frinfo (below)
//public: //data members
//    PERF_STATS perf_stats; //double perf_stats[SIZEOF(m_txtr.perf_stats) + 2]; //2 extra counters for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
//    const std::function<int(void*)> shmfree_shim; //CAUTION: must be initialized before m_nodebuf
    NODEBUF_debug m_nodebuf; //CAUTION: must be initialized before nodes, frinfo (below)
//    AutoShmary<FrameInfo, false> m_frinfo; //initialized to 0
//    const ScreenConfig* const m_cfg; //CAUTION: must be initialized before frame_time (below)
public: //data members
    NODEBUF& /*NODEROW* const*/ /*NODEROW[]&*/ nodes;
    const double& frame_time;
    FrameInfo& frinfo;
    /*const*/ std::function<void(void*, const void*, size_t)> my_xfr_bb; //memcpy signature; TODO: try to use AutoTexture<>::XFR
//    SDL_AutoTexture<XFRTYPE>::XFR my_xfr_bb; //memcpy signature
//    using txtr_type = decltype(m_txtr);
//    txtr_type::XFR my_xfr_bb; //memcpy signature
public: //ctors/dtors:
    explicit GpuPort(SrcLine srcline = 0): GpuPort(0, 0, 1, srcline) {}
    GpuPort(int screen = 0, key_t shmkey = 0, int vzoom = 1, SrcLine srcline = 0):
        m_cfg(getScreenConfig(screen, NVL(srcline, SRCLINE))),
        UNIV_LEN(divup(m_cfg? m_cfg->vdisplay: UNIV_MAX, vzoom)),
        m_wh(XFRW, std::min(UNIV_LEN, UNIV_MAX)), //kludge: need to init before passing to txtr ctor below
//        my_xfr_bb(std::bind(/*GpuPort::*/xfr_bb, *this, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),
        m_txtr(SDL_AutoTexture<XFRTYPE>::create(NAMED{ _.wh = &m_wh; _.w_padded = XFRW_PADDED; _.screen = screen; _.srcline = NVL(srcline, SRCLINE); })),
        shmfree_shim(std::bind(shmfree, std::placeholders::_1, NVL(srcline, SRCLINE))),
        m_nodebuf(shmalloc_typed<NODEBUF_FrameInfo>(/*SIZEOF(nodes) + 1*/ 1, shmkey, NVL(srcline, SRCLINE)), shmfree_shim), //std::bind(shmfree, std::placeholders::_1, NVL(srcline, SRCLINE))),
        nodes(m_nodebuf.nodes), // /*static_cast<NODEROW*>*/ &(m_nodebuf.get())[1]), //use first row for frame info
        frinfo(m_nodebuf.frinfo), // /*static_cast<FrameInfo*>*/ (m_nodebuf.get())[0]),
        frame_time(m_cfg->frame_time(NVL(srcline, SRCLINE))),
        m_started(now()),
        m_srcline(srcline)
    {
//        if (sizeof(frinfo) > sizeof(nodes[0] /*NODEROW*/)) exc_hard(RED_MSG << "FrameInfo " << sizeof(frinfo) << " too large for node buf row " << sizeof(nodes[0] /*NODEROW*/) << ENDCOLOR_ATLINE(srcline));
        if (!m_cfg) exc_hard(RED_MSG "can't get screen config" ENDCOLOR_ATLINE(srcline));
//        frame_time = cfg->frame_time();
        if (UNIV_LEN > UNIV_MAX) exc_soft(RED_MSG "video settings " << *m_cfg << ", vzoom " << vzoom << " require univ len " << UNIV_LEN << " exceeding max " << UNIV_MAX << " allowed by compiled node buf" << ENDCOLOR_ATLINE(srcline));
        if (!&nodes || !&frinfo) exc_hard(RED_MSG "missing ptrs" ENDCOLOR_ATLINE(srcline));
//        const int UNIV_LEN = divup(m_cfg->vdisplay, vzoom);
//        const ScreenConfig* cfg = getScreenConfig(screen, NVL(srcline, SRCLINE));
//video config => xres, yres, clock => W = NODEBITS * 3 - 1, H = vres / vzoom, fps = clock / xres / yres
//        const int W = NODEBITS * 3 - 1; //, H = divup(m_cfg->vdisplay, vzoom), H_PADDED = cache_pad(H * sizeof(NODEVAL)) / sizeof(NODEVAL); //pixel buf sizes
//        SDL_Size wh(XFRW, UNIV_LEN); //, wh_padded(XFRW_PADDED, UNIV_LEN);
//        m_txtr = txtr_bb::create(NAMED{ _.wh = &wh; _.w_padded = XFRW_PADDED; _.screen = screen; _.srcline = NVL(srcline, SRCLINE); });
//        const double FPS = (double)m_cfg->dot_clock * 1e3 / m_
        if (!shmexisted(m_nodebuf.get())) frinfo.owner = thrid(); //creator thread = owner
        INSPECT(GREEN_MSG << "ctor " << *this, srcline);
    }
    virtual ~GpuPort() { INSPECT(RED_MSG << "dtor " << *this << ", lifespan " << elapsed(m_started) << " sec", m_srcline); }
//public: operators
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const GpuPort& that)
    {
        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
        ostrm << "GpuPort" << my_templargs();
//        ostrm << "\n{cfg " << *that.m_cfg;
        ostrm << "\n{" << &that;
        ostrm << ", fr time " << that.frame_time;
        ostrm << ", mine? " << that.ismine() << " (owner " << that.frinfo.owner << ")";
        ostrm << ", frinfo #fr " << that.frinfo.numfr << ", time " << that.frinfo.nexttime;
        ostrm << ", nodebuf " << that.m_nodebuf; //<< ", nodes at ofs " << sizeof(that.nodes[0]);
//        ostrm << ", xfrbuf[" << W << " x " << V
        ostrm << ", txtr " << that.m_txtr;
        ostrm << "}";
        return ostrm; 
    }
protected: //helpers
//    void bitbang(SrcLine srcline = 0) //m_nodebuf, m_xfrbuf)
//    {
////        using NODEBUF = NODEVAL[NUM_UNIV][H_PADDED]; //fx rendered into 2D node (pixel) buffer
//        SDL_Size wh(NUM_UNIV, m_txtr->wh.h); //use univ len from txtr; nodebuf is oversized (due to H cache padding, vzoom, and compile-time guess on max univ len)
//        debug(BLUE_MSG "bit bang " << wh << " node buf => " << m_txtr->wh << " txtr" ENDCOLOR_ATLINE(srcline));
//    }
    static void xfr_bb(GpuPort& gp, void* txtrbuf, const void* nodebuf, size_t xfrlen) //, SrcLine srcline2 = 0) //h * pitch(NUM_UNIV)
    {
        SrcLine srcline2 = 0; //TODO: bind from caller?
//            VOID memcpy(pxbuf, pixels, xfrlen);
//            SDL_Size wh(NUM_UNIV, m_cached.wh.h); //use univ len from txtr; nodebuf is oversized (due to H cache padding, vzoom, and compile-time guess on max univ len)
//            int wh = xfrlen; //TODO
        SDL_Size wh_bb(NUM_UNIV, H_PADDED), wh_txtr(XFRW_PADDED, xfrlen / XFRW_PADDED / sizeof(XFRTYPE)); //NOTE: txtr w is XFRW_PADDED, not XFRW
        debug(BLUE_MSG "bit bang xfr " << wh_bb << " node buf => " << wh_txtr << " txtr, limit " << std::min(wh_bb.h, wh_txtr.h) << " rows" ENDCOLOR_ATLINE(srcline2));
//TODO: bit bang here
        VOID memcpy(txtrbuf, nodebuf, xfrlen);
        gp.wake(); //wake other threads/processes that are waiting to update more nodes as soon as copy to txtr is done to maxmimum parallelism
    }
public: //methods
    void fill(NODEVAL color, const SDL_Rect* rect = 0, SrcLine srcline = 0)
    {
        SDL_Rect all(0, 0, m_wh.w, m_wh.h);
        if (!rect) rect = &all;
        debug(BLUE_MSG "fill " << *rect << " with 0x%x" ENDCOLOR_ATLINE(srcline), color);
        for (int x = rect->x, xlimit = rect->x + rect->w; x < xlimit; ++x)
            for (int y = rect->y, ylimit = rect->y + rect->h; y < ylimit; ++y)
                nodes[x][y] = color;
    }
//    FrameInfo* frinfo() const { return static_cast<FrameInfo*>(that.m_nodebuf.get()); }
    //void ready(SrcLine srcline = 0) { VOID ready(0, srcline); } //caller already updated frinfo->bits
    bool ready(UNIVMAP more = 0, SrcLine srcline = 0) //mark universe(s) as ready/rendered, wait for them to be processed; CAUTION: blocks caller
    {
        perf_stats[0] = m_txtr.perftime(); //caller's render time
        frinfo.bits |= more;
        if (!ismine())
        {
            /*auto*/uint32_t svnumfr = frinfo.numfr;
            if (frinfo.bits == ALL_UNIV) wake(); //wake owner to xfr all universes to GPU
            while (frinfo.numfr == svnumfr) wait(); //wait for GPU xfr to finish; loop compensates for spurious wakeups
            return (num_threads() > 1); //return to caller to render next frame
        }
        while (frinfo.bits != ALL_UNIV) wait(); //wait for remaining universes; loop handles spurious wakeups
        perf_stats[1] = m_txtr.perftime(); //ipc wait time
//all universes rendered; send to GPU:
//        bitbang/*<NUM_UNIV, UNIV_LEN,*/(NVL(srcline, SRCLINE)); //encode nodebuf -> xfrbuf
//        perf_stats[1] = m_txtr.perftime(); //bitbang time
        frinfo.bits = 0;
        frinfo.nexttime = ++frinfo.numfr * frame_time; //update fr# and timestamp of next frame before waking other threads
//        static void xfr_bb(void* txtrbuf, const void* nodebuf, size_t xfrlen, SrcLine srcline2 = 0) //h * pitch(NUM_UNIV)
//        static XFR xfr_bb_shim(std::bind(xfr_bb, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, NVL(srcline, SRCLINE))),
        VOID m_txtr.update(NAMED{ _.pixels = /*&m_xfrbuf*/&nodes[0][0]; _.perf = &perf_stats[SIZEOF(perf_stats) - 4]; _.xfr = my_xfr_bb; _.srcline = NVL(srcline, SRCLINE); }); //, true, SRCLINE); //, sizeof(myPixels[0]); //W * sizeof (Uint32)); //no rect, pitch = row length
        for (int i = 0; i < SIZEOF(perf_stats); ++i) frinfo.times[i] += perf_stats[i];
    }
//inter-thread/process sync:
//SDL is not thread-safe; only owner thread is allowed to render to window
//TODO: use ipc?; causes extra overhead but avoids polling (aside from extraneous wakeups)
//sluz(m_frinfo->bits, NVL(srcline, SRCLINE)); //wait until frame is rendered
//            usleep(2000); //2 msec; wait for remaining universes to be rendered (by other threads or processes)
//TODO        if (NUM_THREAD > 1) wake_others(NVL(srcline, SRCLINE)); //let other threads/processes overwrite nodebuf while xfrbuf is going to GPU
    static inline auto /*std::thread::id*/ thrid() { return std::this_thread::get_id(); }
    bool ismine() const { return (thrid() == frinfo.owner); }
    int num_threads() { return shmnattch(m_nodebuf.get()); }
    static const UNIVMAP ALL_UNIV = (1 << NUM_UNIV) - 1;
    void wait()
    {
        if (num_threads() < 2) return; //no other threads/processes can wake me; get fresh count in case #threads changed
        if (!ismine()) waiters().push_back(thrid()); //this);
#if 0 //TODO
        sleep(); //wait for another process/thread to wake me
#endif
    }
    void wake()
    {
        if (num_threads() < 2) return; //no other threads/processes to wake; get fresh count in case #threads changed
#if 0 //TODO
        if (!ismine()) wake(frinfo.owner);
        else wake(waiters());
#endif
    }
public: //named arg variants
        struct CtorParams
        {
            int screen = 0;
            key_t shmkey = 0;
            int vzoom = 1; //for DEV/debug to screen
            SrcLine srcline = 0;
        };
        static struct CtorParams& ctor_params() { static struct CtorParams cp; return cp; } //static decl wrapper
    template <typename CALLBACK>
    GpuPort(CALLBACK&& named_params): GpuPort(unpack(ctor_params(), named_params), Unpacked{}) {} //perfect fwd to arg unpack
protected: //named arg variants
    GpuPort(const CtorParams& params, Unpacked): GpuPort(params.screen, params.shmkey, params.vzoom, params.srcline) {}
protected: //data members
//    /*SDL_AutoTexture<XFRTYPE>*/ txtr_bb m_txtr;
//    AutoShmary<NODEBUF, false> m_nodebuf; //initialized to 0
//    std::unique_ptr<NODEROW, std::function<void(NODEROW*)>> m_nodebuf; //CAUTION: must be initialized before nodes, frinfo (below)
//public: //data members
//    PERF_STATS perf_stats; //double perf_stats[SIZEOF(m_txtr.perf_stats) + 2]; //2 extra counters for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
//    const std::function<int(void*)> shmfree_shim; //CAUTION: must be initialized before m_nodebuf
//    NODEBUF_debug m_nodebuf; //CAUTION: must be initialized before nodes, frinfo (below)
//    AutoShmary<FrameInfo, false> m_frinfo; //initialized to 0
//    const ScreenConfig* const m_cfg; //CAUTION: must be initialized before frame_time (below)
//public: //data members
//    NODEBUF& /*NODEROW* const*/ /*NODEROW[]&*/ nodes;
//    const double& frame_time;
//    FrameInfo& frinfo;
protected: //data members
//    static int m_count = 0;
//    XFRBUF m_xfrbuf; //just use txtr
    static std::vector<std::thread::id>& waiters() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::vector<std::thread::id> m_all;
        return m_all;
    }
//    const int H;
//    const ScreenConfig cfg;
    const elapsed_t m_started;
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
    static std::string& my_templargs() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::string m_templ_args(TEMPL_ARGS); //, dummy = m_templ_args.append("\n"); //only used for debug msgs
        return m_templ_args;
    }
};


#if 0
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
#endif


#if 0
#define GpuPort GpuPort_other
class GpuPort
{
//    using PIXEL = Uint32;
//    using UnivPadLen = cache_pad(H * sizeof(PIXEL)); //univ buf padded up to cache size (for better memory performance while rendering pixels)
//    using H_PADDED = UnivPadLen / sizeof(PIXEL); //effective univ len
//    using ROWTYPE = NODEVAL[H_PADDED]; //padded out to cache row size
//    using BUFTYPE = ROWTYPE[W];
//    typedef ROWTYPE NODEBUFTYPE[W]; //2D W x H_PADDED
    using NODEBUFTYPE = NODEVAL[W][H_PADDED]; //2D W x H_PADDED
public: //ctors/dtors
    GpuPort(int clock, key_t key = 0, SrcLine srcline = 0): Clock(clock), FPS(clock / 3 / BPN / H ), //NUM_UNIV(W), UNIV_LEN(H), 
//CAUTION: dcl order determines init order, not occurrence order here!
        inout("GpuPort init/exit", SRCLINE), 
//TODO: store > 1 frame in texture and use rect to select?
        m_txtr(SDL_AutoTexture::create(NAMED{ _.w = 3 * W; /*_.h = H;*/ _.srcline = NVL(srcline, SRCLINE); })), //always use as-is
//        inout2("order check 2", SRCLINE), 
        m_shmbuf(1, key, NVL(srcline, SRCLINE)), 
        Hclip({0, 0, 3 * W - 1, H}), //CRITICAL: clip last col (1/3 pixel) so it overlaps with H-sync
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
        ostrm << ", " << W /*NUM_UNIV*/ << " x " << H /*UNIV_LEN*/ << " (" << (W * H) << ") => " << H_PADDED << " (" << (W * H_PADDED) << ")";
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
//    const size_t H_PADDED = UnivPadLen / sizeof(PIXEL); //effective univ len
//    PIXEL (&pixels)[W][H_PADDED]; //CAUTION: pivoted so nodes within each univ (column) are adjacent (for better cache performance)
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
//            for (int y = 0; y < H; ++y) //CAUTION: leaves gaps (H_PADDED vs. H)
//                pixels[x][y] = color;
//        if (&nodes[0][W * H_PADDED] != &nodes[W][0]) exc("&nodes[0][%d x %d = %d] %p != &nodes[%d][0] %p", W, H_PADDED, W * H_PADDED, &nodes[0][W * H_PADDED], W, &nodes[W][0]);
        debug(BLUE_MSG << *this << ENDCOLOR);
        debug(BLUE_MSG "&nodes[0][0] = %p, &[1][0] = %p, [%d][0] = %p" ENDCOLOR, &nodes[0][0], &nodes[1][0], W, &nodes[W][0]);
        for (int i = 0; i < W * H_PADDED; ++i) nodes[0][i] = color; //NOTE: fills in H..H_PADDED gap as well for simplicity
//checknodes(SRCLINE);
//        elapsed += elapsed_msec();
//        debug(BLUE_MSG << "fill all %d x %d = %d pixels with 0x%x took %ld msec" << ENDCOLOR_ATLINE(srcline), W, H, W * H, elapsed);
    }
#if 0 //TODO?
    void dirty(ROWTYPE& univ, SrcLine srcline = 0) //PIXEL (&univ)[H_PADDED]
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
protected: //helpers
//PIXEL[H_PADDED]* const& pixels;
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
protected: //data members
//    Uint32* m_shmptr;
//    int m_clock;
//    const size_t m_univlen; //univ buf padded up to cache size (for better memory performance while rendering pixels)
    InOutDebug inout; //put this before AutoTexture
    SDL_Rect Hclip;
    SDL_AutoTexture m_txtr;
    Uint32 m_pixbits[3 * W * H];
//to look at shm: ipcs -m 
//detailde info:  ipcs -m -i <shmid>
//to delete shm: ipcrm -M <key>
    AutoShmary<NODEBUFTYPE, false> m_shmbuf; //PIXEL[H_PADDED] //CAUTION: must occur before nodes
public:
//    InOutDebug inout3; //inout4, inout2, inout3; //check init order
//wrong!    /*BUFTYPE&*/ ROWTYPE* const& /*const*/ nodes; //CAUTION: pivoted so nodes within each univ (column) are adjacent (for better cache performance); NOTE: "const" ref needed for rvalue
//    ROWTYPE (&nodes)[W]; //ref to array of W ROWTYPEs; CAUTION: "()" required
    NODEBUFTYPE& nodes; //CAUTION: must come after m_shmbuf (init order)
protected:
//    InOutDebug inout3; //inout4, inout2, inout3; //check init order
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
    static std::string& my_templargs() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::string m_templ_args(TEMPL_ARGS), dummy = m_templ_args.append("\n"); //only used for debug msgs
        return m_templ_args;
    }
};
#endif


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
void unit_test(ARGS& args)
{
    int screen = 0; //default
    for (auto arg : args) //int i = 0; i < args.size(); ++i)
        if (!arg.find("-s")) screen = atoi(arg.substr(2).c_str());
//    const int NUM_UNIV = 4, UNIV_LEN = 5; //24, 1111
//    const int NUM_UNIV = 30, UNIV_LEN = 100; //24, 1111
//    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE, dimARGB(0.75, WHITE), dimARGB(0.25, WHITE)};

//    GpuPort<NUM_UNIV, UNIV_LEN/*, raw WS281X*/> gp(2.4 MHz, 0, SRCLINE);
    GpuPort<> gp(NAMED{ _.screen = screen; _.vzoom = 1; SRCLINE; }); //NAMED{ .num_univ = NUM_UNIV, _.univ_len = UNIV_LEN, SRCLINE});
//    Uint32* pixels = gp.Shmbuf();
//    Uint32 myPixels[H][W]; //TODO: [W][H]; //NOTE: put nodes in same universe adjacent for better cache performance
//    auto& pixels = gp.pixels; //NOTE: this is shared memory, so any process can update it
//    debug(CYAN_MSG "&nodes[0] %p, &nodes[0][0] = %p, sizeof gp %zu, sizeof nodes %zu" ENDCOLOR, &gp.nodes[0], &gp.nodes[0][0], sizeof(gp), sizeof(gp.nodes));
    debug(CYAN_MSG << gp << ENDCOLOR);
    SDL_Delay(2 sec);

    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        VOID gp.fill(palette[c], NULL, SRCLINE);
        bool result = gp.ready(0xffffff, SRCLINE);
        debug(CYAN_MSG << timestamp() << "color 0x%x, next fr# %d, next time %f, ready? %d, perf: [%4.3f s, %4.3f ms, %4.3f ms, %4.3f ms, %4.3f ms, %4.3f ms]" ENDCOLOR,
            palette[c], gp.frinfo.numfr, gp.frinfo.nexttime, result, gp.frinfo.times[0] / gp.frinfo.numfr, gp.frinfo.times[1] / gp.frinfo.numfr, gp.frinfo.times[2] / gp.frinfo.numfr, gp.frinfo.times[3] / gp.frinfo.numfr, gp.frinfo.times[4] / gp.frinfo.numfr, gp.frinfo.times[5] / gp.frinfo.numfr);
        SDL_Delay(1 sec);
    }

#if 0
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
#endif
    debug(BLUE_MSG << "finish" << ENDCOLOR);
//    return 0;
}

#endif //def WANT_UNIT_TEST