## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libopentts.la
libopentts_la_SOURCES = libopentts.c
libopentts_la_CFLAGS = $(ERROR_CFLAGS)
libopentts_la_CPPFLAGS = "-I$(top_srcdir)/include" $(GLIB_CFLAGS) \
	-D_GNU_SOURCE -DSPD_SPAWN_CMD="\"$(bindir)/openttsd\""
libopentts_la_LDFLAGS = -version-info $(LIBOPENTTS_CURRENT):$(LIBOPENTTS_REVISION):$(LIBOPENTTS_AGE)
libopentts_la_LIBADD = -lpthread $(GLIB_LIBS)

