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
//const pathlib = require("path"); //NOTE: called it something else to reserve "path" for other var names
//const JSON5 = require("json5"); //more reader-friendly JSON; https://github.com/json5/json5
const XRegExp = require("xregexp"); //https://github.com/slevithan/xregexp
extensions(); //hoist to top


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
        `.replace(/^(\s*)/gm, (match) => match.substr((this.prefix || (this.prefix = match)).length)).red_lt); //remove indents
    process.exit(1);
}
files.forEach((filearg, inx) => (!inx? shebang_args(filearg): [filearg]).forEach((filename) => readfile(filename)));

//console.error(`hello @${__line}`.cyan_lt);
//var x = 4; //test "use strict"
//console.error(`bye @${__line}`.cyan_lt);


function readfile(path)
{
    if (path.match(/^[-+].+|=/)) { debug(`TODO: handle option '${str}'`.yellow_lt); return; }
    debug(`read file '${path}' ...`.cyan_lt);
    const infile = (path == "-")? process.stdin: fs.createReadStream(path);
    infile.pipe(
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


function debug(args)
{
    console.log.apply(null, arguments);
}


function extensions()
{
    Object.defineProperty(Array.prototype, "top", { get() { return this[this.length - 1]; }});
}


//eof
