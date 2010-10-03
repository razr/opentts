/*
 * milena.c -- OpenTTS interface for Milena TTS system.
 *
 * Copyright (C) 2010 Bohdan R. Rau <ethanak@polip.com>
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
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

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
#error "Where is dlfcn?"
#endif
#include <math.h>
#include "audio.h"
#include "mbrola.h"

#include "opentts/opentts_types.h"
#include "opentts/opentts_synth_plugin.h"
#include "module_utils.h"
#include "logging.h"

#include <ctype.h>
#include <fcntl.h>

#if HAVE_SNDFILE
#include <sndfile.h>
#endif

#define MODULE_NAME     "milena"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG();


/* milena module _must_ be compiled without milena libraries! */

/* typedefs for milena */

typedef struct milena * (*MILTYP_INIT) (char *,char *,char *);
typedef int (*MILTYP_UC) (struct milena *,char *);
typedef int (*MILTYP_UCI) (struct milena *,char *,int);
typedef int (*MILTYP_UCCI) (struct milena *,char *,char *,int);
typedef int (*MILTYP_UCC) (struct milena *,char *,char *);
typedef int (*MILTYP_UCICI) (struct milena *,char *,int,char *,int);
typedef int (*MILTYP_UCCII) (struct milena *,char *,char *,int,int);
typedef int (*MILTYP_US) (struct milena *,char **);
typedef int (*MILTYP_USI) (struct milena *,char **,int);
typedef int (*MILTYP_UI) (struct milena *,int);
typedef int (*MILTYP_UGFRA) (struct milena *,char **,char *,int,int *);
typedef int (*MILTYP_UGFRAP) (struct milena *,char **,char *,int,int *,int,char *);

typedef void (*MILTYP_VI) (struct milena *,int);
typedef void (*MILTYP_VC) (struct milena *,char *);
typedef void (milena_errfun)(char *,char *,int);
typedef void (*MILTYP_VF) (struct milena *,milena_errfun *);
typedef void (*MILTYP_V) (struct milena *);

typedef int (*MILTYP_CCI) (char *,char *,int);
typedef char *(*MILTYP_C2CC) (char *,char *);
typedef char *(*MILTYP_C2V) (void);

typedef int (*MILTYP_CCIJ) (char *,char *,int,int *);
typedef int (*MILTYP_UCCIJ) (struct milena *,char *,char *,int,int *);
typedef int (*MILTYP_UICIJ) (struct milena *,int,char *,int,int *);
typedef int (*MILTYP_CCIJSI) (char *,char *,int,int *,char **,int);


void *milena_dll,*mimbla_dll;

/* all milena functions */

MILTYP_INIT milena_Init;
MILTYP_UC milena_ReadUserDic;
MILTYP_UCI milena_ReadUserDicWithFlags;
MILTYP_UCICI milena_ReadUserDicLineWithFlags;
MILTYP_UC milena_ReadPhraser;
MILTYP_UC milena_ReadStressFile;
MILTYP_USI milena_GetParType;
MILTYP_VI milena_SetPhraserMode;
MILTYP_VC milena_SetLangMode;
MILTYP_VF milena_registerErrFun;
MILTYP_UGFRA milena_GetPhrase;
MILTYP_UGFRAP milena_GetPhraseWithPunct;
MILTYP_UCCII milena_TranslatePhrase;
MILTYP_UCCI milena_Prestresser;
MILTYP_CCI milena_Poststresser;
MILTYP_UC milena_ReadVerbs;
MILTYP_UC milena_ReadKeyChar;
MILTYP_US milena_SkipToUnknownWord;
MILTYP_C2CC milena_FilePath;
MILTYP_UC milena_IsIgnoredWord;
MILTYP_V milena_Close;
MILTYP_C2V milena_GetVersion;
MILTYP_CCIJSI milena_utf2iso_mp;
MILTYP_CCIJ milena_utf2iso;
MILTYP_UICIJ milena_wchar;
MILTYP_UCCIJ milena_char;
MILTYP_UCCIJ milena_key;


struct phone_buffer {
	char *str;
	int str_len;
	int buf_len;
};

