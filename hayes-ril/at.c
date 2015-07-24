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

#define LOG_TAG "RIL-AT"
#include <utils/Log.h>

#include <hayes-ril.h>

int confirm_next_pending = 0;

/*
 * Utilities
 */

int at_strings_compare(char *major, char *minor)
{
	int major_length;
	int minor_length;

	if (major == NULL || minor == NULL)
		return -EINVAL;

	major_length = strlen(major);
	minor_length = strlen(minor);

	// We can't check against the whole major string
	if (major_length > minor_length)
		return 0;

	if (strncmp(major, minor, major_length) == 0)
		return 1;
	else
		return 0;
}

/*
 * Error
 */

int at_error(int error)
{
	return error & 0xff;
}

int at_cme_error(int error)
{
	return (error >> 8);
}

int at_error_process(char *string)
{
	int error;
	int d = -1;

	int rc;

	if (string == NULL)
		return -EINVAL;

	error = AT_ERROR_UNDEF;

	if (at_strings_compare("OK", string))
		error = AT_ERROR_OK;
	if (at_strings_compare("> ", string))
		error = AT_ERROR_OK_EXPECT_DATA;
	if (at_strings_compare("ERROR", string))
		error = AT_ERROR_ERROR;
	if (at_strings_compare("CONNECT", string))
		error = AT_ERROR_CONNECT;
	if (at_strings_compare("+CME ERROR", string)) {
		error = AT_ERROR_CME_ERROR;
		rc = sscanf(string, "+CME ERROR: %d", &d);
		if (rc == 1 && d >= 0)
			error |= d << 8;
	}
	if (at_strings_compare("NO CARRIER", string))
		error = AT_ERROR_NO_CARRIER;

	return error;
}

char *at_error_string(int error)
{
	switch (error) {
		case AT_ERROR_OK:
			return "OK";
		case AT_ERROR_OK_EXPECT_DATA:
			return "OK EXPECT DATA";
		case AT_ERROR_ERROR:
			return "ERROR";
		case AT_ERROR_CONNECT:
			return "CONNECT";
		case AT_ERROR_CME_ERROR:
			return "CME ERROR";
		case AT_ERROR_NO_CARRIER:
			return "NO CARRIER";
		case AT_ERROR_INTERNAL:
			return "INTERNAL ERROR";
		case AT_ERROR_UNDEF:
		default:
			return "UNDEF";
	}
}

int at_delimiter_process(char *string)
{
	if (string == NULL)
		return -EINVAL;

	switch (string[0]) {
		case '\r':
		case '\n':
		case '\0':
			return 1;
	}

	return 0;
}

/*
 * Generic callbacks
 */

int at_generic_callback(char *string, int error, RIL_Token token)
{
	if (at_error(error) == AT_ERROR_UNDEF)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) == AT_ERROR_OK)
		ril_request_complete(token, RIL_E_SUCCESS, NULL, 0);
	else
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	return AT_STATUS_HANDLED;
}

int at_generic_callback_locked(char *string, int error, RIL_Token token)
{
	ril_data->at_data.lock_error = error;

	AT_LOCK_UNLOCK();

	return AT_STATUS_HANDLED;
}

/*
 * Response
 */

int at_response_register(char *string, int error, struct at_request *request)
{
	struct at_response *response;
	struct list_head *list_end;
	struct list_head *list;

	response = calloc(1, sizeof(struct at_response));
	if (response == NULL)
		return -ENOMEM;

	AT_RESPONSES_LOCK();

	response->string = string != NULL ? strdup(string) : NULL;
	response->error = error;
	response->request = request;

	list_end = ril_data->at_data.responses;
	while (list_end != NULL && list_end->next != NULL)
		list_end = list_end->next;

	list = list_head_alloc((void *) response, list_end, NULL);

	if (ril_data->at_data.responses == NULL)
		ril_data->at_data.responses = list;

//	ALOGD("%d response(s) registered", list_head_count(ril_data->at_data.responses));

	AT_RESPONSES_UNLOCK();

	return 0;
}

int at_response_unregister(struct at_response *response)
{
	struct list_head *list;

	if (response == NULL)
		return -EINVAL;

	AT_RESPONSES_LOCK();

	list = ril_data->at_data.responses;
	while (list != NULL) {
		if (list->data == (void *) response) {
			if (response->string != NULL)
				free(response->string);
			memset(response, 0, sizeof(struct at_response));
			free(response);

			if (list == ril_data->at_data.responses)
				ril_data->at_data.responses = list->next;

			list_head_free(list);

			break;
		}
list_continue:
		list = list->next;
	}

//	ALOGD("%d response(s) registered", list_head_count(ril_data->at_data.responses));

	AT_RESPONSES_UNLOCK();

	return 0;
}

