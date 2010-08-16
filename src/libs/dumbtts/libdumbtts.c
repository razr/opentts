/*
 * libdumbtts.c - dumb synthesizers helper library
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fctype.h"
#include "local.h"

#define addstr(str) do {if (outbuf && strlen(str)+pos < len) strcpy(outbuf+pos,str);pos+=strlen(str);} while(0)
#define addbuf(chr) do {if (outbuf && pos < len-1) outbuf[pos]=chr;pos++;} while(0)
#define endbuf do {if (outbuf && pos < len) {outbuf[pos]=0;return 0;};return pos+1;} while(0)

int roman(int wc,wchar_t *oc,char *str,char **ostr,struct dumbtts_conf *conf);

static struct {
	unsigned short flags;
	char letter;
	char second;
} char_descr[]={
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
{8,'a',0},{3,'a',0},{6,'a',0},{14,'a',0},{7,'a',0},{12,'a',0},{1,'a','e'},{5,'c',0},
{8,'e',0},{3,'e',0},{6,'e',0},{7,'e',0},{8,'i',0},{3,'i',0},{6,'i',0},{7,'i',0},
{0,0,0},{14,'n',0},{8,'o',0},{3,'o',0},{6,'o',0},{14,'o',0},{7,'o',0},{0,0,0},
{13,'o',0},{8,'u',0},{3,'u',0},{6,'u',0},{7,'u',0},{3,'y',0},{0,0,0},{0,0,0},
{8,'a',0},{3,'a',0},{6,'a',0},{14,'a',0},{7,'a',0},{12,'a',0},{1,'a','e'},{5,'c',0},
{8,'e',0},{3,'e',0},{6,'e',0},{7,'e',0},{8,'i',0},{3,'i',0},{6,'i',0},{7,'i',0},
{0,0,0},{14,'n',0},{8,'o',0},{3,'o',0},{6,'o',0},{14,'o',0},{7,'o',0},{0,0,0},
{13,'o',0},{8,'u',0},{3,'u',0},{6,'u',0},{7,'u',0},{3,'y',0},{0,0,0},{7,'y',0},
{10,'a',0},{10,'a',0},{4,'a',0},{4,'a',0},{11,'a',0},{11,'a',0},{3,'c',0},{3,'c',0},
{6,'c',0},{6,'c',0},{15,'c',0},{15,'c',0},{9,'c',0},{9,'c',0},{9,'d',0},{9,'d',0},
{13,'d',0},{13,'d',0},{10,'e',0},{10,'e',0},{4,'e',0},{4,'e',0},{15,'e',0},{15,'e',0},
{11,'e',0},{11,'e',0},{9,'e',0},{9,'e',0},{6,'g',0},{6,'g',0},{4,'g',0},{4,'g',0},
{15,'g',0},{15,'g',0},{5,'g',0},{5,'g',0},{6,'h',0},{6,'h',0},{13,'h',0},{13,'h',0},
{14,'i',0},{14,'i',0},{10,'i',0},{10,'i',0},{4,'i',0},{4,'i',0},{11,'i',0},{11,'i',0},
{15,'i',0},{0,0,0},{1,'i','j'},{1,'i','j'},{6,'j',0},{6,'j',0},{5,'k',0},{5,'k',0},
{0,0,0},{3,'l',0},{3,'l',0},{5,'l',0},{5,'l',0},{9,'l',0},{9,'l',0},{0,'l',0},
{0,'l',0},{13,'l',0},{13,'l',0},{3,'n',0},{3,'n',0},{5,'n',0},{5,'n',0},{9,'n',0},
{9,'n',0},{0,0,0},{0,0,0},{0,0,0},{10,'o',0},{10,'o',0},{4,'o',0},{4,'o',0},
{2,'o',0},{2,'o',0},{1,'o','e'},{1,'o','e'},{3,'r',0},{3,'r',0},{5,'r',0},{5,'r',0},
{9,'r',0},{9,'r',0},{3,'s',0},{3,'s',0},{6,'s',0},{6,'s',0},{5,'s',0},{5,'s',0},
{9,'s',0},{9,'s',0},{5,'t',0},{5,'t',0},{9,'t',0},{9,'t',0},{13,'t',0},{13,'t',0},
{14,'u',0},{14,'u',0},{10,'u',0},{10,'u',0},{4,'u',0},{4,'u',0},{12,'u',0},{12,'u',0},
{2,'u',0},{2,'u',0},{11,'u',0},{11,'u',0},{6,'w',0},{6,'w',0},{6,'y',0},{6,'y',0},
{7,'y',0},{3,'z',0},{3,'z',0},{15,'z',0},{15,'z',0},{9,'z',0},{9,'z',0},{0,0,0}};


int get_unichar(char  *str, char **ptr, int hMode)
{
	wchar_t wc;
	int n;

	if (hMode) {
		char *c;int base;
		c=(char *)str;
		if (*c=='&') c++;
		if (*c=='#') {
			c++;
			base=10;
			if (*c=='x' || *c=='X') {
				c++;
				base=16;
			}
			if (*c &&
				(((base == 10) && isdigit(*c)) ||
				((base == 16) && isxdigit(*c)))) {
				wc=strtol(c,&c,base);
				if (*c==';') {
					c++;
					if (ptr) *ptr=c;
					return wc;
				}
				if (hMode==2) {
					if (ptr) *ptr=c;
					return wc;
				}
			}
		}
	}
	wc=*str++ & 255;
	if ((wc & 0xe0)==0xc0) {
		wc &=0x1f;
		n=1;
	}
	else if ((wc & 0xf0)==0xe0) {
		wc &=0x0f;
		n=2;
	}
	else if ((wc & 0xf8)==0xf0) {
		wc &=0x07;
		n=3;
	}
	else if ((wc & 0xfc)==0xf8) {
		wc &=0x03;
		n=4;
	}
	else if ((wc & 0xfe)==0xfc) {
		wc &=0x01;
		n=5;
	}
	else {
		if (ptr) *ptr=str;
		return wc;
	}

	while (n--) {
		if ((*str & 0xc0) != 0x80) {
			wc='?';
			break;
		}
		wc=(wc << 6) | ((*str++) & 0x3f);
	}
	if (ptr) *ptr=str;
	return wc;
}

static int ustrlen(char  *c)
{
	int i;
	for (i=0;;i++) if (!get_unichar(c,&c,1)) return i;
}

void *allocMemBlock(struct dumbtts_conf *conf,int size,int flags)
{
	char *mb;
	if (!conf->memo || conf->memo->lastfree - conf->memo->firstfree < size) {
		struct memblock *memo;
		memo=malloc(sizeof(*memo));
		if (!memo) return NULL;
		memo->next=conf->memo;
		conf->memo=memo;
		memo->firstfree=0;
		memo->lastfree=MEMO_BLOCK_SIZE;
	}
	if (!flags) {
		conf->memo->lastfree-=size;
		mb=conf->memo->block+conf->memo->lastfree;
	}
	else {
		size=(size+7) & 0xfff8;
		mb=conf->memo->block+conf->memo->firstfree;
		conf->memo->firstfree+=size;
	}
	return mb;
}

char *strdupMemBlock(struct dumbtts_conf *conf,char *str)
{
	char *dst=allocMemBlock(conf,strlen(str)+1,0);
	if (dst) strcpy(dst,str);
	return dst;
}

static void add_keyname(struct dumbtts_conf *conf,char *key,char *trans)
{
	struct dumbtts_keyname *k;
	char *c;
	for (k=conf->keynames;k;k=k->next) if (!strcmp(k->key,key)) {
		if (!strcmp(k->trans,trans)) return;
		c=strdupMemBlock(conf,trans);
		if (c) k->trans=c;
		return;
	}
	k=allocMemBlock(conf,sizeof(*k),1);
	if (!k) return;
	if (!(k->key=strdupMemBlock(conf,key))) return;
	if (!(k->trans=strdupMemBlock(conf,trans))) return;
	k->next=conf->keynames;
	conf->keynames=k;
}

static struct dumbtts_charlist *find_conf_cl(struct dumbtts_conf *conf,wchar_t wc)
{
	struct dumbtts_charlist *cl;
	for (cl=conf->charlist;cl;cl=cl->next) if (cl->wc == wc) return cl;
	cl=allocMemBlock(conf,sizeof(*cl),1);
	if (!cl) return NULL;
	cl->next=conf->charlist;
	conf->charlist=cl;
	cl->wc=wc;
	cl->name=NULL;
	cl->prono=NULL;
	return cl;
}

static void add_charname(struct dumbtts_conf *conf,wchar_t wc,char *name)
{
	char *c=strdupMemBlock(conf,name);
	struct dumbtts_charlist *cl;
	if (!c) return;
	if (wc < 0x180) {
		conf->chartable[wc].name=c;
		return;
	}
	cl=find_conf_cl(conf,wc);
	if (cl) cl->name=c;
}

static void add_charpron(struct dumbtts_conf *conf,wchar_t wc,char *pron)
{
	char *c=strdupMemBlock(conf,pron);
	struct dumbtts_charlist *cl;
	if (!c) return;
	if (wc < 0x180) {
		conf->chartable[wc].prono=c;
		return;
	}
	cl=find_conf_cl(conf,wc);
	if (cl) cl->prono=c;

}

static void add_cyril(struct dumbtts_conf *conf,wchar_t wc,wchar_t nc,char *trans)
{
	struct dumbtts_cyrtrans *dc;
	if (trans) {
		if (!(trans=strdupMemBlock(conf,trans))) return;
	}
	dc=allocMemBlock(conf,sizeof(*dc),1);
	if (!dc) return;
	dc->c1=wc;
	dc->c2=nc;
	dc->translit=trans;
	dc->next=conf->cyrtrans;
	conf->cyrtrans=dc;
}

static void add_abbr(struct dumbtts_conf *conf,char *abbr,char *res)
{
	struct dumbtts_abbr *da;
	da=allocMemBlock(conf,sizeof(*da),1);
	if (!da) return;
	if (!(da->abbr=strdupMemBlock(conf,abbr))) return;
	if (!(da->res=strdupMemBlock(conf,res))) return;
	da->next=conf->abbrev;
	conf->abbrev=da;
}

char *local_conv(char *line,struct dumbtts_conf *conf)
{
	int i;
	char *dst,*s;
	wchar_t wc;
	while (*line && isspace(*line)) line++;
	if (!*line) return NULL;
	s=dst=line;
	while ((wc=get_unichar(line,&line,1))!= 0) {
		if (wc < 0x7f) {
			*s++=wc;
			continue;
		}
		for (i=0;i<conf->rconf.nchar;i++) if (conf->rconf.wc[i]==wc) break;
		if (i>=conf->rconf.nchar) return NULL;
		*s++=conf->rconf.ch[i];
	}
	*s=0;
	return dst;
}

char *line_trim(char *str)
{
	char *c,*d;
	while (*str && isspace(*str)) str++;
	for (c=d=str;*c;c++) if (!strchr(" \t\r\n",*c)) d=c+1;
	*d=0;
	return str;
}

char *dia_sym[14]={"ac2","acu","bre","ced","cir","dia","gra","car",
		   "mac","ogo","rin","str","til","dot"};

char *dia_names[14]={"double acute","acute","breve","cedilla",
			"circumflex","diaeresis","grave","caron",
			"macron","ogonek","ring","stroke",
			"tilde","dot"};

static int is_7bit(char *c)
{
	for (;*c;c++) if ((*c) & 0x80) return 0;
	return 1;
}

static void read_roman_line(struct dumbtts_conf *conf,char *line)
{
	char *par;int nr;
	if (!is_7bit(line)) return;
	par=line;
	while (*par && !isspace(*par)) par++;
	if (*par) *par++=0;
	while (*par && isspace(*par)) par++;
	if (!*par) return;
	if (!strcasecmp(line,"minlen")) {
		nr=strtol(par,&par,10);
		if (nr<1 || nr>10) return;
		conf->roman_minlen=nr;
		return;
	}
	if (!strcasecmp(line,"maxval")) {
		nr=strtol(par,&par,10);
		if (nr<10 || nr>5000) return;
		conf->roman_maxval=nr;
		return;
	}
	if (!strcasecmp(line,"ignore")) {
		if (!strncmp(par,"reset",5) && (!par[5] || isspace(par[5]))) {
			conf->roman_ignore=NULL;
			par+=5;
			while (*par && isspace(*par)) par++;
		}
		while (*par) {
			int v1,v2;
			struct dumbtts_intpair *pair;
			if (isdigit(*par)) {
				v1=strtol(par,&par,10);
				if (*par=='-') {
					par++;
					if (!*par || !isdigit(*par)) return;
					v2=strtol(par,&par,10);
				} else v2=v1;
			}
			else {
				wchar_t w1,w2;
				w1=*par++;
				w2=0;
				v1=roman(w1,&w2,par,NULL,NULL);
				v2=v1;
				while (*par && !isspace(*par)) par++;
			}
			if (*par && !isspace(*par)) return;
			if (!v1) return;
			pair=allocMemBlock(conf,sizeof(*pair),1);
			if (!pair) return;
			pair->next=conf->roman_ignore;
			conf->roman_ignore=pair;
			pair->v1=v1;
			pair->v2=v2;
			while (*par && isspace(*par)) par++;
		}
		return;
	}
}

static int read_line(struct dumbtts_conf *conf,FILE *f)
{
	char buf[1024];
	char *line,*c;
	int zn;wchar_t wc,nc;
	int i;

	if (!fgets(buf,256,f)) return 0;
	c=strstr(buf,"//");
	if (c) *c=0;
	line=line_trim(buf);
	if (!*line) return 1;
	if (*line=='[') {
		line++;
		if (!strncmp(line,"clis",4)) conf->rconf.mode=1;
		else if (!strncmp(line,"spel",4)) conf->rconf.mode=2;
		else if (!strncmp(line,"pron",4)) conf->rconf.mode=3;
		else if (!strncmp(line,"diac",4)) conf->rconf.mode=4;
		else if (!strncmp(line,"char",4)) conf->rconf.mode=5;
		else if (!strncmp(line,"keys",4)) conf->rconf.mode=6;
		else if (!strncmp(line,"cyri",4)) conf->rconf.mode=7;
		else if (!strncmp(line,"abbr",4)) conf->rconf.mode=8;
		else if (!strncmp(line,"roma",4)) conf->rconf.mode=9;
		else if (!strncmp(line,"dict",4)) conf->rconf.mode=10;
		else if (!strncmp(line,"form",4)) conf->rconf.mode=11;
		else conf->rconf.mode=0;
		return 1;
	}
	switch(conf->rconf.mode) {
		case 1: /* character list */

		if (*line=='x') {
			line++;
			zn=strtol(line,&line,16);
		}
		else zn=strtol(line,&line,10);
		if (zn<161 || zn >255) return 1;
		while (*line && isspace(*line)) line++;
		if (!*line) return 1;
		wc=get_unichar(line,&line,1);
		if (!wc) return 1;
		for (i=0;i<conf->rconf.nchar;i++) {
			if (conf->rconf.wc[i]==wc) {
				conf->rconf.wc[i]=zn;
				return 1;
			}
		}
		if (conf->rconf.nchar >= 128) return 1;
		conf->rconf.wc[conf->rconf.nchar]=wc;
		conf->rconf.ch[conf->rconf.nchar++]=zn;
		return 1;

		case 2: /* letter  spelling */
		case 5: /* character names */

		if (*line == '\\') {
			line++;
			if (!*line) return 1;
			wc=get_unichar(line,&line,0);
		}
		else wc=get_unichar(line,&line,2);
		if (!*line || !isspace(*line)) return 1;
		line=local_conv(line,conf);
		if (!line) return 1;
		add_charname(conf,wc,line);
		return 1;

		case 3: /* unicode letter pronunciation */
		if (*line == '\\') {
			line++;
			if (!*line) return 1;
			wc=get_unichar(line,&line,0);
		}
		else wc=get_unichar(line,&line,1);
		if (!*line || !isspace(*line)) return 1;
		line=local_conv(line,conf);
		if (!line) return 1;
		add_charpron(conf,wc,line);
		return 1;

		case 4: /* diacritics and special symbols */

		c=line;
		while (*c && !isspace(*c)) c++;
		if (!*c) return 1;
		*c++=0;
		c=local_conv(c,conf);
		if (!c) return 1;
		c=strdupMemBlock(conf,c);
		if (!strcmp(line,"cap")) conf->cap=c;
		else if (!strcmp(line,"ill")) conf->ill=c;
		else if (!strcmp(line,"sym")) conf->sym=c;
		else if (!strcmp(line,"lig")) conf->lig=c;
		else if (!strcmp(line,"let")) conf->let=c;
		else if (!strcmp(line,"caplet")) conf->caplet=c;
		else if (!strcmp(line,"caplig")) conf->caplig=c;
		else {
			int i;
			for (i=0;i<14;i++) if (!strcmp(dia_sym[i],line)) {
				conf->dia[i]=c;
				break;
			}
		}
		return 1;

		case 6: /* key name translation */

		c=line;
		while (*c && !isspace(*c)) c++;
		if (!*c) return 1;
		*c++=0;
		c=local_conv(c,conf);
		if (!c) return 1;
		add_keyname(conf,line,c);
		return 1;

		case 7: /* cyrillic transliteration */
		wc=get_unichar(line,&line,1);
		if (!*line || wc < 0x400 || wc >= 0x480) return 1;
		nc=0;
		if (!isspace(*line)) {
			nc=get_unichar(line,&line,1);
			if (nc < 0x400 || nc >= 0x480) return 1;
		}
		c=local_conv(line,conf);
		add_cyril(conf,wc,nc,c);
		return 1;

		case 8: /* abbreviations */
		c=line;
		while (*c && !isspace(*c)) c++;
		if (!*c) return 1;
		*c++=0;
		c=local_conv(c,conf);
		if (c) add_abbr(conf,line,c);
		return 1;

		case 9: /* roman numbers */
		read_roman_line(conf,line);
		return 1;

		case 10: /* dictionary */
		if (conf->flags & 1) return 1; /* ignore dict */
		c=line;
		while (*c && !isspace(*c)) c++;
		if (!*c) return 1;
		*c++=0;
		if (strcasecmp(line,"path")) return 1;
		while (*c && isspace(*c)) c++;
		if (*c) dumbtts_LoadDictionary(conf,c);
		return 1;

		case 11:
		read_format_line(conf,line);
		return 1;
	}
	return 1;

}

