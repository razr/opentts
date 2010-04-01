
/*
 * spd_audio.c -- Spd Audio Output Library
 *
 * Copyright (C) 2004, 2006 Brailcom, o.p.s.
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
 * $Id: spd_audio.c,v 1.21 2008-06-09 10:29:12 hanke Exp $
 */

/* 
 * spd_audio is a simple realtime audio output library with the capability of
 * playing 8 or 16 bit data, immediate stop and synchronization. This library
 * currently provides OSS, NAS, ALSA and PulseAudio backend. The available backends are
 * specified at compile-time using the directives WITH_OSS, WITH_NAS, WITH_ALSA, 
 * WITH_PULSE, WITH_LIBAO but the user program is allowed to switch between them at run-time.
 */

#include "spd_audio.h"

#include <stdio.h>
#include <string.h>
#include <sys/soundcard.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

static int spd_audio_log_level;

/* Open the audio device.

   Arguments:
   type -- The requested device. Currently AudioOSS or AudioNAS.
   pars -- and array of pointers to parameters to pass to
           the device backend, terminated by a NULL pointer.
           See the source/documentation of each specific backend.
   error -- a pointer to the string where error description is
           stored in case of failure (returned AudioID == NULL).
           Otherwise will contain NULL.

   Return value:
   Newly allocated AudioID structure that can be passed to
   all other spd_audio functions, or NULL in case of failure.

*/
AudioID*
spd_audio_open(char *name, void **pars, char **error)
{
    AudioID *id;
    struct spd_audio_plugin *function;

    *error = NULL;

    if (!strcmp(name,"oss")){
#ifdef WITH_OSS
	function = oss_plugin_get();
	function->set_loglevel(spd_audio_log_level);

	if (function->open != NULL){
	    id = function->open(pars);
	    if (id == NULL){
		*error = (char*) strdup("Couldn't open OSS device.");
		return NULL;
	    }
	}
	else{
	    *error = (char*) strdup("Couldn't open OSS device module.");
	    return NULL;
	}
#else
	*error = strdup("The sound library wasn't compiled with OSS support.");
	return NULL;
#endif       

    }
    else if (!strcmp(name,"alsa")){
#ifdef WITH_ALSA
	function = alsa_plugin_get();
	function->set_loglevel(spd_audio_log_level);

	if (function->open != NULL){
	    id = function->open(&pars[1]);
	    if (id == NULL){
		*error = (char*) strdup("Couldn't open ALSA device.");
		return NULL;
	    }
	}
	else{
	    *error = (char*) strdup("Couldn't open ALSA device module.");
	    return NULL;
	}
#else
	*error = strdup("The sound library wasn't compiled with Alsa support.");
	return NULL;
#endif
    }
    else if (!strcmp(name,"nas")){
#ifdef WITH_NAS
	function = nas_plugin_get();
	function->set_loglevel(spd_audio_log_level);

	if (function->open != NULL){
	    id = function->open(&pars[2]);
	    if (id == NULL){
		*error = (char*) strdup("Couldn't open connection to the NAS server.");
		return NULL;
	    }
	}
	else{
	    *error = (char*) strdup("Couldn't open NAS device module.");
	    return NULL;
	}
#else
	*error = strdup("The sound library wasn't compiled with NAS support.");
	return NULL;
#endif
    }
    else if (!strcmp(name,"pulse")){
#ifdef WITH_PULSE
	function = pulse_plugin_get();
	function->set_loglevel(spd_audio_log_level);

	if (function->open != NULL){
	    id = function->open(&pars[3]);
	    if (id == NULL){
		*error = (char*) strdup("Couldn't open connection to the PulseAudio server.");
		return NULL;
	    }
	}
	else{
	    *error = (char*) strdup("Couldn't open PulseAudio device module.");
	    return NULL;
	}
#else
	*error = strdup("The sound library wasn't compiled with PulseAudio support.");
	return NULL;
#endif
    }
    else if (!strcmp(name,"libao")){
#ifdef WITH_LIBAO
	function = libao_plugin_get();
	function->set_loglevel(spd_audio_log_level);

	if (function->open != NULL){
	    id =  function->open(pars);
	    if (id == NULL){
		*error = (char*) strdup("Couldn't open libao");
		return NULL;
	    }
	}
	else{
	    *error = (char*) strdup("Couldn't open libao  module.");
	    return NULL;
	}
#else
	*error = strdup("The sound library wasn't compiled with libao support.");
	return NULL;
#endif
    }
    else{
	*error = (char*) strdup("Unknown device");
	return NULL;
    }

    id->function = function;
#if defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
    id->format = SPD_AUDIO_BE;
#else
    id->format = SPD_AUDIO_LE;
#endif

    return id;
}

