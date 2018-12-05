#!/usr/bin/env node
//debug logger:
//patterned after https://github.com/visionmedia/debug (uses DEBUG env var for control),
// but auto-defines name, uses indents, and allows caller to specify color and detail level or adjust stack
//turn debug on with:
// DEBUG=* node caller.js
// DEBUG=thing1,thing2 node caller.js
// DEBUG=thing1=level,thing2=level node caller.js
//turn off or exclude with:
// DEBUG=-thing

//to debug: DEBUG=* node debug file.js

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
require("magic-globals"); //__file, __line, __stack, __func, etc
const util = require('util');
//const pathlib = require('path');
//const {elapsed/*, milli*/} = require('./elapsed');
//const {caller/*, calledfrom, shortname*/} = require("./caller");
const {elapsed/*, milli*/} = require('./elapsed'); //CAUTION: recursive require(), so defer until debug() is called

extensions(); //hoist for in-line code below

//console.log("parent", module.parent);
//const my_parent = path.basename(module.parent.filename, path.extname(module.parent.filename));
//delete require.cache[__filename]; //kludge: get fresh parent info each time; see http://stackoverflow.com/questions/13651945/what-is-the-use-of-module-parent-in-node-js-how-can-i-refer-to-the-requireing
//const parent_name = module.parent.filename;


const caller =
module.exports.caller =
function caller(depth)
{
    if (isNaN(depth)) depth = 0;
    for (var i = 1; __stack[i]; ++i) //skip self
    {
        var parent = `${__stack[i].getFileName().split(/[\\\/]/g).back.replace(/\.js$/, "")}:${__stack[i].getLineNumber()}`; //drop ".js" (redundant since we already know it's Javascript)
//        if (!want_all)
        if (parent.indexOf("node_modules") != -1) continue; //exclude npm pkgs
//        if (stack.getFileName() == __filename) return true;
//if (!debug.nested--) break;
        if (!depth--) return parent;
    }
}


