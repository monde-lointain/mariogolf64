/*
 * _Ldtob: floating-point-to-text conversion for the %e/%f/%g family. Splits a
 * value into sign, decimal digits, and a decimal exponent, then hands those to
 * _Genld to lay out the final text. The digit generation works the magnitude
 * down into the [1,10) range with a table of powers of ten, then peels off
 * decimal digits eight at a time. Bit-level IEEE-754 double decoding is in
 * _Ldunscale.
 */

#include "stdlib.h"
#include "string.h"
#include "xstdio.h"
#ident "$Revision: 1.23 $"
#ident "$Revision: 1.34 $"
#ident "$Revision: 1.5 $"

// Work-buffer size: room for the most digits any conversion can generate.
#define BUFF_LEN 0x20

static short _Ldunscale(short* pex, ldouble* px);
static void _Genld(_Pft* px, char code, unsigned char* p, short nsig,
                   short xexp);

// Powers of ten at binary-exponent strides (10^1, 10^2, 10^4, 10^8, ...). A
// binary exponent is decomposed bit by bit and the matching entries are
// multiplied in, scaling any magnitude into range in a handful of steps.
static const ldouble pows[] = {10e0L,  10e1L,  10e3L,   10e7L,  10e15L,
                               10e31L, 10e63L, 10e127L, 10e255L};

// IEEE-754 double field layout, as seen through the 16-bit-word view used
// below. _D0 is the most significant word; the exponent occupies _DOFF..14.
// These are upstream multi-format (float/long-double) layout constants; only
// the double path is exercised here, so most of the _F*/_L* entries are unused.
// clang-format off: keep the field-layout constants aligned for scanning
#define _D0     0       // index of the most significant 16-bit word
#define _DBIAS  0x3ff   // exponent bias for double
#define _DLONG  1
#define _DOFF   4       // bit offset of the exponent within word _D0
#define _FBIAS  0x7e
#define _FOFF   7
#define _FRND   1
#define _LBIAS  0x3ffe
#define _LOFF   15
#define _C2     1
#define _CSIGN  1
#define _ILONG  0
#define _MBMAX  8
// clang-format on

// Classification results returned by _Ldunscale.
#define NAN 2
#define INF 1
#define FINITE (-1)

// Derived exponent-field masks for the double format.
#define _DFRAC ((1 << _DOFF) - 1)        // fraction bits within word _D0
#define _DMASK (0x7fff & ~_DFRAC)        // exponent bits within word _D0
#define _DMAX ((1 << (15 - _DOFF)) - 1)  // all-ones exponent (NaN/Inf)
#define _DNAN (0x8000 | _DMAX << _DOFF | 1 << (_DOFF - 1))
#define _DSIGN 0x8000  // sign bit within word _D0
#define _D1 1          // word indices for the
#define _D2 2          // lower-order mantissa
#define _D3 3          // halfwords
#define ALIGN(s, align) (((unsigned int)(s) + ((align) - 1)) & ~((align) - 1))

/*
 * Convert the long double in px->v.ld to text under conversion letter `code`.
 * Produces, via _Genld, the digit string plus a decimal exponent. nsig is the
 * significant-digit count and xexp is the power of ten of the leading digit.
 */
