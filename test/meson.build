util_sources = []
subdir('lib')
incdir = include_directories(['lib', '../src/lib', '../include'])
empty = executable('empty', ['empty.c'] + util_sources,
                   include_directories: incdir,
                   dependencies: [zstd_dep, openssl_dep])
optelems = executable('optelems', ['optelems.c'] + util_sources,
                     include_directories: incdir,
                     dependencies: [zstd_dep, openssl_dep])
copy_chunks = executable('copy_chunks', ['copy_chunks.c'] + util_sources,
                     include_directories: incdir,
                     dependencies: [zstd_dep, openssl_dep])

invalid_input_checksum = executable('invalid_input_checksum',
                                    ['invalid_input_checksum.c'] + util_sources,
                                    include_directories: incdir,
                                    dependencies: [zstd_dep, openssl_dep])
read_single_chunk = executable('read_single_chunk',
                               ['read_single_chunk.c'] + util_sources,
                               include_directories: incdir,
                               dependencies: [zstd_dep, openssl_dep])
read_single_comp_chunk = executable('read_single_comp_chunk',
                                    ['read_single_comp_chunk.c'] + util_sources,
                                    include_directories: incdir,
                                    dependencies: [zstd_dep, openssl_dep])
shacheck = executable('shacheck', ['shacheck.c'] + util_sources, include_directories: incdir, dependencies: [zstd_dep, openssl_dep])
file_path = join_paths(meson.source_root(), 'test/files')

test(
    'create and validate empty zchunk file',
    empty,
    is_parallel: false
)
test(
    'check version info in zck',
    zck,
    args: [
        '-V'
    ]
)
test(
    'check version info in unzck',
    unzck,
    args: [
        '-V'
    ]
)
test(
    'check version info in zckdl',
    zckdl,
    args: [
        '-V'
    ]
)
test(
    'check version info in zck_read_header',
    zck_read_header,
    args: [
        '-V'
    ]
)
test(
    'check version info in zck_delta_size',
    zck_delta_size,
    args: [
        '-V'
    ]
)

test(
    'check opening file with optional flags',
    optelems,
    args: [
        join_paths(file_path, 'empty.optelems.zck')
    ]
)

test(
    'checksum with non-hex character',
    invalid_input_checksum,
    args: [
        join_paths(file_path, 'empty.optelems.zck')
    ]
)

test(
    'read single chunk',
    read_single_chunk,
    args: [
        join_paths(file_path, 'LICENSE.dict.fodt.zck')
    ]
)

test(
    'read single compressed chunk',
    read_single_comp_chunk,
    args: [
        join_paths(file_path, 'LICENSE.dict.fodt.zck')
    ]
)

test(
    'check verbosity in unzck',
    unzck,
    args: [
        '-vvvvv',
        'empty.zck'
    ],
    is_parallel: false
)
test(
    'check verbosity in zck',
    zck,
    args: [
        '-vvvvv',
        'empty'
    ],
    is_parallel: false
)
test(
    'check verbosity in zckdl',
    zckdl,
    args: [
        '-vvvvv',
        'file:///empty'
    ],
    should_fail: true
)
test(
    'check verbosity in zck_read_header',
    zck_read_header,
    args: [
        '-vvvvvfc',
        'empty.zck'
    ]
)
test(
    'check verbosity in zck_delta_size',
    zck_delta_size,
    args: [
        '-vvvvv',
        'empty.zck',
        'empty.zck'
    ]
)
test(
    'copy chunks from source',
    copy_chunks,
    args: [
        join_paths(file_path, 'LICENSE.header.new.nodict.fodt.zck'),
        join_paths(file_path, 'LICENSE.nodict.fodt.zck')
    ]
)
test(
    'decompress auto-chunked file - nocomp',
    shacheck,
    args: [
        unzck,
        'LICENSE.nocomp.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.nocomp.fodt.zck')
    ]
)
test(
    'decompress auto-chunked file - no dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.nodict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.nodict.fodt.zck')
    ]
)
test(
    'decompress auto-chunked file - dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.dict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.dict.fodt.zck')
    ]
)
test(
    'decompress manual file - no dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.manual.nodict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.manual.nodict.fodt.zck')
    ]
)
test(
    'decompress manual file - dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.manual.dict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.manual.dict.fodt.zck')
    ]
)
test(
    'decompress dict from auto-chunked file',
    shacheck,
    args: [
        unzck,
        'LICENSE.dict.fodt.zdict',
        'b20064d89c3beb8605d99c994ff45304f308abd840c0981475dd2faca6ec854b',
        '--dict',
        join_paths(file_path, 'LICENSE.dict.fodt.zck')
    ]
)

