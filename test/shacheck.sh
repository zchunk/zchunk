#!/bin/sh
CMD="$1"
EOUTPUT="$2"
ECHKSUM="$3"
shift 3
"$CMD" $@

if [ "$?" -ne 0 ]; then
    exit 1
fi

CHKSUM=$(sha256sum "$EOUTPUT" | awk '{ print $1 }')
if [ "$CHKSUM" != "$ECHKSUM" ]; then
    echo "Calculated checksum: $CHKSUM"
    echo "Expected checksum: $ECHKSUM"
    echo "Checksum doesn't match"
    exit 1
fi

rm "$EOUTPUT" -f
exit 0
