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
/*
int ril_gprs_connection_register(int cid)
{
	struct ril_gprs_connection *gprs_connection;
	struct list_head *list_end;
	struct list_head *list;

	gprs_connection = calloc(1, sizeof(struct ril_gprs_connection));
	if (gprs_connection == NULL)
		return -1;

	gprs_connection->cid = cid;

	list_end = ril_data.gprs_connections;
	while (list_end != NULL && list_end->next != NULL)
		list_end = list_end->next;

	list = list_head_alloc((void *) gprs_connection, list_end, NULL);

	if (ril_data.gprs_connections == NULL)
		ril_data.gprs_connections = list;

	return 0;
}

void ril_gprs_connection_unregister(struct ril_gprs_connection *gprs_connection)
{
	struct list_head *list;

	if (gprs_connection == NULL)
		return;

	list = ril_data.gprs_connections;
	while (list != NULL) {
		if (list->data == (void *) gprs_connection) {
			memset(gprs_connection, 0, sizeof(struct ril_gprs_connection));
			free(gprs_connection);

			if (list == ril_data.gprs_connections)
				ril_data.gprs_connections = list->next;

			list_head_free(list);

			break;
		}
list_continue:
		list = list->next;
	}
}

struct ril_gprs_connection *ril_gprs_connection_find_cid(int cid)
{
	struct ril_gprs_connection *gprs_connection;
	struct list_head *list;

	list = ril_data.gprs_connections;
	while (list != NULL) {
		gprs_connection = (struct ril_gprs_connection *) list->data;
		if (gprs_connection == NULL)
			goto list_continue;

		if (gprs_connection->cid == cid)
			return gprs_connection;

list_continue:
		list = list->next;
	}

	return NULL;
}

struct ril_gprs_connection *ril_gprs_connection_find_token(RIL_Token t)
{
	struct ril_gprs_connection *gprs_connection;
	struct list_head *list;

	list = ril_data.gprs_connections;
	while (list != NULL) {
		gprs_connection = (struct ril_gprs_connection *) list->data;
		if (gprs_connection == NULL)
			goto list_continue;

		if (gprs_connection->token == t)
			return gprs_connection;

list_continue:
		list = list->next;
	}

	return NULL;
}

struct ril_gprs_connection *ril_gprs_connection_start(void)
{
	//struct ipc_client_gprs_capabilities gprs_capabilities;
	struct ril_gprs_connection *gprs_connection;
	//struct ipc_client *ipc_client;
	struct list_head *list;
	int cid, cid_max;
	int rc;
	int i;

	//if (ril_data.ipc_fmt_client == NULL || ril_data.ipc_fmt_client->data == NULL)
	//	return NULL;

	//ipc_client = (struct ipc_client *) ril_data.ipc_fmt_client->data;

	//ipc_client_gprs_get_capabilities(ipc_client, &gprs_capabilities);
	//cid_max = gprs_capabilities.cid_max;

	for (i = 0 ; i < cid_max ; i++) {
		cid = i + 1;
		list = ril_data.gprs_connections;
		while (list != NULL) {
			if (list->data == NULL)
				goto list_continue;

			gprs_connection = (struct ril_gprs_connection *) list->data;
			if (gprs_connection->cid == cid) {
				cid = 0;
				break;
			}

list_continue:
			list = list->next;
		}

		if (cid > 0)
			break;
	}

	if (cid <= 0) {
		RIL_LOGE("Unable to find an unused cid, aborting");
		return NULL;
	}

	ALOGD("Using GPRS connection cid: %d", cid);
	rc = ril_gprs_connection_register(cid);
	if (rc < 0)
		return NULL;

	gprs_connection = ril_gprs_connection_find_cid(cid);
	return gprs_connection;
}

void ril_gprs_connection_stop(struct ril_gprs_connection *gprs_connection)
{
	if (gprs_connection == NULL)
		return;

	if (gprs_connection->interface != NULL)
		free(gprs_connection->interface);

	ril_gprs_connection_unregister(gprs_connection);
}*/

int at_cgdcont_callback(char *string, int error, RIL_Token token)
{
	if (at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) != AT_ERROR_OK)
		goto error;

	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return AT_STATUS_HANDLED;
}

