#!/usr/bin/perl -w

# Short script to combine boot1a.bin start16.bin NIC.z?img and to
# put the block count in the LE word at location 2

use POSIX;

use constant BLOCKSIZE => 512;
use constant TRACKSIZE => 36;	# sectors per track

use strict;

use vars qw($image $length $nblocks $trackblocks $i);

undef($/);
$image = "";
while (<>) {
	$image .= $_;
}
$length = length($image);
$nblocks = int(($length + BLOCKSIZE - 1) / BLOCKSIZE);
# Put the block count in the LE word at offset 2
substr($image,2,2) = pack('v', $nblocks);
$trackblocks = int(($nblocks + TRACKSIZE - 1) / TRACKSIZE) * TRACKSIZE;
$image .= "\0" x ($trackblocks * BLOCKSIZE - $length);
print $image;
