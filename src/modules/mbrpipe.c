/*
 * mbrpipe.c -- OpenTTS interface for various command-line text-to-phoneme.
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
#include "module_utils.h"
#include "logging.h"

#include <ctype.h>
#include <fcntl.h>

#if HAVE_SNDFILE
#include <sndfile.h>
#endif

#define MODULE_NAME     "mbrpipe"
#define MODULE_VERSION  "0.1"

#define DEBUG_MODULE 1
DECLARE_DEBUG();

/* Thread and process control */
static int mbrpipe_speaking = 0;

static pthread_t mbrpipe_speak_thread;
static sem_t *mbrpipe_semaphore;

static char **mbrpipe_message;
static SPDMessageType mbrpipe_message_type;

int mbrpipe_stop = 0;

static void *mbrpipe_speak(void *);

MOD_OPTION_1_STR(MbrpipeSpeakerName);
MOD_OPTION_1_STR(MbrpipeSpeakerBase);
MOD_OPTION_1_STR(MbrpipeSpeakerCommand);
MOD_OPTION_1_STR(MbrpipeDelimiters);
MOD_OPTION_1_STR(MbrpipePunctuationSome);
MOD_OPTION_1_STR(MbrpipeSoundIconFolder);
MOD_OPTION_1_INT(MbrpipeCleanOutput);
MOD_OPTION_1_INT(MbrpipeIgnoreDumbTTSDic);
MOD_OPTION_1_INT(MbrpipeNoDumbTTS);
MOD_OPTION_1_INT(MbrpipeDontSpellUcase);
MOD_OPTION_1_INT(MbrpipeSpeakerSingleShot);
MOD_OPTION_1_STR(MbrpipeSynthEncoding);
MOD_OPTION_1_STR(MbrpipeMinCapLet);
MOD_OPTION_1_INT(MbrpipeContrastLevel);
MOD_OPTION_1_INT(MbrpipeMaxFinalPause);
MOD_OPTION_1_INT(MbrpipeMaxInnerPause);

static int MbrpipeMinICapLet=1;

static void *dumb_conf;

static struct otts_mbrolaHandler *omh;

static int mbrpipe_volume,mbrpipe_rate,mbrpipe_pitch,mbrpipe_cap_mode,mbrpipe_punct;

static int to_tts[2],from_tts[2];


/* local functions */

static int tts_fork(char **status_info);
static void init_dumbtts(void);


int module_load(void)
{
	init_settings_tables();

	REGISTER_DEBUG();

	MOD_OPTION_1_STR_REG(MbrpipeDelimiters, ".;:,!?");
	MOD_OPTION_1_STR_REG(MbrpipePunctuationSome,"_()[]{}");

	MOD_OPTION_1_STR_REG(MbrpipeSoundIconFolder,
			     "/usr/share/sound/sound-icons/");
	MOD_OPTION_1_STR_REG(MbrpipeSpeakerBase, "fr1");
	MOD_OPTION_1_STR_REG(MbrpipeSpeakerCommand, "text2phone -p");
	MOD_OPTION_1_STR_REG(MbrpipeSpeakerName, "Demonstrator");
	MOD_OPTION_1_STR_REG(MbrpipeSynthEncoding, "ISO-8859-1");
	MOD_OPTION_1_STR_REG(MbrpipeMinCapLet, "icon");
	MOD_OPTION_1_INT_REG(MbrpipeCleanOutput, 1);
	MOD_OPTION_1_INT_REG(MbrpipeSpeakerSingleShot, 0);
	MOD_OPTION_1_INT_REG(MbrpipeIgnoreDumbTTSDic, 1);
	MOD_OPTION_1_INT_REG(MbrpipeDontSpellUcase, 0);
	MOD_OPTION_1_INT_REG(MbrpipeNoDumbTTS, 0);
	MOD_OPTION_1_INT_REG(MbrpipeContrastLevel, 0);
	MOD_OPTION_1_INT_REG(MbrpipeMaxFinalPause, 20);
	MOD_OPTION_1_INT_REG(MbrpipeMaxInnerPause, 50);
	MbrpipeMaxFinalPause=ensure(MbrpipeMaxFinalPause,5,200);
	MbrpipeMaxInnerPause=ensure(MbrpipeMaxInnerPause,5,200);

	otts_mbrolaModLoad();

	return 0;
}