typedef struct milena_mbrola_cfg * (*MIMTYP_INIT) (char *);
typedef int (*MIMTYP_UC) (struct milena_mbrola_cfg *,char *);
typedef void (*MIMTYP_VID) (struct milena_mbrola_cfg *,int,...);
typedef void (*MIMTYP_PHR) (struct milena_mbrola_cfg *,char *,FILE *,int);
typedef void (*MIMTYP_PHRP) (struct milena_mbrola_cfg *,char *,struct phone_buffer *,int);
typedef void (*MIMTYP_UFI) (struct milena_mbrola_cfg *,FILE *,int);
typedef void (*MIMTYP_UPI) (struct milena_mbrola_cfg *,struct phone_buffer *,int);
typedef void (*MIMTYP_UII) (struct milena_mbrola_cfg *,int,int);
typedef int (*MIMTYP_UV) (struct milena_mbrola_cfg *);

MIMTYP_INIT milena_InitModMbrola;
MIMTYP_UC milena_ModMbrolaExtras;
MIMTYP_VID milena_ModMbrolaSetVoice;
MIMTYP_PHR milena_ModMbrolaGenPhrase;
MIMTYP_PHRP milena_ModMbrolaGenPhraseP;
MIMTYP_UC milena_ReadInton;
MIMTYP_UFI milena_ModMbrolaBreak;
MIMTYP_UPI milena_ModMbrolaBreakP;
MIMTYP_UII milena_ModMbrolaSetFlag;
MIMTYP_UV milena_CloseModMbrola;

#define MILENA_UDIC_DICTMODE 1
#define MILENA_UDIC_IGNORE 2
#define MILENA_PHR_IGNORE_TILDE 1
#define MILENA_PHR_IGNORE_INFO 2
#define MILENA_PHR_IGNORE_ALL 3
#define MILENA_PARTYPE_EMPTY 0
#define MILENA_PARTYPE_NORMAL 1
#define MILENA_PARTYPE_DIALOG 2


static int milena_speaking = 0;

static pthread_t milena_speak_thread;
static void *milena_speak_routine(void *);

static sem_t *milena_semaphore;

static char **milena_message;
static SPDMessageType milena_message_type;


static int milena_initialize(char **);
static void milena_bye(void);
static void milena_play_icon(char *icon);

int milena_stopped = 0;

static int milena_get_msgpart(char **msg,char *icon,char **outbuf,int *phmode);

#define MILENA_PITCH 0.9
#define MILENA_TEMPO 0.9

int milena_rate=0,
	milena_pitch=0,
	milena_volume=100,
	milena_cap_mode=0,
	milena_punct=0;

static struct milena *milena;
static struct milena_mbrola_cfg *mimbrola;
static struct otts_mbrolaHandler *omh;

static char *milena_trans_phrase(char *buf,int phmode);


MOD_OPTION_1_STR(MilenaPunctuationSome);
MOD_OPTION_1_STR(MilenaSoundIconFolder);
MOD_OPTION_1_STR(MilenaMinCapLet);
MOD_OPTION_1_INT(MilenaContrastLevel);
MOD_OPTION_1_STR(MilenaUserDic);
MOD_OPTION_1_STR(MilenaMbrolaFile);


static int
milena_load(void)
{
	init_settings_tables();

	REGISTER_DEBUG();

	MOD_OPTION_1_STR_REG(MilenaMinCapLet,"icon");
	MOD_OPTION_1_STR_REG(MilenaSoundIconFolder,"/usr/share/sound/sound-icons/");
	MOD_OPTION_1_STR_REG(MilenaUserDic,"");
	MOD_OPTION_1_STR_REG(MilenaMbrolaFile,"");
	MOD_OPTION_1_INT_REG(MilenaContrastLevel,0);
	MOD_OPTION_1_STR_REG(MilenaPunctuationSome,"()");
	otts_mbrolaModLoad();
	return 0;
}

