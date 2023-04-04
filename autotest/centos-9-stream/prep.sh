#!/bin/bash
docker pull quay.io/centos/centos:stream9
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-centos:9 2>/dev/null 1>/dev/null
docker image build -t zchunk-centos:9 --file autotest/centos-9-stream/prep/Dockerfile ./
