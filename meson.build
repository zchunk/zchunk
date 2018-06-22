project('zck', 'c', version: '0.7.3')

so_version = '1'

use_zstd = get_option('with-zstd')
zstd_dep = []
if use_zstd == 'yes'
    zstd_required = true
else
    zstd_required = false
endif
if use_zstd == 'auto' or use_zstd == 'yes'
    zstd_dep = dependency('libzstd', required:zstd_required)
    if zstd_dep.found()
        add_project_arguments('-DZCHUNK_ZSTD', language : 'c')
        use_zstd = 'yes'
    endif
endif
curl_dep = dependency('libcurl', required:true)

inc = include_directories('include')

subdir('include')
subdir('src')
subdir('test')

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries: zcklib,
                 version: meson.project_version(),
                 name: 'libzck',
                 filebase: 'zck',
                 description: 'A library for generating easy-to-delta files.')