static int
milena_init(char **status_info)
{
	int ret;
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": module init");
	*status_info = NULL;

	if (!milena_initialize(status_info)) {
		log_msg(OTTS_LOG_ERR, MODULE_NAME": Couldn't init milena");
		if (!*status_info) *status_info=g_strdup("Cannot initialize Milena");
		milena_bye();
		return -1;
	}

	milena_message = malloc (sizeof (char*));
	*milena_message = NULL;

	milena_semaphore = module_semaphore_init();

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME
	        ": creating new thread for milena_speak\n");
	milena_speaking = 0;
	ret = pthread_create(&milena_speak_thread, NULL, milena_speak_routine, NULL);
	if(ret != 0){
		log_msg(OTTS_LOG_ERR, MODULE_NAME": thread failed\n");
		*status_info = strdup("The module couldn't initialize threads "
			      "This could be either an internal problem or an "
			      "architecture problem. If you are sure your architecture "
			      "supports threads, please report a bug.");
		milena_bye();
		return -1;
	}
	omh=otts_mbrolaInit("pl1");
	if (!omh) {
		log_msg(OTTS_LOG_ERR, MODULE_NAME": can't find pl1 voice");
		*status_info =
		    g_strdup("Voice pl1 not found");
		milena_bye();
		return -1;
	}

	module_audio_id = NULL;
	*status_info = strdup("Milena initialized successfully.");
	return 0;
}

static int milena_audio_init(char **status_info)
{
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Opening audio");
	return module_audio_init_spd(status_info);
}

static SPDVoice voice_demonstrator;
static SPDVoice *voice_milena[] = { &voice_demonstrator, NULL };

static SPDVoice **milena_list_voices(void)
{
	voice_demonstrator.name = "Milena";
	voice_demonstrator.language = "pl";
	return voice_milena;
}


static void milena_set_volume(signed int volume)
{
	milena_volume = ensure(volume,-100,100);
}

static void milena_set_rate(signed int rate)
{
	milena_rate = ensure((int)(rate / MILENA_TEMPO),-100,100);
}

static void milena_set_pitch(signed int pitch)
{
	milena_pitch = ensure((int)(pitch * MILENA_PITCH),-100,100);
}

static void milena_set_cap_let_recogn(SPDCapitalLetters cap_mode)
{
	milena_cap_mode = 0;
	switch (cap_mode) {
	case SPD_CAP_NONE:
		milena_cap_mode = 0;
		break;
	case SPD_CAP_ICON:
		milena_cap_mode = 1;
		break;
	case SPD_CAP_SPELL:
		milena_cap_mode = 2;
		break;
	case SPD_CAP_ERR:
		/* Do nothing.  This code should never be reached. */
		break;
	}
	if (!strcasecmp(MilenaMinCapLet,"spell")) {
		milena_cap_mode=2;
	}
	else if (strcasecmp(MilenaMinCapLet,"none") && !milena_cap_mode) {
		milena_cap_mode=1;
	}
}

static void milena_set_punctuation_mode(SPDPunctuation punct_mode)
{
	milena_punct = 1;
	switch (punct_mode) {
	case SPD_PUNCT_NONE:
		milena_punct = 0;
		break;
	case SPD_PUNCT_SOME:
		milena_punct = 1;
		break;
	case SPD_PUNCT_ALL:
		milena_punct = 2;
		break;
	case SPD_PUNCT_ERR:
		/* Do nothing.  This code should never be reached. */
		break;
	}
}

static int
milena_speak(gchar *data, size_t bytes, SPDMessageType msgtype)
{
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": write\n");

	if (milena_speaking){
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME
		        ": Speaking when requested to write");
		return 0;
	}

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Requested data: |%s|\n", data);

	if (*milena_message != NULL){
		xfree(*milena_message);
		*milena_message = NULL;
	}
	*milena_message = module_strip_ssml(data);
	milena_message_type = msgtype;
	if ((msgtype == SPD_MSGTYPE_TEXT)
	    && (msg_settings.spelling_mode == SPD_SPELL_ON))
		milena_message_type = SPD_MSGTYPE_SPELL;
	if (milena_message_type == SPD_MSGTYPE_TEXT) {
		int nlen=milena_utf2iso_mp(*milena_message,NULL,3,NULL,NULL,0);
		char *c;
		if (nlen<=0) {
			c=strdup("ehm?");
		} else {
			c=malloc(nlen);
			milena_utf2iso_mp(*milena_message,c,3,NULL,NULL,0);
		}
		xfree(*milena_message);
		*milena_message=c;
	}
	/* Setting voice */
	UPDATE_PARAMETER(volume, milena_set_volume);
	UPDATE_PARAMETER(rate, milena_set_rate);
	UPDATE_PARAMETER(pitch, milena_set_pitch);
	UPDATE_PARAMETER(punctuation_mode,milena_set_punctuation_mode);
	UPDATE_PARAMETER(cap_let_recogn, milena_set_cap_let_recogn);

	/* Send semaphore signal to the speaking thread */
	milena_speaking = 1;
	sem_post(milena_semaphore);

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": leaving write normally\n\r");
	return bytes;
}

