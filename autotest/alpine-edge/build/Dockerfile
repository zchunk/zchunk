FROM zchunk-alpine:edge
ADD ./ /code
WORKDIR /code
RUN meson build && cd build && ninja || cat /code/build/meson-logs/meson-log.txt
WORKDIR /code/build
CMD ninja test
