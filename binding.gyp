#for node-gyp info see https://gyp.gsrc.io/docs/UserDocumentation.md
#this serves a similar purpose to Makefile, I guess
{
    'targets':
    [
        {
            'target_name': "ws281X-gpu",
            'sources': [ "src/ws281x-gpu.cpp"],
            'cflags+': ["-w", "-g", "-DNODEJS_ADDON", "-Wall", "-std=c++11"],
            'include_dirs+' : ["<!(node -e \"require('nan')\")"],
            'libraries+': ["-lGLESv2", "-lEGL", "-lm"],
            'OTHER_LDFLAGS+': ['-stdlib=libc++'],
#            'dependencies': [ 'deps/mpg123/mpg123.gyp:output' ],
            'conditions':
            [
                [
#                    'OS=="linux-rpi"',
#                    '<!@(uname -p)=="armv7l"', #RPi 2
                    'target_arch=="arm"', #RPi 2
                    {
                        'defines': ["RPI_NO_X"], #don't want X Windows client
                        'libraries+': ["-L/opt/vc/lib", "-lbcm_host"],
                        'include_dirs+':
                        [
                            "/opt/vc/include",
                            "/opt/vc/include/interface/vcos/pthreads",
                            "/opt/vc/include/interface/vmcs_host/linux"
                        ]
                    },
#                    'OS=="linux-pc"', #else
                    {
#                        "xcode_settings": {
#                        'defines': ["UNIV_LEN=32"], #for dev/testing
#                        'defines': ["SHADER_DEBUG"], #for dev/testing
                        'libraries': ["-lX11"]
                    }
                ]
            ]
        }
    ]
}
#eof
