/* plugin.ld - linker script for ocarina.3gx, a minimal Luma3DS 3GX plugin (no libc, no C++ runtime).
 *
 * Luma3DS's plugin loader maps the plugin image at 0x07000000 in the game process; the first 0x100 bytes of that
 * block are the loader's plugin header, so the plugin's own code starts at 0x07000100 and the entry point is the
 * first instruction there (src/start.s, section .crt0).
 *
 * 3gxtool turns the ELF into a .3gx and takes three loadable parts from it: code (R-X), read-only data (R--) and
 * read-write data (RW-). Each output section below goes into its own PT_LOAD segment in that order, and 3gxtool
 * refuses segments that are not back to back ("Non-contiguous segments!"), hence the padding and assertions.
 *
 * The plugin keeps all of its state in .data on purpose (it never relies on .bss being zeroed), so .bss must stay
 * empty; the assertions below catch a mistake at link time.
 *
 * SPDX-License-Identifier: MIT
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PHDRS
{
    code   PT_LOAD FLAGS(5);   /* R-X */
    rodata PT_LOAD FLAGS(4);   /* R-- */
    data   PT_LOAD FLAGS(6);   /* RW- */
}

SECTIONS
{
    . = 0x07000100;

    .text : ALIGN(4)
    {
        KEEP(*(.crt0))          /* _start must be the first word */
        *(.text)                /* all plain .text first, then the named sections (start.s trampolines) */
        *(.text.*)
        *(.glue_7 .glue_7t)
        . = ALIGN(4);
    } :code

    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(8);           /* .data is 8-aligned (u64 state); 3gxtool needs the segments back to back */
    } :rodata

    .data : ALIGN(8)
    {
        *(.data .data.*)
        . = ALIGN(4);
    } :data

    .bss (NOLOAD) : ALIGN(4)
    {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
    } :data

    __end__ = .;

    /DISCARD/ : { *(.comment) *(.note .note.*) *(.ARM.exidx*) *(.ARM.extab*) }
}

ASSERT(ADDR(.rodata) == ADDR(.text) + SIZEOF(.text), "3gxtool needs contiguous segments: gap between .text and .rodata")
ASSERT(ADDR(.data) == ADDR(.rodata) + SIZEOF(.rodata), "3gxtool needs contiguous segments: gap between .rodata and .data")
ASSERT(SIZEOF(.bss) == 0, "ocarina.3gx keeps all state in .data: a variable ended up in .bss (use the DATA attribute)")
ASSERT(__end__ < 0x07100000, "plugin image larger than 1 MiB")
