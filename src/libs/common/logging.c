/*
* logging.c - write out log mesages
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

#include <assert.h>
#include <glib.h>
#include <locale.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<logging.h>

/* constants for max and min allowed values for debug and log_level */
static const int max_log_level = OTTS_LOG_DEBUG;
static const int min_log_level = OTTS_LOG_CRIT;
static const int max_debug = DEBUG_ON;
static const int min_debug = DEBUG_OFF;
static const int time_bufsize = 255;

static FILE *debug_file = NULL;
static FILE *log_file = NULL;
static FILE *custom_log_file = NULL;
static char *custom_type = NULL;

static int log_level = OTTS_LOG_ERR;
static int debug = DEBUG_OFF;
static pthread_mutex_t logging_lock;

static inline void log_msg_helper(FILE * out_file, const char *timestamp,
				  const char *format, va_list args)
{
	fprintf(out_file, "%s  ", timestamp);
	vfprintf(out_file, format, args);
	fputc('\n', out_file);
	fflush(out_file);
}

static char *get_timestamp(void)
{
	char *timestamp = g_malloc(time_bufsize);
	struct timeval current_time;
	struct tm time_parts;
	struct tm *localtime_success;
	size_t remaining;	/* Buffer size after strftime */
	size_t strftime_chars;	/* strftime's return value */
	int snprintf_chars;	/* snprintf's return value */

	gettimeofday(&current_time, NULL);
	localtime_success = localtime_r(&current_time.tv_sec, &time_parts);
	if (localtime_success == NULL) {
		strcpy(timestamp, "[unknown timestamp]");
		return timestamp;
	}

	/*
	 * This strftime call creates the first part of the timestamp.  E.G.,
	 * [Tue May 11 09:45:00 2010
	 */
	strftime_chars = strftime(timestamp, time_bufsize,
				  "[%a %d %b %H:%M:%S %Y", &time_parts);
	if ((strftime_chars == 0) || (strftime_chars >= time_bufsize)) {
		strcpy(timestamp, "[unknown timestamp]");
		return timestamp;
	}

	remaining = time_bufsize - strftime_chars;

	snprintf_chars =
	    snprintf(timestamp + strftime_chars, remaining, " : %d]",
		     (int)current_time.tv_usec);
	if ((snprintf_chars < 0) || (snprintf_chars >= remaining)) {
		/* snprintf failed. */
		strcpy(timestamp, "[unknown timestamp]");
	}

	return timestamp;
}

static FILE *open_log_file(char *name)
{
	FILE *fp;

	assert(name);
	if (!strcmp(name, "stderr")) {
		fp = stderr;
	} else if (!strcmp(name, "stdout")) {
/* this case existed in openttsd.c but it doesn't make much sense */
		fp = stdout;
	} else {
		fp = fopen(name, "a");
		if (fp == NULL) {
			perror
			    ("can't open new log file, using stderr instead.");
			fp = stderr;
		} else {
			if (chmod(name, S_IRUSR | S_IWUSR))
				perror("can't change permission of log file");
/* there is no reason for log files to stay open accross exec so set FD_CLOEXEC */
fcntl(fileno(fp), F_SETFD, FD_CLOEXEC);
		}
	}
	return fp;
}

/* must be called before any logging takes place */
void init_logging(void)
{
	int err = 0;
	char *locale_str = NULL;

	err = pthread_mutex_init(&logging_lock, NULL);
	if (err) {
		fprintf(stderr, "failed to initialize logging mutex!\n");
		exit(1);
	}

	/*
	 * According to the manpage, portable code should call tzset
	 * before the first use of localtime_r in a program.
	 * Our code uses localtime_r, so we call tzset here.
	 *
	 * Also, timestamps should use standard abbreviations for months
	 * and days.  This is what the setlocale call insures.
	 */
	tzset();
	locale_str = setlocale(LC_TIME, "POSIX");
	if (locale_str == NULL) {
		fprintf(stderr, "Unable to initialize time stamp library.\n");
	}
}

/*
 * open log file
 * parameters
 * new_log_file new file to log to in case of null old value is left
 * new_log_level new min log level if invalid the old value is unchanged
 */
