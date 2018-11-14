
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


//node.js shm:
//https://www.npmjs.com/package/shm-typed-array
//https://www.npmjs.com/package/shared-buffer
//https://www.npmjs.com/package/ems

//https://www.npmjs.com/package/shmmap
//https://www.npmjs.com/package/node-shm-buffer
//https://www.npmjs.com/package/node_shm


///////////////////////////////////////////////////////////////////////////////
////
/// entry points:
//

exports = module.exports = WS281X;


//general:
exports.module_name = binding.name;
exports.api_version = binding.api_version;
exports.description = binding.description;


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


//config settings:
exports.IsPI = binding.IsPI;
exports.width = binding.width;
exports.height = binding.height;

//TODO: change to getters/setters; for now, just make it look that way

var fps_latest = 30;
//exports.fps = binding.fps;
Object.defineProperty(exports, "fps",
{
    get: function() { return fps_latest; },
    set: function(newval) { fps_latest = newval; }, //TODO: add to bindings
});

var wsout_latest;
//exports.wsout = binding.wsout;
Object.defineProperty(exports, "wsout",
{
    get: function() { return wsout_latest; },
    set: function(newval) { binding.wsout(wsout_latest = newval); },
});

var group_latest;
//exports.group = binding.group;
Object.defineProperty(exports, "group",
{
    get: function() { return group_latest; },
    set: function(newval) { binding.group(group_latest = newval); },
});


var autoclip_latest;
//exports.autoclip = binding.auticlip;
Object.defineProperty(exports, "autoclip",
{
    get: function() { return autoclip_latest; },
    set: function(newval) { binding.autoclip(autoclip_latest = newval); },
});

var debug_latest = false;
//exports.debug = binding.debug;
Object.defineProperty(exports, "debug",
{
    get: function() { return debug_latest; },
    set: function(newval) { debug_latest = newval; }, //TODO: add to bindings and/or hook up to other debug()
});


//buffer mgmt:
exports.fblen = binding.fblen;
exports.FBLEN = binding.FBLEN; //frame buffer max size (header + data)
exports.FBUFST = binding.FBUFST; //start of frame marker (helps check stream integrity and byte order)
exports.shmatt = binding.shmatt;
exports.swap32 = binding.swap32;


//interactive (non-stream) api:
//TODO: make these fluent; simulate it for now

//exports.pixel = binding.pixel;
exports.pixel = function(x, y, color)
{
    var retval = binding.pixel.apply(binding, arguments);
    if (arguments.length > 2) retval = this; //fluent only if not returning current color data
    return retval;
}

//exports.fill = binding.fill;
exports.fill = function(args)
{
    binding.fill.apply(binding, arguments);
    return this; //fluent
}

//exports.render = binding.render;
exports.render = function(args)
{
    binding.render.apply(binding, arguments);
    return this; //fluent
}


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

//streaming api:
exports.open = binding.open; //this one is for interactive API as well
exports.write = binding.write;
exports.flush = binding.flush;


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
    if (this.debug) console.log("state = open".blue_light);
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
    if (this.debug) console.log("state = closed".blue_light);
    this.emit('close');
};


//eof