int module_init(char **status_info)
{
	int ret,pid;
	GString *info;

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Module init");

	*status_info = NULL;
	info = g_string_new("");

	if (!MbrpipeSpeakerSingleShot) {
		pid=tts_fork(status_info);
	}
	mbrpipe_message = g_malloc(sizeof(char *));
	*mbrpipe_message = NULL;

	mbrpipe_semaphore = module_semaphore_init();

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME
	        ": creating new thread for mbrpipe_speak\n");
	mbrpipe_speaking = 0;
	ret = pthread_create(&mbrpipe_speak_thread, NULL, mbrpipe_speak, NULL);
	if (ret != 0) {
		log_msg(OTTS_LOG_ERR, MODULE_NAME": thread failed\n");
		*status_info =
		    g_strdup("The module couldn't initialize threads "
			     "This could be either an internal problem or an "
			     "architecture problem. If you are sure your architecture "
			     "supports threads, please report a bug.");
		if (!MbrpipeSpeakerSingleShot) {
			close(to_tts[1]);
			close(from_tts[0]);
		}
		return -1;
	}

	omh=otts_mbrolaInit(MbrpipeSpeakerBase);
	if (!omh) {
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": can't find base %s\n",
		        MbrpipeSpeakerBase);
		*status_info =
		    g_strdup("Speaker base not found");
		if (!MbrpipeSpeakerSingleShot) {
			close(to_tts[1]);
			close(from_tts[0]);
		}
		return -1;
	}
	init_dumbtts();
	module_audio_id = NULL;
	*status_info = g_strdup("MbrPipe initialized successfully.");
	return 0;
}


int module_audio_init(char **status_info)
{
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Opening audio");
	return module_audio_init_spd(status_info);
}

static SPDVoice voice_demonstrator;
static SPDVoice *voice_mbr[] = { &voice_demonstrator, NULL };

SPDVoice **module_list_voices(void)
{
	voice_demonstrator.name = MbrpipeSpeakerName;
	voice_demonstrator.language = omh->voice->lang;
	return voice_mbr;
}


static void mbrpipe_set_volume(signed int volume)
{
	mbrpipe_volume = ensure(volume,-100,100);
}

static void mbrpipe_set_rate(signed int rate)
{
	mbrpipe_rate = ensure(rate,-100,100);
}

static void mbrpipe_set_pitch(signed int pitch)
{
	mbrpipe_pitch = ensure(pitch,-100,100);
}

static void mbrpipe_set_cap_let_recogn(SPDCapitalLetters cap_mode)
{
	mbrpipe_cap_mode = 0;
	switch (cap_mode) {
	case SPD_CAP_NONE:
		mbrpipe_cap_mode = 0;
		break;
	case SPD_CAP_ICON:
		mbrpipe_cap_mode = 1;
		break;
	case SPD_CAP_SPELL:
		mbrpipe_cap_mode = 2;
		break;
	case SPD_CAP_ERR:
		/* Do nothing.  This code should never be reached. */
		break;
	}
	if (!strcasecmp(MbrpipeMinCapLet,"spell")) {
		mbrpipe_cap_mode=2;
	}
	else if (strcasecmp(MbrpipeMinCapLet,"none") && !mbrpipe_cap_mode) {
		mbrpipe_cap_mode=1;
	}
}

static void mbrpipe_set_punctuation_mode(SPDPunctuation punct_mode)
{
	mbrpipe_punct = 1;
	switch (punct_mode) {
	case SPD_PUNCT_NONE:
		mbrpipe_punct = 0;
		break;
	case SPD_PUNCT_SOME:
		mbrpipe_punct = 1;
		break;
	case SPD_PUNCT_ALL:
		mbrpipe_punct = 2;
		break;
	case SPD_PUNCT_ERR:
		/* Do nothing.  This code should never be reached. */
		break;
	}
}


