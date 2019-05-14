//logging related functions + macros:
//- debug, warning, error (exc) msgs
//- ANSI color coding of msgs
//- inter-proc elapsed time
//- napi version for use with Node/JS


//TODO:
//- set level + modules from env.DEBUG
//- use circular shm msg log
//- thrinx/thrid list in shm
//- incl elapsed (shm epoch)
//- expose thru napi, combine with debug.js
//NOT NECESSARY:
//- use lambda to reduce overhead when turned off
//- use esc codes for colors, thrinx
//-take a look at: https://github.com/gabime/spdlog

#if 0
struct
{
    uint64_t epoch;
    key_t shmkey;
    PreallocVec<thrid, 8> thrids;
    int msgs[100]; //oldest, newest;
    char pool[0x4000];
}
merge srcline
merge msgcolors
merge debugexc
#incl napi
no: #incl commas
#endif


#if !defined(_LOGGING_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _LOGGING_H //CAUTION: put this before defs to prevent loop on cyclic #includes
//#pragma message("#include " __FILE__)


//accept variable # up to 2 macro args:
#ifndef UPTO_2ARGS
 #define UPTO_2ARGS(one, two, three, ...)  three
#endif
//#ifndef UPTO_3ARGS
// #define UPTO_3ARGS(one, two, three, four, ...)  four
//#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Source line:
//


#if 1 //simpler, less overhead
 #include "str-helpers.h" //TOSTR(), NVL()

 #define SRCLINE  "  @" __FILE__ ":" TOSTR(__LINE__)
 #define ATLINE(srcline)  NVL(srcline, SRCLINE) //<< ENDCOLOR_NOLINE //"\n"
//TODO: use std::source_location instead?:
//#include <experimental/source_location>
//std::experimental::source_location

 typedef const char* SrcLine; //allow compiler to distinguish param types, catch implicit conv
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// ANSI console colors:
//

//console colors

//#include "srcline.h" //SrcLine, SRCLINE //shortsrc()
//#include "str-helpers.h" //NVL()

//accept variable # up to 1 -2 macro args:
//#ifndef UPTO_2ARGS
// #define UPTO_2ARGS(one, two, three, ...)  three
//#endif


//ANSI color codes (for console output):
//https://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_COLOR(code)  "\x1b[" code "m"
//#define ANSI_COLOR(code)  std::ostringstream("\x1b[" code "m")
#define RED_MSG  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_MSG  ANSI_COLOR("1;32")
#define YELLOW_MSG  ANSI_COLOR("1;33")
#define BLUE_MSG  ANSI_COLOR("1;34")
#define MAGENTA_MSG  ANSI_COLOR("1;35")
#define PINK_MSG  MAGENTA_MSG //easier to spell :)
#define CYAN_MSG  ANSI_COLOR("1;36")
#define GRAY_MSG  ANSI_COLOR("0;37")
#define ENDCOLOR_NOLINE  ANSI_COLOR("0")

//single-char esc codes (shorter alternatives to above):
#if 0 //TODO?
#define ESC_NOCOLOR  "\xc0"
#define ESC_RED  "\xc1"
#define ESC_GREEN  "\xc2"
#define ESC_YELLOW  "\xc3"
#define ESC_BLUE  "\xc4"
#define ESC_MAGENTA  "\xc5"
#define ESC_PINK  ESC_MAGENTA //easier to spell :)
#define ESC_CYAN  "\xc6"
#define ESC_WHITE  "\xc7"
#define ESC_GRAY  "\xc8"
#endif

//append the src line# to make debug easier:
#if 0
//#define ENDCOLOR_ATLINE(srcline)  " &" TOSTR(srcline) ANSI_COLOR("0") "\n"
//#define ENDCOLOR_ATLINE(srcline)  "  &" << static_cast<const char*>(shortsrc(srcline, SRCLINE)) << ENDCOLOR_NOLINE "\n"
#define ENDCOLOR_ATLINE(srcline)  "  &" << shortsrc(srcline, SRCLINE) << ENDCOLOR_NOLINE //"\n"
//#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%s) //%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(SRCLINE) //__LINE__)
//#define ENDCOLOR_LINE(line)  FMT(ENDCOLOR_MYLINE) << (line? line: __LINE__) //show caller line# if available
#else
//TODO:
// #define ENDCOLOR_1ARG(blend, val1, val2)  ((int)((val1) * (blend) + (val2) * (1 - (blend)))) //uses floating point
// #define ENDCOLOR_2ARGS(num, den, val1, val2)  (((val1) * (num) + (val2) * (den - num)) / (den)) //use fractions to avoid floating point at compile time
// #define ENDCOLOR(...)  UPTO_2ARGS(__VA_ARGS__, ENDCOLOR_2ARGS, ENDCOLOR_1ARG) (__VA_ARGS__)
//#define ENDCOLOR_ATLINE(srcline)  "  &" << NVL(srcline, SRCLINE) << ENDCOLOR_NOLINE //"\n"
// #define ENDCOLOR_ATLINE(srcline)  NVL(srcline, SRCLINE) << ENDCOLOR_NEWLINE //"\n" //NOTE: special formatting likely not for debug(), so include newline
// #define ENDCOLOR  "  &" SRCLINE ENDCOLOR_NOLINE //use const char* if possible; don't call shortsrc()
////////////////////// #define ENDCOLOR  "  &" SRCLINE ENDCOLOR_NOLINE //use const char* if possible; don't call shortsrc()
#endif
#define ENDCOLOR_NEWLINE  ENDCOLOR_NOLINE "\n"


///////////////////////////////////////////////////////////////////////////////
////
/// Elapsed time:
//

//std::chrono::duration<double> elapsed()
#include <chrono> //now(), duration<>
//#include <sstream> //std::stringstream
//#include <unistd.h> //getpid()

//#include "str-helpers.h" //"ostrfmt.h" //FMT()

typedef /*decltype(now())*/ /*int64_t*/ uint32_t my_time_msec_t; //32 bits is enough for ~ 1200 hours (50 days) of msec or ~1.2 hr of usec
typedef /*decltype(elapsed())*/ uint32_t my_elapsed_msec_t;
//#ifdef time_t
// #undef time_t
//#endif
//#ifdef elapsed_t
// #undef elapsed_t
//#endif
//CAUTION: must come after thread.h
//BROKEN: conflicts with time_t used in condition_variable
//BROKEN #define time_t  my_time_msec_t //conflict; override time.h
#define elapsed_t  my_elapsed_msec_t

//fwd refs:
//elapsed_t Elapsed();
//elapsed_t Elapsed(elapsed_t reset);

inline /*time_t*/ my_time_msec_t Now() //msec
{
    using namespace std::chrono;
    return /*std::chrono::*/duration_cast</*std::chrono::*/milliseconds>(/*std::chrono::*/system_clock::now().time_since_epoch()).count();
}


//TYPE* shmalloc_typesafe(key_t key = 0, size_t ENTS = 1, SrcLine srcline = 0)
#define NO_RESET  ((elapsed_t)0x80000000)
//inline elapsed_t Elapsed() { return Now() - m_started; }
//elapsed_t Elapsed(elapsed_t reset = NO_RESET)
//CAUTION: times are relative to local process; use LogInfo::Elapsed() for shared/adjustable epoch
inline elapsed_t Elapsed()
{
//    typedef std::atomic<time_t> epoch_t;
//    static AutoShmobj<epoch_t> epoch = shmalloc_typesafe<epoch_t>(0xF00D0001, 1, SRCLINE);> epoch = Now();
    static decltype(Now()) epoch = Now();
//    elapsed_t retval = Now() - epoch;
//    if (reset != NO_RESET) m_started += retval - reset;
//    return retval; //return un-updated value
    return Now() - epoch;
}


void sleep_msec(int msec)
{
    if (msec > 0) std::this_thread::sleep_for(std::chrono::milliseconds(msec))); //CAUTION: blocking; should only be used on bkg threads
}


#if 0 //replaced
//#include <iostream> //std::ostringstream, std::ostream
//#include <iomanip> //setfill, setw, etc
double elapsed_msec()
{
    static auto started = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
//    std::cout << "f(42) = " << fibonacci(42) << '\n';
//    auto end = std::chrono::system_clock::now();
//     std::chrono::duration<double> elapsed_seconds = end-start;    
//    long sec = std::chrono::system_clock::now() - started;
#if 0
    static bool first = true;
    if (first)
    {
        first = false;
        std::ostringstream op;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&started);
//convert 'struct' into 'hex string'
//from https://www.geeksforgeeks.org/conversion-of-struct-data-type-to-hex-string-and-vice-versa/
//void convert_to_hex_string(ostringstream &op, const unsigned char* data, int size)
        std::ostream::fmtflags old_flags = op.flags(); //Format flags
        char old_fill  = op.fill();
        op << std::hex << std::setfill('0'); //Fill characters
        for (int i = 0; i < sizeof(started); i++)
        {
            if (i) op << ' '; //space between two hex values
            op << "0x" << std::setw(2) << static_cast<int>(data[i]); //force output to use hex version of ascii code
        }
        op.flags(old_flags);
        op.fill(old_fill);
        std::cout << "elapsed epoch " << std::hex << op.str() /*time_point*/ << "\n" << std::flush;
    }
#endif
    auto now = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
//https://stackoverflow.com/questions/14391327/how-to-get-duration-as-int-millis-and-float-seconds-from-chrono
//http://en.cppreference.com/w/cpp/chrono
//    std::chrono::milliseconds msec = std::chrono::duration_cast<std::chrono::milliseconds>(fs);
//    std::chrono::duration<float> duration = now - started;
//    float msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
#if 0
    typedef std::chrono::milliseconds ms;
    typedef std::chrono::duration<float> fsec;
    fsec fs = now - started;
    ms d = std::chrono::duration_cast<ms>(fs);
    std::cout << fs.count() << "s\n";
    std::cout << d.count() << "ms\n";
    return d.count();
#endif
    std::chrono::duration<double, std::milli> elapsed = now - started;
//    std::cout << "Waited " << elapsed.count() << " ms\n";
    return elapsed.count();
}


//from https://stackoverflow.com/questions/19555121/how-to-get-current-timestamp-in-milliseconds-since-1970-just-the-way-java-gets
int64_t now_msec()
{
    using namespace std::chrono;
    return /*std::chrono::*/duration_cast</*std::chrono::*/milliseconds>(/*std::chrono::*/system_clock::now().time_since_epoch()).count();
}
#endif


#if 0
class Stopwatch
{
public:
    explicit Stopwatch(/*SrcLine srcline = 0*/): m_started(elapsed_msec()) {} //, m_label(label), m_srcline(NVL(srcline, SRCLINE)) { debug(BLUE_MSG << label << ": in" ENDCOLOR_ATLINE(srcline)); }
    virtual ~Stopwatch() {} //debug(BLUE_MSG << m_label << ": out after %f msec" ENDCOLOR_ATLINE(m_srcline), restart()); }
public: //methods
    double restart() //my_elapsed_msec(bool restart = false)
    {
        double retval = elapsed_msec() - m_started;
        /*if (restart)*/ m_started = elapsed_msec();
        return retval;
    }
private: //data members
    double m_started; //= -elapsed_msec();
};
#endif


