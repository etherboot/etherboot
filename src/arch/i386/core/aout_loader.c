/* a.out */
struct exec {
	unsigned long      a_midmag;	/* flags<<26 | mid<<16 | magic */
	unsigned long      a_text;	/* text segment size */
	unsigned long      a_data;	/* initialized data size */
	unsigned long      a_bss;	/* uninitialized data size */
	unsigned long      a_syms;	/* symbol table size */
	unsigned long      a_entry;	/* entry point */
	unsigned long      a_trsize;	/* text relocation size */
	unsigned long      a_drsize;	/* data relocation size */
};

struct aout_state {
	struct exec head;
	unsigned long curaddr;
	int segment;			/* current segment number, -1 for none */
	unsigned long loc;		/* start offset of current block */
	unsigned long skip;		/* padding to be skipped to current segment */
	unsigned long toread;		/* remaining data to be read in the segment */
};

static struct aout_state astate;

static sector_t aout_download(unsigned char *data, unsigned int len, int eof);
static inline os_download_t aout_probe(unsigned char *data, unsigned int len)
{
	unsigned long start, mid, end, istart, iend;
	if (len < sizeof(astate.head)) {
		return 0;
	}
	memcpy(astate.head, data, sizeof(astate.head));
	if ((astate.head.a_midmag & 0xffff) != 0x010BL) {
		return 0;
	}
	
	printf("(a.out");
	aout_freebsd_probe();
	printf(")... ");
	/* Check the aout image */
	start  = astate.head.a_entry;
	mid    = (((start + astate.head.a_text) + 4095) & ~4095) + astate.head.a_data;
	end    = ((mid + 4095) & ~4095) + astate.head.a_bss;
	istart = 4096;
	iend   = istart + (mid - start);
	if (!prep_segment(start, mid, end, istart, iend))
		return 0;
	segment = -1;
	loc = 0;
	skip = 0;
	toread = 0;
	return aout_download;
}

static sector_t aout_download(unsigned char *data, unsigned int len, int eof)
{
	unsigned int offset;	/* working offset in the current data block */

	offset = 0;

#ifdef AOUT_LYNX_KDI
	segment++;
	if (segment == 0) {
		curaddr = 0x100000;
		astate.head.a_entry = curaddr + 0x20;
	}
	memcpy(phys_to_virt(curaddr), data, len);
	curaddr += len;
	return 0;
#endif

	do {
		if (segment != -1) {
			if (skip) {
				if (skip >= len - offset) {
					skip -= len - offset;
					break;
				}
				offset += skip;
				skip = 0;
			}

			if (toread) {
				if (toread >= len - offset) {
					memcpy(phys_to_virt(curaddr), data+offset,
						len - offset);
					curaddr += len - offset;
					toread -= len - offset;
					break;
				}
				memcpy(phys_to_virt(curaddr), data+offset, toread);
				offset += toread;
				toread = 0;
			}
		}

		/* Data left, but current segment finished - look for the next
		 * segment.  This is quite simple for a.out files.  */
		segment++;
		switch (segment) {
		case 0:
			/* read text */
			curaddr = astate.head.a_entry;
			skip = 4096;
			toread = astate.head.a_text;
			break;
		case 1:
			/* read data */
			/* skip and curaddr may be wrong, but I couldn't find
			 * examples where this failed.  There is no reasonable
			 * documentation for a.out available.  */
			skip = ((curaddr + 4095) & ~4095) - curaddr;
			curaddr = (curaddr + 4095) & ~4095;
			toread = astate.head.a_data;
			break;
		case 2:
			/* initialize bss and start kernel */
			curaddr = (curaddr + 4095) & ~4095;
			skip = 0;
			toread = 0;
			memset(phys_to_virt(curaddr), '\0', astate.head.a_bss);
			goto aout_startkernel;
		default:
			break;
		}
	} while (offset < len);

	loc += len;

	if (eof) {
		int	j;
		unsigned long entry;

aout_startkernel:
		entry = astate.head.a_entry;
		done();

		aout_freebsd_boot();
#ifdef AOUT_LYNX_KDI
		xstart32(entry);
#endif
		printf("unexpected a.out variant\n");
		longjmp(restart_etherboot, -2);
	}
	return 0;
}
