/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2012-2013 Paul Kocialkowski <contact@paulk.fr>
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

#include <errno.h>
#include <pthread.h>

#define LOG_TAG "RIL-DEV"
#include <utils/Log.h>

#include <hayes-ril.h>

/*
 * Global
 */

int ril_device_init(void)
{
	struct ril_device *ril_device;
	int rc;

	ril_device = ril_data->device;

	rc = ril_device_data_create(ril_device);
	if (rc < 0) {
		ALOGE("Failed to create device data!");
		return -1;
	}

	rc = ril_device_power_on(ril_device);
	if (rc < 0) {
		ALOGE("Failed to power on device!");
		return -1;
	}

	rc = ril_device_power_boot(ril_device);
	if (rc < 0) {
		ALOGE("Failed to boot device!");
		return -1;
	}

	rc = ril_device_transport_open(ril_device);
	if (rc < 0) {
		ALOGE("Failed to open device!");
		return -1;
	}

	return 0;
}

int ril_device_deinit(void)
{
	struct ril_device *ril_device;

	ril_device = ril_data->device;

	ril_device_transport_close(ril_device);

	ril_device_power_off(ril_device);

	ril_device_data_destroy(ril_device);

	return 0;
}

int ril_device_setup(void)
{
	struct ril_device *ril_device;
	int rc;

	ril_device = ril_data->device;

	rc = ril_device_at_setup(ril_device);
	if (rc < 0) {
		ALOGE("Failed to setup device!");
		return -1;
	}

	return 0;
}

/*
 * Data
 */

int ril_device_data_create(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->power == NULL) {
		ALOGE("Missing device power handlers!");
		return -1;
	}

	if (ril_device->handlers->power->sdata_create == NULL) {
		ALOGE("Missing device power data handlers!");
		return -1;
	}

	ALOGD("Creating data for power handlers...");
	rc = ril_device->handlers->power->sdata_create(&ril_device->handlers->power->sdata);
	if (rc < 0) {
		ALOGE("Creating data for power handlers failed!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	if (ril_device->handlers->transport->sdata_create == NULL) {
		ALOGE("Missing device transport data handlers!");
		return -1;
	}

	ALOGD("Creating data for transport handlers...");
	ril_device->handlers->transport->sdata_create(&ril_device->handlers->transport->sdata);
	if (rc < 0) {
		ALOGE("Creating data for transport handlers failed!");
		return -1;
	}

	ALOGD("Creating mutex for transport handlers...");
	pthread_mutex_init(&(ril_device->handlers->transport->mutex), NULL);

	// Missing AT handlers is not fatal
	if (ril_device->handlers->at == NULL) {
		ALOGE("Missing device AT handlers!");
		return 0;
	}

	if (ril_device->handlers->at->sdata_create == NULL) {
		ALOGE("Missing device AT data handlers!");
		return 0;
	}

	ALOGD("Creating data for AT handlers...");
	ril_device->handlers->at->sdata_create(&ril_device->handlers->at->sdata);
	if (rc < 0) {
		ALOGE("Creating data for AT handlers failed!");
		return -1;
	}

	return 0;
}

int ril_device_data_destroy(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->power == NULL) {
		ALOGE("Missing device power handlers!");
		return -1;
	}

	if (ril_device->handlers->power->sdata_destroy == NULL) {
		ALOGE("Missing device power data handlers!");
		return -1;
	}

	ALOGD("Destroying data for power handlers...");
	rc = ril_device->handlers->power->sdata_destroy(ril_device->handlers->power->sdata);
	if (rc < 0) {
		ALOGE("Destroying data for power handlers failed!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	if (ril_device->handlers->transport->sdata_destroy == NULL) {
		ALOGE("Missing device transport data handlers!");
		return -1;
	}

	ALOGD("Destroying data for transport handlers...");
	ril_device->handlers->transport->sdata_destroy(ril_device->handlers->transport->sdata);
	if (rc < 0) {
		ALOGE("Destroying data for transport handlers failed!");
		return -1;
	}

	ALOGD("Destroying mutex for transport handlers...");
	pthread_mutex_destroy(&(ril_device->handlers->transport->mutex));

	// Missing AT handlers is not fatal
	if (ril_device->handlers->at == NULL) {
		ALOGE("Missing device AT handlers!");
		return 0;
	}

	if (ril_device->handlers->at->sdata_create == NULL) {
		ALOGE("Missing device AT data handlers!");
		return 0;
	}

	ALOGD("Creating data for AT handlers...");
	ril_device->handlers->at->sdata_destroy(ril_device->handlers->at->sdata);
	if (rc < 0) {
		ALOGE("Destroying data for AT handlers failed!");
		return -1;
	}

	return 0;
}

