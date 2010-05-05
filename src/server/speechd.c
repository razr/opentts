/*
 * speechd.c - Speech Dispatcher server program
 *  
 * Copyright (C) 2001, 2002, 2003, 2006, 2007 Brailcom, o.p.s.
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
 * $Id: speechd.c,v 1.81 2008-07-10 15:36:49 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gmodule.h>
#include <sys/stat.h>

#include "speechd.h"

/* Declare dotconf functions and data structures*/
#include "configuration.h"

/* Declare functions to allocate and create important data
 * structures */
#include "alloc.h"
#include "sem_functions.h"
#include "speaking.h"
#include "set.h"
#include "options.h"
#include "server.h"

/* Glib g_makedir */
int g_mkdir(const gchar * filename, int mode);
/* Manipulating pid files */
int create_pid_file();
void destroy_pid_file();

/* Server socket file descriptor */
int server_socket;

void speechd_load_configuration(int sig);

#ifdef __SUNPRO_C
/* Added by Willie Walker - daemon is a gcc-ism
 */
#include <sys/filio.h>
static int daemon(int nochdir, int noclose)
{
	int fd, i;

	switch (fork()) {
	case 0:
		break;
	case -1:
		return -1;
	default:
		_exit(0);
	}

	if (!nochdir) {
		chdir("/");
	}

	if (setsid() < 0) {
		return -1;
	}

	if (!noclose) {
		if (fd = open("/dev/null", O_RDWR) >= 0) {
			for (i = 0; i < 3; i++) {
				dup2(fd, i);
			}
			if (fd > 2) {
				close(fd);
			}
		}
	}
	return 0;
}
#endif /* __SUNPRO_C */

char *spd_get_path(char *filename, char *startdir)
{
	char *ret;
	if (filename == NULL)
		return NULL;
	if (filename[0] != '/') {
		if (startdir == NULL)
			ret = g_strdup(filename);
		else
			ret = g_strdup_printf("%s/%s", startdir, filename);
	} else {
		ret = g_strdup(filename);
	}
	return ret;
}

/* --- DEBUGGING --- */

/* Just to be able to set breakpoints */
void fatal_error(void)
{
	int i = 0;
	i++;
}

/* Logging messages, level of verbosity is defined between 1 and 5, 
 * see documentation */
void MSG2(int level, char *kind, char *format, ...)
{
	int std_log = level <= SpeechdOptions.log_level;
	int custom_log = (kind != NULL && custom_log_kind != NULL &&
			  !strcmp(kind, custom_log_kind) &&
			  custom_logfile != NULL);

	if (std_log || custom_log) {
		va_list args;
		va_list args2;
		int i;

		pthread_mutex_lock(&logging_mutex);

		if (std_log)
			va_start(args, format);
		if (custom_log)
			va_start(args2, format);

		{
			{
				/* Print timestamp */
				time_t t;
				char *tstr;
				struct timeval tv;
				t = time(NULL);
				tstr = g_strdup(ctime(&t));
				gettimeofday(&tv, NULL);
				assert(tstr);
				/* Remove the trailing \n */
				assert(strlen(tstr) > 1);
				tstr[strlen(tstr) - 1] = 0;
				if (std_log) {
					fprintf(logfile, "[%s : %d] speechd: ",
						tstr, (int)tv.tv_usec);
					//            fprintf(logfile, "[test : %d] speechd: ",
					//                     (int) tv.tv_usec); 
				}
				if (custom_log) {
					fprintf(custom_logfile,
						"[%s : %d] speechd: ", tstr,
						(int)tv.tv_usec);
				}
				if (SpeechdOptions.debug) {
					fprintf(debug_logfile,
						"[%s : %d] speechd: ", tstr,
						(int)tv.tv_usec);
				}
				g_free(tstr);
			}
			for (i = 1; i < level; i++) {
				if (std_log) {
					fprintf(logfile, " ");
				}
				if (custom_log) {
					fprintf(custom_logfile, " ");
				}
			}
			if (std_log) {
				vfprintf(logfile, format, args);
				fprintf(logfile, "\n");
				fflush(logfile);
			}
			if (custom_log) {
				vfprintf(custom_logfile, format, args2);
				fprintf(custom_logfile, "\n");
				fflush(custom_logfile);
			}
			if (SpeechdOptions.debug) {
				va_start(args, format);
				vfprintf(debug_logfile, format, args);
				va_end(args);
				fprintf(debug_logfile, "\n");
				fflush(debug_logfile);
			}
		}
		if (std_log) {
			va_end(args);
		}
		if (custom_log) {
			va_end(args2);
		}
		pthread_mutex_unlock(&logging_mutex);
	}
}

