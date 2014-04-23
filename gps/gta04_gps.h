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
#include <termios.h>
#include <sys/eventfd.h>

#include <hardware/gps.h>

#ifndef _GTA04_GPS_H_
#define _GTA04_GPS_H_

/*
 * Structures
 */

struct gta04_gps {
	GpsCallbacks *callbacks;

	GpsLocation location;
	GpsStatus status;
	GpsSvStatus sv_status;
	uint32_t capabilities;

	int sv_index;
	unsigned char accurate;

	int year;
	int month;
	int day;

	GpsPositionRecurrence recurrence;
	uint32_t interval;

	pthread_mutex_t mutex;
	pthread_t thread;

	int serial_fd;
	int event_fd;
};

/*
 * Values
 */

enum {
	GTA04_GPS_EVENT_NONE,
	GTA04_GPS_EVENT_TERMINATE,
	GTA04_GPS_EVENT_START,
	GTA04_GPS_EVENT_STOP,
	GTA04_GPS_EVENT_RESTART,
	GTA04_GPS_EVENT_INJECT_TIME,
	GTA04_GPS_EVENT_INJECT_LOCATION,
	GTA04_GPS_EVENT_SET_POSITION_MODE,
};

/*
 * Globals
 */

extern const char antenna_state_path[];
extern const char serial_path[];
extern const speed_t serial_speed;

extern const int channel_count;

extern struct gta04_gps *gta04_gps;

/*
 * Declarations
 */

// GTA04 GPS

void gta04_gps_location_callback(void);
void gta04_gps_status_callback(void);
void gta04_gps_sv_status_callback(void);
void gta04_gps_set_capabilities_callback(void);
void gta04_gps_acquire_wakelock_callback(void);
void gta04_gps_release_wakelock_callback(void);

int gta04_gps_serial_open(void);
int gta04_gps_serial_close(void);
int gta04_gps_serial_read(void *buffer, size_t length);
int gta04_gps_serial_write(void *buffer, size_t length);

int gta04_gps_event_read(eventfd_t *event);
int gta04_gps_event_write(eventfd_t event);

// NMEA

char *gta04_gps_nmea_prepare(char *nmea);
char *gta04_gps_nmea_extract(char *buffer, size_t length);
char *gta04_gps_nmea_parse(char *nmea);
int gta04_gps_nmea_parse_int(char *string, size_t offset, size_t length);
double gta04_gps_nmea_parse_float(char *string, size_t offset, size_t length);

int gta04_gps_nmea_time(char *nmea);
int gta04_gps_nmea_date(char *nmea);
int gta04_gps_nmea_coordinates(char *nmea);

int gta04_gps_nmea_gpgga(char *nmea);
int gta04_gps_nmea_gpgll(char *nmea);
int gta04_gps_nmea_gpgsa(char *nmea);
int gta04_gps_nmea_gpgsv(char *nmea);
int gta04_gps_nmea_gprmc(char *nmea);

int gta04_gps_nmea_psrf103(unsigned char message, int interval);

#endif
