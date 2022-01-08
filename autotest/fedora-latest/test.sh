#!/bin/bash
# Build zchunk-fedora:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-fedora:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/fedora-latest/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-fedora-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-fedora-test zchunk-fedora:test
RETVAL=$?
docker rm zchunk-fedora-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-fedora:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
