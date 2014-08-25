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

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define LOG_TAG "RIL-DEV"
#include <utils/Log.h>

#include "gta04.h"
#include <hayes-ril.h>

int gta04_power_count_nodes(void)
{
	struct stat tty_node_stat;
	char *tty_node = NULL;
	int tty_nodes_count = 0;
	int rc;
	int i;

	// Count how many nodes are available
	for (i = 0 ; i < TTY_NODE_MAX ; i++) {
		asprintf(&tty_node, "%s/%s%d", TTY_DEV_BASE, TTY_NODE_BASE, i);

		rc = stat(tty_node, &tty_node_stat);
		if (rc == 0)
			tty_nodes_count++;

		free(tty_node);
	}

	return tty_nodes_count;
}

int gta04_power_on(void *sdata)
{
	char gpio_sysfs_value_0[] = "0\n";
	char gpio_sysfs_value_1[] = "1\n";
	int tty_nodes_count;
	int fd;
	int retry = 0;

retry:
	tty_nodes_count = gta04_power_count_nodes();
	if (tty_nodes_count < 2) {
		ALOGD("Powering modem on");

		fd = open(GPIO_SYSFS, O_RDWR);
		if (fd < 0) {
			ALOGE("Unable to open GPIO SYSFS node, modem will stay off");
			return -1;
		}

		write(fd, gpio_sysfs_value_0, strlen(gpio_sysfs_value_0));
		sleep(1);
		write(fd, gpio_sysfs_value_1, strlen(gpio_sysfs_value_1));
		sleep(3);

		tty_nodes_count = gta04_power_count_nodes();
		if(tty_nodes_count < 2 && retry < 10) {
			retry++;
			ALOGD("Powering modem on: retry %d", retry);
			goto retry;
		}
		else if(tty_nodes_count < 2 && retry >= 10) {
			ALOGE("Powering modem on didn't succeed");
			return -1;
		}

		return 0;
	}

	ALOGD("Modem is already on");
	return 0;
}

int gta04_power_off(void *sdata)
{
	char gpio_sysfs_value_0[] = "0\n";
	char gpio_sysfs_value_1[] = "1\n";
	int tty_nodes_count;
	int fd;
	int retry = 0;

retry:
	tty_nodes_count = gta04_power_count_nodes();
	if (tty_nodes_count > 0) {
		ALOGD("Powering modem off");

		fd = open(GPIO_SYSFS, O_RDWR);
		if (fd < 0) {
			ALOGE("Unable to open GPIO SYSFS node, modem will stay on");
			return -1;
		}

		write(fd, gpio_sysfs_value_1, strlen(gpio_sysfs_value_1));
		sleep(1);
		write(fd, gpio_sysfs_value_0, strlen(gpio_sysfs_value_0));
		sleep(3);

		tty_nodes_count = gta04_power_count_nodes();
		if(tty_nodes_count > 0 && retry < 10) {
			retry++;
			ALOGD("Powering modem off: retry %d", retry);
			goto retry;
		}
		else if(tty_nodes_count > 0 && retry >= 10) {
			ALOGE("Powering modem off didn't succeed");
			return -1;
		}

		return 0;
	}

	ALOGD("Modem is already off");
	return 0;
}

int gta04_power_boot(void *sdata)
{
	int tty_nodes_count;

	tty_nodes_count = gta04_power_count_nodes();
	if (tty_nodes_count < 2) {
		// We need at least Modem and Application
		ALOGE("Not enough modem nodes available!");
		return -1;
	}

	return 0;
}

int gta04_transport_sdata_create(void **sdata)
{
	struct gta04_transport_data *transport_data = NULL;

	transport_data = malloc(sizeof(struct gta04_transport_data));
	memset(transport_data, 0, sizeof(struct gta04_transport_data));

	*sdata = (void *) transport_data;

	return 0;
}

int gta04_transport_sdata_destroy(void *sdata)
{
	if (sdata != NULL)
		free(sdata);

	return 0;
}

int gta04_transport_find_node(char **tty_node, char *name)
{
	char *tty_sysfs_node = NULL;
	char *buf = NULL;
	int name_length;
	int retries = 10;
	int fd = -1;
	int i;

	if (tty_node == NULL || name == NULL)
		return -1;

	name_length = strlen(name);
	buf = calloc(1, name_length);

	while (retries) {
		for (i = 0 ; i < TTY_NODE_MAX ; i++) {
			asprintf(&tty_sysfs_node, "%s/%s%d/%s",
				TTY_SYSFS_BASE, TTY_NODE_BASE, i, TTY_HSOTYPE);

			fd = open(tty_sysfs_node, O_RDONLY);
			if (fd < 0) {
				free(tty_sysfs_node);
				continue;
			}

			read(fd, buf, name_length);
			if (strncmp(name, buf, name_length) == 0) {
				asprintf(tty_node, "%s/%s%d", TTY_DEV_BASE, TTY_NODE_BASE, i);

				free(tty_sysfs_node);
				return 0;
			} else {
				free(tty_sysfs_node);
			}
		}

		retries--;
		usleep(750000);
	}

	*tty_node = NULL;

	return -1;
}

int gta04_transport_open_node(char *dev_node)
{
	struct termios term;
	int retries = 10;
	int fd = -1;
	int rc = -1;

	while (retries) {
		fd = open(dev_node, O_RDWR | O_NOCTTY | O_NDELAY);
		if (fd < 0)
			goto failure;

		rc = tcgetattr(fd, &term);
		if (rc < 0)
			goto failure;

		term.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
		cfsetispeed(&term, B115200);
		cfsetospeed(&term, B115200);

		rc = tcsetattr(fd, TCSANOW, &term);
		if (rc < 0)
			goto failure;

		return fd;

failure:
		retries--;
		usleep(750000);
	}

	return -1;
}

