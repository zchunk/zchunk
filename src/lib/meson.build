lib_sources = files()
subdir('buzhash')
subdir('comp')
subdir('hash')
subdir('index')
subdir('dl')
lib_sources += files('zck.c', 'header.c', 'io.c', 'log.c', 'compint.c', 'error.c')
zcklib = library('zck',
                        lib_sources,
                        # in meson 0.48, use `gnu_symbol_visibility: 'hidden'` kwarg
                        c_args: ['-fvisibility=hidden'],
                        include_directories: inc,
                        dependencies: [zstd_dep, openssl_dep],
                        install: true,
                        version: meson.project_version(),
                        soversion: so_version)
