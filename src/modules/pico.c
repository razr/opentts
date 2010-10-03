/*
 * pico.c - OpenTTS pico SVOX output module
 *
 * A pico SVOX output module
 *
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

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include <glib.h>
#include <semaphore.h>

#include <picoapi.h>

#include "opentts/opentts_types.h"
#include "opentts/opentts_synth_plugin.h"
#include "module_utils.h"
#include "logging.h"

#define MODULE_NAME     "pico"
#define MODULE_VERSION  "0.1"

#define MAX_OUTBUF_SIZE			(128)
#define PICO_MEM_SIZE			(10000000)

#define PICO_VOICE_SPEED_MIN		(20)
#define PICO_VOICE_SPEED_MAX		(500)
#define PICO_VOICE_SPEED_DEFAULT	(100)

#define PICO_VOICE_PITCH_MIN		(50)
#define PICO_VOICE_PITCH_MAX		(200)
#define PICO_VOICE_PITCH_DEFAULT	(100)

#define PICO_VOICE_VOLUME_MIN		(0)
#define PICO_VOICE_VOLUME_MAX		(500)
#define PICO_VOICE_VOLUME_DEFAULT	(100)

static pico_System     picoSystem;
static pico_Resource   picoTaResource;
static pico_Resource   picoSgResource;
static pico_Engine     picoEngine;
static pico_Char       *picoInp;

const char * PICO_LINGWARE_PATH = "/usr/share/pico/lang/";
const int PICO_SAMPLE_RATE = 16000;
const char * picoInternalTaLingware[] = {
	"en-US_ta.bin",
	"en-GB_ta.bin",
	"de-DE_ta.bin",
	"es-ES_ta.bin",
	"fr-FR_ta.bin",
	"it-IT_ta.bin" };
const char * picoInternalSgLingware[] = {
	"en-US_lh0_sg.bin",
	"en-GB_kh0_sg.bin",
	"de-DE_gl0_sg.bin",
	"es-ES_zl0_sg.bin",
	"fr-FR_nk0_sg.bin",
	"it-IT_cm0_sg.bin" };

const SPDVoice pico_voices[] = {
	{"samantha", "en", "en-US"},
	{"serena", "en", "en-GB"},
	{"sabrina", "de", "de-DE"},
	{"isabel", "es", "es-ES"},
	{"virginie", "fr", "fr-FR"},
	{"silvia", "it", "it-IT"}
};

const SPDVoice *pico_voices_list[] = {
	&pico_voices[0],
	&pico_voices[1],
	&pico_voices[2],
	&pico_voices[3],
	&pico_voices[4],
	&pico_voices[5],
	NULL
};

static GThread *pico_play_thread;
static sem_t *pico_play_semaphore;

enum states {STATE_IDLE, STATE_PLAY, STATE_PAUSE, STATE_STOP, STATE_CLOSE};
static enum states pico_state;

/* Module configuration options */
MOD_OPTION_1_STR(PicoLingwarePath)

static int pico_set_rate(signed int value)
{
	int speed;

	if (value < OTTS_VOICE_RATE_DEFAULT)
		speed = PICO_VOICE_SPEED_MIN + (value - OTTS_VOICE_RATE_MIN)
		* (PICO_VOICE_SPEED_DEFAULT - PICO_VOICE_SPEED_MIN)
		/ (OTTS_VOICE_RATE_DEFAULT - OTTS_VOICE_RATE_MIN);
	else
		speed = PICO_VOICE_SPEED_DEFAULT + (value
		                                    - OTTS_VOICE_RATE_DEFAULT)
		* (PICO_VOICE_SPEED_MAX - PICO_VOICE_SPEED_DEFAULT)
		/ (OTTS_VOICE_RATE_MAX - OTTS_VOICE_RATE_DEFAULT);

	return speed;
}

