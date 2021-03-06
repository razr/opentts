
/*
 * alsa.c -- The Advanced Linux Sound System backend for OpenTTS
 *
 * Copyright (C) 2005,2006 Brailcom, o.p.s.
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
 * $Id: alsa.c,v 1.30 2008-10-15 17:27:32 hanke Exp $
 */

/* NOTE: This module uses the non-blocking write() / poll() approach to
    alsa-lib functions.*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <glib.h>

#include <alsa/asoundlib.h>

#define AUDIO_PLUGIN_ENTRY otts_alsa_LTX_audio_plugin_get
#include <opentts/opentts_audio_plugin.h>
#include <opentts/opentts_types.h>
#include<logging.h>

typedef struct {
	AudioID id;
	snd_pcm_t *alsa_pcm;	/* identifier of the ALSA device */
	snd_pcm_hw_params_t *alsa_hw_params;	/* parameters of sound */
	snd_pcm_sw_params_t *alsa_sw_params;	/* parameters of playback */
	snd_pcm_uframes_t alsa_buffer_size;
	pthread_mutex_t alsa_pcm_mutex;	/* mutex to guard the state of the device */
	pthread_mutex_t alsa_pipe_mutex;	/* mutex to guard the stop pipes */
	int alsa_stop_pipe[2];	/* Pipe for communication about stop requests */
	int alsa_fd_count;	/* Counter of descriptors to poll */
	struct pollfd *alsa_poll_fds;	/* Descriptors to poll */
	int alsa_opened;	/* 1 between snd_pcm_open and _close, 0 otherwise */
	char *alsa_device_name;	/* the name of the device to open */
} alsa_id_t;

static int _alsa_close(alsa_id_t * id);
static int _alsa_open(alsa_id_t * id);

static int xrun(alsa_id_t * id);
static int suspend(alsa_id_t * id);

static int wait_for_poll(alsa_id_t * id, struct pollfd *alsa_poll_fds,
			 unsigned int count, int draining);

#ifndef timersub
#define	timersub(a, b, result) \
do { \
         (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	 (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	 if ((result)->tv_usec < 0) { \
		 --(result)->tv_sec; \
		 (result)->tv_usec += 1000000; \
	 } \
 } while (0)
#endif

static char const *alsa_play_cmd = "aplay";
static logging_func audio_log;

/* I/O error handler */
static int xrun(alsa_id_t * id)
{
	snd_pcm_status_t *status;
	int res;

	if (id == NULL)
		return -1;

	audio_log(OTTS_LOG_ERR, "alsa: WARNING: Entering XRUN handler");

	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(id->alsa_pcm, status)) < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: status error: %s", snd_strerror(res));

		return -1;
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		struct timeval now, diff, tstamp;
		gettimeofday(&now, 0);
		snd_pcm_status_get_trigger_tstamp(status, &tstamp);
		timersub(&now, &tstamp, &diff);
		audio_log(OTTS_LOG_ERR, "alsa: underrun!!! (at least %.3f ms long)",
			  diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
		if ((res = snd_pcm_prepare(id->alsa_pcm)) < 0) {
			audio_log(OTTS_LOG_CRIT, "alsa: xrun: prepare error: %s",
				  snd_strerror(res));

			return -1;
		}

		return 0;	/* ok, data should be accepted again */
	}
	audio_log(OTTS_LOG_CRIT, "alsa: read/write error, state = %s",
		  snd_pcm_state_name(snd_pcm_status_get_state(status)));

	return -1;
}

/* I/O suspend handler */
static int suspend(alsa_id_t * id)
{
	int res;

	audio_log(OTTS_LOG_ERR, "alsa: WARNING: Entering SUSPEND handler.");

	if (id == NULL)
		return -1;

	while ((res = snd_pcm_resume(id->alsa_pcm)) == -EAGAIN)
		sleep(1);	/* wait until suspend flag is released */

	if (res < 0) {
		if ((res = snd_pcm_prepare(id->alsa_pcm)) < 0) {
			audio_log(OTTS_LOG_CRIT, "alsa: suspend: prepare error: %s",
				  snd_strerror(res));

			return -1;
		}
	}

	return 0;
}

/* Open the device so that it's ready for playing on the default
   device. Internal function used by the public alsa_open. */
static int _alsa_open(alsa_id_t * id)
{
	int err;

	audio_log(OTTS_LOG_ERR, "Opening ALSA device");
	fflush(stderr);

	/* Open the device */
	if ((err = snd_pcm_open(&id->alsa_pcm, id->alsa_device_name,
				SND_PCM_STREAM_PLAYBACK,
				SND_PCM_NONBLOCK)) < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Cannot open audio device %s (%s)",
			  id->alsa_device_name, snd_strerror(err));
		return -1;
	}

	/* Allocate space for hw_params (description of the sound parameters) */
	/* Allocate space for sw_params (description of the sound parameters) */
	audio_log(OTTS_LOG_WARN, "alsa: Allocating new sw_params structure");
	if ((err = snd_pcm_sw_params_malloc(&id->alsa_sw_params)) < 0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Cannot allocate hardware parameter structure (%s)",
			  snd_strerror(err));
		return -1;
	}

	audio_log(OTTS_LOG_ERR, "alsa: Opening ALSA device ... success");

	return 0;
}

