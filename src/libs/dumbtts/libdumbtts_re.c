/*
 * libdumbtts_re.c - dumb synthesizers helper library
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
 * License along with libdumbtts.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "local.h"

static int is_good_recog(char *c)
{
	wchar_t wc;
	while (*c) {
		wc=get_unichar(c,&c,0);
		if (!wc) return 0;
		if (wc == '"') {
			for (;;) {
				wc=get_unichar(c,&c,0);
				if (!wc) return 0;
				if (wc=='"') break;
				if (wc=='\\') {
					wc=get_unichar(c,&c,0);
					if (!wc) return 0;
				}
			}
			continue;
		}
		if (isfspace(wc)) continue;
		if (wc>=0x7f) return 0;
		if (isalpha(wc)) {
			if (*c++!='{') return 0;
			if (*c=='c') {
				c++;
				if (!isalnum(*c)) return 0;
				while (*c && isalnum(*c)) c++;
				if (*c++!='}') return 0;
				continue;
			}
			if (isdigit(*c)) c++;
			if (*c!='r' && *c!='d' && *c!='x') return 0;
			c++;
			if (isdigit(*c)) {
				c++;
				while(*c && isdigit(*c)) c++;
				if (*c++!='-') return 0;
				if (!*c || !isdigit(*c)) return 0;
				c++;
				while(*c && isdigit(*c)) c++;
			}
			if (*c++!='}') return 0;
			continue;
		}
		if (!strchr(":,.-_/",wc)) return 0;
	}
	return 1;
}

static int is_good_sayas(char *c)
{
	while (*c) {
		if (isspace(*c)) {
			c++;continue;
		}
		if (*c=='"') {
			c++;
			while (*c) {
				if (*c=='"') {
					c++;
					break;
				}
				if (*c=='\\') {
					c++;
				}
				if (!*c) return 0;
				c++;
			}
			continue;
		}
		if (!isalpha(*c++)) return 0;
		if (*c=='{') {
			c++;
			while (*c && isalnum(*c)) c++;
			if (*c++!='}') return 0;
		}
	}
	return 1;

}

static struct dumbtts_expr *read_rc_expr(struct dumbtts_conf *conf,char **str,int level);
static struct dumbtts_expr *read_rc_item(struct dumbtts_conf *conf,char **str);
static struct dumbtts_expr *alloc_expr(struct dumbtts_conf *conf)
{
	return allocMemBlock(conf,sizeof(struct dumbtts_expr),1);
}

void read_format_line(struct dumbtts_conf *conf,char *line)
{
	char *c;
	c=line;
	while (*c && !isspace(*c)) c++;
	if (!*c) return;
	*c++=0;
	while (*c && isspace(*c)) c++;
	if (!*c) return;
	if (!strcmp(line,"sayas")) {
		char *fname=c;
		struct dumbtts_abbr *sy;
		while (c && !isspace(*c)) c++;
		if (!*c) return;
		*c++=0;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		c=local_conv(c,conf);if (!c) return;
		if (!is_good_sayas(c)) return;
		if (!(c=strdupMemBlock(conf,c))) return;
		for (sy=conf->sayas;sy;sy=sy->next) if (!strcmp(sy->abbr,fname)) break;
		if (!sy) {
			if (!(sy=allocMemBlock(conf,sizeof(*sy),1))) return;
			if (!(sy->abbr=strdupMemBlock(conf,fname))) return;
			sy->next=conf->sayas;
			conf->sayas=sy;
		}
		sy->res=c;
		return;
	}
	if (!strcmp(line,"recognize")) {
		char *fname=c;
		struct dumbtts_abbr *sy;
		while (c && !isspace(*c)) c++;
		if (!*c) return;
		*c++=0;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		if (!is_good_recog(c)) return;
		if (!(c=strdupMemBlock(conf,c))) return;
		if (!(sy=allocMemBlock(conf,sizeof(*sy),1))) return;
		if (!(sy->abbr=strdupMemBlock(conf,fname))) return;
		sy->next=conf->recog;
		conf->recog=sy;
		sy->res=c;
		return;
	}
	if (!strcmp(line,"format")) {
		char *fname=c;
		int is_dflt=0;
		int val;
		struct dumbtts_format *df;
		struct dumbtts_format_item *dfi;
		while (c && !isspace(*c)) c++;
		if (!*c) return;
		*c++=0;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		if (!strncmp(c,"default",7)) {
			is_dflt=1;
			c+=7;
		}
		else {
			if (!isdigit(*c)) return;
			val=strtol(c,&c,10);
		}
		if (!*c || !isspace(*c)) return;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		c=local_conv(c,conf);if (!c) return;
		for (df=conf->formats;df;df=df->next) if (!strcmp(df->name,fname)) break;
		if (!df) {
			df=allocMemBlock(conf,sizeof(*df),1);
			if (!df) return;
			if (!(df->name=strdupMemBlock(conf,fname))) return;
			df->next=conf->formats;
			conf->formats=df;
			df->dfi=NULL;
			df->deflt=NULL;
		}
		c=strdupMemBlock(conf,c);
		if (!c) return;
		if (is_dflt) {
			df->deflt=c;
		}
		else {
			for (dfi=df->dfi;dfi;dfi=dfi->next) if (dfi->value==val) break;
			if (!dfi) {
				dfi=allocMemBlock(conf,sizeof(*dfi),1);
				if (!dfi) return;
				dfi->value=val;
				dfi->format=c;
				dfi->next=df->dfi;
				df->dfi=dfi;
			}
			else {
				dfi->format=c;
			}
		}
		return;
	}
	if (!strcmp(line,"choice")) {
		char *fname=c;
		int val;
		struct dumbtts_choice *df;
		struct dumbtts_format_item *dfi;
		while (c && !isspace(*c)) c++;
		if (!*c) return;
		*c++=0;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		if (!isdigit(*c)) return;
		val=strtol(c,&c,10);
		if (!*c || !isspace(*c)) return;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		for (df=conf->choices;df;df=df->next) if (!strcmp(df->name,fname)) break;
		if (!df) {
			df=allocMemBlock(conf,sizeof(*df),1);
			if (!df) return;
			if (!(df->name=strdupMemBlock(conf,fname))) return;
			df->next=conf->choices;
			conf->choices=df;
			df->dfi=NULL;
		}
		c=strdupMemBlock(conf,c);
		if (!c) return;
		dfi=allocMemBlock(conf,sizeof(*dfi),1);
		if (!dfi) return;
		dfi->value=val;
		dfi->format=c;
		dfi->next=df->dfi;
		df->dfi=dfi;
		return;
	}
	if (!strcmp(line,"form")) {
		char *stype=c;
		int type;
		int forma;
		struct dumbtts_expr *e;
		struct dumbtts_formex *fe;

		while (*c && !isspace(*c)) c++;
		if (!*c) return;
		*c++=0;
		if (!strcmp(stype,"int")) type=0;
		else if (!strcmp(stype,"float")) type=1;
		else return;
		while (*c && isspace(*c)) c++;
		if (!*c || !isdigit(*c)) return;
		forma=strtol(c,&c,10);
		if (!*c || !isspace(*c)) return;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		if (!strcmp(c,"default")) {
			conf->defforms[type]=forma;
			return;
		}
		e=read_rc_expr(conf,&c,4);
		if (!e || *c) return;
		fe=allocMemBlock(conf,sizeof(*fe),1);
		fe->type=type;
		fe->forma=forma;
		fe->expr=e;
		fe->next=conf->formex;
		conf->formex=fe;
		return;
	}
	if (!strcmp(line,"unit")) {
		struct dumbtts_unit *u;
		char *uname=c;
		while (*c && !isspace(*c)) c++;
		if (!*c) return;
		*c++=0;
		while (*c && isspace(*c)) c++;
		if (!*c) return;
		c=local_conv(c,conf);if (!c) return;
		if (!(u=allocMemBlock(conf,sizeof(*u),1))) return;
		if (!(u->unit=strdupMemBlock(conf,uname))) return;
		if (!(u->result=strdupMemBlock(conf,c))) return;
		u->next=conf->units;
		conf->units=u;
		return;
	}
}


static int do_recognize_item(struct dumbtts_conf *conf,char *patr,int wnc,char **str,struct recog_param *rp,int *rcount)
{
	char *c,*d;
	wchar_t wc,pc,zero;
	int pname,ndits,range1,range2,mode,val;
	int chrget()
	{
		if (wnc) {
			int n=wnc;
			wnc=0;
			return n;
		}
		return get_unichar(c,&c,0);
	}
	int chrgot()
	{
		if (wnc) {
			int n=wnc;
			wnc=0;
			d=c;
			return n;
		}
		return get_unichar(c,&d,0);
	}
	int nchar()
	{
		if (wnc) return wnc;
		return get_unichar(c,NULL,0);
	}
	int chcom(char *a,char *b)
	{
		while (*a) if (*a++!=*b++) return 0;
		if (*b != '}') return 0;
		return 1;
	}
	int cmwrd(char *fmat)
	{
		char *dx=c;
		wchar_t wxc=wnc;
		wchar_t px,sx;
		for (;*fmat;) {
			px=get_unichar(fmat,&fmat,0);
			if (!px) return 0;
			if (wxc) {
				sx=wxc;
				wxc=0;
			}
			else {
				sx=get_unichar(dx,&dx,0);
			}
			if (!sx) return 0;
			if (isfupper(px)) {
				if (sx != px) return 0;
			}
			else {
				if (toflower(sx)!=px) return 0;
			}
		}
		sx=get_unichar(dx,NULL,0);
		if (sx && isfalnum(sx)) return 0;
		c=dx;
		return 1;
	}

	if (rcount) *rcount=0;
	c=*str;
	while (*patr) {
		pc=get_unichar(patr,&patr,0);
		if (!pc) return 0;
		if (isfspace(pc)) {
			wc=chrget();
			if (!wc) return 0;
			if (!isfspace(wc)) return 0;
			for (;;) {
				wc=chrgot();
				if (!wc) return 0;
				if (!isfspace(wc)) break;
				c=d;
			}
			continue;
		}
		if (pc==':' || pc=='.' || pc=='/' || pc==',') {
			wc=chrget();
			if (pc != wc) return 0;
			continue;
		}
		if (pc=='_') { /* optional space */
			for (;;) {
				wc=chrgot();
				if (!wc) return 0;
				if (!isfspace(wc)) break;
				c=d;
			}
			continue;
		}
		if (pc=='-') { /* any dash */
			wc=chrget();
			if (wc != '-') return 0;
			continue;
		}
		if (pc=='"') {
			for (;;) {
				pc=get_unichar(patr,&patr,0);
				if (!pc) return 0;
				if (pc=='"') break;
				if (pc=='\\') {
					pc=get_unichar(patr,&patr,0);
					if (!pc) return 0;
				}
				wc=chrget();
				if (!wc) return 0;
				if (isflower(pc)) {
					if (pc != toflower(wc)) return 0;
				}
				else {
					if (pc != wc) return 0;
				}
			}
			continue;
		}
		if (pc<'a' || pc>'z') return 0;
		pname=pc;
		ndits=0;
		range1=0;
		range2=0;
		if (*patr++!='{') return 0;
		if (*patr=='c') {
			struct dumbtts_choice *dc;
			struct dumbtts_format_item *dfi;
			patr++;
			for (dc=conf->choices;dc;dc=dc->next) if (chcom(dc->name,patr)) break;
			if (!dc) return 0;
			patr=strchr(patr,'}');
			if (!patr) return 0;
			patr++;
			for (dfi=dc->dfi;dfi;dfi=dfi->next) if (cmwrd(dfi->format)) break;
			if (!dfi) return 0;

			if (rp && rcount) {
				rp[*rcount].name=pname;
				rp[(*rcount)++].value=dfi->value;
			}
			continue;
		}


		if (isdigit(*patr)) ndits=(*patr++)-'0';
		if (*patr=='d') mode=0;
		else if (*patr=='r') mode=1;
		else if (*patr=='x') mode=2;
		else return 0;

		patr++;

		if (isdigit(*patr)) {
			range1=strtol(patr,&patr,10);
			if(*patr++!='-') return 0;
			if (!*patr || !isdigit(*patr)) return 0;
			range2=strtol(patr,&patr,10);
		}
		if (*patr++!='}') return 0;
		switch(mode) {
			case 0: /* decimal */
			if (ndits) {
				int i;
				if (wnc) {
					if (wnc<'0' || wnc>'9') return 0;
					ndits--;
				}
				for (i=0;i<ndits;i++) if (!c[i] || !isdigit(c[i])) return 0;
				if (c[i] && isdigit(c[i])) return 0;
			}
			else {
				if (wnc) {
					if (wnc<'0' || wnc >'9') return 0;
				}
				else {
					if (!*c || !isdigit(*c)) return 0;
				}
			}
			if (wnc) {
				val=wnc-'0';
				wnc=0;
				for (;*c && isdigit(*c);c++) {
					val=10*val+(*c)-'0';
				}
			}
			else val=strtol(c,&c,10);
			break;

			case 2: /* hexadecimal */
			if (ndits) {
				int i;
				if (wnc) {
					if (wnc>0x7f || !isxdigit(wnc)) return 0;
					ndits--;
				}
				for (i=0;i<ndits;i++) if (!c[i] || !isxdigit(c[i])) return 0;
				if (c[i] && isxdigit(c[i])) return 0;
			}
			else {
				if (wnc) {
					if (wnc>0x7f || !isxdigit(wnc)) return 0;
				}
				else {
					if (!*c || !isxdigit(*c)) return 0;
				}
			}
			val=strtol(c,&c,16);
			break;

			default: /* roman */
			pc=chrget();
			if (!pc) return 0;
			zero=0;
			val=roman(pc,&zero,c,&c,NULL);
			break;
		}
		if (val<range1) return 0;
		if (range2 && val>range2) return 0;
		if (rp && rcount) {
			rp[*rcount].name=pname;
			rp[(*rcount)++].value=val;
		}
	}
	if (*c=='.' || *c==':') {
		d=c+1;
		if (*d) {
			pc=get_unichar(d,NULL,0);
			if (pc && !isfspace(pc)) return 0;
		}
	}
	else {
		pc=nchar();
		if (pc && isfalnum(pc)) return 0;
	}
	*str=c;
	return 1;

}

