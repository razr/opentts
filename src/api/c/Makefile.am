
inc_local = "-I$(top_srcdir)/src/common/"

include_HEADERS = libopentts.h

lib_LTLIBRARIES = libopentts.la
libopentts_la_SOURCES = libopentts.c
libopentts_la_CFLAGS = @ERROR_CFLAGS@
libopentts_la_CPPFLAGS = -D_GNU_SOURCE $(inc_local) $(GLIB_CFLAGS)
libopentts_la_LDFLAGS = -version-info @LIBOPENTTS_CURRENT@:@LIBOPENTTS_REVISION@:@LIBOPENTTS_AGE@
libopentts_la_LIBADD = $(top_srcdir)/src/common/libcommon.la -lpthread $(GLIB_LIBS)

if ENABLE_SESSION
libopentts_la_CPPFLAGS += -DENABLE_SESSION -DSPD_SPAWN_CMD=\"@prefix@/bin/speechd-server-spawn\"
endif
