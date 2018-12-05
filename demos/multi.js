#!/usr/bin/env node
//example multi-threaded render + I/O
//to run: DEBUG=* node --experimental-worker ./multi.js
//NOTE: use  --experimental-worker flag
//requires Node >= 10.5.0
//example code fragments at: https://nodejs.org/api/worker_threads.html

//to debug:
//node  debug  this-file  args
//cmds: run, cont, step, out, next, bt, .exit, repl

//RPi Node perf notes:
//slow memory, build Node without snapshot, JS on RPi1 ~ interp V8 speed, not JIT x86 speed: https://techfindings.one/archives/2053
//RPi I/O is limiting factor: https://github.com/nodejs/node/issues/11070
//Node advantage is concurrency: https://stackoverflow.com/questions/41928870/nodejs-much-slower-than-php
//Node on RPi setup: https://thisdavej.com/beginners-guide-to-installing-node-js-on-a-raspberry-pi/
//RPi headless config: https://raspberrypi.stackexchange.com/questions/14963/how-do-i-maximize-a-headless-pi-to-run-node-js
//GPIO ui, auto-boot Node: https://tutorials-raspberrypi.com/setup-raspberry-pi-node-js-webserver-control-gpios/

//node shm options:
//https://www.npmjs.com/package/shmmap
//https://www.npmjs.com/package/mmap-object
//https://github.com/ukrbublik/shm-typed-array
//https://www.npmjs.com/package/shared-buffer



'use strict'; //find bugs easier
//modules common to main + wker threads:
require("magic-globals"); //__file, __line, __stack, __func, etc
require('colors').enabled = true; //for console output (incl bkg threads); https://github.com/Marak/colors.js/issues/127

//console.log("here1", process.pid, "env", process.env);
const fs = require("fs");
const util = require("util");
//const buffer = require("buffer"); //needed for consts; only classes/methods are pre-included in global Node scope
const {elapsed} = require("./elapsed");
const {debug, caller} = require("./debug");
const tryRequire = require("try-require"); //https://github.com/rragan/try-require
//const multi = tryRequire('worker_threads'); //NOTE: requires Node >= 10.5.0; https://nodejs.org/api/worker_threads.html
//const {fork} = require("child_process"); //https://nodejs.org/api/child_process.html
const cluster = tryRequire("cluster") || {isMaster: true}; //https://nodejs.org/api/cluster.html; //http://learnboost.github.com/cluster
const /*{ createSharedBuffer, detachSharedBuffer }*/ sharedbuf = tryRequire('shared-buffer'); //https://www.npmjs.com/package/shared-buffer
cluster.proctype = cluster.isMaster? "Main": cluster.isWorker? "Wker": "?UNKN-TYPE?";
//console.log("here1");
//debug("here2");
//console.log("here3");
const gp = require("../build/Release/gpuport"); //{NUM_UNIV: 24, UNIV_MAXLEN: 1136, FPS: 30};
const {/*limit, listen, nodebufq,*/ NUM_UNIV, UNIV_MAXLEN, FPS} = gp; //make global for easier access
//console.log("here4");
//const /*GpuPort*/ {limit, listen, nodebufq} = GpuPort; //require('./build/Release/gpuport'); //.node');
const NUM_CPUs = require('os').cpus().length; //- 1; //+ 3; //leave 1 core for render and/or audio; //0; //1; //1; //2; //3; //bkg render wkers: 43 fps 1 wker, 50 fps 2 wkers on RPi
const QUELEN = 4;


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
debug("palette", BLACK, WHITE, hex(BLACK), hex(WHITE));
const PALETTE = [BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE]; //red 0b001, green 0b010, blue 0b100
//const PALETTEix = [0b000, 0b001, 0b010, 0b011, 0b100, 0b101, 0b110, 0b111]; //red 0b001, green 0b010, blue 0b100

show_env();
extensions(); //hoist for use by in-line code

//set up shm node bufs:
const {frctl, nodes} = make_nodes();


/////////////////////////////////////////////////////////////////////////////////
////
/// Main thread/process:
//

