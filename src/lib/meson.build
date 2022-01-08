lib_sources = files()
subdir('buzhash')
subdir('comp')
subdir('hash')
subdir('index')
subdir('dl')
lib_sources += files('zck.c', 'header.c', 'io.c', 'log.c', 'compint.c', 'error.c')

extra_c_args = []
if host_machine.system() != 'windows'
    extra_c_args += ['-fvisibility=hidden']
else
    lib_sources += files(['win32/regerror.c', 'win32/regcomp.c', 'win32/regexec.c', 'win32/tre-mem.c'])
    extra_c_args += ['-DZCHUNK_EXPORTS=1']
endif

zcklib = library('zck',
                        lib_sources,
                        # in meson 0.48, use `gnu_symbol_visibility: 'hidden'` kwarg
                        c_args: extra_c_args,
                        include_directories: inc,
                        dependencies: [zstd_dep, openssl_dep],
                        install: true,
                        version: meson.project_version(),
                        soversion: so_version)
