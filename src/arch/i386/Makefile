ARCH_FORMAT=	elf32-i386

# For debugging, don't delete intermediates
#.SECONDARY:

LCONFIG+=	-DRELOC=$(RELOCADDR) -Ui386

START=	$(BIN)/start32.o $(BIN)/linuxbios.o \
	$(BIN)/bios.o $(BIN)/console.o $(BIN)/memsizes.o $(BIN)/basemem.o \
	$(BIN)/hidemem.o $(BIN)/e820mangler.o \
	$(BIN)/realmode.o $(BIN)/realmode_asm.o \
	$(BIN)/callbacks.o $(BIN)/pxe_callbacks.o

SRCS+=	arch/i386/prefix/floppyprefix.S
SRCS+=	arch/i386/prefix/unhuf.S
SRCS+=	arch/i386/prefix/unnrv2b.S
SRCS+=	arch/i386/firmware/pcbios/bios.c
SRCS+=	arch/i386/firmware/pcbios/console.c
SRCS+=	arch/i386/firmware/pcbios/memsizes.c
SRCS+=	arch/i386/firmware/pcbios/basemem.c
SRCS+=	arch/i386/firmware/pcbios/hidemem.c
SRCS+=	arch/i386/firmware/pcbios/e820mangler.S
SRCS+=	arch/i386/prefix/liloprefix.S
SRCS+=	arch/i386/prefix/elfprefix.S
SRCS+=	arch/i386/prefix/lmelf_prefix.S
SRCS+=	arch/i386/prefix/comprefix.S
SRCS+=	arch/i386/prefix/pxeprefix.S
SRCS+=	arch/i386/prefix/romprefix.S

SRCS+=	arch/i386/core/init.S
SRCS+=	arch/i386/core/start32.S
SRCS+=	arch/i386/core/pxe.c
SRCS+=	arch/i386/core/pci_io.c
SRCS+=	arch/i386/core/i386_timer.c
SRCS+=	arch/i386/core/elf.c
SRCS+=	arch/i386/core/cpu.c
SRCS+=	arch/i386/core/video_subr.c
SRCS+=	arch/i386/core/pic8259.c
SRCS+=	arch/i386/core/hooks.c
SRCS+=	arch/i386/core/callbacks.c
SRCS+=	arch/i386/core/realmode.c
SRCS+=	arch/i386/core/realmode_asm.S
SRCS+=	arch/i386/core/pxe_callbacks.c

# ROM loaders: ISA and PCI versions
ISAPREFIX=	$(BIN)/isaprefix.o
ISAENTRY=	$(BIN)/isaprefix.entry.o
ISAEXIT=	$(BIN)/isaprefix.exit.o
PCIPREFIX=	$(BIN)/pciprefix.o
PCIENTRY=	$(BIN)/pciprefix.entry.o
PCIEXIT=	$(BIN)/pciprefix.exit.o
# Variables xxx_ROMTYPE are defined by genrules.pl.  ROMENTRY and
# ROMEXIT will evaluate to give the correct objects to use.
TARGETBASE=$(patsubst $(BIN)/%,%,$(firstword $(subst ., ,$@)))
ROMCARD=$(firstword $(subst --, ,$(TARGETBASE)))
ROMTYPE=$(firstword $(ROMTYPE_$(ROMCARD)) ISA)
ROMENTRY=$($(ROMTYPE)ENTRY)
ROMEXIT=$($(ROMTYPE)EXIT)

# Other real-mode entry loaders
DSKPREFIX=	$(BIN)/floppyprefix.o
DSKENTRY=	$(BIN)/floppyprefix.entry.o
DSKEXIT=	$(BIN)/floppyprefix.exit.o
COMPREFIX=	$(BIN)/comprefix.o
COMENTRY=	$(BIN)/comprefix.entry.o
COMEXIT=	$(BIN)/comprefix.exit.o
LILOPREFIX=	$(BIN)/liloprefix.o
LILOENTRY=	$(BIN)/liloprefix.entry.o
LILOEXIT=	$(BIN)/liloprefix.exit.o
PXEPREFIX=	$(BIN)/pxeprefix.o
PXEENTRY=	$(BIN)/pxeprefix.entry.o
PXEEXIT=	$(BIN)/pxeprefix.exit.o
NULLPREFIX=	$(BIN)/nullprefix.o
NULLENTRY=	$(BIN)/nullprefix.entry.o
NULLEXIT=	$(BIN)/nullprefix.exit.o

# ELF loader
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

