/*
  libopentts.c - Shared library for easy acces to OpenTTS functions
 *
 * Copyright (C) 2001, 2002, 2003, 2006, 2007, 2008 Brailcom, o.p.s.
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

#include <sys/types.h>
#include <wchar.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <errno.h>
#include <assert.h>

#include <def.h>
#include <opentts/libopentts.h>

/* Comment/uncomment to switch debugging on/off */
// #define LIBOPENTTS_DEBUG 1

#ifdef LIBOPENTTS_DEBUG
/* Debugging */
static FILE *spd_debug;
#endif

/* Unless there is an fatal error, it doesn't print anything */
#define SPD_FATAL(msg) { printf("Fatal error (libopentts) [%s:%d]:"msg, __FILE__, __LINE__); fflush(stdout); exit(EXIT_FAILURE); }

/* --------------  Private functions headers ------------------------*/

static int spd_set_priority(SPDConnection * connection, SPDPriority priority);
static char *escape_dot(const char *text);
static int isanum(char *str);
static char *get_reply(SPDConnection * connection);
static int get_err_code(char *reply);
static char *get_param_str(char *reply, int num, int *err);
static int get_param_int(char *reply, int num, int *err);
static void *xmalloc(size_t bytes);
static void xfree(void *ptr);
static int ret_ok(char *reply);
static void SPD_DBG(char *format, ...);
static void *spd_events_handler(void *);
static void init_debugging(void);
static void do_autospawn(void);
static SPDConnection *make_connection(SPDConnectionMethod method);
static SPDConnection *init_connection_state(SPDConnection * connection,
					    const char *client_name,
					    const char *connection_name,
					    const char *user_name,
					    SPDConnectionMode mode);
static pthread_mutex_t *new_mutex(void);
static void destroy_mutex(pthread_mutex_t * mutex);
static pthread_cond_t *new_cond_variable(void);
static void destroy_cond_variable(pthread_cond_t * cond);

pthread_mutex_t spd_logging_mutex;

#ifndef HAVE_STRNDUP
/* Added by Willie Walker - strndup is a gcc-ism */
char *strndup(const char *s, size_t n)
{
	size_t nAvail;
	char *p;

	if (!s)
		return 0;

	if (strlen(s) > n)
		nAvail = n + 1;
	else
		nAvail = strlen(s) + 1;
	p = malloc(nAvail);
	memcpy(p, s, nAvail);
	p[nAvail - 1] = '\0';

	return p;
}
#endif /* HAVE_STRNDUP */

#define INITIAL_BUF_SIZE 120
#define GETLINE_FAILURE -1
static ssize_t otts_getline(char **lineptr, size_t * n, FILE * f)
{
	char ch;
	ssize_t buf_pos = 0;
	ssize_t needed = 2;	/* Always buf_pos + 2 (see below). */
	size_t new_length = 0;
	char *temp;

	if ((n == NULL) || (lineptr == NULL) || (f == NULL)) {
		errno = EINVAL;
		return GETLINE_FAILURE;
	}

	if (errno != 0)
		errno = 0;

	if ((*lineptr == NULL) || (*n == 0)) {
		*n = INITIAL_BUF_SIZE;
		*lineptr = malloc(*n * sizeof(char));

		if (*lineptr == NULL) {
			*n = 0;
			return GETLINE_FAILURE;	/* Out of memory. */
		}
	}

	/*
	 * For each iteration of this loop, the following holds.
	 * There are buf_pos characters in the buffer.  When we read another
	 * character, we want to store it, and we also need enough
	 * room for a NUL.  So we need to realloc as soon as our capacity
	 * becomes less than buf_pos + 2.
	 * Hence the variable "needed" which always equals buf_pos + 2.
	 */

	while ((ch = getc(f)) != EOF) {
		if (errno != 0)
			return GETLINE_FAILURE;

		if (needed > *n) {
			new_length = *n * 2;
			if (new_length <= *n) {	/* Overflow. */
				errno = ENOMEM;
				/* We couldn't store the character, */
				/* so put it back on the stream. */
				ungetc(ch, f);
				return GETLINE_FAILURE;
			}
			temp =
			    (char *)realloc(*lineptr,
					    new_length * sizeof(char));
			if (temp == NULL) {
				ungetc(ch, f);
				return GETLINE_FAILURE;
			}
			*n = new_length;
			*lineptr = temp;
		}
		(*lineptr)[buf_pos++] = ch;

		if (ch == '\n')
			break;

		if (needed == SSIZE_MAX) {
			/* We'll overflow ssize_t on the next round. */
			errno = ENOMEM;
			return GETLINE_FAILURE;
		}
		needed++;
	}
	(*lineptr)[buf_pos] = '\0';

	if (buf_pos == 0) {
		buf_pos = GETLINE_FAILURE;
	}
	return buf_pos;
}

/* --------------------- Public functions ------------------------- */

#define SPD_REPLY_BUF_SIZE 65536

/* Opens a new connection to openttsd.
 * Returns socket file descriptor of the created connection
 * or -1 if no connection was opened. */

SPDConnection *spd_open(const char *client_name, const char *connection_name,
			const char *user_name, SPDConnectionMode mode)
{
	return spd_open2(client_name, connection_name, user_name,
			 mode, SPD_METHOD_UNIX_SOCKET, 1);
}

SPDConnection *spd_open2(const char *client_name, const char *connection_name,
			 const char *user_name, SPDConnectionMode mode,
			 SPDConnectionMethod method, int autospawn)
{
	SPDConnection *connection;
	const char *conn_name;
	const char *usr_name;

	if (client_name == NULL)
		return NULL;

	if (user_name == NULL)
		usr_name = (char *)g_get_user_name();
	else
		usr_name = user_name;

	if (connection_name == NULL)
		conn_name = "main";
	else
		conn_name = connection_name;

	init_debugging();

	connection = make_connection(method);

	/* Autospawn -- check if openttsd is not running and if so, start it */
	if (autospawn && connection == NULL) {
		do_autospawn();
		connection = make_connection(method);
	}

	if (connection != NULL)
		connection = init_connection_state(connection,
						   client_name, conn_name,
						   usr_name, mode);

	return connection;
}

#define RET(r) \
    { \
    pthread_mutex_unlock(connection->ssip_mutex); \
    return r; \
    }

