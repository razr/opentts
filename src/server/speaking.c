
/*
 * speaking.c - speech output functions
 * 
 * Copyright (C) 2001,2002,2003, 2006, 2007 Brailcom, o.p.s
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
 * $Id: speaking.c,v 1.56 2008-10-15 18:06:48 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>

#include <pthread.h>
#include <glib.h>

#include <logging.h>
#include "openttsd.h"
#include "server.h"
#include "index_marking.h"
#include "module.h"
#include "set.h"
#include "alloc.h"
#include "compare.h"
#include "msg.h"
#include "output.h"
#include "sem_functions.h"
#include "speaking.h"

static openttsd_message *current_message = NULL;
static SPDPriority current_priority = SPD_TEXT;
static gboolean thread_must_stop = FALSE;
static pthread_mutex_t speak_stop_mutex = PTHREAD_MUTEX_INITIALIZER;

static int SPEAKING = 0;
int poll_count;

/* Helper functions. */
static void speaking_module_cleanup(void);

/*
  Speak() is responsible for getting right text from right
  queue in right time and saying it loud through the corresponding
  synthetiser.  This runs in a separate thread.
*/
void *speak(void *data)
{
	openttsd_message *message = NULL;
	int ret;
	struct pollfd *poll_fds;	/* Descriptors to poll */
	struct pollfd main_pfd;
	struct pollfd helper_pfd;
	int revents;

	/* Block all signals and set thread states */
	set_speak_thread_attributes();

	poll_fds = g_malloc(2 * sizeof(struct pollfd));

	main_pfd.fd = speaking_pipe[0];
	main_pfd.events = POLLIN;
	main_pfd.revents = 0;

	helper_pfd.fd = -1;
	helper_pfd.events = POLLIN;
	helper_pfd.revents = 0;

	poll_fds[0] = main_pfd;
	poll_fds[1] = helper_pfd;
	poll_count = 1;

	while (1) {
		ret = poll(poll_fds, poll_count, -1);
		log_msg(OTTS_LOG_DEBUG,
			"Poll in speak() returned socket activity, main_pfd revents=%d, poll_pfd revents=%d",
			poll_fds[0].revents, poll_fds[1].revents);
		if ((revents = poll_fds[0].revents)) {
			if (revents & POLLIN) {
				char buf[100];
				log_msg(OTTS_LOG_DEBUG,
					"wait_for_poll: activity in openttsd");
				read(poll_fds[0].fd, buf, 1);
			}
		}
		if (poll_count > 1) {
			if ((revents = poll_fds[1].revents)) {
				if (revents & POLLHUP) {
					/* The speaking module terminated */
					/* abnormally.  Clean up. */
					log_msg(OTTS_LOG_ERR,
						"The current output module failed.");
					speaking_module_cleanup();
				} else if ((revents & POLLIN)
					   || (revents & POLLPRI)) {
					log_msg(OTTS_LOG_DEBUG,
						"wait_for_poll: activity on output_module");
					/* Check if sb is speaking or they are all silent. 
					 * If some synthesizer is speaking, we must wait. */
					is_sb_speaking();
				}
			}
		}

		/* Handle pause requests */
		if (pause_requested) {
			log_msg(OTTS_LOG_INFO, "Trying to pause...");
			if (pause_requested == 1)
				speaking_pause_all(pause_requested_fd);
			if (pause_requested == 2)
				speaking_pause(pause_requested_fd,
					       pause_requested_uid);
			log_msg(OTTS_LOG_INFO, "Paused...");
			pause_requested = 0;
			continue;
		}

		if (SPEAKING) {
			log_msg(OTTS_LOG_DEBUG,
				"Continuing because already speaking in speak()");
			continue;
		}

		/* Handle resume requests */
		if (resume_requested) {
			GList *gl;

			log_msg(OTTS_LOG_DEBUG, "Resume requested");

			/* Is there any message after resume? */
			if (g_list_length(MessagePausedList) != 0) {
				while (1) {
					pthread_mutex_lock(&element_free_mutex);
					gl = g_list_find_custom
					    (MessagePausedList, (void *)NULL,
					     message_nto_speak);
					log_msg(OTTS_LOG_DEBUG,
						"Message insterted back to the queues!");
					MessagePausedList =
					    g_list_remove_link
					    (MessagePausedList, gl);
					pthread_mutex_unlock
					    (&element_free_mutex);
					if ((gl != NULL) && (gl->data != NULL)) {
						log_msg(OTTS_LOG_DEBUG,
							"Reloading message");
						reload_message((openttsd_message
								*) gl->data);
					} else
						break;
				}
			}
			log_msg(OTTS_LOG_DEBUG, "End of resume processing");
			resume_requested = 0;
		}

		pthread_mutex_lock(&speak_stop_mutex);
		if (thread_must_stop == TRUE) {
			thread_must_stop = FALSE;
			pthread_mutex_unlock(&speak_stop_mutex);
			break;
		} else
			pthread_mutex_unlock(&speak_stop_mutex);

		log_msg(OTTS_LOG_DEBUG,
			"Locking element_free_mutex in speak()");
		pthread_mutex_lock(&element_free_mutex);
		/* Handle postponed priority progress message */

		check_locked(&element_free_mutex);

		if ((g_list_length(last_p5_block) != 0)
		    && (g_list_length(MessageQueue->p5) == 0)) {
			/* Transfer messages from last_p5_block to priority SPD_MESSAGE queue */
			while (g_list_length(last_p5_block) != 0) {
				GList *item;
				item = g_list_first(last_p5_block);
				message = item->data;
				check_locked(&element_free_mutex);
				MessageQueue->p2 =
				    g_list_insert_sorted(MessageQueue->p2,
							 message, sortbyuid);
				last_p5_block =
				    g_list_remove_link(last_p5_block, item);
			}
			assert(message != NULL);
			current_priority = SPD_MESSAGE;
			stop_priority_older_than(SPD_TEXT, message->id);
			stop_priority(SPD_NOTIFICATION);
			stop_priority(SPD_PROGRESS);
			check_locked(&element_free_mutex);
			pthread_mutex_unlock(&element_free_mutex);
			speaking_semaphore_post();
			continue;
		} else {
			/* Extract the right message from priority queue */
			message = get_message_from_queues();
			if (message == NULL) {
				pthread_mutex_unlock(&element_free_mutex);
				log_msg(OTTS_LOG_DEBUG,
					"No message in the queue");
				continue;
			}
		}

		/* Isn't the parent client of this message paused? 
		 * If it is, insert the message to the MessagePausedList. */
		if (message_nto_speak(message, NULL)) {
			log_msg(OTTS_LOG_INFO,
				"Inserting message to paused list...");
			MessagePausedList =
			    g_list_append(MessagePausedList, message);
			pthread_mutex_unlock(&element_free_mutex);
			continue;
		}

		/* Insert index marks into textual messages */
		if (message->settings.type == SPD_MSGTYPE_TEXT) {
			insert_index_marks(message,
					   message->settings.ssml_mode);
		}

		/* Write the message to the output layer. */
		ret = output_speak(message);
		log_msg(OTTS_LOG_INFO, "Message sent to output module");
		if (ret == -1) {
			log_msg(OTTS_LOG_WARN, "Error: Output module failed");
			output_check_module(get_output_module(message));
			pthread_mutex_unlock(&element_free_mutex);
			continue;
		}
		if (ret != 0) {
			log_msg(OTTS_LOG_WARN,
				"ERROR: Can't say message. Module reported error in speaking: %d",
				ret);
			pthread_mutex_unlock(&element_free_mutex);
			continue;
		}
		SPEAKING = 1;

		if (speaking_module != NULL) {
			poll_count = 2;
			helper_pfd.fd = speaking_module->pipe_out[0];
			poll_fds[1] = helper_pfd;
		}

		/* Set the id of the client who is speaking. */
		speaking_uid = message->settings.uid;
		if (current_message != NULL)
			if (!current_message->settings.paused_while_speaking)
				mem_free_message(current_message);
		current_message = message;

		/* Check if the last priority 5 message wasn't said yet */
		if (last_p5_block != NULL) {
			GList *elem;
			openttsd_message *p5_message;
			elem = g_list_last(last_p5_block);
			if (elem != NULL) {
				p5_message = (openttsd_message *) elem->data;
				if (p5_message->settings.reparted ==
				    message->settings.reparted) {
					g_list_foreach(last_p5_block,
						       (GFunc) mem_free_message,
						       NULL);
					g_list_free(last_p5_block);
					last_p5_block = NULL;
				}
			}
		}

		pthread_mutex_unlock(&element_free_mutex);
	}

	g_free(poll_fds);
	poll_count = 0;
	speaking_module = NULL;
	SPEAKING = 0;

	return NULL;
}