#if 0 //replaced
std::string timestamp(bool undecorated = false)
{
    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    float x = 1.2;
//    int h = 42;
//TODO: add commas
    if (undecorated) { ss << FMT("%4.3f") << elapsed_msec(); return ss.str(); }
    ss << FMT("[%4.3f msec") << elapsed_msec();
#ifdef IPC_THREAD
    ss << " " << getpid();
#endif
    ss << "] ";
    return ss.str();
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// thread info:
//


#include <thread> //std::thread::get_id(), std::thread()
//#include <utility> //std::forward<>
//#include <sstream> //std::ostringstream
//#include <condition_variable>
#include <vector> //std::vector<>
#include <mutex> //std:mutex<>, std::unique_lock<>

//#include "str-helpers.h" //PreallocVector<>

#if 0 //replaced
inline auto /*std::thread::id*/ Thrid()
{
//TODO: add pid for multi-process uniqueness?
    return std::this_thread::get_id();
}
#else
const thread_local auto thrid = std::this_thread::get_id();
#endif


#if 1
//reduce verbosity by using a unique small int instead of thread id:
//CAUTION: indices are relative to local process; use LogInfo::thrinx() for shared indices across processes
size_t Thrinx(const std::thread::id/*auto*/& myid = thrid) //Thrid())
{
//    PreallocVector</*thrid*/ std::thread::id, 8> thrids;
    static std::mutex mtx;
    using LOCKTYPE = std::unique_lock<decltype(mtx)>; //not: std::lock_guard<decltype(m_mtx)>;
    static std::vector</*std::decay<decltype(thrid())>*/std::thread::id> ids;

    LOCKTYPE lock(mtx);
    for (auto it = ids.begin(); it != ids.end(); ++it)
        if (*it == myid) return it - ids.begin();
    size_t newinx = ids.size();
    ids.push_back(myid);
    return newinx;
}
#endif


////////////////////////////////////////////////////////////////////////////////
////
/// debug and error helpers
//

#if 0
#include <sstream> //std::ostringstream
#include <stdexcept> //std::runtime_error
#include <utility> //std::forward<>
#include <stdio.h> //<cstdio> //vsnprintf()
#include <stdarg.h> //varargs
#include <cctype> //isspace()
#include <regex> //std::regex, std::cmatch, std::regex_match()
#include <string>

//#include "msgcolors.h" //MSG_*, ENDCOLOR_*
#include "str-helpers.h" //NVL(), commas()
//cyclic; moved below; #include "thr-helpers.h" //thrid(), thrinx()
//#include "srcline.h" //SRCLINE
//#include "elapsed.h" //elapsed_msec()


#ifndef MAX_DEBUG_LEVEL
 #define MAX_DEBUG_LEVEL  100 //[0..9] for high-level, [10..19] for main logic, [20..29] for mid logic, [30..39] for lower level alloc/dealloc
#endif
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


////////////////////////////////////////////////////////////////////////////////
////
/// Convenience macros:
//

#include <sstream> //std::ostringstream
#include <stdexcept> //std::runtime_error
#include <stdio.h> //printf(), fflush() //<cstdio> //vsnprintf()
//#include <stdarg.h> //varargs
#include <utility> //std::forward<>
#include <string> //std::string


//debug messages:
//structured as expr to avoid surrounding { } and possible syntax errors
//#define debug(level, ...)  (((level) <= MAX_DEBUG_LEVEL)? myprintf(level, __VA_ARGS__): noop())
//inline void noop() {}

//void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...); //fwd ref
//void myprintf(const char* fmt, ...)
//{
//    /*return*/ myprintf(0, __VA_ARGS__);
//}

//inline int32_t debug_level(int32_t newlevel)
//{
//    static int current_level = MAX_DEBUG_LEVEL;
//    if (newlevel >= 0) current_level = newlevel;
//    return current_level;
//}
//inline int32_t debug_level() { return debug_level(-1); }
//extern int32_t debug_level = MAX_DEBUG_LEVEL;

#ifndef MAX_DEBUG_LEVEL
 #define MAX_DEBUG_LEVEL  100 //[0..9] for high-level, [10..19] for main logic, [20..29] for mid logic, [30..39] for lower level alloc/dealloc
#endif

#define HERE(n)  { printf("here " TOSTR(n) SRCLINE "\n"); fflush(stdout); } //TOSTR(n) " @" TOSTR(__LINE__) "\n"); fflush(stdout); }


#define warn exc_soft
#define error exc_hard
#define exc_soft(...)  debug(DetailLevel::WARN_LEVEL, __VA_ARGS__) //logprintf(DetailLevel::WARN_LEVEL, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
#define exc_hard(...)  debug(DetailLevel::ERROR_LEVEL, __VA_ARGS__) //logprintf(DetailLevel::ERROR_LEVEL, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
#define exc_throw(...)  throw std::runtime_error((static_cast<std::ostringstream&>(std::ostringstream() << RED_MSG << __VA_ARGS__ << ENDCOLOR_NOLINE)).str())

#if 0
#define SNAT_1ARG(var)  snapshot("(no name)", var)
#define SNAT_2ARGS(desc, var)  SNAT_3ARGS(desc, &(var), sizeof(var))
#define SNAT_3ARGS(desc, addr, len)  SNAT_4ARGS(desc, addr, len, SRCLINE)
#define SNAT_4ARGS(desc, addr, len, srcline)  snapshot(desc, addr, len, srcline) //NVL(srcline, SRCLINE))
#define SNAT(...)  UPTO_4ARGS(__VA_ARGS__, SNAT_4ARGS, SNAT_3ARGS, SNAT_2ARGS, SNAT_1ARG) (__VA_ARGS__)
#endif


//kludge: "!this" no worky with g++ on RPi
//#ifndef isnull
// #ifdef __ARMEL__ //RPi //__arm__
//  #define isnull(ptr)  ((ptr) < reinterpret_cast<decltype(ptr)>(2)) //kludge: "!this" no worky with g++ on RPi; this !< 1 and != 0, but is < 2 so use that
// #else //PC
//  #define isnull(ptr)  !(ptr)
// #endif
//#endif


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


//#define WantDetail_0ARGS()  detail(__FILE__) //substr(__FILE__, strrofs(__FILE__, '.')))
//#define WantDetail_1ARG(level)  detail(__FILE__, level)
//#define WantDetail_2ARGS(file, level)  detail(file, level)
//#define WantDetail(...)  UPTO_2ARGS(__VA_ARGS__, WantDetail_2ARGS, WantDetail_1ARG, WantDetail_0ARGS) (__VA_ARGS__)


//fwd refs:
//int detail();
int detail(const char* key);
/*void*/ int logprintf(int level, /*SrcLine srcline,*/ SrcLine srcline, const char* fmt, ...);


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
//compiler should filter out anything > MAX_DEBUG_LEVEL, leaving only the second condition for run-time eval
//TODO: use lamba function for lazy param eval (better run-time perf, can leave debug enabled)
#define debug(level, ...)  ((((level) <= MAX_DEBUG_LEVEL) && ((level) <= detail(__FILE__)))? ::logprintf(level, SRCLINE, std::ostringstream() << __VA_ARGS__): 0) //filter out *max* detail at compile time
//#define debugx(level, ...)  ((((level) <= MAX_DEBUG_LEVEL) && ((level) <= detail()))? ::logprintf(level, SRCLINE, std::ostringstream() << __VA_ARGS__): (printf("!debug: level %d > %d && > %d" ENDCOLOR_NEWLINE, level, MAX_DEBUG_LEVEL, detail()), 0)) //filter out *max* detail at compile time
//#define debug(...)  myprintf(0, ssfriend() << __VA_ARGS__)
//template <typename ... ARGS>
//void debug(ARGS&& ... args)
//{
//    myprintf(0, std::forward<ARGS>(args) ...); //perfect fwding
//}

//kludge: implicit cast ostringstream -> const char* !worky; overload with perfect fwd for now
template <typename ... ARGS>
//see https://stackoverflow.com/questions/24315434/trouble-with-stdostringstream-as-function-parameter
/*void*/ int logprintf(int level, SrcLine srcline, std::/*ostringstream*/ostream& fmt, ARGS&& ... args) //const std::ostringstream& fmt, ...);
{
    logprintf(level, srcline, static_cast<std::ostringstream&>(fmt).str().c_str(), std::forward<ARGS>(args) ...); //perfect fwding
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
    explicit InOutDebug(SrcLine srcline, const char* label = ""): m_started(Now()), m_label(label), m_srcline(NVL(srcline, SRCLINE)) { debug(INOUT_LEVEL, BLUE_MSG << label << ": IN" << m_srcline); } //ENDCOLOR_ATLINE(srcline)); }
    /*virtual*/ ~InOutDebug() { debug(INOUT_LEVEL, BLUE_MSG << m_label << ": OUT after %f msec" << m_srcline, elapsed()); } //ENDCOLOR_ATLINE(m_srcline), restart()); }
public: //methods
#if 0
    double restart(bool update = true) //my_elapsed_msec(bool restart = false)
    {
        double retval = elapsed() - m_started;
        if (update) m_started = elapsed();
        return retval;
    }
#endif
//    elapsed_t elapsed() { return Now() - m_started; }
//allow caller to reset:
    elapsed_t elapsed(elapsed_t reset = NO_RESET)
    {
        elapsed_t retval = Now() - m_started; //elapsed();
        if (reset != NO_RESET) m_started += retval - reset;
        return retval; //elapsed();
    }
    void checkpt(const char* desc = 0, SrcLine srcline = 0) { debug(INOUT_LEVEL, BLUE_MSG << m_label << " CHKPT(%s) after %s msec" << m_srcline /*ENDCOLOR_ATLINE(NVL(srcline, m_srcline))*/, NVL(desc, ""), commas(elapsed(0))); }
protected: //data members
//    /*const*/ int m_started; //= -elapsed_msec();
    decltype(Now()) m_started;
//    const char* m_label;
    std::string m_label; //make a copy in case caller's string is on stack
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
};


/////////////////////////////////////////////////////////////////////////////////
////
/// debug env control (detail level):
//

//#include <sstream> //std::ostringstream
//#include <stdexcept> //std::runtime_error
//#include <utility> //std::forward<>
//#include <stdio.h> //<cstdio> //vsnprintf()
//#include <stdarg.h> //varargs
//#include <cctype> //isspace()
#include <regex> //std::regex, std::cmatch, std::regex_match()
//#include <string>
#include <string.h> //strrchr()
#include <cstdlib> //atoi(), getenv()
#include <vector>

//#include "msgcolors.h" //MSG_*, ENDCOLOR_*
#include "str-helpers.h" //substr, NVL(), commas()
//cyclic; moved below; #include "thr-helpers.h" //thrid(), thrinx()
//#include "srcline.h" //SRCLINE
//#include "elapsed.h" //elapsed_msec()


struct DetailLevel
{
//    static const int DEFAULT = 0;
    static const int TRUE = MAX_DEBUG_LEVEL; // / 3;
    static const int FALSE = 0; //no debug, just top level info, warnings, errors
    static const int WARN_LEVEL = -1;
    static const int ERROR_LEVEL = -2;
    static const int SILENT = -10; //< exc; completely silent
    static const int RESET = -98;
    static const int NO_CHANGE = -99;
//    static const int EMPTY = -100;
    static const int DUMP = -100; //only for dev/debug
//    const char* key; //CAUTION: expected to always be __FILE__ or env; use address as key (don't need to store local copy of char array)
//    int keylen, level;
    substr key; //CAUTION: expected to always be __FILE__ or env; use caller's key address (don't need to store local copy of char array)
    int level;
    DetailLevel(const char* that_key, int that_level): key(that_key), level(that_level) {} // /*MAX_DEBUG_LEVEL*/ EMPTY) {}
    DetailLevel(const substr& that_key, int that_level): key(that_key), level(that_level) {}
};


void first_time(); //fwd ref

//get/set detail logging level for given file (key):
int detail(const char* key /*= "*"*/, size_t keylen, int new_level /*= NO_CHANGE*/)
{
//set initial debug level to max, then discard if debug found to be off later; this avoids any debug data loss
    static /*PreallocVector<DetailLevel, 10>*/ std::vector<DetailLevel> details = {{"*", MAX_DEBUG_LEVEL}}; //max 1 per src file + 1 global
    static bool busy = false;
//    if (busy) return DetailLevel::SILENT;
    bool was_busy = busy;
    busy = true;
    first_time();

    if (new_level == DetailLevel::DUMP)
    {
        if (!was_busy) /*debug(32-32,*/printf( CYAN_MSG "debug detail %d entries:" ENDCOLOR_NEWLINE, details.size());
        for (auto it = details.begin(); it != details.end() /*&& (it->level != EMPTY)*/; ++it)
            if (!was_busy) /*debug(32-32,*/printf( BLUE_MSG "debug detail[%d/%d]: '%.*s' = %d" ENDCOLOR_NEWLINE, it - details.begin(), details.size(), it->key.len, it->key.str, it->level);
//        new_level = DetailLevel::NO_CHANGE;
        return 0;
    }
    if (!keylen)
    {
        keylen = strrofs(key, '.');
        if (!keylen) keylen = strlen(key);
        if (!keylen) return 0;
    }
    static const substr ALL("*");
    substr new_key(key, keylen);
    if (!was_busy && (new_level != DetailLevel::NO_CHANGE)) debug(32, "detail level " << new_key << " <- %d", new_level);
    busy = false;
    for (auto it = details.begin(); it != details.end() /*&& (it->level != EMPTY)*/; ++it)
//        if (!strncmp(it->key, key, sizeof(it->key)) return it;
        if (it->key == new_key) //(keylen == it->keylen) && !strncmp(it->key, key, keylen))
        {
            if (new_level == DetailLevel::NO_CHANGE) return it->level;
            if (new_level == DetailLevel::RESET) { details.erase(it); new_level = DetailLevel::NO_CHANGE; break; }
            return it->level = new_level;
        }
    if ((new_key != ALL) && (new_level == DetailLevel::NO_CHANGE))
        for (auto it = details.begin(); it != details.end() /*&& (it->level != NONE)*/; ++it)
            if (it->key == ALL) return it->level;
    if ((new_level == DetailLevel::NO_CHANGE) || (new_level == DetailLevel::RESET)) return DetailLevel::FALSE;
    details.emplace_back(new_key, new_level);
    return new_level;
}
//const substr me(__FILE__, strrofs(__FILE__, '.'));
inline int detail() { return detail(__FILE__); } //, strrofs(__FILE__, '.'), DetailLevel::NO_CHANGE); }
inline int detail(int new_level) { return detail(__FILE__, /*strrofs(__FILE__, '.')*/ 0, new_level); }
inline int detail(const char* key)
{
    int retval = detail(key, 0, DetailLevel::NO_CHANGE);
//printf("detail(%d:%s) = %d\n", NVL(strrofs(key, '.'), strlen(key)), key, retval);
    return retval;
}
int detail(const substr& key, const substr& level) //const char* key, size_t keylen, const char* new_level)
{
    debug(24, "set detail: key " << key << ", level " << level);
    int new_level;
    if (!level.len || (level == "+") || (level == "true")) new_level = DetailLevel::TRUE;
    else if ((level == "-") || (level == "false")) new_level = DetailLevel::FALSE;
    else new_level = atoi(level.str);
    return detail(key.str, key.len, new_level);
}


inline const char* skip_folder(const char* path)
{
    const char* bp = strrchr(path, '/');
    return bp? bp + 1: path;
}


void first_time()
{
    static bool inited = false;
    if (inited) return;
    inited = true;
//    std::string str(NVL(getenv("DEBUG"), ""));
    const char* str = NVL(getenv("DEBUG"), getenv("debug"));
//    printf("%.*s %d:'%s', trunc %d:'%.*s'\n", 4, "qwerty", strlen(__FILE__), __FILE__, strrofs(__FILE__, '.'), strrofs(__FILE__, '.'), __FILE__);
    if (!str) { warn("Prefix with \"DEBUG=%.*s\" or \"DEBUG=*\" for debug level %d (default is %d).", strrofs(skip_folder(__FILE__), '.'), skip_folder(__FILE__), DetailLevel::TRUE, DetailLevel::FALSE); return; } //tell user how to get debug info
    static const int           ONOFF = 1, NAME = 2,            LEVEL = 4;
    static const std::regex re("^([+\\-])?([^\\s=]+)(\\s*=\\s*(\\d+))?(\\s*,\\s*)?");
//    static const char* DELIM = ", ";
    for (int count = 0; *str; ++count)
    {
        if (count > 50) { warn("whoops; inf loop?"); break; } //paranoid/debug
//        while (strchr(DELIM, *str)) ++str;
//        const char* bp = str;
//        while (*str && !strchr(DELIM, *str)) ++str;
//        if (str == bp) continue;
//        if (!*str) break;
        std::cmatch cm; //const std::match_results
        debug(24, "check[%d] '%s' for debug", count, str);
//        if (!std::regex_match(/*std::string(bp, str - bp).c_str()*/ str, cm, re)) //figure out meaning for this arg
        if (!std::regex_search(str, cm, re)) //figure out meaning for this arg
        {
            const char* bp = NVL(strchr(str, ','), strend(str));
//            while (bp[-1] == ' ') --bp;
            warn("Ignoring unrecognized DEBUG option[%d]: '%.*s'", count, bp - str, str);
            str = bp + 1;
            continue;
        }
        debug(24, "%d submatches", cm.size());
        for (int i = 0; i < cm.size(); ++i) debug(24, "[%d] %d:'%.*s' = " << substr(cm[i].length()? strstr(str, cm[i].str().c_str()): "" /*.position()*/, cm[i].length()), i, cm[i].length(), cm[i].length(), cm[i].str().c_str()); //str().c_str());
//    var [, remove, name,, level] = parsed;
//        detail(/*cm[NAME].str().c_str()*/ substr(str + cm[NAME].position(), cm[NAME].length()), substr(str + cm[LEVEL].position(), cm[LEVEL].length())); //new_level);
//        debug(24, " name " << cm[NAME].length() << ":" << cm[NAME] << " = " << substr(strstr(str, cm[NAME].str().c_str()), cm[NAME].length()) << ", val " << cm[LEVEL]);
        substr name(strstr(str, cm[NAME].str().c_str())/*.position()*/, cm[NAME].length()); //kludge: recover original (env) str address using cmatch str copy
        substr onoff(cm[ONOFF].length()? strstr(str, cm[ONOFF].str().c_str()): "", cm[ONOFF].length()); //kludge: supply dummy string if len == 0 to compensate for strlen() in substr()
        substr level(cm[LEVEL].length()? strstr(str, cm[LEVEL].str().c_str()): "", cm[LEVEL].length());
        detail(name, onoff.len? onoff: level); //str + cm[LEVEL].position(), cm[LEVEL].length())); //new_level);
        str += cm[0].length();
//        searchStart = cm.suffix().first;
    }
    detail(DetailLevel::DUMP);
    debug(0, "my debug level = %d", detail());
}


////////////////////////////////////////////////////////////////////////////////
////
/// In-memory log files:
//

#include <mutex>
#include <condition_variable>
#include <string>
#include <stdio.h>

//abstract base class (interface):
class LogFile
{
public: //ctor/dtor
    LogFile() {}
    virtual ~LogFile() {} // show(); } //can't call virtual function from dtor; https://stackoverflow.com/questions/10707286/how-to-resolve-pure-virtual-method-called
public: //methods
    virtual size_t alloc(size_t buflen) = 0; //must be atomic; other methods don't need to be
    size_t writex(const char* buf, size_t buflen = 0) //kludge: can't overload virtual with non-virtual function, so use different name
    {
        if (!buflen) buflen = strlen(buf);
        return write(buf, buflen, alloc(buflen));
    }
    virtual size_t write(const char* buf, size_t buflen, size_t ofs) = 0;
    virtual std::string read(bool wait) = 0; //int maxents = 0)
    void wake() { m_cvar.notify_all(); }
//    void sleep()
//    {
//        LOCKTYPE lock(m_mutex);
//        while (!pred()) m_cvar.wait(lock);
//    }
protected: //members
    std::mutex m_mutex;
    std::condition_variable m_cvar;
    using LOCKTYPE = std::unique_lock<decltype(m_mutex)>; //not: std::lock_guard<decltype(m_mtx)>;
protected: //helpers
    void show() //bool clear = true)
    {
//        LOCKTYPE lock(m_mutex);
//        if (!clear) { printf(CYAN_MSG "flush nonshm log" ENDCOLOR_NEWLINE); fflush(stdout); }
//HERE(33);
        const std::string& display = read();
//HERE(34);
        if (!display.length()) return;
        printf(CYAN_MSG "drain mem log %p:\n%s" ENDCOLOR_NEWLINE, this, display.c_str());
        fflush(stdout);
//        /*if (clear)*/ m_log.clear();
    }
};


#if 1
class debug_string: public std::string
{
public:
    debug_string() { printf("str %p ctor\n", this); fflush(stdout); }
    ~debug_string() { printf("str %p dtor, len %u\n", this, length()); fflush(stdout); }
};
#else
 #define debug_string  std::string
#endif


//#if 1
//linear log file:
//for use with local memory when shm log not available
class LocalMemLog: public LogFile, public debug_string
{
//    std::mutex m_mutex;
//    std::condition_variable m_cvar;
//    using LOCKTYPE = std::unique_lock<decltype(m_mutex)>; //not: std::lock_guard<decltype(m_mtx)>;
//    debug_string m_log;
#define m_log  (*this)
public: //ctor/dtor
    LocalMemLog()
    {
        static int m_count = 0;
        if (m_count++) exc_throw("should be singleton");
//        static bool disp_later = false;
//        if (!disp_later) { disp_later = true; atexit([]() { show_log(false); }); }
    }
    /*virtual*/ ~LocalMemLog() { show(); } //NOTE: if object is static and created < LogInfo, don't need atexit() call here
public: //methods
    size_t alloc(size_t buflen) //must be atomic; other methods don't need to be
    {
        LOCKTYPE lock(m_mutex);
        size_t retval = m_log.length();
//        reserve(retval + buflen);
        m_log.append(buflen, '\0');
        return retval;
    }
    size_t write(const char* buf, size_t buflen, size_t ofs)
    {
        LOCKTYPE lock(m_mutex); //is lock needed for in-place replacement?
        m_log.replace(ofs, buflen, buf);
        return buflen;
    }
//CAUTION: blocking wait; should only be used on bkg threads
    std::string read(bool wait) //int maxents = 0)
    {
        LOCKTYPE lock(m_mutex);
        for (;;)
        {
            std::string retval = m_log; //*this;
            m_log.clear();
            if (retval.length() || !wait) return retval;
            m_cvar.wait(lock);
        }
    }
//    void wake() { m_cvar.notify_all(); }
//private: //helpers
//    void sleep()
//    {
//        LOCKTYPE lock(m_mutex);
//        while (!pred()) m_cvar.wait(lock);
//    }
#if 0
    void show() //bool clear = true)
    {
//        LOCKTYPE lock(m_mutex);
//        if (!clear) { printf(CYAN_MSG "flush nonshm log" ENDCOLOR_NEWLINE); fflush(stdout); }
        const std::string& display = read();
        if (!display.length()) return;
        printf(CYAN_MSG "local mem log:\n%s" ENDCOLOR_NEWLINE, display.c_str());
        fflush(stdout);
//        /*if (clear)*/ m_log.clear();
    }
#endif
#undef m_log
};
//CAUTION: must be created < and destroyed > all logging calls
static LocalMemLog nonshm_log; //must be outside of LogInfo to outlive LogInfo instances
//#endif


//circular log file:
//for use with shared memory
class CircularLog: public LogFile //, public debug_string
{
    std::atomic<size_t> m_head; //, m_tail /*= 0*/;
    size_t m_tail; //read protected by mutex/cvar; doesn't need to be atomic
//    std::atomic<int>& m_retries;
    char m_storage[0x4000]; //= ""; //16K will hold ~200 entries of 80 char
public: //ctor/dtor
//    CircularLog(): m_head(0), m_tail(0), m_retries(0)
//    CircularLog(std::atomic<int>& retries): m_retries(retries)
//    {
//        static int m_count = 0;
//        if (m_count++) exc_throw("should be singleton");
//    }
    /*virtual*/ ~CircularLog() { show(); } //NOTE: if object is static and created < LogInfo, don't need atexit() call here
public: //methods
//    size_t re_reads() const { return m_retries; }
    size_t alloc(size_t buflen) //must be atomic; other methods don't need to be
    {
//        if (/*isclosing()*/ /*isnull(this)*/ !THIS()) return 0;
        size_t retval = m_head.fetch_add(buflen); //% sizeof(pool); //prev atomic += didn't adjust for wrap-around, so do it here
//wrapwrite will wrap its private copy of write ofs to pool size, but shm copy is not updated
//eventual shm copy overflow will cause incorrect wrap (misalignment) if !power of 2, so update shm copy of write ofs when needed:
//no        if (retval + buflen > sizeof(pool)) head.fetch_add(-sizeof(pool)); //retval -= sizeof(pool); } //wrapped
        if (retval + buflen < retval) exc_throw("log size wrapped");
//        size_t tail_bump = retval;
//        tail.compare_exchange_strong(tail_bump, (retval + 1) % sizeof(pool)); //kludge: tail == head means empty; bump tail if full to avoid ambiguity
// |==========pool==============|
// |============tail===new-head=|
// |==new-head==tail============|
//        for (;;)
//        {
//            size_t svtail = tail;
//            if ((svtail >= retval) && (svtail < retval + buflen))
//            if ()
//        }
//        while (!head.compare_exchange_weak(retval, retval % sizeof(pool)); //CAS loop; https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/
//NOTE: race condition exists while buf is being filled in; mitigate by pre-filling with nulls (race window is much shorter, but still there):
//        retval %= sizeof(pool);
//        size_t wraplen = std::min(buflen, sizeof(pool) - retval);
//        if (!buf) return ofs; //caller will write
//        if (wraplen) memset(pool + retval, 0, wraplen);
//        if (wraplen < buflen) memset(pool, 0, buflen - wraplen);
        return retval;
    }
    size_t write(const char* buf, size_t buflen, size_t ofs)
    {
//        if (!buf) return ofs; //caller will write
//no        if (!buflen) buflen = strlen(buf);
//        if (THIS()) ///*!isclosing()*/ !isnull(this) && !isclosing())
//        if (/*isclosing()*/ !this) return 0;
        ofs %= sizeof(m_storage);
        size_t wraplen = std::min(buflen, sizeof(m_storage) - ofs);
        if (wraplen) memcpy(m_storage + ofs, buf, wraplen); //std::min(intro_len, sizeof(pool) - ofs));
//            int wrap_len = ofs + intro_len - sizeof(pool);
        if (wraplen < buflen) memcpy(m_storage, buf + wraplen, buflen - wraplen);
//            if (nonshm_log().length() >= sizeof(pool)) show_log(); //don't let it get too big
        return buflen;
    }
//CAUTION: blocking wait; should only be used on bkg threads
    std::string read(bool wait) //int maxents = 0)
    {
#if 0 //read() doesn't need to be lockless; use mutex so cvar can also be used
        for (;;) //retry lockless read
        {
            std::string retval;
            size_t svtail = m_tail;
            int loglen = m_head - svtail; //CAUTION: atomic read on head allows new log entries to come in; tail assumed safe (only one reader)
            if (loglen < 0) exc_throw("log wrapped: " << (loglen + svtail) << " <- " << svtail); //loglen += sizeof(pool); //wrapped
            
            size_t newtail = svtail + loglen;
            loglen %= sizeof(m_storage); //can't exceed buf size
            svtail %= sizeof(m_storage);
//            size_t svretlen = retval.length();
            size_t wraplen = std::min(sizeof(m_storage) - svtail, (size_t)loglen);
            retval.append(m_storage + svtail, wraplen);
            retval.append(m_storage, loglen - wraplen);
//                if (tail == svtail) break; //no new log entries were written while reading
            if (m_tail.compare_exchange_strong(svtail, newtail)) return retval; //break; //svtail + loglen) % sizeof(pool)); //kludge: tail == head means empty; bump tail if full to avoid ambiguity
//            retval.erase(svretlen, loglen); //try again
            ++m_retries;
        }
#endif
        LOCKTYPE lock(m_mutex);
        for (;;)
        {
            int loglen = m_head - m_tail; //atomic head allows new log entries to come in; tail is locked (only one reader)
            if (loglen < 0) exc_throw("log wrapped: " << (loglen + m_tail) << " <- " << m_tail); //loglen += sizeof(pool); //wrapped
            loglen %= sizeof(m_storage); //can't exceed buf size
            size_t tailofs = m_tail % sizeof(m_storage);
            std::string retval;
            size_t wraplen = std::min(sizeof(m_storage) - tailofs, (size_t)loglen);
            retval.append(m_storage + tailofs, wraplen);
            retval.append(m_storage, loglen - wraplen);
            m_tail += loglen; //clear unread log
            if (retval.length() || !wait) return retval;
            m_cvar.wait(lock);
        }
    }
//    void wake() {} //noop
};


////////////////////////////////////////////////////////////////////////////////
////
/// shared memory logger:
//

//#include <cstdlib> //getenv(), atexit()
//#include <regex> //std::regex, std::regex_match()
//#include <sstream> //std::ostringstream
//#include <stdexcept> //std::runtime_error
//#include <utility> //std::forward<>
#include <stdio.h> //<cstdio> //vsnprintf()
#include <stdarg.h> //varargs, va_list
//#include <cctype> //isspace()
//#include <regex> //std::regex, std::cmatch, std::regex_match()
#include <string>
#include <string.h> //strrchr(), strlen(), strcmp(), strncpy(), strstr()
//#include <cstdlib> //atoi(), getenv()
//#include <vector>
#include <thread> //std::thread::*
#include <mutex> //std:mutex<>, std::unique_lock<>
#include <atomic> //std::atomic<>
#include <algorithm> //std::max(), std::min()


//#include "msgcolors.h" //MSG_*, ENDCOLOR_*
#include "str-helpers.h" //substr, NVL(), commas(), PreallocVector<>
#include "shmalloc.h" //shmalloc(), shmfree(), STATIC_WRAP


#if 0 //experimental
//from above:
//#define MAX_DEBUG_LEVEL
//#define exc exc_hard
//#define warn exc_soft
#undef exc_hard
#define exc_hard(...)  logprintf(-1, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
#undef exc_soft
#define exc_soft(...)  logprintf(-2, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
//#define DebugInOut(...)  InOutDebug inout(SRCLINE, std::ostringstream() << __VA_ARGS__)

//use macro so SRCLINE will be correct
//compiler should filter out anything > MAX_DEBUG_LEVEL, leaving only the second condition for run-time eval
//TODO: use lamba function for lazy param eval (better run-time perf, can leave debug enabled)
#define log(level, ...)  ((((level) <= MAX_DEBUG_LEVEL) && ((level) <= debug_level))? (logprintf(level, SRCLINE, std::ostringstream() << __VA_ARGS__), 0): 0) //filter out *max* detail at compile time
//#else
// #define log  if (0) debug
#endif


//debug/diagnostic log msgs and related info:
//don't want anything slowing down gpu_wker (file or console output could cause screen flicker),
// so log messages are stored in shm and another process can read them aynchronously if interested
//circular fifo is used; older msgs will be overwritten automatically; no memory mgmt overhead
#if 1
//static std::mutex& nonshm_mutex()
//{
//    static std::mutex m_mutex;
//    return m_mutex;
//}
//static /*thread_local*/ std::string& nonshm_log() //collect msgs in local mem until shm set up
//{
//    static std::string m_str;
//    return m_str;
//}
//CAUTION: must be inited first/destroyed last; dcl separatedly from LogInfo
//static struct nonshm
//{
//    std::mutex mutex;
//    std::string str;
//    nonshm() {}
//    ~nonshm() { printf("NON-SHM @EXIT:\n%s\n", str.c_str()); }
//} LogInfo_nonshm;
struct LogInfo
{
    static const key_t SHMKEY = 0xF00D0000; //| sizeof(LogInfo); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
    const uint32_t m_hdr = VALIDCHK; //bytes[0..3]; 1 x int32
    std::mutex mtx; //for thrinx() and local log only; other methods are atomic/lockless
    using LOCKTYPE = std::unique_lock<decltype(mtx)>; //not: std::lock_guard<decltype(m_mtx)>;
//    PreallocVector<thrid, 8> thrids;
//    std::atomic<int32_t> latest; //lockless struct
//    std::atomic<time_t> epoch = now();
//        struct
//        {
//TODO: structured msg info?
//            elapsed_t timestamp;
//            char msg[120]; //free-format text (null terminated)
//        } log[100];
//    char msgs[100][120];
//    size_t head, tail;
//    std::atomic<size_t> head = 0;
//    char pool[0x4000] = ""; //16K will hold ~200 entries of 80 char
//    static STATIC_WRAP(bool, isclosing, = true);
//    static LogInfo*& singleton()
//    {
//        static LogInfo* m_singleton = 0;
//        return m_singleton;
//    }
//public: //ctors/dtors
//    LogInfo() //: m_epoch(now())
//    {
//        latest = 0; //not really needed (log is circular), but it's nicer to start in a predictable state
//        for (int i = 0; i < SIZEOF(msgs); ++i)
//        {
////                log[i].timestamp = 0;
//            msgs[i][0] = '\0';
//        }
//    }
//public: //operators
public: //timer methods
//    typedef /*decltype(now())*/ /*int64_t*/ uint32_t time_t; //32 bits is enough for ~ 1200 hours of msec or ~1.2 hr of usec
//    typedef /*decltype(elapsed())*/ uint32_t elapsed_t;
//    static time_t now() //msec
//    {
//        using namespace std::chrono;
//        return /*std::chrono::*/duration_cast</*std::chrono::*/milliseconds>(/*std::chrono::*/system_clock::now().time_since_epoch()).count();
//    }
    std::atomic<decltype(Now())> epoch /*= now()*/;
//    inline elapsed_t elapsed() { return !isnull(this)? now() - epoch: 0; } //msec
//allow caller to adjust:
    /*inline*/ elapsed_t elapsed(elapsed_t reset = NO_RESET) //, int scaled = 1) //Freq = #ticks/second
    {
        if (/*isclosing()*/ /*isnull(this)*/ !THIS()) return -1; //unknown
//    started += delta; //reset to now() each time called
        elapsed_t retval = Now() - epoch;
        if (reset != NO_RESET)
        {
            epoch += retval - reset; //set new epoch; CAUTION: lockless
            if (retval != reset) debug(0, "adjust elapsed %u -> %u %c%u", retval, reset, "+-"[reset < retval], std::max(reset, retval) - std::min(reset, retval));
        }
//        return /*scaled? (double)delta * scaled / SDL_TickFreq():*/ delta; //return actual time vs. #ticks
        return retval; //elapsed(); //msec
    }
//    static inline elapsed_t elapsed() { return singleton()->elapsed(); } //msec
//    static inline elapsed_t elapsed(elapsed_t reset) { return singleton()->elapsed(reset); }
//elapsed.pause = function() { elapsed.paused || (elapsed.paused = elapsed.now()); }
//elapsed.resume = function() { if (elapsed.paused) elapsed.epoch += elapsed.now() - elapsed.paused; elapsed.paused = null; }
//protected: //data members
//    time_t m_epoch;
public: //thread methods
#if 0
    static inline /*auto*/ std::thread::id thrid()
    {
//TODO: add pid for multi-process uniqueness?
        return std::this_thread::get_id();
    }
#endif
//reduce verbosity by using a unique small int instead of thread id:
    PreallocVector</*thrid*/ std::thread::id, 8> thrids; //TODO: xfr to local during ctor/dtor?
    int thrinx()
    {
        thread_local static int my_thrinx = -1; //cached
        if (my_thrinx == -1) my_thrinx = thrinx(thrid);
        return my_thrinx;
    }
    int thrinx(const std::thread::id/*auto*/& newid) //= thrid)
    {
//        static std::vector</*std::decay<decltype(thrid())>*/std::thread::id> ids;
//        static std::mutex mtx;
        if (/*isclosing()*/ /*isnull(this)*/ !THIS()) return -1; //unknown
        LOCKTYPE lock(mtx);

        for (auto it = thrids.begin(); it != thrids.end(); ++it)
            if (*it == newid) return it - thrids.begin();
//        int newinx = thrids.size();
//        thrids.push.back(newid);
//        LOCKTYPE lock(mtx);
        for (auto it = thrids.begin(); it != thrids.end(); ++it)
            if (*it == (std::thread::id)0) { *it = newid; return it - thrids.begin(); }
//        return newinx;
        exc_throw("Prealloc thrid vector[" << thrids.size() << "] is full");
//        return -1;
    }
//    static inline int thrinx(const std::thread::id/*auto*/& newid = thrid()) { return singleton()->thrinx(); }
public: //logging methods
    struct //LogStats
    {
        std::atomic<int> num_writes{0} /*= 0*/, num_ovfl{0} /*= 0*/; //total #log msgs, #fmtbuf ovfls
        std::atomic<int> buf_len{0} /*= 0*/, ovfl_len{0} /*= 0*/, move_len{0} /*= 0*/; //total buf, ovfl, memmove len
        std::atomic<int> color_resets{0} /*= 0*/; //#color ends within buf (indicates multi-colored log msgs)
        std::atomic<int> re_reads{0}; //lockless write retries
//        LogStats() //std::atomic<int>& retries): re_reads(retries) //kludge: avoid atomic<> "deleted function" errors
//        {
//            num_writes.store(0);
//            num_ovfl.store(0);
//            buf_len.store(0);
//            ovfl_len.store(0);
//            move_len.store(0);
//            color_resets.store(0);
//            re_reads.store(0);
//        }
//    };
//    LogStats stats;
    } stats;
//    std::atomic<size_t> head, tail /*= 0*/;
//    char pool[0x4000] = ""; //16K will hold ~200 entries of 80 char
    CircularLog m_shmlog; //(stats.re_reads);
#if 0 //CAUTION: must be inited before and destroyed after all member data
    static std::mutex& nonshm_mutex()
    {
        static std::mutex m_mutex;
        return m_mutex;
//        return LogInfo_nonshm.mutex;
    }
    static /*thread_local*/ std::string& nonshm_log() //collect msgs in local mem until shm set up
    {
//NO        static /*std::string*/ debug_string m_str; //move outside class so it can outlive instances
        static bool disp_later = false;
        if (!disp_later) { disp_later = true; atexit([]() { show_log(false); }); }
//        {
//            disp_later = true;
//            atexit([]() { show_log(false); });
//            {
//                if (m_str.length()) { printf("residual log:\n%s", m_str.c_str()); fflush(stdout); }
//                m_str.clear();
//            });
//        }
        return m_str;
//        return LogInfo_nonshm.str;
    }
    static void show_log(bool clear = true)
    {
//        if (!clear) { printf(CYAN_MSG "flush nonshm log" ENDCOLOR_NEWLINE); fflush(stdout); }
        std::string& m_str = nonshm_log();
        if (!clear) { printf(CYAN_MSG "flush nonshm log len %u" ENDCOLOR_NEWLINE, m_str.length()); fflush(stdout); }
        if (!m_str.length()) return;
        printf("%slog:\n%s", clear? "residual ": "", m_str.c_str());
        fflush(stdout);
        if (clear) m_str.clear();
    }
#endif
//NOTE: wrapper macro already decided whether to keep this message or not
/*void*/ int logprintf(int level, SrcLine srcline, const char* fmt, va_list args) //...) //use ret type to allow conditional/ternary usage
    {
//        bool THIS = !isnull(this); //kludge: "!this" no worky with g++ on RPi
//        bool nullthis = (this == 0); //kludge: "!this" doesn't seem to work on RPi
        if (THIS()) //!isnull(this))
        {
//            printf("null? %p %d %d %d %d %d %d %d %d %d\n", this, !this, this != 0, !!this, this == 0, this == nullptr, reinterpret_cast<intptr_t>(this) == 0, (this == reinterpret_cast<LogInfo*>(0)), (this < reinterpret_cast<LogInfo*>(0x100)), 123);
//            printf("null? %p %d 0x%x 0x%x %d %d %d %d\n", this, sizeof(this), reinterpret_cast<intptr_t>(this), this == reinterpret_cast<LogInfo*>(1), (this > reinterpret_cast<LogInfo*>(0)), (this < reinterpret_cast<LogInfo*>(1)), (this < reinterpret_cast<LogInfo*>(2)), 123);
//            printf("null? %p %p %d %d\n", this, &stats, !isnull(this), 123);
            if (&stats.num_writes < reinterpret_cast<std::atomic<int>*>(0x100)) exc_throw("whoops: bad this!");
        }
//HERE(1);
//        if (isclosing()) return 0;
//        if (/*!isclosing()*/ this) first_time();
//    if (level > MAX_DEBUG_LEVEL) return 0; //caller doesn't want this much detail
//printf("logprintf(%d, '%s', '%s', ...)\n", level, NVL(srcline, SRCLINE), fmt);
        const char* msg_color = 
            (level <= DetailLevel::ERROR_LEVEL)? RED_MSG: 
            (level <= DetailLevel::WARN_LEVEL)? YELLOW_MSG: 
            BLUE_MSG;
        const size_t msg_colorlen = strlen(msg_color);
//pre-fmt basic msg contents:
        char fmtbuf[1000]; //should be large enough for most msgs
//        va_list args;
//        va_start(args, fmt);
        size_t fmtlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args); //+ 1;
//        if (fmtlen >= sizeof(fmtbuf)) stats.ovfl_len += fmtlen - sizeof(fmtbuf);
//        stats.buf_len += fmtlen;
//        ++stats.num_writes;
        va_end(args);
//if (level >= 0) printf("debug%d: %u:'%.*s'%s from '%s'\n", level, fmtlen, std::min(sizeof(fmtbuf) - 1, fmtlen), fmtbuf, (fmtlen >= sizeof(fmtbuf))? "...": "", NVL(srcline, "??")); //fflush(stdout);
//        static const char* ColorEnd = ENDCOLOR_NOLINE;
//HERE(12);
//printf("logpr lvl %d, src '%s', msg '%.*s'\n", level, NVL(srcline, "(none)"), 200, fmtbuf);
        static const size_t endlen = strlen(ENDCOLOR_NOLINE); //ANSI_COLOR("0")
//printf("from %s, raw fmtlen %u", NVL(srcline, "??"), fmtlen); fflush(stdout);
        if ((fmtlen >= endlen) && (fmtlen < sizeof(fmtbuf)) && !strcmp(fmtbuf + fmtlen - endlen, ENDCOLOR_NOLINE)) fmtbuf[fmtlen -= endlen] = '\0'; //drop trailing ENDCOLOR; going to re-add it later anyway
//        std::vector<size_t> color_resets;
        for (char* bp = fmtbuf; bp = strstr(bp, ENDCOLOR_NOLINE); bp += endlen) //infrequent, and hopefully occurrences are toward buf end so memmove is not too expensive
        {
//            color_resets.push(bp - fmtbuf);
            size_t tail_len = fmtbuf + std::min(sizeof(fmtbuf) - std::max(msg_colorlen - endlen, 0UL) - 1, fmtlen) - (bp + endlen);
//printf(", mv len %u, mv? %d", tail_len, msg_colorlen != endlen); fflush(stdout);
            if (msg_colorlen != endlen)
            {
                memmove(bp + msg_colorlen, bp + endlen, tail_len); //excl null terminator
                bp[msg_colorlen + tail_len] = '\0';
            }
            strncpy(bp, msg_color, msg_colorlen);
            fmtlen += msg_colorlen - endlen;
            if (/*isclosing()*/ /*!this*/ /*isnull(this)*/ !THIS()) continue; //no stats
            stats.move_len += tail_len;
            ++stats.color_resets;
        }
//printf(", re-ended fmtlen %u, '%.*s' %s\n", fmtlen, 40, fmtbuf, (fmtlen >= sizeof(fmtbuf))? "...": ""); fflush(stdout);
//HERE(2);
//find first + last color codes so we can insert more text correctly into msg:
#if 0
//        const ColorCodes = /\x1b\[\d+(;\d+)?m/g; //ANSI color escape codes; NOTE: need /g to get last match
//    static const char* ColorCodes_ldr = ANSI_COLOR("\0"); //kludge: inject null color to truncate, then use string search instead of regex for better performance; //code)  "\x1b[" code "m"
//    static const char* ColorCodes_tlr = strend(ColorCodes_ldr) + 1; //skip injected null to get remainder of string
        static const /*std::string*/ char* ColorCodes = ANSI_COLOR("###"); //use dummy color code for length parsing
        static const size_t prefix_len = strstr(ColorCodes, "###") - ColorCodes; //.find("###") - ColorCodes.begin();
//        static const char* ColorCodes_trailer = ColorCodes + cc_startlen + 3;
//        static const size_t suffix_ofs = prefix_len + strlen("###"); //strlen(ColorCodes + prefix_len) - strlen("###");
        static const size_t suffix_len = strlen(ColorCodes + prefix_len + 3); //suffix_ofs);
//    static const size_t cc_endlen = strlen(ColorCodes + cc_startlen + 3);
        const char* bp = !strncmp(fmtbuf, ColorCodes, prefix_len)? strstr(fmtbuf + prefix_len, ColorCodes + prefix_len + 3): 0;
        const size_t leading_colorend = bp? bp + suffix_len - fmtbuf: 0; //!0 == end of leading color code
#else
        const size_t leading_colorend = [](const char* fmtbuf) -> size_t
        {
            static const /*std::string*/ char* ColorCodes = ANSI_COLOR("###"); //use dummy color code for length parsing
            static const size_t prefix_len = strstr(ColorCodes, "###") - ColorCodes; //.find("###") - ColorCodes.begin();
            static const size_t suffix_len = strlen(ColorCodes + prefix_len + 3); //suffix_ofs);
            const char* bp = !strncmp(fmtbuf, ColorCodes, prefix_len)? strstr(fmtbuf + prefix_len, ColorCodes + prefix_len + 3): 0;
            return bp? bp + suffix_len - fmtbuf: 0; //!0 == end of leading color code
        }(fmtbuf);
#endif
//        bool ends_colored = strcmp(fmtbuf + std::min(fmtlen, sizeof(fmtbuf)) - strlen(EndColor), EndColor);
//        size_t first_ofs, last_ofs = 0; //text ins offsets at beginning and end of fmt str
//        {
//            const char* match = strstr(fmtbuf + prevofs, ColorEnd); //strnstr(fmtbuf + prevofs, ColorCodes, prefix_len); //ColorCodes.exec(fmt);
//printf("match %u, ", match - fmtbuf);
//            if (!match) break;
//            color_resets.push(match - fmtbuf);
//            const char* match_end = strstr(match + prefix_len, ColorCodes + suffix_ofs);
//printf("match end %u, ", match_end - fmtbuf);
//            if (!match_end) break; //not a valid color code?
//            match_end += suffix_len;
//        if (match_end - match > cc_len) { last_ofs += cc_startlen; continue; } //skip partial and find next one
//console.log("found last", match, ColorCodes);
//        last_ofs = match[0].length; //(match && !match.index)? match[0].length: 0; //don't split leading color code
//        if (!first_ofs) first_ofs = last_ofs;
//console.log(match[0].length, match.index, ColorCodes.lastIndex, match);
//            if (!last_ofs) first_ofs = (match == fmtbuf)? (match_end - fmtbuf): 0; //(match && !match.index)? match[0].length: 0; //don't split leading color code
//            last_ofs = (match_end == fmtbuf + fmtlen)? (match - fmtbuf): fmtlen; //ColorCodes.lastIndex;
//printf("first %u, last %u, look again ...\n", /*match - fmtbuf, match_end - fmtbuf,*/ first_ofs, last_ofs);
//        }
//show warning if fmtbuf too short:
//    std::ostringstream tooshort;
        if (THIS()) //!isnull(this)) //!isclosing())
        {
//            if (&stats.num_writes < reinterpret_cast<std::atomic<int>*>(0x100)) exc_throw("whoops!");
            ++stats.num_writes;
            stats.buf_len += fmtlen;
        }
        if (fmtlen >= sizeof(fmtbuf))
        {
            static const int RESERVE = 20;
            if (THIS()) //!isnull(this)) //!isclosing())
            {
                ++stats.num_ovfl;
                stats.ovfl_len += fmtlen - sizeof(fmtbuf);
            }
            fmtlen = sizeof(fmtbuf) - RESERVE + snprintf(&fmtbuf[sizeof(fmtbuf) - RESERVE], RESERVE, " >> %s ...", commas(fmtlen));
        }
//HERE(3);
//TODO: replace color resets with msg_color
//        if (!last_ofs) { first_ofs = 0; last_ofs = fmtlen; }
//printf("FINAL: first %u, last %u, cc start %u, cc end %u\n", /*match - fmtbuf, match_end - fmtbuf,*/ first_ofs, last_ofs, cc_startlen, cc_endlen);
//    std::ostringstream ss;
//    ss << ENDCOLOR_ATLINE(srcline);
//    if (strstr(fmtbuf, )) srcline = ""; //already there; don't add again
//#define ENDCOLOR  "  &" SRCLINE ENDCOLOR_NOLINE //use const char* if possible; don't call shortsrc()
//    static SrcLine me = SRCLINE;
//HERE(13);
        size_t src_insofs = fmtlen, srclen = 0;
        if (!srcline) srcline = SRCLINE; //use self if caller unknown
        char srcbuf[80];
        {
            const char* bp = skip_folder(srcline); //strrchr(srcline, '/');
            if (bp != srcline)
            {
//printf("srcline was %d:'%s', last '/' @ ofs %d", strlen(srcline), srcline, bp - srcline);
                size_t prefix = skipspaces(srcline) - srcline;
                strncpy(srcbuf, srcline, prefix + 1);
                strcpy(srcbuf + prefix + 1, bp); //+ 1);
                srcline = srcbuf; //drop folder name; base file name should be enough
//printf(" => %d:'%s'\n", strlen(srcbuf), srcbuf);
            }
        }
//        srcline = skipspaces(srcline);
#if 0
        srclen = strlen(srcline);
        src_insofs = fmtlen;
#else
//    while (isspace(*srcline)) ++srcline;
        if (strstr(fmtbuf, skipspaces(srcline) + 1)); //printf("found '%s' in '%s'\n", skipspaces(srcline), fmtbuf); //srclen = 0; //srcline = 0; //""; //don't repeat if already there
        else //try again with just filename part, no line#
        {
            const char* bp = strchr(srcline, ':');
//printf("srcline %p, bp %p, bp - srcline %u\n", srcline, bp, bp - srcline);
            char* foundofs = bp? strnstr(fmtbuf, srcline, bp - srcline): 0;
//printf("found %d:'%s' @%d in %d:'%s'\n", bp - srcline, srcline, foundofs? foundofs - fmtbuf: -1, fmtlen, fmtbuf);
            if (foundofs) //same file, different line; just show additional line#
            {
                src_insofs = foundofs - fmtbuf + (bp - srcline);
                srcline = bp;
            }
//            else srclen = 0;
//            else src_insofs = fmtlen;
            srclen = strlen(srcline);
        }
//        if (!srclen) { src_insofs = /*fmtbuf +*/ /*std::min(fmtlen, sizeof(fmtbuf) - 1)*/ fmtlen; srcline = ""; }
#endif
//send msg to stderr or stdout, depending on severity:
//        static std::mutex mtx;
        thread_local static int /*numerr = 0,*/ count = 0; //show thread info once
//no        LOCKTYPE lock(mtx); //avoid interleaved output between threads
//        bool want_shmlog = THIS(); //decide once so log entry won't be fragmented
        LogFile* logp = THIS()? &m_shmlog: (true? &nonshm_log: logp); //decide once so log entry won't be fragmented; local vs. shared state change ignored below; ternary ptr trick from https://stackoverflow.com/questions/6179314/casting-pointers-and-the-ternary-operator-have-i-reinvented-the-wheel
//    static int count = 0;
//    if (!count++) printf(CYAN_MSG "[elapsed-msec $thread] ===============================\n" ENDCOLOR_NOLINE);
        if (!count++) //show thread info first time
        {
            char intro_buf[80];
            size_t intro_len = snprintf(intro_buf, sizeof(intro_buf), PINK_MSG "[msec $thr] ======== thread# %d, id 0x%x, pid %d ======== @srcline.lvl" ENDCOLOR_NEWLINE, thrinx(), thrid, getpid());
            logp->writex(intro_buf); //, intro_len /*+ 1*/);
//            if (isnull(this)) printf("%.*s", intro_len, intro_buf);
        }
//    const char* hdr_color = (level == -1)? RED_MSG: (level == -2)? YELLOW_MSG: PINK_MSG;
//    if (level < 0) //error
//    {
//for (char* bp = fmtbuf; bp = strchr(bp, '\n'); *bp++ = '\\');
//for (char* bp = fmtbuf; bp = strchr(bp, '\x1b'); *bp++ = '?');
//printf("first ofs %d, last ofs %d, len %d, cc start %u, end %u, buf \"%s\"\n", first_ofs, last_ofs, fmtlen, cc_startlen, cc_endlen, fmtbuf); fflush(stdout);
//HERE(4);
        char lvlbuf[7];
        size_t lvl_len = snprintf(lvlbuf, sizeof(lvlbuf), ".%d", level);
        char timestamp[20];
        size_t timest_len = snprintf(timestamp, sizeof(timestamp), /*!isnull(this)*/ "[%s $%c] ", THIS()? commas((double)elapsed() / 1000, "%4.3f"): "?.???", THIS()? '0' + thrinx(): '?'); //, level);
//    if (undecorated) { ss << FMT("%4.3f") << elapsed_msec(); return ss.str(); }
//        fprintf(fout, "%s%.*s[%s $%d] %.*s%s%s" ENDCOLOR_NEWLINE, msg_color, first_ofs, fmtbuf, my_timestamp(true).c_str(), thrinx(), last_ofs - first_ofs, fmtbuf + first_ofs, srcline, fmtbuf + last_ofs);
//        substr full_msg[]
        size_t buflen = (!leading_colorend? msg_colorlen: 0) + timest_len + fmtlen + /*color.resets.length() * (msg_colorlen - endlen) + (srcline? strlen(srcline): 0)*/ srclen + lvl_len + endlen + 1;
//size_t buflen1 = !leading_colorend? msg_colorlen: 0, buflen2 = timest_len, buflen3 = fmtlen , buflen4 = srclen, buflen5 = endlen;
//alloc space for msg in shm log, then write it piece by piece:
        size_t wrofs = logp->alloc(buflen /*+ 1*/), svofs = wrofs;
//size_t svofs2 = wrofs;
        if (!leading_colorend) wrofs += logp->write(msg_color, msg_colorlen, wrofs);
        else wrofs += logp->write(fmtbuf, leading_colorend, wrofs);
//if (wrofs - svofs2 != (!leading_colorend? msg_colorlen: leading_colorend)) exc_throw("bad wrwr len1: got " << (wrofs - svofs2) << ", expected " << (!leading_colorend? msg_colorlen: leading_colorend));
//svofs2 = wrofs;
        wrofs += logp->write(timestamp, timest_len, wrofs);
//if (wrofs - svofs2 != timest_len) exc_throw("bad wrwr len2: got " << (wrofs - svofs2) << ", expected " << timest_len);
//svofs2 = wrofs;
//        if (srclen)
//        {
//size_t svofs2 = wrofs;
        wrofs += logp->write(fmtbuf + leading_colorend, /*(srclen? src_insofs: fmtlen)*/ src_insofs - leading_colorend, wrofs);
//if (wrofs - svofs2 != src_insofs - leading_colorend) exc_throw("bad wrwr len3: got " << (wrofs - svofs2) << ", expected " << (src_insofs - leading_colorend));
//svofs2 = wrofs;
        wrofs += logp->write(srcline, srclen, wrofs);
//if (wrofs - svofs2 != srclen) exc_throw("bad wrwr len3: got " << (wrofs - svofs2) << ", expected " << srclen);
//svofs2 = wrofs;
//        }
        if (srclen) wrofs += logp->write(fmtbuf + src_insofs, fmtlen - src_insofs, wrofs);
//if (srclen && (wrofs - svofs2 != fmtlen - src_insofs)) exc_throw("bad wrwr len4: got " << (wrofs - svofs2) << ", expected " << (fmtlen - src_insofs));
//svofs2 = wrofs;
        wrofs += logp->write(lvlbuf, lvl_len, wrofs);
        wrofs += logp->write(ENDCOLOR_NEWLINE, endlen + 1, wrofs);
//if (wrofs - svofs2 != endlen + 1) exc_throw("bad wrwr len4: got " << (wrofs - svofs2) << ", expected " << endlen + 1);
        if (wrofs != svofs + buflen) exc_throw(/*throw std::runtime_error((std::ostringstream() << RED_MSG <<*/ "wrapwrite wrong len: got " << wrofs << ", expected " << svofs + buflen); // << ENDCOLOR_NEWLINE).str());
        logp->wake(); //notify listeners
//        FILE* fout = (level < 0)? stderr: stdout;
//        if (level < 0) { fflush(stdout); fflush(fout); } //make sure important msgs are output; might as well incl stdout as well
//        if (level == -1) throw std::runtime_error(fmtbuf);
//printf("((%s))\n", fmtbuf);
//HERE(5);
        if (level <= DetailLevel::ERROR_LEVEL) exc_throw(/*throw std::runtime_error((std::ostringstream() << RED_MSG <<*/ fmtbuf); //<< ENDCOLOR_NEWLINE).str());
        else if (level <= DetailLevel::WARN_LEVEL) { fprintf(stderr, YELLOW_MSG "%s" ENDCOLOR_NEWLINE, fmtbuf); fflush(stderr); } //make sure important msgs are output
//if (nonshm_log().length()) printf("nonshm_log len is now %u\n", nonshm_log().length());
//also show on screen if no shm:
//        else if (true || isnull(this)) printf(BLUE_MSG "%.*s%s%.*s%s%s" ENDCOLOR_NEWLINE, leading_colorend, fmtbuf, timestamp, src_insofs - leading_colorend, fmtbuf + leading_colorend, srcline, fmtbuf + src_insofs);
#if 0
        else if (/*true ||*/ isnull(this)) printf("%.*s%s%.*s%.*s%.*s" ENDCOLOR_NEWLINE, NVL(leading_colorend, msg_colorlen), leading_colorend? fmtbuf: msg_color, timestamp, src_insofs - leading_colorend, fmtbuf + leading_colorend, srclen, srcline, srclen? fmtlen - src_insofs: 0, fmtbuf + src_insofs);
#endif
//        else printf(fmtbuf); //TEMP
//HERE(6);
    }
//    inline static int logprintf(int level, SrcLine srcline, const char* fmt, va_list args) { return singleton()->logprintf(level, srcline, fmt, args); }
//CAUTION: wait is blocking; should only be used on bkg threads
    std::string read_log(bool wait) //int maxents = 0)
    {
//        std::string retval;
//        if (THIS()) retval = nonsmh_log.read();
        return THIS()? m_shmlog.read(wait): nonshm_log.read(wait); //assume local vs. shared state doesn't change while reading
#if 0
        {
            LOCKTYPE lock(nonshm_mutex());
//            std::lock_guard<std::mutex> guard(nonshm_mutex()); //only allow one thread to read/write non-shm at a time
            if (nonshm_log().length()) //get non-shm msgs first
            {
                retval.append("NON-SHM:\n");
                retval += nonshm_log(); //CAUTION: assume doesn't change while reading
                nonshm_log().clear(); //erase(0, retval.length()); //should only be 1 writer
                retval.append("SHM:\n");
            }
        }
        if (THIS()) //!isnull(this))
#if 0
            for (const char* bp = pool + tail; bp != pool + head;)
            {
                for (;;)
                {
                    size_t len = strnlen(bp, &pool[sizeof(pool)] - bp);
                    retval.append(bp, len);
                    bp += len;
                    if (bp != &pool[sizeof(pool)]) break; //end of entry
                    if (len == sizeof(pool)) break; //already wrapped
                    bp = pool; //entry wrapped
                }
                retval.append("\n");
                tail/*.store*/ = ++bp - pool;
                if (maxents && !--maxents) break;
//                if (tail == head) break;
            }
#else
            for (;;) //retry lockless read
            {
                size_t svtail = tail;
                int loglen = head - svtail; //CAUTION: atomic read on head allows new log entries to come in; tail assumed safe (only one reader)
                if (loglen < 0) exc_throw("log wrapped: " << (loglen + svtail) << " <- " << svtail); //loglen += sizeof(pool); //wrapped
                size_t newtail = svtail + loglen;
                loglen %= sizeof(pool); //can't exceed buf size
                svtail %= sizeof(pool);
                size_t svretlen = retval.length();
                size_t wraplen = std::min(sizeof(pool) - svtail, (size_t)loglen);
                retval.append(pool + svtail, wraplen);
                retval.append(pool, loglen - wraplen);
//                if (tail == svtail) break; //no new log entries were written while reading
                if (tail.compare_exchange_strong(svtail, newtail)) break; //svtail + loglen) % sizeof(pool)); //kludge: tail == head means empty; bump tail if full to avoid ambiguity
                retval.erase(svretlen, loglen); //try again
                ++stats.re_reads;
            }
//            if (wraplen < loglen) retval.append(pool, loglen - wraplen);
//            tail += loglen;
//        }
#endif
        return retval;
#endif
    }
private: //helpers
#if 0 //moved into CircularLog
    size_t alloc(size_t buflen)
    {
        if (/*isclosing()*/ /*isnull(this)*/ !THIS()) return 0;
        size_t retval = head.fetch_add(buflen) /*% sizeof(pool)*/; //prev atomic += didn't adjust for wrap-around, so do it here
//wrapwrite will wrap its private copy of write ofs to pool size, but shm copy is not updated
//eventual shm copy overflow will cause incorrect wrap (misalignment) if !power of 2, so update shm copy of write ofs when needed:
//no        if (retval + buflen > sizeof(pool)) head.fetch_add(-sizeof(pool)); //retval -= sizeof(pool); } //wrapped
        if (retval + buflen < retval) exc_throw("log size wrapped");
//        size_t tail_bump = retval;
//        tail.compare_exchange_strong(tail_bump, (retval + 1) % sizeof(pool)); //kludge: tail == head means empty; bump tail if full to avoid ambiguity
// |==========pool==============|
// |============tail===new-head=|
// |==new-head==tail============|
//        for (;;)
//        {
//            size_t svtail = tail;
//            if ((svtail >= retval) && (svtail < retval + buflen))
//            if ()
//        }
//        while (!head.compare_exchange_weak(retval, retval % sizeof(pool)); //CAS loop; https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/
//NOTE: race condition exists while buf is being filled in; mitigate by pre-filling with nulls (race window is much shorter, but still there):
//        retval %= sizeof(pool);
//        size_t wraplen = std::min(buflen, sizeof(pool) - retval);
//        if (!buf) return ofs; //caller will write
//        if (wraplen) memset(pool + retval, 0, wraplen);
//        if (wraplen < buflen) memset(pool, 0, buflen - wraplen);
        return retval;
    }
    size_t wrapwrite(const char* buf, size_t buflen = 0) { if (!buflen) buflen = strlen(buf); return wrapwrite(buf, buflen, alloc(buflen)); }
    size_t wrapwrite(const char* buf, size_t buflen, size_t ofs)
    {
//        if (!buf) return ofs; //caller will write
//no        if (!buflen) buflen = strlen(buf);
        if (THIS()) ///*!isclosing()*/ !isnull(this) && !isclosing())
        {
//        if (/*isclosing()*/ !this) return 0;
            ofs %= sizeof(pool);
            size_t wraplen = std::min(buflen, sizeof(pool) - ofs);
            if (wraplen) memcpy(pool + ofs, buf, wraplen); //std::min(intro_len, sizeof(pool) - ofs));
//            int wrap_len = ofs + intro_len - sizeof(pool);
            if (wraplen < buflen) memcpy(pool, buf + wraplen, buflen - wraplen);
        }
        else
        {
            LOCKTYPE lock(nonshm_mutex());
//            std::lock_guard<std::mutex> guard(nonshm_mutex()); //only allow one thread to read/write non-shm at a time
            nonshm_log().append(buf, buflen);
            if (nonshm_log().length() >= sizeof(pool)) show_log(); //don't let it get too big
        }
        return buflen;
    }
#endif
    const uint32_t m_tlr = VALIDCHK;
//    /*txtr_bb*/ /*SDL_AutoTexture<XFRTYPE>*/ TXTR m_txtr; //in-memory copy of bit-banged node (color) values (formatted for protocol)
//    InOutDebug inout2;
public: //ctors/dtors
//    explicit ShmData(int new_screen, const SDL_Size& new_wh, double new_frame_time): info(new_screen, new_wh, new_frame_time) {}
//    static STATIC_WRAP(bool, isclosing, = true);
//    static bool& isclosing() //kludge: use wrapper to avoid trailing static decl at global scope
//    {
//        static bool m_isclosing = true;
//        return m_isclosing;
//    }
    explicit LogInfo(): /*head(0), tail(0),*/ epoch(Now()), m_shmlog(stats.re_reads) //kludge: avoid atomic<> "deleted function" errors
    {
//        isclosing() = false;
//        epoch.store(Now());
//        head.store(0);
//        tail.store(0);
        std::string xfr = nonshm_log.read();
//TODO: check whether to keep each debug() item; for now, just all or nothing
        if (detail()) m_shmlog.write(xfr.c_str(), xfr.length(), xfr.length()); //xfr local to shm so caller can still get it; TODO: why is 3rd arg needed?
        debug(0, GREEN_MSG "LogInfo ctor, xfr len %u", xfr.length());
//        atexit([](){ /*if (nonshm_log.length())*/ std::string log = read_log(); printf("EXIT FLUSH:\n%.*s", log.length(), log.c_str()); });
    }
    ~LogInfo()
    {
//        debug(0, RED_MSG "LogInfo dtor"); //isclosing() = true;
        std::string xfr = m_shmlog.read();
        char buf[40];
//        nonshm_log.writex(&buf[0], (size_t)snprintf(buf, sizeof(buf), "LogInfo dtor xfr len %u\n", xfr.length())); //isclosing() = true;
        snprintf(buf, sizeof(buf), "LogInfo dtor xfr len %u\n", xfr.length()); //isclosing() = true;
        nonshm_log.writex(buf);
        nonshm_log.write(xfr.c_str(), xfr.length(), xfr.length()); //xfr local to shm so caller can still get it; TODO: why is 3rd arg needed?
    }
public: //operators
    static const uint32_t VALIDCHK = 0xf00d6789;
    bool isvalid() const { return /*!isnull(this)*/ THIS() && (m_hdr == VALIDCHK) && (m_tlr == VALIDCHK); }
public: //members
//singleton init:
//NOTE: using singleton instance to collect all data members in shm (static members are excluded, ~thread-local stg for procs)
//race conditions won't occur here; ctor/dtor only called from a single (first) thread
//    static bool& isclosing()
//    {
//        static bool m_closing = false;
//        return m_closing;
//    }
#if 0 //attach shm to each thread to allow detached threads to work correctly
    static LogInfo* const /*&*/ THIS() //singleton()
    {
//        static std::atomic<LogInfo*> m_shlptr(0); //static_cast<LogInfo*>(0));
//        shmalloc_debug(sizeof(LogInfo), SHMKEY | sizeof(LogInfo), SRCLINE))
//        static bool m_inited = false;
//        if (shmnattch(newptr) == 1) new (newptr) LogInfo; //placement "new" to call ctor; CAUTION: first time only
//        if (m_inited.compare_exchange_strong(false, true))
        static LogInfo* const NOT_READY = reinterpret_cast<LogInfo*>(-2);
//don't need atomic for thread-local ptr
        static thread_local LogInfo* m_shlptr(0); //= std::atomic<LogInfo*>(reinterpret_cast<LogInfo*>(0)); //TOALLOC;
        if (m_shlptr == NOT_READY) return 0; //CAUTION: "this" will be 0 < ctor called; called methods must check !THIS(); //CAUTION: "this" will be 0 in called methods
//        LogInfo* newptr = 0;
//        if (!m_shlptr) //== NEEDS_ALLOC)
//        if (m_shlptr.compare_exchange_strong(newptr, NOT_READY))
        if (!m_shlptr)
        {
            m_shlptr = NOT_READY;
            debug(10, "singleton init"); //static std::string& m_strlog = nonshm_log(); //kludge: force static var to be created < LogInfo ctor so it won't be destroyed until > LogInfo dtor
//printf("singleton: first time\n");
            first_time(); //get detail level info
//printf("singleton: alloc\n");
            LogInfo* newptr = static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY | sizeof(LogInfo), SRCLINE)); //O/S will dealloc shm at process exit; use at_exit() or std::unique_ptr<> to dealloc sooner
            if (shmnattch(newptr) == 1) new (newptr) LogInfo; //placement "new" to call ctor; CAUTION: first time only
            debug(10, "... using shm now ..."); //nonshm_log().append(
            m_shlptr = newptr; //allow other threads/proc to use shared instance now
        }
        return m_shlptr;
    }
