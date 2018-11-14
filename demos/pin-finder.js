#!/usr/bin/env node
// d8 --trace_opt --trace-deopt --prof
// --allow-natives-syntax
//Pin finder RGB patterns: tests GPU config, GPIO pins, and WS281X connections
//Sends out a different pattern on each GPIO pin to make them easier to identify.
//Copyright (c) 2016-2018 Don Julien
//Can be used for non-commercial purposes.
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture re-write working on RPi
//ver 1.0a DJ  9/24/17  minor clean up
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas
//ver 1.0.18 DJ  1/9/18  updated for multi-threading, simplified
//ver 1.0.18b DJ  6/6/18  minor api cleanup; misc fixes to multi-threading

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output (all threads)
const os = require('os'); //cpus()
//const pathlib = require('path');
//const {blocking, wait} = require('blocking-style');
//const cluster = require('cluster');
//const JSON = require('circular-json'); //CAUTION: replace std JSON with circular-safe version
const {debug} = require('./shared/debug');
//const memwatch = require('memwatch-next');
//const {Screen, GpuCanvas, UnivTypes} = require('gpu-friends-ws281x');
const {Screen, GpuCanvas, UnivTypes/*, wait, elapsed, cluster, AtomicAdd, optimizationStatus*/} = require('gpu-friends-ws281x');
//const EPOCH = cluster.isWorker? elapsed(+process.env.EPOCH): elapsed(); //use consistent time base for logging
//debug(`epoch ${EPOCH}, master? ${cluster.isMaster}`.blue_lt); //TODO: fix shared time base
//console.log(JSON.stringify(Screen));
//process.exit();

/*
const {shmbuf} = require('gpu-friends-ws281x');
const shmkey = 0x123456;
const THIS_shmbuf = new Uint32Array(shmbuf(shmkey, 4 * Uint32Array.BYTES_PER_ELEMENT)); //cluster.isMaster));
//THIS_shmbuf.fill(0); //start with all pixels dark
console.log("at add:", AtomicAdd(THIS_shmbuf, 0, 2));
process.exit();
*/


/*
function thr()
{
    console.log("here1");
    try { var retval = 12; console.log("here2"); ++x; return retval; }
//    catch (exc) { var svexc = exc; }
    finally { console.log("here3"); }
    console.log("here4");
    return retval;
}
var val = thr();
console.log("val", val);
process.exit();
*/


////////////////////////////////////////////////////////////////////////////////
////
/// Config settings:
//

const DURATION = 60 -55+25; //how long to run (sec)
//const PAUSE = 5; //how long to pause at end
const FPS = 30 -28; //target animation speed (may not be achievable; max depends on screen settings)


//display settings:
//NOTE: some of these only apply when GPIO pins are *not* presenting VGA signals (ie, dev mode); otherwise, they interfere with WS281X signal fmt
const OPTS =
{
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
    DEV_MODE: true, //disable WS281X formatting on screen (for demo purposes); TODO: use process.env.NODE_ENV?
    WANT_STATS: true, //collect performance stats
//    AUTO_PLAY: false, //true, //start playback
//    WS281X_DEBUG: true, //show timing debug info
//    UNIV_TYPE: UnivTypes.CHPLEX_SSR,
//    gpufx: "./pin-finder.glsl", //generate fx on GPU instead of CPU
    FPS, //target fps; throttle back if running faster than this
    TITLE: "PinFinder",
//    EXTRA_LEN: 1 + this.NUM_WKERS,
//    SHM_KEY: process.env.SHM_KEY,
//choose one of options below for performance tuning:
//    NUM_WKERS: 0, //whole-house fg render
    NUM_WKERS: 1, //whole-house bg render
//    NUM_WKERS: os.cpus().length, //1 bkg wker for each core (optimal)
//    NUM_WKERS: 6, //hard-coded #bkg wkers
};
if (OPTS.DEV_MODE) Screen.vgaIO = true; //force full screen (dev/test only)


