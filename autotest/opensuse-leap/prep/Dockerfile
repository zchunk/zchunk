FROM opensuse/leap
RUN zypper --non-interactive install meson gcc pkgconfig "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)" && rm -rf /var/cache/zypp