struct at_response *at_response_find(void)
{
	struct at_response *response;
	struct list_head *list;

	AT_RESPONSES_LOCK();

	list = ril_data->at_data.responses;
	while (list != NULL) {
		response = (struct at_response *) list->data;
		if (response == NULL)
			goto list_continue;

		AT_RESPONSES_UNLOCK();
		return response;

list_continue:
		list = list->next;
	}

	AT_RESPONSES_UNLOCK();

	return NULL;
}

int at_response_dispatch(struct at_response *response)
{
	struct list_head *list;
	struct at_request *request;
	int status = AT_STATUS_UNHANDLED;

	if (response == NULL)
		return -EINVAL;

	request = response->request;

	if (request != NULL) {
		if (request->callback != NULL)
			status = request->callback(response->string, response->error, request->token);
		else
			status = AT_STATUS_HANDLED;

		// Even if unhandled, it will go unsol, so unregister the request
		if (response->error != AT_ERROR_OK_EXPECT_DATA)
			at_request_unregister(request);
	}

	return status;
}

int at_response_process(char *data, int length)
{
	ALOGD("at_response_process()");
	static char buffer[AT_RECV_BYTES_MAX] = { 0 };
	char *string;
	char *line;
	int error;
	int rc;

	struct at_request *request_pending;
	struct at_request *request_sent;

	static int oo, o = 0;
	int i, p, l;

	if (data == NULL || length <= 0)
		return -EINVAL;

	request_sent = at_request_find_status(AT_STATUS_SENT);
	request_pending = at_request_find_status(AT_STATUS_PENDING);
	if (request_sent != NULL)
		ALOGD("at_response_process: SENT   : %s", request_sent->string);
	else
		ALOGD("at_response_process: SENT   : NULL");
	if (request_pending != NULL)
		ALOGD("at_response_process: PENDING: %s", request_pending->string);
	else
		ALOGD("at_response_process: PENDING: NULL");

	if (confirm_next_pending == 1 && request_pending != NULL) {
		ALOGD("CONFIRM NEXT PENDING");
		confirm_next_pending = 0;
		ALOGD("Confirmation of pending request"); //received echo of previouly sent (PENDING) command/request

		request_pending->status = AT_STATUS_SENT; //awaiting response/OK/ERROR
		request_sent = request_pending;
		request_pending = NULL;
	}

	p = l = 0;

	ril_data_log(data, length);

	//loop over received buffer (from transport) character for character, create line if delimiter matches
	for (i = 0 ; i < length ; i++) {
		if (at_delimiter_process(&data[i]) || i == (length - 1)) {
			l = i - p;

			if (o + l > AT_RECV_BYTES_MAX) {
				ALOGE("Buffer overflow!");
				return -1;
			}

			if (l > 0) {
				oo = 0;

				if (o > 0) {
					buffer[o] = '\n';
					oo = o;
					o++;
				}

				memcpy(&buffer[o], &data[p], l + 1);
				line = &buffer[o];
				o += l;
				if (at_delimiter_process(&data[i]))
					buffer[o] = '\0';
				else
					buffer[o+1] = '\0';

				ALOGD("at_response_process (i=%d, len=%d), line: %s", i, length, line);
				//if the received bytes contain an echo of a pending request
				if (request_pending != NULL && at_strings_compare(request_pending->string, line)) {
					ALOGD("Confirmation of pending request"); //received echo of previouly sent (PENDING) command/request

					request_pending->status = AT_STATUS_SENT; //awaiting response/OK/ERROR
					request_sent = request_pending;
					request_pending = NULL;

					//TODO: adopt for AT+VTS special case only and not general NOWAIT
					if(request_sent != NULL && (request_sent->flags & AT_FLAG_NOWAIT)) {
						//TODO: Do not wait for a response of this request
						confirm_next_pending = 1; //echo of next command after AT+VTS=... is lost, so we need to confirm it manually
						ALOGD("Confirmed NOWAIT request: %s", request_sent->string);
						rc = at_response_register("NOWAIT HACK", AT_ERROR_OK, request_sent); //inject response for nowait request
						if (rc >= 0)
							AT_RESPONSES_QUEUE_UNLOCK();
					}

					o = 0;
					p = i + 1;

					continue;
				}

				string = (char *) &buffer;

				error = at_error_process(line);

				ril_recv_log(line, error);
				if (error != AT_ERROR_UNDEF) {
					if (oo > 0)
						buffer[oo] = '\0';
					else
						string = NULL;
				}

				// Either we don't need an error or we already have one
				if (request_sent == NULL || error != AT_ERROR_UNDEF) {
					if (request_sent != NULL)
						ALOGD("at_response_register: %s", request_sent->string);
					else
						ALOGD("at_response_register: NULL");
					rc = at_response_register(string, error, request_sent);
					if (rc >= 0)
						AT_RESPONSES_QUEUE_UNLOCK();

					o = 0;
				}
			}

			p = i + 1;
		}
	}

	return 0;
}