struct dumbtts_conf *dumbtts_TTSInitWithFlags(char *lang,int flags)
{
	FILE *f;
	char namebuf[256],*c;
	struct dumbtts_conf *conf;
	conf=malloc(sizeof(*conf));
	if (!conf) return NULL;
	memset(conf,0,sizeof(*conf));
	conf->flags = flags;
	conf->roman_minlen=2;
	conf->roman_maxval=2999;
	sprintf(namebuf,"/usr/share/dumbtts/%s.conf",lang);
	f=fopen(namebuf,"r");
	if (f) {
		conf->rconf.mode=0;
		while (read_line(conf,f));
		fclose(f);
	}
	sprintf(namebuf,"/etc/dumbtts/%s.conf",lang);
	f=fopen(namebuf,"r");
	if (f) {
		conf->rconf.mode=0;
		while (read_line(conf,f));
		fclose(f);
	}
	c=getenv("HOME");
	if (c) {
		sprintf(namebuf,"%s/.dumbtts_%s.conf",c,lang);
		f=fopen(namebuf,"r");
		if (f) {
			conf->rconf.mode=0;
			while(read_line(conf,f));
			fclose(f);
		}
	}
	return conf;

}

struct dumbtts_conf *dumbtts_TTSInit(char *lang)
{
	return dumbtts_TTSInitWithFlags(lang,0);
}

