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
#include <string.h>
#include <errno.h>

#include <hayes-ril.h>

//TODO: FIXME: only on GTA04!
#include "device/gta04/gsm-voice-route.h"
pthread_t gta04_voice_routing = 0;

/*
 * Calls list
 */

RIL_CallState at2ril_call_state(int state)
{
	switch(state) {
		case 0:
			//TODO: only on GTA04!
			/* If gta04_voice_routing thread is not yet running, let's start it */
			if (gta04_voice_routing == 0 || pthread_kill(gta04_voice_routing, 0) != 0) {
				ALOGD("Starting gta04_voice_routing");
				pthread_create(&gta04_voice_routing, NULL, gta04_start_voice_routing, NULL);
			}
			return RIL_CALL_ACTIVE;
		case 1:
			return RIL_CALL_HOLDING;
		case 2:
			return RIL_CALL_DIALING;
		case 3:
			return RIL_CALL_ALERTING;
		case 4:
			return RIL_CALL_INCOMING;
		case 5:
			return RIL_CALL_WAITING;
		default:
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

/*
 * USSD
 */
/*
 * Create a dynamically allocated copy of string,
 * changing the encoding from ISO-8859-15 to UTF-8.
 * This function is provided by Nominal Animal under a CC-BY-SA 3.0 license.
 * http://stackoverflow.com/questions/11258019/conversion-from-iso-8859-15-latin9-to-utf-8
 */
char *latin9_to_utf8(char *string)
{
    char   *result;
    size_t  n = 0;

    if (string) {
        const unsigned char  *s = (const unsigned char *)string;

        while (*s)
            if (*s < 128) {
                s++;
                n += 1;
            } else
            if (*s == 164) {
                s++;
                n += 3;
            } else {
                s++;
                n += 2;
            }
    }

    /* Allocate n+1 (to n+7) bytes for the converted string. */
    result = malloc((n | 7) + 1);
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }

    /* Clear the tail of the string, setting the trailing NUL. */
    memset(result + (n | 7) - 7, 0, 8);

    if (n) {
        const unsigned char  *s = (const unsigned char *)string;
        unsigned char        *d = (unsigned char *)result;

        while (*s)
            if (*s < 128) {
                *(d++) = *(s++);
            } else
            if (*s < 192) switch (*s) {
                case 164: *(d++) = 226; *(d++) = 130; *(d++) = 172; s++; break;
                case 166: *(d++) = 197; *(d++) = 160; s++; break;
                case 168: *(d++) = 197; *(d++) = 161; s++; break;
                case 180: *(d++) = 197; *(d++) = 189; s++; break;
                case 184: *(d++) = 197; *(d++) = 190; s++; break;
                case 188: *(d++) = 197; *(d++) = 146; s++; break;
                case 189: *(d++) = 197; *(d++) = 147; s++; break;
                case 190: *(d++) = 197; *(d++) = 184; s++; break;
                default:  *(d++) = 194; *(d++) = *(s++); break;
            } else {
                *(d++) = 195;
                *(d++) = *(s++) - 64;
            }
    }

    /* Done. Remember to free() the resulting string when no longer needed. */
    return result;
}

int at_cusd_unsol(char *string, int error)
{
	ALOGD("%s",string);
	int mode = 2;
	int code = -1;
	char tmp[200] = { 0 };
	char *result[2];
	char *mode_str;
	char *utf8_res;
	//TODO: FIXME: Read multiline strings!!!
	//sscanf(string, "+CUSD: %d,\"%200[^\"]\",%d", &mode, (char *) &tmp, &code);
	sscanf(string, "+CUSD: %d,\"%199c\",%d", &mode, (char *) &tmp, &code);
	utf8_res = latin9_to_utf8((char *) &tmp);
	asprintf(&mode_str, "%d", mode);
	result[0] = mode_str;
	result[1] = utf8_res;

	ALOGD("USSD: %d,\"%s\",%d", mode, utf8_res, code);

	switch(mode) {
		case 0:
			ril_request_unsolicited(RIL_UNSOL_ON_USSD, result, sizeof(result));
			at_send_callback("AT+CUSD=0", RIL_TOKEN_NULL, at_generic_callback);
			break;
		case 1:
			ril_request_unsolicited(RIL_UNSOL_ON_USSD, result, sizeof(result));
			break;
		case 2:
		case 3:
		case 4:
		case 5:
		default:
			at_send_callback("AT+CUSD=0", RIL_TOKEN_NULL, at_generic_callback);
			break;
	}
	free(utf8_res);
	return AT_STATUS_HANDLED;
}

void ril_request_send_ussd(void *data, size_t length, RIL_Token token)
{
	char* string;
	int rc;
	//code/rcs = 15 -> Use ISO8859-15 (Latin9)
	asprintf(&string, "AT+CUSD=1,\"%s\",15", (char *)data);
	rc = at_send_callback(string, token, at_generic_callback);
	free(string);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_cancel_ussd(void *data, size_t length, RIL_Token token)
{
	int rc;
	rc = at_send_callback("AT+CUSD=0", token, at_generic_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_dtmf(void *data, size_t length, RIL_Token token)
{
	int rc;
	char *str;

	asprintf(&str, "AT+VTS=%s", (char*)data);
	rc = at_send_callback(str, token, at_generic_callback);
	free(str);

	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_dtmf_start(void *data, size_t length, RIL_Token token)
{
	int rc;
	char *str;

	//TODO: This hould continuously play the sound until dtmf_stop is called
	asprintf(&str, "AT+VTS=%s", (char*)data);
	rc = at_send_callback(str, token, at_generic_callback);
	free(str);

	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_dtmf_stop(void *data, size_t length, RIL_Token token)
{
	//TODO: This is a dummy
	ril_request_complete(token, RIL_E_SUCCESS, NULL, 0);
}
