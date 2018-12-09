//TODO:
//- set level + modules from env.DEBUG


////////////////////////////////////////////////////////////////////////////////
////
/// debug and error helpers
//

#if !defined(_DEBUGEXC_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _DEBUGEXC_H //CAUTION: put this before defs to prevent loop on cyclic #includes
//#pragma message("#include " __FILE__)

#include <sstream> //std::ostringstream
#include <stdexcept> //std::runtime_error
#include <utility> //std::forward<>
#include <stdio.h> //<cstdio> //vsnprintf()
#include <stdarg.h> //varargs
#include <string>

#include "msgcolors.h" //MSG_*, ENDCOLOR_*
#include "str-helpers.h" //NVL()
//cyclic; moved below; #include "thr-helpers.h" //thrid(), thrinx()
#include "srcline.h" //SRCLINE
#include "elapsed.h" //elapsed_msec()


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
#define exc_hard(...)  myprintf(-1, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
#define exc_soft(...)  myprintf(-2, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
//#define exc(...)  myprintf(-1, ssfriend() << __VA_ARGS__)
//template <typename ... ARGS>
//void exc(ARGS&& ... args)
//{
//    myprintf(-1, std::forward<ARGS>(args) ...); //perfect fwding
//}
//#ifndef DEBUG_DEFLEVEL
// #define DEBUG_DEFLEVEL  0
//#endif
//#define debug(...)  myprintf(DEBUG_DEFLEVEL, std::ostringstream() << __VA_ARGS__)
//#define debug_level(level, ...)  myprintf(level, std::ostringstream() << __VA_ARGS__)
//set default if caller didn't specify:
//use macro so SRCLINE will be correct
#define debug(level, ...)  ((level <= MAX_DEBUG_LEVEL)? (myprintf(level, SRCLINE, std::ostringstream() << __VA_ARGS__), 0): 0) //filter out *max* detail at compile time
//#define debug(...)  myprintf(0, ssfriend() << __VA_ARGS__)
//template <typename ... ARGS>
//void debug(ARGS&& ... args)
//{
//    myprintf(0, std::forward<ARGS>(args) ...); //perfect fwding
//}

#define HERE(n)  { printf("here " TOSTR(n) SRCLINE "\n"); fflush(stdout); } //TOSTR(n) " @" TOSTR(__LINE__) "\n"); fflush(stdout); }

#if 0
#define SNAT_1ARG(var)  snapshot("(no name)", var)
#define SNAT_2ARGS(desc, var)  SNAT_3ARGS(desc, &(var), sizeof(var))
#define SNAT_3ARGS(desc, addr, len)  SNAT_4ARGS(desc, addr, len, SRCLINE)
#define SNAT_4ARGS(desc, addr, len, srcline)  snapshot(desc, addr, len, srcline) //NVL(srcline, SRCLINE))
#define SNAT(...)  UPTO_4ARGS(__VA_ARGS__, SNAT_4ARGS, SNAT_3ARGS, SNAT_2ARGS, SNAT_1ARG) (__VA_ARGS__)
#endif


#ifndef INSPECT_LEVEL
 #define INSPECT_LEVEL  12
#endif
//put desc/dump of object to debug:
//use macro so SRCLINE will be correct
#define inspect_1ARG(thing)  inspect_2ARGS(INSPECT_LEVEL, thing) //SRCLINE)
//#define inspect_2ARGS(things, srcline)  inspect_3ARGS(INSPECT_LEVEL, things, srcline)
#define inspect_2ARGS(level, things)  debug(level, BLUE_MSG << things)
//#define inspect_3ARGS(level, things, srcline)  debug(level, BLUE_MSG << things << srcline) //NVL(srcline, "") //ENDCOLOR_ATLINE(srcline))
//#define INSPECT(...)  UPTO_3ARGS(__VA_ARGS__, inspect_3ARGS, inspect_2ARGS, inspect_1ARG) (__VA_ARGS__)
#define INSPECT(...)  UPTO_2ARGS(__VA_ARGS__, inspect_2ARGS, inspect_1ARG) (__VA_ARGS__)


