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
//const {elapsed/*, milli*/} = require('./elapsed') || (()=>"????"); //CAUTION: cyclic require(), so return a dummy function if real one is not available yet
let elapsed = ()=>"????"; //kludge: (cyclic require) give initial dummy function and then fill in later
const pathlib = require('path');
//const {elapsed/*, milli*/} = require('./elapsed');
//const {caller/*, calledfrom, shortname*/} = require("./caller");
//module.exports.debug = debug; //hoist debug
//const {elapsed/*, milli*/} = require('./elapsed'); //CAUTION: cyclic require(), so defer until debug() is called

extensions(); //hoist for in-line code below


////////////////////////////////////////////////////////////////////////////////
////
/// Detail level handler:
//

//detail levels for certain types of messages:
const WARN_LEVEL = -1;
const ERROR_LEVEL = -2;

//detail levels:
const SILENT_LEVEL = -3; //absolutely nothing; don't use
const NONE_LEVEL = -1; //warnings/errors only
const DEFAULT_LEVEL = 33;


//check debug detail level:
//if used as follows, allows caller to avoid expensive info if not wanted
//  debug(...., wanted(5) && expensive_function(), ...)
//const wanted =
//module.exports.wanted =
function wanted(name, msg_level)
{
    if (!isNaN(name)) [name, msg_level] = [debug.parent, name]; //shuffle optional args
    const wanted_level = detail(name);
//console.log(`debug.wanted(name '${name}', msg ${msg_level}): <= wanted ${wanted_level}? ${msg_level <= wanted_level} @${__fili.drop_dirname}`);
    return msg_level <= wanted_level;
}


//get/set detail level:
//const detail =
//module.exports.detail =
//debug.wanted =
function detail(name, new_level)
{
    if (!detail.levels) detail.levels = {};
    if (!isNaN(name)) [name, new_level] = [debug.parent, name]; //shuffle optional args
    if (new_level === true) new_level = DEFAULT_LEVEL;
    if (new_level === false) new_level = NONE_LEVEL;
    if (typeof new_level != "undefined") //arguments.length > 1) //set new level
    {
        if (name == "*") detail.levels = !isNaN(new_level)? {"*": new_level}: {}; //NOTE: overrides all settings; //DEFAULT_LEVEL}; //-1
        else if (!isNaN(new_level)) detail.levels[name] = new_level;
        else if (detail.levels.hasOwnProperty(name)) delete detail.levels.name; //NOTE: defers to "*" setting; //DEFAULT_LEVEL;
    }
    return detail.levels.hasOwnProperty(name)? detail.levels[name]: //specific caller
        detail.levels.hasOwnProperty("*")? detail.levels['*']: //all callers
        DEFAULT_LEVEL; //unspecified default
}
//    debug.wanted || (debug.wanted = {});
//    return (debug.wanted(parent) >= detail_level);


//initialize debug levels from cmd line args:
//similar format to node debug pkg, but allows numeric values as well as on/off
//debug.nested = 0; //allow caller to adjust stack level
//var deb_levels = {};
//detail.levels = {};
(process.env.DEBUG || "").split(/\s*,\s*/).forEach((chunk, inx) =>
{
    if (!chunk) return; //continue;
    const parsed = chunk.match(/^([+-])?([^\s=]+|\*)(\s*=\s*(\d+))?$/); //figure out meaning for this arg
    if (!parsed) return console.error(`ignoring unrecognized DEBUG option[${inx}]: '${chunk}'`.red_lt);
    var [, remove, name,, level] = parsed;
    if (typeof level == "undefined") level = true;
//console.log("chunk: \"%s\", +/- '%s', name '%s', level '%s'", chunk, remove, name, level);
    const onoff = {"+": true, "-": false};
    detail(name, onoff[remove] || level);
//    if (name == "*") //all on/off; clear previous options
//        deb_levels = (remove == "-")? {}: {'*': (level || 1000)}; //Number.MAX_SAFE_INTEGER)}; //remove/enable all, set default level if missing
//    else //named module
//        deb_levels[name] = (remove == "-")? -1: (level || 1000); //Number.MAX_SAFE_INTEGER);
});
//tell user how to set debug options:
if (!process.env.DEBUG) console.error(`Prefix with "DEBUG=${__file}" or "DEBUG=*" to see debug info.`.yellow_lt);
debug("initial debug levels", JSON.stringify(detail.levels));
//console.error(`want_debug: ${JSON.stringify(ALL)}`);
//process.exit();


