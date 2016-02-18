/*
 * Copyright (C) 2014 Paul Kocialkowski <contact@paulk.fr>
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

#include <stdlib.h>
#include <termios.h>

#include <bt_vendor_lib.h>

#ifndef _GTA04_BT_VENDOR_H_
#define _GTA04_BT_VENDOR_H_

/*
 * Values
 */

#define HCI_H4_TYPE_CMD						1
#define HCI_H4_TYPE_ACL_DATA					2
#define HCI_H4_TYPE_SCO_DATA					3
#define HCI_H4_TYPE_EVENT					4

#define HCI_CMD_VENDOR						0xFC00
#define HCI_CMD_RESET						0x0C03
#define HCI_CMD_LOCAL_VERSION_INFO				0x1001

#define HCI_CMD_PREAMBLE_SIZE					3

#define HCI_EVENT_VENDOR					0xFF
#define HCI_EVENT_COMMAND_COMPLETE				0x0E

#define HCI_BCCMD_PDU_GETREQ					0x0000
#define HCI_BCCMD_PDU_GETRESP					0x0001
#define HCI_BCCMD_PDU_SETREQ					0x0002
#define HCI_BCCMD_STAT_OK					0x0000

#define HCI_BCCMD_DESCRIPTOR					0xC2
#define HCI_BCCMD_WORDS_COUNT					9

#define HCI_BCCMD_STORES_PSI					0x0001
#define HCI_BCCMD_STORES_PSF					0x0002
#define HCI_BCCMD_STORES_PSROM					0x0004
#define HCI_BCCMD_STORES_PSRAM					0x0008

#define HCI_BCCMD_VARID_WARM_RESET				0x4002
#define HCI_BCCMD_VARID_PS					0x7003
#define HCI_BCCMD_PSKEY_UART_BAUDRATE				0x01BE

/*
 * Structures
 */

struct gta04_bt_vendor {
	bt_vendor_callbacks_t *callbacks;

	int serial_fd;
};

/*
 * Globals
 */

extern const char serial_path[];
extern speed_t serial_init_speed;
extern speed_t serial_work_speed;

extern struct gta04_bt_vendor *gta04_bt_vendor;

/*
 * Declarations
 */

// GTA04 BT Vendor

int gta04_bt_vendor_serial_open(void);
int gta04_bt_vendor_serial_close(void);
int gta04_bt_vendor_serial_read(void *buffer, size_t length);
int gta04_bt_vendor_serial_write(void *buffer, size_t length);
int gta04_bt_vendor_serial_work_speed(void);

// HCI

int gta04_bt_vendor_hci_cmd_prepare(void *buffer, size_t length);
int gta04_bt_vendor_hci_preamble(void *buffer, size_t length,
	unsigned short cmd);
int gta04_bt_vendor_hci_bccmd(void *buffer, size_t length,
	unsigned short pdu, unsigned short varid);
unsigned short gta04_bt_vendor_hci_bccmd_speed(speed_t speed);
int gta04_bt_vendor_hci_h4_write(void *buffer, size_t length);
int gta04_bt_vendor_hci_h4_read_event(void *buffer, size_t length,
	unsigned char event);

#endif
