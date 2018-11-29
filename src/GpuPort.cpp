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
//2. gdb node; run ../

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
//#define WANT_BROKEN_CODE
//#define WANT_SIMPLE_EXAMPLE
//#define WANT_CALLBACK_EXAMPLE


#if defined(USE_NAPI)
 #define NAPI_EXPERIMENTAL //NOTE: need this to avoid compile errors; needs Node v10.6.0 or later
 #include <node_api.h> //C style api; https://nodejs.org/api/n-api.html
 #define NAPI_EXPORTS  NAPI_MODULE //kludge: make macro name consistent to reduce #ifs
 #ifndef SRC_NODE_API_H_
  #define SRC_NODE_API_H_ //USE_NAPI; for "smart" text editors
 #endif
#elif defined(USE_NODE_ADDON_API)
 #include "napi.h" //C++ style api; #includes node_api.h
 #define NAPI_EXPORTS  NODE_API_MODULE //kludge: make macro name consistent to reduce #ifs
#elif defined(USE_NAN)
 #include <nan.h> //older V8 api
#else
 #error Use which Node api?
#endif

#include <sstream> //std::ostringstream
#include <utility> //std::forward<>
#include <functional> //std::bind()
#include <string> //std::string
#include <map> //std::map<>

#include "str-helpers.h" //unmap()
#include "thr-helpers.h" //thrnx()
#include "elapsed.h" //elapsed_msec(), timestamp(), now()
#include "msgcolors.h"
#include "debugexc.h"

#if __cplusplus < 201103L
 #pragma message("CAUTION: this file probably needs c++11 or later to compile correctly")
#endif


#define UNUSED(thing)  //(void)thing //avoid compiler warnings

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


////////////////////////////////////////////////////////////////////////////////
////
/// NAPI helpers, wrappers:
//

#ifdef SRC_NODE_API_H_ //USE_NAPI

#define NO_HINT  NULL //const void* NO_HINT = NULL; //optional finalize_hint
#define NO_FINALIZE  NULL //napi_finalize NO_FINALIZE = NULL;
#define NO_FINAL_DATA  NULL //void* NO_FINAL_DATA = NULL;

//define top of module init chain:
#define module_exports(env, exports)  exports

//important NOTEs about napi object lifespan:
//https://nodejs.org/api/n-api.html#n_api_object_lifetime_management


//remember last error (mainly for debug msgs):
//CAUTION: shared between threads
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
#define DEF_ENV  env //assume env is named "env"
#define NO_ERRCODE  NULL
#define NAPI_exc_1ARG(msg)  NAPI_exc_2ARGS(DEF_ENV, msg)
#define NAPI_exc_2ARGS(env, msg)  NAPI_exc_3ARGS(env, NO_ERRCODE, msg)
#define NAPI_exc_3ARGS(env, errcode, msg)  NAPI_exc_4ARGS(env, errcode, msg, SRCLINE) //(napi_throw_error(env, errcode, std::ostringstream() << RED_MSG << msg << ": " << NAPI_ErrorMessage(env) << ENDCOLOR), 0) //dummy "!okay" or null ptr result to allow usage in conditional expr; throw() won't fall thru at run-time, though
#define NAPI_exc_4ARGS(env, errcode, msg, srcline)  (napi_throw_error(env, errcode, std::ostringstream() << RED_MSG << msg << ": " << NAPI_ErrorMessage(env) << ENDCOLOR_ATLINE(srcline)), 0) //dummy "!okay" or null ptr result to allow usage in conditional expr; throw() won't fall thru at run-time, though
//#define NAPI_exc_4ARGS(env, errcode, msg, retval)  (napi_throw_error(env, errcode, std::ostringstream() << RED_MSG << msg << ": " << NAPI_ErrorMessage(env) << ENDCOLOR), retval) //dummy "!okay" result to allow usage in conditional expr; throw() won't fall thru at run-time, though
#define NAPI_exc(...)  UPTO_4ARGS(__VA_ARGS__, NAPI_exc_4ARGS, NAPI_exc_3ARGS, NAPI_exc_2ARGS, NAPI_exc_1ARG) (__VA_ARGS__)
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

//reduce verbosity by using a unique small int instead of ugly env id:
//mainly for debug msgs
int envinx(const napi_env env)
{
//TODO: move to shm?
    static std::vector</*std::decay<decltype(thrid())>*/napi_env> envlist;
    static std::mutex mtx;
    std::unique_lock<decltype(mtx)> lock(mtx);

    for (auto it = envlist.begin(); it != envlist.end(); ++it) //small list; hash/map overhead not justified so just use linear scan
        if (*it == env) return it - envlist.begin();
    int newinx = envlist.size();
    envlist.push_back(env);
    return newinx;
}


//napi value type convenience/helper functions:
napi_valuetype valtype(napi_env env, napi_value value)
{
    napi_valuetype valtype = napi_undefined;
//    std::string reason;
//    std::exception_ptr excptr = nullptr; //init redundant (default init)
//use try/catch in case value is bad:
#if 0
    try { !NAPI_OK(napi_typeof(env, value, &valtype), "Get value type failed"); }
    catch (const std::exception& exc) { debug(RED_MSG " valtype exc: " << exc.what() << ENDCOLOR); } //reason = exc.what(); }
    catch (...) { debug(RED_MSG "valtype exc: ??EXC??" ENDCOLOR); } //reason = "??EXC??"; }
//    debug(RED_MSG "bkg wker exc: " << aoptr->exc_reason() << ENDCOLOR);
#else
    !NAPI_OK(napi_typeof(env, value, &valtype), "Get value type failed");
#endif
    return valtype;
}
static inline const char* TypeName(napi_valuetype key)
{
    static const std::map<napi_valuetype, const char*> names =
    {
        {napi_undefined, "undef"},
        {napi_null, "null"},
        {napi_boolean, "bool"},
        {napi_number, "number"},
        {napi_string, "string"},
        {napi_symbol, "symb"},
        {napi_object, "obj"},
        {napi_function, "func"},
        {napi_external, "extern"},
        {napi_bigint, "bigint"},
    };
    return unmap(names, key); //names;
}
static inline const char* TypeName(napi_typedarray_type key)
{
    static const std::map<napi_typedarray_type, const char*> names =
    {
        {napi_int8_array, "int8"},
        {napi_uint8_array, "uin8"},
        {napi_uint8_clamped_array, "cluint8"},
        {napi_int16_array, "int16"},
        {napi_uint16_array, "uint16"},
        {napi_int32_array, "int32"},
        {napi_uint32_array, "uint32"}, //only one used below; others included for completeness
        {napi_float32_array, "float32"},
        {napi_float64_array, "float64"},
        {napi_bigint64_array, "bigint64"},
        {napi_biguint64_array, "biguint64"},
    };
    return unmap(names, key); //names;
}


