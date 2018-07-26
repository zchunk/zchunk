util_sources = []
subdir('lib')
incdir = include_directories(['lib', '../src/lib', '../include'])
empty = executable('empty', ['empty.c'] + util_sources, include_directories: incdir, dependencies: [zstd_dep, openssl_dep])
shacheck = find_program('shacheck.sh')
file_path = join_paths(meson.source_root(), 'test/files')

test(
    'create and validate empty zchunk file',
    empty
)
test(
    'decompress and validate automatically chunked zchunk file with no dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.nodict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.nodict.fodt.zck')
    ]
)
test(
    'decompress and validate automatically chunked zchunk file with dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.dict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.dict.fodt.zck')
    ]
)
test(
    'decompress and validate manually chunked zchunk file with no dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.manual.nodict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.manual.nodict.fodt.zck')
    ]
)
test(
    'decompress and validate manually chunked zchunk file with dict',
    shacheck,
    args: [
        unzck,
        'LICENSE.manual.dict.fodt',
        '394ed6c2fc4ac47e5ee111a46f2a35b8010a56c7747748216f52105e868d5a3e',
        join_paths(file_path, 'LICENSE.manual.dict.fodt.zck')
    ]
)
test(
    'compress and validate automatically chunked zchunk file with no dict',
    shacheck,
    args: [
        zck,
        'LICENSE.nodict.fodt.zck',
        'ad9c6c972113615c9b28e67d01b8fa646a7090426f48349a0a53ab8a83457175',
        '-o', 'LICENSE.nodict.fodt.zck',
        join_paths(file_path, 'LICENSE.fodt')
    ]
)
test(
    'compress and validate automatically chunked zchunk file with dict',
    shacheck,
    args: [
        zck,
        'LICENSE.dict.fodt.zck',
        '3fc106134f6f08d25db6f2e46b32b67035333cb3362c29c54e26adb010545712',
        '-D', join_paths(file_path, 'LICENSE.dict'),
        '-o', 'LICENSE.dict.fodt.zck',
        join_paths(file_path, 'LICENSE.fodt')
    ]
)
test(
    'compress and validate manually chunked zchunk file with no dict',
    shacheck,
    args: [
        zck,
        'LICENSE.manual.nodict.fodt.zck',
        'cba69c5fc88fda4352470f3eb0fe5df04e9f1f08c88e0af5a96e53ed3f84526c',
        '-m',
        '-s', '<text:',
        '-o', 'LICENSE.manual.nodict.fodt.zck',
        join_paths(file_path, 'LICENSE.fodt')
    ]
)
test(
    'compress and validate manually chunked zchunk file with dict',
    shacheck,
    args: [
        zck,
        'LICENSE.manual.dict.fodt.zck',
        'fd726deb04f1e1cdaf53cb8629ef4c121bb1f11f8388983f638a62610168d079',
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
