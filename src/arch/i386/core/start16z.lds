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

	/* payload__size is the size of the binary image appended to
	 * start16, in bytes.
	 */
	payload__size = payload__end - payload__start ;

	/* runtime__size is the size of the runtime image
	 * (start32 + the C code), in bytes.
	 */
	runtime__size = runtime__end - runtime__start ;

	/* decompressor__size is the size of the decompressor, in
	 * bytes.  For a non-compressed image, start16.lds sets
	 * decompressor_uncompressed = decompressor__start = 0.
	 */
	decompressor__size = decompressor_uncompressed - decompressor__start ;

	/* image__size is the total size of the image, after
	 * decompression and including the decompressor if applicable.
	 * It is therefore the amount of memory that start16's payload
	 * needs in order to execute, in bytes.
	 */
	image__size = runtime__size + decompressor__size ;

	/* Amount to add to runtime_* symbols to obtain the offset of
	 * that symbol within the image.
	 */
	offset_adjust = decompressor__size - runtime__start ;

	/* Calculations for the stack
	 */
	runtime__stack_size = runtime__estack - runtime__stack ;
	offset__stack = runtime__stack + offset_adjust ;

	/* Calculations for the prefix copy
	 */
	runtime__prefix_copy_size = runtime__eprefix_copy - runtime__prefix_copy ;
	offset__prefix_copy = runtime__prefix_copy + offset_adjust;

	/* Some symbols will be larger than 16 bits but guaranteed to
	 * be multiples of 16.  We calculate them in paragraphs and
	 * export these symbols which can be used in 16-bit code
	 * without risk of overflow.
	 */
	image__size_pgh = ( image__size / 16 );
	runtime__start_pgh = ( runtime__start / 16 );
	decompressor__size_pgh = ( decompressor__size / 16 );
	offset__stack_pgh = ( offset__stack / 16 );
}