const debug =
module.exports.debug =
function debug(args)
{
//no; allow it but handle it    if (debug.busy) return; //avoid recursion (via elapsed())
    let was_busy = debug.busy;
    debug.busy = true;
    try
    {
//console.error("debug:" + JSON.stringify(arguments));
        var detail = 0;
        args = Array.from(arguments); //turn into real array
        if (/*(args.length >= 1) &&*/ (typeof args[0] == "number") && (args[1].toString().indexOf("%") != -1)) detail = /*args[0];*/ args.shift(); //optional first arg = debug detail level
//    if ((args.length < 1) || (typeof args[0] != "string")) args.unshift("%j"); //placeholder for fmt
//??    else if (args[0].toString().indexOf("%") == -1) args.unshift("%s"); //placeholder for fmt
        const parent = caller(++debug.nested || 1);
        debug.nested = 0; //reset for next time
//    debug.wanted || (debug.wanted = {});
        var want_detail = debug.wanted[parent.replace(/^@|:.*$/g, "")] || debug.wanted['*'] || -1;
//console.log("enabled: %j, parent %s", Object.keys(want_debug), my_parent.replace(/^@|:.*$/g, ""));
//console.log("DEBUG '%s': want %d vs current %d, discard? %d, options %j", my_parent, want_detail, detail, detail >= want_detail, want_debug);
        if (detail >= want_detail) return; //too much detail; caller doesn't want it

//    if (typeof args[0] == "string")
//    if (args[0].toString().indexOf("%") == -1)
//    if (args.length > 1)
//    {
//        var fmt = args[0];
//    debugger;
//    const {elapsed/*, milli*/} = require('./elapsed'); //CAUTION: recursive require(), so defer until debug() is called
        var fmt = util.format.apply(util, args);
//    ColorCodes.lastIndex = -1; //clear previous search (persistent)
        const ColorCodes = /\x1b\[\d+(;\d+)?m/g; //ANSI color escape codes; NOTE: need /g to get last match
        var first_ofs, last_ofs;
        for (;;)
        {
            var match = ColorCodes.exec(fmt);
            if (!match) break;
//console.log("found last", match, ColorCodes);
//        last_ofs = match[0].length; //(match && !match.index)? match[0].length: 0; //don't split leading color code
//        if (!first_ofs) first_ofs = last_ofs;
//console.log(match[0].length, match.index, ColorCodes.lastIndex, match);
            if (isNaN(first_ofs)) first_ofs = !match.index? match[0].length: 0; //(match && !match.index)? match[0].length: 0; //don't split leading color code
            last_ofs = (match.index + match[0].length == fmt.length)? match.index: fmt.length; //ColorCodes.lastIndex;
        }
        ColorCodes.lastIndex = 0; //reset for next time (regex state is persistent with /g or /y)
//    var svcolor = [];
//    fmt = fmt.replace(ColorCodes, function(str) { svcolor.push(str); return ''; }); //strip color codes
//    if (!svcolor.length) svcolor.push('');
//    fmt = fmt.substr(0, ofs) + `[${parent.slice(1)} @${trunc(elapsed(), 1e3)}] ` + fmt.substr(ofs);
//console.log(`${fmt.length}:'${fmt.replace(/\x1b/g, "\\x1b")}', first ofs ${first_ofs}, last ofs ${last_ofs}`);
        if (isNaN(first_ofs)) { first_ofs = 0; last_ofs = fmt.length; }
//TODO: sprintf %4.3f
//    debug.busy = true; //avoid recursion
        var timestamp = was_busy? "????": elapsed().toString(); //util.format("[%f] ", elapsed() / 1e3)
//    debug.busy = false;
        if (timestamp.length < 4) timestamp = ("0000" + timestamp).slice(-4);
        timestamp = `[${timestamp.slice(0, -3)}.${timestamp.slice(-3)}] `;
        fmt = fmt.slice(0, first_ofs) + timestamp + fmt.slice(first_ofs, last_ofs) + `  @${parent}` + fmt.slice(last_ofs);
//console.log("result", `${fmt.length}:'${fmt.replace(/\x1b/g, "\\x1b")}'`);
//    }
//    return console.error.apply(console, fmt); //args); //send to stderr in case stdout is piped
        return console.error(fmt);
    }
    finally { debug.busy = false; }
}
debug.nested = 0;


//debug.nested = 0; //allow caller to adjust stack level
debug.wanted = {};
(process.env.DEBUG || "").split(/\s*,\s*/).forEach((part, inx) =>
{
    if (!part) return; //continue;
    const parsed = part.match(/^([+-])?([^\s=]+|\*)(\s*=\s*(\d+))?$/); //figure out meaning for this arg
    if (!parsed) return console.error(`ignoring unrecognized DEBUG option[${inx}]: '${part}'`.red_lt);
    var [, remove, name,, level] = parsed;
//    console.log("part: \"%s\", +/- '%s', name '%s', level '%s'", part, remove, name, level);
    if (name == "*") //all on/off; clear previous options
        debug.wanted = (remove == "-")? {}: {'*': level || Number.MAX_SAFE_INTEGER}; //remove/enable all, set default level if missing
    else //named module
        debug.wanted[name] = (remove == "-")? -1: level || Number.MAX_SAFE_INTEGER;
});
//tell user how to get debug msgs:
if (!process.env.DEBUG) console.error(`Prefix with "DEBUG=${__file}" or "DEBUG=*" for debug msgs.`.yellow_lt);


/////////////////////////////////////////////////////////////////////////////////
////
/// utility/helper functions:
//

//function trunc(val, digits)
//{
//    return Math.round(val * digits) / digits;
//}

function extensions()
{
    if (!Array.prototype.back)
        Object.defineProperty(Array.prototype, "back", //"top",
        {
            get() { return this[this.length - 1]; }, //NOTE: will be undefined when array is empty; okay
            set(newval) { if (this.length) this[this.length - 1] = newval; else this.push(newval); }, //return this; },
        });
//escape all newlines:
//makes debug or detecting multiple lines easier
//    String.prototype.esc = function(ch) { return (this || "").replace(/\n/g, "\\n".cyan_lt); }; //draw attention to newlines (for easier debug)
}


////////////////////////////////////////////////////////////////////////////////
////
/// Unit test:
//

if (!module.parent)
{
    require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127

//    /*if (!process.env.DEBUG)*/ console.error(`use "DEBUG=${__file}" prefix for debug`.yellow_lt);
    debug("hello".green_lt);
    setTimeout(() => { debug("nope"); ++debug.nested; debug("good", "bye".red_lt); }, 1000);
}

//eof