//create thread-safe wrapper for caller's js callback function:
//    napi_ref aoref; //ref to wrapped version of this object
void make_fats(napi_env env, napi_value jsfunc, napi_threadsafe_function_call_js napi_cb, napi_threadsafe_function* fats) //asynchronous thread-safe JavaScript call-back function; can be called from any thread
{
    napi_value wker_name;
    const napi_value NO_RESOURCE = NULL; //optional, for init hooks
    const int QUE_NOMAX = 0; //no limit; TODO: should this be 1 to prevent running ahead? else requires multi-frame pixel bufs
    const int NUM_THREADS = 1; //#threads that will use caller's func (including main thread)
//    void* NO_FINAL_DATA = NULL;
//    napi_finalize NO_FINALIZE = NULL;
    void* NO_CONTEXT = NULL;
//    void* FINAL_DATA = NULL; //optional data for thread_finalize_cb
    napi_finalize THREAD_FINAL = NULL; //optional func to destroy tsfn
    if (valtype(env, jsfunc) != napi_function) NAPI_exc("expected js function arg");
    !NAPI_OK(napi_create_string_utf8(env, "GpuPort async thread-safe callback function", NAPI_AUTO_LENGTH, &wker_name), "Cre wkitem desc str failed");
    !NAPI_OK(napi_create_threadsafe_function(env, jsfunc, /*aodata->listener.obj.value*/ NO_RESOURCE, wker_name, QUE_NOMAX, NUM_THREADS, NO_FINAL_DATA, NO_FINALIZE, NO_CONTEXT, napi_cb, fats), "Cre JS fats failed");
}

struct my_napi_property_descriptor: public napi_property_descriptor
{
//    using super = napi_property_descriptor;
    static SrcLine srcline; //kludge: create a place for _.srcline; CAUTION: don't change size of prop descr (used in arrays)
    my_napi_property_descriptor() //clear first so NULL members don't need to be explicitly set by caller
    {
        utf8name = NULL; name = NULL;
        method = NULL; getter = NULL; setter = NULL; value = NULL;
        attributes = napi_default; //default = read-only, !enumerable, !cfgable
        data = NULL;
    }
};
SrcLine /*GpuPortData::*/my_napi_property_descriptor::srcline; //kludge: create a place for _.srcline in above code; can't use static function wrapper due to definition of SRCLINE

//napi property convenience/helper functions:
bool has_prop(napi_env env, napi_value obj, const char* name)
{
    bool has_prop;
    !NAPI_OK(napi_has_named_property(env, obj, name, &has_prop), "Check for named prop failed");
    return has_prop;
}

//template <typename napi_status (*getval)
bool /*napi_status*/ get_prop(napi_env env, napi_value obj, const char* name, napi_value* valp)
{
//    bool has_prop;
//    napi_value prop_val;
//    if (!NAPI_OK(napi_has_named_property(env, obj, name, has_prop)) || !*has_prop) return NAPI_LastStatus;
//    return napi_get_named_property(env, obj, name, valp);
    if (!has_prop(env, obj, name)) return false;
    !NAPI_OK(napi_get_named_property(env, obj, name, valp), "Get named prop failed");
    return true;
}

//type-specific overloads:
bool /*napi_status*/ get_prop(napi_env env, napi_value obj, const char* name, int32_t* valp)
{
    napi_value prop_val;
    if (!get_prop(env, obj, name, &prop_val)) return false;
//    if (valtype(env, obj) != )
    return NAPI_OK(napi_get_value_int32(env, prop_val, valp), "Get int32 prop failed");
}
bool /*napi_status*/ get_prop(napi_env env, napi_value obj, const char* name, uint32_t* valp)
{
//    bool has_prop;
    napi_value prop_val;
    if (!get_prop(env, obj, name, &prop_val)) return false;
    return NAPI_OK(napi_get_value_uint32(env, prop_val, valp), "Get uint32 prop failed");
}


