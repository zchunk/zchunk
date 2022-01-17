#!/bin/bash
docker pull ubuntu:latest
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-ubuntu-lts:latest 2>/dev/null 1>/dev/null
docker image build -t zchunk-ubuntu-lts:latest --file autotest/ubuntu-lts/prep/Dockerfile ./
