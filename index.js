
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


//exports:
exports = module.exports = WS281X;

exports.api_version = binding.api_version;
exports.description = binding.description;
exports.module_name = binding.name;

exports.width = binding.width;
exports.height = binding.height;
exports.FBUFST = binding.FBUFST; //start of frame marker (helps check stream integrity)
exports.swap32 = binding.swap32;
exports.shmatt = binding.shmatt;

//TODO: change to getters/setters; for now, make it look that way
//exports.want = binding.want;
var want_latest;
Object.defineProperty(exports, "want",
{
    get: function() { return want_latest; },
    set: function(newval) { binding.want(want_latest = newval); },
});
//exports.group = binding.group;
var group_latest;
Object.defineProperty(exports, "group",
{
    get: function() { return group_latest; },
    set: function(newval) { binding.group(group_latest = newval); },
});
//exports.debug = binding.debug;

//define ARGB primary colors:
exports.RED = binding.RED;
exports.GREEN = binding.GREEN;
exports.BLUE = binding.BLUE;
exports.YELLOW = binding.YELLOW;
exports.CYAN = binding.CYAN;
exports.MAGENTA = binding.MAGENTA;
exports.WHITE = binding.WHITE;
exports.BLACK = binding.BLACK;
exports.XPARENT = binding.XPARENT;


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
    if (null == opts.lowWaterMark) opts.lowWaterMark = 0;
    if (null == opts.highWaterMark) opts.highWaterMark = 0;
    Writable.call(this, opts); //super

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
WS281X.prototype._open = function()
{
    debug('open()');
    if (this.state >= States.OPEN)
        throw new Error('_open() called more than once!');
    this.state = States.OPEN; //isopen = true; //new Buffer(binding.sizeof_audio_output_t);
//console.log("state = open");
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
