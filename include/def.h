
/*
 * def.h - internal global definitions for OpenTTS
 *
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
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
 */

#ifndef DEF_H
#define DEF_H

#include "opentts/opentts_types.h"

#define OPENTTSD_DEFAULT_PORT		6560
#define OPENTTSD_DEFAULT_MODE		DAEMON
#define OPENTTSD_DEFAULT_LANGUAGE	"en"

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
