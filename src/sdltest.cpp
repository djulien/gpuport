
#include <iostream> //cout

#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
#include <SDL_endian.h> //SDL_BYTE_ORDER


#ifndef WANT_UNIT_TEST
//#undef WANT_UNIT_TEST //prevent recursion

void unit_test()
{
    SDL_version ver;
    SDL_GetVersion(&ver);
    std::cout << "SDL_Lib {version " << (int)ver.major << "." << (int)ver.minor << "." << (int)ver.patch << std::endl;
}

#endif //def WANT_UNIT_TEST

#ifndef __SRCFILE__
int main() { unit_test(); }
#endif //ndef __SRCFILE__

//eof
