/*
 * module_utils.c - Module utilities
 *           Functions to help writing output modules for OpenTTS
 * Copyright (C) 2003,2006, 2007 Brailcom, o.p.s.
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
#include <fdsetconv.h>
#include <getline.h>
#include "module_utils.h"

static char *module_audio_pars[10];

void xfree(void *data)
{
	if (data != NULL)
		free(data);
}

gchar *do_message(SPDMessageType msgtype)
{
	int ret;
	char *cur_line;
	GString *msg;
	size_t n;
	int nlines = 0;

	msg = g_string_new("");

	printf("202 OK RECEIVING MESSAGE\n");
	fflush(stdout);

	while (1) {
		cur_line = NULL;
		n = 0;
		ret = otts_getline(&cur_line, &n, stdin);
		nlines++;
		if (ret == -1)
			return g_strdup("401 ERROR INTERNAL");

		if (!strcmp(cur_line, "..\n")) {
			xfree(cur_line);
			cur_line = strdup(".\n");
		} else if (!strcmp(cur_line, ".\n")) {
			/* Strip the trailing \n */
			msg->str[strlen(msg->str) - 1] = 0;
			xfree(cur_line);
			break;
		}
		g_string_append(msg, cur_line);
		xfree(cur_line);
	}

	if ((msgtype != SPD_MSGTYPE_TEXT) && (nlines > 2)) {
		return g_strdup("305 DATA MORE THAN ONE LINE");
	}

	if ((msgtype == SPD_MSGTYPE_CHAR) && (!strcmp(msg->str, "space"))) {
		g_string_free(msg, 1);
		msg = g_string_new(" ");
	}

	ret = module_speak(msg->str, strlen(msg->str), msgtype);

	g_string_free(msg, 1);
	if (ret <= 0)
		return g_strdup("301 ERROR CANT SPEAK");

	return g_strdup("200 OK SPEAKING");
}

gchar *do_speak(void)
{
	return do_message(SPD_MSGTYPE_TEXT);
}

gchar *do_sound_icon(void)
{
	return do_message(SPD_MSGTYPE_SOUND_ICON);
}

gchar *do_char(void)
{
	return do_message(SPD_MSGTYPE_CHAR);
}

gchar *do_key(void)
{
	return do_message(SPD_MSGTYPE_KEY);
}

void do_stop(void)
{
	module_stop();
	return;
}

void do_pause(void)
{
	int ret;

	ret = module_pause();
	if (ret) {
		dbg("WARNING: Can't pause");
		return;
	}

	return;
}

#define SETTINGS_OK 0
#define SETTINGS_BAD_SYNTAX 1
#define SETTINGS_BAD_ITEM 2

/*
 * set_numeric_parameter handles settings such as pitch, rate, and volume.
 * It converts its num_text argument to an integer.  If that integer is
 * in the range [min_val, max_val], then the number is stored in *target.
 * It returns SETTINGS_OK (which is 0) on success, and it returns
 * SETTINGS_BAD_ITEM on failure.
 */

static int set_numeric_parameter(char *num_text, int *target,
				 int min_val, int max_val)
{
	int err = SETTINGS_OK;
	char *tptr = NULL;
	int number = strtol(num_text, &tptr, 10);
	if (tptr == num_text)
		err = SETTINGS_BAD_ITEM;
	else if ((number < min_val) || (number > max_val))
		err = SETTINGS_BAD_ITEM;
	else {
		*target = number;
	}
	return err;
}

