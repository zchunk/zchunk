#!/bin/bash
# Build zchunk-ubuntu-lts:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-ubuntu-lts:latest)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/ubuntu-lts/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-ubuntu-lts:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-ubuntu-lts:test --file autotest/ubuntu-lts/build/Dockerfile ./

