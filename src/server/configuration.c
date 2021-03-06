
/*
 * configuration.c - dotconf functions for OpenTTSd
 *
 * Copyright (C) 2010 OpenTTS developers
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <dotconf.h>

#include "openttsd.h"
#include "opentts/opentts_types.h"
#include <fdsetconv.h>
#include <logging.h>
#include "configuration.h"

static TFDSetClientSpecific *cl_spec_section;

/* Work around a limitation of the FATAL macro. */
static void cfg_fatal(const char *errmsg)
{
	fatal_error();
	log_msg(OTTS_LOG_CRIT, "Aborting: %s", errmsg);
	exit(EXIT_FAILURE);
}

/* == CONFIGURATION MANAGEMENT FUNCTIONS */

/* Add dotconf configuration option */
configoption_t *add_config_option(configoption_t * options,
				  int *num_config_options, char *name, int type,
				  dotconf_callback_t callback, info_t * info,
				  unsigned long context)
{
	configoption_t *opts;

	(*num_config_options)++;
	opts =
	    (configoption_t *) g_realloc(options,
					 (*num_config_options) *
					 sizeof(configoption_t));
	opts[*num_config_options - 1].name = g_strdup(name);
	opts[*num_config_options - 1].type = type;
	opts[*num_config_options - 1].callback = callback;
	opts[*num_config_options - 1].info = info;
	opts[*num_config_options - 1].context = context;
	return opts;
}

/* Free all configuration options. */
void free_config_options(configoption_t * opts, int *num)
{
	int i = 0;

	if (opts == NULL)
		return;

	for (i = 0; i <= (*num) - 1; i++) {
		g_free((char *)opts[i].name);
	}
	g_free(opts);
	*num = 0;
	opts = NULL;
}

/* == CALLBACK DEFINITION MACROS == */

