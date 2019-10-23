#ifndef __BOOT_ASM_H__
#define __BOOT_ASM_H__

/* Assembler macros to create x86 segments */

/* Normal segment */
#define SEG_NULLASM                                             \
    .word 0, 0;                                                 \
    .byte 0, 0, 0, 0

/**
 * 0x90 -> 0b1001_0000
 * the rightmost 1 for `always`, should be 1 for everything but TSS and LDT
 * the leftmost 1 for `present`
 * the middle `00` for DPL
 * 
 * 0xC0 -> 0b1100_0000
 * the leftmost first 1 for gran=1, use 4k page addressing
 * the left second 1 for big=1, 32bit opcodes for code, uint32_t stack for data
 */
#define SEG_ASM(type,base,lim)                                  \
    .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);          \
    .byte (((base) >> 16) & 0xff), (0x90 | (type)),             \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

/**
 * NOTICE: no need for this macro, since GDT will be gracefully reloaded after init.
 * set descriptor for user mode
 * 0xf0 -> 0b1111_0000
 * the middle `11` for DPL=3
 */
#define SEG_UASM(type,base,lim)                                  \
    .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);          \
    .byte (((base) >> 16) & 0xff), (0xf0 | (type)),             \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

/* Application segment type bits */
#define STA_X       0x8     // Executable segment
#define STA_E       0x4     // Expand down (for non-executable segments)
#define STA_C       0x4     // Conforming code segment (for executable only)
#define STA_W       0x2     // Writeable (for non-executable segments)
#define STA_R       0x2     // Readable (for executable segments)
#define STA_A       0x1     // Accessed

#endif /* !__BOOT_ASM_H__ */

