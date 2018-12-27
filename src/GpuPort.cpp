//TODO:
//MULTI_PROC thrinx()
//DEBUG=* => C++
//cache pad nodebq
//expose debug() to napi

////////////////////////////////////////////////////////////////////////////////
////
/// GpuPort: a Node.js add-on with simple async callback to wrap RPi GPU as a parallel port
//

//This is a Node.js add-on to display a rectangular grid of pixels (a texture) on screen using SDL2 and hardware acceleration (via GPU).
//In essence, the RPi GPU provides a 24-bit high-speed parallel port with precision timing.
//Optionally, OpenGL and GLSL shaders can be used for generating effects.
//In dev mode, an SDL window is used.  In live mode, full screen is used (must be configured for desired resolution).
//Screen columns generate data signal for each bit of the 24-bit parallel port.
//Without external mux, there can be 24 "universes" of external LEDs to be controlled.  Screen height defines max universe length.
//
//Copyright (c) 2015, 2016, 2017, 2018 Don Julien, djulien@thejuliens.net


//to build:
//git clone (this repo); cd into
//npm test  -or-  npm install --verbose  -or-  npm run build  -or-  node-gyp rebuild --verbose
//NOTE: if errors, try manually:  "npm install nan@latest --save"  -or-  npm install -g node-gyp
//or try uninstall/re-install node:
//https://stackoverflow.com/questions/11177954/how-do-i-completely-uninstall-node-js-and-reinstall-from-beginning-mac-os-x
//$node --version
//$nvm deactivate
//$nvm uninstall v11.1.0

//to debug:
//1. compile first
//2. gdb -tui node; run ../; layout split

//to get corefiles:
// https://stackoverflow.com/questions/2065912/core-dumped-but-core-file-is-not-in-current-directory
// echo "[main]\nunpackaged=true" > ~/.config/apport/settings
// core files end up in /var/crash
// mkdir ~/.core-files
// rm -f  ~/.core-files/*; apport-unpack /var/crash/* ~/.core-files   #makes them readable by gdb
// load into gdb:  gdb ./unittest ~/.core-files/CoreDump

//vsync:
//NOTE: need sudo or be a member of video group to use this
// ls -l /dev/fb0
//list groups: more /etc/group
//which groups am i a member of:  groups
//add user to group: usermod -aG video "$USER"
//**need to log out and back in or "su username -"; see https://unix.stackexchange.com/questions/277240/usermod-a-g-group-user-not-work

//example/setup info:
//** https://github.com/nodejs/node-addon-examples
//https://github.com/1995parham/Napi101
//https://www.nearform.com/blog/the-future-of-native-amodules-in-node-js/
//https://hackernoon.com/n-api-and-getting-started-with-writing-c-addons-for-node-js-cf061b3eae75
//or https://github.com/nodejs/node-addon-examples/tree/master/1_hello_world
//or https://medium.com/@atulanand94/beginners-guide-to-writing-nodejs-addons-using-c-and-n-api-node-addon-api-9b3b718a9a7f
//https://github.com/master-atul/blog-addons-example

//framebuffer info:
// ** https://www.google.com/search?client=ubuntu&channel=fs&q=how+to+use+hardware+acceleration+to+draw+image+to+framebuffer+linux&ie=utf-8&oe=utf-8
// ** https://github.com/denghongcai/node-framebuffer
// https://github.com/DirectFB/directfb
// blit perf: https://theosperiment.wordpress.com/2015/10/07/blitting-around/
// https://github.com/bitbank2/bbgfx


//Node.js event loop and other bkg info:
//https://stackoverflow.com/questions/10680601/nodejs-event-loop
// * "the Event Loop should orchestrate client requests, not fulfill them itself."

//TODO: JSONStream: https://github.com/dominictarr/JSONStream

//to restart wifi on ununtu:
//sudo service network-manager restart

#include <sstream> //std::ostringstream
#include <utility> //std::forward<>
#include <type_traits> //underlying_type<>, remove_reference<>
#include <functional> //std::bind()
#include <string> //std::string
#include <map> //std::map<>
#include <limits.h> //INT_MAX
#include <bitset> //std::bitset<>

#define MAX_DEBUG_LEVEL  100 //set this before debug() is included via nested #includes
#include "str-helpers.h" //unmap(), NNNN_hex(), vector_cxx17<>
//#include "thr-helpers.h" //thrinx()
//can't get rid of flicker; use framebuf instead: 
#include "sdl-helpers.h" //AutoTexture, Uint32, elapsed(), now()
//#include "fb-helpers.h" //AutoTexture, SDL shims
#include "rpi-helpers.h" //ScreenConfig, getScreenConfig()
//#include "elapsed.h" //elapsed_msec(), timestamp(), now()
//#include "msgcolors.h" //MSG_*, ATLINE()
//#include "debugexc.h" //debug(), exc(), INSPECT()
//#include "ostrfmt.h" //FMT()
#include "logging.h"
#include "rgb-helpers.h" //must come after sdl-helpers

//which Node API to use?
//V8 is older, requires more familiarity with V8
//NAPI is C-style api and works ok
//Node Addon API is C++ style but seems to have some issues
//use NAPI for now
#define USE_NAPI
//#define USE_NODE_ADDON_API //TODO: convert to newer API
//#define USE_NAN
//NAPI perf notes: https://github.com/nodejs/node/issues/14379
#include "napi-helpers.h" //napi_thingy, NAPI_OK(), NAPI_exc, etc


#if __cplusplus < 201103L
 #pragma message("CAUTION: this file probably needs c++11 or later to compile correctly")
#endif


#define WANT_REAL_CODE
//#define WANT_BROKEN_CODE
//#define WANT_SIMPLE_EXAMPLE
//#define WANT_CALLBACK_EXAMPLE

#define MAX_DEBUG_LEVEL  99 //1

#define UNUSED(thing)  //(void)thing //avoid compiler warnings

#ifndef SIZEOF_2D
 #define SIZEOF_2D(thing)  (SIZEOF(thing) * SIZEOF(thing[0]))
#endif


//accept variable # 2-4 macro args:
#ifndef UPTO_2ARGS
 #define UPTO_2ARGS(skip1, skip2, use3, ...)  use3
#endif
#ifndef UPTO_3ARGS
 #define UPTO_3ARGS(skip1, skip2, skip3, use4, ...)  use4
#endif
#ifndef UPTO_4ARGS
 #define UPTO_4ARGS(skip1, skip2, skip3, skip4, use5, ...)  use5
#endif


#if 0 //moved to shmalloc.h
//polyfill c++17 methods:
template <typename TYPE, typename super = std::vector<TYPE>> //2nd arg to help stay DRY
class vector_cxx17: public super //std::vector<TYPE>
{
//    using super = std::vector<TYPE>;
public:
    template <typename ... ARGS>
    TYPE& emplace_back(ARGS&& ... args)
    {
        super::emplace_back(std::forward<ARGS>(args) ...); //perfect fwd
        return super::back();
    }
};
#endif


////////////////////////////////////////////////////////////////////////////////
////
/// Gpu Port main code:
//

#ifdef WANT_REAL_CODE
 #ifdef SRC_NODE_API_H_ //USE_NAPI

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
#define MHz  *1000000 //NOTE: can't use 1e6; must be int for template param

#define CLOCK_CONSTRAINT_2ARGS(hres, nodetime)  ((hres) / (nodetime))
#define CLOCK_CONSTRAINT_3ARGS(hres, vres, fps)  ((hres) * (vres) * (fps))
#define CLOCK_CONSTRAINT(...)  UPTO_3ARGS(__VA_ARGS__, CLOCK_CONSTRAINT_3ARGS, CLOCK_CONSTRAINT_2ARGS, CLOCK_CONSTRAINT_1ARG) (__VA_ARGS__)

#define HRES_CONSTRAINT_2ARGS(clock, nodetime)  ((clock) * (nodetime))
#define HRES_CONSTRAINT_3ARGS(clock, vres, fps)  ((clock) / (vres) / (fps))
#define HRES_CONSTRAINT(...)  UPTO_3ARGS(__VA_ARGS__, HRES_CONSTRAINT_3ARGS, HRES_CONSTRAINT_2ARGS, HRES_CONSTRAINT_1ARG) (__VA_ARGS__)

#define NODETIME_CONSTRAINT(clock, hres)  ((hres) / (clock))
#define VRES_CONSTRAINT(clock, hres, fps)  ((clock) / (hres) / (fps))
#define FPS_CONSTRAINT(clock, hres, vres)  ((clock) / (hres) / (vres))


#include "shmalloc.h" //AutoShmary<>, cache_pad(), WithShmHdr<>
//template<typename TYPE>
//static constexpr size_t cache_pad_typed(size_t count) { return cache_pad(count * sizeof(TYPE)) / sizeof(TYPE); }
//#undef cache_pad
//#define cache_pad  cache_pad_typed //kludge; override macro


//typedef uint32_t elapsed_t; //20 bits is enough for 5-minute timing using msec; 32 bits is plenty

//inline size_t my_offsetof(const uint8_t* from, const void* to)
//{
////static_cast<intptr_t>(to) - static_cast<intptr_t>(from); }
//    intptr_t fromm = from, too = to;
//    return too - fromm;
//}


#define IFDEBUG(yes_stmt, no_stmt)  no_stmt //yes_stmt
#pragma message(IFDEBUG(YELLOW_MSG "Debug sizing ON" ENDCOLOR_NOLINE, GREEN_MSG "Debug sizing off" ENDCOLOR_NOLINE))

//shm struct shared by bkg gpu wker thread and main/renderer threads:
//CAUTION: don't store ptrs; they won't be valid in other procs
//all ipc and sync occurs via this struct to allow procs/threads to run at max speed and avoid costly memory xfrs
//#define IFDEBUG(yes_stmt, no_stmt)  no_stmt
struct ShmData
{
//    static const bool CachedWrapper = false; //true; //BROKEN; leave turned OFF
//probably a little too much compile-time init, but here goes ...
    using NODEVAL = Uint32; //data type for node colors (ARGB)
    static const napi_typedarray_type GPU_NODE_type = napi_uint32_array; //NOTE: must match NODEVAL type
    using XFRTYPE = Uint32; //data type for bit banged node bits (ARGB)
    using TXTR = SDL_AutoTexture<XFRTYPE, 0, true>; //false>;
    static const int CACHELEN = 64; //RPi 2/3 reportedly have 32/64 byte cache rows; use larger size to accomodate both
//settings that must match h/w:
//TODO: move some of this to run-time or extern #include
    static const int IOPINS = 24; //total #I/O pins available (h/w dependent); also determined by device overlay
    static const int HWMUX = 0; //#I/O pins (0..23) to use for external h/w mux
//derived settings:
    static const int NUM_UNIV = IFDEBUG(3, (1 << HWMUX) * (IOPINS - HWMUX)); //max #univ with/out external h/w mux
//settings that must match (cannot exceed) video config:
//put 3 most important constraints first, 4th will be dependent on other 3
//default values are for my layout
    static const int CLOCK = 52 MHz; //pixel clock speed (constrained by GPU)
    static const int HTOTAL = 1536; //total x res including blank/sync (might be contrained by GPU); 
    static const int FPS = 30; //target #frames/sec
//derived settings:
    static const int UNIV_MAXLEN = VRES_CONSTRAINT(CLOCK, HTOTAL, FPS); //max #nodes per univ; above values give ~1128
//    static const int UNIV_MAXLEN_pad = IFDEBUG(4, cache_pad<NODEVAL>(UNIV_MAXLEN_raw)); //1132 for above values; padded for better memory cache performance
//    static const SDL_Size max_wh(NUM_UNIV, UNIV_MAXLEN_pad);
    typedef typename std::conditional<(NUM_UNIV <= 32), uint32_t, std::bitset<NUM_UNIV>>::type MASK_TYPE;
//    using MASK_TYPE = uint32_t; //using UNIV_MASK = XFRTYPE; //cross-univ bitmaps
//settings determined by s/w:
    static const int NODEBITS = 24; //# bits to send for each WS281X node (protocol dependent)
    static const int BIT_SLICES = NODEBITS * 3; //divide each node data bit into 1/3s (last 1/3 of last node bit will overlap hsync)
    static const unsigned int NODEVAL_MSB = 1 << (NODEBITS - 1);
    static const int BRIGHTEST = pct(50/60);
//    static const unsigned int NODEVAL_MASK = 1 << NODEBITS - 1;
    static const MASK_TYPE UNIV_MASK = (1 << NUM_UNIV) - 1;
    static const MASK_TYPE ALL_UNIV = UNIV_MASK; //NODEVAL_MASK;
    static const MASK_TYPE NOT_READY = ALL_UNIV >> (NUM_UNIV / 2); //turn off half the universes to use as intermediate value
//    std::unique_ptr<NODEVAL> m_nodes; //define as member data to avoid WET defs needed for class derivation; NOTE: must come before depend refs below; //NODEBUF_FrameInfo, NODEBUF_deleter>; //DRY kludge
//    using SYNCTYPE = BkgSync<MASK_TYPE, true>;
    static const int QUELEN = IFDEBUG(2, 4); //#render queue entries (circular)
    static const int SPARELEN = IFDEBUG(6, 64);
    static const uint32_t VALIDCHK = 0xf00d1234;
    static const int VERSION = 0x001812; //0.18.12
//    static const key_t SHMKEY = 0xfeed0000 | NNNN_hex(UNIV_MAXLEN_pad); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
public: //dependent types:
//data format (protocol) selector:
//currently only WS281X and a dev mode are supported, but new fmts/protocols could be added
//    enum class Protocol: int32_t { NONE = 0, DEV_MODE, WS281X, CANCEL = -1}; //combine bkg wker control with protocol selection
    struct Protocol //kludge: enum class can't have members, so use struct to encapsulate operators and methods
    {
//        using base_type = int32_t;
//CAUTION: base type must be compatible with napi_value types
//broken        enum class Enum: int32_t { NONE = 0, DEV_MODE, WS281X, CANCEL = -1}; //CANCEL combines bkg wker control with protocol selection
//        static const Enum NONE = Enum::NONE, DEV_MODE = Enum::DEV_MODE, WS281X = Enum::WS281X, CANCEL = Enum::CANCEL; //kludge: make it look more like an enum; don't require caller to use "Enum::"
        enum { NONE = 0, DEV_MODE, WS281X, CANCEL = -1}; //CANCEL combines bkg wker control with protocol selection
//        static const val_type NONE = 0, DEV_MODE = 1, WS281X = 2, CANCEL = -1;
        /*Enum*/ int32_t value;
        using Enum = decltype(value);
        using base_type = Enum; //std::underlying_type<Enum>::type;
#if 0
        static const std::map<Enum /*base_type*/, const char*>& Names() //kludge: gcc won't allow static member init so wrap in static function
        {
            static const std::map<Enum /*base_type*/, const char*> names =
            {
                {Enum::NONE, "NONE"},
                {Enum::DEV_MODE, "DEV MODE"},
                {Enum::WS281X, "WS281X"},
                {Enum::CANCEL, "CANCELLED"},
            };
            return names;
        }
#else
//        using wrap_type = str_map<Enum, const char*>;
        using wrap_type = std::map<Enum, const char*>; //kludge: can't use "," in macro param
//kludge: gcc won't allow static member init so wrap in static function/data member:
        static STATIC_WRAP(wrap_type, Names, =
        {
//NOTE: strings should be valid Javascript names
            {/*Enum::*/NONE, "NONE"},
            {/*Enum::*/DEV_MODE, "DEV_MODE"},
            {/*Enum::*/WS281X, "WS281X"},
            {/*Enum::*/CANCEL, "CANCELLED"},
        });
#endif
    public: //ctor/dtor
    //NOTE: "Foo f = 42" equiv to "Foo f (Foo(42))"; non-explicit copy ctor needed; https://stackoverflow.com/questions/372665/c-instance-initialization-syntax
        /*explicit*/ Protocol(const Enum& that): value(that) {} //used by FrameControl
        /*explicit*/ Protocol(const Protocol& that): value(that.value) {} //used by xfr_bb
//initializer list example: https://en.cppreference.com/w/cpp/utility/initializer_list
//        Protocol(std::initializer_list<Enum> il): value(il[0]) {} //kludge: can't get assignment initializer to work
//        ~Protocol() {}
    public: //operators
        static inline Enum cast(base_type val) { return static_cast<Enum>(val); }
        static inline base_type uncast(Enum val) { return static_cast<base_type>(val); }
        inline operator Enum() const { return cast(value); }
//        operator const char*() const { return NVL(unmap(ProtocolNames, that), "??PROTOCOL??"); }
        inline Protocol& operator=(const Protocol& that) { value = that.value; return *this; } //needed for inline init (used by NAPI_open)
        inline Protocol& operator=(const Enum& rhs) { value = rhs; return *this; } //needed for inline init (used by NAPI_open)
//        inline Protocol& operator=(base_type rhs) { value = cast(rhs); return *this; } //needed for inline init (used by FrontControl, xfr_bb)
//        inline bool operator==(const Protocol& rhs) { return (value == rhs.value); } //used by xfr_bb
//        inline bool operator!=(const Protocol& rhs) { return !(value == rhs.value); }
//        inline operator const char*() const { return toString(); } //causes amb with op!=
        const char* toString() const { return NVL(unmap(static_Names(), value), "??PROTOCOL??"); }
        STATIC friend std::ostream& operator<<(std::ostream& ostrm, const Protocol& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        {
            return ostrm << that.toString();
        }
//kludge: supply supporting operators; class gets "incomplete" errors without; https://stackoverflow.com/questions/42437155/how-to-stdmapenum-class-stdstring
        inline bool operator <(/*const Enum lhs,*/ const Enum rhs) { return uncast(value) < uncast(rhs); }
        inline bool operator >(/*const Enum lhs,*/ const Enum rhs) { return uncast(value) > uncast(rhs); }
    public: //NAPI methods
        static inline Protocol* my(void* ptr) { return static_cast<Protocol*>(ptr); }
        static inline /*uint32_t*/ napi_value getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, uncast(my(ptr)->value), napi_thingy::Int32{}); }
        static void setter(const napi_thingy& newval, void* ptr) { my(ptr)->value = cast(newval.as_int32(true)); }
//        static napi_value my_exports(napi_env env, napi_value exports)
        static inline napi_value my_exports(napi_env env) { return my_exports(env, napi_thingy(env, napi_thingy::Object{})); }
        static napi_value my_exports(napi_env env, const napi_value& retval)
        {
//            exports = module_exports(env, exports); //include previous exports
//            napi_thingy retval(env, napi_thingy::Object{});
            vector_cxx17<my_napi_property_descriptor> props;
    //        add_prop("NONE", static_cast<int32_t>(Protocol::NONE))(enum_props.emplace_back()); //(*pptr++);
    //        add_prop("DEV_MODE", static_cast<int32_t>(Protocol::DEV_MODE))(enum_props.emplace_back()); //(*pptr++);
    //        add_prop("WS281X", static_cast<int32_t>(Protocol::WS281X))(enum_props.emplace_back()); //(*pptr++);
    //        add_prop("CANCEL", static_cast<int32_t>(Protocol::CANCEL))(enum_props.emplace_back()); //(*pptr++);
//            for (auto& it: static_Names) //it.first, it.second
            static const decltype(static_Names()) Names = static_Names();
            for (auto it = Names.cbegin(); it != Names.cend(); ++it) //it->first, it->second
                add_prop_uint32(it->second, Protocol::uncast(it->first))(props.emplace_back()); //(*pptr++);
    //        debug(9, "add %d props", props.size());
//            !NAPI_OK(napi_define_properties(env, retval, props.size(), props.data()), "export protocol enum props failed");
//            napi_thingy more_retval(env, retval);
//            more_retval += props;
//            return more_retval;
            return napi_thingy(env, retval) += props;
        }
    };
//provide manifest info so other procs can find data of interest within shm seg:
    struct ManifestType
    {
//CAUTION: don't make these static; they need to be placed as members directly within object instance (in memory)
//NOTE: force storage types here so sizes don't depend on compiler or arch; Intel was using a mix of uin64_t and 32, making it awkward for external readers
//TODO? sizeof(key_t), sizeof(uint32_t), sizeof(size_t), sizeof(double);
        const /*key_t*/ uint32_t shmkey = FramebufQuent::SHMKEY, shmlen = sizeof(ShmData); //shmkey demoted to here for completeness
        const /*size_t*/ uint32_t frctl_ofs = offsetof(ShmData, m_frctl), frctl_len = sizeof(m_frctl);
        const /*size_t*/ uint32_t spares_ofs = offsetof(ShmData, m_spare), spares_len = sizeof(m_spare);
        const /*size_t*/ uint32_t nodebufs_ofs = offsetof(ShmData, m_fbque), nodebufs_len = sizeof(m_fbque);
//        const /*size_t*/ uint32_t msgs_ofs = offsetof(ShmData, m_msglog), msgs_len = sizeof(m_msglog);
    public: //operators
        STATIC friend std::ostream& operator<<(std::ostream& ostrm, const ManifestType& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        {
//            HERE(6);
//            ostrm << "manifest"; //<< my_templargs();
//            ostrm << "{" << commas(sizeof(that)) << ":" << &that;
//        if (!&that) return ostrm << " NO DATA}";
//        if (!that.isvalid()) return ostrm << " INVALID}";
            ostrm << "{shm " << commas(that.shmlen) << ":" << std::hex << that.shmkey << std::dec;
            ostrm << ", frctl " << commas(that.frctl_len) << ":+" << commas(that.frctl_ofs);
            ostrm << ", spares " << commas(that.spares_len) << ":+" << commas(that.spares_ofs);
            ostrm << ", nodebufs " << commas(that.nodebufs_len) << ":+" << commas(that.nodebufs_ofs);
//            ostrm << ", msgs " << commas(that.msgs_len) << ":+" << commas(that.msgs_ofs);
            return ostrm << "}";
        }
    public: //NAPI methods
//exported manifest allows other procs to find other non-exported data:
//        napi_value my_exports(napi_env env) //vector_cxx17<my_napi_property_descriptor>& props, napi_env env)
        /*static*/ napi_value my_exports(napi_env env) { return my_exports(env, napi_thingy(env, napi_thingy::Object{})); }
        /*static*/ napi_value my_exports(napi_env env, const napi_value& retval)
        {
//            exports = module_exports(env, exports); //include previous exports
//            napi_thingy retval(env, napi_thingy::Object{});
//        add_prop_uint32(SHMKEY)(props.emplace_back()); //(*pptr++);

            vector_cxx17<my_napi_property_descriptor> props;
            add_prop_uint32(shmkey)(props.emplace_back());
            add_prop_uint32(shmlen)(props.emplace_back());
            add_prop_uint32(frctl_ofs)(props.emplace_back());
            add_prop_uint32(frctl_len)(props.emplace_back());
            add_prop_uint32(spares_ofs)(props.emplace_back());
            add_prop_uint32(spares_len)(props.emplace_back());
            add_prop_uint32(nodebufs_ofs)(props.emplace_back());
            add_prop_uint32(nodebufs_len)(props.emplace_back());
//            add_prop_uint32(msgs_ofs)(props.emplace_back());
//            add_prop_uint32(msgs_len)(props.emplace_back());
            add_prop_uint32("sizeof_float", sizeof(double))(props.emplace_back()); //for debug frame_time; NOTE: not present in shm, just JS retval
//            debug(9, "add %d props", props.size());
//            !NAPI_OK(napi_define_properties(env, retval, props.size(), props.data()), "export manifest props failed");
//            napi_thingy more_retval(env, retval);
//            more_retval += props;
//            return more_retval;
            return napi_thingy(env, retval) += props;
        }
    };
//overall frame control info:
//this is state info that is not frame-specific
    /*alignas(CACHELEN)*/ struct FrameControl //ShmInfo
    {
//NOTE: force storage types here so sizes don't depend on compiler or arch; Intel was using a mix of uin64_t and 32, making it awkward for external readers
        CONST int32_t screen;
        CONST SDL_Size wh; // /*(0, 0)*/, view; //(0, 0); //#univ, univ len for caller-accessible node values
        CONST double frame_time; //kludge: set value here to match txtr; used in FrameInfo
        Protocol protocol = Protocol::/*Enum::*/NONE; //WS281X;
        Protocol prev_protocol = Protocol::/*Enum::*/CANCEL; //track protocol changes so all nodes can be updated
        /*bool*/ uint32_t isrunning = false;
//        int32_t debug_level = MAX_DEBUG_LEVEL;
//TODO: use alignof here instead of cache_pad
//    static const napi_typedarray_type perf_stats_type = napi_uint32_array; //NOTE: must match elapsed_t
//??        std::atomic<int32_t> numfr;
        int32_t numfr; //divisor for avg timing stats
//        elapsed_msec_t perf_stats[SIZEOF(TXTR::perf_stats) + 1]; //= {0}; //1 extra counter for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
        const elapsed_t started = now(); //elapsed();
        elapsed_t /*decltype(TXTR::latest)*/ latest = 0; //timestamp of latest loop iteration
        PreallocVector<elapsed_t, SIZEOF(TXTR::perf_stats) /*+ 1*/> perf_stats; //NO: 1 extra slot for loop count, but still want a local copy
        char exc_reason[80] = ""; //exc message if bkg gpu wker throws error
    public: //ctors/dtors
        explicit FrameControl(int new_screen, const SDL_Size& new_wh, double new_frame_time): screen(new_screen), wh(new_wh), frame_time(new_frame_time) {} // HERE(3); }
    public: //operators
        STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FrameControl& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        {
//            HERE(4);
//            ostrm << "manifest"; //<< my_templargs();
//            ostrm << "{" << commas(sizeof(that)) << ":" << &that;
//        if (!&that) return ostrm << " NO DATA}";
//        if (!that.isvalid()) return ostrm << " INVALID}";
            ostrm << "{screen " << that.screen;
            ostrm << ", wh " << that.wh << " nodes";
            ostrm << ", frame_time " << that.frame_time << " msec";
            ostrm << ", protocol " << that.protocol; //NVL(unmap(names, that.protocol)/*ProtocolName(that.protocol)*/, "??PROTOCOL??");
            ostrm << ", previous " << that.prev_protocol; //NVL(unmap(names, that.prev_protocol)/*ProtocolName(that.protocol)*/, "??PROTOCOL??");
            ostrm << ", debug level " << /*that.*/debug_level; //TODO: put a copy in shm
            ostrm << ", #fr " << commas(that.numfr);
            ostrm << ", latest " << that.latest << " msec";
            ostrm << ", perf [";
//            for (int i = 0; i < SIZEOF(that.perf_stats); ++i)
//broken            for (const auto it: that.perf_stats)
            for (/*const*/ auto /*decltype(that.perf_stats.cbegin())*/ it = that.perf_stats.cbegin(); it != that.perf_stats.cend(); ++it)
//            for (const /*std::remove_reference<decltype(that.perf_stats[0])>*/ elapsed_t* it = &that.perf_stats[0]; it != &that.perf_stats[SIZEOF(perf_stats)]; ++it)
//            {
//                debug(0, "perf[%u/%u] %u", it - that.perf_stats.cbegin(), that.perf_stats.cend() - that.perf_stats.cbegin(), *it);
                ostrm << &", "[(it == that.perf_stats.cbegin())? 2: 0] << (that.numfr? (double)*it / that.numfr: (double)0); //std::max(that.numfr, 1));
//            }
            ostrm << "]";
            if (that.exc_reason[0]) ostrm << ", exc '" << that.exc_reason << "'";
            ostrm << ", age " << commas(elapsed(that.started)) << " msec";
            return ostrm << "}";
        }
    public: //NAPI methods
        static inline FrameControl* my(void* ptr) { return static_cast<FrameControl*>(ptr); }
        static inline /*uint32_t*/ napi_value univlen_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->wh.h, napi_thingy::Uint32{}); }
        static /*uint32_t*/ napi_value fps_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->frame_time? 1000.0 / my(ptr)->frame_time: 0, napi_thingy::Float{}); }
        static /*uint32_t*/ napi_value frtime_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->frame_time, napi_thingy::Float{}); }
//        static /*uint32_t*/ auto protocol_getter(void* ptr) /*const*/ { return static_cast<int32_t>(me(ptr)->protocol.value); }
//        static void protocol_setter(FrameControl* fcptr, napi_thingy& newval) { fcptr->protocol = static_cast<Protocol>(newval.as_int32(true)); }
        static inline /*uint32_t*/ napi_value deblevel_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, /*my(ptr)->*/debug_level, napi_thingy::Int32{}); }
        static void deblevel_setter(const napi_thingy& newval, void* ptr)
        {
//            /*my(ptr)->*/debug_level = new_level;
            int new_level = newval.as_int32(true), old_level = debug_level;
            if (new_level > old_level) debug_level = new_level; //inc detail before showing debug msg (more likely to show msg that way)
            debug(44, "debug level %d -> %d", old_level, new_level);
            if (new_level < old_level) debug_level = new_level; //dec detail afte showing debug msg (more likely to show msg that way)
        }
        static /*uint32_t*/ napi_value numfr_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->numfr, napi_thingy::Uint32{}); }
        static /*uint32_t*/ napi_value latest_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->latest, napi_thingy::Uint32{}); }
        static /*uint32_t*/ napi_value exc_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->exc_reason); }
//        /*static*/ napi_value my_exports(napi_env env) { return my_exports(env, napi_thingy(env, napi_thingy::Object{})); }
        /*static*/ napi_value my_exports(napi_env env, const napi_value& retval)
        {
            vector_cxx17<my_napi_property_descriptor> props;
//        CONST int screen;
//        CONST SDL_Size wh; // /*(0, 0)*/, view; //(0, 0); //#univ, univ len for caller-accessible node values
//        CONST double frame_time; //kludge: set value here to match txtr; used in FrameInfo
//        Protocol protocol = Protocol::NONE; //WS281X;
//        Protocol prev_protocol = Protocol::CANCEL; //track protocol changes so all nodes can be updated
//        bool isrunning = false;
//        int numfr; //divisor for avg timing stats
//        PreallocVector<elapsed_t, SIZEOF(TXTR::perf_stats) + 1> perf_stats;
//        char exc_reason[80] = ""; //exc message if bkg gpu wker throws error
//        const elapsed_t started = now(); //elapsed();
//no; not set until port opened        add_prop("frame_time", m_frctl.frame_time)(props.emplace_back());
//no            add_prop("FPS", 1000.0 / m_frctl.frame_time)(props.emplace_back());
            add_getter("UNIV_LEN", FrameControl::univlen_getter, this)(props.emplace_back());
            add_getter("FPS", FrameControl::fps_getter, this)(props.emplace_back());
            add_getter("frtime", FrameControl::frtime_getter, this)(props.emplace_back());
            add_getter("protocol", Protocol::getter, Protocol::setter, &protocol)(props.emplace_back());
            add_getter("debug_level", FrameControl::deblevel_getter, FrameControl::deblevel_setter, this)(props.emplace_back()); //(*pptr++);
            add_getter("numfr", FrameControl::numfr_getter, this)(props.emplace_back()); //(*pptr++);
            add_getter("latest", FrameControl::latest_getter, this)(props.emplace_back()); //(*pptr++);
            napi_thingy arybuf(env, &perf_stats[0], sizeof(perf_stats));
            napi_thingy perf_typary(env, GPU_NODE_type, SIZEOF(perf_stats), arybuf); //UNIV_MAXLEN * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
            add_prop("perf_stats", perf_typary)(props.emplace_back()); //(*pptr++);
//            add_prop("perf_stats", napi_thingy(env, GPU_NODE_type, SIZEOF(perf_stats), napi_thingy(env, &perf_stats[0], sizeof(perf_stats)))(props.emplace_back()); //(*pptr++);
            add_getter("exc_reason", FrameControl::exc_getter, this)(props.emplace_back()); //(*pptr++);
//            retval += props;
//            return retval;
            return napi_thingy(env, retval) += props;
        }
        static napi_value my_exports_perfinx(napi_env env) { return my_exports_perfinx(env, napi_thingy(env, napi_thingy::Object{})); }
        static napi_value my_exports_perfinx(napi_env env, const napi_value& retval)
        {
//            exports = module_exports(env, exports); //include previous exports
//            napi_thingy retval(env, napi_thingy::Object{});
            vector_cxx17<my_napi_property_descriptor> props;
    //        add_prop("NONE", static_cast<int32_t>(Protocol::NONE))(enum_props.emplace_back()); //(*pptr++);
    //        add_prop("DEV_MODE", static_cast<int32_t>(Protocol::DEV_MODE))(enum_props.emplace_back()); //(*pptr++);
    //        add_prop("WS281X", static_cast<int32_t>(Protocol::WS281X))(enum_props.emplace_back()); //(*pptr++);
    //        add_prop("CANCEL", static_cast<int32_t>(Protocol::CANCEL))(enum_props.emplace_back()); //(*pptr++);
//            for (auto& it: static_Names) //it.first, it.second
            for (auto it = TXTR::static_PerfNames().cbegin(); it != TXTR::static_PerfNames().cend(); ++it) //it->first, it->second
                add_prop_uint32(it->second, it->first)(props.emplace_back()); //(*pptr++);
    //        debug(9, "add %d props", props.size());
//            !NAPI_OK(napi_define_properties(env, retval, props.size(), props.data()), "export protocol enum props failed");
//            napi_thingy more_retval(env, retval);
//            more_retval += props;
//            return more_retval;
            return napi_thingy(env, retval) += props;
        }
    };
#if 0
//debug/diagnostic msgs:
//don't want anything slowing down gpu_wker (file or console output could cause screen flicker),
// so log messages are stored in shm and another process can read them if interested
//circular fifo is used; older msgs will be overwritten; no memory mgmt overhead
    struct MsgLog
    {
        std::atomic<int32_t> latest;
//        struct
//        {
//TODO: structured msg info?
//            elapsed_t timestamp;
//            char msg[120]; //free-format text (null terminated)
//        } log[100];
        char msgs[100][120];
    public: //ctors/dtors
        MsgLog()
        {
            latest = 0; //not really needed (log is circular), but it's nicer to start in a predictable state
            for (int i = 0; i < SIZEOF(msgs); ++i)
            {
//                log[i].timestamp = 0;
                msgs[i][0] = '\0';
            }
        }
    public: //operators
        STATIC friend std::ostream& operator<<(std::ostream& ostrm, const MsgLog& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        {
//            HERE(7);
            ostrm << "{" << commas(SIZEOF(that.msgs)) << " msgs";
            ostrm << ", most recent[" << commas(that.latest) << "/" << commas(SIZEOF(msgs)) << "] "; //<< " at " << commas(that.log[that.latest].timestamp) << " msec";
            ostrm << strlen(that.msgs[that.latest]) << ": '" << that.msgs[that.latest] << "'";
            return ostrm << "}";
        }
    public: //methods
//TODO: move to separate shm seg and combine with debug()
        void log(/*int level, SrcLine srcline,*/ const char* fmt, ...)
        {
            int inx = latest++;
//TODO: add structured info
//            strncpy(msgs[inx], new_msg, SIZEOF(msgs[inx]));
            va_list args;
            va_start(args, fmt);
            size_t fmtlen = vsnprintf(msgs[inx], sizeof(msgs[inx]), fmt, args);
            va_end(args);
//show warning if fmtbuf too short:
            if (fmtlen >= sizeof(msgs[inx])) fmtlen = sizeof(msgs[inx]) - 20 + snprintf(&msgs[inx][sizeof(msgs[inx]) - 20], 20, " >> %s ...", commas(fmtlen));
//            msgs[inx][SIZEOF(msgs[inx] - 1)] = '\0'; //add null termintor in case msg was truncated
        }
    public: //NAPI methods
        static inline MsgLog* my(void* ptr) { return static_cast<MsgLog*>(ptr); }
        static napi_value latest_getter(napi_env env, void* ptr) { return napi_thingy(env, my(ptr)->latest.load(), napi_thingy::Int32{}); }
        static napi_value toString_NAPI(napi_env env, napi_callback_info info)
        {
            if (!env) return NULL; //Node cleanup mode?
            DebugInOut("toString_napi");

            ShmData* shmptr;
            napi_value argv[0+1], This; //allow 1 extra arg to check for extras
            size_t argc = SIZEOF(argv);
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
            !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&shmptr), "Get cb info failed");
            if (argc) NAPI_exc("expected 0 args, got " << argc << " arg" << plural(argc));
            shmptr->isvalid(env, SRCLINE);
            return napi_thingy(env, shmptr->m_msglog.msgs[shmptr->m_msglog.latest]); //, napi_thingy::String{});
        }
        /*static*/ napi_value my_exports(napi_env env) { return my_exports(env, napi_thingy(env, napi_thingy::Object{})); } //napi_value arybuf, napi_thingy::Array{}, NUM_UNIV)); }
        /*static*/ napi_value my_exports(napi_env env, const napi_value& retval)
        {
            vector_cxx17<my_napi_property_descriptor> props;
//            exports = module_exports(env, exports); //include previous exports
//            napi_thingy retval(env, napi_thingy::Object{});
//            vector_cxx17<my_napi_property_descriptor> props;
//            !NAPI_OK(napi_create_array_with_length(env, QUELEN, &retval.value), "Cre que ary failed");
//            napi_thingy arybuf(env, &m_fbque[0], sizeof(m_fbque));
            add_getter("latest", MsgLog::latest_getter, this)(props.emplace_back());
            napi_thingy msg_ary(env, napi_thingy::Array{}, SIZEOF(msgs));
            for (int i = 0; i < SIZEOF(msgs); ++i)
            {
//TODO: add handle_scope? https://nodejs.org/api/n-api.html#n_api_making_handle_lifespan_shorter_than_that_of_the_native_method
//                debug(33, "cre typed ary, ofs %d x %s + %d x %s + %u = %s", inx, commas(sizeof(*this)), x, commas(sizeof(nodes[0])), addrof(&nodes[0][0]) - addrof(this), commas(inx * sizeof(*this) + x * sizeof(nodes[0]) + addrof(&nodes[0][0]) - addrof(this))); //UNIV_MAXLEN * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
//                napi_thingy node_typary(env, GPU_NODE_type, /*wh.h*/ SIZEOF(nodes[0]) /*UNIV_MAXLEN_pad*/ /*_raw*/, arybuf, inx * sizeof(*this) + x * sizeof(nodes[0]) + addrof(&nodes[0][0]) - addrof(this)); //UNIV_MAXLEN * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
                napi_thingy msg_obj(env, napi_thingy::Object{});
                vector_cxx17<my_napi_property_descriptor> msg_props;
//expose toString() method; don't want static copy of msg
                add_method("toString", MsgLog::toString_NAPI, this)(msg_props.emplace_back());
                msg_obj += msg_props;
                !NAPI_OK(napi_set_element(env, msg_ary, i, msg_obj), "Cre msg typary failed");
            }
            add_prop("msgs", msg_ary)(props.emplace_back());
            return napi_thingy(env, retval) += props;
        }
    };
