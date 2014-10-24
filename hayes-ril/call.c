/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2012-2013 Paul Kocialkowski <contact@paulk.fr>
 *
 * Parts of this code are based on htcgeneric-ril, reference-ril:
 * Copyright 2006-2011, htcgeneric-ril contributors
 * Copyright 2006, The Android Open Source Project
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

#define LOG_TAG "RIL-CALL"
#include <utils/Log.h>
#include <telephony/ril.h>

#include <hayes-ril.h>

//TODO: FIXME: only on GTA04!
#include "device/gta04/gsm-voice-route.h"
pthread_t gta04_voice_routing;

/*
 * Calls list
 */

RIL_CallState at2ril_call_state(int state)
{
	switch(state) {
		case 0:
			ALOGD("RIL_CALL_ACTIVE");
			//TODO: FIXME: start only once per call!
			//pthread_create(&gta04_voice_routing, NULL, gta04_start_voice_routing, NULL);
			//TODO: stop the thread later on
			return RIL_CALL_ACTIVE;
		case 1:
			ALOGD("RIL_CALL_HOLDING");
			return RIL_CALL_HOLDING;
		case 2:
			ALOGD("RIL_CALL_DIALING");
			return RIL_CALL_DIALING;
		case 3:
			ALOGD("RIL_CALL_ALERTING");
			return RIL_CALL_ALERTING;
		case 4:
			ALOGD("RIL_CALL_INCOMING");
			return RIL_CALL_INCOMING;
		case 5:
			ALOGD("RIL_CALL_WAITING");
			return RIL_CALL_WAITING;
		default:
			ALOGD("RIL_CALL_default");
			return -1;
	}
}

void ril_call_state_changed(void *data)
{
	ril_request_unsolicited(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

int at_clcc_callback(char *string, int error, RIL_Token token)
{
	struct timeval poll_interval = { 0, 750000 };
	RIL_Call **calls = NULL;
	char buffer[20] = { 0 };
	int values[6] = { 0 };
	int count = 0;
	char *p;
	int rc;
	int i;

	// CLCC can return a NULL string

	if (at_error(error) != AT_ERROR_OK)
		goto error;

	p = string;
	while (p != NULL) {
		count++;

		p = strchr(p, '\n');
		if (p != NULL)
			p++;
	}

	if (count == 0)
		goto complete;

	calls = (RIL_Call **) calloc(1, sizeof(RIL_Call *) * count);

	i = 0;
	p = string;
	while (p != NULL) {
		rc = sscanf(p, "+CLCC: %d,%d,%d,%d,%d,\"%20[^\"]\",%d", &values[0], &values[1], &values[2], &values[3], &values[4], (char *) &buffer, &values[5]);
		if (rc < 7)
			goto error;

		calls[i] = (RIL_Call *) calloc(1, sizeof(RIL_Call));
		calls[i]->index = values[0];
		calls[i]->isMT = values[1];
		calls[i]->state = at2ril_call_state(values[2]);
		calls[i]->isVoice = values[3] == 0;
		calls[i]->isMpty = values[4];
		calls[i]->toa = values[5];
		calls[i]->number = strdup(buffer);

		p = strchr(p, '\n');
		if (p != NULL)
			p++;

		i++;
	}

	count = i;
	if (count > 0)
		ril_request_timed_callback(ril_call_state_changed, NULL, &poll_interval);

complete:
	ril_request_complete(token, RIL_E_SUCCESS, calls, sizeof(RIL_Call *) * count);

	if (calls != NULL) {
		for (i = 0 ; i < count ; i++) {
			if (calls[i] == NULL)
				continue;

			if (calls[i]->number != NULL)
				free(calls[i]->number);
		}

		free(calls);
	}

	return AT_STATUS_HANDLED;

error:
	if (calls != NULL) {
		for (i = 0 ; i < count ; i++) {
			if (calls[i] == NULL)
				continue;

			if (calls[i]->number != NULL)
				free(calls[i]->number);
		}

		free(calls);
	}

	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_get_current_calls(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("AT+CLCC", token, at_clcc_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

int at_cring_unsol(char *string, int error)
{
	ril_request_unsolicited(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);

	return AT_STATUS_HANDLED;
}

/*
 * Answer
 */

void ril_request_answer(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("ATA", token, at_generic_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/*
 * Dial
 */

void ril_request_dial(void *data, size_t length, RIL_Token token)
{
	RIL_Dial *dial;
	char *string = NULL;
	char *clir = NULL;
	int rc;

	if (length < sizeof(RIL_Dial) || data == NULL)
		return;

	dial = (RIL_Dial *) data;

	switch(dial->clir) {
		case 1: clir = "I"; break;  // invocation
		case 2: clir = "i"; break;  // suppression
		default:
		case 0: clir = ""; break;   // subscription default
	}

	asprintf(&string, "ATD%s%s;", dial->address, clir);
	if (string == NULL)
		return;

	rc = at_send_callback(string, token, at_generic_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	free(string);
}

/*
 * Hangup
 */

void ril_request_hangup(void *data, size_t length, RIL_Token token)
{
	char *string = NULL;
	int index;
	int rc;

	if (length < sizeof(int) || data == NULL)
		return;

	index = *((int *) data);

	asprintf(&string, "AT+CHLD=1%d", index);
	if (string == NULL)
		return;

	rc = at_send_callback(string, token, at_generic_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	free(string);
}

void ril_request_hangup_waiting_or_background(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("AT+CHLD=0", token, at_generic_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_hangup_foreground_resume_background(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("AT+CHLD=1", token, at_generic_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_switch_waiting_or_holding_and_active(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("AT+CHLD=2", token, at_generic_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}
