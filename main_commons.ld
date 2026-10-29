/* Allocates the commons of main's combined relocatable into one section, which
 * the main segment's linker script places at the end of its bss */
SECTIONS
{
  .main_commons : { *(COMMON) }
  /DISCARD/ : { *(*) }
}