/* Close a connection to openttsd */
void spd_close(SPDConnection * connection)
{

	if (connection->ssip_mutex != NULL) {
		pthread_mutex_lock(connection->ssip_mutex);

		if (connection->mode == SPD_MODE_THREADED) {
			if (connection->events_thread != NULL) {
				pthread_cancel(*connection->events_thread);
				pthread_join(*connection->events_thread, NULL);
				free(connection->events_thread);
			}
			destroy_mutex(connection->mutex_reply_ready);
			destroy_mutex(connection->mutex_reply_ack);
			destroy_cond_variable(connection->cond_reply_ready);
			destroy_cond_variable(connection->cond_reply_ack);
			connection->mode = SPD_MODE_SINGLE;
		}

		pthread_mutex_unlock(connection->ssip_mutex);
		destroy_mutex(connection->ssip_mutex);
	}

	/* close the stream and socket */
	if (connection->stream != NULL) {
		fclose(connection->stream);
		connection->socket = -1;
		/* So we don't try to close the socket twice. */
	}

	if (connection->socket != -1)
		close(connection->socket);

	xfree(connection);
}

/* Helper functions for spd_say. */
static inline int
spd_say_prepare(SPDConnection * connection, SPDPriority priority,
		const char *text, char **escaped_text)
{
	int ret = 0;

	SPD_DBG("Text to say is: %s", text);

	/* Insure that there is no escape sequence in the text */
	*escaped_text = escape_dot(text);
	/* Caller is now responsible for escaped_text. */
	if (*escaped_text == NULL) {	/* Out of memory. */
		SPD_DBG("spd_say could not allocate memory.");
		ret = -1;
	} else {
		/* Set priority */
		SPD_DBG("Setting priority");
		ret = spd_set_priority(connection, priority);
		if (!ret) {
			/* Start the data flow */
			SPD_DBG("Sending SPEAK");
			ret = spd_execute_command_wo_mutex(connection, "speak");
			if (ret) {
				SPD_DBG("Error: Can't start data flow!");
			}
		}
	}

	return ret;
}

static inline int spd_say_sending(SPDConnection * connection, const char *text)
{
	int msg_id = -1;
	int err = 0;
	char *reply = NULL;
	char *pret = NULL;

	/* Send data */
	SPD_DBG("Sending data");
	pret = spd_send_data_wo_mutex(connection, text, SPD_NO_REPLY);
	if (pret == NULL) {
		SPD_DBG("Can't send data wo mutex");
	} else {
		/* Terminate data flow */
		SPD_DBG("Terminating data flow");
		err =
		    spd_execute_command_with_reply(connection, "\r\n.", &reply);
		if (err) {
			SPD_DBG("Can't terminate data flow");
		} else {
			msg_id = get_param_int(reply, 1, &err);
			if (err < 0) {
				SPD_DBG
				    ("Can't determine SSIP message unique ID parameter.");
				msg_id = -1;
			}
		}
	}

	xfree(reply);
	xfree(pret);
	return msg_id;
}

/* Say TEXT with priority PRIORITY.
 * Returns msg_uid on success, -1 otherwise. */
int spd_say(SPDConnection * connection, SPDPriority priority, const char *text)
{
	char *escaped_text = NULL;
	int msg_id = -1;
	int prepare_failed = 0;

	if (text != NULL) {
		pthread_mutex_lock(connection->ssip_mutex);

		prepare_failed =
		    spd_say_prepare(connection, priority, text, &escaped_text);
		if (!prepare_failed)
			msg_id = spd_say_sending(connection, escaped_text);

		xfree(escaped_text);
		pthread_mutex_unlock(connection->ssip_mutex);
	} else {
		SPD_DBG("spd_say called with a NULL argument for <text>");
	}

	SPD_DBG("Returning from spd_say");
	return msg_id;
}

/* The same as spd_say, accepts also formated strings */
int
spd_sayf(SPDConnection * connection, SPDPriority priority, const char *format,
	 ...)
{
	static int ret;
	va_list args;
	char *buf;

	if (format == NULL)
		return -1;

	/* Print the text to buffer */
	va_start(args, format);
	buf = g_strdup_vprintf(format, args);
	va_end(args);

	/* Send the buffer to openttsd */
	ret = spd_say(connection, priority, buf);
	xfree(buf);

	return ret;
}

int spd_stop(SPDConnection * connection)
{
	return spd_execute_command(connection, "STOP SELF");
}

int spd_stop_all(SPDConnection * connection)
{
	return spd_execute_command(connection, "STOP ALL");
}

int spd_stop_uid(SPDConnection * connection, int target_uid)
{
	static char command[16];

	sprintf(command, "STOP %d", target_uid);
	return spd_execute_command(connection, command);
}

int spd_cancel(SPDConnection * connection)
{
	return spd_execute_command(connection, "CANCEL SELF");
}

int spd_cancel_all(SPDConnection * connection)
{
	return spd_execute_command(connection, "CANCEL ALL");
}

int spd_cancel_uid(SPDConnection * connection, int target_uid)
{
	static char command[16];

	sprintf(command, "CANCEL %d", target_uid);
	return spd_execute_command(connection, command);
}

int spd_pause(SPDConnection * connection)
{
	return spd_execute_command(connection, "PAUSE SELF");
}

int spd_pause_all(SPDConnection * connection)
{
	return spd_execute_command(connection, "PAUSE ALL");
}

int spd_pause_uid(SPDConnection * connection, int target_uid)
{
	char command[16];

	sprintf(command, "PAUSE %d", target_uid);
	return spd_execute_command(connection, command);
}

int spd_resume(SPDConnection * connection)
{
	return spd_execute_command(connection, "RESUME SELF");
}

int spd_resume_all(SPDConnection * connection)
{
	return spd_execute_command(connection, "RESUME ALL");
}

int spd_resume_uid(SPDConnection * connection, int target_uid)
{
	static char command[16];

	sprintf(command, "RESUME %d", target_uid);
	return spd_execute_command(connection, command);
}

int
spd_key(SPDConnection * connection, SPDPriority priority, const char *key_name)
{
	char *command_key;
	int ret;

	if (key_name == NULL)
		return -1;

	pthread_mutex_lock(connection->ssip_mutex);

	ret = spd_set_priority(connection, priority);
	if (ret)
		RET(-1);

	command_key = g_strdup_printf("KEY %s", key_name);
	ret = spd_execute_command_wo_mutex(connection, command_key);
	xfree(command_key);
	if (ret)
		RET(-1);

	pthread_mutex_unlock(connection->ssip_mutex);

	return 0;
}

