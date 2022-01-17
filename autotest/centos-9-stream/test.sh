#!/bin/bash
# Build zchunk-centos:9 if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-centos-9:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/centos-9-stream/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-centos-9-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-centos-9-test zchunk-centos-9:test
RETVAL=$?
docker rm zchunk-centos-9-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-centos-9:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
