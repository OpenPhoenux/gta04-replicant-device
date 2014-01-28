/*
 * Copyright (C) 2012 Paul Kocialkowski <contact@paulk.fr>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "lights"
#include <cutils/log.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <hardware/lights.h>

/*
 * Globals
 */

pthread_mutex_t lights_mutex = PTHREAD_MUTEX_INITIALIZER;

const char backlight_brightness[] =
	"/sys/class/backlight/pwm-backlight/brightness";
const char backlight_max_brightness[] =
	"/sys/class/backlight/pwm-backlight/max_brightness";

char battery_red_brightness[] =
	"/sys/class/leds/gta04:red:power/brightness";
char battery_red_max_brightness[] =
	"/sys/class/leds/gta04:red:power/max_brightness";
char battery_green_brightness[] =
	"/sys/class/leds/gta04:green:power/brightness";
char battery_green_max_brightness[] =
	"/sys/class/leds/gta04:green:power/max_brightness";

char notifications_red_brightness[] =
	"/sys/class/leds/gta04:red:aux/brightness";
char notifications_red_max_brightness[] =
	"/sys/class/leds/gta04:red:aux/max_brightness";
char notifications_green_brightness[] =
	"/sys/class/leds/gta04:green:aux/brightness";
char notifications_green_max_brightness[] =
	"/sys/class/leds/gta04:green:aux/max_brightness";

/*
 * Lights utils
 */

int sysfs_write_int(char *path, int value)
{
	char buf[10];
	int length = 0;
	int fd = -1;
	int rc;

	if(path == NULL)
		return -1;

	length = snprintf(buf, 10, "%d\n", value);

	fd = open(path, O_WRONLY);
	if(fd < 0)
		return -1;

	rc = write(fd, buf, length);

	close(fd);

	if(rc < length)
		return -1;

	return 0;
}

int sysfs_read_int(char *path)
{
	char buf[10];
	int val = 0;
	int fd = -1;
	int rc;

	if(path == NULL)
		return -1;

	fd = open(path, O_RDONLY);
	if(fd < 0)
		return -1;

	rc = read(fd, buf, 10);
	if(rc <= 0) {
		close(fd);
		return -1;
	}

	val = atoi(buf);

	return val;
}

/*
 * Lights functions
 */

static int set_light_notifications(struct light_device_t *dev,
	const struct light_state_t *state)
{
	int red, green;
	int max;
	int rc;

	// GTA04 only has red and green
	red = state->color & 0x00ff0000;
	green = state->color & 0x0000ff00;

	// Red max
	pthread_mutex_lock(&lights_mutex);
	max = sysfs_read_int(notifications_red_max_brightness);
	pthread_mutex_unlock(&lights_mutex);

	if(max > 0)
		red = (red * max) / 0xff;

	// Green max
	pthread_mutex_lock(&lights_mutex);
	max = sysfs_read_int(notifications_green_max_brightness);
	pthread_mutex_unlock(&lights_mutex);

	if(max > 0)
		green = (green * max) / 0xff;

	pthread_mutex_lock(&lights_mutex);
	rc = sysfs_write_int(notifications_red_brightness, red);
	if(rc >= 0)
		rc = sysfs_write_int(notifications_green_brightness, green);
	pthread_mutex_unlock(&lights_mutex);
	ALOGD("set_light_notifications: %d (red), %d (green)", red, green);

	return rc;
}

static int set_light_battery(struct light_device_t *dev,
	const struct light_state_t *state)
{
	int red, green;
	int max;
	int rc;

	// GTA04 only has red and green
	red = state->color & 0x00ff0000;
	green = state->color & 0x0000ff00;

	// Red max
	pthread_mutex_lock(&lights_mutex);
	max = sysfs_read_int(battery_red_max_brightness);
	pthread_mutex_unlock(&lights_mutex);

	if(max > 0)
		red = (red * max) / 0xff;

	// Green max
	pthread_mutex_lock(&lights_mutex);
	max = sysfs_read_int(battery_green_max_brightness);
	pthread_mutex_unlock(&lights_mutex);

	if(max > 0)
		green = (green * max) / 0xff;

	pthread_mutex_lock(&lights_mutex);
	rc = sysfs_write_int(battery_red_brightness, red);
	if(rc >= 0)
		rc = sysfs_write_int(battery_green_brightness, green);
	pthread_mutex_unlock(&lights_mutex);
	ALOGD("set_light_battery: %d (red), %d (green)", red, green);

	return rc;
}

