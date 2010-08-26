/*
 * mbrola.c -- simple interface for Mbrola binary.
 *
 * Copyright (C) 2010 Bohdan R. Rau <ethanak@polip.com>
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this package; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>

#include "opentts/opentts_types.h"
#include "module_utils.h"
#include "audio.h"
#include "mbrola.h"
#include "logging.h"

MOD_OPTION_1_STR(MbrolaBaseDir);
MOD_OPTION_1_STR(MbrolaBinary);
MOD_OPTION_4_HT(MbrolaVoice,code,sex,lang,freq);

void otts_mbrolaModLoad(void)
{
	MOD_OPTION_1_STR_REG(MbrolaBaseDir, "/usr/share/mbrola");
	MOD_OPTION_1_STR_REG(MbrolaBinary, "mbrola");
	MOD_OPTION_HT_REG(MbrolaVoice);
}

struct otts_mbrolaInfo *otts_mbrolaShowVoices(int *count)
{
	static int mbrola_param_got=0;
	static struct otts_mbrolaInfo *ombi=NULL;
	static int otts_count=0;
	char pbuf[256];

	if (!mbrola_param_got) {
		FILE *f;
		int msize=0;
		mbrola_param_got=1;
		sprintf(pbuf,"find '%s' -type f -and -regex '.*/[a-z][a-z][1-9][0-9]*$'",MbrolaBaseDir);
		f=popen(pbuf,"r");
		if (!f) return NULL;
		while (fgets(pbuf,256,f)) {
			char *code;int i;char lang[16];int female;int freq;
			TMbrolaVoice *params;
			pbuf[strlen(pbuf)-1]=0;
			code=strrchr(pbuf,'/')+1;
			for (i=0;i<otts_count;i++) if (!strcmp(code,ombi[i].code)) break;
			if (i<otts_count) continue;
			params=g_hash_table_lookup(MbrolaVoice,code);
			if (!params) continue;
			if (!msize) {
				msize=10;
				ombi=malloc(sizeof(*ombi) * msize);
			}
			else if (msize <= otts_count) {
				msize+=10;
				ombi=realloc(ombi,sizeof(*ombi) * msize);
			}
			ombi[otts_count].lang=strdup(params->lang);
			ombi[otts_count].path=strdup(pbuf);
			ombi[otts_count].code=strrchr(ombi[otts_count].path,'/')+1;
			ombi[otts_count].sex=(tolower(params->sex[0])=='m');
			ombi[otts_count++].freq=strtol(params->freq,NULL,10);
			log_msg(OTTS_LOG_DEBUG, "Mbrola: Found %s",code);
		}
		pclose(f);
	}
	if (count) *count=otts_count;
	return ombi;
}

struct otts_mbrolaInfo *otts_mbrolaVoiceInfo(char *code)
{
	int count,i;
	struct otts_mbrolaInfo * ombi=otts_mbrolaShowVoices(&count);
	if (!ombi) return NULL;
	for (i=0;i<count;i++) {
		if (!strcmp(code,ombi[i].code)) return &ombi[i];
	}
	return NULL;
}


struct otts_mbrolaHandler *otts_mbrolaInit(char *code)
{
	struct otts_mbrolaHandler *ost;
	struct otts_mbrolaInfo *mi;
	mi=otts_mbrolaVoiceInfo(code);
	if (!mi) return NULL;
	ost=malloc(sizeof(*ost));
	ost->voice=mi;
	return ost;
}


static void mbrola_fk(char *db,int *to_mbr,int *from_mbr)
{
	close(to_mbr[1]);
	close(from_mbr[0]);
	dup2(to_mbr[0],0);
	dup2(from_mbr[1],1);
	execlp(MbrolaBinary,"mbrola","-e",db,"-","-",NULL);
	exit(1);
}

static void apply_contrast(signed short *data,int len,double contrast_level)
{
        while (len-- > 0) {
                double d=(*data) * (M_PI_2/32768.0);
                *data++=sin(d + contrast_level * sin(d * 4)) * 32767;
        }
}

int otts_mbrolaSpeakD(struct otts_mbrolaHandler *oh,
		char *phonemes,
		double speed,
		double pitch,
		double volume,
		double contrast
		)
{
	char *waveform=NULL;
	int wave_size;
	char buffer[4096];
	int to_mbr[2],from_mbr[2];
	int pid;
	char buf[256];
	AudioTrack track;
	int contrast_do=0;
	if (contrast >= 0.3) {
		contrast=contrast/10.0;
		contrast_do=1;
	}
	log_msg(OTTS_LOG_DEBUG, "Mbrola: speakd started");
	pipe(to_mbr);
	pipe(from_mbr);
	pid=fork();
	if (pid<0) {
		close(to_mbr[0]);
		close(to_mbr[1]);
		close(from_mbr[0]);
		close(from_mbr[1]);
		return -1;
	}
	if (!pid) mbrola_fk(oh->voice->path,to_mbr,from_mbr);
	/*log_msg(OTTS_LOG_DEBUG, "Mbrola: Start speak:\n%s",phonemes);*/
	close(to_mbr[0]);
	close(from_mbr[1]);
	sprintf(buf,";;T=%.3f\n;;F=%.3f\n",speed,pitch);
	write(to_mbr[1],buf,strlen(buf));
	write(to_mbr[1],phonemes,strlen(phonemes));
	close(to_mbr[1]);
	wave_size=0;
	for (;;) {
		int n=read(from_mbr[0],buffer,4096);
		if (n<=0) break;
		if (!wave_size) waveform=malloc(n);
		else waveform=realloc(waveform,wave_size+n);
		memcpy(waveform+wave_size,buffer,n);
		wave_size+=n;
	}
	close(from_mbr[0]);
	if (!wave_size) return -2;
	if (contrast_do) {
		apply_contrast((signed short *)waveform,wave_size/2,contrast);
	}
	/* now we can play */
	opentts_audio_set_volume(module_audio_id, ensure((int)(200*volume-100),-100,100));
	track.num_samples = wave_size/2;
	track.num_channels = 1;
	track.sample_rate = oh->voice->freq;
	track.bits = 16;
	track.samples = (short *)waveform;
	if (opentts_audio_play(module_audio_id, track, SPD_AUDIO_LE)<0) {
		return -3;
	}
	return 0;
}


int otts_mbrolaSpeak(struct otts_mbrolaHandler *h,
		char *phonemes,
		int speed,
		int pitch,
		int volume,
		int contrast
		)
{
	double d_speed=(speed > 0)?(1.0 - speed/200.0):(1.0-speed/100.0);
	double d_pitch=(pitch < 0)?(1.0 + pitch/200.0):(1.0+pitch/100.0);
	double d_contrast=(contrast > 0)?(0.3+contrast/99.0):0.0;
	double d_volume=(volume+100.0)/200.0;


	return otts_mbrolaSpeakD(h,
		phonemes,
		d_speed,
		d_pitch,
		d_volume,
		d_contrast
		);
}

