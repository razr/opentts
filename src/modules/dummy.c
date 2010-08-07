
/*
 * dummy.c - OpenTTS dummy output module
 *
 * A simplific output module that just tries to play an
 * an error message in various ways.
 *
 * Copyright (C) 2008 Brailcom, o.p.s.
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

#include <glib.h>

#include<logging.h>
#include "opentts/opentts_types.h"
#include "module_utils.h"

#define MODULE_NAME     "dummy"
#define MODULE_VERSION  "0.1"

#define Debug 1

/* Thread and process control */
static int dummy_speaking = 0;

static pthread_t dummy_speak_thread;
static pid_t dummy_pid;
static sem_t *dummy_semaphore;

/* Internal functions prototypes */
static void *_dummy_speak(void *);
static void _dummy_child();

/* Fill the module_info structure with pointers to this modules functions */

/* Public functions */
int module_load(void)
{

	init_settings_tables();

	return 0;
}

int module_init(char **status_info)
{
	int ret;

	*status_info = NULL;

	dummy_semaphore = module_semaphore_init();

	log_msg(OTTS_LOG_NOTICE,
		"Dummy: creating new thread for dummy_speak\n");
	dummy_speaking = 0;
	ret = pthread_create(&dummy_speak_thread, NULL, _dummy_speak, NULL);
	if (ret != 0) {
		log_msg(OTTS_LOG_ERR, "Dummy: thread failed\n");
		*status_info = g_strdup("The module couldn't initialize threads"
					"This can be either an internal problem or an"
					"architecture problem. If you are sure your architecture"
					"supports threads, please report a bug.");
		return -1;
	}

	*status_info = g_strdup("Everything ok so far.");

	log_msg(OTTS_LOG_INFO, "Ok, now debugging");

	return 0;
}

int module_audio_init(char **status_info)
{
	status_info = NULL;
	return 0;
}

SPDVoice **module_list_voices(void)
{
	return NULL;
}

int module_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{

	log_msg(OTTS_LOG_INFO, "speak()\n");

	if (dummy_speaking) {
		log_msg(OTTS_LOG_DEBUG, "Speaking when requested to write");
		return 0;
	}

	if (module_write_data_ok(data) != 0)
		return -1;

	log_msg(OTTS_LOG_DEBUG, "Requested data: |%s|\n", data);

	/* Send semaphore signal to the speaking thread */
	dummy_speaking = 1;
	sem_post(dummy_semaphore);

	log_msg(OTTS_LOG_DEBUG, "Dummy: leaving write() normaly\n\r");
	return bytes;
}

int module_stop(void)
{
	log_msg(OTTS_LOG_DEBUG,
		"dummy: stop(), dummy_speaking=%d, dummy_pid=%d\n",
		dummy_speaking, dummy_pid);

	if (dummy_speaking && dummy_pid) {
		log_msg(OTTS_LOG_DEBUG,
			"dummy: stopping process group pid %d\n", dummy_pid);
		kill(-dummy_pid, SIGKILL);
	}
	log_msg(OTTS_LOG_DEBUG, "Already stopped, no action");
	return 0;
}

size_t module_pause(void)
{
	log_msg(OTTS_LOG_INFO, "pause requested\n");
	if (dummy_speaking) {
		log_msg(OTTS_LOG_WARN, "Dummy module can't pause\n");
		return 0;
	} else {
		return -1;
	}
}

char *module_is_speaking(void)
{
	return NULL;
}

void module_close(int status)
{
	log_msg(OTTS_LOG_NOTICE, "dummy: close()\n");

	if (dummy_speaking) {
		module_stop();
	}

	if (module_terminate_thread(dummy_speak_thread) != 0)
		exit(1);

	exit(status);
}

/* Internal functions */

