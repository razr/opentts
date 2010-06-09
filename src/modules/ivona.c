
/*
 * ivona.c - OpenTTS backend for Ivona (IVO Software)
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

/* this file is strictly based on flite.c */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libdumbtts.h>
#include "audio.h"

#include "opentts/opentts_types.h"
#include "module_utils.h"
#include "ivona_client.h"

#if HAVE_SNDFILE
#include <sndfile.h>
#endif

#define MODULE_NAME     "ivona"
#define MODULE_VERSION  "0.2"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Thread and process control */
static int ivona_speaking = 0;

static pthread_t ivona_speak_thread;
static sem_t *ivona_semaphore;

static char **ivona_message;
static SPDMessageType ivona_message_type;

signed int ivona_volume = 0;
signed int ivona_cap_mode = 0;
int ivona_punct_mode = 0;

/* Internal functions prototypes */
static void ivona_set_volume(signed int volume);
static void ivona_set_punctuation_mode(SPDPunctuation punct_mode);
static void ivona_set_cap_let_recogn(SPDCapitalLetters cap_mode);

static void *_ivona_speak(void *);

int ivona_stop = 0;

MOD_OPTION_1_STR(IvonaDelimiters);
MOD_OPTION_1_STR(IvonaPunctuationSome);
MOD_OPTION_1_INT(IvonaMinCapLet);
MOD_OPTION_1_STR(IvonaSoundIconPath);
MOD_OPTION_1_STR(IvonaServerHost);
MOD_OPTION_1_INT(IvonaServerPort);
MOD_OPTION_1_INT(IvonaSampleFreq);

MOD_OPTION_1_STR(IvonaSpeakerLanguage);
MOD_OPTION_1_STR(IvonaSpeakerName);

static struct dumbtts_conf *ivona_conf;

/* Public functions */

int module_load(void)
{
	init_settings_tables();

	REGISTER_DEBUG();

	MOD_OPTION_1_STR_REG(IvonaDelimiters, ".;:,!?");
	MOD_OPTION_1_INT_REG(IvonaMinCapLet, 0);
	MOD_OPTION_1_STR_REG(IvonaSoundIconPath,
			     "/usr/share/sound/sound-icons/");

	MOD_OPTION_1_STR_REG(IvonaServerHost, "127.0.0.1");
	MOD_OPTION_1_INT_REG(IvonaServerPort, 9123);
	MOD_OPTION_1_INT_REG(IvonaSampleFreq, 16000);

	MOD_OPTION_1_STR_REG(IvonaSpeakerLanguage, "pl");
	MOD_OPTION_1_STR_REG(IvonaSpeakerName, "Jacek");

	MOD_OPTION_1_STR_REG(IvonaPunctuationSome, "()");
	ivona_init_cache();

	return 0;
}

#define ABORT(msg) g_string_append(info, msg); \
        dbg("FATAL ERROR:", info->str); \
	*status_info = info->str; \
	g_string_free(info, 0); \
	return -1;

int module_init(char **status_info)
{
	int ret;
	GString *info;

	dbg("Module init");

	*status_info = NULL;
	info = g_string_new("");

	/* Init Ivona */
	if (ivona_init_sock(IvonaServerHost, IvonaServerPort)) {
		dbg("Couldn't init socket parameters");
		*status_info = g_strdup("Can't initialize socket. "
					"Check server host/port.");
		return -1;
	}
	ivona_conf = dumbtts_TTSInit(IvonaSpeakerLanguage);

	dbg("IvonaDelimiters = %s\n", IvonaDelimiters);

	ivona_message = g_malloc(sizeof(char *));
	*ivona_message = NULL;

	ivona_semaphore = module_semaphore_init();

	dbg("Ivona: creating new thread for ivona_speak\n");
	ivona_speaking = 0;
	ret = pthread_create(&ivona_speak_thread, NULL, _ivona_speak, NULL);
	if (ret != 0) {
		dbg("Ivona: thread failed\n");
		*status_info =
		    g_strdup("The module couldn't initialize threads "
			     "This could be either an internal problem or an "
			     "architecture problem. If you are sure your architecture "
			     "supports threads, please report a bug.");
		return -1;
	}

	module_audio_id = NULL;

	*status_info = g_strdup("Ivona initialized successfully.");

	return 0;
}

