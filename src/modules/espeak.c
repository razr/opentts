
/*
 * espeak.c - OpenTTS backend for espeak
 *
 * Copyright (C) 2007 Brailcom, o.p.s.
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
 * @author Lukas Loehrer
 * Based on ibmtts.c.
 *
 */

/* < Includes*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System includes. */
#include <string.h>
#include <glib.h>

/* espeak header file */
#include <espeak/speak_lib.h>
#ifndef ESPEAK_API_REVISION
#define ESPEAK_API_REVISION 1
#endif

/* OpenTTS includes. */
#include "opentts/opentts_types.h"
#include "opentts/opentts_synth_plugin.h"
#include "audio.h"
#include "module_utils.h"
#include<logging.h>

#if HAVE_SNDFILE
#include <sndfile.h>
#endif

/* > */
/* < Basic definitions*/

#define MODULE_NAME     "espeak"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG()
#define DBG_WARN(e, msg)						\
	if ( !(e)) {						\
		log_msg(OTTS_LOG_WARN, "Espeak: Warning:  " msg);			\
	}
typedef enum {
	FATAL_ERROR = -1,
	OK = 0,
	ERROR = 1
} TEspeakSuccess;

typedef enum {
	IDLE,
	BEFORE_SYNTH,
	BEFORE_PLAY,
	SPEAKING
} TEspeakState;

typedef enum {
	ESPEAK_PAUSE_OFF,
	ESPEAK_PAUSE_REQUESTED,
	ESPEAK_PAUSE_MARK_REPORTED
} TEspeakPauseState;

/* > */
/* < Thread and process control. */

static TEspeakState espeak_state = IDLE;
static pthread_mutex_t espeak_state_mutex;

static pthread_t espeak_play_thread;
static pthread_t espeak_stop_or_pause_thread;

static sem_t *espeak_stop_or_pause_semaphore;
static pthread_mutex_t espeak_stop_or_pause_suspended_mutex;
static sem_t *espeak_play_semaphore;
static pthread_mutex_t espeak_play_suspended_mutex;

static gboolean espeak_close_requested = FALSE;
static TEspeakPauseState espeak_pause_state = ESPEAK_PAUSE_OFF;
static gboolean espeak_stop_requested = FALSE;

/* > */

static int espeak_sample_rate = 0;
static SPDVoice **espeak_voice_list = NULL;

/* < The playback queue. */

typedef enum {
	ESPEAK_QET_AUDIO,	/* Chunk of audio. */
	ESPEAK_QET_INDEX_MARK,	/* Index mark event. */
	ESPEAK_QET_SOUND_ICON,	/* A Sound Icon */
	ESPEAK_QET_BEGIN,	/* Beginning of speech. */
	ESPEAK_QET_END		/* Speech completed. */
} EPlaybackQueueEntryType;

typedef struct {
	long num_samples;
	short *audio_chunk;
} TPlaybackQueueAudioChunk;

typedef struct {
	EPlaybackQueueEntryType type;
	union {
		char *markId;
		TPlaybackQueueAudioChunk audio;
		char *sound_icon_filename;
	} data;
} TPlaybackQueueEntry;

static GSList *playback_queue = NULL;
static int playback_queue_size = 0;	/* Number of audio frames currently in queue */
static pthread_mutex_t playback_queue_mutex;
pthread_cond_t playback_queue_condition;

/* When a voice is set, this is the baseline pitch of the voice.
   SSIP PITCH commands then adjust relative to this. */
static int espeak_voice_pitch_baseline = 50;

/* <Function prototypes*/

static void espeak_state_reset();
static TEspeakSuccess espeak_set_punctuation_list_from_utf8(const char *punct);
static SPDVoice **espeak_list_synthesis_voices();
static void espeak_free_voice_list();

/* Callbacks */
static int synth_callback(short *wav, int numsamples, espeak_EVENT * events);
static int uri_callback(int type, const char *uri, const char *base);

/* Internal function prototypes for main thread. */

/* Basic parameters */
static void espeak_set_rate(signed int rate);
static void espeak_set_pitch(signed int pitch);
static void espeak_set_volume(signed int volume);
static void espeak_set_punctuation_mode(SPDPunctuation punct_mode);
static void espeak_set_cap_let_recogn(SPDCapitalLetters cap_mode);

#if 0
static void espeak_set_pitch_range(signed int pitch_range);
#endif

/* Voices and languages */
static void espeak_set_language(char *lang);
static void espeak_set_voice(SPDVoiceType voice);
static void espeak_set_language_and_voice(char *lang, SPDVoiceType voice);
static void espeak_set_synthesis_voice(char *);

/* Internal function prototypes for playback thread. */
static gboolean espeak_add_sound_icon_to_playback_queue(const char *filename);
static gboolean espeak_add_audio_to_playback_queue(short *audio_chunk,
						   int num_samples);
static gboolean espeak_add_mark_to_playback_queue(const char *markId);
static gboolean espeak_add_flag_to_playback_queue(EPlaybackQueueEntryType type);
static void espeak_delete_playback_queue_entry(TPlaybackQueueEntry *
					       playback_queue_entry);
static gboolean espeak_send_to_audio(TPlaybackQueueEntry *
				     playback_queue_entry);
static gboolean espeak_play_file(char *filename);

/* Miscellaneous internal function prototypes. */
static gboolean is_thread_busy(pthread_mutex_t * suspended_mutex);
static void espeak_clear_playback_queue();

/* The playback thread start routine. */
static void *_espeak_play(void *);
/* The stop_or_pause start routine. */
static void *_espeak_stop_or_pause(void *);

/* > */
/* < Module configuration options*/

MOD_OPTION_1_INT(EspeakPitchRange)
    MOD_OPTION_1_STR(EspeakPunctuationList)
    MOD_OPTION_1_INT(EspeakCapitalPitchRise)

    MOD_OPTION_1_INT(EspeakAudioChunkSize)
    MOD_OPTION_1_INT(EspeakAudioQueueMaxSize)
    MOD_OPTION_1_STR(EspeakSoundIconFolder)
    MOD_OPTION_1_INT(EspeakSoundIconVolume)

