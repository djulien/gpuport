#!/usr/bin/env node 
//#-xyz xdebug=10 -abc what is this #this is ignored
//some test strings:
// #!./shebang.js +debug +echo +hi#lo ./pic8-dsl.js "arg space" \#not-a-comment +preproc "not #comment" -DX -UX -DX=4 -DX="a b" +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./shebang.js +debug +echo +hi\#lo ./pic8-dsl.js "arg space" \#not-a-comment +preproc "not #comment" -DX -UX -DX=4 -DX="a b" +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./shebang.js +debug +echo ./pic8-dsl.js xyz"arg space"ab \#not-a-comment +preproc p"not #comment" -DX -UX -DX=4 -DX="a b"q +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./shebang.js +debug +echo ./pic8-dsl.js -DX -UX -DX=4 -DX="a b" +preproc +ast -run -reduce -codegen  #comment out this line for Node.js REPL .load command
//shebang ("#!") utility functions

//to debug:
//node  debug  this-file  args
//cmds: run, cont, step, out, next, bt, .exit, repl

'use strict';
//const DEV = true; //false;
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
const XRegExp = require("xregexp"); //https://github.com/slevithan/xregexp
//TODO? const miss = require("mississippi"); //stream utils
const fs = require("fs");
const thru2 = require("through2"); //https://www.npmjs.com/package/through2
const {debug} = require('./debug');
extensions(); //hoist for in-line code below


////////////////////////////////////////////////////////////////////////////////
////
/// Text stream handler:
//

//comment out #! if found at start of stream:
const shebang =
module.exports.shebang =
function shebang(opts)
{
    if (this instanceof shebang) throw `don't use "new" with shebang()`.red_lt;
//    opts = opts || {};
//    return Object.assign(thru2(xform, flush), {pushline}); //Object.assign(thru2(/*{objectMode: false},*/ xform, flush), {pushline});
    return thru2(xform, flush); //Object.assign(thru2(/*{objectMode: false},*/ xform, flush), {pushline});

    function xform(chunk, enc, cb)
    {
        const SHEBANG_xre = XRegExp(`
            ^  #start of line
#            (?<shebang>
            \\s* \\# \\s* !  #ignore white space (non-standard, though)
            [^\\n]*  #shebang args (greedy)
#            )
#            \\n?
            `, "x"); //NOTE: real shebang doesn't allow white space
//        const SHEBANG_re = /^\s*\#\s*![^\n]*/; //greedy; NOTE: real shebang doesn't allow white space, but allow it here for easier dev/debug
//        if (isNaN(++this.numchunks)) this.numchunks = 1;
        ++this.numchunks || (this.numchunks = 1);
        if (typeof chunk != "string") chunk = chunk.toString(); //TODO: enc?
//debug(`shebang[${this.numlines}]: '${chunk.escnl}'`); //replace(/\n/g, "\\n")}'`);
//        if (!opts.shebang && !this.chunks.length && chunk.match(/^\s*#\s*!/)) { this.chunks.push(`//${chunk} //${chunk.length}:line 1`); cb(); return; } //skip shebang; must occur before prepend()
//            if (!opts.shebang && (this.linenum == 1) && chunk.match(/^\s*#\s*!/)) { this.chunks.push("//" + chunk + "\n"); cb(); return; } //skip shebang; must occur before prepend()
//NOTE: input stream does not need to be split by newlines here, because #! is only checked first time
//        var parts;
        if (!opts.shebang && (this.numchunks == 1))
//            if (parts = chunk.match(SHEBANG_xre)) chunk = "//" + parts.shebang.blue_lt; //`${chunk} //${chunk.length}:line 0 ${opts.filename || "stdin"}`.blue_lt; //cb(); return; } //skip shebang; must be first line
//            else debug(`no shebang: ${chunk.slice(0, 100).escnl}`.blue_lt);
            if ((opts || {}).emit_shebang && chunk.match(SHEBANG_xre)) this.emit("shebang", chunk.split("\n").front);
            chunk = chunk.replace(SHEBANG_xre, (match) => "//" + match.blue_lt); //.echo_stderr(`shebang replace: '${chunk.slice(0, 100).escnl}'...`); //`${chunk} //${chunk.length}:line 0 ${opts.filename || "stdin"}`.blue_lt; //cb(); return; } //skip shebang; must be first line
        this.push/*line*/(chunk);
        cb();
    }
    function flush(cb) { cb(); }
}


//function is_shebang(chunk)
//{
//    return (this.linenum == 1) && chunk.match(/^\s*#\s*!/);
//}


