#!/usr/bin/perl -w
#
# Program to apply an elf header to an i386 etherboot file.
#
# GPL v2  Eric Biederman 2003
#

use strict;

main(@ARGV);

sub usage
{
	my ($err) = @_;
	print STDERR $err , "\n";
	die "Usage $0 load_addr prrefix file bss_size\n";
}
sub main
{
	my ($load_addr, $prefix_name, $suffix_name, $bss_size) = @_;
	usage("No load addr") unless(defined($load_addr));
	usage("No prefix") unless (defined($prefix_name));
	usage("No suffix") unless (defined($suffix_name));
	usage("No bss size") unless (defined($bss_size));
	
	$load_addr = oct($load_addr) or die "Invalid load address";
	open(PREFIX, "<$prefix_name") or die "Cannot open $prefix_name";
	open(SUFFIX, "<$suffix_name") or die "Cannot open $suffix_name";

	$/ = undef;
	my $prefix = <PREFIX>; close(PREFIX);
	my $suffix = <SUFFIX>; close(SUFFIX);

	# Payload sizes.
	my $payload_size = length($suffix);
	my $payload_bss  = $bss_size;

	# Update the image size
	my $image_size_off     = 0x64;
	my $image_mem_size_off = 0x68;
	my $load_off1          = 0x18;
	my $load_off2	       = 0x5c;
	my $load_off3          = 0x60;

	my $image_size     = unpack("V", substr($prefix, $image_size_off, 4));
	my $image_mem_size = unpack("V", substr($prefix, $image_mem_size_off, 4));

	#print(STDERR "load_addr:      $load_addr\n");
	#print(STDERR "image_size:     $image_size\n");
	#print(STDERR "image_mem_size: $image_mem_size\n");

	$image_size     += $payload_size;
	$image_mem_size += $payload_size + $payload_bss;

	substr($prefix, $image_size_off, 4)     = pack("V", $image_size);
	substr($prefix, $image_mem_size_off, 4) = pack("V", $image_mem_size);
	substr($prefix, $load_off1, 4)          = pack("V", $load_addr);
	substr($prefix, $load_off2, 4)          = pack("V", $load_addr);
	substr($prefix, $load_off3, 4)          = pack("V", $load_addr);

	#print(STDERR "image_size:     $image_size\n");
	#print(STDERR "image_mem_size: $image_mem_size\n");

	print $prefix;
	print $suffix;
}