/* > */
/* < plugin functions */
static int espeak_load(void)
{
	init_settings_tables();

	REGISTER_DEBUG();

	/* Options */
	MOD_OPTION_1_INT_REG(EspeakAudioChunkSize, 2000);
	MOD_OPTION_1_INT_REG(EspeakAudioQueueMaxSize, 20 * 22050);
	MOD_OPTION_1_STR_REG(EspeakSoundIconFolder,
			     "/usr/share/sounds/sound-icons/");
	MOD_OPTION_1_INT_REG(EspeakSoundIconVolume, 0);

	MOD_OPTION_1_INT_REG(EspeakPitchRange, 0);
	MOD_OPTION_1_STR_REG(EspeakPunctuationList, "@/+-_");
	MOD_OPTION_1_INT_REG(EspeakCapitalPitchRise, 800);
	if (EspeakCapitalPitchRise == 1 || EspeakCapitalPitchRise == 2) {
		EspeakCapitalPitchRise = 0;
	}

	module_audio_id = NULL;

	return OK;
}

#define ABORT(msg)								\
	g_string_append(info, msg);					\
	log_msg(OTTS_LOG_CRIT, "FATAL ERROR: %s", info->str);			\
	*status_info = info->str;					\
	g_string_free(info, FALSE);					\
	return FATAL_ERROR;

static int espeak_init(char **status_info)
{
	int ret;
	const char *espeak_version;
	GString *info;

	log_msg(OTTS_LOG_NOTICE, "Espeak: Module init().");
	/* Make sure the glib functions are thread safe. */
	if (!g_thread_supported())
		g_thread_init(NULL);

	*status_info = NULL;
	info = g_string_new("");

	/* Report versions. */
	espeak_version = espeak_Info(NULL);
	log_msg(OTTS_LOG_INFO,
		"Espeak: espeak Output Module version %s, espeak Engine version %s",
		MODULE_VERSION, espeak_version);

	/* <Espeak setup */

	log_msg(OTTS_LOG_INFO,
		"Espeak: Initializing engine with buffer size %d ms.",
		EspeakAudioChunkSize);
#if ESPEAK_API_REVISION == 1
	espeak_sample_rate =
	    espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, EspeakAudioChunkSize,
			      NULL);
#else
	espeak_sample_rate =
	    espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, EspeakAudioChunkSize,
			      NULL, 0);
#endif
	if (espeak_sample_rate == EE_INTERNAL_ERROR) {
		log_msg(OTTS_LOG_CRIT, "Espeak: Could not initialize engine.");
		*status_info = g_strdup("Could not initialize engine. ");
		return FATAL_ERROR;
	}

	log_msg(OTTS_LOG_INFO, "Espeak: Registering callbacks.");
	espeak_SetSynthCallback(synth_callback);
	espeak_SetUriCallback(uri_callback);

	log_msg(OTTS_LOG_DEBUG,
		"Setting up espeak specific configuration settings.");
	ret = espeak_set_punctuation_list_from_utf8(EspeakPunctuationList);
	if (ret != OK)
		log_msg(OTTS_LOG_WARN,
			"Espeak: Failed to set punctuation list.");

	espeak_voice_list = espeak_list_synthesis_voices();

	/* Reset global state */
	espeak_state_reset();

	/* <Threading setup */

	/* These mutexes are locked when the corresponding threads are suspended. */
	pthread_mutex_init(&espeak_stop_or_pause_suspended_mutex, NULL);
	pthread_mutex_init(&espeak_play_suspended_mutex, NULL);

	/* This mutex mediates access to the playback queue between the espeak synthesis thread andthe the playback thread. */
	pthread_mutex_init(&playback_queue_mutex, NULL);
	pthread_cond_init(&playback_queue_condition, NULL);

	/* The following mutex protects access to various flags */
	pthread_mutex_init(&espeak_state_mutex, NULL);

	log_msg(OTTS_LOG_NOTICE,
		"Espeak: Creating new thread for stop or pause.");
	espeak_stop_or_pause_semaphore = module_semaphore_init();
	ret =
	    pthread_create(&espeak_stop_or_pause_thread, NULL,
			   _espeak_stop_or_pause, NULL);
	if (0 != ret) {
		ABORT("Failed to create stop-or-pause thread.");
	}

	espeak_play_semaphore = module_semaphore_init();
	log_msg(OTTS_LOG_INFO, "Espeak: Creating new thread for playback.");
	ret = pthread_create(&espeak_play_thread, NULL, _espeak_play, NULL);
	if (ret != OK) {
		ABORT("Failed to create playback thread.");
	}

	*status_info = g_strdup("Espeak: Initialized successfully.");

	return OK;
}

#undef ABORT

static int espeak_audio_init(char **status_info)
{
	log_msg(OTTS_LOG_NOTICE, "Opening audio");
	return module_audio_init_spd(status_info);
}

static SPDVoice **espeak_list_voices(void)
{
	return espeak_voice_list;
}

