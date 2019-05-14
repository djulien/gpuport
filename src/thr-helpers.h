/// IPC/thread stuff:

#if !defined(_THREAD_HELPERS_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _THREAD_HELPERS_H //CAUTION: put this before defs to prevent loop on cyclic #includes
//#pragma message("#include " __FILE__)

#include <thread> //std::thread::get_id(), std::thread()
#include <utility> //std::forward<>
#include <sstream> //std::ostringstream
#include <condition_variable>
#include <mutex> //std:mutex<>, std::unique_lock<>
#include <vector>
#include <bitset>

//#include "srcline.h"
//#include "msgcolors.h"
//#include "debugexc.h"
#include "logging.h"


#ifndef STATIC
 #define STATIC //dummy keyword for readability
#endif

#ifndef VOID
 #define VOID //dummy keyword for readability
#endif

#ifndef CONST
 #define CONST //dummy keyword for readability
#endif


#if 0 //moved to logging.h
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

#if 0
inline bool operator==(const std::thread::id& lhs, const std::thread::id& rhs)
{
    return lhs 
}
#endif
//STATIC /*friend*/ std::ostream& operator<<(std::ostream& ostrm, const std::thread::id& thrid)
//{
//    ostrm << "0x" << std::hex << thrid << std::dec << " ";
//    return ostrm;
//}


//detached thread:
//NOTE: not useful if additional methods need to be called (thread's "this" not valid after detach())
class thread_det: public std::thread
{
    using super = std::thread;
public:
    template <typename ... ARGS>
    explicit thread_det(ARGS&& ... args): super(std::forward<ARGS>(args) ...) { detach(); } //perfect fwd to ctor, then detach
public:
    void join() { warn("join() ignored on detached thread"); }
};


//put down here to avoid cyclic #include errors (debugexc uses thrid() and thrinx()):
//#include "srcline.h"
//#include "msgcolors.h"
//#include "debugexc.h"

//sync with bkg thread:
//NOTE: std::mutex, std::condition_variable in shm *cannot* be used across processes (read that Posix ipc not implemented in stl)
template <typename VALTYPE = uint32_t, bool WANT_DEBUG = false>
class BkgSync
{
    static const int SYNC_LEVEL = 55;
#define DEBUG(desc, srcline)  debug(SYNC_LEVEL, GREEN_MSG << desc << /*ENDCOLOR_*/ ATLINE(srcline))
    std::atomic<VALTYPE> m_val; //avoid mutex locks except when waiting; //= 0; //init to !busy
    std::mutex m_mtx;
//    std::atomic<std::condition_variable> m_cv; //avoid mutex locks except when waiting
    std::condition_variable m_cv;
    using LOCKTYPE = std::unique_lock<decltype(m_mtx)>; //not: std::lock_guard<decltype(m_mtx)>;
public: //ctors/dtors
    explicit inline BkgSync(VALTYPE init = 0): m_val(init) {}
public: //operators
    inline operator VALTYPE() { return m_val.load(); }
    inline VALTYPE operator=(VALTYPE newval) { store(newval); return m_val.load(); } //m_val = newval; //m_cv.notify_all();
    inline VALTYPE operator|=(VALTYPE moreval) { fetch_or(moreval); return m_val.load(); } //m_val |= moreval; //m_cv.notify_all();
public: //operators
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const BkgSync& that) CONST
    {
        ostrm << "{" << commas(sizeof(that)) << ": @" << &that;
        if (!&that) { ostrm << " (NO DATA)"; return ostrm; }
        ostrm << ", val " << sizeof(that.m_val) << ": 0x" << std::hex << that.load() << std::dec;
        ostrm << "}";
        return ostrm;
    }
public: //methods
//make interchangeable with std::atomic<>:
//TODO: perfect fwd or derive?
    inline /*VALTYPE*/ auto load() const { return m_val.load(); }
