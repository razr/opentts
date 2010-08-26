/*
 * mbrola.h -- simple interface for Mbrola binary.
 *
 * Copyright (C) 2010 Bohdan R. Rau <ethanak@polip.com>
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this package; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _INC_MBROLA_H
#define _INC_MBROLA_H

struct otts_mbrolaInfo {
	char *lang;
	char *code;
	char *path;
	int sex;
	int freq;
};

struct otts_mbrolaHandler {
	struct otts_mbrolaInfo *voice;
};

struct otts_mbrolaHandler *otts_mbrolaInit(char *code);
struct otts_mbrolaInfo *otts_mbrolaVoiceInfo(char *code);
int otts_mbrolaSpeak(struct otts_mbrolaHandler *h,
		char *phonemes,
		int speed,
		int pitch,
		int volume,
		int contrast
		);

void otts_mbrolaModLoad(void);

#endif
