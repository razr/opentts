/*
 *opentts_types.h - OpenTTS definitions
 *
 * Copyright (C) 2010 OpenTTS developers
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

#ifndef _OPENTTS_TYPES_H
#define _OPENTTS_TYPES_H

#define OTTS_VOICE_RATE_MIN		(-100)
#define OTTS_VOICE_RATE_MAX		(100)
#define OTTS_VOICE_RATE_DEFAULT		(0)

#define OTTS_VOICE_PITCH_MIN		(-100)
#define OTTS_VOICE_PITCH_MAX		(100)
#define OTTS_VOICE_PITCH_DEFAULT	(0)

#define OTTS_VOICE_VOLUME_MIN		(-100)
#define OTTS_VOICE_VOLUME_MAX		(100)
#define OTTS_VOICE_VOLUME_DEFAULT	(0)

/*
 * Constants surrounded by #ifdef OPENTTS_INTERNAL are not meant for use
 * by clients.
 */

typedef enum {
#ifdef OPENTTS_INTERNAL
	SPD_PUNCT_ERR = -1,
#endif
	SPD_PUNCT_ALL = 0,
	SPD_PUNCT_NONE = 1,
	SPD_PUNCT_SOME = 2
} SPDPunctuation;

typedef enum {
#ifdef OPENTTS_INTERNAL
	SPD_CAP_ERR = -1,
#endif
	SPD_CAP_NONE = 0,
	SPD_CAP_SPELL = 1,
	SPD_CAP_ICON = 2
} SPDCapitalLetters;

typedef enum {
#ifdef OPENTTS_INTERNAL
	SPD_SPELL_ERR = -1,
#endif
	SPD_SPELL_OFF = 0,
	SPD_SPELL_ON = 1
} SPDSpelling;

typedef enum {
#ifdef OPENTTS_INTERNAL
	SPD_NO_VOICE = 0,
#endif
	SPD_MALE1 = 1,
	SPD_MALE2 = 2,
	SPD_MALE3 = 3,
	SPD_FEMALE1 = 4,
	SPD_FEMALE2 = 5,
	SPD_FEMALE3 = 6,
	SPD_CHILD_MALE = 7,
	SPD_CHILD_FEMALE = 8
} SPDVoiceType;

typedef struct {
	char *name;		/* Name of the voice (id) */
	char *language;		/* 2-letter ISO language code */
	char *variant;		/* a not-well defined string describing dialect etc. */
} SPDVoice;

typedef enum {
#ifdef OPENTTS_INTERNAL
	SPD_NOTHING = 0,
#endif
	SPD_BEGIN = 1,
	SPD_END = 2,
	SPD_INDEX_MARKS = 4,
	SPD_CANCEL = 8,
	SPD_PAUSE = 16,
	SPD_RESUME = 32
} SPDNotification;

typedef enum {
	SPD_EVENT_BEGIN,
	SPD_EVENT_END,
	SPD_EVENT_CANCEL,
	SPD_EVENT_PAUSE,
	SPD_EVENT_RESUME,
	SPD_EVENT_INDEX_MARK
} SPDNotificationType;

typedef enum {
	SPD_DATA_TEXT = 0,
	SPD_DATA_SSML = 1
} SPDDataMode;

typedef enum {
#ifdef OPENTTS_INTERNAL
	SPD_PRIORITY_ERR = -1,
#endif
	SPD_IMPORTANT = 1,
	SPD_MESSAGE = 2,
	SPD_TEXT = 3,
	SPD_NOTIFICATION = 4,
	SPD_PROGRESS = 5
} SPDPriority;

typedef enum {
	SPD_MSGTYPE_TEXT = 0,
	SPD_MSGTYPE_SOUND_ICON = 1,
	SPD_MSGTYPE_CHAR = 2,
	SPD_MSGTYPE_KEY = 3,
	SPD_MSGTYPE_SPELL = 99
} SPDMessageType;

#endif /* ifndef _OPENTTS_TYPES_H */