/* 
   Close the device. Internal function used by public alsa_close. 
*/

static int _alsa_close(alsa_id_t * id)
{
	int err;

	audio_log(OTTS_LOG_ERR, "Closing ALSA device");

	if (id->alsa_opened == 0)
		return 0;

	pthread_mutex_lock(&id->alsa_pipe_mutex);
	id->alsa_opened = 0;

	if ((err = snd_pcm_close(id->alsa_pcm)) < 0) {
		audio_log(OTTS_LOG_WARN, "alsa: Cannot close ALSA device (%s)",
			  snd_strerror(err));
		return -1;
	}

	snd_pcm_sw_params_free(id->alsa_sw_params);

	g_free(id->alsa_poll_fds);
	id->alsa_poll_fds = NULL;
	pthread_mutex_unlock(&id->alsa_pipe_mutex);

	audio_log(OTTS_LOG_ERR, "alsa: Closing ALSA device ... success");

	return 0;
}

/* Open ALSA for playback.
  
  These parameters are passed in pars:
  (char*) pars[0] ... null-terminated string containing the name
                      of the device to be used for sound output
                      on ALSA
  (void*) pars[1] ... =NULL
*/
static AudioID *alsa_open(void **pars, logging_func log)
{
	alsa_id_t *alsa_id;
	int ret;

	if (audio_log == NULL)
		audio_log = log;

	if (pars[1] == NULL) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Can't open ALSA sound output, missing parameters in argument.");
		return NULL;
	}

	alsa_id = (alsa_id_t *) g_malloc(sizeof(alsa_id_t));

	pthread_mutex_init(&alsa_id->alsa_pipe_mutex, NULL);

	alsa_id->alsa_opened = 0;

	audio_log(OTTS_LOG_ERR, "alsa: Opening ALSA sound output");

	alsa_id->alsa_device_name = g_strdup(pars[1]);
	alsa_id->alsa_poll_fds = NULL;

	ret = _alsa_open(alsa_id);
	if (ret) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Cannot initialize Alsa device '%s': Can't open.",
			  alsa_id->alsa_device_name);
		g_free(alsa_id);
		return NULL;
	}

	audio_log(OTTS_LOG_ERR, "alsa: Device '%s' initialized succesfully.",
		  alsa_id->alsa_device_name);

	return (AudioID *) alsa_id;
}

/* Close ALSA */
static int alsa_close(AudioID * id)
{
	int err;
	alsa_id_t *alsa_id = (alsa_id_t *) id;

	/* Close device */
	if ((err = _alsa_close(alsa_id)) < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Cannot close audio device");
		return -1;
	}
	audio_log(OTTS_LOG_ERR, "alsa: ALSA closed.");

	g_free(alsa_id->alsa_device_name);
	g_free(alsa_id);
	id = NULL;

	return 0;
}

/* Wait until ALSA is readdy for more samples or alsa_stop() was called.

Returns 0 if ALSA is ready for more input, +1 if a request to stop
the sound output was received and a negative value on error.  */

