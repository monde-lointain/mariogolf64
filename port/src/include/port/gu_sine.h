/*
 * The port's stand-ins for libultra's sinf and cosf, which the game and the
 * gu transforms call. The host's own differ from each other in their last
 * bit for about one angle in a thousand, so a shot could fly differently
 * from one host to the next. These give the same result on every host,
 * which is the nearest float to the true sine or cosine.
 */

#ifndef PORT_GU_SINE_H
#define PORT_GU_SINE_H

// The sine and cosine of an angle in radians. As libultra's do, they give
// 0 for an infinite angle or one of 2^28 or more, where the sum that turns
// the angle into a half turn's would be too far out, and NaN for NaN.
float GuSine(float angle);
float GuCosine(float angle);

#endif
