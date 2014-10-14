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

#include <telephony/ril.h>

#include <at.h>
#include <device.h>
#include <util.h>

#ifndef _HAYES_RIL_H_
#define _HAYES_RIL_H_

/*
 * Macros
 */

#define RIL_LOG_LOCK() pthread_mutex_lock(&ril_data->log_mutex);
#define RIL_LOG_UNLOCK() pthread_mutex_unlock(&ril_data->log_mutex);
#define RIL_DATA_LOCK() pthread_mutex_lock(&ril_data->mutex);
#define RIL_DATA_UNLOCK() pthread_mutex_unlock(&ril_data->mutex);


#define ril_request_complete(t, e, d, l) ril_data->env->OnRequestComplete(t, e, d, l)
#define ril_request_unsolicited(r, d, l) ril_data->env->OnUnsolicitedResponse(r, d, l)
#define ril_request_timed_callback(c, d, t) ril_data->env->RequestTimedCallback(c, d, t);

/*
 * Values
 */

#define RIL_TOKEN_UNSOL	(RIL_Token) 0xffff
#define RIL_TOKEN_NULL	(RIL_Token) 0x0000

/*
 * Structures
 */

struct ril_dispatch_handler {
	char *string;
	int (*callback)(char *string, int error);
};

struct ril_request_handler {
	int request;
	void (*callback)(void *data, size_t length, RIL_Token t);
};

struct ril_data {
	struct RIL_Env *env;

	struct ril_device *device;
	struct at_data at_data;

	pthread_t dispatch_thread;
	pthread_mutex_t log_mutex;

	RIL_RadioState radio_state;

	char *registration_state[3];

	pthread_mutex_t mutex;

	int sim_ready_initialized;
	RIL_Token imsi_token;

	struct list_head *outgoing_sms;
};

/*
 * Globals
 */

extern struct ril_data *ril_data;

/*
 * Functions
 */

// Call
int at_cring_unsol(char *string, int error);
void ril_request_get_current_calls(void *data, size_t length, RIL_Token token);
void ril_request_answer(void *data, size_t length, RIL_Token token);
void ril_request_dial(void *data, size_t length, RIL_Token token);
void ril_request_hangup(void *data, size_t length, RIL_Token token);
void ril_request_hangup_waiting_or_background(void *data, size_t length, RIL_Token token);
void ril_request_hangup_foreground_resume_background(void *data, size_t length, RIL_Token token);
void ril_request_switch_waiting_or_holding_and_active(void *data, size_t length, RIL_Token token);

// Network 
int at_csq_callback(char *string, int error, RIL_Token token);
void ril_request_signal_strength(void *data, size_t length, RIL_Token token);
int at_creg_unsol(char *string, int error);
#if RIL_VERSION >= 6
void ril_request_voice_registration_state(void *data, size_t length, RIL_Token token);
#else
void ril_request_registration_state(void *data, size_t length, RIL_Token token);
#endif
void ril_request_operator(void *data, size_t length, RIL_Token token);
void ril_request_get_preferred_network_type(void *data, size_t length, RIL_Token token);
void ril_request_set_preferred_network_type(void *data, size_t length, RIL_Token token);
void ril_request_data_registration_state(void *data, size_t length, RIL_Token token);
void ril_request_query_network_selection_mode(void *data, size_t length, RIL_Token token);
void ril_request_query_available_networks(void *data, size_t length, RIL_Token token);
void ril_request_get_neighboring_cell_ids(void *data, size_t length, RIL_Token token);

// Power
void ril_request_radio_power(void *data, size_t length, RIL_Token token);

// SIM
int at_cpin_callback(char *string, int error, RIL_Token token);
void ril_request_get_sim_status(void *data, size_t length, RIL_Token token);
void ril_request_enter_sim_pin(void *data, size_t length, RIL_Token token);
void ril_request_sim_io(void *data, size_t length, RIL_Token token);
void ril_request_query_facility_lock(void *data, size_t length, RIL_Token token);
void ril_request_get_imsi(void *data, size_t length, RIL_Token token);

// SMS
void check_sms_on_sim();
int at_cmt_unsol(char *string, int error);
int at_cmti_unsol(char *string, int error);
int at_cmgr_callback(char *string, int error, RIL_Token token);
void ril_request_send_sms(void *data, size_t length, RIL_Token token);
void ril_request_report_sms_memory_status(void *data, size_t length, RIL_Token token);
void ril_request_delete_sms_on_sim(void *data, size_t length, RIL_Token token);
void ril_request_sms_acknowledge(void *data, size_t length, RIL_Token token);

// SMS extra
int ril_outgoing_sms_send_next(void);

// Device
void ril_request_baseband_version(void *data, size_t length, RIL_Token token);
void ril_request_get_imei(void *data, size_t length, RIL_Token token);
void ril_request_screen_state(void *data, size_t length, RIL_Token token);

// Device extra
void ril_device_sim_ready_setup(void);

// Gprs
void ril_request_setup_data_call(void *data, size_t length, RIL_Token token);
void ril_request_deactivate_data_call(void *data, size_t length, RIL_Token token);

#endif
