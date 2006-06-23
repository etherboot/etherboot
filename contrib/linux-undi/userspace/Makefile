all: undi-user

install: all
	cp undi-user /home/ashieh/soc/root-qm/driver/

CFLAGS := -Wall -I../../../src/include -I../../../src/arch/i386/include -ggdb3 -I../common
LDFLAGS := -static -T undi-user.lds --gc-sections

undi-user.o: undi-user.c ../common/undi.h

undi-user: undi-user.o
