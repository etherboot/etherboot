ARCH_FORMAT=	armnommu

BUILD_ROMS=	$(ROMS)

START=	$(BIN)/start.o

SRCS+=	arch/armnommu/core/arm_timer.c
SRCS+=	arch/armnommu/core/start.S
SRCS+=	arch/armnommu/core/serial.c
SRCS+=	arch/armnommu/core/mem.c
SRCS+=	arch/armnommu/core/setjmp.S
SRCS+=	arch/armnommu/drivers/net/p2001_eth.c

# not greater than 100kB
ROMLIMIT:=1024000

include		$(BIN)/Roms

ROMS= $(BIN)/p2001_eth.rom
IMGS= $(BIN)/p2001_eth.img


allfiles:	$(BUILD_ROMS)

BOBJS+=		$(BIN)/arm_timer.o
BOBJS+=		$(BIN)/serial.o
BOBJS+=		$(BIN)/mem.o
BOBJS+=		$(BIN)/setjmp.o

# Utilities

$(BIN)/nrv2b:	util/nrv2b.c
	$(HOST_CC) -O2 -DENCODE -DDECODE -DMAIN -DVERBOSE -DNDEBUG -DBITSIZE=32 -DENDIAN=0 -o $@ $<

# Pattern Rules
# General for compiling/assembly source files
$(BIN)/%.o:	arch/armnommu/core/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN)/%.o:	arch/armnommu/drivers/net/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN)/%.S:	arch/armnommu/core/%.c $(MAKEDEPS)
	$(CC) $(CFLAGS) -S -o $@ -c $<

$(BIN)/%.o:	arch/armnommu/core/%.S $(MAKEDEPS)
	$(CPP) $(CFLAGS) -D ASSEMBLY $< | $(AS) $(ASFLAGS) -o $@

# general ruls for generating .img files
$(BIN)/%.tmp: $(BIN)/%.o $(START) $(BIN)/config.o arch/$(ARCH)/core/etherboot.lds $(LIBS) $(STDDEPS) $(MAKEDEPS)
	$(LD) $(LDFLAGS) -T arch/$(ARCH)/core/etherboot.lds -o $@ $(START) $(BIN)/config.o $< $(LIBS)
	@$(SIZE) $@ | $(CHECKSIZE)

$(BIN)/%.img: $(BIN)/%.tmp $(MAKEDEPS)
	$(OBJCOPY) -O binary $< $@