util_sources = []
subdir('lib')
incdir = include_directories(['lib', '../src/lib', '../include'])
empty = executable('empty', ['empty.c'] + util_sources, include_directories: incdir, dependencies: [zstd_dep, curl_dep, openssl_dep])
file_path = join_paths(meson.source_root(), 'test/files')

test('create and validate empty zchunk file', empty)

run_target('test-abi',
           command: 'abi.sh')

run_target('new-abi',
           command: 'new_abi.sh')
