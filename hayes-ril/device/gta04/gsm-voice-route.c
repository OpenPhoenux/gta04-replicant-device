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
#include <math.h>

#define LOG_TAG "RIL-VOICE"
#include <utils/Log.h>

#include <hayes-ril.h>
#include "gsm-voice-route.h"

/**
 * Upmix a mono audio buffer to an interleaved stereo buffer of twice the size.
 * Both buffers contain the same amount of frames.
 */
void gta04_mono2stereo(unsigned int frames, int16_t *mono_in_buf, int16_t *stereo_out_buf) {
	unsigned int i;
	for (i = 0; i < frames; ++i)
	{
		stereo_out_buf[i * 2]     = mono_in_buf[i];
		stereo_out_buf[i * 2 + 1] = mono_in_buf[i];
	}
	return;
}

/**
 * Downmix a stereo audio buffer to an averaged mono buffer of half the size.
 * Both buffers contain the same amount of frames.
 */
void gta04_stereo2mono(unsigned int frames, int16_t *stereo_in_buf, int16_t *mono_out_buf) {
	unsigned int i;
	for (i = 0; i < frames; ++i)
	{
		mono_out_buf[i] = round((stereo_in_buf[i*2]*0.5)+(stereo_in_buf[i*2+1]*0.5));
	}
	return;
}

/**
 * This function is meant to be called in a new thread, whenever an incoming or
 * outgoing voice call is started. This function will then record the voice from
 * the modem soundcard (hw1,0) and play it back to the gta04 sound card (hw0,0)
 * and vice versa.
 *
 * The modem works with 8000Hz, 16bit, mono
 * The gta04 works with 44100Hz, 16bit, stereo
 * => resampling is needed in between, which can be done via the audio_utils lib
 * => up/downmixing is needed, which is done manually with the functions above
 */
void* gta04_start_voice_routing(void* data)
{
	ALOGD("gta04_start_voice_routing() called");
	struct pcm_config modem_config;
	struct pcm_config gta04_config;
	struct pcm *modem_record;
	struct pcm *modem_play;
	struct pcm *gta04_record;
	struct pcm *gta04_play;
	int16_t *modem_rec;
	int16_t *modem_ply;
	int16_t *gta04_rec;
	int16_t *gta04_ply;
	int16_t *modem_final_playback;
	int16_t *gta04_final_playback;
	unsigned int frames_modem_in, frames_gta04_out;
	unsigned int frames_gta04_in, frames_modem_out;

	struct resampler_itfe *modem_resampler;
	struct resampler_itfe *gta04_resampler;

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

	modem_record = pcm_open(1, 0, PCM_IN, &modem_config);
	modem_play = pcm_open(1, 0, PCM_OUT, &modem_config);
	gta04_record = pcm_open(0, 0, PCM_IN, &gta04_config);
	gta04_play = pcm_open(0, 0, PCM_OUT, &gta04_config);

	if (!modem_record || !pcm_is_ready(modem_record)) {
		ALOGE("Unable to open PCM device 1,0 (%s)", pcm_get_error(modem_record));
		return NULL;
	}
	if (!modem_play || !pcm_is_ready(modem_play)) {
		ALOGE("Unable to open PCM device 1,0 (%s)", pcm_get_error(modem_play));
		return NULL;
	}
	if (!gta04_record || !pcm_is_ready(gta04_record)) {
		ALOGE("Unable to open PCM device 0,0 (%s)", pcm_get_error(gta04_record));
		return NULL;
	}
	if (!gta04_play || !pcm_is_ready(gta04_play)) {
		ALOGE("Unable to open PCM device 0,0 (%s)", pcm_get_error(gta04_play));
		return NULL;
	}

	frames_modem_in = pcm_get_buffer_size(modem_record);
	frames_modem_out = pcm_get_buffer_size(modem_play);
	frames_gta04_in = pcm_get_buffer_size(gta04_record);
	frames_gta04_out = pcm_get_buffer_size(gta04_play);

	modem_rec = malloc(frames_modem_in);
	gta04_ply = malloc(pcm_frames_to_bytes(gta04_play, frames_gta04_out));
	gta04_final_playback = malloc(pcm_frames_to_bytes(gta04_play, frames_gta04_out));

	gta04_rec = malloc(frames_gta04_in);
	modem_ply = malloc(pcm_frames_to_bytes(gta04_play, frames_modem_out));
	modem_final_playback = malloc(pcm_frames_to_bytes(gta04_play, frames_modem_out));

	if (!modem_rec || !gta04_ply || !gta04_final_playback || !modem_ply || !gta04_rec || !modem_final_playback) {
		ALOGE("Unable to allocate memory");
		free(modem_rec);
		free(modem_ply);
		free(modem_final_playback);
		free(gta04_rec);
		free(gta04_ply);
		free(gta04_final_playback);
		pcm_close(modem_record);
		pcm_close(modem_play);
		pcm_close(gta04_record);
		pcm_close(gta04_play);
		return NULL;
	}

	create_resampler(8000, 44100, 1, RESAMPLER_QUALITY_DEFAULT, NULL, &modem_resampler);
	create_resampler(44100, 8000, 2, RESAMPLER_QUALITY_DEFAULT, NULL, &gta04_resampler);

	while (1) {
		/* Record from internal (gta04) card first */
		pcm_read(gta04_record, gta04_rec, frames_gta04_in);
		/* Record from modem card next, this might fail, then we have to stop routing (hangup happened) */
		if (pcm_read(modem_record, modem_rec, frames_modem_in)) {
			ALOGE("Error capturing sample from modem");
			break;
		}

		/* Resample from 44100 Hz Stereo (from gta04) to 8000 Hz Stereo (for modem) */
		gta04_resampler->resample_from_input(gta04_resampler, gta04_rec, &frames_gta04_in, modem_ply, &frames_modem_out);
		/* Downmix from 8000 Hz Stereo to 8000 Hz Mono (for modem) */
		gta04_stereo2mono(frames_modem_out, modem_ply, modem_final_playback);

		/* Resample from 8000 Hz Mono (from modem) to 44100 Hz Mono (for gta04) */
		modem_resampler->resample_from_input(modem_resampler, modem_rec, &frames_modem_in, gta04_ply, &frames_gta04_out);
		/* Upmix from 44100 Hz Mono to 44100 Hz Stereo (for gta04) */
		gta04_mono2stereo(frames_gta04_out, gta04_ply, gta04_final_playback);

		/* Playback on internal (gta04) card first */
		pcm_write(gta04_play, gta04_final_playback, frames_gta04_out);
		/* Playback on modem card next, this might fail, then we have to stop routing (hangup happened) */
		if (pcm_write(modem_play, modem_final_playback, frames_modem_out)) {
			ALOGE("Error playing sample on modem");
			break;
		}
	}

	release_resampler(modem_resampler);
	release_resampler(gta04_resampler);

	free(modem_rec);
	free(modem_ply);
	free(modem_final_playback);
	free(gta04_rec);
	free(gta04_ply);
	free(gta04_final_playback);
	pcm_close(modem_record);
	pcm_close(modem_play);
	pcm_close(gta04_record);
	pcm_close(gta04_play);
	ALOGD("gta04_start_voice_routing() finished");
	return NULL;
}
