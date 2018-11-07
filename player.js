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

"use strict";
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
const fs = require("fs"); //https://nodejs.org/api/fs.html
extensions(); //hoist to top


///////////////////////////////////////////////////////////////////////////////
////
/// Main logic (CLI):
//

const files = process.argv.slice(2);
const ME = process.argv[1].split("/").top;
function isPiped(fd) { return fs.fstatSync(fd || 0).isFIFO() || fs.fstatSync(fd || 0).isFile(); }
if (isPiped() && !files.some((arg) => (arg == "-"))) files.push("-"); //include stdin at end unless placed earlier; allows other files to act like #includes
process.argv.forEach((arg, inx, all) => console.error(`arg[${inx}/${all.length}]: ${arg.length}:'${arg}'`.blue_lt));

if (!files.length) // && !isPiped(0))
{
    console.error(`
        usage: any of
            ${ME} <file> <file> ...  #process multiple files; use "-" for stdin
            <file>  #first line is shebang: "#!<${ME} path> <arg> <arg>"
            ${ME} < <file>  #redirected file
            <other command> | ${ME}  #piped input
        `.replace(/^(\s*)/gm, (match) => match.substr((this.prefix || (this.prefix = match)).length)).red_lt); //remove indents
    process.exit(1);
}
files.forEach((path, inx) => readfile(!inx? shebang(path): path));

//console.error(`hello @${__line}`.cyan_lt);
//var x = 4; //test "use strict"
//console.error(`bye @${__line}`.cyan_lt);


function readfile(path)
{
    console.log(`read file ${path} ...`.cyan_lt);
}

function shebang(str)
{
    console.log(`split shebang? ${str}`.pink_lt);
    return `de-shebang(${str})`;
}


///////////////////////////////////////////////////////////////////////////////
////
/// :
//

function extensions()
{
    Object.defineProperty(Array.prototype, "top", { get() { return this[this.length - 1]; }});
}


//eof
