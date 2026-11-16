/*
 * The RSP's microcode: rspboot, which starts each task, the n_audio
 * microcode and F3DEX2's FIFO version. make extract cuts it from the
 * original ROM (see assets.txt). The RSP loads it by DMA from addresses
 * aligned to 8 bytes; the SDK's objects align each part to 16, as the
 * original's layout does.
 */

/* Defines NAMEStart and NAMEEnd around the file at PATH */
.macro microcode name, path
    .balign 16
    .global \name\()Start, \name\()End
    \name\()Start:
    .incbin "\path"
    \name\()End:
.endm

.section .text, "ax"

microcode rspbootText, "assets/microcode/rspboot_text.bin"
microcode n_aspMainText, "assets/microcode/n_aspMain_text.bin"
microcode gspF3DEX2_fifoText, "assets/microcode/gspF3DEX2_fifo_text.bin"

.section .data, "wa"

microcode n_aspMainData, "assets/microcode/n_aspMain_data.bin"
microcode gspF3DEX2_fifoData, "assets/microcode/gspF3DEX2_fifo_data.bin"
