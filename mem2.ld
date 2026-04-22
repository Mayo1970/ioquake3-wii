/*
 * mem2.ld — place oversized BSS globals in Wii MEM2 (0x90000000)
 *
 * backEnd (~900KB) and tess (~600KB) are too large for MEM1.
 * This fragment appends a new output section after the normal BSS
 * that lives at the start of MEM2.
 */
SECTIONS
{
    .bss.mem2 0x90002000 (NOLOAD) :
    {
        *(.bss.mem2)
    }
}
INSERT AFTER .bss;
