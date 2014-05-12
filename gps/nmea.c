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
#include <string.h>
#include <errno.h>
#include <math.h>

#define LOG_TAG "gta04_gps"
#include <cutils/log.h>

#include <hardware/gps.h>

#include "gta04_gps.h"

unsigned char gta04_gps_nmea_checksum(void *data, size_t size)
{
	unsigned char checksum = 0;
	unsigned char *p;
	size_t c;

	if (data == NULL || size == 0)
		return 0;

	p = (unsigned char *) data;

	for (c = 0; c < size; c++)
		checksum ^= *p++;

	return checksum;
}

char *gta04_gps_nmea_prepare(char *nmea)
{
	static char buffer[80] = { 0 };
	unsigned char checksum;
	size_t length;

	if (nmea == NULL)
		return NULL;

	length = strlen(nmea);

	checksum = gta04_gps_nmea_checksum(nmea, length);

	memset(&buffer, 0, sizeof(buffer));
	snprintf((char *) &buffer, sizeof(buffer), "$%s*%02X\r\n", nmea, checksum);

	return buffer;
}

char *gta04_gps_nmea_extract(char *buffer, size_t length)
{
	unsigned char checksum[2];
	char *nmea = NULL;
	char *start = NULL;
	char *end = NULL;
	size_t c;

	if (buffer == NULL || length == 0)
		return NULL;

	for (c = 0; c < length; c++) {
		if (buffer[c] == '$' &&  c < (length -1))
			start = &buffer[c + 1];
		else if (buffer[c] == '*' && c < (length - 2))
			end = &buffer[c];
	}

	if (start == NULL || end == NULL || start == end)
		goto error;

	*end++ = '\0';
	nmea = strdup(start);

	checksum[0] = gta04_gps_nmea_checksum(nmea, strlen(nmea));
	checksum[1] = strtol(end, NULL, 16);

	if (checksum[0] != checksum[1]) {
		ALOGE("Checksum mismatch: %02X != %02X", checksum[0], checksum[1]);
		goto error;
	}

	goto complete;

error:
	if (nmea != NULL) {
		free(nmea);
		nmea = NULL;
	}

complete:
	return nmea;
}

char *gta04_gps_nmea_parse(char *nmea)
{
	static char *reference = NULL;
	static char *start = NULL;
	char *buffer;
	char *p;
	size_t c;

	if (nmea != reference) {
		reference = nmea;
		start = nmea;
	}

	if (nmea == NULL)
		return NULL;

	buffer = start;
	p = start;

	while (*p != '\0' && *p != ',')
		p++;

	if (p == start)
		buffer = NULL;

	if (*p == ',')
		*p++ = '\0';

	start = p;

	return buffer;
}

int gta04_gps_nmea_parse_int(char *string, size_t offset, size_t length)
{
	char *buffer;
	int value;

	if (string == NULL || length == 0)
		return -EINVAL;

	buffer = (char *) calloc(1, length + 1);

	strncpy(buffer, string + offset, length);
	value = atoi(buffer);

	free(buffer);

	return value;
}

double gta04_gps_nmea_parse_float(char *string, size_t offset, size_t length)
{
	char *buffer;
	double value;

	if (string == NULL || length == 0)
		return -EINVAL;

	buffer = (char *) calloc(1, length + 1);

	strncpy(buffer, string + offset, length);
	value = atof(buffer);

	free(buffer);

	return value;
}

int gta04_gps_nmea_time(char *nmea)
{
	struct tm tm;
	struct tm tm_utc;
	struct tm tm_local;
	time_t timestamp;
	time_t time_now;
	int hour;
	int minute;
	double second;
	char *buffer;

	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL) {
		hour = gta04_gps_nmea_parse_int(buffer, 0, 2);
		minute = gta04_gps_nmea_parse_int(buffer, 2, 2);
		second = gta04_gps_nmea_parse_float(buffer, 4, strlen(buffer) - 4);

		memset(&tm, 0, sizeof(tm));

		tm.tm_year = gta04_gps->year + 100;
		tm.tm_mon = gta04_gps->month - 1;
		tm.tm_mday = gta04_gps->day;

		tm.tm_hour = hour;
		tm.tm_min = minute;
		tm.tm_sec = (int) second;

		// In the end, we need UTC, so never mind DST so that we only have timezone diff from mktime
		tm.tm_isdst = 0;

		// Offset between UTC and local time due to timezone
		time_now = time(NULL);
		gmtime_r(&time_now, &tm_utc);
		localtime_r(&time_now, &tm_local);

		// Time with correct offset (timezone-independent)
		timestamp = mktime(&tm) - (mktime(&tm_utc) - mktime(&tm_local));

		gta04_gps->location.timestamp = (GpsUtcTime) timestamp * 1000 + (GpsUtcTime) ((second - tm.tm_sec) * 1000);
	}

	return 0;
}

