# Native (x86_64 or aarch64) build and test environment for Kick.
#   docker build -t kick-build Kick/docker
#   docker run --rm -v "$PWD":/src -w /src/Kick kick-build sh docker/build.sh
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates pkg-config file \
        libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev libfreetype6-dev libfontconfig1-dev \
        libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev \
        libxrender-dev libxi-dev libglu1-mesa-dev mesa-common-dev libserial-dev libserialport-dev \
        xvfb xauth libcairo2-dev lv2-dev lilv-utils jackd2 alsa-utils \
    && rm -rf /var/lib/apt/lists/*