char *do_recognize(struct dumbtts_conf *conf,wchar_t wc,char **str,struct recog_param *rp,int *rcount)
{
	struct dumbtts_abbr *rc;
	for (rc=conf->recog;rc;rc=rc->next) {
		if (do_recognize_item(conf,rc->res,wc,str,rp,rcount)) return rc->abbr;
	}
	return NULL;
}

void do_format(struct dumbtts_conf *conf,char *fname,struct recog_param *rp,int rcount,char *outbuf)
{
	struct dumbtts_abbr *df;
	char *fmt;
	for (df=conf->sayas;df;df=df->next) if (!strcmp(df->abbr,fname)) break;

	if (!df) {
		int i;
		for (i=0;i<rcount;i++) {
			if (i) *outbuf++=' ';
			sprintf(outbuf,"%d",rp[i].value);
			while (*outbuf) outbuf++;
		}
		return;
	}
	fmt=df->res;
	*outbuf=0;
	while (*fmt) {
		int val,i;char *fstr;
		if (isspace(*fmt)) {
			while (*fmt && isspace(*fmt)) fmt++;
			if (!*fmt) break;
			*outbuf++=' ';
			continue;
		}
		if (*fmt=='"') {
			fmt++;
			while(*fmt) {
				if (*fmt=='"') break;
				if (*fmt=='\\') {
					fmt++;
					if (!*fmt) break;
				}
				*outbuf++=*fmt++;
			}
			fmt++;
			continue;
		}
		if (!isalpha(*fmt)) break;
		val=0;
		for (i=0;i<rcount;i++) if (*fmt == rp[i].name) {
			val=rp[i].value;
			break;
		}
		fstr="%d";
		fmt++;
		if (*fmt=='{') {
			char fbuf[16],*c;
			struct dumbtts_format *dfo;
			struct dumbtts_format_item *dfi;
			fmt++;
			for (c=fbuf;*fmt && *fmt!='}';fmt++) *c++=*fmt;
			if (*fmt) fmt++;
			*c=0;
			for (dfo=conf->formats;dfo;dfo=dfo->next) if (!strcmp(dfo->name,fbuf)) break;
			if (dfo) {
				if (dfo->deflt) fstr=dfo->deflt;
				for (dfi=dfo->dfi;dfi;dfi=dfi->next) if (dfi->value == val) {
					fstr=dfi->format;
					break;
				}
			}
		}
		sprintf(outbuf,fstr,val);
		while(*outbuf) outbuf++;
	}
	*outbuf=0;

}

