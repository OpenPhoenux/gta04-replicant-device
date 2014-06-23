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
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#define LOG_TAG "gta04_bt_vendor"
#include <cutils/log.h>

#include <bt_vendor_lib.h>
#include <bt_hci_bdroid.h>

#include "gta04_bt_vendor.h"

int gta04_bt_vendor_hci_cmd_prepare(void *buffer, size_t length)
{
	HC_BT_HDR *header;

	if (buffer == NULL || length < BT_HC_HDR_SIZE)
		return -EINVAL;

	header = (HC_BT_HDR *) buffer;
	header->event = MSG_STACK_TO_HC_HCI_CMD;
	header->len = length - BT_HC_HDR_SIZE;
	header->offset = 0;
	header->layer_specific = 0;

	return 0;
}

int gta04_bt_vendor_hci_preamble(void *buffer, size_t length,
	unsigned short cmd)
{
	unsigned char *p;

	if (buffer == NULL || length < HCI_CMD_PREAMBLE_SIZE)
		return -EINVAL;

	p = (unsigned char *) buffer;

	p[0] = cmd & 0xff;
	p[1] = cmd >> 8;
	p[2] = length - HCI_CMD_PREAMBLE_SIZE;

	return 0;
}

int gta04_bt_vendor_hci_bccmd(void *buffer, size_t length,
	unsigned short pdu, unsigned short varid)
{
	static unsigned bccmd_seq = 0;
	size_t bccmd_words_count = HCI_BCCMD_WORDS_COUNT;
	size_t bccmd_length = bccmd_words_count * sizeof(unsigned short) + 1;
	unsigned char *p;

	if (buffer == NULL || length < bccmd_length)
		return -EINVAL;

	memset(buffer, 0, 11);

	bccmd_words_count = (length - 1) / 2;

	p = (unsigned char *) buffer;

	// Descriptor
	p[0] = HCI_BCCMD_DESCRIPTOR;

	// Payload
	p[1] = pdu & 0xff;
	p[2] = pdu >> 8;
	p[3] = bccmd_words_count & 0xff;
	p[4] = bccmd_words_count >> 8;
	p[5] = bccmd_seq & 0xff;
	p[6] = bccmd_seq >> 8;
	p[7] = varid & 0xff;
	p[8] = varid >> 8;
	p[9] = HCI_BCCMD_STAT_OK & 0xff;
	p[10] = HCI_BCCMD_STAT_OK >> 8;

	bccmd_seq++;

	return 0;
}

unsigned short gta04_bt_vendor_hci_bccmd_speed(speed_t speed)
{
	switch (speed) {
		case B0:
		case B50:
		case B75:
		case B110:
		case B134:
		case B150:
		case B200:
			return 0;
		case B300:
			return 1;
		case B600:
			return 3;
		case B1200:
			return 5;
		case B1800:
			return 7;
		case B2400:
			return 10;
		case B4800:
			return 20;
		case B9600:
			return 39;
		case B19200:
			return 79;
		case B38400:
			return 157;
		case B57600:
			return 236;
		case B115200:
			return 472;
		case B230400:
			return 944;
		case B460800:
			return 1887;
		case B500000:
			return 2048;
		case B576000:
			return 2359;
		case B921600:
			return 3775;
		case B1000000:
			return 4096;
		case B1152000:
			return 4719;
		case B1500000:
			return 6144;
		case B2000000:
			return 8192;
		case B2500000:
			return 10240;
		case B3000000:
			return 12288;
		case B3500000:
			return 14336;
		case B4000000:
			return 16384;
		default:
			return 12288;
	}
}

int gta04_bt_vendor_hci_h4_write(void *buffer, size_t length)
{
	void *data;
	size_t size;
	unsigned char *p;
	int rc;

	if (buffer == NULL || length < BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE)
		return -EINVAL;

	p = (unsigned char *) buffer + BT_HC_HDR_SIZE - 1;

	data = (void *) p;
	size = length - BT_HC_HDR_SIZE + 1;

	p[0] = HCI_H4_TYPE_CMD;

	rc = gta04_bt_vendor_serial_write(data, size);
	if (rc < 0) {
		ALOGE("%s: Writing to serial failed", __func__);
		return -1;
	}

	return 0;
}