void _Ldtob(_Pft* px, char code) {
  char buff[BUFF_LEN];
  char* p;
  ldouble ldval;
  short err;
  short nsig;
  short xexp;

  p = buff;
  ldval = px->v.ld;

  // Default precision is 6; %g treats a requested precision of 0 as 1.
  if (px->prec < 0) {
    px->prec = 6;
  } else if (px->prec == 0 && (code == 'g' || code == 'G')) {
    px->prec = 1;
  }

  // Decode the value. Returns the binary exponent in xexp and a class code:
  // >0 is NaN/Inf, 0 is zero, <0 is an ordinary finite number to format.
  err = _Ldunscale(&xexp, &px->v.ld);
  if (err > 0) {
    memcpy(px->s, err == 2 ? "NaN" : "Inf", px->n1 = 3);
    return;
  }
  if (err == 0) {
    // Exact zero: no digits, exponent zero.
    nsig = 0;
    xexp = 0;
  } else {
    // Scale the magnitude into a workable decimal range, peel off digits, then
    // trim and round to the needed significance.
    {
      int i;
      int n;
      if (ldval < 0) {
        ldval = -ldval;
      }

      // Estimate the decimal exponent from the binary one (log10(2) ~
      // 30103/100000), biased down by 4 so the digit loop has headroom. Then
      // multiply or divide by the matching powers of ten, decomposing the
      // (4-aligned) exponent one bit at a time against the pows[] table.
      if ((xexp = (xexp * 30103 / 100000) - 4) < 0) {
        n = ALIGN(-xexp, 4), xexp = -n;
        for (i = 0; n > 0; n >>= 1, i++) {
          if (n & 1) {
            ldval *= pows[i];
          }
        }
      } else if (xexp > 0) {
        ldouble factor = 1;
        xexp &= ~3;
        for (n = xexp, i = 0; n > 0; n >>= 1, i++) {
          if (n & 1) {
            factor *= pows[i];
          }
        }
        ldval /= factor;
      }
    }

    {
      // Number of digits to generate, bounded so we never overrun the float's
      // real precision (0x13 ~ 19 significant decimal digits).
      int gen = px->prec + ((code == 'f') ? 10 + xexp : 6);
      if (gen > 0x13) {
        gen = 0x13;
      }

      // Peel digits eight at a time: take the integer part as a digit octet,
      // promote the fraction by 1e8, and repeat. Each octet is written low
      // digit last by stepping the pointer forward then filling backward.
      for (*p++ = '0'; gen > 0 && 0 < ldval; p += 8) {
        int j;
        long lo = ldval;
        if ((gen -= 8) > 0) {
          ldval = (ldval - lo) * 1e8;
        }
        for (p += 8, j = 8; lo > 0 && --j >= 0;) {
          ldiv_t qr;
          qr = ldiv(lo, 10);
          *--p = qr.rem + '0', lo = qr.quot;
        }
        // Left-pad a short octet with zeros so each octet stays 8 wide.
        while (--j >= 0) {
          *--p = '0';
        }
      }

      // Drop leading zeros, adjusting the digit count and decimal exponent as
      // each one is skipped, to land p on the first significant digit.
      gen = p - &buff[1];
      for (p = &buff[1], xexp += 7; *p == '0'; p++) {
        --gen, --xexp;
      }

      // How many significant digits the conversion actually wants.
      nsig =
          px->prec +
          ((code == 'f') ? xexp + 1 : ((code == 'e' || code == 'E') ? 1 : 0));
      if (gen < nsig) {
        nsig = gen;
      }

      // Round to nsig digits. If the first dropped digit is >= 5 we round up,
      // realized here by trimming trailing '9's (carry) versus trailing '0's,
      // then bumping the last kept digit. A carry off the front shifts the
      // whole number up one decimal place.
      if (nsig > 0) {
        const char drop = nsig < gen && '5' <= p[nsig] ? '9' : '0';
        int n;
        for (n = nsig; p[--n] == drop;) {
          --nsig;
        }
        if (drop == '9') {
          ++p[n];
        }
        if (n < 0) {
          --p, ++nsig, ++xexp;
        }
      }
    }
  }
  _Genld(px, code, p, nsig, xexp);
}

/*
 * Decompose the double at *px into a normalized fraction in [1,2) (written
 * back through *px) and its binary exponent (returned in *pex). The return
 * value classifies the input: 2 = NaN, 1 = Inf, 0 = zero, -1 = finite nonzero.
 */
short _Ldunscale(short* pex, ldouble* px) {
  unsigned short* ps = (unsigned short*)px;

  // Extract the biased exponent field from the high word.
  short xchar = (ps[_D0] & _DMASK) >> _DOFF;

  if (xchar == _DMAX) {
    // All-ones exponent: Inf if the mantissa is zero, otherwise NaN.
    *pex = 0;
    return (ps[_D0] & _DFRAC) || ps[_D1] || ps[_D2] || ps[_D3] ? 2 : 1;
  }
  if (xchar > 0) {
    // Normal number: force the exponent field to bias-0 so the value becomes a
    // fraction in [1,2), and hand back the unbiased binary exponent.
    ps[_D0] = (ps[_D0] & ~_DMASK) | 0x3FF0;
    *pex = xchar - 0x3FE;
    return -1;
  }
  if (xchar < 0) {
    // Cannot occur for a real field, but guards the signed shift result.
    return 2;
  }  // Zero (and subnormals, treated as zero here).
  *pex = 0;
  return 0;
}

/*
 * Lay out the final text for the digit string p (nsig significant digits, the
 * leading digit at decimal exponent xexp) under conversion letter `code`. Fills
 * px->s and the run-length fields with either fixed (%f), exponential (%e), or
 * shortest (%g) notation, plus the decimal point and field padding.
 */
