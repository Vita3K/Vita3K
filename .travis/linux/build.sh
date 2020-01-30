#!/usr/bin/env bash
set -ex

docker run -v $(pwd):/Vita3K -v "$HOME/.ccache":/root/.ccache gcc:8 /usr/bin/env bash -ex /Vita3K/.travis/linux/docker.sh || exit $?
