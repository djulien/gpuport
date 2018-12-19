#!/usr/bin/env node
//mp3 utility functions

//to debug:
//node  debug  this-file  args
//cmds: run, cont, step, out, next, bt, .exit, repl

'use strict';
//const DEV = true; //false;
require("magic-globals"); //__file, __line, __stack, __func, etc
require("colors").enabled = true; //for console output; https://github.com/Marak/colors.js/issues/127
//require('colors'); //var colors = require('colors/safe'); //https://www.npmjs.com/package/colors; http://stackoverflow.com/questions/9781218/how-to-change-node-jss-console-font-color
const os = require('os');
const fs = require('fs');
const glob = require('glob');
const pathlib = require('path');
const lame = require('lame'); //NOT node-lame
const spkr = require("speaker"); //DEV? "../": "speaker"); //('yalp/my-speaker');
const {elapsed} = require('./elapsed');
const {debug, caller, detail, wanted} = require('./debug');
//const mp3len = require('./mp3len');
//const relpath = require('./yalp/relpath');
//const {timescale} = require('./yalp/timefmt');
extensions(); //hoist for in-line code below


//debugger;
/*
function a() { debug("A"); return 1; }
function b() { debug("B"); return 0; }
function c() { debug("C"); return 4; }
function d() { debug("D"); return 0; }
debug(a() || b() || c());
debug(a() || b() && c());
debug(a() && b() || c());
debug(a() && b() && c());
debug(b() || c() || d());
debug(b() || c() && d());
debug(b() && c() || d());
debug(b() && c() && d());
var x = 2;
--x; debug(typeof x, x);
--x; debug(typeof x, x);
x = true;
--x; debug(typeof x, x);
x = false;
--x; debug(typeof x, x);
process.exit();
*/

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


//version info:
const about =
module.exports.about =
function about()
{
    debug(`node.js version: ${JSON.stringify(process.versions, null, 2).json_tidy}`.cyan_lt);
    debug(`spkr api ${spkr.api_version}, name '${spkr.name}', rev ${spkr.revision}, \ndesc '${spkr.description}'`.cyan_lt);
    /*if (!process.env.DEBUG)*/ console.error(`use "DEBUG=${__file}" prefix for debug`.yellow_lt);
}


////////////////////////////////////////////////////////////////////////////////
////
/// Streamed MP3 playback:
//

//need status info:
//https://www.mpg123.de/api/group__mpg123__seek.shtml

