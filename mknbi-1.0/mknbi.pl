#!/usr/bin/perl -w

# Program to create a netboot image for ROM/FreeDOS/DOS/Linux
# Placed under GNU Public License by Ken Yap, April 2000

BEGIN {
	push(@INC, '@@LIBDIR@@');
}

use strict;
use Getopt::Long;
use Socket;

use TruncFD;
use Nbi;

use constant;
use constant DEBUG => 0;

use vars qw($libdir $target $output $module $param $append $rootdir
	$ipaddrs $ramdisk $rdmode $simhd $squashfd $first32);

sub check_file {
	my ($f, $status);

	$status = 1;
	foreach $f (@_) {
		if (!-e $f) {
			print STDERR "$f: file not found\n";
			$status = 0;
		} elsif (!-f $f) {
			print STDERR "$f: not a plain file\n";
			$status = 0;
		} elsif (!-r $f) {
			print STDERR "$f: file not readable\n";
			$status = 0;
		}
	}
	return ($status);
}

sub mknbi_rom {
	my ($format) = @_;
	my ($romdesc);

	$#ARGV >= 0 or die "Usage: progname romimage\n";
	return unless check_file($ARGV[0]);
	$format->add_header('mknbi-rom', 0x9320, 0x1000, 6);
	$romdesc = { file => $ARGV[0],
		segment => 0x1000,
		maxlen => 0x10000,
		id => 16,
		end => 1 };
	$format->add_segment($romdesc);
	$format->dump_segments();
	$format->copy_file($romdesc);
}

sub inet_aton_warn {
	my ($ip);

	print STDERR "Warning: $_[0] cannot be resolved to an IP address\n" unless defined($ip = inet_aton($_[0]));
	return ($ip);
}

sub resolve_names {
	my ($i);

	my ($client, $server, $gateway, $netmask, $hostname) = split(/:/, $_[0], 5);
	unless (defined($hostname)) {
		print STDERR "$_[0]: invalid specification\n";
		return ($_[0]);
	}
	$client = inet_ntoa($i) if defined($i = &inet_aton_warn($client));
	$server = inet_ntoa($i) if defined($i = &inet_aton_warn($server));
	$gateway = inet_ntoa($i) if defined($i = &inet_aton_warn($gateway));
	return (join(':', $client, $server, $gateway, $netmask, $hostname));
}

sub make_paramstring {
	my ($paramsize) = @_;
	my ($string, $nfsroot, $auto);

	# --param= overrides everything
	return ($param) if (defined($param));
	# String substitute various options, should do sanity checks also
	if (!defined($rootdir)) {
		$rootdir = '/dev/nfs';
	} elsif ($rootdir !~ m(^/dev/)) {
		$nfsroot = $rootdir;
		undef($nfsroot) if ($nfsroot eq 'kernel');
		$rootdir = '/dev/nfs';
	}
	if (defined($ipaddrs)) {
		if ($ipaddrs eq 'kernel') {
			undef($ipaddrs);
		} elsif ($ipaddrs ne 'rom') {
			$ipaddrs = &resolve_names($ipaddrs);
		}
	}
	# At the moment, only auto is supported
	$rdmode = 'auto' if (!defined($rdmode));
	die "Ramdisk mode should be one of: auto eom 0xNNNNNNNN (hex address)\n"
		if ($rdmode !~ /^(auto|eom|0x[\da-fA-F]{1,8})$/);
	$string = "auto rw root=$rootdir";
	$string .= " nfsroot=$nfsroot" if (defined($nfsroot));
	$string .= " ip=$ipaddrs" if (defined($ipaddrs));
	$string .= " $append" if (defined($append));
	return ($string);
}

