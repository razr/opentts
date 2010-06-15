/*
 * openttsd.c - OpenTTS server program
 *  
 * Copyright (C) 2001, 2002, 2003, 2006, 2007 Brailcom, o.p.s.
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

#include <gmodule.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <glib/gstdio.h>

#include <i18n.h>
#include <timestamp.h>
#include "openttsd.h"

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

/* Private constants. */
static const int OTTS_MAX_QUEUE_LEN = 50;

/* Manipulating pid files */
int create_pid_file();
void destroy_pid_file();

/* Server socket file descriptor */
int server_socket;

static void load_configuration(int sig);

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

char *get_path(char *filename, char *startdir)
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
	int std_log = level <= options.log_level;
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
				char *tstr = get_timestamp();
				if (std_log) {
					fprintf(logfile, "%s openttsd: ", tstr);
				}
				if (custom_log) {
					fprintf(custom_logfile,
						"%s openttsd: ", tstr);
				}
				if (options.debug) {
					fprintf(debug_logfile,
						"%s openttsd: ", tstr);
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
			if (options.debug) {
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

/* The main logging function for the OpenTTS daemon,
   level is between 1 and 5. 1 means the most important.*/
/* TODO: Define this in terms of MSG somehow. I don't
   know how to pass '...' arguments to another C function */
void MSG(int level, char *format, ...)
{

	if ((level <= options.log_level)
	    || (options.debug)) {
		va_list args;
		int i;
		pthread_mutex_lock(&logging_mutex);
		{
			{
				/* Print timestamp */
				char *tstr = get_timestamp();
				if (level <= options.log_level)
					fprintf(logfile, "%s openttsd: ", tstr);
				if (options.debug)
					fprintf(debug_logfile,
						"%s openttsd: ", tstr);
				g_free(tstr);
			}

			for (i = 1; i < level; i++) {
				fprintf(logfile, " ");
			}
			if (level <= options.log_level) {
				va_start(args, format);
				vfprintf(logfile, format, args);
				va_end(args);
				fprintf(logfile, "\n");
				fflush(logfile);
			}
			if (options.debug) {
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
int connection_new(int server_socket)
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
	if (client_socket > status.max_fd)
		status.max_fd = client_socket;
	MSG(4, "Adding client on fd %d", client_socket);

	/* Check if there is space for server status data; allocate it */
	if (client_socket >= status.num_fds - 1) {
		openttsd_sockets = (sock_t *) g_realloc(openttsd_sockets,
							   status.num_fds
							   * 2 *
							   sizeof
							   (sock_t));
		status.num_fds *= 2;
	}

	openttsd_sockets[client_socket].o_buf = NULL;
	openttsd_sockets[client_socket].o_bytes = 0;
	openttsd_sockets[client_socket].awaiting_data = 0;
	openttsd_sockets[client_socket].inside_block = 0;

	/* Create a record in fd_settings */
	new_fd_set = (TFDSetElement *) default_fd_set();
	if (new_fd_set == NULL) {
		MSG(2,
		    "Error: Failed to create a record in fd_settings for the new client");
		if (status.max_fd == client_socket)
			status.max_fd--;
		FD_CLR(client_socket, &readfds);
		return -1;
	}
	new_fd_set->fd = client_socket;
	new_fd_set->uid = ++status.max_uid;
	p_client_uid = (int *)g_malloc(sizeof(int));
	*p_client_uid = status.max_uid;

	g_hash_table_insert(fd_settings, p_client_uid, new_fd_set);

	p_client_socket = (int *)g_malloc(sizeof(int));
	*p_client_socket = client_socket;
	p_client_uid = (int *)g_malloc(sizeof(int));
	*p_client_uid = status.max_uid;

	g_hash_table_insert(fd_uid, p_client_socket, p_client_uid);

	MSG(4, "Data structures for client on fd %d created", client_socket);
	return 0;
}

int connection_destroy(int fd)
{
	TFDSetElement *fdset_element;

	/* Client has gone away and we remove it from the descriptor set. */
	MSG(4, "Removing client on fd %d", fd);

	MSG(4, "Tagging client as inactive in settings");
	fdset_element = get_client_settings_by_fd(fd);
	if (fdset_element != NULL) {
		fdset_element->fd = -1;
		fdset_element->active = 0;
	} else if (OPENTTSD_DEBUG) {
		DIE("Can't find settings for this client\n");
	}

	MSG(4, "Removing client from the fd->uid table.");
	g_hash_table_remove(fd_uid, &fd);

	MSG(4, "Closing clients file descriptor %d", fd);

	if (close(fd) != 0)
		if (OPENTTSD_DEBUG)
			DIE("Can't close file descriptor associated to this client");

	FD_CLR(fd, &readfds);
	if (fd == status.max_fd)
		status.max_fd--;

	MSG(3, "Connection closed");

	return 0;
}

gboolean client_terminate(gpointer key, gpointer value, gpointer user)
{
	TFDSetElement *set;

	set = (TFDSetElement *) value;
	if (set == NULL) {
		MSG(2, "Error: Empty connection, internal error");
		if (OPENTTSD_DEBUG)
			FATAL("Internal error");
		return TRUE;
	}

	if (set->fd > 0) {
		MSG(4, "Closing connection on fd %d\n", set->fd);
		connection_destroy(set->fd);
	}
	mem_free_fdset(set);
	return TRUE;
}

/* --- OUTPUT MODULES MANAGING --- */

gboolean modules_terminate(gpointer key, gpointer value, gpointer user)
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

void modules_reload(gpointer key, gpointer value, gpointer user)
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

void module_debug(gpointer key, gpointer value, gpointer user)
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

void module_nodebug(gpointer key, gpointer value, gpointer user)
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

static void reload_dead_modules(int sig)
{
	/* Reload dead modules */
	g_hash_table_foreach(output_modules, modules_reload, NULL);

	/* Make sure there aren't any more child processes left */
	while (waitpid(-1, NULL, WNOHANG) > 0) ;
}

void modules_debug(void)
{
	/* Redirect output to debug for all modules */
	g_hash_table_foreach(output_modules, module_debug, NULL);

}

void modules_nodebug(void)
{
	/* Redirect output to normal for all modules */
	g_hash_table_foreach(output_modules, module_nodebug, NULL);
}

/* --- openttsd START/EXIT FUNCTIONS --- */

static void options_init(void)
{
	options.communication_method = NULL;
	options.communication_method_set = 0;
	options.socket_name = NULL;
	options.socket_name_set = 0;
	options.log_level_set = 0;
	options.port_set = 0;
	options.localhost_access_only_set = 0;
	options.pid_file = NULL;
	options.conf_file = NULL;
	options.opentts_dir = NULL;
	options.log_dir = NULL;
	options.debug = 0;
	options.debug_destination = NULL;
	debug_logfile = NULL;
	mode = DAEMON;
}

static void init()
{
	int START_NUM_FD = 16;
	int ret;
	int i;

	status.max_uid = 0;
	status.max_gid = 0;

	/* Initialize inter-thread comm pipe */
	if (pipe(speaking_pipe)) {
		MSG(1, "Speaking pipe creation failed (%s)", strerror(errno));
		FATAL("Can't create pipe");
	}

	/* Initialize the OpenTTS daemon's priority queue */
	MessageQueue = (queue_t *) queue_alloc();
	if (MessageQueue == NULL)
		FATAL("Couldn't alocate memmory for MessageQueue.");

	/* Initialize lists */
	MessagePausedList = NULL;
	message_history = NULL;
	output_modules_list = NULL;

	/* Initialize hash tables */
	fd_settings = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	assert(fd_settings != NULL);

	fd_uid = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	assert(fd_uid != NULL);

	language_default_modules = g_hash_table_new(g_str_hash, g_str_equal);
	assert(language_default_modules != NULL);

	output_modules = g_hash_table_new(g_str_hash, g_str_equal);
	assert(output_modules != NULL);

	openttsd_sockets =
	    (sock_t *) g_malloc(START_NUM_FD * sizeof(sock_t));
	status.num_fds = START_NUM_FD;
	for (i = 0; i <= START_NUM_FD - 1; i++) {
		openttsd_sockets[i].awaiting_data = 0;
		openttsd_sockets[i].inside_block = 0;
		openttsd_sockets[i].o_buf = 0;
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

	if (options.log_dir == NULL) {
		options.log_dir =
		    g_strdup_printf("%s/log/", options.opentts_dir);
		mkdir(options.log_dir, S_IRWXU);
		if (!options.debug_destination) {
			options.debug_destination =
			    g_strdup_printf("%slog/debug",
					    options.opentts_dir);
			mkdir(options.debug_destination, S_IRWXU);
		}
	}

	/* Load configuration from the config file */
	MSG(4, "Reading openttsd's configuration from %s",
	    options.conf_file);
	load_configuration(0);

	logging_init();

	/* Check for output modules */
	if (g_hash_table_size(output_modules) == 0) {
		DIE("No speech output modules were loaded - aborting...");
	} else {
		MSG(3, "openttsd started with %d output module%s",
		    g_hash_table_size(output_modules),
		    g_hash_table_size(output_modules) > 1 ? "s" : "");
	}

	last_p5_block = NULL;
}

static void load_configuration(int sig)
{
	configfile_t *configfile = NULL;

	/* Clean previous configuration */
	assert(output_modules != NULL);
	g_hash_table_foreach_remove(output_modules, modules_terminate,
				    NULL);

	/* Make sure there aren't any more child processes left */
	while (waitpid(-1, NULL, WNOHANG) > 0) ;

	/* Load new configuration */
	load_default_global_set_options();

	num_options = 0;
	configoptions = load_config_options(&num_options);

	/* Add the LAST option */
	configoptions = add_config_option(configoptions, &num_options, "", 0,
					NULL, NULL, 0);

	configfile =
	    dotconf_create(options.conf_file, configoptions, 0,
			   CASE_INSENSITIVE);
	if (configfile) {
		configfile->includepath = strdup(options.conf_dir);
		MSG(5, "Config file include path is: %s",
		    configfile->includepath);
		if (dotconf_command_loop(configfile) == 0)
			DIE("Error reading config file\n");
		dotconf_cleanup(configfile);
		MSG(2, "Configuration has been read from \"%s\"",
		    options.conf_file);
	} else {
		MSG(1, "Can't open %s", options.conf_file);
	}

	free_config_options(configoptions, &num_options);
}

static void quit(int sig)
{
	int ret;

	MSG(1, "Terminating...");

	MSG(2, "Closing open connections...");
	/* We will browse through all the connections and close them. */
	g_hash_table_foreach_remove(fd_settings, client_terminate,
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
	g_hash_table_foreach_remove(output_modules, modules_terminate,
				    NULL);
	g_hash_table_destroy(output_modules);

	MSG(2, "Closing server connection...");
	if (close(server_socket) == -1)
		MSG(2, "close() failed: %s", strerror(errno));
	FD_CLR(server_socket, &readfds);

	MSG(3, "Removing pid file");
	destroy_pid_file();

	fflush(NULL);

	MSG(2, "openttsd terminated correctly");

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
	pid_file = fopen(options.pid_file, "r");
	if (pid_file != NULL) {
		pid_fd = fileno(pid_file);

		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = 1;
		lock.l_len = 3;

		/* If there is a lock, exit, otherwise remove the old file */
		ret = fcntl(pid_fd, F_GETLK, &lock);
		if (ret == -1) {
			MSG(1,
			    "Can't check lock status of an existing pid file.\n");
			return -1;
		}

		fclose(pid_file);
		if (lock.l_type != F_UNLCK) {
			MSG(1, "openttsd is already running.\n");
			return -1;
		}

		unlink(options.pid_file);
	}

	/* Create a new pid file and lock it */
	pid_file = fopen(options.pid_file, "w");
	if (pid_file == NULL) {
		MSG(1,
		    "Can't create pid file in %s, wrong permissions?\n",
		    options.pid_file);
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
		MSG(1, "Can't set lock on pid file.\n");
		return -1;
	}

	return 0;
}

void destroy_pid_file(void)
{
	unlink(options.pid_file);
}

void logging_init(void)
{
	char *file_name =
	    g_strdup_printf("%s/openttsd.log", options.log_dir);
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
			if (chmod(file_name, S_IRUSR|S_IWUSR) == -1) {
				fprintf(stderr, "Unable to change permission for %s (%s)\n",
				file_name, strerror(errno));
			}
			MSG(2, "openttsd is logging to file %s",
			    file_name);
		}
	}

	if (!debug_logfile)
		debug_logfile = stdout;

	g_free(file_name);
	return;
}

/* --- Sockets --- */

int make_local_socket(const char *filename)
{
	struct sockaddr_un name;
	int sock;
	size_t size;

	/* Create the socket. */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		FATAL("Can't create local socket");
	}

	/* Bind a name to the socket. */
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, filename, sizeof(name.sun_path));
	name.sun_path[sizeof(name.sun_path) - 1] = '\0';
	size = SUN_LEN(&name);

	if (bind(sock, (struct sockaddr *)&name, size) < 0) {
		FATAL("Can't bind local socket");
	}

	if (chmod(filename, S_IRUSR | S_IWUSR) == -1) {
		MSG(2, "ERRNO:%s", strerror(errno));
		FATAL("Unable to set permissions on local socket.");
	}
	if (listen(sock, OTTS_MAX_QUEUE_LEN) == -1) {
		MSG(2, "ERRNO:%s", strerror(errno));
		FATAL("listen() failed for local socket");
	}

	return sock;
}

int make_inet_socket(const int port)
{
	struct sockaddr_in server_address;
	int server_socket;

	/* Create an inet socket */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		FATAL("Can't create inet socket");
	}

	/* Set REUSEADDR flag */
	const int flag = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &flag,
		       sizeof(int)))
		MSG(2, "Error: Setting socket option failed!");

	server_address.sin_family = AF_INET;

	/* Enable access only to localhost or for any address
	   based on LocalhostAccessOnly configuration option. */
	if (options.localhost_access_only)
		server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	else
		server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	server_address.sin_port = htons(port);

	MSG(3, "Openning inet socket connection");
	if (bind(server_socket, (struct sockaddr *)&server_address,
		 sizeof(server_address)) == -1) {
		MSG(1, "bind() failed: %s", strerror(errno));
		FATAL("Couldn't open inet socket, try a few minutes later.");
	}

	if (listen(server_socket, OTTS_MAX_QUEUE_LEN) == -1) {
		MSG(2, "ERRNO:%s", strerror(errno));
		FATAL
		    ("listen() failed for inet socket, is another openttsd running?");
	}

	return server_socket;
}

