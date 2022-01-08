#!/bin/bash
docker pull centos:7
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-centos:7 2>/dev/null 1>/dev/null
docker image build -t zchunk-centos:7 --file autotest/centos-7/prep/Dockerfile ./
