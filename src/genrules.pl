#!/usr/bin/perl -w
#
#	Helper program to generate Makefile rules into file Rom from table in
#	file NIC
#
#	GPL, Ken Yap 2001, with major contributions by Klaus Espenlaub
#	Revised 2002
#

use strict;

use vars qw($curfam %drivers %pcient %isaent);

sub gendep ($) {
	my ($driver) = @_;

	# Automatically generate the dependencies for the driver sources.
	my %driver_dep = ();
	# The "$driver.c" is never inserted into %driver_dep, because it is
	# treated specially below.
	my @new_dep = ("$driver.c");
	while ($#new_dep >= 0) {
		my @collect_dep = ();
		foreach my $source (@new_dep) {
# Warn about failure to open, then skip, rather than soldiering on with the read
			unless (open(INFILE, "$source")) {
				print STDERR "$source: $! (shouldn't happen)\n";
				next;
			}
			while (<INFILE>) {
				chomp($_);
# This code is not very smart: no C comments or CPP conditionals processing is
# done.  This may cause unexpected (or incorrect) additional dependencies.
# However, ignoring the CPP conditionals is in some sense correct: we need to
# figure out a superset of all the headers for the driver source.  The pci.h
# file is treated specially, because we know which cards are PCI and not ISA.
				next unless (s/^\s*#include\s*"([^"]*)".*$/$1/);
				next if ($_ eq 'pci.h');
# Ignore system includes, like the ones in osdep.h
				next if ($_ =~ m:^/:);
				next if (exists $driver_dep{"$_"});
				$driver_dep{"$_"} = "$_";
				push(@collect_dep,($_));
			}
			close(INFILE);
		}
		@new_dep = @collect_dep;
	}
	return (join(' ', sort keys %driver_dep));
}

sub addfam ($) {
	my ($family) = @_;

	# We store the list of dependencies in the hash for each family
	$drivers{$family} = &gendep($family);
	$pcient{$family} = [];
}

sub addrom ($) {
	my ($rom, $ids, $comment) = split(' ', $_[0], 3);

	my $aref;
	# defaults if missing
	$ids = '-' unless ($ids);
	$comment = $rom unless ($comment);
	if ($ids eq '-') {
		# We store the base driver file for each ISA target
		$isaent{$rom} = $curfam;
	} else {
		# We store a list of PCI IDs and comments for each PCI target
		push(@{$pcient{$curfam}}, [$rom, $ids, $comment]);
	}
}

# Return true if this driver is ISA only
sub isaonly ($) {
	my $aref = $pcient{$_[0]};

	return ($#$aref < 0);
}

$#ARGV >= 0 or die "Usage: $0 configfile\n";
open(STDIN, $ARGV[0]) or die "$ARGV[0]: $!\n";

$curfam = '';
while(<>) {
	chomp($_);
	next if (/^\s*(#.*)?$/);
	my ($keyword) = split(' ', $_ , 2);
	if ($keyword eq 'family') {
		my ($keyword, $driver) = split(' ', $_, 2);
		if (! -e "$driver.c") {
			$curfam = '';
			print STDERR "Driver file $driver.c not found, skipping...\n";
			next;
		}
		&addfam($curfam = $driver);
	} else {
		# skip until we have a valid family
		next if ($curfam eq '');
		&addrom($_);
	}
}
close(STDIN);

# Generate the assignments to DOBJS and BINS
print "# Driver object files and ROM image files\n";
print "DOBJS32\t+= bin32/pci.o\n";
foreach my $key (sort keys %drivers) {
	# PCI drivers are compiled only once for all ROMs
	print "DOBJS32\t+= bin32/$key.o\n";
}
foreach my $family (sort keys %pcient) {
	my $aref = $pcient{$family};
	foreach my $entry (@$aref) {
		my $rom = $entry->[0];
		print "BINS32\t+= bin32/$rom.rom bin32/$rom.lzrom\n";
	}
}
foreach my $isa (sort keys %isaent) {
	print "BINS32\t+= bin32/$isa.rom bin32/$isa.lzrom\n";
}

# Helper macros for multi-driver ROMs
print <<EOF;

# Helper macros for multi-driver ROM builds
# \$* will be set to the driver list (e.g. rtl8139--prism2_pci)
#
# Examples:
# With \$* = rtl8139:
# MDROM_ALL_DRIVERS will expand to "rtl8139"
# MDROM_ALL_INCLUDES will expand to "-DINCLUDE_RTL8139"
# MDROM_ALL_OBJS will expand to "bin32/rtl8139.o"
# MDROM_TRY_ALL_DEVICES will expand to ""
#
# With \$* = rtl8139--prism2_pci:
# MDROM_ALL_DRIVERS will expand to "rtl8139 prism2_pci"
# MDROM_ALL_INCLUDES will expand to "-DINCLUDE_RTL8139 -DINCLUDE_PRISM2_PCI"
# MDROM_ALL_OBJS will expand to "bin32/rtl8139.o bin32/prism2_pci.o"
# MDROM_TRY_ALL_DEVICES will expand to "-DTRY_ALL_DEVICES"

MDROM_ALL_DRIVERS=\$(subst --, ,\$*)
MDROM_ALL_INCLUDES=\$(foreach include,\$(MDROM_ALL_DRIVERS),\$(INCLUDE_\$(include)))
MDROM_ALL_OBJS=\$(patsubst %,bin32/%.o,\$(MDROM_ALL_DRIVERS))
MDROM_TRY_ALL_DEVICES=\$(if \$(findstring --,\$*),-DTRY_ALL_DEVICES)

# List of all possible *.o dependencies for multi-driver ROMs
# Required since there doesn't seem to be any easy way to specify dependencies
# based on a function of the target name (because any functions get evaluated
# *before* the contents of % are substituted in)
MDROM_ALL_DEPS :=

# Rule to build the config-*.o files

bin32/config-%.o:	config.c \$(MAKEDEPS) osdep.h etherboot.h nic.h cards.h
	\$(CC32) \$(CFLAGS32) \$(MDROM_ALL_INCLUDES) \$(MDROM_TRY_ALL_DEVICES) -o \$@ -c \$<

EOF

# Generate the *.o rules
print "\n# Rules to build the driver object files\n";
foreach my $pci (sort keys %drivers) {
	# For ISA the rule for .o will generated later
	next if &isaonly($pci);
	# PCI drivers are compiled only once for all ROMs
	(my $macro = $pci) =~ tr/\-/_/;
	my $deps = $drivers{$pci};
	print <<EOF;
INCLUDE_$pci = -DINCLUDE_\U$macro\E

MDROM_ALL_DEPS += bin32/$pci.o

bin32/$pci.o:	$pci.c \$(MAKEDEPS) pci.h $deps
	\$(CC32) \$(CFLAGS32) \$(\U$macro\EFLAGS) -o \$@ -c \$<

EOF
}
# Do the ISA entries
foreach my $isa (sort keys %isaent) {
	(my $macro = $isa) =~ tr/\-/_/;
	my $base = $isaent{$isa};
	my $deps = $drivers{$base};
	print <<EOF;
INCLUDE_$isa = -DINCLUDE_\U$macro\E

MDROM_ALL_DEPS += bin32/$isa.o

bin32/$isa.o:	$base.c \$(MAKEDEPS) $deps
	\$(CC32) \$(CFLAGS32) \$(\U$macro\EFLAGS) -o \$@ -c \$<

EOF
}

# Generate the Rom rules
print "# Rules to build the ROM files\n";
foreach my $family (sort keys %pcient) {
	my $aref = $pcient{$family};
	foreach my $entry (@$aref) {
		my ($rom, $ids, $comment) = @$entry;
		next if ($ids eq '-');
		print <<EOF;
bin32/$rom.rom:	bin32/$family.img \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_\$*) -p $ids -i\$(IDENT32) \$@

bin32/$rom--%.rom:	bin32/$family--%.img \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_\$*) -p $ids -i\$(IDENT32) \$@

bin32/$rom.lzrom:	bin32/$family.lzimg \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_\$*) -p $ids -i\$(IDENT32) \$@

bin32/$rom--%.lzrom:	bin32/$family--%.huf \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_\$*) -p $ids -i\$(IDENT32) \$@

EOF
	}
}
# ISA ROMs are prepared from the matching code images
foreach my $isa (sort keys %isaent) {
	print <<EOF;
bin32/$isa.rom:	bin32/$isa.img \$(RLOADER)

bin32/$isa.lzrom:	bin32/$isa.huf \$(RZLOADER)

bin32/$isa.pxe:	bin32/$isa.img \$(PXELOADER)

bin32/$isa.lzpxe:	bin32/$isa.huf \$(PXEZLOADER)

EOF
}

