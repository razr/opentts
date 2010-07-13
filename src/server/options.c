/*
 * options.c - Parse and process possible command line options
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include <getopt.h>

#include <i18n.h>
#include <logging.h>
#include "openttsd.h"

#include "options.h"

static struct option long_options[] = {
	{"run-daemon", 0, 0, 'd'},
	{"run-single", 0, 0, 's'},
	{"log-level", 1, 0, 'l'},
	{"communication-method", 1, 0, 'c'},
	{"socket-name", 1, 0, 'S'},
	{"port", 1, 0, 'p'},
	{"pid-file", 1, 0, 'P'},
	{"config-dir", 1, 0, 'C'},
	{"system-service", no_argument, NULL, 'y'},
	{"version", 0, 0, 'v'},
	{"debug", 0, 0, 'D'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
};

static char *short_options = "dsl:c:S:p:P:C:vDhy";

void options_print_help(char *argv[])
{
	assert(argv);
	assert(argv[0]);

	printf(_
	       ("Usage: %s [-{d|s|y}] [-l {0|1|2|3|4|5}] [-c com_method] [-S socket_name] [-p port] | [-v] | [-h]\n"),
	       argv[0]);
	printf(_
	       ("OpenTTS -- Common interface for Speech Synthesis (GNU GPL)\n\n"));
	printf("-d, --run-daemon\t");
	printf(_("Run as a daemon\n"));
	printf("-s, --run-single\t");
	printf(_("Run as single application\n"));
	printf("-y, --system-service\t");
	printf(_("Run as system wide service\n"));
	printf("-l, --log-level\t\t");
	printf(_("Set log level (0..5)\n"));
	printf("-c, --communication-method\t");
	printf(_("Communication method to use (unix_socket or inet_socket)\n"));
	printf("-S, --socket-name\t");
	printf(_
	       ("Socket name to use for 'unix_socket' method (path or 'default')\n"));
	printf("-p, --port\t\t");
	printf(_("Specify a port number for 'inet_socket' method\n"));
	printf("-P, --pid-file\t\t");
	printf(_("Set path to pid file\n"));
	printf("-C, --config-dir\t");
	printf(_("Set path to configuration\n"));
	printf("-v, --version\t\t");
	printf(_("Report version of this program\n"));
	printf("-D, --debug\t\t");
	printf(_("Output debugging information into \${TMPDIR}/openttsd-debug\n"
		 "\t\t\tif \${TMPDIR} does not exists it uses /tmp/openttsd-debug\n"));
	printf("-h, --help\t\t");
	printf(_("Print this information\n\n"));
	printf(_("Copyright (C) 2003,2006 Brailcom, o.p.s.\n"
		 "Copyright (C) 2010 OpenTTS Developers\n"
		 "This is free software; you can redistribute it and/or modify it\n"
		 "under the terms of the GNU General Public License as published by\n"
		 "the Free Software Foundation; either version 2, or (at your option)\n"
		 "any later version. Please see COPYING for more details.\n\n"));
	printf(_
	       ("Please report bugs on the issues page at <http://opentts.org>,\n"));
	printf(_("or on our mailing list at <%s>\n\n"), PACKAGE_BUGREPORT);
}

void options_print_version(void)
{
	printf("%s %s\n", PACKAGE, VERSION);
	printf(_("Copyright (C) 2002, 2003, 2006 Brailcom, o.p.s.\n"
		 "Copyright (C) 2010 OpenTTS Developers\n"
		 "OpenTTS comes with ABSOLUTELY NO WARRANTY.\n"
		 "You may redistribute copies of OpenTTS\n"
		 "under the terms of the GNU General Public License.\n"
		 "For more information about these matters, see the file named COPYING.\n"));
}

#define OPTION_SET_INT(param) \
    val = strtol(optarg, &tail_ptr, 10); \
    if(tail_ptr != optarg){ \
        options.param ## _set = 1; \
        options.param = val; \
    }

#define OPTION_SET_STR(param) \
    options.param = optarg

void options_parse(int argc, char *argv[])
{
	char *tail_ptr;
	int c_opt;
	int option_index;
	int val;
	int ret;
	int mode_set = 0;

	char *tmpdir;
	char *debug_logfile_path;

	assert(argc > 0);
	assert(argv);

	while (1) {
		option_index = 0;

		c_opt =
		    getopt_long(argc, argv, short_options, long_options,
				&option_index);
		if (c_opt == -1)
			break;
		switch (c_opt) {
		case 'd':
			if (mode_set) {
				log_msg(OTTS_LOG_ERR,
					"-%c option error, options {d|s|y} are mutual exclusive", c_opt);
				exit(1);
			}
			options.mode = DAEMON;
			mode_set = 1;
			break;
		case 's':
			if (mode_set) {
				log_msg(OTTS_LOG_ERR,
					"-%c option error, options {d|s|y} are mutual exclusive", c_opt);
				exit(1);
			}
			options.mode = SESSION;
			mode_set = 1;
			break;
		case 'y':
			if (mode_set) {
				log_msg(OTTS_LOG_ERR,
					"-%c option error, options {d|s|y} are mutual exclusive", c_opt);
				exit(1);
			}
			options.mode = SYSTEM;
			mode_set = 1;
			break;
		case 'l':
			OPTION_SET_INT(log_level);
			if (val >= OTTS_LOG_DEBUG || OTTS_LOG_CRIT <= val) {
				log_msg(OTTS_LOG_ERR,
					"-l %d option error, log level shall be in a range %d...%d",
					val, OTTS_LOG_CRIT, OTTS_LOG_DEBUG);
				exit(1);
			}
			break;
		case 'c':
			if (strcmp(optarg,"unix_socket")
			    || strcmp(optarg,"inet_socket")) {
				log_msg(OTTS_LOG_ERR,
					"-c %s option error, communication method shall be unix_socket or inet_socket",
					optarg);
				exit(1);
			}
			OPTION_SET_STR(communication_method);
			options.communication_method_set = 1;
			break;
		case 'S':
			OPTION_SET_STR(socket_name);
			options.socket_name_set = 1;
			break;
		case 'p':
			OPTION_SET_INT(port);
			break;
		case 'P':
			OPTION_SET_STR(pid_file);
			break;
		case 'C':
			OPTION_SET_STR(conf_dir);
			break;
		case 'v':
			options_print_version();
			exit(0);
			break;
		case 'D':
			tmpdir = g_strdup(getenv("TMPDIR"));
			if (!tmpdir)
				tmpdir = g_strdup("/tmp");
			options.debug_destination =
			    g_strdup_printf("%s/openttsd-debug", tmpdir);
			g_free(tmpdir);

			ret = mkdir(options.debug_destination, S_IRWXU);
			if (ret) {
				log_msg(OTTS_LOG_ERR,
					"Can't create additional debug destination in %s, reason %d-%s",
					options.debug_destination, errno,
					strerror(errno));
				if (errno == EEXIST) {
					log_msg(OTTS_LOG_ERR,
						"Debugging directory %s already exists, please delete it first",
						options.debug_destination);
				}
				exit(1);
			}

			debug_logfile_path = g_strdup_printf("%s/openttsd.log",
							     options.
							     debug_destination);
			/* Open logfile for writing */
			open_debug_log(debug_logfile_path, DEBUG_ON);
			g_free(debug_logfile_path);
			options.debug = 1;
			break;
		case 'h':
			options_print_help(argv);
			exit(0);
			break;
		default:
			log_msg(OTTS_LOG_WARN, "Unrecognized option\n");
			options_print_help(argv);
			exit(1);
		}
	}
}

#undef OPTION_SET_INT
