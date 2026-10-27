#include "libm.h"

/* Programs may replace this to handle math errors; it handles none, so errno is
 * set instead */
int matherr(struct exception* exc) {
  return 0;
}
