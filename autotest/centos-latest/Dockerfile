FROM centos:latest
ADD ./ /code
WORKDIR /code
RUN yum -y install epel-release
RUN yum -y update epel-release
RUN yum -y install meson gcc "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)"
RUN meson build
RUN /bin/bash -c 'cd build; ninja-build'
RUN /bin/bash -c 'cd build; ninja-build test'
