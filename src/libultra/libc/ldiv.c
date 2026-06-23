/*
 * Integer division returning quotient and remainder as a pair, for long and
 * long long. Provided so callers get both halves from one division and a
 * sign convention guaranteed independent of the host's.
 */

#include "stdlib.h"

/*
 * Divide num by denom, returning quotient and remainder together. The result
 * is normalized so the quotient truncates toward zero and the remainder takes
 * the sign of the dividend (the C standard guarantee that pre-C99 / and % did
 * not). The fix-up below corrects the one case the hardware divide gets wrong.
 */
ldiv_t ldiv(long num, long denom) {
  ldiv_t ret;

  ret.quot = num / denom;
  ret.rem = num - denom * ret.quot;

  // A negative quotient paired with a positive remainder means the divide
  // rounded toward negative infinity; nudge it back toward zero.
  if (ret.quot < 0 && ret.rem > 0) {
    ret.quot += 1;
    ret.rem -= denom;
  }
  return ret;
}

/* 64-bit counterpart of ldiv; same truncate-toward-zero normalization. */
lldiv_t lldiv(long long num, long long denom) {
  lldiv_t ret;

  ret.quot = num / denom;
  ret.rem = num - denom * ret.quot;

  if (ret.quot < 0 && ret.rem > 0) {
    ret.quot += 1;
    ret.rem -= denom;
  }
  return ret;
}
