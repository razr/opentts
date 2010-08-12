#include <stdio.h>
#include <stdlib.h>
#include <libdumbtts.h>

main(int argc,char *argv[])
{
	struct dumbtts_conf *conf;
	char *buf;
	int len;
	conf=dumbtts_TTSInit(argv[1]);
	len=dumbtts_GetString(conf,argv[2],NULL,0,0,"()[]",".,!?;:");
	if (len < 0) exit(1);
	printf("Len=%d\n",len);
	buf=malloc(len);
	dumbtts_GetString(conf,argv[2],buf,len,0,"()[]",".,!?;:");
	printf("%s\n",buf);



}
