#!/usr/bin/perl -w

use Getopt::Std;

use constant MINROMSIZE => 8192;
use constant MAXROMSIZE => 65536;

use constant PCI_PTR_LOC => 0x18;	# from beginning of ROM
use constant PCI_HDR_SIZE => 0x18;
use constant PNP_PTR_LOC => 0x1a;	# from beginning of ROM
use constant PNP_HDR_SIZE => 0x20;
use constant PNP_CHKSUM_OFF => 0x9;	# bytes from beginning of PnP header
use constant PNP_DEVICE_OFF => 0x10;	# bytes from beginning of PnP header
use constant PCI_VEND_ID_OFF => 0x4;	# bytes from beginning of PCI header
use constant PCI_DEV_ID_OFF => 0x6;	# bytes from beginning of PCI header
use constant PCI_SIZE_OFF => 0x10;	# bytes from beginning of PCI header

use strict;

use vars qw(%opts $rom);

sub getromsize ($) {
	my ($romref) = @_;
	my $i;

	print STDERR "BIOS extension ROM Image did not start with 0x55 0xAA\n"
		if (substr($$romref, 0, 2) ne "\x55\xaa");
	my $size = ord(substr($$romref, 2, 1)) * 512;
	for ($i = MINROMSIZE; $i < MAXROMSIZE and $i < $size; $i *= 2) { }
	print STDERR "$size is a strange size for a boot ROM\n"
		if ($size > 0 and $i > $size);
	return ($size);
}

sub addident ($) {
	my ($romref) = @_;

	return (0) unless (my $s = $opts{'i'});
	# include the terminating NUL byte too
	$s .= "\x00";
	my $len = length($s);
	# Put the identifier in only if the space is blank
	my $pos = length($rom) - $len - 2;
	return (0) if (substr($$romref, $pos, $len) ne ("\xFF" x $len));
	substr($$romref, $pos, $len) = $s;
	return ($pos);
}

sub pcipnpheaders ($$) {
	my ($romref, $identoffset) = @_;
	my ($pci_hdr_offset, $pnp_hdr_offset);

	$pci_hdr_offset = unpack('v', substr($$romref, PCI_PTR_LOC, 2));
	$pnp_hdr_offset = unpack('v', substr($$romref, PNP_PTR_LOC, 2));
	# Sanity checks
	if ($pci_hdr_offset < PCI_PTR_LOC + 2
		or $pci_hdr_offset > length($$romref) - PCI_HDR_SIZE
		or $pnp_hdr_offset < PNP_PTR_LOC + 2
		or $pnp_hdr_offset > length($$romref) - PNP_HDR_SIZE
		or substr($$romref, $pci_hdr_offset, 4) ne 'PCIR'
		or substr($$romref, $pnp_hdr_offset, 4) ne '$PnP') {
		$pci_hdr_offset = $pnp_hdr_offset = 0;
	} else {
		printf "PCI header at 0x%x and PnP header at 0x%x\n",
			$pci_hdr_offset, $pnp_hdr_offset;
	}
	if ($pci_hdr_offset > 0) {
		substr($$romref, $pci_hdr_offset + PCI_SIZE_OFF, 2)
			= pack('v', length($$romref) / 512);
		my ($pci_vendor_id, $pci_device_id) = split(/,/, $opts{'p'});
		substr($$romref, $pci_hdr_offset+PCI_VEND_ID_OFF, 2)
			= pack('v', oct($pci_vendor_id)) if ($pci_vendor_id);
		substr($$romref, $pci_hdr_offset+PCI_DEV_ID_OFF, 2)
			= pack('v', oct($pci_device_id)) if ($pci_device_id);
	}
	if ($pnp_hdr_offset > 0) {
		# Point to device id string at end of ROM image
		substr($$romref, $pnp_hdr_offset+PNP_DEVICE_OFF, 2)
			= pack('v', $identoffset);
		substr($$romref, $pnp_hdr_offset+PNP_CHKSUM_OFF, 1) = "\x00";
		my $sum = unpack('%8C*', substr($$romref, $pnp_hdr_offset,
			PNP_HDR_SIZE));
		substr($$romref, $pnp_hdr_offset+PNP_CHKSUM_OFF, 1) = chr(256 - $sum);
	}
}

sub writerom ($$) {
	my ($filename, $romref) = @_;

	open(R, ">$filename") or die "$filename: $!\n";
	print R $$romref;
	close(R);
}

# Main routine
getopts('3xi:p:s:v', \%opts);
$ARGV[0] or die "Usage: $0 [-s romsize] [-i ident] [-p vendorid,deviceid] [-x] [-3] rom-file\n";
open(R, $ARGV[0]) or die "$ARGV[0]: $!\n";
# Read in the whole ROM in one gulp
my $filesize = read(R, $rom, MAXROMSIZE+1);
close(R);
defined($filesize) and $filesize >= 3 or die "Cannot get first 3 bytes of file\n";
print "$filesize bytes read\n" if $opts{'v'};
# If PXE image, just fill the length field and write it out
if ($opts{'x'}) {
	substr($rom, 2, 1) = chr((length($rom) + 511) / 512);
	&writerom($ARGV[0], \$rom);
	exit(0);
}
# Don't work out romsize in image if size specified
my $romsize = defined($opts{'s'}) ? oct($opts{'s'}) : &getromsize(\$rom);
$romsize = MAXROMSIZE if ($romsize <= 0);
print STDERR "ROM size of $romsize not big enough for data\n"
	if ($filesize > $romsize);
# Shrink it down to the smallest size that will do
for ($romsize = MAXROMSIZE;
	$romsize > MINROMSIZE and $romsize >= 2*$filesize;
	$romsize /= 2) { }
# Pad with 0xFF to $romsize
$rom .= "\xFF" x ($romsize - length($rom));
substr($rom, 2, 1) = chr($romsize / 512);
print "ROM size is $romsize\n" if $opts{'v'};
my $identoffset = &addident(\$rom);
&pcipnpheaders(\$rom, $identoffset);
# 3c503 requires last two bytes to be 0x80
substr($rom, MINROMSIZE-2, 2) = "\x80\x80"
	if ($opts{'3'} and $romsize == MINROMSIZE);
substr($rom, 5, 1) = "\x00";
my $sum = unpack('%8C*', $rom);
substr($rom, 5, 1) = chr(256 - $sum);
# Double check
$sum = unpack('%8C*', $rom);
if ($sum != 0) {
	print "Checksum fails\n"
} elsif ($opts{'v'}) {
	print "Checksum ok\n";
}
&writerom($ARGV[0], \$rom);
exit(0);
