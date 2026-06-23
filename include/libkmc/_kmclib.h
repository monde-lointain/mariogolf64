/*
 * _kmclib.h -- internal build configuration and portability layer for the KMC
 * C library.
 *
 * Sets the compile-time feature flags (target CPU, endianness, float support,
 * speed-vs-size tradeoff) that the library sources branch on, defines the
 * short integer-type and register-hint aliases the sources are written in for
 * the selected CPU, and declares the low-level console primitives. Included
 * first by every library source file.
 */

#define LIB_DBG 0     // 1 enables internal library debug paths
#define FAST_SPEED 1  // 1 favors speed (unrolled/word-wide), 0 favors size
#define ENDIAN_BIG 1  // target byte order: 1 big-endian, 0 little-endian
#define FLOAT_S 0     // 1 if float is the native size, 0 for double

// Target CPU select: exactly one is 1, the rest 0. MIPS is the N64 build.
#define CPU_MIPS 1
#define CPU_SH7 0
#define CPU_68K 0
#define CPU_V800 0
#define CPU_386 0

#define ET 1              // error-table support enabled
#define FLOAT_LLONG_ON 1  // use 64-bit long long for fixed-point math

// Per-CPU type and register-hint aliases the sources are written in. The MIPS
// block sizes BYTE/WORD/DWORD to this target's integer widths.
#if CPU_MIPS
// clang-format off: aligned type/register-hint table
#define BUS_ERR_ALLIGN 3      /* alignment mask that faults a misaligned access */
#define BYTE           char            /* 8-bit signed */
#define UBYTE          unsigned char   /* 8-bit unsigned */
#define WORD           short int       /* 16-bit signed */
#define UWORD          unsigned short  /* 16-bit unsigned */
#define DWORD          long int        /* 32-bit signed */
#define UDWORD         unsigned long   /* 32-bit unsigned */
#define REG1           register        /* register hints, ranked REG1..REG6 */
#define REG2           register
#define REG3           register
#define REG4           register
#define REG5           register
#define REG6           register
// clang-format on
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef EOF
#define EOF (-1)  // end-of-file / no-more-input sentinel
#endif

// Low-level console primitives supplied by the porting layer: read one key,
// display one character, and the one-byte key buffer they share.
int _pt_key(void);
void _pt_dsp(int c);
extern unsigned char _keybuf;
