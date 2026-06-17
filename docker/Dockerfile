# PRIVATE build image for mariogolf64 CI.
#
# This image BAKES the copyrighted baserom (at /orig/baserom.z64), so the GHCR
# package built from it MUST stay PRIVATE. Do not push it to a public registry.
#
# Build it locally where baserom.z64 is present (it is gitignored), then push to
# a private GHCR package. See docker/README.md.
#
# REQUIRED baserom SHA-1: e2c4e7a905b29529b49a1619a401fe699224829b (NMFE, rev 0).
# A wrong region/rev extracts to different asm and the build will not match.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Build deps. gcc-mips-linux-gnu supplies mips-linux-gnu-cpp (the Makefile gate-
# checks it); binutils-mips-linux-gnu supplies as/ld/objcopy/strip; python3-venv
# is needed by `make setup`'s `python3 -m venv`. wget+ca-certificates let
# `make -C tools` download the KMC toolchain.
RUN apt-get update && apt-get install -y --no-install-recommends \
        make git build-essential wget ca-certificates \
        python3 python3-venv python3-pip \
        binutils-mips-linux-gnu gcc-mips-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

# Build context = repo root; baserom.z64 must be present there (gitignored).
COPY baserom.z64 /orig/baserom.z64
