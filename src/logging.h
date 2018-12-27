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
#include <sstream> //std::stringstream
#include <unistd.h> //getpid()

#include "str-helpers.h" //"ostrfmt.h" //FMT()


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
#include <condition_variable>
#include <mutex> //std:mutex<>, std::unique_lcok<>

#if 0 //replaced
inline auto /*std::thread::id*/ thrid()
{
//TODO: add pid for multi-process uniqueness?
    return std::this_thread::get_id();
}

//reduce verbosity by using a unique small int instead of thread id:
int thrinx(const std::thread::id/*auto*/& myid = thrid())
{
//TODO: move to shm
    static std::vector</*std::decay<decltype(thrid())>*/std::thread::id> ids;
    static std::mutex mtx;
    std::unique_lock<decltype(mtx)> lock(mtx);

    for (auto it = ids.begin(); it != ids.end(); ++it)
        if (*it == myid) return it - ids.begin();
    int newinx = ids.size();
    ids.push_back(myid);
    return newinx;
}
#endif


////////////////////////////////////////////////////////////////////////////////
////
/// debug and error helpers
//

#include <sstream> //std::ostringstream
#include <stdexcept> //std::runtime_error
#include <utility> //std::forward<>
#include <stdio.h> //<cstdio> //vsnprintf()
#include <stdarg.h> //varargs
#include <cctype> //isspace()
#include <string>

//#include "msgcolors.h" //MSG_*, ENDCOLOR_*
#include "str-helpers.h" //NVL(), commas()
//cyclic; moved below; #include "thr-helpers.h" //thrid(), thrinx()
//#include "srcline.h" //SRCLINE
//#include "elapsed.h" //elapsed_msec()


#ifndef MAX_DEBUG_LEVEL
 #define MAX_DEBUG_LEVEL  100 //[0..9] for high-level, [10..19] for main logic, [20..29] for mid logic, [30..39] for lower level alloc/dealloc
#endif


//accept variable #3 macro args:
#ifndef UPTO_2ARGS
 #define UPTO_2ARGS(one, two, three, ...)  three
#endif
//#ifndef UPTO_3ARGS
// #define UPTO_3ARGS(one, two, three, four, ...)  four
//#endif


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


/////////////////////////////////////////////////////////////////////////////////
////
/// Debug detail levels:
//

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


