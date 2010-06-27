
/*
 * module.c - Output modules for the OpenTTS daemon
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
 * $Id: module.c,v 1.40 2008-07-07 14:30:51 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <getline.h>
#include<logging.h>
#include "openttsd.h"
#include "output.h"

static int send_initial_commands(OutputModule * module);
static void start_module(OutputModule * module);

void destroy_module(OutputModule * module)
{
	g_free(module->name);
	g_free(module->filename);
	if (module->debugfilename)
		g_free(module->debugfilename);
	g_free(module->configfilename);
	g_free(module);
}

static OutputModule *create_module(char *name, char *prog, char *cfg_file,
				   char *dbg_file)
{
	OutputModule *module;
	char *module_conf_dir;

	module = g_malloc(sizeof(OutputModule));
	module->name = g_strdup(name);
	module->filename = get_path(prog, MODULEBINDIR);
	module->debugfilename = g_strdup(dbg_file);
	module_conf_dir = g_strdup_printf("%s/modules/", options.conf_dir);
	module->configfilename = get_path(cfg_file, module_conf_dir);
	g_free(module_conf_dir);

	return module;
}

OutputModule *load_output_module(char *mod_name, char *mod_prog,
				 char *mod_cfgfile, char *mod_dbgfile)
{
	OutputModule *module;
	int ret, pid;

	if (mod_name == NULL)
		return NULL;

	module = create_module(mod_name, mod_prog, mod_cfgfile, mod_dbgfile);

	if (!strcmp(mod_name, "testing")) {
		module->pipe_in[1] = 1;	/* redirect to stdin */
		module->pipe_out[0] = 0;	/* redirect to stdout */
		return module;
	}

	if ((pipe(module->pipe_in) != 0) || (pipe(module->pipe_out) != 0)) {
		log_msg(OTTS_LOG_NOTICE, "Can't open pipe! Module not loaded.");
		destroy_module(module);
		return NULL;
	}

	/* Open the file for child stderr (logging) redirection */
	if (module->debugfilename != NULL) {
		module->stderr_redirect = open(module->debugfilename,
					       O_WRONLY | O_CREAT | O_TRUNC,
					       S_IRUSR | S_IWUSR);
		if (module->stderr_redirect == -1)
			log_msg(OTTS_LOG_ERR,
				"ERROR: Openning debug file for %s failed: (error=%d) %s",
				module->name, module->stderr_redirect,
				strerror(errno));
	} else {
		module->stderr_redirect = -1;
	}

	log_msg(OTTS_LOG_WARN,
		"Initializing output module %s with binary %s and configuration %s",
		module->name, module->filename, module->configfilename);
	if (module->stderr_redirect >= 0)
		log_msg(OTTS_LOG_WARN, "Output module is logging to file %s",
			module->debugfilename);
	else
		log_msg(OTTS_LOG_WARN,
			"Output module is logging to standard error output (stderr)");

	pid = fork();
	if (pid == -1) {
		printf("Can't fork, error! Module not loaded.");
		destroy_module(module);
		return NULL;
	}

	if (pid == 0) {
		start_module(module);
	}

	module->pid = pid;
	close(module->pipe_in[0]);
	close(module->pipe_out[1]);

	usleep(100);		/* So that the other child has at least time to fail
				   with the execlp */
	ret = waitpid(module->pid, NULL, WNOHANG);
	if (ret != 0) {
		log_msg(OTTS_LOG_WARN,
			"ERROR: Can't load output module %s with binary %s. "
			"Bad filename in configuration?", module->name,
			module->filename);
		destroy_module(module);
		return NULL;
	}

	module->working = 1;
	log_msg(OTTS_LOG_WARN, "Module %s loaded.", module->name);

	/* Create a stream from the socket */
	module->stream_out = fdopen(module->pipe_out[0], "r");
	if (!module->stream_out)
		FATAL("Can't create a stream for socket, fdopen() failed.");

	/* Switch to line buffering mode */
	ret = setvbuf(module->stream_out, NULL, _IONBF, 4096);
	if (ret)
		FATAL("Can't set line buffering, setvbuf failed.");

	if (0 != send_initial_commands(module)) {
		module->working = 0;
		kill(module->pid, SIGKILL);
		waitpid(module->pid, NULL, WNOHANG);
		destroy_module(module);
		return NULL;
	}

	return module;
}

