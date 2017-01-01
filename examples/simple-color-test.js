#!/usr/bin/env node
//generate some color test patterns for WS281X-gpu

//TODO: split into separate test files

//to install/compile:
//  1. cd my parent folder
//  2. npm install
//to test:
//  1. cd my parent folder
//  2. env DEBUG=* examples/myfile.js

'use strict';

require('colors');
const fs = require('fs');
const Readable = require('stream').Readable || require('readable-stream/readable');
const Transform = require('stream').Transform;
const inherits = require('util').inherits;
const WS281X = require('../');

const WHICH = 4; //(process.argv.length > 2)? 1; //which test to run
const DEBUG = true; //false;
console.log("%s ver %s\n%s".blue_light, WS281X.module_name, WS281X.api_version, WS281X.description);
console.log("canvas %d x %d, Pi? %d".blue_light, WS281X.width, WS281X.height, WS281X.IsPI);
WS281X.wsout = DEBUG? -1: true;
WS281X.group = DEBUG? WS281X.height / 25: 1;

//use smaller #univ and univ len for test/debug:
const NUM_UNIV = DEBUG? 4: WS281X.width;
const UNIV_LEN = DEBUG? 10: WS281X.height;

//ARGB colors:
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
const YELLOW = 0xffffff00;
const MAGENTA = 0xffff00ff;
const CYAN = 0xff00ffff;
const WHITE = 0xffffffff;
const WARM_WHITE = 0xffffffb4; //h 60/360, s 30/100, v 1.0
const BLACK = 0xff000000;
const XPARENT = 0; //alpha = 0 for transparent

//test colors and start times:
const playback =
[
    {when: 0, color: RED, },
    {when: 1.0, color: GREEN, },
    {when: 2.0, color: BLUE, },
    {when: 3.0, color: YELLOW, },
    {when: 4.0, color: CYAN, },
    {when: 5.0, color: MAGENTA, },
    {when: 6.0, color: WHITE, },
    {when: 6.5, color: BLACK, },
];

//slow it down for easier test/debug:
const lastarg = process.argv.slice(-1)[0];
if (lastarg.match(/^\*[0-9]+$/))
    playback.reduce((prior, current) =>
    {
        current.when_bk = current.when;
        var delay = current.when - (prior.when_bk || 0);
        delay *= lastarg.substr(1);
        current.when = (prior.when || 0) + delay;
        return current;
    }, {});
else if (lastarg.match(/^slow|[+-][0-9]+$/))
    playback.forEach((evt, inx) => { evt.when += (1 * lastarg || 20) * inx; });
//console.log("adjusted playback", playback);


//various tests:
//send in-memory patterns directly to GPU (no file):
if (WHICH == 1) //false)
setImmediate(function() //avoid hoist errors
{
    console.log("memory -> GPU".cyan_light);
    var src = new FrameWriter(playback);
    var dest = new WS281X({title: "memory -> GPU"}); //{ lowWaterMark: 1, highWaterMark: 4});
    src.pipe(dest);
});

//write patterns to file:
if (WHICH == 2) //false)
setImmediate(function() //avoid hoist errors
{
    console.log("memory -> file".cyan_light);
    var src = new FrameWriter(playback);
    var dest = fs.createWriteStream("test.stream"); //path, [options])
    src.pipe(dest);
});

//send patterns from file to GPU:
if (WHICH == 3) //false)
setImmediate(function() //avoid hoist errors
{
    console.log("file -> GPU".cyan_light);
    var src = fs.createReadStream("test.stream");
    var dest = new WS281X({title: "file -> GPU"});
    src.pipe(new FrameReader()).pipe(dest);
});

//API patterns to GPU:
if (WHICH == 4) //false)
blocking(function*() //use synchronous coding style for simplicity
{
    var ok = WS281X.open("interactive API");
    console.log("interactive API, open ok? %d".cyan_light, ok);
    var started = now_sec();
    for (var i = 0; i < playback.length; ++i)
    {
        var {when, color} = playback[i];
  //      console.log("delay %d, wait %d", when, when + started - now_sec());
        yield wait(when + started - now_sec()); //minimize cumulative timing errors
        WS281X.fill(color).render(); //use fluent API
        console.log("fill 0x%s @%d sec".blue_light, color.toString(16), now_sec() - started);
    }
    console.log("eof".blue_light);
});


