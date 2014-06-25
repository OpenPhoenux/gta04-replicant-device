#!/bin/sh

# Replicant GTA04 installer
#
# Copyright (C) 2012-2013 Paul Kocialkowski, GPLv2
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

#
# Env vars
#

export LC_ALL=C

#
# Global vars
#

DRIVE=""
DRIVE_NAME=""
DRIVE_SIZE=""
DRIVE_CYLINDERS=""
DRIVE_PART=""

FILES_BASE="./"
MOUNT_BASE="/media"

#
# Functions
#

# Display

display_banner() {
	echo "Replicant GTA04 installer"
	echo ""
}

display_help() {
	echo "Usage: $0 [FILES_BASE] [DRIVE]"
	echo ""
	echo "Arguments:"
	echo "- The [FILES_BASE] argument is the path to the files"
	echo "  The following files must be in that directory:"
	echo "  MLO, u-boot.bin, splash.rgb16z, boot.scr, boot.img, system.tar.bz2"
	echo "- The [DRIVE] argument is the sdcard drive node and can be omitted"
}

# Drive

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

drive_select_confirm() {
	if [ "$DRIVE" = "" ]
	then
		echo "Wrong drive block"
		exit 1
	fi

	list=$( mount | grep $DRIVE | sed "s|$DRIVE[0-9]* on \([^ ]*\) .*|\1|g" )

	echo "This is going to erase the following drive:"

	if [ "$DRIVE_NAME" != "" ]
	then
		echo "- $DRIVE_NAME ($DRIVE)"
	else
		echo "- $DRIVE"
	fi

	for mount_point in $list
	do
		mount_point_dir=$( dirname $mount_point )
		if [ "$mount_point_dir" != "/media" ] && [ "$mount_point_dir" != "/mnt" ] && [ "$mount_point" != "/media" ] && [ "$mount_point" != "/mnt" ]
		then
			echo ""
			echo "Warning: the drive is mounted as $mount_point!"
			echo "This is probably not the drive you want to use!"
		fi
	done

	echo ""
	echo -n "Are you sure? [Y/N] "
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
	list=$( mount | grep $DRIVE | sed "s|$DRIVE[0-9]* on \([^ ]*\) .*|\1|g" )

	for mount_point in $list
	do
		echo "Unmounting $mount_point"

		umount "$mount_point"
		if [ $? != 0 ]
		then
			echo "Unmounting $mount_point failed, arborting!"
			exit 1
		fi
	done
}

drive_empty() {
	# Backup
	dd if="$DRIVE" of=".drive_start_backup" bs=1024 count=1024

	dd if=/dev/zero of="$DRIVE" bs=1024 count=1024
	if [ $? != 0 ]
	then
		echo "Emptying the drive failed, aborting!"
		exit 1
	fi
}

drive_rescue() {
	if [ -f ".drive_start_backup" ]
	then
		echo -n "Something went wrong, do you want to restore drive start backup? [Y/N] "
		if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]
		then
			dd if=".drive_start_backup" of="$DRIVE" bs=1024 count=1024
		fi
	fi
}

drive_infos_get() {
	DRIVE_SIZE=$( fdisk -l "$DRIVE" | grep Disk | grep bytes | awk '{print $5}' )
	DRIVE_CYLINDERS=$( echo "$DRIVE_SIZE/255/63/512" | bc )
}

drive_eject() {
	rm -rf ".drive_start_backup"
	eject "$DRIVE"
}

drive_partitions_set() {
	boot_size=$( echo "(50 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )
	system_size=$( echo "(250 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )
	cache_size=$( echo "(100 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )

	{
		echo ",$boot_size,c,*"
		echo ",$system_size,83,-"
		echo ",$cache_size,83,-"
		echo ",,83,-"
	} | sfdisk -D -H 255 -S 63 -C "$DRIVE_CYLINDERS" "$DRIVE"
	if [ $? != 0 ]
	then
		drive_rescue
		exit 1
	fi

	sleep 1

	if [ -e "${DRIVE}p1" ]
	then
		DRIVE_PART="${DRIVE}p"
	else
		DRIVE_PART="${DRIVE}"
	fi

	mkfs.vfat -F 32 -n "boot" "${DRIVE_PART}1"
	mkfs.ext2 -L "system" "${DRIVE_PART}2"
	mkfs.ext2 -L "cache" "${DRIVE_PART}3"
	mkfs.ext2 -L "data" "${DRIVE_PART}4"

	sleep 1
}

drive_write() {
	echo "Writing boot files"

	mkdir -p "$MOUNT_BASE/boot"
	mount "${DRIVE_PART}1" "$MOUNT_BASE/boot"

	cp $FILES_BASE/MLO "$MOUNT_BASE/boot/"
	cp $FILES_BASE/u-boot.bin "$MOUNT_BASE/boot/"
	cp $FILES_BASE/splash.rgb16z "$MOUNT_BASE/boot"
	cp $FILES_BASE/boot.scr "$MOUNT_BASE/boot"
	cp $FILES_BASE/boot.img "$MOUNT_BASE/boot"

	if [ -f "$MOUNT_BASE/boot/splash.rgb16z" ]
	then
		gunzip < "$MOUNT_BASE/boot/splash.rgb16z" > "$MOUNT_BASE/boot/splash.rgb16"
	fi

	dir=$( pwd )
	echo "Syncing boot files"
	cd "$MOUNT_BASE/boot"
	sync
	cd "$dir"

	umount "$MOUNT_BASE/boot"
	rmdir "$MOUNT_BASE/boot"

	echo "Writing system files"

	mkdir -p "$MOUNT_BASE/system"
	mount "${DRIVE_PART}2" "$MOUNT_BASE/system"

	tar -p -xf "system.tar.bz2" -C "$MOUNT_BASE/system/" --strip-components=1 "system/"
	if [ $? != 0 ]
	then
		umount "$MOUNT_BASE/system"
		rmdir "$MOUNT_BASE/system"

		exit 1
	fi

	dir=$( pwd )
	echo "Syncing system files"
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

# Script start

if [ $# -eq 0 ] || [ $# -gt 2 ] || [ "$1" = "--help" ] || [ "$1" = "help" ]
then
	display_help
	exit 1
fi

if [ $# -eq 1 ]
then
	FILES_BASE=$1
fi

if [ $# -eq 2 ]
then
	FILES_BASE=$1
	DRIVE=$2
fi

display_banner

if [ "$DRIVE" = "" ]
then
	drive_select
fi

# Drive
drive_select_confirm
drive_umount
drive_empty
drive_infos_get

drive_partitions_set
drive_write

# Finishing
display_end
drive_eject