/*void*/ int myprintf(int level, /*SrcLine srcline,*/ SrcLine srcline, const char* fmt, ...); //fwd ref

//kludge: implicit cast ostringstream -> const char* !worky; overload with perfect fwd for now
template <typename ... ARGS>
//see https://stackoverflow.com/questions/24315434/trouble-with-stdostringstream-as-function-parameter
/*void*/ int myprintf(int level, SrcLine srcline, std::/*ostringstream*/ostream& fmt, ARGS&& ... args) //const std::ostringstream& fmt, ...);
{
    myprintf(level, srcline, static_cast<std::ostringstream&>(fmt).str().c_str(), std::forward<ARGS>(args) ...); //perfect fwding
//    printf(static_cast<std::ostringstream&>(fmt).str().c_str(), std::forward<ARGS>(args) ...);
}

//void myprintf(int level, std::ostringstream& fmt, int& val)
//{
//    myprintf(level, fmt.str().c_str(), val);
//}


//utility class for tracing function in/out:
//use macro to preserve SRCLINE
#define DebugInOut(...)  InOutDebug inout(SRCLINE, std::ostringstream() << __VA_ARGS__)
class InOutDebug
{
    static const int INOUT_LEVEL = 15;
public:
//kludge: overload until implicit cast ostringstream -> const char* works
    explicit InOutDebug(SrcLine srcline, std::/*ostringstream*/ostream& label): InOutDebug(srcline, static_cast<std::ostringstream&>(label).str().c_str()) {} //delegated ctor
    explicit InOutDebug(SrcLine srcline, const char* label = ""): m_started(elapsed_msec()), m_label(label), m_srcline(NVL(srcline, SRCLINE)) { debug(INOUT_LEVEL, BLUE_MSG << label << ": IN" << m_srcline); } //ENDCOLOR_ATLINE(srcline)); }
    /*virtual*/ ~InOutDebug() { debug(INOUT_LEVEL, BLUE_MSG << m_label << ": OUT after %f msec" << m_srcline, restart()); } //ENDCOLOR_ATLINE(m_srcline), restart()); }
public: //methods
    double restart(bool update = true) //my_elapsed_msec(bool restart = false)
    {
        double retval = elapsed_msec() - m_started;
        if (update) m_started = elapsed_msec();
        return retval;
    }
    void checkpt(const char* desc = 0, SrcLine srcline = 0) { debug(INOUT_LEVEL, BLUE_MSG << m_label << " CHKPT(%s) after %f msec" << m_srcline /*ENDCOLOR_ATLINE(NVL(srcline, m_srcline))*/, NVL(desc, ""), restart(false)); }
protected: //data members
    /*const*/ int m_started; //= -elapsed_msec();
//    const char* m_label;
    std::string m_label; //make a copy in case caller's string is on stack
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
};


#if 0
const int SNAT_LEVEL = 33;
void snapshot(const char* desc, const void* addr, size_t len, SrcLine srcline = 0)
{
    debug(SNAT_LEVEL, BLUE_MSG "%s %p..+%u:" ENDCOLOR_ATLINE(srcline), desc, addr, len);
    std::ostringstream ss;
//    ss << std::hex;
    for (int i = 0; i < len; i += 4)
    {
        if (!(i % 16))
        {
            if (i) ss << "\n";
//            ss << " '0x" << i << " " << addr + i << ":";
            ss << FMT(" '0x%x") << i << FMT(" %p:") << addr + i;
        }
        ss << FMT(" 0x%.8x") << *(uint32_t*)(addr + i);
    }
    debug(SNAT_LEVEL, BLUE_MSG << ss.str() << ENDCOLOR);
}
#endif


//display a message:
//popup + console for dev/debug only
//NOTE: must come before perfect fwding overloads (or else use fwd ref)
//void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...)
#include "thr-helpers.h" //thrid(), thrinx(); //CAUTION: cyclic #include; must be after #def debug
/*void*/ int myprintf(int level, SrcLine srcline, const char* fmt, ...) //use ret type to allow conditional/ternary usage
{
    if (level > MAX_DEBUG_LEVEL) return 0; //caller doesn't want this much detail

//fmt basic msg contents:
    char fmtbuf[800];
    va_list args;
    va_start(args, fmt);
    size_t fmtlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args);
    va_end(args);
