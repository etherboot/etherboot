OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_in_call)
SECTIONS {
	/* Prefix */
	.prefix : {
		_binary_start = . ;
		_prefix_start = . ;
		*(.prefix)
		. = ALIGN(4);
		compressed = . ;
		*(.prefix.data)
		. = ALIGN(16);
		_prefix_end = . ;
		decompress = DEFINED(decompress) ? decompress : 0 ;
	} = 0x9090
	_prefix_size = _prefix_end - _prefix_start;

	/* Start address of etherboot in the virtual address space */
	_virt_start = 0;
	.text : {
		. = ALIGN(16);
		_text = . ;
		_runtime_start = . ;
	}	
	.text.nocompress : {
		*(.text*.nocompress)
		_compressed_binary_end = . ;
	} = 0x9090
	.text.compress : {
		decompress_to = . ;
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
		*(.text16.*)
		_etext16 = . ;
	}
	_etext = . ;
	_data = . ;
	.data : {
		*(.data)
		*(.data.*)
	}
	_edata = . ;
	_uncompressed_binary_end = . ;
	_binary_end = decompress ? _compressed_binary_end : _uncompressed_binary_end ;
	_binary_size = _binary_end - _binary_start ;
	. = ALIGN(16);
	.bss.preserve : {
		*(.bss.preserve)
		*(.bss.preserve.*)
	}
	_bss = . ;
	.bss : {
		*(.bss)
		*(.bss.*)
	}
	. = ALIGN(16);
	_ebss = .;
	_stack = . ;
	.stack : {
		_stack_start = . ;
		*(.stack)
		*(.stack.*)
		_stack_end = . ;
	}
	_stack_offset = _stack - _runtime_start + _prefix_size ;
	_stack_offset_pgh = _stack_offset / 16 ;
	_stack_size = _stack_end - _stack_start ;
	. = ALIGN(16);
	_end = . ;
	_runtime_end = . ;
	_runtime_size = _runtime_end - _runtime_start ;
	_image_size = _prefix_size + _runtime_size ;
	_image_size_pgh = _image_size / 16 ;
	/DISCARD/ : {
		*(.comment)
		*(.note)
	}

	/* PXE-specific symbol calculations.  The results of these are
	 * needed in romprefix.S, which is why they must be calculated
	 * here.
	 */
	_pxe_stack_size = _pxe_stack_t_size
		+ _pxe_callback_interface_size
		+ _rm_callback_interface_size;
}
