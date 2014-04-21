/*
 * Copyright (C) 2014 Paul Kocialkowski <contact@paulk.fr>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/input.h>

#define LOG_TAG "vibrator"
#include <cutils/log.h>

/*
 * Globals
 */

const char rumble_input[] = "twl4030:vibrator";

int rumble_fd = -1;
int rumble_id = -1;

/*
 * Input
 */

void input_event_set(struct input_event *event, int type, int code, int value)
{
	if (event == NULL)
		return;

	memset(event, 0, sizeof(struct input_event));

	event->type = type,
	event->code = code;
	event->value = value;

	gettimeofday(&event->time, NULL);
}

int input_open(const char *name)
{
	DIR *d;
	struct dirent *di;

	char input_name[80] = { 0 };
	char path[PATH_MAX];
	char *c;
	int fd;
	int rc;

	if (name == NULL)
		return -EINVAL;

	d = opendir("/dev/input");
	if (d == NULL)
		return -1;

	while ((di = readdir(d))) {
		if (di == NULL || strcmp(di->d_name, ".") == 0 || strcmp(di->d_name, "..") == 0)
			continue;

		snprintf(path, PATH_MAX, "/dev/input/%s", di->d_name);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd < 0)
			continue;

		rc = ioctl(fd, EVIOCGNAME(sizeof(input_name) - 1), &input_name);
		if (rc < 0)
			continue;

		c = strstr((char *) &input_name, "\n");
		if (c != NULL)
			*c = '\0';

		if (strcmp(input_name, name) == 0)
			return fd;
		else
			close(fd);
	}

	return -1;
}

/*
 * Vibrator
 */

int vibrator_exists(void)
{
	int fd;

	fd = input_open(rumble_input);
	if (fd < 0)
		return 0;

	close(fd);

	return 1;
}

int sendit(int duration)
{
	struct input_event event;
	struct ff_effect effect;
	int rc;

	if (rumble_fd < 0)
		rumble_fd = input_open(rumble_input);

	if (rumble_fd < 0)
		goto error;

	if (rumble_id >= 0) {
		input_event_set(&event, EV_FF, rumble_id, 0);

		rc = write(rumble_fd, &event, sizeof(event));
		if (rc < (int) sizeof(event))
			goto error;

		ioctl(rumble_fd, EVIOCRMFF, &rumble_id);

		rumble_id = -1;
	}

	// Previous code should have removed the effect already
	if (duration == 0) {
		rc = 0;
		goto complete;
	}

	memset(&effect, 0, sizeof(effect));
	effect.type = FF_RUMBLE;
	effect.id = -1;
	effect.replay.length = duration;
	effect.replay.delay = 0;
	effect.u.rumble.strong_magnitude = 0xffff;

	rc = ioctl(rumble_fd, EVIOCSFF, &effect);
	if (rc < 0)
		goto error;

	rumble_id = effect.id;

	input_event_set(&event, EV_FF, rumble_id, 1);

	rc = write(rumble_fd, &event, sizeof(event));
	if (rc < (int) sizeof(event))
		goto error;

	rc = 0;
	goto complete;

error:
	if (rumble_fd >= 0) {
		if (rumble_id >= 0) {
			input_event_set(&event, EV_FF, rumble_id, 0);
			write(rumble_fd, &event, sizeof(event));

			ioctl(rumble_fd, EVIOCRMFF, &rumble_id);
			rumble_id = -1;
		}

		close(rumble_fd);
		rumble_fd = -1;
	}

	rc = -1;

complete:
	return rc;
}