//show warning if fmtbuf too short:
//    std::ostringstream tooshort;
    if (fmtlen >= sizeof(fmtbuf)) fmtlen = sizeof(fmtbuf) - 30 + snprintf(&fmtbuf[sizeof(fmtbuf) - 30], 30, " (too long: need %d)", fmtlen);
//    /*if (stm == stderr)*/ printf("%.*s%s%.*s%s!%d%s\n", (int)srcline_ofs, fmtbuf, details.c_str(), (int)(lastcolor_ofs - srcline_ofs), fmtbuf + srcline_ofs, tooshort.str().c_str(), /*shortname*/ Thread::isBkgThread(), fmtbuf + lastcolor_ofs);
//    const char* tsbuf = timestamp(), endts = strchr(tsbuf, ' ');
//    if (*tsbuf == '[') ++tsbuf;
//    if (!endts) endts = tsbuf + strlen(tsbuf);
//TODO: combine with env/command line options, write to file, etc 
//    char msghdr[20];
//    snprintf(msghdr, sizeof(msghdr), "[%s $%d]", timestamp(true).c_str(), thrinx());

//find first + last color codes so we can insert more text correctly into msg:
//        const ColorCodes = /\x1b\[\d+(;\d+)?m/g; //ANSI color escape codes; NOTE: need /g to get last match
//    static const char* ColorCodes_ldr = ANSI_COLOR("\0"); //kludge: inject null color to truncate, then use string search instead of regex for better performance; //code)  "\x1b[" code "m"
//    static const char* ColorCodes_tlr = strend(ColorCodes_ldr) + 1; //skip injected null to get remainder of string
    static const /*std::string*/ char* ColorCodes = ANSI_COLOR("###"); //use dummy color code for length parsing
    static const size_t cc_startlen = strstr(ColorCodes, "###") - ColorCodes; //.find("###") - ColorCodes.begin();
    static const char* ColorCodes_end = ColorCodes + cc_startlen + 3;
    static const size_t cc_endlen = strlen(ColorCodes_end);
//    static const size_t cc_endlen = strlen(ColorCodes + cc_startlen + 3);
    size_t first_ofs, last_ofs = 0; //text ins offsets at beginning and end of fmt str
    for (;;)
    {
        const char* match = strnstr(fmtbuf + last_ofs, ColorCodes, cc_startlen); //ColorCodes.exec(fmt);
//printf("match %u, ", match - fmtbuf);
        if (!match) break;
        const char* match_end = strstr(match, ColorCodes_end);
//printf("match end %u, ", match_end - fmtbuf);
        if (!match_end) break; //not a valid color code?
        match_end += cc_endlen;
//        if (match_end - match > cc_len) { last_ofs += cc_startlen; continue; } //skip partial and find next one
//console.log("found last", match, ColorCodes);
//        last_ofs = match[0].length; //(match && !match.index)? match[0].length: 0; //don't split leading color code
//        if (!first_ofs) first_ofs = last_ofs;
//console.log(match[0].length, match.index, ColorCodes.lastIndex, match);
        if (!last_ofs) first_ofs = (match == fmtbuf)? (match_end - fmtbuf): 0; //(match && !match.index)? match[0].length: 0; //don't split leading color code
        last_ofs = (match_end == fmtbuf + fmtlen)? (match - fmtbuf): fmtlen; //ColorCodes.lastIndex;
//printf("first %u, last %u, look again ...\n", /*match - fmtbuf, match_end - fmtbuf,*/ first_ofs, last_ofs);
    }
    if (!last_ofs) { first_ofs = 0; last_ofs = fmtlen; }
