FROM docker.io/tgagor/centos:stream9
RUN dnf -y install "dnf-command(config-manager)" && dnf -y config-manager --set-enabled crb && dnf -y install meson gcc "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)" && rm -rf /var/cache/yum
