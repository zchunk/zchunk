#!/bin/sh
docker pull fedora:latest
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-fedora:latest 2>/dev/null 1>/dev/null
docker image build -t zchunk-fedora:latest --file autotest/fedora-latest/prep/Dockerfile ./
