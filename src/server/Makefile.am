
bin_PROGRAMS = openttsd
openttsd_SOURCES = openttsd.c openttsd.h server.c server.h history.c history.h module.c module.h configuration.c configuration.h parse.c parse.h set.c set.h msg.h alloc.c alloc.h compare.c compare.h speaking.c speaking.h sighandler.c sighandler.h options.c options.h output.c output.h sem_functions.c sem_functions.h index_marking.c index_marking.h fdset.h

openttsd_LDADD = $(top_builddir)/src/libs/common/libcommon.la $(DOTCONF_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(EXTRA_SOCKET_LIBS)
openttsd_LDFLAGS = $(RDYNAMIC)

openttsd_CFLAGS = $(ERROR_CFLAGS)
openttsd_CPPFLAGS = "-I$(top_srcdir)/include" \
$(DOTCONF_CFLAGS) $(GLIB_CFLAGS) $(GMODULE_CFLAGS) \
$(GTHREAD_CFLAGS) -DOPENTTS_INTERNAL \
-DSYS_CONF=\"$(spdconfdir)\" -DSND_DATA=\"$(snddatadir)\" \
-DMODULEBINDIR=\"$(modulebindir)\" \
-DOPENTTS_USER=\"$(opentts_user)\" \
-DOPENTTS_GROUP=\"$(opentts_group)\" \
-DSYSTEM_LOG_PATH=\"$(system_log_path)\" \
-DSYSTEM_PID_PATH=\"$(system_pid_path)\" \
-DSYSTEM_SOCKET_PATH=\"$(system_socket_path)\"