sub mknbi_linux {
	my ($format) = @_;
	my ($setupfile, $kernelfile, $setupdesc);
	my ($paramseg, $paramstring, $bootseg, $block);
	my ($setupseg, $kernelseg, $kernellen, $ramdiskseg, $rdloc);
	my ($setupsects, $flags, $syssize, $swapdev,
		$ramsize, $vidmode, $rootdev, $sig, $ver, $bigker);

	$setupfile = $first32 ? "$libdir/first32.linux" : "$libdir/first.linux";
	$#ARGV >= 0 or die "Usage: progname kernelimage [ramdisk]\n";
	$kernelfile = $ARGV[0];
	return unless check_file($setupfile, $kernelfile);
	if (defined($ramdisk = $ARGV[1])) {
		return unless check_file($ramdisk);
	}
	$format->add_header('mknbi-linux', 0x9320, 0x9220, 0);
	$setupdesc = { file => $setupfile,
		segment => 0x9220,
		maxlen => 4096,
		id => 16 };
	$paramstring = &make_paramstring(512);
	$paramseg = { string => $paramstring,
		segment => 0x9340,
		maxlen => 2048,
		id => 17 };
	$bootseg = { file => $kernelfile,
		segment => 0x9000,
		len => 512,
		maxlen => 512,
		id => 18 };
	$format->peek_file($bootseg, \$block, 512) == 512
		or die "Error reading boot sector of $kernelfile\n";
	(undef, $setupsects, $flags, $syssize, $swapdev, $ramsize, $vidmode,
		$rootdev, $sig) = unpack('a497Cv7', $block);
	if ($sig != 0xAA55) {
		print STDERR "$kernelfile: not a Linux kernel image\n";
		return;
	}
	print STDERR 'setupsects flags syssize swapdev ramsize vidmode rootdev sig', "\n" if (DEBUG);
	print STDERR "$setupsects $flags $syssize $swapdev $ramsize $vidmode $rootdev $sig\n" if (DEBUG);
	$setupseg = { file => $kernelfile,
		segment => 0x9020,
		fromoff => 512,
		len => $setupsects * 512,
		maxlen => 79 * 512,
		id => 19 };
	$format->peek_file($setupseg, \$block, 512) == 512
		or die "Error reading first setup sector of $kernelfile\n";
	(undef, $sig, $ver, undef, undef, undef, undef, undef, $flags) =
		unpack('va4v5C2', $block);
	print STDERR 'sig ver flags', "\n" if (DEBUG);
	print STDERR "$sig $ver $flags\n" if (DEBUG);
	if ($sig ne 'HdrS' or $ver < 0x201) {
		print STDERR "$kernelfile: not a Linux kernel image\n";
		return;
	}
	$bigker = ($flags & 0x1);
	$kernelseg = { file => $ARGV[0],
		segment => $bigker ? 0x10000 : 0x1000,
		maxlen => $bigker ? 15 * 1024 * 1024 : 1024 * 512, 
		fromoff => $setupsects * 512 + 512,
		id => 20,
		end => 1 };
	$ramdiskseg = { file => $ramdisk,
		segment => 0x10000,
		align => 4096,
		id => 21,
		end => 1 };
	$$kernelseg{'end'} = 0 if (defined($ramdisk));
	$format->add_segment($setupdesc);
	$format->add_segment($paramseg);
	$format->add_segment($bootseg);
	$format->add_segment($setupseg);
	$kernellen = $format->add_segment($kernelseg);
	$$ramdiskseg{'segment'} += $kernellen >> 4 if ($bigker);
	# should be 0, 1 or 2 depending on rdmode
	$format->add_segment($ramdiskseg, "\x00") if (defined($ramdisk));
	$format->dump_segments();
	$format->copy_file($setupdesc);
	$format->copy_string($paramseg);
	$format->copy_file($bootseg);
	$format->copy_file($setupseg);
	$format->copy_file($kernelseg);
	$format->copy_file($ramdiskseg) if (defined($ramdisk));
}

sub get_geom{
	my ($file, $block) = @_;
	my ($usedsize, $declsize, $firsttracksize, $geom_string, $fstype);
	my ($secttot, $secttrk, $heads, $bootid, $sig, $cyltot);

	($usedsize = $squashfd ? &TruncFD::truncfd($file) : -s $file) > 0
		or die "Error reading $file\n";
	(undef, $secttot, undef, $secttrk, $heads, undef, $bootid, undef,
		$fstype, $sig) = unpack('a19va3vva8Ca17Z5@510a2', $$block);
	print STDERR "Warning, this doesn't appear to be a DOS boot sector\n"
		if ($sig ne "\x55\xAA");
	if ($simhd) {
		# change MediaDescriptor
		substr($$block, 0x15, 1) = "\xF8";
		# change HiddenSectors
		substr($$block, 0x1c, 4) = pack('V', $secttrk * $heads);
		# change the boot drive
		substr($$block, 0x24, 1) = "\x80";
	}
	$cyltot = $secttot / ($secttrk * $heads);
	$declsize = $secttot * 512;
	$firsttracksize = $secttrk * $heads * 512;
	print STDERR "Warning, used size $usedsize is greater than declared size $declsize\n"
		if ($usedsize > $declsize);
	$geom_string = pack('v3C2', $secttot, $secttrk, $cyltot, $simhd ? 0x80 : 0, 0);
	return ($usedsize, $declsize, $firsttracksize, $geom_string, $fstype);
}

