
c_api = $(top_srcdir)/src/api/c

bin_PROGRAMS = spd-say
spd_say_SOURCES = say.c options.c options.h $(c_api)/libspeechd.h
spd_say_CPPFLAGS = -I$(c_api)
spd_say_LDADD = $(c_api)/libspeechd.la @EXTRA_SOCKET_LIBS@
