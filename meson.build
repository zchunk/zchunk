project(
    'zck', 'c',
    version : '1.1.16',
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
    zstd_dep = dependency('zstd', modules: 'zstd::libzstd_shared', required : get_option('with-zstd') == 'enabled')
    if zstd_dep.found()
        add_project_arguments('-DZCHUNK_ZSTD', language : 'c')
        if zstd_dep.version().version_compare('<=1.4.9')
            add_project_arguments('-DOLD_ZSTD', language : 'c')
        endif
    endif
endif

curl_dep = dependency('curl', modules: 'CURL::libcurl')

if build_machine.system() != 'windows'
    if build_machine.system() == 'darwin' or build_machine.system() == 'freebsd' or not cc.links('#include <argp.h>\nstatic error_t parse_opt (int key, char *arg, struct argp_state *state) { argp_usage(state); return 0; }; void main() {}')
       argplib = cc.find_library('argp')
    else
       argplib = dependency('', required : false)
    endif
endif

if get_option('with-openssl') == 'disabled'
    openssl_dep = dependency('', required : false)
else
    openssl_dep = dependency('openssl', modules : ['OpenSSL::SSL', 'OpenSSL::Crypto'], required : get_option('with-openssl') == 'enabled')
    if openssl_dep.found()
        add_project_arguments('-DZCHUNK_OPENSSL', language : 'c')
    endif
endif

inc = []
inc += include_directories('include')
if host_machine.system() == 'windows'
    inc += include_directories('src/lib/win32')
endif

subdir('include')
subdir('src')
if not get_option('coverity') and host_machine.system() != 'windows'
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
