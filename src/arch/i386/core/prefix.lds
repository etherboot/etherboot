OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)

SECTIONS {
	.text : {
		*(.text)
	}
	.payload : {
		__payload_start = .;
		*(.data)
		__payload_end = .;
	}
}

