/*
 * libdumbtts_dic.c - configuration reader
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

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "local.h"

static void get_diction(struct dumbtts_conf *conf,char *str,char *repl)
{
	int litera,n;
	struct dumbtts_diction_letter *dl,*paren;
	struct dumbtts_abbr *pair;
	char *c=str;
	struct dumbtts_diction_letter **dc=conf->dics;

	dl=NULL;
	for (;;) {
		litera=get_unichar(c,&c,0);
		if (!litera || !isfalnum(litera)) break;
		litera=toflower(litera);
		n=litera & 31;
		paren=dl;
		dl=dc[n];
		if (!dl) {
			if (!(dl=allocMemBlock(conf,sizeof(*dl),1))) return;
			memset(dl,0,sizeof(*dl));
			dc[n]=dl;
			dl->parent=paren;
		}
		dc=dl->nlet;
	}
	if (!dl) return;
	pair=allocMemBlock(conf,sizeof(*pair),1);
	if (!pair) return;
	if (!(pair->abbr=strdupMemBlock(conf,str))) return;
	if (!(pair->res=strdupMemBlock(conf,repl))) return;
	pair->next=dl->patterns;
	dl->patterns=pair;

}

void dumbtts_LoadDictionary(struct dumbtts_conf *conf,char *fname)
{
	FILE *f;
	char buf[1024];
	char *line,*c;
	struct dumbtts_abbr *dic;
	if (!conf) return;
	f=fopen(fname,"r");
	if (!f) return;
	while (fgets(buf,1024,f)) {
		int bra,ok;wchar_t pc;
		c=strstr(buf,"//");
		if (c) *c=0;
		line=line_trim(buf);
		if (!*line) continue;
		c=line;
		bra=ok=0;
		while (*c) {
			char *d;
			pc=get_unichar(c,&d,0);
			if (!pc) break;
			if (isfspace(pc)) {
				if (!bra) {
					ok=1;
					*c=0;
					c=d;
				}
				break;
			}
			c=d;
			if (pc=='(') {
				if (bra) break;
				bra=2;
				continue;
			}
			if (pc==')') {
				if (bra!=2) break;
				bra=0;
				pc=get_unichar(c,NULL,0);
				if (isfspace(pc)) continue;
				if (pc < 0x7f && strchr("+-_",pc)) continue;
				break;
			}
			if (pc=='|') {
				if (bra != 2) break;
				continue;
			}
			if (pc=='[') {
				if (bra) break;
				bra=1;
				continue;
			}
			if (pc==']') {
				if (bra != 1) break;
				bra=0;
				continue;
			}
		}
		if (!ok) continue;
		c=local_conv(c,conf);
		if (!c || !*c) continue;
		if (isdigit(*line)) {
			dic=allocMemBlock(conf,sizeof(*dic),1);
			if (!dic) break;
			if (!(dic->abbr=strdupMemBlock(conf,line))) break;
			if (!(dic->res=strdupMemBlock(conf,c))) break;
			dic->next=conf->dic;
			conf->dic=dic;
			continue;
		}
		get_diction(conf,line,c);
	}
	fclose(f);
}

/* matcher */