#elif 1 //use atomic ptr so it can be shared with other threads
    static LogInfo* const /*&*/ THIS() //singleton()
    {
//        static std::atomic<LogInfo*> m_shlptr(0); //static_cast<LogInfo*>(0));
//        shmalloc_debug(sizeof(LogInfo), SHMKEY | sizeof(LogInfo), SRCLINE))
//        static bool m_inited = false;
//        if (shmnattch(newptr) == 1) new (newptr) LogInfo; //placement "new" to call ctor; CAUTION: first time only
//        if (m_inited.compare_exchange_strong(false, true))
        static LogInfo* const NOT_READY = reinterpret_cast<LogInfo*>(-2);
        static std::atomic<LogInfo*> m_shlptr(0); //= std::atomic<LogInfo*>(reinterpret_cast<LogInfo*>(0)); //TOALLOC;
        if (m_shlptr == NOT_READY) return 0; //CAUTION: "this" will be 0 < ctor called; called methods must check !THIS(); //CAUTION: "this" will be 0 in called methods
        LogInfo* newptr = 0;
//        if (!m_shlptr) //== NEEDS_ALLOC)
        if (m_shlptr.compare_exchange_strong(newptr, NOT_READY))
        {
            debug(10, "singleton init"); //static std::string& m_strlog = nonshm_log(); //kludge: force static var to be created < LogInfo ctor so it won't be destroyed until > LogInfo dtor
//printf("singleton: first time\n");
            first_time(); //get detail level info
//printf("singleton: alloc\n");
            /*LogInfo* */ newptr = static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY | sizeof(LogInfo), SRCLINE)); //O/S will dealloc shm at process exit; use at_exit() or std::unique_ptr<> to dealloc sooner
            if (shmnattch(newptr) == 1) new (newptr) LogInfo; //placement "new" to call ctor; CAUTION: first time only
            debug(10, "... using shm now ..."); //nonshm_log().append(
            m_shlptr = newptr; //allow other threads/proc to use shared instance now
        }
        return m_shlptr;
    }
