FROM opensuse/leap
ADD ./ /code
WORKDIR /code
RUN zypper --non-interactive install meson gcc pkgconfig "pkgconfig(libzstd)" "pkgconfig(libcurl)" "pkgconfig(openssl)"
RUN meson build
RUN /bin/bash -c 'cd build; ninja'
RUN /bin/bash -c 'cd build; ninja test'