/*
 * Request
 */

struct at_request *at_request_register(char *string, RIL_Token token,
	int (*callback)(char *string, int error, RIL_Token token), int flags)
{
	struct at_request *request;
	struct list_head *list_end;
	struct list_head *list;

	if (string == NULL)
		return NULL;

	request = calloc(1, sizeof(struct at_request));
	if (request == NULL)
		return NULL;

	AT_REQUESTS_LOCK();

	request->string = strdup(string);
	request->token = token;
	request->callback = callback;
	request->flags = flags;
	request->status = AT_STATUS_WAITING;

	list_end = ril_data->at_data.requests;
	while (list_end != NULL && list_end->next != NULL)
		list_end = list_end->next;

	list = list_head_alloc((void *) request, list_end, NULL);

	if (ril_data->at_data.requests == NULL)
		ril_data->at_data.requests = list;

//	ALOGD("%d request(s) registered", list_head_count(ril_data->at_data.requests));

	AT_REQUESTS_UNLOCK();

	return request;
}

int at_request_unregister(struct at_request *request)
{
	struct list_head *list;

	if (request == NULL)
		return -EINVAL;

	AT_REQUESTS_LOCK();

	list = ril_data->at_data.requests;
	while (list != NULL) {
		if (list->data == (void *) request) {
			if (request->string != NULL)
				free(request->string);
			memset(request, 0, sizeof(struct at_request));
			free(request);

			if (list == ril_data->at_data.requests)
				ril_data->at_data.requests = list->next;

			list_head_free(list);

			break;
		}
list_continue:
		list = list->next;
	}

//	ALOGD("%d request(s) registered", list_head_count(ril_data->at_data.requests));

	AT_REQUESTS_UNLOCK();

	return 0;
}

struct at_request *at_request_find_status(int status)
{
	struct at_request *request;
	struct list_head *list;

	AT_REQUESTS_LOCK();

	list = ril_data->at_data.requests;
	while (list != NULL) {
		request = (struct at_request *) list->data;
		if (request == NULL)
			goto list_continue;

		if (request->status == status) {
			AT_REQUESTS_UNLOCK();
			return request;
		}

list_continue:
		list = list->next;
	}

	AT_REQUESTS_UNLOCK();

	return NULL;
}

struct at_request *at_request_find_flags(int flags)
{
	struct at_request *request;
	struct list_head *list;

	AT_REQUESTS_LOCK();

	list = ril_data->at_data.requests;
	while (list != NULL) {
		request = (struct at_request *) list->data;
		if (request == NULL)
			goto list_continue;

		if (request->flags & flags) {
			AT_REQUESTS_UNLOCK();
			return request;
		}

list_continue:
		list = list->next;
	}

	AT_REQUESTS_UNLOCK();

	return NULL;
}

struct at_request *at_request_find_token(RIL_Token token)
{
	struct at_request *request;
	struct list_head *list;

	AT_REQUESTS_LOCK();

	list = ril_data->at_data.requests;
	while (list != NULL) {
		request = (struct at_request *) list->data;
		if (request == NULL)
			goto list_continue;

		if (request->token == token) {
			AT_REQUESTS_UNLOCK();
			return request;
		}

list_continue:
		list = list->next;
	}

	AT_REQUESTS_UNLOCK();

	return NULL;
}

int at_request_send(struct at_request *request)
{
	struct ril_device *ril_device;

	char *data = NULL;
	int length = 0;

	char separator[3] = "\r\n";
	int rc;
	int i;

	if (request == NULL || request->string == NULL)
		return -EINVAL;

	ril_device = ril_data->device;

	i = 0;

	if (request->flags & AT_FLAG_DELIMITERS_CR) {
		separator[i] = '\r';
		i++;
		separator[i] = '\0';
	}

	if (request->flags & AT_FLAG_DELIMITERS_NL) {
		separator[i] = '\n';
		i++;
		separator[i] = '\0';
	}

	asprintf(&data, "%s%s%s", separator, request->string, separator);
	if (data == NULL)
		return -ENOMEM;

	length = strlen(data);

	ril_data_log(data, length);
	ril_send_log(request->string);

	rc = ril_device_transport_send(ril_device, data, length);
	free(data);
	if (rc <= 0) {
		ALOGE("Sending data failed!");
		return -1;
	}

	return 0;
}

