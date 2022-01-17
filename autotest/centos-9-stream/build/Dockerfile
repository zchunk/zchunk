FROM zchunk-centos:9
ADD ./ /code
WORKDIR /code
RUN meson build --auto-features=enabled && cd build && ninja-build
WORKDIR /code/build
CMD ninja-build test
