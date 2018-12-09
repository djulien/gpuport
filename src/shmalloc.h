//shared memory allocator
//see https://www.ibm.com/developerworks/aix/tutorials/au-memorymanager/index.html
//see also http://anki3d.org/cpp-allocators-for-games/

//CAUTION: “static initialization order fiasco”
//https://isocpp.org/wiki/faq/ctors

//to look at shm:
// ipcs -m 
//to delete shm:
// ipcrm -M <key>

//shm notes:
// https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
// https://stackoverflow.com/questions/4836863/shared-memory-or-mmap-linux-c-c-ipc
//    you identify your shared memory segment with some kind of symbolic name, something like "/myRegion"
//    with shm_open you open a file descriptor on that region
//    with ftruncate you enlarge the segment to the size you need
//    with mmap you map it into your address space
//NOTE: Posix smh prefered over SysV:
//https://stackoverflow.com/questions/4175379/what-is-the-point-of-having-a-key-t-if-what-will-be-the-key-to-access-shared-mem
// http://man7.org/linux/man-pages/man7/shm_overview.7.html

//memory notes:
// https://www.itu.dk/~sestoft/bachelor/IEEE754_article.pdf
// http://dewaele.org/~robbe/thesis/writing/references/what-every-programmer-should-know-about-memory.2007.pdf
// https://softwareengineering.stackexchange.com/questions/328775/how-important-is-memory-alignment-does-it-still-matter
// alignof stuf

//#ifdef //prevent loop on cyclic #includes
//#elif !defined(_GENERIC_MEMORY_H) //#ifndef _GENERIC_MEMORY_H
//#define _GENERIC_MEMORY_H //prevent loop on cyclic #includes
#if !defined(_SHMALLOC_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _SHMALLOC_H //CAUTION: put this before defs to prevent loop on cyclic #includes
//#pragma message("#include " __FILE__)

#include <cstdlib> //atexit()
#include <sys/shm.h> //shmctl(), shmget(), shmat(), shmdt(), shmid_ds
//TODO: convert to Posix shm api:
//#include <sys/mman.h> //shm_*()
//#include <sys/stat.h> //mode consts
//#include <fcntl.h> //O_* consts
#include <stdexcept> //std::runtime_error
#include <cstring> //strerror()
#include <stdint.h> //uint*_t
#include <unistd.h> //sysconf()
#include <errno.h>
//#include <vector>
//#include <mutex>
//#include <type_traits> //std::remove_const<>

#include "msgcolors.h" //*_MSG, ENDCOLOR_*
#include "elapsed.h" //timestamp()
#include "srcline.h" //SrcLine, SRCLINE
#include "str-helpers.h" //NVL(), plural(), commas(), NNNN_hex()
#include "ostrfmt.h" //FMT()
#include "debugexc.h" //debug(), TEMPL_ARGS; //CAUTION: cyclic #include; put this one last


#ifndef STATIC
 #define STATIC
#endif


#ifndef rndup
 #define rndup(num, den)  (((num) + (den) - 1) / (den) * (den))
#endif


//varargs to accept up to 2 macro args:
#ifndef UPTO_2ARGS
 #define UPTO_2ARGS(one, two, three, ...)  three
#endif


const int SHM_LEVEL = 75; //detailed low-level debug level


#define CACHE_LEN  64 //CAUTION: use larger of RPi and RPi 2 size to ensure fewer conflicts across processors; //static size_t cache_len = sysconf(_SC_PAGESIZE);
#define cache_pad_1ARG(raw_len)  cache_pad_2ARGS(raw_len, CACHE_LEN) //rndup(raw_len, CACHE_LEN)
//#define cache_pad_2ARGS(raw_len, use_hdr)  (rndup((raw_len) + sizeof(ShmHdr), CACHE_LEN) - sizeof(ShmHdr))
#define cache_pad_2ARGS(raw_len, page_size)  rndup(raw_len, page_size) //(rndup((raw_len) + sizeof(ShmHdr), CACHE_LEN) - sizeof(ShmHdr))
#define cache_pad(...)  UPTO_2ARGS(__VA_ARGS__, cache_pad_2ARGS, cache_pad_1ARG) (__VA_ARGS__)


#define err_ret_1ARG(ret_val)  return ret_val
//#define SDL_exc_2ARGS(what_failed, want_throw)  ((want_throw)? SDL_errmsg(exc, what_failed, 0): SDL_errmsg(debug, what_failed, 0))
#define err_ret_2ARGS(ret_val, ret_errno)  { errno = ret_errno; return ret_val; }
#define err_ret(...)  UPTO_2ARGS(__VA_ARGS__, err_ret_2ARGS, err_ret_1ARG) (__VA_ARGS__)


//#define WANT_SHMHDR //obsolete; use O/S now
//#ifdef WANT_SHMHDR
// #define IFHDR_1ARG(stmt)  stmt
// #define IFHDR_2ARGS(yes_stmt, no_stmt)  yes_stmt


//keep a little extra info in shm:
//uses 24 bytes on Intel, 16 bytes on ARM (Rpi)
//#define SHM_MAGIC  0xfeedbeef //marker to detect valid shmem block
//#define SHM_LOCAL  -33 //kludge: use local alloc instead of shm
struct ShmHdr //16 or 24 bytes (depending on processor architecture)
{
//    int id; key_t key;
//    template<bool SHARED_inner = SHARED>
//    typename std::enable_if<SHARED, int>::type id;
//    template<bool SHARED_inner = SHARED>
//    typename std::enable_if<SHARED, key_t>::type key;
    int id;
    key_t key;
    size_t size;
    size_t numents; //allows dtor loop in type-safe shmfree
    void* usrptr; //allows hdr lookup from heap
    uint32_t marker;
    static const int SHM_LOCAL = 0xfacade; //-33; //kludge: use local alloc instead of shm
    static const int SHM_VALID = 0xfeedbeef; //marker to detect valid shmem block
//methods:
    inline bool isvalid() const { return (this && ((marker == SHM_VALID) || (marker == SHM_VALID ^ 1))); }
}; //ShmHdr;

#if 0
union ShmHdr_padded
{
    ShmHdr hdr;
    char pad[cache_pad(sizeof(ShmHdr))]; //pad to reduce memory cache contention
}; //ShmHdr_padded;
#endif
template <typename TYPE>
struct WithShmHdr
{
    ShmHdr hdr;
    TYPE data;
};

#define SHMHDR_IN_HEAP
#ifdef SHMHDR_IN_HEAP
 #define IFHEAPHDR_1ARG(stmt)  stmt
 #define IFHEAPHDR_2ARGS(yes_stmt, no_stmt)  yes_stmt
 #include <vector>
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
 vector_cxx17<ShmHdr> hdrs; //store hdrs in heap; owner is only proc that needs info anyway; linear search ok for small counts
#else
 #define IFHEAPHDR_1ARG(stmt)  //noop
 #define IFHEAPHDR_2ARGS(yes_stmt, no_stmt)  no_stmt
#endif
#define IFHEAPHDR(...)  UPTO_2ARGS(__VA_ARGS__, IFHEAPHDR_2ARGS, IFHEAPHDR_1ARG) (__VA_ARGS__)


//check if mem ptr is valid:
//template <bool IPC>
//static MemHdr<IPC>* memptr(void* addr, const char* func)
//#include <sstream> //std::ostringstream; for debug only
const ShmHdr* get_shmhdr(const void* addr, SrcLine srcline = 0)
{
//    MemHdr<IPC>* ptr = static_cast<MemHdr<IPC>*>(addr);
//printf("here5 %p\n", addr); fflush(stdout);
#ifdef SHMHDR_IN_HEAP
//    std::ostringstream ss;
//    debug(SHM_LEVEL, BLUE_MSG "get shm hdr: looking for addr %p" ENDCOLOR, addr);
//    for (auto it = hdrs.begin(); it != hdrs.end(); ++it)
//    for (auto& it: hdrs)
//        debug(SHM_LEVEL, BLUE_MSG "hdr[%d]: addr %p, size %u, match? %d" ENDCOLOR, &it - &hdrs[0], it.usrptr, it.size, it.usrptr == addr);
    for (auto& it: hdrs)
//    for (auto it = hdrs.begin(); it != hdrs.end(); ++it)
        if (it.usrptr == addr) return &it;
#else
    const ShmHdr* ptr = static_cast<const ShmHdr*>(addr);
//    if (ptr-- && !((ptr->marker ^ SHM_MAGIC) & ~1)) return ptr;
    if (ptr-- && ptr->isvalid() && (ptr->usrptr == addr)) return ptr;
#endif
//printf("here6\n"); fflush(stdout);
//    char buf[64];
//    snprintf(buf, sizeof(buf), "%s: bad shmem pointer %p", func, addr);
//    throw std::runtime_error(buf);
//    exc(FMT("bad shm ptr %p") << addr, srcline);
    exc("bad shm ptr " << addr, srcline);
}
//#else //ifdef WANT_SHMHDR
// #define IFHDR_1ARG(stmt)  //noop
// #define IFHDR_2ARGS(yes_stmt, no_stmt)  no_stmt
// #define ShmHdr  void
//#endif //def WANT_SHMHDR
//#define IFHDR(...)  UPTO_2ARGS(__VA_ARGS__, IFHDR_2ARGS, IFHDR_1ARG) (__VA_ARGS__)


//allocate shm:
//TODO: use ftok?
//NOTE: same shm seg can be attached at multiple addresses within same proc
void* shmalloc(size_t size, key_t key = 0, /*bool* existed = 0,*/ SrcLine srcline = 0)
{
    errno = 0; //clear previous error; this shouldn't be necessary
//printf("here1\n"); fflush(stdout);
//        if (key) throw std::runtime_error("key not applicable to non-shared memory");
//        return static_cast<MemHdr*>(::malloc(size));
    if ((!key && !size) || (size >= 10e6)) err_ret(nullptr, EOVERFLOW); //throw std::runtime_error("shmalloc: bad size"); //throw std::bad_alloc(); //set reasonable limits
//    size += extralen; //IFHEAPHDR(0, sizeof(ShmHdr));
//    bool dummy;
//    if (!existed) existed = &dummy;
    if (!key) key = (rand() << 16) | 0xbeef; //generate new (pseudo-random) key
    ShmHdr* hdrptr;
    const int extralen = IFHEAPHDR(0, sizeof(ShmHdr));
    if (key == ShmHdr::SHM_LOCAL) //kludge: just use local heap; allows back-emulation with malloc/free
    {
        void* memptr = malloc(size + extralen);
        if (!memptr) err_ret(nullptr); //errno probably already set by malloc(); //throw std::runtime_error(std::string(strerror(errno)));
        /*ShmHdr* */ hdrptr = IFHEAPHDR(&hdrs.emplace_back(), static_cast<ShmHdr*>(memptr));
        hdrptr->id = 0;
        hdrptr->key = key;
        hdrptr->size = size; //- IFHEAPHDR(0, sizeof(ShmHdr));
        hdrptr->usrptr = IFHEAPHDR(memptr, hdrptr + 1);
        hdrptr->marker = ShmHdr::SHM_VALID ^ (false? 1: 0);
        debug(SHM_LEVEL, CYAN_MSG << timestamp() << "shmalloc: get LOCAL size " << commas(size + extralen) << " (hdr " << extralen << ") => " << FMT(" addr %p") << hdrptr->usrptr << " (hdr " << hdrptr << ")" << ATLINE(srcline));
    }
    else
    {
        int shmid = shmget(key, size + extralen, 0666); // | IPC_CREAT); //create if !exist; clears to 0 upon creation
        if (shmid == -1) shmid = shmget(key, 1, 0666); //re-check if existed with smaller size
        bool existed = (shmid != -1);
        if (shmid == -1) shmid = shmget(key, size + extralen, 0666 | IPC_CREAT); //create if !exist; clears to 0 upon creation
//printf("here2 %d\n", shmid); fflush(stdout);
//    debug(CYAN_MSG << timestamp() << "shmalloc: cre shmget key " << FMT("0x%lx") << key << ", size " << size << ", existed? " << existed << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR_ATLINE(srcline));
        if (shmid == -1) err_ret(nullptr); //errno probably already set by shmget(); //throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
        struct shmid_ds shminfo;
        if (shmctl(shmid, IPC_STAT, &shminfo) == -1) err_ret(nullptr); //errno probably already set by shmctl(); //throw std::runtime_error(strerror(errno));
//printf("here3 %zu vs. %zu - %zu\n", size, shminfo.shm_segsz, sizeof(ShmHdr)); fflush(stdout);
//    size = shminfo.shm_segsz; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so give caller actual size
        if (size + extralen > shminfo.shm_segsz) err_ret(nullptr, EOVERFLOW); //throw "pre-existing shm smaller than requested"; //TODO: enlarge?
        void* shmptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
        if (!shmptr || (shmptr == (void*)-1)) err_ret(nullptr); //errno probably already set by shmat(); //throw std::runtime_error(std::string(strerror(errno)));
        /*ShmHdr* */ hdrptr = IFHEAPHDR(&hdrs.emplace_back(), static_cast<ShmHdr*>(shmptr));
//printf("here4 %p\n", ptr); fflush(stdout);
        hdrptr->id = shmid;
        hdrptr->key = key;
        hdrptr->size = shminfo.shm_segsz - extralen; //- sizeof(ShmHdr);
        hdrptr->usrptr = IFHEAPHDR(shmptr, hdrptr + 1); //read/write access
        hdrptr->marker = ShmHdr::SHM_VALID ^ (existed? 1: 0);
        debug(SHM_LEVEL, CYAN_MSG << timestamp() << "shmalloc: cre shmget key " << FMT("0x%lx") << key << ", size " << commas(size + extralen) << " (" << commas(shminfo.shm_segsz) << " padded, hdr " << extralen << "), existed? " << existed << ", #att " <<  shminfo.shm_nattch << " => " << FMT("id 0x%lx") << shmid << FMT(", addr %p") << hdrptr->usrptr << ATLINE(srcline));
    }
    hdrptr->numents = 0; //no type info; added by type-safe wrappers
//    err_ret(ptr + 1, 0);
    err_ret(hdrptr->usrptr, 0);
}


key_t shmkey(void* addr, SrcLine srcline = 0)
{
//printf("here10 %p\n", addr); fflush(stdout);

    return get_shmhdr(addr, srcline)->key;
}

size_t shmsize(const void* addr, SrcLine srcline = 0)
{
//printf("here11 %p\n", addr); fflush(stdout);
//    struct shmid_ds shminfo;
//    ShmHdr* ptr = get_shmhdr(addr, srcline);
//    if (shmctl(ptr->id, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
//    return shminfo.shm_segsz - sizeof(*ptr); //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so give caller actual size
    return get_shmhdr(addr, srcline)->size;
}

