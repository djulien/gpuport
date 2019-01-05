//WS281X test using Linux framebuffer:
//build:  gcc fbws.c -o fbws
//run:  [sudo]  fbws

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <memory.h> //memmove()
#include <string.h> //snprintf()
//#include <algorithm> //std::min<>(), std::max<>()

#define _0H  16
#define _0L  48
#define _1H  36
#define _1L  28
#define _H(b)  ((b)? _1H: _0H)
#define _L(b)  ((b)? _1L: _0L)
#define BITW(b)  (((b) < 23)? 64: 48) //last bit is partially hidden
#define SIZEOF(ary)  (sizeof(ary) / sizeof((ary)[0]))
#define RGSWAP(rgb24)  ((((rgb24) >> 8) & 0xff00) | (((rgb24) << 8) & 0xff0000) | ((rgb24) & 0xff))


//(A)RGB primary colors:
//NOTE: consts below are processor-independent (hard-coded for ARGB msb..lsb)
//internal SDL_Color is RGBA
//use later macros to adjust in-memory representation based on processor endianness (RGBA vs. ABGR)
//#pragma message("Compiled for ARGB color format (hard-coded)")
#define RED  0xFFFF0000 //fromRGB(255, 0, 0) //0xFFFF0000
#define GREEN  0xFF00FF00 //fromRGB(0, 255, 0) //0xFF00FF00
#define BLUE  0xFF0000FF //fromRGB(0, 0, 255) //0xFF0000FF
#define YELLOW  (RED | GREEN) //0xFFFFFF00
#define CYAN  (GREEN | BLUE) //0xFF00FFFF
#define MAGENTA  (RED | BLUE) //0xFFFF00FF
#define PINK  MAGENTA //easier to spell :)
#define BLACK  (RED & GREEN & BLUE) //0xFF000000 //NOTE: needs Alpha
#define WHITE  (RED | GREEN | BLUE) //fromRGB(255, 255, 255) //0xFFFFFFFF

//use low brightness to reduce eye burn during testing:
//#define BLACK 0
//#define RED  0x1f0000
//#define GREEN  0x001f00
//#define BLUE  0x00001f
//#define YELLOW  0x1f1f00
//#define CYAN  0x001f1f
//#define MAGENTA  0x1f001f
//#define WHITE  0x1f1f1f
//#define ALPHA  0xff000000


int min(int lhs, int rhs) { return (lhs < rhs)? lhs: rhs; }


//vsync based on sample code from https://forums.libsdl.org/viewtopic.php?p=33010
//#include <unistd.h>
//#include <fcntl.h> //open(), close()
//#include <sys/ioctl.h> //ioctl()
//#include <linux/fb.h>
//#include <cstdlib> //atexit()
//#include <stdexcept> //std::runtime_error


#ifndef FBIO_WAITFORVSYNC
 #define FBIO_WAITFORVSYNC  _IOW('F', 0x20, __u32)
#endif


//explicit call to wait for vsync:
//various posts suggest that RPi video driver doesn't support vsync, so use this to force it
//NOTE: need sudo or be a member of video group to use this
//list groups: more /etc/group
//which groups am i a member of:  groups
//add user to group: usermod -aG video "$USER"
//**need to log out and back in or "su username -"; see https://unix.stackexchange.com/questions/277240/usermod-a-g-group-user-not-work
int vsync(int fbfd)
{
    int arg = 0;
    if (fbfd < 0) return -1;
    return ioctl(fbfd, FBIO_WAITFORVSYNC, &arg);
//        if (ioctl(fbfd, FBIO_WAITFORVSYNC, &arg) < 0)
//            if (want_exc) throw std::runtime_error(strerror(errno)); //let caller deal with it
}


//insert commas into a numeric string (for readability):
//CAUTION: uses static data to preserve data after return; semaphore arbitrates a pool of 12 ret values
const char* commas(int64_t val)
{
#define LIMIT  4 //    static const int LIMIT = 4; //max #commas to insert
    /*thread_local*/ static int ff; //std::atomic<int> ff; //use TLS to avoid the need for mutex (don't need atomic either)
    /*thread_local*/ static char buf[12][16 + LIMIT]; //allow 12 simultaneous calls (each thread if using TLS)
//    static auto_ptr<SDL_sem> acquire(SDL_CreateSemaphore(SIZE(buf)));
//    auto_ptr<SDL_LockedSemaphore> lock_HERE(acquire.cast); //SDL_LOCK(acquire));

    char* bufp = buf[++ff % SIZEOF(buf)] + LIMIT; //alloc ret val from pool; don't overwrite other values within same printf, allow space for commas
    for (int grplen = min(sprintf(bufp, "%ld", val), LIMIT * 3) - 3; grplen > 0; grplen -= 3)
    {
        memmove(bufp - 1, bufp, grplen);
        (--bufp)[grplen] = ',';
    }
    return bufp;
}


