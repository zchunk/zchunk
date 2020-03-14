#!/bin/sh
# Build zchunk-alpine:edge if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-alpine:edge)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/alpine-edge/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-alpine:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-alpine:test --file autotest/alpine-edge/build/Dockerfile ./

