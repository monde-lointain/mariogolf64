/*
 * _Printf: the shared core of the printf family. It walks the format string,
 * parses each conversion's flags/width/precision/length, dispatches to a
 * converter for the argument, then emits the field with the requested padding
 * through a caller-supplied output sink (pfn). sprintf and the debug-console
 * printers all funnel through here.
 */

#include "PRinternal/macros.h"
#include "string.h"
#include "stdarg.h"
#include "xstdio.h"
#ident "$Revision: 1.34 $"
#ident "$Revision: 1.5 $"
#ident "$Revision: 1.23 $"

// Local classifier so the parser does not depend on locale or <ctype.h>.
#define isdigit(x) ((x >= '0' && x <= '9'))

/*
 * Sign bit of a long double, read from its most significant 16-bit word. Used
 * to detect negative zero, which a plain `< 0` test would miss.
 */
#define LDSIGN(x) (((unsigned short*)&(x))[0] & 0x8000)

/*
 * Parse a run of decimal digits at src into dst, advancing src past them. The
 * running value is clamped at 999 so a pathological width/precision can't
 * overflow.
 */
#define ATOI(dst, src)                          \
  for (dst = 0; isdigit(*src); ++src) {         \
    if (dst < 999) dst = dst * 10 + *src - '0'; \
  }

// Longest padding run available from one pad string.
#define MAX_PAD ((sizeof(spaces) - 1))

/*
 * Emit n pad characters from the constant string s, looping in MAX_PAD-sized
 * chunks because the pad strings are only 32 bytes long.
 */
#define PAD(s, n)                                       \
  if (0 < (n)) {                                        \
    int i, j = (n);                                     \
    for (; 0 < j; j -= i) {                             \
      i = MAX_PAD < (unsigned int)j ? (int)MAX_PAD : j; \
      PUT(s, i);                                        \
    }                                                   \
  }

/*
 * Push n bytes from s through the output sink. On a sink failure (NULL return)
 * bail out of the whole conversion, returning the count emitted so far.
 */
#define PUT(s, n)                          \
  if (0 < (n)) {                           \
    if ((arg = (*pfn)(arg, s, n)) != NULL) \
      x.nchar += (n);                      \
    else                                   \
      return x.nchar;                      \
  }

// Reusable padding sources, sized to MAX_PAD characters apiece.
static char spaces[] = "                                ";
static char zeroes[] = "00000000000000000000000000000000";

static void _Putfld(_Pft* px, va_list* pap, char code, char* ac);

/*
 * Format fmt with the arguments in ap, sending output through pfn (which is
 * handed arg as its first parameter and the byte run as the rest). Returns the
 * number of characters emitted, or stops early and returns the partial count
 * if the sink ever fails.
 */
int _Printf(void* pfn(void*, const char*, size_t), void* arg, const char* fmt,
            va_list ap) {
  _Pft x;

  x.nchar = 0;
  while (1) {
    const char* s;
    char c;
    const char* t;

    // Flag-character table, parallel to its bit table: the index of a flag
    // char in fchar selects its FLAGS_* bit in fbit.
    static const char fchar[] = {' ', '+', '-', '#', '0', '\0'};
    static const unsigned int fbit[] = {FLAGS_SPACE, FLAGS_PLUS, FLAGS_MINUS,
                                        FLAGS_HASH,  FLAGS_ZERO, 0};

    // Per-conversion work buffer holding the sign/prefix and digit text.
    char ac[32];

    // Copy the literal run up to the next '%' (or end of string) verbatim.
    s = fmt;
    for (c = *s; c != 0 && c != '%';) {
      c = *++s;
    }
    PUT(fmt, s - fmt);
    if (c == 0) {
      return x.nchar;
    }
    fmt = ++s;

    // Parse the conversion's flag characters into x.flags.
    for (x.flags = 0; (t = strchr(fchar, *s)) != NULL; s++) {
      x.flags |= fbit[t - fchar];
    }

    // Field width: '*' takes it from an int argument (a negative one means
    // left-justify), otherwise read it inline. ATOI clamps the inline form.
    if (*s == '*') {
      x.width = va_arg(ap, int);
      if (x.width < 0) {
        x.width = -x.width;
        x.flags |= FLAGS_MINUS;
      }
      s++;
    } else
      ATOI(x.width, s);

    // Precision: absent leaves -1, '.*' takes it from an argument, else parse
    // the digits inline (clamped at 999, same guard as ATOI).
    if (*s != '.') {
      x.prec = -1;
    } else if (*++s == '*') {
      x.prec = va_arg(ap, int);
      ++s;
    } else {
      for (x.prec = 0; isdigit(*s); s++) {
        if (x.prec < 999) {
          x.prec = (x.prec * 10) + *s - '0';
        }
      }
    }

    // Length modifier: collapse "ll" to the single 'L' qual the converters use.
    x.qual = strchr("hlL", *s) ? *s++ : '\0';
    if (x.qual == 'l' && *s == 'l') {
      x.qual = 'L';
      ++s;
    }

    // *s is now the conversion letter. Fetch the argument and convert it; the
    // converter fills the six run lengths describing the formatted field.
    _Putfld(&x, &ap, *s, ac);

    // Whatever field width is left after the field's own characters becomes
    // padding on one side or the other.
    x.width -= x.n0 + x.nz0 + x.n1 + x.nz1 + x.n2 + x.nz2;
    {
      // Right-justify (the default): pad with leading spaces before the field.
      if (!(x.flags & FLAGS_MINUS)) {
        int i;
        int j;
        if (0 < (x.width)) {
          i, j = x.width;
          for (; 0 < j; j -= i) {
            i = MAX_PAD < (unsigned int)j ? (int)MAX_PAD : j;
            PUT(spaces, i);
          }
        }
      }

      // Emit the field's runs in order: sign/prefix, the two digit groups, and
      // the zero padding interleaved between them.
      PUT(ac, x.n0);
      PAD(zeroes, x.nz0)
      PUT(x.s, x.n1);
      PAD(zeroes, x.nz1);
      PUT(x.s + x.n1, x.n2);
      PAD(zeroes, x.nz2);

      // Left-justify: any leftover width becomes trailing spaces.
      if (x.flags & FLAGS_MINUS) {
        PAD(spaces, x.width);
      }
    }
    fmt = s + 1;
  }
  return 0;
}