//get/set detail logging level for given file (key):
int detail(const char* key /*= "*"*/, size_t keylen, int new_level /*= NO_CHANGE*/)
{
    static /*PreallocVector<DetailLevel, 10>*/ std::vector<DetailLevel> details; //max 1 per src file + 1 global
    if (new_level == DetailLevel::DUMP)
    {
        debug(32, BLUE_MSG "debug detail %d entries:", details.size());
        for (auto it = details.begin(); it != details.end() /*&& (it->level != EMPTY)*/; ++it)
            debug(32, BLUE_MSG "debug detail[%d/%d]: " << it->key << " = %d", it - details.begin(), details.size(), it->level);
        new_level = DetailLevel::NO_CHANGE;
    }
//    if (!keylen) keylen = strlen(key);
    static const substr ALL("*");
    substr new_key(key, keylen);
    debug(32, "detail level " << new_key << " <- %d", new_level);
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
int detail() { return detail(__FILE__, 0, DetailLevel::NO_CHANGE); }
int detail(int new_level) { return detail(__FILE__, 0, new_level); }
int detail(const char* key) { return detail(key, 0, DetailLevel::NO_CHANGE); }
int detail(const substr& key, const substr& level) //const char* key, size_t keylen, const char* new_level)
{
    debug(24, "set detail: key " << key << ", level " << level);
    int new_level;
    if (!level.len || (level == "+")) new_level = DetailLevel::TRUE;
    else if (level == "-") new_level = DetailLevel::FALSE;
    else new_level = atoi(level.str);
    return detail(key.str, key.len, new_level);
}


void first_time()
{
//    std::string str(NVL(getenv("DEBUG"), ""));
    const char* str = getenv("DEBUG");
//    printf("%.*s %d:'%s', trunc %d:'%.*s'\n", 4, "qwerty", strlen(__FILE__), __FILE__, strrofs(__FILE__, '.'), strrofs(__FILE__, '.'), __FILE__);
    if (!str) { warn("Prefix with \"DEBUG=%.*s\" or \"DEBUG=*\" to see debug info.", strrofs(__FILE__, '.'), __FILE__); return; } //tell user how to get debug info
    static const int           ONOFF = 1, NAME = 2,            LEVEL = 4;
    static const std::regex re("^([+\\-])?([^\\s=]+)(\\s*=\\s*(\\d+))?(\\s*,\\s*)?");
//    static const char* DELIM = ", ";
    for (int count = 0; *str; ++count)
    {
        if (count > 50) { exc_soft("whoops; inf loop?"); break; } //paranoid/debug
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
}


////////////////////////////////////////////////////////////////////////////////
////
/// shared memory logger:
//

#include <cstdlib> //getenv()
#include <regex> //std::regex, std::regex_match()

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
#else
// #define log  if (0) debug
#endif


#define exc_throw(...)  throw std::runtime_error((std::ostringstream() << RED_MSG << __VA_ARGS__ << ENDCOLOR_NOLINE).str())


//debug/diagnostic log msgs and related info:
//don't want anything slowing down gpu_wker (file or console output could cause screen flicker),
// so log messages are stored in shm and another process can read them aynchronously if interested
//circular fifo is used; older msgs will be overwritten automatically; no memory mgmt overhead
#if 1
struct LogInfo
{
    static const key_t SHMKEY = 0xF00D0000 | sizeof(LogInfo); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
    std::mutex mtx;
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
public: //ctors/dtors
//    LogInfo() //: m_epoch(now())
//    {
//        latest = 0; //not really needed (log is circular), but it's nicer to start in a predictable state
//        for (int i = 0; i < SIZEOF(msgs); ++i)
//        {
////                log[i].timestamp = 0;
//            msgs[i][0] = '\0';
//        }
//    }
public: //operators
public: //timer methods
    typedef /*decltype(now())*/ /*int64_t*/ uint32_t time_t; //32 bits is enough for ~ 1200 hours of msec or ~1.2 hr of usec
    typedef /*decltype(elapsed())*/ uint32_t elapsed_t;
    static time_t now() //msec
    {
        using namespace std::chrono;
        return /*std::chrono::*/duration_cast</*std::chrono::*/milliseconds>(/*std::chrono::*/system_clock::now().time_since_epoch()).count();
    }
    std::atomic<time_t> epoch = now();
    inline elapsed_t elapsed() { return now() - m_epoch; } //msec
    inline elapsed_t elapsed(elapsed_t reset) //, int scaled = 1) //Freq = #ticks/second
    {
//    started += delta; //reset to now() each time called
        m_epoch = now() - reset; //set new epoch
//        return /*scaled? (double)delta * scaled / SDL_TickFreq():*/ delta; //return actual time vs. #ticks
        return elapsed(); //msec
    }
//elapsed.pause = function() { elapsed.paused || (elapsed.paused = elapsed.now()); }
//elapsed.resume = function() { if (elapsed.paused) elapsed.epoch += elapsed.now() - elapsed.paused; elapsed.paused = null; }
//protected: //data members
//    time_t m_epoch;
public: //thread methods
    static inline auto /*std::thread::id*/ thrid()
    {
//TODO: add pid for multi-process uniqueness?
        return std::this_thread::get_id();
    }
//reduce verbosity by using a unique small int instead of thread id:
    PreallocVector<thrid, 8> thrids;
    int thrinx(const std::thread::id/*auto*/& newid = thrid())
    {
//        static std::vector</*std::decay<decltype(thrid())>*/std::thread::id> ids;
//        static std::mutex mtx;
        LOCKTYPE lock(mtx);

        for (auto it = thrids.begin(); it != thrids.end(); ++it)
            if (*it == newid) return it - ids.begin();
        int newinx = ids.size();
        thrids.push_back(newid);
        return newinx;
    }
public: //logging methods
    std::atomic<size_t> head = 0;
    struct
    {
        std::atomic<int> num_writes = 0, num_ovfl = 0; //total #log msgs, #fmtbuf ovfls
        std::atomic<int> buf_len = 0, ovfl_len = 0, move_len = 0; //total buf, ovfl, memmove len
        std::atomic<int> color_resets = 0; //#color ends within buf (indicates multi-colored log msgs)
    } stats;
    char pool[0x4000] = ""; //16K will hold ~200 entries of 80 char
//NOTE: wrapper macro already decided whether to keep this message or not
/*void*/ int logprintf(int level, SrcLine srcline, const char* fmt, ...) //use ret type to allow conditional/ternary usage
    {
//    if (level > MAX_DEBUG_LEVEL) return 0; //caller doesn't want this much detail
        const char* msg_color = (level <= DetailLevel::ERROR_LEVEL)? RED_MSG: (level <= DetailLevel::WARN_LEVEL)? YELLOW_MSG: BLUE_MSG;
        size_t msg_colorlen = strlen(msg_color);
//pre-fmt basic msg contents:
        char fmtbuf[1000]; //should be large enough for most msgs
        va_list args;
        va_start(args, fmt);
        size_t fmtlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args); //+ 1;
//        if (fmtlen >= sizeof(fmtbuf)) stats.ovfl_len += fmtlen - sizeof(fmtbuf);
//        stats.buf_len += fmtlen;
//        ++stats.num_writes;
        va_end(args);
//        static const char* ColorEnd = ENDCOLOR_NOLINE;
        static const size_t endlen = strlen(ENDCOLOR_NOLINE); //ANSI_COLOR("0")
        if ((fmtlen >= endlen) && (fmtlen < sizeof(fmtbuf)) && !strcmp(fmtbuf + fmtlen - endlen, ENDCOLOR_NOLINE)) fmtbuf[fmtlen -= endlen] = '\0'; //drop trailing ENDCOLOR; going to re-add it later anyway
//        std::vector<size_t> color_resets;
        for (const char* bp = fmtbuf; bp = strstr(bp, ENDCOLOR_NOLINE); bp += endlen) //infrequent, and hopefully occurrences are toward buf end so memmove is not too expensive
        {
//            color_resets.push(bp - fmtbuf);
            size_t tail_len = std::min(sizeof(fmtbuf), fmtlen) - (bp + endlen - fmtbuf);
            if (msg_colorlen != endlen) memmove(bp + msg_colorlen, bp + endlen, tail_len);
            strncpy(bp, msg_color, msg_colorlen);
            fmtlen += msg_colorlen - endlen;
            stats.move_len += tail_len;
            ++stats.color_resets;
        }
//find first + last color codes so we can insert more text correctly into msg:
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
        size_t leading_colorend = bp? bp + suffix_len - fmtbuf: 0; //!0 == end of leading color code
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
        ++stats.num_writes;
        stats.buf_len += fmtlen;
        if (fmtlen >= sizeof(fmtbuf))
        {
            static const int RESERVE = 20;
            ++stats.num_ovfl;
            stats.ovfl_len += fmtlen - sizeof(fmtbuf);
            fmtlen = sizeof(fmtbuf) - RESERVE + snprintf(&fmtbuf[sizeof(fmtbuf) - RESERVE], RESERVE, " >> %s ...", commas(fmtlen));
        }
//TODO: replace color resets with msg_color
//        if (!last_ofs) { first_ofs = 0; last_ofs = fmtlen; }
//printf("FINAL: first %u, last %u, cc start %u, cc end %u\n", /*match - fmtbuf, match_end - fmtbuf,*/ first_ofs, last_ofs, cc_startlen, cc_endlen);
//    std::ostringstream ss;
//    ss << ENDCOLOR_ATLINE(srcline);
//    if (strstr(fmtbuf, )) srcline = ""; //already there; don't add again
//#define ENDCOLOR  "  &" SRCLINE ENDCOLOR_NOLINE //use const char* if possible; don't call shortsrc()
//    static SrcLine me = SRCLINE;
        size_t src_insofs, srclen;
        if (!srcline) srcline = SRCLINE; //use self if caller unknown
//    while (isspace(*srcline)) ++srcline;
        if (strstr(fmtbuf, skipspaces(srcline))) srclen = 0; //srcline = 0; //""; //don't repeat if already there
        else //try again with just filename part, no line#
        {
            const char* bp = strchr(srcline, ':');
            if (src_insofs = strnstr(fmtbuf, srcline, bp - srcline)) { srcline = bp; src_insofs += bp - srcline; } //same file, different line; just show additional line#
            srclen = strlen(bp);
        }
//send msg to stderr or stdout, depending on severity:
//        static std::mutex mtx;
        thread_local static int /*numerr = 0,*/ count = 0; //show thread info once
//no        LOCKTYPE lock(mtx); //avoid interleaved output between threads
//    static int count = 0;
//    if (!count++) printf(CYAN_MSG "[elapsed-msec $thread] ===============================\n" ENDCOLOR_NOLINE);
        if (!count++) //show thread info first time
        {
            char intro_buf[80];
            size_t intro_len = snprintf(intro_buf, sizeof(intro_buf), PINK_MSG "[msec $thr] ======== thread# %d, id 0x%x, pid %d ========" ENDCOLOR_NEWLINE, thrinx(), thrid(), getpid());
            wrapwrite(intro_buf, intro_len + 1);
        }
//    const char* hdr_color = (level == -1)? RED_MSG: (level == -2)? YELLOW_MSG: PINK_MSG;
//    if (level < 0) //error
//    {
//for (char* bp = fmtbuf; bp = strchr(bp, '\n'); *bp++ = '\\');
//for (char* bp = fmtbuf; bp = strchr(bp, '\x1b'); *bp++ = '?');
//printf("first ofs %d, last ofs %d, len %d, cc start %u, end %u, buf \"%s\"\n", first_ofs, last_ofs, fmtlen, cc_startlen, cc_endlen, fmtbuf); fflush(stdout);
        char timestamp[20];
        size_t timest_len = snprintf(timestamp, sizeof(timestamp), "[%s $%d] ", commas((double)elapsed() / 1000), thrinx());
//    if (undecorated) { ss << FMT("%4.3f") << elapsed_msec(); return ss.str(); }
//        fprintf(fout, "%s%.*s[%s $%d] %.*s%s%s" ENDCOLOR_NEWLINE, msg_color, first_ofs, fmtbuf, my_timestamp(true).c_str(), thrinx(), last_ofs - first_ofs, fmtbuf + first_ofs, srcline, fmtbuf + last_ofs);
//        substr full_msg[]
        size_t buflen = (!leading_colored? msg_colorlen: 0) + timest_len + fmtlen + /*color.resets.length() * (msg_colorlen - endlen) + (srcline? strlen(srcline): 0)*/ srclen + endlen;
//alloc space for msg in log, then write it piece by piece:
        size_t wrofs = alloc(buflen + 1), svofs = wrofs;
        if (!leading_colored) wrofs += wrapwrite(msg_color, msg_colorlen, wrofs);
        else wrofs += wrapwrite(fmtbuf, leading_colorend, wrofs);
        wrofs += wrapwrite(timestamp, timest_len, wrofs);
//        if (srclen)
//        {
        wrofs += wrapwrite(fmtbuf + leading_colorend, (srclen? src_insofs: fmtlen) - leading_colorend, wrofs);
        wrofs += wrapwrite(srcline, srclen, wrofs);
//        }
        if (srclen) wrofs += wrapwrite(fmtbuf + src_insofs, fmtlen - src_insofs, wrofs);
        wrofs += wrapwrite(ENDCOLOR_NOLINE, endlen, wrofs);
        if (wrofs != svofs + buflen) exc_throw(/*throw std::runtime_error((std::ostringstream() << RED_MSG <<*/ "wrapwrite wrong len: got " << wrofs << ", expected " << svofs + buflen); // << ENDCOLOR_NEWLINE).str());
//        FILE* fout = (level < 0)? stderr: stdout;
//        if (level < 0) { fflush(stdout); fflush(fout); } //make sure important msgs are output; might as well incl stdout as well
//        if (level == -1) throw std::runtime_error(fmtbuf);
        if (level <= ERROR_LEVEL) exc_throw(/*throw std::runtime_error((std::ostringstream() << RED_MSG <<*/ fmtbuf); //<< ENDCOLOR_NEWLINE).str());
        else if (level <= WARN_LEVEL) { fprintf(stderr, YELLOW_MSG << fmtbuf << ENDCOLOR_NEWLINE); fflush(stderr); } //make sure important msgs are output
    }
    size_t alloc(size_t buflen)
    {
        size_t retval = head.fetch_add(buflen) /*% sizeof(pool)*/; //prev atomic += didn't adjust for wrap-around, so do it here
//wrapwrite will wrap its private copy of write ofs to pool size, but shm copy is not updated
//eventual shm copy overflow will cause incorrect wrap (misalignment) if !power of 2, so update shm copy of write ofs when needed:
        if (retval + buflen >= sizeof(pool)) { head.fetch_add(-sizeof(pool)); retval -= sizeof(pool); }
//        while (!head.compare_exchange_weak(retval, retval % sizeof(pool)); //CAS loop; https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/
//NOTE: race condition exists while buf is being filled in; mitigate by pre-filling with nulls (race window is much shorter, but still there):
//        retval %= sizeof(pool);
        size_t wraplen = std::min(buflen, sizeof(pool) - retval);
//        if (!buf) return ofs; //caller will write
        if (wraplen) memset(pool + retval, 0, wraplen);
        if (wraplen < buflen) memset(pool, 0, buflen - wraplen);
        return retval;
    }
    size_t wrapwrite(const char* buf, size_t buflen) { return wrapwrite(buf, buflen, alloc(buflen)); }
    size_t wrapwrite(const char* buf, size_t buflen, size_t ofs)
    {
        ofs %= sizeof(pool);
        size_t wraplen = std::min(buflen, sizeof(pool) - ofs);
//        if (!buf) return ofs; //caller will write
        if (wraplen) memcpy(pool + ofs, buf, wraplen); //std::min(intro_len, sizeof(pool) - ofs));
//            int wrap_len = ofs + intro_len - sizeof(pool);
        if (wraplen < buflen) memcpy(pool, buf + wraplen, buflen - wraplen);
        return buflen;
    }
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


// /*void*/ int myprintf(int level, /*SrcLine srcline,*/ SrcLine srcline, const char* fmt, ...); //fwd ref

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
    exc_soft("test completed");
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
    std::cout << BLUE_MSG "start" ENDCOLOR_NOLINE "\n";
    func2(1);
    func2(2, SRCLINE);
    X<int, const char*> aa(SRCLINE);
    X<long, long> bb(SRCLINE);
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
    std::cout << BLUE_MSG /*<<*/ "start" /*<<*/ ENDCOLOR_NEWLINE;
    func3(1);
    func3(2, SRCLINE);
    std::cout << BLUE_MSG << "finish" << ENDCOLOR_NEWLINE;
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
    debug(0, GREEN_MSG << "finish");
//    return 0;
}


void bg() ///*BkgSync<>*/ /*auto*/ SYNCTYPE& bs, int want)
//void bg(/*BkgSync<>*/ /*auto*/ XSYNCTYPE& xbs, int want)
{
//    SYNCTYPE& bs = *shmalloc_typed<SYNCTYPE>(xbs, 1, SRCLINE);
    DebugInOut(PINK_MSG "bkg thr#" << thrinx());
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


#include "thr-helpers.h"

void thread_test() //ARGS& args)
{
    debug(0, "my thrid " << thrid() << ", my inx " << thrinx());
    thread_det fg1(bg);
    sleep(1);
    thread_det fg2(bg);
    sleep(3);
    thread_det fg3(bg);
//    sync_test();
}


//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    first();
    return;
    DebugInOut("unit test");
    debug_test();
    elapsed_test();
    srcline_test();
    msgcolors_test();
    thread_test();
//    return 0;
}

#endif //def WANT_UNIT_TEST