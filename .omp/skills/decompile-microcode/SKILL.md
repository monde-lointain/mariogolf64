---
name: decompile-microcode
description: Use this skill if you need to decompile a function that builds an F3DEX2 display list. A `Gfx *`/`Gfx **` parameter or 8-byte command pairs written through a cursor are strongly indicative of display-list construction.
---

# Decompiling F3DEX2 Code

Mario Golf 64 runs the F3DEX2 microcode (`gspF3DEX2.fifo`), and game code is compiled with `-DF3DEX_GBI_2`. Display list commands are microcode that are passed to the RSP for rendering. They are all 8 bytes long.

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

Special care needs to be taken with functions constructing display lists as the original developer used specific macros from `include/libultra/PR/gbi.h`. These macro calls need to be replicated in order for the function to match.

The first step is to identify the commands in question, for example a naive decompilation might produce:

```
    temp_v1 = gfx;
    temp_a1 = (s32)arg0->unk30;
    temp_v1->words.w0 = 0xDA380003;
    temp_v1->words.w1 = temp_a1;
```

But we can see here that there's really just one F3DEX2 command. It starts with 0xDA380003 and uses temp_a1 as an argument. We can use `gfxdis.f3dex2` to help us identify what it is:

```
gfxdis.f3dex2 -d DA38000300000000
```

Which gives us:

```
gsSPMatrix(0x00000000, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
```

This is close to what we want. But it needs a few improvements:

1. We need the variant of this macro that also accepts a display list pointer: `gSPMatrix`.
2. We need to replace 0x00000000 with our actual variable: arg0->unk30.

This ultimately will give us:

```
gSPMatrix(gfx++, (Mtx*)arg0->unk30, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
```

Note the inline increment of the cursor, this is very common in commands dealing with F3DEX2 microcode.

The mapping may not always be this clear, however. You can consult @f3dex2-reference.md for a full list of F3DEX2 commands and `include/libultra/PR/gbi.h` for suitable macros to use. F3DEX (`0x01` matrix opcode) and F3DEX2 (`0xDA` matrix opcode) encode commands differently; MG64 game code is F3DEX2.

## Aggregate Macros

Be aware that some macros in `gbi.h` aggregate a number of regular F3DEX2 commands. You should always look for patterns of commands that could translate into one of these macros. The use of which is very likely to increase your match percentage. Common examples of this include:

* gDPLoadTLUT (and its variations gDPLoadTLUT*)
* gDPLoadTextureBlock (and its variations gDPLoadTextureBlock*)
* gDPSetCombineMode with `G_CC_*` presets

But there are many others.

## Permuter

The permuter may sometimes be helpful but it is not aware of the macros in gbi.h and will often try to replace them with their underlying commands. This is not what you want. Do not use its results if it requires you to remove gbi.h macros.

You can use the `--preserve-f3dex` argument when calling the permuter to explicitly prevent it from expanding macros. This can be useful for fixing register assignment issues on very close matches.
