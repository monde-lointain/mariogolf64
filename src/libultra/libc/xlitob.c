/*
 * _Litob: integer-to-text conversion for the %d/%i/%o/%u/%x/%X conversions.
 * Turns the integer staged in px->v.ll into its digit string and fills in the
 * field-length accounting (n1 / nz0) that _Printf later pads and emits.
 */

#include "stdlib.h"
#include "string.h"
#include "xstdio.h"
#ident "$Revision: 1.34 $"
#ident "$Revision: 1.5 $"
#ident "$Revision: 1.23 $"

// Scratch buffer size: wide enough for the longest 64-bit conversion (octal).
#define BUFF_LEN 0x18

static char ldigs[] = "0123456789abcdef";  // digits for lowercase %x
static char udigs[] = "0123456789ABCDEF";  // digits for uppercase %X

/*
 * Convert px->v.ll to text under conversion letter `code`, writing the digits
 * to px->s and recording the digit count in px->n1 (plus any zero padding the
 * precision or the '0' flag demands). The sign/prefix is already in place;
 * this routine produces only the magnitude digits.
 */
void _Litob(_Pft* px, char code) {
  char buff[BUFF_LEN];
  const char* digs;
  int base;
  int i;
  unsigned long long ullval;

  // Pick the digit alphabet and radix from the conversion letter.
  digs = (code == 'X') ? udigs : ldigs;
  base = (code == 'o') ? 8 : ((code != 'x' && code != 'X') ? 10 : 16);

  // Build digits from the low end of buff upward, so they come out in order.
  i = BUFF_LEN;
  ullval = px->v.ll;

  // Signed conversions arrive already flagged with a '-'; work on the
  // magnitude. The cast through unsigned makes negating LLONG_MIN well defined.
  if ((code == 'd' || code == 'i') && px->v.ll < 0) {
    ullval = -ullval;
  }

  // Emit the least significant digit. A zero value still produces one '0',
  // except when an explicit precision of 0 asks for no digits at all.
  if (ullval != 0 || px->prec != 0) {
    buff[--i] = digs[ullval % base];
  }

  // Emit the remaining digits. The first step keeps the plain /; the loop then
  // uses lldiv so the quotient and remainder come from a single 64-bit divide.
  px->v.ll = ullval / base;
  while (px->v.ll > 0 && i > 0) {
    lldiv_t qr = lldiv(px->v.ll, base);
    px->v.ll = qr.quot;
    buff[--i] = digs[qr.rem];
  }

  // Hand the produced digits to the output staging area.
  px->n1 = BUFF_LEN - i;
  memcpy(px->s, buff + i, px->n1);

  // Precision wider than the digit count is satisfied by leading zeros.
  if (px->n1 < px->prec) {
    px->nz0 = px->prec - px->n1;
  }

  // With no precision, the '0' flag (unless overridden by left-justify) pads
  // the field to its full width with zeros rather than spaces.
  if (px->prec < 0 && (px->flags & (FLAGS_ZERO | FLAGS_MINUS)) == FLAGS_ZERO) {
    if ((i = px->width - px->n0 - px->nz0 - px->n1) > 0) {
      px->nz0 += i;
    }
  }
}
