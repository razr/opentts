
/*
 * compare.c - Auxiliary functions for comparing elements in lists
 * 
 * Copyright (C) 2001, 2002, 2003, 2007 Brailcom, o.p.s.
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
 * $Id: compare.c,v 1.5 2007-02-17 18:58:53 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>

#include <glib.h>
#include "openttsd.h"
#include "compare.h"

gint compare_message_fd(gconstpointer element, gconstpointer value, gpointer x)
{
	int *fd_val;
	openttsd_message *message;

	fd_val = (int *)value;

	message = ((openttsd_message *) element);
	assert(message != NULL);
	assert(message->settings.fd != 0);

	return (message->settings.fd - *fd_val);
}

gint compare_message_uid(gconstpointer element, gconstpointer value)
{
	int *uid_val;
	openttsd_message *message;

	uid_val = (int *)value;

	message = ((openttsd_message *) element);
	assert(message != NULL);

	return (message->settings.uid - *uid_val);
}
