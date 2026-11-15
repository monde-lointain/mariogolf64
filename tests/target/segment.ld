    /* The test ROM's segment of tests, which tests/target/boot.c loads
       into the Expansion Pak's memory. The game uses that memory for the
       hole editor, the expansion vertex buffers and, from 0x80600000, the
       heap, so the tests go between the buffers and the heap. Every object
       of the tests but boot.c's goes here. */
    target_tests_ROM_START = __romPos;
    target_tests_VRAM = ADDR(.target_tests);
    .target_tests 0x80500000 : AT(target_tests_ROM_START)
    {
        FILL(0x00000000);
        */tests/*.o(.text* .rodata* .data*);
        . = ALIGN(., 16);
        target_tests_TEXT_END = .;
    }
    .target_tests_bss (NOLOAD) :
    {
        target_tests_BSS_START = .;
        */tests/*.o(.bss* COMMON .scommon*);
        . = ALIGN(., 16);
        target_tests_BSS_END = .;
    }
    __romPos += SIZEOF(.target_tests);
    __romPos = ALIGN(__romPos, 16);
    target_tests_ROM_END = __romPos;
    /* Two buffers of 5000 vertices of 16 bytes (game.c) */
    ASSERT(g_ExpansionVertexBuffers + 2 * 5000 * 16 <= target_tests_VRAM, "the tests overlap the expansion vertex buffers")
    ASSERT(target_tests_BSS_END <= 0x80600000, "the tests overlap the heap's expansion region")

