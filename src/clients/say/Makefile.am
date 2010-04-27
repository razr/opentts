
c_api = $(top_srcdir)/src/api/c

bin_PROGRAMS = otts-say
otts_say_SOURCES = say.c options.c options.h
otts_say_CPPFLAGS = -I$(c_api)
otts_say_LDADD = $(c_api)/libopentts.la @EXTRA_SOCKET_LIBS@
