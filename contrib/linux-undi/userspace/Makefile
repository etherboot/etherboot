all: undi-user

install: all
	cp undi-user /home/ashieh/soc/root-qm/driver/

CFLAGS := -Wall -I/home/ashieh/soc/current/src/include -I/home/ashieh/soc/current/src/arch/i386/include -ggdb3
LDFLAGS := -static -T undi-user.lds --gc-sections

undi-user.o: undi-user.c undi.h

undi-user: undi-user.o
