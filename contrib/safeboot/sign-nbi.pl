#!/usr/bin/perl -w
# sign-nbi.pl
my ($a, $b, $c, $i);
$a = shift; $b = shift; $c = ""; $i = 0;
if ( ! defined ( $a ) ) { $a = ""; }
if ( ! defined ( $b ) ) { $b = ""; }
print "sign-nbi.pl (C) 2003 Anselm Martin Hoffmeister\nNo guarantees, free usage, GPL\n";
if ( $a =~ /gen/i ) {
  print "Key generation with openssl, 512 bit rsa, public exponent Fermat-4:\n";
  open ( OSG, "openssl genrsa -f4|" ) or die "OpenSSL error";
  umask 0177;
  open ( KEY, ">.KEY" ) or die "Cannot write KEY file";
  while ( <OSG> ) {
  	( $a ) = split ( /\n/, $_ );
	print KEY $_;
  }
  close ( KEY ); close ( OSG );
  umask 022;
  print "Finished\n";
} elsif ( $a =~ /code/i ) {
  print "Enter KEY into Etherboot sourcecode:\n";
  open ( SOUR, ">include/safeboot_key.h" ) or die "Cannot write include/safeboot_key.h";
  # 129 bytes for public key
  open ( RIN, "openssl rsa -modulus -in .KEY -noout |" ) or die "Cannot read .KEY";
  $a = <RIN>;
  ( $a, $a ) = split ( /=/, $a );
  ( $a ) = split ( /\n/, $a );
  $i = length ( $a );	# MAX RSA MODULUS LEN is 128 bytes
  for ( $i = length ( $a ) ; ( $i = length ( $a ) ) < 256; ) { $a = "00".$a; }
  print SOUR "char safemode_pubkey_m[] = { ";
  for ( $i = 0; $i < 127; ++$i ) {
  	print SOUR "0x".substr($a, $i * 2, 2 ).", ";
  }	print SOUR "0x".substr($a, 254   , 2 ). "};\n";
  print SOUR "char safemode_pubkey_d[] = { ";
  for ( $i = 0; $i < 125; ++ $i ) {
  	print SOUR "0x00, ";
  }
  print SOUR "01, 00, 01};\n";
  close ( SOUR );
  print "Done.\n";
} elsif ( $a =~ /sign/i ) {
  print "Sign NBI / Network Bootable Image with KEY:\n";
  # Therefore, generate /tmp/sign-nbi.tmp with the correct bit structure
  # Sign this and concatenate
  ( $b ) = split ( /\ /, $b );
  ( $b ) = split ( /;/, $b );
  if ( $b =~ /^$/ ) { die "Filename must be specified\n"; }
  system ( "dd if=$b of=/tmp/sign-nbi.tmp-0 bs=446 count=1" );
  system ( "dd if=$b of=/tmp/sign-nbi.tmp-2 bs=1 skip=510" );
  system ( "dd if=/dev/zero of=/tmp/sign-nbi.tmp-1 bs=64 count=1" );
  system ( "cat /tmp/sign-nbi.tmp-0 /tmp/sign-nbi.tmp-1 /tmp/sign-nbi.tmp-2 > /tmp/sign-nbi.tmp-3" );
  open ( MDSUM, "md5sum < /tmp/sign-nbi.tmp-3 |" ) or die "Cannot generate MD5-Sum";
  while ( <MDSUM> ) { $a = $_; }
  close ( MDSUM );
  # convert checksum into binary data and sign that binary stuff
  open ( TMPO, ">/tmp/sign-nbi.tmp-4" ) or die "Cannot write temp file";
  ( $a ) = split ( /\n/, $a );
  ( $a ) = split ( /\ /, $a );
  print "MD5-Checksum is [$a]\n"; $c = "";
  for ( $i = 0; $i < 16; ++$i ) {
  	$c = $c.sprintf("%c",hex(substr($a, $i*2, 2 )));
  }
  syswrite(TMPO, $c);
  close ( TMPO );
  system ( "openssl rsautl -in /tmp/sign-nbi.tmp-4 -out /tmp/sign-nbi.tmp-1 -inkey .KEY -pkcs -sign" );
  system ( "cat /tmp/sign-nbi.tmp-0 /tmp/sign-nbi.tmp-1 /tmp/sign-nbi.tmp-2 > $b" );
  print "Done.\n";
} else {
print << "EOF1"
This program has three modes of usage:
==> Key generation ( sign-nbi.pl gen )
  generate a 512 bit rsa key and store it to ".KEY"
==> Public Key submission ( sign-nbi.pl code )
  enters the public KEY into etherboot sources (include/safeboot_key.h)
==> NBI signing ( sign-nbi.pl sign FILE )
  reads KEY and stores the file signature into the NBI headers
EOF1
}