int module_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": write()\n");

	if (mbrpipe_speaking) {
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME
		        ": Speaking when requested to write");
		return 0;
	}

	if (module_write_data_ok(data) != 0)
		return -1;

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Requested data: |%s|\n", data);

	if (*mbrpipe_message != NULL) {
		g_free(*mbrpipe_message);
		*mbrpipe_message = NULL;
	}
	*mbrpipe_message = module_strip_ssml(data);
	mbrpipe_message_type = msgtype;
	if ((msgtype == SPD_MSGTYPE_TEXT)
	    && (msg_settings.spelling_mode == SPD_SPELL_ON))
		mbrpipe_message_type = SPD_MSGTYPE_SPELL;

	/* Setting voice */

	UPDATE_PARAMETER(volume, mbrpipe_set_volume);
	UPDATE_PARAMETER(rate, mbrpipe_set_rate);
	UPDATE_PARAMETER(pitch, mbrpipe_set_pitch);
	UPDATE_PARAMETER(cap_let_recogn, mbrpipe_set_cap_let_recogn);
	UPDATE_PARAMETER(punctuation_mode, mbrpipe_set_punctuation_mode);

	/* Send semaphore signal to the speaking thread */
	mbrpipe_speaking = 1;
	sem_post(mbrpipe_semaphore);

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": leaving write() normally\n\r");
	return bytes;
}

int module_stop(void)
{
	int ret;
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": stop\n");

	mbrpipe_stop = 1;
	if (module_audio_id) {
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Stopping audio");
		ret = opentts_audio_stop(module_audio_id);
		if (ret != 0)
			log_msg(OTTS_LOG_WARN, MODULE_NAME
			        ": Non 0 value from spd_audio_stop: %d", ret);
	}

	return 0;
}

size_t module_pause(void)
{
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": pause requested\n");
	if (mbrpipe_speaking) {
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME
		        ": doesn't support pause, stopping\n");

		module_stop();

		return -1;
	} else {
		return 0;
	}
}

void module_close(int status)
{

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": close\n");

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Stopping speech");
	if (mbrpipe_speaking) {
		module_stop();
	}

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Terminating threads");
	if (module_terminate_thread(mbrpipe_speak_thread) != 0)
		exit(1);

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Closing audio output");
	opentts_audio_close(module_audio_id);

	exit(status);
}




/* Internal functions */

void *dumbtts_dll;
typedef void * (*DTSISTR) (char *);
typedef void * (*DTSISTRI) (char *,int);
typedef int (*DTSINTCHR) (void *,char *,char *,int,int,int *);
typedef int (*DTSINTWCHR) (void *,wchar_t,char *,int,int,int *);
typedef int (*DTSINTTXT) (void *,char *,char *,int,int,char *,char *);

static DTSISTR dumbtts_TTSInit;
static DTSISTRI dumbtts_TTSInitWithFlags;
static DTSINTCHR dumbtts_CharString;
static DTSINTCHR dumbtts_KeyString;
static DTSINTWCHR dumbtts_WCharString;
static DTSINTTXT dumbtts_GetString;
static int detilde;

static void init_dumbtts(void)
{
#ifdef HAVE_DLFCN_H
	if (MbrpipeNoDumbTTS) return;
	dumbtts_dll=dlopen("libdumbtts.so",RTLD_LAZY);
	if (!dumbtts_dll) {
		log_msg(OTTS_LOG_ERR, MODULE_NAME": Can't find libdumbtts.so");
		return;
	}
	dumbtts_TTSInitWithFlags=(DTSISTRI) dlsym(dumbtts_dll,"dumbtts_TTSInitWithFlags");
	if (!dumbtts_TTSInitWithFlags) {
		dumbtts_TTSInit=(DTSISTR) dlsym(dumbtts_dll,"dumbtts_TTSInit");
		dumb_conf=dumbtts_TTSInit(omh->voice->lang);
	}
	else {
		int flags=0;
		if (MbrpipeIgnoreDumbTTSDic) flags =1;
		if (MbrpipeDontSpellUcase) flags |= 2;
		dumb_conf=dumbtts_TTSInitWithFlags(omh->voice->lang,flags);
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Flags DumbTTS %d",flags);
	}
	if (!dumb_conf) {
		return;
	}
	else if (!strcmp(omh->voice->lang,"pl")) {
		detilde=1;
	}
	dumbtts_CharString=(DTSINTCHR) dlsym(dumbtts_dll,"dumbtts_CharString");
	dumbtts_KeyString=(DTSINTCHR) dlsym(dumbtts_dll,"dumbtts_KeyString");
	dumbtts_WCharString=(DTSINTWCHR) dlsym(dumbtts_dll,"dumbtts_WCharString");
	dumbtts_GetString=(DTSINTTXT) dlsym(dumbtts_dll,"dumbtts_GetString");
#endif
}


