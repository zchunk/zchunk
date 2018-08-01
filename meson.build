project('zck', 'c',
        version : '0.9.5',
        meson_version : '>=0.47.0',
        default_options : ['c_std=gnu99'])

so_version = '1'

common_cflags = [
  '-Wunused-result',
]
cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(common_cflags), language : 'c')

zstd_dep = dependency('libzstd', required : get_option('with-zstd'))
if zstd_dep.found()
    add_project_arguments('-DZCHUNK_ZSTD', language : 'c')
endif

curl_dep = dependency('libcurl')

openssl_dep = dependency('openssl', required : get_option('with-openssl'))
if openssl_dep.found()
    add_project_arguments('-DZCHUNK_OPENSSL', language : 'c')
endif

inc = include_directories('include')

subdir('include')
subdir('src')
subdir('test')

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : zcklib,
                 version : meson.project_version(),
                 name : 'libzck',
                 filebase : 'zck',
                 description : 'A library for generating easy-to-delta files.')
