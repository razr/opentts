
/*
 * flite.c - OpenTTS backend for Flite (Festival Lite)
 *
 * Copyright (C) 2001, 2002, 2003, 2007 Brailcom, o.p.s.
 * Copyright (C) 2010 OpenTTS Developers
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "opentts/opentts_types.h"
#include <flite/flite.h>
#include<logging.h>
#include "audio.h"
#include "module_utils.h"

#define MODULE_NAME     "flite"
#define MODULE_VERSION  "0.5"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Thread and process control */
static int flite_speaking = 0;

static pthread_t flite_speak_thread;
static sem_t *flite_semaphore;

static char **flite_message;
static SPDMessageType flite_message_type;

static int flite_position = 0;
static int flite_pause_requested = 0;
static int current_index_mark;

signed int flite_volume = 0;

/* Internal functions prototypes */
static void flite_set_rate(signed int rate);
static void flite_set_pitch(signed int pitch);
static void flite_set_volume(signed int pitch);
static void flite_set_voice(SPDVoiceType voice);

static void flite_strip_silence(AudioTrack *);
static void *_flite_speak(void *);

/* Voice */
cst_voice *register_cmu_us_kal();
cst_voice *flite_voice;

int flite_stop = 0;

MOD_OPTION_1_INT(FliteMaxChunkLength);
MOD_OPTION_1_STR(FliteDelimiters);

/* Public functions */

int module_load(void)
{
	init_settings_tables();

	REGISTER_DEBUG();

	MOD_OPTION_1_INT_REG(FliteMaxChunkLength, 300);
	MOD_OPTION_1_STR_REG(FliteDelimiters, ".");

	return 0;
}

#define ABORT(msg) g_string_append(info, msg); \
        log_msg(OTTS_LOG_CRIT, "FATAL ERROR:", info->str); \
	*status_info = info->str; \
	g_string_free(info, 0); \
	return -1;

int module_init(char **status_info)
{
	int ret;
	GString *info;

	log_msg(OTTS_LOG_INFO, "Module init");
	*status_info = NULL;
	info = g_string_new("");

	/* Init flite and register a new voice */
	flite_init();
	flite_voice = register_cmu_us_kal();

	if (flite_voice == NULL) {
		log_msg(OTTS_LOG_ERR,
			"Couldn't register the basic kal voice.\n");
		*status_info =
		    g_strdup("Can't register the basic kal voice. "
			     "Currently only kal is supported. Seems your FLite "
			     "installation is incomplete.");
		return -1;
	}

	log_msg(OTTS_LOG_DEBUG, "FliteMaxChunkLength = %d\n",
		FliteMaxChunkLength);
	log_msg(OTTS_LOG_DEBUG, "FliteDelimiters = %s\n", FliteDelimiters);

	flite_message = g_malloc(sizeof(char *));
	*flite_message = NULL;

	flite_semaphore = module_semaphore_init();

	log_msg(OTTS_LOG_INFO, "Flite: creating new thread for flite_speak\n");
	flite_speaking = 0;
	ret = pthread_create(&flite_speak_thread, NULL, _flite_speak, NULL);
	if (ret != 0) {
		log_msg(OTTS_LOG_ERR, "Flite: thread failed\n");
		*status_info =
		    g_strdup("The module couldn't initialize threads "
			     "This could be either an internal problem or an "
			     "architecture problem. If you are sure your architecture "
			     "supports threads, please report a bug.");
		return -1;
	}

	module_audio_id = NULL;

	*status_info = g_strdup("Flite initialized successfully.");

	return 0;
}

#undef ABORT

int module_audio_init(char **status_info)
{
	log_msg(OTTS_LOG_WARN, "Opening audio");
	return module_audio_init_spd(status_info);
}

SPDVoice **module_list_voices(void)
{
	return NULL;
}

int module_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{
	log_msg(OTTS_LOG_DEBUG, "write()\n");

	if (flite_speaking) {
		log_msg(OTTS_LOG_WARN, "Speaking when requested to write");
		return 0;
	}

	log_msg(OTTS_LOG_INFO, "Requested data: |%s|\n", data);

	if (*flite_message != NULL) {
		g_free(*flite_message);
		*flite_message = NULL;
	}
	*flite_message = module_strip_ssml(data);
	flite_message_type = SPD_MSGTYPE_TEXT;

	/* Setting voice */
	UPDATE_PARAMETER(voice_type, flite_set_voice);
	UPDATE_PARAMETER(rate, flite_set_rate);
	UPDATE_PARAMETER(volume, flite_set_volume);
	UPDATE_PARAMETER(pitch, flite_set_pitch);

	/* Send semaphore signal to the speaking thread */
	flite_speaking = 1;
	sem_post(flite_semaphore);

	log_msg(OTTS_LOG_NOTICE, "Flite: leaving write() normally\n\r");
	return bytes;
}

