util_sources = []
subdir('lib')
incdir = [include_directories(['lib', '../src/lib', '../include'])]
if build_machine.system() == 'windows'
    incdir += include_directories('../src/lib/win32')
endif

if host_machine.system() == 'windows'
    preprocessor_defines = '-DZCHUNK_STATIC_LIB=1'
    win_basename = ['../src/lib/win32/basename.c']
else
    preprocessor_defines = []
    win_basename = []
endif


empty = executable('empty', ['empty.c'] + util_sources,
                   include_directories: incdir,
                   dependencies: [zstd_dep, openssl_dep],
                   c_args: preprocessor_defines)
optelems = executable('optelems', ['optelems.c'] + util_sources,
                     include_directories: incdir,
                     dependencies: [zstd_dep, openssl_dep],
                     c_args: preprocessor_defines)
copy_chunks = executable('copy_chunks', ['copy_chunks.c'] + win_basename + util_sources,
                     include_directories: incdir,
                     dependencies: [zstd_dep, openssl_dep],
                     c_args: preprocessor_defines)

invalid_input_checksum = executable('invalid_input_checksum',
                                    ['invalid_input_checksum.c'] + util_sources,
                                    include_directories: incdir,
                                    dependencies: [zstd_dep, openssl_dep],
                                    c_args: preprocessor_defines)
read_single_chunk = executable('read_single_chunk',
                               ['read_single_chunk.c'] + util_sources,
                               include_directories: incdir,
                               dependencies: [zstd_dep, openssl_dep],
                               c_args: preprocessor_defines)
read_single_comp_chunk = executable('read_single_comp_chunk',
                                    ['read_single_comp_chunk.c'] + util_sources,
                                    include_directories: incdir,
                                    dependencies: [zstd_dep, openssl_dep],
                                    c_args: preprocessor_defines)
shacheck = executable('shacheck', 
                      ['shacheck.c'] + util_sources,
                      include_directories: incdir,
                      dependencies: [zstd_dep, openssl_dep],
                      c_args: preprocessor_defines)
exitcodecheck = executable('exitcodecheck',
                      ['exitcodecheck.c'] + util_sources,
                      include_directories: incdir,
                      dependencies: [zstd_dep, openssl_dep],
                      c_args: preprocessor_defines)
zck_cmp_uncomp = executable(
    'zck_cmp_uncomp',
    ['zck_cmp_uncomp.c'],
    include_directories: inc + incdir,
    dependencies: argplib,
    link_with: zcklib,
    c_args: preprocessor_defines,
    install: false
)

if meson.version().version_compare('>=0.56.0')
    file_path = join_paths(meson.project_source_root(), 'test/files')
else
    file_path = join_paths(meson.source_root(), 'test/files')
