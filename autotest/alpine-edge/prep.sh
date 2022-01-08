#!/bin/bash
docker pull alpine:edge
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-alpine:edge 2>/dev/null 1>/dev/null
docker image build -t zchunk-alpine:edge --file autotest/alpine-edge/prep/Dockerfile ./