//    inline bool hasval(VALTYPE want_val) const { return load() == want_val; }
    inline void store(VALTYPE newval, SrcLine srcline = 0)
    {
        if (WANT_DEBUG) DEBUG("BkgSync = 0x" << std::hex << newval << std::dec, srcline);
        LOCKTYPE lock(m_mtx); //NOTE: mutex must be held while var is changed even if atomic, according to https://en.cppreference.com/w/cpp/thread/condition_variable
//        want_or? m_val |= newval: m_val = newval;
        VOID m_val.store(newval);
        VOID notify(srcline);
    }
    inline auto fetch_or(VALTYPE bits, SrcLine srcline = 0)
    {
        if (WANT_DEBUG) DEBUG("BkgSync |= 0x" << std::hex << bits << std::dec, srcline);
        LOCKTYPE lock(m_mtx); //NOTE: mutex must be held while var is changed even if atomic, according to https://en.cppreference.com/w/cpp/thread/condition_variable
        VALTYPE oldval = m_val.fetch_or(bits);
        VOID notify(srcline);
        return oldval; //give *old* value to caller
    }
    void notify(SrcLine srcline = 0)
    {
        if (WANT_DEBUG) DEBUG("BkgSync notify all, val " << load(), srcline);
////        all? m_cv.notify_all(): m_cv.notify_one();
//        m_cv.notify_all();
        VOID m_cv.notify_all();
    }
    typedef std::function<bool(void)> CANCEL; //void* (*REFILL)(mySDL_AutoTexture* txtr); //void);
    bool wait(VALTYPE want_value = 0, CANCEL cancel = NULL, bool blocking = true, SrcLine srcline = 0)
    {
        if (WANT_DEBUG) DebugInOut(YELLOW_MSG "BkgSync wait for 0x" << std::hex << want_value << std::dec << " (" << &"non-blocking"[blocking? 4: 0] << "): thr# " << Thrinx() << ", cur val 0x" << std::hex << load() << " or 0x" << m_val << std::dec << ", match? " << (load() == want_value) << ATLINE(srcline));
        if (load() == want_value) return true; //no need to wait, already has desired value
        if (blocking)
        {
            if (WANT_DEBUG) DEBUG("BkgSync lock and wait", srcline);
            LOCKTYPE lock(m_mtx);
            m_cv.wait(lock, [this, want_value, cancel]
            {
                if (cancel && cancel()) return true;
                return load() == want_value; //filter out spurious wakeups
            });
        }
        return blocking;
    }
#undef DEBUG
};

#if 0
//from https://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
class semaphore
{
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned long count_ = 0; // Initialized as locked.
public:
    void notify() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }
    void wait() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) condition_.wait(lock); // Handle spurious wake-ups.
        --count_;
    }
    bool try_wait() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        if(count_) { --count_; return true; }
        return false;
    }
};
#endif

#endif //ndef _THREAD_HELPERS_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

//#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
//#include "sdl-helpers.h"
//#include "msgcolors.h"
//#include "debugexc.h"
//#include "srcline.h"
//#include "sdl-helpers.h" //SDL_Delay()
//#include "elapsed.h" //timestamp()
#include "logging.h"

#include "thr-helpers.h"


#define sec  *1000
#define msec  *1
#define usec  /1000
#define sleep(delay)  usleep((delay) * 1000) //msec => usec


std::string timestamp(bool undecorated = false)
{
    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    float x = 1.2;
//    int h = 42;
//TODO: add commas
    if (undecorated) { ss << FMT("%4.3f") << Elapsed(); return ss.str(); }
    ss << FMT("[%4.3f msec") << Elapsed();
#ifdef IPC_THREAD
    ss << " " << getpid();
#endif
    ss << "] ";
    return ss.str();
}


std::string info()
{
    std::ostringstream ostrm;
    ostrm << timestamp() /*<< std::hex << "0x"*/ << "thr#" << Thrinx() << " (" << thrid /*<< std::dec*/ << ") ";
    return ostrm.str();
}


//#include "shmalloc.h"
using SYNCTYPE = BkgSync<uint32_t, true>;
//using XSYNCTYPE = key_t;