//example mp3 player from https://gist.github.com/TooTallNate/3947591
//more info: https://jwarren.co.uk/blog/audio-on-the-raspberry-pi-with-node-js/
//this is impressively awesome - 6 lines of portable code!
//https://github.com/TooTallNate/node-lame
const playback =
module.exports.playback =
function playback(filename, opts, cb)
{//12.11.18
//shuffle args if they are not in expected order:
    if (typeof opts == "function") [opts, cb] = [cb, opts];
    if ((typeof opts == "string") || Array.isArray(opts)) [filename, opts] = [opts, filename];
    if (Array.isArray(filename)) exc_hard("TODO: filename array");
//console.log("mp3player: type", typeof filename, filename);
//    filename = filename.replace(/[()]/, "."); //kludge: () treated as special chars
//    const duration = mp3len(filename);
//    const want_play = true;
//    elapsed(0); //reset stopwatch
//debugger;
//debug(cb.toString());
    const state = {}; //create a place for caller to save data
    cb = cb? cb.bind(state): () => (debug.apply(debug, arguments) || true);
//debug(wanted(30), detail());
    debug(`playing '${relpath(filename)}', duration ${wanted(50) && mp3len(filename) || "(TMI)"} sec ...`.cyan_lt); //CAUTION: mp3en() is expensive (reads entire file); don't display unless caller wants it
//    var decoder, pbstart, pbtimer;
//    elapsed(0);
    grab({close: ()=>0}, "decoder"); //define dummy function until stream opens
    grab({progress: ()=>0}, "speaker");
    cb(`audio decode start @T+${timestamp()}`) || cancel(); //console.log("[%s] decode done!".yellow_light, timescale(elapsed() - started)); })
    fs.createReadStream(filename) //process.argv[2]) //specify mp3 file on command line
        .pipe(grab(new lame.Decoder(), "decoder"))
        .on('format', (fmt) =>
        {
            debug("opts", opts, "autoplay?", (opts ||{}).autoplay);
            debug(`mp3 '${relpath(filename)}', fmt ${JSON.stringify(fmt).json_tidy}`.blue_lt); //.sampleRate, format.bitDepth);
            grab(setInterval(() => cb(`audio progress @T+${timestamp()}`, null, grab.speaker.progress()) || cancel(), 1000), "progress");
//debug(grab.progress);
            if ((opts || {}).autoplay) /*this*/ grab.decoder.pipe(grab(new spkr(fmt), "speaker"))
//                .on('progress', (data) => cb(`audio progress @T+${timestamp()}: ${JSON.stringify(data).json_tidy}`, null, data) || cancel()) //{numwr: THIS.numwr, wrlen: r, wrtotal: THIS.wrtotal, buflen: (left || []).length}); //give caller some progress info
                .on('open', () => volume(), cb(`audio open @T+${timestamp()}`) || cancel()) //pbstart = elapsed(); cb("audio open"); }) // /*pbtimer = setInterval(position, 1000)*/; console.log("[%s] speaker opened".yellow_light, timescale(pbstart - started)); })
                .on('flush', () => cb(`audio flush @T+${timestamp()}`, eof())) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker end-flushed".yellow_light, timescale(elapsed() - started)); })
                .on('close', () => cb(`audio close @T+${timestamp()}`, eof())) //too late to cancel this one; // /*clearInterval(pbtimer)*/; console.log("[%s] speaker closed".yellow_light, timescale(elapsed() - started)); });
                .on('error', (err) => cb(`audio ERROR: ${err} @T+${timestamp()}`, eof(err))); //console.log("[%s] decode ERROR %s".red_light, timescale(elapsed() - started), err); });
            cb(`audio start @T+${elapsed()}`) || cancel();
//            elapsed(0);
        })
        .on('end', () => cb(`audio decode done @T+${timestamp()}`)) //console.log("[%s] decode done!".yellow_light, timescale(elapsed() - started)); })
        .on('error', (err) => cb(`audio decode ERROR: ${err} @T+${timestamp()}`, eof(err))); //console.log("[%s] decode ERROR %s".red_light, timescale(elapsed() - started), err); });
//    return started - 0.25; //kludge: compensate for latency
    function cancel() { grab.decoder.close(); } //((grab.decoder || {}).close || (()=>0))(); } //TODO: does eof propagate?
    function eof(err) { debug("cancel", caller(1), grab.progess); const was_playing = !!grab.progress; clearInterval(grab.progress); grab.progress = null; return err || was_playing; } //only generate one eof event
    function volume() { if (opts.volume == void(0)) return; grab.speaker.set_vol(opts.volume); debug(`set volume ${opts.volume}`.yellow_lt); }
}


////////////////////////////////////////////////////////////////////////////////
////
/// Rudimentary playlist:
//

const playlist =
module.exports.playlist =
function playlist(files, opts, inx)
{//12.16.18
    if (typeof opts != "object") [opts, inx] = [inx, null]; //shuffle optional args
    if (!Array.isArray(files)) files = [files];
    if (!files[inx || 0]) //end of list
    {
        if (!(opts || {}).repeat--) return; //CAUTION: alters caller's opts; is this okay?
        debug("repeat".pink_lt);
        inx = 0;
//        --opts.repeat; //TODO: don't alter caller's opts?
    }
//    inx || (inx = 0);
    debug(`playlist: start[${inx || 0}/${files.length}]`.pink_lt);
    playback(files[inx || 0], opts, function(evt, eof, data) //=> //NOTE: arrow functions don't have "this" so use a regular function
    {
//            cb.call(123, null, 456, "abc");
//            debug("this", typeof this, this);
//            debug(arguments.length, evt, eof, data);
//        ++this.count || (this.count = 1);
//        if (!(this.count % 50) && data) debug(`#wr ${commas(data.numwr)}, count ${commas(this.count)}, wrtotal ${commas(data.wrtotal)}, time ${data.wrtotal / 4 / 44100}`);
//        if (!data) console.log(evt, this, `time ${this.wrtotal / 4 / 44100}`);
        debug(evt.blue_lt, (eof || false).toString().red, JSON.stringify(data || {}).json_tidy.yellow_lt);
        if (eof) setTimeout(() => debug(`gap ${opts.gap || 500}`.pink_lt), playlist(files, opts, ++inx || 1), opts.gap || 500); //give a little time in between
        return true; //continue playback
    });
}


