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
//npm install --verbose  -or-  npm run build  -or-  node-gyp rebuild --verbose
//NOTE: if errors, try manually:  "npm install nan@latest --save"  -or-  npm install -g node-gyp
//or try uninstall/re-install node:
//https://stackoverflow.com/questions/11177954/how-do-i-completely-uninstall-node-js-and-reinstall-from-beginning-mac-os-x
//$node --version
//$nvm deactivate
//$nvm uninstall v11.1.0

//example/setup info:
//** https://github.com/nodejs/node-addon-examples
//https://github.com/1995parham/Napi101
//https://www.nearform.com/blog/the-future-of-native-amodules-in-node-js/
//https://hackernoon.com/n-api-and-getting-started-with-writing-c-addons-for-node-js-cf061b3eae75
//or https://github.com/nodejs/node-addon-examples/tree/master/1_hello_world
//or https://medium.com/@atulanand94/beginners-guide-to-writing-nodejs-addons-using-c-and-n-api-node-addon-api-9b3b718a9a7f
//https://github.com/master-atul/blog-addons-example


//which Node API to use?
//V8 is older, requires more familiarity with V8
//NAPI is C-style api and works ok
//Node Addon API is C++ style but seems to have some issues
//use NAPI for now
#define USE_NAPI
//#define USE_NODE_ADDON_API //TODO: convert to newer API
//#define USE_NAN
//NAPI perf notes: https://github.com/nodejs/node/issues/14379

#define WANT_REAL_CODE
//#define WANT_SIMPLE_EXAMPLE
//#define WANT_CALLBACK_EXAMPLE


#if defined(USE_NAPI)
 #define NAPI_EXPERIMENTAL //NOTE: need this to avoid compile errors; need v10.6.0 or later
 #include <node_api.h> //C style api; https://nodejs.org/api/n-api.html
#elif defined(USE_NODE_ADDON_API)
 #include "napi.h" //C++ style api; #includes node_api.h
#elif defined(USE_NAN)
 #include <nan.h> //older V8 api
#else
 #error Use which Node api?
#endif

#include <sstream> //std::ostringstream
#include <utility> //std::forward<>
#include <string>
//#include <map> //std::map<>
#include "str-helpers.h" //unmap()
#include "thr-helpers.h" //thrnx()
#include "msgcolors.h"

#if __cplusplus < 201103L
 #pragma message("CAUTION: this file probably needs c++11 or later to compile correctly")
#endif


#define UNUSED(thing)  //(void)thing

//accept variable # up to 1 - 2 macro args:
#ifndef UPTO_2ARGS
 #define UPTO_2ARGS(skip1, skip2, use3, ...)  use3
#endif
#ifndef UPTO_3ARGS
 #define UPTO_3ARGS(skip1, skip2, skip3, use4, ...)  use4
#endif
#ifndef UPTO_4ARGS
 #define UPTO_4ARGS(skip1, skip2, skip3, skip4, use5, ...)  use5
#endif


////////////////////////////////////////////////////////////////////////////////
////
/// NAPI helpers, wrappers to GpuPort functions:
//

//remember last error (mainly for debug msgs):
//CAUTION: shared between threads
#define DEF_ENV  env //assume env is named "env"
#define NO_ERRCODE  NULL
napi_status NAPI_LastStatus = napi_ok;
//?? napi_get_and_clear_last_exception(napi_env env, napi_value* result);

//get error message info for latest error:
std::string NAPI_ErrorMessage(napi_env env)
{
    std::ostringstream ss;
    const napi_extended_error_info* errinfo;
    if (napi_get_last_error_info(env, &errinfo) != napi_ok) ss << "Can't get error " << NAPI_LastStatus << " info!";
    else ss << errinfo->error_message << " (errcode " << errinfo->error_code << ", status " << NAPI_LastStatus << ", thr# " << thrinx() << ")";
    return ss.str(); //NOTE: returns stack var by value, not by ref
}

