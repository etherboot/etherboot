#!/usr/bin/perl -w
#
# Quick Perl program to decode and display details about 
# tagged images created by mknbi
# -e extracts directory to `nbidir' and segments to `segmentN'
# N = 0, 1, 2, ...
#
# Added code to dump vendor tag in hex (for DOS disk parameters)
#
# Ken Yap, August 1999
#

use strict;

use vars qw($imagefile $data $curoffset @seglengths $vendordata
	$extract $segnum $status $i);

sub getvendordata {
	my ($flags) = @_;

	my $vendordata = '';
	my $vendorlen = ($flags & 0xff) >> 4;
	if ($vendorlen > 0) {
		$vendorlen *= 4;
		$vendordata = unpack("a$vendorlen", substr($data, $curoffset));
		$curoffset += $vendorlen;
	}
	return ($vendordata);
}

sub decodesegmentflags {
	my ($flags) = @_;
	my ($type);

	$flags >>= 24;
	$flags &= 0x3;
	($flags == 0) and $type = "Absolute";
	($flags == 1) and $type = "Follows last segment";
	($flags == 2) and $type = "Below end of memory";
	($flags == 3) and $type = "Below last segment loaded";
	return ($type);
}

sub onesegment
{
	my ($segnum) = @_;
	my ($type, $vendordata, @vdata, $i);

	my ($flags, $loadaddr, $imagelen, $memlength) = unpack("V4", substr($data, $curoffset));
	$curoffset += 16;
	print "Segment number $segnum\n";
	printf "Load address:\t\t%08x\n", $loadaddr;
	printf "Image length:\t\t%d\n", $imagelen;
	printf "Memory length:\t\t%d\n", $memlength;
	$type = &decodesegmentflags($flags);
	printf "Position:\t\t$type\n";
	printf "Vendor tag:\t\t%d\n", ($flags >> 8) & 0xff;
	if (($vendordata = &getvendordata($flags)) ne '') {
		print "Vendor data:\t\t\"", $vendordata, "\"\n";
		@vdata = unpack('C*', $vendordata);
		print "Vendor data in hex:\t";
		foreach $i (0..$#vdata) {
			printf "%02x ", $vdata[$i];
		}
		print "\n";
	}
	print "\n";
	push (@seglengths, $imagelen);
	return (($flags >> 26) & 1);
}

$extract = 0;
@seglengths = ();
$#ARGV >= 0 or die "Usage: disnbi [-e] tagged-image-file\n";
if ($ARGV[0] eq '-e') {
	$extract = 1; shift
}
$#ARGV >= 0 or die "Usage: disnbi [-e] tagged-image-file\n";
$imagefile= $ARGV[0];
open(I, $ARGV[0]) or die "$imagefile: $!\n";
(defined($status = sysread(I, $data, 512)) and $status == 512)
	or die "$imagefile: Cannot read header\n";
my ($magic, $flags, $bx, $ds, $ip, $cs) = unpack("a4Vv4", substr($data, 0, 16));
$magic eq "\x36\x13\x03\x1B" or die "$imagefile: Not a tagged image file\n";
$curoffset = 16;

# Now decode the header

printf "Header location:\t%04x:%04x\n", $ds, $bx;
printf "Start address:\t\t%04x:%04x\n", $cs, $ip;
printf "Flags:\n";
	print "Return to loader after execution (extension)\n" if (($flags >> 8) &  1);
if (($vendordata = &getvendordata($flags)) ne '') {
	print "Vendor data:\t\t", $vendordata, "\n";
}
print "\n";

# Now decode each segment record

$segnum = 1;
do {
	$i = &onesegment($segnum);
	++$segnum;
} while (!$i);

exit(0) if ($extract == 0);
print "Dumping directory to `nbidir'...\n";
open(O, ">nbidir") or die "nbidir: $!\n";
print O $data;
close(O);
$data = '';
foreach $i (0..$#seglengths) {
	print "Extracting segment $i to `segment$i'...\n";
	open(O, ">segment$i") or die "segment$i: $!\n";
	(defined($status = sysread(I, $data, $seglengths[$i]))
		and $status = $seglengths[$i])
		or die "$imagefile: Cannot read data\n";
	print O $data;
	close(O);
}
print "Done\n";
exit(0);

__END__

=head1 NAME

disnbi - display network bootable image

=head1 SYNOPSIS

B<disnbi> [C<-e>] I<nbifile>

=head1 DESCRIPTION

B<disnbl> is a program that to display in symbolic form the contents
of a network bootable image created by mknbi.

B<-e> Extract contents of image as well. The directory will be written
to C<nbidir> and the segments to C<segment>I<n> where I<n> is 0, 1, 2,
etc.

=head1 BUGS

Please report all bugs to the author.

=head1 SEE ALSO

Etherboot tutorial at C<http://etherboot.sourceforge.net/>

=head1 COPYRIGHT

B<disnbl> is under the GNU Public License

=head1 AUTHOR

Ken Yap (C<ken_yap@users.sourceforge.net>)

=head1 DATE

Version 1.0 April 2000