////////////////////////////////////////////////////////////////////////////////
////
/// "#!"" line handler:
//

//split shebang string into separate args:
//function shebang_args(str, which)
//{
//    if (!which) str = str.replace(/\s*#.*$/, ""); //strip comments
//    return (which < 0)? [str]: str.split(" "); //split into separate args
//}


//split shebang string into separate args:
//shebang args are space-separated in argv[2]
//some test strings:
// #!./prexproc.js +debug +echo +hi#lo ./pic8-dsl.js "arg space" \#not-a-comment +preproc "not #comment" -DX -UX -DX=4 -DX="a b" +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./prexproc.js +debug +echo +hi\#lo ./pic8-dsl.js "arg space" \#not-a-comment +preproc "not #comment" -DX -UX -DX=4 -DX="a b" +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./prexproc.js +debug +echo ./pic8-dsl.js xyz"arg space"ab \#not-a-comment +preproc p"not #comment" -DX -UX -DX=4 -DX="a b"q +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./prexproc.js +debug +echo ./pic8-dsl.js -DX -UX -DX=4 -DX="a b" +preproc +ast -run -reduce -codegen  #comment out this line for Node.js REPL .load command
const shebang_args =
module.exports.shebang_args =
function shebang_args(str)
{
    debug(`shebang args from: ${str}`.blue_lt);
/*
    const COMMENT_xre = XRegExp(`
        \\s*  #skip white space
        (?<! [\\\\] )  #negative look-behind; don't want to match escaped "#"
        \\#  #in-line comment
        .* (?: \\n | $ )  #any string up until newline (non-capturing)
        `, "x");
    const UNQUO_SPACE_xre = /\s+/g;
//https://stackoverflow.com/questions/366202/regex-for-splitting-a-string-using-space-when-not-surrounded-by-single-or-double
    const xUNQUO_SPACE_xre = XRegExp(`
        ' ( .*? ) '  #non-greedy
     |  " ( .*? ) "  #non-greedy
     |  \\S+
        `, "xg");
    const KEEP_xre = XRegExp(`
        \\s*
        (  #quoted string
            (?<quotype> (?<! \\\\ ) ['"] )  #capture opening quote type; negative look-behind to skip escaped quotes
            (?<quostr>
                (?: . (?! (?<! \\\\ ) \\k<quotype> ))  #exclude escaped quotes; use negative lookahead because it's not a char class
                .*?  #capture anything up until trailing quote (non-greedy)
            )
            \\k<quotype>  #trailing quote same as leading quote
        |  #or bare (space-terminated) string
            (?<barestr>
#                ( (?<! [\\\\] ) [^\\s\\n\\#] )+  #any string up until non-escaped space, newline or "#"
                (?: . (?! (?<! \\\\ ) [\\s\\n\\#] )) +  #exclude escaped space, newline, or "#"; use negative lookahead because it's not a char class
                .*?  #capture anything not above (non-greedy)
                (?: [\\s\\n\\#] )
            )
        )
        \\s*
*/
//new Regex(@"(?< switch> -{1,2}\S*)(?:[=:]?|\s+)(?< value> [^-\s].*?)?(?=\s+[-\/]|$)");
    const KEEP_xre = XRegExp(`
#        (?<= \\s )  #leading white space (look-behind)
#        (?: ^ | \\s+ )  #skip leading white space (greedy, not captured)
        \\s*  #skip white space at start
        (?<argstr>  #kludge: need explicit capture here; match[0] will include leading/trailing stuff even if though non-capturing
            (
                (  #take quoted string as-is
                    (?: (?<quotype> (?<! \\\\ ) ['"] ))  #opening quote type; negative look-behind to skip escaped quotes
                    (
                        (?: . (?! (?<! \\\\ ) \\k<quotype> ))  #exclude escaped quotes; use negative lookahead because it's not a char class
                        .*?  #capture anything up until trailing quote (non-greedy)
                    )
                    (?: \\k<quotype> )  #trailing quote same as leading quote (not captured)
                )
            |
                (?<= \\\\ ) [\\s\\n\\#]  #or take escaped space/newline/"#" as regular chars; positive look-behind
            |
                [^\\s\\n\\#]  #or any other char
            )+  #multiple occurrences of above (greedy)
        )
        (?: \\s+ | \\# .* | $ )  #skip trailing white space or comment (greedy, not captured)
#        (?= \s )  #trailing white space (look-ahead)
        `, "xy"); //"gx"); //sticky to prevent skipping over anything; //no-NOTE: sticky ("y") no worky with .forEach //"gxy"); //"gmxy");
//        (?: \\n | $ )
//    str.replace(/\s*#.*$/, ""); //strip trailing comment
//    return (which < 0)? [str]: str.split(" "); //split into separate args
//    return (str || "").replace(COMMENT_xre, "").split(UNQUO_SPACE_xre).map((val) => { return val.unquoted || val}); //strip comment and split remaining string
//debug(`${"shebang str".cyan_lt}: '${str}'`);
//debug(!!"0");
    var matches = [];
//    for (var ofs = 0;;)
//    {
//        var match = XRegExp.exec(` ${str}` || "", KEEP_xre); //, ofs, "sticky");
//        if (!match) break;
    XRegExp.forEach(str || "", KEEP_xre, (match, inx) => matches.push(match.argstr)); //debug(`#![${inx}]: '${match[0]}' ${match.index}`); //no-kludge: exclude surrounding spaces, which are included even though non-captured; //`${match.quostr || match.barestr}`); // || ""}`);
//    {
//        debug(`match[${inx}]:`.blue_lt, JSON.stringify(match), match.trimmed.quoted.cyan_lt); //, ${"quostr".cyan_lt} '${match.quostr}', ${"barestr".cyan_lt} '${match.barestr}'`);
//        matches.push(match.trimmed); //kludge: exclude surrounding spaces, which are included even though non-captured; //`${match.quostr || match.barestr}`); // || ""}`);
//        ofs = match.index + match[0].length;
//    }
//    });
    return matches;
}
//(?:x)  non-capturing match
//x(?=y)  positive lookahead
//x(?!y)  negative lookahead
//x(?<=y)  positive lookbehind
//x(?<!y)  negative lookbehind