/*
	 * This function is called from the signal-handling thread.
	 * It stops the speaking thread and joins it.
	 */

void stop_speak_thread(void)
{
	int ret;

	pthread_mutex_lock(&speak_stop_mutex);
	thread_must_stop = TRUE;
	speaking_semaphore_post();
	pthread_mutex_unlock(&speak_stop_mutex);
	ret = pthread_join(speak_thread, NULL);

	if (ret != 0)
		FATAL("Unable to join speaking thread.");
}

int reload_message(openttsd_message * msg)
{
	TFDSetElement *client_settings;
	int im;
	char *pos;
	char *newtext;
	char *tptr;

	if (msg == NULL) {
		log_msg(OTTS_LOG_INFO,
			"Warning: msg == NULL in reload_message()");
		return -1;
	}

	if (msg->settings.index_mark != NULL) {
		log_msg(OTTS_LOG_DEBUG, "Recovering index mark %s",
			msg->settings.index_mark);
		client_settings = get_client_settings_by_uid(msg->settings.uid);
		/* Scroll back to provide context, if required */
		/* WARNING: This relies on ordered SD_MARK_BODY index marks! */
		log_msg(OTTS_LOG_DEBUG, "Recovering index mark (number)");
		im = strtol(msg->settings.index_mark + SD_MARK_BODY_LEN, &tptr,
			    10);
		log_msg(OTTS_LOG_DEBUG,
			"Recovering index mark (comparing tptr)");
		if (msg->settings.index_mark + SD_MARK_BODY_LEN == tptr) {
			log_msg2(2, "index_marking",
				 "ERROR: Invalid index_mark '%s'. Message not reloaded.",
				 msg->settings.index_mark);
			return -1;
		}
		log_msg(OTTS_LOG_DEBUG, "Recovered index mark number: %d", im);

		im += client_settings->pause_context;

		log_msg2(5, "index_marking",
			 "Requested index mark (with context) is %d (%d+%d)",
			 im, msg->settings.index_mark,
			 client_settings->pause_context);
		if (im < 0) {
			im = 0;
			pos = msg->buf;
		} else {
			pos = find_index_mark(msg, im);
			if (pos == NULL)
				return -1;
		}

		newtext = strip_index_marks(pos, client_settings->ssml_mode);
		g_free(msg->buf);

		if (newtext == NULL)
			return -1;
		msg->buf = newtext;
		msg->bytes = strlen(msg->buf);

		if (queue_message
		    (msg, -msg->settings.uid, 0, SPD_MSGTYPE_TEXT, 0)
		    == 0) {
			if (OPENTTSD_DEBUG)
				FATAL("Can't queue message\n");
			g_free(msg->buf);
			g_free(msg);
			return -1;
		}

		return 0;
	} else {
		log_msg(OTTS_LOG_DEBUG,
			"Index mark unknown, inserting the whole message.");

		if (queue_message
		    (msg, -msg->settings.uid, 0, SPD_MSGTYPE_TEXT, 0)
		    == 0) {
			if (OPENTTSD_DEBUG)
				FATAL("Can't queue message\n");
			g_free(msg->buf);
			g_free(msg);
			return -1;
		}

		return 0;
	}
	return 0;
}