static int get_unichar(char **str)
{
	wchar_t wc;
	int n;
	wc = *(*str)++ & 255;
	if ((wc & 0xe0) == 0xc0) {
		wc &= 0x1f;
		n = 1;
	} else if ((wc & 0xf0) == 0xe0) {
		wc &= 0x0f;
		n = 2;
	} else if ((wc & 0xf8) == 0xf0) {
		wc &= 0x07;
		n = 3;
	} else if ((wc & 0xfc) == 0xf8) {
		wc &= 0x03;
		n = 4;
	} else if ((wc & 0xfe) == 0xfc) {
		wc &= 0x01;
		n = 5;
	} else
		return wc;
	while (n--) {
		if ((**str & 0xc0) != 0x80) {
			wc = '?';
			break;
		}
		wc = (wc << 6) | ((*(*str)++) & 0x3f);
	}
	return wc;
}

int mbrpipe_get_msgpart(void *conf, SPDMessageType type, char **msg,
		      char *icon, char **buf, int *len, int cap_mode,
		      char *delimiters, int punct_mode, char *punct_some)
{
	int rc;
	int isicon;
	int n, bytes;
	unsigned int pos;
	wchar_t wc;
	char xbuf[1024];

	if (!*msg)
		return 1;
	if (!**msg)
		return 1;
	isicon = 0;
	icon[0] = 0;
	if (*buf)
		**buf = 0;
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": message %s type %d\n", *msg, type);
	if (!dumb_conf) { /* No dumbtts library? */
		pos=0;
		bytes =
		    module_get_message_part(*msg, xbuf, &pos, 1023, delimiters);
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Got bytes %d, %s",
		        bytes, xbuf);
		if (bytes <= 0) {
			*msg = NULL;
			return 1;
		}
		*msg += pos;
		xbuf[bytes] = 0;
		g_free(*buf);
		if (strcasecmp(MbrpipeSynthEncoding,"utf-8")) {
			*buf=g_convert(xbuf,-1,MbrpipeSynthEncoding,"UTF-8",NULL,NULL,NULL);
			*len=strlen(*buf);
		}
		else {
			*len=strlen(xbuf);
			*buf=g_strdup(xbuf);
		}
		return 0;
	}
	switch (type) {
	case SPD_MSGTYPE_SOUND_ICON:
		if (strlen(*msg) < 63) {
			strcpy(icon, *msg);
			rc = 0;
		} else {
			rc = 1;
		}
		*msg = NULL;
		return rc;

	case SPD_MSGTYPE_SPELL:
		wc = get_unichar(msg);
		if (!wc) {
			*msg = NULL;
			return 1;
		}
		n = dumbtts_WCharString(conf, wc, *buf, *len, cap_mode,
					&isicon);
		if (n > 0) {
			*len = n + 128;
			*buf = g_realloc(*buf, *len);
			n = dumbtts_WCharString(conf, wc, *buf, *len, cap_mode,
						&isicon);
		}
		if (n) {
			*msg = NULL;
			return 1;
		}
		if (isicon)
			strcpy(icon, "capital");
		return 0;

	case SPD_MSGTYPE_KEY:
	case SPD_MSGTYPE_CHAR:

		if (type == SPD_MSGTYPE_KEY) {
			n = dumbtts_KeyString(conf, *msg, *buf, *len, cap_mode,
					      &isicon);
		} else {
			n = dumbtts_CharString(conf, *msg, *buf, *len, cap_mode,
					       &isicon);
		}
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Got n=%d", n);
		if (n > 0) {
			*len = n + 128;
			*buf = g_realloc(*buf, *len);
			if (type == SPD_MSGTYPE_KEY) {
				n = dumbtts_KeyString(conf, *msg, *buf, *len,
						      cap_mode, &isicon);
			} else {
				n = dumbtts_CharString(conf, *msg, *buf, *len,
						       cap_mode, &isicon);
			}
		}
		*msg = NULL;

		if (!n && isicon)
			strcpy(icon, "capital");
		return n;

	case SPD_MSGTYPE_TEXT:
		pos = 0;
		bytes =
		    module_get_message_part(*msg, xbuf, &pos, 1023, delimiters);
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Got bytes %d, %s",
		        bytes, xbuf);
		if (bytes <= 0) {
			*msg = NULL;
			return 1;
		}
		*msg += pos;
		xbuf[bytes] = 0;

		n = dumbtts_GetString(conf, xbuf, *buf, *len, punct_mode,
				      punct_some, ",.;:!?'");

		if (n > 0) {
			*len = n + 128;
			*buf = g_realloc(*buf, *len);
			n = dumbtts_GetString(conf, xbuf, *buf, *len,
					      punct_mode, punct_some, ",.;:!?'");
		}
		if (n) {
			*msg = NULL;
			return 1;
		}
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME
		        ": Returning to translator |%s|", *buf);
		return 0;

	default:

		*msg = NULL;
		log_msg(OTTS_LOG_WARN, MODULE_NAME": Unknown message type\n");
		return 1;
	}
}