static int espeak_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{
	espeak_ERROR result = EE_INTERNAL_ERROR;
	int flags = espeakSSML | espeakCHARS_UTF8;

	log_msg(OTTS_LOG_INFO, "Espeak: module_speak().");

	pthread_mutex_lock(&espeak_state_mutex);
	if (espeak_state != IDLE) {
		log_msg(OTTS_LOG_WARN,
			"Espeak: Warning, module_speak called when not ready.");
		pthread_mutex_unlock(&espeak_state_mutex);
		return FALSE;
	}

	log_msg(OTTS_LOG_DEBUG, "Espeak: Requested data: |%s| %d %ld", data,
		msgtype, bytes);

	espeak_state_reset();
	espeak_state = BEFORE_SYNTH;

	/* Setting speech parameters. */
	UPDATE_STRING_PARAMETER(voice.language, espeak_set_language);
	UPDATE_PARAMETER(voice_type, espeak_set_voice);
	UPDATE_STRING_PARAMETER(voice.name, espeak_set_synthesis_voice);

	UPDATE_PARAMETER(rate, espeak_set_rate);
	UPDATE_PARAMETER(volume, espeak_set_volume);
	UPDATE_PARAMETER(pitch, espeak_set_pitch);
	UPDATE_PARAMETER(punctuation_mode, espeak_set_punctuation_mode);
	UPDATE_PARAMETER(cap_let_recogn, espeak_set_cap_let_recogn);

	/*
	   UPDATE_PARAMETER(spelling_mode, espeak_set_spelling_mode);
	 */
	/* Send data to espeak */
	switch (msgtype) {
	case SPD_MSGTYPE_TEXT:
		result = espeak_Synth(data, bytes + 1, 0, POS_CHARACTER, 0,
				      flags, NULL, NULL);
		break;
	case SPD_MSGTYPE_SOUND_ICON:
		{
			char *msg =
			    g_strdup_printf("<audio src=\"%s%s\">%s</audio>",
					    EspeakSoundIconFolder, data, data);
			result =
			    espeak_Synth(msg, strlen(msg) + 1, 0, POS_CHARACTER,
					 0, flags, NULL, NULL);
			g_free(msg);
			break;
		}
	case SPD_MSGTYPE_CHAR:
		{
			wchar_t wc = 0;
			if (bytes == 1) {	// ASCII
				wc = (wchar_t) data[0];
			} else if (bytes == 5
				   && (0 == strncmp(data, "space", bytes))) {
				wc = (wchar_t) 0x20;
			} else {
				gsize bytes_out;
				gchar *tmp =
				    g_convert(data, -1, "wchar_t", "utf-8",
					      NULL, &bytes_out, NULL);
				if (tmp != NULL && bytes_out == sizeof(wchar_t)) {
					wchar_t *wc_ptr = (wchar_t *) tmp;
					wc = wc_ptr[0];
				} else {
					log_msg(OTTS_LOG_NOTICE,
						"Espeak: Failed to convert utf-8 to wchar_t, or not exactly one utf-8 character given.");
				}
				g_free(tmp);
			}
			char *msg =
			    g_strdup_printf
			    ("<say-as interpret-as=\"tts:char\">&#%d;</say-as>",
			     wc);
			result =
			    espeak_Synth(msg, strlen(msg) + 1, 0, POS_CHARACTER,
					 0, flags, NULL, NULL);
			g_free(msg);
			break;
		}
	case SPD_MSGTYPE_KEY:
		{
			/* TODO: Convert unspeakable keys to speakable form */
			char *msg =
			    g_strdup_printf
			    ("<say-as interpret-as=\"tts:key\">%s</say-as>",
			     data);
			result =
			    espeak_Synth(msg, strlen(msg) + 1, 0, POS_CHARACTER,
					 0, flags, NULL, NULL);
			g_free(msg);
			break;
		}
	case SPD_MSGTYPE_SPELL:
		/* TODO: Not sure what to do here... */
		break;
	}

	if (result != EE_OK) {
		return FALSE;
	}

	pthread_mutex_unlock(&espeak_state_mutex);

	log_msg(OTTS_LOG_DEBUG, "Espeak: Leaving module_speak() normally.");
	return bytes;
}

static int espeak_stop(void)
{
	log_msg(OTTS_LOG_INFO, "Espeak: module_stop().");

	pthread_mutex_lock(&espeak_state_mutex);
	if (espeak_state != IDLE &&
	    !espeak_stop_requested &&
	    !is_thread_busy(&espeak_stop_or_pause_suspended_mutex)) {
		log_msg(OTTS_LOG_INFO, "Espeak: stopping...");
		espeak_stop_requested = TRUE;
		/* Wake the stop_or_pause thread. */
		sem_post(espeak_stop_or_pause_semaphore);
	} else {
		log_msg(OTTS_LOG_WARN, "Espeak: Cannot stop now.");
	}
	pthread_mutex_unlock(&espeak_state_mutex);

	return OK;
}

static size_t espeak_pause(void)
{
	log_msg(OTTS_LOG_INFO, "Espeak: module_pause().");
	pthread_mutex_lock(&espeak_state_mutex);
	if (espeak_pause_state == ESPEAK_PAUSE_OFF && !espeak_stop_requested) {
		espeak_pause_state = ESPEAK_PAUSE_REQUESTED;
	}
	pthread_mutex_unlock(&espeak_state_mutex);

	return OK;
}

static void espeak_close(int status)
{
	log_msg(OTTS_LOG_INFO, "Espeak: close().");

	log_msg(OTTS_LOG_NOTICE, "Espeak: Terminating threads");
	espeak_stop_requested = TRUE;
	espeak_close_requested = TRUE;

	pthread_mutex_lock(&playback_queue_mutex);
	pthread_cond_broadcast(&playback_queue_condition);
	pthread_mutex_unlock(&playback_queue_mutex);

	sem_post(espeak_play_semaphore);
	sem_post(espeak_stop_or_pause_semaphore);
	/* Give threads a chance to quit on their own terms. */
	g_usleep(25000);

	/* Make sure threads have really exited */
	pthread_cancel(espeak_play_thread);
	pthread_cancel(espeak_stop_or_pause_thread);

	log_msg(OTTS_LOG_INFO, "Joining  play thread.");
	pthread_join(espeak_play_thread, NULL);
	log_msg(OTTS_LOG_DEBUG, "Joininging stop thread.");
	pthread_join(espeak_stop_or_pause_thread, NULL);

	log_msg(OTTS_LOG_NOTICE, "Espeak: terminating synthesis.");
	espeak_Terminate();

	log_msg(OTTS_LOG_INFO, "Espeak: Closing audio output");
	if (module_audio_id) {
		opentts_audio_close(module_audio_id);
	}

	log_msg(OTTS_LOG_INFO, "Freeing resources.");
	espeak_clear_playback_queue();
	espeak_free_voice_list();

	pthread_mutex_destroy(&espeak_state_mutex);
	pthread_mutex_destroy(&espeak_play_suspended_mutex);
	pthread_mutex_destroy(&espeak_stop_or_pause_suspended_mutex);
	pthread_mutex_destroy(&playback_queue_mutex);
	pthread_cond_destroy(&playback_queue_condition);
	sem_destroy(espeak_play_semaphore);
	sem_destroy(espeak_stop_or_pause_semaphore);

	exit(status);
}

