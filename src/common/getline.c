/*
 * getline.c - portable implementation of getline
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

/*
     * A portable implementation of getline.
     * Originally provided by Willie Walker, and modified by Chris Brannon
     * to comply with glibc's behavior.
 */

#define INITIAL_BUF_SIZE 120
#define GETLINE_FAILURE -1
ssize_t otts_getline(char **lineptr, size_t * n, FILE * f)
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
