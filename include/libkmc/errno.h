/*
 * errno.h -- error-status reporting for the KMC C library.
 *
 * Declares the shared `errno` cell that math and conversion routines set on
 * failure, plus the two error codes they can report.
 */
#ifndef _ERRNO_H
#define _ERRNO_H

// Last error reported by a library routine; routines set it but never clear it,
// so check it immediately after the call that may fail.
extern int errno;

#define EDOM 33    // argument outside a routine's mathematical domain
#define ERANGE 34  // result too large or too small to represent

#endif