gchar *do_set(void)
{
	char *cur_item = NULL;
	char *cur_value = NULL;
	char *line = NULL;
	int ret;
	size_t n;
	int err = 0;		/* Error status */

	printf("203 OK RECEIVING SETTINGS\n");
	fflush(stdout);

	while (1) {
		line = NULL;
		n = 0;
		ret = otts_getline(&line, &n, stdin);
		if (ret == -1) {
			err = 1;
			break;
		}
		if (!strcmp(line, ".\n")) {
			xfree(line);
			break;
		}
		if (!err) {
			cur_item = strtok(line, "=");
			if (cur_item == NULL) {
				err = 1;
				xfree(line);
				continue;
			}
			cur_value = strtok(NULL, "\n");
			if (cur_value == NULL) {
				err = 1;
				xfree(line);
				continue;
			}

			if (!strcmp(cur_item, "rate")) {
				err =
				    set_numeric_parameter(cur_value,
							  &msg_settings.rate,
							  OTTS_VOICE_RATE_MIN,
				                          OTTS_VOICE_RATE_MAX);
			} else if (!strcmp(cur_item, "pitch")) {
				err =
				    set_numeric_parameter(cur_value,
							  &msg_settings.pitch,
							  OTTS_VOICE_PITCH_MIN,
				                          OTTS_VOICE_PITCH_MAX);
			} else if (!strcmp(cur_item, "volume")) {
				err =
				    set_numeric_parameter(cur_value,
							  &msg_settings.volume,
							  OTTS_VOICE_VOLUME_MIN,
				                          OTTS_VOICE_VOLUME_MAX);
			} else if (!strcmp(cur_item, "punctuation_mode")) {
				ret = str2punct(cur_value);
				if (ret != SPD_PUNCT_ERR)
					msg_settings.punctuation_mode = ret;
				else
					err = 2;
			} else if (!strcmp(cur_item, "spelling_mode")) {
				ret = str2spell(cur_value);
				if (ret != SPD_SPELL_ERR)
					msg_settings.spelling_mode = ret;
				else
					err = 2;
			} else if (!strcmp(cur_item, "cap_let_recogn")) {
				ret = str2recogn(cur_value);
				if (ret != SPD_CAP_ERR)
					msg_settings.cap_let_recogn = ret;
				else
					err = 2;
			} else if (!strcmp(cur_item, "voice")) {
				ret = str2voice(cur_value);
				msg_settings.voice_type = ret;
			} else if (!strcmp(cur_item, "synthesis_voice")) {
				g_free(msg_settings.voice.name);
				if (!strcmp(cur_value, "NULL"))
					msg_settings.voice.name = NULL;
				else
					msg_settings.voice.name =
					    g_strdup(cur_value);
			} else if (!strcmp(cur_item, "language")) {
				g_free(msg_settings.voice.language);
				if (!strcmp(cur_value, "NULL"))
					msg_settings.voice.language = NULL;
				else
					msg_settings.voice.language =
					    g_strdup(cur_value);
			} else
				err = 2;	/* Unknown parameter */
		}
		xfree(line);
	}

	if (err == 0)
		return g_strdup("203 OK SETTINGS RECEIVED");
	if (err == 1)
		return g_strdup("302 ERROR BAD SYNTAX");
	if (err == 2)
		return g_strdup("303 ERROR INVALID PARAMETER OR VALUE");

	return g_strdup("401 ERROR INTERNAL");	/* Can't be reached */
}

/*
 * set_audio_parameter: sets module_audio_pars[parameter_index] to
 * current_value.
 */

static void set_audio_parameter(char *cur_value, int parameter_index)
{
	g_free(module_audio_pars[parameter_index]);
	if (!strcmp(cur_value, "NULL"))
		module_audio_pars[parameter_index] = NULL;
	else
		module_audio_pars[parameter_index] = g_strdup(cur_value);
}