static int milena_stop(void)
{
	int ret;
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": stop\n");

	milena_stopped = 1;
	if (module_audio_id) {
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Stopping audio");
		ret = opentts_audio_stop(module_audio_id);
		if (ret != 0)
			log_msg(OTTS_LOG_WARN, MODULE_NAME
			        ": Non 0 value from spd_audio_stop: %d", ret);
	}

	return 0;
}

static size_t milena_pause(void)
{
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": pause requested\n");
	if (milena_speaking) {
		log_msg(OTTS_LOG_WARN, MODULE_NAME
		        ": doesn't support pause, stopping\n");

		milena_stop();

		return -1;
	} else {
		return 0;
	}
}


static void milena_close(int status)
{

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": close\n");

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Stopping speech");
	if (milena_speaking) {
		milena_stop();
	}

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Terminating threads");
	if (module_terminate_thread(milena_speak_thread) != 0)
		exit(1);

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Closing audio output");
	opentts_audio_close(module_audio_id);

	exit(status);
}

/* milena initialization */

#define MILENA_FUN(type,name) do {if (!(name=(type)dlsym(milena_dll,#name))) {log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Function %s not found in milena library",#name);*status_info=g_strdup("Incorect version of milena library");return 0;}} while(0)
#define MIMBLA_FUN(type,name) do {if (!(name=(type)dlsym(mimbla_dll,#name))) {log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Function %s not found in mimbla library",#name);*status_info=g_strdup("Incorect version of mimbla library");return 0;}} while(0)

int load_milena_libs(char **status_info)
{
	char *c;int n,gd;
	if (!(milena_dll=dlopen("libmilena.so",RTLD_GLOBAL | RTLD_NOW))) {
		*status_info=g_strdup("Milena library not found");
		log_msg(OTTS_LOG_ERR, MODULE_NAME": Milena library not found");
		return 0;
	}
	if (!(mimbla_dll=dlopen("libmilena_mbrola.so",RTLD_GLOBAL | RTLD_NOW))) {
		*status_info=g_strdup("Mimbla library not found");
		log_msg(OTTS_LOG_ERR, MODULE_NAME": Mimbla library not found");
		return 0;
	}
	MILENA_FUN(MILTYP_INIT,milena_Init);
	MILENA_FUN(MILTYP_C2V,milena_GetVersion);
	c=milena_GetVersion();
	n=strtol(c,&c,10);
	gd=0;
	if (n>0) gd=1;
	else if (*c=='.') {
		c++;
		n=strtol(c,&c,10);
		if (n>2) gd=1;
		else if (n<2) gd=0;
		else if (*c=='.') {
			c++;
			n=strtol(c,&c,10);
			if (n>18) gd=1;
		}
	}
	if (!gd) {
		*status_info=g_strdup("Please upgrade Milena to 0.2.19 or higher");
		log_msg(OTTS_LOG_ERR, MODULE_NAME
		        ": Milena library version < 0.2.19 is not supported");
		return 0;
	}

	MILENA_FUN(MILTYP_UC,milena_ReadUserDic);
	MILENA_FUN(MILTYP_UCI,milena_ReadUserDicWithFlags);
	MILENA_FUN(MILTYP_UCICI,milena_ReadUserDicLineWithFlags);
	MILENA_FUN(MILTYP_UC,milena_ReadPhraser);
	MILENA_FUN(MILTYP_UC,milena_ReadStressFile);
	MILENA_FUN(MILTYP_USI,milena_GetParType);
	MILENA_FUN(MILTYP_VI,milena_SetPhraserMode);
	MILENA_FUN(MILTYP_VC,milena_SetLangMode);
	MILENA_FUN(MILTYP_VF,milena_registerErrFun);
	MILENA_FUN(MILTYP_UGFRA,milena_GetPhrase);
	MILENA_FUN(MILTYP_UGFRAP,milena_GetPhraseWithPunct);
	MILENA_FUN(MILTYP_UCCII,milena_TranslatePhrase);
	MILENA_FUN(MILTYP_UCCI,milena_Prestresser);
	MILENA_FUN(MILTYP_CCI,milena_Poststresser);
	MILENA_FUN(MILTYP_UC,milena_ReadVerbs);
	MILENA_FUN(MILTYP_UC,milena_ReadKeyChar);
	MILENA_FUN(MILTYP_US,milena_SkipToUnknownWord);
	MILENA_FUN(MILTYP_C2CC,milena_FilePath);
	MILENA_FUN(MILTYP_UC,milena_IsIgnoredWord);
	MILENA_FUN(MILTYP_V,milena_Close);
	MILENA_FUN(MILTYP_C2V,milena_GetVersion);
	MILENA_FUN(MILTYP_CCIJ,milena_utf2iso);
	MILENA_FUN(MILTYP_CCIJSI,milena_utf2iso_mp);
	MILENA_FUN(MILTYP_UICIJ,milena_wchar);
	MILENA_FUN(MILTYP_UCCIJ,milena_char);
	MILENA_FUN(MILTYP_UCCIJ,milena_key);

	MIMBLA_FUN(MIMTYP_INIT,milena_InitModMbrola);
	MIMBLA_FUN(MIMTYP_UC,milena_ModMbrolaExtras);
	MIMBLA_FUN(MIMTYP_VID,milena_ModMbrolaSetVoice);
	MIMBLA_FUN(MIMTYP_PHR,milena_ModMbrolaGenPhrase);
	MIMBLA_FUN(MIMTYP_PHRP,milena_ModMbrolaGenPhraseP);
	MIMBLA_FUN(MIMTYP_UC,milena_ReadInton);
	MIMBLA_FUN(MIMTYP_UFI,milena_ModMbrolaBreak);
	MIMBLA_FUN(MIMTYP_UPI,milena_ModMbrolaBreakP);
	MIMBLA_FUN(MIMTYP_UII,milena_ModMbrolaSetFlag);
	MIMBLA_FUN(MIMTYP_UV,milena_CloseModMbrola);
	return 1;
}

