#!/bin/bash
# Build zchunk-rocky:8 if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-rocky-8:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/rocky-8/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-rocky-8-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-rocky-8-test zchunk-rocky-8:test
RETVAL=$?
docker rm zchunk-rocky-8-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-rocky-8:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
