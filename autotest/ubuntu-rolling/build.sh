#!/bin/bash
# Build zchunk-ubuntu:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-ubuntu:latest)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/ubuntu-rolling/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-ubuntu:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-ubuntu:test --file autotest/ubuntu-rolling/build/Dockerfile ./

