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

use vars qw($nic $config @families $curfam %drivers %pcient %isaent %isalist %buildent $arch @srcs);

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
			$isalist{$rom} = $curfam;
			$buildent{$rom} = 1;
			push(@{$isaent{$curfam}}, [$rom, $comment]);
		}
	}
}

sub addfam ($) {
	my ($family) = @_;

	push(@families, $family);
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
	if ($ids ne '-') {
		# We store a list of PCI IDs and comments for each PCI target
		push(@{$pcient{$curfam}}, [$rom, $ids, $comment]);
	} else {
		# We store the base driver file for each ISA target
		$isalist{$rom} = $curfam;
		$buildent{$rom} = 1;
		push(@{$isaent{$curfam}}, [$rom, $comment]);
	}
}

# Return true if this driver is ISA only
sub isaonly ($) {
	my $aref = $pcient{$_[0]};

	return ($#$aref < 0);
}

$#ARGV >= 1 or die "Usage: $0 bin/NIC arch sources...\n";
$nic  = shift(@ARGV);
$arch = shift(@ARGV);
@srcs = @ARGV;
$config = <<'EOF';
# This is the config file for creating Makefile rules for Etherboot ROMs
#
# To make a ROM for a supported NIC locate the appropriate family
# and add a line of the form
#
# ROM		PCI-IDs		Comment
#
# ROM is the desired output name for both .rom and .lzrom images.
# PCI IDs are the PCI vendor and device IDs of the PCI NIC
# For ISA NICs put -
#
# All PCI ROMs that share a single driver are only built once (because they
# only have different PCI-IDs, but identical code).  ISA ROMS are built for
# each ROM type, because different vendors used a different logic around the
# basic chip.  The most popular example is the NS8390, which some cards use
# in PIO mode, some in DMA mode.  Two chips currently don't fit into this nice
# black-and-white scheme (the Lance and the NS8390).  Their driver deals
# with both PCI and ISA cards.  These drivers will be treated similarly to
# ISA only drivers by genrules.pl and are compiled for each ROM type that is
# ISA, and additionally compiled for the PCI card type.
#
# Then do: make clean, make Roms and make
#
# Please send additions to this file to <kenUNDERSCOREyap AT users PERIOD sourceforge PERIOD net>

# Start of configuration

family		drivers/net/skel

family		arch/ia64/drivers/net/undi_nii
undi_nii	-

# 3c59x cards (Vortex) and 3c900 cards
# If your 3c900 NIC detects but fails to work, e.g. no link light, with
# the 3c90x driver, try using the 3c595 driver. I have one report that the
# 3c595 driver handles these NICs properly. (The 595 driver uses the
# programmed I/O mode of operation, whereas the 90x driver uses the bus
# mastering mode. These NICs are capable of either mode.) When it comes to
# making a ROM, as usual, you must choose the correct image, the one that
# contains the same PCI IDs as your NIC.
family		drivers/net/3c595

# 3Com 3c90x cards
family		drivers/net/3c90x

# Intel Etherexpress Pro/100
family		drivers/net/eepro100

#Intel Etherexpress Pro/1000
family		drivers/net/e1000

#Broadcom Tigon 3
family		drivers/net/tg3

family		drivers/net/lance
ne2100		-		Novell NE2100, NE1500
ni6510		-		Racal-Interlan NI6510

family		drivers/net/tulip

family		drivers/net/davicom

family		drivers/net/rtl8139

family		drivers/net/via-rhine

family		drivers/net/w89c840

family		drivers/net/sis900

family		drivers/net/natsemi

family		drivers/net/fa311

family		drivers/net/prism2_plx

family		drivers/net/prism2_pci
# Various Prism2.5 (PCI) devices that manifest themselves as Harris Semiconductor devices
# (with the actual vendor appearing as the vendor of the first subsystem)
hwp01170	0x1260,0x3873	ActionTec HWP01170
dwl520		0x1260,0x3873	DLink DWL-520

family		drivers/net/ns8390
wd		-		WD8003/8013, SMC8216/8416, SMC 83c790 (EtherEZ)
ne		-		NE1000/2000 and clones
3c503		-		3Com503, Etherlink II[/16]

family		drivers/net/epic100

family		drivers/net/3c509
3c509		-		3c509, ISA/EISA
3c529		-		3c529 == MCA 3c509

family		drivers/net/3c515
3c515		-		3c515, Fast EtherLink ISA

family		drivers/net/eepro
eepro		-		Intel Etherexpress Pro/10

family		drivers/net/cs89x0
cs89x0		-		Crystal Semiconductor CS89x0

family		drivers/net/depca
depca		-		Digital DE100 and DE200

family		drivers/net/sk_g16
sk_g16		-		Schneider and Koch G16

family		drivers/net/smc9000
smc9000		-		SMC9000

family		drivers/net/sundance

family		drivers/disk/ide_disk
ide_disk	0x0000,0x0000	Generic IDE disk support

family		drivers/disk/pc_floppy
EOF

$curfam = '';
for $_ (split(/\n/, $config)) {
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

open(N,">$nic") or die "$nic: $!\n";
print N <<EOF;
# This is an automatically generated file, do not edit
# It does not affect anything in the build, it's only for rom-o-matic

EOF
foreach my $family (@families) {
	print N "family\t$family\n";
	if (exists($pcient{$family})) {
		my $aref = $pcient{$family};
		foreach my $entry (@$aref) {
			my $rom = $entry->[0];
			my $ids = $entry->[1];
			my $comment = $entry->[2];
			print N "$rom\t$ids\t$comment\n";
		}
	}
	if (exists($isaent{$family})) {
		my $aref = $isaent{$family};
		foreach my $entry (@$aref) {
			my $rom = $entry->[0];
			my $comment = $entry->[1];
			print N "$rom\t-\t$comment\n";
		}
	}
	print N "\n";
}
close(N);

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
foreach my $isa (sort keys %isalist) {
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
foreach my $isa (sort keys %isalist) {
	(my $macro = $isa) =~ tr/\-/_/;
	my $base = $isalist{$isa};
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
foreach my $isa (sort keys %isalist) {
	print <<EOF;
\$(BIN)/$isa.rom:		\$(BIN)/$isa.img \$(RLOADER)

\$(BIN)/$isa.zrom:	\$(BIN)/$isa.zimg \$(RLOADER)

EOF
}

