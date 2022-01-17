#!/bin/bash
docker pull docker.io/tgagor/centos:stream8
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-centos:8 2>/dev/null 1>/dev/null
docker image build -t zchunk-centos:8 --file autotest/centos-8-stream/prep/Dockerfile ./
