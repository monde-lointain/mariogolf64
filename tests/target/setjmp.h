/*
 * The setjmp that Unity uses to end a test at its first failed assertion,
 * even one in a helper. The game has no C library to provide one, so GCC's
 * builtins stand in: Unity always passes longjmp 1, the only value that
 * __builtin_longjmp takes, and never calls it from the function that
 * called setjmp.
 */

#ifndef TARGET_SETJMP_H
#define TARGET_SETJMP_H

// __builtin_setjmp's buffer: five words
typedef void *jmp_buf[5];

#define setjmp(buffer) __builtin_setjmp(buffer)
#define longjmp(buffer, value) __builtin_longjmp(buffer, value)

#endif