/* units

At now only integer values are implemented!

*/

#define PFT_INT	1
#define PFT_INTVAR 2
#define PFT_FLT 3
#define PFT_FLTVAR 4
#define PFT_EXPR 5


#define PFO_ADD 1
#define PFO_SUB 2
#define PFO_DIV 3
#define PFO_MUL 4
#define PFO_MOD 5
#define PFO_EQU 6
#define PFO_NE 7
#define PFO_GT 8
#define PFO_GE 9
#define PFO_LT 10
#define PFO_LE 11
#define PFO_AND 12
#define PFO_OR 13
#define PFO_NOT 14
#define PFO_NEG 15

static char *rc_opstrs[]={
	"!-","+-","*/%","<>=","&|"};

static int rc_is_operator(char **str,int level)
{
	if (!**str) return 0;
	return strchr(rc_opstrs[level],**str)?1:0;
}

static int rc_get_operator(char **str)
{
	char znak;
	int i;
	static char *opstr=" +-/*%=_>_<_&|";

	znak=*(*str)++;
	if (znak=='!') {
		if (**str == '=') {
			(*str)++;
			return PFO_NE;
		}
		return 0;
	}
	else if (znak=='>') {
		if (**str == '=') {
			(*str)++;
			return PFO_GE;
		}
	}
	else if (znak=='<') {
		if (**str == '=') {
			(*str)++;
			return PFO_LE;
		}
	}
	for (i=1;opstr[i];i++) if (opstr[i]==znak) return i;
	return 0;
}

