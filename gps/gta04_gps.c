/*
 * Copyright (C) 2014 Paul Kocialkowski <contact@paulk.fr>
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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/select.h>

#define LOG_TAG "gta04_gps"
#include <cutils/log.h>

#include <hardware/gps.h>

#include "gta04_gps.h"
#include "rfkill.h"

/*
 * Globals
 */

const char antenna_state_path[] = "/sys/class/switch/gps_antenna/state";

const char serial_path[] = "/dev/ttyO1";
const speed_t serial_speed = B9600;

const int channel_count = 12;

struct gta04_gps *gta04_gps = NULL;

/*
 * Callbacks
 */

void gta04_gps_location_callback(void)
{
	GpsLocationFlags single_flags = GPS_LOCATION_HAS_LAT_LONG | GPS_LOCATION_HAS_ALTITUDE | GPS_LOCATION_HAS_ACCURACY;

	if (gta04_gps == NULL || gta04_gps->callbacks == NULL || gta04_gps->callbacks->location_cb == NULL)
		return;

	if (gta04_gps->recurrence == GPS_POSITION_RECURRENCE_SINGLE) {
		if ((gta04_gps->location.flags & single_flags) != single_flags)
			return;
		else
			gta04_gps_event_write(GTA04_GPS_EVENT_STOP);
	}

	gta04_gps->callbacks->location_cb(&gta04_gps->location);
}

void gta04_gps_status_callback(void)
{
	if (gta04_gps == NULL || gta04_gps->callbacks == NULL || gta04_gps->callbacks->status_cb == NULL)
		return;

	gta04_gps->callbacks->status_cb(&gta04_gps->status);
}

void gta04_gps_sv_status_callback(void)
{
	if (gta04_gps == NULL || gta04_gps->callbacks == NULL || gta04_gps->callbacks->sv_status_cb == NULL)
		return;

	if (gta04_gps->recurrence == GPS_POSITION_RECURRENCE_SINGLE)
		return;

	gta04_gps->callbacks->sv_status_cb(&gta04_gps->sv_status);
}

void gta04_gps_set_capabilities_callback(void)
{
	if (gta04_gps == NULL || gta04_gps->callbacks == NULL || gta04_gps->callbacks->set_capabilities_cb == NULL)
		return;

	gta04_gps->callbacks->set_capabilities_cb(gta04_gps->capabilities);
}

void gta04_gps_acquire_wakelock_callback(void)
{
	if (gta04_gps == NULL || gta04_gps->callbacks == NULL || gta04_gps->callbacks->acquire_wakelock_cb == NULL)
		return;

	gta04_gps->callbacks->acquire_wakelock_cb();
}

void gta04_gps_release_wakelock_callback(void)
{
	if (gta04_gps == NULL || gta04_gps->callbacks == NULL || gta04_gps->callbacks->release_wakelock_cb == NULL)
		return;

	gta04_gps->callbacks->release_wakelock_cb();
}

/*
 * GTA04 GPS
 */

int gta04_gps_antenna_state(void)
{
	char state[9] = { 0 };
	int fd;

	fd = open(antenna_state_path, O_RDONLY);
	if (fd < 0)
		return -1;

	read(fd, &state, sizeof(state));
	close(fd);

	state[8] = '\0';

	ALOGD("GPS antenna state is: %s", state);

	return 0;
}

// Rfkill

int gta04_gps_rfkill_change(unsigned char type, unsigned char soft)
{
	struct rfkill_event event;
	int fd = -1;
	int rc;

	fd = open("/dev/rfkill", O_WRONLY);
	if (fd < 0) {
		ALOGE("Opening rfkill device failed");
		return -1;
	}

	memset(&event, 0, sizeof(event));
	event.type = type;
	event.op = RFKILL_OP_CHANGE_ALL;
	event.soft = soft;

	rc = write(fd, &event, sizeof(event));
	if (rc < (int) sizeof(struct rfkill_event)) {
		ALOGE("Writing rfkill event failed");
		goto error;
	}

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}

// Serial

