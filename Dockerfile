# base image
FROM ubuntu:24.04
ARG DEBIAN_FRONTEND=noninteractive

# docker image arguments
ARG RISCV32_TOOLCHAIN_LINK=https://github.com/bao-project/bao-riscv-toolchain/releases/download/gc891d8dc23e/riscv32-unknown-elf.tar.gz
# josecm/qemu @ spmp-hypervisor - SPMP-for-Hypervisor aware QEMU.
ARG QEMU_REPO=https://github.com/josecm/qemu.git
ARG QEMU_REVISION=f942c7d7f67175fbed6886af31df95758adbaa01

# Install base dependencies. Includes:
#   - toolchain unpack / repo cloning (git, curl, wget, xz-utils)
#   - C/C++ build tools (build-essential, cmake, ninja-build, make)
#   - Python + pip for Zephyr's cmake-time scripts
#   - device-tree-compiler for Zephyr's DT pipeline
#   - meson + dev libs for the QEMU build (glib, pixman, slirp, fdt)
RUN apt-get update && apt-get install -y software-properties-common && \
    add-apt-repository ppa:deadsnakes/ppa && \
    apt-get update

RUN apt-get install -y \
        git \
        curl \
        wget \
        xz-utils \
        build-essential \
        cmake \
        ninja-build \
        meson \
        python3 \
        python3-pip \
        python3-venv \
        device-tree-compiler \
        pkg-config \
        libglib2.0-dev \
        libpixman-1-dev \
        libslirp-dev \
        libfdt-dev \
        texinfo \
        tree \
        vim \
        nano \
        tmux \
        minicom \
        libpython3.10 \
        gperf

# Python packages used at Zephyr cmake/build time (we drive cmake/ninja
# directly, no west). --break-system-packages is needed on Ubuntu 24.04
# because the system Python is marked externally managed (PEP 668).
RUN pip3 install --break-system-packages \
        pyelftools \
        pyyaml \
        pykwalify \
        packaging \
        anytree \
        ply \
        jsonschema \
        pyserial

# Install GNU RISC-V toolchain
RUN mkdir /opt/riscv32-toolchain && curl -L $RISCV32_TOOLCHAIN_LINK | tar xz -C /opt/riscv32-toolchain --strip-components=1

# Build the SPMP-aware QEMU (josecm/qemu @ spmp-hypervisor). Only the
# riscv32-softmmu target is built. UI / display backends are disabled
# since the workshop is headless.
RUN git clone $QEMU_REPO /tmp/qemu-src && \
    cd /tmp/qemu-src && \
    git checkout $QEMU_REVISION && \
    git submodule update --init --depth 1 && \
    ./configure \
        --target-list=riscv32-softmmu \
        --prefix=/opt/qemu-spmp \
        --disable-gtk --disable-sdl --disable-vnc --disable-curses \
        --disable-cocoa --disable-werror --disable-docs && \
    make -j$(nproc) && \
    make install && \
    ln -sf /opt/qemu-spmp/bin/qemu-system-riscv32 \
           /opt/qemu-spmp/bin/qemu-system-riscv32-spmp && \
    rm -rf /tmp/qemu-src

# Add generic non-root user
RUN addgroup bao && adduser -disabled-password --ingroup bao bao

# setup environment
ENV PATH=$PATH:/opt/riscv32-toolchain/bin
ENV PATH=$PATH:/opt/qemu-spmp/bin

# default startup command
CMD /bin/bash
