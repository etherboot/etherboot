ARCH_FORMAT=	elf32-i386

LCONFIG+=	-DRELOC=$(RELOCADDR) -DS16SIZE=$(START16_BYTES) -Ui386

BUILD_ROMS=	$(ROMS)
BUILD_ELFS=	$(patsubst %img, %elf, $(IMGS))
BUILD_PXES=	$(patsubst %.img, %.pxe, $(IMGS)) $(patsubst %.img, %.zpxe, $(IMGS))
BUILD_COMS=	$(patsubst %.img, %.com, $(IMGS))
BUILD_LILOS=	$(patsubst %.img, %.lilo, $(IMGS)) $(patsubst %.img, %.zlilo, $(IMGS))

START=	$(BIN)/start32.o $(BIN)/pcbios.o $(BIN)/memsizes.o $(BIN)/linuxbios.o

SRCS+=	arch/i386/prefix/boot1a.s arch/i386/prefix/comprefix.S
SRCS+=	arch/i386/prefix/liloprefix.S arch/i386/prefix/loader.S
SRCS+=	arch/i386/core/start32.S

# ROM loaders: NRV2B version (prefix NRV2B)
RLOADER=	$(BIN)/rloader.bin
PRLOADER=	$(BIN)/prloader.bin
START16=	$(BIN)/start16.bin
DISKLOADER=	$(BIN)/boot1a.bin
COMPREFIX=	$(BIN)/comprefix.bin
LILOPREFIX=	$(BIN)/liloprefix.bin
PXEPREFIX=	$(BIN)/pxeprefix.bin
ELFPREFIX=	$(BIN)/elfprefix.o

# Make sure that the relocation address is acceptable for all ROM sizes.  
# The old setting was 0x9400 which left about 45kB of space for Etherboot.
# Now we default to 0x7e00 which is where most disk loaders will load us.
# Without relocation enabled this means etherboot cannot load images that
# resize in the low 640KB.  With relocation enabled we Etherboot should be
# able to load any image.  The size is limited to 320KB to catch really flagrant
# abuses, and to ensure there is enough room for etherboot to decompress itself.
# This check may need to be updated to ensure the single driver etherboot will
# fit in a bootrom.  The check is done based running 'size' on the binary, not
# ROM size.  With previous incarnations this meant a ROM of 16kB or a partially
# used ROM of 32kB, remembering to take compressed ROM images into account.
#
ifndef RELOCADDR
#RELOCADDR=0x7e00
# The floppy code uses the rom loader which does an extra memcpy.
# If we place ourselves at 0x7e00 we will get stomped, so run
# slightly higher until that can be fixed.
RELOCADDR:=0x20000
endif

# Until all of the drivers are updated to be relocateable, 
# Carefully setup loader.S to loader
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
ROMLIMIT:=327680

# We need allroms,allelfs,allpxes,allcoms,alllilos because $(IMGS) is not defined until
# the Makefile fragment "Roms" is read.

include		$(BIN)/Roms

allroms:	$(BUILD_ROMS)
allelfs:	$(BUILD_ELFS)
allpxes:	$(BUILD_PXES)
allcoms:	$(BUILD_COMS)
alllilos:	$(BUILD_LILOS)

SRCS+=		arch/i386/core/pxe.c arch/i386/core/pci_io.c
BOBJS+=		$(BIN)/pxe.o $(BIN)/pci_io.o $(BIN)/i386_timer.o
BOBJS+=		$(BIN)/elf.o


$(BIN)/pxe.o:		arch/i386/core/pxe.c include/etherboot.h include/osdep.h include/nic.h
$(BIN)/pci_io.o:	arch/i386/core/pci_io.c include/pci.h include/etherboot.h include/osdep.h
$(BIN)/i386_timer.o:	arch/i386/core/i386_timer.c include/timer.h include/etherboot.h include/osdep.h
$(BIN)/elf.o:		arch/i386/core/elf.c include/etherboot.h include/osdep.h \
			include/elf.h arch/i386/include/bits/elf.h

