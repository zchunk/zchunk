#!/bin/sh
# Build zchunk-centos:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-centos:latest)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/centos-latest/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-centos:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-centos:test --file autotest/centos-latest/build/Dockerfile ./

