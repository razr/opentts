/*
 * module_main.c - One way of doing main() in output modules.
 * 
 * Copyright (C) 2001, 2002, 2003, 2006 Brailcom, o.p.s.
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
 * $Id: module_main.c,v 1.17 2008-10-15 17:05:37 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <glib.h>
#include <dotconf.h>
#include <ltdl.h>

#include "module_utils.h"

#ifdef __SUNPRO_C
/* Added by Willie Walker - getline is a gcc-ism */
ssize_t getline (char **lineptr, size_t *n, FILE *f);
#endif

int dispatch_cmd(char *cmd_line);

int 
main(int argc, char *argv[])
{
    char *cmd_buf;
    int ret;
    int ret_init;
    size_t n;
    char *configfilename;
    char *status_info;        

    g_thread_init(NULL);

    /* Initialize ltdl's list of preloaded audio backends. */
    LTDL_SET_PRELOADED_SYMBOLS();
    module_num_dc_options = 0;
    module_audio_id = 0;

    if (argc >= 2){
        configfilename = strdup(argv[1]);
    }else{
        configfilename = NULL;
    }

    ret = module_load();
    if (ret == -1) module_close(1);

    if (configfilename != NULL){
        /* Add the LAST option */
        module_dc_options = add_config_option(module_dc_options,
                                              &module_num_dc_options, "", 0, NULL, NULL, 0);

        configfile = dotconf_create(configfilename, module_dc_options, 0, CASE_INSENSITIVE);
        if (configfile){
            if (dotconf_command_loop(configfile) == 0){
                DBG("Error reading config file\n");
                module_close(1);
            }
	    dotconf_cleanup(configfile);
	    DBG("Configuration (pre) has been read from \"%s\"\n", configfilename);    
	    
	    xfree(configfilename);
        }else{
            DBG("Can't read specified config file!\n");        
        }
    }else{
        DBG("No config file specified, using defaults...\n");        
    }
    
    ret_init = module_init(&status_info);

    cmd_buf = NULL;  n=0;
    ret = getline(&cmd_buf, &n, stdin);
    if (ret == -1){
	DBG("Broken pipe when reading INIT, exiting... \n");
	module_close(2); 
    }

    if (!strcmp(cmd_buf, "INIT\n")){
	if (ret_init == 0){
	    printf("299-%s\n", status_info);
	    ret = printf("%s\n", "299 OK LOADED SUCCESSFULLY");
	}else{
	    printf("399-%s\n", status_info);
	    ret = printf("%s\n", "399 ERR CANT INIT MODULE");
	    return -1;
	}
      	xfree(status_info);

	if (ret < 0){ 
	    DBG("Broken pipe, exiting...\n");
            module_close(2); 
	}
	fflush(stdout);
    }else{
	DBG("ERROR: Wrong communication from module client: didn't call INIT\n");
	module_close(3);
    }
    xfree(cmd_buf);

    while(1){
        cmd_buf = NULL;  n=0;
        ret = getline(&cmd_buf, &n, stdin);
        if (ret == -1){
            DBG("Broken pipe, exiting... \n");
            module_close(2); 
        }

	DBG("CMD: <%s>", cmd_buf);

dispatch_cmd(cmd_buf);
	
	xfree(cmd_buf);
    } 
}

int dispatch_cmd(char *cmd_line){
char *cmd = NULL;
size_t cmd_len;
char *msg = NULL;

cmd_len = strcspn(cmd_line, " \t\n\r\f");
cmd = g_strndup(cmd_line, cmd_len);
pthread_mutex_lock(&module_stdout_mutex);

if( ! strcasecmp("audio", cmd)){
msg = do_audio();
} else if( ! strcasecmp("set", cmd)){
msg = do_set();
} else if( ! strcasecmp("speak", cmd)){
msg = do_speak();
} else if( ! strcasecmp("key", cmd)){
msg = do_key();
} else if(! strcasecmp("sound_icon", cmd)){
msg = do_sound_icon();
} else if(! strcasecmp("char", cmd)){
msg = do_char();
} else if(! strcasecmp("pause", cmd)){
do_pause();
} else if( ! strcasecmp("stop", cmd)){
do_stop();
} else if( ! strcasecmp("list_voices", cmd)){
msg = do_list_voices();
} else if( ! strcasecmp("loglevel", cmd)){
msg = do_loglevel();
} else if( ! strcasecmp("debug", cmd)){
msg = do_debug(cmd_line);
} else if( ! strcasecmp("quit", cmd)){
do_quit();
} else {
/*should we log?*/
          printf("300 ERR UNKNOWN COMMAND\n");
fflush(stdout);
}

if(msg != NULL ){
 if ( 0 > printf("%s\n", msg)){
     DBG("Broken pipe, exiting...\n");
     module_close(2);
 }
 fflush(stdout);
g_free(msg);
}

pthread_mutex_unlock(&module_stdout_mutex);
g_free(cmd);

return(0);
}

