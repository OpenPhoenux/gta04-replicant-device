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
#include <stdlib.h>
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

#define max(a,b) ({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })

static pthread_mutex_t lights_mutex = PTHREAD_MUTEX_INITIALIZER;

#define L2804_GTA04 0
#define L3704_7004  1
int device_type = L2804_GTA04; /* Needs to be detected, default to GTA04 */

char backlight_brightness[] =
	"/sys/class/backlight/backlight/brightness";
char backlight_max_brightness[] =
	"/sys/class/backlight/backlight/max_brightness";

/* GTA04/Letux2804 */
char pwr_red_brightness[] =
	"/sys/class/leds/gta04:red:power/brightness";
char pwr_red_max_brightness[] =
	"/sys/class/leds/gta04:red:power/max_brightness";
char pwr_green_brightness[] =
	"/sys/class/leds/gta04:green:power/brightness";
char pwr_green_max_brightness[] =
	"/sys/class/leds/gta04:green:power/max_brightness";

char aux_red_brightness[] =
	"/sys/class/leds/gta04:red:aux/brightness";
char aux_red_max_brightness[] =
	"/sys/class/leds/gta04:red:aux/max_brightness";
char aux_green_brightness[] =
	"/sys/class/leds/gta04:green:aux/brightness";
char aux_green_max_brightness[] =
	"/sys/class/leds/gta04:green:aux/max_brightness";

/* Letux3704/Letux7004 */
char right_brightness[] =
	"/sys/class/leds/gta04:right/brightness";
char right_max_brightness[] =
	"/sys/class/leds/gta04:right/max_brightness";
char left_brightness[] =
	"/sys/class/leds/gta04:left/brightness";
char left_max_brightness[] =
	"/sys/class/leds/gta04:left/max_brightness";

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
	int red, green, blue;
	unsigned int colorRGB;
	int max;
	int rc = 0;

	colorRGB = state->color;
	red   = (colorRGB >> 16) & 0xFF;
	green = (colorRGB >> 8) & 0xFF;
	blue  = colorRGB & 0xFF;

	if(device_type == L2804_GTA04) {
		// GTA04 only has red and green, so use green LED as green & blue
		green = max(green, blue);

		// Red max
		pthread_mutex_lock(&lights_mutex);
		max = sysfs_read_int(aux_red_max_brightness);
		pthread_mutex_unlock(&lights_mutex);

		// convert byte- VS percentage-representation
		if(max > 0)
			red = (red * max) / 0xff;

		// Green max
		pthread_mutex_lock(&lights_mutex);
		max = sysfs_read_int(aux_green_max_brightness);
		pthread_mutex_unlock(&lights_mutex);

		// convert byte- VS percentage-representation
		if(max > 0)
			green = (green * max) / 0xff;

		pthread_mutex_lock(&lights_mutex);
		rc = sysfs_write_int(aux_red_brightness, red);
		if(rc >= 0)
			rc = sysfs_write_int(aux_green_brightness, green);
		pthread_mutex_unlock(&lights_mutex);
		ALOGD("set_light_notifications: %d (red), %d (green)", red, green);
	} else if(device_type == L3704_7004) {
		// Letux3704 has only one LED
		red = max(red, green);
		red = max(red, blue);

		// Left max
		pthread_mutex_lock(&lights_mutex);
		max = sysfs_read_int(left_max_brightness);
		pthread_mutex_unlock(&lights_mutex);

		// convert byte- VS percentage-representation
		if(max > 0)
			red = (red * max) / 0xff;

		pthread_mutex_lock(&lights_mutex);
		rc = sysfs_write_int(left_brightness, red);
		pthread_mutex_unlock(&lights_mutex);
		ALOGD("set_light_notifications: %d (left)", red);
	}
	else {
		ALOGE("set_light_notifications: LED device nodes unknown.");
	}

	return rc;
}