int
spd_char(SPDConnection * connection, SPDPriority priority,
	 const char *character)
{
	static char command[16];
	int ret;

	if (character == NULL)
		return -1;
	if (strlen(character) > 6)
		return -1;

	pthread_mutex_lock(connection->ssip_mutex);

	ret = spd_set_priority(connection, priority);
	if (ret)
		RET(-1);

	sprintf(command, "CHAR %s", character);
	ret = spd_execute_command_wo_mutex(connection, command);
	if (ret)
		RET(-1);

	pthread_mutex_unlock(connection->ssip_mutex);

	return 0;
}

int
spd_wchar(SPDConnection * connection, SPDPriority priority, wchar_t wcharacter)
{
	static char command[16];
	char character[8];
	int ret;

	pthread_mutex_lock(connection->ssip_mutex);

	ret = wcrtomb(character, wcharacter, NULL);
	if (ret <= 0)
		RET(-1);

	ret = spd_set_priority(connection, priority);
	if (ret)
		RET(-1);

	assert(character != NULL);
	sprintf(command, "CHAR %s", character);
	ret = spd_execute_command_wo_mutex(connection, command);
	if (ret)
		RET(-1);

	pthread_mutex_unlock(connection->ssip_mutex);

	return 0;
}

int
spd_sound_icon(SPDConnection * connection, SPDPriority priority,
	       const char *icon_name)
{
	char *command;
	int ret;

	if (icon_name == NULL)
		return -1;

	pthread_mutex_lock(connection->ssip_mutex);

	ret = spd_set_priority(connection, priority);
	if (ret)
		RET(-1);

	command = g_strdup_printf("SOUND_ICON %s", icon_name);
	ret = spd_execute_command_wo_mutex(connection, command);
	xfree(command);
	if (ret)
		RET(-1);

	pthread_mutex_unlock(connection->ssip_mutex);

	return 0;
}

int
spd_w_set_punctuation(SPDConnection * connection, SPDPunctuation type,
		      const char *who)
{
	char command[32];
	int ret;

	if (type == SPD_PUNCT_ALL)
		sprintf(command, "SET %s PUNCTUATION all", who);
	if (type == SPD_PUNCT_NONE)
		sprintf(command, "SET %s PUNCTUATION none", who);
	if (type == SPD_PUNCT_SOME)
		sprintf(command, "SET %s PUNCTUATION some", who);

	ret = spd_execute_command(connection, command);

	return ret;
}

int
spd_w_set_capital_letters(SPDConnection * connection, SPDCapitalLetters type,
			  const char *who)
{
	char command[64];
	int ret;

	if (type == SPD_CAP_NONE)
		sprintf(command, "SET %s CAP_LET_RECOGN none", who);
	if (type == SPD_CAP_SPELL)
		sprintf(command, "SET %s CAP_LET_RECOGN spell", who);
	if (type == SPD_CAP_ICON)
		sprintf(command, "SET %s CAP_LET_RECOGN icon", who);

	ret = spd_execute_command(connection, command);

	return ret;
}

int
spd_w_set_spelling(SPDConnection * connection, SPDSpelling type,
		   const char *who)
{
	char command[32];
	int ret;

	if (type == SPD_SPELL_ON)
		sprintf(command, "SET %s SPELLING on", who);
	if (type == SPD_SPELL_OFF)
		sprintf(command, "SET %s SPELLING off", who);

	ret = spd_execute_command(connection, command);

	return ret;
}

int spd_set_data_mode(SPDConnection * connection, SPDDataMode mode)
{
	char command[32];
	int ret;

	if (mode == SPD_DATA_TEXT)
		sprintf(command, "SET SELF SSML_MODE off");
	if (mode == SPD_DATA_SSML)
		sprintf(command, "SET SELF SSML_MODE on");

	ret = spd_execute_command(connection, command);

	return ret;
}

int
spd_w_set_voice_type(SPDConnection * connection, SPDVoiceType type,
		     const char *who)
{
	static char command[64];

	switch (type) {
	case SPD_MALE1:
		sprintf(command, "SET %s VOICE MALE1", who);
		break;
	case SPD_MALE2:
		sprintf(command, "SET %s VOICE MALE2", who);
		break;
	case SPD_MALE3:
		sprintf(command, "SET %s VOICE MALE3", who);
		break;
	case SPD_FEMALE1:
		sprintf(command, "SET %s VOICE FEMALE1", who);
		break;
	case SPD_FEMALE2:
		sprintf(command, "SET %s VOICE FEMALE2", who);
		break;
	case SPD_FEMALE3:
		sprintf(command, "SET %s VOICE FEMALE3", who);
		break;
	case SPD_CHILD_MALE:
		sprintf(command, "SET %s VOICE CHILD_MALE", who);
		break;
	case SPD_CHILD_FEMALE:
		sprintf(command, "SET %s VOICE CHILD_FEMALE", who);
		break;
	default:
		return -1;
	}

	return spd_execute_command(connection, command);
}

#define SPD_SET_COMMAND_INT(param, ssip_name, condition) \
    int \
    spd_w_set_ ## param (SPDConnection *connection, signed int val, const char* who) \
    { \
        static char command[64]; \
        if ((!condition)) return -1; \
        sprintf(command, "SET %s " #ssip_name " %d", who, val); \
        return spd_execute_command(connection, command); \
    } \
    int \
    spd_set_ ## param (SPDConnection *connection, signed int val) \
    { \
        return spd_w_set_ ## param (connection, val, "SELF"); \
    } \
    int \
    spd_set_ ## param ## _all(SPDConnection *connection, signed int val) \
    { \
        return spd_w_set_ ## param (connection, val, "ALL"); \
    } \
    int \
    spd_set_ ## param ## _uid(SPDConnection *connection, signed int val, unsigned int uid) \
    { \
        char who[8]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, val, who); \
    }

