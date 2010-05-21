/*
 * options.h - Defines possible command line options
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

#ifndef OPTIONS_H
#define OPTIONS_H

/* Global variable declarations. */
extern signed int rate;
extern signed int pitch;
extern signed int volume;

extern char *output_module;
extern char *language;
extern char *voice_type;
extern char *punctuation_mode;
extern char *priority;
extern int pipe_mode;
extern SPDDataMode ssml_mode;
extern int spelling;
extern int wait_till_end;
extern int stop_previous;
extern int cancel_previous;

extern char *application_name;
extern char *connection_name;

/* Function prototypes. */
int options_parse(int argc, char *argv[]);
void options_print_version();
void options_print_help(char *argv[]);
#endif