void dumbtts_TTSFree(struct dumbtts_conf *conf)
{
	struct memblock *mb;
	if (!conf) return;
	while ((mb=conf->memo) != NULL) {
		conf->memo=mb->next;
		free(mb);
	}
	free(conf);
}

int dumbtts_WCharString(struct dumbtts_conf *conf,wchar_t wc,char *outbuf,int len,int capMode,int *isCap)
{
	wchar_t owc;
	int cap=0,pos=0,lett;
	char *chname,nbuf[32];

	owc=wc;
	if (isfupper(wc)) {
		owc=toflower(wc);
		cap=1;
	}
	if (capMode == 1) {
		if (isCap) *isCap=cap;
		capMode=0;
	}
	else if (isCap) *isCap=0;
	chname=NULL;
	if (conf) {
		if (owc<0x180) chname=conf->chartable[owc].name;
		else {
			struct dumbtts_charlist *cl;
			for (cl=conf->charlist;cl;cl=cl->next) if (cl->wc == owc) {
				chname=cl->name;
				break;
			}
		}
	}
	if (chname) {
		if (cap && capMode) {
			if (conf && conf->cap) addstr(conf->cap);
			else addstr("capital");
			addbuf(' ');
		}
		addstr(chname);
		endbuf;
	}
	if (owc < 0x180 && char_descr[owc].letter) { /* diacritical */
		if (char_descr[owc].flags == 1) { /* ligature */
			if (cap && capMode) {
				if (conf) {
					if (conf->caplig) addstr(conf->caplig);
					else {
						if (conf->cap) addstr(conf->cap);
						else addstr("capital");
						addbuf(' ');
						if (conf->lig) addstr(conf->lig);
						else addstr("ligature");
					}
					addbuf(' ');
				}
				else addstr("capital ligature ");
			}
			else {
				if (conf && conf->lig) addstr(conf->lig);
				else addstr("ligature");
				addbuf(' ');
			}
			lett=char_descr[owc].letter;
			if (conf && conf->chartable[lett].name) addstr(conf->chartable[lett].name);
			else addbuf(lett);
			lett=char_descr[owc].second;
			if (lett) {
				addbuf(' ');
				if (conf && conf->chartable[lett].name) addstr(conf->chartable[lett].name);
				else addbuf(lett);
			}
			endbuf;
		}

		/* not ligature */
		if (cap && capMode) {
			if (conf && conf->cap) addstr(conf->cap);
			else addstr("capital");
			addbuf(' ');
		}
		lett=char_descr[owc].letter;
		if (conf && conf->chartable[lett].name) addstr(conf->chartable[lett].name);
		else addbuf(lett);
		if (char_descr[owc].flags) {
			addbuf(' ');
			if (conf && conf->dia[char_descr[owc].flags - 2]) {
				addstr(conf->dia[char_descr[owc].flags - 2]);
			}
			else {
				addstr(dia_names[char_descr[owc].flags - 2]);
			}
		}
		endbuf;
	}
	if (owc<0x7f && isalnum(owc)) {
		if (cap && capMode) {
			if (conf && conf->cap) addstr(conf->cap);
			else addstr("capital");
			addbuf(' ');
		}
		addbuf(owc);
		endbuf;
	}
	if (owc==' ') {
		addstr("space");
		endbuf;
	}
	if (isfalpha(owc)) {
		if (cap && capMode) {
			if (conf) {
				if (conf->caplet) addstr(conf->caplet);
				else {
					if (conf->cap) addstr(conf->cap);
					else addstr("capital");
					addbuf(' ');
					if (conf->let) addstr(conf->let);
					else addstr("letter");
				}
				addbuf(' ');
			}
			else addstr("capital letter ");
		}
		else {
			if (conf && conf->let) addstr(conf->let);
			else addstr("letter");
			addbuf(' ');
		}
	}
	else {
		if (conf && conf->sym) addstr(conf->sym);
		else addstr("symbol");
		addbuf(' ');
	}
	sprintf(nbuf,"%d",(int)wc);
	addstr(nbuf);
	endbuf;
}

