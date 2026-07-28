#!/usr/bin/env bash
# Per-function instruction-stream comparator: the extracted .s vs a freshly built object.
#
# Usage: tools/cmpfn.sh [--mnemonics] <func> [<object>]
#   tools/cmpfn.sh func_80088890
#   tools/cmpfn.sh func_80088890 build/src/main/func_80080220.o
#   tools/cmpfn.sh --mnemonics func_8005C038
#
# `--mnemonics` diffs the OPCODE stream only, dropping every operand. Use it when the counts differ
# and the normal diff reports one huge replaced block: a register that plays a different role on the
# two sides makes every line differ, which hides WHERE the extra instructions are. With operands
# gone the alignment survives, and the surviving hunks localise the count mismatch to a few
# insertions and deletions (S289 `func_8005C038`: "+13, cause unknown" became "these two parameters
# are spilled and reloaded" in one command). It is strictly a localiser: a body that is clean here
# can still differ in every operand, so read the normal diff before believing anything.
#
# Why this exists (S258): `tools/asm-differ/diff.py` reads `build/*.map`, which an incremental
# per-object build does not refresh, so mid-iteration it goes STALE in BOTH directions (see
# `docs/agent-workflow.md` and the memory `subagent-diff-crack-not-a-bank`). This reads the object
# directly, so it never lies about the code you just compiled. It is an ITERATION oracle only —
# every bank still gates on `tools/verify-rom.sh` (full-make ROM SHA-1).
#
# Both sides are normalised so that only real differences show: `$` register prefixes, `%hi/%lo`
# relocations, immediates, `(0xX >> 16)` / `(0xX & 0xFFFF)` splat spellings, `addu rX,rY,zero` vs
# `move`, `addiu rX,zero,N` vs `li`, the SDK FP register aliases (`fv0`/`fs1` vs `f0`/`f22`), `fp` vs
# `s8`, and EXTERNAL branch/jal targets. The first output line is the instruction COUNT of each side,
# which is the most actionable number when a body is structurally right but the wrong length.
#
# `fp` and `s8` are the SAME register (`$30`): splat prints `fp`, objdump prints `s8`. Every function
# that uses the frame pointer as a general callee-saved register showed a false row per reference
# until S288, where 5 of the 7 remaining rows on an already byte-exact `func_8005B314` were this.
#
# The FP alias table is a full SDK-name-to-number MAP, not a collapse to one placeholder. It covers
# every `fv*`/`ft*`/`fa*`/`fs*` name, so an `fa0` line no longer reads as a difference against the
# same instruction spelled `f12` (S285: a byte-exact function showed 12 phantom rows that way). It
# also keeps a genuine FP register permutation VISIBLE, which the old five-name collapse to `fN` hid
# among `f0`/`f2`/`f20`/`f22`/`f24`.
#
# The splat-`ori` / objdump-`li` false row is FIXED (S300): the object side is disassembled with
# `-M no-aliases`, so both sides carry the real mnemonic and a matching unsigned load-immediate
# cancels, while a genuine `addiu`-vs-`ori` encoding difference stays visible. Folding the asm side
# instead, the shortcut this comment used to justify skipping, would have hidden that difference.
# Three such rows sat in func_8006BC80's residual and were counted as real.
#
# INTERNAL branch targets are NOT collapsed to a placeholder (S260). A `.L<vram>` (asm side) or a
# `<fn+0xNN>` (object side) target is rewritten to a POSITION-RELATIVE delta `@Dp<n>`/`@Dm<n>` (the
# signed distance in instructions from the branch to its target). This makes a redirected back edge
# VISIBLE: S260 `collect_keyframe_events_at` was byte-clean under the old placeholder normalisation
# (both sides read `T`) while the ROM branch went to the loop-top `lh` and the build's went one insn
# later to the `bnel`; that single-instruction redirect broke the full-make ROM and only
# `tools/verify-rom.sh` caught it. With the delta form the two edges now differ (`@Dm52` vs `@Dm51`).
# This remains an ITERATION oracle only — every bank still gates on `tools/verify-rom.sh`.
#
# The object is auto-resolved from the function's segment when not given. Rebuild it first:
#   find build -name '<obj>.o' -delete && make build/src/<tree>/<obj>.o