// 'global' variables to store screen info
char* fbp = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

void put_pixel_RGB32(int x, int y, int r, int g, int b)
{
    // calculate the pixel's byte offset inside the buffer
    // note: x * 3 as every pixel is 3 consecutive bytes
    unsigned int pix_offset = x * 4 + y * finfo.line_length;

    // now this is about the same as 'fbp[pix_offset] = value'
    *((char*)(fbp + pix_offset)) = b;
    *((char*)(fbp + pix_offset + 1)) = g;
    *((char*)(fbp + pix_offset + 2)) = r;
    *((char*)(fbp + pix_offset + 3)) = 0xff;

}

void put_pixel_RGB24(int x, int y, int r, int g, int b)
{
    // calculate the pixel's byte offset inside the buffer
    // note: x * 3 as every pixel is 3 consecutive bytes
    unsigned int pix_offset = x * 3 + y * finfo.line_length;

    // now this is about the same as 'fbp[pix_offset] = value'
    *((char*)(fbp + pix_offset)) = b;
    *((char*)(fbp + pix_offset + 1)) = g;
    *((char*)(fbp + pix_offset + 2)) = r;

}

void put_pixel_RGB565(int x, int y, int r, int g, int b)
{
    // calculate the pixel's byte offset inside the buffer
    // note: x * 2 as every pixel is 2 consecutive bytes
    unsigned int pix_offset = x * 2 + y * finfo.line_length;

    // now this is about the same as 'fbp[pix_offset] = value'
    // but a bit more complicated for RGB565
    //unsigned short c = ((r / 8) << 11) + ((g / 4) << 5) + (b / 8);
    unsigned short c = ((r / 8) * 2048) + ((g / 4) * 32) + (b / 8);
    // write 'two bytes at once'
    *((unsigned short*)(fbp + pix_offset)) = c;

}

void put_pixel(int x, int y, int r, int g, int b)
{
	switch ( vinfo.bits_per_pixel)
	{
		case 16: put_pixel_RGB565(x, y, r, g, b); return;
        case 24: put_pixel_RGB24(x, y, r, g, b); return;
        case 32: put_pixel_RGB32(x, y, r, g, b); return;
    }
}


void draw()
{
	uint32_t colors[] = {RGSWAP(RED), RGSWAP(GREEN), BLUE, YELLOW, RGSWAP(CYAN), RGSWAP(MAGENTA), WHITE};
//for (int i = 0; i < SIZEOF(colors); ++i) printf("color[%d/%d]: 0x%x\n", i, SIZEOF(colors), colors[i]);
	long int scrsize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	memset(fbp, 0, scrsize);
//set first 10 nodes (24-1 bits):
//	uint32_t color = 0xff00ff; //R <-> G; //0x00ffff; //cyan (RGB)
for (int loop = 0; loop <= 10; ++loop)
{
for (int y = 0; y < 37; ++y)
	for (int b = 0; b < 24; ++b) //NOTE: last bit is partially hidden by hsync
	{
		uint32_t color = colors[(y + loop) % SIZEOF(colors)];
		if (loop == 10) color = 0;
//if (!b) printf("node[%d]: 0x%x\n", y, color);
		uint32_t bv = color & (0x800000 >> b);
		for (int i = 0; i < BITW(b); ++i)
		{
			int onoff = (i < _H(bv))? 0xff: 0;
			put_pixel(BITW(0) * b + i, y, onoff, onoff, onoff);
		}
	}
sleep(1);
}
}


// helper function for drawing - no more need to go mess with
// the main function when just want to change what to draw...
#define sqrt(x)  0
void old_draw() {

    int x, y;
    int r, g, b;
    int dr;
    int cr = vinfo.yres / 3;
    int cg = vinfo.yres / 3 + vinfo.yres / 4;
    int cb = vinfo.yres / 3 + vinfo.yres / 4 + vinfo.yres / 4;

    for (y = 0; y < (vinfo.yres); y++) {
        for (x = 0; x < vinfo.xres; x++) {
            dr = (int)sqrt((cr - x)*(cr - x)+(cr - y)*(cr - y));
            r = 255 - 256 * dr / cr;
            r = (r >= 0) ? r : 0;
            dr = (int)sqrt((cg - x)*(cg - x)+(cr - y)*(cr - y));
            g = 255 - 256 * dr / cr;
            g = (g >= 0) ? g : 0;
            dr = (int)sqrt((cb - x)*(cb - x)+(cr - y)*(cr - y));
            b = 255 - 256 * dr / cr;
            b = (b >= 0) ? b : 0;

                put_pixel(x, y, r, g, b);
        }
    }
}