# Assembly files
$(BIN)/start32.o:	arch/i386/core/start32.S
$(BIN)/start16.o:	arch/i386/core/start16.S
$(BIN)/unhuf.o:		arch/i386/prefix/unhuf.S
$(BIN)/unnrv2b.o:	arch/i386/prefix/unnrv2b.S
$(BIN)/memsizes.o:	arch/i386/firmware/pcbios/memsizes.c
$(BIN)/pcbios.o:	arch/i386/firmware/pcbios/pcbios.S
$(BIN)/liloprefix.o:	arch/i386/prefix/liloprefix.S
$(BIN)/elfprefix.o:	arch/i386/prefix/elfprefix.S
$(BIN)/floppyload.o:	arch/i386/prefix/floppyload.S
$(BIN)/comprefix.o:	arch/i386/prefix/comprefix.S
$(BIN)/pxeprefix.o:	arch/i386/prefix/pxeprefix.S
$(BIN)/loader.o:	arch/i386/prefix/loader.S


# Prepended loaders
$(BIN)/rloader.bin:	$(BIN)/rloader.o
$(BIN)/prloader.bin:	$(BIN)/prloader.o
$(BIN)/start16.bin:	$(BIN)/rloader.o
$(BIN)/floppyload.bin:	$(BIN)/floppyload.o
$(BIN)/comprefix.bin:   $(BIN)/comprefix.o
$(BIN)/liloprefix.bin:	$(BIN)/liloprefix.o
$(BIN)/pxeprefix.bin:	$(BIN)/pxeprefix.o

$(BIN)/rloader.o:	arch/i386/prefix/loader.S $(MAKEDEPS)
	$(CPP) $(LCONFIG) $< | $(AS) $(ASFLAGS) -o $@ 

$(BIN)/prloader.o:	arch/i386/prefix/loader.S $(MAKEDEPS)
	$(CPP) -DPCI_PNP_HEADER $(LCONFIG) $< | $(AS) $(ASFLAGS) -o $@ 

# 16 to 32bit transition
$(BIN)/start16.o:	arch/i386/core/start16.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) $(LCONFIG) $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/boot1a.bin: arch/i386/prefix/boot1a.s $(MAKEDEPS)
	as --defsym FLAGS=0x80 arch/i386/prefix/boot1a.s -o $(BIN)/boot1a.o
	ld -nostdlib -static -N -e start -Ttext 0x7c00 -o $(BIN)/boot1a.out $(BIN)/boot1a.o
	objcopy -S -O binary $(BIN)/boot1a.out $(BIN)/boot1a.bin
	$(RM) $(BIN)/boot1a.o $(BIN)/boot1a.out

# PXE loader
$(BIN)/pxeprefix.s:	arch/i386/prefix/pxeprefix.S $(MAKEDEPS)
	$(CPP) -o $@ $<

$(BIN)/pxeprefix.bin:	$(BIN)/pxeprefix.o
	$(LD) -Ttext 0x7c00 $(LDBINARY) -o $@ $<

# COM loader

$(BIN)/comprefix.s:	arch/i386/prefix/comprefix.S $(MAKEDEPS)
	$(CPP) -o $@ $<

# LILO prefix:

$(BIN)/liloprefix.s:	arch/i386/prefix/liloprefix.S $(MAKEDEPS)
	$(CPP) -o $@ $<

$(BIN)/liloprefix.bin:	$(BIN)/liloprefix.o
	$(LD) -Ttext 0x7c00 $(LDBINARY) -o $@ $<

# Utilities

