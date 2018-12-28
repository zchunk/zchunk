project('zck', 'c',
        version : '1.0.2',
        meson_version : '>=0.44.0',
        default_options : ['c_std=gnu99'])

so_version = '1'

common_cflags = [
  '-Wunused-result',
]
cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(common_cflags), language : 'c')

# Fix coverity bug, shamelessly stolen from libinput
if get_option('coverity')
    add_project_arguments('-D_Float128=__uint128_t', language : 'c')
    add_project_arguments('-D_Float32x=int', language : 'c')
    add_project_arguments('-D_Float32=int', language : 'c')
    add_project_arguments('-D_Float64x=long', language : 'c')
    add_project_arguments('-D_Float64=long', language : 'c')
endif

if get_option('with-zstd') == 'disabled'
    zstd_dep = dependency('', required : false)
else
    zstd_dep = dependency('libzstd', required : get_option('with-zstd') == 'enabled')
    if zstd_dep.found()
        add_project_arguments('-DZCHUNK_ZSTD', language : 'c')
    endif
endif

curl_dep = dependency('libcurl')

if get_option('with-openssl') == 'disabled'
    openssl_dep = dependency('', required : false)
else
    openssl_dep = dependency('openssl', required : get_option('with-openssl') == 'enabled')
    if openssl_dep.found()
        add_project_arguments('-DZCHUNK_OPENSSL', language : 'c')
    endif
endif

inc = include_directories('include')

subdir('include')
subdir('src')
if not get_option('coverity')
    subdir('test')
endif

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : zcklib,
                 version : meson.project_version(),
                 name : 'libzck',
                 filebase : 'zck',
                 description : 'A library for generating easy-to-delta files.')
