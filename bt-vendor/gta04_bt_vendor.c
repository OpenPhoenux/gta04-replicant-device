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

/*
 * Globals
 */

extern int is_gta04a3(); //from libgta04
extern int is_gta04a4(); //from libgta04

const char serial_path[] = "/dev/ttyO0";
speed_t serial_init_speed;
speed_t serial_work_speed;

struct gta04_bt_vendor *gta04_bt_vendor = NULL;

/*
 * Serial
 */

int gta04_bt_vendor_serial_open(void)
{
	struct termios termios;
	int serial_fd = -1;
	int rc;

	if (gta04_bt_vendor == NULL)
		return -1;

	serial_fd = open(serial_path, O_RDWR | O_NONBLOCK);
	if (serial_fd < 0) {
		ALOGE("%s: Opening serial failed", __func__);
		goto error;
	}

	tcflush(serial_fd, TCIOFLUSH);

	rc = tcgetattr(serial_fd, &termios);
	if (rc < 0) {
		ALOGE("%s: Getting serial attributes failed", __func__);
		goto error;
	}

	cfmakeraw(&termios);

	cfsetispeed(&termios, serial_init_speed);
	cfsetospeed(&termios, serial_init_speed);

	rc = tcsetattr(serial_fd, TCSANOW, &termios);
	if (rc < 0) {
		ALOGE("%s: Setting serial attributes failed", __func__);
		goto error;
	}

	tcflush(serial_fd, TCIOFLUSH);

	gta04_bt_vendor->serial_fd = serial_fd;

	rc = 1;
	goto complete;

error:
	if (serial_fd >= 0)
		close(serial_fd);

	gta04_bt_vendor->serial_fd = -1;

	rc = -1;

complete:
	return rc;
}

int gta04_bt_vendor_serial_close(void)
{
	if (gta04_bt_vendor == NULL)
		return -1;

	if (gta04_bt_vendor->serial_fd < 0)
		return 0;

	close(gta04_bt_vendor->serial_fd);
	gta04_bt_vendor->serial_fd = -1;

	return 0;
}

int gta04_bt_vendor_serial_read(void *buffer, size_t length)
{
	if (buffer == NULL || length == 0)
		return -EINVAL;

	if (gta04_bt_vendor == NULL || gta04_bt_vendor->serial_fd < 0)
		return -1;

	return read(gta04_bt_vendor->serial_fd, buffer, length);
}

int gta04_bt_vendor_serial_write(void *buffer, size_t length)
{
	unsigned int count;
	unsigned char *p;
	int rc;

	if (buffer == NULL || length == 0)
		return -EINVAL;

	if (gta04_bt_vendor == NULL || gta04_bt_vendor->serial_fd < 0)
		return -1;

	p = (unsigned char *) buffer;
	count = 0;

	while (count < length) {
		rc = write(gta04_bt_vendor->serial_fd, p, length - count);
		if (rc <= 0)
			return -1;

		p += rc;
		count += rc;
	}

	return count;
}

int gta04_bt_vendor_serial_work_speed(void)
{
	struct termios termios;
	int rc;

	if (gta04_bt_vendor == NULL || gta04_bt_vendor->serial_fd < 0)
		return -1;

	tcflush(gta04_bt_vendor->serial_fd, TCIOFLUSH);

	rc = tcgetattr(gta04_bt_vendor->serial_fd, &termios);
	if (rc < 0) {
		ALOGE("%s: Getting serial attributes failed", __func__);
		return -1;
	}

	cfsetispeed(&termios, serial_work_speed);
	cfsetospeed(&termios, serial_work_speed);

	rc = tcsetattr(gta04_bt_vendor->serial_fd, TCSANOW, &termios);
	if (rc < 0) {
		ALOGE("%s: Setting serial attributes failed", __func__);
		return -1;
	}

	tcflush(gta04_bt_vendor->serial_fd, TCIOFLUSH);

	return 0;
}

/*
 * Bluetooth Interface
 */

