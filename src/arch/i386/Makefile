ARCH_FORMAT=	elf32-i386

LCONFIG+=	-DRELOC=$(RELOCADDR) -DS16SIZE=$(START16_BYTES) -Ui386

BUILD_ROMS=	$(ROMS)
BUILD_ELFS=	$(patsubst %img, %elf, $(IMGS))
BUILD_PXES=	$(patsubst %.img, %.pxe, $(IMGS)) $(patsubst %.img, %.zpxe, $(IMGS))
BUILD_COMS=	$(patsubst %.img, %.com, $(IMGS))
BUILD_LILOS=	$(patsubst %.img, %.lilo, $(IMGS)) $(patsubst %.img, %.zlilo, $(IMGS))

START=	$(BIN)/start32.o $(BIN)/pcbios.o $(BIN)/memsizes.o \
	$(BIN)/basemem.o $(BIN)/linuxbios.o

SRCS+=	arch/i386/prefix/floppyprefix.S
SRCS+=	arch/i386/prefix/unhuf.S
SRCS+=	arch/i386/prefix/unnrv2b.S
SRCS+=	arch/i386/firmware/pcbios/memsizes.c
SRCS+=	arch/i386/firmware/pcbios/basemem.c
SRCS+=	arch/i386/firmware/pcbios/pcbios.S
SRCS+=	arch/i386/prefix/liloprefix.S
SRCS+=	arch/i386/prefix/elfprefix.S
SRCS+=	arch/i386/prefix/lmelf_prefix.S
SRCS+=	arch/i386/prefix/comprefix.S
SRCS+=	arch/i386/prefix/pxeprefix.S
SRCS+=	arch/i386/prefix/loader.S

SRCS+=	arch/i386/core/start32.S
SRCS+=	arch/i386/core/start16.S
SRCS+=	arch/i386/core/pxe.c
SRCS+=	arch/i386/core/pci_io.c
SRCS+=	arch/i386/core/i386_timer.c
SRCS+=	arch/i386/core/elf.c
SRCS+=	arch/i386/core/cpu.c
SRCS+=	arch/i386/core/video_subr.c
SRCS+=	arch/i386/core/pic8259.c

# ROM loaders: PCI header version (prefix P)
RLOADER=	$(BIN)/rloader.bin
PRLOADER=	$(BIN)/prloader.bin

START16=	$(BIN)/start16.bin
DISKLOADER=	$(BIN)/floppyprefix.bin
COMPREFIX=	$(BIN)/comprefix.bin
LILOPREFIX=	$(BIN)/liloprefix.bin
PXEPREFIX=	$(BIN)/pxeprefix.bin
ELFPREFIX=	$(BIN)/elfprefix.o

# Make sure that the relocation address is acceptable for all ROM sizes.  
# The old setting was 0x9400 which left about 45kB of space for Etherboot.
# Now we default to 0x20000. Without relocation enabled this means
# Etherboot cannot load images that resize in the low 640KB.
# (We should see if we can lower this to allow larger images in the low
# 640kB but this is not urgent.)
# With relocation enabled we Etherboot should be able to load any image.
# The size is limited to 320KB to catch really flagrant
# abuses, and to ensure there is enough room for etherboot to decompress itself.
# This check may need to be updated to ensure the single driver etherboot will
# fit in a bootrom.  The check is done based running 'size' on the binary, not
# ROM size.  With previous incarnations this meant a ROM of 16kB or a partially
# used ROM of 32kB, remembering to take compressed ROM images into account.
#
ifndef RELOCADDR
RELOCADDR:=0x20000
endif

# Until all of the drivers are updated to be relocatable,
# carefully setup loader.S to know start16.bin size
#
ifndef START16_BYTES
START16_BYTES:=112
endif

