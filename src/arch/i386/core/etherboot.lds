OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_in_call)
SECTIONS {
	/* Prefix */
	.prefix : {
		_verbatim_start = . ;
		_prefix_start = . ;
		*(.prefix)
		. = ALIGN(16);
		_prefix_end = . ;
	} = 0x9090
	_prefix_size = _prefix_end - _prefix_start;

	/* Start address of Etherboot in the virtual address space */
	_virt_start = 0;
	.text : {
		. = ALIGN(16);
		_text = . ;
		_runtime_start = . ;
	}	
	.text.nocompress : {
		*(.text*.nocompress)
		_compressed_verbatim_end = . ;
		. = ALIGN(16);
	} = 0x9090

	. = ALIGN(16);
	compressed = . ;
	decompress_to = . ;
	.text16 : {
		_text16 = .;
		*(.text16)
		*(.text16.*)
		_etext16 = . ;
	}
	.text.compress : {
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
	_etext = . ;
	_data = . ;
	.data : {
		*(.data)
		*(.data.*)
	}
	_edata = . ;
	_uncompressed_verbatim_end = . ;
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
	/* this is where we copy the compressed image before decompression */
	compressed_copy = . ;

	_zfile_size = DEFINED(_zfile_size) ? _zfile_size : 0 ;
	decompress = DEFINED(decompress) ? decompress : 0;
	_verbatim_end = DEFINED(decompress) ? _compressed_verbatim_end : _uncompressed_verbatim_end ;
	_verbatim_size = _verbatim_end - _verbatim_start + _zfile_size;
	/* _verbatim_size is the actual amount that has to be copied to
	base memory */
	_runtime_size = _runtime_end - _runtime_start ;
	_image_size = _prefix_size + _runtime_size + _zfile_size;
	/* _image_size is the amount of base memory needed to run,
	cf _verbatim_size */
	/* for some reason _image_size / 16 gives the wrong value, ld bug?
	_image_size_pgh = _image_size / 16 ;
	*/
	_image_size_pgh = (_prefix_size + _runtime_size + _zfile_size) / 16 ;
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
		+ _rm_callback_interface_size
		+ _e820mangler_size + 15 ;
}