set -euo pipefail
source "$(dirname "$0")/lib.sh"

MNEMONICS=0
if [ "${1:-}" = "--mnemonics" ]; then
    MNEMONICS=1
    shift
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 [--mnemonics] <func> [<object>]" >&2
    exit 1
fi

FUNC="$1"
OBJ="${2:-}"

ASM_FILE=$(find asm/nonmatchings -name "${FUNC}.s" -print -quit)
if [ -z "$ASM_FILE" ]; then
    echo "$0: no asm/nonmatchings/**/${FUNC}.s (is the function already banked?)" >&2
    exit 1
fi

if [ -z "$OBJ" ]; then
    # asm/nonmatchings/<tree>/<stem>/<func>.s  ->  build/src/<tree>/<stem>.o
    STEM=$(dirname "${ASM_FILE#asm/nonmatchings/}")
    OBJ="build/src/${STEM}.o"
fi

if [ ! -f "$OBJ" ]; then
    echo "$0: $OBJ not built" >&2
    exit 1
fi

# --- Extract each side as one instruction per line, INTERNAL branch targets rewritten to a
#     signed position-relative delta (@Dp<n> / @Dm<n>). External targets are left as names so norm
#     collapses them to `T`. ---

# ROM side: two-pass awk over the .s (mawk-compatible: no 3-arg match / strtonum). Instruction lines
# are `/* off vram word */ mnem operands`; with default FS the vram is field 3. Internal targets are
# `.L<vram>` (vram embedded in the label). Pass 1 maps each instruction's vram string -> index; pass 2
# rewrites `.L<vram>` to a signed position delta and drops the comment.
rom_stream() {
    awk '
        FNR == NR {
            if ($1 == "/*" && $5 == "*/") { idx++; vram2idx[tolower($3)] = idx }
            next
        }
        {
            if ($1 != "/*" || $5 != "*/") next
            NR2++
            sub(/^.*\*\/[ \t]+/, "")
            if (match($0, /\.L[0-9A-Fa-f]+/)) {
                lab = substr($0, RSTART, RLENGTH)
                vr = tolower(substr(lab, 3))
                if (vr in vram2idx) {
                    d = vram2idx[vr] - NR2
                    tok = (d < 0) ? "@Dm" (-d) : "@Dp" d
                    $0 = substr($0, 1, RSTART - 1) tok substr($0, RSTART + RLENGTH)
                }
            }
            print
        }
    ' "$1" "$1"
}