/* The main logging function for Speech Dispatcher,
   level is between 1 and 5. 1 means the most important.*/
/* TODO: Define this in terms of MSG somehow. I don't
   know how to pass '...' arguments to another C function */
void MSG(int level, char *format, ...)
{

	if ((level <= SpeechdOptions.log_level)
	    || (SpeechdOptions.debug)) {
		va_list args;
		int i;
		pthread_mutex_lock(&logging_mutex);
		{
			{
				/* Print timestamp */
				time_t t;
				char *tstr;
				struct timeval tv;
				t = time(NULL);
				tstr = g_strdup(ctime(&t));
				gettimeofday(&tv, NULL);
				/* Remove the trailing \n */
				assert(tstr);
				assert(strlen(tstr) > 1);
				tstr[strlen(tstr) - 1] = 0;
				if (level <= SpeechdOptions.log_level)
					fprintf(logfile, "[%s : %d] speechd: ",
						tstr, (int)tv.tv_usec);
				if (SpeechdOptions.debug)
					fprintf(debug_logfile,
						"[%s : %d] speechd: ", tstr,
						(int)tv.tv_usec);
				/*                fprintf(logfile, "[%s : %d] speechd: ",
				   tstr, (int) tv.tv_usec); */
				g_free(tstr);
			}

			for (i = 1; i < level; i++) {
				fprintf(logfile, " ");
			}
			if (level <= SpeechdOptions.log_level) {
				va_start(args, format);
				vfprintf(logfile, format, args);
				va_end(args);
				fprintf(logfile, "\n");
				fflush(logfile);
			}
			if (SpeechdOptions.debug) {
				va_start(args, format);
				vfprintf(debug_logfile, format, args);
				va_end(args);
				fprintf(debug_logfile, "\n");
				fflush(debug_logfile);
			}
		}
		pthread_mutex_unlock(&logging_mutex);
	}
}

/* --- CLIENTS / CONNECTIONS MANAGING --- */

/* activity is on server_socket (request for a new connection) */
int speechd_connection_new(int server_socket)
{
	TFDSetElement *new_fd_set;
	struct sockaddr_in client_address;
	unsigned int client_len = sizeof(client_address);
	int client_socket;
	int *p_client_socket, *p_client_uid;

	client_socket =
	    accept(server_socket, (struct sockaddr *)&client_address,
		   &client_len);

	if (client_socket == -1) {
		MSG(2,
		    "Error: Can't handle connection request of a new client");
		return -1;
	}

	/* We add the associated client_socket to the descriptor set. */
	FD_SET(client_socket, &readfds);
	if (client_socket > SpeechdStatus.max_fd)
		SpeechdStatus.max_fd = client_socket;
	MSG(4, "Adding client on fd %d", client_socket);

	/* Check if there is space for server status data; allocate it */
	if (client_socket >= SpeechdStatus.num_fds - 1) {
		SpeechdSocket = (TSpeechdSock *) g_realloc(SpeechdSocket,
							   SpeechdStatus.
							   num_fds * 2 *
							   sizeof
							   (TSpeechdSock));
		SpeechdStatus.num_fds *= 2;
	}

	SpeechdSocket[client_socket].o_buf = g_string_new("");
	SpeechdSocket[client_socket].o_bytes = 0;
	SpeechdSocket[client_socket].awaiting_data = 0;
	SpeechdSocket[client_socket].inside_block = 0;

	/* Create a record in fd_settings */
	new_fd_set = (TFDSetElement *) default_fd_set();
	if (new_fd_set == NULL) {
		MSG(2,
		    "Error: Failed to create a record in fd_settings for the new client");
		if (SpeechdStatus.max_fd == client_socket)
			SpeechdStatus.max_fd--;
		FD_CLR(client_socket, &readfds);
		return -1;
	}
	new_fd_set->fd = client_socket;
	new_fd_set->uid = ++SpeechdStatus.max_uid;
	p_client_socket = (int *)g_malloc(sizeof(int));
	p_client_uid = (int *)g_malloc(sizeof(int));
	*p_client_socket = client_socket;
	*p_client_uid = SpeechdStatus.max_uid;

	g_hash_table_insert(fd_settings, p_client_uid, new_fd_set);

	g_hash_table_insert(fd_uid, p_client_socket, p_client_uid);

	MSG(4, "Data structures for client on fd %d created", client_socket);
	return 0;
}

