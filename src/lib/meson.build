sources = []
subdir('comp')
subdir('hash')
subdir('index')
subdir('dl')
sources += ['zck.c', 'header.c', 'io.c', 'log.c', 'compint.c']
zcklib = shared_library('zck',
                        sources,
                        include_directories: inc,
                        dependencies: [zstd_dep, curl_dep],
                        install: true,
                        version: lib_version,
                        soversion: so_version)
