ARCH:=$(shell uname -m | sed -e s,i[3456789]86,i386,)
MAKEDEPS:=
SUFFIXES:=

include Config
include arch/$(ARCH)/Config
include Makefile.main