#endif
//put nodes last in case caller overruns boundary:
    /*alignas(CACHELEN)*/ struct FramebufQuent
    {
//        /*alignas(CACHELEN)*/ struct
//        {
        std::atomic<int32_t> frnum; //, prevfr;
        std::atomic<elapsed_t> frtime, prevtime;
        std::atomic<MASK_TYPE> ready; //per-univ Ready/dirty bits
//        } frinfo; //per-frame state info
//        uint8_t pad[];
//        typedef /*alignas(CACHELEN)*/ NODEVAL UNIV[UNIV_MAXLEN]; //align univ to cache for better mem perf across cpus
//align univ to cache for better mem perf across cpus:
        alignas(CACHELEN) NODEVAL nodes[NUM_UNIV][rndup(UNIV_MAXLEN, CACHELEN)]; //_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
        static const key_t SHMKEY = 0xFEED0000 | NNNN_hex(SIZEOF(nodes[0])); //0; //show size (padded) in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
    public: //ctors/dtors
//        FramebufQuent() //: frnum(0), prevfr(0), frtime(0), prevtime(0), ready(0) //need to init to avoid "deleted function" errors
//        {
//            frnum.store(0); prevfr.store(0);
//            frtime.store(0); prevtime.store(0;
//            ready.store(0);
//        }
    public: //operators
        STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FramebufQuent& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        {
//            HERE(7);
            ostrm << "{fr# " << commas(that.frnum.load()); //<< ", prev " << commas(that.prevfr.load());
            ostrm << ", fr time " << commas(that.frtime.load()) << ", prev " << commas(that.prevtime.load()) << " msec";
            ostrm << ", ready 0x" << std::hex << that.ready.load() << std::dec;
            SDL_Size wh(SIZEOF(nodes), SIZEOF(nodes[0]));
            ostrm << ", nodes " << wh;
            return ostrm << "}";
        }
    public: //NAPI methods
        static inline FramebufQuent* my(void* ptr) { return static_cast<FramebufQuent*>(ptr); }
        static /*uint32_t*/ /*auto*/ napi_value frnum_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->frnum.load(), napi_thingy::Int32{}); }
//        static /*uint32_t*/ /*auto*/ napi_value frnum_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->frnum.load()); }
        static /*uint32_t*/ /*auto*/ napi_value frtime_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->frtime.load(), napi_thingy::Uint32{}); }
        static /*uint32_t*/ /*auto*/ napi_value prevtime_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->prevtime.load(), napi_thingy::Uint32{}); }
        static /*uint32_t*/ /*auto*/ napi_value ready_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->ready.load(), napi_thingy::Uint32{}); } //MASK_TYPE
        static void ready_setter(const napi_thingy& newval, void* ptr)
        {
//NOTE: js "|=" uses getter + setter so it's not atomic; this setter implements "|=" semantics directly in here to ensure atomic updates
            uint32_t newbits = newval.as_uint32(true);
            uint32_t sv_ready = my(ptr)->ready.load(); //TODO: find out where upper 8 bits are being set
            if (newbits) my(ptr)->ready |= newbits;
            else my(ptr)->ready.store(0); //"|= 0" will reset value to 0
            debug(12, "ready 0x%x |= 0x%x => 0x%x", sv_ready, newbits, my(ptr)->ready.load());
        }
//??        static STATIC_WRAP(napi_ref, m_nodes_ref, = nullptr);
        static intptr_t addrof(void* member) { return (intptr_t)member; } //kludge: bypass compiler's refusal to give address of data members
//        size_t my_offset_of(void* member) { intptr_t ptr = member; return ptr; }
        /*static*/ napi_value my_exports(napi_env env, napi_value arybuf, size_t inx) { return my_exports(env, arybuf, inx, napi_thingy(env, napi_thingy::Object{})); } //napi_value arybuf, napi_thingy::Array{}, NUM_UNIV)); }
        /*static*/ napi_value my_exports(napi_env env, napi_value arybuf, size_t inx, const napi_value& retval)
        {
            vector_cxx17<my_napi_property_descriptor> props;
//            exports = module_exports(env, exports); //include previous exports
//            napi_thingy retval(env, napi_thingy::Object{});
//            vector_cxx17<my_napi_property_descriptor> props;
//            !NAPI_OK(napi_create_array_with_length(env, QUELEN, &retval.value), "Cre que ary failed");
//            napi_thingy arybuf(env, &m_fbque[0], sizeof(m_fbque));
            add_getter("frnum", FramebufQuent::frnum_getter, this)(props.emplace_back());
            add_getter("frtime", FramebufQuent::frtime_getter, this)(props.emplace_back());
            add_getter("prevtime", FramebufQuent::prevtime_getter, this)(props.emplace_back());
            add_getter("ready", FramebufQuent::ready_getter, FramebufQuent::ready_setter, this)(props.emplace_back());
//            for (auto& it = m_fbque.begin(); it != m_fbque.end(); ++it)
//            {
//            napi_thingy arybuf(env, &it->nodes[0][0], sizeof(it->nodes)); //ext buf for all nodes in all univ
//2D array; helps separate univ from eacher
//    NODEVAL nodes[NUM_UNIV][UNIV_MAXLEN_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
//            napi_thingy univ_ary(env);
//                !NAPI_OK(napi_create_array_with_length(env, NUM_UNIV, &univ_ary.value), "Cre univ ary failed");
//            FramebufQuent* ofs_ptr = 0;
//            intptr_t ofs_of_nodes = &ofs_ptr->nodes[0][0];
            napi_thingy univ_ary(env, napi_thingy::Array{}, NUM_UNIV);
            for (int x = 0; x < /*wh.w*/ NUM_UNIV; ++x)
            {
//TODO: add handle_scope? https://nodejs.org/api/n-api.html#n_api_making_handle_lifespan_shorter_than_that_of_the_native_method
//                debug(33, "cre typed ary, ofs %d x %s + %d x %s + %u = %s", inx, commas(sizeof(*this)), x, commas(sizeof(nodes[0])), addrof(&nodes[0][0]) - addrof(this), commas(inx * sizeof(*this) + x * sizeof(nodes[0]) + addrof(&nodes[0][0]) - addrof(this))); //UNIV_MAXLEN * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
                napi_thingy node_typary(env, GPU_NODE_type, /*wh.h*/ SIZEOF(nodes[0]) /*UNIV_MAXLEN_pad*/ /*_raw*/, arybuf, inx * sizeof(*this) + x * sizeof(nodes[0]) + addrof(&nodes[0][0]) - addrof(this)); //UNIV_MAXLEN * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
                !NAPI_OK(napi_set_element(env, univ_ary, x, node_typary), "Cre inner node typary failed");
            }
            add_prop("nodes", univ_ary)(props.emplace_back());
//#if 1 //does this work?
//        std::atomic<int32_t> frnum, prevfr;
//        std::atomic<elapsed_t> frtime, prevtime;
//        std::atomic<MASK_TYPE> ready; //per-univ Ready/dirty bits
//napi_callback cb; cb = std::bind(getter_shim2, std::placeholders::_1, std::placeholders::_2, FramebufQuent::frnum_getter);
//napi_callback cb; cb = [](napi_env env, napi_callback_info info) -> napi_value { return getter_shim2(env, info, FramebufQuent::frnum_getter); }
//NAMED{ _.utf8name = "frtime"; _.getter = std::bind(getter_shim2, std::placeholders::_1, std::placeholders::_2, FramebufQuent::frnum_getter); _.attributes = napi_enumerable; _.data = this; }(props.emplace_back());
//            !NAPI_OK(napi_define_properties(env, univ_ary, props.size(), props.data()), "export protocol props failed");
//            retval += props;
//            return retval;
            return napi_thingy(env, retval) += props;
        }
    };
//    InOutDebug inout1;
//protected: //data members
public: //data members (visible to bkg wkers via shm)
//bkg gpu wker stores private data on stack or heap
//RPi memory is slow, so cache perf is important; use alignment to avoid spanning memory cache rows:
//NOTE: force storage types here so sizes don't depend on compiler or arch; Intel was using a mix of uin64_t and 32, making it awkward for external readers
    const uint32_t m_hdr = VALIDCHK; //bytes[0..3]; 1 x int32
    const int32_t m_ver = VERSION; //bytes[4..7]; 1 x int32
    ManifestType m_manifest; //bytes[8..55]; 6 x uint32
    FrameControl m_frctl; //bytes[56..]; 8 x int32 + 1 x float + 5 x uint stats + 80 char (168 bytes total)
    const uint32_t m_flag1 = VALIDCHK; //bytes[]; 1 x int32
    alignas(CACHELEN) uint32_t m_spare[SPARELEN]; //leave room for caller-defined data within same shm seg; bytes[284..]; 64 x uint32
    const uint32_t m_flag2 = VALIDCHK; //1 x int32
//    alignas(CACHELEN) struct FramebufQuent
//    PreallocVector<MsgLog, LOGLEN> m_msglog; //circular queue of nodebufs + perf stats
//    MsgLog m_msglog;
    PreallocVector<alignas(CACHELEN) FramebufQuent, QUELEN> m_fbque; //circular queue of nodebufs + perf stats
//    napi_reference m_ref = nullptr;
    const uint32_t m_tlr = VALIDCHK;
//    /*txtr_bb*/ /*SDL_AutoTexture<XFRTYPE>*/ TXTR m_txtr; //in-memory copy of bit-banged node (color) values (formatted for protocol)
//    InOutDebug inout2;
public: //ctors/dtors
//    explicit ShmData(int new_screen, const SDL_Size& new_wh, double new_frame_time): info(new_screen, new_wh, new_frame_time) {}
    explicit ShmData(): /*inout1("1"), inout2("2"),*/ m_frctl(-1, SDL_Size(0, 0), 0) { /*HERE(2);*/ INSPECT(GREEN_MSG "ctor " << *this); } //set junk values until bkg wker starts
    ~ShmData() { INSPECT(RED_MSG "dtor " << *this); }
public: //operators
    bool isvalid() const { return this && (m_hdr == VALIDCHK) && (m_flag1 == VALIDCHK) && (m_flag2 == VALIDCHK) && (m_tlr == VALIDCHK); }
    bool isvalid(napi_env env, SrcLine srcline = 0) const
    {
        return isvalid() || NAPI_exc("ShmData " << this << " invalid: " << !!this << std::hex << ((m_hdr < 10)? ",": ",0x") << m_hdr << ((m_flag1 < 10)? ",": ",0x") << m_flag1 << ((m_flag2 < 10)? ",0x": ",") << m_flag2 << ((m_tlr < 10)? ",0x": ",") << m_tlr << std::dec << ATLINE(srcline));
    }
//alloc/dealloc in shared memory:
//this allows caller to use new + delete without special code for shm
#if 0 //NO; need to decide earlier whether to call ctor or not
    STATIC void* operator new(size_t size) //, size_t ents)
    {
        size_t ents = 1;
        static int uniq = 0; //should only be a singleton, but code for multi in case needed in future
//        return shmalloc_typesafe<ShmData>(SHMKEY + 0x1000 * uniq++, ents, SRCLINE);
        key_t key = SHMKEY + 0x1000 * uniq++;
        void* ptr = shmalloc_debug(ents * sizeof(ShmData), key, SRCLINE);
        debug(12, "alloc#%d shmdata x %d cop%s at %p", uniq, ents, plural(ents, "ies", "y"), ptr);
//        HERE(1);
        if (!ptr && (ptr = shmalloc_debug(1, key, SRCLINE))) exc_hard("alloc ShmData 0x%lx failed: bigger than existing", key);
        if (!ptr) exc_hard("alloc ShmData 0x%x failed", key);
        if (shmnattch(ptr) != 1) exc_soft("ShmData multiple calls to ctor");
        return ptr;
    }
    STATIC void operator delete(void* shmdata) //, size_t ents)
    {
        ShmData* shmptr = static_cast<ShmData*>(shmdata);
        if (!shmptr->isvalid()) exc_hard("Invalid shmdata");
        size_t ents = get_shmhdr(shmptr, SRCLINE)->numents;
        debug(12, "dealloc %d copies of shmdata at %p", ents, shmdata);
//        shmfree_typesafe<ShmData>(shmptr, SRCLINE);
        shmfree_debug(shmptr, SRCLINE);
    }
#endif
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const ShmData& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
//        HERE(5);
//        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
//        SDL_version ver;
//        SDL_GetVersion(&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
//        ostrm << "SDL_Lib {version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d}", ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//    %p, valid? %d" ENDCOLOR, aodata.get(), aodata.get()->isvalid());
        ostrm << "ShmData"; //<< my_templargs();
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
        if (!that.isvalid()) return ostrm << " INVALID}";
        ostrm << ", ver 0x" << std::hex << that.m_ver << std::dec;
        ostrm << ", manifest " << that.m_manifest;
        ostrm << ", frctl " << that.m_frctl;
        ostrm << ", is open? " << that.isopen();
        ostrm << ", #attch " << shmnattch(&that); //.m_manifest.shmkey);
//        ostrm << ", msglog " << that.m_msglog;
        ostrm << ", fbque [";
//broken        for (const auto it: that.m_fbque)
        for (/*const*/ auto it = that.m_fbque.cbegin(); it != that.m_fbque.cend(); ++it)
            ostrm << &", "[(it == that.m_fbque.cbegin())? 2: 0] << (it - that.m_fbque.cbegin()) << *it;
#if 0
        ostrm << ", 'ver " << offsetof(ShmData, m_hdr) << " " << &that.m_hdr;
        ostrm << ", 'manif " << offsetof(ShmData, m_manifest) << " " << &that.m_manifest;
        ostrm << ", 'frctl " << offsetof(ShmData, m_frctl) << " " << &that.m_frctl;
        ostrm << ", 'spare " << offsetof(ShmData, m_spare) << " " << &that.m_spare[0];
        ostrm << ", 'fbque " << offsetof(ShmData, m_fbque) << " " << &that.m_fbque[0];
        ostrm << ", 'tlr " << offsetof(ShmData, m_tlr) << " " << &that.m_tlr;
#endif
        return ostrm << "}";
    }
public: //methods:
    bool isopen() const { return (isvalid() && m_frctl.isrunning); } //(m_frctl.protocol != Protocol::CANCEL)); }
    bool isopen(bool yesno)
    {
        if (!isvalid()) return false;
//        return (isvalid() && m_frctl.isrunning); //(m_frctl.protocol != Protocol::CANCEL)); }
        return m_frctl.isrunning = yesno;
    }
    void init_fbque(NODEVAL color = BLACK)
    {
//set up first round of framebufs to be processed by wker threads:
//broken        for (auto it: m_fbque) //.begin(); int i = 0; i < SIZEOF(fbque); ++i)
        for (auto it = m_fbque.begin(); it != m_fbque.end(); ++it)
        {
            it->ready.store(0);
            it->frnum = it - m_fbque.begin(); //initially set to 0, 1, 2, ...
            it->prevtime = it->frtime = 0; //it->frnum * m_frctl.frame_time; //deadline for this frame based on known frame_time; float -> int
//            it->prevtime = it->prevfr = -1; //no previous frame
//NOTE: loop (1 write/element) is more efficient than memcpy (1 read + 1 write / element)
            for (int i = 0; i < SIZEOF_2D(it->nodes); ++i) it->nodes[0][i] = color; //BLACK; //clear *entire* buf in case h < max and caller wants linear (1D) addressing
#if 0 //test pattern for js client shm test
 #pragma message("test pattern")
            for (int x = 0; x < SIZEOF(it->nodes); ++x)
                for (int y = 0; y < SIZEOF(it->nodes[0]); ++y)
                    it->nodes[x][y] = ((it - m_fbque.begin() + 10) * 0x11000000UL) | ((x + 1) * 0x10000UL) | (y + 1);
#endif
        }
        debug(44, "init %d fbque ents to 0x%x", SIZEOF(m_fbque), color);
//also init gpu wker stats:
        m_frctl.numfr = 0;
        memset(&m_frctl.perf_stats[0], 0, sizeof(m_frctl.perf_stats));
    }
//unresolved    template <typename ... ARGS>
//    static void gpu_wker_static(ShmData* shmptr, ARGS&& ... args) //shim for std::thread()
//    {
//        shmptr->gpu_wker(std::forward<ARGS>(args) ...); //perfect fwd
//    }
    static void gpu_wker_static(ShmData* shmptr, int NUMFR = INT_MAX, int screen = FIRST_SCREEN, SDL_Size* want_wh = NO_SIZE, size_t vgroup = 1, NODEVAL init_color = BLACK, SrcLine srcline = 0) //shim for std::thread()
    {
        shmptr->gpu_wker(NUMFR, screen, want_wh, vgroup, init_color, srcline); //TODO: perfect fwd
    }
//start bkg gpu wker; the gpu "port" is "open" while this is running
//NOTE: this should be run on a separate thread so it doesn't block the Node event loop
//NOTE also: SDL is not thread-safe, so it needs to be a dedicated thread
    void gpu_wker(int NUMFR = INT_MAX, int screen = FIRST_SCREEN, SDL_Size* want_wh = NO_SIZE, size_t vgroup = 1, NODEVAL init_color = BLACK, SrcLine srcline = 0)
    {
        std::string exc_msg;
        elapsed_t started = now();
//debug(0, "elapsed " << elapsed(started));
//        strcpy(m_frctl.exc_reason, "");
        try //TODO: let it die (for dev/debug)?
        {
//nodes: #univ x univ_len, txtr: 3 * 24 x univ len, view (clipped): 3 * 24 - 1 x univ len
//debug("here50" ENDCOLOR);
            SDL_Size view;
            m_frctl.wh.w = NUM_UNIV; //nodes
            view.w = BIT_SLICES - 1; //last 1/3 bit will overlap hblank; clip from visible part of window
//TODO: consolidate ScreenInfo + ScreenConfig
            view.h = m_frctl.wh.h = std::min(divup(ScreenInfo(screen, SRCLINE)->bounds.h, vgroup? vgroup: 1), /*static_cast<int>*/SIZEOF(m_fbque[0].nodes[0])); //univ len == display height
            const ScreenConfig* const cfg = getScreenConfig(screen, SRCLINE); //NVL(srcline, SRCLINE)); //get this first for screen placement and size default; //CAUTION: must be initialized before txtr and frame_time (below)
            if (!cfg) exc_hard("can't get screen %d config");
            m_frctl.screen = cfg->screen;
//        /*static_cast<std::remove_const(decltype(m_frctl.frame_time))>*/ m_frctl.frame_time = cfg->frame_time()? cfg->frame_time(): 1.0 / FPS_CONSTRAINT(NVL(cfg->dot_clock * 1000, CLOCK), NVL(cfg->htotal, HTOTAL), NVL(cfg->vtotal, (decltype(cfg->vtotal))SIZEOF(m_fbque[0].nodes[0]))); //UNIV_MAXLEN_raw))); //estimate from known info if not configured
        /*static_cast<std::remove_const(decltype(m_frctl.frame_time))>*/ (m_frctl.frame_time = cfg->frame_time() * 1e3) || (m_frctl.frame_time = 1e3 / FPS_CONSTRAINT(NVL(cfg->dot_clock * 1000, CLOCK), NVL(cfg->htotal, HTOTAL), NVL(cfg->vtotal, (decltype(cfg->vtotal))SIZEOF(m_fbque[0].nodes[0])))); //UNIV_MAXLEN_raw))); //estimate from known info if not configured
            debug(33, "set fr time: from cfg %f, from templ %f, chose %f", cfg->frame_time() * 1e3, 1e3 / FPS_CONSTRAINT(NVL(cfg->dot_clock * 1000, CLOCK), NVL(cfg->htotal, HTOTAL), NVL(cfg->vtotal, (decltype(cfg->vtotal))SIZEOF(m_fbque[0].nodes[0]))), m_frctl.frame_time); //UNIV_MAXLEN_raw))); //estimate from known info if not configured
            SDL_Size zero(0, 0);
//debug(0, "want_wh " << want_wh);
//debug(0, "*want_wh " << (want_wh? *want_wh: zero));
//CAUTION: null deref broken on RPi; need to check for null here
            debug(33, CYAN_MSG "start bkg gpu wker: screen %d " << *cfg << ", want wh " << (want_wh? *want_wh: zero) << ", fr time %f" << ATLINE(srcline), screen, m_frctl.frame_time);
//        wh.h = new_wh.h; //cache_pad32(new_wh.h); //pad univ to memory cache size for better memory perf (multi-proc only)
//        m_debug3(m_view.h),
//TODO: don't recreate if already exists with correct size
//debug("here51" ENDCOLOR);
//{ DebugInOut("assgn new txtr", SRCLINE);
            SDL_Size txtr_wh(BIT_SLICES, m_frctl.wh.h);
//        ShmData::TXTR txtr(ShmData::TXTR::NullOkay{}); //leave empty until bkg thread starts
//        m_txtr(TXTR::NullOkay{}), //leave empty until bkg thread starts
            TXTR txtr = TXTR::create(NAMED{ _.wh = &txtr_wh; _.view_wh = &view, _.screen = screen; _.init_color = init_color; SRCLINE; });
//        m_txtr = newtxtr; //kludge: G++ thinks m_txtr is a ref so assign create() to temp first
            TXTR::XFR xfr = std::bind(xfr_bb, std::ref(*this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, SRCLINE); //protocol bit-banger shim
//TODO: refill not needed?
            txtr.clear_stats(&m_frctl.perf_stats[0]); //perftime(); //kludge: flush perf timer, but leave a little overhead so first-time results are realistic
//            elapsed_t first_caller_correction;
//}
            debug(19, "bkg gpu txtr " << txtr);
            init_fbque(init_color); //do this *before* set running state, but after determining frame_time
            isopen(true); //m_frctl.isrunning = true;
//debug("here52" ENDCOLOR);
//        m_txtr = txtr.release(); //kludge: xfr ownership from temp to member
//done by main:        dirty.store(0); //first sync with client thread(s)
//debug("here53" ENDCOLOR);
//TODO?        wrap(env);
//debug("here54" ENDCOLOR);
//        if (UNIV_LEN > UNIV_MAX) exc_soft(RED_MSG "video settings " << *m_cfg << ", vgroup " << vgroup << " require univ len " << UNIV_LEN << " exceeding max " << UNIV_MAX << " allowed by compiled node buf" << ENDCOLOR_ATLINE(srcline));
//        size_t len = new_wh.w * new_wh.h;
//        m_nodes.reset(new NODEVAL[len]);

//        std::thread bkg([/*env,*/ aoptr]() //env, screen, vgroup, init_color]()
//        napi_status status;
//        ExecuteWork(env, addon_data);
//        WorkComplete(env, status, addon_data);
            debug(12, PINK_MSG "start bkg loop: aodata " << *this); //, valid? %d", this, isvalid());
//        debug(YELLOW_MSG "bkg acq" ENDCOLOR);
//        !NAPI_OK(napi_acquire_threadsafe_function(aoptr->fats), "Can't acquire JS fats");
//        !NAPI_OK(napi_reference_ref(env, aodata->listener.ref, &ref_count), "Listener ref failed");
//        aoptr->islistening(true);
//        aoptr->start(); //env);
//        try{
//        elapsed_msec_t started = elapsed_msec(), previous = started, delta;
//            const int delay_msec = 1000; //2 msec;
            started = now(); //reset timebase so timing stats are just for render loop
//debug(0, "elapsed " << (now() - started) << ", " << (1000 * (now() - started)));
            log(12, "gpu_wkr start playback loop");
            for (int frnum = 0; frnum < NUMFR; m_frctl.numfr = ++frnum) //no-CAUTION: numfr pre-inc to account for clear_stats() at end of first iter; //int i = 0; i < 5; ++i)
//        for (auto it = fbque.begin(true); info.Protocol != CANCEL; ++it) //CAUTION: circular queue
            {
//        let qent = frnum % frctl.length; //simple, circular queue
                FramebufQuent* it = &m_fbque[frnum % SIZEOF(m_fbque)]; //CAUTION: circular queue
                if (it->frnum != frnum) exc_hard("frbuf que addressing messed up: got fr#%d, wanted %d", it->frnum.load(), frnum); //main is only writer; this shouldn't happen!
                int wait_frames = 0;
                while ((it->ready & ALL_UNIV) != ALL_UNIV) //wait for all wkers to render nodes (ignore unused bits); wait means wkers are running too slow
                {
                    VOID txtr.idle(NO_PERF, SRCLINE); //wait for next vsync
//                    debug(15, YELLOW_MSG "fr[%d/%d] buf[%d/%d] not ready: 0x%x, gpu wker wait %d msec for wkers to render ...", frnum, NUMFR, it - &m_fbque[0], SIZEOF(m_fbque), it->ready.load(), delay_msec);
//                    SDL_Delay(delay_msec); //2 msec); //timing is important; don't wait longer than needed
                    ++wait_frames;
                }
//                if (wait_frames) debug(15, YELLOW_MSG "qpu wker fr[%d/%d] waited %s frame times (%s msec) for buf[%d/%d] ready", frnum, NUMFR, commas(wait_frames), commas(wait_frames * m_frctl.frame_time), it - &m_fbque[0], SIZEOF(m_fbque));
                if (wait_frames) log(15, "gpu_wkr fr[%d] waited %d", frnum, wait_frames);
//            delta = elapsed.now() - previous; perf_stats[0] += delta; previous += delta;
//TODO: tweening for missing/!ready frames?
//        static const decltype(m_frinfo.elapsed_msec()) TIMING_SLOP = 5; //allow +/-5 msec
//        const decltype(m_frinfo.elapsed_msec()) /*elapsed_t*/ overdue = m_opts.frtime_msec? m_frinfo.elapsed_msec() - numfr.load() * m_opts.frtime_msec: 0, delay = (overdue < -TIMING_SLOP/2)? -overdue: 0;
//        const char* severity = /*((overdue < -10) || (overdue > 10))? RED_MSG:*/ PINK_MSG;
//        debug(15, severity << "update playback: fr# %s due at %s msec, overdue %s msec, delay? %s" ENDCOLOR, commas(numfr.load()), commas(numfr.load() * m_opts.frtime_msec), commas(overdue), commas(delay));
//        if (delay) SDL_Delay(delay); //delay if early
//useless:                m_frctl.perf_stats[0] += txtr.perftime(); //measure caller's time (presumably for rendering); //1000); //ipc wait time (msec)
                debug(15, "xfr fr[%d/%d] to gpu, protocol " << m_frctl.protocol << " ... ", frnum, NUMFR);
//                if (!(frnum % 50)) debug(0, "elapsed " << (now() - started) << ", " << (1000 * (now() - started)));
                if (m_frctl.protocol == Protocol::CANCEL) break;
                VOID txtr.update(NAMED{ _.pixels = /*&m_xfrbuf*/ &it->nodes[0][0]; _.perf = &m_frctl.perf_stats[1-1]; _.xfr = xfr; /*_.refill = refill;*/ SRCLINE; });
                it->prevtime.store(it->frtime.load()); //save previous so caller can decide how to apply updates
                it->frtime = m_frctl.latest = txtr.m_latest; //just echo txtr; //now() - started;
                if (!(frnum % 120)) log(15, "gpu_wkr fr[%d] rendered", frnum);
//                ++m_frctl.perf_stats[0]; //moved to txtr
//            m_frctl.numfr = frnum + 1;
//TODO: pivot/update txtr, update screen (NON-BLOCKING)?
//make frbuf available for next round of frames:
//CAUTION: potential race condition, but render wkers should be far enough ahead that it doesn't matter:
                it->ready.store(0);
//                it->prevfr.store(it->frnum.load());
                it->frnum += SIZEOF(m_fbque); //tell wkers which frame to render next;//QUELEN; //NOTE: do this last (wkers look for this)
//                m_frctl.numfr = frnum; //pre-inc
//kludge: try to compensate for first iteration likely had extra startup overhead or had extra time for prep:
//                if (!frnum) memset(&m_frctl.perf_stats[0], 0, sizeof(m_frctl.perf_stats)); //clear special case values; //TXTR::CALLER] = 0;
//                else if (frnum == 1) //duplicate second iteration values for first iteration
//                    for (int i = 0; i < SIZEOF(m_frctl.perf_stats); ++i) m_frctl.perf_stats[i] *= 2;
//RPi seems to take 2 iterations to stabilize, so apply kludge to first 3 frames:
                if (frnum && (frnum < 3)) //use this iteration values for previous iteration
                    for (int i = 0; i < SIZEOF(m_frctl.perf_stats); ++i)
                        m_frctl.perf_stats[i] = (frnum + 1) * (m_frctl.perf_stats[i] - txtr.perf_stats[i]);
                if (frnum < 2) //leave suspect edge case stats in place, just make a copy to compare against next iter
                    memcpy(&txtr.perf_stats[0], &m_frctl.perf_stats[0], sizeof(m_frctl.perf_stats));
//        delta = elapsed.now() - previous; perf[0].render += delta; previous += delta;
//        perf[0].numfr = frnum + 1;
//        perf[0].update(true);
//                DebugInOut("call fats for fr# " << commas(aoptr->/*m_frinfo.*/numfr.load()) << ", wait for 0x" << std::hex << Nodebuf::ALL_UNIV << std::dec << " (blocking)", SRCLINE);
//                !NAPI_OK(napi_call_threadsafe_function(aoptr->fats, aoptr, napi_tsfn_blocking), "Can't call JS fats");
//            while (aoptr->islistening()) //break; //allow cb to break out of playback loop
//    typedef std::function<bool(void)> CANCEL; //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
//                aoptr->dirty.wait(Nodebuf::ALL_UNIV, [aoptr](){ return !aoptr->islistening(); }, true, SRCLINE); //CAUTION: blocks until al univ ready or caller cancelled
//                const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE};
//                for (int i = 0; i < SIZEOF(aoptr->m_nodebuf.nodes); ++i) aoptr->m_nodebuf.nodes[0][i] = palette[aoptr->numfr.load() % SIZEOF(palette)];
//                if (aoptr->numfr.load() >= 10) break;
//                debug(12, BLUE_MSG "bkg woke from fr# %s with ready 0x%x, caller listening? %d" ENDCOLOR, commas(aoptr->numfr.load()), aoptr->dirty.load(), aoptr->islistening());
//                if (!aoptr->islistening()) break;
//                SDL_Delay(1 sec);
//                aoptr->update(); //env);
//            }
//            SDL_Delay(1 sec);
//https://stackoverflow.com/questions/233127/how-can-i-propagate-exceptions-between-threads
            }
//            debug(12, YELLOW_MSG "bkg exit after %s frames" ENDCOLOR, commas(m_frctl.numfr));
        }
        catch (const std::exception& exc) { exc_msg = exc.what(); }
        catch (...) { exc_msg = "??EXC??"; }
//            std::exception_ptr excptr; //= nullptr; //init redundant (default init)
//            excptr = std::current_exception(); //allow main thread to rethrow
//        uint32_t usec_per_fr = m_frctl.numfr? rdiv(1000 * elapsed(started), m_frctl.numfr): 0;
        double elapsed_sec = (double)elapsed(started) / SDL_TickFreq();
        int elapsed_usec = elapsed_sec * 1e6;
        int usec_per_fr = m_frctl.numfr? elapsed_usec / m_frctl.numfr: 0; //use "int" here to reduce #dec places after convert to msec
//        debug(0, "elapsed " << elapsed_sec << " sec, " << elapsed_usec << " usec, " << msec_per_fr << " msec/fr");
        std::ostringstream ss;
        ss << " after " << commas(m_frctl.numfr) << FMT(" frames (%4.3f") << ((double)usec_per_fr / 1000) << " msec avg), valid? " << isvalid();
//        ss << ", usec/fr " << usec_per_fr;
//        ss << ", elaps " << (now() - started) << ", usec/fr " << usec_per_fr << "," << usec_per_fr64 << "," << usec_per_fr64i << "," << usec_per_fr32i;
//        if (exc_msg.size()) debug(2, RED_MSG "gpu wker exc: %s after %s frames, valid? %d", exc_msg.c_str(), commas(m_frctl.numfr), isvalid());
//        else debug(12, YELLOW_MSG "bkg exit after %s frames, valid? %d", commas(m_frctl.numfr), isvalid());
        log(15, "gpu_wkr exit %s", ss.str());
        if (exc_msg.size()) debug(0, RED_MSG "gpu wker exc: %s" << ss.str(), exc_msg.c_str());
        else debug(0, YELLOW_MSG "gpu wker exit" << ss.str());
        strncpy(m_frctl.exc_reason, exc_msg.c_str(), sizeof(m_frctl.exc_reason));
        m_frctl.protocol = Protocol::CANCEL;
        isopen(false); //m_frctl.isrunning = false;
//            aoptr->islistening(false); //listener.busy = true; //(void*)1;
//        aoptr->stop(); //env);
//        !NAPI_OK(napi_release_threadsafe_function(aoptr->fats, napi_tsfn_release), "Can't release JS fats");
//        aoptr->fats = NULL;
//        aodata->listener.busy = false; //work = 0;
//    bkg.detach();
    }
public: //NAPI methods:
//    napi_ref ref = nullptr; //keep my data valid between open() and close()
    static inline ShmData* my(void* ptr) { return static_cast<ShmData*>(ptr); }
//expose a few little helper/debug methods also:
    static napi_value isopen_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->isopen(), napi_thingy::Boolean{}); }
    static napi_value isvalid_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, my(ptr)->isvalid(), napi_thingy::Boolean{}); }
    static napi_value nattch_getter(napi_env env, void* ptr) /*const*/ { return napi_thingy(env, shmnattch(ptr, SRCLINE), napi_thingy::Int32{}); }
