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

/* Definitions of global variables. */
signed int rate;
signed int pitch;
signed int volume;

char *output_module;
char *language;
char *voice_type;
char *punctuation_mode;
char *priority;
int pipe_mode;
SPDDataMode ssml_mode;
int spelling;
int wait_till_end;
int stop_previous;
int cancel_previous;

char *application_name;
char *connection_name;

static struct option long_options[] = {
	{"rate", 1, 0, 'r'},
	{"pitch", 1, 0, 'p'},
	{"volume", 1, 0, 'i'},
	{"output-module", 1, 0, 'o'},
	{"language", 1, 0, 'l'},
	{"voice-type", 1, 0, 't'},
	{"punctuation-mode", 1, 0, 'm'},
	{"spelling", 0, 0, 's'},
	{"ssml", 0, 0, 'x'},
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

static char *short_options = "r:p:i:l:o:t:m:sxeP:N:n:wSCvh";

void options_print_help(char *argv[])
{
	assert(argv);
	assert(argv[0]);

	printf("Usage: %s [options] \"some text\"\n", argv[0]);
	printf
	    ("otts-say -- a simple client for speech synthesis (GNU GPL)\n\n");
	printf("-r, --rate             -     Set the rate of the speech\n"
	       "                               (between -100 and +100, default: 0)\n"
	       "-p, --pitch            -     Set the pitch of the speech\n"
	       "                               (between -100 and +100, default: 0)\n"
	       "-i, --volume           -     Set the volume (intensity) of the speech\n"
	       "                               (between -100 and +100, default: 0) \n"
	       "-o, --output-module    -     Set the output module\n"
	       "-l, --language         -     Set the language (iso code)\n"
	       "-t, --voice-type       -     Set the prefered voice type\n"
	       "                               (male1, male2, male3, female1, female2\n"
	       "                                female3, child_male, child_female)\n"
	       "-m, --punctuation-mode -     Set the punctuation mode (none, some, all) \n"
	       "-s, --spelling         -     Spell the message\n"
	       "-x, --ssml             -     Set SSML mode on (default: off)\n"
	       "\n"
	       "-e, --pipe-mode        -     Pipe from stdin to stdout plus openttsd\n"
	       "-P, --priority         -     Set priority of the message (important, message,\n"
	       "                                text, notification, progress; default: text)\n"
	       "-N, --application-name -     Set the application name used to estabilish\n"
	       "                                the connection to specified string value\n"
	       "                                (default: otts-say)\n"
	       "-n, --connection-name  -     Set the connection name used to estabilish\n"
	       "                                the connection to specified string value\n"
	       "                                (default: main)\n" "\n"
	       "-w, --wait             -     Wait till the message is spoken or discarded\n"
	       "-S, --stop             -     Stop speaking the message being spoken\n"
	       "                                in openttsd\n"
	       "-C, --cancel           -     Cancel all messages in openttsd\n"
	       "\n"
	       "-v, --version          -     Print version and copyright info\n"
	       "-h, --help             -     Print this info\n" "\n"
	       "Copyright (C) 2003 Brailcom, o.p.s.\n"
	       "Copyright (C) 2010 OpenTTS Developers.\n"
	       "This is free software; you can redistribute it and/or modify it\n"
	       "under the terms of the GNU General Public License as published by\n"
	       "the Free Software Foundation; either version 2, or (at your option)\n"
	       "any later version. Please see COPYING for more details.\n\n"
	       "Please report bugs on the Issues page at <http://opentts.org/>.\n\n");

}

void options_print_version()
{
	printf("otts-say: " PACKAGE " " VERSION "\n");
	printf("Copyright (C) 2002-2006 Brailcom, o.p.s.\n"
	       "Copyright (C) 2010 OpenTTS Developers.\n"
	       "otts-say comes with ABSOLUTELY NO WARRANTY.\n"
	       "You may redistribute copies of otts-say\n"
	       "under the terms of the GNU General Public License.\n"
	       "For more information about these matters, see the file named COPYING.\n");
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
		case 'm':
			OPT_SET_STR(punctuation_mode);
			break;
		case 's':
			spelling = 1;
			break;
		case 'e':
			pipe_mode = 1;
			break;
		case 'P':
			OPT_SET_STR(priority);
			break;
		case 'x':
			ssml_mode = SPD_DATA_SSML;
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
			options_print_version(argv);
			exit(0);
			break;
		case 'h':
			options_print_help(argv);
			exit(0);
			break;
		default:
			printf("Unrecognized option\n");
			options_print_help(argv);
			exit(1);
		}
	}
	return 0;
}

#undef SPD_OPTION_SET_INT
