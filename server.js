#!/usr/bin/env node

"use strict";


//example infinite JSON stream from https://medium.com/@Jekrb/process-streaming-json-with-node-js-d6530cde72e9
//const http = require('http');
//http.createServer(infinite).listen(9090);
//function infinite (req, resp)
//{
//    resp.setHeader('Content-Type', 'application/json'); //make visible in browser
//    var seq = 0;
//    setInterval(() => resp.write(JSON.stringify({value: seq++}) + '\n'), 1000);
//}

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
step(main, done); //async
function done(retval) { console.log("retval: " + retval); }
console.log("after");

//step thru generator function:
//cb called async at end
//if !cb, blocks until generator function exits
function step(gen, async_cb)
{
    if (gen && !gen.next) gen = gen(); //generator function
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

function wait(msec)
{
//    if (!step.async_cb)
//    return setTimeout.bind(null, step, msec); //defer to step loop
    setTimeout(step, msec);
//    return msec; //dummy retval for testing
    return +4;
}

//eof
