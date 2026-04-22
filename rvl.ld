/*--------------------------------------------------------------------------------
        ** Linker script for Wii homebrew **
--------------------------------------------------------------------------------*/
PHDRS {
        stub PT_LOAD FLAGS(5);
}
SECTIONS {
        . = 0x00003400;
        .stub : {
                KEEP(*(.stub))
        } :stub = 0
        . = 0x80003f00;
}
PROVIDE_HIDDEN( __Arena2Lo    = 0x90002000 );
PROVIDE_HIDDEN( __Arena2Hi    = 0x933e0000 );
PROVIDE_HIDDEN( __ipcbufferLo = 0x933e0000 );
PROVIDE_HIDDEN( __ipcbufferHi = 0x93400000 );
INCLUDE libogc_common.ld
