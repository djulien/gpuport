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

#include <utility> //std::forward<>
#include <string>
#include <sstream> //std::ostringstream

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
    else ss << errinfo->error_message << " (errcode " << errinfo->error_code << ", " << NAPI_LastStatus << ")";
    return ss.str(); //NOTE: returns stack var by value, not by ref
}

//TODO: add SRCLINE
#define NAPI_exc_1ARG(msg)  NAPI_exc_2ARGS(DEF_ENV, msg)
#define NAPI_exc_2ARGS(env, msg)  NAPI_exc_3ARGS(env, NO_ERRCODE, msg)
#define NAPI_exc_3ARGS(env, errcode, msg)  (napi_throw_error(env, errcode, std::ostringstream() << msg), false) //dummy "!okay" result to allow usage in conditional expr; throw() won't fall thru at run-time, though
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


//for debug or error msgs:
struct { napi_env env; napi_value value; } napi_valenv;
friend std::ostream& operator<<(std::ostream& ostrm, const napi_valenv& napval)
{
    int str_len;
    bool bool_val;
    char str_val[200];
    double float_val;
    napi_valuetype vtype;
    !NAPI_OK(napi_typeof(napval.env, napval.value, &vtype), napval.env, "Get typeof failed");
    switch (vtype)
    {
        case napi_undefined: ostrm << "napi undef"; break;
        case napi_null: ostrm << "napi null"; break;
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
            !NAPI_OK(napi_get_value_numbernapval.env, napval.value, &float_val), napval.env, "Get number value failed");
            ostrm << "napi number " << float_val;
            break;
//TODO if needed:
//napi_symbol,
//napi_object,
//napi_function,
//napi_external,
//napi_bigint,
        default: ostrm << "napi type " << vtype; break;
    }
    return ostrm;
}


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
    /*bool BKG_THREAD =*/ true
//    int NODEBITS = 24> //# bits to send for each WS281X node (protocol dependent)
>;


#ifdef WANT_REAL_CODE
 #ifdef SRC_NODE_API_H_ //USE_NAPI
 //#include <string>
//#include "sdl-helpers.h"
//set some hard-coded defaults:
//using LIMIT = limit<83>;
//#define LIMIT  limit<pct(50/60)> //limit brightness to 83% (50 mA/pixel instead of 60 mA); gives 15A/300 pixels, which is a good safety factor for 20A power supplies
//using GPUPORT = GpuPort<> gp/* = GpuPort<>::factory*/(NAMED{ _.screen = screen; _.vgroup = vgroup; /*_.wh = wh*/; _.protocol = static_cast<GpuPort<>::Protocol>(protocol) /*GpuPort<>::NONE WS281X*/; _.init_color = dimARGB(0.25, RED); SRCLINE; }); //NAMED{ .num_univ = NUM_UNIV, _.univ_len = UNIV_LEN, SRCLINE});

//limit brightness:
//NOTE: JS <-> C++ overhead is significant for this function
//it's probably better to just use a pure JS function for high-volume usage
napi_value Limit_NAPI(napi_env env, napi_callback_info info)
{
    napi_value argv[1], This;
    size_t argc = SIZEOF(argv);
//    napi_status status;
    void* aodata;
  // Retrieve the JavaScript callback we should call with items generated by the
  // worker thread, and the per-addon data.
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Get cb info extract failed");
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
    color = GPUPORT::limit(color); //actual work done here; the rest is overhead :(
    napi_value retval;
    !NAPI_OK(napi_create_uint32(env, color, &retval), "Retval failed");
    return retval;
}


//below loosely based on object_wrap and async_work_thread_safe_function examples at https://github.com/nodejs/node-addon-examples.git
//#include <assert.h>
//#include <stdlib.h>
#include <memory> //std::unique_ptr<>