void speaking_stop(int uid)
{
	openttsd_message *msg;
	GList *gl;
	GList *queue;
	signed int gid = -1;

	/* Only act if the currently speaking client is the specified one */
	if (get_speaking_client_uid() == uid) {
		output_stop();

		/* Get the queue where the message being spoken came from */
		queue = speaking_get_queue(current_priority);
		if (queue == NULL)
			return;

		/* Get group ID of the current message */
		gl = g_list_last(queue);
		if (gl == NULL)
			return;
		if (gl->data == NULL)
			return;

		msg = (openttsd_message *) gl->data;
		if ((msg->settings.reparted != 0) && (msg->settings.uid == uid)) {
			gid = msg->settings.reparted;
		} else {
			return;
		}

		while (1) {
			gl = g_list_last(queue);
			if (gl == NULL) {
				speaking_set_queue(current_priority, queue);
				return;
			}
			if (gl->data == NULL)
				return;

			msg = (openttsd_message *) gl->data;

			if ((msg->settings.reparted == gid)
			    && (msg->settings.uid == uid)) {
				queue = g_list_remove_link(queue, gl);
				assert(gl->data != NULL);
				mem_free_message(gl->data);
			} else {
				speaking_set_queue(current_priority, queue);
				return;
			}
		}
	}
}