/* > */
/* < Internal functions */
/* Return true if the thread is busy, i.e., suspended mutex is not locked. */
static gboolean is_thread_busy(pthread_mutex_t * suspended_mutex)
{
	if (EBUSY == pthread_mutex_trylock(suspended_mutex))
		return FALSE;
	else {
		pthread_mutex_unlock(suspended_mutex);
		return TRUE;
	}
}

static void espeak_state_reset()
{
	espeak_state = IDLE;
	espeak_pause_state = ESPEAK_PAUSE_OFF;
	espeak_stop_requested = FALSE;
}

/* Stop or Pause thread. */
static void *_espeak_stop_or_pause(void *nothing)
{
	int ret;

	log_msg(OTTS_LOG_INFO, "Espeak: Stop or pause thread starting.......");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	while (!espeak_close_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(espeak_stop_or_pause_semaphore)) {
			pthread_mutex_lock
			    (&espeak_stop_or_pause_suspended_mutex);
			sem_wait(espeak_stop_or_pause_semaphore);
			pthread_mutex_unlock
			    (&espeak_stop_or_pause_suspended_mutex);
		}
		log_msg(OTTS_LOG_INFO, "Espeak: Stop or pause semaphore on.");
		if (espeak_close_requested)
			break;
		if (!espeak_stop_requested) {
			/* This sometimes happens after wake-up from suspend-to-disk.  */
			log_msg(OTTS_LOG_WARN,
				"Espeak: Warning: spurious wake-up  of stop thread.");
			continue;
		}

		pthread_mutex_lock(&playback_queue_mutex);
		pthread_cond_broadcast(&playback_queue_condition);
		pthread_mutex_unlock(&playback_queue_mutex);

		if (module_audio_id) {
			log_msg(OTTS_LOG_WARN, "Espeak: Stopping audio.");
			ret = opentts_audio_stop(module_audio_id);
			DBG_WARN(ret == 0,
				 "spd_audio_stop returned non-zero value.");
			while (is_thread_busy(&espeak_play_suspended_mutex)) {
				ret = opentts_audio_stop(module_audio_id);
				DBG_WARN(ret == 0,
					 "spd_audio_stop returned non-zero value.");
				g_usleep(5000);
			}
		} else {
			while (is_thread_busy(&espeak_play_suspended_mutex)) {
				g_usleep(5000);
			}
		}

		log_msg(OTTS_LOG_INFO,
			"Espeak: Waiting for synthesis to stop.");
		ret = espeak_Cancel();
		DBG_WARN(ret == EE_OK, "Espeak: error in espeak_Cancel().");

		log_msg(OTTS_LOG_DEBUG, "Espeak: Clearing playback queue.");
		espeak_clear_playback_queue();

		int save_pause_state = espeak_pause_state;
		pthread_mutex_lock(&espeak_state_mutex);
		espeak_state_reset();
		pthread_mutex_unlock(&espeak_state_mutex);

		if (save_pause_state == ESPEAK_PAUSE_MARK_REPORTED) {
			module_report_event_pause();
		} else {
			module_report_event_stop();
		}

		log_msg(OTTS_LOG_INFO,
			"Espeak: Stop or pause thread ended.......\n");
	}
	pthread_exit(NULL);
}

static void espeak_set_rate(signed int rate)
{
	int speed;
	int normal_rate = 170, max_rate = 390, min_rate = 80;

	assert(rate >= OTTS_VOICE_RATE_MIN && rate <= OTTS_VOICE_RATE_MAX);
	if (rate < OTTS_VOICE_RATE_DEFAULT)
		speed = min_rate + ((normal_rate - min_rate)
				    * (rate - OTTS_VOICE_RATE_MIN)
				    / (OTTS_VOICE_RATE_DEFAULT -
				       OTTS_VOICE_RATE_MIN));
	else
		speed = normal_rate + ((max_rate - normal_rate)
				       * (rate - OTTS_VOICE_RATE_DEFAULT)
				       / (OTTS_VOICE_RATE_MAX -
					  OTTS_VOICE_RATE_DEFAULT));

	espeak_ERROR ret = espeak_SetParameter(espeakRATE, speed, 0);
	if (ret != EE_OK) {
		log_msg(OTTS_LOG_WARN, "Espeak: Error setting rate %i.", speed);
	} else {
		log_msg(OTTS_LOG_DEBUG, "Espeak: Rate set to %i.", speed);
	}
}

static void espeak_set_volume(signed int volume)
{
	int vol;

	assert(volume >= OTTS_VOICE_VOLUME_MIN
	       && volume <= OTTS_VOICE_VOLUME_MAX);

	/* Possible range 0 to 100. */
	if (volume < OTTS_VOICE_VOLUME_DEFAULT)
		vol = (volume - OTTS_VOICE_VOLUME_MIN) * 100 /
		    (OTTS_VOICE_VOLUME_MAX - OTTS_VOICE_VOLUME_MIN);
	else
		vol = 50 + ((volume - OTTS_VOICE_VOLUME_DEFAULT) * 100 /
			    (OTTS_VOICE_VOLUME_MAX - OTTS_VOICE_VOLUME_MIN));

	espeak_ERROR ret = espeak_SetParameter(espeakVOLUME, vol, 0);
	if (ret != EE_OK) {
		log_msg(OTTS_LOG_WARN, "Espeak: Error setting volume %i.", vol);
	} else {
		log_msg(OTTS_LOG_DEBUG, "Espeak: Volume set to %i.", vol);
	}
}