////////////////////////////////////////////////////////////////////////////////
////
/// Parse MP3 file to get duration:
//

//get mp3 playback length
//ID3 tags are unreliable, so this code actually scans the headers and adds up the frame times

//var binread = require('binary-reader'); //https://github.com/gagle/node-binary-reader

//logic taken from http://www.zedwood.com/article/php-calculate-duration-of-mp3
//see also http://stackoverflow.com/questions/383164/how-to-retrieve-duration-of-mp3-in-net/13269914#13269914
//Read first mp3 frame only...  use for CBR constant bit rate MP3s

//NOTE: this is synchronous for now, so caller can prep playlist immediately
//TODO: rewrite as async and use node-binary, node-struct, or binary-parser module?

//The number of bits/frame is:  frame_size*bit_rate/sample_rate.
//For MPEG1, frame_size = 1152 samples/frame
//For MPEG2, frame_size =  576 samples/frame

const mp3len =
module.exports.mp3len =
function mp3len(filename, use_cbr_estimate, cb)
{//5.20.15
    exc_soft("TODO: sync vs. async version");
    if (typeof use_cbr_estimate === 'function') { cb = use_cbr_estimate; use_cbr_estimate = false; } //optional param
    if (!cb) cb = function(retval) { return retval; }

    var fd = fs.openSync(filename, 'r');
    if (fd < 0) return cb(-1);
    var block = new Buffer(10); //(100);
    var rdlen = fs.readSync(fd, block, 0, block.length);
    if (rdlen < block.length) { fs.closeSync(fd); return cb(-1); }
//    block = block.slice(0, 10); //only need smaller reads

    var duration = 0;
    var offset = skipID3v2Tag(block);
    var filesize = fs.fstatSync(fd).size;
    while (offset < filesize)
    {
        rdlen = fs.readSync(fd, block, 0, 10, offset);
        if (rdlen < 10) break;
        //looking for 1111 1111 111 (frame synchronization bits)
        else if ((block[0] == 0xff) && ((block[1] & 0xe0) == 0xe0))
        {
//            console.log("hdr at ofs %d", offset - 10);
            var info = parseFrameHeader(block);
            if (!info.Framesize) return cb(Math.round(1000 * duration) / 1000); //some corrupt mp3 files; return partial result; make it accurate to msec
            if (use_cbr_estimate && info) return cb(estimateDuration(filesize, info.Bitrate, offset));
//                fseek($fd, $info['Framesize']-10, SEEK_CUR);
            offset += info.Framesize; //- 10;
            duration += info.Samples / info['Sampling Rate'];
        }
        else if (block.slice(0, 3).toString('utf8') == 'TAG')
        {
//            console.log("tag at ofs %d", offset - 10);
//                fseek($fd, 128-10, SEEK_CUR);//skip over id3v1 tag size
            offset += 128; //- 10;
        }
        else
        {
//                fseek($fd, -9, SEEK_CUR);
//            offset += -9;
            ++offset;
        }
    }
    fs.closeSync(fd);
//console.log("actual VBR");
    return cb(Math.round(1000 * duration) / 1000); //return sec, but make it accurate to msec
}


function estimateDuration(filesize, bitrate, offset)
{
//console.log("EST CBR");
    var kbps = (bitrate * 1000) / 8;
    var datasize = /*fs.statSync(filename).*/ filesize - offset;
    return Math.round(1000 * datasize / kbps) / 1000; //return sec, but make it accurate to msec
}


function skipID3v2Tag(block)
{
    var data = block.slice(0, 3).toString('utf8');
    if (data == "ID3")
    {
//        var id3v2_major_version = block[3]; //.charCodeAt(3);
//        var id3v2_minor_version = block[4]; //.charCodeAt(4);
        var id3v2_flags = block[5]; //.charCodeAt(5);
//        var flag_unsynchronisation  = (id3v2_flags & 0x80)? 1: 0;
//        var flag_extended_header = (id3v2_flags & 0x40)? 1: 0;
//        var flag_experimental_ind = (id3v2_flags & 0x20)? 1: 0;
        var flag_footer_present = (id3v2_flags & 0x10)? 1: 0;
        var z0 = block[6]; //.charCodeAt(6);
        var z1 = block[7]; //.charCodeAt(7);
        var z2 = block[8]; //.charCodeAt(8);
        var z3 = block[9]; //.charCodeAt(9);
        if (!(z0 & 0x80) && !(z1 & 0x80) && !(z2 & 0x80) && !(z3 & 0x80))
        {
            var header_size = 10;
            var tag_size = ((z0 & 0x7f) * 2097152) + ((z1 & 0x7f) * 16384) + ((z2 & 0x7f) * 128) + (z3 & 0x7f);
            var footer_size = flag_footer_present? 10: 0;
            return header_size + tag_size + footer_size; //bytes to skip
        }
    }
    return 0;
}