key_t shmexisted(const void* addr, SrcLine srcline = 0)
{
//printf("here12 %p\n", addr); fflush(stdout);
    return (get_shmhdr(addr, srcline)->marker != ShmHdr::SHM_VALID);
}

int shmnattch(const void* addr, SrcLine srcline = 0)
{
    struct shmid_ds shminfo;
    if (get_shmhdr(addr, srcline)->key == ShmHdr::SHM_LOCAL) return 1; //TODO: count threads?
    return (shmctl(get_shmhdr(addr, srcline)->id, IPC_STAT, &shminfo) != -1)? shminfo.shm_nattch: 0; //always ask O/S because might have changed > create/attach
}

//size_t shments(const void* addr, SrcLine srcline = 0)
//{
//    return get_shmhdr(addr, srcline)->numents;
//}


//void free(const void* ptr) { free((void*)ptr); }

//release shm:
int shmfree(const void* addr, SrcLine srcline = 0) //CAUTION: data members not valid after
{
//    char buf[64];
//    snprintf(buf, sizeof(buf), "%s: bad shmem pointer %p", func, addr);
//    throw std::runtime_error(buf);
//    ShmPtr* shmptr = static_cast<ShmPtr*>(addr);
//    if (!shmptr-- || (shmptr->marker != SHM_MAGIC)) return;
//printf("here7\n"); fflush(stdout);
    errno = 0; //clear previous error; this shouldn't be necessary
    struct shmid_ds shminfo;
    const ShmHdr* hdrptr = get_shmhdr(addr, srcline), svhdr = *hdrptr; //save before dettaching/destroying memory
    if (svhdr.key == ShmHdr::SHM_LOCAL)
    {
        free(IFHEAPHDR(svhdr.usrptr, /*svhdr.usrptr - 1*/ hdrptr)); //(void*)ptr);
        shminfo.shm_nattch = 0;
    }
    else
    {
        if (shmdt(IFHEAPHDR(svhdr.usrptr, hdrptr)) == -1) throw std::runtime_error(strerror(errno));
        if (shmctl(svhdr.id, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
//printf("here8\n"); fflush(stdout);
        if (!shminfo.shm_nattch) //no more procs attached, delete memory
//    {
            if (shmctl(svhdr.id, IPC_RMID, NULL /*ignored*/)) throw std::runtime_error(strerror(errno));
//        debug(CYAN_MSG << timestamp() << "shmfree: freed " << svhdr.id << FMT("0x%lx") << svhdr.key << ", size " << shminfo.shm_segsz << ENDCOLOR_ATLINE(srcline));
//    }
    }
    IFHEAPHDR(hdrs.erase(hdrptr - &hdrs[0] + hdrs.begin()), 0); //static_cast<ShmHdr*>(memptr));
    debug(SHM_LEVEL, CYAN_MSG << timestamp() << "shmfree: ptr " << addr << FMT(", key 0x%lx") << svhdr.key << ", size " << commas(svhdr.size) << ", #attch " << shminfo.shm_nattch << ATLINE(srcline));
    err_ret(shminfo.shm_nattch, 0); //return #procs still using memory
}
#undef err_ret


inline const char* const_strerror(int my_errno) { return strerror(my_errno); } //"const" shim

#if 1
void* shmalloc_debug(size_t size, key_t key = 0, /*bool* existed = 0,*/ SrcLine srcline = 0)
{
    void* retval = shmalloc(size, key, srcline);
    debug(SHM_LEVEL, CYAN_MSG "shmalloc(size %s, key 0x%lx) => key 0x%lx, ptr %p, size %s (%d %s)" << ATLINE(srcline), commas(size), key, retval? shmkey(retval): 0, retval, commas(retval? shmsize(retval): 0), errno, errno? NVL(const_strerror(errno), &"??error??"[0]): "no error");
    return retval;
}
#define shmalloc  shmalloc_debug

int shmfree_debug(const void* addr, SrcLine srcline = 0)
{
    int retval = shmfree(addr, srcline);
    debug(SHM_LEVEL, CYAN_MSG "shmfree(%p) => %d (%d %s)" << ATLINE(srcline), addr, retval, errno, errno? NVL(const_strerror(errno), &"??error??"[0]): "no error");
    return retval;
}
#define shmfree  shmfree_debug
#endif


//type-safe shmalloc/shmfree:
template <typename TYPE> //= uint8_t>
TYPE* shmalloc_typesafe(key_t key = 0, size_t ENTS = 1, SrcLine srcline = 0)
{
    srcline = NVL(srcline, SRCLINE);
    TYPE* ptr = (TYPE*)shmalloc(sizeof(TYPE) * ENTS, key, srcline);
#if 1
    if (shmnattch(ptr, srcline) > 1) return ptr; //already inited and dtors defered; nothing else to do here
    for (int i = 0; i < ENTS; ++i)
        new (&ptr[i]) TYPE(); //placement new; apply ctors so shm is really inited and has valid state (esp mutex)
    ((ShmHdr*)get_shmhdr(ptr, srcline))->numents = ENTS; //remember #times to call dtor
//    if (WANT_MUTEX) new (&m_ptr[ENTS]) std::mutex();
#endif
    return ptr;
}

template <typename TYPE> //= uint8_t>
int shmfree_typesafe(TYPE* ptr, SrcLine srcline = 0)
{
    srcline = NVL(srcline, SRCLINE);
#if 1
    size_t ENTS = get_shmhdr(ptr, srcline)->numents; //get #times to call dtor
    if (shmnattch(ptr, srcline) <= 1)
        for (int i = 0; i < ENTS; ++i) //TODO: should this be reverse order?  atexit is in order of occurrence so it's probably okay here too
//                {
//                    debug(0, "delete @%p" ENDCOLOR, &m_ptr[i]);
//                    delete &m_ptr[i]; //WRONG
//TODO: should take into account m_count
            ptr[i].~TYPE(); //call dtor only; can't free shm; //example at https://stackoverflow.com/questions/14187006/is-calling-destructor-manually-always-a-sign-of-bad-design
#endif
    return shmfree(ptr, srcline); //CAUTION: data members not valid after
}
//#define shmalloc  shmalloc_typesafe
//#define shmfree  shmfree_typesafe


#endif //ndef _SHMALLOC_H


///////////////////////////////////////////////////////////////////////////////
////
/// Generic (shared or private):
//

#if !defined(_GENERIC_MEMORY_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _GENERIC_MEMORY_H //CAUTION: put this before defs to prevent loop on cyclic #includes

//accept variable # up to 3 - 4 macro args:
#ifndef UPTO_3ARGS
 #define UPTO_3ARGS(one, two, three, four, ...)  four
#endif


//kludge: wrapper to avoid trailing static decl at global scope:
#if 1
#define INIT_NONE  //dummy arg for macro
#define STATIC_WRAP_3ARGS(TYPE, VAR, INIT)  \
    /*static*/ inline TYPE& static_##VAR() \
    { \
        static TYPE m_##VAR /*=*/ INIT; \
        return m_##VAR; \
    }; \
    TYPE& VAR = static_##VAR() //kludge-2: create ref to avoid the need for "()"
#define STATIC_WRAP_2ARGS(TYPE, VAR)  STATIC_WRAP_3ARGS(TYPE, VAR, INIT_NONE) //optional third param
#define STATIC_WRAP(...)  UPTO_3ARGS(__VA_ARGS__, STATIC_WRAP_3ARGS, STATIC_WRAP_2ARGS, STATIC_WRAP_1ARG) (__VA_ARGS__)
#else //no worky
template <typename TYPE>
class static_wrap
{
public:
    /*no: explicit*/ inline static_wrap(TYPE&& that) { get() = that; } //copy ctor
//    static_wrap(TYPE&& that)
//    inline static_wrap& operator=(TYPE /*&&*/ that) { return get() = that; } //fluent
//    inline static_wrap& operator=(int that)
    inline TYPE& get() { return *this; }
    inline operator TYPE&() const
    {
        static TYPE m_var; //wrapped in method to avoid trailing static decl at global scope; CAUTION: pseudo-static (shared)
        return m_var;
    }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const static_wrap& that)
    {
        return ostrm << ((static_wrap)that).get(); //static_cast<TYPE>(that.get());
    }
};
#endif

#endif //ndef _GENERIC_MEMORY_H


///////////////////////////////////////////////////////////////////////////////
////
/// Shm typed array with auto clean-up:
//

//from https://www.raspberrypi.org/forums/viewtopic.php?t=114228
//RPi I-Cache line size 32 bytes, D-Cache line size 32 bytes Pi, 64 bytes Pi2
//MMU page size 4K
//see also https://softwareengineering.stackexchange.com/questions/328775/how-important-is-memory-alignment-does-it-still-matter

//NOTE: had to split due to circular #include in "thr-helpers.h"
#if !defined(_SHMARY_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _SHMARY_H //CAUTION: put this before defs to prevent loop on cyclic #includes

#include <vector>
#include <mutex>

#include "msgcolors.h"
#include "srcline.h"


const key_t KEY_NONE = -2;

//share static members across template instances:
class AutoShmary_common_base
{
protected: //methods
    typedef void (*DTOR)(void*);
    typedef std::pair<DTOR, void*> DTOR_INFO; //trying to keep things DRY
    /*STATIC*/ static void cleanup_later(DTOR dtor, void* data = 0) //void* ptr)
    {
        auto& m_mutex = static_m_mutex();
        auto& m_all = static_m_all();
//        static int done = 0;
        std::lock_guard<std::mutex> guard(m_mutex); //only allow one thread to init or get count at a time
//        if (done++) return;
        if (!m_all.size())
        {
//            auto thunk = [](AutoShmary_common_base* that){ that->cleanup(); }; //NOTE: must be captureless (for use as template arg), so wrap it
//            atexit(std::bind<void(*)(void)>(thunk, this)); //kludge: convert to parameterless static function
            atexit(cleanup);
        }
        m_all.push_back(DTOR_INFO(dtor, data)); //cleanup() must be a static member, so make a list and do it all once at the time
    }
private: //helper methods
    /*STATIC*/ static void cleanup()
    {
        auto& m_all = static_m_all();
        SrcLine srcline = 0; //TODO: where to get this?
        debug(SHM_LEVEL, CYAN_MSG "AutoShmary: clean up %d shm ptr%s" << ATLINE(srcline), m_all.size(), plural(m_all.size()));
        for (auto it = m_all.begin(); it != m_all.end(); ++it) it->first(it->second); //shmfree(*it);
    }
private: //data members
#if 0
    static std::vector<DTOR_INFO>& m_static_all() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::vector<DTOR_INFO> m_all; //TODO: should this be thread_local?
        return m_all;
    }
//broken    static std::vector<DTOR_INFO>& m_all = m_shared_all();
    static std::mutex& m_static_mutex() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::mutex m_mutex;
        return m_mutex;
    }
//broken    static std::mutex& m_mutex = m_shared_mutex();
#else //broken
    static STATIC_WRAP(std::vector<DTOR_INFO>, m_all); //TODO: should this be thread_local?
    static STATIC_WRAP(std::mutex, m_mutex);
#endif
};


//shm array auto-cleanup wrapper:
//defers cleanup until process exit in case other threads or processes are using shmbuf
//thread safe
//NOTE: no need for std::unique_ptr<> or std::shared_ptr<> since underlying memory itself is shared
//#pragma message("debug() used here")
//#pragma message("AutoShmary<> defined here")
template <typename TYPE = uint8_t, size_t ENTS = 1, bool AUTO_INIT = true, bool WANT_MUTEX = false> //, typename PTRTYPE=TYPE*>
class AutoShmary: public AutoShmary_common_base //public std::unique_ptr<SDL_Window, std::function<void(SDL_Window*)>>
{
//TODO: use shmalloc_typesafe<>, shmfree_typesafe<>
public:
//    const key_t KEY_NONE = -2;
//TODO: use "new shmalloc()" to call ctor on shm directly
    explicit AutoShmary(SrcLine srcline = 0): AutoShmary(0, srcline) {}
    AutoShmary(key_t key = 0, /*size_t ents = 1,*/ SrcLine srcline = 0): m_ptr((key != KEY_NONE)? (TYPE*)shmalloc(ENTS * sizeof(TYPE) + (WANT_MUTEX? sizeof(std::mutex): 0), key, NVL(srcline, SRCLINE)): 0), /*len(shmsize(m_ptr) / sizeof(TYPE)), key(shmkey(m_ptr)), existed(shmexisted(m_ptr)),*/ m_count(0), m_srcline(NVL(srcline, SRCLINE))
    {
        if (!m_ptr && (key != KEY_NONE)) exc_soft("shmalloc" << m_templargs << "(" << ENTS << FMT(", 0x%lx") << key << ") failed");
        if (!m_ptr) return;
        if (shmnattch(m_ptr) > 1) return; //already inited and dtors defered; nothing else to do here
//NOTE: can't get std::bind to attach #ents to func ptr, so embed #ents into template instead (ENTS)
        if (AUTO_INIT) //call ctors
        {
            for (int i = 0; i < ENTS; ++i)
//            {
//                debug(0, "placement new @%p" ENDCOLOR, &m_ptr[i]);
//TODO: should take into account m_count
                new (&m_ptr[i]) TYPE(); //placement new; apply ctors so shm is really inited and has valid state (esp mutex)
//            }
            if (WANT_MUTEX) new (&m_ptr[ENTS]) std::mutex();
        }
        AutoShmary_common_base::DTOR dtor = /*AUTO_INIT? std::bind(*/ [](/*size_t ents,*/ void* ptr)
        {
            TYPE* m_ptr = static_cast<TYPE*>(ptr);
            std::mutex* m_mutex = reinterpret_cast<std::mutex*>(&m_ptr[ENTS]);
            if (AUTO_INIT) //call dtors for each array item
            {
                for (int i = 0; i < ENTS; ++i) //TODO: should this be reverse order?  atexit is in order of occurrence so it's probably okay here too
//                {
//                    debug(0, "delete @%p" ENDCOLOR, &m_ptr[i]);
//                    delete &m_ptr[i]; //WRONG
//TODO: should take into account m_count
                    m_ptr[i].~TYPE(); //call dtor only; can't free shm; //example at https://stackoverflow.com/questions/14187006/is-calling-destructor-manually-always-a-sign-of-bad-design
//                }
                using std_mutex = std::mutex; //kludge: compiler seems to mis-parse explicit call to dtor with "::" so use alias
                if (WANT_MUTEX) /*mutex()*/ m_mutex->~std_mutex(); //kludge: "this" not captured, so cast from m_ptr
            }
            shmfree(ptr);
//            }, ents, std::placeholders::_1):
//            [](void* ptr) { shmfree(ptr); };
        };
//        std::lock_guard<std::mutex> guard(mutex()); //only allow one thread to init at a time
//        debug(GREEN_MSG "AutoShmary(%p) ctor: ptr %p, key 0x%x => 0x%x, size %zu * %zu => %zu, existed? %d, page size %ld" ENDCOLOR_ATLINE(srcline), this, m_ptr, key, shmkey(m_ptr), ents, sizeof(TYPE), shmsize(m_ptr), shmexisted(m_ptr), sysconf(_SC_PAGESIZE));
//        debug(GREEN_MSG "ctor " << *this << ENDCOLOR_ATLINE(srcline));
        cleanup_later(dtor, m_ptr);
    }
//    virtual ~AutoShmary() {} //if (m_ptr) debug(RED_MSG "dtor " << *this << ENDCOLOR_ATLINE(m_srcline)); } //"AutoShmary(%p) dtor, ptr %p defer dealloc" ENDCOLOR_ATLINE(m_srcline), this, m_ptr); }
public: //operators
    operator TYPE*() { return m_ptr; }
    std::mutex& mutex() { return *(std::mutex*)(WANT_MUTEX? &m_ptr[ENTS]: NULL); } //NOTE: usage of retval will give memory error if !WANT_MUTEX
//    operator void*() { return m_ptr; }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const AutoShmary& that) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
//        ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
        ostrm << "AutoShmary" << that.m_templargs;
        ostrm << "{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
        ostrm << FMT(", key 0x%lx") << that.key();
        ostrm << ", ptr " << that.m_ptr;
        ostrm << ", len " << that.len();
        ostrm << ", hdr " << (that.m_ptr? get_shmhdr(that.m_ptr): 0);
        ostrm << ", len " << (that.m_ptr? sizeof(*get_shmhdr(that.m_ptr)): 0);
        ostrm << ", existed? " << that.existed();
        ostrm << ", #attch " << (that.m_ptr? shmnattch(that.m_ptr): 0);
        ostrm << "}";
        return ostrm;
    }
