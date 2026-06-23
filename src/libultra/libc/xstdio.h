/*
 * Internal interface shared by the formatted-output engine (_Printf) and its
 * integer/float converters (_Litob, _Ldtob): the per-conversion descriptor
 * _Pft, the conversion-flag bits, and the converter prototypes.
 */

#ifndef _XSTDIO_H
#define _XSTDIO_H
#include "stdlib.h"
#include "stdarg.h"

// The host's widest float. On SGI hardware long double is the same 64-bit
// double, so alias it to keep the converter math identical across hosts.
#ifdef __sgi
typedef double ldouble;
#else
typedef long double ldouble;
#endif

/*
 * Descriptor for one %-conversion, filled in by _Printf and the converters.
 * The output of a conversion is laid out as up to six contiguous runs that the
 * emitter pads and writes in order: sign/prefix (n0), leading zeros (nz0),
 * first digit group (n1), zero pad (nz1), second digit group (n2), trailing
 * zero pad (nz2). The converters compute these run lengths; _Printf emits them.
 */
typedef struct {
  union {
    long long ll;  // the argument when an integer conversion
    ldouble ld;    // the argument when a floating conversion
  } v;
  unsigned char* s;    // start of the formatted digits within the work buffer
  int n0;              // count: sign/base-prefix characters
  int nz0;             // count: leading zeros before the digits
  int n1;              // count: characters in the first digit run
  int nz1;             // count: zero padding after the first run
  int n2;              // count: characters in the second digit run
  int nz2;             // count: trailing zero padding
  int prec;            // requested precision, or -1 when none was given
  int width;           // requested minimum field width
  size_t nchar;        // running total of characters emitted so far
  unsigned int flags;  // FLAGS_* bits parsed from the conversion's flag chars
  char qual;           // length modifier: 'h', 'l', 'L', or '\0'
} _Pft;

// Conversion flags, one bit per printf flag character.
// clang-format off: keep the bit values in an aligned column
#define FLAGS_SPACE 1   // ' ': blank before a non-negative number
#define FLAGS_PLUS  2   // '+': force a sign on a non-negative number
#define FLAGS_MINUS 4   // '-': left-justify within the field width
#define FLAGS_HASH  8   // '#': alternate form (0/0x prefix, force point)
#define FLAGS_ZERO  16  // '0': pad with zeros instead of spaces
// clang-format on

// The shared core and the two number-to-text converters.
int _Printf(void* pfn(void*, const char*, size_t), void* arg, const char* fmt,
            va_list ap);
void _Litob(_Pft* px, char code);
void _Ldtob(_Pft* px, char code);
#endif