int speechd_connection_destroy(int fd)
{
	TFDSetElement *fdset_element;

	/* Client has gone away and we remove it from the descriptor set. */
	MSG(4, "Removing client on fd %d", fd);

	MSG(4, "Tagging client as inactive in settings");
	fdset_element = get_client_settings_by_fd(fd);
	if (fdset_element != NULL) {
		fdset_element->fd = -1;
		fdset_element->active = 0;
	} else if (SPEECHD_DEBUG) {
		DIE("Can't find settings for this client\n");
	}

	MSG(4, "Removing client from the fd->uid table.");
	g_hash_table_remove(fd_uid, &fd);

	SpeechdSocket[fd].awaiting_data = 0;
	SpeechdSocket[fd].inside_block = 0;
	if (SpeechdSocket[fd].o_buf != NULL)
		g_string_free(SpeechdSocket[fd].o_buf, 1);

	MSG(4, "Closing clients file descriptor %d", fd);

	if (close(fd) != 0)
		if (SPEECHD_DEBUG)
			DIE("Can't close file descriptor associated to this client");

	FD_CLR(fd, &readfds);
	if (fd == SpeechdStatus.max_fd)
		SpeechdStatus.max_fd--;

	MSG(3, "Connection closed");

	return 0;
}

gboolean speechd_client_terminate(gpointer key, gpointer value, gpointer user)
{
	TFDSetElement *set;

	set = (TFDSetElement *) value;
	if (set == NULL) {
		MSG(2, "Error: Empty connection, internal error");
		if (SPEECHD_DEBUG)
			FATAL("Internal error");
		return TRUE;
	}

	if (set->fd > 0) {
		MSG(4, "Closing connection on fd %d\n", set->fd);
		speechd_connection_destroy(set->fd);
	}
	mem_free_fdset(set);
	return TRUE;
}

/* --- OUTPUT MODULES MANAGING --- */

gboolean speechd_modules_terminate(gpointer key, gpointer value, gpointer user)
{
	OutputModule *module;

	module = (OutputModule *) value;
	if (module == NULL) {
		MSG(2, "Error: Empty module, internal error");
		return TRUE;
	}
	unload_output_module(module);

	return TRUE;
}

void speechd_modules_reload(gpointer key, gpointer value, gpointer user)
{
	OutputModule *module;

	module = (OutputModule *) value;
	if (module == NULL) {
		MSG(2, "Empty module, internal error");
		return;
	}

	reload_output_module(module);

	return;
}

void speechd_module_debug(gpointer key, gpointer value, gpointer user)
{
	OutputModule *module;

	module = (OutputModule *) value;
	if (module == NULL) {
		MSG(2, "Empty module, internal error");
		return;
	}

	output_module_debug(module);

	return;
}

void speechd_module_nodebug(gpointer key, gpointer value, gpointer user)
{
	OutputModule *module;

	module = (OutputModule *) value;
	if (module == NULL) {
		MSG(2, "Empty module, internal error");
		return;
	}

	output_module_nodebug(module);

	return;
}

void speechd_reload_dead_modules(int sig)
{
	/* Reload dead modules */
	g_hash_table_foreach(output_modules, speechd_modules_reload, NULL);

	/* Make sure there aren't any more child processes left */
	while (waitpid(-1, NULL, WNOHANG) > 0) ;
}

void speechd_modules_debug(void)
{
	/* Redirect output to debug for all modules */
	g_hash_table_foreach(output_modules, speechd_module_debug, NULL);

}

void speechd_modules_nodebug(void)
{
	/* Redirect output to normal for all modules */
	g_hash_table_foreach(output_modules, speechd_module_nodebug, NULL);
}

/* --- SPEECHD START/EXIT FUNCTIONS --- */

void speechd_options_init(void)
{
	SpeechdOptions.log_level_set = 0;
	SpeechdOptions.port_set = 0;
	SpeechdOptions.localhost_access_only_set = 0;
	SpeechdOptions.pid_file = NULL;
	SpeechdOptions.conf_file = NULL;
	SpeechdOptions.home_speechd_dir = NULL;
	SpeechdOptions.log_dir = NULL;
	SpeechdOptions.debug = 0;
	SpeechdOptions.debug_destination = NULL;
	debug_logfile = NULL;
	spd_mode = SPD_MODE_DAEMON;
}

