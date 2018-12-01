#!/usr/bin/env node
//mp3 playback

//to debug:
//node  debug  this-file  args
//cmds: run, cont, step, out, next, .exit, repl

'use strict';
const DEV = true; //false;
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
//require('colors'); //var colors = require('colors/safe'); //https://www.npmjs.com/package/colors; http://stackoverflow.com/questions/9781218/how-to-change-node-jss-console-font-color
const os = require('os');
const fs = require('fs');
const glob = require('glob');
const pathlib = require('path');
const lame = require('lame'); //NOT node-lame
const spkr = require("speaker"); //DEV? "../": "speaker"); //('yalp/my-speaker');

//debugger;
const {debug} = require('./debug');
const mp3len = require('./mp3len');
//const relpath = require('./yalp/relpath');
const {elapsed} = require('./elapsed');
//const {timescale} = require('./yalp/timefmt');

//function test()
//{
//    const obj = {};
//obj.a = 1;
//function x(n)
//{
//    console.log(this, n);
//}
//x.call(obj, 1);
//process.exit(0);
//}
//test();

extensions(); //hoist for in-line code below

//version info:
debug(`node.js version: ${JSON.stringify(process.versions, null, 2)}`.cyan_lt);
debug(`spkr api ${spkr.api_version}, name '${spkr.name}', rev ${spkr.revision}, \ndesc '${spkr.description}'`.cyan_lt);
/*if (!process.env.DEBUG)*/ console.error(`use "DEBUG=${__file}" prefix for debug`.yellow_lt);

//need status info:
//https://www.mpg123.de/api/group__mpg123__seek.shtml

//example mp3 player from https://gist.github.com/TooTallNate/3947591
//more info: https://jwarren.co.uk/blog/audio-on-the-raspberry-pi-with-node-js/
//this is impressively awesome - 6 lines of portable code!
//https://github.com/TooTallNate/node-lame
const mp3player =
module.exports =
function mp3player(filename, cb)
{
//    cb || (cb = debug);
//console.log("mp3player: type", typeof filename, filename);
//    filename = filename.replace(/[()]/, "."); //kludge: () treated as special chars
    const duration = mp3len(filename);
//    var decode = new Elapsed();
    const want_play = true;
//    elapsed(0); //reset stopwatch

//function State() {}
//debugger;
//debug(cb.toString());
    const state = {};
    cb = cb? cb.bind(state): debug;
//state.hello = 1;
//debug(state);
//cb.call(null, null, 456, "abc");
//cb.call(state, null, 456, "abc");
    /*debug*/ console.log(`playing '${relpath(filename)}', duration ${duration} sec ...`.yellow_lt);
//    var decoder, pbstart, pbtimer;
    fs.createReadStream(filename) //process.argv[2]) //specify mp3 file on command line
        .pipe(grab(new lame.Decoder(), "lame"))
        .on('format', (fmt) =>
        {
            console.log(`mp3 '${relpath(filename)}', format ${JSON.stringify(fmt).json_tidy}`.blue_lt); //.sampleRate, format.bitDepth);
            if (want_play) /*this*/ grab.lame.pipe(new spkr(fmt))
                .on('progress', (data) => { Object.assign(state, data); cb(`audio progress @T+${timestamp()}: ${JSON.stringify(data).json_tidy}`, null, data); }) //{numwr: THIS.numwr, wrlen: r, wrtotal: THIS.wrtotal, buflen: (left || []).length}); //give caller some progress info
                .on('open', () => cb(`audio open @T+${timestamp()}`)) //pbstart = elapsed(); cb("audio open"); }) // /*pbtimer = setInterval(position, 1000)*/; console.log("[%s] speaker opened".yellow_light, timescale(pbstart - started)); })
                .on('flush', () => cb(`audio flush @T+${timestamp()}`)) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker end-flushed".yellow_light, timescale(elapsed() - started)); })
                .on('close', () => cb(`audio close @T+${timestamp()}`)); // /*clearInterval(pbtimer)*/; console.log("[%s] speaker closed".yellow_light, timescale(elapsed() - started)); });
//            elapsed(0);
            cb(`audio start @T+${elapsed()}`);
            elapsed(0);
        })
        .on('end', () => cb(`audio decode done @T+${timestamp()}`)) //console.log("[%s] decode done!".yellow_light, timescale(elapsed() - started)); })
        .on('error', (err) => cb(`audio ERROR: ${err} @T+${timestamp()}`, err)); //console.log("[%s] decode ERROR %s".red_light, timescale(elapsed() - started), err); });
//    return started - 0.25; //kludge: compensate for latency
}


