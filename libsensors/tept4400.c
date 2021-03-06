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
#include <stdio.h>

#include <hardware/sensors.h>
#include <hardware/hardware.h>

#define LOG_TAG "gta04_sensors"
#include <utils/Log.h>

#include "gta04_sensors.h"

const char SENSOR_PATH[] = "/sys/bus/i2c/devices/2-0048/values"; //tsc2007 on i2c-2

struct tept4400_light_data {
	/* light in SI lux units */
	float light;
};

int tept4400_init(struct gta04_sensors_handlers *handlers,
	struct gta04_sensors_device *device)
{

	struct tept4400_light_data *data = NULL;
	char path[PATH_MAX] = { 0 };
	int input_fd = -1;
	int rc;
	int i;

	ALOGD("%s(%p, %p)", __func__, handlers, device);

	if (handlers == NULL)
		return -EINVAL;

	data = (struct tept4400_light_data *) calloc(1, sizeof(struct tept4400_light_data));

	for (i = 0; i < device->handlers_count; i++) {
		if (device->handlers[i] == NULL)
			continue;
	}

	input_fd = input_open("TSC2007 Touchscreen"); //TEPT4400 is read via TSC2007 'AUX' ADC
	if (input_fd < 0) {
		ALOGE("%s: Unable to open input", __func__);
		goto error;
	}

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

int tept4400_deinit(struct gta04_sensors_handlers *handlers)
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

int tept4400_activate(struct gta04_sensors_handlers *handlers)
{
	struct tept4400_light_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct tept4400_light_data *) handlers->data;

	handlers->activated = 1;

	return 0;
}

int tept4400_deactivate(struct gta04_sensors_handlers *handlers)
{
	struct tept4400_light_data *data;
	int rc;

	ALOGD("%s(%p)", __func__, handlers);

	if (handlers == NULL || handlers->data == NULL)
		return -EINVAL;

	data = (struct tept4400_light_data *) handlers->data;

	handlers->activated = 0;

	return 0;
}

int tept4400_set_delay(struct gta04_sensors_handlers *handlers, long int delay)
{
	return 0;
}

float tept4400_convert(int value)
{
	float ambient_light;

	//TEPT4400 is measured via the TSC2007s "AUX" ADC, which returns values between 0 and 4095 (12 bit).
	//We approximate/map 0 => 0V  -> 0lx
	//                4095 => 1V8 -> 10lx
	ambient_light = ((float)value/4095.0)*10.0;
	//ALOGD("AUX value: %d", value);
	//ALOGD("LUX value: %f", ambient_light);

	return (ambient_light<0.4)?0.0f:ambient_light; //ignore jitter if TEP4400 is not installed
}

/*
float tsc2007_read_sysfs()
{
	char line[100];
	int v1,v2,v3,v4,v5,v6,v7,v8,v9,v11;
	int aux;
	float ambient_light;

	FILE *fd = fopen(SENSOR_PATH, "r");
	if(fd==NULL) {
		ALOGE("%s: Could not open sensor node", __func__); //TODO: check errno
		return -1;
	}
	//e.g. "2200,3812,0,0,280,605,3191,1324,1589,2622,65535" second last (2622) is ambient light value (on aux ADC).
	if(fgets(line, 100, fd) != NULL) {
		sscanf(line, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8, &v9, &aux, &v11);
		//TEPT4400 is measured via the TSC2007s "AUX" ADC, which returns values between 0 and 4095 (12 bit).
		//We approximate/map 0 => 0V  -> 0lx
		//                4095 => 1V8 -> 10lx
		ambient_light = ((float)aux/4095.0)*10.0;
		//ALOGD("AUX value: %d", aux);
		//ALOGD("LUX value: %f", ambient_light);
	}
	fclose(fd);

	return ambient_light;
}
*/

int tept4400_get_data(struct gta04_sensors_handlers *handlers,
	struct sensors_event_t *event)
{
	struct tept4400_light_data *data;
	struct input_event input_event;
	int input_fd;
	int rc;

	//ALOGD("%s(%p, %p)", __func__, handlers, event);

	if (handlers == NULL || handlers->data == NULL || event == NULL)
		return -EINVAL;

	data = (struct tept4400_light_data *) handlers->data;

	input_fd = handlers->poll_fd;
	if (input_fd < 0)
		return -1;

	memset(event, 0, sizeof(struct sensors_event_t));
	event->version = sizeof(struct sensors_event_t);
	event->sensor = handlers->handle;
	event->type = handlers->handle;

	event->light = data->light;

	event->magnetic.status = SENSOR_STATUS_ACCURACY_MEDIUM; //TODO: needed?


	do {
		rc = read(input_fd, &input_event, sizeof(input_event));
		if (rc < (int) sizeof(input_event))
			break;

		if (input_event.type == EV_ABS) {
			switch (input_event.code) {
				case ABS_MISC:
					//ALOGD("ABS_MISC: %d", input_event.value);
					event->light = tept4400_convert(input_event.value);
					break;
				default:
					continue;
			}
		}
		else if (input_event.type == EV_SYN) {
			if (input_event.code == SYN_REPORT) {
				//ALOGD("SYN_REPORT");
				event->timestamp = input_timestamp(&input_event);
			}
		}
	} while (input_event.type != EV_SYN);

	data->light = event->light;

	return 0;
}

struct gta04_sensors_handlers tept4400 = {
	.name = "TEPT4400 Light",
	.handle = SENSOR_TYPE_LIGHT,
	.init = tept4400_init,
	.deinit = tept4400_deinit,
	.activate = tept4400_activate,
	.deactivate = tept4400_deactivate,
	.set_delay = tept4400_set_delay,
	.get_data = tept4400_get_data,
	.activated = 0,
	.needed = 0,
	.poll_fd = -1,
	.data = NULL,
};
