#include "etherboot.h"
#include "disk.h"

#undef disk_disable

static unsigned char disk_buffer[DISK_BUFFER_SIZE];
static struct disk disk;

static int disk_read(
	struct disk *disk, unsigned char *buffer, sector_t sector)
{
	int result;
	sector_t base_sector;

	/* Note: I do not handle disk wrap around here! */

	/* Compute the start of the track cache */
	base_sector = (sector / disk->sectors_per_read)*disk->sectors_per_read;

	/* See if I need to update the track cache */
	if ((sector < disk->sector) ||
		sector >= disk->sector + (disk->bytes >> 9)) {
		printf(".");
		result = disk->read(disk, base_sector);
		if (result < 0)
			return result;
	}
	/* Service the request from the track cache */
	memcpy(buffer, disk->buffer + ((sector - base_sector)<<9), SECTOR_SIZE);
	return 0;
}
	
static int disk_read_sectors(
	struct disk *disk,
	unsigned char *buffer,
	sector_t base_sector, unsigned int sectors)
{
	sector_t start_sector = 0;
	sector_t sector = 0;
	unsigned long offset;
	int result = 0;

	for(offset = 0; offset < sectors; offset++) {
		sector = base_sector + offset;
		if (sector >= disk->sectors) {
			sector -= disk->sectors;
		}
		result = disk_read(disk, buffer + (offset << 9), sector);
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
		end = len - SECTOR_SIZE;
	}
	do {
		offset += increment;
		os_download = probe_image(buffer + offset, len - offset);
	} while(!os_download && (offset != end));
	*roffset = offset;
	return os_download;
}

static int load_image(
	struct disk *disk, 
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
		if (block >= disk->sectors) {
			block -= disk->sectors;
		}

		offset = 0;
		buf_sectors = 1;
		if (disk_read_sectors(disk, buffer, block, 1) < 0) {
			return 0;
		}
	}
}

int url_file(const char *name,
	int (*fnc)(unsigned char *, unsigned int, unsigned int, int))
{
	/* 16K == 8K in either direction from the start of the disk */
	static unsigned char buffer[32*SECTOR_SIZE]; 
	unsigned int drive;
	int adapter;
	os_download_t os_download;
	unsigned int disk_offset, offset, len;
	volatile int increment, inc;
	int i;
	volatile sector_t block;
	sector_t buf_sectors;
	int type;
	jmpbuf real_restart;

	disk_offset = 0;
	increment = 1;
	if (memcmp(name, "disk", 4) == 0) {
		type = DISK_DRIVER;
		name += 4;
	}
	else if (memcmp(name, "floppy", 6) == 0) {
		type = FLOPPY_DRIVER;
		name += 6;
	}
	else {
		printf("Unknown device type\n");
		return 0;
	}
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
	memset(&disk, 0, sizeof(disk));
	disk.buffer = disk_buffer;
	disk.drive = 0 - 1;
	adapter = -1;
	do {
		disk_disable();
		disk.drive += 1;
		adapter = probe(adapter, &disk.dev, type);
		if (adapter < 0) {
			printf("Not that many drives\n");
			return 0;
		}
	} while(disk.drive < drive);
	
	/* Load a buffer, and see if it contains the start of an image
	 * we can boot from disk.
	 */
	len = sizeof(buffer);
	buf_sectors = sizeof(buffer) / SECTOR_SIZE;
	inc = increment;
	block = disk_offset >> 9;
	if (buf_sectors/2 > block) {
		block = (disk.sectors - (buf_sectors/2)) + block;
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
	if (disk_read_sectors(&disk, buffer, block, buf_sectors) < 0) {
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
	return load_image(&disk, buffer, buf_sectors, block, offset, os_download);

}

void disk_disable(void)
{
	disable(&disk.dev);
}
