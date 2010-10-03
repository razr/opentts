/*
 * spd_synth_plugin.h -- The OpenTTS synthesyzer plugin header
 *
 * Copyright (C) 2010 Andrei Kholodnyi
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

#ifndef __OTTS_SYNTH_PLUGIN_H
#define __OTTS_SYNTH_PLUGIN_H

#include <stdlib.h>

#include "opentts_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYNTH_PLUGIN_ENTRY synth_plugin_get

typedef struct otts_synth_plugin {
    const char * name;
    const char * version;
    int (* load) (void);
    int (* init) (char **status_info);
    int (* audio_init) (char **status_info);
    int  (* speak) (char *data, size_t bytes, SPDMessageType type);
    int   (* stop) (void);
    SPDVoice ** (* list_voices) (void);
    size_t  (* pause) (void);
    void (* close) (int status);
} otts_synth_plugin_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ifndef #__OTTS_SYNTH_PLUGIN_H */