//public: //data members (read-only)
//no worky: need to call shmalloc before setting these
//    const size_t len;
//    const key_t key;
//    const bool existed;
public: //methods
//    PTRTYPE& ptr() /*const*/ { return m_ptr; }
    TYPE* ptr() const { return m_ptr; }
    size_t len() const { return m_ptr? shmsize(m_ptr) / sizeof(TYPE): 0; } //NOTE: round down; >= ENTS (shm seg might have given more than requested)
    key_t key() const { return m_ptr? shmkey(m_ptr): 0; }
    bool existed() const { return m_ptr? shmexisted(m_ptr): false; }
public: //std::vector methods
    TYPE* begin() { return m_ptr; }
    TYPE* end() { return &m_ptr[m_count]; }
    size_t size() const { return m_count; }
    void push_back(const TYPE& new_item)
    {
//TODO: reserve(), capacity(), grow(), etc
        if (m_count >= ENTS) exc_hard("shm::vector no room");
        m_ptr[m_count++] = new_item; //TODO: emplace?
    }
private: //data members
    int m_count;
    TYPE* /*const*/ m_ptr; //doesn't change after ctor
    SrcLine m_srcline; //save for parameter-less methods (dtor, etc)
#if 0
    static std::string& my_templargs() //kludge: use wrapper to avoid trailing static decl at global scope
    {
        static std::string m_templ_args(TEMPL_ARGS); //only used for debug msgs
        return m_templ_args;
    }
#else
    static STATIC_WRAP(std::string, m_templargs, = TEMPL_ARGS); //only used for debug msgs
#endif
};


//fixed len vector for use in shm:
template <typename TYPE, size_t SIZE>
class FixedShmVector
{
public:
    static const key_t KEY = 0xbeef0000 | NNNN_hex(SIZE); //0; //show size in key; avoids recompile/rerun size conflicts and makes debug easier (ipcs -m)
    struct shdata
    {
        int count = 0;
        TYPE list[SIZE];
        std::mutex mutex;
        inline auto begin() /*const*/ { return &list[0]; }
        inline auto end() /*const*/ { return &list[count]; }
        inline size_t size() /*const*/ { return count; }
        void push_back(/*const*/ TYPE& new_item)
        {
//TODO: reserve(), capacity(), grow(), etc
            if (count >= SIZEOF(list)) exc_hard("FixedSmhVector<" << SIZE << "> no room");
            list[count++] = new_item; //TODO: emplace?
        }
    };
#if 0 //broken
    /*static*/ AutoShmary<shdata> wrapper(KEY, SRCLINE); //MYKEY, SRCLINE);
public:
    std::mutex* mutex() { return &wrapper.ptr()->mutex; }
    /*static*/ void info() { debug(33, YELLOW_MSG "shm ids @" << wrapper.ptr() << ", #attch " << shmnattch(wrapper.ptr()) << ENDCOLOR); }
public:
    /*static*/ inline auto /*std::thread::id* */ begin() /*const*/ { return wrapper->ptr().begin(); }
    /*static*/ inline auto /*std::thread::id* */ end() /*const*/ { return wrapper->ptr().end(); }
    /*static*/ inline size_t size() /*const*/ { return wrapper->ptr().count; }
    /*static*/ inline void push_back(const TYPE& new_item) { wrapper->ptr().push_back(new_item); }
#endif
};


#endif //ndef _SHMARY_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit tests:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion, #include non-test part of file

#define MAX_DEBUG_LEVEL  100
#include "debugexc.h" //debug()
#include "msgcolors.h" //*_MSG, ENDCOLOR_*

#include "shmalloc.h" //shmalloc(), shmfree(), AutoShmary<>, static_wrap<>

struct A
{
//members:
    static STATIC_WRAP(int, count, = 0); //static_wrap<int> count = 0; //CAUTION: must be placed < uniq_val
    int my_val;
    int uniq_val;
    static STATIC_WRAP(char*, bufptr);
//ctors/dtors:
    A(int val = 0): uniq_val(count++), my_val(val) {}
//operators:
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const A& that)
    {
        ostrm << "A{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
        ostrm << ", val " << that.my_val;
        ostrm << ", uniq " << that.uniq_val;
        ostrm << ", count " << that.count;
        ostrm << "}";
        return ostrm; 
    }
};

//exact copy of A:
//(except "struct" vs. "class")
//checks whether count will be shared between A & B
class B1
{
public:
//members:
    static STATIC_WRAP(int, count, = 0); //static_wrap<int> count = 0; //NOTE: shared between B1 & B2, separate from A; //CAUTION: must be placed < uniq_val
    int my_val;
    int uniq_val;
    static STATIC_WRAP(char*, bufptr);
//ctors/dtors:
    B1(int val = 0): uniq_val(count++), my_val(val) {}
//operators:
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const B1& that)
    {
        ostrm << "B1{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
        ostrm << ", val " << that.my_val;
        ostrm << ", uniq " << that.uniq_val;
        ostrm << ", count " << that.count;
        ostrm << "}";
        return ostrm; 
    }
};

class B2: public B1
{
public:
    int other_member;
//ctors/dtors:
    B2(int val = 0): B1(val), other_member(0) {}
//operators:
//    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const B2& that)
};


struct TestClass
{
    int val = 0;
    TestClass() { INSPECT(GREEN_MSG "ctor " << *this); }
    ~TestClass() { INSPECT(RED_MSG "dtor " << *this); }
    STATIC friend std::ostream& operator<<(std::ostream& ostrm, const TestClass& that)
    {
        ostrm << "TestClass{" << commas(sizeof(that)) << ":" << &that;
        if (!&that) return ostrm << " NO DATA}";
        ostrm << ", val " << that.val;
        ostrm << "}";
        return ostrm; 
    }
};

void unit_test(ARGS& args)
{
#if 0
    void* ptr = shmalloc(100, 0, SRCLINE);
    debug(BLUE_MSG << FMT("%p") << ptr << ", key " << shmkey(ptr) << ", size " << shmsize(ptr) << ", existed? " << shmexisted(ptr) << ENDCOLOR);
    shmfree(ptr, SRCLINE);

    key_t key = 0x1234;
    void* ptr2 = shmalloc(100, key, SRCLINE);
    void* ptr3 = shmalloc(100, key, SRCLINE);
    debug(BLUE_MSG << FMT("%p") << ptr2 << ", key " << shmkey(ptr2) << ", size " << shmsize(ptr2) << ", existed? " << shmexisted(ptr) << ENDCOLOR);
    shmfree(ptr2, SRCLINE);
    shmfree(ptr3, SRCLINE);
#endif

    const key_t KEY = ShmHdr::SHM_LOCAL;
    uint32_t* ptr = shmalloc_typesafe<uint32_t>(KEY, 4, SRCLINE);
    ptr[0] = 0x12345678;
    INSPECT("ptr " << ptr << ", *ptr " << std::hex << ptr[0] << std::dec);
    shmfree(ptr);

    AutoShmary<TestClass, 5> ary1(KEY, SRCLINE);
    INSPECT(ary1 << ", &end " << &ary1[5]);

    { //create nested scope to force dtor
        AutoShmary<TestClass, 0> ary2(ary1.key(), SRCLINE);
        INSPECT(ary2);
    }

    key_t key;
    { //create nested scope to force dtor
        AutoShmary<TestClass, 6> ary3(KEY, SRCLINE);
        INSPECT(ary3 << ", &end " << &ary3[6]);
        key = ary3.key();
    }
    AutoShmary<TestClass, 2> ary4(key, SRCLINE); //NOTE: even though ary2 went out of scope, it should still be alive
    INSPECT(ary4);

    AutoShmary<TestClass, 40> ary5(key, SRCLINE); //NOTE: this one should fail because original memory is smaller than requested
    INSPECT(ary5);

    A a1, a2(22);
    B1 b3, b4(44);
    B2 b5(55);
    A a6(66);
    B1 b7(77);
    B2 b8(88);
//    debug(0, BLUE_MSG "a1 " << a1 << ", \na2 " << a2 << ", \nb3 " << b3 << ", \nb4 " << b4 << ", \nb5 " << b5 << ", \na6 " << a6 << ", \nb7 " << b7 << ", \nb8 " << b8 << ENDCOLOR);
    INSPECT("a1 " << a1);
    INSPECT("a2 " << a2);
    INSPECT("b3 " << b3);
    INSPECT("b4 " << b4);
    INSPECT("b5 " << b5);
    INSPECT("a6 " << a6);
    INSPECT("b7 " << b7);
    INSPECT("b8 " << b8);
}

#endif //def WANT_UNIT_TEST


///////////////////////////////////////////////////////////////////////////////////
#if 0
#ifndef _SHMALLOC_H
#define _SHMALLOC_H

//#include <iostream> //std::cout, std::flush
#include <sys/ipc.h>
#include <sys/shm.h> //shmctl(), shmget(), shmat(), shmdt()
#include <errno.h>
#include <stdexcept> //std::runtime_error()
#include <mutex> //std::mutex, lock
#include <atomic> //std::atomic
#include <type_traits> //std::conditional<>, std::enable_if<>
#include <memory> //std::shared_ptr<>

#include "msgcolors.h" //SrcLine, msg colors
#include "ostrfmt.h" //FMT()
#ifdef SHMALLOC_DEBUG //CAUTION: recursive
 #include "atomic.h" //ATOMIC_MSG()
 #include "elapsed.h" //timestamp()
 #define DEBUG_MSG  ATOMIC_MSG
// #undef SHMALLOC_DEBUG
// #define SHMALLOC_DEBUG  ATOMIC_MSG
#else
 #define DEBUG_MSG(...)  {} //noop158000
// #define SHMALLOC_DEBUG(msg)  //noop
#endif


template <int THREADS = 0>
class Shm
{

};


///////////////////////////////////////////////////////////////////////////////
////
/// Low-level malloc/free emulator:
//

//#include <memory> //std::deleter
//#include "msgcolors.h"
//#define REDMSG(msg)  RED_MSG msg ENDCOLOR_NOLINE


/*
https://stackoverflow.com/questions/25492589/can-i-use-sfinae-to-selectively-define-a-member-variable-in-a-template-class?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
template<typename T, typename Enable = void>
class base_class;
//SFINAE:
// my favourite type :D
template<typename T>
class base_class<T, std::enable_if_t<std::is_same<T, myFavouriteType>::value>>{
    public:
        int some_variable;
};
// not my favourite type :(
template<typename T>
class base_class<T, std::enable_if_t<!std::is_same<T, myFavouriteType>::value>>{
    public:
        // no variable
};
template<typename T>
class derived_class: public base_class<T>{
    public:
        // do stuff
};
*/

/*
https://stackoverflow.com/questions/6972368/stdenable-if-to-conditionally-compile-a-member-function?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
template<class T>
struct check
{
    template<class Q = T>
    typename std::enable_if<std::is_same<Q, bar>::value, bool>::type test()
    {
        return true;
    }
    template<class Q = T>
    typename std::enable_if<!std::is_same<Q, bar>::value, bool>::type test()
    {
        return false;
    }
};
*/

#include <iostream> //std::cout, std::flush, std::ostream
//#include <sstream>

//stash some info within mem block returned to caller:
#define SHM_MAGIC  0xfeedbeef //marker to detect valid shmem block

//typedef struct { int id; key_t key; size_t size; uint32_t marker; } ShmHdr;
//template <bool, typename = void>
template <bool IPC = false>
struct MemHdr
{
//    int id; key_t key;
//    template<bool SHARED_inner = SHARED>
//    typename std::enable_if<SHARED, int>::type id;
//    template<bool SHARED_inner = SHARED>
//    typename std::enable_if<SHARED, key_t>::type key;
    size_t size;
    uint32_t marker;
public:
    static MemHdr* alloc(size_t& size, key_t key = 0, bool* existed = 0)
    {
        if (existed) *existed = false;
        if (key) throw std::runtime_error("key not applicable to non-shared memory");
        return static_cast<MemHdr*>(::malloc(size));
    }
    void destroy() { ::free(this); } //CAUTION: data members not valid after
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MemHdr& hdr) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "memhdr{ size " << hdr.size << " }";
        return ostrm;
    }
//private:
    MemHdr& desc() { return *this; } //dummy function for readability
};

