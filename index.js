#!/usr/bin/env node

//to debug:
//node  debug  this-file  args
//cmds: run, cont, step, out, next, bt, .exit, repl

"use strict";
//require("./demos/multi");
//const chproc = require("child_process");
//chproc.fork(__dirname + "/demos/multi", process.argv);
//process.exit();
/////////////////////////////////////////////////////////////////////////////////
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
const util = require("util"); //format()
//console.error("hello".green_lt);
//process.exit();
const pathlib = require("path"); //NOTE: called it something else to reserve "path" for other var names
//TODO? const log4js = require('log4js'); //https://github.com/log4js-node/log4js-node
//const /*{ createSharedBuffer, detachSharedBuffer }*/ sharedbuf = require('shared-buffer'); //https://www.npmjs.com/package/shared-buffer
const /*GpuPort*/ gp = require('./build/Release/gpuport'); //.node');
const {limit, NUM_UNIV, /*UNIV_LEN,*/ nodebufs} = gp; //make global for easier access

extensions(); //hoist so inline code below can use them
process.on('uncaughtException', (err) =>
{
    console.log("DEBUG LOG:\n", gp.read_debug());
    console.error(`${datestr()} uncaught exception: ${err.message? err.message: err}`.red_lt);
    if (err.stack) console.error(err.stack);
    process.exit(1);
});

//debug("isvalid?", GpuPort.isvalid);
//const /*GpuPort*/ {limit, listen, nodebufq} = GpuPort; //require('./build/Release/gpuport'); //.node');
//const shmpeek = new Uint32Array(sharedbuf.createSharedBuffer(GpuPort.manifest.shmkey, 100 * Uint32Array.BYTES_PER_ELEMENT, true), 100, 0);


//ARGB primary colors:
//NOTE: consts below are processor-independent (hard-coded for ARGB msb..lsb)
//internal SDL_Color is RGBA, FB is ARGB
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

//convert (r, g, b) to 32-bit ARGB color:
function toargb(r, g, b)
{
    return (0xff000000 | (r << 16) | (g << 8) | b) >>> 0; //force convert to uint32
}


///////////////////////////////////////////////////////////////////////////////
////
/// main test program:
//

const OPTS =
{
//{vgroup: 20, debug: 33, init_color: 0xFF00FFFF /*CYAN*/, protocol: GpuPort.Protocols.DEV_MODE});
    debug: true,
//    screen: 0,
//    vgroup: 30, //vertical stretch for dev/debug
//    vgroup: 3, //vertical stretch for dev/debug
//    vgroup: 1, //vertical stretch for dev/debug
    vgroup: 32, //1,
//    color: -1, //0xffff00ff,
    init_color: hsv2rgb(0, 1.0, 0.25), //dark gray //BLACK,
//    protocol: GpuPort.NONE,
    protocol: gp.Protocols.DEV_MODE,
//    protocol: GpuPort.WS281X,
//    fps: .1,
//    fps: 30,
//    mynodes: new Uint32Array(GpuPort.NUM_UNIV * GpuPort.UNIV_MAXLEN + 1),
    frtime: 1/60, //60 FPS
};

setInterval(() =>
{
    console.log("DEBUG LOG", gp.get_debug());
}, 100);

//kludge: trim down for shorter display info:
//GpuPort.nodebufs.forEach((nodebuf) => { nodebuf.nodes.splice(3, 21); nodebuf.nodes.forEach((univ) => univ.splice(4, univ.length)); });
//console.log("GpuPort", GpuPort);
debug("isvalid?", gp.isvalid);
gp.open(OPTS);
wait4port();
const UNIV_LEN = gp.UNIV_LEN; //CAUTION: must go after GpuPort is opened
debug("GpuPort imports".cyan_lt, JSON.stringify(gp, null, 2).json_tidy.trunc(1600));
debug(`GpuPort: ${commas(NUM_UNIV)} x ${commas(UNIV_LEN)} = ${commas(NUM_UNIV * UNIV_LEN)} nodes, open? ${gp.isopen}`);
if (!NUM_UNIV || !UNIV_LEN) throw "no nodes?".red_lt;
//debug("spares", Array.isArray(GpuPort.spares)? "array": typeof(GpuPort.spares), GpuPort.spares.length, "nodebufs", typeof GpuPort.nodebufs, GpuPort.nodebufs.length, typeof GpuPort.nodebufs[0], GpuPort.nodebufs[0].nodes.length, GpuPort.nodebufs[0].nodes[0].length);
//debug(`peek ${hex(shmpeek[0])}, ${hex(shmpeek[1])}, ${hex(shmpeek[2])}, ${hex(shmpeek[3])}, ...`);
//process.exit();