function parseFrameHeader(fourbytes)
{
    const versions = {0x0: '2.5', 0x1: 'x', 0x2: '2', 0x3: '1', }; // x=>'reserved'
    const layers = {0x0: 'x', 0x1: '3', 0x2: '2', 0x3: '1', }; // x=>'reserved'
    const bitrates =
    {
        'V1L1': [0, 32, 64, 96, 128, 160, 192, 224, 256, 288,320,352,384,416,448],
        'V1L2': [0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384],
        'V1L3': [0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320],
        'V2L1': [0,32,48,56, 64, 80, 96,112,128,144,160,176,192,224,256],
        'V2L2': [0, 8,16,24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160],
        'V2L3': [0, 8,16,24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160],
    };
    const sample_rates =
    {
        '1': [44100,48000,32000],
        '2': [22050,24000,16000],
        '2.5': [11025,12000, 8000],
    };
    const samples =
    {
        1: {1: 384, 2: 1152, 3: 1152, }, //MPEGv1,     Layers 1,2,3
        2: { 1: 384, 2: 1152, 3: 576, }, //MPEGv2/2.5, Layers 1,2,3
    };
    //var b0 = fourbytes[0]; //.charCodeAt(0); //will always be 0xff
    var b1 = fourbytes[1]; //.charCodeAt(1);
    var b2 = fourbytes[2]; //.charCodeAt(2);
    var b3 = fourbytes[3]; //.charCodeAt(3);

    var version_bits = (b1 & 0x18) >> 3;
    var version = versions[version_bits];
    var simple_version = (version == '2.5')? 2: version;

    var layer_bits = (b1 & 0x06) >> 1;
    var layer = layers[layer_bits];

//    var protection_bit = (b1 & 0x01);
    var bitrate_key = 'V' + simple_version + 'L' + layer; //sprintf('V%dL%d', simple_version , layer);
    var bitrate_idx = (b2 & 0xf0) >> 4;
    var bitrate = bitrates[bitrate_key][bitrate_idx] || 0;

    var sample_rate_idx = (b2 & 0x0c) >> 2; //0xc => b1100
    var sample_rate = sample_rates[version][sample_rate_idx] || 0;
    var padding_bit = (b2 & 0x02) >> 1;
//    var private_bit = (b2 & 0x01);
//    var channel_mode_bits = (b3 & 0xc0) >> 6;
//    var mode_extension_bits = (b3 & 0x30) >> 4;
//    var copyright_bit = (b3 & 0x08) >> 3;
//    var original_bit = (b3 & 0x04) >> 2;
//    var emphasis = (b3 & 0x03);

    var info = {};
    info.Version = version; //MPEGVersion
    info.Layer = layer;
    //$info['Protection Bit'] = $protection_bit; //0=> protected by 2 byte CRC, 1=>not protected
    info.Bitrate = bitrate;
    info['Sampling Rate'] = sample_rate;
    //$info['Padding Bit'] = $padding_bit;
    //$info['Private Bit'] = $private_bit;
    //$info['Channel Mode'] = $channel_mode_bits;
    //$info['Mode Extension'] = $mode_extension_bits;
    //$info['Copyright'] = $copyright_bit;
    //$info['Original'] = $original_bit;
    //$info['Emphasis'] = $emphasis;
    info.Framesize = framesize(layer, bitrate, sample_rate, padding_bit);
    info.Samples = samples[simple_version][layer];
    return info;
}

function framesize(layer, bitrate, sample_rate, padding_bit)
{
    var factor = (layer == 1)? [12, 4]: [144, 1]; //MPEG layer 1 vs. 2/3
    return Math.floor(((factor[0] * bitrate * 1000 / sample_rate) + padding_bit) * factor[1]);
}


//======================================================
//old tries:
//see http://stackoverflow.com/questions/383164/how-to-retrieve-duration-of-mp3-in-net/13269914#13269914

