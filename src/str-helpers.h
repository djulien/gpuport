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
#include <stdio.h> //fflush()
#include <vector>
#include <utility> //std::pair<>


#ifndef SIZEOF
 #define SIZEOF(thing)  /*int*/(sizeof(thing) / sizeof((thing)[0])) //should be unsigned (size_t)
#endif


#ifndef TOSTR
 #define TOSTR(str)  TOSTR_NESTED(str)
 #define TOSTR_NESTED(str)  #str
#endif


//get end of string:
//use inline function instead of macro to avoid side effects if str is dynamic
inline const char* strend(const char* str)
{
    return str? str + strlen(str): NULL;
}


const char* strnstr(const char* str, const char* substr, size_t cmplen)
{
    for (;;)
    {
        const char* found = strchr(str++, *substr);
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


//return default string instead of null:
//use function to avoid evaluating params > 1x
//#define NVL(str, defval)  ((str)? (str): (defval)? (defval): "(null")
//inline const char* NVL(const char* str, const char* defval = 0) { return str? str: defval? defval: "(null)"; }
//use template to allow different types
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
#if 0 //TODO
const char* commas(double val)
{
//TODO
    return commas((int64_t)val);
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

#endif //ndef _STR_HELPERS_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include "msgcolors.h"
#include "debugexc.h"
#include <map>


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

//    return 0;
}

#endif //def WANT_UNIT_TEST