int dumbtts_CharString(struct dumbtts_conf *conf,char *ch,char *outbuf,int len,int capMode,int *isCap)
{
	wchar_t wc;
	if (!strcmp(ch,"space")) wc=32;
	else wc=get_unichar(ch,NULL,0);
	if (wc) return dumbtts_WCharString(conf,wc,outbuf,len,capMode,isCap);
	return -1;
}

static char *dumbtts_translateKey(struct dumbtts_conf *conf,char *inbuf,int len,char *buf)
{
	int i;char *c;wchar_t wc;
	struct dumbtts_keyname *key;

	if (len>63) return (conf && conf->ill)?conf->ill:"illegal";
	memcpy(buf,inbuf,len);
	buf[len]=0;
	wc=get_unichar(buf,&c,1);
	if (!*c) {
		/* single character */
		i=dumbtts_WCharString(conf,wc,buf,64,0,NULL);
		if (i) return (conf && conf->ill)?conf->ill:"illegal";
		return buf;
	}
	if (conf) {
		for (key=conf->keynames;key;key=key->next) {
			if (!strcmp(key->key,buf)) return key->trans;
		}
	}
	return buf; /* untranslated */
}

int dumbtts_KeyString(struct dumbtts_conf *conf,char  *str,char *outbuf,int len,int capMode,int *isCap)
{
	char *c,*s;
	int pos;
	char buf[64];
	pos=0;
	if (ustrlen(str)==1) return dumbtts_CharString(conf,str,outbuf,len,capMode,isCap);
	if (isCap) *isCap=0;
	for (;;) {
		while (*str && strchr(" _\"",*str)) str++;
		if (!*str) break;
		c=strpbrk(str," _\"");
		if (!c) c=(char *)(str+strlen(str));
		s=dumbtts_translateKey(conf,str,c-str,buf);
		if(s) {
			if (pos) addbuf(' ');
			addstr(s);
		}
		str=c;
	}
	if (!pos) return -1;
	endbuf;
}

