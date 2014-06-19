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

#define LOG_TAG "RIL"
#include <utils/Log.h>
#include <telephony/ril.h>

#include <hayes-ril.h>
#include <device.h>

#define RIL_VERSION_STRING "Hayes RIL"

struct ril_data *ril_data = NULL;

struct ril_dispatch_handler ril_dispatch_handlers[] = {
	{
		.string = "+CREG",
		.callback = at_creg_unsol,
	},
	{
		.string = "+CRING",
		.callback = at_cring_unsol,
	},
};

struct ril_request_handler ril_request_handlers[] = {
	// Call
	{
		.request = RIL_REQUEST_GET_CURRENT_CALLS,
		.callback = ril_request_get_current_calls,
	},
	{
		.request = RIL_REQUEST_ANSWER,
		.callback = ril_request_answer,
	},
	{
		.request = RIL_REQUEST_DIAL,
		.callback = ril_request_dial,
	},
	{
		.request = RIL_REQUEST_HANGUP,
		.callback = ril_request_hangup,
	},
	{
		.request = RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND,
		.callback = ril_request_hangup_waiting_or_background,
	},
	{
		.request = RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND,
		.callback = ril_request_hangup_foreground_resume_background,
	},
	{
		.request = RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE,
		.callback = ril_request_switch_waiting_or_holding_and_active,
	},
	// Network
	{
		.request = RIL_REQUEST_SIGNAL_STRENGTH,
		.callback = ril_request_signal_strength,
	},
#if RIL_VERSION >= 6
	{
		.request = RIL_REQUEST_VOICE_REGISTRATION_STATE,
		.callback = ril_request_voice_registration_state,
	},
#else
	{
		.request = RIL_REQUEST_REGISTRATION_STATE,
		.callback = ril_request_registration_state,
	},
#endif
	{
		.request = RIL_REQUEST_OPERATOR,
		.callback = ril_request_operator,
	},
	// Power
	{
		.request = RIL_REQUEST_RADIO_POWER,
		.callback = ril_request_radio_power,
	},
	// SIM
	{
		.request = RIL_REQUEST_GET_SIM_STATUS,
		.callback = ril_request_get_sim_status,
	},
	{
		.request = RIL_REQUEST_ENTER_SIM_PIN,
		.callback = ril_request_enter_sim_pin,
	},
	{
		.request = RIL_REQUEST_SIM_IO,
		.callback = ril_request_sim_io,
	},
	{
		.request = RIL_REQUEST_QUERY_FACILITY_LOCK,
		.callback = ril_request_query_facility_lock,
	},
	// SMS
	{
		.request = RIL_REQUEST_SEND_SMS,
		.callback = ril_request_send_sms,
	},
	// Device
	{
		.request = RIL_REQUEST_BASEBAND_VERSION,
		.callback = ril_request_baseband_version,
	},
	// Gprs
	{
		.request = RIL_REQUEST_SETUP_DATA_CALL,
		.callback = ril_request_setup_data_call,
	},
};

void ril_on_request(int request, void *data, size_t length, RIL_Token token)
{
	int count;
	int i;

	count = sizeof(ril_request_handlers) / sizeof(struct ril_request_handler);

	for (i = 0 ; i < count ; i++) {
		if (ril_request_handlers[i].callback == NULL)
			continue;

		if (ril_request_handlers[i].request == request) {
			ril_request_handlers[i].callback(data, length, token);
			return;
		}
	}

	ril_request_complete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
}

const char *ril_get_version(void)
{
	return RIL_VERSION_STRING;
}

RIL_RadioState ril_on_state_request(void)
{
	ALOGD("State request");

	return ril_data->radio_state;
}

int ril_on_supports(int request)
{
	int count;
	int i;

	count = sizeof(ril_request_handlers) / sizeof(struct ril_request_handler);

	for (i = 0 ; i < count ; i++) {
		if (ril_request_handlers[i].callback == NULL)
			continue;

		if (ril_request_handlers[i].request == request)
			return 1;
	}

	return 0;
}

void ril_on_cancel(RIL_Token t)
{
	return;
}

static const RIL_RadioFunctions ril_ops = {
	RIL_VERSION,
	ril_on_request,
	ril_on_state_request,
	ril_on_supports,
	ril_on_cancel,
	ril_get_version
};

void *ril_dispatch(void *data)
{
	struct at_response *response = NULL;
	int status;
	int count;
	int i;

wait:
	AT_RESPONSES_QUEUE_LOCK();

	while (1) {
		response = at_response_find();
		if (response == NULL)
			break;

		if (response->string != NULL)
			ALOGD("RIL DISPATCH [%s]", response->string);

		status = at_response_dispatch(response);
		if (status == AT_STATUS_HANDLED)
			goto next;

		if (response->string == NULL)
			goto next;

		count = sizeof(ril_dispatch_handlers) / sizeof(struct ril_dispatch_handler);
		for (i = 0 ; i < count ; i++) {
			if (ril_dispatch_handlers[i].string == NULL || ril_dispatch_handlers[i].callback == NULL)
				continue;

			if (at_strings_compare(ril_dispatch_handlers[i].string, response->string)) {
				status = ril_dispatch_handlers[i].callback(response->string, response->error);
				if (status == AT_STATUS_HANDLED)
					goto next;
			}
		}

next:
		at_response_unregister(response);
	}

	// Attempt to send queued requests now
	at_request_send_next();

	goto wait;

	return NULL;
}

int ril_dispatch_thread_start(void)
{
	pthread_attr_t attr;
	int rc;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	rc = pthread_create(&ril_data->dispatch_thread, &attr, ril_dispatch, NULL);
	if (rc != 0) {
		ALOGE("Creating dispatch thread failed!");
		return -1;
	}

	return 0;
}

int ril_data_init(void)
{
	if (ril_data != NULL)
		return -EINVAL;

	ril_data = (struct ril_data *) calloc(1, sizeof(struct ril_data));
	if (ril_data == NULL)
		return -ENOMEM;

	ril_data->radio_state = RADIO_STATE_OFF;

	pthread_mutex_init(&ril_data->log_mutex, NULL);
	pthread_mutex_init(&ril_data->at_data.requests_mutex, NULL);
	pthread_mutex_init(&ril_data->at_data.lock_mutex, NULL);

	// First lock
	AT_RESPONSES_QUEUE_LOCK();
	AT_LOCK_LOCK();

	return 0;
}

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
	int rc;

	if (env == NULL)
		return NULL;

	rc = ril_data_init();
	if (rc < 0 || ril_data == NULL) {
		ALOGE("Failed to init RIL data!");
		return NULL;
	}

	ril_data->env = (struct RIL_Env *) env;

	ril_device_register(&ril_data->device);
	if (ril_data->device == NULL) {
		ALOGE("Failed to register RIL device!");
		return NULL;
	}

	ALOGD("Starting %s for device: %s", RIL_VERSION_STRING, ril_data->device->name);

	rc = ril_device_init();
	if (rc < 0) {
		ALOGE("Failed to init device!");
		ril_device_deinit();

		return NULL;
	}

	rc = ril_device_transport_recv_thread_start();
	if (rc < 0) {
		ALOGE("Failed to start device recv thread!");
		ril_device_deinit();

		return NULL;
	}

	rc = ril_dispatch_thread_start();
	if (rc < 0) {
		ALOGE("Failed to start dispatch thread!");
		ril_device_deinit();

		return NULL;
	}

	rc = ril_device_setup();
	if (rc < 0) {
		ALOGE("Failed to setup device!");
		ril_device_deinit();

		return NULL;
	}

	ALOGD("Initialization complete");

	return &ril_ops;
}
