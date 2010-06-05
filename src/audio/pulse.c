/*
 * pulse.c -- The simple pulseaudio backend for opentts
 *
 * Based on libao.c from Marco Skambraks <marco@openblinux.de>
 * Date:  2009-12-15
 *
 * Copied from Luke Yelavich's libao.c driver, and merged with code from
 * Marco's ao_pulse.c driver, by Bill Cox, Dec 21, 2009.
 *
 * Minor changes be Rui Batista <rui.batista@ist.utl.pt> to configure settings through OpenTTS configuration files
 * Date: Dec 22, 2009
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

#include <glib.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#define SPD_AUDIO_PLUGIN_ENTRY pulse_LTX_spd_audio_plugin_get
#include <opentts/opentts_audio_plugin.h>

typedef struct {
	AudioID id;
	pa_simple *pa_simple;
	char *pa_server;
	int pa_min_audio_length;
	volatile int pa_stop_playback;
	int current_rate;
	int current_bps;
	int current_channels;
} pulse_id_t;

/* Switch this on to debug, see output log location in MSG() */
/*#define DEBUG_PULSE*/

/* send a packet of XXX bytes to the sound device */
#define PULSE_SEND_BYTES 256

/* This is the smallest audio sound we are expected to play immediately without buffering. */
/* Changed to define on config file. Default is the same. */
#define DEFAULT_PA_MIN_AUDIO_LENgTH 100

static int pulse_log_level;
static char const *pulse_play_cmd = "paplay";

static FILE *pulseDebugFile = NULL;

/* Write to /tmp/opentts-pulse.log */
#ifdef DEBUG_PULSE
static void MSG(char *message, ...)
{
	va_list ap;

	if (pulseDebugFile == NULL) {
		pulseDebugFile = fopen("/tmp/opentts-pulse.log", "w");
	}
	va_start(ap, message);
	vfprintf(pulseDebugFile, message, ap);
	va_end(ap);
	fflush(pulseDebugFile);
}
#else
static void MSG(char *message, ...)
{
}
#endif

static AudioID *pulse_open(void **pars)
{
	pulse_id_t *pulse_id;

	pulse_id = (pulse_id_t *) g_malloc(sizeof(pulse_id_t));

	pulse_id->pa_simple = NULL;
	pulse_id->pa_server = (char *)pars[3];
	pulse_id->current_rate = -1;
	pulse_id->current_bps = -1;
	pulse_id->current_channels = -1;
	pulse_id->pa_min_audio_length = DEFAULT_PA_MIN_AUDIO_LENgTH;

	if (!strcmp(pulse_id->pa_server, "default")) {
		pulse_id->pa_server = NULL;
	}

	if (pars[4] != NULL && atoi(pars[4]) != 0)
		pulse_id->pa_min_audio_length = atoi(pars[4]);

	pulse_id->pa_stop_playback = 0;
	return (AudioID *) pulse_id;
}

