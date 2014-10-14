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

#ifndef _HAYES_RIL_DEVICE_H_
#define _HAYES_RIL_DEVICE_H_

/*
 * Macros
 */

#define RIL_DEVICE_LOCK() pthread_mutex_lock(&ril_data->device->handlers->transport->mutex);
#define RIL_DEVICE_UNLOCK() pthread_mutex_unlock(&ril_data->device->handlers->transport->mutex);

#define DEV_GSM		RIL_DEVICE_TYPE_GSM
#define DEV_CDMA	RIL_DEVICE_TYPE_CDMA

/*
 * Structures
 */

enum ril_device_type {
	RIL_DEVICE_TYPE_GSM,
	RIL_DEVICE_TYPE_CDMA
};

struct ril_device_power_handlers {
	void *sdata;
	int (*sdata_create)(void **sdata);
	int (*sdata_destroy)(void *sdata);

	int (*power_on)(void *sdata);
	int (*power_off)(void *sdata);

	int (*suspend)(void *sdata);
	int (*resume)(void *sdata);

	int (*boot)(void *sdata);
};

struct ril_device_transport_handlers {
	void *sdata;
	int (*sdata_create)(void **sdata);
	int (*sdata_destroy)(void *sdata);

	int (*open)(void *sdata);
	int (*close)(void *sdata);

	int (*send)(void *sdata, void *data, int length);
	int (*recv)(void *sdata, void *data, int length);

	int (*recv_poll)(void *sdata);

	pthread_t recv_thread;
	pthread_mutex_t mutex;
};

struct ril_device_at_handlers {
	void *sdata;
	int (*sdata_create)(void **sdata);
	int (*sdata_destroy)(void *sdata);

	int (*power_on)(void *sdata);
	int (*power_off)(void *sdata);
	int (*setup)(void *sdata);

	int flags;
};

struct ril_device_handlers {
	struct ril_device_power_handlers *power;
	struct ril_device_transport_handlers *transport;
	struct ril_device_at_handlers *at;
};

struct ril_device {
	char *name;
	char *tag;
	void *sdata;

	enum ril_device_type type;
	struct ril_device_handlers *handlers;
};

/*
 * Functions
 */

void ril_device_register(struct ril_device **ril_device_p);

int ril_device_init(void);
int ril_device_deinit(void);
int ril_device_setup(void);

int ril_device_data_create(struct ril_device *ril_device);
int ril_device_data_destroy(struct ril_device *ril_device);

int ril_device_power_on(struct ril_device *ril_device);
int ril_device_power_off(struct ril_device *ril_device);
int ril_device_power_suspend(struct ril_device *ril_device);
int ril_device_power_resume(struct ril_device *ril_device);
int ril_device_power_boot(struct ril_device *ril_device);

int ril_device_transport_open(struct ril_device *ril_device);
int ril_device_transport_close(struct ril_device *ril_device);
int ril_device_transport_send(struct ril_device *ril_device, void *data, int length);
int ril_device_transport_recv(struct ril_device *ril_device, void *data, int length);
int ril_device_transport_recv_poll(struct ril_device *ril_device);

int ril_device_transport_recv_loop(struct ril_device *ril_device);
void *ril_device_transport_recv_thread(void *data);
int ril_device_transport_recv_thread_start(void);
int ril_device_at_power_on(struct ril_device *ril_device);
int ril_device_at_power_off(struct ril_device *ril_device);
int ril_device_at_setup(struct ril_device *ril_device);

#endif
