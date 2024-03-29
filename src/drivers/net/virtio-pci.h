#ifndef _VIRTIO_PCI_H_
# define _VIRTIO_PCI_H_

#define offsetof(t,m) ((int )&(((t *)0)->m))

/* A 32-bit r/o bitmask of the features supported by the host */
#define VIRTIO_PCI_HOST_FEATURES        0

/* A 32-bit r/w bitmask of features activated by the guest */
#define VIRTIO_PCI_GUEST_FEATURES       4

/* A 32-bit r/w PFN for the currently selected queue */
#define VIRTIO_PCI_QUEUE_PFN            8

/* A 16-bit r/o queue size for the currently selected queue */
#define VIRTIO_PCI_QUEUE_NUM            12

/* A 16-bit r/w queue selector */
#define VIRTIO_PCI_QUEUE_SEL            14

/* A 16-bit r/w queue notifier */
#define VIRTIO_PCI_QUEUE_NOTIFY         16

/* An 8-bit device status register.  */
#define VIRTIO_PCI_STATUS               18

/* An 8-bit r/o interrupt status register.  Reading the value will return the
 * current contents of the ISR and will also clear it.  This is effectively
 * a read-and-acknowledge. */
#define VIRTIO_PCI_ISR                  19

/* The bit of the ISR which indicates a device configuration change. */
#define VIRTIO_PCI_ISR_CONFIG           0x2

/* The remaining space is defined by each driver as the per-driver
 * configuration space */
#define VIRTIO_PCI_CONFIG               20

/* Virtio ABI version, this must match exactly */
#define VIRTIO_PCI_ABI_VERSION          0

static inline u32 vp_get_features(struct nic *nic)
{
   return inl(nic->ioaddr + VIRTIO_PCI_HOST_FEATURES);
}

static inline void vp_set_features(struct nic *nic, u32 features)
{
        outl(features, nic->ioaddr + VIRTIO_PCI_GUEST_FEATURES);
}

static inline void vp_get(struct nic *nic, unsigned offset,
                     void *buf, unsigned len)
{
   u8 *ptr = buf;
   unsigned i;

   for (i = 0; i < len; i++)
           ptr[i] = inb(nic->ioaddr + VIRTIO_PCI_CONFIG + offset + i);
}

static inline u8 vp_get_status(struct nic *nic)
{
   return inb(nic->ioaddr + VIRTIO_PCI_STATUS);
}

static inline void vp_set_status(struct nic *nic, u8 status)
{
   if (status == 0)        /* reset */
           return;
        outb(status, nic->ioaddr + VIRTIO_PCI_STATUS);
}


static inline void vp_reset(struct nic *nic)
{
   outb(0, nic->ioaddr + VIRTIO_PCI_STATUS);
   (void)inb(nic->ioaddr + VIRTIO_PCI_ISR);
}

static inline void vp_notify(struct nic *nic, int queue_index)
{
   outw(queue_index, nic->ioaddr + VIRTIO_PCI_QUEUE_NOTIFY);
}

static inline void vp_del_vq(struct nic *nic, int queue_index)
{
   /* select the queue */

   outw(queue_index, nic->ioaddr + VIRTIO_PCI_QUEUE_SEL);

   /* deactivate the queue */

   outl(0, nic->ioaddr + VIRTIO_PCI_QUEUE_PFN);
}
#endif /* _VIRTIO_PCI_H_ */