//canvas settings:
//const SPEED = 0.5; //animation step speed (sec)
//const FPS = 30; //estimated animation speed
const NUM_UNIV = 24; //#universes; can't exceed #VGA output pins (24) without external h/w mux
const UNIV_LEN = Screen.vgaIO? Screen.height: 30; //universe length; can't exceed #display lines; show larger pixels in dev mode
const WKER_UNIV = Math.ceil(NUM_UNIV / (OPTS.NUM_WKERS || 1)); //#univ for each bkg wker to render
debug("Screen %d x %d @%d Hz, is RPi? %d, vgaIO? %d, env %s".cyan_lt, Screen.width, Screen.height, round(Screen.fps, 10), Screen.isRPi, Screen.vgaIO, process.env.NODE_ENV || "(dev)");
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//GPU canvas:
//global to reduce parameter passing (only one needed)
//no-NOTE: must occur before creating models and bkg wkers (sets up shm, etc)
//debug(`pid '${process.pid}' master? ${cluster.isMaster}, wker? ${cluster.isWorker}`.cyan_lt);
const canvas = new GpuCanvas(NUM_UNIV, UNIV_LEN, OPTS);

//if (canvas.isMaster)
//{
//    debug("TODO: MP3 any".yellow_lt);
//    debug("TODO: http stats server/monitor".yellow_lt);
//    debug("TODO: js encode".yellow_lt);
//    debug("TODO: emit warnings?".yellow_lt);    
//    debug("TODO: .fseq reader?".yellow_lt);    
//}

//no worky:
//process.on('uncaughtException', err => {
//    console.error(err, 'Uncaught Exception thrown');
//    process.exit(1);
//  });


////////////////////////////////////////////////////////////////////////////////
////
/// Models/effects rendering:
//

//spread models across multiple threads:
//each model handles WKER_UNIV universes
const models = []; //list of models/effects to be rendered
for (var u = 0, wker = 0; u < NUM_UNIV; u += WKER_UNIV, ++wker)
//    if (!OPTS.NUM_WKERS || (canvas.WKER_ID == wker))
    models.push(new PinFinder({univ: [u, Math.min(u + WKER_UNIV, NUM_UNIV)], affinity: wker})); //, get name() { return `Univ#${this.ubegin}..${this.uend - 1}`; }, })); //, wker: Math.floor(u / WKER_UNIV)}));
models.mine = function(want_names)
{
    return want_names?
        models.map(m => m.ismine()? m.name || m.constructor.name: "(not this one)"): //list
        models.reduce((prev, m, inx, all) => prev + m.ismine(), 0); //count only
}
/*
if (OPTS.NUM_WKERS > 1) //split up models across multiple bkg threads
    for (var u = 0; u < NUM_UNIV; ++u)
//        models.push((canvas.WKER_ID == u >> 3)? new PinFinder({univ_num: u, name: `Univ#${u}`}): canvas.isMaster? new BkgWker(u >> 3): null);
        models.push(new PinFinder({univ_num: u, name: `Univ#${u}`, wker: Math.floor(u / WKER_UNIV)}));
else //whole-house on main or bkg thread
//    models.push(/-*new*-/ BkgWker(/-*new*-/ PinFinder({name: "WholeHouse"})));
    models.push(new PinFinder({name: "WholeHouse"})); //, wker: OPTS.NUM_WKERS? 0: undefined}));
//    models.push((!OPTS.NUM_WKERS || canvas.isWorker)? new PinFinder(): new BkgWker());
*/
debug(`${canvas.proctype}# ${canvas.WKER_ID} '${process.pid}' has ${models.mine(false)}/${models.length} model(s) to render: ${models.mine(true)/*.join(",")*/}`.blue_lt);
//console.log(JSON.stringify(models));
//process.exit();

//ARGB primary colors:
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
//const YELLOW = 0xffffff00;
//const CYAN = 0xff00ffff;
//const MAGENTA = 0xffff00ff;
//const WHITE = 0xffffffff;
const BLACK = 0xff000000; //NOTE: alpha must be on to take effect

//const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA];


