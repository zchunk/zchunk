lib_sources = files()
add_global_arguments('-fvisibility=hidden', language : 'c')
subdir('comp')
subdir('hash')
subdir('index')
subdir('dl')
lib_sources += files('zck.c', 'header.c', 'io.c', 'log.c', 'compint.c')
zcklib = shared_library('zck',
                        lib_sources,
                        include_directories: inc,
                        dependencies: [zstd_dep],
                        install: true,
                        version: lib_version,
                        soversion: so_version)