static int find_phoneme(char **str,char **pho,int *len,char **rest,int *pa,double *pf1,double *pf2)
{
	char *s,*c;int pau_len;
	pau_len=0;*pa=0;
	for (;**str;) {
		while (**str && isspace(**str)) str++;
		if (!**str) break;
		if (**str!='_' && pau_len) return 1;
		s=strchr(*str,'\n');
		if (s) {
			*s++=0;
		}
		else {
			s=*str+strlen(*str);
		}
		c=strchr(*str,';');
		if (c) *c=0;
		c=*str;
		*str=s;
		while (*c && isspace(*c)) c++;
		if (!*c) continue;
		if (*c!='_')  {
			*pho=c;
			while (*c && !isspace(*c)) c++;
			if (!*c) continue;
			*c++=0;
			while (*c && isspace(*c)) c++;
			if (!*c || !isdigit(*c)) continue;
			*len=strtol(c,&c,10);
			while (*c && isspace(*c)) c++;
			*rest=c;
			return 1;
		}
		if (!pau_len) *pho=c;
		while (*c && !isspace(*c)) c++;
		if (!*c) continue;
		*c++=0;
		while (*c && isspace(*c)) c++;
		if (!isdigit(*c)) continue;
		pau_len+=strtol(c,&c,10);
		*len=pau_len;
		while (*c) {
			double pc;double pf;
			while (*c && isspace(*c)) c++;
			if (!*c || !isdigit(*c)) break;
			pc=strtod(c,&c);
			while (*c && isspace(*c)) c++;
			if (!*c || !isdigit(*c)) break;
			pf=strtod(c,&c);
			if (!*pa) {
				*pf1=pf;
				*pa=1;
			}
			*pf2=pf;
		}
	}
	if (pau_len) return 1;
	return 0;
}


char *mbrpipe_cleaned_buffer(char *buf)
{
	GString *str;
	char *pho,*rest;int len,phas,was_pho,last_pau;double pf1,pf2;
	str=g_string_sized_new(strlen(buf)+32);
	g_string_assign(str,"");
	if (!find_phoneme(&buf,&pho,&len,&rest,&phas,&pf1,&pf2)) return g_string_free(str,0);
	last_pau=0;
	if (*pho=='_') {
		if (len>20) len=20;
		if (phas) {
			g_string_append_printf(str,"_ %d 50 %.1f\n",len,pf2);
		}
		else {
			g_string_append_printf(str,"_ %d\n",len);
		}
		last_pau=1;
		was_pho=find_phoneme(&buf,&pho,&len,&rest,&phas,&pf1,&pf2);
	}
	else {
		was_pho=1;
	}
	while (was_pho) {
		if (*pho == '_') {
			int maxlen,oldlen;int phasa;double pf1a,pf2a;
			last_pau=1;
			oldlen=len;phasa=phas;pf1a=pf1;pf2a=pf2;
			was_pho=find_phoneme(&buf,&pho,&len,&rest,&phas,&pf1,&pf2);
			maxlen=MbrpipeMaxFinalPause;
			if (was_pho) maxlen=MbrpipeMaxInnerPause;
			if (oldlen > maxlen) oldlen=maxlen;
			if (phasa) {
				g_string_append_printf(str,"_ %d 25 %.1f 75 %.1f\n",oldlen,pf1a,pf2a);
			}
			else {
				g_string_append_printf(str,"_ %d\n",oldlen);
			}
			continue;
		}
		last_pau=0;
		if (pho[1]=='#') pho[1]=0; /* Bug in Polish espeak */
		g_string_append_printf(str,"%s %d %s\n",pho,len,rest);
		was_pho=find_phoneme(&buf,&pho,&len,&rest,&phas,&pf1,&pf2);
	}
	if (!last_pau) {
		/* no final pause? */
		g_string_append_printf(str,"_ %d\n",MbrpipeMaxFinalPause);
	}
	return g_string_free(str,0);
}

