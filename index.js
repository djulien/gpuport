
'use strict';

var os = require('os');
var debug = require('debug')('ws281X-gpu');
var binding = require('bindings')('ws281X-gpu');
var inherits = require('util').inherits;
var Writable = require('readable-stream/writable');
//console.log("binding", binding);


// determine the native host endianness, the only supported playback endianness
//var endianness = 'function' == os.endianness ?
//                 os.endianness() :
//                 'LE'; // assume little-endian for older versions of node.js


///////////////////////////////////////////////////////////////////////////////
////
/// entry points:
//

exports = module.exports = WS281X;


//general:
WS281X.api_version = binding.api_version;
WS281X.description = binding.description;
WS281X.module_name = binding.name;


//define ARGB primary colors:
WS281X.RED = binding.RED;
WS281X.GREEN = binding.GREEN;
WS281X.BLUE = binding.BLUE;
WS281X.YELLOW = binding.YELLOW;
WS281X.CYAN = binding.CYAN;
WS281X.MAGENTA = binding.MAGENTA;
WS281X.WHITE = binding.WHITE;
WS281X.BLACK = binding.BLACK;
WS281X.XPARENT = binding.XPARENT;


//config settings:
WS281X.width = binding.width;
WS281X.height = binding.height;

//TODO: change to getters/setters; for now, just make it look that way
//exports.want = binding.want;
var want_latest;
Object.defineProperty(WS281X, "want",
{
    get: function() { return want_latest; },
    set: function(newval) { binding.want(want_latest = newval); },
});
//exports.group = binding.group;
var group_latest;
Object.defineProperty(WS281X, "group",
{
    get: function() { return group_latest; },
    set: function(newval) { binding.group(group_latest = newval); },
});
//exports.debug = binding.debug;


//buffer mgmt:
WS281X.fblen = binding.fblen;
WS281X.FBLEN = binding.FBLEN; //frame buffer size (header + data)
WS281X.FBUFST = binding.FBUFST; //start of frame marker (helps check stream integrity)
WS281X.shmatt = binding.shmatt;
WS281X.swap32 = binding.swap32;


//interactive (non-stream) api:
WS281X.pixel = binding.pixel;
WS281X.fill = binding.fill;
WS281X.render = binding.render;

//streaming api:
WS281X.open = binding.open;
WS281X.write = binding.write;
WS281X.flush = binding.flush;


///////////////////////////////////////////////////////////////////////////////
////
/// stream object class:
//

const States =
{
    NEW: 0,
    OPEN: 1,
    CLOSED: 2,
};


//ctor:
//TODO: class WS281X extends Writable
function WS281X(opts)
{
    if (!(this instanceof WS281X)) return new WS281X(opts);
    if (!opts) opts = {};
    if (!opts.lowWaterMark) opts.lowWaterMark = 0;
    if (!opts.highWaterMark) opts.highWaterMark = 0;
    Writable.call(this, opts); //super
//    this.title = opts.title || "WS281X-GPU test";
//    this.wndw = opts.wndw || 640;
//    this.wndh = opts.wndh || 480;
//console.log("opts", opts);
    if (!binding.open(opts.title, opts.wndw, opts.wndh)) //"WS281X-GPU test", 640, 480))
        throw "WS281X open failed";

    this.state = States.NEW;
//console.log("state = new");

// bind event listeners
//  this._format = this._format.bind(this);
    this.on('finish', this._flush);
//  this.on('pipe', this._pipe);
//  this.on('unpipe', this._unpipe);
}
inherits(WS281X, Writable);


//dummy method to look like real streams:
WS281X.prototype._open = function() //title, width, height)
{
    debug('open()');
    if (this.state >= States.OPEN)
        throw new Error('_open() called more than once!');
    this.state = States.OPEN; //isopen = true; //new Buffer(binding.sizeof_audio_output_t);
//    binding.open.apply(null, arguments);
//    if (!binding.open(this.title, this.wndw, this.wndh); //"WS281X-GPU test", 640, 480))
console.log("state = open");
    this.emit('open');
    return true; 
};


//main worker method:
WS281X.prototype._write = function(chunk, encoding, done)
{
debug('_write() (%o bytes)', chunk.length);
    if (this.state == States.NEW) this._open(); //auto-open
    if (this.state == States.CLOSED)
    {
        debug('aborting remainder of write() call (%o bytes), since speaker is `_closed`', left.length);
        return done();
    }
//    if (this.state != States.OPEN)
//        return done(new Error('write() call after close() call'));

    var buf = chunk;
    function onwrite(wrlen)
    {
debug('wrote %o bytes', wrlen);
        if (wrlen != buf.length) done(new Error('write() failed: ' + wrlen));
        else
        {
            debug('done with this chunk len ' + buf.length);
//            done(); //NOTE: can cause recursion
            setImmediate(done);
        }
    }

debug('writing %o byte chunk', buf.length);
    binding.write(null, buf, buf.length, onwrite);
};


/**
 * Called when this stream is pipe()d to from another readable stream.
 * If the "sampleRate", "channels", "bitDepth", and "signed" properties are
 * set, then they will be used over the currently set values.
 *
 * @api private
 */
/*
WS281X.prototype._pipe = function (source)
{
  debug('_pipe()');
//  this._format(source);
//  source.once('format', this._format);
};
*/


/**
 * Called when this stream is pipe()d to from another readable stream.
 * If the "sampleRate", "channels", "bitDepth", and "signed" properties are
 * set, then they will be used over the currently set values.
 *
 * @api private
 */
/*
WS281X.prototype._unpipe = function (source)
{
  debug('_unpipe()');
//  source.removeListener('format', this._format);
};
*/


/**
 * Emits a "flush" event and then calls the `.close()` function on
 * this instance.
 *
 * @api private
 */
WS281X.prototype._flush = function()
{
    debug('_flush()');
    this.emit('flush');
    this.close(false);
};


/**
 * Closes the backend. Normally this function will be called automatically
 * after the backend has finished playing the buffer.
 *
 * @param {Boolean} flush - if `false`, then don't call the `flush()` native binding call. Defaults to `true`.
 * @api public
 */
WS281X.prototype.close = function(flush)
{
    debug('close(%o)', flush);
    if (this.state == States.CLOSED) return debug('already closed...');

    debug('invoking flush() native binding');
    binding.flush(); //this.audio_handle);

    this.state = States.CLOSED; //_closed = true;
console.log("state = closed");
    this.emit('close');
};


//eof
