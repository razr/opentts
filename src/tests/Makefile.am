## Process this file with automake to produce Makefile.in

c_api = $(top_builddir)/src/api/c
AM_CPPFLAGS = "-I$(top_srcdir)/include"

check_PROGRAMS = long_message clibrary clibrary2 run_test connection_recovery

long_message_SOURCES = long_message.c
long_message_LDADD = $(c_api)/libopentts.la $(EXTRA_SOCKET_LIBS)

clibrary_SOURCES = clibrary.c
clibrary_LDADD = $(c_api)/libopentts.la $(EXTRA_SOCKET_LIBS)

clibrary2_SOURCES = clibrary2.c
clibrary2_LDADD = $(c_api)/libopentts.la $(EXTRA_SOCKET_LIBS)

connection_recovery_SOURCES = connection-recovery.c
connection_recovery_LDADD = $(c_api)/libopentts.la $(EXTRA_SOCKET_LIBS)

run_test_SOURCES = run_test.c
run_test_LDADD = $(c_api)/libopentts.la $(EXTRA_SOCKET_LIBS)

if libdumbtts_support
check_PROGRAMS += demo

demo_SOURCES = demo.c
demo_LDADD = $(top_builddir)/src/libs/dumbtts/libdumbtts.la
endif

EXTRA_DIST= basic.test general.test keys.test priority_progress.test pronunciation.test punctuation.test sound_icons.test spelling.test ssml.test stop_and_pause.test voices.test yo.wav
