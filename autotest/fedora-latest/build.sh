#!/bin/sh
# Build zchunk-fedora:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-fedora:latest)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/fedora-latest/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-fedora:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-fedora:test --file autotest/fedora-latest/build/Dockerfile ./

