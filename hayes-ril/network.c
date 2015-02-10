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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
	//TODO: from GTA04 documentation: signal strenght = 2*n-113dBm, n = values[0]
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
 * Network state / technology
 */
int at_owcti_callback(char *string, int error, RIL_Token token)
{
	int rc;
	int cell_type;

	rc = sscanf(string, "_OWCTI: %d", &cell_type);

	switch(cell_type) {
		case 0: // Non-WCDMA, check GSM
		at_send_callback("AT_OCTI?", token, at_octi_callback); //TODO: FIXME: GTA04 only
		break;
		case 1: // WCDMA only
		ril_data->radio_technology = RADIO_TECH_UMTS;
		ALOGD("GTA04 radio_tech: UMTS");
		break;
		case 2: // WCDMA + HSDPA
		ril_data->radio_technology = RADIO_TECH_HSDPA;
		ALOGD("GTA04 radio_tech: HSDPA");
		break;
		case 3: // WCDMA + HSUPA
		ril_data->radio_technology = RADIO_TECH_HSUPA;
		ALOGD("GTA04 radio_tech: HSUPA");
		break;
		case 4: // WCDMA + HSDPA + HSUPA
		ril_data->radio_technology = RADIO_TECH_HSPA;
		ALOGD("GTA04 radio_tech: HSPA");
		break;
		default: // ERROR?
		ril_data->radio_technology = RADIO_TECH_UNKNOWN;
		ALOGD("GTA04 radio_tech: UNKNOWN");
		break;
	}

	return AT_STATUS_HANDLED;
}

