////////////////////////////////////////////////////////////////////////////////
////
/// String helpers:
//

#if !defined(_STR_HELPERS_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _STR_HELPERS_H //CAUTION: put this before defs to prevent loop on cyclic #includes

//NOTE from https://stackoverflow.com/questions/1277627/overhead-of-pthread-mutexes
//about atomic: In practice, you can assume that int and other integer types no longer than int are atomic. You can also assume that pointer types are atomic
// http://axisofeval.blogspot.com/2010/11/numbers-everybody-should-know.html
//go ahead and use std::atomic<> anyway, for safety:
#include <atomic> //std::atomic. std::memory_order
#include <algorithm> //std::min<>(), std::max<>()
#include <string.h> //snprintf()
#include <memory.h> //memmove()
#include <map> //std::map<>
#include <stdio.h> //fflush(), snprintf()
#include <vector>
#include <sstream> //std::ostream, std::ostringstream
//#include <iostream> //std::cout
#include <iomanip> //std::setw()
#include <utility> //std::pair<>
//#include <regex> //std::regex, std::regex_match()
#include <cctype> //isxdigit()
#define __STDC_FORMAT_MACROS //https://stackoverflow.com/questions/9225567/how-to-print-a-int64-t-type-in-c
#include <inttypes.h> //PRI*** printf masks; https://en.cppreference.com/w/cpp/types/integer


#ifndef STATIC
 #define STATIC //should be static but compiler doesn't allow
#endif


#ifndef SIZEOF
 #define SIZEOF(thing)  /*int*/(sizeof(thing) / sizeof((thing)[0])) //should be unsigned (size_t)
#endif


#ifndef TOSTR
 #define TOSTR(str)  TOSTR_NESTED(str)
 #define TOSTR_NESTED(str)  #str
#endif


//pass char array (unterminated) in place of null-terminated string:
struct substr //StringFragment
{
    const char* str;
    size_t len;
//ctors/dtors:
    substr(const substr& that): substr(that.str, that.len) {} //delegated
    explicit substr(const char* other_str, size_t other_len = 0): str(other_str), len(other_len? other_len: other_str? strlen(other_str): 0) {}
//    substr(const char* match_str, const std::sub_match& match): str(match_str + match.position()), len(match.length()) {}
//operators:
    inline bool operator!=(const substr& that) const { return !(*this == that); }
    inline bool operator==(const char* that_str) const { return *this == substr(that_str); }
    inline bool operator==(const substr& that) const { return (len == that.len) && (!len || !strncmp(str, that.str, len)); }
//    {
//        /*if (!len)*/ int that_len = that_str? strlen(that_str): 0;
//        return ((other_len == len) && !strcmp(other)str, str, len);
//    }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const substr& that) //dummy_shared_state) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
//str len no worky        return ostrm << that.len << ":'" << std::setw(that.len) << that.str << "'";
        return ostrm << that.len << ":'" << std::string(that.str, that.len) << "'";
    }
};


//inline char* skipspaces(const char* str) { return static_cast<char*>(skipspaces(str)); }
//inline const char* skipspaces(const char* str)
template <typename CHTYPE = const char>
inline CHTYPE* skipspaces(CHTYPE* str)
{
    if (str)
        while (isspace(*str)) ++str;
    return str;
}

//get end of string:
//use inline function instead of macro to avoid side effects if str is dynamic
inline const char* strend(const char* str)
{
    return str? str + strlen(str): NULL;
}


//right-most offset of char within string:
inline size_t strrofs(const char* str, char ch)
{
    const char* bp = strrchr(str, ch);
//if (!bp) printf("!strrofs('%s', '%c')\n", str, ch);
    return bp? bp - str: -1;
}


/*const*/ char* strnstr(/*const*/ char* str, const char* substr, size_t cmplen)
{
    for (;;)
    {
        /*const*/ char* found = strchr(str++, *substr);
        if (!found || !strncmp(found, substr, cmplen)) return found; //return no or full match
    }
}