/* Play a track on the audio device (blocking).

   Arguments:
   id -- the AudioID* of the device returned by spd_audio_open
   track -- a track to play (see spd_audio.h)

   Return value:
   0 if everything is ok, a non-zero value in case of failure.
   See the particular backend documentation or source for the
   meaning of these non-zero values.

   Comment:
   spd_audio_play() is a blocking function. It returns exactly
   when the given track stopped playing. However, it's possible
   to safely interrupt it using spd_audio_stop() described bellow.
   (spd_audio_stop() needs to be called from another thread, obviously.)

*/
int
spd_audio_play(AudioID *id, AudioTrack track, AudioFormat format)
{
    int ret;

    if (id && id->function->play){
        /* Only perform byte swapping if the driver in use has given us audio in
	   an endian format other than what the running CPU supports. */
        if (format != id->format){
                unsigned char *out_ptr, *out_end, c;
                out_ptr = (unsigned char *) track.samples;
                out_end = out_ptr + track.num_samples*2 * track.num_channels;
                while(out_ptr < out_end){
                    c = out_ptr[0];
                    out_ptr[0] = out_ptr[1];
                    out_ptr[1] = c;
                    out_ptr += 2;
                }
        }
	ret = id->function->play(id, track);
    }
    else{
	fprintf(stderr, "Play not supported on this device\n");
	return -1;
    }

    return ret;
}

/* Stop playing the current track on device id

Arguments:
   id -- the AudioID* of the device returned by spd_audio_open

Return value:
   0 if everything is ok, a non-zero value in case of failure.
   See the particular backend documentation or source for the
   meaning of these non-zero values.

Comment:
   spd_audio_stop() safely interrupts spd_audio_play() when called
   from another thread. It shouldn't cause any clicks or unwanted
   effects in the sound output.

   It's safe to call spd_audio_stop() even if the device isn't playing
   any track. In that case, it does nothing. However, there is a danger
   when using spd_audio_stop(). Since you must obviously do it from
   another thread than where spd_audio_play is running, you must make
   yourself sure that the device is still open and the id you pass it
   is valid and will be valid until spd_audio_stop returns. In other words,
   you should use some mutex or other synchronization device to be sure
   spd_audio_close isn't called before or during spd_audio_stop execution.
*/

int
spd_audio_stop(AudioID *id)
{
    int ret;
    if (id && id->function->stop){
	ret = id->function->stop(id);
    }
    else{
	fprintf(stderr, "Stop not supported on this device\n");
	return -1;
    }
    return ret;
}

/* Close the audio device id

Arguments:
   id -- the AudioID* of the device returned by spd_audio_open

Return value:
   0 if everything is ok, a non-zero value in case of failure.

Comments:

   Please make sure no other spd_audio function with this device id
   is running in another threads. See spd_audio_stop() for detailed
   description of possible problems.
*/
int
spd_audio_close(AudioID *id)
{
    int ret;
    if (id && id->function->close){
	return (id->function->close(id));
    }
    return -1;
}

/* Set volume for playing tracks on the device id

Arguments:
   id -- the AudioID* of the device returned by spd_audio_open
   volume -- a value in the range <-100:100> where -100 means the
             least volume (probably silence), 0 the default volume
	     and +100 the highest volume possible to make on that
	     device for a single flow (i.e. not using mixer).

Return value:
   0 if everything is ok, a non-zero value in case of failure.
   See the particular backend documentation or source for the
   meaning of these non-zero values.

Comments:

   In case of /dev/dsp, it's not possible to set volume for
   the particular flow. For that reason, the value 0 means
   the volume the track was recorded on and each smaller value
   means less volume (since this works by deviding the samples
   in the track by a constant).
*/
int
spd_audio_set_volume(AudioID *id, int volume)
{
    if ((volume > 100) || (volume < -100)){
	fprintf(stderr, "Requested volume out of range");
	return -1;
    }

    id->volume = volume;
    return 0;
}

void
spd_audio_set_loglevel(AudioID *id, int level)
{
    if (level){
        spd_audio_log_level = level;
	if (id != 0 && id->function != 0)
	    id->function->set_loglevel(level);
    }
}

char const *
spd_audio_get_playcmd(AudioID *id)
{
	if (id != 0 && id->function != 0) {
	    return id->function->get_playcmd();
    }
    return NULL;
}
