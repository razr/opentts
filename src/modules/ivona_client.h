/*
 * ivona_client.h - Declarations from ivona_client.c
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
#ifndef IVONA_CLIENT_H
#define IVONA_CLIENT_H

/* A constant, used in both ivona.c and ivona_client.c */
#define IVONA_CACHE_MAX_STRLEN 11

int ivona_init_sock(char *host, int port);
int ivona_get_msgpart(struct dumbtts_conf *conf, SPDMessageType type,
		      char **msg, char *icon, char **buf, int *len,
		      int cap_mode, char *delimeters, int punct_mode,
		      char *punct_some);
int ivona_send_string(char *to_say);
char *ivona_get_wave_fd(int fd, int *nsamples, int *offset);
char *ivona_get_wave(char *to_say, int *nsamples, int *offset);
void play_icon(char *path, char *name);
void ivona_init_cache(void);
void ivona_store_wave_in_cache(char *to_say, char *wave, int nsamples);
char *ivona_get_wave_from_cache(char *to_say, int *nsamples);
#endif
