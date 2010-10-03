/*
 * module_utils.h - Module utilities
 *           Functions to help writing output modules for OpenTTS
 * Copyright (C) 2003, 2004, 2007 Brailcom, o.p.s.
 * Copyright (c) 2010 OpenTTS Developers
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

#ifndef __MODULE_UTILS_H
#define __MODULE_UTILS_H

#include <semaphore.h>
#include <dotconf.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <dotconf.h>

#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <def.h>
#include <opentts/opentts_types.h>
#include "opentts/opentts_synth_plugin.h"
#include "audio.h"

extern AudioID *module_audio_id;

extern OTTS_MsgSettings msg_settings;
extern OTTS_MsgSettings msg_settings_old;

extern pthread_mutex_t module_stdout_mutex;
extern int LogLevel;

extern configoption_t *module_dc_options;
extern int module_num_dc_options;

void clean_old_settings_table(void);
void init_settings_tables(void);

#define UPDATE_PARAMETER(value, setter) \
  if (msg_settings_old.value != msg_settings.value) \
    { \
      msg_settings_old.value = msg_settings.value; \
      setter (msg_settings.value); \
    }

#define UPDATE_STRING_PARAMETER(value, setter) \
  if (msg_settings_old.value == NULL || msg_settings.value == NULL \
         || strcmp (msg_settings_old.value, msg_settings.value)) \
    { \
      if (msg_settings_old.value != NULL) \
      { \
        g_free (msg_settings_old.value); \
	msg_settings_old.value = NULL; \
      } \
      if (msg_settings.value != NULL) \
      { \
        msg_settings_old.value = g_strdup (msg_settings.value); \
        setter (msg_settings.value); \
      } \
    }

#define CHILD_SAMPLE_BUF_SIZE 16384

typedef struct {
	int pc[2];
	int cp[2];
} TModuleDoublePipe;

void xfree(void *data);

int module_get_message_part(const char *message, char *part,
			    unsigned int *pos, size_t maxlen,
			    const char *dividers);

void set_speaking_thread_parameters(void);

void module_parent_dp_init(TModuleDoublePipe dpipe);
void module_child_dp_init(TModuleDoublePipe dpipe);
void module_parent_dp_close(TModuleDoublePipe dpipe);
void module_child_dp_close(TModuleDoublePipe dpipe);

void module_child_dp_write(TModuleDoublePipe dpipe, const char *msg,
			   size_t bytes);
int module_parent_dp_write(TModuleDoublePipe dpipe, const char *msg,
			   size_t bytes);
int module_parent_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen);
int module_child_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen);

void module_signal_end(void);

void module_strip_punctuation_default(char *buf);
void module_strip_punctuation_some(char *buf, char *punct_some);
char *module_strip_ssml(char *buf);

void module_sigblockall(void);
void module_sigblockusr(sigset_t * signal_set);
void module_sigunblockusr(sigset_t * signal_set);
void module_signal_end(void);

gchar *do_message(otts_synth_plugin_t *synth, SPDMessageType msgtype);
gchar *do_speak(otts_synth_plugin_t *synth);
gchar *do_sound_icon(otts_synth_plugin_t *synth);
gchar *do_char(otts_synth_plugin_t *synth);
gchar *do_key(otts_synth_plugin_t *synth);
void do_stop(otts_synth_plugin_t *synth);
void do_pause(otts_synth_plugin_t *synth);
gchar *do_list_voices(otts_synth_plugin_t *synth);
gchar *do_set(otts_synth_plugin_t *synth);
gchar *do_audio(otts_synth_plugin_t *synth);
gchar *do_loglevel(otts_synth_plugin_t *synth);
gchar *do_debug(otts_synth_plugin_t *synth, char *cmd_buf);
void do_quit(otts_synth_plugin_t *synth);

typedef void (*TChildFunction) (TModuleDoublePipe dpipe, const size_t maxlen);
typedef size_t(*TParentFunction) (TModuleDoublePipe dpipe, const char *message,
				  const SPDMessageType msgtype,
				  const size_t maxlen, const char *dividers,
				  int *pause_requested);

void module_speak_thread_wfork(sem_t * semaphore, pid_t * process_pid,
			       TChildFunction child_function,
			       TParentFunction parent_function,
			       int *speaking_flag, char **message,
			       const SPDMessageType * msgtype,
			       const size_t maxlen, const char *dividers,
			       size_t * module_position, int *pause_requested);

size_t module_parent_wfork(TModuleDoublePipe dpipe, const char *message,
			   SPDMessageType msgtype, const size_t maxlen,
			   const char *dividers, int *pause_requested);

int module_parent_wait_continue(TModuleDoublePipe dpipe);

int module_terminate_thread(pthread_t thread);
sem_t *module_semaphore_init();
char *module_recode_to_iso(char *data, int bytes, char *language,
			   char *fallback);
int semaphore_post(int sem_id);

void *module_get_ht_option(GHashTable * hash_table, const char *key);

configoption_t *module_add_config_option(configoption_t * options,
					 int *num_config_options, char *name,
					 int type, dotconf_callback_t callback,
					 info_t * info, unsigned long context);

/* --- MODULE DOTCONF OPTIONS DEFINITION AND REGISTRATION --- */

