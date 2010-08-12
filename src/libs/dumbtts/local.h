/*
 * libdumbtts - dumb synthesizers helper library
 * Copyright (C) Bohdan R. Rau 2008 <ethanak@polip.com>
 *
 * libdumbtts is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libdumbtts is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libdumbtts.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef DUMBTTS_LOCAL_H
#define DUMBTTS_LOCAL_H
#define MEMO_BLOCK_SIZE 8160

struct dumbtts_char {
	char *name;
	char *prono;
};

struct memblock {
	struct memblock *next;
	int firstfree;
	int lastfree;
	char block[MEMO_BLOCK_SIZE];
};

struct dumbtts_keyname {
	struct dumbtts_keyname *next;
	char *key;
	char *trans;
};


struct dumbtts_charlist {
	struct dumbtts_charlist *next;
	wchar_t wc;
	char *name;
	char *prono;
};

struct dumbtts_cyrtrans {
	struct dumbtts_cyrtrans *next;
	short c1;
	short c2;
	char *translit;
};

struct dumbtts_abbr {
	struct dumbtts_abbr *next;
	char *abbr;
	char *res;
};

struct dumbtts_rconf {
	int mode;
	int nchar;
	wchar_t wc[128];
	char ch[128];
};

struct dumbtts_intpair {
	struct dumbtts_intpair *next;
	int v1;
	int v2;
};


struct dumbtts_format_item {
	struct dumbtts_format_item *next;
	int value;
	char *format;
};

struct dumbtts_format {
	struct dumbtts_format *next;
	char *name;
	char *deflt;
	struct dumbtts_format_item *dfi;
};

struct dumbtts_choice {
	struct dumbtts_choice *next;
	char *name;
	struct dumbtts_format_item *dfi;
};

struct dumbtts_arth_item {
	char mode;
	union {
		int d;
		struct dumbtts_arth_item *it;
	} v;
};


struct dumbtts_diction_letter {
	struct dumbtts_diction_letter *nlet[32]; //nastêpna litera
	struct dumbtts_diction_letter *parent;
	struct dumbtts_abbr *patterns;
};

struct dumbtts_expr {
	char type;
	char operator;
	union {
		int ival;
		double fval;
		struct dumbtts_expr *ops[2];
	} u;
};

struct dumbtts_formex {
	struct dumbtts_formex *next;
	short type;
	short forma;
	struct dumbtts_expr *expr;
};

struct dumbtts_unit {
	struct dumbtts_unit *next;
	char *unit;
	char *result;
};

struct dumbtts_conf {
	int flags;
	struct dumbtts_keyname *keynames;
	struct dumbtts_charlist *charlist;
	struct dumbtts_cyrtrans *cyrtrans;
	struct dumbtts_abbr *abbrev;
	struct dumbtts_char chartable[0x180];
	char *cap;
	char *caplet;
	char *lig;
	char *caplig;
	char *sym;
	char *let;
	char *ill;
	char *dia[16];
	struct dumbtts_rconf rconf;
	struct memblock *memo;
	int roman_minlen;
	int roman_maxval;
	struct dumbtts_intpair *roman_ignore;
	struct dumbtts_abbr *dic;
	struct dumbtts_format *formats;
	struct dumbtts_choice *choices;
	struct dumbtts_abbr *recog;
	struct dumbtts_abbr *sayas;
	struct dumbtts_formex *formex;
	short defforms[2];
	struct dumbtts_unit *units;
	struct dumbtts_diction_letter *dics[32];
};

#include "libdumbtts.h"
#endif