#undef ABORT

int module_audio_init(char **status_info)
{
	dbg("Opening audio");
	return module_audio_init_spd(status_info);
}

static SPDVoice voice_jacek;
static SPDVoice *voice_ivona[] = { &voice_jacek, NULL };

SPDVoice **module_list_voices(void)
{
	voice_jacek.name = IvonaSpeakerName;
	voice_jacek.language = IvonaSpeakerLanguage;
	return voice_ivona;
}

int module_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{
	dbg("write()\n");

	if (ivona_speaking) {
		dbg("Speaking when requested to write");
		return 0;
	}

	if (module_write_data_ok(data) != 0)
		return -1;

	dbg("Requested data: |%s|\n", data);

	if (*ivona_message != NULL) {
		g_free(*ivona_message);
		*ivona_message = NULL;
	}
	*ivona_message = module_strip_ssml(data);
	ivona_message_type = msgtype;
	if ((msgtype == SPD_MSGTYPE_TEXT)
	    && (msg_settings.spelling_mode == SPD_SPELL_ON))
		ivona_message_type = SPD_MSGTYPE_SPELL;

	/* Setting voice */
	UPDATE_PARAMETER(volume, ivona_set_volume);
	UPDATE_PARAMETER(cap_let_recogn, ivona_set_cap_let_recogn);
	UPDATE_PARAMETER(punctuation_mode, ivona_set_punctuation_mode);

	/* Send semaphore signal to the speaking thread */
	ivona_speaking = 1;
	sem_post(ivona_semaphore);

	dbg("Ivona: leaving write() normally\n\r");
	return bytes;
}

int module_stop(void)
{
	int ret;
	dbg("ivona: stop()\n");

	ivona_stop = 1;
	if (module_audio_id) {
		dbg("Stopping audio");
		ret = opentts_audio_stop(module_audio_id);
		if (ret != 0)
			dbg("WARNING: Non 0 value from spd_audio_stop: %d",
			    ret);
	}

	return 0;
}

size_t module_pause(void)
{
	dbg("pause requested\n");
	if (ivona_speaking) {
		dbg("Ivona doesn't support pause, stopping\n");

		module_stop();

		return -1;
	} else {
		return 0;
	}
}

void module_close(int status)
{

	dbg("ivona: close()\n");

	dbg("Stopping speech");
	if (ivona_speaking) {
		module_stop();
	}

	dbg("Terminating threads");
	if (module_terminate_thread(ivona_speak_thread) != 0)
		exit(1);

	dbg("Closing audio output");
	opentts_audio_close(module_audio_id);

	exit(status);
}

/* Internal functions */

