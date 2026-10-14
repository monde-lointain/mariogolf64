---
type: Playbook
title: Private CI build container
description: Builds and publishes the private ROM-bearing image used by trusted CI build jobs.
sources:
  - resource: Dockerfile
    title: Container dependencies and ROM location
  - resource: ../.github/workflows/build.yml
    title: CI permissions and job conditions
---

# CI build container

The [Build workflow](../.github/workflows/build.yml) uses a private container with the copyrighted
ROM at `/orig/baserom.z64`. Keep the image private. Its build job runs only for same-repository
pushes and PRs, not fork PRs: PR-controlled scripts could otherwise exfiltrate the ROM. The separate
Python analysis job is ROM-free and advisory.

## One-time / on-dep-change: build and push the image

Run from the **repo root** (the build context), with `baserom.z64` present there.

```sh
# Compare this input SHA-1 with the top-level sha1 in mariogolf64.yaml:
sha1sum baserom.z64

docker build -f docker/Dockerfile -t ghcr.io/monde-lointain/mariogolf64-build:main .

# $CR_PAT = a classic PAT with write:packages
echo "$CR_PAT" | docker login ghcr.io -u monde-lointain --password-stdin
docker push ghcr.io/monde-lointain/mariogolf64-build:main
```

Then, in the GHCR package settings:

- Keep visibility **Private** (the image contains the ROM).
- Under *Manage Actions access*, give the `mariogolf64` repo Read access (or rely
  on the `GHCR_PAT` secret below).

Rebuild and push whenever the system dependencies in `docker/Dockerfile` change.
The image is hand-maintained: it can never be rebuilt by CI, since the ROM cannot
live in CI.

## One-time: repo secret for CI pull

Add a repo secret **`GHCR_PAT`** = a classic PAT with `read:packages`. The
workflow uses it to pull the private image. (A personal-account private package
usually cannot be pulled with the default `GITHUB_TOKEN`, hence the PAT.)

## Sequencing

Push the image and add the `GHCR_PAT` secret before enabling or updating the workflow's image
reference; a missing image or credential fails at container pull, before checkout.

## Local end-to-end test (optional)

Reproduce the workflow steps before pushing:

```sh
docker build -f docker/Dockerfile -t mg64-ci-test .
docker run --rm -v "$PWD":/repo -w /repo mg64-ci-test bash -c \
  'cp /orig/baserom.z64 baserom.z64 && cmake --preset us && tools/verify-rom.sh'
# expect: CMake build OK + ROM SHA-1 MATCH
```

## Local checkout prerequisites

Use the [local build guide](../docs/build.md) for bootstrap tools, offline dependency ownership,
extraction, verification, and presets. Rebuild the private image for this CMake cutover before CI:
the old image lacks CMake 3.30+ and the 32-bit host compilation prerequisites.
Required missing assembly stubs are now recovered automatically during fresh extraction.
