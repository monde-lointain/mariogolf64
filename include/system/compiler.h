#ifndef COMPILER_H
#define COMPILER_H

#define UNUSED __attribute__((unused))

#ifdef __IS_KMC__
#    define FILE_STATIC
#else
#    define FILE_STATIC static
#endif

#ifdef __IS_KMC__
#    define ORDERED_BSS __attribute__((section(".bss")))
#else
#    define ORDERED_BSS
#endif

#ifdef __IS_KMC__
#    define NO_RESULT s32
#else
#    define NO_RESULT void
#endif

#ifdef __IS_KMC__
#    define UNSET_RESULT
#else
#    define UNSET_RESULT 0
#endif

#ifdef __IS_KMC__
#    define NONSTRING
#else
#    define NONSTRING __attribute__((nonstring))
#endif

#ifndef AVOID_UB
#    define TRAILING_ARRAY_LENGTH 1
#elif defined(__IS_KMC__)
#    define TRAILING_ARRAY_LENGTH 0
#else
#    define TRAILING_ARRAY_LENGTH
#endif

#ifdef __IS_KMC__
#    define UNUSED_FUNCTION
#else
#    define UNUSED_FUNCTION UNUSED
#endif

#ifdef __IS_KMC__
#    define READ_ONLY
#else
#    define READ_ONLY const
#endif

#define IN_OWN_BLOCK(statements) \
    do { \
        statements; \
    } \
    while (FALSE)

#define ASSIGN_FOR_VALUE(assignment) ({ assignment; })

#define GetStackPointer() \
    ({ \
        void *stackPointer; \
        __asm__ volatile("move %0, $sp" : "=r"(stackPointer)); \
        stackPointer; \
    })

#endif