void speaking_stop_all()
{
	openttsd_message *msg;
	GList *gl;
	GList *queue;
	int gid = -1;

	output_stop();

	queue = speaking_get_queue(current_priority);
	if (queue == NULL)
		return;

	gl = g_list_last(queue);
	if (gl == NULL)
		return;
	assert(gl->data != NULL);
	msg = (openttsd_message *) gl->data;

	if (msg->settings.reparted != 0) {
		gid = msg->settings.reparted;
	} else {
		return;
	}

	while (1) {
		gl = g_list_last(queue);
		if (gl == NULL) {
			speaking_set_queue(current_priority, queue);
			return;
		}
		if (OPENTTSD_DEBUG)
			assert(gl->data != NULL);

		msg = (openttsd_message *) gl->data;
		if (msg->settings.reparted == 1) {
			queue = g_list_remove_link(queue, gl);
			assert(gl->data != NULL);
			mem_free_message(gl->data);
		} else {
			speaking_set_queue(current_priority, queue);
			return;
		}
	}
}

void speaking_cancel(int uid)
{
	pthread_mutex_lock(&element_free_mutex);
	speaking_stop(uid);
	stop_from_uid(uid);
	pthread_mutex_unlock(&element_free_mutex);
}

void speaking_cancel_all()
{
	output_stop();
	pthread_mutex_lock(&element_free_mutex);
	stop_priority(SPD_IMPORTANT);
	stop_priority(SPD_MESSAGE);
	stop_priority(SPD_TEXT);
	stop_priority(SPD_NOTIFICATION);
	stop_priority(SPD_PROGRESS);
	pthread_mutex_unlock(&element_free_mutex);
}

int speaking_pause_all(int fd)
{
	int err = 0;
	int i;
	int uid;

	for (i = 1; i <= status.max_fd; i++) {
		uid = get_client_uid_by_fd(i);
		if (uid == 0)
			continue;
		err += speaking_pause(i, uid);
	}

	if (err > 0)
		return 1;
	else
		return 0;
}

int speaking_pause(int fd, int uid)
{
	TFDSetElement *settings;
	int ret;

	log_msg(OTTS_LOG_NOTICE, "Pause");

	/* Find settings for this particular client */
	settings = get_client_settings_by_uid(uid);
	if (settings == NULL) {
		log_msg(OTTS_LOG_NOTICE,
			"ERROR: Can't get settings of active client in speaking_pause()!");
		return 1;
	}
	settings->paused = 1;

	if (speaking_uid != uid) {
		log_msg(OTTS_LOG_DEBUG, "given uid %d not speaking_uid",
			speaking_uid, uid);
		return 0;
	}

	if (SPEAKING) {
		if (current_message == NULL) {
			log_msg(OTTS_LOG_DEBUG, "current_message is null");
			return 0;
		}

		ret = output_pause();
		if (ret < 0) {
			log_msg(OTTS_LOG_DEBUG, "output_pause returned %d",
				ret);
			return 0;
		}

		log_msg(OTTS_LOG_DEBUG,
			"Including current message into the message paused list");
		current_message->settings.paused = 2;
		current_message->settings.paused_while_speaking = 1;
		MessagePausedList =
		    g_list_append(MessagePausedList, current_message);
	}

	return 0;
}

