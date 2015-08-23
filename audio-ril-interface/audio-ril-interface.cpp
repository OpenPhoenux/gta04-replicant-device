/**
 * Audio RIL Interface for GTA04
 *
 * Copyright (C) 2012 Paul Kocialkowski <contact@paulk.fr>
 *               2015 Golden Delicious Computers
                      Lukas Märdian <lukas@goldelico.com>
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
#include <math.h>

#define LOG_TAG "AudioRILInterface"
#include <cutils/log.h>
#include <media/AudioSystem.h>
#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h> //for swrouting

#include <audio_ril_interface.h>

#define DEVICE	"Goldelico GTA04"

/*
 * GTA04 A3 Software Routing
 */

pthread_t gta04_voice_routing = 0;

/**
 * Upmix a mono audio buffer to an interleaved stereo buffer of twice the size.
 * Both buffers contain the same amount of frames.
 */
void gta04_mono2stereo(unsigned int frames, int16_t *mono_in_buf, int16_t *stereo_out_buf)
{
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
void gta04_stereo2mono(unsigned int frames, int16_t *stereo_in_buf, int16_t *mono_out_buf)
{
	unsigned int i;
	for (i = 0; i < frames; ++i)
	{
		mono_out_buf[i] = round((stereo_in_buf[i*2]*0.5)+(stereo_in_buf[i*2+1]*0.5));
	}
	return;
}

/**
 * Return 1 if hardware routing is enabled (i.e. "Voice route" ctl -> "Voice to twl4030")
 * Return 0 if software routing is enabled (i.e. "Voice route" ctl -> "Voice to SoC")
 * Return -1 otherwise -> ERROR
 */
int gta04_check_hwrouting() {
	struct mixer *mixer;
	struct mixer_ctl *ctl;
	const char *active_value;
	int active_id;

	mixer = mixer_open(0); //open card 0 (gta04 internal)
	if (!mixer) {
		ALOGE("Failed to open mixer");
		return -1;
	}

	ctl = mixer_get_ctl_by_name(mixer, "Voice route"); //open "Voice route" ctl
	active_id = mixer_ctl_get_value(ctl, 0);
	active_value = mixer_ctl_get_enum_string(ctl, active_id);
	if (strncmp(active_value, "Voice to twl4030", 16) == 0) {
		ALOGD("HW routing detected"); //don't start the SW router in this case
		sleep(5); //sleep a little, so we won't be called every sec (in each AT+CLCC request)
		mixer_close(mixer);
		return 1;
	}
	else if(strncmp(active_value, "Voice to SoC", 12) == 0) {
		ALOGD("SW routing detected");
		mixer_close(mixer);
		return 0;
	}
	else {
		ALOGE("Unable to detect HW/SW routing state");
		mixer_close(mixer);
		return -1;
	}
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

	/*
	 * Data
	 */
	struct pcm_config p0, r0, p1, r1;
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
	int rc;
	int gta04_rec_buffer_size, gta04_ply_buffer_size;
	int modem_rec_buffer_size, modem_ply_buffer_size;

	unsigned int frames_avail1, frames_avail2;
	struct timespec timespec_dummy;

	struct resampler_itfe *modem_resampler;
	struct resampler_itfe *gta04_resampler;

	//p0 = playback gta04 internal ("default")
	p0.channels = 2;
	p0.rate = 44100;
	/*
	 * we want a period_size of 1323, so we get a latency of 33,3ms (= 44100/1323)
	 * 1323 = 5292/(2*16/8), see magic recalculation in pcm_read()/pcm_write()
	 */
	p0.period_size = 5292;
	p0.period_count = 4;
	p0.format = PCM_FORMAT_S16_LE;
	p0.start_threshold = 0; //default
	p0.stop_threshold = 0; //default
	p0.silence_threshold = 0;

	//r0 = record gta04 internal ("default")
	r0.channels = 2;
	r0.rate = 44100;
	/*
	 * we want a period_size of 1323, so we get a latency of 33,3ms (= 44100/1323)
	 * 1323 = 5292/(2*16/8), see magic recalculation in pcm_read()/pcm_write()
	 */
	r0.period_size = 5292;
	r0.period_count = 4;
	r0.format = PCM_FORMAT_S16_LE;
	r0.start_threshold = 0; //default
	r0.stop_threshold = 0; //default
	r0.silence_threshold = 0;

	//p1 = playback modem ("hw:1,0")
	p1.channels = 1;
	p1.rate = 8000;
	/*
	 * we want a period size of 240, so we get a latency of 33,3ms (= 8000/240)
	 * 240 = 480/(1*16/8), see magic recalculation in pcm_read()/pcm_write()
	 */
	p1.period_size = 480;
	p1.period_count = 4;
	p1.format = PCM_FORMAT_S16_LE;
	p1.start_threshold = 0; //default
	p1.stop_threshold = 0; //default
	p1.silence_threshold = 0;

	//r1 = record modem ("hw:1,0")
	r1.channels = 1;
	r1.rate = 8000;
	/*
	 * we want a period size of 240, so we get a latency of 33,3ms (= 8000/240)
	 * 240 = 480/(1*16/8), see magic recalculation in pcm_read()/pcm_write()
	 */
	r1.period_size = 480;
	r1.period_count = 4;
	r1.format = PCM_FORMAT_S16_LE;
	r1.start_threshold = 0; //default
	r1.stop_threshold = 0; //default
	r1.silence_threshold = 0;

	/*
	 * Setup
	 */
	modem_record = pcm_open(1, 0, PCM_IN, &r1);
	modem_play = pcm_open(1, 0, PCM_OUT, &p1);
	gta04_record = pcm_open(0, 0, PCM_IN, &r0);
	gta04_play = pcm_open(0, 0, PCM_OUT, &p0);

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

	/* buffer_size = period_size * period_count, see pcm_open() */
	frames_modem_in = pcm_get_buffer_size(modem_record);
	frames_modem_out = pcm_get_buffer_size(modem_play);
	frames_gta04_in = pcm_get_buffer_size(gta04_record);
	frames_gta04_out = pcm_get_buffer_size(gta04_play);

	/* Use twice the buffer size, because we get a segfault otherwise... */
	gta04_rec_buffer_size = 2*pcm_frames_to_bytes(gta04_record, frames_gta04_in);
	gta04_ply_buffer_size = 2*pcm_frames_to_bytes(gta04_play, frames_gta04_out);
	gta04_rec = (int16_t *)malloc(gta04_rec_buffer_size);
	gta04_ply = (int16_t *)malloc(gta04_ply_buffer_size);
	gta04_final_playback = (int16_t *)malloc(gta04_ply_buffer_size);

	modem_rec_buffer_size = 2*pcm_frames_to_bytes(modem_record, frames_modem_in);
	modem_ply_buffer_size = 2*pcm_frames_to_bytes(modem_play, frames_modem_out);
	modem_rec = (int16_t *)malloc(modem_rec_buffer_size);
	modem_ply = (int16_t *)malloc(modem_ply_buffer_size);
	modem_final_playback = (int16_t *)malloc(modem_ply_buffer_size);

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

	/* We want realtime thread priority */
	//rc = nice(-20);
	//if (rc != -20)
	//	ALOGD("nice() failed");

	while (1) {
		/* Clear buffers */
		memset(gta04_rec, 0, gta04_rec_buffer_size);
		memset(gta04_ply, 0, gta04_ply_buffer_size);
		memset(gta04_final_playback, 0, gta04_ply_buffer_size);
		memset(modem_rec, 0, modem_rec_buffer_size);
		memset(modem_ply, 0, modem_ply_buffer_size);
		memset(modem_final_playback, 0, modem_ply_buffer_size);

		/* Record from internal (gta04) card first */
		rc = pcm_read(gta04_record, gta04_rec, r0.period_size);
		if(rc<0)
			ALOGD("pcm_read, gta04: ERRNO %d", rc);

		/* Record from modem card next, this might fail, then we have to stop routing (hangup happened) */
		rc = pcm_read(modem_record, modem_rec, r1.period_size);
		if (rc==-1) {
			ALOGD("Error capturing sample from modem (hangup)");
			break;
		}
		if(rc<0)
			ALOGD("pcm_read, modem: ERRNO %d", rc);

		/* Resample from 44100 Hz Stereo (from gta04) to 8000 Hz Stereo */
		gta04_resampler->resample_from_input(gta04_resampler, gta04_rec, &frames_gta04_in, modem_ply, &frames_modem_out);
		/* Downmix from 8000 Hz Stereo to 8000 Hz Mono (for modem) */
		gta04_stereo2mono(frames_modem_out, modem_ply, modem_final_playback);

		/* Resample from 8000 Hz Mono (from modem) to 44100 Hz Mono */
		modem_resampler->resample_from_input(modem_resampler, modem_rec, &frames_modem_in, gta04_ply, &frames_gta04_out);
		/* Upmix from 44100 Hz Mono to 44100 Hz Stereo (for gta04) */
		gta04_mono2stereo(frames_gta04_out, gta04_ply, gta04_final_playback);

		/* Playback on internal (gta04) card first */
		rc = pcm_write(gta04_play, gta04_final_playback, p0.period_size);
		if(rc<0)
			ALOGD("pcm_write, gta04: ERRNO %d", rc);

		/* Playback on modem card next, this might fail, then we have to stop routing (hangup happened) */
		rc = pcm_write(modem_play, modem_final_playback, p1.period_size);
		if (rc==-1) {
			ALOGE("Error playing sample on modem (hangup)");
			break;
		}
		if(rc<0)
			ALOGD("pcm_write, modem: ERRNO %d", rc);
	}

	/*
	 * Teardown
	 */
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

/*
 * End of GTA04 A3 Software Routing
 */

/*
 * Structures
 */

struct gta04_pdata {
	struct pcm *voice_pcm;
	int pcm_active;
};

/*
 * Functions
 */

void *gta04_pdata_create(void)
{
	struct gta04_pdata *pdata = NULL;

	pdata = (struct gta04_pdata *) calloc(1, sizeof(struct gta04_pdata));
	pdata->pcm_active = 0;
	return (void *) pdata;
}

void gta04_pdata_destroy(void *pdata)
{
	if(pdata != NULL)
		free(pdata);
}

int gta04_open_pcm(void *pdata)
{
	struct pcm_config config;
	struct gta04_pdata *gta04_pdata = NULL;
	void *buffer = NULL;
	int size = 0;

	if(pdata == NULL)
		return -1;

	gta04_pdata = (struct gta04_pdata *) pdata;

	if(gta04_pdata->pcm_active == 0)
	{
		//Swrouting
		if(gta04_check_hwrouting() == 0)
		{
			if (gta04_voice_routing == 0 || pthread_kill(gta04_voice_routing, 0) != 0)
			{
				ALOGD("Starting gta04_voice_routing thread");
				pthread_create(&gta04_voice_routing, NULL, gta04_start_voice_routing, NULL);
			}
		}
		//Hwrouting
		else
		{
			ALOGD("Starting call, activating voice!");

			// It seems that on GTA04 A4, microphone voice routing to
			// the modem only works when a capture channel is active.
			// Note that Android will close any existing capture channel.

			memset(&config, 0, sizeof(config));

			config.channels = 2;
			config.rate = 44100;
			config.period_size = 1056;
			config.period_count = 2;
			config.format = PCM_FORMAT_S16_LE;

			gta04_pdata->voice_pcm = pcm_open(0, 0, PCM_IN, &config);
			if(!gta04_pdata->voice_pcm || !pcm_is_ready(gta04_pdata->voice_pcm)) {
				ALOGE("Failed to open capture channel!");
			}

			size = pcm_get_buffer_size(gta04_pdata->voice_pcm);
			buffer = malloc(size);

			ALOGD("Reading one sample");

			pcm_read(gta04_pdata->voice_pcm, buffer, size);

			free(buffer);
			gta04_pdata->pcm_active = 1; //only used for hwrouting
		}
	}
	return 0;
}

int gta04_close_pcm(void *pdata)
{
	struct gta04_pdata *gta04_pdata = NULL;
	gta04_pdata = (struct gta04_pdata *) pdata;

	//This is only for hwrouting.
	//The swrouting thread detects the call hangup itself
	if(gta04_pdata->pcm_active == 1)
	{
		ALOGD("Ending call, deactivating voice!");

		if(gta04_pdata->voice_pcm != NULL)
			pcm_close(gta04_pdata->voice_pcm);

		gta04_pdata->voice_pcm = NULL;
	}
	return 0;
}

int gta04_mic_mute(void *pdata, int mute)
{
	ALOGD("RIL-Interface MIC-MUTE");
	return 0;
}

int gta04_voice_volume(void *pdata, audio_devices_t device, float volume)
{
	ALOGD("RIL-Interface VOICE-VOLUME");
	return 0;
}

int gta04_route(void *pdata, audio_devices_t device)
{
	ALOGD("RIL-Interface ROUTE: %d", device);
	gta04_open_pcm(pdata);
	return 0;
}

/*
 * Interface
 */

extern "C" {

struct audio_ril_interface gta04_interface = {
	NULL, //pdata
	gta04_mic_mute,
	gta04_voice_volume,
	gta04_route
};

struct audio_ril_interface *audio_ril_interface_open(void)
{
	ALOGE("%s (%s)", __func__, DEVICE);
	ALOGD("RIL-Interface OPEN");
	gta04_interface.pdata = gta04_pdata_create();

	return &gta04_interface;
}

void audio_ril_interface_close(struct audio_ril_interface *interface_p)
{
	gta04_close_pcm(gta04_interface.pdata);
	gta04_pdata_destroy(interface_p->pdata);
	ALOGD("RIL-Interface CLOSE");
	ALOGE("%s (%s)", __func__, DEVICE);
}

}
