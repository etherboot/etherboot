# Perl subroutine to shorten a floppy image by omitting trailing sectors
# that are not in use
#
# AFAIK works on FAT12 and FAT16. Won't work on FAT32
#
# Placed under GNU Public License by Ken Yap, April 2000

package TruncFD;

use strict;
use IO::Seekable;
use POSIX qw(ceil);

use constant;
use constant DEBUG => 0;

sub truncfd {
	my ($file) = @_;
	my ($nread, $buffer, $cylinders, $clusters, $fatsectors,
		$rootdirsectors, @fatents, $i, $lastclus, $clusstart, $lastsector);
	
	return -1 if (!defined($file) or !open(F, "$file"));
	$nread = read(F, $buffer, 512);
	return -1 if (!defined($nread));

	my ($dummy1, $bytepersect, $sectperclus, $resvsectors, $fats, $rootdirents,
		$sectors, $dummy2, $sectperfat, $sectpertrk, $heads, $hidsectors,
		$dummy3, $fstype) =
		unpack('a11vCvCvvavvvva24Z5', $buffer);
	$cylinders = $sectors / ($sectpertrk * $heads);
	$clusters = $sectors / $sectperclus;
	$fatsectors = $fats * $sectperfat;
	$rootdirsectors = POSIX::ceil(($rootdirents * 32) / $bytepersect);
	if (DEBUG) {
		print STDERR <<EOF;
$fstype filesystem:

Bytes/sector:		$bytepersect
Heads:			$heads
Cylinders:		$cylinders
Sectors/cluster:	$sectperclus
Clusters:		$clusters
Reserved sectors:	$resvsectors
Hidden sectors:		$hidsectors
Total sectors:		$sectors
Sectors/track:		$sectpertrk
FATs:			$fats
Sectors/FAT:		$sectperfat
FAT sectors:		$fatsectors
Root dir entries:	$rootdirents
Root dir sectors:	$rootdirsectors

EOF
	}
	return -1 if (!seek(F, $resvsectors * 512, SEEK_SET));
	$nread = read(F, $buffer, $sectperfat * 512);
	close(F);
	return -1 if (!defined($nread));
	# Default assumption is FAT16
	# For FAT12
	if ($fstype eq 'FAT12') {
		$buffer =~ s/(...)/$1\x00/sg;
	}
	@fatents = unpack("V$clusters", $buffer);
	# For FAT12 shift bits 23..12 up 4 bits to make it just like FAT16
	if ($fstype eq 'FAT12') {
		foreach $i (0..$#fatents) {
			$fatents[$i] = ($fatents[$i] & 0xFFF) | ($fatents[$i] >> 12) << 16;
		}
	}
	# Make a string of it
	$buffer = pack("L*", @fatents);
	# Then extract in little endian format
	@fatents = unpack("v*", $buffer);
	$#fatents = $clusters if ($clusters < $#fatents);
	foreach $i (reverse(0..$#fatents)) {
		$lastclus = $i, last if ($fatents[$i] != 0);
	}
	print STDERR "Last used cluster is $lastclus\n" if (DEBUG);
	$clusstart = $resvsectors + $fatsectors + $rootdirsectors;
	$lastsector = $clusstart + ($lastclus - 1) * $sectperclus;
	print STDERR "Last used sector is $lastsector\n" if (DEBUG);
	return ($lastsector * 512);
}

1;