static int pulse_play(AudioID * id, AudioTrack track)
{
	int bytes_per_sample;
	int num_bytes;
	int outcnt = 0;
	signed short *output_samples;
	int i;
	pa_sample_spec ss;
	pa_buffer_attr buffAttr;
	int error;
	pulse_id_t *pulse_id = (pulse_id_t *) id;

	if (id == NULL) {
		return -1;
	}
	if (track.samples == NULL || track.num_samples <= 0) {
		return 0;
	}
	MSG("Starting playback\n");
	/* Choose the correct format */
	if (track.bits == 16) {
		bytes_per_sample = 2;
	} else if (track.bits == 8) {
		bytes_per_sample = 1;
	} else {
		MSG("ERROR: Unsupported sound data format, track.bits = %d\n",
		    track.bits);
		return -1;
	}
	output_samples = track.samples;
	num_bytes = track.num_samples * bytes_per_sample;

	/* Insure that the connection is open, and that its parameters
	 * are suitable for this track. */
	if (pulse_id->pa_simple == NULL
	    || pulse_id->current_rate != track.sample_rate
	    || pulse_id->current_bps != track.bits
	    || pulse_id->current_channels != track.num_channels) {
		/* Choose the correct format */
		ss.rate = track.sample_rate;
		ss.channels = track.num_channels;
		if (bytes_per_sample == 2) {
			switch (id->format) {
			case SPD_AUDIO_LE:
				ss.format = PA_SAMPLE_S16LE;
				break;
			case SPD_AUDIO_BE:
				ss.format = PA_SAMPLE_S16BE;
				break;
			}
		} else {
			ss.format = PA_SAMPLE_U8;
		}
		/* Set prebuf to one sample so that keys are spoken as soon as typed rather than delayed until the next key pressed */
		buffAttr.maxlength = (uint32_t) - 1;
		//buffAttr.tlength = (uint32_t)-1; - this is the default, which causes key echo to not work properly.
		buffAttr.tlength = pulse_id->pa_min_audio_length;
		buffAttr.prebuf = (uint32_t) - 1;
		buffAttr.minreq = (uint32_t) - 1;
		buffAttr.fragsize = (uint32_t) - 1;

		if (pulse_id->pa_simple != NULL) {
			/* Close the old connection. */
			pa_simple_free(pulse_id->pa_simple);
			MSG("Reopenning connection due to change in track parameters sample_rate:%d bps:%d channels:%d\n", track.sample_rate, track.bits, track.num_channels);
		}

		if (!
		    (pulse_id->pa_simple =
		     pa_simple_new(pulse_id->pa_server, "OpenTTS",
				   PA_STREAM_PLAYBACK, NULL, "playback", &ss,
				   NULL, &buffAttr, &error))) {
			fprintf(stderr,
				__FILE__ ": pa_simple_new() failed: %s\n",
				pa_strerror(error));
			return 1;
		}

		pulse_id->current_rate = track.sample_rate;
		pulse_id->current_bps = track.bits;
		pulse_id->current_channels = track.num_channels;
	}
	MSG("bytes to play: %d, (%f secs)\n", num_bytes,
	    (((float)(num_bytes) / 2) / (float)track.sample_rate));
	pulse_id->pa_stop_playback = 0;
	outcnt = 0;
	i = 0;
	while ((outcnt < num_bytes) && !pulse_id->pa_stop_playback) {
		if ((num_bytes - outcnt) > PULSE_SEND_BYTES) {
			i = PULSE_SEND_BYTES;
		} else {
			i = (num_bytes - outcnt);
		}
		if (pa_simple_write
		    (pulse_id->pa_simple, ((char *)output_samples) + outcnt, i,
		     &error) < 0) {
			pa_simple_drain(pulse_id->pa_simple, NULL);
			pa_simple_free(pulse_id->pa_simple);
			pulse_id->pa_simple = NULL;
			MSG("ERROR: Audio: pulse_play(): %s - closing device - re-open it in next run\n", pa_strerror(error));
			return -1;
		} else {
			MSG("Pulse: wrote %u bytes\n", i);
		}
		outcnt += i;
	}
	return 0;
}

/* stop the pulse_play() loop */
static int pulse_stop(AudioID * id)
{
	pulse_id_t *pulse_id = (pulse_id_t *) id;

	pulse_id->pa_stop_playback = 1;
	return 0;
}

static int pulse_close(AudioID * id)
{
	pulse_id_t *pulse_id = (pulse_id_t *) id;

	if (pulse_id->pa_simple != NULL) {
		pa_simple_drain(pulse_id->pa_simple, NULL);
		pa_simple_free(pulse_id->pa_simple);
		pulse_id->pa_simple = NULL;
	}

	g_free(pulse_id);
	id = NULL;

	return 0;
}

static int pulse_set_volume(AudioID * id, int volume)
{
	return 0;
}

static void pulse_set_loglevel(int level)
{
	if (level) {
		pulse_log_level = level;
	}
}

static char const *pulse_get_playcmd(void)
{
	return pulse_play_cmd;
}

/* Provide the pulse backend. */
static spd_audio_plugin_t pulse_functions = {
	"pulse",
	pulse_open,
	pulse_play,
	pulse_stop,
	pulse_close,
	pulse_set_volume,
	pulse_set_loglevel,
	pulse_get_playcmd
};

spd_audio_plugin_t *pulse_plugin_get(void)
{
	return &pulse_functions;
}

spd_audio_plugin_t *SPD_AUDIO_PLUGIN_ENTRY(void)
    __attribute__ ((weak, alias("pulse_plugin_get")));
