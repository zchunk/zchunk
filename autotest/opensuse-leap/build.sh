#!/bin/bash
# Build zchunk-opensuse:latest if it doesn't exist
HAVE_IMAGE=$(docker image ls -q zchunk-opensuse:latest)
if [ "$HAVE_IMAGE" == "" ]; then
	autotest/opensuse-leap/prep.sh
	if [ "$?" -ne 0 ]; then
		exit 1
	fi
fi
docker image rm zchunk-opensuse:test -f 2>/dev/null 1>/dev/null
docker image build -t zchunk-opensuse:test --file autotest/opensuse-leap/build/Dockerfile ./

