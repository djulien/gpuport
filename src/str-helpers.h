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
#include "string.h" //snprintf()
#include "memory.h" //memmove()

#ifndef SIZE
 #define SIZE(thing)  int(sizeof(thing) / sizeof((thing)[0]))
#endif


//return default string instead of null:
//use function to avoid evaluating params > 1x
//#define NVL(str, defval)  ((str)? (str): (defval)? (defval): "(null")
inline const char* NVL(const char* str, const char* defval = 0) { return str? str: defval? defval: "(null)"; }
//TODO:
//template <typename VALTYPE, VALTYPE DEFVAL = 0>
//_GLIBCXX14_CONSTEXPR inline const VALTYPE& NVL(const VALTYPE& val, const VALTYPE& defval = DEFVAL) { return val? val: defval? defval: DEFVAL; }


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
    static std::atomic<int> ff;
    static char buf[12][16 + LIMIT]; //allow 12 simultaneous calls (across all threads)
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


//int main(int argc, const char* argv[])
void unit_test()
{
    debug(BLUE_MSG "1K = %s" ENDCOLOR, commas(1024));
    debug(BLUE_MSG "1M = %s" ENDCOLOR, commas(1024 * 1024));
    int count0 = 0, count1 = 1, count2 = 2;
    debug(BLUE_MSG << count0 << " thing" << plural(count0) << ENDCOLOR);
    debug(BLUE_MSG << count1 << " thing" << plural(count1) << ENDCOLOR);
    debug(BLUE_MSG << count2 << " thing" << plural(count2, "ies") << ENDCOLOR);
    int x1 = 0, x2 = 1;
//    debug(BLUE_MSG << NVL(x1, -1) << ENDCOLOR);
//    debug(BLUE_MSG << NVL(x2, -1) << ENDCOLOR);
    const char* str = "hello";
    const char* null = 0;
    debug(BLUE_MSG << NVL(null, "(null)") << ENDCOLOR);
    debug(BLUE_MSG << NVL(str, "(null)") << ENDCOLOR);

//    return 0;
}

#endif //def WANT_UNIT_TEST
