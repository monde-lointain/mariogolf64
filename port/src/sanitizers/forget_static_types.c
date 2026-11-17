/*
 * Built into each of the project's programs when it is built with
 * TypeSanitizer, which types each object with static storage as a whole by
 * its declared type before main. An element of an array in a struct that it
 * types so is then reported whenever it is stored or read, even through the
 * struct's own member access, as the type of the access names the element
 * and not the struct. The game keeps much of its state in such structs.
 *
 * Forgetting those types, once TypeSanitizer's constructors have set them,
 * types each object by how the code first stores into it, as allocated
 * memory is, and leaves the checks of what the code stores and reads, which
 * are the ones that strict aliasing depends on.
 */

#include <sanitizer/tysan_interface.h>
#include <stddef.h>

// The ends of the program's data and bss, which the C library and the
// linker define on Linux
extern char __data_start[];
extern char _end[];

// TypeSanitizer's constructors have the highest priority, so this, with
// none, runs after them
__attribute__((constructor)) static void ForgetStaticTypes(void) {
    __tysan_set_type_unknown(__data_start, (size_t)(_end - __data_start));
}
