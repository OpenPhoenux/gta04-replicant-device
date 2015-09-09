/*
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
 *               2015 Golden Delicious Computers
                      Lukas Märdian <lukas@goldelico.com>
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
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <linux/ioctl.h>
#include <linux/input.h>

#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "gta04_sensors"
#include <utils/Log.h>

#include "gta04_sensors.h"

struct bmp085_data {
	char path_delay[PATH_MAX];
	float pressure;
	//char path_pressure[PATH_MAX];
};

int bmp085_init(struct gta04_sensors_handlers *handlers,
	struct gta04_sensors_device *device)
{
	struct bmp085_data *data = NULL;
	char path[PATH_MAX] = { 0 };
	int input_fd = -1;
	int rc;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL)
		return -EINVAL;

	data = (struct bmp085_data *) calloc(1, sizeof(struct bmp085_data));

	input_fd = input_open("bmp085");
	if (input_fd < 0) {
		ALOGE("%s: Unable to open input", __func__);
		goto error;
	}

	rc = sysfs_path_prefix("bmp085", (char *) &path);
	if (rc < 0 || path[0] == '\0') {
		ALOGE("%s: Unable to open sysfs", __func__);
		goto error;
	}

	//snprintf(data->path_delay, PATH_MAX, "%s/poll_delay", path);
	//FIXME: static path
	snprintf(data->path_delay, PATH_MAX, "%s", "/sys/devices/platform/omap_i2c.2/i2c-2/2-0077/poll_delay");
	//path = "/sys/devices/platform/omap_i2c.2/i2c-2/2-0077";
	//snprintf(data->path_pressure, PATH_MAX, "%s/pressure0_input", path);

	handlers->poll_fd = input_fd;
	handlers->data = (void *) data;

	return 0;

error:
	if (data != NULL)
		free(data);

	if (input_fd >= 0)
		close(input_fd);

	handlers->poll_fd = -1;
	handlers->data = NULL;

	return -1;
}

int bmp085_deinit(struct gta04_sensors_handlers *handlers)
{
	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL)
		return -EINVAL;

	if (handlers->poll_fd >= 0)
		close(handlers->poll_fd);
	handlers->poll_fd = -1;

	if (handlers->data != NULL)
		free(handlers->data);
	handlers->data = NULL;

	return 0;
}

int bmp085_set_delay(struct gta04_sensors_handlers *handlers, long int delay)
{
	struct bmp085_data *data;
	int rc;
	int delay_ms = (int)(delay/1000000); //bmp085 expects milliseconds, not nanoseconds

	ALOGD("%s(%p, %ld)", __func__, handlers, delay);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct bmp085_data *) handlers->data;

	rc = sysfs_value_write(data->path_delay, delay_ms);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	return 0;
}

int bmp085_activate(struct gta04_sensors_handlers *handlers)
{
	struct bmp085_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct bmp085_data *) handlers->data;

	handlers->activated = 1;

	return 0;
}

int bmp085_deactivate(struct gta04_sensors_handlers *handlers)
{
	struct bmp085_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct bmp085_data *) handlers->data;

	bmp085_set_delay(handlers, 0); //deactivate bmp085 to save some power

	handlers->activated = 0;

	return 0;
}

float bmp085_convert(int value)
{
	return value / 100.0f; //we need hPa, BMP085 delivers Pa
}

int bmp085_get_data(struct gta04_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct bmp085_data *data;
	struct input_event input_event;
	int input_fd;
	int rc;

//	ALOGD("%s(%p, %p)", __func__, handlers, event);

	if (handlers == NULL || handlers->data == NULL || event == NULL)
		return -EINVAL;

	data = (struct bmp085_data *) handlers->data;

	input_fd = handlers->poll_fd;
	if (input_fd < 0)
		return -EINVAL;

	memset(event, 0, sizeof(struct sensors_event_t));
	event->version = sizeof(struct sensors_event_t);
	event->sensor = handlers->handle;
	event->type = handlers->handle;

	event->pressure = data->pressure;

	do {
		rc = read(input_fd, &input_event, sizeof(input_event));
		if (rc < (int) sizeof(input_event))
			break;

		if (input_event.type == EV_ABS) {
			switch (input_event.code) {
				case ABS_PRESSURE:
					event->pressure = bmp085_convert(input_event.value);
					break;
				default:
					continue;
			}
		} else if (input_event.type == EV_SYN) {
			if (input_event.code == SYN_REPORT) {
				event->timestamp = input_timestamp(&input_event);
				break;
			} else {
				return -1;
			}
		}
	} while (input_event.type != EV_SYN);

	data->pressure = event->pressure;

	return 0;
}

struct gta04_sensors_handlers bmp085 = {
	.name = "BMP085",
	.handle = SENSOR_TYPE_PRESSURE,
	.init = bmp085_init,
	.deinit = bmp085_deinit,
	.activate = bmp085_activate,
	.deactivate = bmp085_deactivate,
	.set_delay = bmp085_set_delay,
	.get_data = bmp085_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
