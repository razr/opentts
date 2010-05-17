
inc_local = "-I$(top_srcdir)/include" "-I$(top_srcdir)/src/common/"

lib_LTLIBRARIES = libopentts.la
libopentts_la_SOURCES = libopentts.c
libopentts_la_CFLAGS = @ERROR_CFLAGS@
libopentts_la_CPPFLAGS = -D_GNU_SOURCE $(inc_local) $(GLIB_CFLAGS) \
-DSPD_SPAWN_CMD="\"$(bindir)/openttsd\""
libopentts_la_LDFLAGS = -version-info @LIBOPENTTS_CURRENT@:@LIBOPENTTS_REVISION@:@LIBOPENTTS_AGE@
libopentts_la_LIBADD = -lpthread $(GLIB_LIBS)

