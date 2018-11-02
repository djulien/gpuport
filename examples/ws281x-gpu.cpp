//Node.js add-on to write pixels to a texture using RPi GPU
//GPU gives a 24-bit parallel port with precision timing, and GLSL shaders can be used for generating effects
//RPi API only seems to support OpenGLES 2.0 currently; more info at http://elinux.org/Raspberry_Pi_VideoCore_APIs
//OpenGL ES 2 API at http://docs.gl/es2

//partly based on Chapter 9 Simple_Texture2D example at "OpenGL(R) ES 2.0 Programming Guide" by Aaftab Munshi, Dan Ginsburg, Dave Shreiner at http://www.opengles-book.com
//partly based on lesson 5 examples at http://lazyfoo.net/tutorials/OpenGL
//partly based on info at http://blog.scottfrees.com/building-an-asynchronous-c-addon-for-node-js-using-nan
//and http://blog.scottfrees.com/calling-native-c-dlls-from-a-node-js-web-app


//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO//TODO
//TODO: rewr as Xwindow + OpenGL canvas with run-time size + shaders (WS281X timing) from caller + pixel/line/rect/fill methods
////////////////////////////////////////////////////////////////////////////


//TODO: get mpg123 playback position https://gist.github.com/coma/5003885
//  https://www.mpg123.de/api/group__mpg123__seek.shtml
//  https://github.com/TooTallNate/node-speaker
//  or mpg321?  http://mpg321.sourceforge.net/about.html

//TODO: heartbeat?
//http://www.raspberry-projects.com/pi/programming-in-c/timing/clock_gettime-for-acurate-timing

//WS281X timing note: https://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/

//requires OpenGLES 2.0
// sudo apt-get install libgles2-mesa-dev
//maybe also mesa-utils-extra ??

//TODO: use SDL2 to cut down on init code?
//TODO: push more work onto GPU, espsecially pivots, then fx
//TODO: consider one of:
//  VESA 1280 x 1024 @60 Hz (pixel clock 108.0 MHz)
//  VESA Signal 1400 x 1050 @60 Hz timing

//critical design parameters:
//SCREEN_SCAN_WIDTH / pixel clock must be ~= 30 usec (or a fraction of) for WS281X
//HSYNC + front/back porch time must be <= 1 usec to conform to WS281X protocol timing
//significant design parameter:
//UNIV_LEN * 30 usec determines refresh rate (FPS)
//pixel clock determines max mux for output signals and max #universes
//theoretical limit for RPi is ~ 1.5M nodes @30 FPS (~1500 univ * 1K univ len)

#define SCREEN_SCAN_WIDTH  (1536-216) //set to *exact* RPi screen width (pixels) *including* hsync time; must be a multiple of NODE_BITS
#define SCREEN_DISP_WIDTH  (1488-208) //set to *exact* RPi displayable screen width (pixels) *excluding* hsync time; must be 93/96 of SCREEN_SCAN_WIDTH
#define NODE_BITS  24 //#bits to send to each WS281X node; determined by protocol
#define NUM_GPIO  24 //#pins to use for WS281X control; up to 24 available for VGA usage
#define LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% limits each WS281X node to 50 mA instead of 60 mA

//select max "universe" size and count:
#define NUM_UNIV  24 //max #universes; one "universe" per screen column; > NUM_GPIO requires external hardware mux up to 1-to-(SCREEN_SCAN_WIDTH/24) * NUM_GPIO (1536 max)
//universe size can be any number up to displayable screen height excluding v sync (pixels)
//smaller is easier for debug, but caller can also group (repeat) each node to cut down on #nodes
//#ifndef UNIV_LEN //allow set from Makefile or binding.gyp
#define UNIV_LEN  1140 //1111 gives 30 FPS
 //#define UNIV_LEN  220 //groups of 5 nodes
 //#define UNIV_LEN  20 //for testing
//#endif

#define WS281X_SHADER  true //tell GPU to render WS281X protocol (node.js can override); value determines default mode in fragment shader; must be TRUE for live show
#define SHADER_DEBUG //show debug info on screen
#define CPU_PIVOT //RPi GPU won't access > 1 texture per pixel, so pivot on CPU instead
//#define HWMUX //use GPIO to drive external mux SR; gives up to ~ 800K nodes @30 FPS
//#define SHOW_CONFIG //show display surface config, shaders on console (mainly for debug)
//#define SHOW_VERT //show vertex info on console (only for debug)

#if defined(RPI_NO_X) && WS281X_SHADER && !defined(HWMUX) && !defined(CPU_PIVOT)
 #pragma message("Turning on CPU pivot")
 #define CPU_PIVOT
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// external defs, env dependencies
//

//TODO: is it better to use one larger texture or several smaller (one for each prop)?
//for CPU-to-GPU xfr, it looks like one larger texture is recommended

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //usleep
#include <string.h>
//#include <cstring>
#include <stdarg.h> //var arg defs
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unordered_map>

//#include <GL/glew.h> //must come before gl.h
#include <GLES2/gl2.h>
#include <EGL/egl.h>


//#define NODEJS_ADDON //selected by gyp bindings
#ifdef NODEJS_ADDON
// #pragma message "compiled for Node.js"
//kludge: name conflicts between XWindows and Nan/Node; use alternate name for Nan items
 #define True  True_not_XWin
 #define False  False_not_XWin
 #define None  None_not_XWin
 #include <nan.h>  // includes v8 too
// #include <node.h>
 #define main  main_not_XWin
#endif


#define init()  //dumy definition for innermost init

//supply dummy definitions to reduce clutter in code later; TODO: supply equiv funcs?
#ifdef RPI_NO_X
 #include "bcm_host.h"
 inline void bcm_init()
 {
	bcm_host_init(); //RPi VC(GPU) init; must be called before any other GPU calls; http://elinux.org/Raspberry_Pi_VideoCore_APIs
    printf("bcm init\n");
    init();
 }
 #undef init
 #define init()  bcm_init()
 #define IFPI(yesstmt, nostmt)  yesstmt

// #include <conio.h> //kbhit, getch
// #include <curses.h>
 #define Display  void
// typedef int KeySym;
 #define KeyPress  0
 #define DestroyNotify  1
 #define EscapeKeyCode  0x1b //keycode for Escape
 typedef struct { int type; struct { int keycode; } xkey; } XEvent;
// #define XLookupString(evtkey, text, textlen, key, whatever)  0
 bool XPending(Display*& disp)
 {
//	return kbhit(); //TODO: use SDL for keystroke detection?
//broken	disp = (Display*)getch();
//printf("getch got 0x%s\n", (int)disp);
//	return ((int)disp != ERR);
//	usleep(1000000/60); //throttle to 60 FPS
	return false;
 }
 void XNextEvent(Display* disp, XEvent* evt)
 {
	evt->type = KeyPress;
	evt->xkey.keycode = (int)disp; //getch();
 }
//ncurses wedge:
// void host_init_wedge()
// {
//	bcm_host_init();
//	nodelay(stdscr, TRUE); //getch non-blocking
// }
// #define bcm_host_init  host_init_wedge
#else

 #include <X11/Xlib.h>
 #include <X11/Xatom.h>
 #include <X11/Xutil.h>
 #define EscapeKeyCode  9 //XWindows keycode for Escape
 #define IFPI(yesstmt, nostmt)  nostmt
// void bcm_host_init() {}
#endif


#ifdef SHOW_CONFIG //always show it for sanity check?
 #undef SHOW_CONFIG
 #define SHOW_CONFIG(stmt)  stmt
#else
 #define SHOW_CONFIG(stmt)
#endif


//int main()
//{
//    float hscale = float(SCREEN_WIDTH) / float(NODE_BITS * BIT_WIDTH);
//    printf("hscale = %d / (%d * %d) = %f\n", SCREEN_WIDTH, NODE_BITS, BIT_WIDTH, hscale);
//    return 0;
//}
//#define main  other_main


///////////////////////////////////////////////////////////////////////////////
////
/// globals
//

#ifndef TRUE
 #define TRUE  1
#endif
#ifndef FALSE
 #define FALSE  0
#endif

//in-line IF:
//BoostC compiler (and maybe others) doesn't handle "?:" tenary operator at compile time, even with constants (it should).
//This macro gives equivalent functionality, but results in a constant expression that can be reduced at compile time.
//This generates *a lot* more efficent code, especially for * / % operators.
//CAUTION: when used with #defines these macros may generate long statements that could overflow preprocessors's macro body limit.
//#define IIF(tfval, tval, fval)  //((((tfval)!=0)*(tval)) + (((tfval)==0)*(fval))) //this one repeats tfval so there could be side effects ot macro body could get too long
#define IIF(tfval, tval, fval)  (((tfval)!=0) * ((tval)-(fval)) + (fval)) //this one assumes fval is short and tries to minimize macro body size and side effects by expanding tfval only once
//#define IIF0(tfval, tval, dummy)  (((tfval)!=0)*(tval))  //;shorter version if fval == 0; helps avoid macro body length errors
#if ((3 == 3) != 1) || ((3 == 4) != 0)  //paranoid check; IIF relies on this behavior
 #error "IIF broken: TRUE != 1 or FALSE != 0"
#endif

#define STRINGIZE(text)  #text
#define STRING(text)  STRINGIZE(text) //kludge to force string; http://stackoverflow.com/questions/1562074/how-do-i-show-the-value-of-a-define-at-compile-time

#define rdup(n, d)  ((d) * divup(n, d))
#define rddown(n, d)  ((d) * ((n) / (d)))
#define divup(n, d)  (((n) + (d) - 1) / (d))

#define LEN(thing)  (sizeof(thing) / sizeof((thing)[0]))
#define FLOAT(x)  x##.0 //turn int #def into float


//ABGR colors:
//TODO: use ARGB instead and/or fix little vs. big endian
#define RED  0xff0000ff
#define GREEN  0xff00ff00
#define BLUE  0xffff0000
#define YELLOW  0xff00ffff
#define CYAN  0xffffff00
#define MAGENTA  0xffff00ff
#define WHITE  0xffffffff
#define WARM_WHITE  0xffffffb4 //h 60/360, s 30/100, v 1.0
#define BLACK  0xff000000 //NOTE: need alpha
#define XPARENT  0 //no alpha

#define A(color)  (Amask(color) >> 24)
#define R(color)  (Rmask(color) >> 16)
#define G(color)  (Gmask(color) >> 8)
#define B(color)  Bmask(color)
#define Amask(color)  ((color) & 0xff000000)
#define Rmask(color)  ((color) & 0x00ff0000)
#define Gmask(color)  ((color) & 0x0000ff00)
#define Bmask(color)  ((color) & 0x000000ff)

#ifdef LIMIT_BRIGHTNESS
 #define SUM(color)  (R(color) + G(color) + B(color))
 #define LIMIT(color)  IIF(SUM(color) > LIMIT_BRIGHTNESS, \
    Amask(color) | \
    ((R(color) * LIMIT_BRIGHTNESS / SUM(color)) << 16) | \
    ((G(color) * LIMIT_BRIGHTNESS / SUM(color)) << 8) | \
    (B(color) * LIMIT_BRIGHTNESS / SUM(color)), \
    color)
#else
 #define LIMIT(color)  color
#endif

//convert color ARGB <-> ABGR format:
//OpenGL seems to prefer ABGR format, but RGB order is more readable (for me)
//convert back with same function & 0xffffff
//TODO: drop alpha setting?
//??	if (!Amask(color) /*&& (color & 0xffffff)*/) color |= 0xff000000; //RGB present but no alpha; add full alpha to force color to show
#define ARGB2ABGR(color)  \
	(Amask(color) | (Rmask(color) >> 16) | Gmask(color) | (Bmask(color) << 16)) //swap R, B
#define SWAP32(uint32)  \
    ((Amask(uint32) >> 24) | (Rmask(uint32) >> 8) | (Gmask(uint32) << 8) | (Bmask(uint32) << 24))


#define CLAMP_RECT(x, y, w, h, maxw, maxh)  \
{ \
    if (x < 0) x = 0; \
    if (y < 0) y = 0; \
    if (x >= maxw) x = maxw - 1; \
    if (y >= maxh) y = maxh - 1; \
    if (w < 0) w = 0; \
    if (h < 0) h = 0; \
    if (x + w > maxw) w = maxw - x; \
    if (y + h > maxh) h = maxh - y; \
}


//fwd refs:
void init_chain(void);
int why(int, const char*, ...);
GLuint limit(GLuint);
//uint32_t ARGB2ABGR(uint32_t);
void blend(uint32_t&, int, int, int, int);
void blend(uint32_t&, uint32_t);

bool eglcre(void); //EGLNativeWindowType hWnd, EGLDisplay* eglDisplay, EGLContext* eglContext, EGLSurface* eglSurface, EGLint attribList[]);
bool wincre(const char*, GLint, GLint);
GLuint progcre(const char *vertShaderSrc, const char *fragShaderSrc);
GLuint shadecre(GLenum type, const char *shaderSrc);
GLuint texcre(void);
bool shaders(void);
void render(void);
void logmsg(const char *formatStr, ...);


///////////////////////////////////////////////////////////////////////////////
////
/// state info
//

//state info:
typedef struct
{
	Display* xDisplay;
//   void*       userData;
	const char* title;
	GLint width, height;
	EGLNativeWindowType hWnd;
	EGLDisplay eglDisplay;
	EGLContext eglContext;
	EGLSurface eglSurface;
// Callbacks
//   void(ESCALLBACK *drawFunc)(struct _escontext *);
//   void(ESCALLBACK *keyFunc)(struct _escontext *, unsigned char, int, int);
//   void(ESCALLBACK *updateFunc)(struct _escontext *, float deltaTime);
	GLuint programObject;
// Attribute locations:
	GLint positionLoc;
	GLint texCoordLoc;
//    GLint hscaleLoc;
    GLboolean wsoutLoc;
    GLint grpLoc;
// Sampler location
	GLint samplerLoc;
//	GLint samplerLoc2;
//	GLint samplerLoc3;
//	GLint samplerLoc4;
// Texture handle
//	GLuint textureId;
//runtime stats:
	struct timeval started, latest;
	struct timezone tz;
	float totaltime;
    int wsout, autoclip; //tri-state
    float group;
//	unsigned int frames, draws;
} MyState;
MyState state; //= {0};

inline void state_init()
{
    init();
	memset(&state, 0, sizeof(state));
	gettimeofday(&state.started, &state.tz);
	state.latest = state.started;
//    state.wsout = WS281X_SHADER; //1;
//    state.group = 1.0;
//    state.autoclip = -1; //warn but allow
}
#undef init
#define init()  state_init()


///////////////////////////////////////////////////////////////////////////////
////
/// wrapper class for texture functions
//  this abtracts the actual hardware interface to the LEDs

//#define FORCE_DRAW //repaint even if not needed
#define ERRCHK(msg)  errchk(msg STRING(_LINE_))
bool errchk(const char* msg)
{
	GLenum errcode = glGetError();
//	const GLubyte* errstr = glewGetErrorString(errcode); //gluErrorString(errcode); //glew.h
	const char* errstr = ""; //TODO: resolve problem with GLEW/GLUT

	if (errcode != GL_NO_ERROR) printf("ERROR code %d: %s %s\n", errcode, msg, errstr);
	return (errcode != GL_NO_ERROR); //true == bad
}


//TODO: split pixel handling from texture handling
template <int W1, int H>
class MyTexture
{
	public: //TODO: private:
	GLuint mTextureID;
//	GLuint mTextureWidth;
//	GLuint mTextureHeight;
	GLushort minx[2][3]; //2 triangles to draw a quad
//NOPE; NOTE: one extra row used as lookup table to help shader run faster
//NOPE//lookup table should be excluded from displayable vertices; however, it's included in order to make debug easier
#ifdef CPU_PIVOT
 #define PIVOT(x)  (WW + (x))
 #define WW  rdup(W1, NUM_GPIO)
 #define TXTW  (2 * WW) //true copy + pivot copy
 #define IFPIVOT(stmt)  stmt
#else //hardware or GPU pivot
 #define WW  W1
 #define TXTW  W1 //just one (true) copy
 #define IFPIVOT(stmt)
#endif
	struct { GLfloat x, y, z, s, t/*, spivot*/; } mverts[WW * H]; //use 1D array to allow more flexible order + mapping
//only vertex colors change, never coordinates, so use a 2D texture to store them
//pixels are sent to GPU all together as one 2D texture
//therefore use a 2D array to simplify access by coordinates
//Y is inner dimension for locality of reference (related nodes stored together)
//first W columns are actual pixel colors, remainder are pivoted to compensate for slow memory access by RPi GPU
#define XY(x, y) y][x //x][y //TODO: BROKEN
	GLuint mpixels[XY(TXTW, H)]; //[W][H]; //x is universe#, y is node#

    int latest_wsout = true;
    float latest_group = 1.0;
	unsigned int render_count, flush_count;
	bool dirty; //texture needs to be resent to GPU
    IFPIVOT(bool dirty_pivot[divup(W1, NUM_GPIO)][H]); //blocks of pixels need to be re-pivoted
	static bool initgl;

	public:
	MyTexture() //GLuint w = 0, GLuint h = 0)
	{
		mTextureID = 0;
//		mTextureWidth = w;
//		mTextureHeight = h;
//		mverts = mpixels = 0;
		dirty = false;
        render_count = flush_count = 0;
	}

	~MyTexture()
	{
		freeTexture();
	}

//TODO: move into init() chain?
	static bool initGL() //once-only system wide
	{
		if (initgl) return true;
		if (!state.width || !state.height) logmsg("initGL: missing w %d, h %d\n", state.width, state.height);
        /*SHOW_CONFIG*/(printf("initGL: w %d, h %d\n", state.width, state.height));
		shaders();
		glViewport(0, 0, state.width, state.height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); //0.0f); //set clear-to color
//		glEnable(GL_TEXTURE_2D);
//    glDepthFunc(GL_ALWAYS);
//    glDisable(GL_DEPTH_TEST);
//    glDisable(GL_STENCIL_TEST);
//    glDisable(GL_CULL_FACE);
		initgl = !ERRCHK("initGL");
		return initgl;
	}