gchar *do_audio(void)
{
	char *cur_item = NULL;
	char *cur_value = NULL;
	char *line = NULL;
	int ret;
	size_t n;
	int err = 0;		/* Error status */
	char *status;
	char *msg;

	printf("207 OK RECEIVING AUDIO SETTINGS\n");
	fflush(stdout);

	while (1) {
		line = NULL;
		n = 0;
		ret = otts_getline(&line, &n, stdin);
		if (ret == -1) {
			err = 1;
			break;
		}
		if (!strcmp(line, ".\n")) {
			xfree(line);
			break;
		}
		if (!err) {
			cur_item = strtok(line, "=");
			if (cur_item == NULL) {
				err = 1;
				xfree(line);
				continue;
			}
			cur_value = strtok(NULL, "\n");
			if (cur_value == NULL) {
				err = 1;
				xfree(line);
				continue;
			}

			if (!strcmp(cur_item, "audio_output_method"))
				set_audio_parameter(cur_value, 0);
			else if (!strcmp(cur_item, "audio_oss_device"))
				set_audio_parameter(cur_value, 1);
			else if (!strcmp(cur_item, "audio_alsa_device"))
				set_audio_parameter(cur_value, 2);
			else if (!strcmp(cur_item, "audio_nas_server"))
				set_audio_parameter(cur_value, 3);
			else if (!strcmp(cur_item, "audio_pulse_server"))
				set_audio_parameter(cur_value, 4);
			else if (!strcmp(cur_item, "audio_pulse_min_length"))
				set_audio_parameter(cur_value, 5);
			else
				err = 2;	/* Unknown parameter */
		}
		xfree(line);
	}

	if (err == 1)
		return g_strdup("302 ERROR BAD SYNTAX");
	if (err == 2)
		return g_strdup("303 ERROR INVALID PARAMETER OR VALUE");

	err = module_audio_init(&status);

	if (err == 0)
		msg = g_strdup_printf("203 OK AUDIO INITIALIZED");
	else
		msg = g_strdup_printf("300-%s\n300 UNKNOWN ERROR", status);

	return msg;
}

gchar *do_loglevel(void)
{
	char *cur_item = NULL;
	char *cur_value = NULL;
	char *line = NULL;
	int ret;
	size_t n;
	int number;
	char *tptr;
	int err = 0;		/* Error status */
	char *msg;

	printf("207 OK RECEIVING LOGLEVEL SETTINGS\n");
	fflush(stdout);

	while (1) {
		line = NULL;
		n = 0;
		ret = otts_getline(&line, &n, stdin);
		if (ret == -1) {
			err = 1;
			break;
		}
		if (!strcmp(line, ".\n")) {
			xfree(line);
			break;
		}
		if (!err) {
			cur_item = strtok(line, "=");
			if (cur_item == NULL) {
				err = 1;
				xfree(line);
				continue;
			}
			cur_value = strtok(NULL, "\n");
			if (cur_value == NULL) {
				err = 1;
				xfree(line);
				continue;
			}

			if (!strcmp(cur_item, "log_level")) {
				number = strtol(cur_value, &tptr, 10);
				if (tptr == cur_value) {
					err = 2;
					xfree(line);
					continue;
				}
				opentts_audio_set_loglevel(module_audio_id,
							   number);
			} else
				err = 2;	/* Unknown parameter */
		}
		xfree(line);
	}

	if (err == 1)
		return g_strdup("302 ERROR BAD SYNTAX");
	if (err == 2)
		return g_strdup("303 ERROR INVALID PARAMETER OR VALUE");

	msg = g_strdup_printf("203 OK LOG LEVEL SET");

	return msg;
}

gchar *do_debug(char *cmd_buf)
{
	/* TODO: Develop the full on/off logic etc. */

	char **cmd;
	char *filename;

	cmd = g_strsplit(cmd_buf, " ", -1);

	if (!cmd[1]) {
		g_strfreev(cmd);
		return g_strdup("302 ERROR BAD SYNTAX");
	}

	if (!strcmp(cmd[1], "ON")) {
		if (!cmd[2]) {
			g_strfreev(cmd);
			return g_strdup("302 ERROR BAD SYNTAX");
		}

		filename = cmd[2];
		dbg("Additional logging into specific path %s requested",
		    filename);
		CustomDebugFile = fopen(filename, "w+");
		if (CustomDebugFile == NULL) {
			dbg("ERROR: Can't open custom debug file for logging: %d (%s)", errno, strerror(errno));
			return g_strdup("303 CANT OPEN CUSTOM DEBUG FILE");
		}
		if (Debug == 1)
			Debug = 3;
		else
			Debug = 2;

		dbg("Additional logging initialized");
	} else if (!strcmp(cmd[1], "OFF")) {
		if (Debug == 3)
			Debug = 1;
		else
			Debug = 0;

		if (CustomDebugFile != NULL)
			fclose(CustomDebugFile);
		CustomDebugFile = NULL;
		dbg("Additional logging into specific path terminated");
	} else {
		return g_strdup("302 ERROR BAD SYNTAX");
	}

	g_strfreev(cmd);
	return g_strdup("200 OK DEBUGGING ON");
}