//debug(`nodebufq: ${nodebufq.length} x ${nodebufq[0].length} x ${nodebufq[0][0].length}`);
//process.exit();

const TB1 = 0xff80ff, TB2 = 0xffddbb; //too bright
//if (typeof GpuPort.limit == "function")
//{ //use scope to delay execution of limit()
debug(() => `
    limit test: 
    blue ${hex(BLUE)} => ${hex(limit(BLUE))}, 
    cyan ${hex(CYAN)} => ${hex(limit(CYAN))},
    white ${hex(WHITE)} => 0x${hex(limit(WHITE))}, //should be reduced
    ${hex(TB1)} => ${hex(limit(TB1))}, //should be reduced
    ${hex(TB2)} => ${hex(limit(TB2))} //should be reduced
    white float => ${hex(limit(16777215.5))} //WHITE as float
    `.unindent(/^\s*/gm).nocomment.nonempty.trimEnd()/*.escnl.quote().*/); //.blue_lt);
//    ${limit("16777215")} //WHITE as string
//    ${limit()} //missing value
//    ${limit({})} //bad value
//}

//const seqlen = 10;
//var THIS = {count: 0, };
//TODO: try{
//const WANT_GP = false; //true; //false;
//const false_listen = false;
//OBSOLETE
//const gp = listen && listen(opts, (frnum, nodes, frinfo) =>
//{
//    const more = (frnum < SEQLEN);
//    debug(`req# ${++this.count || (this.count = 1)} for fr# ${frnum} from GPU port: ${arguments.length} args. nodes ${commas(nodes.length)}:${JSON.stringify(nodes).json_tidy.trunc()}, frinfo ${JSON.stringify(frinfo /*, null, 2*/).json_tidy}, want more? ${more}`);
//    /*if (this.count == 1)*/ debug("this", `(${typeof this})`, this); //, `(${typeof THIS})`, THIS, 'retval', hex((THIS.count < SEQLEN)? 0xffffff: 0));
//    return (++THIS.count < SEQLEN)? 0xffffff: 0;
//    for (var i = 0; i < this.NUM_UNIV * this.UNIV_LEN; ++i) nodes[i] = PALETTE[(frnum + i) % PALETTE.length];
//    for (var x = 0; x < frinfo.NUM_UNIV; ++x)
//        for (var y = 0; y < frinfo.UNIV_LEN; ++y)
//            nodes[x * frinfo.UNIV_LEN + y] = ((x + y) & 1)? (x && y && (x < frinfo.NUM_UNIV - 1) && (y < frinfo.UNIV_LEN - 1))? PALETTE[(x + frnum) % PALETTE.length]: WHITE: BLACK;
//            nodes[x][y] = ((x + y) & 1)? (x && y && (x < frinfo.NUM_UNIV - 1) && (y < frinfo.UNIV_LEN - 1))? PALETTE[(x + frnum) % (PALETTE.length - 1)]: WHITE: BLACK;
//    for (var i = 0; i < nodes.length; ++i) nodes[i] = PALETTE[frnum % PALETTE.length];
//perf stats:
// '0 = wait for caller data
// '1 = 0
// '2 = txtr lock + pivot/encode + unlock
// '3 = render copy txtr to GPU
// '4 = render present + vsync wait
//    debug(`elapsed: ${frinfo.elapsed / 1000} msec, perf avg: ${frinfo.perf.map((msec) => commas(msec / frnum)).join(", ")}`.cyan_lt);
//    if (!more) { debug("EOF".red_lt); setTimeout(() => process.exit(0), 2000); } //kludge: dangling refs keep proc open, so forcefully kill it
//    return more? 0xffffff: 0; //tell GpuPort which univ ready; 0 => stop
//    debug(`retval: ${frnum} < ${SEQLEN} => ${retval}`);
//    return retval;
//});
//}catch(exc){}
//if (listen) debug("gpu listen:", `(${typeof gp})`, JSON.stringify(gp, null, 2).json_tidy);
//setTimeout(() => debug("gpu +1 sec:", `(${typeof gp})`, JSON.stringify(gp, null, 2).json_tidy), 1000);
//const prevInstance = new testAddon.ClassExample(4.3);
//console.log('Initial value : ', prevInstance.getValue());