/*
 * Power
 */

int ril_device_power_on(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->power == NULL) {
		ALOGE("Missing device power handlers!");
		return -1;
	}

	if (ril_device->handlers->power->power_on == NULL) {
		ALOGE("Missing device power on handler!");
		return -1;
	}

	ALOGD("Powering modem on...");

	rc = ril_device->handlers->power->power_on(ril_device->handlers->power->sdata);
	return rc;
}

int ril_device_power_off(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->power == NULL) {
		ALOGE("Missing device power handlers!");
		return -1;
	}

	if (ril_device->handlers->power->power_off == NULL) {
		ALOGE("Missing device power off handler!");
		return -1;
	}

	ALOGD("Powering modem off...");

	rc = ril_device->handlers->power->power_off(ril_device->handlers->power->sdata);
	return rc;
}

int ril_device_power_boot(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->power == NULL) {
		ALOGE("Missing device power handlers!");
		return -1;
	}

	if (ril_device->handlers->power->boot == NULL) {
		ALOGE("Missing device power boot handler!");
		return -1;
	}

	ALOGD("Booting modem...");

	rc = ril_device->handlers->power->boot(ril_device->handlers->power->sdata);
	return rc;
}

/*
 * Transport
 */


int ril_device_transport_open(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	if (ril_device->handlers->transport->open == NULL) {
		ALOGE("Missing device transport open handler!");
		return -1;
	}

	ALOGD("Opening modem...");

	rc = ril_device->handlers->transport->open(ril_device->handlers->transport->sdata);
	return rc;
}

int ril_device_transport_close(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	if (ril_device->handlers->transport->close == NULL) {
		ALOGE("Missing device transport close handler!");
		return -1;
	}

	ALOGD("Closing modem...");

	rc = ril_device->handlers->transport->close(ril_device->handlers->transport->sdata);
	return rc;
}

int ril_device_transport_send(struct ril_device *ril_device, void *data, int length)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	if (ril_device->handlers->transport->send == NULL) {
		ALOGE("Missing device transport send handler!");
		return -1;
	}

	RIL_DEVICE_LOCK();
	rc = ril_device->handlers->transport->send(ril_device->handlers->transport->sdata, data, length);
	RIL_DEVICE_UNLOCK();

	return rc;
}

int ril_device_transport_recv(struct ril_device *ril_device, void *data, int length)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	if (ril_device->handlers->transport->recv == NULL) {
		ALOGE("Missing device transport recv handler!");
		return -1;
	}

	RIL_DEVICE_LOCK();
	rc = ril_device->handlers->transport->recv(ril_device->handlers->transport->sdata, data, length);
	RIL_DEVICE_UNLOCK();

	return rc;
}

int ril_device_transport_recv_poll(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	if (ril_device->handlers->transport->recv_poll == NULL) {
		ALOGE("Missing device transport recv poll handler!");
		return -1;
	}

	rc = ril_device->handlers->transport->recv_poll(ril_device->handlers->transport->sdata);
	return rc;
}