//export control data and methods for JS clients to use:
//        static napi_value my_exports(napi_env env, napi_value exports)
//    static napi_value my_exports(napi_env env)
//    {
//            exports = module_exports(env, exports); //include previous exports
//    }
//    void napi_export(vector_cxx17<my_napi_property_descriptor>& props, napi_env env)
//    napi_value my_exports(napi_env env, napi_value& retval)
    napi_value my_exports(napi_env env) { return my_exports(env, napi_thingy(env, napi_thingy::Object{})); }
    napi_value my_exports(napi_env env, const napi_value& retval)
    {
//            exports = module_exports(env, exports); //include previous exports
        napi_thingy my_exports(env, retval);
        vector_cxx17<my_napi_property_descriptor> props;
//        vector_cxx17<my_napi_property_descriptor>& props, napi_env env)
//static cfg consts:
        add_prop_uint32(VERSION)(props.emplace_back()); //(*pptr++);
//        add_prop_uint32(SHMKEY)(props.emplace_back()); //(*pptr++);
        add_prop_uint32(NUM_UNIV)(props.emplace_back()); //(*pptr++);
        add_prop_uint32("UNIV_MAXLEN", SIZEOF(m_fbque[0].nodes[0]) /*UNIV_MAXLEN_pad*/)(props.emplace_back()); //give caller actual row len for correct node addressing
//expose Protocol types (enum consts):
        add_prop("Protocols", Protocol::my_exports(env))(props.emplace_back());
        add_prop("PerfStats", FrameControl::my_exports_perfinx(env))(props.emplace_back());
//shm data structs:
        add_prop("manifest", /*ManifestType::*/m_manifest.my_exports(env))(props.emplace_back()); //(*pptr++);
//state getters/setters:
        add_getter("isopen", ShmData::isopen_getter, this)(props.emplace_back()); //(*pptr++);
        add_getter("isvalid", ShmData::isvalid_getter, this)(props.emplace_back()); //(*pptr++);
        add_getter("nattch", ShmData::nattch_getter, this)(props.emplace_back()); //(*pptr++);
        debug(9, "export %d more methods/props", props.size());
//export data members in order by address (helpful when debugging against shm seg)
        my_exports += props; props.clear();
//        add_prop("frctl", m_frctl.my_exports(env))(props.emplace_back()); //(*pptr++);
        my_exports = m_frctl.my_exports(env, my_exports); //promote these for easier access;
//        napi_thingy fbque(env);
//        fbque.cre_object();
//        napi_thingy arybuf(env, &nodes[0][0], sizeof(nodes));
//        /*alignas(CACHELEN)*/ NODEVAL nodes[NUM_UNIV][UNIV_MAXLEN_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
//        add_prop("nodes", nodes_export(env))(props.emplace_back()); //(*pptr++);
        napi_thingy spare_arybuf(env, &m_spare[0], sizeof(m_spare));
        napi_thingy spare_typary(env, GPU_NODE_type, SIZEOF(m_spare), spare_arybuf); //UNIV_MAXLEN * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
        add_prop("spares", spare_typary)(props.emplace_back()); //(*pptr++);
//        add_prop("msglog", m_msglog.my_exports(env))(props.emplace_back());
        napi_thingy node_arybuf(env, &m_fbque[0], sizeof(m_fbque));
        napi_thingy fbque_ary(env, napi_thingy::Array{}, SIZEOF(m_fbque));
//        for (auto& fbquent: m_fbque)
        for (auto it = m_fbque.begin(); it != m_fbque.end(); ++it)
        {
            size_t inx = it - m_fbque.begin();
            napi_value fbquent = it->my_exports(env, node_arybuf, inx);
            !NAPI_OK(napi_set_element(env, fbque_ary, inx, fbquent), "Cre inner node ary failed");
        }
        add_prop("nodebufs", fbque_ary)(props.emplace_back()); //(*pptr++);
//??        const int REF_COUNT = 1;
//??        !NAPI_OK(napi_create_reference(env, retval, REF_COUNT, &m_nodes_ref), "Cre ref failed"); //allow to be reused next time
//        add_prop("frctl", frctl_export(env))(props.emplace_back()); //(*pptr++);
//        !NAPI_OK(napi_define_properties(env, retval, props.size(), props.data()), "export protocol props failed");
//methods:
        add_method("open", ShmData::Open_NAPI, this)(props.emplace_back()); //(*pptr++);
        add_method("close", ShmData::Close_NAPI, this)(props.emplace_back()); //(*pptr++);
//        napi_thingy more_retval(env, retval);
//        more_retval += props;
//        return more_retval;
//        return napi_thingy(env, more_retval) += props;
        debug(9, "export %d more methods/props", props.size());
        return my_exports += props;
    }
//"open" GPU port:
    static napi_value Open_NAPI(napi_env env, napi_callback_info info)
    {
        if (!env) return NULL; //Node cleanup mode?
        DebugInOut("Open_napi");

        ShmData* shmptr;
        napi_value argv[1+1], This; //allow 1 extra arg to check for extras
        size_t argc = SIZEOF(argv);
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
        !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&shmptr), "Get cb info failed");
        if (argc > 1) NAPI_exc("expected 0-1 opts arg, got " << argc << " args");
        shmptr->isvalid(env, SRCLINE);
        if (shmptr->isopen()) NAPI_exc("GPU port is already open");
//    if (argc < 1) 
//    napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
//    char str[1024];
//    size_t str_len;
//    status = napi_get_value_string_utf8(env, argv[0], (char *) &str, 1024, &str_len);
//    if (status != napi_ok) { napi_throw_error(env, "EINVAL", "Expected string"); return NULL; }
//    Napi::String str = Napi::String::New(env, )
//        int debug = 33;
        int screen = FIRST_SCREEN;
//    key_t PREALLOC_shmkey = 0;
        int vgroup = 1;
        Uint32 init_color = BLACK;
        /*Nodebuf::Protocol*/ /*Protocol::base_type*/ auto protocol = Protocol::uncast(Protocol::/*Enum::*/WS281X);
        int frtime_msec = 0; //target frame rate; //double fps;
        bool had_opts = false;

//        napi_thingy opts(env, argv[0]);
        if (argc && (valtype(env, argv[0]) != napi_undefined))
        {
            uint32_t listlen;
            napi_value proplist;
            if (valtype(env, argv[0]) != napi_object) NAPI_exc("First arg not object"); //TODO: allow other types?
            !NAPI_OK(napi_get_property_names(env, argv[0], &proplist), "Get prop names failed");
            !NAPI_OK(napi_get_array_length(env, proplist, &listlen), "Get array len failed");
//#if 1
//            static const std::map<const char*, int*> known_opts =
//            static const struct { const char* name; int* valp; } known_opts[] =
//            using KEYTYPE = const char*;
//            using MAPTYPE = std::vector<std::pair<KEYTYPE, int*>>;
            static const str_map<const char*, int*> known_opts = //MAPTYPE known_opts =
            {
//NOTE: NUM_UNIV and UNIV_LEN are determined by video config so they are not selectable by caller
//                {"debug", &debug},
                {"screen", &screen},
                {"vgroup", &vgroup},
                {"init_color", (int*)&init_color},
                {"protocol", &protocol},
                {"frtime_msec", &frtime_msec},
            };
//            std::function<int(KEYTYPE key)> find = [known_opts](KEYTYPE key) -> std::pair<KEYTYPE, int*>*
//            {
//                for (auto pair : known_opts)
//                    if (!strcmp(key, pair.first)) return &pair;
//                return 0;
//            };
//            debug(BLUE_MSG "checking %d option names in list of %d known options" ENDCOLOR, listlen, known_opts.size());
//            for (auto opt : known_opts) debug(BLUE_MSG "key '%s', val@ %p vs. %p" ENDCOLOR, opt.first, opt.second, this);
            for (int i = 0; i < listlen; ++i)
            {
//TODO: add handle_scope? https://nodejs.org/api/n-api.html#n_api_making_handle_lifespan_shorter_than_that_of_the_native_method
                char buf[20]; //= ",";
                size_t buflen;
                napi_thingy propname(env), propval(env);
                !NAPI_OK(napi_get_element(env, proplist, i, &propname.value), "Get array element failed");
                !NAPI_OK(napi_get_value_string_utf8(env, propname, buf, sizeof(buf), &buflen), "Get string failed");
//                strcpy(&buf[1 + buflen], ",");
//                if (strstr(",screen,vgroup,color,protocol,debug,nodes,", buf)) continue;
//                buf[1 + buflen] = '\0';
//                if (!get_prop(env, optsval, buf, &propval.value)) propval.cre_undef();
                !NAPI_OK(napi_get_named_property(env, argv[0], buf, &propval.value), "Get named prop failed");
                debug(17, "find prop[%d/%d] %d:'%s' " << propname << ", value " << propval << ", found? %d", i, listlen, buflen, buf, !!known_opts.find(buf));
                if (!strcmp(buf, "fps")) { double fps; !NAPI_OK(napi_get_value_double(env, propval, &fps), "Get prop failed"); frtime_msec = 1000.0 / fps; } //alias for frame_time_msec; kludge: float not handled by known_opts table
                else if (!known_opts.find(buf)) NAPI_exc("unrecognized option: " << buf << " " << propval); //strlen(buf) - 2, &buf[1]);
                else !NAPI_OK(napi_get_value_int32(env, propval, known_opts.find(buf)->second), "Get " << buf << " opt failed for " << propval);
            }
            had_opts = true;
//#endif
//            opts.get_prop("debug", &m_opts.debug);
//            /*!NAPI_OK(*/opts.get_prop("screen", &m_opts.screen); //, opts.env, "Invalid .screen prop");
//        !NAPI_OK(get_prop(env, argv[0], "shmkey", &PREALLOC_shmkey), "Invalid .shmkey prop");
//            /*!NAPI_OK(*/opts.get_prop("vgroup", &m_opts.vgroup); //, opts.env, "Invalid .vgroup prop");
//            /*!NAPI_OK(*/opts.get_prop("color", &m_opts.init_color); //, opts.env, "Invalid .color prop");
//            int /*int32_t*/ prtemp;
//            opts.get_prop("protocol", &prtemp); //!NAPI_OK(napi_get_value_int32(env, argv[0], &prtemp), "Get uint32 setval failed");
//            m_opts.protocol = static_cast<Nodebuf::Protocol>(prtemp);
//        if (islistening()) debug(RED_MSG "TODO: check for arg mismatch" ENDCOLOR);
        }
        debug(17, "open opts: explicit? %d, screen %d, vgroup %d, init_color 0x%x, protocol %d (%s), frtime_msec %d", had_opts, screen, vgroup, init_color, protocol, Protocol(protocol).toString(), frtime_msec); //, debug);
//internal state:
//        static const Nodebuf::TXTR* PBEOF = (Nodebuf::TXTR*)-5;
//        napi_threadsafe_function fats; //asynchronous thread-safe JavaScript call-back function; can be called from any thread
//        Nodebuf::TXTR::REFILL refill; //void (*m_refill)(Nodebuf::TXTR*); //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
//to "open" gpu port, start up bkg wker:
//??        init_fbque(); //do this before returning to caller
        shmptr->m_frctl.protocol = protocol; //this one is under caller control and passed via shm
//        void gpu_wker(int NUMFR = INT_MAX, int screen = FIRST_SCREEN, SDL_Size* want_wh = NO_SIZE, size_t vgroup = 1, NODEVAL init_color = BLACK, SrcLine srcline = 0)
//        uint32_t ref_count;
//        !NAPI_OK(napi_reference_ref(env, shmptr->ref, &ref_count), "Inc ref count failed");
        std::thread bkg(gpu_wker_static, shmptr, INT_MAX, screen, NO_SIZE, vgroup, init_color, &SRCLINE[0]);
//      try {} catch {}
        bkg.detach();
#if 0
    Uint32 color; //= BLACK;
    napi_value num_arg;
    !NAPI_OK(napi_coerce_to_number(env, argv[0], &num_arg), "Get arg as num failed");
    !NAPI_OK(napi_get_value_uint32(env, num_arg, &color), "Get uint32 colo failed");
//    using LIMIT = limit<pct(50/60)>; //limit brightness to 83% (50 mA/pixel instead of 60 mA); gives 15A/300 pixels, which is a good safety factor for 20A power supplies
//actual work done here; surrounding code is overhead :(
    color = limit<ShmData::BRIGHTEST>(color); //83% //= 3 * 212, //0xD4D4D4, //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA
//    napi_value retval;
//    !NAPI_OK(napi_create_uint32(env, color, &retval), "Cre retval failed");
//    return retval;
    return napi_thingy(env, color);
#endif
//        napi_thingy retval(env, thrinx(bkg.get_id()));
//        return NULL;
//        return retval;
        return napi_thingy(env, thrinx(bkg.get_id()), napi_thingy::Int32{});
    }
//"close" GPU port:
    static napi_value Close_NAPI(napi_env env, napi_callback_info info)
    {
        if (!env) return NULL; //Node cleanup mode?
        DebugInOut("Close_napi");

        ShmData* shmptr;
        napi_value argv[0+1], This; //allow 1 extra arg to check for extras
        size_t argc = SIZEOF(argv);
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
        !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&shmptr), "Get cb info failed");
        if (argc) NAPI_exc("expected 0 args, got " << argc << " arg" << plural(argc));
        shmptr->isvalid(env, SRCLINE);
        if (!shmptr->isopen()) NAPI_exc("GPU port is not already open");
//        uint32_t ref_count;
//        !NAPI_OK(napi_reference_unref(env, shmptr->ref, &ref_count), "Dec ref count failed");
//    if (argc < 1) 
//    napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
//    char str[1024];
//    size_t str_len;
//    status = napi_get_value_string_utf8(env, argv[0], (char *) &str, 1024, &str_len);
//    if (status != napi_ok) { napi_throw_error(env, "EINVAL", "Expected string"); return NULL; }
//    Napi::String str = Napi::String::New(env, )
        shmptr->m_frctl.protocol = Protocol::/*Enum::*/CANCEL;
        shmptr->m_fbque[shmptr->m_frctl.numfr % SIZEOF(shmptr->m_fbque)].ready = ALL_UNIV; //make sure bkg wker sees new protocol
        return napi_thingy(env, shmptr->m_frctl.numfr, napi_thingy::Int32{}); //TODO: what to put here?
//        return retval;
    }
#if 0 //not needed
public: //named arg variants
    template <typename CALLBACK>
    inline static auto gpu_wker(CALLBACK&& named_params)
    {
//        struct CreateParams params;
//        return create(unpack(create_params, named_params), Unpacked{});
        struct //CreateParams
        {
            int NUMFR = INT_MAX;
            int screen = FIRST_SCREEN;
            SDL_Size* wh = NO_SIZE;
            size_t vgroup = 1;
            NODEVAL init_color = BLACK;
            SrcLine srcline = 0;
        } params;
        unpack(params, named_params);
        VOID gpu_wker(params.NUMFR, params.screen, params.wh, params.vgroup, params.init_color, params.srcline);
    }
#endif
private: //helpers
//xfr node (color) values to txtr, bit-bang into currently selected protocol format:
//CAUTION: this needed to run fast because it blocks Node fg thread
    static void xfr_bb(ShmData& shdata, void* txtrbuf, const void* nodes, size_t xfrlen, SrcLine srcline) // = 0) //, SrcLine srcline2 = 0) //h * pitch(NUM_UNIV)
    {
        FramebufQuent& fbquent = shdata.m_fbque[shdata.m_frctl.numfr % SIZEOF(shdata.m_fbque)]; //CAUTION: circular queue
        XFRTYPE bbdata/*[UNIV_MAX]*/[BIT_SLICES]; //3 * NODEBITS]; //bit-bang buf; enough for *1 row* only; dcl in heap so it doesn't need to be fully re-initialized every time
//        SrcLine srcline2 = 0; //TODO: bind from caller?
//printf("here7\n"); fflush(stdout);.wh.h
//            VOID memcpy(pxbuf, pixels, xfrlen);
//            SDL_Size wh(NUM_UNIV, m_cached.wh.h); //use univ len from txtr; nodebuf is oversized (due to H cache padding, vgroup, and compile-time guess on max univ len)
//            int wh = xfrlen; //TODO
//        SDL_Size nodes_wh(NUM_UNIV, gp.m_wh.h);

        if (/*!shdata.m_frctl.wh.w || !shdata.m_frctl.wh.h ||*/ !xfrlen || (shdata.m_frctl.wh.w != NUM_UNIV /*SIZEOF(bbdata)*/) || (xfrlen != shdata.m_frctl.wh.h * sizeof(bbdata) /*gp.m_wh./-*datalen<XFRTYPE>()*-/ w * sizeof(XFRTYPE)*/)) exc_hard("xfr size mismatch: nodebuf " << shdata.m_frctl.wh << " vs. " << SDL_Size(NUM_UNIV, SIZEOF(fbquent.nodes[0]) /*UNIV_MAXLEN_pad*/) << ", byte count " << commas(xfrlen) << " vs, " << commas(shdata.m_frctl.wh.h * sizeof(bbdata)));
        if (nodes != &fbquent.nodes[0][0]) exc_hard("&nodes[0][0] " << nodes << " != &fbquent.nodes[0][0] " << &fbquent.nodes[0][0]);
//        SDL_Size wh_bb(NUM_UNIV, H_PADDED), wh_txtr(XFRW/*_PADDED*/, xfrlen / XFRW/*_PADDED*/ / sizeof(XFRTYPE)); //NOTE: txtr w is XFRW_PADDED, not XFRW
//        if (!(count++ % 100))
//        static int count = 0;
//        if (!count++)
//            debug(BLUE_MSG "bit bang xfr: " << nodes_wh << " node buf => " << gp.m_wh << ENDCOLOR_ATLINE(srcline));
//NOTE: txtrbuf = in-memory texture, nodebuf = just a ptr of my *unformatted* nodes
//this won't work: different sizes        VOID memcpy(txtrbuf, nodebuf, xfrlen);
//        using TXTRBUF = XFRTYPE[gp.m_wh.h][XFRW];
//        TXTRBUF& txtrptr = txtrbuf;y
        XFRTYPE* ptr = static_cast<XFRTYPE*>(txtrbuf);
//allow caller to turn formatting on/off at run-time (only useful for dev/debug, since h/w doesn't change):
//adds no extra run-time overhead if protocol is checked outside the loops
//3x as many x accesses as y accesses are needed, so pixels (horizontally adjacent) are favored over nodes (vertically adjacent) to get better memory cache performance
        static const bool rbswap = false; //isRPi(); //R <-> G swap only matters for as-is display; for pivoted data, user can just swap I/O pins
        /*auto*/ MASK_TYPE dirty = fbquent.ready.load() | (255 * Ashift); //use dirty/ready bits as start bits
        if (shdata.m_frctl.protocol != shdata.m_frctl.prev_protocol) dirty = ALL_UNIV; //protocol/fmt changed; force all nodes to be updated (for dev/debug); wouldn't happen in prod
        debug(19, "xfr " << commas(xfrlen) << " *3, protocol " << shdata.m_frctl.protocol); //static_cast<int>(nodebuf.protocol) << ENDCOLOR);
//        if (debug_level <= 80)
#define DUMP_LEVEL  80
#if MAX_DEBUG_LEVEL >= DUMP_LEVEL //dump
        debug(DUMP_LEVEL, "xfr_bb: " << shdata.m_frctl.wh);
        int yy = shdata.m_frctl.wh.h;
        while ((yy > 1) /*&& (fbquent.nodes[*][yy - 1] == fbquent.nodes[*][yy - 2])*/) //--yy;
            for (int x = 0; x < NUM_UNIV; ++x)
                if (fbquent.nodes[x][yy - 1] != fbquent.nodes[x][yy - 2]) { yy = -yy; break; }
                else if (x == NUM_UNIV - 1) --yy; //truncate repeating rows
        if (yy < 0) yy = -yy; //kludge: restore unique len after outer loop break
        for (int y = 0; y < /*shdata.m_frctl.wh.h*/ yy; ++y) //outer loop = node# within each universe
        {
            std::ostringstream ss;
            ss << "[" << y << "/" << shdata.m_frctl.wh.h << "]:'" << std::hex << &"0x"[(y * NUM_UNIV < 10)? 2: 0] << (y * NUM_UNIV);
            int xx = NUM_UNIV;
            while ((xx > 1) && (fbquent.nodes[xx - 1][y] == fbquent.nodes[xx - 2][y])) --xx; //truncate repeating cells
            for (int x = 0; x < /*NUM_UNIV*/ xx; ++x) //inner loop = universe#
            {
                ss << (x? ", ": ": ") << &"0x"[(fbquent.nodes[x][y] < 10)? 2: 0] << fbquent.nodes[x][y];
//                for (int xx = x + 1; xx < NUM_UNIV; ++xx) //look ahead for changes
//                    if (fbquent.nodes[xx][y] != fbquent.nodes[x][y])) break;
//                    else if (xx == NUM_UNIV - 1) ss << " ...";
            }
            if (xx < NUM_UNIV) ss << " ... x " << (NUM_UNIV - xx);
            ss << std::dec;
            debug(DUMP_LEVEL, ss.str());
        }
        if (yy < shdata.m_frctl.wh.h) debug(DUMP_LEVEL, " ::: x " << (shdata.m_frctl.wh.h - yy));
#endif
        switch (shdata.m_frctl.protocol.value)
        {
            default: //NONE (raw)
                for (int y = 0; y < shdata.m_frctl.wh.h; ++y) //outer loop = node# within each universe
                    for (uint32_t x = 0, /*xofs = 0,*/ xmask = NODEVAL_MSB; x < NUM_UNIV; ++x, /*xofs += nodebuf.wh.h,*/ xmask >>= 1) //inner loop = universe#
                    {
                        NODEVAL color_out = limit<BRIGHTEST>(fbquent.nodes[x][/*xofs +*/ y]); //limit() is marginally useful in this mode, but use it in case view wants accuracy
                        if (!A(color_out) || !(dirty & xmask)) continue; //no change to node; since is portraying nodes so leave old value on screen
                        *ptr++ = *ptr++ = *ptr++ = /*(dirty & xmask)?*/ rbswap? ARGB2ABGR(color_out): color_out; //: BLACK; //copy as-is (3x width)
                    }
//                for (int xy = 0; xy < nodebuf.wh.w * nodebuf.wh.h; ++xy)
//                    *ptr++ = *ptr++ = *ptr++ = (dirty & xmask)? rbswap? ARGB2ABGR(nodebuf.nodes[0][xy]): nodebuf.nodes[0][xy]: BLACK; //copy as-is (3x width)
                break;
            case Protocol::/*Enum::*/DEV_MODE: //partially formatted
                for (int y = 0; y < shdata.m_frctl.wh.h; ++y) //outer loop = node# within each universe
                    for (uint32_t x = 0, /*xofs = 0,*/ xmask = NODEVAL_MSB; x < NUM_UNIV; ++x, /*xofs += nodebuf.wh.h,*/ xmask >>= 1) //inner loop = universe#
                    {
                        static const Uint32 ByteColors[] {RED, GREEN, BLUE}; //only for dev/debug
                        NODEVAL color_out = limit<BRIGHTEST>(fbquent.nodes[x][/*xofs +*/ y]); //limit() is marginally useful in this mode, but use it in case view wants accuracy
//show start + stop bits around unpivoted data:
//NOTE: start/stop bits portray formatted protocol, middle node section *does not*
                        *ptr++ = ByteColors[x / 8] & xmask; //show byte (color) indicator (easier dev/debug); //dirty; //WHITE;
                        if (!A(color_out) || !(dirty & xmask)) ++ptr; //no change; leave old value
                        else *ptr++ = /*(dirty & xmask)?*/ rbswap? ARGB2ABGR(color_out): color_out; //: BLACK; //unpivoted node values
                        *ptr++ = BLACK;
                    }
                break;
            case Protocol::/*Enum::*/WS281X: //fully formatted (24-bit pivot)
                for (int y = 0, yofs = 0; y < shdata.m_frctl.wh.h; ++y, yofs += BIT_SLICES) //TXR_WIDTH) //outer loop = node# within each universe
                {
//initialize 3x signal for this row of 24 WS281X pixels:
//            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
//            {
//                pxbuf32[yofs + x3 + 0] = leading_edges; //WHITE;
//                pxbuf32[yofs + x3 + 1] = BLACK; //data bit body (will be overwritten with pivoted color bits)
////                if (x3) pxbuf32[yofs + x3 - 1] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//                pxbuf32[yofs + x3 + 2] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//            }
                    memset(&ptr[yofs], 0, sizeof(bbdata)); //begin with all bits off and then turn bits on again as needed
                    for (int bit3x = 0; bit3x < BIT_SLICES; bit3x += 3) ptr[yofs + bit3x] = dirty; //WHITE; //leading edge = high; turn on for all universes
//pivot pixel data onto 24 parallel GPIO pins:
//  WS281X = 1, PLAIN_SSR = 2, CHPLEX_SSR = 3,TYPEBITS = 0xF,
// RGSWAP = 0x20, CHECKSUM = 0x40, POLARITY = 0x80};
//NOTE: xmask loop assumes ARGB or ABGR fmt (A in upper byte)
                    for (uint32_t x = 0, xofs = 0, xmask = NODEVAL_MSB /*1 << (NUM_UNIV - 1)*/; x < NUM_UNIV; ++x, xofs += shdata.m_frctl.wh.h, xmask >>= 1) //inner loop = universe#
                    {
                        XFRTYPE color_out = limit<BRIGHTEST>(fbquent.nodes[x][y]); //[0][xofs + y]; //pixels? pixels[xofs + y]: fill;
//                            if (!A(color) || (!R(color) && !G(color) && !B(color))) continue; //no data to pivot
//                        if (rbswap) color_out = ARGB2ABGR(color_out); //user-requested explicit R <-> G swap
//no                            color = ARGB2ABGR(color); //R <-> G swap doesn't need to be automatic for RPi; user can swap GPIO pins
//                        if (MAXBRIGHT && (MAXBRIGHT < 100)) color_out = limit(color_out); //limit brightness/power
//WS281X encoding: 1/3 white, 1/3 data, 1/3 black
//3x as many x accesses as y accesses, so favor pixels over nodes in memory cache
//                        /*bbdata[y][xofs + 0]*/ *ptr++ = WHITE;
//                        /*bbdata[y][xofs + 1]*/ *ptr++ = gp.nodes[x][y];
//                        /*if (u) bbdata[y][xofs - 1]*/ *ptr++ = BLACK; //CAUTION: last 1/3 (hsync) missing from last column
//24 WS281X data bits spread across 72 screen pixels = 3 pixels per WS281X data bit:
#if 1
//TODO: try 8x loop with r_yofs/r_msb, g_yofs/g_msb, b_yofs/b_msb
                        for (int bit3x = 0+1; bit3x < BIT_SLICES; bit3x += 3, color_out <<= 1)
//                        {
//                            ptr[yofs + bit3 + 0] |= xmask; //leading edge = high
                            if (color_out & NODEVAL_MSB) ptr[yofs + bit3x] |= xmask; //set this data bit for current node
//                                pxbuf32[yofs + bit3 + 2] &= ~xmask; //trailing edge = low
//                        }
#else
//set data bits for current node:
                    if (color_out & 0x800000) ptr[yofs + 3*0 + 1] |= xmask;
                    if (color_out & 0x400000) ptr[yofs + 3*1 + 1] |= xmask;
                    if (color_out & 0x200000) ptr[yofs + 3*2 + 1] |= xmask;
                    if (color_out & 0x100000) ptr[yofs + 3*3 + 1] |= xmask;
                    if (color_out & 0x080000) ptr[yofs + 3*4 + 1] |= xmask;
                    if (color_out & 0x040000) ptr[yofs + 3*5 + 1] |= xmask;
                    if (color_out & 0x020000) ptr[yofs + 3*6 + 1] |= xmask;
                    if (color_out & 0x010000) ptr[yofs + 3*7 + 1] |= xmask;
                    if (color_out & 0x008000) ptr[yofs + 3*8 + 1] |= xmask;
                    if (color_out & 0x004000) ptr[yofs + 3*9 + 1] |= xmask;
                    if (color_out & 0x002000) ptr[yofs + 3*10 + 1] |= xmask;
                    if (color_out & 0x001000) ptr[yofs + 3*11 + 1] |= xmask;
                    if (color_out & 0x000800) ptr[yofs + 3*12 + 1] |= xmask;
                    if (color_out & 0x000400) ptr[yofs + 3*13 + 1] |= xmask;
                    if (color_out & 0x000200) ptr[yofs + 3*14 + 1] |= xmask;
                    if (color_out & 0x000100) ptr[yofs + 3*15 + 1] |= xmask;
                    if (color_out & 0x000080) ptr[yofs + 3*16 + 1] |= xmask;
                    if (color_out & 0x000040) ptr[yofs + 3*17 + 1] |= xmask;
                    if (color_out & 0x000020) ptr[yofs + 3*18 + 1] |= xmask;
                    if (color_out & 0x000010) ptr[yofs + 3*19 + 1] |= xmask;
                    if (color_out & 0x000008) ptr[yofs + 3*20 + 1] |= xmask;
                    if (color_out & 0x000004) ptr[yofs + 3*21 + 1] |= xmask;
                    if (color_out & 0x000002) ptr[yofs + 3*22 + 1] |= xmask;
                    if (color_out & 0x000001) ptr[yofs + 3*23 + 1] |= xmask;
#endif
                    }
                }
                break;
        }
        shdata.m_frctl.prev_protocol = shdata.m_frctl.protocol;
    }
};


#if 0
//addon state/context data:
//using structs to allow inline member init
//#define GpuPortData  GpuPortData_new
struct GpuPortData
{
    AutoShmary<ShmData> m_shdata;
//    ShmData* m_shdata = 0;
    const elapsed_t m_started = now_msec();
    const SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
public: //ctors/dtors
    GpuPortData(SrcLine srcline = 0): m_srcline(NVL(srcline, SRCLINE)) {}
public: //operators
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const GpuPortData& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
//        SDL_version ver;
//        SDL_GetVersion(&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
//        ostrm << "SDL_Lib {version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d}", ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//    %p, valid? %d" ENDCOLOR, aodata.get(), aodata.get()->isvalid());
        ostrm << "GpuPortData"; //<< my_templargs();
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
//        if (!that.isvalid()) return ostrm << " INVALID}";
        ostrm << ", shdata " << *that.m_shdata.ptr();
        ostrm << ", age " << /*elapsed(that.started)*/ commas(elapsed(that.m_started)) << " msec";
        return ostrm << "}";
    }
};
#endif


