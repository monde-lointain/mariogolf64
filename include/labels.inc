# This file is used by the original compiler/assembler.
# Defines the expected assembly macros.
#
# The file has no blank lines on purpose. KMC as scans each line for
# its VR4300 multiplication fix, and on a blank line that follows a
# line starting in column 0 it reads output it has not yet written.
# Leftover heap bytes there can then make it insert a stray nop at the
# start of the including file's .text.
# A function symbol.
.macro glabel label, visibility=global
    .\visibility \label
    .type \label, @function
    \label:
        .ent \label
.endm
# The end of a function symbol.
.macro endlabel label
    .size \label, . - \label
    .end \label
.endm
# An alternative entry to a function.
.macro alabel label, visibility=global
    .\visibility \label
    .type \label, @function
    \label:
        .aent \label
.endm
# A label referenced by an error handler table.
.macro ehlabel label, visibility=global
    .\visibility \label
    \label:
.endm
# A label referenced by a jumptable.
.macro jlabel label, visibility=local
    .\visibility \label
    \label:
.endm
# A data symbol.
.macro dlabel label, visibility=global
    .\visibility \label
    .type \label, @object
    \label:
.endm
# End of a data symbol.
.macro enddlabel label
    .size \label, . - \label
.endm
# Label to signal the symbol haven't been matched yet.
.macro nonmatching label, size=1
    .global \label\().NON_MATCHING
    .type \label\().NON_MATCHING, @object
    .size \label\().NON_MATCHING, \size
    \label\().NON_MATCHING:
.endm
