//RGB color-related defs and helpers:

#if !defined(_COLOR_HELPERS_H) && !defined(WANT_UNIT_TEST) //force unit test to explicitly #include this file
#define _COLOR_HELPERS_H //CAUTION: put this before defs to prevent loop on cyclic #includes

#include <map>


#ifndef pct
 #define pct(val)  rdiv(200 * val, 2) //NOTE: ridv used for int-only arithmetic (so it can be used as template arg); NOTE: don't enclose val in "()"; want 100* to occur first to delay round-off
#endif

#ifndef rdiv
 #define rdiv(num, den)  (((num) + (den) / 2) / (den))
#endif


#ifndef clamp
 #define clamp(val, min, max)  ((val) < (min)? (min): (val) > (max)? (max): (val))
#endif
//#define clip_255(val)  ((val) & 0xFF)


//accept variable #up to 4 macro args:
#ifndef UPTO_4ARGS
 #define UPTO_4ARGS(one, two, three, four, five, ...)  five
#endif
//#ifndef UPTO_3ARGS
// #define UPTO_3ARGS(one, two, three, four, ...)  four
//#endif


////////////////////////////////////////////////////////////////////////////////
////
/// Color handling:
//


//(A)RGB primary colors:
//NOTE: consts below are processor-dependent (not hard-coded for ARGB msb..lsb)
//internal FB_Color on RPi is ARGB
//use later macros to adjust in-memory representation based on processor endianness (RGBA vs. ABGR)
//#pragma message("Compiled for ARGB color format (hard-coded)")
#define RED  (Amask | Rmask) //0xFFFF0000 //fromRGB(255, 0, 0) //0xFFFF0000
#define GREEN  (Amask | Gmask) //0xFF00FF00 //fromRGB(0, 255, 0) //0xFF00FF00
#define BLUE  (Amask | Bmask) //0xFF0000FF //fromRGB(0, 0, 255) //0xFF0000FF
#define YELLOW  (RED | GREEN) //0xFFFFFF00
#define CYAN  (GREEN | BLUE) //0xFF00FFFF
#define MAGENTA  (RED | BLUE) //0xFFFF00FF
#define PINK  MAGENTA //easier to spell :)
#define BLACK  (RED & GREEN & BLUE) //0xFF000000 //NOTE: needs Alpha
#define WHITE  (RED | GREEN | BLUE) //fromRGB(255, 255, 255) //0xFFFFFFFF
//#define ALPHA  0xFF000000
//use low brightness to reduce eye burn during testing:
//#define BLACK 0
//#define RED  0x1f0000
//#define GREEN  0x001f00
//#define BLUE  0x00001f
//#define YELLOW  0x1f1f00
//#define CYAN  0x001f1f
//#define MAGENTA  0x1f001f
//#define WHITE  0x1f1f1f

//set in-memory byte order according to architecture of host processor:
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN //Intel (PCs)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ //Intel (PCs)
 #pragma message("big endian")
 #define Rmask  0xFF000000
 #define Gmask  0x00FF0000
 #define Bmask  0x0000FF00
 #define Amask  0x000000FF
//#define Abits(a)  (clamp(a, 0, 0xFF) << 24)
//#define Rbits(a)  (clamp(r, 0, 0xFF) << 16)
//#define Gbits(a)  (clamp(g, 0, 0xFF) << 8)
//#define Bbits(a)  clamp(b, 0, 0xFF)
// #define Amask(color)  ((color) & 0xFF000000)
// #define Rmask(color)  ((color) & 0x00FF0000)
// #define Gmask(color)  ((color) & 0x0000FF00)
// #define Bmask(color)  ((color) & 0x000000FF)
// #define A(color)  (((color) >> 24) & 0xFF)
// #define R(color)  (((color) >> 16) & 0xFF)
// #define G(color)  (((color) >> 8) & 0xFF)
// #define B(color)  ((color) & 0xFF)
// #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))

