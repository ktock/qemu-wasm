#!/bin/bash

set -eu -o pipefail

REPO_PATH="$(pwd)/../../"
DEST="${1}"
TARGETS=(
    "tci"
    "tcg"
    "mttcg"
)

echo "DEST=${DEST}"

docker build --progress=plain -t buildbase-bench-base \
       --build-arg TARGET_CPU=wasm64 --build-arg WASM64_MEMORY64=2 \
       - < ${REPO_PATH}/tests/docker/dockerfiles/emsdk-wasm-cross.docker
cat <<EOF | docker build --progress=plain -t buildbase-bench -
FROM buildbase-bench-base
WORKDIR /builddeps/
RUN npm i xterm-pty@v0.10.1
RUN cp /builddeps/node_modules/xterm-pty/emscripten-pty.js /builddeps/target/lib/libemscripten-pty.js
ENV EMCC_CFLAGS="-L/builddeps/target/lib/ -lemscripten-pty.js -Wno-unused-command-line-argument"
WORKDIR /build/
EOF

cp ${REPO_PATH}/examples/benchmarks/image/xterm-pty.conf "${DEST}/"
mkdir -p "${DEST}/htdocs"

for TARGET in "${TARGETS[@]}" ; do
    EXTRA_CONFIGURE_FLAGS=--enable-wasm64-32bit-address-limit
    if [ "${TARGET}" == "tci" ] ; then
        EXTRA_CONFIGURE_FLAGS=--enable-tcg-interpreter
    fi

    mkdir -p "${DEST}/htdocs/${TARGET}"
    echo "target: ${TARGET}" >> "${DEST}/htdocs/${TARGET}/spec"
    echo "extra flags: ${EXTRA_CONFIGURE_FLAGS}" >> "${DEST}/htdocs/${TARGET}/spec"

    docker build --progress=plain -f ${REPO_PATH}/examples/benchmarks/image/Dockerfile \
           --build-context qemusrc=${REPO_PATH} \
           --build-arg QEMU_BUILD_BASE=buildbase-bench \
           --build-arg TARGET_MODULE="${TARGET}" \
           --build-arg EXTRA_CONFIGURE_FLAGS="${EXTRA_CONFIGURE_FLAGS}" \
           --output type=local,dest="${DEST}/htdocs/${TARGET}" ${REPO_PATH}/examples/benchmarks/image/
done
