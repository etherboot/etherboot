OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)

SECTIONS
{
	. = 0;
	.text : { 
		_text = .;
		*(.text) 
		_payload = . ;
		*(.img)
		_epayload = . ;
	} = 0x9090
	_data_size = _epayload  - _payload;
	/* FIXME do a better calculation of _image_size! */
	_image_size = _data_size + 65536;
}