#if 0
#define GpuPortData  GpuPortData_old
struct Nodebuf
{
//    static const bool CachedWrapper = false; //true; //BROKEN; leave turned OFF
//probably a little too much compile-time init, but here goes ...
    using NODEVAL = Uint32; //data type for node colors (ARGB)
    static const napi_typedarray_type GPU_NODE_type = napi_uint32_array; //NOTE: must match NODEVAL type
    using XFRTYPE = Uint32; //data type for bit banged node bits (ARGB)
    using TXTR = SDL_AutoTexture<XFRTYPE>;
//settings that must match h/w:
//TODO: move some of this to run-time or extern #include
    static const int IOPINS = 24; //total #I/O pins available (h/w dependent); also determined by device overlay
    static const int HWMUX = 0; //#I/O pins (0..23) to use for external h/w mux
//derived settings:
    static const int NUM_UNIV = (1 << HWMUX) * (IOPINS - HWMUX); //max #univ with/out external h/w mux
//settings that must match (cannot exceed) video config:
//put 3 most important constraints first, 4th will be dependent on other 3
//default values are for my layout
    static const int CLOCK = 52 MHz; //pixel clock speed (constrained by GPU)
    static const int HTOTAL = 1536; //total x res including blank/sync (might be contrained by GPU); 
    static const int FPS = 30; //target #frames/sec
//derived settings:
    static const int UNIV_MAXLEN_raw = VRES_CONSTRAINT(CLOCK, HTOTAL, FPS); //max #nodes per univ; above values give ~1128
    static const int UNIV_MAXLEN_pad = cache_pad<NODEVAL>(UNIV_MAXLEN_raw); //1132 for above values; padded for better memory cache performance
//    static const SDL_Size max_wh(NUM_UNIV, UNIV_MAXLEN_pad);
    typedef typename std::conditional<(NUM_UNIV <= 32), uint32_t, std::bitset<NUM_UNIV>>::type MASK_TYPE;
//    using MASK_TYPE = uint32_t; //using UNIV_MASK = XFRTYPE; //cross-univ bitmaps
//settings determined by s/w:
    static const int NODEBITS = 24; //# bits to send for each WS281X node (protocol dependent)
    static const int BIT_SLICES = NODEBITS * 3; //divide each node data bit into 1/3s (last 1/3 of last node bit will overlap hsync)
    static const unsigned int NODEVAL_MSB = 1 << (NODEBITS - 1);
    static const int BRIGHTEST = pct(50/60);
//    static const unsigned int NODEVAL_MASK = 1 << NODEBITS - 1;
    static const MASK_TYPE UNIV_MASK = (1 << NUM_UNIV) - 1;
    static const MASK_TYPE ALL_UNIV = UNIV_MASK; //NODEVAL_MASK;
    static const MASK_TYPE NOT_READY = ALL_UNIV >> (NUM_UNIV / 2); //turn off half the universes to use as intermediate value
//    std::unique_ptr<NODEVAL> m_nodes; //define as member data to avoid WET defs needed for class derivation; NOTE: must come before depend refs below; //NODEBUF_FrameInfo, NODEBUF_deleter>; //DRY kludge
    using SYNCTYPE = BkgSync<MASK_TYPE, true>;
protected: //data members
//    struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
//    std::unique_ptr<TXTR> m_txtr; //kludge: ptr allows texture creation to be postponed until params known
    TXTR m_txtr; //(TXTR::NullOkay{}); //leave empty until bkg thread starts
//    typedef std::function<void(void* dest, const void* src, size_t len)> XFR; //void* (*XFR)(void* dest, const void* src, size_t len); //NOTE: must match memcpy sig; //decltype(memcpy);
//    const /*std::function<void(void*, const void*, size_t)>*/TXTR::XFR m_xfr(); //protocol formatter; bit-bangs caller-defined node colors into protocol format; memcpy signature; TODO: try to use AutoTexture<>::XFR; TODO: find out why const& no worky
    const TXTR::XFR m_xfr; //(std::bind(xfr_bb, std::ref(*this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, SRCLINE)); //protocol bit-banger shim
//    using SHARED_INFO = SharedInfo<NUM_UNIV, UNIV_MAX, SIZEOF(m_txtr.perf_stats), NODEVAL>;
//    using REFILL = std::function<void(void)>; //mySDL_AutoTexture* txtr)>;
//    NODEVAL& nodes;
public: //data members
    enum class Protocol: int32_t { NONE = 0, DEV_MODE, WS281X, INVALID};
//public: //data members
//    /*txtr_bb*/ /*SDL_AutoTexture<XFRTYPE>*/ TXTR m_txtr; //in-memory copy of bit-banged node (color) values (formatted for protocol)
    Protocol protocol = Protocol::NONE; //WS281X;
    Protocol prev_protocol = Protocol::INVALID; //track protocol changes so all nodes can be updated
//    static const int NUM_STATS = SIZEOF(TXTR::perf_stats);
//    typedef std::function<void(mySDL_AutoTexture* txtr)> REFILL; //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
    SYNCTYPE dirty; //= -1; //one Ready bit for each universe; init to bad value to force sync first time
    CONST SDL_Size wh/*(0, 0)*/, view; //(0, 0); //#univ, univ len for node values, display viewport
//TODO: use alignof here instead of cache_pad
    static const napi_typedarray_type perf_stats_type = napi_uint32_array; //NOTE: must match elapsed_t
    elapsed_t perf_stats[SIZEOF(TXTR::perf_stats) + 1] = {0}; //1 extra counter for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
//put nodes last in case caller overruns boundary:
//TODO: use alignof() for node rows:
    typedef NODEVAL NODEBUF[NUM_UNIV][UNIV_MAXLEN_pad];
    NODEVAL nodes[NUM_UNIV][UNIV_MAXLEN_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
public: //ctors/dtors
    explicit Nodebuf(): // /*napi_env env*/):
//        m_cached(env), //allow napi values to be inited
        m_xfr(std::bind(xfr_bb, std::ref(*this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, SRCLINE)), //protocol bit-banger shim
        m_txtr(TXTR::NullOkay{}), //leave empty until bkg thread starts
        dirty(NOT_READY) //>> (NUM_UNIV / 2)) //init to intermediate value to force sync first time (don't use -1 in case all bits valid)
        {}
//        m_cached(nullptr),
//        { reset(); }
    ~Nodebuf() { reset(); }
public: //operators
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const Nodebuf& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        static const std::map<Protocol, const char*> names =
        {
            {Protocol::NONE, "NONE"},
            {Protocol::DEV_MODE, "DEV MODE"},
            {Protocol::WS281X, "WS281X"},
        };
//        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
//        SDL_version ver;
//        SDL_GetVersion(&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
//        ostrm << "SDL_Lib {version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d}", ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//    %p, valid? %d" ENDCOLOR, aodata.get(), aodata.get()->isvalid());
        ostrm << "Nodebuf"; //<< my_templargs();
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
//        if (!that.isvalid()) return ostrm << " INVALID}";
//        ostrm << ", protocol " << NVL(ProtocolName(that.protocol), "??PROTOCOL??");
//        ostrm << ", fr time " << that.frame_time << " (" << (1 / that.frame_time) << " fps)";
//        ostrm << ", mine? " << that.ismine(); //<< " (owner 0x" << std::hex << that.frinfo.owner << ")" << std::dec;
//        ostrm << ", exc " << that.excptr.what(); //that.excptr << " (" <<  << ")"; //= nullptr; //TODO: check for thread-safe access
//        ostrm << ", owner 0x" << std::hex << that.owner << std::dec; //<< " " << sizeof(that.owner);
        ostrm << ", protocol " << NVL(unmap(names, that.protocol)/*ProtocolName(that.protocol)*/, "??PROTOCOL??");
        ostrm << ", dirty 0x" << std::hex << that.dirty/*.load()*/ << std::dec; //sizeof(that.dirty) << ": 0x" << std::hex << that.dirty.load() << std::dec;
//        ostrm << ", #fr " << that.numfr.load();
        ostrm << ", wh " << that.wh << ", view " << that.view;
//        SDL_Size wh(SIZEOF(that./*shdata.*/nodes), SIZEOF(that./*shdata.*/nodes[0]));
        ostrm << ", nodes@ " << &that.nodes[0][0] << ".." << &that.nodes[NUM_UNIV][0]; //+" << commas(sizeof(that.nodes)) << " (" << wh << ")";
//                ostrm << ", time " << that.nexttime.load();
//        ostrm << ", cached node wrapper " << that.m_cached;
        ostrm << ", txtr " << that.m_txtr;
//        ostrm << ", age " << /*elapsed(that.started)*/ that.elapsed() << " sec";
        return ostrm << "}";
    }
//    static inline const char* ProtocolName(Protocol key)
//        return unmap(names, key); //names;
public: //methods
//    static size_t cache_pad32(size_t count) return { cache_pad(count * sizeof(NODEVAL)) / sizeof(NODEVAL); }
    double frame_time = 0; //kludge: set value here to match txtr; used in FrameInfo
//called by dtor:
    void reset() //bool screen_only = false)
    {
//        m_txtr.reset(0); //delete wnd/txtr
        TXTR empty(TXTR::NullOkay{});
        m_txtr = empty; //CAUTION: force AutoTexture to release txtr *and* wnd
//        if (screen_only) return; //leave all other data intact for cb one last time
//        wh = view = SDL_Size(0, 0);
//        frame_time = 0;
//        dirty.store(0);
//NO        m_cached.cre_undef(); //NOTE: can't do this unless called from napi with active env
//        memset(&perf_stats[0], 0, sizeof(perf_stats));
    }
//called by GpuPortData dtor in fg thread:
    void reset(napi_env env)
    {
        if (m_cached) !NAPI_OK(napi_delete_reference(env, m_cached), "Del ref failed");
        m_cached = nullptr;
    }
//called by bkg thread:
    void reset(int screen /*= FIRST_SCREEN*/, int vgroup /*= 0*/, NODEVAL init_color /*= BLACK*/) //SDL_Size& new_wh) //, SrcLine srcline = 0)
    {
//nodes: #univ x univ_len, txtr: 3 * 24 x univ len, view (clipped): 3 * 24 - 1 x univ len
//debug("here50" ENDCOLOR);
        wh.w = NUM_UNIV; //nodes
        view.w = BIT_SLICES - 1; //last 1/3 bit will overlap hblank; clip from visible part of window
        view.h = wh.h = std::min(divup(ScreenInfo(screen, SRCLINE)->bounds.h, vgroup? vgroup: 1), static_cast<int>(SIZEOF(nodes[0]))); //univ len == display height
//NOTE: loop (1 write/element) is more efficient than memcpy (1 read + 1 write / element)
        for (int i = 0; i < SIZEOF_2D(nodes); ++i) nodes[0][i] = BLACK; //clear *entire* buf in case h < max and caller wants linear (1D) addressing
//        wh.h = new_wh.h; //cache_pad32(new_wh.h); //pad univ to memory cache size for better memory perf (multi-proc only)
//        m_debug3(m_view.h),
//TODO: don't recreate if already exists with correct size
//debug("here51" ENDCOLOR);
//{ DebugInOut("assgn new txtr", SRCLINE);
        SDL_Size txtr_wh(BIT_SLICES, wh.h);
        TXTR m_txtr2 = TXTR::create(NAMED{ _.wh = &txtr_wh; _.view_wh = &view, _.screen = screen; _.init_color = init_color; SRCLINE; });
        m_txtr = m_txtr2; //kludge: G++ thinks m_txtr is a ref so assign create() to temp first
//}
        debug(19, BLUE_MSG "txtr after reset " << m_txtr << ENDCOLOR);
        m_txtr.perftime(); //kludge: flush perf timer, but leave a little overhead so first-time results are realistic
//debug("here52" ENDCOLOR);
//        m_txtr = txtr.release(); //kludge: xfr ownership from temp to member
//done by main:        dirty.store(0); //first sync with client thread(s)
//debug("here53" ENDCOLOR);
        const ScreenConfig* const m_cfg = getScreenConfig(screen, SRCLINE); //NVL(srcline, SRCLINE)); //get this first for screen placement and size default; //CAUTION: must be initialized before txtr and frame_time (below)
        if (!m_cfg) exc_hard(RED_MSG "can't get screen config" ENDCOLOR);
        frame_time = m_cfg->frame_time();
        if (!frame_time /*|| !m_cfg->dot_clock || !m_cfg->htotal || !m_cfg->vtotal*/) frame_time = 1.0 / FPS_CONSTRAINT(NVL(m_cfg->dot_clock * 1000, CLOCK), NVL(m_cfg->htotal, HTOTAL), NVL(m_cfg->vtotal, UNIV_MAXLEN_raw)); //estimate from known info
        memset(&perf_stats[0], 0, sizeof(perf_stats));
//TODO?        wrap(env);
//debug("here54" ENDCOLOR);
//        if (UNIV_LEN > UNIV_MAX) exc_soft(RED_MSG "video settings " << *m_cfg << ", vgroup " << vgroup << " require univ len " << UNIV_LEN << " exceeding max " << UNIV_MAX << " allowed by compiled node buf" << ENDCOLOR_ATLINE(srcline));
//        size_t len = new_wh.w * new_wh.h;
//        m_nodes.reset(new NODEVAL[len]);
    }
//create typed array wrapper for nodes:
//    napi_thingy m_cached; //wrapped nodebuf for napi
    napi_ref m_cached = nullptr;
    napi_value wrap(napi_env env) //, napi_value* valp = 0)
    {
//can't cache :(        if (nodes.arytype() != napi_uint32_array) //napi_typedarray_type)
//        {
        if (!env) return NULL; //Node cleanup mode?
#if 0
        debug(BLUE_MSG "wrap nodebuf, ret? %d, cached " << m_cached << ENDCOLOR, !!valp);
#endif
        napi_thingy retval(env);
        if (m_cached) !NAPI_OK(napi_get_reference_value(env, m_cached, &retval.value), "Get ret val failed");
//        retval = m_cached.value();
//        m_cached.env = env;
        static int count = 0;
        debug(19, BLUE_MSG "wrap nodebuf[%d]: cached " << retval << ", ret? %d" ENDCOLOR, count++, retval.type() == napi_object);
//        if (/*CachedWrapper && valp &&*/ (m_cached.env == env) && (m_cached.type() == napi_object)) { *valp = m_cached.value; return; }
//        if (m_cached.type() == napi_object) return m_cached;
//        napi_thingy frinfo(env, napi_thingy::Object{});
//debug("here71" ENDCOLOR);
        if (retval.type() == napi_object) return retval;
//        retval.cre_object();
//        if (env != m_cached.env) debug(YELLOW_MSG "env changed: 0x" << std::hex << env << " vs. 0x" << m_cached.env << std::dec << ENDCOLOR);
//can't cache :(        if (frinfo.type() != napi_object)
//CAUTION:
//no        if (CachedWrapper && valp && (m_cached.env == env) && (m_cached.arytype() == GPU_NODE_type /*m_cached.type() != napi_undefined*/)) { *valp = m_cached.value; return; }
//        napi_thingy frinfo(env);
//        napi_thingy arybuf(env), nodes(env);
//napi_get_null(env, &arybuf.value);
//napi_get_null(env, &nodes.value);
//            void* NO_HINT = NULL; //optional finalize_hint
//debug("env " << env << ", size " << commas(sizeof(gpu_wker->m_nodes)) << ENDCOLOR);
    //        debug("arybuf1 " << arybuf << ENDCOLOR);
    //        !NAPI_OK(napi_create_int32(env, 1234, &arybuf.value), "cre int32 failed");
    //        debug("arybuf2 " << arybuf << ENDCOLOR);
    //        !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &arybuf.value), "cre str failed");
    //        debug("arybuf3 " << arybuf << ENDCOLOR);
    //        !NAPI_OK(napi_create_external_arraybuffer(env, &junk[0][0], sizeof(junk), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
    //        debug("arybuf4 " << arybuf << ENDCOLOR);
        if (!wh.w || !wh.h) NAPI_exc("no nodes");
        napi_thingy arybuf(env, &nodes[0][0], sizeof(nodes));
//        arybuf.cre_ext_arybuf(&nodes[0][0], sizeof(nodes));
//        !NAPI_OK(napi_create_external_arraybuffer(env, &/*gpu_wker->*/m_shdata.nodes[0][0], sizeof(/*gpu_wker->*/m_shdata.nodes), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
//        debug(YELLOW_MSG "arybuf5 " << arybuf << ", dims " << wh << ENDCOLOR);
//            debug("nodes1 " << nodes << ENDCOLOR);
//            !NAPI_OK(napi_create_int32(env, 1234, &nodes.value), "cre int32 failed");
//            debug("nodes2 " << nodes << ENDCOLOR);
//            !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &nodes.value), "cre str failed");
//            debug("nodes3 " << nodes << ENDCOLOR);
//            !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF_2D(junk), arybuf.value, 0, &nodes.value), "Cre nodes typed array failed");
//            debug("nodes4 " << nodes << ENDCOLOR);
//        m_cached.object(env);
//        debug("cre typed array nodes wrapper " << wh << ENDCOLOR);
//        m_cached.cre_typed_ary(env, GPU_NODE_type, wh.w * wh.h, arybuf);
#if 0 //1D array; caller must use UNIV_MAXLEN_pad in index arithmeric :(
        napi_thingy typary(env, GPU_NODE_type, wh.w * wh.h, arybuf);
#else //2D array; helps separate univ from eacher
        napi_thingy univ_ary(env);
        !NAPI_OK(napi_create_array_with_length(env, NUM_UNIV, &univ_ary.value), "Cre outer univ ary failed");
//    NODEVAL nodes[NUM_UNIV][UNIV_MAXLEN_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
        for (int x = 0; x < /*wh.w*/ NUM_UNIV; ++x)
        {
//TODO: add handle_scope? https://nodejs.org/api/n-api.html#n_api_making_handle_lifespan_shorter_than_that_of_the_native_method
            napi_thingy node_typary(env, GPU_NODE_type, /*wh.h*/ UNIV_MAXLEN_pad, arybuf, x * sizeof(nodes[0])); //UNIV_MAXLEN_pad * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
            !NAPI_OK(napi_set_element(env, univ_ary, x, node_typary), "Cre inner node typary failed");
        }
#endif
        debug(19, YELLOW_MSG "nodes5 " << univ_ary << ENDCOLOR);
//        debug(YELLOW_MSG "nodes typed array created: &node[0][0] " << &/*gpu_wker->*/m_shdata.nodes[0][0] << ", #bytes " <<  commas(sizeof(/*gpu_wker->*/m_shdata.nodes)) << ", " << commas(SIZEOF_2D(/*gpu_wker->*/m_shdata.nodes)) << " " << NVL(TypeName(napi_uint32_array)) << " elements, arybuf " << arybuf << ", nodes thingy " << nodes << ENDCOLOR);
//        }
//        if (nodes.env != env) NAPI_exc("nodes env mismatch");
//        if (valp) *valp = m_cached.value;
//        *valp = wrapper;
        const int REF_COUNT = 1;
        !NAPI_OK(napi_create_reference(env, univ_ary, REF_COUNT, &m_cached), "Cre ref failed"); //allow to be reused next time
        return univ_ary;
    }
    void update(TXTR::REFILL& refill, SrcLine srcline = 0)
    {
//        m_frinfo.dirty.wait(ALL_UNIV, NVL(srcline, SRCLINE)); //wait for all universes to be rendered
        perf_stats[0] += m_txtr.perftime(); //measure caller's time (presumably for rendering); //1000); //ipc wait time (msec)
        VOID m_txtr.update(NAMED{ _.pixels = /*&m_xfrbuf*/ &nodes[0][0]; _.perf = &perf_stats[1]; _.xfr = m_xfr; _.refill = refill; SRCLINE; });
//TODO: fix ipc race condition here:
//        for (int i = 0; i < SIZEOF(perf_stats); ++i) m_frinfo.times[i] += perf_stats[i];
//        RenderPresent();
    }
#if 0
public: //named arg variants
    template <typename CALLBACK>
    inline void reset(CALLBACK&& named_params)
    {
        struct //ResetParams
        {
            bool screen_only = false;
            int screen = FIRST_SCREEN;
            int vgroup = 0;
            NODEVAL init_color = BLACK;
            SrcLine srcline = 0;
        } params;
        unpack(params, named_params);
        if (??) VOID reset(params.screen, params.vgroup, params.init_color);
        else VOID reset(params.screen_only);
    }
#endif
private: //helpers
//xfr node (color) values to txtr, bit-bang into currently selected protocol format:
//CAUTION: this needed to run fast because it blocks Node fg thread
    static void xfr_bb(Nodebuf& nodebuf, void* txtrbuf, const void* nodes, size_t xfrlen, SrcLine srcline) // = 0) //, SrcLine srcline2 = 0) //h * pitch(NUM_UNIV)
    {
        XFRTYPE bbdata/*[UNIV_MAX]*/[BIT_SLICES]; //3 * NODEBITS]; //bit-bang buf; enough for *1 row* only; dcl in heap so it doesn't need to be fully re-initialized every time
//        SrcLine srcline2 = 0; //TODO: bind from caller?
//printf("here7\n"); fflush(stdout);.wh.h
//            VOID memcpy(pxbuf, pixels, xfrlen);
//            SDL_Size wh(NUM_UNIV, m_cached.wh.h); //use univ len from txtr; nodebuf is oversized (due to H cache padding, vgroup, and compile-time guess on max univ len)
//            int wh = xfrlen; //TODO
//        SDL_Size nodes_wh(NUM_UNIV, gp.m_wh.h);
        if (!nodebuf.wh.w || !nodebuf.wh.h || !xfrlen || (nodebuf.wh.w != NUM_UNIV /*SIZEOF(bbdata)*/) || (xfrlen != nodebuf.wh.h * sizeof(bbdata) /*gp.m_wh./-*datalen<XFRTYPE>()*-/ w * sizeof(XFRTYPE)*/)) exc_hard(RED_MSG "xfr size mismatch: nodebuf " << nodebuf.wh << " vs. " << SDL_Size(NUM_UNIV, UNIV_MAXLEN_pad) << ", byte count " << commas(xfrlen) << " vs, " << commas(nodebuf.wh.h * sizeof(bbdata)) << ENDCOLOR);
        if (nodes != &nodebuf.nodes[0][0]) exc_hard(RED_MSG "&nodes[0][0] " << nodes << " != &nodebuf.nodes[0][0] " << &nodebuf.nodes[0][0] << ENDCOLOR);
//        SDL_Size wh_bb(NUM_UNIV, H_PADDED), wh_txtr(XFRW/*_PADDED*/, xfrlen / XFRW/*_PADDED*/ / sizeof(XFRTYPE)); //NOTE: txtr w is XFRW_PADDED, not XFRW
//        if (!(count++ % 100))
//        static int count = 0;
//        if (!count++)
//            debug(BLUE_MSG "bit bang xfr: " << nodes_wh << " node buf => " << gp.m_wh << ENDCOLOR_ATLINE(srcline));
//NOTE: txtrbuf = in-memory texture, nodebuf = just a ptr of my *unformatted* nodes
//this won't work: different sizes        VOID memcpy(txtrbuf, nodebuf, xfrlen);
//        using TXTRBUF = XFRTYPE[gp.m_wh.h][XFRW];
//        TXTRBUF& txtrptr = txtrbuf;y
        XFRTYPE* ptr = static_cast<XFRTYPE*>(txtrbuf);
//allow caller to turn formatting on/off at run-time (only useful for dev/debug, since h/w doesn't change):
//adds no extra run-time overhead if protocol is checked outside the loops
//3x as many x accesses as y accesses are needed, so pixels (horizontally adjacent) are favored over nodes (vertically adjacent) to get better memory cache performance
        static const bool rbswap = false; //isRPi(); //R <-> G swap only matters for as-is display; for pivoted data, user can just swap I/O pins
        /*auto*/ MASK_TYPE dirty = nodebuf.dirty.load() | (255 * Ashift); //use dirty/ready bits as start bits
        if (nodebuf.protocol != nodebuf.prev_protocol) dirty = ALL_UNIV; //protocol/fmt changed; force all nodes to be updated (for dev/debug); wouldn't happen in prod
        debug(19, BLUE_MSG "xfr " << xfrlen << " *3, protocol " << static_cast<int>(nodebuf.protocol) << ENDCOLOR);
        switch (nodebuf.protocol)
        {
            default: //NONE (raw)
                for (int y = 0; y < nodebuf.wh.h; ++y) //outer loop = node# within each universe
                    for (uint32_t x = 0, /*xofs = 0,*/ xmask = NODEVAL_MSB; x < NUM_UNIV; ++x, /*xofs += nodebuf.wh.h,*/ xmask >>= 1) //inner loop = universe#
                    {
                        NODEVAL color_out = limit<BRIGHTEST>(nodebuf.nodes[x][/*xofs +*/ y]); //limit() is marginally useful in this mode, but use it in case view wants accuracy
                        if (!A(color_out) || !(dirty & xmask)) continue; //no change to node; since is portraying nodes so leave old value on screen
                        *ptr++ = *ptr++ = *ptr++ = /*(dirty & xmask)?*/ rbswap? ARGB2ABGR(color_out): color_out; //: BLACK; //copy as-is (3x width)
                    }
//                for (int xy = 0; xy < nodebuf.wh.w * nodebuf.wh.h; ++xy)
//                    *ptr++ = *ptr++ = *ptr++ = (dirty & xmask)? rbswap? ARGB2ABGR(nodebuf.nodes[0][xy]): nodebuf.nodes[0][xy]: BLACK; //copy as-is (3x width)
                break;
            case Protocol::DEV_MODE: //partially formatted
                for (int y = 0; y < nodebuf.wh.h; ++y) //outer loop = node# within each universe
                    for (uint32_t x = 0, /*xofs = 0,*/ xmask = NODEVAL_MSB; x < NUM_UNIV; ++x, /*xofs += nodebuf.wh.h,*/ xmask >>= 1) //inner loop = universe#
                    {
                        static const Uint32 ByteColors[] {RED, GREEN, BLUE}; //only for dev/debug
                        NODEVAL color_out = limit<BRIGHTEST>(nodebuf.nodes[x][/*xofs +*/ y]); //limit() is marginally useful in this mode, but use it in case view wants accuracy
//show start + stop bits around unpivoted data:
//NOTE: start/stop bits portray formatted protocol, middle node section *does not*
                        *ptr++ = ByteColors[x / 8] & xmask; //show byte (color) indicator (easier dev/debug); //dirty; //WHITE;
                        if (!A(color_out) || !(dirty & xmask)) ++ptr; //no change; leave old value
                        else *ptr++ = /*(dirty & xmask)?*/ rbswap? ARGB2ABGR(color_out): color_out; //: BLACK; //unpivoted node values
                        *ptr++ = BLACK;
                    }
                break;
            case Protocol::WS281X: //fully formatted (24-bit pivot)
                for (int y = 0, yofs = 0; y < nodebuf.wh.h; ++y, yofs += BIT_SLICES) //TXR_WIDTH) //outer loop = node# within each universe
                {
//initialize 3x signal for this row of 24 WS281X pixels:
//            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
//            {
//                pxbuf32[yofs + x3 + 0] = leading_edges; //WHITE;
//                pxbuf32[yofs + x3 + 1] = BLACK; //data bit body (will be overwritten with pivoted color bits)
////                if (x3) pxbuf32[yofs + x3 - 1] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//                pxbuf32[yofs + x3 + 2] = BLACK; //trailing edge of data bits (right-most overlaps H-blank)
//            }
                    memset(&ptr[yofs], 0, sizeof(bbdata)); //begin with all bits off and then turn bits on again as needed
                    for (int bit3x = 0; bit3x < BIT_SLICES; bit3x += 3) ptr[yofs + bit3x] = dirty; //WHITE; //leading edge = high; turn on for all universes
//pivot pixel data onto 24 parallel GPIO pins:
//  WS281X = 1, PLAIN_SSR = 2, CHPLEX_SSR = 3,TYPEBITS = 0xF,
// RGSWAP = 0x20, CHECKSUM = 0x40, POLARITY = 0x80};
//NOTE: xmask loop assumes ARGB or ABGR fmt (A in upper byte)
                    for (uint32_t x = 0, xofs = 0, xmask = NODEVAL_MSB /*1 << (NUM_UNIV - 1)*/; x < NUM_UNIV; ++x, xofs += nodebuf.wh.h, xmask >>= 1) //inner loop = universe#
                    {
                        XFRTYPE color_out = limit<BRIGHTEST>(nodebuf.nodes[x][y]); //[0][xofs + y]; //pixels? pixels[xofs + y]: fill;
//                            if (!A(color) || (!R(color) && !G(color) && !B(color))) continue; //no data to pivot
//                        if (rbswap) color_out = ARGB2ABGR(color_out); //user-requested explicit R <-> G swap
//no                            color = ARGB2ABGR(color); //R <-> G swap doesn't need to be automatic for RPi; user can swap GPIO pins
//                        if (MAXBRIGHT && (MAXBRIGHT < 100)) color_out = limit(color_out); //limit brightness/power
//WS281X encoding: 1/3 white, 1/3 data, 1/3 black
//3x as many x accesses as y accesses, so favor pixels over nodes in memory cache
//                        /*bbdata[y][xofs + 0]*/ *ptr++ = WHITE;
//                        /*bbdata[y][xofs + 1]*/ *ptr++ = gp.nodes[x][y];
//                        /*if (u) bbdata[y][xofs - 1]*/ *ptr++ = BLACK; //CAUTION: last 1/3 (hsync) missing from last column
//24 WS281X data bits spread across 72 screen pixels = 3 pixels per WS281X data bit:
#if 1
//TODO: try 8x loop with r_yofs/r_msb, g_yofs/g_msb, b_yofs/b_msb
                        for (int bit3x = 0+1; bit3x < BIT_SLICES; bit3x += 3, color_out <<= 1)
//                        {
//                            ptr[yofs + bit3 + 0] |= xmask; //leading edge = high
                            if (color_out & NODEVAL_MSB) ptr[yofs + bit3x] |= xmask; //set this data bit for current node
//                                pxbuf32[yofs + bit3 + 2] &= ~xmask; //trailing edge = low
//                        }
#else
//set data bits for current node:
                    if (color_out & 0x800000) ptr[yofs + 3*0 + 1] |= xmask;
                    if (color_out & 0x400000) ptr[yofs + 3*1 + 1] |= xmask;
                    if (color_out & 0x200000) ptr[yofs + 3*2 + 1] |= xmask;
                    if (color_out & 0x100000) ptr[yofs + 3*3 + 1] |= xmask;
                    if (color_out & 0x080000) ptr[yofs + 3*4 + 1] |= xmask;
                    if (color_out & 0x040000) ptr[yofs + 3*5 + 1] |= xmask;
                    if (color_out & 0x020000) ptr[yofs + 3*6 + 1] |= xmask;
                    if (color_out & 0x010000) ptr[yofs + 3*7 + 1] |= xmask;
                    if (color_out & 0x008000) ptr[yofs + 3*8 + 1] |= xmask;
                    if (color_out & 0x004000) ptr[yofs + 3*9 + 1] |= xmask;
                    if (color_out & 0x002000) ptr[yofs + 3*10 + 1] |= xmask;
                    if (color_out & 0x001000) ptr[yofs + 3*11 + 1] |= xmask;
                    if (color_out & 0x000800) ptr[yofs + 3*12 + 1] |= xmask;
                    if (color_out & 0x000400) ptr[yofs + 3*13 + 1] |= xmask;
                    if (color_out & 0x000200) ptr[yofs + 3*14 + 1] |= xmask;
                    if (color_out & 0x000100) ptr[yofs + 3*15 + 1] |= xmask;
                    if (color_out & 0x000080) ptr[yofs + 3*16 + 1] |= xmask;
                    if (color_out & 0x000040) ptr[yofs + 3*17 + 1] |= xmask;
                    if (color_out & 0x000020) ptr[yofs + 3*18 + 1] |= xmask;
                    if (color_out & 0x000010) ptr[yofs + 3*19 + 1] |= xmask;
                    if (color_out & 0x000008) ptr[yofs + 3*20 + 1] |= xmask;
                    if (color_out & 0x000004) ptr[yofs + 3*21 + 1] |= xmask;
                    if (color_out & 0x000002) ptr[yofs + 3*22 + 1] |= xmask;
                    if (color_out & 0x000001) ptr[yofs + 3*23 + 1] |= xmask;
#endif
                    }
                }
                break;
        }
        nodebuf.prev_protocol = nodebuf.protocol;
    }
//protected: //helpers
};


struct FrameInfo
{
//    static const bool CachedWrapper = false; //true; //BROKEN; leave turned OFF
//    static const int NUM_STATS = SIZEOF(Nodebuf::TXTR::perf_stats);
    using Protocol = Nodebuf::Protocol;
public: //data members
//kludge: link some nodebuf data members into here for easier access during wrap():
    decltype(Nodebuf::protocol)& protocol;
    CONST /*double*/ decltype(Nodebuf::frame_time)& frame_time; //msec
    CONST /*SDL_Size*/ decltype(Nodebuf::wh)& wh; //int NumUniv, UnivLen;
    std::atomic</*uint32_t*/ int32_t> numfr; //= 0; //#frames rendered / next frame#
    CONST decltype(Nodebuf::dirty)& dirty;
    CONST /*elapsed_t*/ /*std::result_of<now_msec()>::type*/ decltype(now_msec()) started_msec;
//    uint32_t times[NUM_STATS + 1]; //total init/sleep (sec), render (msec), upd txtr (msec), xfr txtr (msec), present/sync (msec)
    /*elapsed_t perf_stats[SIZEOF(TXTR::perf_stats) + 1]*/ decltype(Nodebuf::perf_stats)& perf_stats; //1 extra counter for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
//    typedef bool (*Validator)(napi_env env); //const;
    typedef std::function<bool(napi_env env)> Validator;
    const Validator isvalid;
public: //ctors/dtors
    FrameInfo(/*napi_env env,*/ Nodebuf& nodebuf, Validator aovalid):
        protocol(nodebuf.protocol), frame_time(nodebuf.frame_time), wh(nodebuf.wh), dirty(nodebuf.dirty), perf_stats(nodebuf.perf_stats), //started(nodebuf.started), //link nodebuf data members for easier access
        isvalid(aovalid),
//        m_cached(nullptr, unref), //env), 
        started_msec(now_msec())
        { reset(); }
public: //operators
//    bool ismine() const { if (owner != thrid()) debug(YELLOW_MSG "not mine: owner " << owner << " vs. me " << thrid() << ENDCOLOR); return (owner == thrid()); } //std::thread::get_id(); }
//    bool isvalid() const { return (sentinel == FRINFO_VALID); }
    inline /*double*/ decltype(now_msec()) elapsed() const { return now_msec() - started_msec; } //msec; //rdiv(now_msec() - started, 1000); } //.0; }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const FrameInfo& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
//        SDL_version ver;
//        SDL_GetVersion(&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
//        ostrm << "SDL_Lib {version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d}", ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//    %p, valid? %d" ENDCOLOR, aodata.get(), aodata.get()->isvalid());
        ostrm << "FrameInfo"; //<< my_templargs();
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
//        if (!that.isvalid()) return ostrm << " INVALID}";
        ostrm << ", fr time " << that.frame_time;
        if (that.frame_time) ostrm << " (" << (1 / that.frame_time) << " fps)";
//        ostrm << ", mine? " << that.ismine(); //<< " (owner 0x" << std::hex << that.frinfo.owner << ")" << std::dec;
//        ostrm << ", exc " << that.excptr.what(); //that.excptr << " (" <<  << ")"; //= nullptr; //TODO: check for thread-safe access
//        ostrm << ", owner 0x" << std::hex << that.owner << std::dec; //<< " " << sizeof(that.owner);
//        ostrm << ", dirty " << that.dirty; //sizeof(that.dirty) << ": 0x" << std::hex << that.dirty.load() << std::dec;
        ostrm << ", #fr " << that.numfr.load();
//        ostrm << ", wh " << that.wh;
//        SDL_Size wh(SIZEOF(that./*shdata.*/nodes), SIZEOF(that./*shdata.*/nodes[0]));
//        ostrm << ", nodes@ " << &that.nodes[0][0] << "..+" << commas(sizeof(that.nodes)) << " (" << wh << ")";
//                ostrm << ", time " << that.nexttime.load();
//        ostrm << ", cached napi wrapper " << that.m_cached;
        ostrm << ", age " << /*elapsed(that.started)*/ commas(that.elapsed()) << " msec";
        return ostrm << "}";
    }
public: //methods
//called by GpuPortData dtor in fg thread:
    void reset(napi_env env)
    {
        if (m_cached) !NAPI_OK(napi_delete_reference(env, m_cached), "Del ref failed");
        m_cached = nullptr;
    }
//called by bkg thread:
    void reset() //int start_frnum = 0) //, SrcLine srcline = 0)
    {
        protocol = Protocol::WS281X;
//            nexttime.store(0);
        frame_time = 0; //msec
//        frinfo.wh = SDL_Size(0, 0); //int NumUniv, UnivLen;
        started_msec = now_msec(); //reset epoch for actual run time
//no; interferes with sync        dirty.store(0, NVL(srcline, SRCLINE));
        numfr.store(0); //start_frnum; //.store(frnum);
//        frinfo.dirty = 0; //.store(0);
//        for (int i = 0; i < SIZEOF(times); ++i) times[i] = 0;
//        memset(&times[0], 0, sizeof(times));
//            sentinel = FRINFO_MAGIC;
//        m_cached.cre_undef();
//        if (m_cached) !NAPI_OK(napi_delete_reference())
//        m_cached.reset();
    }
public: //playback methods
    bool m_listening = false;
    inline bool islistening() const { return m_listening; } //(listener.type() != napi_null); }
    inline bool islistening(bool yesno) { return m_listening = yesno; } //islistening(yesno, listener.env); }
//    bool islistening(napi_env env, bool yesno)
//    {
//        listener.env = env;
//        if (yesno) !NAPI_OK(napi_get_undefined(env, &listener.value), "Get undef failed");
//        else !NAPI_OK(napi_get_null(env, &listener.value), "Get null failed");
//        listener = yesno;
//        return yesno; //islistening();
//    }
    std::exception_ptr excptr = nullptr; //init redundant (default init)
    std::string exc_reason(const char* no_reason = 0) const
    {
        static std::string reason;
        reason.assign(NVL(no_reason, "(no reason)"));
        if (!excptr) return reason;
//get desc or other info for caller:
        try { std::rethrow_exception(excptr); }
//            catch (...) { excstr = std::current_exception().what(); }
        catch (const std::exception& exc) { reason = exc.what(); }
        catch (...) { reason = "??EXC??"; }
        return reason;
    }
//    void next() //SrcLine srcline = 0)
//    {
//        ++numfr;
//NOTE: do this here so subscribers can work while RenderPresent waits for VSYNC
//        dirty.store(0); //, NVL(srcline, SRCLINE)); //clear dirty bits; NOTE: this will wake client rendering threads
//        gp.wake(); //wake other threads/processes that are waiting to update more nodes as soon as copy to txtr is done to maxmimum parallelism
//    }
//    static void refill(GpuPort_shdata* ptr) { ptr->refill(); }
//    void owner_init()
//    {
//        owner = std::thread::get_id();
//    }
//    SrcLine my_napi_property_descriptor::srcline; //kludge: create a place for _.srcline
#if 0
    static napi_value get_dirty(FrameInfo* THIS, napi_env env, napi_callback_info info) //[](napi_env env, void* data) //GetDirty_NAPI; //live update
    {
        if (!env) return NULL; //Node cleanup mode?
        /*GpuPortData*/ void* aodata;
        napi_value argv[1], This;
        size_t argc = SIZEOF(argv);
        struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
        !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.dirty.load();
//                THIS->isvalid(env);
//        if (aodata != static_cast<void*>(THIS)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
        return napi_thingy(env, THIS->dirty.load());
//                retval = aodata->/*m_frinfo.*/dirty.load();
//                return retval.value;
    }
#endif
//    napi_thingy m_cached; //wrapped frinfo napi object for caller
//    napi_ref m_cached;
//    std::unique_ptr<napi_ref, std::function<void(napi_ref)>> m_cached;
    napi_ref m_cached = nullptr;
    napi_value wrap(napi_env env) //, napi_value* valp = 0)
    {
//can't cache :(        if (nodes.arytype() != napi_uint32_array) //napi_typedarray_type)
        if (!env) return NULL; //Node cleanup mode?
//can't cache :(        if (frinfo.type() != napi_object)
//debug("here70" ENDCOLOR);
// status = napi_create_reference(env, cons, 1, &constructor);
//  if (status != napi_ok) return status;
//        if (env != m_cached.env) debug(YELLOW_MSG "env changed: from 0x%x to 0x%x" ENDCOLOR, m_cached.env, env);
#if 0
        debug(BLUE_MSG "wrap nodebuf, ret? %d, cached " << m_cached << ENDCOLOR, !!valp);
#endif
        napi_thingy retval(env);
//        if (m_cached.ref) retval = m_cached.value();
        if (m_cached) !NAPI_OK(napi_get_reference_value(env, m_cached, &retval.value), "Get ret val failed");
//        retval = m_cached.value();
//        m_cached.env = env;
        static int count = 0;
        debug(19, BLUE_MSG "wrap frinfo[%d]: cached " << retval << ", ret? %d" ENDCOLOR, count++, retval.type() == napi_object);
//        if (/*CachedWrapper && valp &&*/ (m_cached.env == env) && (m_cached.type() == napi_object)) { *valp = m_cached.value; return; }
//        if (m_cached.type() == napi_object) return m_cached;
//        napi_thingy frinfo(env, napi_thingy::Object{});
//debug("here71" ENDCOLOR);
        if (retval.type() == napi_object) return retval;
        retval.cre_object();
//        m_cached.cre_object(env);
//        m_cached.cre_object(); //= napi_thingy(env, napi_thingy::Object{});
//        napi_thingy wrapper(env, napi_thingy::Object{});
//        wrapper.cre_object();
//        !NAPI_OK(napi_create_object(env, &frinfo.value), "Cre frinfo obj failed");
//        {
//Protocol protocol; //= WS281X;
//const double frame_time; //msec
//const SDL_Size wh; //int NumUniv, UnivLen;
//const /*elapsed_t*/ /*std::result_of<now_msec()>::type*/ decltype(now_msec()) started;
//std::atomic</*uint32_t*/ int> numfr; //= 0; //#frames rendered / next frame#
//BkgSync<MASK_TYPE, true> dirty; //one Ready bit for each universe
//uint64_t times[NUM_STATS]; //total init/sleep (sec), render (msec), upd txtr (msec), xfr txtr (msec), present/sync (msec)
//debug("here72" ENDCOLOR);
//        my_napi_property_descriptor props[10], *pptr = props; //TODO: use std::vector<>
        my_vector<my_napi_property_descriptor> props; //(10);
//        memset(&props[0], 0, sizeof(props)); //clear first so NULL members don't need to be explicitly set
//kludge: use lambas in lieu of C++ named member init:
//(named args easier to maintain than long param lists)
        [/*env,*/ this](auto& _)
        {
            _.utf8name = "protocol"; //_.name = NULL;
//                _.method = NULL;
//                _.getter = GetProtocol_NAPI; _.setter = SetProtocol_NAPI; //read/write; allow live update
            _.getter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //void* data) //GetProtocol_NAPI; //live update
            {
//TODO: refactor with other getters, setter
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
//                !NAPI_OK(napi_create_int32(env, static_cast<int32_t>(aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol), &argv[0]), "Get uint32 getval failed");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
//                napi_thingy retval(env, protocol);
//                retval = /*aodata->m_frinfo.*/protocol;
                return napi_thingy(env, static_cast<int>(frdata->protocol)); //retval.value;
            };
            _.setter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //SetProtocol_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1+1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Setter info extract failed");
                if (argc != 1) NAPI_exc("got " << argc << " args, expected 1");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
                int /*int32_t*/ prtemp;
                !NAPI_OK(napi_get_value_int32(env, argv[0], &prtemp), "Get uint32 setval failed");
//                aodata->/*wker_ok(env, SRCLINE)->*/m_frinfo.
                frdata->protocol = static_cast<Nodebuf::Protocol>(prtemp);
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    aodata->wker_ok(env)->m_frinfo.protocol = 0; //TODO
            };
//                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.protocol, &_.value), "Cre protocol int failed");
            _.attributes = napi_enumerable; //napi_default; //napi_writable | napi_enumerable; //read-write; //napi_default;
            _.data = this; //needed by getter/setter
        }(props.emplace_back()); //(*pptr++);
//            NAMED{ _.itf8name = "protocol"; _.getter = GetProtocol_NAPI; _.setter = SetProtocol_NAPI; _.attributes = napi_default; }(*props++);
//debug("here73" ENDCOLOR);
        [env, this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "frame_time";
//                _.getter = GetFrameTime_NAPI; //read-only
//            !NAPI_OK(napi_create_double(env, /*gpu_wker->m_frinfo.*/frame_time, &_.value), "Cre frame_time float failed");
            _.value = napi_thingy(env, frame_time);
            _.attributes = napi_enumerable; //read-only; //napi_default;
        }(props.emplace_back()); //(*pptr++);
        [env, this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "NUM_UNIV";
//                _.getter = GetNumUniv_NAPI; //read-only
//            !NAPI_OK(napi_create_int32(env, /*gpu_wker->m_frinfo.*/wh.w, &_.value), "Cre w int failed");
            _.value = napi_thingy(env, wh.w);
            _.attributes = napi_enumerable; //read-only; //napi_default;
        }(props.emplace_back()); //(*pptr++);
        [env, this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "UNIV_LEN";
//            !NAPI_OK(napi_create_int32(env, /*gpu_wker->m_frinfo.*/wh.h, &_.value), "Cre h int failed");
            _.value = napi_thingy(env, wh.h);
            _.attributes = napi_enumerable; //read-only; //napi_default;
        }(props.emplace_back()); //(*pptr++);
//debug("here75" ENDCOLOR);
        [/*env,*/ this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "numfr";
//                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.numfr.load(), &_.value), "Cre #fr int failed");
            _.getter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetNumfr_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
//                !NAPI_OK(napi_create_int32(env, static_cast<int32_t>(aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol), &argv[0]), "Get uint32 getval failed");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
                return napi_thingy(env, frdata->numfr.load());
//                retval = aodata->m_frinfo.numfr.load();
//                return retval; //retval.value;
            };
            _.attributes = napi_enumerable; //read-only; //napi_default;
            _.data = this; //needed by getter
        }(props.emplace_back()); //(*pptr++);