static void espeak_set_pitch(signed int pitch)
{
	int pitchBaseline;

	assert(pitch >= OTTS_VOICE_PITCH_MIN && pitch <= OTTS_VOICE_PITCH_MAX);

	/* Possible range 0 to 100. */
	if (pitch < OTTS_VOICE_PITCH_DEFAULT) {
		pitchBaseline =
		    ((OTTS_VOICE_PITCH_DEFAULT -
		      pitch) * espeak_voice_pitch_baseline) /
		    (OTTS_VOICE_PITCH_DEFAULT - OTTS_VOICE_PITCH_MIN);
	} else {
		pitchBaseline = espeak_voice_pitch_baseline
		    +
		    (((pitch - OTTS_VOICE_PITCH_DEFAULT) * (100 -
							    espeak_voice_pitch_baseline))
		     / (OTTS_VOICE_PITCH_MAX - OTTS_VOICE_PITCH_DEFAULT));
	}

	espeak_ERROR ret = espeak_SetParameter(espeakPITCH, pitchBaseline, 0);
	if (ret != EE_OK) {
		log_msg(OTTS_LOG_WARN, "Espeak: Error setting pitch %i.",
			pitchBaseline);
	} else {
		log_msg(OTTS_LOG_DEBUG, "Espeak: Pitch set to %i.",
			pitchBaseline);
	}
}

static void espeak_set_punctuation_mode(SPDPunctuation punct_mode)
{
	espeak_PUNCT_TYPE espeak_punct_mode = espeakPUNCT_SOME;

	switch (punct_mode) {
	case SPD_PUNCT_ALL:
		espeak_punct_mode = espeakPUNCT_ALL;
		break;
	case SPD_PUNCT_SOME:
		espeak_punct_mode = espeakPUNCT_SOME;
		break;
	case SPD_PUNCT_NONE:
		espeak_punct_mode = espeakPUNCT_NONE;
		break;
	case SPD_PUNCT_ERR:
		/* Do nothing.  This code should never be reached. */
		break;
	}

	espeak_ERROR ret =
	    espeak_SetParameter(espeakPUNCTUATION, espeak_punct_mode, 0);
	if (ret != EE_OK) {
		log_msg(OTTS_LOG_WARN,
			"Espeak: Failed to set punctuation mode.");
	} else {
		log_msg(OTTS_LOG_INFO, "Set punctuation mode.");
	}
}

static void espeak_set_cap_let_recogn(SPDCapitalLetters cap_mode)
{
	int espeak_cap_mode = 0;
	switch (cap_mode) {
	case SPD_CAP_NONE:
		espeak_cap_mode = EspeakCapitalPitchRise;
		break;
	case SPD_CAP_SPELL:
		espeak_cap_mode = 2;
		break;
	case SPD_CAP_ICON:
		espeak_cap_mode = 1;
		break;
	case SPD_CAP_ERR:
		/* Not reached.  Do nothing. */
		break;
	}

	espeak_ERROR ret =
	    espeak_SetParameter(espeakCAPITALS, espeak_cap_mode, 1);
	if (ret != EE_OK) {
		log_msg(OTTS_LOG_WARN, "Espeak: Failed to set capitals mode.");
	} else {
		log_msg(OTTS_LOG_INFO, "Set capitals mode.");
	}
}

/* Given a language code and SD voice code, sets the espeak voice. */
static void espeak_set_language_and_voice(char *lang, SPDVoiceType voice_code)
{
	log_msg(OTTS_LOG_DEBUG, "Espeak: set_language_and_voice %s %d", lang,
		voice_code);
	espeak_ERROR ret;

	unsigned char overlay = 0;
	switch (voice_code) {
	case SPD_NO_VOICE:
		overlay = 0;
		break;
	case SPD_MALE1:
		overlay = 0;
		break;
	case SPD_MALE2:
		overlay = 1;
		break;
	case SPD_MALE3:
		overlay = 2;
		break;
	case SPD_FEMALE1:
		overlay = 11;
		break;
	case SPD_FEMALE2:
		overlay = 12;
		break;
	case SPD_FEMALE3:
		overlay = 13;
		break;
	case SPD_CHILD_MALE:
		overlay = 4;
		break;
	case SPD_CHILD_FEMALE:
		overlay = 14;
		break;
	}

	char *name = g_strdup_printf("%s+%d", lang, overlay);
	log_msg(OTTS_LOG_INFO, "Espeak: set_language_and_voice name=%s", name);
	ret = espeak_SetVoiceByName(name);

	if (ret != EE_OK) {
		log_msg(OTTS_LOG_WARN, "Espeak: Error selecting language %s",
			name);
	} else {
		log_msg(OTTS_LOG_DEBUG,
			"Espeak: Successfully set voice to \"%s\"", name);
	}
	g_free(name);
}

static void espeak_set_voice(SPDVoiceType voice)
{
	assert(msg_settings.voice.language);
	espeak_set_language_and_voice(msg_settings.voice.language, voice);
}

static void espeak_set_language(char *lang)
{
	espeak_set_language_and_voice(lang, msg_settings.voice_type);
}

static void espeak_set_synthesis_voice(char *synthesis_voice)
{
	if (synthesis_voice != NULL) {
		espeak_ERROR ret = espeak_SetVoiceByName(synthesis_voice);
		if (ret != EE_OK) {
			log_msg(OTTS_LOG_WARN,
				"Espeak: Failed to set synthesis voice to %s.",
				synthesis_voice);
		}
	}
}

/* Callbacks */

static gboolean espeak_send_audio_upto(short *wav, int *sent, int upto)
{
	assert(*sent >= 0);
	assert(upto >= 0);
	int numsamples = upto - (*sent);
	if (wav == NULL || numsamples == 0) {
		return TRUE;
	}
	short *start = wav + (*sent);
	gboolean result = espeak_add_audio_to_playback_queue(start, numsamples);
	*sent = upto;
	return result;
}

