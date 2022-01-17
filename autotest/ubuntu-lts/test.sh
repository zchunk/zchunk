#!/bin/bash
# Build zchunk-ubuntu-lts:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-ubuntu-lts:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/ubuntu-lts/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-ubuntu-lts-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-ubuntu-lts-test zchunk-ubuntu-lts:test
RETVAL=$?
docker rm zchunk-ubuntu-lts-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-ubuntu-lts:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
