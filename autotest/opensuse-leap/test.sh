#!/bin/sh
# Build zchunk-opensuse:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-opensuse:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/opensuse-leap/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-opensuse-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-opensuse-test zchunk-opensuse:test
RETVAL=$?
docker rm zchunk-opensuse-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-opensuse:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
