OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)
SECTIONS {
	.text : {
		_text = . ;
		*(.text)
		*(.text.*)
		. = ALIGN(4);
		pci_drivers = .;
		*(.rodata.pci_drivers)
		epci_drivers = .;
		. = ALIGN(4);
		isa_drivers = .;
		*(.rodata.isa_drivers)
		eisa_drivers = .;
		. = ALIGN(16);
		*(.rodata)
		*(.rodata.*)
	} = 0x9090
	.text16 : {
		. = ALIGN(16);
		_text16 = .;
		*(.text16)
		_etext16 = . ;
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