static int pico_set_volume(signed int value)
{
	int volume;

	if (value < OTTS_VOICE_VOLUME_DEFAULT)
		volume = PICO_VOICE_VOLUME_MIN
			+ (value - OTTS_VOICE_VOLUME_MIN)
		* (PICO_VOICE_VOLUME_DEFAULT - PICO_VOICE_VOLUME_MIN)
		/ (OTTS_VOICE_VOLUME_DEFAULT - OTTS_VOICE_VOLUME_MIN);
	else
		volume = PICO_VOICE_VOLUME_DEFAULT
			+ (value - OTTS_VOICE_VOLUME_DEFAULT)
		* (PICO_VOICE_VOLUME_MAX - PICO_VOICE_VOLUME_DEFAULT)
		/ (OTTS_VOICE_VOLUME_MAX - OTTS_VOICE_VOLUME_DEFAULT);

	return volume;
}

static int pico_set_pitch(signed int value)
{
	int pitch;

	if (value < OTTS_VOICE_PITCH_DEFAULT)
		pitch = PICO_VOICE_PITCH_MIN + (value - OTTS_VOICE_PITCH_MIN)
		* (PICO_VOICE_PITCH_DEFAULT - PICO_VOICE_PITCH_MIN)
		/ (OTTS_VOICE_PITCH_DEFAULT - OTTS_VOICE_PITCH_MIN);
	else
		pitch = PICO_VOICE_PITCH_DEFAULT + (value
		                                    - OTTS_VOICE_PITCH_DEFAULT)
		* (PICO_VOICE_PITCH_MAX - PICO_VOICE_PITCH_DEFAULT)
		/ (OTTS_VOICE_PITCH_MAX - OTTS_VOICE_PITCH_DEFAULT);

	return pitch;
}

static int pico_process_tts(void)
{
	pico_Int16 bytes_sent, bytes_recv, text_remaining, out_data_type;
	int ret, getstatus;
	short outbuf[MAX_OUTBUF_SIZE];
	pico_Retstring outMessage;
	AudioTrack track;
	pico_Char *buf = picoInp;

	text_remaining = strlen((const char *) buf) + 1;

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME " Text: %s\n", picoInp);

	/* synthesis loop   */
	while (text_remaining) {
		/* Feed the text into the engine.   */
		if((ret = pico_putTextUtf8(picoEngine, buf, text_remaining,
		                           &bytes_sent))) {
			pico_getSystemStatusMessage(picoSystem, ret, outMessage);
			log_msg(OTTS_LOG_ERR, MODULE_NAME
			        "Cannot put Text (%i): %s\n", ret, outMessage);
			return -1;
		}

		text_remaining -= bytes_sent;
		buf += bytes_sent;

		do {
			/* Retrieve the samples and add them to the buffer.
			   SVOX pico TTS sample rate is 16K */
			getstatus = pico_getData(picoEngine, (void *) outbuf,
				MAX_OUTBUF_SIZE, &bytes_recv, &out_data_type );
			if((getstatus != PICO_STEP_BUSY)
			   && (getstatus != PICO_STEP_IDLE)){
				pico_getSystemStatusMessage(picoSystem, getstatus, outMessage);
				log_msg(OTTS_LOG_ERR, MODULE_NAME
				        "Cannot get Data (%i): %s\n", getstatus,
				        outMessage);
				return -1;
			}

			if (bytes_recv) {
				track.num_samples = bytes_recv / 2;
				track.samples = (short *) g_memdup((gconstpointer) outbuf, bytes_recv);
				track.num_channels = 1;
				track.sample_rate = PICO_SAMPLE_RATE;
				track.bits = 16;
				log_msg(OTTS_LOG_DEBUG, MODULE_NAME
				        ": Sending %i samples to audio.", track.num_samples);

				if (opentts_audio_set_volume(module_audio_id, 85) < 0) {
					log_msg(OTTS_LOG_ERR, MODULE_NAME
						"Can't set volume. audio not initialized?");
					continue;
				}
				if (opentts_audio_play(module_audio_id, track,
				                       module_audio_id->format) < 0) {
					log_msg(OTTS_LOG_ERR, MODULE_NAME
					        "Can't play track for unknown reason.");
					return -1;
				}
			}
		} while (PICO_STEP_BUSY == getstatus
		         && g_atomic_int_get(&pico_state) == STATE_PLAY);

	}

	g_free(picoInp);
	picoInp = NULL;
	return 0;
}

