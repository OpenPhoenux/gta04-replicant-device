#!/bin/sh

# Replicant GTA04 Install script
#
# Copyright (C) 2012 Paul Kocialkowski, GPLv2
#
# Based on mkcard.sh v0.5
# Copyright (C) 2009 Graeme Gregory <dp@xora.org.uk>, GPLv2
# Parts of the procudure base on the work of Denys Dmytriyenko
# http://wiki.omap.com/index.php/MMC_Boot_Format
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

# Env vars

export LC_ALL=C

# Global vars

DRIVE=""
DRIVE_NAME=""
DRIVE_SIZE=""
DRIVE_CYLINDERS=""
DRIVE_PART=""

SYSTEM_AR=""
MOUNT_BASE="/media"

# Functions

display_banner() {
	echo "Replicant GTA04 installer"
	echo ""
}

display_help() {
	echo "Usage: $0 [DRIVE] [SYSTEM]"
	echo ""
	echo "Notes: "
	echo "- The [DRIVE] argument can be omitted"
	echo "- The [SYSTEM] argument must be a tar archive"
}

drive_select_list()
{
	drives_dir="/dev/disk/by-id/"
	count=0

	list=$( ls "$drives_dir" )

	for drive in $list
	do
		drive_nopart=$( echo "$drive" | sed "s/-part[0-9]*$//g" )
		if [ "$drive" = "$drive_nopart" ]
		then
			ls $drives_dir/$drive-part* 2> /dev/null > /dev/null

			if [ $? -eq 0 ]
			then
				count=$(( $count + 1 ))
				name=$( echo "$drive" | sed "s/^[^-]*-\(.*\)$/\1/g" )

				case "$1" in
					"show")
						echo "$count - $name"
					;;
					"store")
						if [ $count -eq $2 ]
						then
							DRIVE=$( readlink -f "$drives_dir/$drive" )
							DRIVE_NAME="$name"
						fi
					;;
				esac
			fi
		fi
	done 
}

drive_select()
{
	echo "Available devices:"
	drive_select_list "show"

	echo -n "Choice: "
	read choice

	drive_select_list "store" $choice
	echo ""
}

drive_write_confirm() {
	if [ "$DRIVE" = "" ]
	then
		echo "Wrong drive block"
		exit 1
	fi

	echo "This is going to erase the following drive:"

	if [ "$DRIVE_NAME" != "" ]
	then
		echo "- $DRIVE_NAME ($DRIVE)"
	else
		echo "- $DRIVE"
	fi

	echo ""
	echo -n "Are you sure? "
	read confirm

	if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]
	then
		echo ""
		return
	else
		exit 0
	fi
}

drive_umount() {
	umount $DRIVE* 2> /dev/null
}

drive_empty() {
	dd if=/dev/zero of="$DRIVE" bs=1024 count=1024
}

drive_infos_get() {
	DRIVE_SIZE=$( fdisk -l "$DRIVE" | grep Disk | grep bytes | awk '{print $5}' )
	DRIVE_CYLINDERS=$( echo "$DRIVE_SIZE/255/63/512" | bc )
}

drive_partitions_set() {
	system_size=$( echo "(250 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )
	data_size=$( echo "(250 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )
	cache_size=$( echo "(100 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )

	{
		echo ",$system_size,83"
		echo ",$data_size,83"
		echo ",$cache_size,83"
		echo ",,0c"
	} | sfdisk -D -H 255 -S 63 -C "$DRIVE_CYLINDERS" "$DRIVE"

	mkfs.ext2 -L "system" "${DRIVE}1"
	mkfs.ext2 -L "data" "${DRIVE}2"
	mkfs.ext2 -L "cache" "${DRIVE}3"
	mkfs.vfat -F 32 -n "storage" "${DRIVE}4"
}

system_write() {
	echo "Writing system files"

	mkdir -p "$MOUNT_BASE/system"
	mount "${DRIVE}1" "$MOUNT_BASE/system"
	tar -p -xf $SYSTEM_AR -C "$MOUNT_BASE/system" "system"

	dir=$( pwd )
	echo "Syncing files"
	cd "$MOUNT_BASE/system"
	sync
	cd "$dir"

	umount "$MOUNT_BASE/system"
	rmdir "$MOUNT_BASE/system"
}

display_end() {
	echo ""
	echo "Your drive is now ready to be used!"
}

drive_eject() {
	eject "$DRIVE"
}

# Script start

if [ $# -eq 0 ]
then
	display_help
	exit 1
fi

if [ $# -eq 1 ]
then
	SYSTEM_AR=$1
fi

if [ $# -eq 2 ]
then
	DRIVE=$1
	SYSTEM_AR=$2
fi

display_banner

if [ "$DRIVE" = "" ]
then
	drive_select
fi

drive_write_confirm
drive_umount
drive_empty
drive_infos_get
drive_partitions_set
system_write
display_end
drive_eject