//template <typename SYNCTYPE>
void fg(/*BkgSync<>*/ /*auto*/ SYNCTYPE& bs, int which)
//void fg(/*BkgSync<>*/ /*auto*/ XSYNCTYPE& xbs, int which)
{
//    SYNCTYPE& bs = *shmalloc_typed<SYNCTYPE>(xbs, 1, SRCLINE);
    DebugInOut(PINK_MSG "FG thr#" << Thrinx() << ", bits " << which);
    std::string status;
    for (int i = 0; i < 3; ++i)
    {
        debug(0, CYAN_MSG << info() << "FG " << status << "set %d", which);
        for (int bit = 1; which & ~(bit - 1); bit <<= 1)
            if (which & bit)
            {
                sleep(bit * 0.25 sec); //SDL_Delay(bit * 0.25 sec);
                bs.fetch_or(bit, SRCLINE); // |= bit;
            }
        debug(0, CYAN_MSG << info() << "FG now wait for 0");
        bs.wait(0, NULL, true, SRCLINE);
        status = "got 0, now ";
    }
}

//template <typename SYNCTYPE>
void bg(/*BkgSync<>*/ /*auto*/ SYNCTYPE& bs, int want)
//void bg(/*BkgSync<>*/ /*auto*/ XSYNCTYPE& xbs, int want)
{
//    SYNCTYPE& bs = *shmalloc_typed<SYNCTYPE>(xbs, 1, SRCLINE);
    DebugInOut(PINK_MSG "bkg thr#" << Thrinx());
    std::string status;
    for (int i = 0; i < 3; ++i)
    {
        debug(0, CYAN_MSG << info() << "BKG wait for %d", want);
        bs.wait(want, NULL, true, SRCLINE);
        debug(0, CYAN_MSG << info() << "BKG got %d, now reset to 0", want);
        sleep(0.5 sec); //SDL_Delay(0.5 sec);
        bs.store(0, SRCLINE);
    }
}

void sync_test()
{
#if 0
    /*BkgSync<uint32_t, true>*/ SYNCTYPE bs;
    thread_det wker(bg, std::ref(bs), 7);
//    wker.detach();
    fg(bs, 7); //fg(std::ref(bs), 7);
#elif 1
    /*BkgSync<uint32_t, true>*/ SYNCTYPE bs;
    thread_det fg1(fg, std::ref(bs), 1), fg2(fg, std::ref(bs), 2), fg3(fg, std::ref(bs), 4);
    bg(bs, 7); //bg(std::ref(bs), 7);
#elif 1
    /*BkgSync<uint32_t, true>*/ XSYNCTYPE bs = 0x4444beef; //shm key
    { SYNCTYPE& initbs = *new (shmalloc_typed<SYNCTYPE>(bs, 1, SRCLINE)) SYNCTYPE; } //placement new
    thread_det fg1(fg, std::ref(bs), 1), fg2(fg, std::ref(bs), 2), fg3(fg, std::ref(bs), 4);
    bg(bs, 7); //bg(std::ref(bs), 7);
#else //ipc
//    int counter = 0;
    for (int p = 0; p < 3; ++p)
    {
        pid_t pid = fork();
        if (!pid) //child process
        {
//            for (int i = 0; i < 5; ++i)
//            {
//                printf("child#%d: counter = %d\n", p, ++counter);
//            }
            fg(bs, 1 << p);
            return;
        }
        if (pid > 0) continue; //parent process
        else //fork failed
        {
            printf("fork()[%d] failed! %s (%d)\n", p, strerror(errno), errno);
//        return 1;
        }
    }
//    for (int j = 0; j < 5; ++j)
//    {
//        printf("parent process: counter=%d\n", ++counter);
//    }
    bg(bs, 7);
#endif
}


//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    debug(0, "my thrid " << thrid << ", my inx " << Thrinx());
    sync_test();
}

#endif //def WANT_UNIT_TEST