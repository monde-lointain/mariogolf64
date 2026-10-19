.set noat
.set noreorder
.set gp=64
.include "macro.inc"
# The target is one function, so jlabel labels stay local. macro.inc makes them global, and the
# assembler then leaves a branch to them unresolved (R_MIPS_PC16) where the linked ROM has it resolved.
.purgem jlabel
.macro jlabel label
    \label:
.endm