#define SPD_SET_COMMAND_STR(param, ssip_name) \
    int \
    spd_w_set_ ## param (SPDConnection *connection, const char *str, const char* who) \
    { \
        char *command; \
        int ret; \
        if (str == NULL) return -1; \
        command = g_strdup_printf("SET %s " #param " %s", \
                              who, str); \
        ret = spd_execute_command(connection, command); \
        xfree(command); \
        return ret; \
    } \
    int \
    spd_set_ ## param (SPDConnection *connection, const char *str) \
    { \
        return spd_w_set_ ## param (connection, str, "SELF"); \
    } \
    int \
    spd_set_ ## param ## _all(SPDConnection *connection, const char *str) \
    { \
        return spd_w_set_ ## param (connection, str, "ALL"); \
    } \
    int \
    spd_set_ ## param ## _uid(SPDConnection *connection, const char *str, unsigned int uid) \
    { \
        char who[8]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, str, who); \
    }

#define SPD_SET_COMMAND_SPECIAL(param, type) \
    int \
    spd_set_ ## param (SPDConnection *connection, type val) \
    { \
        return spd_w_set_ ## param (connection, val, "SELF"); \
    } \
    int \
    spd_set_ ## param ## _all(SPDConnection *connection, type val) \
    { \
        return spd_w_set_ ## param (connection, val, "ALL"); \
    } \
    int \
    spd_set_ ## param ## _uid(SPDConnection *connection, type val, unsigned int uid) \
    { \
        char who[8]; \
        sprintf(who, "%d", uid); \
        return spd_w_set_ ## param (connection, val, who); \
    }

	SPD_SET_COMMAND_INT(voice_rate, RATE, ((val >= OTTS_VOICE_RATE_MIN)
	                                       && (val <= OTTS_VOICE_RATE_MAX)))
	SPD_SET_COMMAND_INT(voice_pitch, PITCH, ((val >= OTTS_VOICE_PITCH_MIN)
	                                         && (val <= OTTS_VOICE_PITCH_MAX)))
	SPD_SET_COMMAND_INT(volume, VOLUME, ((val >= OTTS_VOICE_VOLUME_MIN)
	                                     && (val <= OTTS_VOICE_VOLUME_MAX)))

	SPD_SET_COMMAND_STR(language, LANGUAGE)
	SPD_SET_COMMAND_STR(output_module, OUTPUT_MODULE)
	SPD_SET_COMMAND_STR(synthesis_voice, SYNTHESIS_VOICE)

	SPD_SET_COMMAND_SPECIAL(punctuation, SPDPunctuation)
	SPD_SET_COMMAND_SPECIAL(capital_letters, SPDCapitalLetters)
	SPD_SET_COMMAND_SPECIAL(spelling, SPDSpelling)
	SPD_SET_COMMAND_SPECIAL(voice_type, SPDVoiceType)
#undef SPD_SET_COMMAND_INT
#undef SPD_SET_COMMAND_STR
#undef SPD_SET_COMMAND_SPECIAL
int
spd_set_notification_on(SPDConnection * connection,
			SPDNotification notification)
{
	if (connection->mode == SPD_MODE_THREADED)
		return spd_set_notification(connection, notification, "on");
	else
		return -1;
}

int
spd_set_notification_off(SPDConnection * connection,
			 SPDNotification notification)
{
	if (connection->mode == SPD_MODE_THREADED)
		return spd_set_notification(connection, notification, "off");
	else
		return -1;
}

#define NOTIFICATION_SET(val, ssip_val) \
    if (notification & val){ \
	sprintf(command, "SET SELF NOTIFICATION "ssip_val" %s", state);\
	ret = spd_execute_command_wo_mutex(connection, command);\
	if (ret < 0) RET(-1);\
    }

int
spd_set_notification(SPDConnection * connection, SPDNotification notification,
		     const char *state)
{
	static char command[64];
	int ret;

	if (connection->mode != SPD_MODE_THREADED)
		return -1;

	if (state == NULL) {
		SPD_DBG("Requested state is NULL");
		return -1;
	}
	if (strcmp(state, "on") && strcmp(state, "off")) {
		SPD_DBG("Invalid argument for spd_set_notification: %s", state);
		return -1;
	}

	pthread_mutex_lock(connection->ssip_mutex);

	NOTIFICATION_SET(SPD_INDEX_MARKS, "index_marks");
	NOTIFICATION_SET(SPD_BEGIN, "begin");
	NOTIFICATION_SET(SPD_END, "end");
	NOTIFICATION_SET(SPD_CANCEL, "cancel");
	NOTIFICATION_SET(SPD_PAUSE, "pause");
	NOTIFICATION_SET(SPD_RESUME, "resume");
	NOTIFICATION_SET(SPD_RESUME, "pause");

	pthread_mutex_unlock(connection->ssip_mutex);

	return 0;
}

#undef NOTIFICATION_SET

/* spd_list_modules retrieves information about the available output modules.
   The return value is a null-terminated array of strings containing output module
   names.
*/

char **spd_list_modules(SPDConnection * connection)
{
	char **available_modules;
	available_modules =
	    spd_execute_command_with_list_reply(connection,
						"LIST OUTPUT_MODULES");
	return available_modules;
}

char **spd_list_voices(SPDConnection * connection)
{
	char **voices;
	voices = spd_execute_command_with_list_reply(connection, "LIST VOICES");
	return voices;
}

SPDVoice **spd_list_synthesis_voices(SPDConnection * connection)
{
	char **svoices_str;
	SPDVoice **svoices;
	int i, num_items;
	svoices_str =
	    spd_execute_command_with_list_reply(connection,
						"LIST SYNTHESIS_VOICES");

	if (svoices_str == NULL)
		return NULL;

	for (i = 0;; i++)
		if (svoices_str[i] == NULL)
			break;
	num_items = i;
	svoices = (SPDVoice **) malloc((num_items + 1) * sizeof(SPDVoice *));

	for (i = 0; i < num_items; i++) {
		const char delimiters[] = " ";
		char *running;

		if (svoices_str[i] == NULL)
			break;
		running = strdup(svoices_str[i]);

		svoices[i] = (SPDVoice *) malloc(sizeof(SPDVoice));
		svoices[i]->name = strsep(&running, delimiters);
		svoices[i]->language = strsep(&running, delimiters);
		svoices[i]->variant = strsep(&running, delimiters);
		assert(svoices[i]->name != NULL);
	}

	svoices[num_items] = NULL;

	return svoices;
}

char **spd_execute_command_with_list_reply(SPDConnection * connection,
					   char *command)
{
	char *reply, *line;
	int err;
	int max_items = 50;
	char **result;
	int i, ret;

	result = malloc((max_items + 1) * sizeof(char *));

	ret = spd_execute_command_with_reply(connection, command, &reply);
	if (!ret_ok(reply))
		return NULL;

	for (i = 0;; i++) {
		line = get_param_str(reply, i + 1, &err);
		if ((err) || (line == NULL))
			break;
		result[i] = strdup(line);
		if (i >= max_items - 2) {
			max_items *= 2;
			result = realloc(result, max_items * sizeof(char *));
		}
	}

	result[i] = NULL;

	return result;
}

