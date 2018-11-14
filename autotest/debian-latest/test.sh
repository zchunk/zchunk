#!/bin/sh
# Build zchunk-debian:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-debian:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/debian-latest/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-debian-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-debian-test zchunk-debian:test
RETVAL=$?
docker rm zchunk-debian-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-debian:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
