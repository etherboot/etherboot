OUTPUT_ARCH(i386)

SECTIONS
{
	. = 0;
	.hdrs : {
		_hdrs = . ;
		*(.hdrs)
		_ehdrs = . ;
	}
	.img : {
		_img = . ;
		. = ALIGN(16);
		_text = . ;
		*(.text)
		_etext = . ;
		_data = . ;
		*(.data)
		_edata = . ;
	}
	_payload_start = . ;
	_eimg = .;
	/DISCARD/ : {
		*(*)
	}
	_hdrs_size      = _ehdrs - _hdrs;
	_start_offset   = _start - elf_header;
	_image_size     = _eimg - _img;
	_image_mem_size = _eimg - _img;
}