gchar *do_list_voices(void)
{
	SPDVoice **voices;
	int i;
	const char *lang;
	const char *dialect;
	GString *voice_list;

	voices = module_list_voices();
	if (voices == NULL) {
		return g_strdup("304 CANT LIST VOICES");
	}

	voice_list = g_string_new("");
	for (i = 0;; i++) {
		if (voices[i] == NULL)
			break;
		if (voices[i]->language == NULL)
			lang = "none";
		else
			lang = voices[i]->language;
		if (voices[i]->variant == NULL)
			dialect = "none";
		else
			dialect = voices[i]->variant;
		g_string_append_printf(voice_list, "200-%s %s %s\n",
				       voices[i]->name, lang, dialect);
	}
	g_string_append(voice_list, "200 OK VOICE LIST SENT");

	dbg("Voice list prepared to  send to openttsd");

	return voice_list->str;
}

/* This has to return int (although it doesn't return at all) so that we could
 * call it from PROCESS_CMD() macro like the other commands that return
 * something */
void do_quit(void)
{
	printf("210 OK QUIT\n");
	fflush(stdout);
	module_close(0);
	return;
}

int
module_get_message_part(const char *message, char *part, unsigned int *pos,
			size_t maxlen, const char *dividers)
{
	int i, n;
	int num_dividers;
	int len;

	assert(part != NULL);
	assert(message != NULL);

	len = strlen(message);

	if (message[*pos] == 0)
		return -1;

	if (dividers != NULL) {
		num_dividers = strlen(dividers);
	} else {
		num_dividers = 0;
	}

	for (i = 0; i <= maxlen - 1; i++) {
		part[i] = message[*pos];

		if (part[i] == 0) {
			return i;
		}
		// dbg("pos: %d", *pos);

		if ((len - 1 - i) > 2) {
			if ((message[*pos + 1] == ' ')
			    || (message[*pos + 1] == '\n')
			    || (message[*pos + 1] == '\r')) {
				for (n = 0; n < num_dividers; n++) {
					if ((part[i] == dividers[n])) {
						part[i + 1] = 0;
						(*pos)++;
						return i + 1;
					}
				}
				if ((message[*pos] == '\n')
				    && (message[*pos + 1] == '\n')) {
					part[i + 1] = 0;
					(*pos)++;
					return i + 1;
				}
				if ((len - 1 - i) > 4) {
					if (((message[*pos] == '\r')
					     && (message[*pos + 1] == '\n'))
					    && ((message[*pos + 2] == '\r')
						&& (message[*pos + 3] == '\n'))) {
						part[i + 1] = 0;
						(*pos)++;
						return i + 1;
					}
				}
			}
		}

		(*pos)++;
	}
	part[i] = 0;

	return i;
}

void module_strip_punctuation_some(char *message, char *punct_chars)
{
	int len;
	char *p = message;
	int i;
	assert(message != NULL);

	if (punct_chars == NULL)
		return;

	len = strlen(message);
	for (i = 0; i <= len - 1; i++) {
		if (strchr(punct_chars, *p)) {
			dbg("Substitution %d: char -%c- for whitespace\n", i,
			    *p);
			*p = ' ';
		}
		p++;
	}
}