//generate test frames:
function* main()
{
//debugger;
    const DURATION = 10; //* opts.fps; //10 sec total
//    if (!NUM_UNIV || !UNIV_LEN) throw "no nodes?".red_lt;
//GpuPort.fill(BLACK);
//    var seq = 0;
//    for (var fr = 0; fr < DURATION / OPTS.frtime; ++i)
//console.log("numfr", DURATION / OPTS.frtime, "#nodebuf", nodebufs.length);
    for (let frnum = 0;;)
        for (let x = 0; x < NUM_UNIV; ++x)
            for (let y = 0; y < UNIV_LEN; ++y)
            {
//console.log("hjere1");
                let color = PALETTE[frnum % PALETTE.length];
                if (++frnum >= DURATION / OPTS.frtime) return -2;
//console.log("hjere2");
                const qent = frnum % nodebufs.length; //get next framebuf queue entry; simple, circular queue addressing
//        debug({main_loop: seq++});
//console.log(`main: fr ${frnum}/${DURATION / OPTS.frtime}, qent ${qent}, x ${x}, y ${y}`);
                debug(() => `fr# ${frnum}: set node[${x}/${NUM_UNIV}, ${y}/${UNIV_LEN}] to 0x${hex(color)}`);
                yield* wait4port(() => !gp.isopen || (nodebufs[qent].frnum == frnum)); //wait for nodebuf to become available; this means wker is running 3 frames ahead of GPU xfr
//            nodes[x * frinfo.UNIV_LEN + y] = ((x + y) & 1)? (x && y && (x < frinfo.NUM_UNIV - 1) && (y < frinfo.UNIV_LEN - 1))? PALETTE[(x + frnum) % PALETTE.length]: WHITE: BLACK;
                nodebufs[qent].nodes[x][y] = color; //((x + y) & 1)? (x && y && (x < frinfo.NUM_UNIV - 1) && (y < frinfo.UNIV_LEN - 1))? PALETTE[(x + frnum) % (PALETTE.length - 1)]: WHITE: BLACK;
                nodebufs[qent].ready = -1; //mark all univ ready
//        yield wait(2000 + i); //use unique value for easier debug
            }
//    wait.cancel("done"); //cancel latest
//    yield; //kludge: wait for last timer to occur
//    process.stdout.end();
//    return -2; //for debug
}

step.debug = debug;
//done(step(main)); //sync; CAUTION: need to hoist additional in-line code from further below
step(main, done); //async
function done(retval)
{
    debug(`main retval: ${retval}`);
//show anything preventing process exit:
//    process.ste
    return;
    debug("remaining handles", process._getActiveHandles()); //.map((obj) => (obj.constructor || {}).name || obj));
//    process._getActiveHandles().forEach((obj) => ((obj.constructor || {}).name == "Timer")? debug(obj): null);
    debug("remaining requests", process._getActiveRequests());
}
debug("after main");

//check that event loop is not blocked:
//if (false)
(function idle()
{
//    if ((++idle.count || (idle.count = 1)) < 5) setTimeout(idle, 250); //.unref();
    setTimeout(idle, 250); //.unref();
    debug(`idle ${++idle.count || (idle.count = 1)}`); //, arguments.back); //how much overdue?
})();


///////////////////////////////////////////////////////////////////////////////
////
/// Misc utility/helper functions:
//

//step thru generator function:
//cb called async at end
//if !cb, blocks until generator function exits
function step(gen, async_cb)
{
//console.log(`step(gen? ${!!gen}, cb? ${!!async_cb})`);
    if (gen && !gen.next) gen = gen(); //func -> generator
    if (!step.gen) step.gen = gen;
    if (!step.async_cb) step.async_cb = async_cb;
//    for (;;)
//    {
//    try
    {
        const {done, retval} = step.gen.next? step.gen.next(step.retval): {done: true, retval: step.gen}; //done if !generator
//console.log("step: done?", done, "retval:", retval)
        if (step.debug) step.debug(() => `done? ${done}, retval ${typeof(retval)}: ${retval}, async? ${!!step.async_cb}`); //, overdue ${arguments.back}`);
//    if (typeof value == "function") value = value(); //execute wakeup events
//    step.value = value;
        if (done)
        {
            step.gen = null; //prevent further execution; step() will cause "undef" error
            if (step.async_cb) step.async_cb(retval);
//            setTimeout(() => process.exit(0), 2000); //kludge: dangling refs keep proc open, so forcefully kill it
//        return value;
        }
        return step.retval = retval;
    }
//    catch (exc) { console.error(`EXC: ${exc}`.red_lt); }
}