	bool setup() //once-only per texture
	{
//see https://www.opengl.org/wiki/Vertex_Specification_Best_Practices
//		int numvert = mTextureWidth * mTextureHeight;
//		static GLfloat vertices[] = { -0.5f,  0.5f, 0.0f,  // Position 0
//                            0.0f,  0.0f,        // TexCoord 0 
//                           -0.5f, -0.5f, 0.0f,  // Position 1
//                            0.0f,  1.0f,        // TexCoord 1
//                            0.5f, -0.5f, 0.0f,  // Position 2
//                            1.0f,  1.0f,        // TexCoord 2
//                            0.5f,  0.5f, 0.0f,  // Position 3
//                            1.0f,  0.0f         // TexCoord 3
//                         };
//		GLubyte pixels[NUM_VERT * 3] =
//		{
//			255,   0,   0, // Red
//			0, 255,   0, // Gree
//			0,   0, 255, // Blue
//			255, 255,   0  // Yellow
//		};
//		const int TL = 0, BL = 1, BR = 2, TR = 3;
#define SQUEEZE(x)  (x) //no squeeze
//#define SQUEEZE(x)  ((x) * 0.95) //easier to check on screen
//		for (int x = 0, pivot = 0; x < W; ++x)
//			for (int y = 0; y < H; ++y, ++pivot)
//		int TL = 0, TR = 0, BL = 0, BR = 0;
		int TL = 0, TR = (WW - 1) * H, BL = H - 1, BR = WW * H - 1;
#ifdef SHOW_VERT
        printf("%d+ x %d texture setup:\n", WW, H);
#endif
		for (int x = 0, inx = 0; x < WW; ++x) //include W1..WW first time only
			for (int y = 0; y < H; ++y, ++inx)
			{
//				int x = i % W, y = i / W;
//bad				int x = i / W, y = i % W;
//				int x = i / H, y = i % H;
//bad				int x = i % H, y = i / H;
//				int pivot_x = inx % W, pivot_y = inx / W; //BROKEN
				int pivot_x = x, pivot_y = y;
//corner tracking:
//this will select triangles automatically instead of needing to adjust the code below
//				if (!pivot_x && !pivot_y) TL = inx;
//				if (!pivot_x && (pivot_y == H - 1)) BL = inx;
//				if ((pivot_x == W - 1) && !pivot_y) TR = inx;
//				if ((pivot_x == W - 1) && (pivot_y == H - 1)) BR = inx;
//				int yy = H - y - 1; //y is backwards; TODO: swap y and yy? (is this upside down?)
//#if 1 //W - 1, H - 1 ??
				mverts[inx].x = SQUEEZE(pivot_x / ((WW - 1.0) / 2.0) - 1.0); //screen coords [-1..1]
				mverts[inx].y = SQUEEZE((H - pivot_y - 1) / ((H - 1.0) / 2.0) - 1.0); //y is backwards; is this upside down now?
				mverts[inx].z = 0;
//NO!				mverts[inx].s = x / (W) + 0.5; // - 1.0); //(state.width - 1); //(U, V) are 0..1
//NO!				mverts[inx].t = y / (H) + 0.5; // - 1.0); //(state.height - 1);
				mverts[inx].s = x / (WW - 1.0); //(state.width - 1); //(U, V) are 0..1
				mverts[inx].t = y / (H - 1.0); //(state.height - 1); //NOTE: need to compensate for extra row here
//				mverts[inx].spivot = x / (W + rdup(W, NUM_GPIO) - 1.0); //(state.width - 1); //(U, V) are 0..1
//#else
//				mverts[inx].x = pivot_x / (W / 2.0) - 1.0; //screen coords [-1..1]
//				mverts[inx].y = (H - pivot_y - 1) / (H / 2.0) - 1.0; //y is backwards; is this upside down now?
//				mverts[inx].z = 0;
//				mverts[inx].s = (x + 0.5) / W; //(state.width - 1); //(U, V) are 0..1
//				mverts[inx].t = (y + 0.5) / H; //(state.height - 1);
//#endif
#ifdef SHOW_VERT
                const char* lkup = (y == H)? "**": "";
                const char* tag = (inx == TL)? "TL": (inx == TR)? "TR": (inx == BL)? "BL": (inx == BR)? "BR": "";
                if ((x < 2) || (x >= WW - 2))
                    if ((y < 2) || (y >= H - 2)) //just show near edge (too much debug caused lost data)
				printf("%svert[%d]%s = pixel[%d,%d] (%3.2f, %3.2f, %3.2f) = txtr[%d,%d] (%3.2f, %3.2f)%s\n", lkup, inx, tag, x, y, mverts[inx].x, mverts[inx].y, mverts[inx].z, pivot_x, pivot_y, mverts[inx].s, mverts[inx].t, lkup);
#endif
//??NOTE: (U,V) pivot for locality of reference on texture *and* vertex arrays in memory
				mpixels[XY(x, y)] = BLACK;
			}
#ifdef CPU_PIVOT
        need_pivot(false);
		for (int x = WW; x < 2 * WW; ++x)
			for (int y = 0; y < H; ++y)
				mpixels[XY(x, y)] = BLACK; //also set pivot values all to black
#endif
#if 0
//kludge: add a bitmask lookup table as the last row in the texture
//this helps shader run a little faster (RPi GLSL lacks some useful features)
//I put this in with the pixels to avoid swapping between textures within the shader; not sure if that matters or not
//this should only need to be set once; after that, it is protected from updates
        char buf[10 * W + 2], *bp = buf;
        for (int x = 0, mask = 0x800000; x < W; ++x, mask >>= 1)
        {
            mpixels[XY(x, (H+1) - 1)] = ARGB2ABGR(mask);
            sprintf(bp, ", %8x", mpixels[XY(x, (H+1) - 1))]; bp += strlen(bp);
        }
        *bp++ = '\0';
        printf("last row: %s\n", buf + 2);
#endif
//		const int TL = 0, TR = mTextureWidth - 1, BL = mTextureWidth * (mTextureHeight - 1), BR = mTextureWidth * mTextureHeight - 1;
//		static GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
//		static GLushort indices[] = { TL, TR, BR, TL, BR, BL }; //2 triangles
//CAUTION: TR + BL depend on 2D fill order above
		minx[0][0] = minx[1][0] = TL; //top left
		minx[0][1] = TR; //top right
		minx[0][2] = minx[1][1] = BR; //bottom right
		minx[1][2] = BL; //bottom left
#ifdef SHOW_VERT
		printf("triangle corners: TL %d, TR %d, BR %d, BL %d\n", TL, TR, BR, BL);
#endif
		if (!initGL()) return false;
//CAUTION: keep these in sync??
		glPixelStorei(GL_PACK_ALIGNMENT, 1); //tightly packed data??
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//		GLuint textureId = 0;
		glGenTextures(1, &mTextureID);
		if (!mTextureID) return why(0, "can't alloc texture");
		glBindTexture(GL_TEXTURE_2D, mTextureID);
//??		glActiveTexture(GL_TEXTURE0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA /*broken GL_RGBA8*/, TXTW, H, 0, GL_RGBA /*broken GL_BGRA*/, GL_UNSIGNED_BYTE, &mpixels[0][0]); //load texture from pixel colors
//consider:    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height); //https://www.opengl.org/wiki/Common_Mistakes
//NOPE: one extra row for lookup table; no mipmaps
//another (pivoted) copy of pixels is used to compensate for slow texture access by RPi GPU
#if 0 //NO- do during render; load vertex position, texture coordinates:
		glVertexAttribPointer(state.positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices);
		glVertexAttribPointer(state.texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vertices[3]);
		glEnableVertexAttribArray(state.positionLoc);
		glEnableVertexAttribArray(state.texCoordLoc);
		glUniform1i(state.samplerLoc, 0); //set sampler texture unit to 0
#endif
//set up uniforms:
//#pragma message "hscale " STRING(SCREEN_WIDTH) " / (" STRING(NODE_BITS) " * " STRING(BIT_WIDTH) ")" //1488 / 1536 = 0.96875
//        glUniform1f(state.hscaleLoc, float(SCREEN_WIDTH) / float(NODE_BITS * BIT_WIDTH)); ERRCHK("hscale"); //used to compensate for partially hidden last node bit
//#define BIT_WIDTH  64 //#screen pixels per node bit (determines max mux and timing)
//#define NUM_GPIO  24 //#pins to use for WS281X control; up to 24 available for VGA usage
//#define NUM_UNIV //one "universe" per screen column; > 24 requires external hardware mux; can be up to BIT_WIDTH * NUM_GPIO (1536 max)

//set filtering mode:
//GL_LINEAR gives weighted average, GL_NEAREST uses only 1 pixel
//we don't want colors to be changed because they are used as hardware signals
//NEAREST seems like the best choice
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//set wrap mode:
//there might be rounding errors on coordinates, so don't wrap or mirror
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // GL_REPEAT);
//        wsout(true);
#ifdef WS281X_SHADER  //tell GPU to render WS281X protocol (node.js can override)
        wsout(state.wsout); //WS281X_SHADER);
//        glUniform1i(state.wsoutloc, onoff); ERRCHK("wsoutWS281X");
        group(state.group); //1.0);
#endif
//		glBindTexture(GL_TEXTURE_2D, 0);
		bool ok = !ERRCHK("setup");
		dirty = true;
		return ok;
	}

	bool render() //GLuint* pixels = 0) //&mpixels[0][0]) //, GLuint width = 0, GLuint height = 0)
	{
        ++render_count;
//		if (!pixels) pixels = &mpixels[0][0]; //use local copy
//	    freeTexture();
//		if (width)
//			if (width != mTextureWidth) printf("width mismatch: was %d, is now %d\n", mTextureWidth, width);
//			else mTextureWidth = width;
//		if (height)
//			if (height != mTextureHeight) printf("height mismatch: was %d, is now %d\n", mTextureHeight, height);
//			else mTextureHeight = height;
//	    glGenTextures(1, &mTextureID);
		glBindTexture(GL_TEXTURE_2D, mTextureID); ERRCHK("render");
//	    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        IFPIVOT(pivot());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TXTW, H, GL_RGBA /*broken GL_BGRA*/, GL_UNSIGNED_BYTE, &mpixels[0][0]); ERRCHK("render"); //NO-NOTE: excludes lookup table (assumes never overwritten)
//		GLuint* pix1d = &mpixels[0][0];
//		char buf[W * H * 8 + 2], *bp = buf;
//		for (int i = 0; i < W * H; ++i) { sprintf(bp, ",%x", pix1d[i] & 0xffffff); bp += strlen(bp); }
//		printf("render: %s\n", buf + 1);
//	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, NULL);
		bool ok = !ERRCHK("render");
		dirty = true;
printf("render: ok? %d\n", ok);
		return ok;
	}

    void wsout(int tristate)
    {
#ifdef WS281X_SHADER  //tell GPU to render WS281X protocol (node.js can override)
        printf("wsout: %d (%s)\n", tristate, !tristate? "texture as-is": (tristate < 0)? "original + WS281X render overlayed": "real WS281X output");
        glUniform1i(state.wsoutLoc, tristate); ERRCHK("wsout");
        latest_wsout = tristate;
#else
        if (tristate) printf("ignoring wsout: %d\n", tristate);
#endif
    }

    void group(float grp)
    {
#ifdef WS281X_SHADER  //tell GPU to render WS281X protocol (node.js can override)
        printf("group: %f\n", grp);
        glUniform1f(state.grpLoc, grp); ERRCHK("group");
        latest_group = grp;
#else
        if (grp != 1.0) printf("ignoring group %f\n", grp);
#endif
    }

#ifdef CPU_PIVOT
//invalidate entire pivot cache:
    void need_pivot(bool yesno = true)
    {
        for (int x = 0; x < divup(W1, NUM_GPIO); ++x)
            for (int y = 0; y < H; ++y)
                dirty_pivot[x][y] = yesno;
    }

//kludge: CPU needs to do this for now; RPi GPU mem access too slow?
    void pivot(void)
    {
//undo R<->B swap during pivot:
//makes debug easier; no impact to live usage (just swap the wires)
//bit order is 0x80..1,0x8000..0x100,0x800000..0x10000 when red + blue swapped
        static int bitmasks[] =
        {
            0x80, 0x40, 0x20, 0x10, 8, 4, 2, 1, //R7..R0
            0x8000, 0x4000, 0x2000, 0x1000, 0x800, 0x400, 0x200, 0x100, //G7..G0
            0x800000, 0x400000, 0x200000, 0x100000, 0x80000, 0x40000, 0x20000, 0x10000, //B7..B0
            0 //dummy entry to allow trailing comma above (Javascript-like convenence)
        };
//for (int b = 0, x = 0, y = 0; b < NUM_GPIO; ++b)
//  printf("pixel[%d,%d] bit %d: 0x%x\n", x, y, b, mpixels[XY(x, y)] & bitmasks[b]);
//printf("pivot\n");
//for (int x = 0; x < WW; ++x)
//    for (int y = 0; y < H; ++y) //NOTE: cross-row access gives poor cache performance if many blocks are dirty
//        mpixels[XY(PIVOT(x), y)] = (y & 8)? 0xffaa0000: 0xff0000aa;
//if (false)

        for (int x = 0; x < W1; ++x) //NOTE: W1..WW should be 0 so they can be skipped
        {
            int xgrp = x / NUM_GPIO, xofs = x - NUM_GPIO * xgrp;
            if (bitmasks[xofs] == 1) continue; //lsb overlaps H sync; unless H sync + porch time < 0.6 usec, it will generate noise due to partial pulse so turn it off (WS281X nodes might get confused)
            for (int y = 0; y < H; ++y) //NOTE: cross-row access gives poor cache performance if many blocks are dirty
            {
                if (!dirty_pivot[xgrp][y]) continue;
                if (!xofs) //initialize pivoted NUM_GPIO * NUM_BITS blocks
                    for (int b = 0; b < NUM_GPIO; ++b)
                        mpixels[XY(PIVOT(x + b), y)] = 0xff000000; //set alpha; RPi cares, XWindows doesn't seem to
                for (int b = 0; b < NUM_GPIO; ++b) //pivot one bit at a time; TODO: move this to GPU if mem access constraints are lifted
                    if (mpixels[XY(x, y)] & bitmasks[b]) //(0x800000 >> b))
                        mpixels[XY(PIVOT(x - xofs + b), y)] |= bitmasks[xofs]; //0x800000 >> xofs; //NOTE: don't need ARGB2BGR here; just interchange output pin connections
//#define GOWHOOPS //pin-swap kludge for GoWhoops PiHat board
//#ifdef GOWHOOPS
//kludge: move R6 to B0; GoWhoops board omits R6 (GPIO26) so remap it to B0 (GPIO04) since that one is lost to H sync anyway
//NOTE: don't need ARGB2BGR here; just interchange output pin connections
//NO                mpixels[XY(PIVOT(x - xofs + 23), y)] = mpixels[XY(PIVOT(x - xofs + 1), y)];
//#endif
//NOTE: lsb blue overlaps H sync so it will not be usable; it will generate noise unless H sync is < 0.6 usec (node bit trailer time), so turn it off
            }
        }
        need_pivot(false);
    }
#endif //def CPU_PIVOT

	bool flush() //whenever texture changes
	{
        ++flush_count;
#ifndef FORCE_DRAW
// #pragma message "Maintaining 60 FPS"
		if (!dirty)
		{
			usleep(1000000/60); //maintain frame rate at 60 FPS; glSwapBuffers does this?
			return false;
		}
#endif
		printf("flush %d x %d\n", textureWidth(), textureHeight());

		glViewport(0, 0, state.width, state.height); ERRCHK("flush");
		glClear(GL_COLOR_BUFFER_BIT); ERRCHK("flush"); //clear color buffer

#if 1 //THIS MUST BE DONE HERE AT RENDER TIME?
		glVertexAttribPointer(state.positionLoc, 3, GL_FLOAT, GL_FALSE, sizeof(mverts[0]) /*5 * sizeof(GLfloat)*/, &mverts[0].x); ERRCHK("flush"); //vVertices);
		glVertexAttribPointer(state.texCoordLoc, 3, GL_FLOAT, GL_FALSE, sizeof(mverts[0]) /*5 * sizeof(GLfloat)*/, &mverts[0].s); ERRCHK("flush"); //&vVertices[3]); //(U,V) interleaved with (X,Y,Z)
		glEnableVertexAttribArray(state.positionLoc); ERRCHK("flush");
		glEnableVertexAttribArray(state.texCoordLoc); ERRCHK("flush");
		glUniform1i(state.samplerLoc, 0); ERRCHK("flush"); //set sampler texture unit to 0
//		glUniform1i(state.samplerLoc2, 0); ERRCHK("flush"); //set sampler texture unit to 0
//		glUniform1i(state.samplerLoc3, 0); ERRCHK("flush"); //set sampler texture unit to 0
//		glUniform1i(state.samplerLoc4, 0); ERRCHK("flush"); //set sampler texture unit to 0
#endif
		glActiveTexture(GL_TEXTURE0); ERRCHK("flush");
		glBindTexture(GL_TEXTURE_2D, mTextureID); ERRCHK("flush"); //state.textureId);
//QUADS not available in GL ES 2.0 so use TRIANGLES
		glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, &minx[0][0]); ERRCHK("flush"); //indices);

		eglSwapBuffers(state.eglDisplay, state.eglSurface); ERRCHK("flush"); //double buffering for smoother animation
		dirty = false;
		return true;
	}

	void freeTexture()
	{
		if (mTextureID != 0) glDeleteTextures(1, &mTextureID);
		mTextureID = 0;
	}

#if 0
    void incorrect_render(GLfloat x = 0, GLfloat y = 0, bool extras = false)
	{
	    if (mTextureID == 0) return;
	    if (extras)
	    {
		    glClear(GL_COLOR_BUFFER_BIT); //clear color buffer
    //Calculate centered offsets
//		    x = ( SCREEN_WIDTH - gCheckerBoardTexture.textureWidth() ) / 2.f;
//		    y = ( SCREEN_HEIGHT - gCheckerBoardTexture.textureHeight() ) / 2.f;
	    }
#if 0 //TODO
            glLoadIdentity(); //remove previous xforms
            glTranslatef(x, y, 0.f);
            glBindTexture(GL_TEXTURE_2D, mTextureID);
            glBegin(GL_QUADS); //requires GLES 3.0
    	        glTexCoord2f(0.f, 0.f); glVertex2f(0.f, 0.f);
    	        glTexCoord2f(1.f, 0.f); glVertex2f(mTextureWidth, 0.f);
    	        glTexCoord2f(1.f, 1.f); glVertex2f(mTextureWidth, mTextureHeight);
    	        glTexCoord2f(0.f, 1.f); glVertex2f(0.f, mTextureHeight);
            glEnd();
#endif
	    if (extras) eglSwapBuffers(state.eglDisplay, state.eglSurface); //glSwapBuffers(); //double buffering for smoother screen updates
	}
