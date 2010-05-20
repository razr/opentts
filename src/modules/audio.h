
/*
 * audio.h -- Interface between the modules and the audio subsystem.
 *
 * Copyright (C) 2004 Brailcom, o.p.s.
 * Copyright (C) 2010 OpenTTS Developers
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

#ifndef __SPD_AUDIO_H
#define __SPD_AUDIO_H

#include <opentts/opentts_audio_plugin.h>


AudioID *spd_audio_open(char *name, void **pars, char **error);

int spd_audio_play(AudioID * id, AudioTrack track, AudioFormat format);

int spd_audio_stop(AudioID * id);

int spd_audio_close(AudioID * id);

int spd_audio_set_volume(AudioID * id, int volume);

void spd_audio_set_loglevel(AudioID * id, int level);

char const *spd_audio_get_playcmd(AudioID * id);

#endif /* ifndef #__SPD_AUDIO_H */