////////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions:
//

//add quotes around a string:
function quote(str, quotype)
{
    return `${quotype = quotype || '"'}${str || ""}${quotype}`;
}


function extensions()
{
//array:
    Object.defineProperties(Array.prototype,
    {
        front: iif(!Array.prototype.hasOwnProperty("front", { get() { return this[0]; }})), //polyfill
    });
    Array.prototype.unshift_fluent = function(args) { this.unshift.apply(this, arguments); return this; };
//string:
    Object.defineProperties(String.prototype,
    {
        escnl: { get() { return this.replace(/\n/g, "\\n"); }}, //escape all newlines
        quoted: { get() { return quote(this/*.toString()*/); }, },
        quoted1: { get() { return quote(this/*.toString()*/, "'"); }, },
//        unquoted: { get() { return unquote(this/*.toString()*/); }, },
//        nocomment: { get() { return this.replace(/\/\*.*?\*\//gm, "").replace(/(#|\/\/).*?$/gm, ""); }}, //strip comments; multi-line has priority over single-line; CAUTION: no parsing or overlapping
//        nonempty: { get() { return this.replace(/^\s*\r?\n/gm , ""); }}, //strip empty lines
//        escre: { get() { return this.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }}, //escape all RE special chars
        json_tidy: { get() { return this.replace(/,"/g, ", \"").replace(/"(.*?)":/g, "$1: ").replace(/\d{5,}/g, (val) => `0x${(val >>> 0).toString(16)}`); }}, //tidy up JSON for readability
        str_desc: { get() { return `${this.length}:${this.escnl.quoted}`; }, }, //show length and add quotes (helps find leading/trailing white space)
    });
//    String.prototype.grab = function(inx) { return grab(this, inx); }
    String.prototype.quote = function(quotype) { return quote(this/*.toString()*/, quotype); }
//    String.prototype.unquote = function(quotype) { return unquote(this/*.toString()*/, quotype); }
}


////////////////////////////////////////////////////////////////////////////////
////
/// CLI/unit test/sample code:
//

if (!module.parent) //.pipe(process.stdout); //auto-run CLI, send generated output to console
{
    debugger;
    debug("raw args:");
//    for (var i = 0; i < process.argv.length; ++i)
    process.argv.forEach(show_arg);
    debug("my first line:", fs.readFileSync(__filename).split("\n").front.str_desc);
    debug("processed args:");
//shebang might also have args (need to split and strip comments); merge into argv:
    process.argv.splice(2, 1, shebang_args(process.argv[2])).forEach(show_arg);
    fs.createReadStream(__filename)
        .pipe(shebang())

    function show_arg(arg, i, all)
    {
        const new_arg_count = all.length - process.argv.length - 1;
        debug(`arg[${i}/${all.length}]: '${arg}'`[(i < 2)? "gray_lt": (i < 2 + new_arg_count)? "cyan_lt": "blue_lt"]);
    }
}

//eof