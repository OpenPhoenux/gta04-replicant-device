/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2013 Paul Kocialkowski <contact@paulk.fr>
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

#define LOG_TAG "RIL-SMS"
#include <utils/Log.h>
#include <telephony/ril.h>

#include <hayes-ril.h>

struct ril_outgoing_sms {
	RIL_Token token;
	int waiting;
	char *smsc;
	char *pdu;
};

int ril_outgoing_sms_register(RIL_Token token, char *smsc, char *pdu)
{
	struct ril_outgoing_sms *outgoing_sms;
	struct list_head *list_end;
	struct list_head *list;

	outgoing_sms = calloc(1, sizeof(struct ril_outgoing_sms));
	if (outgoing_sms == NULL)
		return -1;

	RIL_DATA_LOCK();

	outgoing_sms->token = token;
	outgoing_sms->smsc = smsc == NULL ? NULL : strdup(smsc);
	outgoing_sms->pdu = pdu == NULL ? NULL : strdup(pdu);

	outgoing_sms->waiting = 1;

	list_end = ril_data->outgoing_sms;
	while (list_end != NULL && list_end->next != NULL)
		list_end = list_end->next;

	list = list_head_alloc((void *) outgoing_sms, list_end, NULL);

	if (ril_data->outgoing_sms == NULL)
		ril_data->outgoing_sms = list;

	RIL_DATA_UNLOCK();

	return 0;
}

void ril_outgoing_sms_unregister(struct ril_outgoing_sms *outgoing_sms)
{
	struct list_head *list;

	if (outgoing_sms == NULL)
		return;

	RIL_DATA_LOCK();

	list = ril_data->outgoing_sms;
	while (list != NULL) {
		if (list->data == (void *) outgoing_sms) {
			memset(outgoing_sms, 0, sizeof(struct ril_outgoing_sms));

			if (outgoing_sms->smsc != NULL)
				free(outgoing_sms->smsc);
			if (outgoing_sms->pdu != NULL)
				free(outgoing_sms->pdu);

			free(outgoing_sms);

			if (list == ril_data->outgoing_sms)
				ril_data->outgoing_sms = list->next;

			list_head_free(list);

			break;
		}
list_continue:
		list = list->next;
	}

	RIL_DATA_UNLOCK();
}

struct ril_outgoing_sms *ril_outgoing_sms_find_waiting(int waiting)
{
	struct ril_outgoing_sms *outgoing_sms;
	struct list_head *list;

	RIL_DATA_LOCK();

	list = ril_data->outgoing_sms;
	while (list != NULL) {
		outgoing_sms = (struct ril_outgoing_sms *) list->data;
		if (outgoing_sms == NULL)
			goto list_continue;

		if (outgoing_sms->waiting == waiting) {
			RIL_DATA_UNLOCK();
			return outgoing_sms;
		}

list_continue:
		list = list->next;
	}

	RIL_DATA_UNLOCK();

	return NULL;
}

struct ril_outgoing_sms *ril_outgoing_sms_find_token(RIL_Token token)
{
	struct ril_outgoing_sms *outgoing_sms;
	struct list_head *list;

	RIL_DATA_LOCK();

	list = ril_data->outgoing_sms;
	while (list != NULL) {
		outgoing_sms = (struct ril_outgoing_sms *) list->data;
		if (outgoing_sms == NULL)
			goto list_continue;

		if (outgoing_sms->token == token) {
			RIL_DATA_UNLOCK();
			return outgoing_sms;
		}

list_continue:
		list = list->next;
	}

	RIL_DATA_UNLOCK();

	return NULL;
}

int at_cmgs_callback(char *string, int error, RIL_Token token)
{
	RIL_SMS_Response response;
	char *buffer = NULL;
	int id;
	int rc;

	struct ril_outgoing_sms *outgoing_sms = NULL;

	// We might have an echo of the SMS data
	if (at_error(error) == AT_ERROR_OK) {
		while (string != NULL) {
			if (at_strings_compare("+CMGS", string))
				break;

			string = strchr(string, '+');
		}

		if (string == NULL)
			return AT_STATUS_UNHANDLED;
	}

	if ((at_error(error) != AT_ERROR_OK || string == NULL) && at_error(error) != AT_ERROR_OK_EXPECT_DATA)
		goto error;

	outgoing_sms = ril_outgoing_sms_find_token(token);
	if (outgoing_sms == NULL)
		goto error;

	if (at_error(error) == AT_ERROR_OK_EXPECT_DATA) {
		asprintf(&buffer, "%s%s", outgoing_sms->smsc, outgoing_sms->pdu);
		if (buffer == NULL)
			goto error;

		rc = at_send_request_data(token, buffer, strlen(buffer));

		free(buffer);

		return AT_STATUS_HANDLED;
	} else {
		rc = sscanf(string, "+CMGS: %d", &id);
		if (rc < 1)
			goto error;

		memset(&response, 0, sizeof(response));
		response.ackPDU = NULL;
		response.messageRef = id;

		ril_request_complete(token, RIL_E_SUCCESS, &response, sizeof(response));
	}

complete:
	if (outgoing_sms != NULL)
		ril_outgoing_sms_unregister(outgoing_sms);
	ril_outgoing_sms_send_next();

	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	if (outgoing_sms != NULL)
		ril_outgoing_sms_unregister(outgoing_sms);
	ril_outgoing_sms_send_next();

	return AT_STATUS_HANDLED;
}