//frame-rate test:
int test(int fbfd) //(*vsync(void)))
{
	const uint32_t palette[] = {RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE}; //RGSWAP(RED), RGSWAP(GREEN), BLUE, YELLOW, RGSWAP(CYAN), RGSWAP(MAGENTA), WHITE};
//for (int i = 0; i < SIZEOF(colors); ++i) printf("color[%d/%d]: 0x%x\n", i, SIZEOF(colors), colors[i]);
	const int scrsize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    const int pitch32 = finfo.line_length / sizeof(uint32_t);
    const int num_pixels = vinfo.xres * vinfo.yres;
    uint32_t* const fb32p = (uint32_t*)fbp;

    int numfr = 0;
    for (int loop = 0; loop < 1; ++loop) //10; ++loop)
        for (int y = 0; y < vinfo.yres; ++y)
	        for (int b = 0; b < vinfo.xres; ++b) //NOTE: last bit is partially hidden by hsync
            {
                fb32p[b + y * pitch32] = palette[loop % SIZEOF(palette)];
                if (!(b % 100)) printf("numfr %s\n", commas(numfr));
                if (b & 0xF) continue;
//                sleep(1); //sec
                if (vsync(fbfd) < 0) { printf("error %d\n", errno); return numfr; } //error
                ++numfr;
            }
    // now this is about the same as 'fbp[pix_offset] = value'
//    *((char*)(fbp + pix_offset)) = b;
//    *((char*)(fbp + pix_offset + 1)) = g;
//    *((char*)(fbp + pix_offset + 2)) = r;
//    *((char*)(fbp + pix_offset + 3)) = 0xff;
    return numfr;
}


// application entry point
int main(int argc, char* argv[])
{
    struct fb_var_screeninfo orig_vinfo;
    long int screensize = 0;
    int fbfd = 0;

    system("cat /dev/fb0 > before.dat");

    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (!fbfd || ((int)fbfd == -1))
    {
      printf("Error: cannot open framebuffer device.\n");
      return(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) 
      printf("Error reading fixed information.\n");

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) 
      printf("Error reading variable information.\n");
    if (!vinfo.pixclock) vinfo.pixclock = -1;
    printf("Original %dx%d, %d bpp, linelen %d, pxclk %d, lrul marg %d %d %d %d, sync len h %d v %d, fps %f\n", 
       vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length, vinfo.pixclock,
       vinfo.left_margin, vinfo.right_margin, vinfo.upper_margin, vinfo.lower_margin, vinfo.hsync_len, vinfo.vsync_len,
       (double)(vinfo.xres + vinfo.left_margin + vinfo.hsync_len + vinfo.right_margin) * (vinfo.yres + vinfo.upper_margin + vinfo.vsync_len + vinfo.lower_margin ) / vinfo.pixclock);

    // Store for reset (copy vinfo to vinfo_orig)
    memcpy(&orig_vinfo, &vinfo, sizeof(vinfo)); //struct fb_var_screeninfo));

    // map fb to user mem 
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);


    int numfr = 0;
    struct timespec start, finish;
    clock_gettime(/*CLOCK_REALTIME*/ CLOCK_MONOTONIC, &start);
    if (!fbfd || (fbp == (char*)-1))
        printf("Failed to mmap.\n");
    else {
        // draw...
//        draw();
        numfr = test(fbfd);
        printf("sleep(5)\n");
        sleep(5);
    }
    clock_gettime(/*CLOCK_REALTIME*/ CLOCK_MONOTONIC, &finish);
    long elapsed = (finish.tv_sec - start.tv_sec) * 1e3; //sec -> msec
    elapsed += finish.tv_nsec / 1e6 - start.tv_nsec / 1e6; //nsec -> msec
    printf("%s of %s frames in %s msec = %f fps\n", commas(numfr), commas(vinfo.xres * vinfo.yres), commas(elapsed), (double)1000 * numfr / elapsed);

    // cleanup
    munmap(fbp, screensize);
    if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo))
        printf("Error re-setting variable information.\n");
    close(fbfd);

   system("cat /dev/fb0 > after.dat");
   printf("done\n");
   return 0; 
}