static int dic_compare_word(struct dumbtts_abbr *da,char *str,char **res,wchar_t wc,char *db,struct dumbtts_conf *conf)
{
	char *pat=da->abbr;
	int nmatch,n;
	char *matches[10];
	wchar_t pc,sc;
	char *c;


	int chrget(char *str,char **outstr)
	{
		int n;
		if (wc) {
			n=wc;
			wc=0;
			return n;
		}
		return get_unichar(str,outstr,0);
	}

	nmatch=0;
	for (;;) {
		if (!*pat) { /* end of pattern */
			if (!*str) break;
			sc=get_unichar(str,NULL,0);
			if (sc && !isfalnum(sc)) break;
			return 0;
		}
		pc=get_unichar(pat,&pat,0);
		if (pc=='_') { /* optional space */
			for (;*str;) {
				sc=chrget(str,&c);
				if (!sc) return 0;
				if (!isfspace(sc)) break;
				str=c;
			}
			continue;
		}
		if (pc=='+') { /* space */
			if (!*str) return 0;
			sc=chrget(str,&str);
			if (!sc || !isfspace(sc)) return 0;
			for (;*str;) {
				sc=chrget(str,&c);
				if (!sc) return 0;
				if (!isfspace(sc)) break;
				str=c;
			}
			continue;
		}
		if (pc=='[') {
			sc=chrget(str,&str);
			if (!sc) return 0;
			while (*pat) {
				pc=get_unichar(pat,&pat,0);
				if (pc==']') return 0; /* not found */
				if (isflower(pc)){
					if (pc==toflower(sc)) break;
				}
				else {
					if (pc == sc) break;
				}
			}
			if (pc != ']') {
				pat=strchr(pat,']');
				if (!pat) return 0;
			}
			pat++;
			continue;
		}
		if (pc=='`') {
			sc=chrget(str,&c);
			if (sc=='\'') str=c;
			continue;
		}
		if (pc != '(') {
			sc=chrget(str,&str);
			if (isflower(pc)) sc=toflower(sc);
			if (sc != pc) return 0;
			continue;
		}
		if (nmatch>=10) return 0; /* too many matches */
		for (;;) {
			char *pstr,*sstr;
			wchar_t owc;
			int found;
			if (!*pat) return 0;
			pstr=pat;
			sstr=str;
			owc=wc;
			found=0;
			for (;;) {
				pc=get_unichar(pat,&pat,0);
				if (!pc) return 0;
				if (pc=='|' || pc==')') {
					found=1;
					break;
				}
				sc=chrget(sstr,&sstr);
				if (isflower(pc)) sc=toflower(sc);
				if (sc != pc) break;
			}
			if (found) {
				str=sstr;
				if (pc!=')') {
					pat=strchr(pat,')');
					if (!pat) return 0;
					pat++;
				}
				matches[nmatch++]=pstr;
				break;
			}
			while (*pat && *pat != '|' && *pat !=')') pat++;
			if (*pat != '|') return 0;
			pat++;
			wc=owc;
		}
	}
	c=da->res;
	n=0;
	while (*c) {
		int pos;
		char *d;
		wchar_t zn;
		if (*c != '%') {
			*db++=*c++;
			continue;
		}
		c++;
		if (isdigit(*c)) {
			pos=(*c)-'1';
			c++;
		}
		else pos=n++;
		if (pos<0 || pos >=nmatch) continue;
		d=matches[pos];
		for (;;) {
			zn=get_unichar(d,&d,0);
			if (!zn || zn=='|' || zn==')') break;
			if (zn<=0x7f) *db++=zn;
			else {
				int i;
				for (i=0;i<conf->rconf.nchar;i++) if (conf->rconf.wc[i]==zn) break;
				if (i<conf->rconf.nchar) *db++=conf->rconf.ch[i];
			}
		}
	}
	*db=0;
	*res=str;
	return 1;

}

static struct dumbtts_diction_letter *findmat_dic(struct dumbtts_diction_letter **dc,wchar_t wc,char *str)
{
	int n;
	struct dumbtts_diction_letter *dl;
	n=toflower(wc) & 31;
	if (!(dl=dc[n])) return NULL;
	for (;;) {
		n=get_unichar(str,&str,0);
		if (!n || !isfalnum(n)) return dl;
		n=toflower(n) & 31;
		if (!dl->nlet[n]) return dl;
		dl=dl->nlet[n];
	}
}


int get_dict(struct dumbtts_conf *conf,char **str,wchar_t wc,char *dictbuf)
{
	struct dumbtts_abbr *da;
	char *c;
	struct dumbtts_diction_letter *dl;
	if (wc < 0x7f && isdigit(wc)) {
		for (da=conf->dic;da;da=da->next) {
			if (dic_compare_word(da,*str,&c,wc,dictbuf,conf)) {
				*str=c;
				return 1;
			}
		}
		return 0;
	}
	dl=findmat_dic(conf->dics,wc,*str);
	while(dl) {
		for (da=dl->patterns;da;da=da->next) {
			if (dic_compare_word(da,*str,&c,wc,dictbuf,conf)) {
				*str=c;
				return 1;
			}
		}
		dl=dl->parent;
	}
	return 0;
}