int send_initial_commands(OutputModule * module)
{
	FILE *f;
	size_t n = 0;
	GString *reply;
	char s, *rep_line = NULL;
	int ret;

	log_msg(OTTS_LOG_INFO, "Trying to initialize %s.", module->name);
	if (output_send_data("INIT\n", module, 0) != 0) {
		log_msg(OTTS_LOG_ERR,
			"ERROR: Something wrong with %s, can't initialize",
			module->name);
		return -1;
	}

	reply = g_string_new("\n---------------\n");
	f = fdopen(dup(module->pipe_out[0]), "r");
	while (1) {
		ret = otts_getline(&rep_line, &n, f);
		if (ret <= 0) {
			log_msg(OTTS_LOG_ERR,
				"ERROR: Bad syntax from output module %s 1",
				module->name);
			if (rep_line != NULL)
				free(rep_line);
			return -1;
		}

		assert(rep_line != NULL);
		log_msg(OTTS_LOG_DEBUG, "Reply from output module: %d %s", n,
			rep_line);
		if (strlen(rep_line) <= 4) {
			log_msg(OTTS_LOG_ERR,
				"ERROR: Bad syntax from output module %s 2",
				module->name);
			free(rep_line);
			return -1;
		}

		if (rep_line[3] == '-')
			g_string_append(reply, rep_line + 4);
		else {
			s = rep_line[0];
			free(rep_line);
			break;
		}

	}

	g_string_append_printf(reply, "---------------\n");
	if (s == '2')
		log_msg(OTTS_LOG_WARN,
			"Module %s started sucessfully with message: %s",
			module->name, reply->str);
	else if (s == '3') {
		log_msg(OTTS_LOG_ERR,
			"ERROR: Module %s failed to initialize. Reason: %s",
			module->name, reply->str);
		return -1;
	}
	g_string_free(reply, 1);

	if (options.debug) {
		log_msg(OTTS_LOG_INFO,
			"Switching debugging on for output module %s",
			module->name);
		output_module_debug(module);
	}

	/* Initialize audio settings */
	ret = output_send_audio_settings(module);
	if (ret != 0) {
		log_msg(OTTS_LOG_ERR,
			"ERROR: Can't initialize audio in output module, see reason above.");
		return -1;
	}

	/* Send log level configuration setting */
	ret = output_send_loglevel_setting(module);
	if (ret != 0) {
		log_msg(OTTS_LOG_ERR,
			"ERROR: Can't set the log level inin the output module.");
		return -1;
	}

	/* Get a list of supported voices */
	_output_get_voices(module);
	fclose(f);
	return 0;
}

static void start_module(OutputModule * module)
{
	int ret;

	ret = dup2(module->pipe_in[0], 0);
	close(module->pipe_in[0]);
	close(module->pipe_in[1]);

	ret = dup2(module->pipe_out[1], 1);
	close(module->pipe_out[1]);
	close(module->pipe_out[0]);

	/* Redirrect stderr to the appropriate logfile */
	if (module->stderr_redirect >= 0) {
		ret = dup2(module->stderr_redirect, 2);
	}

	if (module->configfilename) {
		execlp(module->filename, "", module->configfilename, (char *)0);
	} else {
		execlp(module->filename, "", (char *)0);
	}

	log_msg(OTTS_LOG_CRIT, "failed to exec module %s!", module->filename);
	exit(1);
}

int unload_output_module(OutputModule * module)
{
	assert(module != NULL);

	log_msg(OTTS_LOG_NOTICE, "Unloading module name=%s", module->name);

	output_close(module);

	close(module->pipe_in[1]);
	close(module->pipe_out[0]);

	destroy_module(module);

	return 0;
}

int reload_output_module(OutputModule * old_module)
{
	OutputModule *new_module;

	assert(old_module != NULL);
	assert(old_module->name != NULL);

	if (old_module->working)
		return 0;

	log_msg(OTTS_LOG_NOTICE, "Reloading output module %s",
		old_module->name);

	output_close(old_module);
	close(old_module->pipe_in[1]);
	close(old_module->pipe_out[0]);

	new_module = load_output_module(old_module->name, old_module->filename,
					old_module->configfilename,
					old_module->debugfilename);
	if (new_module == NULL) {
		log_msg(OTTS_LOG_NOTICE,
			"Can't load module %s while reloading modules.",
			old_module->name);
		return -1;
	}

	g_hash_table_replace(output_modules, new_module->name, new_module);
	destroy_module(old_module);

	return 0;
}

int output_module_debug(OutputModule * module)
{
	char *new_log_path;

	assert(module != NULL);
	assert(module->name != NULL);
	if (!module->working)
		return -1;

	log_msg(OTTS_LOG_INFO, "Output module debug logging for %s into %s",
		module->name, options.debug_destination);

	new_log_path = g_strdup_printf("%s/%s.log",
				       options.debug_destination, module->name);

	output_send_debug(module, 1, new_log_path);

	return 0;
}

int output_module_nodebug(OutputModule * module)
{
	assert(module != NULL);
	assert(module->name != NULL);
	if (!module->working)
		return -1;

	log_msg(OTTS_LOG_INFO, "Output module debug logging off for",
		module->name);

	output_send_debug(module, 0, NULL);

	return 0;
}