endif

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
    'decompress previously generated auto-chunked file - nocomp',
    shacheck,
    args: [
        unzck,
        'LICENSE.nocomp.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.nocomp.fodt.zck')
    ]
)
test(
    'decompress previously generated auto-chunked file - no dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.nodict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.nodict.fodt.zck')
    ]
)
test(
    'decompress previously generated auto-chunked file - dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.dict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.dict.fodt.zck')
    ]
)
test(
    'decompress previously generated manual file - no dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.manual.nodict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.manual.nodict.fodt.zck')
    ]
)
test(
    'decompress previously generated manual file - dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.manual.dict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.manual.dict.fodt.zck')
    ]
)
test(
    'decompress dict from previously generated auto-chunked file',
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
    check_sha = '7412757c744bcb17a112fedcbec6914d307bc4d2ab5936a2b6908a64a5c8c0ec'
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.3')
        check_sha = 'ac9d431f1f568d8921257fa17929c320f0cdcbff450d9f3c38c78c4f410ee788'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.0')
        check_sha = '4f07f865bb15624cf854aa369e14a3538ad9e9bf98e233036d37d2568e60b7cc'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.10')
        check_sha = 'c8c14ae369c341753e634b94fe1d071d3551f2b69469c2196e6dc657d613b975'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.9')
        check_sha = 'eff3098803ba80f0c446d49f48188f89167d7f29cdc8a98c19f0ecfb4e2ee3c9'
    endif
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
    test(
        'decompress auto-chunked file - no dict',
        shacheck,
        args: [
            unzck,
            'LICENSE.nodict.fodt',
            '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
            'LICENSE.nodict.fodt.zck'
        ],
        is_parallel: false
    )


    check_sha = '25669f40abd98cc6a010173ad9891960ef56f12fad2ba95acb642ca11a98e96f'
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.3')
        check_sha = '35549b28b01c0a43d7389d4c40d5b750c73fb6c0c6701355ef709ef4a37df389'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.0')
        check_sha = '11d08d01c7877d51b84a0f97ebf651ca3304de10cdf207223df2dbc3295fa532'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.9')
        check_sha = 'b86795ca14eb04b382d1c7f94501aa5d1a2ddb05a5351c0235d00edb954e9b66'
    endif
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
    test(
        'decompress auto-chunked file - dict',
        shacheck,
        args: [
            unzck,
            'LICENSE.dict.fodt',
            '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
            'LICENSE.dict.fodt.zck'
        ],
        is_parallel: false
    )

    check_sha = 'f9057c70daf878ec93b86156ccabb4ce4549d409fe0446b4f88a0f88f916956b'
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.3')
        check_sha = '2580f66412adce1ed8d92abc09b0f64eb155155f61e21a8269a2c4a338857e51'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.0')
        check_sha = 'db42e903a2cf3b859835298272ff45bff6d2435d32fe7fda2bfe2815ab161994'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.9')
        check_sha = 'c46929367cd3d05daaca3b44657726791b428fb2198f5e7e5367b5cc781307aa'
    endif
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
    if host_machine.system() == 'windows'
        split_at = '^<text:'
    else
        split_at = '<text:'
    endif
    test(
        'compress manual file - no dict',
        shacheck,
        args: [
            zck,
            'LICENSE.manual.nodict.fodt.zck',
            check_sha,
            '-m',
            '-s', split_at,
            '-o', 'LICENSE.manual.nodict.fodt.zck',
            join_paths(file_path, 'LICENSE.fodt')
        ]
    )
    test(
        'decompress manual file - no dict',
        shacheck,
        args: [
            unzck,
            'LICENSE.manual.nodict.fodt',
            '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
            'LICENSE.manual.nodict.fodt.zck'
        ],
        is_parallel: false
    )

    check_sha = 'ec068277d5277b2c7c3c45e25f5b23a0e7ef56fd4c0463db28255642850379ab'
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.3')
        check_sha = '863dc2bda721a32c26485eeb2223d5dabcf9ac517ecba8c7991fa0120d04c937'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.5.0')
        check_sha = 'ee8df850bc4797e6696b966a74a459129b0f6d23dfb720d1cc8cba6f2b209dd2'
    endif
    if zstd_dep.found() and zstd_dep.version().version_compare('<=1.4.9')
        check_sha = '5fc2449aeb51f6a898529fa4d53b04de6df6a352826f819dc2b39b153ddaa788'
    endif
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
            '-s', split_at,
            '-o', 'LICENSE.manual.dict.fodt.zck',
            join_paths(file_path, 'LICENSE.fodt')
        ]
    )
    test(
        'decompress manual file - dict',
        shacheck,
        args: [
            unzck,
            'LICENSE.manual.dict.fodt',
            '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
            'LICENSE.manual.dict.fodt.zck'
        ],
        is_parallel: false
    )
    test(
        'handles integer overflow in header',
        exitcodecheck,
        args: [
            zck_read_header,
            '1',
            '-v',
            join_paths(file_path, 'integer_overflow_header.zck')
        ]
    )
    test(
        'handles empty file',
        exitcodecheck,
        args: [
            unzck,
            '1',
            '-v',
            join_paths(file_path, 'zerolen.zck')
        ]
    )
    test(
        'handles compressed int that\'s too large - variant 1',
        exitcodecheck,
        args: [
            unzck,
            '1',
            '-v',
            join_paths(file_path, 'compressed_int_too_large1.zck')
        ]
    )
    test(
        'handles compressed int that\'s too large - variant 2',
        exitcodecheck,
        args: [
            unzck,
            '1',
            '-v',
            join_paths(file_path, 'compressed_int_too_large2.zck')
        ]
    )
    test(
        'handles compressed int that\'s too large - variant 3',
        exitcodecheck,
        args: [
            unzck,
            '1',
            '-v',
            join_paths(file_path, 'compressed_int_too_large3.zck')
        ]
    )
    test(
        'handles compressed int that\'s too large - variant 4',
        exitcodecheck,
        args: [
            unzck,
            '1',
            '-v',
            join_paths(file_path, 'compressed_int_too_large4.zck')
        ]
    )
    test(
        'handles compressed int that\'s too large - variant 5',
        exitcodecheck,
        args: [
            unzck,
            '1',
            '-v',
            join_paths(file_path, 'compressed_int_too_large5.zck')
        ]
    )
endif

run_target('test-abi',
           command: 'abi.sh')

run_target('new-abi',
           command: 'new_abi.sh')
