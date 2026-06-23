/*
 * mips_as.h -- assembler build-time switches for the KMC hand-written MIPS
 * routines. Included by the library's .s sources, which select code paths with
 * .if on the symbols defined here.
 */
.equ FPU, 1 /* build for a hardware FPU (1) rather than software float (0) */
