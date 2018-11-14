#!/bin/sh
# Build zchunk-ubuntu:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-ubuntu:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/ubuntu-rolling/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-ubuntu-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-ubuntu-test zchunk-ubuntu:test
RETVAL=$?
docker rm zchunk-ubuntu-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-ubuntu:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
