FROM zchunk-centos:8
ADD ./ /code
WORKDIR /code
RUN meson build --auto-features=enabled && cd build && ninja-build
WORKDIR /code/build
CMD ninja-build test
