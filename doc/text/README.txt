  Etherboot README
  Markus Gutschke and Ken Yap, markus+etherboot@gutschke.com,
  ken_yap@users.sourceforge.net
  30 Nov 2000

  This is the README file for the Etherboot package. This document
  explains how to install, configure and use the Etherboot package.  The
  instructions here apply to version 4.6 of Etherboot.
  ______________________________________________________________________

  Table of Contents























































  1. Introduction

     1.1 What hardware is supported?
     1.2 Availability of this document
     1.3 Getting help

  2. Unpacking, compiling and testing the package

     2.1 Unpacking the distribution
     2.2 Compiling the ROM images
     2.3 Testing the ROM images

  3. Setting up a diskless boot

     3.1 Making a tagged image
     3.2 Compiling a custom kernel
     3.3 Setting up a bootp daemon
     3.4 Setting up a DHCP daemon
     3.5 Setting up a tftp daemon

  4. Testing the network booting

     4.1 Setting up a NFS root filesystem
     4.2 Initial ramdisk
     4.3 Swap over NFS

  5. Booting DOS

  6. Making an Etherboot EPROM or EEPROM

     6.1 Choosing the EPROM
     6.2 Enabling the EPROM
     6.3 Size and speed of the EPROM

  7. Troubleshooting tips

  8. Frequently Answered Questions

     8.1 Building Etherboot
     8.2 Testing Etherboot
     8.3 Hardware capabilities
     8.4 Booting Linux
     8.5 Running X
     8.6 Other client applications
     8.7 Booting FreeBSD
     8.8 Booting other operating systems (DOS, Windows)
     8.9 Hardware issues
     8.10 Drivers
     8.11 Miscellaneous

  9. Writing an Etherboot Driver

  10. Acknowledgements (a.k.a Hall of Fame)

  11. Copyright



  ______________________________________________________________________

  11..  IInnttrroodduuccttiioonn


  Etherboot is a package for creating ROM images that can download code
  over the network to be executed on an x86 computer. Etherboot requires
  the PC architecture, it does not work for other Linux platforms such
  as Alphas or Suns. Typically the computer is diskless and the code is
  Linux or FreeBSD, but these are not the only possibilities. The code
  uses the bootp, tftp and NFS Internet Protocols.



  Etherboot is Open Source. Please support it by joining the Open Source
  community and sharing. See the Etherboot home page
  <http://etherboot.sourceforge.net/> for some ways you can help
  Etherboot.



  _D_i_s_c_l_a_i_m_e_r_: _E_t_h_e_r_b_o_o_t _c_o_m_e_s _w_i_t_h _N_O _w_a_r_r_a_n_t_i_e_s _o_f _a_n_y _k_i_n_d_. _I_t _i_s
  _h_o_p_e_d _t_h_a_t _i_t _w_i_l_l _b_e _u_s_e_f_u_l _t_o _y_o_u_, _a_n_d _N_O _r_e_s_p_o_n_s_i_b_i_l_i_t_y _i_s _a_c_c_e_p_t_e_d
  _f_o_r _a_n_y _o_u_t_c_o_m_e _o_f _u_s_i_n_g _i_t_. _E_t_h_e_r_b_o_o_t _a_l_s_o _c_o_m_e_s _w_i_t_h _N_O _s_u_p_p_o_r_t_,
  _a_l_t_h_o_u_g_h _y_o_u _c_a_n _u_s_u_a_l_l_y _g_e_t _h_e_l_p_f_u_l _a_d_v_i_c_e _f_r_o_m _t_h_e _m_a_i_l_i_n_g _l_i_s_t_s
  _l_i_s_t_e_d _o_n _t_h_e _E_t_h_e_r_b_o_o_t _h_o_m_e _p_a_g_e_.


  11..11..  WWhhaatt hhaarrddwwaarree iiss ssuuppppoorrtteedd??


  The following is the current NIC configuration file as of 2000-11-29.

  Even if your NIC does not appear in the list, it may still be
  supported if the chip is one of those supported.  Many OEMs use chips
  from foundries. An exhaustive list of brand names is impossible.  The
  best strategy is to read the number on the LAN controller chip on the
  board.




































  ______________________________________________________________________
  # This is the config file for creating Makefile rules for Etherboot ROMs
  #
  # To make a ROM for a supported NIC add a line of the form
  #
  # ROM           Driver-name     PCI-IDs
  #
  # ROM is the desired output name for both .rom and .lzrom images.
  # Driver name is the driver it uses (can be omitted if the same as ROM).
  # PCI IDs are the PCI vendor and device IDs of the PCI NIC
  #   (leave blank for ISA NICs).
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
  # Send additions to this file to <ken_yap@users.sourceforge.net>
  #
  # 3Com503, aka Etherlink II, also /16 model
  3c503           ns8390
  # 3Com507 (i82586 based)
  3c507           i82586
  # 3c509, ISA/EISA
  3c509
  # 3c529 == MCA 3c509
  3c529           3c509
  # 3c59x cards (Vortex)
  3c590           3c595           0x10b7,0x5900
  3c595           3c595           0x10b7,0x5950
  3c595-1         3c595           0x10b7,0x5951
  3c595-2         3c595           0x10b7,0x5952
  # 3C90x cards device IDs
  # Original 90x revisions:
  #   0x9000 : 10 Base TPO
  #   0x9001 : 10/100 T4
  #   0x9050 : 10/100 TPO
  #   0x9051 : 10 Base Combo
  # Newer 90xB revisions:
  #   0x9004 : 10 Base TPO
  #   0x9005 : 10 Base Combo
  #   0x9006 : 10 Base TP and Base2
  #   0x900A : 10 Base FL
  #   0x9055 : 10/100 TPO
  #   0x9056 : 10/100 T4
  #   0x905A : 10 Base FX
  # Newer 90xC revision:
  #   0x9200 : 10/100 TPO (3C905C-TXM)
  3c905-tpo       3c90x           0x10b7,0x9000
  3c905-t4        3c90x           0x10b7,0x9001
  3c905-tpo100    3c90x           0x10b7,0x9050
  3c905-combo     3c90x           0x10b7,0x9051
  3c905b-tpo      3c90x           0x10b7,0x9004
  3c905b-combo    3c90x           0x10b7,0x9005
  3c905b-tpb2     3c90x           0x10b7,0x9006
  3c905b-fl       3c90x           0x10b7,0x900a
  3c905b-tpo100   3c90x           0x10b7,0x9055
  3c905b-t4       3c90x           0x10b7,0x9056
  3c905b-fx       3c90x           0x10b7,0x905a
  3c905c-tpo      3c90x           0x10b7,0x9200
  # Crystal Semiconductor CS89x0
  cs89x0
  # Digital DE100 and DE200
  depca           depca
  # Intel Etherexpress Pro/10 (ISA card)
  eepro           eepro
  # Intel Etherexpress Pro/100
  eepro100        eepro100        0x8086,0x1229
  82559er         eepro100        0x8086,0x1209
  # SMC 83c170 EPIC/100
  epic100         epic100         0x10b8,0x0005
  # Exos 205 (i82586 based)
  exos205         i82586
  # Lance PCI PCNet/32
  lancepci        lance           0x1022,0x2000
  # Linksys LNE100TX and other NICs using this Tulip clone chip
  lc82c115        tulip           0x11ad,0xc115
  # Netgear FA310TX and other NICs using this Tulip clone chip
  lc82c168        tulip           0x11ad,0x0002
  # Tulip clones based on the ADMtek Centaur-P
  admtek0985      tulip           0x1317,0x0985
  # Reported by Ranjan Parthasarathy
  stmicro         tulip           0x1317,0x0981
  # Tulip clones based on the Macronix 987x5
  mx987x5         tulip           0x10d9,0x0531
  # Davicom DM9102
  davicom9102     davicom         0x1282,0x9102
  davicom9009     davicom         0x1282,0x9009
  # NE1000/2000 and clones (ISA)
  ne              ns8390
  # Novell NE2100 (Lance based, also works on NE1500)
  ne2100          lance
  # NE2000 PCI clone (RTL8029)
  nepci           ns8390          0x10ec,0x8029
  # Winbond 86C940
  winbond940      ns8390          0x1050,0x0940
  # Compex RL2000
  compexrl2000    ns8390          0x11f6,0x1401
  # KTI ET32P2
  ktiet32p2       ns8390          0x8e2e,0x3000
  # NetVin 5000SC
  nv5000sc        ns8390          0x4a14,0x5000
  # Racal-Interlan NI5010
  ni5010          ni5010
  # Racal-Interlan NI5210
  ni5210          i82586
  # Racal-Interlan NI6510 (Lance based)
  ni6510          lance
  # Base driver for Tulip clones
  tulip           tulip           0x1011,0x0002
  # Tulip-Fast
  tulipfast       tulip           0x1011,0x0009
  # Tulip+
  tulip+          tulip           0x1011,0x0014
  # Tulip 21142
  tulip21142      tulip           0x1011,0x0019
  # Realtek 8029 (NE2000 PCI clone)
  rtl8029         ns8390          0x10ec,0x8029
  # Realtek 8139
  rtl8139         rtl8139         0x10ec,0x8139
  # SMC1211 (uses Realtek 8139 but with different IDs)
  smc1211         rtl8139         0x1112,0x1211
  # Schneider and Koch G16
  sk_g16
  # SMC9000
  smc9000
  # Tiara, Fujitsu Lancard
  tiara
  # Old base driver for Tulip clones
  otulip          otulip          0x1011,0x0002
  # Rhine-I, e.g. D-Link DFE-530TX
  dlink-530tx     via-rhine       0x1106,0x3043
  # Rhine-II
  via-rhine       via-rhine       0x1106,0x6100
  # WD8003/8013, SMC8216/8416
  wd              ns8390
  # Winbond W89c840
  winbond840      w89c840         0x1050,0x0840
  # Compex RL100-ATX
  compexrl100atx  w89c840         0x11f6,0x2011
  ______________________________________________________________________





  All Etherboot drivers are autoprobing, which means they attempt to
  detect the hardware addresses at which the card is installed. It's
  fairly easy to write a driver if you know C and are familiar with
  Ethernet hardware interfacing. Please read the section on ``Writing an
  Etherboot Driver'' if you wish to do so.


  11..22..  AAvvaaiillaabbiilliittyy ooff tthhiiss ddooccuummeenntt


  This document and related documents are also kept online at the
  Etherboot Home Page <http://etherboot.sourceforge.net/>.  This will in
  general have the latest distributions and documentation.



  The contents of this document are copyright Ken Yap and Markus
  Gutschke.  However permission is granted to copy and reproduce this
  document in full or in part provided this document is identified as
  the source.



  For a talk/tutorial type introduction to what Etherboot does and how
  to set it up, see my SLUG talk <diskless.html>. You may wish to review
  this before reading further.



  An older version of this README, which is now out of date, can be
  accessed via this link <old-README.html>.


  11..33..  GGeettttiinngg hheellpp


  There are mailing lists for all Etherboot related issues. To subscribe
  follow the instructions on the Etherboot home page
  <http://etherboot.sourceforge.net/>.


  22..  UUnnppaacckkiinngg,, ccoommppiilliinngg aanndd tteessttiinngg tthhee ppaacckkaaggee



  22..11..  UUnnppaacckkiinngg tthhee ddiissttrriibbuuttiioonn


  Unpack the distribution using gunzip and tar, using one of the
  following commands, where you replace x by the patchlevel number:




  ______________________________________________________________________
          tar zxvf etherboot-4.6.x.tar.gz
          gunzip < etherboot-4.6.x.tar.gz | tar xvf -
          bunzip2 < etherboot-4.6.x.tar.bz2 | tar xvf -
  ______________________________________________________________________




  22..22..  CCoommppiilliinngg tthhee RROOMM iimmaaggeess


  To build the ROM images you need a recent release of gcc and the
  binutils tools. This package was compiled with the tools from a SuSE
  7.0 distribution but it should work with any recent Linux or FreeBSD
  distribution. For the 16 bit version you need the bcc tools from the
  Embedded Linux Kernel Subset (ELKS) project, for more details see the
  notes on 16 bit Etherboot <16.html>. You need the 16 bit version only
  if you intend to run Etherboot on a 286 or 086/088 PC.



  You probably want to make the 32 bit version. You only have to go to
  src/, edit the options in Config and say make. We suggest you accept
  the default options if you are not sure what to select.  This will
  create all the ROM images available in bin32. The .lzrom images are
  the same as the .rom images. Since the .lzrom images are smaller and
  work exactly the same, there is no real reason to use .rom images any
  more, unless you are nervous about compression algorithm patents. We
  believe the algorithm used does not infringe patents, having been in
  public use for some time, but we do not know all the legal
  ramifications. See here <COPYING_compressor.html> for more details.



  Here is a brief description of the options available:





















  ______________________________________________________________________
  Basic options:

  -DNO_DHCP_SUPPORT-Use BOOTP instead of DHCP
  -DRARP_NOT_BOOTP- Use RARP instead of BOOTP/DHCP
  -DIMAGE_MENU    - Allow to interactively chose between different
                    bootimages; read vendortags.html for further
                    information.
  -DMOTD          - Display message of the day; read vendortags.html
                    for further information.
  -DASK_BOOT=n    - Ask "Boot from Network or from Local? " at startup,
                    timeout after n seconds (0 = no timeout); this
                    can be done in a more generic way by using the
                    IMAGE_MENU, but it requires that the "bootp"
                    server is accessible, even when booting locally.
  -DANS_DEFAULT=ANS_NETWORK
                  - Assume Network to previous question
                    (alternative: ANS_LOCAL) on timeout or Return key
                    See etherboot.h for prompt and answer strings.
  -DEMERGENCYDISKBOOT
                  - if no BOOTP server can be found, then boot from
                    local disk. The accessibility of the TFTP server
                    has no effect, though! So configure your BOOTP
                    server properly. You should probably reduce
                    MAX_BOOTP_RETRIES to a small number like 3.
  -DNOINT19H      - Take control as soon as BIOS detects the ROM
                    Normally hooks onto INT19H
  -DMOVEROM       - if your motherboard does not cache adapter memory
                    space, then this option can speed up loading of
                    compressed BOOT-Prom images. It has not affect on
                    uncompressed images. Unless you are very tight on
                    free space, you will usually want to define this
                    option.  This flag must be added to LCONFIG!
  -DDELIMITERLINES - print a line of = characters at the start
                    and also just before starting an image.
  -DSIZEINDICATOR - update a running total of the amount of code
                    loaded so far, in kilobytes
  -DT509HACK      - send two bootp packets before waiting for a
                    reply to the first. Makes a 3c509 do bootp
                    quicker
  -DT503_AUI      - Use AUI by default on 3c503 cards.
  -DCONGESTED     - turns on packet retransmission.  Use it on a
                    congested network, where the normal operation
                    can't boot the image.
  -DBACKOFF_LIMIT - sets the maximum RFC951 backoff exponent to n.
                    Do not set this unreasonably low, because on networks
                    with many machines they can saturate the link
                    (the delay corresponding to the exponent is a random
                    time in the range 0..3.5*2^n seconds).  Use 5 for a
                    VERY small network (max. 2 minutes delay), 7 for a
                    medium sized network (max. 7.5 minutes delay) or 10
                    for a really huge network with many clients, frequent
                    congestions (max. 1  hour delay).  On average the
                    delay time will be half the maximum value.  If in
                    doubt about the consequences, use a larger value.
                    Also keep in mind that the number of retransmissions
                    is not changed by this setting, so the default of 20
                    may no longer be appropriate.  You might need to set
                    MAX_ARP_RETRIES, MAX_BOOTP_RETRIES, MAX_TFTP_RETRIES
                    and MAX_RPC_RETRIES to a larger value.

  Etherboot/32 only options:

  -DAOUT_IMAGE    - Add a.out kernel boot support (generic)
  -DELF_IMAGE     - Add ELF kernel boot support (generic)
  -DIMAGE_MULTIBOOT - Add Multiboot image support (currently only
                    for ELF images)
  -DIMAGE_FREEBSD - Add FreeBSD image loading support (requires at least
                    -DAOUT_IMAGE and/or -DELF_IMAGE)
  -DCONSOLE_CRT   - set for CRT console (default if nothing else is set)
  -DCONSOLE_SERIAL- set for serial console.
  -DCONSOLE_DUAL  - set for CRT and serial console, see comment at
                    -DANSIESC and -DGFX
  -DCOMCONSOLE    - set port, e.g. 0x378
  -DCONSPEED      - set speed, e.g. 57600
  -DCOMPARM       - set Line Control Register value for data bits, stop
                    bits and parity. See a National Semiconditor 8250/
                    16450/16550 data sheet for bit meanings.
                    If undefined, defaults to 0x03 = 8N1.
  -DPASSWD        - enable password protection for boot images; this
                    requires -DIMAGE_MENU
  -DUSRPARMS      - allow the user to interactively edit parameters
                    that are passed to the booted kernel; you should
                    probably enable -DPASSWD as well; this feature
                    requires -DIMAGE_MENU
  -DANSIESC       - evaluate a subset of common ANSI escape sequences
                    when displaying the message of the day; this
                    probably does not make sense unless you also
                    define -DMOTD or at least -DIMAGE_MENU. It is
                    possible to combine this option with -DCONSOLE_DUAL,
                    but you have to be aware that the boot menu will
                    no longer use ANSI escapes to be compatible with the
                    serial console. Also be careful with your banners, as
                    they may confuse your serial console.  Gererally you
                    lose most of the ANSIESC functionality.
  -DGFX           - support extensions to the ANSI escape sequences for
                    displaying graphics (icons or logos); this
                    requires -DANSIESC. It probably does not make sense
                    to use -DGFX if you have -DCONSOLE_DUAL, as the
                    serial console normally cannot handle the GFX stuff.
  -DFLOPPY        - boot from floppy/hd if bootimage matches the
                    pattern "/dev/[fh]d*"; if you do not have
                    enough space in the EPROM, then disable this
                    feature and use "mknbi-blkdev" for booting
                    from a local blockdevice.
  -DTRY_FLOPPY_FIRST
                  - If > 0, tries that many times to read the boot
                    sector from a floppy drive before booting from
                    ROM. If successful, does a local boot.
                    It assumes the floppy is bootable. Requires -DFLOPPY.
  -DCONFIG_PCI_DIRECT
                  - define this for PCI BIOSes that do not implement
                    BIOS32 or not correctly
  -DINTERNAL_BOOTP_DATA
                  - define if the area 0x93C00-0x93FFF is not available
                    for use for bootpd_data by the loader for some reason

  -DNO_DHCP_SUPPORT-Use BOOTP instead of DHCP
  -DIMAGE_MENU    - Allow to interactively chose between different
                    bootimages; read vendortags.html for further
                    information.
  -DMOTD          - Display message of the day; read vendortags.html
                    for further information.
  -DASK_BOOT=n    - Ask "Boot from Network or from Local? " at startup,
                    timeout after n seconds (0 = no timeout); this
                    can be done in a more generic way by using the
                    IMAGE_MENU, but it requires that the "bootp"
                    server is accessible, even when booting locally.
  -DANS_DEFAULT=ANS_NETWORK
                  - Assume Network to previous question
                    (alternative: ANS_LOCAL) on timeout or Return key
                    See etherboot.h for prompt and answer strings.
  -DEMERGENCYDISKBOOT
                  - if no BOOTP server can be found, then boot from
                    local disk. The accessibility of the TFTP server
                    has no effect, though! So configure your BOOTP
                    server properly.  You should probably reduce
                    MAX_BOOTP_RETRIES to a small number like 3.

                    pattern "/dev/[fh]d*"; if you do not have
                    enough space in the EPROM, then disable this
                    feature and use "mknbi-blkdev" for booting
                    from a local blockdevice.
  -DCONFIG_PCI_DIRECT
                  - define this for PCI BIOSes that do not implement
                    BIOS32 or not correctly
  -DINTERNAL_BOOTP_DATA
                  - define if the area 0x93C00-0x93FFF is not available
                    for use for bootpd_data by the loader for some reason
  ______________________________________________________________________





  If you find you need to adjust the PCI vendor and device IDs for PCI
  NICs, add the appropriate line to the file NIC (and send me a copy).
  If you do not set the IDs correctly, the floppy version will work, but
  the ROM will not.  The PCI IDs are usually displayed by the BIOS on
  booting up. They can also be read out from a running Linux system
  using the Linux PCI Utilities
  <http://atrey.karlin.mff.cuni.cz/~mj/pciutils.html>.


  22..33..  TTeessttiinngg tthhee RROOMM iimmaaggeess


  You can test the image with a floppy before programming an EPROM. On
  Linux just put a blank floppy in fd0 and say make bin32/card.fd0 where
  card is the name of your network card and it will copy a bootable
  image onto the floppy. If you wish to do this by hand, it's easy, just
  make floppyload.bin.pre and prepend floppyload.bin.pre to card.rom (or
  card.lzrom) and write this combined binary to the floppy raw, i.e.
  starting at the boot block. Like this:




  ______________________________________________________________________
          cat floppyload.bin.pre 3c509.lzrom > /dev/fd0
  ______________________________________________________________________



  Make sure the floppy has no bad blocks. It is best if it has been
  formatted just before use. You do not need to put any kind of
  filesystem on it. If you wish, you could substitute /dev/fd0 with the
  actual device suitable for the floppy size you are using, for example
  /dev/fd0H1440 for 1.44 MB floppies. This may be more reliable than
  using the autodetecting device /dev/fd0.



  When you boot with this floppy it will load the Etherboot ROM image
  from floppy and execute it. If you chose the correct ROM image, it
  should be able to detect your card. To get the bootrom to acquire an
  IP address and load the intended code, you need to set up bootp, tftp
  and NFS services, which we will discuss next.
  We suggest you continue to use floppy booting until you have completed
  the setup of the server and are satisfied that diskless booting works.


  33..  SSeettttiinngg uupp aa ddiisskklleessss bboooott


  In this section I assume you want to boot a Linux kernel. Booting a
  FreeBSD kernel is documented elsewhere and does not require a tagged
  image.  Booting a DOS kernel is similar, the main differences being in
  the way you set up the tagged image.


  33..11..  MMaakkiinngg aa ttaaggggeedd iimmaaggee


  Etherboot expects to download a tagged image <spec.html> containing
  the code to be executed. Briefly explained, a tagged image is a
  wrapper around the pieces of code or data that need to be put in
  various places in the computer's memory. It contains a directory
  telling how large the pieces are and where they go in memory. It also
  says where to start execution.



  A tagged image is created using a utility program. The utility program
  <mknbi.html> is specific to the kernel you want to load. The version
  for Linux is called mknbi-linux and that for DOS is mknbi-dos. These
  utilities are found in the mknbi-<version> directory of the
  distribution.


  33..22..  CCoommppiilliinngg aa ccuussttoomm kkeerrnneell


  You will almost certainly have to compile a custom kernel because the
  kernel needs to have the "Root file system on NFS" option compiled in.
  (You need to select "NFS filesystem support" as built-in, not a
  module, and possibly also "Kernel level autoconfiguration" before this
  option will appear.)  You should also select "BOOTP support" and/or
  "DHCP support".  "RARP support" is not needed.  In 2.2 kernels you
  have to enable the "Kernel level autoconfiguration" option under IP
  networking to access the BOOTP support question.  And unless you are
  using an initrd (initial ramdisk) you will have to compile in the
  driver for your network card too. For details, see the file
  /usr/src/linux/Documentation/nfsroot.txt in a Linux kernel source
  distribution.



  After you have compiled the custom kernel, make the tagged image,
  typically like this:


  ______________________________________________________________________
          mknbi-linux --output=/tftpdir/vmlinuz.xterm zImage
  ______________________________________________________________________



  Then put the tagged image in where the tftp daemon expects to find it,
  in this example /tftpdir. Make sure it is world-readable because
  typically the tftp daemon runs as an unprivileged user. It is
  recommended that you set a path explicitly for tftpd instead of
  relying on any defaults. For example:

  ______________________________________________________________________
  tftp    dgram   udp     wait    root    /usr/sbin/tcpd  in.tftpd /tftpdir
  ______________________________________________________________________




  33..33..  SSeettttiinngg uupp aa bboooottpp ddaaeemmoonn


  Now set up a bootp daemon. This means installing the bootp package,
  making sure that the bootps service is active in /etc/inetd.conf; it
  should look something like this:


  ______________________________________________________________________
  bootps  dgram   udp     wait    root    /usr/sbin/tcpd  bootpd
  ______________________________________________________________________



  Then you need to edit /etc/bootptab. The essential pieces of
  information you need to put in bootptab are:


  1. The domain name of the machine.

  2. The Ethernet (MAC) address of the network card, which you generally
     obtain from a sticker on the card, a configuration program for the
     card, or in the last resort, from watching the output of Etherboot
     or from the packets sent from the card when trying to boot, using
     the debug option of bootpd.

  3. The name of the tagged image file, relative to the tftpdir
     directory.

  4. The IP address you intend to give it.

  5. The IP addresses of various servers. You will need at least the
     tftp server's address.



  Here is an example of a /etc/bootptab for the bootpd supplied many
  with Linux distributions and probably many versions of Unix:


  ______________________________________________________________________
  .default:\
          :ht=ethernet:\
          :hd=/tftpdir:bf=null:\
          :ds=nameserver:\
          :hn:to=36000:
  xterm.ken.net.au:tc=.default:ha=08002BB7F380:ip=192.168.26.100:bf=vmlinuz.xterm
  ______________________________________________________________________



  The first entry sets up some common defaults which applies to all
  succeeding entries which can be "included" using the tc=.default
  attribute. The first field is the domain name of the machine. The ha
  attribute is the Ethernet address.  The ip attribute is self-
  explanatory. The bf field specifies the tagged image filename. For
  more details, consult the bootptab man page.


  Please note that if you use the ef (extension file) attribute to be
  able to send more configuration data to the diskless machine, you must
  run bootpef everytime bootptab is modified.


  33..44..  SSeettttiinngg uupp aa DDHHCCPP ddaaeemmoonn


  As an alternative to bootp, you could set up a DHCP server which has
  the advantage of automating the handing out of IP addresses.  In
  recent Linux distributions bootpd is not supplied and DHCPD is
  preferred.  However the kernel will still do a bootp request to find
  the IP address for mounting the NFS filesystem. The exceptions to the
  last sentence are recent kernels in the 2.2 series (2.2.15 or 2.2.16
  onwards), which can autoconfigure using DHCP thanks to code by Chip
  Salzenberg. It is rumoured that this feature will not be ported to 2.4
  kernels because DHCP autoconfig is a userland issue. You may perhaps
  wish to investigate the --rootdir=rom option in mknbi-linux
  <mknbi.html> which tells the kernel to take the address from the
  initial DHCP reply, or to use initrds in conjunction with a userland
  DHCP client program to configure the netbooted machine.



  This is the roughly equivalent dhcpd.conf file to the above bootptab:


  ______________________________________________________________________
  option domain-name "ken.net.au";
  option domain-name-servers 192.168.26.1;
  option broadcast-address 192.168.26.255;
  use-host-decl-names on;
  subnet 192.168.26.0 netmask 255.255.255.0 {
          filename "/tftpdir/vmlinuz.xterm";
          host xterm {
                  hardware ethernet 08:00:2B:B7:F3:80;
                  fixed-address xterm.ken.net.au;
                  filename "/tftpdir/vmlinuz.xterm";
          }
  }
  ______________________________________________________________________



  You don't have to use fixed addresses, of course, but if you use
  dynamic addresses, then you have to deal with the resulting issues of
  NFS mounting.



  More information about DHCP can be found at the DHCP FAQ Web Page
  <http://web.syr.edu/~jmwobus/comfaqs/dhcp.faq.html>.



  If you are on a local network that is not directly connected to the
  Internet, you can use the "private" IP addresses 192.168.x.y (or in
  the other ranges mentioned in RFC1918
  <http://ds.internic.net/rfc/rfc1918.txt>). Otherwise please ask either
  your network administrator or your Internet service provider for your
  own IP address(es).





  33..55..  SSeettttiinngg uupp aa ttffttpp ddaaeemmoonn


  Now set up a tftp daemon. This means installing the tftp package and
  making sure that the tftp service is active in /etc/inetd.conf. If you
  want to be very careful You may wish to use the secure (-s) option of
  tftpd, this chroots to the specified directory, but then your
  pathnames in bootptab or dhcpd.conf must be relative to the new root
  directory.



  If you are booting many clients you should be aware of the limitations
  of running tftpd from inetd. Typically inetd has limits on how often a
  daemon can be spawned, to detect runaway daemons. If many clients
  request the tftp service within a short period, inetd may shutdown
  that service.  If you have a setup where there are many clients, it
  may be better to use an improved replacement for inetd, such as
  xinetd.  Another solution is to run a dedicated tftpd that is not
  spawned from inetd. One such tftpd can be found here at:
  ftp://nilo.on.openprojects.net/pub/nilo/snapshots


  44..  TTeessttiinngg tthhee nneettwwoorrkk bboooottiinngg


  Now when you start up Etherboot, it should obtain an IP address and
  print out what it received. If you do not get this to work, turn on
  debugging in bootpd and see if any query was received. You may also
  wish to use the tcpdump or ethereal utilities to watch the network for
  bootp packets (port bootps).  If not, check your network hardware
  (cables, etc). If a query was received, check if bootpd/dhcpd was able
  to give an answer. If not, then the Ethernet address was not found in
  /etc/bootptab or /etc/dhcp.conf. If a reply was sent, then only faulty
  hardware or a bug in Etherboot would prevent it being received by
  Etherboot.



  Assuming an IP address was received, the next thing Etherboot tries to
  do is load a file using tftp. Check your system logs to see if a tftp
  daemon was started up and a file requested. Generally if you run tftpd
  under tcpwrapper security, a log entry will be generated. If not, it
  could be a path problem or file permission problem (the file needs to
  be readable by tftpd). Another problem could be that tftpd needs to
  reverse map the IP address to a name for security checking, and you
  don't have the client's details in /etc/hosts or in DNS, or your
  tcpwrapper config files (/etc/hosts.deny, /etc/hosts/allow) do not
  allow the access. Fix the problem.



  After the tagged image is loaded, Etherboot will jump to it. If it
  crashes here, check that the image is a tagged image. If it executes
  and stops at the point where it's trying to mount the NFS root, then
  you need to check that you have the "root on NFS" option compiled in
  and that you have compiled in the network card driver.


  44..11..  SSeettttiinngg uupp aa NNFFSS rroooott ffiilleessyysstteemm


  Now you need to set up a NFS root filesystem for the diskless
  computer.  Typically this is under /tftpboot/<ip address of computer>.
  In 2.1 and higher kernels, this should be /tftpboot/<name of computer
  in bootptab>.  This needs to contain a complete root filesystem that
  will make the kernel boot happily.  This means, for most kernels, it
  should contain /dev, /proc, /etc, /sbin, /bin, /tmp and /var. The
  details vary from distribution to distribution.  See the FAQ section
  for various methods of constructing a NFS root filesystem. There is no
  one true method. In the method I use I was lazy so I just make a copy
  of the necessary files from an existing Linux filesystem on the server
  and modify some key files appropriately. You can find a description in
  my tutorial <diskless.html> and some shell scripts to copy the files.
  Since the amount of disk space needed is relatively small in these
  days of large disks, I don't bother to throw out things that may not
  be needed.



  One thing to be aware of is that when you host the root filesystem on
  a NFS server that is not Linux, the major and minor numbers of device
  files will be different from what Linux is expecting, so the init
  process will probably break just after it mounts the root NFS, maybe
  when it tries to open the console device. You must create the root
  filesystem so that it is Linux compatible, even though it is hosted on
  a different Unix.  One way might be to use cpio to capture a Linux
  root FS and then to unpack on the target Unix system.



  Warning: Do not attempt to reuse the root filesystem of your server,
  whether by exporting it directly or by making hard links (symbolic
  links will not work). First of all, the configuration files will
  contain information pertaining to the server, not the client, so your
  client will get the wrong information. Secondly, this is a security
  risk. NFS is already not totally safe, but this way you are directly
  exposing your server root to clients. Even if you make hard links, the
  clients could (maliciously or accidentally) overwrite key binaries,
  making the server unusable. Don't try to save a few megabytes of disk
  space this way. You can however share some directories between
  clients, typically /sbin, /bin and /lib.  The sample scripts in the
  tutorial show you how.



  The root filesystem should be exported rw and no_root_squash because
  the various processes need to be root and need to write to log files
  in the root partition. You may wish to export /usr and /home
  filesystems to the diskless computer also. These do not need
  no_root_squash permission, and in the case of /usr probably only needs
  to be ro. Otherwise you will be opening up a security hole for hacking
  the server from the client. Be aware that practically all Linux
  distributions have a few "bugs" relating to symlinks and so forth for
  diskless booting.  These are mentioned in the tutorial.


  44..22..  IInniittiiaall rraammddiisskk


  It is possible to use initial ramdisk (initrd) in addition to, or in
  place of an NFS root. See the ramdisk argument of mknbi-linux
  <mknbi.html>. You will also need to read the Linux kernel
  documentation to see what extra arguments should be passed to the
  kernel to make it use an initrd, and how to arrange the initrd so that
  the startup script within it is called when it's mounted.



  Initrds are useful for loading modules before the NFS root is mounted,
  or to use some other network filesystem instead of NFS root, for
  example.  Some applications could even run totally out of initrd,
  provided you have the memory, of course. Also traffic on the kernel
  mailing lists indicate that at some point in the future, kernel level
  autoconfiguration (BOOTP/DHCP from the kernel) may be removed from the
  Linux kernel and initrds may be the way to start up a diskless sytem
  that can acquire an identity using a userland DHCP client program.


  44..33..  SSwwaapp oovveerr NNFFSS


  Swap over NFS can be arranged but you have to patch the kernel source.
  There are patches in the contrib directory for NFS swap but for up to
  date patches, try here <http://www.instmath.rwth-aachen.de/~heine/nfs-
  swap/nfs-swap.html>.



  Be aware that opinions are divided on NFS swap.  Some people think
  it's a bad thing because it just kills the network if you have lots of
  diskless computers and that you shouldn't be running into a swap
  regime on a diskless computer anyway. Some other people like having a
  bit of insurance.



  Also have a look at the NBD
  <http://atrey.karlin.mff.cuni.cz/~pavel/nbd/nbd.html> Network Block
  Device web page for swapping over that. This requires a 2.1 or 2.2
  kernel.


  55..  BBoooottiinngg DDOOSS


  What about DOS? The deal with DOS is that one is loading a virtual
  floppy called A: into extended memory and then booting from this
  floppy.  So you have to capture an image of a bootable DOS floppy
  first.  Some more details can be found in the mknbi-dos <mknbi.html>
  utility.



  I have booted DOS (both M$ versions up to 5.0 and DR versions up to
  7.03) diskless this way.  A mknbi-fdos <mknbi.html> is available for
  building tagged images for booting FreeDOS, the procedure differs
  slightly from booting M$ or DR DOS. Note that extended memory is used
  so that rules out 086/088 computers but 286s are ok. See this document
  <atnetboot.html> for more details.



  If you were thinking of booting a Windows machine via the network, it
  seems (I'm not masochistic enough to do this) the problem is not the
  network booting but the mounting of a file system over NetBIOS
  (Windows does not do remote mounts of root filesystems over NetBIOS on
  TCP). So that rules out a Samba server. It appears to be possible over
  a Netware server, for which Linux or FreeBSD has workalikes. Also it
  is said that only certain versions of Windows will allow diskless
  booting.  But then what do you do about the networking stack? This
  situation may change with with future Samba developments. But you will
  still have problems with pathnames and the usual Windows hassles. Do
  you really want to do this?  You do know that you can run lots of
  desktop applications like Netscape, StarOffice, etc. on Linux,
  FreeBSD, etc. now?  In the Etherboot home page
  <http://etherboot.sourceforge.net/>, there are links to external Web
  pages, one explaining how this was done with a commercial TCP/IP boot
  ROM, another explaining how to do it using Etherboot and Netbios over
  IPX. Good luck and send us your experiences or better still a URL to a
  page explaining how you did it.


  66..  MMaakkiinngg aann EEtthheerrbboooott EEPPRROOMM oorr EEEEPPRROOMM


  Assuming you have satisfactorily set up your server environment, you
  may now wish to put the Etherboot onto an EPROM or EEPROM. Naturally
  this assumes access to hardware to program (and possibly erase)
  EPROMs.  Access to a friendly electronics engineer and/or lab is one
  way to program and erase EPROMs. Otherwise you can look at the
  commercial links page
  <http://etherboot.sourceforge.net/commercial.html> for places you can
  buy programmed EPROMs from.



  If you are familiar with electronics construction, an alternative is
  to use an EEPROM card. There is a schematic and PCB artwork for such a
  card at the web site where you got the Etherboot distribution. This
  EEPROM card plugs onto the ISA bus and can be reprogrammed with
  software.



  Some high-end network cards, for example the 3Com 905B, have a socket
  for an EEPROM which can be programmed in situ with the right
  utilities.  See any release notes accompanying Etherboot for more
  information.


  66..11..  CChhoooossiinngg tthhee EEPPRROOMM


  Most network cards come with a blank (E)EPROM socket even though it is
  seldom used. When it is used, it is typically filled with a
  proprietary EPROM from the network card manufacturer. You can put an
  Etherboot EPROM there instead.


  66..22..  EEnnaabblliinngg tthhee EEPPRROOMM


  First you must discover how to enable the EPROM socket on your card.
  Typically the EPROM is not enabled from the factory and a jumper or a
  software configuration program is used to enable it.


  66..33..  SSiizzee aanndd ssppeeeedd ooff tthhee EEPPRROOMM


  Secondly, you must discover what size and speed of EPROM is needed.
  This can be difficult as network card manufacturers often neglect to
  provide this information.



  The smallest EPROM that is accepted by network cards is an 8k EPROM
  (2764). 16kB (27128) or 32kB (27256) are the norm. Some cards will
  even go up to 64kB EPROMs (27512). You want to use the smallest EPROM
  you can so that you don't take up more of the upper memory area than
  needed as other extensions BIOSes may need the space.  However you
  also want to get a good price for the EPROM. Currently the 32kB and
  64kB EPROMs (27256 and 27512) seem to be the cheapest per unit.
  Smaller EPROMs appear to be more expensive because they are out of
  mainstream production.



  If you cannot find out from the documentation what capacity of EPROM
  your card takes, for ISA NICs only, you could do it by trial and
  error. (PCI NICs do not enable the EPROM until the BIOS tells the NIC
  to.)  Take a ROM with some data on it (say a character generator ROM)
  and plug it into the socket. Be careful not to use an extension BIOS
  for this test because it may be detected and activated and prevent you
  from booting your computer.  Using the debug program under DOS, dump
  various regions of the memory space. Say you discover that you can see
  the data in a memory window from CC00:0 to CC00:3FFF (= 4000 hex =
  16384 decimal locations). This indicates that a 16kB EPROM is needed.
  However if you see an alias in parts of the memory space, say the
  region from CC00:0 to CC00:1FFF is duplicated in CC00:2000 to
  CC00:3FFF, then you have put an 8kB EPROM into a 16kB slot and you
  need to try a larger EPROM.



  Note that because pinouts for 28 pin EPROMs are upward compatible
  after a fashion, you can probably use a larger capacity EPROM in a
  slot intended for a smaller one. The higher address lines will
  probably be held high so you will need to program the image in the
  upper half or upper quarter of the larger EPROM, as the case may be.
  However you should double check the voltages on the pins armed with
  data sheet and a meter because CMOS EPROMs don't like floating pins.



  If the ROM is larger than the size of the image, for example, a 32 kB
  ROM containing a 16 kB image, then you can put the image in either
  half of the ROM. You will sometimes see advice to put two copies of
  the image in the ROM. This will work but is not recommended because
  the ROM will be activated twice if it's a legacy ROM and may not work
  at all if it's a PCI/PnP ROM.  It is tolerated by Etherboot because
  the code checks to see if it's been activated already and the second
  activation will do nothing. The recommended method is to fill the
  unused half with blank data.  All ones data is recommended because it
  is the natural state of the EPROM and involves less work for the PROM
  programmer. Here is a Unix command line that will generate 16384 bytes
  of 0xFF and combine it with a 16 kB ROM into a 32 kB image for your
  PROM programmer.


  ______________________________________________________________________
          (perl -e 'print "\xFF" x 16384'; cat 3c509.lzrom) > 32kbimage
  ______________________________________________________________________





  The speed of the EPROM needed depends on how it is connected to the
  computer bus.  If the EPROM is directly connected to the computer bus,
  as in the case of many cheap NE2000 clones, then you will probably
  have to get an EPROM that is at least as fast as the ROMs used for the
  main BIOS. This is typically 150 ns. Some network cards mediate access
  to the EPROM via circuitry and this may insert wait states so that
  slower EPROMs can be used. Incidentally the slowness of the EPROM
  doesn't affect Etherboot execution speed much because Etherboot copies
  itself to RAM before executing. I'm told Netboot does the same thing.


  77..  TTrroouubblleesshhoooottiinngg ttiippss



  +o  Floppy boot doesn't work. Is the floppy error-free? Have you copied
     the ROM image (with the floppyloader prepended) to the floppy raw?
     Is that size of floppy bootable by your computer? Are you trying to
     run a 32 bit Etherboot on a 16 bit machine (286, 086/088)? Have you
     selected too many compile time options? The real limit on Etherboot
     is not the size of the EPROM but the fact that it executes in the
     32kB region between 0x98000 and 0xA0000. If the sum of code, stack
     and bss is greater than 32kB, then Etherboot might crash at
     unexpected places. You could increase the memory to 48kB by
     lowering the RELOCADDR in the Makefile but this is outside the
     specifications. Definitely do not lower RELOCADDR below 0x94000
     because various pieces of booting information are stored from
     0x90000 upwards.

  +o  Floppy version works but EPROM version doesn't work. There is a
     program called rom-scan (Linux, FreeBSD and DOS versions) in the
     directory contrib/rom-scan which will help detect problems. Rom-
     scan will only work on ISA ROMs though.


  +o  If the EPROM is not detected at all then the contents of the EPROM
     are not visible to the BIOS. Check that you have enabled the EPROM
     with any jumpers or soft configuration settings. Check that you do
     not have any conflicts in the memory address of the EPROM and any
     other hardware.  Perhaps you have to prevent it from being mapped
     out by your BIOS settings. Or perhaps you have to shadow it with
     RAM. Maybe you put the code in the wrong half or wrong quarter of
     the EPROM. Maybe the access time of the EPROM is not low enough.
     You can also use the debug program under BIOS to examine the memory
     area in question.

  +o  If rom-scan says the EPROM is present but not active, then
     something prevented the BIOS from seeing it as a valid extension
     BIOS. This could be truncation of the EPROM window, maybe you have
     a larger EPROM in a slot meant for a smaller one. Maybe there is a
     checksum error in the EPROM due to some bits not properly
     programmed or the EPROM not being fast enough. In one case that we
     know of, the 3c503 network card, the ASIC actually returns 2 bytes
     of 80 80 in the end locations of the EPROM space. This apparently
     is a kind of signature. The makerom program in Etherboot
     compensates for this, but if the pattern is not 80 80, then the
     code needs to be modified.

  +o  If rom-scan says the EPROM is present and active, but BIOS does not
     see it, then perhaps the EPROM is located in an area that the BIOS
     does not scan. The range scanned is supposed to be 0xC0000 to
     0xEF800 in increments of 2kB but I have seen some BIOSes that
     continue the scan into the 0xF0000 page.


     Note that rom-scan will also detect other extension BIOSes mounted
     on your computer, for example VGA BIOSes and SCSI adapter BIOSes.
     This is normal.

  +o  Etherboot does not detect card. Are you using the right ROM image?
     Is the card properly seated in the computer? Can you see the card
     with other software? Are there any address conflicts with other
     hardware? Is the PCI id of the card one that is not known to
     Etherboot yet? In this case and where you think there is a bug in
     Etherboot, please contact the author with all details.


  +o  Etherboot detects card but hangs computer after detection. Some
     cards are booby traps while they are enabled. The typical offenders
     are NE2000s which will hang the bus if any access is made to the
     reset addresses while interrupts are active. You may need to do a
     hard reset of the computer, i.e. power down and up again before
     running Etherboot.

  +o  Etherboot detects card but does nothing after saying Searching for
     server. Check your network hardware. Did you select the right
     hardware interface (AUI, BNC, RJ45)? Is the cabling ok? If you have
     a Unix computer on the network and have root privileges, you could
     run tcpdump or ethereal looking for broadcast packets on the bootps
     port. If the requests are getting sent out but no replies are
     getting back, check your bootpd setup. Also check if the server has
     a route to the client.

  +o  Etherboot obtains IP address but fails to load file. Check the tftp
     server. Is the tagged image file installed? Is the file world
     readable? Is the path to the file allowed by the configuration of
     tftpd? Is the client denied by tcpwrapper rules?  Did you put the
     right home directory and boot filename in bootptab? If you are
     booting lots of clients, is inetd shutting down tftpd for being
     spawned too often? If so, you need to get a better inetd or a a
     dedicated tftpd that runs as an independent daemon.

  +o  Etherboot loads file via tftp but Linux fails to boot. This is a
     large category. Here are some suggestions:


  +o  You do not have a private copy of the /, /etc, /var, ...
     directories

  +o  Your /dev directory is missing entries for /dev/zero and/or
     /dev/null or is sharing device entries from a server that uses
     different major and minor numbers (i.e. a server that is not
     running Linux).

  +o  Your /lib directory is missing libraries (most notably libc* and/or
     libm*) or does not have the loader files ld*.so*

  +o  You neglected to run ldconfig to update /etc/ldconfig.cache or you
     do not have a configuration file for ldconfig.

  +o  Your /etc/inittab and/or /etc/rc.d/* files have not been customized
     for the clients. For example if you set the wrong IP address in
     your client's init files you could interfere with the server.

  +o  Your kernel is missing some crucial compile-time feature (such as
     NFS filesystem support, booting from the net, transname (optional),
     ELF file support, networking support, driver for your ethernet
     card).

  +o  Missing init executable (in one of the directories known by the
     kernel: /etc, /sbin, ?). Remember /sbin/init must be a real file,
     not a symlink.

  +o  Missing /etc/inittab

  +o  Missing /dev/tty?

  +o  Missing /bin/sh

  +o  System programs that insist on creating/writing to files outside of
     /var (mount and /etc/mtab* is the canonical example)


     The essence is that you must provide whatever is needed in the NFS
     root filesystem that your kernel needs to boot. This is somewhat
     distribution dependent. In the discussion mentioned above I solved
     the problem by making a copy of an existing root filesystem and
     modifying a few bits.  Be aware that some, if not all,
     distributions contain bugs in their layout that hinder diskless
     mounting so you will have to fix any problems that arise. An
     example was a directory in /usr/X11R6/lib that needed to be
     writable, however /usr was mounted read-only. Another common
     problem is the assumption in init scripts that /usr is available
     before NFS mounts are done and invoking programs under /usr.

  +o  Etherboot works fine and kernel starts but network interface
     doesn't work. Check your network configuration in the OS.
     Etherboot uses polled I/O and not interrupt I/O to fetch the
     images. So the IRQ line doesn't get exercised. This manifested
     itself in one case with a NIC that could netboot but network
     programs run afterwards couldn't receive any packets. It seemed to
     be a combination of a weak NIC IRQ line and a fussy motherboard.
     But the same thing could happen if say the IRQ is not set to where
     the software is expecting it. The image will load fine with the
     wrong IRQ but the software will not run properly. This is more of
     an issue with say DOS packet drivers, where the user has to specify
     the IRQ line than with Linux, which autoprobes.




  88..  FFrreeqquueennttllyy AAnnsswweerreedd QQuueessttiioonnss


  Contributions are invited for this section. Mail them to me
  <mailto:ken_yap@users.sourceforge.net>.


  88..11..  BBuuiillddiinngg EEtthheerrbboooott



  1. What do I need to build Etherboot?



     You need gcc, gas and objcopy, as well as any accompanying
     libraries and include files. Generally speaking on a package based
     system using RPM or DEB, you will need the C compiler package, the
     include file package, the C library package, the assembler package
     and the binutils package (this may include the assembler). In
     addition, if you intend to modify the assembler files you will need
     an up-to-date as86 and ld86, not the ones that come standard with
     Linux, but from the ELKS project; or you will need nasm. You will
     also need perl if you modify the file NIC or use a different
     RELOCADDR value in the Makefile than the default, and m4 if you
     modify this file README.xsgml.


  2. I get an error from as saying data32 is an unknown directive.



     Your gas is too old, upgrade to 2.9.1 at least.


  3. I get an error from as saying data32 (and others) should be
     prefixes.

     Support for extended directives changed in syntax between gas 2.9.1
     and gas 2.9.5. Disable -DGAS291 in Config to allow for the new
     syntax. In recent versions of Etherboot, this should be
     automatically detected.


  4. The documentation talks about mknbi-linux and mknbi-dos. Where are
     they?



     These are utilities <mknbi.html> in the mknbi-<version> directory.
     You need to cd into this directory, edit Makefile and run make then
     make install to install the utilities in their final location.  You
     cannot use them until they have been installed as they rely on a
     library directory.


  5. Why don't you provide prebuilt ROM images?



     Etherboot is constantly updated, and binary images would get out of
     date soon.  Binaries can lose any external indication of what
     release they were from. Old binaries might float around and create
     problems for users who are not aware that bugs have been fixed in
     newer versions. Also there are some user configurable options you
     might want to change.  For this reason, Etherboot is provided in
     source form for you to build.



  88..22..  TTeessttiinngg EEtthheerrbboooott



  1. I put the ROM image on floppy like you wrote (cat
     floppyload.bin.pre bin32/foo.rom > /dev/fd0, or make bin32/foo.fd0)
     but the floppy prints out a bunch of register names and hex numbers
     when it boots.



     The floppy you use should be an error-free, preferably a recently
     formatted floppy. Do not trust new floppies; they have been known
     to lose their manufacturer formatting in storage. You don't need to
     put a filesystem of any sort on it, FAT or ext2 or otherwise.
     Another possible cause is that there are alignment differences
     between the drive used to write the floppy and that used on the
     target machine. As far as I know the floppy boot should work on all
     floppy sizes (1.44M, 720k, and even 1.2M and 360k if you can still
     find these), but beware of differences between drives, e.g. writing
     a 360k floppy in a 1.2M drive creates narrow tracks which may not
     overwrite previous contents.


  2. My network adapter is detected but I get no reply to the BOOTP/DHCP
     request.



     Do you have a BOOTP or DHCP server running on the same Ethernet
     segment?  On many operating systems the server is not enabled by
     default. Review the instructions in the Troubleshooting section.
     Another thing to note is that the BOOTP (or DHCP in fixed address
     mode) server will not reply if it does not know the network
     adapter's Ethernet address. Since the address may be hard to
     determine if it is not printed on the card or you do not have the
     adapter's setup program, you can copy it from Etherboot's startup
     message. Remember to restart the server if you have edited the
     config file and the server does not automatically reread when it
     discovers an updated config file.



     Another thing to check is that the BOOTP or DHCP server is allowed
     to receive the query. You may have some protection mechanism such
     as tcpwrappers in front of it. As the booting computer does not
     have an IP address, the request will come from 0.0.0.0 so your
     rules must allow through packets from this address.



     If the BOOTP or DHCP server is on another Ethernet segment, things
     get more complicated. You need to run a BOOTP or DHCP relay. You
     will probably also need to set the gateway field in the reply so
     that TFTP will work across the gateway. You should read a good
     explanation of how these work, in say, W. Richard Steven's book
     TCP/IP Illustrated.


  3. Etherboot gets the BOOTP/DHCP parameters but cannot find a TFTP
     server.



     Do you have a TFTP server running and is it allowed to serve the
     client in question? For example the tcpwrapper rules may not allow
     TFTPD to respond to the IP address the booting computer is at. You
     should look at the log files on the server for any clues.


  4. The TFTP server is found but it replies Access violation.



     Access violation is a blanket reply for many different problems but
     essentially the TFTP server cannot give Etherboot the file
     requested. Did you put the file where TFTPD expects to find it,
     e.g. on a directory that is on its path? Did you make the file
     world readable? Case of the filename is important too.


  5. I made this kernel and put it in /tftpdir like you wrote but
     Etherboot says Unable to load file.



     Is the file a tagged image? You cannot use a ordinary kernel image,
     you must process it with mknbi-linux <mknbi.html> first.


  6. I have this proprietary boot ROM and I used mknbi-linux to make a
     tagged image, (or I got this tagged image from the LTSP project),
     but the boot ROM doesn't load it, or it fails to run.



     Tagged image format is specific to Etherboot and Netboot. It will
     not work with proprietary boot ROMs. You have to find out from the
     supplier what boot procedures you should use. For example, if you
     are using a Lanworks boot ROM, the infomation you need is here
     <http://www.3com.com/managedpc>.


  7. Why don't you use some standard format instead of tagged image
     format?



     First of all, tagged image format actually precedes many of the
     proprietary formats in use today. So we were there first. Secondly
     there is no acknowledged standard for the boot image format,
     although there are some contenders now. Perhaps some day there will
     be an open, widely-accepted format. Until then you have to aware
     that there are different ones out there.



  88..33..  HHaarrddwwaarree ccaappaabbiilliittiieess



  1. What network cards are supported?



     See an earlier section in this README document.


  2. I have a machine with the X processor and Y megabytes of memory.
     What can I expect to run on it?



     Please note that these estimates are approximate:


  +o  On a 8088/8086 you can only boot ELKS, a 16-bit mini-version of
     Linux, since 0.0.83 there has been support for making a tagged
     image and a ramdisk. However there are few applications for ELKS.
     For more details please go to the ELKS web site and mailing lists.
     Note that there is no networking in ELKS yet (and probably never).

  +o  On a 286 you can run DOS. With 384kB of extended memory you can run
     a 320kB ramdisk that can hold NCSA telnet with a VT100 terminal
     emulation. With about 720kB of extended memory you can run DOS
     Kermit, which has telnet capabilities and a VT320 terminal
     emulation. You can run various small application and network
     services. See the AT netbooting document <atnetboot.html> and the
     16-bit HOWTO <16.html> for some ideas.

  +o  On a 386 with at least 4MB of memory you can boot Linux. With 4MB
     perhaps only a few telnet sessions are possible. With 8MB you might
     be able to run a text based web browser like Lynx or W3M. You can
     also run firewalls such as floppyfw <http://www.zelow.no/floppyfw>.

  +o  On a 486 with 16MB of memory you can run X to make an X-terminal.

  +o  On a Pentium with 32MB of memory you can run an X-terminal and some
     applications locally, say perhaps printing, daemons to control
     devices, etc.

  +o  On anything faster and with more memory you could perhaps do
     distributed computation, e.g. a Beowulf cluster.



  88..44..  BBoooottiinngg LLiinnuuxx



  1. The kernel loads but it cannot configure the network device.



     Did you compile the network adaptor driver into the kernel? Is the
     network adaptor at an address that is probed by the driver? If not,
     perhaps you need to provide some kernel parameters to help it find
     the hardware.


  2. The kernel loads but it cannot mount a root filesystem, then it
     asks me to insert a floppy.



     Did you build the kernel with the root on NFS option? You cannot
     use a stock kernel from a distribution.


  3. The kernel loads but it cannot find a NFS server for the root
     filesystem.



     Do you have a NFS server running and is it allowed to serve this
     client?  NFS is actually several services. On Linux at least you
     need: nfsd (either kernel or userland version), rpc.mountd and
     portmapper. Check if the tcpwrappers config file is allowing
     portmapper to receive the request.  Look at the log files for
     clues. Did we already mention that log files are your friends?


  4. The root filesystem mounts but it says something about not being
     able to open an initial console. Or alternatively, various services
     complain about not being able to write to the filesystem.



     A common mistake in Linux NFS servers is to put extra spaces in
     /etc/exports. Please see the NFS FAQ <http://nfs.sourceforge.net/>
     for frequently answered questions about Linux NFS services.



     Please review the Troubleshooting section for what is required on
     this root filesystem. The situation is complicated by the fact that
     there are many possible ways of setting this up, including using a
     root filesystem that is on ramdisk. If you wish to avoid many of
     the troubles, try using a packaged solution such as LTSP
     <http://www.ltsp.org/> or my shell scripts for creating a root
     filesystem by copying files from an existing distribution.


  5. What is the recommended way of setting up the NFS root?



     There is no one true way, but several approaches. Network booting
     is a tool and a very flexible one.  Here are just a few
     suggestions:


     a. You can make a separate NFS root for each client, but share the
        sharable files between clients using hard links to minimise disk
        space requirements. This is the approach I took in my tutorial
        <diskless.html>. I used this because I was lazy, I have only a
        couple of diskless clients and I could copy the files from an
        existing distribution.

     b. You could use the packaged solution from LTSP
        <http://www.ltsp.org/>, which creates a shareable NFS root for
        multiple clients.

     c. August Hoerandl describes his approach
        <http://elina.htlw16.ac.at/~hoerandl/nfs-root/> which uses a
        ramdisk for the initial startup, and then merges in a readonly
        NFS filesystem.

     d. It is rumoured that the Debian distribution
        <http://www.debian.org/> has a package called diskless that
        contains Perl scripts to create such a filesystem.



     If you have an approach which you think the world should know
     about, please send the URL of a web page that describes it to me
     <mailto:ken_yap@users.sourceforge.net>.



  88..55..  RRuunnnniinngg XX



  1. I tried to run X on the client but it aborted.



     Remember that the config files used by the X server should pertain
     to the client's video adapter and display hardware. If you used a
     LTSP <http://www.ltsp.org/> package, please review the
     configuration directions. If you used the copy files from server
     solution, then you need to customise the X server configuration.
     Another thing that may cause the server to abort is lack of a mouse
     device.


  2. X -query server runs but all I get is a gray stippled screen.



     Either you don't have an XDM server running on the server machine
     or it is not allowed to serve this client. In the latter case check
     XDM's Xaccess file, because for security reasons, the ability for
     clients to connect is usually disabled.


  3. When I am logged in using an X-terminal, I find that the floppy
     drive, sound card and name of the computer are those of the
     server!?



     This is to be expected! This is exactly how an X-terminal works.
     You are indeed logged onto the server and the client just provides
     display (screen) and input device (keyboard and mouse) services to
     the application. This is one of the beauties of the X Windowing
     system model, it's _n_e_t_w_o_r_k _t_r_a_n_s_p_a_r_e_n_t.
  4. So how do I run applications on the client? I have this (smartcard
     reader, printer, sound card, etc) program that must execute
     locally.



     Some client configurations allow you to run applications locally,
     in addition to running them on the server. In fact the printer and
     sound daemons mentioned later are applications that are run on the
     client.


  5. X applications cannot find (some) fonts.



     Do you have an X font server (XFS) running on the server machine,
     is it allowed to serve this client, and has the client been told to
     use the font server? The last point is usually configured in the
     XF86Config file, or by a xset command to modify the font path after
     logging in.



     Also note that RedHat (and possibly other distributions) has made
     XFS by default serve only the local machine using a Unix socket.
     You need to modify the startup script to tell XFS to use a TCP/IP
     socket.


  6. How much CPU power and memory do I need on the client? On the
     server?



     It depends on the configuration. There are two major cases: where
     the client is an X-terminal, and not much more; and where the
     client is configured to run applications locally.



     An X server will fit in 16MB of memory, and 32MB is quite adequate.
     Performance depends on the CPU, video card and your expectations.
     An old Pentium 200 with a PCI video card does very well, but if you
     are not fussy, a high-end 486 with a VLB video card can be
     satisfying too.



     If you want to run apps locally, well how long is a piece of
     string?  Netscape will need say another 16MB. It all depends.
     Whatever you do, it's worth trimming down on the services you run
     on the client. Don't run more virtual consoles than you need and
     don't run unneeded daemons.



     As for the server, in the X-terminal case this has all the
     applications running on it, so it should be adequate for the
     multiuser aspect. A high-end Pentium, with 64 MB of memory to start
     with, and between 8 and 16MB for each extra client is a good
     starting point. It will also depend on your mix of client access,
     statistically perhaps not everybody will be running at the same
     time. Remember that you don't have to have one big server for all
     your clients, you can and you should distribute the load across
     servers.
  88..66..  OOtthheerr cclliieenntt aapppplliiccaattiioonnss



  1. How can I print to a printer attached to a diskless client?



     There is a server program called p910nd at the Etherboot site (and
     improved versions of it at LTSP <http://www.ltsp.org/>) that
     funnels data from a TCP/IP connection to the printer port. You can
     instruct lpd or CUPS <http://www.cups.org/> on the server to send
     jobs across the network to p910nd.


  2. How can I output sound on the client?



     There is a package called virtualfs <http://www.solucorp.qc.ca>
     that proxies the sound devices across the network. It can also
     proxy the floppy drive.



     Another solution is EsounD
     <http://www.tux.org/~ricdude/EsounD.html>


  3. How can I access the floppy on the client?



     Besides virtualfs mentioned above, recent distributions of mtools
     <http://wauug.erols.com/pub/knaff/mtools/> have a floppyd. This
     only works with the mtools utilities though.



  88..77..  BBoooottiinngg FFrreeeeBBSSDD



  1. Where are the instructions for booting FreeBSD?



     For now, there is just a short document in the doc directory.
     Better versions of this document depend on contributions from the
     FreeBSD community, I am unable to test FreeBSD because I don't run
     it.



  88..88..  BBoooottiinngg ootthheerr ooppeerraattiinngg ssyysstteemmss ((DDOOSS,, WWiinnddoowwss))



  1. I want to boot FreeDOS.



     The new mknbi utility supports creating tagged images from FreeDOS
     kernels now.  See the man page <mknbi.html> for details.  FreeDOS
     is a moving target and the layout of the kernel image or other
     things may have changed so you would need to be well acquainted
     with FreeDOS. Please send any corrections to me.


  2. I cannot boot DR-DOS 7.03.



     There is some difference between the DR-DOS 7.03 and 7.02 bootblock
     that causes it not to boot. But a 7.02 bootblock works just as well
     with the DR-DOS 7.03 kernel, so you can substitute that. There are
     instructions to extract the bootblock in the AT netbooting document
     <atnetboot.html>.


  3. DOS dies when I load HIMEM.SYS.



     Use the /testmem:off option to prevent HIMEM from scribbling over
     the ramdisk which is the floppy A:.


  4. How do I make A: my real floppy again after booting is complete?



     Use the rmrd.com program supplied with mknbi-1.0 <mknbi.html>.


  5. I want to use the real floppy at the same time I am using the
     ramdisk image of the boot floppy.



     The --harddisk option of mknbi is intended for this. It causes your
     boot drive to be C:, so you can use A: for the real floppy. See the
     man page <mknbi.html> for more details


  6. I want to boot Windows.



     I pass on this one, as I do not have (by choice) any Windows
     systems running on my computers. Perhaps others can contribute to
     this section. However I gather that it is only possible on
     Windows95A, as other versions don't have the necessary support for
     diskless booting.



  88..99..  HHaarrddwwaarree iissssuueess



  1. Where can I get an EPROM made?



     Depending on where you live, you might find a supplier listed on
     the Commercial Links page. Another possibility is to seek the help
     of someone working in a university or industrial lab who has an
     EPROM programmer.  If you are handy with hardware, you could buy a
     kit or build your own.  There are links to kit suppliers in the
     Commercial Links part of the home page.

     Some high end adapters, for example the 3Com and Intel ones, accept
     an EEPROM in the socket. This can be programmed in-situ using
     utility programs, some of which or information about are under the
     contrib directory in the Etherboot distribution.



     Finally some recent motherboard have flash BIOSes which contain
     space where an extension BIOS such as Etherboot can be inserted.
     The Phoenix Award BIOSes can be modified using a program called
     cbrom.exe possibly here <http://www.ping.be/bios>. It is also
     reputed to live here
     <http://www.boardrunner.com/download/utility/utility.htm>. Or do a
     Web search for it. No success has been reported for AMI BIOSes.
     Dirk von Suchodoletz maintains a list of successes and failures
     here <http://goe.net/anleitungen/award_board.html>.

     Here is some text contributed by Dirk von Suchodoletz. He hopes to
     put it on a web site someday:















































     ___________________________________________________________________
     6.4 Using your mainboard's BIOS to integrate etherboot-code

     Newer mainboards that have an AWARD-BIOS can use etherboot without
     seperate EPROMS and therefore without the necessity of having a
     EPROM-programmer[?]. (Heinrich Rebehn wishes to add: Flashing the BIOS
     is always a (small) risk. Flashing with unsupported, hacked BIOS image
     is *dangerous* and may render your PC unbootable. If you don't
     have access to a PROM burner you should stay away from experimenting.) In
     order to do this, you need 2 software tools: awdflash.exe, which should be
     included in your mainboard package, and cbrom.exe, which is an OEM-tool
     that allows modifications of the BIOS. awdflash.exe reads and writes
     the flashrom content, whereas cbrom.exe is used to analyze the content
     of the AWARD BIOS image. cbrom.exe can also add code to the BIOS image
     or remove components. This way you can easily integrate etherboot into
     your mainboards without even opening the PC's case.

     After the BIOS image has been saved (e.g. as bios.bin), or in case the
     current version of the BIOS has been copied from the board manufacturer's
     website, 'cbrom bios.bin /d' shows how much space is left on the image
     for your code.

     As the flashrom holds the compressed BIOS, cbrom will also compress
     the code when adding it to the BIOS. Therefore, 8 to 20 kbyte of free
     memory is needed, depending on the network adapter's driver. In case not
     enough memory is left, unneeded BIOS components can be removed from the
     BIOS image to regain space: the manufacturer's logo or the Symbios/NCR
     SCSI-code are note needed for diskless systems. 'cbrom bios.bin
     /[pci|ncr|logo|isa] release' will remove those unnessary components.

     The command line "cbrom bios.bin /[pci|isa] bootimg.rom [D000:0]" adds
     the compiled etherboot code to the bios. bootimg.rom is the code we
     would use to burn onto EEPROMs in other cases. Depending on your network
     card, either the pci or isa options have to be used. With isa cards
     you have to tell cbrom to which RAM location the code will be extracted
     at boot time. Attention: Compile the etherboot with the -DASK_BOOT or
     -DEMERGENCYDISKBOOT option to be able to access a disk. The code added
     by cbrom will be executed before the computer seeks for a boot disk
     or floppy.

     6.5 Booting with a DOS executable (COM) file

     If the computer has to be used with more than one operating systems,
     for example using the computer as an X-Terminal in addition to the
     already installed NT on the harddrive, etherboot has to be used with
     the compile-time option -DASK_BOOT. In case hardware-conflicts beetween
     Windows NT and the installed EPROM exist, creating DOS Executables
     (e.g. using 'make rtl8139.com') can provide a useful remedy. Those DOS
     Executables are comparable in their functionality to .rom images and
     can be used as substitutes.

     In case an existing DOS-bootsector, stored in BOOTSECT.DOS, cannot be
     used, creating one has to be done by formatting and installing a harddrive
     using DOS before installing NT (see Win-NT Multiboot-HOWTO). In addition
     the DOS system files are needed (IO.SYS, MSDOS.SYS, or KERNEL.SYS
     when using Freedos) and have to be copied into the directory of the
     NT loader. If using Autoexec.bat to start the .COM file is desired,
     either the particluar COMMAND.COM has to be provided or the etherboot
     file needs to be renamed as COMMAND.COM. This file will then be started
     instead of the DOS-Shell which is useful for avoiding unwanted user
     interaction. Afterwards a line has to be added to BOOT.INI as if DOS
     was to be booted:

     [boot loader]
     timeout=20
     default=C:\bootsect.dos #add this line if dhcp/tftp should be default action
     [operating systems]
     multi(0)disk(0)rdisk(0)partition(2)\WINNT="Windows NT Workstation, Version 4.0"
     [VGA-Modus]" /basevideo /sos
     C:\bootsect.dos="DHCP/TFTP (Linux diskless via etherboot)" #our boot option
     ___________________________________________________________________



  And here are some comments by Rapp Informatik Systeme GmbH about
  cbrom.exe versions:


  ______________________________________________________________________
  Some more remarks for cbrom..

  There are several version numbers of cbrom.exe p.e. 1.x and 2.x.
  and there is a cbrom called cbrom6.exe. First cbrom.exe with Version
  1.x (newest 1.32) is for Award Bios Version 4.5x  and cbrom6.exe is
  for Award Bios Version 6.xx.

  So because it seems a lot people become confused and use cbrom 1.x
  for the new 6.x Bios Award merged this together to a cbrom.exe  with
  Version number 2.x ( newest know 2.04) witch now runs on Award 4.5x and
  6.xx Bioses.

  Now how to find cbrom.exe. Different 1.x Versions of cbrom.exe
  could be found on the net, cbrom6.exe seems to be gone. It seems
  that Award/Phoenix do all that cbrom is deleted from servers of board
  manufaktures. So cbrom.exe Vers 2.04 is not available on the net.
  If somebody need this please try to send a demand question to the list -
  I hope somebody will mail it to you.
  ______________________________________________________________________




  2. How do I enable the ROM socket on my network adapter? There are no
     jumpers on the card.



     These jumperless cards need a card-specific utility program to
     enable the ROM. Normally the manufacturer supplies it on a diskette
     or CDROM.  You lost the diskette? If you know the manufacturer, you
     might be able to get the program from their website. You have a
     mystery card? Well the first thing to do is to identify the card.
     If it is an ISA card and made in Taiwan or China it's almost
     certainly a NE2000 clone. For some information, try here
     <http://members.nbci.com/ken_yap/NIC/>.


  3. I would like to boot my laptop diskless from a floppy containing
     Etherboot.



     The problem is that laptops these days use PCMCIA network adapter
     cards.  These in turn connect to the PCMCIA controller when docked.
     To be able to communicate with the PCMCIA card, Etherboot would
     first have to talk to the PCMCIA controller. Until somebody writes
     the code to do this...  Booting from disk is different because the
     kernel will load the PCMCIA controller code from disk first. You
     could always put a Linux kernel on the boot floppy.



  88..1100..  DDrriivveerrss



  1. There is no Etherboot driver for my network adapter. Can you write
     me one?



     If I were independently wealthy and had nothing else to do in life,
     sure!  But unfortunately I have a day job and Etherboot is a hobby.
     A couple of the drivers were written for pay and the others were
     written by volunteers. Perhaps you might like to volunteer?  If you
     have a good grasp of C, and understand basic hardware concepts, it
     is quite doable, and not nearly as difficult as writing a Linux
     device driver.  See the section on ``Writing an Etherboot Driver''.
     You will have the reward of understanding hardware intimately and
     seeing your work benefit users worldwide.


  2. I see that my network adapter is supported in Linux. Can I use the
     Linux driver in Etherboot? Or maybe you can adapt the Linux source.
     I can send you the file if you just say the word.



     No, the structure of Linux and Etherboot drivers are quite
     different.  There are several reasons: Linux drivers are more
     complicated and written to give good performance, whereas Etherboot
     drivers are written to be simple. Linux drivers are interrupt
     driven, whereas Etherboot drivers are polling. Linux drivers have
     an elaborate support structure, whereas Etherboot drivers are
     fairly self-standing. Finally Etherboot drivers have a difficult
     constraint on the amount of memory available to it, about 32kB near
     the top of 640kB.



     But... you can use Linux drivers as a source of reverse-engineering
     information. Several of the drivers in Etherboot were adapted from
     Linux drivers. But don't send me the file; see previous FAQ about
     volunteering. And I have the latest Linux source anyway, doesn't
     everyone?



  88..1111..  MMiisscceellllaanneeoouuss



  1. I don't understand something, or I have a question not covered by
     this list.



     Please join the Etherboot mailing lists
     <http://sourceforge.net/projects/etherboot>. These are listed on
     the Etherboot home page.



     Other lists you can join are the Netboot mailing list (joining
     details on Etherboot home page), and the LTSP mailing lists at LTSP
     < http://www.ltsp.org/>.  Etherboot and Netboot lists are for
     general netbooting issues, while LTSP is focused more on the LTSP
     packages. However there is a fair amount of overlap between the
     lists and many key people are on all lists.



     I strongly suggest you post Etherboot questions to the Etherboot-
     users mailing list instead of mailing me because: you get the
     benefit of a lot of experts seeing your question (no, I don't know
     everything, if only because there are many configurations I have
     never used); a lot of people see the question and answer and this
     helps them too. For these reasons you may not get any reply from me
     if you email me privately. I want to make the best use of my time
     and that is by making sure that as many people as possible see the
     questions and answers.



     Note that the Netboot mailing list is spam protected.  If your ISP
     has been slack security-wise and had a machine of theirs get onto
     one of the Open Relay Blacklists, then you will not be allowed to
     post. The only things I can suggest are to change your ISP to a
     more responsible one, or to get a Web based mailbox. I cannot help
     you with problems here as I do not administer the Netboot mailing
     list.



  99..  WWrriittiinngg aann EEtthheerrbboooott DDrriivveerr


  So Etherboot does not have a driver for your network adapter and you
  want to write one. You should have a good grasp of C, especially with
  respect to bit operations. You should also understand hardware
  interfacing concepts, such as the fact that the x86 architecture has a
  separate I/O space and that peripherals are commanded with out
  instructions and their status read with in instructions.  A
  microprocessor course such as those taught in engineering or computer
  science curricula would have given you the fundamentals. (Note to
  educators and students in computer engineering: An Etherboot driver
  should be feasible as a term project for a final year undergraduate
  student. I estimate about 40 hours of work is required. I am willing
  to be a source of technical advice.)



  Next you need a development machine. This can be your normal Linux
  machine. You need another test machine, networked to the development
  machine. This should be a machine you will not feel upset rebooting
  very often. So the reset button should be in working condition. :-) It
  should have a floppy drive on it but does not need a hard disk, and in
  fact a hard disk will slow down rebooting. Alternatively, it should
  have a another network adapter which can netboot; see discussion
  further down.  Needless to say, you need a unit of the adapter you are
  trying to write a driver for. You should gather all the documentation
  you can find for the hardware, from the manufacturer and other
  sources.



  There are several types of network adapter architecture. The simplest
  to understand is probably programmed I/O. This where the controller
  reads incoming packets into memory that resides on the adapter and the
  driver uses in instructions to extract the packet data, word by word,
  or sometimes byte by byte. Similarly, packets are readied for
  transmission by writing the data into the adapter's memory using out
  instructions. This architecture is used for example on the NE2000 and
  3C509. The disadvantage of this architecture is the load on the CPU
  imposed by the I/O. However this is of no import to Etherboot (who
  cares how loaded the CPU is during booting), but will be to Linux.
  Next in the sophistication scale are shared memory adapters such as
  the Western Digital or SMC series, of which the WD8013 is a good
  example. Here the adapter's memory is also accessible in the memory
  space of the main CPU.  Transferring data between the driver and the
  adapter is done with memory copy instructions. Load on the CPU is
  light. Adapters in this category are some of the best performers for
  the ISA bus.  Finally there are bus mastering cards such as the Lance
  series for the ISA bus and practically all good PCI adapters (but not
  the NE2000 PCI). Here the data is transferred between the main memory
  and the adapter controller using Direct Memory Access.



  Examine the file skel.c, in the src directory, which is a template for
  a driver. You may also want to examine a working driver. You will see
  that an Etherboot driver requires 5 functions to be provided:


  +o  A probe routine, that determines if the network adapter is present
     on the machine. This is passed a pointer to a nic struct, a list of
     candidate addresses to probe, and possibly a pointer to a pci
     struct.  This routine should initialise the network adapter if
     present.  If a network adapter of the type the driver can handle is
     found, it should save the I/O address at which it was found for use
     by the other routines. In the case of ISA adapters, it may be
     passed a list of addresses to try, or if no list is passed in, it
     may use an internal list of candidate addresses. In the case of PCI
     adapters, the address has already been found by the PCI support
     routines. Then it should determine the Ethernet (MAC) address of
     the adapter and save it in nic->node_addr. It should then call the
     reset routine to initialise the adapter.  Finally it should fill in
     the function pointers for the other routines, and return the nic
     pointer. If it fails to find an adapter, it should return 0.



     Initialising the adapter means programming the registers so that
     the chip is ready to send and receive packets. This includes
     enabling the appropriate hardware interface (10B2, 10BT) in the
     case of adapters with more than one interface, and setting the
     right speed (10Mb, 100Mb) if the hardware does not autosense and
     set it.  It also includes setting up any memory buffers needed by
     the hardware, along with any necessary pointers.



     Note that you should program the receiver registers to allow
     broadcast Ethernet packets to be received.  This is needed because
     other IP hosts will do an ARP request on the diskless computer when
     it boots.


  +o  A reset routine, that resets the adapter to a known state.  This is
     passed a pointer to a nic struct.  This can be called from the
     probe routine.

  +o  A disable routine, which puts the adapter into a disabled state.
     This is passed a pointer to a nic struct.  This is needed to leave
     the adapter in a suitable state for use by the operating system
     which will be run after Etherboot. Some adapters, if left in an
     active state, may crash the operating system at boot time, or
     cannot be found by the operating system.


  +o  A transmit routine, to send an Ethernet packet. This is passed a
     pointer to a nic struct, the 6 byte Ethernet address of the
     destination, a packet type (IP, ARP, etc), the size of the data
     payload in bytes, and a pointer to the data payload. Remember the
     packet type and length fields are in x86 byte order (little-endian)
     and the adapter's byte order may be the reverse (big-endian). Note
     that the routine knows nothing about IP (or any other type)
     packets, the data payload is assumed to be a filled in packet,
     ready to transmit.

  +o  A poll routine, to check if a packet has been received and ready
     for processing. This is passed a pointer to a nic struct. If a
     packet is available, it should copy the packet from the adapter
     into the data area pointed to by nic->packet, and set
     nic->packetlen to the length of the data, and return 1, otherwise
     0.



     A few Ethernet controller chips will receive packets from itself,
     as detected by having a source address of itself. You can throw
     these out immediately on reception and not bother the upper layer
     with them.


  Only the probe routine needs to be public, all the other routines
  should be static and private to the driver module. Similarly all
  global data in the driver should be static and private.



  A prototype for the probe routine should be added to the file
  config.c, conditional on INCLUDE_NAMEOFNIC.  NAMEOFNIC is derived from
  the name of the driver source file for your adapter by uppercasing
  alphabets and converting hyphen to underscore. If the file is pop-
  sicle.c, then the symbol is INCLUDE_POP_SICLE. Also conditional on
  INCLUDE_NAMEOFNIC should be a struct entry containing the name of the
  driver and a pointer to the probe routine. The third element of the
  structure should be pci_probeaddrs in the case of PCI adapters,
  otherwise 0.



  If the NIC is a PCI adapter, then you also need to put an entry in the
  pci_nic_list array with the name, vendor and id fields filled in. You
  can obtain the vendor and device ids from the file
  /usr/include/linux/pci.h. It is also displayed by PCI BIOSes on
  bootup, or you can use the lspci program from the pciutils package to
  discover the ids.  The other fields will be filled in by the pci
  routines.  The symbol INCLUDE_NAMEOFNIC should be used to set
  INCLUDE_PCI.



  Then add an entry to the file NIC so that the build process will
  create Makefile rules for it in the file Roms. The build process will
  cause the driver object will be pulled in from the driver library. Use
  the rule for bin32/driver.fd0 or the command cat floppyboot.bin
  bin32/driver.rom > /dev/fd0 to write another instance of the driver to
  the floppy for testing. Use lots of printf statements to track where
  execution has reached and to display the status of various variables
  and registers in the code.  You should expect to do this dance with
  the development machine, floppy disk and target machine many many
  times.


  There is another method of testing ROM images that does not involve
  walking a floppy disk between the machines and is much nicer. Set up a
  supported NIC with a boot ROM. Put the target NIC on the same machine
  but at a non-conflicting I/O location. That is to say, your test
  machine has two NICs and two connections to the LAN.  Then when you
  are ready to test a boot image, use the utility mknbi-rom <mknbi.html>
  to create a network bootable image from the ROM image, and set up
  bootpd/DHCPD and tftpd to send this over the when the machine
  netboots.  Using Etherboot to boot another version of itself is rather
  mind-boggling I know.



  Now set up the various required services, i.e. BOOTP/DHCP, tftp, etc.
  on the development machine. You should go through the setup process
  with a supported adapter card on a test machine so that you know that
  the network services are working and what to expect to see on the test
  machine.



  If you are starting from a Linux driver, usually the hardest part is
  filtering out all the things you do not need from the Linux driver.
  Here is a non-exhaustive list: You do not use interrupts. You do not
  need more than one transmit buffer. You do not need to use the most
  efficient method of data transfer. You do not need to implement
  multicasting. You do not need to implement statistics counting.



  Generally speaking, the probe routine is relatively easy to translate
  from the Linux driver. The reset routine is tricky because you won't
  know if it worked until you try to transmit or receive. So check
  carefully for typographical errors. The transmit is usually
  straightforward, and the receive a bit more difficult. The main
  problem is that in the Linux driver, the work is split between
  routines called from the kernel and routines triggered by hardware
  interrupts.  As mentioned before, Etherboot does not use interrupts so
  you have to bring the work of transmitting and receiving back into the
  main routines. The disable routine is straightforward if you have the
  hardware commands.



  When coding, first get the probe routine working. You will need to
  refer to the programmer's guide to the adapter when you do this.  You
  can also get some information by reading a Linux or FreeBSD driver.
  You may also need to get the reset routine working at this time.



  Next, get the transmit routine working. To check that packets are
  going out on the wire, you can use tcpdump or ethereal on the
  development machine to snoop on the Ethernet. The first packet to be
  sent out by Etherboot will be a broadcast query packet, on UDP port
  67. Note that you do not need interrupts at all.  You should ensure
  the packet is fully transmitted before returning from this routine.
  You may also wish to implement a timeout to make sure the driver
  doesn't get stuck inside transmit if it fails to complete. A couple of
  timer routines are available for implementing the timeout, see
  timer.h. You use them like this (in pseudo-code):





  ______________________________________________________________________
          for (load_timer2(TIMEOUT_VALUE);
                  transmitter_busy && (status = timer2_running()); )
                  ;
          if (status == 0)
                  transmitter_timed_out;
  ______________________________________________________________________



  The timeout value should be 1193 per millisecond of wait. The maximum
  value is 65535, which is about 54 milliseconds of timeout. If you just
  need to delay a short time without needing to do other checks during
  the timeout, you can call waiton_timer2(TIMEOUT_VALUE) which will
  load, then poll the timer, and return control on timeout.



  Next, get the receive routine working. If you already have the
  transmit routine working correctly you should be getting a reply from
  the BOOTP/DHCP server. Again, you do not need interrupts, unlike
  drivers from Linux and other operating systems. This means you just
  have to read the right register on the adapter to see if a packet has
  arrived.  Note that you should NOT loop in the receive routine until a
  packet is received. Etherboot needs to do other things so if you loop
  in the poll routine, it will not get a chance to do those tasks. Just
  return 0 if there is no packet awaiting and the main line will call
  the poll routine again later.



  Finally, get the disable routine working. This may simply be a matter
  of turning off something in the adapter.



  Things that may complicate your coding are constraints imposed by the
  hardware. Some adapters require buffers to be on word or doubleword
  boundaries. See rtl8139.c for an example of this. Some adapters need a
  wait after certain operations.



  When you get something more or less working, release early. People on
  the mailing lists can help you find problems or improve the code.
  Besides you don't want to get run over by a bus and then the world
  never gets to see your masterpiece, do you? :-)



  Your opus should be released under GPL, BSD or a similar Open Source
  license, otherwise people will have problems using your code, as most
  of the rest of Etherboot is GPLed.


  1100..  AAcckknnoowwlleeddggeemmeennttss ((aa..kk..aa HHaallll ooff FFaammee))


  The following people have contributed substantially to Etherboot. If
  you feel your name has been left out, just let me know and I will fix
  it up.



     MMaarrkkuuss GGuuttsscchhkkee
        Co-author of Etherboot. He was the person who ported the Netboot
        suite from FreeBSD. He has enhanced Etherboot with many
        features, one new driver and has contributed various utilities
        and addons.


     GGeerroo KKuuhhllmmaannnn
        The mknbi utilities used by Etherboot are from Netboot.  He has
        also clarified the original specification by Jamie Honan.


     JJaammiiee HHoonnaann
        Jamie started Netboot off by writing the first version that used
        code from a packet driver.


     MMaarrttiinn RReenntteerrss eett.. aall
        The original authors of Netboot on FreeBSD.


     BBrruuccee EEvvaannss
        Created bcc compiler used by Etherboot/16.


     RRoobb ddee BBaatthh
        Current maintainer of bcc and associated tools like as86.


     GGeerrdd KKnnoorrrr
        Contributed MASQ for making a boot floppy without DOS.


     AAddaamm RRiicchhtteerr
        Contributed comboot for making a boot floppy without DOS.


     CCllaauuss--JJuussttuuss HHeeiinnee
        Contributed patch for serial console and NFS swapping. See the
        contrib/nfs-swap directory for his Web page.


     DDiicckkoonn RReeeedd
        Contributed display of loading status and a hack for the 3c509
        card.


     DDaavviidd MMuunnrroo
        Contributed PCI detection code originally from Linux sources.


     CChhaarrlliiee BBrraaddyy
        Donated NE2100 card so that a driver could be written, and
        helped test the LancePCI driver. Spotted bug with 4.1 header
        code.


     RRooggiieerr WWoollffff
        Created Intel EtherExpressPro 100 driver and binary to hex
        converter.


     VVllaagg LLuunngguu
        Contributed patches to work with DHCP. Also contributed a fix to
        match the received XID against the transmitted one, important in
        a network with many requesters.


     WWiilllliiaamm AArrbbaauugghh
        Patches for eepro to work with 3.2.


     JJeeaann MMaarrcc LLaaccrrooiixx
        Contributed an improved bin2intelhex.


     JJiimm HHaagguuee
        Contributed fixes to 3c503 driver for PIO mode, fix to makerom
        for presetting EPROM bytes, and various endian fixes.


     AAnnddrreeww CCoouulltthhuurrsstt
        Contributed patch for making Intel eepro work in 4.0.


     DDoouugg AAmmbbrriisskkoo
        Contributed patches to start32.S from FreeBSD version to make it
        boot Windoze after answering N to Boot from Network question.
        Contributed FreeBSD support and improved serial console support
        which is now merged into distribution since version 4.2.8.


     AAlleexx HHaarriinn
        Contributed patches for prepended loaders and makerom to make
        bootrom PnP and PCI compatible.


     PPeetteerr DDoobbccssaannyyii
        Contributed vendor and device IDs for the Netvin NE2000/PCI
        clone.


     aaddaamm@@mmuuddlliisstt..eeoorrbbiitt..nneett
        Contributed RARP code as alternative to BOOTP/DHCP. Activated by
        RARP_NOT_BOOTP define.


     DDaanniieell EEnnggssttrroomm
        Contributed a SMC9000 driver.


     DDiiddiieerr PPooiirroott
        Contributed an Etherpower II (EPIC 100) driver.


     MMaarrttiinn AAttkkiinnss
        Contributed mntnbi for mounting DOS NBIs.


     AAttttiillaa BBooggaarr
        Contributed a bug fix to the bootmenu code and a patch to main.c
        to remove looping menus on failure. Also code for ARP replies
        and TFTP retransmit (#ifdef CONGESTED). Cleanup of tftp and
        tftpd.


     NNaatthhaann RR.. NNeeuulliinnggeerr
        Found bug due to tu_block being declared signed short in
        arpa/tftp.h on many platforms when it should be unsigned short.


     DDaavviidd SShhaarrpp
        Contributed a FreeBSD driver for Tulip based cards. Ken Yap
        ported it to Etherboot. Not tested because code needs to be
        written for all the variants of the Tulip and also because no
        hardware available to me.


     GGrreegg BBeeeelleeyy
        Contributed a 3c905b driver. Be sure to read the release notes
        in 3c905b.txt before using.


     AAlleexx NNeemmiirroovvsskkyy
        Contributed patches to use BIOS call to size memory otherwise
        Etherboot was trampling on top of 640kB area, which is used by
        some extended BIOSes. Also contributed patches to pci.c to
        implement PCI bus support on BIOSes that do not implement
        BIOS32, or incorrectly.


     GGuueenntteerr KKnnaauuff
        Suggested making the ASK_BOOT prompts more generic and clearer.
        Also contributed a DOS utility for extracting the identifier
        string and PCI IDs, if any, out of the boot ROM. Contributed a
        wake on LAN CGI script.


     KKllaauuss EEssppeennllaauubb
        Contributed various cleanup patches to the code especially in
        the bootmenu area, fixes for the NE2000 driver, as well as a
        completely revamped start32.S.  Also introduced Rainer
        Bawidamann's code, see next paragraph. Contributed further
        improvements in Realtek 8139 driver. Did a major rewrite from
        4.4.5 to 4.5.5, see doc/maint/LOG.


     RRaaiinneerr BBaawwiiddaammaannnn
        Contributed a Realtek 8139 driver.


     GGeeoorrgg BBaauumm
        contributed a Schneider & Koch G16 driver.


     jjlluukkee@@ddeeaakkiinn..eedduu..aauu
        sent in a fix for the WD/SMC8013 which I finally verified.


     MMaarrkk BBuurraazziinn
        contributed a fix for Compex RL2000 NICs.


     MMaatttthhiiaass MMeeiixxnneerr
        found a receive status bug in the RTL8139 driver.


     JJiimm MMccQQuuiillllaann
        provided changes to support the SMC1211 which uses the RTL8139
        chip.


     SStteevvee SSmmiitthh
        Extended the 3c905b driver for other members of the 90x family.
        Be sure to read the release notes in 3c90x.txt before using.
        Modified loader.S for some BIOSes that don't behave correctly
        with INT19H.



     JJoohhnn FFiinnllaayy
        Wrote a utility for programming EEPROMs on 3c90x in situ.


     NNiicckk LLooppeezz
        Contributed change to tulip.c to handle Macronix 98715 (Tulip
        clone).


     MMaatttt HHoorrttmmaann
        Contributed fix to eepro100 driver that fixes incorrect latency
        setting. Also Makefile rule for .lzfd0.


     MMaarrttyy CCoonnnnoorr
        Contributed new Tulip driver ntulip.c. Reduced RTL8139
        footprint. Added support for Netgear FA310TX (Tulip clone,
        LC82C168 chip).  Support for 3Com905C. Romutil for 905C, which
        have block erase EEPROMs.  Contributed the development of
        liloprefix.S through thinguin.org.


     AAnnddeerrss LLaarrsseenn
        contributed mkQNXnbi, for generating tagged images from QNX
        kernels.


     BBeerrnndd WWiieebbeelltt
        contributed code to request vendor tags in DHCP.


     PPaaoolloo MMaarriinnii
        contributed the Via-Rhine driver.


     AAddaamm FFrriittzzlleerr
        contributed 3c529 (MCA version of 3c509) support in driver.


     SShhuussuukkee NNiissiiyyaammaa
        contributed a 3c595 (may work for 3c590) driver.


     IIggoorr VV.. KKoovvaalleennkkoo
        contributed a Winbind W89C840 driver.


     GGaarryy BByyeerrss
        of thinguin.org wrote the LILO prefix program liloprefix.S.



  1111..  CCooppyyrriigghhtt


  The boot code from FreeBSD is under the BSD license. The code taken
  from the Linux PCI subsystem and Linux NIC drivers are under GPL. Some
  source files have been put under GPL by their authors.  Hence the
  Etherboot distribution is in general under the GPL, but you may use
  parts of it derived from FreeBSD under FreeBSD rules.  Simply
  speaking, the GPL says that if you distribute a binary derived from
  Etherboot code (this includes boot ROMs) you have to provide, or
  promise to provide on demand, the source code.  The full conditions of
  the GPL are specified in the file COPYING.


  Here are the copyright details, file by file:

































































  ______________________________________________________________________
  Files marked GPL may be used under the GPL. Files marked BSD may be used
  under the GPL or BSD license. For other licenses see the respective source
  for details. The BSD files are those inherited from FreeBSD netboot. The
  GPL files are in general either from Linux or have been explicitly put
  under GPL by the authors.

  File                    Copyright status

  3c509.c                 BSD
  3c509.h                 BSD
  3c595.c                 BSD
  3c595.h                 BSD
  3c90x.c                 Open Source
  ansiesc.c               GPL
  bootmenu.c              BSD
  cards.h                 GPL
  comload.S               GPL
  config.c                GPL
  cs89x0.c                GPL
  cs89x0.h                GPL
  davicom.c               GPL
  depca.c                 GPL
  eepro100.c              GPL
  epic100.c               None
  epic100.h               None
  etherboot.h             GPL
  floppy.c                GPL
  floppyload.S            GPL
  i82586.c                GPL
  lance.c                 GPL
  liloprefix.S            GPL
  linux-asm-io.h          GPL
  linux-asm-string.h      None
  loader.S                BSD/GPL?
  main.c                  BSD
  makerom.c               BSD
  md5.c                   GPL
  misc.c                  BSD
  nfs.c                   GPL
  ni5010.c                GPL
  nic.h                   GPL
  ns8390.c                BSD
  ns8390.h                BSD
  osdep.h                 BSD
  osloader.c              GPL
  otulip.c                BSD
  otulip.h                BSD
  pci.c                   GPL
  pci.h                   GPL
  rtl8139.c               GPL
  serial.S                Mach
  sk_g16.c                GPL
  sk_g16.h                GPL
  skel.c                  GPL
  smc9000.c               GPL
  smc9000.h               GPL
  start16.S               GPL
  start32.S               BSD
  tiara.c                 GPL
  timer.c                 GPL
  timer.h                 GPL
  tulip.c                 BSD
  via-rhine.c             GPL
  w89c840.c               GPL

  This lengthy notice is required for serial.S:

  /*
   * Mach Operating System
   * Copyright (c) 1992, 1991 Carnegie Mellon University
   * All Rights Reserved.
   *
   * Permission to use, copy, modify and distribute this software and its
   * documentation is hereby granted, provided that both the copyright
   * notice and this permission notice appear in all copies of the
   * software, derivative works or modified versions, and any portions
   * thereof, and that both notices appear in supporting documentation.
   *
   * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
   * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
   * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
   *
   * Carnegie Mellon requests users of this software to return to
   *
   *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
   *  School of Computer Science
   *  Carnegie Mellon University
   *  Pittsburgh PA 15213-3890
   *
   * any improvements or extensions that they make and grant Carnegie Mellon
   * the rights to redistribute these changes.
   *
   *      from: Mach, Revision 2.2  92/04/04  11:34:26  rpd
   *      $Id: patch-ad,v 1.1.1.1 1999/04/16 21:44:22 erich Exp $
   */

  /*
    Copyright 1988, 1989, 1990, 1991, 1992
     by Intel Corporation, Santa Clara, California.

                  All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
  its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appears in all
  copies and that both the copyright notice and this permission notice
  appear in supporting documentation, and that the name of Intel
  not be used in advertising or publicity pertaining to distribution
  of the software without specific, written prior permission.

  INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
  IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
  LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
  NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
  WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
  */

  /*
   * Serial bootblock interface routines
   * Copyright (c) 1994, J"org Wunsch
   *
   * Permission to use, copy, modify and distribute this software and its
   * documentation is hereby granted, provided that both the copyright
   * notice and this permission notice appear in all copies of the
   * software, derivative works or modified versions, and any portions
   * thereof, and that both notices appear in supporting documentation.
   *
   * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
   * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
   * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
   */
  ______________________________________________________________________





  Send changes to this document to Ken Yap
  <mailto:ken_yap@users.sourceforge.net>.
























































