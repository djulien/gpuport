#!/usr/bin/env node
//GpuPort log viewer

//to debug:
//node  debug  this-file  args
//cmds: run, cont, step, out, next, bt, .exit, repl

"use strict";
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
//const util = require("util"); //format()
//console.error("hello".green_lt);
//process.exit();
//const pathlib = require("path"); //NOTE: called it something else to reserve "path" for other var names
//TODO? const log4js = require('log4js'); //https://github.com/log4js-node/log4js-node
//const /*{ createSharedBuffer, detachSharedBuffer }*/ sharedbuf = require('shared-buffer'); //https://www.npmjs.com/package/shared-buffer
/*
const AutoCreate_ProxyHandler =
{
    get: function(target, propname, receiver)
    {
//console.log("get " + propname);
//        return Reflect.get(...arguments);
//allow prop to be in object prototype or instance:
//        return target.hasOwnProperty(propname)? target[propname]
        if (propname.slice(-1) == "$") //auto-create object/array
        {
console.log(JSON.stringify(this));
            propname = propname.slice(0, -1);
            return (propname in target)? target[propname]: target[propname] = new Proxy([], AutoCreate_ProxyHandler); //kludge: create Array, but if caller doesn't use Array methods it's just a regular object
        }
        return target[propname];
    },
//    set: function(target, propname, value)
//    {
//console.log("set " + propname);
//        return target[propname] = value;
//    },
}; //require('../build/Release/gpuport'); //.node');
const /-*GpuPort*-/ gp = new Proxy({}, AutoCreate_ProxyHandler);
*/
const /*GpuPort*/ gp = {};
//const {limit, NUM_UNIV, /*UNIV_LEN,*/ nodebufs} = gp; //make global for easier access

/*
const WantLevel = 100;
gp.on = function(msg_type, cb)
{
//    (gp.evth = gp.evth || {})
//    gp.evth[msg_type] || (gp.evth[msg_type] = []);
//    gp.evth[msg_type]
//    gp.evth$[msg_type + "$"].push(cb);
    if (!gp.evth) gp.evth = {};
    if (!gp.evth[msg_type]) gp.evth[msg_type] = [];
    gp.evth[msg_type].push(cb);
}
gp.once = function(msg_type, cb)
{
    /-*return*-/ gp.on(msg_type, () => { cb.apply(null, arguments); gp.evth[msg_type].splice(arguments[3], 1); });
}
gp.emit = function(msg_type, data)
{
    ((gp.evth || {})[msg_type] || []).forEach((cb, inx) => cb(data)); //data.msg, data.hdr, data.level, inx));
}
*/

/*
setInterval(() =>
{
    let msgs = gp.read_debug();
    console.error(msgs);
}, 100);
*/

gp.on("msg", (msg) => //(msg, hdr, level) =>
{
    const level = -99;
    const hdr = {elapsed: "?", thrinx: "?", srcline: "?", };
    if (level <= WantLevel) console.error(`[${hdr.elapsed} $${hdr.thrinx}] ${msg} ${hdr.srcline}.${level}`);
});
gp.once("msg", (msg, hdr, level) => console.error("got once"));



/////////////////////////////////////////////////////////////////////////////////
////
/// test:
//

function nop() {}

function* main()
{
    yield wait_sec(0.4);
    gp.emit("msg", {msg: "msg#1", hdr: {elapsed: 0.4, thrinx: 0, srcline: "@here:1", }, level: 10});
    yield wait_sec(1);
    gp.emit("msg", {msg: "msg#2", hdr: {elapsed: 1.4, thrinx: 0, srcline: "@here:3", }, level: 99});
    yield wait_sec(2);
    gp.emit("msg", {msg: "msg#3", hdr: {elapsed: 3.4, thrinx: 0, srcline: "@here:5", }, level: 0});
}
step(main);


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
        (step.debug || nop)(() => `done? ${done}, retval ${typeof(retval)}: ${retval}, async? ${!!step.async_cb}`); //, overdue ${arguments.back}`);
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
    (step.debug || nop)("wait: set timeout ", msec);
//NOTE: Node timer api is richer than browser; see https://nodejs.org/api/timers.html
    /*wait.latest =*/ setTimeout(step, msec); //.unref(); //function(){ wait.cancel("fired"); step(); }, msec); //CAUTION: assumes only 1 timer active
//    wait.latest.unref();
//    return msec; //dummy retval for testing
    return msec; //dummy value for debug
}
function wait_sec(sec) { return wait_msec(sec * 1000); }


//eof