#endif

	void fill(GLuint color)
	{
        color = limit(color);
/*
        int mix = A(color);
        if (mix == 255)
		    for (int x = 0; x < WW; ++x)
			    for (int y = 0; y < H; ++y)
				    mpixels[XY(x, y)] = color;
        else if (mix)
        {
            int premix_R = (255 - mix) * R(color);
            int premix_G = (255 - mix) * G(color);
            int premix_B = (255 - mix) * B(color);
		    for (int x = 0; x < WW; ++x)
			    for (int y = 0; y < H; ++y)
				    blend(mpixels[XY(x, y)], mix, premix_R, premix_G, premix_B);
        }
*/
	    for (int x = 0; x < W1; ++x)
		    for (int y = 0; y < H; ++y)
			    blend(mpixels[XY(x, y)], color);
        IFPIVOT(need_pivot(true));
//        dirty = true; //no; wait for caller to call render()
	}

	void fill_rect(GLuint color, int x1, int y1, int w, int h)
	{
//clamp:
        CLAMP_RECT(x1, y1, w, h, W1, H);
        int x2 = x1 + w, y2 = y1 + h;
/*
        int mix = A(color);
        if (mix == 255)
		    for (int x = x1; x < x2; ++x)
			    for (int y = y1; y < y2; ++y)
				    mpixels[XY(x, y)] = color;
        else if (mix)
        {
            int premix_R = (255 - mix) * R(color);
            int premix_G = (255 - mix) * G(color);
            int premix_B = (255 - mix) * B(color);
		    for (int x = 0; x < WW; ++x)
			    for (int y = 0; y < H; ++y)
				    blend(mpixels[XY(x, y)], mix, premix_R, premix_G, premix_B);
        }
*/
        color = limit(color);
	    for (int x = x1; x < x2; ++x)
		    for (int y = y1; y < y2; ++y)
			    blend(mpixels[XY(x, y)], color);
#ifdef CPU_PIVOT
	    for (int x = x1; x < x2; x += NUM_GPIO)
	        for (int y = y1, x = x1; y < y2; ++y)
                dirty_pivot[x / NUM_GPIO][y] = true;
#endif //def CPU_PIVOT
	}

	void fillcol(GLuint color, int x)
	{
        if ((x < 0) || (x >= W1)) return;
        color = limit(color);
/*
        int mix = A(color);
        if (mix == 255)
		    for (int y = 0; y < H; ++y)
			    mpixels[XY(x, y)] = color;
        else if (mix)
        {
            int premix_R = (255 - mix) * R(color);
            int premix_G = (255 - mix) * G(color);
            int premix_B = (255 - mix) * B(color);
		    for (int y = 0; y < H; ++y)
			    blend(mpixels[XY(x, y)], mix, premix_R, premix_G, premix_B);
        }
        need_pivot(true);
*/
	    for (int y = 0; y < H; ++y)
        {
		    blend(mpixels[XY(x, y)], color);
            IFPIVOT(dirty_pivot[x / NUM_GPIO][y] = true);
        }
//        dirty = true; //no; wait for caller to call render()
	}

	void fillrow(GLuint color, int y)
	{
        if ((y < 0) || (y >= H)) return;
        color = limit(color);
/*
        int mix = A(color);
        if (mix == 255)
		    for (int x = 0; x < W; ++x)
			    mpixels[XY(x, y)] = color;
        else if (mix)
        {
            int premix_R = mix * R(color);
            int premix_G = mix * G(color);
            int premix_B = mix * B(color);
		    for (int x = 0; x < W; ++x)
			    blend(mpixels[XY(x, y)], 255 - mix, premix_R, premix_G, premix_B);
        }
//        need_pivot(true);
        dirty_pivot[0][y] = true;
*/
	    for (int x = 0; x < W1; ++x)
		    blend(mpixels[XY(x, y)], color);
#ifdef CPU_PIVOT
	    for (int x = 0; x < W1; x += NUM_GPIO)
            dirty_pivot[x / NUM_GPIO][y] = true;
#endif
//        dirty = true; //no; wait for caller to call render()
	}

	GLuint pixel(int x, int y, GLuint color = 0)
	{
		if ((x < 0) || (x >= W1)) return 0; //can't use exc; throw "pixel[x] out of range";
		if ((y < 0) || (y >= H)) return 0; //throw "pixel[y] out of range";
		if (color & 0xff000000) //set pixel
        {
//		char buf[W * H * 8 + 2], *bp = buf;
//		for (int i = 0; i < W * H; ++i) { sprintf(bp, ",%x", mpixels[XY(x, y)] & 0xffffff); bp += strlen(bp); }
//		printf("pixel[%d, %d] => [%d]\n", x, y, &mpixels[XY(x, y)] - &mpixels[0][0]);
            blend(mpixels[XY(x, y)], color);
            IFPIVOT(dirty_pivot[x / NUM_GPIO][y] = true);
//        dirty = true; //no; wait for caller to call render()
        }
		return mpixels[XY(x, y)];
	}

/*
	GLuint& pixel(int x, int y) //, GLuint color = 0)
	{
//		if (color & 0xff000000) mpixels[XY(x, y)] = color;
		if ((x < 0) || (x >= W)) x = 0; //can't use exc; throw "pixel[x] out of range";
		if ((y < 0) || (y >= H)) y = 0; //throw "pixel[y] out of range";
//		char buf[W * H * 8 + 2], *bp = buf;
//		for (int i = 0; i < W * H; ++i) { sprintf(bp, ",%x", mpixels[XY(x, y)] & 0xffffff); bp += strlen(bp); }
//		printf("pixel[%d, %d] => [%d]\n", x, y, &mpixels[XY(x, y)] - &mpixels[0][0]);
        dirty_pivot[x / NUM_GPIO][y] = true; //assume caller will change it
//        dirty = true; //no; wait for caller to call render()
		return mpixels[XY(x, y)];
	}
*/

	GLuint* testPattern(int seed) //GLuint w, GLuint h)
	{
//		int count = w * h;
//	        GLuint* mPattern = (GLuint*)malloc(count * sizeof(GLuint));
//		if (mPattern)
//			for (int i = 0; i < count; ++i)
//				mPattern[i] = ((i / w) ^ (i % w) & 0x10)? 0xFFFFFF: 0xFF0000FF; //red or white if 0x10 bit of x and y match
//		return mPattern;
//		GLuint colors[] = {0xffff0000, 0xff00ff00, 0xffffff00, 0xff0000ff}; //R, G, Y, B (RGBA) //ABGR)
//		GLuint colors[] = {0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff0000}; //R, G, Y, B (ABGR)
		GLuint colors[] = {LIMIT(RED), LIMIT(GREEN), LIMIT(YELLOW), LIMIT(BLUE)};
		for (int x = 0; x < WW; ++x)
			for (int y = 0; y < H; ++y)
				mpixels[XY(x, y)] = colors[(x + y + seed) % 4]; //((x >> 4) + (y >> 4) + seed) % 4];
        IFPIVOT(need_pivot(true));
		return &mpixels[0][0];
	}

	GLuint getTextureID() { return mTextureID; }
	GLuint textureWidth() { return WW; } //mTextureWidth; }
	GLuint textureHeight() { return H; } //mTextureHeight; }
};
template <int W1, int H>
bool MyTexture<W1, H>::initgl = false;

//MyTexture<3, 4> LEDs;
//MyTexture<NUM_UNIV, UNIV_LEN> LEDs;
//#pragma message "Compiled for #univ x maxlen = " STRING(NUM_UNIV) " x " STRING(UNIV_LEN)


///////////////////////////////////////////////////////////////////////////////
////
/// main line (if compiled as stand-alone test program)
//

#ifndef NODEJS_ADDON
//fwd refs:
bool userint(void);
void draw(void);
void quit(void);

//XWindows testing only:
//live RPi uses full screen
#define TITLE  "WS281X-GPU Test"
#define WIDTH  640 //320 //SCREEN_SCAN_WIDTH //NOTE: must be same as RPi in order to simulate accurately
#define HEIGHT  480 //240
MyTexture<NUM_UNIV, UNIV_LEN> LEDs;

int main(int argc, char *argv[])
{
//	static bool done = false; //in case called 2x by node.js
//	if (done) return 1;
//	done = true;

//	for(int i = 0; i < argc; ++i)
//		printf("arg[%d/%d]: '%s'\n", i, argc, argv[i]);
	init_chain();
	bool ok = wincre(TITLE, WIDTH, HEIGHT);
	if (ok) ok = eglcre();
//TODO?	glewInit(); //must occur after context created; BROKEN
	if (ok) ok = LEDs.setup();
	if (!ok) { printf("failed\n"); return 1; }
//#ifndef NODEJS_ADDON
	while (!userint()) draw();
	quit();
//#endif
	return 0;
}
#endif


/*
void init()
{
//	bcm_host_init(); //RPi VC(GPU) init; must be called before any other GPU calls; http://elinux.org/Raspberry_Pi_VideoCore_APIs
	memset(&state, 0, sizeof(state));
	gettimeofday(&state.started, &state.tz);
	state.latest = state.started;
//printf("state %d, tz %d\n", sizeof(state), state.tz);
}
*/


#ifndef NODEJS_ADDON
void quit()
{
//	glDeleteTextures(1, &state.textureId);
	glDeleteProgram(state.programObject);
}
#endif


//check for user interrupt(keyboard):
//reads X11 event loop
#ifndef NODEJS_ADDON
bool userint()
{
	XEvent evt;
//	KeySym key;
//	char text;
	bool cancel = FALSE;

// Pump all messages from X server. Keypresses are directed to keyfunc(if defined)
	while (XPending(state.xDisplay))
	{
		XNextEvent(state.xDisplay, &evt);
		if (evt.type == KeyPress)
		{
			printf("KeyPress: 0x%x\n", evt.xkey.keycode);
//			if (XLookupString(&evt.xkey, &text, 1, &key, 0) > 0)
//				keyfunc(text, 0, 0);
			if (evt.xkey.keycode == EscapeKeyCode) cancel = TRUE;
		}
		if (evt.type == DestroyNotify) cancel = TRUE;
	}
	return cancel;
}
#endif


//repaint:
//maintains FPS stats
#ifndef NODEJS_ADDON
void draw()
{
	struct timeval now;
	gettimeofday(&now, &state.tz);
	float deltatime = (float)(now.tv_sec - state.latest.tv_sec +(now.tv_usec - state.latest.tv_usec) * 1e-6);
	float elapsed = (float)(now.tv_sec - state.started.tv_sec +(now.tv_usec - state.started.tv_usec) * 1e-6);
	state.latest = now;

//#if 1
	static int count = 0;
	++count;
//	static GLuint colors[] = {0xffff0000, 0xff00ff00, 0xffffff00, 0xff0000ff};
//	GLuint pixels[UNIV_LEN][NUM_UNIV];
//	for (int y = 0; y < NUM_UNIV; ++y)
//		for (int x = 0; x < UNIV_LEN; ++x)
//			pixels[x][y] = colors[(x + y) & 3];
//	LEDs.render(&pixels[0][0]);
//#ifndef NODEJS_ADDON //display a test pattern
	if (count / 60 != (count - 1) / 60) //1 sec @60 FPS
    {
		LEDs.testPattern(count / 60);
		LEDs.render();
    }
	if (!(count % 300)) //5 sec
	{
		LEDs.fill(BLACK);
//		LEDs.pixel(0, 0) = LEDs.pixel(2, 0) = BLUE;
//		/*LEDs.pixel(1, 2) = LEDs.pixel(2, 2) =*/ LEDs.pixel(1, 1) = YELLOW;
//		LEDs.pixel(0, 2) = LEDs.pixel(1, 3) = LEDs.pixel(2, 2) = /*LEDs.pixel(3, 4) = LEDs.pixel(4, 4) =*/ RED;
//		for (int x = 0; x < UNIV_LEN; ++x)
//			for (int y = 0; y < NUM_UNIV; ++y)
//				LEDs.pixel(x, y) = x * 0x101010 + y * 0x10101;
		LEDs.pixel(0, 0) = RED;
		LEDs.pixel(2, 0) = GREEN;
		LEDs.pixel(2, 3) = YELLOW;
		LEDs.pixel(0, 3) = BLUE;
		LEDs.render();
	}

//	render();
//	++state.frames;
	LEDs.flush();
//#endif
	state.totaltime += deltatime;
	if (state.totaltime <= 2.0f) return; //show stats every 2 sec

	printf("%d frames rendered, %d drawn in last %1.3f seconds (%1.3f sec total) -> %3.3f FPS, %3.3f dps\n", LEDs.render_count, LEDs.flush_count, state.totaltime, elapsed, LEDs.render_count / state.totaltime, LEDs.flush_count / state.totaltime);
	state.totaltime -= 2.0f;
	LEDs.render_count = LEDs.flush_count = 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// OpenGL helpers
//

//for variations, see:
// https://jan.newmarch.name/LinuxSound/Diversions/RaspberryPiOpenGL/
// http://lobotony.tumblr.com/post/49936884122/minimal-raspberry-pi-opengl-application-without-x


//#define TITLE  "WS281X-GPU"
//XWindows testing only:
//live RPi uses full screen
//#define WIDTH  640 //320
//#define HEIGHT  480 //240

bool wincre(const char* title, GLint width, GLint height) //const char* title, GLint width, GLint height)
{
    if (!width || !height) { width = SCREEN_DISP_WIDTH; height = UNIV_LEN; }
	state.width = width; //|| 640;
	state.height = height; //|| 480;
	state.title = (title && *title)? title: "WS281X-gpu test";
#ifdef RPI_NO_X
// create an EGL window surface, passing context width/height
	uint32_t display_width, display_height;
	if (graphics_get_display_size(0 /* LCD */, &display_width, &display_height) < 0) return why(FALSE, "can't get display device");
	/*always, for sanity SHOW_CONFIG*/(printf("win create(RPi): display %d x %d\n", display_width, display_height));

	VC_RECT_T src_rect, dst_rect;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	static EGL_DISPMANX_WINDOW_T nativewindow;

	src_rect.x = src_rect.y = 0;
	src_rect.width = display_width << 16; //why? maybe just >>> screen size?
	src_rect.height = display_height << 16;
	state.width = display_width; //fixed screen size; override
	state.height = display_height;
    if (state.width != SCREEN_DISP_WIDTH) return why(FALSE, "screen width %d does not match expected %d", state.width, SCREEN_DISP_WIDTH);
    if (state.height != UNIV_LEN) return why(FALSE, "screen height %d does not match expected %d", state.height, UNIV_LEN);

//set dest to full screen:
	dst_rect.x = dst_rect.y = 0;
	dst_rect.width = display_width;
	dst_rect.height = display_height;

//	if ((dst_rect.width != state.width) || (dst_rect.height != state.height))
//		printf("SCREEN MISMATCH: requested %d x %d, should be %d x %d\n", dst_rect.width, dst_rect.height, state.width, state.height);
	dispman_display = vc_dispmanx_display_open(0 /* LCD */);
	dispman_update = vc_dispmanx_update_start(0);
	dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
0/*layer*/, &dst_rect, 0/*src*/, &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);
	nativewindow.element = dispman_element;
	nativewindow.width = display_width;
	nativewindow.height = display_height;
	vc_dispmanx_update_submit_sync(dispman_update);
	state.hWnd = &nativewindow;

#else //X11 native display initialization
	state.xDisplay = XOpenDisplay(NULL);
	if (!state.xDisplay) return why(FALSE, "Xopen display failed");
	/*always, for sanity SHOW_CONFIG*/(printf("win create(XWin !RPi): display 0x%x, size %d x %d\n", state.xDisplay, state.width, state.height));
	Atom wm_state;
	XWMHints hints;
	XEvent evt = {0};
	XSetWindowAttributes swa;
	XSetWindowAttributes xattr;

	Window root = DefaultRootWindow(state.xDisplay);
	swa.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask;
//    state.width = SCREEN_DISP_WIDTH; //override; use compiled size at run-time
//show warning but allow continue for dev/test:
    if (state.width != SCREEN_DISP_WIDTH) printf("WARNING: screen width %d does not match expected width %d\n", state.width, SCREEN_DISP_WIDTH);
    if (state.height != UNIV_LEN) printf("WARNING: screen height %d does not match expected height %d\n", state.height, UNIV_LEN);

	Window win = XCreateWindow(state.xDisplay, root, 0, 0, state.width, state.height, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &swa);
//printf("root 0x%x, win 0x%x, xcre(disp 0x%x, w %d, h %d)\n", root, win, state.xDisplay, state.width, state.height);
	xattr.override_redirect = FALSE;
	XChangeWindowAttributes(state.xDisplay, win, CWOverrideRedirect, &xattr);
	hints.input = TRUE;
	hints.flags = InputHint;
	XSetWMHints(state.xDisplay, win, &hints);
//make the window visible on the screen:
	XMapWindow(state.xDisplay, win);
	XStoreName(state.xDisplay, win, state.title);
//get identifiers for the provided atom name strings:
	wm_state = XInternAtom(state.xDisplay, "_NET_WM_STATE", FALSE);
//	memset(&evt, 0, sizeof(evt));
	evt.type = ClientMessage;
	evt.xclient.window = win;
	evt.xclient.message_type = wm_state;
	evt.xclient.format = 32;
	evt.xclient.data.l[0] = 1;
	evt.xclient.data.l[1] = FALSE;
	XSendEvent(state.xDisplay, DefaultRootWindow(state.xDisplay), FALSE, SubstructureNotifyMask, &evt);
	state.hWnd =(EGLNativeWindowType) win;
//printf("hwnd 0x%x, wmstate 0x%x\n", state.hWnd, wm_state);
#endif

	return TRUE;
}

//show digit or "Z" if zero:
const char* Z(EGLint val)
{
    static int which = 0;
    static char zbuf[8][2] = {0};
    zbuf[which % 8][0] = val? '0' + val: 'Z';
    return zbuf[which++ % 8];
}