//lookup table (typically int -> string):
//use this function instead of operator[] on const maps (operator[] is not const - will add an entry)
template <typename MAP, typename KEY> //... ARGS>
inline const char* unmap(MAP&& map, KEY&& key) //Uint32 value) //ARGS&& ... args)
//template <typename KEY, typename VAL> //... ARGS>
//inline const char* unmap(const std::map<KEY, VAL>&& map, KEY&& key) //Uint32 value) //ARGS&& ... args)
{
    return map.count(key)? map.find(key)->second: NULL;
}
//template <typename KEY, typename VAL> //... ARGS>
//VAL unmap(const std::map<KEY, VAL>& map, VAL&& value)
//{
//    return map.count(value)? map.find(value)->second: "";
//}
//const std::map<Uint32, const char*> SDL_SubSystems =
//    SDL_AutoSurface SDL_CreateRGBSurfaceWithFormat(ARGS&& ... args, SrcLine srcline = 0) //UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
//        return SDL_AutoSurface(::SDL_CreateRGBSurfaceWithFormat(std::forward<ARGS>(args) ...), srcline); //perfect fwd


//custom lookup map:
//std::map looks for exact key match so it won't work with string keys
//define a map to use strcmp instead:
//using KEYTYPE = const char*;
template <typename KEYTYPE = const char*, typename VALTYPE = int>
class str_map //: public std::vector<std::pair<KEYTYPE, VALTYPE>>
{
    using PAIRTYPE = std::pair<KEYTYPE, VALTYPE>;
    std::vector<PAIRTYPE> m_vec;
public: //ctors/dtors
//    str_map(KEYTYPE key, VALTYPE val) {}
//initializer list example: https://en.cppreference.com/w/cpp/utility/initializer_list
    str_map(std::initializer_list<PAIRTYPE> il): m_vec(il) {}
public: //methods
    inline auto begin() { return m_vec.begin(); }
    inline auto end() { return m_vec.end(); }
    inline auto cbegin() const { return m_vec.cbegin(); }
    inline auto cend() const { return m_vec.cend(); }
    /*std::pair<KEYTYPE, VALTYPE>*/ const PAIRTYPE* find(KEYTYPE key, int def = -1) const
    {
//printf("here10\n"); fflush(stdout);
//        static int count = 0;
//        if (!count++)
//            for (auto it = m_vec.begin(); it != m_vec.end(); ++it)
//            { printf("here14 %s %d\n", it->first, it->second); fflush(stdout); }
//        for (auto pair : m_vec) //*this)
//        for (auto it = m_vec.begin(); it != m_vec.end(); ++it)
//            printf("str_map: cmp '%s' to '%s': %d\n", key, it->first, strcmp(key, it->first));
        for (auto it = m_vec.begin(); it != m_vec.end(); ++it)
            if (!strcmp(key, it->first)) return &*it; //use strcmp rather than ==
//        return (def < 0)? 0: &this[def];
//printf("here11\n"); fflush(stdout);
//        printf("ret def %d\n", def);
        if (def < 0) return 0;
//printf("here12\n"); fflush(stdout);
//        return this + def; //(*this)[def];
//        return &(operator[](def));
        return &m_vec[def]; //&(*this)[def]; //return ptr to default entry
    };
};


//std::map<void*, ShmHdr> hdrs; //store hdrs in heap; owner is only proc that needs info anyway
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


