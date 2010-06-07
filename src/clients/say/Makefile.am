c_api = $(top_builddir)/src/api/c

INCLUDES = -I$(top_srcdir)/include

bin_PROGRAMS = otts-say
otts_say_CPPFLAGS = $(GLIB_CFLAGS) -DLOCALEDIR=\"$(localedir)\"
otts_say_LDADD = $(c_api)/libopentts.la $(GLIB_LIBS) $(EXTRA_SOCKET_LIBS)
otts_say_SOURCES = say.c options.c options.h
