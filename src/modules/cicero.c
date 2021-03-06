/*
 * cicero.c - OpenTTS backend for Cicero French TTS engine
 *
 * Copyright (C) 2006 Brailcom, o.p.s.
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
 * @author: Olivier BERT
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <langinfo.h>
#include <sys/stat.h>

#include<logging.h>
#include "module_utils.h"
#include "opentts/opentts_types.h"
#include "opentts/opentts_synth_plugin.h"

#define MODULE_NAME     "cicero"
#define MODULE_VERSION  "0.3"

// #define DEBUG_MODULE 1
DECLARE_DEBUG()

/* Thread and process control */
static int cicero_speaking = 0;

static pthread_t cicero_speaking_thread;
static sem_t *cicero_semaphore;

static char **cicero_message;
static SPDMessageType cicero_message_type;

static int cicero_position = 0;
static int cicero_pause_requested = 0;
static unsigned int CiceroMaxChunkLength = 500;

/* Internal functions prototypes */
static void cicero_set_rate(signed int rate);
static void cicero_set_pitch(signed int pitch);
static void cicero_set_voice(SPDVoiceType voice);

static void *_cicero_speak(void *);

int cicero_stopped = 0;

/*
** Config file options
*/
//MOD_OPTION_1_STR(CiceroWrapper);
MOD_OPTION_1_STR(CiceroExecutable)
MOD_OPTION_1_STR(CiceroExecutableLog)

/*
** Pipes to cicero
*/
static int fd1[2], fd2[2];

/*
** Some internal functions
*/
static long int
millisecondsBetween(const struct timeval *from, const struct timeval *to)
{
	return ((to->tv_sec - from->tv_sec) * 1000) +
	    ((to->tv_usec - from->tv_usec)
	     / 1000);
}

long int millisecondsSince(const struct timeval *from)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return millisecondsBetween(from, &now);
}

static int hasTimedOut(int milliseconds)
{
	static struct timeval start = { 0, 0 };

	if (milliseconds)
		return millisecondsSince(&start) >= milliseconds;

	gettimeofday(&start, NULL);
	return 1;
}

static void mywrite(int fd, const void *buf, int len)
{
	char *pos = (char *)buf;
	int w;
	if (fd < 0)
		return;
	hasTimedOut(0);
	do {
		if ((w = write(fd, pos, len)) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			else if (errno == EPIPE) {
				log_msg(OTTS_LOG_CRIT, "Broken pipe\n");
			} else
				perror("Pipe write");
			return;
		}
		pos += w;
		len -= w;
	} while (len && !hasTimedOut(600));
	if (len)
		fprintf(stderr, "Pipe write timed out");
}

/* Plugin functions */

static int cicero_load(void)
{
	init_settings_tables();
	REGISTER_DEBUG();
	MOD_OPTION_1_STR_REG(CiceroExecutable, "/usr/bin/cicero");
	MOD_OPTION_1_STR_REG(CiceroExecutableLog,
			     "/var/log/opentts/cicero-executable.log");
	return 0;
}

