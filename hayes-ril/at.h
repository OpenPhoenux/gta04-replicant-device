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

#ifndef _HAYES_RIL_AT_H_
#define _HAYES_RIL_AT_H_

/*
 * Macros
 */

#define AT_REQUESTS_LOCK() pthread_mutex_lock(&ril_data->at_data.requests_mutex);
#define AT_REQUESTS_UNLOCK() pthread_mutex_unlock(&ril_data->at_data.requests_mutex);
#define AT_RESPONSES_LOCK() pthread_mutex_lock(&ril_data->at_data.responses_mutex);
#define AT_RESPONSES_UNLOCK() pthread_mutex_unlock(&ril_data->at_data.responses_mutex);
#define AT_RESPONSES_QUEUE_LOCK() pthread_mutex_lock(&ril_data->at_data.responses_queue_mutex);
#define AT_RESPONSES_QUEUE_UNLOCK() pthread_mutex_unlock(&ril_data->at_data.responses_queue_mutex);
#define AT_LOCK_LOCK() pthread_mutex_lock(&ril_data->at_data.lock_mutex);
#define AT_LOCK_UNLOCK() pthread_mutex_unlock(&ril_data->at_data.lock_mutex);

/*
 * Values
 */

#define AT_RECV_BYTES_MAX	1024

enum {
	AT_FLAG_DELIMITERS_NL	= (1 << 0),
	AT_FLAG_DELIMITERS_CR	= (1 << 1),
	AT_FLAG_LOCKED		= (1 << 2),
	AT_FLAG_URGENT		= (1 << 3),
};

enum {
	AT_ERROR_UNDEF,
	AT_ERROR_OK,
	AT_ERROR_OK_EXPECT_DATA,
	AT_ERROR_CONNECT,
	AT_ERROR_ERROR,
	AT_ERROR_CME_ERROR,
	AT_ERROR_NO_CARRIER,
	AT_ERROR_INTERNAL,
};

enum {
	AT_CME_ERROR_PHONE_FAILURE = 0,
	AT_CME_ERROR_PHONE_NO_CONNECTION,
	AT_CME_ERROR_PHONE_LINK_RESERVED,
	AT_CME_ERROR_OPERATION_NOT_ALLOWED,
	AT_CME_ERROR_OPERATION_NOT_SUPPORTED,
	AT_CME_ERROR_PH_SIM_PIN_REQD,
	AT_CME_ERROR_PH_FSIM_PIN_REQD,
	AT_CME_ERROR_PH_FSIM_PUK_REQD,
	AT_CME_ERROR_SIM_NOT_INSERTED = 10,
	AT_CME_ERROR_SIM_PIN_REQD,
	AT_CME_ERROR_SIM_PUK_REQD,
	AT_CME_ERROR_SIM_FAILURE,
	AT_CME_ERROR_SIM_BUSY,
	AT_CME_ERROR_SIM_WRONG,
	AT_CME_ERROR_INCORRECT_PASSWORD,
	AT_CME_ERROR_SIM_PIN2_REQD,
	AT_CME_ERROR_SIM_PUK2_REQD,
	AT_CME_ERROR_MEMORY_FULL = 20,
	AT_CME_ERROR_INVALID_INDEX,
	AT_CME_ERROR_NOT_FOUND,
	AT_CME_ERROR_MEMORY_FAILURE,
	AT_CME_ERROR_TEXT_STRING_TOO_LONG,
	AT_CME_ERROR_INVALID_CHARS_IN_TEXT,
	AT_CME_ERROR_DIAL_STRING_TOO_LONG,
	AT_CME_ERROR_INVALID_CHARS_IN_DIAL,
	AT_CME_ERROR_NO_NETWORK_SERVICE = 30,
	AT_CME_ERROR_NETWORK_TIMEOUT,
	AT_CME_ERROR_NETWORK_NOT_ALLOWED,
	AT_CME_ERROR_NETWORK_PERS_PIN_REQD = 40,
	AT_CME_ERROR_NETWORK_PERS_PUK_REQD,
	AT_CME_ERROR_NETWORK_SUBSET_PERS_PIN_REQD,
	AT_CME_ERROR_NETWORK_SUBSET_PERS_PUK_REQD,
	AT_CME_ERROR_PROVIDER_PERS_PIN_REQD,
	AT_CME_ERROR_PROVIDER_PERS_PUK_REQD,
	AT_CME_ERROR_CORP_PERS_PIN_REQD,
	AT_CME_ERROR_CORP_PERS_PUK_REQD,
	AT_CME_ERROR_PH_SIM_PUK_REQD,
	AT_CME_ERROR_UNKNOWN_ERROR = 100,
	AT_CME_ERROR_ILLEGAL_MS = 103,
	AT_CME_ERROR_ILLEGAL_ME = 106,
	AT_CME_ERROR_GPRS_NOT_ALLOWED,
	AT_CME_ERROR_PLMN_NOT_ALLOWED = 111,
	AT_CME_ERROR_LOCATION_NOT_ALLOWED,
	AT_CME_ERROR_ROAMING_NOT_ALLOWED,
	AT_CME_ERROR_TEMPORARY_NOT_ALLOWED = 126,
	AT_CME_ERROR_SERVICE_OPTION_NOT_SUPPORTED = 132,
	AT_CME_ERROR_SERVICE_OPTION_NOT_SUBSCRIBED,
	AT_CME_ERROR_SERVICE_OPTION_OUT_OF_ORDER,
	AT_CME_ERROR_UNSPECIFIED_GPRS_ERROR = 148,
	AT_CME_ERROR_PDP_AUTHENTICATION_FAILED,
	AT_CME_ERROR_INVALID_MOBILE_CLASS,
	AT_CME_ERROR_OPERATION_TEMP_NOT_ALLOWED = 256,
	AT_CME_ERROR_CALL_BARRED,
	AT_CME_ERROR_PHONE_BUSY,
	AT_CME_ERROR_USER_ABORT,
	AT_CME_ERROR_INVALID_DIAL_STRING,
	AT_CME_ERROR_SS_NOT_EXECUTED,
	AT_CME_ERROR_SIM_BLOCKED,
	AT_CME_ERROR_INVALID_BLOCK,
	AT_CME_ERROR_SIM_POWERED_DOWN = 772
};