//global wrappers to LogInfo methods:
    static elapsed_t elapsed_static() { return THIS()->elapsed(); }
    static std::string read_log_static(/*int maxents = 0*/ bool wait) { return THIS()->read_log(/*maxents*/ wait); }
    static inline int thrinx_static() { return THIS()->thrinx(); }
    static inline int thrinx_static(const std::thread::id/*auto*/& newid) { return THIS()->thrinx(newid); } //= thrid)
#else
//CAUTION: std::unique_ptr<> does not preserve normal dtor order
    static LogInfo* const /*&*/ THIS() //singleton()
    {
//        enum class State { NONE = 0, DEV_MODE, WS281X, CANCEL = -1}; //combine bkg wker control with protocol selection
//        static LogInfo* const TOALLOC = reinterpret_cast<LogInfo*>(-1);
//        static std::string& m_strlog = nonshm_log(); //kludge: force static var to be created < LogInfo ctor so it won't be destroyed until > LogInfo dtor
//TODO? attach each thread to shm separately:
//then nattach() will give global count across procs or threads, last owner thread is easier to identify, and logic below can be single-threaded
        static std::atomic<LogInfo*> m_shlptr(0); //= std::atomic<LogInfo*>(reinterpret_cast<LogInfo*>(0)); //TOALLOC;
        static LogInfo* const NOT_READY = reinterpret_cast<LogInfo*>(-2);
        static std::unique_ptr<LogInfo, /*decltype([](void* ptr)*/ std::function<void(void*)>> m_shldata(m_shlptr, [](void* ptr)
        {
            LogInfo* svptr = static_cast<LogInfo*>(ptr);
            if (svptr != m_shlptr) exc_throw("singleton shlptr mismatch @dtor");
//            isclosing() = true;
            std::string xfr_log;
            if (svptr) xfr_log = svptr->read_log();
            m_shlptr = NOT_READY; //prevent further usage during destruction
            if (!svptr) return;
//printf("xfr log %d\n", xfr_log.length()); fflush(stdout);
HERE(2);
            nonshm_log().append(xfr_log); //xfr unread shm log to local memory
//        LogInfo* shlptr = static_cast<LogInfo*>(shldata.get());
//        printf("destroy shldata: #att %d, dtor? %d\n", shmnattch(shlptr), shmnattch(shlptr) == 1);
HERE(3);
            elapsed_t olde = svptr->elapsed(), newe = Elapsed();
            debug(0, "dealloc singleton %p, xfr unread log %u to local, adjusting elapsed %u -> %u %c%u", svptr, xfr_log.length(), olde, newe, "+-"[newe < olde], std::max(newe, olde) - std::min(newe, olde));
//printf("singleton: dealloc %p\n", svptr);
//            if (!svptr) return;
#if 1 //ok now; let O/S clean up shm; this avoids problems with nonshm_log being destroyed too soon
            if (shmnattch(svptr) == 1) svptr->~LogInfo(); //call dtor but don't dealloc memory
            shmfree_debug(svptr);
#endif
        }); // ) ShmData(env, SRCLINE)); //(GpuPortData*)malloc(sizeof(*addon_data));
//        switch (m_shlptr)
//        {
//            case NEEDS_ALLOC:
//if (m_shlptr == FROZEN) printf("singleton: frozen => 0\n");
        if (m_shlptr == NOT_READY) return 0; //CAUTION: "this" will be 0 < ctor called; called methods must check !THIS(); //CAUTION: "this" will be 0 in called methods
        LogInfo* newptr = 0;
//        if (!m_shlptr) //== NEEDS_ALLOC)
        if (m_shlptr.compare_exchange_strong(newptr, NOT_READY))
        {
//            m_shlptr = NOT_READY; //CAUTION: this must be set before any calls to logprintf() to avoid recursion; first_time() and shmalloc() call it
            newptr->wrapwrite("singleton init"); //static std::string& m_strlog = nonshm_log(); //kludge: force static var to be created < LogInfo ctor so it won't be destroyed until > LogInfo dtor
//printf("singleton: first time\n");
            first_time(); //get detail level info
//printf("singleton: alloc\n");
            /*LogInfo* */ newptr = static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY | sizeof(LogInfo), SRCLINE));
            if (shmnattch(newptr) == 1) new (newptr) LogInfo; //placement "new" to call ctor; CAUTION: first time only
            debug(0, "... using shm now ..."); //nonshm_log().append(
            m_shldata.reset(m_shlptr = newptr); //remember to dealloc later; CAUTION: must call ctor first
            std::string xfr_log = newptr->read_log();
            newptr->wrapwrite(xfr_log.c_str(), xfr_log.length()); //xfr pre-obj local to shm
