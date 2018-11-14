#!/bin/sh
# Build zchunk-debian:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-debian:latest)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/debian-latest/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-debian:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-debian:test --file autotest/debian-latest/build/Dockerfile ./