//int
//spd_get_client_list(SPDConnection *connection, char **client_names, int *client_ids, int* active){
//        SPD_DBG("spd_get_client_list: History is not yet implemented.");
//        return -1;
//
//}

int
spd_get_message_list_fd(SPDConnection * connection, int target, int *msg_ids,
			char **client_names)
{
	SPD_DBG("spd_get_client_list: History is not yet implemented.");
	return -1;
#if 0
	sprintf(command, "HISTORY GET MESSAGE_LIST %d 0 20\r\n", target);
	reply = spd_send_data(fd, command, 1);

/*	header_ok = parse_response_header(reply);
	if(header_ok != 1){
		free(reply);
		return -1;
	}*/

	for (count = 0;; count++) {
		record = (char *)parse_response_data(reply, count + 1);
		if (record == NULL)
			break;
		record_int = get_rec_int(record, 0);
		msg_ids[count] = record_int;
		record_str = (char *)get_rec_str(record, 1);
		assert(record_str != NULL);
		client_names[count] = record_str;
	}
	return count;
#endif
}

int spd_execute_command(SPDConnection * connection, char *command)
{
	char *reply;
	int ret;

	pthread_mutex_lock(connection->ssip_mutex);

	ret = spd_execute_command_with_reply(connection, command, &reply);
	if (ret) {
		SPD_DBG("Can't execute command in spd_execute_command");
	}
	xfree(reply);

	pthread_mutex_unlock(connection->ssip_mutex);

	return ret;
}

int spd_execute_command_wo_mutex(SPDConnection * connection, char *command)
{
	char *reply;
	int ret;

	SPD_DBG("Executing command wo_mutex");
	ret = spd_execute_command_with_reply(connection, command, &reply);
	if (ret)
		SPD_DBG
		    ("Can't execute command in spd_execute_command_wo_mutex");

	xfree(reply);

	return ret;
}

int
spd_execute_command_with_reply(SPDConnection * connection, char *command,
			       char **reply)
{
	char *buf;
	int r;
	SPD_DBG("Inside execute_command_with_reply");

	buf = g_strdup_printf("%s\r\n", command);
	*reply = spd_send_data_wo_mutex(connection, buf, SPD_WAIT_REPLY);
	xfree(buf);
	buf = NULL;
	if (*reply == NULL) {
		SPD_DBG
		    ("Can't send data wo mutex in spd_execute_command_with_reply");
		return -1;
	}

	r = ret_ok(*reply);

	if (!r)
		return -1;
	else
		return 0;
}

char *spd_send_data(SPDConnection * connection, const char *message, int wfr)
{
	char *reply;
	pthread_mutex_lock(connection->ssip_mutex);

	if (connection->stream == NULL)
		RET(NULL);

	reply = spd_send_data_wo_mutex(connection, message, wfr);
	if (reply == NULL) {
		SPD_DBG("Can't send data wo mutex in spd_send_data");
		RET(NULL);
	}

	pthread_mutex_unlock(connection->ssip_mutex);
	return reply;
}

char *spd_send_data_wo_mutex(SPDConnection * connection, const char *message,
			     int wfr)
{

	char *reply;
	int bytes;

	SPD_DBG("Inside spd_send_data_wo_mutex");

	if (connection->stream == NULL)
		return NULL;

	if (connection->mode == SPD_MODE_THREADED) {
		/* Make sure we don't get the cond_reply_ready signal before we are in
		   cond_wait() */
		pthread_mutex_lock(connection->mutex_reply_ready);
	}
	/* write message to the socket */
	SPD_DBG("Writing to socket");
	write(connection->socket, message, strlen(message));
	SPD_DBG("Written to socket");
	SPD_DBG(">> : |%s|", message);

	/* read reply to the buffer */
	if (wfr) {
		if (connection->mode == SPD_MODE_THREADED) {
			/* Wait until the reply is ready */
			SPD_DBG
			    ("Waiting for cond_reply_ready in spd_send_data_wo_mutex");
			pthread_cond_wait(connection->cond_reply_ready,
					  connection->mutex_reply_ready);
			SPD_DBG("Condition for cond_reply_ready satisfied");
			pthread_mutex_unlock(connection->mutex_reply_ready);
			SPD_DBG
			    ("Reading the reply in spd_send_data_wo_mutex threaded mode");
			/* Read the reply */
			if (connection->reply != NULL) {
				reply = strdup(connection->reply);
			} else {
				SPD_DBG
				    ("Error: Can't read reply, broken socket in spd_send_data.");
				return NULL;
			}
			xfree(connection->reply);
			bytes = strlen(reply);
			if (bytes == 0) {
				SPD_DBG("Error: Empty reply, broken socket.");
				return NULL;
			}
			/* Signal the reply has been read */
			pthread_mutex_lock(connection->mutex_reply_ack);
			pthread_cond_signal(connection->cond_reply_ack);
			pthread_mutex_unlock(connection->mutex_reply_ack);
		} else {
			reply = get_reply(connection);
		}
		if (reply != NULL)
			SPD_DBG("<< : |%s|\n", reply);
	} else {
		if (connection->mode == SPD_MODE_THREADED)
			pthread_mutex_unlock(connection->mutex_reply_ready);
		SPD_DBG("<< : no reply expected");
		return strdup("NO REPLY");
	}

	if (reply == NULL)
		SPD_DBG
		    ("Reply from get_reply is NULL in spd_send_data_wo_mutex");

	SPD_DBG("Returning from spd_send_data_wo_mutex");
	return reply;
}

/* --------------------- Internal functions ------------------------- */

static int spd_set_priority(SPDConnection * connection, SPDPriority priority)
{
	static char p_name[16];
	static char command[64];

	switch (priority) {
	case SPD_IMPORTANT:
		strcpy(p_name, "IMPORTANT");
		break;
	case SPD_MESSAGE:
		strcpy(p_name, "MESSAGE");
		break;
	case SPD_TEXT:
		strcpy(p_name, "TEXT");
		break;
	case SPD_NOTIFICATION:
		strcpy(p_name, "NOTIFICATION");
		break;
	case SPD_PROGRESS:
		strcpy(p_name, "PROGRESS");
		break;
	default:
		SPD_DBG("Error: Can't set priority! Incorrect value.");
		return -1;
	}

	sprintf(command, "SET SELF PRIORITY %s", p_name);
	return spd_execute_command_wo_mutex(connection, command);
}

