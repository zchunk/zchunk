FROM centos:latest
RUN yum -y install epel-release && yum -y update epel-release && yum -y install meson gcc "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)" && rm -rf /var/cache/yum