static char *dumbtts_pron(struct dumbtts_conf *conf,wchar_t wc,char *buf)
{
	wc=toflower(wc);
	if (wc >= 0x180) return NULL;
	if (wc < 0x7f && isalnum(wc)) {
		buf[0]=wc;
		buf[1]=0;
		return buf;
	}
	if (conf) {
		char *c;struct dumbtts_charlist *dc;
		if (wc < 0x180) c=conf->chartable[wc].prono;
		else {
			for (dc=conf->charlist;dc;dc=dc->next) if (dc->wc == wc) {
				c=dc->prono;
				break;
			}
		}
		if (c) return c;
	}
	if (wc>=0x180 || !char_descr[wc].letter) return NULL;
	buf[0]=char_descr[wc].letter;
	if (char_descr[wc].second) {
		buf[1]=char_descr[wc].second;
		buf[2]=0;
	}
	else buf[1]=0;
	return buf;
}

static int speak_punct(wchar_t wc,char *punct)
{
	wchar_t wx;
	if (!punct) return 0;
	while ((wx=get_unichar(punct,&punct,0)) != 0) if (wx==wc) return 1;
	return 0;
}

static int isfpunct(wchar_t wc)
{
	static wchar_t wcs[]={0xa1,0xab,0xbb,0xbf,0};
	static int pairs[][2]={
		{0x2010,0x2028},
		{0x2039,0x2047},
		{0x2329,0x232A},
		{0x3008,0x301F},
		{0,0}};
	int i;
	if (wc < 0x7f) {
		return !strchr("@\\#$%&^+*/<>~|=",wc);
	}
	for (i=0;wcs[i];i++) if (wcs[i] == wc) return 1;
	for (i=0;pairs[i][0];i++) if (pairs[i][0]<=wc && pairs[i][1]>wc) return 1;
	return 0;
}