//printf-like formatter for c++ ostreams
//custom stream manipulator for printf-style formating:
//idea from https://stackoverflow.com/questions/535444/custom-manipulator-for-c-iostream
//and https://stackoverflow.com/questions/11989374/floating-point-format-for-stdostream
//std::ostream& fmt(std::ostream& out, const char* str)
class FMT
{
public: //ctor
    explicit FMT(const char* fmt): m_fmt(fmt) {}
private:
    class fmter //actual worker class
    {
    public:
        /*explicit*/ fmter(std::ostream& strm, const FMT& fmt): m_strm(strm), m_fmt(fmt.m_fmt) {}
//output next object (any type) to stream:
        template<typename TYPE>
        std::ostream& operator<<(const TYPE& value)
        {
//            return m_strm << "FMT(" << m_fmt << "," << value << ")";
            char buf[32]; //enlarge as needed
            int needlen = snprintf(buf, sizeof(buf), m_fmt, value);
//            buf[sizeof(buf) - 1] = '\0'; //make sure it's delimited (shouldn't be necessary)
//printf(" [fmt: len %d for '%s' too big? %d] ", needlen, buf, needlen >= sizeof(buf));
            if (needlen < sizeof(buf)) { m_strm << buf; return m_strm; } //fits ok
            char* bigger = new char[needlen + 1];
            snprintf(bigger, needlen + 1, m_fmt, value); //try again
//printf("fmt: len %d too big? %d\n", needlen, needlen >= sizeof(buf));
            m_strm << bigger;
            delete bigger;
            return m_strm;
        }
    private:
        std::ostream& m_strm;
        const char* m_fmt;
    };
    const char* m_fmt; //save fmt string for inner class
//kludge: return derived stream to allow operator overloading:
    friend FMT::fmter operator<<(std::ostream& strm, const FMT& fmt)
    {
        return FMT::fmter(strm, fmt);
    }
};


//return default string instead of null:
//use function to avoid evaluating params > 1x
//#define NVL(str, defval)  ((str)? (str): (defval)? (defval): "(null")
//inline const char* NVL(const char* str, const char* defval = 0) { return str? str: defval? defval: "(null)"; }
//use template to allow different types
//TODO: rename to nonnull or something like that
template <typename TYPE>
inline TYPE NVL(const TYPE val, const TYPE defval = 0)
{
    static const TYPE NONE = 0;
    return val? val: defval? defval: NONE;
}
//template <> NONE<const char*> = "(none)";
//_GLIBCXX14_CONSTEXPR inline const VALTYPE& NVL(const VALTYPE& val, const VALTYPE& defval = DEFVAL) { return val? val: defval? defval: DEFVAL; }
//    template <typename ... ARGS>
//    SDL_AutoSurface SDL_CreateRGBSurfaceWithFormat(ARGS&& ... args, SrcLine srcline = 0) //UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
//        return SDL_AutoSurface(::SDL_CreateRGBSurfaceWithFormat(std::forward<ARGS>(args) ...), srcline); //perfect fwd


//express 4 digit dec number in hex:
//ie, 1234 dec becomes 0x1234
constexpr uint32_t NNNN_hex(uint32_t val)
{
    return (((val / 1000) % 10) * 0x1000) | (((val / 100) % 10) * 0x100) | (((val / 10) % 10) * 0x10) | (val % 10);
}


//for grammatically correct msgs: :)
inline const char* plural(int count, const char* suffix = "s", const char* singular = 0)
{
//    return "(s)";
    return (count != 1)? suffix: singular? singular: "";
}


//skip over first part of string if it matches another:
inline const char* skip_prefix(const char* str, const char* prefix)
{
//    size_t preflen = strlen(prefix);
//    return (str && !strncmp(str, prefix, preflen))? str + preflen: str;
    if (str)
        while (*str == *prefix++) ++str;
    return str;
}


int numlines(const char* str)
{
    if (!str) return -1;
    int count = *str? 1: 0;
    while (str = strchr(str, '\n')) { ++str; ++count; }
    return count;
}


