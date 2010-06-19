/*
* logging.h - prototypes for logging routines
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

#ifndef _LOGGING_H
#define _LOGGING_H

enum LogLevel {
	OTTS_LOG_CRIT,
	OTTS_LOG_ERR,
	OTTS_LOG_WARN,
	OTTS_LOG_NOTICE,
	OTTS_LOG_INFO,
	OTTS_LOG_DEBUG,
};

enum DebugLevel {
	DEBUG_OFF,
	DEBUG_ON,
};

void init_logging(void);
void open_log(char *new_log_file, enum LogLevel new_log_level);
void close_log(void);
void open_debug_log(char *new_debug_file, enum DebugLevel new_debug);
void close_debug_log(void);
void open_custom_log(char *file_name, char *new_type);
void close_custom_log(void);
void log_msg(enum LogLevel level, char *format, ...);
void log_msg2(enum LogLevel level, char *kind, char *format, ...);
void logging_end(void);

#endif