int speaking_resume_all()
{
	int err = 0;
	int i;
	int uid;

	for (i = 1; i <= status.max_fd; i++) {
		uid = get_client_uid_by_fd(i);
		if (uid == 0)
			continue;
		err += speaking_resume(uid);
	}

	if (err > 0)
		return 1;
	else
		return 0;
}

int speaking_resume(int uid)
{
	TFDSetElement *settings;

	/* Find settings for this particular client */
	settings = get_client_settings_by_uid(uid);
	if (settings == NULL)
		return 1;
	/* Set it to speak again. */
	settings->paused = 0;

	resume_requested = 1;
	speaking_semaphore_post();

	return 0;
}

int socket_send_msg(int fd, char *msg)
{
	int ret;

	assert(msg != NULL);
	pthread_mutex_lock(&socket_com_mutex);
	log_msg2(5, "protocol", "%d:REPLY:|%s|", fd, msg);
	ret = write(fd, msg, strlen(msg));
	pthread_mutex_unlock(&socket_com_mutex);
	if (ret < 0) {
		log_msg(OTTS_LOG_ERR, "write() error: %s", strerror(errno));
		return -1;
	}
	return 0;
}

int report_index_mark(openttsd_message * msg, char *index_mark)
{
	char *cmd;
	int ret;

	cmd = g_strdup_printf(EVENT_INDEX_MARK_C "-%d\r\n"
			      EVENT_INDEX_MARK_C "-%d\r\n"
			      EVENT_INDEX_MARK_C "-%s\r\n"
			      EVENT_INDEX_MARK,
			      msg->id, msg->settings.uid, index_mark);
	ret = socket_send_msg(msg->settings.fd, cmd);
	g_free(cmd);
	if (ret) {
		log_msg(OTTS_LOG_ERR, "ERROR: Can't report index mark!");
		return -1;
	}
	return 0;
}

#define REPORT_STATE(state, ssip_code, ssip_msg) \
  int \
  report_ ## state (openttsd_message *msg) \
  { \
    char *cmd; \
    int ret; \
    cmd = g_strdup_printf(ssip_code"-%d\r\n"ssip_code"-%d\r\n"ssip_msg, \
	     msg->id, msg->settings.uid); \
    ret = socket_send_msg(msg->settings.fd, cmd); \
    if (ret){ \
      log_msg(OTTS_LOG_WARN, "ERROR: Can't report index mark!"); \
      return -1; \
    } \
    g_free(cmd); \
    return 0; \
  }

REPORT_STATE(begin, EVENT_BEGIN_C, EVENT_BEGIN)
    REPORT_STATE(end, EVENT_END_C, EVENT_END)
    REPORT_STATE(pause, EVENT_PAUSED_C, EVENT_PAUSED)
    REPORT_STATE(resume, EVENT_RESUMED_C, EVENT_RESUMED)
    REPORT_STATE(cancel, EVENT_CANCELED_C, EVENT_CANCELED)