int ril_device_transport_recv_loop(struct ril_device *ril_device)
{
	char buffer[AT_RECV_BYTES_MAX] = { 0 };

	int failures = 0;
	int run = 1;

	int length = 0;
	int rc;
	int i;

work:
	while (run) {
		rc = ril_device_transport_recv_poll(ril_device);
		if (rc < 0) {
			ALOGE("RIL device transport recv poll failed!");
			break;
		}

		rc = ril_device_transport_recv(ril_device, (void *) &buffer, AT_RECV_BYTES_MAX);
		if (rc <= 0) {
			ALOGE("RIL device transport recv failed!");
			break;
		}

		length = rc;

		// Read works now
		if (failures)
			failures = 0;

		at_response_process((char *) &buffer, length);
	}

	ALOGE("RIL device transport recv loop stopped!");
	failures++;

	at_requests_freeze();

	if (failures < 4) {
		ALOGD("Reopening transport...");

		ril_device_transport_close(ril_device);
	} else if (failures < 7) {
		ALOGD("Powering off and on again...");

		ril_device_transport_close(ril_device);
		ril_device_power_off(ril_device);
		ril_device_power_on(ril_device);

		ALOGD("Reopening transport...");
	} else {
		ALOGE("Something bad is going on, it is advised to reboot!");

		return -1;
	}

	rc = ril_device_transport_open(ril_device);
	if (rc < 0) {
		ALOGE("Unable to reopen transport!");
		run = 0;
	} else {
		run = 1;
		at_requests_unfreeze();
	}

	goto work;
}

void *ril_device_transport_recv_thread(void *data)
{
	struct ril_device *ril_device = (struct ril_device *) data;
	int rc;
	int i;

	rc = ril_device_transport_recv_loop(ril_device);
	ALOGE("RIL device transport recv thread returned with %d, aborting!", rc);

	ril_device_deinit();

	return NULL;
}

int ril_device_transport_recv_thread_start(void)
{
	struct ril_device *ril_device;
	pthread_attr_t attr;
	int rc;

	ril_device = ril_data->device;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	if (ril_device->handlers->transport == NULL) {
		ALOGE("Missing device transport handlers!");
		return -1;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	rc = pthread_create(&(ril_device->handlers->transport->recv_thread), &attr, ril_device_transport_recv_thread, (void *) ril_device);
	if (rc != 0) {
		ALOGE("Creating transport recv thread failed!");
		return -1;
	}

	return 0;
}

/*
 * AT
 */

int ril_device_at_power_on(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	// Missing AT handlers is not fatal
	if (ril_device->handlers->at == NULL) {
		ALOGE("Missing device AT handlers!");
		return 0;
	}

	if (ril_device->handlers->at->power_on == NULL) {
		ALOGE("Missing device AT power on handler!");
		return 0;
	}

	rc = ril_device->handlers->at->power_on(ril_device->handlers->at->sdata);
	return rc;
}

int ril_device_at_power_off(struct ril_device *ril_device)
{
	int rc;

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	// Missing AT handlers is not fatal
	if (ril_device->handlers->at == NULL) {
		ALOGE("Missing device AT handlers!");
		return 0;
	}

	if (ril_device->handlers->at->power_off == NULL) {
		ALOGE("Missing device AT power off handler!");
		return 0;
	}

	rc = ril_device->handlers->at->power_off(ril_device->handlers->at->sdata);
	return rc;
}

int ril_device_at_setup(struct ril_device *ril_device)
{
	int rc;

	// Builtin

	// Echo enabled, send results, verbose enabled
	at_send_locked("ATE1Q0V1", AT_FLAG_URGENT);

	// Extended errors
	at_send_locked("AT+CMEE=1", AT_FLAG_URGENT);

	// Detailed rings, service reporting
	at_send_locked("AT+CRC=1;+CR=1", AT_FLAG_URGENT);

	// SMS PDU mode
	at_send_locked("AT+CMGF=0", AT_FLAG_URGENT);

	// Network registration notifications
	rc = at_send_locked("AT+CREG=2", AT_FLAG_URGENT);
	if (at_error(rc) != AT_ERROR_OK) {
		ALOGD("Modem doesn't support AT+CREG=2");
		at_send_locked("AT+CREG=1", AT_FLAG_URGENT);
	}

	// Handler

	if (ril_device->handlers == NULL) {
		ALOGE("Missing device handlers!");
		return -1;
	}

	// Missing AT handlers is not fatal
	if (ril_device->handlers->at == NULL) {
		ALOGE("Missing device AT handlers!");
		return 0;
	}

	if (ril_device->handlers->at->setup == NULL) {
		ALOGE("Missing device AT setup handler!");
		return 0;
	}

	rc = ril_device->handlers->at->setup(ril_device->handlers->at->sdata);
	return rc;
}
