# syntax = docker/dockerfile:1.5

ARG EMSDK_VERSION_QEMU=3.1.50 # TODO: support recent version
ARG ZLIB_VERSION=1.3.1
ARG GLIB_MINOR_VERSION=2.75
ARG GLIB_VERSION=${GLIB_MINOR_VERSION}.0
ARG PIXMAN_VERSION=0.42.2
ARG FFI_VERSION=adbcf2b247696dde2667ab552cb93e0c79455c84

FROM emscripten/emsdk:$EMSDK_VERSION_QEMU AS build-base
# Porting glib to emscripten inspired by https://github.com/emscripten-core/emscripten/issues/11066
ENV TARGET=/build/target
ENV CFLAGS="-O2 -matomics -mbulk-memory -DNDEBUG -sWASM_BIGINT -DWASM_BIGINT -pthread -sMALLOC=mimalloc  -sASYNCIFY=1 "
ENV CXXFLAGS="$CFLAGS"
ENV LDFLAGS="-L$TARGET/lib -O2"
ENV CPATH="$TARGET/include"
ENV PKG_CONFIG_PATH="$TARGET/lib/pkgconfig"
ENV EM_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
ENV CHOST="wasm32-unknown-linux"
ENV MAKEFLAGS="-j$(nproc)"
RUN apt-get update && apt-get install -y \
    autoconf \
    build-essential \
    libglib2.0-dev \
    libtool \
    pkgconf \
    ninja-build \
    python3-pip
RUN pip3 install meson==1.5.0
RUN mkdir /build
WORKDIR /build
RUN mkdir -p $TARGET

FROM build-base AS zlib-emscripten-dev
ARG ZLIB_VERSION
RUN mkdir -p /zlib
RUN curl -Ls https://zlib.net/zlib-$ZLIB_VERSION.tar.xz | tar xJC /zlib --strip-components=1
WORKDIR /zlib
RUN emconfigure ./configure --prefix=$TARGET --static
RUN make install

FROM build-base AS libffi-emscripten-dev
ARG FFI_VERSION
RUN mkdir -p /libffi
RUN git clone https://github.com/libffi/libffi /libffi
WORKDIR /libffi
RUN git checkout $FFI_VERSION
RUN autoreconf -fiv
RUN LDFLAGS="$LDFLAGS -sEXPORTED_RUNTIME_METHODS='getTempRet0,setTempRet0'" ; \
    emconfigure ./configure --host=$CHOST --prefix=$TARGET --enable-static --disable-shared --disable-dependency-tracking \
    --disable-builddir --disable-multi-os-directory --disable-raw-api --disable-structs --disable-docs
RUN emmake make install SUBDIRS='include'

FROM build-base AS build-dev
ARG GLIB_VERSION
ARG GLIB_MINOR_VERSION
RUN mkdir -p /stub
WORKDIR /stub
RUN <<EOF
cat <<'EOT' > res_query.c
#include <netdb.h>
int res_query(const char *name, int class, int type, unsigned char *dest, int len)
{
    h_errno = HOST_NOT_FOUND;
    return -1;
}
EOT
EOF
RUN emcc ${CFLAGS} -c res_query.c -fPIC -o libresolv.o
RUN ar rcs libresolv.a libresolv.o
RUN mkdir -p $TARGET/lib/
RUN cp libresolv.a $TARGET/lib/

RUN mkdir -p /glib
RUN curl -Lks https://download.gnome.org/sources/glib/${GLIB_MINOR_VERSION}/glib-$GLIB_VERSION.tar.xz | tar xJC /glib --strip-components=1

COPY --link --from=zlib-emscripten-dev /build/ /build/
COPY --link --from=libffi-emscripten-dev /build/ /build/

WORKDIR /glib
ENV CFLAGS="-Wno-error=incompatible-function-pointer-types -Wincompatible-function-pointer-types -O2 -matomics -mbulk-memory -DNDEBUG -pthread -sWASM_BIGINT -sMALLOC=mimalloc -sASYNCIFY=1"
ENV CXXFLAGS="$CFLAGS"
RUN <<EOF
cat <<'EOT' > /emcc-meson-wrap.sh
#!/bin/bash
set -euo pipefail
old_string="-Werror=unused-command-line-argument"
# emscripten ignores some -s flags during compilation with warnings. Meson checking phase fails when it sees these warnings.
new_string="-Wno-error=unused-command-line-argument"
cmd="$1"
shift
new_args=()
for arg in "$@"; do
  new_arg="${arg//$old_string/$new_string}"
  new_args+=("$new_arg")
done
"$cmd" "${new_args[@]}"
EOT
EOF
RUN <<EOF
cat <<'EOT' > /cross.meson
[host_machine]
system = 'emscripten'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'

[binaries]
c = ['bash', '/emcc-meson-wrap.sh', 'emcc']
cpp = ['bash', '/emcc-meson-wrap.sh', 'em++']
ar = 'emar'
ranlib = 'emranlib'
pkgconfig = ['pkg-config', '--static']
EOT
EOF
RUN meson setup _build --prefix=$TARGET --cross-file=/cross.meson --default-library=static --buildtype=release \
    --force-fallback-for=pcre2,gvdb -Dselinux=disabled -Dxattr=false -Dlibmount=disabled -Dnls=disabled \
    -Dtests=false -Dglib_assert=false -Dglib_checks=false
RUN sed -i -E "/#define HAVE_CLOSE_RANGE 1/d" ./_build/config.h
RUN sed -i -E "/#define HAVE_EPOLL_CREATE 1/d" ./_build/config.h
RUN sed -i -E "/#define HAVE_KQUEUE 1/d" ./_build/config.h
RUN sed -i -E "/#define HAVE_POSIX_SPAWN 1/d" ./_build/config.h
RUN sed -i -E "/#define HAVE_FALLOCATE 1/d" ./_build/config.h
RUN meson install -C _build

FROM build-base AS pixman-emscripten-dev
ARG PIXMAN_VERSION
RUN mkdir /pixman/
RUN git clone  https://gitlab.freedesktop.org/pixman/pixman /pixman/
WORKDIR /pixman
RUN git checkout pixman-$PIXMAN_VERSION
RUN NOCONFIGURE=y ./autogen.sh
RUN emconfigure ./configure --prefix=/build/target/
RUN emmake make -j$(nproc)
RUN emmake make install
RUN rm /build/target/lib/libpixman-1.so /build/target/lib/libpixman-1.so.0 /build/target/lib/libpixman-1.so.$PIXMAN_VERSION

FROM build-base
COPY --link --from=zlib-emscripten-dev /build/ /build/
COPY --link --from=build-dev /build/ /build/
COPY --link --from=pixman-emscripten-dev /build/ /build/
WORKDIR /build/
RUN npm i xterm-pty@v0.10.1
CMD sleep infinity
