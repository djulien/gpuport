//N-API helpers

#ifndef _NAPI_HELPERS_H
#define _NAPI_HELPERS_H

//decide which Node API to use:
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
#define NAPI_exc_4ARGS(env, errcode, msg, srcline)  (napi_throw_error(env, errcode, std::ostringstream() << RED_MSG << msg << ": " << NAPI_ErrorMessage(env) << ATLINE(srcline) << ENDCOLOR_NOLINE), 0) //dummy "!okay" or null ptr result to allow usage in conditional expr; throw() won't fall thru at run-time, though
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
        if ((type() != chk_type) || !subtypeok) exc_hard("failed to create " << TypeName(chk_type));
//        else debug(BLUE_MSG << TypeName(chk_type) << " created ok" << ENDCOLOR);
    }
};
//napi_thingy;


//add named prop value to property descriptor array:
#define add_prop_1ARG(var)  add_prop(#var, var)
#define add_prop_2ARGS(name, value)  add_prop_3ARGS(name, value, napi_enumerable)
#define add_prop_3ARGS(name, value, attrs)  NAMED{ _.utf8name = name; _.value = napi_thingy(env, value); _.attributes = attrs; } //(props.emplace_back()); //(*pptr++);
#define add_prop(...)  UPTO_3ARGS(__VA_ARGS__, add_prop_3ARGS, add_prop_2ARGS, add_prop_1ARG) (__VA_ARGS__)

//add method to property descriptor array:
#define add_method_1ARG(func)  add_method_2ARGS(#func, func) //(props.emplace_back()); //(*pptr++);
#define add_method_2ARGS(name, func)  add_method_3ARGS(name, func, nullptr)
#define add_method_3ARGS(name, func, aodata)  NAMED{ _.utf8name = name; _.method = func; _.attributes = napi_default; _.data = aodata; } //(props.emplace_back()); //(*pptr++);
#define add_method(...)  UPTO_3ARGS(__VA_ARGS__, add_method_3ARGS, add_method_2ARGS, add_method_1ARG) (__VA_ARGS__)

#define add_getter_1ARG(func)  add_getter_2ARGS(#func, func) //(props.emplace_back()); //(*pptr++);
#define add_getter_2ARGS(name, func)  add_getter_3ARGS(name, func, nullptr)
#define add_getter_3ARGS(name, func, aodata)  add_getter_4ARGS(name, func, aodata, napi_enumerable)
#define add_getter_4ARGS(name, func, aodata, attrs)  NAMED{ _.utf8name = name; _.getter = std::bind(getter, std::placeholders::_1, std::placeholders::_2, func); _.attributes = attrs; _.data = aodata; } //(props.emplace_back()); //(*pptr++);
#define add_getter(...)  UPTO_4ARGS(__VA_ARGS__, add_getter_4ARGS, add_getter_3ARGS, add_getter_2ARGS, add_getter_1ARG) (__VA_ARGS__)

//wrapper for getter:
napi_value getter(napi_env env, napi_callback_info info, napi_value (*get_value)(napi_env, void*))
{
    void* data;
    napi_value argv[0+1], This;
    size_t argc = SIZEOF(argv);
    if (!env) return NULL; //Node cleanup mode?
    struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name
    !NAPI_OK(napi_get_cb_info(env, info, &argc, argv, &This, &data), "Getter info extract failed");
//                    GpuPortData* aodata = static_cast<GpuPortData*>(data);
//                    return aodata->wker_ok(env)->m_frinfo.protocol;
    return get_value(env, data);
}


//void add_prop(napi_env env, vector_cxx17<my_napi_property_descriptor>& props, const char* name, napi_value value)
//{
//}


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
#endif //def SRC_NODE_API_H_ //USE_NAPI


#endif //ndef _NAPI_HELPERS_H

//eof