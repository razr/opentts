/*
 * timestamp.c - Generate timestamps for log messages
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>
#include <glib.h>
#include "timestamp.h"

/*
 * init_timestamps - initialize timestamp creation code.
 * This function needs to be called at the start of any program which
 * uses timestamps.
 */

void init_timestamps(void)
{
	char *locale_str = NULL;

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
		exit(1);
	}
}

#define TIME_BUFSIZE 256

/*
 * get_timestamp - generates a timestamp corresponding to the current time.
 * The timestamp has the form:
 * [Mon May 10 20:24:29 2010 : 838482]
 * Returns: a dynamically allocated string.  The caller must free it
 * with g_free.
 */

char *get_timestamp(void)
{
	char *timestamp = g_malloc(TIME_BUFSIZE);
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
	strftime_chars = strftime(timestamp, TIME_BUFSIZE,
				  "[%a %d %b %H:%M:%S %Y", &time_parts);
	if ((strftime_chars == 0) || (strftime_chars >= TIME_BUFSIZE)) {
		strcpy(timestamp, "[unknown timestamp]");
		return timestamp;
	}

	remaining = TIME_BUFSIZE - strftime_chars;

	snprintf_chars =
	    snprintf(timestamp + strftime_chars, remaining, " : %d]",
		     (int)current_time.tv_usec);
	if ((snprintf_chars < 0) || (snprintf_chars >= remaining)) {
		/* snprintf failed. */
		strcpy(timestamp, "[unknown timestamp]");
	}

	return timestamp;
}