/*
        fs.createReadStream(filename)
//            .pipe(pool) //does this make much difference?
            .pipe(new lame.Decoder())
             .once('format', function (format)
            {
                console.log("raw_encoding: %d, sampleRate: %d, channels: %d, signed? %d, float? %d, ulaw? %d, alaw? %d, bitDepth: %d".cyan, format.raw_encoding, format.sampleRate, format.channels, format.signed, format.float, format.ulaw, format.alaw, format.bitDepth);
//            this.pipe(this_seq.speaker = new Speaker(format))
            })
            .on('data', function(data)
            {
                if (!ofs)
                {
                    if ((data.charCodeAt(0) == 0xff) && ((data.charCodeAt(1) & 0xe0) == 0xe0)) //11-bit sync
                    {
                        var info = parseFrameHeader(data.substr(0, 4));
                    }

                }
                ofs += data.length;

                total[inx] += data.length;
                console.log("data[%d]: %d", frames[inx]++, data.length);
            })
            .once('close', function() { console.log("close"); })
            .once('end', function()
            {
                console.log("end: len decoded: %d, chunks: %d", total[inx], frames[inx]);
            });
    });
*/

/*
        var audio = fs.createReadStream(filename);
        {
            Mp3Frame frame = Mp3Frame.LoadFromStream(fs);
            if (frame != null)
            {
                _sampleFrequency = (uint)frame.SampleRate;
            }
            while (frame != null)
            {
                if (frame.ChannelMode == ChannelMode.Mono)
                {
                    duration += (double)frame.SampleCount * 2.0 / (double)frame.SampleRate;
                }
                else
                {
                    duration += (double)frame.SampleCount * 4.0 / (double)frame.SampleRate;
                }
                frame = Mp3Frame.LoadFromStream(fs);
            }
        }
        return duration;
*/

//    fs.open(filename, 'r', function(err, fd)
//    {wstream.write(buffer);
//        if (err) { console.log(err.message); return; }
//        var buffer = new Buffer(100);
//        fs.read(fd, buffer, 0, 100, 0, function(err, num)
//        {
//            console.log(buffer.toString('utf8', 0, num));
//        });
//    });


/*
    fs.readFile(filename, 'utf8', function(err, chunk)
    {
        if (err) throw err;
        var data = chunk.toString('utf8');
        var duration = 0;
        console.log(typeof data);
        var block = data.substr(0, 100); //fread($fd, 100);
        var offset = skipID3v2Tag(block);
        var filesize = fs.statSync(filename).size;
//        fseek($fd, $offset, SEEK_SET);
//        while (!feof($fd))
*/


/*
    var sdata = [], dataLen = 0;
    fs.createReadStream(filename)
//            .pipe(pool) //does this make much difference?
//        .pipe(new lame.Decoder())
        .on('data', function(chunk)
        {
            sdata.push(chunk);
            dataLen += chunk.length;
        })
        .on('end', function()
        {
            var buf = new Buffer(dataLen);
            for (var i=0, len=sdata.length, pos=0; i<len; i++)
            {
                sdata[i].copy(buf, pos);
                pos += sdata[i].length;
            }

            var duration = 0;
            var data = buf.toString('utf8');
            console.log(typeof data, data.length, sdata.length, dataLen);
            var block = data.substr(0, 100); //fread($fd, 100);
console.log("buf[%d..] %d %d %d %d %d %d %d %d %d %d", offset, data.charCodeAt(offset), data.charCodeAt(offset+1), data.charCodeAt(offset+2), data.charCodeAt(offset+3), data.charCodeAt(offset+4), data.charCodeAt(offset+5), data.charCodeAt(offset+6), data.charCodeAt(offset+7), data.charCodeAt(offset+8), data.charCodeAt(offset+9));
            var offset = skipID3v2Tag(block);
            var filesize = dataLen; //fs.statSync(filename).size;
var look = 0;
            while (offset < filesize)
            {
if (++look < 10) console.log("buf[%d..] %d %d %d %d %d %d %d %d %d %d", offset, data.charCodeAt(offset), data.charCodeAt(offset+1), data.charCodeAt(offset+2), data.charCodeAt(offset+3), data.charCodeAt(offset+4), data.charCodeAt(offset+5), data.charCodeAt(offset+6), data.charCodeAt(offset+7), data.charCodeAt(offset+8), data.charCodeAt(offset+9));
                var block = data.substr(offset, 10); offset += 10; //fread($fd, 10);
                if (block.length < 10) break;
                //looking for 1111 1111 111 (frame synchronization bits)
                else if ((block.charCodeAt(0) == 0xff) && ((block.charCodeAt(1) & 0xe0) == 0xe0))
                {
                    console.log("hdr at ofs %d", offset - 10);
                    var info = parseFrameHeader(block.substr(0, 4));
                    if (!info.Framesize) return duration; //some corrupt mp3 files
                    if (use_cbr_estimate && info)
                    {
                        cb(estimateDuration(filesize, info.Bitrate, offset));
                        return;
                    }
//                fseek($fd, $info['Framesize']-10, SEEK_CUR);
                    offset += info.Framesize - 10;
                    duration += info.Samples / info['Sampling Rate'];
                }
                else if (block.substr(0, 3) == 'TAG')
                {
                    console.log("tag at ofs %d", offset - 10);
//                fseek($fd, 128-10, SEEK_CUR);//skip over id3v1 tag size
                    offset += 128 - 10;
                }
                else
                {
//                fseek($fd, -9, SEEK_CUR);
                    offset += -9;
                }
            }
            cb(Math.round(duration));
            return;
        });
*/


