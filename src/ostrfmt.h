//printf-like formatter for c++ ostreams

#ifndef _OSTRMFMT_H
#define _OSTRMFMT_H

//custom stream manipulator for printf-style formating:
//idea from https://stackoverflow.com/questions/535444/custom-manipulator-for-c-iostream
//and https://stackoverflow.com/questions/11989374/floating-point-format-for-stdostream
//std::ostream& fmt(std::ostream& out, const char* str)

#include <sstream> //std::ostream
#include <stdio.h> //snprintf


class FMT
{
public: //ctor
    explicit FMT(const char* fmt): m_fmt(fmt) {}
private:
    class fmter //actual worker class
    {
    public:
        /*explicit*/ fmter(std::ostream& strm, const FMT& fmt): m_strm(strm), m_fmt(fmt.m_fmt) {}
//output next object (any type) to stream:
        template<typename TYPE>
        std::ostream& operator<<(const TYPE& value)
        {
//            return m_strm << "FMT(" << m_fmt << "," << value << ")";
            char buf[32]; //enlarge as needed
            int needlen = snprintf(buf, sizeof(buf), m_fmt, value);
//            buf[sizeof(buf) - 1] = '\0'; //make sure it's delimited (shouldn't be necessary)
//printf(" [fmt: len %d for '%s' too big? %d] ", needlen, buf, needlen >= sizeof(buf));
            if (needlen < sizeof(buf)) { m_strm << buf; return m_strm; } //fits ok
            char* bigger = new char[needlen + 1];
            snprintf(bigger, needlen + 1, m_fmt, value); //try again
//printf("fmt: len %d too big? %d\n", needlen, needlen >= sizeof(buf));
            m_strm << bigger;
            delete bigger;
            return m_strm;
        }
    private:
        std::ostream& m_strm;
        const char* m_fmt;
    };
    const char* m_fmt; //save fmt string for inner class
//kludge: return derived stream to allow operator overloading:
    friend FMT::fmter operator<<(std::ostream& strm, const FMT& fmt)
    {
        return FMT::fmter(strm, fmt);
    }
};

#endif //ndef _OSTRMFMT_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include "msgcolors.h"
#include "ostrfmt.h"


//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    int x;
    std::cout << BLUE_MSG << FMT("hex 0x%x") << 42 << ENDCOLOR_NEWLINE;
    std::cout << BLUE_MSG << FMT("str4 %.4s") << "abcdefgh" << ENDCOLOR_NEWLINE;
    std::cout << BLUE_MSG << FMT("ptr %p") << &x << ENDCOLOR_NEWLINE;
//    void* ptr = (void*)0x12345678;
//TODO    std::cout << BLUE_MSG << FMT("dec %d, hex %x, ptr %p") << (long)(int)ptr << (long)(int)ptr << ptr << ", etc" << ENDCOLOR_NEWLINE;
//    return 0;
}

#endif //def WANT_UNIT_TEST