# Evaluate ROMLIMIT only once - it is constant during the make run.
# Under normal situations, 2K of stack are rarely needed.  If you
# experience strange behaviour in functions that use many local
# variables or that call functions that do, check for stack overrun! 
# Make sure that the normal case needs no Perl interpreter - if someone uses a
# different RELOCADDR, then he has Perl installed anyways (the shell cannot
# deal with hex numbers, as test/eval don't support non-decimal integers).
ROMLIMIT:=$(shell perl -e 'printf("%d\n", (640*1024 - $(RELOCADDR)));')

# We need allroms,allelfs,allpxes,allcoms,alllilos because $(IMGS) is not defined until
# the Makefile fragment "Roms" is read.

include		$(BIN)/Roms

allroms:	$(BUILD_ROMS)
allelfs:	$(BUILD_ELFS)
allpxes:	$(BUILD_PXES)
allcoms:	$(BUILD_COMS)
alllilos:	$(BUILD_LILOS)

BOBJS+=		$(BIN)/pxe.o $(BIN)/pci_io.o $(BIN)/i386_timer.o
BOBJS+=		$(BIN)/elf.o $(BIN)/cpu.o $(BIN)/video_subr.o
BOBJS+=		$(BIN)/pic8259.o

# Prepended loaders
$(BIN)/rloader.bin:	$(BIN)/rloader.o
$(BIN)/prloader.bin:	$(BIN)/prloader.o
$(BIN)/floppyprefix.bin: $(BIN)/floppyprefix.o
$(BIN)/comprefix.bin:   $(BIN)/comprefix.o
$(BIN)/liloprefix.bin:	$(BIN)/liloprefix.o
$(BIN)/pxeprefix.bin:	$(BIN)/pxeprefix.o

# ROM loaders

$(BIN)/rloader.o:	arch/i386/prefix/loader.S $(MAKEDEPS)
	$(CPP) $(LCONFIG) $< | $(AS) $(ASFLAGS) -o $@ 

$(BIN)/prloader.o:	arch/i386/prefix/loader.S $(MAKEDEPS)
	$(CPP) -DPCI_PNP_HEADER $(LCONFIG) $< | $(AS) $(ASFLAGS) -o $@ 

# 16 to 32bit transition
$(BIN)/start16.o:	arch/i386/core/start16.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) $(LCONFIG) $< | $(AS) $(ASFLAGS) -o $@

# floppy / hard disk loader (not used)

$(BIN)/boot1a.bin: arch/i386/prefix/boot1a.s $(MAKEDEPS)
	as --defsym FLAGS=0x80 arch/i386/prefix/boot1a.s -o $(BIN)/boot1a.o
	ld -nostdlib -static -N -e start -Ttext 0x7c00 -o $(BIN)/boot1a.out $(BIN)/boot1a.o
	objcopy -S -O binary $(BIN)/boot1a.out $(BIN)/boot1a.bin
	$(RM) $(BIN)/boot1a.o $(BIN)/boot1a.out

# floppy loader

$(BIN)/floppyprefix.o: arch/i386/prefix/floppyprefix.S $(MAKEDEPS)
	$(CPP) $< | $(AS) $(ASFLAGS) -o $(BIN)/floppyprefix.o

$(BIN)/floppyprefix.bin: $(BIN)/floppyprefix.o
	objcopy -O binary $< $(BIN)/floppyprefix.bin
	$(RM) $(BIN)/floppyprefix.o

# PXE prefix

$(BIN)/pxeprefix.s:	arch/i386/prefix/pxeprefix.S $(MAKEDEPS)
	$(CPP) -o $@ $<

$(BIN)/pxeprefix.bin:	$(BIN)/pxeprefix.o
	$(LD) -Ttext 0x7c00 $(LDBINARY) -o $@ $<

# COM prefix

$(BIN)/comprefix.s:	arch/i386/prefix/comprefix.S $(MAKEDEPS)
	$(CPP) -o $@ $<

# LILO prefix:

