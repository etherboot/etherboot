OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)

SECTIONS {
	.text : {
		*(.text)
	}
	.payload : {
		payload__start = .;
		*(.data)
		payload__end = .;
	}

	/* payload__size is the size of the binary image appended to start16 */
	payload__size = payload__end - payload__start ;

	/* runtime__size is the size of the runtime image (start32 + C files) */
	runtime__size = runtime__end - runtime__start ;

	/* image__size is the total size of the image, after
	 * decompression and including the decompressor if applicable.
	 * It is therefore the amount of memory that start16's payload
	 * needs in order to execute.
	 */
	image__size = runtime__size ;
}