////////////////////////////////////////////////////////////////////////////////
////
/// utils/helpers:
//

//return path relative to app dir (cwd); cuts down on verbosity
function relpath(abspath, basedir)
{
    var cwd = process.cwd(); //__dirname; //save it in case it changes later
    
//broken?    return path.relative(basedir || cwd, abspath);
//debug(homeify(abspath));
//debug(homeify(basedir || cwd));
    const shortpath = pathlib.relative(homeify(basedir || cwd), homeify(abspath).grab(2)).grab(1);
//debug(shortpath);
    const prefix = (grab[2] && (grab[1] != grab[2]))? "./": "";
//debug(prefix);
//    debug(`relpath: abs '${abspath}', base '${basedir || cwd}' => prefix '${prefix}' + shortpath '${shortpath}'`.blue_lt);
    return prefix + shortpath;

    function homeify(abspath)
    {
        var homedir = os.homedir();
        if (homedir.slice(-1) != "/") homedir += "/";
//debug("home-1", abspath);
//debug("home-2", homedir);
//debug("home-3", abspath.indexOf(homedir));
//might have sym link so relax position check:
//        return !abspath.indexOf(homedir)? "~/" + abspath.slice(homedir.length): abspath;
        const ofs = abspath.indexOf(homedir);
        return (ofs != -1)? "~/" + abspath.slice(ofs + homedir.length): abspath;
    }
}


function timestamp()
{
    var timestamp = elapsed().toString(); //util.format("[%f] ", elapsed() / 1e3)
    if (timestamp.length < 4) timestamp = ("0000" + timestamp).slice(-4);
    timestamp = `${timestamp.slice(0, -3)}.${timestamp.slice(-3)}`;
    return timestamp;
}


function quit(args)
{
    args = Array.from(arguments).unshift_fluent("quiting:".red_lt);
    debug.apply(debug, args);
    console.error.apply(console.error, args);
    process.exit(1);
}


function commas(num) { return num.toLocaleString(); } //grouping (1000s) default = true


//inline grabber:
//convenience for grabbing and reusing values from within nested expr
function grab(thingy, label)
{
//debug("args", arguments);
    grab[label] = thingy;
//    debug(`grab[${label}] <- '${thingy}'`);
    return thingy; //fluent
};


function extensions()
{
    Array.prototype.unshift_fluent = function(args) { this.unshift.apply(this, arguments); return this; };
    String.prototype.grab = function(inx) { return grab(this, inx); }
    Object.defineProperties(String.prototype,
        {
//            escnl: { get() { return this.replace(/\n/g, "\\n"); }}, //escape all newlines
//            nocomment: { get() { return this.replace(/\/\*.*?\*\//gm, "").replace(/(#|\/\/).*?$/gm, ""); }}, //strip comments; multi-line has priority over single-line; CAUTION: no parsing or overlapping
//            nonempty: { get() { return this.replace(/^\s*\r?\n/gm , ""); }}, //strip empty lines
//            escre: { get() { return this.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }}, //escape all RE special chars
            json_tidy: { get() { return this.replace(/,"/g, ", \"").replace(/"(.*?)":/g, "$1: ").replace(/\d{5,}/g, (val) => `0x${(val >>> 0).toString(16)}`); }}, //tidy up JSON for readability
        });
    }


////////////////////////////////////////////////////////////////////////////////
////
/// Unit test/CLI:
//

//default options:
const OPTS =
{
    folder: process.cwd(),
    mp3name: "*.mp3",
    seqname: "*.seq",
    autoplay: true,
    async: false,
};

