#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

#ifndef PERMUTER

/*
 * Modern GCC remembers which section it is emitting into, so included
 * assembly must give that section back. KMC's assembler has no section
 * stack, and its compiler emits only into .text around the asm.
 */
#    ifdef __IS_KMC__
#        define ASM_SECTION_ENTER(SECTION) ".section " SECTION "\n"
#        define ASM_SECTION_LEAVE ".section .text\n"
/*
 * After included assembly, KMC GCC still thinks the assembler is in the
 * section it last wrote. Where that was .rodata, this puts the assembler
 * back there before the data that follows.
 */
#        define RESUME_RODATA_SECTION() __asm__(".section .rodata")
#    else
#        define ASM_SECTION_ENTER(SECTION) ".pushsection " SECTION "\n"
#        define ASM_SECTION_LEAVE ".popsection\n"
#        define RESUME_RODATA_SECTION() __asm__("")
#    endif

#    ifndef INCLUDE_ASM
#        define INCLUDE_ASM(FOLDER, NAME) \
            __asm__(ASM_SECTION_ENTER(".text") "    .set noat\n" \
                                               "    .set noreorder\n" \
                                               "    .include \"" FOLDER "/" #NAME ".s\"\n" \
                                               "    .set reorder\n" \
                                               "    .set at\n" ASM_SECTION_LEAVE)
#    endif
#    ifndef INCLUDE_RODATA
#        define INCLUDE_RODATA(FOLDER, NAME) \
            __asm__(ASM_SECTION_ENTER(".rodata") "    .include \"" FOLDER "/" #NAME \
                                                 ".s\"\n" ASM_SECTION_LEAVE)
#    endif

/*
 * Modern assemblers need macro.inc's aliases for the o32 float register
 * names, such as $fv0, that the included assembly uses. KMC's assembler
 * needs labels.inc, which has no blank lines.
 */
#    ifdef __IS_KMC__
__asm__(".include \"include/labels.inc\"\n");
#    else
__asm__(".include \"include/macro.inc\"\n");
#    endif

#else

#    ifndef INCLUDE_ASM
#        define INCLUDE_ASM(FOLDER, NAME)
#    endif
#    ifndef INCLUDE_RODATA
#        define INCLUDE_RODATA(FOLDER, NAME)
#    endif
#    define RESUME_RODATA_SECTION() __asm__("")

#endif /* PERMUTER */

#endif /* INCLUDE_ASM_H */