static char *charname(struct dumbtts_conf *conf,wchar_t wc)
{
	struct dumbtts_charlist *cl;
	if (!conf) return NULL;
	if (wc<0x180) return conf->chartable[wc].name;
	for (cl=conf->charlist;cl;cl=cl->next) if (cl->wc == wc) {
		return cl->name;
	}
	return NULL;
}

int compare_word(char *word,char *str,char **outstr,wchar_t wc)
{
	wchar_t w1,w2;
	for (;;) {
		if (!*word) {
			if (*str && !isspace(*str)) return 0;
			*outstr=str;
			return 1;
			return 0;
		}
		if (wc) {
			w2=toflower(wc);
			wc=0;
		}
		else {
			w2=get_unichar(str,&str,0);
			if (!w2) return 0;
			w2=toflower(w2);
		}

		w1=get_unichar(word,&word,0);
		if (!w1) return 0;
		w1=toflower(w1);
		if (w1 == '_') {
			for (;;) {
				if (w2>=0x7f || !isspace(w2)) break;
				w2=get_unichar(str,&str,0);
				if (!w2) return 0;
			}
			wc=w2;
			continue;
		}

		if (w1 != w2) return 0;
	}
}

static char *abbrname(struct dumbtts_conf *conf,char **str,wchar_t wc)
{
	struct dumbtts_abbr *da;
	char *c;
	for (da=conf->abbrev;da;da=da->next) {
		if (compare_word(da->abbr,*str,&c,wc)) {
			*str=c;
			return da->res;
		}
	}
	return NULL;
}