#define WANT_ALPHA  FALSE
#define WANT_DEPTH  FALSE
#define WANT_STENCIL  FALSE
#define WANT_MULTISAMPLE  FALSE
//creates EGL rendering context and all associated elements:
bool eglcre() //EGLNativeWindowType hWnd, EGLDisplay* eglDisplay, EGLContext* eglContext, EGLSurface* eglSurface, EGLint attribList[])
{
	static EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
	static EGLint attribList[] =
	{
		EGL_RED_SIZE, 8, //5,
		EGL_GREEN_SIZE, 8, //6,
		EGL_BLUE_SIZE, 8, //5,
        EGL_BUFFER_SIZE, 24, //??
//wrong one		EGL_DEPTH_SIZE, 24,
		EGL_ALPHA_SIZE, WANT_ALPHA? 8: EGL_DONT_CARE,
		EGL_DEPTH_SIZE, WANT_DEPTH? 8: EGL_DONT_CARE,
		EGL_STENCIL_SIZE, WANT_STENCIL? 8: EGL_DONT_CARE,
		EGL_SAMPLE_BUFFERS, WANT_MULTISAMPLE? 1: 0,
		EGL_NONE
	};   

	/*EGLDisplay*/ state.eglDisplay = eglGetDisplay(IFPI(EGL_DEFAULT_DISPLAY, (EGLNativeDisplayType)state.xDisplay));
//printf("egl: disp 0x%x\n", display);
	if (state.eglDisplay == EGL_NO_DISPLAY) return why(FALSE, "egl get display failed");

//	EGLConfig config;
	EGLint numConfigs;
    EGLConfig cfgs[30];
	EGLint majorVersion, minorVersion;
	if (!eglInitialize(state.eglDisplay, &majorVersion, &minorVersion)) return why(FALSE, "egl display init failed");
    SHOW_CONFIG(printf("egl: GL major %d, minor %d, glsl %s\n", majorVersion, minorVersion, glGetString(GL_SHADING_LANGUAGE_VERSION)));
//	if (!eglGetConfigs(state.eglDisplay, /*NULL, 0,*/ cfgs, 30, &numConfigs)) return why(FALSE, "!egl get cfg");
//printf("egl: #cfg %d\n", numConfigs);

//http://directx.com/2014/06/egl-understanding-eglchooseconfig-then-ignoring-it/
//http://malideveloper.arm.com/resources/sample-code/selecting-the-correct-eglconfig/
//setEGLConfigChooser(8, 8, 8, 8, 16, 0) 
	if (!eglChooseConfig(state.eglDisplay, attribList, cfgs, LEN(cfgs), &numConfigs) || !numConfigs) return why(FALSE, "egl failed to choose cfg");
    SHOW_CONFIG(printf("%d configs:\n", numConfigs));
int found = -1;
for (int i = 0; i < numConfigs; ++i)
{
     EGLint id = 0, alpha = 0, bind_rgb = 0, bind_rgba = 0, red = 0, green = 0, blue = 0, xred = 0, xgreen = 0, xblue = 0, depth = 0, render = 0;
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_CONFIG_ID, &id);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_ALPHA_SIZE, &alpha);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_BIND_TO_TEXTURE_RGB, &bind_rgb);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_BIND_TO_TEXTURE_RGBA, &bind_rgba);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_RED_SIZE, &red);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_GREEN_SIZE, &green);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_BLUE_SIZE, &blue);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_TRANSPARENT_RED_VALUE, &xred);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_TRANSPARENT_GREEN_VALUE, &green);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_TRANSPARENT_BLUE_VALUE, &xblue);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_BUFFER_SIZE, &depth);
    eglGetConfigAttrib(state.eglDisplay, cfgs[i], EGL_RENDERABLE_TYPE, &render);
//    if (!alpha || !red || !green || !blue) continue;
//    if (!render || !bind_rgb || !bind_rgba || !depth) continue;
    bool ok = ((red >= 8) && (green >= 8) && (blue >= 8)) || (depth >= 24);
    const char* tag = ((found < 0) && ok)? "**": ok? "OK": "";
    if (ok) found = i;
    SHOW_CONFIG(printf("egl cfg[%d/%d]%s: id %d, a/r/g/b %s/%s/%s/%s, xpar %s/%s/%s, depth %d, bind rgb %d/%d, render %d\n", i, numConfigs, tag, id, Z(alpha), Z(red), Z(green), Z(blue), Z(xred), Z(xgreen), Z(xblue), depth, bind_rgb, bind_rgba, render));
}
    if (found < 0) return why(FALSE, "no suitable egl config found");
//CAUTION: config enum/choose seems to be flakey
//RPi was selecting a bad config without the above loop to explicitly step thru all configs!  don't just use cfg[0]

	/*EGLSurface*/ state.eglSurface = eglCreateWindowSurface(state.eglDisplay, cfgs[found], (EGLNativeWindowType)state.hWnd, NULL);
	if (state.eglSurface == EGL_NO_SURFACE) return why(FALSE, "no surface");
	/*EGLContext*/ state.eglContext = eglCreateContext(state.eglDisplay, cfgs[found], EGL_NO_CONTEXT, contextAttribs);
	if (state.eglContext == EGL_NO_CONTEXT) return why(FALSE, "create context failed");
	if (!eglMakeCurrent(state.eglDisplay, state.eglSurface, state.eglSurface, state.eglContext)) return why(FALSE, "make current disp failed");
//TODO: eglSwapInterval?
	return TRUE;
} 


//debug is easier with line#s:
const char* linenums(const char* str)
{
    int numlines = 0;
    for (const char* bp = str; bp = strchr(bp, '\n'); ++bp) ++numlines; //get line count before allocating memory
    char* buf = (char*)malloc(strlen(str) + 1 + numlines * 4), *bp = buf; //NOTE: small and only done once, so let it leak afterward
    numlines = 0;
    *bp = '\0';
    for (const char* stline = str; *stline; ++stline)
    {
        const char* enline = strchr(stline, '\n');
        if (!enline) enline = stline + strlen(stline) - 1;
        sprintf(bp, "%d:", ++numlines); bp += strlen(bp);
        strncpy(bp, stline, enline + 1 - stline); bp += enline + 1 - stline; *bp = '\0';
        stline = enline;
    }
    return buf;
}