//kludge: composite object for operator<< specialization:
//CAUTION; do not store napi_values across high-level napi calls / on  heap
struct napi_thingy
{
    struct Object {}; //ctor disambiguation tag
    napi_env env; //CAUTION: doesn't remain valid across napi calls/events
    napi_value value;
public: //ctors/dtors
//    explicit napi_thingy() {}
    napi_thingy() = delete; //default ctor is useless without env
    explicit inline napi_thingy(napi_env new_env): env(new_env) { cre_undef(); } //force env to be available
    inline napi_thingy(napi_env new_env, napi_value new_value): env(new_env), value(new_value) {} //TODO: inc ref count?
    inline napi_thingy(napi_env new_env, Object) { cre_object(new_env); }
    inline napi_thingy(napi_env new_env, int32_t new_val) { cre_int32(new_env, new_val); }
    inline napi_thingy(napi_env new_env, uint32_t new_val) { cre_uint32(new_env, new_val); }
    inline napi_thingy(napi_env new_env, double new_val) { cre_double(new_env, new_val); }
    inline napi_thingy(napi_env new_env, std::string str) { cre_string(new_env, str); }
    inline napi_thingy(napi_env new_env, void* data, size_t len) { cre_ext_arybuf(new_env, data, len); }
    inline napi_thingy(napi_env new_env, napi_typedarray_type arytype, size_t count, napi_value arybuf, size_t bofs = 0) { cre_typed_ary(new_env, arytype, count, arybuf, bofs); }
//    inline napi_thingy(const napi_thingy)
    inline ~napi_thingy() { cre_undef(); } //reset in case caller added refs to other objects; probably not needed
public: //operators
    inline napi_thingy& operator=(const napi_thingy& that) { env = that.env; value = that.value; return *this; }
    inline napi_thingy& operator=(napi_value that) { value = that; return *this; }
    inline operator napi_env() const { return env; }
    inline operator napi_value() const { return value; }
    inline bool isarybuf() const
    {
        bool is_ary;
        if (type() != napi_object) return false;
        !NAPI_OK(napi_is_arraybuffer(env, value, &is_ary), "Check if array buffer failed");
        return is_ary;
    }
    inline bool istypary() const
    {
        bool is_typary;
        if (type() != napi_object) return false;
        !NAPI_OK(napi_is_typedarray(env, value, &is_typary), "Check if typed array failed");
        return is_typary;
    }
//    inline napi_valuetype type(napi_valuetype type)
//    {
//        if (type() != type) NAPI_exc(opts.env, "Expected object as first arg"); //TODO: allow other types?
//    }
    inline napi_valuetype type() const { return valtype(env, value); }
    napi_typedarray_type arytype() const
    {
        void* data;
        napi_value arybuf;
        size_t arylen, bofs;
        napi_typedarray_type arytype;
        if (!istypary()) return static_cast<napi_typedarray_type>(-1);
        !NAPI_OK(napi_get_typedarray_info(env, value, &arytype, &arylen, &data, &arybuf, &bofs), "Get typed array info failed");
        return arytype;
    }
//    napi_status napi_get_dataview_info(napi_env env, napi_value dataview, size_t* byte_length, void** data, napi_value* arraybuffer, size_t* byte_offset)
//for debug or error msgs:
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const napi_thingy& that) CONST
    {
        bool bool_val;
        size_t str_len;
        char str_val[200];
        double float_val;
        ostrm << "napi-";
//        if (napval.istypary())
//        {
//            ostrm << NVL(TypeName(napval.arytype()), "??ARYTYPE??");
//            ostrm << "(" << napval.type() << ":" << napval.arytype() << ")";
//        }
//        else
//        {
        ostrm << NVL(TypeName(that.type()), "??TYPE??");
        ostrm << "(" << that.type() << ")";
//        }
//        if (napval.type() == napi_object) 
        ostrm << " env#" << envinx(that.env); //" env 0x" << std::hex << napval.env << std::dec;
        switch (that.type())
        {
//            case napi_null:
//            case napi_undefined:
//TODO:
//napi_symbol,
//napi_function,
//napi_bigint,
//napi_object,
//                ostrm << "napi " << NVL(TypeName(napval.type()), "??TYPE??");
//                break;
//show value for scalar types:
            case napi_boolean:
                !NAPI_OK(napi_get_value_bool(that.env, that.value, &bool_val), that.env, "Get bool value failed");
                ostrm << " " << bool_val;
                break;
            case napi_string:
                !NAPI_OK(napi_get_value_string_utf8(that.env, that.value, str_val, sizeof(str_val) - 1, &str_len), that.env, "Get string value failed");
                if (str_len >= sizeof(str_val) - 1) strcpy(str_val + sizeof(str_val) - 5, " ...");
                else str_val[str_len] = '\0';
                ostrm << " " << str_len << ":'" << str_val << "'";
                break;
            case napi_number:
                !NAPI_OK(napi_get_value_double(that.env, that.value, &float_val), that.env, "Get number value failed");
                ostrm << " " << float_val;
                break;
//show into for array types:
            case napi_object:
                if (that.isarybuf())
                {
                    void* data;
                    size_t datalen;
                    !NAPI_OK(napi_get_arraybuffer_info(that.env, that.value, &data, &datalen), that.env, "Get arybuf info failed");
                    ostrm << " ARYBUF " << commas(datalen) << ":0x" << std::hex << data << std::dec;
                    break;
                }
                if (that.istypary())
                {
                    void* data;
                    size_t count, bytofs;
                    napi_thingy arybuf(that.env);
                    napi_typedarray_type arytype;
//            ostrm << NVL(TypeName(napval.arytype()), "??ARYTYPE??");
//            ostrm << "(" << napval.type() << ":" << napval.arytype() << ")";
                    !NAPI_OK(napi_get_typedarray_info(that.env, that.value, &arytype, &count, &data, &arybuf.value, &bytofs), that.env, "Get typed array info failed");
                    ostrm << " TYPARY " << TypeName(arytype) << "[" << commas(count) << "] on " << arybuf; //CAUTION: 1 level of recursion
                    if (bytofs) ostrm << "+0x" << std::hex << bytofs << "=" << data << std::dec; 
                    break;
                }
//napi_external,
//            default: ostrm << "napi: type " << napval.type(); break;
        }
        return ostrm;
    }
public: //methods
//get value:
    inline bool has_prop(const char* name) { return ::has_prop(env, value, name); }
    inline bool get_prop(const char* name, napi_value* valp) { return ::get_prop(env, value, name, valp); }
    inline bool get_prop(const char* name, uint32_t* valp) { return ::get_prop(env, value, name, valp); }
    inline bool get_prop(const char* name, int32_t* valp) { return ::get_prop(env, value, name, valp); }
//set new value:
    inline void cre_null(napi_env new_env) { env = new_env; cre_null(); }
    inline void cre_undef(napi_env new_env) { env = new_env; cre_undef(); }
    inline void cre_object(napi_env new_env) { env = new_env; cre_object(); }
    inline void cre_string(napi_env new_env, std::string str) { env = new_env; cre_string(str); }
    inline void cre_string(napi_env new_env, const char* buf, size_t len) { env = new_env; cre_string(buf, len); }
    inline void cre_ext_arybuf(napi_env new_env, void* buf, size_t len) { env = new_env; cre_ext_arybuf(buf, len); }
    inline void cre_typed_ary(napi_env new_env, napi_typedarray_type type, size_t count, napi_value arybuf, size_t bofs = 0) { env = new_env; cre_typed_ary(type, count, arybuf, bofs); }
//n/a    inline void cre_bool(napi_env new_env, bool new_val) { env = new_env; cre_bool(new_val); }
    inline void cre_int32(napi_env new_env, int32_t new_val) { env = new_env; cre_int32(new_val); }
    inline void cre_uint32(napi_env new_env, uint32_t new_val) { env = new_env; cre_uint32(new_val); }
    inline void cre_double(napi_env new_env, double new_val) { env = new_env; cre_double(new_val); }

    inline void cre_null() { !NAPI_OK(napi_get_null(env, &value), "Cre null failed"); verify(napi_null); }
    inline void cre_undef() { !NAPI_OK(napi_get_undefined(env, &value), "Cre undef failed"); verify(napi_undefined); }
    inline void cre_object() { !NAPI_OK(napi_create_object(env, &value), "Cre obj failed"); verify(napi_object); }
    inline void cre_string(std::string str) { !NAPI_OK(napi_create_string_utf8(env, str.c_str(), str.length(), &value), "Cre str failed"); verify(napi_string); }
    inline void cre_string(const char* buf, size_t strlen) { !NAPI_OK(napi_create_string_utf8(env, buf, strlen, &value), "Cre str failed"); verify(napi_string); }
    inline void cre_ext_arybuf(void* buf, size_t len) { !NAPI_OK(napi_create_external_arraybuffer(env, buf, len, NO_FINALIZE, NO_HINT, &value), "Cre arraybuf failed"); verify(napi_object, isarybuf()); }
    inline void cre_typed_ary(napi_typedarray_type type, size_t count, napi_value arybuf, size_t bofs = 0) { !NAPI_OK(napi_create_typedarray(env, type, count, arybuf, bofs, &value), "Cre typed array failed"); verify(napi_object, istypary()); }
