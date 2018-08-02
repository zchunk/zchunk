FROM debian:latest
ADD ./ /code
WORKDIR /code
RUN /bin/bash -c 'echo deb http://ftp.debian.org/debian stretch-backports main >> /etc/apt/sources.list'
RUN apt-get update
RUN cat /etc/apt/sources.list
RUN /bin/bash -c 'export DEBIAN_FRONTEND=noninteractive; apt-get -yqt stretch-backports install meson libzstd-dev'
RUN /bin/bash -c 'export DEBIAN_FRONTEND=noninteractive; apt-get -yq install gcc pkg-config libcurl4-openssl-dev libssl-dev'
RUN meson build
RUN ninja -C build test