/*
    binread.open(filename)
        .on ("error", function (error)
        {
            console.error ("ERROR: ", error);
        })
        .on ("close", function ()
        {
            console.log("closed");
        })
        .read(100, function (rdlen, buffer) //, cb)
        {
debugger;
            var offset = 0; var data = buffer;
console.log("buf[%d..] %d %d %d %d %d %d %d %d %d %d", offset, data[offset], data[offset+1], data[offset+2], data[offset+3], data[offset+4], data[offset+5], data[offset+6], data[offset+7], data[offset+8], data[offset+9]);
            var offset = skipID3v2Tag(buffer);
            console.log("ID3 v2 tag ofs %d", offset);
            get_next(this);

            function get_next(reader)
            {
                reader
                    .seek(offset)
                    .read(10, function (rdlen, buffer)
                    {
debugger;
                        if (rdlen < 10) { this.cancel(); return cb(duration); }
                        var data = buffer.toString('utf8');
                        if ((buffer[0] == 0xff) && ((buffer[1] & 0xe0) == 0xe0))
                        {
                            console.log("hdr at ofs %d", offset);
                            var info = parseFrameHeader(buffer);
                            if (!info.Framesize)
                            {
                                console.log("corrupt file?");
                                return cb(duration); //some corrupt mp3 files
                            }
                            if (use_cbr_estimate && info)
                            {
                                return cb(estimateDuration(this.size(), info.Bitrate, offset));
                            }
                            offset += info.Framesize;
                            duration += info.Samples / info['Sampling Rate'];
                        }
                        else if (data.substr(0, 3) == 'TAG')
                        {
                            console.log("tag at ofs %d", offset);
//                fseek($fd, 128-10, SEEK_CUR);//skip over id3v1 tag size
                            offset += 128;
                        }
                        else
                        {
//                fseek($fd, -9, SEEK_CUR);
                            ++offset;
                        }
                    });
            }
        })
        .close();
*/


////////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions:
//

//return path relative to app dir (cwd); cuts down on verbosity
function relpath(abspath, basedir)
{//12.18.18
    const cwd = process.cwd(); //__dirname; //save it in case it changes later
//broken?    return path.relative(basedir || cwd, abspath);
//debug(homeify(abspath));
//debug(homeify(basedir || cwd));
//debug("relpath: basedir", basedir || cwd);
//debug("relpath: homeify(basedir)", homeify(basedir || cwd));
//debug("relpath: abspath", abspath);
//debug("relpath: homeify(abspath)", homeify(abspath));
//    const shortpath = pathlib.relative(homeify(basedir || cwd), homeify(abspath).grab(2)).grab(1);
//    const prefix = (grab[2] && (grab[1] != grab[2]))? "./": "";
    const shortpath = pathlib.relative(basedir || cwd, abspath);
//debug("relpath: relative(base, abs)", shortpath);
//debug(shortpath);
    const prefix = (abspath && (shortpath != abspath))? "./": "";
//debug("relpath: prefix", prefix);
//debug(grab[1], grab[2], prefix, shortpath);
//debug(prefix);
//    debug(`relpath: abs '${abspath}', base '${basedir || cwd}' => prefix '${prefix}' + shortpath '${shortpath}'`.blue_lt);
    return prefix + shortpath;
}


function exc_soft(args)
{//12.2.18
//    console.error.apply(console.error, arguments);
    console.error(args.red_lt);
}