#if 0
//insert commas into a numeric string (for readability):
//CAUTION: uses static data to preserve data after return; semaphore arbitrates a pool of 12 ret values
const char* commas(int64_t val)
{
    const int LIMIT = 4; //max #commas to insert
    thread_local static int ff; //std::atomic<int> ff; //use TLS to avoid the need for mutex (don't need atomic either)
    thread_local static char buf[12][16 + LIMIT]; //allow 12 simultaneous calls (each thread if using TLS)
//    static auto_ptr<SDL_sem> acquire(SDL_CreateSemaphore(SIZE(buf)));
//    auto_ptr<SDL_LockedSemaphore> lock_HERE(acquire.cast); //SDL_LOCK(acquire));

    char* bufp = buf[++ff % SIZEOF(buf)] + LIMIT; //alloc ret val from pool; don't overwrite other values within same printf, allow space for commas
    for (int grplen = std::min(sprintf(bufp, "%ld", val), LIMIT * 3) - 3; grplen > 0; grplen -= 3)
    {
        memmove(bufp - 1, bufp, grplen);
        (--bufp)[grplen] = ',';
    }
    return bufp;
}
//TODO
const char* commas(double val)
{
//TODO
    return commas((int64_t)val);
}
#elif 1
template <int BUFLEN, int COUNT>
char* StaticPoolBuf()
{
    static char m_pool[COUNT][BUFLEN];
    static int m_ff; //init value doesn't matter - pool is circular
//static int count = 0;
//if (!count++) printf("pool %p %p %p, [%d], %d\n", m_pool[0], m_pool[1], m_pool[2], ff % SIZEOF(m_pool), SIZEOF(m_pool));
//printf(" >>pool[%d/%d] %p<< ", m_ff, SIZEOF(m_pool), m_pool[(m_ff + 1) % SIZEOF(m_pool)]);
    return m_pool[++m_ff % SIZEOF(m_pool)];
}


//insert commas into a numeric string (for readability):
//CAUTION: uses static data to preserve data after return; semaphore arbitrates a pool of 12 ret values
/*const*/ char* commas(char* const buf, int GRPLEN) //= 3) //don't default; use different signature to avoid incorrect usage
{
//buflen  #commas
// 0 - 3   0
// 4 - 6   1
// 7 - 9   2
//printf("commas('%s', %d) => ", buf, GRPLEN);
    char* bufend = buf + strlen(buf);
    char* sep = strrchr(buf, '.');
    for (char* bp = (sep? sep: bufend) - GRPLEN; bp > buf; bp -= GRPLEN)
    {
//        printf("buf[%d]: '%c' is hex digit? %d\n", bp - buf, *bp, !isxdigit(*bp));
        if (!isxdigit(bp[-1])) break;
        memmove(bp + 1, bp, ++bufend - bp); //CAUTION: incl null term
        *bp = ',';
    }
//printf("'%s' @%d\n", buf, __LINE__);
    return buf;
}

/*const*/ char* commas(void* ptr, const char* fmt = "%p") //https://en.cppreference.com/w/cpp/types/integer
{
    static const int GRPLEN = 4;
    static const int NUMBUF = 4;
    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, fmt, ptr);
//printf(" >>%p %s ptr %p<< ", bufp, bufp, ptr);
    return commas(bufp, GRPLEN);
}

/*const*/ char* commas(double val, const char* fmt = "%f")
{
    static const int GRPLEN = 3;
    static const int NUMBUF = 12;
    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, fmt, val);
//printf(" >>%p %s dbl %f<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}

/*const*/ char* commas(uint32_t val, const char* fmt = "%" PRIu32) //"%lu")
{
    static const int GRPLEN = 3;
    static const int NUMBUF = 12;
    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, fmt, val);
//printf(" >>%p %s ui %" PRIu32 "<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}

//for printf masks see https://stackoverflow.com/questions/9225567/how-to-print-a-int64-t-type-in-c
/*const*/ char* commas(/*int32_t*/ int64_t val, const char* fmt = "%" PRId64) //"%ld")
{
    static const int GRPLEN = 3;
    static const int NUMBUF = 12*2;
    static const int BUFLEN = 20*2;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, fmt, val);
//printf(" >>%p %s i %" PRId64 "<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}