sub mod_geom_string {
	my ($geom_string) = @_;
	my ($secttot, $secttrk, $cyltot, $simhd, $x) = unpack('v3C2', $geom_string);
	$cyltot++;	# for partition table
	return (pack('v3C2', $secttot, $secttrk, $cyltot, $simhd, $x));
}

sub encode_chs {
	my ($c, $h, $s) = @_;

	$s = ($s & 0x3F) | (($c & 0x300) >> 2);
	$c &= 0xFF;
	return ($h, $s, $c);
}

sub make_mbr {
	my ($geom_string, $fstype) = @_;
	my ($heads, $bootsect);
	my ($secttot, $secttrk, $cyltot, $simhd, $x) = unpack('v3C2', $geom_string);

	$cyltot--;
	# $cyltot was incremented in mod_geom_string
	$heads = $secttot / ($secttrk * $cyltot);
	# bootsect is first sector of track 1
	$bootsect = $secttrk * $heads;
	# CHS stupidity:
	# cylinders is 0 based, heads is 0 based, but sectors is 1 based
	# 0x01 for FAT12, 0x04 for FAT16
	return (pack('@446C8V2@510v', 0x80, &encode_chs(1, 0, 1),
		$fstype eq 'FAT12' ? 0x01 : 0x04, &encode_chs($cyltot, $heads - 1, $secttrk),
		$bootsect, $secttot, 0xAA55));
}

sub mknbi_fdos {
	my ($format) = @_;
	my ($setupfile, $bootblock);
	my ($usedsize, $declsize, $firsttracksize, $geom_string, $fstype);
	my ($setupdesc, $kerneldesc, $firsttrackdesc, $bootdesc, $floppydesc);

	$setupfile = "$libdir/first.fdos";
	$#ARGV >= 1 or die "Usage: progname kernel.sys floppyimage\n";
	return unless check_file($setupfile, $ARGV[0], $ARGV[1]);
	$format->add_header('mknbi-fdos', 0x9200, 0x9700, 0);
	$setupdesc = { file => $setupfile,
		segment => 0x9700,
		maxlen => 4096,
		id => 16 };
	$kerneldesc = { file => $ARGV[0],
		segment => @@FDKSEG@@,
		maxlen => 0x40000,
		id => 17 };
	$floppydesc = { file => $ARGV[1],
		segment => 0x11000,
		id => 18,
		end => 1 };
	$format->add_segment($setupdesc);
	$format->add_segment($kerneldesc);
	$format->peek_file($floppydesc, \$bootblock, 512) == 512
		or die "Error reading boot sector of $ARGV[1]\n";
	($usedsize, $declsize, $firsttracksize, $geom_string, $fstype)
		= &get_geom($ARGV[1], \$bootblock);
	$firsttrackdesc = { align => $firsttracksize };
	$$floppydesc{'fromoff'} = 512;
	$$floppydesc{'len'} = $usedsize;
	$$floppydesc{'len'} += $firsttracksize if $simhd;
	$$floppydesc{'maxlen'} = $declsize;
	$geom_string = &mod_geom_string($geom_string) if $simhd;
	$format->add_segment($floppydesc, $geom_string);
	$format->dump_segments();
	$format->copy_file($setupdesc);
	$format->copy_file($kerneldesc);
	if ($simhd) {
		$$firsttrackdesc{'string'} = &make_mbr($geom_string, $fstype);
		$format->copy_string($firsttrackdesc);
	}
	# write out modified bootblock, not the one in the file
	$bootdesc = { string => $bootblock };
	$format->copy_string($bootdesc);
	# Restore correct value of len and account for bootblock skipped
	$$floppydesc{'len'} = $usedsize - 512;
	$format->copy_file($floppydesc);
}

