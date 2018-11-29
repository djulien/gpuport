#!/usr/bin/env node

"use strict";
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
const pathlib = require("path"); //NOTE: called it something else to reserve "path" for other var names
//TODO? const log4js = require('log4js'); //https://github.com/log4js-node/log4js-node
const GpuPort = require('./build/Release/gpuport'); //.node');
const /*GpuPort*/ {limit, listen} = GpuPort; //require('./build/Release/gpuport'); //.node');
//console.log("GpuPort", GpuPort);
extensions(); //hoist so inline code below can use

//module.exports = gpuport;

//const nan = require('nan');
//console.log("\nnan", JSON.stringify(nan));
//const napi = require('node-addon-api');
//console.log("\nnapi.incl", JSON.stringify(napi.include));
//console.log("\nnapi.gyp", JSON.stringify(napi.gyp));


///////////////////////////////////////////////////////////////////////////////
////
/// color test:
//

//(A)RGB primary colors:
//NOTE: consts below are processor-independent (hard-coded for ARGB msb..lsb)
//internal SDL_Color is RGBA
//use later macros to adjust in-memory representation based on processor endianness (RGBA vs. ABGR)
//#pragma message("Compiled for ARGB color format (hard-coded)")
const RED = 0xFFFF0000; //fromRGB(255, 0, 0) //0xFFFF0000
const GREEN = 0xFF00FF00; //fromRGB(0, 255, 0) //0xFF00FF00
const BLUE = 0xFF0000FF; //fromRGB(0, 0, 255) //0xFF0000FF
const YELLOW = (RED | GREEN); //0xFFFFFF00
const CYAN = (GREEN | BLUE); //0xFF00FFFF
const MAGENTA = (RED | BLUE); //0xFFFF00FF
const PINK = MAGENTA; //easier to spell :)
const BLACK = (RED & GREEN & BLUE); //0xFF000000 //NOTE: needs Alpha
const WHITE = (RED | GREEN | BLUE); //fromRGB(255, 255, 255) //0xFFFFFFFF
//other ARGB colors (debug):
//const SALMON  0xFF8080
//const LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA
//ARGB primary colors:

const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE];


const TB1 = 0xff80ff, TB2 = 0xffddbb; //too bright
if (typeof limit == "function")
{
debug(`
    limit blue ${hex(BLUE)} => ${hex(limit(BLUE))}, 
    cyan ${hex(CYAN)} => ${hex(limit(CYAN))},
    white ${hex(WHITE)} => 0x${hex(limit(WHITE))}, //should be reduced
    ${hex(TB1)} => ${hex(limit(TB1))}, //should be reduced
    ${hex(TB2)} => ${hex(limit(TB2))} //should be reduced
    white float => ${hex(limit(16777215.5))} //WHITE as float
    `.unindent(/^\s*/gm).nocomment.nonempty.trimEnd()/*.escnl.quote().*/.blue_lt);
//    ${limit("16777215")} //WHITE as string
//    ${limit()} //missing value
//    ${limit({})} //bad value
}

const seqlen = 10;
const opts =
{
    vgroup: 30,
    color: -1, //0xffff00ff,
    protocol: GpuPort.NONE,
//    mynodes: new Uint32Array(GpuPort.NUM_UNIV * GpuPort.UNIV_MAXLEN + 1),
};
//var THIS = {count: 0, };
//TODO: try{
const SEQLEN = 5;
//const WANT_GP = false; //true; //false;
const gp = listen && listen(opts, (frnum, nodes, frinfo) =>
{
    debug(`req# ${++this.count || (this.count = 1)} for fr# ${frnum} from GPU port: ${arguments.length} args. nodes ${commas(nodes.length)}:${JSON.stringify(nodes).json_tidy.trunc()}, frinfo ${JSON.stringify(frinfo).json_tidy}, want more? ${frnum < SEQLEN}`);
    /*if (this.count == 1)*/ debug("this", `(${typeof this})`, this); //, `(${typeof THIS})`, THIS, 'retval', hex((THIS.count < SEQLEN)? 0xffffff: 0));
//    return (++THIS.count < SEQLEN)? 0xffffff: 0;
    for (var i = 0; i < 5; ++i) nodes[i] = PALETTE[(frnum + i) % PALETTE.length];
    const retval = (frnum < SEQLEN)? 0xffffff: 0; //tell GpuPort which univ ready; 0 => stop
    debug(`retval: ${frnum} < ${SEQLEN} => ${retval}`);
    return retval;
});
//}catch(exc){}
debug("gpu listen:", `(${typeof gp})`, JSON.stringify(gp).json_tidy);
setTimeout(() => debug("gpu +1 sec:", `(${typeof gp})`, /*JSON.stringify(gp).json_tidy*/ gp), 1000);
//const prevInstance = new testAddon.ClassExample(4.3);
//console.log('Initial value : ', prevInstance.getValue());

