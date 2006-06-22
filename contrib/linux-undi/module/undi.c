#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>

#include "undi.h"

MODULE_AUTHOR("Alan shieh (alancshieh@gmail.com)");
MODULE_DESCRIPTION("Etherboot UNDI driver, kernel-side functionality");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#define UNDI_MEM_NAME "undimem"

static int undi_mem_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg);
static int undi_mem_mmap(struct file *file, struct vm_area_struct *vma);

enum UNDI_Map_Mode {
	MAP_MODE_NONE,
	MAP_MODE_PXE_SEARCH,
	MAP_MODE_UNDI_EXEC,
};

static enum UNDI_Map_Mode map_mode = MAP_MODE_NONE;

static struct file_operations undi_mem_fops = {
	.owner =	THIS_MODULE,
	.mmap =		undi_mem_mmap,
	.ioctl =	undi_mem_ioctl,
};

static struct miscdevice undi_mem_miscdev = {
	UNDI_MEM_MINOR,
	UNDI_MEM_NAME,
	&undi_mem_fops
};

// Idea 2: Set up mmap() by overloading meaning of mmap on undi device node

// Idea 1: Set up map with sysctl
// This doesn't work well because sysctl is not naturally amenable to mmap()

#define CTL_UNDI (15437) // arbitrary and unused

enum undi_ctl {
	CTL_SETUP_MAP = 1,
};

static int proc_do_setup_map(ctl_table *table, int write, struct file *file,
	     void __user *buffer, size_t *lenp, loff_t *ppos);

static struct ctl_table_header *undi_table_header;
int dummy;
static ctl_table		undi_dir_table[] = {
	{
		.ctl_name	= CTL_SETUP_MAP,
		.procname	= "setup_map",
		.data		= &dummy,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_do_setup_map
	},
	{ .ctl_name = 0 }
};
static ctl_table		undi_table[] = {
	{
		.ctl_name	= CTL_UNDI,
		.procname	= "undi",
		.mode		= 0555,
		.child		= undi_dir_table
	},
	{ .ctl_name = 0 }
};

static int __init undi_init(void)
{
	// based on sunrpc initialization
	if(undi_table_header == NULL) {
		undi_table_header  = register_sysctl_table(undi_table, 1);
#ifdef CONFIG_PROC_FS
		if (undi_table[0].de)
			undi_table[0].de->owner = THIS_MODULE;
#endif
	}

	// based on mmtimer initialization
	strcpy(undi_mem_miscdev.devfs_name, UNDI_MEM_NAME);
	if (misc_register(&undi_mem_miscdev)) {
		printk(KERN_ERR "%s: failed to register device\n",
		       UNDI_MEM_NAME);
		return -1;
	}

	return 0;
}

static void __exit undi_cleanup(void)
{
	unregister_sysctl_table(undi_table_header);
	misc_deregister(&undi_mem_miscdev);
}

static int proc_do_setup_map(ctl_table *table, int write, struct file *file,
			     void __user *buffer, size_t *lenp, loff_t *ppos) {
	if(write) {
		printk("write of setup_map %d %d\n", *lenp, (int)*ppos);
		*ppos += *lenp;
		return 0;
	} else {
		printk("read of setup_map\n");
		*lenp = 0;
		return 0;
	}
}


union UNDI_Mem_Ctl {
	struct {
		// No arguments
	} pxe_search;
	struct UNDI_Exec_Ctl undi_exec;
};

static int undi_mem_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	printk("undi_mem_ioctl()\n");
	switch (cmd) {
	case UNDI_MEM_MAP_PXE_SEARCH:
		printk("map_mode_pxe_search!\n");
		map_mode  = MAP_MODE_PXE_SEARCH;
		break;
	case UNDI_MEM_MAP_UNDI_EXEC: {
		// copy_from_user(
		printk("ioctl(UNDI_MEM_MAP_UNDI_EXEC) not implemented yet!\n");
		return -ENOSYS;
		break;
	}
	default:
		printk("Unknown cmd %d!\n", cmd);
	}
	return 0;
}

static int undi_mem_mmap(struct file *file, struct vm_area_struct *vma) {
	printk("in undi_mem_mmap\n");
	// Based on mmtimer.c
	unsigned int addr;
	unsigned int size;
	switch(map_mode) {
	case MAP_MODE_PXE_SEARCH:
		addr = 0;
		size = PXE_SEARCH_MAP_LENGTH;
		break;
	case MAP_MODE_UNDI_EXEC:
		printk("undi_exec not implemented!\n");
		// break;
	default:
		printk("Invalid mmap mode\n");
		return -ENOSYS;
		break;
	}
	map_mode = MAP_MODE_NONE;
	vma->vm_flags |= (VM_IO | VM_SHM | VM_LOCKED );

	printk("About to remap\n");
	if (remap_pfn_range(vma, vma->vm_start, addr >> PAGE_SHIFT,
					size, vma->vm_page_prot)) {
		printk(KERN_ERR "remap_pfn_range failed in undi_mem\n");
		return -EAGAIN;
	}
	printk("done with remap\n");

	return 0;
}

module_init(undi_init);
module_exit(undi_cleanup);
