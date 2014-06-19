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

#include <ctype.h>

#define LOG_TAG "RIL-SIM"
#include <utils/Log.h>
#include <telephony/ril.h>

#include <hayes-ril.h>

RIL_RadioState at2ril_sim_status(char *string, int error)
{
	char buffer[10] = { 0 };
	int rc;
	int i;

	if (at_error(error) == AT_ERROR_CME_ERROR) {
		switch (at_cme_error(error)) {
			case AT_CME_ERROR_PH_SIM_PIN_REQD:
			case AT_CME_ERROR_PH_SIM_PUK_REQD:
			case AT_CME_ERROR_PH_FSIM_PIN_REQD:
			case AT_CME_ERROR_PH_FSIM_PUK_REQD:
			case AT_CME_ERROR_SIM_NOT_INSERTED:
			case AT_CME_ERROR_SIM_PIN_REQD:
			case AT_CME_ERROR_SIM_PUK_REQD:
			case AT_CME_ERROR_SIM_PIN2_REQD:
			case AT_CME_ERROR_SIM_PUK2_REQD:
			case AT_CME_ERROR_SIM_BLOCKED:
			case AT_CME_ERROR_INCORRECT_PASSWORD:
				return RADIO_STATE_SIM_LOCKED_OR_ABSENT;
			case AT_CME_ERROR_SIM_FAILURE:
			case AT_CME_ERROR_SIM_BUSY:
			case AT_CME_ERROR_SIM_POWERED_DOWN:
			case AT_CME_ERROR_SIM_WRONG:
			default:
				goto complete;
		}
	} else if (at_error(error) == AT_ERROR_OK && string != NULL) {
		rc = sscanf(string, "+CPIN: %9[^\n]", (char *) &buffer);
		if (rc < 1)
			goto complete;

		if (at_strings_compare("SIM PIN", buffer))
			return RADIO_STATE_SIM_LOCKED_OR_ABSENT;
		else if (at_strings_compare("SIM PUK", buffer))
			return RADIO_STATE_SIM_LOCKED_OR_ABSENT;
		else if (at_strings_compare("READY", buffer))
			return RADIO_STATE_SIM_READY;
	}

complete:
	// Fallback
	return ril_data->radio_state;
}