#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ //ARM (RPi)
 #pragma message("little endian")
 #define Rmask  0x00FF0000 //0x000000FF
 #define Gmask  0x0000FF00
 #define Bmask  0x000000FF //0x00FF0000
 #define Amask  0xFF000000
// #define Amask(color)  ((color) & 0xFF000000)
// #define Rmask(color)  ((color) & 0x00FF0000)
// #define Gmask(color)  ((color) & 0x0000FF00)
// #define Bmask(color)  ((color) & 0x000000FF)
// #define A(color)  (((color) >> 24) & 0xFF)
// #define R(color)  (((color) >> 16) & 0xFF)
// #define G(color)  (((color) >> 8) & 0xFF)
// #define B(color)  ((color) & 0xFF)
// #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))

#else
 #error message("Unknown endian")
#endif


//color manipulation:
//NOTE: these are mainly meant for use with compile-time consts (arithmetic performed once at compile time)
//could be used with vars also (hopefully compiler optimizes using byte instructions)
#define Rshift  (Rmask / 0xFF)
#define Gshift  (Gmask / 0xFF)
#define Bshift  (Bmask / 0xFF)
#define Ashift  (Amask / 0xFF)
#define fromRGB(r, g, b)  ((255 * Ashift) | (clamp(r, 0, 255) * Rshift) | (clamp(g, 0, 255) * Gshift) | (clamp(b, 0, 255) * Bshift))
#define fromARGB(a, r, g, b)  ((clamp(a, 0, 255) * Ashift) | (clamp(r, 0, 255) * Rshift) | (clamp(g, 0, 255) * Gshift) | (clamp(b, 0, 255) * Bshift))

#define A(color)  (((color) / Ashift) & 0xFF)
#define R(color)  (((color) / Rshift) & 0xFF)
#define G(color)  (((color) / Gshift) & 0xFF)
#define B(color)  (((color) / Bshift) & 0xFF)

#define Abits(color)  ((color) & Amask)
#define Rbits(color)  ((color) & Rmask)
#define Gbits(color)  ((color) & Gmask)
#define Bbits(color)  ((color) & Bmask)

//#define R_G_B(color)  R(color), G(color), B(color)
//#define A_R_G_B(color)  A(color), R(color), G(color), B(color)
#define R_G_B_A(color)  R(color), G(color), B(color), A(color)
//#define B_G_R(color)  B(color), G(color), R(color)
//#define A_B_G_R(color)  A(color), B(color), G(color), R(color)
//#define B_G_R_A(color)  B(color), G(color), R(color), A(color)


//dim/mix/blend values:
#define dim  mix_2ARGS
#define mix_2ARGS(dim, val)  mix_3ARGS(dim, val, 0)
#define mix_3ARGS(blend, val1, val2)  ((int)((val1) * (blend) + (val2) * (1 - (blend)))) //uses floating point
#define mix_4ARGS(num, den, val1, val2)  (((val1) * (num) + (val2) * (den - num)) / (den)) //use fractions to avoid floating point at compile time
#define mix(...)  UPTO_4ARGS(__VA_ARGS__, mix_4ARGS, mix_3ARGS, mix_2ARGS, mix_1ARG) (__VA_ARGS__)

#define mixARGB_2ARGS(dim, val)  mixARGB_3ARGS(dim, val, Abits(val)) //preserve Alpha
#define mixARGB_3ARGS(blend, c1, c2)  fromARGB(mix(blend, A(c1), A(c2)), mix(blend, R(c1), R(c2)), mix(blend, G(c1), G(c2)), mix(blend, B(c1), B(c2))) //uses floating point
#define mixARGB_4ARGS(num, den, c1, c2)  fromARGB(mix(num, den, A(c1), A(c2)), mix(num, den, R(c1), R(c2)), mix(num, den, G(c1), G(c2)), mix(num, den, B(c1), B(c2))) //use fractions to avoid floating point at compile time
#define mixARGB(...)  UPTO_4ARGS(__VA_ARGS__, mixARGB_4ARGS, mixARGB_3ARGS, mixARGB_2ARGS, mixARGB_1ARG) (__VA_ARGS__)
#define dimARGB  mixARGB_2ARGS