//kludge: resolve ambigous calls by providing exact signatures:
char* commas(int32_t val) { return commas((int64_t)val); } //, const char* fmt = "%ld") { return commas((int64_t)val, fmt); }
//#ifdef sizeof(size_t) != sizeof(uint32_t)
#ifdef __ARMEL__ //RPi //__arm__
char* commas(long int val) { return commas((int64_t)val); } //, const char* fmt = "%ld") { return commas((int64_t)val, fmt); }
char* commas(uint64_t val) { return commas((int64_t)val); } //, const char* fmt = "%ld") { return commas((int64_t)val, fmt); }
#else //need this on RPi:
char* commas(size_t val) { return commas((uint32_t)val); } //, const char* fmt = "%lu") { return commas((uint32_t)val, fmt); }
#endif

#elif 0
//kludge: use template + specialization to avoid "ambiguous" errors:
template <typename TYPE, int GRPLEN = 3, int NUMBUF = 4, int BUFLEN = 20>
/*const*/ char* commas(TYPE&& val)
{
//    static const int GRPLEN = 4;
//    static const int NUMBUF = 4;
//    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    strncpy(bufp, "value", BUFLEN);
    return commas(bufp, GRPLEN);
}

template<>
/*const*/ char* commas<void*, 4, 4, 20>(void* ptr)
{
//    static const int GRPLEN = 4;
//    static const int NUMBUF = 4;
//    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%p", ptr);
printf(" >>%p %s ptr %p<< ", bufp, bufp, ptr);
    return commas(bufp, GRPLEN);
}