#define GLOBAL_FDSET_OPTION_CB_STR(name, arg) \
   DOTCONF_CB(cb_ ## name) \
   { \
       assert(cmd->data.str != NULL); \
       if (!cl_spec_section) \
           GlobalFDSet.arg = g_strdup(cmd->data.str); \
       else \
           cl_spec_section->val.arg = g_strdup(cmd->data.str); \
       return NULL; \
   }

#define GLOBAL_FDSET_OPTION_CB_INT(name, arg, cond, str) \
   DOTCONF_CB(cb_ ## name) \
   { \
       int val = cmd->data.value; \
       if (!(cond)) FATAL(str); \
       if (!cl_spec_section) \
           GlobalFDSet.arg = val; \
       else \
           cl_spec_section->val.arg = val; \
       return NULL; \
   }

#define OPTION_CB_STR_M(name, arg) \
   DOTCONF_CB(cb_ ## name) \
   { \
       if (cl_spec_section) \
         FATAL("This command isn't allowed in a client specific section!"); \
       if (!options.arg ## _set) options.arg = g_strdup(cmd->data.str); \
       return NULL; \
   }

#define OPTION_CB_INT_M(name, arg, cond, str) \
   DOTCONF_CB(cb_ ## name) \
   { \
       int val = cmd->data.value; \
       if (cl_spec_section) \
         FATAL("This command isn't allowed in a client specific section!"); \
       if (!(cond)) FATAL(str); \
       if (!options.arg ## _set) options.arg = val; \
       return NULL; \
   }

#define OPTION_CB_STR(name, arg) \
   DOTCONF_CB(cb_ ## name) \
   { \
       if (cl_spec_section) \
         FATAL("This command isn't allowed in a client specific section!"); \
       options.arg = g_strdup(cmd->data.str); \
       return NULL; \
   }

#define OPTION_CB_INT(name, arg, cond, str) \
   DOTCONF_CB(cb_ ## name) \
   { \
       int val = cmd->data.value; \
       if (cl_spec_section) \
         FATAL("This command isn't allowed in a client specific section!"); \
       if (!(cond)) FATAL(str); \
       options.arg = val; \
       return NULL; \
   }

#define GLOBAL_SET_LOGLEVEL(name, arg, cond, str) \
   DOTCONF_CB(cb_ ## name) \
   { \
       int val = cmd->data.value; \
       if (cl_spec_section) \
         FATAL("This command isn't allowed in a client specific section!"); \
       if (!(cond)) FATAL(str); \
       if (!options.arg ## _set){ \
         options.arg = val; \
         GlobalFDSet.arg = val; \
       } \
       return NULL; \
   }

/* == CALLBACK DEFINITIONS == */
GLOBAL_FDSET_OPTION_CB_STR(DefaultModule, output_module)
GLOBAL_FDSET_OPTION_CB_STR(DefaultLanguage, msg_settings.voice.language)
GLOBAL_FDSET_OPTION_CB_STR(DefaultClientName, client_name)

GLOBAL_FDSET_OPTION_CB_STR(AudioOutputMethod, audio_output_method)
GLOBAL_FDSET_OPTION_CB_STR(AudioOSSDevice, audio_oss_device)
GLOBAL_FDSET_OPTION_CB_STR(AudioALSADevice, audio_alsa_device)
GLOBAL_FDSET_OPTION_CB_STR(AudioNASServer, audio_nas_server)
GLOBAL_FDSET_OPTION_CB_STR(AudioPulseServer, audio_pulse_server)
GLOBAL_FDSET_OPTION_CB_INT(AudioPulseMinLength, audio_pulse_min_length, 1, "")

GLOBAL_FDSET_OPTION_CB_INT(DefaultRate, msg_settings.rate,
                           (val >= OTTS_VOICE_RATE_MIN)
			   && (val <= OTTS_VOICE_RATE_MAX), "Rate out of range.")
GLOBAL_FDSET_OPTION_CB_INT(DefaultPitch, msg_settings.pitch,
                           (val >= OTTS_VOICE_PITCH_MIN)
			   && (val <= OTTS_VOICE_PITCH_MAX), "Pitch out of range.")
GLOBAL_FDSET_OPTION_CB_INT(DefaultVolume, msg_settings.volume,
                           (val >= OTTS_VOICE_VOLUME_MIN)
			   && (val <= OTTS_VOICE_VOLUME_MAX), "Volume out of range.")
GLOBAL_FDSET_OPTION_CB_INT(DefaultSpelling, msg_settings.spelling_mode, 1,
			   "Invalid spelling mode")
GLOBAL_FDSET_OPTION_CB_INT(DefaultPauseContext, pause_context, 1, "")

OPTION_CB_STR_M(CommunicationMethod, communication_method)
OPTION_CB_STR_M(SocketName, socket_name)
OPTION_CB_INT_M(LocalhostAccessOnly, localhost_access_only, val >= 0,
			 "Invalid access controll mode!")
OPTION_CB_INT_M(Port, port, val >= 0,
			"Invalid port number!")
GLOBAL_SET_LOGLEVEL(LogLevel, log_level, (val >= 0)
			&& (val <= 5), "Invalid log (verbosity) level!")
OPTION_CB_INT(MaxHistoryMessages, max_history_messages, val >= 0,
		      "Invalid parameter!")

DOTCONF_CB(cb_DefaultCapLetRecognition)
{
	char *errmsg = g_strdup_printf
	    ("Invalid value %s for the DefaultCapLetRecognition parameter in the configuration file.",
	     cmd->data.str);
	SPDCapitalLetters val;
	char *val_str = g_ascii_strdown(cmd->data.str, strlen(cmd->data.str));
	if (val_str == NULL)
		cfg_fatal(errmsg);
	val = str2recogn(val_str);
	if (val == SPD_CAP_ERR)
		cfg_fatal(errmsg);
	g_free(errmsg);
	g_free(val_str);
	if (!cl_spec_section)
		GlobalFDSet.msg_settings.cap_let_recogn = val;
	else
		cl_spec_section->val.msg_settings.cap_let_recogn = val;
	return NULL;
}

DOTCONF_CB(cb_DefaultPunctuationMode)
{
	char *errmsg =
	    g_strdup_printf
	    ("Invalid value %s for the DefaultPunctuationMode parameter in the configuration file.",
	     cmd->data.str);
	SPDPunctuation val;
	char *val_str = g_ascii_strdown(cmd->data.str, strlen(cmd->data.str));
	if (val_str == NULL)
		cfg_fatal(errmsg);
	val = str2punct(val_str);
	if (val == SPD_PUNCT_ERR)
		cfg_fatal(errmsg);
	g_free(errmsg);
	g_free(val_str);
	if (!cl_spec_section)
		GlobalFDSet.msg_settings.punctuation_mode = val;
	else
		cl_spec_section->val.msg_settings.punctuation_mode = val;
	return NULL;
}

DOTCONF_CB(cb_DefaultVoiceType)
{
	char *errmsg =
	    g_strdup_printf
	    ("Invalid value %s for the DefaultVoiceType parameter in the configuration file.",
	     cmd->data.str);
	SPDVoiceType val;
	char *val_str = g_ascii_strdown(cmd->data.str, strlen(cmd->data.str));
	if (val_str == NULL)
		cfg_fatal(errmsg);
	val = str2voice(val_str);
	if (val == SPD_NO_VOICE)
		cfg_fatal(errmsg);
	g_free(errmsg);
	g_free(val_str);
	if (!cl_spec_section)
		GlobalFDSet.msg_settings.voice_type = val;
	else
		cl_spec_section->val.msg_settings.voice_type = val;
	return NULL;
}

DOTCONF_CB(cb_DefaultPriority)
{
	char *errmsg =
	    g_strdup_printf
	    ("Invalid value %s for the DefaultPriority parameter in the configuration file.",
	     cmd->data.str);
	SPDPriority val;
	char *val_str = g_ascii_strdown(cmd->data.str, strlen(cmd->data.str));
	if (val_str == NULL)
		cfg_fatal(errmsg);
	val = str2priority(val_str);
	if (val == SPD_PRIORITY_ERR)
		cfg_fatal(errmsg);
	g_free(errmsg);
	g_free(val_str);
	if (!cl_spec_section)
		GlobalFDSet.priority = val;
	else
		cl_spec_section->val.priority = val;
	return NULL;
}

DOTCONF_CB(cb_LanguageDefaultModule)
{
	char *key;
	char *value;

	if (cmd->data.list[0] == NULL)
		FATAL("No language specified for LanguageDefaultModule");
	if (cmd->data.list[0] == NULL)
		FATAL("No module specified for LanguageDefaultModule");

	key = g_strdup(cmd->data.list[0]);
	value = g_strdup(cmd->data.list[1]);

	g_hash_table_insert(language_default_modules, key, value);

	return NULL;
}

DOTCONF_CB(cb_LogFile)
{
	/* This option is DEPRECATED. If it is specified, get the directory. */
	assert(cmd->data.str != NULL);
	g_free(options.log_dir);
	options.log_dir = g_path_get_dirname(cmd->data.str);

	log_msg(OTTS_LOG_ERR,
		"WARNING: The LogFile option is deprecated. Directory accepted but filename ignored");
	return NULL;
}

DOTCONF_CB(cb_LogDir)
{
	assert(cmd->data.str != NULL);

	if (strcmp(cmd->data.str, "default")) {
		// cmd->data.str different from "default"
		g_free(options.log_dir);
		options.log_dir = g_strdup(cmd->data.str);
	}

	return NULL;
}

DOTCONF_CB(cb_CustomLogFile)
{
	if (cmd->data.list[0] == NULL)
		FATAL("No log kind specified in CustomLogFile");
	if (cmd->data.list[1] == NULL)
		FATAL("No log file specified in CustomLogFile");

	g_free(options.custom_log_kind);
	g_free(options.custom_log_filename);
	options.custom_log_kind = g_strdup(cmd->data.list[0]);
	options.custom_log_filename = g_strdup(cmd->data.list[1]);

	return NULL;
}

DOTCONF_CB(cb_AddModule)
{
	char *module_name;
	char *module_prgname;
	char *module_cfgfile;
	char *module_dbgfile;

	OutputModule *cur_mod;

	if (cmd->data.list[0] != NULL)
		module_name = g_strdup(cmd->data.list[0]);
	else
		FATAL
		    ("No output module name specified in configuration under AddModule");

	module_prgname = cmd->data.list[1];
	module_cfgfile = cmd->data.list[2];

	module_dbgfile = g_strdup_printf("%s/%s.log", options.log_dir,
					 module_name);

	cur_mod =
	    load_output_module(module_name, module_prgname, module_cfgfile,
			       module_dbgfile);
	if (cur_mod == NULL) {
		log_msg(OTTS_LOG_NOTICE,
			"Couldn't load specified output module");
		return NULL;
	}

	log_msg(OTTS_LOG_DEBUG, "Module name=%s being inserted into hash table",
		cur_mod->name);
	assert(cur_mod->name != NULL);
	g_hash_table_insert(output_modules, g_strdup(module_name), cur_mod);

	g_free(module_dbgfile);
	g_free(module_name);

	return NULL;
}

/* == CLIENT SPECIFIC CONFIGURATION == */

#define SET_PAR(name, value) cl_spec->val.name = value;
#define SET_PAR_STR(name) cl_spec->val.name = NULL;
DOTCONF_CB(cb_BeginClient)
{
	TFDSetClientSpecific *cl_spec;

	if (cl_spec_section != NULL)
		FATAL
		    ("Configuration: Already in client specific section, can't open a new one!");

	if (cmd->data.str == NULL)
		FATAL
		    ("Configuration: You must specify some client's name for BeginClient");

	cl_spec =
	    (TFDSetClientSpecific *) g_malloc(sizeof(TFDSetClientSpecific));
	cl_spec->pattern = g_strdup(cmd->data.str);
	cl_spec_section = cl_spec;

	log_msg(OTTS_LOG_NOTICE, "Reading configuration for pattern %s",
		cl_spec->pattern);

	/*  Warning: If you modify this, you must also modify update_cl_settings() in set.c ! */
	SET_PAR(msg_settings.rate, (OTTS_VOICE_RATE_MIN - 1))
	SET_PAR(msg_settings.pitch, (OTTS_VOICE_PITCH_MIN - 1))
	SET_PAR(msg_settings.volume, (OTTS_VOICE_VOLUME_MIN - 1))
	SET_PAR(msg_settings.punctuation_mode, -1)
	SET_PAR(msg_settings.spelling_mode, -1)
	SET_PAR(msg_settings.voice_type, -1)
	SET_PAR(msg_settings.cap_let_recogn, -1)
	SET_PAR(pause_context, -1);
	SET_PAR(ssml_mode, -1);
	SET_PAR_STR(msg_settings.voice.language)
	SET_PAR_STR(output_module)

	return NULL;
}

#undef SET_PAR
#undef SET_PAR_STR

DOTCONF_CB(cb_EndClient)
{
	if (cl_spec_section == NULL)
		FATAL
		    ("Configuration: Already outside the client specific section!");

	client_specific_settings =
	    g_list_append(client_specific_settings, cl_spec_section);

	cl_spec_section = NULL;

	return NULL;
}

/* == CALLBACK FOR UNKNOWN OPTIONS == */

DOTCONF_CB(cb_unknown)
{
	log_msg(OTTS_LOG_WARN, "Unknown option in configuration!");
	return NULL;
}

/* == LOAD CALLBACKS == */

#define ADD_CONFIG_OPTION(name, arg_type) \
   options = add_config_option(options, num_options, #name, arg_type, cb_ ## name, 0, 0);

#define ADD_LAST_OPTION() \
   options = add_config_option(options, num_options, "", 0, NULL, NULL, 0);

configoption_t *load_config_options(int *num_options)
{
	configoption_t *options = NULL;

	cl_spec_section = NULL;

	ADD_CONFIG_OPTION(CommunicationMethod, ARG_STR);
	ADD_CONFIG_OPTION(SocketName, ARG_STR);
	ADD_CONFIG_OPTION(Port, ARG_INT);
	ADD_CONFIG_OPTION(LocalhostAccessOnly, ARG_INT);
	ADD_CONFIG_OPTION(LogFile, ARG_STR);
	ADD_CONFIG_OPTION(LogDir, ARG_STR);
	ADD_CONFIG_OPTION(CustomLogFile, ARG_LIST);
	ADD_CONFIG_OPTION(LogLevel, ARG_INT);
	ADD_CONFIG_OPTION(DefaultModule, ARG_STR);
	ADD_CONFIG_OPTION(LanguageDefaultModule, ARG_LIST);
	ADD_CONFIG_OPTION(DefaultRate, ARG_INT);
	ADD_CONFIG_OPTION(DefaultPitch, ARG_INT);
	ADD_CONFIG_OPTION(DefaultVolume, ARG_INT);
	ADD_CONFIG_OPTION(DefaultLanguage, ARG_STR);
	ADD_CONFIG_OPTION(DefaultPriority, ARG_STR);
	ADD_CONFIG_OPTION(MaxHistoryMessages, ARG_INT);
	ADD_CONFIG_OPTION(DefaultPunctuationMode, ARG_STR);
	ADD_CONFIG_OPTION(DefaultClientName, ARG_STR);
	ADD_CONFIG_OPTION(DefaultVoiceType, ARG_STR);
	ADD_CONFIG_OPTION(DefaultSpelling, ARG_TOGGLE);
	ADD_CONFIG_OPTION(DefaultCapLetRecognition, ARG_STR);
	ADD_CONFIG_OPTION(DefaultPauseContext, ARG_INT);
	ADD_CONFIG_OPTION(AddModule, ARG_LIST);

	ADD_CONFIG_OPTION(AudioOutputMethod, ARG_STR);
	ADD_CONFIG_OPTION(AudioOSSDevice, ARG_STR);
	ADD_CONFIG_OPTION(AudioALSADevice, ARG_STR);
	ADD_CONFIG_OPTION(AudioNASServer, ARG_STR);
	ADD_CONFIG_OPTION(AudioPulseServer, ARG_STR);
	ADD_CONFIG_OPTION(AudioPulseMinLength, ARG_INT);

	ADD_CONFIG_OPTION(BeginClient, ARG_STR);
	ADD_CONFIG_OPTION(EndClient, ARG_NONE);
	return options;

}

/* == DEFAULT OPTIONS == */

void load_default_global_set_options()
{
	GlobalFDSet.priority = SPD_MESSAGE;
	GlobalFDSet.msg_settings.punctuation_mode = SPD_PUNCT_NONE;
	GlobalFDSet.msg_settings.spelling_mode = SPD_SPELL_OFF;
	GlobalFDSet.msg_settings.rate = OTTS_VOICE_RATE_DEFAULT;
	GlobalFDSet.msg_settings.pitch = OTTS_VOICE_PITCH_DEFAULT;
	GlobalFDSet.msg_settings.volume = OTTS_VOICE_VOLUME_DEFAULT;
	GlobalFDSet.client_name = g_strdup("unknown:unknown:unknown");
	GlobalFDSet.msg_settings.voice.language = g_strdup(OPENTTSD_DEFAULT_LANGUAGE);
	GlobalFDSet.output_module = NULL;
	GlobalFDSet.msg_settings.voice_type = SPD_MALE1;
	GlobalFDSet.msg_settings.cap_let_recogn = SPD_CAP_NONE;
	GlobalFDSet.min_delay_progress = 2000;
	GlobalFDSet.pause_context = 0;
	GlobalFDSet.ssml_mode = SPD_DATA_TEXT;
	GlobalFDSet.notification = SPD_NOTHING;
	GlobalFDSet.log_level = options.log_level;

#ifdef __SUNPRO_C
/* Added by Willie Walker - default to OSS for Solaris */
	GlobalFDSet.audio_output_method = g_strdup("oss");
#else
	GlobalFDSet.audio_output_method = g_strdup("pulse,alsa");
#endif /* __SUNPRO_C */
	GlobalFDSet.audio_oss_device = g_strdup("/dev/dsp");
	GlobalFDSet.audio_alsa_device = g_strdup("default");
	GlobalFDSet.audio_nas_server = g_strdup("tcp/localhost:5450");
	GlobalFDSet.audio_pulse_server = g_strdup("default");
	GlobalFDSet.audio_pulse_min_length = 100;

	options.max_history_messages = 10000;

	/*
	 * Do not override options that were set from the command line.
	 */

	if (!options.log_level_set)
		options.log_level = 3;
	if (!options.communication_method)
		options.communication_method = g_strdup("unix_socket");
	if (!options.socket_name)
		options.socket_name = g_strdup("default");
	if (!options.port_set)
		options.port = OPENTTSD_DEFAULT_PORT;
	if (!options.localhost_access_only_set)
		options.localhost_access_only = 1;

	options.custom_log_filename = NULL;
}
