ARCH_FORMAT=	elf64-ia64-little

LCONFIG+=	-DRELOC=$(RELOCADDR) -DS16SIZE=$(START16_BYTES)


BUILD_EFIS=	$(patsubst %.img, %.efi, $(IMGS)) $(patsubst %.img, %.nrv2befi, $(IMGS))

START=	$(BIN)/start.o $(BIN)/efi_main.o $(BIN)/reloc.o 

SRCS+=	arch/ia64/prefix/efi_prefix.S arch/ia64/prefix/unnrv2b.S
SRCS+=	arch/ia64/core/__call.S 
SRCS+=	arch/ia64/core/ia64_timer.c
SRCS+=	arch/ia64/core/idiv32.S
SRCS+=	arch/ia64/core/idev64.S
SRCS+=	arch/ia64/core/longjmp.S
SRCS+=	arch/ia64/core/memmove.S
SRCS+=	arch/ia64/core/memset.S
SRCS+=	arch/ia64/core/pal.c
SRCS+=	arch/ia64/core/pci_io.c
SRCS+=	arch/ia64/core/reloc.S
SRCS+=	arch/ia64/core/relocate_to.S
SRCS+=	arch/ia64/core/sal.c
SRCS+=	arch/ia64/core/setjmp.S
SRCS+=	arch/ia64/core/start.S

ROMLIMIT:=3276800

include		$(BIN)/Roms

# We need allefis because $(IMGS) is not defined until
# the Makefile fragment "Roms" is read.
allefis:	$(BUILD_EFIS)


BOBJS+=		$(BIN)/acpi.o 
BOBJS+=		$(BIN)/sal.o $(BIN)/pal.o
BOBJS+=		$(BIN)/memset.o $(BIN)/memmove.o
BOBJS+=		$(BIN)/setjmp.o $(BIN)/longjmp.o
BOBJS+=		$(BIN)/relocate_to.o $(BIN)/__call.o
BOBJS+=		$(BIN)/pci_io.o $(BIN)/ia64_timer.o
BOBJS+=		$(BIN)/__divdi3.o $(BIN)/__udivdi3.o $(BIN)/__moddi3.o $(BIN)/__umoddi3.o
BOBJS+=		$(BIN)/__divsi3.o $(BIN)/__udivsi3.o $(BIN)/__modsi3.o $(BIN)/__umodsi3.o

$(BIN)/pci_io.o:	arch/ia64/core/pci_io.c include/pci.h include/etherboot.h include/osdep.h
$(BIN)/ia64_timer.o:	arch/ia64/core/ia64_timer.c include/timer.h include/etherboot.h include/osdep.h
$(BIN)/memmove.o:	arch/ia64/core/memmove.S
$(BIN)/memset.o:	arch/ia64/core/memset.S
$(BIN)/setjmp.o:	arch/ia64/core/setjmp.S
$(BIN)/longjmp.o:	arch/ia64/core/longjmp.S
$(BIN)/relocate_to.o:	arch/ia64/core/relocate_to.S
$(BIN)/__call.o:	arch/ia64/core/__call.S
$(BIN)/start.o:		arch/ia64/core/start.S
$(BIN)/sal.o:		arch/ia64/core/sal.c
$(BIN)/pal.o:		arch/ia64/core/pal.c

$(BIN)/efi_prefix.o:	arch/ia64/prefix/efi_prefix.S
$(BIN)/unnrv2b.o:	arch/ia64/prefix/unnrv2b.S


# IA64 Division routines
$(BIN)/__divdi3.o:	arch/ia64/core/idiv64.S
	$(CPP) $(CFLAGS) -DASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/__udivdi3.o:	arch/ia64/core/idiv64.S
	$(CPP) $(CFLAGS) -DASSEMBLY -DUNSIGNED $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/__moddi3.o:	arch/ia64/core/idiv64.S
	$(CPP) $(CFLAGS) -DASSEMBLY -DMODULO $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/__umoddi3.o:	arch/ia64/core/idiv64.S
	$(CPP) $(CFLAGS) -DASSEMBLY -DUNSIGNED -DMODULO $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/__divsi3.o:	arch/ia64/core/idiv32.S
	$(CPP) $(CFLAGS) -DASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/__udivsi3.o:	arch/ia64/core/idiv32.S
	$(CPP) $(CFLAGS) -DASSEMBLY -DUNSIGNED $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/__modsi3.o:	arch/ia64/core/idiv32.S
	$(CPP) $(CFLAGS) -DASSEMBLY -DMODULO $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/__umodsi3.o:	arch/ia64/core/idiv32.S
	$(CPP) $(CFLAGS) -DASSEMBLY -DUNSIGNED -DMODULO $< | $(AS) $(ASFLAGS) -o $@



# Utilities
$(BIN)/nrv2b:	util/nrv2b.c
	$(GCC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -DNDEBUG -DBITSIZE=64 -DENDIAN=0 -o $@ $<

# Pattern Rules

# General for compiling assembly source files
$(BIN)/%.o:	arch/ia64/prefix/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -DASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/%.o:	arch/ia64/core/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -DASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

$(BIN)/%.o:	$(BIN)/%.s
	$(AS)	$(ASFLAGS) -o $@ $<

# General rules for bootable images

# Rules for nrv2b compressed images
$(BIN)/unnrv2b.tmp:	$(BIN)/unnrv2b.o arch/ia64/prefix/unnrv2b.lds $(MAKEDEPS)
	$(LD) -T arch/ia64/prefix/unnrv2b.lds $< -o $@

$(BIN)/unnrv2b: $(BIN)/unnrv2b.tmp $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.nrv2bimg: $(BIN)/%.nrv2b $(BIN)/unnrv2b arch/ia64/prefix/apply_unnrv2b_prefix.pl $(MAKEDEPS)
	$(PERL) arch/ia64/prefix/apply_unnrv2b_prefix.pl $(BIN)/unnrv2b $< > $@


# rules to generate efi loadable image
$(BIN)/efi_prefix.tmp:	$(BIN)/efi_prefix.o arch/ia64/prefix/efi_prefix.lds $(MAKEDEPS)
	$(LD) -T arch/ia64/prefix/efi_prefix.lds $< -o $@

$(BIN)/efi_prefix: $(BIN)/efi_prefix.tmp  $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@

$(BIN)/%.efi:	$(BIN)/%.img $(BIN)/%.tmp $(BIN)/efi_prefix arch/ia64/prefix/apply_efi_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 bss rest ; echo $$bss )
	$(PERL)	arch/ia64/prefix/apply_efi_prefix.pl $(BIN)/efi_prefix $< `$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 bss rest ; echo $$bss )` > $@

$(BIN)/%.nrv2befi: $(BIN)/%.nrv2bimg $(BIN)/%.tmp $(BIN)/efi_prefix arch/ia64/prefix/apply_efi_prefix.pl $(MAKEDEPS)
	@$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 d3 size rest ; echo $$size )
	$(PERL)	arch/ia64/prefix/apply_efi_prefix.pl $(BIN)/efi_prefix $< `$(SIZE) $(BIN)/$(*).tmp | (read l1; read d1 d2 d3 size rest ; echo $$size )` > $@