enum {
	// Requests
	AT_STATUS_WAITING,
	AT_STATUS_PENDING,
	AT_STATUS_SENT,
	AT_STATUS_FREEZED,
	// Responses
	AT_STATUS_UNHANDLED,
	AT_STATUS_HANDLED,
};

/*
 * Structures
 */

struct at_request {
	char *string;
	RIL_Token token;

	int (*callback)(char *string, int error, RIL_Token token);

	int flags;
	int status;
};


struct at_response {
	char *string;
	int error;

	struct at_request *request;
};

struct at_data {
	struct list_head *requests;
	pthread_mutex_t requests_mutex;

	struct list_head *responses;
	pthread_mutex_t responses_mutex;

	pthread_mutex_t responses_queue_mutex;

	int lock_error;
	pthread_mutex_t lock_mutex;

	int freezed;
};

/*
 * Functions
 */

// Utilities
int at_strings_compare(char *major, char *minor);

// Error
int at_error(int error);
int at_cme_error(int error);
int at_error_process(char *string);
char *at_error_string(int error);

// Generic callbacks
int at_generic_callback(char *string, int error, RIL_Token token);
int at_generic_callback_locked(char *string, int error, RIL_Token token);

// Response
int at_response_register(char *string, int error, struct at_request *request);
int at_response_unregister(struct at_response *response);
struct at_response *at_response_find(void);
int at_response_dispatch(struct at_response *response);
int at_response_process(char *data, int length);

// Request
struct at_request *at_request_register(char *string, RIL_Token token,
	int (*callback)(char *string, int error, RIL_Token token), int flags);
int at_request_unregister(struct at_request *request);
struct at_request *at_request_find_status(int status);
struct at_request *at_request_find_flags(int flags);
struct at_request *at_request_find_token(RIL_Token token);
int at_request_send(struct at_request *request);
int at_request_send_next(void);

// Requests
int at_requests_freeze(void);
int at_requests_unfreeze(void);

// Send
int at_send(char *string, RIL_Token token,
	int (*callback)(char *string, int error, RIL_Token token), int flags);
int at_send_callback(char *string, RIL_Token token,
	int (*callback)(char *string, int error, RIL_Token token));
int at_send_locked(char *string, int flags);
int at_send_string_locked(char *string);
int at_send_request_data(RIL_Token token, char *data, int length);

#endif
