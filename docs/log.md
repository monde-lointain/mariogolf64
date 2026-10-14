# Documentation update log

## 2026-10-14

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