int module_stop(void)
{
	int ret;
	log_msg(OTTS_LOG_NOTICE, "flite: stop()\n");

	flite_stop = 1;
	if (module_audio_id) {
		log_msg(OTTS_LOG_NOTICE, "Stopping audio");
		ret = opentts_audio_stop(module_audio_id);
		if (ret != 0)
			log_msg(OTTS_LOG_WARN,
				"WARNING: Non 0 value from spd_audio_stop: %d",
				ret);
	}

	return 0;
}

size_t module_pause(void)
{
	log_msg(OTTS_LOG_INFO, "pause requested\n");
	if (flite_speaking) {
		log_msg(OTTS_LOG_DEBUG,
			"Flite doesn't support pause, stopping\n");

		module_stop();

		return -1;
	} else {
		return 0;
	}
}

void module_close(int status)
{

	log_msg(OTTS_LOG_DEBUG, "flite: close()\n");

	log_msg(OTTS_LOG_INFO, "Stopping speech");
	if (flite_speaking) {
		module_stop();
	}

	log_msg(OTTS_LOG_INFO, "Terminating threads");
	if (module_terminate_thread(flite_speak_thread) != 0)
		exit(1);

	g_free(flite_voice);

	log_msg(OTTS_LOG_INFO, "Closing audio output");
	opentts_audio_close(module_audio_id);

	exit(status);
}

/* Internal functions */

void flite_strip_silence(AudioTrack * track)
{
	int playlen, skip;

	float stretch =
	    get_param_float(flite_voice->features, "duration_stretch", 1.);
	int speed = (int)(1000. / stretch);
	skip = (187 * track->sample_rate) / speed;
	playlen = track->num_samples - skip * 2;
	if (playlen > 0 && playlen < 500)
		playlen += (skip * 2) / 3;
	if (playlen < 0)
		playlen = 0;

	track->num_samples = playlen;
	assert(track->bits == 16);
	track->samples += skip * track->num_channels;
}

