
/*
 * def.h - internal global definitions for OpenTTS
 *
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

#ifndef SPEECHD_DEF_I
#define SPEECHD_DEF_I

#define OPENTTSD_DEFAULT_PORT 6560

#include "opentts/opentts_types.h"

typedef enum {
	SPD_MSGTYPE_TEXT = 0,
	SPD_MSGTYPE_SOUND_ICON = 1,
	SPD_MSGTYPE_CHAR = 2,
	SPD_MSGTYPE_KEY = 3,
	SPD_MSGTYPE_SPELL = 99
} SPDMessageType;

typedef struct {
	signed int rate;
	signed int pitch;
	signed int volume;

	SPDPunctuation punctuation_mode;
	SPDSpelling spelling_mode;
	SPDCapitalLetters cap_let_recogn;

	SPDVoiceType voice_type;
	SPDVoice voice;
} OTTS_MsgSettings;

#endif
