#!/usr/bin/env python3
"""Decode the RDP command words this project's emitters use."""
import sys

FMT = {0: "RGBA", 1: "YUV", 2: "CI", 3: "IA", 4: "I"}
SIZ = {0: "4b", 1: "8b", 2: "16b", 3: "32b"}


def dec(w0, w1):
    cmd = (w0 >> 24) & 0xFF
    if cmd == 0xF5:  # SetTile
        fmt = (w0 >> 21) & 7
        siz = (w0 >> 19) & 3
        line = (w0 >> 9) & 0x1FF
        tmem = w0 & 0x1FF
        tile = (w1 >> 24) & 7
        pal = (w1 >> 20) & 0xF
        cmT = (w1 >> 18) & 3
        maskT = (w1 >> 14) & 0xF
        shiftT = (w1 >> 10) & 0xF
        cmS = (w1 >> 8) & 3
        maskS = (w1 >> 4) & 0xF
        shiftS = w1 & 0xF
        return ("gDPSetTile(fmt=%s siz=%s line=%d tmem=%d tile=%d pal=%d "
                "cmT=%d maskT=%d shiftT=%d cmS=%d maskS=%d shiftS=%d)" %
                (FMT.get(fmt, fmt), SIZ.get(siz, siz), line, tmem, tile, pal,
                 cmT, maskT, shiftT, cmS, maskS, shiftS))
    if cmd == 0xF2:  # SetTileSize
        uls = (w0 >> 12) & 0xFFF
        ult = w0 & 0xFFF
        tile = (w1 >> 24) & 7
        lrs = (w1 >> 12) & 0xFFF
        lrt = w1 & 0xFFF
        return ("gDPSetTileSize(tile=%d uls=%d(%.2f) ult=%d(%.2f) lrs=%d(%.2f) lrt=%d(%.2f))" %
                (tile, uls, uls / 4.0, ult, ult / 4.0, lrs, lrs / 4.0, lrt, lrt / 4.0))
    if cmd == 0xF4:  # LoadTile
        uls = (w0 >> 12) & 0xFFF
        ult = w0 & 0xFFF
        tile = (w1 >> 24) & 7
        lrs = (w1 >> 12) & 0xFFF
        lrt = w1 & 0xFFF
        return ("gDPLoadTile(tile=%d uls=%.2f ult=%.2f lrs=%.2f lrt=%.2f)" %
                (tile, uls / 4.0, ult / 4.0, lrs / 4.0, lrt / 4.0))
    if cmd == 0xF3:  # LoadBlock
        uls = (w0 >> 12) & 0xFFF
        ult = w0 & 0xFFF
        tile = (w1 >> 24) & 7
        lrs = (w1 >> 12) & 0xFFF
        dxt = w1 & 0xFFF
        return "gDPLoadBlock(tile=%d uls=%d ult=%d lrs=%d dxt=%d)" % (tile, uls, ult, lrs, dxt)
    if cmd in (0xFD, 0xFF):  # SetTextureImage / SetColorImage
        fmt = (w0 >> 21) & 7
        siz = (w0 >> 19) & 3
        width = (w0 & 0xFFF) + 1
        name = "gDPSetTextureImage" if cmd == 0xFD else "gDPSetColorImage"
        return "%s(fmt=%s siz=%s width=%d addr=0x%08X)" % (name, FMT.get(fmt, fmt), SIZ.get(siz, siz), width, w1)
    if cmd == 0xE4:  # TextureRectangle
        xh = (w0 >> 12) & 0xFFF
        yh = w0 & 0xFFF
        tile = (w1 >> 24) & 7
        xl = (w1 >> 12) & 0xFFF
        yl = w1 & 0xFFF
        return ("gSPTextureRectangle(xl=%.2f yl=%.2f xh=%.2f yh=%.2f tile=%d)" %
                (xl / 4.0, yl / 4.0, xh / 4.0, yh / 4.0, tile))
    if cmd == 0xE1:
        return "G_RDPHALF_1(s=%d t=%d)" % ((w1 >> 16) & 0xFFFF, w1 & 0xFFFF)
    if cmd == 0xF1:
        return "G_RDPHALF_2(dsdx=%d dtdy=%d)" % ((w1 >> 16) & 0xFFFF, w1 & 0xFFFF)
    if cmd == 0xED:
        return ("gDPSetScissor(mode=%d ulx=%.2f uly=%.2f lrx=%.2f lry=%.2f)" %
                ((w1 >> 24) & 3, ((w0 >> 12) & 0xFFF) / 4.0, (w0 & 0xFFF) / 4.0,
                 ((w1 >> 12) & 0xFFF) / 4.0, (w1 & 0xFFF) / 4.0))
    if cmd == 0xFA:
        return "gDPSetPrimColor(m=%d l=%d r=%d g=%d b=%d a=%d)" % (
            (w0 >> 8) & 0xFF, w0 & 0xFF, (w1 >> 24) & 0xFF, (w1 >> 16) & 0xFF,
            (w1 >> 8) & 0xFF, w1 & 0xFF)
    return "cmd %02X w0=%08X w1=%08X" % (cmd, w0, w1)


for pair in sys.argv[1:]:
    a, b = pair.split("/")
    print("%s  ->  %s" % (pair, dec(int(a, 16), int(b, 16))))
