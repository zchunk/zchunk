#!/bin/bash
# Build zchunk-centos:8 if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-centos:8)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/centos-8-stream/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-centos-8:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-centos-8:test --file autotest/centos-8-stream/build/Dockerfile ./