//debug("here76" ENDCOLOR);
        [/*env,*/ this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "dirty";
//                !NAPI_OK(napi_create_uint32(env, gpu_wker->m_frinfo.dirty.load(), &_.value), "Cre dirty uint failed");
#if 1
//            _.getter = std::bind([](FrameInfo* THIS, napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetDirty_NAPI; //live update
            _.getter = [](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetDirty_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.dirty.load();
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(THIS)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
                return napi_thingy(env, frdata->dirty.load());
//                retval = aodata->/*m_frinfo.*/dirty.load();
//                return retval.value;
            }; //, this, std::placeholders::_1, std::placeholders::_2);
#else
            _.getter = std::bind(get_dirty, this, std::placeholders::_1, std::placeholders::_2);
#endif
            _.attributes = napi_enumerable; //read-only; //napi_default;
            _.data = this; //needed by getter
        }(props.emplace_back()); //(*pptr++);
//debug("here74" ENDCOLOR);
        [/*env,*/ this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "elapsed"; //msec; //"started";
//            !NAPI_OK(napi_create_int32(env, /*gpu_wker->m_frinfo.*/started, &_.value), "Cre started int failed");
//            _.value = napi_thingy(env, started);
//            _.method
            _.getter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetNumfr_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
//                !NAPI_OK(napi_create_int32(env, static_cast<int32_t>(aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol), &argv[0]), "Get uint32 getval failed");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
//                return napi_thingy(env); //, frdata->elapsed_msec()); //ambiguous
                napi_thingy retval(env);
                retval.cre_int32(frdata->elapsed());
                return retval;
            };
            _.attributes = napi_enumerable; //read-only; //napi_default;
            _.data = this; //needed by getter
        }(props.emplace_back()); //(*pptr++);
//debug("here74" ENDCOLOR);
        [/*env,*/ this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "isrunning"; //"started";
//            !NAPI_OK(napi_create_int32(env, /*gpu_wker->m_frinfo.*/started, &_.value), "Cre started int failed");
//            _.value = napi_thingy(env, started);
//            _.method
            _.getter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetNumfr_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
//                !NAPI_OK(napi_create_int32(env, static_cast<int32_t>(aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol), &argv[0]), "Get uint32 getval failed");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
                return napi_thingy(env, frdata->islistening());
            };
            _.attributes = napi_enumerable; //read-only; //napi_default;
            _.data = this; //needed by getter
        }(props.emplace_back()); //(*pptr++);
        [/*env,*/ this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "exc"; //"started";
//            !NAPI_OK(napi_create_int32(env, /*gpu_wker->m_frinfo.*/started, &_.value), "Cre started int failed");
//            _.value = napi_thingy(env, started);
//            _.method
            _.getter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetNumfr_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
//                !NAPI_OK(napi_create_int32(env, static_cast<int32_t>(aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol), &argv[0]), "Get uint32 getval failed");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
                return napi_thingy(env, frdata->exc_reason(""));
            };
            _.attributes = napi_enumerable; //read-only; //napi_default;
            _.data = this; //needed by getter
        }(props.emplace_back()); //(*pptr++);
#if 0
        [env, this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "debug"; //"started";
//            !NAPI_OK(napi_create_int32(env, /*gpu_wker->m_frinfo.*/started, &_.value), "Cre started int failed");
//            _.value = napi_thingy(env, started);
//            _.method
            _.getter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetNumfr_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
//                !NAPI_OK(napi_create_int32(env, static_cast<int32_t>(aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol), &argv[0]), "Get uint32 getval failed");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
                return napi_thingy(env, false); //frdata->islistening());
            };
            _.setter = [/*this*/](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //SetProtocol_NAPI; //live update
            {
                if (!env) return NULL; //Node cleanup mode?
                /*GpuPortData*/ FrameInfo* frdata;
                napi_value argv[1+1], This;
                size_t argc = SIZEOF(argv);
                struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&frdata), "Setter info extract failed");
                if (argc != 1) NAPI_exc("got " << argc << " args, expected 1");
                frdata->isvalid(env);
//                if (aodata != static_cast<void*>(this)) NAPI_exc("aodata mismatch: " << aodata << " vs. " << this);
                int /*int32_t*/ debtemp;
                !NAPI_OK(napi_get_value_int32(env, argv[0], &debtemp), "Get uint32 setval failed");
//                aodata->/*wker_ok(env, SRCLINE)->*/m_frinfo.
//TODO:                frdata->debug = dedtemp;
                debug(RED_MSG "TODO: set debug = %d" ENDCOLOR, debtemp);
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    aodata->wker_ok(env)->m_frinfo.protocol = 0; //TODO
            };
            _.attributes = napi_enumerable; //read-only; //napi_default;
            _.data = this; //needed by getter
        }(props.emplace_back()); //(*pptr++);
#endif
        [env, this](auto& _) //napi_property_descriptor& _)
        {
            _.utf8name = "perf"; //NOTE: times[] will update automatically due to underlying array buf
            napi_thingy arybuf(env, &perf_stats[0], sizeof(perf_stats));
//        !NAPI_OK(napi_create_external_arraybuffer(env, &/*gpu_wker->*/m_shdata.nodes[0][0], sizeof(/*gpu_wker->*/m_shdata.nodes), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
//            debug("arybuf5 " << arybuf << ENDCOLOR);
//            debug(YELLOW_MSG "cre typed array[%u] perf stats wrapper " << arybuf << ENDCOLOR, SIZEOF(perf_stats));
            _.value = napi_thingy(env, Nodebuf::perf_stats_type, SIZEOF(perf_stats), arybuf);
//            typary.typed_ary(napi_biguint64_array, SIZEOF(perf_stats), arybuf);
//            _.value = typary.value;
//            !NAPI_OK(napi_create_typedarray(env, napi_biguint64_array, SIZEOF(gpu_wker->m_frinfo.times), arybuf, 0, &_.value), "Cre times typed array failed");
            _.attributes = napi_enumerable; //read-only; //napi_default;
        }(props.emplace_back()); //(*pptr++);
//add above props to frame info object:
//debug("here77" ENDCOLOR);
//        if (pptr - props > SIZEOF(props)) NAPI_exc("prop ary overflow: needed " << (pptr - props) << ", only had space for " << SIZEOF(props));
//        napi_thingy retval(env, napi_thingy::Object{});
        !NAPI_OK(napi_define_properties(env, retval, props.size(), props.data()), "set frinfo props failed");
//        m_cached = retval;
//debug("here78" ENDCOLOR);
//        if (frinfo.env != env) NAPI_exc("frinfo env mismatch");
//        if (valp) *valp = m_cached.value;
//        m_cached = retval;
        const int REF_COUNT = 1;
        !NAPI_OK(napi_create_reference(env, retval, REF_COUNT, &m_cached), "Cre ref failed"); //allow to be reused next time
        return retval;
//debug("here79" ENDCOLOR);
    }
};


//circular queue entry of nodebufs:
//goes in shm for multi-proc access
struct NodebufQuent
{
    static const int NUM_UNIV = Nodebuf::NUM_UNIV;
    static const int UNIV_MAXLEN = Nodebuf::UNIV_MAXLEN_pad;
    static const auto GPU_NODE_type = Nodebuf::GPU_NODE_type;
    using MASK_TYPE = Nodebuf::MASK_TYPE;
    using NODEVAL = Nodebuf::NODEVAL;
//    std::mutex mutex;
//CAUTION: reuse same key each time so new procs can take ownership of old data:
    static const key_t KEY = 0xfeed0000 | NNNN_hex(UNIV_MAXLEN); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
    static const int QUELEN = 4;
//    struct NodebufFrame
//    {
    std::atomic<int32_t> frnum, prevfr; //bkg gpu wker bumps fr# (lockless)
    std::atomic<elapsed_t> frtime, prevtime; //frame timestamp
    std::atomic<MASK_TYPE> ready; //fx gen threads/procs set ready bits; bkg gpu wker resets after xfr to GPU
//TODO: cache pad here
    NODEVAL nodes/*[QUELEN]*/[NUM_UNIV][UNIV_MAXLEN];
//    };
//    NodebufFrame bufs[QUELEN]; //4 x 24 x 1136 x 4 ~= 440KB
//    static STATIC_WRAP(std::vector<NodebufQuent*>, m_all);
//    static WRAP(int, m_count, = 0);
public: //ctors/dtors
    explicit NodebufQuent() //NOTE: only called when first process attaches
    {
        static int count = 0;
//        debug(5, CYAN_MSG << "NodebufQuent[%d]: init (shm) %p" << ENDCOLOR, m_all.size(), this);
//        for (int i = 0; i < SIZEOF(bufs); ++i)
        frnum = count++; //m_all.size(); //m_count++; //first round of fr#s are sequential from 0
        prevfr = -1;
        frtime = prevtime = 0;
        ready = Nodebuf::NOT_READY;
        debug(5, BLUE_MSG "clearing %s = %s nodes" ENDCOLOR, commas(SIZEOF_2D(nodes)), commas(sizeof(nodes) / sizeof(nodes[0][0]))); //, frnum); //, i, SIZEOF(bufs));
        for (int n = 0; n < SIZEOF_2D(nodes); ++n) //CAUTION: 1D addressing for simpler loop control
            /*bufs[i].*/nodes[0][n] = BLACK; //start in known state
//        m_all.push_back(this);
        INSPECT(GREEN_MSG << "ctor " << *this); //, srcline);
    }
    ~NodebufQuent() { INSPECT(RED_MSG << "dtor " << *this); } //, srcline); }
public: //operators
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const NodebufQuent& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
        ostrm << "NodebufQuent"; //<< my_templargs();
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
//        if (!that.isvalid()) return ostrm << " INVALID}";
        ostrm << ", " << commas(SIZEOF_2D(that.nodes)) << " nodes";
        ostrm << ", fr# " << that.frnum << ", prev " << that.prevfr;
        ostrm << ", fr time " << that.frtime << ", prev " << that.prevtime;
        ostrm << ", ready 0x" << std::hex << that.ready.load() << std::dec;
        ostrm << "}";
        return ostrm;
    }
public: //methods
    int GetNext(int previous = 0) { return ++previous % QUELEN; } //m_all.size(); } //SIZEOF(bufs); } //TODO
    int nextrd() {} //TODO
    int nextwr() {} //TODO
//    static inline napi_ref& ref() //kludge: use wrapper to avoid trailing static decl at global scope
//    {
//        static napi_ref m_ref = nullptr; //NOTE: this also 
//        return m_ref;
//    }
//    static WRAP(napi_ref, m_ref, = nullptr); //napi_ref m_ref = nullptr;
    static STATIC_WRAP(napi_ref, m_ref, = nullptr);
    STATIC void reset(napi_env env)
    {
//        napi_ref& m_ref = ref();
        if (m_ref) !NAPI_OK(napi_delete_reference(env, m_ref), "Del ref failed");
        m_ref = nullptr;
    }
//TODO: split into 2 levels (private + shared)?
    /*static*/ STATIC napi_value expose(napi_env env, NodebufQuent* nodebufs)
    {
debug(9, BLUE_MSG "here20, env %p" ENDCOLOR, env);
        if (!env) return NULL; //Node cleanup mode?
        napi_thingy retval(env);
#if 0 //BROKEN in slave
//debug(9, BLUE_MSG "here21" << m_ref << ENDCOLOR);
        if (m_ref) !NAPI_OK(napi_get_reference_value(env, m_ref, &retval.value), "Get ret val failed");
debug(9, BLUE_MSG "here22" ENDCOLOR);
        static int count = 0;
        debug(19, BLUE_MSG "wrap nodeque[%d]: cached " << retval << ", ret? %d" ENDCOLOR, count++, retval.type() == napi_object);
//can't cache :(        if (nodes.arytype() != napi_uint32_array) //napi_typedarray_type)
//        if (/*CachedWrapper && valp &&*/ (m_cached.env == env) && (m_cached.type() == napi_object)) { *valp = m_cached.value; return; }
//        if (m_cached.type() == napi_object) return m_cached;
        if (retval.type() == napi_object) return retval;
//debug(9, BLUE_MSG "here23" ENDCOLOR);
//        retval.cre_object();
//CAUTION:
//no        if (CachedWrapper && valp && (m_cached.env == env) && (m_cached.arytype() == GPU_NODE_type /*m_cached.type() != napi_undefined*/)) { *valp = m_cached.value; return; }
//can't check yet:        if (!wh.w || !wh.h) NAPI_exc("no nodes");
        !NAPI_OK(napi_create_array_with_length(env, QUELEN, &retval.value), "Cre que ary failed");
//        for (int q = 0; q < /*QUELEN*/ m_all.size(); ++q)
//NO: only set on first process        for (auto it = m_all.begin(); it != m_all.end(); ++it)
        for (NodebufQuent* it = nodebufs; it != &nodebufs[QUELEN]; ++it)
        {
debug(9, BLUE_MSG "here24, buf[%d/%d]" ENDCOLOR, it - nodebufs, QUELEN); //m_all.begin(), m_all.size());
            napi_thingy arybuf(env, &it->nodes[0][0], sizeof(it->nodes)); //ext buf for all nodes in all univ
//debug(9, BLUE_MSG "here25" ENDCOLOR);
//        arybuf.cre_ext_arybuf(&nodes[0][0], sizeof(nodes));
//        !NAPI_OK(napi_create_external_arraybuffer(env, &/*gpu_wker->*/m_shdata.nodes[0][0], sizeof(/*gpu_wker->*/m_shdata.nodes), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
//        debug(YELLOW_MSG "arybuf5 " << arybuf << ", dims " << wh << ENDCOLOR);
//        m_cached.cre_typed_ary(env, GPU_NODE_type, wh.w * wh.h, arybuf);
#if 0 //1D array; caller must use UNIV_MAXLEN_pad in index arithmeric :(
        napi_thingy typary(env, GPU_NODE_type, wh.w * wh.h, arybuf);
#else //2D array; helps separate univ from eacher
            napi_thingy univ_ary(env);
            !NAPI_OK(napi_create_array_with_length(env, NUM_UNIV, &univ_ary.value), "Cre univ ary failed");
//    NODEVAL nodes[NUM_UNIV][UNIV_MAXLEN_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
            for (int x = 0; x < /*wh.w*/ NUM_UNIV; ++x)
            {
//debug(9, BLUE_MSG "here26, univ[%d/%d]" ENDCOLOR, x, NUM_UNIV);
//TODO: add handle_scope? https://nodejs.org/api/n-api.html#n_api_making_handle_lifespan_shorter_than_that_of_the_native_method
                napi_thingy node_typary(env, GPU_NODE_type, /*wh.h*/ UNIV_MAXLEN, arybuf, x * sizeof(it->nodes[0])); //UNIV_MAXLEN * sizeof(NODEVAL)); //sizeof(nodes[0][0]));
                !NAPI_OK(napi_set_element(env, univ_ary, x, node_typary), "Cre inner node typary failed");
            }
debug(9, BLUE_MSG "here27" ENDCOLOR);
            !NAPI_OK(napi_set_element(env, retval, it - nodebufs, univ_ary), "Cre inner node ary failed");
#endif
        }
        debug(19, YELLOW_MSG "nodebq " << retval << ENDCOLOR);
        const int REF_COUNT = 1;
//debug(9, BLUE_MSG "here28" ENDCOLOR);
        !NAPI_OK(napi_create_reference(env, retval, REF_COUNT, &m_ref), "Cre ref failed"); //allow to be reused next time
#endif
        return retval;
    }
};


//addon state/context data:
//using structs to allow inline member init
struct GpuPortData
{
private: //data members
//    using SHDATA = GPUPORT::WKER::shdata;
    static const int VALID = 0x1234beef;
    const int valid1 = VALID; //put one at start and one at end
//    std::unique_ptr<SHDATA, std::function<int(SHDATA*)>> m_shmptr; //define as member data to avoid WET defs needed for class derivation; NOTE: must come before depend refs below; //NODEBUF_FrameInfo, NODEBUF_deleter>; //DRY kludge
//    m_shmptr = decltype(m_shmptr)(shmalloc_typed<SHDATA>(0), std::bind(shmfree, std::placeholders::_1, SRCLINE)); //NVL(srcline, SRCLINE))), //shim; put nodes in shm so multiple procs/threads can render
//    SHDATA& m_shdata; //= *m_shmptr.get();
//    const napi_env m_env;
public: //public data members
//    SHDATA m_shdata;
//    std::atomic<int> numfr; //= 0;
//    uint32_t dirty = 0; //must be locked for cond var notify any; don't need atomic<> here
//    typedef typename std::conditional<(NUM_UNIV <= 32), uint32_t, std::bitset<NUM_UNIV>>::type MASK_TYPE;
    FrameInfo m_frinfo;
    Nodebuf m_nodebuf; //TODO: replace with nodebuf que
    AutoShmary<NodebufQuent, NodebufQuent::QUELEN> m_nodebq; //circular queue of nodebufs in shm
//    decltype(m_nodebuf.m_txtr)& m_txtr = m_nodebuf.m_txtr;
    decltype(m_frinfo.numfr)& numfr = m_frinfo.numfr; //delegated
    decltype(m_nodebuf.dirty)& dirty = m_nodebuf.dirty; //delegated
//    elapsed_t perf_stats[SIZEOF(m_txtr.perf_stats) + 1]; //1 extra counter for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
//        Uint32 nodes[3][5];
//    std::unique_ptr<NODEVAL> m_nodes; //define as member data to avoid WET defs needed for class derivation; NOTE: must come before depend refs below; //NODEBUF_FrameInfo, NODEBUF_deleter>; //DRY kludge
//    } m_shdata;
//    int svfrnum;s
//    napi_threadsafe_function fats; //asynchronous thread-safe JavaScript call-back function; can be called from any thread
private: //data members
    const decltype(now()) /*elapsed_t*/ m_started;
    const SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
public: //ctors/dtors
//    GpuPortData() = delete; //must have env so delete default
//    explicit GpuPortData(napi_env env, SrcLine srcline = 0): /*cbthis(env), nodes(env), frinfo(env),*/ m_started(now()), m_srcline(srcline)
    explicit GpuPortData(/*napi_env env,*/ SrcLine srcline = 0):
        m_frinfo(/*env,*/ m_nodebuf, std::bind([](const GpuPortData* aodata, napi_env env) -> bool { return aodata->isvalid(env, SRCLINE); }, this, std::placeholders::_1)), //m_nodebuf(env),
        m_nodebq(NodebufQuent::KEY, /*NodebufQuent::QUELEN,*/ SRCLINE),
        /*cbthis(env), nodes(env), frinfo(env),*/ m_started(now()), m_srcline(srcline)
//        nodes(env), frinfo(env), listener(env), //NOTE: need these because default ctor deleted
//        m_shmptr(shmalloc_typed<SHDATA>(SHM_LOCAL), std::bind(shmfree, std::placeholders::_1, SRCLINE)), //NVL(srcline, SRCLINE))), //shim; put nodes in shm so multiple procs/threads can render
//        m_shdata(*m_shmptr.get())
//        nodes_setup(env, &dummy); //kludge: pre-alloc to avoid memory problems
//        frinfo_setup(env, &dummy); //kludge: pre-alloc to avoid memory problems
    {
//        m_frinfo.reset(); //already done
//        numfr.store(0);
//        dirty.store(0);
//        islistening(false); //listener.busy = false;
        INSPECT(GREEN_MSG << "ctor " << *this, srcline);
    }
    /*virtual*/ ~GpuPortData() { INSPECT(RED_MSG << "dtor " << *this << ", lifespan " << elapsed(m_started) << " sec", m_srcline); } //reset(); }
public: //operators
    inline bool isvalid() const { return (this && (valid1 == VALID) && (valid2 == VALID)); } //paranoid/debug
    bool isvalid(napi_env env, SrcLine srcline = 0) const { return isvalid() || NAPI_exc(env, NO_ERRCODE, "aoptr invalid", NVL(srcline, SRCLINE)); }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const GpuPortData& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        SrcLine srcline = NVL(that.m_srcline, SRCLINE);
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
//        SDL_version ver;
//        SDL_GetVersion(&ver); //TODO: maybe derive SDL_AutoLib from SDL_version?
//        ostrm << "SDL_Lib {version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, likely isRPi? %d}", ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());
//    %p, valid? %d" ENDCOLOR, aodata.get(), aodata.get()->isvalid());
        ostrm << "GpuPortData"; //<< my_templargs();
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
        if (!that.isvalid()) return ostrm << " INVALID}";
//        ostrm << ", env#" << envinx(that.cbthis);
//        ostrm << ", listener: " << that.listener << ", listening? " << that.islistening();
        ostrm << ", listening? " << that.islistening(); //<< ", cb this: " << that.cbthis;
        ostrm << ", frinfo: " << that.m_frinfo;
        ostrm << ", nodes: " << that.m_nodebuf.wh; //<< ", cached napi wrapper " << that.m_nodebuf.m_cached;
//        if (that.wker_ok()) ostrm << ", gpdata {#fr " << that.gpu_wker->m_frinfo.numfr << "}";
        ostrm << "}";
        return ostrm;
    }
#if 0
private:
    bool m_listening = false;
public: //playback methods
    inline bool islistening() const { return m_listening; } //(listener.type() != napi_null); }
    inline bool islistening(bool yesno) { return m_listening = yesno; } //islistening(yesno, listener.env); }
//    bool islistening(napi_env env, bool yesno)
//    {
//        listener.env = env;
//        if (yesno) !NAPI_OK(napi_get_undefined(env, &listener.value), "Get undef failed");
//        else !NAPI_OK(napi_get_null(env, &listener.value), "Get null failed");
//        listener = yesno;
//        return yesno; //islistening();
//    }
    std::exception_ptr excptr = nullptr; //init redundant (default init)
    std::string exc_reason(const char* no_reason = 0) const
    {
        static std::string reason;
        reason.assign(NVL(no_reason, "(no reason)"));
        if (!excptr) return reason;
//get desc or other info for caller:
        try { std::rethrow_exception(excptr); }
//            catch (...) { excstr = std::current_exception().what(); }
        catch (const std::exception& exc) { reason = exc.what(); }
        catch (...) { reason = "??EXC??"; }
        return reason;
    }
#endif
    inline bool islistening() const { return m_frinfo.islistening(); }
    inline bool islistening(bool yesno) { return m_frinfo.islistening(yesno); }
    std::exception_ptr& excptr = m_frinfo.excptr;
    inline std::string exc_reason(const char* no_reason = 0) const { return m_frinfo.exc_reason(no_reason); }
//BROKEN    napi_thingy cbthis, nodes, frinfo; //info for fats
//CAUTION: napi values appear to need to be re-created each time; can't store in heap and span napi calls :(
//    void reset()
    void reset(napi_env env)
    {
        m_nodebuf.reset(env);
        m_frinfo.reset(env);
        m_nodebq[0].reset(env);
    }
//set playback options:
private:
    struct
    {
//caller-selectable values:
        int screen; //= FIRST_SCREEN;
//    key_t PREALLOC_shmkey = 0;
        int vgroup; //= 1;
        int debug;
        Uint32 init_color; //= 0;
        /*Nodebuf::Protocol*/ int protocol;
        int frtime_msec; //double fps;
//internal state:
//        static const Nodebuf::TXTR* PBEOF = (Nodebuf::TXTR*)-5;
        napi_threadsafe_function fats; //asynchronous thread-safe JavaScript call-back function; can be called from any thread
        Nodebuf::TXTR::REFILL refill; //void (*m_refill)(Nodebuf::TXTR*); //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
//        napi_threadsafe_function fats; //asynchronous thread-safe JavaScript call-back function; can be called from any thread
    } m_opts;
public:
//    aoptr->make_refill(env, argv[argc - 1], Listen_cb);
//    void set_opts()
//    {
//        m_opts.screen = FIRST_SCREEN;
//        m_opts.vgroup = 1;
//        m_opts.init_color = BLACK;
//    }
//    static constexpr const Nodebuf::TXTR* const NO_ADVANCE = (const Nodebuf::TXTR*)-1;
//    typedef Nodebuf::TXTR* txptr;
//    static constexpr txptr NO_ADVANCE = reinterpret_cast<txptr>(-1);
    static constexpr Nodebuf::TXTR* NO_ADVANCE = NULL; //(Nodebuf::TXTR*)-1;
    void set_opts(napi_env env, napi_value& optsval, napi_value& jsfunc, napi_threadsafe_function_call_js napi_cb) //napi_callback napi_cb)
    {
//prep caller's port params:
//    if (argc > 1) //unpack option values from first arg
//        bool has_prop;
//        napi_value propval;
//        napi_valuetype valtype;
//        !NAPI_OK(napi_typeof(env, argv[0], &valtype), "Get arg type failed");
//        set_opts(); //set defaults first
        m_opts.debug = 0;
        m_opts.screen = FIRST_SCREEN;
        m_opts.vgroup = 1;
        m_opts.init_color = BLACK;
        m_opts.protocol = static_cast<int>(Nodebuf::Protocol::WS281X);
        m_opts.frtime_msec = 0; //fps = 0; //Nodebuf::FPS;
        napi_thingy opts(env, optsval);
        if (opts.type() != napi_undefined)
        {
            uint32_t listlen;
            napi_value proplist;
            if (opts.type() != napi_object) NAPI_exc("First arg not object"); //TODO: allow other types?
            !NAPI_OK(napi_get_property_names(env, optsval, &proplist), "Get prop names failed");
            !NAPI_OK(napi_get_array_length(env, proplist, &listlen), "Get array len failed");
//#if 1
//            static const std::map<const char*, int*> known_opts =
//            static const struct { const char* name; int* valp; } known_opts[] =
//            using KEYTYPE = const char*;
//            using MAPTYPE = std::vector<std::pair<KEYTYPE, int*>>;
            static const str_map<const char*, int*> known_opts = //MAPTYPE known_opts =
            {
                {"debug", &m_opts.debug},
                {"screen", &m_opts.screen},
                {"vgroup", &m_opts.vgroup},
                {"color", (int*)&m_opts.init_color},
                {"protocol", &m_opts.protocol},
                {"frame_msec", &m_opts.frtime_msec},
            };
//            std::function<int(KEYTYPE key)> find = [known_opts](KEYTYPE key) -> std::pair<KEYTYPE, int*>*
//            {
//                for (auto pair : known_opts)
//                    if (!strcmp(key, pair.first)) return &pair;
//                return 0;
//            };
//            debug(BLUE_MSG "checking %d option names in list of %d known options" ENDCOLOR, listlen, known_opts.size());
//            for (auto opt : known_opts) debug(BLUE_MSG "key '%s', val@ %p vs. %p" ENDCOLOR, opt.first, opt.second, this);
            for (int i = 0; i < listlen; ++i)
            {
//TODO: add handle_scope? https://nodejs.org/api/n-api.html#n_api_making_handle_lifespan_shorter_than_that_of_the_native_method
                char buf[20]; //= ",";
                size_t buflen;
                napi_thingy propname(env), propval(env);
                !NAPI_OK(napi_get_element(env, proplist, i, &propname.value), "Get array element failed");
                !NAPI_OK(napi_get_value_string_utf8(env, propname, buf, sizeof(buf), &buflen), "Get string failed");
//                strcpy(&buf[1 + buflen], ",");
//                if (strstr(",screen,vgroup,color,protocol,debug,nodes,", buf)) continue;
//                buf[1 + buflen] = '\0';
//                if (!get_prop(env, optsval, buf, &propval.value)) propval.cre_undef();
                !NAPI_OK(napi_get_named_property(env, optsval, buf, &propval.value), "Get named prop failed");
                debug(17, BLUE_MSG "find prop[%d/%d] %d:'%s' " << propname << ", value " << propval << ", found? %d" ENDCOLOR, i, listlen, buflen, buf, !!known_opts.find(buf));
                if (!strcmp(buf, "fps")) { double fps; !NAPI_OK(napi_get_value_double(env, propval, &fps), "Get prop failed"); m_opts.frtime_msec = 1000.0 / fps; } //alias for frame_time_msec; kludge: float not handled by known_opts table
                else if (!known_opts.find(buf)) exc_soft("unrecognized option: '%s' " << propval, buf); //strlen(buf) - 2, &buf[1]);
                else !NAPI_OK(napi_get_value_int32(env, propval, known_opts.find(buf)->second), "Get prop failed");
            }
//#endif
//            opts.get_prop("debug", &m_opts.debug);
//            /*!NAPI_OK(*/opts.get_prop("screen", &m_opts.screen); //, opts.env, "Invalid .screen prop");
//        !NAPI_OK(get_prop(env, argv[0], "shmkey", &PREALLOC_shmkey), "Invalid .shmkey prop");
//            /*!NAPI_OK(*/opts.get_prop("vgroup", &m_opts.vgroup); //, opts.env, "Invalid .vgroup prop");
//            /*!NAPI_OK(*/opts.get_prop("color", &m_opts.init_color); //, opts.env, "Invalid .color prop");
//            int /*int32_t*/ prtemp;
//            opts.get_prop("protocol", &prtemp); //!NAPI_OK(napi_get_value_int32(env, argv[0], &prtemp), "Get uint32 setval failed");
//            m_opts.protocol = static_cast<Nodebuf::Protocol>(prtemp);
            debug(17, BLUE_MSG "listen opts: screen %d, vgroup %d, init_color 0x%x, protocol %d, frtime_msec %d, debug %d" ENDCOLOR, m_opts.screen, m_opts.vgroup, m_opts.init_color, m_opts.protocol, m_opts.frtime_msec, m_opts.debug);
//        if (islistening()) debug(RED_MSG "TODO: check for arg mismatch" ENDCOLOR);
        }
//void make_fats(napi_env env, napi_value jsfunc, napi_threadsafe_function_call_js napi_cb, napi_threadsafe_function* fats) //asynchronous thread-safe JavaScript call-back function; can be called from any thread
        make_fats(env, jsfunc, napi_cb, &m_opts.fats); //last arg = js cb func
        m_opts.refill = std::bind([](/*napi_env env,*/ GpuPortData* aodata, const Nodebuf::TXTR* txtr)
        {
            aodata->dirty.store(0);
            if (txtr != NO_ADVANCE) ++aodata->m_frinfo.numfr; //advance to next frame
            if (!NAPI_OK(napi_call_threadsafe_function(aodata->m_opts.fats, aodata, napi_tsfn_blocking))) exc_hard("Can't call JS fats"); //get node values from js cb func
        }, /*env,*/ this, std::placeholders::_1);
//        m_refill = )(Nodebuf::TXTR*); //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
    }
    napi_value wrap_frinfo(napi_env env) { return m_frinfo.wrap(env); } //, napi_value* valp = 0) { m_frinfo.wrap(env, valp); }
    napi_value wrap_nodebuf(napi_env env) { return m_nodebuf.wrap(env); } //, napi_value* valp = 0) { m_nodebuf.wrap(env, valp); }
//start playback (bkg thread):
    void start() //napi_env env)
    {
        islistening(true);
//        UNIV_LEN(divup(/*m_cfg? m_cfg->vdisplay: UNIV_MAX*/ ScreenInfo(screen, NVL(srcline, SRCLINE))->bounds.h, vgroup)), //univ len == display height
//debug("here60" ENDCOLOR);
        m_nodebuf.reset(m_opts.screen, m_opts.vgroup, m_opts.init_color); //create txtr, wnd
//TODO?        m_nodebuf.wrap(env);
//TODO?        m_frinfo.wrap(env);
//debug("here61" ENDCOLOR);
        if (!NAPI_OK(napi_acquire_threadsafe_function(m_opts.fats))) exc_hard("Can't acquire JS fats");
        debug(15, PINK_MSG "start playback" ENDCOLOR);
//debug("here62" ENDCOLOR);
        m_frinfo.reset(); //(-1);
        m_frinfo.protocol = static_cast<Nodebuf::Protocol>(m_opts.protocol); //CAUTION: do this after frinfo.reset()
//debug("here63" ENDCOLOR);
        m_opts.refill(NO_ADVANCE);
//debug("here64" ENDCOLOR);
    }
//update screen (bkg thread):
//    void (*m_refill)(Nodebuf::TXTR*); //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
//    void make_refill()
//    static void refill(GpuPortData* aodata, napi_env env)
//    {
//        ++m_frinfo.numfr;
//        !NAPI_OK(napi_call_threadsafe_function(fats, this, napi_tsfn_blocking), "Can't call JS fats"); //get node values from js cb func
//    }
//    Nodebuf::TXTR::REFILL refill;
    void update() //napi_env env)
    {
        if (!islistening()) return;
//TODO: move this to frinfo, reconcile with frinfo.frame_time
        static const decltype(m_frinfo.elapsed()) TIMING_SLOP = 5; //allow +/-5 msec
        const decltype(m_frinfo.elapsed()) /*elapsed_t*/ overdue = m_opts.frtime_msec? m_frinfo.elapsed() - numfr.load() * m_opts.frtime_msec: 0, delay = (overdue < -TIMING_SLOP/2)? -overdue: 0;
        const char* severity = /*((overdue < -10) || (overdue > 10))? RED_MSG:*/ PINK_MSG;
        debug(15, severity << "update playback: fr# %s due at %s msec, overdue %s msec, delay? %s" ENDCOLOR, commas(numfr.load()), commas(numfr.load() * m_opts.frtime_msec), commas(overdue), commas(delay));
        if (delay) SDL_Delay(delay); //delay if early
//debug("here65" ENDCOLOR);
        VOID m_nodebuf.update(m_opts.refill, SRCLINE);
//debug("here66" ENDCOLOR);
    }
//stop playback (bkg thread):
    void stop() //napi_env env)
    {
        islistening(false);
//debug("here67" ENDCOLOR);
//        --aodata->m_frinfo.numfr;
        m_nodebuf.reset(); //close wnd/txtr, leave other data as-is for cb to see it
        m_opts.refill(NO_ADVANCE); //call one last time with results or exc
//        m_nodebuf.reset(m_opts.screen, m_opts.vgroup, m_opts.init_color);
        SDL_Delay(1 sec); //kludge: give cb time to execute before releasing fats
        if (!NAPI_OK(napi_release_threadsafe_function(m_opts.fats, napi_tsfn_release))) exc_hard("Can't release JS fats");
        m_opts.fats = NULL;
//debug("here68" ENDCOLOR);
        debug(15, PINK_MSG "stop playback after %s frames, %s msec" ENDCOLOR, commas(numfr.load()), commas(m_frinfo.elapsed()));
    }
//    void make_thread(napi_env env)
//    {
//no        !NAPI_OK(napi_create_int32(env, 1234, &nodes.value), "Create arg failed");
//no        !NAPI_OK(napi_create_int32(env, 5678, &frinfo.value), "Create arg failed");
//        islistening(true);
//    }
protected: //private data members
    const int valid2 = VALID; //put one at start and one at end
};
#undef GpuPortData  //GpuPortData_old
#endif


//limit brightness:
//NOTE: JS <-> C++ overhead is significant for this function
//it's probably better to just use a pure JS function for high-volume usage
napi_value Limit_NAPI(napi_env env, napi_callback_info info)
{
    if (!env) return NULL; //Node cleanup mode?
    DebugInOut("Limit_napi");

    ShmData* shmptr; //not used
    napi_value argv[1+1], This; //allow 1 extra arg to check for extras
    size_t argc = SIZEOF(argv);
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&shmptr), "Get cb info failed");
    if (argc != 1) NAPI_exc("expected 1 color param, got " << argc << " params");
