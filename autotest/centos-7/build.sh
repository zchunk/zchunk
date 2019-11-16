#!/bin/sh
# Build zchunk-centos:7 if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-centos:7)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/centos-7/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-centos:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-centos:test --file autotest/centos-7/build/Dockerfile ./

