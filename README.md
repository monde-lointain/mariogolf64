Mario Golf 64
=============

A matching decompilation of [Mario Golf](https://en.wikipedia.org/wiki/Mario_Golf_(video_game)) for Nintendo 64 (North America, revision 0).

**This repository does not contain any game assets or assembly whatsoever. An existing copy of the game is required. This project is not a port, to PC or any other platform. It takes a Nintendo 64 rom, extracts assets from it, and combines them with C code we reverse-engineered from the rom, to produce an identical rom. It is explicitly a non-goal for this project to be used for any commercial purpose.**

## Quick start

Linux x86-64. Dependencies and details: [docs/project/setup-and-build.md](docs/project/setup-and-build.md).

```bash
git clone --recurse-submodules -j8 https://github.com/monde-lointain/mariogolf64.git
cd mariogolf64
make setup                                   # KMC toolchain + git pre-commit hook
python3 -m venv .venv && source .venv/bin/activate
python3 -m pip install -r requirements.txt
cp /path/to/baserom.z64 .                    # SHA1 e2c4e7a905b29529b49a1619a401fe699224829b
./tools/build-and-verify.sh                  # prints build/mariogolf64.z64: OK and BUILD SUCCEEDED
```

The output is `build/mariogolf64.z64`. `python3 tools/progress.py` then reports how much of its code and data is decompiled ([progress](docs/tools/progress.md)).

## Documentation

All documentation is the [OKF](https://github.com/GoogleCloudPlatform/open-knowledge-format) bundle in [docs/](docs/index.md): project orientation, step-by-step procedures, tool references, compiler and format references, and the harness that builds and polices the work. Start at [docs/index.md](docs/index.md). Agent instructions are in [AGENTS.md](AGENTS.md); the agent-driven decompilation workflow follows the Snowboard Kids decompilation projects.

## License

The [MIT license](LICENSE) covers first-party work, not Nintendo's ROM or the third-party tools in `tools/`.
