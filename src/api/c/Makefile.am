
inc_local = "-I$(top_srcdir)/intl/"

include_HEADERS = libspeechd.h

lib_LTLIBRARIES = libspeechd.la
libspeechd_la_SOURCES = libspeechd.c
libspeechd_la_CFLAGS = @ERROR_CFLAGS@
libspeechd_la_CPPFLAGS = -D_GNU_SOURCE $(inc_local) @glib_include@
libspeechd_la_LDFLAGS = -version-info @LIB_SPD_CURRENT@:@LIB_SPD_REVISION@:@LIB_SPD_AGE@
libspeechd_la_LIBADD = -lpthread @glib_libs@

if ENABLE_SESSION
libspeechd_la_CPPFLAGS += -DENABLE_SESSION -DSPD_SPAWN_CMD=\"@prefix@/bin/speechd-server-spawn\"
endif