//delay for step/generator function:
function wait_msec(msec)
{
//    if (!step.async_cb)
//    return setTimeout.bind(null, step, msec); //defer to step loop
//    wait.pending || (wait.pending = {});
//    wait.pending[setTimeout(step, msec)] =
    debug("wait: set timeout ", msec);
//NOTE: Node timer api is richer than browser; see https://nodejs.org/api/timers.html
    /*wait.latest =*/ setTimeout(step, msec); //.unref(); //function(){ wait.cancel("fired"); step(); }, msec); //CAUTION: assumes only 1 timer active
//    wait.latest.unref();
//    return msec; //dummy retval for testing
    return msec; //dummy value for debug
}
function wait_sec(sec) { return wait_msec(sec * 1000); }
//wait.cancel = function wait_cancel(reason)
//{
//    reason && (reason += ": ");
//    debug(`${reason || ""}wait cancel? ${JSON.stringify(this.latest)}, has ref? ${((this.latest || {}).hasRef || noop)()}`.yellow_lt);
//    if (typeof this.latest != "undefined") clearTimeout(this.latest);
//    this.latest = undefined;
//}


//wait for Gpu port:
function* wait4port(pred, delay_time)
{
//debug("here1");
//    if (want_state) want_state = "open";
    if (!pred) pred = () => gp.isopen; //want_state) want_state = "open";
    const MAX_RETRY = 10;
    for (var retry = 0; /*retry < MAX_RETRY*/; ++retry)
    {
//debug("here2", retry);
//        let qent = gp.numfr % nodebufs.length; //circular queue
//        debug(`wait#[${retry}/${MAX_RETRY}] for GPU port '${pred}', state: open? ${gp.isopen}, fr# ${commas(gp.numfr)}, frtime ${prec(gp.numfr * gp.frtime, 1e3)} msec`);
//        if (!!gp.isopen == !!want_state) return;
//        while (gp.numfr < frnum) //wait for GPU to finish rendering before exit (prevent main from premature exit)
        if (pred()) { show_state(); return; }
        if (!(retry % 50)) show_state();
        yield wait_msec(delay_time || gp.frtime); //timing not critical here (render wker is 3 frames ahead of GPU or main thread is waiting for port to open/close)
    }
//debug("here3");
    exc(`GPU port '${pred}' timeout`);

    function show_state()
    {
//if(false)
        if (retry) debug(11, `waited ${retry}/${MAX_RETRY} for GPU port '${pred}', state: open? ${gp.isopen}, fr# ${commas(gp.numfr)}, frtime ${commas(prec(gp.numfr * gp.frtime * 1e3, 1e3))} msec`);
    }
}


//module.exports = gpuport;

//const nan = require('nan');
//console.log("\nnan", JSON.stringify(nan));
//const napi = require('node-addon-api');
//console.log("\nnapi.incl", JSON.stringify(napi.include));
//console.log("\nnapi.gyp", JSON.stringify(napi.gyp));


//TODO?
//var cluster = require('cluster');
//var workers = process.env.WORKERS || require('os').cpus().length;
//if (cluster.isMaster)
//{
//  console.log('start cluster with %s workers', workers);
//  for (var i = 0; i < workers; ++i)
//    {
//    var worker = cluster.fork().process;
//    console.log('worker %s started.', worker.pid);
//  }
//  cluster.on('exit', function(worker)
//  {
//    console.log('worker %s died. restart...', worker.process.pid);
//    cluster.fork();
//  });
//} else {
//worker thread
//}


//convert color space:
//HSV is convenient for color selection during fx gen, but display hardware requires RGB
//h, s, v args are 0..1
function hsv2rgb(h, s, v)
//based on sample code from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
{
    h *= 6; //[0..6]
    const segment = h >>> 0; //(long)hh; //convert to uint32
    const angle = (segment & 1)? h - segment: 1 - (h - segment); //fractional part
//NOTE: it's faster to do the *0xff >>> 0 in here than in toargb
    const p = ((v * (1.0 - s)) * 0xff) >>> 0;
    const qt = ((v * (1.0 - (s * angle))) * 0xff) >>> 0;
//redundant    var t = (v * (1.0 - (s * (1.0 - angle))) * 0xff) >>> 0;
    v = (v * 0xff) >>> 0;

    switch (segment)
    {
        default: //h >= 1 comes in here also
        case 0: return toargb(v, qt, p); //[v, t, p];
        case 1: return toargb(qt, v, p); //[q, v, p];
        case 2: return toargb(p, v, qt); //[p, v, t];
        case 3: return toargb(p, qt, v); //[p, q, v];
        case 4: return toargb(qt, p, v); //[t, p, v];
        case 5: return toargb(v, p, qt); //[v, p, q];
    }
}