#if 0 //read_log() will get this info; don't need to read it here
            if (nonshm_log.length()) //move collected msgs to shm now that it's ready
            {
                wrap_write("DEQUEUE:\n");
                wrap_write(nonshm_log.c_str(), nonshm_log.length());
                nonshm_log.clear();
            }
#endif
        }
//                break;
//                return 0;
//            case FROZEN:
//                return 0;
//            default:
//                return m_shlptr;
//        }
//printf("singleton: ret %p\n", m_shlptr);
        return m_shlptr;
    }
#endif
#if 0
    LogInfo* get_shldata()
    {
//    static LogInfo* lp = ;
        static const key_t SHMKEY = LogInfo::SHMKEY | sizeof(LogInfo); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
        static LogInfo* shlptr = 0; //kludge: prevent usage during construction
//    static LogInfo* newptr = static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY, SRCLINE));
//    static bool isnew = (shmnattch(shlptr) == 1);
//    static std::unique_ptr<LogInfo, /*decltype([](void* ptr)*/ std::function<void(void*)>> shldata(static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY, SRCLINE)), [](void* ptr)
        static std::unique_ptr<LogInfo, /*decltype([](void* ptr)*/ std::function<void(void*)>> shldata(shlptr, [](void* ptr)
        {
            if (static_cast<LogInfo*>(ptr) != shlptr) exc_throw("shlptr mismatch @dtor");
            LogInfo* svptr = shlptr;
            shlptr = 0; //prevent further usage while destruction
//        LogInfo* shlptr = static_cast<LogInfo*>(shldata.get());
//        printf("destroy shldata: #att %d, dtor? %d\n", shmnattch(shlptr), shmnattch(shlptr) == 1);
            if (shmnattch(svptr) == 1) svptr->~LogInfo(); //call dtor but don't dealloc memory
            shmfree(svptr);
        }); // ) ShmData(env, SRCLINE)); //(GpuPortData*)malloc(sizeof(*addon_data));
//    static LogInfo* shlptr = shldata.get();
//    static bool isnew = (shmnattch(shlptr) == 1);
//    printf("global logprintf: shmptr %p, #attach %d, valid? %d, isnew? %d\n", shlptr, shmnattch(shlptr), shlptr->isvalid(), isnew);
        static LogInfo* newptr = static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY, SRCLINE));
        static bool isnew = (shmnattch(shlptr) == 1);
        bool was_new = isnew;
        if (isnew) { new (shlptr) LogInfo; isnew = false; } //placement "new" to call ctor; CAUTION: first time only