//    aoptr->isvalid(env, SRCLINE); //doesn't matter here, but check anyway
//    if (argc < 1) 
//    napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
//    char str[1024];
//    size_t str_len;
//    status = napi_get_value_string_utf8(env, argv[0], (char *) &str, 1024, &str_len);
//    if (status != napi_ok) { napi_throw_error(env, "EINVAL", "Expected string"); return NULL; }
//    Napi::String str = Napi::String::New(env, )
    Uint32 color; //= BLACK;
    napi_value num_arg;
    !NAPI_OK(napi_coerce_to_number(env, argv[0], &num_arg), "Get arg as num failed");
    !NAPI_OK(napi_get_value_uint32(env, num_arg, &color), "Get uint32 colo failed");
//    using LIMIT = limit<pct(50/60)>; //limit brightness to 83% (50 mA/pixel instead of 60 mA); gives 15A/300 pixels, which is a good safety factor for 20A power supplies
//actual work done here; surrounding code is overhead :(
    color = limit<ShmData::BRIGHTEST>(color); //83% //= 3 * 212, //0xD4D4D4, //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA
//    napi_value retval;
//    !NAPI_OK(napi_create_uint32(env, color, &retval), "Cre retval failed");
//    return retval;
    return napi_thingy(env, color, napi_thingy::Uint32{});
}


#if 0
//return a js wrapper to node buf:
napi_value GetNodes_NAPI(napi_env env, napi_callback_info info)
{
//    elapsed_t started = now_msec();
    if (!env) return NULL; //Node cleanup mode?
    DebugInOut("GetNodes_napi", SRCLINE);

    GpuPortData* aoptr;
    napi_value argv[3+1], This; //allow 1 extra arg to check for extras
    size_t argc = SIZEOF(argv);
//    if (!env) return NULL; //Node cleanup mode
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aoptr), "Get cb info failed");
    debug(12, CYAN_MSG "get nodes: aoptr %p, valid? %d" ENDCOLOR, aoptr, aoptr->isvalid());
    if ((argc < 2) || (argc > 3)) NAPI_exc("expected 2-3 int args: q, w, h; got " << argc << " args");
//    debug(BLUE_MSG "async listen loop %d args: arg[0] " << napi_thingy(env, argv[0]) << ", arg[1] " << napi_thingy(env, argv[1]) << ENDCOLOR, argc);
    aoptr->isvalid(env, SRCLINE);
//no; allow restart    if (aoptr->islistening()) NAPI_exc("Already listening (single threaded for now)"); //check if que empty

    int frnum;
    SDL_Size want_wh;
//    !NAPI_OK(napi_coerce_to_number(env, retval/*.value*/, &num_retval.value), "Get retval as num failed");
    !NAPI_OK(napi_get_value_int32(env, argv[0], &want_wh.w), "Get int32 retval failed");
    !NAPI_OK(napi_get_value_int32(env, argv[1], &want_wh.h), "Get int32 retval failed");
    if ((want_wh.w < 1) || (want_wh.w > aoptr->m_nodebuf.wh.w)) NAPI_exc(env, "Bad #univ " << want_wh.w << ": should be 1.." << aoptr->m_nodebuf.wh.w);
    if ((want_wh.h < 1) || (want_wh.h > aoptr->m_nodebuf.wh.h)) NAPI_exc(env, "Bad univ len " << want_wh.h << ": should be 1.." << aoptr->m_nodebuf.wh.h);

//    if (!wh.w || !wh.h) NAPI_exc("no nodes");
    napi_thingy arybuf(env, &aoptr->m_nodebuf.nodes[0][0], sizeof(aoptr->m_nodebuf.nodes));
#if 0 //1D array; caller must use UNIV_MAXLEN_pad in index arithmeric :(
    napi_thingy typary(env, GPU_NODE_type, wh.w * wh.h, arybuf);
#else //2D array; helps separate univ from eacher
    napi_thingy typary(env);
    !NAPI_OK(napi_create_array_with_length(env, want_wh.w, &typary.value), "Cre outer univ ary failed");
//    NODEVAL nodes[NUM_UNIV][UNIV_MAXLEN_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
    for (int x = 0; x < want_wh.w; ++x)
    {
//TODO: add handle_scope? https://nodejs.org/api/n-api.html#n_api_making_handle_lifespan_shorter_than_that_of_the_native_method
        napi_thingy univ_ary(env, Nodebuf::GPU_NODE_type, want_wh.h, arybuf, x * Nodebuf::UNIV_MAXLEN_pad * sizeof(aoptr->m_nodebuf.nodes[0][0]));
        !NAPI_OK(napi_set_element(env, typary, x, univ_ary), "Cre inner node ary failed");
    }
#endif
    debug(19, YELLOW_MSG "nodes" << typary << ENDCOLOR);
//    const int REF_COUNT = 1;
//    !NAPI_OK(napi_create_reference(env, typary, REF_COUNT, &m_cached), "Cre ref failed"); //allow to be reused next time
    return typary;
}
#endif


#if 0
//convert results from wker thread to napi and pass to JavaScript callback:
//NOTE: this executes on Node main thread only
static void Listen_cb(napi_env env, napi_value jsfunc, void* context, void* data)
{
    UNUSED(context);
    if (!env) return; //Node cleanup mode
    DebugInOut("Listen_cb");

  // Retrieve the prime from the item created by the worker thread.
//    int the_prime = *(int*)data;
    GpuPortData_old* aoptr = static_cast<GpuPortData_old*>(data);
    debug(17, BLUE_MSG "listen js cb func: aodata %p, valid? %d, listening? %d, context %p, clup mode? %d" ENDCOLOR, aoptr, aoptr->isvalid(), aoptr->isvalid()? aoptr->islistening(): false, context, !env);
    aoptr->isvalid(env, SRCLINE);
//no; allow one last time    if (!aoptr->islistening()) return;
//    if (!aodata->listener.busy) NAPI_exc("not listening");
//no! finish the call if bkg thread requested it;    if (!aoptr->islistening()) NAPI_exc("not listening");
  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
//    debug(CYAN_MSG "cb %p" ENDCOLOR, aodata);
//    {
//    napi_thingy retval; retval.env = env;

    napi_value argv[3];
// Convert the integer to a napi_value.
//NOTE: need to lazy-load params here because bkg wker didn't exist first time Listen_NAPI() was called
//    aodata->wker_ok(env); //) NAPI_exc(env, "Gpu wker problem: " << aodata->exc_reason());
    !NAPI_OK(napi_create_int32(env, aoptr->numfr.load(), &argv[0]), "Create arg failed");
//    !NAPI_OK(napi_create_int32(env, 1234, &argv[1]), "Create arg failed");
    argv[1] = aoptr->wrap_nodebuf(env); //, &argv[1]); //CAUTION: must be called from Node fg thread; maybe also each time - napi doesn't like napi_values saved across calls?
#if 0
    Uint32 buf[10];
    debug(YELLOW_MSG "&buf[0] %p vs. &nodes[0][0]) %p %p, size %zu" ENDCOLOR, &buf[0], aoptr, &aoptr->m_nodebuf.nodes[0][0], sizeof(aoptr->m_nodebuf.nodes));
    napi_thingy arybuf(env, &aoptr->m_nodebuf.nodes[0][0], sizeof(aoptr->m_nodebuf.nodes));
    debug("arybuf5 " << arybuf << ENDCOLOR);
    napi_thingy wrapper(env, Nodebuf::GPU_NODE_type, 6, arybuf);
    debug("nodes5 " << wrapper << ENDCOLOR);
    argv[1] = wrapper;
#endif
    argv[2] = aoptr->wrap_frinfo(env); //, &argv[2]);
//    argv[2] = aoptr->m_frinfo.m_cached;
//    argv[1] = aoptr->nodes.value;
//    SNAT("node val", aoptr->nodes.value);
//    SNAT("argv[1]", argv[1]);
//    argv[2] = aoptr->frinfo.value;
//    napi_thingy temp(env, argv[1]);
//    SNAT("thingy argv[1]", temp); //napi_thingy(env, argv[1]));

    uint32_t ready_bits; //UNIV_MASK
    napi_thingy cbthis(env), retval(env), num_retval(env); //, This;
//CAUTION: seems to be some undocumented magic here: need to pass Undefined as "this" (2nd) arg here or else memory errors occur
//see "this" at https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/this#As_an_object_method
//HERE(1);
//    INSPECT(GREEN_MSG << "js func call: func " << napi_thingy(env, jsfunc));
//    INSPECT(GREEN_MSG << "js func call: this " << aoptr->cbthis);
//    INSPECT(GREEN_MSG << "js func call: arg[0] " << napi_thingy(env, argv[0]));
//HERE(2);
//    INSPECT(GREEN_MSG << "js func call: arg[1] " << napi_thingy(env, argv[1])); //aoptr->nodes << ENDCOLOR);
//    INSPECT(GREEN_MSG << "js func call: arg[2] " << napi_thingy(env, argv[2])); //aoptr->frinfo << ENDCOLOR);
//    INSPECT(GREEN_MSG << "js func call: func " << napi_thingy(env, jsfunc) << ", this " << /*aoptr->*/cbthis << ", arg[0] " << napi_thingy(env, argv[0]) << ", arg[1] " << napi_thingy(env, argv[1]) << ", arg[2] " << napi_thingy(env, argv[2]));
//    {
//        DebugInOut("js func call for fr# " << aoptr->numfr.load(), SRCLINE); //<< ", retval init " << retval, SRCLINE);
    !NAPI_OK(napi_call_function(env, /*aoptr->*/cbthis.value, jsfunc, SIZEOF(argv), argv, &retval.value), "Call JS fats failed");
    if (retval.type() == napi_undefined) exc_hard("js cb did not return a value"); //js function might have aborted with error
//    }
//HERE(3);
//    debug(BLUE_MSG "cb: check fats retval" ENDCOLOR);
//        /*int32_t*/ bool want_continue;
//static int count = 0; aodata->want_continue = (count++ < 7); return;
    !NAPI_OK(napi_coerce_to_number(env, retval/*.value*/, &num_retval.value), "Get retval as num failed");
//    debug(BLUE_MSG "cb: get bool %p" ENDCOLOR, aodata);
    !NAPI_OK(napi_get_value_uint32(env, num_retval/*.value*/, &ready_bits), "Get uint32 retval failed");
    debug(17, BLUE_MSG "js fats: got retval " << retval << " => num " << num_retval << " => ready bits 0x%x, new dirty 0x%x, continue? %d, undef? %d" ENDCOLOR, ready_bits, aoptr->dirty | ready_bits, !!ready_bits, retval.type() == napi_undefined);
    if (!ready_bits) aoptr->islistening(false); //caller signals eof
    aoptr->dirty |= ready_bits; //.fetch_or(ALL_UNIV || more, SRCLINE); //mark rendered universes; wake up bkg wker (even wth no new dirty univ so it will see cancel)
//    ++aoptr->numfr;
//    aodata->frinfo.refill(); //++aodata->gpdata.numfr; //move to next frame
//NOTE: bkg thread will respond to busy resest
//    }
//    if (!aodata->want_continue) //tell NAPI js fats will no longer be used
//        !NAPI_OK(napi_release_threadsafe_function(aodata->fats, napi_tsfn_release), "Can't release JS fats");
  // Free the item created by the worker thread.
//    free(data);
//HERE(4);
//    debug(BLUE_MSG "Listen: return %d msec" ENDCOLOR, now_msec() - started);
//didn't help    if (!ready_bits) aoptr->reset(env);
}


napi_value Listen_NAPI(napi_env env, napi_callback_info info)
{
//    elapsed_t started = now_msec();
    if (!env) return NULL; //Node cleanup mode?
    DebugInOut("Listen_napi", SRCLINE);

    GpuPortData_old* aoptr;
    napi_value argv[2+1], This; //allow 1 extra arg to check for extras
    size_t argc = SIZEOF(argv);
//    if (!env) return NULL; //Node cleanup mode
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aoptr), "Get cb info failed");
    debug(12, CYAN_MSG "async listen loop: aoptr %p, valid? %d" ENDCOLOR, aoptr, aoptr->isvalid());
    if ((argc < 1) || (argc > 2)) NAPI_exc("expected 1-2 args: [{opts}], cb(); got " << argc << " args");
//    debug(BLUE_MSG "async listen loop %d args: arg[0] " << napi_thingy(env, argv[0]) << ", arg[1] " << napi_thingy(env, argv[1]) << ENDCOLOR, argc);
    aoptr->isvalid(env, SRCLINE);
//no; allow restart    if (aoptr->islistening()) NAPI_exc("Already listening (single threaded for now)"); //check if que empty

//    napi_thingy opts(env);
//    if (argc > 1) opts.value = argv[0]; //first arg = options
    napi_thingy undef(env);
//    if (argc < 2) aoptr->set_opts();
//    else aoptr->set_opts(env, argv[0]);
//    make_fats(env, argv[argc - 1], Listen_cb, &aoptr->fats); //last arg = js cb func
//    aoptr->make_refill(env, argv[argc - 1], Listen_cb);
//debug("here80" ENDCOLOR);
    aoptr->set_opts(env, (argc > 1)? argv[0]: undef.value, argv[argc - 1], Listen_cb);
//debug("here81" ENDCOLOR);
//    aoptr->wrap_frinfo(env); //set up cb info
//debug("here82" ENDCOLOR);
//BROKEN:    aoptr->wrap_nodebuf(env); //, &argv[1]); //CAUTION: must be called each time; napi doesn't like napi_values saved across calls
//debug("here83" ENDCOLOR);
//    aoptr->mk_nodes(env);
//    aoptr->mk_frinfo(env);
//    debug(BLUE_MSG "Listen: setup %d msec" ENDCOLOR, now_msec() - started);
    inout.checkpt("setup");

//run txtr updates on bkg thread so fg Node thread doesn't block:
//    aoptr->islistening(true);
//#if 1
//    aoptr->make_thread(env);
#if 0
    Uint32 buf[10];
    napi_thingy arybuf(env, &buf[0], sizeof(buf));
    debug("arybuf5 " << arybuf << ENDCOLOR);
    napi_thingy wrapper(env, Nodebuf::GPU_NODE_type, 6, arybuf);
    debug("nodes5 " << wrapper << ENDCOLOR);
#endif
//return NULL;
    std::thread bkg([/*env,*/ aoptr]() //env, screen, vgroup, init_color]()
    {
//        napi_status status;
//        ExecuteWork(env, addon_data);
//        WorkComplete(env, status, addon_data);
        debug(12, PINK_MSG "bkg: aodata %p, valid? %d" ENDCOLOR, aoptr, aoptr->isvalid());
//        debug(YELLOW_MSG "bkg acq" ENDCOLOR);
//        !NAPI_OK(napi_acquire_threadsafe_function(aoptr->fats), "Can't acquire JS fats");
//        !NAPI_OK(napi_reference_ref(env, aodata->listener.ref, &ref_count), "Listener ref failed");
//        aoptr->islistening(true);
        aoptr->start(); //env);
        try
        {
            for (;;) //int i = 0; i < 5; ++i)
            {
                DebugInOut("call fats for fr# " << commas(aoptr->/*m_frinfo.*/numfr.load()) << ", wait for 0x" << std::hex << Nodebuf::ALL_UNIV << std::dec << " (blocking)", SRCLINE);
//                !NAPI_OK(napi_call_threadsafe_function(aoptr->fats, aoptr, napi_tsfn_blocking), "Can't call JS fats");
//            while (aoptr->islistening()) //break; //allow cb to break out of playback loop
//    typedef std::function<bool(void)> CANCEL; //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
                aoptr->dirty.wait(Nodebuf::ALL_UNIV, [aoptr](){ return !aoptr->islistening(); }, true, SRCLINE); //CAUTION: blocks until al univ ready or caller cancelled
//                const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE};
//                for (int i = 0; i < SIZEOF(aoptr->m_nodebuf.nodes); ++i) aoptr->m_nodebuf.nodes[0][i] = palette[aoptr->numfr.load() % SIZEOF(palette)];
//                if (aoptr->numfr.load() >= 10) break;
                debug(12, BLUE_MSG "bkg woke from fr# %s with ready 0x%x, caller listening? %d" ENDCOLOR, commas(aoptr->numfr.load()), aoptr->dirty.load(), aoptr->islistening());
                if (!aoptr->islistening()) break;
//                SDL_Delay(1 sec);
                aoptr->update(); //env);
            }
//            SDL_Delay(1 sec);
//https://stackoverflow.com/questions/233127/how-can-i-propagate-exceptions-between-threads
        }
        catch (...)
        {
            aoptr->excptr = std::current_exception(); //allow main thread to rethrow
            debug(12, RED_MSG "bkg wker exc: %s" ENDCOLOR, aoptr->exc_reason().c_str());
//            aoptr->islistening(false); //listener.busy = true; //(void*)1;
        }
        aoptr->stop(); //env);
//        !NAPI_OK(napi_release_threadsafe_function(aoptr->fats, napi_tsfn_release), "Can't release JS fats");
//        aoptr->fats = NULL;
//        aodata->listener.busy = false; //work = 0;
        debug(12, YELLOW_MSG "bkg exit after %s frames" ENDCOLOR, commas(aoptr->/*m_frinfo.*/numfr.load()));
    });
    bkg.detach();
//#endif
//debug("here84" ENDCOLOR);
    aoptr->dirty.wait(0, NULL, true, SRCLINE); //block until bkg thread starts so frinfo values will be filled in; *don't* check islistening() - bkg thread might not be running yet
    debug(15, BLUE_MSG "sync verify: woke up with dirty = 0x%x" ENDCOLOR, aoptr->dirty.load());
    napi_thingy retval(env);
    retval = aoptr->wrap_frinfo(env); //napi_thingy::Object{});
    aoptr->wrap_nodebuf(env); //go ahead and wrap nodebuf now also even though caller doesn't need it
//    debug(BLUE_MSG "Listen: return %d msec" ENDCOLOR, now_msec() - started);
#if 1 //check if cached correctly:
    aoptr->wrap_frinfo(env); //napi_thingy::Object{});
    aoptr->wrap_nodebuf(env); //go ahead and wrap nodebuf now also even though caller doesn't need it
#endif
    return retval; //NULL; //aoptr->cbthis; //TODO: where to get this?
}
#endif


//template <typename TYPE = uint32_t, typename SUPER = std::unique_ptr<TYPE>> //, size_t ENTS = 1, bool AUTO_INIT = true, bool WANT_MUTEX = false> //, typename PTRTYPE=TYPE*>
//class ShmUniquePtr: public SUPER
//{
//public
//};


//export napi functions to js callers:
napi_value GpuModuleInit(napi_env env, napi_value exports)
{
//    DebugInOut("ModuleInit", SRCLINE);
    uint32_t napi_ver;
    const napi_node_version* ver;
    !NAPI_OK(napi_get_node_version(env, &ver), "Get node version info failed");
    !NAPI_OK(napi_get_version(env, &napi_ver), "Get napi version info failed");
    debug(9, "using Node v" << ver->major << "." << ver->minor << "." << ver->patch << " (" << ver->release << "), N-API v" << napi_ver);
    exports = module_exports(env, exports); //include previous exports

//    std::unique_ptr<GpuPortData> aodata(new GpuPortData(/*env,*/ SRCLINE)); //(GpuPortData*)malloc(sizeof(*addon_data));
//    GpuPortData* aoptr = aodata.get();
//    AutoShmary<ShmData> shdata;
//    ShmData* shmptr = shdata.ptr();
#if 0
    using mySDL_AutoWindow_super = std::unique_ptr<SDL_Window, std::function<void(SDL_Window*)>>; //DRY kludge
    std::function<void(void*)> deleter = [](void* shmdata)
    {
        ShmData* shmptr = static_cast<ShmData*>(shmdata); //(GpuPortData*)data;
        if (!shmptr) return;

//see custom lamba deleter example at https://en.cppreference.com/w/cpp/memory/unique_ptr
//[](SDL_Surface* surf){ 
        shmfree_typesafe<ShmData>(shmdata, SRCLINE);
        debug(LEVEL, RED_MSG "SDL_AutoWindow: destroy window %p" ENDCOLOR, ptr);
//debug("here24" ENDCOLOR);
    }
#endif
//wrap shmdata struct like a std::unique_ptr<> to normal heap:
//    using ShmDeleter = std::function<int(void*)>; //shmfree shim; keep it DRY
//    ShmData* shmptr = shmalloc_typesafe<ShmData>(ShmData::SHMKEY, 1, SRCLINE);
//    ShmDeleter dtor = std::bind(shmfree_typesafe<ShmData>, std::placeholders::_1, SRCLINE);
//    std::unique_ptr<ShmData, ShmDeleter> shmdata(shmptr, dtor); // ) ShmData(env, SRCLINE)); //(GpuPortData*)malloc(sizeof(*addon_data));
    std::unique_ptr<ShmData> shmdata(ShmData::my(shmalloc_debug(sizeof(ShmData), ShmData::FramebufQuent::SHMKEY, SRCLINE))); // ) ShmData(env, SRCLINE)); //(GpuPortData*)malloc(sizeof(*addon_data));
    ShmData* shmptr = shmdata.get();
    bool isnew = (shmnattch(shmptr) == 1);
    debug(5, "ModuleInit: shmptr %p, #attach %d, valid? %d, isnew? %d", shmptr, shmnattch(shmptr), shmptr->isvalid(), isnew);
    if (isnew) new (shmptr) ShmData; //placement "new" to call ctor; CAUTION: first time only
    if (/*(shmdata.get() != shmptr) ||*/ !shmptr->isvalid()) NAPI_exc((isnew? "alloc": "reattch") << " shmdata " << shmptr << " failed");
    napi_thingy my_exports(env, shmptr->my_exports(env, exports));
    if (/*(shmdata.get() != shmptr) ||*/ !shmptr->isvalid()) NAPI_exc((isnew? "alloc": "reattch") << " shmdata " << shmptr << " failed"); //paranoid/debug; check again
//    debug(11, BLUE_MSG "aodata %p, &node[0][0[0] %p" ENDCOLOR, aoptr, &aoptr->m_nodebq[0].nodes[0][0]);
//    inout.checkpt("cre data");
//    aoptr->isvalid(env);
//expose methods for caller to use:
//    my_napi_property_descriptor props[7], *pptr = props; //TODO: use std::vector<>
    vector_cxx17<my_napi_property_descriptor> props;
//debug(9, BLUE_MSG "here10" ENDCOLOR);
//    memset(&props[0], 0, sizeof(props)); //clear first so NULL members don't need to be explicitly set below
//kludge: use lambas in lieu of C++ named member init:
//(named args easier to maintain than long param lists)
    add_method("limit", Limit_NAPI, shmptr)(props.emplace_back()); //(*pptr++);
//    add_method("open", ShmData::Open_NAPI, shmptr)(methods.emplace_back()); //(*pptr++);
//    add_method("close", ShmData::Close_NAPI, shmptr)(methods.emplace_back()); //(*pptr++);
#if 0 //broken in slave process
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "nodebufq"; //"GetNodes";
//        _.method = GetNodes_NAPI;
//debug(9, BLUE_MSG "here11" ENDCOLOR);
        _.value = aoptr->m_nodebq[0].expose(env, aoptr->m_nodebq);
//debug(9, BLUE_MSG "here12" ENDCOLOR);
        _.attributes = napi_default; //!writable, !enumerable
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
#endif
#if 0
    [/*env,*/ aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "listen";
        _.method = Listen_NAPI;
        _.attributes = napi_default; //!writable, !enumerable
        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
#endif
#if 0
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "NUM_UNIV";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.w/*NUM_UNIV*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, ShmData::NUM_UNIV); //aoptr->m_nodebuf.wh.w);
        _.attributes = napi_default; //!writable, !enumerable
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "UNIV_MAXLEN";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, ShmData::UNIV_MAXLEN_pad); //give caller actual row len for correct node addressing
        _.attributes = napi_default; //!writable, !enumerable
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
//expose Protocol types:
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "NONE";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, static_cast<int32_t>(ShmData::Protocol::NONE));
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "DEV_MODE";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, static_cast<int32_t>(ShmData::Protocol::DEV_MODE));
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "WS281X";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, static_cast<int32_t>(ShmData::Protocol::WS281X));
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "CANCEL";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, static_cast<int32_t>(ShmData::Protocol::CANCEL));
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "version";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, ShmData::VERSION); //"0.11.18");
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
#endif
//TODO: define primary colors?
//decorate exports with the above-defined properties:
//    if (pptr - props > SIZEOF(props)) NAPI_exc("prop ary overflow: needed " << (pptr - props) << ", only had space for " << SIZEOF(props));
//    !NAPI_OK(napi_define_properties(env, exports, pptr - props, props), "export method props failed");
    debug(9, "export %d more methods/props", props.size());
//    !NAPI_OK(napi_define_properties(env, exports, props.size(), props.data()), "export methods/props failed");
//    napi_thingy more_exports(env, exports);
    my_exports += props;
//    return napi_thingy(env, more_retval) += props;
//    methods = shmptr->my_exports(env, methods);
//debug(9, BLUE_MSG "here14" ENDCOLOR);

//wrap internal data with module exports object:
  // Associate the addon data with the exports object, to make sure that when the addon gets unloaded our data gets freed.
//    void* NO_HINT = NULL; //optional finalize_hint
// Free the per-addon-instance data.
//TODO: find out why this is not being called
    napi_finalize /*std::function<void(napi_env, void*, void*)>*/ addon_final = [](napi_env env, void* shmdata, void* hint)
    {
        UNUSED(hint);
//        GpuPortData* aoptr = static_cast<GpuPortData*>(aodata); //(GpuPortData*)data;
        ShmData* shmptr = ShmData::my(shmdata); //(GpuPortData*)data;
//    if (!env) return; //Node cleanup mode
        debug(9, RED_MSG "addon finalize: aodata %p, valid? %d, open? %d, hint %p, #nattch %d", shmptr, shmptr->isvalid(), shmptr->isopen(), hint, shmnattch(shmptr));
//        aoptr->isvalid(env);
//        if (aoptr->isopen()) NAPI_exc("GpuPort still open");
//        aoptr->reset(env);
//        !NAPI_OK(napi_delete_reference(env, shmptr->ref), "Del ref failed");
//        shmptr->ref = nullptr;
        if (shmnattch(shmptr) == 1) shmptr->~ShmData(); //call dtor before dealloc/dettach
//        delete shmptr; //free(addon_data);
        shmfree_debug(shmptr, SRCLINE); //dealloc/dettach
    };
//debug(9, BLUE_MSG "here15" ENDCOLOR);
    napi_ref* NO_REF = NULL; //optional ref to wrapped object
    !NAPI_OK(napi_wrap(env, my_exports, shmptr, addon_final, /*aoptr.get()*/NO_HINT, /*&shmptr->ref*/ NO_REF), "Wrap shmdata failed");
  // Return the decorated exports object.
//    napi_status status;
//    INSPECT(CYAN_MSG "napi init: " << *aoptr, SRCLINE);
//debug(9, BLUE_MSG "here16" ENDCOLOR);
//    aoptr->isvalid(env);
//debug(9, BLUE_MSG "here17" ENDCOLOR);
    if (/*(shmdata.get() != shmptr) ||*/ !shmptr->isvalid()) NAPI_exc((isnew? "alloc": "reattch") << " shmdata " << shmptr << " failed"); //paranoid/debug; check again
    shmdata.release(); //NAPI owns it now; finalize will clean it up
//debug(9, BLUE_MSG "here18" ENDCOLOR);
    return my_exports;
}
// NAPI_MODULE(NODE_GYP_MODULE_NAME, ModuleInit)
 #endif //def SRC_NODE_API_H_ //USE_NAPI

 #undef module_exports
 #define module_exports  GpuModuleInit //cumulative exports
#endif //def WANT_REAL_CODE







//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////





#ifdef WANT_BROKEN_CODE
 #ifdef SRC_NODE_API_H_ //USE_NAPI
 //#include <string>
//#include "sdl-helpers.h"
//set some hard-coded defaults:
//using LIMIT = limit<83>;
//#define LIMIT  limit<pct(50/60)> //limit brightness to 83% (50 mA/pixel instead of 60 mA); gives 15A/300 pixels, which is a good safety factor for 20A power supplies
//using GPUPORT = GpuPort<> gp/* = GpuPort<>::factory*/(NAMED{ _.screen = screen; _.vgroup = vgroup; /*_.wh = wh*/; _.protocol = static_cast<GpuPort<>::Protocol>(protocol) /*GpuPort<>::NONE WS281X*/; _.init_color = dimARGB(0.25, RED); SRCLINE; }); //NAMED{ .num_univ = NUM_UNIV, _.univ_len = UNIV_LEN, SRCLINE});

//below loosely based on object_wrap and async_work_thread_safe_function examples at https://github.com/nodejs/node-addon-examples.git
//#include <assert.h>
//#include <stdlib.h>
#include <memory> //std::unique_ptr<>

#include "GpuPort.h"
using GPUPORT = GpuPort
<
    /*int CLOCK =*/ 52 MHz, //pixel clock speed (constrained by GPU)
    /*int HTOTAL =*/ 1536, //total x res including blank/sync (might be contrained by GPU); 
    /*int FPS =*/ 30, //target #frames/sec
    /*int MAXBRIGHT =*/ pct(50/60), //3 * 212, //0xD4D4D4, //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA
//settings that must match h/w:
    /*int IOPINS =*/ 24, //total #I/O pins available (h/w dependent)
    /*int HWMUX =*/ 0, //#I/O pins to use for external h/w mux
    /*bool BKG_THREAD =*/ false //true //taking explicit control of bkg wker below so bkg not needed
//    int NODEBITS = 24> //# bits to send for each WS281X node (protocol dependent)
>;
static const napi_typedarray_type GPU_NODE_type = napi_uint32_array; //NOTE: must match GPUPORT NODEVAL type

struct GpuPortData
{
    const int valid1 = VALID;
    static const int VALID = 0x1234beef;
//    int frnum;
//    int the_prime;
    GPUPORT::WKER* gpu_wker = 0;
//    struct { int numfr; } gpdata; //TODO
//kludge: can't create typed array while running, so create them up front
//TODO: find out why cre typed array later no worky!
    using SHDATA = GPUPORT::WKER::shdata;
    std::unique_ptr<SHDATA, std::function<int(SHDATA*)>> m_shmptr; //define as member data to avoid WET defs needed for class derivation; NOTE: must come before depend refs below; //NODEBUF_FrameInfo, NODEBUF_deleter>; //DRY kludge
//    m_shmptr = decltype(m_shmptr)(shmalloc_typed<SHDATA>(0), std::bind(shmfree, std::placeholders::_1, SRCLINE)); //NVL(srcline, SRCLINE))), //shim; put nodes in shm so multiple procs/threads can render
    SHDATA& m_shdata; //= *m_shmptr.get();
    std::exception_ptr excptr = nullptr; //init redundant (default init)
    int svfrnum;
//    std::string exc_reason;
//https://stackoverflow.com/questions/233127/how-can-i-propagate-exceptions-between-threads
//    bool want_continue;
//    napi_ref gpwrap;
//    GPUPORT::SHAREDINFO gpdata;
    napi_thingy nodes, frinfo;
//    std::thread wker; //kludge: force thread affinity by using explicit thread object
//    struct { napi_thingy obj; napi_ref ref; bool busy = false; } listener;
    napi_thingy listener; //"this" object for js async callback to use; also DRY busy flag: null == !busy
//    /*napi_async_work*/ void* work = NULL;
    napi_threadsafe_function fats; //asynchronous thread-safe JavaScript call-back function; can be called from any thread
//    napi_ref aoref; //ref to wrapped version of this object
    const int valid2 = VALID;
public: //ctors/dtors
    explicit GpuPortData(napi_env env):
        nodes(env), frinfo(env), listener(env), //NOTE: need these because default ctor deleted
        m_shmptr(shmalloc_typed<SHDATA>(SHM_LOCAL), std::bind(shmfree, std::placeholders::_1, SRCLINE)), //NVL(srcline, SRCLINE))), //shim; put nodes in shm so multiple procs/threads can render
        m_shdata(*m_shmptr.get())
    {
        napi_value dummy;
        nodes_setup(env, &dummy); //kludge: pre-alloc to avoid memory problems
        frinfo_setup(env, &dummy); //kludge: pre-alloc to avoid memory problems
        islistening(false); //listener.busy = false;
        INSPECT(CYAN_MSG "ctor " << *this);
    }
    ~GpuPortData()
    {
        INSPECT(CYAN_MSG "dtor " << *this);
//        listener.busy = false;
//        if (valtype(listener) != napi_undefined)
//            !NAPI_OK(napi_delete_reference(listener.obj.env, listener.ref), listener.obj.env, "Del ref failed");
        islistening(false); //in case caller added refs to other objects; probably not needed
//        !NAPI_OK(napi_get_null(listener.env, &listener.value), listener.env, "Get null failed");
        if (gpu_wker) delete gpu_wker; //close at playlist end, not after each sequence
        gpu_wker = 0;
    }
public: //methods
//    enum tristate: int {False = 0, True = 1, Maybe = -1};
    inline bool isvalid() const { return (this && (valid1 == VALID) && (valid2 == VALID)); } //paranoid/debug
    inline bool wker_ok() const { return (isvalid() && gpu_wker && !excptr); }
    GPUPORT::WKER* wker_ok(napi_env env, SrcLine srcline = 0) const //bool want_valid_ignored) const
    {
        return wker_ok()? gpu_wker: (GPUPORT::WKER*)NAPI_exc(env, NO_ERRCODE, "Gpu wker problem: " << exc_reason(), NVL(srcline, SRCLINE));
    }
//keep it DRY by using listener value itself:
    inline bool islistening() const { return (listener.type() != napi_null); }
    inline bool islistening(bool yesno) { return islistening(yesno, listener.env); }
    bool islistening(bool yesno, napi_env env)
    {
        listener.env = env;
        if (yesno) !NAPI_OK(napi_get_undefined(env, &listener.value), "Get undef failed");
        else !NAPI_OK(napi_get_null(env, &listener.value), "Get null failed");
        return yesno; //islistening();
    }
    std::string exc_reason(const char* no_reason = 0) const
    {
        static std::string reason;
        reason.assign(NVL(no_reason, "(no reason)"));
        if (!excptr) return reason;
//get desc or other info for caller:
        try { std::rethrow_exception(excptr); }
//            catch (...) { excstr = std::current_exception().what(); }
        catch (const std::exception& exc) { reason = exc.what(); }
        catch (...) { reason = "??EXC??"; }
        return reason;
    }
//paranoid checks:
//    napi_finalize thread_final = [](napi_env env, void* data, void* hint)
    static void wker_check(napi_env env, void* data, void* hint)
    {
        UNUSED(hint);
        GpuPortData* aodata = static_cast<GpuPortData*>(data);
        debug(9, RED_MSG "thread final: aodata %p, valid? %d, hint %p", aodata, aodata->isvalid(), hint);
//    if (!env) return; //Node cleanup mode
        aodata->wker_ok(env, SRCLINE); //) NAPI_exc("Gpu wker problem: " << aodata->exc_reason());
    }
//lazy data setup for js:
//can't be set up before gpu_wker is created
//create typed array wrapper for nodes:
    void nodes_setup(napi_env env, napi_value* valp)
    {
        if (nodes.arytype() != napi_uint32_array) //napi_typedarray_type)
        {
            nodes.env = env;
            napi_thingy arybuf(env);
            Uint32 junk[3][5];
//napi_get_null(env, &arybuf.value);
//napi_get_null(env, &nodes.value);
//            void* NO_HINT = NULL; //optional finalize_hint
//debug("env " << env << ", size " << commas(sizeof(gpu_wker->m_nodes)) << ENDCOLOR);
            debug(9, "arybuf1 " << arybuf);
            !NAPI_OK(napi_create_int32(env, 1234, &arybuf.value), "cre int32 failed");
            debug(9, "arybuf2 " << arybuf);
            !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &arybuf.value), "cre str failed");
            debug(9, "arybuf3 " << arybuf);
            !NAPI_OK(napi_create_external_arraybuffer(env, &junk[0][0], sizeof(junk), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
            debug(9, "arybuf4 " << arybuf);
            !NAPI_OK(napi_create_external_arraybuffer(env, &/*gpu_wker->*/m_shdata.nodes[0][0], sizeof(/*gpu_wker->*/m_shdata.nodes), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
            debug(9, "arybuf5 " << arybuf);

            debug(9, "nodes1 " << nodes);
            !NAPI_OK(napi_create_int32(env, 1234, &nodes.value), "cre int32 failed");
            debug(9, "nodes2 " << nodes);
            !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &nodes.value), "cre str failed");
            debug(9, "nodes3 " << nodes);
            !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF_2D(junk), arybuf.value, 0, &nodes.value), "Cre nodes typed array failed");
            debug(9, "nodes4 " << nodes);
            !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF_2D(/*gpu_wker->*/m_shdata.nodes), arybuf.value, 0, &nodes.value), "Cre nodes typed array failed");
            debug(9, "nodes5 " << nodes);
            debug(9, YELLOW_MSG "nodes typed array created: &node[0][0] " << &/*gpu_wker->*/m_shdata.nodes[0][0] << ", #bytes " <<  commas(sizeof(/*gpu_wker->*/m_shdata.nodes)) << ", " << commas(SIZEOF_2D(/*gpu_wker->*/m_shdata.nodes)) << " " << NVL(TypeName(GPU_NODE_type)) << " elements, arybuf " << arybuf << ", nodes thingy " << nodes);
        }
        if (nodes.env != env) NAPI_exc("nodes env mismatch");
        *valp = nodes.value;
    }