int is_sb_speaking(void)
{
	int ret;
	char *index_mark;
	TFDSetElement *settings;

	log_msg(OTTS_LOG_DEBUG, "is_sb_speaking(), SPEAKING=%d", SPEAKING);

	/* Determine if the current module is still speaking */
	if (speaking_module != NULL) {
		if (current_message == NULL) {
			log_msg(OTTS_LOG_ERR,
				"Error: Current message is NULL in is_sb_speaking()");
			return -1;
		}
		settings = &(current_message->settings);

		ret = output_is_speaking(&index_mark);
		if (index_mark == NULL)
			return SPEAKING = 0;

		if (!strcmp(index_mark, "no")) {
			g_free(index_mark);
			return SPEAKING;
		}

		log_msg(OTTS_LOG_DEBUG, "INDEX MARK: %s", index_mark);

		if (!strcmp(index_mark, SD_MARK_BODY "begin")) {
			SPEAKING = 1;
			if (!settings->paused_while_speaking) {
				if (settings->notification & SPD_BEGIN)
					report_begin(current_message);
			} else {
				if (settings->notification & SPD_RESUME)
					report_resume(current_message);
				settings->paused_while_speaking = 0;
			}
		} else if (!strcmp(index_mark, SD_MARK_BODY "end")) {
			SPEAKING = 0;
			poll_count = 1;
			if (settings->notification & SPD_END)
				report_end(current_message);
			speaking_semaphore_post();
		} else if (!strcmp(index_mark, SD_MARK_BODY "paused")) {
			SPEAKING = 0;
			poll_count = 1;
			if (settings->notification & SPD_PAUSE)
				report_pause(current_message);
			/* We don't want to free this message in speak() since we will
			   later copy it in resume() */
			current_message = NULL;
		} else if (!strcmp(index_mark, SD_MARK_BODY "stopped")) {
			SPEAKING = 0;
			poll_count = 1;
			if (settings->notification & SPD_CANCEL)
				report_cancel(current_message);
			speaking_semaphore_post();
		} else if (index_mark != NULL) {
			if (strncmp(index_mark, SD_MARK_BODY, SD_MARK_BODY_LEN)) {
				if (settings->notification & SPD_INDEX_MARKS)
					report_index_mark(current_message,
							  index_mark);
			} else {
				log_msg(OTTS_LOG_DEBUG,
					"Setting current index_mark for the message to %s",
					index_mark);
				if (current_message->settings.index_mark !=
				    NULL)
					g_free(current_message->settings.
					       index_mark);
				current_message->settings.index_mark =
				    g_strdup(index_mark);
			}

		}
		g_free(index_mark);
	} else {
		log_msg(OTTS_LOG_DEBUG, "Speaking module is NULL, SPEAKING==%d",
			SPEAKING);
		SPEAKING = 0;
	}

	if (SPEAKING == 0)
		speaking_module = NULL;

	return SPEAKING;
}

int get_speaking_client_uid(void)
{
	int speaking = 0;
	if (SPEAKING == 0) {
		speaking_uid = 0;
		return 0;
	}
	if (speaking_uid != 0) {
		speaking = speaking_uid;
	}
	return speaking;
}

GList *queue_remove_message(GList * queue, GList * gl)
{
	openttsd_message *msg;
	assert(gl != NULL);
	assert(gl->data != NULL);
	msg = (openttsd_message *) gl->data;
	if (msg->settings.notification & SPD_CANCEL)
		report_cancel(msg);
	mem_free_message(gl->data);
	queue = g_list_delete_link(queue, gl);
	return queue;
}

GList *empty_queue(GList * queue)
{
	int num, i;
	GList *gl;

	num = g_list_length(queue);
	for (i = 0; i <= num - 1; i++) {
		gl = g_list_first(queue);
		queue = queue_remove_message(queue, gl);
	}

	return queue;
}

GList *empty_queue_by_time(GList * queue, unsigned int uid)
{
	int num, i;
	GList *gl, *gln;
	openttsd_message *msg;

	num = g_list_length(queue);
	gl = g_list_first(queue);
	for (i = 0; i <= num - 1; i++) {
		gln = g_list_next(gl);
		if (gl == NULL)
			break;
		assert(gl->data != NULL);
		msg = gl->data;
		if (msg->id < uid) {
			queue = queue_remove_message(queue, gl);
		}
		gl = gln;
	}

	return queue;
}

int stop_priority(SPDPriority priority)
{
	GList *queue;

	queue = speaking_get_queue(priority);

	if (current_priority == priority) {
		output_stop();
	}

	queue = empty_queue(queue);

	speaking_set_queue(priority, queue);

	return 0;
}

int stop_priority_older_than(SPDPriority priority, unsigned int uid)
{
	GList *queue;

	queue = speaking_get_queue(priority);

	if (current_priority == priority) {
		output_stop();
	}

	queue = empty_queue_by_time(queue, uid);

	speaking_set_queue(priority, queue);

	return 0;
}

GList *stop_priority_from_uid(GList * queue, const int uid)
{
	GList *ret = queue;
	GList *gl;

	while ((gl = g_list_find_custom(ret, &uid, compare_message_uid)))
		ret = queue_remove_message(ret, gl);

	return ret;
}