void _Genld(_Pft* px, char code, unsigned char* p, short nsig, short xexp) {
  const unsigned char point = '.';

  // A value rounded away to nothing still prints a single "0" digit.
  if (nsig <= 0) {
    nsig = 1, p = "0";
  }

  // Fixed-point layout: chosen for %f, and for %g when the exponent is in the
  // range where fixed notation is the shorter form (-4 <= xexp < precision).
  if (code == 'f' ||
      (code == 'g' || code == 'G') && xexp >= -4 && xexp < px->prec) {
    xexp++;

    // For %g, precision counts significant digits, not fraction digits: clamp
    // it to the digits available (unless '#' forces trailing zeros) and rebase
    // it to a fraction-digit count for the layout below.
    if (code != 'f') {
      if (((px->flags & 8) == 0) && nsig < px->prec) {
        px->prec = nsig;
      }
      if ((px->prec -= xexp) < 0) {
        px->prec = 0;
      }
    }

    if (xexp <= 0) {
      // Magnitude below 1: emit "0", a point, then leading fraction zeros
      // before the significant digits.
      px->s[px->n1++] = '0';
      if (px->prec > 0 || (px->flags & 8)) {
        px->s[px->n1++] = point;
      }
      if (px->prec < -xexp) {
        xexp = -px->prec;
      }
      px->nz1 = -xexp;
      px->prec += xexp;
      if (px->prec < nsig) {
        nsig = px->prec;
      }
      memcpy(&px->s[px->n1], p, px->n2 = nsig);
      px->nz2 = px->prec - nsig;
    } else if (nsig < xexp) {
      // Integer part wider than the digits we have: emit the digits, then pad
      // out the integer part with zeros, then the (empty) fraction.
      memcpy(&px->s[px->n1], p, nsig);
      px->n1 += nsig;
      px->nz1 = xexp - nsig;
      if (px->prec > 0 || (px->flags & 8)) {
        px->s[px->n1] = point;
        px->n2++;
      }
      px->nz2 = px->prec;
    } else {
      // Point falls inside the digit string: integer digits, point, then the
      // remaining digits as the fraction (capped at precision).
      memcpy(&px->s[px->n1], p, xexp);
      px->n1 += xexp;
      nsig -= xexp;
      if (px->prec > 0 || (px->flags & 8)) {
        px->s[px->n1++] = point;
      }
      if (px->prec < nsig) {
        nsig = px->prec;
      }
      memcpy(&px->s[px->n1], &p[xexp], nsig);
      px->n1 += nsig;
      px->nz1 = px->prec - nsig;
    }
  } else {
    // Exponential layout (%e, or %g that fell outside the fixed-point range).
    if (code == 'g' || code == 'G') {
      // Convert %g precision (significant digits) to %e precision (fraction
      // digits) and switch to the matching exponential letter.
      if (nsig < px->prec) {
        px->prec = nsig;
      }
      if (--px->prec < 0) {
        px->prec = 0;
      }
      code = (code == 'g') ? 'e' : 'E';
    }

    // Leading digit, point, then up to precision fraction digits.
    px->s[px->n1++] = *p++;
    if (px->prec > 0 || (px->flags & 8)) {
      px->s[px->n1++] = point;
    }
    if (px->prec > 0) {
      if (px->prec < --nsig) {
        nsig = px->prec;
      }
      memcpy(&px->s[px->n1], p, nsig);
      px->n1 += nsig;
      px->nz1 = px->prec - nsig;
    }

    // Exponent: the 'e'/'E' marker, a forced sign, then the exponent digits.
    // At least two digits are always written.
    p = &px->s[px->n1];
    *p++ = code;
    if (xexp >= 0) {
      *p++ = '+';
    } else {
      *p++ = '-';
      xexp = -xexp;
    }
    if (xexp >= 100) {
      if (xexp >= 1000) {
        *p++ = (xexp / 1000) + '0', xexp %= 1000;
      }
      *p++ = (xexp / 100) + '0', xexp %= 100;
    }
    *p++ = (xexp / 10) + '0', xexp %= 10;
    *p++ = xexp + '0';
    px->n2 = (size_t)p - ((size_t)px->s + px->n1);
  }

  // With the '0' flag and no left-justify, fill the rest of the field width
  // with leading zeros (placed after the sign, in the nz0 run).
  if ((px->flags & 0x14) == 0x10) {
    int n = px->n0 + px->n1 + px->nz1 + px->n2 + px->nz2;
    if (n < px->width) {
      px->nz0 = px->width - n;
    }
  }
}