//n/a    inline void cre_bool(bool new_val) { !NAPI_OK(napi_create_bool(env, new_val, &value), "Cre bool failed"); show(); }
    inline void cre_int32(int32_t new_val) { !NAPI_OK(napi_create_int32(env, new_val, &value), "Cre int32 failed"); verify(napi_number); }
    inline void cre_uint32(uint32_t new_val) { !NAPI_OK(napi_create_uint32(env, new_val, &value), "Cre uint32 failed"); verify(napi_number); }
    inline void cre_double(double new_val) { !NAPI_OK(napi_create_double(env, new_val, &value), "Cre float failed"); verify(napi_number); }
    int32_t operator=(const int32_t& new_val) { cre_int32(new_val); return new_val; }
    uint32_t operator=(const uint32_t& new_val) { cre_uint32(new_val); return new_val; }
    void verify(napi_valuetype chk_type, bool subtypeok = true) //napi_typedarray_type arytype = 0)
    {
        if ((type() != chk_type) || !subtypeok) debug(RED_MSG << "failed to create " << TypeName(chk_type) << ENDCOLOR);
//        else debug(BLUE_MSG << TypeName(chk_type) << " created ok" << ENDCOLOR);
    }
};
//napi_thingy;
#endif //def SRC_NODE_API_H_ //USE_NAPI


#if 0
//NOTE about napi object lifespan, refs: https://nodejs.org/api/n-api.html#n_api_references_to_objects_with_a_lifespan_longer_than_that_of_the_native_method
struct napi_autoref //: public napi_ref
{
    napi_env env;
    napi_ref ref;
//    using super = napi_ref;
public: //ctors/dtors
    inline napi_autoref(): ref(nullptr) {}
//    inline autoref(napi_env new_env, napi_value new_value): autoref(), obj(new_env, new_value)
    ~napi_autoref() { reset(); }
public: //operators
//    inline operator napi_ref() const { return ref; }
//    operator napi_value() const { return value(); }
    inline napi_value operator=(const napi_thingy& that) { reset(that); return that.value; }
public: //methods
//    napi_thingy value() { return napi_thingy(env, value()); }
    napi_value value() const
    {
        napi_value val;
        if (!ref) exc_hard("no object ref");
        if (thrinx()) debug(YELLOW_MSG "wrong thread" ENDCOLOR);
        !NAPI_OK(napi_get_reference_value(env, ref, &val), "Get ref val failed");
        return val;
    }
    void reset()
    {
        if (ref) debug(YELLOW_MSG "del ref to " << value() << ENDCOLOR);
        if (thrinx()) debug(YELLOW_MSG "wrong thread" ENDCOLOR);
        if (ref) !NAPI_OK(napi_delete_reference(env, ref), "Del ref failed");
        ref = nullptr;
    }
    void reset(const napi_thingy& that) { reset(that.env, that.value); }
    void reset(napi_env new_env, napi_value new_value)
    {
        reset();
        env = new_env;
        if (thrinx()) debug(YELLOW_MSG "wrong thread" ENDCOLOR);
        !NAPI_OK(napi_create_reference(env, new_value, 1, &ref), "Cre ref failed");
    }
};
#endif


//polyfill c++17 methods:
template <typename TYPE>
class my_vector: public std::vector<TYPE>
{
    using super = std::vector<TYPE>;
public:
    template <typename ... ARGS>
    TYPE& emplace_back(ARGS&& ... args)
    {
        super::emplace_back(std::forward<ARGS>(args) ...); //perfect fwd
        return super::back();
    }
};


////////////////////////////////////////////////////////////////////////////////
////
/// Gpu Port main code:
//

#ifdef WANT_REAL_CODE
 #ifdef SRC_NODE_API_H_ //USE_NAPI

#include "sdl-helpers.h" //AutoTexture, Uint32, now()

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
#define MHz  *1000000 //must be int for template param; //*1e6

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
template<typename TYPE>
static constexpr size_t cache_pad_typed(size_t count) { return cache_pad(count * sizeof(TYPE)) / sizeof(TYPE); }
#undef cache_pad
#define cache_pad  cache_pad_typed //kludge; override macro


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
    enum class Protocol: int32_t { NONE = 0, DEV_MODE, WS281X};
//public: //data members
//    /*txtr_bb*/ /*SDL_AutoTexture<XFRTYPE>*/ TXTR m_txtr; //in-memory copy of bit-banged node (color) values (formatted for protocol)
    Protocol protocol = Protocol::NONE; //WS281X;
//    static const int NUM_STATS = SIZEOF(TXTR::perf_stats);
//    typedef std::function<void(mySDL_AutoTexture* txtr)> REFILL; //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
    SYNCTYPE dirty; //= -1; //one Ready bit for each universe; init to bad value to force sync first time
    CONST SDL_Size wh/*(0, 0)*/, view; //(0, 0); //#univ, univ len for node values, display viewport
//TODO: use alignof here instead of cache_pad
    static const napi_typedarray_type perf_stats_type = napi_uint32_array; //NOTE: must match elapsed_t
    elapsed_t perf_stats[SIZEOF(TXTR::perf_stats) + 1] = {0}; //1 extra counter for my internal overhead; //, total_stats[SIZEOF(perf_stats)] = {0};
//put nodes last in case caller overruns boundary:
//TODO: use alignof() for node rows:
    NODEVAL nodes[NUM_UNIV][UNIV_MAXLEN_pad]; //node color values (max size); might not all be used; rows (univ) padded for better memory cache perf with multiple CPUs
public: //ctors/dtors
    explicit Nodebuf(): // /*napi_env env*/):
