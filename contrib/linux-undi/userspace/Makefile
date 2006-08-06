all: undi-user

install: all
	cp undi-user /home/ashieh/soc/root-qm/driver/

CFLAGS := -Wall -idirafter ../../../src/include -I../../../src/arch/i386/include -ggdb3 -I../common -pthread
LDFLAGS := -static -T undi-user.lds --gc-sections -pthread

undi-user.o: undi-user.c ../common/undi.h

undi-user: undi-user.o
