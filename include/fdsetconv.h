/*
 * fdsetconv.h - Conversion of types for the OpenTTS daemon
 *
 * Copyright (C) 2003, 2006 Brailcom, o.p.s.
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

#ifndef FDSETCONV_H
#define FDSETCONV_H

#include <stdio.h>
#include <string.h>

#include "opentts/opentts_types.h"

char *voice2str(SPDVoiceType voice);

SPDVoiceType str2voice(char *str);

char *punct2str(SPDPunctuation punct);

SPDPunctuation str2punct(char *str);

char *spell2str(SPDSpelling spell);

SPDSpelling str2spell(char *str);

char *recogn2str(SPDCapitalLetters recogn);

SPDCapitalLetters str2recogn(char *str);

SPDPriority str2priority(char *str);

#endif