//        m_cached(env), //allow napi values to be inited
        m_xfr(std::bind(xfr_bb, std::ref(*this), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, SRCLINE)), //protocol bit-banger shim
        m_txtr(TXTR::NullOkay{}), //leave empty until bkg thread starts
        dirty(ALL_UNIV >> 1) //>> (NUM_UNIV / 2)) //init to intermediate value to force sync first time (don't use -1 in case all bits valid)
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
        for (int i = 0; i < SIZEOF(nodes) * SIZEOF(nodes[0]); ++i) nodes[0][i] = BLACK; //clear *entire* buf in case h < max and caller wants linear (1D) addressing
//        wh.h = new_wh.h; //cache_pad32(new_wh.h); //pad univ to memory cache size for better memory perf (multi-proc only)
//        m_debug3(m_view.h),
//TODO: don't recreate if already exists with correct size
//debug("here51" ENDCOLOR);
//{ DebugInOut("assgn new txtr", SRCLINE);
        SDL_Size txtr_wh(BIT_SLICES, wh.h);
        TXTR m_txtr2 = TXTR::create(NAMED{ _.wh = &txtr_wh; _.view_wh = &view, _.screen = screen; _.init_color = init_color; SRCLINE; });
        m_txtr = m_txtr2; //kludge: G++ thinks m_txtr is a ref so assign create() to temp first
//}
        debug(BLUE_MSG "txtr after reset " << m_txtr << ENDCOLOR);
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
        debug(BLUE_MSG "wrap nodebuf[%d]: cached " << retval << ", ret? %d" ENDCOLOR, count++, retval.type() == napi_object);
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
//            !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF(junk) * SIZEOF(junk[0]), arybuf.value, 0, &nodes.value), "Cre nodes typed array failed");
//            debug("nodes4 " << nodes << ENDCOLOR);
//        m_cached.object(env);
//        debug("cre typed array nodes wrapper " << wh << ENDCOLOR);
//        m_cached.cre_typed_ary(env, GPU_NODE_type, wh.w * wh.h, arybuf);
        napi_thingy typary(env, GPU_NODE_type, wh.w * wh.h, arybuf);
        debug(YELLOW_MSG "nodes5 " << typary << ENDCOLOR);
