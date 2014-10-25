/*
 * This file is part of Hayes-RIL.
 *
 * Copyright (C) 2014 Golden Delicious Computers
                      Lukas Märdian <lukas@goldelico.com>
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

#include <tinyalsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_TAG "RIL-VOICE"
#include <utils/Log.h>

#include <hayes-ril.h>
#include "gsm-voice-route.h"

/* Set to 0 to stop capturing */
int capturing = 1;

/*
 * This function is meant to be called in a new thread, whenever an incoming or
 * outgoing voice call is started. This function will then record the voice from
 * the modem soundcard (hw1,0) and play it back to the gta04 sound card (hw0,0)
 * and vice versa.
 *
 * The modem works with 8000Hz, 16bit, mono
 * The gta04 works with 44100Hz, 16bit, stereo
 * => resampling is needed in between, which can be done via the audio_utils lib
 */
void* gta04_start_voice_routing(void* data)
{
	ALOGD("gta04_start_voice_routing() called");
	unsigned int frames;
	struct pcm_config modem_config;
	struct pcm_config gta04_config;
	struct pcm *modem_pcm;
	struct pcm *gta04_pcm;
	char *buffer;
	unsigned int size;
	unsigned int bytes_read = 0;
	int num_read;

	modem_config.channels = 1;
	modem_config.rate = 8000;
	modem_config.period_size = 1024; //tinycap default
	modem_config.period_count = 4; //tinycap default
	modem_config.format = PCM_FORMAT_S16_LE;
	modem_config.start_threshold = 0;
	modem_config.stop_threshold = 0;
	modem_config.silence_threshold = 0;

	gta04_config.channels = 2;
	gta04_config.rate = 44100;
	gta04_config.period_size = 1024; //tinycap default
	gta04_config.period_count = 4; //tinycap default
	gta04_config.format = PCM_FORMAT_S16_LE;
	gta04_config.start_threshold = 0;
	gta04_config.stop_threshold = 0;
	gta04_config.silence_threshold = 0;

	modem_pcm = pcm_open(1, 0, PCM_IN,  &modem_config);
	gta04_pcm = pcm_open(0, 0, PCM_OUT, &gta04_config);

	if (!modem_pcm || !pcm_is_ready(modem_pcm)) {
		ALOGE("Unable to open PCM device 1,0 (%s)", pcm_get_error(modem_pcm));
		return NULL;
	}
	if (!gta04_pcm || !pcm_is_ready(gta04_pcm)) {
		ALOGE("Unable to open PCM device 0,0 (%s)", pcm_get_error(gta04_pcm));
		return NULL;
	}

	size = pcm_frames_to_bytes(gta04_pcm, pcm_get_buffer_size(gta04_pcm));
	ALOGD("gta04 size: %d (f2b)", size);
	size = pcm_get_buffer_size(gta04_pcm);
	ALOGD("gta04 size: %d", size);
	size = pcm_get_buffer_size(modem_pcm);
	ALOGD("modem size: %d", size);
	buffer = malloc(size);
	if (!buffer) {
		ALOGE("Unable to allocate %d bytes", size);
		free(buffer);
		pcm_close(modem_pcm);
		pcm_close(gta04_pcm);
		return NULL;
	}

	ALOGD("Capturing sample: %u ch, %u hz, %u bit", modem_config.channels, modem_config.rate, 16);
	ALOGD("Playing sample: %u ch, %u hz, %u bit", gta04_config.channels, gta04_config.rate, 16);

	//Disable for now
	capturing = 0; //TODO: testing only, remove later
	sleep(15); //TODO: testing only, remove later
	while (capturing && !pcm_read(modem_pcm, buffer, size)) {
		bytes_read += size;
		//TODO: we need to resample the data here! Compare to https://gitorious.org/replicant/hardware_tinyalsa-audio
		if (pcm_write(gta04_pcm, buffer, size)) {
			ALOGE("Error playing sample");
			break;
		}
	}
	//TODO: make sure the thread exits after the call has ended

	//reset capturing for next call
	capturing = 1;

	free(buffer);
	pcm_close(modem_pcm);
	pcm_close(gta04_pcm);
	frames = bytes_read / ((16 / 8) * modem_config.channels);
	ALOGD("Captured %d frames", frames);
	return NULL;
}