#define desp while(**str && isspace(**str)) (*str)++


static struct dumbtts_expr *read_rc_item(struct dumbtts_conf *conf,char **str)
{
	struct dumbtts_expr *expr;
	desp;
	if (!**str) return NULL;
	if (**str=='(') {
		(*str)++;
		desp;
		expr=read_rc_expr(conf,str,4);
		if (!expr) return NULL;
		if (**str !=')') return NULL;
		(*str)++;
		desp;
		return expr;
	}
	if (**str=='!') {
		(*str)++;
		desp;
		expr=alloc_expr(conf);
		expr->type=PFT_EXPR;
		expr->operator=PFO_NEG;
		expr->u.ops[0]=read_rc_item(conf,str);
		if (!expr->u.ops[0]) return NULL;
		return expr;
	}
	if (**str == '-') {
		(*str)++;
		if (!**str) return NULL;
		if (isdigit(**str)) {
			expr=read_rc_item(conf,str);
			if (!expr) return NULL;
			if (expr->type == PFT_INT) expr->u.ival=-expr->u.ival;
			else if (expr->type == PFT_FLT) expr->u.fval=-expr->u.fval;
			else return NULL;
			return expr;
		}
		desp;
		expr=alloc_expr(conf);
		expr->type=PFT_EXPR;
		expr->operator=PFO_NEG;
		expr->u.ops[0]=read_rc_item(conf,str);
		if (!expr->u.ops[0]) return NULL;
		return expr;
	}
	if (isdigit(**str)) {
		char *c=*str;
		expr=alloc_expr(conf);
		while (*c && isdigit(*c)) c++;
		if (*c!='.') {
			expr->type=PFT_INT;
			expr->u.ival=strtol(*str,str,10);
			desp;
			return expr;
		}
		expr->type=PFT_FLT;
		expr->u.fval=strtod(*str,str);
		desp;
		return expr;
	}
	if (**str=='d') {
		(*str)++;
		desp;
		expr=alloc_expr(conf);
		expr->type=PFT_INTVAR;
		return expr;
	}
	if (**str=='f') {
		(*str)++;
		desp;
		expr=alloc_expr(conf);
		expr->type=PFT_FLTVAR;
		return expr;
	}
	return NULL;
}

