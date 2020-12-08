#!/bin/sh
cd "$(dirname "$0")"

rm -rf ../build/src/lib/libzck.so.*.*.*.p 2>/dev/null

if [ ! -e ../build/src/lib/libzck.so.*.*.* ]; then
    echo "No library exists"
    exit 1
fi

# Copy latest build to abi/new and remove softlinks
rm abi/new -rf
mkdir -p abi/new
cp -a ../build/src/lib/libzck.so.* abi/new
cp -a ../build/include/zck.h abi/new
find abi -type l -delete

# Redump abi/stable
cd abi/stable
abi-dumper libzck.so.* -lver `cat zck.h | grep ZCK_VERSION | head -n 1 | awk '{ print $3 }' | sed s/\"//g` -public-headers `pwd abi/stable`
if [ "$?" -ne 0 ]; then
    exit 1
fi

# Dump abi/new
cd ../new
abi-dumper libzck.so.* -lver `cat zck.h | grep ZCK_VERSION | head -n 1 | awk '{ print $3 }' | sed s/\"//g` -public-headers `pwd abi/new`
if [ "$?" -ne 0 ]; then
    exit 1
fi
cd ../../

# Remove any old reports and generate new abi compliance report
rm compat_reports -rf
abi-compliance-checker -l zchunk -old abi/stable/ABI.dump -new abi/new/ABI.dump
exit $?
