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
//const tryRequire = require("try-require"); //https://github.com/rragan/try-require
//const multi = tryRequire('worker_threads'); //NOTE: requires Node >= 10.5.0; https://nodejs.org/api/worker_threads.html
//const {fork} = require("child_process"); //https://nodejs.org/api/child_process.html
const cluster = require("cluster"); //tryRequire("cluster") || {isMaster: true}; //https://nodejs.org/api/cluster.html; //http://learnboost.github.com/cluster
//const /*{ createSharedBuffer, detachSharedBuffer }*/ sharedbuf = tryRequire('shared-buffer'); //https://www.npmjs.com/package/shared-buffer
cluster.proctype = cluster.isMaster? "Main": cluster.isWorker? "Wker": "?UNKN-TYPE?";
//console.log("here1");
//debug("here2");
//console.log("here3");
const gp = require("../build/Release/gpuport"); //{NUM_UNIV: 24, UNIV_MAXLEN: 1136, FPS: 30};
const {/*limit, listen, nodebufq, frctl, perf, nodes,*/ NUM_UNIV, UNIV_MAXLEN, FPS} = gp; //make global for easier access
debug("gp", JSON.stringify(gp).json_tidy);

//render control info:
const ALL_READY = (1 << NUM_UNIV) - 1, SOME_READY = (1 << (NUM_UNIV / 2)) - 1, FIRST_READY = 1 << (NUM_UNIV - 1); //set half-ready to force sync first time
debug(`all ready ${hex(ALL_READY)}, some ready ${hex(SOME_READY)}, first ready ${hex(FIRST_READY)}`);
//console.log("here4");
//const /*GpuPort*/ {limit, listen, nodebufq} = GpuPort; //require('./build/Release/gpuport'); //.node');
const NUM_CPUs = require('os').cpus().length; //- 1; //+ 3; //leave 1 core for render and/or audio; //0; //1; //1; //2; //3; //bkg render wkers: 43 fps 1 wker, 50 fps 2 wkers on RPi
const NUM_WKERs = 3-3; //Math.max(NUM_CPUs - 1, 1); //leave 1 core available for bkg gpu xfr, node event loop, etc
debug("change this".red_lt, "change this".red);
//    NUM_WKERs: 0, //whole-house fg render
//  NUM_WKERs: 1, //whole-house bg render
//    NUM_WKERs: os.cpus().length, //1 bkg wker for each core (optimal)
//    NUM_WKERs: 6, //hard-coded #bkg wkers
const WKER_UNIV = Math.ceil(NUM_UNIV / NUM_WKERs); //#univ each wker should render
debug(`#wkers ${NUM_WKERs}, #univ/wker ${WKER_UNIV}`.cyan_lt);
//const QUELEN = 4;

//sequence/layout info:
//const seq = tryRequire("./my-seq");
debug("load seq here".red_lt);
const seq = {NUMFR: 18, FRTIME: 50}; //seq len (#frames total)
seq.duration = seq.NUMFR * seq.FRTIME;
seq.gpufr = divup(seq.duration, FPS);
debug("seq info", JSON.stringify(seq).json_tidy);


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

//set up shm node bufs + fr ctl:
//const {frctl, perf, nodes} = shmbufs();
//debug("frctl", frctl);
//debug(`frctl[3]: fr# ${frctl[3].frnum}, ready 0x${frctl[3].ready}`);


/////////////////////////////////////////////////////////////////////////////////
////
/// Main thread/process:
//

//step.debug = debug;
//if (multi.isMainThread)
function* main()
{
    const opts =
    {
//        screen: 0, //FIRST_SCREEN,
    //    key_t PREALLOC_shmkey = 0;
        vgroup: 20,
        debug: 33,
        init_color: PINK, //= 0;
        protocol: gp.Protocol.DEVMODE,
//        int frtime_msec; //double fps;
    };
    //int
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
    gp.open(opts); //{screen, init_color, wh} //numfr: seq.NUMFR});
    nodes.forEach((nodes) => nodes.forEach((univ) => univ.forEach((node) => node = PINK))); //BLACK))); //start with all LEDs off
//init shm frbuf que < wkers so they will have already work to do:
//    frctl.forEach((quent, inx) => { quent.frnum = inx; quent.ready = 0; }); //SOME_READY;
//    debug(`frbuf quent initialized`.blue_lt);
    debug(`launch ${plural(NUM_WKERs)} wker${plural.cached}`.blue_lt);
    for (let w = 0; w < NUM_WKERs; ++w) /*const wker =*/ start_wker(); //leave one core feel for sound + misc
//    for (let i = 1; i <= 10; ++i)
//        setTimeout(() =>
//        {
//            debug(`send wker req#${i}`.blue_lt)
//            wker.postMessage("message", `req-from-main#${shdata.main_which = i}`);
//        }, 1000 * i);
//    const perf = {wait: 0, render: 0}; //wait time, render time
    while (gp.isopen)
    {
        debug(`fr# ${gp.numfr}, frtime ${gp.frtime}, ready ${gp.ready}, perf stats:`, JSON.stringify(gp.perf_stats).json_tidy);
        yield wait_sec(1);
    }
    debug(`main idle (done): ${gp.perf_stats}`.cyan_lt);
}


