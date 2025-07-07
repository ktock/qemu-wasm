#!/bin/bash

set -euo pipefail

BUILD_IMAGE_NAME=testbuildqemu
BUILD_CONTAINER_NAME=testbuildqemu
REPO_PATH="$(pwd)/../../"

ALL_ARCHS=(
    "x86_64"
    "riscv64"
    "aarch64"
    "arm"
    "s390x"
)

ARCHS=("${ALL_ARCHS[@]}")
if [[ $# -gt 0 ]]; then
    ARCHS=($(printf "%s\n" "${ALL_ARCHS[@]}" | grep -E "$1"))
fi

BROWSERS=(
    "chrome"
    "firefox"
    "edge"
)

TESTS=($(ls -1 targets))

echo "Tests: ${TESTS[@]}"
echo "Archs: ${ARCHS[@]}"
echo "Browsers: ${BROWSERS[@]}"

RES=()
FAIL="false"

for TEST in "${TESTS[@]}" ; do
    for ARCH in "${ARCHS[@]}" ; do
        TARGET_WASM=wasm32
        MEMORY64=(0)
        if [ "${ARCH}" != "arm" ] ; then
            TARGET_WASM=wasm64
            MEMORY64=(1 2)
        fi
        for WASM64_MEMORY64 in "${MEMORY64[@]}" ; do
            if [[ -f targets/${TEST}/arch ]] ; then
                if ! cat targets/${TEST}/arch | grep ${ARCH} ; then
                    echo "Test ${TEST} doesn't supported this arch ${ARCH}; skipping..."
                    continue
                fi
            fi
            docker build --progress=plain -t buildbase --build-arg TARGET_CPU=${TARGET_WASM} --build-arg WASM64_MEMORY64=${WASM64_MEMORY64} - < ${REPO_PATH}/tests/docker/dockerfiles/emsdk-wasm-cross.docker
            cat <<EOF | docker build --progress=plain -t ${BUILD_IMAGE_NAME} -
FROM buildbase
WORKDIR /builddeps/
RUN npm i xterm-pty@v0.10.1
RUN cp /builddeps/node_modules/xterm-pty/emscripten-pty.js /builddeps/target/lib/libemscripten-pty.js
ENV EMCC_CFLAGS="-L/builddeps/target/lib/ -lemscripten-pty.js -Wno-unused-command-line-argument"
WORKDIR /build/
EOF
            WASM64_MEMORY64_FLAG=""
            if [ "${WASM64_MEMORY64}" != "2" ] ; then
                WASM64_MEMORY64_FLAG="--enable-wasm64-32bit-address-limit"
            fi
            docker build --progress=plain -t htdocsassets \
                   --build-arg TEST_TARGET_ARCH=${ARCH} \
                   --build-arg QEMU_BUILD_BASE=${BUILD_IMAGE_NAME} \
                   --build-arg TARGET_WASM=${TARGET_WASM} \
                   --build-arg WASM64_MEMORY64_FLAG=${WASM64_MEMORY64_FLAG} \
                   --build-context qemusrc=${REPO_PATH} \
                   targets/${TEST}/image/

            docker build --progress=plain -t test-page -f ./Dockerfile.testpage .
            docker build --progress=plain -t test-node-chrome -f ./Dockerfile.node-chrome .
            docker build --progress=plain -t test-node-firefox -f ./Dockerfile.node-firefox .
            docker build --progress=plain -t test-node-edge -f ./Dockerfile.node-edge .

            docker compose up -d --force-recreate --build
            docker cp "$(pwd)/targets/${TEST}/run.sh" tests-runner-1:/

            for BROWSER in "${BROWSERS[@]}" ; do
                NAME="test:${TEST} arch:${ARCH} browser:${BROWSER} memory64:${WASM64_MEMORY64}"
                echo "Running test: ${NAME}"
                if docker exec -e TARGET_BROWSER="${BROWSER}" tests-runner-1 /bin/bash /run.sh; then
                    RES+=("OK: ${NAME}")
                else
                    RES+=("NG: ${NAME}")
                    FAIL="true"
                fi
            done
        done
    done
done

docker compose down -v
printf "%s\n" "${RES[@]}"

if [[ "${FAIL}" == "true" ]] ; then
    exit 1
fi
