#!/bin/sh
docker pull ubuntu:rolling
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-ubuntu:latest 2>/dev/null 1>/dev/null
docker image build -t zchunk-ubuntu:latest --file autotest/ubuntu-rolling/prep/Dockerfile ./