sub mknbi_dos {
	my ($format) = @_;
	my ($setupfile, $bootblock);
	my ($usedsize, $declsize, $firsttracksize, $geom_string, $fstype);
	my ($setupdesc, $firsttrackdesc, $bootdesc, $floppydesc);

	$setupfile = "$libdir/first.dos";
	$#ARGV >= 0 or die "Usage: progname floppyimage\n";
	return unless check_file($setupfile, $ARGV[0]);
	$format->add_header('mknbi-dos', 0x1000, 0x1040, 0);
	$setupdesc = { file => $setupfile,
		segment => 0x1040,
		maxlen => 64512,
		id => 16 };
	$floppydesc = { file => $ARGV[0],
		segment => 0x11000,
		id => 17,
		end => 1 };
	$format->add_segment($setupdesc);
	$format->peek_file($floppydesc, \$bootblock, 512) == 512
		or die "Error reading boot sector of $ARGV[0]\n";
	($usedsize, $declsize, $firsttracksize, $geom_string, $fstype)
		= &get_geom($ARGV[0], \$bootblock);
	$firsttrackdesc = { align => $firsttracksize };
	$$floppydesc{'fromoff'} = 512;
	$$floppydesc{'len'} = $usedsize;
	$$floppydesc{'len'} += $firsttracksize if $simhd;
	$$floppydesc{'maxlen'} = $declsize;
	$geom_string = &mod_geom_string($geom_string) if $simhd;
	$format->add_segment($floppydesc, $geom_string);
	$format->dump_segments();
	$format->copy_file($setupdesc);
	if ($simhd) {
		$$firsttrackdesc{'string'} = &make_mbr($geom_string, $fstype);
		$format->copy_string($firsttrackdesc);
	}
	# write out modified bootblock, not the one in the file
	$bootdesc = { string => $bootblock };
	$format->copy_string($bootdesc);
	# Restore correct value of len and account for bootblock skipped
	$$floppydesc{'len'} = $usedsize - 512;
	$format->copy_file($floppydesc);
}

$libdir = '@@LIBDIR@@';		# where config and auxiliary files are stored

$simhd = 0;
$squashfd = 1;
$first32 = 0;
GetOptions('target=s' => \$target,
	'output=s' => \$output,
	'param=s' => \$param,
	'append=s' => \$append,
	'rootdir=s' => \$rootdir,
	'ipaddrs=s' => \$ipaddrs,
	'rdmode=s' => \$rdmode,
	'harddisk!' => \$simhd,
	'squash!' => \$squashfd,
	'first32!' => \$first32);

$0 =~ /-([a-z]+)$/ and $target = $1;
if (!defined($target)) {
	print STDERR "No target specified with program name or --target=\n";
	exit 1;
}
if (defined($output)) {
	die "$output: $!\n" unless open(STDOUT, ">$output");
}
binmode(STDOUT);

$module = Nbi->new($libdir);
if ($target eq 'rom') {
	&mknbi_rom($module);
} elsif ($target eq 'linux') {
	&mknbi_linux($module);
} elsif ($target eq 'fdos') {
	&mknbi_fdos($module);
} elsif ($target eq 'dos') {
	&mknbi_dos($module);
} else {
	print STDERR "Target $target not supported\n";
}

close(STDOUT);
exit 0;

__END__

=head1 NAME

mknbi - make network bootable image

=head1 SYNOPSIS

B<mknbi> --target=I<target> [--output=I<outputfile>] I<target-specific-arguments>

B<mknbi-linux> [--output=I<outputfile>] I<kernelimage> [I<ramdisk>]

B<mknbi-rom> [--output=I<outputfile>] I<ROM-image>

B<mknbi-fdos> [--output=I<outputfile>] I<kernel.sys floppyimage>

B<mknbi-dos> [--output=I<outputfile>] I<floppyimage>

=head1 DESCRIPTION

B<mknbi> is a program that makes network bootable images for various
operating systems suitable for network loading by Etherboot or Netboot.

B<mknbi> can be invoked with the B<--target> option or links can be made
to it under target specific names.

B<--target>=I<target> Specify the target binary. Currently available are
linux, rom, fdos and dos. B<mknbi> is not needed for booting FreeBSD.

B<--output=>I<outputfile> Specify the output file, can be used with
all variants.  Stdout is the default.

The package must be installed in the destination location before the
executables can be run, because it looks for library files.

Each of the variants will be described separately.

=head1 MKNBI-LINUX

B<mknbi-linux> makes a tagged image from a Linux kernel image, either
a zImage or a bzImage.

=head1 MKNBI-LINUX OPTIONS