/* Playback thread. */
static gpointer
pico_play_func(gpointer nothing)
{
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME ": Playback thread starting");

	set_speaking_thread_parameters();

	while (g_atomic_int_get(&pico_state) != STATE_CLOSE) {

		sem_wait(pico_play_semaphore);
		if (g_atomic_int_get(&pico_state) != STATE_PLAY)
			continue;

		log_msg(OTTS_LOG_DEBUG, MODULE_NAME ": Sending to TTS engine");
		module_report_event_begin();

		if (0 != pico_process_tts()) {
			log_msg(OTTS_LOG_ERR, MODULE_NAME ": ERROR in TTS");
		}


		if (g_atomic_int_get(&pico_state) == STATE_PLAY) {
			module_report_event_end();
			g_atomic_int_set(&pico_state, STATE_IDLE);
		}

		if (g_atomic_int_get(&pico_state) == STATE_STOP) {
			module_report_event_stop();
			g_atomic_int_set(&pico_state, STATE_IDLE);
		}

		if (g_atomic_int_get(&pico_state) == STATE_PAUSE) {
			module_report_event_pause();
			g_atomic_int_set(&pico_state, STATE_IDLE);
		}

		log_msg(OTTS_LOG_DEBUG, MODULE_NAME ": state %d", pico_state);

	}
	return 0;
}

/* plugin functions */
static int pico_load(void)
{
	init_settings_tables();

	MOD_OPTION_1_STR_REG(PicoLingwarePath,PICO_LINGWARE_PATH);

	module_audio_id = NULL;

	return 0;
}

int pico_init_voice (int voice_index) {
	int ret;
	pico_Retstring outMessage;
	pico_Char picoTaFileName[PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE];
	pico_Char picoSgFileName[PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE];
	pico_Char picoTaResourceName[PICO_MAX_RESOURCE_NAME_SIZE];
	pico_Char picoSgResourceName[PICO_MAX_RESOURCE_NAME_SIZE];

	/* Load the text analysis Lingware resource file.   */
	strcpy((char *) picoTaFileName, PicoLingwarePath);
	strcat((char *) picoTaFileName,
	       (const char *) picoInternalTaLingware[voice_index]);
	if((ret = pico_loadResource(picoSystem, picoTaFileName, &picoTaResource))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot load TA Lingware resource file (%i): %s\n", ret,
		        outMessage);
		return -1;
	}

	/* Load the signal generation Lingware resource file.   */
	strcpy((char *) picoSgFileName, PicoLingwarePath);
	strcat((char *) picoSgFileName,
	       (const char *) picoInternalSgLingware[voice_index]);
	if((ret = pico_loadResource(picoSystem, picoSgFileName, &picoSgResource))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot load SG Lingware resource file (%i): %s\n", ret,
		        outMessage);
		return -1;
	}

	/* Get the text analysis resource name.     */
	if((ret = pico_getResourceName(picoSystem, picoTaResource,
	                               (char *) picoTaResourceName))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot get TA resource name (%i): %s\n", ret,
		        outMessage);
		return -1;
	}

	/* Get the signal generation resource name. */
	if((ret = pico_getResourceName(picoSystem, picoSgResource,
	                               (char *) picoSgResourceName))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot get SG resource name (%i): %s\n", ret,
		        outMessage);
		return -1;
	}

	/* Create a voice definition.   */
	if((ret = pico_createVoiceDefinition(picoSystem,
			(const pico_Char *) pico_voices[voice_index].name))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot create voice definition (%i): %s\n", ret,
		        outMessage);
		return -1;
	}

	/* Add the text analysis resource to the voice. */
	if((ret = pico_addResourceToVoiceDefinition(picoSystem,
			(const pico_Char *)pico_voices[voice_index].name,
			picoTaResourceName))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot add TA resource to the voice (%i): %s\n",
		        ret, outMessage);
		return -1;
	}

	/* Add the signal generation resource to the voice. */
	if((ret = pico_addResourceToVoiceDefinition(picoSystem,
			(const pico_Char *)pico_voices[voice_index].name,
			picoSgResourceName))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot add SG resource to the voice (%i): %s\n",
		        ret, outMessage);
		return -1;
	}

	return 0;
}