$(BIN)/liloprefix.s:	arch/i386/prefix/liloprefix.S $(MAKEDEPS)
	$(CPP) -o $@ $<

$(BIN)/liloprefix.bin:	$(BIN)/liloprefix.o
	$(LD) -Ttext 0x7c00 $(LDBINARY) -o $@ $<

# Utilities

$(BIN)/nrv2b:	util/nrv2b.c
	$(HOST_CC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -DNDEBUG -DBITSIZE=32 -DENDIAN=0 -o $@ $<

# Pattern Rules

# General for compiling/assembly source files

$(BIN)/%.o:	arch/i386/core/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN)/%.o:	arch/i386/core/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -Ui386 -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/%.o:	arch/i386/firmware/pcbios/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN)/%.o:	arch/i386/firmware/pcbios/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -Ui386 -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/%.o:	arch/i386/prefix/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -Ui386 -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

# general rule for 16bit .o, may be overridden
$(BIN)/%.o:	$(BIN)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# Rules for linking start16 to the compressed or uncompressed images.  These are done
# using the linker rather than cat, because we now want to be able to use symbols from
# the %.sym files and symbols derived from the size of the image with a linker script.
$(BIN)/%.bin:	$(BIN)/%.img $(BIN)/%.sym $(BIN)/start16.o arch/i386/prefix/start16.lds
	$(LD) --accept-unknown-input-arch -T arch/i386/prefix/start16.lds \
		--oformat binary -o $@ $(BIN)/start16.o $(BIN)/$*.sym -b binary $<

$(BIN)/%.zbin:	$(BIN)/%.zimg $(BIN)/%.zsym $(BIN)/start16.o arch/i386/prefix/start16z.lds
	$(LD) --accept-unknown-input-arch -T arch/i386/prefix/start16z.lds \
		--oformat binary -o $@ $(BIN)/start16.o $(BIN)/$*.zsym -b binary $<

# general rule for .bin (plain binary loader code), may be overridden
$(BIN)/%.bin:	$(BIN)/%.o
	$(LD) -Ttext 0x10000 $(LDBINARY) -o $@ $<

# general rule for .huf (compressed binary code), may be overridden
$(BIN)/%.huf:	$(BIN)/%.img $(BIN)/lzhuf $(MAKEDEPS)
	$(BIN)/lzhuf e $< $@

$(BIN)/%.hufo: $(BIN)/%.huf arch/i386/prefix/huf.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/huf.lds -b binary $< -o $@

$(BIN)/%.lzimg: $(BIN)/%.hufo $(BIN)/unhuf.o arch/i386/prefix/unhuf.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/unhuf.lds $(BIN)/unhuf.o $< -o $@

# general rule for .z (compressed binary code), may be overridden
# rule for .z is in top level Makefile
# Give the directory name, e.g. use $(BIN)/rtl8139.com as the target.

$(BIN)/%.zo:	$(BIN)/%.z arch/i386/prefix/huf.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/huf.lds -b binary $< -o $@

$(BIN)/%.ztmp:	$(BIN)/%.zo $(BIN)/unnrv2b.o arch/i386/prefix/unhuf.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/unhuf.lds $(BIN)/unnrv2b.o $< -o $@

$(BIN)/%.zimg: $(BIN)/%.ztmp $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@

# Generate .zsym files containing symbols from decompressor alongside symbols from .sym file
$(BIN)/%.ztmp.allsym: $(BIN)/%.ztmp $(MAKEDEPS)
	$(OBJCOPY) -x -O symbolsrec --prefix-symbols=decompressor_ $< $@

$(BIN)/%.zsym: $(BIN)/%.sym $(BIN)/%.ztmp.sym $(MAKEDEPS)
	$(LD) --accept-unknown-input-arch --oformat symbolsrec -r -o $@ $< $(BIN)/$*.ztmp.sym

