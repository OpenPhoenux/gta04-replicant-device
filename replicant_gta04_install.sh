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

#
# Env vars
#

export LC_ALL=C

#
# Global vars
#

TYPE=""
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
	echo "Usage: $0 [TYPE] [DRIVE]"
	echo ""
	echo "Arguments:"
	echo "- The [TYPE] argument can either be:"
	echo "  * \"install\" to create an install sdcard"
	echo "  * \"system\" to create a system sdcard"
	echo "- The [DRIVE] argument is the sdcard drive node and can be omitted"
	echo ""
	echo "Notes:"
	echo "The following files must be present in the directory where you run the script:"
	echo "* bootloader.img boot.scr boot.img (install)"
	echo "* system.tar.bz2 (system)"
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
		echo -n "Something went wrong, do you want to restore drive start backup? "
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

# Install

install_partitions_set() {
	{
		echo ",,0c"
	} | sfdisk -D -H 255 -S 63 -C "$DRIVE_CYLINDERS" "$DRIVE"
	if [ $? != 0 ]
	then
		drive_rescue
		exit 1
	fi

	sleep 1

	mkfs.vfat -F 32 -n "install" "${DRIVE}1"

	sleep 1
}

install_write() {
	echo "Writing install files"

	mkdir -p "$MOUNT_BASE/install"
	mount "${DRIVE}1" "$MOUNT_BASE/install"

	cp "bootloader.img" "$MOUNT_BASE/install"
	if [ $? != 0 ]
	then
		exit 1
	fi

	cp "boot.scr" "$MOUNT_BASE/install"
	if [ $? != 0 ]
	then
		exit 1
	fi

	cp "boot.img" "$MOUNT_BASE/install"
	if [ $? != 0 ]
	then
		exit 1
	fi

	dir=$( pwd )
	echo "Syncing files"
	cd "$MOUNT_BASE/install"
	sync
	cd "$dir"

	umount "$MOUNT_BASE/install"
	rmdir "$MOUNT_BASE/install"
}

# System

system_partitions_set() {
	system_size=$( echo "(250 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )
	data_size=$( echo "(250 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )
	cache_size=$( echo "(100 * 1024 * 1024) / ($DRIVE_SIZE/$DRIVE_CYLINDERS)" | bc )

	{
		echo ",$system_size,83"
		echo ",$data_size,83"
		echo ",$cache_size,83"
		echo ",,0c"
	} | sfdisk -D -H 255 -S 63 -C "$DRIVE_CYLINDERS" "$DRIVE"
	if [ $? != 0 ]
	then
		drive_rescue
		exit 1
	fi

	sleep 1

	mkfs.ext2 -L "system" "${DRIVE}1"
	mkfs.ext2 -L "data" "${DRIVE}2"
	mkfs.ext2 -L "cache" "${DRIVE}3"
	mkfs.vfat -F 32 -n "storage" "${DRIVE}4"

	sleep 1
}

system_write() {
	echo "Writing system files"

	mkdir -p "$MOUNT_BASE/system"
	mount "${DRIVE}1" "$MOUNT_BASE/system"

	tar -p -xf "system.tar.bz2" -C "$MOUNT_BASE/system/" --strip-components=1 "system/"
	if [ $? != 0 ]
	then
		umount "$MOUNT_BASE/system"
		rmdir "$MOUNT_BASE/system"

		exit 1
	fi

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

# Script start

if [ $# -eq 0 ]
then
	display_help
	exit 1
fi

if [ $# -eq 1 ]
then
	TYPE=$1
fi

if [ $# -eq 2 ]
then
	TYPE=$1
	DRIVE=$2
fi

display_banner

if [ "$TYPE" != "install" ] && [ "$TYPE" != "system" ]
then
	display_help
	exit 1
fi

if [ "$DRIVE" = "" ]
then
	drive_select
fi

# Drive
drive_select_confirm
drive_umount
drive_empty
drive_infos_get

# Install/System
if [ "$TYPE" = "install" ]
then
	install_partitions_set
	install_write
else
	system_partitions_set
	system_write
fi

# Finishing
display_end
drive_eject
