FROM debian:latest
RUN /bin/bash -c 'echo deb http://ftp.debian.org/debian stretch-backports main >> /etc/apt/sources.list && apt-get update && export DEBIAN_FRONTEND=noninteractive && apt-get -yqt stretch-backports install meson libzstd-dev && apt-get -yq install gcc pkg-config libcurl4-openssl-dev libssl-dev'