//        debug(YELLOW_MSG "nodes typed array created: &node[0][0] " << &/*gpu_wker->*/m_shdata.nodes[0][0] << ", #bytes " <<  commas(sizeof(/*gpu_wker->*/m_shdata.nodes)) << ", " << commas(SIZEOF(/*gpu_wker->*/m_shdata.nodes) * SIZEOF(/*gpu_wker->*/m_shdata.nodes[0])) << " " << NVL(TypeName(napi_uint32_array)) << " elements, arybuf " << arybuf << ", nodes thingy " << nodes << ENDCOLOR);
//        }
//        if (nodes.env != env) NAPI_exc("nodes env mismatch");
//        if (valp) *valp = m_cached.value;
//        *valp = wrapper;
        const int REF_COUNT = 1;
        !NAPI_OK(napi_create_reference(env, typary, REF_COUNT, &m_cached), "Cre ref failed"); //allow to be reused next time
        return typary;
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
        debug(BLUE_MSG "xfr " << xfrlen << " *3, protocol " << static_cast<int>(nodebuf.protocol) << ENDCOLOR);
        switch (nodebuf.protocol)
        {
            default: //NONE (raw)
                for (int y = 0; y < nodebuf.wh.h; ++y) //outer loop = node# within each universe
                    for (uint32_t x = 0, /*xofs = 0,*/ xmask = NODEVAL_MSB; x < NUM_UNIV; ++x, /*xofs += nodebuf.wh.h,*/ xmask >>= 1) //inner loop = universe#
                        *ptr++ = *ptr++ = *ptr++ = (dirty & xmask)? rbswap? ARGB2ABGR(nodebuf.nodes[x][/*xofs +*/ y]): nodebuf.nodes[x][/*xofs +*/ y]: BLACK; //copy as-is (3x width)
//                for (int xy = 0; xy < nodebuf.wh.w * nodebuf.wh.h; ++xy)
//                    *ptr++ = *ptr++ = *ptr++ = (dirty & xmask)? rbswap? ARGB2ABGR(nodebuf.nodes[0][xy]): nodebuf.nodes[0][xy]: BLACK; //copy as-is (3x width)
                break;
            case Protocol::DEV_MODE: //partially formatted
                for (int y = 0; y < nodebuf.wh.h; ++y) //outer loop = node# within each universe
                    for (uint32_t x = 0, /*xofs = 0,*/ xmask = NODEVAL_MSB; x < NUM_UNIV; ++x, /*xofs += nodebuf.wh.h,*/ xmask >>= 1) //inner loop = universe#
                    {
//show start + stop bits around unpivoted data:
                        *ptr++ = dirty; //WHITE;
                        *ptr++ = (dirty & xmask)? rbswap? ARGB2ABGR(nodebuf.nodes[x][/*xofs +*/ y]): nodebuf.nodes[x][/*xofs +*/ y]: BLACK; //unpivoted node values
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
                    memset(&ptr[yofs], 0, sizeof(bbdata));
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
    CONST /*elapsed_t*/ /*std::result_of<now_msec()>::type*/ decltype(now_msec()) started;
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
        started(now_msec())
        { reset(); }
public: //operators
//    bool ismine() const { if (owner != thrid()) debug(YELLOW_MSG "not mine: owner " << owner << " vs. me " << thrid() << ENDCOLOR); return (owner == thrid()); } //std::thread::get_id(); }
//    bool isvalid() const { return (sentinel == FRINFO_VALID); }
    /*double*/ int elapsed() const { return now_msec() - started; } //msec; //rdiv(now_msec() - started, 1000); } //.0; }
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
        started = now_msec();
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
        debug(BLUE_MSG "wrap frinfo[%d]: cached " << retval << ", ret? %d" ENDCOLOR, count++, retval.type() == napi_object);
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
            _.utf8name = "elapsed"; //"started";
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
                return napi_thingy(env, frdata->elapsed());
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
            _.utf8name = "times"; //NOTE: times[] will update automatically due to underlying array buf
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
    Nodebuf m_nodebuf;
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
    const elapsed_t m_started;
    const SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
public: //ctors/dtors
//    GpuPortData() = delete; //must have env so delete default
//    explicit GpuPortData(napi_env env, SrcLine srcline = 0): /*cbthis(env), nodes(env), frinfo(env),*/ m_started(now()), m_srcline(srcline)
    explicit GpuPortData(/*napi_env env,*/ SrcLine srcline = 0):
        m_frinfo(/*env,*/ m_nodebuf, std::bind([](const GpuPortData* aodata, napi_env env) -> bool { return aodata->isvalid(env, SRCLINE); }, this, std::placeholders::_1)), //m_nodebuf(env),
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
//set playback options:
private:
    struct
    {
        int screen; //= FIRST_SCREEN;
//    key_t PREALLOC_shmkey = 0;
        int vgroup; //= 1;
        int debug;
        Uint32 init_color; //= 0;
        /*Nodebuf::Protocol*/ int protocol;
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
    void reset(napi_env env)
    {
        m_nodebuf.reset(env);
        m_frinfo.reset(env);
    }
    static constexpr Nodebuf::TXTR* NO_ADVANCE = (Nodebuf::TXTR*)-1;
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
                debug(BLUE_MSG "find prop[%d/%d] %d:'%s' " << propname << ", value " << propval << ", found? %d" ENDCOLOR, i, listlen, buflen, buf, !!known_opts.find(buf));
                if (!known_opts.find(buf)) exc_soft("unrecognized option: '%s' " << propval, buf); //strlen(buf) - 2, &buf[1]);
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
            debug(BLUE_MSG "listen opts: screen %d, vgroup %d, init_color 0x%x, protocol %d, debug %d" ENDCOLOR, m_opts.screen, m_opts.vgroup, m_opts.init_color, m_opts.protocol, m_opts.debug);
//        if (islistening()) debug(RED_MSG "TODO: check for arg mismatch" ENDCOLOR);
        }
//void make_fats(napi_env env, napi_value jsfunc, napi_threadsafe_function_call_js napi_cb, napi_threadsafe_function* fats) //asynchronous thread-safe JavaScript call-back function; can be called from any thread
        make_fats(env, jsfunc, napi_cb, &m_opts.fats); //last arg = js cb func
        m_opts.refill = std::bind([](/*napi_env env,*/ GpuPortData* aodata, Nodebuf::TXTR* txtr)
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
        debug(PINK_MSG "start playback" ENDCOLOR);
//        UNIV_LEN(divup(/*m_cfg? m_cfg->vdisplay: UNIV_MAX*/ ScreenInfo(screen, NVL(srcline, SRCLINE))->bounds.h, vgroup)), //univ len == display height
//debug("here60" ENDCOLOR);
        m_nodebuf.reset(m_opts.screen, m_opts.vgroup, m_opts.init_color);
//TODO?        m_nodebuf.wrap(env);
//TODO?        m_frinfo.wrap(env);
//debug("here61" ENDCOLOR);
        if (!NAPI_OK(napi_acquire_threadsafe_function(m_opts.fats))) exc_hard("Can't acquire JS fats");
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
        debug(PINK_MSG "update playback" ENDCOLOR);
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
        debug(PINK_MSG "stop playback" ENDCOLOR);
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


//limit brightness:
//NOTE: JS <-> C++ overhead is significant for this function
//it's probably better to just use a pure JS function for high-volume usage
napi_value Limit_NAPI(napi_env env, napi_callback_info info)
{
    if (!env) return NULL; //Node cleanup mode?
    DebugInOut("Limit_napi", SRCLINE);

    GpuPortData* aoptr;
    napi_value argv[1+1], This; //allow 1 extra arg to check for extras
    size_t argc = SIZEOF(argv);
//    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, NULL, NULL), "Arg parse failed");
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aoptr), "Get cb info failed");
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
    color = limit<Nodebuf::BRIGHTEST>(color); //83% //= 3 * 212, //0xD4D4D4, //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA
//    napi_value retval;
//    !NAPI_OK(napi_create_uint32(env, color, &retval), "Cre retval failed");
//    return retval;
    return napi_thingy(env, color);
}


//convert results from wker thread to napi and pass to JavaScript callback:
//NOTE: this executes on Node main thread only
static void Listen_cb(napi_env env, napi_value jsfunc, void* context, void* data)
{
    UNUSED(context);
    if (!env) return; //Node cleanup mode
    DebugInOut("Listen_cb");

  // Retrieve the prime from the item created by the worker thread.
//    int the_prime = *(int*)data;
    GpuPortData* aoptr = static_cast<GpuPortData*>(data);
    debug(BLUE_MSG "listen js cb func: aodata %p, valid? %d, listening? %d, context %p, clup mode? %d" ENDCOLOR, aoptr, aoptr->isvalid(), aoptr->isvalid()? aoptr->islistening(): false, context, !env);
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
//    }
//HERE(3);
//    debug(BLUE_MSG "cb: check fats retval" ENDCOLOR);
//        /*int32_t*/ bool want_continue;
//static int count = 0; aodata->want_continue = (count++ < 7); return;
    !NAPI_OK(napi_coerce_to_number(env, retval/*.value*/, &num_retval.value), "Get retval as num failed");
//    debug(BLUE_MSG "cb: get bool %p" ENDCOLOR, aodata);
    !NAPI_OK(napi_get_value_uint32(env, num_retval/*.value*/, &ready_bits), "Get uint32 retval failed");
    debug(BLUE_MSG "js fats: got retval " << retval << " => num " << num_retval << " => ready bits 0x%x, new dirty 0x%x, continue? %d" ENDCOLOR, ready_bits, aoptr->dirty | ready_bits, !!ready_bits);
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
}


napi_value Listen_NAPI(napi_env env, napi_callback_info info)
{
//    elapsed_t started = now_msec();
    if (!env) return NULL; //Node cleanup mode?
    DebugInOut("Listen_napi", SRCLINE);

    GpuPortData* aoptr;
    napi_value argv[2+1], This; //allow 1 extra arg to check for extras
    size_t argc = SIZEOF(argv);
//    if (!env) return NULL; //Node cleanup mode
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, (void**)&aoptr), "Get cb info failed");
    debug(CYAN_MSG "async listen loop: aoptr %p, valid? %d" ENDCOLOR, aoptr, aoptr->isvalid());
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
        debug(PINK_MSG "bkg: aodata %p, valid? %d" ENDCOLOR, aoptr, aoptr->isvalid());
