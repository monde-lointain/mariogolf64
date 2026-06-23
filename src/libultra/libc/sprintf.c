/*
 * sprintf: format into a caller-supplied buffer. A thin front end over the
 * shared _Printf engine, paired with the sink callback that drops each
 * formatted chunk into the destination string.
 */

#include "xstdio.h"
#include "string.h"
#include "os.h"
#ident "$Revision: 1.23 $"

static void* proutSprintf(void* s, const char* buf, size_t n);

/*
 * Format fmt and the trailing arguments into s, NUL-terminate, and return the
 * character count (excluding the NUL). _Printf streams the output through
 * proutSprintf, which advances a write cursor through s.
 */
int sprintf(char* s, const char* fmt, ...) {
  int ans;
  va_list ap;

  va_start(ap, fmt);
  ans = _Printf(proutSprintf, s, fmt, ap);

  // _Printf does not terminate; add the NUL once the length is known.
  if (ans >= 0) {
    s[ans] = 0;
  }
  return ans;
}

/*
 * _Printf output sink: append n bytes to the buffer at s and return the new
 * write position, which _Printf threads back in as s on the next chunk.
 */
static void* proutSprintf(void* s, const char* buf, size_t n) {
  return (char*)memcpy(s, buf, n) + n;
}
