#!/usr/bin/perl -w
#
#	Helper program to generate Makefile rules into file Rom from table in
#	file NIC
#
#	GPL, Ken Yap 2001, with major contributions by Klaus Espenlaub
#	Revised 2002
#

use strict;

use vars qw($curfam %drivers %pcient %isaent %buildent);

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
		$buildent{$rom} = 1;
	} else {
		# We store a list of PCI IDs and comments for each PCI target
		push(@{$pcient{$curfam}}, [$rom, $ids, $comment]);
		$buildent{$curfam} = 1;
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
print "IMG3S2\t:=\n";
foreach my $img (sort keys %buildent) {
	print "IMGS32\t+= bin32/$img.img bin32/$img.lzimg\n"
}

print "ROMS32\t:=\n";
foreach my $family (sort keys %pcient) {
	my $aref = $pcient{$family};
	foreach my $entry (@$aref) {
		my $rom = $entry->[0];
		print "ROMS32\t+= bin32/$rom.rom bin32/$rom.lzrom\n";
	}
}
foreach my $isa (sort keys %isaent) {
	print "ROMS32\t+= bin32/$isa.rom bin32/$isa.lzrom\n";
}

# Generate the *.o rules
print "\n# Rules to build the driver object files\n";
foreach my $pci (sort keys %drivers) {
	# For ISA the rule for .o will generated later
	next if &isaonly($pci);
	# PCI drivers are compiled only once for all ROMs
	(my $macro = $pci) =~ tr/\-/_/;
	my $deps = $drivers{$pci};
	print <<EOF;

bin32/$pci.o:	$pci.c \$(MAKEDEPS) pci.h $deps
	\$(CC32) \$(CFLAGS32) \$(\U$macro\EFLAGS) -o \$@ -c \$<

bin32/$pci--%.o:	bin32/%.o bin32/$pci.o \$(MAKEDEPS)
	\$(LD32) -r bin32/$pci.o \$< -o \$@

EOF
}
# Do the ISA entries
foreach my $isa (sort keys %isaent) {
	(my $macro = $isa) =~ tr/\-/_/;
	my $base = $isaent{$isa};
	my $deps = $drivers{$base};
	print <<EOF;
bin32/$isa.o:	$base.c \$(MAKEDEPS) $deps
	\$(CC32) \$(CFLAGS32) \$(\U$macro\EFLAGS) -o \$@ -c \$<

bin32/$isa--%.o:	bin32/%.o bin32/$isa.o \$(MAKEDEPS)
	\$(LD32) -r bin32/$isa.o \$< -o \$@ 
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
bin32/$isa.rom:		bin32/$isa.img \$(RLOADER)

bin32/$isa.lzrom:	bin32/$isa.lzimg \$(RLOADER)

EOF
}

