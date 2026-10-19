---
okf_version: "0.2"
---

# Mario Golf 64 decompilation knowledge

An [OKF](https://github.com/GoogleCloudPlatform/open-knowledge-format) bundle: markdown concepts with YAML frontmatter, read top-down. Open this index, then the section index that matches the task, then only the concepts you need. Links starting with `/` are relative to this directory (`docs/`). [log.md](log.md) records notable docs and harness changes.

# Sections

* [Project](project/) - what the project is, ground rules, repository layout, setup and build, code-quality bar, compiler profiles
* [Procedures](procedures/) - step-by-step tasks and the matching method; each task-shaped one is also an omp skill
* [Tools](tools/) - what each script does, its flags and output: build and diff, per-function environments, selection and status, nigel automation
* [Reference](reference/) - KMC GCC behavior, decompilation artefacts, `symbol_addrs.txt` and `mariogolf64.yaml` formats, F3DEX2 commands
* [Harness](harness/) - where the build, scoring, selection and policing code lives, its tests, the policy hook, git hook and CI, and how to change them

# Routing by task

* [Decompile a function](procedures/decompile-function.md) - match one function; strategy when it will not match is in [matching method](procedures/matching-method.md)
* [Decompile F3DEX2 display-list code](procedures/decompile-microcode.md) - the function writes a display list (`Gfx *` cursor)
* [KMC GCC learnings](reference/kmc-gcc/) - a diff you cannot explain, by symptom
* [asm-differ](tools/asm-differ.md) - the full ROM checksum fails after integrating; find the function that differs
* [Match a data or rodata segment](procedures/match-data-segment.md) - move data or rodata into C; bss is [match-bss-segment](procedures/match-bss-segment.md)
* [Match a libultra segment](procedures/match-ultralib-segment.md) - turn a libultra ROM range into source
* [Add an asset and convert ROM-address references](procedures/add-asset.md) - replace raw ROM addresses with linker symbols
* [Rename a global variable or function](procedures/rename-global.md) - rename a symbol; files are [rename-file](procedures/rename-file.md)
* [Clean up a matched function](procedures/cleanup-function.md) - improve names and types of matched code
* [Selection, ranking and status tools](tools/selection-and-status.md) - choose what to work on next; overall progress is [progress](tools/progress.md)
* [task-runner (nigel)](tools/task-runner.md) - run or change automated agent tasks
* [Changing the harness](harness/changing-the-harness.md) - a tool, script, prompt, doc or the policy hook is wrong or in the way