# and generate the .img image rules
print "# Rules to build the image files\n";
foreach my $pci (sort keys %drivers) {
	# If there are no PCI ID entries, it's an ISA only family
	next if &isaonly($pci);
	# PCI images are prepared once per driver
	print <<EOF;
bin32/$pci.tmp:	bin32/$pci.o bin32/config-$pci.o bin32/pci.o \$(STDDEPS32)
	\$(LD32) \$(LDFLAGS32) -o \$@ \$(START32) bin32/config-$pci.o bin32/$pci.o bin32/pci.o \$(LIBS32)
	@\$(SIZE32) \$@ | \$(CHECKSIZE)

bin32/$pci--%.tmp:	bin32/$pci.o \$(MDROM_ALL_DEPS) bin32/config-$pci--%.o bin32/pci.o \$(STDDEPS32)
	\$(LD32) \$(LDFLAGS32) -o \$@ \$(START32) bin32/config-$pci--\$*.o bin32/$pci.o \$(MDROM_ALL_OBJS) bin32/pci.o \$(LIBS32)
	@\$(SIZE32) \$@ | \$(CHECKSIZE)

EOF
}
foreach my $isa (sort keys %isaent) {
	# ISA images are built one per entry
	print <<EOF;
bin32/$isa.tmp:	bin32/$isa.o bin32/config-$isa.o \$(STDDEPS32)
	\$(LD32) \$(LDFLAGS32) -o \$@ \$(START32) bin32/config-$isa.o bin32/$isa.o \$(LIBS32)
	@\$(SIZE32) \$@ | \$(CHECKSIZE)

EOF
}
