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

#define LOG_TAG "RIL-PWR"
#include <utils/Log.h>
#include <telephony/ril.h>

#include <hayes-ril.h>

int at_cfun_enable_callback(char *string, int error, RIL_Token token)
{
	if (at_error(error) == AT_ERROR_OK) {
		ril_request_complete(token, RIL_E_SUCCESS, NULL, 0);

		ril_data->radio_state = RADIO_STATE_SIM_NOT_READY;
		ril_request_unsolicited(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
	} else {
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	return AT_STATUS_HANDLED;
}

int at_cfun_disable_callback(char *string, int error, RIL_Token token)
{
	if (at_error(error) == AT_ERROR_OK) {
		ril_request_complete(token, RIL_E_SUCCESS, NULL, 0);

		ril_data->radio_state = RADIO_STATE_OFF;
		ril_request_unsolicited(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
	} else {
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	return AT_STATUS_HANDLED;
}

void ril_request_radio_power(void *data, size_t length, RIL_Token token)
{
	struct ril_device *ril_device;
	int power_state;
	int rc;

	if (data == NULL || length < sizeof(int))
		return;

	power_state = *((int *) data);

	ril_device = ril_data->device;

	if (power_state > 0) {
		rc = at_send_callback("AT+CFUN=1", token, at_cfun_enable_callback);
		if (rc < 0)
			ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

		ril_device_at_power_on(ril_device);

		// Ask for PIN status
		at_send_callback("AT+CPIN?", RIL_TOKEN_UNSOL, at_cpin_callback);
	} else {
		rc = at_send_callback("AT+CFUN=0", token, at_cfun_disable_callback);
		if (rc < 0)
			ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

		ril_device_at_power_off(ril_device);
	}
}
