# zchunk

[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/16509.svg)](https://scan.coverity.com/projects/zchunk-zchunk)

zchunk is a compressed file format that splits the file into independent chunks.
This allows you to only download changed chunks when downloading a new version
of the file, and also makes zchunk files efficient over rsync.

zchunk files are protected with strong checksums to verify that the file you
downloaded is, in fact, the file you wanted.

**zchunk-0.9.0 has been released with the proposed final ABI.  Once zchunk-1.0
has been released, the ABI will be marked as stable, and the only allowed
API/ABI and file format changes will be additions**


## Installation
To build and install zchunk, first install meson and run
```
meson build
cd build
ninja
sudo ninja install
```

## Using the utilities
To decompress a zchunk file, simply run:
```
unzck <filename>
```

To compress a new zchunk file, run:
```
zck <filename>
```

To download a zchunk file, run:
```
zckdl -s <source> <url of target>
```

To read a zchunk header, run:
```
zck_read_header <file>
```

## C API
\#TODO
