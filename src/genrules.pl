#!/usr/bin/perl -w
#
#	Helper program to generate Makefile rules into file Rom from table in
#	file NIC
#
#	GPL, Ken Yap 2001, with major contributions by Klaus Espenlaub
#	Revised 2002
#

use strict;
use File::Basename;

use vars qw($curfam %drivers %pcient %isaent %buildent $arch $configfile @srcs);

sub __gendep ($$$)
{
	my ($file, $deps, $driver_dep) = @_;
	foreach my $source (@$deps) {
		my $inc;
		my @collect_dep = ();
		$inc = "arch/$arch/include/$source" unless ! -e "arch/$arch/include/$source";
		$inc = "include/$source" unless ! -e "include/$source";
		$inc = dirname($file) . "/$source" unless ! -e dirname($file) . "/$source";
		unless (defined($inc)) {
			print STDERR "$source from $file not found (shouldn't happen)\n";
			next;
		};
		next if (exists ${$driver_dep}{$inc});
		${$driver_dep}{$inc} = $inc;
# Warn about failure to open, then skip, rather than soldiering on with the read
		unless (open(INFILE, "$inc")) {
			print STDERR "$inc: $! (shouldn't happen)\n";
			next;
		};
		while (<INFILE>) {
			chomp($_);
# This code is not very smart: no C comments or CPP conditionals processing is
# done.  This may cause unexpected (or incorrect) additional dependencies.
# However, ignoring the CPP conditionals is in some sense correct: we need to
# figure out a superset of all the headers for the driver source.  
			next unless (s/^\s*#include\s*"([^"]*)".*$/$1/);
# Ignore system includes, like the ones in osdep.h
			next if ($_ =~ m:^/:);
			push(@collect_dep, $_);
		}
		close(INFILE);
		if (@collect_dep) {
			&__gendep($inc, \@collect_dep, $driver_dep);
		}
	}
}

sub gendep ($) {
	my ($driver) = @_;

	# Automatically generate the dependencies for the driver sources.
	my %driver_dep = ();
	__gendep( "", [ $driver ], \%driver_dep);
	return sort values %driver_dep
}

sub genroms($) {
	my ($driver) = @_;
	
	# Automatically discover the ROMS this driver can produce.
	unless (open(INFILE, "$driver")) {
		print STDERR "$driver: %! (shouldn't happen)\n";
		next;
	};
	while (<INFILE>) {
		chomp($_);
		if ($_ =~ m/^\s*PCI_ROM\(\s*0x([0-9A-Fa-f]*)\s*,\s*0x([0-9A-Fa-f]*)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\)/) {

			# We store a list of PCI IDs and comments for each PC target
			my ($vendor_id, $device_id, $rom, $comment) = (hex($1), hex($2), $3, $4);
			my $ids = sprintf("0x%04x,0x%04x", $vendor_id, $device_id);
			push(@{$pcient{$curfam}}, [$rom, $ids, $comment]);
		}
		elsif($_ =~ m/^\s*ISA_ROM\(\s*"([^"]*)"\s*,\s*"([^"]*)"\)/) {
			my ($rom, $comment) = ($1, $2);
			# We store the base driver file for each ISA target
			$isaent{$rom} = $curfam;
			$buildent{$rom} = 1;
		}
	}
}

sub addfam ($) {
	my ($family) = @_;

	# We store the list of dependencies in the hash for each family
	my @deps = &gendep("$family.c");
	$drivers{$family} = join(' ', @deps);
	$pcient{$family} = [];
	genroms("$family.c");
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
	}
}