/* --- MAIN --- */

int main(int argc, char *argv[])
{
	fd_set testfds;
	int fd;
	int ret;
	struct sigaction sig;
	sig.sa_flags = SA_RESTART;
	sigemptyset(&sig.sa_mask);

	/* initialize i18n support */
	init_i18n();
	/* Initialize threading and thread safety in Glib */
	g_thread_init(NULL);

	init_timestamps();	/* For correct timestamp generation. */

	/* Initialize logging */
	logfile = stderr;
	options.log_level = 1;
	custom_logfile = NULL;
	custom_log_kind = NULL;

	options_init();

	options_parse(argc, argv);

	MSG(1, "openttsd " VERSION " starting");

	/* By default, search for configuration options and put everything
	 * in a .opentts directory  in user's home directory. */
	{
		const char *user_home_dir;

		/* Get users home dir */
		user_home_dir = g_getenv("HOME");
		if (!user_home_dir)
			user_home_dir = g_get_home_dir();

		if (user_home_dir) {
			/* Setup a ~/.opentts/ directory or create a new one */
			options.opentts_dir =
			    g_strdup_printf("%s/.opentts",
					    user_home_dir);
			MSG(4, "Home dir found, trying to find %s",
			    options.opentts_dir);
			g_mkdir(options.opentts_dir, S_IRWXU);
			MSG(4,
			    "Using home directory: %s for configuration, pidfile and logging",
			    options.opentts_dir);

			/* Pidfile */
			if (options.pid_file == NULL) {
				/* If no pidfile path specified on command line, use default local dir */
				options.pid_file =
				    g_strdup_printf
				    ("%s/pid/openttsd.pid",
				     options.opentts_dir);
				g_mkdir(g_path_get_dirname
					(options.pid_file), S_IRWXU);
			}

			/* Config file */
			if (options.conf_dir == NULL) {
				/* If no conf_dir was specified on command line, try default local config dir */
				options.conf_dir =
				    g_strdup_printf("%s/conf/",
						    options.opentts_dir);
				if (!g_file_test
				    (options.conf_dir,
				     G_FILE_TEST_IS_DIR)) {
					/* If the local confiration dir doesn't exist, read the global configuration */
					if (strcmp(SYS_CONF, ""))
						options.conf_dir =
						    g_strdup(SYS_CONF);
					else
						options.conf_dir =
						    g_strdup
						    ("/etc/opentts/");
				}
			}
		} else {
			if (options.pid_file == NULL
			    || options.conf_dir == NULL)
				FATAL
				    ("Paths to pid file or conf dir not specified and the current user has no HOME directory!");
		}
		options.conf_file =
		    g_strdup_printf("%s/openttsd.conf", options.conf_dir);
	}

	/*
	 * If no PID file exists, create it and proceed.  Otherwise,
	 * exit, since openttsd is already running.
	 */
	if (create_pid_file() != 0)
		exit(1);

	/* Initialize logging mutex to workaround ctime threading bug */
	/* Must be done no later than here */
	ret = pthread_mutex_init(&logging_mutex, NULL);
	if (ret != 0) {
		fprintf(stderr, "Mutex initialization failed");
		exit(1);
	}

	/* Register signals */
	sig.sa_handler = quit;
	sigaction(SIGINT, &sig, NULL);
	sig.sa_handler = load_configuration;
	sigaction(SIGHUP, &sig, NULL);
	sig.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sig, NULL);
	sig.sa_handler = reload_dead_modules;
	sigaction(SIGUSR1, &sig, NULL);

	init();

	if (!strcmp(options.communication_method, "inet_socket")) {
		MSG(2, "openttsd will use inet port %d",
		    options.port);
		/* Connect and start listening on inet socket */
		server_socket = make_inet_socket(options.port);
	} else if (!strcmp(options.communication_method, "unix_socket")) {
		/* Determine appropriate socket file name */
		GString *socket_filename;
		if (!strcmp(options.socket_name, "default")) {
			/* This code cannot be moved above next to conf_dir and pidpath resolution because
			 * we need to also consider the DotConf configuration,
			 * which is read in init() */
			socket_filename = g_string_new("");
			if (options.opentts_dir) {
				g_string_printf(socket_filename,
						"%s/openttsd.sock",
						options.
						opentts_dir);
			} else {
				FATAL
				    ("Socket file name not set and user has no home directory");
			}
		} else {
			socket_filename =
			    g_string_new(options.socket_name);
		}
		MSG(2, "openttsd will use local unix socket: %s",
		    socket_filename->str);

		/* Delete an old socket file if it exists */
		if (g_file_test(socket_filename->str, G_FILE_TEST_EXISTS))
			if (g_unlink(socket_filename->str) == -1)
				FATAL
				    ("Local socket file exists but impossible to delete. Wrong permissions?");

		/* Connect and start listening on local unix socket */
		server_socket = make_local_socket(socket_filename->str);
		g_string_free(socket_filename, 1);
	} else {
		FATAL("Unknown communication method");
	}

	/* Fork, set uid, chdir, etc. */
	if (mode == DAEMON) {
		daemon(0, 0);
		/* Re-create the pid file under this process */
		unlink(options.pid_file);
		if (create_pid_file() == -1)
			return -1;
	}

	MSG(4, "Creating new thread for speak()");
	ret = pthread_create(&speak_thread, NULL, speak, NULL);
	if (ret != 0)
		FATAL("Speak thread failed!\n");

	FD_ZERO(&readfds);
	FD_SET(server_socket, &readfds);
	status.max_fd = server_socket;

	/* Now wait for clients and requests. */
	MSG(1, "openttsd started, and it is waiting for clients ...");
	while (1) {
		testfds = readfds;

		if (select
		    (FD_SETSIZE, &testfds, (fd_set *) 0, (fd_set *) 0,
		     NULL) >= 1) {
			/* Once we know we've got activity,
			 * we find which descriptor it's on by checking each in turn using FD_ISSET. */

			for (fd = 0;
			     fd <= status.max_fd && fd < FD_SETSIZE;
			     fd++) {
				if (FD_ISSET(fd, &testfds)) {
					MSG(4, "Activity on fd %d ...", fd);

					if (fd == server_socket) {
						/* server activity (new client) */
						ret =
						    connection_new
						    (server_socket);
						if (ret != 0) {
							MSG(2,
							    "Error: Failed to add new client!");
							if (OPENTTSD_DEBUG)
								FATAL
								    ("Failed to add new client");
						}
					} else {
						/* client activity */
						int nread;
						ioctl(fd, FIONREAD, &nread);

						if (nread == 0) {
							/* client has gone */
							connection_destroy
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
