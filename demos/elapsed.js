#!/usr/bin/env node
//elapsed time (msec); multiple procs can use same time base


'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
//require("magic-globals"); //__file, __line, __stack, __func, etc
//const hires_now = require("performance-now");
const tryRequire = require("try-require"); //https://github.com/rragan/try-require
const cluster = tryRequire("cluster") || {isMaster: true}; //https://nodejs.org/api/cluster.html; //http://learnboost.github.com/cluster
const sharedbuf = tryRequire('shared-buffer'); //|| {}; //https://www.npmjs.com/package/shared-buffer
//const {debug} = require('./debug') || {debug: console.error}; //CAUTION: recursive require(), so defer until elapsed() is called
//if (debug/*.enabled*/) debugger;
//const {debug} = require('./debug');
//console.log("debug", debug);

//detect if shbuf is too small:
//more data tends to be added during dev, so size mismatch is likely
//if request is too small, error return is cryptic so test for it automatically here
sharedbuf.createSharedBuffer_retry = function(key, bytelen, create)
{
    try { return sharedbuf.createSharedBuffer(key, bytelen, create); } //size is okay
    catch (exc)
    {
//        if (exc == "invalid argument") //size is too small (probably)
        var retval = (bytelen > 1)? sharedbuf.createSharedBuffer(key, 1, create): null; //retry with smaller size
        if (retval)
        {
            debug("shm buf 0x${hex(key)} is probably smaller than requested".yellow_lt);
            sharedbuf.detachSharedBuffer(retval);
        }
        throw exc; //rethrow original error so caller can deal with it
    }
}
    

//function msec2sec(msec) { return msec / 1000; }
//function sec2msec(sec) { return sec * 1000; }

//#msec elapsed already:
//caller can back-date using < 0, or skip ahead using > 0
//NOTE: elapsed times are all relative, so local vs. UTC is irrelevant
const elapsed = 
module.exports.elapsed =
function elapsed(reset)
{
    if (arguments.length || !elapsed.epoch) //!isNaN(elapsed.epoch)) //epoch will always be num (Uint32Array[]), but shouldn't ever be 0
    {
//        debugger;
//        const {debug} = require('./debug') || {debug: console.error}; //CAUTION: recursive require(), so defer until elapsed() is called
        elapsed.epoch = elapsed.now() - (reset || 0); //set new epoch
        /*if (debug)*/ ++debug.nested;
        debug(`reset epoch to 0x${hex(elapsed.epoch)}, now 0x${hex(elapsed.now())}, elapsed = ${reset || 0}`.cyan_lt);
    }
//n/a Uint32Array[]    if (isNaN(elapsed.epoch)) throw "set epoch before calling elapsed()".red_lt;
//CAUTION: calls to debug() are recursive:
//    debug(`elapsed(): now 0x${hex(elapsed.now())} - epoch 0x${hex(elapsed.epoch)} = diff ${elapsed.now() - elapsed.epoch}`);
    return elapsed.now() - elapsed.epoch;
}

//give caller access to my (shared) timebase:
//CAUTION: must occur < first call to debug() or elapsed()
Object.defineProperty(elapsed, "epoch",
{
    get() { return timebuf[0]; },
    set(new_epoch) { timebuf[0] = new_epoch; },
});
 

//get current system time:
//used for relative (elapsed) time, so units !matter
//CAUTION: must occur < first call to debug() or elapsed()
const WANT_HIRES = false;
elapsed.now = function()
{
    timebuf[1] = WANT_HIRES? process.hrtime.bigint(): Date.now(); //high vs. low res
    return timebuf[1]; //always ret thru timebuf so values wrap consistently and give the correct diff
}


elapsed.pause = function()
{
    elapsed.paused || (elapsed.paused = elapsed.now());
}


elapsed.resume = function()
{
    if (elapsed.paused) elapsed.epoch += elapsed.now() - elapsed.paused;
    elapsed.paused = null;
}


/////////////////////////////////////////////////////////////////////////////////
////
/// safe to call debug() or elapsed() after this point
//

//CAUTION: debug <-> elapsed circular dependency
//place debug() import *after* elapsed() export to avoid undef imports
const {debug} = require('./debug') || {debug: console.error}; //CAUTION: recursive require()
//if (debug/*.enabled*/) debugger;


//define shared memory for epoch so all procs are in sync:
//TODO: combine this with other shm struct members?
const shmkey = 0xbeef0001;
const shbuf = sharedbuf?
    sharedbuf.createSharedBuffer_retry(shmkey, 2 * Uint32Array.BYTES_PER_ELEMENT, cluster.isMaster): //master creates new, slaves acquire existing
    new ArrayBuffer(2 * Uint32Array.BYTES_PER_ELEMENT);
//use typed array rather than data view; TODO: verify it's faster
const timebuf = new Uint32Array(shbuf, 0, 2); //CAUTION: len = #elements, !#bytes; https://stackoverflow.com/questions/3746725/create-a-javascript-array-containing-1-n
debug(`${sharedbuf? "shared": "private"} elapsed epoch created: shmkey ${typeof shmkey}:`.pink_lt, hex(shmkey));
//if (cluster.isMaster) epoch[0] = 0;


if (cluster.isMaster) elapsed(0); //set initial epoch


////////////////////////////////////////////////////////////////////////////////
////
/// Unit test:
//

//use sync coding style for simplicity:
//TODO: add cluster tests
function* unit_test()
{
    console.log("start test", elapsed());

    yield wait(123);
    console.log("T+123", elapsed());

    yield wait(1000);
    console.log("T+1000", elapsed());
    elapsed(0);

    yield wait(2345);
    console.log("T+3345-1000", elapsed());

    yield wait(100);
    console.log("T+100", elapsed());
    elapsed.pause();

    yield wait(200);
    elapsed.resume();

    yield wait(300);
    console.log("T+500-200", elapsed());

    elapsed.epoch += 100; //go back in time
    console.log("T-100", elapsed());
}


//step thru generator function:
//async cb called at end
//if !cb, will block until generator function exits
function step(gen, async_cb)
{
    if (gen && !gen.next) gen = gen(); //func -> generator
    step.gen || (step.gen = gen);
    step.async_cb || (step.async_cb = async_cb);
    try
    {
        const {done, value} = step.gen.next(step.value);
//        if (step.debug) step.debug(`done? ${done}, retval ${typeof(value)}: ${value}, async? ${!!step.async_cb}`); //, overdue ${arguments.back}`);
        if (done)
        {
            step.gen = null; //prevent further execution; step() will cause "undef" error
            if (step.async_cb) step.async_cb(value);
        }
        return step.value = value;
    }
    catch (exc) { console.error(`EXC: ${exc}`.red_lt); }
}


//delay for step/generator function:
function wait(msec)
{
//    debug("wait: set timeout ", msec);
    setTimeout(step, msec);
    return msec; //dummy value for easier debug
}


function hex(thing) { return `0x${(thing >>> 0).toString(16)}`; }


const ASYNC = true; //test style: sync vs. async
if (!module.parent)
{
    require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127

    if (ASYNC) step(unit_test, done);
    else done(step(unit_test));

    function done(retval) { console.log(`done: retval ${retval}`.cyan_lt); }
}

//eof