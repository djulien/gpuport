#!/usr/bin/env node
//define stuff that needs to be shared between modules/processes

'use strict'; //find bugs easier
//require('colors').enabled = true; //for console output colors
const tryRequire = require("try-require"); //https://github.com/rragan/try-require
const cluster = tryRequire("cluster") || {}; //https://nodejs.org/api/cluster.html; //http://learnboost.github.com/cluster


//shm keys:
//memory is slow on RPi (compared to PCs) so use shm to avoid costly memory xfrs
//for that to work, the same shm keys and structs need to be used by all procs
//this is a central place to define keys and structs so they keep in sync
//"ipcs -m" is useful for dev/debug; it shows keys in hex; use same prefix (upper word) for all shm segs for easier debug

//TODO: maybe use https://github.com/majimboo/c-struct or https://github.com/streamich/typebase or https://github.com/xdenser/node-struct

const shmdata = 
module.exports.shmdata =
{
    info: 0xbeef0000, //general control info
    get epoch() { return } 0xbeef0001;
    nodes: (0xfeed0000 | NNNN_hex(UNIV_MAXLEN)) >>> 0;

};

shmdata
    .int("i", 10)
    .uint32("u", 0x1234)
    .str("s", 24, "hello")
    .nodes("nodes", 4 )
const ver = shmdata.int("ver", "18.12.04").ver;

const shmkey = (0xfeed0000 | NNNN_hex(UNIV_MAXLEN)) >>> 0;
const UNIV_BYTELEN = /*cache_pad64*/(UNIV_MAXLEN) * Uint32Array.BYTES_PER_ELEMENT;
debug(`make_nodes: shmkey ${typeof shmkey}:`, hex(shmkey), "univ bytelen", commas(UNIV_BYTELEN));
const shbuf = sharedbuf.createSharedBuffer(shmkey, QUELEN * NUM_UNIV * UNIV_BYTELEN, cluster.isMaster); //master creates new, slaves acquire existing


//eof
