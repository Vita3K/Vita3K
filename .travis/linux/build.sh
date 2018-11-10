#!/bin/bash -ex

docker pull ubuntu:18.04
docker run -v $(pwd):/Vita3K -v "$HOME/.ccache":/root/.ccache ubuntu:18.04 /bin/bash -ex /Vita3K/.travis/linux/docker.sh