static struct dumbtts_expr *read_rc_expr(struct dumbtts_conf *conf,char **str,int level)
{
	struct dumbtts_expr *expr,*eit;

	if (!level) return read_rc_item(conf,str);

	expr=read_rc_expr(conf,str,level-1);
	if (!expr) return NULL;

	for (;;) {
		if (!rc_is_operator(str,level)) return expr;
		eit=alloc_expr(conf);
		eit->type=PFT_EXPR;
		eit->operator=rc_get_operator(str);
		eit->u.ops[0]=expr;
		expr=eit;
		if (!(eit->u.ops[1]=read_rc_expr(conf,str,level-1))) return NULL;
	}
}

#undef desp


/* emiter */

static int unit_compare(char **str,char *patr)
{
	wchar_t wu,wp;
	char *c;
	c=*str;
	while (*patr) {
		wp=get_unichar(patr,&patr,0);
		if (!wp) return 0;
		wu=get_unichar(c,&c,0);
		if (wp !=wu) return 0;
	}
	if (!*c) {
		*str=c;
		return 1;
	}
	wu=get_unichar(c,NULL,0);
	if (!isfalnum(wu)) {
		*str=c;
		return 1;
	}
	return 0;
}

#define di(e) dumbtts_compute_iexpr(e,v)