int wait_for_poll(alsa_id_t * id, struct pollfd *alsa_poll_fds,
		  unsigned int count, int draining)
{
	unsigned short revents;
	snd_pcm_state_t state;
	int ret;

	//      audio_log("Waiting for poll");

	/* Wait for certain events */
	while (1) {
		ret = poll(id->alsa_poll_fds, count, -1);
		//      audio_log("wait_for_poll: activity on %d descriptors", ret);

		/* Check for stop request from alsa_stop on the last file
		   descriptors */
		if (revents = id->alsa_poll_fds[count - 1].revents) {
			if (revents & POLLIN) {
				audio_log(OTTS_LOG_INFO,
					  "alsa: wait_for_poll: stop requested");
				return 1;
			}
		}

		/* Check the first count-1 descriptors for ALSA events */
		snd_pcm_poll_descriptors_revents(id->alsa_pcm,
						 id->alsa_poll_fds, count - 1,
						 &revents);

		/* Ensure we are in the right state */
		state = snd_pcm_state(id->alsa_pcm);
		//      audio_log("State after poll returned is %s", snd_pcm_state_name(state));

		if (SND_PCM_STATE_XRUN == state) {
			if (!draining) {
				audio_log(OTTS_LOG_ERR,
					  "alsa: WARNING: Buffer underrun detected!");
				if (xrun(id) != 0)
					return -1;
				return 0;
			} else {
				audio_log(OTTS_LOG_INFO, "alsa: Poll: Playback terminated");
				return 0;
			}
		}

		if (SND_PCM_STATE_SUSPENDED == state) {
			audio_log(OTTS_LOG_ERR, "alsa: WARNING: Suspend detected!");
			if (suspend(id) != 0)
				return -1;
			return 0;
		}

		/* Check for errors */
		if (revents & POLLERR) {
			audio_log(OTTS_LOG_INFO,
				  "alsa: wait_for_poll: poll revents says POLLERR");
			return -EIO;
		}

		/* Is ALSA ready for more input? */
		if ((revents & POLLOUT)) {
			// audio_log("Poll: Ready for more input");
			return 0;
		}
	}
}

#define ERROR_EXIT()\
    g_free(track_volume.samples); \
    audio_log(OTTS_LOG_CRIT, "alsa_play() abnormal exit"); \
    _alsa_close(alsa_id); \
    return -1;

/* Play the track _track_ (see opentts_audio_plugin.h) using the id->alsa_pcm device and
 id-hw_params parameters. This is a blocking function, however, it's possible
 to interrupt playing from a different thread with alsa_stop(). alsa_play
 returns after and immediatelly after the whole sound was played on the
 speakers.

 The idea is that we get the ALSA file descriptors and we will poll() to see
 when alsa is ready for more input while sleeping in the meantime. We will
 additionally poll() for one more descriptor used by alsa_stop() to notify the
 thread with alsa_play() that the stop of the playback is requested. The
 variable can_be_stopped is used for very simple synchronization between the
 two threads. */
