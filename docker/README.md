# CI build container

The `Build` workflow (`.github/workflows/build.yml`) runs inside a **private**
container that bakes the copyrighted baserom at `/orig/baserom.z64`, mirroring the
drmario64 CI setup. The ROM is gitignored, so it cannot ship in the repo or live
in CI; it is baked into this image instead, which is why the image must stay
private.

## One-time / on-dep-change: build and push the image

Run from the **repo root** (the build context), with `baserom.z64` present there.

```sh
# baserom.z64 must be SHA-1 e2c4e7a905b29529b49a1619a401fe699224829b (NMFE, rev 0):
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

Push the image and add the `GHCR_PAT` secret **before** merging the workflow,
otherwise the first CI run fails at the container pull.

## Local end-to-end test (optional)

Reproduce the workflow steps before pushing:

```sh
docker build -f docker/Dockerfile -t mg64-ci-test .
docker run --rm -v "$PWD":/repo -w /repo mg64-ci-test bash -c \
  'cp /orig/baserom.z64 baserom.z64 && make setup && make extract && tools/verify-rom.sh'
# expect: make OK + ROM SHA-1 MATCH ✓
```

## Local checkout prerequisites

Local builds use host `cpp` and system MIPS binutils for extracted assembly; C still uses
KMC GCC 2.7.2 and KMC binutils 2.6. Install `binutils-mips-linux-gnu`, `cpp`, and `clang-format-23`,
place the USA revision-0 ROM at `baserom.z64`, and initialize the recorded submodules.
On uv-managed machines, create `venv` with `uv venv --seed venv`, install
`tools/requirements.txt` with `uv pip install --python venv/bin/python3 -r tools/requirements.txt`,
and download the legacy compiler with `make -C tools`.
Run `make extract`, then `tools/verify-rom.sh`; stop on failure rather than trusting an old ROM.
For missing generated assembly stubs, use
`docs/hazards.md#stale-persistent-nonmatchings-relic-recovery`.