int gta04_gps_serial_open(void)
{
	struct termios termios;
	int serial_fd = -1;
	int rc;

	if (gta04_gps == NULL)
		return -1;

	pthread_mutex_lock(&gta04_gps->mutex);

	if (gta04_gps->serial_fd >= 0)
		close(gta04_gps->serial_fd);

	serial_fd = open(serial_path, O_RDWR | O_NONBLOCK);
	if (serial_fd < 0) {
		ALOGE("Opening serial failed");
		goto error;
	}

	rc = tcgetattr(serial_fd, &termios);
	if (rc < 0)
		goto error;

	if (cfgetispeed(&termios) != serial_speed) {
		cfsetispeed(&termios, serial_speed);

		rc = tcsetattr(serial_fd, 0, &termios);
		if (rc < 0)
			goto error;
	}

	if (cfgetospeed(&termios) != serial_speed) {
		cfsetospeed(&termios, serial_speed);

		rc = tcsetattr(serial_fd, 0, &termios);
		if (rc < 0)
			goto error;
	}

	gta04_gps->serial_fd = serial_fd;

	rc = 0;
	goto complete;

error:
	if (serial_fd >= 0)
		close(serial_fd);

	gta04_gps->serial_fd = -1;

	rc = -1;

complete:
	pthread_mutex_unlock(&gta04_gps->mutex);

	return rc;
}

int gta04_gps_serial_close(void)
{
	if (gta04_gps == NULL)
		return -1;

	if (gta04_gps->serial_fd < 0)
		return 0;

	pthread_mutex_lock(&gta04_gps->mutex);

	close(gta04_gps->serial_fd);
	gta04_gps->serial_fd = -1;

	pthread_mutex_unlock(&gta04_gps->mutex);

	return 0;
}

int gta04_gps_serial_read(void *buffer, size_t length)
{
	int rc;

	if (buffer == NULL || length == 0)
		return -EINVAL;

	if (gta04_gps == NULL || gta04_gps->serial_fd < 0)
		return -1;

	pthread_mutex_lock(&gta04_gps->mutex);
	gta04_gps_acquire_wakelock_callback();

	rc = read(gta04_gps->serial_fd, buffer, length);
	if (rc <= 0)
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	gta04_gps_release_wakelock_callback();
	pthread_mutex_unlock(&gta04_gps->mutex);

	return rc;
}

int gta04_gps_serial_write(void *buffer, size_t length)
{
	int rc;

	if (buffer == NULL || length == 0)
		return -EINVAL;

	if (gta04_gps == NULL || gta04_gps->serial_fd < 0)
		return -1;

	pthread_mutex_lock(&gta04_gps->mutex);
	gta04_gps_acquire_wakelock_callback();

	rc = write(gta04_gps->serial_fd, buffer, length);
	if (rc < (int) length)
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	gta04_gps_release_wakelock_callback();
	pthread_mutex_unlock(&gta04_gps->mutex);

	return rc;
}

// Event

int gta04_gps_event_read(eventfd_t *event)
{
	int rc;

	if (event == NULL)
		return -EINVAL;

	if (gta04_gps == NULL || gta04_gps->event_fd < 0)
		return -1;

	rc = eventfd_read(gta04_gps->event_fd, event);
	if (rc < 0)
		return -1;

	return 0;
}

int gta04_gps_event_write(eventfd_t event)
{
	eventfd_t flush;
	int rc;

	if (gta04_gps == NULL || gta04_gps->event_fd < 0)
		return -1;

	eventfd_read(gta04_gps->event_fd, &flush);

	rc = eventfd_write(gta04_gps->event_fd, event);
	if (rc < 0)
		return -1;

	return 0;
}

// Thread