/////////////////////////////////////////////////////////////////////////////////
////
/// Generate debug msg:
//

//console.log("parent", module.parent);
//const my_parent = path.basename(module.parent.filename, path.extname(module.parent.filename));
//delete require.cache[__filename]; //kludge: get fresh parent info each time; see http://stackoverflow.com/questions/13651945/what-is-the-use-of-module-parent-in-node-js-how-can-i-refer-to-the-requireing
//const parent_name = module.parent.filename;

//might as well export this one also, might be useful to caller:
//const caller =
//module.exports.caller =
function caller(depth)
{
    if (isNaN(depth)) depth = 0;
    const svdepth = depth;
    for (var i = 1; __stack[i]; ++i) //skip self, node pkgs; need a loop because caller's callbacks might occur lower in the stack
    {
        var parent = `${__stack[i].getFileName().split(/[\\\/]/g).back.replace(/\.js$/, "")}:${__stack[i].getLineNumber()}`; //drop ".js" (redundant since we already know it's Javascript)
//        if (!want_all)
        if (parent.indexOf("node_modules") != -1) continue; //exclude npm pkgs
//        if (stack.getFileName() == __filename) return true;
//if (!debug.nested--) break;
        if (!depth--) return /*console.log(`caller[${svdepth}] = ${parent} @${__fili.drop_dirname}`),*/ parent;
    }
    throw `no caller[${svdepth}]? ${__stack.map((val, inx, all) => `[${inx}/${all.length}] ${pathlib.basename(val.getFileName())}:${val.getLineNumber()}`).join(", ")} @${__fili.drop_dirname}`.red_lt;
}


//const debug =
//module.exports.debug =
//TODO: convert args to lamba callback to reduce overhead when debug turned off
function debug(args)
{
//no; allow, but handle it    if (debug.busy) return; //avoid recursion (via elapsed())
    const was_busy = debug.busy;
//console.error(`debug IN: was busy? ${was_busy} ${__fili.drop_dirname}`);
    debug.busy = true;
    try
    {
//console.error("debug:" + JSON.stringify(arguments));
        var msg_detail = 1; //make non-0 so msg will be hidden by default
//TODO: need a more precise way to specify msg debug level:
//this function is already varargs, so using first arg is probably the most reliable approach
        args = Array.from(arguments); //turn into real array
        if (/*(args.length >= 1) &&*/ (typeof args[0] == "number") && (args.length > 1) /*&& (args[1].toString().indexOf("%") != -1)*/) msg_detail = /*args[0];*/ args.shift(); //optional first arg = debug detail level
//    if ((args.length < 1) || (typeof args[0] != "string")) args.unshift("%j"); //placeholder for fmt
//??    else if (args[0].toString().indexOf("%") == -1) args.unshift("%s"); //placeholder for fmt
//        ++debug.nested || (debug.nested = 1);
        const parent = caller(++debug.nested || 1);
        const want_msg = wanted(parent, msg_detail);
//        const svnested = debug.nested;
        debug.nested = 0; //reset for next time
        if (!want_msg) return; //console.log(`!msg[${svnested}] '${args[0]}' @${__fili.drop_dirname}`);
/*
        const parent = caller(++debug.nested || 1);
        debug.nested = 0; //reset for next time
//    debug.wanted || (debug.wanted = {});
        var want_detail = debug.wanted(parent.replace(/^@|:.*$/g, "")); //] || debug.wanted['*'] || -1;
//console.log("enabled: %j, parent %s", Object.keys(want_debug), my_parent.replace(/^@|:.*$/g, ""));
//console.log("DEBUG '%s': want %d vs current %d, discard? %d, options %j", my_parent, want_detail, detail, detail >= want_detail, want_debug);
//console.log(`DEBUG: msg_det ${msg_detail} vs. ${want_detail}, keep? ${msg_detail <= want_detail}, msg '${args[0]}'`)
        if (msg_detail > want_detail) return; //too much detail; caller doesn't want it
*/
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
//debugger;
//        console.error(`debug call elapsed?: was busy? ${was_busy} ${__fili.drop_dirname}`);
        if (typeof elapsed != "function") console.log(elapsed, __fili.drop_dirname);
        var timestamp = was_busy? "????": elapsed().toString(); //avoid recursion; //util.format("[%f] ", elapsed() / 1e3)
//    debug.busy = false;
        if (timestamp.length < 4) timestamp = ("0000" + timestamp).slice(-4);
        timestamp = `[[${timestamp.slice(0, -3)}.${timestamp.slice(-3)}]] `;
        fmt = fmt.slice(0, first_ofs) + timestamp + fmt.slice(first_ofs, last_ofs) + `  @${parent}.${msg_detail}` + fmt.slice(last_ofs);
//console.log("result", `${fmt.length}:'${fmt.replace(/\x1b/g, "\\x1b")}'`);
//    }
//    return console.error.apply(console, fmt); //args); //send to stderr in case stdout is piped
        return console.error(fmt);
    }
    finally { debug.busy = false; }
//console.error(`debug OUT: was busy? ${was_busy} ${__fili.drop_dirname}`);
}
//debug.nested = 0; //set initial nesting level


