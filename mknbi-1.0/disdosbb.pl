#!/usr/bin/perl -w
if ($#ARGV >= 0) {
	open(STDIN, "$ARGV[0]") or die "$ARGV[0]: $!\n";
}
$nread = read(STDIN, $params, 0x3e);
(defined($nread) and $nread == 0x3e) or die "Cannot read 0x3e bytes of boot block\n";
(undef,
	$oem_name,
	$bytes_per_sector,
	$sectors_per_cluster,
	$reserved_sectors,
	$fat_copies,
	$root_dir_entries,
	$total_disk_sectors,
	$media_descriptor,
	$sectors_per_fat,
	$sectors_per_track,
	$sides,
	$hidden_sectors_low,
	$hidden_sectors_high,
	$total_num_sectors,
	$phys_drive_number_1,
	$phys_drive_number_2,
	$boot_record_sig,
	$vol_serial_num,
	$volume_label,
	$file_system_id) = unpack("A3a8vCvCvvCvvvvvVCCCVa11a8", $params);
print <<EOF;
oem_name: $oem_name
bytes_per_sector: $bytes_per_sector
sectors_per_cluster: $sectors_per_cluster
reserved_sectors: $reserved_sectors
fat_copies: $fat_copies
root_dir_entries: $root_dir_entries
total_disk_sectors: $total_disk_sectors
media_descriptor: $media_descriptor
sectors_per_fat: $sectors_per_fat
sectors_per_track: $sectors_per_track
sides: $sides
hidden_sectors_low: $hidden_sectors_low
hidden_sectors_high: $hidden_sectors_high
total_num_sectors: $total_num_sectors
phys_drive_number_1: $phys_drive_number_1
phys_drive_number_2: $phys_drive_number_2
boot_record_sig: $boot_record_sig
vol_serial_num: $vol_serial_num
volume_label: $volume_label
file_system_id: $file_system_id
EOF
