#include "libm.h"

/* Programs may replace this to handle math errors; it handles none, so errno is
 * set instead */
int matherr(struct exception* exc __attribute__((unused))) {
  return 0;
}