int gta04_bt_vendor_hci_h4_read_event(void *buffer, size_t length,
	unsigned char event)
{
	struct timeval timeout;
	unsigned char cleanup[256];
	unsigned char byte;
	unsigned char type = 0;
	unsigned char size = 0;
	unsigned char *p = NULL;
	unsigned int count;
	unsigned int limit;
	int failures;
	int skip = 0;
	fd_set fds;
	int rc;

	if (gta04_bt_vendor == NULL || gta04_bt_vendor->serial_fd < 0)
		return -1;

	failures = 0;

	while (1) {
		FD_ZERO(&fds);
		FD_SET(gta04_bt_vendor->serial_fd, &fds);

		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		rc = select(gta04_bt_vendor->serial_fd + 1, &fds, NULL, NULL, &timeout);
		if (rc < 0) {
			ALOGE("%s: Polling failed", __func__);

			if (failures) {
				return -1;
			} else {
				failures++;
				continue;
			}
		} else if (rc == 0) {
			ALOGE("%s: Polling timed out", __func__);
			return -1;
		}

		if (failures > 10) {
			ALOGE("%s: Too many failures", __func__);
			return -1;
		}

		if (p == NULL) {
			// Read single bytes first

			rc = gta04_bt_vendor_serial_read(&byte, sizeof(byte));
			if (rc < (int) sizeof(byte)) {
				ALOGE("%s: Reading from serial failed", __func__);
				return -1;
			}

			if (type != HCI_H4_TYPE_EVENT) {
				if (byte != HCI_H4_TYPE_EVENT) {
					ALOGD("%s: Ignored response with type: %d", __func__, byte);
					gta04_bt_vendor_serial_read(&cleanup, sizeof(cleanup));
					continue;
				}

				type = byte;
			} else {
				if (byte != event) {
					ALOGD("%s: Ignored response with event: 0x%x", __func__, byte);
					gta04_bt_vendor_serial_read(&cleanup, sizeof(cleanup));
					type = 0;
					continue;
				}

				rc = gta04_bt_vendor_serial_read(&size, sizeof(size));
				if (rc <= 0) {
					ALOGE("%s: Reading from serial failed", __func__);
					failures++;
					continue;
				} else if (failures) {
					failures = 0;
				}


				if (size == 0)
					return 0;

				if (buffer == NULL || length == 0) {
					count = 0;

					while (count < size) {
						rc = gta04_bt_vendor_serial_read(&cleanup, size - count);
						if (rc <= 0) {
							ALOGE("%s: Reading from serial failed", __func__);
							failures++;
							break;
						} else if (failures) {
							failures = 0;
						}

						count += rc;
					}

					if (rc <= 0)
						continue;

					return 0;
				}

				p = (unsigned char *) buffer;
			}
		}

		if (p != NULL) {
			// Make sure to read from the start in case of failure
			p = (unsigned char *) buffer;

			if (size > length) {
				ALOGE("%s: Provided buffer length is too small for size: %d/%d bytes", __func__, length, size);

				limit = length;
			} else {
				limit = size;
			}

			count = 0;

			while (count < limit) {
				rc = gta04_bt_vendor_serial_read(p, limit - count);
				if (rc <= 0) {
					ALOGE("%s: Reading from serial failed", __func__);
					failures++;
					break;
				} else if (failures) {
					failures = 0;
				}

				p += rc;
				count += rc;
			}

			if (rc <= 0)
				continue;

			if (limit < size) {
				limit = size;

				while (count < limit) {
					rc = gta04_bt_vendor_serial_read(&cleanup, limit - count);
					if (rc <= 0) {
						ALOGE("%s: Reading from serial failed", __func__);
						failures++;
						break;
					} else if (failures) {
						failures = 0;
					}

					count += rc;
				}
			}

			if (rc <= 0)
				continue;

			return 0;
		}
	}

	return 0;
}