//step.debug = debug;
//if (multi.isMainThread)
function* main()
{
//    frame_test();
//fill_test();
//fwrite_test();
//    return;
//    const shdata =
//    {
//        hello: 1,
//    };
    debug(`main thread start`.cyan_lt);
//    yield wait(10000); //check if wker epoch reset affects main

    for (var w = 0; w < NUM_CPUs - 1; ++w) /*const wker =*/ start_wker(); //leave one core feel for sound + misc
//    for (let i = 1; i <= 10; ++i)
//        setTimeout(() =>
//        {
//            debug(`send wker req#${i}`.blue_lt)
//            wker.postMessage("message", `req-from-main#${shdata.main_which = i}`);
//        }, 1000 * i);
    for (var i = 0; i < 10; ++i)
    {
        yield wait(0.5 * 1e3);
        debug(`main loop[${i}/10]. node234 = ${hex(nodes[2][3][4])}`);
    }
    debug("main idle".cyan_lt);
}


function start_wker(filename, data, opts)
{
//    debug("env", process.env);
    data = Object.assign({}, data, {ID: ++start_wker.count || (start_wker.count = 1)}); //shallow copy + extend
//    const OPTS = //null;
//    {
//        cwd: process.cwd(),
    const env = Object.assign({}, process.env, {wker_data: JSON.stringify(data)}); //shallow copy + extend
//        silent: false, //inherit from parent; //true, //stdin/out/err piped to parent
//        eval: false, //whether first arg is javascript instead of filename
//        workerData: data || {ID: start_wker.count || 0}, //cloned wker data
//        stdin: false, //whether to give wker a process.stdin
//        stdout: false, //whether to prevent wker process.stdout going to parent
//        stderr: false, //whether to prevent wker process.stderr going to parent
//    };
//    const wker = new multi.Worker(filename || __filename, opts || OPTS)
//    const wker = cluster.fork(data || {wker_data: {ID: start_wker.count || 0}})
    const NO_PID = "?NO-PID?";
//    opts = Object.assign({}, OPTS, opts || {});
    const wker = cluster.fork(/*filename || __filename, Array.from(process.argv).slice(2), opts) //|| OPTS)*/ env)
        .on("online", () => debug(`wker ${(wker/*.threadId*/ .process || {}).pid || NO_PID} is on-line`.cyan_lt)) //, arguments))
        .on('message', (data) => debug("msg from wker".blue_lt, data)) //, /*arguments,*/ "my data".blue_lt, shdata)) //args: {}, require, module, filename, dirname
        .on('error', (data) => debug("err from wker".red_lt, data, arguments))
        .on('exit', (code) => debug("wker exit".yellow_lt, code)) //, arguments)) //args: {}, require, module, filename, dirname
//          if (code !== 0) reject(new Error(`Worker stopped with exit code ${code}`));
        .on('disconnect', () => debug(`wker disconnect`.cyan_lt)); //, arguments));
    debug(`launched wker#${start_wker.count} pid ${(wker.process || {}).pid || NO_PID}, now have ${Object.keys(cluster.workers).length} wkers: ${Object.keys(cluster.workers).join(", ")} ...`.cyan_lt);
//    if (wker.connected) wker.send()
    return wker;
}


//debug("more down here", multi.isMainThread);
//const {Worker, isMainThread, parentPort, workerData, MessageChannel} = require('worker_threads'); //https://nodejs.org/api/worker_threads.html
//wker: parentPort.postMessage() -> parent wker.on("message")
//parent: wker.postMessage() -> wker parentPort.on("message")
//wker workerData = clone of data from ctor by parent

//example code snipets at: //cmds: run, cont, step, out, next, bt, .exit, repl

//boilerplate from: https://nodejs.org/api/worker_threads.html
//if (isMainThread) {
//    module.exports = async function parseJSAsync(script) {
//      return new Promise((resolve, reject) => {
//        const worker = new Worker(__filename, {
//          workerData: script
//        });
//        worker.on('message', resolve);
//        worker.on('error', reject);
//        worker.on('exit', (code) => {
//          if (code !== 0)
//            reject(new Error(`Worker stopped with exit code ${code}`));
//        });
//      });
//    };
//  } else {
//    const { parse } = require('some-js-parsing-library');
//    const script = workerData;
//    parentPort.postMessage(parse(script));
//  }


/////////////////////////////////////////////////////////////////////////////////
////
/// Wker threads:
//


