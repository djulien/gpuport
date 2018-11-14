//Node.js GpuPort add-on (wrap GPU as a parallel port)

//example/setup info:
//** https://github.com/nodejs/node-addon-examples
//https://github.com/1995parham/Napi101
//https://www.nearform.com/blog/the-future-of-native-amodules-in-node-js/
//https://hackernoon.com/n-api-and-getting-started-with-writing-c-addons-for-node-js-cf061b3eae75
//or https://github.com/nodejs/node-addon-examples/tree/master/1_hello_world
//or https://medium.com/@atulanand94/beginners-guide-to-writing-nodejs-addons-using-c-and-n-api-node-addon-api-9b3b718a9a7f
//https://github.com/master-atul/blog-addons-example

//npm install -g node-gyp  ??
//NOTE: had to manually "npm install nan@latest --save"

//to build:
//npm rebuild  -or-  npm run build


//#include <nan.h> //older V8 api
#include <node_api.h> //C style api
//#include <src/node_api_types.h> //kludge: not sure why this needs to be explicitly included
//typedef int napi_callback_scope;
//typedef int napi_open_callback_scope;
//typedef int napi_close_callback_scope;
//BROKEN: 
//#include "napi.h" //C++ style api; #includes node_api.h; WHERE IS napi_callback_scope?
//#ifndef SRC_NAPI_H_ //kludge: avoid rewriting code below
// namespace Napi
// {
//     using String = napi_value;
// };
//#endif

#define NO_ERRCODE  NULL


////////////////////////////////////////////////////////////////////////////////
////
/// example code
//

#include <string>

std::string hello()
{
      return "Hello World";
}
#ifdef SRC_NAPI_H_
 Napi::String Hello_wrapped(const Napi::CallbackInfo& info)
 {
    Napi::Env env = info.Env();
    Napi::String returnValue = Napi::String::New(env, hello());
    return returnValue;
 }
#elif defined(SRC_NODE_API_H_)
 napi_value Hello_wrapped(napi_env env, napi_callback_info info)
 {
    napi_status status;
    napi_value retval;
    status = napi_create_string_utf8(env, hello().c_str(), hello().length(), &retval);
    if (status != napi_ok) napi_throw_error(env, NULL, "Failed to parse arguments");
    return retval;
 }
#else
 #error which api?
#endif


#ifdef SRC_NAPI_H_
 Napi::Number MyFunction_wrapped(const Napi::CallbackInfo& info)
 {
    Napi::Env env = info.Env();
    if (info.Length() < 1) Napi::TypeError::New(env, "number expected").ThrowAsJavaScriptException();
    int number = info[0].As<Napi::Number>().Int32Value();
    number = number * 2;
    Napi::Number myNumber = Napi::Number::New(env, number);
    return myNumber;
 }
#elif defined(SRC_NODE_API_H_)
 napi_value MyFunction_wrapped(napi_env env, napi_callback_info info)
 {
    napi_status status;

    size_t argc = 1;
    napi_value argv[1];
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) napi_throw_error(env, NULL, "Failed to parse arguments");
//    if (argc < 1) ...

//    char str[1024];
//    size_t str_len;
//    status = napi_get_value_string_utf8(env, argv[0], (char *) &str, 1024, &str_len);
//    if (status != napi_ok) { napi_throw_error(env, "EINVAL", "Expected string"); return NULL; }
//    Napi::String str = Napi::String::New(env, )

    int number = 0;
    status = napi_get_value_int32(env, argv[0], &number);
    if (status != napi_ok) napi_throw_type_error(env, NO_ERRCODE, "Invalid number was passed as argument");
    napi_value myNumber; //= 2 * number;
//    napi_value retval;
    status = napi_create_int32(env, 2 * number, &myNumber);
    if (status != napi_ok) napi_throw_error(env, NULL, "Unable to create return value");
// ...
    return myNumber;
 }
#endif


#ifdef SRC_NAPI_H_
 Napi::Object Init(Napi::Env env, Napi::Object exports)
 {
    exports.Set(Napi::String::New(env, "my_function"), Napi::Function::New(env, MyFunction_wrapped));
    exports.Set(Napi::String::New(env, "hello"), Napi::Function::New(env, Hello_wrapped));
    return exports;
 }
 NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