void speechd_init()
{
	int START_NUM_FD = 16;
	int ret;
	int i;

	SpeechdStatus.max_uid = 0;
	SpeechdStatus.max_gid = 0;

	/* Initialize inter-thread comm pipe */
	if (pipe(speaking_pipe)) {
		MSG(1, "Speaking pipe creation failed (%s)", strerror(errno));
		FATAL("Can't create pipe");
	}

	/* Initialize Speech Dispatcher priority queue */
	MessageQueue = (TSpeechDQueue *) speechd_queue_alloc();
	if (MessageQueue == NULL)
		FATAL("Couldn't alocate memmory for MessageQueue.");

	/* Initialize lists */
	MessagePausedList = NULL;
	message_history = NULL;
	output_modules_list = NULL;

	/* Initialize hash tables */
	fd_settings = g_hash_table_new(g_int_hash, g_int_equal);
	assert(fd_settings != NULL);

	fd_uid = g_hash_table_new(g_str_hash, g_str_equal);
	assert(fd_uid != NULL);

	language_default_modules = g_hash_table_new(g_str_hash, g_str_equal);
	assert(language_default_modules != NULL);

	output_modules = g_hash_table_new(g_str_hash, g_str_equal);
	assert(output_modules != NULL);

	SpeechdSocket =
	    (TSpeechdSock *) g_malloc(START_NUM_FD * sizeof(TSpeechdSock));
	SpeechdStatus.num_fds = START_NUM_FD;
	for (i = 0; i <= START_NUM_FD - 1; i++) {
		SpeechdSocket[i].awaiting_data = 0;
		SpeechdSocket[i].inside_block = 0;
		SpeechdSocket[i].o_buf = 0;
	}

	pause_requested = 0;
	resume_requested = 0;

	/* Perform some functionality tests */
	if (g_module_supported() == FALSE)
		DIE("Loadable modules not supported by current platform.\n");

	if (_POSIX_VERSION < 199506L)
		DIE("This system doesn't support POSIX.1c threads\n");

	/* Fill GlobalFDSet with default values */
	GlobalFDSet.min_delay_progress = 2000;

	/* Initialize list of different client specific settings entries */
	client_specific_settings = NULL;

	/* Initialize mutexes, semaphores and synchronization */
	ret = pthread_mutex_init(&element_free_mutex, NULL);
	if (ret != 0)
		DIE("Mutex initialization failed");

	ret = pthread_mutex_init(&output_layer_mutex, NULL);
	if (ret != 0)
		DIE("Mutex initialization failed");

	ret = pthread_mutex_init(&socket_com_mutex, NULL);
	if (ret != 0)
		DIE("Mutex initialization failed");

	if (SpeechdOptions.log_dir == NULL) {
		if (SpeechdOptions.home_speechd_dir) {
			SpeechdOptions.log_dir = g_strdup_printf("%s/log/",
								 SpeechdOptions.
								 home_speechd_dir);
			mkdir(SpeechdOptions.log_dir, S_IRWXU);
			if (!SpeechdOptions.debug_destination) {
				SpeechdOptions.debug_destination =
				    g_strdup_printf("%slog/debug",
						    SpeechdOptions.
						    home_speechd_dir);
				mkdir(SpeechdOptions.debug_destination,
				      S_IRWXU);
			}
		} else {
			SpeechdOptions.log_dir =
			    g_strdup("/var/log/speech-dispatcher/");
			if (!SpeechdOptions.debug_destination) {
				SpeechdOptions.debug_destination =
				    g_strdup
				    ("/var/log/speech-dispatcher/debug");
				mkdir(SpeechdOptions.debug_destination,
				      S_IRWXU);
			}
		}
	}

	/* Load configuration from the config file */
	MSG(4, "Reading Speech Dispatcher configuration from %s",
	    SpeechdOptions.conf_file);
	speechd_load_configuration(0);

	logging_init();

	/* Check for output modules */
	if (g_hash_table_size(output_modules) == 0) {
		DIE("No speech output modules were loaded - aborting...");
	} else {
		MSG(3, "Speech Dispatcher started with %d output module%s",
		    g_hash_table_size(output_modules),
		    g_hash_table_size(output_modules) > 1 ? "s" : "");
	}

	last_p5_block = NULL;
	highest_priority = 0;
}