static int set_light_battery(struct light_device_t *dev,
	const struct light_state_t *state)
{
	int red, green, blue;
	unsigned int colorRGB;
	int max;
	int rc = 0;

	colorRGB = state->color;
	red   = (colorRGB >> 16) & 0xFF;
	green = (colorRGB >> 8) & 0xFF;
	blue  = colorRGB & 0xFF;

	ALOGD("set_light_battery: %#x (red), %#x (green), %#x (blue)", red, green, blue);

	if(device_type == L2804_GTA04) {
		// GTA04 only has red and green, so use green LED as green & blue
		green = max(green, blue);

		// Red max
		pthread_mutex_lock(&lights_mutex);
		max = sysfs_read_int(pwr_red_max_brightness);
		pthread_mutex_unlock(&lights_mutex);

		// convert byte- VS percentage-representation
		if(max > 0)
			red = (red * max) / 0xff;

		// Green max
		pthread_mutex_lock(&lights_mutex);
		max = sysfs_read_int(pwr_green_max_brightness);
		pthread_mutex_unlock(&lights_mutex);

		// convert byte- VS percentage-representation
		if(max > 0)
			green = (green * max) / 0xff;

		pthread_mutex_lock(&lights_mutex);
		rc = sysfs_write_int(pwr_red_brightness, red);
		if(rc >= 0)
			rc = sysfs_write_int(pwr_green_brightness, green);
		pthread_mutex_unlock(&lights_mutex);
		ALOGD("set_light_battery: %d (red), %d (green)", red, green);
	} else if(device_type == L3704_7004) {
		// Letux3704 has only one LED
		green = max(red, green);
		green = max(green, blue);

		// Right max
		pthread_mutex_lock(&lights_mutex);
		max = sysfs_read_int(right_max_brightness);
		pthread_mutex_unlock(&lights_mutex);

		// convert byte- VS percentage-representation
		if(max > 0)
			green = (green * max) / 0xff;

		pthread_mutex_lock(&lights_mutex);
		rc = sysfs_write_int(right_brightness, green);
		pthread_mutex_unlock(&lights_mutex);
		ALOGD("set_light_battery: %d (right)", green);
	}
	else {
		ALOGE("set_light_battery: LED device nodes unknown.");
	}

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

	//avoid low brightness, but allow off (0)
	//this helps in situations where you think the device is off/crashed,
	//even though its working, but brighness is set too low (e.g. 3).
	if(brightness > 0 && brightness < 2)
		brightness = 2; //minimum brighness: 20%

	ALOGD("Setting brightness to: %d", brightness);

	pthread_mutex_lock(&lights_mutex);
	rc = sysfs_write_int(backlight_brightness, brightness);
	pthread_mutex_unlock(&lights_mutex);

	return rc;
}

/* TODO: implement other light devices (from lights.h):
#define LIGHT_ID_BACKLIGHT          "backlight"
#define LIGHT_ID_KEYBOARD           "keyboard"
#define LIGHT_ID_BUTTONS            "buttons"
#define LIGHT_ID_BATTERY            "battery"
#define LIGHT_ID_NOTIFICATIONS      "notifications"
#define LIGHT_ID_ATTENTION          "attention"

These lights aren't currently supported by the higher
layers, but could be someday, so we have the constants
here now:

#define LIGHT_ID_BLUETOOTH          "bluetooth"
#define LIGHT_ID_WIFI               "wifi"

Additional hardware-specific lights:

#define LIGHT_ID_CAPS "caps"
#define LIGHT_ID_FUNC "func"
#define LIGHT_ID_WIMAX "wimax"
#define LIGHT_ID_FLASHLIGHT "flashlight"
*/

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
	int (*set_light)(struct light_device_t *dev,
		struct light_state_t const *state);

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

	if( access( pwr_red_brightness, F_OK ) != -1 &&
		access( pwr_red_max_brightness, F_OK ) != -1 &&
		access( pwr_green_brightness, F_OK ) != -1 &&
		access( pwr_green_max_brightness, F_OK ) != -1 &&
		access( aux_red_brightness, F_OK ) != -1 &&
		access( aux_red_max_brightness, F_OK ) != -1 &&
		access( aux_green_brightness, F_OK ) != -1 &&
		access( aux_green_max_brightness, F_OK ) != -1) {
		ALOGD("Detected GTA04/Letux2804.");
		device_type = L2804_GTA04;
	} else if( access( left_brightness, F_OK ) != -1 &&
		access( left_max_brightness, F_OK ) != -1 &&
		access( right_brightness, F_OK ) != -1 &&
		access( right_max_brightness, F_OK ) != -1) {
		ALOGD("Detected Letux3704/Letux7004.");
		device_type = L3704_7004;
	}
	else {
		ALOGE("Could not detect device variant. Assuming GTA04/Letux2804");
		device_type = L2804_GTA04;
	}

	pthread_mutex_init(&lights_mutex, NULL);

	struct light_device_t *dev = malloc(sizeof(struct light_device_t));
	memset(dev, 0, sizeof(*dev));

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

struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.version_major = 1,
	.version_minor = 0,
	.id = LIGHTS_HARDWARE_MODULE_ID,
	.name = "Goldelico GTA04 lights",
	.author = "Lukas Märdian",
	.methods = &lights_module_methods,
};
