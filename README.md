was: Node.js stream and API to drive WS281X LEDs using the RPi GPU via OpenGL

ws281x-gpu
==========
### Node.js add-on module to drive WS281X nodes using RPi GPU
(write-up still in progress)

A Writable stream instance that accepts JavaScript buffer objects containing RGB values and sends them to a Raspberry PI GPU to control WS2811 or WS2812 LED nodes.

The inspiration for this work came from the following brilliant work by Steve Hardy:

https://stevehardyblog.wordpress.com/2016/01/02/ws2811-christmas-lighting-using-vga

He figured out that the GPU could be used as an output port with precision timing, and used an OpenGL shader to generate timing to control WS281X LED nodes.

This module adapts and builds on that technique to allow the RPi GPU to control up to 24 strings of WS281X nodes in parallel using Node.js.
Each scan line drives 1 WS281X node per string instead of 2 scan lines per node as in the original article.

Critical design factors
-----------------------
* screen scan width must be ~= 30 usec to drive one WS281X node using each scan row
* HSYNC + front/back porch time must be <= 1 usec to conform to WS281X protocol timing

Other significant design factors
--------------------------------
* screen scan height (#scan rows, or #WS281X nodes) determines refresh rate (FPS)
* an external mux can be used with the GPIO pins to control *multiples* of 24 strings of WS281X;

for example, a horizontal resolution of 1536 pixels would give up to 64 * 24 = 1536 parallel strings of WS281X nodes;

at 60 FPS, that would be 1536 * 550 nodes ~= 3/4 M WS281X nodes

at 30 FPS, that would be 1536 * 1100 nodes ~= 1.5 M WS281X nodes

Installation
============
Install and compile using `npm`:
``` bash
$ npm install ws281x-gpu
```

On Debian/Ubuntu, OpenGL (and mesa) is needed.  Output will go the the XWindows screen (for development/testing).

On RPi, the dpi24 overlay must be loaded, and HDMI timing parameters set in the config.txt file.

(add details here)

Testing
=======
Run the examples/simple-color-test.js file:
``` bash
$ examples/simple-color-test.js
```

This will send some colors to any WS281X connected to the RPi (or to an XWindows debug screen).

Custom Usage/API
================
(need to add more details here)

TODO:
=====
* maybe add RPi watchdog timer:

http://harizanov.com/2013/08/putting-raspberry-pis-hardware-watchdog-to-work/

http://pi.gadgetoid.com/article/who-watches-the-watcher

License:
--------
CC-BY-NC-SA-4.0

https://spdx.org/licenses/CC-BY-SA-4.0.html

eof
