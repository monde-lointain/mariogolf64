#!/usr/bin/env bash
# Shared helpers for the coddog / nusys reference sweeps (coddog_sweep.sh,
# build_nusys_ref.sh). Source from a script that has already set REPO:
#   source "$REPO/tools/sweep_lib.sh"

# KMC objects carry non-standard ECOFF/debug sections (.mdebug*, .reginfo, .pdr,
# .comment) that GNU ld rejects; `objcopy "${MG_KMC_STRIP_SECTIONS[@]}"` strips
# them and normalizes the symtab to GNU order. The SAME set is used by the
# ultralib sweep and the nusys reference build (keep it one source of truth).
MG_KMC_STRIP_SECTIONS=(-R .mdebug -R .mdebug.abi32 -R .reginfo -R .pdr -R .comment)