static void mbrpipe_play_icon(char *icon);

/* delete Ivona markers (for Polish dumbtts) */
void remove_tilde_escapes(char *b)
{
	char *c;
	for (c=b;*b;) {
		if (*b!='~') {
			*c++=*b++;
			continue;
		}
		b++;
		if (*b != '!' && *b != '\'') {
			*c++='~';
		}
		else {
			b++;
		}
	}
	*c=0;
}

static void *mbrpipe_speak(void *nothing)
{

	char *buf;
	int len;

	char *msg;
	char *outbuf;
	int outlen,outpos;
	char rdbuf[4096];
	char icon[64];

	int n,bg;

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": speaking thread starting...\n");

	set_speaking_thread_parameters();

	buf=g_malloc(len=512);

	while (1) {
		sem_wait(mbrpipe_semaphore);
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Semaphore on\n");

		mbrpipe_stop = 0;
		mbrpipe_speaking = 1;

		module_report_event_begin();
		msg = *mbrpipe_message;
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": To say: %s\n", msg);
		buf = NULL;
		len = 0;
		if (!MbrpipeSpeakerSingleShot) {
			/* old wave may still exist after stop */
			while((n=read(from_tts[0],rdbuf,4096))>0) {
				log_msg(OTTS_LOG_DEBUG, MODULE_NAME
				        ": DISCARD %d",n);
			}
		}
		while (1) {
			if (mbrpipe_stop) {
				log_msg(OTTS_LOG_DEBUG, MODULE_NAME
				        ": Stop in child, terminating");
				mbrpipe_speaking = 0;
				module_report_event_stop();
				break;
			}
			if (!msg || !*msg || mbrpipe_get_msgpart(
				dumb_conf,mbrpipe_message_type,
				&msg,icon,&buf,&len,
				mbrpipe_cap_mode,MbrpipeDelimiters,
				mbrpipe_punct,MbrpipePunctuationSome))
			{
				mbrpipe_speaking = 0;
				if (mbrpipe_stop)
					module_report_event_stop();
				else
					module_report_event_end();
				break;
			}
			if (icon[0]) mbrpipe_play_icon(icon);
			if (!buf || !*buf) continue;
			if (detilde) remove_tilde_escapes(buf);
			log_msg(OTTS_LOG_DEBUG, MODULE_NAME
			        ": Speak buffer is %s", buf);
			if (MbrpipeSpeakerSingleShot) {
				if (tts_fork(NULL)<0) {
					break;
				}
			}
			write(to_tts[1],buf,strlen(buf));
			write(to_tts[1],"\n",1);
			if (MbrpipeSpeakerSingleShot) {
				close(to_tts[1]);
			}
			log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Wrote");
			outlen=0;
			outpos=0;
			outbuf=NULL;
			for (bg=0;;bg=1) {
				struct timeval tv;
				fd_set rdfs;
				int retval,n;
				if (!MbrpipeSpeakerSingleShot) {
					FD_ZERO(&rdfs);
					FD_SET(from_tts[0],&rdfs);

					tv.tv_sec=0;
					tv.tv_usec=(bg)?10000:100000;

					retval=select(from_tts[0]+1,&rdfs,NULL,NULL,&tv);
					log_msg(OTTS_LOG_DEBUG, MODULE_NAME
					        ": Select %d", retval);
					if (retval<0) {
						break;
					}
					if (!retval) break;
				}
				n=read(from_tts[0],rdbuf,4096);
				log_msg(OTTS_LOG_DEBUG, MODULE_NAME
				        ": Read %d", n);
				if (n<=0) break;
				if (!outlen) {
					outlen=n+1024;
					outbuf=g_malloc(outlen);
				}
				else if (outpos+n+1>=outlen) {
					outlen=outpos+n+1024;
					outbuf=g_realloc(outbuf,outlen);
				}
				memcpy(outbuf+outpos,rdbuf,n);
				outpos+=n;
			}
			if (MbrpipeSpeakerSingleShot) {
				close(from_tts[0]);
			}
			if (!outbuf) continue;
			outbuf[outpos]=0;


			for (;;) {
				if (mbrpipe_stop) break;
				if (MbrpipeCleanOutput) {
					char *c=mbrpipe_cleaned_buffer(outbuf);
					g_free(outbuf);
					outbuf=c;
				}
				otts_mbrolaSpeak(omh,outbuf,mbrpipe_rate,mbrpipe_pitch,mbrpipe_volume,ensure(MbrpipeContrastLevel,-100,100));
				g_free(outbuf);
				outlen=0;
				outpos=0;
				outbuf=NULL;
				if (mbrpipe_stop || MbrpipeSpeakerSingleShot) break;
				n=read(from_tts[0],rdbuf,4096);
				if (n<0) break;
				outlen=n+1024;
				outbuf=g_malloc(outlen);
				memcpy(outbuf+outpos,rdbuf,n);
				outpos=n;
				for (;;) {
					n=read(from_tts[0],rdbuf,4096);
					if (n<=0) break;
					if (outpos+n+1>=outlen) {
						outlen=outpos+n+1024;
						outbuf=g_realloc(outbuf,outlen);
					}
					memcpy(outbuf+outpos,rdbuf,n);
					outpos+=n;
				}
				outbuf[outpos]=0;
			}
			if (mbrpipe_stop) {
				log_msg(OTTS_LOG_DEBUG, MODULE_NAME
				        ": Stop in child, terminating");
				if (outbuf) g_free(outbuf);
				mbrpipe_speaking = 0;
				module_report_event_stop();
				break;
			}
		}
		mbrpipe_speaking = 0;
		mbrpipe_stop = 0;
	}
	mbrpipe_speaking = 0;

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": speaking thread ended...\n");

	pthread_exit(NULL);
}