void speechd_load_configuration(int sig)
{
	configfile_t *configfile = NULL;

	/* Clean previous configuration */
	assert(output_modules != NULL);
	g_hash_table_foreach_remove(output_modules, speechd_modules_terminate,
				    NULL);

	/* Make sure there aren't any more child processes left */
	while (waitpid(-1, NULL, WNOHANG) > 0) ;

	/* Load new configuration */
	load_default_global_set_options();

	spd_num_options = 0;
	spd_options = load_config_options(&spd_num_options);

	/* Add the LAST option */
	spd_options = add_config_option(spd_options, &spd_num_options, "", 0,
					NULL, NULL, 0);

	configfile =
	    dotconf_create(SpeechdOptions.conf_file, spd_options, 0,
			   CASE_INSENSITIVE);
	if (!configfile) {
		MSG(1, "Can't open %s", SpeechdOptions.conf_file);
		DIE("Error opening config file\n");
	}
	configfile->includepath = strdup(SpeechdOptions.conf_dir);
	MSG(5, "Config file include path is: %s", configfile->includepath);
	if (dotconf_command_loop(configfile) == 0)
		DIE("Error reading config file\n");
	dotconf_cleanup(configfile);

	free_config_options(spd_options, &spd_num_options);
	MSG(2, "Configuration has been read from \"%s\"",
	    SpeechdOptions.conf_file);
}

void speechd_quit(int sig)
{
	int ret;

	MSG(1, "Terminating...");

	MSG(2, "Closing open connections...");
	/* We will browse through all the connections and close them. */
	g_hash_table_foreach_remove(fd_settings, speechd_client_terminate,
				    NULL);
	g_hash_table_destroy(fd_settings);

	MSG(4, "Closing speak() thread...");
	ret = pthread_cancel(speak_thread);
	if (ret != 0)
		FATAL("Speak thread failed to cancel!\n");

	ret = pthread_join(speak_thread, NULL);
	if (ret != 0)
		FATAL("Speak thread failed to join!\n");

	MSG(2, "Closing open output modules...");
	/*  Call the close() function of each registered output module. */
	g_hash_table_foreach_remove(output_modules, speechd_modules_terminate,
				    NULL);
	g_hash_table_destroy(output_modules);

	MSG(2, "Closing server connection...");
	if (close(server_socket) == -1)
		MSG(2, "close() failed: %s", strerror(errno));
	FD_CLR(server_socket, &readfds);

	MSG(3, "Removing pid file");
	destroy_pid_file();

	fflush(NULL);

	MSG(2, "Speech Dispatcher terminated correctly");

	exit(0);
}

/* --- PID FILES --- */

int create_pid_file()
{
	FILE *pid_file;
	int pid_fd;
	struct flock lock;
	int ret;

	/* If the file exists, examine it's lock */
	pid_file = fopen(SpeechdOptions.pid_file, "r");
	if (pid_file != NULL) {
		pid_fd = fileno(pid_file);

		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = 1;
		lock.l_len = 3;

		/* If there is a lock, exit, otherwise remove the old file */
		ret = fcntl(pid_fd, F_GETLK, &lock);
		if (ret == -1) {
			fprintf(stderr,
				"Can't check lock status of an existing pid file.\n");
			return -1;
		}

		fclose(pid_file);
		if (lock.l_type != F_UNLCK) {
			fprintf(stderr, "Speech Dispatcher already running.\n");
			return -1;
		}

		unlink(SpeechdOptions.pid_file);
	}

	/* Create a new pid file and lock it */
	pid_file = fopen(SpeechdOptions.pid_file, "w");
	if (pid_file == NULL) {
		fprintf(stderr,
			"Can't create pid file in %s, wrong permissions?\n",
			SpeechdOptions.pid_file);
		return -1;
	}
	fprintf(pid_file, "%d\n", getpid());
	fflush(pid_file);

	pid_fd = fileno(pid_file);
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 1;
	lock.l_len = 3;

	ret = fcntl(pid_fd, F_SETLK, &lock);
	if (ret == -1) {
		fprintf(stderr, "Can't set lock on pid file.\n");
		return -1;
	}

	return 0;
}

void destroy_pid_file(void)
{
	unlink(SpeechdOptions.pid_file);
}