function* wker(wker_data)
{
//    debug("wker env", process.env);
    wker_data = JSON.parse(process.env.wker_data); //TODO: move up a level
    debug(`wker thread start`.cyan_lt, "cloned data", /*process.env.*/wker_data, "id", cluster.worker.id); //multi.workerData);
//    for (let i = 1; i <= 3; ++i)
//        setTimeout(() => multi.parentPort.postMessage(`hello-from-wker#${multi.workerData.wker_which = i}`, {other: 123}), 6000 * i);
//    multi.parentPort.on("message", (data) => debug("msg from main".blue_lt, data, /*arguments,*/ "his data".blue_lt, multi.workerData)); //args: {}, require, module, filename, dirname
//    setTimeout(() => { debug("wker bye".red_lt); process.exit(123); }, 15000);
    for (var fr = 0; fr < 4; ++fr)
    {
        yield wait(0.6 * 1e3); //.6, 1.2, 1.8, 2.4 sec
        nodes[2][3][4] = PALETTE[fr + 1];
        debug(`wker loop[${fr}/4]. set node234 to ${hex(PALETTE[fr + 1])}`);
    }
//    process.send({data}, (err) => {});
    debug("wker idle".cyan_lt);
}


/////////////////////////////////////////////////////////////////////////////////
////
/// Tests:
//

function frame_test()
{
    debug("mem test");
    const outfs = fs.createWriteStream("dummy.yalp");
    outfs.write(`#single frame ${datestr()}\n`);
    outfs.write(`#NUM_UNIV: ${NUM_UNIV}, UNIV_MAXLEN: ${UNIV_MAXLEN}\n`);
    elapsed(0);
    for (var x = 0; x < NUM_UNIV; ++x)
    {
        for (var y = 0; y < UNIV_MAXLEN; ++y)
//        {
//            nodes[x][y] = PALETTE[(x + y) % PALETTE.length];
            nodes[x][y] = ((x + y) & 1)? PALETTE[1 + x % (PALETTE.length - 1)]: BLACK;
//            debug(`n[${x}][${y}] <- ${PALETTEix[1 + x % (PALETTE.length - 1)]}`);
//        }
//        outfs.write(`${x}: ` + nodes[x].toString("hex") + "\n"); //~300K/frame
//        outfs.write(`${x}: ` + nodes[x].toString("base64") + "\n"); //~300K/frame
    }
//    debug(JSON.stringify({frnum: -1, nodes}).json_tidy);
//    outfs.write(JSON.stringify({frnum: -1, nodes}/*, null, 2*/).json_tidy + "\n"); //~460K/frame
    outfs.write(`${x}: ` + util.inspect(nodes) + "\n"); //~300K/frame
    outfs.end("#eof; elapsed ${prec(elapsed(), 1e6)} msec"); //close file
}


const SEQLEN = 120 * FPS; //total #fr in seq

function fill_test()
{
    debug("fill test");
    const REPEATS = 100;

    elapsed(0);
    for (var loop = 0; loop < REPEATS; ++loop)
    {
        for (var numfr = 0; numfr < SEQLEN; ++numfr)
    //        for (var c = 0; c < PALETTE.length; ++c)
                for (var x = 0; x < NUM_UNIV; ++x)
                    for (var y = 0; y < UNIV_MAXLEN; ++y)
                        nodes[x][y] = PALETTE[numfr % PALETTE.length];
//perf:
//PC: [2.889] filler loop#99/100, avg time 0.008106 msec
//RPi2: [40.532] filler loop#99/100, avg time 0.113723 msec ~= 14x slow
        debug(`filler loop#${loop}/${REPEATS}, avg time ${prec(elapsed() / numfr / loop, 1e6)} msec`);
    }
}

function fwrite_test()
{
    debug("fwrite test");
    const REPEATS = 3;
    const outfs = fs.createWriteStream("dummy.yalp");
    outfs.write(`#fill from palette frames ${datestr()}\n`);
    outfs.write(`#NUM_UNIV: ${NUM_UNIV}, UNIV_MAXLEN: ${UNIV_MAXLEN}, #fr: ${SEQLEN}, repeats: ${REPEATS}\n`);

    elapsed(0);
    for (var loop = 0; loop < REPEATS; ++loop)
    {
        for (var numfr = 0; numfr < SEQLEN; ++numfr)
        {
//            elapsed.pause();
    //        for (var c = 0; c < PALETTE.length; ++c)
                for (var x = 0; x < NUM_UNIV; ++x)
                    for (var y = 0; y < UNIV_MAXLEN; ++y)
                        nodes[x][y] = PALETTE[numfr % PALETTE.length];
//            elapsed.resume();
            outfs.write(JSON.stringify({frnum: numfr, nodes}).json_tidy + "\n");
        }
//perf:
//RPi2: [40.532] filler loop#99/100, avg time 0.113723 msec
//PC: [26.490] filler loop#2/3, avg time 2.452778 msec 
        debug(`filler loop#${loop}/${REPEATS}, avg time ${prec(elapsed() / numfr / (loop + 1), 1e6)} msec`);
    }
    outfs.end("#eof; elapsed ${prec(elapsed(), 1e6)} msec"); //close file
}