//#define R_G_B_A_masks(color)  Rmask(color), Gmask(color), Bmask(color), Amask(color)

//convert color ARGB <-> ABGR format:
//OpenGL seems to prefer ABGR format, but ARGB order is more readable (for me)
//convert back with same function & 0xffffff
//TODO: drop alpha setting?
//??	if (!Amask(color) /*&& (color & 0xffffff)*/) color |= 0xff000000; //RGB present but no alpha; add full alpha to force color to show
#define ARGB2ABGR(color)  ((color) & (Amask | Gmask) | (R(color) * Bshift) | (B(color) * Rshift)) //swap R <-> B
//#define SWAP32(uint32)  ((Amask(uint32) >> 24) | (Rmask(uint32) >> 8) | (Gmask(uint32) << 8) | (Bmask(uint32) << 24))


//limit brightness:
//NOTE: A bits are dropped/ignored
template<int MAXBRIGHT = pct(50/60), typename COLOR = uint32_t> //83% //= 3 * 212, //0xD4D4D4, //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA
COLOR limit(COLOR color)
{
    /*using*/ static const int BRIGHTEST = 3 * 255 * MAXBRIGHT / 100;
    if (!MAXBRIGHT || (MAXBRIGHT >= 100)) return color; //R(BRIGHTEST) + G(BRIGHTEST) + B(BRIGHTEST) >= 3 * 255)) return color; //no limit
//#pragma message "limiting R+G+B brightness to " TOSTR(LIMIT_BRIGHTNESS)
    unsigned int r = R(color), g = G(color), b = B(color);
    unsigned int sum = r + g + b; //max = 3 * 255 = 765
    if (sum <= BRIGHTEST) return color;
//reduce brightness, try to preserve relative colors:
    r = rdiv(r * BRIGHTEST, sum);
    g = rdiv(g * BRIGHTEST, sum);
    b = rdiv(b * BRIGHTEST, sum);
    color = /*Abits(color) |*/ (r * Rshift) | (g * Gshift) | (b * Bshift); //| Rmask(r) | Gmask(g) | Bmask(b);
//printf("REDUCE: 0x%x, sum %d, R %d, G %d, B %d => r %d, g %d, b %d, 0x%x\n", sv, sum, R(sv), G(sv), B(sv), r, g, b, color);
    return color;
}

//const uint32_t PALETTE[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};

//readable names (mainly for debug msgs):
const std::map<uint32_t, const char*> ColorNames =
{
    {RED, "Red"},
    {GREEN, "Green"},
    {BLUE, "Blue"},
    {YELLOW, "Yellow"},
    {CYAN, "Cyan"},
    {MAGENTA, "Pink/Magenta"},
    {WHITE, "White"},
    {BLACK, "Black"},
};

#endif //ndef _COLOR_HELPERS_H


////////////////////////////////////////////////////////////////////////////////
////
/// unit test:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

//#include "debugexc.h"
#include "logging.h"
#include "str-helpers.h"

#include "rgb-helpers.h"


// application entry point
//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    debug(0, "red 0x%x, green 0x%x, blue 0x%x", RED, GREEN, BLUE);
    debug(0, "black 0x%x, white 0x%x, limit white 0x%x", BLACK, WHITE, limit<>(WHITE));
    debug(0, "75%% 256 = 0x" << std::hex << dim(0.75, 256) << ", 25%% 256 0x" << dim(0.25, 256) << std::dec);
    debug(0, "75%% white = 0x" << std::hex << dimARGB(0.75, WHITE) << ", 25%% white 0x" << dimARGB(0.25, WHITE) << std::dec);

    debug(0, "done");
//    return 0; 
}

#endif //def WANT_UNIT_TEST

//eof