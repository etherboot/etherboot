OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_prefix_start)
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

	.text.nocompress : {
		*(.prefix.udata)
	} = 0x9090

	decompress_to = . ;
	.prefix.zdata : {
		_compressed = . ;
		*(.prefix.zdata)
		_compressed_end = . ;
	}
	_compressed_size = _compressed_end - _compressed;

	. = ALIGN(16);
	_verbatim_end = . ;


	_base_size = _end - _text;
	_prefix_size = _prefix_end - _prefix_start;

	/* _verbatim_size is the actual amount that has to be copied to base memory */
	_verbatim_size = _verbatim_end - _verbatim_start;
	_exe_size = (_verbatim_size) + 16;
	_exe_size_tail = (_exe_size) % 512;
	_exe_size_pages = ((_exe_size) + 511) / 512;
	_bss_size_pgh = (_bss_size + 15) / 16;
	_rom_size = (_verbatim_size + 511) / 512 ; 

	/* _image_size is the amount of base memory needed to run */
	_image_size = _base_size +  _prefix_size;
	_image_size_pgh = (_image_size + 15) / 16 ;


	/* This is where we copy the compressed image before decompression.
	 * Prepare to decompress in place.  The end mark is about 8.25 bytes long,
	 * and the worst case symbol is about 16.5 bytes long.  Therefore
	 * We need to reserve at least 25 bytes of slack here.  
	 * Currently I reserve 2048 bytes of just slack to be safe :)
	 * 2048 bytes easily falls within the BSS (the defualt stack is 4096 bytes)
	 * so we really are decompressing in place.
	 */
	_compressed_copy = _edata + _prefix_size + 2048 - _compressed_size;

	decompress = DEFINED(decompress) ? decompress : 0;
	/DISCARD/ : {
		*(.comment)
		*(.note)
	}

	/* Symbols used by the prefixes whose addresses are inconvinient 
	 * to compute, at runtime in the code.
	 */
	image_basemem_size = DEFINED(image_basemem_size)? image_basemem_size : 65536;
	image_basemem      = DEFINED(image_basemem)?     image_basemem : 65536;
	_prefix_real_to_prot          = _real_to_prot         + _prefix_size ;
	_prefix_prot_to_real          = _prot_to_real         + _prefix_size ;
	_prefix_image_basemem_size    = image_basemem_size    + _prefix_size ;
	_prefix_image_basemem         = image_basemem         + _prefix_size ;
	_prefix_rm_in_call            = _rm_in_call           + _prefix_size ;
	_prefix_in_call               = _in_call              + _prefix_size ;
	_prefix_rom                   = rom                   + _prefix_size ;
	_prefix_rm_etherboot_location = rm_etherboot_location + _prefix_size ;
}
