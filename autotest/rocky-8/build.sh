#!/bin/bash
# Build zchunk-rocky:8 if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-rocky:8)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/rocky-8/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-rocky-8:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-rocky-8:test --file autotest/rocky-8/build/Dockerfile ./