int gta04_gps_serial_handle(void)
{
	char buffer[80] = { 0 };
	char *nmea = NULL;
	char *address;
	int interval;
	int rc;

	if (gta04_gps == NULL)
		return -1;

	rc = gta04_gps_serial_read(&buffer, sizeof(buffer));
	if (rc < 0) {
		ALOGE("Reading from serial failed");
		return -1;
	}

	nmea = gta04_gps_nmea_extract(buffer, sizeof(buffer));
	if (nmea == NULL) {
		rc = 0;
		goto complete;
	}

	if (gta04_gps->status.status != GPS_STATUS_SESSION_BEGIN) {
		// Now is a good time to setup the interval of location messages

		if (gta04_gps->recurrence == GPS_POSITION_RECURRENCE_SINGLE)
			gta04_gps->interval = 1000;

		interval = gta04_gps->interval / 1000;

		// Location is reported from GPRMC message
		gta04_gps_nmea_psrf103(4, interval);

		gta04_gps->status.status = GPS_STATUS_SESSION_BEGIN;
		gta04_gps_status_callback();
	}

	// ALOGD("NMEA: %s", nmea);

	address = gta04_gps_nmea_parse(nmea);
	if (address == NULL) {
		ALOGE("Parsing NMEA failed");
		goto error;
	}

	if (strcmp("GPGGA", address) == 0)
		rc = gta04_gps_nmea_gpgga(nmea);
	else if (strcmp("GPGLL", address) == 0)
		rc = gta04_gps_nmea_gpgll(nmea);
	else if (strcmp("GPGSA", address) == 0)
		rc = gta04_gps_nmea_gpgsa(nmea);
	else if (strcmp("GPGSV", address) == 0)
		rc = gta04_gps_nmea_gpgsv(nmea);
	else if (strcmp("GPRMC", address) == 0)
		rc = gta04_gps_nmea_gprmc(nmea);
	else
		rc = 0;

	gta04_gps_nmea_parse(NULL);

	goto complete;

error:
	rc = -1;

complete:
	if (nmea != NULL)
		free(nmea);

	return rc;
}

int gta04_gps_event_handle(void)
{
	eventfd_t event = 0;
	int rc;

	if (gta04_gps == NULL)
		return -1;

	rc = gta04_gps_event_read(&event);
	if (rc < 0) {
		ALOGE("Reading event failed");
		return -1;
	}

	switch (event) {
		case GTA04_GPS_EVENT_TERMINATE:
			return 1;
		case GTA04_GPS_EVENT_START:
			ALOGD("Starting the GPS");

			rc = gta04_gps_rfkill_change(RFKILL_TYPE_GPS, 0);
			if (rc < 0)
				return -1;

			// Give it some time to power up the antenna
			usleep(20000);

			gta04_gps_antenna_state();

			rc = gta04_gps_serial_open();
			if (rc < 0)
				return -1;

			gta04_gps->status.status = GPS_STATUS_ENGINE_ON;
			gta04_gps_status_callback();
			break;
		case GTA04_GPS_EVENT_STOP:
			ALOGD("Stopping the GPS");

			gta04_gps->status.status = GPS_STATUS_SESSION_END;
			gta04_gps_status_callback();

			rc = gta04_gps_serial_close();
			if (rc < 0)
				return -1;

			rc = gta04_gps_rfkill_change(RFKILL_TYPE_GPS, 1);
			if (rc < 0)
				return -1;

			gta04_gps->status.status = GPS_STATUS_ENGINE_OFF;
			gta04_gps_status_callback();
			break;
		case GTA04_GPS_EVENT_RESTART:
			ALOGD("Restarting the GPS");

			gta04_gps->status.status = GPS_STATUS_SESSION_END;
			gta04_gps_status_callback();

			rc = gta04_gps_serial_close();
			if (rc < 0)
				return -1;

			gta04_gps->status.status = GPS_STATUS_ENGINE_OFF;
			gta04_gps_status_callback();

			// Small delay between off and on
			usleep(20000);

			rc = gta04_gps_serial_open();
			if (rc < 0)
				return -1;

			gta04_gps->status.status = GPS_STATUS_ENGINE_ON;
			gta04_gps_status_callback();
			break;
		case GTA04_GPS_EVENT_INJECT_TIME:
			break;
		case GTA04_GPS_EVENT_INJECT_LOCATION:
			break;
		case GTA04_GPS_EVENT_SET_POSITION_MODE:
			// Position mode is set later on
			break;
	}

	return 0;
}