# Return true if this driver is ISA only
sub isaonly ($) {
	my $aref = $pcient{$_[0]};

	return ($#$aref < 0);
}

$#ARGV >= 1 or die "Usage: $0 arch configfile sources...\n";
$arch = shift(@ARGV);
$configfile = shift(@ARGV);
@srcs = @ARGV;

open(CONFIG, "<$configfile") or die "Cannot open $configfile";
$curfam = '';
while(<CONFIG>) {
	chomp($_);
	next if (/^\s*(#.*)?$/);
	my ($keyword) = split(' ', $_ , 2);
	if ($keyword eq 'family') {
		my ($keyword, $driver) = split(' ', $_, 2);
		$curfam = '';
		if (! -e "$driver.c") {
			print STDERR "Driver file $driver.c not found, skipping...\n";
			next;
		}
		if ($driver =~ "^arch" && $driver !~ "^arch/$arch") {
			print STDERR "Driver file $driver.c not for arch $arch, skipping...\n";
			next;
		}
		&addfam($curfam = $driver);
	} else {
		# skip until we have a valid family
		next if ($curfam eq '');
		&addrom($_);
	}
}
close(CONFIG);

# Generate the normal source dependencies
print "# Core object file dependencies\n";
foreach my $source (@srcs) {
	next if ($source !~ '[.][cS]$');
	my @deps = &gendep($source);
	my $obj = $source;
	$obj =~ s/[.][cS]/.o/;
	foreach my $dep (@deps) {
		print "$obj: $dep\n";
	}
	print("\n");
}

# Generate the assignments to DOBJS and BINS
print "# Driver object files and ROM image files\n";
print "IMGS\t:=\n";
print "DOBJS\t:=\n";
 print "PCIOBJS\t:=\n";
foreach my $pci (sort keys %pcient) {
	my $img = basename($pci);
	print "DOBJS\t+= \$(BIN)/$img.o\n";
	print "PCIOBJS\t+= \$(BIN)/$img.o\n";
	print "IMGS\t+= \$(BIN)/$img.img \$(BIN)/$img.zimg\n";
}
foreach my $img (sort keys %buildent) {
	print "DOBJS\t+= \$(BIN)/$img.o\n";
	print "IMGS\t+= \$(BIN)/$img.img \$(BIN)/$img.zimg\n";
}

print "ROMS\t:=\n";
foreach my $family (sort keys %pcient) {
	my $aref = $pcient{$family};
	foreach my $entry (@$aref) {
		my $rom = $entry->[0];
		print "ROMS\t+= \$(BIN)/$rom.rom \$(BIN)/$rom.zrom\n";
	}
}
foreach my $isa (sort keys %isaent) {
	print "ROMS\t+= \$(BIN)/$isa.rom \$(BIN)/$isa.zrom\n";
}

# Generate the *.o rules
print "\n# Rules to build the driver object files\n";
foreach my $pci (sort keys %drivers) {
	# For ISA the rule for .o will generated later
	next if &isaonly($pci);
	# PCI drivers are compiled only once for all ROMs
	(my $macro = basename($pci)) =~ tr/\-/_/;
	my $obj = basename($pci);
	my $deps = $drivers{$pci};
	print <<EOF;

\$(BIN)/$obj.o:	$pci.c \$(MAKEDEPS) $deps
	\$(CC) \$(CFLAGS) \$(\U$macro\EFLAGS) -o \$@ -c \$<

\$(BIN)/$obj--%.o:	\$(BIN)/%.o \$(BIN)/$obj.o \$(MAKEDEPS)
	\$(LD) -r \$(BIN)/$obj.o \$< -o \$@

EOF
}

# Do the ISA entries
foreach my $isa (sort keys %isaent) {
	(my $macro = $isa) =~ tr/\-/_/;
	my $base = $isaent{$isa};
	my $deps = $drivers{$base};
	print <<EOF;
\$(BIN)/$isa.o:	$base.c \$(MAKEDEPS) $deps
	\$(CC) \$(CFLAGS) \$(\U$macro\EFLAGS) -o \$@ -c \$<

\$(BIN)/$isa--%.o:	\$(BIN)/%.o \$(BIN)/$isa.o \$(MAKEDEPS)
	\$(LD) -r \$(BIN)/$isa.o \$< -o \$@ 
EOF
}

# Generate the Rom rules
print "# Rules to build the ROM files\n";
foreach my $family (sort keys %pcient) {
	my $aref = $pcient{$family};
	foreach my $entry (@$aref) {
		my ($rom, $ids, $comment) = @$entry;
		my $img = basename($family);
		next if ($ids eq '-');
		print <<EOF;
\$(BIN)/$rom.rom:	\$(BIN)/$img.img \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_FLAGS) \$(MAKEROM_\$*) -p $ids -i\$(IDENT) \$@

\$(BIN)/$rom--%.rom:	\$(BIN)/$img--%.img \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_FLAGS) \$(MAKEROM_\$*) -p $ids -i\$(IDENT) \$@

\$(BIN)/$rom.zrom:	\$(BIN)/$img.zimg \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_FLAGS) \$(MAKEROM_\$*) -p $ids -i\$(IDENT) \$@

\$(BIN)/$rom--%.zrom:	\$(BIN)/$img--%.zimg \$(PRLOADER) \$(START16)
	cat \$(PRLOADER) \$(START16) \$< > \$@
	\$(MAKEROM) \$(MAKEROM_FLAGS) \$(MAKEROM_\$*) -p $ids -i\$(IDENT) \$@

EOF
		next if($rom eq $img);
		print <<EOF;
\$(BIN)/$rom.img:	\$(BIN)/$img.img
	cat \$< > \$@

EOF
	}
}

# ISA ROMs are prepared from the matching code images
foreach my $isa (sort keys %isaent) {
	print <<EOF;
\$(BIN)/$isa.rom:		\$(BIN)/$isa.img \$(RLOADER)

\$(BIN)/$isa.zrom:	\$(BIN)/$isa.zimg \$(RLOADER)

EOF
}

