lib_sources = files()

if host_machine.system() == 'windows'
    lib_sources += files(['win32/regerror.c', 'win32/regcomp.c', 'win32/regexec.c', 'win32/tre-mem.c'])
    preprocessor_defines = '-DZCHUNK_EXPORTS=1'
else
    preprocessor_defines = ''
endif

if host_machine.system() != 'windows'
    add_global_arguments('-fvisibility=hidden', language : 'c')
endif
subdir('buzhash')
subdir('comp')
subdir('hash')
subdir('index')
subdir('dl')
lib_sources += files('zck.c', 'header.c', 'io.c', 'log.c', 'compint.c', 'error.c')


zcklib = library('zck',
                lib_sources,
                include_directories: inc,
                dependencies: [zstd_dep, openssl_dep],
                install: true,
                version: meson.project_version(),
                soversion: so_version,
                c_args: preprocessor_defines)
