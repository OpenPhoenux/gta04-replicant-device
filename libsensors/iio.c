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

char *make_sysfs_name(const char *input_name, const char *file_name)
{
	char *name;
	int ret;

	ret = asprintf(&name, "/sys/bus/iio/devices/%s/%s", input_name, file_name);
	if (ret < 0)
		return NULL;

	return name;
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
        ALOGE("No industrialio devices available");
        return ret;
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
    if (dev_num >= 0) {
        int fd;
        sprintf(devname, "/dev/iio:device%d", dev_num);
        fd = open(devname, O_RDONLY);
        if (fd >= 0) {
            if (ioctl(fd, IIO_GET_EVENT_FD_IOCTL, &event_fd) >= 0)
                strcpy(mInputName, devname + 5); //e.g. iio:deviceX
            else
                ALOGE("couldn't get a event fd from %s", devname);
            close(fd); /* close /dev/iio:device* */
        } else {
            ALOGE("couldn't open %s (%s)", devname, strerror(errno));
        }
    } else {
       ALOGE("couldn't find the device %s", inputName);
    }

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