//pause("rgb done (pause)");
console.log("done (async main)".green_light);


///////////////////////////////////////////////////////////////////////////////
////
/// write stream data:
//

//    var src = new Readable();
//    src._read = read;
//class FrameWriter extends Readable
//{
//    constructor(opts) { this.opts = opts; }
//}
function FrameWriter(opts)
{
    if (!(this instanceof FrameWriter)) return new FrameWriter(opts);
    Readable.call(this); //super
    this.playback = opts;
}
inherits(FrameWriter, Readable);


//Readable callback function to generate RGB data:
//this just steps thru the playback list, generating one frame for each color
FrameWriter.prototype._read =
function read(n)
{
    if (!this.playback) return;
    if (!this.latest) this.latest = 0;
    if (this.latest >= this.playback.length) { this.push(null); return; } //eof
    var {when, color} = this.playback[this.latest++];
//console.log(typeof when, typeof color);
    var buf = Buffer.alloc(4 * NUM_UNIV * UNIV_LEN + 16);
//console.log("alloc buf %s x %s = %s", WS281X.width, WS281X.height, buf.length);

    var ofs = -4;
    buf.writeUInt32BE(WS281X.FBUFST, ofs += 4); //mark start of frame buffer
    buf.writeUInt32BE((1000 * when) >>> 0, ofs += 4); //frame delay time
    buf.writeUInt32BE(0, ofs += 4); //frame offset (x, y)
    buf.writeUInt32BE((NUM_UNIV << 16) | UNIV_LEN, ofs += 4); //frame size (w, h)
//TODO: use fill opcode, flush flag
    for (var x = 0; x < NUM_UNIV; ++x)
        for (var y = 0; y < UNIV_LEN; ++y)
            buf.writeUInt32BE(((x + y) & 1)? color >>> 0: BLACK >>> 0, ofs += 4);
    this.push(buf);
console.log("push buf#%d, color 0x%s, delay %s sec".blue_light, this.latest, color.toString(16), when);
}


function mkfr(when)
{
    var buf = Buffer.alloc(4 * NUM_UNIV * UNIV_LEN + 16);
//console.log("alloc buf %s x %s = %s", WS281X.width, WS281X.height, buf.length);

    var ofs = -4;
    buf.writeUInt32BE(WS281X.FBUFST, ofs += 4); //mark start of frame buffer
    buf.writeUInt32BE((1000 * when) >>> 0, ofs += 4); //frame delay time
    buf.writeUInt32BE(0, ofs += 4); //frame offset (x, y)
    buf.writeUInt32BE((NUM_UNIV << 16) | UNIV_LEN, ofs += 4); //frame size (w, h)
    buf.fill = function(color)
    {
        var ofs = 16 - 4;
        for (var x = 0; x < NUM_UNIV; ++x)
            for (var y = 0; y < UNIV_LEN; ++y)
                buf.writeUInt32BE(color >>> 0, ofs += 4);
    }
    buf.pixel = function(x, y, color)
    {
        var ofs = x * UNIV_LEN + y + 4;
        buf.writeUInt32BE(color >>> 0, 4 * ofs);
    }
    return buf;
}


///////////////////////////////////////////////////////////////////////////////
////
/// read stream data:
//

//var parser = new Transform();
//parser._transform = function(data, encoding, done) { this.push(data); done(); };
//class FrameReader extends Transform
//{
//    constructor(opts) { this.opts = opts; }
//}

function FrameReader(opts)
{
    if (!(this instanceof FrameReader)) return new FrameReader(opts);
    this.objectMode = true;
    Transform.call(this); //super
}
inherits(FrameReader, Transform);


