/*
 * Copyright (C) 2014-2015 Golden Delicious Computers
 *                         Lukas Märdian <lukas@goldelico.com>
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
#include "iio/myiio.h"

struct hmc5883l_data {
	char* name;
	char* iio_name;
	sensors_vec_t magnetic;

	int x_fp;
	int y_fp;
	int z_fp;
};

int hmc5883l_enable_iio_channels(char* device_name)
{
	char *tmp;
	int rc;

	//Enable IIO channel X
	tmp = make_sysfs_name(device_name, "scan_elements/in_magn_x_en");
	rc = sysfs_value_write(tmp, 1); //1 == enable
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value (%d) to %s", __func__, 1, tmp);
		goto error;
	}
	free(tmp);

	//Enable IIO channel Y
	tmp = make_sysfs_name(device_name, "scan_elements/in_magn_y_en");
	rc = sysfs_value_write(tmp, 1); //1 == enable
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value (%d) to %s", __func__, 1, tmp);
		goto error;
	}
	free(tmp);

	//Enable IIO channel Z
	tmp = make_sysfs_name(device_name, "scan_elements/in_magn_z_en");
	rc = sysfs_value_write(tmp, 1); //1 == enable
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value (%d) to %s", __func__, 1, tmp);
		goto error;
	}
	free(tmp);

	return 0;

error:
	free(tmp);
	return -1;
}

int hmc5883l_init(struct gta04_sensors_handlers *handlers,
	struct gta04_sensors_device *device)
{
	struct hmc5883l_data *data = NULL;
	char path[PATH_MAX] = { 0 };
	int iio_fd = -1;
	int rc;
	int dev_num;
	char* tmp;
	char *x_path;
	char *y_path;
	char *z_path;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL)
		return -EINVAL;

	data = (struct hmc5883l_data *) calloc(1, sizeof(struct hmc5883l_data));
	data->name = "hmc5883l";

	dev_num = find_type_by_name(data->name, "iio:device");
	asprintf(&(data->iio_name), "iio:device%d", dev_num); //i.e. device_name = iio:device0

	hmc5883l_enable_iio_channels(data->iio_name);
	iio_set_default_trigger(data->iio_name, data->name, dev_num);

	x_path = make_sysfs_name(data->iio_name, "in_magn_x_raw");
	y_path = make_sysfs_name(data->iio_name, "in_magn_y_raw");
	z_path = make_sysfs_name(data->iio_name, "in_magn_z_raw");
	data->x_fp = open(x_path, O_RDONLY);
	data->y_fp = open(y_path, O_RDONLY);
	data->z_fp = open(z_path, O_RDONLY);
	if (data->x_fp < 0)
		ALOGE("%s: unable to open %s", __func__, x_path);
	if (data->y_fp < 0)
		ALOGE("%s: unable to open %s", __func__, y_path);
	if (data->z_fp < 0)
		ALOGE("%s: unable to open %s", __func__, z_path);
	free(x_path);
	free(y_path);
	free(z_path);

/*
	int flags = fcntl(iio_fd, F_GETFL, 0);
	fcntl(iio_fd, F_SETFL, flags | O_NONBLOCK);
*/
	iio_fd = open("/dev/iio:device0", O_RDONLY);// | O_NONBLOCK);
	if (iio_fd < 0) {
		ALOGE("%s: Unable to open device.", __func__);
		goto error;
	}

	handlers->poll_fd = iio_fd;
	handlers->data = (void *) data;

	return 0;

error:
	if (data != NULL)
		free(data);

	if (iio_fd >= 0)
		close(iio_fd);

	handlers->poll_fd = -1;
	handlers->data = NULL;

	return -1;
}

int hmc5883l_deinit(struct gta04_sensors_handlers *handlers)
{
	struct hmc5883l_data *data;
	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL)
		return -EINVAL;

	data = (struct hmc5883l_data *) handlers->data;
	close(data->x_fp);
	close(data->y_fp);
	close(data->z_fp);

	if (handlers->poll_fd >= 0)
		close(handlers->poll_fd);
	handlers->poll_fd = -1;

	if (handlers->data != NULL)
		free(handlers->data);
	handlers->data = NULL;

	return 0;
}

int hmc5883l_activate(struct gta04_sensors_handlers *handlers)
{
	struct hmc5883l_data *data;
	int rc;
	char *tmp;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL) {
		ALOGD("%s: handler==NULL || handlers->data==NULL", __func__);
		return -EINVAL;
	}

	data = (struct hmc5883l_data *) handlers->data;

	//operating_mode is removed in kernels > 3.12!
	tmp = make_sysfs_name(data->iio_name, "operating_mode");
	rc = sysfs_value_write(tmp, 0); //0 = continuous sampling
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value (%d) to %s", __func__, 0, tmp);
		goto error;
	}
	free(tmp);

	//Enable IIO buffer
	rc = iio_set_buffer_state(data->iio_name, 1);
	if (rc < 0) {
		ALOGE("%s: Unable to enable IIO buffer", __func__);
		return -1;
	}

	handlers->activated = 1;
	return 0;