typedef struct aodata
{
    int valid = VALID;
    static const int VALID = 0x1234beef;
//    int frnum;
//    int the_prime;
    GPUPORT* gpu_port = 0;
    bool want_continue;
//    napi_ref gpwrap;
    struct { int x; bool y; int numfr; } gpdata;
//    GPUPORT::SHAREDINFO gpdata;
  //  GPUPORT::WKER* wker;
//    std::thread wker; //kludge: force thread affinity by using explicit thread object
    /*napi_async_work*/ void* work = NULL;
    napi_threadsafe_function fats; //tsfn; //asynchronous thread-safe JavaScript call-back function
    napi_ref aoref; //ref to wrapped version of this object
public: //methods
    bool isvalid() const { return this && (valid == VALID); }
} AddonData;


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
static void Listen_cb(napi_env env, napi_value js_fats, void* context, void* data)
{
    UNUSED(context);
  // Retrieve the prime from the item created by the worker thread.
//    int the_prime = *(int*)data;
    AddonData* aodata = static_cast<AddonData*>(data);
  // env and js_cb may both be NULL if Node.js is in its cleanup phase, and
  // items are left over from earlier thread-safe calls from the worker thread.
  // When env is NULL, we simply skip over the call into Javascript and free the
  // items.
//    debug(CYAN_MSG "cb %p" ENDCOLOR, aodata);
    if (!env) return; // != NULL)
//    {
    napi_valenv retval; retval.env = env;
    napi_value argv[1], /*retval,*/ bool_retval, This;
// Convert the integer to a napi_value.
    !NAPI_OK(napi_create_int32(env, aodata->gpdata.numfr, &argv[0]), "Create arg failed");
//        !NAPI_OK(napi_get_undefined(env, &argv[1]), "Create null this failed");
  //  !NAPI_OK(napi_create_int32(env, aodata->the_prime, &argv[1]), "Create arg[1] failed"); //TODO: get rid of
// Retrieve the JavaScript `undefined` value so we can use it as the `this`
// value of the JavaScript function call.
//        !NAPI_OK(napi_get_undefined(env, &This), "Create null this failed");
// Call the JavaScript function and pass it the prime that the secondary
// thread found.
    debug(BLUE_MSG "cb: call fats" ENDCOLOR);
    !NAPI_OK(napi_call_function(env, /*aodata->gpwrap*/This, js_fats, SIZEOF(argv), argv, &retval.value), "Call JS fats failed");
//    debug(BLUE_MSG "cb: check fats retval" ENDCOLOR);
//        /*int32_t*/ bool want_continue;
    INSPECT(GREEN_MSG << "retval " << retval << ENDCOLOR);
    !NAPI_OK(napi_coerce_to_bool(env, retval.value, &bool_retval), "Can't get JS fats retval as bool");
//    debug(BLUE_MSG "cb: get bool %p" ENDCOLOR, aodata);
    !NAPI_OK(napi_get_value_bool/*int32*/(env, bool_retval, &aodata->want_continue), "Can't get JS fats bool retval");
    debug(BLUE_MSG "js fats: continue? %d" ENDCOLOR, aodata->want_continue);
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


void my_refill(GPUPORT::TXTR* ignored, AddonData* aodata)
{
    napi_env env;
//    debug(CYAN_MSG "refill: get ctx" ENDCOLOR);
    if (!NAPI_OK(napi_get_threadsafe_function_context(aodata->fats, (void**)&env))) //what to do?
    {
        debug(BLUE_MSG "cant get fats env" ENDCOLOR);
        return;
    }
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
//TODO: blocking or non-blocking?
//    debug(BLUE_MSG "refill: call cb" ENDCOLOR);
    !NAPI_OK(napi_call_threadsafe_function(aodata->fats, aodata, napi_tsfn_blocking), "Can't call JS fats");
    ++aodata->gpdata.numfr;
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


// Create a thread-safe function and an async queue work item. We pass the
// thread-safe function to the async queue work item so the latter might have a
// chance to call into JavaScript from the worker thread on which the
// ExecuteWork callback runs.
//main entry point for this add-on:
static napi_value Listen_NAPI(napi_env env, napi_callback_info info)
{
    napi_value argv[2], This; //*This_DONT_CARE = NULL;
    size_t argc = SIZEOF(argv);
    AddonData* aodata;
  // Retrieve the JavaScript callback we should call with items generated by the
  // worker thread, and the per-addon data.
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aodata), "Get cb info extract failed");
    debug(CYAN_MSG "listen: aodata %p" ENDCOLOR, aodata);
    if ((argc < 1) || (argc > 2)) NAPI_exc("expected [{opts}], cb() params, got " << argc << " params");
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
    if (aodata->work) NAPI_exc("Already listening (only one work item must exist at a time)"); //check if que empty
    if (aodata->gpu_port) NAPI_exc("Gpu port already open");
  // Convert the callback retrieved from JavaScript into a thread-safe function
  // which we can call from a worker thread.
    napi_value work_name;
    !NAPI_OK(napi_create_string_utf8(env, "GpuPort async thread-safe callback function", NAPI_AUTO_LENGTH, &work_name), "Cre wkitem desc str failed");
    const napi_value NO_RESOURCE = NULL; //optional, for init hooks
    const int QUE_LEN = 0; //no limit
    const int NUM_THREADS = 1; //#threads that will use caller's func (including main thread)
    void* FINAL_DATA = NULL; //optional data for thread_finalize_cb
    napi_finalize THREAD_FINAL = NULL; //optional func to destroy tsfn
    void* CONTEXT = NULL; //optional data to attach to tsfn
    !NAPI_OK(napi_create_threadsafe_function(env, argv[argc - 1], NO_RESOURCE, work_name, QUE_LEN, NUM_THREADS, FINAL_DATA, THREAD_FINAL, CONTEXT, Listen_cb, &aodata->fats), "Cre JS fats failed");
//wrapped object for state info:
//    void* NO_HINT = NULL; //optional finalize_hint
//    napi_ref* NO_REF = NULL; //optional ref to wrapped object
//    !NAPI_OK(napi_wrap(env, This, aodata, addon_dtor, NO_HINT, NO_REF), "Wrap failed");
  // Create an async work item, passing in the addon data, which will give the worker thread access to the above-created thread-safe function.
    int screen = FIRST_SCREEN;
    key_t shmkey = 0;
    int vgroup = 1;
    GPUPORT::NODEVAL init_color = 0;
    GPUPORT::REFILL refill = std::bind(my_refill, std::placeholders::_1, aodata);
    if (argc > 1) //unpack option values
    {
        bool has_prop;
//        napi_value propval;
        napi_valuetype valtype;
        !NAPI_OK(napi_typeof(env, argv[0], &valtype), "Get arg type failed");
        if (valtype != napi_object) NAPI_exc("Expected object as first param"); //TODO: allow other types?
        !NAPI_OK(get_prop(env, argv[0], "screen", &screen), "Invalid .screen prop");
        !NAPI_OK(get_prop(env, argv[0], "shmkey", &shmkey), "Invalid .shmkey prop");
        !NAPI_OK(get_prop(env, argv[0], "vgroup", &vgroup), "Invalid .vgroup prop");
        !NAPI_OK(get_prop(env, argv[0], "color", &init_color), "Invalid .color prop");
    }
//    aodata->gpdata.numfr = 0;
//    aodata->wkr = std::thread(bkg_wker, aodata, screen, shmkey, vgroup, init_color, SRCLINE);
//    !NAPI_OK(napi_acquire_threadsafe_function(aodata->fats), "Can't acquire JS fats");
#if 0
    aodata->gpu_port = new GPUPORT(NAMED{ _.screen = screen; _.shmkey = shmkey; _.vgroup = vgroup; _.init_color = init_color; _.refill = refill; SRCLINE; });
    if (!aodata->gpu_port) NAPI_exc("Cre bkg gpu port failed");
#endif
    aodata->want_continue = true;
    std::thread bkg([refill, aodata, env]()
    {
        debug(PINK_MSG "bkg: aodata %p" ENDCOLOR, aodata);
        aodata->work = (void*)1;
//        debug(YELLOW_MSG "bkg acq" ENDCOLOR);
        !NAPI_OK(napi_acquire_threadsafe_function(aodata->fats), "Can't acquire JS fats");
        for (;;) //int i = 0; i < 5; ++i)
        {
            debug(YELLOW_MSG "bkg refill" ENDCOLOR);
            refill(NULL);
            if (!aodata->want_continue) break;
//            debug(YELLOW_MSG "bkg pivot encode + xfr" ENDCOLOR);
            SDL_Delay(0.5 sec);
//            debug(YELLOW_MSG "bkg present + wait vsync" ENDCOLOR);
            SDL_Delay(0.5 sec);
        }
//        debug(YELLOW_MSG "bkg release" ENDCOLOR);
        !NAPI_OK(napi_release_threadsafe_function(aodata->fats, napi_tsfn_release), "Can't release JS fats");
        aodata->fats = NULL;
        aodata->work = 0;
        debug(YELLOW_MSG "bkg exit" ENDCOLOR);
    });
    bkg.detach();
#if 0
    void* NO_HINT = NULL; //optional finalize_hint
//    napi_ref* NO_REF = NULL; //optional ref to wrapped object
//    !NAPI_OK(napi_wrap(env, This, /*reinterpret_cast<void*>*/&aodata->gpdata, gpdata_dtor, aodata/*NO_HINT*/, &aodata->gpwrap), "Wrap gpdata failed");
    !NAPI_OK(napi_create_async_work(env, NO_RESOURCE, work_name, ExecuteWork, WorkComplete, aodata, &(aodata->work)), "Cre async wkitem failed");
    !NAPI_OK(napi_queue_async_work(env, aodata->work), "Enqueue async wkitem failed");
#endif
//    return NULL; //return "undefined" to JavaScript
    napi_value retval;
    !NAPI_OK(napi_get_reference_value(env, aodata->aoref, &retval), "Cre retval ref failed");
    return retval;
}


