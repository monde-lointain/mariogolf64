---
type: Tool
title: "asm-differ"
description: "Diff a function in the built ROM against the target assembly; vendored submodule, protected."
tags: [diff, verify]
resource: ../../tools/asm-differ/diff.py
---

```bash
python3 tools/asm-differ/diff.py --no-pager <function name>
```

Configured by `diff_settings.py` at the repository root (base image `baserom.z64`, built image `build/mariogolf64.z64`, map `build/mariogolf64.map`, sources `./src ./include ./asm`).

**Rebuild first.** It compares against the current `build/`; after any source change run [build-and-verify](build-and-verify.md) or you will read stale data. Use it after a full-ROM checksum failure to check every function in the files you changed.

Inside a decomp environment use `./build.sh` and `./diff.sh` instead ([decomp-env](decomp-env.md)); they score a candidate in isolation.

`tools/asm-differ` is a vendored submodule and protected ([ground rules](/project/ground-rules.md)).
