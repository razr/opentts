/*
 * sighandler.c - Signal handling code.
 *
 *
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

#include <signal.h>
#include <glib.h>

#include "openttsd.h"
#include "server.h"
#include "sighandler.h"

static sigset_t blocked_signals;

static void reload_dead_modules(void);
static void install_dummy_handlers(void);

/*
 * catch_signals runs in our signal-handling thread.
 * Once all threads are started, it sits in a loop, repeatedly
 * calling sigwait to grab signals sent to the process.
 * If it catches SIGTERM or SIGINT, the loop terminates.
 * If it catches SIGHUP, it reloads the configuration file in
 * a thread-safe way.  We reload dead modules if SIGUSR1 is
 * received.  When the loop finishes, we tell the main thread to stop.
 *
 * This code was heavily inspired by the espeakup program by William
 * Hubbs.
 */

void *catch_signals(void *data)
{
	gboolean quitting = FALSE;
	int current_signal = 0;

	install_dummy_handlers();

	/*
	 * Don't start catching signals until the speaking thread is running.
	 * If the speaking thread couldn't be started, there's
	 * nothing for us to do.
	 */
	pthread_mutex_lock(&thread_controller);
	if (speak_thread_started == FALSE)
		quitting = TRUE;
	pthread_mutex_unlock(&thread_controller);

	while (quitting != TRUE) {
		sigwait(&blocked_signals, &current_signal);
		switch (current_signal) {
		case SIGINT:
		case SIGTERM:	/* Fall-through. */
			quitting = TRUE;
			break;
		case SIGUSR1:
			MSG(3,
			    "Reloading dead output modules at user request.");
			stop_speak_thread();
			reload_dead_modules();
			start_speak_thread();
			pthread_mutex_lock(&thread_controller);
			if (speak_thread_started == FALSE)
				quitting = TRUE;
			pthread_mutex_unlock(&thread_controller);
			break;
		case SIGHUP:
			MSG(3, "Reloading configuration file at user request.");
			stop_speak_thread();
			load_configuration();
			start_speak_thread();
			pthread_mutex_lock(&thread_controller);
			if (speak_thread_started == FALSE)
				quitting = TRUE;
			pthread_mutex_unlock(&thread_controller);
			break;
		default:
			break;	/* Nothing to do for this signal. */
		}
	}

	stop_main_thread();

	return NULL;
}

static void add_signal(int sig)
{
	int ret = sigaddset(&blocked_signals, sig);
	if (ret != 0)
		FATAL("Unable to add a signal to the blocked set.");
}

/*
	 * Add four signals to the set of signals that should be blocked.
	 * Next, set our signal mask, so that this set is blocked.
	 */

void init_blocked_sig_set(void)
{
	int ret;
	add_signal(SIGINT);
	add_signal(SIGTERM);
	add_signal(SIGHUP);
	add_signal(SIGUSR1);
	ret = pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL);
	if (ret != 0)
		FATAL("Unable to set signal mask.");
}

static void reload_dead_modules(void)
{
	/* Reload dead modules */
	g_hash_table_foreach(output_modules, modules_reload, NULL);

	/* Make sure there aren't any more child processes left */
	while (waitpid(-1, NULL, WNOHANG) > 0) ;
}

static void dummy_handler(int sig)
{
	return;
}

static void install_dummy_handlers(void)
{
	/* install dummy handlers for the signals we want to process */
	struct sigaction temp;
	temp.sa_handler = dummy_handler;
	sigemptyset(&temp.sa_mask);
	sigaction(SIGINT, &temp, NULL);
	sigaction(SIGHUP, &temp, NULL);
	sigaction(SIGTERM, &temp, NULL);
	sigaction(SIGUSR1, &temp, NULL);
}