static int pico_init(char **status_info)
{
	int ret, i;
	pico_Retstring outMessage;
        void * pmem;
	GError *error = NULL;

	if (!g_thread_supported())
		g_thread_init(NULL);

	pico_play_semaphore = module_semaphore_init();
	if (pico_play_semaphore == NULL) {
		*status_info = g_strdup_printf(MODULE_NAME
			"Failed to initialize play thread semaphore!");
		log_msg(OTTS_LOG_CRIT, MODULE_NAME": %s", *status_info);
		return -1;
	}

	if ((pico_play_thread = g_thread_create((GThreadFunc)pico_play_func,
	                                        NULL, TRUE, &error)) == NULL) {
		*status_info = g_strdup_printf(MODULE_NAME
			"Failed to create a play thread : %s\n",
			error->message );
		log_msg(OTTS_LOG_CRIT, MODULE_NAME": %s", *status_info);
		g_error_free(error);
		return -1;
	}

	pmem = g_malloc(PICO_MEM_SIZE);
	if((ret = pico_initialize(pmem, PICO_MEM_SIZE, &picoSystem))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		*status_info = g_strdup_printf(MODULE_NAME
			": Cannot initialize (%i): %s\n", ret, outMessage);
		g_free(pmem);
		return -1;
	}

	/* load resource for all language, probably need only one */
	for (i = 0; i < sizeof(pico_voices)/sizeof(SPDVoice); i++) {
		if (0 != pico_init_voice(i)) {
			*status_info = g_strdup_printf(MODULE_NAME
				": fail init voice (%s)\n", pico_voices[i].name);
			g_free(pmem);
			return -1;
		}
	}

	/* Create a new Pico engine, english default */
	if((ret = pico_newEngine(picoSystem,
	                         (const pico_Char *)pico_voices[0].name,
	                         &picoEngine))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		*status_info = g_strdup_printf(MODULE_NAME
			": Cannot create a new pico engine (%i): %s\n",
			ret, outMessage);
		g_free(pmem);
		return -1;
	}

	*status_info = g_strdup(MODULE_NAME ": Initialized successfully.");

	g_atomic_int_set(&pico_state, STATE_IDLE);
	return 0;
}

static int pico_audio_init(char **status_info)
{
	return module_audio_init_spd(status_info);
}

static SPDVoice **pico_list_voices(void)
{
	return pico_voices_list;
}

void pico_set_synthesis_voice(char *voice_name)
{
	int ret;
	pico_Retstring outMessage;

	/* Create a new Pico engine, english default */
	if((ret = pico_disposeEngine(picoSystem, &picoEngine))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot dispose pico engine (%i): %s\n", ret,
		        outMessage);
		return;
	}

	/* Create a new Pico engine, english default */
	if((ret = pico_newEngine(picoSystem, (const pico_Char *)voice_name,
	                         &picoEngine))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot create a new pico engine (%i): %s\n", ret,
		        outMessage);
		return;
	}

	return;
}

static void pico_set_language(char *lang)
{
	int i;

	/* get voice name based on language */
	for (i = 0; i < sizeof(pico_voices)/sizeof(SPDVoice); i++) {
		if (!strcmp(pico_voices[i].language,lang)) {
			pico_set_synthesis_voice(pico_voices[i].name);
			return;
		}
	}
	return;
}

static int pico_speak(char * data, size_t bytes, SPDMessageType msgtype)
{
	int value;
	static pico_Char *tmp;

	if (g_atomic_int_get(&pico_state) != STATE_IDLE){
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME
		        ": module still speaking state = %d", pico_state);
		return 0;
	}

	/* Setting speech parameters. */

	UPDATE_STRING_PARAMETER(voice.name, pico_set_synthesis_voice);
