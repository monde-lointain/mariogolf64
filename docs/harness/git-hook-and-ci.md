---
type: Reference
title: "Git pre-commit hook and CI"
description: "The local pre-commit rebuild, the GitHub Actions ROM build, and how to build and publish the private container that bakes in the ROM."
tags: [harness, ci, git]
resource: ../../.github/workflows/build.yml
---

# Pre-commit hook

`tools/git-hooks/pre-commit` is symlinked to `.git/hooks/pre-commit` by `tools/install-git-hooks.sh` (run by `make setup`; an existing different hook is backed up to `pre-commit.bak.<timestamp>`). The same script registers the `merge.decomp-c` merge driver on every run; `.gitattributes` assigns it to `src/**/*.c` ([shard integration](/tools/shard-integration.md)). On every commit it:

1. rejects staged changes to `mariogolf64.sha1`;
2. runs `tools/build-and-verify.sh`, a full clean rebuild and SHA1 check.

Never bypass it (`--no-verify` is blocked by the [policy hook](/harness/policy-hook.md)). A failing hook means the change does not match.

# CI

`.github/workflows/build.yml` runs on push and on pull requests **from the same repository only**. The job runs in a private container with the copyrighted ROM at `/orig/baserom.z64`; PR-controlled scripts from a fork could otherwise exfiltrate it, and forks never receive the pull credential anyway. Steps: checkout with submodules, copy the ROM to `baserom.z64`, `make -C tools` (KMC toolchain), create `.venv` and install `requirements.txt`, `./tools/build-and-verify.sh`, `python3 tools/progress.py --segments -o report.json` (table to the job summary, see [progress](/tools/progress.md)), upload `report.json` as artifact `us_report`.

No step runs the unit tests; see the [code map](/harness/code-map.md#tests).

# The build container

The image `ghcr.io/monde-lointain/mariogolf64-build:main` is built from `docker/Dockerfile` (Ubuntu 24.04 with `make`, `cpp`, `git`, `curl`, `ca-certificates`, `perl`, `xxd`, `binutils-mips-linux-gnu`, `python3`, `python3-venv`) and bakes in `baserom.z64`. **Keep the image and package private.** It is hand-maintained: CI can never rebuild it because the ROM cannot live in CI. Rebuild and push whenever `docker/Dockerfile`'s system dependencies change.

Build and push, from the repo root with `baserom.z64` present (`.dockerignore` excludes everything else from the context):

```sh
sha1sum baserom.z64     # must equal mariogolf64.sha1
docker build -f docker/Dockerfile -t ghcr.io/monde-lointain/mariogolf64-build:main .
echo "$CR_PAT" | docker login ghcr.io -u monde-lointain --password-stdin   # classic PAT, write:packages
docker push ghcr.io/monde-lointain/mariogolf64-build:main
```

In the GHCR package settings keep visibility **Private** and give the `mariogolf64` repo read access under *Manage Actions access*. Add a repo secret `GHCR_PAT` (classic PAT with `read:packages`); a personal-account private package usually cannot be pulled with the default `GITHUB_TOKEN`.

Test locally before pushing. The repo is bind-mounted, so the `cp` overwrites the host's `baserom.z64`:

```sh
docker build -f docker/Dockerfile -t mg64-ci-test .
docker run --rm -v "$PWD":/repo -w /repo mg64-ci-test bash -c \
  'cp /orig/baserom.z64 baserom.z64 && make -C tools &&
   python3 -m venv /tmp/venv && /tmp/venv/bin/pip install -r requirements.txt &&
   PATH=/tmp/venv/bin:$PATH ./tools/build-and-verify.sh'
# expect: build/mariogolf64.z64: OK
```
