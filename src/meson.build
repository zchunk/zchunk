subdir('lib')
zck = executable(
    'zck',
    ['zck.c', 'util_common.c'],
    include_directories: inc,
    link_with: zcklib,
    install: true
)
unzck = executable(
    'unzck',
    ['unzck.c', 'util_common.c'],
    include_directories: inc,
    link_with: zcklib,
    install: true
)
zckdl = executable(
    'zckdl',
    ['zck_dl.c', 'util_common.c'],
    include_directories: inc,
    dependencies: curl_dep,
    link_with: zcklib,
    install: true
)
zck_read_header = executable(
    'zck_read_header',
    ['zck_read_header.c', 'util_common.c'],
    include_directories: inc,
    link_with: zcklib,
    install: true
)
zck_delta_size = executable(
    'zck_delta_size',
    ['zck_delta_size.c', 'util_common.c', 'memmem.c'],
    include_directories: inc,
    link_with: zcklib,
    install: true
)