//split input stream into frames:
FrameReader.prototype._transform =
function xform(data, encoding, done)
{
//    this.push(data);
console.log("xform got %s len %d", encoding, data.length);
    if (!this.order) //figure out byte order (assumes data is a Buffer)
    {
        var first = data.readUInt32BE(0);
        if (first == WS281X.FBUFST) this.order = "readUInt32BE";
        else if (first == WS281X.swap32(WS281X.FBUFST)) this.order = "readUInt32LE";
        else throw "Unknown byte order / invalid header";
    }
    this.quebuf = this.quebuf? Buffer.concat([this.quebuf, data]): data;

    for (;;)
    {
        if (this.quebuf.length < 4*4) break; //header incomplete; { done(); return; }
        var wh = this.quebuf[this.order](4 * 2);
        var w = wh >> 16, h = wh & 0xffff;
        var buflen = 4 * w * h + 16;
        if (this.quebuf.length < buflen) break; //data incomplete; { done(); return; }
console.log("eat buf %d x %d, len %d", w, h, buflen);
        var buf = this.quebuf.slice(0, buflen);
        this.quebuf = this.quebuf.slice(buflen); //do this before push() in case recursion revisits quebuf
        this.push(buf);
    }
    done();
console.log("xform done");
}

FrameReader.prototype._flush =
function flush(done)
{
     if (this.quebuf.length) this.push(this.quebuf); //incomplete; let caller deal with it
     this.quebuf = null;
     done();
}

/*
function FrameReader(file, opts) {
  if (!(this instanceof readLine)) return new readLine(file, opts);

  EventEmitter.call(this);
  opts = opts || {};
  opts.maxLineLength = opts.maxLineLength || 4096; // 4K
  opts.retainBuffer = !!opts.retainBuffer; //do not convert to String prior to invoking emit 'line' event
  var self = this,
      lineBuffer = new Buffer(opts.maxLineLength),
      lineLength = 0,
      lineCount = 0,
      byteCount = 0,
      emit = function spliceAndEmit(lineCount, byteCount) {
        try {
          var line = lineBuffer.slice(0, lineLength);
          self.emit('line', opts.retainBuffer? line : line.toString(), lineCount, byteCount);
        } catch (err) {
          self.emit('error', err);
        } finally {
          lineLength = 0; // Empty buffer.
        }
      };
  this.input = ('string' === typeof file) ? fs.createReadStream(file, opts) : file;
  this.input
    .on('open', function onOpen(fd) {
      self.emit('open', fd);
    })
    .on('data', function onData(data) {
      var dataLen = data.length;
      for (var i = 0; i < dataLen; i++) {
        if (data[i] == 10 || data[i] == 13) { // Newline char was found.
          if (data[i] == 10) {
            lineCount++;
            emit(lineCount, byteCount);
          }
        } else {
          lineBuffer[lineLength] = data[i]; // Buffer new line data.
          lineLength++;
        }
        byteCount++;
      }
    })
    .on('error', function onError(err) {
      self.emit('error', err);
    })
    .on('end', function onEnd() {
      // Emit last line if anything left over since EOF won't trigger it.
      if (lineLength) {
        lineCount++;
        emit(lineCount, byteCount);
      }
      self.emit('end');
    })
    .on('close', function onClose() {
      self.emit('close');
    });
};
util.inherits(readLine, EventEmitter);
*/


///////////////////////////////////////////////////////////////////////////////
////
/// synchronous (blocking) code:
//

//step thru a generator function:
//allows synchronous (blocking) coding style
function blocking(gen)
{
	if (typeof gen == "function") //invoke generator if not already
    {
        setImmediate(function() { blocking(gen()); }); //avoid hoist errors
        return;
    }
//    process.stdin.pause(); //don't block process exit while not waiting for input
    var retval;
	for (;;)
	{
		var status = gen.next(retval); //send previous value to generator
//		if (!status.done && (typeof status.value == "undefined")) continue; //cooperative multi-tasking
		if (typeof status.value == "function") //caller wants manual step
        {
            retval = status.value(gen);
            return;
        }
        retval = status.value;
		if (status.done) return retval;
	}
}

//wake up from synchronous sleep:
function wait(delay)
{
    delay *= 1000; //sec -> msec
    if (delay > 1) return setTimeout.bind(null, blocking, delay);
}


///////////////////////////////////////////////////////////////////////////////
////
/// misc:
//

//current time in seconds:
function now_sec()
{
    return Date.now() / 1000;
}

//wait for console input:
function pause(prompt)
{
    console.log(prompt);
    process.stdin.once('data', function(text) { process.exit(); });
//see https://docs.nodejitsu.com/articles/command-line/how-to-prompt-for-command-line-input/
    process.stdin.resume();
    process.stdin.setEncoding('utf8');
}


//eof