# Object side: two-pass awk over the objdump slice (mawk-compatible). Instruction lines are
# `<hex>:  <word>  mnem operands [<addr> <sym>]`. Pass 1 maps each instruction's absolute hex address
# string -> index. Pass 2 strips the addr+word prefix; a trailing `<addr> <fn...>` referencing THIS
# function is rewritten to the same position delta (so an internal branch is position-relative), and
# any other trailing `<addr> <sym>` (an external call/tail-jump) is collapsed to `T` to match norm.
obj_stream() {
    local slice
    slice=$(mktemp)
    # -z / --disassemble-zeroes: without it objdump COLLAPSES runs of identical zero words
    # (consecutive `nop`s) into a single `...` line, so cmpfn undercounts the instruction total and
    # reports a byte-EXACT function as short (S275: update_putting_meter read 107/112 with 2 mflo-latency
    # nops hidden as `...`, nearly abandoned as a sched-coin carry). -z forces every nop to disassemble.
    # -M no-aliases: objdump renders BOTH `addiu rX,zero,N` and `ori rX,zero,N` as `li rX,N`, while
    # splat prints the real mnemonic, so a matching unsigned load-immediate used to show as a
    # permanent false row (S300, three of them in func_8006BC80's residual). Asking for the real
    # mnemonics makes both sides agree AND keeps a genuine addiu-vs-ori encoding difference visible,
    # which the old fold-the-asm-side-too shortcut would have hidden. The object-side alias table in
    # norm() folds what no-aliases spells differently from splat (`sll zero,zero,0` -> nop,
    # `beq zero,zero` -> b, `subu rd,zero,rs` -> negu).
    mips-linux-gnu-objdump -dz -M no-aliases "$OBJ" | awk "/<${FUNC}>:/,/^\$/" > "$slice"
    awk -v fn="$FUNC" '
        FNR == NR {
            if (match($0, /^[ \t]+[0-9a-f]+:/)) {
                s = substr($0, RSTART, RLENGTH)
                gsub(/[ \t:]/, "", s)
                idx++; addr2idx[tolower(s)] = idx
            }
            next
        }
        {
            if (!match($0, /^[ \t]+[0-9a-f]+:/)) next
            NR2++
            line = $0
            sub(/^[ \t]+[0-9a-f]+:[ \t]+[0-9a-f]+[ \t]+/, "", line)
            # trailing "<targetaddr> <sym>"
            if (match(line, /[0-9a-f]+ <[^>]*>[ \t]*$/)) {
                tail = substr(line, RSTART, RLENGTH)
                head = substr(line, 1, RSTART - 1)
                # target address = leading hex of the tail
                ta = tail; sub(/ .*$/, "", ta); ta = tolower(ta)
                internal = (index(tail, "<" fn ">") > 0) || (index(tail, "<" fn "+") > 0)
                if (internal && (ta in addr2idx)) {
                    d = addr2idx[ta] - NR2
                    tok = (d < 0) ? "@Dm" (-d) : "@Dp" d
                    line = head tok
                } else {
                    line = head "T"
                }
            }
            print line
        }
    ' "$slice" "$slice"
    rm -f "$slice"
}

# Trailing PADDING nops on the object side are not part of the function (S292). The linker pads a
# function up to its alignment boundary, and `-dz` faithfully disassembles that padding, so the
# object stream runs long: `func_8005D334` read 36 against the ROM's 33 (3 pad nops) and
# `func_8005DFE8` 104 against 102 (2). That is worse than a cosmetic miscount, because
# `docs/workflow/loop.md ## Oracles` routes a differing count to "structural deficit, fix that
# first" -- so a body already at exact count sends the whole diagnosis at a deficit that does not
# exist. This is the mirror of the `-dz` UNDERcount above: same stream, opposite direction.
#
# The trim is deliberately conservative, because the nop in a `jr $ra` delay slot IS part of the
# function and must survive. Only nops beyond that delay slot are dropped, and only when every one
# of them is a nop; anything else keeps the stream whole rather than masking real instructions. The
# .s side never needs this -- splat carves it at the function's exact size.
trim_trailing_pad() {
    awk '
        { line[NR] = $0 }
        END {
            last_jr = 0
            for (i = 1; i <= NR; i++)
                if (line[i] ~ /^[ \t]*jr[ \t]+\$?ra[ \t]*$/) last_jr = i
            keep = NR
            if (last_jr > 0 && NR > last_jr + 1) {
                pad_only = 1
                for (i = last_jr + 2; i <= NR; i++)
                    # `-M no-aliases` spells a nop as `sll zero,zero,0x0` (S300), and this trim runs
                    # before norm() folds it back, so match both spellings.
                    if (line[i] !~ /^[ \t]*nop[ \t]*$/ &&
                        line[i] !~ /^[ \t]*sll[ \t]+zero,zero,0x0[ \t]*$/) pad_only = 0
                if (pad_only) keep = last_jr + 1
            }
            for (i = 1; i <= keep; i++) print line[i]
        }
    '
}

