#!/bin/bash
# Build zchunk-alpine:edge if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-alpine:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/alpine-edge/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-alpine-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-alpine-test zchunk-alpine:test
RETVAL=$?
docker rm zchunk-alpine-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-alpine:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
