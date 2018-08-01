util_sources = []
subdir('lib')
incdir = include_directories(['lib', '../src/lib', '../include'])
empty = executable('empty', ['empty.c'] + util_sources, include_directories: incdir, dependencies: [zstd_dep, openssl_dep])
shacheck = find_program('shacheck.sh')
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

check_sha = '0418aaca75b6b64c3ac9bc50fc0974e48c76691869977907fad25eea834f3c85'
if zstd_dep.version().version_compare('<=1.3.4')
    check_sha = '08c9ce94470ad4ab7f8a64e67872e138964eb562d13686d9c941baa3a09d2835'
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

check_sha = 'fedf75caa19cc03383b2a2abac75a490360f9bab6419c9f9241bae584fe64f04'
if zstd_dep.version().version_compare('<=1.3.4')
    check_sha = '0512327cc771a73c23377dd3804fffcaa557dd03cfd980dae22100f7b2193255'
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

check_sha = 'cba69c5fc88fda4352470f3eb0fe5df04e9f1f08c88e0af5a96e53ed3f84526c'
if zstd_dep.version().version_compare('<=1.3.4')
    check_sha = '53205d490819bbb681224e21acf0b85ec44c62c5c1f46e59bd084ac471ed534c'
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

check_sha = '1c30587c95a7be32e282272e713f72dd944b446dc7a9250adf8d90e2d524ed13'
if zstd_dep.version().version_compare('<=1.3.4')
    check_sha = 'a02d64a7cdbfe07af674b4aa94707d2dd072f8ff1abd0bd9801d313019e69cba'
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
