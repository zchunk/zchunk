#!/bin/sh
cd "$(dirname "$0")"

if [ ! -e ../build/src/lib/libzck.so.*.*.* ]; then
    echo "No library exists"
    exit 1
fi

# Remove stable build and copy current build abi/stable
rm abi/stable -rf
mkdir -p abi/stable
cp -a ../build/src/lib/libzck.so.* abi/stable
cp -a ../include/zck.h abi/stable
find abi -type l -delete
