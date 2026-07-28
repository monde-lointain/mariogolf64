#!/usr/bin/env python3
"""Decode an RDP SETCOMBINE word pair into gDPSetCombineLERP arguments."""
import re
import sys

GBI = "include/libultra/PR/gbi.h"
cc = {}
ac = {}
for line in open(GBI):
    m = re.match(r"#define\s+G_CCMUX_(\w+)\s+(\w+)", line)
    if m:
        cc.setdefault(int(m.group(2), 0), m.group(1))
    m = re.match(r"#define\s+G_ACMUX_(\w+)\s+(\w+)", line)
    if m:
        ac.setdefault(int(m.group(2), 0), m.group(1))


def dec(w0, w1):
    a0 = (w0 >> 20) & 0xF
    c0 = (w0 >> 15) & 0x1F
    Aa0 = (w0 >> 12) & 0x7
    Ac0 = (w0 >> 9) & 0x7
    a1 = (w0 >> 5) & 0xF
    c1 = (w0 >> 0) & 0x1F
    b0 = (w1 >> 28) & 0xF
    b1 = (w1 >> 24) & 0xF
    Aa1 = (w1 >> 21) & 0x7
    Ac1 = (w1 >> 18) & 0x7
    d0 = (w1 >> 15) & 0x7
    Ab0 = (w1 >> 12) & 0x7
    Ad0 = (w1 >> 9) & 0x7
    d1 = (w1 >> 6) & 0x7
    Ab1 = (w1 >> 3) & 0x7
    Ad1 = (w1 >> 0) & 0x7
    g = lambda t, v: t.get(v, str(v))
    return "gDPSetCombineLERP(p, %s, %s, %s, %s,  %s, %s, %s, %s,  %s, %s, %s, %s,  %s, %s, %s, %s)" % (
        g(cc, a0), g(cc, b0), g(cc, c0), g(cc, d0),
        g(ac, Aa0), g(ac, Ab0), g(ac, Ac0), g(ac, Ad0),
        g(cc, a1), g(cc, b1), g(cc, c1), g(cc, d1),
        g(ac, Aa1), g(ac, Ab1), g(ac, Ac1), g(ac, Ad1))


for pair in sys.argv[1:]:
    w0, w1 = pair.split("/")
    print("%s -> %s" % (pair, dec(int(w0, 16), int(w1, 16))))
