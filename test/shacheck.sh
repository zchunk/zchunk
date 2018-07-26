#!/bin/sh
"$1" "$2"
if [ "$?" -ne 0 ]; then
    exit 1
fi

ls -l

CHKSUM=$(sha256sum "$3" | awk '{ print $1 }')
if [ "$CHKSUM" != "$4" ]; then
    echo "Checksum doesn't match"
    exit 1
fi
