# Documentation update log

## 2026-10-14

### uv-managed Python tooling

- **Dependencies**: Consolidated runtime, test, and analysis dependencies in `pyproject.toml`;
  resolved current stable compatible releases into `uv.lock`. Removed requirements files and the
  obsolete vendored Python wheelhouse/source distributions. Initial installation now uses PyPI.
- **Environment**: Required uv 0.13.1+ and one root `.venv`; migrated CMake, shell wrappers, CI,
  container bootstrap, active command examples, and analysis configuration away from build-tree
  environments. Ordinary setup uses `uv sync --locked`; upgrades use `uv lock --upgrade`.
- **Tools**: Updated Splat to 0.50.1, spimdisasm to 1.42.4, rabbitizer to 1.16.2, Ruff to 0.17.0,
  basedpyright to 1.40.2, and coverage to 7.16.2. Imported current asm-differ and m2c upstream
  revisions; permuter was already current and retains its local import adaptations.
- **Compatibility**: Kept target compiler profiles and native versions unchanged. Adapted extraction
  to preserve encoded absolute J targets when current disassembly emits undefined local labels;
  retained colon and macro-defined local labels and their relocations.
- **CMake**: Applied Professional CMake's lifecycle and dependency guidance: uv discovery with
  `find_program`, locked configure-time manifest generation, explicit target ordering, and a
  content-stable lockfile byproduct so unchanged environments do not force extraction.
- **Proof**: The full-ROM verifier passed with SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
  Nine targeted compiler/permuter/jump cases passed. The final full tooling run reported 120 passed,
  two skipped, and the six documented ranker/coddog failures; no goldens were regenerated.
- **Limits**: Hosted CI and the private Docker image have not been executed here. Rebuild the image
  before publishing this workflow's container changes.

### Monorepo and CMake modernization

- **Ownership**: Imported asm-differ, decomp-permuter, and m2c as ordinary Git source; removed
  submodule configuration. Added pinned compiler/binutils source archives and the complete
  hash-locked offline Python wheelhouse with corresponding source distributions and license notices.
- **Build**: Replaced the repository Make interface with CMake 3.30+ presets, source-built tools,
  binary-tree extraction, output-tracked objects, include depfiles, and authoritative per-source
  compiler profiles. Preserved the KMC/modern-assembler split and exact linker object order.
- **Workflow**: Consolidated prompt entrypoints into the native Oh My Pi `mg64-decomp` skill and
  `/decomp` command. Retired duplicated Claude/Codex prompts, prompt budgets/linting, legacy Make
  fragments, and duplicate permuter profiles; retained reconstruction and sprint history.
- **Integration**: Migrated ranker, diff, isolated compilation, permuter, optional reference builds,
  CI, and container instructions to the configured binary-tree tools. Permuter imports inherit the
  parent TU's profile and recognize CMake roots and GNU assembly section flags.
- **ROM proof**: A source-only export, without old assembly/assets, tool binaries, submodules, or
  Python environments, built the US retail ROM using CMake 3.30.9. The verifier reported SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`; the working tree also passed with CMake 4.2.3.
- **Behavior proof**: Header invalidation rebuilt dependent objects and retained the matching ROM.
  A deliberate compiler error rejected an older matching ROM before hashing; a nonmatching
  configuration was refused. Unchanged extraction preserved generated-file timestamps and avoided
  object rebuilds. Real `rand` isolated compilation and permuter scoring both measured score zero.
- **Checks**: The ROM CTest passed; 14 targeted compiler-profile, assembly-parser, decomp-loop,
  and hazard-anchor tests passed; the tooling undefined-name check passed. Exercised ranking,
  profile probing, allocno reporting, and native Oh My Pi command/skill discovery.
- **Limits**: Docker and hosted CI configuration were updated but not executed. The private CI image
  must be rebuilt. No full-suite-green claim replaces the historical tooling limitations below.

### Earlier documentation restructuring

- **Structure**: Established the OKF v0.2 [bundle index](index.md), workflow and reconstruction
  indexes, typed concept metadata, and explicit separation of current guidance from historical evidence.
- **Discovery**: Separated [candidate/build hazards](hazard-index.md) from
  [matching residuals](hazard-symptoms.md), made playbook routes clickable, and repaired the
  default-return-variable anchor without renaming the playbook headings.
- **Guidance**: Added canonical [local build](build.md), [tooling](tooling.md),
  [history](history.md), and [documentation maintenance](documentation.md) references.
- **Alignment**: Corrected the segment-versus-library filter description, made byte-exact matching
  take priority over generic C style, and consolidated duplicated local setup out of the container guide.
- **Preservation**: Retained historical entry bodies, reconstruction evidence, harness invocation
  fields, and the shared safety-invariant block.
- **Tooling integration**: Excluded OKF's reserved `index.md` and `log.md` files from executable
  prompt selection; instruction checks and their byte budgets remain enforced.
- **Validation**: Parsed 102 bundle concepts and checked directory coverage and local navigation;
  prompt and hazard-anchor contracts passed. Exercised Make help, segment ranking, loose-stub
  discovery, and rejection of a segment passed to `--lib`.
- **Guard review**: Token-count losses arise from moving residual rows, replacing code-span
  citations with links, consolidating duplicated routing/naming/oracle text, retiring generic C++
  advice, and updating the formatter version. The guard is documented as a review signal for these
  structural changes, not proof that file-local token counts must remain unchanged.
- **Suite limitation**: The full tooling suite reports six ranker/coddog snapshot or hazard-expectation
  failures; no goldens were regenerated or ranker implementation changed in this documentation work.
