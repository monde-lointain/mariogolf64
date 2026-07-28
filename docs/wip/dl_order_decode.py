#!/usr/bin/env python3
"""Walk a splat .s: constant-propagate registers + $sp slots, and track the
display-list write pointer symbolically, so stores can be re-sorted into DL order."""
import re
import sys

path = sys.argv[1]
INSN = re.compile(r"/\* \w+ (\w{8}) (\w{8}) \*/\s+(\S+)\s*(.*)")


class DL(object):
    __slots__ = ("off",)

    def __init__(self, off):
        self.off = off

    def __repr__(self):
        return "DL+%d" % self.off


reg = {"$zero": 0}
DLBASE = None
slot = {}
out = []   # (dl_offset, word_index, value, addr)
notes = []


def fmt(v):
    if v is None:
        return "?"
    if isinstance(v, DL):
        return repr(v)
    if isinstance(v, str):
        return v
    return "0x%08X" % (v & 0xFFFFFFFF)


for line in open(path):
    t = line.strip()
    m = INSN.search(line)
    if not m:
        if t.startswith(".L") or t.startswith("glabel"):
            out.append((None, None, "LABEL %s" % t, 0))
        continue
    addr = int(m.group(1), 16)
    op = m.group(3)
    args = [a.strip() for a in m.group(4).split(",")]
    try:
        if op == "lui":
            mm = re.search(r"\(([^)]*) >> 16\)", args[1])
            reg[args[0]] = (int(mm.group(1), 0) & 0xFFFF0000) if mm else args[1]
        elif op == "ori":
            mm = re.search(r"\(([^)]*) & 0xFFFF\)", args[2])
            b = reg.get(args[1])
            reg[args[0]] = (b | (int(mm.group(1), 0) & 0xFFFF)) & 0xFFFFFFFF \
                if (mm and isinstance(b, int)) else "%s|%s" % (fmt(b), args[2])
        elif op == "addiu":
            b = reg.get(args[1])
            k = int(args[2], 0)
            if args[1] == "$zero":
                reg[args[0]] = k & 0xFFFFFFFF
            elif isinstance(b, DL):
                reg[args[0]] = DL(b.off + k)
            elif isinstance(b, int):
                reg[args[0]] = (b + k) & 0xFFFFFFFF
            else:
                reg[args[0]] = "%s+%s" % (fmt(b), args[2])
        elif op == "addu":
            a, b = reg.get(args[1]), reg.get(args[2])
            if isinstance(a, DL) and b == 0:
                reg[args[0]] = DL(a.off)
            elif isinstance(b, DL) and a == 0:
                reg[args[0]] = DL(b.off)
            elif isinstance(a, int) and isinstance(b, int):
                reg[args[0]] = (a + b) & 0xFFFFFFFF
            else:
                reg[args[0]] = "<addu %s %s>" % (fmt(a), fmt(b))
        elif op == "sw":
            src, dst = args[0], args[1]
            mm = re.match(r"(-?0x[0-9A-Fa-f]+|0)\(\$sp\)$", dst)
            if mm:
                slot[int(mm.group(1), 0)] = reg.get(src)
                continue
            mm = re.match(r"(-?0x[0-9A-Fa-f]+|0)\((\$\w+)\)$", dst)
            if mm:
                base = reg.get(mm.group(2))
                d = int(mm.group(1), 0)
                if isinstance(base, DL):
                    out.append((base.off + d, addr, reg.get(src), addr))
                    continue
            out.append((None, addr, "ST %s <- %s" % (dst, fmt(reg.get(src))), addr))
        elif op == "lw":
            mm = re.match(r"(-?0x[0-9A-Fa-f]+|0)\(\$sp\)$", args[1])
            reg[args[0]] = slot.get(int(mm.group(1), 0)) if mm else "<mem %s>" % args[1]
            if addr == 0x8002CDE4:
                reg[args[0]] = DL(0)
        elif op == "jal":
            out.append((None, addr, "JAL %s (a0=%s)" % (args[0], fmt(reg.get("$a0"))), addr))
        elif op in ("beq", "bne", "blez", "bgez", "bgtz", "bltz", "beqz", "bnez", "j", "b"):
            out.append((None, addr, "BR %s %s" % (op, " ".join(args)), addr))
        elif op in ("or", "and"):
            a = reg.get(args[1])
            b = reg.get(args[2]) if args[2].startswith("$") else int(args[2], 0)
            if isinstance(a, int) and isinstance(b, int):
                reg[args[0]] = (a | b) if op == "or" else (a & b)
            else:
                reg[args[0]] = "(%s %s %s)" % (fmt(a), op, fmt(b) if not isinstance(b, int) else hex(b))
        else:
            if args and args[0].startswith("$"):
                reg[args[0]] = "<%s %s>" % (op, ",".join(args[1:]))
    except Exception as exc:
        out.append((None, addr, "?? %s %s (%s)" % (op, args, exc), addr))

# print in DL order where known, keeping unknown entries inline by address
known = [(o, a, v) for (o, a, v, _) in out if o is not None]
known.sort(key=lambda x: (x[0], x[1]))
print("=== DL-ordered word stream ===")
for o, a, v in known:
    print("+%-5d (%08X) %s" % (o, a, fmt(v)))
print()
print("=== non-DL events, program order ===")
for o, a, v, _ in out:
    if o is None:
        print("%08X %s" % (a, v))
