#ifdef IDE_DISK

#include "etherboot.h"
#include "timer.h"

#define BSY_SET_DURING_SPINUP 1
/*
 *   UBL, The Universal Talkware Boot Loader 
 *    Copyright (C) 2000 Universal Talkware Inc.
 *    Copyright (C) 2002 Eric Biederman
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version. 
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *
 */

struct harddisk_info {
	uint16_t controller_port;
	uint16_t heads;
	uint16_t cylinders;
	uint16_t sectors_per_track;
	uint8_t  model_number[41];
	uint8_t  slave;
	sector_t sectors;
	int  address_mode;	/* am i lba (0x40) or chs (0x00) */
#define ADDRESS_MODE_CHS   0
#define ADDRESS_MODE_LBA   1
#define ADDRESS_MODE_LBA48 2
	int drive_exists;
	int slave_absent;
	int basedrive;
};

#define NUM_HD (8)

#define IDE_SECTOR_SIZE 0x200

#define IDE_BASE1             (0x1F0u) /* primary controller */
#define IDE_BASE2             (0x170u) /* secondary */
#define IDE_BASE3             (0x0F0u) /* third */
#define IDE_BASE4             (0x070u) /* fourth */

#define IDE_REG_EXTENDED_OFFSET   (0x200u)

#define IDE_REG_DATA(base)           ((base) + 0u) /* word register */
#define IDE_REG_ERROR(base)          ((base) + 1u)
#define IDE_REG_PRECOMP(base)        ((base) + 1u)
#define IDE_REG_FEATURE(base)        ((base) + 1u)
#define IDE_REG_SECTOR_COUNT(base)   ((base) + 2u)
#define IDE_REG_SECTOR_NUMBER(base)  ((base) + 3u)
#define IDE_REG_LBA_LOW(base)        ((base) + 3u)
#define IDE_REG_CYLINDER_LSB(base)   ((base) + 4u)
#define IDE_REG_LBA_MID(base)	     ((base) + 4u)
#define IDE_REG_CYLINDER_MSB(base)   ((base) + 5u)
#define IDE_REG_LBA_HIGH(base)	     ((base) + 5u)
#define IDE_REG_DRIVEHEAD(base)      ((base) + 6u)
#define IDE_REG_DEVICE(base)	     ((base) + 6u)
#define IDE_REG_STATUS(base)         ((base) + 7u)
#define IDE_REG_COMMAND(base)        ((base) + 7u)
#define IDE_REG_ALTSTATUS(base)      ((base) + IDE_REG_EXTENDED_OFFSET + 6u)
#define IDE_REG_DEVICE_CONTROL(base) ((base) + IDE_REG_EXTENDED_OFFSET + 6u)
#define IDE_REG_ADDRESS(base)        ((base) + IDE_REG_EXTENDED_OFFSET + 7u)

struct ide_pio_command
{
	uint8_t feature;
	uint8_t sector_count;
	uint8_t lba_low;
	uint8_t lba_mid;
	uint8_t lba_high;
	uint8_t device;
#       define IDE_DH_DEFAULT (0xA0)
#       define IDE_DH_HEAD(x) ((x) & 0x0F)
#       define IDE_DH_MASTER  (0x00)
#       define IDE_DH_SLAVE   (0x10)
#       define IDE_DH_LBA     (0x40)
#       define IDE_DH_CHS     (0x00)
	uint8_t command;
	uint8_t sector_count2;
	uint8_t lba_low2;
	uint8_t lba_mid2;
	uint8_t lba_high2;
};

#define IDE_DEFAULT_COMMAND { 0xFFu, 0x01, 0x00, 0x0000, IDE_DH_DEFAULT }

#define IDE_ERR_ICRC	0x80	/* ATA Ultra DMA bad CRC */
#define IDE_ERR_BBK	0x80	/* ATA bad block */
#define IDE_ERR_UNC	0x40	/* ATA uncorrected error */
#define IDE_ERR_MC	0x20	/* ATA media change */
#define IDE_ERR_IDNF	0x10	/* ATA id not found */
#define IDE_ERR_MCR	0x08	/* ATA media change request */
#define IDE_ERR_ABRT	0x04	/* ATA command aborted */
#define IDE_ERR_NTK0	0x02	/* ATA track 0 not found */
#define IDE_ERR_NDAM	0x01	/* ATA address mark not found */

