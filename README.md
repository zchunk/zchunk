# zchunk

[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/16509.svg)](https://scan.coverity.com/projects/zchunk-zchunk)<br>
[![Jenkins Build Status](https://jenkins.zchunk.net/buildStatus/icon?job=zchunk)](https://jenkins.zchunk.net)

zchunk is a compressed file format that splits the file into independent chunks.
This allows you to only download changed chunks when downloading a new version
of the file, and also makes zchunk files efficient over rsync.

zchunk files are protected with strong checksums to verify that the file you
downloaded is, in fact, the file you wanted.

**As of zchunk-1.0, the ABI and API have been marked stable, and the only changes
allowed are backwards-compatible additions**

## Installation
To build and install zchunk, first install meson and run
```
meson build
cd build
ninja
ninja test
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

## Documentation
- [Format definition](zchunk_format.txt)
- [Initial announcement](https://www.jdieter.net/posts/2018/04/30/introducing-zchunk)
- [How zchunk works (with pretty pictures)](https://www.jdieter.net/posts/2018/05/31/what-is-zchunk)
