/*
 * first.inc  -  constants for assembler module for DOS boot loader
 *
 * Copyright (C) 1996-1998 Gero Kuhlmann   <gero@gkminix.han.de>
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
 * Definitions for accessing the BIOS data area
 */

#define BIOS_SEG	0x0040		/* Segment of BIOS data area */
#define BIOS_FDSTAT	0x0041		/* Floppy disk status byte */
#define BIOS_HDSTAT	0x0074		/* Hard disk status byte */

#define I13_INT		0x13 * 4	/* interrupt vector 13h */
#define I2F_INT		0x2F * 4	/* interrupt vector 2Fh */
#define IF1_INT		0xF1 * 4	/* interrupt vector F1h */
#define IF8_INT		0xF8 * 4	/* interrupt vector F8h */



/*
 *====================================================================
 *
 * Layout of disk boot sector
 */

#define SECT_SIZE	512		/* sector size must be 512 bytes */

#define DISK_BPS	11		/* offset of bytes per sectors */
#define DISK_SECTS	19		/* offset of total number of sects */
#define DISK_SPT	24		/* offset of sectors per track */
#define DISK_HEADS	26		/* offset of number of heads */
#define DISK_BOOT	36		/* offset of disk ID to boot from */

#define PART_STATUS	0x01BE		/* offset of partition status */
#define PART_FRST_HEAD	0x01BF		/* offset of first head number */
#define PART_FRST_SECT	0x01C0		/* offset of first sector number */
#define PART_FRST_CYL	0x01C1		/* offset of first cylinder number */
#define PART_TYPE	0x01C2		/* offset of partition id */
#define PART_LAST_HEAD	0x01C3		/* offset of last head number */
#define PART_LAST_SECT	0x01C4		/* offset of last sector number */
#define PART_LAST_CYL	0x01C5		/* offset of last cylinder number */
#define PART_ABS_SECT	0x01C6		/* offset of first sector number */
#define PART_SEC_NUM	0x01CA		/* offset of number of sectors */

#define PART_ACTIVE	0x80		/* indicates active partition */
#define PART_FAT12	0x01		/* indicates partition type */
#define PART_FAT16	0x04		/* indicates partition type */

#define BOOT_ID_FD	0x00		/* BIOS id of floppy boot disk */
#define BOOT_ID_HD	0x80		/* BIOS id of hard boot disk */
#define BOOT_OFFSET	0x7C00		/* offset for boot block in segment 0 */
#define TEMP_SEGMENT	0x7000		/* segment for temporary storage */



/*
 *====================================================================
 *
 * Layout of BIOS parameter block as used by DOS
 */

#define BPB_BPS		 0		/* offset to bytes per sector */
#define BPB_SPC		 2		/* offset to sectors per cluster */
#define BPB_RES		 3		/* offset to # of reserved sectors */
#define BPB_FATS	 5		/* offset to # of FATs */
#define BPB_DIR		 6		/* offset to # of root dir entries */
#define BPB_TOT_SECTS	 8		/* offset to total # of sectors */
#define BPB_MEDIA_ID	10		/* offset to media ID */
#define BPB_SPF		11		/* offset to # of sectors per FAT */
#define BPB_SPT		13		/* offset to # of sectors per track */
#define BPB_HEADS	15		/* offset to # of heads */
#define BPB_HIDDEN	17		/* offset to # of hidden sectors */



/*
 *====================================================================
 *
 * Flags for DOS drive parameter block
 */

#define DPB_F_FIXED	0x0001		/* fixed media */
#define DPB_F_DOOR	0x0002		/* drive supports door lock status */
#define DPB_F_TSIZE	0x0004		/* all sectors in track are same size */
#define DPB_F_MULTI	0x0008		/* multiple logical units in drive */
#define DPB_F_SHARED	0x0010		/* logical for shared physical drive */
#define DPB_F_CHANGE	0x0020		/* disk change detected */
#define DPB_F_NEWPARA	0x0040		/* device parameters were changed */
#define DPB_F_FORMAT	0x0080		/* disk reformatted */
#define DPB_F_ACCESS	0x0100		/* access flag - fixed media only */

#define DPB_F_DEFAULT	DPB_F_CHANGE + DPB_F_NEWPARA

#define DPB_T_360	0		/* type ID for 360kB disk */
#define DPB_T_1200	1		/* type ID for 1.2MB disk */
#define DPB_T_720	2		/* type ID for 720kB disk */
#define DPB_T_SD8	3		/* type ID for SD 8 inch disk */
#define DPB_T_HD8	4		/* type ID for HD 8 inch disk */
#define DPB_T_FIXED	5		/* type ID for fixed disk */
#define DPB_T_TAPE	6		/* type ID for tape drive */
#define DPB_T_1440	7		/* type ID for 1.44MB disk */
#define DPB_T_OPTICAL	8		/* type ID for optical disk */
#define DPB_T_2880	9		/* type ID for 2.88MB disk */



/*
 *====================================================================
 *
 * Vendor information for the boot rom image. These values have to be
 * identical to those in mknbi.h.
 */

#ifdef	FREEDOS
#define VENDOR_MAGIC	"mknbi-fdos"	/* vendor ID */
#define VENDOR_SIZE	3		/* size of vendor ID in dwords */
#else
#define VENDOR_MAGIC	"mknbi-dos"	/* vendor ID */
#define VENDOR_SIZE	3		/* size of vendor ID in dwords */
#endif

#define VENDOR_BOOTL	16		/* tag for boot loader segment */
#ifdef	FREEDOS
#define VENDOR_RAMDISK	18		/* tag for ramdisk image */
#else
#define VENDOR_RAMDISK	17		/* tag for ramdisk image */
#endif



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

#define BOOT_LD_SECNUM	0		/* offset of total number of sectors */
#define BOOT_LD_SPT	2		/* offset of sectors per track */
#define BOOT_LD_CYL	4		/* offset of number of cylinders */
#define BOOT_LD_DRIVE	6		/* offset of boot drive ID */
#define BOOT_LD_NOHD	7		/* offset of no-hard-disk flag */



/*
 *====================================================================
 *
 * Layout and magic ID of bootp record. Refer to RFC 951, RFC 1048
 * and RFC 1533 for further information. The BOOTP record can be
 * longer than the standard length as the bootrom might be able to
 * load a vendor extension file via tftp.
 */

#define BOOTP_MAGIC_RFC	0x63, 0x82, 0x53, 0x63	/* RFC 1048 vendor ID */
#define BOOTP_MAGIC_CMU	0x43, 0x4D, 0x55, 0x00	/* CMU vendor ID */
#define BOOTP_MAGIC_STA	0x53, 0x54, 0x41, 0x4E	/* Stanford vendor ID */
#define BOOTP_MAGIC_LEN	4			/* length of vendor ID */

#define BOOTP_REQUEST	1		/* bootp OP-code */
#define BOOTP_REPLY	2

#define BOOTP_OP	0		/* offset to bootp OP-code */
#define BOOTP_VEND	236		/* offset to vendor information */
#define BOOTP_SIZE	300		/* size of complete bootp record */

#define BOOTP_RFC_NOP	0		/* RFC vendor tag for NO-OP */
#define BOOTP_RFC_END	255		/* RFC vendor tag for end of record */

