OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)
SECTIONS {
	/* Start address of etherboot in the virtual address space */
	_virt_start = .;
	.text : {
		_text = . ;
		*(.text)
		*(.text.*)
	} = 0x9090
	.rodata : {
		. = ALIGN(4);
		*(.rodata)
		*(.rodata.*)
	}
	. = ALIGN(4);
	.drivers.pci : {
		pci_drivers = . ;
		*(.drivers.pci);
		pci_drivers_end = . ;
	}
	. = ALIGN(4);
	.drivers.isa : {
		isa_drivers = . ;
		*(.drivers.isa);
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
	_stack = . ;
	.stack : {
		*(.stack)
		*(.stack.*)
	}
	. = ALIGN(16);
	_end = .;
	/DISCARD/ : {
		*(.comment)
		*(.note)
	}
}