//    {
//        new (shlptr) LogInfo; //placement "new" to call ctor; CAUTION: first time only
//        atexit([shlptr]()
//        {
//            if (shmnattch(shlptr) == 1) delete(shlptr);
//        });
//        isnew = false;
//    }
//    isnew = false;
//    if (LogInfo::isclosing()) printf("LogInfo is closing\n");
//    if (/*LogInfo::isclosing()*/ !shlptr) return 0; //CAUTION: can only call static methods on this
        if (shlptr && /*(shmdata.get() != shmptr) ||*/ !shlptr->isvalid()) exc_throw((was_new? "alloc": "reattch") << " shldata " << shlptr << " failed");
        return shlptr;
    }
#endif
};
#endif


#if 0
//log a message to shm:
//(another process will display msgs later)
/*void*/ int logprintf(int level, SrcLine srcline, const char* fmt, ...) //use ret type to allow conditional/ternary usage
{
//    if (level > MAX_DEBUG_LEVEL) return 0; //caller doesn't want this much detail

//fmt basic msg contents:
    char fmtbuf[1000]; //should be large enough for most (all?) msgs
    va_list args;
    va_start(args, fmt);
    size_t fmtlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args);
    va_end(args);
//show warning if fmtbuf too short:
//    std::ostringstream tooshort;
    if (fmtlen >= sizeof(fmtbuf)) fmtlen = sizeof(fmtbuf) - 20 + snprintf(&fmtbuf[sizeof(fmtbuf) - 20], 20, " >> %s ...", commas(fmtlen));
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
//    while (isspace(*srcline)) ++srcline;
    if (strstr(fmtbuf, skipspaces(srcline))) srcline = ""; //don't repeat if already there
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
//    const char* hdr_color = (level == -1)? RED_MSG: (level == -2)? YELLOW_MSG: PINK_MSG;
    const char* msg_color = (level == -1)? RED_MSG: (level == -2)? YELLOW_MSG: BLUE_MSG;
    thread_local static int /*numerr = 0,*/ nummsg = 0; //show thread info once