# Evaluate ROMLIMIT only once - it is constant during the make run.
# Under normal situations, 2K of stack are rarely needed.  If you
# experience strange behaviour in functions that use many local
# variables or that call functions that do, check for stack overrun! 
# Make sure that the normal case needs no Perl interpreter - if someone uses a
# different RELOCADDR, then he has Perl installed anyways (the shell cannot
# deal with hex numbers, as test/eval don't support non-decimal integers).
ROMLIMIT:=$(shell perl -e 'printf("%d\n", (640*1024 - $(RELOCADDR)));')

include		$(BIN)/Roms

all:		$(ROMS)
allroms:	$(ROMS)
allzroms:	$(ROMS)
alldsks:	$(EB_DSKS)
allzdsks:	$(EB_DSKS)
alllilos:	$(EB_LILOS)
allzlilos:	$(EB_LILOS)
allpxes:	$(EB_PXES)
allzpxes:	$(EB_PXES)
allelfs:	$(EB_ELFS)
allzelfs:	$(EB_ELFS)
allcoms:	$(EB_COMS)
allisos:	$(EB_ISOS)
alllisos:	$(EB_LISOS)

BOBJS+=		$(BIN)/pxe.o $(BIN)/pci_io.o $(BIN)/i386_timer.o
BOBJS+=		$(BIN)/elf.o $(BIN)/cpu.o $(BIN)/video_subr.o
BOBJS+=		$(BIN)/pic8259.o $(BIN)/hooks.o

# ROM loaders

$(ISAPREFIX):	arch/i386/prefix/romprefix.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) $(LCONFIG) -Ui386 -D ASSEMBLY $< \
		| $(AS) $(ASFLAGS) -o $@ 

$(PCIPREFIX):	arch/i386/prefix/romprefix.S $(MAKEDEPS)
	$(CPP) -DPCI_PNP_HEADER $(CFLAGS) $(LCONFIG) -Ui386 -D ASSEMBLY $< \
		| $(AS) $(ASFLAGS) -o $@ 

# Prefix splitters
$(BIN)/%prefix.entry.o: $(BIN)/%prefix.o $(MAKEDEPS)
	$(OBJCOPY) -R .text16 $< $@

$(BIN)/%prefix.exit.o: $(BIN)/%prefix.o $(MAKEDEPS)
	$(OBJCOPY) -R .prefix $< $@

# Generic prefix objects
PREFIXOBJS = $(BIN)/init.o
ZPREFIXOBJS = $(BIN)/init.o $(BIN)/unnrv2b.o

