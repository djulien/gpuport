#!/usr/bin/env node
//generate some color test patterns for WS281X-gpu

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

const DEBUG = true; //false;
console.log("%s ver %s\n%s".blue_light, WS281X.module_name, WS281X.api_version, WS281X.description);
console.log("canvas %d x %d".blue_light, WS281X.width, WS281X.height);
WS281X.want = true; //DEBUG? -1: true;
WS281X.group = DEBUG? WS281X.height / 25: 1;

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

//test colors and durations:
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


//various tests:
//send in-memory patterns directly to GPU (no file):
//if (false)
setImmediate(function() //avoid hoist errors
{
    console.log("memory -> GPU".cyan_light);
    var src = new FrameWriter(playback);
    var dest = new WS281X({title: "memory -> GPU"}); //{ lowWaterMark: 1, highWaterMark: 4});
    src.pipe(dest);
});

//write patterns to file:
if (false)
setImmediate(function() //avoid hoist errors
{
    console.log("memory -> file".cyan_light);
    var src = new FrameWriter(playback);
    var dest = fs.createWriteStream("test.stream"); //path, [options])
    src.pipe(dest);
});

//send patterns from file to GPU:
if (false)
setImmediate(function() //avoid hoist errors
{
    console.log("file -> GPU".cyan_light);
    var src = fs.createReadStream("test.stream");
    var dest = new WS281X({title: "file -> GPU"});
    src.pipe(new FrameReader()).pipe(dest);
});

//pause("rgb done (pause)");
console.log("done (async)".green_light);


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
    if (!this.latest) this.latest = 0;
    if (this.latest >= this.playback.length) { this.push(null); return; } //eof
    var {when, color} = this.playback[this.latest++];
    var lastarg = process.argv.slice(-1)[0];
var svw = when;
    if (lastarg.match(/^x[0-9]+$/)) when *= lastarg.substr(1); //slow down for easier debug
console.log("when %d => %d  %s", svw, when, lastarg);
    if (process.argv.slice(-1)[0].match(/^x[0-9]+$/)) when *= 10; //easier debug
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
/// misc:
//

function pause(prompt)
{
    console.log(prompt);
    process.stdin.once('data', function(text) { process.exit(); });
//see https://docs.nodejitsu.com/articles/command-line/how-to-prompt-for-command-line-input/
    process.stdin.resume();
    process.stdin.setEncoding('utf8');
}


//eof