//TODO: add SRCLINE
#define NAPI_exc_1ARG(msg)  NAPI_exc_2ARGS(DEF_ENV, msg)
#define NAPI_exc_2ARGS(env, msg)  NAPI_exc_3ARGS(env, NO_ERRCODE, msg)
#define NAPI_exc_3ARGS(env, errcode, msg)  (napi_throw_error(env, errcode, std::ostringstream() << RED_MSG << msg << ": " << NAPI_ErrorMessage(env) << ENDCOLOR), false) //dummy "!okay" result to allow usage in conditional expr; throw() won't fall thru at run-time, though
#define NAPI_exc(...)  UPTO_3ARGS(__VA_ARGS__, NAPI_exc_3ARGS, NAPI_exc_2ARGS, NAPI_exc_1ARG) (__VA_ARGS__)
inline napi_status napi_throw_error(napi_env env, const char* errcode, std::/*ostringstream*/ostream& errmsg)
{
    return napi_throw_error(env, errcode, static_cast<std::ostringstream&>(errmsg).str().c_str());
}
//inline bool NAPI_OK(napi_status retval) { return ((NAPI_LastError = retval) == napi_ok); }
#define NAPI_OK_1ARG(retval)  ((NAPI_LastStatus = (retval)) == napi_ok)
#define NAPI_OK_2ARGS(retval, errmsg)  NAPI_OK_3ARGS(retval, DEF_ENV, errmsg)
#define NAPI_OK_3ARGS(retval, env, errmsg)  NAPI_OK_4ARGS(retval, env, NO_ERRCODE, errmsg)
#define NAPI_OK_4ARGS(retval, env, errcode, errmsg)  (NAPI_OK_1ARG(retval) || NAPI_exc(env, errcode, errmsg))
#define NAPI_OK(...)  UPTO_4ARGS(__VA_ARGS__, NAPI_OK_4ARGS, NAPI_OK_3ARGS, NAPI_OK_2ARGS, NAPI_OK_1ARG) (__VA_ARGS__)
//NO-use overloaded functions to allow usage in conditional stmts by caller (napi_throw_error doesn't return a bool)
//inline bool NAPI_OK(napi_status retval)  { return ((NAPI_LastError = (retval)) == napi_ok); }
//inline bool NAPI_OK(napi_status retval, const char* errmsg)  { if (NAPI_OK(retval)) , DEF_ENV, errmsg)
//#define NAPI_OK_3ARGS(retval, env, errmsg)  NAPI_OK_4ARGS(retval, env, NO_ERRCODE, errmsg)
//#define NAPI_OK_4ARGS(retval, env, errcode, errmsg)  if (!NAPI_OK_1ARG(retval)) NAPI_exc(env, errcode, errmsg)
//allow napi_throw_error() to be used as bool (false) return value:
//template <typename ... ARGS>
//BROKEN; just use macros instead
//inline bool napi_throw_error_false(ARGS&& ... args)
//{
//    napi_throw_error(std::forward<ARGS>(args) ...); //perfect fwd
//    return false; //dummy "okay" result to satisfy compiler (throw() won't fall thru)
//}


napi_valuetype valtype(napi_env env, napi_value value)
{
    napi_valuetype valtype;
    !NAPI_OK(napi_typeof(env, value, &valtype), "Get val type failed");
    return valtype;
}


//template <typename napi_status (*getval)
napi_status get_prop(napi_env env, napi_value obj, const char* name, bool* has_prop, napi_value* valp)
{
//    bool has_prop;
//    napi_value prop_val;
    if (!NAPI_OK(napi_has_named_property(env, obj, name, has_prop)) || !*has_prop) return NAPI_LastStatus;
    return napi_get_named_property(env, obj, name, valp);
}
//type-specific overloads:
napi_status get_prop(napi_env env, napi_value obj, const char* name, int32_t* valp)
{
    bool has_prop;
    napi_value prop_val;
    if (!NAPI_OK(get_prop(env, obj, name, &has_prop, &prop_val)) || !has_prop) return NAPI_LastStatus;
    return napi_get_value_int32(env, prop_val, valp);
}
napi_status get_prop(napi_env env, napi_value obj, const char* name, uint32_t* valp)
{
    bool has_prop;
    napi_value prop_val;
    if (!NAPI_OK(get_prop(env, obj, name, &has_prop, &prop_val)) || !has_prop) return NAPI_LastStatus;
    return napi_get_value_uint32(env, prop_val, valp);
}


