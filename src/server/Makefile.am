
INCLUDES = -I$(top_srcdir)/src/common/ -I$(top_srcdir)/include/
bin_PROGRAMS = openttsd
openttsd_SOURCES = openttsd.c openttsd.h server.c server.h history.c history.h module.c module.h configuration.c configuration.h parse.c parse.h set.c set.h msg.h alloc.c alloc.h compare.c compare.h speaking.c speaking.h options.c options.h output.c output.h sem_functions.c sem_functions.h index_marking.c index_marking.h fdset.h

openttsd_LDADD = -L$(top_builddir)/src/common -lcommon $(DOTCONF_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(EXTRA_SOCKET_LIBS)
openttsd_LDFLAGS = $(RDYNAMIC)

openttsd_CFLAGS = $(ERROR_CFLAGS)
openttsd_CPPFLAGS = $(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GMODULE_CFLAGS) \
$(GTHREAD_CFLAGS) -DSYS_CONF=\"$(spdconfdir)\" -DSND_DATA=\"$(snddatadir)\" \
-DMODULEBINDIR=\"$(modulebindir)\" -DOPENTTS_INTERNAL