///////////////////////////////////////////////////////////////////////////////
////
/// Misc utility/helper functions:
//

//function cache_pad64(val) { return ((val + 15) >>> 0) & ~15; }

//set up 2D array of nodes in shared memory:
//const NUM_UNIV = 24;
//const UNIV_MAXLEN = 1136; //max #nodes per univ
//const UNIV_ROWSIZE = UNIV_MAXLEN * Uint32Array.BYTES_PER_ELEMENT; //sizeof(Uin32) * univ_len
//const shbuf = new SharedArrayBuffer(NUM_UNIV * UNIV_ROWSIZE); //https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer
//const nodes = Array.from({length: NUM_UNIV}, (undef, univ) => new Uint32Array(shbuf, univ * UNIV_ROWSIZE, UNIV_ROWSIZE / Uint32Array.BYTES_PER_ELEMENT)); //CAUTION: len = #elements, !#bytes; https://stackoverflow.com/questions/3746725/create-a-javascript-array-containing-1-n
//nodes.forEach((row, inx) => debug(typeof row, typeof inx, row.constructor.name)); //.prototype.inspect = function() { return "custom"; }; }});
//debug(`shm nodes ${NUM_UNIV} x ${UNIV_MAXLEN} x ${Uint32Array.BYTES_PER_ELEMENT} = ${commas(NUM_UNIV *UNIV_MAXLEN *Uint32Array.BYTES_PER_ELEMENT)} = ${commas(shbuf.byteLength)} created`.pink_lt);
function make_nodes()
{
    const shmkey = (0xfeed0000 | NNNN_hex(UNIV_MAXLEN)) >>> 0;
    const UNIV_BYTELEN = /*cache_pad64*/(UNIV_MAXLEN) * Uint32Array.BYTES_PER_ELEMENT;
    debug(`make_nodes: shmkey ${typeof shmkey}:`, hex(shmkey), "univ bytelen", commas(UNIV_BYTELEN), `"owner? ${cluster.isMaster}`);
    const shbuf = sharedbuf.createSharedBuffer(shmkey, (QUELEN * NUM_UNIV + 1) * UNIV_BYTELEN, cluster.isMaster); //master creates new, slaves acquire existing; one extra univ for fr ctl
    const nodes = Array.from({length: QUELEN}, (undef, quent) =>
        Array.from({length: NUM_UNIV}, (undef, univ) =>
            new Uint32Array(shbuf, (quent * NUM_UNIV + univ) * UNIV_BYTELEN, UNIV_MAXLEN))); //CAUTION: len = #elements, !#bytes; https://stackoverflow.com/questions/3746725/create-a-javascript-array-containing-1-n
    const FRNUM = 0, PREVFR = 1, FRTIME = 2, PREVTIME = 3, READY = 4; //frctl ofs for each nodebuf quent
    const frctl = Array.from({length: QUELEN}, (undef, quent) =>
    {
        frnum: new Uint32Array(shbuf, QUELEN * NUM_UNIV * UNIV_BYTELEN, QUELEN * 4);
    });
    debug(`shm fr ctl ${commas(QUELEN * 4)}, nodebuf queue ${commas(nodes.length)} x ${commas(nodes[0].length)} x ${commas(nodes[0][0].length)} ${cluster.isMaster? "created": "acquired"}`.pink_lt);
    if (cluster.isMaster)
    {
        nodes.forEach((quent) => quent.forEach((univ) => univ.forEach((node) => node = BLACK)));
        for (var q = 0; q < QUELEN; ++q) frctl[]
    }
//    if (cluster.isMaster) cluster.fork();
    return {frctl, nodes};
//??    detachSharedBuffer(sharedBuffer); 
//    process.exit(0);
}


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
//    try //omit this for easier dev/debug
    {
        const {done, value} = step.gen.next? step.gen.next(step.value): {done: true, value: step.gen}; //done if !generator
        if (step.debug) step.debug(`done? ${done}, retval ${typeof(value)}: ${value}, async? ${!!step.async_cb}`); //, overdue ${arguments.back}`);
//    if (typeof value == "function") value = value(); //execute wakeup events
//    step.value = value;
        if (done)
        {
            step.gen = null; //prevent further execution; step() will cause "undef" error
            if (step.async_cb) step.async_cb(value);
//            setTimeout(() => process.exit(0), 2000); //kludge: dangling refs keep proc open, so forcefully kill it
//        return value;
        }
        return step.value = value;
    }
