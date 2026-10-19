---
okf_version: "0.2"
---

# Mario Golf 64 decompilation knowledge

An [OKF](https://github.com/GoogleCloudPlatform/open-knowledge-format) bundle: markdown files with YAML frontmatter, read top-down. Start at this index, open the section index that matches the task, then open only the concepts you need. Links starting with `/` are relative to this directory (`docs/`).

# Start here

* [Project](project/) - what the project is, repository layout, ground rules, code-quality bar, compiler profiles, setup and build
* [Procedures](procedures/) - step-by-step how-tos; each task-shaped one is also an omp skill
* [Tools](tools/) - what each script does, when to use it, and how to call it
* [Reference](reference/) - KMC GCC behavior, decompilation artefacts, `symbol_addrs.txt`, `mariogolf64.yaml`, F3DEX2 commands
* [Harness](harness/) - the build, scoring, selection and policing machinery: code map, change rules, policy hook, git hook, CI

# Routing by task

* Match one function → [decompile-function](procedures/decompile-function.md), then [matching-method](procedures/matching-method.md)
* Function builds a display list (`Gfx *`) → [decompile-microcode](procedures/decompile-microcode.md)
* Register-only differences at 95%+ → [decomp-permuter](procedures/decomp-permuter.md)
* Move data, rodata or bss into C → [match-data-segment](procedures/match-data-segment.md), [match-bss-segment](procedures/match-bss-segment.md)
* Turn a libultra ROM range into source → [match-ultralib-segment](procedures/match-ultralib-segment.md)
* Replace raw ROM addresses with symbols → [add-asset](procedures/add-asset.md)
* Rename a symbol or file → [rename-global](procedures/rename-global.md), [rename-file](procedures/rename-file.md)
* Clean up already-matched code → [cleanup-function](procedures/cleanup-function.md)
* A tool, script, prompt or the policy hook is wrong → [changing the harness](harness/changing-the-harness.md)