static int alsa_play(AudioID * id, AudioTrack track)
{
	snd_pcm_format_t format;
	int bytes_per_sample;
	int num_bytes;
	alsa_id_t *alsa_id = (alsa_id_t *) id;

	signed short *output_samples;

	AudioTrack track_volume;
	float real_volume;
	int i;

	int err;
	int ret;

	snd_pcm_uframes_t framecount;
	snd_pcm_uframes_t period_size;
	size_t samples_per_period;
	size_t silent_samples;
	size_t volume_size;
	unsigned int sr;

	snd_pcm_state_t state;

	struct pollfd alsa_stop_pipe_pfd;

	if (alsa_id == NULL) {
		audio_log(OTTS_LOG_CRIT, "alsa: Invalid device passed to alsa_play()");
		return -1;
	}

	pthread_mutex_lock(&alsa_id->alsa_pipe_mutex);

	audio_log(OTTS_LOG_WARN, "alsa: Start of playback on ALSA");

	/* Is it not an empty track? */
	/* Passing an empty track is not an error */
	if (track.samples == NULL) {
		pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);
		return 0;
	}

	/*
	 * The system could have suspended to RAM between two calls
	 * to alsa_play, in which case, the state of the device will
	 * be SND_PCM_STATE_SUSPENDED.  We have to recover from that
	 * state before we do anything else with the pcm handle.
	 */
	state = snd_pcm_state(alsa_id->alsa_pcm);
	if (state == SND_PCM_STATE_SUSPENDED) {
		err = suspend(alsa_id);
		if (err != 0) {
			/* Fatal error: can't recover from suspend. */
			audio_log(OTTS_LOG_CRIT,
				  "alsa: Audio playback could not be resumed after a recent suspend.");
			pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);
			return -1;
		}
	}

	/* Allocate space for hw_params (description of the sound parameters) */
	audio_log(OTTS_LOG_WARN, "alsa: Allocating new hw_params structure");
	if ((err = snd_pcm_hw_params_malloc(&alsa_id->alsa_hw_params)) < 0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Cannot allocate hardware parameter structure (%s)",
			  snd_strerror(err));
		pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);
		return -1;
	}

	/* Initialize hw_params on our pcm */
	if ((err =
	     snd_pcm_hw_params_any(alsa_id->alsa_pcm,
				   alsa_id->alsa_hw_params)) < 0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Cannot initialize hardware parameter structure (%s)",
			  snd_strerror(err));
		pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);
		return -1;
	}

	/* Create the pipe for communication about stop requests */
	if (pipe(alsa_id->alsa_stop_pipe)) {
		audio_log(OTTS_LOG_CRIT, "alsa: Stop pipe creation failed (%s)",
			  strerror(errno));
		pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);
		return -1;
	}

	/* Find how many descriptors we will get for poll() */
	alsa_id->alsa_fd_count =
	    snd_pcm_poll_descriptors_count(alsa_id->alsa_pcm);
	if (alsa_id->alsa_fd_count <= 0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Invalid poll descriptors count returned from ALSA.");
		pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);
		return -1;
	}

	/* Create and fill in struct pollfd *alsa_poll_fds with ALSA descriptors */
	alsa_id->alsa_poll_fds =
	    g_malloc((alsa_id->alsa_fd_count + 1) * sizeof(struct pollfd));
	if ((err =
	     snd_pcm_poll_descriptors(alsa_id->alsa_pcm, alsa_id->alsa_poll_fds,
				      alsa_id->alsa_fd_count)) < 0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Unable to obtain poll descriptors for playback: %s\n",
			  snd_strerror(err));
		g_free(alsa_id->alsa_poll_fds);
		alsa_id->alsa_poll_fds = NULL;
		pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);
		return -1;
	}

	/* Create a new pollfd structure for requests by alsa_stop() */
	alsa_stop_pipe_pfd.fd = alsa_id->alsa_stop_pipe[0];
	alsa_stop_pipe_pfd.events = POLLIN;
	alsa_stop_pipe_pfd.revents = 0;

	/* Join this our own pollfd to the ALSAs ones */
	alsa_id->alsa_poll_fds[alsa_id->alsa_fd_count] = alsa_stop_pipe_pfd;
	alsa_id->alsa_fd_count++;

	alsa_id->alsa_opened = 1;
	pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);

	/* Report current state */
	state = snd_pcm_state(alsa_id->alsa_pcm);
	audio_log(OTTS_LOG_INFO, "alsa: PCM state before setting audio parameters: %s",
		  snd_pcm_state_name(state));

	/* Choose the correct format */
	if (track.bits == 16) {
		switch (alsa_id->id.format) {
		case SPD_AUDIO_LE:
			format = SND_PCM_FORMAT_S16_LE;
			break;
		case SPD_AUDIO_BE:
			format = SND_PCM_FORMAT_S16_BE;
			break;
		}
		bytes_per_sample = 2;
	} else if (track.bits == 8) {
		bytes_per_sample = 1;
		format = SND_PCM_FORMAT_S8;
	} else {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Unsupported sound data format, track.bits = %d",
			  track.bits);
		return -1;
	}

	/* Set access mode, bitrate, sample rate and channels */
	audio_log(OTTS_LOG_INFO, "alsa: Setting access type to INTERLEAVED");
	if ((err = snd_pcm_hw_params_set_access(alsa_id->alsa_pcm,
						alsa_id->alsa_hw_params,
						SND_PCM_ACCESS_RW_INTERLEAVED)
	    ) < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Cannot set access type (%s)",
			  snd_strerror(err));
		return -1;
	}

	audio_log(OTTS_LOG_INFO, "alsa: Setting sample format to %s",
		  snd_pcm_format_name(format));
	if ((err =
	     snd_pcm_hw_params_set_format(alsa_id->alsa_pcm,
					  alsa_id->alsa_hw_params,
					  format)) < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Cannot set sample format (%s)",
			  snd_strerror(err));
		return -1;
	}

	audio_log(OTTS_LOG_INFO, "alsa: Setting sample rate to %i", track.sample_rate);
	sr = track.sample_rate;
	if ((err =
	     snd_pcm_hw_params_set_rate_near(alsa_id->alsa_pcm,
					     alsa_id->alsa_hw_params, &sr,
					     0)) < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Cannot set sample rate (%s)",
			  snd_strerror(err));

		return -1;
	}

	audio_log(OTTS_LOG_INFO, "alsa: Setting channel count to %i", track.num_channels);
	if ((err =
	     snd_pcm_hw_params_set_channels(alsa_id->alsa_pcm,
					    alsa_id->alsa_hw_params,
					    track.num_channels)) < 0) {
		audio_log(OTTS_LOG_INFO, "alsa: cannot set channel count (%s)",
			  snd_strerror(err));
		return -1;
	}

	audio_log(OTTS_LOG_INFO, "alsa: Setting hardware parameters on the ALSA device");
	if ((err =
	     snd_pcm_hw_params(alsa_id->alsa_pcm,
			       alsa_id->alsa_hw_params)) < 0) {
		audio_log(OTTS_LOG_INFO, "alsa: cannot set parameters (%s) state=%s",
			  snd_strerror(err),
			  snd_pcm_state_name(snd_pcm_state(alsa_id->alsa_pcm)));
		return -1;
	}

	/* Get the current swparams */
	if ((err =
	     snd_pcm_sw_params_current(alsa_id->alsa_pcm,
				       alsa_id->alsa_sw_params)) < 0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Unable to determine current swparams for playback: %s\n",
			  snd_strerror(err));
		return -1;
	}
	//    audio_log("Checking buffer size");
	if ((err =
	     snd_pcm_hw_params_get_buffer_size(alsa_id->alsa_hw_params,
					       &(alsa_id->alsa_buffer_size))) <
	    0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Unable to get buffer size for playback: %s\n",
			  snd_strerror(err));
		return -1;
	}
	audio_log(OTTS_LOG_INFO, "alsa: Buffer size on ALSA device is %d bytes",
		  (int)alsa_id->alsa_buffer_size);

	/* This is probably better left for the device driver to decide */
	/* allow the transfer when at least period_size samples can be processed */
	/*    err = snd_pcm_sw_params_set_avail_min(id->alsa_pcm, id->alsa_sw_params, id->alsa_buffer_size/4);
	   if (err < 0) {
	   audio_log(OTTS_LOG_CRIT, "alsa: Unable to set avail min for playback: %s\n", snd_strerror(err));
	   return err;
	   } */

	/* Get period size. */
	snd_pcm_hw_params_get_period_size(alsa_id->alsa_hw_params, &period_size,
					  0);

	/* Calculate size of silence at end of buffer. */
	samples_per_period = period_size * track.num_channels;
	//    audio_log("samples per period = %i", samples_per_period);
	//    audio_log("num_samples = %i", track.num_samples);
	silent_samples =
	    samples_per_period - (track.num_samples % samples_per_period);
	//    audio_log("silent samples = %i", silent_samples);

	audio_log(OTTS_LOG_INFO, "alsa: Preparing device for playback");
	if ((err = snd_pcm_prepare(alsa_id->alsa_pcm)) < 0) {
		audio_log(OTTS_LOG_CRIT,
			  "alsa: Cannot prepare audio interface for playback (%s)",
			  snd_strerror(err));

		return -1;
	}

	/* Calculate space needed to round up to nearest period size. */
	volume_size = bytes_per_sample * (track.num_samples + silent_samples);
	audio_log(OTTS_LOG_INFO, "alsa: volume size = %i", (int)volume_size);

	/* Create a copy of track with adjusted volume. */
	audio_log(OTTS_LOG_INFO, "alsa: Making copy of track and adjusting volume");
	track_volume = track;
	track_volume.samples = (short *)g_malloc(volume_size);
	real_volume = (float)(alsa_id->id.volume - OTTS_VOICE_VOLUME_MIN)
	    / (float)(OTTS_VOICE_VOLUME_MAX - OTTS_VOICE_VOLUME_MIN);
	for (i = 0; i <= track.num_samples - 1; i++)
		track_volume.samples[i] = track.samples[i] * real_volume;

	if (silent_samples > 0) {
		u_int16_t silent16;
		u_int8_t silent8;

		/* Fill remaining space with silence */
		audio_log(OTTS_LOG_INFO,
			  "alsa: Filling with silence up to the period size, silent_samples=%d",
			  (int)silent_samples);
		/* TODO: This hangs.  Why?
		   snd_pcm_format_set_silence(format,
		   track_volume.samples + (track.num_samples * bytes_per_sample), silent_samples);
		 */
		switch (bytes_per_sample) {
		case 2:
			silent16 = snd_pcm_format_silence_16(format);
			for (i = 0; i < silent_samples; i++)
				track_volume.samples[track.num_samples + i] =
				    silent16;
			break;
		case 1:
			silent8 = snd_pcm_format_silence(format);
			for (i = 0; i < silent_samples; i++)
				track_volume.samples[track.num_samples + i] =
				    silent8;
			break;
		}
	}

	/* Loop until all samples are played on the device. */
	output_samples = track_volume.samples;
	num_bytes = (track.num_samples + silent_samples) * bytes_per_sample;
	//    audio_log("alsa: Still %d bytes left to be played", num_bytes);
	while (num_bytes > 0) {

		/* Write as much samples as possible */
		framecount = num_bytes / bytes_per_sample / track.num_channels;
		if (framecount < period_size)
			framecount = period_size;

		/* Report current state state */
		state = snd_pcm_state(alsa_id->alsa_pcm);
		//      audio_log("PCM state before writei: %s",
		//          snd_pcm_state_name(state));

		/* audio_log("snd_pcm_writei() called") */
		ret =
		    snd_pcm_writei(alsa_id->alsa_pcm, output_samples,
				   framecount);
		//        audio_log("Sent %d of %d remaining bytes", ret*bytes_per_sample, num_bytes);

		if (ret == -EAGAIN) {
			audio_log(OTTS_LOG_INFO, "alsa: Warning: Forced wait!");
			snd_pcm_wait(alsa_id->alsa_pcm, 100);
		} else if (ret == -EPIPE) {
			if (xrun(alsa_id) != 0)
				ERROR_EXIT();
		} else if (ret == -ESTRPIPE) {
			if (suspend(alsa_id) != 0)
				ERROR_EXIT();
		} else if (ret == -EBUSY) {
			audio_log(OTTS_LOG_INFO, "alsa: WARNING: sleeping while PCM BUSY");
			usleep(100);
			continue;
		} else if (ret < 0) {
			audio_log(OTTS_LOG_CRIT,
				  "alsa: Write to audio interface failed (%s)",
				  snd_strerror(ret));
			ERROR_EXIT();
		}

		if (ret > 0) {
			/* Update counter of bytes left and move the data pointer */
			num_bytes -=
			    ret * bytes_per_sample * track.num_channels;
			output_samples +=
			    ret * bytes_per_sample * track.num_channels / 2;
		}

		/* Report current state */
		state = snd_pcm_state(alsa_id->alsa_pcm);
		//      audio_log("PCM state before polling: %s",
		//          snd_pcm_state_name(state));

		err =
		    wait_for_poll(alsa_id, alsa_id->alsa_poll_fds,
				  alsa_id->alsa_fd_count, 0);
		if (err < 0) {
			audio_log(OTTS_LOG_CRIT, "alsa: Wait for poll() failed\n");
			ERROR_EXIT();
		} else if (err == 1) {
			audio_log(OTTS_LOG_INFO, "alsa: Playback stopped");

			/* Drop the playback on the sound device (probably
			   still in progress up till now) */
			err = snd_pcm_drop(alsa_id->alsa_pcm);
			if (err < 0) {
				audio_log(OTTS_LOG_CRIT, "alsa: snd_pcm_drop() failed: %s",
					  snd_strerror(err));
				return -1;
			}

			goto terminate;
		}

		if (num_bytes <= 0)
			break;
//      audio_log("ALSA ready for more samples");

		/* Stop requests can be issued again */
	}

	audio_log(OTTS_LOG_INFO, "alsa: Draining...");

	/* We want to next "device ready" notification only after the buffer is
	   already empty */
	err =
	    snd_pcm_sw_params_set_avail_min(alsa_id->alsa_pcm,
					    alsa_id->alsa_sw_params,
					    alsa_id->alsa_buffer_size);
	if (err < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Unable to set avail min for playback: %s\n",
			  snd_strerror(err));
		return err;
	}
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(alsa_id->alsa_pcm, alsa_id->alsa_sw_params);
	if (err < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Unable to set sw params for playback: %s\n",
			  snd_strerror(err));
		return -1;
	}

	err =
	    wait_for_poll(alsa_id, alsa_id->alsa_poll_fds,
			  alsa_id->alsa_fd_count, 1);
	if (err < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: Wait for poll() failed\n");
		return -1;
	} else if (err == 1) {
		audio_log(OTTS_LOG_INFO, "alsa: Playback stopped while draining");

		/* Drop the playback on the sound device (probably
		   still in progress up till now) */
		err = snd_pcm_drop(alsa_id->alsa_pcm);
		if (err < 0) {
			audio_log(OTTS_LOG_CRIT, "alsa: snd_pcm_drop() failed: %s",
				  snd_strerror(err));
			return -1;
		}
	}
	audio_log(OTTS_LOG_INFO, "lsa: Draining terminated");