//debug.detail = detail;
Object.defineProperties(debug,
{
    parent: { get() { return caller((this.nested + 1) || 1).replace(/^@|:.*$/g, ""); }, }, //] || debug.wanted['*'] || -1;
    detail:
    {
        get() { return detail(this.parent); },
        set(newval) { return detail(this.parent, newval); },
    },
});


/////////////////////////////////////////////////////////////////////////////////
////
/// utility/helper functions:
//

//function trunc(val, digits)
//{
//    return Math.round(val * digits) / digits;
//}

//debugger;
//const {elapsed/*, milli*/} = require('./elapsed'); //CAUTION: cyclic require(), so defer until debug() is called
//const {elapsed/*, milli*/} = fixup(require('./elapsed'), ()=>"????"); //CAUTION: cyclic require(), so return a dummy function if real one is not available yet
//function fixup(exports, dummy)
//{
//    if (!exports.elapsed) exports.elapsed = dummy;
//    return exports;
//}
//console.error(require("./elapsed"));

function extensions()
{
//    if (!Array.prototype.back)
    if (!Array.prototype.hasOwnProperty("back")) //polyfill
        Object.defineProperty(Array.prototype, "back", //"top",
        {
            get() { return this[this.length - 1]; }, //NOTE: will be undefined when array is empty; okay
            set(newval) { if (this.length) this[this.length - 1] = newval; else this.push(newval); }, //return this; },
        });
//escape all newlines:
//makes debug or detecting multiple lines easier
//    String.prototype.esc = function(ch) { return (this || "").replace(/\n/g, "\\n".cyan_lt); }; //draw attention to newlines (for easier debug)
    Object.defineProperties(String.prototype,
    {
        drop_dirname: { get() { return this.replace(/^.*\//, ""); }, },
    });
}

//elapsed = require('./elapsed').elapsed; //kludge: resolve cyclic dependency after in-line init above done; //CAUTION: cyclic require(), so return a dummy function if real one is not available yet


////////////////////////////////////////////////////////////////////////////////
////
/// Unit test:
//

if (!module.parent)
{
    require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127

    debugger;
//    /*if (!process.env.DEBUG)*/ console.error(`use "DEBUG=${__file}" prefix for debug`.yellow_lt);
    debug("hello".green_lt);
    setTimeout(() => { debug(`nope`); ++debug.nested; debug("good", "bye".red_lt); }, 1000);
    debug("my detail level", debug.detail, detail.levels);
    debug.detail = 5;
    debug("my detail level", debug.detail, detail.levels);
    debug(20, "not visible", debug.detail, detail.levels);
    debug(detail.levels);
}


//exports and cyclic imports:
module.exports.wanted = wanted;
module.exports.detail = detail;
module.exports.caller = caller;
module.exports.debug = debug;
elapsed = require('./elapsed').elapsed; //kludge: resolve cyclic dependency after in-line init above done; //CAUTION: cyclic require(), so return a dummy function if real one is not available yet

//eof