//    if (level < 0) //error
//    {
    if (!nummsg++) fprintf(stderr, PINK_MSG "[msec $thr] ======== thread# %d, id 0x%x, pid %d ========" ENDCOLOR_NEWLINE, thrinx(), thrid(), getpid());
//for (char* bp = fmtbuf; bp = strchr(bp, '\n'); *bp++ = '\\');
//for (char* bp = fmtbuf; bp = strchr(bp, '\x1b'); *bp++ = '?');
//printf("first ofs %d, last ofs %d, len %d, cc start %u, end %u, buf \"%s\"\n", first_ofs, last_ofs, fmtlen, cc_startlen, cc_endlen, fmtbuf); fflush(stdout);
    fprintf(fout, "%s%.*s[%s $%d] %.*s%s%s" ENDCOLOR_NEWLINE, msg_color, first_ofs, fmtbuf, my_timestamp(true).c_str(), thrinx(), last_ofs - first_ofs, fmtbuf + first_ofs, srcline, fmtbuf + last_ofs);
    if (level < 0) { fflush(stdout); fflush(fout); } //make sure important msgs are output; might as well incl stdout as well
    if (level == -1) throw std::runtime_error(fmtbuf);
}
#endif

//define singleton instance in shm and global wrapper functions:

#if 0
void shlfree(void* ptr)
{
    LogInfo* shlptr = static_cast<LogInfo*>(ptr);
    printf("destroy shldata: #att %d, dtor? %d\n", shmnattch(shlptr), shmnattch(shlptr) == 1);
    if (shmnattch(shlptr) == 1) shlptr->~LogInfo(); //call dtor but don't dealloc memory
    shmfree(shlptr);
}
#endif