static int set_light_backlight(struct light_device_t *dev,
	const struct light_state_t *state)
{
	int color;
	unsigned char brightness;
	int brightness_max;
	int rc;

	pthread_mutex_lock(&lights_mutex);
	brightness_max =
		sysfs_read_int(backlight_max_brightness);
	pthread_mutex_unlock(&lights_mutex);

	color = state->color & 0x00ffffff;
	brightness = ((77*((color>>16) & 0x00ff)) + (150*((color>>8) & 0x00ff))
		+ (29*(color & 0x00ff))) >> 8;

	if(brightness_max > 0)
		brightness = (brightness * brightness_max) / 0xff;

	ALOGD("Setting brightness to: %d", brightness);

	pthread_mutex_lock(&lights_mutex);
	rc = sysfs_write_int(backlight_brightness, brightness);
	pthread_mutex_unlock(&lights_mutex);

	return rc;
}

/*
 * Interface
 */

static int close_lights(struct light_device_t *dev)
{
	ALOGD("close_lights()");

	if(dev != NULL)
		free(dev);

	return 0;
}

static int open_lights(const struct hw_module_t *module, char const *name,
	struct hw_device_t **device)
{
	struct light_device_t *dev = NULL;
	int (*set_light)(struct light_device_t *dev,
		const struct light_state_t *state);

	ALOGD("open_lights(): %s", name);

	if(strcmp(LIGHT_ID_BACKLIGHT, name) == 0) {
		set_light = set_light_backlight;
	} else if(strcmp(LIGHT_ID_BATTERY, name) == 0) {
		set_light = set_light_battery;
	} else if(strcmp(LIGHT_ID_NOTIFICATIONS, name) == 0) {
		set_light = set_light_notifications;
	} else {
		return -1;
	}

	FILE * file = fopen("/proc/cmdline", "r");
	if (file) {
		char cmdline[4096];
		if (fgets(cmdline, sizeof(cmdline), file)) {
			/* check if we are on Letux3704 (GTA04B2) or Letux7004 (GTA04B3) */
			if (strstr(cmdline, "mux=GTA04B2") != NULL ||
				strstr(cmdline, "mux=GTA04B3") != NULL) {
				ALOGD("Detected Letux3704/Letux7004, setting LED sysfs paths.");
				/* FIXME: make sure not to overflow the strings */
				strcpy(battery_red_brightness, "");
				strcpy(battery_red_max_brightness, "");
				strcpy(battery_green_brightness,
				"/sys/class/leds/gta04:right/brightness");
				strcpy(battery_green_max_brightness,
				"/sys/class/leds/gta04:right/max_brightness");

				strcpy(notifications_red_brightness,
				"/sys/class/leds/gta04:left/brightness");
				strcpy(notifications_red_max_brightness,
				"/sys/class/leds/gta04:left/max_brightness");
				strcpy(notifications_green_brightness, "");
				strcpy(notifications_green_max_brightness, "");
				ALOGD("PATHs (battery): %s, %s, %s, %s", battery_red_brightness, battery_red_max_brightness, battery_green_brightness, battery_green_max_brightness);
				ALOGD("PATHs (notifications): %s, %s, %s, %s", notifications_red_brightness, notifications_red_max_brightness, notifications_green_brightness, notifications_green_max_brightness);
			}
		} else {
			ALOGE("Error reading cmdline: %s (%d)", strerror(errno),
				errno);
		}
		fclose(file);
	} else {
		ALOGE("Could not detect device variant. Error opening /proc/cmdline: %s (%d)", strerror(errno),
			errno);
	}
	fclose(file);

	pthread_mutex_init(&lights_mutex, NULL);

	dev = calloc(1, sizeof(struct light_device_t));
	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = (struct hw_module_t *) module;
	dev->common.close = (int (*)(struct hw_device_t *)) close_lights;
	dev->set_light = set_light;

	*device = (struct hw_device_t *) dev;

	return 0;
}

static struct hw_module_methods_t lights_module_methods = {
	.open =  open_lights,
};

const struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.version_major = 1,
	.version_minor = 0,
	.id = LIGHTS_HARDWARE_MODULE_ID,
	.name = "Goldelico GTA04 lights",
	.author = "Paul Kocialkowski",
	.methods = &lights_module_methods,
};