void ril_request_setup_data_call(void *data, size_t length, RIL_Token token)
{
	struct ril_gprs_connection *gprs_connection = NULL;

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

	ALOGD("Requesting data connection to APN '%s'\n", apn);

	asprintf(&string, "AT+CGDCONT=1,\"IP\",\"%s\"", apn); //IPv4 context #1
	rc = at_send_callback(string, token, at_cgdcont_callback);

/*
	gprs_connection = ril_gprs_connection_start();

	if (!gprs_connection) {
		ALOGE("Unable to create GPRS connection, aborting");

		ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	gprs_connection->token = t;

	// Create the structs with the apn
	ipc_gprs_define_pdp_context_setup(&(gprs_connection->define_context),
		gprs_connection->cid, 1, apn);

	// Create the structs with the username/password tuple
	ipc_gprs_pdp_context_setup(&(gprs_connection->context),
		gprs_connection->cid, 1, username, password);

	ipc_client_gprs_get_capabilities(ipc_client, &gprs_capabilities);

	// If the device has the capability, deal with port list
	if (gprs_capabilities.port_list) {
		ipc_gprs_port_list_setup(&port_list);

		ipc_gen_phone_res_expect_to_func(ril_request_get_id(t), IPC_GPRS_PORT_LIST,
			ipc_gprs_port_list_complete);

		ipc_fmt_send(IPC_GPRS_PORT_LIST, IPC_TYPE_SET,
			(void *) &port_list, sizeof(struct ipc_gprs_port_list), ril_request_get_id(t));
	} else {
		ipc_gen_phone_res_expect_to_func(ril_request_get_id(t), IPC_GPRS_DEFINE_PDP_CONTEXT,
			ipc_gprs_define_pdp_context_complete);

		ipc_fmt_send(IPC_GPRS_DEFINE_PDP_CONTEXT, IPC_TYPE_SET,
			(void *) &(gprs_connection->define_context),
				sizeof(struct ipc_gprs_define_pdp_context), ril_request_get_id(t));
	}
*/

	return;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/*
void ril_request_deactivate_data_call(RIL_Token t, void *data, int length)
{
	struct ril_gprs_connection *gprs_connection;
	struct ipc_gprs_pdp_context_set context;

	char *cid;
	int rc;

	if (data == NULL || length < (int) sizeof(char *))
		goto error;

	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	cid = ((char **) data)[0];

	gprs_connection = ril_gprs_connection_find_cid(atoi(cid));

	if (!gprs_connection) {
		RIL_LOGE("Unable to find GPRS connection, aborting");

		ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	gprs_connection->token = t;

	ipc_gprs_pdp_context_setup(&context, gprs_connection->cid, 0, NULL, NULL);

	ipc_gen_phone_res_expect_to_func(ril_request_get_id(t), IPC_GPRS_PDP_CONTEXT,
		ipc_gprs_pdp_context_disable_complete);

	ipc_fmt_send(IPC_GPRS_PDP_CONTEXT, IPC_TYPE_SET,
		(void *) &context, sizeof(struct ipc_gprs_pdp_context_set), ril_request_get_id(t));

	return;

error:
	ril_request_complete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

#if RIL_VERSION >= 6
void ril_data_call_response_free(RIL_Data_Call_Response_v6 *response)
#else
void ril_data_call_response_free(RIL_Data_Call_Response *response)
#endif
{
	if (response == NULL)
		return;

	if (response->type != NULL)
		free(response->type);

#if RIL_VERSION >= 6
	if (response->addresses)
		free(response->addresses);
	if (response->ifname)
		free(response->ifname);
	if (response->dnses)
		free(response->dnses);
	if (response->gateways)
		free(response->gateways);
#else
	if (response->apn)
		free(response->apn);
	if (response->address)
		free(response->address);
#endif
}

void ril_request_last_data_call_fail_cause(RIL_Token t)
{
	struct ril_gprs_connection *gprs_connection;
	int last_failed_cid;
	int fail_cause;

	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	last_failed_cid = ril_data.state.gprs_last_failed_cid;

	if (!last_failed_cid) {
		RIL_LOGE("No GPRS connection was reported to have failed");

		goto fail_cause_unspecified;
	}

	gprs_connection = ril_gprs_connection_find_cid(last_failed_cid);

	if (!gprs_connection) {
		RIL_LOGE("Unable to find GPRS connection");

		goto fail_cause_unspecified;
	}

	fail_cause = gprs_connection->fail_cause;

	RIL_LOGD("Destroying GPRS connection with cid: %d", gprs_connection->cid);
	ril_gprs_connection_stop(gprs_connection);

	goto fail_cause_return;

fail_cause_unspecified:
	fail_cause = PDP_FAIL_ERROR_UNSPECIFIED;

fail_cause_return:
	ril_data.state.gprs_last_failed_cid = 0;

	ril_request_complete(t, RIL_E_SUCCESS, &fail_cause, sizeof(fail_cause));
}

void ril_unsol_data_call_list_changed(void)
{
	ipc_fmt_send_get(IPC_GPRS_PDP_CONTEXT, 0xff);
}

void ril_request_data_call_list(RIL_Token t)
{
	if (ril_radio_state_complete(RADIO_STATE_OFF, t))
		return;

	ipc_fmt_send_get(IPC_GPRS_PDP_CONTEXT, ril_request_get_id(t));
}*/