error:
	free(tmp);
	return -1;
}

int hmc5883l_deactivate(struct gta04_sensors_handlers *handlers)
{
	struct hmc5883l_data *data;
	int rc;
	char *tmp;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct hmc5883l_data *) handlers->data;

	//operating_mode is removed in kernels > 3.12!
	tmp = make_sysfs_name(data->iio_name, "operating_mode");
	rc = sysfs_value_write(tmp, 3); //3 = power save
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value (%d) to %s", __func__, 0, tmp);
		goto error;
	}
	free(tmp);

	//Disable IIO buffer
	rc = iio_set_buffer_state(data->iio_name, 0);
	if (rc < 0) {
		ALOGE("%s: Unable to disable IIO buffer", __func__);
		return -1;
	}

	handlers->activated = 0;
	return 0;

error:
	free(tmp);
	return -1;
}

/**
 * hmc5883l_set_delay - function to set the sensor's polling delay
 * @delay: delay between polling in nanoseconds
 */
int hmc5883l_set_delay(struct gta04_sensors_handlers *handlers, long int delay)
{
	struct hmc5883l_data *data;
	int rc;
	char *tmp;
	long int one_second = 1000000000; //in nanoseconds
	int frequency = 0; //in Hz

	ALOGD("%s(%p, %ld)", __func__, handlers, delay);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct hmc5883l_data *) handlers->data;

	frequency = (int)ceil((double)one_second/(double)delay);

	//if sensor is not active, set sampling_frequency to 0, in order to save power
	/* Does not work for some reason: Sensor won't deliver data if deactivated for the 1st time.
	if(handlers->activated == 0)
		frequency = 0;
	*/

	tmp = make_sysfs_name(data->iio_name, "in_magn_sampling_frequency");
/* TODO: hmc5883l has only a certain set of frequencies (see sampling_frequency_available in sysfs)
	ALOGD("%s: setting sampling_frequency to %d", __func__, frequency);
	rc = sysfs_value_write(tmp, frequency);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value (%d) to %s", __func__, 0, tmp);
		goto error;
	}
*/
	free(tmp);

	return 0;

error:
	free(tmp);
	return -1;
}

float hmc5883l_convert(int value)
{
	return (value * (100.0f / 1370.0f)); //Gain: 0.88 => /1370, Gauss to uT => *100
	//return value * 1.0f;
}

int hmc5883l_get_data(struct gta04_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct hmc5883l_data *data;
	struct input_event input_event;
	int iio_fd;
	int rc;
	char buf[1024];

	//ALOGD("%s(%p, %p)", __func__, handlers, event);

	if (handlers == NULL || handlers->data == NULL || event == NULL)
		return -EINVAL;

	data = (struct hmc5883l_data *) handlers->data;

	iio_fd = handlers->poll_fd;
	if (iio_fd < 0)
		return -EINVAL;

	//clear '/dev/iio:device0', so poll() won't loop infinitely
	do {
		rc = read(iio_fd, &buf, 1024);
		//ALOGD("hmc5883l: read %d bytes", rc);
	} while (rc > 0);


	memset(event, 0, sizeof(struct sensors_event_t));
	event->version = sizeof(struct sensors_event_t);
	event->sensor = handlers->handle;
	event->type = handlers->handle;

	event->magnetic.x = data->magnetic.x;
	event->magnetic.y = data->magnetic.y;
	event->magnetic.z = data->magnetic.z;

	//TODO: use bytes from iio_fd, instead of reading sysfs
	rc = pread(data->x_fp, &buf, 1024, 0);
	event->magnetic.y = hmc5883l_convert(atoi(buf)) - 75.0; //X and Y are interchanged

	rc = pread(data->y_fp, &buf, 1024, 0);
	event->magnetic.x = hmc5883l_convert(atoi(buf)) + 10.0; //X and Y are interchanged

	rc = pread(data->z_fp, &buf, 1024, 0);
	event->magnetic.z = hmc5883l_convert(atoi(buf)) - 50.0;

	data->magnetic.x = event->magnetic.x;
	data->magnetic.y = event->magnetic.y;
	data->magnetic.z = event->magnetic.z;

	return 0;
}

struct gta04_sensors_handlers hmc5883l = {
	.name = "HMC5883L Magnetometer",
	.handle = SENSOR_TYPE_MAGNETIC_FIELD,
	.init = hmc5883l_init,
	.deinit = hmc5883l_deinit,
	.activate = hmc5883l_activate,
	.deactivate = hmc5883l_deactivate,
	.set_delay = hmc5883l_set_delay,
	.get_data = hmc5883l_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