void *_dummy_speak(void *nothing)
{
	int status;

	log_msg(OTTS_LOG_NOTICE, "dummy: speaking thread starting.......\n");

	set_speaking_thread_parameters();

	while (1) {
		sem_wait(dummy_semaphore);
		log_msg(OTTS_LOG_DEBUG, "Semaphore on\n");
		module_report_event_begin();

		/* Create a new process so that we could send it signals */
		dummy_pid = fork();

		switch (dummy_pid) {
		case -1:
			log_msg(OTTS_LOG_ERR,
				"Can't say the message. fork() failed!\n");
			dummy_speaking = 0;
			continue;

		case 0:
			{

				/* Set this process as a process group leader (so that SIGKILL
				   is also delivered to the child processes created by system()) */
				if (setpgid(0, 0) == -1)
					log_msg(OTTS_LOG_ERR,
						"Can't set myself as project group leader!");

				log_msg(OTTS_LOG_NOTICE, "Starting child...\n");
				_dummy_child();
			}
			break;

		default:
			/* This is the parent. Send data to the child. */

			log_msg(OTTS_LOG_INFO, "Waiting for child...");
			waitpid(dummy_pid, &status, 0);
			dummy_speaking = 0;

			log_msg(OTTS_LOG_NOTICE, "Child exited");

			// Report CANCEL if the process was signal-terminated
			// and END if it terminated normally
			if (WIFSIGNALED(status))
				module_report_event_stop();
			else
				module_report_event_end();

			log_msg(OTTS_LOG_INFO,
				"child terminated -: status:%d signal?:%d signal number:%d.\n",
				WIFEXITED(status), WIFSIGNALED(status),
				WTERMSIG(status));
		}
	}

	dummy_speaking = 0;

	log_msg(OTTS_LOG_NOTICE, "dummy: speaking thread ended.......\n");

	pthread_exit(NULL);
}

void _dummy_child()
{
	sigset_t some_signals;

	int ret;
	char *dummy_fname, *try1, *try2, *try3;

	sigfillset(&some_signals);
	module_sigunblockusr(&some_signals);

	log_msg(OTTS_LOG_INFO, "Entering child loop\n");
	/* Read the waiting data */

	dummy_fname = g_strdup_printf(DATADIR"/%s-dummy-message.wav",
	                              msg_settings.voice.language);
	if (!g_file_test(dummy_fname, G_FILE_TEST_EXISTS)) {
		g_free (dummy_fname);
		dummy_fname = g_strdup(DATADIR"/"OPENTTSD_DEFAULT_LANGUAGE
		                       "-dummy-message.wav");
	}

	try1 =
	    g_strdup_printf("play %s > /dev/null 2> /dev/null", dummy_fname);
	try2 =
	    g_strdup_printf("aplay %s > /dev/null 2> /dev/null", dummy_fname);
	try3 =
	    g_strdup_printf("paplay %s > /dev/null 2> /dev/null", dummy_fname);

	log_msg(OTTS_LOG_DEBUG, "child: synth commands = |%s|%s|%s|", try1,
		try2, try3);
	log_msg(OTTS_LOG_INFO, "Speaking in child...");
	module_sigblockusr(&some_signals);

	ret = system(try1);
	log_msg(OTTS_LOG_NOTICE, "Executed shell command '%s' returned with %d",
		try1, ret);
	if ((ret != 0)) {
		log_msg(OTTS_LOG_WARN,
			"Execution failed, trying seccond command");
		ret = system(try2);
		log_msg(OTTS_LOG_INFO,
			"Executed shell command '%s' returned with %d", try1,
			ret);
		if ((ret != 0)) {
			log_msg(OTTS_LOG_WARN,
				"Execution failed, trying third command");
			ret = system(try3);
			log_msg(OTTS_LOG_NOTICE,
				"Executed shell command '%s' returned with %d",
				try1, ret);
			if ((ret != 0) && (ret != 256)) {
				log_msg(OTTS_LOG_ERR, "Failed, giving up.");
			}
		}
	}

	module_sigunblockusr(&some_signals);

	g_free (dummy_fname);
	g_free(try1);
	g_free(try2);
	g_free(try3);

	log_msg(OTTS_LOG_NOTICE, "Done, exiting from child.");
	exit(0);
}