static int milena_initialize(char **status_info)
{
        char ixbuf[256],ibuf[256],obuf[256];
	if (!load_milena_libs(status_info)) return 0;
	milena=milena_Init(
                milena_FilePath("pl_pho.dat",obuf),
                milena_FilePath("pl_dict.dat",ibuf),
                milena_FilePath("pl_stress.dat",ixbuf));
	if (!milena) return 0;
        if (!milena_ReadPhraser(milena,
                milena_FilePath("pl_phraser.dat",ibuf))) return 0;
        if (!milena_ReadPhraser(milena,
                milena_FilePath("pl_hours.dat",ibuf))) return 0;
        if (!milena_ReadPhraser(milena,
                milena_FilePath("pl_inet_phr.dat",ibuf))) return 0;
        mimbrola=milena_InitModMbrola(
                milena_FilePath("pl_mbrola.dat",ibuf));
        if (!mimbrola) return 0;
        if (!milena_ReadUserDicWithFlags(milena,
                milena_FilePath("pl_udict.dat",ibuf),0)) return 0;
        if (!milena_ReadUserDicWithFlags(milena,
               milena_FilePath("pl_computer.dat",ibuf),0)) return 0;
	if (MilenaUserDic[0]) {

		if (!milena_ReadUserDicWithFlags(milena,MilenaUserDic,MILENA_UDIC_IGNORE)) return 0;
	}
        if (!milena_ReadInton(mimbrola,
                milena_FilePath("pl_intona.dat",ibuf))) return 0;
	if (MilenaMbrolaFile[0]) {
		if (MilenaMbrolaFile[0]=='/') strcpy(ibuf,MilenaMbrolaFile);
		else milena_FilePath(MilenaMbrolaFile,ibuf);
		if (!milena_ModMbrolaExtras(mimbrola,ibuf)) return 0;
	}
	milena_SetPhraserMode(milena,3);
	milena_ReadKeyChar(milena,milena_FilePath("pl_keys.dat",ibuf));
	return 1;
}

static void milena_bye(void)
{
	if (milena) milena_Close(milena);
	if (mimbrola) milena_CloseModMbrola(mimbrola);
	milena=NULL;
	mimbrola=NULL;
}



/* internal module functions */