static char *get_reply(SPDConnection * connection)
{
	GString *str;
	char *line = NULL;
	size_t N = 0;
	int bytes;
	char *reply;
	gboolean errors = FALSE;

	str = g_string_new("");

	/* Wait for activity on the socket, when there is some,
	   read all the message line by line */
	do {
		bytes = otts_getline(&line, &N, connection->stream);
		if (bytes == -1) {
			SPD_DBG
			    ("Error: Can't read reply, broken socket in get_reply!");
			if (connection->stream != NULL)
				fclose(connection->stream);
			connection->stream = NULL;
			errors = TRUE;
		} else {
			g_string_append(str, line);
		}
		/* terminate if we reached the last line (without '-' after numcode) */
	} while (!errors && !((strlen(line) < 4) || (line[3] == ' ')));

	xfree(line);		/* otts_getline allocates with malloc. */

	if (errors) {
		/* Free the GString and its character data, and return NULL. */
		g_string_free(str, TRUE);
		reply = NULL;
	} else {
		/* The resulting message received from the socket is stored in reply */
		reply = str->str;
		/* Free the GString, but not its character data. */
		g_string_free(str, FALSE);
	}

	return reply;
}

static void *spd_events_handler(void *conn)
{
	char *reply;
	int reply_code;
	SPDConnection *connection = conn;

	while (1) {

		/* Read the reply/event (block if none is available) */
		SPD_DBG("Getting reply in spd_events_handler");
		reply = get_reply(connection);
		if (reply == NULL) {
			SPD_DBG("ERROR: BROKEN SOCKET");
			reply_code = -1;
		} else {
			SPD_DBG("<< : |%s|\n", reply);
			reply_code = get_err_code(reply);
		}

		if ((reply_code >= 700) && (reply_code < 800)) {
			int msg_id;
			int client_id;
			int err;

			SPD_DBG("Callback detected: %s", reply);

			/* This is an index mark */
			/* Extract message id */
			msg_id = get_param_int(reply, 1, &err);
			if (err < 0) {
				SPD_DBG
				    ("Bad reply from openttsd: %s (code %d)",
				     reply, err);
				break;
			}
			client_id = get_param_int(reply, 2, &err);
			if (err < 0) {
				SPD_DBG
				    ("Bad reply from openttsd: %s (code %d)",
				     reply, err);
				break;
			}
			/*  Decide if we want to call a callback */
			if ((reply_code == 701) && (connection->callback_begin))
				connection->callback_begin(msg_id, client_id,
							   SPD_EVENT_BEGIN);
			if ((reply_code == 702) && (connection->callback_end))
				connection->callback_end(msg_id, client_id,
							 SPD_EVENT_END);
			if ((reply_code == 703)
			    && (connection->callback_cancel))
				connection->callback_cancel(msg_id, client_id,
							    SPD_EVENT_CANCEL);
			if ((reply_code == 704) && (connection->callback_pause))
				connection->callback_pause(msg_id, client_id,
							   SPD_EVENT_PAUSE);
			if ((reply_code == 705)
			    && (connection->callback_resume))
				connection->callback_resume(msg_id, client_id,
							    SPD_EVENT_RESUME);
			if ((reply_code == 700) && (connection->callback_im)) {
				char *im;
				int err;
				im = get_param_str(reply, 3, &err);
				if ((err < 0) || (im == NULL)) {
					SPD_DBG
					    ("Broken reply from openttsd: %s",
					     reply);
					break;
				}
				/* Call the callback */
				connection->callback_im(msg_id, client_id,
							SPD_EVENT_INDEX_MARK,
							im);
				xfree(im);
			}

		} else {
			/* This is a protocol reply */
			pthread_mutex_lock(connection->mutex_reply_ready);
			/* Prepare the reply to the reply buffer in connection */
			if (reply != NULL) {
				connection->reply = strdup(reply);
			} else {
				SPD_DBG("Connection reply is NULL");
				connection->reply = NULL;
				break;
			}
			/* Signal the reply is available on the condition variable */
			/* this order is correct and necessary */
			pthread_cond_signal(connection->cond_reply_ready);
			pthread_mutex_lock(connection->mutex_reply_ack);
			pthread_mutex_unlock(connection->mutex_reply_ready);
			/* Wait until it has bean read */
			pthread_cond_wait(connection->cond_reply_ack,
					  connection->mutex_reply_ack);
			pthread_mutex_unlock(connection->mutex_reply_ack);
			xfree(reply);
			/* Continue */
		}
	}
	/* In case of broken socket, we must still signal reply ready */
	if (connection->reply == NULL) {
		SPD_DBG("Signalling reply ready after communication failure");
		pthread_mutex_unlock(connection->mutex_reply_ready);
		pthread_mutex_unlock(connection->mutex_reply_ack);
		if (connection->stream != NULL)
			fclose(connection->stream);
		connection->stream = NULL;
		pthread_cond_signal(connection->cond_reply_ready);
		pthread_exit(0);
	}
	return 0;		/* to please gcc */
}

static int ret_ok(char *reply)
{
	int err;

	if (reply == NULL)
		return -1;

	err = get_err_code(reply);

	if ((err >= 100) && (err < 300))
		return 1;
	if (err >= 300)
		return 0;

	SPD_FATAL("Internal error during communication.");
}

static char *get_param_str(char *reply, int num, int *err)
{
	int i;
	char *tptr;
	char *pos;
	char *pos_begin;
	char *pos_end;
	char *rep;

	assert(err != NULL);

	if (num < 1) {
		*err = -1;
		return NULL;
	}

	pos = reply;
	for (i = 0; i <= num - 2; i++) {
		pos = strstr(pos, "\r\n");
		if (pos == NULL) {
			*err = -2;
			return NULL;
		}
		pos += 2;
	}

	if (strlen(pos) < 4)
		return NULL;

	*err = strtol(pos, &tptr, 10);
	if (*err >= 300 && *err <= 399)
		return NULL;

	if ((*tptr != '-') || (tptr != pos + 3)) {
		*err = -3;
		return NULL;
	}

	pos_begin = pos + 4;
	pos_end = strstr(pos_begin, "\r\n");
	if (pos_end == NULL) {
		*err = -4;
		return NULL;
	}

	rep = (char *)strndup(pos_begin, pos_end - pos_begin);
	*err = 0;

	return rep;
}