static int synth_callback(short *wav, int numsamples, espeak_EVENT * events)
{
	/* Number of samples sent in current message. */
	static int numsamples_sent_msg = 0;
	/* Number of samples already sent during this call to the callback. */
	int numsamples_sent = 0;
	gboolean result = FALSE;

	pthread_mutex_lock(&espeak_state_mutex);
	if (espeak_state == BEFORE_SYNTH) {
		numsamples_sent_msg = 0;
		espeak_state = BEFORE_PLAY;
		espeak_add_flag_to_playback_queue(ESPEAK_QET_BEGIN);
		/* Wake up playback thread */
		sem_post(espeak_play_semaphore);
	}
	pthread_mutex_unlock(&espeak_state_mutex);

	if (espeak_stop_requested) {
		return 1;
	}

	/* Process events and audio data */
	while (events->type != espeakEVENT_LIST_TERMINATED) {
		/* Enqueue audio upto event */
		switch (events->type) {
		case espeakEVENT_MARK:
		case espeakEVENT_PLAY:
			{
				/* Convert ms position to samples */
				gint64 pos_msg = events->audio_position;
				pos_msg = pos_msg * espeak_sample_rate / 1000;
				/* Convert position in message to position in current chunk */
				int upto =
				    (int)CLAMP(pos_msg - numsamples_sent_msg,
					       0, numsamples);	/* This is just for safety */
				espeak_send_audio_upto(wav, &numsamples_sent,
						       upto);
				break;
			}
		default:
			break;
		}
		/* Process actual event */
		switch (events->type) {
		case espeakEVENT_MARK:
			result =
			    espeak_add_mark_to_playback_queue(events->id.name);
			break;
		case espeakEVENT_PLAY:
			result =
			    espeak_add_sound_icon_to_playback_queue(events->
								    id.name);
			break;
		case espeakEVENT_MSG_TERMINATED:
			// This event never has any audio in the same callback
			result =
			    espeak_add_flag_to_playback_queue(ESPEAK_QET_END);
			break;
		default:
			break;
		}
		if (espeak_stop_requested) {
			return 1;
		}
		events++;
	}
	espeak_send_audio_upto(wav, &numsamples_sent, numsamples);
	numsamples_sent_msg += numsamples;
	return 0;
}

static int uri_callback(int type, const char *uri, const char *base)
{
	int result = 1;
	if (type == 1) {
		/* Audio icon */
#if HAVE_SNDFILE
		if (g_file_test(uri, G_FILE_TEST_EXISTS)) {
			result = 0;
		}
#endif
	}
	return result;
}

static TPlaybackQueueEntry *playback_queue_pop()
{
	TPlaybackQueueEntry *result = NULL;
	pthread_mutex_lock(&playback_queue_mutex);
	while (!espeak_stop_requested && playback_queue == NULL) {
		pthread_cond_wait(&playback_queue_condition,
				  &playback_queue_mutex);
	}
	if (!espeak_stop_requested) {
		result = (TPlaybackQueueEntry *) playback_queue->data;
		playback_queue =
		    g_slist_remove(playback_queue, playback_queue->data);
		if (result->type == ESPEAK_QET_AUDIO) {
			playback_queue_size -= result->data.audio.num_samples;
			pthread_cond_signal(&playback_queue_condition);
		}
	}
	pthread_mutex_unlock(&playback_queue_mutex);
	return result;
}

static gboolean playback_queue_push(TPlaybackQueueEntry * entry)
{
	pthread_mutex_lock(&playback_queue_mutex);
	playback_queue = g_slist_append(playback_queue, entry);
	if (entry->type == ESPEAK_QET_AUDIO) {
		playback_queue_size += entry->data.audio.num_samples;
	}
	pthread_cond_signal(&playback_queue_condition);
	pthread_mutex_unlock(&playback_queue_mutex);
	return TRUE;
}

/* Adds a chunk of pcm audio to the audio playback queue.
   Waits until there is enough space in the queue. */
static gboolean
espeak_add_audio_to_playback_queue(short *audio_chunk, int num_samples)
{
	pthread_mutex_lock(&playback_queue_mutex);
	while (!espeak_stop_requested
	       && playback_queue_size > EspeakAudioQueueMaxSize) {
		pthread_cond_wait(&playback_queue_condition,
				  &playback_queue_mutex);
	}
	pthread_mutex_unlock(&playback_queue_mutex);
	if (espeak_stop_requested) {
		return FALSE;
	}

	TPlaybackQueueEntry *playback_queue_entry =
	    g_new(TPlaybackQueueEntry, 1);

	playback_queue_entry->type = ESPEAK_QET_AUDIO;
	playback_queue_entry->data.audio.num_samples = num_samples;
	gint nbytes = sizeof(short) * num_samples;
	playback_queue_entry->data.audio.audio_chunk =
	    (short *)g_memdup((gconstpointer) audio_chunk, nbytes);

	playback_queue_push(playback_queue_entry);
	return TRUE;
}

/* Adds an Index Mark to the audio playback queue. */
static gboolean espeak_add_mark_to_playback_queue(const char *markId)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));

	playback_queue_entry->type = ESPEAK_QET_INDEX_MARK;
	playback_queue_entry->data.markId = g_strdup(markId);
	return playback_queue_push(playback_queue_entry);
}

/* Adds a begin or end flag to the playback queue. */
static gboolean espeak_add_flag_to_playback_queue(EPlaybackQueueEntryType type)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));

	playback_queue_entry->type = type;
	return playback_queue_push(playback_queue_entry);
}

/* Add a sound icon to the playback queue. */
static gboolean espeak_add_sound_icon_to_playback_queue(const char *filename)
{
	TPlaybackQueueEntry *playback_queue_entry =
	    (TPlaybackQueueEntry *) g_malloc(sizeof(TPlaybackQueueEntry));

	playback_queue_entry->type = ESPEAK_QET_SOUND_ICON;
	playback_queue_entry->data.sound_icon_filename = g_strdup(filename);
	return playback_queue_push(playback_queue_entry);
}

/* Deletes an entry from the playback audio queue, freeing memory. */
static void
espeak_delete_playback_queue_entry(TPlaybackQueueEntry * playback_queue_entry)
{
	switch (playback_queue_entry->type) {
	case ESPEAK_QET_AUDIO:
		g_free(playback_queue_entry->data.audio.audio_chunk);
		break;
	case ESPEAK_QET_INDEX_MARK:
		g_free(playback_queue_entry->data.markId);
		break;
	case ESPEAK_QET_SOUND_ICON:
		g_free(playback_queue_entry->data.sound_icon_filename);
		break;
	default:
		break;
	}
	g_free(playback_queue_entry);
}