void logging_init(void)
{
	char *file_name =
	    g_strdup_printf("%s/speechd.log", SpeechdOptions.log_dir);
	assert(file_name != NULL);
	if (!strncmp(file_name, "stdout", 6)) {
		logfile = stdout;
	} else if (!strncmp(file_name, "stderr", 6)) {
		logfile = stderr;
	} else {
		logfile = fopen(file_name, "a");
		if (logfile == NULL) {
			fprintf(stderr,
				"Error: can't open logging file %s! Using stdout.\n",
				file_name);
			logfile = stdout;
		} else {
			MSG(2, "Speech Dispatcher Logging to file %s",
			    file_name);
		}
	}

	if (!debug_logfile)
		debug_logfile = stdout;

	g_free(file_name);
	return;
}

/* --- MAIN --- */

int main(int argc, char *argv[])
{
	struct sockaddr_in server_address;
	fd_set testfds;
	int fd;
	int ret;
	struct sigaction sig;
	sig.sa_flags = SA_RESTART;
	sigemptyset(&sig.sa_mask);

	/* Initialize threading and thread safety in Glib */
	g_thread_init(NULL);

	/* Strip all permisions for 'others' from the files created */
	umask(007);

	/* Initialize logging */
	logfile = stderr;
	SpeechdOptions.log_level = 1;
	custom_logfile = NULL;
	custom_log_kind = NULL;

	speechd_options_init();

	options_parse(argc, argv);

#ifdef ENABLE_SESSION
	/* Get the server port from the SPEECHD_PORT environment variable. */
	char *tail_ptr;
	int val;
	if (getenv("SPEECHD_PORT") && !SpeechdOptions.port_set) {
		SpeechdOptions.port_set = 1;
		val = strtol(getenv("SPEECHD_PORT"), &tail_ptr, 10);
		if (tail_ptr != getenv("SPEECHD_PORT")) {
			SpeechdOptions.port = val;
		}
	}
#endif

	MSG(1, "Speech Dispatcher " VERSION " starting");

	/* Check if there is .speech-dispatcher directory
	   in user's home directory. If yes, put everything
	   here */
	{
		char *home_dir;
		GDir *testing;
		home_dir = (char *)g_get_home_dir();
		if (home_dir) {
			SpeechdOptions.home_speechd_dir =
			    g_strdup_printf("%s/.speech-dispatcher/", home_dir);
			MSG(4, "Home dir found, trying to find %s",
			    SpeechdOptions.home_speechd_dir);
			testing =
			    g_dir_open(SpeechdOptions.home_speechd_dir, 0,
				       NULL);
			if (testing) {
				/* Ok, directory exists */
				g_dir_close(testing);
				MSG(4,
				    "Using home directory: %s for configuration, pidfile and logging",
				    SpeechdOptions.home_speechd_dir);
			} else {
				MSG(1,
				    "Directory .speech-dispatcher not found in home");
				g_free(SpeechdOptions.home_speechd_dir);
				SpeechdOptions.home_speechd_dir = NULL;
			}
			g_free(home_dir);
		}
	}

	/* Initialize logging mutex to workaround ctime threading bug */
	/* Must be done no later than here */
	ret = pthread_mutex_init(&logging_mutex, NULL);
	if (ret != 0) {
		fprintf(stderr, "Mutex initialization failed");
		exit(1);
	}

	if (SpeechdOptions.pid_file == NULL) {
		if (SpeechdOptions.home_speechd_dir) {
			char *temp = NULL;
			SpeechdOptions.pid_file =
			    g_strdup_printf("%s/pid/speech-dispatcher.pid",
					    SpeechdOptions.home_speechd_dir);
			temp = g_path_get_dirname(SpeechdOptions.pid_file);
			mkdir(temp, S_IRWXU);
		} else if (!strcmp(PIDPATH, ""))
			SpeechdOptions.pid_file =
			    g_strdup("/var/run/speech-dispatcher.pid");
		else
			SpeechdOptions.pid_file =
			    g_strdup(PIDPATH "speech-dispatcher.pid");
	}

	if (SpeechdOptions.conf_dir == NULL) {
		if (SpeechdOptions.home_speechd_dir) {
			SpeechdOptions.conf_dir = g_strdup_printf("%sconf/",
								  SpeechdOptions.
								  home_speechd_dir);
		} else if (!strcmp(SYS_CONF, "")) {
			SpeechdOptions.conf_dir =
			    g_strdup("/etc/speech-dispatcher/");
		} else {
			SpeechdOptions.conf_dir = g_strdup(SYS_CONF);
		}
	}
	SpeechdOptions.conf_file = g_strdup_printf("%s/speechd.conf",
						   SpeechdOptions.conf_dir);
	/* This is needed for DotConf so that it knows where to include files from */

	if (create_pid_file() != 0)
		exit(1);

	/* Register signals */
	sig.sa_handler = speechd_quit;
	sigaction(SIGINT, &sig, NULL);
	sig.sa_handler = speechd_load_configuration;
	sigaction(SIGHUP, &sig, NULL);
	sig.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sig, NULL);
	sig.sa_handler = speechd_reload_dead_modules;
	sigaction(SIGUSR1, &sig, NULL);

	speechd_init();

	MSG(1, "Speech Dispatcher will use port %d", SpeechdOptions.port);

	/* Initialize socket functionality */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	{
		const int flag = 1;
		if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &flag,
			       sizeof(int)))
			MSG(2, "Error: Setting socket option failed!");
	}

	server_address.sin_family = AF_INET;

	/* Enable access only to localhost or for any address
	   based on LocalhostAccessOnly configuration option. */
	if (SpeechdOptions.localhost_access_only)
		server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	else
		server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	server_address.sin_port = htons(SpeechdOptions.port);

	MSG(3, "Openning socket connection");
	if (bind(server_socket, (struct sockaddr *)&server_address,
		 sizeof(server_address)) == -1) {
		MSG(1, "bind() failed: %s", strerror(errno));
		FATAL("Couldn't open socket, try a few minutes later.");
	}

	/* Create a connection queue and initialize readfds to handle input
	   from server_socket. */
	if (listen(server_socket, 50) == -1)
		FATAL("listen() failed, another Speech Dispatcher running?");

	/* Fork, set uid, chdir, etc. */
	if (spd_mode == SPD_MODE_DAEMON) {
		daemon(0, 0);
		/* Re-create the pid file under this process */
		unlink(SpeechdOptions.pid_file);
		if (create_pid_file() == -1)
			return -1;
	}

	MSG(4, "Creating new thread for speak()");
	ret = pthread_create(&speak_thread, NULL, speak, NULL);
	if (ret != 0)
		FATAL("Speak thread failed!\n");

	FD_ZERO(&readfds);
	FD_SET(server_socket, &readfds);
	SpeechdStatus.max_fd = server_socket;

	/* Now wait for clients and requests. */
	MSG(1,
	    "Speech Dispatcher " VERSION
	    " started on port %d and waiting for clients ...",
	    SpeechdOptions.port);
	while (1) {
		testfds = readfds;

		if (select
		    (FD_SETSIZE, &testfds, (fd_set *) 0, (fd_set *) 0,
		     NULL) >= 1) {
			/* Once we know we've got activity,
			 * we find which descriptor it's on by checking each in turn using FD_ISSET. */

			for (fd = 0;
			     fd <= SpeechdStatus.max_fd && fd < FD_SETSIZE;
			     fd++) {
				if (FD_ISSET(fd, &testfds)) {
					MSG(4, "Activity on fd %d ...", fd);

					if (fd == server_socket) {
						/* server activity (new client) */
						ret =
						    speechd_connection_new
						    (server_socket);
						if (ret != 0) {
							MSG(2,
							    "Error: Failed to add new client!");
							if (SPEECHD_DEBUG)
								FATAL
								    ("Failed to add new client");
						}
					} else {
						/* client activity */
						int nread;
						ioctl(fd, FIONREAD, &nread);

						if (nread == 0) {
							/* client has gone */
							speechd_connection_destroy
							    (fd);
							if (ret != 0)
								MSG(2,
								    "Error: Failed to close the client!");
						} else {
							/* client sends some commands or data */
							if (serve(fd) == -1)
								MSG(2,
								    "Error: Failed to serve client on fd %d!",
								    fd);
						}
					}
				}
			}
		}
	}
}

void check_locked(pthread_mutex_t * lock)
{
	if (pthread_mutex_trylock(lock) == 0) {
		MSG(1,
		    "CRITICAL ERROR: Not locked but accessing structure data!");
		fprintf(stderr, "WARNING! WARNING! MUTEX CHECK FAILED!\n");
		fflush(stderr);
		exit(0);
	}
}
