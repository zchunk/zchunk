FROM fedora:latest
ADD ./ /code
WORKDIR /code
RUN dnf -y install meson gcc "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)"
RUN meson build --auto-features=enabled
RUN ninja -C build test
