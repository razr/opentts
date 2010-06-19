
/*
 * openttsd.h - openttsd header
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
 */

#ifndef OPENTTSD_H
#define OPENTTSD_H

#define _GNU_SOURCE

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <assert.h>

#include <pthread.h>

#include <glib.h>

#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/* Definition of semun needed for semaphore manipulation */
/* TODO: This fixes compilation for Mac OS X but might not be a correct
 * solution for other platforms. A better check is needed, possibly including
 * _POSIX_C_SOURCE and friends*/
#if (defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)) || defined(__APPLE__)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;		/* value for SETVAL */
	struct semid_ds *buf;	/* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;	/* array for GETALL, SETALL */
	struct seminfo *__buf;	/* buffer for IPC_INFO */
};
#endif

#include <def.h>
#include <fdset.h>
#include "module.h"
#include "parse.h"
#include "compare.h"

/* Size of the buffer for socket communication */
#define BUF_SIZE 128

/* Mode of openttsd execution */
typedef enum {
	DAEMON,			/* Run as daemon (background, ...) */
	SESSION			/*  */
} openttsd_mode;

openttsd_mode mode;

/*  message_queue is a queue for messages. */
typedef struct {
	GList *p1;		/* important */
	GList *p2;		/* text */
	GList *p3;		/* message */
	GList *p4;		/* notification */
	GList *p5;		/* progress */
} queue_t;

/*  openttsd_message is an element in a message queue,
    that is, some text with or without index marks
    inside  and it's configuration. */
typedef struct {
	guint id;		/* unique id */
	time_t time;		/* when was this message received */
	char *buf;		/* the actual text */
	int bytes;		/* number of bytes in buf */
	TFDSetElement settings;	/* settings of the client when queueing this message */
} openttsd_message;

#include "alloc.h"
#include "speaking.h"

struct {
	char *communication_method;
	int communication_method_set;
	char *socket_name;
	int socket_name_set;
	int port, port_set;
	int localhost_access_only, localhost_access_only_set;
	int log_level, log_level_set;
	char *pid_file;
	char *conf_file;
	char *conf_dir;
	char *opentts_dir;
	char *log_dir;
	int debug;
	char *debug_destination;
	char *debug_logfile;
	char *custom_log_kind;
	char *custom_log_filename;
	int max_history_messages;	/* Maximum of messages in history before they expire */
} options;

struct {
	int max_uid;		/* The largest assigned uid + 1 */
	int max_gid;		/* The largest assigned gid + 1 */
	int max_fd;
	int num_fds;		/* Number of available allocated sockets */
} status;

/* We create two additional threads: signal-handler and speaking. */
extern pthread_t speak_thread;
extern pthread_t sighandler_thread;
extern gboolean speak_thread_started;

pthread_mutex_t logging_mutex;
pthread_mutex_t element_free_mutex;
pthread_mutex_t output_layer_mutex;
pthread_mutex_t socket_com_mutex;
extern pthread_mutex_t thread_controller;

/* Activity requests for the speaking thread are
 handled with SYSV/IPC semaphore */
key_t speaking_sem_key;
int speaking_sem_id;

/* Table of all configured (and succesfully loaded) output modules */
GHashTable *output_modules;
GList *output_modules_list;
/* Table of settings for each active client (=each active socket)*/
GHashTable *fd_settings;
/* Table of default output modules for different languages */
GHashTable *language_default_modules;
/* Table of relations between client file descriptors and their uids */
GHashTable *fd_uid;

/* Main priority queue for messages */
queue_t *MessageQueue;
/* List of messages from paused clients waiting for resume */
GList *MessagePausedList;
/* List of settings related to history */
GList *history_settings;
/* List of messages in history */
GList *message_history;

/* List of different entries of client-specific configuration */
GList *client_specific_settings;

/* Saves the last received priority progress message */
GList *last_p5_block;

/* Global default settings */
TFDSetElement GlobalFDSet;

/* Variables for socket communication */
fd_set readfds;

/* Inter thread comm pipe */
extern int speaking_pipe[2];

/* Arrays needed for receiving data over socket */
typedef struct {
	int awaiting_data;
	int inside_block;
	size_t o_bytes;
	GString *o_buf;
} sock_t;

sock_t *openttsd_sockets;
extern int server_socket;

/* Debugging */
void MSG2(int level, char *kind, char *format, ...);

#define OPENTTSD_DEBUG 0

#define FATAL(msg) { fatal_error(); log_msg(OTTS_LOG_CRIT,"Fatal error [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }
#define DIE(msg) { log_msg(OTTS_LOG_CRIT,"Error [%s:%d]:"msg, __FILE__, __LINE__); exit(EXIT_FAILURE); }

/* For debugging purposes, does nothing */
void fatal_error(void);

/* isanum() tests if the given string is a number,
 * returns 1 if yes, 0 otherwise. */
int isanum(const char *str);

/* Construct a path given a filename and the directory
 where to refer relative paths. filename can be either
 absolute (starting with slash) or relative. */
char *get_path(char *filename, char *startdir);

/* Functions for starting threads. */
gboolean start_speak_thread(void);

/* Tell the main thread to stop. */
void stop_main_thread(void);

/* Functions used in openttsd.c only */
int connection_new(int server_socket);
int connection_destroy(int fd);
gboolean client_terminate(gpointer key, gpointer value, gpointer user);
gboolean modules_terminate(gpointer key, gpointer value, gpointer user);
void modules_reload(gpointer key, gpointer value, gpointer user);
void modules_debug(void);
void modules_nodebug(void);

int create_pid_file(void);
void destroy_pid_file(void);

void configure(void);

void check_locked(pthread_mutex_t * lock);

#endif