static int get_param_int(char *reply, int num, int *err)
{
	char *rep_str;
	char *tptr;
	int ret;

	rep_str = get_param_str(reply, num, err);
	if (rep_str == NULL) {
		/* err is already set to the error return code, just return */
		return 0;
	}

	ret = strtol(rep_str, &tptr, 10);
	if (*tptr != '\0') {
		/* this is not a number */
		*err = -3;
		return 0;
	}
	xfree(rep_str);

	return ret;
}

static int get_err_code(char *reply)
{
	char err_code[4];
	int err;

	if (reply == NULL)
		return -1;
	SPD_DBG("spd_send_data:	reply: %s\n", reply);

	err_code[0] = reply[0];
	err_code[1] = reply[1];
	err_code[2] = reply[2];
	err_code[3] = '\0';

	SPD_DBG("ret_ok: err_code:	|%s|\n", err_code);

	if (isanum(err_code)) {
		err = atoi(err_code);
	} else {
		SPD_DBG("ret_ok: not a number\n");
		return -1;
	}

	return err;
}

/* isanum() tests if the given string is a number,
 *  returns 1 if yes, 0 otherwise. */
static int isanum(char *str)
{
	int i;
	if (str == NULL)
		return 0;
	for (i = 0; i <= strlen(str) - 1; i++) {
		if (!isdigit(str[i]))
			return 0;
	}
	return 1;
}

static void *xmalloc(size_t bytes)
{
	void *mem;

	mem = malloc(bytes);
	if (mem == NULL) {
		SPD_FATAL("Not enough memmory!");
		exit(1);
	}

	return mem;
}

static void xfree(void *ptr)
{
	if (ptr != NULL)
		free(ptr);
}

/*
 * escape_dot: Replace . with .. at the start of lines.
 * @text: text to escape
 * @Returns: An allocated string, containing the escaped text.
 */
static char *escape_dot(const char *text)
{
	size_t orig_len = 0;
	const char *orig_end;
	char *result = NULL;
	char *result_ptr;
	static const char *ESCAPED_DOTLINE = "\r\n..";
	static const size_t ESCAPED_DOTLINELEN = 4;
	static const size_t DOTLINELEN = 3;

	if (text == NULL)
		return NULL;

	orig_len = strlen(text);
	orig_end = text + orig_len;
	result = malloc((orig_len * 2 + 1) * sizeof(char));

	if (result == NULL)
		return NULL;

	result_ptr = result;

	/* We're over-allocating.  Even if we replaced every character
	 * in text with "..", the length of the escaped string can be no more
	 * than orig_len * 2.  We could tighten that upper bound with
	 * a little more work.
	 */

	if ((orig_len >= 1) && (text[0] == '.')) {
		*(result_ptr++) = '.';
		*(result_ptr++) = '.';
		text += 1;
	}

	while (text < orig_end) {
		if ((text[0] == '\r') && (text[1] == '\n') && (text[2] == '.')) {
			memcpy(result_ptr, ESCAPED_DOTLINE, ESCAPED_DOTLINELEN);
			result_ptr += ESCAPED_DOTLINELEN;
			text += DOTLINELEN;
		} else {
			*(result_ptr++) = *(text++);
		}
	}

	*result_ptr = '\0';
	return result;
}

#ifdef LIBOPENTTS_DEBUG
static void SPD_DBG(char *format, ...)
{
	va_list args;

	pthread_mutex_lock(&spd_logging_mutex);
	va_start(args, format);
	vfprintf(spd_debug, format, args);
	va_end(args);
	fprintf(spd_debug, "\n");
	fflush(spd_debug);
	pthread_mutex_unlock(&spd_logging_mutex);
}
#else /* LIBOPENTTS_DEBUG */
static void SPD_DBG(char *format, ...)
{
}
#endif /* LIBOPENTTS_DEBUG */

static void init_debugging(void)
{
#ifdef LIBOPENTTS_DEBUG
	/*
	 * Don't try to initialize debugging if debugging is already
	 * initialized.  I.E., if the debug file is open, don't reopen it.
	 */
	if (spd_debug != NULL)
		return;

	spd_debug = fopen("/tmp/libopentts.log", "w");
	if (spd_debug == NULL)
		SPD_FATAL("COULDN'T ACCES FILE INTENDED FOR DEBUG");

	ret = pthread_mutex_init(&spd_logging_mutex, NULL);
	if (ret != 0) {
		fprintf(stderr, "Mutex initialization failed");
		fflush(stderr);
		exit(1);
	}
	SPD_DBG("Debugging started");
#endif /* LIBOPENTTS_DEBUG */
}

static void do_autospawn(void)
{
	const char *speechd_cmd[] = { SPD_SPAWN_CMD, NULL };
	gint exit_status;
	GError *error = NULL;

	g_spawn_sync(NULL, (gchar **) speechd_cmd, NULL,
		     G_SPAWN_SEARCH_PATH |
		     G_SPAWN_STDOUT_TO_DEV_NULL |
		     G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
		     NULL, NULL, &exit_status, &error);
	sleep(1);
}

/*
 * Make the connection to the server, using the chosen method.
 * Return a pointer to a heap-allocated SPDConnection, or NULL if the
 * connection could not be made.
 */

