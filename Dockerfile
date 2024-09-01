FROM nvidia/opengl:1.2-glvnd-runtime-ubuntu20.04

ENV NVIDIA_VISIBLE_DEVICES=all \
    DEBIAN_FRONTEND=noninteractive \
    NVIDIA_DRIVER_CAPABILITIES=all

RUN /bin/bash <<'__INSTALL_DEPENDENCIES__'
set -euo pipefail on

apt-get update \
##

packages=(
    cmake
    build-essential
    gcc
    git
    libegl1-mesa-dev
    freeglut3-dev
    libegl-mesa0
    nouveau-firmware
    mesa-utils
    libosmesa6-dev
    zlib1g-dev
    autotools-dev
    automake
    autopoint
    libtool
    texinfo
    git
    python3.9
    python3-pip
    python3-virtualenv
    python3.9-dev
    pkg-config
    # vim's needed for xxd
    vim
    uuid-dev
    gdb
)

apt-get install -y \
    "${packages[@]}" \
##

rm -rf \
    /var/lib/apt/lists/* \
##

__INSTALL_DEPENDENCIES__