int gta04_bt_vendor_init(const bt_vendor_callbacks_t *callbacks,
	unsigned char *local_bdaddr)
{
	int rc;

	ALOGD("%s(%p, %p)", __func__, callbacks, local_bdaddr);

	if(is_gta04a3()) {
		//FIXME: it still does not work with B115200 on A3
		ALOGD("Detected GTA04a3, reducing baud rate.");
		serial_init_speed = B115200;
		serial_work_speed = B115200;
	}
	else {
		serial_init_speed = B3000000;
		serial_work_speed = B3000000;
	}

	if (callbacks == NULL || local_bdaddr == NULL)
		return -EINVAL;

	if (gta04_bt_vendor != NULL)
		free(gta04_bt_vendor);

	gta04_bt_vendor = (struct gta04_bt_vendor *) calloc(1, sizeof(struct gta04_bt_vendor));
	gta04_bt_vendor->callbacks = (bt_vendor_callbacks_t *) callbacks;

	rc = 0;
	goto complete;

error:
	if (gta04_bt_vendor != NULL) {
		free(gta04_bt_vendor);
		gta04_bt_vendor = NULL;
	}

	rc = -1;

complete:
	return rc;
}

void gta04_bt_vendor_cleanup(void)
{
	if (gta04_bt_vendor == NULL)
		return;

	free(gta04_bt_vendor);
	gta04_bt_vendor = NULL;
}

void gta04_bt_vendor_op_fw_cfg_callback(void *response)
{
	void *buffer, *data;
	size_t length, size;
	int rc;

	ALOGD("%s(%p)", __func__, response);

	if (response == NULL)
		return;

	if (gta04_bt_vendor == NULL || gta04_bt_vendor->callbacks == NULL || gta04_bt_vendor->callbacks->fwcfg_cb == NULL)
		return;

	// Perform warm reset so that the work UART speed can take effect

	length = BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + 1 + HCI_BCCMD_WORDS_COUNT * sizeof(unsigned short);
	buffer = calloc(1, length);

	gta04_bt_vendor_hci_cmd_prepare(buffer, length);

	data = (void *) ((unsigned char *) buffer + BT_HC_HDR_SIZE);
	size = length - BT_HC_HDR_SIZE;

	gta04_bt_vendor_hci_preamble(data, size, HCI_CMD_VENDOR);

	data = (void *) ((unsigned char *) buffer + BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE);
	size = length - BT_HC_HDR_SIZE - HCI_CMD_PREAMBLE_SIZE;

	gta04_bt_vendor_hci_bccmd(data, size, HCI_BCCMD_PDU_SETREQ, HCI_BCCMD_VARID_WARM_RESET);

	rc = gta04_bt_vendor_hci_h4_write(buffer, length);
	if (rc < 0) {
		ALOGE("%s: Writing HCI failed", __func__);
		return;
	}

	free(buffer);

	sleep(1);

	rc = gta04_bt_vendor_serial_work_speed();
	if (rc < 0) {
		ALOGD("%s: Switching to serial work speed failed", __func__);
		return;
	}

	ALOGD("%s: Switched to serial work speed", __func__);

	gta04_bt_vendor->callbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
}

int gta04_bt_vendor_op_fw_cfg(void *param)
{
	void *buffer, *data;
	size_t length, size;
	unsigned short speed;
	unsigned char *p;

	ALOGD("%s(%p)", __func__, param);

	if (gta04_bt_vendor == NULL || gta04_bt_vendor->callbacks == NULL || gta04_bt_vendor->callbacks->fwcfg_cb == NULL || gta04_bt_vendor->callbacks->alloc == NULL || gta04_bt_vendor->callbacks->xmit_cb == NULL)
		return -1;

	if (serial_init_speed == serial_work_speed) {
		gta04_bt_vendor->callbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
		return 0;
	}

	// Set work UART speed to PSRAM

	speed = gta04_bt_vendor_hci_bccmd_speed(serial_work_speed);

	length = BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + 1 + HCI_BCCMD_WORDS_COUNT * sizeof(unsigned short);
	buffer = gta04_bt_vendor->callbacks->alloc(length);

	memset(buffer, 0, length);

	gta04_bt_vendor_hci_cmd_prepare(buffer, length);

	data = (void *) ((unsigned char *) buffer + BT_HC_HDR_SIZE);
	size = length - BT_HC_HDR_SIZE;

	gta04_bt_vendor_hci_preamble(data, size, HCI_CMD_VENDOR);

	data = (void *) ((unsigned char *) buffer + BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE);
	size = length - BT_HC_HDR_SIZE - HCI_CMD_PREAMBLE_SIZE;

	gta04_bt_vendor_hci_bccmd(data, size, HCI_BCCMD_PDU_SETREQ, HCI_BCCMD_VARID_PS);

	p = (unsigned char *) data + 1 + 5 * sizeof(unsigned short);

	p[0] = HCI_BCCMD_PSKEY_UART_BAUDRATE & 0xff;
	p[1] = HCI_BCCMD_PSKEY_UART_BAUDRATE >> 8;
	p[2] = 0x01;
	p[3] = 0x00;
	p[4] = HCI_BCCMD_STORES_PSRAM & 0xff;
	p[5] = HCI_BCCMD_STORES_PSRAM >> 8;
	p[6] = speed & 0xff;
	p[7] = speed >> 8;

	gta04_bt_vendor->callbacks->xmit_cb(HCI_CMD_VENDOR, buffer, gta04_bt_vendor_op_fw_cfg_callback);

	return 0;
}

