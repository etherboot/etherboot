VERSION=1.0
# spec file can override
RPMVERSION=$(VERSION)

# If you don't have either nasm or the ELKS as86, then just do make
# and make install because the assembler sources are supplied preassembled
# If you do make reallyclean you will have to unpack the Etherboot archive again

# Use nasm or as86
ASM=nasm
# This must be the as86 from the ELKS project, the default Linux one won't do
#ASM=as86

# for first32.linux
RELOCADDR=	0x92200
OLDGAS:=	$(shell $(AS) --version | grep -q '2\.9\.1' && echo -DGAS291)
CFLAGS32=	-O2 -fstrength-reduce -fomit-frame-pointer -m386 \
		-Wall -W -Wno-format -Wno-unused

# for FreeDOS
FDKSEG=	0x60

PREFIX=		/usr/local
INSTPREFIX=	$(BUILD_ROOT)$(PREFIX)
LIBDIR=		$(PREFIX)/lib/mknbi
INSTLIBDIR=	$(INSTPREFIX)/lib/mknbi
BINDIR=		$(INSTPREFIX)/bin
MANDIR=		$(INSTPREFIX)/man/man1
DOCDIR=		$(INSTPREFIX)/doc/mknbi-$(RPMVERSION)

INSTALL=	install

PROG=	mknbi disnbi dismbr disdosbb
MODULES=Nbi.pm TruncFD.pm
FIRSTS=	first.linux first.dos first.fdos
RMRD=	rmrd.com
MANS=	mknbi.1 disnbi.1
HTMLS=	mknbi.html disnbi.html
DOCS=	$(MANS) $(HTMLS)

all:	$(PROG) $(FIRSTS) $(RMRD) $(DOCS)

mknbi:	mknbi.pl Nbi.pm TruncFD.pm
	sed -e 's:@@LIBDIR@@:$(LIBDIR):' -e 's/@@FDKSEG@@/$(FDKSEG)/' mknbi.pl > $@
	chmod 755 $@

disnbi:	disnbi.pl
	cp -p disnbi.pl disnbi
	chmod 755 disnbi

dismbr:	dismbr.pl
	cp -p dismbr.pl dismbr
	chmod 755 dismbr

disdosbb:	disdosbb.pl
	cp -p disdosbb.pl disdosbb
	chmod 755 disdosbb

first.linux:	first-linux.S first-linux.h version-linux.h
ifeq ($(ASM),as86)
	gcc $(CFLAGS) -DUSE_AS86 -E -traditional -o first.s first-linux.S
	as86 -0 -b first.linux first.s
else
	gcc $(CFLAGS) -DUSE_NASM -E -traditional -o first.s first-linux.S
	nasm -f bin first.s -o first.linux
endif

first32.linux:	start32.o first32.o
	$(LD) -N -Ttext $(RELOCADDR) -e _start -oformat binary -o $@ start32.o first32.o

start32.o:	start32.S
	gcc -E -DRELOC=$(RELOCADDR) $(OLDGAS) start32.S | $(AS) -o start32.o

first32.o:	first32.c
	gcc $(CFLAGS32) -c $*.c

first.dos:	first-dos.S first-dos.h version-dos.h
ifeq ($(ASM),as86)
	gcc $(CFLAGS) -DUSE_AS86 -E -traditional -o first.s first-dos.S
	as86 -0 -b first.dos first.s
else
	gcc $(CFLAGS) -DUSE_NASM -E -traditional -o first.s first-dos.S
	nasm -f bin first.s -o first.dos
endif

first.fdos:	first-dos.S first-dos.h version-dos.h
ifeq ($(ASM),as86)
	gcc $(CFLAGS) -DFREEDOS -DFDKSEG=$(FDKSEG) -DUSE_AS86 -E -traditional -o first.s first-dos.S
	as86 -0 -b first.fdos first.s
else
	gcc $(CFLAGS) -DFREEDOS -DFDKSEG=$(FDKSEG) -DUSE_NASM -E -traditional -o first.s first-dos.S
	nasm -f bin first.s -o first.fdos
endif

rmrd.com:	rmrd.S
ifeq ($(ASM),as86)
	gcc $(CFLAGS) -DUSE_AS86 -E -traditional -o rmrd.s rmrd.S
	# -s appears to be necessary to start binary at 0x100
	as86 -0 -b rmrd.com -s rmrd.map rmrd.s
else
	gcc $(CFLAGS) -DUSE_NASM -E -traditional -o rmrd.s rmrd.S
	nasm -f bin rmrd.s -o rmrd.com
endif

$(MANS):	mknbi.pl disnbi.pl Makefile
	pod2man --date="2000-08-07" --release="Mknbi 1.0" --center="Etherboot tools" mknbi.pl > mknbi.1
	pod2man --date="2000-08-07" --release="Mknbi 1.0" --center="Etherboot tools" disnbi.pl > disnbi.1

$(HTMLS):	mknbi.pl disnbi.pl
	pod2html mknbi.pl > mknbi.html
	pod2html disnbi.pl > disnbi.html

install:	all
	-mkdir -p $(INSTLIBDIR)
	$(INSTALL) $(PROG) $(INSTLIBDIR)/
	$(INSTALL) -m 644 $(MODULES) $(FIRSTS) $(RMRD) $(INSTLIBDIR)/
	-mkdir -p $(BINDIR)
	cd $(BINDIR); \
	for i in linux rom fdos dos; \
	do \
		ln -sf ../lib/mknbi/mknbi mknbi-$$i; \
	done
	(cd $(BINDIR); ln -sf ../lib/mknbi/disnbi disnbi)
	(cd $(BINDIR); ln -sf ../lib/mknbi/dismbr dismbr)
	(cd $(BINDIR); ln -sf ../lib/mknbi/disdosbb disdosbb)
	-mkdir -p $(MANDIR)
	$(INSTALL) -m 644 mknbi.1 disnbi.1 $(MANDIR)/
	-mkdir -p $(DOCDIR)
	$(INSTALL) -m 644 README spec.txt $(DOCDIR)/

tarball:
	(cd ..; tar zcf /tmp/mknbi-$(VERSION).tar.gz --exclude CVS mknbi-$(VERSION))

clean:
	$(RM) -f $(PROG) start32.o first32.o first.s rmrd.s rmrd.map pod2htm*

reallyclean:	clean
	$(RM) -f $(FIRSTS) $(RMRD) $(DOCS)