function exc_hard(msg)
{//12.2.18
    exc_soft.apply(null, arguments);
//    quit(1);
    throw msg.red_lt;
}


function homeify(abspath)
{//12.16.18
    var homedir = os.homedir();
    if (homedir.slice(-1) != "/") homedir += "/";
//debug("home-1", abspath);
//debug("home-2", homedir);
//debug("home-3", abspath.indexOf(homedir));
//might have sym link so relax position check:
//        return !abspath.indexOf(homedir)? "~/" + abspath.slice(homedir.length): abspath;
    const ofs = abspath.indexOf(homedir);
    return ~ofs? "~/" + abspath.slice(ofs + homedir.length): abspath; //(ofs != -1); //~-1 == 0
}

function abspath(relpath, basedir)
{//12.12.18
//    const cwd = process.cwd(); //__dirname; //save it in case it changes later
    return pathlib.resolve(basedir || process.cwd(), relpath);
}


function timestamp()
{//11.20.18
    var timestamp = elapsed().toString(); //util.format("[%f] ", elapsed() / 1e3)
    if (timestamp.length < 4) timestamp = ("0000" + timestamp).slice(-4);
    timestamp = `${timestamp.slice(0, -3)}.${timestamp.slice(-3)}`;
    return timestamp;
}


/*
function quit(args)
{
    args = Array.from(arguments).unshift_fluent("quiting:".red_lt);
    debug.apply(debug, args);
    console.error.apply(console.error, args);
    process.exit(1);
}
*/


//make larger numbers more readable:
//CAUTION: converts number to string
function commas(num)
{//6.20.16
    return num.toLocaleString(); //grouping (1000s) default = true
}


//satisfy the grammar police:
function plural(ary, suffix)
{//10.10.18
//    if (!arguments.length) return plural.cached;
    const count = Array.isArray(ary)? (ary || []).length: ary;
    plural.cached = (count != 1)? (suffix || "s"): "";
    return ary;
}
function plurals(ary) { return plural(ary, "s"); }
function plurales(ary) { return plural(ary, "es"); }


//inline grabber:
//convenience function for reusing values without breaking up surrounding nested expr
function grab(thingy, label)
{//10.20.18
//debug("args", arguments);
    grab[label] = thingy;
//    debug(`grab[${label}] <- '${thingy}'`);
    return thingy; //fluent
};


//add custom props, methods to object:
//use this function in lieu of adding to Object.prototype (which is strongly discouraged)
function objext(obj)
{//12.17.18
    Object.defineProperties(obj, //default: !enumerable, !writable
    {
        forEach:
        {
            value: function objext_forEach(cb)
            {
                Object.keys(this).forEach((key, inx, all) => cb(obj[key], key, all));
            },
        },
    });
    return obj;
}


function extensions()
{//12.16.18
//array:
    Array.prototype.unshift_fluent = function(args) { this.unshift.apply(this, arguments); return this; };
    Array.prototype.push_fluent = function(args) { /*debug(this);*/ this.push.apply(this, arguments); return this; };
//    Object.defineProperties(Array.prototype,
//    {
//        drop_nulls: { get() { return this.reduce((keepers, arg) => arg && keepers.push(arg), keepers, []); }, }, //exclude empty values from final list
//    });
//string:
    String.prototype.grab = function(inx) { return grab(this, inx); }
//    String.prototype.
console.error("TODO: fix json_tidy big/hex#s".red_lt);
    Object.defineProperties(String.prototype,
    {
//            escnl: { get() { return this.replace(/\n/g, "\\n"); }}, //escape all newlines
//            nocomment: { get() { return this.replace(/\/\*.*?\*\//gm, "").replace(/(#|\/\/).*?$/gm, ""); }}, //strip comments; multi-line has priority over single-line; CAUTION: no parsing or overlapping
//            nonempty: { get() { return this.replace(/^\s*\r?\n/gm , ""); }}, //strip empty lines
//            escre: { get() { return this.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }}, //escape all RE special chars
        json_tidy: { get() { return this.replace(/,"/g, ", \"").replace(/"(.*?)":/g, "$1: "); }}, //.replace(/\d{5,}/g, (val) => `0x${(val >>> 0).toString(16)}`); }}, //tidy up JSON for readability
    });
}


////////////////////////////////////////////////////////////////////////////////
////
/// CLI/unit test/sample code:
//