int gta04_gps_nmea_date(char *nmea)
{
	int date;
	char *buffer;

	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL) {
		gta04_gps->day = gta04_gps_nmea_parse_int(buffer, 0, 2);
		gta04_gps->month = gta04_gps_nmea_parse_int(buffer, 2, 2);
		gta04_gps->year = gta04_gps_nmea_parse_int(buffer, 4, strlen(buffer) - 4);
	}

	return 0;
}

int gta04_gps_nmea_coordinates(char *nmea)
{
	double latitude = 0;
	double longitude = 0;
	double minutes;
	int degrees;
	char *buffer;

	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL) {
		latitude = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));
		degrees = (int) floor(latitude / 100);
		minutes = latitude - degrees * 100.0f;
		latitude = degrees + minutes / 60.0f;
	}

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL && buffer[0] == 'S')
		latitude *= -1.0f;

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL) {
		longitude = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));
		degrees = (int) floor(longitude / 100);
		minutes = longitude - degrees * 100.0f;
		longitude = degrees + minutes / 60.0f;
	}

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL && buffer[0] == 'W')
		longitude *= -1.0f;

	gta04_gps->location.latitude = latitude;
	gta04_gps->location.longitude = longitude;

	if (latitude != 0 && longitude != 0)
		gta04_gps->location.flags |= GPS_LOCATION_HAS_LAT_LONG;

	return 0;
}

int gta04_gps_nmea_accurate(char *nmea)
{
	unsigned char accurate;
	char *buffer;

	if (gta04_gps == NULL)
		return -1;

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer == NULL)
		return -1;

	if (buffer[0] == 'A')
		accurate = 1;
	else
		accurate = 0;

	if (gta04_gps->accurate == 1 && accurate == 0) {
		memset(&gta04_gps->location, 0, sizeof(GpsLocation));
		gta04_gps->location.size = sizeof(GpsLocation);
	}

	gta04_gps->accurate = accurate;

	return 0;
}

int gta04_gps_nmea_gpgga(char *nmea)
{
	double altitude = 0;
	char *buffer;
	int i;

	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	gta04_gps_nmea_time(nmea);
	gta04_gps_nmea_coordinates(nmea);

	// Skip the following 3 arguments
	for (i = 6; i < 9; i++)
		gta04_gps_nmea_parse(nmea);

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL)
		altitude = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));

	gta04_gps->location.altitude = altitude;

	if (altitude != 0)
		gta04_gps->location.flags |= GPS_LOCATION_HAS_ALTITUDE;

	return 0;
}

int gta04_gps_nmea_gpgll(char *nmea)
{
	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	gta04_gps_nmea_coordinates(nmea);
	gta04_gps_nmea_time(nmea);
	gta04_gps_nmea_accurate(nmea);

	if (gta04_gps->accurate)
		gta04_gps_location_callback();

	return 0;
}

int gta04_gps_nmea_gpgsa(char *nmea)
{
	char *buffer;
	int prn;
	float accuracy = 0;
	int i;

	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	// Skip the first 2 arguments
	for (i = 0; i < 2; i++)
		gta04_gps_nmea_parse(nmea);

	gta04_gps->sv_status.used_in_fix_mask = 0;

	for (i = 0; i < channel_count; i++) {
		buffer = gta04_gps_nmea_parse(nmea);
		if (buffer == NULL)
			continue;

		prn = gta04_gps_nmea_parse_int(buffer, 0, strlen(buffer));
		if (prn != 0)
			gta04_gps->sv_status.used_in_fix_mask |= 1 << (prn - 1);
	}

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL) {
		accuracy = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));

		gta04_gps->location.accuracy = accuracy;
		gta04_gps->location.flags |= GPS_LOCATION_HAS_ACCURACY;
	}

	return 0;
}

