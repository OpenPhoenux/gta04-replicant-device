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

#define LOG_TAG "RIL-NET"
#include <utils/Log.h>
#include <telephony/ril.h>

#include <hayes-ril.h>

/*
 * Signal Strength
 */

#if RIL_VERSION >= 6
void at2ril_signal_strength(RIL_SignalStrength_v6 *ss, int *values)
#else
void at2ril_signal_strength(RIL_SignalStrength *ss, int *values)
#endif
{
	const int dbm_table[8] = {135,125,115,105,95,85,75,70};
	const int edbm_table[8] = {120,110,100,90,80,75,70,65};
	const int ecio_table[8] = {200,150,130,120,110,100,90,80};

#if RIL_VERSION >= 6
	memset(ss, 0, sizeof(RIL_SignalStrength_v6));
#else
	memset(ss, 0, sizeof(RIL_SignalStrength));
#endif
	if (ril_data->device->type == DEV_GSM) {
		ss->GW_SignalStrength.signalStrength = values[0];
		ss->GW_SignalStrength.bitErrorRate = values[1];
	} else {
		/* 1st # is CDMA, 2nd is EVDO */
		ss->CDMA_SignalStrength.dbm = dbm_table[values[0]];
		ss->CDMA_SignalStrength.ecio = ecio_table[values[0]];
		ss->EVDO_SignalStrength.dbm = edbm_table[values[1]];
		ss->EVDO_SignalStrength.ecio = ecio_table[values[1]];
		ss->EVDO_SignalStrength.signalNoiseRatio = values[1];
	}
}

int at_csq_callback(char *string, int error, RIL_Token token)
{
#if RIL_VERSION >= 6
	RIL_SignalStrength_v6 ss;
#else
	RIL_SignalStrength ss;
#endif
	int values[2] = { 0 };
	int rc;

	if (!at_strings_compare("+CSQ", string) && at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) != AT_ERROR_OK || string == NULL)
		goto error;

	rc = sscanf(string, "+CSQ: %d,%d", &values[0], &values[1]);
	if (rc < 2)
		goto error;

	at2ril_signal_strength(&ss, (int *) &values);

#if RIL_VERSION >= 6
	ril_request_complete(token, RIL_E_SUCCESS, &ss, sizeof(RIL_SignalStrength_v6));
#else
	ril_request_complete(token, RIL_E_SUCCESS, &ss, sizeof(RIL_SignalStrength));
#endif

complete:
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_signal_strength(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("AT+CSQ", token, at_csq_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/*
 * Network registration
 */

int at_creg_unsol(char *string, int error)
{
	int state = 0;
	char lac[10] = { 0 };
	char cid[10] = { 0 };
	int rc;

	rc = sscanf(string, "+CREG: %d,\"%10[^\"]\",\"%10[^\"]\"", &state, (char *) &lac, (char *) &cid);
	if (rc < 1)
		goto complete;

	asprintf(&ril_data->registration_state[0], "%d", state);
	if (lac[0] != '\0')
		asprintf(&ril_data->registration_state[1], "%s", lac);
	if (cid[0] != '\0')
		asprintf(&ril_data->registration_state[2], "%s", cid);

#if RIL_VERSION >= 6
	ril_request_unsolicited(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0);
#else
	ril_request_unsolicited(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
#endif

complete:
	return AT_STATUS_HANDLED;
}

int at_creg_callback(char *string, int error, RIL_Token token)
{
	char *registration_state[3] = { NULL };
	int unsol, state = 0;
	char lac[10] = { 0 };
	char cid[10] = { 0 };
	int rc;
	int i;

	if (!at_strings_compare("+CREG", string) && at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) != AT_ERROR_OK || string == NULL)
		goto error;

	rc = sscanf(string, "+CREG: %d,%d,\"%10[^\"]\",\"%10[^\"]\"", &unsol, &state, (char *) &lac, (char *) &cid);
	if (rc < 2)
		goto error;

	asprintf(&registration_state[0], "%d", state);
	if (lac[0] != '\0')
		asprintf(&registration_state[1], "%s", lac);
	if (cid[0] != '\0')
		asprintf(&registration_state[2], "%s", cid);

	ril_request_complete(token, RIL_E_SUCCESS, registration_state,  sizeof(registration_state));

	for (i = 0 ; i < 3 ; i++)
		if (registration_state[i] != NULL)
			free(registration_state[i]);

complete:
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}
#if RIL_VERSION >= 6
void ril_request_voice_registration_state(void *data, size_t length, RIL_Token token)
#else
void ril_request_registration_state(void *data, size_t length, RIL_Token token)
#endif
{
	int rc;
	int i;

	if (ril_data->registration_state[0] != NULL) {
		ril_request_complete(token, RIL_E_SUCCESS, ril_data->registration_state, sizeof(ril_data->registration_state));

		for (i = 0 ; i < 3 ; i++)
			if (ril_data->registration_state[i] != NULL)
				free(ril_data->registration_state[i]);

		memset(&ril_data->registration_state, 0, sizeof(ril_data->registration_state));
	} else {
		rc = at_send_callback("AT+CREG?", token, at_creg_callback);
		if (rc < 0)
			ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
}

/*
 * Operator
 */

int at_cops_callback(char *string, int error, RIL_Token token)
{
	char *operator[3] = { NULL };
	char buffer[50] = { 0 };
	int values[3] = { 0 };
	char *p;
	int rc;
	int i;

	if (!at_strings_compare("+COPS", string) && at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) != AT_ERROR_OK || string == NULL)
		goto error;

	p = string;
	while (p != NULL) {
		rc = sscanf(p, "+COPS: %d,%d,\"%50[^\"]\",%d", &values[0], &values[1], (char *) &buffer, &values[2]);
		if (rc < 1)
			goto error;

		if (rc < 2)
			break;

		i = values[1];
		if (i >= 0 && i <= 2)
			asprintf(&operator[i], "%s", buffer);

		p = strchr(p, '\n');
		if (p != NULL)
			p++;
	}

	ril_request_complete(token, RIL_E_SUCCESS, operator,  sizeof(operator));

	for (i = 0 ; i < 3 ; i++)
		if (operator[i] != NULL)
			free(operator[i]);

complete:
	return AT_STATUS_HANDLED;

error:
	for (i = 0 ; i < 3 ; i++)
		if (operator[i] != NULL)
			free(operator[i]);

	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_operator(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?", token, at_cops_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}
