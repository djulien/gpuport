//#!/bin/bash -x
//echo -e '\e[1;36m'; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -DUNIT_TEST -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
//#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)


//console colors

#ifndef _COLORS_H
#define _COLORS_H

#include "srcline.h" //SrcLine, shortsrc()

//ANSI color codes (for console output):
//https://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_COLOR(code)  "\x1b[" code "m"
#define RED_MSG  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_MSG  ANSI_COLOR("1;32")
#define YELLOW_MSG  ANSI_COLOR("1;33")
#define BLUE_MSG  ANSI_COLOR("1;34")
#define MAGENTA_MSG  ANSI_COLOR("1;35")
#define PINK_MSG  MAGENTA_MSG //easier to spell :)
#define CYAN_MSG  ANSI_COLOR("1;36")
#define GRAY_MSG  ANSI_COLOR("0;37")
#define ENDCOLOR_NOLINE  ANSI_COLOR("0")

//append the src line# to make debug easier:
//#define ENDCOLOR_ATLINE(srcline)  " &" TOSTR(srcline) ANSI_COLOR("0") "\n"
#define ENDCOLOR_ATLINE(srcline)  "  &" << shortsrc(srcline, SRCLINE) << ENDCOLOR_NOLINE "\n"
//#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%s) //%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(SRCLINE) //__LINE__)
//#define ENDCOLOR_LINE(line)  FMT(ENDCOLOR_MYLINE) << (line? line: __LINE__) //show caller line# if available

#endif //ndef _COLORS_H


#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include <iostream>
#include "srcline.h"
#include "msgcolors.h"

void func(int a, SrcLine srcline = 0)
{
    std::cout << BLUE_MSG << "hello " << a << " from" << ENDCOLOR;
    std::cout << CYAN_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
}


//int main(int argc, const char* argv[])
void unit_test()
{
    std::cout << BLUE_MSG << "start" << ENDCOLOR;
    func(1);
    func(2, SRCLINE);
//    return 0;
}

#endif //def WANT_UNIT_TEST