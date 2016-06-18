/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2012-2013 Paul Kocialkowski <contact@paulk.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <hayes-ril.h>

#ifndef _HAYES_RIL_GTA04_H
#define _HAYES_RIL_GTA04_H

#define TTY_SYSFS_BASE	"/sys/class/tty"
#define TTY_DEV_BASE	"/dev"
#define TTY_HSOTYPE	"hsotype"
#define TTY_NODE_BASE	"ttyHS"
#define TTY_NODE_MAX	6 * 6
#define GPIO_SYSFS	"/sys/class/gpio/gpio186/value"

struct gta04_transport_data {
	// File descriptors
	int modem_fd;
	int application_fd;

	// Available data counts
	int modem_ac;
	int application_ac;
};

// from linux/rfkill.h
#define RFKILL_OP_CHANGE_ALL	3
#define RFKILL_TYPE_WWAN	5
struct rfkill_event {
	uint32_t idx;
	uint8_t  type;
	uint8_t  op;
	uint8_t  soft, hard;
};

//from libgta04
extern int is_gta04a3();

#endif
