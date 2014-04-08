/**
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2012 Paul Kocialkowski <contact@paulk.fr>
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define LOG_TAG "RIL-DEV"
#include <utils/Log.h>

#include "dream_sapphire.h"
#include <hayes-ril.h>

int dream_sapphire_boot(void *sdata)
{
	struct stat smd_stat;
	int rc;

	// Just check if SMD device was correctly created
	rc = stat(SMD_DEV, &smd_stat);
	if (rc < 0)
		return -1;

	return 0;
}

int dream_sapphire_transport_sdata_create(void **sdata)
{
	struct dream_sapphire_transport_data *transport_data = NULL;

	transport_data = malloc(sizeof(struct dream_sapphire_transport_data));
	memset(transport_data, 0, sizeof(struct dream_sapphire_transport_data));

	*sdata = (void *) transport_data;

	return 0;
}

int dream_sapphire_transport_sdata_destroy(void *sdata)
{
	if (sdata == NULL)
		return 0;

	free(sdata);

	return 0;
}

int dream_sapphire_transport_open(void *sdata)
{
	struct dream_sapphire_transport_data *transport_data = NULL;
	int fd = -1;

	struct termios term;
	int rc;

	if (sdata == NULL)
		return -1;

	transport_data = (struct dream_sapphire_transport_data *) sdata;

	fd = open(SMD_DEV, O_RDWR);

	if (fd < 0)
		return -1;

	/* disable echo on serial ports */
	rc = tcgetattr(fd, &term);
	if (rc < 0)
		return -1;

	term.c_lflag = 0;
	rc = tcsetattr(fd, TCSANOW, &term);
	if (rc < 0)
		return -1;

	transport_data->fd = fd;

	return 0;
}

int dream_sapphire_transport_close(void *sdata)
{
	struct dream_sapphire_transport_data *transport_data = NULL;

	if (sdata == NULL)
		return -1;

	transport_data = (struct dream_sapphire_transport_data *) sdata;

	if (transport_data->fd < 0)
		return -1;

	close(transport_data->fd);

	return 0;
}

int dream_sapphire_transport_send(void *sdata, void *data, int length)
{
	struct dream_sapphire_transport_data *transport_data = NULL;

	int wc;
	int mc; // Min count
	int tc = 0; // Total count of written data

	if (sdata == NULL || data == NULL || length <= 0)
		return -1;

	transport_data = (struct dream_sapphire_transport_data *) sdata;

	if (transport_data->fd < 0)
		return -1;

	mc = length;
	while (mc > 0) {
		wc = write(transport_data->fd, data + (length - mc), mc);

		if (wc < 0)
			return -1;

		tc += wc;
		mc -= wc;
	}

	return tc;
}

int dream_sapphire_transport_recv(void *sdata, void **data, int length)
{
	struct dream_sapphire_transport_data *transport_data = NULL;
	int l = READ_MAX_DATA_LEN;
	void *d = NULL;

	int rc;
	int mc = 1; // Min count is 1 byte so whatever we read is fine (even 1 byte)
	int tc = 0; // Total count of read data

	if (sdata == NULL)
		return -1;

	transport_data = (struct dream_sapphire_transport_data *) sdata;

	if (transport_data->fd < 0)
		return -1;

	// The amount of data to read is fixed
	if (length > 0) {
		l = length;
		mc = l;
	}

	d = malloc(l);
	memset(d, 0, l);

	while (mc > 0) {
		rc = read(transport_data->fd, d + tc, l - tc);

		if (rc < 0)
			return -1;

		tc += rc;
		mc -= rc;
	}

	*data = malloc(tc);
	memset(*data, 0, tc);

	memcpy(*data, d, tc);

	free(d);

	return tc;
}

int dream_sapphire_dummy(void *sdata)
{
	return 0;
}

struct ril_device_power_handlers dream_sapphire_power_handlers = {
	.sdata = NULL,
	.sdata_create = dream_sapphire_dummy,
	.sdata_destroy = dream_sapphire_dummy,
	.power_on = dream_sapphire_dummy,
	.power_off = dream_sapphire_dummy,
	.suspend = dream_sapphire_dummy,
	.resume = dream_sapphire_dummy,
	.boot = dream_sapphire_boot,
};

struct ril_device_transport_handlers dream_sapphire_transport_handlers = {
	.sdata = NULL,
	.sdata_create = dream_sapphire_transport_sdata_create,
	.sdata_destroy = dream_sapphire_transport_sdata_destroy,
	.open = dream_sapphire_transport_open,
	.close = dream_sapphire_transport_close,
	.send = dream_sapphire_transport_send,
	.recv = dream_sapphire_transport_recv,
};

struct ril_device_handlers dream_sapphire_handlers = {
	.power = &dream_sapphire_power_handlers,
	.transport = &dream_sapphire_transport_handlers,
};

struct ril_device dream_sapphire_device = {
	.name = "HTC Dream/HTC Magic",
	.type = DEV_GSM,
	.sdata = NULL,
	.handlers = &dream_sapphire_handlers,
};

void ril_device_register(struct ril_device **ril_device_p)
{
	*ril_device_p = &dream_sapphire_device;
}