//load shaders and program object:
//load a vertex and fragment shader, create a program object, link program
//Errors output to log.
/// \param vertShaderSrc Vertex shader source code
/// \param fragShaderSrc Fragment shader source code
/// \return A new program object linked with the vertex/fragment shader pair, 0 on failure
bool shaders()
{
//GLbyte vShaderStr[] =
//const char* x_vShaderStr = STRING(
//	precision mediump float; //suitable for texture coordinates, colors\n

#define BIT_WIDTH  (SCREEN_SCAN_WIDTH / NODE_BITS) //#screen pixels per WS281X node bit (determines max mux and timing)
#if NODE_BITS * (SCREEN_SCAN_WIDTH / NODE_BITS) != SCREEN_SCAN_WIDTH
 #error "Screen scan width must be a multiple of " STRING(NODE_BITS) " to accomodate WS281X nodes: " STRING(SCREEN_SCAN_WIDTH)
#endif
#if NUM_GPIO > 24
 #error "#GPIO cannot exceed 24 VGA pins"
#endif

#define HSCALE  (float(SCREEN_DISP_WIDTH) / float(NODE_BITS * BIT_WIDTH))
//#define HSCALE  (FLOAT(SCREEN_WIDTH) / (FLOAT(NODE_BITS) * FLOAT(BIT_WIDTH)))
//#if defined(WS281X_SHADER) || defined(SHADER_DEBUG) //tell GPU to render WS281X protocol (node.js can override)
//#ifdef WS281X_SHADER //tell GPU to render WS281X protocol (node.js can override)
 #define fShaderStr  fShaderStr_ws281x
//#else
// #define fShaderStr  fShaderStr_asis
//#endif


const char* vShaderStr =
	"precision mediump float;\n" //suitable for texture coordinates, colors
//    "uniform float HSCALE;\n" //compensate for partially off-screen last node bit
//    "#define HSCALE  " STRING(HSCALE) "\n"
    "const float HSCALE = " STRING(HSCALE) ";\n"
    "const float TXTW = " STRING(float(rdup(NUM_UNIV, NUM_GPIO))) ";\n"
    "uniform float group_ws281x;\n"
//    "uniform int screenw, screenh;\n"
	"attribute vec4 a_position;\n"
	"attribute vec2 a_texCoord;\n"
	"varying vec4 v_texCoord;\n"

	"uniform sampler2D s_texture;\n"
	"varying vec4 pivbits0;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = a_position;\n"
    "   vec4 unpack;\n"
    "   unpack.stpq = a_texCoord.stss;\n"
#ifdef CPU_PIVOT
    "   unpack.q /= 2.0;\n" //seems backwards??
    "   unpack.p /= 2.0;\n" //seems backwards??
    "   unpack.p += 0.5 + .017;\n" //shift over; //TODO: fix this ~= +1/60
#endif //def CPU_PIVOT
    "   unpack.t /= group_ws281x;\n" //scale it up to cover multiple output pixels
//    "   unpack.p += (TXTW - 1.0) / (2.0 * TXTW - 1.0);\n" //shift over
//    "   unpack.q *= (2.0 * TXTW - 1.0) / (TXTW - 1.0);\n" //show original texture, hide pivot data
//    "   unpack.p *= (2.0 * TXTW - 1.0) / (TXTW - 1.0);\n" //show pivoted texture, hide original
//compensate for last node bit partially off-screen:
//    "   remap.x *= HSCALE;\n"
//    "   remap.z *= HSCALE;\n"
	"	v_texCoord = unpack;\n"
//	"	v_texCoord = a_texCoord;\n"
    "           pivbits0 = texture2D(s_texture, vec2(0.0 / 23.0, v_texCoord.t));\n" //should call fx()
	"}\n";


//https://www.khronos.org/opengles/sdk/docs/reference_cards/OpenGL-ES-2_0-Reference-card.pdf

//original fragment shader:
//GLbyte fShaderStr[] = 
/*
const char* fShaderStr_asis = //show texture as-is
	"precision mediump float;\n" //suitable for texture coordinates, colors
	"uniform sampler2D s_texture;                        \n"
	"varying vec4 v_texCoord;                            \n"
  	"void main()                                         \n"
	"{                                                   \n"
    "   vec4 outcolor = texture2D(s_texture, v_texCoord.st);\n"
    "   gl_FragColor = outcolor;\n"
	"}\n";
*/


//WS281X shader:

//WS281X low, high bit times:
//bit time is 1.25 usec and divided into 3 parts:
//          ______ _______           _____
//   prev  / lead \  data \  trail  / next
//   bit _/        \_______\_______/  bit
//all bits start with low-to-high transition
//"0" bits fall after 1/4 of bit time; "1" bits fall after 5/8 of bit time
//all bits end in a low state

//below timing is compliant with *both* W2811 and WS2812:
//this allows them to be interchangeable wrt software
//however, certain types of strips are still subjec to R <-> G swap
#define BIT_0H  16.0/64.0 //middle of common range
//#define BIT_0L  48.0/64.0
#define BIT_1H  36.0/64.0 //middle of common range
//#define BIT_1L  28.0/64.0
#define BIT_TIME  1.28 //64 * 50 MHz pixel clock (rounded up)

//only middle portion of bit time actually depends on bit value:
#define BIT_DATA_BEGIN  (min(BIT_0H, BIT_1H) * BIT_TIME) //before this time is high
#define BIT_DATA_END  (max(BIT_0H, BIT_1H) * BIT_TIME) //after this time is low

//#define LKUP_ROW  1.0 //(float(UNIV_LEN) / float(UNIV_LEN + 1)) //contains bitmask lookup table


//CAUTION: output is degraded if shader is too complicated??
const char* fShaderStr_ws281x =
    "#version 100 //130 //150\n" //NOTE: GLES GLSL versions != GL GLSL versions; GLES 2.0 only supports GLSL 1.0
	"precision mediump float;\n" //suitable for texture coordinates, colors
//    "#define TRUE  1\n"
//    "#define FALSE  0\n"
//    "uniform float HSCALE;\n" //compensate for partially off-screen last node bit
//    "const float HSCALE = " STRING(HSCALE) ";\n" //horizontal squeeze factor
    "const vec2 UNDERSCAN = vec2(" STRING(HSCALE) ", 1.0);\n"
    "const float DWSTART = " STRING(BIT_DATA_BEGIN) ";\n" //real data window start
    "const float DWEND = " STRING(BIT_DATA_END) ";\n" //real data window end
//    "const float TXTW = " STRING(float(2 * NUM_UNIV)) ";\n"
//    "#define NUM_GPIO " STRING(float(NUM_GPIO)) "\n" //#GPIO pins assigned to WS281X signals; cannot exceed 24
    "const float NUM_UNIV = " STRING(float(NUM_UNIV)) ";\n" //total #LED strings; use external hw mux when > NUMPINS
    "const float NODE_BITS = " STRING(float(NODE_BITS)) ";\n" //#bits per node to send
	"uniform sampler2D s_texture;\n"
//	"uniform sampler2D s_texture2;\n"
//	"uniform sampler2D s_texture3;\n"
//	"uniform sampler2D s_texture4;\n"
    "uniform int want_ws281x;\n"
    "uniform float group_ws281x;\n"
//    "uniform int screenw, screenh;\n"
    "#define ISDEBUG  (want_ws281x < 0)\n"
    "#define ENABLED  (want_ws281x != 0)\n"
//    "#define want_ws281x  " STRING(WS281X_SHADER) "\n" //true
	"varying vec4 v_texCoord;\n"
	"varying vec4 pivbits0;\n"
//    "const float PERNODE = " STRING(SCREEN_WIDTH) ".0 / " STRING(BIT_WIDTH) ".0;\n" //#bits that will fit on display area of screen
//    "const float HSCALE = PERNODE / float(ceil(PERNODE));\n" //compensate for partially hidden last node bit for more accurage view when debugging on screen
//Bitmask table: MASK[0] = 0x800000, MASK[1] = 0x400000, ..., MASK[23] = 1
//RPi GPU doesn't like non-const array indexing :(
//use lookup table in texture instead
//    "   int BITS[24];\n"
//    "   BITS[0] = 0x800000; BITS[1] = 0x400000; BITS[2] = 0x200000; BITS[3] = 0x100000;\n"
//    "   BITS[4] = 0x80000; BITS[5] = 0x40000; BITS[6] = 0x20000; BITS[7] = 0x10000;\n"
//    "   BITS[8] = 0x8000; BITS[9] = 0x4000; BITS[10] = 0x2000; BITS[11] = 0x1000;\n"
//    "   BITS[12] = 0x800; BITS[13] = 0x400; BITS[14] = 0x200; BITS[15] = 0x100;\n"
//    "   BITS[16] = 0x80; BITS[17] = 0x40; BITS[18] = 0x20; BITS[19] = 0x10;\n"
//    "   BITS[20] = 8; BITS[21] = 4; BITS[22] = 2; BITS[23] = 1;\n" //TODO: use small texture instead?
//    "const float MASKROW = 1.0;\n" //last row of texture contains bitmask lookup table
//fwd refs:
    "#define ERROR(n)  vec4(1.0, 0.2 * float(n), 1.0, 1.0)\n" //bug if this shows up
    "const vec4 ALLOFF = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "const vec4 ALLON = vec4(1.0, 1.0, 1.0, 1.0);\n" //all output lines on
#ifdef SHADER_DEBUG
    "#define BORDERW  0.05\n"
    "#define OVLX  0.9\n"
    "#define OVLY  0.9\n"
    "#define OVERLAY(coord)  ((coord.s >= OVLX) && (coord.y >= OVLY))\n"
    "vec4 overlay(vec2 xy);\n" //for DEBUG only
    "#define rgb2scalar(val)  dot(val, vec4(256.0 * 256.0, 256.0, 1.0, 0.0))\n" //strip alpha
    "#define ALLON  RGB(1.0)\n" //DEBUG ONLY: show R/G/B color
    "bool chkcolor(vec4 color);\n"
    "vec4 gray(float val);\n"
    "vec4 hwmux_debug(vec2 coord);\n"
#endif
    "vec4 fx(vec4 color);\n"
//    "#define LSB(val)  (floor((val) / 2.0) != (val) / 2.0)\n"
    "bool LSB(float val);\n"
    "bool AND(vec4 bits, float mask);\n"
    "vec4 OR(vec4 bits, float mask);\n"
    "#define FUD  0.001\n" //compensate for slight rounding error on RPi
    "#define RGB(val)  _RGB(val, floor((nodebit + FUD) / 8.0))\n"
    "vec4 _RGB(float val, float which);\n" //for DEBUG only
  	"void main()\n"
	"{\n"
//map scan beam location ([0..1], [0..1]) back to screen (x, y)
//x determines which bit within the 24 bit node to send out next
//y determines which node within the LED string (universe) to send
    "   vec4 outcolor = vec4(0.0, 0.0, 0.0, 1.0);\n" //= texture2D(s_texture, v_texCoord); //original rendering
//    "   vec3 remap = v_texCoord;\n"
    "   vec2 rawimg = v_texCoord.st;\n"
#if 0
    "           vec4 pivbits0 = texture2D(s_texture, vec2(0.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits1 = texture2D(s_texture2, vec2(1.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits2 = texture2D(s_texture3, vec2(2.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits3 = texture2D(s_texture4, vec2(3.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits4 = texture2D(s_texture, vec2(4.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits5 = texture2D(s_texture, vec2(5.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits6 = texture2D(s_texture, vec2(6.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits7 = texture2D(s_texture, vec2(7.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits8 = texture2D(s_texture, vec2(8.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits9 = texture2D(s_texture, vec2(9.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits10 = texture2D(s_texture, vec2(10.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits11 = texture2D(s_texture, vec2(11.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits12 = texture2D(s_texture, vec2(12.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits13 = texture2D(s_texture, vec2(13.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits14 = texture2D(s_texture, vec2(14.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits15 = texture2D(s_texture, vec2(15.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits16 = texture2D(s_texture, vec2(16.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits17 = texture2D(s_texture, vec2(17.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits18 = texture2D(s_texture, vec2(18.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits19 = texture2D(s_texture, vec2(19.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits20 = texture2D(s_texture, vec2(20.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits21 = texture2D(s_texture, vec2(21.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits22 = texture2D(s_texture, vec2(22.0 / 23.0, v_texCoord.t));\n" //should call fx()
    "           vec4 pivbits23 = texture2D(s_texture, vec2(23.0 / 23.0, v_texCoord.t));\n" //should call fx()
#endif
    "   rawimg.y *= group_ws281x;\n" //restore original t coord for screen location checking
    "   vec2 trueimg = v_texCoord.qt;\n"
    "   vec2 pivotimg = v_texCoord.pt;\n"
//    "   remap.x *= 2.0;\n" //show original texture, hide pivot data
//    "   remap.z = remap.z * 2.0 - 1.0;\n" //show pivoted texture, hide original
//compensate for last node bit partially off-screen:
//    "   remap.x *= HSCALE;\n"
//    "   remap.z *= HSCALE;\n"
//    "   vec3 nodemask = texture2D(s_texture, vec2(v_texCoord.x, MASKROW)).rgb;\n"
    "   float nodebit = floor(rawimg.s * UNDERSCAN.s * NODE_BITS);\n" //* PERNODE);\n" //bit# within node; last bit is mostly off-screen (during h sync period); use floor to control rounding (RPi was rounding UP sometimes)
    "   float bitangle = rawimg.s * UNDERSCAN.s * NODE_BITS - nodebit;\n" //position within current bit timeslot
    "   float nodemask = pow(0.5, mod(nodebit, 8.0) + 1.0); \n" //BITMASK(nodebit);
    "   if ((nodebit == 0.0) || (nodebit == 8.0) || (nodebit == 16.0)) nodemask = 0.5;\n" //kludge: arithmetic bad on RPi?
//send non-WS281X data:
    "   if (!ENABLED) outcolor = texture2D(s_texture, v_texCoord.st);\n" //show texture as-is
//paranoid checking:
    "   else if ((nodebit < 0.0) || (nodebit > 23.0)) outcolor = ERROR(1);\n" //logic error; shouldn't happen
    "   else if ((bitangle < 0.0) || (bitangle > 1.0)) outcolor = ERROR(1);\n" //logic error; shouldn't happen
    "   else if ((nodemask < 0.0039) || (nodemask > 0.5)) outcolor = ERROR(1);\n"
    "   else if ((nodebit < 8.0) && (pow(0.5, nodebit + 1.0) < 0.003)) outcolor = ERROR(1);\n"
//    "   else if ((nodemask < 1.0/64.0) || (nodemask > 0.5 + FUD)) outcolor = ERROR(1);\n"
#ifdef SHADER_DEBUG
//show useful debug info (horizontal areas):
//    "   else if (outcolor.a != 1.0) outcolor = ERROR(2);\n" //caller should set full alpha
//    "   else if (!chkcolor(outcolor)) outcolor = ERROR(3);\n" //data integrity check during debug
    "   else if (OVERLAY(rawimg)) outcolor = overlay(rawimg);\n" //debug overlay
//"else if ((rawimg.y >= 0.45) && (rawimg.y < 0.5)) outcolor = RGB((test != 0)? 1.0: 0.0);\n"
    "   else if ((rawimg.y >= 0.5) && (rawimg.y < 0.51) && ((nodebit == 1.0) || (nodebit == 8.0) || (nodebit == 22.0))) outcolor = RGB(0.75);\n"
    "   else if ((rawimg.y >= 0.5) && (rawimg.y < 0.54)) outcolor = gray(nodebit / NODE_BITS);\n"
    "   else if ((rawimg.y >= 0.54) && (rawimg.y < 0.58)) outcolor = gray(1.0 - nodebit / NODE_BITS);\n"
    "   else if ((rawimg.y >= 0.58) && (rawimg.y < 0.6)) outcolor = RGB(1.0);\n"
    "   else if ((rawimg.y >= 0.6) && (rawimg.y < 0.65)) outcolor = RGB(bitangle);\n"
    "   else if ((rawimg.y >= 0.65) && (rawimg.y < 0.7)) outcolor = RGB(nodemask);\n" //scalar2rgb(nodemask);\n"
//    "   else if ((rawimg.y >= 0.7) && (rawimg.y < 0.75) && (mod(nodebit, 8.0) == 0.0)) outcolor = ERROR(1);\n"
#if 0
    "#define EVAL  nodemask\n"
    "   else if ((rawimg.y >= 0.7) && (rawimg.y < 0.71) && (EVAL < 0.0001)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.71) && (rawimg.y < 0.72) && (EVAL < 0.0005)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.72) && (rawimg.y < 0.73) && (EVAL < 0.001)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.73) && (rawimg.y < 0.74) && (EVAL < 0.005)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.74) && (rawimg.y < 0.75) && (EVAL < 0.01)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.75) && (rawimg.y < 0.76) && (EVAL < 0.05)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.76) && (rawimg.y < 0.77) && (EVAL < 0.1)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.77) && (rawimg.y < 0.78) && (EVAL < 0.5)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.78) && (rawimg.y < 0.79) && (EVAL >= 0.5)) outcolor = ERROR(1);\n"
    "   else if ((rawimg.y >= 0.79) && (rawimg.y < 0.8) && (EVAL >= 1.0)) outcolor = ERROR(1);\n"
#endif
#ifdef HWMUX //RPI_NO_X //drive external mux SR (SN74HC595) using RPi GPIO or non-Pi ADC
    "   else if ((rawimg.y >= 0.7) && (rawimg.y < 0.8)) outcolor = hwmux_debug(rawimg);\n"
#endif
#endif //def SHADER_DEBUG
//generate timing signals (vertical areas):
//leader (on), encoded data, trailer (off), original color (debug only)
//    "   else if (ISDEBUG) outcolor = texture2D(s_texture, trueimg * UNDERSCAN);\n" //original rendering; (S, T) swizzle
//    "   else if (!ENABLED) outcolor = texture2D(s_texture, pivotimg * UNDERSCAN);\n" //original rendering; (S, T) swizzle
    "   else if (bitangle < DWSTART) outcolor = ISDEBUG? RGB(1.0): ALLON;\n" //WS281X bits start high
//#ifdef SHADER_DEBUG
//    "   else if (bitangle > 0.75) outcolor = texture2D(s_texture, trueimg * UNDERSCAN);\n" //show pixel (DEBUG)
//    "   else if (bitangle > DWEND) outcolor = gray(0.2);\n" //off (DEBUG)
//#endif //def SHADER_DEBUG
    "   else if (ISDEBUG && (bitangle >= 0.65)) outcolor = ((bitangle >= 0.7) && (bitangle <= 0.95))? texture2D(s_texture, trueimg * UNDERSCAN): ALLOFF;\n" //original rendering; (S, T) swizzle
    "   else if (bitangle > DWEND) outcolor = ALLOFF;\n" //WS281X bits end low
//    "   else if (!ENABLED) outcolor.rgb = texture2D(s_texture, v_texCoord).rgb;\n" //normal rendering overlayed with start/stop bars (for debug)
//build output bit buffer to send real data:
#ifdef CPU_PIVOT //Pi GPU can't perform multiple texture lookups, so pivot must be done on CPU
    "   else outcolor = texture2D(s_texture, pivotimg * UNDERSCAN);\n" //send real node bit values (pivoted); pivoted (S, T) swizzle
#else
    "   else\n" //GPU pivot or hardware demux
    "   {\n"
#ifdef HWMUX //RPI_NO_X //experimental; drive external mux SR (SN74HC595) using RPi VGA GPIO or non-Pi ADC
    "       float x = round(1536.0 * UNDERSCAN.s * coord.x);\n" //[0..1488)
//TODO: finish this code
    "       if (x < 2.0*8.0)\n" //only do 8 bits (1 SR) for now
    "       {\n"
    "           if (LSB(x))\n" //clock
    "               outcolor = (y < floor(x / 2.0) / 8.0)? vec4(1.0, 1.0, 0.0, 1.0): ALLOFF;\n"
    "           else\n" //data
    "               outcolor = (y < floor(x / 2.0) / 8.0)? vec4(0.0, 1.0, 1.0, 1.0): ALLOFF;\n"
    "       }\n"
    "       else if (x == 2.0*8.0) outcolor = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "       else outcolor = ERROR(1);\n"
#else
//    "       nodemask = pow(2.0, 23.0 - nodebit); \n" //BITMASK(nodebit);
    "       rawimg.y /= group_ws281x;\n" //switch back to stretched (grouped) location
//    "       outcolor = vec4(0.0, 0.0, 0.0, 0.0);\n"
//    "       vec3 nodemaskbits = vec3((nodebit < 8.0)? nodemask: 0.0, (nodebit bitmask(nodebit);\n"
//NOTE: this isn't quite right yet
#if 1
    "       for (float pivbit = 0.0; pivbit < 24.0; ++pivbit)\n"
    "       {\n"
//    "   float nodemask = pow(0.5, mod(nodebit, 8.0) + 1.0); \n" //BITMASK(nodebit);
//    "           float bitmask = pow(2.0, 23.0 - bit); \n" //BITMASK(nodebit);
    "           vec2 pivot_coord = vec2(pivbit / 23.0, rawimg.y);\n"
    "           vec4 pivbits = texture2D(s_texture, pivot_coord);\n" //should call fx()
//    "           vec4 pivbits = vec4(pivot_coord.x, pivot_coord.y, pivot_coord.x + pivot_coord.y, 1.0);\n" //texture2D(s_texture, pivot_coord);\n"
//    "           if (and(bits, nodemask)) outcolor = or(outcolor, bitmask);\n"
    "           if (AND(pivbits, nodebit)) outcolor = OR(outcolor, pivbit);\n"
//    "           outcolor = bits;\n"
//"if (pivbit > 0.0) break;\n"
    "       }\n"
#else
        "   outcolor = ERROR(1);\n"
#if 0
"           if (AND(pivbits0, nodebit)) outcolor = OR(outcolor, 0.0);\n"
"           if (AND(pivbits1, nodebit)) outcolor = OR(outcolor, 1.0);\n"
"           if (AND(pivbits2, nodebit)) outcolor = OR(outcolor, 2.0);\n"
"           if (AND(pivbits3, nodebit)) outcolor = OR(outcolor, 3.0);\n"
"           if (AND(pivbits4, nodebit)) outcolor = OR(outcolor, 4.0);\n"
"           if (AND(pivbits5, nodebit)) outcolor = OR(outcolor, 5.0);\n"
"           if (AND(pivbits6, nodebit)) outcolor = OR(outcolor, 6.0);\n"
"           if (AND(pivbits7, nodebit)) outcolor = OR(outcolor, 7.0);\n"
"           if (AND(pivbits8, nodebit)) outcolor = OR(outcolor, 8.0);\n"
"           if (AND(pivbits9, nodebit)) outcolor = OR(outcolor, 9.0);\n"
"           if (AND(pivbits10, nodebit)) outcolor = OR(outcolor, 10.0);\n"
"           if (AND(pivbits11, nodebit)) outcolor = OR(outcolor, 11.0);\n"
"           if (AND(pivbits12, nodebit)) outcolor = OR(outcolor, 12.0);\n"
"           if (AND(pivbits13, nodebit)) outcolor = OR(outcolor, 13.0);\n"
"           if (AND(pivbits14, nodebit)) outcolor = OR(outcolor, 14.0);\n"
"           if (AND(pivbits15, nodebit)) outcolor = OR(outcolor, 15.0);\n"
"           if (AND(pivbits16, nodebit)) outcolor = OR(outcolor, 16.0);\n"
"           if (AND(pivbits17, nodebit)) outcolor = OR(outcolor, 17.0);\n"
"           if (AND(pivbits18, nodebit)) outcolor = OR(outcolor, 18.0);\n"
"           if (AND(pivbits19, nodebit)) outcolor = OR(outcolor, 19.0);\n"
"           if (AND(pivbits20, nodebit)) outcolor = OR(outcolor, 20.0);\n"
"           if (AND(pivbits21, nodebit)) outcolor = OR(outcolor, 21.0);\n"
"           if (AND(pivbits22, nodebit)) outcolor = OR(outcolor, 22.0);\n"
"           if (AND(pivbits23, nodebit)) outcolor = OR(outcolor, 23.0);\n"
#endif
#endif
#endif //def RPI_NO_X
    "   }\n"
#endif //def CPU_PIVOT
//    "   outcolor = texture2D(s_texture, v_texCoord.xy);\n" //original multi-rendering
//    "   outcolor = texture2D(s_texture, remap.xy);\n" //pivot data
//    "   outcolor = texture2D(s_texture, remap.zy);\n" //pivot data
    "   outcolor.a = 1.0;\n" //make sure we can see it (paranoid)
    "   gl_FragColor = outcolor;\n"
	"}\n"
//apply fx:
    "vec4 fx(vec4 color)\n"
    "{\n"
//TODO    "           color = pow(color, vec3(2.2));\n" //gamma correction, other fx here
//TODO: other fx as desired, maybe controlled by uniforms or other run-time parameters
//NOTE: must operate on non-pivoted values
    "   return color;\n"
    "}\n"
    "bool AND(vec4 bits, float bit) //mask)\n" //bit-wise AND; NOTE: only checks lsb of mask
    "{\n"
//    "   if (mask < 256.0) return LSB(255.0 * bits.b / mask);\n"
//    "   mask = floor(mask / 256.0);\n"
//    "   if (mask < 256.0) return LSB(255.0 * bits.g / mask);\n"
//    "   mask = floor(mask / 256.0);\n"
//    "   if (mask < 256.0) return LSB(255.0 * bits.r / mask);\n"
//    "   return true;\n"
    "   if (bit < 0.0) return true;\n" //bug: should not happen
    "   if (bit < 8.0) return LSB(255.0 * bits.r / pow(2.0, 7.0 - bit));\n"
    "   if (bit < 16.0) return LSB(255.0 * bits.g / pow(2.0, 15.0 - bit));\n"
    "   if (bit < 24.0) return LSB(255.0 * bits.b / pow(2.0, 23.0 - bit));\n"
    "   return true;\n" //bug: should not happen
    "}\n"
    "vec4 OR(vec4 bits, float bit) //mask)\n" //bit-wise OR; NOTE: assumes target bit is off to start
    "{\n"
//    "   if (mask < 256.0) return bits + vec4(0.0, 0.0, mask / 256.0, 0.0);\n"
//    "   mask = floor(mask / 256.0);\n"
//    "   if (mask < 256.0) return bits + vec4(0.0, mask / 256.0, 0.0, 0.0);\n"
//    "   mask = floor(mask / 256.0);\n"
//    "   if (mask < 256.0) return bits + vec4(mask / 256.0, 0.0, 0.0, 0.0);\n"
    "   if (bit < 0.0) bits = ERROR(1);\n" //bug: should not happen
    "   else if (bit < 8.0) bits += pow(0.5, bit + 1.0);\n"
    "   else if (bit < 16.0) bits += pow(0.5, bit - 7.0);\n"
    "   else if (bit < 24.0) bits += pow(0.5, bit - 15.0);\n"
    "   else bits = ERROR(1);\n" //bug: should not happen
    "   return bits;\n"
    "}\n"
    "vec4 _RGB(float val, float which)\n" //this is for debug, but leave it in so debug can be turned on/off at run-time
    "{\n"
//    "   if (which == 3.0) return vec4(0.0, 1.0, 1.0, 1.0);\n"
    "   if ((which != 0.0) && (which != 1.0) && (which != 2.0)) return ERROR(4);\n"
    "   return vec4((which == 0.0)? val: 0.0, (which == 1.0)? val: 0.0, (which == 2.0)? val: 0.0, 1.0);\n" //show R/G/B for debug
    "}\n"
    "bool LSB(float val)\n"
    "{\n"
    "   val = floor(val);\n"
    "   return (floor((val) / 2.0) != (val) / 2.0);\n"
#ifdef SHADER_DEBUG
    "}\n"
//DEBUG stuff:
    "vec4 hwmux_debug(vec2 coord)\n"
    "{\n"
    "   float y = 10.0 * (coord.y - 0.7);\n" //[0..1)
    "   float x = 66.0; //round(1536.0 * UNDERSCAN.s * coord.x);\n" //[0..1488)
//    "   return vec4(0.0, coord.x, 0.0, 1.0);\n"
//    "   return vec4(0.0, y, 0.0, 1.0);\n"
//    "   return (y < coord.x)? vec4(0.0, 1.0, 1.0, 1.0): ALLOFF;\n"
//    "   float univ = floor(NUM_UNIV * coord.x);\n"
//    "   if (mod(x, 4.0) == 0.0) return vec4(1.0, 0.0, 0.0, 1.0);\n" //r
//    "   if (mod(x, 4.0) == 1.0) return vec4(0.0, 1.0, 0.0, 1.0);\n" //g
//    "   if (mod(x, 4.0) == 2.0) return vec4(0.0, 0.0, 1.0, 1.0);\n" //b
//    "   if (mod(x, 4.0) == 3.0) return vec4(1.0, 1.0, 0.0, 1.0);\n" //y
//    "   return ERROR(1);\n"
    "   if (x < 2.0*8.0)\n" //only do 8 bits (1 SR) for now
    "   {\n"
    "       if (LSB(x))\n" //clock
    "           return (y < floor(x / 2.0) / 8.0)? vec4(1.0, 1.0, 0.0, 1.0): ALLOFF;\n"
    "       else\n" //data
    "           return (y < floor(x / 2.0) / 8.0)? vec4(0.0, 1.0, 1.0, 1.0): ALLOFF;\n"
    "   }\n"
    "   else if (x == 2.0*8.0) return ERROR(1);\n"
    "   else return ALLOFF;\n"
    "}\n"
    "bool chkcolor(vec4 color)\n"
    "{\n"
    "   float bits = 0.0;\n"
    "   float val = rgb2scalar(color);\n"
    "   for (int i = 0; i < 32; ++i)\n"
    "      if (LSB(val)) { ++bits; val = floor(val / 2.0); }\n" //count bits on
    "   return !LSB(bits);\n" //even parity
    "}\n"
    "vec4 gray(float val)\n"
    "{\n"
    "   return vec4(val, val, val, 1.0);\n"
    "}\n"
//ERROR code legend:
//  1 | 3
// ---+---
//  2 | 4
    "vec4 overlay(vec2 coord)\n"
    "{\n"
    "   vec2 ofs = (coord.st - vec2(OVLX, OVLY)) / vec2(1.0 - OVLX, 1.0 - OVLY);\n" //relative position
    "   if ((ofs.x < BORDERW) || (ofs.x > 1.0 - BORDERW) || (ofs.y < BORDERW) || (ofs.y > 1.0 - BORDERW)) return vec4(0.0, 1.0, 0.0, 1.0);\n"
    "   if ((ofs.x < 2.0 * BORDERW) || (ofs.x > 1.0 - 2.0 * BORDERW) || (ofs.y < 2.0 * BORDERW) || (ofs.y > 1.0 - 2.0 * BORDERW)) return vec4(0.0, 0.0, 0.0, 1.0);\n"
    "   return ERROR((ofs.x < 0.5)? ((ofs.y < 0.5)? 1: 2): ((ofs.y < 0.5)? 3: 4));\n"
#endif
    "}\n";

    SHOW_CONFIG(printf("compiling shaders:\n"));
    SHOW_CONFIG(printf("-----\n%s\n", linenums(vShaderStr)));
    SHOW_CONFIG(printf("-----\n%s\n-----\n", linenums(fShaderStr)));
	state.programObject = progcre(vShaderStr, fShaderStr);
	if (!state.programObject) return why(FALSE, "create GL program failed");
//get attribute, sampler locations:
	state.positionLoc = glGetAttribLocation(state.programObject, "a_position"); ERRCHK("positin");
	state.texCoordLoc = glGetAttribLocation(state.programObject, "a_texCoord"); ERRCHK("txcoord");
	state.samplerLoc = glGetUniformLocation(state.programObject, "s_texture"); ERRCHK("txtr");
//	state.samplerLoc2 = glGetUniformLocation(state.programObject, "s_texture2"); ERRCHK("txtr2");
//	state.samplerLoc3 = glGetUniformLocation(state.programObject, "s_texture3"); ERRCHK("txtr3");
//	state.samplerLoc4 = glGetUniformLocation(state.programObject, "s_texture4"); ERRCHK("txtr4");
//    state.hscaleLoc = glGetUniformLocation(state.programObject, "HSCALE");
	state.wsoutLoc = glGetUniformLocation(state.programObject, "want_ws281x"); ERRCHK("want");
	state.grpLoc = glGetUniformLocation(state.programObject, "group_ws281x"); ERRCHK("group");
	glUseProgram(state.programObject); //now use shaders
//load the texture:
//	state.textureId = texcre();
//	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	ERRCHK("shader");
	return TRUE;
}


