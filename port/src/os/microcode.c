/*
 * The RSP's microcode, which the port doesn't run: the RSP's work happens
 * on the host instead. The game still names the boot code and the audio
 * microcode when it fills an audio task, so they exist, empty, for it to
 * point at.
 */

#include <PR/n_libaudio_sc.h>
#include <PR/ucode.h>
#include <ultra64.h>

long long int rspbootTextStart[1];
long long int rspbootTextEnd[1];
long long int n_aspMainTextStart[1];
long long int n_aspMainTextEnd[1];