static int cicero_init(char **status_info)
{
	int ret;
	int stderr_redirect;
	struct sigaction ignore;
	memset(&ignore, '\0', sizeof(ignore));
	ignore.sa_flags = SA_RESTART;
	sigemptyset(&ignore.sa_mask);
	ignore.sa_handler = SIG_IGN;

	log_msg(OTTS_LOG_NOTICE, "Module init\n");

	sigaction(SIGPIPE, &ignore, NULL);

	log_msg(OTTS_LOG_DEBUG, "call the pipe system call\n");
	if (pipe(fd1) < 0 || pipe(fd2) < 0) {
		log_msg(OTTS_LOG_ERR, "Error pipe()\n");
		return -1;
	}
	log_msg(OTTS_LOG_INFO, "Call fork system call\n");

	stderr_redirect = open(CiceroExecutableLog,
			       O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (stderr_redirect == -1) {
		log_msg(OTTS_LOG_WARN,
			"ERROR: Openning debug file for Cicero binary failed: (error=%d) %s",
			stderr_redirect, strerror(errno));
	} else {
		log_msg(OTTS_LOG_INFO, "Cicero synthesizer logging to file %s",
			CiceroExecutableLog);
	}
	switch (fork()) {
	case -1:
		{
			log_msg(OTTS_LOG_CRIT, "Error fork()\n");
			return -1;
		}
	case 0:
		{
			if (dup2(fd2[0], 0) < 0	/* stdin */
			    || dup2(fd1[1], 1) < 0) {	/* stdout */
				log_msg(OTTS_LOG_CRIT, "Error dup2()\n");
				exit(1);
			}
			if (stderr_redirect >= 0) {
				if (dup2(stderr_redirect, 2) < 0)
					log_msg(OTTS_LOG_ERR,
						"ERROR: Couldn't redirect stderr, not logging for Cicero synthesizer.");
			}
			/*close(2);
			   close(fd2[1]);
			   close(fd1[0]); */
			int i = 0;
			for (i = 3; i < 256; i++)
				close(i);
			sigaction(SIGPIPE, &ignore, NULL);
			execl(CiceroExecutable, CiceroExecutable, (void *)NULL);
			log_msg(OTTS_LOG_CRIT, "Error execl()\n");
			exit(1);
		}
	default:
		{
			close(fd1[1]);
			close(fd2[0]);
			if (fcntl(fd2[1], F_SETFL, O_NDELAY) < 0
			    || fcntl(fd1[0], F_SETFL, O_NDELAY) < 0) {
				log_msg(OTTS_LOG_ERR, "Error fcntl()\n");
				return -1;
			}
		}
	}

	cicero_message = g_malloc(sizeof(char *));
	*cicero_message = NULL;

	cicero_semaphore = module_semaphore_init();

	log_msg(OTTS_LOG_NOTICE,
		"Cicero: creating new thread for cicero_tracking\n");
	cicero_speaking = 0;
	ret =
	    pthread_create(&cicero_speaking_thread, NULL, _cicero_speak, NULL);
	if (ret != 0) {
		log_msg(OTTS_LOG_ERR, "Cicero: thread failed\n");
		*status_info =
		    g_strdup("The module couldn't initialize threads "
			     "This can be either an internal problem or an "
			     "architecture problem. If you are sure your architecture "
			     "supports threads, please report a bug.");
		return -1;
	}

	*status_info = g_strdup("Cicero initialized succesfully.");

	return 0;
}

static int cicero_audio_init(char **status_info)
{
	/* The following statement has no effect and so was commented out */
	//status_info == NULL;
	return 0;
}

static SPDVoice **cicero_list_voices(void)
{
	return NULL;
}

static int cicero_speak(gchar * data, size_t bytes, SPDMessageType msgtype)
{
	log_msg(OTTS_LOG_NOTICE, "Module speak\n");

	/* The following should not happen */
	if (cicero_speaking) {
		log_msg(OTTS_LOG_ERR, "Speaking when requested to write");
		return 0;
	}

	log_msg(OTTS_LOG_DEBUG, "Requested data: |%s|\n", data);

	if (*cicero_message != NULL) {
		g_free(*cicero_message);
		*cicero_message = NULL;
	}
	*cicero_message = module_strip_ssml(data);
	cicero_message_type = SPD_MSGTYPE_TEXT;

	/* Setting voice */
	/*    UPDATE_PARAMETER(voice, cicero_set_voice); */
	UPDATE_PARAMETER(rate, cicero_set_rate);
	/*    UPDATE_PARAMETER(pitch, cicero_set_pitch); */

	/* Send semaphore signal to the speaking thread */
	cicero_speaking = 1;
	sem_post(cicero_semaphore);

	log_msg(OTTS_LOG_INFO, "Cicero: leaving module_speak() normaly\n\r");
	return bytes;
}

static int cicero_stop(void)
{
	unsigned char c = 1;

	log_msg(OTTS_LOG_NOTICE, "cicero: stop()\n");
	cicero_stopped = 1;
	mywrite(fd2[1], &c, 1);
	return 0;
}

static size_t cicero_pause(void)
{
	log_msg(OTTS_LOG_NOTICE, "pause requested\n");
	if (cicero_speaking) {
		log_msg(OTTS_LOG_WARN, "Pause not supported by cicero\n");
		cicero_pause_requested = 1;
		cicero_stop();
		return -1;
	}
	cicero_pause_requested = 0;
	return 0;
}

static void cicero_close(int status)
{
	log_msg(OTTS_LOG_NOTICE, "cicero: close()\n");
	if (cicero_speaking) {
		cicero_stop();
	}

	if (module_terminate_thread(cicero_speaking_thread) != 0)
		exit(1);

	/*    xfree(cicero_voice); */
	exit(status);
}

/* Internal functions */

void *_cicero_speak(void *nothing)
{
	char stop_code = 1;
	unsigned int pos = 0, inx = 0, len = 0;
	int flag = 0;
	int bytes;
	int ret;
	char buf[CiceroMaxChunkLength], l[5], b[2];
	struct pollfd ufds = { fd1[0], POLLIN | POLLPRI, 0 };

	log_msg(OTTS_LOG_INFO, "cicero: speaking thread starting.......\n");
	set_speaking_thread_parameters();
	while (1) {
		sem_wait(cicero_semaphore);
		log_msg(OTTS_LOG_DEBUG, "Semaphore on\n");
		len = strlen(*cicero_message);
		cicero_stopped = 0;
		cicero_speaking = 1;
		cicero_position = 0;
		pos = 0;
		module_report_event_begin();
		while (1) {
			flag = 0;
			if (cicero_stopped) {
				log_msg(OTTS_LOG_DEBUG,
					"Stop in thread, terminating");
				cicero_speaking = 0;
				module_report_event_stop();
				break;
			}
			if (pos >= len) {	/* end of text */
				log_msg(OTTS_LOG_DEBUG,
					"End of text in speaking thread\n");
				module_report_event_end();
				cicero_speaking = 0;
				break;
			}
			log_msg(OTTS_LOG_DEBUG,
				"Call get_parts: pos=%d, msg=\"%s\" \n", pos,
				*cicero_message);
			bytes =
			    module_get_message_part(*cicero_message, buf, &pos,
						    CiceroMaxChunkLength,
						    ".;?!");
			log_msg(OTTS_LOG_DEBUG,
				"Returned %d bytes from get_part\n", bytes);
			if (bytes < 0) {
				log_msg(OTTS_LOG_WARN,
					"ERROR: Can't get message part, terminating");
				cicero_speaking = 0;
				module_report_event_stop();
				break;
			}
			buf[bytes] = 0;
			log_msg(OTTS_LOG_INFO, "Text to synthesize is '%s'\n",
				buf);

			if (bytes > 0) {
				log_msg(OTTS_LOG_NOTICE, "Speaking ...");
				log_msg(OTTS_LOG_DEBUG,
					"Trying to synthesize text");
				l[0] = 2;	/* say code */
				l[1] = bytes >> 8;
				l[2] = bytes & 0xFF;
				l[3] = 0, l[4] = 0;
				mywrite(fd2[1], &stop_code, 1);
				mywrite(fd2[1], l, 5);
				mywrite(fd2[1], buf, bytes);
				cicero_position = 0;
				while (1) {
					ret = poll(&ufds, 1, 60);
					if (ret)
						log_msg(OTTS_LOG_DEBUG,
							"poll() system call returned %d, events=%d\n",
							ret, ufds.events);
					if (ret < 0) {
						perror("poll");
						module_report_event_stop();
						flag = 1;
						cicero_speaking = 0;
						break;
					}
					if (ret > 0)
						read(fd1[0], b, 2);
					if (cicero_stopped) {
						cicero_speaking = 0;
						module_report_event_stop();
						flag = 1;
						break;
					}
					if (ret == 0)
						continue;
					inx = (b[0] << 8 | b[1]);
					log_msg(OTTS_LOG_INFO,
						"Tracking: index=%u, bytes=%d\n",
						inx, bytes);
					if (inx == bytes) {
						cicero_speaking = 0;
						break;
					} else {
						if (inx)
							cicero_position = inx;
					}
				}
			} else {
				cicero_speaking = 0;
				break;
			}
			if (flag)
				break;
		}
		cicero_stopped = 0;
	}
	cicero_speaking = 0;
	log_msg(OTTS_LOG_DEBUG, "cicero: tracking thread ended.......\n");
	pthread_exit(NULL);
}

static void cicero_set_rate(signed int rate)
{
	const float spkRateTable[] = {
		0.3333,
		0.3720,
		0.4152,
		0.4635,
		0.5173,
		0.5774,
		0.6444,
		0.7192,
		0.8027,
		0.8960,
		1.0000,
		1.1161,
		1.2457,
		1.3904,
		1.5518,
		1.7320,
		1.9332,
		2.1577,
		2.4082,
		2.6879,
		3.0000
	};
	float expand;
	int pos, size = sizeof(spkRateTable) / sizeof(float);

	/*
	 * Map values in the range -100 to 100 to the range 20 to 0.
	 * -100 maps to 20, 0 maps to 10, and 100 maps to 0.
	 * Then use table lookup to find the appropriate value.
	 */
	pos = (OTTS_VOICE_RATE_MAX - rate) * (size - 1)
	    / (OTTS_VOICE_RATE_MAX - OTTS_VOICE_RATE_MIN);
	expand = spkRateTable[pos];

	unsigned char *p = (unsigned char *)&expand;
	unsigned char l[5];
	l[0] = 3;
	l[1] = p[3];
	l[2] = p[2];
	l[3] = p[1];
	l[4] = p[0];
	mywrite(fd2[1], l, 5);
}

static void cicero_set_pitch(signed int pitch)
{
}

static void cicero_set_voice(SPDVoiceType voice)
{
}

static otts_synth_plugin_t cicero_plugin = {
	MODULE_NAME,
	MODULE_VERSION,
	cicero_load,
	cicero_init,
	cicero_audio_init,
	cicero_speak,
	cicero_stop,
	cicero_list_voices,
	cicero_pause,
	cicero_close
};

otts_synth_plugin_t *cicero_plugin_get (void)
{
	return &cicero_plugin;
}

otts_synth_plugin_t *SYNTH_PLUGIN_ENTRY(void)
	__attribute__ ((weak, alias("cicero_plugin_get")));
