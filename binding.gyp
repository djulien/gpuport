#node-gyp uses this file to generate a Makefile
#NOTE: node-gyp reads all uncommented entries
#NOTE: after changing this file, delete build folder to remove cached info
#NOTE: nvm uses an internal copy of node.gyp; https://github.com/nodejs/node-gyp/wiki/Updating-npm%27s-bundled-node-gyp
#NOTE?: cflags needs node.gyp ver >= 3.6.2; see https://gist.github.com/TooTallNate/1590684
#to upgrade node.gyp:
#  [sudo] npm explore npm -g -- npm install node-gyp@latest
#for node-gyp info see https://gyp.gsrc.io/docs/UserDocumentation.md
#for bindings.gyp file see https://gyp.gsrc.io/docs/InputFormatReference.md
#keys ending with "=" will completely replace old value
#keys ending with "?" will be used only if key doesn't already have a value
#keys ending with "+" will prepend to previous value
#undecorated keys will append to previous value
#example files: https://github.com/nodejs/node-gyp/wiki/%22binding.gyp%22-files-out-in-the-wild
{
    'targets':
    [
        {
            "target_name": "gpuport",
#            'type': '<(library)',
#            'type': 'executable',
            "sources": ["src/GpuPort.cpp"],
#            "cflags_cc!": [ "-fno-exceptions" ],
#CAUTION: cflags requires node.gyp ver >= 3.6.2 ??
#use "npm install --verbose" to verify these options are being passed correctly to compiler:
            "cflags":
            [
                "-g", #include debug symbols
                "-O3", #optimization
#                "-O0", #no optimization
                "-std=c++14",
                "-fPIC", "-pthread", "-fno-omit-frame-pointer", "-fno-rtti", 
                "-Wall", "-Wextra", "-Wno-unused-parameter", "-w", "-Wall", "-pedantic", "-Wvariadic-macros",
                "-fexceptions", #"-fno-exceptions",
            ],
#CAUTION: check common.gypi for whether to use cflags or cflags_cc
#path ~/.node-gyp/*/include/node/common.gypi/.node-gyp/*/include/node/common.gypi
            "cflags_cc!": #conflict with above; turn off these options (in case common.gypi turns them on)
            [
                "-fno-exceptions",
                "-std=gnu++1y",
            ],
#NOTE?: node-gyp only reads *one* "cflags_cc+" here, otherwise node-gyp ignores it
#            'cflags_cc': ["-w", "-Wall", "-pedantic", "-Wvariadic-macros", "-g", "-std=c++11"], #-std=c++0x
#            'cflags+': ["-w", "-g", "-DNODEJS_ADDON", "-Wall", "-std=c++11"],
#            'cflags+': ["-w", "-g", "-Wall", "-std=c++11"],
            'include_dirs':
            [
#                "<!@(node -p \"require('nan')\")",
#                "<!(node -e \"require('nan')\")",
#               'include_dirs+' : ["<!(node -e \"require('nan')\")"],
#                "<!@(node -p \"require('node-addon-api').include\")"
#                "<!@(node -p \"require('node-addon-api').include + '/src'\")"
#               'include_dirs+': [" <!(sdl2-config --cflags)"], #CAUTION: need "+" and leading space here
                " <!(sdl2-config --cflags)", #CAUTION: need leading space here
            ],
#            'OTHER_LDFLAGS+': ['-stdlib=libc++'],
            'libraries':
            [
                " <!@(sdl2-config --libs)", #-lGL
#                " -L'<!(pwd)'",
#                "<(module_root_dir)/build/Release/",
            ],
            'dependencies':
            [
#                "<!(node -p \"require('node-addon-api').gyp\")",
#            'dependencies': [ 'deps/mpg123/mpg123.gyp:output' ],
#            "dependencies": [ "<!(node -p \"require('node-addon-api').gyp\")" ],
#TODO:                'deps/sdl2/sdl2.gyp:output',
#                "<!(node -p \"console.log('add SDL2 compile');\")",
            ],
            'defines':
            [
#?                'NAPI_DISABLE_CPP_EXCEPTIONS',
                'BUILT="<!(date +\"%F %T\")"',
            ],
            'conditions':
            [
                [
#                    'OS=="linux-rpi"',
#                    '<!@(uname -p)=="armv7l"', #RPi 2
                    'target_arch=="arm"', #RPi 2
                    {
                        'defines': ["RPI_NO_X"], #don't want X Windows client
#                        'libraries+': ["-L/opt/vc/lib", "-lbcm_host"],
#                        'include_dirs+':
#                        [
#                            "/opt/vc/include",
#                            "/opt/vc/include/interface/vcos/pthreads",
#                            "/opt/vc/include/interface/vmcs_host/linux",
#                        ],
                    },
#                    'OS=="linux-pc"', #else
                    {
#                        "xcode_settings": {
#                        'defines': ["UNIV_LEN=32"], #for dev/testing
#                        'defines': ["SHADER_DEBUG"], #for dev/testing
#                        'libraries': ["-lX11"]
                        'libraries': ["-lX11", "-lXxf86vm"], #, "-lXext", "-lXxF86vm"] #for dev/debug only
#            'libraries+': ["-lGLESv2", "-lEGL", "-lm"],
                    },
                ],
            ],
        },
    ],
}
#eof