B<--param=>I<string> Replace the default parameter string with the
specified one. This option overrides all the following options so you
should know what you are doing.

B<--append>=I<string> Appends the specified string to the existing
parameter string. This option operates after the other parameter options
have been evaluated.

B<--rootdir>=I<rootdir> Define name of directory to mount via NFS from
the boot server.

In the absence of this option, the default is to use the directory
C</tftpboot/>I<%s>, with the I<%s> representing the hostname or
IP-address of the booting system, depending on whether the hostname
attribute is present in the BOOTP/DHCP reply.

If C<rom> is given, and if the BOOTP/DHCP server is able to handle the RFC 1497
extensions, the value of the rootpath option is used as the root directory.

If the name given to the option starts with C</dev/>, the corresponding
device is used as the root device, and no NFS directory will be mounted.

B<--ipaddrs=>I<string> Define client and server IP addresses.

In the absence of this option no IP addresses are defined, and the
kernel should determine the IP addresses by itself, usually by using
RARP, BOOTP or DHCP.  Note that the kernel's query is I<in addition to>
the query made by the bootrom, and requires the IP: kernel level
autoconfiguration (CONFIG_IP_PNP) feature to be included in the kernel.

If C<rom> is given as the argument to this option, all necessary IP
addresses for NFS root mounting will be inherited from the BOOTP/DHCP
answer the bootrom got from the server.

It's also possible to define the addresses during compilation of the boot
image. Then, all addresses must be seperated by a colon, and ordered in
the following way:

C<--ipaddrs=>I<client:server:gateway:netmask:hostname[:dev]>

Using this option B<mknbi-linux> will automatically convert system names
into decimal IP addresses for the first three entries in this string. The
B<hostname> entry will be used by the kernel to set the host name of the
booted Linux diskless client.  When more than one network interface is
installed in the diskless client, it is possible to specify the name of
the interface to use for mounting the root directory via NFS by giving
the optional value C<dev>.  This entry has to start with the string
C<eth> followed by a number from 0 to 9. However, if only one interface
is installed in the client, this C<dev> entry including the preceding
semicolon can be left out.

Run the program thus:

C<mknbi-linux> I<kernel-image> [I<ramdisk-image>] > C<linux.nb>

Then move F<linux.nb> to where the network booting process expects to
find it.

=head1 MKNBI-LINUX BOOTP/DHCP VENDOR TAGS

B<mknbi-linux> includes a startup code at the beginning of the Linux
kernel which is able to detect certain BOOTP vendor defined tags. These
can be used to modify the kernel loading process at runtime. To use
these tags with bootpd, a publicly available BOOTP server daemon, you
can use the following syntax in the F</etc/bootptab> file:

C<T>I<number>C<=">I<string>C<">

For example, to specify a different root NFS device, you can use:

C<T130="eth1">

The following tags are presently supported by B<mknbi-linux>:

B<129> The I<string> value given with this tag is appended verbatim to
the end of the kernel command line.  It can be used to specify arguments
like I/O addresses or DMA channels required for special hardware
like SCSI adapters, network cards etc. Please consult the Linux kernel
documentation about the syntax required by those options. It is the same
as the B<--append> command line option to B<mknbi-linux>, but works at
runtime instead of compile time.

B<130> With this tag it is possible to the select the network adapter
used for mounting root via NFS on a multihomed diskless client. The
syntax for the I<string> value is the same as for the C<dev> entry used
with the B<--ipaddrs=> option as described above. However note that the
B<mknbi-linux> runtime loader does not check the syntax of the string.

The same tags will work in DHCP with the appropriate syntax for your
DHCP server configuration file.

=head1 MKNBI-ROM

B<mknbi-rom> makes a tagged image from an Etherboot C<.rom> or C<.lzrom>
boot ROM image.  This allows it to be netbooted using an existing
ROM. This is useful for developing Etherboot drivers.

Run mknbi like this:

C<mknbi-rom nic.lzrom> > C<nic.nb>

Move F<nic.nb> to where the network booting process expects to find it.
The boot ROM will load this as the I<operating system> and execute the
ROM image.

=head1 MKNBI-FDOS

B<mknbi-fdos> makes a tagged image from a FreeDOS kernel file and a
floppy image.  Note that the kernel image is not read from the floppy
section of the tagged image, but is a separate section in the tagged
image. The bootloader has been adjusted to jump to it directly. This
means the space that would be taken up on the I<floppy> by the kernel
image file can now be used for applications and data.

