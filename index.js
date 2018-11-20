#!/usr/bin/env node

"use strict";
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
const /*GpuPort*/ {limit, listen} = require('./build/Release/gpuport'); //.node');
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


const TB1 = 0xff80ff, TB2 = 0xffddbb; //too bright
debug(`
    limit blue ${hex(BLUE)} => ${hex(limit(BLUE))}, 
    cyan ${hex(CYAN)} => ${hex(limit(CYAN))},
    white ${hex(WHITE)} => 0x${hex(limit(WHITE))}, //should be reduced
    ${hex(TB1)} => ${hex(limit(TB1))}, //should be reduced
    ${hex(TB2)} => ${hex(limit(TB2))} //should be reduced
    ${hex(limit(16777215.5))} //WHITE as float
    `.unindent(/^\s*/gm).nocomment.nonempty.trimEnd()/*.escnl.quote().*/.blue_lt);
//    ${limit("16777215")} //WHITE as string
//    ${limit()} //missing value
//    ${limit({})} //bad value

//const value = 8;    
//console.log(`${value} times 2 equals`, GpuPort.my_function(value));
//console.log(GpuPort.hello());
const seqlen = 10;
const opts =
{
};
var count = 0;
const gp = listen(opts, (frnum) =>
{
    console.log(`req# ${++this.count || (this.count = 1)} for fr# ${frnum} from GPU port: ${arguments[1]}`);
    /*if (this.count == 1)*/ console.log("this", typeof this, this);
    return ++count * 100;
    return (++count < 10);
    return (frnum < seqlen); //tell GpuPort when to stop
});
console.log("gpu:", typeof gp, gp);
//const prevInstance = new testAddon.ClassExample(4.3);
//console.log('Initial value : ', prevInstance.getValue());

//doing something else while listening on gpu port:
step.debug = false; //true;
function* main()
{
    var seq = 0;
    for (var i = 0; i < 100/20; ++i)
    {
        console.log({value: seq++});
        yield wait(2000);
    }
    return -2;
}
//done(step(main)); //sync
//step(main, done); //async
function done(retval) { console.log("retval: " + retval); }
console.log("after main");


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
    var {done, value} = step.gen.next(step.value);
    if (step.debug) console.log("done? " + done + ", value " + typeof(value) + ": " + value + ", async? " + !!step.async_cb);
//    if (typeof value == "function") value = value(); //execute wakeup events
    step.value = value;
    if (done)
    {
        step.gen = null; //prevent further execution
        if (step.async_cb) step.async_cb(value);
        return value;
    }
}


//delay for step/generator function:
function wait(msec)
{
//    if (!step.async_cb)
//    return setTimeout.bind(null, step, msec); //defer to step loop
    setTimeout(step, msec);
//    return msec; //dummy retval for testing
    return +4; //dummy value for debug
}


function debug(args) { console.log.apply(null, arguments); }

function hex(thing) { return `0x${(thing >>> 0).toString(16)}`; }

function extensions()
{
    if (!Array.prototype.back) //polyfill
        Object.defineProperty(Array.prototype, "back", { get() { return this[this.length - 1]; }});
    String.prototype.splitr = function splitr(sep, repl) { return this.replace(sep, repl).split(sep); } //split+replace
    String.prototype.unindent = function unindent(prefix) { unindent.svprefx = ""; return this.replace(prefix, (match) => match.substr((unindent.svprefix || (unindent.svprefix = match)).length)); } //remove indents
    String.prototype.quote = function quote(quotype) { quotype || (quotype = '"'); return `${quotype}${this/*.replaceAll(quotype, `\\${quotype}`)*/}${quotype}`; }
//below are mainly for debug:
//    if (!String.prototype.echo)
    String.prototype.echo = function echo(args) { args = Array.from(arguments); args.push(this.escnl); console.error.apply(null, args); return this; } //fluent to allow in-line usage
    String.prototype.replaceAll = function replaceAll(from, to) { return this.replace(new RegExp(from, "g"), to); } //NOTE: caller must esc "from" string if contains special chars
//    if (!String.prototype.splitr)
    Object.defineProperties(String.prototype,
    {
        escnl: { get() { return this.replace(/\n/g, "\\n"); }}, //escape all newlines
        nocomment: { get() { return this.replace(/\/\*.*?\*\//gm, "").replace(/(#|\/\/).*?$/gm, ""); }}, //strip comments; multi-line has priority over single-line; CAUTION: no parsing or overlapping
        nonempty: { get() { return this.replace(/^\s*\r?\n/gm , ""); }}, //strip empty lines
        escre: { get() { return this.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }}, //escape all RE special chars
    });
}

//eof