norm() {
    # Stack-slot displacements survive the immediate mask (S296). A frame-slot offset is a real
    # difference, unlike a reloc immediate: S296 `update_vertex_texture_coords_per_frame` read
    # byte-clean here at 244/244 with a MATCHING frame size while the ROM build failed, because the
    # frame-pinned local sat at 0xd8(sp) against the ROM's 0x58(sp). `diff.py` after a full make
    # found it in one line. This is the third normalisation to need surfacing (S260 branch targets,
    # S276 frame size), and stack offsets are the one immediate class known to be meaningful.
    # Both sides fold to decimal: splat prints `0x58($sp)`, objdump prints `88(sp)` for that slot.
    # The marker is glued to letters (`@F88`) so the later `\b[0-9]+\b` mask cannot reach the digits.
    awk '
        function h2d(s,   i, c, v, neg, digits) {
            neg = 0
            if (substr(s, 1, 1) == "-") { neg = 1; s = substr(s, 2) }
            if (tolower(substr(s, 1, 2)) == "0x") {
                s = substr(s, 3); v = 0; digits = "0123456789abcdef"
                for (i = 1; i <= length(s); i++) {
                    c = tolower(substr(s, i, 1))
                    v = v * 16 + (index(digits, c) - 1)
                }
            } else {
                v = s + 0
            }
            return neg ? -v : v
        }
        {
            out = ""
            while (match($0, /(0x[0-9a-fA-F]+|-?[0-9]+)\(\$?sp\)/)) {
                tok = substr($0, RSTART, RLENGTH)
                sub(/\(\$?sp\)/, "", tok)
                out = out substr($0, 1, RSTART - 1) "@F" h2d(tok) "(sp)"
                $0 = substr($0, RSTART + RLENGTH)
            }
            print out $0
        }
    ' |
    sed -E '
        s/\$//g
        s/%[a-z]+\([^)]*\)/N/g
        s/<[^>]*>//g
        s/0x[0-9a-fA-F]+/N/g
        s/\b[0-9]+\b/N/g
        s/\(N >> N\)/N/g
        s/\(N & N\)/N/g
        s/[[:space:]]+/ /g
        s/, /,/g
        s/^ //
        s/ $//
        s/\b(addu|or) ([a-z0-9]+),([a-z0-9]+),zero/move \2,\3/
        s/\bsll zero,zero,N$/nop/
        s/\bbeq zero,zero,/b /
        s/\bsubu ([a-z0-9]+),zero,/negu \1,/
        s/\baddiu ([a-z0-9]+),zero,/li \1,/
        s/\bbeq ([a-z0-9]+),zero,/beqz \1,/
        s/\bbne ([a-z0-9]+),zero,/bnez \1,/
        s/\bbeql ([a-z0-9]+),zero,/beqzl \1,/
        s/\bbnel ([a-z0-9]+),zero,/bnezl \1,/
        s/\b(jal|j|b[a-z]*) [.A-Za-z_][A-Za-z0-9_.]*$/\1 T/
        s/\bfp\b/s8/g
        s/\bfv0\b/f0/g
        s/\bfv1\b/f2/g
        s/\bft0\b/f4/g
        s/\bft1\b/f6/g
        s/\bft2\b/f8/g
        s/\bft3\b/f10/g
        s/\bfa0\b/f12/g
        s/\bfa1\b/f14/g
        s/\bft4\b/f16/g
        s/\bft5\b/f18/g
        s/\bfs0\b/f20/g
        s/\bfs1\b/f22/g
        s/\bfs2\b/f24/g
        s/\bfs3\b/f26/g
        s/\bfs4\b/f28/g
        s/\bfs5\b/f30/g
        s/(b[a-z]+ [a-z0-9,]*),[.A-Za-z0-9_]+$/\1,T/
    '
}

ROM_N=$(mktemp)
MINE_N=$(mktemp)
ROM_RAW=$(mktemp)
MINE_RAW=$(mktemp)
trap 'rm -f "$ROM_N" "$MINE_N" "$ROM_RAW" "$MINE_RAW"' EXIT