static inline const char* TypeName(napi_valuetype key)
{
    static const std::map<napi_valuetype, const char*> names =
    {
        {napi_undefined, "undef"},
        {napi_null, "null"},
        {napi_boolean, "bool"},
        {napi_string, "string"},
        {napi_number, "number"},
//TODO if needed:
//napi_symbol,
//napi_object,
//napi_function,
//napi_external,
//napi_bigint,
    };
    return unmap(names, key); //names;
}
static inline const char* TypeName(napi_typedarray_type key)
{
    static const std::map<napi_typedarray_type, const char*> names =
    {
        {napi_uint32_array, "uint32 ary"},
//TODO if needed:
//other typedarray types
    };
    return unmap(names, key); //names;
}


//kludge: composite object for operator<< specialization:
struct napi_thingy
{
    napi_env env;
    napi_value value;
public: //ctors/dtors
//    explicit napi_thingy() {}
    napi_thingy() = delete; //default ctor is useless without env
    explicit inline napi_thingy(napi_env new_env): env(new_env) { reset(); }
    inline napi_thingy(napi_env new_env, napi_value new_value): env(new_env), value(new_value) {}
    inline ~napi_thingy() { reset(); } //reset in case caller added refs to other objects; probably not needed
public: //operators
    inline operator napi_env() const { return env; }
    inline operator napi_value() const { return value; }
    inline napi_valuetype type() const { return valtype(env, value); }
    napi_typedarray_type arytype() const
    {
        void* data;
        napi_value arybuf;
        size_t arylen, bofs;
        napi_typedarray_type arytype;
        !NAPI_OK(napi_get_typedarray_info(env, value, &arytype, &arylen, &data, &arybuf, &bofs), "Get typed array info failed");
        return arytype;
    }
//    napi_status napi_get_dataview_info(napi_env env,
//                                   napi_value dataview,
//                                   size_t* byte_length,
//                                   void** data,
//                                   napi_value* arraybuffer,
//                                   size_t* byte_offset)
//for debug or error msgs:
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const napi_thingy& napval) CONST
    {
        bool bool_val;
        size_t str_len;
        char str_val[200];
        double float_val;
        switch (napval.type())
        {
            case napi_boolean:
                !NAPI_OK(napi_get_value_bool(napval.env, napval.value, &bool_val), napval.env, "Get bool value failed");
                ostrm << "napi bool " << bool_val;
                break;
            case napi_string:
                !NAPI_OK(napi_get_value_string_utf8(napval.env, napval.value, str_val, sizeof(str_val) - 1, &str_len), napval.env, "Get string value failed");
                str_val[str_len] = '\0';
                ostrm << "napi str " << str_len << ": '" << str_val << "'";
                break;
            case napi_number:
                !NAPI_OK(napi_get_value_double(napval.env, napval.value, &float_val), napval.env, "Get number value failed");
                ostrm << "napi number " << float_val;
                break;
//valueless types:
            case napi_null:
            case napi_undefined:
                ostrm << "napi " << NVL(TypeName(napval.type()), "??TYPE??");
                break;
//TODO if needed:
//napi_symbol,
//napi_object,
//napi_function,
//napi_external,
//napi_bigint,
            default: ostrm << "napi: type " << napval.type(); break;
        }
        return ostrm;
    }
public: //methods
    inline void reset() { !NAPI_OK(napi_get_undefined(env, &value), "Get undef failed"); }
};
//napi_thingy;


#include "debugexc.h"
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
    /*bool BKG_THREAD =*/ false //true //taking explicit control of bkg wker below so not needed
//    int NODEBITS = 24> //# bits to send for each WS281X node (protocol dependent)
>;
static const napi_typedarray_type GPU_NODE_type = napi_uint32_array; //NOTE: must match GPUPORT NODEVAL type


#ifdef WANT_REAL_CODE
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