static SPDConnection *make_connection(SPDConnectionMethod method)
{
	char *env_port;
	char *env_socket;
	int port;
	int ret;
	char tcp_no_delay = 1;
	SPDConnection *connection = xmalloc(sizeof(SPDConnection));

	memset(connection, '\0', sizeof(SPDConnection));

	if (method == SPD_METHOD_INET_SOCKET) {
		struct sockaddr_in address_inet;

		env_port = getenv("OPENTTSD_PORT");
		if (env_port != NULL)
			port = strtol(env_port, NULL, 10);
		else
			port = OPENTTSD_DEFAULT_PORT;

		address_inet.sin_addr.s_addr = inet_addr("127.0.0.1");
		address_inet.sin_port = htons(port);
		address_inet.sin_family = AF_INET;
		connection->socket = socket(AF_INET, SOCK_STREAM, 0);
		if (connection->socket == -1) {
			SPD_DBG("Unable to allocate a socket: %s",
				strerror(errno));
			free(connection);
			return NULL;
		}
		ret =
		    connect(connection->socket,
			    (struct sockaddr *)&address_inet,
			    sizeof(address_inet));
		if (ret == -1) {
			SPD_DBG
			    ("Error: Can't connect to localhost on port %d using inet sockets: %s",
			     port, strerror(errno));
			close(connection->socket);
			free(connection);
			return NULL;
		}
		setsockopt(connection->socket, IPPROTO_TCP, TCP_NODELAY,
			   &tcp_no_delay, sizeof(int));
	} else if (method == SPD_METHOD_UNIX_SOCKET) {
		struct sockaddr_un address_unix;
		GString *socket_filename;
/* Determine address for the unix socket */
		env_socket = getenv("OPENTTSD_SOCKET");
		if (env_socket) {
			socket_filename = g_string_new(env_socket);
		} else {
			const char *homedir = g_getenv("HOME");
			if (!homedir)
				homedir = g_get_home_dir();
			socket_filename = g_string_new("");
			g_string_printf(socket_filename,
					"%s/.opentts/openttsd.sock", homedir);
		}
/* Create the unix socket */
		address_unix.sun_family = AF_UNIX;
		strncpy(address_unix.sun_path, socket_filename->str,
			sizeof(address_unix.sun_path));
		address_unix.sun_path[sizeof(address_unix.sun_path) - 1] = '\0';
		connection->socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (connection->socket == -1) {
			SPD_DBG("Unable to allocate a socket: %s",
				strerror(errno));
			g_string_free(socket_filename, TRUE);
			free(connection);
			return NULL;
		}
		ret =
		    connect(connection->socket,
			    (struct sockaddr *)&address_unix,
			    SUN_LEN(&address_unix));
		if (ret == -1) {
			SPD_DBG("Error: Can't connect to unix socket %s: %s",
				socket_filename->str, strerror(errno));
			g_string_free(socket_filename, TRUE);
			close(connection->socket);
			free(connection);
			return NULL;
		}
		g_string_free(socket_filename, TRUE);
	} else
		SPD_FATAL("Unsupported connection method to spd_open");

	return connection;
}

/*
 * Initialize the state of the connection, after we've connected to
 * the server.  This includes tasks such as spawning threads, initializing
 * mutexes, and so forth.  Also, send the username, client name, and
 * connection name to the server.
 * If initialization was successful, return the connection.  Otherwise,
 * return NULL.  The connection and its resources are freed on failure.
 * This function calls on spd_close to free the connection's resources.
 */

static SPDConnection *init_connection_state(SPDConnection * connection,
					    const char *client_name,
					    const char *connection_name,
					    const char *user_name,
					    SPDConnectionMode mode)
{
	int ret;
	char *set_client_name = NULL;
	pthread_t *our_thread = NULL;

	connection->callback_begin = NULL;
	connection->callback_end = NULL;
	connection->callback_im = NULL;
	connection->callback_pause = NULL;
	connection->callback_resume = NULL;
	connection->callback_cancel = NULL;

	connection->mode = mode;

	/* Create a stream from the socket */
	connection->stream = fdopen(connection->socket, "r");
	if (!connection->stream)
		SPD_FATAL("Can't create a stream for socket, fdopen() failed.");
	/* Switch to line buffering mode */
	ret = setvbuf(connection->stream, NULL, _IONBF, SPD_REPLY_BUF_SIZE);
	if (ret)
		SPD_FATAL("Can't set buffering, setvbuf failed.");

	connection->ssip_mutex = new_mutex();
	if (connection->ssip_mutex == NULL) {
		spd_close(connection);
		return NULL;
	}

	if (mode == SPD_MODE_THREADED) {
		SPD_DBG
		    ("Initializing threads, condition variables and mutexes...");
		connection->cond_reply_ready = new_cond_variable();
		if (connection->cond_reply_ready == NULL) {
			spd_close(connection);
			return NULL;
		}
		connection->cond_reply_ack = new_cond_variable();
		if (connection->cond_reply_ack == NULL) {
			spd_close(connection);
			return NULL;
		}
		connection->mutex_reply_ready = new_mutex();
		if (connection->mutex_reply_ready == NULL) {
			spd_close(connection);
			return NULL;
		}
		connection->mutex_reply_ack = new_mutex();
		if (connection->mutex_reply_ack == NULL) {
			spd_close(connection);
			return NULL;
		}

		our_thread = xmalloc(sizeof(pthread_t));
		ret =
		    pthread_create(our_thread, NULL,
				   spd_events_handler, connection);
		if (ret != 0) {
			SPD_DBG("Thread initialization failed");
			spd_close(connection);
			free(our_thread);
			return NULL;
		}
		connection->events_thread = our_thread;
	}

	/* By now, the connection is created and operational */

	set_client_name =
	    g_strdup_printf("SET SELF CLIENT_NAME \"%s:%s:%s\"", user_name,
			    client_name, connection_name);
	ret = spd_execute_command_wo_mutex(connection, set_client_name);
	g_free(set_client_name);

	if (ret) {
		spd_close(connection);
		connection = NULL;
	}

	return connection;
}

/*
 * Functions for creating and destroying condition variables and mutexes
 * on the heap.
 */

static pthread_cond_t *new_cond_variable(void)
{
	int ret;
	pthread_cond_t *cond = xmalloc(sizeof(pthread_cond_t));

	memset(cond, '\0', sizeof(pthread_cond_t));
	ret = pthread_cond_init(cond, NULL);

	if (ret != 0) {
		free(cond);
		cond = NULL;
	}

	return cond;
}

static void destroy_cond_variable(pthread_cond_t * cond)
{
	if (cond != NULL) {
		pthread_cond_destroy(cond);
		free(cond);
	}
}

static pthread_mutex_t *new_mutex(void)
{
	int ret;
	pthread_mutex_t *mutex = xmalloc(sizeof(pthread_mutex_t));

	memset(mutex, '\0', sizeof(pthread_mutex_t));
	ret = pthread_mutex_init(mutex, NULL);
	if (ret != 0) {
		free(mutex);
		mutex = NULL;
	}
	return mutex;
}

static void destroy_mutex(pthread_mutex_t * mutex)
{
	if (mutex != NULL) {
		pthread_mutex_destroy(mutex);
		free(mutex);
	}
}