int gta04_bt_vendor_op_userial_open(void *param)
{
	void *buffer, *data;
	size_t length, size;
	unsigned int i;
	int *fds;
	int rc;

	ALOGD("%s(%p)", __func__, param);

	if (param == NULL)
		return -EINVAL;

	if (gta04_bt_vendor == NULL)
		return -1;

	fds = (int *) param;

	rc = gta04_bt_vendor_serial_open();
	if (rc < 0 || gta04_bt_vendor->serial_fd < 0) {
		ALOGD("%s: Opening serial failed", __func__);
		return -1;
	}

	length = BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE;
	buffer = calloc(1, length);

	gta04_bt_vendor_hci_cmd_prepare(buffer, length);

	data = (void *) ((unsigned char *) buffer + BT_HC_HDR_SIZE);
	size = length - BT_HC_HDR_SIZE;

	gta04_bt_vendor_hci_preamble(data, size, HCI_CMD_RESET);

	rc = gta04_bt_vendor_hci_h4_write(buffer, length);
	if (rc < 0) {
		ALOGE("%s: Writing HCI failed", __func__);
		return -1;
	}

	free(buffer);

	sleep(1);

	//rc = gta04_bt_vendor_hci_h4_read_event(NULL, 0, HCI_EVENT_VENDOR);
	rc = gta04_bt_vendor_hci_h4_read_event(NULL, 0, HCI_EVENT_COMMAND_COMPLETE);
	if (rc < 0) {
		ALOGE("%s: Reading HCI failed", __func__);
		return -1;
	}

	for (i = 0; i < CH_MAX; i++)
		fds[i] = gta04_bt_vendor->serial_fd;

	return 1;
}

int gta04_bt_vendor_op_userial_close(void *param)
{
	ALOGD("%s(%p)", __func__, param);

	return gta04_bt_vendor_serial_close();
}

int gta04_bt_vendor_op(bt_vendor_opcode_t opcode, void *param)
{
	ALOGD("%s(%d, %p)", __func__, opcode, param);

	switch (opcode) {
		case BT_VND_OP_POWER_CTRL:
			return 0;
		case BT_VND_OP_FW_CFG:
			return gta04_bt_vendor_op_fw_cfg(param);
		case BT_VND_OP_SCO_CFG:
			gta04_bt_vendor->callbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
			return 0;
		case BT_VND_OP_USERIAL_OPEN:
			return gta04_bt_vendor_op_userial_open(param);
		case BT_VND_OP_USERIAL_CLOSE:
			return gta04_bt_vendor_op_userial_close(param);
		case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
			*((int *) param) = 0;
			return 0;
		case BT_VND_OP_LPM_SET_MODE:
			gta04_bt_vendor->callbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
			return 0;
		default:
			return 0;
	}
}

/*
 * Interface
 */

const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
	.size = sizeof(bt_vendor_interface_t),
	.init = gta04_bt_vendor_init,
	.cleanup = gta04_bt_vendor_cleanup,
	.op = gta04_bt_vendor_op,
};
