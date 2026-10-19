# Instructions

Clean up the matched function `$INPUT`: improve its name, the names of the globals and struct fields it uses, its local variables and its structure so they reflect its purpose, while the ROM keeps matching.

Follow `docs/procedures/cleanup-function.md` (what to improve, safe refactoring, the 30-attempt give-up rule). Rename functions and globals with `docs/procedures/rename-global.md`. Verify with `./tools/build-and-verify.sh` and commit when done.

If the C function is not named `$INPUT`, it was renamed without updating `symbol_addrs.txt`; update that entry (a missing or wrong entry is the cause when `$INPUT` starts with `func_`).

# Use thinking
