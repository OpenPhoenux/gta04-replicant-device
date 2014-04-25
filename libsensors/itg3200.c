/*
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
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
#include "ssp.h"
#include "iio/myiio.h"

extern int sensor_init_failed;
char mInputName[PATH_MAX];
//char *mInputSysfsEnable;
char *mInputSysfsSamplingFrequency;
char *data_name = "itg3200";

struct itg3200_data {
	char path_delay[PATH_MAX];

	sensors_vec_t gyro;
};

int itg3200_init(struct gta04_sensors_handlers *handlers,
	struct gta04_sensors_device *device)
{
	struct itg3200_data *data = NULL;
	char path[PATH_MAX] = { 0 };
	int input_fd = -1;
	int rc;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL)
		return -EINVAL;

	data = (struct itg3200_data *) calloc(1, sizeof(struct itg3200_data));

	input_fd = open_input(mInputName, data_name); //from iio.c
	ALOGD("%s: mInputName: %s", __func__, mInputName);
	if (input_fd < 0) {
		ALOGE("%s: Unable to open input", __func__);
		goto error;
	}

	//mInputSysfsEnable = make_sysfs_name(); //TODO: no enable node in sysfs?

	mInputSysfsSamplingFrequency = make_sysfs_name(mInputName, "sampling_frequency");
	if (mInputSysfsSamplingFrequency!=NULL) { // warning will always be true
		ALOGE("%s: unable to allocate mem for %s:poll_delay", __func__, data_name);
		goto error;
	}

/* TODO: check x,y,z channels
	mIioSysfsChan = makeSysfsName(mInputName, chan_name);
	if (!mIioSysfsChan) {
		ALOGE("%s: unable to allocate mem for %s:%s", __func__, data_name, chan_name);
		goto error;
	}
*/

/*
	rc = iio_sysfs_path_prefix("itg3200", (char *) &path);
	if (rc < 0 || path[0] == '\0') {
		ALOGE("%s: Unable to open sysfs", __func__);
		goto error;
	}

	snprintf(data->path_delay, PATH_MAX, "%s/sampling_frequency", path);
*/

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
	sensor_init_failed = 1; //don't fail to boot if BMA180/150 is missing

	return -1;
}

int itg3200_deinit(struct gta04_sensors_handlers *handlers)
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

int itg3200_activate(struct gta04_sensors_handlers *handlers)
{
	struct itg3200_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct itg3200_data *) handlers->data;

	rc = ssp_sensor_enable(GYROSCOPE_SENSOR);
	if (rc < 0) {
		ALOGE("%s: Unable to enable ssp sensor", __func__);
		return -1;
	}

	handlers->activated = 1;

	return 0;
}

int itg3200_deactivate(struct gta04_sensors_handlers *handlers)
{
	struct itg3200_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct itg3200_data *) handlers->data;

	rc = ssp_sensor_disable(GYROSCOPE_SENSOR);
	if (rc < 0) {
		ALOGE("%s: Unable to disable ssp sensor", __func__);
		return -1;
	}

	handlers->activated = 1;

	return 0;
}

int itg3200_set_delay(struct gta04_sensors_handlers *handlers, long int delay)
{
	struct itg3200_data *data;
	int rc;

	//TODO: which unit do we need? we get nanoseconds in 'long int delay'
	ALOGD("%s(%p, %ld)", __func__, handlers, delay);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct itg3200_data *) handlers->data;

	rc = sysfs_value_write(data->path_delay, (int) delay);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value", __func__);
		return -1;
	}

	return 0;
}

float itg3200_convert(int value)
{
	return value * (70.0f / 4000.0f) * (3.1415926535f / 180.0f);
}

int itg3200_get_data(struct gta04_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct itg3200_data *data;
	struct input_event input_event;
	int input_fd;
	int rc;

//	ALOGD("%s(%p, %p)", __func__, handlers, event);

	if (handlers == NULL || handlers->data == NULL || event == NULL)
		return -EINVAL;

	data = (struct itg3200_data *) handlers->data;

	input_fd = handlers->poll_fd;
	if (input_fd < 0)
		return -EINVAL;

	memset(event, 0, sizeof(struct sensors_event_t));
	event->version = sizeof(struct sensors_event_t);
	event->sensor = handlers->handle;
	event->type = handlers->handle;

	event->gyro.x = data->gyro.x;
	event->gyro.y = data->gyro.y;
	event->gyro.z = data->gyro.z;

	do {
		rc = read(input_fd, &input_event, sizeof(input_event));
		if (rc < (int) sizeof(input_event))
			break;

		if (input_event.type == EV_REL) {
			switch (input_event.code) {
				case REL_RX:
					event->gyro.x = itg3200_convert(input_event.value);
					break;
				case REL_RY:
					event->gyro.y = itg3200_convert(input_event.value);
					break;
				case REL_RZ:
					event->gyro.z = itg3200_convert(input_event.value);
					break;
				default:
					continue;
			}
		} else if (input_event.type == EV_SYN) {
			if (input_event.code == SYN_REPORT)
				event->timestamp = input_timestamp(&input_event);
		}
	} while (input_event.type != EV_SYN);

	data->gyro.x = event->gyro.x;
	data->gyro.y = event->gyro.y;
	data->gyro.z = event->gyro.z;

	return 0;
}

struct gta04_sensors_handlers itg3200 = {
	.name = "ITG3200 Gyroscope",
	.handle = SENSOR_TYPE_GYROSCOPE,
	.init = itg3200_init,
	.deinit = itg3200_deinit,
	.activate = itg3200_activate,
	.deactivate = itg3200_deactivate,
	.set_delay = itg3200_set_delay,
	.get_data = itg3200_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
