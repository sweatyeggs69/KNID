# Build image for the Linux (AppImage) release job, shared by all the GBA ports.
#
# GitHub's Ubuntu runners have had trouble reaching any Ubuntu package mirror, so the release
# job runs in this image, which already has every package the build needs, instead of running
# apt itself. Build and push it from any machine with Docker (Apple Silicon included):
#
#   docker login ghcr.io -u <github user>      # token with write:packages
#   docker buildx build --platform linux/amd64 \
#     -t ghcr.io/sweatyeggs69/gba-port-linux:22.04 --push ci/linux
#
# Then make the package public on GitHub (Packages > gba-port-linux > Package settings) so every
# port's workflow can pull it. Rebuild only when this list changes. Keep it on Ubuntu 22.04: the
# AppImage runs on any distro whose glibc is at least as new as the one it was built against.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates curl git file zstd xz-utils python3 python3-pip \
      build-essential ninja-build pkg-config zlib1g-dev \
      libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
      libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
      libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
      libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev libibus-1.0-dev \
      libfuse2 desktop-file-utils \
 && rm -rf /var/lib/apt/lists/*

# Ubuntu 22.04 ships CMake 3.22; the ports need 3.24 or newer.
RUN pip3 install --no-cache-dir cmake==3.31.6 && cmake --version
