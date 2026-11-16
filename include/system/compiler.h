/*
 * Macros that adapt the source to the compiler building it. The matching
 * build's KMC GCC 2.7.2 gets the forms that compile to the original's code,
 * and other builds get portable ones that the compiler can check.
 */

#ifndef COMPILER_H
#define COMPILER_H

// Follows the declarator of a variable or parameter that nothing reads but
// that the original keeps, often for the size of its stack frame
#define UNUSED __attribute__((unused))

// Begins the definition of a variable that only its own file uses. The
// original made such variables global, so the matching build keeps them
// global for main_bss.c or the overlay's common block to place where the
// original's linker did. Other builds make them static.
#ifdef __IS_KMC__
#    define FILE_STATIC
#else
#    define FILE_STATIC static
#endif

// Follows the declarator of a zero-initialized variable that the original
// places in .bss ahead of the file's uninitialized statics. KMC GCC emits
// those statics after everything else in .bss, but an initialized variable
// where it is defined, and it honors a section attribute only on an
// initialized variable.
#ifdef __IS_KMC__
#    define ORDERED_BSS __attribute__((section(".bss")))
#else
#    define ORDERED_BSS
#endif

// The return type of a routine that returns no value but that the original
// declared to return one. KMC GCC keeps v0 live to the end of such a
// routine, which its scheduling shows, so the matching build declares it
// s32. Other builds declare it void, so nothing can read the value it never
// sets.
#ifdef __IS_KMC__
#    define NO_RESULT s32
#else
#    define NO_RESULT void
#endif

// What a routine that returns a value on some paths returns on the others,
// where the original left whatever v0 held. Returning nothing keeps KMC
// GCC's code; other builds return 0.
#ifdef __IS_KMC__
#    define UNSET_RESULT
#else
#    define UNSET_RESULT 0
#endif

// Marks a char array that a string initializes but that needs no
// terminating NUL. KMC GCC has no attribute for it.
#ifdef __IS_KMC__
#    define NONSTRING
#else
#    define NONSTRING __attribute__((nonstring))
#endif

// The length to declare a struct's last array with when the game reads past
// it. The original declares one element and reads on, which is undefined;
// with AVOID_UB it is a flexible array, which KMC GCC writes as a
// zero-length one.
#ifndef AVOID_UB
#    define TRAILING_ARRAY_LENGTH 1
#elif defined(__IS_KMC__)
#    define TRAILING_ARRAY_LENGTH 0
#else
#    define TRAILING_ARRAY_LENGTH
#endif

// Marks a routine definition that nothing calls. KMC GCC takes no attribute
// in front of a definition.
#ifdef __IS_KMC__
#    define UNUSED_FUNCTION
#else
#    define UNUSED_FUNCTION UNUSED
#endif

// Marks what a pointer parameter points to as unchanged by the routine. KMC
// GCC treats memory read through a const pointer as unchanging, which
// compiles the routine differently, so the matching build's pointer stays
// plain.
#ifdef __IS_KMC__
#    define READ_ONLY
#else
#    define READ_ONLY const
#endif

// Follows the declarator of a buffer of bytes or halfwords that the RCP or a
// DMA reads or writes: a texture, palette or image, or a DMA's target, or of
// a structure that holds one at an 8-byte offset, such as the save data that
// SRAM's DMA reads and writes whole. Their address registers ignore the low
// three bits, as does TextureAddress, so the buffer must start on an 8-byte
// boundary. The original's linker happened to place each one so; other
// layouts have to ask for it. A type that always holds such a buffer, such as
// ImageHeader, asks for it once, after its closing brace.
#define RCP_ALIGNED __attribute__((aligned(8)))

// Follows the declarator of a byte array that a thread uses as its stack. The
// O32 ABI keeps the stack pointer a multiple of 8, so that doubles can be
// stored with sdc1, and the stack's top is the end of the array.
#define STACK_ALIGNED __attribute__((aligned(8)))

// Compiles statements in a block of their own, to match. KMC GCC schedules
// instructions and reuses values only within a basic block, and the
// original's code shows block boundaries around some statements where its
// source has none that survives. A loop that runs once gives them back.
#define IN_OWN_BLOCK(statements) \
    do { \
        statements; \
    } \
    while (FALSE)

// Makes an assignment the way GCC makes one whose value is used, to match:
// it copies the value to a register and stores the register, where a
// statement's constant would be stored directly.
#define ASSIGN_FOR_VALUE(assignment) ({ assignment; })

// The stack pointer's current value. The asm is volatile so that GCC can't
// move memory accesses across the read.
#ifdef __mips__
#    define GetStackPointer() \
        ({ \
            void *stackPointer; \
            __asm__ volatile("move %0, $sp" : "=r"(stackPointer)); \
            stackPointer; \
        })
#else
#    define GetStackPointer() __builtin_frame_address(0)
#endif

#endif
