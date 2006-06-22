#ifndef _UNDI_H_
#define _UNDI_H_

#define UNDI_MEM_MAP_PXE_SEARCH (0x1)
#define UNDI_MEM_MAP_UNDI_EXEC (0x2)

#define PXE_SEARCH_MAP_BASE (0x00000000)
#define PXE_SEARCH_MAP_LENGTH (0x100000)

#define UNDI_MEM_MINOR (250)

struct UNDI_Exec_Ctl {
	void * mem_base;
	unsigned mem_length;
};

#endif // _UNDI_H_
