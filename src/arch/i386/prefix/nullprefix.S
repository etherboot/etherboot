	.org	0
	.text

	.section ".prefix", "ax", @progbits
	.code16
	.globl	_prefix
_prefix:

	.section ".text16", "ax", @progbits
	.globl	prefix_exit
prefix_exit:

	.globl	prefix_exit_end
prefix_exit_end:
	.previous