//pin finder model/fx:
//generates a different 1-of-N pattern for each GPIO pin
//usage:
//  new PinFinderModel(univ_num, univ_len, color)
//  where:
//    univ_num = universe# (screen column); 0..23; also determines pattern repeat factor
//    univ_len = universe length (screen height)
//    color = RGB color to use for this universe (column)
//NOTE: use function ctor instead of ES6 class syntax to allow hoisting
function PinFinder(opts)
{
    if (!(this instanceof PinFinder)) return new PinFinder(opts); //{univ, name, wker}
//    {
//        if (OPTS.NUM_WKERS && canvas.isMaster) return opts; //don't need real model; return placeholder
//    }
//    if (isNaN(++PinFinder.count)) PinFinder.count = 1; //do this before affinity check so model names are unified across processes (easier debug)
//    this.name = (opts || {}).name || `PinFinder#${PinFinder.count}`;
//    const whole_house = (typeof (opts || {}).univ == "undefined");
//        this.univ_num = univ_num; //stofs = univ_num * univ_len; //0..23 * screen height (screen column)
    const [univ_begin, univ_end] = (opts || {}).univ;
//    const multi = ((opts || {}).univ || []).length; //caller wants univ range (although it could be just one)
//    this.univ_begin = multi? opts.univ[0]: (opts || {}).univ || 0;
//    this.univ_end = multi? opts.univ.slice(-1)[0]: (typeof (opts || {}).univ != "undefined")? opts.univ + 1: NUM_UNIV; //range vs. single univ vs. whole-house
    debug(`univ begin ${typeof univ_begin}:${univ_begin}, end ${typeof univ_end}:${univ_end}`);
    this.name = (opts || {}).name || `PinFinder[${this.univ_begin}..${this.univ_end - 1}]`; //`PinFinder#${PinFinder.count}`;
//    this.affinity = (typeof (opts || {}).affinity != "undefined")? opts.affinity: (PinFinder.count || 0)/*models.length*/ % (OPTS.NUM_WKERS || 1); //which thread to run on; default to round robin assignment
//    if (isNaN(++PinFinder.count)) PinFinder.count = 1;
    if (!ismine()) return; //not for this wker thread; bypass remaining init
//        this.pixels = canvas.pixels.slice(begin_univ * UNIV_LEN, end_univ * UNIV_LEN); //pixels for this/these universe(s) (screen column(s))
//        this.repeat = 9 - (univ & 7); //patterm repeat factor; 9..2
//        this.height = univ_len; //universe length (screen height)
//        this.color = (opts || {}).color || WHITE;
//        this.frnum = 0; //frame# (1 animation step per frame)
//TODO: additional init here (models for this wker only)
}
PinFinder.prototype.ismine =
function ismine()
{
    return !OPTS.NUM_WKERS || (this.affinity == canvas.WKER_ID);
}
//synchronous render:
//handles 1 or more universes
PinFinder.prototype.render =
function render(frnum, timestamp)
{
//        if (isNaN(++this.frnum)) this.frnum = 0; //frame# (1 animation step per frame)
    for (var u = this.univ_begin/*, uofs = u * canvas.height*/; u < this.univ_end; ++u) //, uofs += canvas.height)
    {
        var color = [RED, GREEN, BLUE][u >> 3]; //Math.floor(x / 8)];
        var repeat = 9 - (u & 7); //(x % 8);
//        for (var n = 0, c = -frnum % repeat; n < canvas.height; ++n, ++c)
        for (var n = 0; n < canvas.height; ++n)
//, csel = -this.count++ % this.repeat
//            if (csel >= this.repeat) csel = 0;
//if ((t < 3) && !x && (y < 10)) console.log(t, typeof c, c);
//                    var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
//                    var repeat = 9 - (x % 8);
//                    canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
//                    canvas.pixel(x, y, c? BLACK: color);
            canvas.pixels[/*uofs + n*/[u, n]] = ((n - frnum) % repeat)? BLACK: color;
//                canvas.pixels[uofs + n] = csel? BLACK: this.color;
//            this.pixels[y] = ((y - frnum) % this.repeat)? BLACK: this.color;
    }
}


/*
//verify alignment of corners:
function calibrate()
{
    canvas.pixels[0] = RED;
    canvas.pixels[(NUM_UNIV - 1) * UNIV_LEN] = GREEN;
    canvas.pixels[UNIV_LEN - 1] = RED | GREEN; //YELLOW;
    canvas.pixels[NUM_UNIV * UNIV_LEN - 1] = BLUE;
    canvas.paint();
//yield wait(10);
//return;
}
*/