//        debug(YELLOW_MSG "bkg acq" ENDCOLOR);
//        !NAPI_OK(napi_acquire_threadsafe_function(aoptr->fats), "Can't acquire JS fats");
//        !NAPI_OK(napi_reference_ref(env, aodata->listener.ref, &ref_count), "Listener ref failed");
//        aoptr->islistening(true);
        aoptr->start(); //env);
        try
        {
            for (;;) //int i = 0; i < 5; ++i)
            {
                DebugInOut("call fats for fr# " << aoptr->/*m_frinfo.*/numfr.load() << ", wait for 0x" << std::hex << Nodebuf::ALL_UNIV << std::dec << " (blocking)", SRCLINE);
//                !NAPI_OK(napi_call_threadsafe_function(aoptr->fats, aoptr, napi_tsfn_blocking), "Can't call JS fats");
//            while (aoptr->islistening()) //break; //allow cb to break out of playback loop
//    typedef std::function<bool(void)> CANCEL; //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
                aoptr->dirty.wait(Nodebuf::ALL_UNIV, [aoptr](){ return !aoptr->islistening(); }, true, SRCLINE); //CAUTION: blocks until al univ ready or caller cancelled
//                const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE};
//                for (int i = 0; i < SIZEOF(aoptr->m_nodebuf.nodes); ++i) aoptr->m_nodebuf.nodes[0][i] = palette[aoptr->numfr.load() % SIZEOF(palette)];
//                if (aoptr->numfr.load() >= 10) break;
                debug(BLUE_MSG "bkg woke from fr# %d with ready 0x%x, caller listening? %d" ENDCOLOR, aoptr->numfr.load(), aoptr->dirty.load(), aoptr->islistening());
                if (!aoptr->islistening()) break;
                SDL_Delay(1 sec);
                aoptr->update(); //env);
            }
//            SDL_Delay(1 sec);
//https://stackoverflow.com/questions/233127/how-can-i-propagate-exceptions-between-threads
        }
        catch (...)
        {
            aoptr->excptr = std::current_exception(); //allow main thread to rethrow
            debug(RED_MSG "bkg wker exc: %s" ENDCOLOR, aoptr->exc_reason().c_str());
//            aoptr->islistening(false); //listener.busy = true; //(void*)1;
        }
        aoptr->stop(); //env);
//        !NAPI_OK(napi_release_threadsafe_function(aoptr->fats, napi_tsfn_release), "Can't release JS fats");
//        aoptr->fats = NULL;
//        aodata->listener.busy = false; //work = 0;
        debug(YELLOW_MSG "bkg exit after %d frames" ENDCOLOR, aoptr->/*m_frinfo.*/numfr.load());
    });
    bkg.detach();
//#endif
//debug("here84" ENDCOLOR);
    aoptr->dirty.wait(0, NULL, true, SRCLINE); //block until bkg thread starts so frinfo values will be filled in; *don't* check islistening() - bkg thread might not be running yet
    debug(BLUE_MSG "sync verify: woke up with dirty = 0x%x" ENDCOLOR, aoptr->dirty.load());
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


//export napi functions to js caller:
napi_value GpuModuleInit(napi_env env, napi_value exports)
{
    DebugInOut("ModuleInit", SRCLINE);
    uint32_t napi_ver;
    const napi_node_version* ver;
    !NAPI_OK(napi_get_node_version(env, &ver), "Get node version info failed");
    !NAPI_OK(napi_get_version(env, &napi_ver), "Get napi version info failed");
    debug(BLUE_MSG "using Node v" << ver->major << "." << ver->minor << "." << ver->patch << " (" << ver->release << "), napi v" << napi_ver << ENDCOLOR);
    exports = module_exports(env, exports); //include previous exports

    std::unique_ptr<GpuPortData> aodata(new GpuPortData(/*env,*/ SRCLINE)); //(GpuPortData*)malloc(sizeof(*addon_data));
    GpuPortData* aoptr = aodata.get();
    inout.checkpt("cre data");
//    aoptr->isvalid(env);
//expose methods for caller to use:
//    my_napi_property_descriptor props[7], *pptr = props; //TODO: use std::vector<>
    my_vector<my_napi_property_descriptor> props;
//    memset(&props[0], 0, sizeof(props)); //clear first so NULL members don't need to be explicitly set below
//kludge: use lambas in lieu of C++ named member init:
//(named args easier to maintain than long param lists)
    [/*env,*/ aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "limit"; //TODO: add prop to show <pct>?
        _.method = Limit_NAPI;
        _.attributes = napi_default; //!writable, !enumerable
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [/*env,*/ aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "listen";
        _.method = Listen_NAPI;
        _.attributes = napi_default; //!writable, !enumerable
        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "NUM_UNIV";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.w/*NUM_UNIV*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, Nodebuf::NUM_UNIV); //aoptr->m_nodebuf.wh.w);
        _.attributes = napi_default; //!writable, !enumerable
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "UNIV_MAXLEN";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, Nodebuf::UNIV_MAXLEN_pad); //give caller actual row len for correct node addressing
        _.attributes = napi_default; //!writable, !enumerable
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
//expose Protocol types:
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "NONE";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, static_cast<int32_t>(Nodebuf::Protocol::NONE));
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "DEV_MODE";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, static_cast<int32_t>(Nodebuf::Protocol::DEV_MODE));
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "WS281X";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, static_cast<int32_t>(Nodebuf::Protocol::WS281X));
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
    [env, aoptr](auto& _) //napi_property_descriptor& _) //kludge: use lamba in lieu of C++ named member init
    {
        _.utf8name = "version";
//        !NAPI_OK(napi_create_int32(env, aoptr->m_nodebuf.wh.h/*UNIV_MAXLEN_pad*/, &_.value), "Cre w int failed");
        _.value = napi_thingy(env, "0.11.18");
        _.attributes = napi_enumerable; //read-only; //napi_default;
//        _.data = aoptr;
    }(props.emplace_back()); //(*pptr++);
//TODO: define primary colors?
//decorate exports with the above-defined properties:
//    if (pptr - props > SIZEOF(props)) NAPI_exc("prop ary overflow: needed " << (pptr - props) << ", only had space for " << SIZEOF(props));
//    !NAPI_OK(napi_define_properties(env, exports, pptr - props, props), "export method props failed");
    debug("export %d props" ENDCOLOR, props.size());
    !NAPI_OK(napi_define_properties(env, exports, props.size(), props.data()), "export method props failed");

//wrap internal data with module exports object:
  // Associate the addon data with the exports object, to make sure that when the addon gets unloaded our data gets freed.
//    void* NO_HINT = NULL; //optional finalize_hint
    napi_ref* NO_REF = NULL; //optional ref to wrapped object
