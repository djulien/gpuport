////////////////////////////////////////////////////////////////////////////////
////
/// debug and error helpers
//

#ifndef _DEBUGEXC_H
#define _DEBUGEXC_H


#include <sstream> //std::ostringstream
#include <stdexcept> //std::runtime_error
#include <utility> //std::forward<>
#include <stdio.h> //<cstdio> //vsnprintf()
#include <stdarg.h> //varargs

#include "msgcolors.h" //MSG_*, ENDCOLOR_*


//set default if caller didn't specify:
#ifndef MAX_DEBUG_LEVEL
 #define MAX_DEBUG_LEVEL  30 //100 //[0..9] for high-level, [10..19] for main logic, [20..29] for mid logic, [30..39] for lower level alloc/dealloc
#endif


//accept variable #3 macro args:
#ifndef UPTO_3ARGS
 #define UPTO_3ARGS(one, two, three, four, ...)  four
#endif


/*
//TODO: figure out why implicit conversion no worky
class ssfriend: public std::ostringstream //ostream
{
public: //ctor
    explicit ssfriend(const char* str = 0): m_str(str) {}
public: //operators
    operator const char*() const { return str().c_str(); }
//private:
//    class inner //actual worker class
//    {
//    public:
//        /-*explicit*-/ inner(std::ostream& strm, const ssfriend& str): m_strm(strm), m_str(str.m_str) {}
//output next object (any type) to stream:
//        template<typename TYPE>
//        std::ostream& operator<<(const TYPE& value) { m_strm << value; return m_strm; }
//    private:
//        std::ostream& m_strm;
//        const char* m_str;
//    };
//    const char* m_str; //save fmt string for inner class
//kludge: return derived stream to allow operator overloading:
//    friend ssfriend::inner operator<<(std::ostream& strm, const ssfriend& str) { return ssfriend::inner(strm, str); }
    friend std::ostream& operator<<(std::ostream& strm, const ssfriend& str) { return ssfriend::inner(strm, str); }
};
//#define ssfriend  std::ostringstream
*/


//#define ATOMIC_1ARG(stmt)  { /*InOut here("atomic-1arg")*/; LockOnce<WANT_IPC> lock(true, 10, SRCLINE); stmt; }
//#define ATOMIC_2ARGS(stmt, want_lock)  { /*InOut here("atomic-2args")*/; if (want_lock) ATOMIC_1ARG(stmt) else stmt; }
//#define ATOMIC(...)  USE_ARG3(__VA_ARGS__, ATOMIC_2ARGS, ATOMIC_1ARG) (__VA_ARGS__)


//debug messages:
//structured as expr to avoid surrounding { } and possible syntax errors
//#define debug(level, ...)  (((level) <= MAX_DEBUG_LEVEL)? myprintf(level, __VA_ARGS__): noop())
//inline void noop() {}

//void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...); //fwd ref
//void myprintf(const char* fmt, ...)
//{
//    /*return*/ myprintf(0, __VA_ARGS__);
//}

#define exc exc_hard
#define exc_hard(...)  myprintf(-1, std::ostringstream() << __VA_ARGS__)
#define exc_soft(...)  myprintf(-2, std::ostringstream() << __VA_ARGS__)
//#define exc(...)  myprintf(-1, ssfriend() << __VA_ARGS__)
//template <typename ... ARGS>
//void exc(ARGS&& ... args)
//{
//    myprintf(-1, std::forward<ARGS>(args) ...); //perfect fwding
//}
#define debug(...)  myprintf(0, std::ostringstream() << __VA_ARGS__)
#define debug_level(level, ...)  myprintf(level, std::ostringstream() << __VA_ARGS__)
//#define debug(...)  myprintf(0, ssfriend() << __VA_ARGS__)
//template <typename ... ARGS>
//void debug(ARGS&& ... args)
//{
//    myprintf(0, std::forward<ARGS>(args) ...); //perfect fwding
//}


//put desc/dump of object to debug:
#define inspect_1ARG(thing)  inspect_2ARGS(thing, 0)
#define inspect_2ARGS(thing, srcline)  inspect_3ARGS(12, thing, srcline)
#define inspect_3ARGS(level, thing, srcline)  debug_level(level, BLUE_MSG << thing << ENDCOLOR_ATLINE(srcline))
#define INSPECT(...)  UPTO_3ARGS(__VA_ARGS__, inspect_3ARGS, inspect_2ARGS, inspect_1ARG) (__VA_ARGS__)


//display a message:
//popup + console for dev/debug only
//NOTE: must come before perfect fwding overloads (or else use fwd ref)
//void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...)
void myprintf(int level, const char* fmt, ...)
{
    if (level > MAX_DEBUG_LEVEL) return; //0;
    char fmtbuf[600];
    va_list args;
    va_start(args, fmt);
    size_t needlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args);
    va_end(args);
//show warning if fmtbuf too short:
    std::ostringstream tooshort;
    if (needlen >= sizeof(fmtbuf)) tooshort << " (fmt buf too short: needed " << needlen << " bytes) ";
//    /*if (stm == stderr)*/ printf("%.*s%s%.*s%s!%d%s\n", (int)srcline_ofs, fmtbuf, details.c_str(), (int)(lastcolor_ofs - srcline_ofs), fmtbuf + srcline_ofs, tooshort.str().c_str(), /*shortname*/ Thread::isBkgThread(), fmtbuf + lastcolor_ofs);
//TODO: combine with env/command line options, write to file, etc 
    if (level < 0) //error
    {
        fprintf(stderr, RED_MSG "%s%s\n" ENDCOLOR_NOLINE, fmtbuf, tooshort.str().c_str()); fflush(stderr);
        if (level == -1) throw std::runtime_error(fmtbuf);
    }
    else //debug
        printf("%s%s\n", fmtbuf, tooshort.str().c_str());
}


//kludge: overload with perfect forwarding until implicit cast ostringstream -> const char* works
template <typename ... ARGS>
//see https://stackoverflow.com/questions/24315434/trouble-with-stdostringstream-as-function-parameter
void myprintf(int level, std::/*ostringstream*/ostream& fmt, ARGS&& ... args) //const std::ostringstream& fmt, ...);
{
    myprintf(level, static_cast<std::ostringstream&>(fmt).str().c_str(), std::forward<ARGS>(args) ...); //perfect fwding
//    printf(static_cast<std::ostringstream&>(fmt).str().c_str(), std::forward<ARGS>(args) ...);
}

//void myprintf(int level, std::ostringstream& fmt, int& val)
//{
//    myprintf(level, fmt.str().c_str(), val);
//}


#endif //ndef _DEBUGEXC_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include <iostream>
#include "msgcolors.h"
#include "srcline.h"
#include "debugexc.h"


void func(int val = -1, SrcLine srcline = 0)
{
    debug(CYAN_MSG "in func(%d)" ENDCOLOR, val); //without "<<", with value
    debug(BLUE_MSG "func " << "val: " << "0x%x" ENDCOLOR_ATLINE(srcline), val); //with "<<", with value
    debug(CYAN_MSG "out func" ENDCOLOR); //without "<<", without value
}

//int main(int argc, const char* argv[])
void unit_test()
{
//    debug(BLUE_MSG "SDL_Lib: init 0x%x" ENDCOLOR_ATLINE(srcline), flags);
    func();
    func(1);
    func(2, SRCLINE);
//    return 0;
}

#endif //def WANT_UNIT_TEST