#define IDE_STATUS_BSY	0x80	/* busy */
#define IDE_STATUS_RDY	0x40	/* ready */
#define IDE_STATUS_DF	0x20	/* device fault */
#define IDE_STATUS_WFT	0x20	/* write fault (old name) */
#define IDE_STATUS_SKC	0x10	/* seek complete */
#define IDE_STATUS_DRQ	0x08	/* data request */
#define IDE_STATUS_CORR	0x04	/* corrected */
#define IDE_STATUS_IDX	0x02	/* index */
#define IDE_STATUS_ERR	0x01	/* error (ATA) */
#define IDE_STATUS_CHK	0x01	/* check (ATAPI) */

#define IDE_CTRL_HD15	0x08	/* bit should always be set to one */
#define IDE_CTRL_SRST	0x04	/* soft reset */
#define IDE_CTRL_NIEN	0x02	/* disable interrupts */


/* Most mandtory and optional ATA commands (from ATA-3), */

#define IDE_CMD_CFA_ERASE_SECTORS            0xC0
#define IDE_CMD_CFA_REQUEST_EXT_ERR_CODE     0x03
#define IDE_CMD_CFA_TRANSLATE_SECTOR         0x87
#define IDE_CMD_CFA_WRITE_MULTIPLE_WO_ERASE  0xCD
#define IDE_CMD_CFA_WRITE_SECTORS_WO_ERASE   0x38
#define IDE_CMD_CHECK_POWER_MODE1            0xE5
#define IDE_CMD_CHECK_POWER_MODE2            0x98
#define IDE_CMD_DEVICE_RESET                 0x08
#define IDE_CMD_EXECUTE_DEVICE_DIAGNOSTIC    0x90
#define IDE_CMD_FLUSH_CACHE                  0xE7
#define IDE_CMD_FORMAT_TRACK                 0x50
#define IDE_CMD_IDENTIFY_DEVICE              0xEC
#define IDE_CMD_IDENTIFY_DEVICE_PACKET       0xA1
#define IDE_CMD_IDENTIFY_PACKET_DEVICE       0xA1
#define IDE_CMD_IDLE1                        0xE3
#define IDE_CMD_IDLE2                        0x97
#define IDE_CMD_IDLE_IMMEDIATE1              0xE1
#define IDE_CMD_IDLE_IMMEDIATE2              0x95
#define IDE_CMD_INITIALIZE_DRIVE_PARAMETERS  0x91
#define IDE_CMD_INITIALIZE_DEVICE_PARAMETERS 0x91
#define IDE_CMD_NOP                          0x00
#define IDE_CMD_PACKET                       0xA0
#define IDE_CMD_READ_BUFFER                  0xE4
#define IDE_CMD_READ_DMA                     0xC8
#define IDE_CMD_READ_DMA_QUEUED              0xC7
#define IDE_CMD_READ_MULTIPLE                0xC4
#define IDE_CMD_READ_SECTORS                 0x20
#define IDE_CMD_READ_SECTORS_EXT             0x24
#define IDE_CMD_READ_VERIFY_SECTORS          0x40
#define IDE_CMD_RECALIBRATE                  0x10
#define IDE_CMD_SEEK                         0x70
#define IDE_CMD_SET_FEATURES                 0xEF
#define IDE_CMD_SET_MULTIPLE_MODE            0xC6
#define IDE_CMD_SLEEP1                       0xE6
#define IDE_CMD_SLEEP2                       0x99
#define IDE_CMD_STANDBY1                     0xE2
#define IDE_CMD_STANDBY2                     0x96
#define IDE_CMD_STANDBY_IMMEDIATE1           0xE0
#define IDE_CMD_STANDBY_IMMEDIATE2           0x94
#define IDE_CMD_WRITE_BUFFER                 0xE8
#define IDE_CMD_WRITE_DMA                    0xCA
#define IDE_CMD_WRITE_DMA_QUEUED             0xCC
#define IDE_CMD_WRITE_MULTIPLE               0xC5
#define IDE_CMD_WRITE_SECTORS                0x30
#define IDE_CMD_WRITE_VERIFY                 0x3C

