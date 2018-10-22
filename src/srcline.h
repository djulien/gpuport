#ifndef _SRCLINE_H
#define _SRCLINE_H


//smart ptr wrapper for DIR:
#include <dirent.h> //opendir(), readdir(), closedir()
class Dir
{
public: //ctor/dtor
    Dir(const char* dirname): m_ptr(opendir(dirname)) {}
    ~Dir() { if (m_ptr) closedir(m_ptr); }
public: //operators
    operator bool() { return m_ptr; }
public: //methods:
    struct dirent* next() { return m_ptr? readdir(m_ptr): nullptr; }
private: //data
    DIR* m_ptr;
};


//check for ambiguous base file name:
#include <string.h>
#include <algorithm> //std::min()
#include <string>
//#include <ostream> //std::ostream
//#include <memory> //std::unique_ptr<>
//#include <stdio.h> 

//macro expansion helpers:
#ifndef TOSTR
 #define TOSTR(str)  TOSTR_NESTED(str)
 #define TOSTR_NESTED(str)  #str
#endif

//typedef struct { int line; } SRCLINE; //allow compiler to distinguish param types, prevent implicit conversion
//typedef int SRCLINE;
#define SRCLINE  _sl.srcline = __FILE__ ":" TOSTR(__LINE__)
typedef const char* SrcLine; //allow compiler to distinguish param types, catch implicit conv
//    friend ostream& operator<<(ostream& os, const Date& dt);  
//std::ostream& operator<<(std::ostream& ostrm, SrcLine srcline) { ostrm << static_cast<const char*>(srcline); return ostrm; }
//std::ostream& operator<<(std::ostream& ostrm, const char* srcline) { ostrm << "srcline"; return ostrm; }
struct { SrcLine srcline; } _sl; //kludge: global destination so SRCLINE can be used outside NAMED


//struct UniqParams { const char* folder = 0; const char* basename = 0; const char* ext = 0; SrcLine srcline = SRCLINE; };
//#ifndef NAMED
// #define NAMED  SRCLINE, [&](auto& _)
//#endif

#ifndef SIZEOF
 #define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))
#endif


static bool isunique(const char* folder, const char* basename, const char* ext)
//static bool isunique(SrcLine mySrcLine = 0, void (*get_params)(struct UniqParams&) = 0)
{
//    /*static*/ struct UniqParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//    if (mySrcLine) params.srcline = mySrcLine;
//    if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//    static std::map<const char*, const char*> exts;
    static struct { char name[32]; int count; } cached[10] = {0}; //base file name + #matching files
    const auto cachend /*const*/ = cached + SIZEOF(cached);
    if (!ext) return false; //don't check if no extension
    size_t baselen = std::min<size_t>(ext - basename + 1, sizeof(cached->name)); //ext - basename;

//    if (baselen <= sizeof(cached[0].name))
//first check filename cache:
    auto cacheptr = cached; //NOTE: need initializer so compiler can deduce type
    for (/*auto*/ cacheptr = cached; (cacheptr < cachend /*cached + SIZEOF(cached)*/) && cacheptr->name[0]; ++cacheptr)
//    while ((++cacheptr < cached + SIZEOF(cached)) && cacheptr->name[0])
        if (!strncmp(basename, cacheptr->name, baselen)) return (cacheptr->count == 1);
//        else std::cout << "cmp[" << (cacheptr - cached) << "] '" << cacheptr->name << "' vs. " << baselen << ":'" << basename << "'? " << strncmp(basename, cacheptr->name, baselen) << "\n" << std::flush;
//        else if (!cacheptr->name[0]) nextptr = cacheptr;
//next go out and count (could be expensive):
//std::cout << "dir len " << (basename - folder - 1) << ", base len " << (ext - basename) << "\n" << std::flush;
    std::string dirname(folder, (basename != folder)? basename - folder - 1: 0);
//    std::unique_ptr<DIR> dir(opendir(dirname.length()? dirname.c_str(): "."), [](DIR* dirp) { closedir(dirp); });
    Dir dir(dirname.length()? dirname.c_str(): ".");
    if (!dir) return false;
    int count = 0;
    struct dirent* direntp;
    while (direntp = dir.next())
        if (direntp->d_type == DT_REG)
        {
            const char* bp = strrchr(direntp->d_name, '.');
            if (!bp) bp = direntp->d_name + strlen(direntp->d_name);
//            if (bp - direntp->d_name != baselen) continue;
            if (strncmp(direntp->d_name, basename, baselen)) continue;
//std::cout << direntp->d_name << " matches " << baselen << ":" << basename << "\n" << std::flush;
            ++count;
        }
//if there's enough room, cache results for later:
    if (cacheptr < cachend /*cached + SIZEOF(cached)*/) { strncpy(cacheptr->name, basename, baselen); cacheptr->count = count; }
//    std::cout << "atline::isunique('" << folder << "', '" << basename << "', '" << ext << "') = [" << (cacheptr - cached) << "] '" << cacheptr->name << "', " << count << "\n" << std::flush;
    return (count == 1);
}


