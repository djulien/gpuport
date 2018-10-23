////////////////////////////////////////////////////////////////////////////////
////
/// RPi helpers (runnable on non-RPi machines):
//

#ifndef _RPI_HELPERS_H
#define _RPI_HELPERS_H

//NOTE from https://stackoverflow.com/questions/1277627/overhead-of-pthread-mutexes
//about atomic: In practice, you can assume that int and other integer types no longer than int are atomic. You can also assume that pointer types are atomic
// http://axisofeval.blogspot.com/2010/11/numbers-everybody-should-know.html
//go ahead and use std::atomic<> anyway, for safety:
#include <atomic> //std::atomic. std::memory_order
#include <sys/stat.h>


enum class tristate: int {No = false, Yes = true, Maybe, Error = Maybe};

//check for file existence:
bool exists(const char* path)
{
    struct stat info;
    return !stat(path, &info); //file exists
}


//check for RPi:
//NOTE: results are cached (outcome won't change until reboot)
bool isRPi()
{
//NOTE: mutex not needed here
//main thread will call first, so race conditions won't occur (benign anyway)
    static std::atomic<tristate> isrpi(tristate::Maybe);
//    static std::mutex protect;
//    std::lock_guard<std::mutex> lock(protect); //not really needed (low freq api), but just in case

//    myprintf(3, BLUE_MSG "isRPi()" ENDCOLOR);
//    serialize.lock(); //not really needed (low freq api), but just in case
    if (isrpi == tristate::Maybe) isrpi = exists("/boot/config.txt")? tristate::Yes: tristate::No;
//    serialize.unlock();
    return (isrpi == tristate::Yes);
}


#endif //ndef _RPI_HELPERS_H



///////////////////////////////////////////////////////////////////////////////
////
/// Unit test
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include "msgcolors.h"
#include "debugexc.h"


//int main(int argc, const char* argv[])
void unit_test()
{
    debug(CYAN_MSG "is RPi? %d" ENDCOLOR, isRPi());
//    return 0;
}

#endif //def WANT_UNIT_TEST