function* junk1()
{
    perf[0].init();
//    let previous = elapsed.now(), started = previous;
    for (var frnum = 0; frnum < NUMFR; ++frnum)
    {
        let qent = frnum % nodes.length; //simple, circular queue
        if (frctl[qent].frnum != frnum) exc(`frbuf que addressing messed up: got fr#${frctl[qent].frnum}, wanted ${frnum}`); //main is only writer; this shouldn't happen!
        while (frctl[qent].ready != ALL_UNIV) //wait for all wkers to render nodes; wait means wkers are running too slow
        {
            debug(`fr[${frnum}/${NUMFR}] not ready: ${hex(frctl[qent].ready)}, main waiting for wkers to render ...`.yellow_lt);
            yield wait_msec(2); //timing is important; don't wait longer than needed
        }
//TODO: tweening for missing/!ready frames?
 //       let delta = elapsed.now() - previous; perf[0].wait += delta; previous += delta;
        perf[0].update();
        debug(`xfr fr[${frnum}/${SEQNUM}] to gpu ...`);
//TODO: pivot/update txtr, update screen (NON-BLOCKING)
//make frbuf ready available a new frame:
        frctl[qent].ready = 0;
        frctl[qent].frnum += QUELEN; //NOTE: do this last (wkers look for this)
//        delta = elapsed.now() - previous; perf[0].render += delta; previous += delta;
//        perf[0].numfr = frnum + 1;
        perf[0].update(true);
    }
//    perf[0].unknown = previous - started - perf[0].render - perf[0].wait; //should be 0 or close to
    debug(`main idle (done): ${perf[0].show("xfred", 0)}`.cyan_lt);
//    process.send({data}, (err) => {});
}


