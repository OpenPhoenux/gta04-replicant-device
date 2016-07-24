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
#include <dirent.h>
#include <linux/ioctl.h>
//#include <linux/iio/iio.h>
//#include "iio/linux_iio_iio.h"
#include "iio/events.h"
#include "iio/myiio.h"

#define LOG_TAG "gta04_sensors"
#include <utils/Log.h>

#include "gta04_sensors.h"

const char *iio_dir = "/sys/bus/iio/devices/";

char *make_sysfs_name(const char *device_name, const char *file_name)
{
	char *name;
	int ret;

	ret = asprintf(&name, "/sys/bus/iio/devices/%s/%s", device_name, file_name);
	if (ret < 0)
		return NULL;

	return name;
}

/**
 * iio_set_default_trigger - function to select a sensor's default (data ready) IIO trigger
 * A sensor's default trigger has a name of this format: <SENSOR_NAME>-dev<SENSOR_NUM>, i.e. itg3200-dev1
 *
 * @device_name: the sensor's sysfs name (i.e. iio:device1)
 * @name: the sensor's name (i.e. itg3200)
 * @dev_num: the sensor's IIO number (i.e. 1)
 */
int iio_set_default_trigger(char* device_name, char* name, int dev_num)
{
	char *tmp;
	char *default_trigger;
	int rc;

	tmp = make_sysfs_name(device_name, "trigger/current_trigger");
	asprintf(&default_trigger, "%s-dev%d", name, dev_num); //construct default trigger name
	rc = sysfs_string_write(tmp, default_trigger, strlen(default_trigger)+1); //+1 for null byte
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs string (%s) to %s (%s)", __func__, default_trigger, tmp, strerror(errno));
		goto error;
	}
	free(tmp);
	free(default_trigger);
	return 0;

error:
	free(tmp);
	free(default_trigger);
	return -1;
}

/**
 * iio_set_buffer_state - function to enable/disable an IIO buffer
 *
 * @device_name: the sensor's sysfs name (i.e. iio:device1)
 * @state: 1 to enable, 0 to disable
 */
int iio_set_buffer_state(char* device_name, int state)
{
	char *tmp;
	int rc;

	tmp = make_sysfs_name(device_name, "buffer/enable");
	rc = sysfs_value_write(tmp, state);
	if (rc < 0) {
		ALOGE("%s: Unable to write sysfs value (%d) to %s (%s)", __func__, state, tmp, strerror(errno));
		goto error;
	}
	free(tmp);
	return 0;

error:
	free(tmp);
	return -1;
}

/*
 * find_type_by_name() - function to match top level types by name
 * @name: top level type instance name
 * @type: the type of top level instance being sort
 *
 * Typical types this is used for are device and trigger.
 *
 * NOTE: This function is copied from drivers/staging/iio/Documentation/iio_utils.h
 * and modified.
 */
int find_type_by_name(const char *name, const char *type)
{
    const struct dirent *ent;
    int iio_id;
    int ret = -ENODEV;

    FILE *nameFile;
    DIR *dp;
    char thisname[IIO_MAX_NAME_LENGTH];
    char filename[PATH_MAX];

    dp = opendir(iio_dir);
    if (dp == NULL) {
        ALOGE("No industrialio devices available (%s)", strerror(errno));
        return -errno;
    }

    while (ent = readdir(dp), ent != NULL) {
        if (strcmp(ent->d_name, ".") != 0 &&
            strcmp(ent->d_name, "..") != 0 &&
            strlen(ent->d_name) > strlen(type) &&
            strncmp(ent->d_name, type, strlen(type)) == 0) {
            if (sscanf(ent->d_name + strlen(type), "%d", &iio_id) != 1)
                continue;

            sprintf(filename, "%s%s%d/name", iio_dir, type, iio_id);
            nameFile = fopen(filename, "r");
            if (!nameFile)
                continue;

            if (fscanf(nameFile, "%s", thisname) == 1) {
                if (strcmp(name, thisname) == 0) {
                    fclose(nameFile);
                    ret = iio_id;
                    break;
                }
            }
            fclose(nameFile);
        }
    }
    closedir(dp);
    return ret;
}

int open_input(char* mInputName, const char* inputName) { //data_name
    int event_fd = -1;
    char devname[PATH_MAX];
    int dev_num;

    dev_num =  find_type_by_name(inputName, "iio:device");
    ALOGD("%s: dev_num: %d", __func__, dev_num);
    if (dev_num >= 0) {
        int fd;
        sprintf(devname, "/dev/iio:device%d", dev_num);
        fd = open(devname, O_RDONLY);
        if (fd >= 0) {
            if (ioctl(fd, IIO_GET_EVENT_FD_IOCTL, &event_fd) >= 0)
                strcpy(mInputName, devname + 5); //e.g. iio:deviceX
            else
                ALOGE("couldn't get a event fd from %s", devname);
            ALOGD("%s: fd: %d", __func__, fd);
            close(fd); /* close /dev/iio:device* */
        } else {
            ALOGE("couldn't open %s (%s)", devname, strerror(errno));
        }
    } else {
       ALOGE("couldn't find the device %s", inputName);
    }
    ALOGD("%s: event_fd: %d", __func__, event_fd);

    return event_fd;
}

//====================
/*
int iio_sysfs_path_prefix(char *name, char *path_prefix)
{
	DIR *d;
	struct dirent *di;

	char iio_name[80] = { 0 };
	char path[PATH_MAX];
	char *c;
	int fd;

	if (name == NULL || path_prefix == NULL)
		return -EINVAL;

	d = opendir("/sys/bus/iio/devices");
	if (d == NULL)
		return -1;

	while ((di = readdir(d))) {
		if (di == NULL || strcmp(di->d_name, ".") == 0 || strcmp(di->d_name, "..") == 0)
			continue;

		snprintf(path, PATH_MAX, "/sys/bus/iio/devices/%s/name", di->d_name);

		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;

		read(fd, &iio_name, sizeof(iio_name));
		close(fd);

		c = strstr((char *) &iio_name, "\n");
		if (c != NULL)
			*c = '\0';

		if (strcmp(iio_name, name) == 0) {
			snprintf(path_prefix, PATH_MAX, "/sys/bus/iio/devices/%s", di->d_name);
			return 0;
		}
	}

	return -1;
}
*/