//    catch (exc) { console.error(`EXC: ${exc}  @${caller()}`.red_lt); } //, __stack); }
}


//delay for step/generator function:
function wait(msec)
{
//    if (!step.async_cb)
//    return setTimeout.bind(null, step, msec); //defer to step loop
//    wait.pending || (wait.pending = {});
//    wait.pending[setTimeout(step, msec)] =
    !step.debug || step.debug("wait: set timeout ", msec);
//NOTE: Node timer api is richer than browser; see https://nodejs.org/api/timers.html
    /*wait.latest =*/ setTimeout(step, msec); //.unref(); //function(){ wait.cancel("fired"); step(); }, msec); //CAUTION: assumes only 1 timer active
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


//from https://stackoverflow.com/questions/287903/what-is-the-preferred-syntax-for-defining-enums-in-javascript
//TODO: convert to factory-style ctor
class Enum
{
    constructor(enumObj)
    {
        const handler =
        {
            get(target, name)
            {
                if (target[name]) return target[name];
                throw new Error(`No such enumerator: ${name}`);
            },
        };
        return new Proxy(Object.freeze(enumObj), handler);
    }
}
//usage: const roles = new Enum({ ADMIN: 'Admin', USER: 'User', });


//function debug(args) { console.log.apply(null, Array.from(arguments).push_fluent(__parent_srcline)); debugger; }

function hex(thing) { return `0x${(thing >>> 0).toString(16)}`; }


//adjust precision on a float val:
function prec(val, scale) { return Math.round(val * scale) / scale; }

//makes numbers easier to read:
function commas(num) { return num.toLocaleString(); } //grouping (1000s) default = true


//const myDate = new Date(Date.parse('04 Dec 1995 00:12:00 GMT'))
function datestr(date)
{
    const JAN = 1;
    date || (date = new Date());
    return `${date.getFullYear()}-${NN(JAN + date.getMonth())}-${NN(date.getDate())} ${NN(date.getHours())}:${NN(date.getMinutes())}:${NN(date.getSeconds())}`;
}

//left-pad:
function NN(val) { return ("00" + val.toString()).slice(-2); }
function NNNN(val) { return ("0000" + val.toString()).slice(-2); }

//express decimal as hex:
function NNNN_hex(val) { return (Math.floor((val / 1000) % 10) * 0x1000) | (Math.floor((val / 100) % 10) * 0x100) | (Math.floor((val / 10) % 10) * 0x10) | (val % 10) >>> 0; }

//show info about env:
function show_env(more_details)
{
//debug("env", process.env);
    debug(`Module '${module.filename.split(/[\\\/]/g).back}', has parent? ${!!module.parent}`.pink_lt);
    debug(`Node v${process.versions.node}, V8 v${process.versions.v8}, N-API v${process.versions.napi}`.pink_lt);
    debug(`${(process.env.NODE_ENV == "production")? "Prod": "Dev"} mode, ${plural(NUM_CPUs)} CPU${plural.cached}`.pink_lt);
    if (more_details) debug("Node", process.versions);
    show_args();
}

function show_args()
{
    debug(`${plural((process.argv || []).length)} arg${plural.cached}:`.blue_lt);
    (process.argv || []).forEach((arg, inx, all) => debug(`  [${inx}/${all.length}] '${arg}'`.blue_lt));
}

//satisfy the grammar police:
function plural(ary, suffix)
{
//    if (!arguments.length) return plural.cached;
    const count = Array.isArray(ary)? (ary || []).length: ary;
    plural.cached = (count != 1)? (suffix || "s"): "";
    return ary;
}
function plurals(ary) { return plural(ary, "s"); }
function plurales(ary) { return plural(ary, "es"); }


function quit(msg, code)
{
//    if (typeof msg == "number" && )
    console.error(`${msg}  @${caller(+1)}`.red_lt);
    process.exit(isNaN(code)? 1: code);
}

function exc(msg)
{
    msg = util.format.apply(null, arguments);
    throw msg.red_lt;
}

function extensions()
{
//    String.prototype.toCamelCase = function()
//    {
//        return this.replace(/([a-z])((\w|-)+)/gi, (match, first, other) => `${first.toUpperCase()}${other.toLowerCase()}`);
//    }
//    Object.defineProperty(String.prototype, "extend_color", {get() { return extend_color(this); }, });
//    Object.defineProperty(Array.prototype, "top",
//    {
//        get() { return this[this.length - 1]; },
//        set(newval) { this.pop(); this.push(newval); },
//    });
//    Object.defineProperty(global, '__parent_line', { get: function() { return __stack[2].getLineNumber(); } });
//    Array.prototype.unshift_fluent = function(args) { this.unshift.apply(this, arguments); return this; };
//    String.prototype.grab = function(inx) { return grab(this, inx); }
    Object.defineProperties(String.prototype,
    {
//            escnl: { get() { return this.replace(/\n/g, "\\n"); }}, //escape all newlines
//            nocomment: { get() { return this.replace(/\/\*.*?\*\//gm, "").replace(/(#|\/\/).*?$/gm, ""); }}, //strip comments; multi-line has priority over single-line; CAUTION: no parsing or overlapping
//            nonempty: { get() { return this.replace(/^\s*\r?\n/gm , ""); }}, //strip empty lines
//            escre: { get() { return this.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }}, //escape all RE special chars
        json_tidy: { get() { return this.replace(/,"/g, ", \"").replace(/"(.*?)":/g, "$1: ").replace(/\d{5,}/g, (val) => `0x${(val >>> 0).toString(16)}`); }}, //tidy up JSON for readability
    });
//    Uint32Array.prototype.inspect = function() //not a good idea; custom inspect deprecated in Node 11.x
}


/*
function traverse(node, cb, parent, childname)
{
    if (typeof node != "object") return;
    cb(node, parent, childname); //visit parent node first
    for (var i in node)
        traverse(node[i], cb, node, i);
}


function ansi_color(code) { return `\\x1b\\[${code}m`; }


function extend_color(str)
{
    const DelayColorEnd_xre = new XRegExp(`
        ^
        (?<leader> .* )
        (?<terminator> ${ansi_color(0)} )
        (?<trailer> .* )
        $
        `, "xi");
    return (str || "").replace(DelayColorEnd_xre, "$1$3$2");
}

process.on("beforeExit", (xcode) => console.error(`${eof.remaining_files} remaining files`[eof.remaining_files? "red": "green"]));


function btwn(val, min, max) { return (val >= min) && (val <= max); }
*/


/////////////////////////////////////////////////////////////////////////////////
////
/// Unit test/CLI trailer:
//

//if (require.main === module)
if (module.parent) exc("not a callable module; run it directly");
//if (!multi) exc(`worker_threads not found; did you add "--experiemtal-worker" to command line args?`);
//if (multi.isMainThread) step(main);
//if (!process.env.wker_data) step(main);
if (cluster.isMaster) step(main);
else if (cluster.isWorker) step(wker, () => process.disconnect()); //, JSON.parse(process.env.wker_data));
else exc(`don't know what type process ${process.pid} is`);
//debug(`${/*multi.isMainThread*/ /*cluster.isMaster? "Main": "Wker"*/ cluster.proctype} fell thru - process might exit.`.yellow_lt); //main() or wker() should probably not return; could end process prematurely
//if (cluster.isWorker) process.disconnect();
//show what's keeping proc alive:
if (process._getActiveHandles().length)
    debug(`${cluster.proctype} remaining handles`.yellow_lt, process._getActiveHandles().map((obj) => (obj.constructor || {}).name || obj));
//    process._getActiveHandles().forEach((obj) => ((obj.constructor || {}).name == "Timer")? debug(obj): null);
if (process._getActiveRequests().length)
    debug(`${cluster.proctype} remaining requests`.yellow_lt, process._getActiveRequests());

//eof