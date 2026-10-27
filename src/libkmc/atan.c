#include "libm.h"

/* atan(2^-i) in FIXED_POINT_ONE units, for the CORDIC iterations */
long long g_CordicAngles[64] = {
    0x0C90FDAA22168C00LL,
    0x076B19C1586ED3C0LL,
    0x03EB6EBF25901BA0LL,
    0x01FD5BA9AAC2F6E0LL,
    0x00FFAADDB967EF50LL,
    0x007FF556EEA5D894LL,
    0x003FFEAAB776E536LL,
    0x001FFFD555BBBA97LL,
    0x000FFFFAAAADDDDBLL,
    0x0007FFFF55556EEFLL,
    0x0003FFFFEAAAAB77LL,
    0x0001FFFFFD55555BLL,
    0x0000FFFFFFAAAAAALL,
    0x00007FFFFFF55555LL,
    0x00003FFFFFFEAAAALL,
    0x00001FFFFFFFD555LL,
    0x00000FFFFFFFFAAALL,
    0x000007FFFFFFFF55LL,
    0x000003FFFFFFFFEALL,
    0x000001FFFFFFFFFDLL,
    0x000000FFFFFFFFFFLL,
    0x0000007FFFFFFFFFLL,
    0x0000003FFFFFFFFFLL,
    0x0000001FFFFFFFFFLL,
    0x0000000FFFFFFFFFLL,
    0x00000007FFFFFFFFLL,
    0x00000003FFFFFFFFLL,
    0x0000000200000000LL,
    0x0000000100000000LL,
    0x0000000080000000LL,
    0x0000000040000000LL,
    0x0000000020000000LL,
    0x0000000010000000LL,
    0x0000000008000000LL,
    0x0000000004000000LL,
    0x0000000002000000LL,
    0x0000000001000000LL,
    0x0000000000800000LL,
    0x0000000000400000LL,
    0x0000000000200000LL,
    0x0000000000100000LL,
    0x0000000000080000LL,
    0x0000000000040000LL,
    0x0000000000020000LL,
    0x0000000000010000LL,
    0x0000000000008000LL,
    0x0000000000004000LL,
    0x0000000000002000LL,
    0x0000000000001000LL,
    0x0000000000000800LL,
    0x0000000000000400LL,
    0x0000000000000200LL,
    0x0000000000000100LL,
    0x0000000000000080LL,
    0x0000000000000040LL,
    0x0000000000000020LL,
    0x0000000000000010LL,
    0x0000000000000008LL,
    0x0000000000000004LL,
    0x0000000000000002LL,
    0x0000000000000001LL,
};

/*
 * Turns the vector (x, y), in fixed point with x > 0, onto the x axis
 * through ever smaller known angles, summing them into atan(y / x)
 */
void CordicAngle(long long x, long long y, double* angle) {
  long long sum;
  long long rotatedX;
  long long currentX;
  int i;

  currentX = x;
  sum = 0;
  for (i = 0; i < CORDIC_ITERATION_COUNT; i++) {
    if (y >= 0) {
      rotatedX = currentX + (y >> i);
      y -= currentX >> i;
      sum += g_CordicAngles[i];
    } else {
      rotatedX = currentX - (y >> i);
      y += currentX >> i;
      sum -= g_CordicAngles[i];
    }
    currentX = rotatedX;
  }
  *angle = sum / FIXED_POINT_ONE_F;
}

double atan(double x) {
  int sign;
  long long cordicX;
  long long cordicY;
  double angle;

  if (x < 0.0) {
    x = -x;
    sign = -1;
  } else {
    sign = 1;
  }
  /* atan(x) is x this close to 0 */
  if (x > 1e-7) {
    /* Keeps both coordinates within the fixed point's range */
    if (x >= 1.0) {
      cordicY = FIXED_POINT_ONE;
      cordicX = (1.0 / x) * FIXED_POINT_ONE_F;
    } else {
      cordicX = FIXED_POINT_ONE;
      cordicY = x * FIXED_POINT_ONE_F;
    }
    CordicAngle(cordicX, cordicY, &angle);
  } else {
    angle = x;
  }
  return (sign < 0) ? -angle : angle;
}

double atan2(double y, double x) {
  double angle;

  if (x == 0.0 && y == 0.0) {
    return ReportMathError(DOMAIN, "atan2", x, y, 0.0);
  }
  if (x == 0.0) {
    return (y > 0.0) ? M_PI / 2 : -M_PI / 2;
  }
  angle = atan(y / x);
  if (x > 0.0) {
    return angle;
  }
  if (y == 0.0) {
    return M_PI;
  }
  if (y > 0.0) {
    return angle + M_PI;
  }
  return angle - M_PI;
}