////////////////////////////////////////////////////////////////////////////////
////
/// Main playback logic (uses synchronous coding style to simplify timing flow):
//

canvas.playback =
function* playback() //onexit)
{
throw new Error("todo".red_lt);
    debug(`begin: startup took %d sec, now run fx for %d sec`.green_lt, round(process.uptime(), 10), DURATION);
    var perf = {render: 0, /*idle: 0,*/ paint: 0, cpu: process.cpuUsage()};
//    this.on('stats', stats_cb, perf);
//    canvas.wait_stats = true; //request wait() stats
//    onexit(BkgWker.closeAll);
//    for (var i = 0; i < 100; ++i)
//        models.forEach((model) => { model.render(i); });

//    var started = now_sec();
    this.duration = DURATION; //set progress bar limit
    perf.time = this.elapsed = 0; //elapsed(0);
//    if (OPTS.gpufx) canvas.fill(GPUFX); //generate fx on GPU
//    var hd = new memwatch.HeapDiff();
//debug(optimatizationStatus(this.render).blue_lt);
    for (var frnum = 1; this.elapsed <= DURATION; ++frnum)
    {
//        var now = this.elapsed;
        perf.render -= perf.time; //this.elapsed; //exclude non-rendering (paint + V-sync wait) time
//        models.renderAll(canvas, frnum); //forEach((model) => { model.render(frnum); });
//render all models (synchronous):
//        models.forEach(model => { model.render(frnum, canvas.elapsed); }); //bkg wker: sync render; main thread: sync render or async ipc
        yield this.render(frnum, perf.time); //use frame timestamp for render time of all models
        perf.time = this.elapsed;
//        render(frnum);
//        yield canvas.render(frnum);
        perf.render += perf.time; //this.elapsed;

//NOTE: pixel (0, 0) is upper left on screen
//NOTE: the code below will run up to ~ 35 FPS on RPi 2
//        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
/*
            for (var x = 0, xofs = 0; x < canvas.width; ++x, xofs += canvas.height)
            {
                var color = [RED, GREEN, BLUE][x >> 3]; //Math.floor(x / 8)];
                var repeat = 9 - (x & 7); //(x % 8);
                for (var y = 0, c = -t % repeat; y < canvas.height; ++y, ++c)
                {
                    if (c >= repeat) c = 0;
//if ((t < 3) && !x && (y < 10)) console.log(t, typeof c, c);
//                    var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
//                    var repeat = 9 - (x % 8);
//                    canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
//                    canvas.pixel(x, y, c? BLACK: color);
//                    canvas.pixels[xofs + y] = ((y - t) % repeat)? BLACK: color;
                    canvas.pixels[xofs + y] = c? BLACK: color;
                }
//if (y > 1) break; //59 fps
//if (y > 100) break; //55 fps, 30%-40% CPU
//if (y > 200) break; //44 fps, 60% CPU
//if (y > 300) break; //29 fps, 70% CPU
//9 fps, 50% CPU (~110 msec / iter)
//30 fps * 72 w * 1024 h ~= 2.2M loop iterations/sec
//72 w * 1024 h ~= 74K loop iter/frame (33 msec)
//need ~ 0.45 usec / iter
                }
//        yield wait(started + (t + 1) / FPS - now_sec()); //avoid cumulative timing errors
        perf.idle -= perf.time; //this.elapsed; //exclude non-waiting time
//bkg wker: set reply status for main (atomic dec); main thread: sync wait and check all ipc reply rcvd
        yield this.wait(frnum / FPS - perf.time); //throttle to target frame rate; avoid cumulative timing errors
        perf.time = this.elapsed;
        perf.idle += perf.time; //this.elapsed;
*/

//bkg wker: noop - already waited for ipc req (rdlock?); main thread: gpu update
        perf.paint -= perf.time; //this.elapsed;
        yield this.paint();
        perf.time = this.elapsed;
        perf.paint += perf.time; //this.elapsed;
//        yield wait(1);
//        ++canvas.elapsed; //update progress bar

        if (OPTS.WANT_STATS && !(frnum % 50)) stats(perf, frnum);
    }
    if (!stats.reported || --frnum % 50) stats(perf, frnum); //stats for last partial interval; undo last loop inc

//    debug("heap diff:", JSON.stringify(hd.end(), null, 2));
/*
    render_time *= 1000 / frnum; //= 1000 * render_time / frnum; //(DURATION * FPS);
    idle_time *= 1000 / frnum; //= 1000 * idle_time / frnum;
    paint_time *= 1000 / frnum;
    var frtime = 1000 * canvas.elapsed / frnum;
    if (wait.stats) wait.stats.report(); //debug(`overdue frames: ${commas(wait.stats.true)}/${commas(wait.stats.false + wait.stats.true)} (${round(100 * wait.stats.true / (wait.stats.false + wait.stats.true), 10)}%), avg delay ${round(wait.stats.total, 10)} msec, min delay[${}] ${} msec, max delay[] ${} msec`[wait.stats.false? "red_lt": "green_lt"]);
    debug(`avg render time: ${round(render_time, 10)} msec (${round(1000 / render_time, 10)} fps), avg frame rate: ${round(frtime, 10)} msec (${round(1000 / frtime, 10)} fps), avg idle: ${round(idle_time, 10)} msec (${round(100 * idle_time / frtime, 10)}%), ${frnum} frames`.blue_lt);
    cpu = process.cpuUsage(cpu);
    debug(`cpu usage: ${JSON.stringify(cpu)} usec = ${round((cpu.user + cpu.system) / 1e4 / canvas.elapsed, 10)}% of elapsed`.blue_lt);
*/
    debug(`end: ran for ${round(canvas.elapsed, 10)} sec`.green_lt); //, now pause for ${PAUSE} sec`.green_lt);
//    yield this.wait(PAUSE); //pause to show screen stats longer
//    canvas.StatsAdjust = DURATION - canvas.elapsed; //-END_DELAY; //exclude pause from final stats
//    canvas.close();
};