static int legal_roman(struct dumbtts_conf *conf,int len,int v)
{
	struct dumbtts_intpair *ip;
	if (len < conf->roman_minlen) return 0;
	if (v > conf->roman_maxval) return 0;
	for (ip=conf->roman_ignore;ip;ip=ip->next) {
		if (v>=ip->v1 && v <=ip->v2) return 0;
	}
	return 1;
}

int roman(int wc,wchar_t *oc,char *str,char **ostr,struct dumbtts_conf *conf)
{
	int goc=0;
	int val=0;
	int chr,i;
	int len=0;

	int nchar()
	{
		if (wc) return wc;
		if (*oc && !goc) return *oc;
		return *str;
		return 0;
	}
	int gchar()
	{
		int n;
		len++;
		if (wc) {
			n=wc;
			wc=0;
			return n;
		}
		if (*oc && !goc) {
			goc=1;
			return *oc;
		}
		if (*str) return *str++;
		return 0;
	}
	while(nchar() == 'M' && val<4000) {
		val+=1000;
		gchar();
	}
	if(nchar()=='C') {
		gchar();
		if (nchar()=='M') {
			gchar();
			val+=900;
		}
		else if (nchar()=='D') {
			gchar();
			val+=400;
		}
		else {
			val+=100;
			for (i=0;i<2;i++) {
				if (nchar()=='C') {
					val+=100;
					gchar();
				}
				else break;
			}
		}
	}
	else if (nchar() == 'D') {
		val+=500;
		gchar();
		for (i=0;i<3;i++) {
			if (nchar()=='C') {
				val+=100;
				gchar();
			}
			else break;
		}
	}
	/* decimals */
	if (nchar()=='X') {
		gchar();
		if (nchar()=='C') {
			val+=90;
			gchar();
		}
		else if (nchar()=='L') {
			val+=40;
			gchar();
		}
		else {
			val+=10;
			for (i=0;i<2;i++) {
				if (nchar()=='X') {
					gchar();
					val+=10;
				}
				else break;
			}
		}
	}
	else if (nchar()=='L') {
		val+=50;
		gchar();
		for (i=0;i<3;i++) {
			if (nchar()=='X') {
				gchar();
				val+=10;
			}
			else break;
		}
	}
	/* units */
	if (nchar()=='I') {
		gchar();
		if (nchar()=='X') {
			gchar();
			val+=9;
		}
		else if (nchar()=='V') {
			gchar();
			val+=4;
		}
		else {
			val++;
			for (i=0;i<2;i++) {
				if (nchar()=='I') {
					gchar();
					val++;
				}
				else break;
			}
		}
	}
	else if (nchar()=='V') {
		gchar();
		val+=5;
		for (i=0;i<3;i++) {
			if (nchar()=='I') {
				gchar();
				val++;
			}
			else break;
		}
	}
	if (!val) return 0;
	if (conf && !legal_roman(conf,len,val)) return 0;
	if (!goc && *oc) chr=*oc;
	else if (*str) chr=get_unichar(str,NULL,0);
	else chr=0;
	if (chr && isfalnum(chr)) return 0;
	if (goc) *oc=0;
	if (ostr) *ostr=str;
	return val;
}

static int check_word_type(struct dumbtts_conf *conf,wchar_t wc,char *str)
{
	int llet,hlet,dit,wlen;

	for (llet=hlet=dit=wlen=0;wc;wc=get_unichar(str,&str,0),wlen++) {
		if (!wc || !isfalnum(wc)) break;
		if (wc>=0x180) break;
		if (isfupper(wc)) hlet=1;
		else if (isflower(wc)) llet=1;
		else if (isfdigit(wc)) dit=1;
	}
	if (wlen<2) return 0;
	if (!(conf->flags & 2) && hlet && !llet) return 1;
	if (dit) return 1;
	return 0;
}