#define MOD_OPTION_1_STR(name) \
    static char *name = NULL; \
    DOTCONF_CB(name ## _cb) \
    { \
        g_free(name); \
        if (cmd->data.str != NULL) \
            name = g_strdup(cmd->data.str); \
        return NULL; \
    }

#define MOD_OPTION_1_INT(name) \
    static int name; \
    DOTCONF_CB(name ## _cb) \
    { \
        name = cmd->data.value; \
        return NULL; \
    }

/* TODO: Switch this to real float, not /100 integer,
   as soon as DotConf supports floats */
#define MOD_OPTION_1_FLOAT(name) \
    static float name; \
    DOTCONF_CB(name ## _cb) \
    { \
        name = ((float) cmd->data.value) / ((float) 100); \
        return NULL; \
    }

#define MOD_OPTION_2(name, arg1, arg2) \
    typedef struct{ \
        char* arg1; \
        char* arg2; \
    }T ## name; \
    T ## name name; \
    \
    DOTCONF_CB(name ## _cb) \
    { \
        if (cmd->data.list[0] != NULL) \
            name.arg1 = g_strdup(cmd->data.list[0]); \
        if (cmd->data.list[1] != NULL) \
            name.arg2 = g_strdup(cmd->data.list[1]); \
        return NULL; \
    }

#define MOD_OPTION_2_HT(name, arg1, arg2) \
    typedef struct{ \
        char* arg1; \
        char* arg2; \
    }T ## name; \
    GHashTable *name; \
    \
    DOTCONF_CB(name ## _cb) \
    { \
        T ## name *new_item; \
        char* new_key; \
        new_item = (T ## name *) g_malloc(sizeof(T ## name)); \
        if (cmd->data.list[0] == NULL) return NULL; \
        new_item->arg1 = g_strdup(cmd->data.list[0]); \
        new_key = g_strdup(cmd->data.list[0]); \
        if (cmd->data.list[1] != NULL) \
           new_item->arg2 = g_strdup(cmd->data.list[1]); \
        else \
            new_item->arg2 = NULL; \
        g_hash_table_insert(name, new_key, new_item); \
        return NULL; \
    }

#define MOD_OPTION_3_HT(name, arg1, arg2, arg3) \
    typedef struct{ \
        char* arg1; \
        char* arg2; \
        char *arg3; \
    }T ## name; \
    GHashTable *name; \
    \
    DOTCONF_CB(name ## _cb) \
    { \
        T ## name *new_item; \
        char* new_key; \
        new_item = (T ## name *) g_malloc(sizeof(T ## name)); \
        if (cmd->data.list[0] == NULL) return NULL; \
        new_item->arg1 = g_strdup(cmd->data.list[0]); \
        new_key = g_strdup(cmd->data.list[0]); \
        if (cmd->data.list[1] != NULL) \
           new_item->arg2 = g_strdup(cmd->data.list[1]); \
        else \
            new_item->arg2 = NULL; \
        if (cmd->data.list[2] != NULL) \
           new_item->arg3 = g_strdup(cmd->data.list[2]); \
        else \
            new_item->arg3 = NULL; \
        g_hash_table_insert(name, new_key, new_item); \
        return NULL; \
    }

#define MOD_OPTION_4_HT(name, arg1, arg2, arg3, arg4) \
    typedef struct{ \
        char* arg1; \
        char* arg2; \
        char *arg3; \
	char *arg4; \
    }T ## name; \
    GHashTable *name; \
    \
    DOTCONF_CB(name ## _cb) \
    { \
        T ## name *new_item; \
        char* new_key; \
        new_item = (T ## name *) g_malloc(sizeof(T ## name)); \
        if (cmd->data.list[0] == NULL) return NULL; \
        new_item->arg1 = g_strdup(cmd->data.list[0]); \
        new_key = g_strdup(cmd->data.list[0]); \
        if (cmd->data.list[1] != NULL) \
           new_item->arg2 = g_strdup(cmd->data.list[1]); \
        else \
            new_item->arg2 = NULL; \
        if (cmd->data.list[2] != NULL) \
           new_item->arg3 = g_strdup(cmd->data.list[2]); \
        else \
            new_item->arg3 = NULL; \
        if (cmd->data.list[3] != NULL) \
           new_item->arg4 = g_strdup(cmd->data.list[3]); \
        else \
            new_item->arg4 = NULL; \
        g_hash_table_insert(name, new_key, new_item); \
        return NULL; \
    }

#define MOD_OPTION_1_STR_REG(name, default) \
    if (default != NULL) name = g_strdup(default); \
    else name = NULL; \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_STR, name ## _cb, NULL, 0);

#define MOD_OPTION_1_INT_REG(name, default) \
    name = default; \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_INT, name ## _cb, NULL, 0);

/* TODO: Switch this to real float, not /100 integer,
   as soon as DotConf supports floats */
#define MOD_OPTION_1_FLOAT_REG(name, default) \
    name = default; \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_INT, name ## _cb, NULL, 0);

#define MOD_OPTION_MORE_REG(name) \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_LIST, name ## _cb, NULL, 0);

#define MOD_OPTION_HT_REG(name) \
    name = g_hash_table_new(g_str_hash, g_str_equal); \
    module_dc_options = module_add_config_option(module_dc_options, \
                                     &module_num_dc_options, #name, \
                                     ARG_LIST, name ## _cb, NULL, 0);

/* --- DEBUGGING SUPPORT  ---*/

#define DECLARE_DEBUG() \
    DOTCONF_CB(LogLevel ## _cb) \
    { \
        open_log("stderr", cmd->data.value); \
        return NULL; \
    }

#define REGISTER_DEBUG() \
    MOD_OPTION_1_INT_REG(LogLevel, 0); \

/* --- INDEX MARKING --- */

#define INDEX_MARK_BODY_LEN 6
#define INDEX_MARK_BODY "__spd_"

void module_report_index_mark(char *mark);
void module_report_event_begin(void);
void module_report_event_end(void);
void module_report_event_stop(void);
void module_report_event_pause(void);
int module_utils_init(void);
int module_audio_init_spd(char **status_info);
int module_audio_init(char **status_info);

/* Prototypes from module_utils_addvoice.c */
void module_register_settings_voices(void);
char *module_getvoice(char *language, SPDVoiceType voice);

/* exit on fatal error with a message */
void fatal(char *msg);

/* replace for stupid asserts */

int ensure(int v,int m1,int m2);

#endif /* #ifndef __MODULE_UTILS_H */
