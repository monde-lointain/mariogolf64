# PRIVATE build image for mariogolf64 CI.
#
# This image BAKES the copyrighted baserom (at /orig/baserom.z64), so the GHCR
# package built from it MUST stay PRIVATE. Do not push it to a public registry.
#
# Build it locally where baserom.z64 is present (it is gitignored), then push to
# a private GHCR package. See docs/harness/git-hook-and-ci.md.
#
# REQUIRED baserom SHA-1: e2c4e7a905b29529b49a1619a401fe699224829b (NMFE, rev 0).
# A wrong region/rev extracts to different asm and the build will not match.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# The Makefile needs GNU Make, the host C preprocessor, MIPS binutils, curl for
# the KMC toolchain download (`make setup`), shasum, and Python for Splat.
RUN apt-get update && apt-get install -y --no-install-recommends \
        make cpp git curl ca-certificates perl xxd \
        binutils-mips-linux-gnu python3 python3-venv \
    && rm -rf /var/lib/apt/lists/*

# Build context = repo root; baserom.z64 must be present there (gitignored).
COPY baserom.z64 /orig/baserom.z64
