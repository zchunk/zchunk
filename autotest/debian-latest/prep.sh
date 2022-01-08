#!/bin/bash
docker pull debian:latest
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-debian:latest 2>/dev/null 1>/dev/null
docker image build -t zchunk-debian:latest --file autotest/debian-latest/prep/Dockerfile ./