/* IDE_CMD_SET_FEATURE sub commands */
#define IDE_FEATURE_CFA_ENABLE_8BIT_PIO                     0x01
#define IDE_FEATURE_ENABLE_WRITE_CACHE                      0x02
#define IDE_FEATURE_SET_TRANSFRE_MODE                       0x03
#define IDE_FEATURE_ENABLE_POWER_MANAGEMENT                 0x05
#define IDE_FEATURE_ENABLE_POWERUP_IN_STANDBY               0x06
#define IDE_FEATURE_STANDBY_SPINUP_DRIVE                    0x07
#define IDE_FEATURE_CFA_ENABLE_POWER_MODE1                  0x0A
#define IDE_FEATURE_DISABLE_MEDIA_STATUS_NOTIFICATION       0x31
#define IDE_FEATURE_ENABLE_AUTOMATIC_ACOUSTIC_MANAGEMENT    0x42
#define IDE_FEATURE_SET_MAXIMUM_HOST_INTERFACE_SECTOR_TIMES 0x43
#define IDE_FEATURE_DISABLE_READ_LOOKAHEAD                  0x55
#define IDE_FEATURE_ENABLE_RELEASE_INTERRUPT                0x5D
#define IDE_FEATURE_ENABLE_SERVICE_INTERRUPT                0x5E
#define IDE_FEATURE_DISABLE_REVERTING_TO_POWERON_DEFAULTS   0x66
#define IDE_FEATURE_CFA_DISABLE_8BIT_PIO                    0x81
#define IDE_FEATURE_DISABLE_WRITE_CACHE                     0x82
#define IDE_FEATURE_DISABLE_POWER_MANAGEMENT                0x85
#define IDE_FEATURE_DISABLE_POWERUP_IN_STANDBY              0x86
#define IDE_FEATURE_CFA_DISABLE_POWER_MODE1                 0x8A
#define IDE_FEATURE_ENABLE_MEDIA_STATUS_NOTIFICATION        0x95
#define IDE_FEATURE_ENABLE_READ_LOOKAHEAD                   0xAA
#define IDE_FEATURE_DISABLE_AUTOMATIC_ACOUSTIC_MANAGEMENT   0xC2
#define IDE_FEATURE_ENABLE_REVERTING_TO_POWERON_DEFAULTS    0xCC
#define IDE_FEATURE_DISABLE_SERVICE_INTERRUPT               0xDE



struct harddisk_info harddisk_info[2];

static int await_ide(int (*done)(unsigned long base), 
	unsigned long base, unsigned long timeout)
{
	int result;
	for(;;) {
		result = done(base);
		if (result) {
			return 0;
		}
		if (iskey() && (getchar() == ESC)) {
			longjmp(restart_etherboot, -1);
		}
		if ((timeout == 0) || (currticks() > timeout)) {
			break;
		}
	}
	return -1;
}

/* The maximum time any IDE command can last 31 seconds,
 * So if any IDE commands takes this long we know we have problems.
 */
#define IDE_TIMEOUT (32*TICKS_PER_SEC)

static int not_bsy(unsigned long base)
{
	return !(inb(IDE_REG_STATUS(base)) & IDE_STATUS_BSY);
}
#if  !BSY_SET_DURING_SPINUP
static int timeout(unsigned long base)
{
	return 0;
}
#endif

