/*
 * first.inc  -  constants for assembler module
 *
 * Copyright (C) 1995,1998 Gero Kuhlmann   <gero@gkminix.han.de>
 * Copyright (C) 1996,1997 Gero Kuhlmann   <gero@gkminix.han.de>
 *                and Markus Gutschke <gutschk@math.uni-muenster.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



/*
 *====================================================================
 *
 * Definitions for allocating dynamic memory, which is used to hold
 * temporary working buffers.
 */

/*
 * The MAX_MEM segment address must be within the range of addresses
 * contained in MIN_MEM number of kB.
 */
#define MIN_MEM		616		/* Minimum number of kB of base mem */
#define MAX_MEM		0x9800		/* Maximum usable segment (see SPEC.DOC) */

#define STACK_SIZE	4096		/* Size of static stack memory */
#define CMDL_SIZE	4096		/* Size of static command line memory */

#define	MAX_PATH_LEN	1024		/* Maximum length of path name */
#define MAX_BOOTP_LEN	 512		/* Maximum length of bootp record */
#define MAX_ADDRS_LEN	 512		/* Maximum length of IP addr string */

#define MAX_DYNAMIC_LEN	MAX_PATH_LEN+MAX_BOOTP_LEN+MAX_ADDRS_LEN



/*
 *====================================================================
 *
 * Vendor information for the boot rom image. These values have to be
 * identical to those in mknbi.h.
 */

#define VENDOR_MAGIC	"mknbi-linux"    /* vendor ID */
#define VENDOR_SIZE	3		 /* size of vendor ID in dwords */

#define VENDOR_BOOTL	16		/* tag for boot loader segment */
#define VENDOR_CMDL	17		/* tag for command line segment */
#define VENDOR_INIT	18		/* tag for floppy boot sector */
#define VENDOR_SETUP	19		/* tag for kernel setup segment */
#define VENDOR_KERNEL	20		/* tag for kernel segment */
#define VENDOR_RAMDISK	21		/* tag for ramdisk image */



/*
 *====================================================================
 *
 * Layout and magic ID of boot rom image header. These values have to
 * be identical to those used by the boot rom. See SPEC.DOC of the
 * boot rom source for further information.
 */

#define	BOOT_MAGIC	0x1B031336	/* boot image header magic cookie */
#define BOOT_SIZE	512		/* total size of boot image header */

#define BOOT_HD_SIZE	4		/* size of header in dwords */
#define BOOT_HD_MAGIC	0		/* offset for header magic number */
#define BOOT_HD_LENGTH	4		/* offset for header length */
#define BOOT_HD_FLAG1	5		/* offset for header flag 1 */
#define BOOT_HD_FLAG2	6		/* offset for header flag 2 */
#define BOOT_HD_FLAG3	7		/* offset for header flag 3 */
#define BOOT_HD_LOCN	8		/* offset for header location */
#define BOOT_HD_EXEC	12		/* offset for execute address */
#define BOOT_HD_VENDOR	16		/* offset for header vendor information */

#define BOOT_LD_SIZE	4		/* size of load record in dwords */
#define BOOT_LD_LENGTH	0		/* offset for load record length */
#define BOOT_LD_TAG1	1		/* offset for load record tag 1 */
#define BOOT_LD_TAG2	2		/* offset for load record tag 2 */
#define BOOT_LD_FLAGS	3		/* offset for load record flags */
#define BOOT_LD_ADDR	4		/* offset for absolute address */
#define BOOT_LD_ILENGTH	8		/* offset for image length */
#define BOOT_LD_MLENGTH	12		/* offset for memory length */
#define BOOT_LD_VENDOR	16		/* offset for vendor information */

#define BOOT_FLAG_B0	0x01		/* mask for load record flag B0 */
#define BOOT_FLAG_B1	0x02		/* mask for load record flag B1 */
#define BOOT_FLAG_EOF	0x04		/* mask for load record flag EOF */


/*
 *====================================================================
 *
 * Ramdisk images can be loaded in three different ways:
 *  RD_AUTO  --  The ramdisk image is loaded at either 0x100000 or right
 *               behind the kernel image; at run-time it will be moved
 *               to the end of physical memory or right before 0x1000000.
 *               This technique guarantees maximum compatibility with
 *               broken BIOS versions without making assumptions on the
 *               way, the BOOT-Prom determined the end of memory. The
 *               drawback is, that moving the image costs some extra time.
 *  RD_EOM   --  The ramdisk image is loaded right before the end of
 *               physical memory. It is the BOOT-Proms responsibility to do
 *               this reliably. This might cause problems with some ancient
 *               BIOS versions, if the computer is equipped with more then
 *               16M of main memory.
 *  RD_FIXED --  The user provided a fixed load address for the ramdisk
 *               image. This entails possible compatibility restrictions
 *               when moving the image from one client to another. OTOH,
 *               it could be the final solution if everything else fails...
 */

#define RD_AUTO		0
#define RD_EOM		1
#define RD_FIXED	2


