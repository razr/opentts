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

typedef enum {
	SPD_PUNCT_ALL = 0,
	SPD_PUNCT_NONE = 1,
	SPD_PUNCT_SOME = 2
} SPDPunctuation;

typedef enum {
	SPD_CAP_NONE = 0,
	SPD_CAP_SPELL = 1,
	SPD_CAP_ICON = 2
} SPDCapitalLetters;

typedef enum {
	SPD_SPELL_OFF = 0,
	SPD_SPELL_ON = 1
} SPDSpelling;

typedef enum {
	SPD_NO_VOICE = 0,
	SPD_MALE1 = 1,
	SPD_MALE2 = 2,
	SPD_MALE3 = 3,
	SPD_FEMALE1 = 4,
	SPD_FEMALE2 = 5,
	SPD_FEMALE3 = 6,
	SPD_CHILD_MALE = 7,
	SPD_CHILD_FEMALE = 8
} SPDVoiceType;

#endif /* ifndef _OPENTTS_TYPES_H */
