OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)
SECTIONS {
	.text : {
		_text = . ;
		*(.text)
		*(.text.*)
	} = 0x9090
	.rodata : {
		. = ALIGN(4);
		*(.rodata)
	}
	.rodata.pci_drivers : {
		. = ALIGN(4);
		pci_drivers = . ;
		*(.rodata.pci_drivers);
		pci_drivers_end = . ;
	}
	.rodata.isa_drivers : {
		. = ALIGN(4);
		isa_drivers = . ;
		*(.rodata.isa_drivers);
		isa_drivers_end = .;
	}
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