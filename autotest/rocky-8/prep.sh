#!/bin/bash
docker pull docker.io/library/rockylinux:8
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-rocky:8 2>/dev/null 1>/dev/null
docker image build -t zchunk-rocky:8 --file autotest/rocky-8/prep/Dockerfile ./