char *module_strip_ssml(char *message)
{

	int len;
	char *out;
	int i, n;
	int omit = 0;

	assert(message != NULL);

	len = strlen(message);
	out = (char *)g_malloc(sizeof(char) * (len + 1));

	for (i = 0, n = 0; i <= len; i++) {

		if (message[i] == '<') {
			omit = 1;
			continue;
		}
		if (message[i] == '>') {
			omit = 0;
			continue;
		}
		if (!strncmp(&(message[i]), "&lt;", 4)) {
			i += 3;
			out[n++] = '<';
		} else if (!strncmp(&(message[i]), "&gt;", 4)) {
			i += 3;
			out[n++] = '>';
		} else if (!strncmp(&(message[i]), "&amp;", 5)) {
			i += 4;
			out[n++] = '&';
		} else if (!omit || i == len)
			out[n++] = message[i];
	}
	dbg("In stripping ssml: |%s|", out);

	return out;
}

void module_strip_punctuation_default(char *buf)
{
	assert(buf != NULL);
	module_strip_punctuation_some(buf, "~#$%^&*+=|<>[]_");
}

void
module_speak_thread_wfork(sem_t * semaphore, pid_t * process_pid,
			  TChildFunction child_function,
			  TParentFunction parent_function,
			  int *speaking_flag, char **message,
			  const SPDMessageType * msgtype, const size_t maxlen,
			  const char *dividers, size_t * module_position,
			  int *pause_requested)
{
	TModuleDoublePipe module_pipe;
	int ret;
	int status;

	set_speaking_thread_parameters();

	while (1) {
		sem_wait(semaphore);
		dbg("Semaphore on\n");

		ret = pipe(module_pipe.pc);
		if (ret != 0) {
			dbg("Can't create pipe pc\n");
			*speaking_flag = 0;
			continue;
		}

		ret = pipe(module_pipe.cp);
		if (ret != 0) {
			dbg("Can't create pipe cp\n");
			close(module_pipe.pc[0]);
			close(module_pipe.pc[1]);
			*speaking_flag = 0;
			continue;
		}

		dbg("Pipes initialized");

		/* Create a new process so that we could send it signals */
		*process_pid = fork();

		switch (*process_pid) {
		case -1:
			dbg("Can't say the message. fork() failed!\n");
			close(module_pipe.pc[0]);
			close(module_pipe.pc[1]);
			close(module_pipe.cp[0]);
			close(module_pipe.cp[1]);
			*speaking_flag = 0;
			continue;

		case 0:
			/* This is the child. Make it speak, but exit on SIGINT. */

			dbg("Starting child...\n");
			(*child_function) (module_pipe, maxlen);
			exit(0);

		default:
			/* This is the parent. Send data to the child. */

			*module_position =
			    (*parent_function) (module_pipe, *message, *msgtype,
						maxlen, dividers,
						pause_requested);

			dbg("Waiting for child...");
			waitpid(*process_pid, &status, 0);

			*speaking_flag = 0;

			module_report_event_end();

			dbg("child terminated -: status:%d signal?:%d signal number:%d.\n", WIFEXITED(status), WIFSIGNALED(status), WTERMSIG(status));
		}
	}
}

size_t
module_parent_wfork(TModuleDoublePipe dpipe, const char *message,
		    SPDMessageType msgtype, const size_t maxlen,
		    const char *dividers, int *pause_requested)
{
	unsigned int pos = 0;
	char msg[16];
	char *buf;
	int bytes;
	size_t read_bytes = 0;

	dbg("Entering parent process, closing pipes");

	buf = (char *)g_malloc((maxlen + 1) * sizeof(char));

	module_parent_dp_init(dpipe);

	pos = 0;
	while (1) {
		dbg("  Looping...\n");

		bytes =
		    module_get_message_part(message, buf, &pos, maxlen,
					    dividers);

		dbg("Returned %d bytes from get_part\n", bytes);

		if (*pause_requested) {
			dbg("Pause requested in parent");
			module_parent_dp_close(dpipe);
			*pause_requested = 0;
			return 0;
		}

		if (bytes > 0) {
			dbg("Sending buf to child:|%s| %d\n", buf, bytes);
			module_parent_dp_write(dpipe, buf, bytes);

			dbg("Waiting for response from child...\n");
			while (1) {
				read_bytes =
				    module_parent_dp_read(dpipe, msg, 8);
				if (read_bytes == 0) {
					dbg("parent: Read bytes 0, child stopped\n");
					break;
				}
				if (msg[0] == 'C') {
					dbg("Ok, received report to continue...\n");
					break;
				}
			}
		}

		if ((bytes == -1) || (read_bytes == 0)) {
			dbg("End of data in parent, closing pipes");
			module_parent_dp_close(dpipe);
			break;
		}

	}
	return 0;
}