//create program object to compile+link shaders:
GLuint progcre(const char *vertShaderSrc, const char *fragShaderSrc)
{
	GLuint vertexShader = shadecre(GL_VERTEX_SHADER, vertShaderSrc);
	if (!vertexShader) return 0;
	GLuint fragmentShader = shadecre(GL_FRAGMENT_SHADER, fragShaderSrc);
	if (!fragmentShader)
	{
		glDeleteShader(vertexShader);
		return 0;
	}
//TODO: fix mem leaks; wrap in classes with dtors
	GLuint programObject = glCreateProgram();
	if (!programObject) return 0;

	glAttachShader(programObject, vertexShader); ERRCHK("prog");
	glAttachShader(programObject, fragmentShader); ERRCHK("prog");
	glLinkProgram(programObject); ERRCHK("prog");
// Check the link status
	GLint linked;
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked); ERRCHK("prog");
	if (!linked) 
	{
		GLint infoLen = 0;
		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char* infoLog =(char*)malloc(sizeof(char) * infoLen);
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			logmsg("Error linking program:\n%s\n", infoLog);            
			free(infoLog);
		}
		glDeleteProgram(programObject);
		return 0;
	}
// Free up no longer needed shader resources:
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	return programObject;
}


//load a shader:
//check for compile errors, print error messages to output log
// \param type Type of shader(GL_VERTEX_SHADER or GL_FRAGMENT_SHADER)
/// \param shaderSrc Shader source string
/// \return A new shader object on success, 0 on failure
GLuint shadecre(GLenum type, const char *shaderSrc)
{
	GLuint shader = glCreateShader(type);
	if (!shader) return 0;
	glShaderSource(shader, 1, &shaderSrc, NULL); ERRCHK("shader"); //load shader source
	glCompileShader(shader); ERRCHK("shader");

// Check the compile status:
	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled) return shader;
	GLint infoLen = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
	if (infoLen > 1)
	{
		char* infoLog =(char*)malloc(sizeof(char) * infoLen);
		glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
		logmsg("Error compiling shader:\n%s\n", infoLog);
		free(infoLog);
	}
	glDeleteShader(shader);
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
////
/// node.js interface
//

//see https://nodejs.org/api/addons.html
//and http://stackabuse.com/how-to-create-c-cpp-addons-in-node/
//parameter passing:
// http://www.puritys.me/docs-blog/article-286-How-to-pass-the-paramater-of-Node.js-or-io.js-into-native-C/C++-function..html
// http://luismreis.github.io/node-bindings-guide/docs/arguments.html
// https://github.com/nodejs/nan/blob/master/doc/methods.md

//build:
// npm install -g node-gyp
// npm install --save nan
// node-gyp configure
// node-gyp build

#ifdef NODEJS_ADDON //selected from gyp bindings
// using namespace Nan;
// using namespace v8;
// using namespace std;

MyTexture<NUM_UNIV, UNIV_LEN>* LEDs = 0; //don't create until needed

namespace //anonymous namespace wrapper for Node.js functions
{

#define GetOptionalArg(args, inx, v8type, defval)  \
	((args.Length() <= inx)? defval: args[inx]->v8type##Value())
//#define GetOptionalBool(args, inx, defval)  GetOptionalArg(args, inx, Boolean, defval)
//#define GetOptionalFloat(args, inx, defval)  GetOptionalArg(args, inx, Number, defval)

bool GetOptionalBool(const Nan::FunctionCallbackInfo<v8::Value>& args, int argi, bool defval = false)
{
    return GetOptionalArg(args, argi, Boolean, defval);
//	if (args.Length() <= argi) return defval;
////	if (!args[argi]->IsNumber()) Nan::ThrowTypeError("Pixel: arg should be number");
//	return args[argi]->BooleanValue(); //NumberValue();
}
float GetOptionalFloat(const Nan::FunctionCallbackInfo<v8::Value>& args, int argi, float defval = 0)
{
    return GetOptionalArg(args, argi, Number, defval);
}

int GetOptionalInt(const Nan::FunctionCallbackInfo<v8::Value>& args, int argi, int defval = 0)
{
    return GetOptionalArg(args, argi, Number, defval);
}


//frame buffer control:
//frame buf data from caller contains:
// [0] = magic/validate value
// [1] = delay time (0 to reset epoch and/or flush immediately)
// [2..W*H+2] = pixels
//TODO: move this to state struct
//int nxt_ofs = -2;
//uint32_t fbhdr[2], started;
//uv_timer_t frbuf_delay;

#define FBUFST  0x59414c50 //"YALP" start of frame marker; checks stream integrity, as well as byte order


//libuv info:
//https://nikhilm.github.io/uvbook/basics.html
//https://media.readthedocs.org/pdf/libuv/v1.x/libuv.pdf
//http://nikhilm.github.io/uvbook/utilities.html#timers

//#define now()  (uv_hrtime() / 1000000)

//inline void timer_init()
//{
//    init();
//    uv_timer_init(uv_default_loop(), &frbuf_delay);
////printf("timer init\n");
//}
//#undef init
//#define init()  timer_init()


//send colors to LEDs:
//if current frame buf is marked for delay, wait before sending
//bool send() //bool flush)
/*
bool send() //uv_timer_s* timer = 0)
{
//	if (!flush) return true; //false?
//    if (nxt_ofs == -2) return false; //nothing to send
//TODO: add checksum if needed
    bool ok = true; //= (fbhdr[0] == FBUFST);
//++state.draws;
//    if (!ok) { printf("bad frbuf[%d] hdr: 0x%x, len %d\n", state.draws, fbhdr[0], nxt_ofs); return false; }

    LEDs.render();
    LEDs.flush();
//    nxt_ofs = -2;
//TODO: call message loop?
//	args.GetReturnValue().Set(ok);
	return ok;
}
*/


#if 0
//write next pixel to WS281X frame buffer:
//accepts a continuous stream of data
//flushes each time buffer is full
void write281x(uint32_t argb)
{
    uv_loop_t* loop = uv_default_loop();
    if (nxt_ofs < 0) //frame header
    {
        if (nxt_ofs == -2) started = uv_now(loop); //remember when frame buf started filling
        fbhdr[nxt_ofs++ + 2] = argb;
        return;
    }
    int x = nxt_ofs / UNIV_LEN, y = nxt_ofs % UNIV_LEN;
	uint32_t abgr = ARGB2ABGR(argb); //xlate to internal color format
    uint32_t color = limit(abgr); //limit power to 85%
	blend(LEDs.mpixels[XY(x, y)], color); //use alpha to mix
    if (++nxt_ofs < UNIV_LEN * NUM_UNIV) return;
//    nxt_ofs = -2;
//    if (++nxt_x < UNIV_LEN) return;
//    nxt_x = 0;
//    if (++nxt_y < NUM_UNIV) return;
//    nxt_y = 0;
//    LEDs.need_pivot(true);
    if (!fbhdr[1]) send(); //flush immediately
    else //delay (msec)
        uv_timer_start(&frbuf_delay, (uv_timer_cb)send, started + fbhdr[1] - uv_now(loop), 0);
//TODO: collect timing stats, uv_update_time to get up-to-date info
}
#endif


//send to stdout in case errors not caught in node.js:
int noderr(const char* errmsg, ...)
{
	va_list params;
	char buf[BUFSIZ];
	va_start(params, errmsg);
	vsprintf(buf, errmsg, params); //TODO: send to stderr?
	va_end(params);
//	printf("THROW: %s\n", buf);
	Nan::ThrowTypeError(buf);
    return 0; //dummy ret to match printf
}


///////////////////////////////////////////////////////////////////////////////
////
/// custom api:
//

int (*clip_warn)(const char*, ...);
int silent(const char* ignored, ...) { return 0; }

int warn(const char* errmsg, ...)
{
    static std::unordered_map<const char*, int> seen;
    if (++seen[errmsg] > 10) return 0;

	va_list params;
	char buf[BUFSIZ];
	va_start(params, errmsg);
	int retval = vsprintf(buf, errmsg, params); //TODO: send to stderr?
	va_end(params);
	printf("WARNING: %s\n", buf);
    if (seen[errmsg] == 10) printf("(ignoring further warnings of this type)\n");
    return retval; //dummy ret to match printf
}


inline void morestate_init()
{
    init();
    state.wsout = WS281X_SHADER; //1;
    state.group = 1.0;
    state.autoclip = false; //don't allow
    clip_warn = noderr;
}
#undef init
#define init()  morestate_init()


//enable WS281X formatting in shader:
void wsout_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
	int tristate = GetOptionalInt(args, 0, true); //Bool(args, 0, true);
//printf("wsoutWS281X: %d\n", tristate);
//    if (LEDs) LEDs->wsout(tristate);
    state.wsout = tristate; //allow it to be set before LEDs instantiated
	args.GetReturnValue().Set(state.wsout);
}

//Handle<v8::Value> Getter( Local<v8::String> property, const AccessorInfo& info ) 
/*
NAN_GETTER(wsout_getter)
{
//    auto person = Nan::ObjectWrap::Unwrap<NanPerson>(info.Holder());
//    auto name = Nan::New(person->name).ToLocalChecked();
    info.GetReturnValue().Set(LEDs.latest_wsout);
//    HandleScope scope;
//    v8::Local<v8::Value> retval = Nan::New(LEDs.latest_wsout);
//    return retval; //handle_scope.Close(obj); 
}

NAN_SETTER(wsout_setter)
{
//    auto person = Nan::ObjectWrap::Unwrap<NanPerson>(info.Holder());
//    // [NOTE] `value` is defined argument in `NAN_SETTER`
//    auto name = Nan::To<v8::String>(value).ToLocalChecked();
//    person->name = *Nan::Utf8String(name);
	int tristate = 1; //GetOptionalInt(info, 0, true); //Bool(args, 0, true);
//printf("wsoutWS281X: %d\n", tristate);
    LEDs.wsout(tristate);
	info.GetReturnValue().Set(tristate);
}

v8::Handle<v8::Value> wsout_getter(v8::Local<v8::Number> property, const v8::AccessorInfo& info)
{
//    Box* boxInstance = ObjectWrap::Unwrap<Box>(info.Holder());
//    Local<Object> size = Object::New();
//    size->Set(String::New("width"), Integer::New(boxInstance->width));
//    size->Set(String::New("height"), Integer::New(boxInstance->height));
    v8::Local<v8::Number> retval = Nan::New(LEDs.latest_wsout);
    return retval;
}

v8::Handle<v8::Value> wsout_setter(v8::Local<v8::Number> property, 
    v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
//	if (!args[0]->IsNumber()) Nan::ThrowTypeError("Pixel: 1st arg should be number");
	int tristate = value->Int32Value(); //NumberValue();
//printf("wsoutWS281X: %d\n", tristate);
    LEDs.wsout(tristate);
}
*/


//pixel grouping in shader:
void group_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
	float grp = GetOptionalFloat(args, 0, 1); //Number(args, 0, 1);
//printf("groupWS281X: %f\n", grp);
//    if (LEDs) LEDs->group(grp);
    state.group = grp; //allow it to be set before LEDs instantiated
	args.GetReturnValue().Set(grp);
}

//auto-clip coordinates:
void autoclip_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
	int onoff = GetOptionalInt(args, 0, true); //Bool(args, 0, true);
    state.autoclip = onoff; //allow it to be set before LEDs instantiated
    clip_warn = !onoff? noderr: (onoff < 0)? warn: silent;
	args.GetReturnValue().Set(onoff);
}


//return #"universes":
//void Width_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
//{
//	args.GetReturnValue().Set(UNIV_LEN);
//}

//return max universe size:
//void Height_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
//{
//	args.GetReturnValue().Set(NUM_UNIV);
//}


//set colors for all LEDs:
void render_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
//    if (!LEDs) LEDs = new MyTexture<NUM_UNIV, UNIV_LEN>;
    if (!LEDs) { noderr("LED buffer must be opened first"); return; }

    static int prev_outlen = 0, prev_inlen = 0;
	int outlen = 0, inlen = 0;
	if (args.Length()) //copy data to buffer before sending
	{
		if (!args[0]->IsArray()) { noderr("Render: outer argument not array"); return; }
//		v8::Handle<v8::Array> jsArray = v8::Handle<v8::Array>::Cast(args[0]);
		v8::Local<v8::Array> outer = v8::Local<v8::Array>::Cast(args[0]);
		outlen = outer->Length();
		if (outer->Length() > UNIV_LEN)
        {
            clip_warn("Render: outer array too long: %d (max is %d)", outer->Length(), UNIV_LEN);
//            if (!autoclip) return;
            outlen = UNIV_LEN;
        }
//		bool is2D = (outer->Length() == UNIV_LEN);
		for (int x = 0; x < outlen; ++x)
		{
			v8::Local<v8::Value> val = outer->Get(x);
			if (!val->IsArray()) { noderr("Render: inner argument not array"); return; }
			v8::Local<v8::Array> inner = v8::Local<v8::Array>::Cast(val);
			inlen = inner->Length();
			if (inner->Length() > NUM_UNIV)
            {
                clip_warn("Render: inner array[%d] too long: %d (max is %d)", x, inner->Length(), NUM_UNIV);
//                if (!autoclip) return;
                inlen = NUM_UNIV;
            }
			for (int y = 0; y < inlen; ++y)
			{
				val = inner->Get(y);
//				v8::Local<v8::Value> element = v8::Local<v8::Number>::Cast(val);
				uint32_t color = ARGB2ABGR(val->Uint32Value());
//if (!x && !y) printf("render[%d, %d]: blend 0x%x with 0x%x,", x, y, LEDs->mpixels[XY(x, y)], limit(color));
				blend(LEDs->mpixels[XY(x, y)], limit(color));
//if (!x && !y) printf(" got 0x%x\n", LEDs->mpixels[XY(x, y)]);
                IFPIVOT(LEDs->dirty_pivot[x / NUM_GPIO][y] = true);
//printf("got ary[%d/%d][%d/%d] = 0x%x\n", x, UNIV_LEN, y, NUM_UNIV, color);
			}
		}
	}
    if ((outlen != prev_outlen) || (inlen != prev_inlen))
    	printf("render got ary %d x %d\n", outlen, inlen);
    prev_outlen = outlen;
    prev_inlen = inlen;
	args.GetReturnValue().Set(LEDs->render() && LEDs->flush());
}


//set all LEDs or rect to a color:
void fill_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
//	if (args.Length() ...)
//	if (!args[0]->IsNumber()) {
//	        Nan::ThrowTypeError("Both arguments should be numbers");
//	        return;
//	v8::Isolate* isolate = args.GetIsolate();
//	args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "world"));
//	if (!args[0]->IsNumber()) Nan::ThrowTypeError("SetAll: argument should be number");
//	        return;

    bool flush;
    GLuint color;
    int xofs, yofs, w, h;
//    if (!LEDs) LEDs = new MyTexture<NUM_UNIV, UNIV_LEN>;
    if (!LEDs) { noderr("LED buffer must be opened first"); return; }
    if (args.Length() >= 4) //rect
    {
    	xofs = args[0]->Uint32Value(); //NumberValue();
    	yofs = args[1]->Uint32Value(); //NumberValue();
    	w = args[2]->Uint32Value(); //NumberValue();
    	h = args[3]->Uint32Value(); //NumberValue();
    	color = (args.Length() > 4)? ARGB2ABGR(args[4]->Uint32Value()): BLACK; //NumberValue();
    	flush = GetOptionalBool(args, 5);
//clipping:
        if (xofs < 0)
        {
            clip_warn("X = %d out of range [0..%d)", xofs, state.width);
            w += xofs; xofs = 0;
        }
        if (xofs >= state.width)
        {
            clip_warn("X = %d out of range [0..%d)", xofs, state.width);
            return; //nothing to do
        }
        if (w > state.width - xofs)
        {
            clip_warn("W = %d out of range [0..%d)", w, state.width - xofs);
            w = state.width - xofs;
        }
        if (w < 1) return; //nothing to do

        if (yofs < 0)
        {
            clip_warn("Y = %d out of range [0..%d)", yofs, state.height);
            h += yofs; yofs = 0;
        }
        if (yofs >= state.height)
        {
            clip_warn("H = %d out of range [0..%d)", yofs, state.height);
            return; //nothing to do
        }
        if (h > state.height - yofs)
        {
            clip_warn("H = %d out of range [0..%d)", h, state.height - yofs);
            h = state.height - yofs;
        }
        if (h < 1) return; //nothing to do
    }
    else //all
    {
        xofs = yofs = 0;
        w = state.width;
        h = state.height;
    	color = args.Length()? ARGB2ABGR(args[0]->Uint32Value()): BLACK; //NumberValue();
//	bool
//	v8::Local<v8::Number> num = Nan::New(color + 3);
//	if (args.Length() > 1)
//		if (!args[1]->IsNumber()) Nan::ThrowTypeError("SetAll: 2nd arg should be number");
    	flush = GetOptionalBool(args, 1);
    }

