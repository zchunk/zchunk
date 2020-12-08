#!/bin/sh
cd "$(dirname "$0")"

rm -rf ../build/src/lib/libzck.so.*.*.*.p 2>/dev/null

if [ ! -e ../build/src/lib/libzck.so.*.*.* ]; then
    echo "No library exists"
    exit 1
fi

# Remove stable build and copy current build abi/stable
rm abi/stable -rf
mkdir -p abi/stable
cp -a ../build/src/lib/libzck.so.* abi/stable
cp -a ../build/include/zck.h abi/stable
find abi -type l -delete
