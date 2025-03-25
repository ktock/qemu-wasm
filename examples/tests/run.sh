#!/bin/bash

set -euo pipefail

BUILD_IMAGE_NAME=testbuildqemu
BUILD_CONTAINER_NAME=testbuildqemu
REPO_PATH="$(pwd)/../../"

ALL_ARCHS=(
    "arm"
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

docker build --progress=plain -t buildbase - < ${REPO_PATH}/tests/docker/dockerfiles/emsdk-wasm32-cross.docker
cat <<EOF | docker build --progress=plain -t ${BUILD_IMAGE_NAME} -
FROM buildbase
WORKDIR /builddeps/
ENV EMCC_CFLAGS="--js-library=/builddeps/node_modules/xterm-pty/emscripten-pty.js"
RUN npm i xterm-pty@v0.10.1
WORKDIR /build/
EOF

RES=()
FAIL="false"

for TEST in "${TESTS[@]}" ; do
    for ARCH in "${ARCHS[@]}" ; do
        if [[ -f targets/${TEST}/arch ]] ; then
            if ! cat targets/${TEST}/arch | grep ${ARCH} ; then
                echo "Test ${TEST} doesn't supported this arch ${ARCH}; skipping..."
                continue
            fi
        fi
        docker build --progress=plain -t htdocsassets \
               --build-arg TEST_TARGET_ARCH=${ARCH} \
               --build-arg QEMU_BUILD_BASE=${BUILD_IMAGE_NAME} \
               --build-context qemusrc=${REPO_PATH} \
               targets/${TEST}/image/

        docker build --progress=plain -t test-page -f ./Dockerfile.testpage .
        docker build --progress=plain -t test-node-chrome -f ./Dockerfile.node-chrome .
        docker build --progress=plain -t test-node-firefox -f ./Dockerfile.node-firefox .
        docker build --progress=plain -t test-node-edge -f ./Dockerfile.node-edge .

        docker compose up -d --force-recreate --build
        docker cp "$(pwd)/targets/${TEST}/run.sh" tests-runner-1:/

        for BROWSER in "${BROWSERS[@]}" ; do
            NAME="test:${TEST} arch:${ARCH} browser:${BROWSER}"
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

docker compose down -v
printf "%s\n" "${RES[@]}"

if [[ "${FAIL}" == "true" ]] ; then
    exit 1
fi
