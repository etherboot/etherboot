#!/usr/bin/perl -w
#
# Perl script to make a bootable image from a floppyfw floppy
# The basic idea is to unpack and replace or convert all
# the necessary config files into the initrd
# and then make a bootable image out of it

use strict;

use vars qw($testing $verbose $libdir $tftpdir $tempdir $tempmount);

sub findversion () {
	my ($version) = grep(/FloppyFW/, `mtype a:floppyfw.msg`);
	return '' unless defined($version) and $version ne '';
	chomp($version);
	$version =~ s/.*(\d+\.\d+\.\d+).*/$1/;
	return ($version);
}

# Copy whole floppy to the current directory
# m preserves timestamps, n overwrites without warning and / means recursive
sub mcopy () {
	my $status = system('mcopy', '-mn/', 'a:*', '.');
	return ($status / 256);
}

# Gunzip file, -f forces overwriting of uncompressed file
sub gunzip ($) {
	my ($file) = @_;

	print "Gunzipping $file\n" if ($verbose);
	my $status = system('gunzip', '-f', $file);
	return ($status / 256);
}

# Gzip file, -f forces overwriting of compressed file
sub gzip ($) {
	my ($file) = @_;

	print "Gzipping $file\n" if ($verbose);
	my $status = system('gzip', '-f', $file);
	return ($status / 256);
}

sub loopbackmount ($$) {
	my ($file, $point) = @_;

	print "Mounting $file on $point loopback\n" if ($verbose);
	my $status = system('mount', '-t', 'ext2', '-o', 'loop', $file, $point);
	return ($testing ? 0 : $status / 256);
}

sub loopbackumount ($) {
	my ($point) = @_;

	print "Umounting $point\n" if ($verbose);
	my $status = system('umount', $point);
	return ($testing ? 0 : $status / 256);
}

# Convert DOS CR-NL to Unix NL. $dst has implied prefix of $tempmount
# Use @output for temporary storage in case we write back to the same file
sub dostounix ($$) {
	my ($src, $dst) = @_;
	my @output = ();

	$dst = "$tempmount/$dst";
	print "Converting $src to $dst\n" if ($verbose);
	unless (open(S, $src)) {
		print "$src: $!\n";
		return;
	}
	while (<S>) {
		chomp;
		tr /\015//d;
		push(@output, $_);
	}
	close(S);
	open(D, ">$dst") or return;
	for $_ (@output) {
		print D "$_\n";
	}
	close(D);
	chmod(0755, $dst);
}

sub bunzip2untar ($$) {
	my ($file, $dir) = @_;

	print "Unpacking $file into $dir\n" if ($verbose);
	system("bunzip2 < $file | (cd $dir; tar xf -)");
}

$testing = $< != 0;
$verbose = 1;
$libdir = '/usr/local/lib/mkffwnb';
$tftpdir = '/tftpdir';
print <<EOF;
This program requires mtools, tar, bzip2, loopback mount in the kernel,
and root privileges to execute. Hope you have them.
EOF
my $version = &findversion();
$version ne '' or die "Cannot determine version\n";
print "Version $version\n";
$libdir .= '/' . $version;
-d $libdir or die "Cannot find files for $version\n";
$tempdir = '/tmp/mkffwnb';
$tempmount = 'tmpmount';
mkdir($tempdir, 0755);
chdir($tempdir);
print "Copying files off floppy, please be patient...\n";
&mcopy() == 0 or die "Mcopy failed, diskette problem?\n";
&gunzip('initrd.gz') == 0 or die "Gunzip of initrd.gz failed\n";
mkdir($tempmount, 0755);
&loopbackmount('initrd', $tempmount) == 0 or die "Loopback mount failed\n";
&dostounix("$libdir/linuxrc", "linuxrc");
&dostounix("$libdir/floppyfw.ini", "floppyfw.ini");
&dostounix("config", "etc/config");
for my $i (glob('floppyfw/add.bz2 modules/*.bz2 packages/*.bz2')) {
	&bunzip2untar($i, $tempmount);
}
for my $i (glob('packages/pre-*.ini packages/post-*.ini')) {
	my $file = $i;
	$file =~ s:packages/::;
	&dostounix($i, "etc/$file");
}
&dostounix("hosts", "etc/hosts");
&dostounix("modules.lst", "etc/modules.lst");
&dostounix("network.ini", "etc/network.init");
&dostounix("firewall.ini", "etc/firewall.init");
&dostounix("syslog.cfg", "etc/syslog.conf");
system("cp -p licenses/* $tempmount/licenses/");
# This conditional code is for 1.1.2 and below
unless (glob('modules/*.bz2')) {
	print "Copying additional modules\n" if ($verbose);
	system("cp -p modules/* $tempmount/lib/modules/");
}
&loopbackumount($tempmount) == 0 or die "Loopback umount failed\n";
&gzip('initrd') == 0 or die "Gzip of initrd failed\n";
print "Calling mknbi-linux to make the netbootable image\n" if ($verbose);
system("mknbi-linux --output=$tftpdir/floppyfw-$version.nb vmlinuz initrd.gz");
print "Please clean up by removing $tempdir\n";