int at_request_send_next(void)
{
	struct at_request *request;
	int rc;

	request = at_request_find_flags(AT_FLAG_URGENT);
	if (request != NULL && request->status == AT_STATUS_WAITING)
		goto send;

	if (at_request_find_status(AT_STATUS_SENT) != NULL ||
		at_request_find_status(AT_STATUS_PENDING) != NULL ||
		at_request_find_status(AT_STATUS_FREEZED) != NULL) {
		ALOGD("There is still at least one unanswered request!");
		request = at_request_find_status(AT_STATUS_SENT);
		if (request != NULL)
			ALOGD("AT_STATUS_SENT: %s (%p)", request->string, (void*)request->token);
		request = at_request_find_status(AT_STATUS_PENDING);
		if (request != NULL)
			ALOGD("AT_STATUS_PENDING: %s (%p)", request->string, (void*)request->token);
		request = at_request_find_status(AT_STATUS_FREEZED);
		if (request != NULL)
			ALOGD("AT_STATUS_FREEZED: %s (%p)", request->string, (void*)request->token);

		return -1;
	}

	if (ril_data->at_data.freezed) {
		ALOGD("AT requests are freezed!");
		return 0;
	}

	request = at_request_find_status(AT_STATUS_WAITING);
	if (request == NULL) {
		ALOGD("No waiting request to send");
		return 0;
	}

send:
	rc = at_request_send(request);
	if (rc < 0) {
		at_request_unregister(request);
		return -1;
	}

	request->status = AT_STATUS_PENDING; //awaiting the echo of this command on the transport

	return 0;
}

/*
 * Requests freeze
 */

int at_requests_freeze(void)
{
	struct at_request *request;

	while ((request = at_request_find_status(AT_STATUS_SENT)) != NULL)
		request->status = AT_STATUS_FREEZED;

	while ((request = at_request_find_status(AT_STATUS_PENDING)) != NULL)
		request->status = AT_STATUS_FREEZED;

	ril_data->at_data.freezed = 1;

	return 0;
}

void at_requests_unfreeze_complete(void *data)
{
	struct timeval interval = { 3, 0 };
	struct at_request *request;

	ril_device_setup();

	while ((request = at_request_find_status(AT_STATUS_FREEZED)) != NULL)
		request->status = AT_STATUS_WAITING;

	ril_data->at_data.freezed = 0;

	at_request_send_next();

	// Request SIM PIN in case this was a full modem reset
	ril_request_timed_callback((RIL_TimedCallback) ril_request_get_sim_status, NULL, &interval);

}

int at_requests_unfreeze(void)
{
	ril_request_timed_callback(at_requests_unfreeze_complete, NULL, NULL);

	return 0;
}

/*
 * Send
 */

int at_send(char *string, RIL_Token token,
	int (*callback)(char *string, int error, RIL_Token token), int flags)
{
	struct at_request *request;

	if (callback == NULL) {
		ALOGE("%s: No callback function, aborting", __func__);
		return -1;
	}

	request = at_request_register(string, token, callback, flags);
	if (request == NULL) {
		ALOGE("%s: Failed to register AT request", __func__);
		return -1;
	}

	at_request_send_next();

	if (flags & AT_FLAG_LOCKED)
		AT_LOCK_LOCK();

	return 0;
}

int at_send_callback(char *string, RIL_Token token,
	int (*callback)(char *string, int error, RIL_Token token))
{
	return at_send(string, token, callback, 0);
}

int at_send_callback_nowait(char *string, RIL_Token token,
	int (*callback)(char *string, int error, RIL_Token token))
{
	return at_send(string, token, callback, AT_FLAG_NOWAIT);
}

/*
 * Make sure to never call locked send functions from the dispatch thread:
 * that would prevent the request from ever being answered.
 * It is only safe to call locked send functions from the main thread.
 */

int at_send_locked(char *string, int flags)
{
	int rc;

	rc = at_send(string, NULL, at_generic_callback_locked, flags | AT_FLAG_LOCKED);
	if (rc < 0)
		return -1;

	return ril_data->at_data.lock_error;
}

int at_send_string_locked(char *string)
{
	return at_send_locked(string, 0);
}

int at_send_request_data(RIL_Token token, char *data, int length)
{
	struct ril_device *ril_device;
	struct at_request *request;
	char *buffer;
	int rc;

	if (data == NULL || length <= 0)
		return -EINVAL;

	ril_device = ril_data->device;

	request = at_request_find_token(token);
	if (request == NULL || request->status != AT_STATUS_SENT)
		return -1;

	buffer = calloc(1, length + 1);
	if (buffer == NULL)
		return -ENOMEM;

	memcpy(buffer, data, length);
	buffer[length] = '\032';

	ril_data_log(buffer, length + 1);

	rc = ril_device_transport_send(ril_device, buffer, length + 1);

	free(buffer);

	if (rc <= 0) {
		ALOGE("Sending data failed!");
		return -1;
	}

	return 0;
}