/* Erases the entire playback queue, freeing memory. */
static void espeak_clear_playback_queue()
{
	pthread_mutex_lock(&playback_queue_mutex);

	while (NULL != playback_queue) {
		TPlaybackQueueEntry *playback_queue_entry =
		    playback_queue->data;
		espeak_delete_playback_queue_entry(playback_queue_entry);
		playback_queue =
		    g_slist_remove(playback_queue, playback_queue->data);
	}
	playback_queue = NULL;
	playback_queue_size = 0;
	pthread_mutex_unlock(&playback_queue_mutex);
}

/* Sends a chunk of audio to the audio player and waits for completion or error. */
static gboolean espeak_send_to_audio(TPlaybackQueueEntry * playback_queue_entry)
{
	int ret = 0;
	AudioTrack track;
	track.num_samples = playback_queue_entry->data.audio.num_samples;
	track.num_channels = 1;
	track.sample_rate = espeak_sample_rate;
	track.bits = 16;
	track.samples = playback_queue_entry->data.audio.audio_chunk;

	log_msg(OTTS_LOG_DEBUG, "Espeak: Sending %i samples to audio.",
		track.num_samples);
	/* Volume is controlled by the synthesizer.  Always play at normal on audio device. */
	opentts_audio_set_volume(module_audio_id, 85);
	ret = opentts_audio_play(module_audio_id, track, SPD_AUDIO_LE);
	if (ret < 0) {
		log_msg(OTTS_LOG_ERR,
			"ERROR: Can't play track for unknown reason.");
		return FALSE;
	}
	log_msg(OTTS_LOG_DEBUG, "Espeak: Sent to audio.");
	return TRUE;
}

/* Playback thread. */
static void *_espeak_play(void *nothing)
{
	char *markId;
	TPlaybackQueueEntry *playback_queue_entry = NULL;

	log_msg(OTTS_LOG_NOTICE, "Espeak: Playback thread starting.......");

	/* Block all signals to this thread. */
	set_speaking_thread_parameters();

	while (!espeak_close_requested) {
		/* If semaphore not set, set suspended lock and suspend until it is signaled. */
		if (0 != sem_trywait(espeak_play_semaphore)) {
			pthread_mutex_lock(&espeak_play_suspended_mutex);
			sem_wait(espeak_play_semaphore);
			pthread_mutex_unlock(&espeak_play_suspended_mutex);
		}
		log_msg(OTTS_LOG_INFO, "Espeak: Playback semaphore on.");
		if (espeak_close_requested)
			break;
		if (espeak_state < BEFORE_PLAY) {
			/* This can happen after wake-up  from suspend-to-disk */
			log_msg(OTTS_LOG_NOTICE,
				"Espeak: Warning: Spurious wake of of playback thread.");
			continue;
		}

		while (1) {
			gboolean finished = FALSE;
			playback_queue_entry = playback_queue_pop();
			if (playback_queue_entry == NULL) {
				log_msg(OTTS_LOG_INFO,
					"Espeak: playback thread detected stop.");
				break;
			}

			switch (playback_queue_entry->type) {
			case ESPEAK_QET_AUDIO:
				espeak_send_to_audio(playback_queue_entry);
				break;
			case ESPEAK_QET_INDEX_MARK:
				markId = playback_queue_entry->data.markId;
				log_msg(OTTS_LOG_DEBUG,
					"Espeak: reporting index mark |%s|.",
					markId);
				module_report_index_mark(markId);
				log_msg(OTTS_LOG_DEBUG,
					"Espeak: index mark reported.");
				pthread_mutex_lock(&espeak_state_mutex);
				if (espeak_state == SPEAKING
				    && espeak_pause_state ==
				    ESPEAK_PAUSE_REQUESTED
				    &&
				    !is_thread_busy
				    (&espeak_stop_or_pause_suspended_mutex)
				    && g_str_has_prefix(markId, "__spd_")) {
					log_msg(OTTS_LOG_INFO,
						"Espeak: Pause requested in playback thread.  Stopping.");
					espeak_stop_requested = TRUE;
					espeak_pause_state =
					    ESPEAK_PAUSE_MARK_REPORTED;
					sem_post
					    (espeak_stop_or_pause_semaphore);
					finished = TRUE;
				}
				pthread_mutex_unlock(&espeak_state_mutex);
				break;
			case ESPEAK_QET_SOUND_ICON:
				espeak_play_file(playback_queue_entry->
						 data.sound_icon_filename);
				break;
			case ESPEAK_QET_BEGIN:
				{
					gboolean report_begin = FALSE;
					pthread_mutex_lock(&espeak_state_mutex);
					if (espeak_state == BEFORE_PLAY) {
						espeak_state = SPEAKING;
						report_begin = TRUE;
					}
					pthread_mutex_unlock
					    (&espeak_state_mutex);
					if (report_begin)
						module_report_event_begin();
					break;
				}
			case ESPEAK_QET_END:
				pthread_mutex_lock(&espeak_state_mutex);
				log_msg(OTTS_LOG_INFO,
					"Espeak: playback thread got END from queue.");
				if (espeak_state == SPEAKING) {
					if (!espeak_stop_requested) {
						log_msg(OTTS_LOG_NOTICE,
							"Espeak: playback thread reporting end.");
						espeak_state = IDLE;
						espeak_pause_state =
						    ESPEAK_PAUSE_OFF;
					}
					finished = TRUE;
				}
				pthread_mutex_unlock(&espeak_state_mutex);
				if (finished)
					module_report_event_end();
				break;
			}

			espeak_delete_playback_queue_entry
			    (playback_queue_entry);
			if (finished)
				break;
		}
	}
	log_msg(OTTS_LOG_NOTICE, "Espeak: Playback thread ended.......");
	return 0;
}