int dumbtts_GetString(
	struct dumbtts_conf *conf,
	char *str,
	char *outbuf,
	int len,
	int punctMode,
	char *punctChars,
	char *punctLeave)
{
	int pos=0;
	int blank=0;
	wchar_t wc,oc;
	char *c,nbuf[32];
	int speak_char;
	oc=0;
	while (oc || *str) {
		if (oc) {
			wc=oc;
			oc=0;
		}
		else wc=get_unichar(str,&str,0);
		if (!wc) break;
		if (wc == 173) continue; /* soft hyphen */
		if (isfspace(wc)) {
			if (pos) blank=1;
			continue;
		}
		if (isfalnum(wc)) {
			if (blank || !pos) {
				char *s;
				int rom;
				char dictbuf[1024];
				struct recog_param rparams[16];
				int rcount;
				char *fmname;
				if (blank) addbuf(' ');
				blank=0;
				s=abbrname(conf,&str,wc);
				if (s) {
					addstr(s);
					continue;
				}
				if (get_dict(conf,&str,wc,dictbuf)) {
					addstr(dictbuf);
					continue;
				}
				if ((fmname=do_recognize(conf,wc,&str,rparams,&rcount))) {

					do_format(conf,fmname,rparams,rcount,dictbuf);
					addstr(dictbuf);
					continue;
				}
				if (do_units(conf,wc,&str,dictbuf)) {
					addstr(dictbuf);
					continue;
				}
				rom=roman(wc,&oc,str,&str,conf);
				if (rom) {
					char buf[8];
					sprintf(buf,"%d",rom);
					addstr(buf);
					continue;
				}
				/* configuration check! */
				if (check_word_type(conf,wc,str)) {
					int lastlet=2;
					for(;wc;) {
						char *cn;
						if (!isfalnum(wc)) break;
						if (isfdigit(wc)) {
							//if (lastlet==1) addbuf(' ');
							if (lastlet != 2) addbuf(' ');
							lastlet=0;
							for (;;) {
								addbuf(wc);
								wc=get_unichar(str,&cn,0);
								if (wc && isfalnum(wc)) str=cn;
								if (!wc || !isfdigit(wc)) break;
							}
							continue;
						}
						dumbtts_WCharString(conf,wc,dictbuf,1024,0,NULL);
						wc=get_unichar(str,&cn,0);
						//if (lastlet==0) addbuf(' ');
						if (lastlet!=2) addbuf(' ');
						lastlet=1;
						//if (!isfalnum(wc)) addstr("~!");
						//else str=cn;
						if (isfalnum(wc)) str=cn;
						//addstr("~'");
						addstr(dictbuf);
					}
					continue;

				}
			}
			if (wc>=0x400 && wc<0x480 && conf && conf->cyrtrans) {
				/* simplified cyrillic translation */
				int u1,u2;
				struct dumbtts_cyrtrans *dc;
				c=NULL;
				u1=toflower(wc);
				if (!oc && *str) oc=get_unichar(str,&str,0);
				u2=oc?toflower(oc):0;
				for (dc=conf->cyrtrans;dc;dc=dc->next) {
					if (dc->c1 != u1) continue;
					if (!dc->c2) break;
					if (dc->c2 == u2) break;
				}
				if (dc) {
					c=dc->translit;
					if (dc->c2) oc=0;
				}
				if (c) addstr(c);
				continue;
			}
			c=dumbtts_pron(conf,wc,nbuf);
			if (c) {
				addstr(c);
				continue;
			}

			/* unknown letter? */

			if (pos) addbuf(' ');
			if (conf && conf->let) addstr(conf->let);
			else addstr("letter");
			sprintf(nbuf," %d",(int)wc);
			addstr(nbuf);
			blank=1;
			continue;
		}
		speak_char=0;
		if (!isfpunct(wc) ||
			punctMode == 2 ||
			(punctMode == 1 && speak_punct(wc,punctChars))) {
				speak_char=1;
		}
		if (speak_char) {
			char *chname=charname(conf,wc);
			if (pos) addbuf(' ');
			if (chname) {
				addstr(chname);
			}
			else {
				if (conf && conf->sym) addstr(conf->sym);
				else addstr("symbol");
				sprintf(nbuf," %d",(int)wc);
				addstr(nbuf);
			}
			if (wc< 0x7f && punctLeave && strchr(punctLeave,wc)) {
				if (!oc && *str) oc=get_unichar(str,&str,0);
				if (!oc || isfspace(oc)) {
					addbuf(wc);
					blank=0;
				}
				else blank=1;
			}
			else blank=2;
			continue;
		}
		if (wc< 0x7f && punctLeave && strchr(punctLeave,wc)) {
			if (blank == 1) addbuf(' ');
			addbuf(wc);
			blank=0;
			continue;
		}
		/* special characters sequence */

		if (wc == '-' || wc == 0x2013 || wc == 0x2014) {
			if (blank == 0) {
				blank=3;
				continue;
			}
			if (!oc && *str) oc=get_unichar(str,&str,0);
			if (oc && isfspace(oc)) {
				blank=0;
				addbuf(',');
			}
			continue;
		}
		if (wc == '(') {
			if (blank == 0) continue;
			if (!oc && *str) oc=get_unichar(str,&str,0);
			if (!oc) continue;
			if (isfspace(oc) || isfalnum(oc)) {
				addbuf(',');
			}
			continue;
		}
		if (wc == ')') {
			if (!pos) continue;
			if (!oc && *str) oc=get_unichar(str,&str,0);
			if (!oc) continue;
			if (isfspace(oc)) {
				addbuf(',');
				blank=1;
			}
			continue;
		}
		blank=3;
	}
	endbuf;
}

