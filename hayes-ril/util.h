/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2012-2013 Paul Kocialkowski <contact@paulk.fr>
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

#ifndef _HAYES_RIL_UTIL_H_
#define _HAYES_RIL_UTIL_H_

// List

struct list_head {
	struct list_head *prev;
	struct list_head *next;
	void *data;
};

struct list_head *list_head_alloc(void *data, struct list_head *prev, struct list_head *next);
void list_head_free(struct list_head *list);
int list_head_count(struct list_head *list);

// Debug

int debug_lsusb(void);

// Log

void hex_dump(void *data, int size);
void ril_data_log(char *data, int length);
void ril_recv_log(char *string, int error);
void ril_send_log(char *string);

#endif