/* Plays the specified audio file. */
static gboolean espeak_play_file(char *filename)
{
	gboolean result = TRUE;
#if HAVE_SNDFILE
	int subformat;
	sf_count_t items;
	sf_count_t readcount;
	SNDFILE *sf;
	SF_INFO sfinfo;

	log_msg(OTTS_LOG_INFO, "Espeak: Playing |%s|", filename);
	memset(&sfinfo, 0, sizeof(sfinfo));
	sf = sf_open(filename, SFM_READ, &sfinfo);
	subformat = sfinfo.format & SF_FORMAT_SUBMASK;
	items = sfinfo.channels * sfinfo.frames;
	log_msg(OTTS_LOG_DEBUG, "Espeak: frames = %ld, channels = %d",
		sfinfo.frames, sfinfo.channels);
	log_msg(OTTS_LOG_DEBUG, "Espeak: samplerate = %i, items = %Ld",
		sfinfo.samplerate, (long long)items);
	log_msg(OTTS_LOG_DEBUG,
		"Espeak: major format = 0x%08X, subformat = 0x%08X, endian = 0x%08X",
		sfinfo.format & SF_FORMAT_TYPEMASK, subformat,
		sfinfo.format & SF_FORMAT_ENDMASK);
	if (sfinfo.channels < 1 || sfinfo.channels > 2) {
		log_msg(OTTS_LOG_NOTICE, "Espeak: ERROR: channels = %d.\n",
			sfinfo.channels);
		result = FALSE;
		goto cleanup1;
	}
	if (sfinfo.frames > 0x7FFFFFFF) {
		log_msg(OTTS_LOG_WARN,
			"Espeak: ERROR: Unknown number of frames.");
		result = FALSE;
		goto cleanup1;
	}
	if (subformat == SF_FORMAT_FLOAT || subformat == SF_FORMAT_DOUBLE) {
		/* Set scaling for float to integer conversion. */
		sf_command(sf, SFC_SET_SCALE_FLOAT_INT_READ, NULL, SF_TRUE);
	}
	AudioTrack track;
	track.num_samples = sfinfo.frames;
	track.num_channels = sfinfo.channels;
	track.sample_rate = sfinfo.samplerate;
	track.bits = 16;
	track.samples = g_malloc(items * sizeof(short));
	readcount = sf_read_short(sf, (short *)track.samples, items);
	log_msg(OTTS_LOG_INFO, "Espeak: read %Ld items from audio file.",
		(long long)readcount);

	if (readcount > 0) {
		track.num_samples = readcount / sfinfo.channels;
		log_msg(OTTS_LOG_INFO, "Espeak: Sending %i samples to audio.",
			track.num_samples);
		/* Volume is controlled by the synthesizer.  Always play at normal on audio device. */
		opentts_audio_set_volume(module_audio_id,
					 EspeakSoundIconVolume);
		int ret =
		    opentts_audio_play(module_audio_id, track, SPD_AUDIO_LE);
		if (ret < 0) {
			log_msg(OTTS_LOG_ERR,
				"ERROR: Can't play track for unknown reason.");
			result = FALSE;
			goto cleanup2;
		}
		log_msg(OTTS_LOG_INFO, "Espeak: Sent to audio.");
	}
cleanup2:
	g_free(track.samples);
cleanup1:
	sf_close(sf);
#endif
	return result;
}

static SPDVoice **espeak_list_synthesis_voices()
{
	SPDVoice **result = NULL;
	const espeak_VOICE **espeak_voices = espeak_ListVoices(NULL);
	int i = 0;
	int j = 0;

	/* Count number of voices */
	int numvoices = 0;
	while (espeak_voices[numvoices] != NULL) {
		numvoices++;
	}
	log_msg(OTTS_LOG_INFO, "Espeak: %d voices total.", numvoices);
	result = g_new0(SPDVoice *, numvoices + 1);
	for (i = j = 0; espeak_voices[i] != NULL; i++) {
		const espeak_VOICE *v = espeak_voices[i];
		if (!g_str_has_prefix(v->identifier, "mb/")) {
			/* Not an mbrola voice */
			SPDVoice *voice = g_new0(SPDVoice, 1);

			voice->name = g_strdup(v->name);

			const gchar *first_lang = v->languages + 1;
			gchar *lang = NULL;
			gchar *dialect = NULL;
			if (g_utf8_validate(first_lang, -1, NULL)) {
				gchar *dash =
				    g_utf8_strchr(first_lang, -1, '-');
				if (dash != NULL) {
					/* There is probably a dialect string (like en-uk) */
					lang =
					    g_strndup(first_lang,
						      dash - first_lang);
					dialect =
					    g_strdup(g_utf8_next_char(dash));
				} else {
					lang = g_strdup(first_lang);
				}
			} else {
				log_msg(OTTS_LOG_WARN,
					"Espeak: Not a valid utf8 string: %s",
					first_lang);;
			}
			voice->language = lang;
			voice->variant = dialect;

			result[j++] = voice;
		}
	}
	result[j] = NULL;
	log_msg(OTTS_LOG_NOTICE, "Espeak: %d usable voices.", j);

	return result;
}

static void espeak_free_voice_list()
{
	if (espeak_voice_list != NULL) {
		int i;
		for (i = 0; espeak_voice_list[i] != NULL; i++) {
			g_free(espeak_voice_list[i]->name);
			g_free(espeak_voice_list[i]->language);
			g_free(espeak_voice_list[i]->variant);
			g_free(espeak_voice_list[i]);
		}
		g_free(espeak_voice_list);
		espeak_voice_list = NULL;
	}
}

static TEspeakSuccess espeak_set_punctuation_list_from_utf8(const gchar * punct)
{
	TEspeakSuccess result = ERROR;
	wchar_t *wc_punct =
	    (wchar_t *) g_convert(punct, -1, "wchar_t", "utf-8", NULL, NULL,
				  NULL);
	if (wc_punct != NULL) {
		espeak_ERROR ret = espeak_SetPunctuationList(wc_punct);
		if (ret == EE_OK)
			result = OK;
		g_free(wc_punct);
	}
	return result;
}

static otts_synth_plugin_t espeak_plugin = {
	MODULE_NAME,
	MODULE_VERSION,
	espeak_load,
	espeak_init,
	espeak_audio_init,
	espeak_speak,
	espeak_stop,
	espeak_list_voices,
	espeak_pause,
	espeak_close
};

otts_synth_plugin_t * espeak_plugin_get (void)
{
	return &espeak_plugin;
}

otts_synth_plugin_t *SYNTH_PLUGIN_ENTRY(void)
	__attribute__ ((weak, alias("espeak_plugin_get")));

/* > */

/* local variables: */
/* folded-file: t */
/* c-basic-offset: 4 */
/* end: */
