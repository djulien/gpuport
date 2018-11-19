#!/usr/bin/env node

//ways to run:
//1. player.js file.json file.anyext filename ...
//2. playlist.anyext with first line of  #!.../player.js -arg -arg #comment
//3. piped  something | player.js

//ref info:
//install directly from github:
//  npm install https://github.com/djulien/gpuport
//JSON streaming:
// https://en.wikipedia.org/wiki/JSON_streaming#Line-delimited_JSON
//https://medium.com/@Jekrb/process-streaming-json-with-node-js-d6530cde72e9
//https://github.com/uhop/stream-json
//http://oboejs.com/examples

//if need to do it in C++: https://github.com/nlohmann/json/

"use strict";
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
const fs = require("fs"); //https://nodejs.org/api/fs.html
//const pathlib = require("path"); //NOTE: called it something else to reserve "path" for other var names
//const JSON5 = require("json5"); //more reader-friendly JSON; https://github.com/json5/json5
const XRegExp = require("xregexp"); //https://github.com/slevithan/xregexp
const JSONStream = require('JSONStream'); //https://github.com/dominictarr/JSONStream
const evtstrm = require('event-stream'); //https://github.com/dominictarr/event-stream
const thru2 = require("through2"); //https://www.npmjs.com/package/through2
//const GpuPort = require("")
elapsed.started = Date.now(); //msec
extensions(); //hoist so inline code below can use
//const str = "{a}\n{b}\n{c}";
//debug(JSON.stringify(str.splitr(/}\s*{/g, "}}{{")));
//process.exit();


///////////////////////////////////////////////////////////////////////////////
////
/// Main logic (CLI):
//

const files = process.argv.slice(2);
const ME = process.argv[1].split("/").top;
function isPiped(fd) { return fs.fstatSync(fd || 0).isFIFO() || fs.fstatSync(fd || 0).isFile(); }
if (isPiped() && !files.some((arg) => (arg == "-"))) files.push("-"); //include stdin at end unless placed earlier; allows other files to act like #includes
process.argv.forEach((arg, inx, all) => debug(`arg[${inx}/${all.length}]: ${arg.length}:'${arg}'`.blue_lt));

if (!files.length) // && !isPiped(0))
{
    console.error(`
        usage: any of
            ${ME} <file> <file> ...  #process multiple files; use "-" for stdin
            <file>  #first line is shebang: "#!<${ME} path> <arg> <arg>"
            ${ME} < <file>  #redirected file
            <other command> | ${ME}  #piped input
        `.unindent(/^\s*/gm).red_lt); //.replace(/^\s*/gm, (match) => match.substr((this.prefix || (this.prefix = match)).length)).red_lt); //remove indents
    process.exit(1);
}
files.forEach((filearg, inx) => (!inx? shebang_args(filearg): [filearg]).forEach((filename) => readfile(filename)));

//console.error(`hello @${__line}`.cyan_lt);
//var x = 4; //test "use strict"
//console.error(`bye @${__line}`.cyan_lt);


function readfile(path)
{
    if (path.match(/^[-+].+|=/)) { debug(`TODO: handle option '${path}'`.yellow_lt); return; }
    debug(`read file '${path}' ...`.cyan_lt);
    ((path == "-")? process.stdin: fs.createReadStream(path))
//        .pipe(evtstrm.split()) //split on newlines
        .pipe(thru2(parse_xform, parse_flush))
//        .pipe(JSONStream.parse("*")) //"frames.*"))
//        .pipe(evtstrm.mapSync((data) => { console.error(data); return data; }));
        .on("frame", data => debug((timestamp() + JSON.stringify(data)).blue_lt))
        .on("eof", () => debug((timestamp() + "eof").cyan_lt))
        .pipe(process.stdout);
}


//clean up and parse input stream:
//strip comments (JSONStream doesn't like them)
function parse_xform(chunk, enc, cb)
{
    if (typeof chunk != "string") chunk = chunk.toString(); //TODO: enc?
//NO        chunk = chunk.replace(/\s+$/, ""); //right-trim only; need to keep left spaces for indent checking
//        chunk += ParsePad;
//    ++this.count || (this.count = 1);
    this.buf = (this.buf || "") + chunk; //append to fifo buf
//    for (var ofs, prev = -1; (ofs = this.buf.indexOf("\n", prev + 1)) != -1; prev = ofs)
//        this.write(this.buf.slice(prev + 1, ofs));
//TODO: better parsing
//    const re = /['"#\n{}]/g; //special chars
//    for (var match; match = re.exec(s);)
//    this.buf = this.buf.slice(prev + 1);
//    debug(`[${this.id || 1}]. ${chunk.length}:${chunk.replace(/\n/g, "\\n")}`.blue_lt);
    var chunks = this.buf.splitr(/}\s*{/g, "}}{{"); //kludge: valid JSON objects can't have "}{" so split there (arrays need "," between elements); look-behind no worky, so consume entire separator after inserting extra chars first; split idea from https://stackoverflow.com/questions/38833020/how-can-i-split-a-string-containing-n-concatenated-json-string-in-javascript-nod/38833262; this one didn't help: https://stackoverflow.com/questions/12001953/javascript-and-regex-split-string-and-keep-the-separator
//    debug(`${chunks.length} chunks:\n` + chunks.map((str, inx, all) => `[${inx}/${all.length}] ${str.length}:'${str.replace(/\n/g, "\\n")}'`).join("\n"));
    this.buf = chunks.pop(); //assume last chunk is partial; hold and prepend to next chunk
    chunks.forEach((str) => frame.call(this, str));
    cb();
}