/*
 *====================================================================
 *
 * Layout and magic ID of bootp record. Refer to RFC 951, RFC 1048
 * and RFC 1533 for further information. Vendor tag 129 is special in
 * that it is in the range of tags which can be freely used (see
 * RFC 1533). It will be used by first.S to add a string at the end
 * of the kernel command line. The BOOTP record can be longer than
 * the standard length as the bootrom might be able to load a vendor
 * extension file via tftp.
 */

#define BOOTP_MAGIC_RFC	0x63, 0x82, 0x53, 0x63	/* RFC 1048 vendor ID */
#define BOOTP_MAGIC_CMU	0x43, 0x4D, 0x55, 0x00	/* CMU vendor ID */
#define BOOTP_MAGIC_STA	0x53, 0x54, 0x41, 0x4E	/* Stanford vendor ID */
#define BOOTP_MAGIC_LEN	4			/* length of vendor ID */

#define BOOTP_REQUEST	1		/* bootp OP-code */
#define BOOTP_REPLY	2

#define BOOTP_OP	0		/* offset to bootp OP-code */
#define BOOTP_HTYPE	1		/* offset to hardware type */
#define BOOTP_HLEN	2		/* offset to length of hardware addr */
#define BOOTP_HOPS	3		/* offset to number of gateway hops */
#define BOOTP_XID	4		/* offset to transfer ID */
#define BOOTP_SECS	8		/* offset to seconds since boot began */
#define BOOTP_CIADDR	12		/* offset to client IP address */
#define BOOTP_YIADDR	16		/* offset to "your" IP address */
#define BOOTP_SIADDR	20		/* offset to server IP address  */
#define BOOTP_GIADDR	24		/* offset to gateway IP address */
#define BOOTP_CHADDR	28		/* offset to hardware address */
#define BOOTP_SNAME	44		/* offset to server name */
#define BOOTP_FILE	108		/* offset to name of boot file */
#define BOOTP_VEND	236		/* offset to vendor information */

#define BOOTP_SIZE	300		/* size of complete bootp record */

#define BOOTP_RFC_NOP	0		/* RFC vendor tag for NO-OP */
#define BOOTP_RFC_MSK	1		/* RFC vendor tag for netmask */
#define BOOTP_RFC_GWY	3		/* RFC vendor tag for gateway */
#define BOOTP_RFC_HNAME	12		/* RFC vendor tag for host name */
#define BOOTP_RFC_ROOT	17		/* RFC vendor tag for root directory */
#define BOOTP_RFC_VID	128		/* Vendor tag: magic identification */
#ifndef BOOTP_RFC_CMDAD
#define BOOTP_RFC_CMDAD	129		/* Addition to kernel command line */
#endif
#ifndef BOOTP_RFC_RDEV
#define BOOTP_RFC_RDEV	130		/* Device by which to find root FS */
#endif
#define BOOTP_RFC_VSEL	176		/* index to user selected image */
#define BOOTP_RFC_END	255		/* RFC vendor tag for end of record */


#define BOOTP_VID_1	0x45E4
#define BOOTP_VID_2	0x6874



/*
 *====================================================================
 *
 * The command line descriptor is placed in the floppy boot sector area
 * and looks like this:
 *
 * 0x90020-0x90021     2 bytes   command line magic number
 * 0x90022-0x90023     2 bytes   command line offs. relative to floppy boot sec
 * 0x901FA-0x901FB     2 bytes   video mode
 */

#define CL_MAGIC_ADDR	0x20		/* command line magic number */
#define CL_MAGIC	0xA33F		/* very unusual command sequence */
#define CL_OFFSET	0x22		/* command line offset */
#define VGA_OFFSET	0x1FA		/* video mode */



/*
 *====================================================================
 *
 * The setup-header has to be changed in order to tell the kernel about a
 * loaded ramdisk image
 *
 * +2                  4 bytes   setup magic number
 * +6                  2 bytes   setup version number
 * +16                 1 byte    type of loader
 * +17                 1 byte    load flags
 *                               bit 0: loaded high
 *                               bit 7: can use heap
 * +18                 2 bytes   setup move size
 * +20                 4 bytes   protected mode start address
 * +24                 4 bytes   ramdisk image start address
 * +28                 4 bytes   ramdisk image size
 * +32                 2 bytes   heap pointer
 *
 * This only works with Linux kernels 1.3.73 or above; that is why the
 * setup magic number and the setup version number need to be verified
 * before relying on the layout of this data structure.
 */

#define SU_MAGIC_ADDR		2
#define SU_VERSION_ADDR		6
#define SU_TYPE_OF_LOADER	16
#define SU_LOAD_FLAGS		17
#define SU_MOVE_SIZE		18
#define	SU_32_START		20
#define SU_RAMDISK_START	24
#define	SU_RAMDISK_SIZE		28
#define SU_HEAP			32

#define SU_MAGIC_L		0x6448
#define SU_MAGIC_H		0x5372
#define SU_VERSION		0x0201
#define SU_MY_TYPE_OF_LOADER	0x41
#define	SU_MY_LOAD_FLAGS	0x80
#define SU_HEAP_END		0x9E00

