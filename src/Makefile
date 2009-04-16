# Try hard to prevent people from using Etherboot.  When will this
# software be allowed to die?

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

# Pad to line 10 of the Makefile to make the messages prettier.
#
ifneq ($(ETHERBOOT),TOTALLY_UNSUPPORTED)
ifneq ($(WHAT_I_SHOULD_UPGRADE_TO),GPXE)
$(warning $(SPACE))
$(warning *******************************************************************)
$(warning *                                                                 *)
$(warning *                Please do not use this software!                 *)
$(warning *                                                                 *)
$(warning *******************************************************************)
$(warning $(SPACE))
$(warning Etherboot has been obsoleted by gPXE.  There is no further)
$(warning development work taking place on Etherboot.  No bugs will be fixed,)
$(warning and any patches will be rejected.  Please switch to using gPXE)
$(warning instead.  You can obtain gPXE from)
$(warning $(SPACE))
$(warning $(SPACE)   http://etherboot.org/wiki/download)
$(warning $(SPACE))
$(warning If you really want to build Etherboot, then you must acknowledge)
$(warning this message by typing)
$(warning $(SPACE))
$(warning $(SPACE)   make ETHERBOOT=TOTALLY_UNSUPPORTED \)
$(warning $(SPACE)        WHAT_I_SHOULD_UPGRADE_TO=GPXE \)
$(warning $(SPACE)        $(MAKECMDGOALS))
$(warning $(SPACE))
$(error Refusing to build obsolete software)
endif
endif

# Override ARCH here or on the command line
# ARCH=i386
# Additionally you can supply extra compilation arguments, e.g. for x86_64
# EXTRA_CFLAGS=-m32
# EXTRA_ASFLAGS=--32
# EXTRA_LDFLAGS=-m elf_i386
ifndef ARCH
ARCH:=$(shell uname -m | sed -e s,i[3456789]86,i386,)
endif
MAKEDEPS:=
SUFFIXES:=

include Config
include arch/$(ARCH)/Config
include Makefile.main