function parse_flush(cb)
{
    if (this.buf) frame.call(this, this.buf);
    cb();
    this.emit("eof");
    console.error(`${plural(this.id)} frame${plural.suffix} processed`.cyan);
}


//process a frame:
//apply JSON fixups: strip comments, strip trailing commas, put quotes around property names, fix floats
//append unique id
//emit parsed object
function frame(str)
{
    const fixedup = str/*.echo("original str:")*/.replace(/#.*$/gm, "").replace(/,\s*(?=[}\]])/gm, "").replace(/(\w+):/g, '"$1":').replace(/(\D)\.(\d+)/g, "$1 0.$2").trim()/*.echo("cleaned up str:")*/;
    if (!fixedup) return;
//    debug(`parse ${fixedup.length}:${fixedup.escnl}`);
    const obj = JSON.parse(fixedup);
    obj.id = ++this.id || (this.id = 1);
    this.emit("frame", obj);
}


///////////////////////////////////////////////////////////////////////////////
////
/// :
//

//split shebang string into separate args:
//shebang args are space-separated in argv[2]
//some test strings:
// #!./prexproc.js +debug +echo +hi#lo ./pic8-dsl.js "arg space" \#not-a-comment +preproc "not #comment" -DX -UX -DX=4 -DX="a b" +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./prexproc.js +debug +echo +hi\#lo ./pic8-dsl.js "arg space" \#not-a-comment +preproc "not #comment" -DX -UX -DX=4 -DX="a b" +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./prexproc.js +debug +echo ./pic8-dsl.js xyz"arg space"ab \#not-a-comment +preproc p"not #comment" -DX -UX -DX=4 -DX="a b"q +echo +ast -run -reduce -codegen  #comment out this line for use with .load in Node.js REPL
// #!./prexproc.js +debug +echo ./pic8-dsl.js -DX -UX -DX=4 -DX="a b" +preproc +ast -run -reduce -codegen  #comment out this line for Node.js REPL .load command
//!hoist: const shebang_args =
//!hoist: module.exports.shebang_args =
function shebang_args(str)
{
    debug(`split shebang args from: '${str}'`.blue_lt);
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
    try{ //got a bad regex lib?
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
    const matches = [];
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
    }catch(exc) { console.error(`shebang_args: ${exc}`.red_lt); return [str]; }
}
//(?:x)  non-capturing match
//x(?=y)  positive lookahead
//x(?!y)  negative lookahead
//x(?<=y)  positive lookbehind
//x(?<!y)  negative lookbehind


function elapsed(reset)
{
    const delta = Date.now() - elapsed.started;
    if (reset) (elapsed.started += delta) || (elapsed.started = Date.now());
    return delta;
}

function timestamp()
{
    return `[${elapsed()}] `;
}


function plural(count, suffix_n, suffix_1)
{
    if (!arguments.length) return plural.suffix;
    plural.suffix = (count != 1)? (suffix_n || "es"): (suffix_1 || "");
    return count;
}


function debug(args)
{
    console.log.apply(null, arguments);
}


function extensions()
{
    if (!Array.prototype.back) //polyfill
        Object.defineProperty(Array.prototype, "back", { get() { return this[this.length - 1]; }});
//    if (!String.prototype.echo)
    String.prototype.echo = function echo(args) { args = Array.from(arguments); args.push(this.replace(/\n/g, "\\n")); console.error.apply(null, args); return this; } //fluent to allow in-line calls
//    if (!String.prototype.splitr)
    Object.defineProperty(String.prototype, "escnl", { get() { return this.replace(/\n/g, "\\n"); }});
    String.prototype.splitr = function splitr(sep, repl) { return this.replace(sep, repl).split(sep); }
    String.prototype.unindent = function unindent(prefix) { return this.replace(prefix, (match) => match.substr((this.svprefix || (this.svprefix = match)).length)); } //remove indents
}


//eof