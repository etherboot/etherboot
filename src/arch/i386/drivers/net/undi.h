/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
UNDI NIC driver for Etherboot - header file

This file copyright (C) Michael Brown <mbrown@fensystems.co.uk> 2003.
All rights reserved.

$Id$
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

/* Include pxe.h from FreeBSD.
 * pxe.h defines PACKED, which etherboot.h has already defined.
 */

#undef PACKED
#include "pxe.h"

/* __undi_call is the assembler wrapper to the real-mode UNDI calls.
 * Pass it the real-mode segment:offset address of an undi_call_info_t
 * structure.  The parameters are only uint16_t, but GCC will push
 * them on the stack as uint32_t anyway for the sake of alignment.  We
 * specify them here as uint32_t to remove ambiguity.
 */

typedef struct undi_call_info {
	SEGOFF16_t	routine;
	uint16_t	stack[3];
} undi_call_info_t;

typedef uint16_t PXENV_EXIT_t;
#define PXENV_EXIT_SUCCESS 0x0000
#define PXENV_EXIT_FAILURE 0x0001
PXENV_EXIT_t __undi_call ( uint32_t, uint32_t );

/* The UNDI loader parameter structure is not defined in pxe.h
 */

typedef struct undi_loader {
	PXENV_STATUS_t	status;
	uint16_t	ax;
	uint16_t	bx;
	uint16_t	dx;
	uint16_t	di;
	uint16_t	es;
	uint16_t	undi_ds;
	uint16_t	undi_cs;
	uint16_t	pxe_off;
	uint16_t	pxenv_off;
} undi_loader_t;

/* A union that can function as the parameter block for any UNDI API call.
 */

typedef union pxenv_structure {
	PXENV_STATUS_t			Status; /* Make it easy to read status
						   for any operation */
	undi_loader_t			loader;
	t_PXENV_START_UNDI		start_undi;
	t_PXENV_UNDI_STARTUP		undi_startup;
	t_PXENV_UNDI_CLEANUP		undi_cleanup;
	t_PXENV_UNDI_INITIALIZE		undi_initialize;
	t_PXENV_UNDI_SHUTDOWN		undi_shutdown;
	t_PXENV_UNDI_GET_INFORMATION	undi_get_information;
	t_PXENV_STOP_UNDI		stop_undi;
} pxenv_structure_t;

/* BIOS PnP parameter block.  We scan for this so that we can pass it
 * to the UNDI driver.
 */

#define PNP_BIOS_SIGNATURE ( ('$'<<0) + ('P'<<8) + ('n'<<16) + ('P'<<24) )
typedef struct pnp_bios {
	uint32_t	signature;
	uint8_t		version;
	uint8_t		length;
	uint16_t	control;
	uint8_t		checksum;
	uint8_t		dontcare[24];
} PACKED pnp_bios_t;

/* Structures within the PXE ROM.
 */

#define ROM_SIGNATURE 0xaa55
typedef struct rom {
	uint16_t	signature;
	uint8_t		unused[0x14];
	uint16_t	undi_rom_id_off;
	uint16_t	pcir_off;
	uint16_t	pnp_off;
} PACKED rom_t;	

#define PCIR_SIGNATURE ( ('P'<<0) + ('C'<<8) + ('I'<<16) + ('R'<<24) )
typedef struct pcir_header {
	uint32_t	signature;
	uint16_t	vendor_id;
	uint16_t	device_id;
} PACKED pcir_header_t;

#define PNP_SIGNATURE ( ('$'<<0) + ('P'<<8) + ('n'<<16) + ('P'<<24) )
typedef struct pnp_header {
	uint32_t	signature;
	uint8_t		struct_revision;
	uint8_t		length;
	uint16_t	next;
	uint8_t		reserved;
	uint8_t		checksum;
	uint16_t	id[2];
	uint16_t	manuf_str_off;
	uint16_t	product_str_off;
	uint8_t		base_type;
	uint8_t		sub_type;
	uint8_t		interface_type;
	uint8_t		indicator;
	uint16_t	boot_connect_off;
	uint16_t	disconnect_off;
	uint16_t	initialise_off;
	uint16_t	reserved2;
	uint16_t	info;
} PACKED pnp_header_t;

#define UNDI_SIGNATURE ( ('U'<<0) + ('N'<<8) + ('D'<<16) + ('I'<<24) )
typedef struct undi_rom_id {
	uint32_t	signature;
	uint8_t		struct_length;
	uint8_t		struct_cksum;
	uint8_t		struct_rev;
	uint8_t		undi_rev[3];
	uint16_t	undi_loader_off;
	uint16_t	stack_size;
	uint16_t	data_size;
	uint16_t	code_size;
} PACKED undi_rom_id_t;

/* Driver private data structure.
 */

typedef struct undi {
	pnp_bios_t	*pnp_bios;
	rom_t		*rom;
	undi_rom_id_t	*undi_rom_id;
	pxe_t		*pxe;
} undi_t;

/* Constants
 */

#define UNDI_DRIVER_DATA_SEGMENT 0x5000
#define UNDI_DRIVER_CODE_SEGMENT 0x6000

#define HUNT_FOR_PIXIES 0
#define HUNT_FOR_UNDI_ROMS 1