static void mbrpipe_play_icon(char *icon)
{
#if HAVE_SNDFILE
	int subformat;
	sf_count_t items;
	sf_count_t readcount;
	SNDFILE *sf;
	SF_INFO sfinfo;
	char filename[256];
	sprintf(filename, "%s/%s",MbrpipeSoundIconFolder,icon);

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
		opentts_audio_set_volume(module_audio_id, mbrpipe_volume);
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


static void tts_start(void)
{
	char *args[32];
	int nargs;
	char *buf=strdup(MbrpipeSpeakerCommand);
	char *c;
	args[0]=buf;
	nargs=1;
	c=buf;
	while (*c && nargs<31) {
		while (*c && !isspace(*c)) c++;
		if (!*c) break;
		*c++=0;
		while (*c && isspace(*c)) c++;
		if (!*c) break;
		args[nargs++]=c;
	}
	args[nargs]=0;
	c=strrchr(args[0],'/');
	if (c) args[0]=c+1;
	close(to_tts[1]);
	close(from_tts[0]);
	dup2(to_tts[0],0);
	dup2(from_tts[1],1);
	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Command %s",buf);
	int i;
	for (i = 0; i < nargs; i++)
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Arg %s",args[i]);
	execvp(buf,args);
	exit(1);
}

static int tts_pid;

static int tts_fork(char **status_info)
{
	int pid;
	pipe(to_tts);
	pipe(from_tts);

	log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Fork now");
	pid=fork();
	if (!pid) {
		tts_start();
	}
	close(to_tts[0]);
	close(from_tts[1]);
	if (pid<0) {
		log_msg(OTTS_LOG_DEBUG, MODULE_NAME": Can't fork");
		if (status_info) *status_info = g_strdup("Can't fork.");
		close(to_tts[1]);
		close(from_tts[0]);
		return -1;
	}
	if (!MbrpipeSpeakerSingleShot) {
		fcntl(from_tts[0],F_SETFL,O_NONBLOCK);
		tts_pid=pid;
	}
	return pid;
}