/*
 * Fetch the argument for conversion letter `code` and turn it into a formatted
 * field. The result is described in px's run lengths (and, for numbers, the
 * digits land in the caller's work buffer ac); the actual emission and padding
 * happen back in _Printf.
 */
static void _Putfld(_Pft* px, va_list* pap, char code, char* ac) {
  // Start every field empty; each case fills in only the runs it needs.
  px->n0 = px->nz0 = px->n1 = px->nz1 = px->n2 = px->nz2 = 0;
  switch (code) {
    case 'c':
      // Single character, promoted through int by the call.
      ac[px->n0++] = va_arg(*pap, int);
      break;

    case 'd':
    case 'i':
      // Signed decimal. Read at the argument's width, then sign-narrow an
      // 'h'-qualified value so a short prints with the right sign.
      if (px->qual == 'l') {
        px->v.ll = va_arg(*pap, long);
      } else if (px->qual == 'L') {
        px->v.ll = va_arg(*pap, long long);
      } else {
        px->v.ll = va_arg(*pap, int);
      }
      if (px->qual == 'h') {
        px->v.ll = (short)px->v.ll;
      }

      // Place the sign character (or the space/plus the flags request) ahead
      // of the digits _Litob will produce.
      if (px->v.ll < 0) {
        ac[px->n0++] = '-';
      } else if (px->flags & FLAGS_PLUS) {
        ac[px->n0++] = '+';
      } else if (px->flags & FLAGS_SPACE) {
        ac[px->n0++] = ' ';
      }
      px->s = (&ac[px->n0]);
      _Litob(px, code);
      break;

    case 'x':
    case 'X':
    case 'u':
    case 'o':
      // Unsigned conversions. Mask the value down to the unsigned width the
      // qualifier names so the high bits of a widened argument don't print.
      if (px->qual == 'l') {
        px->v.ll = va_arg(*pap, long);
      } else if (px->qual == 'L') {
        px->v.ll = va_arg(*pap, long long);
      } else {
        px->v.ll = va_arg(*pap, int);
      }
      if (px->qual == 'h') {
        px->v.ll = (unsigned short)px->v.ll;
      } else if (px->qual == 0) {
        px->v.ll = (unsigned int)px->v.ll;
      }

      // '#' adds the alternate-form prefix: a leading 0 for octal, 0x/0X for
      // hex.
      if (px->flags & FLAGS_HASH) {
        ac[px->n0++] = '0';
        if (code == 'x' || code == 'X') {
          ac[px->n0++] = code;
        }
      }
      px->s = (&ac[px->n0]);
      _Litob(px, code);
      break;

    case 'e':
    case 'f':
    case 'g':
    case 'E':
    case 'G':
      // Floating point. A bare double argument is read as double; only 'L'
      // pulls a long double. LDSIGN catches negative zero that `< 0` misses.
      px->v.ld = px->qual == 'L' ? va_arg(*pap, ldouble) : va_arg(*pap, double);
      if (LDSIGN(px->v.ld)) {
        ac[px->n0++] = '-';
      } else if (px->flags & FLAGS_PLUS) {
        ac[px->n0++] = '+';
      } else if (px->flags & FLAGS_SPACE) {
        ac[px->n0++] = ' ';
      }
      px->s = (&ac[px->n0]);
      _Ldtob(px, code);
      break;

    case 'n':
      // Store the running output count into the pointer argument, at the
      // width the qualifier names. Produces no output of its own.
      if (px->qual == 'h') {
        *va_arg(*pap, unsigned short*) = px->nchar;
      } else if (px->qual == 'l') {
        *va_arg(*pap, unsigned long*) = px->nchar;
      } else if (px->qual == 'L') {
        *va_arg(*pap, unsigned long long*) = px->nchar;
      } else {
        *va_arg(*pap, unsigned int*) = px->nchar;
      }
      break;

    case 'p':
      // Pointer: print its bits as hex, reusing the integer converter.
      px->v.ll = (long)va_arg(*pap, void*);
      px->s = (&ac[px->n0]);
      _Litob(px, 'x');
      break;

    case 's':
      // String: emit it directly, capped at the precision if one was given.
      px->s = va_arg(*pap, char*);
      px->n1 = strlen(px->s);
      if (px->prec >= 0 && px->prec < px->n1) {
        px->n1 = px->prec;
      }
      break;

    case '%':
      // "%%" is a literal percent sign.
      ac[px->n0++] = '%';
      break;

    default:
      // Unknown conversion: print the letter verbatim.
      ac[px->n0++] = code;
      break;
  }
}
