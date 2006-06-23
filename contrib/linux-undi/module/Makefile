CONFIG_QUICKUSB := m

KVER := $(shell uname -r)

KSRCDIR = /lib/modules/$(KVER)/build

EXTRA_CFLAGS += -Wall -I$(PWD) -I$(PWD)/../common/

obj-$(CONFIG_QUICKUSB) += undi.o

all :
	$(MAKE) -C $(KSRCDIR) M=$(PWD) modules

clean :
	rm -f *.o *.s *.ko *.mod.c
	$(MAKE) -C $(KSRCDIR) M=$(PWD) clean
