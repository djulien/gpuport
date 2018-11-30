#ifndef _SRCLINE_H
#define _SRCLINE_H


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
#define SRCLINE  _.srcline = __FILE__ ":" TOSTR(__LINE__)
typedef const char* SrcLine; //allow compiler to distinguish param types, catch implicit conv
//    friend ostream& operator<<(ostream& os, const Date& dt);  
//std::ostream& operator<<(std::ostream& ostrm, SrcLine srcline) { ostrm << static_cast<const char*>(srcline); return ostrm; }
//std::ostream& operator<<(std::ostream& ostrm, const char* srcline) { ostrm << "srcline"; return ostrm; }
struct { SrcLine srcline; } _; //kludge: global destination so SRCLINE can be used outside NAMED; NOTE: name must match NAMED var name


//struct UniqParams { const char* folder = 0; const char* basename = 0; const char* ext = 0; SrcLine srcline = SRCLINE; };
//#ifndef NAMED
// #define NAMED  SRCLINE, [&](auto& _)
//#endif

#ifndef SIZEOF
 #define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))
#endif


#include <dirent.h> //opendir(), readdir(), closedir()

//smart ptr wrapper for DIR:
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


static bool isunique(const char* folder, const char* basename, const char* ext)
//static bool isunique(SrcLine mySrcLine = 0, void (*get_params)(struct UniqParams&) = 0)
{
//    /*static*/ struct UniqParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//    if (mySrcLine) params.srcline = mySrcLine;
//    if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//    static std::map<const char*, const char*> exts;
    thread_local static struct { char name[32]; int count; } cached[10] = {0}; //base file name + #matching files
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


#include <stdio.h> //snprintf()
#include <atomic> //std::atomic<>

//shorten src file name:
//#define _GNU_SOURCE //select GNU version of basename()
//#include <stdlib.h> //atoi()
SrcLine shortsrc(SrcLine srcline, SrcLine defline) //int line = 0)
{
//NO-needs to be thread-safe:    static char buf[60]; //static to preserve after return to caller
    thread_local static std::atomic<int> ff;
    thread_local static char buf_pool[4][60]; //static to preserve after return to caller; kuldge: use buf pool in lieu of mem mgmt of ret bufs
    char* buf = buf_pool[ff++ % SIZEOF(buf_pool)];
    int toolong;

    const char* defbp = 0;
    if (!defline) defline = SRCLINE; //need a value here
    if (srcline == defline) srcline = 0;
    if (srcline && ((toolong = strlen(srcline)) > 50)) srcline += toolong - 50;
    if (defline && ((toolong = strlen(defline)) > 50)) defline += toolong - 50;
    if (srcline) //show default also (easier debug/trace)
    {
        defbp = strrchr(defline, '/');
        if (defbp) defline = defbp + 1; //drop parent folder name
        defbp = strrchr(defline, ':');
        if (!defbp) defbp = ":#?"; //endp + strlen(endp);
    }
    if (!srcline) srcline = defline;
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
    snprintf(buf, sizeof(buf_pool[0]), "%.*s%s%s", (int)(extp - srcline), srcline, bp, defbp? defbp: ""); //line);
    return buf;
}


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


template<typename ARG1, typename ARG2>
class X
{
public:
    X(SrcLine srcline = 0) { std::cout << BLUE_MSG "hello from X<" << TEMPL_ARGS << ">" ENDCOLOR_ATLINE(srcline) "\n"; }
    ~X() {}
};


//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    std::cout << BLUE_MSG "start" ENDCOLOR "\n";
    func(1);
    func(2, SRCLINE);
    X<int, const char*> aa(SRCLINE);
    X<long, long> bb(SRCLINE);
//    return 0;
}

#endif //def WANT_UNIT_TEST
