FROM docker.io/library/rockylinux:8
RUN sed -i "s/enabled=0/enabled=1/" /etc/yum.repos.d/Rocky-PowerTools.repo && dnf -y install meson gcc "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)" && rm -rf /var/cache/yum