static void* milena_speak_routine(void* nothing)
{
	char *buf;
	int phmode;
	char icon[64];
	char *msg;
	char *phonemes;

	set_speaking_thread_parameters();
	while(1){
		sem_wait(milena_semaphore);
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Semaphore on\n");

		milena_stopped = 0;
		milena_speaking = 1;


		module_report_event_begin();
		msg=*milena_message;
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": To say: %s\n",msg);
		while(1) {
			if (milena_stopped){
				log_msg(OTTS_LOG_DEBUG, MODULE_NAME
				        ": Stop in child, terminating");
				milena_speaking = 0;
				module_report_event_stop();
				break;
			}
			icon[0]=0;
			buf=NULL;
			if (!milena_get_msgpart(&msg,icon,&buf,&phmode)) {
				milena_speaking=0;
				if (milena_stopped) module_report_event_stop();
				else module_report_event_end();
				break;
			}
			if (icon[0]) {
				milena_play_icon(icon);
				icon[0]=0;
				if (milena_stopped) {
					if (buf) free(buf);
					buf=NULL;
					milena_speaking=0;
					module_report_event_stop();
					break;
				}

			}
			if (buf) {
				phonemes=milena_trans_phrase(buf,phmode);
				free(buf);
				buf=NULL;
				if (phonemes) {
					otts_mbrolaSpeak(omh,phonemes,
						milena_rate,
						milena_pitch,
						milena_volume,
						ensure(MilenaContrastLevel,-100,100));
				}
			}

			if (milena_stopped) {
				milena_speaking=0;
				module_report_event_stop();
				break;
			}
		}
		milena_stopped=0;
	}
	milena_speaking = 0;
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": speaking thread ended.......\n");
	pthread_exit(NULL);
}

static char *milena_trans_phrase(char *inbuf,int phmode)
{
	static char *ibuf,*obuf;
	static int ilen=0,olen=0;
	static struct phone_buffer phb;
	int n;
	if (!ilen) {
		ibuf=malloc(ilen=8192);
		obuf=malloc(olen=8192);
	}
	n=milena_Prestresser(milena,inbuf,obuf,olen);
	if (n<0) return NULL;
	if (n>0) {
		obuf=realloc(obuf,olen=n+1024);
		milena_Prestresser(milena,inbuf,obuf,olen);
	}
	n=milena_TranslatePhrase(milena,obuf,ibuf,ilen,0);
	if (n<0) return NULL;
	if (n>0) {
		ibuf=realloc(ibuf,ilen=n+1024);
		milena_TranslatePhrase(milena,obuf,ibuf,ilen,0);
	}
	n=milena_Poststresser(ibuf,obuf,olen);
	if (n>0) {
		obuf=realloc(obuf,olen=n+1024);
		milena_Poststresser(ibuf,obuf,olen);
	}
	phb.str_len=0;
	milena_ModMbrolaGenPhraseP(mimbrola,obuf,&phb,phmode & 7);
	return phb.str;
}

static int get_unichar(char **str)
{
	wchar_t wc;
	int n;
	wc=*(*str)++ & 255;
	if ((wc & 0xe0)==0xc0) {
		wc &=0x1f;
		n=1;
	}
	else if ((wc & 0xf0)==0xe0) {
		wc &=0x0f;
		n=2;
	}
	else if ((wc & 0xf8)==0xf0) {
		wc &=0x07;
		n=3;
	}
	else if ((wc & 0xfc)==0xf8) {
		wc &=0x03;
		n=4;
	}
	else if ((wc & 0xfe)==0xfc) {
		wc &=0x01;
		n=5;
	}
	else return wc;
	while (n--) {
		if ((**str & 0xc0) != 0x80) {
			wc='?';
			break;
		}
		wc=(wc << 6) | ((*(*str)++) & 0x3f);
	}
	return wc;
}