$(BIN)/nrv2b:	util/nrv2b.c
	$(GCC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -DNDEBUG -DBITSIZE=32 -DENDIAN=0 -o $@ $<


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

$(BIN)/%.zo:	$(BIN)/%.z arch/i386/prefix/huf.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/huf.lds -b binary $< -o $@

$(BIN)/%.zimg:	$(BIN)/%.zo $(BIN)/unnrv2b.o arch/i386/prefix/unhuf.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/unhuf.lds $(BIN)/unnrv2b.o $< -o $@


# general rules for normal/compressed ROM images, may be overridden
$(BIN)/%.rom:	$(BIN)/%.img $(RLOADER) $(START16)
	cat $(RLOADER) $(START16) $< > $@
	$(MAKEROM) $(MAKEROM_FLAGS) $(MAKEROM_$*) -i$(IDENT) $@

$(BIN)/%.zrom: $(BIN)/%.zimg $(RLOADER) $(START16)
	cat $(RLOADER) $(START16) $< > $@
	$(MAKEROM) $(MAKEROM_FLAGS)  $(MAKEROM_$*) -i$(IDENT) $@

# general rules for ELF images
$(BIN)/%.imgo:	$(BIN)/%.img $(MAKEDEPS)
	$(LD) -T arch/$(ARCH)/prefix/img.lds -b binary $< -o $@

$(BIN)/%.lzimgo:	$(BIN)/%.lzimg $(MAKEDEPS)
	$(LD) -T arch/$(ARCH)/prefix/img.lds -b binary $< -o $@

$(BIN)/%.zimgo:	$(BIN)/%.zimg $(MAKEDEPS)
	$(LD) -T arch/$(ARCH)/prefix/img.lds -b binary $< -o $@

$(BIN)/%.elf:	$(BIN)/%.imgo $(ELFPREFIX) $(MAKEDEPS)
	$(LD) -T arch/$(ARCH)/prefix/elfprefix.lds $(ELFPREFIX) $< -o $@

$(BIN)/%.lzelf:	$(BIN)/%.lzimgo $(ELFPREFIX) $(MAKEDEPS)
	$(LD) -T arch/$(ARCH)/prefix/elfprefix.lds $(ELFPREFIX) $< -o $@

$(BIN)/%.zelf:	$(BIN)/%.zimgo $(ELFPREFIX) $(MAKEDEPS)
	$(LD) -T arch/$(ARCH)/prefix/elfprefix.lds $(ELFPREFIX) $< -o $@

# rules to write the .rom/.zrom image onto a blank floppy
# Give the directory name, e.g. $(BIN)/rtl8139.zfd0 as the target.
%.fd0:	%.rom $(DISKLOADER)
	cat $(DISKLOADER) $< > /dev/fd0

%.zfd0:	%.zrom $(DISKLOADER)
	cat $(DISKLOADER) $< > /dev/fd0

# rules to generate a .com executable
# Give the directory name, e.g. use $(BIN)/rtl8139.com as the target.
%.com:	%.zimg $(COMPREFIX)
	cat $(COMPREFIX) $< > $@

# rules to generate a pxe loadable image
%.pxe:	%.img $(PXEPREFIX)
	cat $(PXEPREFIX) $< > $@

%.zpxe:	%.zimg $(PXEPREFIX)
	cat $(PXEPREFIX) $< > $@

# rules to make a disk image (padding to fill an even number of cylinders).
# VMware reports disk image read errors if it cannot read ahead 36 sectors,
# probably because the floppyload.S code reads up to that number of sectors in
# a single request.  Not that 18k matters much these days...
# Probably not required now that boot1a.s is in use, but shouldn't hurt.
# You must give the directory name, e.g. use $(BIN)/rtl8139.dsk as the target.
%.dsk:	%.rom $(DISKLOADER) 
	cat $(DISKLOADER) $< /dev/zero | head -c 36k > $@

%.zdsk:	%.zrom $(DISKLOADER)
	cat $(DISKLOADER) $< /dev/zero | head -c 36k > $@

# rules to make a LILO-bootable image
%.lilo:		%.img $(LILOPREFIX)
	cat $(LILOPREFIX) $< /dev/zero | head -c 64k > $@

%.zlilo:	%.zimg $(LILOPREFIX)
	cat $(LILOPREFIX) $< /dev/zero | head -c 64k > $@
