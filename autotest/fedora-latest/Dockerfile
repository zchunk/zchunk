FROM fedora:latest
ADD ./ /code
WORKDIR /code
RUN dnf -y install meson gcc "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)"
RUN meson build
RUN /bin/bash -c 'cd build; ninja'
RUN /bin/bash -c 'cd build; ninja test'
