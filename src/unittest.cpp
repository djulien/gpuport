#!/bin/bash
#ANSI color codes (console colors): https://en.wikipedia.org/wiki/ANSI_escape_code
#bash string replacement: https://stackoverflow.com/questions/13043344/search-and-replace-in-bash-using-regular-expressions?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
ANSI_COLOR='\e[COLORm'
RED=${ANSI_COLOR//COLOR/1;31}  #too dark: '\e[0;31m' #`tput setaf 1`
GREEN=${ANSI_COLOR//COLOR/1;32} #`tput setaf 2`
#YELLOW=${ANSI_COLOR//COLOR/1;33} #`tput setaf 3`
#BLUE=${ANSI_COLOR//COLOR/1;34} #`tput setaf 4`
PINK=${ANSI_COLOR//COLOR/1;35} #`tput setaf 5`
CYAN=${ANSI_COLOR//COLOR/1;36} #`tput setaf 6`
#GRAY=${ANSI_COLOR//COLOR/0;37}
ENDCOLOR=${ANSI_COLOR//COLOR/0} #`tput sgr0`
TOTEST=$*
STARTTIME=$(date +%s)
if [ "x$TOTEST" == "x" ]; then TOTEST="?? no files ??"; fi
echo -e "${PINK}Unit testing '${TOTEST}' ...${ENDCOLOR}"
OPT=3  #3 = max; 0 = none
#CFLAGS="-fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14"
CFLAGS="`sdl2-config --cflags` -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -x c++"
CLIBS="`sdl2-config --libs`"
BIN="${BASH_SOURCE%.*}"
rm $BIN 2> /dev/null
set -x
#echo -e $CYAN; OPT=3; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" $CFLAGS -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e $ENDCOLOR
#echo -e $CYAN; g++ -DWANT_UNIT_TEST $CFLAGS -o "${BASH_SOURCE%.*}" -x c++ $TOTEST; echo -e $ENDCOLOR
echo -e $CYAN; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -D__TEST_FILE__="\"${TOTEST}\"" $CFLAGS - $CLIBS -o "${BASH_SOURCE%.*}" <<//EOF; echo -e $ENDCOLOR
#line 28 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)

#include <iostream> //std::cout, std::endl, std::flush

#include __TEST_FILE__ //include first time without unit test
#define WANT_UNIT_TEST
#include __TEST_FILE__ //include unit test second time

#ifndef MSG
 #define MSG(msg)  { std::cout << msg << std::endl << std::flush; }
#endif

int main(int argc, const char* argv[])
{
    MSG("testing " __TEST_FILE__ " ...");
    unit_test();
    return 0;
}
//EOF
set +x
#if [ $? -eq 0 ]; then
ENDTIME=$(date +%s)
ELAPSED="$((${ENDTIME} - ${STARTTIME})) sec" #https://stackoverflow.com/questions/16908084/bash-script-to-calculate-time-elapsed
if [ -f $BIN ]; then
    echo -e "${GREEN}compiled OK after ${ELAPSED}, now run it ...${ENDCOLOR}"
    ${BASH_SOURCE%.*}
    echo -e "${GREEN}to re-run: ${BASH_SOURCE%.*}${ENDCOLOR}"
else
    echo -e "${RED}compile FAILED after ${ELAPSED}${ENDCOLOR}"
fi
#eof