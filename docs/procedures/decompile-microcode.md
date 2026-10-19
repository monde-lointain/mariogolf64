---
type: Procedure
name: decompile-microcode
title: "Decompile F3DEX2 display-list code"
description: "Use this skill if you need to decompile a function that builds an F3DEX2 display list. A `Gfx *`/`Gfx **` parameter or 8-byte command pairs written through a cursor are strongly indicative of display-list construction."
tags: [decompile, graphics, f3dex2]
---

Mario Golf 64 runs the F3DEX2 microcode (`gspF3DEX2.fifo`); `src/main` and libultra compile with `-DF3DEX_GBI_2` ([profiles](/project/compiler-profiles.md)). Display-list commands are 8 bytes each. Use the macros in `include/libultra/PR/gbi.h`; raw command words are prohibited ([code quality](/project/code-quality.md)).

Display lists are usually built through a local cursor loaded from a `Gfx **` argument and stored back at the end:

```c
void emit_sky_horizon_compositor_dl(Gfx** gfxp) {
  Gfx* gfx = *gfxp;

  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_COPY);
  ...
  *gfxp = gfx;
}
```

# Identify the commands

A naive decompilation shows raw words:

```c
temp_v1 = gfx;
temp_a1 = (s32)arg0->unk30;
temp_v1->words.w0 = 0xDA380003;
temp_v1->words.w1 = temp_a1;
```

That is one F3DEX2 command: the opcode word `0xDA380003` with `temp_a1` as its argument. `gfxdis.f3dex2` (external tool) names it:

```
gfxdis.f3dex2 -d DA38000300000000
→ gsSPMatrix(0x00000000, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
```

Then:

1. Use the `g` variant that takes a display-list pointer (`gSPMatrix`, not `gsSPMatrix`).
2. Substitute the real argument for the placeholder.

```c
gSPMatrix(gfx++, (Mtx*)arg0->unk30, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
```

Advance the cursor inline with `gfx++`; never manipulate it by hand. F3DEX (matrix opcode `0x01`) and F3DEX2 (`0xDA`) encode commands differently; this game is F3DEX2. The full command list is in [F3DEX2 commands](/reference/f3dex2-commands.md).

# Aggregate macros

Some `gbi.h` macros expand to several commands. Look for these patterns first; using them usually raises the match score considerably:

* `gDPLoadTLUT` and its `gDPLoadTLUT_*` variants
* `gDPLoadTextureBlock` and its variants, `gDPLoadTextureTile_4b`
* `gDPSetCombineMode` with `G_CC_*` presets