//render all models for this thread:
//if run on main thread, should be non-blocking
canvas.render =
function* render(frnum, timestamp)
{
//    const {usleep} = require('gpu-friends-ws281x');

//    models.forEach(model => { model.render(frnum, timestamp); });
    var count = models.reduce((count, model) => { if (!OPTS.NUM_WKERS || (model.affinity == canvas.WKER_ID)) ++count; return count; }, 0);
//debug(optimizationStatus(this.render).blue_lt);
    debug(`TODO: ${this.prtype} '${process.pid}' render fr# ${frnum}, ${count} models, timestamp ${timestamp}`.yellow_lt);
//    if (OPTS.NUM_WKERS && (this.affinity != canvas.WKER_ID)) return; //not for this wker thread; bypass remaining init
//    usleep(5000); //simulate processing (5 msec)
    return this.wait(5e-3 * count); //msec; simulate 5 msec per model
}


//periodically display perf stats:
//stats are collected from 3 sources: above playback(), process.cpuUsage(), and canvas methods (paint + wait)
//TODO: http server
function stats(perf, numfr)
{
    var canvst;
//    if (!OPTS.WANT_STATS) return;
//    var perf = {render: 0, idle: 0, paint: 0, cpu: process.cpuUsage()}, timestamp;
//debug(JSON.stringify(wait.stats));
    debug("------ stats -----".blue_lt);
    if (canvst = canvas.render_stats) //cut down on verbosity; //console.log(canvas.wait_stats.report); //wait.stats.report(); //debug(`overdue frames: ${commas(wait.stats.true)}/${commas(wait.stats.false + wait.stats.true)} (${round(100 * wait.stats.true / (wait.stats.false + wait.stats.true), 10)}%), avg delay ${round(wait.stats.total, 10)} msec, min delay[${}] ${} msec, max delay[] ${} msec`[wait.stats.false? "red_lt": "green_lt"]);
        console.log(`#fr ${canvst.numfr}, #err ${canvst.numerr}, #dirty ${canvst.num_dirty} (${percent(canvst.num_dirty / canvst.numfr)}%), \
            elapsed ${round(canvst.elapsed, 10)} sec (${round(canvst.fps, 10)} fps), avg ${round(1000 / canvst.fps, 10)} msec = \
            caller ${round(canvst.caller_time, 10)} (${percent(canvst.caller_time / 1000 * canvst.fps)}%) + \
            encode ${round(canvst.encode_time, 10)} (${percent(canvst.encode_time / 1000 * canvst.fps)}%) + \
            update ${round(canvst.update_time, 10)} (${percent(canvst.update_time / 1000 * canvst.fps)}%) + \
            render ${round(canvst.render_time, 10)} (${percent(canvst.render_time / 1000 * canvst.fps)}%), \
            frame rate: ${canvst.frrate.map(fr => { return round(fr, 10); })} msec (avg ${round(canvst.avg_fps, 10)} fps)`.replace(/\s+/gm, " ").yellow_lt);
    if (canvst = canvas.wait_stats) //cut down on verbosity; //console.log(canvas.wait_stats.report); //wait.stats.report(); //debug(`overdue frames: ${commas(wait.stats.true)}/${commas(wait.stats.false + wait.stats.true)} (${round(100 * wait.stats.true / (wait.stats.false + wait.stats.true), 10)}%), avg delay ${round(wait.stats.total, 10)} msec, min delay[${}] ${} msec, max delay[] ${} msec`[wait.stats.false? "red_lt": "green_lt"]);
        console.log(`wait stats: overdue frames ${commas(canvst.true)}/${commas(canvst.count)} (${percent(canvst.true / canvst.count)}%), \
            avg wait ${round(canvst.total_delay / canvst.count, 10)} msec, \
            min wait[${canvst.min_when}] ${round(canvst.min_delay, 10)} msec, \
            max wait[${canvst.max_when}] ${round(canvst.max_delay, 10)} msec`.replace(/\s+/gm, " ")[canvst.true? "red_lt": "green_lt"]);
    console.log(`avg frame: ${round(perf.time * 1e3 / numfr, 10)} msec (${round(numfr / perf.time, 10)} fps) = \
        avg render: ${round(perf.render * 1e3 / numfr, 10)} msec @${round(numfr / perf.render, 10)} fps (${percent(perf.render / perf.time)}%) + \
        avg idle: ${round(perf.idle * 1e3 / numfr, 10)} msec (${percent(perf.idle / perf.time)}%) + \
        avg paint: ${round(perf.paint * 1e3 / numfr, 10)} msec (${percent(perf.paint / perf.time)}%), \
        ${numfr} frames`.replace(/\s+/gm, " ").blue_lt);
    var cpu = usec2sec(process.cpuUsage(perf.cpu));
    console.log(`cpu usage: ${cpu.user} user + ${cpu.system} system = ${cpu.user + cpu.system} sec (${percent((cpu.user + cpu.system) / perf.time)}% elapsed)`.blue_lt);
    debug("------ /stats -----".blue_lt);
    stats.reported = true;
}


