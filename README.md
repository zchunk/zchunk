# zchunk

[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/16509.svg)](https://scan.coverity.com/projects/zchunk-zchunk)[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fzchunk%2Fzchunk.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fzchunk%2Fzchunk?ref=badge_shield)
<br>
[![GitHub Actions Test Status](https://github.com/zchunk/zchunk/actions/workflows/main.yml/badge.svg)](https://github.com/zchunk/zchunk/actions)

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

If you're building on an operating system where some libraries are stored in
/usr/local, you'll need to replace `meson build` above as follows:
```
CFLAGS=-I/usr/local/include CXXFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/lib meson build
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


## Zchunk dictionaries

By default, each chunk in a zchunk file is compressed independently.  However,
if you're creating a zchunk file that has any repetitive data, you may
be able to reduce the overall file size by using a [zstd dictionary](https://facebook.github.io/zstd/#small-data).
The dictionary takes up extra space at the beginning of the zchunk file, but is
used as an identical initial dictionary for compressing each chunk, which can
give a significant overall savings.

It is important that all further revisions of the zchunk file use the same
dictionary.  If the dictionary changes, none of the chunks will match from the
old file, and the full new file will be downloaded.

Zchunk can use any zstd dictionary, but also includes a utility to generate the
ideal zstd dictionary for a zchunk file.

To create an ideal dictionary for a zchunk file, run:
```
zck_gen_zdict <file.zck>
```

The dictionary will be saved as `<file.zdict>`.

You will then need to recompress the file with the dictionary:
```
zck -D <uncompressed file>
```

Note that `zck_gen_zdict` does require that the `zstd` binary be installed on
your system.


## Documentation
- [Format definition](zchunk_format.txt)
- [Initial announcement](https://www.jdieter.net/posts/2018/04/30/introducing-zchunk)
- [How zchunk works (with pretty pictures)](https://www.jdieter.net/posts/2018/05/31/what-is-zchunk)


## License
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fzchunk%2Fzchunk.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fzchunk%2Fzchunk?ref=badge_large)