void *_ivona_speak(void *nothing)
{
	AudioTrack track;
	char *buf;
	int len;
	char *msg, *audio;
	char icon[64];
	int samples, offset;
	int fd;
	char *next_audio;
	int next_samples, next_offset;
	char next_icon[64];
	char next_cache[16];

	dbg("ivona: speaking thread starting.......\n");

	set_speaking_thread_parameters();

	while (1) {
		sem_wait(ivona_semaphore);
		dbg("Semaphore on\n");

		ivona_stop = 0;
		ivona_speaking = 1;

		opentts_audio_set_volume(module_audio_id, ivona_volume);

		module_report_event_begin();
		msg = *ivona_message;
		dbg("To say: %s\n", msg);
		buf = NULL;
		len = 0;
		fd = -1;
		audio = NULL;
		next_audio = NULL;
		next_icon[0] = 0;
		while (1) {
			if (ivona_stop) {
				dbg("Stop in child, terminating");
				ivona_speaking = 0;
				module_report_event_stop();
				break;
			}
			audio = NULL;
			if (next_audio) {
				audio = next_audio;
				samples = next_samples;
				offset = next_offset;
				strcpy(icon, next_icon);
				next_audio = NULL;
				dbg("Got wave from next_audio");
			} else if (fd >= 0) {
				audio =
				    ivona_get_wave_fd(fd, &samples, &offset);
				strcpy(icon, next_icon);
				if (audio && next_cache[0]) {
					ivona_store_wave_in_cache(next_cache,
								  audio +
								  2 * offset,
								  samples);
				}

				fd = -1;
				dbg("Got wave from fd");
			} else if (next_icon[0]) {
				strcpy(icon, next_icon);
				dbg("Got icon");
			}
			if (!audio && !icon[0]) {
				if (!msg || !*msg
				    || ivona_get_msgpart(ivona_conf,
							 ivona_message_type,
							 &msg, icon, &buf, &len,
							 ivona_cap_mode,
							 IvonaDelimiters,
							 ivona_punct_mode,
							 IvonaPunctuationSome))
				{
					ivona_speaking = 0;
					if (ivona_stop)
						module_report_event_stop();
					else
						module_report_event_end();
					break;
				}
				if (buf && *buf) {
					audio =
					    ivona_get_wave(buf, &samples,
							   &offset);
					dbg("Got wave from direct");
				}
			}

			/* tu mamy audio albo icon, mozna gadac */
			if (ivona_stop) {
				dbg("Stop in child, terminating");
				ivona_speaking = 0;
				module_report_event_stop();
				break;
			}

			next_icon[0] = 0;
			if (msg && *msg) {
				if (!ivona_get_msgpart
				    (ivona_conf, ivona_message_type, &msg,
				     next_icon, &buf, &len, ivona_cap_mode,
				     IvonaDelimiters, ivona_punct_mode,
				     IvonaPunctuationSome)) {
					if (buf && *buf) {
						next_offset = 0;
						next_audio =
						    ivona_get_wave_from_cache
						    (buf, &next_samples);
						if (!next_audio) {
							dbg("Sending %s to ivona", buf);
							next_cache[0] = 0;
							if (strlen(buf) <=
							    IVONA_CACHE_MAX_STRLEN)
								strcpy
								    (next_cache,
								     buf);
							fd = ivona_send_string
							    (buf);
						}
					}
				}
			}
			if (ivona_stop) {
				dbg("Stop in child, terminating");
				ivona_speaking = 0;
				module_report_event_stop();
				break;
			}
			if (icon[0]) {
				play_icon(IvonaSoundIconPath, icon);
				if (ivona_stop) {
					ivona_speaking = 0;
					module_report_event_stop();
					break;
				}
				icon[0] = 0;
			}
			if (audio) {
				track.num_samples = samples;
				track.num_channels = 1;
				track.sample_rate = IvonaSampleFreq;
				track.bits = 16;
				track.samples = ((short *)audio) + offset;
				dbg("Got %d samples", track.num_samples);
				opentts_audio_play(module_audio_id, track,
					       SPD_AUDIO_LE);
				g_free(audio);
				audio = NULL;
			}
			if (ivona_stop) {
				ivona_speaking = 0;
				module_report_event_stop();
				break;
			}
		}
		ivona_stop = 0;
		g_free(buf);
		g_free(audio);
		g_free(next_audio);
		if (fd >= 0)
			close(fd);
		fd = -1;
		audio = NULL;
		next_audio = NULL;
	}
	ivona_speaking = 0;

	dbg("Ivona: speaking thread ended.......\n");

	pthread_exit(NULL);
}

static void ivona_set_volume(signed int volume)
{
	assert(volume >= -100 && volume <= +100);
	ivona_volume = volume;
}

static void ivona_set_cap_let_recogn(SPDCapitalLetters cap_mode)
{
	ivona_cap_mode = 0;
	switch (cap_mode) {
	case SPD_CAP_NONE:
		ivona_cap_mode = 0;
		break;
	case SPD_CAP_ICON:
		ivona_cap_mode = 1;
		break;
	case SPD_CAP_SPELL:
		ivona_cap_mode = 2;
		break;
	case SPD_CAP_ERR:
		/* Do nothing.  This code should never be reached. */
		break;
	}
	if (ivona_cap_mode < IvonaMinCapLet) {
		ivona_cap_mode = IvonaMinCapLet;
	}
}

static void ivona_set_punctuation_mode(SPDPunctuation punct_mode)
{
	ivona_punct_mode = 1;
	switch (punct_mode) {
	case SPD_PUNCT_NONE:
		ivona_punct_mode = 0;
		break;
	case SPD_PUNCT_SOME:
		ivona_punct_mode = 1;
		break;
	case SPD_PUNCT_ALL:
		ivona_punct_mode = 2;
		break;
	case SPD_PUNCT_ERR:
		/* Do nothing.  This code should never be reached. */
		break;
	}
}
