lib_sources += files('hash.c')
if openssl_dep.found()
    subdir('openssl')
else
    subdir('bundled')
endif
