////////////////////////////////////////////////////////////////////////////////
////
/// String helpers:
//

#ifndef _STR_HELPERS_H
#define _STR_HELPERS_H

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


#ifndef SIZE
 #define SIZE(thing)  int(sizeof(thing) / sizeof((thing)[0]))
#endif


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


//from https://stackoverflow.com/questions/287903/what-is-the-preferred-syntax-for-defining-enums-in-javascript
//TODO: convert to factory-style ctor
class Enum
{
    constructor(enumObj)
    {
        const handler =
        {
            get(target, name)
            {
                if (target[name]) return target[name];
                throw new Error(`No such enumerator: ${name}`);
            },
        };
        return new Proxy(Object.freeze(enumObj), handler);
    }
}
//usage: const roles = new Enum({ ADMIN: 'Admin', USER: 'User', });


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




//for grammatically correct msgs: :)
inline const char* plural(int count, const char* suffix = "s", const char* singular = 0)
{
//    return "(s)";
    return (count != 1)? suffix: "";
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


//insert commas into a numeric string (for readability):
//CAUTION: uses static data to preserve data after return; semaphore arbitrates a pool of 12 ret values
const char* commas(int64_t val)
{
    const int LIMIT = 4; //max #commas to insert
    thread_local static std::atomic<int> ff;
    thread_local static char buf[12][16 + LIMIT]; //allow 12 simultaneous calls (across all threads)
//    static auto_ptr<SDL_sem> acquire(SDL_CreateSemaphore(SIZE(buf)));
//    auto_ptr<SDL_LockedSemaphore> lock_HERE(acquire.cast); //SDL_LOCK(acquire));

    char* bufp = buf[++ff % SIZE(buf)] + LIMIT; //alloc ret val from pool; don't overwrite other values within same printf, allow space for commas
    for (int grplen = std::min(sprintf(bufp, "%ld", val), LIMIT * 3) - 3; grplen > 0; grplen -= 3)
    {
        memmove(bufp - 1, bufp, grplen);
        (--bufp)[grplen] = ',';
    }
    return bufp;
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
    debug(0, BLUE_MSG "1K = %s" ENDCOLOR, commas(1024));
    debug(0, BLUE_MSG "1M = %s" ENDCOLOR, commas(1024 * 1024));
    int count0 = 0, count1 = 1, count2 = 2;
    debug(0, BLUE_MSG << count0 << " thing" << plural(count0) << ENDCOLOR);
    debug(0, BLUE_MSG << count1 << " thing" << plural(count1) << ENDCOLOR);
    debug(0, BLUE_MSG << count2 << " thing" << plural(count2, "ies") << ENDCOLOR);
    int x1 = 0, x2 = 1;
//    debug(BLUE_MSG << NVL(x1, -1) << ENDCOLOR);
//    debug(BLUE_MSG << NVL(x2, -1) << ENDCOLOR);
    const char* str = "hello";
    const char* null = 0;
    debug(0, BLUE_MSG << NVL(null, "(null)") << ENDCOLOR);
    debug(0, BLUE_MSG << NVL(str, "(null)") << ENDCOLOR);
    debug(0, BLUE_MSG << NVL(123, -1) << ENDCOLOR);
    debug(0, BLUE_MSG << NVL(0, -1) << ENDCOLOR);

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
    debug(0, BLUE_MSG "find third: %p = %d" ENDCOLOR, opts.find("third"), opts.find("third", 0)->second);
//printf("here3\n"); fflush(stdout);
    debug(0, BLUE_MSG "find sixth: %p = %d" ENDCOLOR, opts.find("sixth"), opts.find("sixth", 0)->second);

//    return 0;
}

#endif //def WANT_UNIT_TEST