if (!module.parent) //.pipe(process.stdout); //auto-run CLI, send generated output to console
{//12.18.18
debugger;
//set default options:
    const opts =
    {
        folder: true? process.cwd(): __dirname, //if (basedir)
        get pattern() { return pathlib.join(this.folder, "*.mp3"); }, //"*.mp3"
//    seqname: "*.seq",
        autoplay: true,
        repeat: 2,
        gap: 0.5, //time between songs (sec)
//    async: false,
    };
//    ((i == 2)? shebang_args(process.argv[i]): [process.argv[i]]).forEach((arg, inx, all) => //shebang might also have args (need to split and strip comments)
//    opts = Object.assign({}, OPTS, (typeof opts == "object")? opts: {[`${default_props[typeof opts] || "filename"}`]: opts}); //shallow copy, convert to obj
//map single unnamed arg to named options:
//    let THIS; //kludge: not accessible during init
/*
    const files = process.argv
//no        .slice(2) //drop node + my source file name
//        .map((arg, inx, all) => all[(inx + 2) % all.length]) //move "node" and self to end to trigger end-of-list processing
        .map((arg, inx, all) =>
        {
//            THIS = all;
//console.log("here1", inx);
            if (!arg.match(/^[+\-]|=/)) return arg; //use filename as-is
            const [lhs, rhs] = arg.split("=", 1);
            opts[lhs.slice(!rhs)] = rhs || (lhs[0] == "+"); //set numeric or true/false option; CAUTION: "0" or "" will give "false"
            return null; //remove options from list of filenames
        }, {}) //provide container object for local data
        .map((item, inx, all))
        {
            (item === null)
        }
        .push_fluent(push_fluent.apply(THIS, glob.sync(opts.pattern)) //pattern, {}); //, function (err, files)
        .reduce((keepers, arg) => arg && keepers.push(arg), []) //exclude empty values from list (those were taken as options)
*/
    const files = process.argv
        .reduce((retval, arg, inx, all) =>
        {
            let parts;
            const SIGN = 1, NAME = 2, VALUE = 4;
            if (inx < 2); //drop node + self
            else if ((parts = arg.match(/^([+-])?([^=]+)(=(.+))?$/)) && (parts[SIGN] || (parts[VALUE] || "").length)) //save + drop options; need non-greedy name to allow optional "=" to match
                opts[parts[NAME].toLowerCase()] = (parts[VALUE] || "").length? parts[VALUE]: (parts[SIGN] != "-"); //set numeric or true/false option; CAUTION: "0" or "" will give "false"
//            else if (arg.match(/^[+\-]|=/)) //save + drop options
//            {
//                const [lhs, rhs] = arg.split("=", 1);
//                opts[lhs.replace(/^[+\-]/, "")] = rhs || (lhs[0] == "+"); //set numeric or true/false option; CAUTION: "0" or "" will give "false"
//            }
            else retval.push(abspath(arg)); //convert to abs path
//CAUTION: all of above cases should flow thru here (not short-circuit return) in order to search for files if none specified
//if (inx == all.length - 1) debug("glob", glob.sync(opts.pattern));
            if ((inx == all.length - 1) && !retval.length) //search for files if none specified on command line
                retval.push.apply(retval, glob.sync(opts.pattern)); //.map((file) => abspath(pathlib.dirname(opts.pattern), file))); //convert to abs path
            return retval;
        }, []);
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
//debugger;
    process.argv.forEach((arg, i, all) => debug(`argv[${i}/${all.length}]: '${arg}'`.blue_lt));
    objext(opts).forEach((opt, key) => debug(`opts[${key}]: '${opt}'`.yellow_lt));
//    files.forEach((file, i, all) => debug(`files[${i}/${all.length}]: '${relpath(file)}'`.cyan_lt));
//    debug(`options: ${JSON.stringify(opts, null, 2).json_tidy}`.blue_lt);
//    debug(`${files.length} file(s): \n${files.map((file, inx, all) => `${inx + 1}/${all.length}: '${relpath(file)}'`).join(", \n")}`.blue_lt);
    console.error(`${plural(files.length)} matching file${plural.cached}:`.yellow_lt); //match '${relpath(pattern)}'`.yellow_lt);
    files.forEach((file, inx, all) => console.error(`  file[${inx}/${all.length}]: '${relpath(file)}'`.yellow_lt));
//    files.forEach((file) => mp3player(file));
//debugger;
//debug(relpath(files[0]));
//process.exit();
    if (files.length) playlist(files, opts);
}

//eof