# general rules for normal/compressed ROM images, may be overridden
$(BIN)/%.rom:	$(BIN)/%.img $(RLOADER) $(START16)
	cat $(RLOADER) $(START16) $< > $@
	$(MAKEROM) $(MAKEROM_FLAGS) $(MAKEROM_$*) -i$(IDENT) $@

$(BIN)/%.zrom: $(BIN)/%.zimg $(RLOADER) $(START16)
	cat $(RLOADER) $(START16) $< > $@
	$(MAKEROM) $(MAKEROM_FLAGS)  $(MAKEROM_$*) -i$(IDENT) $@

# general rules for ELF images
$(BIN)/elfprefix.tmp:	$(BIN)/elfprefix.o arch/i386/prefix/elfprefix.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/elfprefix.lds $< -o $@

$(BIN)/elfprefix:	$(BIN)/elfprefix.tmp $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.elf:	$(BIN)/%.img $(BIN)/%.tmp $(BIN)/elfprefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 bss rest ; echo $$bss )
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/elfprefix $< `$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 bss rest ; echo $$bss)` > $@

$(BIN)/%.zelf:	$(BIN)/%.zimg $(BIN)/%.tmp $(BIN)/elfprefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 d3 size rest ; echo $$size )
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/elfprefix $< `$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 d3 size rest ; echo $$size)` > $@

# general rules for Long Mode ELF images
$(BIN)/lmelf_prefix.tmp:	$(BIN)/lmelf_prefix.o arch/i386/prefix/elfprefix.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/elfprefix.lds $< -o $@

$(BIN)/lmelf_prefix:	$(BIN)/lmelf_prefix.tmp $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.lmelf:	$(BIN)/%.img $(BIN)/%.tmp $(BIN)/lmelf_prefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 bss rest ; echo $$bss )
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/lmelf_prefix $< `$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 bss rest ; echo $$bss)` > $@

$(BIN)/%.zlmelf:	$(BIN)/%.zimg $(BIN)/%.tmp $(BIN)/lmelf_prefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 d3 size rest ; echo $$size )
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/lmelf_prefix $< `$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 d3 size rest ; echo $$size)` > $@

# rules to generate a DOS loadable .com executable
%.com:	%.zimg $(COMPREFIX) $(START16)
	cat $(COMPREFIX) $(START16) $< > $@

# rules to make a LILO loadable image
%.lilo:		%.img $(LILOPREFIX) $(START16)
	$(PERL) util/makelilo.pl $(LILOPREFIX) $(START16) $< > $@

%.zlilo:	%.zimg $(LILOPREFIX) $(START16)
	$(PERL) util/makelilo.pl $(LILOPREFIX) $(START16) $< > $@

# rules to generate a PXE loadable image
%.pxe:	%.img $(PXEPREFIX) $(START16)
	cat $(PXEPREFIX) $(START16) $< > $@

%.zpxe:	%.zimg $(PXEPREFIX) $(START16)
	cat $(PXEPREFIX) $(START16) $< > $@

# rules to generate the .dsk/.zdsk floppy images
%.dsk:	%.img $(DISKLOADER) $(START16)
	cat $(DISKLOADER) $(START16) $< > $@

%.zdsk:	%.zimg $(DISKLOADER) $(START16)
	cat $(DISKLOADER) $(START16) $< > $@

# rules to write the .img/.zimg image onto a blank floppy
%.fd0:	%.img $(DISKLOADER) $(START16)
	cat $(DISKLOADER) $(START16) $< > /dev/fd0

%.zfd0:	%.zimg $(DISKLOADER) $(START16)
	cat $(DISKLOADER) $(START16) $< > /dev/fd0

# rule to make a non-emulation ISO boot image
%.iso:	%.zimg $(START16)
	mkdir -p $*.dir
	cat $(START16) $< > $*.dir/$(*F).bin
	mkisofs -o $@ -c boot.cat -no-emul-boot -b $(*F).bin $*.dir
	rm -fr $*.dir