void open_log(char *new_log_file, enum LogLevel new_log_level)
{
	pthread_mutex_lock(&logging_lock);

	if (new_log_file != NULL) {
		if (log_file != NULL) {
			if ((log_file != stderr) && (log_file != stdout))
				fclose(log_file);
		}
		log_file = open_log_file(new_log_file);
	}

	if (min_log_level <= new_log_level && new_log_level <= max_log_level)
		log_level = new_log_level;

	pthread_mutex_unlock(&logging_lock);
}

void close_log(void)
{
	pthread_mutex_lock(&logging_lock);
	if (log_file != NULL) {
		if ((log_file != stderr) && (log_file != stdout))
			fclose(log_file);
		log_file = NULL;
	}
	pthread_mutex_unlock(&logging_lock);
}

/*
* open debug log
* parameters
*  new_debug_file new file to write debug messages to if debug is set, if null the old value is left
* new_debug new debug value if invalid old value is let
*/
void open_debug_log(char *new_debug_file, enum DebugLevel new_debug)
{
	pthread_mutex_lock(&logging_lock);

	if (new_debug_file != NULL) {
		if (debug_file != NULL) {
			if ((debug_file != stderr) && (debug_file != stdout))
				fclose(debug_file);
			debug_file = NULL;
		}
		debug_file = open_log_file(new_debug_file);
	}

	if (min_debug <= new_debug && new_debug <= max_debug)
		debug = new_debug;

	pthread_mutex_unlock(&logging_lock);
}

void close_debug_log(void)
{
	pthread_mutex_lock(&logging_lock);
	if (debug_file != NULL) {
		if ((debug_file != stderr) && (debug_file != stdout))
			fclose(debug_file);
		debug_file = NULL;
	}

	debug = DEBUG_OFF;
	pthread_mutex_unlock(&logging_lock);
}

void open_custom_log(char *file_name, char *new_type)
{
	pthread_mutex_lock(&logging_lock);

	if (file_name != NULL) {
		if (custom_log_file != NULL) {
			if ((custom_log_file != stderr)
			    && (custom_log_file != stdout))
				fclose(custom_log_file);
		}
		custom_log_file = open_log_file(file_name);
	}

	if (new_type) {
		g_free(custom_type);
		custom_type = g_strdup(new_type);
	}

	pthread_mutex_unlock(&logging_lock);
}

void close_custom_log(void)
{
	pthread_mutex_lock(&logging_lock);
	if (custom_log_file != NULL) {
		if ((custom_log_file != stderr) && (custom_log_file != stdout))
			fclose(custom_log_file);
		custom_log_file = NULL;
	}
	pthread_mutex_unlock(&logging_lock);
}

/*
* write a log message to the relevant files
* log_level the logging priority of the message
* format and args same as printf
*/
void log_msg(enum LogLevel level, char *format, ...)
{
	char *tstr;
	va_list args;

	pthread_mutex_lock(&logging_lock);
	assert(format);
	if ((level <= log_level) || debug == DEBUG_ON) {
		/* Print timestamp */
		tstr = get_timestamp();
		if ((level <= log_level) && log_file) {
			va_start(args, format);
			log_msg_helper(log_file, tstr, format, args);
			va_end(args);
		}
		if (debug == DEBUG_ON && debug_file) {
			va_start(args, format);
			log_msg_helper(debug_file, tstr, format, args);
			va_end(args);
		}
		g_free(tstr);
	}
	pthread_mutex_unlock(&logging_lock);
}

void log_msg2(enum LogLevel level, char *kind, char *format, ...)
{
	int std_log;
	int custom_log;
	va_list args;
	char *tstr;

	pthread_mutex_lock(&logging_lock);
	assert(format);
	tstr = get_timestamp();
	std_log = (level <= log_level && log_file);
	custom_log = (kind && custom_type && !strcmp(kind, custom_type)
		      && custom_log_file);
	if (std_log) {
		va_start(args, format);
		log_msg_helper(log_file, tstr, format, args);
		va_end(args);
	}
	if (custom_log) {
		va_start(args, format);
		log_msg_helper(custom_log_file, tstr, format, args);
		va_end(args);
	}
	if (debug == DEBUG_ON && debug_file) {
		va_start(args, format);
		log_msg_helper(debug_file, tstr, format, args);
		va_end(args);
	}

	pthread_mutex_unlock(&logging_lock);
	g_free(tstr);
}

/* must be called after all logging is complete */
void logging_end(void)
{
	pthread_mutex_destroy(&logging_lock);
}