//create structured object wrapper for frame info:
//klugde: define a place for srcline below
//    static constexpr SrcLine& outer_srcline()
//    {
//        static SrcLine m_srcline; //kludge: create a place for _.srcline
//        return m_srcline;
//    }
//    struct my_napi_property_descriptor: public napi_property_descriptor
//    {
////        static constexpr SrcLine& srcline = outer_srcline(); //kludge: create a place for _.srcline
//        static SrcLine srcline; //= outer_srcline(); //kludge: create a place for _.srcline
////    my_napi_property_descriptor()
//    };
//    SrcLine my_napi_property_descriptor::srcline; //kludge: create a place for _.srcline
    void frinfo_setup(napi_env env, napi_value* valp)
    {
        if (nodes.type() != napi_object)
        {
            frinfo.env = env;
            napi_thingy arybuf(env);
//napi_get_null(env, &arybuf.value);
//napi_get_null(env, &frinfo.value);
//            void* NO_HINT = NULL; //optional finalize_hint
//            !NAPI_OK(napi_create_external_arraybuffer(env, &gpu_wker->m_frinfo, sizeof(gpu_wker->m_frinfo), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
//            !NAPI_OK(napi_create_dataview(env, sizeof(gpu_wker->m_frinfo), arybuf, 0, &frinfo.value), "Cre frinfo data view failed");
//            debug(YELLOW_MSG "frinfo data view created: &data " << &gpu_wker->m_frinfo << ", size " << commas(sizeof(gpu_wker->m_frinfo)) << ", arybuf " << arybuf << ", frinfo thingy " << frinfo << ENDCOLOR);
            !NAPI_OK(napi_create_object(env, &frinfo.value), "Cre frinfo obj failed");
//Protocol protocol; //= WS281X;
//const double frame_time; //msec
//const SDL_Size wh; //int NumUniv, UnivLen;
//const /*elapsed_t*/ /*std::result_of<now_msec()>::type*/ decltype(now_msec()) started;
//std::atomic</*uint32_t*/ int> numfr; //= 0; //#frames rendered / next frame#
//BkgSync<MASK_TYPE, true> dirty; //one Ready bit for each universe
//uint64_t times[NUM_STATS]; //total init/sleep (sec), render (msec), upd txtr (msec), xfr txtr (msec), present/sync (msec)
            my_napi_property_descriptor props[7], *pptr = props;
            memset(&props[0], 0, sizeof(props)); //clear first so NULL members don't need to be explicitly set
//kludge: use lambas in lieu of C++ named member init:
//(named args easier to maintain than long param lists)
            [env, this](auto& _)
            {
                _.utf8name = "protocol"; //_.name = NULL;
//                _.method = NULL;
//                _.getter = GetProtocol_NAPI; _.setter = SetProtocol_NAPI; //read/write; allow live update
                _.getter = [](napi_env env, napi_callback_info info) -> napi_value //void* data) //GetProtocol_NAPI; //live update
                {
                    GpuPortData* aodata;
                    napi_value argv[1], This;
                    size_t argc = SIZEOF(argv);
                    if (!env) return NULL; //Node cleanup mode?
                    struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
                    !NAPI_OK(napi_create_int32(env, static_cast<int32_t>(aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol), &argv[0]), "Get uint32 getval failed");
                    return argv[0];
                };
                _.setter = [](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //SetProtocol_NAPI; //live update
                {
                    GpuPortData* aodata;
                    napi_value argv[1+1], This;
                    size_t argc = SIZEOF(argv);
                    if (!env) return NULL; //Node cleanup mode?
                    struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Setter info extract failed");
                    if (argc != 1) NAPI_exc("got " << argc << " args, expected 1");
                    int /*int32_t*/ prtemp;
                    !NAPI_OK(napi_get_value_int32(env, argv[0], &prtemp), "Get uint32 setval failed");
                    aodata->wker_ok(env, SRCLINE)->m_frinfo.protocol = static_cast<GPUPORT::Protocol>(prtemp);
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    aodata->wker_ok(env)->m_frinfo.protocol = 0; //TODO
                };
//                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.protocol, &_.value), "Cre protocol int failed");
                _.attributes = napi_enumerable; //napi_default; //napi_writable | napi_enumerable; //read-write; //napi_default;
                _.data = this;
            }(*pptr++);
//            NAMED{ _.itf8name = "protocol"; _.getter = GetProtocol_NAPI; _.setter = SetProtocol_NAPI; _.attributes = napi_default; }(*props++);
            [env, this](auto& _) //napi_property_descriptor& _)
            {
                _.utf8name = "frame_time";
//                _.getter = GetFrameTime_NAPI; //read-only
                !NAPI_OK(napi_create_double(env, gpu_wker->m_frinfo.frame_time, &_.value), "Cre frame_time float failed");
                _.attributes = napi_enumerable; //read-only; //napi_default;
            }(*pptr++);
            [env, this](auto& _) //napi_property_descriptor& _)
            {
                _.utf8name = "NUM_UNIV";
//                _.getter = GetNumUniv_NAPI; //read-only
                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.wh.w, &_.value), "Cre w int failed");
                _.attributes = napi_enumerable; //read-only; //napi_default;
            }(*pptr++);
            [env, this](auto& _) //napi_property_descriptor& _)
            {
                _.utf8name = "UNIV_LEN";
                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.wh.h, &_.value), "Cre h int failed");
                _.attributes = napi_enumerable; //read-only; //napi_default;
            }(*pptr++);
            [env, this](auto& _) //napi_property_descriptor& _)
            {
                _.utf8name = "started";
                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.started, &_.value), "Cre started int failed");
                _.attributes = napi_enumerable; //read-only; //napi_default;
            }(*pptr++);
            [env, this](auto& _) //napi_property_descriptor& _)
            {
                _.utf8name = "numfr";
//                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.numfr.load(), &_.value), "Cre #fr int failed");
                _.getter = [](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetNumfr_NAPI; //live update
                {
                    GpuPortData* aodata;
                    napi_value argv[1], This;
                    size_t argc = SIZEOF(argv);
                    if (!env) return NULL; //Node cleanup mode?
                    struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.numfr.load();
                    !NAPI_OK(napi_create_uint32(env, aodata->wker_ok(env, SRCLINE)->m_frinfo.numfr.load(), &argv[0]), "Get uint32 getval failed");
                    return argv[0];
                };
                _.attributes = napi_enumerable; //read-only; //napi_default;
                _.data = this;
            }(*pptr++);
            [env, this](auto& _) //napi_property_descriptor& _)
            {
                _.utf8name = "dirty";
//                !NAPI_OK(napi_create_uint32(env, gpu_wker->m_frinfo.dirty.load(), &_.value), "Cre dirty uint failed");
                _.getter = [](napi_env env, napi_callback_info info) -> napi_value //[](napi_env env, void* data) //GetDirty_NAPI; //live update
                {
                    GpuPortData* aodata;
                    napi_value argv[1], This;
                    size_t argc = SIZEOF(argv);
                    if (!env) return NULL; //Node cleanup mode?
                    struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
                    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.dirty.load();
                    !NAPI_OK(napi_create_uint32(env, aodata->wker_ok(env, SRCLINE)->m_frinfo.dirty.load(), &argv[0]), "Get uint32 getval failed");
                    return argv[0];
                };
                _.attributes = napi_enumerable; //read-only; //napi_default;
                _.data = this;
            }(*pptr++);
            [env, this](auto& _) //napi_property_descriptor& _)
            {
                _.utf8name = "times"; //NOTE: times[] will update automatically due to underlying array buf
                napi_value arybuf;
                !NAPI_OK(napi_create_external_arraybuffer(env, &gpu_wker->m_frinfo.times[0], sizeof(gpu_wker->m_frinfo.times), wker_check, NO_HINT, &arybuf), "Cre arraybuf failed");
                !NAPI_OK(napi_create_typedarray(env, napi_biguint64_array, SIZEOF(gpu_wker->m_frinfo.times), arybuf, 0, &_.value), "Cre times typed array failed");
                _.attributes = napi_enumerable; //read-only; //napi_default;
            }(*pptr++);
//add above props to frame info object:
            if (pptr - props > SIZEOF(props)) NAPI_exc("prop overflow");
            !NAPI_OK(napi_define_properties(env, frinfo.value, pptr - props, props), "set frinfo props failed");
        }
        if (frinfo.env != env) NAPI_exc("frinfo env mismatch");
        *valp = frinfo.value;
    }
#if 0
//update structured object wrapper from frame info:
//only items that could change need to be updated
    void frinfo_update(napi_env env, napi_value* valp)
    {
        frinfo_setup(env, valp);
//Protocol protocol; //= WS281X;
//const double frame_time; //msec
//const SDL_Size wh; //int NumUniv, UnivLen;
//const /*elapsed_t*/ /*std::result_of<now_msec()>::type*/ decltype(now_msec()) started;
//std::atomic</*uint32_t*/ int> numfr; //= 0; //#frames rendered / next frame#
//BkgSync<MASK_TYPE, true> dirty; //one Ready bit for each universe
//uint64_t times[NUM_STATS]; //total init/sleep (sec), render (msec), upd txtr (msec), xfr txtr (msec), present/sync (msec)
            napi_property_descriptor props[7] = {0}, *pptr = props;
//kludge: use lambas in lieu of C++ named member init:
//(named args easier to maintain than long param lists)
                _.utf8name = "protocol"; //_.name = NULL;
                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.protocol, &_.value), "Cre protocol int failed");
                _.utf8name = "numfr";
                !NAPI_OK(napi_create_int32(env, gpu_wker->m_frinfo.numfr.load(), &_.value), "Cre #fr int failed");
                _.utf8name = "dirty";
                !NAPI_OK(napi_create_uint32(env, gpu_wker->m_frinfo.dirty.load(), &_.value), "Cre dirty uint failed");
//NOTE: times[] will update automatically due to underlying array buf
//add above props to frame info object:
            !NAPI_OK(napi_define_properties(env, frinfo.value, pptr - props, props), "set frinfo props failed");
        }
        if (frinfo.env != env) NAPI_exc("frinfo env mismatch");
        *valp = frinfo.value;
    }
#endif
public: //operators:
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const GpuPortData& that) CONST
    {
//    std::ostringstream ss;
//    if (!rect) ss << "all";
//    else ss << (rect->w * rect->h) << " ([" << rect->x << ", " << rect->y << "]..[+" << rect->w << ", +" << rect->h << "])";
//    return ss.str();
        ostrm << "GpuPortData";
        if (!&that) { ostrm << " (NO DATA)"; return ostrm; }
        ostrm << "{" << sizeof(that) << ":" << &that;
        if (!that.isvalid()) ostrm << " INVALID " << that.valid1 << "/" << that.valid2;
        ostrm << ", listener: " << that.listener << ", listening? " << that.islistening();
        ostrm << ", nodes: " << that.nodes << ", frinfo: " << that.frinfo;
        if (that.wker_ok()) ostrm << ", gpdata {#fr " << that.gpu_wker->m_frinfo.numfr << "}";
        ostrm << ", GpuPort wker " << *that.gpu_wker;
        ostrm << "}";
        return ostrm;
    }
}; //GpuPortData;


//put all bkg wker execution on a consistent thread (SDL is not thread-safe):
//void bkg_wker(GpuPortData* aodata, int screen, key_t shmkey, int vgroup, GPUPORT::NODEVAL init_color)
//{
//}


#if 0 //this is overkill for singleton
class GpuPortWker_NAPI
{
public:
    static napi_value Init(napi_env env, napi_value exports);
    static void Destructor(napi_env env, void* nativeObject, void* finalize_hint);
private:
    explicit GpuPortWker_NAPI(int screen /*= FIRST_SCREEN*/, key_t shmkey = 0, int vgroup = 1, NODEVAL init_color = 0, SrcLine srcline = 0);
    ~GpuPortWker_NAPI();

    static napi_value New(napi_env env, napi_callback_info info);
    static napi_value GetValue(napi_env env, napi_callback_info info);
    static napi_value SetValue(napi_env env, napi_callback_info info);
//    static napi_value PlusOne(napi_env env, napi_callback_info info);
//    static napi_value Multiply(napi_env env, napi_callback_info info);
    static napi_ref constructor;
    double value_;
    napi_env env_;
    napi_ref wrapper_;
};
#endif


//limit brightness:
//NOTE: JS <-> C++ overhead is significant for this function
//it's probably better to just use a pure JS function for high-volume usage
napi_value Limit_NAPI(napi_env env, napi_callback_info info)
{
    GpuPortData* aodata;
    napi_value argv[1], This;
    size_t argc = SIZEOF(argv);
//    napi_status status;
  // Retrieve the JavaScript callback we should call with items generated by the
  // worker thread, and the per-addon data.
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
    if (!env) return NULL; //Node cleanup mode?
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Get cb info extract failed");
    if (argc > 1) NAPI_exc("expected color param, got " << argc << " params");
    if (!aodata->isvalid()) NAPI_exc("aodata invalid"); //doesn't matter here, but check anyway
//    if (argc < 1) 
//    napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
//    char str[1024];
//    size_t str_len;
//    status = napi_get_value_string_utf8(env, argv[0], (char *) &str, 1024, &str_len);
//    if (status != napi_ok) { napi_throw_error(env, "EINVAL", "Expected string"); return NULL; }
//    Napi::String str = Napi::String::New(env, )
    Uint32 color; //= BLACK;
    !NAPI_OK(napi_get_value_uint32(env, argv[0], &color), "Invalid uint32 arg");
//    using LIMIT = limit<pct(50/60)>; //limit brightness to 83% (50 mA/pixel instead of 60 mA); gives 15A/300 pixels, which is a good safety factor for 20A power supplies
    color = GPUPORT::limit(color); //actual work done here; surrounding code is overhead :(
    napi_value retval;
    !NAPI_OK(napi_create_uint32(env, color, &retval), "Retval failed");
    return retval;
}


#if 0
typedef struct
{
    const int UNIV_LEN; //= divup(m_cfg->vdisplay, vgroup);
    const SDL_Size wh, view; //kludge: need param to txtr ctor; CAUTION: must occur before m_txtr; //(XFRW, UNIV_LEN);
    /*txtr_bb*/ SDL_AutoTexture<GPUPORT::NODEVAL> txtr;
    const std::function<void(void*, const void*, size_t)> xfr; //memcpy signature; TODO: try to use AutoTexture<>::XFR; TODO: find out why const& no worky
    elapsed_t perf_stats[SIZEOF(txtr.perf_stats) + 2]; //2 extra counters for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
    const int screen = 0;
    const key_t shmkey = 0;
    const int vgroup = 1;
    const Uint32 init_color = BLACK;
public: //ctors/dtors:
    explicit WkerData():
        UNIV_LEN(divup(/*m_cfg? m_cfg->vdisplay: UNIV_MAX*/ ScreenInfo(screen, SRCLINE)->bounds.h, vgroup)), //univ len == display height
//        m_debug2(UNIV_LEN),
        m_wh(/*SIZEOF(bbdata[0])*/ GPUPORT::BIT_SLICES /*3 * NODEBITS*/, std::min(UNIV_LEN, GPUPORT::UNIV_MAX)), //texture (virtual window) size; kludge: need to init before passing to txtr ctor below
        m_view(/*m_wh.w*/ GPUPORT::BIT_SLICES - 1, m_wh.h), //last 1/3 bit will overlap hblank; clip from visible part of window
//        m_debug3(m_view.h),
        m_txtr(SDL_AutoTexture<GPUPORT::XFRTYPE>::create(NAMED{ _.wh = &m_wh; _.view_wh = &m_view, /*_.w_padded = XFRW_PADDED;*/ _.screen = screen; _.init_color = init_color; SRCLINE; })),
        m_xfr(std::bind(GPUPORT::xfr_bb, std::ref(*this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, SRCLINE)) //memcpy shim
//            m_protocol(protocol),
    {}
} WkerData;
#endif


//convert results from wker thread to napi and pass to JavaScript callback:
//NOTE: this executes on Node main thread only
static void Listen_cb(napi_env env, napi_value js_func, void* context, void* data)
{
    UNUSED(context);
  // Retrieve the prime from the item created by the worker thread.
//    int the_prime = *(int*)data;
    GpuPortData* aodata = static_cast<GpuPortData*>(data);
    debug(11, "call listen fats: aodata %p, valid? %d, context %p, clup mode? %d", aodata, aodata->isvalid(), context, !env);
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
//    if (!aodata->listener.busy) NAPI_exc("not listening");
    if (!aodata->islistening()) NAPI_exc("not listening");
  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
//    debug(CYAN_MSG "cb %p" ENDCOLOR, aodata);
    if (!env) return; //Node cleanup mode
//    {
//    napi_thingy retval; retval.env = env;
    napi_value argv[3];
// Convert the integer to a napi_value.
//NOTE: need to lazy-load params here because bkg wker didn't exist first time Listen_NAPI() was called
//    aodata->wker_ok(env); //) NAPI_exc(env, "Gpu wker problem: " << aodata->exc_reason());
    !NAPI_OK(napi_create_int32(env, aodata->wker_ok(env, SRCLINE)->m_frinfo.numfr, &argv[0]), "Create arg failed");
#if 0 //BROKEN
    aodata->nodes_setup(env, &argv[1]); //lazy-load data items for js
    aodata->frinfo_setup(env, &argv[2]); //lazy-load data items for js
#else
    argv[1] = aodata->nodes.value;
    argv[2] = aodata->frinfo.value;
#endif
//        !NAPI_OK(napi_get_undefined(env, &argv[1]), "Create null this failed");
  //  !NAPI_OK(napi_create_int32(env, aodata->the_prime, &argv[1]), "Create arg[1] failed"); //TODO: get rid of
// Retrieve the JavaScript `undefined` value so we can use it as the `this`
// value of the JavaScript function call.
//    !NAPI_OK(napi_get_undefined(env, &This), "Create null this failed");
// Call the JavaScript function and pass it the prime that the secondary
// thread found.
//    !NAPI_OK(napi_get_global(env, &This), "Get global failed");
//    bool want_continue;
    uint32_t ready_bits;
    napi_value retval, num_retval; //, This;
//CAUTION: seems to be some undocumented magic here: need to pass Undefined as "this" (2nd) arg here or else memory errors occur
//see "this" at https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/this#As_an_object_method
    !NAPI_OK(napi_call_function(env, aodata->listener/*This*/, js_func, SIZEOF(argv), argv, &retval), "Call JS fats failed");
//    debug(BLUE_MSG "cb: check fats retval" ENDCOLOR);
//        /*int32_t*/ bool want_continue;
    INSPECT(GREEN_MSG << "js fats this " << aodata->listener << ", svfrnum " << aodata->svfrnum << ", arg[0] " << napi_thingy(env, argv[0]) << ", arg[1] " << napi_thingy(env, argv[1]) << ", arg[2] " << napi_thingy(env, argv[2]) << ", retval " << napi_thingy(env, retval));
//static int count = 0; aodata->want_continue = (count++ < 7); return;
    !NAPI_OK(napi_coerce_to_number(env, retval, &num_retval), "Get retval as num failed");
//    debug(BLUE_MSG "cb: get bool %p" ENDCOLOR, aodata);
    !NAPI_OK(napi_get_value_uint32(env, num_retval, &ready_bits), "Get uint32 retval failed");
    debug(11, "js fats: ready bits 0x%x, continue? %d", ready_bits, !!ready_bits);
    if (!ready_bits) aodata->islistening(false);
    aodata->gpu_wker->m_frinfo.dirty |= ready_bits; //.fetch_or(ALL_UNIV || more, SRCLINE); //mark rendered universes; wake up bkg wker (even wth no univ so it will see cancel)
//    aodata->frinfo.refill(); //++aodata->gpdata.numfr; //move to next frame
//NOTE: bkg thread will respond to busy resest
//    }
//    if (!aodata->want_continue) //tell NAPI js fats will no longer be used
//        !NAPI_OK(napi_release_threadsafe_function(aodata->fats, napi_tsfn_release), "Can't release JS fats");
  // Free the item created by the worker thread.
//    free(data);
}


#if 0
// This function runs on a worker thread. It has no access to the JavaScript
// environment except through the thread-safe function.
// Limit ourselves to this many primes, starting at 2
static void ExecuteWork(napi_env env, void* data)
{
    GpuPortData* aodata = static_cast<GpuPortData*>(data);
    int idx_inner, idx_outer;
    int prime_count = 0;
  // We bracket the use of the thread-safe function by this thread by a call to
  // napi_acquire_threadsafe_function() here, and by a call to
  // napi_release_threadsafe_function() immediately prior to thread exit.
    !NAPI_OK(napi_acquire_threadsafe_function(aodata->fats), "Can't acquire JS fats");
  // Find the first 1000 prime numbers using an extremely inefficient algorithm.
//#define PRIME_COUNT 100000
//#define REPORT_EVERY 1000
//    for (idx_outer = 2; prime_count < PRIME_COUNT; idx_outer++)
//    {
//        for (idx_inner = 2; idx_inner < idx_outer; idx_inner++)
//            if (idx_outer % idx_inner == 0) break;
//        if (idx_inner < idx_outer) continue;
        // We found a prime. If it's the tenth since the last time we sent one to
        // JavaScript, send it to JavaScript.
//        if (!(++prime_count % REPORT_EVERY))
//        {
      // Save the prime number to the heap. The JavaScript marshaller (CallJs)
      // will free this item after having sent it to JavaScript.
//      int* the_prime = (int*)malloc(sizeof(*the_prime)); //cast (cpp) -dj
//      *the_prime = idx_outer;
//            aodata->the_prime = idx_outer;
      // Initiate the call into JavaScript. The call into JavaScript will not
      // have happened when this function returns, but it will be queued.
//TODO: blocking or non-blocking?
//            !NAPI_OK(napi_call_threadsafe_function(aodata->fats, data, napi_tsfn_blocking), "Can't call JS fats");
//        }
//        if (!aodata->want_continue) break;
//    }
    for (;;)
    {
//TODO: blocking or non-blocking?
//            !NAPI_OK(napi_call_threadsafe_function(aodata->fats, data, napi_tsfn_blocking), "Can't call JS fats");
//        }
//        if (!aodata->want_continue) break;
        
    }
  // Indicate that this thread will make no further use of the thread-safe function.
    !NAPI_OK(napi_release_threadsafe_function(aodata->fats, napi_tsfn_release), "Can't release JS fats");
}


// This function runs on the main thread after `ExecuteWork` exits.
static void WorkComplete(napi_env env, napi_status status, void* data)
{
    GpuPortData* aodata = static_cast<GpuPortData*>(data);
  // Clean up the thread-safe function and the work item associated with this run.
  // Set both values to NULL so JavaScript can order a new run of the thread.
    !NAPI_OK(napi_release_threadsafe_function(aodata->fats, napi_tsfn_release), "Can't release JS fats");
    aodata->fats = NULL;
    !NAPI_OK(napi_delete_async_work(env, aodata->work), "Can't delete async work");
    aodata->work = NULL;
//leave gpu port open here
}
#endif


#if 0
//void MyObject::Destructor(napi_env env, void* nativeObject, void* /*finalize_hint*/)
//{
//  reinterpret_cast<MyObject*>(nativeObject)->~MyObject();
//}
void gpdata_dtor(napi_env env, void* nativeObject, void* hint)
{
//    GpuPortData* aodata = static_cast<GpuPortData*>(hint);
//    void* wrapee; //redundant; still have ptr in aodata
//    !NAPI_OK(napi_remove_wrap(env, aodata->gpwrap, &wrapee), "Unwrap failed");
////    !NAPI_OK(napi_get_undefined(env, &aodata->gpwrap), "Create null gpwrap failed");
}
#endif


#if 0
void my_refill(napi_env env, GpuPortData* aodata, GPUPORT::TXTR* ignored)
{
//    debug(CYAN_MSG "refill: get ctx" ENDCOLOR);
//    !NAPI_OK(napi_get_threadsafe_function_context(aodata->fats, (void**)&env), "Get fats env failed"); //what to do?
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
//    if (!env) return; //Node in clean up state
//TODO: blocking or non-blocking?
//    debug(BLUE_MSG "refill: call cb" ENDCOLOR);
    !NAPI_OK(napi_call_threadsafe_function(aodata->fats, aodata, napi_tsfn_blocking), "Can't call JS fats");
    ++aodata->gpdata.numfr; //prep for next frame
}
#endif


// Create a thread-safe function and an async queue work item. We pass the
// thread-safe function to the async queue work item so the latter might have a
// chance to call into JavaScript from the worker thread on which the
// ExecuteWork callback runs.
//main entry point for this add-on:
static napi_value Listen_NAPI(napi_env env, napi_callback_info info)
{
    GpuPortData* aodata;
    napi_value argv[2], This; //*This_DONT_CARE = NULL;
    size_t argc = SIZEOF(argv);
  // Retrieve the JavaScript callback we should call with items generated by the
  // worker thread, and the per-addon data.
    if (!env) return NULL; //Node cleanup mode
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Get cb info failed");
    debug(9, CYAN_MSG "listen: aodata %p, valid? %d", aodata, aodata->isvalid());
    if ((argc < 1) || (argc > 2)) NAPI_exc("expected 1-2 args: [{opts}], cb(); got " << argc << " args");
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
    if (aodata->islistening()) NAPI_exc("Already listening (single threaded for now)"); //check if que empty
//ok    if (aodata->gpu_wker) NAPI_exc("Gpu port already open");

//create listener's "this" object for use with js callback function:
#if 0 //BROKEN; doesn't seem to be needed anyway
    if (valtype(aodata->listener.obj) == napi_undefined) //create new object if none previous
    {
        void* NO_HINT = NULL; //optional finalize_hint
//    napi_ref* NO_REF = NULL; //optional ref to wrapped object
        napi_finalize NO_FINAL = 0; //module finalize will delete add-on data; no need to do it again here
//    napi_value listener_obj;
        debug(BLUE_MSG "cre listener obj" ENDCOLOR);
        !NAPI_OK(napi_create_object(env, &aodata->listener.obj.value), "Cre obj failed");
//BROKEN:
        !NAPI_OK(napi_wrap(env, aodata->listener.obj.value, aodata, NO_FINAL, NO_HINT, &aodata->listener.ref), "Wrap listen obj failed"); //NOTE: weak ref
    }
#endif

//create thread-safe wrapper for caller's callback function:
  // Convert the callback retrieved from JavaScript into a thread-safe function
  // which we can call from a worker thread.
    const napi_value NO_RESOURCE = NULL; //optional, for init hooks
//    void* NO_FINAL_DATA = NULL;
//    napi_finalize NO_FINALIZE = NULL;
    void* NO_CONTEXT = NULL;
    const int QUE_NOMAX = 0; //no limit; TODO: should this be 1 to prevent running ahead? else requires multi-frame pixel bufs
    const int NUM_THREADS = 1; //#threads that will use caller's func (including main thread)
//    void* FINAL_DATA = NULL; //optional data for thread_finalize_cb
    napi_finalize THREAD_FINAL = NULL; //optional func to destroy tsfn
//    void* CONTEXT = NULL; //optional data to attach to tsfn
#if 0 //not needed
    napi_finalize /*std::function<void(napi_env, void*, void*)>*/ thread_final = [](napi_env env, void* data, void* hint)
    {
        UNUSED(hint);
        GpuPortData* aodata = static_cast<GpuPortData*>(data); //(GpuPortData*)data;
//    if (!env) return; //Node in clean up state
        debug(RED_MSG "thread final: aodata %p, valid? %d, hint %p" ENDCOLOR, aodata, aodata->isvalid(), hint);
        if (!aodata->isvalid()) NAPI_exc("aodata invalid");
        if (valtype(aodata->listener) != napi_null) NAPI_exc("Listener still active");
//  free(addon_data);
//not yet        delete aodata;
    };
#endif
    napi_value work_name;
    !NAPI_OK(napi_create_string_utf8(env, "GpuPort async thread-safe callback function", NAPI_AUTO_LENGTH, &work_name), "Cre wkitem desc str failed");
    !NAPI_OK(napi_create_threadsafe_function(env, argv[argc - 1], /*aodata->listener.obj.value*/ NO_RESOURCE, work_name, QUE_NOMAX, NUM_THREADS, NO_FINAL_DATA, NO_FINALIZE, NO_CONTEXT, Listen_cb, &aodata->fats), "Cre JS fats failed");
//  status = napi_set_named_property(env, obj, "msg", args[0]);

//prep caller's port params:
    int screen = FIRST_SCREEN;
    key_t PREALLOC_shmkey = 0;
    int vgroup = 1;
    GPUPORT::NODEVAL init_color = 0;
    if (argc > 1) //unpack option values
    {
//        bool has_prop;
//        napi_value propval;
//        napi_valuetype valtype;
//        !NAPI_OK(napi_typeof(env, argv[0], &valtype), "Get arg type failed");
        if (valtype(env, argv[0]) != napi_object) NAPI_exc("Expected object as first param"); //TODO: allow other types?
        !NAPI_OK(get_prop(env, argv[0], "screen", &screen), "Invalid .screen prop");
        !NAPI_OK(get_prop(env, argv[0], "shmkey", &PREALLOC_shmkey), "Invalid .shmkey prop");
        !NAPI_OK(get_prop(env, argv[0], "vgroup", &vgroup), "Invalid .vgroup prop");
        !NAPI_OK(get_prop(env, argv[0], "color", &init_color), "Invalid .color prop");
        debug(9, "listen opts: screen %d, shmkey %lx, vgroup %d, init_color 0x%x", screen, PREALLOC_shmkey, vgroup, init_color);
        if (aodata->gpu_wker) debug(RED_MSG "TODO: check for arg mismatch");
    }
    PREALLOC_shmkey = shmkey(&aodata->m_shdata);
    debug(12, YELLOW_MSG "override shmkey with prealloc key 0x%x", PREALLOC_shmkey);
//wrapped object for state info:
//    void* NO_HINT = NULL; //optional finalize_hint
//    napi_ref* NO_REF = NULL; //optional ref to wrapped object
//    !NAPI_OK(napi_wrap(env, This, aodata, addon_dtor, NO_HINT, NO_REF), "Wrap failed");
  // Create an async work item, passing in the addon data, which will give the worker thread access to the above-created thread-safe function.
#if 1
//create callback function to call above function and pass results on to bkg wker:
//    std::function<void(napi_env, GpuPortData*, GPUPORT::TXTR*)> my_refill = [](napi_env env, GpuPortData* aodata, GPUPORT::TXTR* ignored)
    GPUPORT::REFILL refill = [env, aodata](GPUPORT::TXTR* unused) //std::bind(my_refill, env, aodata, std::placeholders::_1);
    {
        DebugInOut(YELLOW_MSG "refill fr# " << aodata->gpu_wker->m_frinfo.numfr.load() << ", napi lamba (blocking)", SRCLINE);
//    debug(CYAN_MSG "refill: get ctx" ENDCOLOR);
//    !NAPI_OK(napi_get_threadsafe_function_context(aodata->fats, (void**)&env), "Get fats env failed"); //what to do?
        if (!aodata->isvalid()) NAPI_exc(env, "aodata invalid");
//    if (!env) return; //Node in clean up state
//TODO: blocking or non-blocking?
//    debug(BLUE_MSG "refill: call cb" ENDCOLOR);
//        nodes(m_wker->m_nodes), //expose as property for simpler caller usage; /*static_cast<NODEROW*>*/ &(m_nodebuf.get())[1]), //use first row for frame info
//        frinfo(m_wker->m_frinfo), // /*static_cast<FrameInfo*>*/ (m_nodebuf.get())[0]),
        aodata->gpu_wker->m_frinfo.refill(SRCLINE); //++aodata->gpdata.numfr; //move to next frame, unblock client threads if waiting
        aodata->svfrnum = aodata->gpu_wker->m_frinfo.numfr.load(); //debug
//NOTE: this will enque an async js func call, and then continue txtr xfr to GPU and wait for vsync
//this allows node rendering in main thread simultaneous with GPU blocking refreshes in bkg thread
        InOutDebug inout2("call fats (blocking)", SRCLINE);
        !NAPI_OK(napi_call_threadsafe_function(aodata->fats, aodata, napi_tsfn_blocking), aodata->listener.env, "Can't call JS fats");
//        frinfo.dirty.fetch_or(ALL_UNIV || more, SRCLINE); //mark rendered universes; bkg wker will be notified or unblock
//        aodata->frinfo.refill(); //++aodata->gpdata.numfr; //move to next frame
    };
//    GPUPORT::REFILL refill = std::bind(my_refill, env, aodata, std::placeholders::_1);
#endif

//start bkg thread for async xfr caller <-> bkg wker <-> GPU:
//    aodata->want_continue = true;
//    aodata->gpdata.numfr = 0;
//    aodata->wkr = std::thread(bkg_wker, aodata, screen, shmkey, vgroup, init_color, SRCLINE);
//    !NAPI_OK(napi_acquire_threadsafe_function(aodata->fats), "Can't acquire JS fats");
//??    aodata->islistening(true); //listener.busy = true; //(void*)1;
    std::thread bkg([aodata, screen, PREALLOC_shmkey, vgroup, init_color, refill]() //,refill,env](); //NOTE: bkg thread code should not use env
    {
        debug(9, PINK_MSG "bkg: aodata %p, valid? %d", aodata, aodata->isvalid());
//        debug(YELLOW_MSG "bkg acq" ENDCOLOR);
        !NAPI_OK(napi_acquire_threadsafe_function(aodata->fats), aodata->listener.env, "Can't acquire JS fats");
//        !NAPI_OK(napi_reference_ref(env, aodata->listener.ref, &ref_count), "Listener ref failed");
        try
        {
//open Gpu port:
//NOTE: this must be done on bkg thread for 2 reasons:
//- SDL not thread-safe; window/renderer calls must always be on same thread
//- Node main event loop must remain responsive (unblocked); some of these functions are blocking
#if 1
            aodata->gpu_wker || (aodata->gpu_wker = new GPUPORT::WKER(screen, PREALLOC_shmkey, vgroup, init_color, refill, SRCLINE));
            aodata->gpu_wker || NAPI_exc(aodata->listener.env, "Cre bkg gpu wker failed");
//        nodes(m_wker->m_nodes), //expose as property for simpler caller usage; /*static_cast<NODEROW*>*/ &(m_nodebuf.get())[1]), //use first row for frame info
//        frinfo(m_wker->m_frinfo), // /*static_cast<FrameInfo*>*/ (m_nodebuf.get())[0]),
//        NUM_UNIV(frinfo.wh.w),
//        UNIV_LEN(frinfo.wh.h),
//        protocol(frinfo.protocol),
//        m_started(now()),
//        m_srcline(srcline)
//        if (!m_wker || !&nodes || !&frinfo) exc_hard(RED_MSG "missing ptrs; wker/shmalloc failed?" ENDCOLOR_ATLINE(srcline));
//        frinfo.protocol = protocol; //allow any client to change protocol
//        m_stats.clear();
//          std::thread m_bkg(bkg_loop, screen, shmkey, vgroup, init_color, refill, NVL(srcline, SRCLINE)); //use bkg thread for GPU render to avoid blocking Node.js event loop on main thread
//            wker() = new GpuPort_wker(screen, shmkey, vgroup, /*protocol,*/ init_color, refill, NVL(srcline, SRCLINE)); //open connection to GPU
//            debug(YELLOW_MSG "hello inside" ENDCOLOR);
//            while (!excptr()) wker()->bkg_proc1req(NVL(srcline, SRCLINE)); //allow subscribers to request cancel
//    static void my_refill(TXTR* ignored, shdata* ptr) { ptr->refill(); }
#endif
//        uint32_t ref_count;
            aodata->islistening(true); //listener.busy = true; //(void*)1;
            aodata->gpu_wker->m_frinfo.reset(-1, SRCLINE); //rewind to first frame (compensate for ++ in refill) and clear stats; TODO: allow skip ahead?
//            aodata->gpu_wker->m_frinfo.numfr = -1; //kludge: setup first refill() for frane# 0
            refill(NULL); //send request for first frame; CAUTION: async call to js func
            for (;;) //int i = 0; i < 5; ++i)
            {
                DebugInOut("bkg loop fr# " << aodata->gpu_wker->m_frinfo.numfr.load(), SRCLINE);
//                debug(YELLOW_MSG "bkg send refill fr# %d" ENDCOLOR, aodata->gpu_wker->m_frinfo.numfr.load());
                if (!aodata->islistening()) break; //allow cb to break out of playback loop
//            if (aodata->excptr) break;
//            !NAPI_OK(napi_call_threadsafe_function(aodata->fats, aodata, napi_tsfn_blocking), aodata->listener.env, "Can't call JS fats");
//            m_frinfo.dirty.wait(ALL_UNIV, NVL(srcline, SRCLINE)); //wait for all universes to be rendered
//NOTE: async call to Listen_cb
                aodata->gpu_wker->bkg_proc1req(SRCLINE); //CAUTION: will block if cb not ready, and then again until vsync; that's why it's on a separte thread ;)
//            ++aodata->gpdata.numfr; //prep for next frame
//            debug(YELLOW_MSG "bkg pivot encode + xfr" ENDCOLOR);
//            SDL_Delay(0.5 sec);
//            debug(YELLOW_MSG "bkg present + wait vsync" ENDCOLOR);
//            SDL_Delay(0.5 sec);
            }
//https://stackoverflow.com/questions/233127/how-can-i-propagate-exceptions-between-threads
        }
        catch (...)
        {
            aodata->excptr = std::current_exception(); //allow main thread to rethrow
            debug(5, RED_MSG "bkg wker exc: " << aodata->exc_reason());
            aodata->islistening(false); //listener.busy = true; //(void*)1;
        }
//        debug(YELLOW_MSG "bkg release" ENDCOLOR);
//        !NAPI_OK(napi_reference_unref(env, aodata->listener.ref, &ref_count), "Listener unref failed");
        !NAPI_OK(napi_release_threadsafe_function(aodata->fats, napi_tsfn_release), aodata->listener.env, "Can't release JS fats");
        aodata->fats = NULL;
//        aodata->listener.busy = false; //work = 0;
        debug(5, YELLOW_MSG "bkg exit after %d frames", aodata->gpu_wker->m_frinfo.numfr.load());
    });
    bkg.detach();
//    while (!wker()) { debug(YELLOW_MSG "waiting for new bkg wker" ENDCOLOR); SDL_Delay(.1 sec); } //kludge: wait for bkg wker to init; TODO: use sync/wait
//no-caller has async cb
//kludge: wait for bkg wker to init; TODO: use sync/wait
//??    while (!wker()) { debug(YELLOW_MSG "waiting for new bkg wker" ENDCOLOR); SDL_Delay(.1 sec); }
#if 0
    void* NO_HINT = NULL; //optional finalize_hint
//    napi_ref* NO_REF = NULL; //optional ref to wrapped object
//    !NAPI_OK(napi_wrap(env, This, /*reinterpret_cast<void*>*/&aodata->gpdata, gpdata_dtor, aodata/*NO_HINT*/, &aodata->gpwrap), "Wrap gpdata failed");
    !NAPI_OK(napi_create_async_work(env, NO_RESOURCE, work_name, ExecuteWork, WorkComplete, aodata, &(aodata->work)), "Cre async wkitem failed");
    !NAPI_OK(napi_queue_async_work(env, aodata->work), "Enqueue async wkitem failed");
#endif
//    return NULL; //return "undefined" to JavaScript
//    napi_value retval;
//??    !NAPI_OK(napi_get_reference_value(env, aodata->aoref, &retval), "Cre retval ref failed");
    return NULL; //aodata->listener; //TODO: where to get this?
}


#if 0
// Free the per-addon-instance data.
static void addon_final(napi_env env, void* data, void* hint)
{
    UNUSED(hint);
    GpuPortData* aodata = static_cast<GpuPortData*>(data); //(GpuPortData*)data;
//    if (!env) return; //Node in clean up state
    debug(RED_MSG "napi destroy: aodata %p, valid? %d" ENDCOLOR, aodata, aodata->isvalid());
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
    if (aodata->listener.busy) NAPI_exc("Listener still active at module unload");
//  free(addon_data);
    delete aodata;
}
#endif


// The commented-out return type and the commented out formal function
// parameters below help us keep in mind the signature of the addon
// initialization function. We write the body as though the return value were as
// commented below and as though there were parameters passed in as commented
// below.
//module exports:
//NOTE: GpuPort is a singleton for now; use "exports" as object and define simple named props for exported methods
napi_value GpuModuleInit(napi_env env, napi_value exports)
{
    exports = module_exports(env, exports); //include previous exports
    uint32_t napi_ver;
    const napi_node_version* ver;
    !NAPI_OK(napi_get_node_version(env, &ver), "Get node version info failed");
    !NAPI_OK(napi_get_version(env, &napi_ver), "Get napi version info failed");
    debug(9, "using Node v" << ver->major << "." << ver->minor << "." << ver->patch << " (" << ver->release << "), napi " << napi_ver);
  // Define addon-level data associated with this instance of the addon.
//    GpuPortData* addon_data = new GpuPortData; //(GpuPortData*)malloc(sizeof(*addon_data));
    std::unique_ptr<GpuPortData> aodata(new GpuPortData(env)); //(GpuPortData*)malloc(sizeof(*addon_data));
    if (!aodata.get()->isvalid() || aodata.get()->islistening()) NAPI_exc("aodata invalid");
    // Arguments 2 and 3 are function name and length respectively
    // We will leave them as empty for this example

//expose methods for caller to use:
    napi_value fn;
    size_t NO_NAMELEN = 0;
    const char* NO_NAME = NULL;
//    void* NO_DATA = NULL;
//    if (!env) return; //Node in clean up state
    !NAPI_OK(napi_create_function(env, NO_NAME, NO_NAMELEN, Limit_NAPI, aodata.get(), &fn), "Wrap native limit() failed");
    !NAPI_OK(napi_set_named_property(env, exports, "limit", fn), "Export limit() failed");
//    status = napi_create_function(env, NULL, 0, Hello_wrapped, NULL, &fn);
//    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to wrap native function");
//    status = napi_set_named_property(env, exports, "hello", fn);
//    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to populate exports");
#if 0
//    addon_data->work = NULL;
//    addon_data->want_continue = true;
  // Define the properties that will be set on exports.
    napi_property_descriptor props[1];
    [aodata](napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "listen"; _.name = NULL;
        _.method = GpuListen_NAPI;
        _.getter = _.setter = NULL;
        _.value = NULL;
        _.attributes = napi_default;
        _.data = aodata;
    }(props[0]);
  // Decorate exports with the above-defined properties.
    !NAPI_OK(napi_define_properties(env, exports, SIZEOF(props), props), "Export GpuListen() failed");
#endif
    !NAPI_OK(napi_create_function(env, NO_NAME, NO_NAMELEN, Listen_NAPI, aodata.get(), &fn), "Wrap native listen() failed");
    !NAPI_OK(napi_set_named_property(env, exports, "listen", fn), "Export listen() failed");

//wrap internal data with module exports object:
  // Associate the addon data with the exports object, to make sure that when the addon gets unloaded our data gets freed.
//    void* NO_HINT = NULL; //optional finalize_hint
    napi_ref* NO_REF = NULL; //optional ref to wrapped object
// Free the per-addon-instance data.
    napi_finalize /*std::function<void(napi_env, void*, void*)>*/ addon_final = [](napi_env env, void* data, void* hint)
    {
        UNUSED(hint);
        GpuPortData* aodata = static_cast<GpuPortData*>(data); //(GpuPortData*)data;
//    if (!env) return; //Node cleanup mode
        debug(9, RED_MSG "addon final: aodata %p, valid? %d, hint %p", aodata, aodata->isvalid(), hint);
        if (!aodata->isvalid()) NAPI_exc("aodata invalid");
        if (aodata->islistening()) NAPI_exc("Listener still active");
        delete aodata; //free(addon_data);
    };
    !NAPI_OK(napi_wrap(env, exports, aodata.get(), addon_final, /*aodata.get()*/NO_HINT, /*&aodata->aoref*/ NO_REF), "Wrap aodata failed");
  // Return the decorated exports object.
//    napi_status status;
    debug(9, GREEN_MSG "napi init: aodata %p, valid? %d", aodata.get(), aodata.get()->isvalid());

//return exports to caller:
#if 0 //paranoid check
    std::thread bkg([env]()
    {
        SDL_Delay(0.25 sec);
        debug(YELLOW_MSG "bkg paranoid check" ENDCOLOR);
        napi_valuetype valtype;
        napi_value global, func;
        !NAPI_OK(napi_get_global(env, &global), "Get global failed");
        !NAPI_OK(napi_get_named_property(env, global, "listen", &func), "Get listen() failed");
        !NAPI_OK(napi_typeof(env, func, &valtype), "Get typeof failed");
        if (valtype != napi_function) NAPI_exc("listen() type mismatch");
//!NAPI_OK(napi_call_function(env, global, func, SIZEOF(argv), argv, &retval), "Call func() failed");
        !NAPI_OK(napi_get_named_property(env, global, "limit", &func), "Get limit() failed");
        !NAPI_OK(napi_typeof(env, func, &valtype), "Get typeof failed");
        if (valtype != napi_function) NAPI_exc("limit() type mismatch");
        debug(YELLOW_MSG "bkg paranoid exit" ENDCOLOR);
    });
    bkg.detach();
#endif
    if (!aodata.get()->isvalid()) NAPI_exc("aodata invalid");
#if 0 //don't use this
    const int INIT_COUNT = 1; //preserve after garbage collection
    aodata->listener.obj = napi_thingy(env, exports); //kludge; use for listen() "this"
    !NAPI_OK(napi_create_reference(env, aodata.get()->listener.obj.value, INIT_COUNT, &aodata.get()->listener.ref), "Create ref");
#endif
#if 1
//    void nodes_setup(napi_env env, napi_value* valp)
//        if (nodes.arytype() != napi_uint32_array) //napi_typedarray_type)
//            nodes.env = env;
    Uint32 junk[3][5];
    napi_thingy arybuf(env), nodes(env);
    debug(9, "ary buf1 " << arybuf);
    !NAPI_OK(napi_create_int32(env, 1234, &arybuf.value), "cre int32 failed");
    debug(9, "ary buf2 " << arybuf);
    !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &arybuf.value), "cre str failed");
    debug(9, "ary buf3 " << arybuf);
    !NAPI_OK(napi_create_external_arraybuffer(env, &junk[0][0], sizeof(junk), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
    debug(9, "ary buf4 " << arybuf);
    !NAPI_OK(napi_create_external_arraybuffer(env, &aodata->m_shdata.nodes[0][0], sizeof(aodata->m_shdata.nodes), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
    debug(9, "shdata arybuf " << arybuf);

    debug(9, "nodes1 " << nodes);
    !NAPI_OK(napi_create_int32(env, 1234, &nodes.value), "cre int32 failed");
    debug(9, "nodes2 " << nodes);
    !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &nodes.value), "cre str failed");
    debug(9, "nodes3 " << nodes);
    !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF_2D(junk), arybuf, 0, &nodes.value), "Cre nodes typed array failed");
    debug(9, "nodes4 " << nodes);
    !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF_2D(aodata->m_shdata.nodes), arybuf, 0, &nodes.value), "Cre nodes typed array failed");
    debug(9, "shdata nodes " << nodes);
