/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2014 Golden Delicious Computers,
 *                    Lukas Märdian <lukas@goldelico.com>
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

#define LOG_TAG "RIL-GPRS"
#include <utils/Log.h>
#include <telephony/ril.h>

#include <hayes-ril.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <netutils/ifc.h>
#include <arpa/inet.h>

int at_owandata_callback(char *string, int error, RIL_Token token)
{
//string: _OWANDATA: 1, 10.156.71.105, 0.0.0.0, 193.189.244.206, 193.189.244.225, 0.0.0.0, 0.0.0.0,144000
//status, ip, gw, dns1, dns2, nbns1, nbns2, speed

	RIL_Data_Call_Response_v6 response;
	int status = 0; //FIXME: RIL_DataCallFailCause
#ifndef HCRADIO
	int suggestedRetryTime = 3;
#endif
	int cid = 1; //FIXME: static
	int active;
	char type[] = "IP";
	char ifname[] = "hso0"; //FIXME: GTA04 specific
	char address[20];
	char gateway[20];
	char dns1[20];
	char dns2[20];
	char nbns1[20];
	char nbns2[20];
	char speed[20];
	int rc;
	char buf[50];

	if(string==NULL)
		goto error;

	rc = sscanf(string, "_OWANDATA: %d, %20[^,], %20[^,], %20[^,], %20[^,], %20[^,], %20[^,],%20[^,]", &active, address, gateway, dns1, dns2, nbns1, nbns2, speed);
	if(rc!=8)
		goto error;
	//ALOGD("================================================================");
	//ALOGD("%s", string);
	//ALOGD("rc = %d", rc);
	//ALOGD("%d %s %s %s %s %s %s %s", active, address, gateway, dns1, dns2, nbns1, nbns2, speed);
	//ALOGD("================================================================");

	response.status = status;
#ifndef HCRADIO
	response.suggestedRetryTime = suggestedRetryTime;
#endif
	response.cid = cid;
	response.active = active?1:0;
	response.type = type;
	response.ifname = ifname;
	response.addresses = address;
	asprintf(&response.dnses, "%s %s", dns1, dns2);
	response.gateways = gateway;

	rc = ifc_configure(ifname, inet_addr(address), 32, inet_addr(gateway), inet_addr(dns1), inet_addr(dns2));
	ALOGD("ifc_configure: rc: %d", rc);
	if(rc<0)
		goto error;

	ril_request_complete(token, RIL_E_SUCCESS, &response, sizeof(response));
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_setup_data_call(void *data, size_t length, RIL_Token token)
{
	char *username = NULL;
	char *password = NULL;
	char *apn = NULL;
	char *string = NULL;
	int rc;

	if (data == NULL || length < (int) (4 * sizeof(char *)))
		goto error;

	apn = ((char **) data)[2]; //e.g.: internet.t-mobile
	username = ((char **) data)[3]; //e.g.: t-mobile
	password = ((char **) data)[4]; //e.g.: tm

	//ALOGD("================================================================");
	//ALOGD("Requesting data connection to APN '%s'\n", apn);
	//ALOGD("================================================================");

	asprintf(&string, "AT+CGDCONT=1,\"IP\",\"%s\"", apn); //IPv4 context #1
	at_send_callback(string, RIL_TOKEN_NULL, at_generic_callback);
	at_send_callback("AT_OWANCALL=1,1,1", RIL_TOKEN_NULL, at_generic_callback); //FIXME: GTA04/gtm601 specific, _OWANCALL=1,0,1 to disconnect
	rc = at_send_callback("AT_OWANDATA=1", token, at_owandata_callback);
	if (rc < 0)
error:		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void ril_request_deactivate_data_call(void *data, size_t length, RIL_Token token)
{
	int rc;
	rc = ifc_reset_connections("hso0", RESET_IPV4_ADDRESSES); //FIXME: gta04 specific interface
	rc = ifc_disable("hso0"); //FIXME: gta04 specific interface
	at_send_callback("AT_OWANCALL=1,0,1", token, at_generic_callback);
}
