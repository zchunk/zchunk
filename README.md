# zchunk

zchunk is a compressed file format that splits the file into independent chunks.
This allows you to only download changed chunks when downloading a new version
of the file, and also makes zchunk files efficient over rsync.

zchunk files are protected with strong checksums to verify that the file you
downloaded is, in fact, the file you wanted.

**Please note that, while the code is pretty reliable and the file format
shouldn't see any further changes, the API is still not fixed.  Please do not
use zchunk for any mission-critical systems yet.**


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
