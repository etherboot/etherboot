OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)
SECTIONS {
	.text : {
		_text = . ;
		*(.text)
		*(.text.*)
		. = ALIGN(16);
		_text16 = .;
		*(.text16)
		_etext16 = . ;
		*(.rodata)
		*(.rodata.*)
	}
	_etext = . ;
	_data = . ;
	.data : {
		*(.data)
		*(.data.*)
	}
	_edata = . ;
	_bss = . ;
	.bss : {
		*(.bss)
		*(.bss.*)
	}
	. = ALIGN(16);
	_ebss = .;
	_end = .;
	/DISCARD/ : {
		*(.comment)
		*(.note)
	}
}