terminate:
	/* Terminating (successfully or after a stop) */
	if (track_volume.samples != NULL)
		g_free(track_volume.samples);

	err = snd_pcm_drop(alsa_id->alsa_pcm);
	if (err < 0) {
		audio_log(OTTS_LOG_CRIT, "alsa: snd_pcm_drop() failed: %s",
			  snd_strerror(err));
		return -1;
	}

	audio_log(OTTS_LOG_WARN, "alsa: Freeing HW parameters");
	snd_pcm_hw_params_free(alsa_id->alsa_hw_params);

	pthread_mutex_lock(&alsa_id->alsa_pipe_mutex);
	alsa_id->alsa_opened = 0;
	close(alsa_id->alsa_stop_pipe[0]);
	close(alsa_id->alsa_stop_pipe[1]);

	g_free(alsa_id->alsa_poll_fds);
	alsa_id->alsa_poll_fds = NULL;
	pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);

	audio_log(OTTS_LOG_ERR, "alsa: End of playback on ALSA");

	return 0;
}

#undef ERROR_EXIT

/*
 Stop the playback on the device and interrupt alsa_play()
*/
static int alsa_stop(AudioID * id)
{
	char buf;
	int ret;
	alsa_id_t *alsa_id = (alsa_id_t *) id;

	audio_log(OTTS_LOG_ERR, "alsa: STOP!");

	if (alsa_id == NULL)
		return 0;

	pthread_mutex_lock(&alsa_id->alsa_pipe_mutex);
	if (alsa_id->alsa_opened) {
		/* This constant is arbitrary */
		buf = 42;

		ret = write(alsa_id->alsa_stop_pipe[1], &buf, 1);
		if (ret <= 0) {
			audio_log(OTTS_LOG_CRIT,
				  "alsa: Can't write stop request to pipe, err %d: %s",
				  errno, strerror(errno));
		}
	}
	pthread_mutex_unlock(&alsa_id->alsa_pipe_mutex);

	return 0;
}

/* 
  Set volume

  Comments: It's not possible to set individual track volume with Alsa, so we
   handle volume in alsa_play() by multiplication of each sample.
*/
static int alsa_set_volume(AudioID * id, int volume)
{
	return 0;
}

static char const *alsa_get_playcmd(void)
{
	return alsa_play_cmd;
}

/* Provide the Alsa backend. */
static audio_plugin_t alsa_functions = {
	"alsa",
	alsa_open,
	alsa_play,
	alsa_stop,
	alsa_close,
	alsa_set_volume,
	alsa_get_playcmd
};

audio_plugin_t *alsa_plugin_get(void)
{
	return &alsa_functions;
}

audio_plugin_t *AUDIO_PLUGIN_ENTRY(void)
    __attribute__ ((weak, alias("alsa_plugin_get")));
#undef MSG
#undef ERR