int module_parent_wait_continue(TModuleDoublePipe dpipe)
{
	char msg[16];
	int bytes;

	dbg("parent: Waiting for response from child...\n");
	while (1) {
		bytes = module_parent_dp_read(dpipe, msg, 8);
		if (bytes == 0) {
			dbg("parent: Read bytes 0, child stopped\n");
			return 1;
		}
		if (msg[0] == 'C') {
			dbg("parent: Ok, received report to continue...\n");
			return 0;
		}
	}
}

void module_parent_dp_init(TModuleDoublePipe dpipe)
{
	close(dpipe.pc[0]);
	close(dpipe.cp[1]);
}

void module_parent_dp_close(TModuleDoublePipe dpipe)
{
	close(dpipe.pc[1]);
	close(dpipe.cp[0]);
}

void module_child_dp_init(TModuleDoublePipe dpipe)
{
	close(dpipe.pc[1]);
	close(dpipe.cp[0]);
}

void module_child_dp_close(TModuleDoublePipe dpipe)
{
	close(dpipe.pc[0]);
	close(dpipe.cp[1]);
}

void
module_child_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes)
{
	int ret;
	assert(msg != NULL);
	do {
		ret = write(dpipe.cp[1], msg, bytes);
		if (ret < 0) {
			if (errno == EINTR)
				continue;	/* Try again. */
			else
				FATAL("Unable to write data.");
		} else if (ret == bytes)
			break;
		else {
			/* ret < bytes.  We haven't written it all. */
			bytes -= ret;
			msg += ret;
		}
	} while (bytes > 0);
}

int
module_parent_dp_write(TModuleDoublePipe dpipe, const char *msg, size_t bytes)
{
	int ret;
	assert(msg != NULL);
	dbg("going to write %ld bytes", bytes);
	ret = write(dpipe.pc[1], msg, bytes);
	dbg("written %d bytes", ret);
	return ret;
}

int module_child_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen)
{
	int bytes;
	while ((bytes = read(dpipe.pc[0], msg, maxlen)) < 0) {
		if (errno != EINTR) {
			FATAL("Unable to read data");
		}
	}
	return bytes;
}

int module_parent_dp_read(TModuleDoublePipe dpipe, char *msg, size_t maxlen)
{
	int bytes;
	while ((bytes = read(dpipe.cp[0], msg, maxlen)) < 0) {
		if (errno != EINTR) {
			FATAL("Unable to read data");
		}
	}
	return bytes;
}

void module_sigblockall(void)
{
	int ret;
	sigset_t all_signals;

	dbg("Blocking all signals");

	sigfillset(&all_signals);

	ret = sigprocmask(SIG_BLOCK, &all_signals, NULL);
	if (ret != 0)
		dbg("Can't block signals, expect problems with terminating!\n");
}

void module_sigunblockusr(sigset_t * some_signals)
{
	int ret;

	dbg("UnBlocking user signal");

	sigdelset(some_signals, SIGUSR1);
	ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
	if (ret != 0)
		dbg("Can't block signal set, expect problems with terminating!\n");
}