//shorten src file name:
#include <stdio.h> //snprintf()
#include <atomic> //std::atomic<>

//#define _GNU_SOURCE //select GNU version of basename()
//#include <stdlib.h> //atoi()
SrcLine shortsrc(SrcLine srcline, SrcLine defline) //int line = 0)
{
//NO-needs to be thread-safe:    static char buf[60]; //static to preserve after return to caller
    static std::atomic<int> ff;
    static char buf_pool[4][60]; //static to preserve after return to caller; kuldge: use buf pool in lieu of mem mgmt of ret bufs
    char* buf = buf_pool[ff++ % SIZEOF(buf_pool)];
    if (!srcline) srcline = defline;
    if (!srcline) srcline = SRCLINE;
//std::cout << "raw file " << srcline << "\n" << std::flush;
    const char* svsrc = srcline; //dirname = strdup(srcline);

    const char* bp = strrchr(srcline, '/');
    if (bp) srcline = bp + 1; //drop parent folder name
//    const char* endp = strrchr(srcline, '.'); //drop extension
//    if (!endp) endp = srcline + strlen(srcline);
//    if (!line)
//    {
        bp = strrchr(srcline, ':');
        if (!bp) bp = ":#?"; //endp + strlen(endp);
//        if (bp) line = atoi(bp+1);
//    }
    const char* extp = strrchr(srcline, '.');
    if (!extp || !isunique(svsrc, srcline, extp)) extp = srcline + std::min<size_t>(bp - srcline, strlen(srcline)); //drop extension if unambiguous
//    if (!extp || !isunique([svrc,srcline,extp](auto& _) { _.folder = svsrc; _.basename = srcline; _.ext = extp; }) extp = srcline + std::min<size_t>(bp - srcline, strlen(srcline)); //drop extension if unambiguous
    snprintf(buf, sizeof(buf_pool[0]), "%.*s%s", (int)(extp - srcline), srcline, bp); //line);
    return buf;
}


#if 0
class SRCLINE
{
    int m_line;
public: //ctor/dtor
    explicit SRCLINE(int line): m_line(line) {}
public: //opeartors
    inline operator bool() { return m_line != 0; }
//    static SRCLINE FromInt(int line)
//    {
//        SRCLINE retval;
//        retval.m_line = line;
//        return retval;
//    }
};
#endif

#endif //ndef _SRCLINE_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include <iostream>
#include "msgcolors.h"
#include "srcline.h"

void func(int a, SrcLine srcline = 0)
{
    std::cout << PINK_MSG << "hello " << a << " from" << ENDCOLOR "\n";
    std::cout << PINK_MSG "hello " << a << " from" ENDCOLOR "\n";
    std::cout << RED_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline) << std::endl;
    std::cout << RED_MSG "hello " << a << " from" ENDCOLOR_ATLINE(srcline) << std::endl;
}


//int main(int argc, const char* argv[])
void unit_test()
{
    std::cout << BLUE_MSG "start" ENDCOLOR "\n";
    func(1);
    func(2, SRCLINE);
//    return 0;
}

#endif //def WANT_UNIT_TEST