static int milena_get_msgpart(char **msg,char *icon,char **buf,int *phmode)
{
	int rc,wc,nlen,iscap,*iscp;char *c;
	if (!*msg || !**msg) return 0;
	iscp=NULL;
	iscap=0;
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": CAP %d", milena_cap_mode);
	if (milena_cap_mode == 1) iscp=&iscap;
	switch(milena_message_type) {
		case SPD_MSGTYPE_SOUND_ICON:
		if (strlen(*msg)<63) {
			strcpy(icon,*msg);
			rc=1;
		}
		else {
			rc=0;
		}
		*msg=NULL;
		return rc;

		case SPD_MSGTYPE_SPELL:
		wc=get_unichar(msg);
		if (!wc) {
			*msg=NULL;
			return 0;
		}
		nlen=milena_wchar(milena,wc,NULL,0,iscp);
		if (nlen>0) {
			*buf=malloc(nlen);
			milena_wchar(milena,wc,*buf,nlen,iscp);
			*phmode=16;
		}
		else {
			*buf=strdup("b\xb3\261d");
			*phmode=2;
		}
		*phmode=(**msg)?1:16;
		if (iscap) strcpy(icon,"capital");
		return 1;

		case SPD_MSGTYPE_KEY:
		nlen=milena_key(milena,*msg,NULL,0,iscp);
		if (nlen>0) {
			*buf=malloc(nlen);
			milena_key(milena,*msg,*buf,nlen,iscp);
			*phmode=16;
		}
		else {
			*buf=strdup("b\xb3\261d");
			*phmode=2;
		}
		*msg=NULL;
		if (iscap) strcpy(icon,"capital");
		return 1;
		case SPD_MSGTYPE_CHAR:
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": CHAR [%s]\n",*msg);
		wc=get_unichar(msg);
		nlen=milena_wchar(milena,wc,NULL,0,iscp);
		if (nlen>0) {
			*buf=malloc(nlen);
			milena_wchar(milena,wc,*buf,nlen,iscp);
			*phmode=16;
		}
		else {
			*buf=strdup("b\xb3\261d");
			*phmode=2;
		}
		*msg=NULL;
		if (iscap) strcpy(icon,"capital");
		return 1;

		case SPD_MSGTYPE_TEXT:
		c=*msg;
		nlen=milena_GetPhraseWithPunct(milena,msg,NULL,0,phmode,milena_punct,MilenaPunctuationSome);
		if (nlen<=0) {
			*msg=NULL;
			return 0;
		}
		*msg=c;
		*buf=malloc(nlen+1);
		milena_GetPhraseWithPunct(milena,msg,*buf,nlen+1,phmode,milena_punct,MilenaPunctuationSome);
		return 1;

		default:
		*msg=NULL;
		log_msg(OTTS_LOG_WARN, MODULE_NAME": Unknown message type\n");
		return 0;
	}
	return 0;
}

static void milena_play_icon(char *icon)
{
#if HAVE_SNDFILE
	int subformat;
	sf_count_t items;
	sf_count_t readcount;
	SNDFILE *sf;
	SF_INFO sfinfo;
	char filename[256];
	sprintf(filename, "%s/%s",MilenaSoundIconFolder,icon);

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Playing |%s|", filename);
	memset(&sfinfo, 0, sizeof(sfinfo));
	sf = sf_open(filename, SFM_READ, &sfinfo);
	if (!sf) return;
	subformat = sfinfo.format & SF_FORMAT_SUBMASK;
	items = sfinfo.channels * sfinfo.frames;
	if (sfinfo.channels < 1 || sfinfo.channels > 2) {
		log_msg(OTTS_LOG_ERR, MODULE_NAME": channels = %d.\n",
		        sfinfo.channels);
		goto cleanup1;
	}
	if (sfinfo.frames > 0x7FFFFFFF) {
		log_msg(OTTS_LOG_ERR, MODULE_NAME": Unknown number of frames.");
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

	if (readcount > 0) {
		track.num_samples = readcount / sfinfo.channels;
		opentts_audio_set_volume(module_audio_id, milena_volume);
		int ret = opentts_audio_play(module_audio_id, track, SPD_AUDIO_LE);
		if (ret < 0) {
			log_msg(OTTS_LOG_ERR, MODULE_NAME
			        ": Can't play track for unknown reason.");
			goto cleanup2;
		}
	}
cleanup2:
	g_free(track.samples);
cleanup1:
	sf_close(sf);
#endif
}

static otts_synth_plugin_t milena_plugin = {
	MODULE_NAME,
	MODULE_VERSION,
	milena_load,
	milena_init,
	milena_audio_init,
	milena_speak,
	milena_stop,
	milena_list_voices,
	milena_pause,
	milena_close
};

otts_synth_plugin_t *milena_plugin_get (void)
{
	return &milena_plugin;
}

otts_synth_plugin_t *SYNTH_PLUGIN_ENTRY(void)
	__attribute__ ((weak, alias("milena_plugin_get")));