struct AddonData
{
    const int valid1 = VALID;
    static const int VALID = 0x1234beef;
//    int frnum;
//    int the_prime;
    GPUPORT::WKER* gpu_wker = 0;
//    struct { int numfr; } gpdata; //TODO
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
    explicit AddonData(napi_env env):
        nodes(env), frinfo(env), listener(env) //NOTE: need these because default ctor deleted
    {
        islistening(false); //listener.busy = false;
        INSPECT(CYAN_MSG "ctor " << *this);
    }
    ~AddonData()
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
    std::string exc_reason(const char* no_reason = 0)
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
        AddonData* aodata = static_cast<AddonData*>(data);
        debug(RED_MSG "thread final: aodata %p, valid? %d, hint %p" ENDCOLOR, aodata, aodata->isvalid(), hint);
//    if (!env) return; //Node clean up mode
        if (!aodata->wker_ok()) NAPI_exc("Gpu wker problem: " << aodata->exc_reason());
    }
//lazy data setup for js:
//can't be set up before gpu_wker is created
    void nodes_setup(napi_env env, napi_value* valp)
    {
        if (nodes.arytype() != napi_uint32_array) //napi_typedarray_type)
        {
            nodes.env = env;
            napi_value arybuf;
            void* NO_HINT = NULL; //optional finalize_hint
            !NAPI_OK(napi_create_external_arraybuffer(env, &gpu_wker->m_nodes[0][0], sizeof(gpu_wker->m_nodes), wker_check, NO_HINT, &arybuf), "Cre arraybuf failed");
            !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF(gpu_wker->m_nodes), arybuf, 0, &nodes.value), "Cre nodes typed array failed");
            debug(YELLOW_MSG "nodes typed array created: &node[0][0] " << &gpu_wker->m_nodes[0][0] << ", " << commas(SIZEOF(gpu_wker->m_nodes)) << " " << NVL(TypeName(GPU_NODE_type)) << " elements, napi thingy " << nodes << ENDCOLOR);
        }
        if (nodes.env != env) NAPI_exc("nodes env mismatch");
        *valp = nodes.value;
    }
    void frinfo_setup(napi_env env, napi_value* valp)
    {
        if (nodes.type() != napi_object)
        {
            frinfo.env = env;
            napi_value arybuf;
            void* NO_HINT = NULL; //optional finalize_hint
            !NAPI_OK(napi_create_external_arraybuffer(env, &gpu_wker->m_frinfo, sizeof(gpu_wker->m_frinfo), wker_check, NO_HINT, &arybuf), "Cre arraybuf failed");
            !NAPI_OK(napi_create_dataview(env, sizeof(gpu_wker->m_frinfo), arybuf, 0, &frinfo.value), "Cre frinfo data view failed");
            debug(YELLOW_MSG "frinfo data view created: &data " << &gpu_wker->m_frinfo << ", size " << sizeof(gpu_wker->m_frinfo) << ", napi thingy " << frinfo << ENDCOLOR);
//TODO:
//Protocol protocol; //= WS281X;
//const double frame_time; //msec
//const SDL_Size wh; //int NumUniv, UnivLen;
//const /*elapsed_t*/ /*std::result_of<now_msec()>::type*/ decltype(now_msec()) started;
//std::atomic</*uint32_t*/ int> numfr; //= 0; //#frames rendered / next frame#
//BkgSync<MASK_TYPE, true> dirty; //one Ready bit for each universe
//uint64_t times[NUM_STATS]; //total init/sleep (sec), render (msec), upd txtr (msec), xfr txtr (msec), present/sync (msec)
        }
        if (frinfo.env != env) NAPI_exc("frinfo env mismatch");
        *valp = frinfo.value;
    }
public: //operators:
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const AddonData& that) CONST
    {
//    std::ostringstream ss;
//    if (!rect) ss << "all";
//    else ss << (rect->w * rect->h) << " ([" << rect->x << ", " << rect->y << "]..[+" << rect->w << ", +" << rect->h << "])";
//    return ss.str();
        ostrm << "AddonData";
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
}; //AddonData;


//put all bkg wker execution on a consistent thread (SDL is not thread-safe):
//void bkg_wker(AddonData* aodata, int screen, key_t shmkey, int vgroup, GPUPORT::NODEVAL init_color)
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
    AddonData* aodata;
    napi_value argv[1], This;
    size_t argc = SIZEOF(argv);