template<>
/*const*/ char* commas<double, 3, 12, 20>(double val)
{
//    static const int GRPLEN = 3;
//    static const int NUMBUF = 12;
//    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%f", val);
printf(" >>%p %s dbl %f<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}

template<>
/*const*/ char* commas<uint32_t, 3, 12, 20>(uint32_t val)
{
//    static const int GRPLEN = 3;
//    static const int NUMBUF = 12;
//    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%lu", val);
printf(" >>%p %s ui %lu<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}

template<>
/*const*/ char* commas<int32_t, 3, 12, 20>(int32_t val)
{
//    static const int GRPLEN = 3;
//    static const int NUMBUF = 12;
//    static const int BUFLEN = 20;
    char* bufp = /*new*/ StaticPoolBuf<BUFLEN * (GRPLEN + 1) / GRPLEN, NUMBUF>();
    snprintf(bufp, BUFLEN, "%ld", val);
printf(" >>%p %s i %ld<< ", bufp, bufp, val);
    return commas(bufp, GRPLEN);
}
#endif


//#include <regex>
#include <vector>

#define TEMPL_ARGS  templ_args(__PRETTY_FUNCTION__)
//https://bytes.com/topic/c/answers/878171-any-way-print-out-template-typename-value
//typeid(TYPE).name() can't use with -fno-rtti compiler flag

//get template arg types from __PRETTY_FUNC__
//example: AutoShmary<TYPE, WANT_MUTEX>::AutoShmary(size_t, key_t, SrcLine) [with TYPE = unsigned int; bool WANT_MUTEX = false; size_t = unsigned int; key_t = int; SrcLine = const char*] 
/*const char* */std::string/*&*/ templ_args(const char* str)
{
    const char* svstr = str;
//printf("get args from: %s\n", str);
    if (!(str = strchr(str, '<'))) return "????"; //svstr;
//    const char* name_start = strchr(str, '<');
//    if (!name_start++) return str;
//    const char* name_end = strchr(name_start, '>');
//    if (!name_end) return str;
//string split examples: https://stackoverflow.com/questions/1894886/parsing-a-comma-delimited-stdstring
//or http://www.partow.net/programming/strtk/index.html
    std::vector<std::pair<const char*, int>> arg_names;
//if (strstr(str, "anonymous")) { printf("templ args in: %s\n", str); fflush(stdout); }
    for (const char* sep = ++str; /*sep != end*/; ++sep)
        if ((*sep == ',') || (*sep == '>'))
        {
//printf("arg#%d: '%.*s'\n", arg_names.size(), sep - str, str);
//            if (!strncmp(str, "<anonymous", sep - str)) ++str; //unnamed args; skip extra "<"
            arg_names.push_back(std::pair<const char*, int>(str, sep - str)); //std::string(start, sep - start));
            if (*sep == '>') break;
            str = sep + 1;
            if (*str == ' ') ++str;
        }
//printf("found %d templ arg names in %s\n", arg_names.size(), svstr);
//get type string for each arg:
//    std::ostringstream argtypes;
//    int numfound = 0;
    std::string arg_types;
    for (auto it = arg_names.begin(); it != arg_names.end(); ++it)
    {
        char buf[64];
        arg_types.push_back(arg_types.length()? ',': '<');
        bool hasname = strncmp(it->first, "<anonymous", it->second); //kludge: unnamed args; NOTE: extra "<" present from above
        if (!hasname) { strcpy(buf, " = "); ++it->first; it->second -= 6; } 
        else snprintf(buf, sizeof(buf), " %.*s = ", it->second, it->first);
        const char* bp1 = strstr(str, buf);
//printf("templ args[%d]: found '%s' in '%s'? %d\n", it - arg_names.begin(), buf, str, bp1? (bp1 - str): -1);
        if (!hasname && bp1) //try to find real name
            for (const char* bp0 = bp1; bp0 > str; --bp0)
                if (bp0[-1] == ' ') { it->second = bp1 - (it->first = bp0); break; } //found start of name
        arg_types.append(it->first, it->second);
        if (!bp1) continue; //just give arg name if can't find type
        bp1 += strlen(buf);
        const char* bp2 = strchr(bp1, ';'); if (!bp2) bp2 = strrchr(bp1, ']'); //strpbrk(bp1, ";]");
//printf("templ args: then found ;] ? %d\n", bp2? (bp2 - str): -1);
        if (!bp2) continue;
//        ++numfound;
        arg_types.push_back('=');
        arg_types.append(bp1, bp2 - bp1);
//        if (arg_types.length() != 1) continue;
//        sprintf(buf, "%d args: ", arg_names.size());
//        arg_types.insert(0, buf);
    }
    if (arg_types.length()) arg_types.push_back('>');
//printf("found %d templ arg names, %d types in %s\n", arg_names.size(), numfound, svstr);
//if (strstr(arg_types.c_str(), "anonymous")) { printf("templ args in: %s\n", arg_types.c_str()); fflush(stdout); }
    return arg_types;
//    std::regex args_re ("<([^>]*)>");
//    std::smatch m;
//    if (!std::regex_search(str, m, re)) return str;
//    for (auto x:m) std::cout << x << " ";
//    std::cout << std::endl;
//    s = m.suffix().str();
}


//#include <iostream>
//#include <vector>
#include <algorithm>
#include <iterator>
#include <initializer_list>
//#include <cassert>

//fixed-content/length vector:
//NOTE: don't store any member data except the list of values (or put extra data elsewhere in heap)
template <typename TYPE, size_t SIZE>
class PreallocVector
{
//public:
//    static const key_t KEY = 0xbeef0000 | NNNN_hex(SIZE); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
//    struct shdata
//    {
//    int count = 0;
    TYPE m_list[SIZE];
//    std::mutex mutex;
public: //ctors/dtors
//    str_map(KEYTYPE key, VALTYPE val) {}
    explicit PreallocVector() {}
//initializer list example: https://en.cppreference.com/w/cpp/utility/initializer_list
    explicit PreallocVector(std::initializer_list<TYPE> initl): m_list(initl) {}
public: //operators:
    inline TYPE& operator[](int inx) { return m_list[inx]; } //TODO: bounds checking?
    inline const TYPE& operator[](int inx) const { return m_list[inx]; }
public: //iterators
//no worky    using my_iter = typename std::vector<TYPE>::iterator; //just reuse std::vector<> iterator
//no worky    using my_const_iter = typename std::vector<TYPE>::const_iterator;
#if 0
//see example at https://gist.github.com/jeetsukumaran/307264
    template <typename ITER_TYPE> //keep it DRY; use templ param to select "const"
    class iter
    {
    public:
        typedef std::iterator self_type;
//        typedef const_iterator self_type;
        typedef ITER_TYPE value_type;
        typedef ITER_TYPE& reference;
        typedef ITER_TYPE* pointer;
        typedef std::forward_iterator_tag iterator_category;
        typedef int difference_type;
        inline std::iterator(pointer ptr): ptr_(ptr) {}
//        const_iterator(pointer ptr) : ptr_(ptr) { }
        inline self_type operator++() { self_type i = *this; ++ptr_; return i; }
        inline self_type operator++(int junk) { ++ptr_; return *this; }
        inline reference operator*() { return *ptr_; }
//        const reference operator*() { return *ptr_; }
        inline pointer operator->() { return ptr_; }
//        const pointer operator->() { return ptr_; }
        inline bool operator==(const self_type& rhs) { return (ptr_ == rhs.ptr_); }
        inline bool operator!=(const self_type& rhs) { return (ptr_ != rhs.ptr_); }
    private:
        pointer ptr_;
    };
    using my_iter = iter<TYPE>;
    using my_const_iter = iter<const TYPE>;
#elif 0
//https://lorenzotoso.wordpress.com/2016/01/13/defining-a-custom-iterator-in-c/
    class my_iter: public std::iterator</*Category*/ std::output_iterator_tag, /*class T*/ TYPE> //, class Distance = ptrdiff_t, class Pointer = T*, class Reference = T&
    {
    public:
        explicit inline my_iter(PreallocVector& Container, size_t index = 0): m_ptr(&Container[index]) {}
        inline TYPE operator*() const { return *m_ptr; }
        inline iterator& operator++() { return *m_ptr++; }
        inline iterator operator++(int junk) { return *++m_ptr; }
    private:
//        size_t nIndex = 0;
//        CustomContainer& Container;
        TYPE* m_ptr;
    };
#elif 0 //broken
//http://cpp-tip-of-the-day.blogspot.com/2014/05/building-custom-iterators.html
//custom iterators need to implement:
//ctors, copy ctor, assignment op, inc, dec, deref
//complete list is at: http://www.cplusplus.com/reference/iterator/iterator/
    template<typename ITER_TYPE = TYPE>
    class my_iter
    {
    public: //ctors/dtors
        explicit inline my_iter(PreallocVector& vec, size_t inx = 0): m_ptr(&vec[inx]) {}
        inline my_iter(const my_iter& that): m_ptr(that.m_ptr) {} //copy ctor
        inline my_iter(ITER_TYPE* that): m_ptr(that) {} //custom copy ctor for post++ op
    public: //operators
        inline my_iter& operator=(const my_iter& that) { m_ptr = that.m_ptr; return *this; }
//        inline size_t operator-(const TYPE* that) const { return m_ptr - that.m_ptr; }
        friend size_t operator-(const my_iter& lhs, const my_iter& rhs) { return lhs.m_ptr - rhs.m_ptr; }
        inline bool operator==(const my_iter& that) const { return (that.m_ptr == m_ptr); } //*this == that
        inline bool operator!=(const my_iter& that) const { return !(that.m_ptr == m_ptr); } //!(*this == that)
//        inline my_iter& operator=(TYPE* that) { m_ptr = that; return *this; }
        inline my_iter& operator++() { ++m_ptr; return *this; } //pre-inc; TODO: bounds check?
        inline my_iter operator++(int) { my_iter pre_inc(m_ptr++); return pre_inc; } //post-inc
        inline ITER_TYPE& operator*() { return *m_ptr; } //deref op; eg: (*it).member
        inline ITER_TYPE* operator->() { return m_ptr; } //deref op; eg: it->member
    private: //data members
        ITER_TYPE* m_ptr;
    };
#endif
public: //methods:
    inline size_t size() const { return SIZE; }
#if 0 //broken
//no worky    using my_iter = typename std::vector<TYPE>::iterator; //just reuse std::vector<> iterator
//no worky    using my_const_iter = typename std::vector<TYPE>::const_iterator;
    inline /*auto*/ /*TYPE* */ my_iter<TYPE> begin() /*const*/ { return my_iter<TYPE>(&m_list[0]); }
    inline /*auto*/ /*TYPE* */ my_iter<const TYPE> cbegin() const { return my_iter<const TYPE>(m_list[0]); }
//    vector<string>::iterator iter;
    inline /*auto*/ /*TYPE* */ my_iter<TYPE> end() /*const*/ { return my_iter<TYPE>(&m_list[SIZE]); }
    inline /*auto*/ /*TYPE* */ my_iter<const TYPE> cend() const { return my_iter<const TYPE>(m_list[SIZE]); }
#else
    inline TYPE* begin() { return &m_list[0]; }
    inline TYPE* end() { return &m_list[SIZE]; }
    inline const TYPE* cbegin() const { return &m_list[0]; }
    inline const TYPE* cend() const { return &m_list[SIZE]; }
#endif
//    void push_back(/*const*/ TYPE& new_item)
//    {
////TODO: reserve(), capacity(), grow(), etc
//        if (count >= SIZEOF(list)) exc_hard("FixedSmhVector<" << SIZE << "> no room");
//        list[count++] = new_item; //TODO: emplace?
//    }
};

#endif //ndef _STR_HELPERS_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

//#include "msgcolors.h"
//#include "debugexc.h"
#include <map>
#include "logging.h"

//#include "str-helpers.h"

//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    debug(0, "0x%d = 0x%x", 1234, NNNN_hex(1234));

    debug(0, "1K = %s", commas(1024));
    debug(0, "1M = %s", commas(1024 * 1024));
    int count0 = 0, count1 = 1, count2 = 2;
    debug(0, count0 << " thing" << plural(count0));
    debug(0, count1 << " thing" << plural(count1));
    debug(0, count2 << " thing" << plural(count2, "ies"));
    int x1 = 0, x2 = 1;
//    debug(BLUE_MSG << NVL(x1, -1) << ENDCOLOR);
//    debug(BLUE_MSG << NVL(x2, -1) << ENDCOLOR);
    const char* str = "hello";
    const char* null = 0;
    debug(0, NVL(null, "(null)"));
    debug(0, NVL(str, "(null)"));
    debug(0, NVL(123, -1));
    debug(0, NVL(0, -1));

#if 0
//    static const std::map<int, const char*> SDL_RendererFlagNames =
    static const std::vector<std::pair<int, const char*>> SDL_RendererFlagNames =
    {
        {1, "SW"}, //0x01
        {2, "ACCEL"}, //0x02
        {3, "VSYNC"}, //0x04
        {4, "TOTXR"}, //0x08
//        {~(SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE), "????"},
    };
#endif
//http://www.informit.com/articles/article.aspx?p=1852519

    /*static const*/ str_map<const char*, int> opts //=
//    static const std::vector<std::pair<const char*, int>> opts =
//    XYZ opts //=
    {
//    opts::PAIRTYPE /*std::pair<const char*, int>*/ not_found = 
        {"!found", -1},
        {"first", 1},
        {"second", 2},
        {"third", 3},
        {"fourth", 4},
        {"fifth", 5},
    };
//printf("here2\n"); fflush(stdout);
    debug(0, "find third: %p = %d", opts.find("third"), opts.find("third", 0)->second);
//printf("here3\n"); fflush(stdout);
    debug(0, "find sixth: %p = %d", opts.find("sixth"), opts.find("sixth", 0)->second);

    int x;
    debug(0, BLUE_MSG << FMT("hex 0x%x") << 42);
    debug(0, BLUE_MSG << FMT("str4 %.4s") << "abcdefgh");
    debug(0, BLUE_MSG << FMT("ptr %p") << &x);
    debug(0, BLUE_MSG "ptr %s", commas(&x));

//    return 0;
}

#endif //def WANT_UNIT_TEST