#if RIL_VERSION >= 6
RIL_RadioState at2ril_card_status(RIL_CardStatus_v6 *card_status, char *string, int error)
#else
RIL_RadioState at2ril_card_status(RIL_CardStatus *card_status, char *string, int error)
#endif
{
	char buffer[10] = { 0 };
	RIL_RadioState radio_state;

	int app_status_array_length = 0;
	int app_index = -1;
	int rc;
	int i;

	static RIL_AppStatus app_status_array[] = {
		// SIM_ABSENT = 0
		{ RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
		// SIM_NOT_READY = 1
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
		// SIM_READY = 2
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
		NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
		// SIM_PIN = 3
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		// SIM_PUK = 4
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
		// SIM_BLOCKED = 5
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_PERM_BLOCKED, RIL_PINSTATE_UNKNOWN },
		// SIM_NETWORK_PERSO = 6
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		// SIM_NETWORK_SUBSET_PERSO = 7
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK_SUBSET,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		// SIM_CORPORATE_PERSO = 8
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_CORPORATE,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		// SIM_SERVICE_PROVIDER_PERSO = 9
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_SERVICE_PROVIDER,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
	};

	app_status_array_length = sizeof(app_status_array) / sizeof(RIL_AppStatus);

	if (app_status_array_length > RIL_CARD_MAX_APPS)
		app_status_array_length = RIL_CARD_MAX_APPS;

	radio_state = at2ril_sim_status(string, error);

	if (at_error(error) == AT_ERROR_CME_ERROR) {
		switch (at_cme_error(error)) {
			case AT_CME_ERROR_SIM_NOT_INSERTED:
				card_status->card_state = RIL_CARDSTATE_ABSENT;
				card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
				app_index = 0;
				break;
			case AT_CME_ERROR_PH_SIM_PIN_REQD:
			case AT_CME_ERROR_PH_FSIM_PIN_REQD:
			case AT_CME_ERROR_SIM_PIN_REQD:
			case AT_CME_ERROR_INCORRECT_PASSWORD:
				card_status->card_state = RIL_CARDSTATE_PRESENT;
				card_status->universal_pin_state = RIL_PINSTATE_ENABLED_NOT_VERIFIED;
				app_index = 3;
				break;
			case AT_CME_ERROR_PH_SIM_PUK_REQD:
			case AT_CME_ERROR_PH_FSIM_PUK_REQD:
			case AT_CME_ERROR_SIM_PUK_REQD:
				card_status->card_state = RIL_CARDSTATE_PRESENT;
				card_status->universal_pin_state = RIL_PINSTATE_ENABLED_BLOCKED;
				app_index = 4;
				break;
			case AT_CME_ERROR_SIM_PIN2_REQD:
			case AT_CME_ERROR_SIM_PUK2_REQD:
			case AT_CME_ERROR_SIM_BLOCKED:
				card_status->card_state = RIL_CARDSTATE_PRESENT;
				card_status->universal_pin_state = RIL_PINSTATE_ENABLED_PERM_BLOCKED;
				app_index = 5;
				break;
			case AT_CME_ERROR_SIM_FAILURE:
			case AT_CME_ERROR_SIM_BUSY:
			case AT_CME_ERROR_SIM_POWERED_DOWN:
			case AT_CME_ERROR_SIM_WRONG:
			default:
				card_status->card_state = RIL_CARDSTATE_PRESENT;
				card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
				app_index = 1;
				break;
		}
	} else if (at_error(error) == AT_ERROR_OK && string != NULL) {
		rc = sscanf(string, "+CPIN: %9[^\n]", (char *) &buffer);
		if (rc < 1)
			goto complete;

		if (at_strings_compare("SIM PIN", buffer)) {
			card_status->card_state = RIL_CARDSTATE_PRESENT;
			card_status->universal_pin_state = RIL_PINSTATE_ENABLED_NOT_VERIFIED;
			app_index = 3;
		} else if (at_strings_compare("SIM PUK", buffer)) {
			card_status->card_state = RIL_CARDSTATE_PRESENT;
			card_status->universal_pin_state = RIL_PINSTATE_ENABLED_BLOCKED;
			app_index = 4;
		} else if (at_strings_compare("READY", buffer)) {
			card_status->card_state = RIL_CARDSTATE_PRESENT;
			card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
			app_index = 2;
		}
	}

	// Fallback
	if (app_index == -1) {
		card_status->card_state = RIL_CARDSTATE_PRESENT;
		card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
		app_index = 1;
	}

	// Initialize the apps
	for (i = 0 ; i < app_status_array_length ; i++) {
		memcpy((void *) &(card_status->applications[i]), (void *) &(app_status_array[i]), sizeof(RIL_AppStatus));
	}
	for (i = app_status_array_length ; i < RIL_CARD_MAX_APPS ; i++) {
		memset((void *) &(card_status->applications[i]), 0, sizeof(RIL_AppStatus));
	}

	card_status->gsm_umts_subscription_app_index = app_index;
	card_status->cdma_subscription_app_index = app_index;
	card_status->num_applications = app_status_array_length;

	ALOGD("Selecting application #%d on %d", app_index, app_status_array_length);

complete:
	return radio_state;
}

