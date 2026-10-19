# Cleanup Function

Look at `$INPUT[1]` in `$INPUT[0]`. Think about its purpose in the broader project and how it relates to its callers and callees, and what the original developers would have written: proper structs, proper struct and array accesses, proper variable names, clear function-local intent.

Follow `docs/procedures/cleanup-function.md` (problems list, safe refactoring, cross-file refactors, the 30-attempt give-up rule). Remove decompilation artefacts per `docs/reference/decompilation-artefacts.md` without breaking the match. Run `./tools/build-and-verify.sh` when done and commit.