//shared memory (ipc) specialization:
//remember shmem info also
//template <bool IPC>
//struct MemHdr<IPC, std::enable_if_t<IPC>>
template <>
struct MemHdr<true>: public MemHdr<false>
{
    int id;
    key_t key;
public:
    static MemHdr* alloc(size_t& size, key_t key = 0, bool* existed = 0)
    {
//        if (key) throw std::runtime_error("key not applicable to non-shared memory");
//        return static_cast<MemHdr*>(::malloc(size));
        bool dummy;
        if (!existed) existed = &dummy;
        if (!key) key = (rand() << 16) | 0xbeef; //generate new (pseudo-random) key
        int shmid = shmget(key, size, 0666); // | IPC_CREAT); //create if !exist; clears to 0 upon creation
        *existed = (shmid != -1);
        if (shmid == -1) shmid = shmget(key, size, 0666 | IPC_CREAT); //create if !exist; clears to 0 upon creation
        DEBUG_MSG(CYAN_MSG << timestamp() << "shmalloc: cre shmget key " << FMT("0x%lx") << key << ", size " << size << ", existed? " << *existed << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR_ATLINE(srcline));
        if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
        struct shmid_ds shminfo;
        if (shmctl(shmid, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
        size = shminfo.shm_segsz; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so give caller actual size
        MemHdr* ptr = static_cast<MemHdr*>(shmat(shmid, NULL /*system choses adrs*/, 0)); //read/write access
        if (ptr == (MemHdr*)-1) return 0; //throw std::runtime_error(std::string(strerror(errno)));
        ptr->id = shmid;
        ptr->key = key;
        return ptr;
    }
    void destroy() //CAUTION: data members not valid after
    {
        int svid = id; //copy id to stack
        if (shmdt(this) == -1) throw std::runtime_error(strerror(errno));
        struct shmid_ds shminfo;
        if (shmctl(svid, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
        if (!shminfo.shm_nattch) //no more procs attached, delete myself
            if (shmctl(svid, IPC_RMID, NULL /*ignored*/)) throw std::runtime_error(strerror(errno));
    }
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MemHdr& hdr) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "memhdr{ shared, " << FMT("key 0x%lx") << hdr.key << FMT(", id 0x%lx") << hdr.id << ", size " << hdr.size << " }";
        return ostrm;
    }
};


//check if mem ptr is valid:
template <bool IPC>
static MemHdr<IPC>* memptr(void* addr, const char* func)
{
    MemHdr<IPC>* ptr = static_cast<MemHdr<IPC>*>(addr);
    if (ptr-- && !((ptr->marker ^ SHM_MAGIC) & ~1)) return ptr;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: bad shmem pointer %p", func, addr);
    throw std::runtime_error(buf);
}


//allocate memory:
//uses shared + non-shared specializations
//template<bool SHARED>
//typename std::enable_if<SHARED, void*>::type memalloc(size_t size, key_t key = 0, SrcLine srcline = 0)
template<bool IPC>
//typename std::enable_if<!SHARED, void*>::type memalloc(size_t size, key_t key = 0, SrcLine srcline = 0)
void* memalloc(size_t size, key_t key = 0, SrcLine srcline = 0)
{
    if ((size < 1) || (size >= 10e6)) throw std::runtime_error("memalloc: bad size"); //throw std::bad_alloc(); //set reasonable limits
    size += sizeof(MemHdr<IPC>);
    bool existed;
    MemHdr<IPC>* ptr = MemHdr<IPC>::alloc(size, key, &existed);
    if (!ptr) throw std::runtime_error(std::string(strerror(errno)));
    DEBUG_MSG(CYAN_MSG << timestamp() << "memalloc: alloc " << ptr->desc() << ENDCOLOR_ATLINE(srcline), debug_msg);
//    DEBUG_MSG(BLUE_MSG << timestamp() << "shmalloc: shmat id " << FMT("0x%lx") << shmid << " => " << FMT("%p") << ptr << ", cre by pid " << shminfo.shm_cpid << ", #att " << shminfo.shm_nattch << ENDCOLOR);
//    DEBUG_MSG(CYAN_MSG << timestamp() << "memalloc: size " << size << " => " << FMT("%p") << ptr << ENDCOLOR_ATLINE(srcline));
//    ptr->size = shminfo.shm_segsz; //size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
    ptr->size = size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
    ptr->marker = existed? (SHM_MAGIC ^ 1): SHM_MAGIC;
    return ++ptr;
}
template<bool IPC>
void* memalloc(size_t size, SrcLine srcline = 0) { return memalloc<IPC>(size, 0, srcline); }


//get mem key:
//    template<bool SHARED_inner = SHARED>
//typename std::enable_if<SHARED, key_t>::type shmkey(void* addr) { return shmptr(addr, "shmkey")->key; }
template<bool IPC>
key_t memkey(void* addr) { return memptr<IPC>(addr, "memkey")->key; }

//get size:
//size_t shmsize(void* addr) { return shmptr(addr, "shmsize")->size; }
template<bool IPC>
size_t memsize(void* addr) { return memptr<IPC>(addr, "memsize")->size; }

//tell caller if new mem blk was created:
template<bool IPC>
bool existed(void* addr) { return (memptr<IPC>(addr, "existed")->marker != SHM_MAGIC); }

//dealloc mem:
//uses shared + non-shared specializations
//template<bool SHARED_inner = SHARED>
//template<bool SHARED>
//typename std::enable_if<SHARED, void>::type memfree(void* addr, bool debug_msg, SrcLine srcline = 0)
template<bool IPC>
//typename std::enable_if<!SHARED, void>::type memfree(void* addr, bool debug_msg, SrcLine srcline = 0)
void memfree(void* addr, bool debug_msg, SrcLine srcline = 0)
{
    MemHdr<IPC>* ptr = memptr<IPC>(addr, "memfree");
    DEBUG_MSG(CYAN_MSG << timestamp() << FMT("memfree: adrs %p") << addr << FMT(" = ptr %p") << ptr << ENDCOLOR_ATLINE(srcline));
    MemHdr<IPC> info = *ptr; //copy info before destroying
//    struct shmid_ds info;
//    if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
    ptr->destroy();
    ptr = 0; //CAUTION: can't use ptr after this point
//    DEBUG_MSG(CYAN_MSG << "shmfree: dettached " << ENDCOLOR); //desc();
//    int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
//    if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
//    if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
    DEBUG_MSG(CYAN_MSG << timestamp() << "memfree: freed " << info.desc() << ENDCOLOR_ATLINE(srcline), debug_msg);
}
template<bool IPC>
void memfree(void* addr, SrcLine srcline = 0) { memfree<IPC>(addr, true, srcline); } //overload


//std::Deleter wrapper for memfree:
//based on example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
template <typename TYPE, bool IPC = true>
struct memdeleter
{ 
    void operator() (TYPE* ptr) const { MemHdr<IPC>::memfree(ptr); }
//    {
//        std::cout << "Call delete from function object...\n";
//        delete p;
//    }
};


///////////////////////////////////////////////////////////////////////////////
////
/// Shared/non-shared memory wrapper:
//

//pointer to block of memory:
//can be heap or shmem
template <typename TYPE = void, bool IPC = false>
class MemPtr
{
    TYPE* m_ptr;
    bool m_persist;
public: //ctor/dtor
    explicit MemPtr(size_t size = 0, int shmkey = 0, bool persist = false, SrcLine srcline = 0): m_ptr(static_cast<TYPE*>(memalloc<IPC>(sizeof(TYPE) + size, shmkey, srcline))), m_persist(persist) {}
    ~MemPtr() { /*if (m_ptr)*/ if (!m_persist) memfree<IPC>(m_ptr); }
    template <typename CALLBACK> //accept any arg type (only want caller's lambda function)
    explicit MemPtr(CALLBACK&& named_params): MemPtr(unpack(named_params), Unpacked{}) {}
public: //operators
    operator TYPE*() { return m_ptr; }
    TYPE* operator->() { return m_ptr; }
public: //members
    key_t memkey() { return ::memkey<IPC>(m_ptr); }
    size_t memsize() { return ::memsize<IPC>(m_ptr); }
    bool existed() { return ::existed<IPC>(m_ptr); }
private: //named param helpers
    struct Unpacked {}; //ctor disambiguation tag
    struct CtorParams
    {
        size_t size = 0;
        int shmkey = 0;
        bool persist = false;
        SrcLine srcline = 0;
    };
    MemPtr(const CtorParams& params, Unpacked): MemPtr(params.size, params.shmkey, params.persist, params.srcline) {}
    template <typename CALLBACK>
    static struct CtorParams& unpack(CALLBACK&& named_params)
    {
        static struct CtorParams params; //need "static" to preserve address after return
        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
        auto thunk = [](auto get_params, struct CtorParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(named_params, params);
        return params;
    }
private:
#if 1
//helper class to ensure unlock() occurs after member function returns
//TODO: derive from unique_ptr<>
    class unlock_later
    {
        TYPE* m_ptr;
    public: //ctor/dtor to wrap lock/unlock
        unlock_later(TYPE* ptr): m_ptr(ptr) { /*if (AUTO_LOCK)*/ m_ptr->m_mutex.lock(); }
        ~unlock_later() { /*if (AUTO_LOCK)*/ m_ptr->m_mutex.unlock(); }
    public:
        inline TYPE* operator->() { return m_ptr; } //allow access to wrapped members
//        inline operator TYPE() { return *m_ptr; } //allow access to wrapped members
//        inline TYPE& base() { return *m_ptr; }
//        inline TYPE& operator()() { return *m_ptr; }
    };
#endif
};


///////////////////////////////////////////////////////////////////////////////
////
/// Higher level utility/mixin classes
//

//#include <atomic>
//#include "msgcolors.h"
//#include "ostrfmt.h" //FMT()


#ifndef SIZEOF
 #define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))
#endif

#define divup(num, den)  (((num) + (den) - 1) / (den))
#define rdup(num, den)  (divup(num, den) * (den))
//#define size32_t  uint32_t //don't need huge sizes in shared memory; cut down on wasted bytes

#define MAKE_TYPENAME(type)  template<> const char* type::TYPENAME() { return #type; }


//"perfect forwarding" (typesafe args) to ctor:
//proxy/perfect forwarding info:
// http://www.stroustrup.com/wrapper.pdf
// https://stackoverflow.com/questions/24915818/c-forward-method-calls-to-embed-object-without-inheritance
// https://stackoverflow.com/questions/13800449/c11-how-to-proxy-class-function-having-only-its-name-and-parent-class/13885092#13885092
// http://cpptruths.blogspot.com/2012/06/perfect-forwarding-of-parameter-groups.html
// http://en.cppreference.com/w/cpp/utility/functional/bind
#define PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    explicit type(ARGS&& ... args): base(std::forward<ARGS>(args) ...)
//special handling for last param:

#define NEAR_PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    explicit type(ARGS&& ... args, key_t key = 0): base(std::forward<ARGS>(args) ..., key)


#define PAGE_SIZE  4096 //NOTE: no Userland #define for this, so set it here and then check it at run time; https://stackoverflow.com/questions/37897645/page-size-undeclared-c
#if 0
#include <unistd.h> //sysconf()
//void check_page_size()
int main_other(int argc, const char* argv[]);
//template <typename... ARGS >
//int main_other(ARGS&&... args)
//{ if( a<b ) std::bind( std::forward<FN>(fn), std::forward<ARGS>(args)... )() ; }
int main(int argc, const char* argv[])
#define main  main_other
{
    int pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize != PAGE_SIZE) DEBUG_MSG(RED_MSG << "PAGE_SIZE incorrect: is " << pagesize << ", expected " << PAGE_SIZE << ENDCOLOR)
    else DEBUG_MSG(GREEN_MSG << "PAGE_SIZE correct: " << pagesize << ENDCOLOR);
    return main(argc, argv);
}
#endif


//minimal memory pool:
//can be used with stl containers, but will only reclaim/dealloc space at highest used address
//NOTE: do not store pointers; addresses can vary between processes
//use shmalloc + placement new to put this in shared memory
//NOTE: no key/symbol mgmt here; use a separate lkup sttr or container
template <int SIZE> //, int PAGESIZE = 0x1000>
class MemPool
{
//    SrcLine m_srcline;
public:
    MemPool(SrcLine srcline = 0): m_storage{2} { debug(srcline); } // { m_storage[0] = 1; } //: m_used(m_storage[0]) { m_used = 0; }
    ~MemPool() { DEBUG_MSG(YELLOW_MSG << timestamp() << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); }
public:
    inline size_t used(size_t req = 0) { return (m_storage[0] + req) * sizeof(m_storage[0]); }
    inline size_t avail() { return (SIZEOF(m_storage) - m_storage[0]) * sizeof(m_storage[0]); }
//    inline void save() { m_storage[1] = m_storage[0]; }
//    inline void restore() { m_storage[0] = m_storage[1]; }
    inline size_t nextkey() { return m_storage[0]; }
    void* alloc(size_t count, size_t key, SrcLine srcline = 0) //repeat a previous alloc()
    {
        size_t saved = m_storage[0];
        m_storage[0] = key;
        void* retval = alloc(count, srcline);
        m_storage[0] = saved;
        return retval;
    }
    /*virtual*/ void* alloc(size_t count, SrcLine srcline = 0)
    {
        if (count < 1) return nullptr;
        count = divup(count, sizeof(m_storage[0])); //for alignment
        DEBUG_MSG(BLUE_MSG << timestamp() << timestamp() << "MemPool: alloc count " << count << ", used(req) " << used(count + 1) << " vs size " << sizeof(m_storage) << ENDCOLOR_ATLINE(srcline));
        if (used(count + 1) > sizeof(m_storage)) enlarge(used(count + 1));
        m_storage[m_storage[0]++] = count;
        void* ptr = &m_storage[m_storage[0]];
        m_storage[0] += count;
        DEBUG_MSG(YELLOW_MSG << timestamp() << "MemPool: allocated " << count << " octobytes at " << FMT("%p") << ptr << ", used " << used() << ", avail " << avail() << ENDCOLOR);
        return ptr; //malloc(count);
    }
    /*virtual*/ void free(void* addr, SrcLine srcline = 0)
    {
//        free(ptr);
        size_t inx = ((intptr_t)addr - (intptr_t)m_storage) / sizeof(m_storage[0]);
        if ((inx < 1) || (inx >= SIZE)) DEBUG_MSG(RED_MSG << "inx " << inx << ENDCOLOR);
        if ((inx < 1) || (inx >= SIZE)) throw std::bad_alloc();
        size_t count = m_storage[--inx] + 1;
        DEBUG_MSG(BLUE_MSG << timestamp() << "MemPool: deallocate count " << count << " at " << FMT("%p") << addr << ", reclaim " << m_storage[0] << " == " << inx << " + " << count << "? " << (m_storage[0] == inx + count) << ENDCOLOR_ATLINE(srcline));
        if (m_storage[0] == inx + count) m_storage[0] -= count; //only reclaim at end of pool (no other addresses will change)
        DEBUG_MSG(YELLOW_MSG << timestamp() << "MemPool: deallocated " << count << " octobytes at " << FMT("%p") << addr << ", used " << used() << ", avail " << avail() << ENDCOLOR);
    }
    /*virtual*/ void enlarge(size_t count, SrcLine srcline = 0)
    {
        count = rdup(count, PAGE_SIZE / sizeof(m_storage[0]));
        DEBUG_MSG(RED_MSG << timestamp() << "MemPool: want to enlarge " << count << " octobytes" << ENDCOLOR_ATLINE(srcline));
        throw std::bad_alloc(); //TODO
    }
    static const char* TYPENAME();
//private:
#define EXTRA  2
    void debug(SrcLine srcline = 0) { DEBUG_MSG(BLUE_MSG << timestamp() << "MemPool: size " << SIZE << " (" << sizeof(*this) << " actually) = #elements " << (SIZEOF(m_storage) - 1) << "+1, sizeof(units) " << sizeof(m_storage[0]) << ENDCOLOR_ATLINE(srcline)); }
//    typedef struct { size_t count[1]; uint32_t data[0]; } entry;
    size_t m_storage[EXTRA + divup(SIZE, sizeof(size_t))]; //first element = used count
//    size_t& m_used;
#undef EXTRA
};
//NOTE: caller might need to define these:
//template<>
//const char* MemPool<40>::TYPENAME() { return "MemPool<40>"; }


//mutex with lock indicator:
//lock flag is atomic to avoid extra locks
class MutexWithFlag: public std::mutex
{
public:
//use atomic data member rather than a getter so caller doesn't need to lock/unlock each time just to check locked flag
    std::atomic<bool> islocked; //NOTE: mutex.try_lock() is not reliable (spurious failures); use explicit flag instead; see: http://en.cppreference.com/w/cpp/thread/mutex/try_lock
public: //ctor/dtor
    MutexWithFlag(): islocked(false) {}
//    ~MutexWithFlag() { if (islocked) unlock(); }
    ~MutexWithFlag() { DEBUG_MSG(BLUE_MSG << timestamp() << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); if (islocked) unlock(); }
public: //member functions
    void lock() { debug("lock"); std::mutex::lock(); islocked = true; }
    void unlock() { debug("unlock"); islocked = false; std::mutex::unlock(); }
//    static void unlock(MutexWithFlag* ptr) { ptr->unlock(); } //custom deleter for use with std::unique_ptr
    static const char* TYPENAME();
private:
    void debug(const char* func) { DEBUG_MSG(YELLOW_MSG << timestamp() << func << ENDCOLOR); }
};
const char* MutexWithFlag::TYPENAME() { return "MutexWithFlag"; }


#if 0
//ref count mixin class:
template <class TYPE>
class WithRefCount: public TYPE
{
    int m_count;
public: //ctor/dtor
    PERFECT_FWD2BASE_CTOR(WithRefCount, TYPE), m_count(0) { /*addref()*/; } //std::cout << "with mutex\n"; } //, islocked(m_mutex.islocked /*false*/) {} //derivation
    ~WithRefCount() { /*delref()*/; }
public: //helper class to clean up ref count
    typedef WithRefCount<TYPE> type;
    typedef void (*_Destructor)(type*); //void* data); //TODO: make generic?
    class Scope
    {
        type& m_obj;
        _Destructor& m_clup;
    public: //ctor/dtor
        Scope(type& obj, _Destructor&& clup): m_obj(obj), m_clup(clup) { m_obj.addref(); }
        ~Scope() { if (!m_obj.delref()) m_clup(&m_obj); }
    };
//    std::shared_ptr<type> clup(/*!thread.isParent()? 0:*/ &testobj, [](type* ptr) { DEBUG_MSG("bye " << ptr->numref() << "\n"); if (ptr->dec()) return; ptr->~TestObj(); shmfree(ptr, SRCLINE); });
public:
    int& numref() { return m_count; }
    int& addref() { return ++m_count; }
    int& delref() { /*DEBUG_MSG("dec ref " << m_count - 1 << "\n")*/; return --m_count; }
};
#endif


//mutex mixin class:
//3 ways to wrap parent class methods:
//    void* ptr1 = pool20->alloc(10);
//    void* ptr2 = pool20()().alloc(4);
//    void* ptr3 = pool20.base().base().alloc(1);
// https://stackoverflow.com/questions/4421706/what-are-the-basic-rules-and-idioms-for-operator-overloading?rq=1
//NOTE: operator-> is the only one that is applied recursively until a non-class is returned, and then it needs a pointer


//used to attach mutex directly to another object type (same memory space)
//optional auto lock/unlock around access to methods
//also wraps all method calls via operator-> with mutex lock/unlock
template <class TYPE, bool AUTO_LOCK = true> //derivation
//class WithMutex //mixin/wrapper
class WithMutex: public TYPE //derivation
{
public: //ctor/dtor
//    typedef TYPE base_type;
    typedef WithMutex<TYPE, AUTO_LOCK> this_type;
//    typedef decltype(*this) this_type;
//    Mutexed(): m_locked(false) {} //mixin
//    bool& islocked;
    PERFECT_FWD2BASE_CTOR(WithMutex, TYPE) {} //std::cout << "with mutex\n"; } //, islocked(m_mutex.islocked /*false*/) {} //derivation
//    PERFECT_FWD2BASE_CTOR(WithMutex, m_wrapped), m_locked(false) {} //wrapped
//    Mutexed(TYPE* ptr):
    ~WithMutex() { DEBUG_MSG(BLUE_MSG << timestamp() << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); }
//    TYPE* operator->() { return this; } //allow access to parent members (auto-upcast only needed for derivation)
private:
//protected:
//    TYPE m_wrapped;
public:
    MutexWithFlag /*std::mutex*/ m_mutex;
//    std::atomic<bool> m_locked; //NOTE: mutex.try_lock() is not reliable (spurious failures); use explicit flag instead; see: http://en.cppreference.com/w/cpp/thread/mutex/try_lock
public:
//    bool islocked() { return m_locked; } //if (m_mutex.try_lock()) { m_mutex.unlock(); return false; }
//private:
//    void lock() { DEBUG_MSG(YELLOW_MSG << "lock" << ENDCOLOR); m_mutex.lock(); /*islocked = true*/; }
//    void unlock() { DEBUG_MSG(YELLOW_MSG << "unlock" << ENDCOLOR); /*islocked = false*/; m_mutex.unlock(); }
private:
#if 1
//helper class to ensure unlock() occurs after member function returns
//TODO: derive from unique_ptr<>
    class unlock_later
    {
        this_type* m_ptr;
    public: //ctor/dtor to wrap lock/unlock
        unlock_later(this_type* ptr): m_ptr(ptr) { /*if (AUTO_LOCK)*/ m_ptr->m_mutex.lock(); }
        ~unlock_later() { /*if (AUTO_LOCK)*/ m_ptr->m_mutex.unlock(); }
    public:
        inline TYPE* operator->() { return m_ptr; } //allow access to wrapped members
//        inline operator TYPE() { return *m_ptr; } //allow access to wrapped members
//        inline TYPE& base() { return *m_ptr; }
//        inline TYPE& operator()() { return *m_ptr; }
    };
#endif
#if 0
    typedef std::unique_ptr<this_type, void(*)(this_type*)> my_ptr_type;
    class unlock_later: public my_ptr_type
    {
    public: //ctor/dtor to wrap lock/unlock
//        PERFECT_FWD2BASE_CTOR(unlock_later, my_type) {} //std::cout << "with mutex\n"; } //, islocked(m_mutex.islocked /*false*/) {} //derivation
        unlock_later(this_type* ptr): my_ptr_type(ptr) { base_type::get()->m_mutex.lock(); }
        ~unlock_later() { base_type::get()->m_mutex.unlock(); }
//    public:
    };
#endif
public: //pointer operator; allows safe multi-process access to shared object's member functions
//nope    TYPE* /*ProxyCaller*/ operator->() { typename WithMutex<TYPE>::scoped_lock lock(m_inner.ptr); return m_inner.ptr; } //ProxyCaller(m_ps.ptr); } //https://stackoverflow.com/questions/22874535/dependent-scope-need-typename-in-front
//    typedef std::unique_ptr<this_type, void(*)(this_type*)> unlock_later; //decltype(&MutexWithFlag::unlock)> unlock_later;
//    typedef std::unique_ptr<TYPE, void(*)(TYPE*)> unlock_later; //decltype(&MutexWithFlag::unlock)> unlock_later;
#if 0
    template<bool NEED_LOCK = AUTO_LOCK>
    inline typename std::enable_if<!NEED_LOCK, TYPE*>::type operator->() { return this; }
    template<bool NEED_LOCK = AUTO_LOCK>
//    inline typename std::enable_if<NEED_LOCK, unlock_later/*&&*/>::type operator->() { m_mutex.lock(); return unlock_later(this, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    inline typename std::enable_if<NEED_LOCK, unlock_later/*&&*/>::type operator->() { return unlock_later(this); } //, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
#endif
    inline unlock_later operator->() { return unlock_later(this); } //, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
//    inline unlock_later/*&&*/ operator->() { m_mutex.lock(); return unlock_later(this, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
//alternate approach: perfect forwarding without operator->; generates more code
//TODO?    template<typename ... ARGS> \
//    type(ARGS&& ... args): base(std::forward<ARGS>(args) ...)
    static const char* TYPENAME();
#if 0
    inline unlock_later/*&&*/ operator->() { return unlock_later(this); }
//    inline operator TYPE() { return unlock_later(this); }
    inline unlock_later locked() { return unlock_later(this); }
    inline TYPE& nested() { return locked().base(); }
//    inline unlock_later operator()() { return unlock_later(this); }
//    TYPE* operator->() { return this; } //allow access to parent members (auto-upcast only needed for derivation)
#endif
//public:
//    static void lock() { std::cout << "lock\n" << std::flush; }
//    static void unlock() { std::cout << "unlock\n" << std::flush; }
};
//NOTE: caller might need to define these:
//template<>
//const char* WithMutex<MemPool<40>, true>::TYPENAME() { return "WithMutex<MemPool<40>, true>"; }


#if 0 //use ShmPtr<> instead
//shm object wrapper:
//use operator-> to access wrapped object's methods
//std::shared_ptr<> used to clean up shmem afterwards
//NOTE: use key to share objects across processes
template <typename TYPE> //, typename BASE_TYPE = std::unique_ptr<TYPE, void(*)(TYPE*)>>
class shm_obj//: public TYPE& //: public std::unique_ptr<TYPE, void(*)(TYPE*)> //use smart ptr to clean up afterward
{
//    typedef std::shared_ptr<TYPE /*, shmdeleter<TYPE>*/> base_type; //clup(&this, shmdeleter<TYPE());
//    typedef std::unique_ptr<TYPE, shmdeleter<TYPE>> base_type; //clup(&thisl, shmdeleter<TYPE>());
//    typedef std::unique_ptr<TYPE, void(*)(TYPE*)> base_type;
//    typedef std::unique_ptr<TYPE, void(*)(TYPE*)> base_type; //decltype(&MutexWithFlag::unlock)> unlock_later;
//    base_type m_ptr; //clean up shmem automatically
    std::shared_ptr<TYPE /*, shmdeleter<TYPE>*/> m_ptr; //clean up shmem automtically
public: //ctor/dtor
//equiv to:    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
#define INNER_CREATE(args, key)  m_ptr(new (shmalloc(sizeof(TYPE), key, SRCLINE)) TYPE(args), [](TYPE* ptr) { shmfree(ptr, SRCLINE); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    NEAR_PERFECT_FWD2BASE_CTOR(shm_obj, INNER_CREATE) {} //, m_clup(this, TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) {} //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
#undef INNER_CREATE
public: //operators
//    inline TYPE* operator->() { return base_type::get(); } //allow access to wrapped members; based on http://www.stroustrup.com/wrapper.pdf
//    inline operator TYPE&() { return *m_ptr.get(); }
    inline TYPE& operator->() { return *m_ptr.get(); }
    static const char* TYPENAME();
//private:
//    void debug() { DEBUG_MSG((*this)->TYPENAME << ENDCOLOR); }
};
//NOTE: caller might need to define these:
//template<>
//const char* shm_obj<WithMutex<MemPool<40>, true>>::TYPENAME() { return "shm_obj<WithMutex<MemPool<40>, true>>"; }
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// shmem ptr with auto-cleanup:
//

//#pragma message("TODO: use named args")
//https://marcoarena.wordpress.com/2014/12/16/bring-named-parameters-in-modern-cpp/
//TEST_F(SomeDiceGameConfig, JustTwoTurnsGame)
//{
//   auto gameConfig = CreateGameConfig( [](auto& r) {
//       r.NumberOfDice = 5u;
//       r.MaxDiceValue = 6;
//       r.NumberOfTurns = 2u;
//   });
//}

#if 0
//kludge: allow caller to pass run-time params into ShmPtr ctor without changing ShmPtr ctor signature
//use base class so it can be shared between all templates
class ShmPtr_params
{
public: //ctor/dtor
//    ShmPtr_params(const char* where = "ctor"): ShmPtr_params(0, 0, true, true, where) {}
    ShmPtr_params(/*const char* where = "ctor",*/ SrcLine srcline = 0, int key = 0, int extra = 0, bool want_reinit = true, bool debug_free = true)
    {
        debug(srcline /*where*/, "before");
        ShmKey = key;
        Extra = extra;
        WantReInit = want_reinit;
        DebugFree = debug_free;
        debug(srcline /*where*/, "after");
    }
public: //members
    static int ShmKey;
    static int Extra;
    static bool WantReInit;
    static bool DebugFree;
//public: //methods
    void debug(SrcLine srcline = 0, /*const char* where = "",*/ const char* when = "")
    {
//        DEBUG_MSG(BLUE_MSG << timestamp() << "ShmPtr params " << when << FMT(": key 0x%x") << ShmKey << ", extra " << Extra << ", init? " << WantInit << ", debug free? " << DebugFree << ENDCOLOR_ATLINE(srcline));
    }
//    static void defaults()
//    {
//        ShmKey = 0;
//        WantInit = true;
//    }
};
int ShmPtr_params::ShmKey;
int ShmPtr_params::Extra;
bool ShmPtr_params::WantReInit;
bool ShmPtr_params::DebugFree;
ShmPtr_params defaults(SRCLINE); //"default"); //DRY kludge: set params to default values

#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ [&](auto& _)
#endif

//shmem ptr wrapper class:
//like std::safe_ptr<> but for shmem:
//additional params are passed via template to keep ctor signature clean (for perfect fwding)
template <typename TYPE, /*int KEY = 0, int EXTRA = 0, bool INIT = true,*/ bool AUTO_LOCK = true>
class ShmPtr: public ShmPtr_params
{
//    WithMutex<TYPE>* m_ptr;
//    struct ShmContents
//    {
//        int shmid; //need this to dettach/destroy later (key might have been generated)
//        std::mutex mutex; //need a mutex to control access, so include it with shmem object
//        std::atomic<bool> locked; //NOTE: mutex.try_lock() is not reliable (spurious failures); use explicit flag instead; see: http://en.cppreference.com/w/cpp/thread/mutex/try_lock
//        TYPE data; //caller's data (could be larger)
//    }* m_ptr;
    typedef typename std::conditional<AUTO_LOCK, WithMutex<TYPE, AUTO_LOCK>, TYPE>::type shm_type; //see https://stackoverflow.com/questions/17854407/how-to-make-a-conditional-typedef-in-c?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    shm_type* m_ptr;
    bool m_want_init, m_debug_free;
//    WithMutex<TYPE, AUTO_LOCK>* m_ptr;
//    typedef decltype(*m_ptr) shm_type;
public: //ctor/dtor
//    PERFECT_FWD2BASE_CTOR(ShmPtr, TYPE)
    template<typename ... ARGS>
//    explicit ShmPtr(ARGS&& ... args, bool want_init = true): m_ptr(0), m_want_init(want_init), debug_free(true)
    explicit ShmPtr(ARGS&& ... args): ShmPtr_params(SRCLINE /*"preserve base"*/, ShmKey, Extra, WantReInit, DebugFree), m_ptr(0), m_want_init(WantReInit), m_debug_free(DebugFree) //kludge: preserve ShmPtr_params
    {
        m_ptr = static_cast<shm_type*>(::shmalloc(sizeof(*m_ptr) + Extra, ShmKey)); //, SrcLine srcline = 0)
        ShmPtr_params defaults(SRCLINE); //("reset"); //reset to default values for next instantiation
        if (::shmexisted(m_ptr))
            if (/*!INIT ||*/ !m_ptr || !m_want_init) return;
        memset(m_ptr, 0, ::shmsize(m_ptr)); //sizeof(*m_ptr) + EXTRA); //re-init (not needed first time)
//        m_ptr->mutex.std::mutex();
//        m_ptr->locked = false;
//        m_ptr->data.TYPE();
//        m_ptr->WithMutex<TYPE>(std::forward<ARGS>(args) ...);
//        m_ptr->WithMutex<TYPE, AUTO_LOCK>(std::forward<ARGS>(args ...)); //pass args to TYPE's ctor (perfect fwding)
        new (m_ptr) shm_type(std::forward<ARGS>(args) ...); //, srcline); //pass args to TYPE's ctor (perfect fwding)
     }
    ~ShmPtr()
    {
        if (/*!INIT ||*/ !m_ptr || !m_want_init) return;
//        m_ptr->~WithMutex<TYPE>();
        m_ptr->~shm_type(); //call TYPE's dtor
//        std::cout << "good-bye ShmPtr\n" << std::flush;
        ::shmfree(m_ptr, m_debug_free);
    }
public: //operators
//    TYPE* operator->() { return m_ptr; }
    shm_type* operator->() { return m_ptr; }
//    operator TYPE&() { return *m_ptr; }
    shm_type* get() { return m_ptr; }
public: //info about shmem
//    bool debug_free;
    key_t shmkey() const { return ::shmkey(m_ptr); }
    size_t shmsize() const { return ::shmsize(m_ptr); }
    bool existed() const { return ::shmexisted(m_ptr); }
};
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// STL-compatible shm allocator
//

//typedef shm_obj<WithMutex<MemPool<PAGE_SIZE>>> ShmHeap;
//class ShmHeap;

//minimal stl allocator (C++11)
//based on example at: https://msdn.microsoft.com/en-us/library/aa985953.aspx
//NOTE: use a separate sttr for shm key/symbol mgmt
template <class TYPE, int HEAP_SIZE = 300>
struct ShmAllocator  
{  
    typedef TYPE value_type;
    typedef WithMutex<MemPool<HEAP_SIZE>> ShmHeap; //bare sttr here, not smart_ptr;
    ShmAllocator() noexcept {} //default ctor; not required by STL
    ShmAllocator(ShmHeap* heap): m_heap(heap) {}
    template<class OTHER>
    ShmAllocator(const ShmAllocator<OTHER>& other) noexcept: m_heap(other.m_heap) {} //converting copy ctor (more efficient than C++03)
    template<class OTHER>
    bool operator==(const ShmAllocator<OTHER>& other) const noexcept { return m_heap/*.get()*/ == other.m_heap/*.get()*/; } //true; }
    template<class OTHER>
    bool operator!=(const ShmAllocator<OTHER>& other) const noexcept { return m_heap/*.get()*/ != other.m_heap/*.get()*/; } //false; }
    TYPE* allocate(const size_t count = 1, SrcLine srcline = 0) const { return allocate(count, 0, srcline); }
    TYPE* allocate(const size_t count, key_t key, SrcLine srcline) const
    {
        if (!count) return nullptr;
        if (count > static_cast<size_t>(-1) / sizeof(TYPE)) throw std::bad_array_new_length();
//        void* const ptr = m_heap? m_heap->alloc(count * sizeof(TYPE), key, srcline): shmalloc(count * sizeof(TYPE), key, srcline);
        void* const ptr = m_heap? memalloc<false>(count * sizeof(TYPE), key, srcline): memalloc<true>(count * sizeof(TYPE), key, srcline);
        DEBUG_MSG(YELLOW_MSG << timestamp() << "ShmAllocator: allocated " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << FMT(" bytes for key 0x%lx") << key << " from " << (m_heap? "custom": "heap") << " at " << FMT("%p") << ptr << ENDCOLOR);
        if (!ptr) throw std::bad_alloc();
        return static_cast<TYPE*>(ptr);
    }  
    void deallocate(TYPE* const ptr, size_t count = 1, SrcLine srcline = 0) const noexcept
    {
        DEBUG_MSG(YELLOW_MSG << timestamp() << "ShmAllocator: deallocate " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << " bytes from " << (m_heap? "custom": "heap") << " at " << FMT("%p") << ptr << ENDCOLOR_ATLINE(srcline));
        if (m_heap) m_heap->free(ptr);
        else memfree<true>(ptr);
    }
//    std::shared_ptr<ShmHeap> m_heap; //allow heap to be shared between allocators
    ShmHeap* m_heap; //allow heap to be shared between allocators
    static const char* TYPENAME();
};


///////////////////////////////////////////////////////////////////////////////
////
/// Junk or obsolete code
//

#if 0
//shared memory segment:
//used to store persistent/shared data across processes
//NO: stores persistent state between ShmAllocator instances
class ShmSeg
{
public: //ctor/dtor
    enum class persist: int {Reuse = 0, NewTemp = -1, NewPerm = +1};
//    ShmSeg(persist cre = persist::NewTemp, size_t size = 0x1000): m_ptr(0), m_keep(true)
    explicit ShmSeg(key_t key = 0, persist cre = persist::NewTemp, size_t size = 0x1000): m_ptr(0), m_keep(true)
    {
        DEBUG_MSG(CYAN_MSG << "ShmSeg.ctor: key " << FMT("0x%lx") << key << ", persist " << (int)cre << ", size " << size << ENDCOLOR);
        if (cre != persist::Reuse) destroy(key); //start fresh
        m_keep = (cre != persist::NewTemp);        
        create(key, size, cre != persist::Reuse);
//            std::cout << "ctor from " << __FILE__ << ":" << __LINE__ << "\n" << std::flush;
    }
    ~ShmSeg()
    {
        detach();
        if (!m_keep) destroy(m_key);
    }
public: //getters
    inline key_t shmkey() const { return m_key; }
    inline void* shmptr() const { return m_ptr; }
//    inline size32_t shmofs(void* ptr) const { return (VOID*)ptr - (VOID*)m_ptr; } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
//    inline size32_t shmofs(void* ptr) const { return (uintptr_t)ptr - (uintptr_t)m_ptr; } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
    inline size_t shmofs(void* ptr) const { return (uintptr_t)ptr - (uintptr_t)m_ptr; } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
//    inline operator uintptr_t(void* ptr) { return (uintptr_t)(*((void**) a));
    inline size_t shmsize() const { return m_size; }
//    inline size_t shmused() const { return m_used; }
//    inline size_t shmavail() const { return m_size - m_used; }
private: //data
    key_t m_key;
    void* m_ptr;
    size_t m_size; //, m_used;
    bool m_keep;
private: //helpers
    void create(key_t key, size_t size, bool want_new)
    {
        if (!key) key = crekey(); //(rand() << 16) | 0xfeed;
//#define  SHMKEY  ((key_t) 7890) /* base value for shmem key */
//#define  PERMS 0666
//        if (cre != persist::PreExist) destroy(key); //delete first (parent wants clean start)
//        if (!key) key = (rand() << 16) | 0xfeed;
        if ((size < 1) || (size >= 10000000)) throw std::runtime_error("ShmSeg: bad size"); //set reasonable limits
        int shmid = shmget(key, size, 0666 | (want_new? IPC_CREAT | IPC_EXCL: 0)); // | SHM_NORESERVE); //NOTE: clears to 0 upon creation
        DEBUG_MSG(CYAN_MSG << "ShmSeg: cre shmget key " << FMT("0x%lx") << key << ", size " << size << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR);
        if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
        struct shmid_ds info;
        if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
        void* ptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
        DEBUG_MSG(BLUE_MSG << "ShmSeg: shmat id " << FMT("0x%lx") << shmid << " => " << FMT("%p") << ptr << ENDCOLOR);
        if (ptr == (void*)-1) throw std::runtime_error(std::string(strerror(errno)));
        m_key = key;
        m_ptr = ptr;
        m_size = info.shm_segsz; //size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
//        m_used = sizeof(size_t);
    }
//    void* alloc(size_t size)
//    {
//        if (shmavail() < size) return 0; //not enough space
//        void* retval = (intptr_t)m_ptr + m_used;
//        m_used += size;
//        return retval;
//    }
    void detach()
    {
        if (!m_ptr) return; //not attached
//  int shmctl(int shmid, int cmd, struct shmid_ds *buf);
        if (shmdt(m_ptr) == -1) throw std::runtime_error(strerror(errno));
        m_ptr = 0; //can't use m_shmptr after this point
        DEBUG_MSG(CYAN_MSG << "ShmSeg: dettached " << ENDCOLOR); //desc();
    }
    static void destroy(key_t key)
    {
        if (!key) return; //!owner or !exist
        int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
        DEBUG_MSG(CYAN_MSG << "ShmSeg: destroy " << FMT("key 0x%lx") << key << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR);
        if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
        if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
        throw std::runtime_error(strerror(errno));
    }
public: //custom helpers
    static key_t crekey() { return (rand() << 16) | 0xfeed; }
};
#endif


#if 0
template <typename TYPE>
class shm_ptr
{
public: //ctor/dtor
#define INNER_CREATE(args)  m_ptr(new (shmalloc(sizeof(TYPE))) TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    PERFECT_FWD2BASE_CTOR(shm_ptr, INNER_CREATE) {}
#undef INNER_CREATE
public: //operators
    inline TYPE* operator->() { return m_ptr.get(); } //allow access to wrapped members; based on http://www.stroustrup.com/wrapper.pdf
private:
    std::shared_ptr<TYPE> m_ptr; //use smart ptr to clean up afterward
};
#endif
//class shm_ptr
//{
//public: //ctor/dtor
//#define INNER_CREATE(args)  m_var(*new (shmalloc(sizeof(TYPE))) TYPE(args)) //pass ctor args down into m_var ctor
//    PERFECT_FWD2BASE_CTOR(shm_ptr, INNER_CREATE), m_clup(&m_var, shmdeleter<TYPE>()) {}
//#undef INNER_CREATE
//public: //operators
//    inline TYPE* operator->() { return &m_var; } //allow access to wrapped members
//private: //members
//    TYPE& m_var; //= *new (shmalloc(sizeof(TYPE))) TYPE();
//    std::shared_ptr<TYPE> m_clup; //(&var, shmdeleter<TYPE>())
//};


#if 0
class ShmHeap //: public ShmSeg
{
public:
//    PERFECT_FWD2BASE_CTOR(ShmHeap, ShmSeg), m_used(sizeof(m_used)) {}
public:
    inline size_t used() { return m_used; }
    inline size_t avail() { return shmsize() - m_used; }
    void* alloc(size_t count)
    {
//        if (count > max_size()) { throw std::bad_alloc(); }
//        pointer ptr = static_cast<pointer>(::operator new(count * sizeof(type), ::std::nothrow));
        if (m_used + count > shmsize()) throw std::bad_alloc();
        void* ptr = shmptr() + m_used;
        m_used += count;
        DEBUG_MSG(YELLOW_MSG << "ShmHeap: allocated " << count << " bytes at " << FMT("%p") << ptr << ", " << avail() << " bytes remaining" << ENDCOLOR);
        return ptr;
    }
    void dealloc(void* ptr)
    {
        int count = 1;
        DEBUG_MSG(YELLOW_MSG << "ShmHeap: deallocate " << count << " bytes at " << FMT("%p") << ptr << ", " << avail() << " bytes remaining" << ENDCOLOR);
//        ::operator delete(ptr);
    }
private:
    size_t m_used;
};
#endif


#if 0
//shmem key:
//define unique type for function signatures
//to look at shm:
// ipcs -m 
class ShmKey
{
public: //debug only
    key_t m_key;
public: //ctor/dtor
//    explicit ShmKey(key_t key = 0): key(key? key: crekey()) {}
    /*explicit*/ ShmKey(const int& key = 0): m_key(key? key: crekey()) { std::cout << "ShmKey ctor " << FMT("0x%lx\n") << m_key; }
//    explicit ShmKey(const ShmKey&& other): m_key((int)other) {} //copy ctor
//    ShmKey(const ShmKey& that) { *this = that; } //copy ctor
    ~ShmKey() {}
public: //operators
    ShmKey& operator=(const int& key) { m_key = key? key: crekey(); return *this; } //conv op; //std::cout << "key assgn " << FMT("0x%lx") << key << FMT(" => 0x%lx") << m_key << "\n"; return *this; } //conv operator
    inline operator int() { return /*(int)*/m_key; }
//    bool operator!() { return key != 0; }
//    inline key_t 
//??    std::ostream& operator<<(const ShmKey& value)
public: //static helpers
    static inline key_t crekey() { int r = (rand() << 16) | 0xfeed; /*std::cout << FMT("rnd key 0x%lx\n") << r*/; return r; }
};
#endif

#undef DEBUG_MSG
#endif //ndef _SHMALLOC_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit tests:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion
//#!/bin/bash -x
//echo -e '\e[1;36m'; g++ -O3 -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
//#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line); add "-E" to above to see raw src

//shared memory allocator test
//self-compiling c++ file; run this file to compile it; //https://stackoverflow.com/questions/17947800/how-to-compile-code-from-stdin?lq=1
//how to get name of bash script: https://stackoverflow.com/questions/192319/how-do-i-know-the-script-file-name-in-a-bash-script
//or, to compile manually:
// g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 shmalloc.cpp -o shmalloc
// gdb ./shmalloc
// bt
// x/80xw 0x7ffff7ff7000


#define WANT_TEST1
//#define WANT_TEST2
//#define WANT_TEST3
//#define WANT_TEST4
#define SHMALLOC_DEBUG //show shmalloc debug msgs
#define IPC_DEBUG //show ipc debug msgs
#define CRITICAL_DEBUG //show critical section debug msgs
#define ATOMIC_DEBUG //show ATOMIC() debug msgs

#include "ipc.h" //put first to request ipc variants; comment out for in-proc multi-threading
#include "atomic.h" //otherwise put this one first so shared mutex will be destroyed last; ATOMIC_MSG()
#include "msgcolors.h" //SrcLine, msg colors
#include "ostrfmt.h" //FMT()
#include "elapsed.h" //timestamp()
#include "shmalloc.h" //MemPool<>, WithMutex<>, ShmAllocator<>, ShmPtr<>
//#include "shmallocator.h"


//little test class for testing shared memory:
class TestObj
{
//    std::string m_name;
    char m_name[20]; //store name directly in object so shm object doesn't use char pointer
//    SrcLine m_srcline;
    int m_count;
public:
    explicit TestObj(const char* name, SrcLine srcline = 0): /*m_name(name),*/ m_count(0) { strncpy(m_name, name, sizeof(m_name)); ATOMIC_MSG(CYAN_MSG << timestamp() << FMT("TestObj@%p") << this << " '" << name << "' ctor" << ENDCOLOR_ATLINE(srcline)); }
    TestObj(const TestObj& that): /*m_name(that.m_name),*/ m_count(that.m_count) { strcpy(m_name, that.m_name); ATOMIC_MSG(CYAN_MSG << timestamp() << FMT("TestObj@%p") << this << " '" << m_name << FMT("' copy ctor from %p") << that << ENDCOLOR); }
    ~TestObj() { ATOMIC_MSG(CYAN_MSG << timestamp() << FMT("TestObj@%p") << this << " '" << m_name << "' dtor" << ENDCOLOR); } //only used for debug
public:
    void print(SrcLine srcline = 0) { ATOMIC_MSG(BLUE_MSG << timestamp() << "TestObj.print: (name" << FMT("@%p") << &m_name /*<< FMT(" contents@%p") << m_name/-*.c_str()*/ << " '" << m_name << "', count" << FMT("@%p") << &m_count << " " << m_count << ")" << ENDCOLOR_ATLINE(srcline)); }
    int& inc() { return ++m_count; }
};


///////////////////////////////////////////////////////////////////////////////
////
/// Test 1
//

#ifdef WANT_TEST1 //shm_obj test, single proc
#pragma message("Test1")

//#define MEMSIZE  rdup(10, 8)+8 + rdup(4, 8)+8
//WithMutex<MemPool<MEMSIZE>> pool20;
WithMutex<MemPool<rdup(10, 8)+8 + rdup(4, 8)+8>> pool20;
//typedef WithMutex<MemPool<MEMSIZE>> pool_type;
//pool_type pool20;
MAKE_TYPENAME(MemPool<32>)
MAKE_TYPENAME(MemPool<40>)
MAKE_TYPENAME(WithMutex<MemPool<40>>)


//int main(int argc, const char* argv[])
void unit_test()
{
    ATOMIC_MSG(PINK_MSG << timestamp() << "data space:" <<ENDCOLOR);
    void* ptr1 = pool20->alloc(10);
//    void* ptr1 = pool20()()->alloc(10);
//    void* ptr1 = pool20.base().base().alloc(10);
    void* ptr2 = pool20.alloc(4);
    pool20->free(ptr2);
    void* ptr3 = pool20->alloc(1);

    ATOMIC_MSG(PINK_MSG << timestamp() << "stack:" <<ENDCOLOR);
    MemPool<rdup(1, 8)+8 + rdup(1, 8)+8> pool10; //don't need mutex on stack mem (not safe to share it)
    void* ptr4 = pool10.alloc(1);
    void* ptr5 = pool10.alloc(1);
    void* ptr6 = pool10.alloc(0);

//    typedef decltype(pool20) pool_type; //use same type as pool20
    typedef MemPool<rdup(10, 8)+8 + rdup(4, 8)+8> pool_type; //use same type as pool20
    ATOMIC_MSG(PINK_MSG << timestamp() << "shmem: actual size " << sizeof(pool_type) << ENDCOLOR);
//    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> shmpool(new (shmalloc(sizeof(pool_type))) pool_type(), shmdeleter<pool_type>());
//    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> shmpool(new (shmalloc(sizeof(pool_type))) pool_type(), shmdeleter<pool_type>());
//    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
//    pool_type shmpool = *shmpool_ptr; //.get();
//#define SHM_DECL(type, var)  type& var = *new (shmalloc(sizeof(type))) type(); std::shared_ptr<type> var##_clup(&var, shmdeleter<type>())
#if 0
    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> clup(&shmpool, shmdeleter<pool_type>());
//OR
    std::unique_ptr<pool_type, shmdeleter<pool_type>> clup(&shmpool, shmdeleter<pool_type>());
#endif
//    SHM_DECL(pool_type, shmpool); //equiv to "pool_type shmpool", but allocates in shmem
//    shm_obj<pool_type> shmpool; //put it in shared memory instead of stack
    ShmPtr<pool_type> shmpool;
//    ATOMIC_MSG(BLUE_MSG << shmpool.TYPENAME() << ENDCOLOR); //NOTE: don't use -> here (causes recursive lock)
//    shmpool->m_mutex.lock();
//    shmpool->debug();
//    shmpool->m_mutex.unlock();
    void* ptr7 = shmpool->alloc(10);
    void* ptr8 = shmpool->alloc(4); //.alloc(4);
    shmpool->free(ptr8);
    void* ptr9 = shmpool->alloc(1);
//    shmfree(&shmpool);

//    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Test 2
//

#ifdef WANT_TEST2 //proxy example, multi-proc; BROKEN
#pragma message("Test2")
//#include "vectorex.h"
//#include <unistd.h> //fork()
//#include "ipc.h"
#ifndef IPC_THREAD
 #error "uncomment ipc.h near top for this test"
#endif

//template <>
//const char* ShmAllocator<TestObj>::TYPENAME() { return "TestObj"; }
//template <>
//const char* ShmAllocator<std::vector<TestObj, ShmAllocator<TestObj>>>::TYPENAME() { return "std::vector<TestObj>"; }
//template<>
//const char* WithMutex<TestObj, true>::TYPENAME() { return "WithMutex<TestObj, true>"; }

#define COMMA ,  //kludge: macros don't like commas within args; from https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
MAKE_TYPENAME(WithMutex<TestObj COMMA true>)
MAKE_TYPENAME(ShmAllocator<TestObj>)
MAKE_TYPENAME(WithMutex<MemPool<300> COMMA true>)
MAKE_TYPENAME(MemPool<300>)

//typedef shm_obj<WithMutex<MemPool<PAGE_SIZE>>> ShmHeap;
//template <typename TYPE, int KEY = 0, int EXTRA = 0, bool INIT = true, bool AUTO_LOCK = true>
#include "shmkeys.h"
//#include "shmalloc.h"


//#include <sys/types.h>
//#include <sys/wait.h>
//#include <unistd.h> //fork(), getpid()
//#include "ipc.h" //IpcThread(), IpcPipe()
//#include "elapsed.h" //timestamp()
//#include <memory> //unique_ptr<>
//int main(int argc, const char* argv[])
void unit_test()
{
//    ShmKey key(12);
//    std::cout << FMT("0x%lx\n") << key.m_key;
//    key = 0x123;
//    std::cout << FMT("0x%lx\n") << key.m_key;
#if 0 //best way, but doesn't match Node.js fork()
    ShmSeg shm(0, ShmSeg::persist::NewTemp, 300); //key, persistence, size
#else //use pipes to simulate Node.js pass env to child; for example see: https://bytefreaks.net/programming-2/c-programming-2/cc-pass-value-from-parent-to-child-after-fork-via-a-pipe
//    int fd[2];
//	pipe(fd); //create pipe descriptors < fork()
    IpcPipe pipe;
#endif
//    bool owner = fork();
    IpcThread thread(SRCLINE);
//    pipe.direction(owner? 1: 0);
    if (thread.isChild()) sleep(1); //give parent head start
    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent": "child") << " pid " << getpid() << " start" << ENDCOLOR);
#if 1 //simulate Node.js fork()
//    ShmSeg& shm = *std::allocator<ShmSeg>(1); //alloc space but don't init
//    ShmAllocator<ShmSeg> heap_alloc;
//used shared_ptr<> for ref counting *and* to allow skipping ctor in child procs:
//    std::shared_ptr<ShmHeap> shmheaptr; //(ShmAllocator<ShmSeg>().allocate()); //alloc space but don't init yet
//    typedef ShmPtr<WithMutex<MemPool<PAGE_SIZE>>, HEAPPAGE_SHMKEY> ShmHeap;
    typedef ShmPtr<MemPool<PAGE_SIZE>, HEAPPAGE_SHMKEY> ShmHeap;
//    typedef ShmAllocator<TestObj, PAGE_SIZE> ShmHeap;
    ShmHeap shmheaptr;
//    ShmHeap shmheap; //(ShmAllocator<ShmSeg>().allocate()); //alloc space but don't init yet
//    ShmSeg* shmptr = heap_alloc.allocate(1); //alloc space but don't init yet
//    std::unique_ptr<ShmSeg> shm = shmptr;
//    if (owner) shmptr.reset(new /*(shmptr.get())*/ ShmHeap(0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//    if (owner) shmheaptr.reset(new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(), shmdeleter<ShmHeap>()); //[](TYPE* ptr) { shmfree(ptr); }); //0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//#define INNER_CREATE(args)  m_ptr(new (shmalloc(sizeof(TYPE))) TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
//    shmheaptr.reset(thread.isParent()? new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(): (ShmHeap*)shmalloc(sizeof(ShmHeap), pipe.child_rcv(SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //call ctor to init (parent only)
//    shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), thread.isParent()? 0: pipe.rcv(SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //call ctor to init (parent only)
//    if (thread.isParent()) new (shmheaptr.get()) ShmHeap(); //call ctor to init (parent only)
//    if (thread.isParent()) pipe.send(shmkey(shmheaptr.get()), SRCLINE);
    ATOMIC_MSG(BLUE_MSG << timestamp() << FMT("shmheap at %p") << shmheaptr.get() << ENDCOLOR);
//    else shmptr.reset(new /*(shmptr.get())*/ ShmHeap(rcv(fd), ShmSeg::persist::Reuse, 1));
//    else shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), rcv(fd, SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //don't call ctor; Seg::persist::Reuse, 1));
//    std::vector<std::string> args;
//    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
//    if (!owner) new (&shm) ShmSeg(shm.shmkey(), ShmSeg::persist::Reuse, 1); //attach to same shmem seg (child only)
#endif
//parent + child have ref; parent init; parent + child use it; parent + child detach; parent frees

    TestObj bare("berry", SRCLINE);
    bare.inc();
    bare.print(SRCLINE);

    WithMutex<TestObj> prot("protected", SRCLINE);
//    ((TestObj)prot).inc();
//    ((TestObj)prot).print();
    prot->inc(); //NOTE: must use operator-> to get auto-lock wrapping
    prot->print(SRCLINE);
    
//    ProxyWrapper<Person> person(new Person("testy"));
//can't set ref to rvalue :(    ShmSeg& shm = owner? ShmSeg(0x1234beef, ShmSeg::persist::NewTemp, 300): ShmSeg(0x1234beef, ShmSeg::persist::Reuse, 300); //key, persistence, size
//    if (owner) new (&shm) ShmSeg(0x1234beef, ShmSeg::persist::NewTemp, 300);
//    else new (&shm) ShmSeg(0x1234beef, ShmSeg::persist::Reuse); //key, persistence, size

//    ShmHeap<TestObj> shm_heap(shm_seg);
//    ATOMIC_MSG("shm key " << FMT("0x%lx") << shm.shmkey() << ", size " << shm.shmsize() << ", adrs " << FMT("%p") << shm.shmptr() << "\n");
//    std::set<Example, std::less<Example>, allocator<Example, heap<Example> > > foo;
//    typedef ShmAllocator<TestObj> item_allocator_type;
//    item_allocator_type item_alloc; item_alloc.m_heap = shmptr; //explicitly create so it can be reused in other places (shares state with other allocators)
    ShmAllocator<TestObj> item_alloc(shmheaptr.get()); //item_alloc.m_heap = shmptr; //explicitly create so it can be reused in other places (shares state with other allocators)
//reuse existing shm obj in child proc (bypass ctor):

//    if (owner) shmheaptr.reset(new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(), shmdeleter<ShmHeap>()); //[](TYPE* ptr) { shmfree(ptr); }); //0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//    if (owner) send(fd, shmkey(shmheaptr.get()), SRCLINE);
//    else shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), rcv(fd, SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //don't call ctor; Seg::persist::Reuse, 1));

    key_t svkey = shmheaptr->nextkey();
    TestObj& testobj = *(TestObj*)item_alloc.allocate(1, svkey, SRCLINE); //NOTE: ref avoids copy ctor
    ATOMIC_MSG(BLUE_MSG << timestamp() << "next key " << svkey << FMT(" => adrs %p") << &testobj << ENDCOLOR);
    if (thread.isParent()) new (&testobj) TestObj("testy", SRCLINE);
//    TestObj& testobj = owner? *new (item_alloc.allocate(1, svkey, SRCLINE)) TestObj("testy", SRCLINE): *(TestObj*)item_alloc.allocate(1, svkey, SRCLINE); //NOTE: ref avoids copy ctor
//    shm_ptr<TestObj> testobj("testy", shm_alloc);
    testobj.inc();
    testobj.inc();
    testobj.print(SRCLINE);
    ATOMIC_MSG(BLUE_MSG << timestamp() << FMT("&testobj %p") << &testobj << ENDCOLOR);

#if 0
//    typedef std::vector<TestObj, item_allocator_type> list_type;
    typedef std::vector<TestObj, ShmAllocator<TestObj>> list_type;
//    list_type testlist;
//    ShmAllocator<list_type, ShmHeap<list_type>>& list_alloc = item_alloc.rebind<list_type>.other;
//    item_allocator_type::rebind<list_type> list_alloc;
//    typedef typename item_allocator_type::template rebind<list_type>::other list_allocator_type; //http://www.cplusplus.com/forum/general/161946/
//    typedef ShmAllocator<list_type> list_allocator_type;
//    SmhAllocator<list_type, ShmHeap<list_type>> shmalloc;
//    list_allocator_type list_alloc(item_alloc); //list_alloc.m_heap = shmptr; //(item_alloc); //share state with item allocator
    ShmAllocator<list_type> list_alloc(item_alloc); //list_alloc.m_heap = shmptr; //(item_alloc); //share state with item allocator
//    list_type testobj(item_alloc); //stack variable
//    list_type* ptr = list_alloc.allocate(1);
    svkey = shmheaptr->nextkey();
//    list_type& testlist = *new (list_alloc.allocate(1, SRCLINE)) list_type(item_alloc); //(item_alloc); //custom heap variable
    list_type& testlist = *(list_type*)list_alloc.allocate(1, svkey, SRCLINE);
    if (owner) new (&testlist) list_type(item_alloc); //(item_alloc); //custom heap variable
    ATOMIC_MSG(BLUE_MSG << FMT("&list %p") << &testlist << ENDCOLOR);

    testlist.emplace_back("list1");
    testlist.emplace_back("list2");
    testlist[0].inc();
    testlist[0].inc();
    testlist[1].inc();
    testlist[0].print();
    testlist[1].print();
#endif

    ATOMIC_MSG(BLUE_MSG
        << timestamp()
        << "sizeof(berry) = " << sizeof(bare)
        << ", sizeof(prot) = " << sizeof(prot)
        << ", sizeof(test obj) = " << sizeof(testobj)
//        << ", sizeof(test list) = " << sizeof(testlist)
//        << ", sizeof(shm_ptr<int>) = " << sizeof(shm_ptr<int>)
        << ENDCOLOR);

    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent (waiting to)": "child") << " exit" << ENDCOLOR);
    if (thread.isParent()) thread.join(SRCLINE); //waitpid(-1, NULL /*&status*/, /*options*/ 0); //NOTE: will block until child state changes
    else shmheaptr.reset((ShmHeap*)0); //don't call dtor for child
//    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Test 3
//

#ifdef WANT_TEST3 //generic shm usage pattern
#pragma message("Test3")
#ifndef IPC_THREAD
 #error "uncomment ipc.h near top for this test"
#endif
//usage:
//    ShmMsgQue& msgque = *(ShmMsgQue*)shmalloc(sizeof(ShmMsgQue), shmkey, SRCLINE);
//    if (isParent) new (&msgque) ShmMsgQue(name, SRCLINE); //call ctor to init (parent only)
//    ...
//    if (isParent) { msgque.~ShmMsgQue(); shmfree(&msgque, SRCLINE); }

#include "vectorex.h"
#include "shmkeys.h"
//#include <unistd.h> //fork()
//#include "ipc.h" //IpcThread(), IpcPipe()
//#include <memory> //unique_ptr<>

//template <>
//const char* ShmAllocator<TestObj>::TYPENAME() { return "TestObj"; }
//template <>
//const char* ShmAllocator<std::vector<TestObj, ShmAllocator<TestObj>>>::TYPENAME() { return "std::vector<TestObj>"; }
//template<>
//const char* WithMutex<TestObj, true>::TYPENAME() { return "WithMutex<TestObj, true>"; }
#define COMMA ,  //kludge: macros don't like commas within args; from https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
MAKE_TYPENAME(WithMutex<TestObj COMMA true>)

//template<typename TYPE>
//TYPE& shmobj(typedef TYPE& Ref; //kludge: C++ doesn't like "&" on derivation line

//int main(int argc, const char* argv[])
void unit_test()
{
//    IpcPipe pipe; //create pipe descriptors < fork()
    IpcThread thread(SRCLINE);
//    pipe.direction(owner? 1: 0);
    if (thread.isChild()) sleep(1); //run child after parent
    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent": "child") << " pid " << getpid() << " start" << ENDCOLOR);

#if 0 //private object
    TestObj testobj("testobj", SRCLINE);
#elif 0 //explicitly shared object
//    typedef WithRefCount<TestObj> type; //NOTE: assumes parent + child access overlap
//    typedef ShmObj<TestObj> type;
    TestObj& testobj = *(TestObj*)shmalloc(sizeof(TestObj), thread.ParentKeyGen(SRCLINE), SRCLINE);
    if (thread.isParent()) thread.send(shmkey(new (&testobj) TestObj("testobj", SRCLINE)), SRCLINE); //call ctor to init, shared shmkey with child (parent only)
//    else testobj.inc();
//    std::shared_ptr<type> clup(/*!thread.isParent()? 0:*/ &testobj, [](type* ptr) { ATOMIC_MSG("bye " << ptr->numref() << "\n"); if (ptr->dec()) return; ptr->~TestObj(); shmfree(ptr, SRCLINE); });
//    type::Scope clup(testobj, thread, [](type* ptr) { ATOMIC_MSG("bye\n"); ptr->~type(); shmfree(ptr, SRCLINE); });
    std::shared_ptr<TestObj> clup(/*!thread.isParent()? 0:*/ &testobj, [thread](TestObj* ptr) { if (!thread.isParent()) return; ATOMIC_MSG("bye\n"); ptr->~TestObj(); shmfree(ptr, SRCLINE); });
#else //automatically shared object
    typedef WithMutex<TestObj> type;
//    ShmScope<type> scope(thread, SRCLINE, "testobj", SRCLINE); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    ShmScope<type, 2> scope(SRCLINE, "testobj", SRCLINE); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    type& testobj = scope.shmobj.data; //ShmObj<TestObj>("testobj", thread, SRCLINE);
    ShmPtr_params(SRCLINE, TESTOBJ_SHMKEY, 0, thread.isParent(), true);
    ShmPtr<TestObj /*, TESTOBJ_SHMKEY, 0, false*/> testobj("testobj", SRCLINE); //, thread.isParent()); //only allow parent to init/destroy object
//    thread.shared<TestObj> testobj("testobj", SRCLINE);
//    Shmobj<TestObj> testobj("testobj", thread, SRCLINE);

//call ctor to init, shared shmkey with child (parent only), destroy later:
//#define SHARED(ctor, thr)  \
//    *(TestObj*)shmalloc(sizeof(TestObj), thr.isParent()? 0: thr.rcv(SRCLINE), SRCLINE); \
//    if (thr.isParent()) thr.send(shmkey(new (&testobj) ctor), SRCLINE); \
//    std::shared_ptr<TestObj> dealloc(&testobj, [](TestObj* ptr){ ptr->~TestObj(); shmfree(ptr, SRCLINE); }
//    TestObj& testobj = SHARED(TestObj("shmobj", SRCLINE), thread);
//    IpcShared<TestObj> TestObj("shmobj", SRCLINE), thread);
#endif
    testobj->inc(); //testobj.inc();
    testobj->print(SRCLINE); //testobj.print();

    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent (waiting to)": "child") << " exit" << ENDCOLOR);
    if (thread.isParent()) thread.join(SRCLINE); //don't let parent go out of scope before child (parent calls testobj dtor, not child); NOTE: blocks until child state changes
//    if (thread.isParent()) { testobj.~TestObj(); shmfree(&testobj, SRCLINE); } //only parent will destroy obj
//    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Test 4
//

#ifdef WANT_TEST4 //generic shm usage pattern
#pragma message("Test4")
//#include <memory> //unique_ptr<>
#include "shmkeys.h"
#include "critical.h"


//#define COMMA ,  //kludge: macros don't like commas within args; from https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
//MAKE_TYPENAME(WithMutex<TestObj COMMA true>)

#if 0
class thing
{
    const char* m_name;
public:
    thing(const char* name): m_name(name) { std::cout << m_name << " thing ctor\n"; }
    thing(const thing& that) { std::cout << "cpoy " << that.m_name << "\n"; }
    ~thing() { std::cout << m_name << " thing dtor\n"; }
};
    {
        std::vector<thing> things;
        things.reserve(5);
        things.emplace_back("th1");
        std::cout << "here1 " << things.size() << "\n";
        things.emplace_back("th2");
        std::cout << "here2 " << things.size() << "\n";
        {
            things.emplace_back("th3");
            std::cout << "here3 " << things.size() << "\n";
        }
        std::cout << "here4 " << things.size() << "\n";
        things.emplace_back("th4");
        std::cout << "here5 " << things.size() << "\n";
    }
    std::cout << "here6\n";
#endif

//int main(int argc, const char* argv[])
void unit_test()
{
    ATOMIC_MSG(BLUE_MSG << timestamp() << "start test 4" << ENDCOLOR);
//    ShmPtr_params(SRCLINE, 0x444decaf, 100, true); //!i && thread.isParent());
//    ShmPtr<TestObj> objptr("shmobj", SRCLINE);
//    IpcThread threads[4];
//    ATOMIC_MSG(PINK_MSG << timestamp() << "start pid " << IpcThread::get_id() << ENDCOLOR);
//    /*std::atomic<int>*/ int first = 0;
    std::vector<IpcThread> threads; //prevent going out of scope and forcing join until all children created
    threads.reserve(4); //alloc space to avoid extraneous copy ctor calls later
//    for (int i = 0; i < 4; ++i)
//        if (!i ||)
    for (int i = 0; i < threads.capacity(); ++i)
    {
//        IpcThread thread;
        IpcThread& thread = (threads.emplace_back(SRCLINE), threads[i]); //threads.size() - 1]; //add one new thread; outer scope delays dtor
        ATOMIC_MSG(PINK_MSG << timestamp() << thread.proctype() << "[" << i << "] pid " << thread.get_id() << ENDCOLOR);
        if (!i || !thread.isParent())
        {
            CriticalSection<SHARED_CRITICAL_SHMKEY> cs(SRCLINE); //ensure ShmPtr_params used correctly
            ShmPtr_params(SRCLINE, TESTOBJ_SHMKEY, 100, false); //!first++); //!i && thread.isParent()); //init first time only
            ShmPtr<TestObj, false> objptr("shmobj", SRCLINE); //NOTE: don't need auto-lock due to critical section
            objptr->inc();
            objptr->inc();
            objptr->inc();
            objptr->print();
        }
//        objptr->lock();
        if (!thread.isParent()) break;
        thread.allow_any(); //children might not execute in order
    }
    ATOMIC_MSG(BLUE_MSG << timestamp() << "finish" << ENDCOLOR);
//    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////

#ifdef JUNK
//#define ShmHeap  ShmHeapAlloc::ShmHeap

//define shared memory heap:
//ShmHeap shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);
ShmHeap ShmHeapAlloc::shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);


//typename ... ARGS>
//unique_ptr<T> factory(ARGS&&... args)
//{
//    return unique_ptr<T>(new T { std::forward<ARGS>(args)... });
//}
//    explicit vector_ex(std::size_t count = 0, const ALLOC& alloc = ALLOC()): std::vector<TYPE>(count, alloc) {}

#include "vectorex.h"
//#include "autoptr.h"
//#include <memory> //unique_ptr
int main()
{
#if 0
    typedef vector_ex<int, ShmAllocator<int>> vectype;
    static vectype& ids = //SHARED(SRCKEY, vectype, vectype);
        *(vectype*)ShmHeapAlloc::shmheap.alloc(SRCKEY, sizeof(vectype), 4 * sizeof(int), true, [] (void* shmaddr) { new (shmaddr) vectype; })
#else
//    ShmObj<vector_ex<int>, 0x5678feed, 4 * sizeof(int)> vect; //, __LINE__> vect;
//pool + "new" pattern from: https://stackoverflow.com/questions/20945439/using-operator-new-and-operator-delete-with-a-custom-memory-pool-allocator
//    ShmAllocator<vector_ex<int>> shmalloc;
//    unique_ptr<ShmObj<vector_ex<int>>> vectp = new (shmalloc) ShmObj<vector_ex<int>>();
    unique_ptr<ShmObj<vector_ex<int>>> vectp = new ShmObj<vector_ex<int>>();
#define vect  (*vectp)
//    ShmPtr<vector_ex<int>> vectp = new vector_ex<int>();
//    ShmObj<vector_ex<int>> vect;
#endif
    std::cout << "&vec " << FMT("%p") << &vect
        << FMT(", shmkey 0x%lx") << vect.allocator.shmkey()
        << ", sizeof(vect) " << sizeof(vect)
//        << ", extra " << vect.extra
        << ", #ents " << vect.size()
//        << ", srcline " << vect.srcline
        << "\n" << std::flush;
    for (int n = 0; n < 2; ++n) vect.emplace_back(100 + n);
    int ofs = vect.find(100);
    if (ofs == -1) throw std::runtime_error(RED_MSG "can't find entry" ENDCOLOR);
    vect.push_back(10);
    vect.push_back(12);
    std::cout << "sizeof(vect) " << sizeof(vect)
        << ", #ents " << vect.size() << "\n" << std::flush;
    vect.push_back(10);
    std::cout << "sizeof(vect) " << sizeof(vect)
        << ", #ents " << vect.size() << "\n" << std::flush;
//    std::unique_lock<std::mutex> lock(shmheap.mutex()); //low usage; reuse mutex
//    std::cout << "hello from " << __FILE__ << ":" << __LINE__ << "\n" << std::flush;
}
//#define main   mainx
#endif

#if 0
class Complex: public ShmHeapAlloc
{
public:
    Complex() {} //needed for custom new()
    Complex (double a, double b): r (a), c (b) {}
private:
    double r; // Real Part
    double c; // Complex Part
public:
};


#define NUM_LOOP  5 //5000
#define NUM_ENTS  5 //1000
int main(int argc, char* argv[]) 
{
    Complex* array[NUM_ENTS];
    std::cout << timestamp() << "start\n" << std::flush;
    for (int i = 0; i < NUM_LOOP; i++)
    {
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new(__FILE__, __LINE__ + j) Complex (i, j); //kludge: force unique key for shmalloc
        for (int j = 0; j < NUM_ENTS; j++) array[j] = new_SHM(+j) Complex (i, j); //kludge: force unique key for shmalloc
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new(shmheap.alloc(sizeof(Complex), __FILE__, __LINE__ + j) Complex (i, j); //kludge: force unique key for shmalloc
        for (int j = 0; j < NUM_ENTS; j++) delete array[j]; //FIFO
//        for (int j = 0; j < NUM_ENTS; j++) delete array[NUM_ENTS-1 - j]; //LIFO
//        for (int j = 0; j < NUM_ENTS; j++) array[j]->~Complex(); //FIFO
    }
    std::cout << timestamp() << "finish\n" << std::flush;
//    std::cout << "#alloc: " << Complex::nalloc << ", #free: " << Complex::nfree << "\n" << std::flush;
    return 0;
}
#endif


//eof
#if 0 //custom new + delete
    void* operator new(size_t size)
    {
        void* ptr = ::new Complex(); //CAUTION: force global new() here (else recursion)
//        void* ptr = malloc(size); //alternate
        ++nalloc;
        return ptr;
    }
    void operator delete(void* ptr)
    {
        ++nfree;
        free(ptr);
    }
    void* operator new (size_t size, const char* filename, int line)
    {
        void* ptr = new char[size];
        cout << "size = " << size << " filename = " << filename << " line = " << line << endl;
        return ptr;
    }
#endif
//    static int nalloc, nfree;
//int Complex::nalloc = 0;
//int Complex::nfree = 0;


#if 0
#include <iostream>

void* operator new(size_t size, const char* filename, int line)
{
    void* ptr = new char[size];
    ++Complex::nalloc;
    if (Complex::nalloc == 1)
        std::cout << "size = " << size << " filename = " << filename << " line = " << line << "\n" << std::flush;
    return ptr;
}
//tag alloc with src location:
//NOTE: cpp avoids recursion so macro names can match actual function names here
#define new  new(__FILE__, __LINE__)
#endif

#endif //def WANT_UNIT_TEST
#endif //0
//eof