//fill rect or all:
printf("fill: color 0x%x, x %d, y %d, w %d, h %d, flush? %d\n", limit(color), xofs, yofs, w, h, flush);
    if (args.Length() <= 2) LEDs->fill(limit(color)); //all
    else //rect
    {
        for (int x = xofs; x < xofs + w; ++x)
            for (int y = yofs; y < yofs + h; ++y)
        		blend(LEDs->mpixels[XY(x, y)], limit(color));
#ifdef CPU_PIVOT
        for (int x = xofs; x < xofs + w; x += NUM_GPIO)
            for (int y = yofs; y < yofs + h; ++y)
                LEDs->dirty_pivot[x / NUM_GPIO][y] = true;
#endif
    }

	args.GetReturnValue().Set(flush && LEDs->render() && LEDs->flush());
}

//TODO: add line

//set color for 1 LED:
//NOTE: only updates in-memory copy unless flush parameter specified
//returns current color if none specified
void pixel_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
//    if (!LEDs) LEDs = new MyTexture<NUM_UNIV, UNIV_LEN>;
    if (!LEDs) { noderr("LED buffer must be opened first"); return; }

	if (args.Length() < 2) { noderr("Pixel: missing x/y index"); return; }
//	if (!args[0]->IsNumber()) Nan::ThrowTypeError("Pixel: 1st arg should be number");
	int x = args[0]->Uint32Value(); //NumberValue();
//	if (!args[1]->IsNumber()) Nan::ThrowTypeError("Pixel: 2nd arg should be number");
	int y = args[1]->Uint32Value(); //NumberValue();

	if ((x < 0) || (x >= state.width)) //NUM_UNIV))
    {
        clip_warn("X = %d out of range [0..%d)", x, state.width); //NUM_UNIV);
        return;
    }
	if ((y < 0) || (y >= state.height)) //UNIV_LEN))
    {
        clip_warn("Y = %d out of range [0..%d)", y, state.height); //UNIV_LEN);
        return;
    }
	if (args.Length() > 2) //set pixel color
	{
//		if (!args[2]->IsNumber()) Nan::ThrowTypeError("Pixel: 3rd arg should be number");
		GLuint color = ARGB2ABGR(args[2]->Uint32Value()); //NumberValue();
		bool flush = GetOptionalBool(args, 3);
//        printf("set pixel[%d,%d]: 0x%x, flush? %d\n", x, y, color, flush);
		blend(LEDs->mpixels[XY(x, y)], limit(color));
        IFPIVOT(LEDs->dirty_pivot[x / NUM_GPIO][y] = true);
		args.GetReturnValue().Set(flush && LEDs->render() && LEDs->flush());
	}
	else args.GetReturnValue().Set(ARGB2ABGR(LEDs->mpixels[XY(x, y)])); //& 0xffffff); //caller doesn't care about alpha?
}


///////////////////////////////////////////////////////////////////////////////
////
/// stream interface:
//

//inline static void wrap_pointer_cb(char *data, void *hint) //called during GC
//{
  //fprintf(stderr, "wrap_pointer_cb\n");
//}
//inline static v8::Local<v8::Value> WrapPointer(void *ptr, size_t length) // * Wraps "ptr" into a new SlowBuffer instance with size "length".
//{
//  void *user_data = NULL;
//  return Nan::NewBuffer((char *)ptr, length, wrap_pointer_cb, user_data).ToLocalChecked();
//}
//inline static v8::Local<v8::Value> WrapPointer(void *ptr) //* Wraps "ptr" into a new SlowBuffer instance with length 0.
//{
//  return WrapPointer((char *)ptr, 0);
//}
inline static char * UnwrapPointer(v8::Local<v8::Value> buffer, int64_t offset = 0) // * Unwraps Buffer instance "buffer" to a C `char *` with the offset specified.
{
    return node::Buffer::HasInstance(buffer)? node::Buffer::Data(buffer.As<v8::Object>()) + offset: NULL;
}
//Templated version of UnwrapPointer that does a reinterpret_cast() on the pointer before returning it:
template <typename Type>
inline static Type UnwrapPointer(v8::Local<v8::Value> buffer)
{
  return reinterpret_cast<Type>(UnwrapPointer(buffer));
}

//from http://stackoverflow.com/questions/34356686/how-to-convert-v8string-to-const-char
const char* cstr(const v8::String::Utf8Value& value)
{
    return *value? *value: "<string conversion failed>";
}

NAN_METHOD(Open_entpt)
//void Open_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
#define args  info //kludge: NaN hard-coded names
//    Nan::HandleScope scope;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
//    v8::HandleScope scope(isolate);
//    NanScope();

//https://github.com/pkrumins/node-png/blob/master/src/png.cpp
//http://stackoverflow.com/questions/30927707/node-js-addon-how-to-pass-a-string-parameter-to-nan-c
//https://github.com/nodejs/node-addon-examples/blob/master/original_docs_source.md
//    String::Utf8Value title;
//    if (args.Length() < 1) title = "WS281X-gpu test";
//    else if (args[0]->IsString()) title = str(args[0]->ToString());
//    else { sprintf(buf, "%d", args[0]->Uint32Value()); title = buf; } //NumberValue();
//    v8::String /*::AsciiValue*/ title((args.Length() < 1)? v8::String::NewFromUtf8(isolate, "WS281X-gpu test"): args[0]->ToString();
//    v8::String /*::AsciiValue*/ title((args.Length() < 1)? Nan::New<v8::String>("WS281X-gpu test"): args[0]->ToString());
//    v8::Local<v8::String> title = (args.Length() < 1)? Nan::New<v8::String>("WS281X-gpu test"): args[0]->ToString();
//    v8::String defval = Nan::New<v8::String>(*"WS281X-gpu test");
//    v8::String defval = v8::String::New("WS281X-gpu test", 15); //int length ).
//https://groups.google.com/forum/#!topic/v8-users/2RF1QksZ_QQ
//    v8::String defval = v8::String::NewFromUTF8(v8::Isolate::GetCurrent(), "foo");

//http://stackoverflow.com/questions/16613828/how-to-convert-stdstring-to-v8s-localstring?rq=1
    v8::Local<v8::String> defval = v8::String::NewFromUtf8(isolate, ""); //"WS281X-gpu test");

    v8::String::Utf8Value title((args.Length() < 1)? defval: args[0]->ToString());
    GLint width = (args.Length() < 2)? 0: args[1]->Uint32Value(); //NumberValue();
    GLint height = (args.Length() < 3)? 0: args[2]->Uint32Value(); //NumberValue();
printf("open stream: title '%s', w %d, h %d\n", cstr(title), width, height);
//    if (!width || !height) { width = 640; height = 480; }

    LEDs = new MyTexture<NUM_UNIV, UNIV_LEN>;
	bool ok = wincre(cstr(title), width, height);
	if (ok) ok = eglcre();
//TODO?	glewInit(); //must occur after context created; BROKEN
	if (ok) ok = LEDs->setup();
	if (!ok) printf("open init failed\n");

//    mydata* ptr = UnwrapPointer<mydata*>(info[0]);
    /* TODO: async */
//    ao->flush(ao);
//    send(); //flush last (partial) buffer
    args.GetReturnValue().Set(ok);
#undef args
}


/*
//	GLuint mpixels[XY(2 * WW, H)]; //[W][H]; //x is universe#, y is node#
NAN_METHOD(Open)
{
    Nan::EscapableHandleScope scope;
//    mydata *ptr = UnwrapPointer<mydata *>(info[0]);
//    memset(ptr, 0, sizeof(*ptr));
//    ptr->x = ptr->y = 0;

//    int r = mpg123_output_module_info.init_output(ao);
//    if (!r) r = ao->open(ao); /-* open() *-/
//    LEDs.fill(BLACK);
    int retval = 1;
printf("open stream\n");
    info.GetReturnValue().Set(scope.Escape(Nan::New<v8::Integer>(retval)));
}
*/


#if 0
struct write_req //Work
{
    uv_work_t req;
    Nan::Callback* callback;
//    v8::Persistent<v8::Function> callback;
//there's only one set of hw, so don't need private buffer here
//  mydata* ptr;
//    int x, y, w, h;
    int rcvtime;
//    unsigned char* buffer;
    uint32_t* buf;
    int buflen;
    int written;
//    string result;
    uint64_t started;
};

//void write_async(uv_work_t*);
//void write_after(uv_work_t*);

void flush(uv_timer_s* ignored) { LEDs.flush(); }

void write_async(uv_work_t* req)
{
//    write_req* wreq = reinterpret_cast<write_req*>(req->data);
    write_req* wreq = static_cast<write_req*>(req->data);
//    uv_loop_t* loop = uv_default_loop();

printf("here4 @%ld\n", nsec2msec(uv_hrtime() - wreq->started));
    wreq->written = wreq->buflen;
    if (!wreq->buf[3]) //no delay
    {
        time_base = wreq->rcvtime; //make other delays relative to this buf
printf("immed write stream len %d\n", wreq->buflen);
        LEDs.flush();
        return;
    }
    uint32_t delay = time_base + swap32(wreq->buf[3]) - uv_now(uv_default_loop()); //TODO: call uv_update_time(loop)?
printf("delayed write stream len %d, delay %d\n", wreq->buflen, delay);
//NO    sleep(delay); //this is blocking; need to use uv_loop instead
//use a new timer each time (allows next render to start):
    uv_timer_t frbuf_delay;
    uv_timer_init(uv_default_loop(), &frbuf_delay);
    uv_timer_start(&frbuf_delay, flush, delay, 0);
printf("here5 @%ld\n", nsec2msec(uv_hrtime() - wreq->started));
}
#if 0
void write_async(uv_work_t* req)
{
    write_req* wreq = reinterpret_cast<write_req*>(req->data);
//    wreq->written = wreq->ao->write(wreq->ptr, wreq->buffer, wreq->len);
    for (int i = 0; i < wreq->len; ++i)
        write281x(wreq->buffer[i]);
    wreq->written = wreq->len;
}
#endif


//called after write_aync returns?
void write_after(uv_work_t *req, int status)
{
//??    v8::Isolate* isolate = v8::Isolate::GetCurrent();
//??    v8::HandleScope handleScope(isolate); //needed when creating new v8 JS objects
    Nan::HandleScope scope;
//    write_req* wreq = reinterpret_cast<write_req*>(req->data);
    write_req* wreq = static_cast<write_req*>(req->data);

printf("async callback @%ld\n", nsec2msec(uv_hrtime() - wreq->started));
//    const char *result = work->result.c_str();
    v8::Local<v8::Value> argv[] = { Nan::New(wreq->written) }; //callback param
//    v8::Local<v8::Value> argv[1] = { String::NewFromUtf8(isolate, result) };
    
// https://stackoverflow.com/questions/13826803/calling-javascript-function-from-a-c-callback-in-v8/28554065#28554065
//    v8::Local<v8::Function>::New(isolate, wreq->callback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);

    wreq->callback->Call(1, argv); //CAUTION: might can recursion
printf("async complete @%ld\n", nsec2msec(uv_hrtime() - wreq->started));
    delete wreq->callback;
//    wreq->callback.Reset();
//??    delete wreq;
}
#endif
#if 0
void write_after(uv_work_t *req)
{
    Nan::HandleScope scope;
    write_req* wreq = reinterpret_cast<write_req*>(req->data);

    v8::Local<v8::Value> argv[] = { Nan::New(wreq->written) };

    wreq->callback->Call(1, argv);

    delete wreq->callback;
}
#endif


//add custom data to uv_timer:
struct my_timer_t
{
    uv_timer_t timer;
    uint32_t* buffer;
    int buflen;
    Nan::Callback* callback;
    uint32_t (*swap32)(uint32_t);
    uint64_t started;
    my_timer_t* nextp;
};

uint32_t time_base;
#define nsec2msec(nsec)  ((nsec) / 1000000.0)
int elapsed() { return uv_now(uv_default_loop()) - time_base; }
float hr_elapsed(uint64_t& started) { return nsec2msec(uv_hrtime() - started); }

#if FBUFST == SWAP32(FBUFST)
 #error "can't detect byte order using magic value"
#endif
uint32_t decode_asis(uint32_t uint32) { return uint32; }
uint32_t decode_swap(uint32_t uint32) { return SWAP32(uint32); }


//http://stackoverflow.com/questions/19404177/freeing-dynamically-allocated-uv-timer-t-libuv-instance-in-c11
void on_timer_close_complete(uv_handle_t* timer)
{
//    free(timer);
    delete timer->data;
}


//send value back to callback:
//also deallocate timer struct
void cbret(my_timer_t& wreq, int retval, bool timer_was_active = false)
{
    Nan::HandleScope scope;

//    const char *result = work->result.c_str();
    v8::Local<v8::Value> argv[] = { Nan::New(retval) }; //callback param
//    v8::Local<v8::Value> argv[1] = { String::NewFromUtf8(isolate, result) };
    
// https://stackoverflow.com/questions/13826803/calling-javascript-function-from-a-c-callback-in-v8/28554065#28554065
//    v8::Local<v8::Function>::New(isolate, wreq->callback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);

printf("cbret %d @%d +%f\n", retval, elapsed(), hr_elapsed(wreq.started));
    wreq.callback->Call(1, argv); //CAUTION: might can recursion
    delete wreq.callback;
//    wreq.callback = 0;
    if (timer_was_active) uv_close((uv_handle_t *)&wreq.timer, on_timer_close_complete); //http://stackoverflow.com/questions/19404177/freeing-dynamically-allocated-uv-timer-t-libuv-instance-in-c11
    else delete &wreq;
}


//#define FBLEN  (4 * (NUM_UNIV * UNIV_LEN + 4))
#define FBLEN(w, h)  (4 * ((w) * (h) + 4))

//apply pixel updates to texture buffer:
void render(my_timer_t& wreq)
{
    uint32_t valbuf = wreq.swap32(wreq.buffer[2]);
    int xofs = valbuf >> 16, yofs = valbuf & 0xffff;
    valbuf = wreq.swap32(wreq.buffer[3]);
    int w = valbuf >> 16, h = valbuf & 0xffff;
    CLAMP_RECT(xofs, yofs, w, h, NUM_UNIV, UNIV_LEN);
    int xlimit = xofs + w, ylimit = yofs + h;

printf("blend x %d, y %d, w %d, h %d @%d +%f\n", xofs, yofs, w, h, elapsed(), hr_elapsed(wreq.started));
//char buf[24*1140*12+10], *bp = buf;
    int ofs = 4;
    for (int x = xofs; x < xlimit; ++x)
        for (int y = yofs; y < ylimit; ++y)
        {
//        int x = (i - 2) / UNIV_LEN, y = (i - 2) % UNIV_LEN;
//        uint32_t color = need_swap? SWAP32(wreq->buf[i]): wreq->buf[i];
            uint32_t color = wreq.swap32(wreq.buffer[ofs++]);
	        color = ARGB2ABGR(color); //xlate to internal color format
            color = limit(color); //limit power to 85%
//TODO: use 1D indexing?
    	    blend(LEDs->mpixels[XY(x, y)], color); //use alpha to mix
//sprintf(bp, ", 0x%x", LEDs->mpixels[XY(x,y)]); bp += strlen(bp);
        }
//strcpy(bp, "\n");
//printf(buf+2);
#ifdef CPU_PIVOT
    for (int x = xofs; x < xlimit; x += NUM_GPIO)
        for (int y = yofs; y < ylimit; ++y)
            LEDs->dirty_pivot[x / NUM_GPIO][y] = true;
#endif
printf("render to texture @%d +%f\n", elapsed(), hr_elapsed(wreq.started));
    LEDs->render(); //copy to texture; allows next frame to be rendered in parallel
}


//frame buffer queue:
my_timer_t* pend_head = 0;
my_timer_t* pend_tail;


//dequeue next frame buffer:
//send current texture to GPU
//render next pixel updates into texture buffer
void dequeue(uv_timer_t* timer)
{
//send texture to GPU after waiting:
    if (timer) //timer wakeup event
    {
        my_timer_t& wreq = *(my_timer_t*)timer->data;
printf("\ndequeue, flush to gpu @%d +%f\n", elapsed(), hr_elapsed(wreq.started));
        LEDs->flush();
        cbret(wreq, wreq.buflen, true);
//    if (pend_head != &wreq) error;
        pend_head = wreq.nextp;
printf("more? %d @%d +%f\n", !!pend_head, elapsed(), hr_elapsed(wreq.started));
    }
    if (!pend_head) return;
//render next pixels to texture:
    my_timer_t& pending = *pend_head;
printf("render next @%d +%f\n", elapsed(), hr_elapsed(pending.started));
    render(pending); //render now so data is ready without delay when timer expires
//schedule texture output to GPU:
    int delay = time_base + pending.swap32(pending.buffer[1]) - uv_now(uv_default_loop()); //make relative to now; TODO: call uv_update_time(loop)?
//delay *= 10;
printf("delay flush %d, overdue? %d, has next? %d, @%d +%f\n", delay, (delay < 1), !!pending.nextp, elapsed(), hr_elapsed(pending.started));
//NO        uv_queue_work(uv_default_loop(), &req->req, write_async, write_after); //use timer for aync work queue
//NO    sleep(delay); //this is blocking; need to use uv_loop instead
//use a new timer each time (allows next render to start):
//    uv_timer_t frbuf_delay;
    uv_timer_init(uv_default_loop(), &pending.timer);
    uv_timer_start(&pending.timer, dequeue, (delay > 0)? delay: 1, 0);
}


