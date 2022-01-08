#!/bin/bash
# Build zchunk-centos:7 if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-centos:test)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/centos-7/build.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker rm zchunk-centos-test -f 2>/dev/null 1>/dev/null
docker run --name zchunk-centos-test zchunk-centos:test
RETVAL=$?
docker rm zchunk-centos-test -f 2>/dev/null 1>/dev/null
docker image rm zchunk-centos:test -f 2>/dev/null 1>/dev/null
exit $RETVAL