//doing something else while listening on gpu port:
function* main()
{
    var seq = 0;
    for (var i = 0; i < 100/20; ++i)
    {
        debug({main_loop: seq++});
        yield wait(2000 + i); //use unique value for easier debug
    }
//    wait.cancel("done"); //cancel latest
//    yield; //kludge: wait for last timer to occur
    process.stdout.end();
    return -2; //for debug
}
step.debug = debug;
//done(step(main)); //sync
step(main, done); //async
function done(retval)
{
    debug("main retval: " + retval);
//show anything preventing process exit:
//    process.ste
    debug("remaining handles", process._getActiveHandles()); //.map((obj) => (obj.constructor || {}).name || obj));
//    process._getActiveHandles().forEach((obj) => ((obj.constructor || {}).name == "Timer")? debug(obj): null);
    debug("remaining requests", process._getActiveRequests());
}
debug("after main");

//if (false)
(function idle()
{
    if ((++idle.count || (idle.count = 1)) < 5) setTimeout(idle, 250);
    debug(`idle ${idle.count}`.blue_lt); //, arguments.back); //how much overdue?
})();


///////////////////////////////////////////////////////////////////////////////
////
/// NAPI test:
//

const binding = GpuPort; //require('./build/Release/gpuport'); //.node');

if (binding.hello)
{
    const value = 8;    
    console.log(`${value} times 2 equals`, binding.my_function(value));
    console.log(binding.hello());
}


// Use the "bindings" package to locate the native bindings.
if (binding.startThread)
{
// Call the function "startThread" which the native bindings library exposes.
// The function accepts a callback which it will call from the worker thread and
// into which it will pass prime numbers. This callback simply prints them out.
    binding.startThread((thePrime, other) =>
    {
        debug(`cb this (${typeof this}) ${JSON.stringify(this)}, other (${typeof other}) ${JSON.stringify(other).trunc(200)}`.blue_lt);
        ++this.prime_count || (this.prime_count = 1);
        other[4] = 4444;
        other[7] = 7777777;
        ++other[0];
        debug(`Received prime# ${this.prime_count} from secondary thread: ${thePrime}`.cyan_lt);
        return (this.prime_count < 10);
    });
//    process.exit();
}


///////////////////////////////////////////////////////////////////////////////
////
/// Misc utility/helper functions:
//

//step thru generator function:
//cb called async at end
//if !cb, blocks until generator function exits
function step(gen, async_cb)
{
    if (gen && !gen.next) gen = gen(); //func -> generator
    step.gen || (step.gen = gen);
    step.async_cb || (step.async_cb = async_cb);
//    for (;;)
//    {
    try
    {
        const {done, value} = step.gen.next(step.value);
        if (step.debug) step.debug(`done? ${done}, retval ${typeof(value)}: ${value}, async? ${!!step.async_cb}`); //, overdue ${arguments.back}`);
//    if (typeof value == "function") value = value(); //execute wakeup events
//    step.value = value;
        if (done)
        {
            step.gen = null; //prevent further execution; step() will cause "undef" error
            if (step.async_cb) step.async_cb(value);
//        return value;
        }
        return step.value = value;
    }
    catch (exc) { console.error(`EXC: ${exc}`.red_lt); }
}