static int ide_software_reset(unsigned base)
{
	/* Wait a little bit in case this is immediately after
	 * hardware reset.
	 */
	mdelay(2);
	/* A software reset should not be delivered while the bsy bit
	 * is set.  If the bsy bit does not clear in a reasonable
	 * amount of time give up.
	 */
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}

	/* Disable Interrupts and reset the ide bus */
	outb(IDE_CTRL_HD15 | IDE_CTRL_SRST | IDE_CTRL_NIEN, 
		IDE_REG_DEVICE_CONTROL(base));
	udelay(5);
	outb(IDE_CTRL_HD15 | IDE_CTRL_NIEN, IDE_REG_DEVICE_CONTROL(base));
	mdelay(2);
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	return 0;
}

static void pio_set_registers(
	unsigned long base, const struct ide_pio_command *cmd)
{
	uint8_t device;
	/* Disable Interrupts */
	outb(IDE_CTRL_HD15 | IDE_CTRL_NIEN, IDE_REG_DEVICE_CONTROL(base));

	/* Possibly switch selected device */
	device = inb(IDE_REG_DEVICE(base));
	outb(cmd->device,          IDE_REG_DEVICE(base));
	if ((device & (1UL << 4)) != (cmd->device & (1UL << 4))) {
		/* Allow time for the selected drive to switch,
		 * The linux ide code suggests 50ms is the right
		 * amount of time to use here.
		 */
		mdelay(50); 
	}
	outb(cmd->feature,         IDE_REG_FEATURE(base));
	outb(cmd->sector_count2,   IDE_REG_SECTOR_COUNT(base));
	outb(cmd->sector_count,    IDE_REG_SECTOR_COUNT(base));
	outb(cmd->lba_low2,        IDE_REG_LBA_LOW(base));
	outb(cmd->lba_low,         IDE_REG_LBA_LOW(base));
	outb(cmd->lba_mid2,        IDE_REG_LBA_MID(base));
	outb(cmd->lba_mid,         IDE_REG_LBA_MID(base));
	outb(cmd->lba_high2,       IDE_REG_LBA_HIGH(base));
	outb(cmd->lba_high,        IDE_REG_LBA_HIGH(base));
	outb(cmd->command,         IDE_REG_COMMAND(base));
	
}


static int pio_non_data(unsigned long base, const struct ide_pio_command *cmd)
{
	/* Wait until the busy bit is clear */
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}

	pio_set_registers(base, cmd);
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	/* FIXME is there more error checking I could do here? */
	return 0;
}

static int pio_data_in(unsigned long base, const struct ide_pio_command *cmd,
	void *buffer, size_t bytes)
{
	unsigned int status;

	/* FIXME handle commands with multiple blocks */
	/* Wait until the busy bit is clear */
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}

	/* How do I tell if INTRQ is asserted? */
	pio_set_registers(base, cmd);
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	status = inb(IDE_REG_STATUS(base));
	if (!(status & IDE_STATUS_DRQ)) {
		return -1;
	}
	insw(IDE_REG_DATA(base), buffer, bytes/2);
	status = inb(IDE_REG_STATUS(base));
	if (status & IDE_STATUS_DRQ) {
		return -1;
	}
	return 0;
}

