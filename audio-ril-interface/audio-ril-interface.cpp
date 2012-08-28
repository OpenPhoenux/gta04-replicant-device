/**
 * Audio RIL Interface for GTA04
 *
 * Copyright (C) 2012 Paul Kocialkowski <contact@paulk.fr>
 *
 * Audio RIL Interface is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Audio RIL Interface is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audio RIL Interface.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#define LOG_TAG "AudioRILInterface"
#include <cutils/log.h>
#include <media/AudioSystem.h>
#include <tinyalsa/asoundlib.h>

#include <audio_ril_interface.h>

#define DEVICE	"Goldelico GTA04"

/*
 * Structures
 */

struct gta04_pdata {
	struct pcm *voice_pcm;
	int mode;
};

/*
 * Functions
 */

void *gta04_pdata_create(void)
{
	struct gta04_pdata *pdata = NULL;

	pdata = (struct gta04_pdata *) calloc(1, sizeof(struct gta04_pdata));
	return (void *) pdata;
}

void gta04_pdata_destroy(void *pdata)
{
	if(pdata != NULL)
		free(pdata);
}

int gta04_mode(void *pdata, int mode)
{
	struct pcm_config config;
	struct gta04_pdata *gta04_pdata = NULL;
	void *buffer = NULL;
	int size = 0;

	if(pdata == NULL)
		return -1;

	gta04_pdata = (struct gta04_pdata *) pdata;

	if(mode == android::AudioSystem::MODE_IN_CALL && gta04_pdata->mode != android::AudioSystem::MODE_IN_CALL) {
		LOGD("Starting call, activating voice!");

		/*
		 * It seems that on GTA04 A4, microphone voice routing to
		 * the modem only works when a capture channel is active.
		 * Note that Android will close any existing capture channel.
		 */
		memset(&config, 0, sizeof(config));

		config.channels = 2;
		config.rate = 44100;
		config.period_size = 1056;
		config.period_count = 2;
		config.format = PCM_FORMAT_S16_LE;

		gta04_pdata->voice_pcm = pcm_open(0, 0, PCM_IN, &config);
		if(!gta04_pdata->voice_pcm || !pcm_is_ready(gta04_pdata->voice_pcm)) {
			LOGE("Failed to open capture channel!");
		}

		size = pcm_get_buffer_size(gta04_pdata->voice_pcm);
		buffer = malloc(size);

		LOGD("Reading one sample");

		pcm_read(gta04_pdata->voice_pcm, buffer, size);

		free(buffer);
	} else if(mode != android::AudioSystem::MODE_IN_CALL && gta04_pdata->mode == android::AudioSystem::MODE_IN_CALL) {
		LOGD("Ending call, deactivating voice!");

		if(gta04_pdata->voice_pcm != NULL)
			pcm_close(gta04_pdata->voice_pcm);

		gta04_pdata->voice_pcm = NULL;
	}

	gta04_pdata->mode = mode;

	return 0;
}

int gta04_mic_mute(void *pdata, int mute)
{
	return 0;
}

int gta04_voice_volume(void *pdata, float volume)
{
	return 0;
}

int gta04_routing(void *pdata, int route)
{
	return 0;
}

/*
 * Interface
 */

extern "C" {

struct audio_ril_interface gta04_interface = {
	NULL,
	gta04_mode,
	gta04_mic_mute,
	gta04_voice_volume,
	gta04_routing
};

struct audio_ril_interface *audio_ril_interface_open(void)
{
	LOGE("%s (%s)", __func__, DEVICE);

	gta04_interface.pdata = gta04_pdata_create();

	return &gta04_interface;
}

void audio_ril_interface_close(struct audio_ril_interface *interface_p)
{
	LOGE("%s (%s)", __func__, DEVICE);

	gta04_pdata_destroy(interface_p->pdata);
}

}