int at_cpin_callback(char *string, int error, RIL_Token token)
{
#if RIL_VERSION >= 6
	RIL_CardStatus_v6 card_status;
#else
	RIL_CardStatus card_status;
#endif
	RIL_RadioState radio_state;

	if (!at_strings_compare("+CPIN", string) && at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (token == RIL_TOKEN_UNSOL) {
		radio_state = at2ril_sim_status(string, error);
		ril_data->radio_state = radio_state;

		ril_request_unsolicited(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
	} else {
		radio_state = at2ril_card_status(&card_status, string, error);
		ril_data->radio_state = radio_state;

		ril_request_complete(token, RIL_E_SUCCESS, &card_status, sizeof(card_status));

		ril_request_unsolicited(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
	}

	return AT_STATUS_HANDLED;
}

void ril_request_get_sim_status(void *data, size_t length, RIL_Token token)
{
	int rc;

	rc = at_send_callback("AT+CPIN?", token, at_cpin_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

int at_cpin_unlock_callback(char *string, int error, RIL_Token token)
{
	int tries = -1;

	if (at_error(error) == AT_ERROR_OK) {
		ril_request_complete(token, RIL_E_SUCCESS, &tries, sizeof(int *));
	} else {
		if (at_cme_error(error) == AT_CME_ERROR_INCORRECT_PASSWORD)
			ril_request_complete(token, RIL_E_PASSWORD_INCORRECT, &tries, sizeof(int *));
		else
			ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	return AT_STATUS_HANDLED;
}

void ril_request_enter_sim_pin(void *data, size_t length, RIL_Token token)
{
	char *string = NULL;
	char **pin;
	int rc;

	if (data == NULL || length < sizeof(char *))
		return;

	pin = (char **) data;

	if (length == 2 * sizeof(char *) && pin[1] != NULL)
		asprintf(&string, "AT+CPIN=%s,%s", pin[0], pin[1]);
	else
		asprintf(&string, "AT+CPIN=%s", pin[0]);

	if (string == NULL)
		return;

	rc = at_send_callback(string, token, at_cpin_unlock_callback);

	free(string);

	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

int at_crsm_callback(char *string, int error, RIL_Token token)
{
	RIL_SIM_IO_Response sim_io_response;
	char buffer[500] = { 0 };
	int rc;

	if (!at_strings_compare("+CRSM", string) && at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) != AT_ERROR_OK || string == NULL)
		goto error;

	memset(&sim_io_response, 0, sizeof(RIL_SIM_IO_Response));
	rc = sscanf(string, "+CRSM: %d,%d,\"%500[^\"]\"", &sim_io_response.sw1, &sim_io_response.sw2, (char *) &buffer);
	if (rc < 2)
		goto error;

	sim_io_response.simResponse = (char *) &buffer;

	ril_request_complete(token, RIL_E_SUCCESS, &sim_io_response, sizeof(RIL_SIM_IO_Response));

complete:
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_sim_io(void *data, size_t length, RIL_Token token)
{
#if RIL_VERSION >= 6
	RIL_SIM_IO_v6 *sim_io_request = NULL;
#else
	RIL_SIM_IO *sim_io_request = NULL;
#endif
	char *string = NULL;
	int rc;

	if (data == NULL || length < sizeof(*sim_io_request))
		return;

#if RIL_VERSION >= 6
	sim_io_request = (RIL_SIM_IO_v6 *) data;
#else
	sim_io_request = (RIL_SIM_IO *) data;
#endif

	if (sim_io_request->data != NULL) {
		asprintf(&string, "AT+CRSM=%d,%d,%d,%d,%d,%s",
			sim_io_request->command, sim_io_request->fileid,
			sim_io_request->p1, sim_io_request->p2, sim_io_request->p3, sim_io_request->data);
	} else {
		asprintf(&string, "AT+CRSM=%d,%d,%d,%d,%d",
			sim_io_request->command, sim_io_request->fileid,
			sim_io_request->p1, sim_io_request->p2, sim_io_request->p3);
	}

	if (string == NULL)
		return;

	rc = at_send_callback(string, token, at_crsm_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	free(string);
}

int at_clck_callback(char *string, int error, RIL_Token token)
{
	int response = 0;
	int rc;

	if (!at_strings_compare("+CLCK", string) && at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) != AT_ERROR_OK || string == NULL)
		goto error;

	rc = sscanf(string, "+CLCK: %d", &response);
	if (rc < 1)
		goto error;

	ril_request_complete(token, RIL_E_SUCCESS, &response, sizeof(response));

complete:
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_query_facility_lock(void *data, size_t length, RIL_Token token)
{
	char *facility = NULL;
	char *password = NULL;
	char *class = NULL;
	char *string = NULL;
	int rc;

	if (data == NULL || length < 3 * sizeof(sizeof(char *)))
		return;

	facility = ((char **) data)[0];
	password = ((char **) data)[1];
	class = ((char **) data)[2];

	if (facility == NULL || password == NULL || class == NULL)
		return;

	asprintf(&string, "AT+CLCK=\"%s\",2,\"%s\",%s", facility, password, class);
	if (string == NULL)
		return;

	rc = at_send_callback(string, token, at_clck_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	free(string);
}

int at_cimi_callback(char *string, int error, RIL_Token token)
{
	char *imsi;
	imsi = string;
	ALOGD("IMSI: %s", imsi);
	ril_request_complete(token, RIL_E_SUCCESS, imsi, sizeof(imsi));
	return AT_STATUS_HANDLED;
}

void ril_request_get_imsi(void *data, size_t length, RIL_Token token)
{
	at_send_callback("AT+CIMI", token, at_cimi_callback);
}
