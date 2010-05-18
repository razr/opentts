
c_api = $(top_srcdir)/src/api/c
inc_local = "-I$(top_srcdir)/include"

bin_PROGRAMS = otts-say
otts_say_SOURCES = say.c options.c options.h
otts_say_CPPFLAGS = $(inc_local)
otts_say_LDADD = $(c_api)/libopentts.la @EXTRA_SOCKET_LIBS@
