 /*
  * options.c - Parse and process possible command line options
  *
  * Copyright (C) 2003 Brailcom, o.p.s.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <opentts/opentts_types.h>
#include "options.h"
#include <glib/gi18n.h>

/* Definitions of global variables. */
signed int rate = OTTS_VOICE_RATE_DEFAULT;
signed int pitch = OTTS_VOICE_PITCH_DEFAULT;
signed int volume = OTTS_VOICE_VOLUME_DEFAULT;

char *output_module;
char *language;
char *voice_type;
char *punctuation_mode;
char *priority;
int pipe_mode;
SPDDataMode data_mode = SPD_DATA_TEXT;  /* default */
SPDSpelling spelling = SPD_SPELL_OFF; /* default */
int wait_till_end;
int stop_previous;
int cancel_previous;
int list_synthesis_voices = 0;

char *application_name;
char *connection_name;

static struct option long_options[] = {
	{"rate", required_argument, 0, 'r'},
	{"pitch", required_argument, 0, 'p'},
	{"volume", required_argument, 0, 'i'},
	{"output-module", 1, 0, 'o'},
	{"language", 1, 0, 'l'},
	{"voice-type", 1, 0, 't'},
	{"list-synthesis-voices", no_argument, 0, 'L'},
	{"punctuation-mode", 1, 0, 'm'},
	{"spelling", no_argument, 0, 's'},
	{"ssml", no_argument, 0, 'x'},
	{"pipe-mode", 0, 0, 'e'},
	{"priority", 1, 0, 'P'},
	{"application-name", 1, 0, 'N'},
	{"connection-name", 1, 0, 'n'},
	{"wait", 0, 0, 'w'},
	{"stop", 1, 0, 'S'},
	{"cancel", 1, 0, 'C'},
	{"version", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
};

static char *short_options = "r:p:i:l:o:t:Lm:sxeP:N:n:wSCvh";

void options_print_help(char *argv[])
{
	assert(argv);
	assert(argv[0]);

	printf(_("Usage: %s [options] \"some text\"\n"), argv[0]);
	printf(_("otts-say -- a simple client for speech synthesis (GNU GPL)\n\n"));
	printf("-r, --rate\t");
	printf(_("Set the rate of the speech (between %d and %d, default: %d)\n"),
	       OTTS_VOICE_RATE_MIN, OTTS_VOICE_RATE_MAX, OTTS_VOICE_RATE_DEFAULT);
	printf("-p, --pitch\t");
	printf(_("Set the pitch of the speech (between %d and %d, default: %d)\n"),
	       OTTS_VOICE_PITCH_MIN, OTTS_VOICE_PITCH_MAX, OTTS_VOICE_PITCH_DEFAULT);
	printf("-i, --volume\t");
	printf(_("Set the volume (intensity) of the speech (between %d and %d, default: %d) \n"),
	       OTTS_VOICE_VOLUME_MIN, OTTS_VOICE_VOLUME_MAX,
		   OTTS_VOICE_VOLUME_DEFAULT);
	printf("-o, --output-module\t");
	printf(_("Set the output module\n"));
	printf("-l, --language\t");
	printf(_("Set the language (iso code)\n"));
	printf("-t, --voice-type \t");
	printf(_("Set the preferred voice type, (%s)\n"),
		"male1, male2, male3, female1, female2, female3, child_male, child_female");
	printf("-L, --list-synthesis-voices\t");
	printf(_("Get the list of synthesis voices\n"));
	printf("-m, --punctuation-mode\t");
	printf(_("Set the punctuation mode (%s)\n"), "none, some, all");
	printf("-s, --spelling\t");
	printf(_("Spell the message\n"));
	printf("-x, --ssml\t");
	printf(_("Set data mode to SSML (default: TEXT)\n"));
	printf("\n");
	printf("-e, --pipe-mode\t");
	printf(_("Pipe from stdin to stdout plus openttsd\n"));
	printf("-P, --priority\t");
	printf(_("Set priority of the message (%s; default: %s)\n"),
	       "important, message, text, notification, progress", "text");
	printf("-N, --application-name\t");
	printf(_("Set the application name for the connection (default: %s)\n"),
	       "otts-say");
	printf("-n, --connection-name\t");
	printf(_("Set the connection name (default: %s)\n"), "main");
	printf("\n");
	printf("-w, --wait \t");
	printf(_("Wait until the message is spoken or discarded\n"));
	printf("-S, --stop\t");
	printf(_("Stop speaking the message being spoken in openttsd\n"));
	printf("-C, --cancel\t");
	printf(_("Cancel all messages in openttsd\n"));
	printf("\n");
	printf("-v, --version\t");
	printf(_("Print version and copyright information\n"));
	printf("-h, --help\t");
	printf(_("Print this information\n"));
	printf(_("\n"
		 "Copyright (C) 2003 Brailcom, o.p.s.\n"
		 "Copyright (C) 2010 OpenTTS Developers.\n"
		 "This is free software; you can redistribute it and/or modify it\n"
		 "under the terms of the GNU General Public License as published by\n"
		 "the Free Software Foundation; either version 2, or (at your option)\n"
		 "any later version. Please see COPYING for more details.\n\n"));
	printf(_
	("Please report bugs on the Issues page at <http://opentts.org/>.\n\n"));
}

void options_print_version(void)
{
	printf("otts-say: " PACKAGE " " VERSION "\n");
	printf(_("Copyright (C) 2002-2006 Brailcom, o.p.s.\n"
		 "Copyright (C) 2010 OpenTTS Developers.\n"
		 "otts-say comes with ABSOLUTELY NO WARRANTY.\n"
		 "You may redistribute copies of otts-say\n"
		 "under the terms of the GNU General Public License.\n"
		 "For more information see the file named COPYING.\n"));
}

#define OPT_SET_INT(param) \
    val = strtol(optarg, &tail_ptr, 10); \
    if(tail_ptr != optarg){ \
        param = val; \
    }else{ \
        printf("Syntax error or bad parameter!\n"); \
        options_print_help(argv); \
        exit(1); \
    }

#define OPT_SET_STR(param) \
    if(optarg != NULL){ \
        param = (char*) strdup(optarg); \
    }else{ \
        printf("Missing argument!\n"); \
        options_print_help(argv); \
        exit(1); \
    }

int options_parse(int argc, char *argv[])
{
	char *tail_ptr;
	int c_opt;
	int option_index;
	int val;

	assert(argc > 0);
	assert(argv);

	while (1) {
		option_index = 0;

		c_opt = getopt_long(argc, argv, short_options, long_options,
				    &option_index);
		if (c_opt == -1)
			break;
		switch (c_opt) {
		case 'r':
			OPT_SET_INT(rate);
			break;
		case 'p':
			OPT_SET_INT(pitch);
			break;
		case 'i':
			OPT_SET_INT(volume);
			break;
		case 'l':
			OPT_SET_STR(language);
			break;
		case 'o':
			OPT_SET_STR(output_module);
			break;
		case 't':
			OPT_SET_STR(voice_type);
			break;
		case 'L':
			list_synthesis_voices = 1;
			break;
		case 'm':
			OPT_SET_STR(punctuation_mode);
			break;
		case 's':
			spelling = SPD_SPELL_ON;
			break;
		case 'e':
			pipe_mode = 1;
			break;
		case 'P':
			OPT_SET_STR(priority);
			break;
		case 'x':
			data_mode = SPD_DATA_SSML;
			break;
		case 'N':
			OPT_SET_STR(application_name);
			break;
		case 'n':
			OPT_SET_STR(connection_name);
			break;
		case 'w':
			wait_till_end = 1;
			break;
		case 'S':
			stop_previous = 1;
			break;
		case 'C':
			cancel_previous = 1;
			break;
		case 'v':
			options_print_version();
			exit(0);
			break;
		case 'h':
			options_print_help(argv);
			exit(0);
			break;
		default:
			printf(_("Unrecognized option\n"));
			options_print_help(argv);
			exit(1);
		}
	}
	return 0;
}

#undef SPD_OPTION_SET_INT