Obtain a distribution of FreeDOS with a recent kernel, probably at least
2006. It has been tested with 2012 but nothing older. You can get the
FreeDOS kernel here:

C<ftp://ftp.gcfl.net/freedos/kernel/>

Follow the instructions to make a bootable floppy. Then get an image
of the floppy with:

C<dd if=/dev/fd0 of=/tmp/floppyimage>

Also extract F<kernel.sys> from the floppy. You can do this from the
image using the mtools package, by specifying a file as a I<drive>
with a declaration like this in F<~/.mtoolsrc>:

C<drive x: file="/tmp/floppyimage">

Then run:

C<mcopy x:kernel.sys .>

Then run mknbi by:

C<mknbi-fdos kernel.sys /tmp/floppyimage> > C<freedos.nb>

where F<kernel.sys> and F</tmp/floppyimage> are the files extracted above.
Then move F<freedos.nb> to where the network booting process expects to
find it.

If you have got it to netboot successfully, then you can go back and
add your files to the floppy image. You can delete F<kernel.sys> in
the floppy image to save space, that is not needed. Note that you can
create a floppy image of any size you desire with the mformat program
from mtools, you are not restricted to the actual size of the boot floppy.

=head1 MKNBI-FDOS OPTIONS

B<--harddisk> Make the boot ramdisk the first hard disk, i.e. C:. One
reason you might want to do this is because you want to use the real
floppy. The limit on "disk size" in the boot image is not raised by this
option so that is not a reason to use this option.

B<--nosquash> Do not try to chop unused sectors from the end of the
floppy image. This increases the tagged image size and hence loading
time if the FAT filesystem on the floppy is mostly empty but you may
wish to use this option if you have doubts as to whether the squashing
algorithm is working correctly.

=head1 MKNBI-DOS

B<mknbi-dos> makes a tagged image from a floppy image containing a
bootable DOS filesystem.  It is not necessary to build the filesystem
on a physical floppy if you have the mtools package, but you need a
bootable floppy of any size to start with. First extract the boot block
from the floppy:

C<dd if=/dev/fd0 of=bootblock bs=512 count=1>

Then get the DOS kernel files (this is correct for DR-DOS, the names
are different in MS-DOS):

C<mcopy a:ibmbio.com a:ibmdos.com .>

Next make an entry in F<~/.mtoolsrc> to declare a floppy to be mapped
to a file:

C<drive x: file="/tmp/floppyimage">

Now format a floppy of the desired size, in this case a 2.88 MB floppy,
at the same time writing the bootblock onto it:

C<mformat -C -t 160 -s 18 -h 2 -B bootblock x:>

Finally, copy all your desired files onto the floppy:

C<mcopy ibmbio.com ibmdos.com config.sys autoexec.bat app.exe app.dat ... x:>

If you happen to have a media of the same size you could test if the
image is bootable by copying it onto the media, and then booting it:

C<dd if=/tmp/floppyimage of=/dev/fd0>

Then run mknbi-dos over the image F</tmp/floppyimage> to create a
tagged image:

C<mknbi-dos /tmp/floppyimage> > C<dos.nb>

Move F<dos.nb> to where the network booting process expects to find it.

=head1 MKNBI-DOS OPTIONS

B<--harddisk> Make the boot ramdisk the first hard disk, i.e. C:. One
reason you might want to do this is because you want to use the real
floppy. The limit on "disk size" in the boot image is not raised by this
option so that is not a reason to use this option.

B<--nosquash> Do not try to chop unused sectors from the end of the
floppy image. This increases the tagged image size and hence loading
time if the FAT filesystem on the floppy is mostly empty but you may
wish to use this option if you have doubts as to whether the squashing
algorithm is working correctly.

=head1 BUGS

Please report all bugs to the author.

=head1 SEE ALSO

Etherboot tutorial at C<http://etherboot.sourceforge.net/> Mtools package
is at C<http://wauug.erols.com/pub/knaff/mtools/> Make sure you have a
recent version, the ability to map a drive to a file is not present in
old versions.

=head1 COPYRIGHT

B<mknbi> is under the GNU Public License

=head1 AUTHOR

Ken Yap (C<ken_yap@users.sourceforge.net>)

=head1 DATE

Version 1.0 April 2000