#if 0
static int pio_packet(unsigned long base, int in,
	const void *packet, int packet_len,
	void *buffer, int buffer_len)
{
	const uint8_t *pbuf;
	unsigned int status;
	struct ide_pio_command cmd;
	memset(&cmd, 0, sizeof(cmd));

	/* Wait until the busy bit is clear */
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	pio_set_registers(base, cmd);
	ndelay(400);
	if (await_ide(not_bsy, base, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	status = inb(IDE_REG_STATUS(base));
	if (!(status & IDE_STATUS_DRQ)) {
		return -1;
	}
	while(packet_len > 1) {
		outb(*pbuf, IDE_REG_DATA(base));
		pbuf++;
		packet_len -= 1;
	}
	inb(IDE_REG_ALTSTATUS(base));
	if (await_ide){}
	/*FIXME finish this function */
	
	
}
#endif

static inline int ide_read_sector_chs(
	struct harddisk_info *info, void *buffer, unsigned long sector)
{
	struct ide_pio_command cmd;
	unsigned int track;
	unsigned int offset;
	unsigned int cylinder;
		
	memset(&cmd, 0, sizeof(cmd));
	cmd.sector_count = 1;

	track = sector / info->sectors_per_track;
	/* Sector number */
	offset = 1 + (sector % info->sectors_per_track);
	cylinder = track / info->heads;
	cmd.lba_low = offset;
	cmd.lba_mid = cylinder & 0xff;
	cmd.lba_high = (cylinder >> 8) & 0xff;
	cmd.device = IDE_DH_DEFAULT |
		IDE_DH_HEAD(track % info->heads) |
		info->slave |
		IDE_DH_CHS;
	cmd.command = IDE_CMD_READ_SECTORS;
	return pio_data_in(info->controller_port, &cmd, buffer, IDE_SECTOR_SIZE);
}

static inline int ide_read_sector_lba(
	struct harddisk_info *info, void *buffer, unsigned long sector)
{
	struct ide_pio_command cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.sector_count = 1;
	cmd.lba_low = sector & 0xff;
	cmd.lba_mid = (sector >> 8) & 0xff;
	cmd.lba_high = (sector >> 16) & 0xff;
	cmd.device = IDE_DH_DEFAULT |
		((sector >> 24) & 0x0f) |
		info->slave | 
		IDE_DH_LBA;
	cmd.command = IDE_CMD_READ_SECTORS;
	return pio_data_in(info->controller_port, &cmd, buffer, IDE_SECTOR_SIZE);
}

static inline int ide_read_sector_lba48(
	struct harddisk_info *info, void *buffer, sector_t sector)
{
	/* Warning LBA48 mode has not been tested */
	struct ide_pio_command cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.sector_count = 1;
	cmd.lba_low = sector & 0xff;
	cmd.lba_mid = (sector >> 8) & 0xff;
	cmd.lba_high = (sector >> 16) & 0xff;
	cmd.lba_low2 = (sector >> 24) & 0xff;
	cmd.lba_mid2 = (sector >> 32) & 0xff;
	cmd.lba_high2 = (sector >> 40) & 0xff;
	cmd.device = IDE_DH_DEFAULT | info->slave | IDE_DH_LBA;
	cmd.command = IDE_CMD_READ_SECTORS_EXT;
	return pio_data_in(info->controller_port, &cmd, buffer, IDE_SECTOR_SIZE);
}


static int ide_read_sector(struct harddisk_info *info, void *buffer, sector_t sector)
{
	int result;
	if (sector > info->sectors) {
		return -1;
	}
	if (info->address_mode == ADDRESS_MODE_CHS) {
		result = ide_read_sector_chs(info, buffer, sector);
	}
	else if (info->address_mode == ADDRESS_MODE_LBA) {
		result = ide_read_sector_lba(info, buffer, sector);
	}
	else if (info->address_mode == ADDRESS_MODE_LBA48) {
		result = ide_read_sector_lba48(info, buffer, sector);
	}
	else {
		result = -1;
	}
	return result;
}

static int init_drive(struct harddisk_info *info, unsigned base, int slave,
	int basedrive, unsigned char *buffer)
{
	uint16_t* drive_info;
	struct ide_pio_command cmd;
	int i;

	info->controller_port = base;
	info->heads = 0u;
	info->cylinders = 0u;
	info->sectors_per_track = 0u;
	info->address_mode = IDE_DH_CHS;
	info->sectors = 0ul;
	info->drive_exists = 0;
	info->slave_absent = 0;
	info->slave = slave?IDE_DH_SLAVE: IDE_DH_MASTER;
	info->basedrive = basedrive;

#if 0
	printf("Testing for disk %d\n", info->basedrive);
#endif

	/* Select the drive that we are testing */
	outb(IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | info->slave, 
		IDE_REG_DEVICE(base));
	mdelay(50);

	/* Test to see if the drive registers exist,
	 * In many cases this quickly rules out a missing drive.
	 */
	for(i = 0; i < 4; i++) {
		outb(0xaa + i, base + 2 + i);
	}
	for(i = 0; i < 4; i++) {
		if (inb(base + 2 + i) != 0xaa + i) {
			return 1;
		}
	}
	for(i = 0; i < 4; i++) {
		outb(0x55 + i, base + 2 + i);
	}
	for(i = 0; i < 4; i++) {
		if (inb(base + 2 + i) != 0x55 + i) {
			return 1;
		}
	}
#if 0
	printf("Probing for disk %d\n", info->basedrive);
#endif
	
	memset(&cmd, 0, sizeof(cmd));
	cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | info->slave;
	cmd.command = IDE_CMD_IDENTIFY_DEVICE;

	
	if (pio_data_in(base, &cmd, buffer, IDE_SECTOR_SIZE) < 0) {
		/* Well, if that command didn't work, we probably don't have drive. */
		return 1;
	}
	

	/* Now suck the data out */
	drive_info = (uint16_t *)buffer;
	if (drive_info[2] == 0x37C8) {
		/* If the response is incomplete spin up the drive... */
		memset(&cmd, 0, sizeof(cmd));
		cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS |
			info->slave;
		cmd.feature = IDE_FEATURE_STANDBY_SPINUP_DRIVE;
		if (pio_non_data(base, &cmd) < 0) {
			/* If the command doesn't work give up on the drive */
			return 1;
		}
		
	}
	if ((drive_info[2] == 0x37C8) || (drive_info[2] == 0x8C73)) {
		/* The response is incomplete retry the drive info command */
		memset(&cmd, 0, sizeof(cmd));
		cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS |
			info->slave;
		cmd.command = IDE_CMD_IDENTIFY_DEVICE;
		if(pio_data_in(base, &cmd, buffer, IDE_SECTOR_SIZE) < 0) {
			/* If the command didn't work give up on the drive. */
			return 1;
		}
	}
	if ((drive_info[2] != 0x37C8) && 
		(drive_info[2] != 0x738C) &&
		(drive_info[2] != 0x8C73) && 
		(drive_info[2] != 0xC837) &&
		(drive_info[2] != 0x0000)) {
		printf("Invalid IDE Configuration: %hx\n", drive_info[2]);
		return 1;
	}
	for(i = 27; i < 47; i++) {
		info->model_number[((i-27)<< 1)] = (drive_info[i] >> 8) & 0xff;
		info->model_number[((i-27)<< 1)+1] = drive_info[i] & 0xff;
	}
	info->model_number[40] = '\0';
	info->drive_exists = 1;

	/* See if LBA is supported */
	if (drive_info[49] & (1 << 9)) {
		info->address_mode = ADDRESS_MODE_LBA;
		info->sectors = (drive_info[61] << 16) | (drive_info[60]);
		if (drive_info[83] & (1 << 10)) {
			/* Should LBA48 depend on LBA? */
			printf("Warning LBA48 not yet tested\n");
			info->address_mode = ADDRESS_MODE_LBA48;
			if ((sizeof(sector_t) < sizeof(uint64_t)) &&
				drive_info[103] || drive_info[102]) {
				/* FIXME use a 64bit sector number */
				printf("Drive to big\n");
				return -1;
			} 
			info->sectors = 
				(((sector_t)drive_info[103]) << 48) |
				(((sector_t)drive_info[102]) << 32) |
				(((sector_t)drive_info[101]) << 16) |
				(((sector_t)drive_info[100]) <<  0);
		}
	} else {
		info->address_mode = ADDRESS_MODE_CHS;
		info->heads = drive_info[3];
		info->cylinders = drive_info[1];
		info->sectors_per_track = drive_info[6];
		info->sectors = 
			info->sectors_per_track *
			info->heads *
			info->cylinders;
		printf(__FUNCTION__ " sectors_per_track=[%d], heads=[%d], cylinders=[%d]\n",
			info->sectors_per_track,
			info->heads,
			info->cylinders);
	}
	/* See if we have a slave */
	if (!info->slave && (((drive_info[93] >> 14) & 3) == 1)) {
		info->slave_absent = !(drive_info[93] & (1 << 5));
	}
	/* See if we need to put the device in CFA power mode 1 */
	if ((drive_info[160] & ((1 << 15) | (1 << 13)| (1 << 12))) ==
		((1 << 15) | (1 << 13)| (1 << 12))) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | info->slave;
		cmd.feature = IDE_FEATURE_CFA_ENABLE_POWER_MODE1;
		if (pio_non_data(base, &cmd) < 0) {
			/* If I need to power up the drive, and I can't
			 * give up.
			 */
			printf("Cannot power up CFA device\n");
			return 1;
		}
	}
	printf("disk%d %dk cap: %hx\n",
		info->basedrive,
		(unsigned long)(info->sectors >> 1),
		drive_info[49]);
	return 0;
}

static int init_controller(unsigned base, int basedrive, unsigned char *buffer) 
{
	struct harddisk_info *info;

	/* Intialize the harddisk_info structures */
	memset(harddisk_info, 0, sizeof(harddisk_info));
	
	/* Put the drives ide channel in a know state and wait
	 * for the drives to spinup.  
	 *
	 * In practice IDE disks to not respond to commands until
	 * they have spun up.  This makes IDE hard to deal with
	 * immediately after power up, as the delays can be quite
	 * long, so we must be very careful here.
	 *
	 * There are two pathological cases that must be dealt with:
	 *
	 * - The BSY bit not being set while the IDE drives spin up.
	 *   In this cases only a hard coded delay will work.  As
	 *   I have not reproduced it, and this is out of spec for
	 *   IDE drives the work around can be enabled by setting
	 *   BSY_SET_DURING_SPINUP to 0.
	 *
	 * - The BSY bit floats high when no drives are plugged in.
	 *   This case will not be deteced except by timeing out but
	 *   we avoid the problems by only probing devices we are
	 *   supposed to boot from.  If we don't do the probe we
	 *   will not experience the problem.
	 *
	 * So speed wise I am only slow if the BSY bit is not set
	 * or not reported by the IDE controller during spinup, which
	 * is quite rare.
	 * 
	 */
#if !BSY_SET_DURING_SPINUP
	if (await_ide(timeout, base, IDE_TIMEOUT) < 0) {
		return -1;
	}
#endif
	if (ide_software_reset(base) < 0) {
		return -1;
	}

	/* Note: I have just done a software reset.  It may be
	 * reasonable to just read the boot time signatures 
	 * off of the drives to see if they are present.
	 *
	 * For now I will go with just sending commands to the drives
	 * and assuming filtering out missing drives by detecting registers
	 * that won't set and, commands that fail to execute properly.
	 */

	/* Now initialize the individual drives */
	info = &harddisk_info[0];
	init_drive(info, base, 0, basedrive, buffer);
	if (info->drive_exists && !info->slave_absent) {
		basedrive++;
		info++;
		init_drive(info, base, 1, basedrive, buffer);
	}

	return 0;
}

static int disk_read_sectors(
	struct harddisk_info *info, 
	unsigned char *buffer,
	sector_t base_sector, unsigned int sectors)
{
	sector_t sector = 0;
	unsigned long offset;
	int result = 0;
	for(offset = 0; offset < sectors; offset++) {
		sector = base_sector + offset;
		if (sector >= info->sectors) {
			sector -= info->sectors;
		}
		result = ide_read_sector(
			info, buffer + (offset << 9), sector);
		if (result < 0)
			break;
	}
	if (result < 0) {
		printf("disk read error at 0x%x\n", sector);
	}
	return result;
}

static os_download_t probe_buffer(unsigned char *buffer, unsigned int len,
	int increment, unsigned int offset, unsigned int *roffset)
{
	os_download_t os_download;
	unsigned int end;
	end = 0;
	os_download = 0;
	if (increment > 0) {
		end = len - IDE_SECTOR_SIZE;
	}
	do {
		offset += increment;
		os_download = probe_image(buffer + offset, len - offset);
	} while(!os_download && (offset != end));
	*roffset = offset;
	return os_download;
}

static int load_image(
	struct harddisk_info *info, 
	unsigned char *buffer, unsigned int buf_sectors,
	sector_t block, unsigned int offset,
	os_download_t os_download)
{
	sector_t skip_sectors;

	skip_sectors = 0;
	while(1) {
		skip_sectors = os_download(buffer + offset, 
			(buf_sectors << 9) - offset, 0);
			block += skip_sectors + buf_sectors;
		if (block >= info->sectors) {
			block -= info->sectors;
		}

		offset = 0;
		buf_sectors = 1;
		if (disk_read_sectors(info, buffer, block, 1) < 0) {
			return 0;
		}
	}
}


int url_file(const char *name,
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int))
{
	static unsigned short ide_base[] = {
		IDE_BASE1, 
#if (NUM_HD >= 3)
		IDE_BASE2, 
#endif
#if (NUM_HD >= 5)
		IDE_BASE3, 
#endif
#if (NUM_HD >= 7)
		IDE_BASE4,
#endif
	};
	static unsigned char buffer[32*IDE_SECTOR_SIZE];
	struct harddisk_info *info;
	unsigned int drive;
	os_download_t os_download;
	unsigned int disk_offset, offset, len;
	volatile int increment, inc;
	int i;
	volatile sector_t block;
	sector_t buf_sectors;
	jmpbuf real_restart;

	disk_offset = 0;
	increment = 1;
	if (memcmp(name, "disk", 4) != 0) {
		printf("Not a disk device\n");
		return 0;
	}
	name = name + 4;
	drive = strtoul(name, &name, 10);
	if ((name[0] == '+') || (name[0] == '-')) {
		increment = (name[0] == '-')? -1 : 1;
		name++;
		disk_offset = strtoul(name, &name, 10);
	}
	if (name[0]) {
		printf("Junk '%s' at end of disk url\n", name);
		return 0;
	}
	if (drive/2 > sizeof(ide_base)/sizeof(ide_base[0])) {
		printf("Not that many drives\n");
		return 0;
	}
	if (init_controller(ide_base[drive/2], drive & ~1, buffer) < 0) {
		printf("Nothing behind the controller\n");
		return 0;
	}
	
	info = &harddisk_info[drive & 1];
	if (!info->drive_exists) {
		printf("unknown_drive\n");
		return 0;
	}
	
	/* Load a buffer, and see if it contains the start of an image
	 * we can boot from disk.
	 */
	len = sizeof(buffer);
	buf_sectors = sizeof(buffer) / IDE_SECTOR_SIZE;
	inc = increment;
	block = disk_offset >> 9;
	if (buf_sectors/2 > block) {
		block = (info->sectors - (buf_sectors/2)) + block;
	}
	/* let probe buffer assume offset always needs to be incremented */
	offset = (len/2 + (disk_offset & 0x1ff)) - inc;


	/* Catch longjmp so if this image fails to load, I start looking
	 * for the next image where I left off looking for this image.
	 */
	memcpy(&real_restart, &restart_etherboot, sizeof(jmpbuf));
	i = setjmp(restart_etherboot);
	if ((i != 0) && (i != -2)) {
		memcpy(&restart_etherboot, &real_restart, sizeof(jmpbuf));
		longjmp(restart_etherboot, i);
	}
	/* Read the canidate sectors into the buffer */
	if (disk_read_sectors(info, buffer, block, buf_sectors) < 0) {
		return -1;
	}
	if (inc == increment) {
		os_download = probe_buffer(buffer, len, inc, offset, &offset);
		if (os_download)
			goto load_image;
		inc = -inc;
	}
	os_download = probe_buffer(buffer, len, inc, offset, &offset);
	if (os_download)
		goto load_image;

	memcpy(&restart_etherboot, &real_restart, sizeof(jmpbuf));
	return 0;
 load_image:
	return load_image(info, buffer, buf_sectors, block, offset, os_download);

}

void disk_disable(void)
{
	if (harddisk_info[0].drive_exists) {
		return;
	}
	ide_software_reset(harddisk_info[0].controller_port);
}
#endif /* IDE_DISK */