function start_wker() //filename) //, data, opts)
{
//    debug("env", process.env);
//    data = Object.assign({}, data, {ID: ++start_wker.count || (start_wker.count = 1)}); //shallow copy + extend
//    const OPTS = //null;
//    {
//        cwd: process.cwd(),
//    const env = Object.assign({}, process.env, {wker_data: JSON.stringify(data)}); //shallow copy + extend
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
    const wker = cluster.fork(/*env*/) // /*filename || __filename, Array.from(process.argv).slice(2), opts) //|| OPTS)*/ env)
        .on("online", () => debug(`wker# ${wker.id} pid ${(wker/*.threadId*/ .process || {}).pid || NO_PID} is on-line`.cyan_lt)) //, arguments))
        .on('message', (data) => debug("msg from wker# ${wker.id}".blue_lt, data)) //, /*arguments,*/ "my data".blue_lt, shdata)) //args: {}, require, module, filename, dirname
        .on('error', (data) => debug("err from wker# ${wker.id}".red_lt, data, arguments))
//TODO: add restart logic if needed
        .on('exit', (code) => debug("wker# ${wker.id} exit".yellow_lt, code)) //, arguments)) //args: {}, require, module, filename, dirname
//          if (code !== 0) reject(new Error(`Worker stopped with exit code ${code}`));
        .on('disconnect', () => debug(`wker# ${wker.id} disconnect`.cyan_lt)); //, arguments));
    debug(`launched wker#${wker.id} pid ${(wker.process || {}).pid || NO_PID}, now have ${Object.keys(cluster.workers).length} wkers: ${Object.keys(cluster.workers).join(", ")} ...`.cyan_lt);
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


function* wker() //wker_data)
{
//    debug("wker env", process.env);
//    wker_data = JSON.parse(process.env.wker_data); //TODO: move up a level
    const /*cluster.worker.inx*/ wkid = cluster.worker.id; //reduce vebosity
    const [univ_begin, univ_end] = [(wkid - 1) * WKER_UNIV, Math.min(wkid * WKER_UNIV, NUM_UNIV)];
//univ ready bits:
//begin  end   my
// 0     1    0x800000
// 0     4    0xF00000
//20     21   0x000008
//20     24   0x00000F
    const my_ready = (ALL_READY >> univ_begin) & ~(ALL_READY >> univ_end); //Ready bits for all my univ
    debug(`wker# ${wkid} started, responsible for rendering univ ${univ_begin}..${univ_end}, my ready bits ${hex(my_ready)}`.cyan_lt); //, "cloned data", /*process.env.*/wker_data); //multi.workerData);
//    for (let i = 1; i <= 3; ++i)
//        setTimeout(() => multi.parentPort.postMessage(`hello-from-wker#${multi.workerData.wker_which = i}`, {other: 123}), 6000 * i);
//    multi.parentPort.on("message", (data) => debug("msg from main".blue_lt, data, /*arguments,*/ "his data".blue_lt, multi.workerData)); //args: {}, require, module, filename, dirname
//    setTimeout(() => { debug("wker bye".red_lt); process.exit(123); }, 15000);
    perf[wkid].init(); //wait = perf[wkid].render = perf[wkid].unknown = perf[wkid].numfr = 0;
//    let previous = elapsed.now(), started = previous;
    for (let frnum = 0; frnum < NUMFR; ++frnum)
    {
        let qent = frnum % frctl.length; //get next framebuf queue entry; simple, circular queue addressing
        while (frctl[qent].frnum != frnum) //wait for nodebuf to become available; this means wker is running 3 frames ahead of GPU xfr
        {
            debug(`wker ${wkid} waiting to render fr[${frnum}/${NUMFR}] into nodebuf quent# ${qent} ...`.green_lt);
            yield wait_msec(1000 / FPS); //msec; timing not critical here; worked ahead ~3 frames and waiting for empty nodebuf
        }
//        let delta = elapsed.now() - previous; perf[wkid].wait += delta; previous += delta;
        perf[wkid].update();
        if (frctl[qent].ready & my_ready) exc(`fr[${frnum}/${NUMFR}] in quent ${qent} ready ${hex(frctl[qent].ready)} already has my ready bits: ${hex(my_ready)}`);
        debug(`wker ${wkid} render fr[${frnum}/${NUMFR}] ready ${hex(frctl[qent].ready)} into nodebuf quent# ${qent} ...`);
//TODO: render
//NOTE: wker must set new values for *all* nodes (prev frame contents are stale from 4 frames ago)
//to leave nodes unchanged, wker can set A = 0 (dev mode only) or copy values from prev frame
//        nodes[2][3][4] = PALETTE[fr + 1];
        for (let u = univ_begin; u < univ_end; ++u)
            nodes[qent][u].forEach((oldval, inx) => nodes[qent][u][inx] = PALETTE[frnum % PALETTE.length]);
        frctl[qent].ready |= my_ready; //TODO: check if this is atomic
        debug(`wker ${wkid} rendered fr[${frnum}/${NUMFR}] into nodebuf quent# ${qent}, ready now ${hex(frctl[qent].ready)} ...`);
//        delta = elapsed.now() - previous; perf[wkid].render += delta; previous += delta;
//        perf[wkid].numfr = frnum + 1;
        perf[wkid].update(true);
    }
//    perf[wkid].unknown = previous - started - perf[wkid].render - perf[wkid].wait; //should be 0 or close to
//    process.send({data}, (err) => {});
    debug(`wker idle (done), ${perf[wkid].show("rendered")}`.cyan_lt); //render ${prec(perf[wkid].render / (perf[wkid].numfr || 1), 1000)} (${prec(1000 * perf[wkid].numfr / perf[wkid].render, 10)} fps), wait ${prec(perf[wkid].wait / (perf[wkid].numfr || 1), 1000)} (msec, avg), total fps ${prec(1000 * perf[wkid].numfr / (perf[wkid].render + perf[wkid].wait), 10)}, unaccounted ${perf[wkid].unknown}`.cyan_lt);
}


///////////////////////////////////////////////////////////////////////////////
////
/// Misc utility/helper functions:
//

//set up 3D and 2D arrays of nodes in shared memory:
//addressing: nodes[quent within framebuf queue][univ][node within univ]
//const NUM_UNIV = 24;
//const UNIV_MAXLEN = 1136; //max #nodes per univ
//const UNIV_ROWSIZE = UNIV_MAXLEN * Uint32Array.BYTES_PER_ELEMENT; //sizeof(Uin32) * univ_len
//const shbuf = new SharedArrayBuffer(NUM_UNIV * UNIV_ROWSIZE); //https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer
//const nodes = Array.from({length: NUM_UNIV}, (undef, univ) => new Uint32Array(shbuf, univ * UNIV_ROWSIZE, UNIV_ROWSIZE / Uint32Array.BYTES_PER_ELEMENT)); //CAUTION: len = #elements, !#bytes; https://stackoverflow.com/questions/3746725/create-a-javascript-array-containing-1-n
//nodes.forEach((row, inx) => debug(typeof row, typeof inx, row.constructor.name)); //.prototype.inspect = function() { return "custom"; }; }});
//debug(`shm nodes ${NUM_UNIV} x ${UNIV_MAXLEN} x ${Uint32Array.BYTES_PER_ELEMENT} = ${commas(NUM_UNIV *UNIV_MAXLEN *Uint32Array.BYTES_PER_ELEMENT)} = ${commas(shbuf.byteLength)} created`.pink_lt);
function OBS_shmbufs()
{
    const shmkey = (0xfeed0000 | NNNN_hex(UNIV_MAXLEN)) >>> 0;
    const UNIV_BYTELEN = cache_pad(UNIV_MAXLEN) * Uint32Array.BYTES_PER_ELEMENT;
    debug(`make_nodes: shmkey ${typeof shmkey}:`, hex(shmkey), "univ bytelen", commas(UNIV_BYTELEN), `owner? ${cluster.isMaster}, univ len ${UNIV_MAXLEN} (${(cache_pad(UNIV_MAXLEN) == UNIV_MAXLEN)? "": "NOT ".red_lt}already padded)`);
    const shbuf = sharedbuf.createSharedBuffer_retry(shmkey, (QUELEN * NUM_UNIV + 1) * UNIV_BYTELEN, cluster.isMaster); //master creates new, slaves acquire existing; one extra univ for fr ctl
    let shused = 0; //shm seg len used (bytes)
//set up 3D indexing (quent x univ x node) for simpler node addressing and fx logic:
    const nodes = Array.from({length: QUELEN}, (undef, quent) =>
        Array.from({length: NUM_UNIV}, (undef, univ) =>
            new Uint32Array(shbuf, (quent * NUM_UNIV + univ) * UNIV_BYTELEN, UNIV_MAXLEN))); //CAUTION: len = #elements, !#bytes; https://stackoverflow.com/questions/3746725/create-a-javascript-array-containing-1-n
    shused += QUELEN * NUM_UNIV * UNIV_BYTELEN;
//2D indexing (quent x all nodes) if wkers just want access to all nodes:
    const frnodes = Array.from({length: QUELEN}, (undef, quent) =>
        new Uint32Array(shbuf, quent * NUM_UNIV * UNIV_BYTELEN, NUM_UNIV * UNIV_MAXLEN)); //CAUTION: len = #elements, !#bytes; https://stackoverflow.com/questions/3746725/create-a-javascript-array-containing-1-n
//1 extra (partial) univ holds fr ctl info (rather than using a separate shm seg):
    const FRNUM = 0, PREVFR = 1, FRTIME = 2, PREVTIME = 3, READY = 4, FRCTL_SIZE = cache_pad(5); //frctl ofs for each nodebuf quent
    const FRCTL = new Uint32Array(shbuf, /*QUELEN * NUM_UNIV * UNIV_BYTELEN*/ shused, QUELEN * FRCTL_SIZE);
    shused += QUELEN * FRCTL_SIZE * Uint32Array.BYTES_PER_ELEMENT;
    const frctl = Array.from({length: QUELEN}, (undef, quent) => //quent);
    {
//TODO: is getter/setter on typed array element faster/slower than data view?
        const retval =
        {
            get frnum() { return FRCTL[quent * FRCTL_SIZE + FRNUM]; },
            set frnum(newval) { FRCTL[quent * FRCTL_SIZE + FRNUM] = newval; },
            get prevfr() { return FRCTL[quent * FRCTL_SIZE + PREVFR]; },
            set prevfr(newval) { FRCTL[quent * FRCTL_SIZE + PREVFR] = newval; },
            get frtime() { return FRCTL[quent * FRCTL_SIZE + FRTIME]; },
            set frtime(newval) { FRCTL[quent * FRCTL_SIZE + FRTIME] = newval; },
            get prevtime() { return FRCTL[quent * FRCTL_SIZE + PREVTIME]; },
            set prevtim(newval) { FRCTL[quent * FRCTL_SIZE + PREVTIME] = newval; },
            get ready() { return FRCTL[quent * FRCTL_SIZE + READY]; },
            set ready(newval) { FRCTL[quent * FRCTL_SIZE + READY] = newval; },
        };
        return retval;
    });
//extra (partial) univ also holds perf stats:
    const RENDER = 0, WAIT = 1, UNKN = 2, NUMFR = 3, PERF_SIZE = cache_pad(4); //perf stats for each wker proc/thread
    const PERF = new Uint32Array(shbuf, shmused, (NUM_WKERs + 1) * PERF_SIZE);
    shused += (NUM_WKERs + 1) * PERF_SIZE * Uint32Array.BYTES_PER_ELEMENT; //main == wker[0]
    const perf = Array.from({length: NUM_WKERs + 1}, (undef, wker) => //quent);
    {
//TODO: is getter/setter on typed array element faster/slower than data view?
        const retval =
        {
            get render() { return PERF[wker * PERF_SIZE + RENDER]; },
            set render(newval) { PERF[wker * PERF_SIZE + RENDER] = newval; },
            get wait() { return PERF[wker * PERF_SIZE + WAIT]; },
            set wait(newval) { PERF[wker * PERF_SIZE + WAIT] = newval; },
            get unknown() { return PERF[wker * PERF_SIZE + UNKN]; },
            set unknown(newval) { PERF[wker * PERF_SIZE + UNKN] = newval; },
            get numfr() { return PERF[wker * PERF_SIZE + NUMFR]; },
            set numfr(newval) { PERF[wker * PERF_SIZE + NUMFR] = newval; },
//add convenience functions:
            init: function()
            {
                this.wait = this.render = this.unknown = this.numfr = 0;
                this.started = this.previous = elapsed.now();
            },
            update: function(render)
            {
                let delta = elapsed.now() - this.previous;
                if (render) { this.render += delta; ++this.numfr; }
                else this.wait += delta;
                this.previous += delta; //==now(), avoid system call overhead
            },
            show: function(verb)
            {
                this.unknown = this.previous - this.started - this.render - this.wait; //should be 0 or close to
                return `
                    ${plural(this.numfr)} frame${plural.cached} ${verb}, //#frames processes
                    perf: render ${prec(this.render / (this.numfr || 1), 1000)} //avg render time (msec)
                    (${prec(1000 * this.numfr / this.render, 10)} fps), //max achievable fps
                    wait ${prec(this.wait / (this.numfr || 1), 1000)} (msec, avg), //avg wait time (msec)
                    total fps ${prec(1000 * this.numfr / (this.render + this.wait), 10)}, //actual fps
                    unaccounted ${this.unknown} //msec
                    `.unindent.nocomments.replace(/\n/g, "");
            },
//            started,
//            previous, //when stats were last updated
        };
        return retval;
    });
    if (shused > UNIV_BYTELEN) exc(`frctl/stats univ space exceeded: used ${shused}, available ${UNIV_BYTELEN}`);
    debug(`shm fr ctl ${commas(frctl.length)}, stats ${commas(perf.length)} (misc space used: ${shused}, remaining: ${UNIV_BYTELEN - shused}), nodebuf queue ${commas(nodes.length)} x ${commas(nodes[0].length)} x ${commas(nodes[0][0].length)}, framebuf queue ${commas(frnodes.length)} x ${commas(frnodes[0].length)} ${cluster.isMaster? "created": "acquired"}`.pink_lt);
//no; let main/wker caller decide    if (cluster.isMaster)
//    {
//        nodes.forEach((quent) => quent.forEach((univ) => univ.forEach((node) => node = BLACK)));
//        frctl.forEach((quent, inx) => { quent.frnum = inx; quent.ready = 0; }); //SOME_READY;
//    }
//    if (cluster.isMaster) cluster.fork();
    return {frctl, perf, frnodes, nodes};
//??    detachSharedBuffer(sharedBuffer); 
//    process.exit(0);

//pad data to avoid spanning memory cache rows:
//RPi memory is slow, so cache perf is important
    function cache_pad(len_uint32)
    {
        const CACHELEN = 64; //RPi 2/3 reportedly have 32/64 byte cache rows; use larger size to accomodate both
        return rndup(len_uint32, CACHELEN / Uint32Array.BYTES_PER_ELEMENT);
    }
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
function wait_msec(msec)
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
function wait_sec(sec) { return wait_msec(sec * 1000); }
function wait_usec(usec) { return wait_msec(usec / 1000); }
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


//unindent a possibly multi-line string:
function unindent(str)
{
//    const FIRST_INDENT_xre = XRegExp(`
//        ^  #start of string or line ("m" flag)
//        (?<indented>  [^\\S\\n]+ )  #white space but not newline; see https://stackoverflow.com/questions/3469080/match-whitespace-but-not-newlines
//    `, "xgm");
    const FIRST_INDENT_re = /^([^\\S\\n]+)/gm;
    var parts = (str || "").match(FIRST_INDENT_re);
//console.error(`str: '${str.replace(/\n/g, "\\n")}'`);
//console.error(`INDENT: ${parts? parts.indented.length: "NO INDENT"}`);
    return (parts && parts.indented)? str.replace(new RegExp(`^${parts.indented}`, "gm"), ""): str;
}


//remove comments:
//handles // or /**/
function nocomments(str)
{
//TODO: handle quoted strings
    return str.replace(/(\/\/.*|\/\*.*\*\/)$/, "");
}


//strip colors from string:
function nocolors(str)
{
//    const ANYCOLOR_xre = XRegExp(`
//        \\x1B  #ASCII Escape char
//        \\[
//        (
//            (?<code>  \\d ; \\d+ )  #begin color
//          | 0  #or end color
//        )
//        m  #terminator
//        `, "xg");
    const ANYCOLOR_re = /\x1b\[((\d;\d+)|0)m/g;
    return (str || "").replace(ANYCOLOR_re, "");
}


//reset color whenever it goes back to default:
function color_reset(str, color)
{
//return str || "";
/*
    const COLORS_xre = XRegExp(`
        \\x1B  #ASCII Escape char
        \\[  (?<code> (\\d | ;)+ )  m
        `, "xg"); //ANSI color codes (all occurrences)
    const ANYCOLOR_xre = XRegExp(`
        \\x1B  #ASCII Escape char
        \\[  (?<code> \\d;\\d+ )  m
    `, "x"); //find first color only; not anchored so it doesn't need to be right at very start of string
    const NOCOLOR_xre = XRegExp(`
        \\x1B  #ASCII Escape char
        \\[  0  m
        (?!  $ )  #negative look-ahead: don't match at end of string
    `, "xg"); //`tput sgr0` #from http://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux
//    const [init_color, code] = (str || "").match(/^x1B\[(\d;\d+)m/); //extra color code from start of string
//    const [init_color, code] = (str || "").match(ANYCOLOR_re) || ["", "NONE"]; //extract first color code from start of string
//console.error(`str ${str || ""}, code ${code}`);
//    return (str || "").replace(ENDCOLOR_re, color || init_color || "\x1B[0m");
    color = color || ((str || "").match(ANYCOLOR_xre) || [])[0]; //extract first color code from start of string
    return color? (str || "").replace(NOCOLOR_xre, color): str; //set color back to first color instead of no color
*/
    const FIRSTCOLOR_xre = XRegExp(`
        ^  #at start of string
        (?<escseq>
            \\x1B  #ASCII Escape char
            \\[  (?<code>  \\d;\\d+ )  m
        )
        `, "x");
    const UNCOLORED_xre = XRegExp(`
        ( ^ | (?<color_end>  \\x1B \\[ 0 m ))  #start or after previous color
        (?<substr>  .*? )  #string region with no color (non-greedy)
        ( $ | (?<color_start>  \\x1B \\[ \\d+ ; \\d+ m ))  #end or before next color
        `, "xgm"); //match start/end of line as well as string; //`tput sgr0` #from http://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux
//    var first, last;
/*
    var uncolored = []; //(ofs, len) pairs where string has no color
    XRegExp.forEach(str || "", UNCOLORED_xre, (match, inx) => 
    {
//        if (match.code == "0") //no color
//        else
        console.error(`match[${inx}]: substr ${match.substr.length}:'${match.substr}', ofs ${match.index}, data ${JSON5.stringify(match)}`);
        if (match.substr.length) uncolored.push({ofs: match.index, len: match.substr.length, });
    });
    console.error(`areas not colored: ${JSON5.stringify(uncolored)}`);
*/
//    var matches = (str || "").match(COLORS_xre);
//    console.error(JSON5.stringify(matches, null, "  "));
    color = ((color || str || "").match(FIRSTCOLOR_xre) || {}).escseq; //extract first color from start if caller didn't specify
//    console.error(`\ncolor to apply: ${JSON.stringify(color)}`);
    return color? (str || "").replace(UNCOLORED_xre, (match, inx) =>
    {
//        console.error(`match[${inx}]: end ${JSON.stringify(match.color_end)}, substr ${match.substr.length}:'${match.substr}', ofs ${match.index}, start ${JSON.stringify(match.color_start)}, data ${JSON5.stringify(match)}`);
        return `${color}${match.substr}${match.color_start || "\x1B[0m"}`; //replace all color ends with new color; reset color at end of line
    }): str; //set color back to first color instead of no color
}


//function debug(args) { console.log.apply(null, Array.from(arguments).push_fluent(__parent_srcline)); debugger; }

function hex(thing) { return `0x${(thing >>> 0).toString(16)}`; }


//adjust precision on a float val:
function prec(val, scale) { return Math.round(val * scale) / scale; }

//makes numbers easier to read:
function commas(num) { return num.toLocaleString(); } //grouping (1000s) default = true


function divup(num, den) { return ((num + den - 1) / den) * den; }

function rndup(num, den) { return num + den - ((num % (den || 1)) || den); }

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
//        quoted: { get() { return quote(this/*.toString()*/); }, },
//        quoted1: { get() { return quote(this/*.toString()*/, "'"); }, },
//        unquoted: { get() { return unquote(this/*.toString()*/); }, },
//        unparen: { get() { return unparen(this/*.toString()*/); }, },
//        unquoescaped: { get() { return unquoescape(this/*.toString()*/); }, },
        unindent: { get() { return unindent(this); }, },
        nocomments: { get() { return nocomments(this); }, },
//        spquote: { get() { return spquote(this); }, },
//        anchorRE: { get() { return anchorRE(this/*.toString()*/); }, },
//        spaceRE: { get() { return spaceRE(this/*.toString()*/); }, },
//        color_reset: { get() { return color_reset(this/*.toString()*/); }, },
//        nocolors: { get() { return nocolors(this/*.toString()*/); }, },
//        echo_stderr: { get() { console.error("echo_stderr:", this.toString()); return this; }, },
//        escnl: { get() { return escnl(this); }, },
    });
//    Uint32Array.prototype.inspect = function() //not a good idea; custom inspect deprecated in Node 11.x

//detect if shbuf is too small:
//more data tends to be added during dev, so size mismatch is likely
//if request is too small, error return is cryptic so test for it automatically here
/*
    sharedbuf.createSharedBuffer_retry = function(key, bytelen, create)
    {
        try { return sharedbuf.createSharedBuffer(key, bytelen, create); } //size is okay
        catch (exc)
        {
//        if (exc == "invalid argument") //size is too small (probably)
            var retval = (bytelen > 1)? sharedbuf.createSharedBuffer(key, 1, create): null; //retry with smaller size
            if (retval)
            {
                debug(`shm buf ${hex(key)} is probably smaller than requested size ${commas(bytelen)}`.yellow_lt);
                sharedbuf.detachSharedBuffer(retval);
            }
            throw exc; //rethrow original error so caller can deal with it
        }
    }
*/
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


const test_SEQLEN = 120 * FPS; //total #fr in seq

function fill_test()
{
    debug("fill test");
    const REPEATS = 100;

    elapsed(0);
    for (var loop = 0; loop < REPEATS; ++loop)
    {
        for (var numfr = 0; numfr < test_SEQLEN; ++numfr)
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
    outfs.write(`#NUM_UNIV: ${NUM_UNIV}, UNIV_MAXLEN: ${UNIV_MAXLEN}, #fr: ${test_SEQLEN}, repeats: ${REPEATS}\n`);

    elapsed(0);
    for (var loop = 0; loop < REPEATS; ++loop)
    {
        for (var numfr = 0; numfr < test_SEQLEN; ++numfr)
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