#!/usr/bin/perl -w

use strict;

$#ARGV >= 0 or die "Usage: $0 configfile\n";
open(STDIN, $ARGV[0]) or die "$ARGV[0]: $!\n";

# Read the config file, storing info in associative arrays
my(%drivers_deps) = ();
my(%drivers_pci) = ();
my(%drivers_isa) = ();
my(%roms_pci) = ();
my(%roms_isa) = ();
my(%ids_pci) = ();

my(%driver_dep, @new_dep, @collect_dep);

my($source, $rom, $drv, $ids, $key, $key2, $macro, $deps);

while(<>) {
	chomp($_);
	next if (/^\s*$/);
	next if (/^\s*#/);
	($rom,$drv,$ids) = split;
	# Driver name defaults to ROM name
	$drv = $rom if (!defined($drv) or $drv eq '');
	if (!-e "$drv.c") {
		print STDERR "Driver file $drv.c not available, skipping...\n";
		next;
	}

	# Automatically generate the dependencies for the driver sources.
	%driver_dep = ();
	# The "$drv.c" is never inserted into %driver_dep, because it is
	# treated specially below.
	@new_dep = ("$drv.c");
	while ($#new_dep >= 0) {
		@collect_dep = ();
		foreach $source (@new_dep) {
			open(INFILE, "$source");
			while (<INFILE>) {
				chomp($_);
# This code is not very smart: no C comments or CPP conditionals processing is
# done.  This may cause unexpected (or incorrect) additional dependencies.
# However, ignoring the CPP conditionals is in some sense correct: we need to
# figure out a superset of all the headers for the driver source.  The pci.h
# file is treated specially, because we know which cards are PCI and not ISA.
				next unless (s/^\s*#include\s*"([^"]*)".*$/$1/);
				next if (exists $driver_dep{"$_"});
				next if ("$_" eq "pci.h");
				$driver_dep{"$_"} = "$_";
				push(@collect_dep,($_));
			}
			close(INFILE);
		}
		@new_dep = @collect_dep;
	}
	%{$drivers_deps{"$drv"}} = %driver_dep;

	if (defined($ids)) {
		@{$drivers_pci{"$drv"}}=() if (!defined($drivers_pci{"$drv"}));
		push(@{$drivers_pci{"$drv"}},($rom));
		$roms_pci{"$rom"} = $drv;
		$ids_pci{"$rom"} = $ids;
	} else {
		@{$drivers_isa{"$drv"}}=() if (!defined($drivers_isa{"$drv"}));
		push(@{$drivers_isa{"$drv"}},($rom));
		$roms_isa{"$rom"} = $drv;
	}
}

# and generate the assignments to DOBJS and BINS
print "# Driver object files and ROM image files\n";
print "DOBJS32\t+= bin32/pci.o\n";
foreach $key (sort keys %drivers_pci) {
	# PCI drivers are compiled only once for all ROMs
	print "DOBJS32\t+= bin32/$key.o\n";
}
foreach $key (sort keys %drivers_isa) {
	foreach $key2 (@{$drivers_isa{$key}}) {
		# ISA drivers are compiled for all used ROMs
		print "DOBJS32\t+= bin32/$key2.o\n";
	}
}
foreach $key (sort keys %drivers_isa) {
	foreach $key2 (@{$drivers_isa{$key}}) {
		# ISA drivers are compiled for all used ROMs
		print "DOBJS16\t+= bin16/$key2.o\n";
	}
}
foreach $key (sort keys %roms_pci) {
	print "BINS32\t+= bin32/$key.rom bin32/$key.lzrom\n";
}
foreach $key (sort keys %roms_isa) {
	print "BINS32\t+= bin32/$key.rom bin32/$key.lzrom\n";
}
foreach $key (sort keys %roms_isa) {
	print "BINS16\t+= bin16/$key.rom bin16/$key.lzrom\n";
}

# and the *.o and config-*.o rules
print "\n# Rules to build the driver (or ROM for ISA/mixed drivers) object files\n";
foreach $key (sort keys %drivers_pci) {
	($macro = $key) =~ tr/\-/_/;
	$deps = join(' ', (sort keys %{$drivers_deps{$key}}));
	# PCI drivers are compiled only once for all ROMs
	print <<EOF;
bin32/$key.o:	$key.c \$(MAKEDEPS) pci.h $deps
	\$(CC32) \$(CFLAGS32) \$(\U$macro\EFLAGS) -o \$@ -c \$<

bin32/config-$key.o:	config.c \$(MAKEDEPS) osdep.h etherboot.h nic.h cards.h
	\$(CC32) \$(CFLAGS32) -DINCLUDE_\U$macro\E -o \$@ -c \$<

EOF
}
foreach $key (sort keys %drivers_isa) {
	$deps = join(' ', (sort keys %{$drivers_deps{$key}}));
	foreach $key2 (@{$drivers_isa{$key}}) {
		# ISA drivers are compiled for all used ROMs
		($macro = $key2) =~ tr/\-/_/;
		print <<EOF;
bin32/$key2.o:	$key.c \$(MAKEDEPS) $deps
	\$(CC32) \$(CFLAGS32) \$(\U$macro\EFLAGS) -o \$@ -c \$<

bin32/config-$key2.o:	config.c \$(MAKEDEPS) osdep.h etherboot.h nic.h cards.h
	\$(CC32) \$(CFLAGS32) -DINCLUDE_\U$macro\E -o \$@ -c \$<

EOF
	}
}
foreach $key (sort keys %drivers_isa) {
	$deps = join(' ', (sort keys %{$drivers_deps{$key}}));
	foreach $key2 (@{$drivers_isa{$key}}) {
		# ISA drivers are compiled for all used ROMs
		($macro = $key2) =~ tr/\-/_/;
		print <<EOF;
bin16/$key2.o:	$key.c \$(MAKEDEPS) $deps
	\$(CC16) \$(CFLAGS16) \$(\U$macro\EFLAGS) -o \$@ -c \$<

bin16/config-$key2.o:	config.c \$(MAKEDEPS) osdep.h etherboot.h nic.h cards.h
	\$(CC16) \$(CFLAGS16) -DINCLUDE_\U$macro\E -o \$@ -c \$<

EOF
	}
}

# and generate the Rom rules
print "\n# Rules to build the ROM files\n";
foreach $rom (sort keys %roms_pci) {
	$drv = $roms_pci{"$rom"};
	$ids = $ids_pci{"$rom"};
	# PCI ROMs are prepared for each type to get the IDs right
	print <<EOF;
bin32/$rom.rom:	bin32/$drv.img \$(PRLOADER)
	cat \$(PRLOADER) \$< > \$@
	bin/makerom \$(MAKEROM_\$*) -p $ids -i\$(IDENT32) \$@

bin32/$rom.lzrom:	bin32/$drv.huf \$(PRZLOADER)
	cat \$(PRZLOADER) \$< > \$@
	bin/makerom \$(MAKEROM_\$*) -p $ids -i\$(IDENT32) \$@

EOF
}
foreach $rom (sort keys %roms_isa) {
	# ISA ROMs are prepared from the matching code images
	print <<EOF;
bin32/$rom.rom:	bin32/$rom.img \$(RLOADER)

bin32/$rom.lzrom:	bin32/$rom.huf \$(RZLOADER)

EOF
}
foreach $rom (sort keys %roms_isa) {
	# ISA ROMs are prepared from the matching code images
	print <<EOF;
bin16/$rom.rom:	bin16/$rom.img \$(RLOADER)

bin16/$rom.lzrom:	bin16/$rom.huf \$(RZLOADER)

EOF
}

# and generate the .img image rules
print "\n# Rules to build the image files\n";
foreach $key (sort keys %drivers_pci) {
	# PCI images are prepared once per driver
	print <<EOF;
bin32/$key.tmp:	bin32/$key.o bin32/config-$key.o bin32/pci.o \$(STDDEPS32)
	\$(LD32) \$(LDFLAGS32) -o \$@ \$(START32) bin32/config-$key.o bin32/$key.o bin32/pci.o \$(LIBS32)
	@\$(SIZE32) \$@ | \$(CHECKSIZE)

bin32/$key.img:	bin32/$key.o bin32/$key.tmp bin32/config-$key.o bin32/pci.o \$(STDDEPS32)
	\$(LD32) \$(LDFLAGS32) \$(LDBINARY32) -o \$@ \$(START32) bin32/config-$key.o bin32/$key.o bin32/pci.o \$(LIBS32)

EOF
}
foreach $rom (sort keys %roms_isa) {
	# ISA images are prepared from the matching code images
	print <<EOF;
bin32/$rom.tmp:	bin32/$rom.o bin32/config-$rom.o \$(STDDEPS32)
	\$(LD32) \$(LDFLAGS32) -o \$@ \$(START32) bin32/config-$rom.o bin32/$rom.o \$(LIBS32)
	@\$(SIZE32) \$@ | \$(CHECKSIZE)

bin32/$rom.img:	bin32/$rom.o bin32/$rom.tmp bin32/config-$rom.o \$(STDDEPS32)
	\$(LD32) \$(LDFLAGS32) \$(LDBINARY32) -o \$@ \$(START32) bin32/config-$rom.o bin32/$rom.o \$(LIBS32)

EOF
}
foreach $rom (sort keys %roms_isa) {
	# ISA images are prepared from the matching code images
	print <<EOF;
bin16/$rom.tmp:	bin16/$rom.o bin16/config-$rom.o \$(STDDEPS16)
	\$(LD16) \$(LDFLAGS16) -o \$@ \$(START16) bin16/config-$rom.o bin16/$rom.o \$(LIBS16)
	@\$(SIZE16) \$@ | \$(CHECKSIZE)

bin16/$rom.img:	bin16/$rom.o bin16/$rom.tmp bin16/config-$rom.o \$(STDDEPS16)
	\$(LD16) \$(LDFLAGS16) \$(LDBINARY16) -o \$@ \$(START16) bin16/config-$rom.o bin16/$rom.o \$(LIBS16)

EOF
}

