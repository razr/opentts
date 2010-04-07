
bin_PROGRAMS = speech-dispatcher
speech_dispatcher_SOURCES = speechd.c speechd.h server.c server.h history.c history.h module.c module.h configuration.c configuration.h parse.c parse.h set.c set.h msg.h alloc.c alloc.h compare.c compare.h speaking.c speaking.h options.c options.h output.c output.h sem_functions.c sem_functions.h index_marking.c index_marking.h

speech_dispatcher_LDADD = -ldotconf -lglib-2.0 -lgmodule-2.0 @glib_libs@ @gthread_libs@ -lpthread @EXTRA_SOCKET_LIBS@
speech_dispatcher_LDFLAGS = @RDYNAMIC@

inc_local = -I$(top_srcdir)/intl/

speech_dispatcher_CFLAGS = @ERROR_CFLAGS@
speech_dispatcher_CPPFLAGS = $(inc_local) @glib_include@ -DSYS_CONF=\"@spdconfdir@\" -DSND_DATA=\"@snddatadir@\" -DMODULEBINDIR=\"@modulebindir@\"  -DVERSION=\"@VERSION@\" -DPACKAGE=\"@PACKAGE@\" -DPIDPATH=\"@pidpath@\" 

if ENABLE_SESSION
speech_dispatcher_CPPFLAGS += -DENABLE_SESSION
endif
