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
COPY --from=ghcr.io/astral-sh/uv:0.13.1 /uv /uvx /usr/local/bin/

ENV DEBIAN_FRONTEND=noninteractive

# CMake builds the pinned MIPS toolchain from local sources. Host compilation,
# uv and GNU Make are bootstrap prerequisites, not target toolchains.
RUN apt-get update && apt-get install -y --no-install-recommends \
        make git build-essential gcc-multilib bison flex texinfo ca-certificates \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*
ENV UV_TOOL_BIN_DIR="/usr/local/bin"
RUN uv python install 3.14 && uv tool install "cmake>=4.4.4"

# Build context = repo root; baserom.z64 must be present there (gitignored).
COPY baserom.z64 /orig/baserom.z64