rom_stream "$ASM_FILE" > "$ROM_RAW"
MINE_UNTRIMMED=$(mktemp)
obj_stream > "$MINE_UNTRIMMED"
trim_trailing_pad < "$MINE_UNTRIMMED" > "$MINE_RAW"
PAD=$(( $(wc -l < "$MINE_UNTRIMMED") - $(wc -l < "$MINE_RAW") ))
rm -f "$MINE_UNTRIMMED"
norm < "$ROM_RAW" > "$ROM_N"
norm < "$MINE_RAW" > "$MINE_N"

# Prologue stack-frame immediate. norm() masks every immediate (0xNN -> N), so a frame-size
# mismatch (e.g. a dead-frame reserve the ROM keeps) is otherwise INVISIBLE in the diff and a
# genuine near-miss reads as "register-permutation only" (S276 func_800990D0: 0x48 mine vs 0x50
# ROM, caught only by binutils cross-check). Surface both sides, canonicalised to hex, and flag a
# mismatch in the summary line.
frame_imm() {
    local raw neg=''
    raw=$(grep -oiE 'addiu[[:space:]]+\$?sp,[[:space:]]*\$?sp,[[:space:]]*-?(0x)?[0-9a-f]+' "$1" \
          | head -1 | grep -oiE '\-?(0x)?[0-9a-f]+$')
    [ -z "$raw" ] && { printf '?'; return; }
    case "$raw" in -*) neg='-'; raw=${raw#-};; esac
    printf '%s0x%x' "$neg" "$((raw))"
}
RF=$(frame_imm "$ROM_RAW"); MF=$(frame_imm "$MINE_RAW")
FRAME="frame rom=$RF mine=$MF"
[ "$RF" != "$MF" ] && FRAME="$FRAME <-- FRAME MISMATCH"

PADNOTE=''
[ "$PAD" -gt 0 ] && PADNOTE="  [+$PAD pad nop(s) trimmed]"
echo "rom=$(wc -l < "$ROM_N") mine=$(wc -l < "$MINE_N")  ($OBJ)  [$FRAME]$PADNOTE"

# Mnemonic histogram delta. A multiset comparison says whether the residual is STRUCTURAL and of
# what kind before any hunk is read: `slti`+`bnez` against `bne` names a loop-exit form, a `div` /
# `break` / `mflo` group naming a duplicated division, `bc1t`+`and` against `bc1f`+`or` naming a
# De Morgan inversion (S305, all three). It is computed off the NORMALISED streams, so the alias
# spellings splat and objdump disagree on (`li` for both `addiu rX,zero,N` and `ori rX,zero,N`,
# `move`/`addu`) are already folded — a hand-rolled histogram over the raw text reports those as
# false deltas. Printed only when the multisets differ; it says nothing about WHERE.
hist() { cut -d' ' -f1 "$1" | sort | uniq -c | awk '{ print $2, $1 }' | sort; }
HDELTA=$(join -a1 -a2 -e0 -o '0,1.2,2.2' <(hist "$ROM_N") <(hist "$MINE_N") \
         | awk '$2 != $3 { printf "  %-10s rom=%s mine=%s\n", $1, $2, $3 }')
if [ -n "$HDELTA" ]; then
    echo "--- mnemonic delta ---"
    printf '%s\n' "$HDELTA"
fi

if [ "$MNEMONICS" = 1 ]; then
    # Opcode stream only. norm() has already folded the alias spellings (move/li/beqz/bnezl), so both
    # sides name the same instruction the same way and the alignment is meaningful.
    echo "--- opcode stream only (localiser; operands dropped) ---"
    diff -u <(cut -d' ' -f1 "$ROM_N") <(cut -d' ' -f1 "$MINE_N") \
        | tail -n +3 | grep -vE '^ ' || true
    exit 0
fi

diff <(cat -n "$ROM_N") <(cat -n "$MINE_N") || true
