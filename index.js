#!/usr/bin/env node

'use strict';
//const gpuport = require("./build/Release/gpuport.node");

//module.exports = gpuport;

//const nan = require('nan');
//console.log("\nnan", JSON.stringify(nan));
//const napi = require('node-addon-api');
//console.log("\nnapi.incl", JSON.stringify(napi.include));
//console.log("\nnapi.gyp", JSON.stringify(napi.gyp));

const addon = require('./build/Release/gpuport'); //.node');
const value = 8;    
console.log(`${value} times 2 equals`, addon.my_function(value));
console.log(addon.hello());

//const prevInstance = new testAddon.ClassExample(4.3);
//console.log('Initial value : ', prevInstance.getValue());

//eof