int dumbtts_compute_iexpr(struct dumbtts_expr *e,int v)
{
	int i;
	switch(e->type) {
		case PFT_INT: return e->u.ival;
		case PFT_INTVAR: return v;
		case PFT_EXPR:

		switch(e->operator) {
			case PFO_ADD: return di(e->u.ops[0])+di(e->u.ops[1]);
			case PFO_SUB: return di(e->u.ops[0])-di(e->u.ops[1]);
			case PFO_DIV:

			i=di(e->u.ops[1]);
			if (!i) return 0;
			return di(e->u.ops[0])/i;

			case PFO_MUL: return di(e->u.ops[0])*di(e->u.ops[1]);
			case PFO_MOD:
			i=di(e->u.ops[1]);
			if (!i) return 0;
			return di(e->u.ops[0])%i;

			case PFO_EQU: return (di(e->u.ops[0])==di(e->u.ops[1]))?1:0;
			case PFO_NE: return (di(e->u.ops[0])!=di(e->u.ops[1]))?1:0;
			case PFO_GT: return (di(e->u.ops[0])>di(e->u.ops[1]))?1:0;
			case PFO_GE: return (di(e->u.ops[0])>=di(e->u.ops[1]))?1:0;
			case PFO_LT: return (di(e->u.ops[0])<di(e->u.ops[1]))?1:0;
			case PFO_LE: return (di(e->u.ops[0])<=di(e->u.ops[1]))?1:0;
			case PFO_AND: return di(e->u.ops[0])?di(e->u.ops[1]):0;
			case PFO_OR: return di(e->u.ops[0])?1:di(e->u.ops[1]);
			case PFO_NOT: return di(e->u.ops[0])?0:1;
			case PFO_NEG: return -di(e->u.ops[0]);
		}
	}
	return 0;

}

static int compute_form_imode(struct dumbtts_conf *conf,int ival)
{
	struct dumbtts_formex *fe;
	for (fe=conf->formex;fe;fe=fe->next) {
		if (fe->type) continue;
		if (dumbtts_compute_iexpr(fe->expr,ival)) return fe->forma;
	}
	return conf->defforms[0];
}

int do_units(struct dumbtts_conf *conf,wchar_t wc,char **str,char *outbuf)
{
	char *c,*d,*eos;int ival,i,forma,typ;
	struct dumbtts_unit *u;
	wchar_t ws;
	if (wc>0x7f || !isdigit(wc) || !conf->units) return 0;
	ival=wc-'0';
	c=*str;
	for (i=0;i<6 && *c && isdigit(*c);i++) {
		ival=(10*ival)+(*c++)-'0';
	}
	typ=0;
	if (*c=='.') {
		c++;
		if (!isdigit(*c)) return 0;
		while(*c && isdigit(*c)) c++;
		typ=1;
	}
	if (*c && isdigit(*c))return 0;
	eos=c;
	while (*c && isspace(*c)) c++;
	for (u=conf->units;u;u=u->next) if (unit_compare(&c,u->unit)) break;
	if (!u) return 0;
	if (!typ) {
		forma=compute_form_imode(conf,ival);
	}
	else {
		forma=conf->defforms[1];
	}
	if (!forma) return 0;
	if (!typ) {
		sprintf(outbuf,"%d ",ival);
		outbuf+=strlen(outbuf);
	}
	else {
		*outbuf++=wc;
		while (*str<eos) *outbuf++=*(*str)++;
		*outbuf++=' ';
	}
	*str=c;
	c=u->result;
	while (forma>1) {
		forma--;
		d=strchr(c,'|');
		if (!d) break;
		c=d+1;
	}
	while (*c && *c!='|') *outbuf++=*c++;
	*outbuf++=0;
	return 1;

}
