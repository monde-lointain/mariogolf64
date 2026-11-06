#ifndef COMPILER_H
#define COMPILER_H

/*
 * Macros that adapt the source to the compiler building it. The matching
 * build's KMC GCC 2.7.2 gets the forms that compile to the original's
 * code, and other builds get portable ones that the compiler can check.
 */

/*
 * Follows the declarator of a static that no code reads, which the original
 * still holds, so that -W does not warn of it
 */
#define UNUSED __attribute__((unused))

/*
 * Begins the definition of a variable that only its own file uses. The
 * original made such variables global, so the matching build keeps them
 * global, for main_bss.c or the overlay's common block to place where the
 * original's linker did; other builds make them static.
 */
#ifdef __IS_KMC__
#define FILE_STATIC
#else
#define FILE_STATIC static
#endif

/*
 * Follows the declarator of a zero-initialized variable that the original
 * places in .bss before the file's uninitialized statics. KMC GCC 2.7.2
 * emits those statics after everything else in .bss, but an initialized
 * variable where it is defined, and only honours a section attribute on an
 * initialized variable.
 */
#ifdef __IS_KMC__
#define ORDERED_BSS __attribute__((section(".bss")))
#else
#define ORDERED_BSS
#endif

/*
 * The return type of a function that returns no value but that the original
 * declared to return one. KMC GCC keeps v0 live to the end of such a function,
 * which changes its scheduling, so the matching build declares it s32; other
 * builds declare it void, so that nothing can use the value it never set.
 */
#ifdef __IS_KMC__
#define NO_RESULT s32
#else
#define NO_RESULT void
#endif

/*
 * What a function that returns a value on some paths returns on the others,
 * where the original returned whatever v0 held. Returning nothing keeps KMC
 * GCC's code; other builds return 0.
 */
#ifdef __IS_KMC__
#define UNSET_RESULT
#else
#define UNSET_RESULT 0
#endif

/*
 * Marks a char array that a string initializes but that needs no terminating
 * NUL, which KMC GCC has no attribute for
 */
#ifdef __IS_KMC__
#define NONSTRING
#else
#define NONSTRING __attribute__((nonstring))
#endif

/*
 * The length to declare a struct's last array with when it runs on past the
 * struct. The original declares one element and reads past it, which is
 * undefined; with AVOID_UB it is a flexible array, which KMC GCC writes as a
 * zero-length array.
 */
#ifndef AVOID_UB
#define TRAILING_ARRAY_LENGTH 1
#elif defined(__IS_KMC__)
#define TRAILING_ARRAY_LENGTH 0
#else
#define TRAILING_ARRAY_LENGTH
#endif

/*
 * Marks a function definition that nothing calls. KMC GCC takes no attribute
 * in front of a definition.
 */
#ifdef __IS_KMC__
#define UNUSED_FUNCTION
#else
#define UNUSED_FUNCTION UNUSED
#endif

/*
 * Marks what a pointer parameter points to as unchanged by the routine. KMC
 * GCC reads through a const pointer as unchanging memory, which compiles the
 * routine differently, so its pointer stays plain.
 */
#ifdef __IS_KMC__
#define READ_ONLY
#else
#define READ_ONLY const
#endif

/*
 * Compiles statements in a basic block of their own, to match. KMC GCC
 * schedules instructions and reuses values only within a block, and the
 * original's code shows block boundaries around some statements where its
 * source has none that survives; a loop that runs once gives them back.
 */
#define IN_OWN_BLOCK(statements) \
  do { \
    statements; \
  } while (0)

/*
 * Ends a basic block between two statements, to match. Each use's note gives
 * the words it saves. The original's osSyncPrintf isn't a do-while (0) macro
 * that would end blocks like this: wrapping every call in IN_OWN_BLOCK breaks
 * 37 matched functions.
 */
#define END_BASIC_BLOCK() \
  do { \
  } while (0)

/*
 * The stack pointer's current value. The asm is volatile, as a read of the
 * register has to be: GCC can't move memory accesses across it, and the
 * original's code shows none that moved.
 */
#define GetStackPointer() \
  ({ \
    void* stackPointer; \
    __asm__ volatile("move %0, $sp" : "=r"(stackPointer)); \
    stackPointer; \
  })

#endif