int ril_outgoing_sms_send(struct ril_outgoing_sms *outgoing_sms)
{
	char *string = NULL;
	int length;
	int rc;

	if (outgoing_sms == NULL)
		return -1;

	if (outgoing_sms->pdu == NULL || outgoing_sms->smsc == NULL)
		goto error;

	length = strlen(outgoing_sms->pdu) / 2;
	asprintf(&string, "AT+CMGS=%d", length);
	if (string == NULL)
		goto error;

	rc = at_send_callback(string, outgoing_sms->token, at_cmgs_callback);
	if (rc < 0)
		goto error;

	free(string);

	return 0;

error:
	if (string != NULL)
		free(string);

	ril_request_complete(outgoing_sms->token, RIL_E_GENERIC_FAILURE, NULL, 0);

	ril_outgoing_sms_unregister(outgoing_sms);
	ril_outgoing_sms_send_next();

	return -1;
}

int at_csca_callback(char *string, int error, RIL_Token token)
{
	struct ril_outgoing_sms *outgoing_sms = NULL;
	char smsc_number[30] = { 0 };
	char smsc[30] = { 0 };
	char *p;
	int tosca = 0;

	int length;
	int o, i;
	int rc;

	if (!at_strings_compare("+CSCA", string) && at_error(error) == AT_ERROR_OK)
		return AT_STATUS_UNHANDLED;

	if (at_error(error) != AT_ERROR_OK || string == NULL)
		goto error;

	outgoing_sms = ril_outgoing_sms_find_token(token);
	if (outgoing_sms == NULL)
		goto error;

	rc = sscanf(string, "+CSCA: \"%30[^\"]\",%d", (char *) &smsc_number, &tosca);
	if (rc < 2)
		goto error;

	p = (char *) &smsc_number;

	if (*p == '+')
		p++;

	length = strlen(p);
	sprintf((char *) &smsc, "%.2x%.2x", (length + 1) / 2 + 1, tosca);

	// FIXME: What is going on here?

	for (i = 0, o = 0 ; o < length - 1 ; i += 2) {
		smsc[5+i] = p[o++];
		smsc[4+i] = p[o++];
	}

	if (length % 2) {
		smsc[4+length] = p[o];
		smsc[3+length] = 'F';
		smsc[5+length] = '\0';
	} else {
		smsc[4+length] = '\0';
	}

	outgoing_sms->smsc = strdup(smsc);

	ril_outgoing_sms_send(outgoing_sms);

complete:
	return AT_STATUS_HANDLED;

error:
	ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	if (outgoing_sms != NULL)
		ril_outgoing_sms_unregister(outgoing_sms);
	ril_outgoing_sms_send_next();

	return AT_STATUS_HANDLED;
}

int ril_outgoing_sms_send_next(void)
{
	struct ril_outgoing_sms *outgoing_sms;
	int rc;

	outgoing_sms = ril_outgoing_sms_find_waiting(0);
	if (outgoing_sms != NULL) {
		ALOGD("Another SMS is being sent!");
		return 0;
	}

	outgoing_sms = ril_outgoing_sms_find_waiting(1);
	if (outgoing_sms == NULL) {
		ALOGD("No more SMS to send!");
		return 0;
	}

	outgoing_sms->waiting = 0;

	if (outgoing_sms->smsc == NULL) {
		rc = at_send_callback("AT+CSCA?", outgoing_sms->token, at_csca_callback);
		if (rc < 0)
			ril_request_complete(outgoing_sms->token, RIL_E_GENERIC_FAILURE, NULL, 0);

		return 0;
	}

	ril_outgoing_sms_send(outgoing_sms);

	return 0;
}

void ril_request_send_sms(void *data, size_t length, RIL_Token token)
{
	char *smsc, *pdu;
	int rc;

	if (length < 2 * sizeof(char *) || data == NULL)
		return;

	smsc = ((char **) data)[0];
	pdu = ((char **) data)[1];

	if (pdu == NULL)
		return;

	rc = ril_outgoing_sms_register(token, smsc, pdu);
	if (rc < 0)
		ril_request_complete(token, RIL_E_GENERIC_FAILURE, NULL, 0);

	ril_outgoing_sms_send_next();
}
