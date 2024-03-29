#ifndef _VIRTIO_RING_H_
# define _VIRTIO_RING_H_
#define PAGE_SHIFT (12)
#define PAGE_SIZE  (1<<PAGE_SHIFT)
#define PAGE_MASK  (PAGE_SIZE-1)

/* Status byte for guest to report progress, and synchronize features. */
/* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE     1
/* We have found a driver for the device. */
#define VIRTIO_CONFIG_S_DRIVER          2
/* Driver has used its parts of the config, and is happy */
#define VIRTIO_CONFIG_S_DRIVER_OK       4
/* We've given up on this device. */
#define VIRTIO_CONFIG_S_FAILED          0x80

#define MAX_QUEUE_NUM      (512)

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define VRING_AVAIL_F_NO_INTERRUPT 1

#define VRING_USED_F_NO_NOTIFY     1

struct vring_desc
{
   u64 addr;
   u32 len;
   u16 flags;
   u16 next;
};

struct vring_avail
{
   u16 flags;
   u16 idx;
   u16 ring[0];
};

struct vring_used_elem
{
   u32 id;
   u32 len;
};

struct vring_used
{
   u16 flags;
   u16 idx;
   struct vring_used_elem ring[];
};

struct vring {
   unsigned int num;
   struct vring_desc *desc;
   struct vring_avail *avail;
   struct vring_used *used;
};

static inline void vring_init(struct vring *vr,
                         unsigned int num, unsigned char *queue)
{
   unsigned int i;
   unsigned long pa;

        vr->num = num;

   /* physical address of desc must be page aligned */

   pa = virt_to_phys(queue);
   pa = (pa + PAGE_MASK) & ~PAGE_MASK;
   vr->desc = phys_to_virt(pa);

        vr->avail = (struct vring_avail *)&vr->desc[num];

   /* physical address of used must be page aligned */

   pa = virt_to_phys(&vr->avail->ring[num]);
   pa = (pa + PAGE_MASK) & ~PAGE_MASK;
        vr->used = phys_to_virt(pa);

   for (i = 0; i < num - 1; i++)
           vr->desc[i].next = i + 1;
   vr->desc[i].next = 0;
}

#define vring_size(num) \
   (((((sizeof(struct vring_desc) * num) + \
      (sizeof(struct vring_avail) + sizeof(u16) * num)) \
         + PAGE_MASK) & ~PAGE_MASK) + \
         (sizeof(struct vring_used) + sizeof(struct vring_used_elem) * num))
#endif /* _VIRTIO_RING_H_ */