void stop_from_uid(const int uid)
{
	check_locked(&element_free_mutex);
	MessageQueue->p1 = stop_priority_from_uid(MessageQueue->p1, uid);
	MessageQueue->p2 = stop_priority_from_uid(MessageQueue->p2, uid);
	MessageQueue->p3 = stop_priority_from_uid(MessageQueue->p3, uid);
	MessageQueue->p4 = stop_priority_from_uid(MessageQueue->p4, uid);
	MessageQueue->p5 = stop_priority_from_uid(MessageQueue->p5, uid);
}

/* Determines if this messages is to be spoken
 * (returns 1) or its parent client is paused (returns 0).
 * Note: If you are wondering why it's reversed (not to speak instead
 * of to speak), it's because we also use this function for
 * searching through the list. */
gint message_nto_speak(gconstpointer data, gconstpointer nothing)
{
	TFDSetElement *global_settings;
	openttsd_message *message = (openttsd_message *) data;

	/* Is there something in the body of the message? */
	if (message == NULL)
		return 0;

	/* Find global settings for this connection. */
	global_settings = get_client_settings_by_fd(message->settings.fd);
	if (global_settings == NULL)
		return 0;

	if (!global_settings->paused)
		return 0;
	else
		return 1;
}

void set_speak_thread_attributes()
{
	int ret;
	sigset_t all_signals;

	ret = sigfillset(&all_signals);
	if (ret == 0) {
		ret = pthread_sigmask(SIG_BLOCK, &all_signals, NULL);
		if (ret != 0)
			log_msg(OTTS_LOG_ERR,
				"Can't set signal set, expect problems when terminating!");
	} else {
		log_msg(OTTS_LOG_ERR,
			"Can't fill signal set, expect problems when terminating!");
	}

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

void stop_priority_except_first(SPDPriority priority)
{
	GList *queue;
	GList *gl;
	openttsd_message *msg;
	GList *gl_next;
	int gid;

	queue = speaking_get_queue(priority);

	gl = g_list_last(queue);

	if (gl == NULL)
		return;
	if (gl->data == NULL)
		return;

	msg = (openttsd_message *) gl->data;
	if (msg->settings.reparted <= 0) {
		queue = g_list_remove_link(queue, gl);
		speaking_set_queue(priority, queue);

		stop_priority(priority);
		/* Fill the queue with the list containing only the first message */
		speaking_set_queue(priority, gl);
	} else {
		gid = msg->settings.reparted;

		if (current_priority == priority && speaking_gid != gid) {
			output_stop();
		}

		gl = g_list_first(queue);
		while (gl) {
			gl_next = g_list_next(gl);
			if (gl->data != NULL) {
				openttsd_message *msgg = gl->data;
				if (msgg->settings.reparted != gid) {
					queue = g_list_remove_link(queue, gl);
					mem_free_message(msgg);
				}
			}
			gl = gl_next;
		}
		speaking_set_queue(priority, queue);
	}

	return;
}

void resolve_priorities(SPDPriority priority)
{
	/* for the priority algorithm see
	   http://cvs.freebsoft.org/doc/speechd/ssip_10.html#SEC11 */
	switch (priority) {
	case SPD_IMPORTANT:
		/* stop current */
		if (SPEAKING && current_priority != SPD_IMPORTANT)
			output_stop();

		/* cancel all messages */
		stop_priority(SPD_NOTIFICATION);
		stop_priority(SPD_PROGRESS);
		break;

	case SPD_MESSAGE:
		/* stop current */
		if (SPEAKING && current_priority != SPD_IMPORTANT
		    && current_priority != SPD_MESSAGE)
			output_stop();

		/* cancel all messages */
		stop_priority(SPD_TEXT);
		stop_priority(SPD_NOTIFICATION);
		stop_priority(SPD_PROGRESS);
		break;

	case SPD_TEXT:
		stop_priority_except_first(SPD_TEXT);

		/* cancel all messages */
		stop_priority(SPD_NOTIFICATION);
		stop_priority(SPD_PROGRESS);
		break;

	case SPD_NOTIFICATION:
		stop_priority_except_first(SPD_NOTIFICATION);
		if (SPEAKING && current_priority != SPD_NOTIFICATION)
			stop_priority(SPD_NOTIFICATION);
		break;

	case SPD_PROGRESS:
		stop_priority(SPD_NOTIFICATION);
		if (SPEAKING) {
			GList *gl;
			check_locked(&element_free_mutex);
			gl = g_list_last(MessageQueue->p5);
			check_locked(&element_free_mutex);
			MessageQueue->p5 =
			    g_list_remove_link(MessageQueue->p5, gl);
			if (gl != NULL) {
				check_locked(&element_free_mutex);
				MessageQueue->p5 =
				    empty_queue(MessageQueue->p5);
				if (gl->data != NULL) {
					MessageQueue->p5 = gl;
				}
			}
		}
	default:
		break;
	}
}

openttsd_message *get_message_from_queues()
{
	GList *gl;
	SPDPriority prio;

	/* We will descend through priorities to say more important
	   messages first. */
	for (prio = SPD_IMPORTANT; prio <= SPD_PROGRESS; prio++) {
		GList *current_queue = speaking_get_queue(prio);
		check_locked(&element_free_mutex);
		gl = g_list_first(current_queue);

		while (gl != NULL) {
			if (message_nto_speak
			    ((openttsd_message *) gl->data, NULL)) {
				gl = g_list_next(gl);
				continue;
			}
			speaking_set_queue(prio,
					   g_list_remove_link(current_queue,
							      gl));
			current_priority = prio;
			return (openttsd_message *) gl->data;
		}
	}

	return NULL;
}

GList *speaking_get_queue(SPDPriority priority)
{
	GList *queue = NULL;

	assert(priority != SPD_PRIORITY_ERR);

	check_locked(&element_free_mutex);
	switch (priority) {
	case SPD_IMPORTANT:
		queue = MessageQueue->p1;
		break;
	case SPD_MESSAGE:
		queue = MessageQueue->p2;
		break;
	case SPD_TEXT:
		queue = MessageQueue->p3;
		break;
	case SPD_NOTIFICATION:
		queue = MessageQueue->p4;
		break;
	case SPD_PROGRESS:
		queue = MessageQueue->p5;
		break;
	case SPD_PRIORITY_ERR:
		/* Should never get here.  See above assertion. */
		break;
	}

	return queue;
}

void speaking_set_queue(SPDPriority priority, GList * queue)
{
	assert(priority != SPD_PRIORITY_ERR);

	check_locked(&element_free_mutex);
	switch (priority) {
	case SPD_IMPORTANT:
		MessageQueue->p1 = queue;
		break;
	case SPD_MESSAGE:
		MessageQueue->p2 = queue;
		break;
	case SPD_TEXT:
		MessageQueue->p3 = queue;
		break;
	case SPD_NOTIFICATION:
		MessageQueue->p4 = queue;
		break;
	case SPD_PROGRESS:
		MessageQueue->p5 = queue;
		break;
	case SPD_PRIORITY_ERR:
		/* Not reached.  See previous assertion. */
		break;
	}
}

gint sortbyuid(gconstpointer a, gconstpointer b)
{
	const openttsd_message *msg1 = a;
	const openttsd_message *msg2 = b;

	if ((msg1 == NULL) && (msg2 != NULL))
		return -1;
	if ((msg1 != NULL) && (msg2 == NULL))
		return +1;
	if ((msg1 == NULL) && (msg2 == NULL))
		return 0;

	return msg1->id - msg2->id;
}

/* Clean up after abnormal termination of the current output module. */
static void speaking_module_cleanup(void)
{

	/*
	 * First, Discard the current message, and tell the client that we are
	 * done speaking it.  Should we try to put it back onto the
	 * appropriate queue, instead?
	 */

	if (current_message != NULL) {
		if (SPEAKING
		    && (current_message->settings.notification & SPD_END))
			report_end(current_message);
		if (!current_message->settings.paused_while_speaking)
			mem_free_message(current_message);
		current_message = NULL;
	}

	/*
	 * Indicate that we are done speaking.  Indicate that the current
	 * output module is no longer working.  Finally, set poll_count
	 * to 1, so we won't try to poll this module's file descriptor
	 * again.
	 */
	SPEAKING = 0;
	if (speaking_module != NULL)
		speaking_module->working = 0;
	speaking_module = NULL;
	poll_count = 1;
}