// Free the per-addon-instance data.
static void addon_destroy(napi_env env, void* data, void* hint)
{
    UNUSED(hint);
    AddonData* aodata = static_cast<AddonData*>(data); //(AddonData*)data;
    debug(RED_MSG "napi destroy: aodata %p" ENDCOLOR, aodata);
    if (!aodata->isvalid()) NAPI_exc("aodata invalid");
    if (!aodata->work) NAPI_exc("Work item still in progress at module unload");
    if (aodata->gpu_port) delete aodata->gpu_port; //close at playlist end, not after each sequence
    aodata->gpu_port = 0;
//  free(addon_data);
    delete aodata;
}


// The commented-out return type and the commented out formal function
// parameters below help us keep in mind the signature of the addon
// initialization function. We write the body as though the return value were as
// commented below and as though there were parameters passed in as commented
// below.
//module exports:
//NOTE: GpuPort is a singleton for now; use "exports" as object and define simple named props for exported methods
napi_value ModuleInit(napi_env env, napi_value exports)
{
  // Define addon-level data associated with this instance of the addon.
//    AddonData* addon_data = new AddonData; //(AddonData*)malloc(sizeof(*addon_data));
    std::unique_ptr<AddonData> aodata(new AddonData); //(AddonData*)malloc(sizeof(*addon_data));
    // Arguments 2 and 3 are function name and length respectively
    // We will leave them as empty for this example
    const char* NO_NAME = NULL;
    size_t NO_NAMELEN = 0;
    napi_value fn;
//    void* NO_DATA = NULL;
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
  // Associate the addon data with the exports object, to make sure that when the addon gets unloaded our data gets freed.
    void* NO_HINT = NULL; //optional finalize_hint
//    napi_ref* NO_REF = NULL; //optional ref to wrapped object
    !NAPI_OK(napi_wrap(env, exports, aodata.get(), addon_destroy, /*aodata.get()*/NO_HINT, &aodata->aoref), "Wrap aodata failed");
  // Return the decorated exports object.
//    napi_status status;
    debug(GREEN_MSG "napi init: aodata %p" ENDCOLOR, aodata.get());
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
 #include <assert.h>
 #include <stdlib.h>
// Limit ourselves to this many primes, starting at 2
 #define PRIME_COUNT 100000
 #define REPORT_EVERY 1000

typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn;
} AddonData;

// This function is responsible for converting data coming in from the worker
// thread to napi_value items that can be passed into JavaScript, and for
// calling the JavaScript function.
static void CallJs(napi_env env, napi_value js_cb, void* context, void* data) {
  // This parameter is not used.
  (void) context;

  // Retrieve the prime from the item created by the worker thread.
  int the_prime = *(int*)data;

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
    assert(napi_call_function(env,
                              undefined,
                              js_cb,
                              1,
                              &js_the_prime,
                              NULL) == napi_ok);
  }

  // Free the item created by the worker thread.
  free(data);
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
      int* the_prime = (int*)malloc(sizeof(*the_prime)); //cast (cpp) -dj
      *the_prime = idx_outer;

      // Initiate the call into JavaScript. The call into JavaScript will not
      // have happened when this function returns, but it will be queued.
      assert(napi_call_threadsafe_function(addon_data->tsfn,
                                           the_prime,
                                           napi_tsfn_blocking) == napi_ok);
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