void module_sigblockusr(sigset_t * some_signals)
{
	int ret;

	dbg("Blocking user signal");

	sigaddset(some_signals, SIGUSR1);
	ret = sigprocmask(SIG_SETMASK, some_signals, NULL);
	if (ret != 0)
		dbg("Can't block signal set, expect problems when terminating!\n");

}

void set_speaking_thread_parameters(void)
{
	int ret;
	sigset_t all_signals;

	ret = sigfillset(&all_signals);
	if (ret == 0) {
		ret = pthread_sigmask(SIG_BLOCK, &all_signals, NULL);
		if (ret != 0)
			dbg("Can't set signal set, expect problems when terminating!\n");
	} else {
		dbg("Can't fill signal set, expect problems when terminating!\n");
	}

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

int module_write_data_ok(char *data)
{
	/* Tests */
	if (data == NULL) {
		dbg("requested data NULL\n");
		return -1;
	}

	if (data[0] == 0) {
		dbg("requested data empty\n");
		return -1;
	}

	return 0;
}

int module_terminate_thread(pthread_t thread)
{
	int ret;

	ret = pthread_cancel(thread);
	if (ret != 0) {
		dbg("Cancelation of speak thread failed");
		return 1;
	}
	ret = pthread_join(thread, NULL);
	if (ret != 0) {
		dbg("join failed!\n");
		return 1;
	}

	return 0;
}

sem_t *module_semaphore_init()
{
	sem_t *semaphore;
	int ret;

	semaphore = (sem_t *) g_malloc(sizeof(sem_t));
	ret = sem_init(semaphore, 0, 0);
	if (ret != 0) {
		dbg("Semaphore initialization failed");
		g_free(semaphore);
		semaphore = NULL;
	}
	return semaphore;
}

char *module_recode_to_iso(char *data, int bytes, char *language,
			   char *fallback)
{
	char *recoded;

	if (language == NULL)
		recoded = g_strdup(data);

	if (!strcmp(language, "cs"))
		recoded =
		    (char *)g_convert_with_fallback(data, bytes, "ISO8859-2",
						    "UTF-8", fallback, NULL,
						    NULL, NULL);
	else
		recoded =
		    (char *)g_convert_with_fallback(data, bytes, "ISO8859-1",
						    "UTF-8", fallback, NULL,
						    NULL, NULL);

	if (recoded == NULL)
		dbg("festival: Conversion to ISO coding failed\n");

	return recoded;
}

int semaphore_post(int sem_id)
{
	static struct sembuf sem_b;

	sem_b.sem_num = 0;
	sem_b.sem_op = 1;	/* V() */
	sem_b.sem_flg = SEM_UNDO;
	return semop(sem_id, &sem_b, 1);
}

void module_send_asynchronous(char *text)
{
	pthread_mutex_lock(&module_stdout_mutex);
	dbg("Printing reply: %s", text);
	fprintf(stdout, "%s", text);
	fflush(stdout);
	dbg("Printed");
	pthread_mutex_unlock(&module_stdout_mutex);
}

void module_report_index_mark(char *mark)
{
	char *reply;
	dbg("Event: Index mark %s", mark);
	if (mark != NULL)
		reply = g_strdup_printf("700-%s\n700 INDEX MARK\n", mark);
	else
		return;

	module_send_asynchronous(reply);

	g_free(reply);
}

void module_report_event_begin(void)
{
	module_send_asynchronous("701 BEGIN\n");
}

void module_report_event_end(void)
{
	module_send_asynchronous("702 END\n");
}

void module_report_event_stop(void)
{
	module_send_asynchronous("703 STOP\n");
}

void module_report_event_pause(void)
{
	module_send_asynchronous("704 PAUSE\n");
}

/* --- CONFIGURATION --- */
configoption_t *module_add_config_option(configoption_t * options,
					 int *num_options, char *name, int type,
					 dotconf_callback_t callback,
					 info_t * info, unsigned long context)
{
	configoption_t *opts;
	int num_config_options = *num_options;

	assert(name != NULL);

	num_config_options++;
	opts =
	    (configoption_t *) g_realloc(options,
					 (num_config_options +
					  1) * sizeof(configoption_t));
	opts[num_config_options - 1].name = (char *)g_strdup(name);
	opts[num_config_options - 1].type = type;
	opts[num_config_options - 1].callback = callback;
	opts[num_config_options - 1].info = info;
	opts[num_config_options - 1].context = context;

	*num_options = num_config_options;
	return opts;
}

void *module_get_ht_option(GHashTable * hash_table, const char *key)
{
	void *option;
	assert(key != NULL);

	option = g_hash_table_lookup(hash_table, key);
	if (option == NULL)
		dbg("Requested option by key %s not found.\n", key);

	return option;
}

int module_utils_init(void)
{
	/* Init mutex */
	pthread_mutex_init(&module_stdout_mutex, NULL);

	return 0;
}

int module_audio_init_spd(char **status_info)
{
	char *error = 0;
	gchar **outputs;
	int i = 0;

	dbg("Openning audio output system");
	if (NULL == module_audio_pars[0]) {
		*status_info =
		    g_strdup
		    ("Sound output method specified in configuration not supported. "
		     "Please choose 'oss', 'alsa', 'nas', 'libao' or 'pulse'.");
		return -1;
	}

	outputs = g_strsplit(module_audio_pars[0], ",", 0);
	while (NULL != outputs[i]) {
		module_audio_id =
		    opentts_audio_open(outputs[i],
				       (void **)&module_audio_pars[1], &error);
		if (module_audio_id) {
			dbg("Using %s audio output method", outputs[i]);
			g_strfreev(outputs);
			return 0;
		}
		i++;
	}

	*status_info =
	    g_strdup_printf("Opening sound device failed. Reason: %s. ", error);
	g_free(error);		/* g_malloc'ed, in spd_audio_open. */

	g_strfreev(outputs);
	return -1;

}

void clean_old_settings_table(void)
{
	msg_settings_old.rate = OTTS_VOICE_RATE_MIN - 1;
	msg_settings_old.pitch = OTTS_VOICE_PITCH_MIN - 1;
	msg_settings_old.volume = OTTS_VOICE_VOLUME_MIN - 1;
	msg_settings_old.punctuation_mode = -1;
	msg_settings_old.spelling_mode = -1;
	msg_settings_old.cap_let_recogn = -1;
	msg_settings_old.voice_type = SPD_NO_VOICE;
	msg_settings_old.voice.name = NULL;
	msg_settings_old.voice.language = NULL;
}

void init_settings_tables(void)
{
	module_dc_options = NULL;
	msg_settings.rate = OTTS_VOICE_RATE_DEFAULT;
	msg_settings.pitch = OTTS_VOICE_PITCH_DEFAULT;
	msg_settings.volume = OTTS_VOICE_VOLUME_DEFAULT;
	msg_settings.punctuation_mode = SPD_PUNCT_NONE;
	msg_settings.spelling_mode = SPD_SPELL_OFF;
	msg_settings.cap_let_recogn = SPD_CAP_NONE;
	msg_settings.voice_type = SPD_MALE1;
	msg_settings.voice.name = NULL;
	msg_settings.voice.language = NULL;
	clean_old_settings_table();
}

void dbg(char *fmt, ...)
{
	char *tstr;
	va_list remaining_args;

	if (Debug) {
		tstr = get_timestamp();
		fputs(tstr, stderr);
		va_start(remaining_args, fmt);
		vfprintf(stderr, fmt, remaining_args);
		va_end(remaining_args);
		fputc('\n', stderr);
		fflush(stderr);
		if ((Debug == 2) || (Debug == 3)) {
			fputs(tstr, CustomDebugFile);
			va_start(remaining_args, fmt);
			vfprintf(CustomDebugFile, fmt, remaining_args);
			va_end(remaining_args);
			fputc('\n', CustomDebugFile);
			fflush(CustomDebugFile);
		}
		g_free(tstr);
	}
}
