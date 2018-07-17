FROM debian:latest
ADD ./ /code
WORKDIR /code
RUN apt-get update
RUN /bin/bash -c 'export DEBIAN_FRONTEND=noninteractive; apt-get -yq install meson gcc pkg-config libzstd-dev libcurl4-openssl-dev libssl-dev'
RUN meson build
RUN /bin/bash -c 'cd build; ninja'
RUN /bin/bash -c 'cd build; ninja test'
