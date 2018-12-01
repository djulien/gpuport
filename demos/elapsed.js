#!/usr/bin/env node
//elapsed time (msec)

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
//require("magic-globals"); //__file, __line, __stack, __func, etc
//debugger;
//const {debug} = require('./debug');
//console.log("debug", debug);


//function msec2sec(msec) { return msec / 1000; }
//function sec2msec(sec) { return sec * 1000; }

//#msec elapsed already:
//caller can back-date using < 0, or skip ahead using > 0
//NOTE: elapsed times are all relative, so local vs. UTC is irrelevant
const elapsed = 
module.exports.elapsed = //give caller access to my timebase
function elapsed(reset)
{
    if (arguments.length || !elapsed.epoch)
    {
//        debugger;
        const {debug} = require('./debug') || {debug: console.error}; //CAUTION: recursive require(), so defer until elapsed() is called
        if (debug) debug(`reset epoch to ${reset}, elapsed now = ${reset || 0}`.cyan_lt);
        elapsed.epoch = elapsed.now() - (reset || 0); //set new epoch
    }
    return elapsed.now() - elapsed.epoch;
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


elapsed.now = false? require("performance-now"): Date.now; //high vs. low res


elapsed(0); //set initial epoch


////////////////////////////////////////////////////////////////////////////////
////
/// Unit test:
//

//use sync coding style for simplicity:
function* unit_test()
{
    console.log(elapsed());

    yield wait(123);
    console.log(elapsed());

    yield wait(1000);
    console.log(elapsed());
    elapsed(0);

    yield wait(2345);
    console.log(elapsed());

    yield wait(100);
    console.log(elapsed());
    elapsed.pause();

    yield wait(200);
    elapsed.resume();

    yield wait(300);
    console.log(elapsed());
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


const ASYNC = true; //test style: sync vs. async
if (!module.parent)
{
    require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127

    if (ASYNC) step(unit_test, done);
    else done(step(unit_test));

    function done(retval) { console.log(`done: ${retval}`.cyan_lt); }
}

//eof