//printf("FINAL: first %u, last %u, cc start %u, cc end %u\n", /*match - fmtbuf, match_end - fmtbuf,*/ first_ofs, last_ofs, cc_startlen, cc_endlen);
//    std::ostringstream ss;
//    ss << ENDCOLOR_ATLINE(srcline);
//    if (strstr(fmtbuf, )) srcline = ""; //already there; don't add again
//#define ENDCOLOR  "  &" SRCLINE ENDCOLOR_NOLINE //use const char* if possible; don't call shortsrc()
//    static SrcLine me = SRCLINE;
    if (!srcline) srcline = SRCLINE;
    if (strstr(fmtbuf, srcline)) srcline = ""; //don't repeat if already there
    else
    {
        const char* bp = strchr(srcline, ':');
        if (strnstr(fmtbuf, srcline, bp - srcline)) srcline = bp; //same file, different line; just show additional line#
    }

//send msg to stderr or stdout, depending on severity:
    static std::mutex mtx;
    using LOCKTYPE = std::unique_lock<decltype(mtx)>; //not: std::lock_guard<decltype(m_mtx)>;
    LOCKTYPE lock(mtx); //avoid interleaved output between threads
//    static int count = 0;
//    if (!count++) printf(CYAN_MSG "[elapsed-msec $thread] ===============================\n" ENDCOLOR_NOLINE);
    FILE* fout = (level < 0)? stderr: stdout;
    const char* hdr_color = (level < 0)? RED_MSG: PINK_MSG;
    const char* msg_color = (level < 0)? RED_MSG: BLUE_MSG;
    thread_local static int /*numerr = 0,*/ nummsg = 0;
//    if (level < 0) //error
//    {
    if (!nummsg++) fprintf(stderr, "%s[msec $thr] ======== thread# %d, id 0x%x, pid %d ========" ENDCOLOR_NEWLINE, hdr_color, thrinx(), thrid(), getpid());
//for (char* bp = fmtbuf; bp = strchr(bp, '\n'); *bp++ = '\\');
//for (char* bp = fmtbuf; bp = strchr(bp, '\x1b'); *bp++ = '?');
//printf("first ofs %d, last ofs %d, len %d, cc start %u, end %u, buf \"%s\"\n", first_ofs, last_ofs, fmtlen, cc_startlen, cc_endlen, fmtbuf); fflush(stdout);
    fprintf(fout, "%s%.*s[%s $%d] %.*s%s%s" ENDCOLOR_NEWLINE, msg_color, first_ofs, fmtbuf, timestamp(true).c_str(), thrinx(), last_ofs - first_ofs, fmtbuf + first_ofs, srcline, fmtbuf + last_ofs);
    if (level < 0) { fflush(stdout); fflush(fout); } //make sure important msgs are output; might as well incl stdout as well
    if (level == -1) throw std::runtime_error(fmtbuf);
//    }
//    else //debug
//    {
//        if (!nummsg++) printf(PINK_MSG "[msec $thr] ======== thread# %d, id 0x%x, pid %d ========\n" ENDCOLOR_NOLINE, thrinx(), thrid(), getpid());
//        printf(BLUE_MSG "%*s[%s $%d] %*s%s%s\n" ENDCOLOR_NOLINE, first_ofs, fmtbuf, timestamp(true).c_str(), thrinx(), last_ofs - first_ofs, fmtbuf + first_ofs, real_srcline, fmtbuf + last_ofs);
//    }
}

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
    debug(0, CYAN_MSG "in func(%d)" /*ENDCOLOR*/, val); //without "<<", with value
    debug(0, BLUE_MSG "func " << "val: " << "0x%x" /*ENDCOLOR_*/ << ATLINE(srcline), val); //with "<<", with value
    debug(0, CYAN_MSG "out func"); //ENDCOLOR); //without "<<", without value
    debug(40, RED_MSG "should%s see this one", (MAX_DEBUG_LEVEL >= 40)? "": "n't"); //ENDCOLOR);
}

//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    DebugInOut("unit test");
//    debug(BLUE_MSG "SDL_Lib: init 0x%x" ENDCOLOR_ATLINE(srcline), flags);
    func();
    func(1);
    func(2, SRCLINE);
    exc_soft("test completed");
//    return 0;
}

#endif //def WANT_UNIT_TEST