//examples of async add-on functions:
//https://github.com/TooTallNate/node-speaker
//https://github.com/paulhauner/example-async-node-addon/blob/master/async-addon/async-addon.cc
//http://blog.trevnorris.com/2013/07/node-with-threads.html
//NOTE: not clear that async execution helps here
//texture needs to be redrawn from main thread anyway, so it's better to draw it all at once
//threading really only helps for rendering
#if 0
NAN_METHOD(Write)
{
    Nan::HandleScope scope;
//    mydata* ptr = UnwrapPointer<mydata*>(info[0]);
    unsigned char* buffer = UnwrapPointer<unsigned char*>(info[1]);
    int len = info[2]->Int32Value();

printf("write stream len %d\n", len);
//    js_work* work = new js_work;
    write_req* req = new write_req;
//    req->ao = ao;
    req->buffer = buffer;
    req->len = len;
    req->written = 0;
    req->callback = new Nan::Callback(info[3].As<v8::Function>());
    req->rcvtime = uv_now();
//    work->req.data = work;
    req->req.data = req;
    uv_queue_work(uv_default_loop(), &req->req, write_async, /*(uv_after_work_cb)write_after*/ NULL);
//TODO: return ok flag here?
    info.GetReturnValue().SetUndefined();
}
#endif
//implicit HandleScope is created for you on JavaScript-accessible methods so you do not need to insert one yourself.
//void Write_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
/*
    if (!valid) cb(err);
    else if (!delay)
    {
        if (pending) cancel();
        render();
        flush();
    }
    else
    {
        if (!pending) render();
        aync(flush(); if (pending) render());
    }
*/
NAN_METHOD(Write_entpt)
{
#define args  info //kludge: NaN hard-coded names
//??    v8::Isolate* isolate = args.GetIsolate();
//??    v8::HandleScope handleScope(isolate); //needed when creating new v8 JS objects
    Nan::HandleScope scope;
//    mydata* ptr = UnwrapPointer<mydata*>(args[0]);
//    uint32_t* buffer = UnwrapPointer<uint32_t*>(args[1]);
//    int buflen = args[2]->Int32Value();
//    Nan::Callback* callback = new Nan::Callback(args[3].As<v8::Function>());
    int rcvtime = uv_now(uv_default_loop()); //save in case this buf defines new time base
 
//    if (!LEDs) LEDs = new MyTexture<NUM_UNIV, UNIV_LEN>;
    if (!LEDs) { noderr("LED buffer must be opened first"); return; }
//start setting up async info in case needed:
//use a new timer each time (allows next render to start):
//    my_timer_t wreq;
    my_timer_t& wreq = *new my_timer_t;
    wreq.timer.data = &wreq; //find myself later
    wreq.buffer = UnwrapPointer<uint32_t*>(args[1]);
    wreq.buflen = args[2]->Int32Value();
    // args[0] is where we pick the callback function out of the JS function params.
    // Because we chose args[0], we must supply the callback fn as the first parameter
//    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[3]);
//    req->callback.Reset(isolate, callback);
    wreq.callback = new Nan::Callback(args[3].As<v8::Function>());
    wreq.started = uv_hrtime(); //only for debug
    wreq.nextp = 0; //end of delay chain
    int wrlen = wreq.buflen >> 2;
printf("wr req, pending? %d, delay? %x, buflen %d\n", !!pend_head, wreq.buffer[1], wreq.buflen);
int depth = 0;
for (my_timer_t* ptr = pend_head; ptr; ptr = ptr->nextp, ++depth)
  printf("queue[%d]: delay %d\n", depth, ptr->swap32(ptr->buffer[1]));

//validate buffer:
    if (wreq.buflen & 3) { cbret(wreq, -1); return; } //should be multiple of 4 (uint32_t)
    if (wrlen <= 4) { cbret(wreq, -2); return; } //no pixel data?
    if (wrlen > 4 + NUM_UNIV * UNIV_LEN) { cbret(wreq, -3); return; } //too much data
    if (wreq.buffer[0] == FBUFST) wreq.swap32 = decode_asis; //need_swap = false;
    else if (wreq.buffer[0] == SWAP32(FBUFST)) wreq.swap32 = decode_swap; //need_swap = true;
    else { printf("fbstart 0x%x vs 0x%x\n", wreq.buffer[0], FBUFST); cbret(wreq, -4); return; } //invalid header
//TODO: add checksum?

//decide whether to do it now or later:
    if (!wreq.buffer[1]) //no delay
    {
printf("immed buf, cancel? %d @%d +%f\n", pend_head, elapsed(), hr_elapsed(wreq.started));
        time_base = rcvtime; //make other delays relative to now
        while (pend_head) //cancel delayed updates; new immediate buf replaces them
        {
//            my_timer_t* next = pending[0]->nextp;
            uv_timer_stop(&pend_head->timer);
            uv_close((uv_handle_s*)&pend_head->timer, on_timer_close_complete);
            cbret(*pend_head, -5); //cancelled
            pend_head = pend_head->nextp;
        }
        render(wreq);
printf("flush to GPU @%d +%f\n", elapsed(), hr_elapsed(wreq.started));
        LEDs->flush(); //send texture to GPU
        cbret(wreq, wreq.buflen);
        return;
    }
    else //delayed render+flush
    {
printf("delayed, pending? %d @%d +%f\n", !!pend_head, elapsed(), hr_elapsed(wreq.started));
//        if (!pend_head) render(wreq); //need to render prior to delayed flush
        if (!pend_head) pend_head = pend_tail = &wreq;
        else { pend_tail->nextp = &wreq; pend_tail = &wreq; }
        dequeue(0);
    }
printf("ret to caller @%d +%f\n", elapsed(), hr_elapsed(wreq.started));
    args.GetReturnValue().SetUndefined();
//TODO: return ok flag here?
//    args.GetReturnValue().Set(v8::Undefined(isolate));
#undef args
}

NAN_METHOD(Flush_entpt)
//void Flush_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
#define args  info //kludge: NaN hard-coded names
    Nan::HandleScope scope;
printf("flush stream\n");
//    mydata* ptr = UnwrapPointer<mydata*>(info[0]);
    /* TODO: async */
//    ao->flush(ao);
//    send(); //flush last (partial) buffer
    args.GetReturnValue().SetUndefined();
#undef args
}


///////////////////////////////////////////////////////////////////////////////
////
/// misc interface:
//

//based on https://github.com/vpj/node_shm/blob/master/shm_addon.cpp
//to see shm segs:  ipcs -a
//to delete:  ipcrm -M key
void shmatt_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);

	if (args.Length() < 2) { noderr("shmat: missing key, size"); return; }
//	if (!args[0]->IsNumber()) Nan::ThrowTypeError("Pixel: 1st arg should be number");
	int key = args[0]->Uint32Value(); //NumberValue();
//	if (!args[1]->IsNumber()) Nan::ThrowTypeError("Pixel: 2nd arg should be number");
	int size = args[1]->Int32Value(); //NumberValue();
	if (/*(size < 1) ||*/ (size >= 10000000)) { noderr("size %d out of range 1..10M", size); return; }

    int shmid = shmget(key, (size > 0)? size: 1, (size > 0)? IPC_CREAT | 0666: 0666);
    if (shmid < 0 ) { noderr("can't alloc shmem: %d", shmid); return; }
//if (!data) //don't attach again
    char* data = (size > 0)? (char *)shmat( shmid, NULL, 0 ): (char*)shmctl(shmid, IPC_RMID, NULL);
    if (data < 0 ) { noderr("att sh mem failed: %d", data); return; }
    if (size < 1) { args.GetReturnValue().SetUndefined(); return; } //.Set(0); return; }

//Create ArrayBuffer:
    v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, (void *)data, size);
    args.GetReturnValue().Set(buffer);
}


void fblen_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
    Nan::HandleScope scope;

	int w = (args.Length() > 0)? args[0]->Uint32Value(): NUM_UNIV; //NumberValue();
	int h = (args.Length() > 1)? args[1]->Uint32Value(): UNIV_LEN; //NumberValue();

	args.GetReturnValue().Set(FBLEN(w, h));
}


//NAN_METHOD(swap32_entpt)
void swap32_entpt(const Nan::FunctionCallbackInfo<v8::Value>& args)
{
    Nan::HandleScope scope;

	if (args.Length() < 1) { noderr("swap32: missing value to swap"); return; }
	uint32_t uint32 = args[0]->Uint32Value(); //NumberValue();

    uint32 = SWAP32(uint32);
	args.GetReturnValue().Set(uint32);
}


/*
NAN_GETTER(NanPerson::NameGet) {
  auto person = Nan::ObjectWrap::Unwrap<NanPerson>(info.Holder());
  auto name = Nan::New(person->name).ToLocalChecked();
  info.GetReturnValue().Set(name);
}

NAN_SETTER(NanPerson::NameSet) {
  auto person = Nan::ObjectWrap::Unwrap<NanPerson>(info.Holder());
  // [NOTE] `value` is defined argument in `NAN_SETTER`
  auto name = Nan::To<v8::String>(value).ToLocalChecked();
  person->name = *Nan::Utf8String(name);
}
*/


/*
NAN_METHOD(Close)
{
printf("close stream\n");
    Nan::EscapableHandleScope scope;
//    mydata* ptr = UnwrapPointer<mydata*>(info[0]);
//    ao->close(ao);
//    int r = 0;
//    if (ao->deinit) { r = ao->deinit(ao); }
    int retval = 0;
    info.GetReturnValue().Set(scope.Escape(Nan::New<v8::Integer>(retval)));
}
*/


///////////////////////////////////////////////////////////////////////////////
////
/// binding info
//

#define CONST_INT(name, value)  \
    Nan::ForceSet(exports, Nan::New(name).ToLocalChecked(), Nan::New(value),  \
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete))
#define CONST_STR(name, value)  \
    Nan::ForceSet(exports, Nan::New(name).ToLocalChecked(), Nan::New(value).ToLocalChecked())
//#define VAR_INT(name, setter)  \
//    Nan::ForceSet(exports, Nan::New(name).ToLocalChecked(), \
//        Nan::New<v8::FunctionTemplate>(setter)->GetFunction(), \
//        static_cast<v8::PropertyAttribute>(v8::DontDelete))


//tell Node.js about my entry points:
//void Initialize(v8::Handle<v8::Object> target)
void entpt_init(v8::Local<v8::Object> exports)
{
    Nan::HandleScope scope; //for v8 GC

//module desc:
    CONST_INT("api_version", 2.0);
    CONST_STR("name", "WS281X-gpu");
    CONST_STR("description", "Node.js stream + GPU driver for WS281X");

//misc consts:
    CONST_INT("IsPI", IFPI(true, false)); //easier dev/test
    CONST_INT("FBUFST", FBUFST); //0x59414c50); //"YALP" start of frame marker; checks stream integrity, as well as byte order
    CONST_INT("FBLEN", FBLEN(NUM_UNIV, UNIV_LEN)); //frame buffer size (header + data)

//define ARGB primary colors:
    CONST_INT("RED", RED); //0xffff0000);
    CONST_INT("GREEN", GREEN); //0xff00ff00);
    CONST_INT("BLUE", BLUE); //0xff0000ff);
    CONST_INT("YELLOW", YELLOW); //0xffffff00);
    CONST_INT("CYAN", CYAN); //0xff00ffff);
    CONST_INT("MAGENTA", MAGENTA); //0xffff00ff);
    CONST_INT("WHITE", WHITE); //0xffffffff);
    CONST_INT("WARM_WHITE", WARM_WHITE); //0xffffffb4); //h 60/360, s 30/100, v 1.0
    CONST_INT("BLACK", BLACK); //0xff000000); //NOTE: need alpha
    CONST_INT("XPARENT", XPARENT); //0); //no alpha

//NOTE: it's simpler to set W, H params in C++ and pass to node.js than the other way around
//	exports->Set(Nan::New("width").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(Width_entpt)->GetFunction());
//	exports->Set(Nan::New("height").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(Height_entpt)->GetFunction());
    CONST_INT("width", NUM_UNIV);
    CONST_INT("height", UNIV_LEN);

//config:
	exports->Set(Nan::New("wsout").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(wsout_entpt)->GetFunction());
	exports->Set(Nan::New("group").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(group_entpt)->GetFunction());
	exports->Set(Nan::New("autoclip").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(autoclip_entpt)->GetFunction());
//    VAR_INT("wsout", set_wsout_entpt);
//    VAR_INT("group", set_group_entpt);
//	exports->Set(Nan::New("wsout").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(wsout_getter)->GetFunction());
//	exports->Set(Nan::New("wsout").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(wsout_setter)->GetFunction());
//	exports->SetAccessor(Nan::New("wsout").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(wsout_getter)->GetFunction(),
//                wsout_getter, wsout_setter);

//	exports->Set(Nan::New("group").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(group_getter)->GetFunction());
//	exports->Set(Nan::New("group").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(group_setter)->GetFunction());
//	exports->SetAccessor(Nan::New("group").ToLocalChecked(),
//                group_getter, group_setter);
//    NAN_PROPERTY_GETTER(fd_getter);

//https://github.com/nodejs/nan/issues/467
//#if NODE_MODULE_VERSION < IOJS_3_0_MODULE_VERSION
//Local<Object> exports = Nan::New<Object>(target);
//#else
//Local<Object> exports = target;
//#endif
//Nan::SetAccessor(exports, Nan::New("foo").ToLocalChecked(), FooGetter, 0);


//interactive api:
	exports->Set(Nan::New("fill").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(fill_entpt)->GetFunction());
	exports->Set(Nan::New("render").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(render_entpt)->GetFunction());
	exports->Set(Nan::New("pixel").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(pixel_entpt)->GetFunction());
//	exports->Set(Nan::New("wsout").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(wsout_entpt)->GetFunction());
//	exports->Set(Nan::New("group").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(group_entpt)->GetFunction());

	exports->Set(Nan::New("fblen").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(fblen_entpt)->GetFunction());
	exports->Set(Nan::New("shmatt").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(shmatt_entpt)->GetFunction());
	exports->Set(Nan::New("swap32").ToLocalChecked(),
                 Nan::New<v8::FunctionTemplate>(swap32_entpt)->GetFunction());

//streaming api:
    Nan::SetMethod(exports, "open", Open_entpt);
    Nan::SetMethod(exports, "write", Write_entpt);
    Nan::SetMethod(exports, "flush", Flush_entpt);
//    Nan::SetMethod(exports, "close", Close);
//	exports->Set(Nan::New("write").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(Write_entpt)->GetFunction());
//	exports->Set(Nan::New("flush").ToLocalChecked(),
//                 Nan::New<v8::FunctionTemplate>(Flush_entpt)->GetFunction());

//    LEDs.want_w281x(true);
//printf("limit(0x%x) = 0x%x\n", 0xffffff, LIMIT(0xffffff));
//printf("limit(0x%x) = 0x%x\n", 0xffff80, LIMIT(0xffff80));
//printf("limit(0x%x) = 0x%x\n", 0xffff70, LIMIT(0xffff70));
//printf("limit(0x%x) = 0x%x\n", 0xaaffff, limit(0xaaffff));

//	main(0, NULL);
	init_chain(); //once only
}


#if 0 //TODO
//https://nodejs.org/api/addons.html#addons_addon_examples
//    v8::AtExit(sanity_check);
//  AtExit(at_exit_cb2, cookie);
//  AtExit(at_exit_cb2, cookie);
//    v8::AtExit(at_exit_cb1, exports->GetIsolate());

static void at_exit_cb1(void* arg)
{
    v8::Isolate* isolate = static_cast<v8::Isolate*>(arg);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Object> obj = v8::Object::New(isolate);
    assert(!obj.IsEmpty()); // assert VM is still alive
    assert(obj->IsObject());
    printf("caller exit\n");
}
#endif

} //namespace

NODE_MODULE(ws281Xgpu, entpt_init) //tells Node.js how to find my entry points
//NOTE: can't use special chars in module name here, but bindings.gyp overrides it anyway?
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// color helpers
//

GLuint limit(GLuint color)
{
#ifdef LIMIT_BRIGHTNESS
//#pragma message "limiting R+G+B brightness to " STRING(LIMIT_BRIGHTNESS)
    static uint32_t cache = 0, limited;

    if (cache != color)
    {
        cache = color;
        unsigned int r = R(color), g = G(color), b = B(color);
        int sum = r + g + b; //max = 3 * 255 = 765
        if (sum > LIMIT_BRIGHTNESS) //reduce brightness, try to keep relative colors
        {
//GLuint sv = color;
            r = (r * LIMIT_BRIGHTNESS) / sum;
            g = (g * LIMIT_BRIGHTNESS) / sum;
            b = (b * LIMIT_BRIGHTNESS) / sum;
            color = Amask(color) | (r << 16) | (g << 8) | b;
//printf("REDUCE: 0x%x, sum %d, R %d, G %d, B %d => r %d, g %d, b %d, 0x%x\n", sv, sum, R(sv), G(sv), B(sv), r, g, b, color);
        }
        limited = color;
    }
    color = limited;
#endif //def LIMIT_BRIGHTNESS
    return color;
}


#if 0
//convert color ARGB <-> ABGR format:
//OpenGL seems to prefer ABGR format, but RGB order is more readable (for me)
//convert back with same function & 0xffffff
uint32_t ARGB2ABGR(uint32_t color)
{
//TODO: drop alpha setting?
//??	if (!Amask(color) /*&& (color & 0xffffff)*/) color |= 0xff000000; //RGB present but no alpha; add full alpha to force color to show
//    return color;
	return Amask(color) | (Rmask(color) >> 16) | Gmask(color) | (Bmask(color) << 16); //swap R, B
}
#endif


//blend 2 RGB values according to newcomer's alpha:
/*
void blend(uint32_t& target, int unmix, int premix_R, int premix_G, int premix_B)
{
//TODO: +1 and >>8 instead of /255?
    target = 0xff000000 | //Amask(color) | //force full alpha on target so it shows up as computed
        (((unmix * R(target) + premix_R) / 255) << 16) |
        (((unmix * G(target) + premix_G) / 255) << 8) |
        ((unmix * B(target) + premix_B) / 255);
}
*/
void blend(uint32_t& target, uint32_t color)
{
    static uint32_t cache[2] = {0}, blended;
    static int premix_R, premix_G, premix_B;

    int mix = A(color);
    if (mix == 255) target = color;
    else if (mix)
    {
        if (cache[0] != color)
        {
            premix_R = mix * R(color);
            premix_G = mix * G(color);
            premix_B = mix * B(color);
            cache[0] = color;
            cache[1] = 0; //invalidate final result cache
        }
        if (cache[1] != target)
        {
            mix = 255 - mix;
            blended = 0xff000000 | //force full alpha on target so it shows up as computed
                (((mix * R(target) + premix_R) / 255) << 16) |
                (((mix * G(target) + premix_G) / 255) << 8) |
                ((mix * B(target) + premix_B) / 255);
            cache[1] = target;
        }
        target = blended;
    }
}


///////////////////////////////////////////////////////////////////////////////
////
/// misc utility helpers
//

//Log an error message to the debug output for the platform
void logmsg(const char *formatStr, ...)
{
	va_list params;
	char buf[BUFSIZ];
	va_start(params, formatStr);
	vsprintf(buf, formatStr, params); //TODO: send to stderr?
	va_end(params);
	printf("ERROR: %s", buf);
}


int why(int retval, const char* reason, ...)
{
	va_list params;
	char buf[BUFSIZ];
	va_start(params, reason);
	vsprintf(buf, reason, params); //TODO: send to stderr?
	va_end(params);
	printf("ret %d: %s\n", retval, buf);
	return retval;
}


//init chain wrapper:
//instantiate all init code here
void init_chain()
{
    init();
}


//eof