# Utilities
$(BIN)/nrv2b:	util/nrv2b.c
	$(HOST_CC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -DNDEBUG -DBITSIZE=32 -DENDIAN=0 -o $@ $<

ZFILELEN = perl util/zfilelen.pl

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


# general rule for .z (compressed binary code), may be overridden
# rule for .z is in top level Makefile
# Give the directory name, e.g. use $(BIN)/rtl8139.com as the target.

$(BIN)/%.zo:	$(BIN)/%.zbin arch/i386/core/prefixdata.lds $(MAKEDEPS)
	$(LD) -T arch/i386/core/prefixdata.lds -b binary $< -o $@

# general rules for normal/compressed ROM images, may be overridden
SUFFIXES +=	rom zrom

$(BIN)/%.rom.rt: $(BIN)/%.rt.o $(ISAEXIT) $(PCIEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -Ttext $(RELOCADDR) -o $@ \
		$< $(ROMEXIT)

$(BIN)/%.rom.prf: $(BIN)/%.rt.o $(ISAENTRY) $(PCIENTRY) $(ISAEXIT) $(PCIEXIT) $(PREFIXOBJS) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ \
		$(ROMENTRY) $(PREFIXOBJS) $< $(ROMEXIT)

$(BIN)/%.zrom.prf: $(BIN)/%.rt.o $(ISAENTRY) $(PCIENTRY) $(ISAEXIT) $(PCIEXIT) $(ZPREFIXOBJS) $(LDSCRIPT) $(MAKEDEPS) $(BIN)/%.rom.rt2.zbin
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) --defsym "_zfile_size=`$(ZFILELEN) $(BIN)/$*.rom.rt2.zbin`" -o $@ \
		$(ROMENTRY) $(ZPREFIXOBJS) $< $(ROMEXIT)

$(BIN)/%.rom: $(BIN)/%.rom.prf.bin $(BIN)/%.rom.rt.bin
	cat $^ > $@
	$(MAKEROM) $(MAKEROM_FLAGS) $(MAKEROM_$(ROMCARD)) $(MAKEROM_ID_$(ROMCARD)) -i$(IDENT) $@

$(BIN)/%.zrom: $(BIN)/%.zrom.prf.bin $(BIN)/%.rom.rt1.bin $(BIN)/%.rom.rt2.zbin
	cat $^ > $@
	$(MAKEROM) $(MAKEROM_FLAGS) $(MAKEROM_$(ROMCARD)) $(MAKEROM_ID_$(ROMCARD)) -i$(IDENT) $@

# general rules for ELF images
SUFFIXES +=	elf zelf
$(BIN)/elfprefix.tmp:	$(BIN)/elfprefix.o arch/i386/prefix/elfprefix.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/elfprefix.lds $< -o $@

$(BIN)/elfprefix:	$(BIN)/elfprefix.tmp $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.elf:	$(BIN)/%.raw $(BIN)/%.raw.prf $(BIN)/elfprefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$*.raw.prf | (read l1; read d1 d2 bss rest; echo $$bss)
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/elfprefix $< `$(SIZE) $(BIN)/$*.raw.prf | (read l1; read d1 d2 bss rest; echo $$bss)` > $@

$(BIN)/%.zelf:	$(BIN)/%.zraw $(BIN)/%.raw.prf $(BIN)/elfprefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$*.raw.prf | (read l1; read d1 d2 d3 size rest; echo $$size)
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/elfprefix $< `$(SIZE) $(BIN)/$*.raw.prf | (read l1; read d1 d2 d3 size rest; echo $$size)` > $@

# general rules for Long Mode ELF images
SUFFIXES +=	lmelf zlmelf
$(BIN)/lmelf_prefix.tmp:	$(BIN)/lmelf_prefix.o arch/i386/prefix/elfprefix.lds $(MAKEDEPS)
	$(LD) -T arch/i386/prefix/elfprefix.lds $< -o $@

$(BIN)/lmelf_prefix:	$(BIN)/lmelf_prefix.tmp $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.lmelf:	$(BIN)/%.raw $(BIN)/%.raw.prf $(BIN)/lmelf_prefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$*.raw.ptf | (read l1; read d1 d2 bss rest; echo $$bss)
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/lmelf_prefix $< `$(SIZE) $(BIN)/$*.raw.prf | (read l1; read d1 d2 bss rest; echo $$bss)` > $@

$(BIN)/%.zlmelf:	$(BIN)/%.zraw $(BIN)/%.raw.prf $(BIN)/lmelf_prefix arch/i386/prefix/apply_elf_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$*.raw.prf | (read l1; read d1 d2 d3 size rest; echo $$size)
	$(PERL) arch/i386/prefix/apply_elf_prefix.pl $(RELOCADDR) $(BIN)/lmelf_prefix $< `$(SIZE) $(BIN)/$(*).raw.prf | (read l1; read d1 d2 d3 size rest; echo $$size)` > $@

# rules to generate a DOS loadable .com executable
SUFFIXES +=	com
$(BIN)/%.com.rt: $(BIN)/%.rt.o $(COMEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -Ttext $(RELOCADDR) -o $@ $< $(COMEXIT)

$(BIN)/%.com.prf: $(BIN)/%.rt.o $(COMENTRY) $(COMEXIT) $(PREFIXOBJS) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(COMENTRY) $(PREFIXOBJS) $< $(COMEXIT)

$(BIN)/%.com: $(BIN)/%.com.prf.bin $(BIN)/%.com.rt.bin
	cat $^ > $@

# rules to make a LILO loadable image
SUFFIXES +=	lilo zlilo

$(BIN)/%.lilo.rt: $(BIN)/%.rt.o $(LILOEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -Ttext $(RELOCADDR) -o $@ $< $(LILOEXIT)

$(BIN)/%.lilo.prf: $(BIN)/%.rt.o $(LILOENTRY) $(LILOEXIT) $(PREFIXOBJS) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(LILOENTRY) $(PREFIXOBJS) $< $(LILOEXIT)

$(BIN)/%.zlilo.prf: $(BIN)/%.rt.o $(LILOENTRY) $(LILOEXIT) $(ZPREFIXOBJS) $(BIN)/%.lilo.rt2.zo $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) --defsym "_zfile_size=`$(ZFILELEN) $(BIN)/$*.lilo.rt2.zbin`" -o $@ \
		$(LILOENTRY) $(ZPREFIXOBJS) $< $(LILOEXIT)

$(BIN)/%.lilo: $(BIN)/%.lilo.prf.bin $(BIN)/%.lilo.rt.bin
	$(PERL) util/makelilo.pl $^ > $@

$(BIN)/%.zlilo: $(BIN)/%.zlilo.prf.bin $(BIN)/%.lilo.rt1.bin $(BIN)/%.lilo.rt2.zbin
	$(PERL) util/makelilo.pl $^ > $@

# rules to generate a PXE loadable image
SUFFIXES +=	pxe zpxe

$(BIN)/%.pxe.rt: $(BIN)/%.rt.o $(PXEEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -Ttext $(RELOCADDR) -o $@ $< $(PXEEXIT)

$(BIN)/%.pxe.prf: $(BIN)/%.rt.o $(PXEENTRY) $(PXEEXIT) $(PREFIXOBJS) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(PXEENTRY) $(PREFIXOBJS) $< $(PXEEXIT)

$(BIN)/%.zpxe.prf: $(BIN)/%.rt.o $(PXEENTRY) $(PXEEXIT) $(ZPREFIXOBJS) $(BIN)/%.pxe.rt2.zo $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) --defsym "_zfile_size=`$(ZFILELEN) $(BIN)/$*.pxe.rt2.zbin`" -o $@ \
		$(PXEENTRY) $(ZPREFIXOBJS) $< $(PXEEXIT)

$(BIN)/%.pxe: $(BIN)/%.pxe.prf.bin $(BIN)/%.pxe.rt.bin
	cat $^ > $@

$(BIN)/%.zpxe: $(BIN)/%.zpxe.prf.bin $(BIN)/%.pxe.rt1.bin $(BIN)/%.pxe.rt2.zbin
	cat $^ > $@

# rules to generate the .dsk/.zdsk floppy images
SUFFIXES +=	dsk zdsk

$(BIN)/%.dsk.rt: $(BIN)/%.rt.o $(DSKEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -Ttext $(RELOCADDR) -o $@ $< $(DSKEXIT)

$(BIN)/%.dsk.prf: $(BIN)/%.rt.o $(DSKENTRY) $(DSKEXIT) $(PREFIXOBJS) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(DSKENTRY) $(PREFIXOBJS) $< $(DSKEXIT)

$(BIN)/%.zdsk.prf: $(BIN)/%.rt.o $(DSKENTRY) $(DSKEXIT) $(ZPREFIXOBJS) $(BIN)/%.dsk.rt2.zo $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) --defsym "_zfile_size=`$(ZFILELEN) $(BIN)/$*.dsk.rt2.zbin`" -o $@ \
		$(DSKENTRY) $(ZPREFIXOBJS) $< $(DSKEXIT)

$(BIN)/%.dsk: $(BIN)/%.dsk.prf.bin $(BIN)/%.dsk.rt.bin
	cat $^ > $@

$(BIN)/%.zdsk: $(BIN)/%.zdsk.prf.bin $(BIN)/%.dsk.rt1.bin $(BIN)/%.dsk.rt2.zbin
	cat $^ > $@

# rules to write the .dsk/.zdsk image onto a blank floppy
SUFFIXES +=	fd0 zfd0
%.fd0:	%.dsk
	dd if=$< bs=512 conv=sync of=/dev/fd0

%.zfd0:	%.zdsk
	dd if=$< bs=512 conv=sync of=/dev/fd0

# rules to create raw executable images
SUFFIXES +=	raw zraw
$(BIN)/%.raw.rt: $(BIN)/%.rt.o $(NULLEXIT) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -Ttext $(RELOCADDR) -o $@ $< $(NULLEXIT)

$(BIN)/%.raw.prf: $(BIN)/%.rt.o $(NULLENTRY) $(NULLEXIT) $(PREFIXOBJS) $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $(NULLENTRY) $(PREFIXOBJS) $< $(NULLEXIT)

$(BIN)/%.zraw.prf: $(BIN)/%.rt.o $(NULLENTRY) $(NULLEXIT) $(ZPREFIXOBJS) $(BIN)/%.raw.rt2.zo $(LDSCRIPT) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) --defsym "_zfile_size=`$(ZFILELEN) $(BIN)/$*.raw.rt2.zbin`" -o $@ \
		$(NULLENTRY) $(ZPREFIXOBJS) $< $(NULLEXIT)

$(BIN)/%.raw: $(BIN)/%.raw.prf.bin $(BIN)/%.raw.rt.bin
	cat $^ > $@

$(BIN)/%.zraw: $(BIN)/%.zraw.prf.bin $(BIN)/%.raw.rt1.bin $(BIN)/%.raw.rt2.zbin
	cat $^ > $@

# rule to make a non-emulation ISO boot image
SUFFIXES +=	iso
%.iso:	util/geniso %.zlilo
	bash util/geniso $*.iso $*.zlilo

# rule to make a floppy emulation ISO boot image
SUFFIXES +=	liso
%.liso:	util/genliso %.zlilo
	bash util/genliso $*.liso $*.zlilo