//return "file:line#":
//mainly for debug or warning/error messages
function srcline(depth)
{
    const want_path = (depth < 0);
//    if (isNaN(++depth)) depth = 1; //skip this function level
    const frame = __stack[Math.abs(depth || 0) + 1]; //skip this stack frame
//console.error(`filename ${frame.getFileName()}`);
    return `  @${(want_path? nop_fluent: pathlib.basename)(frame.getFileName()/*.unquoted || frame.getFileName(), ".js"*/)}:${frame.getLineNumber()}`; //.gray_dk; //.underline; don't drop ext now that js calls C++ debug()
//    return `@${pathlib.basename(__stack[depth].getFileName(), ".js")}:${__stack[depth].getLineNumber()}`;
}

//C++ debug() shim:
//use lamba to defer arg prep; reduces cost of debug() when turned off (if lieu of macros)
//otherwise try to simulate C++ debug() behavior
function debug(args)
{
debugger;
    args = Array.from(arguments);
//    console.log.apply(null, Array.from(arguments).push_fluent(__parent_srcline));
    var level = ((typeof args[0] == "number") && (args.length > 1))? args.shift(): 0;
    if (level > debug.detail) return 0;
    if (typeof args[0] == "function") args[0] = args[0]();
//    args.splice(0, 0, [level, __srcline]);
//    return GpuPort.debug.apply(GpuPort, args); //.unshift_fluent(__srcline).unshift_fluent(level));
    return gp.debug(level, __parent_srcline, util.format.apply(util, args));
}
debug.detail = gp.detail(__file); //cache and assume won't change


//const myDate = new Date(Date.parse('04 Dec 1995 00:12:00 GMT'))
function datestr(date)
{
//    return (new Date()).toUTCString();
    const JAN = 1;
    if (!date) date = new Date();
    return `${date.getFullYear()}-${NN(JAN + date.getMonth())}-${NN(date.getDate())} ${NN(date.getHours())}:${NN(date.getMinutes())}:${NN(date.getSeconds())}`;
}

//left-pad:
function NN(val) { return ("00" + val.toString()).slice(-2); }
function NNNN(val) { return ("0000" + val.toString()).slice(-2); }

function prec(num, den)  { return Math.round(num * den) / den; }

function commas(num) { return num.toLocaleString(); } //grouping (1000s) default = true

function hex(thing) { return `0x${(thing >>> 0).toString(16)}`; }

//function noop() {} //dummy placeholder function

function extensions()
{
    Object.defineProperties(global,
    {
        __srcline: { get: function() { return srcline(1); }, },
        __parent_srcline: { get: function() { return srcline(2); }, },
    });
//    if (!Array.prototype.back) //polyfill
//        Object.defineProperty(Array.prototype, "back", { get() { return this[this.length - 1]; }});
//    Array.prototype.push_fluent = function push_fluent(args) { this.push.apply(this, arguments); return this; };
//    String.prototype.splitr = function splitr(sep, repl) { return this.replace(sep, repl).split(sep); } //split+replace
    String.prototype.unindent = function unindent(prefix) { unindent.svprefx = ""; return this.replace(prefix, (match) => match.substr((unindent.svprefix || (unindent.svprefix = match)).length)); } //remove indents
//    String.prototype.quote = function quote(quotype) { quotype || (quotype = '"'); return `${quotype}${this/*.replaceAll(quotype, `\\${quotype}`)*/}${quotype}`; }
//below are mainly for debug:
//    if (!String.prototype.echo)
//    String.prototype.echo = function echo(args) { args = Array.from(arguments); args.push(this.escnl); console.error.apply(null, args); return this; } //fluent to allow in-line usage
    String.prototype.trunc = function trunc(maxlen) { if (!maxlen) maxlen = 120; return (this.length > maxlen)? this.slice(0, maxlen) + ` ... (${commas(this.length - maxlen)})`: this; }
//    String.prototype.replaceAll = function replaceAll(from, to) { return this.replace(new RegExp(from, "g"), to); } //NOTE: caller must esc "from" string if contains special chars
//    if (!String.prototype.splitr)
    Object.defineProperties(String.prototype,
    {
//        escnl: { get() { return this.replace(/\n/g, "\\n"); }}, //escape all newlines
        nocomment: { get() { return this.replace(/\/\*.*?\*\//gm, "").replace(/(#|\/\/).*?$/gm, ""); }}, //strip comments; multi-line has priority over single-line; CAUTION: no parsing or overlapping
        nonempty: { get() { return this.replace(/^\s*\r?\n/gm , ""); }}, //strip empty lines
//        escre: { get() { return this.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }}, //escape all RE special chars
        json_tidy: { get() { return this.replace(/,"/g, ", \"").replace(/"(.*?)":/g, "$1: ").replace(/\d{5,}/g, (val) => `0x${(val >>> 0).toString(16)}`); }}, //tidy up JSON for readability
    });
}

//eof