//delay for step/generator function:
function wait(msec)
{
//    if (!step.async_cb)
//    return setTimeout.bind(null, step, msec); //defer to step loop
//    wait.pending || (wait.pending = {});
//    wait.pending[setTimeout(step, msec)] =
    debug("wait: set timeout ", msec);
//NOTE: Node timer api is richer than browser; see https://nodejs.org/api/timers.html
    /*wait.latest =*/ setTimeout(step, msec); //function(){ wait.cancel("fired"); step(); }, msec); //CAUTION: assumes only 1 timer active
//    wait.latest.unref();
//    return msec; //dummy retval for testing
    return msec; //dummy value for debug
}
//wait.cancel = function wait_cancel(reason)
//{
//    reason && (reason += ": ");
//    debug(`${reason || ""}wait cancel? ${JSON.stringify(this.latest)}, has ref? ${((this.latest || {}).hasRef || noop)()}`.yellow_lt);
//    if (typeof this.latest != "undefined") clearTimeout(this.latest);
//    this.latest = undefined;
//}

//return "file:line#":
//mainly for debug or warning/error messages
function srcline(depth)
{
    const want_path = (depth < 0);
//    if (isNaN(++depth)) depth = 1; //skip this function level
    const frame = __stack[Math.abs(depth || 0) + 1]; //skip this stack frame
//console.error(`filename ${frame.getFileName()}`);
    return `@${(want_path? nop_fluent: pathlib.basename)(frame.getFileName()/*.unquoted || frame.getFileName()*/, ".js")}:${frame.getLineNumber()}`; //.gray_dk; //.underline;
//    return `@${pathlib.basename(__stack[depth].getFileName(), ".js")}:${__stack[depth].getLineNumber()}`;
}


function debug(args) { console.log.apply(null, Array.from(arguments).push_fluent(__parent_srcline)); }

function commas(num) { return num.toLocaleString(); } //grouping (1000s) default = true

function hex(thing) { return `0x${(thing >>> 0).toString(16)}`; }

function noop() {} //dummy placeholder function

function extensions()
{
    Object.defineProperties(global,
    {
        __srcline: { get: function() { return srcline(1); }, },
        __parent_srcline: { get: function() { return srcline(2); }, },
    });
    if (!Array.prototype.back) //polyfill
        Object.defineProperty(Array.prototype, "back", { get() { return this[this.length - 1]; }});
    Array.prototype.push_fluent = function push_fluent(args) { this.push.apply(this, arguments); return this; };
    String.prototype.splitr = function splitr(sep, repl) { return this.replace(sep, repl).split(sep); } //split+replace
    String.prototype.unindent = function unindent(prefix) { unindent.svprefx = ""; return this.replace(prefix, (match) => match.substr((unindent.svprefix || (unindent.svprefix = match)).length)); } //remove indents
    String.prototype.quote = function quote(quotype) { quotype || (quotype = '"'); return `${quotype}${this/*.replaceAll(quotype, `\\${quotype}`)*/}${quotype}`; }
//below are mainly for debug:
//    if (!String.prototype.echo)
    String.prototype.echo = function echo(args) { args = Array.from(arguments); args.push(this.escnl); console.error.apply(null, args); return this; } //fluent to allow in-line usage
    String.prototype.trunc = function trunc(maxlen) { maxlen || (maxlen = 120); return (this.length > maxlen)? this.slice(0, maxlen) + ` ... (${commas(this.length - maxlen)})`: this; }
    String.prototype.replaceAll = function replaceAll(from, to) { return this.replace(new RegExp(from, "g"), to); } //NOTE: caller must esc "from" string if contains special chars
//    if (!String.prototype.splitr)
    Object.defineProperties(String.prototype,
    {
        escnl: { get() { return this.replace(/\n/g, "\\n"); }}, //escape all newlines
        nocomment: { get() { return this.replace(/\/\*.*?\*\//gm, "").replace(/(#|\/\/).*?$/gm, ""); }}, //strip comments; multi-line has priority over single-line; CAUTION: no parsing or overlapping
        nonempty: { get() { return this.replace(/^\s*\r?\n/gm , ""); }}, //strip empty lines
        escre: { get() { return this.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }}, //escape all RE special chars
        json_tidy: { get() { return this.replace(/,"/g, ", \"").replace(/"(.*?)":/g, "$1: ").replace(/\d{5,}/g, (val) => `0x${(val >>> 0).toString(16)}`); }}, //tidy up JSON for readability
    });
}

//eof