if build_machine.endian() != 'big'
    check_sha = 'eff3098803ba80f0c446d49f48188f89167d7f29cdc8a98c19f0ecfb4e2ee3c9'
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.6')
        check_sha = '45e48c11fea129d2c434ffcec7d8fbc1720f30f33d438654cd117616121c218e'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.5')
        check_sha = '45e48c11fea129d2c434ffcec7d8fbc1720f30f33d438654cd117616121c218e'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.4')
        check_sha = '1749c2551ef26f26a2c61919dac7a4c8801154cdfc0524ef21187cf0c263f8fe'
    endif

    test(
        'compress auto-chunked file - no dict',
        shacheck,
        args: [
            zck,
            'LICENSE.nodict.fodt.zck',
            check_sha,
            '-o', 'LICENSE.nodict.fodt.zck',
            join_paths(file_path, 'LICENSE.fodt')
        ]
    )

    check_sha = 'b86795ca14eb04b382d1c7f94501aa5d1a2ddb05a5351c0235d00edb954e9b66'
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.6')
        check_sha = 'b4805798547be62421944a87db3a90de455b4772c6013e685720b49336b8b17e'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.4')
        check_sha = '0fb0f2262c62714063e409117fcbd4c114b35e3abbfc90b042a41246b62ff789'
    endif
    test(
        'compress auto-chunked file - dict',
        shacheck,
        args: [
            zck,
            'LICENSE.dict.fodt.zck',
            check_sha,
            '-D', join_paths(file_path, 'LICENSE.dict'),
            '-o', 'LICENSE.dict.fodt.zck',
            join_paths(file_path, 'LICENSE.fodt')
        ]
    )
endif

check_sha = 'c46929367cd3d05daaca3b44657726791b428fb2198f5e7e5367b5cc781307aa'
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.6')
    check_sha = '9472ddc74dbd96291077bbb8d793e605824be8dba45e3e94bbc54b7de21a37a1'
endif
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.5')
    check_sha = '6911b50cad5b6ed3ee89322338c559afdd0e72f614d3817b47ce370df52fd6b4'
endif
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.4')
    check_sha = 'ded0d04884b13ba23d00f9b11b2b5950f8b9670d742f67d4195707ed85e73048'
endif
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.3')
    check_sha = '437a0ec28def0c7f361402f6bb337cb963e6a3682bc742dd9e56171f6ece9881'
endif
test(
    'compress manual file - no dict',
    shacheck,
    args: [
        zck,
        'LICENSE.manual.nodict.fodt.zck',
        check_sha,
        '-m',
        '-s', '<text:',
        '-o', 'LICENSE.manual.nodict.fodt.zck',
        join_paths(file_path, 'LICENSE.fodt')
    ]
)

check_sha = '5fc2449aeb51f6a898529fa4d53b04de6df6a352826f819dc2b39b153ddaa788'
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.6')
    check_sha = '4e86b47410a3a5090c7d60040d24d8d390c31f16f96ea112c8f153888eaf8c6e'
endif
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.5')
    check_sha = '62ee66fbf41a1f18617f400383eb38905050050f0f435a6e56df4a30d1bb694d'
endif
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.4')
    check_sha = 'd5670c6ab81bdfa9b21eb21486a1cfa691a211fd83018914ea99ed9bdf509266'
endif
if zstd_dep.found() and zstd_dep.version().version_compare('<=1.3.3')
    check_sha = '71e6e770545df9bae2560d683d876d25df5e7f697ad20f9535b907d76db09876'
endif
test(
    'compress manual file - dict',
    shacheck,
    args: [
        zck,
        'LICENSE.manual.dict.fodt.zck',
        check_sha,
        '-D', join_paths(file_path, 'LICENSE.dict'),
        '-m',
        '-s', '<text:',
        '-o', 'LICENSE.manual.dict.fodt.zck',
        join_paths(file_path, 'LICENSE.fodt')
    ]
)
run_target('test-abi',
           command: 'abi.sh')

run_target('new-abi',
           command: 'new_abi.sh')