// Free the per-addon-instance data.
    napi_finalize /*std::function<void(napi_env, void*, void*)>*/ addon_final = [](napi_env env, void* data, void* hint)
    {
        UNUSED(hint);
        GpuPortData* aoptr = static_cast<GpuPortData*>(data); //(GpuPortData*)data;
//    if (!env) return; //Node cleanup mode
        debug(RED_MSG "addon final: aodata %p, valid? %d, hint %p" ENDCOLOR, aoptr, aoptr->isvalid(), hint);
        aoptr->isvalid(env);
        if (aoptr->islistening()) NAPI_exc("Listener still active");
        aoptr->reset(env);
        delete aoptr; //free(addon_data);
    };
    !NAPI_OK(napi_wrap(env, exports, aoptr, addon_final, /*aoptr.get()*/NO_HINT, /*&aodata->aoref*/ NO_REF), "Wrap aodata failed");
  // Return the decorated exports object.
//    napi_status status;
    INSPECT(CYAN_MSG "napi init: " << *aoptr, SRCLINE);
    aoptr->isvalid(env);
    aodata.release(); //NAPI owns it now
    return exports;
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
        debug(RED_MSG "thread final: aodata %p, valid? %d, hint %p" ENDCOLOR, aodata, aodata->isvalid(), hint);
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
            debug("arybuf1 " << arybuf << ENDCOLOR);
            !NAPI_OK(napi_create_int32(env, 1234, &arybuf.value), "cre int32 failed");
            debug("arybuf2 " << arybuf << ENDCOLOR);
            !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &arybuf.value), "cre str failed");
            debug("arybuf3 " << arybuf << ENDCOLOR);
            !NAPI_OK(napi_create_external_arraybuffer(env, &junk[0][0], sizeof(junk), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
            debug("arybuf4 " << arybuf << ENDCOLOR);
            !NAPI_OK(napi_create_external_arraybuffer(env, &/*gpu_wker->*/m_shdata.nodes[0][0], sizeof(/*gpu_wker->*/m_shdata.nodes), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
            debug("arybuf5 " << arybuf << ENDCOLOR);

            debug("nodes1 " << nodes << ENDCOLOR);
            !NAPI_OK(napi_create_int32(env, 1234, &nodes.value), "cre int32 failed");
            debug("nodes2 " << nodes << ENDCOLOR);
            !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &nodes.value), "cre str failed");
            debug("nodes3 " << nodes << ENDCOLOR);
            !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF(junk) * SIZEOF(junk[0]), arybuf.value, 0, &nodes.value), "Cre nodes typed array failed");
            debug("nodes4 " << nodes << ENDCOLOR);
            !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF(/*gpu_wker->*/m_shdata.nodes) * SIZEOF(/*gpu_wker->*/m_shdata.nodes[0]), arybuf.value, 0, &nodes.value), "Cre nodes typed array failed");
            debug("nodes5 " << nodes << ENDCOLOR);
            debug(YELLOW_MSG "nodes typed array created: &node[0][0] " << &/*gpu_wker->*/m_shdata.nodes[0][0] << ", #bytes " <<  commas(sizeof(/*gpu_wker->*/m_shdata.nodes)) << ", " << commas(SIZEOF(/*gpu_wker->*/m_shdata.nodes) * SIZEOF(/*gpu_wker->*/m_shdata.nodes[0])) << " " << NVL(TypeName(GPU_NODE_type)) << " elements, arybuf " << arybuf << ", nodes thingy " << nodes << ENDCOLOR);
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
    debug(BLUE_MSG "call listen fats: aodata %p, valid? %d, context %p, clup mode? %d" ENDCOLOR, aodata, aodata->isvalid(), context, !env);
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
        debug(BLUE_MSG "listen opts: screen %d, shmkey %lx, vgroup %d, init_color 0x%x" ENDCOLOR, screen, PREALLOC_shmkey, vgroup, init_color);
        if (aodata->gpu_wker) debug(RED_MSG "TODO: check for arg mismatch" ENDCOLOR);
    }
    PREALLOC_shmkey = shmkey(&aodata->m_shdata);
    debug(YELLOW_MSG "override shmkey with prealloc key 0x%x" ENDCOLOR, PREALLOC_shmkey);
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
    debug(BLUE_MSG "using Node v" << ver->major << "." << ver->minor << "." << ver->patch << " (" << ver->release << "), napi " << napi_ver << ENDCOLOR);
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
#if 1
//    void nodes_setup(napi_env env, napi_value* valp)
//        if (nodes.arytype() != napi_uint32_array) //napi_typedarray_type)
//            nodes.env = env;
    Uint32 junk[3][5];
    napi_thingy arybuf(env), nodes(env);
    debug("ary buf1 " << arybuf << ENDCOLOR);
    !NAPI_OK(napi_create_int32(env, 1234, &arybuf.value), "cre int32 failed");
    debug("ary buf2 " << arybuf << ENDCOLOR);
    !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &arybuf.value), "cre str failed");
    debug("ary buf3 " << arybuf << ENDCOLOR);
    !NAPI_OK(napi_create_external_arraybuffer(env, &junk[0][0], sizeof(junk), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
    debug("ary buf4 " << arybuf << ENDCOLOR);
    !NAPI_OK(napi_create_external_arraybuffer(env, &aodata->m_shdata.nodes[0][0], sizeof(aodata->m_shdata.nodes), /*wker_check*/ NULL, NO_HINT, &arybuf.value), "Cre arraybuf failed");
    debug("shdata arybuf " << arybuf << ENDCOLOR);

    debug("nodes1 " << nodes << ENDCOLOR);
    !NAPI_OK(napi_create_int32(env, 1234, &nodes.value), "cre int32 failed");
    debug("nodes2 " << nodes << ENDCOLOR);
    !NAPI_OK(napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &nodes.value), "cre str failed");
    debug("nodes3 " << nodes << ENDCOLOR);
    !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF(junk) * SIZEOF(junk[0]), arybuf, 0, &nodes.value), "Cre nodes typed array failed");
    debug("nodes4 " << nodes << ENDCOLOR);
    !NAPI_OK(napi_create_typedarray(env, GPU_NODE_type, SIZEOF(aodata->m_shdata.nodes) * SIZEOF(aodata->m_shdata.nodes[0]), arybuf, 0, &nodes.value), "Cre nodes typed array failed");
    debug("shdata nodes " << nodes << ENDCOLOR);
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
    debug(YELLOW_MSG "cb arybuf " << napi_thingy(env, arybuf) << ", typed ary " << napi_thingy(env, argv[1]) << ENDCOLOR);

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