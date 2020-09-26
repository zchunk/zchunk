project(
    'zck', 'c',
    version : '1.1.7',
    meson_version : '>=0.44.0',
    default_options : ['c_std=gnu99']
)

so_version = '1'

common_cflags = [
    '-Wunused-result',
]
cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(common_cflags), language : 'c')

if build_machine.system() == 'freebsd'
    add_project_arguments('-DFREEBSD', language : 'c')
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

if build_machine.system() == 'freebsd' or not cc.links('#include <argp.h>\nstatic error_t parse_opt (int key, char *arg, struct argp_state *state) { argp_usage(state); }; void main() {}')
    argplib = cc.find_library('argp')
else
    argplib = dependency('', required : false)
endif

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
pkg_mod.generate(
    libraries : zcklib,
    version : meson.project_version(),
    name : 'libzck',
    filebase : 'zck',
    description : 'A library for generating easy-to-delta files.'
)

install_man([
    'doc/unzck.1',
    'doc/zck.1',
    'doc/zck_delta_size.1',
    'doc/zck_gen_zdict.1',
    'doc/zck_read_header.1',
    'doc/zckdl.1',
])
