/*
 * libdumbtts.h - dumb synthesizers helper library
 * Copyright (C) Bohdan R. Rau 2008 <ethanak@polip.com>
 *
 * libdumbtts.h is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libdumbtts.h is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libdumbtts.h.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef LIBDUMBTTS_H
#define LIBDUMBTTS_H 1

#include <wchar.h>

struct dumbtts_conf *dumbtts_TTSInit(char *lang);
struct dumbtts_conf *dumbtts_TTSInitWithFlags(char *lang,int flags);
void dumbtts_TTSFree(struct dumbtts_conf *conf);
void dumbtts_LoadDictionary(struct dumbtts_conf *conf,char *fname);

#define DUMBTTS_NO_DICTS 1
#define DUMBTTS_DONT_SPELL_UCASE 2

/*

  Arguments:
  outbuf - output buffer or NULL
  len - size of output buffer
  capMode - announce capital letters
  isCap - if not NULL, functions set it to 1 on capitals
  punctMode - punctuation mode. Set to 0 (no punctuation),
	1 (some), 2 (all)
  punctChars - if punctMode is set to 'some', should contain
	utf-8 encoded string with all punctuation characters
	which should be spoken
  punctLeave - ASCII string containing characters which should be
	passed to synthesizer.

  Return value:

  0 - argument is converted
  >0 - minimum size of output buffer needed
  <0 - conversion error
*/



/* convert wide-character to speakable string */

int dumbtts_WCharString(
	struct dumbtts_conf *conf,
	wchar_t wc,
	char *outbuf,
	int len,
	int capMode,
	int *isCap);

/* convert utf-8 character to speakable string */

int dumbtts_CharString(
	struct dumbtts_conf *conf,
	char *ch,
	char *outbuf,
	int len,
	int capMode,
	int *isCap);

/* convert key name to speakable string */

int dumbtts_KeyString(
	struct dumbtts_conf *conf,
	char  *str,char *outbuf,
	int len,
	int capMode,
	int *isCap);

/* convert utf-8 string to speakable string */

int dumbtts_GetString(
	struct dumbtts_conf *conf,
	char *str,
	char *outbuf,
	int len,
	int punctMode,
	char *punctChars,
	char *punctLeave);


#endif