int gta04_gps_nmea_gpgsv(char *nmea)
{
	int sv_count = 0;
	int sv_index;
	int count;
	int index;
	int prn;
	float elevation;
	float azimuth;
	float snr;
	char *buffer;
	int i;

	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer == NULL)
		return -1;

	count = gta04_gps_nmea_parse_int(buffer, 0, strlen(buffer));

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer == NULL)
		return -1;

	index = gta04_gps_nmea_parse_int(buffer, 0, strlen(buffer));

	if (index == 1)
		gta04_gps->sv_index = 0;

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL)
		sv_count = gta04_gps_nmea_parse_int(buffer, 0, strlen(buffer));

	if (sv_count > GPS_MAX_SVS)
		sv_count = GPS_MAX_SVS;

	for (sv_index = gta04_gps->sv_index; sv_index < sv_count; sv_index++) {
		buffer = gta04_gps_nmea_parse(nmea);
		if (buffer == NULL)
			break;

		prn = gta04_gps_nmea_parse_int(buffer, 0, strlen(buffer));

		snr = 0;
		elevation = 0;
		azimuth = 0;

		buffer = gta04_gps_nmea_parse(nmea);
		if (buffer != NULL)
			elevation = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));

		buffer = gta04_gps_nmea_parse(nmea);
		if (buffer != NULL)
			azimuth = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));

		buffer = gta04_gps_nmea_parse(nmea);
		if (buffer != NULL)
			snr = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));

		gta04_gps->sv_status.sv_list[sv_index].size = sizeof(GpsSvInfo);
		gta04_gps->sv_status.sv_list[sv_index].prn = prn;
		gta04_gps->sv_status.sv_list[sv_index].snr = snr;
		gta04_gps->sv_status.sv_list[sv_index].elevation = elevation;
		gta04_gps->sv_status.sv_list[sv_index].azimuth = azimuth;
	}

	gta04_gps->sv_index = sv_index;

	if (index == count) {
		gta04_gps->sv_status.num_svs = sv_count;

		gta04_gps_sv_status_callback();
	}

	return 0;
}

int gta04_gps_nmea_gprmc(char *nmea)
{
	float speed = 0;
	float bearing = 0;
	char *buffer;

	if (nmea == NULL)
		return -EINVAL;

	if (gta04_gps == NULL)
		return -1;

	gta04_gps_nmea_time(nmea);
	gta04_gps_nmea_accurate(nmea);
	gta04_gps_nmea_coordinates(nmea);

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL) {
		speed = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));

		// Convert knots (nautical miles per hour) to meters per second
		speed = speed * 1.852f / 3.6f;
	}

	buffer = gta04_gps_nmea_parse(nmea);
	if (buffer != NULL)
		bearing = gta04_gps_nmea_parse_float(buffer, 0, strlen(buffer));

	gta04_gps->location.speed = speed;
	gta04_gps->location.bearing = bearing;

	if (speed != 0)
		gta04_gps->location.flags |= GPS_LOCATION_HAS_SPEED;

	if (bearing != 0)
		gta04_gps->location.flags |= GPS_LOCATION_HAS_BEARING;

	gta04_gps_nmea_date(nmea);

	if (gta04_gps->accurate)
		gta04_gps_location_callback();

	return 0;
}

int gta04_gps_nmea_psrf103(unsigned char message, int interval)
{
	char nmea[74] = { 0 };
	char *buffer;
	int rc;

	if (gta04_gps == NULL)
		return -1;

	if (interval > 255)
		interval = 255;

	snprintf((char *) &nmea, sizeof(nmea), "PSRF103,%02d,00,%02d,01", message, interval);

	buffer = gta04_gps_nmea_prepare(nmea);
	if (buffer == NULL) {
		ALOGE("Preparing NMEA failed");
		return -1;
	}

	rc = gta04_gps_serial_write(&buffer, strlen(buffer) + 1);
	if (rc < 0) {
		ALOGE("Writing to serial failed");
		return -1;
	}

	return 0;
}