void gta04_gps_thread(void *data)
{
	struct timeval time;
	struct timeval *timeout;
	fd_set fds;
	int fd_max;
	int failures;
	int rc;

	ALOGD("%s(%p)", __func__, data);

	if (gta04_gps == NULL || gta04_gps->event_fd < 0)
		return;

	failures = 0;

	while (1) {
		timeout = NULL;

		FD_ZERO(&fds);
		FD_SET(gta04_gps->event_fd, &fds);

		if (gta04_gps->serial_fd >= 0) {
			FD_SET(gta04_gps->serial_fd, &fds);

			time.tv_sec = 2;
			time.tv_usec = 0;

			timeout = &time;
		}

		fd_max = gta04_gps->event_fd > gta04_gps->serial_fd ? gta04_gps->event_fd : gta04_gps->serial_fd;

		rc = select(fd_max + 1, &fds, NULL, NULL, timeout);
		if (rc < 0) {
			ALOGE("Polling failed");
			break;
		} else if (rc == 0 && gta04_gps->status.status == GPS_STATUS_ENGINE_ON) {
			ALOGE("Not receiving anything from the GPS");
			gta04_gps_event_write(GTA04_GPS_EVENT_RESTART);
			continue;
		}

		rc = 0;

		if (FD_ISSET(gta04_gps->serial_fd, &fds))
			rc |= gta04_gps_serial_handle();

		if (FD_ISSET(gta04_gps->event_fd, &fds))
			rc |= gta04_gps_event_handle();

		if (rc < 0)
			failures++;
		else if (rc == 1)
			break;
		else if (failures)
			failures = 0;

		if (failures > 10) {
			ALOGE("Too many failures, terminating thread");
			break;
		}
	}
}

/*
 * GPS Interface
 */

int gta04_gps_init(GpsCallbacks *callbacks)
{
	int event_fd;
	int rc;

	ALOGD("%s(%p)", __func__, callbacks);

	if (callbacks == NULL || callbacks->create_thread_cb == NULL)
		return -EINVAL;

	if (gta04_gps != NULL)
		free(gta04_gps);

	gta04_gps = (struct gta04_gps *) calloc(1, sizeof(struct gta04_gps));
	gta04_gps->callbacks = callbacks;
	gta04_gps->serial_fd = -1;

	gta04_gps->location.size = sizeof(GpsLocation);
	gta04_gps->status.size = sizeof(GpsStatus);
	gta04_gps->sv_status.size = sizeof(GpsSvStatus);

	gta04_gps->capabilities = GPS_CAPABILITY_SCHEDULING | GPS_CAPABILITY_SINGLE_SHOT;

	pthread_mutex_init(&gta04_gps->mutex, NULL);

	event_fd = eventfd(0, EFD_NONBLOCK);
	if (event_fd < 0) {
		ALOGE("Opening eventfd failed");
		goto error;
	}

	gta04_gps->event_fd = event_fd;

	gta04_gps->thread = callbacks->create_thread_cb("GTA04 GPS", &gta04_gps_thread, NULL);

	rc = gta04_gps_rfkill_change(RFKILL_TYPE_GPS, 1);
	if (rc < 0)
		goto error;

	gta04_gps_set_capabilities_callback();

	gta04_gps->status.status = GPS_STATUS_ENGINE_OFF;
	gta04_gps_status_callback();

	rc = 0;
	goto complete;

error:
	pthread_mutex_destroy(&gta04_gps->mutex);

	if (event_fd >= 0) {
		gta04_gps_event_write(GTA04_GPS_EVENT_TERMINATE);

		close(gta04_gps->event_fd);
		gta04_gps->event_fd = -1;
	}

	if (gta04_gps != NULL) {
		free(gta04_gps);
		gta04_gps = NULL;
	}

	rc = -1;

complete:
	return rc;
}

void gta04_gps_cleanup(void)
{
	ALOGD("%s()", __func__);

	if (gta04_gps == NULL)
		return;

	pthread_mutex_destroy(&gta04_gps->mutex);

	if (gta04_gps->event_fd >= 0) {
		gta04_gps_event_write(GTA04_GPS_EVENT_TERMINATE);

		close(gta04_gps->event_fd);
		gta04_gps->event_fd = -1;
	}

	free(gta04_gps);
	gta04_gps = NULL;
}

int gta04_gps_start(void)
{
	int rc;

	ALOGD("%s()", __func__);

	rc = gta04_gps_event_write(GTA04_GPS_EVENT_START);
	if (rc < 0) {
		ALOGE("Writing event failed");
		return -1;
	}

	return 0;
}

