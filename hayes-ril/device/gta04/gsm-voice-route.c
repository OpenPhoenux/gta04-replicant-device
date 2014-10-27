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
#include <audio_utils/resampler.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_TAG "RIL-VOICE"
#include <utils/Log.h>

#include <hayes-ril.h>
#include "gsm-voice-route.h"

/* Set to 0 to stop capturing */
int capturing = 1;

void gta04_mono2stereo(unsigned int frames, int16_t *mono_in_buf, int16_t *stereo_out_buf) {
	int i;
	//The size of stereo_out_buf has to be twice the size of mono_in_buf!
	for (i = 0; i < frames; ++i)
	{
		stereo_out_buf[i * 2]     = mono_in_buf[i];
		stereo_out_buf[i * 2 + 1] = mono_in_buf[i];
	}
	return;
}

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
	struct pcm_config modem_config;
	struct pcm_config gta04_config;
	struct pcm *modem_pcm;
	struct pcm *gta04_pcm;
	int16_t *modem_rec;
	int16_t *gta04_ply;
	int16_t *gta04_final_playback;
	unsigned int frames_in, frames_out;

	struct resampler_itfe *modem_resampler;

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

	frames_out = pcm_get_buffer_size(gta04_pcm);
	frames_in = pcm_get_buffer_size(modem_pcm);
	modem_rec = malloc(frames_in);
	gta04_ply = malloc(pcm_frames_to_bytes(gta04_pcm, frames_out));
	if (!modem_rec || !gta04_ply) {
		ALOGE("Unable to allocate memory");
		free(modem_rec);
		free(gta04_ply);
		pcm_close(modem_pcm);
		pcm_close(gta04_pcm);
		return NULL;
	}

	create_resampler(8000, 44100, 1, RESAMPLER_QUALITY_DEFAULT, NULL, &modem_resampler);
	//ALOGD("Capturing sample: %u ch, %u hz, %u bit", modem_config.channels, modem_config.rate, 16);
	//ALOGD("Playing sample: %u ch, %u hz, %u bit", gta04_config.channels, gta04_config.rate, 16);

	while (capturing && !pcm_read(modem_pcm, modem_rec, frames_in)) {
		//Resample from 8000 Hz Mono (from modem) to 44100 Hz Mono (for gta04)
		modem_resampler->resample_from_input(modem_resampler, modem_rec, &frames_in, gta04_ply, &frames_out);
		//Convert 44100 Hz Mono to 44100 Hz Stereo (for gta04)
		gta04_final_playback = malloc(pcm_frames_to_bytes(gta04_pcm, frames_out));
		if(!gta04_final_playback) {
			ALOGE("Unable to allocate playback buffer!");
			free(gta04_final_playback);
			return NULL;
		}
		gta04_mono2stereo(frames_out, gta04_ply, gta04_final_playback);
		if (pcm_write(gta04_pcm, gta04_final_playback, frames_out)) { //TODO: change buffer to gta04_out
			ALOGE("Error playing sample");
			break;
		}
		free(gta04_final_playback);
	}

	//reset capturing for next call
	capturing = 1;

	free(modem_rec);
	free(gta04_ply);
	pcm_close(modem_pcm);
	pcm_close(gta04_pcm);
	ALOGD("gta04_start_voice_routing() finished");
	return NULL;
}