const CLI =
module.exports.CLI =
function CLI(caller_opts)
{
debugger;
//    ((i == 2)? shebang_args(process.argv[i]): [process.argv[i]]).forEach((arg, inx, all) => //shebang might also have args (need to split and strip comments)
//    opts = Object.assign({}, OPTS, (typeof opts == "object")? opts: {[`${default_props[typeof opts] || "filename"}`]: opts}); //shallow copy, convert to obj
//map single unnamed arg to named options:
if (false) //TODO?
    switch (typeof opts)
    {
        case "object": opts = Object.assign(OPTS, opts); break; //caution: shallow copy
        case "number": opts = Object.assign(OPTS, {repeat: opts}); break;
        case "boolean": opts = Object.assign(OPTS, {autoplay: opts}); break;
        case "string": opts = Object.assign(OPTS, {[`${opts.match(/mp3$/)? "mp3name": opts.match(/seq$/)? "seqname": "folder"}`]: opts}); break;
        default: quit(`don't know how to handle ${typeof opts} opt ${opts}`.red_lt);
    }
//TODO: show opts
//TODO: async
//    const pattern = opts.mp3name || ((opts.folder || process.cwd()) + "/" + (opts.mp3name || "*.mp3"));
//    debug(`find mp3(s) at '${pattern}'`.blue_lt); //, __dirname + "/*.mp3"); //path.join(__dirname, "*.mp3"));
    const opts = {};
    const folder = true? process.cwd(): __dirname; //if (basedir)
    const files = process.argv
        .slice(2) //drop node + my exe name
        .map((arg) =>
        {
            if (!arg.match(/^[+\-]/)) return arg; //use filename as-is
            opts.push(arg);
            return null; //remove options from list
        })
        .reduce((keep_ary, arg) =>
        {
            if (arg) keep_ary.push(arg); //exclude empty values from final list
            return keep_ary;
        }, []);
    if (!files.length)
    {
        const pattern = folder + "/*.mp3";
        files.push.apply(files, glob.sync(pattern)); //pattern, {}); //, function (err, files)
//    const args = []; //unused args to be passed downstream
//    const defs = []; //macros to pre-define
//    const regurge = [];
//    CaptureConsole.startCapture(process.stdout, (outbuf) => { regurge.push(outbuf); }); //.replace(/\n$/, "").echo_stderr("regurge")); }); //include any stdout in input
//    const files = []; //source files to process (in order)
//    debug.buffered = true; //collect output until debug option is decided (options can be in any order)
//    const startup_code = [];
//    const downstream_args = [];
//    process.argv_unused = {};
//    for (var i = 0; i < process.argv.length; ++i)
        if (!files.length) console.error(`${files.length} file(s) match '${relpath(pattern)}'`.yellow_lt);
    }
//debugger;
    debug("opts", opts);
//    debug("files", files);
//    debug(`${files.length} files: \n{files.map((file, inx, all) => `${inx + 1}/${all.length}. '${relpath(file)}'`).join(", \n")}`.blue_lt);
    debug(`${files.length} file(s): \n${files.map((file, inx, all) => `${inx + 1}/${all.length}: '${relpath(file)}'`).join(", \n")}`.blue_lt);
//    files.forEach((file) => mp3player(file));
    playback();

    function playback(inx)
    {
        inx || (inx = 0);
        if (!files[inx]) return; //end of list
        mp3player(files[inx], function(evt, eof, data) //=> //NOTE: => functions don't have "this"
        {
//            cb.call(123, null, 456, "abc");
//            debug("this", typeof this, this);
//            debug(arguments.length, evt, eof, data);
            ++this.count || (this.count = 1);
            if (!(this.count % 50) && data) console.log(`#wr ${commas(data.numwr)}, count ${commas(this.count)}, wrtotal ${commas(data.wrtotal)}, time ${data.wrtotal / 4 / 44100}`);
            if (!data) console.log(evt, this, `time ${this.wrtotal / 4 / 44100}`);
            if (eof) playback(inx + 1);
        });
    }
}


if (!module.parent) CLI(); //.pipe(process.stdout); //auto-run CLI, send generated output to console
//quit("bye");

//eof