/*	UPDATE_PARAMETER(voice_type, pico_set_voice);*/
	UPDATE_STRING_PARAMETER(voice.language, pico_set_language);

	picoInp = (pico_Char *) module_strip_ssml(data);

	value = pico_set_rate(msg_settings.rate);
	if (PICO_VOICE_SPEED_DEFAULT != value) {
		tmp = picoInp;
		picoInp = (pico_Char *) g_strdup_printf(
				"<speed level='%d'>%s</speed>", value, tmp);
		g_free(tmp);
	}

	value = pico_set_volume(msg_settings.volume);
	if (PICO_VOICE_VOLUME_DEFAULT != value) {
		tmp = picoInp;
		picoInp = (pico_Char *) g_strdup_printf(
				"<volume level='%d'>%s</volume>", value, tmp);
		g_free(tmp);
	}

	value = pico_set_pitch(msg_settings.pitch);
	if (PICO_VOICE_PITCH_DEFAULT != value) {
		tmp = picoInp;
		picoInp = (pico_Char *) g_strdup_printf(
				"<pitch level='%d'>%s</pitch>", value, tmp);
		g_free(tmp);
	}

	switch (msgtype) {
		case SPD_MSGTYPE_CHAR:
		case SPD_MSGTYPE_KEY:
		case SPD_MSGTYPE_TEXT:
		case SPD_MSGTYPE_SOUND_ICON:
		default:
			log_msg(OTTS_LOG_DEBUG, MODULE_NAME
			        ": msgtype = %d", msgtype);
			break;
	}

	g_atomic_int_set(&pico_state, STATE_PLAY);
	sem_post(pico_play_semaphore);
	return bytes;
}

static int pico_stop(void)
{
	pico_Status ret;
	pico_Retstring outMessage;

	if (g_atomic_int_get(&pico_state) != STATE_PLAY){
		log_msg(OTTS_LOG_WARN, MODULE_NAME
		        ": STOP called when not in PLAY state");
		return -1;
	}

	g_atomic_int_set(&pico_state, STATE_STOP);

	/* reset Pico engine. */
	if((ret = pico_resetEngine(picoEngine, PICO_RESET_SOFT))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        "Cannot reset pico engine (%i): %s\n", ret, outMessage);
		return -1;
	}

	return 0;
}

static size_t pico_pause(void)
{
	pico_Status ret;
	pico_Retstring outMessage;

	if (g_atomic_int_get(&pico_state) != STATE_PLAY){
		log_msg(OTTS_LOG_WARN, MODULE_NAME
		        ": PAUSE called when not in PLAY state");
		return -1;
	}

	g_atomic_int_set(&pico_state, STATE_PAUSE);

	/* reset Pico engine. */
	if((ret = pico_resetEngine(picoEngine, PICO_RESET_SOFT))) {
		pico_getSystemStatusMessage(picoSystem, ret, outMessage);
		log_msg(OTTS_LOG_WARN, MODULE_NAME
		        "Cannot reset pico engine (%i): %s\n", ret, outMessage);
		return -1;
	}

	return 0;
}

static void pico_close(int status)
{
	if (module_audio_id) {
		opentts_audio_close(module_audio_id);
	}

	g_atomic_int_set(&pico_state, STATE_CLOSE);
	sem_post(pico_play_semaphore);

	g_thread_join(pico_play_thread);

	if (picoSystem) {
		pico_terminate(&picoSystem);
		picoSystem = NULL;
	}

	g_free(pico_play_semaphore);
}

static otts_synth_plugin_t pico_plugin = {
	MODULE_NAME,
	MODULE_VERSION,
	pico_load,
	pico_init,
	pico_audio_init,
	pico_speak,
	pico_stop,
	pico_list_voices,
	pico_pause,
	pico_close
};

otts_synth_plugin_t *pico_plugin_get (void)
{
	return &pico_plugin;
}

otts_synth_plugin_t *SYNTH_PLUGIN_ENTRY(void)
	__attribute__ ((weak, alias("pico_plugin_get")));