#if 0
//singleton init:
LogInfo* get_shldata()
{
//    static LogInfo* lp = ;
    static const key_t SHMKEY = LogInfo::SHMKEY | sizeof(LogInfo); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
    static LogInfo* shlptr = 0; //kludge: prevent usage during construction
//    static LogInfo* newptr = static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY, SRCLINE));
//    static bool isnew = (shmnattch(shlptr) == 1);
//    static std::unique_ptr<LogInfo, /*decltype([](void* ptr)*/ std::function<void(void*)>> shldata(static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY, SRCLINE)), [](void* ptr)
    static std::unique_ptr<LogInfo, /*decltype([](void* ptr)*/ std::function<void(void*)>> shldata(shlptr, [](void* ptr)
    {
        if (static_cast<LogInfo*>(ptr) != shlptr) exc_throw("shlptr mismatch @dtor");
        LogInfo* svptr = shlptr;
        shlptr = 0; //prevent further usage while destruction
//        LogInfo* shlptr = static_cast<LogInfo*>(shldata.get());
//        printf("destroy shldata: #att %d, dtor? %d\n", shmnattch(shlptr), shmnattch(shlptr) == 1);
        if (shmnattch(svptr) == 1) svptr->~LogInfo(); //call dtor but don't dealloc memory
        shmfree(svptr);
    }); // ) ShmData(env, SRCLINE)); //(GpuPortData*)malloc(sizeof(*addon_data));
//    static LogInfo* shlptr = shldata.get();
//    static bool isnew = (shmnattch(shlptr) == 1);
//    printf("global logprintf: shmptr %p, #attach %d, valid? %d, isnew? %d\n", shlptr, shmnattch(shlptr), shlptr->isvalid(), isnew);
    static LogInfo* newptr = static_cast<LogInfo*>(shmalloc_debug(sizeof(LogInfo), SHMKEY, SRCLINE));
    static bool isnew = (shmnattch(shlptr) == 1);
    bool was_new = isnew;
    if (isnew) { new (shlptr) LogInfo; isnew = false; } //placement "new" to call ctor; CAUTION: first time only
//    {
//        new (shlptr) LogInfo; //placement "new" to call ctor; CAUTION: first time only
//        atexit([shlptr]()
//        {
//            if (shmnattch(shlptr) == 1) delete(shlptr);
//        });
//        isnew = false;
//    }
//    isnew = false;
//    if (LogInfo::isclosing()) printf("LogInfo is closing\n");
//    if (/*LogInfo::isclosing()*/ !shlptr) return 0; //CAUTION: can only call static methods on this
    if (shlptr && /*(shmdata.get() != shmptr) ||*/ !shlptr->isvalid()) exc_throw((was_new? "alloc": "reattch") << " shldata " << shlptr << " failed");
    return shlptr;
}
#endif

//global wrappers to LogInfo methods:
/*inline*/ /*void*/ int logprintf(int level, /*SrcLine srcline,*/ SrcLine srcline, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    return /*get_shldata()->LogInfo::*/ LogInfo::/*singleton*/ THIS()->logprintf(level, srcline, fmt, args);
}
//std::string read_log(/*int maxents = 0*/) { return LogInfo::/*singleton*/ THIS()->read_log(/*maxents*/); }


//#include "thr-helpers.h"
//#define thread_det  DONT_USE_WITH_LogInfo //std::thread //kludge: shm LogInfo !worky with detachhed threads

//inline time_t now() { return LogInfo::now(); } //msec
//inline elapsed_t elapsed() { return /*get_shldata()->LogInfo::*/ LogInfo::singleton()->elapsed(); } //msec
//inline elapsed_t elapsed(elapsed_t reset) { return /*get_shldata()->LogInfo::*/ LogInfo::singleton()->elapsed(reset); }

//inline /*auto*/ std::thread::id thrid() { return LogInfo::thrid(); }
//inline int Thrinx(const std::thread::id/*auto*/& newid = Thrid()) { return /*get_shldata()->LogInfo::*/ LogInfo::singleton()->thrinx(); }


/////////////////////////////////////////////////////////////////////////////////
////
/// Debug logging:
//

//debug messages:
//structured as expr to avoid surrounding { } and possible syntax errors
//#define debug(level, ...)  (((level) <= MAX_DEBUG_LEVEL)? myprintf(level, __VA_ARGS__): noop())
//inline void noop() {}

//void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...); //fwd ref
//void myprintf(const char* fmt, ...)
//{
//    /*return*/ myprintf(0, __VA_ARGS__);
//}

//inline int32_t debug_level(int32_t newlevel)
//{
//    static int current_level = MAX_DEBUG_LEVEL;
//    if (newlevel >= 0) current_level = newlevel;
//    return current_level;
//}
//inline int32_t debug_level() { return debug_level(-1); }
//extern int32_t debug_level = MAX_DEBUG_LEVEL;

#if 0
#define warn exc_soft
#define exc exc_hard
#define exc_soft(...)  logprintf(DetailLevel::WARN_LEVEL, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
#define exc_hard(...)  logprintf(DetailLevel::ERROR_LEVEL, SRCLINE, std::ostringstream() << /*RED_MSG <<*/ __VA_ARGS__)
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
//compiler should filter out anything > MAX_DEBUG_LEVEL, leaving only the second condition for run-time eval
//TODO: use lamba function for lazy param eval (better run-time perf, can leave debug enabled)
#define debug(level, ...)  ((((level) <= MAX_DEBUG_LEVEL) && ((level) <= detail()))? logprintf(level, SRCLINE, std::ostringstream() << __VA_ARGS__): 0) //filter out *max* detail at compile time
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


/*void*/ int logprintf(int level, /*SrcLine srcline,*/ SrcLine srcline, const char* fmt, ...); //fwd ref

//kludge: implicit cast ostringstream -> const char* !worky; overload with perfect fwd for now
template <typename ... ARGS>
//see https://stackoverflow.com/questions/24315434/trouble-with-stdostringstream-as-function-parameter
/*void*/ int logprintf(int level, SrcLine srcline, std::/*ostringstream*/ostream& fmt, ARGS&& ... args) //const std::ostringstream& fmt, ...);
{
    logprintf(level, srcline, static_cast<std::ostringstream&>(fmt).str().c_str(), std::forward<ARGS>(args) ...); //perfect fwding
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
#endif


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


#if 0
std::string my_timestamp(bool undecorated = false)
{
    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    float x = 1.2;
//    int h = 42;
//TODO: add commas
//    if (undecorated) { ss << FMT("%4.3f") << elapsed_msec(); return ss.str(); }
//    ss << FMT("[%4.3f msec") << elapsed_msec();
    if (undecorated) { ss << commas(elapsed_msec(), "%4.3f"); return ss.str(); }
    ss << "[" << commas(elapsed_msec(), "%4.3f");
#ifdef IPC_THREAD
    ss << " " << getpid();
#endif
    ss << "] ";
    return ss.str();
}


//display a message:
//popup + console for dev/debug only
//NOTE: must come before perfect fwding overloads (or else use fwd ref)
//void* errprintf(FILE* dest, const char* reason /*= 0*/, const char* fmt, ...)
//#include "thr-helpers.h" //thrid(), thrinx(); //CAUTION: cyclic #include; must be after #def debug
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
//    while (isspace(*srcline)) ++srcline;
    if (strstr(fmtbuf, skipspaces(srcline))) srcline = ""; //don't repeat if already there
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
//    const char* hdr_color = (level == -1)? RED_MSG: (level == -2)? YELLOW_MSG: PINK_MSG;
    const char* msg_color = (level == -1)? RED_MSG: (level == -2)? YELLOW_MSG: BLUE_MSG;
    thread_local static int /*numerr = 0,*/ nummsg = 0; //show thread info once
//    if (level < 0) //error
//    {
    if (!nummsg++) fprintf(stderr, PINK_MSG "[msec $thr] ======== thread# %d, id 0x%x, pid %d ========" ENDCOLOR_NEWLINE, thrinx(), thrid(), getpid());
//for (char* bp = fmtbuf; bp = strchr(bp, '\n'); *bp++ = '\\');
//for (char* bp = fmtbuf; bp = strchr(bp, '\x1b'); *bp++ = '?');
//printf("first ofs %d, last ofs %d, len %d, cc start %u, end %u, buf \"%s\"\n", first_ofs, last_ofs, fmtlen, cc_startlen, cc_endlen, fmtbuf); fflush(stdout);
    fprintf(fout, "%s%.*s[%s $%d] %.*s%s%s" ENDCOLOR_NEWLINE, msg_color, first_ofs, fmtbuf, my_timestamp(true).c_str(), thrinx(), last_ofs - first_ofs, fmtbuf + first_ofs, srcline, fmtbuf + last_ofs);
    if (level < 0) { fflush(stdout); fflush(fout); } //make sure important msgs are output; might as well incl stdout as well
    if (level == -1) throw std::runtime_error(fmtbuf);
//    }
//    else //debug
//    {
//        if (!nummsg++) printf(PINK_MSG "[msec $thr] ======== thread# %d, id 0x%x, pid %d ========\n" ENDCOLOR_NOLINE, thrinx(), thrid(), getpid());
//        printf(BLUE_MSG "%.*s[%s $%d] %.*s%s%s\n" ENDCOLOR_NOLINE, first_ofs, fmtbuf, timestamp(true).c_str(), thrinx(), last_ofs - first_ofs, fmtbuf + first_ofs, real_srcline, fmtbuf + last_ofs);
//    }
}
#endif


#endif //ndef _LOGGING_H
///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include <iostream>
//#include "msgcolors.h"
//#include "srcline.h"
//#include "debugexc.h"
#include "logging.h"
#include "thr-helpers.h"


void func1(int val = -1, SrcLine srcline = 0)
{
    debug(0, CYAN_MSG "in func(%d)" /*ENDCOLOR*/, val); //without "<<", with value
    debug(0, BLUE_MSG "func " << "val: " << "0x%x" /*ENDCOLOR_*/ << ATLINE(srcline), val); //with "<<", with value
    debug(0, CYAN_MSG "out func"); //ENDCOLOR); //without "<<", without value
    debug(40, RED_MSG "should%s see this one", (MAX_DEBUG_LEVEL >= 40)? "": "n't"); //ENDCOLOR);
}


void debug_test()
{
//    debug(BLUE_MSG "SDL_Lib: init 0x%x" ENDCOLOR_ATLINE(srcline), flags);
    func1();
    func1(1);
    func1(2, SRCLINE);
    warn("test completed");
}


//#include "msgcolors.h"
#include "str-helpers.h"
//#include "srcline.h"

void func2(int a, SrcLine srcline = 0)
{
//    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR "\n";
//    std::cout << PINK_MSG "hello " << a << " from" ENDCOLOR "\n";
//    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline); //<< std::endl;
    std::cout << RED_MSG "hello " << a << " from" << ATLINE(srcline) << ENDCOLOR_NEWLINE; //std::endl; //ENDCOLOR_ATLINE(srcline) << std::endl;
}


template<typename ARG1, typename ARG2>
class X
{
public:
    X(SrcLine srcline = 0) { std::cout << BLUE_MSG "hello from X<" << TEMPL_ARGS << ">" << ATLINE(srcline) << ENDCOLOR_NEWLINE; }
    ~X() {}
};


//int main(int argc, const char* argv[])
void srcline_test() //ARGS& args)
{
    std::cout << BLUE_MSG "start" << SRCLINE << ENDCOLOR_NOLINE "\n";
    func2(1);
    func2(2, SRCLINE);
    X<int, const char*> aa(SRCLINE);
    X<long, long> bb(SRCLINE);
    std::cout << BLUE_MSG "finish" << SRCLINE << ENDCOLOR_NOLINE "\n";
//    return 0;
}


#include <iostream> //std::cout, std::endl
//#include "srcline.h"
//#include "msgcolors.h"

void func3(int a, SrcLine srcline = 0)
{
    std::cout << BLUE_MSG /*<<*/ "hello " << a << " from" /*<<*/ SRCLINE ENDCOLOR_NOLINE "\n";
    std::cout << CYAN_MSG /*<<*/ "hello " << a << " from" /*<<*/ << ATLINE(srcline) << ENDCOLOR_NEWLINE;
}


//int main(int argc, const char* argv[])
void msgcolors_test() //ARGS& args)
{
    std::cout << BLUE_MSG /*<<*/ "start" << SRCLINE << ENDCOLOR_NEWLINE;
    func3(1);
    func3(2, SRCLINE);
    std::cout << BLUE_MSG << "finish" << SRCLINE << ENDCOLOR_NEWLINE;
//    return 0;
}


//#include <iostream> //std::cout, std::flush
//#include "debugexc.h" //debug()
//#include "msgcolors.h" //*_MSG, ENDCOLOR_*
//#include "elapsed.h"

//#ifndef MSG
// #define MSG(msg)  { std::cout << msg << std::flush; }
//#endif

//int main(int argc, const char* argv[])
void elapsed_test() //ARGS& args)
{
    debug(0, BLUE_MSG << "start");
    sleep(2); //give parent head start
    debug(0, GREEN_MSG << "finish 2 sec later");
//    return 0;
}


void bg() ///*BkgSync<>*/ /*auto*/ SYNCTYPE& bs, int want)
//void bg(/*BkgSync<>*/ /*auto*/ XSYNCTYPE& xbs, int want)
{
//    SYNCTYPE& bs = *shmalloc_typed<SYNCTYPE>(xbs, 1, SRCLINE);
    DebugInOut(PINK_MSG "bkg thr#" << Thrinx());
//    std::string status;
    for (int i = 0; i < 3; ++i)
    {
//        debug(0, CYAN_MSG << info() << "BKG wait for %d", want);
//        bs.wait(want, NULL, true, SRCLINE);
//        debug(0, CYAN_MSG << info() << "BKG got %d, now reset to 0", want);
        sleep(2);
//        bs.store(0, SRCLINE);
    }
}


#define THREAD  thread_det
//#define THREAD  std::thread

void thread_test() //ARGS& args)
{
    debug(0, "my thrid " << thrid << ", my inx " << Thrinx());
//HERE(11);
    THREAD fg1(bg);
    sleep(1);
    THREAD fg2(bg);
    sleep(3);
    THREAD fg3(bg);
//    sync_test();
    fg1.join();
    fg2.join();
    fg3.join();
}


//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    DebugInOut("unit test");
    debug_test();
    elapsed_test();
    srcline_test();
    msgcolors_test();
    thread_test();
//    return 0;
}

#endif //def WANT_UNIT_TEST