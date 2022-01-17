project(
    'zck', 'c',
    version : '1.1.16',
    meson_version : '>=0.53.0',
    default_options : ['c_std=gnu99']
)

so_version = '1'

common_cflags = [
    '-Wunused-result',
]
fs = import('fs')
cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(common_cflags), language : 'c')

if build_machine.system() == 'freebsd'
    add_project_arguments('-DFREEBSD', language : 'c')
endif

# zstd dependency
if build_machine.system() == 'windows'
    zstd_dep = dependency('zstd', modules: 'zstd::libzstd_shared', required : get_option('with-zstd'), fallback: ['zstd', 'libzstd_dep'])
else
    zstd_dep = dependency('libzstd', required : get_option('with-zstd'), fallback: ['zstd', 'libzstd_dep'],
                          default_options: ['bin_programs=false', 'zlib=disabled', 'lzma=disabled', 'lz4=disabled'])
endif
if zstd_dep.found()
    add_project_arguments('-DZCHUNK_ZSTD', language : 'c')
    if zstd_dep.version().version_compare('<=1.4.9')
        add_project_arguments('-DOLD_ZSTD', language : 'c')
    endif
endif

# curl dependency
if build_machine.system() == 'windows'
    curl_dep = dependency('curl', modules : 'CURL::libcurl', required : get_option('with-curl'), disabler : true)
else
    curl_dep = dependency('libcurl', required : get_option('with-curl'), disabler : true)
endif

# openssl dependency
if build_machine.system() == 'windows'
    openssl_dep = dependency('openssl', modules : ['OpenSSL::SSL', 'OpenSSL::Crypto'], required : get_option('with-openssl'))
else
    openssl_dep = dependency('openssl', required : get_option('with-openssl'))
endif
if openssl_dep.found()
    add_project_arguments('-DZCHUNK_OPENSSL', language : 'c')
endif

# includes
inc = []
inc += include_directories('include')
if host_machine.system() == 'windows'
    inc += include_directories('src/lib/win32')
endif

# argp-standalone dependency (if required)
if build_machine.system() == 'windows' or build_machine.system() == 'darwin' or build_machine.system() == 'freebsd' or not cc.links('#include <argp.h>\nstatic error_t parse_opt (int key, char *arg, struct argp_state *state) { argp_usage(state); return 0; }; void main() {}')
    if fs.is_dir(join_paths([get_option('prefix'), 'include']))
        inc += include_directories(join_paths([get_option('prefix'), 'include']))
    endif
    argplib = cc.find_library('argp', dirs : join_paths([get_option('prefix'), 'lib']))
else
    argplib = dependency('', required : false)
endif

subdir('include')
subdir('src')
if not get_option('coverity')
    subdir('test')
endif

pkg_mod = import('pkgconfig')
pkg_mod.generate(
    zcklib,
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

libzck_dep = declare_dependency(link_with: zcklib, include_directories: 'include')