#elif defined(SRC_NODE_API_H_)
 napi_value Init(napi_env env, napi_value exports)
 {
    napi_status status;
    napi_value fn;

    // Arguments 2 and 3 are function name and length respectively
    // We will leave them as empty for this example
    status = napi_create_function(env, NULL, 0, MyFunction_wrapped, NULL, &fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to wrap native function");
    status = napi_set_named_property(env, exports, "my_function", fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to populate exports");

    status = napi_create_function(env, NULL, 0, Hello_wrapped, NULL, &fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to wrap native function");
    status = napi_set_named_property(env, exports, "hello", fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to populate exports");
    return exports;
 }
// NODE_API_MODULE(testaddon, Init)
// NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
 NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
#endif


////////////////////////////////////////////////////////////////////////////////
////
/// GpuPort
//

//This is a Node.js add-on to display a rectangular grid of pixels (a texture) on screen using SDL2 and hardware acceleration (via GPU).
//In essence, the RPi GPU provides a 24-bit high-speed parallel port with precision timing.
//Optionally, OpenGL and GLSL shaders can be used for generating effects.
//In dev mode, an SDL window is used.  In live mode, full screen is used (must be configured for desired resolution).
//Screen columns generate data signal for each bit of the 24-bit parallel port.
//Without external mux, there can be 24 "universes" of external LEDs to be controlled.  Screen height defines max universe length.

//Copyright (c) 2015, 2016, 2017, 2018 Don Julien, djulien@thejuliens.net


////////////////////////////////////////////////////////////////////////////////
////
/// Setup
//

//1. Dependencies:
//1a. install nvm
//1b. install node v10.12.0  #or later; older versions might work; ymmv
//1c. install github client
//1d. install SDL2 >=2.0.8 from libsdl.org; install from source recommended (repo versions tend to be older)
//2. Create parent project
//2a. create a folder + cd into it
//2b. npm init
//2c. npm install --save gpuport
//OR
//2a. git clone this repo
//2b. cd this folder
//2c. npm install
//2d. npm test  #dev mode test, optional

//initial construction:
//N-api example at: //https://medium.com/@atulanand94/beginners-guide-to-writing-nodejs-addons-using-c-and-n-api-node-addon-api-9b3b718a9a7f
//npm install node-gyp, node-addon-api


//1d. upgrade node-gyp >= 3.6.2
//     [sudo] npm explore npm -g -- npm install node-gyp@latest
//3. config:
//3a. edit /boot/config.txt, vdi overlay + screen res, disable_overscan=1 ?
#if 0
overscan_left=24
overscan_right=24
Overscan_top=10
Overscan_bottom=24

Framebuffer_width=480
Framebuffer_height=320

Sdtv_mode=2
Sdtv_aspect=2

resolution 82   1920x1080   60Hz    1080p

hdmi_ignore_edid=0xa5000080
hdmi_force_hotplug=1
hdmi_boost=7
hdmi_group=2
hdmi_mode=82
hdmi_drive=1
#endif
//3b. give RPi GPU 256 MB RAM (optional)


//WS281X notes:
//30 usec = 33.3 KHz node rate
//1.25 usec = 800 KHz bit rate; x3 = 2.4 MHz data rate => .417 usec
//AC SSRs:
//120 Hz = 8.3 msec; x256 ~= 32.5 usec (close enough to 30 usec); OR x200 = .0417 usec == 10x WS281X data rate
//~ 1 phase angle dimming time slot per WS281X node
//invert output
//2.7 Mbps serial date rate = SPBRG 2+1
//8+1+1 bits = 3x WS281X data rate; 3 bytes/WS281X node
//10 serial bits compressed into 8 WS281X data bits => 2/3 reliable, 1/3 unreliable bits
//5 serial bits => SPBRG 5+1, 1.35 Mbps; okay since need to encode anyway?


////////////////////////////////////////////////////////////////////////////////
////
/// Headers, general macros, inline functions
//

#if 0
//#include <iostream> //std::cout
#include <SDL.h> //<SDL2/SDL.h> //CAUTION: must #include before other SDL or GL header files
#include <sstream> //std::ostringstream

#include "debugexc.h"
#include "msgcolors.h"
#include "elapsed.h" //timestamp()
#include "GpuPort.h"
#endif

//#define LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA; safely allows 300 LEDs per 20A at "full" (83%) white

//#define rdiv(n, d)  int(((n) + ((d) >> 1)) / (d))
//#define divup(n, d)  int(((n) + (d) - 1) / (d))

//#define SIZE(thing)  int(sizeof(thing) / sizeof((thing)[0]))

//#define return_void(expr) { expr; return; } //kludge: avoid warnings about returning void value

//inline int toint(void* val) { return (int)(long)val; }
//#define toint(ptr)  reinterpret_cast<int>(ptr) //gives "loses precision" warning/error
//#define toint(expr)  (int)(long)(expr)

//#define CONST  //should be const but function signatures aren't defined that way

//typedef enum {No = false, Yes = true, Maybe} tristate;
//enum class tristate: int {No = false, Yes = true, Maybe, Error = Maybe};

//#define uint24_t  uint32_t //kludge: use pre-defined type and just ignore first byte

//kludge: need nested macros to stringize correctly:
//https://stackoverflow.com/questions/2849832/c-c-line-number
//#define TOSTR(str)  TOSTR_NESTED(str)
//#define TOSTR_NESTED(str)  #str

//#define CONCAT(first, second)  CONCAT_NESTED(first, second)
//#define CONCAT_NESTED(first, second)  first ## second


#if 0
//show GpuPort stats:
template <typename GPTYPE>
void gpstats(const char* desc, GPTYPE&& gp, int often = 1)
{
    bool result = gp.ready(0xffffff, SRCLINE); //mark all univ ready to display
    static int count = 0;
    if (++count % often) return;
//see example at https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/
    for (;;)
    {
        /*uint32_t*/ auto numfr = gp.frinfo.numfr.load();
//        /*uint32_t*/ auto next_time = gp.frinfo.nexttime.load();
        double next_time = numfr * gp.frame_time;
        /*uint64_t*/ double times[SIZEOF(gp.frinfo.times)];
        for (int i = 0; i < SIZEOF(times); ++i) times[i] = gp.frinfo.times[i] / numfr / 1e3; //avg msec
        if (gp.frinfo.numfr.load() != numfr) continue; //invalid (inconsistent) results; try again
//CAUTION: use atomic ops; std::forward doesn't allow atomic vars (deleted function errors for copy ctor) so use load()
        debug(CYAN_MSG << timestamp() << "%s, next fr# %d, next time %f, ready? %d, avg perf: [%4.3f s, %4.3f ms, %4.3f s, %4.3f ms, %4.3f ms, %4.3f ms]" ENDCOLOR,
            desc, numfr, next_time, result, times[0] / 1e3, times[1], times[2], times[3], times[4], times[5]);
        break; //got valid results
    }
}


//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    Uint32 tb1 = 0xff80ff, tb2 = 0xffddbb;
    debug(BLUE_MSG "limit blue 0x%x, cyan 0x%x, white 0x%x, 0x%x -> 0x%x, 0x%x -> 0x%x" ENDCOLOR, GpuPort<>::limit(BLUE), GpuPort<>::limit(CYAN), GpuPort<>::limit(WHITE), tb1, GpuPort<>::limit(tb1), tb2, GpuPort<>::limit(tb2));

//    bind_test(); return;
    int screen = 0, vgroup = 0, delay_msec = 100; //default
//    for (auto& arg : args) //int i = 0; i < args.size(); ++i)
    /*GpuPort<>::Protocol*/ int protocol = GpuPort<>::Protocol::NONE;
    for (auto it = ++args.begin(); it != args.end(); ++it)
    {
        auto& arg = *it;
        if (!arg.find("-s")) { screen = atoi(arg.substr(2).c_str()); continue; }
        if (!arg.find("-g")) { vgroup = atoi(arg.substr(2).c_str()); continue; }
        if (!arg.find("-p")) { protocol = atoi(arg.substr(2).c_str()); continue; }
        if (!arg.find("-d")) { delay_msec = atoi(arg.substr(2).c_str()); continue; }
        debug(YELLOW_MSG "what is arg[%d] '%s'?" ENDCOLOR, /*&arg*/ it - args.begin(), arg.c_str());
    }
    const auto* scrn = getScreenConfig(screen, SRCLINE);
    if (!vgroup) vgroup = scrn->vdisplay / 5;
//    const int NUM_UNIV = 4, UNIV_LEN = 5; //24, 1111
//    const int NUM_UNIV = 30, UNIV_LEN = 100; //24, 1111
//    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //convert at compile time for faster run-time loops
    const Uint32 palette[] = {RED, GREEN, BLUE, YELLOW, CYAN, PINK, WHITE}; //, dimARGB(0.75, WHITE), dimARGB(0.25, WHITE)};

//    SDL_Size wh(4, 4);
//    GpuPort<NUM_UNIV, UNIV_LEN/*, raw WS281X*/> gp(2.4 MHz, 0, SRCLINE);
    GpuPort<> gp/* = GpuPort<>::factory*/(NAMED{ _.screen = screen; _.vgroup = vgroup; /*_.wh = wh*/; _.protocol = static_cast<GpuPort<>::Protocol>(protocol) /*GpuPort<>::NONE WS281X*/; _.init_color = dimARGB(0.25, RED); SRCLINE; }); //NAMED{ .num_univ = NUM_UNIV, _.univ_len = UNIV_LEN, SRCLINE});
//    Uint32* pixels = gp.Shmbuf();
//    Uint32 myPixels[H][W]; //TODO: [W][H]; //NOTE: put nodes in same universe adjacent for better cache performance
//    auto& pixels = gp.pixels; //NOTE: this is shared memory, so any process can update it
//    debug(CYAN_MSG "&nodes[0] %p, &nodes[0][0] = %p, sizeof gp %zu, sizeof nodes %zu" ENDCOLOR, &gp.nodes[0], &gp.nodes[0][0], sizeof(gp), sizeof(gp.nodes));
    debug(CYAN_MSG << gp << ENDCOLOR);
    SDL_Delay((5-1) sec);

//    void* addr = &gp.nodes[0][0];
//    debug(YELLOW_MSG "&node[0][0] " << addr << ENDCOLOR);
//    addr = &gp.nodes[1][0];
//    debug(YELLOW_MSG "&node[1][0] " << addr << ENDCOLOR);
//    addr = &gp.nodes[0][gp.UNIV_LEN];
//    debug(YELLOW_MSG "&node[0][%d] " << addr << ENDCOLOR, gp.UNIV_LEN);
//    addr = &gp.nodes[24][0];
//    debug(YELLOW_MSG "&node[24][0] " << addr << ENDCOLOR);

    gp.clear_stats(); //frinfo.numfr.store(0); //reset frame count
//    txtr.perftime(); //kludge: flush perf timer
    VOID SDL_Delay(1 sec); //kludge: even out timer with loop
    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        VOID gp.fill(palette[c], NO_RECT, SRCLINE);
        std::ostringstream desc;
        desc << "all-color 0x" << std::hex << palette[c] << std::dec;
        VOID gpstats(desc.str().c_str(), gp, 100); //SIZEOF(palette) - 1);
        bool result = gp.ready(0xffffff, SRCLINE); //mark all univ ready to display
        SDL_Delay(1 sec);
        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
    }
    VOID gpstats("all-pixel test", gp);

#define BKG  dimARGB(0.25, CYAN) //BLACK
//    VOID gp.fill(dimARGB(0.25, CYAN), NO_RECT, SRCLINE);
    gp.clear_stats(); //frinfo.numfr.store(0); //reset frame count
//    txtr.perftime(); //kludge: flush perf timer
    if (delay_msec) VOID SDL_Delay(delay_msec); //0.1 sec); //kludge: even out timer with loop
    for (int y = 0, c = 0; y < gp.UnivLen; ++y)
        for (int x = 0; x < gp.NumUniv; x += 4, ++c)
        {
            VOID gp.fill(BKG, NO_RECT, SRCLINE); //use up some CPU time
            Uint32 color = palette[c % SIZEOF(palette)]; //dimARGB(0.25, PINK);
            gp.nodes[x + 3][y] = gp.nodes[x + 2][y] = gp.nodes[x + 1][y] =
            gp.nodes[x][y] = color;
            std::ostringstream desc;
            desc << "node[" << x << "," << y << "] <- 0x" << std::hex << color << std::dec;
            VOID gpstats(desc.str().c_str(), gp, 100);
            if (delay_msec) SDL_Delay(delay_msec); //0.1 sec);
            if (SDL_QuitRequested()) { y = gp.UnivLen; x = gp.NumUniv; break; } //Ctrl+C or window close enqueued
        }
    VOID gpstats("4x pixel text", gp);
#if 0
    debug(BLUE_MSG << timestamp() << "render" << ENDCOLOR);
    for (int x = 0; x < gp.Width /*NUM_UNIV*/; ++x)
        for (int y = 0; y < gp.Height /*UNIV_LEN*/; ++y)
            gp.nodes[x][y] = palette[(x + y) % SIZEOF(palette)]; //((x + c) & 1)? BLACK: palette[(y + c) % SIZEOF(palette)]; //((x + y) & 3)? BLACK: palette[c % SIZEOF(palette)];
    debug(BLUE_MSG << timestamp() << "refresh" << ENDCOLOR);
    gp.refresh(SRCLINE);
    debug(BLUE_MSG << timestamp() << "wait" << ENDCOLOR);
    VOID SDL_Delay(10 sec);
    debug(BLUE_MSG << timestamp() << "quit" << ENDCOLOR);
    return;

    for (int c = 0; c < SIZEOF(palette); ++c)
    {
        gp.fill(palette[c], SRCLINE);
        gp.refresh(SRCLINE);
        VOID SDL_Delay(1 sec);
        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
    }

//    debug(BLUE_MSG << "GpuPort" << ENDCOLOR);
    for (int c;; ++c)
//    {
        for (int x = 0; x < gp.Width /*NUM_UNIV*/; ++x)
//        {
            for (int y = 0; y < gp.Height /*UNIV_LEN*/; ++y)
{
                Uint32 color = palette[c % SIZEOF(palette)]; //((x + c) & 1)? BLACK: palette[(y + c) % SIZEOF(palette)]; //((x + y) & 3)? BLACK: palette[c % SIZEOF(palette)];
if ((x < 4) && (y < 4)) printf("%sset pixel[%d,%d] @%p = 0x%x...\n", timestamp().c_str(), x, y, &gp.nodes[x][y], color); fflush(stdout);
                gp.nodes[x][y] = color;
                gp.refresh(SRCLINE);
                VOID SDL_Delay(1 sec);
                if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
}
//printf("%sdirty[%d] ...\n", timestamp().c_str(), x); fflush(stdout);
//            gp.dirty(gp.pixels[x], SRCLINE);
//        }
//        gp.refresh(SRCLINE);
//        VOID SDL_Delay(1 sec);
//        if (SDL_QuitRequested()) break; //Ctrl+C or window close enqueued
//    }
#endif
    debug(BLUE_MSG << "finish" << ENDCOLOR);
//    return 0;
}
#endif

class GpuPort_napi
{
 public:
  static napi_value Init(napi_env env, napi_value exports);
  static void Destructor(napi_env env, void* nativeObject, void* finalize_hint);

 private:
  explicit MyObject(double value_ = 0);
  ~MyObject();

  static napi_value New(napi_env env, napi_callback_info info);
  static napi_value GetValue(napi_env env, napi_callback_info info);
  static napi_value SetValue(napi_env env, napi_callback_info info);
  static napi_value PlusOne(napi_env env, napi_callback_info info);
  static napi_value Multiply(napi_env env, napi_callback_info info);
  static napi_ref constructor;
  double value_;
  napi_env env_;
  napi_ref wrapper_;
};


napi_value Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value fn;

    // Arguments 2 and 3 are function name and length respectively
    // We will leave them as empty for this example
    status = napi_create_function(env, NULL, 0, MyFunction_wrapped, NULL, &fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to wrap native function");
    status = napi_set_named_property(env, exports, "my_function", fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to populate exports");

    status = napi_create_function(env, NULL, 0, Hello_wrapped, NULL, &fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to wrap native function");
    status = napi_set_named_property(env, exports, "hello", fn);
    if (status != napi_ok) napi_throw_error(env, NO_ERRCODE, "Unable to populate exports");
    return exports;
}
// NODE_API_MODULE(testaddon, Init)
// NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)


#if 0
int main(int argc, const char* argv[])
{
//    MSG("testing " __TEST_FILE__ " ...");
    ARGS args;
    for (int i = 0; i < argc; ++i) args.push_back(std::string(argv[i]));
    unit_test(args);
    return 0;
}
#endif

//eof