OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)

SECTIONS
{
	. = 65536;
	_text = .;
	.text (.) : { 
		*(.text) 
	} = 0x9090
	.rodata (.): {
		*(.rodata) 
	}
	_etext = . ;
	.data (.): { 
		*(.data) 
		/* Force 4 byte alignment */
		. = ALIGN(4);
		_payload = . ;
		*(.huf)
		_epayload = . ;
	}
	_edata = . ;
	.bss (.): { 
		*(.bss) 
	}
	_end = . ;
}
