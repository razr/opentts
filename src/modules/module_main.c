/*
 * module_main.c - One way of doing main() in output modules.
 * 
 * Copyright (C) 2001, 2002, 2003, 2006 Brailcom, o.p.s.
 * Copyright (C) 2010 OpenTTS Developers
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <glib.h>
#include <dotconf.h>
#include <ltdl.h>

#include <logging.h>
#include <getline.h>
#include "opentts/opentts_synth_plugin.h"
#include "module_utils.h"

AudioID *module_audio_id;

int current_index_mark;

OTTS_MsgSettings msg_settings;
OTTS_MsgSettings msg_settings_old;

int LogLevel;
pthread_mutex_t module_stdout_mutex;

configoption_t *module_dc_options;
int module_num_dc_options;

int dispatch_cmd(otts_synth_plugin_t *synth, char *cmd_line)
{
	char *cmd = NULL;
	size_t cmd_len;
	char *msg = NULL;

	cmd_len = strcspn(cmd_line, " \t\n\r\f");
	cmd = g_strndup(cmd_line, cmd_len);
	pthread_mutex_lock(&module_stdout_mutex);

	if (!strcasecmp("audio", cmd)) {
		msg = do_audio(synth);
	} else if (!strcasecmp("set", cmd)) {
		msg = do_set(synth);
	} else if (!strcasecmp("speak", cmd)) {
		msg = do_speak(synth);
	} else if (!strcasecmp("key", cmd)) {
		msg = do_key(synth);
	} else if (!strcasecmp("sound_icon", cmd)) {
		msg = do_sound_icon(synth);
	} else if (!strcasecmp("char", cmd)) {
		msg = do_char(synth);
	} else if (!strcasecmp("pause", cmd)) {
		do_pause(synth);
	} else if (!strcasecmp("stop", cmd)) {
		do_stop(synth);
	} else if (!strcasecmp("list_voices", cmd)) {
		msg = do_list_voices(synth);
	} else if (!strcasecmp("loglevel", cmd)) {
		msg = do_loglevel(synth);
	} else if (!strcasecmp("debug", cmd)) {
		msg = do_debug(synth, cmd_line);
	} else if (!strcasecmp("quit", cmd)) {
		do_quit(synth);
	} else {
/*should we log?*/
		printf("300 ERR UNKNOWN COMMAND\n");
		fflush(stdout);
	}

	if (msg != NULL) {
		if (0 > printf("%s\n", msg)) {
			log_msg(OTTS_LOG_CRIT, "Broken pipe, exiting...\n");
			synth->close(2);
		}
		fflush(stdout);
		g_free(msg);
	}

	pthread_mutex_unlock(&module_stdout_mutex);
	g_free(cmd);

	return (0);
}

int main(int argc, char *argv[])
{
	char *cmd_buf;
	int ret;
	int ret_init;
	size_t n;
	char *configfilename;
	configfile_t *configfile;
	otts_synth_plugin_t *synth;
	char *status_info = NULL;

	g_thread_init(NULL);
	init_logging();
	open_log("stderr", 3);

	synth = synth_plugin_get();

	/* Initialize ltdl's list of preloaded audio backends. */
	LTDL_SET_PRELOADED_SYMBOLS();
	module_num_dc_options = 0;
	module_audio_id = 0;

	if (argc >= 2) {
		configfilename = g_strdup(argv[1]);
	} else {
		configfilename = NULL;
	}

	ret = synth->load();
	if (ret == -1)
		synth->close(1);

	if (configfilename != NULL) {
		/* Add the LAST option */
		module_dc_options = module_add_config_option(module_dc_options,
							     &module_num_dc_options,
							     "", 0, NULL, NULL,
							     0);

		configfile =
		    dotconf_create(configfilename, module_dc_options, 0,
				   CASE_INSENSITIVE);
		if (configfile) {
			if (dotconf_command_loop(configfile) == 0) {
				log_msg(OTTS_LOG_CRIT,
					"Error reading config file\n");
				synth->close(1);
			}
			dotconf_cleanup(configfile);
			log_msg(OTTS_LOG_NOTICE,
				"Configuration (pre) has been read from \"%s\"\n",
				configfilename);

			g_free(configfilename);
		} else {
			log_msg(OTTS_LOG_ERR,
				"Can't read specified config file!\n");
		}
	} else {
		log_msg(OTTS_LOG_WARN,
			"No config file specified, using defaults...\n");
	}

	ret_init = synth->init(&status_info);

	if (status_info == NULL) {
		status_info = g_strdup("unknown, was not set by module");
	}

	cmd_buf = NULL;
	n = 0;
	ret = otts_getline(&cmd_buf, &n, stdin);
	if (ret == -1) {
		log_msg(OTTS_LOG_CRIT,
			"Broken pipe when reading INIT, exiting... \n");
		synth->close(2);
	}

	if (!strcmp(cmd_buf, "INIT\n")) {
		if (ret_init != 0) {
			printf("399-%s\n", status_info);
			ret = printf("%s\n", "399 ERR CANT INIT MODULE");
			g_free(status_info);
			return -1;
		}

		printf("299-%s\n", status_info);
		ret = printf("%s\n", "299 OK LOADED SUCCESSFULLY");

		if (ret < 0) {
			log_msg(OTTS_LOG_CRIT, "Broken pipe, exiting...\n");
			synth->close(2);
		}
		fflush(stdout);
	} else {
		log_msg(OTTS_LOG_ERR,
			"ERROR: Wrong communication from module client: didn't call INIT\n");
		synth->close(3);
	}

	g_free(status_info);
	xfree(cmd_buf);

	while (1) {
		cmd_buf = NULL;
		n = 0;
		ret = otts_getline(&cmd_buf, &n, stdin);
		if (ret == -1) {
			log_msg(OTTS_LOG_CRIT, "Broken pipe, exiting... \n");
			synth->close(2);
		}

		log_msg(OTTS_LOG_INFO, "CMD: <%s>", cmd_buf);

		dispatch_cmd(synth, cmd_buf);

		xfree(cmd_buf);
	}
}