int at_octi_callback(char *string, int error, RIL_Token token)
{
	int rc;
	int mode, cell_type;

	rc = sscanf(string, "_OCTI: %d,%d", &mode, &cell_type);
	ALOGD("GTA04: radio_tech: _OCTI: %d", cell_type);

	switch(cell_type) {
		case 0: // Unknown
		ril_data->radio_technology = RADIO_TECH_UNKNOWN;
		ALOGD("GTA04 radio_tech: UNKNOWN");
		break;
		case 1: // GSM
		ril_data->radio_technology = RADIO_TECH_GSM;
		ALOGD("GTA04 radio_tech: GSM");
		break;
		case 2: // GPRS
		ril_data->radio_technology = RADIO_TECH_GPRS;
		ALOGD("GTA04 radio_tech: GPRS");
		break;
		case 3: // EDGE
		ril_data->radio_technology = RADIO_TECH_EDGE;
		ALOGD("GTA04 radio_tech: EDGE");
		break;
		default: // ERROR?
		ril_data->radio_technology = RADIO_TECH_UNKNOWN;
		ALOGD("GTA04 radio_tech: UNKNOWN");
		break;
	}

	return AT_STATUS_HANDLED;
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

	at_send_callback("AT+CSQ", RIL_TOKEN_NULL, at_generic_callback); // update signal strength
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

	if(state == 1 || state == 5) //1=registered, home; 5=registered, roaming
		ril_device_sim_ready_setup();

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

	at_send_callback("AT+CSQ", RIL_TOKEN_NULL, at_generic_callback); // update signal strength
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

	if(state == 1 || state == 5) //1=registered, home; 5=registered, roaming
		ril_device_sim_ready_setup();

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

		//TODO: this conflicts with ril_request_data_registration_state, which accesses ril_data, too.
		//TODO: is it needed?
		//for (i = 0 ; i < 3 ; i++)
		//	if (ril_data->registration_state[i] != NULL)
		//		free(ril_data->registration_state[i]);

		//memset(&ril_data->registration_state, 0, sizeof(ril_data->registration_state));
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

/*
 * Operator List
 */

int at_cops_list_callback(char *string, int error, RIL_Token token)
{
	char *status[] = {"unknown","available","current","forbidden"};
	char delimiter[] = "()";
	char *ptr;
	int rc,i;
	char longname[50];
	char shortname[50];
	char numeric[50]; //mccmnc
	int st,act;
	char *resp_buf[80] = {0}; //80=4*20
	int counter = 0;

	if (at_error(error) != AT_ERROR_OK || string == NULL)
		goto error;

	ptr = strtok(string, delimiter);
	if(strncmp(ptr, "+COPS: ", 7) != 0)
		goto error;

	while(ptr != NULL) {
		rc = sscanf(ptr, "%d,\"%50[^\"]\",\"%50[^\"]\",\"%50[^\"]\",%d", &st, longname, shortname, numeric, &act);
		if (rc==5) {
			asprintf(&resp_buf[counter++], "%s", longname);
			asprintf(&resp_buf[counter++], "%s", shortname);
			asprintf(&resp_buf[counter++], "%s", numeric);
			asprintf(&resp_buf[counter++], "%s", status[st]);
		}
		ptr = strtok(NULL, delimiter);
	}

	//for(i=0; i<counter; i++) {
	//	ALOGD("%d: %s\n", i, resp_buf[i]);
	//}
	//ALOGD("%s", string);

complete:
	ril_request_complete(token, RIL_E_SUCCESS, &resp_buf, counter*sizeof(char*));
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

/*
 * Operator Names
 */

int at_copn_callback(char *string, int error, RIL_Token token)
{
	ALOGD("%s", string); //TODO: match operator name with number given in +cops
	return AT_STATUS_HANDLED;
}

/*
 * Neighboring Cells
 */

int at_onci_callback(char *string, int error, RIL_Token token)
{
	//TODO: implement 2G and 3G variant:
	/* compare to GTM6xx_Nyos_AT_Command_v046ext.pdf
	D/RIL-NET ( 1026): _ONCI: 2G
	D/RIL-NET ( 1026): _ONCI: "C75F", "C837"
	D/RIL-NET ( 1026): _ONCI: "26207"
	D/RIL-NET ( 1026): _ONCI: 1
	D/RIL-NET ( 1026): _ONCI: (639,7,2,36)
	D/RIL-NET ( 1026): _ONCI: 2,1,(665,7,4,30),(698,7,0,),(701,7,3,27)
	D/RIL-NET ( 1026): _ONCI: 2,2,(710,7,2,21)
	*/

	ALOGD("=== AT_ONCI ==================================================");
	ALOGD("%s", string);
	ALOGD("==============================================================");

error:
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

int at_opsys_callback(char *string, int error, RIL_Token token)
{
	int mode;
	int domain;
	int response[1];

	if(string == NULL)
		goto error;

	sscanf(string, "_OPSYS: %d,%d", &mode, &domain);
	switch(mode) {
		case 0:
			response[0] = PREF_NET_TYPE_GSM_ONLY;
		break;
		case 1:
			response[0] = PREF_NET_TYPE_WCDMA;
		break;
		case 3:
		default:
			response[0] = PREF_NET_TYPE_GSM_WCDMA;
		break;
	}

	ril_request_complete(token, RIL_E_SUCCESS, &response, sizeof(response));
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_get_preferred_network_type(void *data, size_t length, RIL_Token token)
{
	at_send_callback("AT_OPSYS?", token, at_opsys_callback); //TODO: FIXME: GTA04 only
}

void ril_request_set_preferred_network_type(void *data, size_t length, RIL_Token token)
{
	/* "data" is int * which is RIL_PreferredNetworkType */
	switch(*(int*)data) {
		case PREF_NET_TYPE_GSM_ONLY:
			at_send_callback("AT_OPSYS=0,2", token, at_generic_callback); //TODO: FIXME: GTA04 only
		break;
		case PREF_NET_TYPE_WCDMA:
			at_send_callback("AT_OPSYS=1,2", token, at_generic_callback); //TODO: FIXME: GTA04 only
		break;
		case PREF_NET_TYPE_GSM_WCDMA:
		default:
			at_send_callback("AT_OPSYS=3,2", token, at_generic_callback); //TODO: FIXME: GTA04 only
		break;
	}
}

void ril_request_set_network_selection_automatic(void *data, size_t length, RIL_Token token)
{
	at_send_callback("AT+COPS=0", token, at_generic_callback);
}

void ril_request_set_network_selection_manual(void *data, size_t length, RIL_Token token)
{
	char *str;
	//maybe better use AT+COPS=4,2,%s in order to fall back to automatic selection?
	asprintf(&str, "AT+COPS=1,2,%s", (const char *)data);
	at_send_callback(str, token, at_generic_callback);
	free(str);
}

void ril_request_data_registration_state(void *data, size_t length, RIL_Token token)
{
/*
 * "response" is a "char **"
 * ((const char **)response)[0] is registration state 0-5 from TS 27.007 10.1.20 AT+CGREG
 * ((const char **)response)[1] is LAC if registered or NULL if not
 * ((const char **)response)[2] is CID if registered or NULL if not
 * ((const char **)response)[3] indicates the available data radio technology,
 *                              valid values as defined by RIL_RadioTechnology.
 * ((const char **)response)[4] if registration state is 3 (Registration
 *                               denied) this is an enumerated reason why
 *                               registration was denied.  See 3GPP TS 24.008,
 *                               Annex G.6 "Additonal cause codes for GMM".
 *      7 == GPRS services not allowed
 *      8 == GPRS services and non-GPRS services not allowed
 *      9 == MS identity cannot be derived by the network
 *      10 == Implicitly detached
 *      14 == GPRS services not allowed in this PLMN
 *      16 == MSC temporarily not reachable
 *      40 == No PDP context activated
 * ((const char **)response)[5] The maximum number of simultaneous Data Calls that can be
 *                              established using RIL_REQUEST_SETUP_DATA_CALL.
 *
 * The values at offsets 6..10 are optional LTE location information in decimal.
 * If a value is unknown that value may be NULL. If all values are NULL,
 * none need to be present.
 *  ((const char **)response)[6] is TAC, a 16-bit Tracking Area Code.
 *  ((const char **)response)[7] is CID, a 0-503 Physical Cell Identifier.
 *  ((const char **)response)[8] is ECI, a 28-bit E-UTRAN Cell Identifier.
 *  ((const char **)response)[9] is CSGID, a 27-bit Closed Subscriber Group Identity.
 *  ((const char **)response)[10] is TADV, a 6-bit timing advance value.
 *
 * LAC and CID are in hexadecimal format.
 * valid LAC are 0x0000 - 0xffff
 * valid CID are 0x00000000 - 0x0fffffff
 *
 * Please note that registration state 4 ("unknown") is treated
 * as "out of service" in the Android telephony system
 */
	at_send_callback("AT_OWCTI?", RIL_TOKEN_NULL, at_owcti_callback); //TODO: FIXME: GTA04 only
	//wait a little, for OWCTI/OCTI to update ril_data
	sleep(1); //FIXME: find proper way to do this

	char *response[6];

	if(ril_data->registration_state[0] != NULL)
		response[0] = ril_data->registration_state[0];
	else
		response[0] = "4"; //unknown

	response[1] = ril_data->registration_state[1]; //LAC or NULL if not registered
	response[2] = ril_data->registration_state[2]; //CID or NULL if not registered

	//ALOGD("================================================================");
	//ALOGD("State: %s, LAC: %s, CID: %s", state, lac, cid);
	//ALOGD("================================================================");

	asprintf(&response[3], "%d", ril_data->radio_technology);
	if(atoi(response[0]) != 3)
		response[4] = NULL;
	else
		response[4] = "40"; //TODO: this is a dummy
	response[5] = "1";  //limit to one pdp context for now

	ril_request_complete(token, RIL_E_SUCCESS, &response, sizeof(response));
}

void ril_request_query_network_selection_mode(void *data, size_t length, RIL_Token token)
{
/**
 * RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE
 *
 * Query current network selectin mode
 *
 * "data" is NULL
 *
 * "response" is int *
 * ((const int *)response)[0] is
 *     0 for automatic selection
 *     1 for manual selection
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
	int *response[1] = {
		0, //automatic selection = 0 //TODO: this is a dummy
		};
	ril_request_complete(token, RIL_E_SUCCESS, &response, sizeof(response));
}

void ril_request_query_available_networks(void *data, size_t length, RIL_Token token)
{
/**
 * RIL_REQUEST_QUERY_AVAILABLE_NETWORKS
 *
 * Scans for available networks
 *
 * "data" is NULL
 * "response" is const char ** that should be an array of n*4 strings, where
 *    n is the number of available networks
 * For each available network:
 *
 * ((const char **)response)[n+0] is long alpha ONS or EONS
 * ((const char **)response)[n+1] is short alpha ONS or EONS
 * ((const char **)response)[n+2] is 5 or 6 digit numeric code (MCC + MNC)
 * ((const char **)response)[n+3] is a string value of the status:
 *           "unknown"
 *           "available"
 *           "current"
 *           "forbidden"
 *
 * This request must not respond until the new operator is selected
 * and registered
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
	int rc;
	rc = at_send_callback("AT+COPS=?", token, at_cops_list_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_get_neighboring_cell_ids(void *data, size_t length, RIL_Token token)
{
/**
 * RIL_REQUEST_NEIGHBORING_CELL_IDS
 *
 * Request neighboring cell id in GSM network
 *
 * "data" is NULL
 * "response" must be a " const RIL_NeighboringCell** "
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
	int rc;
	rc = at_send_callback("AT_ONCI?", token, at_onci_callback);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}
