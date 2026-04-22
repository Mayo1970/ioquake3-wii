/*--------------------------------------------------------------------------------
	** Common linker script logic for GameCube/Wii homebrew **
--------------------------------------------------------------------------------*/

OUTPUT_ARCH(powerpc:common)
ENTRY(__app_start)

PROVIDE_HIDDEN( __isIPL = 0 );

PROVIDE_HIDDEN( __ppc_excpt_buf   = 0x800000d0 );
PROVIDE_HIDDEN( __ppc_excpt_table = 0x80003000 );

PHDRS {
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
	bss  PT_LOAD;
}

SECTIONS {

	.init : {
		*(.crt0 .crt0.*)
	} :text = 0

	/* Code */
	.text : ALIGN_WITH_INPUT {
		KEEP( *(SORT_NONE(.init)) )
		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.exit .text.exit.*)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(SORT(.text.sorted.*))
		*(.text .iplt .stub .text.* .gnu.linkonce.t.*)
		KEEP( *(SORT_NONE(.fini)) )
	} :text = 0

	. = ALIGN(32);

	/* Read-only data */
	.rodata : {
		KEEP( *(.rodata.__sys_versioninfo) )

		*(.rodata .rodata.* .gnu.linkonce.r.*)
		*(.rodata1)

		*(.fixup)
		*(.got1)
		*(.got2)
		*(.branch_lt)
		*(.got)
		*(.plt)
	} :data = 0

	/* Small read-only data */
	.sdata2 : ALIGN(16) {
		*(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
		*(.sbss2 .sbss2.* .gnu.linkonce.sb2.*)
	} :data = 0

	/* Exception frame header */
	.eh_frame_hdr : ALIGN_WITH_INPUT {
		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
	} :data = 0

	/* Exception frame information */
	.eh_frame : ALIGN_WITH_INPUT {
		KEEP( *(.eh_frame) )
		*(.eh_frame.*)
	} :data = 0

	/* GCC language agnostic exception table */
	.gcc_except_table : ALIGN_WITH_INPUT {
		*(.gcc_except_table .gcc_except_table.*)
	} :data = 0

	/* GNU exception table */
	.gnu_extab : ALIGN_WITH_INPUT {
		*(.gnu_extab*)
	} :data = 0

	/* Table of global constructors */
	.ctors : ALIGN_WITH_INPUT {
		KEEP( *ecrti.o(.ctors) )
		KEEP( *crtbegin.o(.ctors) )
		KEEP( *crtbegin?.o(.ctors) )
		KEEP( *(EXCLUDE_FILE(*ecrtn.o *crtend.o *crtend?.o) .ctors) )
		KEEP( *(SORT_BY_NAME(.ctors.*)) )
		KEEP( *(SORT_NONE(.ctors)) )
	} :data = 0

	/* Table of global destructors */
	.dtors : ALIGN_WITH_INPUT {
		KEEP( *ecrti.o(.dtors) )
		KEEP( *crtbegin.o(.dtors) )
		KEEP( *crtbegin?.o(.dtors) )
		KEEP( *(EXCLUDE_FILE(*ecrtn.o *crtend.o *crtend?.o) .dtors) )
		KEEP( *(SORT_BY_NAME(.dtors.*)) )
		KEEP( *(SORT_NONE(.dtors)) )
	} :data = 0

	.data : ALIGN_WITH_INPUT {
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
		SORT(CONSTRUCTORS)
	} :data = 0

	.sdata : ALIGN_WITH_INPUT {
		*(.sdata .sdata.* .gnu.linkonce.s.*)
		*(.lit8)
		*(.lit4)
	} :data = 0

	.sbss . (NOLOAD) : {
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
	} :bss

	PROVIDE_HIDDEN( __sbss_start = ADDR(.sbss)                 );
	PROVIDE_HIDDEN( __sbss_end   = ADDR(.sbss) + SIZEOF(.sbss) );

	.bss . (NOLOAD) : {
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
	} :bss

	PROVIDE_HIDDEN( __bss_start = ADDR(.bss)                );
	PROVIDE_HIDDEN( __bss_end   = ADDR(.bss) + SIZEOF(.bss) );

	. = .;

	PROVIDE_HIDDEN( __Arena1Lo = (. + 0x1f) &~ 0x1f );
	PROVIDE_HIDDEN( __Arena1Hi = 0x81800000         );

	/* ====================== */
	/*   Debugging metadata   */
	/* ====================== */

	/* Stabs debugging sections.  */
	.stab           0 : { *(.stab) }
	.stabstr        0 : { *(.stabstr) }
	.stab.excl      0 : { *(.stab.excl) }
	.stab.exclstr   0 : { *(.stab.exclstr) }
	.stab.index     0 : { *(.stab.index) }
	.stab.indexstr  0 : { *(.stab.indexstr) }
	.comment 0 (INFO) : { *(.comment); LINKER_VERSION; }
	.gnu.build.attributes : { *(.gnu.build.attributes .gnu.build.attributes.*) }

	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0. */
	/* DWARF 1 */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }
	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }
	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
	/* SGI/MIPS DWARF 2 extensions */
	.debug_weaknames 0 : { *(.debug_weaknames) }
	.debug_funcnames 0 : { *(.debug_funcnames) }
	.debug_typenames 0 : { *(.debug_typenames) }
	.debug_varnames  0 : { *(.debug_varnames) }
	/* DWARF 3 */
	.debug_pubtypes 0 : { *(.debug_pubtypes) }
	.debug_ranges   0 : { *(.debug_ranges) }
	/* DWARF Extension.  */
	.debug_macro    0 : { *(.debug_macro) }
	.debug_addr     0 : { *(.debug_addr) }
	/* DWARF 5.  */
	.debug_addr     0 : { *(.debug_addr) }
	.debug_line_str 0 : { *(.debug_line_str) }
	.debug_loclists 0 : { *(.debug_loclists) }
	.debug_macro    0 : { *(.debug_macro) }
	.debug_names    0 : { *(.debug_names) }
	.debug_rnglists 0 : { *(.debug_rnglists) }
	.debug_str_offsets 0 : { *(.debug_str_offsets) }
	.debug_sup      0 : { *(.debug_sup) }

	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }

}