int gta04_transport_open(void *sdata)
{
	struct gta04_transport_data *transport_data = NULL;
	char *dev_node = NULL;
	int fd = -1;
	int rc = -1;

	if (sdata == NULL)
		return -1;

	transport_data = (struct gta04_transport_data *) sdata;

	// Open Modem node
	if (transport_data->modem_fd <= 0) {
		rc = gta04_transport_find_node(&dev_node, "Modem");
		if (rc < 0 || dev_node == NULL) {
			ALOGE("Unable to find Modem node, aborting!");
			goto failure;
		}

		fd = gta04_transport_open_node(dev_node);
		if (fd < 0) {
			ALOGE("Unable to open Modem node, aborting!");
			goto failure;
		}

		ALOGD("Opened Modem node");
		transport_data->modem_fd = fd;
	}

	// Open Application node
	if (transport_data->application_fd <= 0) {
		rc = gta04_transport_find_node(&dev_node, "Application");
		if (rc < 0) {
			ALOGE("Unable to find Application node, aborting!");
			goto failure;
		}

		fd = gta04_transport_open_node(dev_node);
		if (fd < 0) {
			ALOGE("Unable to open Application node, aborting!");
			goto failure;
		}

		ALOGD("Opened Application node");
		transport_data->application_fd = fd;
	}

	return 0;

failure:
	if (dev_node != NULL)
		free(dev_node);

	return -1;
}

int gta04_transport_close(void *sdata)
{
	struct gta04_transport_data *transport_data = NULL;

	if (sdata == NULL)
		return -1;

	transport_data = (struct gta04_transport_data *) sdata;

	if (transport_data->modem_fd > 0)
		close(transport_data->modem_fd);
	transport_data->modem_fd = -1;

	if (transport_data->application_fd > 0)
		close(transport_data->application_fd);
	transport_data->application_fd = -1;

	return 0;
}

int gta04_transport_send(void *sdata, void *data, int length)
{
	struct gta04_transport_data *transport_data = NULL;

	// Written data count
	int wc;
	// Min count
	int mc;
	// Total written data count
	int tc = 0;

	if (sdata == NULL || data == NULL || length <= 0)
		return -1;

	transport_data = (struct gta04_transport_data *) sdata;

	if (transport_data->modem_fd < 0)
		return -1;

	mc = length;
	while (mc > 0) {
		// Outgoing AT data must be written to the Modem node apparently
		wc = write(transport_data->modem_fd, (unsigned char *) data + (length - mc), mc);
		if (wc < 0)
			return -1;

		tc += wc;
		mc -= wc;
	}

	return tc;
}

int gta04_transport_recv(void *sdata, void *data, int length)
{
	struct gta04_transport_data *transport_data = NULL;
	int fd = -1;

	// Total read data count
	int tc = 0;

	if (sdata == NULL)
		return -1;

	transport_data = (struct gta04_transport_data *) sdata;

	if (transport_data->modem_fd < 0)
		return -1;
	if (transport_data->application_fd < 0)
		return -1;

	if (transport_data->modem_ac > 0) {
		fd = transport_data->modem_fd;
		transport_data->modem_ac = 0;
	} else if (transport_data->application_ac > 0) {
		fd = transport_data->application_fd;
		transport_data->application_ac = 0;
	}

	if (fd < 0)
		return -1;

	tc = read(fd, data, length);

	return tc;
}

int gta04_transport_recv_poll(void *sdata)
{
	struct gta04_transport_data *transport_data = NULL;
	fd_set fds;

	if (sdata == NULL)
		return -1;

	transport_data = (struct gta04_transport_data *) sdata;

	if (transport_data->modem_fd < 0)
		return -1;
	if (transport_data->application_fd < 0)
		return -1;

	FD_ZERO(&fds);
	FD_SET(transport_data->modem_fd, &fds);
	FD_SET(transport_data->application_fd, &fds);

	select(FD_SETSIZE, &fds, NULL, NULL, NULL);

	// Process one at a time
	if (FD_ISSET(transport_data->modem_fd, &fds)) {
		transport_data->modem_ac = 1;
	} else if (FD_ISSET(transport_data->application_fd, &fds)) {
		transport_data->application_ac = 1;
	}

	return 1;
}

int gta04_sdata_dummy(void **sdata)
{
	return 0;
}

int gta04_dummy(void *sdata)
{
	return 0;
}

struct ril_device_power_handlers gta04_power_handlers = {
	.sdata = NULL,
	.sdata_create = gta04_sdata_dummy,
	.sdata_destroy = gta04_dummy,
	.power_on = gta04_power_on,
	.power_off = gta04_power_off,
	.suspend = gta04_dummy,
	.resume = gta04_dummy,
	.boot = gta04_power_boot,
};

struct ril_device_transport_handlers gta04_transport_handlers = {
	.sdata = NULL,
	.sdata_create = gta04_transport_sdata_create,
	.sdata_destroy = gta04_transport_sdata_destroy,
	.open = gta04_transport_open,
	.close = gta04_transport_close,
	.send = gta04_transport_send,
	.recv = gta04_transport_recv,
	.recv_poll = gta04_transport_recv_poll,
};

struct ril_device_handlers gta04_handlers = {
	.power = &gta04_power_handlers,
	.transport = &gta04_transport_handlers,
};

struct ril_device gta04_device = {
	.name = "Goldelico GTA04",
	.tag = "GTA04",
	.type = DEV_GSM,
	.sdata = NULL,
	.handlers = &gta04_handlers,
};

void ril_device_register(struct ril_device **ril_device_p)
{
	*ril_device_p = &gta04_device;
}
