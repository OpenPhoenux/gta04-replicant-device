/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2012-2013 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
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

#include <string.h>
#include <ctype.h>

#define LOG_TAG "RIL"
#include <utils/Log.h>

#include <hayes-ril.h>

/*
 * List
 */

struct list_head *list_head_alloc(void *data, struct list_head *prev, struct list_head *next)
{
	struct list_head *list;

	list = calloc(1, sizeof(struct list_head));
	if (list == NULL)
		return NULL;

	list->data = data;
	list->prev = prev;
	list->next = next;

	if (prev != NULL)
		prev->next = list;
	if (next != NULL)
		next->prev = list;

	return list;
}

void list_head_free(struct list_head *list)
{
	if (list == NULL)
		return;

	if (list->next != NULL)
		list->next->prev = list->prev;
	if (list->prev != NULL)
		list->prev->next = list->next;

	memset(list, 0, sizeof(struct list_head));
	free(list);
}

int list_head_count(struct list_head *list)
{
	int count = 0;

	if (list == NULL)
		return count;

	while (list != NULL) {
		count++;
		list = list->next;
	}

	return count;
}

/*
 * Log
 */

void hex_dump(void *data, int size)
{
	/* dumps size bytes of *data to stdout. Looks like:
	 * [0000] 75 6E 6B 6E 6F 77 6E 20
	 *				  30 FF 00 00 00 00 39 00 unknown 0.....9.
	 * (in a single line of course)
	 */

	unsigned char *p = data;
	unsigned char c;
	int n;
	char bytestr[4] = {0};
	char addrstr[10] = {0};
	char hexstr[ 16*3 + 5] = {0};
	char charstr[16*1 + 5] = {0};

	for (n = 1 ; n <= size ; n++) {
		if (n%16 == 1) {
			/* store address for this line */
			snprintf(addrstr, sizeof(addrstr), "%.4x",
			   ((unsigned int)p-(unsigned int)data) );
		}

		c = *p;
		if (isalnum(c) == 0) {
			c = '.';
		}

		/* store hex str (for left side) */
		snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
		strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

		/* store char str (for right side) */
		snprintf(bytestr, sizeof(bytestr), "%c", c);
		strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

		if (n%16 == 0) {
			/* line completed */
			ALOGD("[%4.4s]   %-50.50s  %s", addrstr, hexstr, charstr);
			hexstr[0] = 0;
			charstr[0] = 0;
		} else if (n%8 == 0) {
			/* half line: add whitespaces */
			strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
			strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
		}
		p++; /* next byte */
	}

	if (strlen(hexstr) > 0) {
		/* print rest of buffer if not empty */
		ALOGD("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
	}
}

void ril_data_log(char *data, int length)
{
	RIL_LOG_LOCK();
	ALOGD("\n");
	ALOGD("%s: ==== DATA DUMP: %d BYTES ====", ril_data->device->tag, length);
	//hex_dump(data, length);
	RIL_LOG_UNLOCK();
}

void ril_recv_log(char *string, int error)
{
	RIL_LOG_LOCK();
	if (error != AT_ERROR_UNDEF)
		ALOGD("%s: AT RECV [%s] (error is %s, %d)", ril_data->device->tag,
			string, at_error_string(at_error(error)),
			at_cme_error(error));
	else
		ALOGD("%s: AT RECV [%s]", ril_data->device->tag, string);
	RIL_LOG_UNLOCK();
}

void ril_send_log(char *string)
{
	RIL_LOG_LOCK();
	ALOGD("%s: AT SEND [%s]", ril_data->device->tag, string);
	RIL_LOG_UNLOCK();
}
