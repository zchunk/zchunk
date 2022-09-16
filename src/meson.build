subdir('lib')

preprocessor_defines = []
if host_machine.system() == 'windows'
    extra_win_src = ['lib/win32/basename.c', 'lib/win32/ftruncate.c']
    if get_option('default_library') == 'static'
        preprocessor_defines = '-DZCHUNK_STATIC_LIB=1'
    endif
else
    extra_win_src = []
endif

zck = executable(
    'zck',
    ['zck.c', 'util_common.c', 'memmem.c'] + extra_win_src,
    include_directories: inc,
    dependencies: argplib,
    link_with: zcklib,
    install: true,
    c_args: preprocessor_defines
)
unzck = executable(
    'unzck',
    ['unzck.c', 'util_common.c'] + extra_win_src,
    include_directories: inc,
    dependencies: argplib,
    link_with: zcklib,
    install: true,
    c_args: preprocessor_defines
)
zck_gen_zdict = executable(
    'zck_gen_zdict',
    ['zck_gen_zdict.c', 'util_common.c'] + extra_win_src,
    include_directories: inc,
    dependencies: argplib,
    link_with: zcklib,
    install: true,
    c_args: preprocessor_defines
)
zck_read_header = executable(
    'zck_read_header',
    ['zck_read_header.c', 'util_common.c'] + extra_win_src,
    include_directories: inc,
    dependencies: argplib,
    link_with: zcklib,
    install: true,
    c_args: preprocessor_defines
)
zck_delta_size = executable(
    'zck_delta_size',
    ['zck_delta_size.c', 'util_common.c', 'memmem.c'] + extra_win_src,
    include_directories: inc,
    dependencies: argplib,
    link_with: zcklib,
    install: true,
    c_args: preprocessor_defines
)
zckdl = executable(
    'zckdl',
    ['zck_dl.c', 'util_common.c'] + extra_win_src,
    include_directories: inc,
    dependencies: [argplib, curl_dep],
    link_with: zcklib,
    install: true,
    c_args: preprocessor_defines
)
