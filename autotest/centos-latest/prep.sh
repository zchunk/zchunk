#!/bin/sh
docker pull centos:latest
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-centos:latest 2>/dev/null 1>/dev/null
docker image build -t zchunk-centos:latest --file autotest/centos-latest/prep/Dockerfile ./