#endif
    aodata.release(); //NAPI owns it now
    return exports;
}
// NAPI_MODULE(NODE_GYP_MODULE_NAME, ModuleInit)
 #endif //def SRC_NODE_API_H_ //USE_NAPI

 #undef module_exports
 #define module_exports  GpuModuleInit //cumulative exports
#endif //def WANT_BROKEN_CODE


////////////////////////////////////////////////////////////////////////////////
////
/// example code, hacking:
//

#ifdef WANT_SIMPLE_EXAMPLE
#include <string>

std::string hello()
{
      return "Hello World";
}

#ifdef SRC_NAPI_H_ //USE_NODE_ADDON_API
 Napi::String Hello_wrapped(const Napi::CallbackInfo& info)
 {
    Napi::Env env = info.Env();
    Napi::String returnValue = Napi::String::New(env, hello());
    return returnValue;
 }
#endif //def SRC_NAPI_H_ //USE_NODE_ADDON_API
#ifdef SRC_NODE_API_H_ //USE_NAPI
 napi_value Hello_wrapped(napi_env env, napi_callback_info info)
 {
    napi_status status;
    napi_value retval;
    status = napi__utf8(env, hello().c_str(), hello().length(), &retval);
    if (status != napi_ok) napi_throw_error(env, NULL, "Failed to parse arguments");
    return retval;
 }
#endif //def SRC_NODE_API_H_ //USE_NAPI


#ifdef SRC_NAPI_H_ //USE_NODE_ADDON_API
 Napi::Number MyFunction_wrapped(const Napi::CallbackInfo& info)
 {
    Napi::Env env = info.Env();
    if (info.Length() < 1) Napi::TypeError::New(env, "number expected").ThrowAsJavaScriptException();
    int number = info[0].As<Napi::Number>().Int32Value();
    number = number * 2;
    Napi::Number myNumber = Napi::Number::New(env, number);
    return myNumber;
 }
#endif //def SRC_NAPI_H_ //USE_NODE_ADDON_API
#ifdef SRC_NODE_API_H_ //USE_NAPI
 napi_value MyFunction_wrapped(napi_env env, napi_callback_info info)
 {
    napi_status status;

    size_t argc = 1;
    napi_value argv[1];
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) napi_throw_error(env, NULL, "Failed to parse arguments");
//    if (argc < 1) ...

//    char str[1024];
//    size_t str_len;
//    status = napi_get_value_string_utf8(env, argv[0], (char *) &str, 1024, &str_len);
//    if (status != napi_ok) { napi_throw_error(env, "EINVAL", "Expected string"); return NULL; }
//    Napi::String str = Napi::String::New(env, )

    int number = 0;
    status = napi_get_value_int32(env, argv[0], &number);
    if (status != napi_ok) napi_throw_type_error(env, NO_ERRCODE, "Invalid number was passed as argument");
    napi_value myNumber; //= 2 * number;
//    napi_value retval;
    status = napi_create_int32(env, 2 * number, &myNumber);
    if (status != napi_ok) napi_throw_error(env, NULL, "Unable to create return value");
// ...
    return myNumber;
 }
#endif //def SRC_NODE_API_H_ //USE_NAPI


#ifdef SRC_NAPI_H_ //USE_NODE_ADDON_API
 Napi::Object simple_example_Init(Napi::Env env, Napi::Object exports)
 {
    exports = module_exports(env, exports); //include previous exports
//    if (TODO) Napi::TypeError::New(env, "previous export failed").ThrowAsJavaScriptException();
    exports.Set(Napi::String::New(env, "my_function"), Napi::Function::New(env, MyFunction_wrapped));
    exports.Set(Napi::String::New(env, "hello"), Napi::Function::New(env, Hello_wrapped));
    return exports;
 }
// #ifndef WANT_CALLBACK_EXAMPLE
//  NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
// #endif
#endif //def SRC_NAPI_H_ //USE_NODE_ADDON_API
#ifdef SRC_NODE_API_H_ //USE_NAPI
 napi_value simple_example_Init(napi_env env, napi_value exports)
 {
    exports = module_exports(env, exports); //include previous exports
    napi_status status;
    napi_value fn;

    // Arguments 2 and 3 are function name and length respectively
    // We will leave them as empty for this example
    status = napi_create_function(env, NULL, 0, MyFunction_wrapped, NULL, &fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to wrap native function");
    status = napi_set_named_property(env, exports, "my_function", fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to populate exports");

    status = napi_create_function(env, NULL, 0, Hello_wrapped, NULL, &fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to wrap native function");
    status = napi_set_named_property(env, exports, "hello", fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to populate exports");
    return exports;
 }
// #ifndef WANT_CALLBACK_EXAMPLE
//  NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
// #endif
#endif //def SRC_NODE_API_H_ //USE_NAPI

 #undef module_exports
 #define module_exports  simple_example_Init //cumulative exports
#endif //def WANT_SIMPLE_EXAMPLE


#ifdef WANT_CALLBACK_EXAMPLE
#ifdef SRC_NODE_API_H_ //USE_NAPI
//async_work_thread_safe_function example from https://github.com/nodejs/node-addon-examples.git
//minor hacking -dj
 #include <assert.h>
 #include <stdlib.h>
 #include <thread> //-dj

// Limit ourselves to this many primes, starting at 2
 #define PRIME_COUNT 100000
 #define REPORT_EVERY 1000

typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn;
//more data: -dj
  int the_prime;
  bool running;
  uint32_t junk[11]; //0000];
} AddonData;

// This function is responsible for converting data coming in from the worker
// thread to napi_value items that can be passed into JavaScript, and for
// calling the JavaScript function.
static void CallJs(napi_env env, napi_value js_cb, void* context, void* data) {
  // This parameter is not used.
  (void) context;

  // Retrieve the prime from the item created by the worker thread.
  int the_prime = ((AddonData*)data)->the_prime; //*(int*)data; -dj

  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
  if (!((AddonData*)data)->running) printf("previously cancelled at prime %d\n", the_prime);
  else if (env != NULL) {
    napi_value undefined, argv[2]; //js_the_prime;

    // Convert the integer to a napi_value.
    assert(napi_create_int32(env, the_prime, &argv[0] /*js_the_prime*/) == napi_ok);

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    assert(napi_get_undefined(env, &undefined) == napi_ok);

//    assert(napi_get_null(env, &argv[1]) == napi_ok);
    napi_value arybuf;
//    void* NO_HINT = NULL; //optional finalize_hint
//    napi_finalize NO_FINAL = NULL;
    assert(napi_create_external_arraybuffer(env, ((AddonData*)data)->junk, sizeof(((AddonData*)data)->junk), NO_FINALIZE, NO_HINT, &arybuf) == napi_ok);
    assert(napi_create_typedarray(env, GPU_NODE_type, SIZEOF(((AddonData*)data)->junk), arybuf, 0, &argv[1]) == napi_ok);
    debug(0, YELLOW_MSG "cb arybuf " << napi_thingy(env, arybuf) << ", typed ary " << napi_thingy(env, argv[1]));

    // Call the JavaScript function and pass it the prime that the secondary
    // thread found.
    bool boolval;
    napi_value retval, bool_retval; //check ret val -dj
    assert(napi_call_function(env,
                              undefined,
                              js_cb,
                              SIZEOF(argv), //1,
                              argv, //&js_the_prime,
                              &retval /*NULL*/) == napi_ok);
//check ret val: -dj
    assert(napi_coerce_to_bool(env, retval, &bool_retval) == napi_ok);
    assert(napi_get_value_bool/*int32*/(env, bool_retval, &boolval) == napi_ok);
    printf("js cb returned %d to napi, addon data %p\n", boolval, data);
    ((AddonData*)data)->running = boolval;
  }

  // Free the item created by the worker thread.
//  free(data); -dj
}

// This function runs on a worker thread. It has no access to the JavaScript
// environment except through the thread-safe function.
static void ExecuteWork(napi_env env, void* data) {
  AddonData* addon_data = (AddonData*)data;
  int idx_inner, idx_outer;
  int prime_count = 0;

  // We bracket the use of the thread-safe function by this thread by a call to
  // napi_acquire_threadsafe_function() here, and by a call to
  // napi_release_threadsafe_function() immediately prior to thread exit.
  assert(napi_acquire_threadsafe_function(addon_data->tsfn) == napi_ok);

  // Find the first 1000 prime numbers using an extremely inefficient algorithm.
  for (idx_outer = 2; prime_count < PRIME_COUNT; idx_outer++) {
    for (idx_inner = 2; idx_inner < idx_outer; idx_inner++) {
      if (idx_outer % idx_inner == 0) {
        break;
      }
    }
    if (idx_inner < idx_outer) {
      continue;
    }

    // We found a prime. If it's the tenth since the last time we sent one to
    // JavaScript, send it to JavaScript.
    if (!(++prime_count % REPORT_EVERY)) {

      // Save the prime number to the heap. The JavaScript marshaller (CallJs)
      // will free this item after having sent it to JavaScript.
//      int* the_prime = (int*)malloc(sizeof(*the_prime)); //cast (cpp) -dj
//      *the_prime = idx_outer;
      addon_data->the_prime = idx_outer; //just store in addon struct -dj

      // Initiate the call into JavaScript. The call into JavaScript will not
      // have happened when this function returns, but it will be queued.
      assert(napi_call_threadsafe_function(addon_data->tsfn,
                                           data, //the_prime, -pass whole struct -dj
                                           napi_tsfn_blocking) == napi_ok);
      printf("cb for %d wants to exit? %d\n", idx_outer, !addon_data->running);
      if (!addon_data->running) { prime_count = PRIME_COUNT; break; } //callee exit -dj
     }
  }

  // Indicate that this thread will make no further use of the thread-safe function.
  assert(napi_release_threadsafe_function(addon_data->tsfn,
                                          napi_tsfn_release) == napi_ok);
}

// This function runs on the main thread after `ExecuteWork` exits.
static void WorkComplete(napi_env env, napi_status status, void* data) {
  AddonData* addon_data = (AddonData*)data;

  // Clean up the thread-safe function and the work item associated with this
  // run.
  assert(napi_release_threadsafe_function(addon_data->tsfn,
                                          napi_tsfn_release) == napi_ok);
  if (addon_data->work) //no longer used -dj
  assert(napi_delete_async_work(env, addon_data->work) == napi_ok);

  // Set both values to NULL so JavaScript can order a new run of the thread.
  addon_data->work = NULL;
  addon_data->tsfn = NULL;
}

// Create a thread-safe function and an async queue work item. We pass the
// thread-safe function to the async queue work item so the latter might have a
// chance to call into JavaScript from the worker thread on which the
// ExecuteWork callback runs.
static napi_value StartThread(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value js_cb, work_name;
  AddonData* addon_data;

  // Retrieve the JavaScript callback we should call with items generated by the
  // worker thread, and the per-addon data.
  assert(napi_get_cb_info(env,
                          info,
                          &argc,
                          &js_cb,
                          NULL,
                          (void**)(&addon_data)) == napi_ok);

  // Ensure that no work is currently in progress.
  assert(addon_data->work == NULL && "Only one work item must exist at a time");

  // Create a string to describe this asynchronous operation.
  assert(napi_create_string_utf8(env,
                                 "N-API Thread-safe Call from Async Work Item",
                                 NAPI_AUTO_LENGTH,
                                 &work_name) == napi_ok);

  // Convert the callback retrieved from JavaScript into a thread-safe function
  // which we can call from a worker thread.
  assert(napi_create_threadsafe_function(env,
                                         js_cb,
                                         NULL,
                                         work_name,
                                         0,
                                         1,
                                         NULL,
                                         NULL,
                                         NULL,
                                         CallJs,
                                         &(addon_data->tsfn)) == napi_ok);

#if 0 //original code
  // Create an async work item, passing in the addon data, which will give the
  // worker thread access to the above-created thread-safe function.
  assert(napi_create_async_work(env,
                                NULL,
                                work_name,
                                ExecuteWork,
                                WorkComplete,
                                addon_data,
                                &(addon_data->work)) == napi_ok);

  // Queue the work item for execution.
  assert(napi_queue_async_work(env, addon_data->work) == napi_ok);
#else //simpler to use std::thread and lamba -dj
    addon_data->running = true;
    std::thread bkg([addon_data, env]()
    {
        napi_status status;
        ExecuteWork(env, addon_data);
        WorkComplete(env, status, addon_data);
    });
    bkg.detach();
#endif

  // This causes `undefined` to be returned to JavaScript.
  return NULL;
}

// Free the per-addon-instance data.
static void addon_getting_unloaded(napi_env env, void* data, void* hint) {
  AddonData* addon_data = (AddonData*)data;
  assert(addon_data->work == NULL &&
      "No work item in progress at module unload");
  free(addon_data);
}

// The commented-out return type and the commented out formal function
// parameters below help us keep in mind the signature of the addon
// initialization function. We write the body as though the return value were as
// commented below and as though there were parameters passed in as commented
// below.
// /*napi_value*/ NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
napi_value callback_example_Init(napi_env env, napi_value exports)
{
    exports = module_exports(env, exports); //add previous exports

  // Define addon-level data associated with this instance of the addon.
  AddonData* addon_data = (AddonData*)malloc(sizeof(*addon_data));
  addon_data->work = NULL;
  for (int i = 0; i < SIZEOF(addon_data->junk); ++i) addon_data->junk[i] = i * 100 + 1;

#if 0 //alternate way -dj
  // Define the properties that will be set on exports.
  napi_property_descriptor start_work = {
    "startThread",
    NULL,
    StartThread,
    NULL,
    NULL,
    NULL,
    napi_default,
    addon_data
  };

  // Decorate exports with the above-defined properties.
  assert(napi_define_properties(env, exports, 1, &start_work) == napi_ok);
#else
    napi_value fn;
    assert(napi_create_function(env, NULL, 0, StartThread, addon_data, &fn) == napi_ok);
    assert(napi_set_named_property(env, exports, "startThread", fn) == napi_ok);
#endif

  // Associate the addon data with the exports object, to make sure that when
  // the addon gets unloaded our data gets freed.
  assert(napi_wrap(env,
                   exports,
                   addon_data,
                   addon_getting_unloaded,
                   NULL,
                   NULL) == napi_ok);

  // Return the decorated exports object.
//#ifdef WANT_SIMPLE_EXAMPLE
//  Init(env, exports); //include earlier example code also -dj
//#endif
  return exports;
}
#endif //def SRC_NODE_API_H_ //USE_NAPI

 #undef module_exports
 #define module_exports  callback_example_Init //cumulative exports
#endif //def WANT_CALLBACK_EXAMPLE





////////////////////////////////////////////////////////////////////////////////
////
/// Setup
//

//1. Dependencies:
//1a. install nvm
//1b. install node v10.12.0  #or later; older versions might work; ymmv
//1c. install github client
//1d. install SDL2 >=2.0.8 from libsdl.org; install from source recommended (repo versions tend to be older)
//2. Create parent project
//2a. create a folder + cd into it
//2b. npm init
//2c. npm install --save gpuport
//OR
//2a. git clone this repo
//2b. cd this folder
//2c. npm install
//2d. npm test  #dev mode test, optional

//initial construction:
//N-api example at: //https://medium.com/@atulanand94/beginners-guide-to-writing-nodejs-addons-using-c-and-n-api-node-addon-api-9b3b718a9a7f
//npm install node-gyp, node-addon-api


//1d. upgrade node-gyp >= 3.6.2
//     [sudo] npm explore npm -g -- npm install node-gyp@latest
//3. config:
//3a. edit /boot/config.txt, vdi overlay + screen res, disable_overscan=1 ?
#if 0
overscan_left=24
overscan_right=24
Overscan_top=10
Overscan_bottom=24

Framebuffer_width=480
Framebuffer_height=320

Sdtv_mode=2
Sdtv_aspect=2

resolution 82   1920x1080   60Hz    1080p

hdmi_ignore_edid=0xa5000080
hdmi_force_hotplug=1
hdmi_boost=7
hdmi_group=2
hdmi_mode=82
hdmi_drive=1
#endif
//3b. give RPi GPU 256 MB RAM (optional)


//WS281X notes:
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


////////////////////////////////////////////////////////////////////////////////
////
/// Headers, general macros, inline functions
//

#if 0
//#include <iostream> //std::cout
#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
#include <sstream> //std::ostringstream

#include "debugexc.h"
#include "msgcolors.h"
#include "elapsed.h" //timestamp()
#include "GpuPort.h"
#endif

//#define LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA; safely allows 300 LEDs per 20A at "full" (83%) white

//#define rdiv(n, d)  int(((n) + ((d) >> 1)) / (d))
//#define divup(n, d)  int(((n) + (d) - 1) / (d))

//#define SIZE(thing)  int(sizeof(thing) / sizeof((thing)[0]))

//#define return_void(expr) { expr; return; } //kludge: avoid warnings about returning void value

//inline int toint(void* val) { return (int)(long)val; }
//#define toint(ptr)  reinterpret_cast<int>(ptr) //gives "loses precision" warning/error
//#define toint(expr)  (int)(long)(expr)

//#define CONST  //should be const but function signatures aren't defined that way

//typedef enum {No = false, Yes = true, Maybe} tristate;
//enum class tristate: int {No = false, Yes = true, Maybe, Error = Maybe};

//#define uint24_t  uint32_t //kludge: use pre-defined type and just ignore first byte

//kludge: need nested macros to stringize correctly:
//https://stackoverflow.com/questions/2849832/c-c-line-number
//#define TOSTR(str)  TOSTR_NESTED(str)
//#define TOSTR_NESTED(str)  #str

//#define CONCAT(first, second)  CONCAT_NESTED(first, second)
//#define CONCAT_NESTED(first, second)  first ## second


#if 0
//show GpuPort stats:
template <typename GPTYPE>
void gpstats(const char* desc, GPTYPE&& gp, int often = 1)
{
    bool result = gp.ready(0xffffff, SRCLINE); //mark all univ ready to display
    static int count = 0;
    if (++count % often) return;
//see example at https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/
    for (;;)
    {
        /*uint32_t*/ auto numfr = gp.frinfo.numfr.load();
//        /*uint32_t*/ auto next_time = gp.frinfo.nexttime.load();
        double next_time = numfr * gp.frame_time;
        /*uint64_t*/ double times[SIZEOF(gp.frinfo.times)];
        for (int i = 0; i < SIZEOF(times); ++i) times[i] = gp.frinfo.times[i] / numfr / 1e3; //avg msec
        if (gp.frinfo.numfr.load() != numfr) continue; //invalid (inconsistent) results; try again
//CAUTION: use atomic ops; std::forward doesn't allow atomic vars (deleted function errors for copy ctor) so use load()
        debug(CYAN_MSG << timestamp() << "%s, next fr# %d, next time %f, ready? %d, avg perf: [%4.3f s, %4.3f ms, %4.3f s, %4.3f ms, %4.3f ms, %4.3f ms]" ENDCOLOR,
            desc, numfr, next_time, result, times[0] / 1e3, times[1], times[2], times[3], times[4], times[5]);
        break; //got valid results
    }
}


//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    Uint32 tb1 = 0xff80ff, tb2 = 0xffddbb;
    debug(BLUE_MSG "limit blue 0x%x, cyan 0x%x, white 0x%x, 0x%x -> 0x%x, 0x%x -> 0x%x" ENDCOLOR, GpuPort<>::limit(BLUE), GpuPort<>::limit(CYAN), GpuPort<>::limit(WHITE), tb1, GpuPort<>::limit(tb1), tb2, GpuPort<>::limit(tb2));

//    bind_test(); return;
    int screen = 0, vgroup = 0, delay_msec = 100; //default
//    for (auto& arg : args) //int i = 0; i < args.size(); ++i)
    /*GpuPort<>::Protocol*/ int protocol = GpuPort<>::Protocol::NONE;
    for (auto it = ++args.begin(); it != args.end(); ++it)
    {
        auto& arg = *it;
        if (!arg.find("-s")) { screen = atoi(arg.substr(2).c_str()); continue; }
        if (!arg.find("-g")) { vgroup = atoi(arg.substr(2).c_str()); continue; }
        if (!arg.find("-p")) { protocol = atoi(arg.substr(2).c_str()); continue; }
        if (!arg.find("-d")) { delay_msec = atoi(arg.substr(2).c_str()); continue; }
        debug(YELLOW_MSG "what is arg[%d] '%s'?" ENDCOLOR, /*&arg*/ it - args.begin(), arg.c_str());
    }
    const auto* scrn = getScreenConfig(screen, SRCLINE);
    if (!vgroup) vgroup = scrn->vdisplay / 5;
//    const int NUM_UNIV = 4, UNIV_LEN = 5; //24, 1111
//    const int NUM_UNIV = 30, UNIV_LEN = 100; //24, 1111
//    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //, dimARGB(0.75, WHITE), dimARGB(0.25, WHITE)};

//    SDL_Size wh(4, 4);
//    GpuPort<NUM_UNIV, UNIV_LEN/*, raw WS281X*/> gp(2.4 MHz, 0, SRCLINE);
    GpuPort<> gp/* = GpuPort<>::factory*/(NAMED{ _.screen = screen; _.vgroup = vgroup; /*_.wh = wh*/; _.protocol = static_cast<GpuPort<>::Protocol>(protocol) /*GpuPort<>::NONE WS281X*/; _.init_color = dimARGB(0.25, RED); SRCLINE; }); //NAMED{ .num_univ = NUM_UNIV, _.univ_len = UNIV_LEN, SRCLINE});
//    Uint32* pixels = gp.Shmbuf();
//    Uint32 myPixels[H][W]; //TODO: [W][H]; //NOTE: put nodes in same universe adjacent for better cache performance
//    auto& pixels = gp.pixels; //NOTE: this is shared memory, so any process can update it
//    debug(CYAN_MSG "&nodes[0] %p, &nodes[0][0] = %p, sizeof gp %zu, sizeof nodes %zu" ENDCOLOR, &gp.nodes[0], &gp.nodes[0][0], sizeof(gp), sizeof(gp.nodes));
    debug(CYAN_MSG << gp << ENDCOLOR);
    SDL_Delay((5-1) sec);

//    void* addr = &gp.nodes[0][0];
//    debug(YELLOW_MSG "&node[0][0] " << addr << ENDCOLOR);
//    addr = &gp.nodes[1][0];
//    debug(YELLOW_MSG "&node[1][0] " << addr << ENDCOLOR);
//    addr = &gp.nodes[0][gp.UNIV_LEN];
//    debug(YELLOW_MSG "&node[0][%d] " << addr << ENDCOLOR, gp.UNIV_LEN);
//    addr = &gp.nodes[24][0];
//    debug(YELLOW_MSG "&node[24][0] " << addr << ENDCOLOR);

    gp.clear_stats(); //frinfo.numfr.store(0); //reset frame count
//    txtr.perftime(); //kludge: flush perf timer
    VOID SDL_Delay(1 sec); //kludge: even out timer with loop
    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        VOID gp.fill(palette[c], NO_RECT, SRCLINE);
        std::ostringstream desc;
        desc << "all-color 0x" << std::hex << palette[c] << std::dec;
        VOID gpstats(desc.str().c_str(), gp, 100); //SIZEOF(palette) - 1);
        bool result = gp.ready(0xffffff, SRCLINE); //mark all univ ready to display
        SDL_Delay(1 sec);
        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
    }
    VOID gpstats("all-pixel test", gp);

#define BKG  dimARGB(0.25, CYAN) //BLACK
//    VOID gp.fill(dimARGB(0.25, CYAN), NO_RECT, SRCLINE);
    gp.clear_stats(); //frinfo.numfr.store(0); //reset frame count
//    txtr.perftime(); //kludge: flush perf timer
    if (delay_msec) VOID SDL_Delay(delay_msec); //0.1 sec); //kludge: even out timer with loop
    for (int y = 0, c = 0; y < gp.UnivLen; ++y)
        for (int x = 0; x < gp.NumUniv; x += 4, ++c)
        {
            VOID gp.fill(BKG, NO_RECT, SRCLINE); //use up some CPU time
            Uint32 color = palette[c % SIZEOF(palette)]; //dimARGB(0.25, PINK);
            gp.nodes[x + 3][y] = gp.nodes[x + 2][y] = gp.nodes[x + 1][y] =
            gp.nodes[x][y] = color;
            std::ostringstream desc;
            desc << "node[" << x << "," << y << "] <- 0x" << std::hex << color << std::dec;
            VOID gpstats(desc.str().c_str(), gp, 100);
            if (delay_msec) SDL_Delay(delay_msec); //0.1 sec);
            if (SDL_QuitRequested()) { y = gp.UnivLen; x = gp.NumUniv; break; } //Ctrl+C or window close enqueued
        }
    VOID gpstats("4x pixel text", gp);
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
#endif

#if 0
class GpuPort_napi
{
 public:
  static napi_value Init(napi_env env, napi_value exports);
  static void Destructor(napi_env env, void* nativeObject, void* finalize_hint);

 private:
  explicit MyObject(double value_ = 0);
  ~MyObject();

  static napi_value New(napi_env env, napi_callback_info info);
  static napi_value GetValue(napi_env env, napi_callback_info info);
  static napi_value SetValue(napi_env env, napi_callback_info info);
  static napi_value PlusOne(napi_env env, napi_callback_info info);
  static napi_value Multiply(napi_env env, napi_callback_info info);
  static napi_ref constructor;
  double value_;
  napi_env env_;
  napi_ref wrapper_;
};
#endif


#if 0
int main(int argc, const char* argv[])
{
//    MSG("testing " __TEST_FILE__ " ...");
    ARGS args;
    for (int i = 0; i < argc; ++i) args.push_back(std::string(argv[i]));
    unit_test(args);
    return 0;
}
#endif

NAPI_MODULE(NODE_GYP_MODULE_NAME, module_exports) //cumulative exports; put at end to export everything defined above
//eof