int gta04_gps_stop(void)
{
	int rc;

	ALOGD("%s()", __func__);

	rc = gta04_gps_event_write(GTA04_GPS_EVENT_STOP);
	if (rc < 0) {
		ALOGE("Writing event failed");
		return -1;
	}

	return 0;
}

int gta04_gps_inject_time(GpsUtcTime time, int64_t reference,
	int uncertainty)
{
	int rc;

	ALOGD("%s(%ld, %ld, %d)", __func__, (long int) time, (long int) reference, uncertainty);

	if (gta04_gps == NULL)
		return -1;

	rc = gta04_gps_event_write(GTA04_GPS_EVENT_INJECT_TIME);
	if (rc < 0) {
		ALOGE("Writing event failed");
		return -1;
	}

	return 0;
}

int gta04_gps_inject_location(double latitude, double longitude, float accuracy)
{
	int rc;

	ALOGD("%s(%f, %f, %f)", __func__, latitude, longitude, accuracy);

	if (gta04_gps == NULL)
		return -1;

	rc = gta04_gps_event_write(GTA04_GPS_EVENT_INJECT_LOCATION);
	if (rc < 0) {
		ALOGE("Writing event failed");
		return -1;
	}

	return 0;
}

void gta04_gps_delete_aiding_data(GpsAidingData flags)
{
	return;
}

int gta04_gps_set_position_mode(GpsPositionMode mode,
	GpsPositionRecurrence recurrence, uint32_t interval,
	uint32_t preferred_accuracy, uint32_t preferred_time)
{
	int rc;

	ALOGD("%s(%d, %d, %d, %d, %d)", __func__, mode, recurrence, interval, preferred_accuracy, preferred_time);

	if (gta04_gps == NULL)
		return -1;

	gta04_gps->recurrence = recurrence;
	gta04_gps->interval = interval;

	rc = gta04_gps_event_write(GTA04_GPS_EVENT_SET_POSITION_MODE);
	if (rc < 0) {
		ALOGE("Writing event failed");
		return -1;
	}

	return 0;
}

const void *gta04_gps_get_extension(const char *name)
{
	ALOGD("%s(%s)", __func__, name);

	return NULL;
}

/*
 * Interface
 */

const GpsInterface gta04_gps_interface = {
	.size = sizeof(GpsInterface),
	.init = gta04_gps_init,
	.cleanup = gta04_gps_cleanup,
	.start = gta04_gps_start,
	.stop = gta04_gps_stop,
	.inject_time = gta04_gps_inject_time,
	.inject_location = gta04_gps_inject_location,
	.delete_aiding_data = gta04_gps_delete_aiding_data,
	.set_position_mode = gta04_gps_set_position_mode,
	.get_extension = gta04_gps_get_extension,
};

const GpsInterface *gta04_gps_get_gps_interface(struct gps_device_t *device)
{
	ALOGD("%s(%p)", __func__, device);

	return &gta04_gps_interface;
}

int gta04_gps_close(struct hw_device_t *device)
{
	ALOGD("%s(%p)", __func__, device);

	if (device != NULL)
		free(device);

	return 0;
}

int gta04_gps_open(const struct hw_module_t *module, const char *id,
	struct hw_device_t **device)
{
	struct gps_device_t *gps_device;

	ALOGD("%s(%p, %s, %p)", __func__, module, id, device);

	if (module == NULL || id == NULL || device == NULL)
		return -EINVAL;

	gps_device = calloc(1, sizeof(struct gps_device_t));
	gps_device->common.tag = HARDWARE_DEVICE_TAG;
	gps_device->common.version = 0;
	gps_device->common.module = (struct hw_module_t *) module;
	gps_device->common.close = (int (*)(struct hw_device_t *)) gta04_gps_close;
	gps_device->get_gps_interface = gta04_gps_get_gps_interface;

	*device = (struct hw_device_t *) gps_device;

	return 0;
}

struct hw_module_methods_t gta04_gps_module_methods = {
	.open =  gta04_gps_open,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.version_major = 1,
	.version_minor = 0,
	.id = GPS_HARDWARE_MODULE_ID,
	.name = "GTA04 GPS",
	.author = "Paul Kocialkowski",
	.methods = &gta04_gps_module_methods,
};
