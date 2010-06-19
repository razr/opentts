/*
 * libao.c -- The libao backend for opentts
 *
 * Author: Marco Skambraks <marco@openblinux.de>
 * Date:  2009-12-15
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Leser General Public License as published by the Free
 * Software Foundation; either version 2.1, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this package; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <ao/ao.h>
#include <glib.h>

#define AUDIO_PLUGIN_ENTRY otts_libao_LTX_audio_plugin_get
#include <opentts/opentts_audio_plugin.h>
#include<logging.h>

/* send a packet of XXX bytes to the sound device */
#define AO_SEND_BYTES 256

static logging_func audio_log;
static volatile int ao_stop_playback = 0;

static int default_driver;

ao_device *device = NULL;

static AudioID *libao_open(void **pars, logging_func log)
{
	AudioID *id;

	if (audio_log == NULL)
		audio_log = log;

	id = (AudioID *) g_malloc(sizeof(AudioID));

	ao_initialize();
	default_driver = ao_default_driver_id();
	return id;
}

static int libao_play(AudioID * id, AudioTrack track)
{
	int bytes_per_sample;

	int num_bytes;

	int outcnt = 0;

	signed short *output_samples;

	int i;

	ao_sample_format format;

	if (id == NULL)
		return -1;
	if (track.samples == NULL || track.num_samples <= 0)
		return 0;

	/* Zero the whole ao_sample_format structure. */
	memset(&format, '\0', sizeof(ao_sample_format));

	/* Choose the correct format */
	format.bits = track.bits;
	if (track.bits == 16) {
		bytes_per_sample = 2;
		switch (id->format) {
		case SPD_AUDIO_LE:
			format.byte_format = AO_FMT_LITTLE;
			break;
		case SPD_AUDIO_BE:
			format.byte_format = AO_FMT_BIG;
			break;
		}
	} else if (track.bits == 8)
		bytes_per_sample = 1;
	else {
		audio_log(OTTS_LOG_WARN, "libao: Unrecognized sound data format.\n");
		return -10;
	}
	format.channels = track.num_channels;
	format.rate = track.sample_rate;
	audio_log(OTTS_LOG_NOTICE, "libao: Starting playback");
	output_samples = track.samples;
	num_bytes = track.num_samples * bytes_per_sample;
	if (device == NULL)
		device = ao_open_live(default_driver, &format, NULL);
	if (device == NULL) {
		audio_log(OTTS_LOG_ERR, "libao: error opening libao dev");
		return -2;
	}
	audio_log(OTTS_LOG_NOTICE, "libao: bytes to play: %d, (%f secs)", num_bytes,
		  (((float)(num_bytes) / 2) / (float)track.sample_rate));

	ao_stop_playback = 0;
	outcnt = 0;
	i = 0;

	while ((outcnt < num_bytes) && !ao_stop_playback) {
		if ((num_bytes - outcnt) > AO_SEND_BYTES)
			i = AO_SEND_BYTES;
		else
			i = (num_bytes - outcnt);

		if (!ao_play(device, (char *)output_samples + outcnt, i)) {
			ao_close(device);
			device = NULL;
			audio_log(OTTS_LOG_NOTICE,
				  "libao: ao_play() - closing device - re-open it in next run\n");
			return -1;
		}
		outcnt += i;
	}

	return 0;

}

/* stop the libao_play() loop */
static int libao_stop(AudioID * id)
{

	ao_stop_playback = 1;
	return 0;
}

static int libao_close(AudioID * id)
{

	if (device != NULL)
		ao_close(device);
	device = NULL;

	ao_shutdown();

	g_free(id);
	id = NULL;

	return 0;
}

static int libao_set_volume(AudioID * id, int volume)
{
	return 0;
}

static char const *libao_get_playcmd(void)
{
	return NULL;
}

/* Provide the libao backend. */
static audio_plugin_t libao_functions = {
	"libao",
	libao_open,
	libao_play,
	libao_stop,
	libao_close,
	libao_set_volume,
	libao_get_playcmd
};

audio_plugin_t *libao_plugin_get(void)
{
	return &libao_functions;
}

audio_plugin_t *AUDIO_PLUGIN_ENTRY(void)
    __attribute__ ((weak, alias("libao_plugin_get")));