void *_flite_speak(void *nothing)
{
	AudioTrack track;
	cst_wave *wav;
	unsigned int pos;
	char *buf;
	int bytes;
	int ret;

	log_msg(OTTS_LOG_DEBUG, "flite: speaking thread starting.......\n");

	set_speaking_thread_parameters();

	while (1) {
		sem_wait(flite_semaphore);
		log_msg(OTTS_LOG_INFO, "Semaphore on\n");

		flite_stop = 0;
		flite_speaking = 1;

		if (opentts_audio_set_volume(module_audio_id, flite_volume) < 0) {
			log_msg(OTTS_LOG_ERR,
			        "Can't set volume. audio not initialized?\n");
			continue;
		}

		/* TODO: free(buf) */
		buf =
		    (char *)g_malloc((FliteMaxChunkLength + 1) * sizeof(char));
		pos = 0;
		module_report_event_begin();
		while (1) {
			if (flite_stop) {
				log_msg(OTTS_LOG_INFO,
					"Stop in child, terminating");
				flite_speaking = 0;
				module_report_event_stop();
				break;
			}
			bytes =
			    module_get_message_part(*flite_message, buf, &pos,
						    FliteMaxChunkLength,
						    FliteDelimiters);

			if (bytes < 0) {
				log_msg(OTTS_LOG_DEBUG, "End of message");
				flite_speaking = 0;
				module_report_event_end();
				break;
			}

			buf[bytes] = 0;
			log_msg(OTTS_LOG_DEBUG,
				"Returned %d bytes from get_part\n", bytes);
			log_msg(OTTS_LOG_NOTICE, "Text to synthesize is '%s'\n",
				buf);

			if (flite_pause_requested && (current_index_mark != -1)) {
				log_msg(OTTS_LOG_INFO,
					"Pause requested in parent, position %d\n",
					current_index_mark);
				flite_pause_requested = 0;
				flite_position = current_index_mark;
				break;
			}

			if (bytes > 0) {
				log_msg(OTTS_LOG_DEBUG, "Speaking in child...");

				log_msg(OTTS_LOG_NOTICE,
					"Trying to synthesize text");
				wav = flite_text_to_wave(buf, flite_voice);

				if (wav == NULL) {
					log_msg(OTTS_LOG_NOTICE,
						"Stop in child, terminating");
					flite_speaking = 0;
					module_report_event_stop();
					break;
				}

				track.num_samples = wav->num_samples;
				track.num_channels = wav->num_channels;
				track.sample_rate = wav->sample_rate;
				track.bits = 16;
				track.samples = wav->samples;
				flite_strip_silence(&track);

				log_msg(OTTS_LOG_INFO, "Got %d samples",
					track.num_samples);
				if (track.samples != NULL) {
					if (flite_stop) {
						log_msg(OTTS_LOG_NOTICE,
							"Stop in child, terminating");
						flite_speaking = 0;
						module_report_event_stop();
						delete_wave(wav);
						break;
					}
					log_msg(OTTS_LOG_INFO,
						"Playing part of the message");
					switch (module_audio_id->format) {
					case SPD_AUDIO_LE:
						ret =
						    opentts_audio_play
						    (module_audio_id, track,
						     SPD_AUDIO_LE);
						break;
					case SPD_AUDIO_BE:
						ret =
						    opentts_audio_play
						    (module_audio_id, track,
						     SPD_AUDIO_BE);
						break;
					}
					if (ret < 0)
						log_msg(OTTS_LOG_WARN,
							"ERROR: spd_audio failed to play the track");
					if (flite_stop) {
						log_msg(OTTS_LOG_NOTICE,
							"Stop in child, terminating (s)");
						flite_speaking = 0;
						module_report_event_stop();
						delete_wave(wav);
						break;
					}
				}
				delete_wave(wav);
			} else if (bytes == -1) {
				log_msg(OTTS_LOG_INFO,
					"End of data in speaking thread");
				flite_speaking = 0;
				module_report_event_end();
				break;
			} else {
				flite_speaking = 0;
				module_report_event_end();
				break;
			}

			if (flite_stop) {
				log_msg(OTTS_LOG_NOTICE,
					"Stop in child, terminating");
				flite_speaking = 0;
				module_report_event_stop();
				break;
			}
		}
		flite_stop = 0;
		g_free(buf);
	}

	flite_speaking = 0;

	log_msg(OTTS_LOG_NOTICE, "flite: speaking thread ended.......\n");

	pthread_exit(NULL);
}

static void flite_set_rate(signed int rate)
{
	const float stretch_default = 1., stretch_min = 3., stretch_max =
	    (1 - 100. / 175.);
	float stretch;

	assert(rate >= OTTS_VOICE_RATE_MIN && rate <= OTTS_VOICE_RATE_MAX);
	if (rate < OTTS_VOICE_RATE_DEFAULT)
		stretch = stretch_min + ((float)(rate - OTTS_VOICE_RATE_MIN))
		    * (stretch_default - stretch_min)
		    / ((float)(OTTS_VOICE_RATE_DEFAULT - OTTS_VOICE_RATE_MIN));
	else
		stretch =
		    stretch_default + (((float)(rate - OTTS_VOICE_RATE_DEFAULT))
				       * (stretch_max - stretch_default)
				       / ((float)
					  (OTTS_VOICE_RATE_MAX -
					   OTTS_VOICE_RATE_DEFAULT)));

	feat_set_float(flite_voice->features, "duration_stretch", stretch);
}

static void flite_set_volume(signed int volume)
{
	assert(volume >= OTTS_VOICE_VOLUME_MIN
	       && volume <= OTTS_VOICE_VOLUME_MAX);
	flite_volume = volume;
}

static void flite_set_pitch(signed int pitch)
{
	float f0;

	assert(pitch >= OTTS_VOICE_PITCH_MIN && pitch <= OTTS_VOICE_PITCH_MAX);
	f0 = (((float)pitch) * 0.8) + 100.0;
	feat_set_float(flite_voice->features, "int_f0_target_mean", f0);
}

/*
 * This function is a no-op, because we only support one flite voice.
 */
static void flite_set_voice(SPDVoiceType voice)
{
	return;
}