//    napi_status status;
  // Retrieve the JavaScript callback we should call with items generated by the
  // worker thread, and the per-addon data.
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
    if (!env) return NULL; //Node in clean up mode?
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
    AddonData* aodata = static_cast<AddonData*>(data);
    debug(BLUE_MSG "call listen fats: aodata %p, valid? %d, context %p, clup mode? %d" ENDCOLOR, aodata, aodata->isvalid(), context, !env);
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
//    if (!aodata->listener.busy) NAPI_exc("not listening");
    if (!aodata->islistening()) NAPI_exc("not listening");
  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
//    debug(CYAN_MSG "cb %p" ENDCOLOR, aodata);
    if (!env) return; //Node clean up mode
//    {
//    napi_thingy retval; retval.env = env;
    napi_value argv[3];
// Convert the integer to a napi_value.
//NOTE: need to lazy-load params here because bkg wker didn't exist first time Listen_NAPI() was called
    if (!aodata->wker_ok()) NAPI_exc(env, "Gpu wker problem: " << aodata->exc_reason());
    !NAPI_OK(napi_create_int32(env, aodata->gpu_wker->m_frinfo.numfr, &argv[0]), "Create arg failed");
    aodata->nodes_setup(env, &argv[1]); //lazy-load data items for js
    aodata->frinfo_setup(env, &argv[2]); //lazy-load data items for js
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
    INSPECT(GREEN_MSG << "js fats this " << aodata->listener << ", svfrnum " << aodata->svfrnum << ", arg[0] " << napi_thingy(env, argv[0]) << ", arg[1] " << napi_thingy(env, argv[1]) << ", arg[2] " << napi_thingy(env, argv[2]) << ", retval " << napi_thingy(env, retval) << ENDCOLOR);
//static int count = 0; aodata->want_continue = (count++ < 7); return;
    !NAPI_OK(napi_coerce_to_number(env, retval, &num_retval), "Get retval as num failed");
//    debug(BLUE_MSG "cb: get bool %p" ENDCOLOR, aodata);
    !NAPI_OK(napi_get_value_uint32(env, num_retval, &ready_bits), "Get uint32 retval failed");
    debug(BLUE_MSG "js fats: ready bits 0x%x, continue? %d" ENDCOLOR, ready_bits, !!ready_bits);
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
    AddonData* aodata = static_cast<AddonData*>(data);
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
    AddonData* aodata = static_cast<AddonData*>(data);
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
//    AddonData* aodata = static_cast<AddonData*>(hint);
//    void* wrapee; //redundant; still have ptr in aodata
//    !NAPI_OK(napi_remove_wrap(env, aodata->gpwrap, &wrapee), "Unwrap failed");
////    !NAPI_OK(napi_get_undefined(env, &aodata->gpwrap), "Create null gpwrap failed");
}
#endif


#if 0
void my_refill(napi_env env, AddonData* aodata, GPUPORT::TXTR* ignored)
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
    AddonData* aodata;
    napi_value argv[2], This; //*This_DONT_CARE = NULL;
    size_t argc = SIZEOF(argv);
  // Retrieve the JavaScript callback we should call with items generated by the
  // worker thread, and the per-addon data.
    if (!env) return NULL; //Node clean up mode
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Get cb info failed");
    debug(CYAN_MSG "listen: aodata %p, valid? %d" ENDCOLOR, aodata, aodata->isvalid());
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
    void* NO_FINAL_DATA = NULL;
    napi_finalize NO_FINALIZE = NULL;
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
        AddonData* aodata = static_cast<AddonData*>(data); //(AddonData*)data;
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
    key_t shmkey = 0;
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
        !NAPI_OK(get_prop(env, argv[0], "shmkey", &shmkey), "Invalid .shmkey prop");
        !NAPI_OK(get_prop(env, argv[0], "vgroup", &vgroup), "Invalid .vgroup prop");
        !NAPI_OK(get_prop(env, argv[0], "color", &init_color), "Invalid .color prop");
        debug(BLUE_MSG "listen opts: screen %d, shmkey %lx, vgroup %d, init_color 0x%x" ENDCOLOR, screen, shmkey, vgroup, init_color);
        if (aodata->gpu_wker) debug(RED_MSG "TODO: check for arg mismatch" ENDCOLOR);
    }
