#!/usr/bin/env node

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

console.error(`hello @${__line}`.cyan_lt);
var x = 4; //test "use strict"
process.argv.forEach((arg, inx, all) => console.error(`arg[${inx}/${all.length}]: ${arg.length}:'${arg}'`.blue_lt));

console.error(`bye @${__line}`.cyan_lt);

//eof