/*
 * fctype.h - simplified, locale independent wctype functions
 * Copyright (C) Bohdan R. Rau 2008 <ethanak@polip.com>
 *
 * libdumbtts.c is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libdumbtts.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libdumbtts.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef __FCTYPE_H
#define __FCTYPE_H

#include <wchar.h>

int isfalnum(wint_t wc);
int isfdigit(wint_t wc);
int isfalpha(wint_t wc);
int isfupper(wint_t wc);
int isflower(wint_t wc);
int isfspace(wint_t wc);
wint_t toflower(wint_t wc);

#endif /* __FCTYPE_H */