//wrapped object for state info:
//    void* NO_HINT = NULL; //optional finalize_hint
//    napi_ref* NO_REF = NULL; //optional ref to wrapped object
//    !NAPI_OK(napi_wrap(env, This, aodata, addon_dtor, NO_HINT, NO_REF), "Wrap failed");
  // Create an async work item, passing in the addon data, which will give the worker thread access to the above-created thread-safe function.
#if 1
//create callback function to call above function and pass results on to bkg wker:
//    std::function<void(napi_env, AddonData*, GPUPORT::TXTR*)> my_refill = [](napi_env env, AddonData* aodata, GPUPORT::TXTR* ignored)
    GPUPORT::REFILL refill = [env, aodata](GPUPORT::TXTR* unused) //std::bind(my_refill, env, aodata, std::placeholders::_1);
    {
        InOutDebug inout("refill napi lamba", SRCLINE);
//    debug(CYAN_MSG "refill: get ctx" ENDCOLOR);
//    !NAPI_OK(napi_get_threadsafe_function_context(aodata->fats, (void**)&env), "Get fats env failed"); //what to do?
        if (!aodata->isvalid()) NAPI_exc(env, "aodata invalid");
//    if (!env) return; //Node in clean up state
//TODO: blocking or non-blocking?
//    debug(BLUE_MSG "refill: call cb" ENDCOLOR);
//        nodes(m_wker->m_nodes), //expose as property for simpler caller usage; /*static_cast<NODEROW*>*/ &(m_nodebuf.get())[1]), //use first row for frame info
//        frinfo(m_wker->m_frinfo), // /*static_cast<FrameInfo*>*/ (m_nodebuf.get())[0]),
        aodata->gpu_wker->m_frinfo.refill(); //++aodata->gpdata.numfr; //move to next frame
        aodata->svfrnum = aodata->gpu_wker->m_frinfo.numfr.load();
//NOTE: this will enque an async js func call, and then continue txtr xfr to GPU and wait for vsync
//this allows node rendering in main thread simultaneous with GPU blocking refreshes in bkg thread
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
    std::thread bkg([aodata, screen, shmkey, vgroup, init_color, refill]() //,refill,env](); //NOTE: bkg thread code should not use env
    {
        debug(PINK_MSG "bkg: aodata %p, valid? %d" ENDCOLOR, aodata, aodata->isvalid());
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
            aodata->gpu_wker || (aodata->gpu_wker = new GPUPORT::WKER(screen, shmkey, vgroup, init_color, refill, SRCLINE));
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
            aodata->gpu_wker->m_frinfo.numfr = -1; //kludge: setup first refill() for frane# 0
            aodata->gpu_wker->m_frinfo.reset(); //aodata->gpdata.numfr = 0; //rewind to first frame;; TODO: allow skip ahead?
            refill(NULL); //send request for first frame; CAUTION: async call to js func
            for (;;) //int i = 0; i < 5; ++i)
            {
                InOutDebug inout("bkg loop, fr# " << aodata->gpu_wker->m_frinfo.numfr.load(), SRCLINE);
                debug(YELLOW_MSG "bkg send refill fr# %d" ENDCOLOR, aodata->gpu_wker->m_frinfo.numfr.load());
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
            debug(RED_MSG "bkg wker exc: " << aodata->exc_reason() << ENDCOLOR);
            aodata->islistening(false); //listener.busy = true; //(void*)1;
        }
//        debug(YELLOW_MSG "bkg release" ENDCOLOR);
//        !NAPI_OK(napi_reference_unref(env, aodata->listener.ref, &ref_count), "Listener unref failed");
        !NAPI_OK(napi_release_threadsafe_function(aodata->fats, napi_tsfn_release), aodata->listener.env, "Can't release JS fats");
        aodata->fats = NULL;
//        aodata->listener.busy = false; //work = 0;
        debug(YELLOW_MSG "bkg exit after %d frames" ENDCOLOR, aodata->gpu_wker->m_frinfo.numfr.load());
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
    AddonData* aodata = static_cast<AddonData*>(data); //(AddonData*)data;
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
napi_value ModuleInit(napi_env env, napi_value exports)
{
    uint32_t napi_ver;
    const napi_node_version* ver;
    !NAPI_OK(napi_get_node_version(env, &ver), "Get node version info failed");
    !NAPI_OK(napi_get_version(env, &napi_ver), "Get napi version info failed");
    debug(BLUE_MSG "using Node v" << ver->major << "." << ver->minor << "." << ver->patch << " (" << ver->release << "), napi " << napi_ver << ENDCOLOR);
  // Define addon-level data associated with this instance of the addon.
//    AddonData* addon_data = new AddonData; //(AddonData*)malloc(sizeof(*addon_data));
    std::unique_ptr<AddonData> aodata(new AddonData(env)); //(AddonData*)malloc(sizeof(*addon_data));
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
    void* NO_HINT = NULL; //optional finalize_hint
    napi_ref* NO_REF = NULL; //optional ref to wrapped object
// Free the per-addon-instance data.
    napi_finalize /*std::function<void(napi_env, void*, void*)>*/ addon_final = [](napi_env env, void* data, void* hint)
    {
        UNUSED(hint);
        AddonData* aodata = static_cast<AddonData*>(data); //(AddonData*)data;
//    if (!env) return; //Node clean up mode
        debug(RED_MSG "addon final: aodata %p, valid? %d, hint %p" ENDCOLOR, aodata, aodata->isvalid(), hint);
        if (!aodata->isvalid()) NAPI_exc("aodata invalid");
        if (aodata->islistening()) NAPI_exc("Listener still active");
        delete aodata; //free(addon_data);
    };
    !NAPI_OK(napi_wrap(env, exports, aodata.get(), addon_final, /*aodata.get()*/NO_HINT, /*&aodata->aoref*/ NO_REF), "Wrap aodata failed");
  // Return the decorated exports object.
//    napi_status status;
    debug(GREEN_MSG "napi init: aodata %p, valid? %d" ENDCOLOR, aodata.get(), aodata.get()->isvalid());

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
    aodata.release(); //NAPI owns it now
    return exports;
}
 NAPI_MODULE(NODE_GYP_MODULE_NAME, ModuleInit)
 #endif //def SRC_NODE_API_H_ //USE_NAPI
#endif //def WANT_REAL_CODE


////////////////////////////////////////////////////////////////////////////////
////
/// example code
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
    status = napi_create_string_utf8(env, hello().c_str(), hello().length(), &retval);
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
 Napi::Object Init(Napi::Env env, Napi::Object exports)
 {
    exports.Set(Napi::String::New(env, "my_function"), Napi::Function::New(env, MyFunction_wrapped));
    exports.Set(Napi::String::New(env, "hello"), Napi::Function::New(env, Hello_wrapped));
    return exports;
 }
 #ifndef WANT_CALLBACK_EXAMPLE
  NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
 #endif
#endif //def SRC_NAPI_H_ //USE_NODE_ADDON_API
#ifdef SRC_NODE_API_H_ //USE_NAPI
 napi_value Init(napi_env env, napi_value exports)
 {
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
 #ifndef WANT_CALLBACK_EXAMPLE
  NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
 #endif
#endif //def SRC_NODE_API_H_ //USE_NAPI
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
  if (env != NULL) {
    napi_value undefined, js_the_prime;

    // Convert the integer to a napi_value.
    assert(napi_create_int32(env, the_prime, &js_the_prime) == napi_ok);

    // Retrieve the JavaScript `undefined` value so we can use it as the `this`
    // value of the JavaScript function call.
    assert(napi_get_undefined(env, &undefined) == napi_ok);

    // Call the JavaScript function and pass it the prime that the secondary
    // thread found.
    bool boolval;
    napi_value retval, bool_retval; //check ret val -dj
    assert(napi_call_function(env,
                              undefined,
                              js_cb,
                              1,
                              &js_the_prime,
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
/*napi_value*/ NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {

  // Define addon-level data associated with this instance of the addon.
  AddonData* addon_data = (AddonData*)malloc(sizeof(*addon_data));
  addon_data->work = NULL;

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
#ifdef WANT_SIMPLE_EXAMPLE
  Init(env, exports); //include earlier example code also -dj
#endif
  return exports;
}
#endif //def SRC_NODE_API_H_ //USE_NAPI
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

//eof