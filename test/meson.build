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
        '08c9ce94470ad4ab7f8a64e67872e138964eb562d13686d9c941baa3a09d2835',
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
        'e9b5bcee24965ed6bf8e60c3537c2e1558912bd851cfe0b6e47a44d12bd54cf5',
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
        '53205d490819bbb681224e21acf0b85ec44c62c5c1f46e59bd084ac471ed534c',
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
        '035a58cb3b46c0326f91b193f3e040b9cc9f031c09477b79fd80d7ffbe75e9c4',
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
