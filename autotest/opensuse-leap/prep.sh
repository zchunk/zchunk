#!/bin/bash
docker pull opensuse/leap
if [ "$?" -ne 0 ]; then
        exit 1
fi
docker image rm -f zchunk-opensuse:latest 2>/dev/null 1>/dev/null
docker image build -t zchunk-opensuse:latest --file autotest/opensuse-leap/prep/Dockerfile ./