//const x = {};
//if ((++x.y || 0) < 10) console.log("yes-1");
//else console.log("no-1");
//if (++x.z > 10) console.log("yes-2");
//else console.log("no-2");
//process.exit();


////////////////////////////////////////////////////////////////////////////////
////
/// Helpers:
//


//convert usec to sec:
function usec2sec(obj)
{
    Object.keys(obj).forEach(key => { obj[key] /= 1e6; });
    return obj;
}


//display commas for readability (debug):
//NOTE: need to use string, otherwise JSON.stringify will remove commas again
function commas(val)
{
//number.toLocaleString('en-US', {minimumFractionDigits: 2})
    return val.toLocaleString();
}


//show %:
function percent(val)
{
    return round(100 * val, 10); //+ "%";
}


//round to specified #decimal places:
function round(val, digits)
{
    return Math.floor(val * (digits || 1) + 0.5) / (digits || 1); //round to desired precision
}


//high-res elapsed time:
//NOTE: unknown epoch; useful for relative times only
//function elapsed()
//{
//    var parts = process.hrtime();
//    return parts[0] + parts[1] / 1e9; //sec, with nsec precision
//}


//truncate after 3 dec places:
//function milli(val)
//{
//    return Math.floor(val * 1e3) / 1e3;
//}


//truncate after 1 dec place:
//function tenths(val)
//{
//    return Math.floor(val * 10) / 10;
//}


//current time in seconds:
//function now_sec()
//{
//    return Date.now() / 1000;
//}

debug(`eof ${canvas.prtype} '${process.pid}'`.blue_lt);
//eof