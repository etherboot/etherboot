#!/usr/bin/perl -w
#
# Quick and dirty program to decode a partition table in MBR
# GPL, July 2000, Ken Yap
#

sub decodechs {
	my ($chs) = @_;
	my ($c, $h, $s);

	($h, $s, $c) = unpack('CCC', $chs);
	$c += ($s & 0xC0) << 2;
	$s &= 0x3F;
	return ($c, $h, $s);
}

sub dismbr {
	my ($par) = @_;
	my ($flags, $chs1, $type, $chs2, $bootseg, $numsegs);

	($flags, $chs1, $type, $chs2, $bootseg, $numsegs) =
		unpack('Ca3Ca3VV', $par);
	printf "%s type:%02x %d/%d/%d-%d/%d/%d boot:%04x sectors:%04x\n",
		($flags & 0x80) ? '*' : ' ', $type,
		decodechs($chs1), decodechs($chs2),
		$bootseg, $numsegs;
}

if ($#ARGV >= 0) {
	open(STDIN, "$ARGV[0]") or die "$ARGV[0]: $!\n";
}
$nread = read(STDIN, $mbr, 512);
(defined($nread) and $nread == 512) or die "Cannot read 512 bytes of MBR\n";
(undef, $par[0], $par[1], $par[2], $par[3], $sig) =
	unpack('a446a16a16a16a16v', $mbr);
$sig == 0xAA55 or die "Input is not a MBR\n";
foreach $i (0..3) {
	print "$i: ";
	dismbr($par[$i]);
}
